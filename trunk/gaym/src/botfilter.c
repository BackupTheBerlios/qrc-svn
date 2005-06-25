/**
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
#include "prefs.h"
#include "util.h"
#include "debug.h"

#include "botfilter.h"

typedef struct _FilteredBot FilteredBot;
struct _FilteredBot {
    char *username;
    /**
     * We may need the following later:
     *
     * char *bio;
     */
};
static GSList *filtered_bots = NULL;    /* GSList of FilteredBot */

void update_filtered_bots(const char *nick, gboolean permitted)
{
    GSList *search = NULL;
    gboolean found = FALSE;
    for (search = filtered_bots; search; search = search->next) {
        FilteredBot *bot = search->data;
        if (!gaim_utf8_strcasecmp(bot->username, nick)) {
            found = TRUE;
            break;
        }
    }

    if (permitted) {
        if (found) {
            FilteredBot *bot = search->data;
            g_free(bot->username);
            g_free(bot);
            filtered_bots = g_slist_remove_link(filtered_bots, search);
        }
    } else {
        if (!found) {
            FilteredBot *bot = g_new0(FilteredBot, 1);
            bot->username = g_strdup(nick);
            filtered_bots = g_slist_append(filtered_bots, bot);
        }
    }
}

gboolean gaym_botfilter_check(GaimConnection * gc, const char *nick,
                              const char *text,
                              gboolean gaym_privacy_change)
{
    /**
     * returns TRUE if allowed through, FALSE otherwise
     */
    gboolean permitted = TRUE;
    int i = 0;

    if (!gaim_prefs_get_bool("/plugins/prpl/gaym/botfilter_enable")) {
        permitted = TRUE;
        update_filtered_bots(nick, permitted);
        return permitted;
    }

    if (!gaim_utf8_strcasecmp(gc->account->username, nick)) {
        permitted = TRUE;
        gaim_debug_info("gaym", "declining to block/ignore self\n");
        update_filtered_bots(nick, permitted);
        return permitted;
    }

    /**
     * prevent gaim privacy changes from affecting previously
     * filtered bots
     */
    if (gaym_privacy_change) {
        permitted = TRUE;
        GSList *search = NULL;
        for (search = filtered_bots; search; search = search->next) {
            FilteredBot *bot = search->data;
            if (!gaim_utf8_strcasecmp(bot->username, nick)) {
                permitted = FALSE;
                break;
            }
        }
        return permitted;
    }

    if (!text) {
        if (gaim_prefs_get_bool
            ("/plugins/prpl/gaym/botfilter_ignore_null")) {
            permitted = FALSE;
        } else {
            permitted = TRUE;
        }
        update_filtered_bots(nick, permitted);
        return permitted;
    }

    const char *sep =
        gaim_prefs_get_string("/plugins/prpl/gaym/botfilter_sep");
    const char *patterns =
        gaim_prefs_get_string("/plugins/prpl/gaym/botfilter_patterns");
    if (!sep || !patterns) {
        permitted = TRUE;
        update_filtered_bots(nick, permitted);
        return permitted;
    }

    gchar **pattern_list = g_strsplit(patterns, sep, 0);

    for (i = 0; pattern_list[i] != NULL; i++) {
        if (g_pattern_match_simple(pattern_list[i], text)) {
            permitted = FALSE;
            break;
        }
    }
    g_strfreev(pattern_list);

    update_filtered_bots(nick, permitted);

    return permitted;
}

/**
 * vim:tabstop=4:shiftwidth=4:expandtab:
 */
