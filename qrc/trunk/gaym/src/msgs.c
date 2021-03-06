/**
 * @file msgs.c
 *
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

#include "conversation.h"
#include "blist.h"
#include "notify.h"
#include "util.h"
#include "debug.h"
#include "imgstore.h"
#include "request.h"
#include "privacy.h"
#include "prefs.h"

#include "gaym.h"
#include "gayminfo.h"
#include "gaympriv.h"
#include "helpers.h"

static char *gaym_mask_nick(const char *mask)
{
    char *end, *buf;

    end = strchr(mask, '!');
    if (!end)
        buf = g_strdup(mask);
    else
        buf = g_strndup(mask, end - mask);

    return buf;
}

static void gaym_chat_remove_buddy(PurpleConversation * convo, char *data[2])
{
    /**
     * FIXME: is *message ever used ???
     */
    char *message = g_strdup_printf("quit: %s", data[1]);

    if (purple_conv_chat_find_user(PURPLE_CONV_CHAT(convo), data[0]))
        purple_conv_chat_remove_user(PURPLE_CONV_CHAT(convo), data[0], NULL);

    g_free(message);
}

void gaym_msg_default(struct gaym_conn *gaym, const char *name,
                      const char *from, char **args)
{
    purple_debug(PURPLE_DEBUG_INFO, "gaym", "Unrecognized message: %s\n",
               args[0]);
}

void gaym_msg_away(struct gaym_conn *gaym, const char *name,
                   const char *from, char **args)
{
    PurpleConnection *gc = purple_account_get_connection(gaym->account);

    if (!args || !args[1] || !gc) {
        return;
    }

    gcom_nick_to_gaym(args[1]);
    serv_got_im(gc, args[1], args[2], PURPLE_MESSAGE_AUTO_RESP, time(NULL));
}

static void gaym_fetch_photo_cb(PurpleUtilFetchUrlData *url_data, gpointer user_data, const gchar *url_text, gsize len, const gchar* err)
{
    if (!url_text || !user_data) {
        return;
    }

    struct get_info_data *d = user_data;
    struct gaym_conn *gaym = d->gaym;

    int id = purple_imgstore_add_with_id(g_memdup(url_text, len), len, NULL);
    char* imghtml=g_strdup_printf("<img id=\"%d\">",id);
    purple_debug_misc("userinfo","img html: %s\n",imghtml);
     
    if(g_hash_table_lookup(gaym->info_window_needed, d->who))
    {
        purple_notify_user_info_add_pair(d->info, NULL, imghtml);
        purple_notify_userinfo(d->gc, d->who, d->info, NULL, NULL); 
        g_hash_table_remove(gaym->info_window_needed, d->who);   
    }
    //purple_notify_user_info_destroy(d->info);
    g_free(imghtml);
}

static void gaym_fetch_info_cb(PurpleUtilFetchUrlData *url_data, void *user_data, const gchar *info_data, gsize len, const gchar* error_message)
{
    struct get_info_data *d = user_data;
    char *picpath=NULL;
    char *picurl=NULL;
    char *match = "pictures.0.url=";

    purple_debug_misc("fetch info cb","Starting\n");
    picpath = return_string_between(match, "\n", info_data);
    if(picpath)
        picurl = g_strdup_printf("http://www.gay.com%s", picpath);

    void *needed = g_hash_table_lookup(d->gaym->info_window_needed, d->who);
    if (picurl && g_hash_table_lookup(d->gaym->info_window_needed, d->who)) {
        purple_debug_misc("msgs", "Picture url: %s\nNeeded? %x", picurl, needed);
        purple_util_fetch_url(picurl, FALSE, "Mozilla/4.0 (compatible; MSIE 5.0)",
                       FALSE, gaym_fetch_photo_cb, d);
        return;
        g_free(picurl);
        g_free(picpath);
    }
   
    else {
        g_hash_table_remove(d->gaym->info_window_needed, d->who);
        //purple_notify_user_info_destroy(d->info);
        
    }



}

void gaym_msg_no_such_nick(struct gaym_conn *gaym, const char *name,
                           const char *from, char **args)
{
    /**
     * name = 701
     * from = irc.server.name
     * args[1] = the nick that wasn't found
     */

    if (!gaym || !args || !args[1]) {
        return;
    }

    gcom_nick_to_gaym(args[1]);

    gaym_buddy_status(gaym, args[1], FALSE, NULL, FALSE);

    char *normalized = g_strdup(purple_normalize(gaym->account, args[1]));

    void *dialog;
    if ((dialog =
         g_hash_table_lookup(gaym->info_window_needed, normalized))) {
        g_hash_table_remove(gaym->info_window_needed, normalized);

        PurpleNotifyUserInfo *info = purple_notify_user_info_new();
        char *hashurl =
            g_hash_table_lookup(gaym->confighash, "view-profile-url");
        g_return_if_fail(hashurl != NULL);
        char *proflink = g_strdup_printf("<a href='%s%s'>Check for Full Profile</a>",hashurl,args[1]);
        g_free(hashurl);
        purple_notify_user_info_add_pair(info, NULL, "No such user online.");
        purple_notify_user_info_add_pair(info, NULL, proflink);
        
        purple_notify_userinfo(purple_account_get_connection(gaym->account),
                             args[1], info, NULL, NULL);

    }
    g_free(normalized);
}

void gaym_msg_whois(struct gaym_conn *gaym, const char *name,
                    const char *from, char **args)
{
    /**
     * name = 311
     * from = irc.server.name
     * args[1] = the nick that we have information about
     */

    if (!gaym || !args || !args[1]) {
        return;
    }

    char* nick = args[1];
    char* info = args[5];

    gcom_nick_to_gaym(nick);

    char* stats = gaym_stats_strdup(info);
    char* bio = gaym_bio_strdup(info);
    
    gaym_buddy_status(gaym, nick, TRUE, info, TRUE);

    char *normalized = g_strdup(purple_normalize(gaym->account, nick));


    // Update, but then release the reference. It was already opened
    // during conversation-created.
    //gaym_update_channel_member(gaym, normalized, info);
    //gaym_unreference_channel_member(gaym, normalized);

    //purple_debug_misc("gaym", "signalling info update for %s\n", normalized);
    //purple_signal_emit(purple_accounts_get_handle(), "info-updated",
    //                 gaym->account, normalized);
    
    struct get_info_data* d;
    if ((d = g_hash_table_lookup(gaym->info_window_needed, normalized))) {

        
        char *hashurl;
        hashurl = g_strdup(g_hash_table_lookup(gaym->confighash, "view-profile-url"));
        g_return_if_fail(hashurl != NULL);
        char *proflink = g_strdup_printf("<a href='%s%s'>Full Profile</a>",hashurl,nick);
        
        d->info = purple_notify_user_info_new();
        purple_notify_user_info_add_pair(d->info, NULL, proflink);
        purple_notify_user_info_add_pair(d->info, "Stats", stats?stats:"Not Found");
        purple_notify_user_info_add_pair(d->info, "Bio", bio?bio:"Not Found");
        purple_notify_userinfo(d->gc, nick, d->info, NULL, d); 
        purple_debug_misc("msg_whois","Updated userinfo info\n");
        g_free(hashurl);
        g_free(proflink);

        hashurl = g_hash_table_lookup(gaym->confighash,
                                            "ohm.profile-url");
        g_return_if_fail(hashurl != NULL);
        char *infourl = g_strdup_printf("%s?pw=%s&name=%s", hashurl,
                                        gaym->chat_key, nick);
        if (infourl) {
            purple_debug_misc("msgs","Fetching %s\n",infourl);
            purple_util_fetch_url(infourl, FALSE,
                           "Mozilla/4.0 (compatible; MSIE 5.0)", FALSE,
                           gaym_fetch_info_cb, d);
            g_free(infourl);
        } 
        else {
            g_hash_table_remove(gaym->info_window_needed, normalized);
            purple_notify_user_info_destroy(d->info);
        }
    }
    g_free(normalized);
}

void gaym_msg_login_failed(struct gaym_conn *gaym, const char *name,
                           const char *from, char **args)
{



    gaym_cmd_quit(gaym, "quit", NULL, NULL);

    // if (gc->inpa)
    // purple_input_remove(gc->inpa);

    // g_free(gaym->inbuf);
    // purple_debug_misc("gaym", "Login failed. closing fd %i\n", gaym->fd);
    // close(gaym->fd);
    // purple_debug_misc("gaym", "Get chatkey from weblogin\n");
    // gaym_get_hash_from_weblogin(gaym->account,
    // gaym_login_with_chat_key);

}

void gaym_msg_list(struct gaym_conn *gaym, const char *name,
                   const char *from, char **args)
{
    /**
     * If you free anything here related to the roomlist
     * be sure you test what happens when the roomlist reference
     * count goes to zero! Because it may crash purple.
     */
    if (!gaym->roomlist) {
        return;
    }
    /**
     * Begin result of member created room list
     */
    if (!strcmp(name, "321") && gaym->roomlist_filter == NULL) {
        PurpleRoomlistRoom *room;
        room = purple_roomlist_room_new(PURPLE_ROOMLIST_ROOMTYPE_CATEGORY,
                                      _("Member Created"), NULL);
        purple_roomlist_room_add(gaym->roomlist, room);
        purple_roomlist_set_in_progress(gaym->roomlist, TRUE);
        return;
    }

    /**
     * The list of member created rooms
     */
    if (!strcmp(name, "322")) {
        PurpleRoomlistRoom *room;
        char *field_start = NULL;
        char *field_end = NULL;
        size_t field_len = 0;
        int i = 0;

        if (!args[1]) {
            return;
        }

        /**
         * strip leading "#_" and trailing "=1"
         */
        field_start = strchr(args[1], '_');
        field_end = strrchr(args[1], '=');

        if (!field_start || !field_end) {
            purple_debug_error("gaym",
                             "Member created room list parsing error");
            return;
        }
        field_start++;
        field_end = field_end + 2;

        field_len = field_end - field_start;

        char *field_name = g_strndup(field_start, field_len);

        /**
         * replace all remaining "_" with " "
         */
        for (i = 0; field_name[i] != '\0'; i++) {
            if (field_name[i] == '_') {
                field_name[i] = ' ';
            }
        }
        /**
         * replace '=' with ':'
         */
        field_name[i - 2] = ':';

        gchar *lowercase = g_utf8_strdown(field_name, -1);
        gchar *normalized =
            g_utf8_normalize(lowercase, -1, G_NORMALIZE_ALL);
        g_free(lowercase);
        if (gaym->roomlist_filter == NULL ||
            g_strstr_len(normalized, -1, gaym->roomlist_filter) != NULL) {

            room = purple_roomlist_room_new(PURPLE_ROOMLIST_ROOMTYPE_ROOM,
                                          field_name,
                                          g_list_nth_data(gaym->roomlist->
                                                          rooms, 0));
            purple_roomlist_room_add_field(gaym->roomlist, room, field_name);
            purple_roomlist_room_add_field(gaym->roomlist, room, args[1]);
            purple_roomlist_room_add(gaym->roomlist, room);
        }
        g_free(normalized);
        g_free(field_name);
    }

    /**
     * End result of member created room list
     * This is our trigger to add the static rooms
     */
    if (!strcmp(name, "323")) {
        build_roomlist_from_config(gaym->roomlist, gaym->confighash,
                                   gaym->roomlist_filter);
        if (gaym->roomlist_filter) {
            g_free(gaym->roomlist_filter);
            gaym->roomlist_filter = NULL;
        }
        return;
    }
}

void gaym_msg_unknown(struct gaym_conn *gaym, const char *name,
                      const char *from, char **args)
{
    PurpleConnection *gc = purple_account_get_connection(gaym->account);
    char *buf;

    if (!args || !args[1] || !gc)
        return;

    buf = g_strdup_printf(_("Unknown message '%s'"), args[1]);
    purple_notify_error(gc, _("Unknown message"), buf,
                      _
                      ("Purple has sent a message the IRC server did not understand."));
    g_free(buf);
}

void gaym_msg_names(struct gaym_conn *gaym, const char *name,
                    const char *from, char **args)
{
    char *names, *cur, *end, *tmp, *msg;
    PurpleConversation *convo;
    purple_debug_misc("names", "%s %s %s %s", name, from, args[1], args[2]);
    if (!strcmp(name, "366")) {
        GaymNamelist *namelist = g_queue_peek_head(gaym->namelists);
        purple_debug_misc("names", "namelist->roomname:%s\n",
                        namelist->roomname);
        if (namelist
            && !strncmp(namelist->roomname, args[1],
                        strlen(namelist->roomname))) {
            purple_debug_misc("names",
                            "*****Got all names responses for %s\n",
                            args[1]);
            GaymNamelist *namelist = g_queue_pop_head(gaym->namelists);
            purple_debug_misc("msgs",
                            "should be emitting namelist-complete signal passing namelist %x\n",
                            namelist);
            purple_signal_emit(purple_accounts_get_handle(),
                             "namelist-complete", gaym->account, namelist);
            return;
        }
        if (!gaym->nameconv)
            return;
        convo =
            purple_find_conversation_with_account(PURPLE_CONV_TYPE_CHAT,
                                                gaym->nameconv ? gaym->
                                                nameconv : args[1],
                                                gaym->account);
        if (!convo) {
            purple_debug(PURPLE_DEBUG_ERROR, "gaym",
                       "Got a NAMES list for %s, which doesn't exist\n",
                       args[1]);
            g_string_free(gaym->names, TRUE);
            gaym->names = NULL;
            g_free(gaym->nameconv);
            gaym->nameconv = NULL;
            return;
        }

        names = cur = g_string_free(gaym->names, FALSE);
        gaym->names = NULL;
        if (gaym->nameconv) {
            msg =
                g_strdup_printf(_("Users on %s: %s"),
                                args[1] ? args[1] : "",
                                names ? names : "");
            if (purple_conversation_get_type(convo) == PURPLE_CONV_TYPE_CHAT)
                purple_conv_chat_write(PURPLE_CONV_CHAT(convo), "", msg,
                                     PURPLE_MESSAGE_SYSTEM |
                                     PURPLE_MESSAGE_NO_LOG, time(NULL));
            else
                purple_conv_im_write(PURPLE_CONV_IM(convo), "", msg,
                                   PURPLE_MESSAGE_SYSTEM |
                                   PURPLE_MESSAGE_NO_LOG, time(NULL));
            g_free(msg);
            g_free(gaym->nameconv);
            gaym->nameconv = NULL;
        } else {
            GList *users = NULL;

            while (*cur) {
                end = strchr(cur, ' ');
                tmp = g_strndup(cur, end - cur);
                gcom_nick_to_gaym(tmp);
                users = g_list_prepend(users, tmp);
                cur = end;
                if (*cur)
                    cur++;
            }
            users = g_list_reverse(users);

            if (users != NULL) {
                GList *l;

                purple_conv_chat_add_users(PURPLE_CONV_CHAT(convo), users,
                                         NULL, NULL, FALSE);

                for (l = users; l != NULL; l = l->next)
                    g_free(l->data);

                g_list_free(users);
            }
        }
        g_free(names);
    } else {
        if (gaym->nameconv && !gaym->names) {
            gaym->names = g_string_new("");
            gaym->names = g_string_append(gaym->names, args[3]);
        }
        purple_debug_misc("names", "Response: %s\n", args[3]);
        GaymNamelist *nameslist = g_queue_peek_head(gaym->namelists);
        if (nameslist) {
            gchar **names = g_strsplit(args[3], " ", -1);


            int i = 0;
            purple_debug_misc("names",
                            "names[i]: %s, nameslist->current: %x\n",
                            names[i], nameslist->current);
            while (names[i] && strlen(names[i]) && nameslist->current) {
                ((GaymBuddy *) (nameslist->current->data))->name =
                    g_strdup(names[i]);
                nameslist->current = g_slist_next(nameslist->current);
                i++;
            }
            g_strfreev(names);

        }
    }
}

/**
 * Change this to WELCOME
 */

void gaym_msg_endmotd(struct gaym_conn *gaym, const char *name,
                      const char *from, char **args)
{
    PurpleConnection *gc;

    PurpleBlistNode *gnode, *cnode, *bnode;
    purple_debug_misc("gaym", "Got motd\n");

    gc = purple_account_get_connection(gaym->account);
    if (!gc) {
        purple_debug_misc("gaym", "!gc ???\n");
        return;
    }
    purple_connection_set_state(gc, PURPLE_CONNECTED);
    // serv_finish_login(gc);
    /* this used to be in the core, but it's not now */
    for (gnode = purple_get_blist()->root; gnode; gnode = gnode->next) {
        if (!PURPLE_BLIST_NODE_IS_GROUP(gnode))
            continue;
        for (cnode = gnode->child; cnode; cnode = cnode->next) {
            if (!PURPLE_BLIST_NODE_IS_CONTACT(cnode))
                continue;
            for (bnode = cnode->child; bnode; bnode = bnode->next) {
                PurpleBuddy *b;
                if (!PURPLE_BLIST_NODE_IS_BUDDY(bnode))
                    continue;
                b = (PurpleBuddy *) bnode;
                if (b->account == gc->account) {
                    struct gaym_buddy *ib = g_new0(struct gaym_buddy, 1);
                    ib->name = g_strdup(b->name);
                    g_hash_table_insert(gaym->buddies, ib->name, ib);
                }
            }
        }
    }

    purple_debug_misc("gaym", "Calling blist timeout\n");
    gaym_blist_timeout(gaym);
    if (!gaym->timer)
        gaym->timer =
            purple_timeout_add(BLIST_UPDATE_PERIOD,
                             (GSourceFunc) gaym_blist_timeout,
                             (gpointer) gaym);
}

void gaym_msg_nochan(struct gaym_conn *gaym, const char *name,
                     const char *from, char **args)
{
    PurpleConnection *gc = purple_account_get_connection(gaym->account);

    if (gc == NULL || args == NULL || args[1] == NULL)
        return;

    purple_notify_error(gc, NULL, _("No such channel"), args[1]);
}

void gaym_msg_nonick_chan(struct gaym_conn *gaym, const char *name,
                          const char *from, char **args)
{
    PurpleConnection *gc = purple_account_get_connection(gaym->account);
    PurpleConversation *convo;

    convo =
        purple_find_conversation_with_account(PURPLE_CONV_TYPE_ANY, args[1],
                                            gaym->account);
    if (convo) {
        if (purple_conversation_get_type(convo) == PURPLE_CONV_TYPE_CHAT) {
            /* does this happen? */
            purple_conv_chat_write(PURPLE_CONV_CHAT(convo), args[1],
                                 _("no such channel"),
                                 PURPLE_MESSAGE_SYSTEM | PURPLE_MESSAGE_NO_LOG,
                                 time(NULL));
        } else {
            purple_conv_im_write(PURPLE_CONV_IM(convo), args[1],
                               _("User is not logged in"),
                               PURPLE_MESSAGE_SYSTEM | PURPLE_MESSAGE_NO_LOG,
                               time(NULL));
        }
    } else {
        if ((gc = purple_account_get_connection(gaym->account)) == NULL)
            return;
        purple_notify_error(gc, NULL, _("Not logged in: "), args[1]);
    }

    if (gc == NULL || args == NULL || args[1] == NULL)
        return;


}

void gaym_msg_nonick(struct gaym_conn *gaym, const char *name,
                     const char *from, char **args)
{
    PurpleConnection *gc;
    PurpleConversation *convo;

    convo =
        purple_find_conversation_with_account(PURPLE_CONV_TYPE_ANY, args[1],
                                            gaym->account);
    if (convo) {
        if (purple_conversation_get_type(convo) == PURPLE_CONV_TYPE_CHAT) {
            /* does this happen? */
            purple_conv_chat_write(PURPLE_CONV_CHAT(convo), args[1],
                                 _("no such channel"),
                                 PURPLE_MESSAGE_SYSTEM | PURPLE_MESSAGE_NO_LOG,
                                 time(NULL));
        } else {
            purple_conv_im_write(PURPLE_CONV_IM(convo), args[1],
                               _("User is not logged in"),
                               PURPLE_MESSAGE_SYSTEM | PURPLE_MESSAGE_NO_LOG,
                               time(NULL));
        }
    } else {
        if ((gc = purple_account_get_connection(gaym->account)) == NULL)
            return;
        purple_notify_error(gc, NULL, _("No such nick or channel"), args[1]);
    }
}

void gaym_msg_nosend(struct gaym_conn *gaym, const char *name,
                     const char *from, char **args)
{
    PurpleConnection *gc;
    PurpleConversation *convo;

    convo =
        purple_find_conversation_with_account(PURPLE_CONV_TYPE_CHAT, args[1],
                                            gaym->account);
    if (convo) {
        purple_conv_chat_write(PURPLE_CONV_CHAT(convo), args[1], args[2],
                             PURPLE_MESSAGE_SYSTEM | PURPLE_MESSAGE_NO_LOG,
                             time(NULL));
    } else {
        if ((gc = purple_account_get_connection(gaym->account)) == NULL)
            return;
        purple_notify_error(gc, NULL, _("Could not send"), args[2]);
    }
}

/**
 * Is this used?
 */
void gaym_msg_notinchan(struct gaym_conn *gaym, const char *name,
                        const char *from, char **args)
{
    PurpleConversation *convo =
        purple_find_conversation_with_account(PURPLE_CONV_TYPE_CHAT, args[1],
                                            gaym->account);

    purple_debug(PURPLE_DEBUG_INFO, "gaym",
               "We're apparently not in %s, but tried to use it\n",
               args[1]);
    if (convo) {
        /* g_slist_remove(gaym->gc->buddy_chats, convo);
           purple_conversation_set_account(convo, NULL); */
        purple_conv_chat_write(PURPLE_CONV_CHAT(convo), args[1], args[2],
                             PURPLE_MESSAGE_SYSTEM | PURPLE_MESSAGE_NO_LOG,
                             time(NULL));
    }
}

/**
 * Invite WORKS in gay.com!
 */
void gaym_msg_invite(struct gaym_conn *gaym, const char *name,
                     const char *from, char **args)
{
    PurpleConnection *gc = purple_account_get_connection(gaym->account);
    char *nick = gaym_mask_nick(from);
    GHashTable *components =
        g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

    if (!args || !args[1] || !gc) {
        g_free(nick);
        g_hash_table_destroy(components);
        return;
    }

    if (!gaym_privacy_check(gc, nick)) {
        g_free(nick);
        g_hash_table_destroy(components);
        return;
    }

    g_hash_table_insert(components, strdup("channel"), strdup(args[1]));
    gcom_nick_to_gaym(nick);
    serv_got_chat_invite(gc, args[1], nick, NULL, components);

    g_free(nick);
}

void gaym_msg_inviteonly(struct gaym_conn *gaym, const char *name,
                         const char *from, char **args)
{
    PurpleConnection *gc = purple_account_get_connection(gaym->account);
    char *buf;

    if (!args || !args[1] || !gc)
        return;

    buf =
        g_strdup_printf(_("Joining %s requires an invitation."), args[1]);
    purple_notify_error(gc, _("Invitation only"), _("Invitation only"), buf);
    g_free(buf);
}

void gaym_msg_trace(struct gaym_conn *gaym, const char *name,
                    const char *from, char **args)
{
    PurpleConversation *conv =
        purple_find_conversation_with_account(PURPLE_CONV_TYPE_ANY,
                                            gaym->traceconv ? gaym->
                                            traceconv : args[1],
                                            gaym->account);
    purple_conversation_write(conv, "TRACE", args[3],
                            PURPLE_MESSAGE_SYSTEM | PURPLE_MESSAGE_NO_LOG,
                            time(NULL));

}

void gaym_msg_join(struct gaym_conn *gaym, const char *name,
                   const char *from, char **args)
{
    purple_debug_misc("join", "got join for %s\n", args[0]);
    PurpleConnection *gc = purple_account_get_connection(gaym->account);
    g_return_if_fail(gc != NULL);

    char *nick = gaym_mask_nick(from);

    PurpleConversation *convo;
    PurpleConvChatBuddyFlags flags = PURPLE_CBFLAGS_NONE;
    char *bio = NULL;
    char *bio_markedup = NULL;
    static int id = 1;

    gcom_nick_to_gaym(nick);
    if (!purple_utf8_strcasecmp(nick, purple_connection_get_display_name(gc))) {
        /* We are joining a channel for the first time */

        gpointer data, unused;
        gboolean hammering = g_hash_table_lookup_extended
            (gaym->hammers, args[0], &unused, &data);
        // There was a hammer, but it is cancelled. Leave!
        purple_debug_misc("join", "Joined %s\n", args[0]);
        if (hammering && !data) {       // hammer was cancelled.
            purple_debug_misc("gaym",
                            "JOINED, BUT HAMMER CANCELLED: ABORT!!!!\n");
            g_hash_table_remove(gaym->hammers, args[0]);
            gaym_cmd_part(gaym, NULL, NULL, (const char **) args);
            return;
        }

        g_hash_table_remove(gaym->hammers, args[0]);
        serv_got_joined_chat(gc, id++, args[0]);

        gint *entry = g_new(gint, 1);
        *entry = MAX_CHANNEL_MEMBERS;
        g_hash_table_insert(gaym->entry_order, g_strdup(args[0]), entry);

        g_free(nick);
        return;
    }

    convo =
        purple_find_conversation_with_account(PURPLE_CONV_TYPE_ANY, args[0],
                                            gaym->account);
    if (convo == NULL) {
        purple_debug(PURPLE_DEBUG_ERROR, "gaym", "JOIN for %s failed\n",
                   args[0]);
        g_free(nick);
        return;
    }

    gint *entry = g_hash_table_lookup(gaym->entry_order, args[0]);
    g_return_if_fail(entry != NULL);

    gaym_buddy_status(gaym, nick, TRUE, args[1], TRUE);



    bio = gaym_bio_strdup(args[1]);
    if (bio) {
        bio_markedup = purple_markup_linkify(bio);
        g_free(bio);
    }

    if (*entry <= MAX_CHANNEL_MEMBERS) {
        *entry = MAX_CHANNEL_MEMBERS + 1;
    }

    flags = chat_pecking_order(args[1]);
    flags = include_chat_entry_order(flags, (*entry)++);

    gboolean gaym_privacy_permit = gaym_privacy_check(gc, nick);
    gboolean show_join =
        purple_prefs_get_bool("/plugins/prpl/gaym/show_join");

    if (purple_prefs_get_bool("/plugins/prpl/gaym/show_bio_with_join")) {
        purple_conv_chat_add_user(PURPLE_CONV_CHAT(convo), nick, bio_markedup,
                                flags, (gaym_privacy_permit
                                        && show_join));
    } else {
        purple_conv_chat_add_user(PURPLE_CONV_CHAT(convo), nick, NULL,
                                flags, (gaym_privacy_permit
                                        && show_join));
    }

    /**
     * Make the ignore.png icon appear next to the nick.
     */
    PurpleConversationUiOps *ops = purple_conversation_get_ui_ops(convo);
    if (gaym_privacy_permit) {
        purple_conv_chat_unignore(PURPLE_CONV_CHAT(convo), nick);
    } else {
        purple_conv_chat_ignore(PURPLE_CONV_CHAT(convo), nick);
    }
    ops->chat_update_user((convo), nick);

    gaym_update_channel_member(gaym, nick, args[1]);
    g_free(bio_markedup);
    g_free(nick);
}

void gaym_msg_mode(struct gaym_conn *gaym, const char *name,
                   const char *from, char **args)
{
    PurpleConversation *convo;
    char *nick = gaym_mask_nick(from), *buf;

    if (*args[0] == '#' || *args[0] == '&') {   /* Channel */
        convo =
            purple_find_conversation_with_account(PURPLE_CONV_TYPE_ANY,
                                                args[0], gaym->account);
        if (!convo) {
            purple_debug(PURPLE_DEBUG_ERROR, "gaym",
                       "MODE received for %s, which we are not in\n",
                       args[0]);
            g_free(nick);
            return;
        }
        buf =
            g_strdup_printf(_("mode (%s %s) by %s"), args[1],
                            args[2] ? args[2] : "", nick);
        purple_conv_chat_write(PURPLE_CONV_CHAT(convo), args[0], buf,
                             PURPLE_MESSAGE_SYSTEM | PURPLE_MESSAGE_NO_LOG,
                             time(NULL));
        g_free(buf);
        if (args[2]) {
            PurpleConvChatBuddyFlags newflag, flags;
            char *mcur, *cur, *end, *user;
            gboolean add = FALSE;
            mcur = args[1];
            cur = args[2];
            while (*cur && *mcur) {
                if ((*mcur == '+') || (*mcur == '-')) {
                    add = (*mcur == '+') ? TRUE : FALSE;
                    mcur++;
                    continue;
                }
                end = strchr(cur, ' ');
                if (!end)
                    end = cur + strlen(cur);
                user = g_strndup(cur, end - cur);
                flags =
                    purple_conv_chat_user_get_flags(PURPLE_CONV_CHAT(convo),
                                                  user);
                newflag = PURPLE_CBFLAGS_NONE;
                if (*mcur == 'o')
                    newflag = PURPLE_CBFLAGS_OP;
                else if (*mcur == 'h')
                    newflag = PURPLE_CBFLAGS_HALFOP;
                else if (*mcur == 'v')
                    newflag = PURPLE_CBFLAGS_VOICE;
                if (newflag) {
                    if (add)
                        flags |= newflag;
                    else
                        flags &= ~newflag;
                    purple_conv_chat_user_set_flags(PURPLE_CONV_CHAT(convo),
                                                  user, flags);
                }
                g_free(user);
                cur = end;
                if (*cur)
                    cur++;
                if (*mcur)
                    mcur++;
            }
        }
    } else {                    /* User */
    }
    g_free(nick);
}

void gaym_msg_nick(struct gaym_conn *gaym, const char *name,
                   const char *from, char **args)
{
    GSList *chats;

    PurpleConnection *gc = purple_account_get_connection(gaym->account);
    char *nick = gaym_mask_nick(from);

    if (!gc) {
        g_free(nick);
        return;
    }

    chats = gc->buddy_chats;

    if (!purple_utf8_strcasecmp(nick, purple_connection_get_display_name(gc))) {
        purple_connection_set_display_name(gc, args[0]);
    }

    while (chats) {
        PurpleConvChat *chat = PURPLE_CONV_CHAT(chats->data);
        /* This is ugly ... */
        if (purple_conv_chat_find_user(chat, nick))
            purple_conv_chat_rename_user(chat, nick, args[0]);
        chats = chats->next;
    }
    g_free(nick);
}

void gaym_msg_notice(struct gaym_conn *gaym, const char *name,
                     const char *from, char **args)
{
    PurpleConnection *gc = purple_account_get_connection(gaym->account);

    if (!gc) {
        return;
    }

    char *newargs[2];

    newargs[0] = " notice ";    /* The spaces are magic, leave 'em in! */
    newargs[1] = args[1];
    gaym_msg_privmsg(gaym, name, from, newargs);
}

void gaym_msg_part(struct gaym_conn *gaym, const char *name,
                   const char *from, char **args)
{
    PurpleConversation *convo;
    char *msg;

    PurpleConnection *gc = purple_account_get_connection(gaym->account);
    char *nick = gaym_mask_nick(from);

    if (!args || !args[0] || !gc || !nick) {
        g_free(nick);
        return;
    }

    convo =
        purple_find_conversation_with_account(PURPLE_CONV_TYPE_ANY, args[0],
                                            gaym->account);
    gboolean show_part =
        purple_prefs_get_bool("/plugins/prpl/gaym/show_part");

    gcom_nick_to_gaym(nick);
    if (!purple_utf8_strcasecmp(nick, purple_connection_get_display_name(gc))) {

        g_hash_table_remove(gaym->entry_order, args[0]);
        msg = g_strdup_printf(_("You have parted the channel"));

        purple_conv_chat_write(PURPLE_CONV_CHAT(convo), args[0], msg,
                             PURPLE_MESSAGE_SYSTEM, time(NULL));
        g_free(msg);
        serv_got_chat_left(gc,
                           purple_conv_chat_get_id(PURPLE_CONV_CHAT(convo)));
    } else {
        if (!purple_conv_chat_is_user_ignored(PURPLE_CONV_CHAT(convo), nick)
            && show_part) {
            purple_conv_chat_remove_user(PURPLE_CONV_CHAT(convo), nick, NULL);
        } else {
            PurpleConversationUiOps *ops =
                purple_conversation_get_ui_ops(convo);
            if (ops != NULL && ops->chat_remove_users != NULL) {
		GList* users = g_list_append(NULL, (char*)nick);
                ops->chat_remove_users(convo, users);
            }
            PurpleConvChatBuddy *cb =
                purple_conv_chat_cb_find(PURPLE_CONV_CHAT(convo), nick);
            if (cb) {
                purple_conv_chat_set_users(PURPLE_CONV_CHAT(convo),
                                         g_list_remove
                                         (purple_conv_chat_get_users
                                          (PURPLE_CONV_CHAT(convo)), cb));
                purple_conv_chat_cb_destroy(cb);
                if (!gaym_unreference_channel_member(gaym, nick))
                    purple_debug_error("gaym",
                                     "channel_members reference counting bug.\n");
            }
        }
    }

    g_free(nick);
}

void gaym_msg_ping(struct gaym_conn *gaym, const char *name,
                   const char *from, char **args)
{
    char *buf;
    if (!args || !args[0])
        return;

    buf = gaym_format(gaym, "v:", "PONG", args[0]);
    gaym_send(gaym, buf);
    g_free(buf);
}

void gaym_msg_pong(struct gaym_conn *gaym, const char *name,
                   const char *from, char **args)
{
    PurpleConversation *convo;
    PurpleConnection *gc;
    char **parts, *msg;
    time_t oldstamp;

    if (!args || !args[1])
        return;

    parts = g_strsplit(args[1], " ", 2);

    if (!parts[0] || !parts[1]) {
        g_strfreev(parts);
        return;
    }

    if (sscanf(parts[1], "%lu", &oldstamp) != 1) {
        msg = g_strdup(_("Error: invalid PONG from server"));
    } else {
        msg =
            g_strdup_printf(_("PING reply -- Lag: %lu seconds"),
                            time(NULL) - oldstamp);
    }

    convo =
        purple_find_conversation_with_account(PURPLE_CONV_TYPE_ANY, parts[0],
                                            gaym->account);
    g_strfreev(parts);
    if (convo) {
        if (purple_conversation_get_type(convo) == PURPLE_CONV_TYPE_CHAT)
            purple_conv_chat_write(PURPLE_CONV_CHAT(convo), "PONG", msg,
                                 PURPLE_MESSAGE_SYSTEM | PURPLE_MESSAGE_NO_LOG,
                                 time(NULL));
        else
            purple_conv_im_write(PURPLE_CONV_IM(convo), "PONG", msg,
                               PURPLE_MESSAGE_SYSTEM | PURPLE_MESSAGE_NO_LOG,
                               time(NULL));
    } else {
        gc = purple_account_get_connection(gaym->account);
        if (!gc) {
            g_free(msg);
            return;
        }
        purple_notify_info(gc, NULL, "PONG", msg);
    }
    g_free(msg);
}

void gaym_msg_privmsg(struct gaym_conn *gaym, const char *name,
                      const char *from, char **args)
{
    PurpleConversation *convo;
    char *tmp=0, *msg=0;
    int notice = 0;

    PurpleConnection *gc = purple_account_get_connection(gaym->account);
    char *nick = gaym_mask_nick(from);

    if (!args || !args[0] || !args[1] || !gc) {
        g_free(nick);
        return;
    }

    /**
     * Only nicks (sender/receiver) should use gcom_nick_to_gaym().
     *
     * Channels (which begin with either "#" or "&") should not be
     * converted.
     *
     * Messages should also not be converted.
     *
     * CHAT ROOM:
     * nick = the sender
     * args[0] = the receiving channel
     * args[1] = the message
     *
     * INSTANT MESSAGE:
     * nick = the sender
     * args[0] = the receiver (me)
     * args[1] = the message
     *
     * NOTICE:
     * nick = the sender
     * args[0] = " notice "
     * args[1] = the message
     */
    gcom_nick_to_gaym(nick);
    if (args[0][0] != '#' && args[0][0] != '&') {
        gcom_nick_to_gaym(args[0]);
    }

    convo =
        purple_find_conversation_with_account(PURPLE_CONV_TYPE_ANY, args[0],
                                            gaym->account);

    notice = !strcmp(args[0], " notice ");
    tmp = gaym_parse_ctcp(gaym, nick, args[0], args[1], notice);

    if (!tmp) {
        g_free(nick);
        return;
    }

    if (!gaym_privacy_check(gc, nick)) {
	g_free(tmp);
        g_free(nick);
        return;
    }

    //msg = g_markup_escape_text(tmp, -1);
    //g_free(tmp);
    msg=tmp;

    if (notice) {
        tmp = g_strdup_printf("(notice) %s", msg);
        g_free(msg);
        msg = tmp;
    }

    if (!purple_utf8_strcasecmp
        (args[0], purple_connection_get_display_name(gc))) {
        serv_got_im(gc, nick, msg, 0, time(NULL));
    } else if (notice) {
        serv_got_im(gc, nick, msg, 0, time(NULL));
    } else if (convo) {

        serv_got_chat_in(gc, purple_conv_chat_get_id(PURPLE_CONV_CHAT(convo)),
                         nick, 0, msg, time(NULL));
    } else {
        purple_debug(PURPLE_DEBUG_ERROR, "gaym",
                   "Got a PRIVMSG on %s, which does not exist\n", args[0]);
    }

    g_free(msg);
    g_free(nick);
}

void gaym_msg_regonly(struct gaym_conn *gaym, const char *name,
                      const char *from, char **args)
{
    PurpleConnection *gc = purple_account_get_connection(gaym->account);
    char *msg;

    if (!args || !args[1] || !args[2] || !gc)
        return;

    msg = g_strdup_printf(_("Cannot join %s:"), args[1]);
    purple_notify_error(gc, _("Cannot join channel"), msg, args[2]);
    g_free(msg);
}

/* I don't think gay.com ever sends this message */
void gaym_msg_quit(struct gaym_conn *gaym, const char *name,
                   const char *from, char **args)
{
    PurpleConnection *gc = purple_account_get_connection(gaym->account);
    char *data[2];

    if (!args || !args[0] || !gc)
        return;

    data[0] = gaym_mask_nick(from);
    data[1] = args[0];
    /* XXX this should have an API, I shouldn't grab this directly */
    g_slist_foreach(gc->buddy_chats, (GFunc) gaym_chat_remove_buddy, data);

    gaym_buddy_status(gaym, data[0], FALSE, NULL, FALSE);

    g_free(data[0]);

    return;
}

void gaym_msg_who(struct gaym_conn *gaym, const char *name,
                  const char *from, char **args)
{
    char *pos;
    GaymNamelist *nameslist;

    if (!strncmp(name, "315", 3)) {

        nameslist = g_queue_peek_head(gaym->namelists);
        if (!nameslist)
            return;
        nameslist->members = g_slist_reverse(nameslist->members);
        nameslist->current = nameslist->members;

        // If we are doing an "umbrella room" then we send out this names
        // thing.
        // Because the names parsing section terminates on a "names" from 
        // The exact channel name match.
        if (g_str_has_suffix(args[1], "=*")) {
            purple_debug_misc("who",
                            "Has a =* suffix, sending out one more namescmd \n");
            const char *cmdargs[1] = { args[1] };
            gaym_cmd_names(gaym, NULL, NULL, cmdargs);
        }
        return;
    }

    if (args[2]) {

        nameslist = g_queue_peek_tail(gaym->namelists);
        if (!nameslist)
            return;
        GaymBuddy *member = g_new0(GaymBuddy, 1);
        gchar **parts = g_strsplit(args[2], " ", 7);

        gchar *equals;
        if ((equals = strchr(args[1], '=')))
            member->room = g_strdup(equals + 1);
        if (parts[6]) {
            member->bio = gaym_bio_strdup(parts[6]);
            member->thumbnail = gaym_thumbnail_strdup(parts[6]);
            char *prefix_start = NULL;
            if (g_ascii_isdigit(parts[3][0])
                && (prefix_start = strchr(parts[3], '|')))
                member->prefix = g_strdup(prefix_start + 1);
            else
                member->prefix = g_strdup(parts[3]);


            gchar *stats = gaym_stats_strdup(parts[6]);
            if (stats) {
                gchar **stat_parts = g_strsplit(stats, "|", 3);
                member->sex = stat_parts[0];
                member->age = stat_parts[1];
                member->location = stat_parts[2];
                g_free(stats);
            }

            nameslist->members =
                g_slist_prepend(nameslist->members, member);
        }
        g_strfreev(parts);

        pos = strrchr(args[1], '=');
        int val = 0;
        if (!pos)
            return;
        val = g_ascii_digit_value(*(++pos));
        if (val != nameslist->num_rooms) {
            purple_debug_misc("msgs", "*******NEXT ROOM******\n");
            const char *cmdargs[1] = { args[1] };
            gaym_cmd_names(gaym, NULL, NULL, cmdargs);
            nameslist->num_rooms = val;
        }
    }
    // Use the who msgs cross-referenced with the NAMES list to figure out 
    // 
    // 
    // who is who. Resolve conflicts.

}

void hammer_stop_cb(gpointer data)
{
    struct hammer_cb_data *hdata = (struct hammer_cb_data *) data;

    purple_debug_misc("gaym", "hammer stopped, dialog is %x\n",
                    hdata->cancel_dialog);
    // This destroys the hammer data!
    purple_debug_misc("gaym", "Cancelling hammer: %s\n", hdata->room);
    // I'm not sure if the dialog data is freed. 
    // For now, I assume not. 
    // hdata->cancel_dialog=0;
    // The old key gets freed, so strdup it again
    g_hash_table_replace(hdata->gaym->hammers, g_strdup(hdata->room),
                         NULL);
}

void hammer_cb_data_destroy(struct hammer_cb_data *hdata)
{
    if (!hdata)
        return;
    if (hdata->cancel_dialog)
        purple_request_close(PURPLE_REQUEST_ACTION, hdata->cancel_dialog);
    if (hdata->room)
        g_free(hdata->room);
    g_free(hdata);
}

void hammer_cb_no(gpointer data)
{
    hammer_cb_data_destroy(data);
}

void hammer_cb_yes(gpointer data)
{
    struct hammer_cb_data *hdata = (struct hammer_cb_data *) data;
    char *room = g_strdup(hdata->room);
    const char *args[1] = { room };

    char *msg;
    msg = g_strdup_printf("Hammering into room %s", hdata->room);
    hdata->cancel_dialog =
        purple_request_action(hdata->gaym->account->gc, _("Hammering..."),
                            msg, NULL, 0, NULL, hdata->room, NULL, hdata, 1, ("Cancel"),
                            hammer_stop_cb);
    g_hash_table_insert(hdata->gaym->hammers, g_strdup(hdata->room),
                        hdata);
    gaym_cmd_join(hdata->gaym, NULL, NULL, args);
    if (msg)
        g_free(msg);
    if (room)
        g_free(room);
}
void gaym_msg_chanfull(struct gaym_conn *gaym, const char *name,
                       const char *from, char **args)
{
    PurpleConnection *gc = purple_account_get_connection(gaym->account);
    gchar *buf;
    const char *joinargs[1];

    if (!args || !args[1] || !gc)
        return;

    joinargs[0] = args[1];

    gpointer unused = NULL;
    gpointer data = NULL;
    gboolean hammering = g_hash_table_lookup_extended
        (gaym->hammers, args[1], &unused, &data);

    if (hammering && data) {
        // Add delay here?
        gaym_cmd_join(gaym, NULL, NULL, joinargs);
    } else if (hammering && !data) {    // hammer was cancelled.
        purple_debug_misc("gaym", "HAMMER CANCELLED ON FULL MESSAGE\n");
        g_hash_table_remove(gaym->hammers, args[1]);
    } else {
        buf = g_strdup_printf("%s is full. Do you want to keep trying?", args[1]);
        struct hammer_cb_data *hdata = g_new0(struct hammer_cb_data, 1);
        hdata->gaym = gaym;
        hdata->room = g_strdup(args[1]);
        hdata->cancel_dialog = NULL;
        purple_request_yes_no(gc, _("Room Full"), _("Room Full"), buf, 0,
                            NULL,NULL,NULL,hdata, hammer_cb_yes, hammer_cb_no);

        g_free(buf);
    }

}

void gaym_msg_create_pay_only(struct gaym_conn *gaym, const char *name,
                              const char *from, char **args)
{
    PurpleConnection *gc = purple_account_get_connection(gaym->account);
    char *buf;
    if (!args || !args[1] || !gc) {
        return;
    }
    buf = g_strdup_printf(_("%s"), args[2]);
    purple_notify_error(gc, _("Pay Only"), _("Pay Only"), buf);
    /**
     * FIXME
     * by now the chatroom is already in the buddy list...need
     * to remove it or something
     */
    g_free(buf);
}

void gaym_msg_pay_channel(struct gaym_conn *gaym, const char *name,
                          const char *from, char **args)
{
    PurpleConnection *gc = purple_account_get_connection(gaym->account);
    char *buf;

    if (!args || !args[1] || !gc)
        return;

    buf =
        g_strdup_printf(_("The channel %s is for paying members only."),
                        args[1]);
    purple_notify_error(gc, _("Pay Only"), _("Pay Only"), buf);
    g_free(buf);
}

void gaym_msg_toomany_channels(struct gaym_conn *gaym, const char *name,
                               const char *from, char **args)
{
    PurpleConnection *gc = purple_account_get_connection(gaym->account);
    char *buf;

    if (!args || !args[1] || !gc)
        return;

    buf =
        g_strdup_printf(_
                        ("You have joined too many channels the maximum is (2). You cannot join channel %s. Part another channel first ."),
                        args[1]);
    purple_notify_error(gc, _("Maximum ChannelsReached"),
                      _("Maximum ChannelsReached"), buf);
    g_free(buf);
}

void gaym_msg_list_busy(struct gaym_conn *gaym, const char *name,
                        const char *from, char **args)
{
    PurpleConnection *gc = purple_account_get_connection(gaym->account);
    char *buf;
    if (!args || !args[1] || !gc) {
        return;
    }
    buf = g_strdup_printf(_("%s"), args[1]);
    purple_notify_error(gc, _("Server Busy"), _("Server Busy"), buf);
    // if (gaym->roomlist) {
    // purple_roomlist_cancel_get_list(gaym->roomlist);
    // }
    g_free(buf);
    /**
     * Can't get member created rooms right now.
     * This is our trigger to add the static rooms
     */
    build_roomlist_from_config(gaym->roomlist, gaym->confighash,
                               gaym->roomlist_filter);
    if (gaym->roomlist_filter) {
        g_free(gaym->roomlist_filter);
        gaym->roomlist_filter = NULL;
    }
    return;

}

void gaym_msg_richnames_list(struct gaym_conn *gaym, const char *name,
                             const char *from, char **args)
{
    PurpleConnection *gc = purple_account_get_connection(gaym->account);
    PurpleConversation *convo;
    PurpleConvChatBuddyFlags flags = PURPLE_CBFLAGS_NONE;
    char *channel = args[1];
    char *nick = args[2];
    char *extra = args[4];

    if (!gc) {
        return;
    }

    gcom_nick_to_gaym(nick);
    purple_debug(PURPLE_DEBUG_INFO, "gaym",
               "gaym_msg_richnames_list() Channel: %s Nick: %s Extra: %s\n",
               channel, nick, extra);

    convo =
        purple_find_conversation_with_account(PURPLE_CONV_TYPE_ANY, channel,
                                            gaym->account);

    char *bio = gaym_bio_strdup(extra);
    g_free(bio);

    gaym_buddy_status(gaym, nick, TRUE, extra, FALSE);

    if (convo == NULL) {
        purple_debug(PURPLE_DEBUG_ERROR, "gaym", "690 for %s failed\n",
                   args[1]);
        return;
    }

    gint *entry = g_hash_table_lookup(gaym->entry_order, channel);
    g_return_if_fail(entry != NULL);

    flags = chat_pecking_order(extra);
    flags = include_chat_entry_order(flags, (*entry)--);

    purple_conv_chat_add_user(PURPLE_CONV_CHAT(convo), nick, NULL, flags,
                            FALSE);

    /**
     * Make the ignore.png icon appear next to the nick.
     */
    PurpleConversationUiOps *ops = purple_conversation_get_ui_ops(convo);
    if (gaym_privacy_check(gc, nick)) {
        purple_conv_chat_unignore(PURPLE_CONV_CHAT(convo), nick);
    } else {
        purple_conv_chat_ignore(PURPLE_CONV_CHAT(convo), nick);
    }
    ops->chat_update_user((convo), nick);
    gaym_update_channel_member(gaym, nick, extra);
}

/**
 * vim:tabstop=4:shiftwidth=4:expandtab:
 */
