/**
 * @file gayminfo.c
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

#include "gayminfo.h"
#include "util.h"
#include "debug.h"

char *gaym_thumbnail_strdup(const char *info)
{
    char *start = strchr(info, ':');
    char *end = 0;
    if (start) {
        start++;
        end = strchr(info, '#');
    }
    if (start != end && end) {
        return g_strdup_printf("%.*s", end - start, start);
    } else {
        return 0;
    }
}

char *gaym_bio_strdup(const char *info)
{
    char *start = strchr(info, '#');
    char *end = 0;
    if (start) {
        start++;
        end = strchr(start, 0x01);
        if (!end)
            end = strchr(start, 0);
    }

    if ((end) && (start < end)) {
        return g_strdup_printf("%.*s", end - start, start);
    } else {
        return 0;
    }
}

char *gaym_stats_strdup(const char *info)
{

    char *start = strchr(info, '#');

    if (start)
        start = strchr(start, 0x01);

    char *end = 0;
    if (start) {
        start++;
        end = strchr(info, '\0');
    }

    if (start != end && end) {
        return g_strdup_printf("%.*s", end - start, start);
    } else {
        return 0;
    }
}


void gaym_update_channel_member(struct gaym_conn *gaym, const char *nick,
                                const char *info)
{
    GaymChannelMember *cm = gaym_get_channel_member_reference(gaym, nick);
    if (!cm) {
        gaim_debug_error("gaym",
                         "ERROR: A member has joined a channel, or a conversation was opened, but we were unable to add the member to the internal management structure. Report a bug.");
        return;
    } else {
        gchar *stats = gaym_stats_strdup(info);
        if (stats) {
            gchar **s = g_strsplit(stats, "|", 3);
            if (s[0] && strlen(g_strstrip(s[0])) > 0) {
                cm->sex = g_ascii_strup(s[0], -1);
            }
            if (s[1] && strlen(g_strstrip(s[1])) > 0) {
                cm->age = g_strdup(s[1]);
            }
            if (s[2] && strlen(g_strstrip(s[2])) > 0) {
                cm->location = g_strdup(s[2]);
            }
            g_strfreev(s);
            g_free(stats);
        }
        cm->name = g_strdup(nick);
        cm->bio = gaym_bio_strdup(info);
        cm->thumbnail = gaym_thumbnail_strdup(info);

    }
}
void gaym_buddy_status(struct gaym_conn *gaym, char *name,
                       gboolean online, char *info)
{
    char *bio = NULL;
    char *thumbnail = NULL;
    char *stats = NULL;
    char *url = NULL;
    struct gaym_fetch_thumbnail_data *data;

    if (!gaym || !gaym->account || !gaym->buddies || !name) {
        return;
    }

    if (info) {
        bio = gaym_bio_strdup(info);
        if (bio) {
            bio = g_strstrip(bio);
        }

        thumbnail = gaym_thumbnail_strdup(info);
        if (thumbnail) {
            thumbnail = g_strstrip(thumbnail);
        }

        stats = gaym_stats_strdup(info);
        if (stats) {
            stats = g_strstrip(stats);
        }
    }

    GaimConnection *gc = gaim_account_get_connection(gaym->account);

    if (!gc) {
        return;
    }

    struct gaym_buddy *ib = g_hash_table_lookup(gaym->buddies, name);

    char *normalized = g_strdup(gaim_normalize(gaym->account, name));
    char *im_thumbnail =
        g_hash_table_lookup(gaym->im_thumbnail_needed, normalized);

    if (thumbnail) {
        if ((ib && gaim_utf8_strcasecmp(thumbnail, ib->thumbnail))
            || im_thumbnail) {

            char *hashurl = NULL;
            hashurl =
                g_hash_table_lookup(gaym->confighash,
                                    "mini-profile-panel.thumbnail-prefix");
            g_return_if_fail(hashurl != NULL);
            data = g_new0(struct gaym_fetch_thumbnail_data, 1);
            data->gc = gaim_account_get_connection(gaym->account);
            data->who = g_strdup(name);
            url = g_strdup_printf("%s%s", hashurl, thumbnail);
            gaim_url_fetch(url, FALSE, "Mozilla/4.0", FALSE,
                           gaym_fetch_thumbnail_cb, data);
            g_free(url);
        }
    }

    if (im_thumbnail) {
        g_hash_table_remove(gaym->im_thumbnail_needed, normalized);
    }
    g_free(normalized);

    if (ib) {
        g_free(ib->bio);
        ib->bio = NULL;
        g_free(ib->thumbnail);
        ib->thumbnail = NULL;
        g_free(ib->sex);
        ib->sex = NULL;
        g_free(ib->age);
        ib->age = NULL;
        g_free(ib->location);
        ib->location = NULL;

        ib->online = online;

        if (bio && strlen(g_strstrip(bio)) > 0) {
            ib->bio = bio;
        }
        if (thumbnail && strlen(g_strstrip(thumbnail)) > 0) {
            ib->thumbnail = thumbnail;
        }
        if (stats && strlen(g_strstrip(stats)) > 0) {
            gchar **s = g_strsplit(stats, "|", 3);
            if (s[0] && strlen(g_strstrip(s[0])) > 0) {
                ib->sex = g_ascii_strup(s[0], -1);
            }
            if (s[1] && strlen(g_strstrip(s[1])) > 0) {
                ib->age = g_strdup(s[1]);
            }
            if (s[2] && strlen(g_strstrip(s[2])) > 0) {
                ib->location = g_strdup(s[2]);
            }
            g_strfreev(s);
            g_free(stats);
        }
        GaimBuddy *buddy = gaim_find_buddy(gaym->account, name);
        if (buddy) {
            serv_got_update(gc, buddy->name, online, 0, 0, 0, 0);
        }
    }
    return;
}

/**
 * vim:tabstop=4:shiftwidth=4:expandtab:
 */
