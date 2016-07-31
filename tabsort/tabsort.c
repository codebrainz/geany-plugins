#include <geanyplugin.h>
#include <string.h>

enum
{
  SORT_TABS_BY_BASENAME,
  SORT_TABS_BY_PATHNAME,
  SORT_TABS_BY_BASENAME_REVERSE,
  SORT_TABS_BY_PATHNAME_REVERSE,
  NUM_KEY_EVENTS
};

struct State
{
  /* settings */
  gint mode;
  gboolean autoSort;

  /* widgets added at runtime */
  GtkWidget *sepItem;
  GtkWidget *menuItem;
  GtkWidget *sortTabsByBaseName;
  GtkWidget *sortTabsByPathName;
  GtkWidget *sortTabsByBaseNameReverse;
  GtkWidget *sortTabsByPathNameReverse;

  /* widgets from Glade file */
  GtkWidget *cfgPanel;
  GtkWidget *radioSortByBaseName;
  GtkWidget *radioSortByPathName;
  GtkWidget *radioSortByBaseNameReverse;
  GtkWidget *radioSortByPathNameReverse;
  GtkWidget *checkSortAutomatically;
  GtkWidget *lblSortMode;
  GtkWidget *alignSortMode;
};

struct TabData
{
  gchar *fn;
  GtkWidget *tab;
};

GeanyPlugin *geany_plugin;
GeanyData *geany_data;
static struct State state = { 0 };

static gint
compareTabs (gconstpointer a, gconstpointer b)
{
  const struct TabData *tabA = *((struct TabData **) a);
  const struct TabData *tabB = *((struct TabData **) b);
  return strcmp (tabA->fn, tabB->fn);
}

static gint
compareTabsReverse (gconstpointer a, gconstpointer b)
{
  return -compareTabs (a, b);
}

static void
deleteTabData (gpointer data)
{
  struct TabData *tabData = data;
  g_free (tabData->fn);
  g_slice_free (struct TabData, tabData);
}

static void
sortTabs (gint mode)
{
  GtkNotebook *nb = GTK_NOTEBOOK (geany_data->main_widgets->notebook);
  guint i;
  const guint n = gtk_notebook_get_n_pages (nb);
  GPtrArray *arr = g_ptr_array_new_full (n, deleteTabData);
  gint oldMode = state.mode;
  state.mode = mode;

  for (i = 0; i < n; i++)
    {
      struct TabData *tabData = g_slice_new0 (struct TabData);
      GeanyDocument *doc = document_get_from_page (i);
      gchar *fn;
      if (state.mode == SORT_TABS_BY_BASENAME ||
          state.mode == SORT_TABS_BY_BASENAME_REVERSE)
        {
          if (doc->file_name != NULL)
            fn = g_path_get_basename (doc->file_name);
          else
            fn = document_get_basename_for_display (doc, -1);
        }
      else
        {
          if (doc->file_name != NULL)
            fn = g_strdup (doc->file_name);
          else
            fn = document_get_basename_for_display (doc, -1);
        }
      tabData->fn = g_utf8_collate_key_for_filename (fn, -1);
      g_free (fn);
      tabData->tab = gtk_notebook_get_nth_page (nb, i);
      g_ptr_array_add (arr, tabData);
    }

  if (state.mode == SORT_TABS_BY_BASENAME_REVERSE ||
      state.mode == SORT_TABS_BY_PATHNAME_REVERSE)
    {
      g_ptr_array_sort (arr, (GCompareFunc) compareTabsReverse);
    }
  else
    {
      g_ptr_array_sort (arr, (GCompareFunc) compareTabs);
    }

  for (i = 0; i < n; i++)
    {
      struct TabData *tabData = g_ptr_array_index (arr, i);
      gtk_notebook_reorder_child (nb, tabData->tab, i);
    }

  g_ptr_array_free (arr, TRUE);
  state.mode = oldMode;
}

static void
onKeyEvent (guint id)
{
  guint cmd = id;
  switch (cmd)
    {
      case SORT_TABS_BY_BASENAME:
      case SORT_TABS_BY_PATHNAME:
      case SORT_TABS_BY_BASENAME_REVERSE:
      case SORT_TABS_BY_PATHNAME_REVERSE:
        sortTabs (cmd);
        break;
    }
}

static void
setTabsReorderable (gboolean reorderable)
{
  guint i, n;
  GtkNotebook *nb = GTK_NOTEBOOK (geany_data->main_widgets->notebook);
  n = gtk_notebook_get_n_pages (nb);
  for (i = 0; i < n; i++)
    {
      gtk_notebook_set_tab_reorderable (
        nb, gtk_notebook_get_nth_page (nb, i), reorderable);
    }
}

static void
loadConfig (void)
{
  GKeyFile *kf;
  gchar *confFile;
  GError *error = NULL;
  gint sortMode = state.mode;
  gboolean autoSort = state.autoSort;

  confFile = g_build_filename (
    geany_data->app->configdir, "plugins", "tabsort.conf", NULL);

  if (!g_file_test (confFile, G_FILE_TEST_EXISTS))
    {
      g_free (confFile);
      return;
    }

  kf = g_key_file_new ();

  if (!g_key_file_load_from_file (
        kf,
        confFile,
        G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS,
        &error))
    {
      g_warning (
        _ ("failed to load config file '%s': %s"), confFile, error->message);
      g_error_free (error);
      g_free (confFile);
      g_key_file_free (kf);
      return;
    }

  g_free (confFile);

  sortMode = g_key_file_get_integer (kf, "tabSort", "sortMode", &error);
  if (error != NULL)
    {
      g_debug (_ ("unable to read 'sortMode' key: %s"), error->message);
      g_error_free (error);
      error = NULL;
    }
  else
    state.mode = sortMode;

  autoSort = g_key_file_get_boolean (kf, "tabSort", "autoSort", &error);
  if (error != NULL)
    {
      g_debug (_ ("unable to read 'autoSort' key: %s"), error->message);
      g_error_free (error);
      error = NULL;
    }
  else
    state.autoSort = autoSort;

  g_key_file_free (kf);
}

static void
saveConfig (void)
{
  GKeyFile *kf;
  gchar *confFile;
  GError *error = NULL;

  kf = g_key_file_new ();

  g_key_file_set_integer (kf, "tabSort", "sortMode", state.mode);
  g_key_file_set_boolean (kf, "tabSort", "autoSort", state.autoSort);

  confFile = g_build_filename (
    geany_data->app->configdir, "plugins", "tabsort.conf", NULL);

  if (!g_key_file_save_to_file (kf, confFile, &error))
    {
      g_warning (
        _ ("failed to save config file '%s': %s"), confFile, error->message);
      g_error_free (error);
      g_free (confFile);
      g_key_file_free (kf);
      return;
    }

  g_free (confFile);
  g_key_file_free (kf);
}

static void
loadConfigUI (void)
{
  switch (state.mode)
    {
      case SORT_TABS_BY_BASENAME:
        gtk_toggle_button_set_active (
          GTK_TOGGLE_BUTTON (state.radioSortByBaseName), TRUE);
        break;
      case SORT_TABS_BY_PATHNAME:
        gtk_toggle_button_set_active (
          GTK_TOGGLE_BUTTON (state.radioSortByPathName), TRUE);
        break;
      case SORT_TABS_BY_BASENAME_REVERSE:
        gtk_toggle_button_set_active (
          GTK_TOGGLE_BUTTON (state.radioSortByBaseNameReverse), TRUE);
        break;
      case SORT_TABS_BY_PATHNAME_REVERSE:
        gtk_toggle_button_set_active (
          GTK_TOGGLE_BUTTON (state.radioSortByPathNameReverse), TRUE);
        break;
    }

  if (state.autoSort)
    {
      gtk_toggle_button_set_active (
        GTK_TOGGLE_BUTTON (state.checkSortAutomatically), state.autoSort);
    }
}

static void
storeConfigUI (void)
{
  if (gtk_toggle_button_get_active (
        GTK_TOGGLE_BUTTON (state.radioSortByBaseName)))
    {
      state.mode = SORT_TABS_BY_BASENAME;
    }
  else if (gtk_toggle_button_get_active (
             GTK_TOGGLE_BUTTON (state.radioSortByPathName)))
    {
      state.mode = SORT_TABS_BY_PATHNAME;
    }
  else if (gtk_toggle_button_get_active (
             GTK_TOGGLE_BUTTON (state.radioSortByBaseNameReverse)))
    {
      state.mode = SORT_TABS_BY_BASENAME_REVERSE;
    }
  else if (gtk_toggle_button_get_active (
             GTK_TOGGLE_BUTTON (state.radioSortByPathNameReverse)))
    {
      state.mode = SORT_TABS_BY_PATHNAME_REVERSE;
    }
  state.autoSort = gtk_toggle_button_get_active (
    GTK_TOGGLE_BUTTON (state.checkSortAutomatically));
  setTabsReorderable (!state.autoSort);
}

static void
onMenuItemActivated (GtkMenuItem *item, gpointer userData)
{
  sortTabs (GPOINTER_TO_INT (userData));
}

static void
createMenu (void)
{
  GtkWidget *menu;

  state.sepItem = gtk_separator_menu_item_new ();
  gtk_widget_show_all (state.sepItem);

  state.menuItem =
    gtk_image_menu_item_new_from_stock (GTK_STOCK_SORT_ASCENDING, NULL);
  gtk_menu_item_set_label (GTK_MENU_ITEM (state.menuItem), _ ("Sort Tabs"));
  menu = gtk_menu_new ();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (state.menuItem), menu);
  gtk_widget_show_all (state.menuItem);

  state.sortTabsByBaseName = gtk_menu_item_new_with_label (_ ("By Basename"));
  g_signal_connect (state.sortTabsByBaseName,
                    "activate",
                    G_CALLBACK (onMenuItemActivated),
                    GINT_TO_POINTER (SORT_TABS_BY_BASENAME));
  gtk_widget_show (state.sortTabsByBaseName);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), state.sortTabsByBaseName);

  state.sortTabsByPathName = gtk_menu_item_new_with_label (_ ("By Pathname"));
  g_signal_connect (state.sortTabsByPathName,
                    "activate",
                    G_CALLBACK (onMenuItemActivated),
                    GINT_TO_POINTER (SORT_TABS_BY_PATHNAME));
  gtk_widget_show (state.sortTabsByPathName);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), state.sortTabsByPathName);

  state.sortTabsByBaseNameReverse =
    gtk_menu_item_new_with_label (_ ("By Basename Reversed"));
  g_signal_connect (state.sortTabsByBaseNameReverse,
                    "activate",
                    G_CALLBACK (onMenuItemActivated),
                    GINT_TO_POINTER (SORT_TABS_BY_BASENAME_REVERSE));
  gtk_widget_show (state.sortTabsByBaseNameReverse);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu),
                         state.sortTabsByBaseNameReverse);

  state.sortTabsByPathNameReverse =
    gtk_menu_item_new_with_label (_ ("By Pathname Reversed"));
  g_signal_connect (state.sortTabsByPathNameReverse,
                    "activate",
                    G_CALLBACK (onMenuItemActivated),
                    GINT_TO_POINTER (SORT_TABS_BY_PATHNAME_REVERSE));
  gtk_widget_show (state.sortTabsByPathNameReverse);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu),
                         state.sortTabsByPathNameReverse);

  menu = ui_lookup_widget (geany_data->main_widgets->window, "menu_view1_menu");
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), state.sepItem);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), state.menuItem);
}

static void
onCheckSortAutomatically (GtkToggleButton *chkButton, gpointer userData)
{
  gboolean checked = gtk_toggle_button_get_active (chkButton);
  if (checked)
    {
      gtk_widget_set_sensitive (state.lblSortMode, TRUE);
      gtk_widget_set_sensitive (state.alignSortMode, TRUE);
      gtk_widget_set_visible (state.sepItem, FALSE);
      gtk_widget_set_visible (state.menuItem, FALSE);
    }
  else
    {
      gtk_widget_set_sensitive (state.lblSortMode, FALSE);
      gtk_widget_set_sensitive (state.alignSortMode, FALSE);
      gtk_widget_set_visible (state.sepItem, TRUE);
      gtk_widget_set_visible (state.menuItem, TRUE);
    }
}

static void
createPrefsPanel (void)
{
  GError *error = NULL;
  GtkBuilder *builder = gtk_builder_new ();
  gchar *uiFile = g_build_filename (DATADIR, "ui.glade", NULL);

  if (gtk_builder_add_from_file (builder, uiFile, &error) == 0)
    {
      g_warning (_ ("failed to load UI file '%s': %s"), uiFile, error->message);
      g_error_free (error);
      g_free (uiFile);
      g_object_unref (builder);
      return;
    }
  g_free (uiFile);

  state.cfgPanel = g_object_ref (gtk_builder_get_object (builder, "boxMain"));
  state.radioSortByBaseName =
    GTK_WIDGET (gtk_builder_get_object (builder, "radioSortByBaseName"));
  state.radioSortByPathName =
    GTK_WIDGET (gtk_builder_get_object (builder, "radioSortByFullPathName"));
  state.radioSortByBaseNameReverse = GTK_WIDGET (
    gtk_builder_get_object (builder, "radioSortByBaseNameReversed"));
  state.radioSortByPathNameReverse = GTK_WIDGET (
    gtk_builder_get_object (builder, "radioSortByFullPathNameReversed"));
  state.checkSortAutomatically =
    GTK_WIDGET (gtk_builder_get_object (builder, "checkSortTabsAutomatically"));
  state.lblSortMode =
    GTK_WIDGET (gtk_builder_get_object (builder, "lblSortingMode"));
  state.alignSortMode =
    GTK_WIDGET (gtk_builder_get_object (builder, "alignSortingMode"));

  g_signal_connect (state.checkSortAutomatically,
                    "toggled",
                    G_CALLBACK (onCheckSortAutomatically),
                    NULL);

  onCheckSortAutomatically (GTK_TOGGLE_BUTTON (state.checkSortAutomatically),
                            NULL);

  g_object_unref (builder);
}

static void
deletePrefsPanel (void)
{
  if (GTK_IS_WIDGET (state.cfgPanel))
    gtk_widget_destroy (state.cfgPanel);

  state.cfgPanel = NULL;
  state.radioSortByBaseName = NULL;
  state.radioSortByPathName = NULL;
  state.radioSortByBaseNameReverse = NULL;
  state.radioSortByPathNameReverse = NULL;
  state.checkSortAutomatically = NULL;
}

static void
onDocumentsChanged (GObject *obj, GeanyDocument *doc, gpointer userData)
{
  if (state.autoSort)
    sortTabs (state.mode);
}

static void
onGeanyStartupComplete (GObject *obj, gpointer userData)
{
  setTabsReorderable (!state.autoSort);
  onDocumentsChanged (NULL, NULL, NULL);
}

static gboolean
init (GeanyPlugin *plugin, gpointer pluginData)
{
  geany_plugin = plugin;
  geany_data = plugin->geany_data;

  memset (&state, 0, sizeof (struct State));

  state.mode = SORT_TABS_BY_BASENAME;
  state.autoSort = FALSE;
  state.cfgPanel = NULL;

  createMenu ();
  loadConfig ();

  plugin_signal_connect (geany_plugin,
                         NULL,
                         "document-close",
                         TRUE,
                         G_CALLBACK (onDocumentsChanged),
                         NULL);

  plugin_signal_connect (geany_plugin,
                         NULL,
                         "document-new",
                         TRUE,
                         G_CALLBACK (onDocumentsChanged),
                         NULL);

  plugin_signal_connect (geany_plugin,
                         NULL,
                         "document-open",
                         TRUE,
                         G_CALLBACK (onDocumentsChanged),
                         NULL);

  plugin_signal_connect (geany_plugin,
                         NULL,
                         "geany-startup-complete",
                         TRUE,
                         G_CALLBACK (onGeanyStartupComplete),
                         NULL);

  GeanyKeyGroup *group =
    plugin_set_key_group (geany_plugin, "tabsort", NUM_KEY_EVENTS, NULL);

  keybindings_set_item (group,
                        SORT_TABS_BY_BASENAME,
                        onKeyEvent,
                        0,
                        0,
                        "sortTabsByBaseName",
                        _ ("Sort Tabs by Basename"),
                        state.sortTabsByBaseName);

  keybindings_set_item (group,
                        SORT_TABS_BY_PATHNAME,
                        onKeyEvent,
                        0,
                        0,
                        "sortTabsByPathName",
                        _ ("Sort Tabs by Pathname"),
                        state.sortTabsByPathName);

  keybindings_set_item (group,
                        SORT_TABS_BY_BASENAME_REVERSE,
                        onKeyEvent,
                        0,
                        0,
                        "sortTabsByBaseNameReverse",
                        _ ("Sort Tabs by Basename Reversed"),
                        state.sortTabsByBaseNameReverse);

  keybindings_set_item (group,
                        SORT_TABS_BY_PATHNAME_REVERSE,
                        onKeyEvent,
                        0,
                        0,
                        "sortTabsByPathNameReverse",
                        _ ("Sort Tabs by Pathname Reversed"),
                        state.sortTabsByPathNameReverse);

  onDocumentsChanged (NULL, NULL, NULL);

  return TRUE;
}

static void
cleanup (GeanyPlugin *plugin, gpointer pluginData)
{
  setTabsReorderable (TRUE);
  deletePrefsPanel ();
  gtk_widget_destroy (state.sepItem);
  gtk_widget_destroy (state.sortTabsByBaseName);
  gtk_widget_destroy (state.sortTabsByPathName);
  gtk_widget_destroy (state.sortTabsByBaseNameReverse);
  gtk_widget_destroy (state.sortTabsByPathNameReverse);
}

static void
onConfigDialogResponse (GtkDialog *dialog, gint response, gpointer userData)
{
  switch (response)
    {
      case GTK_RESPONSE_OK:
        storeConfigUI ();
        saveConfig ();
        onDocumentsChanged (NULL, NULL, NULL);
        deletePrefsPanel ();
        break;
      case GTK_RESPONSE_APPLY:
        storeConfigUI ();
        saveConfig ();
        onDocumentsChanged (NULL, NULL, NULL);
        break;
      case GTK_RESPONSE_CANCEL:
      default:
        deletePrefsPanel ();
        break;
    }
}

static GtkWidget *
configure (GeanyPlugin *plugin, GtkDialog *dialog, gpointer userData)
{
  createPrefsPanel ();
  loadConfig ();
  loadConfigUI ();

  plugin_signal_connect (geany_plugin,
                         G_OBJECT (dialog),
                         "response",
                         TRUE,
                         G_CALLBACK (onConfigDialogResponse),
                         NULL);

  return state.cfgPanel;
}

G_MODULE_EXPORT void
geany_load_module (GeanyPlugin *plugin)
{
  plugin->info->name = _ ("Tab Sort");
  plugin->info->description = _ ("Allows sorting document tabs");
  plugin->info->version = "1.0";
  plugin->info->author = "Matthew Brush <matt@geany.org>";
  plugin->funcs->init = init;
  plugin->funcs->cleanup = cleanup;
  plugin->funcs->configure = configure;
  plugin->funcs->help = NULL;
  GEANY_PLUGIN_REGISTER (plugin, 228);
}
