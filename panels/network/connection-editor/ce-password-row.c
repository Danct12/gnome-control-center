/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ce-password-row.c
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

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "ce-password-row"

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <glib/gi18n.h>
#include <NetworkManager.h>

#define HANDY_USE_UNSTABLE_API
#include <handy.h>

#include "ui-helpers.h"
#include "list-box-helper.h"
#include "ce-password-row.h"

/**
 * @short_description: A widget to show passwords
 * @include: "ce-password-row.h"
 *
 * A #GtkListBoxRow to show/modify passwords
 */

struct _CePasswordRow
{
  GtkListBoxRow    parent_instance;

  GtkLabel        *password_mask_label;
  GtkToggleButton *password_button;
  GtkImage        *button_image;
  GtkEntry        *password_entry;
  gchar           *password;
};

G_DEFINE_TYPE (CePasswordRow, ce_password_row, GTK_TYPE_LIST_BOX_ROW)

enum {
  CHANGED,
  N_SIGNALS
};

static guint signals[N_SIGNALS];

static void
password_entry_changed_cb (CePasswordRow *self)
{
  g_autofree gchar *text = NULL;
  guint16 length;

  g_assert (CE_IS_PASSWORD_ROW (self));

  length = gtk_entry_get_text_length (self->password_entry);

  if (length > 0)
    {
      GString *str = g_string_sized_new (length * 2 + 1);

      /* Limit to 10 char */
      if (length > 10)
        length = 10;

      while (length--)
        g_string_append (str, "●");

      text = g_string_free (str, FALSE);
    }
  else
    text = g_strdup ("");

  gtk_label_set_text (self->password_mask_label, text);

  g_signal_emit (self, signals[CHANGED], 0);
}

static void
ce_password_row_finalize (GObject *object)
{
  CePasswordRow *self = (CePasswordRow *)object;

  ce_password_row_clear_password (self);

  G_OBJECT_CLASS (ce_password_row_parent_class)->finalize (object);
}

static void
ce_password_row_class_init (CePasswordRowClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = ce_password_row_finalize;

  signals[CHANGED] =
    g_signal_new ("changed",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_FIRST,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/control-center/"
                                               "network/ce-password-row.ui");

  gtk_widget_class_bind_template_child (widget_class, CePasswordRow, password_mask_label);
  gtk_widget_class_bind_template_child (widget_class, CePasswordRow, password_button);
  gtk_widget_class_bind_template_child (widget_class, CePasswordRow, button_image);
  gtk_widget_class_bind_template_child (widget_class, CePasswordRow, password_entry);

  gtk_widget_class_bind_template_callback (widget_class, password_entry_changed_cb);
}

static void
ce_password_row_init (CePasswordRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

void
ce_password_row_set_password (CePasswordRow *self,
                              const gchar   *password)
{
  g_return_if_fail (CE_IS_PASSWORD_ROW (self));

  if (!password)
    password = "";

  gtk_entry_set_text (self->password_entry, password);
}

const gchar *
ce_password_row_get_password (CePasswordRow *self)
{
  g_return_val_if_fail (CE_IS_PASSWORD_ROW (self), "");

  return gtk_entry_get_text (self->password_entry);
}

void
ce_password_row_clear_password (CePasswordRow *self)
{
  g_return_if_fail (CE_IS_PASSWORD_ROW (self));

  if (!self->password)
    return;

  memset (self->password, 0, strlen (self->password));
  g_free (self->password);
}

void
ce_password_row_set_visible (CePasswordRow *self,
                             gboolean       visible)
{
  g_return_if_fail (CE_IS_PASSWORD_ROW (self));

  gtk_toggle_button_set_active (self->password_button, visible);
}

void
ce_password_row_set_error (CePasswordRow *self,
                           gboolean       error)
{
  g_return_if_fail (CE_IS_PASSWORD_ROW (self));

  if (error)
    gtk_style_context_add_class (gtk_widget_get_style_context (self->password_entry), "error");
  else
    gtk_style_context_remove_class (gtk_widget_get_style_context (self->password_entry), "error");
}
