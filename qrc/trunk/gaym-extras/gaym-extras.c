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
#include "../gaym/src/helpers.h"

#define CHATSORT_PLUGIN_ID "gtk-chaticon"

GHashTable *icons;
GHashTable *pending_updates;
GHashTable *im_window_bios;
GHashTable *popup_rects;
GHashTable *popup_timeouts;
GHashTable *popups;

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
    GtkWidget *bio_area;

} GaymChatIcon;

typedef enum {
    SORT_ALPHA,
    SORT_ENTRY,
    SORT_CATEGORY,
} GaymSortOrder;

static gint
sort_chat_users_by_entry(GtkTreeModel * model, GtkTreeIter * a,
                         GtkTreeIter * b, gpointer userdata)
{
    GaimConvChatBuddyFlags f1 = 0, f2 = 0;
    char *user1 = NULL, *user2 = NULL;
    gint ret = 0;

    gtk_tree_model_get(model, a, CHAT_USERS_NAME_COLUMN, &user1,
                       CHAT_USERS_FLAGS_COLUMN, &f1, -1);
    gtk_tree_model_get(model, b, CHAT_USERS_NAME_COLUMN, &user2,
                       CHAT_USERS_FLAGS_COLUMN, &f2, -1);

    if (user1 == NULL || user2 == NULL) {
        if (!(user1 == NULL && user2 == NULL))
            ret = (user1 == NULL) ? -1 : 1;
    } else if (f1 != f2) {
        /* sort more important users first */
        ret = (f1 > f2) ? -1 : 1;
    } else {
        ret = g_utf8_collate(user1, user2);
    }

    g_free(user1);
    g_free(user2);
    return ret;
}

static gint
sort_chat_users_by_alpha(GtkTreeModel * model, GtkTreeIter * a,
                         GtkTreeIter * b, gpointer userdata)
{
    char *user1 = NULL, *user2 = NULL, *luser1 = NULL, *luser2 = NULL;
    gint ret = 0;

    gtk_tree_model_get(model, a, CHAT_USERS_NAME_COLUMN, &user1, -1);
    gtk_tree_model_get(model, b, CHAT_USERS_NAME_COLUMN, &user2, -1);

    luser1 = g_utf8_strdown(user1, -1);
    luser2 = g_utf8_strdown(user2, -1);
    if (luser1 == NULL || luser2 == NULL) {
        if (!(luser1 == NULL && luser2 == NULL))
            ret = (luser1 == NULL) ? -1 : 1;
    } else {
        ret = g_utf8_collate(luser1, luser2);
    }

    g_free(user1);
    g_free(user2);
    g_free(luser1);
    g_free(luser2);
    return ret;
}


static gint
sort_chat_users_by_category(GtkTreeModel * model, GtkTreeIter * a,
                            GtkTreeIter * b, gpointer userdata)
{
    GaimConvChatBuddyFlags f1 = 0, f2 = 0;
    gint flag_mask = 0x000F;
    char *user1 = NULL, *user2 = NULL, *luser1 = NULL, *luser2 = NULL;
    gint ret = 0;

    gtk_tree_model_get(model, a, CHAT_USERS_NAME_COLUMN, &user1,
                       CHAT_USERS_FLAGS_COLUMN, &f1, -1);
    gtk_tree_model_get(model, b, CHAT_USERS_NAME_COLUMN, &user2,
                       CHAT_USERS_FLAGS_COLUMN, &f2, -1);

    f1 = f1 & flag_mask;
    f2 = f2 & flag_mask;

    luser1 = g_utf8_strdown(user1, -1);
    luser2 = g_utf8_strdown(user2, -1);
    if (luser1 == NULL || luser2 == NULL) {
        if (!(luser1 == NULL && luser2 == NULL))
            ret = (luser1 == NULL) ? -1 : 1;
    } else if (f1 != f2) {
        /* sort more important lusers first */
        ret = (f1 > f2) ? -1 : 1;
    } else {
        ret = g_utf8_collate(luser1, luser2);
    }

    g_free(user1);
    g_free(user2);
    g_free(luser1);
    g_free(luser2);
    return ret;
}



static void change_sort_order(GtkWidget * button, void *data)
{

    static GaymSortOrder order = SORT_ENTRY;


    GtkTreeView *list = (GtkTreeView *) data;
    GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(list));
    gaim_debug_misc("chatsort", "list: %x, data: %x, model: %x\n", list,
                    data, model);
    if (order == SORT_ALPHA) {
        order = SORT_CATEGORY;
        gaim_debug_misc("chatsort", "Change to entry order");
        gtk_button_set_label(GTK_BUTTON(button), "E");
        gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(model),
                                        CHAT_USERS_NAME_COLUMN,
                                        sort_chat_users_by_category, NULL,
                                        NULL);
    } else if (order == SORT_CATEGORY) {
        order = SORT_ENTRY;
        gaim_debug_misc("chatsort", "Change to category order");
        gtk_button_set_label(GTK_BUTTON(button), "P");
        gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(model),
                                        CHAT_USERS_NAME_COLUMN,
                                        sort_chat_users_by_entry, NULL,
                                        NULL);
    } else {
        order = SORT_ALPHA;
        gaim_debug_misc("chatsort", "Change to alpha order");
        gtk_button_set_label(GTK_BUTTON(button), "A");
        gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(model),
                                        CHAT_USERS_NAME_COLUMN,
                                        sort_chat_users_by_alpha, NULL,
                                        NULL);
    }

}
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

void gaym_gtkconv_update_thumbnail(GaimConversation * conv, struct gaym_fetch_thumbnail_data
                                   *thumbnail_data)
{
    GaimGtkConversation *gtkconv;

    char filename[256];
    FILE *file;
    GError *err = NULL;

    size_t len;

    GdkPixbuf *buf;
    GdkPixbuf *scale;
    GdkPixmap *pm;
    GdkBitmap *bm;
    int scale_width, scale_height;


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


}
static gboolean check_for_update(gpointer * conversation,
                                 const gpointer * name, gpointer * data)
{

    g_return_val_if_fail(conversation != NULL, TRUE);
    g_return_val_if_fail(name != NULL, TRUE);
    g_return_val_if_fail(data != NULL, TRUE);

    GaimConversation *c = (GaimConversation *) conversation;
    char *name_needing_update = (char *) name;

    struct gaym_fetch_thumbnail_data *d =
        (struct gaym_fetch_thumbnail_data *) data;


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
    if (len && !g_strrstr_len(pic_data, len, "Server Error")) {
        d->pic_data = pic_data;
        d->pic_data_len = len;
    } else {
        d->pic_data = 0;
        d->pic_data_len = 0;
    }
    g_hash_table_foreach_remove(pending_updates,
                                (GHRFunc) check_for_update, d);
    g_free(d->who);
    g_free(d);
}


static void changed_cb(GtkTreeSelection * selection, gpointer conv)
{

    g_return_if_fail(selection != NULL);
    g_return_if_fail(conv != NULL);

    GaimConversation *c = (GaimConversation *) conv;
    GaymBuddy *cm;
    struct gaym_conn *gaym = c->account->gc->proto_data;
    GtkTreeIter iter;
    GtkTreeModel *model;
    gchar *name;

    if (!gtk_tree_selection_get_selected(selection, &model, &iter))
        return;

    gtk_tree_model_get(model, &iter, CHAT_USERS_NAME_COLUMN, &name, -1);

    /* Remove the current icon stuff */
    GaymChatIcon *icon_data = g_hash_table_lookup(icons, c);
    if (icon_data->event != NULL)
        gtk_widget_destroy(icon_data->event);
    icon_data->event = NULL;

    // Get thumbnail URL.
    cm = gaym_get_channel_member_info(gaym, name);


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

static void clean_im_bio(GaimConversation * c)
{

    g_return_if_fail(c->type == GAIM_CONV_IM);
    g_hash_table_remove(im_window_bios, c->name);

}

static void update_im_bio(GaimAccount * account, gchar * name)
{
    g_return_if_fail(name != NULL);
    g_return_if_fail(account != NULL);

    GaimConversation *c =
        gaim_find_conversation_with_account(name, account);

    g_return_if_fail(c != NULL);

    g_return_if_fail(c->type == GAIM_CONV_IM);

    GaimGtkConversation *gtkconv = GAIM_GTK_CONVERSATION(c);
    struct gaym_conn *gaym = account->gc->proto_data;
    GaymBuddy *cm = gaym_get_channel_member_info(gaym, c->name);

    g_return_if_fail(cm != NULL);

    GtkBox *vbox_big = GTK_BOX(gtkconv->lower_hbox->parent);

    GtkWidget *bio_area = g_hash_table_lookup(im_window_bios, c->name);
    if (!bio_area) {
        bio_area = gtk_label_new(_(" "));
        g_hash_table_insert(im_window_bios, c, bio_area);
        gtk_box_pack_start(vbox_big, bio_area, TRUE, TRUE, 0);
        gtk_widget_show(bio_area);
    }

    char *buf;
    buf =
        g_strconcat(1 ? "Age: " : "",
                    cm->age ? cm->age : "?",
                    1 ? "\nLocation: " : "",
                    cm->location ? cm->location : "?",
                    cm->bio ? "\nInfo: " : "", cm->bio ? cm->bio : "");

    gtk_label_set_text(GTK_LABEL(bio_area), buf);
    gtk_label_set_line_wrap(GTK_LABEL(bio_area), TRUE);
    g_free(buf);

}
static void namelist_leave_cb(GtkWidget * tv, GdkEventCrossing * e,
                              gpointer n)
{
    guint *timeout = g_hash_table_lookup(popup_timeouts, tv);
    g_hash_table_remove(popups, tv);

    if (*timeout) {
        g_source_remove(*timeout);
        *timeout = 0;
    }
}

static void namelist_paint_tip(GtkWidget * tipwindow,
                               GdkEventExpose * event, gchar * tooltiptext)
{
    GtkStyle *style;

    // GdkPixbuf *pixbuf = gaim_gtk_blist_get_status_icon(node,
    // GAIM_STATUS_ICON_LARGE);
    PangoLayout *layout;

    // gchar* tooltiptext=data;
    layout = gtk_widget_create_pango_layout(tipwindow, NULL);
    pango_layout_set_markup(layout, tooltiptext, strlen(tooltiptext));
    pango_layout_set_wrap(layout, PANGO_WRAP_WORD);
    pango_layout_set_width(layout, 300000);
    style = tipwindow->style;

    gtk_paint_flat_box(style, tipwindow->window, GTK_STATE_NORMAL,
                       GTK_SHADOW_OUT, NULL, tipwindow, "tooltip", 0, 0,
                       -1, -1);

#if GTK_CHECK_VERSION(2,2,0)
    // gdk_draw_pixbuf(GDK_DRAWABLE(tipwindow->window), NULL, pixbuf,
    // 0, 0, 4, 4, -1 , -1, GDK_RGB_DITHER_NONE, 0, 0);
#else
    // gdk_pixbuf_render_to_drawable(pixbuf,
    // GDK_DRAWABLE(tipwindow->window), NULL, 0, 0, 4, 4, -1, -1,
    // GDK_RGB_DITHER_NONE, 0, 0);
#endif

    gtk_paint_layout(style, tipwindow->window, GTK_STATE_NORMAL, TRUE,
                     NULL, tipwindow, "tooltip", 38, 4, layout);

    // g_object_unref (pixbuf);
    g_object_unref(layout);

    return;
}

struct timeout_cb_data {
    GtkWidget *tv;
    struct gaym_conn *gaym;
};

static gboolean namelist_tooltip_timeout(struct timeout_cb_data *data)
{
    GtkTreePath *path;
    GtkTreeIter iter;
    GtkTreeModel *model;
    gchar *name;
    int scr_w, scr_h, w, h, x, y;
#if GTK_CHECK_VERSION(2,2,0)
    int mon_num;
    GdkScreen *screen = NULL;
#endif
    PangoLayout *layout;
    gboolean tooltip_top = FALSE;
    char *tooltiptext = NULL;
    GdkRectangle mon_size;
    GdkRectangle *rect;
    guint *timeout;
    GtkWidget *tipwindow;
    GtkWidget *tv = data->tv;
    struct gaym_conn *gaym = data->gaym;
    g_free(data);
    rect = g_hash_table_lookup(popup_rects, tv);
    timeout = (guint *) g_hash_table_lookup(popup_timeouts, tv);
    if (!gtk_tree_view_get_path_at_pos
        (GTK_TREE_VIEW(tv), rect->x, rect->y, &path, NULL, NULL, NULL))
        return FALSE;
    model = gtk_tree_view_get_model(GTK_TREE_VIEW(tv));
    gtk_tree_model_get_iter(model, &iter, path);
    gtk_tree_model_get(model, &iter, CHAT_USERS_NAME_COLUMN, &name, -1);



    while (gtk_events_pending())
        gtk_main_iteration();

    /* we check to see if we're still supposed to be moving, now that gtk
       events have happened, and the mouse might not still be in the buddy 
       list */
    if (!(*timeout)) {
        gtk_tree_path_free(path);
        return FALSE;
    }
    /* 
       gtk_tree_view_get_cell_area(GTK_TREE_VIEW(tv), path, NULL,
       &gtkblist->contact_rect);
       gdk_drawable_get_size(GDK_DRAWABLE(tv->window),
       &(gtkblist->contact_rect.width), NULL); gtk_tree_path_down (path);
       while (gtk_tree_model_get_iter(GTK_TREE_MODEL(gtkblist->treemodel), 
       &i, path)) { GdkRectangle rect;
       gtk_tree_view_get_cell_area(GTK_TREE_VIEW(tv), path, NULL, &rect);
       gtkblist->contact_rect.height += rect.height;
       gtk_tree_path_next(path);

       } */

    gtk_tree_path_free(path);

    GaymBuddy *cm = gaym_get_channel_member_info(gaym, name);

    tooltiptext = build_tooltip_text(cm);

    g_return_val_if_fail(tooltiptext != NULL, FALSE);

    tipwindow = g_hash_table_lookup(popups, tv);
    if (tipwindow)
        g_hash_table_remove(popups, tv);

    tipwindow = gtk_window_new(GTK_WINDOW_POPUP);
    g_hash_table_insert(popups, tv, tipwindow);

    gtk_widget_set_app_paintable(tipwindow, TRUE);
    gtk_window_set_resizable(GTK_WINDOW(tipwindow), FALSE);
    gtk_widget_set_name(tipwindow, "gtk-tooltips");
    g_signal_connect(G_OBJECT(tipwindow), "expose_event",
                     G_CALLBACK(namelist_paint_tip), tooltiptext);
    gtk_widget_ensure_style(tipwindow);

    layout = gtk_widget_create_pango_layout(tipwindow, NULL);
    pango_layout_set_wrap(layout, PANGO_WRAP_WORD);
    pango_layout_set_width(layout, 300000);
    pango_layout_set_markup(layout, tooltiptext, strlen(tooltiptext));
    pango_layout_get_size(layout, &w, &h);

#if GTK_CHECK_VERSION(2,2,0)
    gdk_display_get_pointer(gdk_display_get_default(), &screen, &x, &y,
                            NULL);
    mon_num = gdk_screen_get_monitor_at_point(screen, x, y);
    gdk_screen_get_monitor_geometry(screen, mon_num, &mon_size);

    scr_w = mon_size.width + mon_size.x;
    scr_h = mon_size.height + mon_size.y;
#else
    scr_w = gdk_screen_width();
    scr_h = gdk_screen_height();
    gdk_window_get_pointer(NULL, &x, &y, NULL);
    mon_size.x = 0;
    mon_size.y = 0;
#endif


    w = PANGO_PIXELS(w) + 8;
    h = PANGO_PIXELS(h) + 8;

    /* 38 is the size of a large status icon plus 4 pixels padding on each 
       side.  I should #define this or something */
    w = w + 38;
    h = MAX(h, 38);

#if GTK_CHECK_VERSION(2,2,0)
    if (w > mon_size.width)
        w = mon_size.width - 10;

    if (h > mon_size.height)
        h = mon_size.height - 10;
#endif

    // Find the conversation window here....
    // if (GTK_WIDGET_NO_WINDOW(window))
    // y+=window->allocation.y;

    x -= ((w >> 1) + 4);

    if ((y + h + 4) > scr_h || tooltip_top)
        y = y - h - 5;
    else
        y = y + 6;

    if (y < mon_size.y)
        y = mon_size.y;

    if (y != mon_size.y) {
        if ((x + w) > scr_w)
            x -= (x + w + 5) - scr_w;
        else if (x < mon_size.x)
            x = mon_size.x;
    } else {
        x -= (w / 2 + 10);
        if (x < mon_size.x)
            x = mon_size.x;
    }

    g_object_unref(layout);
    // g_free(tooltiptext);
    gtk_widget_set_size_request(tipwindow, w, h);
    gtk_window_move(GTK_WINDOW(tipwindow), x, y);
    gtk_widget_show(tipwindow);

    return FALSE;
}


static gboolean namelist_motion_cb(GtkWidget * tv, GdkEventMotion * event,
                                   gpointer gaym)
{
    GtkTreeModel *ls = NULL;
    GtkTreePath *path = NULL;
    GtkTreeIter iter;
    char *name;
    static int count = 0;
    gboolean tf;
    GdkRectangle *rect;
    guint *timeout;
    count++;
    guint delay;
    rect = g_hash_table_lookup(popup_rects, tv);
    g_return_val_if_fail(rect != NULL, FALSE);


    timeout = g_hash_table_lookup(popup_timeouts, tv);

    delay = gaim_prefs_get_int("/gaim/gtk/blist/tooltip_delay");

    if (delay == 0)
        return FALSE;

    if (*timeout) {
        if ((event->y > rect->y) && ((event->y - rect->height) < rect->y))
            return FALSE;
        /* We've left the cell.  Remove the timeout and create a new one
           below */
        g_hash_table_remove(popups, tv);
        g_source_remove(*timeout);
    }

    gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(tv), event->x, event->y,
                                  &path, NULL, NULL, NULL);
    g_return_val_if_fail(path != NULL, FALSE);
    struct timeout_cb_data *timeout_data =
        g_new0(struct timeout_cb_data, 1);
    timeout_data->tv = tv;
    timeout_data->gaym = gaym;
    *timeout =
        g_timeout_add(delay, (GSourceFunc) namelist_tooltip_timeout,
                      timeout_data);

    gtk_tree_view_get_cell_area(GTK_TREE_VIEW(tv), path, NULL, rect);

    ls = gtk_tree_view_get_model(GTK_TREE_VIEW(tv));
    tf = gtk_tree_model_get_iter(ls, &iter, path);
    gtk_tree_model_get(ls, &iter, CHAT_USERS_NAME_COLUMN, &name, -1);
    gtk_tree_view_get_cell_area(GTK_TREE_VIEW(tv), path, NULL, rect);

    return TRUE;
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

    GtkBox *hbox = GTK_BOX(gtkconv->lower_hbox);
    // GtkBox *vbox_big = GTK_BOX(gtkconv->lower_hbox->parent);

    g_signal_connect(G_OBJECT(select), "changed", G_CALLBACK(changed_cb),
                     c);
    g_signal_connect(G_OBJECT(gtkchat->list), "motion-notify-event",
                     G_CALLBACK(namelist_motion_cb),
                     account->gc->proto_data);
    g_signal_connect(G_OBJECT(gtkchat->list), "leave-notify-event",
                     G_CALLBACK(namelist_leave_cb), NULL);


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

    icon_data->event = gtk_event_box_new();
    gtk_container_add(GTK_CONTAINER(icon_data->frame), icon_data->event);
    // g_signal_connect(G_OBJECT(icon_data->event), "button-press-event",
    // G_CALLBACK(icon_menu), conv);
    gtk_widget_show(icon_data->event);
    /* 
       icon_data->bio_area = gtk_label_new(_(""));
       gtk_box_pack_start(vbox_big, icon_data->bio_area, TRUE, TRUE, 0);
       gtk_widget_show(icon_data->bio_area); */
    g_hash_table_insert(icons, c, icon_data);
    g_hash_table_insert(popup_rects, gtkchat->list,
                        g_new0(GdkRectangle, 1));
    g_hash_table_insert(popup_timeouts, gtkchat->list, g_new0(guint, 1));

    GtkBox *iconbox = (GtkBox *) gtkconv->info->parent;
    GtkWidget *button = gtk_button_new_with_label("E");
    gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
    gtk_box_pack_end(iconbox, button, FALSE, FALSE, 0);
    gtk_widget_show(button);
    g_signal_connect(G_OBJECT(button), "clicked",
                     G_CALLBACK(change_sort_order), gtkchat->list);
    gaim_debug_misc("chatsort", "Connected signal with data %x\n",
                    gtkchat->list);

}

static gboolean plugin_load(GaimPlugin * plugin)
{
    icons = g_hash_table_new(g_direct_hash, g_direct_equal);
    pending_updates = g_hash_table_new(g_direct_hash, g_direct_equal);
    im_window_bios =
        g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL,
                              (GDestroyNotify) gtk_widget_destroy);
    popup_rects =
        g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, g_free);

    popup_timeouts =
        g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, g_free);

    popups =
        g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL,
                              (GDestroyNotify) gtk_widget_destroy);
    gaim_signal_connect(gaim_conversations_get_handle(), "chat-joined",
                        plugin, GAIM_CALLBACK(redochatwindow), NULL);
    gaim_signal_connect(gaim_accounts_get_handle(), "info-updated", plugin,
                        GAIM_CALLBACK(update_im_bio), NULL);
    gaim_signal_connect(gaim_conversations_get_handle(),
                        "deleting-conversation", plugin,
                        GAIM_CALLBACK(clean_im_bio), NULL);

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
