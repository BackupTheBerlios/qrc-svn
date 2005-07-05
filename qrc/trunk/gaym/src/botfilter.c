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

/* 1 day */
#define MIN_CHECK_INTERVAL 60 * 60 * 24

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

    /* don't ignore self */
    if (!gaim_utf8_strcasecmp(gc->account->username, nick)) {
        permitted = TRUE;
        gaim_debug_info("gaym", "declining to block/ignore self\n");
        update_filtered_bots(nick, permitted);
        return permitted;
    }

    /* don't ignore buddies */
    if (gaim_find_buddy(gc->account, nick)) {
        permitted = TRUE;
        return permitted;
    }

    /* don't ignore permit list members */
    GSList *slist = NULL;
    for (slist = gc->account->permit; slist != NULL; slist = slist->next) {
        if (!gaim_utf8_strcasecmp
            (nick, gaim_normalize(gc->account, (char *) slist->data))) {
            permitted = TRUE;
            return permitted;
        }
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
    const char *gayboi_patterns =
        gaim_prefs_get_string("/plugins/prpl/gaym/botfilter_url_result");

    if (!sep || !gaim_utf8_strcasecmp(sep, "")
        || ((!patterns || !gaim_utf8_strcasecmp(patterns, ""))
            && (!gayboi_patterns
                || !gaim_utf8_strcasecmp(gayboi_patterns, "")))) {
        permitted = TRUE;
        update_filtered_bots(nick, permitted);
        return permitted;
    }

    char *combined = NULL;
    if (patterns && gaim_utf8_strcasecmp(patterns, "") && gayboi_patterns
        && gaim_utf8_strcasecmp(gayboi_patterns, "")) {
        combined = g_strdup_printf("%s|%s", patterns, gayboi_patterns);
    } else {
        if (patterns && gaim_utf8_strcasecmp(patterns, "")) {
            combined = g_strdup_printf("%s", patterns);
        } else if (gayboi_patterns
                   && gaim_utf8_strcasecmp(gayboi_patterns, "")) {
            combined = g_strdup_printf("%s", gayboi_patterns);
        }
    }


    gchar **pattern_list = g_strsplit(combined, sep, 0);

    for (i = 0; pattern_list[i] != NULL; i++) {
        if (g_pattern_match_simple(pattern_list[i], text)) {
            permitted = FALSE;
            break;
        }
    }
    g_free(combined);
    g_strfreev(pattern_list);

    update_filtered_bots(nick, permitted);

    return permitted;
}

void process_spamlist_from_web_cb(void *data, const char *result,
                                  size_t len)
{
    if (!result) {
        gaim_prefs_set_string("/plugins/prpl/gaym/botfilter_url_result",
                              "");
        gaim_prefs_set_int("/plugins/prpl/gaym/botfilter_url_last_check",
                           0);
        return;
    }
    if (!g_str_has_prefix(result, "<HTML>\n")) {
        gaim_prefs_set_string("/plugins/prpl/gaym/botfilter_url_result",
                              "");
        gaim_prefs_set_int("/plugins/prpl/gaym/botfilter_url_last_check",
                           0);
        return;
    }
    if (!g_str_has_suffix(result, "</HTML>")) {
        gaim_prefs_set_string("/plugins/prpl/gaym/botfilter_url_result",
                              "");
        gaim_prefs_set_int("/plugins/prpl/gaym/botfilter_url_last_check",
                           0);
        return;
    }
    const char *sep =
        gaim_prefs_get_string("/plugins/prpl/gaym/botfilter_sep");
    if (!sep || !gaim_utf8_strcasecmp(sep, "")) {
        gaim_prefs_set_string("/plugins/prpl/gaym/botfilter_url_result",
                              "");
        gaim_prefs_set_int("/plugins/prpl/gaym/botfilter_url_last_check",
                           0);
        return;
    }
    char *html_stripped = gaim_markup_strip_html(result);
    char *stripped = g_strstrip(html_stripped);
    char **split = g_strsplit(stripped, "\n ", 0);
    char *starsep = g_strdup_printf("*%s*", sep);
    char *joined = g_strjoinv(starsep, split);
    char *url_result = g_strdup_printf("*%s*", joined);

    gaim_prefs_set_string("/plugins/prpl/gaym/botfilter_url_result",
                          url_result);

    g_free(url_result);
    g_free(joined);
    g_free(starsep);
    g_strfreev(split);
    g_free(html_stripped);
}

void get_spamlist_from_web(void)
{
    char *user_agent = "Mozilla/4.0";

    const char *url =
        gaim_prefs_get_string("/plugins/prpl/gaym/botfilter_url");
    if (!url || !gaim_utf8_strcasecmp(url, "")) {
        gaim_prefs_set_string("/plugins/prpl/gaym/botfilter_url_result",
                              "");
        gaim_prefs_set_int("/plugins/prpl/gaym/botfilter_url_last_check",
                           0);
        return;
    }

    int last_check =
        gaim_prefs_get_int("/plugins/prpl/gaym/botfilter_url_last_check");


    if (!last_check || time(NULL) - last_check > MIN_CHECK_INTERVAL) {
        gaim_prefs_set_int("/plugins/prpl/gaym/botfilter_url_last_check",
                           time(NULL));
        gaim_url_fetch(url, FALSE, user_agent, FALSE,
                       process_spamlist_from_web_cb, NULL);
    }

    return;
}

void botfilter_url_changed_cb(const char *name, GaimPrefType type,
                              gpointer value, gpointer data)
{
    gaim_prefs_set_int("/plugins/prpl/gaym/botfilter_url_last_check", 0);
}

/**
 * vim:tabstop=4:shiftwidth=4:expandtab:
 */
