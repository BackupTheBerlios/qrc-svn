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
#include "internal.h"
#include "debug.h"
#include "helpers.h"

void gcom_nick_to_gaym(char *nick)
{
    int i = 0;

    if (!nick) {
        return;
    }

    /**
     * If there is a "|" in the first position, it must be removed.
     */
    if (nick[0] == '|') {
        nick[0] = ' ';
        nick = g_strchug(nick);
    }

    /**
     * Any remaining "|" must be replaced with "."
     */
    for (i = 0; i < strlen(nick); i++) {
        if (nick[i] == '|') {
            nick[i] = '.';
        }
    }
    return;
}

char *gaym_nick_to_gcom_strdup(const char *nick)
{
    int i = 0;
    char *converted = NULL;

    /**
     * If the first character is not an upper or lower case letter
     * then gay.com's IRC server requires "|" to be prepended
     */
    if (g_ascii_isalpha(nick[0])) {
        converted = g_strdup_printf("%s", nick);
    } else {
        converted = g_strdup_printf("|%s", nick);
    }

    /**
     * gay.com's IRC server requires all "." in nicks to be represented
     * by "|"
     */
    for (i = 0; i < strlen(converted); i++) {
        if (converted[i] == '.') {
            converted[i] = '|';
        }
    }
    return converted;
}

char *return_string_between(const char *startbit, const char *endbit,
                            const char *source)
{
    char *start = 0;
    char *end = 0;

    if (!source || !startbit || !endbit)
        return 0;

    start = strstr(source, startbit);

    if (start) {
        start += strlen(startbit);
        end = strstr(start, endbit);
    }
    /**
     * gaim_debug_misc("gaym", "source: %d; start: %d; end: %d\n", source,
     * start, end);
     */
    if (start && end) {
        return g_strdup_printf("%.*s", end - start, start);
    } else {
        return 0;
    }
}

gchar *ascii2native(const gchar * str)
{
    gint i;                     /* Temp variable */
    gint len = strlen(str);

    /**
     * This allocates enough space, and probably a little too much.
     */
    gchar *outstr = g_malloc(len * sizeof(guchar));

    gint pos = 0;               /* Current position in the output string. */
    for (i = 0; i < len; i++) {
        /**
         * If we see a '\u' then parse it.
         */
        if (((char) *(str + i) == '\\')
            && (((char) *(str + i + 1)) == 'u')
            && (g_ascii_isxdigit((char) *(str + i + 2)))
            && (g_ascii_isxdigit((char) *(str + i + 3)))
            && (g_ascii_isxdigit((char) *(str + i + 4)))
            && (g_ascii_isxdigit((char) *(str + i + 5)))) {

            gint bytes;
            gchar unibuf[6];
            gunichar value = 0;

            /**
             * Assumption that the /u will be followed by 4 hex digits.
             * <<12 to multiply by 16^3, <<8 for 16^2, <<4 for 16
             * I.e., each hex digit
             */
            value = (g_ascii_xdigit_value((gchar) * (str + i + 2)) << 12) +
                (g_ascii_xdigit_value((gchar) * (str + i + 3)) << 8) +
                (g_ascii_xdigit_value((gchar) * (str + i + 4)) << 4) +
                (g_ascii_xdigit_value((gchar) * (str + i + 5)));

            bytes = g_unichar_to_utf8(value, unibuf);
            int j;
            for (j = 0; j < bytes; j++) {
                outstr[pos++] = unibuf[j];
            }
            /**
             * Move past the entire escape sequence.
             */
            i += 5;
        }
        /**
         * Otherwise, just copy the byte.
         */
        else {
            outstr[pos++] = str[i];
        }
    }
    return outstr;
}

gboolean gaym_nick_check(const char *nick)
{
    gboolean retval = FALSE;

    if (!nick) {
        return retval;
    }

    char *allowed =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_.-\0";
    int i = 0;
    int j = 0;

    /**
     * validate characters
     */
    for (i = 0; nick[i]; i++) {
        retval = FALSE;
        for (j = 0; allowed[j]; j++) {
            if (nick[i] == allowed[j]) {
                retval = TRUE;
                break;
            }
        }
        if (!retval) {
            break;
        }
    }
    if (!retval) {
        return retval;
    }
    /**
     * validate length
     */
    if (i > 30) {
        retval = FALSE;
        return retval;
    }
    /**
     * its valid!
     */
    return retval;
}

void replace_dollar_n(gpointer key, gpointer value, gpointer user_data)
{

    /**
     * replace $[0-9] with %s, so we can use printf style
     * processing with the provided property values
     */
    gchar *pos = (gchar *) value;
    while ((pos = (strchr(pos, '$')))) {
        pos++;
        if (g_ascii_isdigit(*pos)) {
            *pos = 's';
            *(pos - 1) = '%';

        }
    }
}

GHashTable *gaym_properties_new(const gchar * str)
{

    gchar *tmpstr = NULL;
    gchar **tmparr = NULL;
    gchar **proparr = NULL;
    int i = 0;

    GHashTable *props =
        g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

    /**
     * convert ascii-escaped to native
     */
    tmpstr = ascii2native(str);

    /**
     * strip out continuation character followed by newline 
     */
    // tmparr = g_strsplit(tmpstr, "\\\n", -1);
    // g_free(tmpstr);
    // tmpstr = g_strjoinv(NULL, tmparr);
    // g_strfreev(tmparr);
    /**
     * Since the properties get stripped of spaces later,
     * just replace \\\n with <space>\n in-place, for speed.
     * */
    char *pos = tmpstr;
    while ((pos = g_strrstr(pos, "\\\n"))) {
        *pos = ' ';
        *(pos + 1) = ' ';
    }
    /**
     * We're getting close.  Now we need an array as follows:
     *
     * property=value
     * property=value
     * ...
     */
    tmparr = g_strsplit(tmpstr, "\n", -1);

    for (i = 0; tmparr[i] != NULL; i++) {
        /**
         * do nothing if this is a blank line
         */
        if (strlen(g_strstrip(tmparr[i])) == 0) {
            continue;
        }
        /**
         * do nothing if this is a comment line
         */
        if (tmparr[i][0] == '#') {
            continue;
        }
        /**
         * this must be a property=value string, so we make
         * it into a 2-element array:
         *
         * property
         * value
         *
         * but we won't store it in our hash table unless both
         * have real values after stripping whitespace
         */
        proparr = g_strsplit(tmparr[i], "=", 2);
        if (proparr[0] && strlen(g_strstrip(proparr[0])) > 0
            && proparr[1] && strlen(g_strstrip(proparr[1])) > 0) {

            g_hash_table_insert(props, g_strdup(proparr[0]),
                                g_strdup(proparr[1]));

        }
        g_strfreev(proparr);
    }

    g_strfreev(tmparr);

    g_hash_table_foreach(props, replace_dollar_n, NULL);

    return props;
}

int roomlist_level_strip(char *description)
{
    int val = 0;
    int i = 0;

    if (!description) {
        return val;
    }

    for (i = 0; i < strlen(description); i++) {
        if (description[i] == '+') {
            description[i] = ' ';
        } else {
            break;
        }
        val++;
    }

    description = g_strchug(description);

    return val;
}

GaimRoomlistRoom *find_parent(int level, int old_level,
                              GaimRoomlistRoom * last_room)
{
    GaimRoomlistRoom *parent = NULL;
    int i = 0;

    if (level == 0) {
        /* do nothing */
    } else if (level == old_level) {
        parent = last_room->parent;
    } else if (level > old_level) {
        parent = last_room;
    } else if (level < old_level) {
        parent = last_room;
        for (i = old_level - level; i >= 0; i--) {
            parent = parent->parent;
        }
    }
    return parent;
}

void build_roomlist_from_config(GaimRoomlist * roomlist,
                                GHashTable * confighash, gchar * pattern)
{
    gchar **roominst = NULL;
    gchar *altname = NULL;
    gchar *normalized = NULL;
    gchar *lowercase = NULL;
    gchar *found = NULL;
    int level = 0;
    int old_level = 0;
    int i = 0;
    GaimRoomlistRoom *room = NULL;
    GaimRoomlistRoom *parent = NULL;

    g_return_if_fail(roomlist != NULL);
    g_return_if_fail(confighash != NULL);

    int max = gaim_prefs_get_int("/plugins/prpl/gaym/chat_room_instances");

    gchar *roomstr = g_hash_table_lookup(confighash, "roomlist");
    g_return_if_fail(roomstr != NULL);

    gchar **roomarr = g_strsplit(roomstr, "|", -1);

    /**
     * We need to skip the first instance, because they start
     * with a "|", which we've just split by, leaving a blank
     * at the beginning of the list
     */
    for (i = 1; roomarr[i] != NULL; i++) {
        if (roomarr[i][0] == '#') {
            /**
             * This is an actual room string, break it into its
             * component parts, determine the level and the parent,
             * and add this as both a room and a category
             */
            if (pattern != NULL) {
                lowercase = g_utf8_strdown(roomarr[i], -1);
                normalized =
                    g_utf8_normalize(lowercase, -1, G_NORMALIZE_ALL);
                found = g_strstr_len(normalized, -1, pattern);
                g_free(normalized);
                g_free(lowercase);
            }
            if (found != NULL || pattern == NULL) {
                found = NULL;
                roominst = g_strsplit(roomarr[i], " ", 2);
                level = roomlist_level_strip(roominst[1]);
                parent = find_parent(level, old_level, room);
                altname = g_strdup_printf("%s:*", roominst[1]);
                if (max == 0) {
                    room =
                        gaim_roomlist_room_new(GAIM_ROOMLIST_ROOMTYPE_ROOM,
                                               altname, parent);
                } else {
                    room =
                        gaim_roomlist_room_new
                        (GAIM_ROOMLIST_ROOMTYPE_CATEGORY |
                         GAIM_ROOMLIST_ROOMTYPE_ROOM, altname, parent);
                }
                gaim_roomlist_room_add_field(roomlist, room, altname);
                gaim_roomlist_room_add_field(roomlist, room, roominst[0]);
                gaim_roomlist_room_add(roomlist, room);
                g_free(altname);
                g_strfreev(roominst);
                old_level = level;
            }
        } else if (pattern == NULL) {
            /**
             * This is a plain category, determine the level and
             * the parent and add it.
             */
            level = roomlist_level_strip(roomarr[i]);
            parent = find_parent(level, old_level, room);
            room =
                gaim_roomlist_room_new(GAIM_ROOMLIST_ROOMTYPE_CATEGORY,
                                       roomarr[i], parent);
            gaim_roomlist_room_add(roomlist, room);
        }
        old_level = level;
    }
    g_strfreev(roomarr);
    gaim_roomlist_set_in_progress(roomlist, FALSE);
}

GaimConvChatBuddyFlags chat_pecking_order(const char *extra)
{
    GaimConvChatBuddyFlags flags = GAIM_CBFLAGS_NONE;
    if (extra[0] == '1' && extra[1] == '8') {
        /* profile and g-rated photo */
        flags = GAIM_CBFLAGS_FOUNDER;
    } else if (extra[0] == '1' && extra[1] == '9') {
        /* profile and x-rated photo */
        flags = GAIM_CBFLAGS_OP;
    } else if (extra[0] == '8') {
        /* profile but no photo */
        flags = GAIM_CBFLAGS_HALFOP;
    } else if (extra[0] == '0') {
        /* no profile and no photo */
        flags = GAIM_CBFLAGS_NONE;
    } else {
        /* unknown */
        flags = GAIM_CBFLAGS_VOICE;
    }
    return flags;
}

char *build_tooltip_text(struct gaym_buddy *ib)
{
    if(!ib->name)
	return g_strdup("No info found.");
    char *escaped;
    GString *tooltip = g_string_new("");
    
    g_string_printf(tooltip, "<b><i>%s</i></b>", ib->name);

    g_return_val_if_fail(ib != NULL, NULL);

    if (ib->sex) {
        escaped = g_markup_escape_text(ib->sex, strlen(ib->sex));
        g_string_append_printf(tooltip, _("\n<b>%s:</b> %s"), _("Sex"),
                               escaped);
        g_free(escaped);
    }

    if (ib->age) {
        escaped = g_markup_escape_text(ib->age, strlen(ib->age));
        g_string_append_printf(tooltip, _("\n<b>%s:</b> %s"), _("Age"),
                               escaped);
        g_free(escaped);
    }
    if (ib->location) {
        escaped = g_markup_escape_text(ib->location, strlen(ib->location));
        g_string_append_printf(tooltip, _("\n<b>%s:</b> %s"),
                               _("Location"), escaped);
        g_free(escaped);
    }

    if (ib->bio) {
        escaped = g_markup_escape_text(ib->bio, strlen(ib->bio));
        g_string_append_printf(tooltip, _("\n<b>%s:</b> %s"), _("Bio"),
                               escaped);
        g_free(escaped);
    }

    if (tooltip->len == 0) {
        g_string_append_printf(tooltip, _(" No info."));
    }
    // g_string_erase(tooltip, 0, 1);

    return g_string_free(tooltip, FALSE);
}

GaimConvChatBuddyFlags include_chat_entry_order(GaimConvChatBuddyFlags
                                                flags, gint entry)
{

    return (flags | (entry << 4));
}

/**
 * vim:tabstop=4:shiftwidth=4:expandtab:
 */
