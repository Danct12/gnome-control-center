/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ce-security-page.h
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

#define CE_TYPE_SECURITY_PAGE (ce_security_page_get_type())
G_DECLARE_FINAL_TYPE (CeSecurityPage, ce_security_page, CE, SECURITY_PAGE, GtkBox)

void          ce_security_page_set_nm_client   (CeSecurityPage *self,
                                                NMClient       *nm_client);
void          ce_security_page_set_connection  (CeSecurityPage *self,
                                                NMConnection   *orig_connection,
                                                NMConnection   *connection,
                                                NMDevice       *device);
void          ce_security_page_save_connection (CeSecurityPage *self);
NMConnection *ce_security_page_get_connection  (CeSecurityPage *self);
void         ce_security_page_refresh          (CeSecurityPage *self);
gboolean     ce_security_page_has_error        (CeSecurityPage *self);

G_END_DECLS
