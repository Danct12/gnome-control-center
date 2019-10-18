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
#define G_LOG_DOMAIN "ce-details-page"

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <glib/gi18n.h>
#include <NetworkManager.h>

#include "list-box-helper.h"
#include "cc-list-row.h"
#include "ce-details-page.h"

/**
 * @short_description: The Details page for a connection
 * @include: "ce-details-page.h"
 *
 * Show details related to a network connection
 */

struct _CeDetailsPage
{
  GtkBox         parent_instance;

  GtkWidget     *list_box;
  GtkWidget     *speed_row;
  GtkWidget     *dns_row;
  GtkWidget     *ipv4_row;
  GtkWidget     *ipv6_row;

  GtkWidget     *network_name_row;
  GtkWidget     *bssid_row;
  GtkWidget     *mac_row;
  GtkWidget     *cloned_mac_row;

  GtkWidget     *edit_dialog;
  GtkWidget     *apply_button;
  GtkWidget     *dialog_label;
  GtkWidget     *dialog_entry;
  GtkWidget     *dialog_combo_box;
  /* The row that activated the dialog */
  GtkWidget     *dialog_row;

  NMClient      *nm_client;
  NMDevice      *device;
  NMConnection  *orig_connection;
  NMConnection  *connection;
  NMAccessPoint *ap;
  NMActiveConnection *active_connection;
  /* NMSettingWireless  *wireless_setting; */

  gboolean       device_is_active;
};

G_DEFINE_TYPE (CeDetailsPage, ce_details_page, GTK_TYPE_BOX)

/* Stolen from ce-page.c
 * Written by Matthias Clasen and Bastien Nocera
 */
static gboolean
ce_details_address_is_valid (const gchar *addr)
{
  guint8 invalid_addr[4][ETH_ALEN] = {
    {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0x44, 0x44, 0x44, 0x44, 0x44, 0x44},
    {0x00, 0x30, 0xb4, 0x00, 0x00, 0x00}, /* prism54 dummy MAC */
  };
  guint8 addr_bin[ETH_ALEN];
  g_autofree char *trimmed_addr = NULL;
  gchar *end;
  guint i;

  /* MAC Address can be empty */
  if (!addr || *addr == '\0')
    return TRUE;

  /* The address text may also have the device name at end, strip it */
  trimmed_addr = g_strdup (addr);
  end = strchr (trimmed_addr, ' ');
  if (end != NULL)
    *end = '\0';

  if (!nm_utils_hwaddr_valid (trimmed_addr, -1))
    return FALSE;

  if (!nm_utils_hwaddr_aton (trimmed_addr, addr_bin, ETH_ALEN))
    return FALSE;

  /* Check for multicast address */
  if ((((guint8 *) addr_bin)[0]) & 0x01)
    return FALSE;

  for (i = 0; i < G_N_ELEMENTS (invalid_addr); i++)
    if (nm_utils_hwaddr_matches (addr_bin, ETH_ALEN, invalid_addr[i], ETH_ALEN))
      return FALSE;

  return TRUE;
}

static gchar *
ce_details_get_human_time (guint64 unix_time)
{
  g_autoptr(GDateTime) now = NULL;
  g_autoptr(GDateTime) utc_time = NULL;
  gchar *last_used;
  GTimeSpan diff;
  gint days;

  if (unix_time == 0)
    return g_strdup (_("Never"));

  now = g_date_time_new_now_utc ();
  utc_time = g_date_time_new_from_unix_utc (unix_time);

  diff = g_date_time_difference (now, utc_time);
  days = diff / G_TIME_SPAN_DAY;

  if (days == 0)
    last_used = g_strdup (_("Today"));
  else if (days == 1)
    last_used = g_strdup (_("Yesterday"));
  else
    last_used = g_strdup_printf (ngettext ("%i day ago", "%i days ago", days), days);

  return last_used;
}

static const gchar *
details_get_ip_string (NMIPConfig *ip_config)
{
  NMIPAddress *address;
  GPtrArray *ip_addresses;

  g_assert (NM_IS_IP_CONFIG (ip_config));

  ip_addresses = nm_ip_config_get_addresses (ip_config);

  if (ip_addresses->len < 1)
    return "";

  address = ip_addresses->pdata[0];
  return nm_ip_address_get_address (address);
}

static void
ce_details_page_update_device_details (CeDetailsPage *self)
{
  NMIPConfig *ip_config;
  const gchar * const *nameservers;
  guint32 speed = 0;
  gboolean is_active;

  g_assert (CE_IS_DETAILS_PAGE (self));
  g_assert (NM_IS_DEVICE (self->device));
  g_assert (NM_IS_CONNECTION (self->connection));

  is_active = ce_deails_page_has_active_connection (self, TRUE);
  gtk_widget_set_visible (self->speed_row, is_active);
  gtk_widget_set_visible (self->dns_row, is_active);
  gtk_widget_set_visible (self->ipv4_row, is_active);
  gtk_widget_set_visible (self->ipv6_row, is_active);

  if (!is_active)
    return;

  /* Link Speed */
  if (NM_IS_DEVICE_WIFI (self->device))
    speed = nm_device_wifi_get_bitrate (NM_DEVICE_WIFI (self->device)) / 1000;
  else if (NM_IS_DEVICE_ETHERNET (self->device))
    speed = nm_device_ethernet_get_speed (NM_DEVICE_ETHERNET (self->device));
  else
    g_warn_if_reached ();

  if (speed > 0)
    {
      g_autofree gchar *speed_label = NULL;

      speed_label = g_strdup_printf (_("%u Mb/s"), speed);
      cc_list_row_set_secondary_label (CC_LIST_ROW (self->speed_row), speed_label);
    }

  /* DNS servers list */
  ip_config = nm_active_connection_get_ip4_config (self->active_connection);

  /* XXX: check IPv6? */
  if (!ip_config)
    return;

  nameservers = nm_ip_config_get_nameservers (ip_config);

  if (nameservers)
    {
      g_autofree gchar *dns_list = NULL;

      dns_list = g_strjoinv (", ", (gchar **)nameservers);
      cc_list_row_set_secondary_label (CC_LIST_ROW (self->dns_row), dns_list);
    }

  /* IPv4 DHCP/Static address list */
  ip_config = nm_active_connection_get_ip4_config (self->active_connection);
  cc_list_row_set_secondary_label (CC_LIST_ROW (self->ipv4_row),
                                   details_get_ip_string (ip_config));

  /* IPv6 DHCP/Static address list */
  ip_config = nm_active_connection_get_ip6_config (self->active_connection);
  cc_list_row_set_secondary_label (CC_LIST_ROW (self->ipv6_row),
                                   details_get_ip_string (ip_config));
}

static gchar *
ce_details_get_ssid (CeDetailsPage *self)
{
  NMSettingWireless *setting;
  GBytes *ssid;
  gchar *ssid_text;

  g_assert (CE_IS_DETAILS_PAGE (self));

  setting = nm_connection_get_setting_wireless (self->orig_connection);
  g_return_val_if_fail (setting, g_strdup (""));

  ssid = nm_setting_wireless_get_ssid (setting);
  if (ssid)
    ssid_text = nm_utils_ssid_to_utf8 (g_bytes_get_data (ssid, NULL),
                                       g_bytes_get_size (ssid));
  else
    ssid_text = g_strdup ("");

  return ssid_text;
}

static void
ce_details_page_update_wireless (CeDetailsPage *self)
{
  NMSettingWireless *setting;
  g_autofree gchar *ssid_text = NULL;

  g_assert (CE_IS_DETAILS_PAGE (self));

  g_object_set (self->network_name_row, "subtitle", "", NULL);

  setting = nm_connection_get_setting_wireless (self->orig_connection);
  if (!setting)
    return;

  g_object_set (self->network_name_row, "subtitle", _("SSID"), NULL);

  ssid_text = ce_details_get_ssid (self);
  cc_list_row_set_secondary_label (CC_LIST_ROW (self->network_name_row), ssid_text);

  cc_list_row_set_secondary_label (CC_LIST_ROW (self->bssid_row),
                                   nm_setting_wireless_get_bssid (setting));
}

static void
ce_details_page_populate_bssid (CeDetailsPage *self)
{
  NMSettingWireless *setting;
  const gchar *bssid;

  g_assert (CE_IS_DETAILS_PAGE (self));

  setting = nm_connection_get_setting_wireless (self->orig_connection);
  g_return_if_fail (setting);

  for (guint32 i = 0; i < nm_setting_wireless_get_num_seen_bssids (setting); i++)
    {
      bssid = nm_setting_wireless_get_seen_bssid (setting, i);
      gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (self->dialog_combo_box), NULL, bssid);
    }
}

static void
ce_details_page_populate_device_mac (CeDetailsPage *self)
{
  const GPtrArray *devices;
  NMDeviceType device_type;

  devices = nm_client_get_devices (self->nm_client);

  if (!devices)
    return;

  if (NM_IS_DEVICE_WIFI (self->device))
    device_type = NM_DEVICE_TYPE_WIFI;
  else /* TODO */
    g_return_if_reached ();

  for (int i = 0; i < devices->len; i++)
    {
      NMDevice *device;
      const char *iface;
      g_autofree gchar *mac_address = NULL;
      g_autofree gchar *item = NULL;

      device = devices->pdata[i];

      if (nm_device_get_device_type (device) != device_type)
        continue;

      g_object_get (G_OBJECT (device), "perm-hw-address", &mac_address, NULL);
      iface = nm_device_get_iface (device);
      item = g_strdup_printf ("%s (%s)", mac_address, iface);

      gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (self->dialog_combo_box), NULL, item);
  }
}

static void
details_dialog_save (CeDetailsPage *self)
{
  NMSettingConnection *connection_setting;
  NMSettingWireless *wireless_setting;
  GtkEntry *entry;
  g_autoptr(GError) error = NULL;
  const gchar *value;

  g_assert (CE_IS_DETAILS_PAGE (self));

  if (self->dialog_row == self->network_name_row)
    entry = GTK_ENTRY (self->dialog_entry);
  else
    entry = GTK_ENTRY (gtk_bin_get_child (GTK_BIN (self->dialog_combo_box)));

  connection_setting = nm_connection_get_setting_connection (self->orig_connection);
  wireless_setting = nm_connection_get_setting_wireless (self->orig_connection);
  value = gtk_entry_get_text (entry);

  if (!value || !*value)
    value = NULL;

  if (self->dialog_row == self->network_name_row)
    {
      g_object_set (G_OBJECT (connection_setting), "id", value, NULL);
      if (wireless_setting)
        {
          g_autoptr(GBytes) ssid = NULL;

          if (value)
            ssid = g_bytes_new_static (value, strlen (value));
          g_object_set (G_OBJECT (wireless_setting), "ssid", ssid, NULL);
        }
    }
  else if (self->dialog_row == self->bssid_row)
    {
      if (wireless_setting)
        g_object_set (G_OBJECT (wireless_setting), "bssid", value, NULL);
    }
  else if (self->dialog_row == self->mac_row)
    {
      g_autofree gchar *addr = NULL;
      gchar *end = NULL;

      /* The address text may also have the device name at end, strip it */
      addr = g_strdup (value);
      if (addr)
        end = strchr (addr, ' ');
      if (end != NULL)
        *end = '\0';

      if (wireless_setting)
        g_object_set (G_OBJECT (wireless_setting), "mac-address", addr, NULL);
    }
  else if (self->dialog_row == self->cloned_mac_row)
    {
      const gchar *id;

      id = gtk_combo_box_get_active_id (GTK_COMBO_BOX (self->dialog_combo_box));

      if (id)
        value = id;

      if (wireless_setting)
        g_object_set (G_OBJECT (wireless_setting), "cloned-mac-address", value, NULL);
    }

  nm_remote_connection_commit_changes (NM_REMOTE_CONNECTION (self->orig_connection),
                                       TRUE, NULL, &error);
  if (error)
    g_warning ("Error saving settings: %s", error->message);
  else
    ce_details_page_update_wireless (self);
}

static void
ce_details_page_populate_spoof_box (CeDetailsPage *self,
                                    const gchar   *mac)
{
  GtkComboBoxText *combo;
  static const char *items[][2] = {
    { "preserve",  N_("Preserve") },
    { "permanent", N_("Permanent") },
    { "random",    N_("Random") },
    { "stable",    N_("Stable") } };
  gchar *match = NULL;

  g_assert (CE_IS_DETAILS_PAGE (self));

  if (mac)
    match = strchr (mac, ':');

  combo = GTK_COMBO_BOX_TEXT (self->dialog_combo_box);
  for (int i = 0; i < G_N_ELEMENTS (items); i++)
    gtk_combo_box_text_append (combo, items[i][0], _(items[i][1]));

  if (!match && mac)
    gtk_combo_box_set_active_id (GTK_COMBO_BOX (combo), mac);
}

static void
details_row_activated_cb (CeDetailsPage *self,
                          GtkWidget     *row)
{
  NMSettingWireless *setting;
  GtkEntry *entry;
  const gchar *label, *value = NULL;
  gint response;
  gboolean is_wifi;

  g_assert (CE_IS_DETAILS_PAGE (self));
  g_assert (GTK_IS_LIST_BOX_ROW (row));

  self->dialog_row = NULL;
  setting = nm_connection_get_setting_wireless (self->orig_connection);
  entry = GTK_ENTRY (gtk_bin_get_child (GTK_BIN (self->dialog_combo_box)));
  is_wifi = NM_IS_DEVICE_WIFI (self->device);
  gtk_widget_hide (self->dialog_entry);

  gtk_entry_set_text (entry, "");
  gtk_entry_set_placeholder_text (entry, "");
  gtk_entry_set_text (GTK_ENTRY (self->dialog_entry), "");
  gtk_widget_set_sensitive (GTK_WIDGET (self->apply_button), FALSE);

  gtk_combo_box_text_remove_all (GTK_COMBO_BOX_TEXT (self->dialog_combo_box));
  if (row == self->network_name_row)
    {
      gtk_widget_show (self->dialog_entry);

      if (is_wifi)
        {
          g_autofree gchar *ssid = NULL;

          ssid = ce_details_get_ssid (self);
          gtk_entry_set_text (GTK_ENTRY (self->dialog_entry), ssid);

          label = _("Choose a new network SSID. This is how it will appear to others.");
        }
      else
        label = _("Choose a new network Name. This is how it will appear to others.");
    }
  else if (row == self->bssid_row)
    {
      label = _("Choose a new network BSSID. This is how it will appear to others.");
      value = nm_setting_wireless_get_bssid (setting);
      ce_details_page_populate_bssid (self);

      if (value && *value)
        gtk_entry_set_text (entry, value);
      else
        gtk_entry_set_placeholder_text (entry, _("Allow connecting to any BSSID"));
    }
  else if (row == self->mac_row)
    {
      label = _("Choose a new network MAC Address. This is how it will appear to others.");
      ce_details_page_populate_device_mac (self);

      if (setting)
        value = nm_setting_wireless_get_mac_address (setting);

      if (value)
        gtk_entry_set_text (entry, value);
    }
  else if (row == self->cloned_mac_row)
    {
      label = _("Choose a new Spoofed MAC Address. This is how it will appear to others.");

      if (setting)
        value = nm_setting_wireless_get_cloned_mac_address (setting);

      ce_details_page_populate_spoof_box (self, value);
    }
  else
    g_return_if_reached ();

  self->dialog_row = row;
  gtk_label_set_label (GTK_LABEL (self->dialog_label), label);

  response = gtk_dialog_run (GTK_DIALOG (self->edit_dialog));
  gtk_widget_hide (self->edit_dialog);

  if (response == GTK_RESPONSE_APPLY)
    details_dialog_save (self);
}

static void
details_value_changed_cb (CeDetailsPage *self,
                          GtkWidget     *widget)
{
  const gchar *value;
  gboolean is_valid;

  g_assert (CE_IS_DETAILS_PAGE (self));
  g_assert (GTK_IS_WIDGET (self));

  if (self->dialog_row == NULL)
    return;

  if (widget == self->dialog_entry)
    {
      if (!gtk_widget_get_visible (widget))
        return;

      value = gtk_entry_get_text (GTK_ENTRY (widget));
      is_valid = value && *value;
    }
  else if (widget == self->dialog_combo_box)
    {
      g_autofree gchar *text = NULL;
      text = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (widget));

      if (self->dialog_row == self->cloned_mac_row)
        {
          is_valid = gtk_combo_box_get_active (GTK_COMBO_BOX (self->dialog_combo_box)) != -1;
          is_valid = is_valid || ce_details_address_is_valid (text);
        }
      else
        is_valid = ce_details_address_is_valid (text);
    }
  else
    g_return_if_reached ();

  gtk_widget_set_sensitive (self->apply_button, is_valid);
}

static void
ce_details_page_finalize (GObject *object)
{
  CeDetailsPage *self = (CeDetailsPage *)object;

  g_clear_object (&self->device);
  g_clear_object (&self->connection);
  g_clear_object (&self->nm_client);

  G_OBJECT_CLASS (ce_details_page_parent_class)->finalize (object);
}

static void
ce_details_page_class_init (CeDetailsPageClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = ce_details_page_finalize;

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/control-center/"
                                               "network/ce-details-page.ui");

  gtk_widget_class_bind_template_child (widget_class, CeDetailsPage, list_box);
  gtk_widget_class_bind_template_child (widget_class, CeDetailsPage, speed_row);
  gtk_widget_class_bind_template_child (widget_class, CeDetailsPage, dns_row);
  gtk_widget_class_bind_template_child (widget_class, CeDetailsPage, ipv4_row);
  gtk_widget_class_bind_template_child (widget_class, CeDetailsPage, ipv6_row);

  gtk_widget_class_bind_template_child (widget_class, CeDetailsPage, network_name_row);
  gtk_widget_class_bind_template_child (widget_class, CeDetailsPage, bssid_row);
  gtk_widget_class_bind_template_child (widget_class, CeDetailsPage, mac_row);
  gtk_widget_class_bind_template_child (widget_class, CeDetailsPage, cloned_mac_row);

  gtk_widget_class_bind_template_child (widget_class, CeDetailsPage, edit_dialog);
  gtk_widget_class_bind_template_child (widget_class, CeDetailsPage, apply_button);
  gtk_widget_class_bind_template_child (widget_class, CeDetailsPage, dialog_label);
  gtk_widget_class_bind_template_child (widget_class, CeDetailsPage, dialog_entry);
  gtk_widget_class_bind_template_child (widget_class, CeDetailsPage, dialog_combo_box);

  gtk_widget_class_bind_template_callback (widget_class, details_row_activated_cb);
  gtk_widget_class_bind_template_callback (widget_class, details_value_changed_cb);
}

static void
ce_details_page_init (CeDetailsPage *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_list_box_set_header_func (GTK_LIST_BOX (self->list_box),
                                cc_list_box_update_header_func,
                                NULL, NULL);
}

void
ce_details_page_set_parent_window (CeDetailsPage *self,
                                   GtkWindow     *parent_window)
{
  g_return_if_fail (CE_IS_DETAILS_PAGE (self));
  g_return_if_fail (GTK_IS_WINDOW (parent_window));

  gtk_window_set_transient_for (GTK_WINDOW (self->edit_dialog),
                                parent_window);
}

void
ce_details_page_set_nm_client (CeDetailsPage *self,
                               NMClient      *nm_client)
{
  g_return_if_fail (CE_IS_DETAILS_PAGE (self));
  g_return_if_fail (NM_IS_CLIENT (nm_client));

  g_set_object (&self->nm_client, nm_client);
}

void
ce_details_page_set_connection (CeDetailsPage *self,
                                NMConnection  *orig_connection,
                                NMConnection  *connection,
                                NMDevice      *device)
{
  g_return_if_fail (CE_IS_DETAILS_PAGE (self));
  g_return_if_fail (NM_IS_REMOTE_CONNECTION (orig_connection));
  g_return_if_fail (NM_IS_CONNECTION (connection));
  g_return_if_fail (NM_IS_DEVICE (device));

  g_set_object (&self->connection, connection);
  g_set_object (&self->orig_connection, orig_connection);
  g_set_object (&self->device, device);
}

void
ce_details_page_set_ap (CeDetailsPage *self,
                        NMAccessPoint *ap)
{
  g_return_if_fail (CE_IS_DETAILS_PAGE (self));
  g_return_if_fail (!ap || NM_IS_ACCESS_POINT (ap));

  g_set_object (&self->ap, ap);
}

const gchar *
ce_details_page_get_ap_strength (CeDetailsPage *self)
{
  const gchar *strength_label;
  guint8 strength;

  g_return_val_if_fail (CE_IS_DETAILS_PAGE (self), NULL);

  if (!self->ap)
    return "";

  strength = nm_access_point_get_strength (self->ap);

  if (strength <= 0)
    strength_label = NULL;
  else if (strength < 20)
    strength_label = C_("Signal strength", "None");
  else if (strength < 40)
    strength_label = C_("Signal strength", "Weak");
  else if (strength < 50)
    strength_label = C_("Signal strength", "Ok");
  else if (strength < 80)
    strength_label = C_("Signal strength", "Good");
  else
    strength_label = C_("Signal strength", "Excellent");

  return strength_label;
}

gchar *
ce_details_page_get_ap_frequency (CeDetailsPage *self)
{
  guint32 frequency;

  g_return_val_if_fail (CE_IS_DETAILS_PAGE (self), NULL);

  if (!self->ap)
    return g_strdup ("");

  frequency = nm_access_point_get_frequency (self->ap);

  if (!frequency)
    return g_strdup ("");

  return g_strdup_printf ("%.2g GHz", frequency / (1000 * 1.0));
}

void
ce_details_page_refresh (CeDetailsPage *self)
{
  g_return_if_fail (CE_IS_DETAILS_PAGE (self));

  self->active_connection = NULL;
  if (self->device)
    self->active_connection = nm_device_get_active_connection (self->device);

  if (self->device && self->connection && self->active_connection)
    ce_details_page_update_device_details (self);

  ce_details_page_update_wireless (self);
}

gboolean
ce_deails_page_has_active_connection (CeDetailsPage *self,
                                      gboolean       force_refresh)
{
  g_return_val_if_fail (CE_IS_DETAILS_PAGE (self), FALSE);

  if (force_refresh)
    {
      const gchar *uuid_active, *uuid;

      if (self->device)
        self->active_connection = nm_device_get_active_connection (self->device);

      if (self->connection && self->active_connection)
        {
          uuid_active = nm_active_connection_get_uuid (self->active_connection);
          uuid = nm_connection_get_uuid (self->connection);

          self->device_is_active = g_str_equal (uuid, uuid_active);
        }
    }

  return self->device_is_active;
}

gchar *
ce_details_page_get_last_used (CeDetailsPage *self)
{
  NMSettingConnection *setting;
  guint64 last_used;

  g_return_val_if_fail (CE_IS_DETAILS_PAGE (self), ("Unknown"));
  g_return_val_if_fail (self->connection, ("Unknown"));

  if (ce_deails_page_has_active_connection (self, FALSE))
    return g_strdup (_("Now"));

  setting = nm_connection_get_setting_connection (self->connection);
  last_used = nm_setting_connection_get_timestamp (setting);

  return ce_details_get_human_time (last_used);
}

const gchar *
ce_details_page_get_gateway (CeDetailsPage *self)
{
  NMIPConfig *ip_config;
  const gchar *gateway = NULL;

  g_return_val_if_fail (CE_IS_DETAILS_PAGE (self), "");

  if (!ce_deails_page_has_active_connection (self, TRUE))
    return "";

  /* Check IPv4 Gateway */
  ip_config = nm_active_connection_get_ip4_config (self->active_connection);
  if (ip_config)
    gateway = nm_ip_config_get_gateway (ip_config);

  if (gateway && *gateway)
    return gateway;

  /* Check IPv6 Gateway */
  ip_config = nm_active_connection_get_ip6_config (self->active_connection);
  if (ip_config)
    gateway = nm_ip_config_get_gateway (ip_config);

  if (gateway)
    return gateway;

  return "";
}

gchar *
ce_details_page_get_security (CeDetailsPage *self)
{
  GString *str;
  NM80211ApSecurityFlags wpa_flags, rsn_flags;
  NM80211ApFlags flags;

  g_return_val_if_fail (CE_IS_DETAILS_PAGE (self), g_strdup (""));

  if (!self->ap)
    return g_strdup ("");

  flags = nm_access_point_get_flags (self->ap);
  wpa_flags = nm_access_point_get_wpa_flags (self->ap);
  rsn_flags = nm_access_point_get_rsn_flags (self->ap);

  str = g_string_new ("");
  if ((flags & NM_802_11_AP_FLAGS_PRIVACY) &&
      (wpa_flags == NM_802_11_AP_SEC_NONE) &&
      (rsn_flags == NM_802_11_AP_SEC_NONE))
    /* TRANSLATORS: this WEP WiFi security */
    g_string_append_printf (str, "%s, ", _("WEP"));
  if (wpa_flags != NM_802_11_AP_SEC_NONE)
    /* TRANSLATORS: this WPA WiFi security */
    g_string_append_printf (str, "%s, ", _("WPA"));
  if (rsn_flags != NM_802_11_AP_SEC_NONE)
    /* TRANSLATORS: this WPA WiFi security */
    g_string_append_printf (str, "%s, ", _("WPA2"));
  if ((wpa_flags & NM_802_11_AP_SEC_KEY_MGMT_802_1X) ||
      (rsn_flags & NM_802_11_AP_SEC_KEY_MGMT_802_1X))
    /* TRANSLATORS: this Enterprise WiFi security */
    g_string_append_printf (str, "%s, ", _("Enterprise"));

  /* Strip the comma suffix */
  if (str->len > 0)
    g_string_set_size (str, str->len - 2);
  else
    g_string_append (str, C_("Wifi security", "None"));

  return g_string_free (str, FALSE);
}
