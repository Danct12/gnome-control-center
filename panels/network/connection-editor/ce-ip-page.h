/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ce-ip-page.h
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

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define CE_TYPE_IP_PAGE (ce_ip_page_get_type())
G_DECLARE_FINAL_TYPE (CeIpPage, ce_ip_page, CE, IP_PAGE, GtkBox)

void         ce_ip_page_set_parent_window (CeIpPage     *self,
                                           GtkWindow    *parent_window);
void         ce_ip_page_set_nm_client   (CeIpPage     *self,
                                         NMClient     *nm_client);
void         ce_ip_page_set_version     (CeIpPage     *self,
                                         gint          version);
void         ce_ip_page_set_connection  (CeIpPage     *self,
                                         NMConnection *connection,
                                         NMDevice     *device);
gboolean     ce_ip_page_has_error       (CeIpPage     *self);
void         ce_ip_page_save_connection (CeIpPage     *self);
void         ce_ip_page_refresh         (CeIpPage     *self);

G_END_DECLS
