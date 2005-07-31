/**
 * @file gaym.c
 *
 * gaim
 *
 * Copyright (C) 2003, Robbert Haarman <gaim@inglorion.net>
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

#include "internal.h"
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

static const char *gaym_blist_icon(GaimAccount * a, GaimBuddy * b);
static void gaym_blist_emblems(GaimBuddy * b, char **se, char **sw,
                               char **nw, char **ne);
static GList *gaym_away_states(GaimConnection * gc);
static GList *gaym_actions(GaimPlugin * plugin, gpointer context);
/* static GList *gaym_chat_info(GaimConnection *gc); */
static void gaym_login(GaimAccount * account);
static void gaym_login_cb(gpointer data, gint source,
                          GaimInputCondition cond);
static void gaym_close(GaimConnection * gc);
static int gaym_im_send(GaimConnection * gc, const char *who,
                        const char *what, GaimConvImFlags flags);
static int gaym_chat_send(GaimConnection * gc, int id, const char *what);
static void gaym_chat_join(GaimConnection * gc, GHashTable * data);
static void gaym_input_cb(gpointer data, gint source,
                          GaimInputCondition cond);

static guint gaym_nick_hash(const char *nick);
static gboolean gaym_nick_equal(const char *nick1, const char *nick2);
static void gaym_buddy_free(struct gaym_buddy *ib);
static void gaym_channel_member_free(GaymBuddy * cm);

static void gaym_buddy_append(char *name, struct gaym_buddy *ib,
                              BListWhois * blist_whois);
static void gaym_buddy_clear_done(char *name, struct gaym_buddy *ib,
                                  gpointer nothing);

static GaimPlugin *_gaym_plugin = NULL;

static const char *status_chars = "@+%&";

int gaym_send(struct gaym_conn *gaym, const char *buf)
{
    int ret;

    if (gaym->fd < 0)
        return -1;

    /* gaim_debug(GAIM_DEBUG_MISC, "gaym", "sent: %s", buf); */
    if ((ret = write(gaym->fd, buf, strlen(buf))) < 0)
        gaim_connection_error(gaim_account_get_connection(gaym->account),
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
        gaim_timeout_remove(gaym->timer);
        gaym->timer =
            gaim_timeout_add(BLIST_UPDATE_PERIOD,
                             (GSourceFunc) gaym_blist_timeout,
                             (gpointer) gaym);
        g_free(list);
        g_free(blist_whois);

        return TRUE;
    }
    gaym->blist_updating = TRUE;
    buf = gaym_format(gaym, "vn", "WHOIS", list);
    gaym_send(gaym, buf);
    gaim_timeout_remove(gaym->timer);
    gaym->timer =
        gaim_timeout_add(BLIST_CHUNK_INTERVAL,
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

static const char *gaym_blist_icon(GaimAccount * a, GaimBuddy * b)
{
    return "gaym";
}

static void gaym_blist_emblems(GaimBuddy * b, char **se, char **sw,
                               char **nw, char **ne)
{
    if (b->present == GAIM_BUDDY_OFFLINE)
        *se = "offline";
}

static char *gaym_status_text(GaimBuddy * buddy)
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

static char *gaym_tooltip_text(GaimBuddy * buddy)
{
    struct gaym_conn *gaym =
        (struct gaym_conn *) buddy->account->gc->proto_data;

    if (!gaym) {
        return NULL;
    }

    struct gaym_buddy *ib =
        g_hash_table_lookup(gaym->channel_members, gaim_normalize(gaym->account,buddy->name)); 
    if(!ib)
         ib=g_hash_table_lookup(gaym->buddies, gaim_normalize(gaym->account,buddy->name));
    
    if (!ib) {
        return g_strdup("No info found.");
    }

    return build_tooltip_text(ib);
}

static GList *gaym_away_states(GaimConnection * gc)
{
    return g_list_prepend(NULL, (gpointer) GAIM_AWAY_CUSTOM);
}

static void gaym_set_info(GaimConnection * gc, const char *info)
{
    struct gaym_conn *gaym = gc->proto_data;
    GaimAccount *account = gaim_connection_get_account(gc);
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

    if (gc->away && !tmpinfo) {
        /**
         * don't change any bio settings, since this is just
         * setting an away message
         */
    } else {
        if (gaym->bio) {
            g_free(gaym->bio);
        }
        if (tmpinfo && strlen(tmpinfo) > 0) {
            gaim_debug_misc("gaym", "option1, info=%x\n", tmpinfo);
            /* java client allows MAX_BIO_LEN characters */
            gaym->bio = g_strndup(tmpinfo, MAX_BIO_LEN);
        } else if (gaym->server_bioline
                   && strlen(gaym->server_bioline) > 0) {
            gaim_debug_misc("gaym", "option2\n");
            gaym->bio = gaym_bio_strdup(gaym->server_bioline);
        } else {
            gaim_debug_misc("gaym", "option3\n");
            gaym->bio = g_strdup("Gaim User");
        }
        gaim_account_set_user_info(account, gaym->bio);
        gaim_account_set_string(account, "bioline", gaym->bio);
        gaim_debug_info("gaym", "INFO=%x BIO=%x\n", tmpinfo, gaym->bio);
        gaim_debug_misc("gaym", "In login_cb, gc->account=%x\n",
                        gc->account);
    }

    bioline =
        g_strdup_printf("%s#%s\xC2\xA0 \xC2\xA0\001%s",
                        gaym->thumbnail ? gaym->thumbnail : "",
                        gc->away ? gc->away : (gaym->bio ? gaym->bio : ""),
                        gaym->server_stats ? gaym->server_stats : "");

    buf = gaym_format(gaym, "vvvv:", "USER",
                      gaim_account_get_username(account),
                      hostname, gaym->server, bioline);

    gaim_debug_misc("gaym", "BIO=%x\n", bioline);

    if (gaym_send(gaym, buf) < 0) {
        gaim_connection_error(gc, "Error registering with server");
    }

    if (tmpinfo) {
        g_free(tmpinfo);
    }
    g_free(bioline);
    g_free(buf);

    return;
}

static void gaym_show_set_info(GaimPluginAction * action)
{
    GaimConnection *gc = (GaimConnection *) action->context;
    gaim_account_request_change_user_info(gaim_connection_get_account(gc));
}

static GList *gaym_actions(GaimPlugin * plugin, gpointer context)
{
    GList *list = NULL;
    GaimPluginAction *act = NULL;

    act = gaim_plugin_action_new(_("Change Bio"), gaym_show_set_info);
    list = g_list_prepend(list, act);

    return list;
}

static void gaym_blist_join_chat_cb(GaimBlistNode * node, gpointer data)
{
    const char *args[1];

    GaimChat *chat = (GaimChat *) node;
    g_free(chat->alias);
    chat->alias = g_strdup("A new alias.");
    struct gaym_conn *gaym = chat->account->gc->proto_data;
    args[0] = data;

    g_return_if_fail(args[0] != NULL);
    g_return_if_fail(gaym != NULL);

    
    gaym_cmd_join(gaym, "join", NULL, args);
}

static GList *gaym_blist_node_menu(GaimBlistNode * node)
{
    GList *m = NULL;
    GaimBlistNodeAction *act = NULL;
    int i = 0;

    if (node->type != GAIM_BLIST_CHAT_NODE) {
        return m;
    }

    GaimChat *chat = (GaimChat *) node;
    char *channel = g_hash_table_lookup(chat->components, "channel");

    if (!channel) {
        return m;
    }

    if (!g_str_has_suffix(channel, "=*")) {
        return m;
    }

    char *label = NULL;
    char *instance = NULL;

    int max = gaim_prefs_get_int("/plugins/prpl/gaym/chat_room_instances");

    for (i = max; i > 0; i--) {
        label = g_strdup_printf(_("Join Room %d"), i);
        instance =
            g_strdup_printf("%.*s%d", strlen(channel) - 1, channel, i);
        act =
            gaim_blist_node_action_new(label, gaym_blist_join_chat_cb,
                                       instance);
        m = g_list_prepend(m, act);
    }
    return m;
}

static GList *gaym_chat_join_info(GaimConnection * gc)
{
    GList *m = NULL;
    struct proto_chat_entry *pce;

    pce = g_new0(struct proto_chat_entry, 1);
    pce->label = _("_Room:");
    pce->identifier = "channel";
    m = g_list_prepend(m, pce);

    return m;
}

GHashTable *gaym_chat_info_defaults(GaimConnection * gc,
                                    const char *chat_name)
{
    GHashTable *defaults;

    defaults =
        g_hash_table_new_full(g_str_hash, g_str_equal, NULL, g_free);

    if (chat_name != NULL)
        g_hash_table_insert(defaults, "channel", g_strdup(chat_name));

    return defaults;
}

static void gaym_login_with_chat_key(GaimAccount * account)
{
    GaimConnection *gc;
    struct gaym_conn *gaym;
    char *buf;
    const char *username = gaim_account_get_username(account);
    int err;

    gc = gaim_account_get_connection(account);
    gaym = gc->proto_data;

    buf = g_strdup_printf(_("Signon: %s"), username);
    gaim_connection_update_progress(gc, buf, 5, 6);
    g_free(buf);
    gaim_debug_misc("gaym", "Trying login to %s\n", gaym->server);
    err = gaim_proxy_connect(account, gaym->server,
                             gaim_account_get_int(account, "port",
                                                  IRC_DEFAULT_PORT),
                             gaym_login_cb, gc);
    if (err || !account->gc) {
        gaim_connection_error(gc, _("Couldn't create socket"));
        gaim_debug_misc("gaym", "err: %d, account->gc: %x\n", err,
                        account->gc);
        return;
    }

}

static void gaym_login(GaimAccount * account)
{
    GaimConnection *gc;
    struct gaym_conn *gaym;
    char *buf;
    const char *username = gaim_account_get_username(account);

    gc = gaim_account_get_connection(account);
    gc->flags |= GAIM_CONNECTION_NO_NEWLINES | GAIM_CONNECTION_AUTO_RESP;

    if (strpbrk(username, " \t\v\r\n") != NULL) {
        gaim_connection_error(gc,
                              _("IRC nicks may not contain whitespace"));
        return;
    }

    gc->proto_data = gaym = g_new0(struct gaym_conn, 1);
    gaym->account = account;


    /**
     * gaim_connection_set_display_name(gc, userparts[0]);
     */
    gaim_connection_set_display_name(gc, username);
    gaym->server =
        g_strdup(gaim_account_get_string
                 (account, "server", "www.gay.com"));
    /**
     * gaym->server = "www.gay.com";
     */
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

    gaym->hammers = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, (GDestroyNotify)hammer_cb_data_destroy);
    /**
     * The last parameter needs to be NULL here, since the same
     * field is added for both the key and the value (and if we
     * free it twice, thats bad and causes crashing!).
     */
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
    gaim_connection_update_progress(gc, buf, 1, 6);
    g_free(buf);


    /**
     * Making a change to try cached password first.
     * gaym_try_cached_password(account, gaym_login_with_chat_key);
     */
    gaym_get_chat_key_from_weblogin(account, gaym_login_with_chat_key);
}


static void gaym_get_configtxt_cb(gpointer proto_data,
                                  const gchar * config_text, size_t len)
{
    struct gaym_conn *gaym = (struct gaym_conn *) proto_data;
    GaimConnection *gc = gaim_account_get_connection(gaym->account);

    g_return_if_fail(config_text != NULL);

    gaym->confighash = gaym_properties_new(config_text);
    g_return_if_fail(gaym->confighash != NULL);

    // synchronize_deny_list(gc, gaym->confighash);

    return;
}
static void gaym_login_cb(gpointer data, gint source,
                          GaimInputCondition cond)
{
    GaimConnection *gc = data;
    struct gaym_conn *gaym = gc->proto_data;
    char hostname[256];
    char *buf;
    const char *username;
    const char *user_bioline = NULL;
    char *bioline;
    char *login_name;

    if (GAIM_CONNECTION_IS_VALID(gc)) {


        GList *connections = gaim_connections_get_all();

        if (source < 0) {
            gaim_connection_error(gc, _("Couldn't connect to host"));
            return;
        }

        if (!g_list_find(connections, gc)) {
            close(source);
            return;
        }

        gaym->fd = source;
        gaim_debug_misc("gaym", "In login_cb with chat_key=%s\n",
                        gaym->chat_key);
        if (gaym->chat_key) {

            buf = gaym_format(gaym, "vv", "PASS", gaym->chat_key);
            if (gaym_send(gaym, buf) < 0) {
                gaim_connection_error(gc, "Error sending password");
                return;
            }
            g_free(buf);
        } else {
            gaim_connection_error(gc,
                                  _
                                  ("Password wasn't recorded. Report bug."));
            return;
        }
        gethostname(hostname, sizeof(hostname));
        hostname[sizeof(hostname) - 1] = '\0';
        username = gaim_account_get_string(gaym->account, "username", "");
        user_bioline =
            g_strdup(gaim_account_get_string
                     (gaym->account, "bioline", ""));
        gaim_debug_info("gaym", "USER BIOLINE=%x\n", user_bioline);
        gaim_account_set_user_info(gc->account, user_bioline);
        gaim_debug_misc("gaym",
                        "In login_cb, user_bioline: %x, gc->account=%x\n",
                        user_bioline, gc->account);

        login_name =
            gaym_nick_to_gcom_strdup(gaim_connection_get_display_name(gc));
        bioline = g_strdup_printf("%s#%s\xC2\xA0 \xC2\xA0\001%s",
                                  gaym->thumbnail,
                                  user_bioline ? user_bioline : "",
                                  gaym->server_stats ? gaym->
                                  server_stats : "");

        buf = gaym_format(gaym, "vn", "NICK", login_name);
        gaim_debug_misc("gaym", "Command: %s\n", buf);

        if (gaym_send(gaym, buf) < 0) {
            gaim_connection_error(gc, "Error sending nickname");
            return;
        }
        g_free(buf);
        buf =
            gaym_format(gaym, "vvvv:", "USER", login_name, hostname,
                        gaym->server, bioline);

        gaim_debug_misc("gaym", "Command: %s\n", buf);
        if (gaym_send(gaym, buf) < 0) {
            gaim_connection_error(gc, "Error registering with server");
            return;
        }
        g_free(login_name);
        g_free(buf);

        const char *server = gaim_account_get_string(gc->account, "server",
                                                     IRC_DEFAULT_SERVER);
        char *url =
            g_strdup_printf
            ("http://%s/messenger/config.txt?%s", server, gaym->chat_key);

        char *user_agent = "Mozilla/4.0";

        get_spamlist_from_web();
        gaim_url_fetch(url, FALSE, user_agent, FALSE,
                       gaym_get_configtxt_cb, gaym);

        g_free(url);
        gc->inpa =
            gaim_input_add(gaym->fd, GAIM_INPUT_READ, gaym_input_cb, gc);


    }
}

void kill_hammer(gpointer* room, struct hammer_cb_data* data, gpointer *null) {
    hammer_cb_data_destroy(data);
}

static void gaym_close(GaimConnection * gc)
{
    struct gaym_conn *gaym = gc->proto_data;

    gaim_debug_misc("gaym", "gaym close function has been called\n");
    if (gaym == NULL)
        return;

    gaym_cmd_quit(gaym, "quit", NULL, NULL);

    if (gc->inpa)
        gaim_input_remove(gc->inpa);

    g_free(gaym->inbuf);
    gaim_debug_misc("gaym", "closing fd %i\n", gaym->fd);
    close(gaym->fd);

    if (gaym->timer)
        gaim_timeout_remove(gaym->timer);

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

    g_hash_table_foreach(gaym->hammers, (GHFunc)kill_hammer, NULL);

    g_free(gaym->server);
    g_free(gaym);
}

static int gaym_im_send(GaimConnection * gc, const char *who,
                        const char *what, GaimConvImFlags flags)
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
    if (flags & GAIM_CONV_IM_AUTO_RESP) {
        stripped_msg = gaim_markup_strip_html(what);
        automsg = g_strdup_printf("<AUTO-REPLY> %s", stripped_msg);
        g_free(stripped_msg);
        args[1] = automsg;

    } else {
        args[1] = what;
    }
    gaym_cmd_privmsg(gaym, "msg", NULL, args);
    if (automsg) {
        g_free(automsg);
    }
    return 1;
}

static void gaym_get_info(GaimConnection * gc, const char *who)
{
    struct gaym_conn *gaym = gc->proto_data;
    const char *args[1];
    args[0] = who;

    char *normalized = g_strdup(gaim_normalize(gc->account, who));
    /**
     * We are adding the same char* to both the key and the value.
     * If this changes, we need to change the corresponding
     * g_hash_table_new_full() so that things are properly cleaned
     * up during the remove/destroy phase.
     */
    g_hash_table_insert(gaym->info_window_needed, normalized, normalized);
    gaym_cmd_whois(gaym, "whois", NULL, args);
}

static void gaym_set_away(GaimConnection * gc, const char *state,
                          const char *msg)
{
    char *bio = NULL;
    char *tmpmsg = NULL;
    int i = 0;
    struct gaym_conn *gaym = gc->proto_data;

    if (gc->away) {
        g_free(gc->away);
        gc->away = NULL;
    }

    /**
     * In addition to setting the away message, set the Bio to the
     * away message; if the away message is NULL, then set the Bio
     * to the original bio.
     */

    if (msg) {
        tmpmsg = g_strdup(msg);
        for (i = 0; i < strlen(tmpmsg); i++) {
            if (tmpmsg[i] == '\n') {
                tmpmsg[i] = ' ';
            }
        }
        tmpmsg = g_strstrip(tmpmsg);

        gc->away = g_strndup(tmpmsg, MAX_BIO_LEN);
        gaym_set_info(gc, NULL);
        g_free(tmpmsg);
    } else {
        if (gaym && gaym->bio) {
            bio = g_strdup(gaym->bio);
            gaym_set_info(gc, bio);
            g_free(bio);
        } else {
            gaym_set_info(gc, NULL);
        }
    }

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
        g_hash_table_insert(gaym->channel_members, g_strdup(gaim_normalize(gaym->account,name)),
                            channel_member);
        gaim_debug_misc("gaym", "Creating channel_members entry for %s\n",
                        name);
        return g_hash_table_lookup(gaym->channel_members, gaim_normalize(gaym->account, name));
    } else {
        gaim_debug_misc("gaym",
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
        (GaymBuddy *) g_hash_table_lookup(gaym->channel_members, gaim_normalize(gaym->account,name));
    if (!channel_member)
        return FALSE;
    else {

        if (channel_member->ref_count <= 0)
            gaim_debug_error("gaym",
                             "****Reference counting error with channel members struct.\n");

        channel_member->ref_count--;

        if (channel_member->ref_count == 0) {
            gaim_debug_misc("gaym", "Removing %s from channel_members\n",
                            name);
            return g_hash_table_remove(gaym->channel_members, gaim_normalize(gaym->account, name));
        }
        return FALSE;
    }
}

static void gaym_add_buddy(GaimConnection * gc, GaimBuddy * buddy,
                           GaimGroup * group)
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
    gaim_debug_misc("gaym", "Add buddy: %s\n", buddy->name);
    /**
     * if the timer isn't set, this is during signon, so we don't want to
     * flood ourself off with WHOIS's, so we don't, but after that we want
     * to know when someone's online asap
     */
    if (gaym->timer)
        gaym_whois_one(gaym, ib);
}

static void gaym_remove_buddy(GaimConnection * gc, GaimBuddy * buddy,
                              GaimGroup * group)
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

    GSList *buddies = gaim_find_buddies(gaym->account, buddy->name);
    guint length = g_slist_length(buddies);

    if (length < 2) {
        g_hash_table_remove(gaym->buddies, buddy->name);
    }

    g_slist_free(buddies);
}

static void gaym_input_cb(gpointer data, gint source,
                          GaimInputCondition cond)
{
    GaimConnection *gc = data;
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
        gaim_connection_error(gc, _("Read error"));
        return;
    } else if (len == 0) {
        gaim_connection_error(gc, _("Server has disconnected"));
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

static void gaym_add_permit(GaimConnection * gc, const char *name)
{
    if (!gaym_nick_check(name)) {
        gaim_privacy_permit_remove(gc->account, name, TRUE);
        gaim_notify_error(gc, _("Invalid User Name"), name,
                          _("Invalid user name not added."));
    } else {
        gaym_privacy_change(gc, name);
    }
}

static void gaym_add_deny(GaimConnection * gc, const char *name)
{
    if (!gaym_nick_check(name)) {
        gaim_privacy_deny_remove(gc->account, name, TRUE);
        gaim_notify_error(gc, _("Invalid User Name"), name,
                          _("Invalid user name not added."));
        return;
    }
    gaym_server_store_deny(gc, name, TRUE);
    gaym_privacy_change(gc, name);
}

static void gaym_rem_permit(GaimConnection * gc, const char *name)
{
    gaym_privacy_change(gc, name);
}

static void gaym_rem_deny(GaimConnection * gc, const char *name)
{
    gaym_server_store_deny(gc, name, FALSE);
    gaym_privacy_change(gc, name);
}

static void gaym_set_permit_deny(GaimConnection * gc)
{
    gaym_privacy_change(gc, NULL);
}

static void gaym_warn(GaimConnection * gc, const char *who,
                      gboolean anonymous)
{
    void *handle = NULL;
    struct gaym_conn *gaym = gc->proto_data;
    char *buf =
        g_strdup_printf
        ("http://%s/members/report/form.html?area=chat&room=&report=%s",
         gaym->server, who);
    gaim_notify_uri(handle, buf);
    g_free(buf);
}

static void gaym_chat_join(GaimConnection * gc, GHashTable * data)
{
    struct gaym_conn *gaym = gc->proto_data;
    const char *args[1];
    char *alias = NULL;

    GaimChat *c = NULL;

    /**
     * need a copy, because data gets
     * destroyed in roomlist.c
     */
    GHashTable *chatinfo = NULL;

    args[0] = g_hash_table_lookup(data, "channel");

    if (args[0]) {
        alias = g_hash_table_lookup(data, "description");
        c = gaim_blist_find_chat(gaim_connection_get_account(gc), args[0]);
        if (!c) {
            chatinfo = g_hash_table_new(g_str_hash, g_str_equal);

            g_hash_table_replace(chatinfo, "channel", g_strdup(args[0]));

            c = gaim_chat_new(gaim_connection_get_account(gc),
                              alias, chatinfo);

            gaim_blist_add_chat(c, NULL, NULL);
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

static void gaym_chat_invite(GaimConnection * gc, int id,
                             const char *message, const char *name)
{
    struct gaym_conn *gaym = gc->proto_data;
    GaimConversation *convo = gaim_find_chat(gc, id);
    const char *args[2];

    if (!convo) {
        gaim_debug(GAIM_DEBUG_ERROR, "gaym",
                   "Got chat invite request for bogus chat\n");
        return;
    }
    args[0] = name;
    args[1] = gaim_conversation_get_name(convo);
    gaym_cmd_invite(gaym, "invite", gaim_conversation_get_name(convo),
                    args);
}

static void gaym_chat_leave(GaimConnection * gc, int id)
{
    struct gaym_conn *gaym = gc->proto_data;
    GaimConversation *convo = gaim_find_chat(gc, id);
    const char *args[2];

    if (!convo)
        return;

    args[0] = gaim_conversation_get_name(convo);
    args[1] = NULL;
    gaym_cmd_part(gaym, "part", gaim_conversation_get_name(convo), args);
    serv_got_chat_left(gc, id);
}

static int gaym_chat_send(GaimConnection * gc, int id, const char *what)
{
    struct gaym_conn *gaym = gc->proto_data;
    GaimConversation *convo = gaim_find_chat(gc, id);
    const char *args[2];
    char *tmp;

    if (!convo) {
        gaim_debug(GAIM_DEBUG_ERROR, "gaym",
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

    tmp = gaim_escape_html(what);
    serv_got_chat_in(gc, id, gaim_connection_get_display_name(gc), 0, tmp,
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
    return (gaim_utf8_strcasecmp(nick1, nick2) == 0);
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

static GaimChat *gaym_find_blist_chat(GaimAccount * account,
                                      const char *name)
{
    char *chat_name;
    GaimChat *chat;
    GaimPlugin *prpl;
    GaimPluginProtocolInfo *prpl_info = NULL;
    struct proto_chat_entry *pce;
    GaimBlistNode *node, *group;
    GList *parts;

    GaimBuddyList *gaimbuddylist = gaim_get_blist();

    g_return_val_if_fail(gaimbuddylist != NULL, NULL);
    g_return_val_if_fail((name != NULL) && (*name != '\0'), NULL);

    if (!gaim_account_is_connected(account))
        return NULL;

    prpl = gaim_find_prpl(gaim_account_get_protocol_id(account));
    prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(prpl);

    for (group = gaimbuddylist->root; group != NULL; group = group->next) {
        for (node = group->child; node != NULL; node = node->next) {
            if (GAIM_BLIST_NODE_IS_CHAT(node)) {

                chat = (GaimChat *) node;

                if (account != chat->account)
                    continue;

                parts =
                    prpl_info->
                    chat_info(gaim_account_get_connection(chat->account));

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

static GaimRoomlist *gaym_roomlist_get_list(GaimConnection * gc)
{
    struct gaym_conn *gaym;
    GList *fields = NULL;
    GaimRoomlistField *f;
    char *buf;

    gaym = gc->proto_data;

    if (gaym->roomlist) {
        gaim_roomlist_unref(gaym->roomlist);
    }

    gaym->roomlist = gaim_roomlist_new(gaim_connection_get_account(gc));

    f = gaim_roomlist_field_new(GAIM_ROOMLIST_FIELD_STRING, _("Channel"),
                                "channel", FALSE);
    fields = g_list_prepend(fields, f);

    f = gaim_roomlist_field_new(GAIM_ROOMLIST_FIELD_STRING, "",
                                "description", TRUE);
    fields = g_list_prepend(fields, f);

    gaim_roomlist_set_fields(gaym->roomlist, fields);

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

static void gaym_roomlist_cancel(struct _GaimRoomlist *list)
{
    GaimConnection *gc = gaim_account_get_connection(list->account);
    struct gaym_conn *gaym;

    if (gc == NULL)
        return;

    gaym = gc->proto_data;

    gaim_roomlist_set_in_progress(list, FALSE);

    if (gaym->roomlist == list) {
        g_list_free(g_list_nth_data(list->rooms, 0));
        gaym->roomlist = NULL;
        gaim_roomlist_unref(list);
    }

    if (gaym->roomlist_filter) {
        g_free(gaym->roomlist_filter);
        gaym->roomlist_filter = NULL;
    }
}

void gaym_roomlist_expand_category(struct _GaimRoomlist *list,
                                   struct _GaimRoomlistRoom *category)
{
    GaimRoomlistRoom *room = NULL;
    gchar *altname = NULL;
    gchar *altchan = NULL;
    int i = 0;

    if (category->type & GAIM_ROOMLIST_ROOMTYPE_ROOM
        && !category->expanded_once) {

        category->expanded_once = TRUE;

        int max =
            gaim_prefs_get_int("/plugins/prpl/gaym/chat_room_instances");

        gchar *name = category->fields->data;
        gchar *chan = category->fields->next->data;

        for (i = 1; i <= max; i++) {
            altname = g_strdup_printf("%.*s%d", strlen(name) - 1, name, i);
            altchan = g_strdup_printf("%.*s%d", strlen(chan) - 1, chan, i);

            room =
                gaim_roomlist_room_new(GAIM_ROOMLIST_ROOMTYPE_ROOM,
                                       altname, category);

            gaim_roomlist_room_add_field(list, room, altname);
            gaim_roomlist_room_add_field(list, room, altchan);
            gaim_roomlist_room_add(list, room);
            g_free(altname);
            g_free(altchan);
        }
    }
    gaim_roomlist_set_in_progress(list, FALSE);
}

static GaimPluginProtocolInfo prpl_info = {
    0,                          /* options */
    NULL,                       /* user_splits */
    NULL,                       /* protocol_options */
    {"jpg", 57, 77, 57, 77},    /* icon_spec */
    gaym_blist_icon,            /* list_icon */
    gaym_blist_emblems,         /* list_emblems */
    gaym_status_text,           /* status_text */
    gaym_tooltip_text,          /* tooltip_text */
    gaym_away_states,           /* away_states */
    gaym_blist_node_menu,       /* blist_node_menu */
    gaym_chat_join_info,        /* chat_info */
    gaym_chat_info_defaults,    /* chat_info_defaults */
    gaym_login,                 /* login */
    gaym_close,                 /* close */
    gaym_im_send,               /* send_im */
    gaym_set_info,              /* set_info */
    NULL,                       /* send_typing */
    gaym_get_info,              /* get_info */
    gaym_set_away,              /* set_away */
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
    gaym_warn,                  /* warn */
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
    gaym_dccsend_send_file      /* send_file */
};

void deref_one_user(gpointer * user, gpointer * data)
{

    struct gaym_conn *gaym = (struct gaym_conn *) data;
    GaimConvChatBuddy *cb = (GaimConvChatBuddy *) user;
    gaim_debug_misc("gaym", "Removing %s in %x from list\n",
                    (char *) cb->name, cb);

    gaim_debug_misc("    ", "Succes was: %i\n",
                    gaym_unreference_channel_member(gaym, cb->name));

}
static void gaym_clean_channel_members(GaimConversation * conv)
{

    g_return_if_fail(conv != NULL);

    if (conv->type == GAIM_CONV_CHAT) {
        GaimConvChat *chat = gaim_conversation_get_chat_data(conv);
        GaimConnection *gc = gaim_conversation_get_gc(conv);
        g_return_if_fail(gc != NULL);
        struct gaym_conn *gaym = gc->proto_data;
        GList *users = gaim_conv_chat_get_users(chat);
        gaim_debug_misc("gaym", "got userlist %x length %i\n", users,
                        g_list_length(users));
        g_list_foreach(users, (GFunc) deref_one_user, gaym);
    } else if (conv->type == GAIM_CONV_IM) {
        gaim_debug_misc("gaym", "removing reference to %s\n", conv->name);
        GaimConnection *gc = gaim_conversation_get_gc(conv);
        g_return_if_fail(gc != NULL);
        struct gaym_conn *gaym = gc->proto_data;
        gaym_unreference_channel_member(gaym, conv->name);
    }
}
static void gaym_get_photo_info(GaimConversation * conv)
{
    char *buf;
    char *name;
    gaim_debug_misc("gaym", "Got conversation-created signal\n");
    if (strncmp(conv->account->protocol_id, "prpl-gaym", 9) == 0
        && gaim_conversation_get_type(conv) == GAIM_CONV_IM) {

        /**
         * First check to see if we already have the photo via
         * the buddy list process.
         */

        struct gaym_conn *gaym;

        GaimConnection *gc = gaim_conversation_get_gc(conv);
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
        gaim_debug_misc("gaym", "Conversation triggered command: %s\n",
                        buf);
        gaym_send(gaym, buf);
        gaym_get_channel_member_reference(gaym, name);
        g_free(name);
        g_free(buf);
        // Opens a reference in channel_members.

    }
}

static GaimPluginPrefFrame *get_plugin_pref_frame(GaimPlugin * plugin)
{
    GaimPluginPrefFrame *frame;
    GaimPluginPref *ppref;

    frame = gaim_plugin_pref_frame_new();

    ppref = gaim_plugin_pref_new_with_label(_("Chat Rooms"));
    gaim_plugin_pref_frame_add(frame, ppref);

    ppref =
        gaim_plugin_pref_new_with_name_and_label
        ("/plugins/prpl/gaym/show_join", _("Show entrance announcement"));
    gaim_plugin_pref_frame_add(frame, ppref);

    ppref =
        gaim_plugin_pref_new_with_name_and_label
        ("/plugins/prpl/gaym/show_bio_with_join",
         _("Show member bio with entrance announcement"));
    gaim_plugin_pref_frame_add(frame, ppref);

    ppref =
        gaim_plugin_pref_new_with_name_and_label
        ("/plugins/prpl/gaym/show_part", _("Show exit announcement"));
    gaim_plugin_pref_frame_add(frame, ppref);

    ppref =
        gaim_plugin_pref_new_with_name_and_label
        ("/plugins/prpl/gaym/chat_room_instances",
         _("Number of chat room instances to display"));
    gaim_plugin_pref_set_bounds(ppref, 0, 9);
    gaim_plugin_pref_frame_add(frame, ppref);

    ppref =
        gaim_plugin_pref_new_with_label(_
                                        ("Bio-Based Chat Room Activity Filtering"));
    gaim_plugin_pref_frame_add(frame, ppref);

    ppref =
        gaim_plugin_pref_new_with_name_and_label
        ("/plugins/prpl/gaym/botfilter_enable", _("Enable"));
    gaim_plugin_pref_frame_add(frame, ppref);

    ppref =
        gaim_plugin_pref_new_with_name_and_label
        ("/plugins/prpl/gaym/botfilter_ignore_null",
         _("Ignore if bio is blank"));
    gaim_plugin_pref_frame_add(frame, ppref);

    ppref =
        gaim_plugin_pref_new_with_name_and_label
        ("/plugins/prpl/gaym/botfilter_patterns",
         _
         ("Ignore if bio contains these patterns\n\t? = match any single character\n\t* = match zero, one, or more"));
    gaim_plugin_pref_frame_add(frame, ppref);

    ppref =
        gaim_plugin_pref_new_with_name_and_label
        ("/plugins/prpl/gaym/botfilter_sep",
         _("Above patterns are separated by"));
    gaim_plugin_pref_set_max_length(ppref, 1);
    gaim_plugin_pref_frame_add(frame, ppref);

    ppref =
        gaim_plugin_pref_new_with_name_and_label
        ("/plugins/prpl/gaym/botfilter_url",
         _
         ("URL for GayBoi's spam database\n\tblank to disable\n\tchanges affect next login\n\tdefault is "
          GAYBOI_SPAM_URL));
    gaim_plugin_pref_frame_add(frame, ppref);

    return frame;
}

static GaimPluginUiInfo prefs_info = {
    get_plugin_pref_frame
};

static GaimPluginInfo info = {
    GAIM_PLUGIN_MAGIC,
    GAIM_MAJOR_VERSION,
    GAIM_MINOR_VERSION,
    GAIM_PLUGIN_PROTOCOL,                                 /**< type           */
    NULL,                                                 /**< ui_requirement */
    0,                                                    /**< flags          */
    NULL,                                                 /**< dependencies   */
    GAIM_PRIORITY_DEFAULT,                                /**< priority       */

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

static void _init_plugin(GaimPlugin * plugin)
{

    GaimAccountOption *option;

    option = gaim_account_option_string_new(_("Bio Line"), "bioline", "");
    prpl_info.protocol_options =
        g_list_prepend(prpl_info.protocol_options, option);

    option =
        gaim_account_option_int_new(_("Port"), "port", IRC_DEFAULT_PORT);
    prpl_info.protocol_options =
        g_list_prepend(prpl_info.protocol_options, option);

    option =
        gaim_account_option_string_new(_("Server"), "server",
                                       IRC_DEFAULT_SERVER);
    prpl_info.protocol_options =
        g_list_prepend(prpl_info.protocol_options, option);

    /**
     * We have to pull thumbnails, since they aren't pushed like with
     * other protocols.
     */
    gaim_signal_connect(gaim_conversations_get_handle(),
                        "conversation-created", plugin,
                        GAIM_CALLBACK(gaym_get_photo_info), NULL);


    gaim_signal_connect(gaim_conversations_get_handle(),
                        "deleting-conversation", plugin,
                        GAIM_CALLBACK(gaym_clean_channel_members), NULL);

    gaim_signal_register(gaim_accounts_get_handle(),
                         "info-updated",
                         gaim_marshal_VOID__POINTER_POINTER, NULL, 2,
                         gaim_value_new(GAIM_TYPE_SUBTYPE,
                                        GAIM_SUBTYPE_ACCOUNT),
                         gaim_value_new(GAIM_TYPE_POINTER,
                                        GAIM_TYPE_CHAR));



    gaim_prefs_add_none("/plugins/prpl/gaym");
    gaim_prefs_add_int("/plugins/prpl/gaym/chat_room_instances", 4);
    gaim_prefs_add_bool("/plugins/prpl/gaym/show_join", TRUE);
    gaim_prefs_add_bool("/plugins/prpl/gaym/show_part", TRUE);
    gaim_prefs_add_bool("/plugins/prpl/gaym/show_bio_with_join", TRUE);

    gaim_prefs_add_bool("/plugins/prpl/gaym/botfilter_enable", FALSE);
    gaim_prefs_add_bool("/plugins/prpl/gaym/botfilter_ignore_null", FALSE);
    gaim_prefs_add_string("/plugins/prpl/gaym/botfilter_sep", "|");
    gaim_prefs_add_string("/plugins/prpl/gaym/botfilter_patterns",
                          "NULL|MODE * -i|*dantcamboy*|*Free preview*|*epowerchat*|*Live gay sex cam show*|*camboiz*");
    gaim_prefs_add_string("/plugins/prpl/gaym/botfilter_url",
                          GAYBOI_SPAM_URL);

    gaim_prefs_connect_callback("/plugins/prpl/gaym/botfilter_url",
                                botfilter_url_changed_cb, NULL);

    gaim_prefs_add_string("/plugins/prpl/gaym/botfilter_url_result", "");
    gaim_prefs_add_int("/plugins/prpl/gaym/botfilter_url_last_check", 0);

    _gaym_plugin = plugin;

    gaym_register_commands();
}

GAIM_INIT_PLUGIN(gaym, _init_plugin, info);



/**
 * vim:tabstop=4:shiftwidth=4:expandtab:
 */
