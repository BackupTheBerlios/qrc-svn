/**
 * @file helpers.h GayM Helpers API
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
#ifndef _GAIM_GAYM_HELPERS_H_
#define _GAIM_GAYM_HELPERS_H_

#include "connection.h"

#include <glib.h>

char *return_string_between(const char *startbit, const char *endbit,
                            const char *source);

void gaym_convert_nick_to_gaycom(char *);

void convert_nick_from_gaycom(char *);

/**
 * Convert a string that is pure ascii, that uses a pure ascii approach
 * to representing utf-8 characters to native utf-8 encoding.
 *
 * @param str The string to convert.
 *
 * @return The converted string.
 */
gchar *ascii2native(const gchar * str);

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
 * @param account The account.
 * @param name    The user that was changed (added/removed from the
 *                permit/deny lists).  If the privacy type has changed
 *                this must be NULL so that all users are reset and
 *                checked.
 */
void gaym_privacy_change(GaimConnection * gc, const char *name);

/**
 * Check if the string could be a valid user name.
 *
 * According to http://www.gay.com/members/join/ the member name is:
 *
 * - Up to 30 characters
 * - Must begin with a letter
 * - Can contain only letters, numbers, and underscores ( _ )
 * - Cannot contain special characters, spaces, periods, or hyphens (-)
 *
 * However, in testing as well as observing existing members, the
 * the member name may in fact contain periods and hyphens.
 *
 * @param nick The string to check.
 */
gboolean gaym_nick_check(const char *nick);

#endif                          /* _GAIM_GAYM_HELPERS_H_ */

/**
 * vim:tabstop=4:shiftwidth=4:expandtab:
 */
