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
#include "util.c"



//This looks for Set-cookie: fields in headers, and adds those cookies 
//To the session struct.
//TODO current the cookie field in the session struct is fixed size, because mem 
//allocation functions were giving me hell. This should be changed, though.
//TODO If a cookie value is re-set, then it will be duplicated, since Set-Cookie
//field is just appended to already existing cookies. This is not an issue for 
//the current gay.com implementation, but this should be changed, so that 
//receiving a duplicate cookie *updates* the existing value.
//JBL 10-16-2004
static void parse_cookies(const char* webdata, GaimUrlSession *session, size_t len){
 
	char *next_token;
	char *haystack=g_malloc0(len*sizeof(char));
	size_t cookie_size;
	strncpy(haystack, webdata, len);

	next_token=strtok(haystack, "\r\n");
	while(next_token) {
		gaim_debug_misc("gaym","Looking for cookie in %s\n",next_token);
		if(!strncmp(next_token, "Set-cookie: ", 12)) {
			cookie_size=strlen(next_token+12);
			if (session->cookies)
			{	//session->cookies=realloc(session->cookies,
				//		session->cookie_len + cookie_size+2);
				//gaim_debug_misc("gaym","Extending cookie struct by %d\n",cookie_size);
				//session->cookie_len += cookie_size+2;
				strncat(session->cookies,"; ",2);
				strncat(session->cookies,next_token+12,cookie_size);
			}
			else
			{
				//session->cookies=malloc(cookie_size*sizeof(char));
				//session->cookie_len = cookie_size;
				
				strncpy(session->cookies,next_token+12,cookie_size);
			}
		}
		next_token=strtok('\0',"\r\n");
	}
}

	
//This is a modified version of gaim's url_fetched_cb function. It has been 
//modified to pass cookies during requests. The cookies are set in user_data,
//cast as a GaimUrlSession item. Any cookies in the response are added to this
//structure, as well.
static void
session_fetched_cb(gpointer url_data, gint sock, GaimInputCondition cond)
{
	GaimFetchUrlData *gfud = url_data;
	char data;
	gboolean got_eof = FALSE;
	if (sock == -1)
	{
		gfud->callback(gfud->user_data, NULL, 0);

		destroy_fetch_url_data(gfud);

		return;
	}

	if (!gfud->sentreq)
	{
		char buf[2048];

		if (gfud->user_agent)
		{
			/* Host header is not forbidden in HTTP/1.0 requests, and HTTP/1.1
			 * clients must know how to handle the "chunked" transfer encoding.
			 * Gaim doesn't know how to handle "chunked", so should always send
			 * the Host header regardless, to get around some observed problems
			 */
			g_snprintf(buf, sizeof(buf),
					   "GET %s%s HTTP/%s\r\n"
					   "User-Agent: %s\r\n"
					   "Host: %s\r\n"
					   "Cookie: %s\r\n", //(1) (see above)
					   (gfud->full ? "" : "/"),
					   (gfud->full ? gfud->url : gfud->website.page),
					   (gfud->http11 ? "1.1" : "1.0"),
					   gfud->user_agent, gfud->website.address,
					   ((GaimUrlSession*)(gfud->user_data))->cookies);
		}
		else
		{
			g_snprintf(buf, sizeof(buf),
					   "GET %s%s HTTP/%s\r\n"
					   "Host: %s\r\n"
					   "Accept-Encoding: identity\r\n"
					   "Cookie: %s\r\n", //(1) See above
					   (gfud->full ? "" : "/"),
					   (gfud->full ? gfud->url : gfud->website.page),
					   (gfud->http11 ? "1.1" : "1.0"),
					   gfud->website.address,
					   ((GaimUrlSession*)(gfud->user_data))->cookies);
		}
		if (((GaimUrlSession*)gfud->user_data)->hasFormData)
			strcat(buf, "Content-Type: application/x-www-form-urlencoded\r\n\r\n");
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

	if (read(sock, &data, 1) > 0 || errno == EWOULDBLOCK)
	{
		if (errno == EWOULDBLOCK)
		{
			errno = 0;

			return;
		}

		gfud->len++;

		if (gfud->len == gfud->data_len + 1)
		{
			gfud->data_len += (gfud->data_len) / 2;

			gfud->webdata = g_realloc(gfud->webdata, gfud->data_len);
		}

		gfud->webdata[gfud->len - 1] = data;

		if (!gfud->startsaving)
		{
			if (data == '\r')
				return;

			if (data == '\n')
			{
				if (gfud->newline)
				{
					size_t content_len;
					gfud->startsaving = TRUE;

					gaim_debug_misc("gaim_url_fetch", "Response headers: '%*.*s'\n", gfud->len, gfud->len, gfud->webdata);

					//JBL 10-16-2004: Put cookies into session 
					gaim_debug(GAIM_DEBUG_MISC, "gaym", "Parsing cookies...");
					
					parse_cookies(gfud->webdata, (GaimUrlSession*)(gfud->user_data), gfud->len); 
					gaim_debug(GAIM_DEBUG_MISC, "gaym", "Found cookies: %s\n",((GaimUrlSession*)(gfud->user_data))->cookies);
					gaim_debug_misc("gaim_url_fetch", "Parsing of cookies successful\n");
					/* See if we can find a redirect. */
					if (parse_redirect(gfud->webdata, gfud->len, sock, gfud))
						return;

					/* No redirect. See if we can find a content length. */
					content_len = parse_content_len(gfud->webdata, gfud->len);

					if (content_len == 0)
					{
						/* We'll stick with an initial 8192 */
						content_len = 8192;
					}
					else
					{
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
						gaim_debug_error("gaim_url_fetch", "Failed to allocate %u bytes: %s\n", gfud->data_len, strerror(errno));
						gaim_input_remove(gfud->inpa);
						close(sock);
						gfud->callback(gfud->user_data, NULL, 0);
						destroy_fetch_url_data(gfud);
					}
				}
				else
					gfud->newline = TRUE;

				return;
			}

			gfud->newline = FALSE;
		}
		else if (gfud->has_explicit_data_len && gfud->len == gfud->data_len)
		{
			got_eof = TRUE;
		}
	}
	else if (errno != ETIMEDOUT)
	{
		got_eof = TRUE;
	}
	else
	{
		gaim_input_remove(gfud->inpa);
		close(sock);

		gfud->callback(gfud->user_data, NULL, 0);

		destroy_fetch_url_data(gfud);
	}

	if (got_eof) {
		gfud->webdata = g_realloc(gfud->webdata, gfud->len + 1);
		gfud->webdata[gfud->len] = 0;

		/* gaim_debug_misc("gaim_url_fetch", "Received: '%s'\n", gfud->webdata); */

		gaim_input_remove(gfud->inpa);
		close(sock);
		gfud->callback(gfud->user_data, gfud->webdata, gfud->len);

		destroy_fetch_url_data(gfud);
	}
}


//Ugh. A whole function replicate just to change url_fetched_cb to session_fetched_cb
static void
gaim_session_fetch(const char *url, gboolean full,
			   const char *user_agent, gboolean http11,
			   void (*cb)(gpointer, const char *, size_t),
			   void *user_data)
{
	int sock;
	GaimFetchUrlData *gfud;

	g_return_if_fail(url != NULL);
	g_return_if_fail(cb  != NULL);

	gaim_debug_info("gaim_session_fetch",
			 "requested to fetch (%s), full=%d, user_agent=(%s), http11=%d\n",
			 url, full, user_agent?user_agent:"(null)", http11);

	gfud = g_new0(GaimFetchUrlData, 1);

	gfud->callback   = cb;
	gfud->user_data  = user_data;
	gfud->url        = g_strdup(url);
	gfud->user_agent = (user_agent != NULL ? g_strdup(user_agent) : NULL);
	gfud->http11     = http11;
	gfud->full       = full;

	gaim_url_parse(url, &gfud->website.address, &gfud->website.port,
				   &gfud->website.page, &gfud->website.user, &gfud->website.passwd);

	if ((sock = gaim_proxy_connect(NULL, gfud->website.address,
					   gfud->website.port, session_fetched_cb,
					   gfud)) < 0)
	{
		destroy_fetch_url_data(gfud);

		cb(user_data, g_strdup(_("g003: Error opening connection.\n")), 0);
	}
}

static void
gaym_weblogin_step5(gpointer session, const char* text, size_t len) {

	//Get hash from text
	
	char *pw_hash;
	char *bio;
	char *temp;
	
	temp=strstr(text, "v0_");
	pw_hash=strtok(temp,"\" ");
	gaim_debug_misc("gaym","Found hash: %s",pw_hash);
	
 
	
	//Ok, maybe we should set this somewhere else. This looks silly. Hehe.
	((struct gaym_conn*)((GaimUrlSession*)session)->account->gc->proto_data)->
		hash_pw=strdup(pw_hash);
	
	
	temp=strstr(text, "
	//We have established a session. Call session callback.
	((GaimUrlSession*)session)->session_cb(((GaimUrlSession*)session)->account);
	
}

static void
gaym_weblogin_step4(gpointer session, const char* text, size_t len) {

	//The fourth step is to parse a rand=# value out of the message text from 
	//The previous step.
	//We then connect to messenger/applet.html  
	char url[512];
	int nonce;
	char* buf = g_strdup_printf(_("Signon: %s"), ((GaimUrlSession*)session)->account->username);
	gaim_connection_update_progress(((GaimUrlSession*)session)->account->gc, buf, 5, 6);
	sscanf(text, "?rand=%d",&nonce);
	snprintf(url,512,"http://www.gay.com/messenger/applet.html?rand=%d",nonce);
	
	((GaimUrlSession*)session)->hasFormData=TRUE;
	gaim_session_fetch(url, FALSE, NULL, FALSE,gaym_weblogin_step5, session);
	
}


static void
gaym_weblogin_step3(gpointer session, const char* text, size_t len) {

	//The third step is to get a nonce needed for getting the applet.
	//We connect to messenger/frameset.html, using previously set cookie values.
	//From the returned body, we will need to parse out the rand values.
	char* url = "http://www.gay.com/messenger/frameset.html";
	char* buf = g_strdup_printf(_("Signon: %s"), ((GaimUrlSession*)session)->account->username);
	gaim_connection_update_progress(((GaimUrlSession*)session)->account->gc, buf, 4, 6);
	((GaimUrlSession*)session)->hasFormData=FALSE;
	gaim_session_fetch(url, FALSE, NULL, FALSE,gaym_weblogin_step4, session);
	
}
static void
gaym_weblogin_step2(gpointer session, const char* text, size_t len) {

	//The second step is to do the actual login.
	//We connect to misc/dologin.html, using cookies set from step 1
	//And add a few more cookie values.
	char url[1024];
	char* buf = g_strdup_printf(_("Signon: %s"), ((GaimUrlSession*)session)->account->username);
	gaim_connection_update_progress(((GaimUrlSession*)session)->account->gc, buf, 3, 6);
	gaim_debug(GAIM_DEBUG_MISC, "gaym", "From step one, got cookies: %s\n", ((GaimUrlSession*)session)->cookies);
	
	snprintf(url,1024,"http://www.gay.com/misc/dologin.html?__login_haveForm=1&__login_save=1&__login_member=%s&redir=%%2Findex.html&__login_basepage=%%2Fmisc%%2Fdologin.html&__login_password=%s",
	((GaimUrlSession*)session)->username, 
	((GaimUrlSession*)session)->password);
	
	gaim_debug(GAIM_DEBUG_MISC, "gaym", "In step 2, trying: %s\n", url);
	((GaimUrlSession*)session)->hasFormData=TRUE;
	gaim_session_fetch(url, FALSE, NULL, FALSE,gaym_weblogin_step3, session);
	
}
static void
gaym_weblogin_step1(gpointer session) {

	//The first step is to establish the initial sesion
	//We connect to index.html, and get a few cookie values. 
	char* url = "http://www.gay.com/index.html";
	char* buf = g_strdup_printf(_("Signon: %s"), ((GaimUrlSession*)session)->account->username);
	gaim_connection_update_progress(((GaimUrlSession*)session)->account->gc, buf, 2, 6);
	((GaimUrlSession*)session)->hasFormData=FALSE;
	gaim_session_fetch(url, FALSE, NULL, FALSE,gaym_weblogin_step2, session);
	
}

void 
gaym_get_hash_from_weblogin(GaimAccount* account, void(*callback)(GaimAccount* account)) {
	
	GaimUrlSession* session=g_new0(GaimUrlSession, 1);
	session->session_cb = callback;
	session->cookie_len=0;
	session->account=account;
	session->username=strtok(account->username,"@");
	session->password=strtok(account->password,"\0");
	gaym_weblogin_step1(session);
}
