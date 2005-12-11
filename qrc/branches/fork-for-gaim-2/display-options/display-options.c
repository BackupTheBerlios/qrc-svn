/*
 * This is to try to add some extra readability to Gaim's aweful
 * display.  I'd like the colors to be background colors, but not
 * just font backgrounds .. a DIV or TD background.  Right now this
 * isn't possible to do while still using GtkIMHTML.  I'd also like
 * to have the thumbnails of who talks on the left-hand side, but for
 * all protocols.  Would also like the name and/or thumbnail to be
 * clickable, and pull up either the Get Info or Full Profile, and
 * possibly a right click menu for Info/IM.
 */

#include <glib.h>
#include "internal.h"
#include "gtkgaim.h"

#include "conversation.h"
#include "debug.h"
#include "log.h"
#include "prefs.h"
#include "signals.h"
#include "util.h"
#include "version.h"

#include "gtkconv.h"
#include "gtkimhtml.h"
#include "gtkplugin.h"

#define CHATSORT_PLUGIN_ID "display-options"

/* XXX */
#include "gaim.h"

#define AUTO_RESPONSE "&lt;AUTO-REPLY&gt; : "

#define SEND_COLOR "#16569E"
#define RECV_COLOR "#A82F2F"

/* These are right out of gaim, and they need to be darkened!*/
static char nick_colors[][8] = {
	"#ba55d3",              /* Medium Orchid */
	"#ee82ee",              /* Violet */
	"#c715b4",              /* Medium Violet Red */
	"#ff69b4",              /* Hot Pink */
	"#ff6347",              /* Tomato */
	"#fa8c00",              /* Dark Orange */
	"#fa8072",              /* Salmon */
	"#b22222",              /* Fire Brick */
	"#f4a460",              /* Sandy Brown */
	"#cd5c5c",              /* Indian Red */
	"#bc8f8f",              /* Rosy Brown */
	"#f0e68c",              /* Khaki */
	"#bdb76b",              /* Dark Khaki */
	"#228b22",              /* Forest Green */
	"#9acd32",              /* Yellow Green */
	"#32cd32",              /* Lime Green */
	"#3cb371",              /* Medium Sea Green (Medium Sea Green?!  What are we women?!) */
	"#2e8b57",              /* Sea Green */
	"#8fbc8f",              /* Dark Sea Green */
	"#66cdaa",              /* Medium Aquamarine */
	"#5f9ea0",              /* Cadet Blue */
	"#48d1cc",              /* Medium Turquoise */
	"#00ced1",              /* Dark Turquoise */
	"#4682b4",              /* Stell Blue */
	"#00bfff",              /* Deep Sky Blue */
	"#1690ff",              /* Dodger Blue */
	"#4169ff",              /* Royal Blue */
	"#6a5acd",              /* Slate Blue */
	"#6495ed",              /* Cornflower Blue */
	"#708090",              /* Slate gray */
	"#2f4f4f",              /* Dark Slate Gray */
	"#ff8c00",              /* Dark Orange */
	"#006400",              /* DarkGreen */
	"#8b4513",              /* SaddleBrown */
	"#8b8989",              /* snow4 */
	"#7d26cd",              /* purple3 */
};

#define NUM_NICK_COLORS (sizeof(nick_colors) / sizeof(*nick_colors))

/* Holds the last message spoken and who said it for each conversation to
 * avoid annoying repeats and floods
 */
struct LastMessage {
	gchar * who;
	gchar * msg;
};

/* This holds the above Structs */
static GHashTable * Conversations = NULL;

/* Holds the pointer to the original function */
static void (*disp_gtkconv_write_conv)(GaimConversation *conv, const char *who,
        const char *message, GaimMessageFlags flags,time_t mtime);

/* Holds UiOps pointer between calls */
static GaimConversationUiOps *disp_op;

/* Custom destructor */
static void RemoveLastMessage (gpointer data)
{
	struct LastMessage * lm = data;
	g_free (lm->msg);
	g_free (lm->who);
	g_free (lm);
}

/* Longer and more complex than I'd like */
gchar * format_system_msg (const gchar * message,struct LastMessage * lm)
{
    char * newmessage = NULL;
    gchar ** parts = NULL;
	
    parts = g_strsplit (message," ",2);

    if ((parts[0]) && (parts[1])) {
        gchar ** prebio = g_strsplit (parts[1],"[",2);
	if (lm->who) {
		g_free(lm->who);
	}
	if (lm->msg) {
		g_free(lm->msg);
	}
	lm->who = strdup (parts[0]);
	lm->msg = NULL;
	if (prebio && prebio[1]) {
	    gchar ** postbio = g_strsplit (prebio[1],"]",2);
	    if (postbio && postbio[1]) {
                newmessage = g_strdup_printf ("<font size=-2><b>%s</b> %s[<font color=\"#006080\"><b>%s</b></font>]%s</font>",parts[0],prebio[0],postbio[0],postbio[1]);
	    }
	    g_strfreev(postbio);
	} else {
            newmessage = g_strdup_printf ("<font size=-2><b>%s</b> %s</font>",parts[0],parts[1]);
	}				
	g_strfreev(prebio);
    }
    g_strfreev(parts);
	
    if (newmessage == NULL) {
        newmessage = g_strdup_printf ("<font size=-2>%s</font>",message);
    }
    return newmessage;
}

/* This replaces gaim's default version */
static void
disp_write_convo (GaimConversation *conv, const char *who,
						const char *message, GaimMessageFlags flags,
						time_t mtime)
{
	GaimGtkConversation *gtkconv;
	GaimConvWindow *win;
	GaimConnection *gc;
	int gtk_font_options = 0;
	char buf[BUF_LONG];
	char buf2[BUF_LONG];
	char mdate[64];
	char color[10];
	char *str;
	char *with_font_tag;
	char *sml_attrib = NULL;
	size_t length = strlen(message) + 1;

	gtkconv = GAIM_GTK_CONVERSATION(conv);
	gc = gaim_conversation_get_gc(conv);

	win = gaim_conversation_get_window(conv);

	if (!(flags & GAIM_MESSAGE_NO_LOG) &&
		((gaim_conversation_get_type(conv) == GAIM_CONV_CHAT &&
		 gaim_prefs_get_bool("/gaim/gtk/conversations/chat/raise_on_events")) ||
		(gaim_conversation_get_type(conv) == GAIM_CONV_IM &&
		 gaim_prefs_get_bool("/gaim/gtk/conversations/im/raise_on_events")))) {
		gaim_conv_window_show(win);
	}
/*
	if (gtk_text_buffer_get_char_count(gtk_text_view_get_buffer(GTK_TEXT_VIEW(gtkconv->imhtml))))
		gtk_imhtml_append_text(GTK_IMHTML(gtkconv->imhtml), "<BR>", 0);
*/
	if(time(NULL) > mtime + 20*60) /* show date if older than 20 minutes */
		strftime(mdate, sizeof(mdate), "%Y-%m-%d %H:%M:%S", localtime(&mtime));
	else
		strftime(mdate, sizeof(mdate), "%H:%M:%S", localtime(&mtime));

	if(gc)
		sml_attrib = g_strdup_printf("sml=\"%s\"",
									 gaim_account_get_protocol_name(conv->account));

	gtk_font_options ^= GTK_IMHTML_NO_COMMENTS;

	if (gaim_prefs_get_bool("/gaim/gtk/conversations/ignore_colors"))
		gtk_font_options ^= GTK_IMHTML_NO_COLOURS;
	if (gaim_prefs_get_bool("/gaim/gtk/conversations/ignore_fonts"))
		gtk_font_options ^= GTK_IMHTML_NO_FONTS;
	if (gaim_prefs_get_bool("/gaim/gtk/conversations/ignore_font_sizes"))
		gtk_font_options ^= GTK_IMHTML_NO_SIZES;

	/* this is gonna crash one day, I can feel it. */
	if (GAIM_PLUGIN_PROTOCOL_INFO(gaim_find_prpl(gaim_account_get_protocol_id(conv->account)))->options &
	    OPT_PROTO_USE_POINTSIZE) {
		gtk_font_options ^= GTK_IMHTML_USE_POINTSIZE;
	}

	if (flags & GAIM_MESSAGE_SYSTEM) {
		gchar * sep = "<BR>";
		gchar * newmessage = NULL; 
		
		struct LastMessage * lastconv = (struct LastMessage *)
			g_hash_table_lookup(Conversations,conv); 

		if (lastconv && (lastconv->who) && (lastconv->msg != NULL)) {
			sep = "<HR>";
		}
		newmessage = format_system_msg (message,lastconv);
		
		if (gaim_prefs_get_bool("/gaim/gtk/conversations/show_timestamps"))
			g_snprintf(buf, BUF_LONG, "<FONT SIZE=\"2\">(%s)</FONT> %s",
				   mdate, newmessage);
		else
			g_snprintf(buf, BUF_LONG, "%s", newmessage);

		g_snprintf(buf2, sizeof(buf2),
			   "%s<FONT %s><FONT SIZE=\"2\"><!--(%s) --></FONT>%s", sep, sml_attrib ? sml_attrib : "", mdate, newmessage);

		g_free(newmessage);
		
		gtk_imhtml_append_text(GTK_IMHTML(gtkconv->imhtml), buf2, 0);

		/* Add the message to a conversations scrollback buffer */
		conv->history = g_string_append(conv->history, buf);
		conv->history = g_string_append(conv->history, "<BR>\n");

	} else if (flags & GAIM_MESSAGE_ERROR) {
		if (gaim_prefs_get_bool("/gaim/gtk/conversations/show_timestamps"))
			g_snprintf(buf, BUF_LONG, "<FONT COLOR=\"#ff0000\"><FONT SIZE=\"2\">(%s)</FONT> <B>%s</B></FONT>",
				   mdate, message);
		else
			g_snprintf(buf, BUF_LONG, "<FONT COLOR=\"#ff0000\"><B>%s</B></FONT>", message);

		g_snprintf(buf2, sizeof(buf2),
			   "<BR><FONT COLOR=\"#ff0000\"><FONT %s><FONT SIZE=\"2\"><!--(%s) --></FONT><B>%s</B></FONT></FONT>",
			   sml_attrib ? sml_attrib : "", mdate, message);

		gtk_imhtml_append_text(GTK_IMHTML(gtkconv->imhtml), buf2, 0);

		/* Add the message to a conversations scrollback buffer */
		conv->history = g_string_append(conv->history, buf);
		conv->history = g_string_append(conv->history, "<BR>\n");
	} else if (flags & GAIM_MESSAGE_NO_LOG) {
		g_snprintf(buf, BUF_LONG,
			   "<BR><B><FONT %s COLOR=\"#777777\">%s</FONT></B>",
			   sml_attrib ? sml_attrib : "", message);

		gtk_imhtml_append_text(GTK_IMHTML(gtkconv->imhtml), buf, 0);
	}
	else {
		char *new_message = g_memdup(message, length);
		char *who_escaped = (who ? g_markup_escape_text(who, strlen(who)) : g_strdup(""));

		if (flags & GAIM_MESSAGE_WHISPER) {
			str = g_malloc(1024);

			/* If we're whispering, it's not an autoresponse. */
			if (gaim_message_meify(new_message, -1)) {
				g_snprintf(str, 1024, "***%s", who_escaped);
				strcpy(color, "#6C2585");
			}
			else {
				g_snprintf(str, 1024, "*%s*:", who_escaped);
				strcpy(color, "#00FF00");
			}
		}
		else {
			if (gaim_message_meify(new_message, -1)) {
				str = g_malloc(1024);

				if (flags & GAIM_MESSAGE_AUTO_RESP)
					g_snprintf(str, 1024, "%s ***%s", AUTO_RESPONSE, who_escaped);
				else
					g_snprintf(str, 1024, "***%s", who_escaped);

				if (flags & GAIM_MESSAGE_NICK)
					strcpy(color, "#AF7F00");
				else
					strcpy(color, "#062585");
			}
			else {
				str = g_malloc(1024);
				if (flags & GAIM_MESSAGE_AUTO_RESP)
					g_snprintf(str, 1024, "%s %s", who_escaped, AUTO_RESPONSE);
				else
					g_snprintf(str, 1024, "%s", who_escaped);
				if (flags & GAIM_MESSAGE_NICK)
					strcpy(color, "#AF7F00");
				else if (flags & GAIM_MESSAGE_RECV) {
					if (flags & GAIM_MESSAGE_COLORIZE) {
						const char *u;
						int m = 0;
						
						for (u = who; *u != '\0'; u++) {
							m += (*u);
						}

						m = m % NUM_NICK_COLORS;

						strcpy(color, nick_colors[m]);
					}
					else
						strcpy(color, RECV_COLOR);
				}
				else if (flags & GAIM_MESSAGE_SEND)
					strcpy(color, SEND_COLOR);
				else {
					gaim_debug_error("gtkconv", "message missing flags\n");
					strcpy(color, "#000000");
				}
			}
		}

		if(who_escaped)
			g_free(who_escaped);

		if (gaim_prefs_get_bool("/gaim/gtk/conversations/show_timestamps"))
			g_snprintf(buf, BUF_LONG,
				   "<HR><FONT SIZE=\"2\">(%s)</FONT> "
				   "<B><A HREF=\"http://my.gay.com/%s\"><FONT COLOR=\"%s\" %s>%s:</FONT></A></B></FONT> ", mdate, str, color,
				   sml_attrib ? sml_attrib : "", str);
		else
			g_snprintf(buf, BUF_LONG,
				   "<HR><FONT SIZE=\"2\"<B><A HREF=\"http://my.gay.com/%s\"><FONT COLOR=\"%s\" %s>%s:</FONT></A></B></FONT> ", str, color,
				   sml_attrib ? sml_attrib : "", str);

		/* FIXME: insert IMG tag in next line */
		g_snprintf(buf2, BUF_LONG,
			   "<HR><FONT SIZE=\"1\"><!--(%s) -->"
			   "<B><A HREF=\"http://my.gay.com/%s\"><FONT COLOR=\"%s\" %s>%s:</FONT></A></B></FONT>",
			   mdate, str, color, sml_attrib ? sml_attrib : "", str);

		g_free(str);

		struct LastMessage * lastconv = (struct LastMessage *) 
			g_hash_table_lookup(Conversations,conv); 

		if (lastconv == NULL) {
			gaim_debug_error ("disp","## CONV NOT FOUND !!! ##\n");
		} else {
			gaim_debug_info ("disp","who: %s lastwho: %s\n",who,lastconv->who);
		}
		if ((lastconv == NULL) || (lastconv->who == NULL) || (g_strcasecmp(who,lastconv->who))) {
			gtk_imhtml_append_text(GTK_IMHTML(gtkconv->imhtml), buf2, 0);
		}
		if(gc){
			char *pre = g_strdup_printf("<br><font color=%s><font %s>    ", color, sml_attrib ? sml_attrib : "");
			char *post = "</font></font>";
			int pre_len = strlen(pre);
			int post_len = strlen(post);

			with_font_tag = g_malloc(length + pre_len + post_len + 1);

			strcpy(with_font_tag, pre);
			memcpy(with_font_tag + pre_len, new_message, length);
			strcpy(with_font_tag + pre_len + length, post);

			length += pre_len + post_len;
			g_free(pre);
		}
		else
			with_font_tag = g_memdup(new_message, length);

		if ((lastconv == NULL) || (lastconv->msg == NULL) || (g_strcasecmp(new_message,lastconv->msg) || (g_strcasecmp(who,lastconv->who))))  {
			gtk_imhtml_append_text(GTK_IMHTML(gtkconv->imhtml),
							 with_font_tag, gtk_font_options);
			if (lastconv) {
				if (lastconv->msg) {
					g_free(lastconv->msg);
				}
				if ((lastconv->who) && (g_strcasecmp(who,lastconv->who))) {
					g_free(lastconv->who);
				}
				lastconv->msg = g_strdup(new_message);
				lastconv->who = g_strdup(who);
			}
			conv->history = g_string_append(conv->history, buf);
			conv->history = g_string_append(conv->history, new_message);
			conv->history = g_string_append(conv->history, "<BR>\n");
		}
		g_free(with_font_tag);
		g_free(new_message);
	}


	if(sml_attrib)
		g_free(sml_attrib);
}


/* Install the above */
static void disp_conv_install (GaimConversation *conv)
{
    disp_op = gaim_conversation_get_ui_ops(conv);
    
    if (disp_op->write_conv != disp_write_convo) {
        disp_gtkconv_write_conv = disp_op->write_conv;
        disp_op->write_conv = disp_write_convo;
    } else {
        gaim_debug_info ("disp_convo_created","Not Installed\n");
    }
    g_hash_table_insert (Conversations,conv,g_new0(struct LastMessage,1));

}

/* Remove a conversation on part */
static gboolean disp_conv_uninstall (GaimConversation *conv)
{
    g_hash_table_remove (Conversations,conv);
    return TRUE;
}

/* Attempts to find a conv to to call the installer above */
static gboolean disp_install_wrapper (GaimPlugin *plugin)
{
    GList *conversations = gaim_get_conversations();
    conversations = g_list_first(conversations);
    
    if (Conversations == NULL) {
	    Conversations = g_hash_table_new_full 
		    (g_direct_hash,g_direct_equal,NULL,RemoveLastMessage);
    }
    if (conversations) {
        while(conversations) {
    	    GaimConversation *conv = conversations->data;
	    if (conv->type == GAIM_CONV_CHAT)
	    {
		disp_conv_install(conv);
		return TRUE;
	    }
	    conversations = g_list_next(conversations);
        }
	return FALSE;
    } else {
	/* Are these right? */
	gaim_signal_connect(gaim_conversations_get_handle(),
	    "chat-joined", plugin, GAIM_CALLBACK(disp_conv_install), NULL);
	gaim_signal_connect(gaim_conversations_get_handle(),
	    "chat-left", plugin, GAIM_CALLBACK(disp_conv_uninstall), NULL);

	return TRUE;
    }
}

/* Uninstall the whole plugin */
static gboolean disp_uninstall (GaimPlugin *plugin)
{
    if (disp_op) {
    	if (disp_op->write_conv == disp_write_convo) {
        	disp_op->write_conv = disp_gtkconv_write_conv;
    	} 
    } else {
        	gaim_debug_info ("display plugin unload","Not Installed");
    }
    if (Conversations) {
	    g_hash_table_destroy(Conversations);
	    Conversations = NULL;
    }
    return TRUE;
}

static GaimPluginInfo info = {
    GAIM_PLUGIN_MAGIC,
    GAIM_MAJOR_VERSION,
    GAIM_MINOR_VERSION,
    GAIM_PLUGIN_STANDARD,
    GAIM_GTK_PLUGIN_TYPE,
    0,
    NULL,
    GAIM_PRIORITY_DEFAULT,
    CHATSORT_PLUGIN_ID,
    N_("Display Options"),
    VERSION,
    N_("Various changes to gaim's display properties"),
    N_("Changes message display format, adding links, display grouping, bioline highlights, smaller system messages, and removes annoying repeats!"),
    "Evan Langlois <evan@coolrunningconcepts.com>",
    GAIM_WEBSITE,
    disp_install_wrapper,
    disp_uninstall,
    NULL,
    NULL,
    NULL,
    NULL
};

static void init_plugin(GaimPlugin * plugin)
{
}

GAIM_INIT_PLUGIN(history, init_plugin, info)
