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
 * Selectively ignores joining / leaving messages based on GayM
 * preferences and Gaim privacy settings.
 *
 * @param gc   The connection.
 * @param name The user joining or leaving.
 *
 * @return 0 to show the join/leave message, or @c 1 otherwise.
 */
int gaym_ignore_joining_leaving(GaimConversation * conv, char *name);

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
 * Check if the plugin's settings allow or block an IM.
 *
 * @param gc   The connection.
 * @param nick The user sending the IM.
 *
 * @return TRUE if the user is allowed, or @c FALSE otherwise.
 */
gboolean gaym_im_check(GaimConnection * gc, const char *nick,
                       const char *msg);

#endif                          /* _GAIM_GAYM_GAYMPRIV_H_ */

/**
 * vim:tabstop=4:shiftwidth=4:expandtab:
 */
