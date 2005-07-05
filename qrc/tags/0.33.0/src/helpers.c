#include "helpers.h"

void gaym_convert_nick_to_gaycom(char *name)
{
    int i;
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

    start = strstr(source, startbit);

    if (start) {
        start += strlen(startbit);
        end = strstr(start, endbit);
    }
    // gaim_debug_misc("gaym", "source: %d; start: %d; end: %d\n", source,
    // start, end);
    if (start && end) {
        return g_strdup_printf("%.*s", end - start, start);
    } else {
        return 0;
    }
}

char *convert_nick_to_gc(char *nick)
{
    int i;
    char *out = g_strdup(nick);
    for (i = 0; i < strlen(out); i++) {
        if (out[i] == '.') {
            out[i] = '|';
        }
    }
    // gaim_debug_misc("gaym", "Converted %s to %s\n", nick, out);
    return out;
}

void convert_nick_from_gaycom(char *name)
{
    int i;
    if (!name) {
        return;
    }
    for (i = 0; i < strlen(name); i++) {
        if (name[i] == '|') {
            name[i] = '.';
        }
    }
}

gchar *ascii2native(const gchar * str)
{
    gint i;                     // Temp variable
    gint len = strlen(str);

    // This allocates enough space, and probably a little too much.
    gchar *outstr = g_malloc(len * sizeof(guchar));

    gint pos = 0;               // Current position in the output string.
    for (i = 0; i < len; i++) {
        // If we see a '\u' then parse it.
        if (((char) *(str + i) == '\\')
            && (((char) *(str + i + 1)) == 'u')
            && (g_ascii_isxdigit((char) *(str + i + 2)))
            && (g_ascii_isxdigit((char) *(str + i + 3)))
            && (g_ascii_isxdigit((char) *(str + i + 4)))
            && (g_ascii_isxdigit((char) *(str + i + 5)))) {

            gint bytes;
            gchar unibuf[6];
            gunichar value = 0;

            // Assumption that the /u will be followed by 4 hex digits.
            // <<12 to multiply by 16^3, <<8 for 16^2, <<4 for 16
            // I.e., each hex digit
            value = (g_ascii_xdigit_value((gchar) * (str + i + 2)) << 12) +
                (g_ascii_xdigit_value((gchar) * (str + i + 3)) << 8) +
                (g_ascii_xdigit_value((gchar) * (str + i + 4)) << 4) +
                (g_ascii_xdigit_value((gchar) * (str + i + 5)));

            bytes = g_unichar_to_utf8(value, unibuf);
            int j;
            for (j = 0; j < bytes; j++) {
                outstr[pos++] = unibuf[j];
            }
            // Move past the entire escape sequence.
            i += 5;
        }
        // Otherwise, just copy the byte.
        else {
            outstr[pos++] = str[i];
        }
    }
    return outstr;
}

// vim:tabstop=4:shiftwidth=4:expandtab:
