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

#include "gtkconv.h"
#include "gtkimhtml.h"
#include "gtkplugin.h"

#define CHATSORT_PLUGIN_ID "gtk-chaticon"

GHashTable *icons;
GHashTable *icon_spots;
static void clear_icon()
{

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
    /**
     * Look in local icon cache here.
     * If the icon isn't here, get it from gaym
     * Update the widget
     */
}

/**
 * This gets called BEFORE a chatlist is populated... just creates a new
 * type of chat window.
 */
static void redochatwindow(GaimConversation * c)
{

    GtkTreeModel *oldls;

    /* Get a handle to the chat pane for the conversation */
    GaimGtkConversation *gtkconv = GAIM_GTK_CONVERSATION(c);
    GaimGtkChatPane *gtkchat = gtkconv->u.chat;
    GtkTreeSelection *select =
        gtk_tree_view_get_selection(GTK_TREE_VIEW(gtkchat->list));
    gtk_tree_selection_set_mode(select, GTK_SELECTION_SINGLE);

    oldls = gtk_tree_view_get_model(GTK_TREE_VIEW(gtkchat->list));
    GtkBox *vbox = GTK_BOX(gtkchat->list->parent->parent);
    GtkWidget *button = gtk_button_new_with_label("A Button");
    g_signal_connect(G_OBJECT(select), "changed", G_CALLBACK(changed_cb),
                     c);

    gtk_box_pack_end(vbox, GTK_WIDGET(button), FALSE, FALSE, 0);
    gtk_widget_show(button);
    g_hash_table_insert(icon_spots, c, button);


}


static gboolean plugin_load(GaimPlugin * plugin)
{
    icons = g_hash_table_new(g_direct_hash, g_direct_equal);
    icon_spots = g_hash_table_new(g_direct_hash, g_direct_equal);

    gaim_signal_connect(gaim_conversations_get_handle(), "chat-joined",
                        plugin, GAIM_CALLBACK(redochatwindow), NULL);
    gaim_signal_connect(gaim_conversations_get_handle(), "chat-buddy-left",
                        plugin, GAIM_CALLBACK(clear_icon), NULL);

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
