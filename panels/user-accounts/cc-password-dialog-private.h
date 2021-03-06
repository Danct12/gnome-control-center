/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* cc-password-dialog-private.c
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

#include "cc-password-dialog.h"

G_BEGIN_DECLS

#define MINIMUM_PASSCODE_LENGTH 6

void cc_passcode_entry_text_inserted_cb (CcPasswordDialog *self,
                                         gchar            *new_text,
                                         gint              new_text_length,
                                         gpointer          position,
                                         GtkEditable      *editable);

void passcode_entry_changed (GtkEntry *passcode_entry,
                             GtkEntry *verify_entry);
G_END_DECLS
