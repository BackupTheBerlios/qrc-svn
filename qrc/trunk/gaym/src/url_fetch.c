#include "gaym.h"
#include "util.h"
#include "debug.h"
struct _PurpleUtilFetchUrlData
{
	PurpleUtilFetchUrlCallback callback;
	void *user_data;

	struct
	{
		char *user;
		char *passwd;
		char *address;
		int port;
		char *page;

	} website;

	char *url;
	int num_times_redirected;
	gboolean full;
	char *user_agent;
	gboolean http11;
	char *request;
	gsize request_written;
	gboolean include_headers;

	PurpleProxyConnectData *connect_data;
	int fd;
	guint inpa;

	gboolean got_headers;
	gboolean has_explicit_data_len;
	char *webdata;
	unsigned long len;
	unsigned long data_len;
};

/**
 * The arguments to this function are similar to printf.
 */
static void
purple_util_fetch_url_error(PurpleUtilFetchUrlData *gfud, const char *format, ...)
{
	gchar *error_message;
	va_list args;

	va_start(args, format);
	error_message = g_strdup_vprintf(format, args);
	va_end(args);

	gfud->callback(gfud, gfud->user_data, NULL, 0, error_message);
	g_free(error_message);
	purple_util_fetch_url_cancel(gfud);
}

static void url_fetch_connect_cb(gpointer url_data, gint source, const gchar *error_message);

static size_t
parse_content_len(const char *data, size_t data_len)
{
	size_t content_len = 0;
	const char *p = NULL;

	/* This is still technically wrong, since headers are case-insensitive
	 * [RFC 2616, section 4.2], though this ought to catch the normal case.
	 * Note: data is _not_ nul-terminated.
	 */
	if(data_len > 16) {
		p = (strncmp(data, "Content-Length: ", 16) == 0) ? data : NULL;
		if(!p)
			p = (strncmp(data, "CONTENT-LENGTH: ", 16) == 0)
				? data : NULL;
		if(!p) {
			p = g_strstr_len(data, data_len, "\nContent-Length: ");
			if (p)
				p++;
		}
		if(!p) {
			p = g_strstr_len(data, data_len, "\nCONTENT-LENGTH: ");
			if (p)
				p++;
		}

		if(p)
			p += 16;
	}

	/* If we can find a Content-Length header at all, try to sscanf it.
	 * Response headers should end with at least \r\n, so sscanf is safe,
	 * if we make sure that there is indeed a \n in our header.
	 */
	if (p && g_strstr_len(p, data_len - (p - data), "\n")) {
		sscanf(p, "%" G_GSIZE_FORMAT, &content_len);
		purple_debug_misc("util", "parsed %u\n", content_len);
	}

	return content_len;
}


static void
url_fetch_recv_cb(gpointer url_data, gint source, PurpleInputCondition cond)
{
	PurpleUtilFetchUrlData *gfud = url_data;
	int len;
	char buf[4096];
	char *data_cursor;
	gboolean got_eof = FALSE;

	while((len = read(source, buf, sizeof(buf))) > 0) {
		/* If we've filled up our buffer, make it bigger */
		if((gfud->len + len) >= gfud->data_len) {
			while((gfud->len + len) >= gfud->data_len)
				gfud->data_len += sizeof(buf);

			gfud->webdata = g_realloc(gfud->webdata, gfud->data_len);
		}

		data_cursor = gfud->webdata + gfud->len;

		gfud->len += len;

		memcpy(data_cursor, buf, len);

		gfud->webdata[gfud->len] = '\0';

		if(!gfud->got_headers) {
			char *tmp;

			/* See if we've reached the end of the headers yet */
			if((tmp = strstr(gfud->webdata, "\r\n\r\n"))) {
				char * new_data;
				guint header_len = (tmp + 4 - gfud->webdata);
				size_t content_len;

				purple_debug_misc("util", "Response headers: '%.*s'\n",
					header_len, gfud->webdata);

				/* See if we can find a redirect. */
				/* GAYM FIX: Don't parse redirects! */
				//if(parse_redirect(gfud->webdata, header_len, source, gfud))
				//	return;

				gfud->got_headers = TRUE;

				/* No redirect. See if we can find a content length. */
				content_len = parse_content_len(gfud->webdata, header_len);

				if(content_len == 0) {
					/* We'll stick with an initial 8192 */
					content_len = 8192;
				} else {
					gfud->has_explicit_data_len = TRUE;
				}


				/* If we're returning the headers too, we don't need to clean them out */
				if(gfud->include_headers) {
					gfud->data_len = content_len + header_len;
					gfud->webdata = g_realloc(gfud->webdata, gfud->data_len);
				} else {
					size_t body_len = 0;

					if(gfud->len > (header_len + 1))
						body_len = (gfud->len - header_len);

					content_len = MAX(content_len, body_len);

					new_data = g_try_malloc(content_len);
					if(new_data == NULL) {
						purple_debug_error("util",
								"Failed to allocate %u bytes: %s\n",
								content_len, strerror(errno));
						purple_util_fetch_url_error(gfud,
								_("Unable to allocate enough memory to hold "
								  "the contents from %s.  The web server may "
								  "be trying something malicious."),
								gfud->website.address);

						return;
					}

					/* We may have read part of the body when reading the headers, don't lose it */
					if(body_len > 0) {
						tmp += 4;
						memcpy(new_data, tmp, body_len);
					}

					/* Out with the old... */
					g_free(gfud->webdata);

					/* In with the new. */
					gfud->len = body_len;
					gfud->data_len = content_len;
					gfud->webdata = new_data;
				}
			}
		}

		if(gfud->has_explicit_data_len && gfud->len >= gfud->data_len) {
			got_eof = TRUE;
			break;
		}
	}

	if(len < 0) {
		if(errno == EAGAIN) {
			return;
		} else {
			purple_util_fetch_url_error(gfud, _("Error reading from %s: %s"),
					gfud->website.address, strerror(errno));
			return;
		}
	}

	if((len == 0) || got_eof) {
		gfud->webdata = g_realloc(gfud->webdata, gfud->len + 1);
		gfud->webdata[gfud->len] = '\0';

		gfud->callback(gfud, gfud->user_data, gfud->webdata, gfud->len, NULL);
		purple_util_fetch_url_cancel(gfud);
	}
}

static void
url_fetch_send_cb(gpointer data, gint source, PurpleInputCondition cond)
{
	PurpleUtilFetchUrlData *gfud;
	int len, total_len;

	gfud = data;

	total_len = strlen(gfud->request);

	len = write(gfud->fd, gfud->request + gfud->request_written,
			total_len - gfud->request_written);

	if (len < 0 && errno == EAGAIN)
		return;
	else if (len < 0) {
		purple_util_fetch_url_error(gfud, _("Error writing to %s: %s"),
				gfud->website.address, strerror(errno));
		return;
	}
	gfud->request_written += len;

	if (gfud->request_written < total_len)
		return;

	/* We're done writing our request, now start reading the response */
	purple_input_remove(gfud->inpa);
	gfud->inpa = purple_input_add(gfud->fd, PURPLE_INPUT_READ, url_fetch_recv_cb,
		gfud);
}

static void
url_fetch_connect_cb(gpointer url_data, gint source, const gchar *error_message)
{
	PurpleUtilFetchUrlData *gfud;

	gfud = url_data;
	gfud->connect_data = NULL;

	if (source == -1)
	{
		purple_util_fetch_url_error(gfud, _("Unable to connect to %s: %s"),
				(gfud->website.address ? gfud->website.address : ""), error_message);
		return;
	}

	gfud->fd = source;

	if (!gfud->request)
	{
		if (gfud->user_agent) {
			/* Host header is not forbidden in HTTP/1.0 requests, and HTTP/1.1
			 * clients must know how to handle the "chunked" transfer encoding.
			 * Purple doesn't know how to handle "chunked", so should always send
			 * the Host header regardless, to get around some observed problems
			 */
			gfud->request = g_strdup_printf(
				"GET %s%s HTTP/%s\r\n"
				"Connection: close\r\n"
				"User-Agent: %s\r\n"
				"Accept: */*\r\n"
				"Host: %s\r\n\r\n",
				(gfud->full ? "" : "/"),
				(gfud->full ? (gfud->url ? gfud->url : "") : (gfud->website.page ? gfud->website.page : "")),
				(gfud->http11 ? "1.1" : "1.0"),
				(gfud->user_agent ? gfud->user_agent : ""),
				(gfud->website.address ? gfud->website.address : ""));
		} else {
			gfud->request = g_strdup_printf(
				"GET %s%s HTTP/%s\r\n"
				"Connection: close\r\n"
				"Accept: */*\r\n"
				"Host: %s\r\n\r\n",
				(gfud->full ? "" : "/"),
				(gfud->full ? (gfud->url ? gfud->url : "") : (gfud->website.page ? gfud->website.page : "")),
				(gfud->http11 ? "1.1" : "1.0"),
				(gfud->website.address ? gfud->website.address : ""));
		}
	}

	purple_debug_misc("util", "Request: '%s'\n", gfud->request);

	gfud->inpa = purple_input_add(source, PURPLE_INPUT_WRITE,
								url_fetch_send_cb, gfud);
	url_fetch_send_cb(gfud, source, PURPLE_INPUT_WRITE);
}



PurpleUtilFetchUrlData *
gaym_util_fetch_url_request(const char *url, gboolean full,
		const char *user_agent, gboolean http11,
		const char *request, gboolean include_headers,
		PurpleUtilFetchUrlCallback callback, void *user_data)
{
	PurpleUtilFetchUrlData *gfud;

	g_return_val_if_fail(url      != NULL, NULL);
	g_return_val_if_fail(callback != NULL, NULL);

	purple_debug_info("util",
			 "requested to fetch (%s), full=%d, user_agent=(%s), http11=%d\n",
			 url, full, user_agent?user_agent:"(null)", http11);

	gfud = g_new0(PurpleUtilFetchUrlData, 1);

	gfud->callback = callback;
	gfud->user_data  = user_data;
	gfud->url = g_strdup(url);
	gfud->user_agent = g_strdup(user_agent);
	gfud->http11 = http11;
	gfud->full = full;
	gfud->request = g_strdup(request);
	gfud->include_headers = include_headers;

	purple_url_parse(url, &gfud->website.address, &gfud->website.port,
				   &gfud->website.page, &gfud->website.user, &gfud->website.passwd);

	gfud->connect_data = purple_proxy_connect(NULL, NULL,
			gfud->website.address, gfud->website.port,
			url_fetch_connect_cb, gfud);

	if (gfud->connect_data == NULL)
	{
		purple_util_fetch_url_error(gfud, _("Unable to connect to %s"),
				gfud->website.address);
		return NULL;
	}

	return gfud;
}


