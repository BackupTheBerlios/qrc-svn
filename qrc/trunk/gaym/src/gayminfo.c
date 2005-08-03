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

// #define GAYM_TOKEN 1

#ifdef GAYM_TOKEN
gboolean gaym_stats_find_gaym_token(const char *info)
{
    gaim_debug_misc("token", "checking for token in %s\n", info);
    return (gboolean) g_strrstr(info, "\xC2\xA0 \xC2\xA0");
}
#endif
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
    if (start && *start) {
        start++;
        end = strchr(start, 0x01);
        if (!end)
            end = strchr(start, 0);
    }
#ifdef GAYM_TOKEN
    gaim_debug_misc("gaym", "end: %x, end-1: %x, end-5: %x\n", end,
                    *(end - 1), *(end - 5));
    if (end - 5 >= start)
        if (!strncmp((end - 5), "\xC2\xA0 \xC2\xA0", 5))
            end -= 5;
#endif
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
    GaymBuddy *cm = gaym_get_channel_member_reference(gaym, nick);
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
#ifdef GAYM_TOKEN
        cm->gaymuser = gaym_stats_find_gaym_token(info);
#endif
        cm->thumbnail = gaym_thumbnail_strdup(info);

    }
}
void gaym_fetch_thumbnail_cb(void *user_data, const char *pic_data,
                             size_t len)
{
    if (!user_data)
        return;
    struct gaym_fetch_thumbnail_data *d = user_data;
    if (!pic_data) {
        return;
    }

    if (len && !g_strrstr_len(pic_data, len, "Server Error")) {
        char *dir =
            g_build_filename(gaim_user_dir(), "icons", "gaym", d->who,
                             NULL);
        char *filename = g_strdup(d->filename);
        char *path = g_build_filename(dir, filename, NULL);
        gaim_debug_misc("gayminfo", "dir: %s\n", dir);
        gaim_debug_misc("gayminfo", "filename: %s\n", filename);
        gaim_debug_misc("gayminfo", "path: %s\n", path);
        if (!g_file_test(dir, G_FILE_TEST_EXISTS))
            gaim_build_dir(dir, S_IRUSR | S_IWUSR | S_IXUSR);

        if (path && !g_file_test(path, G_FILE_TEST_EXISTS)) {
            FILE *file;
            if ((file = g_fopen(path, "wb"))) {
                fwrite(pic_data, 1, len, file);
                fclose(file);
            } else {
                gaim_debug_misc("fetch_thumbnail_cb",
                                "Couldn't write file\n");
            }
            g_free(filename);
            g_free(path);
            g_free(dir);
        }
    }
    if (GAIM_CONNECTION_IS_VALID(d->gc) && len) {
        gaim_signal_emit(gaim_accounts_get_handle(), "info-updated",
                         d->gc, d->who);
        if (gaim_find_conversation_with_account(d->who, d->gc->account)) {
            // gaim_buddy_icons_set_for_user(gaim_connection_get_account
            // (d->gc), d->who,
            // (void *) pic_data, len);
        }

    } else {
        gaim_debug_error("gaym", "Fetching buddy icon failed.\n");
    }

    g_free(d->who);
    g_free(d);
}

void gaym_buddy_status(struct gaym_conn *gaym, char *name,
                       gboolean online, char *info,
                       gboolean fetch_thumbnail)
{
    char *bio = NULL;
    char *thumbnail = NULL;
    char *stats = NULL;
    char *url = NULL;
    struct gaym_fetch_thumbnail_data *data;
    gboolean gaymuser = FALSE;

    if (!gaym || !gaym->account || !gaym->buddies || !name) {
        return;
    }

    if (info) {
#ifdef GAYM_TOKEN
        gaymuser = gaym_stats_find_gaym_token(info);
#endif
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
    if (!ib)
        ib = g_hash_table_lookup(gaym->channel_members, name);

    char *normalized = g_strdup(gaim_normalize(gaym->account, name));

    if (thumbnail && fetch_thumbnail) {
        gboolean do_fetch = 1;
        GError *err = NULL;
        if (!ib || gaim_utf8_strcasecmp(thumbnail, ib->thumbnail)) {
            char *dirname =
                g_build_filename(gaim_user_dir(), "icons", "gaym",
                                 gaim_normalize(gaym->account, name),
                                 NULL);
            GDir *gdir = g_dir_open(dirname, 0, &err);
            if (gdir) {
                const char *filename;

                while ((filename = g_dir_read_name(gdir))) {    /* don't
                                                                   free
                                                                   filename: 
                                                                   owned
                                                                   by
                                                                   glib. */
                    char *thumbnail_base = g_path_get_basename(thumbnail);
                    gaim_debug_misc("gaym", "compared %s and %s\n",
                                    thumbnail_base, filename);
                    if (!gaim_utf8_strcasecmp(thumbnail_base, filename)) {
                        do_fetch = 0;
                        break;
                    }
                    g_free(thumbnail_base);
                }
                g_dir_close(gdir);
            }
            if (do_fetch) {
                char *hashurl = NULL;
                hashurl =
                    g_hash_table_lookup(gaym->confighash,
                                        "mini-profile-panel.thumbnail-prefix");
                g_return_if_fail(hashurl != NULL);
                data = g_new0(struct gaym_fetch_thumbnail_data, 1);
                data->gc = gaim_account_get_connection(gaym->account);
                data->who = g_strdup(gaim_normalize(gaym->account, name));
                data->filename = g_strdup(g_strrstr(thumbnail, "/"));
                gaim_debug_misc("gayminfo", "Found filename: %s\n",
                                data->filename);
                url = g_strdup_printf("%s%s", hashurl, thumbnail);
                gaim_url_fetch(url, FALSE, "Mozilla/4.0", FALSE,
                               gaym_fetch_thumbnail_cb, data);
                g_free(url);
            }

        }
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
        ib->gaymuser = gaymuser;
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
