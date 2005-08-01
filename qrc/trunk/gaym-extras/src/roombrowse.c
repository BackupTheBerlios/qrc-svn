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
    COLUMN_INFO,
    N_COLUMNS
};

struct RoomBrowseInfo {

    GaimAccount *account;
    GaimConnection *gc;
};

struct update_cb_data {
    GaimConnection* gc;
    const char* room;
};

typedef struct RoomBrowseGui {
    GtkWidget* window;
    GtkWidget* button;
    GtkWidget* list;
    GtkTreeModel* model;
    GtkWidget* label;
    GtkTreeIter iter;
    GaimConnection* gc;
} RoomBrowseGui;

void roombrowse_add_info(gpointer data, RoomBrowseGui* browser) {
    /* Add a new row to the model */
    GaymBuddy* member=(GaymBuddy*)data;
    gaim_debug_misc("roombrowse","append row%s\n",member->name);
    char* sync="Y";
    if(!member->name || !member->prefix)
	return;
    if(strncmp(member->name, member->prefix, (MIN(strlen(member->name),strlen(member->prefix))-1))) 
    {
	    sync="N";
    }
    GString* info=g_string_new("");
    if(member->age)
	g_string_append_printf(info, "\nAge: %s", member->age); 
    if(member->location)
	g_string_append_printf(info, "\nLocation: %s", member->location); 
    if(member->bio)
	g_string_append_printf(info, "\nInfo: %s", member->bio); 
    g_string_erase(info, 0, 1);
    char* infoc=g_string_free(info, FALSE);
    gtk_list_store_append (GTK_LIST_STORE(browser->model), &browser->iter);
    if(member->thumbnail) {

	GdkPixbuf *pixbuf=lookup_cached_thumbnail(	browser->gc->account, 
						gaim_normalize(browser->gc->account,member->name));
        gtk_list_store_set(GTK_LIST_STORE(browser->model), &browser->iter, COLUMN_PHOTO, pixbuf, -1);

    }
    gtk_list_store_set (GTK_LIST_STORE(browser->model), &browser->iter,
			    COLUMN_SYNC, sync,
                          COLUMN_NAME, member->name, 
			  COLUMN_PREFIX, member->prefix,
			  COLUMN_INFO, infoc,
			  -1);


}
void roombrowse_update_list(GaimAccount* account, GaymNamelist* namelist) {

    gaim_debug_misc("roombrowse","update_list from namelist at %x\n",namelist);
    g_return_if_fail(namelist);
    
    RoomBrowseGui* browser=g_hash_table_lookup(browsers, namelist->roomname);
    if(!browser) {
	gaim_debug_misc("roombrowse","No browser found for %s\n",namelist->roomname);
    }
    gtk_list_store_clear(GTK_LIST_STORE(browser->model));
    g_slist_foreach(namelist->members, (GFunc)roombrowse_add_info, browser);
    
}
gboolean update_list(GtkWidget* button, gpointer data) {
    
    gaim_debug_misc("roombrowse","Doing list update!\n");
    struct update_cb_data* udata=(struct update_cb_data*)data;
    
    gaym_get_room_namelist(udata->room, udata->gc->proto_data);
    return TRUE;
}
static void roombrowse_menu_cb(GaimBlistNode * node, gpointer data)
{
    RoomBrowseGui* browser=g_new0(RoomBrowseGui, 1);
    GaimConnection* gc=(GaimConnection*)data;
    browser->window=gtk_window_new(GTK_WINDOW_TOPLEVEL);
   
    browser->gc=gc;
    //GaimAccount *account = ((GaimChat *) node)->account;
    //if (!win)
    //    win = gaim_conv_window_new();
    GaimChat *chat = ((GaimChat *) node);
    
    const char* room = gaim_chat_get_name(chat);
    const char* channel = g_hash_table_lookup(chat->components, "channel");
    gaim_debug_misc("roombrowse","chat name: %s\n",room);
    gaim_debug_misc("roombrowse","channel name: %s\n",channel);
    gtk_window_set_title(GTK_WINDOW(browser->window), room);
    
    GtkWidget* vbox=gtk_vbox_new(FALSE, 6);
    gtk_container_add(GTK_CONTAINER(browser->window), vbox);
    gtk_widget_show(vbox);
    
    browser->label=gtk_label_new(room);
    gtk_box_pack_start(GTK_BOX(vbox),browser->label, FALSE, FALSE, 0);
    gtk_widget_show(browser->label);
    GtkWidget* sw=gtk_scrolled_window_new(NULL, NULL);
    gtk_box_pack_start(GTK_BOX(vbox), sw, TRUE, TRUE, 0);
    gtk_widget_set_size_request(GTK_WIDGET(sw), 100, 200);
    gtk_widget_show(sw);

    GtkListStore* ls=gtk_list_store_new(N_COLUMNS,
					GDK_TYPE_PIXBUF,
					G_TYPE_STRING,
					G_TYPE_STRING, 
					G_TYPE_STRING, 
					G_TYPE_STRING);
    browser->model=GTK_TREE_MODEL(ls);
        
    browser->list=gtk_tree_view_new_with_model(GTK_TREE_MODEL(ls));
    //gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(browser->list), FALSE); 
    GtkCellRenderer* rend;
    GtkTreeViewColumn* col;
    
    rend=gtk_cell_renderer_pixbuf_new();
    col=gtk_tree_view_column_new_with_attributes("Photo", rend, "pixbuf", COLUMN_PHOTO, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(browser->list), col);

    rend=gtk_cell_renderer_text_new();
    col=gtk_tree_view_column_new_with_attributes("?", rend, "text", COLUMN_SYNC, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(browser->list), col);


    rend=gtk_cell_renderer_text_new();
    col=gtk_tree_view_column_new_with_attributes("Name", rend, "text", COLUMN_NAME, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(browser->list), col);
  
    rend=gtk_cell_renderer_text_new();
    col=gtk_tree_view_column_new_with_attributes("Pref", rend, "text", COLUMN_PREFIX, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(browser->list), col);


    rend=gtk_cell_renderer_text_new();
    col=gtk_tree_view_column_new_with_attributes("Info", rend, "text", COLUMN_INFO, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(browser->list), col);


    gtk_container_add(GTK_CONTAINER(sw), browser->list);
    gtk_widget_show(browser->list); 

    
    browser->button=gtk_button_new_with_label("Update");
    struct update_cb_data* udata=g_new0(struct update_cb_data, 1);
    udata->gc=gc;
    udata->room=channel;
    
    g_signal_connect(browser->button, "clicked", G_CALLBACK(update_list), udata);
    gtk_box_pack_start(GTK_BOX(vbox), browser->button, FALSE, FALSE, 0);
    gtk_widget_show(browser->button);

    gtk_widget_show(browser->window);

    g_hash_table_insert(browsers, g_strdup(channel), browser);
    update_list(browser->button, udata);
}
static void roombrowse_menu_create(GaimBlistNode * node, GList** menu)
{

    char *label;

    struct gaym_conn *gaym;
    GaimChat *chat = (GaimChat *) node;

    
    if (node->type != GAIM_BLIST_CHAT_NODE)
        return;

    gaym = chat->account->gc->proto_data;

    //char* room = g_strdup(g_hash_table_lookup(chat->components, "alias"));
    gaim_debug_misc("roombrowse", "chat: %xRoom name: %s\n", chat, gaim_chat_get_name(chat));

    label = g_strdup_printf("Browse users in %s", gaim_chat_get_name(chat));
    GaimBlistNodeAction *act = gaim_blist_node_action_new(label,
                                                          roombrowse_menu_cb,
							  chat->account->gc);

    *menu = g_list_append(*menu, act);
    // g_free(label);
}
void init_roombrowse(GaimPlugin* plugin)
{
    gaim_signal_connect(gaim_blist_get_handle(),
                        "blist-node-extended-menu",
                        plugin, GAIM_CALLBACK(roombrowse_menu_create),
                        NULL);

    gaim_signal_connect(gaim_accounts_get_handle(),
			"namelist-complete",
			plugin, GAIM_CALLBACK(roombrowse_update_list), NULL);
    
    browsers=g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    
    return;
}
