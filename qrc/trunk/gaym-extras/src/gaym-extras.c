/* Show icons in chat room windows */

//Messy.
#include "gaym-extras.h"
#ifdef _WIN32
#include "win32/win32dep.h"
#else
#define DATADIR GAIM_DATADIR
#endif
// Adds motion handlers to IM tab labels.
static void redo_im_window(GaimConversation * c)
{
    if (!g_strrstr(gaim_account_get_protocol_id(c->account), "prpl-gaym"))
        return;
    if (c && c->type == GAIM_CONV_IM)
        add_im_popup_stuff(c);
}


static void update_info_cb(GaimAccount * account, char *name)
{
    if (!g_strrstr(gaim_account_get_protocol_id(account), "prpl-gaym"))
        return;
    gaim_debug_misc("gaym-extras", "info update\n");
}

static void redochatwindow(GaimConversation * c)
{
    if (!g_strrstr(gaim_account_get_protocol_id(c->account), "prpl-gaym"))
        return;
    add_chat_sort_functions(c);
    add_chat_popup_stuff(c);
    add_chat_icon_stuff(c);
}
static gchar *find_file(const char *dir, const char *base)
{
    char *filename;

    if (base == NULL)
        return NULL;

    if (!strcmp(dir, "gaim"))
        filename =
            g_build_filename(DATADIR, "pixmaps", "gaim", base, NULL);
    else {
        filename = g_build_filename(DATADIR, "pixmaps", "gaim", dir,
                                    base, NULL);
    }

    if (!g_file_test(filename, G_FILE_TEST_EXISTS)) {
        g_critical("Unable to load stock pixmap %s\n", filename);

        g_free(filename);

        return NULL;
    }

    return filename;

}

void extras_register_stock()
{

    static gboolean stock_is_init = FALSE;
    GtkIconFactory *icon_factory = NULL;
    int i;
    if (stock_is_init)
        return;
    stock_is_init = TRUE;
    icon_factory = gtk_icon_factory_new();

    gtk_icon_factory_add_default(icon_factory);

    for (i = 0; i < G_N_ELEMENTS(stock_icons); i++) {
        GdkPixbuf *pixbuf;
        GtkIconSet *iconset;
        gchar *filename;
        filename = find_file(stock_icons[i].dir, stock_icons[i].filename);
        if (filename == NULL)
            continue;

        pixbuf = gdk_pixbuf_new_from_file(filename, NULL);
        g_free(filename);
        iconset = gtk_icon_set_new_from_pixbuf(pixbuf);

        g_object_unref(pixbuf);
        gtk_icon_factory_add(icon_factory, stock_icons[i].name, iconset);
        gtk_icon_set_unref(iconset);
    }


}
static gboolean plugin_load(GaimPlugin * plugin)
{
    init_chat_icons(plugin);
    init_popups();
    init_roombrowse(plugin);
    gaim_signal_connect(gaim_conversations_get_handle(), "chat-joined",
                        plugin, GAIM_CALLBACK(redochatwindow), NULL);

    gaim_signal_connect(gaim_conversations_get_handle(),
                        "conversation-created", plugin,
                        GAIM_CALLBACK(redo_im_window), NULL);

    gaim_signal_connect(gaim_accounts_get_handle(), "info-updated", plugin,
                        GAIM_CALLBACK(update_info_cb), NULL);

    gaim_signal_connect(gaim_conversations_get_handle(),
                        "deleting-conversation", plugin,
                        GAIM_CALLBACK(clean_popup_stuff), NULL);
    gaim_prefs_add_none("/plugins/gaym-extras");
    gaim_prefs_add_none("/plugins/gaym-extras/silly");

    extras_register_stock();
    
    return TRUE;
}

static GaimPluginPrefFrame *get_plugin_pref_frame(GaimPlugin * plugin)
{
    GaimPluginPrefFrame *frame;
    GaimPluginPref *ppref;

    frame = gaim_plugin_pref_frame_new();

    ppref =
        gaim_plugin_pref_new_with_name_and_label("/plugins/gaym-extras/silly",_("Do you really want to turn any of this off? ;-)"));
    gaim_plugin_pref_frame_add(frame, ppref);

      return frame;
}
static GaimPluginUiInfo prefs_info = {
    get_plugin_pref_frame
};

static GaimPluginInfo info = {
    GAIM_PLUGIN_MAGIC,
    GAIM_MAJOR_VERSION,
    GAIM_MINOR_VERSION,
    GAIM_PLUGIN_STANDARD,
    GAIM_GTK_PLUGIN_TYPE,
    0,
    NULL,
    GAIM_PRIORITY_DEFAULT,
    GAYM_EXTRAS_PLUGIN_ID,
    N_("Gaym Extras"),
    VERSION,
    N_("GUI-related additions for the gaym protocol plugin."),
    N_("Current functionality provided by this plugin:\n1. Allows namelist sort order in rooms to be changed.\n2. Shows thumbnails for currently selected user in rooms.\n3. Popup displays bio when you hover over a name in the namelist.\n4. Popup shows bio when you hover over an IM tab."),
    "Jason LeBrun gaym@jasonlebrun.info",
    GAIM_WEBSITE,
    plugin_load,
    NULL,
    NULL,
    NULL,
    &prefs_info,
    NULL
};

static void init_plugin(GaimPlugin * plugin)
{
}

GAIM_INIT_PLUGIN(history, init_plugin, info)
