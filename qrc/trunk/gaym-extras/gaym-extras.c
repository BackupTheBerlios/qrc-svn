/* Show icons in chat room windows */


#include "gaym-extras.h"


#define GAYM_EXTRAS_PLUGIN_ID "gtk-gaym-extras"



// Adds motion handlers to IM tab labels.
static void redo_im_window(GaimConversation * c)
{
    if (!g_strrstr(gaim_account_get_protocol_id(c->account),"prpl-gaym"))
	    return;
    if (c && c->type == GAIM_CONV_IM)
        add_im_popup_stuff(c);
}


static void update_info_cb(GaimAccount * account, char *name)
{
    if (!g_strrstr(gaim_account_get_protocol_id(account),"prpl-gaym"))
	    return;
    gaim_debug_misc("gaym-extras", "info update\n");
}

static void redochatwindow(GaimConversation * c)
{
    if (!g_strrstr(gaim_account_get_protocol_id(c->account),"prpl-gaym"))
	    return;
    add_chat_sort_functions(c);
    add_chat_popup_stuff(c);
    add_chat_icon_stuff(c);
}

static gboolean plugin_load(GaimPlugin * plugin)
{
    init_chat_icons();
    init_popups();

    gaim_signal_connect(gaim_conversations_get_handle(), "chat-joined",
                        plugin, GAIM_CALLBACK(redochatwindow), NULL);

    gaim_signal_connect(gaim_conversations_get_handle(),
                        "conversation-created", plugin,
                        GAIM_CALLBACK(redo_im_window), NULL);

    gaim_signal_connect(gaim_accounts_get_handle(), "info-updated", plugin,
                        GAIM_CALLBACK(update_info_cb), NULL);

    gaim_signal_connect(gaim_conversations_get_handle(),
                        "deleting-conversation", plugin,
                        GAIM_CALLBACK(clean_popup_stuff), NULL);


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
    GAYM_EXTRAS_PLUGIN_ID,
    N_("Gaym Extras"),
    VERSION,
    N_("GUI-related additions for the gaym protocol plugin."),
    N_("Current functionality provided by this plugin:\n1. Allows namelist sort order in rooms to be changed.\n2. Shows thumbnails for currently selected user in rooms.\n3. Popup displays bio when you hover over a name in the namelist.\n4. Popup shows bio when you hover over an IM tab."),
    "Jason LeBrun gaym@jasonlebrun.info",
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
