#include "internal.h"

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

// shamelessly taken from the yahoo prpl
gboolean gaym_privacy_check(GaimConnection * gc, const char *nick)
{
    // returns TRUE if allowed through, FALSE otherwise
    GSList *list;
    gboolean permitted = FALSE;

    switch (gc->account->perm_deny) {
    case 0:
        gaim_debug_warning("gaym", "Privacy setting was 0.  If you can "
                           "reproduce this, please file a bug report.\n");
        permitted = TRUE;
        break;

    case GAIM_PRIVACY_ALLOW_ALL:
        permitted = TRUE;
        break;

    case GAIM_PRIVACY_DENY_ALL:
        gaim_debug_info("gaym",
                        "%s blocked data received from %s (GAIM_PRIVACY_DENY_ALL)\n",
                        gc->account->username, nick);
        break;

    case GAIM_PRIVACY_ALLOW_USERS:
        for (list = gc->account->permit; list != NULL; list = list->next) {
            if (!gaim_utf8_strcasecmp
                (nick, gaim_normalize(gc->account, (char *) list->data))) {
                permitted = TRUE;
                gaim_debug_info("gaym",
                                "%s allowed data received from %s (GAIM_PRIVACY_ALLOW_USERS)\n",
                                gc->account->username, nick);
                break;
            }
        }
        break;

    case GAIM_PRIVACY_DENY_USERS:
        /* seeing we're letting everyone through, except the deny list */
        permitted = TRUE;
        for (list = gc->account->deny; list != NULL; list = list->next) {
            if (!gaim_utf8_strcasecmp
                (nick, gaim_normalize(gc->account, (char *) list->data))) {
                permitted = FALSE;
                gaim_debug_info("gaym",
                                "%s blocked data received from %s (GAIM_PRIVACY_DENY_USERS)\n",
                                gc->account->username, nick);
                break;
            }
        }
        break;

    case GAIM_PRIVACY_ALLOW_BUDDYLIST:
        if (gaim_find_buddy(gc->account, nick) != NULL) {
            permitted = TRUE;
        } else {
            gaim_debug_info("gaym",
                            "%s blocked data received from %s (GAIM_PRIVACY_ALLOW_BUDDYLIST)\n",
                            gc->account->username, nick);
        }
        break;

    default:
        gaim_debug_warning("gaym",
                           "Privacy setting was unknown.  If you can "
                           "reproduce this, please file a bug report.\n");
        permitted = FALSE;
        break;
    }

    return permitted;
}

void gaym_privacy_change(GaimConnection * gc)
{
    GSList *rooms = NULL;
    for (rooms = gc->buddy_chats; rooms; rooms = rooms->next) {
        GaimConversation *convo = rooms->data;
        GaimConvChat *chat = gaim_conversation_get_chat_data(convo);
        GList *people = NULL;
        for (people = chat->in_room; people; people = people->next) {
            GaimConvChatBuddy *buddy = people->data;
            GaimConversationUiOps *ops =
                gaim_conversation_get_ui_ops(convo);
            if (gaym_privacy_check(gc, buddy->name)) {
                gaim_conv_chat_unignore(GAIM_CONV_CHAT(convo),
                                        buddy->name);
                ops->chat_update_user((convo), buddy->name);
            } else {
                gaim_conv_chat_ignore(GAIM_CONV_CHAT(convo), buddy->name);
                ops->chat_update_user((convo), buddy->name);
            }
        }
    }
}

// vim:tabstop=4:shiftwidth=4:expandtab:
