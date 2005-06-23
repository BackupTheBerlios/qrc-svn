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
#include "internal.h"
#include "debug.h"
#include "privacy.h"
#include "util.h"

#include "gaympriv.h"
#include "gaym.h"

gboolean gaym_privacy_check(GaimConnection * gc, const char *nick)
{
    /**
     * returns TRUE if allowed through, FALSE otherwise
     */
    GSList *list;
    gboolean permitted = FALSE;

    switch (gc->account->perm_deny) {
    case 0:
        gaim_debug_warning("gaym", "Privacy setting was 0.  If you can "
                           "reproduce this, please file a bug report.\n");
        permitted = TRUE;
        break;

    case GAIM_PRIVACY_ALLOW_ALL:
        permitted = TRUE;
        break;

    case GAIM_PRIVACY_DENY_ALL:
        gaim_debug_info("gaym",
                        "%s blocked data received from %s (GAIM_PRIVACY_DENY_ALL)\n",
                        gc->account->username, nick);
        break;

    case GAIM_PRIVACY_ALLOW_USERS:
        for (list = gc->account->permit; list != NULL; list = list->next) {
            if (!gaim_utf8_strcasecmp
                (nick, gaim_normalize(gc->account, (char *) list->data))) {
                permitted = TRUE;
                gaim_debug_info("gaym",
                                "%s allowed data received from %s (GAIM_PRIVACY_ALLOW_USERS)\n",
                                gc->account->username, nick);
                break;
            }
        }
        break;

    case GAIM_PRIVACY_DENY_USERS:
        /* seeing we're letting everyone through, except the deny list */
        permitted = TRUE;
        for (list = gc->account->deny; list != NULL; list = list->next) {
            if (!gaim_utf8_strcasecmp
                (nick, gaim_normalize(gc->account, (char *) list->data))) {
                permitted = FALSE;
                gaim_debug_info("gaym",
                                "%s blocked data received from %s (GAIM_PRIVACY_DENY_USERS)\n",
                                gc->account->username, nick);
                break;
            }
        }
        break;

    case GAIM_PRIVACY_ALLOW_BUDDYLIST:
        if (gaim_find_buddy(gc->account, nick) != NULL) {
            permitted = TRUE;
        } else {
            gaim_debug_info("gaym",
                            "%s blocked data received from %s (GAIM_PRIVACY_ALLOW_BUDDYLIST)\n",
                            gc->account->username, nick);
        }
        break;

    default:
        gaim_debug_warning("gaym",
                           "Privacy setting was unknown.  If you can "
                           "reproduce this, please file a bug report.\n");
        permitted = FALSE;
        break;
    }

    /**
     * don't block/ignore self
     */
    if (!gaim_utf8_strcasecmp(gc->account->username, nick)) {
        permitted = TRUE;
        gaim_debug_info("gaym", "declining to block/ignore self\n");
        return permitted;
    }

    return permitted;
}

void gaym_privacy_change(GaimConnection * gc, const char *name)
{
    /**
     * don't allow adding self to permit/deny lists
     */

    if (name) {
        if (!gaim_utf8_strcasecmp(gc->account->username, name)) {
            gaim_privacy_deny_remove(gc->account, gc->account->username,
                                     TRUE);
            gaim_privacy_permit_remove(gc->account, gc->account->username,
                                       TRUE);
            gaim_debug_info("gaym",
                            "declining to add self to permit/deny list\n");
            return;
        }
    }

    GSList *rooms = NULL;
    for (rooms = gc->buddy_chats; rooms; rooms = rooms->next) {
        GaimConversation *convo = rooms->data;
        GaimConvChat *chat = gaim_conversation_get_chat_data(convo);
        GList *people = NULL;
        for (people = chat->in_room; people; people = people->next) {
            GaimConvChatBuddy *buddy = people->data;
            GaimConversationUiOps *ops =
                gaim_conversation_get_ui_ops(convo);
            if (name) {
                if (!gaim_utf8_strcasecmp(name, buddy->name)) {
                    if (gaym_privacy_check(gc, buddy->name)) {
                        gaim_conv_chat_unignore(GAIM_CONV_CHAT(convo),
                                                buddy->name);
                    } else {
                        gaim_conv_chat_ignore(GAIM_CONV_CHAT(convo),
                                              buddy->name);
                    }
                    ops->chat_update_user((convo), buddy->name);
                }
            } else {
                if (gaym_privacy_check(gc, buddy->name)) {
                    gaim_conv_chat_unignore(GAIM_CONV_CHAT(convo),
                                            buddy->name);
                } else {
                    gaim_conv_chat_ignore(GAIM_CONV_CHAT(convo),
                                          buddy->name);
                }
                ops->chat_update_user((convo), buddy->name);
            }
        }
    }
}

gboolean gaym_im_check(GaimConnection * gc, const char *nick,
                       const char *msg)
{
    gboolean retval = TRUE;

    /* not good, but don't do anything */
    if (!gc || !nick) {
        return retval;
    }

    /* there is already an open conversation, so it must be allowed */
    if (gaim_find_conversation_with_account(nick, gc->account)) {
        return retval;
    }

    /* user wants to allow only Buddies to IM */
    if (gaim_prefs_get_bool("/plugins/prpl/gaym/only_buddies_can_im")) {
        /* nick is not on the account's buddy list */
        if (!gaim_find_buddy(gc->account, nick)) {
            retval = FALSE;
            return retval;
        } else {
            return retval;
        }
    } else {
        /* don't make buddies use the challenge/response system */
        if (gaim_find_buddy(gc->account, nick)) {
            return retval;
        }
    }

    return retval;
}

void gaym_server_change_deny_status_cb(void *data, const char *result,
                                       size_t len)
{
    gaim_debug_info("gaym", "gaym_server_change_deny_status_cb:\n%s\n",
                    result);
    return;
}

void gaym_server_store_deny(GaimConnection * gc, const char *name,
                            gboolean add)
{
    char *action = NULL;
    if (add) {
        action = "add";
    } else {
        action = "remove";
    }

    struct gaym_conn *gaym = gc->proto_data;
    const char *server =
        gaim_account_get_string(gc->account, "server", IRC_DEFAULT_SERVER);

    char *url =
        g_strdup_printf
        ("http://%s/messenger/lists.txt?name=%s&key=%s&list=ignore&op=%s",
         server, name, gaym->hash_pw, action);

    char *user_agent = "Mozilla/4.0";

    gaim_url_fetch(url, FALSE, user_agent, FALSE,
                   gaym_server_change_deny_status_cb, NULL);

    g_free(url);
    return;
}

/**
 * vim:tabstop=4:shiftwidth=4:expandtab:
 */
