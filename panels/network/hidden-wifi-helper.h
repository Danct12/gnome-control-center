
static void
connection_activate_cb (GObject      *object,
                        GAsyncResult *result,
                        gpointer      user_data)
{
  NMClient *nm_client = NM_CLIENT (object);
  g_autoptr(GError) error = NULL;

  g_assert (NM_IS_CLIENT (nm_client));

  if (nm_client_activate_connection_finish (nm_client, result, &error))
    return;

  if (error)
    g_warning ("Connection Error: %s", error->message);
}

static void
connection_add_and_activate_cb (GObject      *object,
                                GAsyncResult *result,
                                gpointer      user_data)
{
  NMClient *nm_client = NM_CLIENT (object);
  NMActiveConnection *active_connection;
  g_autoptr(GError) error = NULL;

  g_assert (NM_IS_CLIENT (nm_client));

  active_connection = nm_client_add_and_activate_connection_finish (nm_client, result, &error);

  if (error)
    g_warning ("Connection Error: %s", error->message);
  else
    g_object_unref (active_connection);
}

static void
hidden_wifi_activate_connection (NMClient      *nm_client,
                                 NMConnection  *connection,
                                 NMDevice      *device,
                                 NMAccessPoint *ap)
{
  g_assert (NM_IS_CLIENT (nm_client));
  g_assert (NM_IS_CONNECTION (connection));

  if (NM_IS_REMOTE_CONNECTION (connection))
    nm_client_activate_connection_async (nm_client, connection, device,
                                         NULL, NULL,
                                         connection_activate_cb, NULL);
  else
    nm_client_add_and_activate_connection_async (nm_client, connection, device,
                                                 NULL, NULL,
                                                 connection_add_and_activate_cb, NULL);

}

static void
hidden_wireless_dialog_response_cb (GtkDialog *dialog,
                                    gint       response,
                                    gpointer   user_data)
{
  NMClient *nm_client = user_data;
  NMConnection *connection = NULL;
  NMDevice *device = NULL;
  NMAccessPoint *ap = NULL;

  g_assert (NM_IS_CLIENT (nm_client));

  gtk_widget_hide (GTK_WIDGET (dialog));

  if (response != GTK_RESPONSE_OK)
    return;

  connection = cc_wifi_hidden_dialog_get_connection (CC_HIDDEN_WIFI_DIALOG (dialog),
                                                     &device, &ap);
  hidden_wifi_activate_connection (nm_client, connection, device, ap);
}

void
cc_network_panel_connect_to_hidden_network (GtkWidget *toplevel,
                                            NMClient  *client,
                                            NMDevice  *wifi_device)
{
  GtkWidget *dialog;

  g_debug ("connect to hidden wifi");
  dialog = cc_hidden_wifi_dialog_new (GTK_WINDOW (toplevel), client);
  cc_hidden_wifi_dialog_set_device (CC_HIDDEN_WIFI_DIALOG (dialog), wifi_device);
  g_signal_connect_object (dialog, "response",
                           G_CALLBACK (hidden_wireless_dialog_response_cb),
                           client, 0);
  gtk_dialog_run (GTK_DIALOG (dialog));
}

