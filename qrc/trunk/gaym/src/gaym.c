/**
 * @file gaym.c
 *
 * purple
 *
 * Copyright (C) 2003, Robbert Haarman <purple@inglorion.net>
 * Copyright (C) 2003, Ethan Blanton <eblanton@cs.purdue.edu>
 * Copyright (C) 2000-2003, Rob Flynn <rob@tgflinux.com>
 * Copyright (C) 1998-1999, Mark Spencer <markster@marko.net>
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

#include "accountopt.h"
#include "blist.h"
#include "conversation.h"
#include "debug.h"
#include "notify.h"
#include "prpl.h"
#include "plugin.h"
#include "util.h"
#include "version.h"
#include "request.h"
#include "privacy.h"
#include "signals.h"

#include "helpers.h"
#include "gayminfo.h"
#include "gaympriv.h"
#include "botfilter.h"
#include "gaym.h"

static const char *gaym_blist_icon(PurpleAccount * a, PurpleBuddy * b);
static GList *gaym_status_types(PurpleAccount * account);
static GList *gaym_actions(PurplePlugin * plugin, gpointer context);
/* static GList *gaym_chat_info(PurpleConnection *gc); */
static void gaym_login(PurpleAccount * account);
static void gaym_login_cb(gpointer data, gint source, const gchar* error_message);
static void gaym_close(PurpleConnection * gc);
static int gaym_im_send(PurpleConnection * gc, const char *who,
                        const char *what, PurpleMessageFlags flags);
static int gaym_chat_send(PurpleConnection * gc, int id, const char *what,
                          PurpleMessageFlags flags);
static void gaym_chat_join(PurpleConnection * gc, GHashTable * data);
static void gaym_input_cb(gpointer data, gint source,
                          PurpleInputCondition cond);
static guint gaym_nick_hash(const char *nick);
static gboolean gaym_nick_equal(const char *nick1, const char *nick2);
static void gaym_buddy_free(struct gaym_buddy *ib);
static void gaym_channel_member_free(GaymBuddy * cm);

static void gaym_buddy_append(char *name, struct gaym_buddy *ib,
                              BListWhois * blist_whois);
static void gaym_buddy_clear_done(char *name, struct gaym_buddy *ib,
                                  gpointer nothing);

static void connect_signals(PurpleConnection * plugin);
static PurplePlugin *_gaym_plugin = NULL;

static const char *status_chars = "@+%&";

int gaym_send(struct gaym_conn *gaym, const char *buf)
{
    int ret;

    if (gaym->fd < 0)
        return -1;

    /* purple_debug(PURPLE_DEBUG_MISC, "gaym", "sent: %s", buf); */
    if ((ret = write(gaym->fd, buf, strlen(buf))) < 0)
        purple_connection_error(purple_account_get_connection(gaym->account),
                              _("Server has disconnected"));

    return ret;
}

gboolean gaym_blist_timeout(struct gaym_conn * gaym)
{
    /**
     * There are 510 characters available for an IRC command (512 if
     * you count CR-LF).  "WHOIS " takes up 6 characters.  Assuming
     * you need allow an extra character for the NULL when using
     * g_string_sized_new(), we need to allocate (510-6)+1=505 here.
     */
    BListWhois *blist_whois = g_new0(BListWhois, 1);
    blist_whois->count = 0;
    blist_whois->string = g_string_sized_new(505);

    char *list, *buf;

    g_hash_table_foreach(gaym->buddies, (GHFunc) gaym_buddy_append,
                         (gpointer) blist_whois);

    list = g_string_free(blist_whois->string, FALSE);
    if (!list || !strlen(list)) {
        g_hash_table_foreach(gaym->buddies, (GHFunc) gaym_buddy_clear_done,
                             NULL);
        purple_timeout_remove(gaym->timer);
        gaym->timer =
            purple_timeout_add(BLIST_UPDATE_PERIOD,
                             (GSourceFunc) gaym_blist_timeout,
                             (gpointer) gaym);
        g_free(list);
        g_free(blist_whois);

        return TRUE;
    }
    gaym->blist_updating = TRUE;
    buf = gaym_format(gaym, "vn", "WHOIS", list);
    gaym_send(gaym, buf);
    purple_timeout_remove(gaym->timer);
    gaym->timer =
        purple_timeout_add(BLIST_CHUNK_INTERVAL,
                         (GSourceFunc) gaym_blist_timeout,
                         (gpointer) gaym);

    g_free(buf);
    g_free(list);
    g_free(blist_whois);

    return TRUE;
}

static void gaym_buddy_clear_done(char *name, struct gaym_buddy *ib,
                                  gpointer nothing)
{
    ib->done = FALSE;
}

static void gaym_buddy_append(char *name, struct gaym_buddy *ib,
                              BListWhois * blist_whois)
{
    char *converted_name = NULL;
    converted_name = gaym_nick_to_gcom_strdup(name);
    /**
     * 
     * There are 510 characters available for an IRC command (512 if
     * you count CR-LF).  "WHOIS " takes up 6 characters.  This means
     * we have up to 504 characters available for comma separated
     * converted_names
     */
    if (ib->done == FALSE && blist_whois->count < 10
        && (strlen(converted_name) + blist_whois->string->len + 1) <=
        504) {
        blist_whois->count++;
        ib->done = TRUE;
        if (blist_whois->string->len == 0) {
            g_string_append_printf(blist_whois->string, "%s",
                                   converted_name);
        } else {
            g_string_append_printf(blist_whois->string, ",%s",
                                   converted_name);
        }
    }

    g_free(converted_name);
    return;
}

static void gaym_whois_one(struct gaym_conn *gaym, struct gaym_buddy *ib)
{
    char *buf;
    char *nick;
    nick = gaym_nick_to_gcom_strdup(ib->name);
    buf = gaym_format(gaym, "vn", "WHOIS", nick);
    gaym_send(gaym, buf);
    g_free(nick);
    g_free(buf);
}

static const char *gaym_blist_icon(PurpleAccount * a, PurpleBuddy * b)
{
    return "gaym";
}

static char *gaym_status_text(PurpleBuddy * buddy)
{
    char *status;

    struct gaym_conn *gaym =
        (struct gaym_conn *) buddy->account->gc->proto_data;

    if (!gaym) {
        return g_strdup(_("Offline"));
    }

    struct gaym_buddy *ib =
        g_hash_table_lookup(gaym->buddies, buddy->name);

    if (!ib) {
        return g_strdup(_("Offline"));
    }

    if (!ib->online) {
        return g_strdup(_("Offline"));
    }

    if (!ib->bio) {
        return NULL;
    }

    status = g_markup_escape_text(ib->bio, strlen(ib->bio));

    return status;
}

static void gaym_tooltip_text(PurpleBuddy * buddy, PurpleNotifyUserInfo *user_info, gboolean full)
{
    if (!buddy || !buddy->account || !buddy->account->gc)
        return;

    
    struct gaym_conn *gaym =
        (struct gaym_conn *) buddy->account->gc->proto_data;

    if (!gaym) {
        return;
    }

    struct gaym_buddy *ib = g_hash_table_lookup(gaym->channel_members,
                                                purple_normalize(gaym->
                                                               account,
                                                               buddy->
                                                               name));
    if (!ib)
        ib = g_hash_table_lookup(gaym->buddies,
                                 purple_normalize(gaym->account,
                                                buddy->name));

    build_tooltip_text(ib, user_info);
    
}

static GList *gaym_status_types(PurpleAccount * account)
{
    PurpleStatusType *type;
    GList *types = NULL;

    type = purple_status_type_new(PURPLE_STATUS_OFFLINE, "offline",
                                _("Offline"), FALSE);
    types = g_list_append(types, type);

    type = purple_status_type_new(PURPLE_STATUS_AVAILABLE, "available",
                                _("Available"), TRUE);
    types = g_list_append(types, type);

    type =
        purple_status_type_new_with_attrs(PURPLE_STATUS_AWAY, "away",
                                        _("Away"), TRUE, TRUE, FALSE,
                                        "message", _("Message"),
                                        purple_value_new(PURPLE_TYPE_STRING),
                                        NULL);
    types = g_list_append(types, type);

    return types;

}

static void gaym_set_info(PurpleConnection * gc, const char *info)
{
    struct gaym_conn *gaym = gc->proto_data;
    PurpleAccount *account = purple_connection_get_account(gc);
    char *hostname = "none";
    char *buf, *bioline;
    int i = 0;
    char *tmpinfo = NULL;
    if (info) {
        tmpinfo = g_strdup(info);
        for (i = 0; i < strlen(tmpinfo); i++) {
            if (tmpinfo[i] == '\n') {
                tmpinfo[i] = ' ';
            }
        }
        tmpinfo = g_strstrip(tmpinfo);
    }

    if (!tmpinfo) {
        /**
         * don't change any bio settings, since this is just
         * setting an away message
         */
    } else {
        if (gaym->bio) {
            g_free(gaym->bio);
        }
        if (tmpinfo && strlen(tmpinfo) > 0) {
            purple_debug_misc("gaym", "option1, info=%x\n", tmpinfo);
            /* java client allows MAX_BIO_LEN characters */
            gaym->bio = g_strndup(tmpinfo, MAX_BIO_LEN);
        } else if (gaym->server_bioline
                   && strlen(gaym->server_bioline) > 0) {
            purple_debug_misc("gaym", "option2\n");
            gaym->bio = gaym_bio_strdup(gaym->server_bioline);
        } else {
            purple_debug_misc("gaym", "option3\n");
            gaym->bio = g_strdup("Purple User");
        }
        purple_account_set_user_info(account, gaym->bio);
        purple_account_set_string(account, "bioline", gaym->bio);
        purple_debug_info("gaym", "INFO=%x BIO=%x\n", tmpinfo, gaym->bio);
        purple_debug_misc("gaym", "In login_cb, gc->account=%x\n",
                        gc->account);
    }

    bioline =
        g_strdup_printf("%s#%s\001%s",
                        gaym->thumbnail ? gaym->thumbnail : "",
                        // gc->away ? gc->away : (gaym->bio ? gaym->bio :
                        // ""),
                        (gaym->bio ? gaym->bio : ""),
                        gaym->server_stats ? gaym->server_stats : "");

    buf = gaym_format(gaym, "vvvv:", "USER",
                      purple_account_get_username(account),
                      hostname, gaym->server, bioline);

    purple_debug_misc("gaym", "BIO=%x\n", bioline);

    if (gaym_send(gaym, buf) < 0) {
        purple_connection_error(gc, "Error registering with server");
    }

    if (tmpinfo) {
        g_free(tmpinfo);
    }
    g_free(bioline);
    g_free(buf);

    return;
}

static void gaym_show_set_info(PurplePluginAction * action)
{
    PurpleConnection *gc = (PurpleConnection *) action->context;
    purple_account_request_change_user_info(purple_connection_get_account(gc));
}

static GList *gaym_actions(PurplePlugin * plugin, gpointer context)
{
    GList *list = NULL;
    PurplePluginAction *act = NULL;

    act = purple_plugin_action_new(_("Change Bio"), gaym_show_set_info);
    list = g_list_prepend(list, act);

    return list;
}

static void gaym_blist_join_chat_cb(PurpleBlistNode * node, gpointer data)
{
    const char *args[1];

    PurpleChat *chat = (PurpleChat *) node;
    struct gaym_conn *gaym = chat->account->gc->proto_data;
    args[0] = data;

    g_return_if_fail(args[0] != NULL);
    g_return_if_fail(gaym != NULL);

    gaym_cmd_join(gaym, "join", NULL, args);
}

static GList *gaym_blist_node_menu(PurpleBlistNode * node)
{
    GList *m = NULL;
    PurpleMenuAction *act = NULL;
    int i = 0;

    if (node->type == PURPLE_BLIST_CHAT_NODE) {

        PurpleChat *chat = (PurpleChat *) node;
        char *channel = g_hash_table_lookup(chat->components, "channel");

        if (!channel) {
            return m;
        }

        if (!g_str_has_suffix(channel, "=*")) {
            return m;
        }

        char *label = NULL;
        char *instance = NULL;

        int max = purple_prefs_get_int("/plugins/prpl/gaym/chat_room_instances");

        for (i = max; i > 0; i--) {
            label = g_strdup_printf(_("Join Room %d"), i);
            instance =
                g_strdup_printf("%.*s%d", strlen(channel) - 1, channel, i);
            act =
                purple_menu_action_new(label, PURPLE_CALLBACK(gaym_blist_join_chat_cb), instance, NULL);
            m = g_list_prepend(m, act);
        }
    }
    if (PURPLE_BLIST_NODE_IS_BUDDY(node)) {
        
    }
    return m;
}

static GList *gaym_chat_join_info(PurpleConnection * gc)
{
    GList *m = NULL;
    struct proto_chat_entry *pce;

    pce = g_new0(struct proto_chat_entry, 1);
    pce->label = _("_Room:");
    pce->identifier = "channel";
    m = g_list_prepend(m, pce);

    return m;
}

GHashTable *gaym_chat_info_defaults(PurpleConnection * gc,
                                    const char *chat_name)
{
    GHashTable *defaults;

    defaults =
        g_hash_table_new_full(g_str_hash, g_str_equal, NULL, g_free);

    if (chat_name != NULL)
        g_hash_table_insert(defaults, "channel", g_strdup(chat_name));

    return defaults;
}

static void gaym_login_with_chat_key(PurpleAccount * account)
{
    PurpleConnection *gc;
    struct gaym_conn *gaym;
    char *buf;
    const char *username = purple_account_get_username(account);

    gc = purple_account_get_connection(account);
    gaym = gc->proto_data;

    buf = g_strdup_printf(_("Signon: %s"), username);
    purple_connection_update_progress(gc, buf, 5, 6);
    g_free(buf);
    purple_debug_misc("gaym", "Trying login to %s\n", gaym->server);
    PurpleProxyConnectData* pdata = purple_proxy_connect(NULL, account, 
			     gaym->server,
                             purple_account_get_int(account, "port", IRC_DEFAULT_PORT),
                             *gaym_login_cb, 
			     gc);
    if (!pdata || !account->gc) {
        purple_connection_error(gc, _("Couldn't create socket"));
        purple_debug_misc("gaym", "account->gc: %x\n", account->gc);
        return;
    }

}

guint gaym_room_hash(gconstpointer key)
{

    if (*((char *) key) == 0)
        return 0;

    return atoi((char *) (key + 1));


}

static void gaym_login(PurpleAccount * account)
{
    PurpleConnection *gc;
    struct gaym_conn *gaym;
    char *buf;
    const char *username = purple_account_get_username(account);

    gc = purple_account_get_connection(account);
    gc->flags |= PURPLE_CONNECTION_NO_NEWLINES | PURPLE_CONNECTION_AUTO_RESP;

    if (strpbrk(username, " \t\v\r\n") != NULL) {
        purple_connection_error(gc,
                              _("IRC nicks may not contain whitespace"));
        return;
    }

    gc->proto_data = gaym = g_new0(struct gaym_conn, 1);
    gaym->account = account;


    /**
     * purple_connection_set_display_name(gc, userparts[0]);
     */
    purple_connection_set_display_name(gc, username);
    gaym->server =
        g_strdup(purple_account_get_string
                 (account, "server", "www.gay.com"));
    /**
     * gaym->server = "www.gay.com";
     */


    gaym->namelists = g_queue_new();

    gaym->buddies =
        g_hash_table_new_full((GHashFunc) gaym_nick_hash,
                              (GEqualFunc) gaym_nick_equal, NULL,
                              (GDestroyNotify) gaym_buddy_free);

    gaym->channel_members =
        g_hash_table_new_full((GHashFunc) gaym_nick_hash,
                              (GEqualFunc) gaym_nick_equal, NULL,
                              (GDestroyNotify) gaym_channel_member_free);

    gaym->cmds = g_hash_table_new(g_str_hash, g_str_equal);
    gaym_cmd_table_build(gaym);
    gaym->msgs = g_hash_table_new(g_str_hash, g_str_equal);
    gaym_msg_table_build(gaym);
    gaym->roomlist_filter = NULL;

    gaym->hammers =
        g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
                              (GDestroyNotify) hammer_cb_data_destroy);
    /**
     * The last parameter needs to be NULL here, since the same
     * field is added for both the key and the value (and if we
     * free it twice, thats bad and causes crashing!).
     */

    gaym->nameconv = NULL;

    gaym->info_window_needed =
        g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);

    gaym->entry_order =
        g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

    /**
     * This is similar to gaym->info_window_needed, except this is
     * for thumbails inside the IM conversation window if the
     * person is not already on the buddy list
     */

    buf = g_strdup_printf(_("Signon: %s"), username);
    purple_connection_update_progress(gc, buf, 1, 6);
    g_free(buf);


    /**
     * Making a change to try cached password first.
     * gaym_try_cached_password(account, gaym_login_with_chat_key);
     */
    gaym_get_chat_key_from_weblogin(account, gaym_login_with_chat_key);
}


static void gaym_get_configtxt_cb(PurpleUtilFetchUrlData* data, gpointer proto_data,
                                  const gchar * config_text, size_t len, const gchar* error_message)
{
    struct gaym_conn *gaym = (struct gaym_conn*)proto_data;
    // PurpleConnection *gc = purple_account_get_connection(gaym->account);

    g_return_if_fail(config_text != NULL);

    gaym->confighash = gaym_properties_new(config_text);
    g_return_if_fail(gaym->confighash != NULL);

    // if(roomlist=g_hash_table_lookup(gaym->confighash, "roomlist"))
    // gaym->roomlist = gaym_parse_roomlist();
    // synchronize_deny_list(gc, gaym->confighash);

    return;
}
static void gaym_login_cb(gpointer data, gint source, const gchar *error_message)
{
    PurpleConnection *gc = data;
    struct gaym_conn *gaym = gc->proto_data;
    char hostname[256]="Peacock";
    char *buf;
    const char *username;
    const char *user_bioline = NULL;
    char *bioline;
    char *login_name;

    if (PURPLE_CONNECTION_IS_VALID(gc)) {


        GList *connections = purple_connections_get_all();

        if (source < 0) {
            purple_connection_error(gc, _("Couldn't connect to host"));
            return;
        }

        if (!g_list_find(connections, gc)) {
            close(source);
            return;
        }

        gaym->fd = source;
        purple_debug_misc("gaym", "In login_cb with chat_key=%s\n",
                        gaym->chat_key);
        if (gaym->chat_key) {

            buf = gaym_format(gaym, "vv", "PASS", gaym->chat_key);
            if (gaym_send(gaym, buf) < 0) {
                purple_connection_error(gc, "Error sending password");
                return;
            }
            g_free(buf);
        } else {
            purple_connection_error(gc,
                                  _
                                  ("Password wasn't recorded. Report bug."));
            return;
        }
        username = purple_account_get_string(gaym->account, "username", "");
        user_bioline =
            g_strdup(purple_account_get_string
                     (gaym->account, "bioline", ""));
        purple_debug_info("gaym", "USER BIOLINE=%x\n", user_bioline);
        purple_account_set_user_info(gc->account, user_bioline);
        purple_debug_misc("gaym",
                        "In login_cb, user_bioline: %x, gc->account=%x\n",
                        user_bioline, gc->account);

        login_name =
            gaym_nick_to_gcom_strdup(purple_connection_get_display_name(gc));
        bioline = g_strdup_printf("%s#%s\001%s",
                                  gaym->thumbnail,
                                  user_bioline ? user_bioline : "",
                                  gaym->server_stats ? gaym->
                                  server_stats : "");

        buf = gaym_format(gaym, "vn", "NICK", login_name);
        purple_debug_misc("gaym", "Command: %s\n", buf);

        if (gaym_send(gaym, buf) < 0) {
            purple_connection_error(gc, "Error sending nickname");
            return;
        }
        g_free(buf);
        buf =
            gaym_format(gaym, "vvvv:", "USER", login_name, hostname,
                        gaym->server, bioline);

        purple_debug_misc("gaym", "Command: %s\n", buf);
        if (gaym_send(gaym, buf) < 0) {
            purple_connection_error(gc, "Error registering with server");
            return;
        }
        g_free(login_name);
        g_free(buf);

        const char *server = purple_account_get_string(gc->account, "server",
                                                     IRC_DEFAULT_SERVER);
        char *url =
            g_strdup_printf
            ("http://%s/messenger/config.txt?%s", server, gaym->chat_key);

        char *user_agent = "Mozilla/4.0";

        get_spamlist_from_web();
        purple_util_fetch_url(url, FALSE, user_agent, FALSE,
                       gaym_get_configtxt_cb, gaym);

        g_free(url);
        gc->inpa =
            purple_input_add(gaym->fd, PURPLE_INPUT_READ, gaym_input_cb, gc);

        connect_signals(gc);

    }
}

void kill_hammer(gpointer * room, struct hammer_cb_data *data,
                 gpointer * null)
{
    hammer_cb_data_destroy(data);
}

static void gaym_close(PurpleConnection * gc)
{
    struct gaym_conn *gaym = gc->proto_data;

    purple_debug_misc("gaym", "gaym close function has been called\n");
    if (gaym == NULL)
        return;

    gaym_cmd_quit(gaym, "quit", NULL, NULL);

    if (gc->inpa)
        purple_input_remove(gc->inpa);

    g_free(gaym->inbuf);
    purple_debug_misc("gaym", "closing fd %i\n", gaym->fd);
    close(gaym->fd);

    if (gaym->timer)
        purple_timeout_remove(gaym->timer);

    if (gaym->thumbnail)
        g_free(gaym->thumbnail);

    if (gaym->chat_key)
        g_free(gaym->chat_key);

    if (gaym->server_bioline)
        g_free(gaym->server_bioline);

    if (gaym->server_stats)
        g_free(gaym->server_stats);

    if (gaym->roomlist_filter)
        g_free(gaym->roomlist_filter);

    if (gaym->bio)
        g_free(gaym->bio);


    // Would we need to free each element, too?
    g_queue_free(gaym->namelists);

    g_hash_table_destroy(gaym->cmds);
    g_hash_table_destroy(gaym->msgs);
    g_hash_table_destroy(gaym->info_window_needed);
    g_hash_table_destroy(gaym->entry_order);
    if (gaym->motd)
        g_string_free(gaym->motd, TRUE);

    if (gaym->names)
        g_string_free(gaym->names, TRUE);

    if (gaym->nameconv)
        g_free(gaym->nameconv);
    if (gaym->subroom)
        g_free(gaym->subroom);

    g_hash_table_destroy(gaym->confighash);

    g_hash_table_foreach(gaym->hammers, (GHFunc) kill_hammer, NULL);

    g_free(gaym->server);
    g_free(gaym);
}

static int gaym_im_send(PurpleConnection * gc, const char *who,
                        const char *what, PurpleMessageFlags flags)
{
    struct gaym_conn *gaym = gc->proto_data;
    const char *args[2];
    char *automsg = NULL;
    char *stripped_msg = NULL;
    if (strchr(status_chars, *who) != NULL) {
        args[0] = who + 1;
    } else {
        args[0] = who;
    }
    stripped_msg = purple_markup_strip_html(what);
    if (flags & PURPLE_MESSAGE_AUTO_RESP) {
        automsg = g_strdup_printf("<AUTO-REPLY> %s", stripped_msg);
        g_free(stripped_msg);
        args[1] = automsg;

    } else {
        args[1] = stripped_msg;
    }
    gaym_cmd_privmsg(gaym, "msg", NULL, args);
    if (automsg) {
        g_free(automsg);
    }
    if (stripped_msg)
	g_free(stripped_msg);

    return 1;
}

static void gaym_get_info_quietly(PurpleConnection * gc, const char *who)
{
    purple_debug_misc("gaym", "request info quietly\n");
    struct gaym_conn *gaym = gc->proto_data;
    const char *args[1];
    args[0] = who;

    gaym_cmd_whois(gaym, "whois", NULL, args);
}


static void gaym_get_info(PurpleConnection * gc, const char *who)
{
    struct gaym_conn *gaym = gc->proto_data;

    const char *args[1];
    args[0] = who;

    char *normalized = g_strdup(purple_normalize(gc->account, who));
    purple_debug_misc("get_info","who: %s; normalized: %s\n",who, normalized);
    struct get_info_data *info_data = g_new0(struct get_info_data, 1);
    info_data->gaym=gaym;
    info_data->who=normalized;
    info_data->gc=gc;
    info_data->info = NULL;
    PurpleNotifyUserInfo *info = purple_notify_user_info_new();

    purple_notify_user_info_add_pair(info, "Fetching info for",who);
    purple_notify_userinfo(gc, who, info, NULL, NULL);
    purple_debug_misc("get_info","who: %s; normalized: %s\n",who, normalized);
    g_hash_table_insert(gaym->info_window_needed, normalized, info_data);
    purple_debug_misc("get_info","who: %s; normalized: %s\n",who, normalized);
    void *needed = g_hash_table_lookup(gaym->info_window_needed, normalized);
    purple_debug_misc("gaym", "get info: Needed for %s? %x\n", normalized, needed);

    purple_notify_user_info_destroy(info);
    gaym_cmd_whois(gaym, "whois", NULL, args);
}

static void gaym_set_status(PurpleAccount * account, PurpleStatus * status)
{
    // char *bio = NULL;
    // char *tmpmsg = NULL;
    // int i = 0;
    // struct gaym_conn *gaym = account->gc->proto_data;

    /* Set the away message */

    /**
     * In addition to setting the away message, set the Bio to the
     * away message; if the away message is NULL, then set the Bio
     * to the original bio.
     */

    /**
     *  The following would be great, and gay.com's server supports
     *  it, but gay.com's clients don't see the result.  So even though
     *  we can see the result, we won't bother.
     *
     * args[0] = msg;
     * gaym_cmd_away(gaym, "away", NULL, args);
     */
}

GaymBuddy *gaym_get_channel_member_reference(struct gaym_conn
                                             *gaym, const gchar * name)
{

    GaymBuddy *channel_member =
        (GaymBuddy *) g_hash_table_lookup(gaym->channel_members,
                                          name);

    if (!channel_member) {
        GaymBuddy *channel_member = g_new0(GaymBuddy, 1);
        channel_member->ref_count = 1;
        g_hash_table_insert(gaym->channel_members,
                            g_strdup(purple_normalize(gaym->account, name)),
                            channel_member);
        purple_debug_misc("gaym", "Creating channel_members entry for %s\n",
                        name);
        return g_hash_table_lookup(gaym->channel_members,
                                   purple_normalize(gaym->account, name));
    } else {
        purple_debug_misc("gaym",
                        "Adding reference to channel_members entry for %s\n",
                        name);
        (channel_member->ref_count)++;
        return channel_member;
    }

}

gboolean gaym_unreference_channel_member(struct gaym_conn * gaym,
                                         gchar * name)
{

    GaymBuddy *channel_member;
    channel_member =
        (GaymBuddy *) g_hash_table_lookup(gaym->channel_members,
                                          purple_normalize(gaym->account,
                                                         name));
    if (!channel_member)
        return FALSE;
    else {

        if (channel_member->ref_count <= 0)
            purple_debug_error("gaym",
                             "****Reference counting error with channel members struct.\n");

        channel_member->ref_count--;

        if (channel_member->ref_count == 0) {
            purple_debug_misc("gaym", "Removing %s from channel_members\n",
                            name);
            return g_hash_table_remove(gaym->channel_members,
                                       purple_normalize(gaym->account,
                                                      name));
        }
        return FALSE;
    }
}

static void gaym_add_buddy(PurpleConnection * gc, PurpleBuddy * buddy,
                           PurpleGroup * group)
{
    if (buddy->name) {
        buddy->name = g_strstrip(buddy->name);
    }
    if (buddy->alias) {
        buddy->alias = g_strstrip(buddy->alias);
    }
    if (buddy->server_alias) {
        buddy->server_alias = g_strstrip(buddy->server_alias);
    }
    struct gaym_conn *gaym = (struct gaym_conn *) gc->proto_data;
    struct gaym_buddy *ib = g_new0(struct gaym_buddy, 1);
    ib->name = g_strdup(buddy->name);
    ib->done = FALSE;
    ib->online = FALSE;
    ib->bio = NULL;
    ib->thumbnail = NULL;
    ib->sex = NULL;
    ib->age = NULL;
    ib->location = NULL;
    g_hash_table_replace(gaym->buddies, ib->name, ib);
    purple_debug_misc("gaym", "Add buddy: %s\n", buddy->name);
    /**
     * if the timer isn't set, this is during signon, so we don't want to
     * flood ourself off with WHOIS's, so we don't, but after that we want
     * to know when someone's online asap
     */
    if (gaym->timer)
        gaym_whois_one(gaym, ib);
}

static void gaym_remove_buddy(PurpleConnection * gc, PurpleBuddy * buddy,
                              PurpleGroup * group)
{
    struct gaym_conn *gaym = (struct gaym_conn *) gc->proto_data;

    /**
     * Only remove buddy->name from gaym->buddies if it doesn't
     * exist in any other group on the buddy list.  This allows us
     * to manage the buddy once, even though it might exist in
     * several groups within the buddy list.
     *
     * To add to confusion, the buddy being deleted is not yet deleted,
     * so we look for less than two identical buddies, and if so, then
     * remove the buddy from gaym->buddies.
     */

    GSList *buddies = purple_find_buddies(gaym->account, buddy->name);
    guint length = g_slist_length(buddies);

    if (length < 2) {
        g_hash_table_remove(gaym->buddies, buddy->name);
    }

    g_slist_free(buddies);
}

static void gaym_input_cb(gpointer data, gint source,
                          PurpleInputCondition cond)
{
    PurpleConnection *gc = data;
    struct gaym_conn *gaym = gc->proto_data;
    char *cur, *end;
    int len;

    if (gaym->inbuflen < gaym->inbufused + IRC_INITIAL_BUFSIZE) {
        gaym->inbuflen += IRC_INITIAL_BUFSIZE;
        gaym->inbuf = g_realloc(gaym->inbuf, gaym->inbuflen);
    }

    if ((len =
         read(gaym->fd, gaym->inbuf + gaym->inbufused,
              IRC_INITIAL_BUFSIZE - 1)) < 0) {
        purple_connection_error(gc, _("Read error"));
        return;
    } else if (len == 0) {
        purple_connection_error(gc, _("Server has disconnected"));
        return;
    }

    gaym->inbufused += len;
    gaym->inbuf[gaym->inbufused] = '\0';

    cur = gaym->inbuf;
    while (cur < gaym->inbuf + gaym->inbufused &&
           ((end = strstr(cur, "\r\n")) || (end = strstr(cur, "\n")))) {
        int step = (*end == '\r' ? 2 : 1);
        *end = '\0';
        gaym_parse_msg(gaym, cur);
        cur = end + step;
    }
    if (cur != gaym->inbuf + gaym->inbufused) { /* leftover */
        gaym->inbufused -= (cur - gaym->inbuf);
        memmove(gaym->inbuf, cur, gaym->inbufused);
    } else {
        gaym->inbufused = 0;
    }
}

static void gaym_add_permit(PurpleConnection * gc, const char *name)
{
    if (!gaym_nick_check(name)) {
        purple_privacy_permit_remove(gc->account, name, TRUE);
        purple_notify_error(gc, _("Invalid User Name"), name,
                          _("Invalid user name not added."));
    } else {
        gaym_privacy_change(gc, name);
    }
}

static void gaym_add_deny(PurpleConnection * gc, const char *name)
{
    if (!gaym_nick_check(name)) {
        purple_privacy_deny_remove(gc->account, name, TRUE);
        purple_notify_error(gc, _("Invalid User Name"), name,
                          _("Invalid user name not added."));
        return;
    }
    gaym_server_store_deny(gc, name, TRUE);
    gaym_privacy_change(gc, name);
}

static void gaym_rem_permit(PurpleConnection * gc, const char *name)
{
    gaym_privacy_change(gc, name);
}

static void gaym_rem_deny(PurpleConnection * gc, const char *name)
{
    gaym_server_store_deny(gc, name, FALSE);
    gaym_privacy_change(gc, name);
}

static void gaym_set_permit_deny(PurpleConnection * gc)
{
    gaym_privacy_change(gc, NULL);
}

/* static void gaym_warn(PurpleConnection * gc, const char *who, gboolean
   anonymous) { void *handle = NULL; struct gaym_conn *gaym =
   gc->proto_data; char *buf = g_strdup_printf
   ("http://%s/members/report/form.html?area=chat&room=&report=%s",
   gaym->server, who); purple_notify_uri(handle, buf); g_free(buf); } */
static void gaym_chat_join(PurpleConnection * gc, GHashTable * data)
{
    struct gaym_conn *gaym = gc->proto_data;
    const char *args[1];
    char *alias = NULL;

    PurpleChat *c = NULL;

    /**
     * need a copy, because data gets
     * destroyed in roomlist.c
     */
    GHashTable *chatinfo = NULL;

    args[0] = g_hash_table_lookup(data, "channel");

    if (args[0]) {
        alias = g_hash_table_lookup(data, "description");
        c = purple_blist_find_chat(purple_connection_get_account(gc), args[0]);
        if (!c) {
            chatinfo = g_hash_table_new(g_str_hash, g_str_equal);

            g_hash_table_replace(chatinfo, "channel", g_strdup(args[0]));

            c = purple_chat_new(purple_connection_get_account(gc),
                              alias, chatinfo);

            purple_blist_add_chat(c, NULL, NULL);
        }
    }

    if (!args[0] || *args[0] != '#') {
        /**
         * Trigger a room search in config.txt....
         */
        return;
    }

    gaym_cmd_join(gaym, "join", NULL, args);
}

static char *gaym_get_chat_name(GHashTable * data)
{
    return g_strdup(g_hash_table_lookup(data, "channel"));
}

static void gaym_chat_invite(PurpleConnection * gc, int id,
                             const char *message, const char *name)
{
    struct gaym_conn *gaym = gc->proto_data;
    PurpleConversation *convo = purple_find_chat(gc, id);
    const char *args[2];

    if (!convo) {
        purple_debug(PURPLE_DEBUG_ERROR, "gaym",
                   "Got chat invite request for bogus chat\n");
        return;
    }
    args[0] = name;
    args[1] = purple_conversation_get_name(convo);
    gaym_cmd_invite(gaym, "invite", purple_conversation_get_name(convo),
                    args);
}

static void gaym_chat_leave(PurpleConnection * gc, int id)
{
    struct gaym_conn *gaym = gc->proto_data;
    PurpleConversation *convo = purple_find_chat(gc, id);
    const char *args[2];

    if (!convo)
        return;

    args[0] = purple_conversation_get_name(convo);
    args[1] = NULL;
    gaym_cmd_part(gaym, "part", purple_conversation_get_name(convo), args);
    serv_got_chat_left(gc, id);
}

static int gaym_chat_send(PurpleConnection * gc, int id, const char *what,
                          PurpleMessageFlags flags)
{
    struct gaym_conn *gaym = gc->proto_data;
    PurpleConversation *convo = purple_find_chat(gc, id);
    const char *args[2];
    char *tmp;

    if (!convo) {
        purple_debug(PURPLE_DEBUG_ERROR, "gaym",
                   "chat send on nonexistent chat\n");
        return -EINVAL;
    }
#if 0
    if (*what == '/') {
        return gaym_parse_cmd(gaym, convo->name, what + 1);
    }
#endif
    args[0] = convo->name;
    args[1] = what;

    gaym_cmd_privmsg(gaym, "msg", NULL, args);

    tmp = g_markup_escape_text(what, -1);
    serv_got_chat_in(gc, id, purple_connection_get_display_name(gc), 0, tmp,
                     time(NULL));
    g_free(tmp);
    return 0;
}

static guint gaym_nick_hash(const char *nick)
{
    char *lc = NULL;
    guint bucket;

    if (!nick)
        return 0;
    lc = g_utf8_strdown(nick, -1);
    bucket = g_str_hash(lc);
    g_free(lc);

    return bucket;
}

static gboolean gaym_nick_equal(const char *nick1, const char *nick2)
{
    return (purple_utf8_strcasecmp(nick1, nick2) == 0);
}

static void gaym_channel_member_free(GaymBuddy * cm)
{
    g_free(cm->name);
    g_free(cm->bio);
    g_free(cm->thumbnail);
    g_free(cm->sex);
    g_free(cm->age);
    g_free(cm->location);
    g_free(cm);
}

static void gaym_buddy_free(struct gaym_buddy *ib)
{
    g_free(ib->name);
    g_free(ib->bio);
    g_free(ib->thumbnail);
    g_free(ib->sex);
    g_free(ib->age);
    g_free(ib->location);
    g_free(ib);
}

static PurpleChat *gaym_find_blist_chat(PurpleAccount * account,
                                      const char *name)
{
    char *chat_name;
    PurpleChat *chat;
    PurplePlugin *prpl;
    PurplePluginProtocolInfo *prpl_info = NULL;
    struct proto_chat_entry *pce;
    PurpleBlistNode *node, *group;
    GList *parts;

    PurpleBuddyList *purplebuddylist = purple_get_blist();

    g_return_val_if_fail(purplebuddylist != NULL, NULL);
    g_return_val_if_fail((name != NULL) && (*name != '\0'), NULL);

    if (!purple_account_is_connected(account))
        return NULL;

    prpl = purple_find_prpl(purple_account_get_protocol_id(account));
    prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(prpl);

    for (group = purplebuddylist->root; group != NULL; group = group->next) {
        for (node = group->child; node != NULL; node = node->next) {
            if (PURPLE_BLIST_NODE_IS_CHAT(node)) {

                chat = (PurpleChat *) node;

                if (account != chat->account)
                    continue;

                parts =
                    prpl_info->
                    chat_info(purple_account_get_connection(chat->account));

                pce = parts->data;
                chat_name = g_hash_table_lookup(chat->components,
                                                pce->identifier);

                if (chat->account == account && chat_name != NULL &&
                    name != NULL
                    && g_pattern_match_simple(chat_name, name)) {
                    return chat;
                }
            }
        }
    }

    return NULL;
}

static PurpleRoomlist *gaym_roomlist_get_list(PurpleConnection * gc)
{
    struct gaym_conn *gaym;
    GList *fields = NULL;
    PurpleRoomlistField *f;
    char *buf;

    gaym = gc->proto_data;

    if (gaym->roomlist) {
        purple_roomlist_unref(gaym->roomlist);
    }

    gaym->roomlist = purple_roomlist_new(purple_connection_get_account(gc));

    f = purple_roomlist_field_new(PURPLE_ROOMLIST_FIELD_STRING, _("Channel"),
                                "channel", FALSE);
    fields = g_list_prepend(fields, f);

    f = purple_roomlist_field_new(PURPLE_ROOMLIST_FIELD_STRING, "",
                                "description", TRUE);
    fields = g_list_prepend(fields, f);

    purple_roomlist_set_fields(gaym->roomlist, fields);

    /**
     * Member created rooms are retrieved through the IRC protocol
     * and after the last response is recieved from that request
     * the static rooms are added
     */

    buf = gaym_format(gaym, "v", "LIST #_*");
    gaym_send(gaym, buf);
    g_free(buf);

    return gaym->roomlist;
}

static void gaym_roomlist_cancel(struct _PurpleRoomlist *list)
{
    PurpleConnection *gc = purple_account_get_connection(list->account);
    struct gaym_conn *gaym;

    if (gc == NULL)
        return;

    gaym = gc->proto_data;

    purple_roomlist_set_in_progress(list, FALSE);

    if (gaym->roomlist == list) {
        g_list_free(g_list_nth_data(list->rooms, 0));
        gaym->roomlist = NULL;
        purple_roomlist_unref(list);
    }

    if (gaym->roomlist_filter) {
        g_free(gaym->roomlist_filter);
        gaym->roomlist_filter = NULL;
    }
}

void gaym_roomlist_expand_category(struct _PurpleRoomlist *list,
                                   struct _PurpleRoomlistRoom *category)
{
    PurpleRoomlistRoom *room = NULL;
    gchar *altname = NULL;
    gchar *altchan = NULL;
    int i = 0;

    if (category->type & PURPLE_ROOMLIST_ROOMTYPE_ROOM
        && !category->expanded_once) {

        category->expanded_once = TRUE;

        int max =
            purple_prefs_get_int("/plugins/prpl/gaym/chat_room_instances");

        gchar *name = category->fields->data;
        gchar *chan = category->fields->next->data;

        for (i = 1; i <= max; i++) {
            altname = g_strdup_printf("%.*s%d", strlen(name) - 1, name, i);
            altchan = g_strdup_printf("%.*s%d", strlen(chan) - 1, chan, i);

            room =
                purple_roomlist_room_new(PURPLE_ROOMLIST_ROOMTYPE_ROOM,
                                       altname, category);

            purple_roomlist_room_add_field(list, room, altname);
            purple_roomlist_room_add_field(list, room, altchan);
            purple_roomlist_room_add(list, room);
            g_free(altname);
            g_free(altchan);
        }
    }
    purple_roomlist_set_in_progress(list, FALSE);
}

static PurplePluginProtocolInfo prpl_info = {
    0,                          /* options */
    NULL,                       /* user_splits */
    NULL,                       /* protocol_options */
    {"jpg,jpeg,gif,bmp,ico", 57, 77, 57, 77, PURPLE_ICON_SCALE_DISPLAY},  /* icon_spec 
                                                                         */
    gaym_blist_icon,            /* list_icon */
    NULL,                        /* list_emblems */
    gaym_status_text,           /* status_text */
    gaym_tooltip_text,          /* tooltip_text */
    gaym_status_types,          /* status_types */
    gaym_blist_node_menu,       /* blist_node_menu */
    gaym_chat_join_info,        /* chat_info */
    gaym_chat_info_defaults,    /* chat_info_defaults */
    gaym_login,                 /* login */
    gaym_close,                 /* close */
    gaym_im_send,               /* send_im */
    gaym_set_info,              /* set_info */
    NULL,                       /* send_typing */
    gaym_get_info,              /* get_info */
    gaym_set_status,            /* set_status */
    NULL,                       /* set_idle */
    NULL,                       /* change_passwd */
    gaym_add_buddy,             /* add_buddy */
    NULL,                       /* add_buddies */
    gaym_remove_buddy,          /* remove_buddy */
    NULL,                       /* remove_buddies */
    gaym_add_permit,            /* add_permit */
    gaym_add_deny,              /* add_deny */
    gaym_rem_permit,            /* rem_permit */
    gaym_rem_deny,              /* rem_deny */
    gaym_set_permit_deny,       /* set_permit_deny */
    gaym_chat_join,             /* join_chat */
    NULL,                       /* reject_chat */
    gaym_get_chat_name,         /* get_chat_name */
    gaym_chat_invite,           /* chat_invite */
    gaym_chat_leave,            /* chat_leave */
    NULL,                       /* chat_whisper */
    gaym_chat_send,             /* chat_send */
    NULL,                       /* keepalive */
    NULL,                       /* register_user */
    NULL,                       /* get_cb_info */
    NULL,                       /* get_cb_away */
    NULL,                       /* alias_buddy */
    NULL,                       /* group_buddy */
    NULL,                       /* rename_group */
    NULL,                       /* buddy_free */
    NULL,                       /* convo_closed */
    NULL,                       /* normalize */
    NULL,                       /* set_buddy_icon */
    NULL,                       /* remove_group */
    NULL,                       /* get_cb_real_name */
    NULL,                       /* set_chat_topic */
    gaym_find_blist_chat,       /* find_blist_chat */
    gaym_roomlist_get_list,     /* roomlist_get_list */
    gaym_roomlist_cancel,       /* roomlist_cancel */
    gaym_roomlist_expand_category,      /* roomlist_expand_category */
    NULL,                       /* can_receive_file */
    NULL,      /* send_file */
    NULL,	/*new_xfr */
    NULL,	/* offline_mode */
    NULL,	/* whiteboard_prpl_ops */
    NULL,	/* send_raw */
};

void deref_one_user(gpointer * user, gpointer * data)
{

    struct gaym_conn *gaym = (struct gaym_conn *) data;
    PurpleConvChatBuddy *cb = (PurpleConvChatBuddy *) user;
    purple_debug_misc("gaym", "Removing %s in %x from list\n",
                    (char *) cb->name, cb);

    purple_debug_misc("    ", "Succes was: %i\n",
                    gaym_unreference_channel_member(gaym, cb->name));

}
static void gaym_clean_channel_members(PurpleConversation * conv)
{
    if (strncmp(conv->account->protocol_id, "prpl-gaym", 9))
        return;

    g_return_if_fail(conv != NULL);

    if (conv->type == PURPLE_CONV_TYPE_CHAT) {
        PurpleConvChat *chat = purple_conversation_get_chat_data(conv);
        PurpleConnection *gc = purple_conversation_get_gc(conv);
        g_return_if_fail(gc != NULL);
        struct gaym_conn *gaym = gc->proto_data;
        GList *users = purple_conv_chat_get_users(chat);
        purple_debug_misc("gaym", "got userlist %x length %i\n", users,
                        g_list_length(users));
        g_list_foreach(users, (GFunc) deref_one_user, gaym);
    } else if (conv->type == PURPLE_CONV_TYPE_IM) {
        purple_debug_misc("gaym", "removing reference to %s\n", conv->name);
        PurpleConnection *gc = purple_conversation_get_gc(conv);
        g_return_if_fail(gc != NULL);
        struct gaym_conn *gaym = gc->proto_data;
        gaym_unreference_channel_member(gaym, conv->name);
    }
}
static void gaym_get_photo_info(PurpleConversation * conv)
{
    char *buf;
    char *name;
    purple_debug_misc("******gaym",
                    "****************Got conversation-created signal\n");
    if (strncmp(conv->account->protocol_id, "prpl-gaym", 9) == 0
        && purple_conversation_get_type(conv) == PURPLE_CONV_TYPE_IM) {

        /**
         * First check to see if we already have the photo via
         * the buddy list process.
         */

        struct gaym_conn *gaym;

        PurpleConnection *gc = purple_conversation_get_gc(conv);
        gaym = (struct gaym_conn *) gc->proto_data;

        if (!gaym) {
            return;
        }

        struct gaym_buddy *ib =
            g_hash_table_lookup(gaym->buddies, conv->name);

        if (ib) {
            return;
        }

        /**
         * Since this person isn't in our buddy list, go ahead
         * with the WHOIS to get the photo for the IM thumbnail
         */


        name = gaym_nick_to_gcom_strdup(conv->name);
        buf = gaym_format(gaym, "vn", "WHOIS", name);
        purple_debug_misc("gaym", "Conversation triggered command: %s\n",
                        buf);
        gaym_send(gaym, buf);
        gaym_get_channel_member_reference(gaym, name);
        g_free(name);
        g_free(buf);
        // Opens a reference in channel_members.

    }
}

static PurplePluginPrefFrame *get_plugin_pref_frame(PurplePlugin * plugin)
{
    PurplePluginPrefFrame *frame;
    PurplePluginPref *ppref;

    frame = purple_plugin_pref_frame_new();

    ppref = purple_plugin_pref_new_with_label(_("Chat Rooms"));
    purple_plugin_pref_frame_add(frame, ppref);

    ppref =
        purple_plugin_pref_new_with_name_and_label
        ("/plugins/prpl/gaym/show_join", _("Show entrance announcement"));
    purple_plugin_pref_frame_add(frame, ppref);

    ppref =
        purple_plugin_pref_new_with_name_and_label
        ("/plugins/prpl/gaym/show_bio_with_join",
         _("Show member bio with entrance announcement"));
    purple_plugin_pref_frame_add(frame, ppref);

    ppref =
        purple_plugin_pref_new_with_name_and_label
        ("/plugins/prpl/gaym/show_part", _("Show exit announcement"));
    purple_plugin_pref_frame_add(frame, ppref);

    ppref =
        purple_plugin_pref_new_with_name_and_label
        ("/plugins/prpl/gaym/chat_room_instances",
         _("Number of chat room instances to display"));
    purple_plugin_pref_set_bounds(ppref, 0, 9);
    purple_plugin_pref_frame_add(frame, ppref);

    ppref =
        purple_plugin_pref_new_with_label(_
                                        ("Bio-Based Chat Room Activity Filtering"));
    purple_plugin_pref_frame_add(frame, ppref);

    ppref =
        purple_plugin_pref_new_with_name_and_label
        ("/plugins/prpl/gaym/botfilter_enable", _("Enable"));
    purple_plugin_pref_frame_add(frame, ppref);

    ppref =
        purple_plugin_pref_new_with_name_and_label
        ("/plugins/prpl/gaym/botfilter_ignore_null",
         _("Ignore if bio is blank"));
    purple_plugin_pref_frame_add(frame, ppref);

    ppref =
        purple_plugin_pref_new_with_name_and_label
        ("/plugins/prpl/gaym/botfilter_patterns",
         _
         ("Ignore if bio contains these patterns\n\t? = match any single character\n\t* = match zero, one, or more"));
    purple_plugin_pref_frame_add(frame, ppref);

    ppref =
        purple_plugin_pref_new_with_name_and_label
        ("/plugins/prpl/gaym/botfilter_sep",
         _("Above patterns are separated by"));
    purple_plugin_pref_set_max_length(ppref, 1);
    purple_plugin_pref_frame_add(frame, ppref);

    ppref =
        purple_plugin_pref_new_with_name_and_label
        ("/plugins/prpl/gaym/botfilter_url",
         _
         ("URL for GayBoi's spam database\n\tblank to disable\n\tchanges affect next login\n\tdefault is "
          GAYBOI_SPAM_URL));
    purple_plugin_pref_frame_add(frame, ppref);

    return frame;
}

static PurplePluginUiInfo prefs_info = {
    get_plugin_pref_frame
};

static PurplePluginInfo info = {
    PURPLE_PLUGIN_MAGIC,
    PURPLE_MAJOR_VERSION,
    PURPLE_MINOR_VERSION,
    PURPLE_PLUGIN_PROTOCOL,                                 /**< type           */
    NULL,                                                 /**< ui_requirement */
    0,                                                    /**< flags          */
    NULL,                                                 /**< dependencies   */
    PURPLE_PRIORITY_DEFAULT,                                /**< priority       */

    "prpl-gaym",                                          /**< id             */
    "GayM",                                               /**< name           */
    VERSION,                                              /**< version        */
    N_("GayM Protocol Plugin"),                           /**  summary        */
    N_("Gay.com Messaging based on IRC"),                 /**  description    */
    NULL,                                                 /**< author         */
    "http://qrc.berlios.de/",                             /**< homepage       */

    NULL,                                                 /**< load           */
    NULL,                                                 /**< unload         */
    NULL,                                                 /**< destroy        */
    NULL,                                                  /**< ui_info        */
    &prpl_info,                                           /**< extra_info     */
    &prefs_info,
    gaym_actions
};


void gaym_get_room_namelist(PurpleAccount * account, const char *room)
{


    if (!account || !room)
        return;

    const char *args[1] = { room };
    struct gaym_conn *gaym = (struct gaym_conn *) account->gc->proto_data;
    GaymNamelist *namelist = g_new0(GaymNamelist, 1);
    namelist->roomname = g_strdup(room);
    if (g_str_has_suffix(room, "*"))
        namelist->multi_room = TRUE;
    else
        namelist->multi_room = FALSE;
    namelist->members = NULL;
    namelist->num_rooms = 100;
    namelist->current = 0;

    g_queue_push_tail(gaym->namelists, namelist);
    // g_hash_table_insert(gaym->namelists, g_strdup(room), namelist); 

    // g_hash_table_insert(gaym->namelist_pending, list);
    gaym_cmd_who(gaym, NULL, NULL, args);
}



static void connect_signals(PurpleConnection * plugin)
{

    static gboolean connection_done = FALSE;
    if (connection_done)
        return;
    connection_done = TRUE;
    purple_debug_misc("gaym",
                    "CONNECTING SIGNALS: purple_conversations_get_handle(): %x\n",
                    purple_conversations_get_handle());
    purple_signal_connect(purple_conversations_get_handle(),
                        "conversation-created", plugin,
                        PURPLE_CALLBACK(gaym_get_photo_info), NULL);

    purple_signal_connect(purple_conversations_get_handle(),
                        "deleting-conversation", plugin,
                        PURPLE_CALLBACK(gaym_clean_channel_members), NULL);


    purple_signal_connect(purple_accounts_get_handle(), "request-namelist",
                        plugin, PURPLE_CALLBACK(gaym_get_room_namelist),
                        NULL);
    purple_signal_connect(purple_accounts_get_handle(), "request-info-quietly",
                        plugin, PURPLE_CALLBACK(gaym_get_info_quietly),
                        NULL);



}
static void _init_plugin(PurplePlugin * plugin)
{

    PurpleAccountOption *option;

    option = purple_account_option_string_new(_("Bio Line"), "bioline", "");
    prpl_info.protocol_options =
        g_list_prepend(prpl_info.protocol_options, option);

    option =
        purple_account_option_int_new(_("Port"), "port", IRC_DEFAULT_PORT);
    prpl_info.protocol_options =
        g_list_prepend(prpl_info.protocol_options, option);

    option =
        purple_account_option_string_new(_("Server"), "server",
                                       IRC_DEFAULT_SERVER);
    prpl_info.protocol_options =
        g_list_prepend(prpl_info.protocol_options, option);

    purple_signal_register(purple_accounts_get_handle(),
                         "info-updated",
                         purple_marshal_VOID__POINTER_POINTER, NULL, 2,
                         purple_value_new(PURPLE_TYPE_SUBTYPE,
                                        PURPLE_SUBTYPE_ACCOUNT),
                         purple_value_new(PURPLE_TYPE_POINTER,
                                        PURPLE_TYPE_CHAR));

    purple_signal_register(purple_accounts_get_handle(),
                         "namelist-complete",
                         purple_marshal_VOID__POINTER_POINTER, NULL, 2,
                         purple_value_new(PURPLE_TYPE_SUBTYPE,
                                        PURPLE_SUBTYPE_ACCOUNT),
                         purple_value_new(PURPLE_TYPE_POINTER));
    purple_signal_register(purple_accounts_get_handle(),
                         "request-namelist",
                         purple_marshal_VOID__POINTER_POINTER, NULL, 2,
                         purple_value_new(PURPLE_TYPE_SUBTYPE,
                                        PURPLE_SUBTYPE_ACCOUNT),
                         purple_value_new(PURPLE_TYPE_POINTER,
                                        PURPLE_TYPE_CHAR));
    purple_signal_register(purple_accounts_get_handle(),
                         "request-info-quietly",
                         purple_marshal_VOID__POINTER_POINTER, NULL, 2,
                         purple_value_new(PURPLE_TYPE_SUBTYPE,
                                        PURPLE_SUBTYPE_ACCOUNT),
                         purple_value_new(PURPLE_TYPE_POINTER,
                                        PURPLE_TYPE_CHAR));



    purple_prefs_add_none("/plugins/prpl/gaym");
    purple_prefs_add_int("/plugins/prpl/gaym/chat_room_instances", 4);
    purple_prefs_add_bool("/plugins/prpl/gaym/show_join", TRUE);
    purple_prefs_add_bool("/plugins/prpl/gaym/show_part", TRUE);
    purple_prefs_add_bool("/plugins/prpl/gaym/show_bio_with_join", TRUE);

    purple_prefs_add_bool("/plugins/prpl/gaym/botfilter_enable", FALSE);
    purple_prefs_add_bool("/plugins/prpl/gaym/botfilter_ignore_null", FALSE);
    purple_prefs_add_string("/plugins/prpl/gaym/botfilter_sep", "|");
    purple_prefs_add_string("/plugins/prpl/gaym/botfilter_patterns",
                          "NULL|MODE * -i|*dantcamboy*|*Free preview*|*epowerchat*|*Live gay sex cam show*|*camboiz*");
    purple_prefs_add_string("/plugins/prpl/gaym/botfilter_url",
                          GAYBOI_SPAM_URL);

    purple_prefs_connect_callback("/plugins/prpl/gaym/botfilter_url",
                                "botfilter_url", botfilter_url_changed_cb,
                                NULL);

    purple_prefs_add_string("/plugins/prpl/gaym/botfilter_url_result", "");
    purple_prefs_add_int("/plugins/prpl/gaym/botfilter_url_last_check", 0);

    _gaym_plugin = plugin;

    gaym_register_commands();
}

PURPLE_INIT_PLUGIN(gaym, _init_plugin, info);



/**
 * vim:tabstop=4:shiftwidth=4:expandtab:
 */
