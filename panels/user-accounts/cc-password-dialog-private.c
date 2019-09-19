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

#include "cc-password-dialog-private.h"

void
cc_passcode_entry_text_inserted_cb (CcPasswordDialog *self,
                                    gchar            *new_text,
                                    gint              new_text_length,
                                    gpointer          position,
                                    GtkEditable      *editable)
{
  size_t digit_end;
  size_t len;

  g_assert (CC_IS_PASSWORD_DIALOG (self));
  g_assert (GTK_IS_WIDGET (editable));

  if (!new_text || !*new_text)
    return;

  if (new_text_length == 1 && g_ascii_isdigit (*new_text))
    return;

  if (new_text_length == -1)
    len = strlen (new_text);
  else
    len = new_text_length;

  if (len == 1 && g_ascii_isdigit (*new_text))
    return;

  digit_end = strspn (new_text, "1234567890");

  /* User inserted only numbers */
  if (digit_end == len)
    return;

  g_signal_stop_emission_by_name (editable, "insert-text");
  gtk_widget_error_bell (GTK_WIDGET (editable));
}


void
passcode_entry_changed (GtkEntry *passcode_entry,
                        GtkEntry *verify_entry)
{
  const gchar *text;

  g_assert (GTK_IS_ENTRY (passcode_entry));
  g_assert (GTK_IS_ENTRY (verify_entry));

  text = gtk_entry_get_text (passcode_entry);

  gtk_widget_set_sensitive (GTK_WIDGET (verify_entry),
                            strlen (text) >= MINIMUM_PASSCODE_LENGTH);
}
