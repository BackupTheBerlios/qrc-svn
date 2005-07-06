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

void gaym_convert_nick_to_gaycom(char *name)
{
    int i;
    
    //If the first character is a punctuation or a number,
    //Gay.com inserts a | character in front. We remedy this by changing 
    //it to a space. This changes it back.
    if (name[0]==' ')
	name[0]='|';
    
    if (!name) {
        return;
    }
    for (i = 0; i < strlen(name); i++) {
        if (name[i] == '.') {
            name[i] = '|';
        }
    }
}

char *return_string_between(const char *startbit, const char *endbit,
                            const char *source)
{
    char *start = 0;
    char *end = 0;

    if(!source || !startbit || !endbit)
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

/*
char *convert_nick_to_gc(char *nick)
{
    int i;
    char *out = g_strdup(nick);
    for (i = 0; i < strlen(out); i++) {
        if (out[i] == '.') {
            out[i] = '|';
        }
    }
    return out;
}
*/
void convert_nick_from_gaycom(char *name)
{
    int i;

    if (!name) {
        return;
    }

    //If the first character is a punctuation or a number,
    //Gay.com inserts a | character in front. Get rid of it.
    if (name[0]=='|')
	name[0]=' ';

    for (i = 0; i < strlen(name); i++) {
        if (name[i] == '|') {
            name[i] = '.';
        }
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

    char *firstchar =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz\0";
    char *allowed =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_.-\0";
    int i = 0;
    int j = 0;

    /**
     * first character validation is different from the remaining
     * characters
     */
    for (i = 0; firstchar[i]; i++) {
        if (nick[0] == firstchar[i]) {
            retval = TRUE;
            break;
        }
    }
    if (!retval) {
        return retval;
    }
    /**
     * validate remaining characters (but not the first character)
     */
    for (i = 1; nick[i]; i++) {
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
