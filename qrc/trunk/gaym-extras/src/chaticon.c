#include "gaym-extras.h"
GHashTable *icons;

void gaym_update_thumbnail(GaimConversation * conv, GdkPixbuf * pixbuf)
{

    GaimGtkConversation *gtkconv;

    GdkPixbuf *scale;
    GdkPixmap *pm = NULL;
    GdkBitmap *bm = NULL;
    int scale_width = 0, scale_height = 0;


    GaimAccount *account;
    GaimPluginProtocolInfo *prpl_info = NULL;
    g_return_if_fail(conv != NULL);
    g_return_if_fail(GAIM_IS_GTK_CONVERSATION(conv));
    g_return_if_fail(gaim_conversation_get_type(conv) ==
                     GAIM_CONV_TYPE_CHAT);

    gtkconv = GAIM_GTK_CONVERSATION(conv);

    GaymChatIcon *icon_data = g_hash_table_lookup(icons, conv);

    if (!icon_data->show_icon)
        return;

    account = gaim_conversation_get_account(conv);
    if (account && account->gc)
        prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(account->gc->prpl);

    if (!gaim_prefs_get_bool
        ("/gaim/gtk/conversations/im/show_buddy_icons"))
        return;

    if (gaim_conversation_get_gc(conv) == NULL)
        return;

    get_icon_scale_size(pixbuf,
                        prpl_info ? &prpl_info->icon_spec : NULL,
                        &scale_width, &scale_height);
    // double
    // aspect=(double)gdk_pixbuf_get_width(pixbuf)/(double)gdk_pixbuf_get_height(pixbuf); 
    // 
    // 
    // 
    // 

    scale =
        gdk_pixbuf_scale_simple(pixbuf,
                                scale_width,
                                scale_height, GDK_INTERP_BILINEAR);

    gdk_pixbuf_render_pixmap_and_mask(scale, &pm, &bm, 100);
    g_object_unref(G_OBJECT(scale));


    icon_data->event = gtk_event_box_new();
    gtk_container_add(GTK_CONTAINER(icon_data->frame), icon_data->event);
    gtk_widget_set_size_request(GTK_WIDGET(icon_data->frame), 57, 77);

    // g_signal_connect(G_OBJECT(icon_data->event), "button-press-event",
    // G_CALLBACK(icon_menu), conv);
    gtk_widget_show(icon_data->event);
    icon_data->icon = gtk_image_new_from_pixmap(pm, bm);
    gtk_container_add(GTK_CONTAINER(icon_data->event), icon_data->icon);
    gtk_widget_show(icon_data->icon);

    if (pm)
        g_object_unref(G_OBJECT(pm));

    if (bm)
        g_object_unref(G_OBJECT(bm));


}


static void changed_cb(GtkTreeSelection * selection, gpointer conv)
{

    g_return_if_fail(selection != NULL);
    g_return_if_fail(conv != NULL);

    GaimConversation *c = (GaimConversation *) conv;

    GtkTreeIter iter;
    GtkTreeModel *model = NULL;
    GdkPixbuf *pixbuf = NULL;
    gchar *name;

    if (!gtk_tree_selection_get_selected(selection, &model, &iter))
        return;

    gtk_tree_model_get(model, &iter, CHAT_USERS_NAME_COLUMN, &name, -1);

    /* Remove the current icon stuff */
    GaymChatIcon *icon_data = g_hash_table_lookup(icons, c);
    if (icon_data->event != NULL)
        gtk_widget_destroy(icon_data->event);
    icon_data->event = NULL;

    gtk_widget_grab_focus(GTK_WIDGET(model)->parent);


    pixbuf = lookup_cached_thumbnail(c->account, name);
    if (pixbuf)
        gaym_update_thumbnail(c, pixbuf);

    g_object_unref(pixbuf);
    return;

}

void add_chat_icon_stuff(GaimConversation * c)
{

    GtkTreeModel *ls;

    GaimGtkConversation *gtkconv = GAIM_GTK_CONVERSATION(c);
    GaimGtkChatPane *gtkchat = gtkconv->u.chat;
    GaimPluginProtocolInfo *prpl_info = NULL;
    GaimAccount *account = gaim_conversation_get_account(c);
    GaymChatIcon *icon_data = g_new0(GaymChatIcon, 1);

    if (account && account->gc)
        prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(account->gc->prpl);
    GtkTreeSelection *select =
        gtk_tree_view_get_selection(GTK_TREE_VIEW(gtkchat->list));

    gtk_tree_selection_set_mode(select, GTK_SELECTION_SINGLE);

    ls = gtk_tree_view_get_model(GTK_TREE_VIEW(gtkchat->list));

    GtkBox *hbox = GTK_BOX(gtkconv->lower_hbox);

    g_signal_connect(G_OBJECT(select), "changed", G_CALLBACK(changed_cb),
                     c);

    icon_data->icon_container_parent = GTK_WIDGET(hbox);
    icon_data->icon_container = NULL;
    icon_data->icon = NULL;
    icon_data->show_icon = TRUE;
    icon_data->icon_container = gtk_vbox_new(FALSE, 0);

    gtk_widget_set_size_request(GTK_WIDGET(icon_data->icon_container), 57, 77); // prpl_info->icon_spec.max_width,
    // prpl_info->icon_spec.max_height);


    icon_data->frame = gtk_frame_new(NULL);
    gtk_frame_set_shadow_type(GTK_FRAME(icon_data->frame),
                              (GTK_SHADOW_IN));
    gtk_box_pack_start(GTK_BOX(icon_data->icon_container),
                       icon_data->frame, FALSE, FALSE, 0);
    gtk_widget_show(icon_data->icon_container);
    gtk_widget_show(icon_data->frame);
    gtk_box_pack_end(GTK_BOX(icon_data->icon_container_parent),
                     icon_data->icon_container, FALSE, FALSE, 0);

    icon_data->event = gtk_event_box_new();
    gtk_container_add(GTK_CONTAINER(icon_data->frame), icon_data->event);

    // Maybe add menu functionality later.
    // g_signal_connect(G_OBJECT(icon_data->event), "button-press-event",
    // G_CALLBACK(icon_menu), conv);
    gtk_widget_show(icon_data->event);
    g_hash_table_insert(icons, c, icon_data);


}

void chaticon_replace(GaimConversation * conv, const char *name,
                      GaimConvChatBuddyFlags flags)
{
    GaimGtkConversation *gtkconv = GAIM_GTK_CONVERSATION(conv);
    GaimGtkChatPane *gtkchat = gtkconv->u.chat;

    gboolean valid;
    GtkTreeIter iter;
    int row_count = 0;
    GtkTreeModel *list_store =
        gtk_tree_view_get_model(GTK_TREE_VIEW(gtkchat->list));
    /* Get the first iter in the list */
    valid = gtk_tree_model_get_iter_first(list_store, &iter);

    while (valid) {
        /* Walk through the list, reading each row */
        gchar *str_data;

        /* Make sure you terminate calls to gtk_tree_model_get() with a
           '-1' value */
        gtk_tree_model_get(list_store, &iter,
                           CHAT_USERS_NAME_COLUMN, &str_data, -1);

        /* Do something with the data */
        g_print("Row %d: (%s)(%s)\n", row_count, str_data, name);

        if (!strcmp(str_data, name)) {
            GdkPixbuf *pixbuf = lookup_cached_thumbnail(conv->account,
                                                        gaim_normalize
                                                        (conv->account,
                                                         name));



            gaim_debug_misc("chaticon", "Got pixbuf: %x\n");
            GtkTreePath *path = gtk_tree_model_get_path(list_store, &iter);
            gtk_list_store_set(GTK_LIST_STORE(list_store), &iter, 0,
                               pixbuf, -1);

            gtk_tree_model_row_changed(list_store, path, &iter);
            // g_free(pixbuf);
            break;
        }
        row_count++;
        valid = gtk_tree_model_iter_next(list_store, &iter);
        g_free(str_data);
    }

}
void init_chat_icons(GaimPlugin * plugin)
{

    // gaim_signal_connect(gaim_accounts_get_handle(),
    // "info-updated",
    // plugin, GAIM_CALLBACK(im_set_icon), NULL);

    icons = g_hash_table_new(g_direct_hash, g_direct_equal);
}
