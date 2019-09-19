/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

/* This is a temporary patch for net-device-wifi.c to use new
 * hotspot dialog to be used until new version is released. */

CcWifiHotspotDialog *hotspot_dialog;
NMConnection *c;
g_autofree gchar *hostname = NULL;
g_autofree gchar *ssid = NULL;
gint response;

hotspot_dialog = g_object_get_data (G_OBJECT (device_wifi), "hotspot-dialog");

if (!hotspot_dialog)
  {
    hotspot_dialog = cc_wifi_hotspot_dialog_new (GTK_WINDOW (window));
    g_object_set_data (G_OBJECT (window), "hotspot-dialog", hotspot_dialog);
  }

cc_wifi_hotspot_dialog_set_device (hotspot_dialog, NM_DEVICE_WIFI (device));
hostname = get_hostname ();
ssid =  pretty_hostname_to_ssid (hostname);
cc_wifi_hotspot_dialog_set_hostname (hotspot_dialog, ssid);
c = net_device_wifi_get_hotspot_connection (device_wifi);
if (c)
  cc_wifi_hotspot_dialog_set_connection (hotspot_dialog, c);

response = gtk_dialog_run (GTK_DIALOG (hotspot_dialog));

if (response == GTK_RESPONSE_APPLY) {
  NMConnection *connection;
  GCancellable *cancellable;

  cancellable = net_object_get_cancellable (NET_OBJECT (device_wifi));

  connection = cc_wifi_hotspot_dialog_get_connection (hotspot_dialog);
  if (NM_IS_REMOTE_CONNECTION (connection))
    nm_remote_connection_commit_changes_async (NM_REMOTE_CONNECTION (connection),
                                               TRUE,
                                               cancellable,
                                               overwrite_ssid_cb,
                                               device_wifi);
  else
    nm_client_add_and_activate_connection_async (client,
                                                 connection,
                                                 device,
                                                 NULL,
                                                 cancellable,
                                                 activate_new_cb,
                                                 device_wifi);
 }

gtk_widget_hide (GTK_WIDGET (hotspot_dialog));

return;
