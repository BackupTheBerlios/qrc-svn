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

#include "internal.h"
#include "conversation.h"
#include "blist.h"
#include "notify.h"
#include "util.h"
#include "debug.h"
#include "imgstore.h"
#include "request.h"
#include "privacy.h"
#include "prefs.h"

#include "botfilter.h"
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

static void gaym_chat_remove_buddy(GaimConversation * convo, char *data[2])
{
    /**
     * FIXME: is *message ever used ???
     */
    char *message = g_strdup_printf("quit: %s", data[1]);

    if (gaim_conv_chat_find_user(GAIM_CONV_CHAT(convo), data[0]))
        gaim_conv_chat_remove_user(GAIM_CONV_CHAT(convo), data[0], NULL);

    g_free(message);
}

void gaym_msg_default(struct gaym_conn *gaym, const char *name,
                      const char *from, char **args)
{
    gaim_debug(GAIM_DEBUG_INFO, "gaym", "Unrecognized message: %s\n",
               args[0]);
}

void gaym_msg_away(struct gaym_conn *gaym, const char *name,
                   const char *from, char **args)
{
    GaimConnection *gc = gaim_account_get_connection(gaym->account);

    if (!args || !args[1] || !gc) {
        return;
    }

    gcom_nick_to_gaym(args[1]);
    serv_got_im(gc, args[1], args[2], GAIM_CONV_IM_AUTO_RESP, time(NULL));
}

void gaym_fetch_thumbnail_cb(void *user_data, const char *pic_data,
                             size_t len)
{
    if (!user_data || !pic_data) {
        return;
    }

    struct gaym_fetch_thumbnail_data *d = user_data;

    if (GAIM_CONNECTION_IS_VALID(d->gc) && len) {
        gaim_buddy_icons_set_for_user(gaim_connection_get_account(d->gc),
                                      d->who, (void *) pic_data, len);
    } else {
        gaim_debug_error("gaym", "Fetching buddy icon failed.\n");
    }

    g_free(d->who);
    g_free(d);
}

static void gaym_fetch_photo_cb(void *user_data, const char *info_data,
                                size_t len)
{
    if (!info_data || !user_data) {
        return;
    }

    struct gaym_fetch_thumbnail_data *d = user_data;

    char *info, *t;

    struct gaym_conn *gaym = d->gc->proto_data;

    char *hashurl =
        g_hash_table_lookup(gaym->confighash, "view-profile-url");
    g_return_if_fail(hashurl != NULL);

    int id = gaim_imgstore_add(info_data, len, NULL);
    if (d->stats && d->bio)
        info =
            g_strdup_printf
            ("<b>Stats:</b> %s<br><b>Bio:</b> %s<br><img id=%d><br><a href='%s%s'>Full Profile</a>",
             d->stats, d->bio, id, hashurl, d->who);
    else if (d->stats)
        info =
            g_strdup_printf
            ("<b>Stats:</b> %s<br><img id=%d><br><a href='%s%s'>Full Profile</a>",
             d->stats, id, hashurl, d->who);
    else if (d->bio)
        info =
            g_strdup_printf
            ("<b>Bio:</b> %s<br><img id=%d><br><a href='%s%s'>Full Profile</a>",
             d->bio, id, hashurl, d->who);
    else
        info =
            g_strdup_printf
            ("No Info Found<br><img id=%d><br><a href='%s%s'>Full Profile</a>",
             id, hashurl, d->who);

    gaim_notify_userinfo(d->gc, d->who,
                         t = g_strdup_printf("Gay.com - %s", d->who),
                         d->who, NULL, info, NULL, NULL);
    g_free(t);

    if (d) {
        if (d->who)
            g_free(d->who);
        if (d->bio)
            g_free(d->bio);
        if (d->stats)
            g_free(d->stats);
        g_free(d);
    }
    gaim_imgstore_unref(id);
}

static void gaym_fetch_info_cb(void *user_data, const char *info_data,
                               size_t len)
{
    struct gaym_fetch_thumbnail_data *d = user_data;
    char *picpath;
    char *picurl;
    char *info, *t;
    char *match = "pictures.0.url=";

    struct gaym_conn *gaym = d->gc->proto_data;

    char *hashurl =
        g_hash_table_lookup(gaym->confighash, "view-profile-url");
    g_return_if_fail(hashurl != NULL);

    if (d->stats && d->bio)
        info =
            g_strdup_printf
            ("<b>Stats:</b> %s<br><b>Bio:</b> %s<br><a href='%s%s'>Full Profile</a>",
             d->stats, d->bio, hashurl, d->who);
    else if (d->stats)
        info =
            g_strdup_printf
            ("<b>Stats:</b> %s<br><a href='%s%s'>Full Profile</a>",
             d->stats, hashurl, d->who);
    else if (d->bio)
        info =
            g_strdup_printf
            ("<b>Bio:</b> %s<br><a href='%s%s'>Full Profile</a>",
             d->bio, hashurl, d->who);
    else
        info =
            g_strdup_printf
            ("No Info Found<br><a href='%s%s'>Full Profile</a>",
             hashurl, d->who);

    picpath = return_string_between(match, "\n", info_data);
    if (!picpath || strlen(picpath) == 0) {
        gaim_notify_userinfo(d->gc, d->who,
                             t = g_strdup_printf("Gay.com - %s", d->who),
                             d->who, NULL, info, NULL, NULL);
        g_free(t);
        return;
    }

    picurl = g_strdup_printf("http://www.gay.com%s", picpath);
    if (picurl) {
        gaim_url_fetch(picurl, FALSE, "Mozilla/4.0 (compatible; MSIE 5.0)",
                       FALSE, gaym_fetch_photo_cb, user_data);
        return;
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

    gaym_buddy_status(gaym, args[1], FALSE, NULL);

    char *normalized = g_strdup(gaim_normalize(gaym->account, args[1]));

    if (g_hash_table_lookup(gaym->info_window_needed, normalized)) {
        g_hash_table_remove(gaym->info_window_needed, normalized);

        char *hashurl =
            g_hash_table_lookup(gaym->confighash, "view-profile-url");
        g_return_if_fail(hashurl != NULL);

        char *buf;
        buf =
            g_strdup_printf
            ("That user is not logged on. Check <a href='%s%s'>here</a> to see if that user has a profile.",
             hashurl, args[1]);
        gaim_notify_userinfo(gaim_account_get_connection(gaym->account),
                             NULL, NULL, "No such user", NULL, buf, NULL,
                             NULL);
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

    gcom_nick_to_gaym(args[1]);

    gaym_buddy_status(gaym, args[1], TRUE, args[5]);

    char *normalized = g_strdup(gaim_normalize(gaym->account, args[1]));

    struct gaym_fetch_thumbnail_data *data;

    if (g_hash_table_lookup(gaym->info_window_needed, normalized)) {
        g_hash_table_remove(gaym->info_window_needed, normalized);
        char *hashurl = g_hash_table_lookup(gaym->confighash,
                                            "ohm.profile-url");
        g_return_if_fail(hashurl != NULL);
        data = g_new0(struct gaym_fetch_thumbnail_data, 1);
        data->gc = gaim_account_get_connection(gaym->account);
        data->who = g_strdup(args[1]);
        data->bio = gaym_bio_strdup(args[5]);
        data->stats = gaym_stats_strdup(args[5]);

        char *infourl = g_strdup_printf("%s?pw=%s&name=%s", hashurl,
                                        gaym->hash_pw, args[1]);
        if (infourl) {
            gaim_url_fetch(infourl, FALSE,
                           "Mozilla/4.0 (compatible; MSIE 5.0)", FALSE,
                           gaym_fetch_info_cb, data);
            g_free(infourl);
        }
    }
    g_free(normalized);
}

void gaym_msg_list(struct gaym_conn *gaym, const char *name,
                   const char *from, char **args)
{
    /**
     * If you free anything here related to the roomlist
     * be sure you test what happens when the roomlist reference
     * count goes to zero! Because it may crash gaim.
     */
    if (!gaym->roomlist) {
        return;
    }
    /**
     * Begin result of member created room list
     */
    if (!strcmp(name, "321")) {
        GaimRoomlistRoom *room;
        room = gaim_roomlist_room_new(GAIM_ROOMLIST_ROOMTYPE_CATEGORY,
                                      _("Member Created"), NULL);
        gaim_roomlist_room_add(gaym->roomlist, room);
        gaim_roomlist_set_in_progress(gaym->roomlist, TRUE);
        return;
    }

    /**
     * The list of member created rooms
     */
    if (!strcmp(name, "322")) {
        GaimRoomlistRoom *room;
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
            gaim_debug_error("gaym",
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

        room = gaim_roomlist_room_new(GAIM_ROOMLIST_ROOMTYPE_ROOM,
                                      field_name,
                                      g_list_nth_data(gaym->roomlist->
                                                      rooms, 0));
        gaim_roomlist_room_add_field(gaym->roomlist, room, field_name);
        gaim_roomlist_room_add_field(gaym->roomlist, room, args[1]);
        gaim_roomlist_room_add(gaym->roomlist, room);
        g_free(field_name);
    }

    /**
     * End result of member created room list
     * This is our trigger to add the static rooms
     */
    if (!strcmp(name, "323")) {
        build_roomlist_from_config(gaym->roomlist, gaym->confighash);
        return;
    }
}

void gaym_msg_unknown(struct gaym_conn *gaym, const char *name,
                      const char *from, char **args)
{
    GaimConnection *gc = gaim_account_get_connection(gaym->account);
    char *buf;

    if (!args || !args[1] || !gc)
        return;

    buf = g_strdup_printf(_("Unknown message '%s'"), args[1]);
    gaim_notify_error(gc, _("Unknown message"), buf,
                      _
                      ("Gaim has sent a message the IRC server did not understand."));
    g_free(buf);
}

void gaym_msg_names(struct gaym_conn *gaym, const char *name,
                    const char *from, char **args)
{
    char *names, *cur, *end, *tmp, *msg;
    GaimConversation *convo;

    if (!strcmp(name, "366")) {
        convo =
            gaim_find_conversation_with_account(gaym->nameconv ? gaym->
                                                nameconv : args[1],
                                                gaym->account);
        if (!convo) {
            gaim_debug(GAIM_DEBUG_ERROR, "gaym",
                       "Got a NAMES list for %s, which doesn't exist\n",
                       args[2]);
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
            if (gaim_conversation_get_type(convo) == GAIM_CONV_CHAT)
                gaim_conv_chat_write(GAIM_CONV_CHAT(convo), "", msg,
                                     GAIM_MESSAGE_SYSTEM |
                                     GAIM_MESSAGE_NO_LOG, time(NULL));
            else
                gaim_conv_im_write(GAIM_CONV_IM(convo), "", msg,
                                   GAIM_MESSAGE_SYSTEM |
                                   GAIM_MESSAGE_NO_LOG, time(NULL));
            g_free(msg);
            g_free(gaym->nameconv);
            gaym->nameconv = NULL;
        } else {
            GList *users = NULL;
            GList *flags = NULL;

            while (*cur) {
                GaimConvChatBuddyFlags f = GAIM_CBFLAGS_NONE;
                end = strchr(cur, ' ');
                if (!end)
                    end = cur + strlen(cur);
                if (*cur == '@') {
                    f = GAIM_CBFLAGS_OP;
                    cur++;
                } else if (*cur == '%') {
                    f = GAIM_CBFLAGS_HALFOP;
                    cur++;
                } else if (*cur == '+') {
                    f = GAIM_CBFLAGS_VOICE;
                    cur++;
                }
                tmp = g_strndup(cur, end - cur);
                users = g_list_prepend(users, tmp);
                flags = g_list_prepend(flags, GINT_TO_POINTER(f));
                cur = end;
                if (*cur)
                    cur++;
            }
            users = g_list_reverse(users);
            flags = g_list_reverse(flags);

            if (users != NULL) {
                GList *l;

                gaim_conv_chat_add_users(GAIM_CONV_CHAT(convo), users,
                                         flags);

                for (l = users; l != NULL; l = l->next)
                    g_free(l->data);

                g_list_free(users);
                g_list_free(flags);
            }
        }
        g_free(names);
    } else {
        if (!gaym->names)
            gaym->names = g_string_new("");

        gaym->names = g_string_append(gaym->names, args[3]);
    }
}

/**
 * Change this to WELCOME
 */
void gaym_msg_endmotd(struct gaym_conn *gaym, const char *name,
                      const char *from, char **args)
{
    GaimConnection *gc;

    gc = gaim_account_get_connection(gaym->account);
    if (!gc)
        return;

    gaim_connection_set_state(gc, GAIM_CONNECTED);
    serv_finish_login(gc);

    gaym_blist_timeout(gaym);
    if (!gaym->timer)
        gaym->timer =
            gaim_timeout_add(BLIST_UPDATE_PERIOD,
                             (GSourceFunc) gaym_blist_timeout,
                             (gpointer) gaym);
}

void gaym_msg_nochan(struct gaym_conn *gaym, const char *name,
                     const char *from, char **args)
{
    GaimConnection *gc = gaim_account_get_connection(gaym->account);

    if (gc == NULL || args == NULL || args[1] == NULL)
        return;

    gaim_notify_error(gc, NULL, _("No such channel"), args[1]);
}

void gaym_msg_nonick_chan(struct gaym_conn *gaym, const char *name,
                          const char *from, char **args)
{
    GaimConnection *gc = gaim_account_get_connection(gaym->account);

    if (gc == NULL || args == NULL || args[1] == NULL)
        return;

    gaim_notify_error(gc, NULL, _("Not logged in:"), args[1]);
}

void gaym_msg_nonick(struct gaym_conn *gaym, const char *name,
                     const char *from, char **args)
{
    GaimConnection *gc;
    GaimConversation *convo;

    convo = gaim_find_conversation_with_account(args[1], gaym->account);
    if (convo) {
        if (gaim_conversation_get_type(convo) == GAIM_CONV_CHAT) {
            /* does this happen? */
            gaim_conv_chat_write(GAIM_CONV_CHAT(convo), args[1],
                                 _("no such channel"),
                                 GAIM_MESSAGE_SYSTEM | GAIM_MESSAGE_NO_LOG,
                                 time(NULL));
        } else {
            gaim_conv_im_write(GAIM_CONV_IM(convo), args[1],
                               _("User is not logged in"),
                               GAIM_MESSAGE_SYSTEM | GAIM_MESSAGE_NO_LOG,
                               time(NULL));
        }
    } else {
        if ((gc = gaim_account_get_connection(gaym->account)) == NULL)
            return;
        gaim_notify_error(gc, NULL, _("No such nick or channel"), args[1]);
    }
}

void gaym_msg_nosend(struct gaym_conn *gaym, const char *name,
                     const char *from, char **args)
{
    GaimConnection *gc;
    GaimConversation *convo;

    convo = gaim_find_conversation_with_account(args[1], gaym->account);
    if (convo) {
        gaim_conv_chat_write(GAIM_CONV_CHAT(convo), args[1], args[2],
                             GAIM_MESSAGE_SYSTEM | GAIM_MESSAGE_NO_LOG,
                             time(NULL));
    } else {
        if ((gc = gaim_account_get_connection(gaym->account)) == NULL)
            return;
        gaim_notify_error(gc, NULL, _("Could not send"), args[2]);
    }
}

/**
 * Is this used?
 */
void gaym_msg_notinchan(struct gaym_conn *gaym, const char *name,
                        const char *from, char **args)
{
    GaimConversation *convo =
        gaim_find_conversation_with_account(args[1], gaym->account);

    gaim_debug(GAIM_DEBUG_INFO, "gaym",
               "We're apparently not in %s, but tried to use it\n",
               args[1]);
    if (convo) {
        /* g_slist_remove(gaym->gc->buddy_chats, convo);
           gaim_conversation_set_account(convo, NULL); */
        gaim_conv_chat_write(GAIM_CONV_CHAT(convo), args[1], args[2],
                             GAIM_MESSAGE_SYSTEM | GAIM_MESSAGE_NO_LOG,
                             time(NULL));
    }
}

/**
 * Invite WORKS in gay.com!
 */
void gaym_msg_invite(struct gaym_conn *gaym, const char *name,
                     const char *from, char **args)
{
    GaimConnection *gc = gaim_account_get_connection(gaym->account);
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
    GaimConnection *gc = gaim_account_get_connection(gaym->account);
    char *buf;

    if (!args || !args[1] || !gc)
        return;

    buf =
        g_strdup_printf(_("Joining %s requires an invitation."), args[1]);
    gaim_notify_error(gc, _("Invitation only"), _("Invitation only"), buf);
    g_free(buf);
}

void gaym_msg_trace(struct gaym_conn *gaym, const char *name,
                    const char *from, char **args)
{
    GaimConversation *conv =
        gaim_find_conversation_with_account(gaym->traceconv ? gaym->
                                            traceconv : args[1],
                                            gaym->account);
    gaim_conversation_write(conv, "TRACE", args[3],
                            GAIM_MESSAGE_SYSTEM | GAIM_MESSAGE_NO_LOG,
                            time(NULL));

}

void gaym_msg_join(struct gaym_conn *gaym, const char *name,
                   const char *from, char **args)
{
    GaimConnection *gc = gaim_account_get_connection(gaym->account);
    char *nick = gaym_mask_nick(from);

    GaimConversation *convo;
    GaimConvChatBuddyFlags flags = GAIM_CBFLAGS_NONE;
    char *bio = NULL;
    char *bio_markedup = NULL;
    static int id = 1;

    if (!gc) {
        g_free(nick);
        return;
    }

    gcom_nick_to_gaym(nick);
    if (!gaim_utf8_strcasecmp(nick, gaim_connection_get_display_name(gc))) {
        /* We are joining a channel for the first time */
        if (gaym->persist_room && !strcmp(gaym->persist_room, args[0])) {
            g_free(gaym->persist_room);
            gaym->persist_room = NULL;
            gaim_request_close(GAIM_REQUEST_ACTION,
                               gaym->hammer_cancel_dialog);

        }

        serv_got_joined_chat(gc, id++, args[0]);
        g_free(nick);
        return;
    }

    convo = gaim_find_conversation_with_account(args[0], gaym->account);
    if (convo == NULL) {
        gaim_debug(GAIM_DEBUG_ERROR, "gaym", "JOIN for %s failed\n",
                   args[0]);
        g_free(nick);
        return;
    }

    gaym_buddy_status(gaym, nick, TRUE, args[1]);

    gboolean gaym_botfilter_permit =
        gaym_botfilter_check(gc, nick, bio, FALSE);

    bio = gaym_bio_strdup(args[1]);
    if (bio) {
        bio_markedup = gaim_markup_linkify(bio);
        g_free(bio);
    }

    flags = chat_pecking_order(args[1]);

    gboolean gaym_privacy_permit = gaym_privacy_check(gc, nick);
    gboolean show_join =
        gaim_prefs_get_bool("/plugins/prpl/gaym/show_join");

    if (gaim_prefs_get_bool("/plugins/prpl/gaym/show_bio_with_join")) {
        gaim_conv_chat_add_user(GAIM_CONV_CHAT(convo), nick, bio_markedup,
                                flags, (gaym_privacy_permit
                                        && gaym_botfilter_permit
                                        && show_join));
    } else {
        gaim_conv_chat_add_user(GAIM_CONV_CHAT(convo), nick, NULL,
                                flags, (gaym_privacy_permit
                                        && gaym_botfilter_permit
                                        && show_join));
    }

    /**
     * Make the ignore.png icon appear next to the nick.
     */
    GaimConversationUiOps *ops = gaim_conversation_get_ui_ops(convo);
    if (gaym_privacy_permit && gaym_botfilter_permit) {
        gaim_conv_chat_unignore(GAIM_CONV_CHAT(convo), nick);
    } else {
        gaim_conv_chat_ignore(GAIM_CONV_CHAT(convo), nick);
    }
    ops->chat_update_user((convo), nick);

    g_free(bio_markedup);
    g_free(nick);
}

void gaym_msg_mode(struct gaym_conn *gaym, const char *name,
                   const char *from, char **args)
{
    GaimConversation *convo;
    char *nick = gaym_mask_nick(from), *buf;

    if (*args[0] == '#' || *args[0] == '&') {   /* Channel */
        convo =
            gaim_find_conversation_with_account(args[0], gaym->account);
        if (!convo) {
            gaim_debug(GAIM_DEBUG_ERROR, "gaym",
                       "MODE received for %s, which we are not in\n",
                       args[0]);
            g_free(nick);
            return;
        }
        buf =
            g_strdup_printf(_("mode (%s %s) by %s"), args[1],
                            args[2] ? args[2] : "", nick);
        gaim_conv_chat_write(GAIM_CONV_CHAT(convo), args[0], buf,
                             GAIM_MESSAGE_SYSTEM | GAIM_MESSAGE_NO_LOG,
                             time(NULL));
        g_free(buf);
        if (args[2]) {
            GaimConvChatBuddyFlags newflag, flags;
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
                    gaim_conv_chat_user_get_flags(GAIM_CONV_CHAT(convo),
                                                  user);
                newflag = GAIM_CBFLAGS_NONE;
                if (*mcur == 'o')
                    newflag = GAIM_CBFLAGS_OP;
                else if (*mcur == 'h')
                    newflag = GAIM_CBFLAGS_HALFOP;
                else if (*mcur == 'v')
                    newflag = GAIM_CBFLAGS_VOICE;
                if (newflag) {
                    if (add)
                        flags |= newflag;
                    else
                        flags &= ~newflag;
                    gaim_conv_chat_user_set_flags(GAIM_CONV_CHAT(convo),
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

    GaimConnection *gc = gaim_account_get_connection(gaym->account);
    char *nick = gaym_mask_nick(from);

    if (!gc) {
        g_free(nick);
        return;
    }

    chats = gc->buddy_chats;

    if (!gaim_utf8_strcasecmp(nick, gaim_connection_get_display_name(gc))) {
        gaim_connection_set_display_name(gc, args[0]);
    }

    while (chats) {
        GaimConvChat *chat = GAIM_CONV_CHAT(chats->data);
        /* This is ugly ... */
        if (gaim_conv_chat_find_user(chat, nick))
            gaim_conv_chat_rename_user(chat, nick, args[0]);
        chats = chats->next;
    }
    g_free(nick);
}

void gaym_msg_notice(struct gaym_conn *gaym, const char *name,
                     const char *from, char **args)
{
    GaimConnection *gc = gaim_account_get_connection(gaym->account);

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
    GaimConversation *convo;
    char *msg;

    GaimConnection *gc = gaim_account_get_connection(gaym->account);
    char *nick = gaym_mask_nick(from);

    if (!args || !args[0] || !gc || !nick) {
        g_free(nick);
        return;
    }

    convo = gaim_find_conversation_with_account(args[0], gaym->account);
    gboolean show_part =
        gaim_prefs_get_bool("/plugins/prpl/gaym/show_part");

    gcom_nick_to_gaym(nick);
    if (!gaim_utf8_strcasecmp(nick, gaim_connection_get_display_name(gc))) {
        msg = g_strdup_printf(_("You have parted the channel"));

        gaim_conv_chat_write(GAIM_CONV_CHAT(convo), args[0], msg,
                             GAIM_MESSAGE_SYSTEM, time(NULL));
        g_free(msg);
        serv_got_chat_left(gc,
                           gaim_conv_chat_get_id(GAIM_CONV_CHAT(convo)));
    } else {
        if (!gaim_conv_chat_is_user_ignored(GAIM_CONV_CHAT(convo), nick)
            && show_part) {
            gaim_conv_chat_remove_user(GAIM_CONV_CHAT(convo), nick, NULL);
        } else {
            GaimConversationUiOps *ops =
                gaim_conversation_get_ui_ops(convo);
            if (ops != NULL && ops->chat_remove_user != NULL) {
                ops->chat_remove_user(convo, nick);
            }
            GaimConvChatBuddy *cb =
                gaim_conv_chat_cb_find(GAIM_CONV_CHAT(convo), nick);
            if (cb) {
                gaim_conv_chat_set_users(GAIM_CONV_CHAT(convo),
                                         g_list_remove
                                         (gaim_conv_chat_get_users
                                          (GAIM_CONV_CHAT(convo)), cb));
                gaim_conv_chat_cb_destroy(cb);
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
    GaimConversation *convo;
    GaimConnection *gc;
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

    convo = gaim_find_conversation_with_account(parts[0], gaym->account);
    g_strfreev(parts);
    if (convo) {
        if (gaim_conversation_get_type(convo) == GAIM_CONV_CHAT)
            gaim_conv_chat_write(GAIM_CONV_CHAT(convo), "PONG", msg,
                                 GAIM_MESSAGE_SYSTEM | GAIM_MESSAGE_NO_LOG,
                                 time(NULL));
        else
            gaim_conv_im_write(GAIM_CONV_IM(convo), "PONG", msg,
                               GAIM_MESSAGE_SYSTEM | GAIM_MESSAGE_NO_LOG,
                               time(NULL));
    } else {
        gc = gaim_account_get_connection(gaym->account);
        if (!gc) {
            g_free(msg);
            return;
        }
        gaim_notify_info(gc, NULL, "PONG", msg);
    }
    g_free(msg);
}

void gaym_msg_privmsg(struct gaym_conn *gaym, const char *name,
                      const char *from, char **args)
{
    GaimConversation *convo;
    char *tmp, *msg;
    int notice = 0;

    GaimConnection *gc = gaim_account_get_connection(gaym->account);
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

    convo = gaim_find_conversation_with_account(args[0], gaym->account);

    notice = !strcmp(args[0], " notice ");
    tmp = gaym_parse_ctcp(gaym, nick, args[0], args[1], notice);

    if (!tmp) {
        g_free(nick);
        return;
    }

    if (!gaym_privacy_check(gc, nick)) {
        g_free(nick);
        return;
    }

    msg = gaim_escape_html(tmp);

    g_free(tmp);

    if (notice) {
        tmp = g_strdup_printf("(notice) %s", msg);
        g_free(msg);
        msg = tmp;
    }

    if (!gaim_utf8_strcasecmp
        (args[0], gaim_connection_get_display_name(gc))) {
        serv_got_im(gc, nick, msg, 0, time(NULL));
    } else if (notice) {
        serv_got_im(gc, nick, msg, 0, time(NULL));
    } else if (convo) {

        serv_got_chat_in(gc, gaim_conv_chat_get_id(GAIM_CONV_CHAT(convo)),
                         nick, 0, msg, time(NULL));
    } else {
        gaim_debug(GAIM_DEBUG_ERROR, "gaym",
                   "Got a PRIVMSG on %s, which does not exist\n", args[0]);
    }

    g_free(msg);
    g_free(nick);
}

void gaym_msg_regonly(struct gaym_conn *gaym, const char *name,
                      const char *from, char **args)
{
    GaimConnection *gc = gaim_account_get_connection(gaym->account);
    char *msg;

    if (!args || !args[1] || !args[2] || !gc)
        return;

    msg = g_strdup_printf(_("Cannot join %s:"), args[1]);
    gaim_notify_error(gc, _("Cannot join channel"), msg, args[2]);
    g_free(msg);
}

/* I don't think gay.com ever sends this message */
void gaym_msg_quit(struct gaym_conn *gaym, const char *name,
                   const char *from, char **args)
{
    GaimConnection *gc = gaim_account_get_connection(gaym->account);
    char *data[2];

    if (!args || !args[0] || !gc)
        return;

    data[0] = gaym_mask_nick(from);
    data[1] = args[0];
    /* XXX this should have an API, I shouldn't grab this directly */
    g_slist_foreach(gc->buddy_chats, (GFunc) gaym_chat_remove_buddy, data);

    gaym_buddy_status(gaym, data[0], FALSE, NULL);

    g_free(data[0]);

    return;
}

void gaym_msg_who(struct gaym_conn *gaym, const char *name,
                  const char *from, char **args)
{
}

void hammer_stop_cb(gpointer data)
{

    struct gaym_conn *gaym = (struct gaym_conn *) data;

    gaym->cancelling_persist = TRUE;
    gaim_debug_misc("gaym", "Cancelling persist: %s\n",
                    gaym->persist_room);
}

void hammer_cb(gpointer data)
{

    struct gaym_conn *gaym = (struct gaym_conn *) data;
    const char *args[1];
    char *msg;
    gaim_debug_misc("gaym", "Persisting room %s\n", gaym->persist_room);
    args[0] = gaym->persist_room;
    gaym->cancelling_persist = FALSE;
    msg = g_strdup_printf("Hammering into room %s", gaym->persist_room);
    gaym->hammer_cancel_dialog =
        gaim_request_action(gaym->account->gc, _("Cancel Hammer"), msg,
                            NULL, 0, gaym, 1, ("Cancel"), hammer_stop_cb);

    gaym_cmd_join(gaym, NULL, NULL, args);
    if (msg)
        g_free(msg);
}

void gaym_msg_chanfull(struct gaym_conn *gaym, const char *name,
                       const char *from, char **args)
{
    GaimConnection *gc = gaim_account_get_connection(gaym->account);
    char *buf;
    const char *joinargs[1];

    if (!args || !args[1] || !gc)
        return;

    joinargs[0] = args[1];

    if (gaym->persist_room && !strcmp(gaym->persist_room, args[1]))
        if (gaym->cancelling_persist) {
            if (gaym->persist_room) {
                g_free(gaym->persist_room);
                gaym->persist_room = NULL;
            }
            gaym->cancelling_persist = FALSE;
        } else {
            gaim_debug_misc("gaym", "trying again\n");
            gaym_cmd_join(gaym, NULL, NULL, joinargs);
    } else {

        gaym->persist_room = g_strdup(args[1]);
        buf =
            g_strdup_printf("%s is full. Do you want to keep trying?",
                            args[1]);
        gaim_request_yes_no(gc, _("Room Full"), _("Room Full"), buf, 0,
                            gaym, hammer_cb, NULL);

        g_free(buf);
    }
}

void gaym_msg_create_pay_only(struct gaym_conn *gaym, const char *name,
                              const char *from, char **args)
{
    GaimConnection *gc = gaim_account_get_connection(gaym->account);
    char *buf;
    if (!args || !args[1] || !gc) {
        return;
    }
    buf = g_strdup_printf(_("%s"), args[2]);
    gaim_notify_error(gc, _("Pay Only"), _("Pay Only"), buf);
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
    GaimConnection *gc = gaim_account_get_connection(gaym->account);
    char *buf;

    if (!args || !args[1] || !gc)
        return;

    buf =
        g_strdup_printf(_("The channel %s is for paying members only."),
                        args[1]);
    gaim_notify_error(gc, _("Pay Only"), _("Pay Only"), buf);
    g_free(buf);
}

void gaym_msg_toomany_channels(struct gaym_conn *gaym, const char *name,
                               const char *from, char **args)
{
    GaimConnection *gc = gaim_account_get_connection(gaym->account);
    char *buf;

    if (!args || !args[1] || !gc)
        return;

    buf =
        g_strdup_printf(_
                        ("You have joined too many channels the maximum is (2). You cannot join channel %s. Part another channel first ."),
                        args[1]);
    gaim_notify_error(gc, _("Maximum ChannelsReached"),
                      _("Maximum ChannelsReached"), buf);
    g_free(buf);
}

void gaym_msg_list_busy(struct gaym_conn *gaym, const char *name,
                        const char *from, char **args)
{
    GaimConnection *gc = gaim_account_get_connection(gaym->account);
    char *buf;
    if (!args || !args[1] || !gc) {
        return;
    }
    buf = g_strdup_printf(_("%s"), args[1]);
    gaim_notify_error(gc, _("Server Busy"), _("Server Busy"), buf);
    if (gaym->roomlist) {
        gaim_roomlist_cancel_get_list(gaym->roomlist);
    }
    g_free(buf);
}

void gaym_msg_richnames_list(struct gaym_conn *gaym, const char *name,
                             const char *from, char **args)
{
    GaimConnection *gc = gaim_account_get_connection(gaym->account);
    GaimConversation *convo;
    GaimConvChatBuddyFlags flags = GAIM_CBFLAGS_NONE;
    char *channel = args[1];
    char *nick = args[2];
    char *extra = args[4];

    if (!gc) {
        return;
    }

    gcom_nick_to_gaym(nick);
    gaim_debug(GAIM_DEBUG_INFO, "gaym",
               "gaym_msg_richnames_list() Channel: %s Nick: %s Extra: %s\n",
               channel, nick, extra);

    convo = gaim_find_conversation_with_account(channel, gaym->account);

    char *bio = gaym_bio_strdup(extra);
    gboolean gaym_botfilter_permit =
        gaym_botfilter_check(gc, nick, bio, FALSE);
    g_free(bio);

    gaym_buddy_status(gaym, nick, TRUE, extra);

    if (convo == NULL) {
        gaim_debug(GAIM_DEBUG_ERROR, "gaym", "690 for %s failed\n",
                   args[1]);
        return;
    }

    flags = chat_pecking_order(extra);

    gaim_conv_chat_add_user(GAIM_CONV_CHAT(convo), nick, NULL, flags,
                            FALSE);

    /**
     * Make the ignore.png icon appear next to the nick.
     */
    GaimConversationUiOps *ops = gaim_conversation_get_ui_ops(convo);
    if (gaym_privacy_check(gc, nick) && gaym_botfilter_permit) {
        gaim_conv_chat_unignore(GAIM_CONV_CHAT(convo), nick);
    } else {
        gaim_conv_chat_ignore(GAIM_CONV_CHAT(convo), nick);
    }
    ops->chat_update_user((convo), nick);
}

/**
 * vim:tabstop=4:shiftwidth=4:expandtab:
 */
