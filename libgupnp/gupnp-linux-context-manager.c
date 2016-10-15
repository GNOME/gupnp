/*
 * Copyright (C) 2011 Jens Georg
 *
 * Author: Jens Georg <mail@jensge.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

/**
 * SECTION:gupnp-linux-context-manager
 * @short_description: Linux-specific implementation of #GUPnPContextManager
 *
 * This is a Linux-specific context manager which uses <ulink
 * url="http://www.linuxfoundation.org/collaborate/workgroups/networking/netlink">Netlink</ulink>
 * to detect the changes in network interface configurations, such as
 * added or removed interfaces, network addresses, ...
 *
 * The context manager works in two phase.
 *
 * Phase one is the "bootstrapping" phase where we query all currently
 * configured interfaces and addresses.
 *
 * Phase two is the "listening" phase where we just listen to the netlink
 * messages that are happening and create or destroy #GUPnPContext<!-- -->s
 * accordingly.
 */

#define G_LOG_DOMAIN "GUPnP-ContextManager-Linux"

#include <config.h>

#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#ifdef HAVE_LINUX_WIRELESS_H
#include <linux/wireless.h>
#endif
#include <sys/ioctl.h>
#include <errno.h>
#include <string.h>

#include <glib.h>
#include <gio/gio.h>

#include "gupnp-linux-context-manager.h"
#include "gupnp-context.h"

G_DEFINE_TYPE (GUPnPLinuxContextManager,
               gupnp_linux_context_manager,
               GUPNP_TYPE_CONTEXT_MANAGER);

struct _GUPnPLinuxContextManagerPrivate {
        /* Socket used for IOCTL calls */
        int fd;

        /* Netlink sequence number; nl_seq > 1 means bootstrapping done */
        int nl_seq;

        /* Socket used to do netlink communication */
        GSocket *netlink_socket;

        /* Socket source used for normal netlink communication */
        GSource *netlink_socket_source;

        /* Idle source used during bootstrap */
        GSource *bootstrap_source;

        /* A hash table mapping system interface indices to a NetworkInterface
         * structure */
        GHashTable *interfaces;

        gboolean dump_netlink_packets;
};

typedef enum {
        /* Interface is up */
        NETWORK_INTERFACE_UP = 1 << 0,

        /* Interface doesn't support multicast or is P-t-P */
        NETWORK_INTERFACE_IGNORE = 1 << 1,

        /* Interface is down but has an address set */
        NETWORK_INTERFACE_PRECONFIGURED = 1 << 2
} NetworkInterfaceFlags;

static void
dump_rta_attr (struct rtattr *rt_attr)
{
        const char *data = NULL;
        const char *label = NULL;
        char buf[INET6_ADDRSTRLEN];

        if (rt_attr->rta_type == IFA_ADDRESS ||
                        rt_attr->rta_type == IFA_LOCAL ||
                        rt_attr->rta_type == IFA_BROADCAST ||
                        rt_attr->rta_type == IFA_ANYCAST) {
                data = inet_ntop (AF_INET,
                                RTA_DATA (rt_attr),
                                buf,
                                sizeof (buf));
        } else if (rt_attr->rta_type == IFA_LABEL) {
                data = (const char *) RTA_DATA (rt_attr);
        } else {
                data = "Unknown";
        }

        switch (rt_attr->rta_type) {
                case IFA_UNSPEC: label = "IFA_UNSPEC"; break;
                case IFA_ADDRESS: label = "IFA_ADDRESS"; break;
                case IFA_LOCAL: label = "IFA_LOCAL"; break;
                case IFA_LABEL: label = "IFA_LABEL"; break;
                case IFA_BROADCAST: label = "IFA_BROADCAST"; break;
                case IFA_ANYCAST: label = "IFA_ANYCAST"; break;
                case IFA_CACHEINFO: label = "IFA_CACHEINFO"; break;
                case IFA_MULTICAST: label = "IFA_MULTICAST"; break;
#if defined(IFA_FLAGS)
                case IFA_FLAGS: label = "IFA_FLAGS"; break;
#endif
                default: label = "Unknown"; break;
        }

        g_debug ("  %s: %s", label, data);
}

static void
gupnp_linux_context_manager_hexdump (const guint8 * const buffer,
                                     size_t               length,
                                     GString             *hexdump)
{
        size_t offset = 0;
        char ascii[17] = { 0 };
        char padding[49] = { 0 };
        uint8_t modulus = 0;

        for (offset = 0; offset < length; offset++) {
                modulus = (offset % 16);
                if (modulus == 0) {
                        g_string_append_printf (hexdump,
                                                "%04zx: ", offset);
                }

                g_string_append_printf (hexdump, "%02x ", buffer[offset]);

                if (g_ascii_isprint (buffer[offset])) {
                        ascii[modulus] = buffer[offset];
                } else {
                        ascii[modulus] = '.';
                }

                /* end of line, dump ASCII */
                if (modulus == 15) {
                        g_string_append_printf (hexdump, "  %s\n", ascii);
                        memset (ascii, 0, sizeof (ascii));
                }
        }

        if (modulus != 15) {
                memset (padding, ' ', 3 * (15 - modulus));
                g_string_append_printf (hexdump, "%s  %s\n", padding, ascii);
        }
}

/* struct representing a network interface */
struct _NetworkInterface {
        /* Weak pointer to context manager associated with this interface */
        GUPnPLinuxContextManager *manager;

        /* Name of the interface (eth0 etc.) */
        char *name;

        /* ESSID for wireless interfaces */
        char *essid;

        /* interface id of the device */
        int index;

        /* States of the interface */
        NetworkInterfaceFlags flags;

        /* UPnP contexts associated with this interface. Can be more than one
         * with alias addresses like eth0:1 etc. */
        GHashTable *contexts;
};

typedef struct _NetworkInterface NetworkInterface;

/* Create a new network interface struct and query the device name */
static NetworkInterface *
network_device_new (GUPnPLinuxContextManager *manager,
                    char                     *name,
                    int                       index)
{
        NetworkInterface *device;

        device = g_slice_new0 (NetworkInterface);
        device->manager = manager;
        device->name = name;
        device->index = index;

        device->contexts = g_hash_table_new_full (g_str_hash,
                                                  g_str_equal,
                                                  g_free,
                                                  g_object_unref);

        return device;
}

/* Try to update the ESSID of a network interface. */
static void
network_device_update_essid (NetworkInterface *device)
{
        char *old_essid = device->essid;
#ifdef HAVE_LINUX_WIRELESS_H
        char essid[IW_ESSID_MAX_SIZE + 1];
        struct iwreq iwr;
        int ret;

        /* Query essid */
        memset (&iwr, 0, sizeof (struct iwreq));
        memset (essid, 0, IW_ESSID_MAX_SIZE + 1);
        strncpy (iwr.ifr_name, device->name, IFNAMSIZ);
        iwr.u.essid.pointer = (caddr_t) essid;
        iwr.u.essid.length = IW_ESSID_MAX_SIZE;
        ret = ioctl (device->manager->priv->fd, SIOCGIWESSID, &iwr);

        if ((ret == 0 && essid[0] != '\0') &&
            (!device->essid || strcmp (device->essid, essid)))
                device->essid = g_strdup (essid);
        else
                old_essid = NULL;
#endif
        g_free (old_essid);
}

static void
network_device_create_context (NetworkInterface *device,
                               const char       *address,
                               const char       *label,
                               const char       *mask)
{
        guint port;
        GError *error = NULL;
        GUPnPContext *context;

        if (g_hash_table_contains (device->contexts, address)) {
                g_debug ("Context for address %s on %s already exists",
                         address,
                         label);

                return;
        }

        g_object_get (device->manager, "port", &port, NULL);

        network_device_update_essid (device);

        context = g_initable_new (GUPNP_TYPE_CONTEXT,
                                  NULL,
                                  &error,
                                  "host-ip", address,
                                  "interface", label,
                                  "network", device->essid ? device->essid
                                                           : mask,
                                  "port", port,
                                  NULL);

        if (error) {
                g_warning ("Error creating GUPnP context: %s",
                           error->message);
                g_error_free (error);

                return;
        }
        g_hash_table_insert (device->contexts, g_strdup (address), context);

        if (device->flags & NETWORK_INTERFACE_UP) {
                g_signal_emit_by_name (device->manager,
                                       "context-available",
                                       context);
        }
}

static void
context_signal_up (G_GNUC_UNUSED gpointer key,
                   gpointer               value,
                   gpointer               user_data)
{
        g_signal_emit_by_name (user_data, "context-available", value);
}

static void
context_signal_down (G_GNUC_UNUSED gpointer key,
                     gpointer               value,
                     gpointer               user_data)
{
        g_signal_emit_by_name (user_data, "context-unavailable", value);
}

static void
network_device_up (NetworkInterface *device)
{
        if (device->flags & NETWORK_INTERFACE_UP)
                return;

        device->flags |= NETWORK_INTERFACE_UP;

        if (g_hash_table_size (device->contexts) > 0)
                g_hash_table_foreach (device->contexts,
                                      context_signal_up,
                                      device->manager);
}

static void
network_device_down (NetworkInterface *device)
{
        if (!device->flags & NETWORK_INTERFACE_UP)
                return;

        device->flags &= ~NETWORK_INTERFACE_UP;

        if (device->contexts)
                g_hash_table_foreach (device->contexts,
                                      context_signal_down,
                                      device->manager);
}

static void
network_device_free (NetworkInterface *device)
{
        g_free (device->name);
        g_free (device->essid);

        if (device->contexts != NULL) {
                GHashTableIter iter;
                char *key;
                GUPnPContext *value;

                g_hash_table_iter_init (&iter, device->contexts);
                while (g_hash_table_iter_next (&iter,
                                               (gpointer *) &key,
                                               (gpointer *) &value)) {
                        g_signal_emit_by_name (device->manager,
                                               "context-unavailable",
                                               value);
                        g_hash_table_iter_remove (&iter);
                }
        }

        g_hash_table_unref (device->contexts);
        device->contexts = NULL;

        g_slice_free (NetworkInterface, device);
}


static void query_all_network_interfaces (GUPnPLinuxContextManager *self);
static void query_all_addresses (GUPnPLinuxContextManager *self);
static void receive_netlink_message (GUPnPLinuxContextManager  *self,
                                     GError                   **error);
static void create_context (GUPnPLinuxContextManager *self,
                            const char               *label,
                            const char               *address,
                            const char               *mask,
                            struct ifaddrmsg         *ifa);
static void remove_context (GUPnPLinuxContextManager *self,
                            const char               *address,
                            const char               *label,
                            struct ifaddrmsg         *ifa);

static gboolean
on_netlink_message_available (G_GNUC_UNUSED GSocket     *socket,
                              G_GNUC_UNUSED GIOCondition condition,
                              gpointer                   user_data)
{
        GUPnPLinuxContextManager *self;

        self = GUPNP_LINUX_CONTEXT_MANAGER (user_data);

        receive_netlink_message (self, NULL);

        return TRUE;
}

#define RT_ATTR_OK(a,l) \
        ((l > 0) && RTA_OK (a, l))

static void
extract_info (struct nlmsghdr *header,
              gboolean         dump,
              guint8           prefixlen,
              char           **address,
              char           **label,
              char           **mask)
{
        int rt_attr_len;
        struct rtattr *rt_attr;
        char buf[INET6_ADDRSTRLEN];

        rt_attr = IFA_RTA (NLMSG_DATA (header));
        rt_attr_len = IFA_PAYLOAD (header);
        while (RT_ATTR_OK (rt_attr, rt_attr_len)) {
                if (dump) {
                        dump_rta_attr (rt_attr);
                }

                if (rt_attr->rta_type == IFA_LABEL) {
                        *label = g_strdup ((char *) RTA_DATA (rt_attr));
                } else if (rt_attr->rta_type == IFA_LOCAL) {
                        if (address != NULL) {
                                inet_ntop (AF_INET,
                                           RTA_DATA (rt_attr),
                                           buf,
                                           sizeof (buf));
                                *address = g_strdup (buf);
                        }

                        if (mask != NULL) {
                                struct in_addr addr, *data;
                                guint32 bitmask;

                                bitmask = htonl(G_MAXUINT32 << (32 - prefixlen));
                                data = RTA_DATA (rt_attr);

                                addr.s_addr = data->s_addr & bitmask;

                                inet_ntop (AF_INET,
                                           &addr,
                                           buf,
                                           sizeof (buf));
                                *mask = g_strdup (buf);

                        }
                }
                rt_attr = RTA_NEXT (rt_attr, rt_attr_len);
        }
}

static void
extract_link_message_info (struct nlmsghdr *header,
                           char           **ifname,
                           gboolean        *is_wifi)
{
        int rt_attr_len;
        struct rtattr *rt_attr;
        *ifname = NULL;
        *is_wifi = FALSE;

        rt_attr = IFLA_RTA (NLMSG_DATA (header));
        rt_attr_len = IFLA_PAYLOAD (header);
        while (RT_ATTR_OK (rt_attr, rt_attr_len)) {
                switch (rt_attr->rta_type)
                {
                case IFLA_WIRELESS:
                        *is_wifi = TRUE;
                        break;
                case IFLA_IFNAME:
                        *ifname = g_strdup ((const char *) RTA_DATA(rt_attr));
                        break;
                default:
                        break;
                }

                rt_attr = RTA_NEXT (rt_attr, rt_attr_len);
        }
}

static void
create_context (GUPnPLinuxContextManager *self,
                const char               *address,
                const char               *label,
                const char               *mask,
                struct ifaddrmsg         *ifa)
{
        NetworkInterface *device;

        device = g_hash_table_lookup (self->priv->interfaces,
                                      GINT_TO_POINTER (ifa->ifa_index));

        if (!device) {
                g_warning ("Got new address for device %d but device is"
                           " not active",
                           ifa->ifa_index);

                return;
        }

        /* If device isn't one we consider, silently skip address */
        if (device->flags & NETWORK_INTERFACE_IGNORE)
                return;

        network_device_create_context (device,
                                       address,
                                       label,
                                       mask);
}

static void
remove_context (GUPnPLinuxContextManager *self,
                const char               *address,
                const char               *label,
                struct ifaddrmsg         *ifa)
{
        NetworkInterface *device;
        GUPnPContext *context;

        device = g_hash_table_lookup (self->priv->interfaces,
                                      GINT_TO_POINTER (ifa->ifa_index));

        if (!device) {
                g_debug ("Device with index %d not found, ignoring",
                         ifa->ifa_index);

                return;
        }

        context = g_hash_table_lookup (device->contexts, address);
        if (context) {
                if (device->flags & NETWORK_INTERFACE_UP) {
                        g_signal_emit_by_name (self,
                                               "context-unavailable",
                                               context);
                }
                g_hash_table_remove (device->contexts, address);
        } else {
                g_debug ("Failed to find context with address %s",
                         address);
        }

        if (g_hash_table_size (device->contexts) == 0)
                device->flags &= ~NETWORK_INTERFACE_PRECONFIGURED;
}

/* Idle-handler for initial interface and address bootstrapping.
 *
 * We cannot send the RTM_GETADDR message until we processed all packets of
 * the RTM_GETLINK message. So on the first call this idle handler processes
 * all answers of RTM_GETLINK on the second call all answers of RTM_GETADDR
 * and on the third call it creates the regular socket source for listening on
 * the netlink socket, detaching itself from the idle source afterwards.
 */
static gboolean
on_bootstrap (GUPnPLinuxContextManager *self)
{
        if (self->priv->nl_seq == 0) {
                query_all_network_interfaces (self);

                return TRUE;
        } else if (self->priv->nl_seq == 1) {
                query_all_addresses (self);

                return TRUE;
        } else {
                self->priv->netlink_socket_source = g_socket_create_source
                                                (self->priv->netlink_socket,
                                                 G_IO_IN | G_IO_PRI,
                                                 NULL);

                g_source_attach (self->priv->netlink_socket_source,
                                 g_main_context_get_thread_default ());

                g_source_set_callback (self->priv->netlink_socket_source,
                                       (GSourceFunc)
                                       on_netlink_message_available,
                                       self,
                                       NULL);
        }

        return FALSE;
}

struct nl_req_s {
        struct nlmsghdr hdr;
        struct rtgenmsg gen;
};

static void
send_netlink_request (GUPnPLinuxContextManager *self,
                      guint netlink_message,
                      guint flags)
{
        struct nl_req_s req;
        struct sockaddr_nl dest;
        struct msghdr msg;
        struct iovec io;
        int fd;

        memset (&req, 0, sizeof (req));
        memset (&dest, 0, sizeof (dest));
        memset (&msg, 0, sizeof (msg));

        dest.nl_family = AF_NETLINK;
        req.hdr.nlmsg_len = NLMSG_LENGTH (sizeof (struct rtgenmsg));
        req.hdr.nlmsg_seq = self->priv->nl_seq++;
        req.hdr.nlmsg_type = netlink_message;
        req.hdr.nlmsg_flags = NLM_F_REQUEST | flags;
        req.gen.rtgen_family = AF_INET;

        io.iov_base = &req;
        io.iov_len = req.hdr.nlmsg_len;

        msg.msg_iov = &io;
        msg.msg_iovlen = 1;
        msg.msg_name = &dest;
        msg.msg_namelen = sizeof (dest);

        fd = g_socket_get_fd (self->priv->netlink_socket);
        if (sendmsg (fd, (struct msghdr *) &msg, 0) < 0)
                g_warning ("Could not send netlink message: %s",
                           strerror (errno));
}

/* Query all available interfaces and immediately process all answers. We need
 * to do this to be able to send RTM_GETADDR in the next step */
static void
query_all_network_interfaces (GUPnPLinuxContextManager *self)
{
        GError *error = NULL;

        g_debug ("Bootstrap: Querying all interfaces");

        send_netlink_request (self, RTM_GETLINK, NLM_F_DUMP);
        do {
                receive_netlink_message (self, &error);
        } while (error == NULL);

        g_error_free (error);
}

/* Start query of all currenly available network addresses. The answer will be
 * processed by the normal netlink socket source call-back. */
static void
query_all_addresses (GUPnPLinuxContextManager *self)
{
        g_debug ("Bootstrap: Querying all addresses");
        send_netlink_request (self,
                              RTM_GETADDR,
                              NLM_F_ROOT | NLM_F_MATCH | NLM_F_ACK);
}

/* Ignore non-multicast device, except loop-back and P-t-P devices */
#define INTERFACE_IS_VALID(ifi) \
        (((ifi)->ifi_flags & (IFF_MULTICAST | IFF_LOOPBACK)) && \
         !((ifi)->ifi_flags & IFF_POINTOPOINT))

/* Handle status changes (up, down, new address, ...) on network interfaces */
static void
handle_device_status_change (GUPnPLinuxContextManager *self,
                             char                     *name,
                             struct ifinfomsg         *ifi)
{
        gpointer key;
        NetworkInterface *device;

        key = GINT_TO_POINTER (ifi->ifi_index);
        device = g_hash_table_lookup (self->priv->interfaces,
                                      key);

        if (device != NULL) {
                if (ifi->ifi_flags & IFF_UP)
                        network_device_up (device);
                else
                        network_device_down (device);

                return;
        }

        device = network_device_new (self, name, ifi->ifi_index);
        if (device) {
                if (!INTERFACE_IS_VALID (ifi))
                        device->flags |= NETWORK_INTERFACE_IGNORE;
                if (ifi->ifi_flags & IFF_UP)
                        device->flags |= NETWORK_INTERFACE_UP;

                g_hash_table_insert (self->priv->interfaces,
                                     key,
                                     device);
        }
}

static void
remove_device (GUPnPLinuxContextManager *self,
               struct ifinfomsg         *ifi)
{
        g_hash_table_remove (self->priv->interfaces,
                             GINT_TO_POINTER (ifi->ifi_index));
}

#define NLMSG_IS_VALID(msg,len) \
        (NLMSG_OK(msg,len) && (msg->nlmsg_type != NLMSG_DONE))

/* Process the raw netlink message and dispatch to helper functions
 * accordingly */
static void
receive_netlink_message (GUPnPLinuxContextManager *self, GError **error)
{
        static char buf[8196];

        gssize len;
        GError *inner_error = NULL;
        struct nlmsghdr *header = (struct nlmsghdr *) buf;
        struct ifinfomsg *ifi;
        struct ifaddrmsg *ifa;

        len = g_socket_receive (self->priv->netlink_socket,
                                buf,
                                sizeof (buf),
                                NULL,
                                &inner_error);
        if (len == -1) {
                if (inner_error->code != G_IO_ERROR_WOULD_BLOCK)
                        g_warning ("Error receiving netlink message: %s",
                                   inner_error->message);
                g_propagate_error (error, inner_error);

                return;
        }

        if (self->priv->dump_netlink_packets) {
                GString *hexdump = NULL;

                /* We should have at most len / 16 + 1 lines with 74 characters each */
                hexdump = g_string_new_len (NULL, ((len / 16) + 1) * 73);
                gupnp_linux_context_manager_hexdump ((guint8 *) buf, len, hexdump);

                g_debug ("Netlink packet dump:\n%s", hexdump->str);
                g_string_free (hexdump, TRUE);
        }

        for (;NLMSG_IS_VALID (header, len); header = NLMSG_NEXT (header,len)) {
                switch (header->nlmsg_type) {
                        /* RTM_NEWADDR and RTM_DELADDR are sent on real address
                         * changes.
                         * RTM_NEWLINK is sent on varous occations:
                         *  - Creation of a new device
                         *  - Device goes up/down
                         *  - Wireless status changes
                         * RTM_DELLINK is sent only if device is removed, like
                         * openvpn --rmtun /dev/tun0, NOT on ifconfig down. */
                        case RTM_NEWADDR:
                            {
                                char *label = NULL;
                                char *address = NULL;
                                char *mask = NULL;

                                g_debug ("Received RTM_NEWADDR");
                                ifa = NLMSG_DATA (header);
                                extract_info (header,
                                              self->priv->dump_netlink_packets,
                                              ifa->ifa_prefixlen,
                                              &address,
                                              &label,
                                              &mask);
                                create_context (self, address, label, mask, ifa);
                                g_free (label);
                                g_free (address);
                                g_free (mask);
                            }
                            break;
                        case RTM_DELADDR:
                            {
                                char *label = NULL;
                                char *address = NULL;

                                g_debug ("Received RTM_DELADDR");
                                ifa = NLMSG_DATA (header);
                                extract_info (header,
                                              self->priv->dump_netlink_packets,
                                              ifa->ifa_prefixlen,
                                              &address,
                                              &label,
                                              NULL);
                                remove_context (self, address, label, ifa);
                                g_free (label);
                                g_free (address);
                            }
                            break;
                        case RTM_NEWLINK: {
                                char *name = NULL;
                                gboolean is_wireless_status_message = FALSE;

                                g_debug ("Received RTM_NEWLINK");
                                ifi = NLMSG_DATA (header);
                                extract_link_message_info (header,
                                                           &name,
                                                           &is_wireless_status_message);

                                /* Check if wireless is up for chit-chat */
                                if (is_wireless_status_message) {
                                        g_free (name);

                                        continue;
                                }

                                handle_device_status_change (self, name, ifi);
                                break;
                        }
                        case RTM_DELLINK:
                                g_debug ("Received RTM_DELLINK");
                                ifi = NLMSG_DATA (header);
                                remove_device (self, ifi);
                                break;
                        case NLMSG_ERROR:
                                break;
                        default:
                                break;
                }
        }
}

/* Create INET socket used for SIOCGIFNAME and SIOCGIWESSID ioctl
 * calls */
static gboolean
create_ioctl_socket (GUPnPLinuxContextManager *self, GError **error)
{
        self->priv->fd = socket (AF_INET, SOCK_DGRAM, 0);

        if (self->priv->fd < 0) {
                self->priv->fd = 0;

                g_set_error_literal (error,
                                     G_IO_ERROR,
                                     g_io_error_from_errno (errno),
                                     "Failed to setup socket for ioctl");

                return FALSE;
        }

        return TRUE;
}

/* Create a netlink socket, bind to it and wrap it in a GSocket */
static gboolean
create_netlink_socket (GUPnPLinuxContextManager *self, GError **error)
{
        struct sockaddr_nl sa;
        int fd, status;
        GSocket *sock;
        GError *inner_error;

        inner_error = NULL;

        fd = socket (PF_NETLINK, SOCK_DGRAM, NETLINK_ROUTE);
        if (fd == -1) {
                g_set_error_literal (error,
                                     G_IO_ERROR,
                                     g_io_error_from_errno (errno),
                                     "Failed to bind to netlink socket");
                return FALSE;
        }

        memset (&sa, 0, sizeof (sa));
        sa.nl_family = AF_NETLINK;
        /* Listen for interface changes and IP address changes */
        sa.nl_groups = RTMGRP_LINK | RTMGRP_IPV4_IFADDR;

        status = bind (fd, (struct sockaddr *) &sa, sizeof (sa));
        if (status == -1) {
                g_set_error_literal (error,
                                     G_IO_ERROR,
                                     g_io_error_from_errno (errno),
                                     "Failed to bind to netlink socket");
                close (fd);

                return FALSE;
        }

        sock = g_socket_new_from_fd (fd, &inner_error);
        if (sock == NULL) {
                close (fd);
                g_propagate_prefixed_error (error,
                                            inner_error,
                                            "Failed to create GSocket from "
                                            "netlink socket");

                return FALSE;
        }

        g_socket_set_blocking (sock, FALSE);

        self->priv->netlink_socket = sock;

        return TRUE;
}

/* public helper function to determine runtime-fallback depending on netlink
 * availability. */
gboolean
gupnp_linux_context_manager_is_available (void)
{
        int fd = -1;

        fd = socket (PF_NETLINK, SOCK_DGRAM, NETLINK_ROUTE);

        if (fd == -1)
                return FALSE;

        close (fd);

        return TRUE;
}

/* GObject virtual functions */

static void
gupnp_linux_context_manager_init (GUPnPLinuxContextManager *self)
{
        self->priv =
                G_TYPE_INSTANCE_GET_PRIVATE (self,
                                             GUPNP_TYPE_LINUX_CONTEXT_MANAGER,
                                             GUPnPLinuxContextManagerPrivate);

        self->priv->nl_seq = 0;

        self->priv->interfaces =
                g_hash_table_new_full (g_direct_hash,
                                       g_direct_equal,
                                       NULL,
                                       (GDestroyNotify) network_device_free);
}

/* Constructor, kicks off bootstrapping */
static void
gupnp_linux_context_manager_constructed (GObject *object)
{
        GObjectClass *parent_class;
        GUPnPLinuxContextManager *self;
        GError *error = NULL;
        const char *env_var = NULL;

        self = GUPNP_LINUX_CONTEXT_MANAGER (object);

        env_var = g_getenv ("GUPNP_DEBUG_NETLINK");
        self->priv->dump_netlink_packets = (env_var != NULL) &&
                strstr (env_var, "1") != NULL;

        if (!create_ioctl_socket (self, &error))
                goto cleanup;

        if (!create_netlink_socket (self, &error))
                goto cleanup;

        self->priv->bootstrap_source = g_idle_source_new ();
        g_source_attach (self->priv->bootstrap_source,
                         g_main_context_get_thread_default ());
        g_source_set_callback (self->priv->bootstrap_source,
                               (GSourceFunc) on_bootstrap,
                               self,
                               NULL);
cleanup:
        if (error) {
                if (self->priv->fd > 0)
                        close (self->priv->fd);

                g_warning ("Failed to setup Linux context manager: %s",
                           error->message);

                g_error_free (error);
        }

        /* Chain-up */
        parent_class = G_OBJECT_CLASS (gupnp_linux_context_manager_parent_class);
        if (parent_class->constructed)
                parent_class->constructed (object);
}

static void
gupnp_linux_context_manager_dispose (GObject *object)
{
        GUPnPLinuxContextManager *self;
        GObjectClass *parent_class;

        self = GUPNP_LINUX_CONTEXT_MANAGER (object);

        if (self->priv->bootstrap_source != NULL) {
                g_source_destroy (self->priv->bootstrap_source);
                g_source_unref (self->priv->bootstrap_source);
                self->priv->bootstrap_source = NULL;
        }

        if (self->priv->netlink_socket_source != NULL) {
               g_source_destroy (self->priv->netlink_socket_source);
               g_source_unref (self->priv->netlink_socket_source);
               self->priv->netlink_socket_source = NULL;
        }

        if (self->priv->netlink_socket != NULL) {
                g_object_unref (self->priv->netlink_socket);
                self->priv->netlink_socket = NULL;
        }

        if (self->priv->fd != 0) {
                close (self->priv->fd);
                self->priv->fd = 0;
        }

        if (self->priv->interfaces) {
                g_hash_table_destroy (self->priv->interfaces);
                self->priv->interfaces = NULL;
        }

        /* Chain-up */
        parent_class = G_OBJECT_CLASS (gupnp_linux_context_manager_parent_class);
        parent_class->dispose (object);
}

static void
gupnp_linux_context_manager_class_init (GUPnPLinuxContextManagerClass *klass)
{
        GObjectClass *object_class;

        object_class = G_OBJECT_CLASS (klass);

        object_class->constructed = gupnp_linux_context_manager_constructed;
        object_class->dispose     = gupnp_linux_context_manager_dispose;

        g_type_class_add_private (klass,
                                  sizeof (GUPnPLinuxContextManagerPrivate));
}
