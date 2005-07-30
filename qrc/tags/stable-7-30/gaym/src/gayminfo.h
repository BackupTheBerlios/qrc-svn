/**
 * @file gayminfo.h GayM User Info (IRC-based) API
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
#ifndef _GAIM_GAYM_GAYMINFO_H_
#define _GAIM_GAYM_GAYMINFO_H_

#include <glib.h>

#include "gaym.h"

/**
 * Begin temporary, pending further refactoring
 */
#include "connection.h"
struct gaym_fetch_thumbnail_data {
    GaimConnection *gc;
    char *who;
    char *bio;
    char *stats;
    // const char *pic_data;
    // gint pic_data_len;
};
void gaym_fetch_thumbnail_cb(void *user_data, const char *pic_data,
                             size_t len);
/**
 * End temporary, pending further refactoring
 */

/**
 * Extract the thumbnail string from the extra IRC info about the user.
 * The returned string should be freed when no longer needed.
 *
 * @param info The extra IRC info string.
 *
 * @return The thumbnail string.
 */
char *gaym_thumbnail_strdup(const char *info);

/**
 * Extract the bio string from the extra IRC info about the user.
 * The returned string should be freed when no longer needed.
 *
 * @param info The extra IRC info string.
 *
 * @return The bio string.
 */
char *gaym_bio_strdup(const char *info);

/**
 * Extract the stats string from the extra IRC info about the user.
 * The returned string should be freed when no longer needed.
 *
 * @param info The extra IRC info string.
 *
 * @return The stats string.
 */
char *gaym_stats_strdup(const char *info);

/**
 * Process extra IRC information about a buddy
 *
 * @param gaym The protocol-specific data related to the connection.
 * @param name The buddy name
 * @param online Is the buddy on line.
 * @param info The extra IRC info string about the buddy, if any.
 */
void gaym_buddy_status(struct gaym_conn *gaym, char *name,
                       gboolean online, char *info);


void gaym_update_channel_member(struct gaym_conn *gaym, const char *nick,
                                const char *info);
#endif                          /* _GAIM_GAYM_GAYMINFO_H_ */

/**
 * vim:tabstop=4:shiftwidth=4:expandtab:
 */
