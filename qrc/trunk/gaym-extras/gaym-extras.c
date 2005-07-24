/* Show icons in chat room windows */

#include "internal.h"
#include "gtkgaim.h"

#include "conversation.h"
#include "debug.h"
#include "log.h"
#include "prefs.h"
#include "signals.h"
#include "util.h"
#include "version.h"
#include "buddyicon.h"

#include "gtkconv.h"
#include "gtkimhtml.h"
#include "gtkplugin.h"

#include "../gaym/src/gaym.h"
#include "../gaym/src/gayminfo.h"

#define CHATSORT_PLUGIN_ID "gtk-chaticon"

GHashTable *icons;
GHashTable *icon_spots;
GHashTable *pending_updates;
GaimConvWindow *hider_window;

typedef struct _GaymChatIcon {

    GaimConversation *conv;
    GtkWidget *icon_container_parent;
    GtkWidget *icon_container;
    GtkWidget *frame;
    GtkWidget *icon;
    GtkWidget *event;
    gboolean show_icon;
    GdkPixbufAnimation *anim;
    GdkPixbufAnimationIter *iter;
    gboolean animate;
    guint32 icon_timer;

} GaymChatIcon;



void
gaym_gtkconv_update_thumbnail(GaimConversation * conv,
                              struct gaym_fetch_thumbnail_data
                              *thumbnail_data)
{
    GaimGtkConversation *gtkconv;

    char filename[256];
    FILE *file;
    GError *err = NULL;

    size_t len;

    GdkPixbuf *buf;

    GdkPixmap *pm;
    GdkBitmap *bm;

    GaimAccount *account;
    GaimPluginProtocolInfo *prpl_info = NULL;

    g_return_if_fail(conv != NULL);
    g_return_if_fail(GAIM_IS_GTK_CONVERSATION(conv));
    g_return_if_fail(gaim_conversation_get_type(conv) == GAIM_CONV_CHAT);

    gtkconv = GAIM_GTK_CONVERSATION(conv);

    GaymChatIcon *icon_data = g_hash_table_lookup(icons, conv);

    if (!thumbnail_data)
        return;
    if (!icon_data->show_icon)
        return;

    const char *data = thumbnail_data->pic_data;
    len = thumbnail_data->pic_data_len;

    account = gaim_conversation_get_account(conv);
    if (account && account->gc)
        prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(account->gc->prpl);


    if (icon_data->anim != NULL)
        g_object_unref(G_OBJECT(icon_data->anim));

    icon_data->anim = NULL;

    if (icon_data->icon_timer != 0)
        g_source_remove(icon_data->icon_timer);

    icon_data->icon_timer = 0;

    if (icon_data->iter != NULL)
        g_object_unref(G_OBJECT(icon_data->iter));

    icon_data->iter = NULL;

    if (!gaim_prefs_get_bool
        ("/gaim/gtk/conversations/im/show_buddy_icons"))
        return;

    if (gaim_conversation_get_gc(conv) == NULL)
        return;


    /* this is such an evil hack, i don't know why i'm even considering
       it. we'll do it differently when gdk-pixbuf-loader isn't leaky
       anymore. */
    /* gdk-pixbuf-loader was leaky? is it still? */

    g_snprintf(filename, sizeof(filename),
               "%s" G_DIR_SEPARATOR_S "gaimicon-%s.%d",
               g_get_tmp_dir(), thumbnail_data->who, getpid());

    if (!(file = g_fopen(filename, "wb")))
        return;

    fwrite(data, 1, len, file);
    fclose(file);
    icon_data->anim = gdk_pixbuf_animation_new_from_file(filename, &err);

    /* make sure we remove the file as soon as possible */
    g_unlink(filename);

    if (err) {
        gaim_debug(GAIM_DEBUG_ERROR, "gtkconv",
                   "Buddy icon error: %s\n", err->message);
        g_error_free(err);
    }



    if (!icon_data->anim)
        return;

    if (gdk_pixbuf_animation_is_static_image(icon_data->anim)) {
        icon_data->iter = NULL;
        buf = gdk_pixbuf_animation_get_static_image(icon_data->anim);
    } else {
        icon_data->iter = gdk_pixbuf_animation_get_iter(icon_data->anim, NULL); /* LEAK 
                                                                                 */
        buf = gdk_pixbuf_animation_iter_get_pixbuf(icon_data->iter);
    }


    gdk_pixbuf_render_pixmap_and_mask(buf, &pm, &bm, 100);
    // g_object_unref(G_OBJECT(buf));


    icon_data->event = gtk_event_box_new();
    gtk_container_add(GTK_CONTAINER(icon_data->frame), icon_data->event);
    // g_signal_connect(G_OBJECT(icon_data->event), "button-press-event",
    // G_CALLBACK(icon_menu), conv);
    gtk_widget_show(icon_data->event);
    icon_data->icon = gtk_image_new_from_pixmap(pm, bm);
    gtk_container_add(GTK_CONTAINER(icon_data->event), icon_data->icon);
    gtk_widget_show(icon_data->icon);

    g_object_unref(G_OBJECT(pm));

    if (bm)
        g_object_unref(G_OBJECT(bm));

    buf = gdk_pixbuf_animation_get_static_image(icon_data->anim);

}
static gboolean check_for_update(gpointer * conversation,
                                 const gpointer * name, gpointer * data)
{

    g_return_val_if_fail(conversation != NULL, TRUE);
    g_return_val_if_fail(name != NULL, TRUE);
    g_return_val_if_fail(data != NULL, TRUE);

    gaim_debug_misc("chaticon", "check for update: %x\n", conversation);
    GaimConversation *c = (GaimConversation *) conversation;
    char *name_needing_update = (char *) name;

    struct gaym_fetch_thumbnail_data *d =
        (struct gaym_fetch_thumbnail_data *) data;

    gaim_debug_misc("chaticon",
                    "Conversation: %x, name %s\n", c, name_needing_update);

    g_return_val_if_fail(name_needing_update != NULL, FALSE);

    if (!strcmp(d->who, name_needing_update)) {
        gaym_gtkconv_update_thumbnail(c, d);
        return TRUE;
    }
    return TRUE;
}

void fetch_thumbnail_cb(void *user_data, const char *pic_data, size_t len)
{
    if (!user_data)
        return;
    struct gaym_fetch_thumbnail_data *d = user_data;
    if (!pic_data) {
        return;
    }
    gaim_debug_misc("gaym", "got pic data: %s\n", pic_data);
    if (len && !g_strrstr_len(pic_data, len, "Server Error")) {
        d->pic_data = pic_data;
        d->pic_data_len = len;
    } else {
        d->pic_data = 0;
        d->pic_data_len = 0;
        gaim_debug_error("gaym", "Fetching thumbnail failed.\n");
    }
    g_hash_table_foreach_remove(pending_updates,
                                (GHRFunc) check_for_update, d);
    g_free(d->who);
    g_free(d);
}


static void changed_cb(GtkTreeSelection * selection, gpointer conv)
{

    GaimConversation *c = (GaimConversation *) conv;
    GaymChannelMember *cm;
    struct gaym_conn *gaym = c->account->gc->proto_data;
    GtkTreeIter iter;
    GtkTreeModel *model;
    gchar *name;

    gtk_tree_selection_get_selected(selection, &model, &iter);
    gtk_tree_model_get(model, &iter, CHAT_USERS_NAME_COLUMN, &name, -1);

    gaim_debug_misc("chatsort", "Click: %s\n", name);
    gtk_button_set_label(g_hash_table_lookup(icon_spots, c), name);


    /* Remove the current icon stuff */
    GaymChatIcon *icon_data = g_hash_table_lookup(icons, c);
    if (icon_data->event != NULL)
        gtk_widget_destroy(icon_data->event);
    icon_data->event = NULL;

    // Get thumbnail URL.
    cm = gaym_get_channel_member_info(gaym, name);
    gaim_debug_misc("chaticon", "got thumbnail url %s for %s\n",
                    cm->thumbnail, name);

    // Fetch thumbnail.

    struct gaym_fetch_thumbnail_data *data;
    char *hashurl = g_hash_table_lookup(gaym->confighash,
                                        "mini-profile-panel.thumbnail-prefix");
    g_return_if_fail(hashurl != NULL);
    data = g_new0(struct gaym_fetch_thumbnail_data, 1);
    data->gc = gaim_account_get_connection(gaym->account);
    data->who = g_strdup(name);
    char *url = g_strdup_printf("%s%s", hashurl, cm->thumbnail);
    gaim_url_fetch(url, FALSE, "Mozilla/4.0", FALSE,
                   fetch_thumbnail_cb, data);

    // Add entry to hash table for tracking.
    g_hash_table_replace(pending_updates, c, name);

}

static void redochatwindow(GaimConversation * c)
{

    GtkTreeModel *oldls;

    GaimGtkConversation *gtkconv = GAIM_GTK_CONVERSATION(c);
    GaimGtkChatPane *gtkchat = gtkconv->u.chat;
    GaimPluginProtocolInfo *prpl_info = NULL;
    GaimAccount *account = gaim_conversation_get_account(c);
    if (account && account->gc)
        prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(account->gc->prpl);
    GtkTreeSelection *select =
        gtk_tree_view_get_selection(GTK_TREE_VIEW(gtkchat->list));
    gtk_tree_selection_set_mode(select, GTK_SELECTION_SINGLE);

    oldls = gtk_tree_view_get_model(GTK_TREE_VIEW(gtkchat->list));
    GtkBox *vbox = GTK_BOX(gtkchat->list->parent->parent);

    GtkBox *hbox = GTK_BOX(gtkconv->lower_hbox);

    GtkWidget *button = gtk_button_new_with_label("A Button");
    gtk_box_pack_end(vbox, GTK_WIDGET(button), FALSE, FALSE, 0);
    gtk_widget_show(button);


    g_signal_connect(G_OBJECT(select), "changed", G_CALLBACK(changed_cb),
                     c);

    GaymChatIcon *icon_data = g_new0(GaymChatIcon, 1);

    icon_data->icon_container_parent = GTK_WIDGET(hbox);
    icon_data->icon_container = NULL;
    icon_data->icon = NULL;
    icon_data->anim = NULL;
    icon_data->iter = NULL;
    icon_data->show_icon = TRUE;

    icon_data->icon_container = gtk_vbox_new(FALSE, 0);
    gtk_widget_set_size_request(GTK_WIDGET(icon_data->icon_container),
                                prpl_info->icon_spec.max_width,
                                prpl_info->icon_spec.max_height);


    icon_data->frame = gtk_frame_new(NULL);
    gtk_frame_set_shadow_type(GTK_FRAME(icon_data->frame),
                              (GTK_SHADOW_IN));
    gtk_box_pack_start(GTK_BOX(icon_data->icon_container),
                       icon_data->frame, FALSE, FALSE, 0);
    gtk_widget_show(icon_data->icon_container);
    gtk_widget_show(icon_data->frame);
    gtk_box_pack_end(GTK_BOX(icon_data->icon_container_parent),
                     icon_data->icon_container, FALSE, FALSE, 0);
    gtk_widget_set_size_request(GTK_WIDGET(icon_data->frame),
                                prpl_info->icon_spec.max_width,
                                prpl_info->icon_spec.max_height);
    gaim_debug_misc("chaticon", "made container\n");

    icon_data->event = gtk_event_box_new();
    gtk_container_add(GTK_CONTAINER(icon_data->frame), icon_data->event);
    // g_signal_connect(G_OBJECT(icon_data->event), "button-press-event",
    // G_CALLBACK(icon_menu), conv);
    gtk_widget_show(icon_data->event);
    g_hash_table_insert(icon_spots, c, button);
    g_hash_table_insert(icons, c, icon_data);

}

static gboolean plugin_load(GaimPlugin * plugin)
{
    icons = g_hash_table_new(g_direct_hash, g_direct_equal);
    icon_spots = g_hash_table_new(g_direct_hash, g_direct_equal);
    pending_updates = g_hash_table_new(g_direct_hash, g_direct_equal);

    gaim_signal_connect(gaim_conversations_get_handle(), "chat-joined",
                        plugin, GAIM_CALLBACK(redochatwindow), NULL);

    return TRUE;
}

static GaimPluginInfo info = {
    GAIM_PLUGIN_MAGIC,
    GAIM_MAJOR_VERSION,
    GAIM_MINOR_VERSION,
    GAIM_PLUGIN_STANDARD,
    GAIM_GTK_PLUGIN_TYPE,
    0,
    NULL,
    GAIM_PRIORITY_DEFAULT,
    CHATSORT_PLUGIN_ID,
    N_("Gaym Extras"),
    VERSION,
    N_("GUI-related additions for the gaym protocol plugin."),
    N_("Allow modifications of namelist sorting order in "),
    "Jason LeBrun <gaim@jasonlebrun.info",
    GAIM_WEBSITE,
    plugin_load,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
};

static void init_plugin(GaimPlugin * plugin)
{
}

GAIM_INIT_PLUGIN(history, init_plugin, info)
