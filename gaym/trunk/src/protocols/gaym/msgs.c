 /**
 * @file msgs.c
 * 
 * gaim
 *
 * Copyright (C) 2003, Ethan Blanton <eblanton@cs.purdue.edu>
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
#include "blist.h"
#include "notify.h"
#include "util.h"
#include "debug.h"
#include "gaym.h"
#include "imgstore.h"
#include "helpers.h"

#include <stdio.h>

static char *gaym_mask_nick(const char *mask);
static char *gaym_mask_userhost(const char *mask);
static void gaym_chat_remove_buddy(GaimConversation *convo, char *data[2]);
static void gaym_buddy_status(char *name, struct gaym_buddy *ib, struct gaym_conn *gaym);

static char * gaym_mask_thumbnail(const char* biostring)
{    
  char * start = strchr(biostring,':');
  char * end=0;
  if(start)
  {   
      start++;
      end = strchr(biostring,'#');
  }
  if(start!=end && end)
  { gaim_debug_misc("gaym","Returning: %.*s\n",end-start,start);
  return g_strdup_printf("%.*s",end-start,start);
  }
  else
    return 0;
       
}

static char* gaym_mask_bio(const char* biostring)
{
  char * start = strchr(biostring,'#');
  char* end=0;
  if(start)
  {   
    start++;
    end = strchr(biostring,0x01);
    if(!end)
      end = strchr(biostring,0);
  }
  
  if(start!=end && end)
  { gaim_debug_misc("gaym","Returning: %.*s\n",end-start,start);
    return g_strdup_printf("%.*s",end-start,start);
  }
  else
    return 0;

}

static char* gaym_mask_stats(const char *biostring)   
{
  
  char * start = strchr(biostring,'#');
  int i;
  if(start)
    start = strchr(start,0x01);
 
  char* end=0;
  if(start)
  {   
    start++;
    end = strchr(biostring,'\0');
  }
  
  if(start!=end && end)
  { gaim_debug_misc("gaym","Returning: %.*s\n",end-start,start);
  return g_strdup_printf("%.*s",end-start,start);
  }
  else
    return 0;

}
static char *gaym_mask_nick(const char *mask)
{
	char *end, *buf;

	end = strchr(mask, '!');
	if (!end)
		buf = g_strdup(mask);
	else
		buf = g_strndup(mask, end - mask);

	return buf;
}

static char *gaym_mask_userhost(const char *mask)
{
	return g_strdup(strchr(mask, '!') + 1);
}

static void gaym_chat_remove_buddy(GaimConversation *convo, char *data[2])
{
	char *message = g_strdup_printf("quit: %s", data[1]);

	if (gaim_conv_chat_find_user(GAIM_CONV_CHAT(convo), data[0]))
		gaim_conv_chat_remove_user(GAIM_CONV_CHAT(convo), data[0], message);

	g_free(message);
}

void gaym_msg_default(struct gaym_conn *gaym, const char *name, const char *from, char **args)
{
	gaim_debug(GAIM_DEBUG_INFO, "gaym", "Unrecognized message: %s\n", args[0]);
}

void gaym_msg_away(struct gaym_conn *gaym, const char *name, const char *from, char **args)
{
	GaimConnection *gc;

	if (!args || !args[1])
		return;

        


	gc = gaim_account_get_connection(gaym->account);
	if (gc)
		serv_got_im(gc, args[1], args[2], GAIM_CONV_IM_AUTO_RESP, time(NULL));
}


struct gaym_fetch_thumbnail_data {
	GaimConnection *gc;
	char *who;
	char *bio;
        char *stats;
};

static 
void gaym_fetch_thumbnail_cb(void* user_data, const char* pic_data, size_t len) {
	struct gaym_fetch_thumbnail_data *d = user_data;
	//GaimBuddy *b;

	if (GAIM_CONNECTION_IS_VALID(d->gc) && len) {
		gaim_buddy_icons_set_for_user(gaim_connection_get_account(d->gc), d->who, (void *)pic_data, len);
		gaim_debug_misc("gaym","Got thumbnail for %s\n",d->who);
		//b = gaim_find_buddy(gaim_connection_get_account(d->gc), d->who);
		//if (b)
		//	gaim_blist_node_set_int((GaimBlistNode*)b, YAHOO_ICON_CHECKSUM_KEY, d->checksum);
	} else {
		gaim_debug_error("yahoo", "Fetching buddy icon failed.\n");
	}

	g_free(d->who);
	g_free(d);
		
}

static 
void gaym_fetch_photo_cb(void* user_data, const char* info_data, size_t len) {
	struct gaym_fetch_thumbnail_data *d = user_data;
	//gaim_debug_misc("gaym","In step 2 of info fetch, %.*s\n",len,info_data);			
	
	char* info;
	int id = gaim_imgstore_add(info_data, len, NULL);
	
        if(d->stats && d->bio)
          info = g_strdup_printf("%s<br>%s<br><img id=%d><a href='http://my.gay.com/%s'>Full Profile</a>",d->stats,d->bio,id,d->who);
        else if(d->stats)
          info = g_strdup_printf("%s<br><img id=%d><a href='http://my.gay.com/%s'>Full Profile</a>",d->stats,id,d->who);
        else if(d->bio)
          info = g_strdup_printf("%s<br><img id=%d><a href='http://my.gay.com/%s'>Full Profile</a>",d->bio,id,d->who);
        else
          info = g_strdup_printf("<img id=%d><a href='http://my.gay.com/%s'>Full Profile</a>",id,d->who);
              
	gaim_notify_userinfo(d->gc, d->who, NULL, "Gay.com Profile",
			     NULL, info, NULL, NULL);
	
        if(d)
        {
          if(d->who)
            g_free(d->who);
          if(d->bio)
            g_free(d->bio);
          if(d->stats)
            g_free(d->stats);
          g_free(d);
}
}
static 
void gaym_fetch_info_cb(void* user_data, const char* info_data, size_t len) {
	struct gaym_fetch_thumbnail_data *d = user_data;
	char * picpath;
	char * picurl;
	char * info;
	char * match="pictures.0.url=";
	
        
        
        if(d->stats && d->bio)
          info = g_strdup_printf("%s<br>%s<br><a href='http://my.gay.com/%s'>Full Profile</a>",d->stats,d->bio,d->who);
        else if(d->stats)
          info = g_strdup_printf("%s<br><a href='http://my.gay.com/%s'>Full Profile</a>",d->stats,d->who);
        else if(d->bio)
          info = g_strdup_printf("%s<br><a href='http://my.gay.com/%s'>Full Profile</a>",d->bio,d->who);
        else
          info = g_strdup_printf("<a href='http://my.gay.com/%s'>Full Profile</a>",d->who);
        
        picpath=return_string_between(match,"\n",info_data);
        gaim_debug_misc("gaym","picpath: %s\n",picpath);
        if(!picpath || strlen(picpath)==0) 
        {    gaim_notify_userinfo(d->gc, d->who, d->who, "Gay.com Profile",
                             NULL, info, NULL, NULL);
                             return;
        }
                                             
        picurl=g_strdup_printf("http://www.gay.com%s",picpath);
		if(picurl)
                {
                        
			gaim_url_fetch(picurl, FALSE, "Mozilla/4.0 (compatible; MSIE 5.0)", FALSE, gaym_fetch_photo_cb, user_data);
			return;
			
			
		}
	
	
	

	
	
}
void gaym_msg_no_such_nick(struct gaym_conn *gaym, const char *name, const char *from, char **args) {
	gaym->info_window_needed=0;
	char* buf;
	//gaim_debug_misc("gaym","Args 0: %s 1: %s 2: %s 3: %s\n",args[0],args[1],args[2]);
	buf = g_strdup_printf("That user is not logged on. Check <a href='http://my.gay.com/%s'>here</a> to see if that user has a profile.",args[1]);
	gaim_notify_userinfo(gaim_account_get_connection(gaym->account), NULL, NULL, "No such user",
			     NULL, buf, NULL, NULL);
	
}
void gaym_msg_whois(struct gaym_conn *gaym, const char *name, const char *from, char **args)
{
  int i;
  for(i=0; i<strlen(args[1]); i++)
    if(args[1][i]=='|')
      args[1][i]='.';
        
	if (!gaym->whois.nick) {
		gaim_debug(GAIM_DEBUG_WARNING, "gaym", "Unexpected WHOIS reply for %s\n", args[1]);
		return;
	}

        
	if (gaim_utf8_strcasecmp(gaym->whois.nick, args[1])) {
		gaim_debug(GAIM_DEBUG_WARNING, "gaym", "Got WHOIS reply for %s while waiting for %s\n", args[1], gaym->whois.nick);
		return;
	}


        
	struct gaym_fetch_thumbnail_data *data, *data2;
	
        data = g_new0(struct gaym_fetch_thumbnail_data, 1);
	data2 = g_new0(struct gaym_fetch_thumbnail_data, 1);
	data->gc = gaim_account_get_connection(gaym->account);
	data->who = g_strdup(gaym->whois.nick);
	
	data2->gc = gaim_account_get_connection(gaym->account);
	data2->who = g_strdup(gaym->whois.nick);
	
        char* thumbpath=gaym_mask_thumbnail(args[5]);
        data2->bio=gaym_mask_bio(args[5]);
        data2->stats=gaym_mask_stats(args[5]);
        
        gaim_debug_misc("gaym","thumbpath: %s, bio: %s, stats: %s\n",thumbpath,data2->bio,data2->stats);
	char* thumburl=g_strdup_printf("http://www.gay.com/images/personals/pictures%s>",thumbpath);
	if(thumburl)	
	{	
		gaim_url_fetch(thumburl, FALSE, "Mozilla/4.0 (compatible; MSIE 5.0)", FALSE,
	       		gaym_fetch_thumbnail_cb, data);
		g_free(thumburl);
	}
	
	if(gaym->info_window_needed==TRUE) {
		gaym->info_window_needed=0;
		char* infourl = g_strdup_printf("http://www.gay.com/messenger/get-profile.txt?pw=%s&name=%s",gaym->hash_pw,gaym->whois.nick);
	
		if(infourl)
		{	
			gaim_url_fetch(infourl, FALSE, "Mozilla/4.0 (compatible; MSIE 5.0)", FALSE, gaym_fetch_info_cb, 	data2);
			g_free(infourl);
		}
	}

	

	
}

void gaym_msg_unknown(struct gaym_conn *gaym, const char *name, const char *from, char **args)
{
	GaimConnection *gc = gaim_account_get_connection(gaym->account);
	char *buf;

	if (!args || !args[1] || !gc)
		return;

	buf = g_strdup_printf(_("Unknown message '%s'"), args[1]);
	gaim_notify_error(gc, _("Unknown message"), buf, _("Gaim has sent a message the IRC server did not understand."));
	g_free(buf);
}

void gaym_msg_names(struct gaym_conn *gaym, const char *name, const char *from, char **args)
{
	char *names, *cur, *end, *tmp, *msg;
	GaimConversation *convo;

	if (!strcmp(name, "366")) {
		convo = gaim_find_conversation_with_account(gaym->nameconv ? gaym->nameconv : args[1], gaym->account);
		if (!convo) {
			gaim_debug(GAIM_DEBUG_ERROR, "gaym", "Got a NAMES list for %s, which doesn't exist\n", args[2]);
			g_string_free(gaym->names, TRUE);
			gaym->names = NULL;
			g_free(gaym->nameconv);
			gaym->nameconv = NULL;
			return;
		}

		names = cur = g_string_free(gaym->names, FALSE);
		gaym->names = NULL;
		if (gaym->nameconv) {
			msg = g_strdup_printf(_("Users on %s: %s"), args[1], names);
			if (gaim_conversation_get_type(convo) == GAIM_CONV_CHAT)
				gaim_conv_chat_write(GAIM_CONV_CHAT(convo), "", msg, GAIM_MESSAGE_SYSTEM|GAIM_MESSAGE_NO_LOG, time(NULL));
			else
				gaim_conv_im_write(GAIM_CONV_IM(convo), "", msg, GAIM_MESSAGE_SYSTEM|GAIM_MESSAGE_NO_LOG, time(NULL));
			g_free(msg);
			g_free(gaym->nameconv);
			gaym->nameconv = NULL;
		} else {
			GList *users = NULL;
			GList *flags = NULL;

			while (*cur) {
				GaimConvChatBuddyFlags f = GAIM_CBFLAGS_NONE;
				end = strchr(cur, ' ');
				if (!end)
					end = cur + strlen(cur);
				if (*cur == '@') {
					f = GAIM_CBFLAGS_OP;
					cur++;
				} else if (*cur == '%') {
					f = GAIM_CBFLAGS_HALFOP;
					cur++;
				} else if(*cur == '+') {
					f = GAIM_CBFLAGS_VOICE;
					cur++;
				}
				tmp = g_strndup(cur, end - cur);
				users = g_list_append(users, tmp);
				flags = g_list_append(flags, GINT_TO_POINTER(f));
				cur = end;
				if (*cur)
					cur++;
			}

			if (users != NULL) {
				GList *l;

				gaim_conv_chat_add_users(GAIM_CONV_CHAT(convo), users, flags);

				for (l = users; l != NULL; l = l->next)
					g_free(l->data);

				g_list_free(users);
				g_list_free(flags);
			}
		}
		g_free(names);
	} else {
		if (!gaym->names)
			gaym->names = g_string_new("");

		gaym->names = g_string_append(gaym->names, args[3]);
	}
}


//Change this to WELCOME

void gaym_msg_endmotd(struct gaym_conn *gaym, const char *name, const char *from, char **args)
{
	GaimConnection *gc;

	gc = gaim_account_get_connection(gaym->account);
	if (!gc)
		return;

	gaim_connection_set_state(gc, GAIM_CONNECTED);
	serv_finish_login (gc);

	gaym_blist_timeout(gaym);
	if (!gaym->timer)
		gaym->timer = gaim_timeout_add(BLIST_UPDATE_PERIOD, (GSourceFunc)gaym_blist_timeout, (gpointer)gaym);
}

void gaym_msg_nochan(struct gaym_conn *gaym, const char *name, const char *from, char **args)
{
	GaimConnection *gc = gaim_account_get_connection(gaym->account);

	if (gc == NULL || args == NULL || args[1] == NULL)
		return;

	gaim_notify_error(gc, NULL, _("No such channel"), args[1]);
}

void gaym_msg_nonick(struct gaym_conn *gaym, const char *name, const char *from, char **args)
{
	GaimConnection *gc;
	GaimConversation *convo;

	convo = gaim_find_conversation_with_account(args[1], gaym->account);
	if (convo) {
		if (gaim_conversation_get_type(convo) == GAIM_CONV_CHAT) /* does this happen? */
			gaim_conv_chat_write(GAIM_CONV_CHAT(convo), args[1], _("no such channel"),
					GAIM_MESSAGE_SYSTEM|GAIM_MESSAGE_NO_LOG, time(NULL));
		else
			gaim_conv_im_write(GAIM_CONV_IM(convo), args[1], _("User is not logged in"),
				      GAIM_MESSAGE_SYSTEM|GAIM_MESSAGE_NO_LOG, time(NULL));
	} else {
		if ((gc = gaim_account_get_connection(gaym->account)) == NULL)
			return;
		gaim_notify_error(gc, NULL, _("No such nick or channel"), args[1]);
	}

	if (gaym->whois.nick && !gaim_utf8_strcasecmp(gaym->whois.nick, args[1])) {
		g_free(gaym->whois.nick);
		gaym->whois.nick = NULL;
	}
}

void gaym_msg_nosend(struct gaym_conn *gaym, const char *name, const char *from, char **args)
{
	GaimConnection *gc;
	GaimConversation *convo;

	convo = gaim_find_conversation_with_account(args[1], gaym->account);
	if (convo) {
		gaim_conv_chat_write(GAIM_CONV_CHAT(convo), args[1], args[2], GAIM_MESSAGE_SYSTEM|GAIM_MESSAGE_NO_LOG, time(NULL));
	} else {
		if ((gc = gaim_account_get_connection(gaym->account)) == NULL)
			return;
		gaim_notify_error(gc, NULL, _("Could not send"), args[2]);
	}
}

//Is this used?
void gaym_msg_notinchan(struct gaym_conn *gaym, const char *name, const char *from, char **args)
{
	GaimConversation *convo = gaim_find_conversation_with_account(args[1], gaym->account);

	gaim_debug(GAIM_DEBUG_INFO, "gaym", "We're apparently not in %s, but tried to use it\n", args[1]);
	if (convo) {
		/*g_slist_remove(gaym->gc->buddy_chats, convo);
		  gaim_conversation_set_account(convo, NULL);*/
		gaim_conv_chat_write(GAIM_CONV_CHAT(convo), args[1], args[2], GAIM_MESSAGE_SYSTEM|GAIM_MESSAGE_NO_LOG, time(NULL));
	}
}


//Invite WORKS in gay.com!
void gaym_msg_invite(struct gaym_conn *gaym, const char *name, const char *from, char **args)
{
	GaimConnection *gc = gaim_account_get_connection(gaym->account);
	GHashTable *components = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
	char *nick = gaym_mask_nick(from);

	if (!args || !args[1] || !gc) {
		g_free(nick);
		g_hash_table_destroy(components);
		return;
	}

	g_hash_table_insert(components, strdup("channel"), strdup(args[1]));

	serv_got_chat_invite(gc, args[1], nick, NULL, components);
	g_free(nick);
}


void gaym_msg_inviteonly(struct gaym_conn *gaym, const char *name, const char *from, char **args)
{
	GaimConnection *gc = gaim_account_get_connection(gaym->account);
	char *buf;

	if (!args || !args[1] || !gc)
		return;

	buf = g_strdup_printf(_("Joining %s requires an invitation."), args[1]);
	gaim_notify_error(gc, _("Invitation only"), _("Invitation only"), buf);
	g_free(buf);
}



void gaym_msg_ison(struct gaym_conn *gaym, const char *name, const char *from, char **args)
{
	char **nicks;
	struct gaym_buddy *ib;
	int i;

	if (!args || !args[1])
		return;
        for(i=0; i<strlen(args[1]); i++)
          if(args[1][i]=='|')
            args[1][i]='.';
	nicks = g_strsplit(args[1], " ", -1);

	for (i = 0; nicks[i]; i++) {
		if ((ib = g_hash_table_lookup(gaym->buddies, (gconstpointer)nicks[i])) == NULL) {
			continue;
		}
		ib->flag=TRUE; 
		
	}

	g_strfreev(nicks);

	
	g_hash_table_foreach(gaym->buddies, (GHFunc)gaym_buddy_status, (gpointer)gaym);
	

	
		
	
	
}


static void gaym_buddy_status(char *name, struct gaym_buddy *ib, struct gaym_conn *gaym)
{
	GaimConnection *gc = gaim_account_get_connection(gaym->account);
	GaimBuddy *buddy = gaim_find_buddy(gaym->account, name);

	if (!gc || !buddy || !ib->stale)
		return;

	if (ib->online && !ib->flag) {
		serv_got_update(gc, buddy->name, FALSE, 0, 0, 0, 0);
		ib->online = FALSE;
		
		
	}

	if (!ib->online && ib->flag) {
		serv_got_update(gc, buddy->name, TRUE, 0, 0, 0, 0);
		ib->online = TRUE;
		ib->flag=FALSE;

		
		
	}
	ib->stale = FALSE;
	ib->flag = FALSE;
}

void gaym_msg_join(struct gaym_conn *gaym, const char *name, const char *from, char **args)
{
	GaimConnection *gc = gaim_account_get_connection(gaym->account);
	GaimConversation *convo;
	char *nick = gaym_mask_nick(from), *bio="Null";
	struct gaym_buddy *ib;
	static int id = 1;

	if (!gc) {
		g_free(nick);
		return;
	}
	
	
	

	if (!gaim_utf8_strcasecmp(nick, gaim_connection_get_display_name(gc))) {
		/* We are joining a channel for the first time */
		serv_got_joined_chat(gc, id++, args[0]);
		g_free(nick);
		return;
	}

	convo = gaim_find_conversation_with_account(args[0], gaym->account);
	if (convo == NULL) {
		gaim_debug(GAIM_DEBUG_ERROR, "gaym", "JOIN for %s failed\n", args[0]);
		g_free(nick);
		return;
	}

	
        bio=gaym_mask_bio(args[1]);
        gaim_debug_misc("gaym","Join args: 0=%s, 1=%s, 2=%s\n",args[0],args[1],args[2]);
        gaim_debug_misc("gaym","Join from: %s\n",from);
	gaim_conv_chat_add_user(GAIM_CONV_CHAT(convo), nick, bio, GAIM_CBFLAGS_NONE, TRUE);

	
	if ((ib = g_hash_table_lookup(gaym->buddies, nick)) != NULL) {
		ib->flag = TRUE;
		gaym_buddy_status(nick, ib, gaym);
	}

	g_free(bio);
	g_free(nick);
}



void gaym_msg_mode(struct gaym_conn *gaym, const char *name, const char *from, char **args)
{
	GaimConversation *convo;
	char *nick = gaym_mask_nick(from), *buf;

	if (*args[0] == '#' || *args[0] == '&') {	/* Channel	*/
		convo = gaim_find_conversation_with_account(args[0], gaym->account);
		if (!convo) {
			gaim_debug(GAIM_DEBUG_ERROR, "gaym", "MODE received for %s, which we are not in\n", args[0]);
			g_free(nick);
			return;
		}
		buf = g_strdup_printf(_("mode (%s %s) by %s"), args[1], args[2] ? args[2] : "", nick);
		gaim_conv_chat_write(GAIM_CONV_CHAT(convo), args[0], buf, GAIM_MESSAGE_SYSTEM|GAIM_MESSAGE_NO_LOG, time(NULL));
		g_free(buf);
		if(args[2]) {
			GaimConvChatBuddyFlags newflag, flags;
			char *mcur, *cur, *end, *user;
			gboolean add = FALSE;
			mcur = args[1];
			cur = args[2];
			while (*cur && *mcur) {
				if ((*mcur == '+') || (*mcur == '-')) {
					add = (*mcur == '+') ? TRUE : FALSE;
					mcur++;
					continue;
				}
				end = strchr(cur, ' ');
				if (!end)
					end = cur + strlen(cur);
				user = g_strndup(cur, end - cur);
				flags = gaim_conv_chat_user_get_flags(GAIM_CONV_CHAT(convo), user);
				newflag = GAIM_CBFLAGS_NONE;
				if (*mcur == 'o')
					newflag = GAIM_CBFLAGS_OP;
				else if (*mcur =='h')
					newflag = GAIM_CBFLAGS_HALFOP;
				else if (*mcur == 'v')
					newflag = GAIM_CBFLAGS_VOICE;
				if (newflag) {
					if (add)
						flags |= newflag;
					else
						flags &= ~newflag;
					gaim_conv_chat_user_set_flags(GAIM_CONV_CHAT(convo), user, flags);
				}
				g_free(user);
				cur = end;
				if (*cur)
					cur++;
				if (*mcur)
					mcur++;
			}
		}
	} else {					/* User		*/
	}
	g_free(nick);
}
                                                        
void gaym_msg_nick(struct gaym_conn *gaym, const char *name, const char *from, char **args)
{
	GaimConnection *gc = gaim_account_get_connection(gaym->account);
	GSList *chats;
	char *nick = gaym_mask_nick(from);

	if (!gc) {
		g_free(nick);
		return;
	}
	chats = gc->buddy_chats;

	if (!gaim_utf8_strcasecmp(nick, gaim_connection_get_display_name(gc))) {
		gaim_connection_set_display_name(gc, args[0]);
	}

	while (chats) {
		GaimConvChat *chat = GAIM_CONV_CHAT(chats->data);
		/* This is ugly ... */
		if (gaim_conv_chat_find_user(chat, nick))
			gaim_conv_chat_rename_user(chat, nick, args[0]);
		chats = chats->next;
	}
	g_free(nick);
}

       
         
void gaym_msg_notice(struct gaym_conn *gaym, const char *name, const char *from, char **args)
{
	char *newargs[2];

	newargs[0] = " notice ";	/* The spaces are magic, leave 'em in! */
	newargs[1] = args[1];
	gaym_msg_privmsg(gaym, name, from, newargs);
}


void gaym_msg_part(struct gaym_conn *gaym, const char *name, const char *from, char **args)
{
	GaimConnection *gc = gaim_account_get_connection(gaym->account);
	GaimConversation *convo;
	char *nick, *msg;

	if (!args || !args[0] || !gc)
		return;

	convo = gaim_find_conversation_with_account(args[0], gaym->account);
	
	nick = gaym_mask_nick(from);
	if (!gaim_utf8_strcasecmp(nick, gaim_connection_get_display_name(gc))) {
		msg = g_strdup_printf(_("You have parted the channel%s%s"),
                                      (args[1] && *args[1]) ? ": " : "", args[1]);
		gaim_conv_chat_write(GAIM_CONV_CHAT(convo), args[0], msg, GAIM_MESSAGE_SYSTEM, time(NULL));
		g_free(msg);
		serv_got_chat_left(gc, gaim_conv_chat_get_id(GAIM_CONV_CHAT(convo)));
	} else {
		gaim_conv_chat_remove_user(GAIM_CONV_CHAT(convo), nick, args[1]);
	}
	g_free(nick);
}

void gaym_msg_ping(struct gaym_conn *gaym, const char *name, const char *from, char **args)
{
	char *buf;
	if (!args || !args[0])
		return;

	buf = gaym_format(gaym, "v:", "PONG", args[0]);
	gaym_send(gaym, buf);
	g_free(buf);
}

void gaym_msg_pong(struct gaym_conn *gaym, const char *name, const char *from, char **args)
{
	GaimConversation *convo;
	GaimConnection *gc;
	char **parts, *msg;
	time_t oldstamp;

	if (!args || !args[1])
		return;

	parts = g_strsplit(args[1], " ", 2);

	if (!parts[0] || !parts[1]) {
		g_strfreev(parts);
		return;
	}

	if (sscanf(parts[1], "%lu", &oldstamp) != 1) {
		msg = g_strdup(_("Error: invalid PONG from server"));
	} else {
		msg = g_strdup_printf(_("PING reply -- Lag: %lu seconds"), time(NULL) - oldstamp);
	}

	convo = gaim_find_conversation_with_account(parts[0], gaym->account);
	g_strfreev(parts);
	if (convo) {
		if (gaim_conversation_get_type (convo) == GAIM_CONV_CHAT)
			gaim_conv_chat_write(GAIM_CONV_CHAT(convo), "PONG", msg, GAIM_MESSAGE_SYSTEM|GAIM_MESSAGE_NO_LOG, time(NULL));
		else
			gaim_conv_im_write(GAIM_CONV_IM(convo), "PONG", msg, GAIM_MESSAGE_SYSTEM|GAIM_MESSAGE_NO_LOG, time(NULL));
	} else {
		gc = gaim_account_get_connection(gaym->account);
		if (!gc) {
			g_free(msg);
			return;
		}
		gaim_notify_info(gc, NULL, "PONG", msg);
	}
	g_free(msg);
}

void gaym_msg_privmsg(struct gaym_conn *gaym, const char *name, const char *from, char **args)
{
	GaimConnection *gc = gaim_account_get_connection(gaym->account);
	GaimConversation *convo;
	char *nick = gaym_mask_nick(from), *tmp, *msg;
	int notice = 0;

	if (!args || !args[0] || !args[1] || !gc) {
		g_free(nick);
		return;
	}

	notice = !strcmp(args[0], " notice ");
	tmp = gaym_parse_ctcp(gaym, nick, args[0], args[1], notice);
	if (!tmp) {
		g_free(nick);
		return;
	}

	msg = gaim_escape_html(tmp);
	g_free(tmp);

	
	if (notice) {
		tmp = g_strdup_printf("(notice) %s", msg);
		g_free(msg);
		msg = tmp;
	}

	if (!gaim_utf8_strcasecmp(args[0], gaim_connection_get_display_name(gc))) {
		serv_got_im(gc, nick, msg, 0, time(NULL));
	} else if (notice) {
		serv_got_im(gc, nick, msg, 0, time(NULL));
	} else {
		convo = gaim_find_conversation_with_account(args[0], gaym->account);
		if (convo)
			serv_got_chat_in(gc, gaim_conv_chat_get_id(GAIM_CONV_CHAT(convo)), nick, 0, msg, time(NULL));
		else
			gaim_debug(GAIM_DEBUG_ERROR, "gaym", "Got a PRIVMSG on %s, which does not exist\n", args[0]);
	}
	g_free(msg);
	g_free(nick);
}

void gaym_msg_regonly(struct gaym_conn *gaym, const char *name, const char *from, char **args)
{
	GaimConnection *gc = gaim_account_get_connection(gaym->account);
	char *msg;

	if (!args || !args[1] || !args[2] || !gc)
		return;

	msg = g_strdup_printf(_("Cannot join %s:"), args[1]);
	gaim_notify_error(gc, _("Cannot join channel"), msg, args[2]);
	g_free(msg);
}

void gaym_msg_quit(struct gaym_conn *gaym, const char *name, const char *from, char **args)
{
	GaimConnection *gc = gaim_account_get_connection(gaym->account);
	struct gaym_buddy *ib;
	char *data[2];

	if (!args || !args[0] || !gc)
		return;

	data[0] = gaym_mask_nick(from);
	data[1] = args[0];
	/* XXX this should have an API, I shouldn't grab this directly */
	g_slist_foreach(gc->buddy_chats, (GFunc)gaym_chat_remove_buddy, data);

	if ((ib = g_hash_table_lookup(gaym->buddies, data[0])) != NULL) {
		ib->flag = FALSE;
		gaym_buddy_status(data[0], ib, gaym);
	}
	g_free(data[0]);

	return;
}



void gaym_msg_pay_channel (struct gaym_conn *gaym, const char *name, const char *from, char **args)
{
       GaimConnection *gc = gaim_account_get_connection(gaym->account);
       char *buf;

       if (!args || !args[1] || !gc)
             return;

       buf = g_strdup_printf(_("The channel %s is for paying members only."), args[1]); 
       gaim_notify_error(gc, _("Pay Only"), _("Pay Only"), buf);
       g_free(buf);
}


void gaym_msg_toomany_channels (struct gaym_conn *gaym, const char *name, const char *from, char **args)
{
       GaimConnection *gc = gaim_account_get_connection(gaym->account);
       char *buf;

       if (!args || !args[1] || !gc)
             return;

       buf = g_strdup_printf(_("You have joined too many channels the maximum is (2). You cannot join channel %s. Part another channel first ."), args[1]);
       gaim_notify_error(gc, _("Maximum ChannelsReached"), _("Maximum ChannelsReached"), buf);
       g_free(buf);
}


void gaym_msg_richnames_list(struct gaym_conn *gaym, const char *name, const char *from, char **args)
{
       GaimConnection *gc = gaim_account_get_connection(gaym->account);
       GaimConversation *convo;
       char *channel = args[1];

       if (!gc) {
         return;
       }

       /* GAYMDIFF: Might want to make "gaym" a #define or something */
       gaim_debug (GAIM_DEBUG_INFO, "gaym", "gaym_msg_richnames_list() Channel: %s\n", channel);

       convo = gaim_find_conversation_with_account(channel, gaym->account);

       if (convo == NULL) {
               gaim_debug (GAIM_DEBUG_ERROR, "gaym", "690 for %s failed\n", args[1]);
               return;
       }

       /* GAYMDIFF: what is args[2]?  Need to make it a descriptive var */
       gaim_conv_chat_add_user(GAIM_CONV_CHAT(convo), args[2], NULL, GAIM_CBFLAGS_NONE, FALSE);
}
