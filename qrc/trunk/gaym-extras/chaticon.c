#include "gaym-extras.h"
GHashTable *icons;
GHashTable *pending_updates;
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

void gaym_gtkconv_update_thumbnail(GaimConversation * conv, struct fetch_thumbnail_data
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
    gtk_widget_set_size_request(GTK_WIDGET(icon_data->frame), scale_width, scale_height);

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

    struct fetch_thumbnail_data *d = (struct fetch_thumbnail_data *) data;

    gaim_debug_misc("chaticon","Check for update: %x %s %s %i\n",c,name,d->who, d->pic_data_len);
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
    struct fetch_thumbnail_data *d = user_data;
    if (!pic_data) {
        return;
    }
    if (len && !g_strrstr_len(pic_data, len, "Server Error")) {
	char* dir;	
	if ((!d->from_file) && (dir = g_build_filename(gaim_user_dir(), "icons", "gaym", NULL) != NULL)) {
	    d->pic_data = pic_data;
	    d->pic_data_len = len;
	    gaim_build_dir(dir, S_IRUSR | S_IWUSR | S_IXUSR);
	    char* filename = g_strdup_printf("%s.jpg",d->who);
	    char* path = g_build_filename(dir, filename, NULL);
	    FILE* file;
	    if ((file = g_fopen(path, "wb")))
	    {
		fwrite(pic_data, 1, len, file);
	        fclose(file);
	    }
	    else
	    {
		gaim_debug_misc("chaticon","Couldn't write file\n");
	    }
	    g_free(filename);
	    g_free(path);
	    g_free(dir);
	}
    } else {
        d->pic_data = 0;
        d->pic_data_len = 0;
    }
     
    g_hash_table_foreach_remove(pending_updates,
                                (GHRFunc) check_for_update, d);
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
    
    
    char* dir = g_build_filename(gaim_user_dir(), "icons", "gaym", NULL);
    char* filename = g_strdup_printf("%s.jpg",name);
    char* path=NULL;
    FILE* file;
    struct stat st;
    struct fetch_thumbnail_data *data=g_new0(struct fetch_thumbnail_data,1);
    
    if ((dir != NULL) && (filename != NULL) && (path = g_build_filename(dir, filename, NULL)));
    {
	if (!g_stat(path, &st) && (file = g_fopen(path, "rb")))
	{
	    data->pic_data = g_malloc(st.st_size);
	    data->who=name;
	    data->pic_data_len = st.st_size;
	    data->from_file = TRUE;
	    fread(data->pic_data, 1, st.st_size, file);
	    fclose(file);
	}
	g_free(dir);
	g_free(filename);
	g_free(path);
	g_hash_table_replace(pending_updates, c, name);
	fetch_thumbnail_cb(data, data->pic_data, data->pic_data_len);
	return;
    }
    // Get GaymBuddy struct for the thumbnail URL.
    cm = g_hash_table_lookup(gaym->channel_members, name);
    if(!cm)
	return;

    
    // Fetch thumbnail.

    char *hashurl = g_hash_table_lookup(gaym->confighash,
                                        "mini-profile-panel.thumbnail-prefix");
   
    data = g_new0(struct fetch_thumbnail_data, 1);
    data->who = name;
    data->from_file = FALSE;
    char *url = g_strdup_printf("%s%s", hashurl, cm->thumbnail);
    gaim_url_fetch(url, FALSE, "Mozilla/4.0", FALSE,
                   fetch_thumbnail_cb, data);

    // Add entry to hash table for tracking.
    g_hash_table_replace(pending_updates, c, name);

}
void add_chat_icon_stuff(GaimConversation *c) {
    
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
    
    icon_data->event = gtk_event_box_new();
    gtk_container_add(GTK_CONTAINER(icon_data->frame), icon_data->event);

    // Maybe add menu functionality later.
    // g_signal_connect(G_OBJECT(icon_data->event), "button-press-event",
    // G_CALLBACK(icon_menu), conv);
    gtk_widget_show(icon_data->event);
    g_hash_table_insert(icons, c, icon_data);


}
void init_chat_icons() {

    icons = g_hash_table_new(g_direct_hash, g_direct_equal);
    pending_updates = g_hash_table_new(g_direct_hash, g_direct_equal);
}
