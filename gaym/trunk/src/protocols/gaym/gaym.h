/**
 * @file gaym.h
 * 
 * gaim
 *
 * Copyright (C) 2003, Ethan Blanton <eblanton@cs.purdue.edu>
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

#ifndef _GAIM_GAYM_H
#define _GAIM_GAYM_H

#include <glib.h>

#include "roomlist.h"

#define IRC_DEFAULT_SERVER "www.gay.com"
#define IRC_DEFAULT_PORT 7514

#define IRC_DEFAULT_CHARSET "UTF-8"
#define IRC_DEFAULT_ALIAS "gaim"

#define IRC_INITIAL_BUFSIZE 1024

#define CHUNK_SIZE 500

#define BLIST_UPDATE_PERIOD 45000 //buddy list updated every 45s
#define BLIST_CHUNK_INTERVAL 5000 //5s between ISON chunks


enum { IRC_USEROPT_SERVER, IRC_USEROPT_PORT, IRC_USEROPT_CHARSET };
enum gaym_state { IRC_STATE_NEW, IRC_STATE_ESTABLISHED };

struct gaym_conn {
	GaimAccount *account;
	GHashTable *msgs;
	GHashTable *cmds;
	char *server;
	int fd;
	guint timer;
	GHashTable *buddies;

	char *inbuf;
	int inbuflen;
	int inbufused;

	char* thumbnail;
	char* hash_pw;
	char* server_bioline;
	
	gboolean blist_updating;
	gboolean info_window_needed;
	
	GString *motd;
	GString *names;
	char *nameconv;
	struct _whois {
		char *nick;
		char *away;
		char *userhost;
		char *name;
		char *server;
		char *serverinfo;
		char *channels;
		int gaymop;
		int identified;
		int idle;
		time_t signon;
	} whois;
	GaimRoomlist *roomlist;

	gboolean quitting;
};

struct gaym_buddy {
	char *name;
	gboolean online;
	gboolean flag;  //Marks an ISON response.
	
	gboolean stale; //Signifies ISON update needed
	gboolean done; //Keep track of which buddies have been checked.
	
};

typedef struct {

	char *cookies;
	void (*session_cb)(GaimAccount*);
	GaimAccount* account;
	char *username;
	char *password;
	gboolean hasFormData;

} GaimUrlSession;


typedef int (*IRCCmdCallback) (struct gaym_conn *gaym, const char *cmd, const char *target, const char **args);

int gaym_send(struct gaym_conn *gaym, const char *buf);
gboolean gaym_blist_timeout(struct gaym_conn *gaym);

char *gaym_mgaym2html(const char *string);
char *gaym_mgaym2txt(const char *string);

void gaym_register_commands(void);
void gaym_msg_table_build(struct gaym_conn *gaym);
void gaym_parse_msg(struct gaym_conn *gaym, char *input);
char *gaym_parse_ctcp(struct gaym_conn *gaym, const char *from, const char *to, const char *msg, int notice);
char *gaym_format(struct gaym_conn *gaym, const char *format, ...);

void gaym_msg_default(struct gaym_conn *gaym, const char *name, const char *from, char **args);
void gaym_msg_away(struct gaym_conn *gaym, const char *name, const char *from, char **args);
void gaym_msg_badmode(struct gaym_conn *gaym, const char *name, const char *from, char **args);
void gaym_msg_banned(struct gaym_conn *gaym, const char *name, const char *from, char **args);
void gaym_msg_chanmode(struct gaym_conn *gaym, const char *name, const char *from, char **args);
void gaym_msg_endwhois(struct gaym_conn *gaym, const char *name, const char *from, char **args);
void gaym_msg_endmotd(struct gaym_conn *gaym, const char *name, const char *from, char **args);
void gaym_msg_invite(struct gaym_conn *gaym, const char *name, const char *from, char **args);
void gaym_msg_inviteonly(struct gaym_conn *gaym, const char *name, const char *from, char **args);
void gaym_msg_ison(struct gaym_conn *gaym, const char *name, const char *from, char **args);
void gaym_msg_join(struct gaym_conn *gaym, const char *name, const char *from, char **args);
void gaym_msg_kick(struct gaym_conn *gaym, const char *name, const char *from, char **args);
void gaym_msg_list(struct gaym_conn *gaym, const char *name, const char *from, char **args);
void gaym_msg_mode(struct gaym_conn *gaym, const char *name, const char *from, char **args);
void gaym_msg_motd(struct gaym_conn *gaym, const char *name, const char *from, char **args);
void gaym_msg_names(struct gaym_conn *gaym, const char *name, const char *from, char **args);
void gaym_msg_nick(struct gaym_conn *gaym, const char *name, const char *from, char **args);
void gaym_msg_nickused(struct gaym_conn *gaym, const char *name, const char *from, char **args);
void gaym_msg_nochan(struct gaym_conn *gaym, const char *name, const char *from, char **args);
void gaym_msg_nonick(struct gaym_conn *gaym, const char *name, const char *from, char **args);
void gaym_msg_no_such_nick(struct gaym_conn *gaym, const char *name, const char *from, char **args);
void gaym_msg_nochangenick(struct gaym_conn *gaym, const char *name, const char *from, char **args);
void gaym_msg_nosend(struct gaym_conn *gaym, const char *name, const char *from, char **args);
void gaym_msg_notice(struct gaym_conn *gaym, const char *name, const char *from, char **args);
void gaym_msg_notinchan(struct gaym_conn *gaym, const char *name, const char *from, char **args);
void gaym_msg_notop(struct gaym_conn *gaym, const char *name, const char *from, char **args);
void gaym_msg_part(struct gaym_conn *gaym, const char *name, const char *from, char **args);
void gaym_msg_ping(struct gaym_conn *gaym, const char *name, const char *from, char **args);
void gaym_msg_pong(struct gaym_conn *gaym, const char *name, const char *from, char **args);
void gaym_msg_privmsg(struct gaym_conn *gaym, const char *name, const char *from, char **args);
void gaym_msg_regonly(struct gaym_conn *gaym, const char *name, const char *from, char **args);
void gaym_msg_quit(struct gaym_conn *gaym, const char *name, const char *from, char **args);
void gaym_msg_topic(struct gaym_conn *gaym, const char *name, const char *from, char **args);
void gaym_msg_unknown(struct gaym_conn *gaym, const char *name, const char *from, char **args);
void gaym_msg_wallops(struct gaym_conn *gaym, const char *name, const char *from, char **args);
void gaym_msg_whois(struct gaym_conn *gaym, const char *name, const char *from, char **args);

void gaym_msg_ignore(struct gaym_conn *gaym, const char *name, const char *from, char **args);


/* GAYMDIFF: This section has additional gay.com messages */
void gaym_msg_richnames_list(struct gaym_conn *gaym, const char *name, const char *from, char **args);
void gaym_msg_pay_channel(struct gaym_conn *gaym, const char *name, const char *from, char **args);
void gaym_msg_toomany_channels(struct gaym_conn *gaym, const char *name, const char * from, char **args);


void gaym_cmd_table_build(struct gaym_conn *gaym);

int gaym_cmd_default(struct gaym_conn *gaym, const char *cmd, const char *target, const char **args);
int gaym_cmd_away(struct gaym_conn *gaym, const char *cmd, const char *target, const char **args);
int gaym_cmd_ctcp_action(struct gaym_conn *gaym, const char *cmd, const char *target, const char **args);
int gaym_cmd_invite(struct gaym_conn *gaym, const char *cmd, const char *target, const char **args);
int gaym_cmd_join(struct gaym_conn *gaym, const char *cmd, const char *target, const char **args);
int gaym_cmd_kick(struct gaym_conn *gaym, const char *cmd, const char *target, const char **args);
int gaym_cmd_list(struct gaym_conn *gaym, const char *cmd, const char *target, const char **args);
int gaym_cmd_mode(struct gaym_conn *gaym, const char *cmd, const char *target, const char **args);
int gaym_cmd_names(struct gaym_conn *gaym, const char *cmd, const char *target, const char **args);
int gaym_cmd_nick(struct gaym_conn *gaym, const char *cmd, const char *target, const char **args);
int gaym_cmd_op(struct gaym_conn *gaym, const char *cmd, const char *target, const char **args);
int gaym_cmd_privmsg(struct gaym_conn *gaym, const char *cmd, const char *target, const char **args);
int gaym_cmd_part(struct gaym_conn *gaym, const char *cmd, const char *target, const char **args);
int gaym_cmd_ping(struct gaym_conn *gaym, const char *cmd, const char *target, const char **args);
int gaym_cmd_quit(struct gaym_conn *gaym, const char *cmd, const char *target, const char **args);
int gaym_cmd_quote(struct gaym_conn *gaym, const char *cmd, const char *target, const char **args);
int gaym_cmd_query(struct gaym_conn *gaym, const char *cmd, const char *target, const char **args);
int gaym_cmd_remove(struct gaym_conn *gaym, const char *cmd, const char *target, const char **args);
int gaym_cmd_topic(struct gaym_conn *gaym, const char *cmd, const char *target, const char **args);
int gaym_cmd_wallops(struct gaym_conn *gaym, const char *cmd, const char *target, const char **args);
int gaym_cmd_whois(struct gaym_conn *gaym, const char *cmd, const char *target, const char **args);

void gaym_dccsend_send_file(GaimConnection *gc, const char *who, const char *file);
void gaym_dccsend_recv(struct gaym_conn *gaym, const char *from, const char *msg);
void gaym_get_hash_from_weblogin(GaimAccount* account, void(*callback)(GaimAccount*));


#endif /* _GAIM_GAYM_H */
