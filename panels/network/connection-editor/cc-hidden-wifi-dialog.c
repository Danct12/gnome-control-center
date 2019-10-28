/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* cc-hidden-wifi-dialog.c
 *
 * Copyright 2019 Purism SPC
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Author(s):
 *   Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "cc-hidden-wifi-dialog"

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <glib/gi18n.h>
#include <NetworkManager.h>

#include "../cc-network-panel.h"
#include "list-box-helper.h"
#include "cc-list-row.h"
#include "ce-ip-page.h"
#include "ce-details-page.h"
#include "ce-security-page.h"
#include "cc-hidden-wifi-dialog.h"

/**
 * @short_description: Connection Editor dialog
 * @include: "cc-hidden-wifi-dialog.h"
 *
 * A Dialog to modify connection settings
 */

struct _CcHiddenWifiDialog
{
  HdyDialog      parent_instance;

  GtkButton     *cancel_button;
  GtkButton     *connect_button;

  GtkScrolledWindow  *main_view;
  GtkBox         *main_page;
  CeSecurityPage *security_page;

  NMClient      *nm_client;
  NMDevice      *device;
  NMAccessPoint *ap;

  /* Set after current view changed, reset after scroll set */
  gboolean       view_changed;
};

G_DEFINE_TYPE (CcHiddenWifiDialog, cc_hidden_wifi_dialog, HDY_TYPE_DIALOG)

static void
hidden_wifi_settings_changed_cb (CcHiddenWifiDialog *self)
{
  gboolean has_error;

  g_assert (CC_IS_HIDDEN_WIFI_DIALOG (self));

  has_error = ce_security_page_has_error (self->security_page);
  gtk_widget_set_sensitive (GTK_WIDGET (self->connect_button), !has_error);
}

static void
cc_hidden_wifi_dialog_response (GtkDialog *dialog,
                                gint       response_id)
{
  CcHiddenWifiDialog *self = (CcHiddenWifiDialog *)dialog;
  g_autoptr(GVariant) settings = NULL;

  g_assert (0);
  if (response_id != GTK_RESPONSE_OK)
    return;

  /* settings = nm_connection_to_dbus (self->connection, NM_CONNECTION_SERIALIZE_ALL); */
  /* nm_connection_replace_settings (self->orig_connection, settings, &error); */

  /* if (error) */
  /*   g_warning ("Error replacing settings: %s", error->message); */
  /* else */
  /*   nm_remote_connection_commit_changes (NM_REMOTE_CONNECTION (self->orig_connection), */
  /*                                        TRUE, */
  /*                                        NULL, /\* cancellable *\/ */
  /*                                        &error); */
  /* if (error) */
  /*   g_warning ("Error saving settings: %s", error->message); */


  ce_security_page_save_connection (self->security_page);
  /* settings = nm_connection_to_dbus (self->connection, NM_CONNECTION_SERIALIZE_ALL); */
  /* nm_connection_replace_settings (self->orig_connection, settings, NULL); */

  /* nm_remote_connection_commit_changes_async (NM_REMOTE_CONNECTION (self->orig_connection), */
  /*                                            TRUE, */
  /*                                            NULL, /\* cancellable *\/ */
  /*                                            NULL, */
  /*                                            NULL); */
}

static void
cc_hidden_wifi_dialog_finalize (GObject *object)
{
  CcHiddenWifiDialog *self = (CcHiddenWifiDialog *)object;

  g_clear_object (&self->device);
  g_object_unref (self->nm_client);

  G_OBJECT_CLASS (cc_hidden_wifi_dialog_parent_class)->finalize (object);
}

static void
cc_hidden_wifi_dialog_class_init (CcHiddenWifiDialogClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = cc_hidden_wifi_dialog_finalize;

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/control-center/"
                                               "network/cc-hidden-wifi-dialog.ui");

  gtk_widget_class_bind_template_child (widget_class, CcHiddenWifiDialog, cancel_button);
  gtk_widget_class_bind_template_child (widget_class, CcHiddenWifiDialog, connect_button);

  gtk_widget_class_bind_template_child (widget_class, CcHiddenWifiDialog, main_view);
  gtk_widget_class_bind_template_child (widget_class, CcHiddenWifiDialog, main_page);
  gtk_widget_class_bind_template_child (widget_class, CcHiddenWifiDialog, security_page);

  gtk_widget_class_bind_template_callback (widget_class, hidden_wifi_settings_changed_cb);

  g_type_ensure (CC_TYPE_LIST_ROW);
  g_type_ensure (CC_TYPE_NETWORK_PANEL);
}

static void
cc_hidden_wifi_dialog_init (CcHiddenWifiDialog *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget *
cc_hidden_wifi_dialog_new (GtkWindow *parent_window,
                           NMClient  *nm_client)
{
  CcHiddenWifiDialog *self;

  g_return_val_if_fail (GTK_IS_WINDOW (parent_window), NULL);
  g_return_val_if_fail (NM_IS_CLIENT (nm_client), NULL);

  self = g_object_new (CC_TYPE_HIDDEN_WIFI_DIALOG,
                       "transient-for", parent_window,
                       "use-header-bar", TRUE,
                       NULL);
  self->nm_client = g_object_ref (nm_client);
  ce_security_page_set_nm_client (self->security_page, nm_client);

  return GTK_WIDGET (self);
}

void
cc_hidden_wifi_dialog_set_device (CcHiddenWifiDialog *self,
                                  NMDevice           *device)
{
  g_return_if_fail (CC_IS_HIDDEN_WIFI_DIALOG (self));
  g_return_if_fail (NM_IS_DEVICE (device));

  g_set_object (&self->device, device);

  ce_security_page_set_connection (self->security_page, NULL, NULL, device);
}

void
cc_hidden_wifi_dialog_set_ap (CcHiddenWifiDialog *self,
                              NMAccessPoint      *ap)
{
  g_return_if_fail (CC_IS_HIDDEN_WIFI_DIALOG (self));
  g_return_if_fail (!ap || NM_IS_ACCESS_POINT (ap));

  g_set_object (&self->ap, ap);
  /* if (g_set_object (&self->ap, ap)) */
  /*   cc_hidden_wifi_dialog_update_ap (self); */
}

NMConnection *
cc_wifi_hidden_dialog_get_connection (CcHiddenWifiDialog  *self,
                                      NMDevice           **device,
                                      NMAccessPoint      **ap)
{
  NMConnection *connection;

  g_return_val_if_fail (CC_IS_HIDDEN_WIFI_DIALOG (self), NULL);

  if (device)
    *device = g_object_ref (self->device);

  if (ap && self->ap)
    *ap = g_object_ref (self->ap);

  ce_security_page_save_connection (self->security_page);
  connection = ce_security_page_get_connection (self->security_page);

  return connection ? g_object_ref (connection) : NULL;
  /* return g_object_ref (self->orig_connection); */
}
