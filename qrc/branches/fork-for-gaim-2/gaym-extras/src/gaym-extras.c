
#include "gaym-extras.h"
/* Show icons in chat room windows */

void get_icon_scale_size(GdkPixbuf * icon, GaimBuddyIconSpec * spec,
                         int *width, int *height)
{
    g_return_if_fail(icon != NULL);
    *width = gdk_pixbuf_get_width(icon);
    *height = gdk_pixbuf_get_height(icon);
    // gaim_debug_misc("popups", "current: w: %i, h: %i\n", *width,
    // *height);
    /* this should eventually get smarter about preserving the aspect
       ratio when scaling, but gimmie a break, I just woke up */
    if (spec && spec->scale_rules & GAIM_ICON_SCALE_DISPLAY) {
        float spec_aspect =
            (float) spec->max_width / (float) spec->max_height;
        float icon_aspect = (float) (*width) / (float) (*height);

        // icon will hit borders horizontally first
        if (icon_aspect > spec_aspect) {
            float width_ratio =
                (float) (*width) / (float) (spec->max_width);
            *height = (float) (*height) / width_ratio;
            *width = spec->max_width;
        }
        if (icon_aspect < spec_aspect) {
            float height_ratio =
                (float) (*height) / (float) (spec->max_height);
            *width = (float) (*width) / height_ratio;
            *height = spec->max_height;
        }



    }

    /* and now for some arbitrary sanity checks */
    if (*width > 100)
        *width = 100;
    if (*height > 100)
        *height = 100;
    // gaim_debug_misc("popups", "scaled: w: %i, h: %i\n", *width,
    // *height);
}

// Adds motion handlers to IM tab labels.

static void redo_im_window(GaimConversation * c)
{
    gaim_debug_misc("chaticon", "GOT CONVERSATION CREATED FOR %s\n",
                    c->name);
    if (!g_strrstr(gaim_account_get_protocol_id(c->account), "prpl-gaym"))
        return;
    if (c && c->type == GAIM_CONV_TYPE_IM)
        add_im_popup_stuff(c);
    else if (c->type == GAIM_CONV_TYPE_CHAT) {
        add_chat_sort_functions(c);
        add_chat_popup_stuff(c);
        add_chat_icon_stuff(c);
    }

}


static void update_info_cb(GaimAccount * account, char *name)
{
    if (!g_strrstr(gaim_account_get_protocol_id(account), "prpl-gaym"))
        return;
    gaim_debug_misc("gaym-extras", "info update\n");
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

GdkPixbuf *lookup_cached_thumbnail(GaimAccount * account,
                                   const char *fullname)
{
    guint len;
    GaimBuddyIcon *icon = gaim_buddy_icons_find(account, fullname);
    if (!icon) {
        gaim_debug_misc("gaym-extras", "No icon found for %s\n", fullname);
        return NULL;
    }
    const guchar *icon_bytes = gaim_buddy_icon_get_data(icon, &len);
    if (!icon_bytes) {
        gaim_debug_misc("gaym-extras", "No icon data found for %s\n",
                        fullname);
        return NULL;
    }

    GError *err = NULL;
    GdkPixbufLoader *loader = gdk_pixbuf_loader_new();
    if (!gdk_pixbuf_loader_write(loader, icon_bytes, len, &err))
        gaim_debug_misc("roombrowse", "write error: %s\n", err->message);
    else
        gaim_debug_misc("roombrowse", "write %d bytes without errors.\n",
                        len);
    GdkPixbuf *pixbuf = gdk_pixbuf_loader_get_pixbuf(loader);
    GdkPixbufFormat *format = gdk_pixbuf_loader_get_format(loader);
    if (format) {
        gaim_debug_misc("gaym-extras", "pixbuf name: %s\n",
                        gdk_pixbuf_format_get_name(format));
        // gaim_debug_misc("gaym-extras","pixbuf domain:
        // %s\n",gdk_pixbuf_format_get_domain(format));
        gaim_debug_misc("gaym-extras", "pixbuf description: %s\n",
                        gdk_pixbuf_format_get_description(format));
        int i = 0;
        gchar **mime_types = gdk_pixbuf_format_get_mime_types(format);
        gchar **extensions = gdk_pixbuf_format_get_extensions(format);
        while (mime_types[i] != NULL)
            gaim_debug_misc("gaym-extras", "pixbuf mime_type: %s\n",
                            mime_types[i++]);
        i = 0;
        while (extensions[i] != NULL)
            gaim_debug_misc("gaym-extras", "pixbuf extensions: %s\n",
                            extensions[i++]);


    }
    gdk_pixbuf_loader_close(loader, NULL);

    return pixbuf;
}

static gboolean plugin_unload(GaimPlugin * plugin)
{

    /* Ok, this is hell. I need to: Remove any icons from the IM windows.
       Disconnect signals Close and destroy roombrowsers/memory associated 
       with Destroy all popups Remove chaticon buttons */
    return TRUE;

}
static gboolean plugin_load(GaimPlugin * plugin)
{
    init_chat_icons(plugin);
    init_popups();
    init_roombrowse(plugin);
    gaim_debug_misc("gaym-extras", "gaim_conversations_get_handle(): %x\n",
                    gaim_conversations_get_handle());
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
        gaim_plugin_pref_new_with_name_and_label
        ("/plugins/gaym-extras/silly",
         _("Do you really want to turn any of this off? ;-)"));
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
    plugin_unload,
    NULL,
    NULL,
    &prefs_info,
    NULL
};

static void init_plugin(GaimPlugin * plugin)
{
}

GAIM_INIT_PLUGIN(history, init_plugin, info)
