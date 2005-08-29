/* Puts last 4k of log in new conversations a la Everybuddy (and then
   stolen by Trillian "Pro") */

#include "internal.h"
#include "gtkgaim.h"

#include "conversation.h"
#include "debug.h"
#include "log.h"
#include "prefs.h"
#include "signals.h"
#include "util.h"
#include "version.h"
#include "prpl.h"

#include "gtkconv.h"
#include "gtkimhtml.h"
#include "gtkplugin.h"
#include "gtkdialogs.h"
#include "gtkutils.h"
#include "gtkblist.h"
#include "gtkimhtmltoolbar.h"
#include <gdk/gdkkeysyms.h>
#include "gaym-extras.h"

#include "../../gaym/src/gaym.h"


static GHashTable *browsers = NULL;


enum {
    COLUMN_PHOTO,
    COLUMN_SYNC,
    COLUMN_NAME,
    COLUMN_PREFIX,
    COLUMN_ROOM,
    COLUMN_INFO,
    N_COLUMNS
};

struct RoomBrowseInfo {

    GaimAccount *account;
    GaimConnection *gc;
};

struct update_cb_data {
    GaimConnection *gc;
    const char *room;
};

typedef struct RoomBrowseGui {
    GtkWidget *window;
    GtkWidget *button;
    GtkWidget *list;
    GtkTreeModel *model;
    GtkWidget *label;
    GtkTreeIter iter;
    GaimConnection *gc;
} RoomBrowseGui;


void update_photos(const char *room, const RoomBrowseGui * browser,
                   const char *name)
{

    GaimPlugin *prpl = NULL;
    GaimPluginProtocolInfo *prpl_info = NULL;
    prpl =
        gaim_find_prpl(gaim_account_get_protocol_id(browser->gc->account));
    if (prpl)
        prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(prpl);

    int scale_width = 0, scale_height = 0;
    gboolean valid;
    GtkTreeIter iter;
    int row_count = 0;
    GtkTreeModel *list_store =
        gtk_tree_view_get_model(GTK_TREE_VIEW(browser->list));
    /* Get the first iter in the list */
    valid = gtk_tree_model_get_iter_first(list_store, &iter);

    while (valid) {
        /* Walk through the list, reading each row */
        gchar *str_data;

        /* Make sure you terminate calls to gtk_tree_model_get() with a
           '-1' value */
        gtk_tree_model_get(list_store, &iter, COLUMN_NAME, &str_data, -1);


        if (!strcmp(str_data, name)) {
            GdkPixbuf *pixbuf =
                lookup_cached_thumbnail(browser->gc->account,
                                        gaim_normalize(browser->gc->
                                                       account,
		                                         name));
	    if(!pixbuf)
		break;
            get_icon_scale_size(pixbuf,
                                prpl_info ? &prpl_info->icon_spec : NULL,
                                &scale_width, &scale_height);

            GdkPixbuf *scale = gdk_pixbuf_scale_simple(pixbuf,
                                                       scale_width,
                                                       scale_height,
                                                       GDK_INTERP_BILINEAR);

            GtkTreePath *path = gtk_tree_model_get_path(list_store, &iter);
            gtk_list_store_set(GTK_LIST_STORE(list_store), &iter,
                               COLUMN_PHOTO, scale, -1);

            gtk_tree_model_row_changed(list_store, path, &iter);
            // g_free(pixbuf);
            break;
        }
        row_count++;
        valid = gtk_tree_model_iter_next(list_store, &iter);
        g_free(str_data);
    }


}

void roombrowse_update_list_row(GaimConnection * gc, const char *who)
{

    g_hash_table_foreach(browsers, (GHFunc) update_photos, (char *) who);

}

void roombrowse_add_info(gpointer data, RoomBrowseGui * browser)
{
    /* Add a new row to the model */
    GaymBuddy *member = (GaymBuddy *) data;
    GaimPluginProtocolInfo *prpl_info = NULL;
    int scale_width = 0, scale_height = 0;
    char *sync = "Y";
    if (!member->name || !member->prefix)
        return;
    if (strncmp
        (member->name, member->prefix,
         (MIN(strlen(member->name), strlen(member->prefix)) - 1))) {
        sync = "N";
    }
    
    GString *info = g_string_new("");
    if (member->age)
        g_string_append_printf(info, "\n<b>Age:</b> %s", member->age);
    if (member->location)
    {
	char* escaped=g_markup_escape_text(member->location, strlen(member->location));
        g_string_append_printf(info, "\n<b>Location:</b> %s", escaped);
    }
    if (member->bio) 
    {
	char* escaped=g_markup_escape_text(member->bio, strlen(member->bio));
        g_string_append_printf(info, "\n<b>Info</b>: %s", escaped);
    }
    g_string_erase(info, 0, 1);
    char *infoc = g_string_free(info, FALSE);


        gtk_list_store_append(GTK_LIST_STORE(browser->model), &browser->iter);
    GdkPixbuf *pixbuf = NULL;
    if (member->thumbnail) {

        pixbuf = lookup_cached_thumbnail(browser->gc->account,
                                         gaim_normalize
                                         (browser->gc->account,
                                          member->name));

        if (browser->gc)
            prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(browser->gc->prpl);


    } else {
        GaimPlugin *prpl = NULL;
        GaimPluginProtocolInfo *prpl_info = NULL;
        prpl =
            gaim_find_prpl(gaim_account_get_protocol_id
                           (browser->gc->account));
        if (prpl)
            prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(prpl);
        if (prpl_info && prpl_info->list_icon) {
            const char *protoname =
                prpl_info->list_icon(browser->gc->account, NULL);

            char *image = g_strdup_printf("%s.png", protoname);
            char *filename =
                g_build_filename(DATADIR, "pixmaps", "gaim", "status",
                                 "default", image, NULL);

            pixbuf = gdk_pixbuf_new_from_file(filename, NULL);
        }
    }
    get_icon_scale_size(pixbuf,
                        prpl_info ? &prpl_info->icon_spec : NULL,
                        &scale_width, &scale_height);

    GdkPixbuf *scale = gdk_pixbuf_scale_simple(pixbuf,
                                               scale_width,
                                               scale_height,
                                               GDK_INTERP_BILINEAR);
    g_object_unref(pixbuf);

    gtk_list_store_set(GTK_LIST_STORE(browser->model), &browser->iter,
                       COLUMN_PHOTO, scale,
#if DEBUG
                       COLUMN_SYNC, sync, COLUMN_PREFIX, member->prefix,
#endif
                       COLUMN_ROOM, member->room,
		       COLUMN_NAME, member->name, 
		       COLUMN_INFO, infoc, -1);
    


}

void roombrowse_update_list(GaimAccount * account, GaymNamelist * namelist)
{

    g_return_if_fail(namelist);
    gaim_debug_misc("roombrowse", "update_list from namelist at %x\n",
                    namelist);

    RoomBrowseGui *browser =
        g_hash_table_lookup(browsers, namelist->roomname);
    if (!browser && namelist->roomname) {
        gaim_debug_misc("roombrowse", "No browser found for %s\n",
                        namelist->roomname);
	return;

    }
    gtk_list_store_clear(GTK_LIST_STORE(browser->model));
    g_slist_foreach(namelist->members, (GFunc) roombrowse_add_info,
                    browser);

}

static gboolean update_list(GtkWidget * button, gpointer data)
{

    gaim_debug_misc("roombrowse", "Doing list update!\n");
    struct update_cb_data *udata = (struct update_cb_data *) data;

    // gaym_get_room_namelist(udata->room, udata->gc->proto_data);
    gaim_signal_emit(gaim_accounts_get_handle(), "request-namelist",
                     udata->gc->account, udata->room);
    return TRUE;
}

static void chat_do_im(RoomBrowseGui * browser, const char *who)
{
    GaimPluginProtocolInfo *prpl_info = NULL;
    GaimConversation *conv;


    if (browser->gc->account && browser->gc)
        prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(browser->gc->prpl);

    conv = gaim_find_conversation_with_account(who, browser->gc->account);

    if (conv != NULL)
        gaim_conv_window_show(gaim_conversation_get_window(conv));
    else
        conv =
            gaim_conversation_new(GAIM_CONV_IM, browser->gc->account, who);

}
static void chat_do_info(RoomBrowseGui * browser, const char *who)
{
    GaimPluginProtocolInfo *prpl_info = NULL;
    GaimConnection *gc = browser->gc;

    if (gc) {
        prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(gc->prpl);

        prpl_info->get_info(gc, who);
    }
}

static void menu_chat_im_cb(GtkWidget * w, RoomBrowseGui * browser)
{
    const char *who = g_object_get_data(G_OBJECT(w), "user_data");

    chat_do_im(browser, who);
}

static void menu_chat_info_cb(GtkWidget * w, RoomBrowseGui * browser)
{
    char *who;

    who = g_object_get_data(G_OBJECT(w), "user_data");

    chat_do_info(browser, who);
}

// Right out of gtkconv.c, with a bunc removed.
static GtkWidget *create_chat_menu(RoomBrowseGui * browser,
                                   const char *who,
                                   GaimPluginProtocolInfo * prpl_info,
                                   GaimConnection * gc)
{
    static GtkWidget *menu = NULL;
    GtkWidget *button;

    /* 
     * If a menu already exists, destroy it before creating a new one,
     * thus freeing-up the memory it occupied.
     */
    if (menu)
        gtk_widget_destroy(menu);

    menu = gtk_menu_new();

    if (gc && (prpl_info->get_info || prpl_info->get_cb_info)) {
        button = gtk_menu_item_new_with_label(_("Info"));
        g_signal_connect(G_OBJECT(button), "activate",
                         G_CALLBACK(menu_chat_info_cb), browser);
        g_object_set_data_full(G_OBJECT(button), "user_data",
                               g_strdup(who), g_free);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), button);
        gtk_widget_show(button);
    }


    button = gtk_menu_item_new_with_label(_("IM"));
    g_signal_connect(G_OBJECT(button), "activate",
                     G_CALLBACK(menu_chat_im_cb), browser);
    g_object_set_data_full(G_OBJECT(button), "user_data", g_strdup(who),
                           g_free);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), button);
    gtk_widget_show(button);


    return menu;
}





// Right out of gtkconv.c
static gint chat_popup_menu_cb(GtkWidget * widget, RoomBrowseGui * browser)
{
    GaimPluginProtocolInfo *prpl_info = NULL;
    GaimConnection *gc;
    GtkTreeSelection *sel;
    GtkTreeIter iter;
    GtkTreeModel *model;
    GtkWidget *menu;
    gchar *who;

    gc = browser->gc;

    model = gtk_tree_view_get_model(GTK_TREE_VIEW(browser->list));

    if (gc != NULL)
        prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(gc->prpl);

    sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(browser->list));
    if (!gtk_tree_selection_get_selected(sel, NULL, &iter))
        return FALSE;

    gtk_tree_model_get(GTK_TREE_MODEL(model), &iter, COLUMN_NAME, &who,
                       -1);
    menu = create_chat_menu(browser, who, prpl_info, gc);
    gtk_menu_popup(GTK_MENU(menu), NULL, NULL,
                   gaim_gtk_treeview_popup_menu_position_func, widget,
                   0, GDK_CURRENT_TIME);
    g_free(who);

    return TRUE;
}
static void changed_cb(GtkTreeSelection * selection, gpointer gc)
{

    g_return_if_fail(selection != NULL);
    gaim_debug_misc("roombrowse","changed_cb\n");

    GtkTreeIter iter;
    GtkTreeModel *model = NULL;
    gchar *name;
    if (!gtk_tree_selection_get_selected(selection, &model, &iter))
        return;

    gtk_tree_model_get(model, &iter, COLUMN_NAME, &name, -1);

    gaim_signal_emit(gaim_accounts_get_handle(), "request-info-quietly",
                     gc, name);

    return;

}

// Right out of gtkconv.c
static gint
click_cb(GtkWidget * widget, GdkEventButton * event,
         RoomBrowseGui * browser)
{
    GaimPluginProtocolInfo *prpl_info = NULL;
    GaimConnection *gc;
    GtkTreePath *path;
    GtkTreeIter iter;
    GtkTreeModel *model;
    GtkTreeViewColumn *column;
    gchar *who;
    int x, y;

    gc = browser->gc;

    model = gtk_tree_view_get_model(GTK_TREE_VIEW(browser->list));

    gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(browser->list),
                                  event->x, event->y, &path, &column, &x,
                                  &y);

    if (path == NULL)
        return FALSE;

    if (gc != NULL)
        prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(gc->prpl);

    gtk_tree_selection_select_path(GTK_TREE_SELECTION
                                   (gtk_tree_view_get_selection
                                    (GTK_TREE_VIEW(browser->list))), path);

    gtk_tree_model_get_iter(GTK_TREE_MODEL(model), &iter, path);
    gtk_tree_model_get(GTK_TREE_MODEL(model), &iter, COLUMN_NAME, &who,
                       -1);

    if (event->button == 1 && event->type == GDK_2BUTTON_PRESS) {
        chat_do_im(browser, who);
    } else if (event->button == 3 && event->type == GDK_BUTTON_PRESS) {
        GtkWidget *menu = create_chat_menu(browser, who, prpl_info, gc);
        gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL,
                       event->button, event->time);
    }
    g_free(who);
    gtk_tree_path_free(path);

    return TRUE;
}

static gboolean browser_window_destroyed(GtkWidget* window, GdkEvent* event, gpointer name) {
    gaim_debug_misc("roombrowser","remove browser entry for %s\n",name);
    g_hash_table_remove(browsers, name);
    g_free(name);
    return FALSE;
}
static void roombrowse_menu_cb(GaimBlistNode * node, gpointer data)
{
    RoomBrowseGui *browser = g_new0(RoomBrowseGui, 1);
    GaimConnection *gc = (GaimConnection *) data;
    browser->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

    browser->gc = gc;
    // GaimAccount *account = ((GaimChat *) node)->account;
    // if (!win)
    // win = gaim_conv_window_new();
    GaimChat *chat = ((GaimChat *) node);

    const char *room = gaim_chat_get_name(chat);
    const char *channel = g_hash_table_lookup(chat->components, "channel");
    gaim_debug_misc("roombrowse", "chat name: %s\n", room);
    gaim_debug_misc("roombrowse", "channel name: %s\n", channel);
    gtk_window_set_title(GTK_WINDOW(browser->window), room);
    
    g_signal_connect(browser->window, "delete-event", G_CALLBACK(browser_window_destroyed), g_strdup(channel));
    GtkWidget *vbox = gtk_vbox_new(FALSE, 6);
    gtk_container_add(GTK_CONTAINER(browser->window), vbox);
    gtk_widget_show(vbox);

    browser->label = gtk_label_new(room);
    gtk_box_pack_start(GTK_BOX(vbox), browser->label, FALSE, FALSE, 0);
    gtk_widget_show(browser->label);
    GtkWidget *sw = gtk_scrolled_window_new(NULL, NULL);
    gtk_box_pack_start(GTK_BOX(vbox), sw, TRUE, TRUE, 0);
    gtk_widget_set_size_request(GTK_WIDGET(sw), 400, 600);
    gtk_widget_show(sw);

    GtkListStore *ls = gtk_list_store_new(N_COLUMNS,
                                          GDK_TYPE_PIXBUF,
                                          G_TYPE_STRING,
                                          G_TYPE_STRING,
                                          G_TYPE_STRING,
					  G_TYPE_STRING,
                                          G_TYPE_STRING);
    browser->model = GTK_TREE_MODEL(ls);

    browser->list = gtk_tree_view_new_with_model(GTK_TREE_MODEL(ls));
    // gtk_tree_view_set_fixed_height_mode(GTK_TREE_VIEW(browser->list),
    // TRUE);
    // gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(browser->list),
    // FALSE); 
    GtkCellRenderer *rend;
    GtkTreeViewColumn *col;
    gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(browser->list), TRUE);
    rend = gtk_cell_renderer_pixbuf_new();
    gtk_cell_renderer_set_fixed_size(rend, -1, 80);
    col =
        gtk_tree_view_column_new_with_attributes("Photo", rend, "pixbuf",
                                                 COLUMN_PHOTO, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(browser->list), col);


    rend = gtk_cell_renderer_text_new();
    gtk_cell_renderer_set_fixed_size(rend, -1, 80);
    col =
        gtk_tree_view_column_new_with_attributes("Name", rend, "text",
                                                 COLUMN_NAME, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(browser->list), col);
#if DEBUG
    rend = gtk_cell_renderer_text_new();
    gtk_cell_renderer_set_fixed_size(rend, -1, 80);
    col =
        gtk_tree_view_column_new_with_attributes("Pref", rend, "text",
                                                 COLUMN_PREFIX, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(browser->list), col);

    rend = gtk_cell_renderer_text_new();
    gtk_cell_renderer_set_fixed_size(rend, -1, 80);
    col =
        gtk_tree_view_column_new_with_attributes("?", rend, "text",
                                                 COLUMN_SYNC, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(browser->list), col);
#endif
    rend = gtk_cell_renderer_text_new();
    gtk_cell_renderer_set_fixed_size(rend, -1, 80);
    col =
        gtk_tree_view_column_new_with_attributes("Room", rend, "text",
                                                 COLUMN_ROOM, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(browser->list), col);

    rend = gtk_cell_renderer_text_new();
    gtk_cell_renderer_set_fixed_size(rend, -1, 80);
    col =
        gtk_tree_view_column_new_with_attributes("Info", rend, "markup",
                                                 COLUMN_INFO, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(browser->list), col);

    g_signal_connect(G_OBJECT(browser->list), "button_press_event",
                     G_CALLBACK(click_cb), browser);
    g_signal_connect(G_OBJECT(browser->list), "popup-menu",
                     G_CALLBACK(chat_popup_menu_cb), browser);

    GtkTreeSelection *select =
        gtk_tree_view_get_selection(GTK_TREE_VIEW(browser->list));

    gtk_tree_selection_set_mode(select, GTK_SELECTION_SINGLE);

    g_signal_connect(G_OBJECT(select), "changed",
                     G_CALLBACK(changed_cb), gc);
    gtk_container_add(GTK_CONTAINER(sw), browser->list);
    gtk_widget_show(browser->list);


    browser->button = gtk_button_new_with_label("Update");
    struct update_cb_data *udata = g_new0(struct update_cb_data, 1);
    udata->gc = gc;
    udata->room = channel;

    g_signal_connect(browser->button, "clicked", G_CALLBACK(update_list),
                     udata);
    gtk_box_pack_start(GTK_BOX(vbox), browser->button, FALSE, FALSE, 0);
    gtk_widget_show(browser->button);

    gtk_widget_show(browser->window);

    g_hash_table_insert(browsers, g_strdup(channel), browser);
    update_list(browser->button, udata);
}
static void roombrowse_menu_create(GaimBlistNode * node, GList ** menu)
{

    char *label;

    struct gaym_conn *gaym;
    GaimChat *chat = (GaimChat *) node;


    if (node->type != GAIM_BLIST_CHAT_NODE)
        return;

    gaym = chat->account->gc->proto_data;


    label =
        g_strdup_printf("Browse users in %s", gaim_chat_get_name(chat));
    GaimBlistNodeAction *act = gaim_blist_node_action_new(label,
                                                          roombrowse_menu_cb,
                                                          chat->account->
                                                          gc);

    *menu = g_list_append(*menu, act);
    // g_free(label);
}

void init_roombrowse(GaimPlugin * plugin)
{
    gaim_signal_connect(gaim_blist_get_handle(),
                        "blist-node-extended-menu",
                        plugin, GAIM_CALLBACK(roombrowse_menu_create),
                        NULL);

    gaim_signal_connect(gaim_accounts_get_handle(),
                        "namelist-complete",
                        plugin, GAIM_CALLBACK(roombrowse_update_list),
                        NULL);

    gaim_signal_connect(gaim_accounts_get_handle(),
                        "info-updated",
                        plugin, GAIM_CALLBACK(roombrowse_update_list_row),
                        NULL);

    browsers =
        g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

    return;
}
