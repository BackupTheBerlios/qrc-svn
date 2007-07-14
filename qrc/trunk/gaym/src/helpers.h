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
#ifndef _PURPLE_GAYM_HELPERS_H_
#define _PURPLE_GAYM_HELPERS_H_

#include <glib.h>

#include "roomlist.h"

#include "gaym.h"

char *return_string_between(const char *startbit, const char *endbit,
                            const char *source);

/**
 * Convert a nick from gay.com to what GayM needs to see.
 * The conversion is done in place.  The converted value
 * is for use within GayM/Purple as well as with http requests
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

/**
 * This function is for use by g_hash_table_foreach() from within
 * gaym_properties_new().  It is to replace $0, $1, ... with %s
 * so we can use printf style processing with the provided property
 * values.
 *
 * @param key       Not used.
 * @param value     The string to search/replace.
 * @param user_data Not used.
 */
void replace_dollar_n(gpointer key, gpointer value, gpointer user_data);

/**
 * Build a GHashTable from a string that contains the contents of java
 * properties file.
 *
 * This is built with g_hash_table_new_full() so when finished with
 * the GHashTable, use only g_hash_table_destroy() to clean up.
 *
 * To retrieve a property, use g_hash_table_lookup().
 *
 * @param str The contents of the java properties file
 *
 * @return The GHashTable containing the properties
 */
GHashTable *gaym_properties_new(const char *str);

/**
 * Gay.com provides a java property that contains instructions for
 * building a hierarchical roomlist.  The level is shown by
 * appending a number of '+' characters to the beginning of the
 * room description.  This function strips the '+' characters
 * and also returns the number of '+' characters found.
 *
 * @param description The string to strip and from which to calculate
 *                    the level.
 *
 * @return The level (number of '+' characters found).
 */
int roomlist_level_strip(char *description);

/**
 * Take the level of the room to be added, the level of the last
 * added room, and the last added room; and return the roomlist
 * parent of the room to be added.
 *
 * @param level The level of the room to be added.
 * @param old_level The level of the last added room.
 * @param last_room The last added room.
 *
 * @return The parent of the room to be added.
 */
PurpleRoomlistRoom *find_parent(int level, int old_level,
                              PurpleRoomlistRoom * last_room);

/**
 * Build the portion of the roomlist that is provided in the
 * config.txt java properties file within the property "roomlist".
 *
 * @param roomlist The PurpleRoomlist that these rooms should be loaded
 *                 into.
 * @param confighash The GHashTable that config.txt was converted into
 * @param pattern The pattern to match against or NULL for everythying
 */
void build_roomlist_from_config(PurpleRoomlist * roomlist,
                                GHashTable * confighash, gchar * pattern);

/**
 * Determine the correct PurpleConvChatBuddyFlags based on the "extra"
 * information that is provided during join, whois, etc.
 *
 * @param extra The string containing the information about the flags.
 *
 * @return The correct PurpleConvChatBuddyFlags.
 */
PurpleConvChatBuddyFlags chat_pecking_order(const char *extra);

/**
 * Format and return the tooltip text for a buddy/user
 *
 * @param ib The stuct containing the information about the buddy
 * @param info The info struct in which to place the tooltip info
 *
 */
void build_tooltip_text(struct gaym_buddy *ib, PurpleNotifyUserInfo *info);

PurpleConvChatBuddyFlags include_chat_entry_order(PurpleConvChatBuddyFlags
                                                flags, gint entry);
#endif                          /* _PURPLE_GAYM_HELPERS_H_ */

/**
 * vim:tabstop=4:shiftwidth=4:expandtab:
 */
