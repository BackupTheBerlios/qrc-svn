/**
 * @file cmds.c
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

#include "conversation.h"
#include "debug.h"
#include "notify.h"
#include "util.h"
#include "helpers.h"
#include "gaym.h"



static void gaym_do_mode(struct gaym_conn *gaym, const char *target,
                         const char *sign, char **ops);

int gaym_cmd_default(struct gaym_conn *gaym, const char *cmd,
                     const char *target, const char **args)
{
    GaimConversation *convo =
        gaim_find_conversation_with_account(target, gaym->account);
    char *buf;

    if (!convo)
        return 1;

    buf = g_strdup_printf(_("Unknown command: %s"), cmd);
    if (gaim_conversation_get_type(convo) == GAIM_CONV_IM)
        gaim_conv_im_write(GAIM_CONV_IM(convo), "", buf,
                           GAIM_MESSAGE_SYSTEM | GAIM_MESSAGE_NO_LOG,
                           time(NULL));
    else
        gaim_conv_chat_write(GAIM_CONV_CHAT(convo), "", buf,
                             GAIM_MESSAGE_SYSTEM | GAIM_MESSAGE_NO_LOG,
                             time(NULL));
    g_free(buf);

    return 1;
}

int gaym_cmd_away(struct gaym_conn *gaym, const char *cmd,
                  const char *target, const char **args)
{
    char *buf, *message, *cur;

    if (args[0] && strcmp(cmd, "back")) {
        message = strdup(args[0]);
        for (cur = message; *cur; cur++) {
            if (*cur == '\n')
                *cur = ' ';
        }
        buf = gaym_format(gaym, "v:", "AWAY", message);
        g_free(message);
    } else {
        buf = gaym_format(gaym, "v", "AWAY");
    }
    gaym_send(gaym, buf);
    g_free(buf);

    return 0;
}

int gaym_cmd_ctcp_action(struct gaym_conn *gaym, const char *cmd,
                         const char *target, const char **args)
{
    GaimConnection *gc = gaim_account_get_connection(gaym->account);
    char *action, *dst, **newargs;
    const char *src;
    GaimConversation *convo;

    if (!args || !args[0] || !gc)
        return 0;

    action = g_malloc(strlen(args[0]) + 10);

    sprintf(action, "\001ACTION ");

    src = args[0];
    dst = action + 8;
    while (*src) {
        if (*src == '\n') {
            if (*(src + 1) == '\0') {
                break;
            } else {
                *dst++ = ' ';
                src++;
                continue;
            }
        }
        *dst++ = *src++;
    }
    *dst++ = '\001';
    *dst = '\0';

    newargs = g_new0(char *, 2);
    newargs[0] = g_strdup(target);
    newargs[1] = action;
    gaym_cmd_privmsg(gaym, cmd, target, (const char **) newargs);
    g_free(newargs[0]);
    g_free(newargs[1]);
    g_free(newargs);

    convo = gaim_find_conversation_with_account(target, gaym->account);
    if (convo) {
        action = g_strdup_printf("/me %s", args[0]);
        if (action[strlen(action) - 1] == '\n')
            action[strlen(action) - 1] = '\0';
        if (gaim_conversation_get_type(convo) == GAIM_CONV_CHAT)
            serv_got_chat_in(gc,
                             gaim_conv_chat_get_id(GAIM_CONV_CHAT(convo)),
                             gaim_connection_get_display_name(gc), 0,
                             action, time(NULL));
        else
            gaim_conv_im_write(GAIM_CONV_IM(convo),
                               gaim_connection_get_display_name(gc),
                               action, 0, time(NULL));
        g_free(action);
    }

    return 1;
}

int gaym_cmd_trace(struct gaym_conn *gaym, const char *cmd,
                   const char *target, const char **args)
{
    char *buf;

    buf = gaym_format(gaym, "vn", "TRACE", args[0]);
    gaym_send(gaym, buf);
    g_free(buf);

    gaym->traceconv = g_strdup(target);

    return 0;
}

int gaym_cmd_invite(struct gaym_conn *gaym, const char *cmd,
                    const char *target, const char **args)
{
    char *buf;

    if (!args || !args[0] || !(args[1] || target))
        return 0;

    buf =
        gaym_format(gaym, "vnc", "INVITE", args[0],
                    args[1] ? args[1] : target);
    gaym_send(gaym, buf);
    g_free(buf);

    return 0;
}

int gaym_cmd_join(struct gaym_conn *gaym, const char *cmd,
                  const char *target, const char **args)
{
    char *buf;

    if (!args || !args[0])
        return 0;

    buf = gaym_format(gaym, "cv", "JOIN", args[0]);

    gaym_send(gaym, buf);
    g_free(buf);

    return 0;
}

int gaym_cmd_kick(struct gaym_conn *gaym, const char *cmd,
                  const char *target, const char **args)
{
    char *buf;
    GaimConversation *convo;

    if (!args || !args[0])
        return 0;

    convo = gaim_find_conversation_with_account(target, gaym->account);
    if (!convo || gaim_conversation_get_type(convo) != GAIM_CONV_CHAT)
        return 0;

    if (args[1])
        buf = gaym_format(gaym, "vcn:", "KICK", target, args[0], args[1]);
    else
        buf = gaym_format(gaym, "vcn", "KICK", target, args[0]);
    gaym_send(gaym, buf);
    g_free(buf);

    return 0;
}

int gaym_cmd_list(struct gaym_conn *gaym, const char *cmd,
                  const char *target, const char **args)
{


    if (args[0])
        gaym->roomlist_filter = g_strdown(g_strdup(args[0]));
    else
        gaym->roomlist_filter = NULL;
    gaim_roomlist_show_with_account(gaym->account);

    return 0;
}

int gaym_cmd_mode(struct gaym_conn *gaym, const char *cmd,
                  const char *target, const char **args)
{
    GaimConnection *gc;
    char *buf;

    if (!args)
        return 0;

    if (!strcmp(cmd, "mode")) {
        if (!args[0] && (*target == '#' || *target == '&'))
            buf = gaym_format(gaym, "vc", "MODE", target);
        else if (args[0] && (*args[0] == '+' || *args[0] == '-'))
            buf = gaym_format(gaym, "vcv", "MODE", target, args[0]);
        else if (args[0])
            buf = gaym_format(gaym, "vv", "MODE", args[0]);
        else
            return 0;
    } else if (!strcmp(cmd, "umode")) {
        if (!args[0])
            return 0;
        gc = gaim_account_get_connection(gaym->account);
        buf =
            gaym_format(gaym, "vnv", "MODE",
                        gaim_connection_get_display_name(gc), args[0]);
    } else {
        return 0;
    }

    gaym_send(gaym, buf);
    g_free(buf);

    return 0;
}

int gaym_cmd_names(struct gaym_conn *gaym, const char *cmd,
                   const char *target, const char **args)
{
    char *buf;

    if (!args)
        return 0;

    buf = gaym_format(gaym, "vc", "NAMES", args[0] ? args[0] : target);
    gaym_send(gaym, buf);
    g_free(buf);

    gaym->nameconv = g_strdup(target);

    return 0;
}

int gaym_cmd_nick(struct gaym_conn *gaym, const char *cmd,
                  const char *target, const char **args)
{
    char *buf;
    char *temp;

    if (!args || !args[0])
        return 0;

    temp = gaym_nick_to_gcom_strdup(args[0]);
    buf = gaym_format(gaym, "v:", "NICK", temp);
    gaym_send(gaym, buf);
    g_free(buf);
    g_free(temp);
    return 0;
}

int gaym_cmd_op(struct gaym_conn *gaym, const char *cmd,
                const char *target, const char **args)
{
    char **nicks, **ops, *sign, *mode;
    int i = 0, used = 0;

    if (!args || !args[0] || !*args[0])
        return 0;

    if (!strcmp(cmd, "op")) {
        sign = "+";
        mode = "o";
    } else if (!strcmp(cmd, "deop")) {
        sign = "-";
        mode = "o";
    } else if (!strcmp(cmd, "voice")) {
        sign = "+";
        mode = "v";
    } else if (!strcmp(cmd, "devoice")) {
        sign = "-";
        mode = "v";
    } else {
        gaim_debug(GAIM_DEBUG_ERROR, "gaym", "invalid 'op' command '%s'\n",
                   cmd);
        return 0;
    }

    nicks = g_strsplit(args[0], " ", -1);

    for (i = 0; nicks[i]; i++)
        /* nothing */ ;
    ops = g_new0(char *, i * 2 + 1);

    for (i = 0; nicks[i]; i++) {
        if (!*nicks[i])
            continue;
        ops[used++] = mode;
        ops[used++] = nicks[i];
    }

    gaym_do_mode(gaym, target, sign, ops);
    g_free(ops);

    return 0;
}

int gaym_cmd_part(struct gaym_conn *gaym, const char *cmd,
                  const char *target, const char **args)
{
    char *buf;

    if (!args)
        return 0;

    if (args[1])
        buf =
            gaym_format(gaym, "vc:", "PART", args[0] ? args[0] : target,
                        args[1]);
    else
        buf = gaym_format(gaym, "vc", "PART", args[0] ? args[0] : target);
    gaym_send(gaym, buf);
    g_free(buf);

    return 0;
}

int gaym_cmd_ping(struct gaym_conn *gaym, const char *cmd,
                  const char *target, const char **args)
{
    char *stamp;
    char *buf;

    if (args && args[0]) {
        if (*args[0] == '#' || *args[0] == '&')
            return 0;
        stamp = g_strdup_printf("\001PING %lu\001", time(NULL));
        buf = gaym_format(gaym, "vn:", "PRIVMSG", args[0], stamp);
        g_free(stamp);
    } else {
        stamp = g_strdup_printf("%s %lu", target, time(NULL));
        buf = gaym_format(gaym, "v:", "PING", stamp);
        g_free(stamp);
    }
    gaym_send(gaym, buf);
    g_free(buf);

    return 0;
}

int gaym_cmd_privmsg(struct gaym_conn *gaym, const char *cmd,
                     const char *target, const char **args)
{
    const char *cur, *end;
    char *msg, *buf, *nick;

    if (!args || !args[0] || !args[1])
        return 0;

    /**
     * Only run gaym_nick_to_gcom_strdup() against nicks,
     * never on channels (which begin with either "#" or "&")
     */
    if (!args[0] == '#' && !args[0] == '&') {
        nick = gaym_nick_to_gcom_strdup(args[0]);
    } else {
        nick = g_strdup(args[0]);
    }
    cur = args[1];
    end = args[1];
    while (*end && *cur) {
        end = strchr(cur, '\n');
        if (!end)
            end = cur + strlen(cur);
        msg = g_strndup(cur, end - cur);
        buf = gaym_format(gaym, "vt:", "PRIVMSG", nick, msg);
        gaym_send(gaym, buf);
        g_free(msg);
        g_free(buf);
        cur = end + 1;
    }
    g_free(nick);
    return 0;
}

int gaym_cmd_quit(struct gaym_conn *gaym, const char *cmd,
                  const char *target, const char **args)
{
    char *buf;

    if (!gaym->quitting) {
        buf =
            gaym_format(gaym, "v:", "QUIT",
                        (args
                         && args[0]) ? args[0] : "Download Gaim: "
                        GAIM_WEBSITE);
        gaym_send(gaym, buf);
        g_free(buf);

        gaym->quitting = TRUE;
    }

    return 0;
}

int gaym_cmd_quote(struct gaym_conn *gaym, const char *cmd,
                   const char *target, const char **args)
{
    char *buf;

    if (!args || !args[0])
        return 0;

    buf = gaym_format(gaym, "v", args[0]);
    gaym_send(gaym, buf);
    g_free(buf);

    return 0;
}

int gaym_cmd_query(struct gaym_conn *gaym, const char *cmd,
                   const char *target, const char **args)
{
    GaimConversation *convo;
    GaimConnection *gc;

    if (!args || !args[0])
        return 0;

    convo = gaim_conversation_new(GAIM_CONV_IM, gaym->account, args[0]);

    if (args[1]) {
        gc = gaim_account_get_connection(gaym->account);
        gaym_cmd_privmsg(gaym, cmd, target, args);
        gaim_conv_im_write(GAIM_CONV_IM(convo),
                           gaim_connection_get_display_name(gc), args[1],
                           GAIM_MESSAGE_SEND, time(NULL));
    }

    return 0;
}

int gaym_cmd_remove(struct gaym_conn *gaym, const char *cmd,
                    const char *target, const char **args)
{
    char *buf;

    if (!args || !args[0])
        return 0;

    if (*target != '#' && *target != '&')       /* not a channel, punt */
        return 0;

    if (args[1])
        buf =
            gaym_format(gaym, "vcn:", "REMOVE", target, args[0], args[1]);
    else
        buf = gaym_format(gaym, "vcn", "REMOVE", target, args[0]);
    gaym_send(gaym, buf);
    g_free(buf);

    return 0;
}

int gaym_cmd_topic(struct gaym_conn *gaym, const char *cmd,
                   const char *target, const char **args)
{
    char *buf;
    const char *topic;
    GaimConversation *convo;

    if (!args)
        return 0;

    convo = gaim_find_conversation_with_account(target, gaym->account);
    if (!convo || gaim_conversation_get_type(convo) != GAIM_CONV_CHAT)
        return 0;

    if (!args[0]) {
        topic = gaim_conv_chat_get_topic(GAIM_CONV_CHAT(convo));

        if (topic) {
            char *tmp, *tmp2;
            tmp = gaim_escape_html(topic);
            tmp2 = gaim_markup_linkify(tmp);
            buf = g_strdup_printf(_("current topic is: %s"), tmp2);
            g_free(tmp);
            g_free(tmp2);
        } else
            buf = g_strdup(_("No topic is set"));
        gaim_conv_chat_write(GAIM_CONV_CHAT(convo), target, buf,
                             GAIM_MESSAGE_SYSTEM | GAIM_MESSAGE_NO_LOG,
                             time(NULL));
        g_free(buf);

        return 0;
    }

    buf = gaym_format(gaym, "vt:", "TOPIC", target, args[0]);
    gaym_send(gaym, buf);
    g_free(buf);

    return 0;
}

int gaym_cmd_wallops(struct gaym_conn *gaym, const char *cmd,
                     const char *target, const char **args)
{
    char *buf;

    if (!args || !args[0])
        return 0;

    if (!strcmp(cmd, "wallops"))
        buf = gaym_format(gaym, "v:", "WALLOPS", args[0]);
    else if (!strcmp(cmd, "operwall"))
        buf = gaym_format(gaym, "v:", "OPERWALL", args[0]);
    else
        return 0;

    gaym_send(gaym, buf);
    g_free(buf);

    return 0;
}

int gaym_cmd_whois(struct gaym_conn *gaym, const char *cmd,
                   const char *target, const char **args)
{
    char *buf;
    char *converted_nick;
    if (!args || !args[0])
        return 0;

    gaym->whois.nick = g_strdup(args[0]);
    gcom_nick_to_gaym(gaym->whois.nick);
    converted_nick = gaym_nick_to_gcom_strdup(args[0]);
    buf = gaym_format(gaym, "vn", "WHOIS", converted_nick);
    gaym_send(gaym, buf);
    g_free(buf);
    g_free(converted_nick);
    return 0;
}

static void gaym_do_mode(struct gaym_conn *gaym, const char *target,
                         const char *sign, char **ops)
{
    char *buf, mode[5];
    int i = 0;

    if (!sign)
        return;

    while (ops[i]) {
        if (ops[i + 2] && ops[i + 4]) {
            g_snprintf(mode, sizeof(mode), "%s%s%s%s", sign,
                       ops[i], ops[i + 2], ops[i + 4]);
            buf = gaym_format(gaym, "vcvnnn", "MODE", target, mode,
                              ops[i + 1], ops[i + 3], ops[i + 5]);
            i += 6;
        } else if (ops[i + 2]) {
            g_snprintf(mode, sizeof(mode), "%s%s%s",
                       sign, ops[i], ops[i + 2]);
            buf = gaym_format(gaym, "vcvnn", "MODE", target, mode,
                              ops[i + 1], ops[i + 3]);
            i += 4;
        } else {
            g_snprintf(mode, sizeof(mode), "%s%s", sign, ops[i]);
            buf =
                gaym_format(gaym, "vcvn", "MODE", target, mode,
                            ops[i + 1]);
            i += 2;
        }
        gaym_send(gaym, buf);
        g_free(buf);
    }

    return;
}

/**
 * vim:tabstop=4:shiftwidth=4:expandtab:
 */
