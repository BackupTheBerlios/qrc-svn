/* Puts last 4k of log in new conversations a la Everybuddy (and then
 * stolen by Trillian "Pro") */

#include "internal.h"
#include "gtkgaim.h"

#include "conversation.h"
#include "debug.h"
#include "log.h"
#include "prefs.h"
#include "signals.h"
#include "util.h"
#include "version.h"

#include "gtkconv.h"
#include "gtkimhtml.h"
#include "gtkplugin.h"

#define CHATSORT_PLUGIN_ID "gtk-chatsort"
#define CHATSORT_USERS_COLUMNS 4
#define CHATSORT_USERS_ENTRY_COLUMN 4

gint entry_id=0;
static gint sort_chat_users(GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b, gpointer userdata) {
return 1;
}

//This gets called BEFORE a chatlist is populated... just creates a new type of chat window.
static void redochatwindow(GaimConversation *c) {

	GtkListStore *ls;
	/**
	 * Unused variables:
	 *
	 * GtkCellRenderer *rend;
	 * GtkTreeViewColumn *col;
	 */
	GtkTreeModel *oldls;
	GtkTreeIter iter;
	
	//Get a handle to the chat pane for the conversation
	GaimGtkConversation* gtkconv = GAIM_GTK_CONVERSATION(c);
	GaimGtkChatPane* gtkchat = gtkconv->u.chat;

	
	//Remember the list in case the plugin is unloaded.
	oldls = gtk_tree_view_get_model(GTK_TREE_VIEW(gtkchat->list));

	//Make a new tree list

	//Columns: Icon, Name, Flags, Entry order
	ls = gtk_list_store_new(CHATSORT_USERS_COLUMNS, GDK_TYPE_PIXBUF, G_TYPE_STRING,
                                                        G_TYPE_INT, G_TYPE_INT);
	gtk_list_store_append(ls, &iter);
        gtk_list_store_set(ls, &iter, CHAT_USERS_NAME_COLUMN, "ChatSort", CHATSORT_USERS_ENTRY_COLUMN, 1, -1);
	//Eventually, should have an entry for default.
        gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(ls), CHAT_USERS_NAME_COLUMN, sort_chat_users, NULL, NULL);
	
	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(ls), CHAT_USERS_NAME_COLUMN,
                                                                               GTK_SORT_DESCENDING);

        gtk_tree_view_set_model(GTK_TREE_VIEW(gtkchat->list), GTK_TREE_MODEL(ls));

	//rend = gtk_cell_renderer_pixbuf_new();
        //col = gtk_tree_view_column_new_with_attributes(NULL, rend, "pixbuf", CHAT_USERS_ICON_COLUMN, NULL);
        //gtk_tree_view_column_set_clickable(GTK_TREE_VIEW_COLUMN(col), TRUE);
        //gtk_tree_view_append_column(GTK_TREE_VIEW(list), col);

	//Maybe eventually make our own menu.
	//g_signal_connect(G_OBJECT(list), "button_press_event",
        //                                 G_CALLBACK(right_click_chat_cb), conv);
        //g_signal_connect(G_OBJECT(list), "popup-menu",
        //                 G_CALLBACK(gtkconv_chat_popup_menu_cb), conv);

        //rend = gtk_cell_renderer_text_new();
        //col = gtk_tree_view_column_new_with_attributes(NULL, rend, "text", CHAT_USERS_NAME_COLUMN, NULL); 
        //gtk_tree_view_column_set_clickable(GTK_TREE_VIEW_COLUMN(col), TRUE);
        //gtk_tree_view_append_column(GTK_TREE_VIEW(list), col);
       
       //gtk_widget_set_size_request(list, 150, -1);
        //gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(list), FALSE);

	gaim_debug_misc("chatsort","redo window\n");

	

}

static void joined(GaimConversation *c, const char* name, GaimConvChatBuddyFlags flags) {

	gaim_debug_misc("chatsort","Joined: %s\n",name);
	gaim_debug_misc("chatsort","Flags: %x\n",flags);
	
}

static gboolean plugin_load(GaimPlugin *plugin)
{
	gaim_signal_connect(gaim_conversations_get_handle(),
						"chat-joined",
						plugin, GAIM_CALLBACK(redochatwindow), NULL);
	
	gaim_signal_connect(gaim_conversations_get_handle(),
						"chat-buddy-joined",
						plugin, GAIM_CALLBACK(joined), NULL);
	

	return TRUE;
}

static GaimPluginInfo info =
{
	GAIM_PLUGIN_MAGIC,
	GAIM_MAJOR_VERSION,
	GAIM_MINOR_VERSION,
	GAIM_PLUGIN_STANDARD,
	GAIM_GTK_PLUGIN_TYPE,
	0,
	NULL,
	GAIM_PRIORITY_DEFAULT,
	CHATSORT_PLUGIN_ID,
	N_("Chatroom Sort options"),
	VERSION,
	N_("Changes the sorting options of chatroom lists."),
	N_("When a new conversation is opened this plugin will insert the last conversation into the current conversation."),
	"Jason LeBrun <gaim@jasonlebrun.info",
	GAIM_WEBSITE,
	plugin_load,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL
};

static void
init_plugin(GaimPlugin *plugin)
{
}

GAIM_INIT_PLUGIN(history, init_plugin, info)
