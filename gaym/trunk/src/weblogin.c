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
#include "internal.h"

#include "conversation.h"
#include "debug.h"
#include "prpl.h"
#include "prefs.h"
#include "util.h"

#include "gaym.h"


typedef struct {
    void (*callback) (void *, const char *, size_t);
    void *user_data;

    struct {
        char *user;
        char *passwd;
        char *address;
        int port;
        char *page;

    } website;

    char *url;
    gboolean full;
    char *user_agent;
    gboolean http11;

    int inpa;

    gboolean sentreq;
    gboolean newline;
    gboolean startsaving;
    gboolean has_explicit_data_len;
    char *webdata;
    unsigned long len;
    unsigned long data_len;
    GaimUrlSession *session;

} GaimFetchUrlData;

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
    gaim_debug_info("gaym", "Changed %s with gaym_url_decode\n", string);
    return string;
}

static void destroy_fetch_url_data(GaimFetchUrlData * gfud)
{
    if (gfud->webdata != NULL)
        g_free(gfud->webdata);
    if (gfud->url != NULL)
        g_free(gfud->url);
    if (gfud->user_agent != NULL)
        g_free(gfud->user_agent);
    if (gfud->website.address != NULL)
        g_free(gfud->website.address);
    if (gfud->website.page != NULL)
        g_free(gfud->website.page);
    if (gfud->website.user != NULL)
        g_free(gfud->website.user);
    if (gfud->website.passwd != NULL)
        g_free(gfud->website.passwd);

    g_free(gfud);
}

static gboolean
parse_redirect(const char *data, size_t data_len, gint sock,
               GaimFetchUrlData * gfud)
{
    gchar *s;

    if ((s = g_strstr_len(data, data_len, "Location: ")) != NULL) {
        gchar *new_url, *temp_url, *end;
        gboolean full;
        int len;

        s += strlen("Location: ");
        end = strchr(s, '\r');

        /* Just in case :) */
        if (end == NULL)
            end = strchr(s, '\n');

        len = end - s;

        new_url = g_malloc(len + 1);
        strncpy(new_url, s, len);
        new_url[len] = '\0';

        full = gfud->full;

        if (*new_url == '/' || g_strstr_len(new_url, len, "://") == NULL) {
            temp_url = new_url;

            new_url = g_strdup_printf("%s:%d%s", gfud->website.address,
                                      gfud->website.port, temp_url);

            g_free(temp_url);

            full = FALSE;
        }

        /* Close the existing stuff. */
        gaim_input_remove(gfud->inpa);
        close(sock);

        gaim_debug_info("gaim_url_fetch", "Redirecting to %s\n", new_url);

        /* Try again, with this new location. */
        gaim_url_fetch(new_url, full, gfud->user_agent, gfud->http11,
                       gfud->callback, gfud->user_data);

        /* Free up. */
        g_free(new_url);
        destroy_fetch_url_data(gfud);

        return TRUE;
    }

    return FALSE;
}

static size_t parse_content_len(const char *data, size_t data_len)
{
    size_t content_len = 0;
    const char *p = NULL;

    /* This is still technically wrong, since headers are case-insensitive
       [RFC 2616, section 4.2], though this ought to catch the normal case.
       Note: data is _not_ nul-terminated. */
    if (data_len > 16) {
        p = strncmp(data, "Content-Length: ", 16) == 0 ? data : NULL;
        if (!p) {
            p = g_strstr_len(data, data_len, "\nContent-Length: ");
            if (p)
                p += 1;
        }
    }

    /* If we can find a Content-Length header at all, try to sscanf it.
       Response headers should end with at least \r\n, so sscanf is safe,
       if we make sure that there is indeed a \n in our header. */
    if (p && g_strstr_len(p, data_len - (p - data), "\n")) {
        sscanf(p, "Content-Length: %zu", &content_len);
        gaim_debug_misc("parse_content_len", "parsed %u\n", content_len);
    }

    return content_len;
}

// This looks for Set-cookie: fields in headers, and adds those cookies
// To the session struct.

// TODO If a cookie value is re-set, then it will be duplicated, since
// Set-Cookie
// field is just appended to already existing cookies. This is not an
// issue for
// the current gay.com implementation, but this should be changed, so that
// receiving a duplicate cookie *updates* the existing value.
// JBL 10-16-2004
static void parse_cookies(const char *webdata, GaimUrlSession * session,
                          size_t len)
{


    char *haystack = g_malloc0(len * sizeof(char));
    size_t cookie_size;
    strncpy(haystack, webdata, len);
    char *next_token = haystack;
    char *end_token;
    char *new_cookie;

    const char *match = "Set-cookie: ";
    while ((next_token = strstr(next_token, match))) {

        next_token += strlen(match);    // We don't want the "Set-cookie:
        // " part!
        end_token = strstr(next_token, "\r\n");

        if (!end_token)
            continue;
        else {

            cookie_size = end_token - next_token;

            if (session->cookies)
                new_cookie =
                    g_strdup_printf("%s; %.*s", session->cookies,
                                    cookie_size, next_token);
            else
                new_cookie =
                    g_strdup_printf("%.*s", cookie_size, next_token);
            if (new_cookie) {
                g_free(session->cookies);       // g_strdup makes
                // allocates memory, so
                // get rid of the old
                // stuff.
                session->cookies = new_cookie;
            }



        }

    }
}


// This is a modified version of gaim's url_fetched_cb function. It has
// been
// modified to pass cookies during requests. The cookies are set in
// user_data,
// cast as a GaimUrlSession item. Any cookies in the response are added to 
// 
// 
// 
// this
// structure, as well.
static void
session_fetched_cb(gpointer url_data, gint sock, GaimInputCondition cond)
{
    GaimFetchUrlData *gfud = url_data;
    char data;
    gboolean got_eof = FALSE;
    if (sock == -1) {
        gfud->callback(gfud->user_data, NULL, 0);

        destroy_fetch_url_data(gfud);

        return;
    }

    if (!gfud->sentreq) {
        char buf[2048];

        if (gfud->session->cookies == NULL)
            gfud->session->cookies = g_strdup("");

        if (gfud->user_agent) {
            /* Host header is not forbidden in HTTP/1.0 requests, and
               HTTP/1.1 clients must know how to handle the "chunked"
               transfer encoding. Gaim doesn't know how to handle
               "chunked", so should always send the Host header
               regardless, to get around some observed problems */
            g_snprintf(buf, sizeof(buf), "GET %s%s HTTP/%s\r\n" "User-Agent: %s\r\n" "Host: %s\r\n" "Cookie: %s\r\n",   // (1) 
                                                                                                                        // 
                       // 
                       // 
                       // (see 
                       // above)
                       (gfud->full ? "" : "/"),
                       (gfud->full ? gfud->url : gfud->website.page),
                       (gfud->http11 ? "1.1" : "1.0"),
                       gfud->user_agent, gfud->website.address,
                       gfud->session->cookies);
        } else {
            g_snprintf(buf, sizeof(buf), "GET %s%s HTTP/%s\r\n" "Host: %s\r\n" "Accept-Encoding: identity\r\n" "Cookie: %s\r\n",        // (1) 
                                                                                                                                        // 
                       // 
                       // 
                       // See 
                       // above
                       (gfud->full ? "" : "/"),
                       (gfud->full ? gfud->url : gfud->website.page),
                       (gfud->http11 ? "1.1" : "1.0"),
                       gfud->website.address, gfud->session->cookies);
        }
        if (gfud->session->hasFormData)
            strcat(buf,
                   "Content-Type: application/x-www-form-urlencoded\r\n\r\n");
        else
            strcat(buf, "\r\n");
        gaim_debug_misc("gaim_url_fetch", "Request: %s\n", buf);

        write(sock, buf, strlen(buf));
        fcntl(sock, F_SETFL, O_NONBLOCK);
        gfud->sentreq = TRUE;
        gfud->inpa = gaim_input_add(sock, GAIM_INPUT_READ,
                                    session_fetched_cb, url_data);
        gfud->data_len = 4096;
        gfud->webdata = g_malloc(gfud->data_len);

        return;
    }

    if (read(sock, &data, 1) > 0 || errno == EWOULDBLOCK) {
        if (errno == EWOULDBLOCK) {
            errno = 0;

            return;
        }

        gfud->len++;

        if (gfud->len == gfud->data_len + 1) {
            gfud->data_len += (gfud->data_len) / 2;

            gfud->webdata = g_realloc(gfud->webdata, gfud->data_len);
        }

        gfud->webdata[gfud->len - 1] = data;

        if (!gfud->startsaving) {
            if (data == '\r')
                return;

            if (data == '\n') {
                if (gfud->newline) {
                    size_t content_len;
                    gfud->startsaving = TRUE;

                    gaim_debug_misc("gaim_url_fetch",
                                    "Response headers: '%*.*s'\n",
                                    gfud->len, gfud->len, gfud->webdata);

                    // JBL 10-16-2004: Put cookies into session
                    gaim_debug(GAIM_DEBUG_MISC, "gaym",
                               "Parsing cookies...");

                    parse_cookies(gfud->webdata, gfud->session, gfud->len);
                    gaim_debug(GAIM_DEBUG_MISC, "gaym",
                               "Found cookies: %s\n",
                               (gfud->session->cookies));
                    gaim_debug_misc("gaim_url_fetch",
                                    "Parsing of cookies successful\n");
                    /* See if we can find a redirect. */
                    if (parse_redirect
                        (gfud->webdata, gfud->len, sock, gfud))
                        return;

                    /* No redirect. See if we can find a content length. */
                    content_len =
                        parse_content_len(gfud->webdata, gfud->len);

                    if (content_len == 0) {
                        /* We'll stick with an initial 8192 */
                        content_len = 8192;
                    } else {
                        gfud->has_explicit_data_len = TRUE;
                    }

                    /* Out with the old... */
                    gfud->len = 0;
                    g_free(gfud->webdata);
                    gfud->webdata = NULL;

                    /* In with the new. */
                    gfud->data_len = content_len;
                    gfud->webdata = g_try_malloc(gfud->data_len);
                    if (gfud->webdata == NULL) {
                        gaim_debug_error("gaim_url_fetch",
                                         "Failed to allocate %u bytes: %s\n",
                                         gfud->data_len, strerror(errno));
                        gaim_input_remove(gfud->inpa);
                        close(sock);
                        gfud->callback(gfud->user_data, NULL, 0);
                        destroy_fetch_url_data(gfud);
                    }
                } else
                    gfud->newline = TRUE;

                return;
            }

            gfud->newline = FALSE;
        } else if (gfud->has_explicit_data_len
                   && gfud->len == gfud->data_len) {
            got_eof = TRUE;
        }
    } else if (errno != ETIMEDOUT) {
        got_eof = TRUE;
    } else {
        gaim_input_remove(gfud->inpa);
        close(sock);

        gfud->callback(gfud->user_data, NULL, 0);

        destroy_fetch_url_data(gfud);
    }

    if (got_eof) {
        gfud->webdata = g_realloc(gfud->webdata, gfud->len + 1);
        gfud->webdata[gfud->len] = 0;

        /* gaim_debug_misc("gaim_url_fetch", "Received: '%s'\n",
           gfud->webdata); */

        gaim_input_remove(gfud->inpa);
        close(sock);
        gfud->callback(gfud->user_data, gfud->webdata, gfud->len);

        destroy_fetch_url_data(gfud);
    }
}



void
gaim_session_fetch(const char *url, gboolean full,
                   const char *user_agent, gboolean http11,
                   void (*cb) (gpointer, const char *, size_t),
                   void *user_data, GaimUrlSession * session)
{
    int sock;
    GaimFetchUrlData *gfud;

    g_return_if_fail(url != NULL);
    g_return_if_fail(cb != NULL);

    gaim_debug_info("gaim_session_fetch",
                    "requested to fetch (%s), full=%d, user_agent=(%s), http11=%d\n",
                    url, full, user_agent ? user_agent : "(null)", http11);

    gfud = g_new0(GaimFetchUrlData, 1);

    gfud->callback = cb;
    gfud->user_data = user_data;
    gfud->url = g_strdup(url);
    gfud->user_agent = (user_agent != NULL ? g_strdup(user_agent) : NULL);
    gfud->http11 = http11;
    gfud->full = full;
    gfud->session = session;
    gaim_url_parse(url, &gfud->website.address, &gfud->website.port,
                   &gfud->website.page, &gfud->website.user,
                   &gfud->website.passwd);

    if ((sock = gaim_proxy_connect(NULL, gfud->website.address,
                                   gfud->website.port, session_fetched_cb,
                                   gfud)) < 0) {
        destroy_fetch_url_data(gfud);

        cb(user_data, g_strdup(_("g003: Error opening connection.\n")), 0);
    }
}

static void
gaym_weblogin_step5(gpointer session, const char *text, size_t len)
{

    struct gaym_conn *gaym = (struct gaym_conn *) session;
    // Get hash from text
    if (gaym->session && GAIM_CONNECTION_IS_VALID(gaym->account->gc)) {
        // char *pw_hash;
        char *bio;
        char *thumbnail;
        char *temp = NULL;
        char *temp2 = NULL;
        const char *match;
        const char *result;




        // First, look for password
        match = "password\" value=\"";
        temp = strstr(text, match);
        if (temp) {
            temp += strlen(match);
            temp2 = strstr(temp, "\" ");
        }
        if (!
            (temp && temp2 && temp != temp2
             && (gaym->hash_pw =
                 g_strndup(temp, (temp2 - temp) * sizeof(char))))) {
            gaim_connection_error((gaym->account->gc),
                                  _
                                  ("Problem parsing password from web. Report a bug."));
            return;
        }

        gaim_debug_misc("weblogin",
                        "Got hash, temp=%x, temp2=%x, gaym->hash_pw=%x\n",
                        temp, temp2, gaym->hash_pw);
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

            }
            // We have established a session. Call session callback.

        } else {
            // gaim_connection_error(
            // gaim_account_get_connection(((struct
            // gaym_conn*)((GaimUrlSession*)session)->account),
            // _("Problem parsing password from web. Report a bug."));
        }
        gaym->session->session_cb(gaym->account);

    }


}

static void
gaym_weblogin_step4(gpointer data, const char *text, size_t len)
{

    struct gaym_conn *gaym = (struct gaym_conn *) data;
    if (gaym->session
        && GAIM_CONNECTION_IS_VALID(gaym->session->account->gc)) {
        // The fourth step is to parse a rand=# value out of the message
        // text from
        // The previous step.
        // We then connect to messenger/applet.html
        char url[512];
        int nonce;
        char *buf = g_strdup_printf(_("Signon: %s"),
                                    (gaym->session->account->username));
        gaim_connection_update_progress(gaym->session->account->gc, buf, 5,
                                        6);
        sscanf(text, "?rand=%d", &nonce);
        snprintf(url, 512,
                 "http://www.gay.com/messenger/applet.html?rand=%d",
                 nonce);

        gaym->session->hasFormData = TRUE;
        gaim_session_fetch(url, FALSE, NULL, FALSE, gaym_weblogin_step5,
                           gaym, gaym->session);
    } else {
        g_free(gaym->session);
    }
}


static void
gaym_weblogin_step3(gpointer data, const char *text, size_t len)
{




    struct gaym_conn *gaym = (struct gaym_conn *) data;

    // gaim_debug_misc("weblogin","***************************%s\n",gaym->session->cookies);
    if (gaym->session
        && GAIM_CONNECTION_IS_VALID(gaym->session->account->gc)) {
        if (!strstr(gaym->session->cookies, "MEMBERX")) {
            g_free(gaym->session);
            gaim_connection_error((gaym->account->gc),
                                  _
                                  ("Problem during login. Are you sure your password is correct?"));
            return;
        }
        // The third step is to get a nonce needed for getting the applet.
        // We connect to messenger/frameset.html, using previously set
        // cookie values.
        // From the returned body, we will need to parse out the rand
        // values.
        char *url = "http://www.gay.com/messenger/frameset.html";
        char *buf = g_strdup_printf(_("Signon: %s"),
                                    (gaym->session->account->username));
        gaim_connection_update_progress(gaym->session->account->gc, buf, 4,
                                        6);
        gaym->session->hasFormData = FALSE;
        gaim_session_fetch(url, FALSE, NULL, FALSE, gaym_weblogin_step4,
                           gaym, gaym->session);
    } else
        g_free(gaym->session);
}
static void
gaym_weblogin_step2(gpointer data, const char *text, size_t len)
{

    struct gaym_conn *gaym = (struct gaym_conn *) data;
    if (gaym->session
        && GAIM_CONNECTION_IS_VALID(gaym->session->account->gc)) {
        // The second step is to do the actual login.
        // We connect to misc/dologin.html, using cookies set from step 1
        // And add a few more cookie values.
        char url[1024];
        char *buf = g_strdup_printf(_("Signon: %s"),
                                    gaym->session->account->username);
        gaim_connection_update_progress(gaym->session->account->gc, buf, 3,
                                        6);

        snprintf(url, 1024,
                 "http://www.gay.com/misc/dologin.html?__login_haveForm=1&__login_save=1&__login_member=%s&redir=%%2Findex.html&__login_basepage=%%2Fmisc%%2Fdologin.html&__login_password=%s",
                 gaym->session->username, gaym->session->password);

        gaym->session->hasFormData = TRUE;
        gaim_session_fetch(url, FALSE, NULL, FALSE, gaym_weblogin_step3,
                           gaym, gaym->session);
    } else
        g_free(gaym->session);
}
void
gaym_get_hash_from_weblogin(GaimAccount * account,
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
        if (gaym->session)
            g_free(gaym->session);

        gaym->session = session;

        if (GAIM_CONNECTION_IS_VALID
            (((GaimUrlSession *) session)->account->gc)) {
            // The first step is to establish the initial sesion
            // We connect to index.html, and get a few cookie values.
            char *url = "http://www.gay.com/index.html";
            char *buf = g_strdup_printf(_("Signon: %s"),
                                        ((GaimUrlSession *) session)->
                                        account->username);
            gaim_connection_update_progress(((GaimUrlSession *) session)->
                                            account->gc, buf, 2, 6);
            ((GaimUrlSession *) session)->hasFormData = FALSE;
            gaim_session_fetch(url, FALSE, NULL, FALSE,
                               gaym_weblogin_step2, gaym, gaym->session);
        }

    }
}

// vim:tabstop=4:shiftwidth=4:expandtab:
