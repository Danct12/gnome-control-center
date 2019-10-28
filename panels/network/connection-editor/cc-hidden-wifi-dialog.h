/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* cc-hidden-wifi-dialog.h
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

#pragma once

#define HANDY_USE_UNSTABLE_API 1
#include <handy.h>
#include <shell/cc-panel.h>

G_BEGIN_DECLS

#define CC_TYPE_HIDDEN_WIFI_DIALOG (cc_hidden_wifi_dialog_get_type())
G_DECLARE_FINAL_TYPE (CcHiddenWifiDialog, cc_hidden_wifi_dialog, CC, HIDDEN_WIFI_DIALOG, HdyDialog)

GtkWidget    *cc_hidden_wifi_dialog_new            (GtkWindow           *parent_window,
                                                    NMClient            *nm_client);
void          cc_hidden_wifi_dialog_set_device     (CcHiddenWifiDialog  *self,
                                                    NMDevice            *device);
void          cc_hidden_wifi_dialog_set_ap         (CcHiddenWifiDialog  *self,
                                                    NMAccessPoint       *ap);
NMConnection *cc_wifi_hidden_dialog_get_connection (CcHiddenWifiDialog  *self,
                                                    NMDevice           **device,
                                                    NMAccessPoint      **ap);
G_END_DECLS
