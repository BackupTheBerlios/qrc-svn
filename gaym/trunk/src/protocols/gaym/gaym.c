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

#include "gaym.h"



static const char *gaym_blist_icon(GaimAccount *a, GaimBuddy *b);
static void gaym_blist_emblems(GaimBuddy *b, char **se, char **sw, char **nw, char **ne);
static GList *gaym_away_states(GaimConnection *gc);
static GList *gaym_actions(GaimPlugin *plugin, gpointer context);
/* static GList *gaym_chat_info(GaimConnection *gc); */
static void gaym_login(GaimAccount *account);
static void gaym_login_cb(gpointer data, gint source, GaimInputCondition cond);
static void gaym_close(GaimConnection *gc);
static int gaym_im_send(GaimConnection *gc, const char *who, const char *what, GaimConvImFlags flags);
static int gaym_chat_send(GaimConnection *gc, int id, const char *what);
static void gaym_chat_join (GaimConnection *gc, GHashTable *data);
static void gaym_input_cb(gpointer data, gint source, GaimInputCondition cond);

static guint gaym_nick_hash(const char *nick);
static gboolean gaym_nick_equal(const char *nick1, const char *nick2);
static void gaym_buddy_free(struct gaym_buddy *ib);

static void gaym_buddy_append(char *name, struct gaym_buddy *ib, GString *string);
static void gaym_buddy_clear_done(char *name, struct gaym_buddy *ib, gpointer nothing);
	
static GaimPlugin *_gaym_plugin = NULL;

static const char *status_chars = "@+%&";


static void gaym_view_motd(GaimPluginAction *action)
{
	GaimConnection *gc = (GaimConnection *) action->context;
	struct gaym_conn *gaym;
	char *title;

	if (gc == NULL || gc->proto_data == NULL) {
		gaim_debug(GAIM_DEBUG_ERROR, "gaym", "got MOTD request for NULL gc\n");
		return;
	}
	gaym = gc->proto_data;
	if (gaym->motd == NULL) {
		gaim_notify_error(gc, _("Error displaying MOTD"), _("No MOTD available"),
				  _("There is no MOTD associated with this connection."));
		return;
	}
	title = g_strdup_printf(_("MOTD for %s"), gaym->server);
	gaim_notify_formatted(gc, title, title, NULL, gaym->motd->str, NULL, NULL);
}

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

/* XXX I don't like messing directly with these buddies */
gboolean gaym_blist_timeout(struct gaym_conn *gaym)
{
	GString *string = g_string_sized_new(512);
	char *list, *buf;

	g_hash_table_foreach(gaym->buddies, (GHFunc)gaym_buddy_append, (gpointer)string);

	list = g_string_free(string, FALSE);
	if (!list || !strlen(list)) {
		g_hash_table_foreach(gaym->buddies, (GHFunc)gaym_buddy_clear_done, NULL);
		gaim_timeout_remove(gaym->timer);
		gaym->timer = gaim_timeout_add(BLIST_UPDATE_PERIOD, (GSourceFunc)gaym_blist_timeout, (gpointer)gaym);
		g_free(list);
		
		return TRUE;
	}

	gaym->blist_updating=TRUE;
	buf = gaym_format(gaym, "vn", "ISON", list);
	gaym_send(gaym, buf);
	gaim_timeout_remove(gaym->timer);
	gaym->timer = gaim_timeout_add(BLIST_CHUNK_INTERVAL, (GSourceFunc)gaym_blist_timeout, (gpointer)gaym);
		
	g_free(buf);
	
	g_free(list);
	

	return TRUE;
}

static void gaym_buddy_clear_done(char *name, struct gaym_buddy *ib, gpointer nothing)
{
	ib->done=FALSE;
}
static void gaym_buddy_append(char *name, struct gaym_buddy *ib, GString *string)
{
	if((strlen(name) + string->len) > CHUNK_SIZE)
		return;
	else if (ib->done == FALSE)
	{
		ib->stale = TRUE;
		ib->done = TRUE;
		ib->flag = FALSE;
		g_string_append_printf(string, "%s ", name);
		return;
	}
	
}



static void gaym_ison_one(struct gaym_conn *gaym, struct gaym_buddy *ib)
{
	char *buf;

	ib->flag = FALSE;
	buf = gaym_format(gaym, "vn", "ISON", ib->name);
	gaym_send(gaym, buf);
	g_free(buf);
}


static const char *gaym_blist_icon(GaimAccount *a, GaimBuddy *b)
{
	return "gaym";
}

static void gaym_blist_emblems(GaimBuddy *b, char **se, char **sw, char **nw, char **ne)
{
	if (b->present == GAIM_BUDDY_OFFLINE)
		*se = "offline";
}

static GList *gaym_away_states(GaimConnection *gc)
{
	return g_list_append(NULL, (gpointer)GAIM_AWAY_CUSTOM);
}

static GList *gaym_actions(GaimPlugin *plugin, gpointer context)
{
	GList *list = NULL;
	GaimPluginAction *act = NULL;

	act = gaim_plugin_action_new(_("View MOTD"), gaym_view_motd);
	list = g_list_append(list, act);

	return list;
}

#if 0
static GList *gaym_blist_node_menu(GaimBlistNode *node)
{
	GList *m = NULL;
	GaimBlistNodeAction *act;

	return m;
}
#endif

static GList *gaym_chat_join_info(GaimConnection *gc)
{
	GList *m = NULL;
	struct proto_chat_entry *pce;

	pce = g_new0(struct proto_chat_entry, 1);
	pce->label = _("_Channel:");
	pce->identifier = "channel";
	m = g_list_append(m, pce);

	pce = g_new0(struct proto_chat_entry, 1);
	pce->label = _("_Password:");
	pce->identifier = "password";
	pce->secret = TRUE;
	m = g_list_append(m, pce);

	return m;
}

GHashTable *gaym_chat_info_defaults(GaimConnection *gc, const char *chat_name)
{
	GHashTable *defaults;

	defaults = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, g_free);

	if (chat_name != NULL)
		g_hash_table_insert(defaults, "channel", g_strdup(chat_name));

	return defaults;
}

static void gaym_login_with_hash(GaimAccount *account)
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

	gaim_debug_misc("gaym","Trying login to %s\n",gaym->server);
	err = gaim_proxy_connect(account, gaym->server, 
				 gaim_account_get_int(account, "port", IRC_DEFAULT_PORT),
				 gaym_login_cb, gc);
	if (err || !account->gc) {
		gaim_connection_error(gc, _("Couldn't create socket"));
		gaim_debug_misc("gaym","err: %d, account->gc: %x\n",err,account->gc);
		return;
	}

}

static void gaym_login(GaimAccount *account) {
	GaimConnection *gc;
	struct gaym_conn *gaym;
	char *buf;
	const char *username = gaim_account_get_username(account);

	gc = gaim_account_get_connection(account);
	gc->flags |= GAIM_CONNECTION_NO_NEWLINES;

	if (strpbrk(username, " \t\v\r\n") != NULL) {
		gaim_connection_error(gc, _("IRC nicks may not contain whitespace"));
		return;
	}

	gc->proto_data = gaym = g_new0(struct gaym_conn, 1);
	gaym->account = account;

	
	//gaim_connection_set_display_name(gc, userparts[0]);
	gaim_connection_set_display_name(gc,username);
	gaym->server = g_strdup(gaim_account_get_string(account, "server", "www.gay.com"));
	//gaym->server = "www.gay.com";
	gaym->buddies = g_hash_table_new_full((GHashFunc)gaym_nick_hash, (GEqualFunc)gaym_nick_equal, 
					     NULL, (GDestroyNotify)gaym_buddy_free);
	gaym->cmds = g_hash_table_new(g_str_hash, g_str_equal);
	gaym_cmd_table_build(gaym);
	gaym->msgs = g_hash_table_new(g_str_hash, g_str_equal);
	gaym_msg_table_build(gaym);

	buf = g_strdup_printf(_("Signon: %s"), username);
	gaim_connection_update_progress(gc, buf, 1, 6);
	g_free(buf);

	gaym_get_hash_from_weblogin(account, gaym_login_with_hash);


}
static void gaym_login_cb(gpointer data, gint source, GaimInputCondition cond)
{
	GaimConnection *gc = data;
	struct gaym_conn *gaym = gc->proto_data;
	char hostname[256];
	char *buf;
	const char *username;
	const char* user_bioline; 
	char* bioline;
	
	if(GAIM_CONNECTION_IS_VALID(gc))
	{
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
	gaim_debug_misc("gaym","In login_cb with pw_hash=%s\n",gaym->hash_pw);
	if (gc->account->password && *gc->account->password) {
		
		buf = gaym_format(gaym, "vv", "PASS", gaym->hash_pw);
		if (gaym_send(gaym, buf) < 0) {
			gaim_connection_error(gc, "Error sending password");
			return;
		}
		g_free(buf);
	}

	gethostname(hostname, sizeof(hostname));
	hostname[sizeof(hostname) - 1] = '\0';
	username = gaim_account_get_string(gaym->account, "username", "");
	user_bioline = gaim_account_get_string(gaym->account, "bioline", ""); 
	int bioline_s=sizeof(char)*(strlen(user_bioline)+strlen(gaym->thumbnail)+2);
	bioline=g_malloc(bioline_s);
	g_snprintf(bioline, bioline_s, "%s#%s", gaym->thumbnail,user_bioline); 
	
	buf = gaym_format(gaym, "vn", "NICK", gaim_connection_get_display_name(gc));
	gaim_debug_misc("gaym","Command: %s\n",buf);
	
	if (gaym_send(gaym, buf) < 0) {
		gaim_connection_error(gc, "Error sending nickname");
		return;
	}
	g_free(buf);
	
	buf = gaym_format(gaym, "vvvv:", "USER", gaim_account_get_username(gc->account), hostname, gaym->server,
			      bioline);
	gaim_debug_misc("gaym","Command: %s\n",buf);
	if (gaym_send(gaym, buf) < 0) {
		gaim_connection_error(gc, "Error registering with server");
		return;
	}
	g_free(buf);
	

	gc->inpa = gaim_input_add(gaym->fd, GAIM_INPUT_READ, gaym_input_cb, gc);
	}
}

static void gaym_close(GaimConnection *gc)
{
	struct gaym_conn *gaym = gc->proto_data;

	if (gaym == NULL)
		return;

	gaym_cmd_quit(gaym, "quit", NULL, NULL);

	if (gc->inpa)
		gaim_input_remove(gc->inpa);

	g_free(gaym->inbuf);
	close(gaym->fd);
	if (gaym->timer)
		gaim_timeout_remove(gaym->timer);
	g_hash_table_destroy(gaym->cmds);
	g_hash_table_destroy(gaym->msgs);
	if (gaym->motd)
		g_string_free(gaym->motd, TRUE);
	g_free(gaym->server);
	g_free(gaym);
}

static int gaym_im_send(GaimConnection *gc, const char *who, const char *what, GaimConvImFlags flags)
{
	struct gaym_conn *gaym = gc->proto_data;
	const char *args[2];

	if (strchr(status_chars, *who) != NULL)
		args[0] = who + 1;
	else
		args[0] = who;
	args[1] = what;

	gaym_cmd_privmsg(gaym, "msg", NULL, args);
	return 1;
}

static void gaym_get_info(GaimConnection *gc, const char *who)
{
	struct gaym_conn *gaym = gc->proto_data;
	const char *args[1];
	args[0] = who;
	gaym->info_window_needed=TRUE;
	gaym_cmd_whois(gaym, "whois", NULL, args);
	
	
}

static void gaym_set_away(GaimConnection *gc, const char *state, const char *msg)
{
	struct gaym_conn *gaym = gc->proto_data;
	const char *args[1];

	if (gc->away) {
		g_free(gc->away);
		gc->away = NULL;
	}

	if (msg)
		gc->away = g_strdup(msg);

	args[0] = msg;
	gaym_cmd_away(gaym, "away", NULL, args);
}

static void gaym_add_buddy(GaimConnection *gc, GaimBuddy *buddy, GaimGroup *group)
{
	struct gaym_conn *gaym = (struct gaym_conn *)gc->proto_data;
	struct gaym_buddy *ib = g_new0(struct gaym_buddy, 1);
	ib->name = g_strdup(buddy->name);
	g_hash_table_insert(gaym->buddies, ib->name, ib);
	gaim_debug_misc("gaym","Add buddy: %s\n",buddy->name);
	/* if the timer isn't set, this is during signon, so we don't want to flood
	 * ourself off with ISON's, so we don't, but after that we want to know when
	 * someone's online asap */
	if (gaym->timer)
		gaym_ison_one(gaym, ib);
}

static void gaym_remove_buddy(GaimConnection *gc, GaimBuddy *buddy, GaimGroup *group)
{
	struct gaym_conn *gaym = (struct gaym_conn *)gc->proto_data;
	g_hash_table_remove(gaym->buddies, buddy->name);
}

static void gaym_input_cb(gpointer data, gint source, GaimInputCondition cond)
{
	GaimConnection *gc = data;
	struct gaym_conn *gaym = gc->proto_data;
	char *cur, *end;
	int len;

	if (gaym->inbuflen < gaym->inbufused + IRC_INITIAL_BUFSIZE) {
		gaym->inbuflen += IRC_INITIAL_BUFSIZE;
		gaym->inbuf = g_realloc(gaym->inbuf, gaym->inbuflen);
	}

	if ((len = read(gaym->fd, gaym->inbuf + gaym->inbufused, IRC_INITIAL_BUFSIZE - 1)) < 0) {
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

static void gaym_chat_join (GaimConnection *gc, GHashTable *data)
{
	struct gaym_conn *gaym = gc->proto_data;
	const char *args[2];

	args[0] = g_hash_table_lookup(data, "channel");
	args[1] = g_hash_table_lookup(data, "password");
	gaym_cmd_join(gaym, "join", NULL, args);
}

static char *gaym_get_chat_name(GHashTable *data) {
	return g_strdup(g_hash_table_lookup(data, "channel"));
}

static void gaym_chat_invite(GaimConnection *gc, int id, const char *message, const char *name) 
{
	struct gaym_conn *gaym = gc->proto_data;
	GaimConversation *convo = gaim_find_chat(gc, id);
	const char *args[2];

	if (!convo) {
		gaim_debug(GAIM_DEBUG_ERROR, "gaym", "Got chat invite request for bogus chat\n");
		return;
	}
	args[0] = name;
	args[1] = gaim_conversation_get_name(convo);
	gaym_cmd_invite(gaym, "invite", gaim_conversation_get_name(convo), args);
}


static void gaym_chat_leave (GaimConnection *gc, int id)
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

static int gaym_chat_send(GaimConnection *gc, int id, const char *what)
{
	struct gaym_conn *gaym = gc->proto_data;
	GaimConversation *convo = gaim_find_chat(gc, id);
	const char *args[2];
	char *tmp;

	if (!convo) {
		gaim_debug(GAIM_DEBUG_ERROR, "gaym", "chat send on nonexistent chat\n");
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
	serv_got_chat_in(gc, id, gaim_connection_get_display_name(gc), 0, tmp, time(NULL));
	g_free(tmp);
	return 0;
}

static guint gaym_nick_hash(const char *nick)
{
	char *lc;
	guint bucket;

	lc = g_utf8_strdown(nick, -1);
	bucket = g_str_hash(lc);
	g_free(lc);

	return bucket;
}

static gboolean gaym_nick_equal(const char *nick1, const char *nick2)
{
	return (gaim_utf8_strcasecmp(nick1, nick2) == 0);
}

static void gaym_buddy_free(struct gaym_buddy *ib)
{
	g_free(ib->name);
	g_free(ib);
}

static void gaym_chat_set_topic(GaimConnection *gc, int id, const char *topic)
{
	char *buf;
	const char *name = NULL;
	struct gaym_conn *gaym;

	gaym = gc->proto_data;
	name = gaim_conversation_get_name(gaim_find_chat(gc, id));

	if (name == NULL)
		return;

	buf = gaym_format(gaym, "vt:", "TOPIC", name, topic);
	gaym_send(gaym, buf);
	g_free(buf);
}

static GaimRoomlist *gaym_roomlist_get_list(GaimConnection *gc)
{
	struct gaym_conn *gaym;
	GList *fields = NULL;
	GaimRoomlistField *f;
	char *buf;

	gaym = gc->proto_data;

	if (gaym->roomlist)
		gaim_roomlist_unref(gaym->roomlist);

	gaym->roomlist = gaim_roomlist_new(gaim_connection_get_account(gc));

	f = gaim_roomlist_field_new(GAIM_ROOMLIST_FIELD_STRING, "", "channel", TRUE);
	fields = g_list_append(fields, f);

	f = gaim_roomlist_field_new(GAIM_ROOMLIST_FIELD_INT, _("Users"), "users", FALSE);
	fields = g_list_append(fields, f);

	f = gaim_roomlist_field_new(GAIM_ROOMLIST_FIELD_STRING, _("Topic"), "topic", FALSE);
	fields = g_list_append(fields, f);

	gaim_roomlist_set_fields(gaym->roomlist, fields);

	buf = gaym_format(gaym, "v", "LIST");
	gaym_send(gaym, buf);
	g_free(buf);

	return gaym->roomlist;
}

static void gaym_roomlist_cancel(GaimRoomlist *list)
{
	GaimConnection *gc = gaim_account_get_connection(list->account);
	struct gaym_conn *gaym;

	if (gc == NULL)
		return;

	gaym = gc->proto_data;

	gaim_roomlist_set_in_progress(list, FALSE);

	if (gaym->roomlist == list) {
		gaym->roomlist = NULL;
		gaim_roomlist_unref(list);
	}
}

static GaimPluginProtocolInfo prpl_info =
{
	OPT_PROTO_CHAT_TOPIC | OPT_PROTO_PASSWORD_OPTIONAL,
	NULL,					/* user_splits */
	NULL,					/* protocol_options */
	{"jpg",55,75,55,75},			/* icon_spec */
	gaym_blist_icon,			/* list_icon */
	gaym_blist_emblems,		/* list_emblems */
	NULL,					/* status_text */
	NULL,					/* tooltip_text */
	gaym_away_states,		/* away_states */
	NULL,					/* blist_node_menu */
	gaym_chat_join_info,		/* chat_info */
	gaym_chat_info_defaults,	/* chat_info_defaults */
	gaym_login,				/* login */
	gaym_close,				/* close */
	gaym_im_send,			/* send_im */
	NULL,					/* set_info */
	NULL,					/* send_typing */
	gaym_get_info,			/* get_info */
	gaym_set_away,			/* set_away */
	NULL,					/* set_idle */
	NULL,					/* change_passwd */
	gaym_add_buddy,			/* add_buddy */
	NULL,					/* add_buddies */
	gaym_remove_buddy,		/* remove_buddy */
	NULL,					/* remove_buddies */
	NULL,					/* add_permit */
	NULL,					/* add_deny */
	NULL,					/* rem_permit */
	NULL,					/* rem_deny */
	NULL,					/* set_permit_deny */
	NULL,					/* warn */
	gaym_chat_join,			/* join_chat */
	NULL,					/* reject_chat */
	gaym_get_chat_name,		/* get_chat_name */
	gaym_chat_invite,		/* chat_invite */
	gaym_chat_leave,			/* chat_leave */
	NULL,					/* chat_whisper */
	gaym_chat_send,			/* chat_send */
	NULL,					/* keepalive */
	NULL,					/* register_user */
	NULL,					/* get_cb_info */
	NULL,					/* get_cb_away */
	NULL,					/* alias_buddy */
	NULL,					/* group_buddy */
	NULL,					/* rename_group */
	NULL,					/* buddy_free */
	NULL,					/* convo_closed */
	NULL,					/* normalize */
	NULL,					/* set_buddy_icon */
	NULL,					/* remove_group */
	NULL,					/* get_cb_real_name */
	gaym_chat_set_topic,		/* set_chat_topic */
	NULL,					/* find_blist_chat */
	gaym_roomlist_get_list,	/* roomlist_get_list */
	gaym_roomlist_cancel,	/* roomlist_cancel */
	NULL,					/* roomlist_expand_category */
	NULL,					/* can_receive_file */
	gaym_dccsend_send_file	/* send_file */
};
static
void gaym_get_photo_info(GaimConversation* conv) {
	char *buf;
	
	gaim_debug_misc("gaym","Got conversation-created signal\n");
        if(strncmp(conv->account->protocol_id,"prpl-gaym",9) == 0 && gaim_conversation_get_type(conv) == GAIM_CONV_IM) {

	struct gaym_conn *gaym;
	
	GaimConnection *gc = gaim_conversation_get_gc(conv);
	gaym = (struct gaym_conn*) gc->proto_data;

	gaym->info_window_needed=FALSE;
        buf = gaym_format(gaym, "vn", "WHOIS", conv->name);
        gaim_debug_misc("gaym","Conversation triggered command: %s\n",buf);
	gaym_send(gaym, buf);
	g_free(buf);
        gaym->whois.nick = g_strdup(conv->name);
        }
	
}

static GaimPluginPrefFrame *
    get_plugin_pref_frame(GaimPlugin *plugin)
{
  GaimPluginPrefFrame *frame;
  GaimPluginPref *ppref;

  frame = gaim_plugin_pref_frame_new();

  ppref = gaim_plugin_pref_new_with_label(_("Conversations"));
  gaim_plugin_pref_frame_add(frame, ppref);

  ppref = gaim_plugin_pref_new_with_name_and_label(
      "/plugins/prpl/gaym/show_bio_with_join",
  _("Show bioline for users joining the room."));
  gaim_plugin_pref_frame_add(frame, ppref);

  return frame;
}

static GaimPluginUiInfo prefs_info = {
  get_plugin_pref_frame
};

static GaimPluginInfo info =
{
	GAIM_PLUGIN_MAGIC,
	GAIM_MAJOR_VERSION,
	GAIM_MINOR_VERSION,
	GAIM_PLUGIN_PROTOCOL,                             /**< type           */
	NULL,                                             /**< ui_requirement */
	0,                                                /**< flags          */
	NULL,                                             /**< dependencies   */
	GAIM_PRIORITY_DEFAULT,                            /**< priority       */

	"prpl-gaym",                                      /**< id             */
	"GAYM",                                           /**< name           */
	VERSION,                                          /**< version        */
	N_("GAYM Protocol Plugin"),                       /**  summary        */
	N_("Gay.com Protocol based on IRC"),              /**  description    */
	NULL,                                             /**< author         */
	"http://gaym.sourceforge.org",      		  /**< homepage      */

	NULL,                                             /**< load           */
	NULL,                                             /**< unload         */
	NULL,                                             /**< destroy        */

	NULL,                                             /**< ui_info        */
	&prpl_info,                                       /**< extra_info     */
	NULL,
	gaym_actions
};



static void _init_plugin(GaimPlugin *plugin)
{
	
	GaimAccountOption *option;

	option = gaim_account_option_string_new(_("Server"), "server", IRC_DEFAULT_SERVER);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);

	option = gaim_account_option_int_new(_("Port"), "port", IRC_DEFAULT_PORT);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);

	
	option = gaim_account_option_string_new(_("Bio Line"), "bioline", "");
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);

        //We have to pull thumbnails, since they aren't pushed like with other protocols.
      gaim_signal_connect(gaim_conversations_get_handle(),
                            "conversation-created",
                            plugin, GAIM_CALLBACK(gaym_get_photo_info), NULL);
      gaim_prefs_add_bool("/plugins/prpl/msn/show_bio_with_join",   TRUE);

        
	_gaym_plugin = plugin;

	gaym_register_commands();
}

GAIM_INIT_PLUGIN(gaym, _init_plugin, info);
