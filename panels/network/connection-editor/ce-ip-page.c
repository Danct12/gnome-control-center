/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ce-wifi-page.c
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
#define G_LOG_DOMAIN "ce-ip-page"

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <errno.h>
#include <arpa/inet.h>
#include <glib/gi18n.h>
#include <NetworkManager.h>

#define HANDY_USE_UNSTABLE_API
#include <handy.h>

#include "ui-helpers.h"
#include "list-box-helper.h"
#include "cc-list-row.h"
#include "ce-ip-page.h"

/**
 * @short_description: The Ip page for a connection
 * @include: "ce-ip-page.h"
 *
 * Show ip related to a network connection
 */

#define MANUAL_IP_NOT_CHANGED 0
#define MANUAL_IP_ADDED    1        /* New manual IP added */
#define MANUAL_IP_LOADED   2        /* Manual IP loaded from NMConnection */
#define MANUAL_IP_MODIFIED 3
#define MANUAL_IP_DELETED  4

#define ROUTE_NOT_CHANGED 0
#define ROUTE_ADDED    1        /* New route added */
#define ROUTE_LOADED   2        /* Route loaded from NMConnection */
#define ROUTE_MODIFIED 3
#define ROUTE_DELETED  4

struct _CeIpPage
{
  GtkBox         parent_instance;

  GtkWidget     *auto_list_box;
  GtkWidget     *ip_method_row;
  GtkWidget     *auto_dns_row;
  GtkWidget     *auto_route_row;

  GtkWidget     *dns_list_box;
  GtkWidget     *dns_title_label;
  GtkWidget     *add_dns_row;

  GtkWidget     *manual_list_box;
  GtkWidget     *manual_address_entry;
  GtkWidget     *manual_netmask_entry;
  GtkWidget     *manual_gateway_entry;

  GtkWidget     *add_dns_dialog;
  GtkWidget     *add_button;
  GtkWidget     *dns_entry;

  GtkWidget     *route_list_box;
  GtkWidget     *route_title_label;
  GtkWidget     *route_address_entry;
  GtkWidget     *route_netmask_entry;
  GtkWidget     *route_gateway_entry;
  GtkWidget     *route_metric_entry;

  NMClient      *nm_client;
  NMDevice      *device;
  NMConnection  *connection;
  NMSetting     *ip_setting;

  GListStore    *ip_methods;
  gchar         *current_method;

  gint           version;
  gint           route_status;
  gint           manual_status;
  gboolean       ip_has_error;  /* Manual IP */
  gboolean       route_has_error;
};

G_DEFINE_TYPE (CeIpPage, ce_ip_page, GTK_TYPE_BOX)

enum {
  CHANGED,
  N_SIGNALS
};

static guint signals[N_SIGNALS];

static gboolean
parse_netmask (const char *str,
               guint      *prefix,
               gint        version)
{
  struct in_addr tmp_addr;
  glong tmp_prefix;
  gint max_limit;

  if (version == AF_INET)
    max_limit = 32;
  else
    max_limit = 128;

  errno = 0;

  /* Is it a prefix? */
  if (!strchr (str, '.')) {
    tmp_prefix = strtol (str, NULL, 10);
    if (!errno && tmp_prefix >= 0 && tmp_prefix <= max_limit) {
      if (prefix)
        *prefix = tmp_prefix;
      return TRUE;
    }
  }

  if (version != AF_INET)
    return FALSE;

  /* Is it a netmask? */
  if (inet_pton (AF_INET, str, &tmp_addr) > 0) {
    if (prefix)
      *prefix = nm_utils_ip4_netmask_to_prefix (tmp_addr.s_addr);
    return TRUE;
  }

  return FALSE;
}

static void
ce_ip_page_populate_methods (CeIpPage *self)
{
  HdyValueObject *object;

  g_assert (CE_IS_IP_PAGE (self));

  g_list_store_remove_all (self->ip_methods);

  /* XXX: Do we need different titles here? */
  if (self->version == AF_INET6)
    object = hdy_value_object_new_string (_("Automatic"));
  else
    object = hdy_value_object_new_string (_("Automatic (DHCP)"));

  g_object_set_data (G_OBJECT (object), "value", "auto");
  g_list_store_append (self->ip_methods, object);
  g_object_unref (object);

  if (self->version == AF_INET6)
    {
      object = hdy_value_object_new_string (_("Automatic, DHCP only"));
      g_object_set_data (G_OBJECT (object), "value", "dhcp");
      g_list_store_append (self->ip_methods, object);
      g_object_unref (object);
    }

  object = hdy_value_object_new_string (_("Link-Local Only"));
  g_object_set_data (G_OBJECT (object), "value", "link-local");
  g_list_store_append (self->ip_methods, object);
  g_object_unref (object);

  object = hdy_value_object_new_string (_("Manual"));
  g_object_set_data (G_OBJECT (object), "value", "manual");
  g_list_store_append (self->ip_methods, object);
  g_object_unref (object);

  object = hdy_value_object_new_string (_("Disable"));
  g_object_set_data (G_OBJECT (object), "value", "disabled");
  g_list_store_append (self->ip_methods, object);
  g_object_unref (object);
}

static void
ce_ip_page_remove_dns_row (GtkWidget *widget,
                           CeIpPage  *self)
{
  g_assert (CE_IS_IP_PAGE (self));
  g_assert (GTK_IS_WIDGET (widget));

  if (widget != self->add_dns_row)
    gtk_container_remove (GTK_CONTAINER (self->dns_list_box), widget);
}

static void
dns_delete_clicked_cb (CeIpPage  *self,
                       GtkButton *button)
{
  GtkWidget *row, *label;
  const gchar *dns;

  g_assert (CE_IS_IP_PAGE (self));
  g_assert (GTK_IS_BUTTON (button));

  row = g_object_get_data (G_OBJECT (button), "row");
  label = g_object_get_data (G_OBJECT (button), "label");
  dns = gtk_label_get_label (GTK_LABEL (label));

  if (nm_setting_ip_config_remove_dns_by_value (NM_SETTING_IP_CONFIG (self->ip_setting), dns))
    gtk_container_remove (GTK_CONTAINER (self->dns_list_box), row);
  else /* Debug */
    g_return_if_reached ();

  g_signal_emit (self, signals[CHANGED], 0);
}

static GtkWidget *
ce_ip_list_row_new (CeIpPage    *self,
                    const gchar *dns)
{
  GtkWidget *row, *box, *label, *button;
  GtkStyleContext *style_context;

  g_assert (CE_IS_IP_PAGE (self));
  g_assert (nm_utils_ipaddr_valid (self->version, dns));

  row = gtk_list_box_row_new ();
  gtk_widget_set_can_focus (row, FALSE);

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
  g_object_set (box, "margin", 6, NULL);
  gtk_widget_set_hexpand (box, TRUE);
  gtk_container_add (GTK_CONTAINER (row), box);

  label = gtk_label_new (dns);
  gtk_widget_set_hexpand (label, TRUE);
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  style_context = gtk_widget_get_style_context (label);
  gtk_style_context_add_class (style_context, "dim-label");
  gtk_container_add (GTK_CONTAINER (box), label);

  button = gtk_button_new_from_icon_name ("edit-delete-symbolic", GTK_ICON_SIZE_BUTTON);
  g_object_set_data (G_OBJECT (button), "row", row);
  g_object_set_data (G_OBJECT (button), "label", label);
  g_signal_connect_object (button, "clicked",
                           G_CALLBACK (dns_delete_clicked_cb),
                           self,
                           G_CONNECT_SWAPPED);
  gtk_container_add (GTK_CONTAINER (box), button);

  gtk_widget_show_all (row);

  return row;
}

static void
ce_ip_page_add_dns_row (CeIpPage    *self,
                        const gchar *dns)
{
  GtkWidget *row;
  gint last_index;

  g_assert (CE_IS_IP_PAGE (self));
  g_assert (nm_utils_ipaddr_valid (self->version, dns));

  row = ce_ip_list_row_new (self, dns);
  last_index = gtk_list_box_row_get_index (GTK_LIST_BOX_ROW (self->add_dns_row));
  gtk_list_box_insert (GTK_LIST_BOX (self->dns_list_box), row, last_index);
}

static void
ce_ip_page_update_device_ip (CeIpPage *self)
{
  g_assert (CE_IS_IP_PAGE (self));
  g_assert (NM_IS_DEVICE (self->device));
  g_assert (NM_IS_CONNECTION (self->connection));
}

static void
ip_method_changed_cb (CeIpPage *self)
{
  g_autoptr(HdyValueObject) object = NULL;
  gchar *method;
  gint index;

  g_assert (CE_IS_IP_PAGE (self));


  index = hdy_combo_row_get_selected_index (HDY_COMBO_ROW (self->ip_method_row));
  if (index == -1)
    return;

  object = g_list_model_get_item (G_LIST_MODEL (self->ip_methods), index);
  g_assert (G_IS_OBJECT (object));
  method = g_object_get_data (G_OBJECT (object), "value");

  if (g_strcmp0 (self->current_method, method) == 0)
    return;

  self->current_method = method;
  gtk_widget_set_sensitive (self->auto_dns_row, TRUE);
  gtk_widget_set_sensitive (self->dns_list_box, TRUE);
  gtk_widget_hide (self->manual_list_box);

  if (g_str_equal (self->current_method, "manual"))
    {
      gtk_widget_show (self->manual_list_box);
    }
  else if (g_str_equal (self->current_method, "disabled"))
    {
      gtk_widget_set_sensitive (self->auto_dns_row, FALSE);
      gtk_widget_set_sensitive (self->dns_list_box, FALSE);
    }
  else if (g_str_equal (self->current_method, "link-local"))
    {
      gtk_widget_set_sensitive (self->dns_list_box, FALSE);
    }

  g_signal_emit (self, signals[CHANGED], 0);
}

static void
ip_auto_row_changed_cb (CeIpPage   *self,
                        GParamSpec *pspec,
                        GtkWidget  *row)
{
  const char *label;
  gboolean is_auto;

  g_assert (CE_IS_IP_PAGE (self));
  g_assert (GTK_IS_LIST_BOX_ROW (row));

  g_object_get (row, "active", &is_auto, NULL);

  if (row == self->auto_dns_row)
    {
      if (is_auto)
        label = _("Additional DNS Servers");
      else
        label = _("DNS Servers");

      gtk_label_set_text (GTK_LABEL (self->dns_title_label), label);
    }
  else if (row == self->auto_route_row)
    {
      if (is_auto)
        label = _("Additional Route");
      else
        label = _("Route");

      gtk_label_set_text (GTK_LABEL (self->route_title_label), label);
    }

  g_signal_emit (self, signals[CHANGED], 0);
}

static void
dns_row_activated_cb (CeIpPage  *self,
                      GtkWidget *row)
{
  int result;

  g_assert (CE_IS_IP_PAGE (self));
  g_assert (GTK_IS_LIST_BOX_ROW (row));
  g_assert (self->ip_setting);

  if (row != self->add_dns_row)
    return;

  gtk_widget_grab_focus (self->dns_entry);
  gtk_entry_set_text (GTK_ENTRY (self->dns_entry), "");
  result = gtk_dialog_run (GTK_DIALOG (self->add_dns_dialog));
  gtk_widget_hide (self->add_dns_dialog);

  if (result == GTK_RESPONSE_APPLY)
    {
      const gchar *dns;

      dns = gtk_entry_get_text (GTK_ENTRY (self->dns_entry));
      if (nm_setting_ip_config_add_dns (NM_SETTING_IP_CONFIG (self->ip_setting), dns))
        {
          ce_ip_page_add_dns_row (self, dns);
          g_signal_emit (self, signals[CHANGED], 0);
        }
    }
}

static void
ce_entry_set_error (GtkWidget *widget,
                    gboolean   has_error)
{
  if (has_error)
    widget_set_error (widget);
  else
    widget_unset_error (widget);
}

static gboolean
ce_ip_entry_validate (GtkWidget *widget,
                      gint       version)
{
  GtkEntry *entry;
  const gchar *ip;
  gboolean valid;

  g_assert (GTK_IS_ENTRY (widget));
  g_assert (version == AF_INET || version == AF_INET6);

  entry = GTK_ENTRY (widget);
  ip = gtk_entry_get_text (entry);

  valid = nm_utils_ipaddr_valid (version, ip);
  ce_entry_set_error (widget, !valid);

  return valid;
}

static gboolean
ce_netmask_entry_validate (GtkWidget *widget,
                           gint       version)
{
  GtkEntry *entry;
  const gchar *text;
  gchar *end;
  glong prefix;
  gboolean valid;

  g_assert (GTK_IS_ENTRY (widget));
  g_assert (version == AF_INET || version == AF_INET6);

  valid = FALSE;
  entry = GTK_ENTRY (widget);
  text = gtk_entry_get_text (entry);

  if (!*text)
    {
      widget_set_error (widget);
      return FALSE;
    }

  if (version == AF_INET)
    valid = ce_ip_entry_validate (widget, version);

  if (gtk_entry_get_text_length (entry) > 4)
    return valid;

  prefix = strtol (text, &end, 10);

  if (*end == '\0')
    {
      if (version == AF_INET)
        valid = prefix >= 0 && prefix <= 32;
      else
        valid = prefix >= 0 && prefix <= 128;
    }

  ce_entry_set_error (widget, !valid);

  return valid;
}

static gboolean
ce_metric_entry_validate (GtkWidget *widget)
{
  GtkEntry *entry;
  const gchar *text;
  gchar *end;
  glong metric;
  gboolean valid;

  g_assert (GTK_IS_ENTRY (widget));

  valid = FALSE;
  entry = GTK_ENTRY (widget);
  text = gtk_entry_get_text (entry);

  if (!*text)
    {
      widget_unset_error (widget);
      return TRUE;
    }

  metric = strtol (text, &end, 10);

  if (*end == '\0' &&
      metric >= 0 && metric <= G_MAXUINT32)
    valid = TRUE;

  ce_entry_set_error (widget, !valid);

  return valid;
}

static void
ce_manual_entry_changed_cb (CeIpPage *self)
{
  const gchar *address, *gateway, *netmask;
  gboolean valid, has_error;

  g_assert (CE_IS_IP_PAGE (self));

  if (!gtk_widget_get_visible (self->manual_list_box))
    return;

  address = gtk_entry_get_text (GTK_ENTRY (self->manual_address_entry));
  gateway = gtk_entry_get_text (GTK_ENTRY (self->manual_gateway_entry));
  netmask = gtk_entry_get_text (GTK_ENTRY (self->manual_netmask_entry));

  if (!*address && !*gateway && !*netmask)
    {
      widget_set_error (self->manual_address_entry);
      widget_set_error (self->manual_gateway_entry);
      widget_set_error (self->manual_netmask_entry);

      if (self->manual_status == MANUAL_IP_LOADED ||
          self->manual_status == MANUAL_IP_MODIFIED)
        self->manual_status = MANUAL_IP_DELETED;
      else
        self->manual_status = MANUAL_IP_NOT_CHANGED;

      self->ip_has_error = TRUE;
      goto end;
    }

  valid = ce_ip_entry_validate (self->manual_address_entry, self->version);
  has_error = !valid;

  valid = ce_ip_entry_validate (self->manual_gateway_entry, self->version);
  has_error = has_error || !valid;

  valid = ce_netmask_entry_validate (self->manual_netmask_entry, self->version);
  has_error = has_error || !valid;

  self->ip_has_error = has_error;
  if (has_error)
    goto end;

  if (self->manual_status == MANUAL_IP_LOADED ||
      self->manual_status == MANUAL_IP_DELETED)
    self->manual_status = MANUAL_IP_MODIFIED;
  else if (self->manual_status == MANUAL_IP_NOT_CHANGED)
    self->manual_status = MANUAL_IP_ADDED;

 end:
  g_signal_emit (self, signals[CHANGED], 0);
}

static void
ce_route_entry_changed_cb (CeIpPage *self)
{
  const gchar *address, *gateway, *netmask, *metric;
  gboolean valid, has_error;

  g_assert (CE_IS_IP_PAGE (self));

  address = gtk_entry_get_text (GTK_ENTRY (self->route_address_entry));
  gateway = gtk_entry_get_text (GTK_ENTRY (self->route_gateway_entry));
  netmask = gtk_entry_get_text (GTK_ENTRY (self->route_netmask_entry));
  metric  = gtk_entry_get_text (GTK_ENTRY (self->route_metric_entry));

  if (!*address && !*gateway && !*netmask && !*metric)
    {
      widget_unset_error (self->route_address_entry);
      widget_unset_error (self->route_gateway_entry);
      widget_unset_error (self->route_netmask_entry);
      widget_unset_error (self->route_metric_entry);

      if (self->route_status == ROUTE_LOADED ||
          self->route_status == ROUTE_MODIFIED)
        self->route_status = ROUTE_DELETED;
      else
        self->route_status = ROUTE_NOT_CHANGED;

      self->route_has_error = FALSE;
      goto end;
    }

  valid = ce_ip_entry_validate (self->route_address_entry, self->version);
  has_error = !valid;

  valid = ce_ip_entry_validate (self->route_gateway_entry, self->version);
  has_error = has_error || !valid;

  valid = ce_netmask_entry_validate (self->route_netmask_entry, self->version);
  has_error = has_error || !valid;

  valid = ce_metric_entry_validate (self->route_metric_entry);
  has_error = has_error || !valid;

  self->route_has_error = has_error;
  if (has_error)
    goto end;

  if (self->route_status == ROUTE_LOADED ||
      self->route_status == ROUTE_DELETED)
    self->route_status = ROUTE_MODIFIED;
  else if (self->route_status == ROUTE_NOT_CHANGED)
    self->route_status = ROUTE_ADDED;

 end:
  g_signal_emit (self, signals[CHANGED], 0);
}

static void
ce_dns_entry_changed_cb (CeIpPage *self)
{
  GtkStyleContext *style_context;
  const gchar *ip;
  gboolean valid;

  g_assert (CE_IS_IP_PAGE (self));

  ip = gtk_entry_get_text (GTK_ENTRY (self->dns_entry));
  style_context = gtk_widget_get_style_context (self->dns_entry);

  valid = nm_utils_ipaddr_valid (self->version, ip);
  gtk_widget_set_sensitive (self->add_button, valid);

  if (valid || *ip == '\0')
    gtk_style_context_remove_class (style_context, "error");
  else
    gtk_style_context_add_class (style_context, "error");

}

static void
ce_ip_page_finalize (GObject *object)
{
  CeIpPage *self = (CeIpPage *)object;

  g_clear_object (&self->ip_methods);
  g_clear_object (&self->device);
  g_clear_object (&self->connection);
  g_clear_object (&self->nm_client);

  G_OBJECT_CLASS (ce_ip_page_parent_class)->finalize (object);
}

static void
ce_ip_page_class_init (CeIpPageClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = ce_ip_page_finalize;

  signals[CHANGED] =
    g_signal_new ("changed",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_FIRST,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/control-center/"
                                               "network/ce-ip-page.ui");

  gtk_widget_class_bind_template_child (widget_class, CeIpPage, auto_list_box);
  gtk_widget_class_bind_template_child (widget_class, CeIpPage, ip_method_row);
  gtk_widget_class_bind_template_child (widget_class, CeIpPage, auto_dns_row);
  gtk_widget_class_bind_template_child (widget_class, CeIpPage, auto_route_row);

  gtk_widget_class_bind_template_child (widget_class, CeIpPage, manual_list_box);
  gtk_widget_class_bind_template_child (widget_class, CeIpPage, manual_address_entry);
  gtk_widget_class_bind_template_child (widget_class, CeIpPage, manual_netmask_entry);
  gtk_widget_class_bind_template_child (widget_class, CeIpPage, manual_gateway_entry);

  gtk_widget_class_bind_template_child (widget_class, CeIpPage, dns_list_box);
  gtk_widget_class_bind_template_child (widget_class, CeIpPage, dns_title_label);
  gtk_widget_class_bind_template_child (widget_class, CeIpPage, add_dns_row);

  gtk_widget_class_bind_template_child (widget_class, CeIpPage, add_dns_dialog);
  gtk_widget_class_bind_template_child (widget_class, CeIpPage, add_button);
  gtk_widget_class_bind_template_child (widget_class, CeIpPage, dns_entry);

  gtk_widget_class_bind_template_child (widget_class, CeIpPage, route_list_box);
  gtk_widget_class_bind_template_child (widget_class, CeIpPage, route_title_label);
  gtk_widget_class_bind_template_child (widget_class, CeIpPage, route_address_entry);
  gtk_widget_class_bind_template_child (widget_class, CeIpPage, route_netmask_entry);
  gtk_widget_class_bind_template_child (widget_class, CeIpPage, route_gateway_entry);
  gtk_widget_class_bind_template_child (widget_class, CeIpPage, route_metric_entry);

  gtk_widget_class_bind_template_callback (widget_class, ip_method_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, ip_auto_row_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, dns_row_activated_cb);
  gtk_widget_class_bind_template_callback (widget_class, ce_manual_entry_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, ce_route_entry_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, ce_dns_entry_changed_cb);
}

static void
ce_ip_page_init (CeIpPage *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
  self->ip_methods = g_list_store_new (HDY_TYPE_VALUE_OBJECT);

  hdy_combo_row_bind_name_model (HDY_COMBO_ROW (self->ip_method_row),
                                 G_LIST_MODEL (self->ip_methods),
                                 (HdyComboRowGetNameFunc) hdy_value_object_dup_string,
                                 NULL, NULL);

  gtk_list_box_set_header_func (GTK_LIST_BOX (self->auto_list_box),
                                cc_list_box_update_header_func,
                                NULL, NULL);

  gtk_list_box_set_header_func (GTK_LIST_BOX (self->dns_list_box),
                                cc_list_box_update_header_func,
                                NULL, NULL);

  gtk_list_box_set_header_func (GTK_LIST_BOX (self->route_list_box),
                                cc_list_box_update_header_func,
                                NULL, NULL);
}

void
ce_ip_page_set_parent_window (CeIpPage  *self,
                              GtkWindow *parent_window)
{
  g_return_if_fail (CE_IS_IP_PAGE (self));
  g_return_if_fail (GTK_IS_WINDOW (parent_window));

  gtk_window_set_transient_for (GTK_WINDOW (self->add_dns_dialog),
                                parent_window);
}

void
ce_ip_page_set_version (CeIpPage *self,
                        gint      version)
{
  g_return_if_fail (CE_IS_IP_PAGE (self));
  g_return_if_fail (version != AF_INET || version != AF_INET6);

  self->version = version;

  ce_ip_page_populate_methods (self);

  if (version == AF_INET6)
    g_object_set (self->ip_method_row, "title", _("IPv6 Method"), NULL);
  else
    g_object_set (self->ip_method_row, "title", _("IPv4 Method"), NULL);
}

void
ce_ip_page_set_connection (CeIpPage     *self,
                           NMConnection *connection,
                           NMDevice     *device)
{
  g_return_if_fail (CE_IS_IP_PAGE (self));
  g_return_if_fail (self->version != 0);
  g_return_if_fail (NM_IS_CONNECTION (connection));
  g_return_if_fail (NM_IS_DEVICE (device));

  g_set_object (&self->connection, connection);
  g_set_object (&self->device, device);

  if (self->version == AF_INET)
    self->ip_setting = NM_SETTING (nm_connection_get_setting_ip4_config (connection));
  if (self->version == AF_INET6)
    self->ip_setting = NM_SETTING (nm_connection_get_setting_ip6_config (connection));

  if (!self->ip_setting)
    {
      if (self->version == AF_INET)
        self->ip_setting = nm_setting_ip4_config_new ();
      else if (self->version == AF_INET6)
        self->ip_setting = nm_setting_ip6_config_new ();
      else /* Debug */
        g_return_if_reached ();
      nm_connection_add_setting (connection, self->ip_setting);

      g_signal_emit (self, signals[CHANGED], 0);
    }

  g_object_bind_property (self->ip_setting, "ignore-auto-dns",
                          self->auto_dns_row, "active",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL | G_BINDING_INVERT_BOOLEAN);

  g_object_bind_property (self->ip_setting, "ignore-auto-routes",
                          self->auto_route_row, "active",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL | G_BINDING_INVERT_BOOLEAN);
}

void
ce_ip_page_refresh (CeIpPage *self)
{
  NMSettingIPConfig *setting;

  g_return_if_fail (CE_IS_IP_PAGE (self));
  g_return_if_fail (self->version != 0);

  if (self->device && self->connection)
    ce_ip_page_update_device_ip (self);

  ip_auto_row_changed_cb (self, NULL, self->auto_dns_row);
  ip_auto_row_changed_cb (self, NULL, self->auto_route_row);

  setting = NM_SETTING_IP_CONFIG (self->ip_setting);
  gtk_container_foreach (GTK_CONTAINER (self->dns_list_box),
                         (GtkCallback)ce_ip_page_remove_dns_row, self);

  for (int i = 0; i < nm_setting_ip_config_get_num_dns (setting); i++)
    {
      const char *dns;

      dns = nm_setting_ip_config_get_dns (setting, i);
      ce_ip_page_add_dns_row (self, dns);
    }

  /* Reset entries */
  gtk_entry_set_text (GTK_ENTRY (self->route_address_entry), "");
  gtk_entry_set_text (GTK_ENTRY (self->route_netmask_entry), "");
  gtk_entry_set_text (GTK_ENTRY (self->route_gateway_entry), "");
  gtk_entry_set_text (GTK_ENTRY (self->route_metric_entry), "");

  gtk_entry_set_text (GTK_ENTRY (self->manual_address_entry), "");
  gtk_entry_set_text (GTK_ENTRY (self->manual_netmask_entry), "");
  gtk_entry_set_text (GTK_ENTRY (self->manual_gateway_entry), "");

  /* We set only one route, the rest is ignored */
  if (nm_setting_ip_config_get_num_routes (setting) > 0)
    {
      NMIPRoute *route;
      gchar *str;
      gint64 metric;
      guint32 prefix;

      route = nm_setting_ip_config_get_route (setting, 0);
      gtk_entry_set_text (GTK_ENTRY (self->route_address_entry),
                          nm_ip_route_get_dest (route));
      gtk_entry_set_text (GTK_ENTRY (self->route_gateway_entry),
                          nm_ip_route_get_next_hop (route));
      self->route_status = ROUTE_LOADED;

      prefix = nm_ip_route_get_prefix (route);
      if (self->version == AF_INET)
        {
          gchar text_netmask[INET_ADDRSTRLEN + 1];
          gulong netmask;

          netmask = nm_utils_ip4_prefix_to_netmask (prefix);
          inet_ntop (AF_INET, &netmask, text_netmask, sizeof (text_netmask));
          gtk_entry_set_text (GTK_ENTRY (self->route_netmask_entry), text_netmask);
        }
      else
        {
          str = g_strdup_printf ("%ud", prefix);
          gtk_entry_set_text (GTK_ENTRY (self->route_netmask_entry), str);
          g_free (str);
        }

      metric = nm_ip_route_get_metric (route);
      if (metric >= 0)
        {
          str = g_strdup_printf ("%ld", metric);
          gtk_entry_set_text (GTK_ENTRY (self->route_metric_entry), str);
          g_free (str);
        }
    }

  /* We set only one address, the rest is ignored */
  if (nm_setting_ip_config_get_num_addresses (setting) > 0)
    {
      NMIPAddress *address;
      gchar *str;
      guint32 prefix;

      address = nm_setting_ip_config_get_address (setting, 0);
      gtk_entry_set_text (GTK_ENTRY (self->manual_address_entry),
                          nm_ip_address_get_address (address));
      gtk_entry_set_text (GTK_ENTRY (self->manual_gateway_entry),
                          nm_setting_ip_config_get_gateway (setting));
      self->manual_status = MANUAL_IP_LOADED;

      prefix = nm_ip_address_get_prefix (address);
      if (self->version == AF_INET)
        {
          gchar text_netmask[INET_ADDRSTRLEN + 1];
          gulong netmask;

          netmask = nm_utils_ip4_prefix_to_netmask (prefix);
          inet_ntop (AF_INET, &netmask, text_netmask, sizeof (text_netmask));
          gtk_entry_set_text (GTK_ENTRY (self->manual_netmask_entry), text_netmask);
        }
      else
        {
          str = g_strdup_printf ("%ud", prefix);
          gtk_entry_set_text (GTK_ENTRY (self->manual_netmask_entry), str);
          g_free (str);
        }
    }
}

static void
ce_ip_page_save_routes (CeIpPage *self)
{
  g_autoptr(GPtrArray) routes = NULL;
  NMSettingIPConfig *setting;
  NMIPRoute *route, *new_route;
  const gchar *address, *text_metric, *gateway;
  gint64 metric;
  guint prefix;

  g_assert (CE_IS_IP_PAGE (self));

  if (self->route_status == ROUTE_NOT_CHANGED)
    return;

  setting = NM_SETTING_IP_CONFIG (self->ip_setting);

  /* If we modified the route, we delete it, and re-add */
  if (self->route_status == ROUTE_DELETED ||
      self->route_status == ROUTE_MODIFIED)
    nm_setting_ip_config_remove_route (setting, 0);

  if (self->route_status == ROUTE_DELETED)
    return;

  if (!parse_netmask (gtk_entry_get_text (GTK_ENTRY (self->route_netmask_entry)),
                      &prefix, self->version))
    return;

  address = gtk_entry_get_text (GTK_ENTRY (self->route_address_entry));
  gateway = gtk_entry_get_text (GTK_ENTRY (self->route_gateway_entry));
  text_metric = gtk_entry_get_text (GTK_ENTRY (self->route_metric_entry));

  if (!text_metric || !*text_metric)
    metric = -1;
  else
    metric = strtol (text_metric, NULL, 10);

  /*
   * We create a copy of all routes, and put the one we added/changed
   * as the first item, so that it'll always be the first item.
   */
  routes = g_ptr_array_new_full (1, (GDestroyNotify)nm_ip_route_unref);
  new_route = nm_ip_route_new (self->version, address, prefix, gateway, metric, NULL);
  g_ptr_array_add (routes, new_route);

  for (guint i = 0; i < nm_setting_ip_config_get_num_routes (setting); i++)
    {
      route = nm_setting_ip_config_get_route (setting, i);
      nm_ip_route_ref (route);
      g_ptr_array_add (routes, route); /* XXX: Safe? */
    }

  g_object_set (self->ip_setting,
                NM_SETTING_IP_CONFIG_ROUTES, routes,
                NULL);
}

static void
ce_ip_page_save_manual_ips (CeIpPage *self)
{
  g_autoptr(GPtrArray) addresses = NULL;
  NMSettingIPConfig *setting;
  NMIPAddress *ip_address;
  const gchar *address, *gateway;
  guint prefix;

  g_assert (CE_IS_IP_PAGE (self));

  if (self->manual_status == ROUTE_NOT_CHANGED)
    return;

  setting = NM_SETTING_IP_CONFIG (self->ip_setting);

  /* If we modified the route, we delete it, and re-add */
  if (self->manual_status == ROUTE_DELETED ||
      self->manual_status == ROUTE_MODIFIED)
    nm_setting_ip_config_remove_address (setting, 0);

  if (self->manual_status == ROUTE_DELETED)
    return;

  if (!parse_netmask (gtk_entry_get_text (GTK_ENTRY (self->manual_netmask_entry)),
                      &prefix, self->version))
    return;

  address = gtk_entry_get_text (GTK_ENTRY (self->manual_address_entry));
  gateway = gtk_entry_get_text (GTK_ENTRY (self->manual_gateway_entry));

  /*
   * We create a copy of all address, and put the one we added/changed
   * as the first item, so that it'll always be the first item.
   */
  addresses = g_ptr_array_new_with_free_func ((GDestroyNotify) nm_ip_address_unref);

  ip_address = nm_ip_address_new (self->version, address, prefix, NULL);
  g_ptr_array_add (addresses, ip_address);

  for (guint i = 0; i < nm_setting_ip_config_get_num_addresses (setting); i++)
    {
      ip_address = nm_setting_ip_config_get_address (setting, i);
      nm_ip_address_ref (ip_address);
      g_ptr_array_add (addresses, ip_address); /* XXX: Safe? */
    }

  g_object_set (setting,
                NM_SETTING_IP_CONFIG_ADDRESSES, addresses,
                NM_SETTING_IP_CONFIG_GATEWAY, gateway,
                NULL);
}

gboolean
ce_ip_page_has_error (CeIpPage *self)
{
  g_return_val_if_fail (CE_IS_IP_PAGE (self), TRUE);

  if (self->route_has_error ||
      (self->ip_has_error && g_str_equal (self->current_method, "manual")))
    return TRUE;

  return FALSE;
}

void
ce_ip_page_save_connection (CeIpPage *self)
{
  g_autoptr(HdyValueObject) object = NULL;
  const gchar *method;
  gint index;

  g_return_if_fail (CE_IS_IP_PAGE (self));
  g_return_if_fail (self->version != 0);

  if (ce_ip_page_has_error (self))
    return;

  index = hdy_combo_row_get_selected_index (HDY_COMBO_ROW (self->ip_method_row));
  object = g_list_model_get_item (G_LIST_MODEL (self->ip_methods), index);
  method = g_object_get_data (G_OBJECT (object), "value");

  if (method)
    g_object_set (self->ip_setting, "method", method, NULL);
  else
    g_warn_if_reached ();

  ce_ip_page_save_routes (self);
  ce_ip_page_save_manual_ips (self);
}
