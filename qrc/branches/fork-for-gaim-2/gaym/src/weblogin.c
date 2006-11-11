/* 
 * @file util.h Utility Functions
 * @ingroup core
 *
 * Gaim is the legal property of its developers, whose names are too numerous
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

#include "conversation.h"
#include "debug.h"
#include "prpl.h"
#include "prefs.h"
#include "util.h"

#include "gaym.h"

#include "helpers.h"


static void gaym_session_destroy(GaimUrlSession * session)
{
    if (session->cookies)
        g_free(session->cookies);
    if (session->username)
        g_free(session->username);
    if (session->password)
        g_free(session->password);
    g_free(session);
}

/* gaim_url_decode doesn't change pluses to spaces - edit in place */
static const char *gaym_url_decode(const char *string)
{
    char *retval;

    retval = (char *) (string = gaim_url_decode(string));
    while (*retval != 0) {
        if (*retval == '+')
            *retval = ' ';
        retval++;
    }
    return string;
}



// This looks for Set-cookie: fields in headers, and adds those cookies
// To the session struct.
static void add_cookie(gpointer key, gpointer value, gpointer data)
{
    g_return_if_fail(key != NULL);
    g_return_if_fail(value != NULL);
    g_return_if_fail(data != NULL);
    GaimUrlSession *session = (GaimUrlSession *) data;
    gchar *cookies = session->cookies;
    session->cookies =
        g_strconcat(cookies ? cookies : "", key, "=", value, "; ", NULL);
    g_free(cookies);

}

static void parse_cookies(const char *webdata, GaimUrlSession * session,
                          size_t len)
{
    gchar **cookies = g_strsplit(webdata, "\n", -1);
    gchar **cookie_parts;
    char *cookie;
    int index = 0;
    while (cookies[index]) {
        cookie =
            return_string_between("Set-cookie: ", "; ", cookies[index]);
        if (cookie) {
            cookie_parts = g_strsplit(cookie, "=", 2);
            if (cookie_parts[0] && cookie_parts[1])
                g_hash_table_replace(session->cookie_table,
                                     cookie_parts[0], cookie_parts[1]);
        }
        g_free(cookie);
        index++;
    }
    g_strfreev(cookies);
    g_hash_table_foreach(session->cookie_table, (GHFunc) add_cookie,
                         session);

}

gchar* gaym_build_session_request(gchar* url, GaimUrlSession* session)
{
	    if(!url || !session)
		return 0;

	    gchar* buf = g_strdup_printf(   "GET %s HTTP/1.0\r\n" 
					    "Accept-Charset: utf-8\r\n"
					    "User-Agent: Mozilla/4.0\r\n"
					    "Accept-Encoding: identity\r\n"
					    "Host: www.gay.com\r\n" 
					    "Cookie: %s\r\n"
					    "Connction: Close\r\n",
                       url, session->cookies);
	gchar* res; 
	if (session->hasFormData)
            res=g_strdup_printf("%sContent-Type: application/x-www-form-urlencoded\r\n\r\n",buf);
	else
	    res=g_strdup_printf("%s\r\n",buf);
	g_free(buf);
	return res;
}
static void
gaym_weblogin_step5(GaimUtilFetchUrlData *url_data, gpointer data, const gchar *text, gsize len, const gchar* err)
{
    gaim_debug_misc("weblogin","STEP FIVE BEGINS\n"); 
    GaimUrlSession *session = (GaimUrlSession *) data;
    struct gaym_conn *gaym = session->gaym;
    // Get hash from text
    if (session && GAIM_CONNECTION_IS_VALID(session->account->gc)) {
        char *bio;
        char *thumbnail;
        char *temp = NULL;
        char *temp2 = NULL;
        const char *match;
        const char *result;



        gaym->server_stats = NULL;
        gaym->chat_key = NULL;
        gaym->server_bioline = NULL;
        gaym->thumbnail = NULL;

        // First, look for password
        match = "password\" value=\"";
        temp = strstr(text, match);
        if (temp) {
            temp += strlen(match);
            temp2 = strstr(temp, "\" ");
        }
        if (!
            (temp && temp2 && temp != temp2
             && (gaym->chat_key =
                 g_strndup(temp, (temp2 - temp) * sizeof(char))))) {
            gaim_connection_error((session->account->gc),
                                  _
                                  ("Problem parsing password from web. Report a bug."));
            return;
        }

        gaim_debug_misc("weblogin",
                        "Got hash, temp=%x, temp2=%x, gaym->chat_key=%x\n",
                        temp, temp2, gaym->chat_key);
        // Next, loook for bio
        match = "param name=\"bio\" value=\"";
        temp = strstr(text, match);
        if (temp) {
            temp += strlen(match);
            temp2 = strstr(temp, "%23");
        }
        if (temp && temp2) {
            thumbnail = g_strndup(temp, (temp2 - temp) * sizeof(char));
            result = gaym_url_decode(thumbnail);
            (gaym->thumbnail = g_strdup(result))
                || (gaym->thumbnail = g_strdup(" "));

            g_free(thumbnail);
            // Parse out non thumbnail part of bio.
            temp = strstr(temp2, "\"");
            if (temp) {
                bio = g_strndup(temp2, (temp - temp2) * sizeof(char));
                result = gaym_url_decode(bio);
                gaim_debug_info("gaym", "Server BIO: %s Thumb: %s\n",
                                result, gaym->thumbnail);
                (gaym->server_bioline = g_strdup(result))
                    || (gaym->server_bioline = NULL);
                g_free(bio);

                // Parse out stats part of bio.
                temp2 = strchr(result, (char) 0x01);
                if (temp2++) {
                    gaim_debug_misc("gaym", "Stats: %s\n", temp2);
                    gaym->server_stats = g_strdup(temp2);
                }
            }
        } else {
            // gaim_connection_error(
            // gaim_account_get_connection(((struct
            // gaym_conn*)((GaimUrlSession*)session)->account),
            // _("Problem parsing password from web. Report a bug.")));
        }
        session->session_cb(gaym->account);

    } else {
        gaim_debug_misc("gaym", "Connection was cancelled before step5\n");
        gaim_debug_misc("gaym", "gaym->session: %x\n", session);
    }

    // We don't need the session info anymore.
    gaym_session_destroy(session);

}

static void
gaym_weblogin_step4(GaimUtilFetchUrlData *url_data, gpointer data, const gchar *text, gsize len, const gchar* err)
{

    GaimUrlSession *session = (GaimUrlSession *) data;
    parse_cookies(text, session, len);
    gaim_debug_misc("gaym", "Step 4: session: %x\n", session);
    if (session && GAIM_CONNECTION_IS_VALID(session->account->gc)) {
        // The fourth step is to parse a rand=# value out of the message
        // text from
        // The previous step.
        // We then connect to messenger/applet.html
        char url[512];
        int nonce;
        //char *buf = g_strdup_printf(_("Signon: %s"),
        //                            (session->account->username));
        //gaim_connection_update_progress(session->account->gc, buf, 3, 6);
        sscanf(text, "?rand=%d", &nonce);
        snprintf(url, 512,
                 "http://www.gay.com/messenger/applet.html?rand=%d",
                 nonce);
	
        session->hasFormData = TRUE;
	gaim_debug_misc("weblogin","About to build url\n");
	gchar* request=gaym_build_session_request(url, session);
	gaim_debug_misc("weblogin","Requesting: %s\n",request);
        gaim_util_fetch_url_request(url, FALSE, NULL, TRUE, 
				    request, TRUE, gaym_weblogin_step5,
				    session);
	gaim_debug_misc("weblogin","applet fetched");
    } else {
        gaim_debug_misc("gaym", "Connection was cancelled before step4\n");
        gaim_debug_misc("gaym", "session: %x\n", session);
        gaym_session_destroy(session);

        // g_free(gaym->session);
    }
}

void
gaym_get_chat_key_from_weblogin(GaimAccount * account,
                                void (*callback) (GaimAccount * account))
{

    struct gaym_conn *gaym = account->gc->proto_data;
    if (GAIM_CONNECTION_IS_VALID(account->gc)) {

        GaimUrlSession *session = g_new0(GaimUrlSession, 1);
        session->session_cb = callback;
        session->cookies = NULL;
        session->account = account;
        session->username = g_strdup(account->username);
        session->password = g_strdup(account->password);
        session->gaym = gaym;
        session->cookie_table =
            g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

        gaim_debug_misc("gaym", "Made session: %x\n", session);
        if (GAIM_CONNECTION_IS_VALID
            (((GaimUrlSession *) session)->account->gc)) {
            // The first step is to establish the initial sesion
            // We connect to index.html, and get a few cookie values.
            char *url =
                g_strdup_printf
                ("http://www.gay.com/misc/dologin.html?__login_haveForm=1&__login_save=1&__login_member=%s&redir=%%2Findex.html&__login_basepage=%%2Fmisc%%2Fdologin.html&__login_password=%s",
                 session->username, session->password);

            session->hasFormData = TRUE;
            gaim_util_fetch_url_request(url, FALSE, NULL, FALSE, NULL, TRUE,
                               gaym_weblogin_step4, session);
        } else {
            gaim_debug_misc("gaym", "cancelled before step1\n");
            gaim_debug_misc("gaym", "gaym->sessoin: %x\n", session);
            gaym_session_destroy(session);
        }

    }
}


/*Doesn't do anything yet*/
void gaym_try_cached_password(GaimAccount * account,
                              void (*callback) (GaimAccount * account))
{

    const char *pw;
    pw = gaim_account_get_string(account, "chat_key", NULL);
    if (pw == NULL) {
        gaym_get_chat_key_from_weblogin(account, callback);
        return;
    }
    // All in one shot:
    // 1. Login to the irc server <--- blocks serv_login
    // 2. grab the applet <--- blocks serv_login
    // 3. spamlist update <--- does not block serv_login
    // 4. get config.txt <--- does not block serv_login
    // 
    // Note: if the chat key happens to be invalid, 2 and 4 will fail.

    // 1
    // callback(gaym->account);

    // 2

    // 3


    // 4

}

/**
 * vim:tabstop=4:shiftwidth=4:expandtab:
 */
