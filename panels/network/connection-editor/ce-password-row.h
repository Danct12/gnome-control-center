/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ce-password-row.h
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

#define CE_TYPE_PASSWORD_ROW (ce_password_row_get_type())
G_DECLARE_FINAL_TYPE (CePasswordRow, ce_password_row, CE, PASSWORD_ROW, GtkListBoxRow)

void ce_password_row_set_password   (CePasswordRow *self,
                                     const gchar   *password);
const gchar *ce_password_row_get_password (CePasswordRow *self);
void ce_password_row_clear_password (CePasswordRow *self);
void ce_password_row_set_visible    (CePasswordRow *self,
                                     gboolean       visible);
void ce_password_row_set_error      (CePasswordRow *self,
                                     gboolean       error);

G_END_DECLS
