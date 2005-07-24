/* Attempt to sort chat users by entry order, instead of alpha */
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


// A dummy sort function... don't sort at all!
static gint sort_chat_users(GtkTreeModel * model, GtkTreeIter * a,
                            GtkTreeIter * b, gpointer userdata)
{
    return 1;
}

// This gets called BEFORE a chatlist is populated... just creates a new
// type of chat window.
static void redochatwindow(GaimConversation * c)
{

    GtkTreeModel *oldls;
    GtkTreeSelection *select;
    GtkTreeIter iter;

    // Get a handle to the chat pane for the conversation
    GaimGtkConversation *gtkconv = GAIM_GTK_CONVERSATION(c);
    GaimGtkChatPane *gtkchat = gtkconv->u.chat;


    oldls = gtk_tree_view_get_model(GTK_TREE_VIEW(gtkchat->list));
    select = gtk_tree_view_get_selection(GTK_TREE_VIEW(gtkchat->list));

    // This is a dummy "root" item. If it's not here,
    // then the first name entered into the list gets "stucK" at the
    // top. This is a hack.
    gtk_list_store_append(GTK_LIST_STORE(oldls), &iter);
    gtk_list_store_set(GTK_LIST_STORE(oldls), &iter,
                       CHAT_USERS_NAME_COLUMN, " ", -1);
    gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(oldls),
                                    CHAT_USERS_NAME_COLUMN,
                                    sort_chat_users, NULL, NULL);

}

static gboolean plugin_load(GaimPlugin * plugin)
{
    gaim_signal_connect(gaim_conversations_get_handle(),
                        "chat-joined",
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

static void init_plugin(GaimPlugin * plugin)
{
}

GAIM_INIT_PLUGIN(history, init_plugin, info)
