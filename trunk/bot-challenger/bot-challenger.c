/**
 * QRC
 *
 * QRC is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/* config.h */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/* system headers */
#include <glib.h>

/* gaim headers for most plugins */
#include "plugin.h"
#include "version.h"

/* gaim headers for this plugin */
#include "util.h"
#include "debug.h"
#include "account.h"
#include "privacy.h"

/* gaim header needed for gettext */
#include "internal.h"

/**
 * Declare the list of struct that is used to store the first
 * message that a member sends for later delivery if he meets
 * the challenge.
 */
typedef struct _PendingMessage PendingMessage;
struct _PendingMessage {
    char *protocol;
    char *username;
    char *sender;
    char *message;
};
static GSList *pending_list = NULL;     /* GSList of PendingMessage */

/**
 * Return TRUE if the protocols match
 */
gboolean protocmp(GaimAccount * account, const PendingMessage * pending)
{
    if (!gaim_utf8_strcasecmp(pending->username, account->username)) {
        return TRUE;
    } else {
        return FALSE;
    }
}

/**
 * Return TRUE if the accounts match
 */
gboolean usercmp(GaimAccount * account, const PendingMessage * pending)
{
    if (!gaim_utf8_strcasecmp(pending->username, account->username)) {
        return TRUE;
    } else {
        return FALSE;
    }
}

/**
 * Return TRUE if the member names match
 */
gboolean sendercmp(const char *sender, const PendingMessage * pending)
{
    if (!gaim_utf8_strcasecmp(pending->sender, sender)) {
        return TRUE;
    } else {
        return FALSE;
    }
}

/**
 * Print the contents of pending_list to the debug log
 */
void debug_pending_list()
{
    GSList *search = NULL;
    for (search = pending_list; search; search = search->next) {
        PendingMessage *pending = search->data;
        gaim_debug_info("bot-challenger",
                        "Pending:  protocol = %s, username = %s, sender = %s, message = %s\n",
                        pending->protocol, pending->username,
                        pending->sender, pending->message);
    }
}

/**
 * Send an IM to the recipient (thanks to the gaim-otr plugin)
 */
void inject_message(GaimAccount * account, const char *recipient,
                    const char *message)
{
    GaimConnection *connection;

    connection = gaim_account_get_connection(account);

    if (!connection) {
        const char *protocol = gaim_account_get_protocol_id(account);
        const char *accountname = gaim_account_get_username(account);
        GaimPlugin *p = gaim_find_prpl(protocol);

        gaim_debug_error("bot-challenger",
                         "You are not currently connected to "
                         "account %s (%s).", accountname, (p
                                                           && p->info->
                                                           name) ? p->
                         info->name : "Unknown");

        return;
    }
    serv_send_im(connection, recipient, message, 0);
}

/**
 * This is our callback for the receiving-im-msg signal.
 *
 * We return TRUE to block the IM, FALSE to accept the IM
 */
static gboolean receiving_im_msg_cb(GaimAccount * account, char **sender,
                                    char **buffer, int *flags, void *data)
{
    gboolean retval = FALSE;    /* assume the sender is allowed */
    gboolean found = FALSE;
    gint pos = -1;
    char *botmsg = NULL;


    PendingMessage *pending = NULL;
    GSList *slist = NULL;
    GSList *search = NULL;

    GaimConnection *connection = NULL;

    connection = gaim_account_get_connection(account);

    /* not good, but don't do anything */
    if (!connection || !sender) {
        return retval;
    }

    /* if there is already an open conversation, allowed it */
    if (gaim_find_conversation_with_account(*sender, account)) {
        return retval;
    }

    /* don't make buddies use the challenge/response system */
    if (gaim_find_buddy(account, *sender)) {
        return retval;
    }

    /* don't make permit list members use the challenge/response system */
    for (slist = account->permit; slist != NULL; slist = slist->next) {
        if (!gaim_utf8_strcasecmp
            (*sender, gaim_normalize(account, (char *) slist->data))) {
            return retval;
        }
    }

    /* if there is no question or no answer, allowe the sender */
    const char *question =
        gaim_prefs_get_string("/plugins/core/bot/challenger/question");
    const char *answer =
        gaim_prefs_get_string("/plugins/core/bot/challenger/answer");
    if (!question || !answer) {
        return retval;
    }

    /* blank / null message ... can this even happen? */
    if (!*buffer) {
        return retval;
    }

    /* ok, now we can actually do someting */

    /* search if this sender is already in pending */
    for (search = pending_list; search; search = search->next) {
        pending = search->data;
        pos = g_slist_position(pending_list, search);

        if (protocmp(account, pending) && usercmp(account, pending)
            && sendercmp(*sender, pending)) {
            found = TRUE;
            break;
        }
    }
    if (!found) {
            /**
             * its the first time through, save the nick/msg to the
             * queue and ask the question
             */
        if (pos == 9) {
            /**
             * don't track more than 10
             * add to the end, remove from the beginning
             */
            PendingMessage *freepend = NULL;
            freepend = g_slist_nth_data(pending_list, 0);
            g_free(freepend->protocol);
            g_free(freepend->username);
            g_free(freepend->sender);
            g_free(freepend->message);
            g_free(freepend);
            pending_list =
                g_slist_remove_link(pending_list,
                                    g_slist_nth(pending_list, 0));
        }

        PendingMessage *newpend = NULL;
        newpend = g_new0(PendingMessage, 1);
        newpend->protocol = g_strdup(account->protocol_id);
        newpend->username = g_strdup(account->username);
        newpend->sender = g_strdup(*sender);
        newpend->message = g_strdup(*buffer);
        pending_list = g_slist_append(pending_list, newpend);

        botmsg =
            g_strdup_printf(_
                            ("Bot Challenger engaged!  You are now on auto-ignore until you provide the correct answer:  %s"),
                            question);
        inject_message(account, *sender, botmsg);

        g_free(botmsg);
        retval = TRUE;
    } else {
        if (gaim_utf8_strcasecmp(*buffer, answer)) {
                /**
                 * Sorry, thanks for playing, please try again
                 */
            retval = TRUE;
        } else {
            botmsg =
                _
                ("Bot Challenger accepted your answer and delivered your original message.  You may now speak freely.");
            inject_message(account, *sender, botmsg);

            if (gaim_prefs_get_bool
                ("/plugins/core/bot/challenger/auto_add_permit")) {
                if (!gaim_privacy_permit_add(account, *sender, FALSE)) {
                    gaim_debug_info("bot-challenger",
                                    "Unable to add %s/%s/%s to permit list\n",
                                    *sender, pending->username,
                                    pending->protocol);
                }
            }

            serv_got_im(connection, *sender, pending->message, 0,
                        time(NULL));

            g_free(pending->protocol);
            g_free(pending->username);
            g_free(pending->sender);
            g_free(pending->message);
            g_free(pending);
            pending_list = g_slist_remove_link(pending_list, search);

            retval = TRUE;
        }
    }
    debug_pending_list();
    return retval;              /* return TRUE to block the IM */
}

void gaim_plugin_remove()
{
    return;
}

static GaimPluginPrefFrame *get_plugin_pref_frame(GaimPlugin * plugin)
{
    GaimPluginPrefFrame *frame;
    GaimPluginPref *ppref;

    frame = gaim_plugin_pref_frame_new();

    ppref = gaim_plugin_pref_new_with_label(_("Define the challenge:"));
    gaim_plugin_pref_frame_add(frame, ppref);

    ppref =
        gaim_plugin_pref_new_with_name_and_label
        ("/plugins/core/bot/challenger/question", _("Question"));
    gaim_plugin_pref_frame_add(frame, ppref);

    ppref =
        gaim_plugin_pref_new_with_name_and_label
        ("/plugins/core/bot/challenger/answer", _("Answer"));
    gaim_plugin_pref_frame_add(frame, ppref);

    ppref =
        gaim_plugin_pref_new_with_label(_
                                        ("When the challenge is met, let this person IM you and:"));
    gaim_plugin_pref_frame_add(frame, ppref);

    ppref =
        gaim_plugin_pref_new_with_name_and_label
        ("/plugins/core/bot/challenger/auto_add_permit",
         _("Add this person to your Permit List"));
    gaim_plugin_pref_frame_add(frame, ppref);

    return frame;
}

static GaimPluginUiInfo prefs_info = {
    get_plugin_pref_frame
};

static gboolean plugin_load(GaimPlugin * plugin)
{
    void *conv_handle = gaim_conversations_get_handle();

    gaim_prefs_add_none("/plugins");
    gaim_prefs_add_none("/plugins/core");
    gaim_prefs_add_none("/plugins/core/bot");
    gaim_prefs_add_none("/plugins/core/bot/challenger");

    gaim_prefs_add_string("/plugins/core/bot/challenger/question",
                          _("How do you spell the number 10?"));
    gaim_prefs_add_string("/plugins/core/bot/challenger/answer", _("ten"));

    gaim_prefs_add_bool("/plugins/core/bot/challenger/auto_add_permit",
                        FALSE);

    gaim_signal_connect(conv_handle, "receiving-im-msg",
                        plugin, GAIM_CALLBACK(receiving_im_msg_cb), NULL);

    return TRUE;
}

static GaimPluginInfo info = {
    GAIM_PLUGIN_MAGIC,
    GAIM_MAJOR_VERSION,
    GAIM_MINOR_VERSION,
    GAIM_PLUGIN_STANDARD,                                 /**< type           */
    NULL,                                                 /**< ui_requirement */
    0,                                                    /**< flags          */
    NULL,                                                 /**< dependencies   */
    GAIM_PRIORITY_DEFAULT,                                /**< priority       */

    N_("core-bot_challenger"),                            /**< id             */
    N_("Bot Challenger"),                                 /**< name           */
    VERSION,                                              /**< version        */
                                                          /**  summary        */
    N_("Block robots from sending Instant Messages"),
                                                          /**  description    */
    N_("A simple challenge-response system to permit only real people to send you instant messages"),
    "David Everly <deckrider@gmail.com>",                 /**< author         */
    "http://developer.berlios.de/projects/qrc/",          /**< homepage       */

    plugin_load,                                          /**< load           */
    NULL,                                                 /**< unload         */
    NULL,                                                 /**< destroy        */

    NULL,                                                 /**< ui_info        */
    NULL,                                                 /**< extra_info     */
    &prefs_info,                                          /**< prefs_info     */
    NULL
};

static void init_plugin(GaimPlugin * plugin)
{
    return;
}

GAIM_INIT_PLUGIN(bot_challenger, init_plugin, info)

/**
 * vim:tabstop=4:shiftwidth=4:expandtab:
 */
