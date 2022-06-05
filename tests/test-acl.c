/*
 * Copyright (C) 2021 Jens Georg.
 *
 * Author: Jens Georg <mail@jensge.org>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <config.h>

#include "libgupnp/gupnp.h"

#include <string.h>
#include <libsoup/soup.h>

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>

static GUPnPContext *
create_context (const char *localhost, guint16 port, GError **error)
{
        return GUPNP_CONTEXT (g_initable_new (GUPNP_TYPE_CONTEXT,
                                              NULL,
                                              error,
                                              "host-ip",
                                              localhost,
                                              "port",
                                              port,
                                              NULL));
}


typedef struct {
        GMainLoop *loop;
        GUPnPContext *context;
        SoupSession *session;
        char *base_uri;
} ContextTestFixture;

static void
test_fixture_setup (ContextTestFixture *tf, gconstpointer user_data)
{
        GError *error = NULL;

        tf->context = create_context ((const char *) user_data, 0, &error);

        g_assert_nonnull (tf->context);
        g_assert_no_error (error);

        tf->loop = g_main_loop_new (NULL, FALSE);
        tf->session = soup_session_new ();

        GSList *uris =
                soup_server_get_uris (gupnp_context_get_server (tf->context));
        g_assert_nonnull (uris);
        g_assert_cmpint (g_slist_length (uris), ==, 1);

        tf->base_uri = g_uri_to_string (uris->data);
        g_slist_free_full (uris, (GDestroyNotify) g_uri_unref);
}

static gboolean
delayed_loop_quitter (gpointer user_data)
{
        g_main_loop_quit (user_data);
        return G_SOURCE_REMOVE;
}

static void
test_fixture_teardown (ContextTestFixture *tf, gconstpointer user_data)
{
        g_free (tf->base_uri);
        g_object_unref (tf->context);

        // Make sure the source teardown handlers get run so we don't confuse valgrind
        g_timeout_add (500, (GSourceFunc) delayed_loop_quitter, tf->loop);
        g_main_loop_run (tf->loop);
        g_main_loop_unref (tf->loop);
        g_object_unref (tf->session);
}

typedef struct _DefaultCallbackData {
        GBytes *bytes;
        GMainLoop *loop;
} DefaultCallbackData;

void
soup_message_default_callback (GObject *source,
                               GAsyncResult *res,
                               gpointer user_data)
{
        DefaultCallbackData *d = user_data;
        GError *error = NULL;
        d->bytes = soup_session_send_and_read_finish (SOUP_SESSION (source),
                                                      res,
                                                      &error);
        g_assert_no_error (error);
        g_assert_nonnull (d->bytes);
        g_bytes_unref (d->bytes);
        g_main_loop_quit (d->loop);
}

static GUPnPAclInterface *acl_parent_iface = NULL;

static gboolean
test_acl_can_sync (GUPnPAcl *acl);

static gboolean
test_acl_is_allowed (GUPnPAcl *self,
                     struct _GUPnPDevice *device,
                     struct _GUPnPService *service,
                     const char *path,
                     const char *address,
                     const char *agent);

static void
test_acl_is_allowed_async (GUPnPAcl *acl,
                           struct _GUPnPDevice *device,
                           struct _GUPnPService *service,
                           const char *path,
                           const char *address,
                           const char *agent,
                           GCancellable *cancellable,
                           GAsyncReadyCallback callback,
                           gpointer user_data);

static gboolean
test_acl_is_allowed_finish (GUPnPAcl *acl, GAsyncResult *res, GError **error);

static void
test_acl_interface_init (gpointer g_iface, gpointer iface_data)
{
        GUPnPAclInterface *iface = (GUPnPAclInterface *) g_iface;
        acl_parent_iface = g_type_interface_peek_parent (g_iface);
        iface->can_sync = test_acl_can_sync;
        iface->is_allowed = test_acl_is_allowed;
        iface->is_allowed_async = test_acl_is_allowed_async;
        iface->is_allowed_finish = test_acl_is_allowed_finish;
}

struct _TestAcl {
        GObject parent;

        gboolean can_sync;
        gboolean is_allowed;
        gboolean finish_should_fail;

        int can_sync_called;
        int is_allowed_called;
        int is_allowed_async_called;
        int is_allowed_finish_called;
};


G_DECLARE_FINAL_TYPE (TestAcl, test_acl, TEST, ACL, GObject)

static void
test_acl_init (TestAcl *instance)
{}

G_DEFINE_TYPE_EXTENDED (TestAcl,
                        test_acl,
                        G_TYPE_OBJECT,
                        0,
                        G_IMPLEMENT_INTERFACE (GUPNP_TYPE_ACL,
                                               test_acl_interface_init))

static void
test_acl_class_init (TestAclClass *klass)
{}


static gboolean
test_acl_can_sync (GUPnPAcl *acl)
{
        TestAcl *self = TEST_ACL (acl);

        self->can_sync_called++;

        return self->can_sync;
}

static gboolean
test_acl_is_allowed (GUPnPAcl *acl,
                     struct _GUPnPDevice *device,
                     struct _GUPnPService *service,
                     const char *path,
                     const char *address,
                     const char *agent)
{
        TestAcl *self = TEST_ACL (acl);

        self->is_allowed_called++;

        return self->is_allowed;
}

static gboolean
on_timeout (gpointer user_data)
{
        TestAcl *self =
                TEST_ACL (g_task_get_source_object (G_TASK (user_data)));
        g_task_return_boolean (G_TASK (user_data), self->is_allowed);
        g_object_unref (G_OBJECT (user_data));

        return G_SOURCE_REMOVE;
}

static void
test_acl_is_allowed_async (GUPnPAcl *acl,
                           struct _GUPnPDevice *device,
                           struct _GUPnPService *service,
                           const char *path,
                           const char *address,
                           const char *agent,
                           GCancellable *cancellable,
                           GAsyncReadyCallback callback,
                           gpointer user_data)
{
        TestAcl *self = TEST_ACL (acl);
        GTask *task = g_task_new (self, cancellable, callback, user_data);

        self->is_allowed_async_called++;

        g_timeout_add (100, on_timeout, task);
}

static gboolean
test_acl_is_allowed_finish (GUPnPAcl *acl, GAsyncResult *res, GError **error)
{

        TestAcl *self = TEST_ACL (acl);

        self->is_allowed_finish_called++;

        if (g_task_is_valid (res, acl)) {
                return self->is_allowed;
        }

        return FALSE;
}

static void
acl_test_handler (SoupServer *server,
                  SoupServerMessage *msg,
                  const char *path,
                  GHashTable *query,
                  gpointer user_data)
{
        soup_server_message_set_status (msg, SOUP_STATUS_OK, NULL);
}

void
destroy_server_handler_data (gpointer data)
{
        (*(int *) (data))++;
}

static void
test_gupnp_context_acl (ContextTestFixture *tf, gconstpointer user_data)
{
        GError *error = NULL;
        DefaultCallbackData d = { .loop = tf->loop };
        int destroy_called = 0;

        // There should be no ACL
        g_assert_null (gupnp_context_get_acl (tf->context));

        TestAcl *acl = g_object_new (test_acl_get_type (), NULL);
        acl->can_sync = TRUE;
        acl->is_allowed = TRUE;

        gupnp_context_set_acl (tf->context, GUPNP_ACL (acl));

        g_assert (acl == (TestAcl *) gupnp_context_get_acl (tf->context));

        gupnp_context_add_server_handler (tf->context,
                                          FALSE,
                                          "/foo",
                                          acl_test_handler,
                                          &destroy_called,
                                          destroy_server_handler_data);

        // pass on a query, to cover more branches
        char *uri = g_uri_resolve_relative (tf->base_uri,
                                            "/foo?foo=bar&bar=baz",
                                            0,
                                            &error);
        g_assert_nonnull (uri);
        g_assert_no_error (error);

        char *request_uri = gupnp_context_rewrite_uri (tf->context, uri);
        g_free (uri);

        SoupMessage *msg = soup_message_new (SOUP_METHOD_HEAD, request_uri);
        soup_session_send_and_read_async (tf->session,
                                          msg,
                                          G_PRIORITY_DEFAULT,
                                          NULL,
                                          soup_message_default_callback,
                                          &d);
        g_main_loop_run (tf->loop);

        g_assert_cmpint (soup_message_get_status (msg), ==, SOUP_STATUS_OK);
        g_assert_cmpint (acl->can_sync_called, ==, 0);
        g_assert_cmpint (acl->is_allowed_called, ==, 0);
        g_assert_cmpint (acl->is_allowed_async_called, ==, 0);
        g_assert_cmpint (acl->is_allowed_finish_called, ==, 0);

        soup_server_remove_handler (gupnp_context_get_server (tf->context),
                                    "/foo");
        g_assert_cmpint (destroy_called, ==, 1);
        destroy_called = FALSE;

        gupnp_context_add_server_handler (tf->context,
                                          TRUE,
                                          "/foo",
                                          acl_test_handler,
                                          &destroy_called,
                                          destroy_server_handler_data);

        g_object_unref (msg);
        msg = soup_message_new (SOUP_METHOD_HEAD, request_uri);
        soup_session_send_and_read_async (tf->session,
                                          msg,
                                          G_PRIORITY_DEFAULT,
                                          NULL,
                                          soup_message_default_callback,
                                          &d);
        g_main_loop_run (tf->loop);

        g_assert_cmpint (soup_message_get_status (msg), ==, SOUP_STATUS_OK);
        g_assert_cmpint (acl->can_sync_called, ==, 1);
        g_assert_cmpint (acl->is_allowed_called, ==, 1);
        g_assert_cmpint (acl->is_allowed_async_called, ==, 0);
        g_assert_cmpint (acl->is_allowed_finish_called, ==, 0);


        acl->is_allowed_called = 0;
        acl->can_sync_called = 0;
        acl->can_sync = FALSE;
        g_object_unref (msg);
        msg = soup_message_new (SOUP_METHOD_HEAD, request_uri);
        soup_session_send_and_read_async (tf->session,
                                          msg,
                                          G_PRIORITY_DEFAULT,
                                          NULL,
                                          soup_message_default_callback,
                                          &d);
        g_main_loop_run (tf->loop);

        g_assert_cmpint (soup_message_get_status (msg), ==, SOUP_STATUS_OK);
        g_assert_cmpint (acl->can_sync_called, ==, 1);
        g_assert_cmpint (acl->is_allowed_called, ==, 0);
        g_assert_cmpint (acl->is_allowed_async_called, ==, 1);
        g_assert_cmpint (acl->is_allowed_finish_called, ==, 1);

        acl->is_allowed = FALSE;
        g_object_unref (msg);
        msg = soup_message_new (SOUP_METHOD_HEAD, request_uri);
        soup_session_send_and_read_async (tf->session,
                                          msg,
                                          G_PRIORITY_DEFAULT,
                                          NULL,
                                          soup_message_default_callback,
                                          &d);
        g_main_loop_run (tf->loop);

        // This should yield forbidden, even if the handler only returns OK
        g_assert_cmpint (soup_message_get_status (msg),
                         ==,
                         SOUP_STATUS_FORBIDDEN);

        acl->is_allowed = FALSE;
        acl->can_sync = TRUE;
        g_object_unref (msg);
        msg = soup_message_new (SOUP_METHOD_HEAD, request_uri);
        soup_session_send_and_read_async (tf->session,
                                          msg,
                                          G_PRIORITY_DEFAULT,
                                          NULL,
                                          soup_message_default_callback,
                                          &d);
        g_main_loop_run (tf->loop);

        // This should yield forbidden, even if the handler only returns OK
        g_assert_cmpint (soup_message_get_status (msg),
                         ==,
                         SOUP_STATUS_FORBIDDEN);

        soup_server_remove_handler (gupnp_context_get_server (tf->context),
                                    "/foo");

        g_assert_cmpint (destroy_called, ==, 1);

        g_object_unref (msg);
        g_free (request_uri);

        g_object_unref (acl);
}

int
main (int argc, char *argv[])
{
        g_test_init (&argc, &argv, NULL);

        GPtrArray *addresses = g_ptr_array_sized_new (2);
        g_ptr_array_add (addresses, g_strdup ("127.0.0.1"));

        // Detect if there is a special network device with "proper" ip addresses to check also link-local addresses
        GUPnPContext *c = g_initable_new (GUPNP_TYPE_CONTEXT,
                                          NULL,
                                          NULL,
                                          "host-ip",
                                          "::1",
                                          NULL);

        if (c != NULL) {
                g_ptr_array_add (
                        addresses,
                        g_strdup (gssdp_client_get_host_ip (GSSDP_CLIENT (c))));
                g_object_unref (c);
        }

        c = g_initable_new (GUPNP_TYPE_CONTEXT,
                            NULL,
                            NULL,
                            "interface",
                            "gupnp0",
                            "address-family",
                            G_SOCKET_FAMILY_IPV4,
                            NULL);

        if (c != NULL) {
                g_ptr_array_add (
                        addresses,
                        g_strdup (gssdp_client_get_host_ip (GSSDP_CLIENT (c))));
                g_object_unref (c);
        }

        c = g_initable_new (GUPNP_TYPE_CONTEXT,
                            NULL,
                            NULL,
                            "interface",
                            "gupnp0",
                            "address-family",
                            G_SOCKET_FAMILY_IPV6,
                            NULL);

        if (c != NULL) {
                g_ptr_array_add (
                        addresses,
                        g_strdup (gssdp_client_get_host_ip (GSSDP_CLIENT (c))));
                g_object_unref (c);
        }

        g_ptr_array_add (addresses, NULL);

        char **data = (char **) g_ptr_array_free (addresses, FALSE);
        char **it = data;
        while (*it != NULL) {
                char *name = NULL;

                name = g_strdup_printf ("/context/http/acl/%s",
                                        *it);
                g_test_add (name,
                            ContextTestFixture,
                            *it,
                            test_fixture_setup,
                            test_gupnp_context_acl,
                            test_fixture_teardown);
                g_free (name);
                it++;
        }

        int result = g_test_run ();

        g_strfreev (data);

        return result;
}

