/**
 * GayM
 *
 * GayM is the legal property of its developers, whose names are too numerous
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

/* purple headers for this plugin */
#include "account.h"
#include "debug.h"
#include "privacy.h"
#include "util.h"

/* local headers for this plugin */
#include "gaym.h"
#include "gaympriv.h"
#include "helpers.h"

gboolean gaym_privacy_check(PurpleConnection * gc, const char *nick)
{
    /**
     * returns TRUE if allowed through, FALSE otherwise
     */
    GSList *list;
    gboolean permitted = FALSE;

    switch (gc->account->perm_deny) {
    case 0:
        purple_debug_warning("gaym", "Privacy setting was 0.  If you can "
                           "reproduce this, please file a bug report.\n");
        permitted = TRUE;
        break;

    case PURPLE_PRIVACY_ALLOW_ALL:
        permitted = TRUE;
        break;

    case PURPLE_PRIVACY_DENY_ALL:
        purple_debug_info("gaym",
                        "%s blocked data received from %s (PURPLE_PRIVACY_DENY_ALL)\n",
                        gc->account->username, nick);
        break;

    case PURPLE_PRIVACY_ALLOW_USERS:
        for (list = gc->account->permit; list != NULL; list = list->next) {
            if (!purple_utf8_strcasecmp
                (nick, purple_normalize(gc->account, (char *) list->data))) {
                permitted = TRUE;
                purple_debug_info("gaym",
                                "%s allowed data received from %s (PURPLE_PRIVACY_ALLOW_USERS)\n",
                                gc->account->username, nick);
                break;
            }
        }
        break;

    case PURPLE_PRIVACY_DENY_USERS:
        /* seeing we're letting everyone through, except the deny list */
        permitted = TRUE;
        for (list = gc->account->deny; list != NULL; list = list->next) {
            if (!purple_utf8_strcasecmp
                (nick, purple_normalize(gc->account, (char *) list->data))) {
                permitted = FALSE;
                purple_debug_info("gaym",
                                "%s blocked data received from %s (PURPLE_PRIVACY_DENY_USERS)\n",
                                gc->account->username, nick);
                break;
            }
        }
        break;

    case PURPLE_PRIVACY_ALLOW_BUDDYLIST:
        if (purple_find_buddy(gc->account, nick) != NULL) {
            permitted = TRUE;
        } else {
            purple_debug_info("gaym",
                            "%s blocked data received from %s (PURPLE_PRIVACY_ALLOW_BUDDYLIST)\n",
                            gc->account->username, nick);
        }
        break;

    default:
        purple_debug_warning("gaym",
                           "Privacy setting was unknown.  If you can "
                           "reproduce this, please file a bug report.\n");
        permitted = FALSE;
        break;
    }

    /**
     * don't block/ignore self
     */
    if (!purple_utf8_strcasecmp(gc->account->username, nick)) {
        permitted = TRUE;
        purple_debug_info("gaym", "declining to block/ignore self\n");
        return permitted;
    }

    return permitted;
}

void gaym_privacy_change(PurpleConnection * gc, const char *name)
{
    /**
     * don't allow adding self to permit/deny lists
     */

    if (name) {
        if (!purple_utf8_strcasecmp(gc->account->username, name)) {
            purple_privacy_deny_remove(gc->account, gc->account->username,
                                     TRUE);
            purple_privacy_permit_remove(gc->account, gc->account->username,
                                       TRUE);
            purple_debug_info("gaym",
                            "declining to add self to permit/deny list\n");
            return;
        }
    }

    GSList *rooms = NULL;
    for (rooms = gc->buddy_chats; rooms; rooms = rooms->next) {
        PurpleConversation *convo = rooms->data;
        PurpleConvChat *chat = purple_conversation_get_chat_data(convo);
        GList *people = NULL;
        for (people = chat->in_room; people; people = people->next) {
            PurpleConvChatBuddy *buddy = people->data;
            PurpleConversationUiOps *ops =
                purple_conversation_get_ui_ops(convo);
            if (name) {
                if (!purple_utf8_strcasecmp(name, buddy->name)) {
                    if (gaym_privacy_check(gc, buddy->name)) {
                        purple_conv_chat_unignore(PURPLE_CONV_CHAT(convo),
                                                buddy->name);
                    } else {
                        purple_conv_chat_ignore(PURPLE_CONV_CHAT(convo),
                                              buddy->name);
                    }
                    ops->chat_update_user((convo), buddy->name);
                }
            } else {
                if (gaym_privacy_check(gc, buddy->name)) {
                    purple_conv_chat_unignore(PURPLE_CONV_CHAT(convo),
                                            buddy->name);
                } else {
                    purple_conv_chat_ignore(PURPLE_CONV_CHAT(convo),
                                          buddy->name);
                }
                ops->chat_update_user((convo), buddy->name);
            }
        }
    }
}

void gaym_server_change_deny_status_cb(PurpleUtilFetchUrlData *url_data, void *data, const gchar *result,
                                       gsize len, const gchar* error_message)
{
    purple_debug_info("gaym", "gaym_server_change_deny_status_cb:\n%s\n",
                    result);
    return;
}

void gaym_server_store_deny(PurpleConnection * gc, const char *name,
                            gboolean add)
{
    char *action = NULL;
    if (add) {
        action = "add";
    } else {
        action = "remove";
    }

    struct gaym_conn *gaym = gc->proto_data;

    char *hashurl =
        g_hash_table_lookup(gaym->confighash, "list-operations-url");
    g_return_if_fail(hashurl != NULL);

    char *url =
        g_strdup_printf("%s?name=%s&key=%s&list=ignore&op=%s", hashurl,
                        name, gaym->chat_key, action);

    char *user_agent = "Mozilla/4.0";

    purple_util_fetch_url(url, FALSE, user_agent, FALSE,
                   gaym_server_change_deny_status_cb, NULL);

    g_free(url);
    return;
}

void synchronize_deny_list(PurpleConnection * gc, GHashTable * confighash)
{
    char *srvdeny = NULL;
    gchar **srvdenylist = NULL;
    GSList *list;
    gint i = 0;
    gboolean needsync = FALSE;

    g_return_if_fail(confighash != NULL);

    srvdeny =
        g_hash_table_lookup(confighash, "connect-list.ignore.members");
    if (!srvdeny) {
        srvdeny = "";
    }
    srvdenylist = g_strsplit(srvdeny, ",", -1);

    /**
     * The nicks come in here as if they came from the IRC server
     * so they need to be converted to GayM format
     */
    for (i = 0; srvdenylist[i]; i++) {
        gcom_nick_to_gaym(srvdenylist[i]);
    }

    /* Add server deny list from config.txt to local deny list */
    for (i = 0; srvdenylist[i]; i++) {
        needsync = TRUE;
        for (list = gc->account->deny; list != NULL; list = list->next) {
            if (!purple_utf8_strcasecmp
                (srvdenylist[i],
                 purple_normalize(gc->account, (char *) list->data))) {
                needsync = FALSE;
                break;
            }
        }
        if (needsync) {
            if (!purple_privacy_deny_add(gc->account, srvdenylist[i], TRUE)) {
                purple_debug_error("gaym",
                                 "Failed to add %s to local deny list from server.\n",
                                 srvdenylist[i]);
            } else {
                purple_debug_misc("gaym",
                                "Added %s to local deny list from server.\n",
                                srvdenylist[i]);
            }
        }
    }

    /* Add local deny list not found in config.txt to server deny list */
    for (list = gc->account->deny; list != NULL; list = list->next) {
        needsync = TRUE;
        for (i = 0; srvdenylist[i]; i++) {
            if (!purple_utf8_strcasecmp
                (srvdenylist[i],
                 purple_normalize(gc->account, (char *) list->data))) {
                needsync = FALSE;
                break;
            }
        }
        if (needsync) {
            gaym_server_store_deny(gc, (char *) list->data, TRUE);
        }
    }

    g_strfreev(srvdenylist);
    return;
}

/**
 * vim:tabstop=4:shiftwidth=4:expandtab:
 */
