/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* cc-network-editor.c
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
#define G_LOG_DOMAIN "cc-connection-editor"

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
#include "cc-connection-editor.h"

/**
 * @short_description: Connection Editor dialog
 * @include: "cc-connection-editor.h"
 *
 * A Dialog to modify connection settings
 */

typedef enum
{
  NET_TYPE_NONE,
  NET_TYPE_WIFI,
  NET_TYPE_ETHERNET,
  NET_TYPE_VPN
} NetType;

struct _CcConnectionEditor
{
  HdyDialog      parent_instance;

  GtkAdjustment *vadjustment;
  gdouble        vadjustment_value;

  GtkButton     *back_button;
  GtkButton     *cancel_button;
  GtkButton     *apply_button;

  GtkScrolledWindow  *main_view;
  GtkStack      *main_stack;
  GtkBox        *main_page;

  GtkListBox    *details_box;
  CcListRow     *signal_row;
  CcListRow     *frequency_row;
  CcListRow     *route_row;
  CcListRow     *last_used_row;
  CcListRow     *details_row;

  GtkListBox    *switch_box;
  CcListRow     *auto_connect_row;
  CcListRow     *allow_others_row;
  CcListRow     *metered_row;

  GtkListBox    *advanced_box;
  CcListRow     *ip4_row;
  CcListRow     *ip6_row;
  CcListRow     *security_row;

  CeDetailsPage *details_page;
  CeIpPage      *ip4_page;
  CeIpPage      *ip6_page;
  CeSecurityPage *security_page;

  NMClient      *nm_client;
  NMDevice      *device;
  NMConnection  *orig_connection;
  NMConnection  *connection;
  NMAccessPoint *ap;
  NetType        network_type;

  /* Set after current view changed, reset after scroll set */
  gboolean       view_changed;
};

G_DEFINE_TYPE (CcConnectionEditor, cc_connection_editor, HDY_TYPE_DIALOG)

static void
cc_connection_editor_update_ap (CcConnectionEditor *self)
{
  CeDetailsPage *details_page;
  gchar *str;

  g_assert (CC_IS_CONNECTION_EDITOR (self));

  details_page = self->details_page;
  ce_details_page_set_ap (details_page, self->ap);

  gtk_widget_set_visible (GTK_WIDGET (self->signal_row), self->ap != NULL);
  gtk_widget_set_visible (GTK_WIDGET (self->frequency_row), self->ap != NULL);

  cc_list_row_set_secondary_label (self->signal_row,
                                   ce_details_page_get_ap_strength (details_page));

  str = ce_details_page_get_ap_frequency (details_page);
  cc_list_row_set_secondary_label (self->frequency_row, str);
  g_free (str);

  str = ce_details_page_get_security (details_page);
  cc_list_row_set_secondary_label (self->security_row, str);
  g_free (str);
}

static void
editor_visible_child_changed_cb (CcConnectionEditor *self)
{
  GtkWidget *visible_child;
  const gchar *title = "";

  g_assert (CC_IS_CONNECTION_EDITOR (self));

  visible_child = gtk_stack_get_visible_child (self->main_stack);

  if (visible_child == GTK_WIDGET (self->main_page))
    title = _("Network Settings");
  else if (visible_child == GTK_WIDGET (self->details_page))
    title = _("Network Details");
  else if (visible_child == GTK_WIDGET (self->ip4_page))
    title = _("Network IPv4 Settings");
  else if (visible_child == GTK_WIDGET (self->ip6_page))
    title = _("Network IPv6 Settings");
  else if (visible_child == GTK_WIDGET (self->security_page))
    title = _("Network Security Settings");
  else
    g_return_if_reached ();

  gtk_window_set_title (GTK_WINDOW (self), title);

  /* Update Apply button */
  gtk_widget_set_sensitive (GTK_WIDGET (self->apply_button), FALSE);

  if (visible_child == GTK_WIDGET (self->main_page) ||
      visible_child == GTK_WIDGET (self->details_page))
    gtk_widget_hide (GTK_WIDGET (self->apply_button));
  else
    gtk_widget_show (GTK_WIDGET (self->apply_button));
}

static void
editor_cancel_clicked_cb (CcConnectionEditor *self)
{
  GtkWidget *visible_child;

  g_assert (CC_IS_CONNECTION_EDITOR (self));

  visible_child = gtk_stack_get_visible_child (self->main_stack);

  if (visible_child == GTK_WIDGET (self->main_page))
    gtk_dialog_response (GTK_DIALOG (self), GTK_RESPONSE_CANCEL);
  else
    gtk_stack_set_visible_child (self->main_stack, GTK_WIDGET (self->main_page));

  gtk_widget_show (GTK_WIDGET (self->back_button));
}

static void
editor_apply_clicked_cb (CcConnectionEditor *self)
{
  GtkWidget *visible_child;
  g_autoptr(GVariant) settings = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (CC_IS_CONNECTION_EDITOR (self));

  visible_child = gtk_stack_get_visible_child (self->main_stack);

  if (visible_child == GTK_WIDGET (self->ip4_page))
    ce_ip_page_save_connection (self->ip4_page);
  else if (visible_child == GTK_WIDGET (self->ip6_page))
    ce_ip_page_save_connection (self->ip6_page);
  else if (visible_child == GTK_WIDGET (self->security_page))
    ce_security_page_save_connection (self->security_page);
  else
    g_return_if_reached ();

  settings = nm_connection_to_dbus (self->connection, NM_CONNECTION_SERIALIZE_ALL);
  nm_connection_replace_settings (self->orig_connection, settings, &error);

  if (error)
    g_warning ("Error replacing settings: %s", error->message);
  else
    nm_remote_connection_commit_changes (NM_REMOTE_CONNECTION (self->orig_connection),
                                         TRUE,
                                         NULL, /* cancellable */
                                         &error);
  if (error)
    g_warning ("Error saving settings: %s", error->message);

  gtk_stack_set_visible_child (self->main_stack, GTK_WIDGET (self->main_page));
}

static void
editor_row_activated_cb (CcConnectionEditor *self,
                         CcListRow          *row)
{
  g_assert (CC_IS_CONNECTION_EDITOR (self));
  g_assert (GTK_IS_LIST_BOX_ROW (row));

  gtk_widget_show (GTK_WIDGET (self->cancel_button));
  self->vadjustment_value = gtk_adjustment_get_value (self->vadjustment);
  self->view_changed = TRUE;

  if (row == self->details_row)
    {
      ce_details_page_refresh (self->details_page);
      gtk_widget_show (GTK_WIDGET (self->back_button));
      gtk_stack_set_visible_child (self->main_stack, GTK_WIDGET (self->details_page));
    }
  else if (row == self->ip4_row)
    {
      ce_ip_page_refresh (self->ip4_page);
      gtk_stack_set_visible_child (self->main_stack, GTK_WIDGET (self->ip4_page));
    }
  else if (row == self->ip6_row)
    {
      ce_ip_page_refresh (self->ip6_page);
      gtk_stack_set_visible_child (self->main_stack, GTK_WIDGET (self->ip6_page));
    }
  else if (row == self->security_row)
    {
      ce_security_page_refresh (self->security_page);
      gtk_stack_set_visible_child (self->main_stack, GTK_WIDGET (self->security_page));
    }
  else
    g_return_if_reached ();     /* XXX: debug, replace with return */
}

static void
ce_main_view_scroll_changed_cb (CcConnectionEditor *self)
{
  GtkWidget *current_view;

  g_assert (CC_IS_CONNECTION_EDITOR (self));

  current_view = gtk_stack_get_visible_child (self->main_stack);

  if (self->view_changed && current_view == GTK_WIDGET (self->main_page))
    {
      gtk_adjustment_set_value (self->vadjustment, self->vadjustment_value);
      self->view_changed = FALSE;
    }
  else if (self->view_changed)
    gtk_adjustment_set_value (self->vadjustment, 0.0);
}

static void
editor_settings_changed_cb (CcConnectionEditor *self)
{
  GtkWidget *visible_child;
  gboolean has_error = TRUE;

  g_assert (CC_IS_CONNECTION_EDITOR (self));

  visible_child = gtk_stack_get_visible_child (self->main_stack);

  if (visible_child == GTK_WIDGET (self->main_page))
    return;

  if (visible_child == GTK_WIDGET (self->ip4_page))
    has_error = ce_ip_page_has_error (self->ip4_page);
  else if (visible_child == GTK_WIDGET (self->ip6_page))
    has_error = ce_ip_page_has_error (self->ip6_page);
  else if (visible_child == GTK_WIDGET (self->security_page))
    has_error = ce_security_page_has_error (self->security_page);
  else
    g_warn_if_reached ();

  gtk_widget_set_sensitive (GTK_WIDGET (self->apply_button), !has_error);
}

static void
editor_row_changed_cb (CcConnectionEditor *self,
                       GParamSpec         *psec,
                       CcListRow          *row)
{
  NMSettingConnection *con_setting;
  g_autoptr(GError) error = NULL;
  gboolean active;

  g_assert (CC_IS_CONNECTION_EDITOR (self));
  g_assert (CC_IS_LIST_ROW (row));

  /* We are saving to the original connection, not clone */
  con_setting = nm_connection_get_setting_connection (self->orig_connection);
  g_object_get (row, "active", &active, NULL);

  if (row == self->allow_others_row)
    {
      g_object_set (con_setting, "permissions", NULL, NULL);

      /* Ie,  Don’t allow others */
      if (!active)
        nm_setting_connection_add_permission (con_setting, "user", g_get_user_name (), NULL);
    }
  else if (row == self->auto_connect_row)
    g_object_set (con_setting, "autoconnect", active, NULL);
  else if (row == self->metered_row)
    g_object_set (con_setting, "metered", active, NULL);
  else
    g_return_if_reached ();

  /* XXX: Currently all networks are assumed to be already saved */
  if (!NM_IS_REMOTE_CONNECTION (self->orig_connection))
    g_return_if_reached ();

  nm_remote_connection_commit_changes (NM_REMOTE_CONNECTION (self->orig_connection),
                                       TRUE, NULL, &error);
  if (error)
    g_warning ("Error saving settings: %s", error->message);
}

static void
editor_delete_clicked_cb (CcConnectionEditor *self)
{
  g_assert (CC_IS_CONNECTION_EDITOR (self));

  /* XXX: Don't close on fail? */
  nm_remote_connection_delete (NM_REMOTE_CONNECTION (self->orig_connection),
                               NULL, NULL);
  gtk_dialog_response (GTK_DIALOG (self), GTK_RESPONSE_CLOSE);
}

static void
cc_connection_editor_response (GtkDialog *dialog,
                               gint       response_id)
{
  CcConnectionEditor *self = (CcConnectionEditor *)dialog;
  g_autoptr(GVariant) settings = NULL;

  if (response_id != GTK_RESPONSE_APPLY)
    return;

  ce_ip_page_save_connection (self->ip4_page);
  settings = nm_connection_to_dbus (self->connection, NM_CONNECTION_SERIALIZE_ALL);
  nm_connection_replace_settings (self->orig_connection, settings, NULL);

  ce_ip_page_save_connection (self->ip6_page);
  settings = nm_connection_to_dbus (self->connection, NM_CONNECTION_SERIALIZE_ALL);
  nm_connection_replace_settings (self->orig_connection, settings, NULL);

  ce_security_page_save_connection (self->security_page);
  settings = nm_connection_to_dbus (self->connection, NM_CONNECTION_SERIALIZE_ALL);
  nm_connection_replace_settings (self->orig_connection, settings, NULL);

  nm_remote_connection_commit_changes_async (NM_REMOTE_CONNECTION (self->orig_connection),
                                             TRUE,
                                             NULL, /* cancellable */
                                             NULL,
                                             NULL);
}

static void
cc_connection_editor_show (GtkWidget *widget)
{
  CcConnectionEditor *self = (CcConnectionEditor *)widget;
/*   NMSettingConnection *con_setting; */
/*   const gchar *type; */
/*   gboolean others_allowed, auto_connect; */

  self->vadjustment_value = 0.0;
  gtk_adjustment_set_value (self->vadjustment, 0.0);
/*   con_setting = nm_connection_get_setting_connection (self->connection); */

/*   auto_connect = nm_setting_connection_get_autoconnect (con_setting); */
/*   g_object_set (self->auto_connect_row, "active", auto_connect, NULL); */

/*   /\* If no users are added, all users are allowed *\/ */
/*   others_allowed = nm_setting_connection_get_num_permissions (con_setting) == 0; */
/*   g_object_set (self->allow_others_row, "active", others_allowed, NULL); */

/*   /\* Disable for VPN; NetworkManager does not implement that yet (see */
/*    * bug https://bugzilla.gnome.org/show_bug.cgi?id=792618) *\/ */
/*   type = nm_setting_connection_get_connection_type (con_setting); */
/*   if (type && !g_str_equal (type, NM_SETTING_VPN_SETTING_NAME)) */
/*     { */
/*       NMMetered metered; */

/*       metered = nm_setting_connection_get_metered (con_setting); */

/*       if (metered == NM_METERED_YES || metered == NM_METERED_GUESS_YES) */
/*         g_object_set (self->metered_row, "active", TRUE, NULL); */
/*     } */
/*   else */
/*     gtk_widget_hide (self->metered_row); */

  GTK_WIDGET_CLASS (cc_connection_editor_parent_class)->show (widget);
}

static void
cc_connection_editor_finalize (GObject *object)
{
  CcConnectionEditor *self = (CcConnectionEditor *)object;

  g_clear_object (&self->device);
  g_clear_object (&self->connection);
  g_clear_object (&self->orig_connection);
  g_object_unref (self->nm_client);

  G_OBJECT_CLASS (cc_connection_editor_parent_class)->finalize (object);
}

static void
cc_connection_editor_class_init (CcConnectionEditorClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = cc_connection_editor_finalize;

  widget_class->show = cc_connection_editor_show;

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/control-center/"
                                               "network/cc-connection-editor.ui");

  gtk_widget_class_bind_template_child (widget_class, CcConnectionEditor, back_button);
  gtk_widget_class_bind_template_child (widget_class, CcConnectionEditor, cancel_button);
  gtk_widget_class_bind_template_child (widget_class, CcConnectionEditor, apply_button);

  gtk_widget_class_bind_template_child (widget_class, CcConnectionEditor, main_view);
  gtk_widget_class_bind_template_child (widget_class, CcConnectionEditor, main_stack);
  gtk_widget_class_bind_template_child (widget_class, CcConnectionEditor, main_page);

  gtk_widget_class_bind_template_child (widget_class, CcConnectionEditor, details_box);
  gtk_widget_class_bind_template_child (widget_class, CcConnectionEditor, signal_row);
  gtk_widget_class_bind_template_child (widget_class, CcConnectionEditor, frequency_row);
  gtk_widget_class_bind_template_child (widget_class, CcConnectionEditor, route_row);
  gtk_widget_class_bind_template_child (widget_class, CcConnectionEditor, last_used_row);
  gtk_widget_class_bind_template_child (widget_class, CcConnectionEditor, details_row);

  gtk_widget_class_bind_template_child (widget_class, CcConnectionEditor, switch_box);
  gtk_widget_class_bind_template_child (widget_class, CcConnectionEditor, auto_connect_row);
  gtk_widget_class_bind_template_child (widget_class, CcConnectionEditor, allow_others_row);
  gtk_widget_class_bind_template_child (widget_class, CcConnectionEditor, metered_row);

  gtk_widget_class_bind_template_child (widget_class, CcConnectionEditor, advanced_box);
  gtk_widget_class_bind_template_child (widget_class, CcConnectionEditor, ip4_row);
  gtk_widget_class_bind_template_child (widget_class, CcConnectionEditor, ip6_row);
  gtk_widget_class_bind_template_child (widget_class, CcConnectionEditor, security_row);

  gtk_widget_class_bind_template_child (widget_class, CcConnectionEditor, details_page);
  gtk_widget_class_bind_template_child (widget_class, CcConnectionEditor, ip4_page);
  gtk_widget_class_bind_template_child (widget_class, CcConnectionEditor, ip6_page);
  gtk_widget_class_bind_template_child (widget_class, CcConnectionEditor, security_page);

  gtk_widget_class_bind_template_callback (widget_class, editor_visible_child_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, editor_cancel_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, editor_apply_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, editor_row_activated_cb);
  gtk_widget_class_bind_template_callback (widget_class, editor_row_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, ce_main_view_scroll_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, editor_settings_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, editor_delete_clicked_cb);

  g_type_ensure (CC_TYPE_LIST_ROW);
  g_type_ensure (CE_TYPE_IP_PAGE);
  g_type_ensure (CC_TYPE_NETWORK_PANEL);
}

static void
cc_connection_editor_init (CcConnectionEditor *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
  editor_visible_child_changed_cb (self);
  ce_ip_page_set_version (self->ip4_page, AF_INET);
  ce_ip_page_set_version (self->ip6_page, AF_INET6);

  gtk_list_box_set_header_func (self->details_box,
                                cc_list_box_update_header_func,
                                NULL, NULL);

  gtk_list_box_set_header_func (self->switch_box,
                                cc_list_box_update_header_func,
                                NULL, NULL);

 gtk_list_box_set_header_func (self->advanced_box,
                                cc_list_box_update_header_func,
                               NULL, NULL);

 self->vadjustment = gtk_scrolled_window_get_vadjustment (self->main_view);
 g_signal_connect_swapped (self->vadjustment, "notify::upper",
                           G_CALLBACK (ce_main_view_scroll_changed_cb), self);
}

GtkWidget *
cc_connection_editor_new (GtkWindow *parent_window,
                          NMClient  *nm_client)
{
  CcConnectionEditor *self;

  g_return_val_if_fail (GTK_IS_WINDOW (parent_window), NULL);
  g_return_val_if_fail (NM_IS_CLIENT (nm_client), NULL);

  self = g_object_new (CC_TYPE_CONNECTION_EDITOR,
                       "transient-for", parent_window,
                       "use-header-bar", TRUE,
                       NULL);
  ce_ip_page_set_parent_window (self->ip4_page, GTK_WINDOW (self));
  ce_ip_page_set_parent_window (self->ip6_page, GTK_WINDOW (self));
  ce_details_page_set_parent_window (self->details_page, GTK_WINDOW (self));

  self->nm_client = g_object_ref (nm_client);
  ce_details_page_set_nm_client (self->details_page, nm_client);
  ce_security_page_set_nm_client (self->security_page, nm_client);

  return GTK_WIDGET (self);
}

void
cc_connection_editor_set_connection (CcConnectionEditor *self,
                                     NMConnection       *connection,
                                     NMDevice           *device)
{
  NMSettingConnection *con_setting;
  const gchar *type;
  gchar *str;
  gboolean others_allowed, is_active;

  g_return_if_fail (CC_IS_CONNECTION_EDITOR (self));
  g_return_if_fail (NM_IS_CONNECTION (connection));
  g_return_if_fail (NM_IS_DEVICE (device));

  g_set_object (&self->orig_connection, connection);
  g_set_object (&self->device, device);

  g_clear_object (&self->connection);
  self->connection = nm_simple_connection_new_clone (self->orig_connection);
  con_setting = nm_connection_get_setting_connection (self->connection);

  g_object_bind_property (con_setting, "autoconnect",
                          self->auto_connect_row, "active",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);

  /* If no users are added, all users are allowed */
  others_allowed = nm_setting_connection_get_num_permissions (con_setting) == 0;
  g_object_set (self->allow_others_row, "active", others_allowed, NULL);

  /* Disable for VPN; NetworkManager does not implement that yet (see
   * bug https://bugzilla.gnome.org/show_bug.cgi?id=792618) */
  type = nm_setting_connection_get_connection_type (con_setting);
  if (type && !g_str_equal (type, NM_SETTING_VPN_SETTING_NAME))
    {
      NMMetered metered;

      metered = nm_setting_connection_get_metered (con_setting);

      if (metered == NM_METERED_YES || metered == NM_METERED_GUESS_YES)
        g_object_set (self->metered_row, "active", TRUE, NULL);
      else
        g_object_set (self->metered_row, "active", FALSE, NULL);
    }
  else
    gtk_widget_hide (GTK_WIDGET (self->metered_row));

  ce_security_page_set_connection (self->security_page, self->orig_connection, self->connection, device);
  ce_details_page_set_connection (self->details_page, self->orig_connection, self->connection, device);
  ce_ip_page_set_connection (self->ip4_page, self->connection, device);
  ce_ip_page_set_connection (self->ip6_page, self->connection, device);

  is_active = ce_deails_page_has_active_connection (self->details_page, TRUE);
  gtk_widget_set_visible (GTK_WIDGET (self->route_row), is_active);
  gtk_widget_set_visible (GTK_WIDGET (self->last_used_row), !is_active);

  if (is_active)
    {
      const gchar *gateway;

      gateway = ce_details_page_get_gateway (self->details_page);
      cc_list_row_set_secondary_label (self->route_row, gateway);
    }
  else
    {
      str = ce_details_page_get_last_used (self->details_page);
      cc_list_row_set_secondary_label (self->last_used_row, str);
      g_free (str);
    }
}

void
cc_connection_editor_set_ap (CcConnectionEditor *self,
                             NMAccessPoint      *ap)
{
  g_return_if_fail (CC_IS_CONNECTION_EDITOR (self));
  g_return_if_fail (!ap || NM_IS_ACCESS_POINT (ap));

  if (g_set_object (&self->ap, ap))
    cc_connection_editor_update_ap (self);
}
