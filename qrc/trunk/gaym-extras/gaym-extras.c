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
#define CHATSORT_PLUGIN_ID "gtk-chaticon"

GHashTable *icons;
GHashTable *icon_spots;
GHashTable *pending_updates;
GaimConvWindow *hider_window;

typedef struct _GaymChatIcon {

    GaimConversation *conv;
    GtkWidget *icon_container_parent;
    GtkWidget *icon_container;
    GtkWidget *icon;
    gboolean show_icon;
    GdkPixbufAnimation *anim;
    GdkPixbufAnimationIter *iter;
    gboolean animate;
    guint32 icon_timer;

} GaymChatIcon;

static void
get_icon_scale_size(GdkPixbufAnimation * icon, GaimBuddyIconSpec * spec,
                    int *width, int *height)
{
    *width = gdk_pixbuf_animation_get_width(icon);
    *height = gdk_pixbuf_animation_get_height(icon);

    /* this should eventually get smarter about preserving the aspect
       ratio when scaling, but gimmie a break, I just woke up */
    if (spec && spec->scale_rules & GAIM_ICON_SCALE_DISPLAY) {
        if (*width < spec->min_width)
            *width = spec->min_width;
        else if (*width > spec->max_width)
            *width = spec->max_width;

        if (*height < spec->min_height)
            *height = spec->min_height;
        else if (*height > spec->max_height)
            *height = spec->max_height;
    }

    /* and now for some arbitrary sanity checks */
    if (*width > 100)
        *width = 100;
    if (*height > 100)
        *height = 100;
}


void
gaym_gtkconv_update_buddy_icon(GaimConversation * conv,
                               GaimBuddyIcon * icon)
{
    GaimGtkConversation *gtkconv;
    // GaimGtkWindow *gtkwin =
    // GAIM_GTK_WINDOW(gaim_conversation_get_window(conv));

    char filename[256];
    FILE *file;
    GError *err = NULL;

    const void *data = NULL;
    size_t len;

    GdkPixbuf *buf;

    GtkWidget *event;
    GtkWidget *frame;
    GdkPixbuf *scale;
    GdkPixmap *pm;
    GdkBitmap *bm;
    int scale_width, scale_height;
    GtkRequisition requisition;

    GaimAccount *account;
    GaimPluginProtocolInfo *prpl_info = NULL;

    // GaimButtonStyle button_type;

    gaim_debug_misc("chaticon", "entered update function\n");
    g_return_if_fail(conv != NULL);
    g_return_if_fail(GAIM_IS_GTK_CONVERSATION(conv));
    g_return_if_fail(gaim_conversation_get_type(conv) == GAIM_CONV_CHAT);

    gtkconv = GAIM_GTK_CONVERSATION(conv);

    GaymChatIcon *icon_data = g_hash_table_lookup(icons, conv);
    gaim_debug_misc("chaticon",
                    "got icon_data: %x ... icon is %x, show_icon is %i\n",
                    icon_data, icon, icon_data->show_icon);
    // Look things up in our hand hash table of icon stuff.

    if (!icon)
        return;
    if (!icon_data->show_icon)
        return;

    account = gaim_conversation_get_account(conv);
    if (account && account->gc)
        prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(account->gc->prpl);

    /* Remove the current icon stuff */
    if (icon_data->icon_container != NULL)
        gtk_widget_destroy(icon_data->icon_container);
    icon_data->icon_container = NULL;

    if (icon_data->anim != NULL)
        g_object_unref(G_OBJECT(icon_data->anim));

    icon_data->anim = NULL;

    gaim_debug_misc("chaticon", "did some unreffing\n");
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

    gaim_debug_misc("chaticon", "did some more unreffing\n");
    // ///ICON GETTING
    // icon = gaim_conv_im_get_icon(GAIM_CONV_IM(conv));

    // if (icon == NULL)
    // return;

    data = gaim_buddy_icon_get_data(icon, &len);

    // ////END ICON GETTING

    /* this is such an evil hack, i don't know why i'm even considering
       it. we'll do it differently when gdk-pixbuf-loader isn't leaky
       anymore. */
    /* gdk-pixbuf-loader was leaky? is it still? */

    gaim_debug_misc("chaticon", "Got icon data\n");
    g_snprintf(filename, sizeof(filename),
               "%s" G_DIR_SEPARATOR_S "gaimicon-%s.%d",
               g_get_tmp_dir(), gaim_buddy_icon_get_username(icon),
               getpid());

    if (!(file = g_fopen(filename, "wb")))
        return;

    fwrite(data, 1, len, file);
    fclose(file);
    gaim_debug_misc("chaticon", "Wrote temp file\n");
    icon_data->anim = gdk_pixbuf_animation_new_from_file(filename, &err);

    gaim_debug_misc("chaticon", "Loaded icon from temp file\n");
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
        // if (icon_data->animate)
        // start_anim(NULL, conv);
    }

    get_icon_scale_size(icon_data->anim,
                        prpl_info ? &prpl_info->icon_spec : NULL,
                        &scale_width, &scale_height);
    scale =
        gdk_pixbuf_scale_simple(buf,
                                MAX(gdk_pixbuf_get_width(buf) *
                                    scale_width /
                                    gdk_pixbuf_animation_get_width
                                    (icon_data->anim), 1),
                                MAX(gdk_pixbuf_get_height(buf) *
                                    scale_height /
                                    gdk_pixbuf_animation_get_height
                                    (icon_data->anim), 1),
                                GDK_INTERP_NEAREST);

    gdk_pixbuf_render_pixmap_and_mask(scale, &pm, &bm, 100);
    g_object_unref(G_OBJECT(scale));


    icon_data->icon_container = gtk_vbox_new(FALSE, 0);

    frame = gtk_frame_new(NULL);
    gtk_frame_set_shadow_type(GTK_FRAME(frame),
                              (bm ? GTK_SHADOW_NONE : GTK_SHADOW_IN));
    gtk_box_pack_start(GTK_BOX(icon_data->icon_container), frame,
                       FALSE, FALSE, 0);

    event = gtk_event_box_new();
    gtk_container_add(GTK_CONTAINER(frame), event);
    // g_signal_connect(G_OBJECT(event), "button-press-event",
    // G_CALLBACK(icon_menu), conv);
    gtk_widget_show(event);

    icon_data->icon = gtk_image_new_from_pixmap(pm, bm);
    gtk_widget_set_size_request(icon_data->icon, scale_width,
                                scale_height);
    gtk_container_add(GTK_CONTAINER(event), icon_data->icon);
    gtk_widget_show(icon_data->icon);

    g_object_unref(G_OBJECT(pm));

    if (bm)
        g_object_unref(G_OBJECT(bm));

    // ////OLD CODE, KEEPING FOR REFERENCE UNTIL COMPLETE.
    // button_type =
    // gaim_prefs_get_int("/gaim/gtk/conversations/button_type");
    /* the button seems to get its size before the box, so... */
    gtk_widget_size_request(gtkconv->send, &requisition);
    // if (button_type == GAIM_BUTTON_NONE || requisition.height * 1.5 <
    // scale_height) {
    gtk_box_pack_start(GTK_BOX(icon_data->icon_container_parent),
                       icon_data->icon_container, FALSE, FALSE, 0);
    /* gtk_box_reorder_child(GTK_BOX(gtkconv->lower_hbox), vbox, 0); */
    // } else {
    // gtk_box_pack_start(GTK_BOX(gtkconv->bbox),
    // icon_data->icon_container, FALSE, FALSE, 0);
    // gtk_box_reorder_child(GTK_BOX(gtkconv->bbox),
    // icon_data->icon_container, 0);
    // }

    gtk_widget_show(icon_data->icon_container);
    gtk_widget_show(frame);
    // ////////////////////
    /* The buddy icon code needs badly to be fixed. */
    buf = gdk_pixbuf_animation_get_static_image(icon_data->anim);
    // if(conv ==
    // gaim_conv_window_get_active_conversation(gaim_conversation_get_window(conv)))
    // gtk_window_set_icon(GTK_WINDOW(gtkwin->window), buf);
}


static void changed_cb(GtkTreeSelection * selection, gpointer data)
{

    GaimConversation *c = (GaimConversation *) data;
    GtkTreeIter iter;
    GtkTreeModel *model;
    gchar *name;

    gtk_tree_selection_get_selected(selection, &model, &iter);
    gtk_tree_model_get(model, &iter, CHAT_USERS_NAME_COLUMN, &name, -1);

    gaim_debug_misc("chatsort", "Click: %s\n", name);
    gtk_button_set_label(g_hash_table_lookup(icon_spots, c), name);

    GaimBuddyIcon *icon =
        gaim_buddy_icons_find(gaim_conversation_get_account(c), name);
    if (!icon) {

        // This triggers a conversation window quickly, to trigger an icon 
        // get.
        GaimConversation *temp =
            gaim_conversation_new(GAIM_CONV_IM,
                                  gaim_conversation_get_account(c), name);
        gaim_debug_misc("chaticon", "Created temp conv %x\n", temp);
        // gaim_conversation_destroy(temp);
        g_hash_table_replace(pending_updates, c, temp);
    } else {
        gaym_gtkconv_update_buddy_icon(c, icon);
    }
}

// This gets called BEFORE a chatlist is populated... just creates a new
// type of chat window.
static void redochatwindow(GaimConversation * c)
{

    GtkTreeModel *oldls;

    // Get a handle to the chat pane for the conversation
    GaimGtkConversation *gtkconv = GAIM_GTK_CONVERSATION(c);
    GaimGtkChatPane *gtkchat = gtkconv->u.chat;
    GtkTreeSelection *select =
        gtk_tree_view_get_selection(GTK_TREE_VIEW(gtkchat->list));
    gtk_tree_selection_set_mode(select, GTK_SELECTION_SINGLE);

    oldls = gtk_tree_view_get_model(GTK_TREE_VIEW(gtkchat->list));
    GtkBox *vbox = GTK_BOX(gtkchat->list->parent->parent);

    GtkWidget *button = gtk_button_new_with_label("A Button");
    gtk_box_pack_end(vbox, GTK_WIDGET(button), FALSE, FALSE, 0);
    gtk_widget_show(button);


    g_signal_connect(G_OBJECT(select), "changed", G_CALLBACK(changed_cb),
                     c);

    GaymChatIcon *icon_data = g_new0(GaymChatIcon, 1);

    icon_data->icon_container_parent = GTK_WIDGET(vbox);
    icon_data->icon_container = NULL;
    icon_data->icon = NULL;
    icon_data->anim = NULL;
    icon_data->iter = NULL;
    icon_data->show_icon = TRUE;

    g_hash_table_insert(icon_spots, c, button);
    g_hash_table_insert(icons, c, icon_data);

}

static gboolean check_for_update(gpointer * conversation,
                                 const gpointer * temp, gpointer * icon)
{

    g_return_val_if_fail(conversation != NULL, TRUE);
    g_return_val_if_fail(temp != NULL, TRUE);
    g_return_val_if_fail(icon != NULL, TRUE);

    GaimBuddyIcon *bicon = (GaimBuddyIcon *) icon;
    GaimConversation *c = (GaimConversation *) conversation;
    GaimConversation *tempconv = (GaimConversation *) temp;


    const gchar *name_needing_update =
        gaim_conversation_get_name(tempconv);
    gaim_debug_misc("chaticon",
                    "Conversation: %x, looked for name in %x, found %s\n",
                    c, tempconv, name_needing_update);

    g_return_val_if_fail(name_needing_update != NULL, FALSE);

    if (!strcmp(gaim_buddy_icon_get_username(bicon), name_needing_update)) {
        gaym_gtkconv_update_buddy_icon(c, bicon);
        gaim_buddy_icon_ref(bicon);
        gaim_conversation_destroy(tempconv);
        return TRUE;
    }
    return TRUE;
}

static void icon_updated(GaimConnection * gc, GaimBuddyIcon * icon)
{

    g_return_if_fail(icon != NULL);
    gaim_debug_misc("chaticon", "Icon updated: (%x) %s\n", icon,
                    gaim_buddy_icon_get_username(icon));
    g_hash_table_foreach_remove(pending_updates,
                                (GHRFunc) check_for_update, icon);

}
static gboolean plugin_load(GaimPlugin * plugin)
{
    icons = g_hash_table_new(g_direct_hash, g_direct_equal);
    icon_spots = g_hash_table_new(g_direct_hash, g_direct_equal);
    pending_updates = g_hash_table_new(g_direct_hash, g_direct_equal);

    gaim_signal_connect(gaim_conversations_get_handle(), "chat-joined",
                        plugin, GAIM_CALLBACK(redochatwindow), NULL);
    gaim_signal_connect(gaim_accounts_get_handle(), "buddy-icon-fetched",
                        plugin, GAIM_CALLBACK(icon_updated), NULL);

    hider_window = gaim_conv_window_new();
    // gaim_conv_window_hide(hider_window);
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
    N_("Chatroom Icons"),
    VERSION,
    N_("Shows user thumbnails below the names list in a chatroom."),
    N_("Shows user thumbnails below the names list in a chatroom."),
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
