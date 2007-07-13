/**
 * @file botfilter.h GayM Bot Filter API
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
#ifndef _GAIM_GAYM_BOTFILTER_H_
#define _GAIM_GAYM_BOTFILTER_H_

#include "connection.h"

#include <glib.h>

/**
 * Check if the account's botfilter settings allow or block the text
 *
 * @param gc                  The connection.
 * @param nick                The user to check.
 * @param text                The text to check.
 * @param gaym_privacy_change Invoked from gaym_privacy_change().
 *
 * @return TRUE if the user is allowed, or @c FALSE otherwise.
 */
gboolean gaym_botfilter_check(GaimConnection * gc, const char *nick,
                              const char *text,
                              gboolean gaym_privacy_change);

 /**
 * Get GayBoi's spam list from his website.  Since this is for the plugin
 * and not for any certain account, it should be done when the plugin
 * is loaded.
 */
void get_spamlist_from_web(void);

void botfilter_url_changed_cb(const char *name, GaimPrefType type,
                              gconstpointer value, gpointer data);

#endif                          /* _GAIM_GAYM_BOTFILTER_H_ */

/**
 * vim:tabstop=4:shiftwidth=4:expandtab:
 */
