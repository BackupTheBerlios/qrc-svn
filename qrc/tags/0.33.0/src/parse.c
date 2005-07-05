/**
 * @file parse.c
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

#include "internal.h"

#include "accountopt.h"
#include "conversation.h"
#include "notify.h"
#include "debug.h"
#include "cmds.h"
#include "gaym.h"
#include "helpers.h"

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

static char *gaym_send_convert(struct gaym_conn *gaym, const char *string);
static char *gaym_recv_convert(struct gaym_conn *gaym, const char *string);

static void gaym_parse_error_cb(struct gaym_conn *gaym, char *input);

/* typedef void (*IRCMsgCallback)(struct gaym_conn *gaym, char *from, char 
 *name, char **args);*/
static struct _gaym_msg {
    char *name;
    char *format;
    void (*cb) (struct gaym_conn * gaym, const char *name,
                const char *from, char **args);
} _gaym_msgs[] = {
    {
    "001", "n:", gaym_msg_endmotd},     /* login ok */
    {
    "263", "n:", gaym_msg_list_busy},   /* Server load to heavy */
    {
    "301", "nn:", gaym_msg_away},       /* User is away */
    {
    "303", "n:", gaym_msg_ison},        /* ISON reply */
    {
    "311", "nnvvv:", gaym_msg_whois},   /* Whois user */
    {
    "312", "nnv:", gaym_msg_whois},     /* Whois server */
    {
    "313", "nn:", gaym_msg_whois},      /* Whois gaymop */
    {
    "315", "nc:", gaym_msg_who},        /* End of WHO list */
    {
    "317", "nnvv", gaym_msg_whois},     /* Whois idle */
    {
    "321", "*", gaym_msg_list}, /* Start of list */
    {
    "322", "ncv:", gaym_msg_list},      /* List */
    {
    "323", ":", gaym_msg_list}, /* End of list */
    {
    "352", "nc:", gaym_msg_who},        /* WHO list */
    {
    "353", "nvc:", gaym_msg_names},     /* Names list */
    {
    "366", "nc:", gaym_msg_names},      /* End of names */
    {
    "376", "n:", gaym_msg_endmotd},     /* End of MOTD */
    {
    "401", "nc:", gaym_msg_nonick_chan},        /* No such channel */
    {
    "403", "nc:", gaym_msg_nochan},     /* No such channel */
    {
    "404", "nt:", gaym_msg_nosend},     /* Cannot send to chan */
    {
    "421", "nv:", gaym_msg_unknown},    /* Unknown command */
    {
    "422", "nv:", gaym_msg_endmotd},    /* No MOTD available */
    {
    "442", "nc:", gaym_msg_notinchan},  /* Not in channel */
    {
    "471", "nc:", gaym_msg_chanfull},   /* Channel Full */
    {
    "690", "ncnt:", gaym_msg_richnames_list},   /* Gay.com's RPL for names 
                                                   list */
    {
    "695", "nc:", gaym_msg_toomany_channels},   /* Too many channels (2)
                                                   maximum joined */
    {
    "696", "nc:", gaym_msg_pay_channel},        /* User tried to enter pay 
                                                   channel, rejected */
    {
    "698", "nc:", gaym_msg_create_pay_only},    /* Creating a new room
                                                   is a feature for paying
                                                   member */
    {
    "701", "nt:", gaym_msg_no_such_nick},       /* Tried to get whois for
                                                   someone not on */
    {
    "invite", "n:", gaym_msg_invite},   /* Invited */
    {
    "join", "c:", gaym_msg_join},       /* Joined a channel */
    {
    "nick", ":", gaym_msg_nick},        /* Nick change */
    {
    "notice", "t:", gaym_msg_notice},   /* NOTICE recv */
    {
    "part", "::", gaym_msg_part},       /* Parted a channel */
    {
    "ping", ":", gaym_msg_ping},        /* Received PING from server */
    {
    "pong", "v:", gaym_msg_pong},       /* Received PONG from server */
    {
    "privmsg", "t:", gaym_msg_privmsg}, /* Received private message */
    {
    "quit", ":", gaym_msg_quit},        /* QUIT notice */
    {
    NULL, NULL, NULL}
};

static struct _gaym_user_cmd {
    char *name;
    char *format;
    IRCCmdCallback cb;
    char *help;
} _gaym_cmds[] = {
    {
    "action", ":", gaym_cmd_ctcp_action,
            N_("action &lt;action to perform&gt;:  Perform an action.")}, {
    "away", ":", gaym_cmd_away,
            N_
            ("away [message]:  Set an away message, or use no message to return from being away.")},
    {
    "j", "cv", gaym_cmd_join,
            N_
            ("j &lt;room1&gt;[,room2][,...] [key1[,key2][,...]]:  Enter one or more channels, optionally providing a channel key for each if needed.")},
    {
    "join", "cv", gaym_cmd_join,
            N_
            ("join &lt;room1&gt;[,room2][,...] [key1[,key2][,...]]:  Enter one or more channels, optionally providing a channel key for each if needed.")},
    {
    "me", ":", gaym_cmd_ctcp_action,
            N_("me &lt;action to perform&gt;:  Perform an action.")}, {
    "msg", "t:", gaym_cmd_privmsg,
            N_
            ("msg &lt;nick&gt; &lt;message&gt;:  Send a private message to a user (as opposed to a channel).")},
    {
    "list", "*", gaym_cmd_list,
            N_
            ("list [filter]: List all channels, or if filter is specified, list channels containing filter text.")},
    {
    "names", "c", gaym_cmd_names,
            N_
            ("names [channel]:  List the users currently in a channel.")},
    {
    "nick", "n", gaym_cmd_nick,
            N_("nick &lt;new nickname&gt;:  Change your nickname.")}, {
    "part", "c:", gaym_cmd_part,
            N_
            ("part [room] [message]:  Leave the current channel, or a specified channel, with an optional message.")},
    {
    "ping", "n", gaym_cmd_ping,
            N_
            ("ping [nick]:  Asks how much lag a user (or the server if no user specified) has.")},
    {
    "query", "n:", gaym_cmd_query,
            N_
            ("query &lt;nick&gt; &lt;message&gt;:  Send a private message to a user (as opposed to a channel).")},
    {
    "quit", ":", gaym_cmd_quit,
            N_
            ("quit [message]:  Disconnect from the server, with an optional message.")},
    {
    "quote", "*", gaym_cmd_quote,
            N_("quote [...]:  Send a raw command to the server.")}, {
    "umode", ":", gaym_cmd_mode,
            N_
            ("umode &lt;+|-&gt;&lt;A-Za-z&gt;:  Set or unset a user mode.")},
    {
    "whois", "n", gaym_cmd_whois,
            N_("whois &lt;nick&gt;:  Get information on a user.")}, {
    NULL, NULL, NULL}
};

static GaimCmdRet gaym_parse_gaim_cmd(GaimConversation * conv,
                                      const gchar * cmd, gchar ** args,
                                      gchar ** error, void *data)
{
    GaimConnection *gc;
    struct gaym_conn *gaym;
    struct _gaym_user_cmd *cmdent;

    gc = gaim_conversation_get_gc(conv);
    if (!gc)
        return GAIM_CMD_RET_FAILED;

    gaym = gc->proto_data;

    if ((cmdent = g_hash_table_lookup(gaym->cmds, cmd)) == NULL)
        return GAIM_CMD_RET_FAILED;

    (cmdent->cb) (gaym, cmd, gaim_conversation_get_name(conv),
                  (const char **) args);

    return GAIM_CMD_RET_OK;
}

static void gaym_register_command(struct _gaym_user_cmd *c)
{
    GaimCmdFlag f;
    char args[10];
    char *format;
    int i;

    f = GAIM_CMD_FLAG_CHAT | GAIM_CMD_FLAG_IM | GAIM_CMD_FLAG_PRPL_ONLY
        | GAIM_CMD_FLAG_ALLOW_WRONG_ARGS;

    format = c->format;

    for (i = 0; (i < (sizeof(args) - 1)) && *format; i++, format++)
        switch (*format) {
        case 'v':
        case 'n':
        case 'c':
        case 't':
            args[i] = 'w';
            break;
        case ':':
        case '*':
            args[i] = 's';
            break;
        }

    args[i] = '\0';

    gaim_cmd_register(c->name, args, GAIM_CMD_P_PRPL, f, "prpl-gaym",
                      gaym_parse_gaim_cmd, _(c->help), NULL);
}

void gaym_register_commands(void)
{
    struct _gaym_user_cmd *c;

    for (c = _gaym_cmds; c && c->name; c++)
        gaym_register_command(c);
}

static char *gaym_send_convert(struct gaym_conn *gaym, const char *string)
{
    char *utf8;
    GError *err = NULL;
    const gchar *charset;

    charset =
        gaim_account_get_string(gaym->account, "encoding",
                                IRC_DEFAULT_CHARSET);
    if (!strcasecmp("UTF-8", charset))
        return g_strdup(string);

    utf8 =
        g_convert(string, strlen(string), charset, "UTF-8", NULL, NULL,
                  &err);
    if (err) {
        gaim_debug(GAIM_DEBUG_ERROR, "gaym", "Send conversion error: %s\n",
                   err->message);
        gaim_debug(GAIM_DEBUG_ERROR, "gaym",
                   "Sending as UTF-8 instead of %s\n", charset);
        utf8 = g_strdup(string);
        g_error_free(err);
    }

    return utf8;
}

static char *gaym_recv_convert(struct gaym_conn *gaym, const char *string)
{
    char *utf8 = NULL;
    GError *err = NULL;
    const gchar *charset;

    charset =
        gaim_account_get_string(gaym->account, "encoding",
                                IRC_DEFAULT_CHARSET);

    if (!strcasecmp("UTF-8", charset)) {
        if (g_utf8_validate(string, strlen(string), NULL))
            utf8 = g_strdup(string);
    } else {
        utf8 =
            g_convert(string, strlen(string), "UTF-8", charset, NULL, NULL,
                      &err);
    }

    if (err) {
        gaim_debug(GAIM_DEBUG_ERROR, "gaym", "recv conversion error: %s\n",
                   err->message);
        g_error_free(err);
    }

    if (utf8 == NULL)
        utf8 =
            g_strdup(_
                     ("(There was an error converting this message.  Check the 'Encoding' option in the Account Editor)"));

    return utf8;
}


char *gaym_parse_ctcp(struct gaym_conn *gaym, const char *from,
                      const char *to, const char *msg, int notice)
{
    GaimConnection *gc;
    const char *cur = msg + 1;
    char *buf, *ctcp;
    time_t timestamp;

    /* Note that this is NOT correct w.r.t. multiple CTCPs in one message
       and low-level quoting ... but if you want that crap, use a real IRC 
       client. */

    if (msg[0] != '\001' || msg[strlen(msg) - 1] != '\001')
        return g_strdup(msg);

    if (!strncmp(cur, "ACTION ", 7)) {
        cur += 7;
        buf = g_strdup_printf("/me %s", cur);
        buf[strlen(buf) - 1] = '\0';
        return buf;
    } else if (!strncmp(cur, "PING ", 5)) {
        if (notice) {           /* reply */
            sscanf(cur, "PING %lu", &timestamp);
            gc = gaim_account_get_connection(gaym->account);
            if (!gc)
                return NULL;
            buf =
                g_strdup_printf(_("Reply time from %s: %lu seconds"), from,
                                time(NULL) - timestamp);
            gaim_notify_info(gc, _("PONG"), _("CTCP PING reply"), buf);
            g_free(buf);
            return NULL;
        } else {
            buf = gaym_format(gaym, "vt:", "NOTICE", from, msg);
            gaym_send(gaym, buf);
            g_free(buf);
            gc = gaim_account_get_connection(gaym->account);
        }
    } else if (!strncmp(cur, "VERSION", 7) && !notice) {
        buf =
            gaym_format(gaym, "vt:", "NOTICE", from,
                        "\001VERSION Gaim IRC\001");
        gaym_send(gaym, buf);
        g_free(buf);
    } else if (!strncmp(cur, "DCC SEND ", 9)) {
        gaym_dccsend_recv(gaym, from, msg + 10);
        return NULL;
    }

    ctcp = g_strdup(msg + 1);
    ctcp[strlen(ctcp) - 1] = '\0';
    buf =
        g_strdup_printf("Received CTCP '%s' (to %s) from %s", ctcp, to,
                        from);
    g_free(ctcp);
    return buf;
}

void gaym_msg_table_build(struct gaym_conn *gaym)
{
    int i;

    if (!gaym || !gaym->msgs) {
        gaim_debug(GAIM_DEBUG_ERROR, "gaym",
                   "Attempt to build a message table on a bogus structure\n");
        return;
    }

    for (i = 0; _gaym_msgs[i].name; i++) {
        g_hash_table_insert(gaym->msgs, (gpointer) _gaym_msgs[i].name,
                            (gpointer) & _gaym_msgs[i]);
    }
}

void gaym_cmd_table_build(struct gaym_conn *gaym)
{
    int i;

    if (!gaym || !gaym->cmds) {
        gaim_debug(GAIM_DEBUG_ERROR, "gaym",
                   "Attempt to build a command table on a bogus structure\n");
        return;
    }

    for (i = 0; _gaym_cmds[i].name; i++) {
        g_hash_table_insert(gaym->cmds, (gpointer) _gaym_cmds[i].name,
                            (gpointer) & _gaym_cmds[i]);
    }
}

char *gaym_format(struct gaym_conn *gaym, const char *format, ...)
{
    GString *string = g_string_new("");
    char *tok, *tmp;
    const char *cur;
    va_list ap;

    va_start(ap, format);
    for (cur = format; *cur; cur++) {

        if (cur != format)
            g_string_append_c(string, ' ');

        tok = va_arg(ap, char *);

        switch (*cur) {
        case 'v':
            g_string_append(string, tok);
            break;
        case ':':
            g_string_append_c(string, ':');
            /* no break! */
        case 'n':
        case 't':
        case 'c':
            tmp = gaym_send_convert(gaym, tok);
            g_string_append(string, tmp);
            g_free(tmp);
            break;
        default:
            gaim_debug(GAIM_DEBUG_ERROR, "gaym",
                       "Invalid format character '%c'\n", *cur);
            break;
        }
    }
    va_end(ap);
    g_string_append(string, "\r\n");
    return (g_string_free(string, FALSE));
}

void gaym_parse_msg(struct gaym_conn *gaym, char *input)
{
    struct _gaym_msg *msgent;
    char *cur = NULL, *end = NULL, *tmp = NULL, *from = NULL, *msgname =
        NULL, *fmt = NULL, **args = NULL, *msg = NULL;
    guint i;

    gaim_debug(GAIM_DEBUG_INFO, "gaym", "RAW Protocol: %s\n", input);

    if (!strncmp(input, "PING ", 5)) {
        msg = gaym_format(gaym, "vv", "PONG", input + 5);
        gaym_send(gaym, msg);
        g_free(msg);
        return;
    } else if (!strncmp(input, "ERROR ", 6)) {
        gaim_connection_error(gaim_account_get_connection(gaym->account),
                              _("Disconnected."));
        return;
    }

    if (input[0] != ':' || (cur = strchr(input, ' ')) == NULL) {
        gaym_parse_error_cb(gaym, input);
        return;
    }

    from = g_strndup(&input[1], cur - &input[1]);

    cur++;
    end = strchr(cur, ' ');
    if (!end)
        end = cur + strlen(cur);
    tmp = g_strndup(cur, end - cur);
    msgname = g_ascii_strdown(tmp, -1);
    g_free(tmp);
    tmp = NULL;
    if ((msgent = g_hash_table_lookup(gaym->msgs, msgname)) == NULL) {
        gaym_msg_default(gaym, "", from, &input);
        g_free(msgname);
        g_free(from);
        return;
    }
    g_free(msgname);

    args = g_new0(char *, strlen(msgent->format));
    for (cur = end, fmt = msgent->format, i = 0; fmt[i] && *cur++; i++) {

        switch (fmt[i]) {
        case 'v':
            if (!(end = strchr(cur, ' ')))
                end = cur + strlen(cur);
            args[i] = g_strndup(cur, end - cur);
            cur += end - cur;
            break;
        case 't':
        case 'n':
        case 'c':
            if (!(end = strchr(cur, ' ')))
                end = cur + strlen(cur);
            tmp = g_strndup(cur, end - cur);
            args[i] = gaym_recv_convert(gaym, tmp);
            g_free(tmp);
            tmp = NULL;
            cur += end - cur;
            break;
        case ':':
            if (*cur == ':')
                cur++;
            args[i] = gaym_recv_convert(gaym, cur);
            cur = cur + strlen(cur);
            break;
        case '*':
            args[i] = g_strdup(cur);
            cur = cur + strlen(cur);
            break;
        default:
            gaim_debug(GAIM_DEBUG_ERROR, "gaym",
                       "invalid message format character '%c'\n", fmt[i]);
            break;
        }
    }
    tmp = gaym_recv_convert(gaym, from);
    (msgent->cb) (gaym, msgent->name, tmp, args);
    g_free(tmp);
    for (i = 0; i < strlen(msgent->format); i++) {
        g_free(args[i]);
    }
    g_free(args);
    g_free(from);
}

static void gaym_parse_error_cb(struct gaym_conn *gaym, char *input)
{
    gaim_debug(GAIM_DEBUG_WARNING, "gaym", "Unrecognized string: %s\n",
               input);
}

// vim:tabstop=4:shiftwidth=4:expandtab:
