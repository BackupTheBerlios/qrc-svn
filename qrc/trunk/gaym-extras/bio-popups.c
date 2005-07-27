#include "gaym-extras.h"
// Consider combining into one popup hash...
GHashTable *popup_rects;
GHashTable *popup_timeouts;
GHashTable *popups;
void clean_popup_stuff(GaimConversation * c)
{

    GaimGtkConversation *gtkconv = GAIM_GTK_CONVERSATION(c);
    if (c->type == GAIM_CONV_IM) {
        g_hash_table_remove(popup_timeouts, gtkconv->tab_label);
        g_hash_table_remove(popups, gtkconv->tab_label);
    } else if (c->type == GAIM_CONV_CHAT) {
        GaimGtkChatPane *gtkchat = gtkconv->u.chat;
        g_hash_table_remove(popup_timeouts, gtkchat->list);
        g_hash_table_remove(popup_rects, gtkchat->list);
        g_hash_table_remove(popups, gtkchat->list);
    }

}

static void namelist_leave_cb(GtkWidget * tv, GdkEventCrossing * e, gpointer n)
{
    //This prevent clicks from demloishing popups.
    if(e->mode != GDK_CROSSING_NORMAL)
	return;
    
    guint *timeout = g_hash_table_lookup(popup_timeouts, tv);
    g_hash_table_remove(popups, tv);

    if (*timeout) {
        g_source_remove(*timeout);
        *timeout = 0;
    }
}

static void namelist_paint_tip(GtkWidget * tipwindow, GdkEventExpose * event, gpointer data)
{
    char* tooltiptext=((struct paint_data*)data)->tooltiptext;
    const char* name=((struct paint_data*)data)->name;
    GtkStyle *style;
    char* filename=g_strdup_printf("%s.jpg",name);
    char* path = g_build_filename(gaim_user_dir(), "icons", "gaym", filename, NULL);
    gaim_debug_misc("popups","trying to load image %s\n",path);
    GError* err=NULL;
    GdkPixbuf* pixbuf = gdk_pixbuf_new_from_file(path, &err);	
    if(err)
    {
	gaim_debug_error("popups","Pixbuf error: %s\n",err->message);
	g_error_free(err);
    }
    g_free(filename);
    g_free(path);

    // GAIM_STATUS_ICON_LARGE);
    PangoLayout *layout;

    layout = gtk_widget_create_pango_layout(tipwindow, NULL);
    pango_layout_set_markup(layout, tooltiptext, strlen(tooltiptext));
    pango_layout_set_wrap(layout, PANGO_WRAP_WORD);
    pango_layout_set_width(layout, 300000);
    style = tipwindow->style;

    gtk_paint_flat_box(style, tipwindow->window, GTK_STATE_NORMAL,
                       GTK_SHADOW_OUT, NULL, tipwindow, "tooltip", 0, 0,
                       -1, -1);

#if GTK_CHECK_VERSION(2,2,0)
     gdk_draw_pixbuf(GDK_DRAWABLE(tipwindow->window), NULL, pixbuf,
     0, 0, 4, 4, -1, -1, GDK_RGB_DITHER_NONE, 0, 0);
#else
     gdk_pixbuf_render_to_drawable(pixbuf,
     GDK_DRAWABLE(tipwindow->window), NULL, 0, 0, 4, 4, -1, -1,
     GDK_RGB_DITHER_NONE, 0, 0);
#endif

    gtk_paint_layout(style, tipwindow->window, GTK_STATE_NORMAL, TRUE,
                     NULL, tipwindow, "tooltip", 57, 4, layout);

    g_object_unref (pixbuf);
    g_object_unref(layout);
    g_free(tooltiptext);
    g_free(data);

    return;
}

static gboolean tooltip_timeout(struct timeout_cb_data *data)
{
    const gchar *name;
    int scr_w, scr_h, w, h, x, y;
#if GTK_CHECK_VERSION(2,2,0)
    int mon_num;
    GdkScreen *screen = NULL;
#endif
    PangoLayout *layout;
    gboolean tooltip_top = FALSE;
    char *tooltiptext = NULL;
    GdkRectangle mon_size;
    guint *timeout;
    GtkWidget *tipwindow;
    GtkWidget *tv = data->tv;
    
    GaymTooltipType type = data->type;
    struct gaym_conn *gaym = data->gaym;
    g_free(data);
    GaimPluginProtocolInfo *prpl_info =
        GAIM_PLUGIN_PROTOCOL_INFO(gaim_find_prpl (gaim_account_get_protocol_id (gaym->account)));

    timeout = (guint *) g_hash_table_lookup(popup_timeouts, tv);
    /* we check to see if we're still supposed to be moving, now that gtk
       events have happened, and the mouse might not still be in the buddy 
       list */
    while (gtk_events_pending())
        gtk_main_iteration();
    if (!(*timeout)) {
        return FALSE;
    }

    if (type == TOOLTIP_CHAT) {
        GtkTreePath *path;
        GtkTreeIter iter;
        GtkTreeModel *model;
        GdkRectangle *rect;

        rect = g_hash_table_lookup(popup_rects, tv);
        if (!gtk_tree_view_get_path_at_pos
            (GTK_TREE_VIEW(tv), rect->x, rect->y, &path, NULL, NULL, NULL))
            return FALSE;
        model = gtk_tree_view_get_model(GTK_TREE_VIEW(tv));
        gtk_tree_model_get_iter(model, &iter, path);
        gtk_tree_model_get(model, &iter, CHAT_USERS_NAME_COLUMN, &name,
                           -1);
        gtk_tree_path_free(path);
    } else if (type == TOOLTIP_IM) {
        name = gtk_label_get_text(GTK_LABEL(tv));
    } else
        return FALSE;


    GaimBuddy *gb = g_new0(GaimBuddy, 1);
    gb->name = g_strdup(name);
    gb->account = gaym->account;
    tooltiptext = prpl_info->tooltip_text(gb);
    g_free(gb->name);
    g_free(gb);
    if (!tooltiptext) {
        guint *timeout = g_hash_table_lookup(popup_timeouts, tv);
        if (timeout) {
            int delay =
                gaim_prefs_get_int("/gaim/gtk/blist/tooltip_delay");
            g_timeout_add(delay, (GSourceFunc) tooltip_timeout, data);
        }
        return FALSE;
    }


    g_return_val_if_fail(tooltiptext != NULL, FALSE);

    tipwindow = g_hash_table_lookup(popups, tv);
    if (tipwindow)
    {
        g_hash_table_remove(popups, tv);
    }
    tipwindow = gtk_window_new(GTK_WINDOW_POPUP);
    g_hash_table_insert(popups, tv, tipwindow);

    gtk_widget_set_app_paintable(tipwindow, TRUE);
    gtk_window_set_resizable(GTK_WINDOW(tipwindow), FALSE);
    gtk_widget_set_name(tipwindow, "gtk-tooltips");
    
    struct paint_data* pdata=g_new0(struct paint_data,1);
    pdata->tooltiptext=tooltiptext;
    pdata->name=name;
    g_signal_connect(G_OBJECT(tipwindow), "expose_event",
                     G_CALLBACK(namelist_paint_tip), pdata);
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

    /* 57 is the size of a large status icon plus 4 pixels padding on each 
       side.  I should #define this or something */
    w = w + 57;
    h = MAX(h, 57);

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
    gtk_widget_set_size_request(tipwindow, w, h);
    gtk_window_move(GTK_WINDOW(tipwindow), x, y);
    gtk_widget_show(tipwindow);

    return FALSE;
}


static gboolean namelist_motion_cb(GtkWidget * tv, GdkEventMotion * event, gpointer gaym)
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
    if(G_UNLIKELY(path == NULL))
	return FALSE;
    struct timeout_cb_data *timeout_data =
        g_new0(struct timeout_cb_data, 1);
    timeout_data->tv = tv;
    timeout_data->gaym = gaym;
    timeout_data->type = TOOLTIP_CHAT;
    *timeout =
        g_timeout_add(delay, (GSourceFunc) tooltip_timeout, timeout_data);

    gtk_tree_view_get_cell_area(GTK_TREE_VIEW(tv), path, NULL, rect);

    ls = gtk_tree_view_get_model(GTK_TREE_VIEW(tv));
    tf = gtk_tree_model_get_iter(ls, &iter, path);
    gtk_tree_model_get(ls, &iter, CHAT_USERS_NAME_COLUMN, &name, -1);
    gtk_tree_view_get_cell_area(GTK_TREE_VIEW(tv), path, NULL, rect);

    return TRUE;
}

static void tab_leave_cb(GtkWidget * event, GdkEventCrossing * e, gpointer n)
{
    //Prevent clicks from demolishing popup.
    if(e->mode != GDK_CROSSING_NORMAL)
	return;
    GtkWidget *tab = gtk_bin_get_child(GTK_BIN(event));
    guint *timeout = g_hash_table_lookup(popup_timeouts, tab);
    g_hash_table_remove(popups, tab);


    if (timeout && *timeout) {
        g_source_remove(*timeout);
        *timeout = 0;
    }
}


static gboolean tab_entry_cb(GtkWidget * event, GdkEventCrossing * crossing, gpointer conv)
{

    guint *timeout;
    guint delay;
    GaimConversation *c = (GaimConversation *) conv;
    struct gaym_conn *gaym = gaim_conversation_get_gc(c)->proto_data;

    GtkWidget *tab = gtk_bin_get_child(GTK_BIN(event));
    timeout = g_hash_table_lookup(popup_timeouts, tab);

    delay = gaim_prefs_get_int("/gaim/gtk/blist/tooltip_delay");

    if (delay == 0)
        return FALSE;

    if (timeout && *timeout)
        return FALSE;

    // g_hash_table_remove(popups, tab);
    // g_source_remove(*timeout);



    struct timeout_cb_data *timeout_data =
        g_new0(struct timeout_cb_data, 1);
    timeout_data->tv = tab;
    timeout_data->gaym = gaym;
    timeout_data->type = TOOLTIP_IM;
    *timeout =
        g_timeout_add(delay, (GSourceFunc) tooltip_timeout, timeout_data);

    return TRUE;
}

void add_chat_popup_stuff(GaimConversation* c) {
    
    GaimGtkConversation *gtkconv = GAIM_GTK_CONVERSATION(c);
    GaimGtkChatPane *gtkchat = gtkconv->u.chat;
    GaimConnection *gc = gaim_conversation_get_gc(c);
    
    g_signal_connect(G_OBJECT(gtkchat->list), "motion-notify-event",
                     G_CALLBACK(namelist_motion_cb),
                     gc->proto_data);
    g_signal_connect(G_OBJECT(gtkchat->list), "leave-notify-event",
                     G_CALLBACK(namelist_leave_cb), NULL);


    g_hash_table_insert(popup_rects, gtkchat->list,
                        g_new0(GdkRectangle, 1));
    g_hash_table_insert(popup_timeouts, gtkchat->list, g_new0(guint, 1));


}
void add_im_popup_stuff(GaimConversation* c) {
	GaimGtkConversation *gtkconv = GAIM_GTK_CONVERSATION(c);
        GtkWidget *event = gtk_event_box_new();
        gtk_widget_ref(gtkconv->tab_label);
        gtk_container_remove(GTK_CONTAINER(gtkconv->tabby), GTK_WIDGET(gtkconv->tab_label));
        gtk_widget_add_events(event, GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK);
        g_signal_connect(G_OBJECT(event), "enter-notify-event", G_CALLBACK(tab_entry_cb), c);
        g_signal_connect(G_OBJECT(event), "leave-notify-event", G_CALLBACK(tab_leave_cb), c);
        gtk_box_pack_start(GTK_BOX(gtkconv->tabby), GTK_WIDGET(event), TRUE, TRUE, 0);
        gtk_widget_show(GTK_WIDGET(event));
        gtk_container_add(GTK_CONTAINER(event), GTK_WIDGET(gtkconv->tab_label));
        gtk_widget_unref(gtkconv->tab_label);
        gtk_widget_show(GTK_WIDGET(gtkconv->tab_label));
        g_hash_table_insert(popup_timeouts, gtkconv->tab_label, g_new0(guint, 1));
}

void init_popups(){
    popup_rects = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, g_free);

    popup_timeouts = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, g_free);

    popups = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, (GDestroyNotify) gtk_widget_destroy);
}


