/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ce-security-page.c
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
#define G_LOG_DOMAIN "ce-security-page"

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <glib/gi18n.h>
#include <NetworkManager.h>

#define HANDY_USE_UNSTABLE_API
#include <handy.h>

#include "ui-helpers.h"
#include "list-box-helper.h"
#include "ce-security-page.h"
#include "ce-password-row.h"

/**
 * @short_description: The Security page for a connection
 * @include: "ce-security-page.h"
 *
 * Show Security settings related to a network connection
 */

struct _CeSecurityPage
{
  GtkBox         parent_instance;

  GtkWidget     *main_list_box;
  HdyComboRow   *connection_list_row;
  GListStore    *connection_list;
  GtkBox        *name_box;
  GtkEntry      *name_entry;

  HdyComboRow   *security_method_row;
  GListStore    *security_methods;
  gchar         *current_method;

  CePasswordRow *wpa_password_row;
  CePasswordRow *wep_password_row;
  GtkWidget     *wep_index_row;
  GtkWidget     *wep_authentication_row;
  GListStore    *wep_indexes;
  GListStore    *wep_authentications;

  NMClient      *nm_client;
  NMDevice      *device;
  NMConnection  *connection;
  NMConnection  *orig_connection;
  NMSetting     *setting;
  NMUtilsSecurityType security_type;

  gboolean       has_error;
  gboolean       allow_create;
};

G_DEFINE_TYPE (CeSecurityPage, ce_security_page, GTK_TYPE_BOX)

enum {
  CHANGED,
  N_SIGNALS
};

static guint signals[N_SIGNALS];

enum {
  PROP_0,
  PROP_ALLOW_CREATE,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static gboolean
security_has_proto (NMSettingWirelessSecurity *sec, const char *item)
{
  g_assert (sec);
  g_assert (item);

  for (guint32 i = 0; i < nm_setting_wireless_security_get_num_protos (sec); i++)
    if (!strcmp (item, nm_setting_wireless_security_get_proto (sec, i)))
      return TRUE;

  return FALSE;
}

static void
wep_password_changed_cb (CeSecurityPage *self)
{
  g_autoptr(HdyValueObject) object = NULL;
  const gchar *method, *password;
  gint index;
  gboolean valid = FALSE;

  g_assert (CE_IS_SECURITY_PAGE (self));

  index = hdy_combo_row_get_selected_index (HDY_COMBO_ROW (self->security_method_row));
  object = g_list_model_get_item (G_LIST_MODEL (self->security_methods), index);
  method = g_object_get_data (G_OBJECT (object), "value");
  password = ce_password_row_get_password (self->wep_password_row);

  if (password && *password)
    {
      if (g_str_equal (method, "wep"))
        valid = nm_utils_wep_key_valid (password, NM_WEP_KEY_TYPE_KEY);
      else
        valid = nm_utils_wep_key_valid (password, NM_WEP_KEY_TYPE_PASSPHRASE);
    }

  ce_password_row_set_error (self->wep_password_row, !valid);
  self->has_error = !valid;
}

static void
wpa_password_changed_cb (CeSecurityPage *self)
{
  const gchar *password;
  gboolean valid = FALSE;

  g_assert (CE_IS_SECURITY_PAGE (self));

  password = ce_password_row_get_password (self->wpa_password_row);
  if (password && *password)
    valid = nm_utils_wpa_psk_valid (password);

  ce_password_row_set_error (self->wpa_password_row, !valid);
  self->has_error = !valid;
}

/* Modified from ce-page-security.c */
static NMUtilsSecurityType
security_page_get_type (CeSecurityPage *self)
{
  NMSettingWirelessSecurity *setting;
  const char *key_mgmt, *auth_alg;

  g_assert (CE_IS_SECURITY_PAGE (self));
  g_return_val_if_fail (self->connection, NMU_SEC_NONE);

  setting = nm_connection_get_setting_wireless_security (self->connection);

  if (!setting)
    return NMU_SEC_NONE;

  key_mgmt = nm_setting_wireless_security_get_key_mgmt (setting);
  auth_alg = nm_setting_wireless_security_get_auth_alg (setting);

  /* No IEEE 802.1x */
  if (!strcmp (key_mgmt, "none"))
    return NMU_SEC_STATIC_WEP;

  if (!strcmp (key_mgmt, "ieee8021x"))
    {
      if (auth_alg && !strcmp (auth_alg, "leap"))
        return NMU_SEC_LEAP;
      return NMU_SEC_DYNAMIC_WEP;
    }

  if (!strcmp (key_mgmt, "wpa-none") ||
      !strcmp (key_mgmt, "wpa-psk"))
    {
      if (security_has_proto (setting, "rsn"))
        return NMU_SEC_WPA2_PSK;
      else
        return NMU_SEC_WPA_PSK;
    }

  if (!strcmp (key_mgmt, "wpa-eap"))
    {
      if (security_has_proto (setting, "rsn"))
        return NMU_SEC_WPA2_ENTERPRISE;
      else
        return NMU_SEC_WPA_ENTERPRISE;
    }

  return NMU_SEC_INVALID;
}

static void
connection_list_item_changed_cb (CeSecurityPage *self)
{
  g_autoptr(HdyValueObject) object = NULL;
  NMConnection *connection;
  gint index;

  g_assert (CE_IS_SECURITY_PAGE (self));

  index = hdy_combo_row_get_selected_index (HDY_COMBO_ROW (self->connection_list_row));
  if (index == -1)
    return;

  /* New connection */
  if (index == 0)
    {
      gtk_widget_set_sensitive (GTK_WIDGET (self->name_box), TRUE);
      gtk_entry_set_text (self->name_entry, "");
      ce_password_row_set_password (self->wpa_password_row, "");
      ce_password_row_set_password (self->wep_password_row, "");
      hdy_combo_row_set_selected_index (self->security_method_row, 0);

      return;
    }

  gtk_widget_set_sensitive (GTK_WIDGET (self->name_box), FALSE);

  object = g_list_model_get_item (G_LIST_MODEL (self->connection_list), index);
  connection = g_object_get_data (G_OBJECT (object), "value");
  g_assert (connection);
  g_set_object (&self->connection, connection);
  g_set_object (&self->orig_connection, connection);

  gtk_entry_set_text (self->name_entry, nm_connection_get_id (connection));
  ce_security_page_refresh (self);
}

static void
security_page_modified_cb (CeSecurityPage *self)
{
  g_autoptr(HdyValueObject) object = NULL;
  gchar *method;
  gint index;

  g_assert (CE_IS_SECURITY_PAGE (self));

  index = hdy_combo_row_get_selected_index (HDY_COMBO_ROW (self->security_method_row));
  if (index == -1)
    return;

  object = g_list_model_get_item (G_LIST_MODEL (self->security_methods), index);
  g_assert (G_IS_OBJECT (object));
  method = g_object_get_data (G_OBJECT (object), "value");

  self->has_error = FALSE;

  if (g_str_equal (method, "wpa"))
    {
      gtk_widget_hide (GTK_WIDGET (self->wep_password_row));
      gtk_widget_show (GTK_WIDGET (self->wpa_password_row));
      wpa_password_changed_cb (self);
    }
  else if (g_str_has_prefix (method, "wep"))
    {
      gtk_widget_hide (GTK_WIDGET (self->wpa_password_row));
      gtk_widget_show (GTK_WIDGET (self->wep_password_row));
      wep_password_changed_cb (self);
    }

  if (self->allow_create && !self->has_error)
    {
      const gchar *name;

      name = gtk_entry_get_text (self->name_entry);

      if (!name || !*name)
        self->has_error = TRUE;
    }

  g_signal_emit (self, signals[CHANGED], 0);
}

static void
ce_security_page_load_wpa (CeSecurityPage            *self,
                           NMSettingWirelessSecurity *setting)
{
  const gchar *password;

  g_assert (CE_IS_SECURITY_PAGE (self));
  g_assert (self->security_type == NMU_SEC_WPA_PSK ||
            self->security_type == NMU_SEC_WPA2_PSK);

  password = nm_setting_wireless_security_get_psk (setting);

  ce_password_row_set_password (self->wpa_password_row, password);
  hdy_combo_row_set_selected_index (HDY_COMBO_ROW (self->security_method_row), 1);
}

static void
ce_security_page_load_wep (CeSecurityPage            *self,
                           NMSettingWirelessSecurity *setting)
{
  const gchar *auth, *password;
  NMWepKeyType type;
  gint index;

  g_assert (CE_IS_SECURITY_PAGE (self));
  g_assert (self->security_type == NMU_SEC_STATIC_WEP);

  type = nm_setting_wireless_security_get_wep_key_type (setting);
  auth = nm_setting_wireless_security_get_auth_alg (setting);

  if (type == NM_WEP_KEY_TYPE_PASSPHRASE)
    hdy_combo_row_set_selected_index (HDY_COMBO_ROW (self->security_method_row), 3);
  else
    hdy_combo_row_set_selected_index (HDY_COMBO_ROW (self->security_method_row), 2);

  g_warning ("auth: %s", auth);
  if (auth && g_str_equal (auth, "shared"))
    hdy_combo_row_set_selected_index (HDY_COMBO_ROW (self->wep_authentication_row), 1);
  else
    hdy_combo_row_set_selected_index (HDY_COMBO_ROW (self->wep_authentication_row), 0);

  index = nm_setting_wireless_security_get_wep_tx_keyidx (setting);
  hdy_combo_row_set_selected_index (HDY_COMBO_ROW (self->wep_index_row), index);

  password = nm_setting_wireless_security_get_wep_key (setting, index);
  ce_password_row_set_password (self->wep_password_row, password);
}

static void
ce_security_page_update (CeSecurityPage *self)
{
  NMSettingWirelessSecurity *setting;
  NMUtilsSecurityType security_type;

  g_assert (CE_IS_SECURITY_PAGE (self));

  setting = nm_connection_get_setting_wireless_security (self->connection);
  security_type = security_page_get_type (self);
  self->security_type = security_type;

  if (security_type != NMU_SEC_NONE &&
      NM_IS_REMOTE_CONNECTION (self->orig_connection))
    {
      g_autoptr(GVariant) secrets = NULL;
      g_autoptr(GError) error = NULL;

      secrets = nm_remote_connection_get_secrets (NM_REMOTE_CONNECTION (self->orig_connection),
                                                  NM_SETTING_WIRELESS_SECURITY_SETTING_NAME,
                                                  NULL, &error);
      if (!error)
        nm_connection_update_secrets (self->connection,
                                      NM_SETTING_WIRELESS_SECURITY_SETTING_NAME,
                                      secrets, &error);
      if (error)
        g_warning ("Error: %s", error->message);
    }

  if (security_type == NMU_SEC_NONE)
    hdy_combo_row_set_selected_index (HDY_COMBO_ROW (self->security_method_row), 0);
  else if (security_type == NMU_SEC_WPA_PSK ||
           security_type == NMU_SEC_WPA2_PSK)
    ce_security_page_load_wpa (self, setting);
  else if (security_type == NMU_SEC_STATIC_WEP)
    ce_security_page_load_wep (self, setting);
  else
    g_return_if_reached ();

  nm_connection_clear_secrets (self->connection);
  security_page_modified_cb (self);
}

static void
ce_security_page_populate_connection_list (CeSecurityPage *self)
{
  HdyValueObject *object;
  const GPtrArray *connections;
  g_autoptr(GPtrArray) valid_connections = NULL;

  g_assert (CE_IS_SECURITY_PAGE (self));

  if (!self->nm_client || !self->device)
    return;

  if (!gtk_widget_get_visible (GTK_WIDGET (self->connection_list_row)))
    return;

  object = hdy_value_object_new_string (_("New"));
  g_object_set_data (G_OBJECT (object), "value", NULL);
  g_list_store_append (self->connection_list, object);
  g_object_unref (object);

  connections = nm_client_get_connections (self->nm_client);
  valid_connections = nm_device_filter_connections (self->device, connections);

  for (int i = 0; i < valid_connections->len; i++)
    {
      NMConnection *connection;
      g_autofree gchar *mac_address = NULL;
      g_autofree gchar *item = NULL;

      connection = valid_connections->pdata[i];

      object = hdy_value_object_new_string (nm_connection_get_id (connection));
      g_object_set_data_full (G_OBJECT (object), "value",
                              g_object_ref (connection), g_object_unref);
      g_list_store_append (self->connection_list, object);
      g_object_unref (object);
    }
}

static void
ce_security_page_populate_stores (CeSecurityPage *self)
{
  HdyValueObject *object;

  g_assert (CE_IS_SECURITY_PAGE (self));

  /* Don’t change the order items are added.  It’ll break the code */

  /* Security methods */
  object = hdy_value_object_new_string (_("None"));
  g_object_set_data (G_OBJECT (object), "value", "off");
  g_list_store_append (self->security_methods, object);
  g_object_unref (object);

  object = hdy_value_object_new_string (_("WPA & WPA2 Personal"));
  g_object_set_data (G_OBJECT (object), "value", "wpa");
  g_list_store_append (self->security_methods, object);
  g_object_unref (object);


  object = hdy_value_object_new_string (_("WEP 40/128-bit Key (Hex or ASCII)"));
  g_object_set_data (G_OBJECT (object), "value", "wep");
  g_list_store_append (self->security_methods, object);
  g_object_unref (object);

  object = hdy_value_object_new_string (_("WEP 128-bit Passphrase"));
  g_object_set_data (G_OBJECT (object), "value", "wep128");
  g_list_store_append (self->security_methods, object);
  g_object_unref (object);


  /* WEP Indexes */
  object = hdy_value_object_new_string (_("1 (Default)"));
  g_list_store_append (self->wep_indexes, object);
  g_object_unref (object);

  object = hdy_value_object_new_string ("2");
  g_list_store_append (self->wep_indexes, object);
  g_object_unref (object);

  object = hdy_value_object_new_string ("3");
  g_list_store_append (self->wep_indexes, object);
  g_object_unref (object);

  object = hdy_value_object_new_string ("4");
  g_list_store_append (self->wep_indexes, object);
  g_object_unref (object);


  /* WEP Authentications */
  object = hdy_value_object_new_string (_("Open System"));
  g_list_store_append (self->wep_authentications, object);
  g_object_unref (object);

  object = hdy_value_object_new_string (_("Shared Key"));
  g_list_store_append (self->wep_authentications, object);
  g_object_unref (object);
}

static void
wep_index_changed_cb (CeSecurityPage *self)
{
  g_assert (CE_IS_SECURITY_PAGE (self));

  g_signal_emit (self, signals[CHANGED], 0);
}

static void
wep_authentication_changed_cb (CeSecurityPage *self)
{
  g_assert (CE_IS_SECURITY_PAGE (self));

  g_signal_emit (self, signals[CHANGED], 0);
}

static void
ce_security_page_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  CeSecurityPage *self = CE_SECURITY_PAGE (object);

  switch (prop_id)
    {
    case PROP_ALLOW_CREATE:
      self->allow_create = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ce_security_page_constructed (GObject *object)
{
  CeSecurityPage *self = CE_SECURITY_PAGE (object);

  if (self->allow_create)
    {
      /* When allow_create set, the name will be empty, and thus set error */
      self->has_error = TRUE;
      gtk_entry_set_text (self->name_entry, "");
      hdy_combo_row_set_selected_index (self->security_method_row, 0);
      gtk_widget_show (GTK_WIDGET (self->connection_list_row));
    }

  G_OBJECT_CLASS (ce_security_page_parent_class)->constructed (object);
}

static void
ce_security_page_finalize (GObject *object)
{
  CeSecurityPage *self = CE_SECURITY_PAGE (object);

  g_clear_object (&self->security_methods);
  g_clear_object (&self->wep_indexes);
  g_clear_object (&self->wep_authentications);

  g_clear_object (&self->device);
  g_clear_object (&self->connection);
  g_clear_object (&self->orig_connection);
  g_clear_object (&self->nm_client);

  G_OBJECT_CLASS (ce_security_page_parent_class)->finalize (object);
}

static void
ce_security_page_class_init (CeSecurityPageClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->set_property = ce_security_page_set_property;
  object_class->constructed = ce_security_page_constructed;
  object_class->finalize = ce_security_page_finalize;

  properties[PROP_ALLOW_CREATE] =
    g_param_spec_boolean ("allow-create",
                          "Allow Create",
                          "Allow Creating new connection",
                          FALSE,
                          G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY |G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  signals[CHANGED] =
    g_signal_new ("changed",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_FIRST,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/control-center/"
                                               "network/ce-security-page.ui");

  gtk_widget_class_bind_template_child (widget_class, CeSecurityPage, main_list_box);
  gtk_widget_class_bind_template_child (widget_class, CeSecurityPage, connection_list_row);
  gtk_widget_class_bind_template_child (widget_class, CeSecurityPage, name_box);
  gtk_widget_class_bind_template_child (widget_class, CeSecurityPage, name_entry);

  gtk_widget_class_bind_template_child (widget_class, CeSecurityPage, security_method_row);
  gtk_widget_class_bind_template_child (widget_class, CeSecurityPage, wpa_password_row);
  gtk_widget_class_bind_template_child (widget_class, CeSecurityPage, wep_password_row);
  gtk_widget_class_bind_template_child (widget_class, CeSecurityPage, wep_index_row);
  gtk_widget_class_bind_template_child (widget_class, CeSecurityPage, wep_authentication_row);

  gtk_widget_class_bind_template_callback (widget_class, connection_list_item_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, security_page_modified_cb);

  gtk_widget_class_bind_template_callback (widget_class, wep_index_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, wep_authentication_changed_cb);

  g_type_ensure (CE_TYPE_PASSWORD_ROW);
}

static void
ce_security_page_init (CeSecurityPage *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->connection_list = g_list_store_new (HDY_TYPE_VALUE_OBJECT);
  hdy_combo_row_bind_name_model (HDY_COMBO_ROW (self->connection_list_row),
                                 G_LIST_MODEL (self->connection_list),
                                 (HdyComboRowGetNameFunc) hdy_value_object_dup_string,
                                 NULL, NULL);

  self->security_methods = g_list_store_new (HDY_TYPE_VALUE_OBJECT);
  self->wep_indexes = g_list_store_new (HDY_TYPE_VALUE_OBJECT);
  self->wep_authentications = g_list_store_new (HDY_TYPE_VALUE_OBJECT);
  ce_security_page_populate_stores (self);

  hdy_combo_row_bind_name_model (HDY_COMBO_ROW (self->security_method_row),
                                 G_LIST_MODEL (self->security_methods),
                                 (HdyComboRowGetNameFunc) hdy_value_object_dup_string,
                                 NULL, NULL);

  hdy_combo_row_bind_name_model (HDY_COMBO_ROW (self->wep_index_row),
                                 G_LIST_MODEL (self->wep_indexes),
                                 (HdyComboRowGetNameFunc) hdy_value_object_dup_string,
                                 NULL, NULL);

  hdy_combo_row_bind_name_model (HDY_COMBO_ROW (self->wep_authentication_row),
                                 G_LIST_MODEL (self->wep_authentications),
                                 (HdyComboRowGetNameFunc) hdy_value_object_dup_string,
                                 NULL, NULL);

  gtk_list_box_set_header_func (GTK_LIST_BOX (self->main_list_box),
                                cc_list_box_update_header_func,
                                NULL, NULL);

}

void
ce_security_page_set_nm_client (CeSecurityPage *self,
                                NMClient       *nm_client)
{
  g_return_if_fail (CE_IS_SECURITY_PAGE (self));
  g_return_if_fail (NM_IS_CLIENT (nm_client));

  g_set_object (&self->nm_client, nm_client);
}

void
ce_security_page_set_connection (CeSecurityPage *self,
                                 NMConnection   *orig_connection,
                                 NMConnection   *connection,
                                 NMDevice       *device)
{
  g_return_if_fail (CE_IS_SECURITY_PAGE (self));
  g_return_if_fail (!connection || NM_IS_CONNECTION (connection));
  g_return_if_fail (!orig_connection || NM_IS_CONNECTION (orig_connection));
  g_return_if_fail (NM_IS_DEVICE (device));

  g_set_object (&self->connection, connection);
  g_set_object (&self->orig_connection, orig_connection);
  g_set_object (&self->device, device);

  ce_security_page_populate_connection_list (self);
}

void
ce_security_page_refresh (CeSecurityPage *self)
{
  g_return_if_fail (CE_IS_SECURITY_PAGE (self));

  ce_password_row_set_visible (self->wpa_password_row, FALSE);
  ce_security_page_update (self);
}

void
ce_security_page_save_connection (CeSecurityPage *self)
{
  NMSetting *setting;
  g_autoptr(HdyValueObject) object = NULL;
  const gchar *method, *password;
  gint index;

  g_return_if_fail (CE_IS_SECURITY_PAGE (self));

  if (!self->connection)
    {
      const gchar *str;
      g_autoptr(GBytes) ssid = NULL;

      g_return_if_fail (self->allow_create);

      self->connection = nm_simple_connection_new ();
      setting = nm_setting_wireless_new ();
      str = gtk_entry_get_text (self->name_entry);
      ssid = g_bytes_new_static (str, strlen (str));
      g_object_set (setting, "ssid", ssid, NULL);
      nm_connection_add_setting (self->connection, setting);

      setting = nm_setting_connection_new ();
      g_object_set (setting, "id", gtk_entry_get_text (self->name_entry), NULL);
      nm_connection_add_setting (self->connection, setting);
    }

  index = hdy_combo_row_get_selected_index (HDY_COMBO_ROW (self->security_method_row));
  if (index == -1 || self->connection == NULL)
    g_return_if_reached ();

  nm_connection_remove_setting (self->connection, NM_TYPE_SETTING_WIRELESS_SECURITY);
  nm_connection_remove_setting (self->connection, NM_TYPE_SETTING_802_1X);

  if (index == 0)
    return;

  object = g_list_model_get_item (G_LIST_MODEL (self->security_methods), index);
  g_assert (G_IS_OBJECT (object));
  method = g_object_get_data (G_OBJECT (object), "value");

  setting = nm_setting_wireless_security_new ();
  nm_connection_add_setting (self->connection, setting);
  nm_setting_set_secret_flags (setting, NM_SETTING_WIRELESS_SECURITY_PSK,
                               NM_SETTING_SECRET_FLAG_NONE, NULL);
  if (g_str_equal (method, "wpa"))
    {
      password = ce_password_row_get_password (self->wpa_password_row);

      g_object_set (setting, NM_SETTING_WIRELESS_SECURITY_PSK, password, NULL);
      g_object_set (setting, NM_SETTING_WIRELESS_SECURITY_KEY_MGMT, "wpa-psk", NULL);
    }

  if (g_str_has_prefix (method, "wep"))
    {
      if (g_str_equal (method, "wep"))
        g_object_set (setting, NM_SETTING_WIRELESS_SECURITY_WEP_KEY_TYPE, NM_WEP_KEY_TYPE_KEY, NULL);
      else
        g_object_set (setting, NM_SETTING_WIRELESS_SECURITY_WEP_KEY_TYPE, NM_WEP_KEY_TYPE_PASSPHRASE, NULL);

      index = hdy_combo_row_get_selected_index (HDY_COMBO_ROW (self->wep_authentication_row));
      g_object_set (setting, "auth-alg", (index == 0) ? "open" : "shared", NULL);

      password = ce_password_row_get_password (self->wep_password_row);
      index = hdy_combo_row_get_selected_index (HDY_COMBO_ROW (self->wep_index_row));
      nm_setting_wireless_security_set_wep_key (NM_SETTING_WIRELESS_SECURITY (setting), index, password);

      g_object_set (setting,
                    NM_SETTING_WIRELESS_SECURITY_KEY_MGMT, "none",
                    NM_SETTING_WIRELESS_SECURITY_WEP_TX_KEYIDX, index,
                    NULL);
    }
}

NMConnection *
ce_security_page_get_connection  (CeSecurityPage *self)
{
  g_return_val_if_fail (CE_IS_SECURITY_PAGE (self), NULL);

  return self->connection;
}

gboolean
ce_security_page_has_error (CeSecurityPage *self)
{
  g_return_val_if_fail (CE_IS_SECURITY_PAGE (self), TRUE);

  return self->has_error;
}
