/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* network-agent.c
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

#define GCR_API_SUBJECT_TO_CHANGE
#include <gcr/gcr.h>
#include "shell-network-agent.h"

static gchar *
network_agent_get_key_type (NMConnection *connection)
{
  NMSettingWirelessSecurity *setting;
  const gchar *key_mgmt;

  g_assert (NM_IS_CONNECTION (connection));

  setting = nm_connection_get_setting_wireless_security (connection);
  key_mgmt = nm_setting_wireless_security_get_key_mgmt (setting);

  if (g_str_equal (key_mgmt, "none"))
    return "wep-key0";

  /* assume it's WEP/WEP2 */
  return "psk";
}

static void
ask_for_password (ShellNetworkAgent *agent,
                  NMClient          *nm_client,
                  NMConnection      *connection,
                  gchar             *request_id)
{
  NMSettingWireless *setting;
  GcrPrompt *prompt;
  g_autofree gchar *str = NULL;
  g_autofree gchar *ssid = NULL;
  g_autoptr(GError) error = NULL;
  const gchar *password;
  gchar *key_type;
  GBytes *bytes;
  gboolean ask_again;

  g_assert (SHELL_IS_NETWORK_AGENT (agent));
  g_assert (NM_IS_CLIENT (nm_client));
  g_assert (NM_IS_CONNECTION (connection));

  setting = nm_connection_get_setting_wireless (connection);

  if (!setting)
    {
      g_warning ("Only Wifi Networks currently supported");
      return;
    }

  prompt = gcr_system_prompt_open (-1, NULL, &error);

  if (error)
    {
      g_warning ("Error: %s", error->message);
      return;
    }

  bytes = nm_setting_wireless_get_ssid (setting);
  ssid = nm_utils_ssid_to_utf8 (g_bytes_get_data (bytes, NULL),
                                g_bytes_get_size (bytes));
  str = g_strdup_printf ("Enter password for '%s'", ssid);
  gcr_prompt_set_message (prompt, str);

  password = gcr_prompt_password_run (prompt, NULL, &error);
  if (!password)
    shell_network_agent_respond (agent, request_id, SHELL_NETWORK_AGENT_USER_CANCELED);
  else if (error)
    {
      shell_network_agent_respond (agent, request_id, SHELL_NETWORK_AGENT_INTERNAL_ERROR);
      g_warning ("Error: %s", error->message);
      goto end;
    }

  if (password)
    {
      key_type = network_agent_get_key_type (connection);
      shell_network_agent_set_password (agent, request_id, key_type, (gchar *)password);
      shell_network_agent_respond (agent, request_id, SHELL_NETWORK_AGENT_CONFIRMED);
    }

 end:
  g_object_unref (prompt);
}

static void
secret_request_new_cb (NMClient                      *nm_client,
                       gchar                         *request_id,
                       NMConnection                  *connection,
                       gchar                         *setting_name,
                       gchar                        **hints,
                       NMSecretAgentGetSecretsFlags   flags,
                       ShellNetworkAgent             *agent)
{
  g_assert (NM_IS_CLIENT (nm_client));
  g_assert (NM_IS_CONNECTION (connection));
  g_assert (SHELL_IS_NETWORK_AGENT (agent));

  ask_for_password (agent, nm_client, connection, request_id);
}

static ShellNetworkAgent *
network_agent_new (NMClient *nm_client)
{
  ShellNetworkAgent *agent;
  g_autoptr(GError) error = NULL;

  g_assert (NM_IS_CLIENT (nm_client));
  g_assert (NM_IS_CLIENT (nm_client));

  agent = g_initable_new (SHELL_TYPE_NETWORK_AGENT, NULL, &error,
                          "identifier", "sm.puri.phosh.NetworkAgent",
                          "auto-register", FALSE, NULL);
  if (error)
    {
      g_warning ("Error: %s", error->message);
      return NULL;
    }

  nm_secret_agent_old_register (NM_SECRET_AGENT_OLD (agent), NULL, &error);

  g_signal_connect_object (agent, "new-request",
                           G_CALLBACK (secret_request_new_cb),
                           nm_client, G_CONNECT_SWAPPED);

  return agent;
}
