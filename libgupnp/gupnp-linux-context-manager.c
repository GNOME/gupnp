/*
 * Copyright (C) 2011 Jens Georg
 *
 * Author: Jens Georg <mail@jensge.org>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
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

#define G_LOG_DOMAIN "gupnp-context-manager"

#include <config.h>

#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#ifdef HAVE_LINUX_WIRELESS_H
#include <linux/wireless.h>
#else
#include <net/if.h>
#endif
#include <sys/ioctl.h>
#include <errno.h>
#include <string.h>

#include <glib.h>
#include <gio/gio.h>

#include "gupnp-linux-context-manager.h"
#include "gupnp-context.h"

struct _RtmAddrInfo {
        uint32_t flags;
        char *label;
        char *ip_string;
        GInetAddress *address;
        GInetAddressMask *mask;
        uint32_t preferred;
        uint32_t valid;
        struct ifaddrmsg *ifa;
};
typedef struct _RtmAddrInfo RtmAddrInfo;

static void
rtm_addr_info_free (RtmAddrInfo *info)
{
        g_clear_pointer (&info->ip_string, g_free);
        g_clear_pointer (&info->label, g_free);
        g_clear_object (&info->address);
        g_clear_object (&info->mask);

        g_free (info);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC (RtmAddrInfo, rtm_addr_info_free)

static GInetAddressMask *
generate_mask (struct ifaddrmsg *ifa, struct rtattr *rt_attr)
{
        struct in6_addr addr = { 0 };
        int i = 0;
        int bits = ifa->ifa_prefixlen;
        guint8 *outbuf = (guint8 *) &addr.s6_addr;

        struct in6_addr *data = RTA_DATA (rt_attr);
        guint8 *inbuf = (guint8 *) &data->s6_addr;

        for (i = 0; i < (ifa->ifa_family == AF_INET ? 4 : 16); i++) {
                if (bits > 8) {
                        bits -= 8;
                        outbuf[i] = inbuf[i] & 0xff;
                } else {
                        static const guint8 bits_a[] = { 0x00,
                                                         0x08,
                                                         0x0C,
                                                         0x0E,
                                                         0x0F };

                        if (bits >= 4) {
                                outbuf[i] = inbuf[i] & 0xf0;
                                bits -= 4;
                        }
                        outbuf[i] = outbuf[i] | (inbuf[i] & bits_a[bits]);
                        break;
                }
        }
        g_autoptr (GInetAddress) mask_address =
                g_inet_address_new_from_bytes (outbuf, ifa->ifa_family);

        g_autoptr (GError) error = NULL;
        GInetAddressMask *mask = g_inet_address_mask_new (mask_address,
                                                          ifa->ifa_prefixlen,
                                                          &error);

        if (error != NULL) {
                g_debug ("Could not create address "
                         "mask: %s",
                         error->message);
        }

        return mask;
}

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

        /* Receive buffer for netlink messages. */
        char recvbuf[8196];

        gboolean dump_netlink_packets;
};
typedef struct _GUPnPLinuxContextManagerPrivate GUPnPLinuxContextManagerPrivate;


struct _GUPnPLinuxContextManager {
        GUPnPContextManager parent;
};

G_DEFINE_TYPE_WITH_PRIVATE (GUPnPLinuxContextManager,
                            gupnp_linux_context_manager,
                            GUPNP_TYPE_CONTEXT_MANAGER)

typedef enum {
        /* Interface is up */
        NETWORK_INTERFACE_UP = 1 << 0,

        /* Interface doesn't support multicast or is P-t-P */
        NETWORK_INTERFACE_IGNORE = 1 << 1,

        /* Interface is down but has an address set */
        NETWORK_INTERFACE_PRECONFIGURED = 1 << 2
} NetworkInterfaceFlags;

static const char *IFA_FLAGS_DECODE[] = {
        [IFA_F_SECONDARY] = "IFA_F_SECONDARY",
        [IFA_F_NODAD] = "IFA_F_NODAD",
        [IFA_F_OPTIMISTIC] = "IFA_F_OPTIMISTIC",
        [IFA_F_DADFAILED] = "IFA_F_DADFAILED",
        [IFA_F_HOMEADDRESS] = "IFA_F_HOMEADDRESS",
        [IFA_F_DEPRECATED] = "IFA_F_DEPRECATED",
        [IFA_F_TENTATIVE] = "IFA_F_TENTATIVE",
        [IFA_F_PERMANENT] = "IFA_F_PERMANENT",
        [IFA_F_MANAGETEMPADDR] = "IFA_F_MANAGETEMPADDR",
        [IFA_F_NOPREFIXROUTE] = "IFA_F_NOPREFIXROUTE",
        [IFA_F_MCAUTOJOIN] = "IFA_F_MCAUTOJOIN",
        [IFA_F_STABLE_PRIVACY] = "IFA_F_STABLE_PRIVACY",
};

static void
dump_rta_attr (sa_family_t family, struct rtattr *rt_attr)
{
        const char *data = NULL;
        const char *label = NULL;
        g_autoptr (GString) builder = NULL;
        char buf[INET6_ADDRSTRLEN];

        if (rt_attr->rta_type == IFA_ADDRESS ||
                        rt_attr->rta_type == IFA_LOCAL ||
                        rt_attr->rta_type == IFA_BROADCAST ||
                        rt_attr->rta_type == IFA_ANYCAST) {
                data = inet_ntop (family,
                                  RTA_DATA (rt_attr),
                                  buf,
                                  sizeof (buf));
        } else if (rt_attr->rta_type == IFA_LABEL) {
                data = (const char *) RTA_DATA (rt_attr);
        } else if (rt_attr->rta_type == IFA_CACHEINFO) {
                struct ifa_cacheinfo *ci =
                        (struct ifa_cacheinfo *) RTA_DATA (rt_attr);
                builder = g_string_new (NULL);
                g_string_append_printf (builder,
                                        "Cache Info: c: %u p: %u v: %u t: %u",
                                        ci->cstamp,
                                        ci->ifa_prefered,
                                        ci->ifa_valid,
                                        ci->tstamp);
                data = builder->str;
#if defined(HAVE_IFA_FLAGS)
        } else if (rt_attr->rta_type == IFA_FLAGS) {
                uint32_t flags = *(uint32_t *) RTA_DATA (rt_attr);
                builder = g_string_new (NULL);
                g_string_append_printf (builder, "IFA flags: 0x%04x, ", flags);
                for (uint32_t i = IFA_F_SECONDARY; i <= IFA_F_STABLE_PRIVACY;
                     i <<= 1) {
                        if (flags & i) {
                                g_string_append_printf (builder,
                                                        " %s(0x%04x)",
                                                        IFA_FLAGS_DECODE[i],
                                                        i);
                        }
                }
                data = builder->str;
#endif
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
                case IFA_MULTICAST:
                        label = "IFA_MULTICAST";
                        break;
#if defined(HAVE_IFA_FLAGS)
                case IFA_FLAGS: label = "IFA_FLAGS"; break;
#endif
                default: label = "Unknown"; break;
        }

        g_debug ("  %s(%d): %s", label, rt_attr->rta_type, data);
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

static int
get_ioctl_fd (GUPnPLinuxContextManager *self)
{
    GUPnPLinuxContextManagerPrivate *priv;

    priv = gupnp_linux_context_manager_get_instance_private (self);

    return priv->fd;
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
        strncpy (iwr.ifr_name, device->name, IFNAMSIZ - 1);
        iwr.u.essid.pointer = (caddr_t) essid;
        iwr.u.essid.length = IW_ESSID_MAX_SIZE;
        ret = ioctl (get_ioctl_fd (device->manager), SIOCGIWESSID, &iwr);

        if ((ret == 0 && essid[0] != '\0') &&
            (!device->essid || strcmp (device->essid, essid)))
                device->essid = g_strdup (essid);
        else
                old_essid = NULL;
#else
        old_essid = NULL;
#endif
        g_free (old_essid);
}

static void
network_device_create_context (NetworkInterface *device, RtmAddrInfo *info)
{
        guint port;
        GError *error = NULL;
        GUPnPContext *context;
        GSSDPUDAVersion version;

        if (g_hash_table_contains (device->contexts, info->ip_string)) {
                g_debug ("Context for address %s on %s already exists",
                         info->ip_string,
                         info->label);

                return;
        }

        g_object_get (device->manager,
                      "port", &port,
                      "uda-version", &version,
                      NULL);

        network_device_update_essid (device);

        g_autofree char *mask = g_inet_address_mask_to_string (info->mask);
        context = g_initable_new (GUPNP_TYPE_CONTEXT,
                                  NULL,
                                  &error,
                                  "address",
                                  info->address,
                                  "address-family",
                                  info->ifa->ifa_family,
                                  "uda-version",
                                  version,
                                  "interface",
                                  info->label,
                                  "network",
                                  device->essid ? device->essid : mask,
                                  "host-mask",
                                  info->mask,
                                  "port",
                                  port,
                                  NULL);

        if (error) {
                g_warning ("Error creating GUPnP context: %s",
                           error->message);
                g_error_free (error);

                return;
        }
        g_hash_table_insert (device->contexts,
                             g_steal_pointer (&info->ip_string),
                             context);

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
        if (!(device->flags & NETWORK_INTERFACE_UP))
                return;

        device->flags &= ~NETWORK_INTERFACE_UP;

        if (device->contexts != NULL)
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
static void
create_context (GUPnPLinuxContextManager *self, RtmAddrInfo *info);
static void
remove_context (GUPnPLinuxContextManager *self, RtmAddrInfo *info);

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

static RtmAddrInfo *
extract_info (struct nlmsghdr *header, gboolean dump, struct ifaddrmsg *ifa)
{
        RtmAddrInfo *info = g_new0 (RtmAddrInfo, 1);
        info->ifa = ifa;
        info->flags = ifa->ifa_flags;

        int rt_attr_len;
        struct rtattr *rt_attr;

        rt_attr = IFA_RTA (NLMSG_DATA (header));
        rt_attr_len = IFA_PAYLOAD (header);

        while (RT_ATTR_OK (rt_attr, rt_attr_len)) {
                if (dump) {
                        dump_rta_attr (ifa->ifa_family, rt_attr);
                }

                if (rt_attr->rta_type == IFA_LABEL) {
                        g_clear_pointer (&info->label, g_free);
                        info->label = g_strdup ((char *) RTA_DATA (rt_attr));
#if defined(HAVE_IFA_FLAGS)
                } else if (rt_attr->rta_type == IFA_FLAGS) {
                        // Overwrite flags with IFA_FLAGS message if present
                        info->flags = *(uint32_t *) RTA_DATA (rt_attr);
#endif
                } else if (rt_attr->rta_type == IFA_ADDRESS) {
                        g_clear_object (&info->address);
                        info->address = g_inet_address_new_from_bytes (
                                RTA_DATA (rt_attr),
                                ifa->ifa_family);

                        if (info->address != NULL) {
                                info->ip_string = g_inet_address_to_string (
                                        info->address);

                                info->mask = generate_mask (ifa, rt_attr);
                        }
                }
                rt_attr = RTA_NEXT (rt_attr, rt_attr_len);
        }

        if (dump) {
                g_autoptr (GString) builder = g_string_new ("    ");
                g_string_append_printf (builder,
                                        "IFA flags: 0x%04x, ",
                                        info->flags);
                for (uint32_t i = IFA_F_SECONDARY; i <= IFA_F_STABLE_PRIVACY;
                     i <<= 1) {
                        if (info->flags & i) {
                                g_string_append_printf (builder,
                                                        " %s(0x%04x)",
                                                        IFA_FLAGS_DECODE[i],
                                                        i);
                        }
                }

                g_debug ("%s", builder->str);
        }

        return info;
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
create_context (GUPnPLinuxContextManager *self, RtmAddrInfo *info)
{
        NetworkInterface *device;
        GUPnPLinuxContextManagerPrivate *priv;

        priv = gupnp_linux_context_manager_get_instance_private (self);
        device = g_hash_table_lookup (priv->interfaces,
                                      GINT_TO_POINTER (info->ifa->ifa_index));

        if (!device) {
                g_warning ("Got new address for device %d but device is"
                           " not active",
                           info->ifa->ifa_index);

                return;
        }

        /* If device isn't one we consider, silently skip address */
        if (device->flags & NETWORK_INTERFACE_IGNORE)
                return;

        network_device_create_context (device, info);
}

static void
remove_context (GUPnPLinuxContextManager *self, RtmAddrInfo *info)
{
        NetworkInterface *device;
        GUPnPContext *context;
        GUPnPLinuxContextManagerPrivate *priv;

        priv = gupnp_linux_context_manager_get_instance_private (self);
        device = g_hash_table_lookup (priv->interfaces,
                                      GINT_TO_POINTER (info->ifa->ifa_index));

        if (!device) {
                g_debug ("Device with index %d not found, ignoring",
                         info->ifa->ifa_index);

                return;
        }

        context = g_hash_table_lookup (device->contexts, info->ip_string);
        if (context) {
                if (device->flags & NETWORK_INTERFACE_UP) {
                        g_signal_emit_by_name (self,
                                               "context-unavailable",
                                               context);
                }
                g_hash_table_remove (device->contexts, info->ip_string);
        } else {
                g_debug ("Failed to find context with address %s",
                         info->ip_string);
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
        GUPnPLinuxContextManagerPrivate *priv;

        priv = gupnp_linux_context_manager_get_instance_private (self);
        if (priv->nl_seq == 0) {
                query_all_network_interfaces (self);

                return TRUE;
        } else if (priv->nl_seq == 1) {
                query_all_addresses (self);

                return TRUE;
        } else {
                priv->netlink_socket_source = g_socket_create_source
                                                (priv->netlink_socket,
                                                 G_IO_IN | G_IO_PRI,
                                                 NULL);

                g_source_attach (priv->netlink_socket_source,
                                 g_main_context_get_thread_default ());

                g_source_set_callback (priv->netlink_socket_source,
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
        GUPnPLinuxContextManagerPrivate *priv;

        priv = gupnp_linux_context_manager_get_instance_private (self);

        memset (&req, 0, sizeof (req));
        memset (&dest, 0, sizeof (dest));
        memset (&msg, 0, sizeof (msg));

        dest.nl_family = AF_NETLINK;
        req.hdr.nlmsg_len = NLMSG_LENGTH (sizeof (struct rtgenmsg));
        req.hdr.nlmsg_seq = priv->nl_seq++;
        req.hdr.nlmsg_type = netlink_message;
        req.hdr.nlmsg_flags = NLM_F_REQUEST | flags;
        req.gen.rtgen_family = gupnp_context_manager_get_socket_family (GUPNP_CONTEXT_MANAGER (self));

        io.iov_base = &req;
        io.iov_len = req.hdr.nlmsg_len;

        msg.msg_iov = &io;
        msg.msg_iovlen = 1;
        msg.msg_name = &dest;
        msg.msg_namelen = sizeof (dest);

        fd = g_socket_get_fd (priv->netlink_socket);
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
                              NLM_F_DUMP);
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
        GUPnPLinuxContextManagerPrivate *priv;

        priv = gupnp_linux_context_manager_get_instance_private (self);

        key = GINT_TO_POINTER (ifi->ifi_index);
        device = g_hash_table_lookup (priv->interfaces,
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

                g_hash_table_insert (priv->interfaces,
                                     key,
                                     device);
        }
}

static void
remove_device (GUPnPLinuxContextManager *self,
               struct ifinfomsg         *ifi)
{
        GUPnPLinuxContextManagerPrivate *priv;

        priv = gupnp_linux_context_manager_get_instance_private (self);

        g_hash_table_remove (priv->interfaces,
                             GINT_TO_POINTER (ifi->ifi_index));
}

#define NLMSG_IS_VALID(msg,len) \
        (NLMSG_OK(msg,len) && (msg->nlmsg_type != NLMSG_DONE))

/* Process the raw netlink message and dispatch to helper functions
 * accordingly */
static void
receive_netlink_message (GUPnPLinuxContextManager *self, GError **error)
{
        gssize len;
        GError *inner_error = NULL;
        struct nlmsghdr *header;
        struct ifinfomsg *ifi;
        struct ifaddrmsg *ifa;
        GUPnPLinuxContextManagerPrivate *priv;

        priv = gupnp_linux_context_manager_get_instance_private (self);

        header = (struct nlmsghdr *) priv->recvbuf;

        len = g_socket_receive (priv->netlink_socket,
                                priv->recvbuf,
                                sizeof (priv->recvbuf),
                                NULL,
                                &inner_error);
        if (len == -1) {
                if (inner_error->code != G_IO_ERROR_WOULD_BLOCK)
                        g_warning ("Error receiving netlink message: %s",
                                   inner_error->message);
                g_propagate_error (error, inner_error);

                return;
        }

        if (priv->dump_netlink_packets) {
                GString *hexdump = NULL;

                /* We should have at most len / 16 + 1 lines with 74 characters each */
                hexdump = g_string_new_len (NULL, ((len / 16) + 1) * 73);
                gupnp_linux_context_manager_hexdump ((guint8 *) priv->recvbuf,
                                                     len, hexdump);

                g_debug ("Netlink packet dump:\n%s", hexdump->str);
                g_string_free (hexdump, TRUE);
        }

        for (;NLMSG_IS_VALID (header, len); header = NLMSG_NEXT (header,len)) {
                switch (header->nlmsg_type) {
                /* RTM_NEWADDR and RTM_DELADDR are sent on real address
                 * changes.
                 * RTM_NEWADDR can also be sent regularly for information
                 * about v6 address lifetime
                 * RTM_NEWLINK is sent on various occasions:
                 *  - Creation of a new device
                 *  - Device goes up/down
                 *  - Wireless status changes
                 * RTM_DELLINK is sent only if device is removed, like
                 * openvpn --rmtun /dev/tun0, NOT on ifconfig down. */
                case RTM_NEWADDR: {
                        g_debug ("Received RTM_NEWADDR");
                        ifa = NLMSG_DATA (header);

#ifdef __clang_analyzer__
                        [[clang::suppress]]
#endif
                        g_autoptr (RtmAddrInfo) info =
                                extract_info (header,
                                              priv->dump_netlink_packets,
                                              ifa);

                        if (info->flags & IFA_F_TENTATIVE) {
                                g_debug ("IP address %s is only tentative, "
                                         "skipping",
                                         info->ip_string);
                                continue;
                        }

                        if (info->flags & IFA_F_DEPRECATED) {
                                g_debug ("Ip address %s is deprecated, "
                                         "skipping",
                                         info->ip_string);
                                continue;
                        }

                        if (info->address != NULL) {
                                create_context (self, info);
                        }
                } break;
                        case RTM_DELADDR:
                            {
                                g_debug ("Received RTM_DELADDR");
                                ifa = NLMSG_DATA (header);

                                g_autoptr (RtmAddrInfo) info = extract_info (
                                        header,
                                        priv->dump_netlink_packets,
                                        ifa);

                                if (info->address != NULL) {
                                        remove_context (self, info);
                                }
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
        GUPnPLinuxContextManagerPrivate *priv;

        priv = gupnp_linux_context_manager_get_instance_private (self);

        priv->fd = socket (AF_INET, SOCK_DGRAM, 0);

        if (priv->fd < 0) {
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
        GUPnPLinuxContextManagerPrivate *priv;

        priv = gupnp_linux_context_manager_get_instance_private (self);

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
        sa.nl_groups = RTMGRP_LINK;
        if (gupnp_context_manager_get_socket_family (GUPNP_CONTEXT_MANAGER (self)) == G_SOCKET_FAMILY_INVALID) {
                sa.nl_groups |= RTMGRP_IPV6_IFADDR | RTMGRP_IPV4_IFADDR;
        } else if (gupnp_context_manager_get_socket_family (GUPNP_CONTEXT_MANAGER (self)) == G_SOCKET_FAMILY_IPV4) {
                sa.nl_groups |= RTMGRP_IPV4_IFADDR;
        } else if (gupnp_context_manager_get_socket_family (GUPNP_CONTEXT_MANAGER (self)) == G_SOCKET_FAMILY_IPV6) {
                sa.nl_groups |= RTMGRP_IPV6_IFADDR;
        }

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

        priv->netlink_socket = sock;

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
        GUPnPLinuxContextManagerPrivate *priv;

        priv = gupnp_linux_context_manager_get_instance_private (self);
        priv->fd = -1;

        priv->nl_seq = 0;

        priv->interfaces =
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
        GUPnPLinuxContextManagerPrivate *priv;

        self = GUPNP_LINUX_CONTEXT_MANAGER (object);
        priv = gupnp_linux_context_manager_get_instance_private (self);

        env_var = g_getenv ("GUPNP_DEBUG_NETLINK");
        priv->dump_netlink_packets = (env_var != NULL) &&
                strstr (env_var, "1") != NULL;

        if (!create_ioctl_socket (self, &error))
                goto cleanup;

        if (!create_netlink_socket (self, &error))
                goto cleanup;

        priv->bootstrap_source = g_idle_source_new ();
        g_source_attach (priv->bootstrap_source,
                         g_main_context_get_thread_default ());
        g_source_set_callback (priv->bootstrap_source,
                               (GSourceFunc) on_bootstrap,
                               self,
                               NULL);
cleanup:
        if (error) {
                if (priv->fd >= 0) {
                        close (priv->fd);
                        priv->fd = -1;
                }

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
        GUPnPLinuxContextManagerPrivate *priv;

        self = GUPNP_LINUX_CONTEXT_MANAGER (object);
        priv = gupnp_linux_context_manager_get_instance_private (self);

        if (priv->bootstrap_source != NULL) {
                g_source_destroy (priv->bootstrap_source);
                g_source_unref (priv->bootstrap_source);
                priv->bootstrap_source = NULL;
        }

        if (priv->netlink_socket_source != NULL) {
               g_source_destroy (priv->netlink_socket_source);
               g_source_unref (priv->netlink_socket_source);
               priv->netlink_socket_source = NULL;
        }

        if (priv->netlink_socket != NULL) {
                g_object_unref (priv->netlink_socket);
                priv->netlink_socket = NULL;
        }

        if (priv->fd >= 0) {
                close (priv->fd);
                priv->fd = -1;
        }

        if (priv->interfaces) {
                g_hash_table_destroy (priv->interfaces);
                priv->interfaces = NULL;
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
}
