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

/**
 * vim:tabstop=4:shiftwidth=4:expandtab:
 */
