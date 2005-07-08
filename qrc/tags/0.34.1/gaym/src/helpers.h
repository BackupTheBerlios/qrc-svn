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

#include <glib.h>

char *return_string_between(const char *startbit, const char *endbit,
                            const char *source);

/**
 * Convert a nick from gay.com to what GayM needs to see.
 * The conversion is done in place.  The converted value
 * is for use within GayM/Gaim as well as with http requests
 * to gay.com. When interacting with gay.com's IRC server,
 * the nick must be converted using gaym_nick_to_gcom_strdup().
 *
 * @param nick The nick to convert.
 */
void gcom_nick_to_gaym(char *nick);

/**
 * Convert a nick to what the gay.com IRC server needs to see.
 * The returned string should be freed when no longer needed.
 *
 * @param nick The nick to convert.
 *
 * @return The converted nick.
 */
char *gaym_nick_to_gcom_strdup(const char *nick);

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
 * the member name may in fact contain periods and hyphens, and may
 * even begin with numbers and other special characters.
 *
 * @param nick The string to check.
 */
gboolean gaym_nick_check(const char *nick);

#endif                          /* _GAIM_GAYM_HELPERS_H_ */

/**
 * vim:tabstop=4:shiftwidth=4:expandtab:
 */
