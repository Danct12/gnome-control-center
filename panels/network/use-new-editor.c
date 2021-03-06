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


CcConnectionEditor *new_editor;

new_editor = g_object_get_data (G_OBJECT (self), "network-editor");
if (!new_editor)
  {
    new_editor = CC_CONNECTION_EDITOR (cc_connection_editor_new (GTK_WINDOW (window), self->client));
    g_object_set_data (G_OBJECT (self), "network-editor", new_editor);
  }

cc_connection_editor_set_connection (new_editor, connection, self->device);
cc_connection_editor_set_ap (new_editor, ap);

gtk_dialog_run (GTK_DIALOG (new_editor));
gtk_widget_hide (GTK_WIDGET (new_editor));

return;
