/**
 * @file gaympriv.h GayM Privacy API
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
#ifndef _GAIM_GAYM_GAYMPRIV_H_
#define _GAIM_GAYM_GAYMPRIV_H_

#include "connection.h"

#include <glib.h>

/**
 * Check if the account's privacy settings allow or block the user
 * (shamelessly taken from the yahoo prpl).
 *
 * @param gc   The connection.
 * @param nick The user to check.
 *
 * @return TRUE if the user is allowed, or @c FALSE otherwise.
 */
gboolean gaym_privacy_check(GaimConnection * gc, const char *nick);

/**
 * Respond to notification that the account's privacy settings have
 * changed.
 *
 * @param         The connection.
 * @param name    The user that was changed (added/removed from the
 *                permit/deny lists).  If the privacy type has changed
 *                this must be NULL so that all users are reset and
 *                checked.
 */
void gaym_privacy_change(GaimConnection * gc, const char *name);

/**
 * Report the status of the http request to add a name to the deny
 * list or remove a name from the deny list.
 *
 * @param gc     The connection.
 * @param data   The data passed from gaim_url_fetch().
 * @param result The information fetched by the http GET.
 * @param len    The length of the result.
 */
void gaym_server_change_deny_status_cb(void *data, const char *result,
                                       size_t len);

/**
 * Add a name to (or remove a name from) the Gay.com server's deny list.
 *
 * @param      gc The connection.
 * @param name The name of the user to add or remove.
 * @param add  TRUE of the name should be added, FALSE if the name should be removed.
 */
void gaym_server_store_deny(GaimConnection * gc, const char *name,
                            gboolean add);

#endif                          /* _GAIM_GAYM_GAYMPRIV_H_ */

/**
 * vim:tabstop=4:shiftwidth=4:expandtab:
 */
