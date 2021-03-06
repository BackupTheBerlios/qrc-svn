/**
 * @file dcc_send.c Functions used in sending files with DCC SEND
 *
 * gaim
 *
 * Copyright (C) 2004, Timothy T Ringenbach <omarvo@hotmail.com>
 * Copyright (C) 2003, Robbert Haarman <gaim@inglorion.net>
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

#include "gaym.h"
#include "debug.h"
#include "ft.h"
#include "notify.h"
#include "network.h"

/***************************************************************************
 * Functions related to receiving files via DCC SEND
 ***************************************************************************/

struct gaym_xfer_rx_data {
    gchar *ip;
};

static void gaym_dccsend_recv_destroy(GaimXfer * xfer)
{
    struct gaym_xfer_rx_data *xd = xfer->data;

    if (xd->ip != NULL)
        g_free(xd->ip);

    g_free(xd);
}

/* 
 * This function is called whenever data is received.
 * It sends the acknowledgement (in the form of a total byte count as an
 * unsigned 4 byte integer in network byte order)
 */
static void gaym_dccsend_recv_ack(GaimXfer * xfer, const char *data,
                                  size_t size)
{
    unsigned long l;

    l = htonl(xfer->bytes_sent);
    write(xfer->fd, &l, sizeof(l));
}

static void gaym_dccsend_recv_init(GaimXfer * xfer)
{
    struct gaym_xfer_rx_data *xd = xfer->data;

    gaim_xfer_start(xfer, -1, xd->ip, xfer->remote_port);
    g_free(xd->ip);
    xd->ip = NULL;
}

/* This function makes the necessary arrangements for receiving files */
void gaym_dccsend_recv(struct gaym_conn *gaym, const char *from,
                       const char *msg)
{
    GaimXfer *xfer;
    struct gaym_xfer_rx_data *xd;
    gchar **token;
    struct in_addr addr;
    GString *filename;
    int i = 0;
    guint32 nip;

    token = g_strsplit(msg, " ", 0);
    if (!token[0] || !token[1] || !token[2]) {
        g_strfreev(token);
        return;
    }

    filename = g_string_new("");
    if (token[0][0] == '"') {
        if (!strchr(&(token[0][1]), '"')) {
            g_string_append(filename, &(token[0][1]));
            for (i = 1; token[i]; i++)
                if (!strchr(token[i], '"')) {
                    g_string_append_printf(filename, " %s", token[i]);
                } else {
                    g_string_append_len(filename, token[i],
                                        strlen(token[i]) - 1);
                    break;
                }
        } else {
            g_string_append_len(filename, &(token[0][1]),
                                strlen(&(token[0][1])) - 1);
        }
    } else {
        g_string_append(filename, token[0]);
    }

    if (!token[i] || !token[i + 1] || !token[i + 2]) {
        g_strfreev(token);
        g_string_free(filename, TRUE);
        return;
    }
    i++;

    xfer = gaim_xfer_new(gaym->account, GAIM_XFER_RECEIVE, from);
    xd = g_new0(struct gaym_xfer_rx_data, 1);
    xfer->data = xd;

    gaim_xfer_set_filename(xfer, filename->str);
    xfer->remote_port = atoi(token[i + 1]);

    nip = strtoul(token[i], NULL, 10);
    if (nip) {
        addr.s_addr = htonl(nip);
        xd->ip = g_strdup(inet_ntoa(addr));
    } else {
        xd->ip = g_strdup(token[i]);
    }
    gaim_debug(GAIM_DEBUG_INFO, "gaym", "Receiving file from %s\n",
               xd->ip);
    gaim_xfer_set_size(xfer, token[i + 2] ? atoi(token[i + 2]) : 0);

    gaim_xfer_set_init_fnc(xfer, gaym_dccsend_recv_init);
    gaim_xfer_set_ack_fnc(xfer, gaym_dccsend_recv_ack);

    gaim_xfer_set_end_fnc(xfer, gaym_dccsend_recv_destroy);
    gaim_xfer_set_request_denied_fnc(xfer, gaym_dccsend_recv_destroy);
    gaim_xfer_set_cancel_send_fnc(xfer, gaym_dccsend_recv_destroy);

    gaim_xfer_request(xfer);
    g_strfreev(token);
    g_string_free(filename, TRUE);
}

/*******************************************************************
 * Functions related to sending files via DCC SEND
 *******************************************************************/

struct gaym_xfer_send_data {
    gint inpa;
    int fd;
    guchar *rxqueue;
    guint rxlen;
};

static void gaym_dccsend_send_destroy(GaimXfer * xfer)
{
    struct gaym_xfer_send_data *xd = xfer->data;

    if (xd == NULL)
        return;

    if (xd->inpa > 0)
        gaim_input_remove(xd->inpa);
    if (xd->fd != -1)
        close(xd->fd);

    if (xd->rxqueue)
        g_free(xd->rxqueue);

    g_free(xd);
}

/* just in case you were wondering, this is why DCC is gay */
static void gaym_dccsend_send_read(gpointer data, int source,
                                   GaimInputCondition cond)
{
    GaimXfer *xfer = data;
    struct gaym_xfer_send_data *xd = xfer->data;
    char *buffer[16];
    int len;

    if ((len = read(source, buffer, sizeof(buffer))) <= 0) {
        gaim_input_remove(xd->inpa);
        xd->inpa = 0;
        return;
    }

    xd->rxqueue = g_realloc(xd->rxqueue, len + xd->rxlen);
    memcpy(xd->rxqueue + xd->rxlen, buffer, len);
    xd->rxlen += len;

    while (1) {
        int acked;

        if (xd->rxlen < 4)
            break;

        acked = ntohl(*((gint32 *) xd->rxqueue));

        xd->rxlen -= 4;
        if (xd->rxlen) {
            char *tmp = g_memdup(xd->rxqueue + 4, xd->rxlen);
            g_free(xd->rxqueue);
            xd->rxqueue = tmp;
        } else {
            g_free(xd->rxqueue);
            xd->rxqueue = NULL;
        }

        if (acked >= gaim_xfer_get_size(xfer)) {
            gaim_input_remove(xd->inpa);
            xd->inpa = 0;
            gaim_xfer_set_completed(xfer, TRUE);
            gaim_xfer_end(xfer);
            return;
        }


    }
}

ssize_t gaym_dccsend_send_write(const char *buffer, size_t size,
                                GaimXfer * xfer)
{
    ssize_t s;

    s = MIN(gaim_xfer_get_bytes_remaining(xfer), size);
    if (!s)
        return 0;

    return write(xfer->fd, buffer, s);
}

static void gaym_dccsend_send_connected(gpointer data, int source,
                                        GaimInputCondition cond)
{
    GaimXfer *xfer = (GaimXfer *) data;
    struct gaym_xfer_send_data *xd = xfer->data;
    int conn;

    conn = accept(xd->fd, NULL, 0);
    if (conn == -1) {
        /* Accepting the connection failed. This could just be related to
           the nonblocking nature of the listening socket, so we'll just
           try again next time */
        /* Let's print an error message anyway */
        gaim_debug_warning("gaym", "accept: %s\n", strerror(errno));
        return;
    }

    gaim_input_remove(xfer->watcher);
    xfer->watcher = 0;
    close(xd->fd);
    xd->fd = -1;

    xd->inpa =
        gaim_input_add(conn, GAIM_INPUT_READ, gaym_dccsend_send_read,
                       xfer);
    /* Start the transfer */
    gaim_xfer_start(xfer, conn, NULL, 0);
}

/* 
 * This function is called after the user has selected a file to send.
 */
static void gaym_dccsend_send_init(GaimXfer * xfer)
{
    struct gaym_xfer_send_data *xd = xfer->data;
    GaimConnection *gc =
        gaim_account_get_connection(gaim_xfer_get_account(xfer));
    struct gaym_conn *gaym = gc->proto_data;
    int sock;
    const char *arg[2];
    char *tmp;
    struct in_addr addr;
    unsigned short int port;

    xfer->filename = g_path_get_basename(xfer->local_filename);

    /* Create a listening socket */
    sock = gaim_network_listen_range(0, 0);

    if (sock < 0) {
        gaim_notify_error(gc, NULL, _("File Transfer Aborted"),
                          _("Gaim could not open a listening port."));
        gaim_xfer_cancel_local(xfer);
        return;
    }

    xd->fd = sock;

    port = gaim_network_get_port_from_fd(sock);
    gaim_debug_misc("gaym", "port is %hu\n", port);
    /* Monitor the listening socket */
    xfer->watcher = gaim_input_add(sock, GAIM_INPUT_READ,
                                   gaym_dccsend_send_connected, xfer);

    /* Send the intended recipient the DCC request */
    arg[0] = xfer->who;
    inet_aton(gaim_network_get_my_ip(gaym->fd), &addr);
    arg[1] = tmp = g_strdup_printf("\001DCC SEND \"%s\" %u %hu %zu\001",
                                   xfer->filename, ntohl(addr.s_addr),
                                   port, xfer->size);

    gaym_cmd_privmsg(xfer->account->gc->proto_data, "msg", NULL, arg);
    g_free(tmp);
}

/**
 * Gaim calls this function when the user selects Send File from the
 * buddy menu
 * It sets up the GaimXfer struct and tells Gaim to go ahead
 */
void gaym_dccsend_send_file(GaimConnection * gc, const char *who,
                            const char *file)
{
    GaimXfer *xfer;
    struct gaym_xfer_send_data *xd;

    /* Build the file transfer handle */
    xfer =
        gaim_xfer_new(gaim_connection_get_account(gc), GAIM_XFER_SEND,
                      who);


    xd = g_new0(struct gaym_xfer_send_data, 1);
    xd->fd = -1;
    xfer->data = xd;

    /* Setup our I/O op functions */
    gaim_xfer_set_init_fnc(xfer, gaym_dccsend_send_init);
    gaim_xfer_set_write_fnc(xfer, gaym_dccsend_send_write);
    gaim_xfer_set_end_fnc(xfer, gaym_dccsend_send_destroy);
    gaim_xfer_set_request_denied_fnc(xfer, gaym_dccsend_send_destroy);
    gaim_xfer_set_cancel_send_fnc(xfer, gaym_dccsend_send_destroy);
    /* Now perform the request */
    if (file)
        gaim_xfer_request_accepted(xfer, file);
    else
        gaim_xfer_request(xfer);
}

/**
 * vim:tabstop=4:shiftwidth=4:expandtab:
 */
