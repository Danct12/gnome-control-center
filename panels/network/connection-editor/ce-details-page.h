/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ce-wifi-page.h
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

#define CE_TYPE_DETAILS_PAGE (ce_details_page_get_type())
G_DECLARE_FINAL_TYPE (CeDetailsPage, ce_details_page, CE, DETAILS_PAGE, GtkBox)

void         ce_details_page_set_parent_window (CeDetailsPage *self,
                                                GtkWindow     *parent_window);
void         ce_details_page_set_nm_client   (CeDetailsPage *self,
                                              NMClient      *nm_client);
void         ce_details_page_set_connection  (CeDetailsPage *self,
                                              NMConnection  *orig_connection,
                                              NMConnection  *connection,
                                              NMDevice      *device);
void         ce_details_page_set_ap          (CeDetailsPage *self,
                                              NMAccessPoint *ap);
void         ce_details_page_refresh         (CeDetailsPage *self);
const gchar *ce_details_page_get_ap_strength (CeDetailsPage *self);
gchar       *ce_details_page_get_ap_frequency (CeDetailsPage *self);
gboolean     ce_deails_page_has_active_connection (CeDetailsPage *self,
                                                   gboolean       force_refresh);
gchar       *ce_details_page_get_last_used   (CeDetailsPage *self);
const gchar *ce_details_page_get_gateway     (CeDetailsPage *self);
gchar       *ce_details_page_get_security    (CeDetailsPage *self);

G_END_DECLS
