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

#include "internal.h"

#include <glib.h>

#include "roomlist.h"

#define IRC_DEFAULT_SERVER "www.gay.com"
#define IRC_DEFAULT_PORT 7514

#define IRC_DEFAULT_CHARSET "UTF-8"
#define IRC_DEFAULT_ALIAS "gaim"

#define IRC_INITIAL_BUFSIZE 1024

#define BLIST_UPDATE_PERIOD 60000       /* buddy list updated every 45s */
#define BLIST_CHUNK_INTERVAL 5000       /* 5s between ISON chunks */

#define MAX_BIO_LEN 150         /* max number of characters in bio */

#define MAX_CHANNEL_MEMBERS 200

#define GAYBOI_SPAM_URL "http://gayboi.org/spam/spamlst.php"

typedef struct _BListWhois BListWhois;
struct _BListWhois {
    int count;
    GString *string;
};

enum { IRC_USEROPT_SERVER, IRC_USEROPT_PORT, IRC_USEROPT_CHARSET };
enum gaym_state { IRC_STATE_NEW, IRC_STATE_ESTABLISHED };
enum info_string { INFO_AGE, INFO_LOCATION, INFO_BIO, INFO_URL };

struct gaym_conn {
    GaimAccount *account;
    GHashTable *msgs;
    GHashTable *cmds;
    char *server;
    int fd;
    guint timer;
    GHashTable *buddies;        /* hash table of struct gaym_buddy */
    GHashTable *channel_members;        /* hash table of struct gaym_buddy 
                                         */

    char *inbuf;
    int inbuflen;
    int inbufused;

    char *thumbnail;
    char *chat_key;
    char *server_bioline;
    char *server_stats;
    char *roomlist_filter;
    char *bio;

    gboolean blist_updating;
    GHashTable *info_window_needed;

    GString *motd;
    GString *names;
    char *nameconv;
    char *traceconv;

    GaimRoomlist *roomlist;

    GList **node_menu;
    gboolean quitting;
    char *subroom;
    GHashTable *confighash;
    GHashTable *entry_order;

    GHashTable *hammers;

    // Namelists come in order
    // So use a queue.
    GQueue *namelists;

};

typedef struct {

    gchar *cookies;
    GHashTable *cookie_table;
    void (*session_cb) (GaimAccount *);
    GaimAccount *account;
    char *username;
    char *password;
    struct gaym_conn *gaym;
    gboolean hasFormData;

} GaimUrlSession;
typedef struct gaym_buddy GaymBuddy;
struct gaym_buddy {
    gchar *name;                 /* gaym formatted nick */
    gboolean done;              /* has been checked */
    gboolean online;            /* is online */
    gint ref_count;             /* reference count for mem mngmnt */
    gchar *bio;                  /* bio string */
    gchar *thumbnail;            /* thumbnail string */
    gchar *sex;                  /* sex string */
    gchar *age;                  /* age string */
    gchar *prefix;               /* prefix string */
    gchar *location;             /* location string */
    gchar *room;		/* Which subroom, if this is a namelist entry*/
    gboolean gaymuser;          /* gaym detected */
};
GaymBuddy *gaym_get_channel_member_info(struct gaym_conn *gaym,
                                        const gchar * name);

GaymBuddy *gaym_get_channel_member_reference(struct gaym_conn
                                             *gaym, const char *name);

gboolean gaym_unreference_channel_member(struct gaym_conn *gaym,
                                         gchar * name);

struct hammer_cb_data {
    struct gaym_conn *gaym;
    char *room;
    void *cancel_dialog;
} hammer_cb_data;

void hammer_cb_data_destroy(struct hammer_cb_data *hdata);

typedef int (*IRCCmdCallback) (struct gaym_conn * gaym, const char *cmd,
                               const char *target, const char **args);

int gaym_send(struct gaym_conn *gaym, const char *buf);
gboolean gaym_blist_timeout(struct gaym_conn *gaym);

char *gaym_mgaym2html(const char *string);
char *gaym_mgaym2txt(const char *string);

void gaym_register_commands(void);
void gaym_msg_table_build(struct gaym_conn *gaym);
void gaym_parse_msg(struct gaym_conn *gaym, char *input);
char *gaym_parse_ctcp(struct gaym_conn *gaym, const char *from,
                      const char *to, const char *msg, int notice);
char *gaym_format(struct gaym_conn *gaym, const char *format, ...);

typedef void (msg_handler) (struct gaym_conn * gaym, const char *name,
                            const char *from, char **args);
msg_handler gaym_msg_away;
msg_handler gaym_msg_default;
msg_handler gaym_msg_away;
msg_handler gaym_msg_badmode;
msg_handler gaym_msg_banned;
msg_handler gaym_msg_chanmode;
msg_handler gaym_msg_endwhois;
msg_handler gaym_msg_endmotd;
msg_handler gaym_msg_invite;
msg_handler gaym_msg_inviteonly;
msg_handler gaym_msg_who;
msg_handler gaym_msg_chanfull;
msg_handler gaym_msg_join;
msg_handler gaym_msg_kick;
msg_handler gaym_msg_list;
msg_handler gaym_msg_login_failed;
msg_handler gaym_msg_mode;
msg_handler gaym_msg_motd;
msg_handler gaym_msg_names;
msg_handler gaym_msg_nick;
msg_handler gaym_msg_nickused;
msg_handler gaym_msg_nochan;
msg_handler gaym_msg_nonick_chan;
msg_handler gaym_msg_nonick;
msg_handler gaym_msg_no_such_nick;
msg_handler gaym_msg_nochangenick;
msg_handler gaym_msg_nosend;
msg_handler gaym_msg_notice;
msg_handler gaym_msg_notinchan;
msg_handler gaym_msg_notop;
msg_handler gaym_msg_part;
msg_handler gaym_msg_ping;
msg_handler gaym_msg_pong;
msg_handler gaym_msg_privmsg;
msg_handler gaym_msg_regonly;
msg_handler gaym_msg_quit;
msg_handler gaym_msg_topic;
msg_handler gaym_msg_trace;
msg_handler gaym_msg_unknown;
msg_handler gaym_msg_wallops;
msg_handler gaym_msg_whois;
msg_handler gaym_msg_richnames_list;
msg_handler gaym_msg_create_pay_only;
msg_handler gaym_msg_pay_channel;
msg_handler gaym_msg_toomany_channels;
msg_handler gaym_msg_list_busy;


void gaym_cmd_table_build(struct gaym_conn *gaym);

typedef int (cmd_handler) (struct gaym_conn * gaym, const char *cmd,
                           const char *target, const char **args);

cmd_handler gaym_cmd_default;
cmd_handler gaym_cmd_away;
cmd_handler gaym_cmd_ctcp_action;
cmd_handler gaym_cmd_invite;
cmd_handler gaym_cmd_join;
cmd_handler gaym_cmd_kick;
cmd_handler gaym_cmd_list;
cmd_handler gaym_cmd_mode;
cmd_handler gaym_cmd_names;
cmd_handler gaym_cmd_nick;
cmd_handler gaym_cmd_op;
cmd_handler gaym_cmd_privmsg;
cmd_handler gaym_cmd_part;
cmd_handler gaym_cmd_ping;
cmd_handler gaym_cmd_quit;
cmd_handler gaym_cmd_quote;
cmd_handler gaym_cmd_query;
cmd_handler gaym_cmd_remove;
cmd_handler gaym_cmd_topic;
cmd_handler gaym_cmd_trace;
cmd_handler gaym_cmd_wallops;
cmd_handler gaym_cmd_whois;
cmd_handler gaym_cmd_who;

typedef struct GaymNamelist {
    char *roomname;
    GSList *members;            // List of GaymBuddies;
    int num_rooms;
    gboolean multi_room;
    GSList *current;            // Pointer to gaymbuddy to be updated next 
                                // 
    // (during names pass)
} GaymNamelist;
void gaym_dccsend_send_file(GaimConnection * gc, const char *who,
                            const char *file);
void gaym_dccsend_recv(struct gaym_conn *gaym, const char *from,
                       const char *msg);
void gaym_get_chat_key_from_weblogin(GaimAccount * account,
                                     void (*callback) (GaimAccount *));

void gaym_get_room_namelist(GaimAccount * account, const char *room);
void gaim_session_fetch(const char *url, gboolean full,
                        const char *user_agent, gboolean http11,
                        void (*cb) (gpointer, const char *, size_t),
                        void *user_data, GaimUrlSession * session);

#endif                          /* _GAIM_GAYM_H */

/**
 * vim:tabstop=4:shiftwidth=4:expandtab:
 */
