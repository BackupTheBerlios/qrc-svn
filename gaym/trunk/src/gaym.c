/**
 * @file gaym.c
 *
 * gaim
 *
 * Copyright (C) 2003, Robbert Haarman <gaim@inglorion.net>
 * Copyright (C) 2003, Ethan Blanton <eblanton@cs.purdue.edu>
 * Copyright (C) 2000-2003, Rob Flynn <rob@tgflinux.com>
 * Copyright (C) 1998-1999, Mark Spencer <markster@marko.net>
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

#include "accountopt.h"
#include "blist.h"
#include "conversation.h"
#include "debug.h"
#include "notify.h"
#include "prpl.h"
#include "plugin.h"
#include "util.h"
#include "version.h"
#include "helpers.h"



#include "gaym.h"

char *gaym_mask_bio(const char *biostring);
static const char *gaym_blist_icon(GaimAccount *a, GaimBuddy *b);
static void gaym_blist_emblems(GaimBuddy *b, char **se, char **sw, char **nw, char **ne);
static GList *gaym_away_states(GaimConnection *gc);
static GList *gaym_actions(GaimPlugin *plugin, gpointer context);
/* static GList *gaym_chat_info(GaimConnection *gc); */
static void gaym_login(GaimAccount *account);
static void gaym_login_cb(gpointer data, gint source, GaimInputCondition cond);
static void gaym_close(GaimConnection *gc);
static int gaym_im_send(GaimConnection *gc, const char *who, const char *what, GaimConvImFlags flags);
static int gaym_chat_send(GaimConnection *gc, int id, const char *what);
static void gaym_chat_join (GaimConnection *gc, GHashTable *data);
static void gaym_input_cb(gpointer data, gint source, GaimInputCondition cond);

static guint gaym_nick_hash(const char *nick);
static gboolean gaym_nick_equal(const char *nick1, const char *nick2);
static void gaym_buddy_free(struct gaym_buddy *ib);

static void gaym_buddy_append(char *name, struct gaym_buddy *ib, GString *string);
static void gaym_buddy_clear_done(char *name, struct gaym_buddy *ib, gpointer nothing);
	
static GaimPlugin *_gaym_plugin = NULL;

static const char *status_chars = "@+%&";

int gaym_send(struct gaym_conn *gaym, const char *buf)
{
	int ret;

	if (gaym->fd < 0)
		return -1;

	/* gaim_debug(GAIM_DEBUG_MISC, "gaym", "sent: %s", buf); */
	if ((ret = write(gaym->fd, buf, strlen(buf))) < 0)
		gaim_connection_error(gaim_account_get_connection(gaym->account),
				      _("Server has disconnected"));

	return ret;
}

/* XXX I don't like messing directly with these buddies */
gboolean gaym_blist_timeout(struct gaym_conn *gaym)
{
	GString *string = g_string_sized_new(512);
	char *list, *buf;

	g_hash_table_foreach(gaym->buddies, (GHFunc)gaym_buddy_append, (gpointer)string);

	list = g_string_free(string, FALSE);
	if (!list || !strlen(list)) {
		g_hash_table_foreach(gaym->buddies, (GHFunc)gaym_buddy_clear_done, NULL);
		gaim_timeout_remove(gaym->timer);
		gaym->timer = gaim_timeout_add(BLIST_UPDATE_PERIOD, (GSourceFunc)gaym_blist_timeout, (gpointer)gaym);
		g_free(list);
		
		return TRUE;
	}
	gaym->blist_updating=TRUE;
	buf = gaym_format(gaym, "vn", "ISON", list);
	gaym_send(gaym, buf);
	gaim_timeout_remove(gaym->timer);
	gaym->timer = gaim_timeout_add(BLIST_CHUNK_INTERVAL, (GSourceFunc)gaym_blist_timeout, (gpointer)gaym);
		
	g_free(buf);
	
	g_free(list);
	

	return TRUE;
}

static void gaym_buddy_clear_done(char *name, struct gaym_buddy *ib, gpointer nothing)
{
	ib->done=FALSE;
}
static void gaym_buddy_append(char *name, struct gaym_buddy *ib, GString *string)
{
	char* converted_name;
	if((strlen(name) + string->len) > CHUNK_SIZE)
		return;
	else if (ib->done == FALSE)
	{
		ib->stale = TRUE;
		ib->done = TRUE;
		ib->flag = FALSE;
		converted_name=g_strdup(name);
		gaym_convert_nick_to_gaycom(converted_name);
		g_string_append_printf(string, "%s ", converted_name);
		return;
	}
	
}



static void gaym_ison_one(struct gaym_conn *gaym, struct gaym_buddy *ib)
{
	char *buf;
	char *nick;
	ib->flag = FALSE;
	nick=g_strdup(ib->name);
	gaym_convert_nick_to_gaycom(nick);
	buf = gaym_format(gaym, "vn", "ISON", nick);
	gaym_send(gaym, buf);
	g_free(buf);
}


static const char *gaym_blist_icon(GaimAccount *a, GaimBuddy *b)
{
	return "gaym";
}

static void gaym_blist_emblems(GaimBuddy *b, char **se, char **sw, char **nw, char **ne)
{
	if (b->present == GAIM_BUDDY_OFFLINE)
		*se = "offline";
}

static GList *gaym_away_states(GaimConnection *gc)
{
	return g_list_append(NULL, (gpointer)GAIM_AWAY_CUSTOM);
}

static void gaym_set_info(GaimConnection *gc, const char* info) {

	struct gaym_conn *gaym=gc->proto_data;
	GaimAccount* account = gaim_connection_get_account(gc);
	char* hostname="none";
	char* buf, *bioline;

	if (gaym->bio)
			g_free (gaym->bio);

	if (info && strlen(info) > 2) {
			gaim_debug_misc("gaym","option1, info=%x\n",info);
			gaym->bio = g_strdup(info);
	} else if (gaym->server_bioline && strlen(gaym->server_bioline) > 2) {
			gaim_debug_misc("gaym","option2\n");
			gaym->bio = g_strdup(gaym_mask_bio(gaym->server_bioline));
	} else {
			gaim_debug_misc("gaym","option3\n");
			gaym->bio = g_strdup("Gaim User");
	}

	gaim_account_set_user_info(account, gaym->bio);
	gaim_account_set_string(account, "bioline",gaym->bio);
	gaim_debug_info ("gaym","INFO=%x BIO=%x\n",info,gaym->bio);
	gaim_debug_misc("gaym","In login_cb, gc->account=%x\n",gc->account);
	bioline=g_strdup_printf("%s#%s", gaym->thumbnail?gaym->thumbnail:"",
					gaym->bio?gaym->bio:"");

	buf = gaym_format(gaym, "vvvv:", "USER",
					gaim_account_get_username(account),
					hostname, gaym->server, bioline);

	gaim_debug_misc("gaym","BIO=%x\n",bioline);
	g_free (bioline);

	if (gaym_send(gaym, buf) < 0) {
			gaim_connection_error(gc, "Error registering with server");
			return;
	}
}
static void gaym_show_set_info(GaimPluginAction *action)
{
	GaimConnection *gc = (GaimConnection *) action->context;
	gaim_account_request_change_user_info(gaim_connection_get_account(gc));
}
static GList *gaym_actions(GaimPlugin *plugin, gpointer context)
{
	GList *list = NULL;
	GaimPluginAction *act = NULL;

	act = gaim_plugin_action_new(_("Change Bio"), gaym_show_set_info);
	list = g_list_append(list, act);

	return list;
}

#if 0
static GList *gaym_blist_node_menu(GaimBlistNode *node)
{
	GList *m = NULL;
	GaimBlistNodeAction *act;

	return m;
}
#endif

static GList *gaym_chat_join_info(GaimConnection *gc)
{
	GList *m = NULL;
	struct proto_chat_entry *pce;

	pce = g_new0(struct proto_chat_entry, 1);
	pce->label = _("_Room:");
	pce->identifier = "name";
	m = g_list_append(m, pce);

	return m;
}

GHashTable *gaym_chat_info_defaults(GaimConnection *gc, const char *chat_name)
{
	GHashTable *defaults;

	defaults = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, g_free);

	if (chat_name != NULL)
		g_hash_table_insert(defaults, "channel", g_strdup(chat_name));

	return defaults;
}

static void gaym_login_with_hash(GaimAccount *account)
{
	GaimConnection *gc;
	struct gaym_conn *gaym;
	char *buf;
	const char *username = gaim_account_get_username(account);
	int err;

	gc = gaim_account_get_connection(account);
	gaym = gc->proto_data;

	buf = g_strdup_printf(_("Signon: %s"), username);
	gaim_connection_update_progress(gc, buf, 5, 6);
	g_free(buf);
	gaim_debug_misc("gaym","Trying login to %s\n",gaym->server);
	err = gaim_proxy_connect(account, gaym->server, 
				 gaim_account_get_int(account, "port", IRC_DEFAULT_PORT),
				 gaym_login_cb, gc);
	if (err || !account->gc) {
		gaim_connection_error(gc, _("Couldn't create socket"));
		gaim_debug_misc("gaym","err: %d, account->gc: %x\n",err,account->gc);
		return;
	}

}

static void gaym_login(GaimAccount *account) {
	GaimConnection *gc;
	struct gaym_conn *gaym;
	char *buf;
	const char *username = gaim_account_get_username(account);

	gc = gaim_account_get_connection(account);
	gc->flags |= GAIM_CONNECTION_NO_NEWLINES | GAIM_CONNECTION_AUTO_RESP;

	if (strpbrk(username, " \t\v\r\n") != NULL) {
		gaim_connection_error(gc, _("IRC nicks may not contain whitespace"));
		return;
	}

	gc->proto_data = gaym = g_new0(struct gaym_conn, 1);
	gaym->account = account;

	
	//gaim_connection_set_display_name(gc, userparts[0]);
	gaim_connection_set_display_name(gc,username);
	gaym->server = g_strdup(gaim_account_get_string(account, "server", "www.gay.com"));
	//gaym->server = "www.gay.com";
	gaym->buddies = g_hash_table_new_full((GHashFunc)gaym_nick_hash, (GEqualFunc)gaym_nick_equal, 
					     NULL, (GDestroyNotify)gaym_buddy_free);
	gaym->cmds = g_hash_table_new(g_str_hash, g_str_equal);
	gaym_cmd_table_build(gaym);
	gaym->msgs = g_hash_table_new(g_str_hash, g_str_equal);
	gaym_msg_table_build(gaym);

	buf = g_strdup_printf(_("Signon: %s"), username);
	gaim_connection_update_progress(gc, buf, 1, 6);
	g_free(buf);

	gaym_get_hash_from_weblogin(account, gaym_login_with_hash);


}
static
GaimChat* gaym_find_chat_by_ircname(GaimAccount* account, char* name) {

	char *chat_name;
	GaimChat *chat;
	GaimPluginProtocolInfo *prpl_info = NULL;
	GaimBlistNode *node, *group;

	GaimBuddyList* gaimbuddylist = gaim_get_blist();

	g_return_val_if_fail(gaimbuddylist != NULL, NULL);
	g_return_val_if_fail((name != NULL) && (*name != '\0'), NULL);

	if (!gaim_account_is_connected(account))
		return NULL;



	for (group = gaimbuddylist->root; group != NULL; group = group->next) {
		for (node = group->child; node != NULL; node = node->next) {
			if (GAIM_BLIST_NODE_IS_CHAT(node)) {

				chat = (GaimChat*)node;

				if (account != chat->account)
					continue;

				chat_name = g_hash_table_lookup(chat->components,
											"ircname");

				if (chat->account == account && chat_name != NULL &&
					name != NULL && !strcmp(chat_name, name)) {

					return chat;
				}
			}
		}
	}

	return NULL;
}

static void gaym_get_roomlist_from_text(gpointer proto_data) {
  
  int current_level=0;
  char * list_position=NULL;
  char * field_end=NULL;
  const char* match="roomlist=|";
  size_t field_len=0;
  char * name=NULL;
  char * num=NULL;
  GaimRoomlistRoom *room = NULL;
  GaimRoomlistRoom *current_parent = NULL;
  GaimRoomlistRoom *last_room=NULL;
  
  int level;
  
  
  struct gaym_conn* gaym=(struct gaym_conn*)proto_data;
  int num_rooms=0;
   
  
  if(!gaym->configtxt)
  {
    gaim_debug_misc("gaym","Room list parsing error: No config webpage\n");
           
    return;
  }
  list_position=strstr(gaym->configtxt,match);
  if(!list_position)
  {
    gaim_debug_misc("gaym","Room list parsing error: No roomlist found\n");
           
    return;
  }
  
  while((list_position=strstr(list_position,"\n|")) && num_rooms < 50)
  {   
    //This num_rooms stuff is just to limit the list during debugging.   
    //num_rooms++;
    level=0; 
    list_position+=2;
    
    //This is a room
    if(*list_position=='#')
    {
      
      //First, parse the room number
      field_end=strchr(list_position,'*');
      if(!field_end)
      {
        gaim_debug_misc("gaym","Room list parsing error: room number not properly terminated\n");
        return;
      }
      field_end++;
      field_len=field_end-list_position;
      num=g_strndup(list_position,field_len);
      
      list_position=field_end+1;
      
      
      //Next, the +'s indicate the level in the tree
      while(*list_position=='+')
      {
        level++;
        list_position++;
      }
      
      if(level>current_level) 
      {
        current_level=level;
        current_parent=last_room;
      }
      if(level<current_level)      
      {
        
        while(level<current_level)
        {
          current_level--;
          current_parent=current_parent?current_parent->parent:NULL;   
        }
      } 
      
      
      //Finally, the readable room name
      field_end=strstr(list_position,"\\\n");         
      if(!field_end)
      {
        gaim_debug_misc("gaym","Room list parsing error!");
        return;
      }
      field_len=field_end-list_position;
      name=g_strndup(list_position,field_len);
      
     
      if(gaym->roomlist_filter)
        current_parent=NULL;
      list_position=field_end;
    
      //gaim_debug_misc("gaym","Just before add condition with name=%s and roomlist_filter=%s\n",name,gaym->roomlist_filter);
      char* lname=g_strdown(g_strdup(name));
      if(!gaym->roomlist_filter || strstr(lname,gaym->roomlist_filter)!=0)
      {
      
      //Actually create and add the room
      room = gaim_roomlist_room_new(GAIM_ROOMLIST_ROOMTYPE_ROOM, name, current_parent);
      gaim_roomlist_room_add_field(gaym->roomlist, room, name);
      gaim_roomlist_room_add_field(gaym->roomlist, room, num);
      gaim_roomlist_room_add(gaym->roomlist, room);
      gaim_debug_misc("gaym","(Ref %x) Added room %s: %s (level %d) (parent %x)\n",room,num,name,current_level,current_parent);
      g_free(name);
      g_free(num);
      if(lname)
        g_free(lname);
      }
    }
    
    //This is a category
    else if(!gaym->roomlist_filter)
    {
      
      //This code is duplicated above. Should probably make a function.
      while(*list_position=='+')
      {
        level++;
        list_position++;
      }
      
      if(level>current_level) 
      {
        current_level=level;
        current_parent=last_room;
      }
      if(level<current_level)      
      {
        while(level<current_level)
        {
          current_level--;
          current_parent=current_parent?current_parent->parent:NULL;   
          gaim_debug_misc("gaym","Changed parent to %x\n",current_parent);
        }
      } 
      //*************end duplicate
        
      field_end=strstr(list_position,"\\\n");
      if(!field_end)
      {
        gaim_debug_misc("gaym","Room list parsing error!");
        return;
      }
      field_len=field_end-list_position;
      name=g_strndup(list_position,field_len);
      room = gaim_roomlist_room_new(GAIM_ROOMLIST_ROOMTYPE_CATEGORY, name, current_parent);
      gaim_roomlist_room_add(gaym->roomlist, room);
    
      gaim_debug_misc("gaym","(Ref %x) Added categroy %s (level %d) (parent %x)\n",room,name,level,current_parent);
      g_free(name);
      list_position=field_end;
    
    }
    else
      list_position=strstr(list_position,"\\\n");
    
    if(*(list_position+2) != '|')
    {
      //gaim_debug_misc("gaym","list_position: 0(%c) 1(%c) 2(%c) 3(%c)\n",*list_position,*(list_position+1),*(list_position+2),*(list_position+3));
      gaim_roomlist_unref(gaym->roomlist);
       gaim_roomlist_set_in_progress(gaym->roomlist, FALSE);
      //gaym->roomlist = NULL;
      if(gaym->roomlist_filter)
        g_free(gaym->roomlist_filter);
      gaym->roomlist_filter=NULL;
      return;
    }
    last_room=room;
    }
    
}
static void gaym_get_configtxt_cb(gpointer proto_data, const char* config_text, size_t len) {
	
	struct gaym_conn *gaym = (struct gaym_conn*)proto_data;
	
	if (!config_text)
		return;
	
	gaym->configtxt = g_strdup(config_text);
	return;
		
}

static void gaym_login_cb(gpointer data, gint source, GaimInputCondition cond)
{
	GaimConnection *gc = data;
	struct gaym_conn *gaym = gc->proto_data;
	char hostname[256];
	char *buf;
	const char *username;
	const char* user_bioline=NULL; 
	char* bioline;
	char* login_name;
	
	if(GAIM_CONNECTION_IS_VALID(gc))
	{
	GList *connections = gaim_connections_get_all();

	if (source < 0) {
		gaim_connection_error(gc, _("Couldn't connect to host"));
		return;
	}

	
		
	if (!g_list_find(connections, gc)) {
		close(source);
		return;
	}

	gaym->fd = source;
	gaim_debug_misc("gaym","In login_cb with pw_hash=%s\n",gaym->hash_pw);
	if (gaym->hash_pw) {
		
		buf = gaym_format(gaym, "vv", "PASS", gaym->hash_pw);
		if (gaym_send(gaym, buf) < 0) {
			gaim_connection_error(gc, "Error sending password");
			return;
		}
		g_free(buf);
	}
	else
	{
		gaim_connection_error(gc, _("Password wasn't recorded. Report bug."));
		return;
	}
	gethostname(hostname, sizeof(hostname));
	hostname[sizeof(hostname) - 1] = '\0';
	username = gaim_account_get_string(gaym->account, "username", "");
	user_bioline = g_strdup(gaim_account_get_string(gaym->account, "bioline", "")); 
	gaim_debug_info ("gaym","USER BIOLINE=%x\n",user_bioline);
	gaim_account_set_user_info(gc->account, user_bioline);
	gaim_debug_misc("gaym","In login_cb, user_bioline: %x, gc->account=%x\n",user_bioline,gc->account);
	
	login_name=g_strdup(gaim_connection_get_display_name(gc));
	gaym_convert_nick_to_gaycom(login_name);
	bioline=g_strdup_printf("%s#%s",
					gaym->thumbnail,user_bioline?user_bioline:""); 
	
	buf = gaym_format(gaym, "vn", "NICK", login_name);
	gaim_debug_misc("gaym","Command: %s\n",buf);
	
	if (gaym_send(gaym, buf) < 0) {
		gaim_connection_error(gc, "Error sending nickname");
		return;
	}
	g_free(buf);
	buf = gaym_format(gaym, "vvvv:", "USER", login_name, hostname, gaym->server,
			      bioline);

	gaim_debug_misc("gaym","Command: %s\n",buf);
	if (gaym_send(gaym, buf) < 0) {
		gaim_connection_error(gc, "Error registering with server");
		return;
	}
        
        g_free(buf);
	const char* roomlisturl="http://www.gay.com/messenger/config.txt";
	gaim_session_fetch(roomlisturl, FALSE, "Mozilla/4.0 (compatible; MSIE 5.0)", FALSE, gaym_get_configtxt_cb, gaym, gaym->session);

	gc->inpa = gaim_input_add(gaym->fd, GAIM_INPUT_READ, gaym_input_cb, gc);
	}
}

static void gaym_close(GaimConnection *gc)
{
	struct gaym_conn *gaym = gc->proto_data;

	if (gaym == NULL)
		return;

	gaym_cmd_quit(gaym, "quit", NULL, NULL);

	if (gc->inpa)
		gaim_input_remove(gc->inpa);

	g_free(gaym->inbuf);
	close(gaym->fd);
	if (gaym->timer)
		gaim_timeout_remove(gaym->timer);
	g_hash_table_destroy(gaym->cmds);
	g_hash_table_destroy(gaym->msgs);
	if (gaym->motd)
		g_string_free(gaym->motd, TRUE);
	g_free(gaym->server);
	g_free(gaym);
}

static int gaym_im_send(GaimConnection *gc, const char *who, const char *what, GaimConvImFlags flags)
{
	struct gaym_conn *gaym = gc->proto_data;
	const char *args[2];
	const char *awaymsg=NULL;
	if (strchr(status_chars, *who) != NULL)
		args[0] = who + 1;
	else
		args[0] = who;
		

	if(flags & GAIM_CONV_IM_AUTO_RESP)
	{
		 awaymsg = g_strdup_printf("<Auto-response> %s",what);
		 args[1]=awaymsg;
		//Prevent memory leak
		//g_free(what);
		//what=args[1];
		
	}
	else
		args[1] = what;
	
	gaym_cmd_privmsg(gaym, "msg", NULL, args);
	g_free(awaymsg);
	return 1;
}

static void gaym_get_info(GaimConnection *gc, const char *who)
{
	struct gaym_conn *gaym = gc->proto_data;
	const char *args[1];
	args[0] = who;
	gaym->info_window_needed=TRUE;
	gaym_cmd_whois(gaym, "whois", NULL, args);
}

static void gaym_set_away(GaimConnection *gc, const char *state, const char *msg)
{
	struct gaym_conn *gaym = gc->proto_data;
	//const char *args[1];

	if (gc->away) {
		g_free(gc->away);
		gc->away = NULL;
	}

	if (msg)
		gc->away = g_strdup(msg);

	//args[0] = msg;
	//gaym_cmd_away(gaym, "away", NULL, args);
}

static void gaym_add_buddy(GaimConnection *gc, GaimBuddy *buddy, GaimGroup *group)
{
	struct gaym_conn *gaym = (struct gaym_conn *)gc->proto_data;
	struct gaym_buddy *ib = g_new0(struct gaym_buddy, 1);
	ib->name = g_strdup(buddy->name);
	g_hash_table_insert(gaym->buddies, ib->name, ib);
	gaim_debug_misc("gaym","Add buddy: %s\n",buddy->name);
	/* if the timer isn't set, this is during signon, so we don't want to flood
	 * ourself off with ISON's, so we don't, but after that we want to know when
	 * someone's online asap */
	if (gaym->timer)
		gaym_ison_one(gaym, ib);
}

static void gaym_remove_buddy(GaimConnection *gc, GaimBuddy *buddy, GaimGroup *group)
{
	struct gaym_conn *gaym = (struct gaym_conn *)gc->proto_data;
	g_hash_table_remove(gaym->buddies, buddy->name);
}

static void gaym_input_cb(gpointer data, gint source, GaimInputCondition cond)
{
	GaimConnection *gc = data;
	struct gaym_conn *gaym = gc->proto_data;
	char *cur, *end;
	int len;

	if (gaym->inbuflen < gaym->inbufused + IRC_INITIAL_BUFSIZE) {
		gaym->inbuflen += IRC_INITIAL_BUFSIZE;
		gaym->inbuf = g_realloc(gaym->inbuf, gaym->inbuflen);
	}

	if ((len = read(gaym->fd, gaym->inbuf + gaym->inbufused, IRC_INITIAL_BUFSIZE - 1)) < 0) {
		gaim_connection_error(gc, _("Read error"));
		return;
	} else if (len == 0) {
		gaim_connection_error(gc, _("Server has disconnected"));
		return;
	}

	gaym->inbufused += len;
	gaym->inbuf[gaym->inbufused] = '\0';

	cur = gaym->inbuf;
	while (cur < gaym->inbuf + gaym->inbufused &&
	       ((end = strstr(cur, "\r\n")) || (end = strstr(cur, "\n")))) {
		int step = (*end == '\r' ? 2 : 1);
		*end = '\0';
		gaym_parse_msg(gaym, cur);
		cur = end + step;
	}
	if (cur != gaym->inbuf + gaym->inbufused) { /* leftover */
		gaym->inbufused -= (cur - gaym->inbuf);
		memmove(gaym->inbuf, cur, gaym->inbufused);
	} else {
		gaym->inbufused = 0;
	}
}

static void gaym_chat_join (GaimConnection *gc, GHashTable *data)
{
	struct gaym_conn *gaym = gc->proto_data;
	const char* name[1];
        char* alias=NULL;
        
        
        GaimChat *c=NULL;
        
        GHashTable *chatinfo = NULL; //need a copy, because data gets destroyed in roomlist.c
        
        
        name[0]=g_hash_table_lookup(data,"ircname");
        
        if(!name[0])
          name[0]=g_hash_table_lookup(data,"name");
        else
        {
          
          
          alias=g_hash_table_lookup(data,"name");
          c=gaim_blist_find_chat(gaim_connection_get_account(gc), alias);
          if(!c)
          { 
            chatinfo = g_hash_table_new(g_str_hash, g_str_equal);
            alias=g_hash_table_lookup(data,"name");
          
            g_hash_table_replace(chatinfo, "name", g_strdup(alias));
            g_hash_table_replace(chatinfo, "ircname", g_strdup(name[0]));
          
            c=gaim_chat_new(gaim_connection_get_account(gc), 
                            alias,
                            chatinfo);
            
              
            gaim_blist_add_chat(c, NULL, NULL);
          }
          
        }
        
        if(!name[0] || *name[0]!='#')
        {
          //Trigger a room search in config.txt....  
          return;
        }
      
	gaym_cmd_join(gaym, NULL, NULL, name);
}

static char *gaym_get_chat_name(GHashTable *data) {
	return g_strdup(g_hash_table_lookup(data, "name"));
}

static void gaym_chat_invite(GaimConnection *gc, int id, const char *message, const char *name) 
{
	struct gaym_conn *gaym = gc->proto_data;
	GaimConversation *convo = gaim_find_chat(gc, id);
	const char *args[2];

	if (!convo) {
		gaim_debug(GAIM_DEBUG_ERROR, "gaym", "Got chat invite request for bogus chat\n");
		return;
	}
	args[0] = name;
	args[1] = gaim_conversation_get_name(convo);
	gaym_cmd_invite(gaym, "invite", gaim_conversation_get_name(convo), args);
}


static void gaym_chat_leave (GaimConnection *gc, int id)
{
	struct gaym_conn *gaym = gc->proto_data;
	GaimConversation *convo = gaim_find_chat(gc, id);
	const char *args[2];

	if (!convo)
		return;

	args[0] = gaim_conversation_get_name(convo);
	args[1] = NULL;
	gaym_cmd_part(gaym, "part", gaim_conversation_get_name(convo), args);
	serv_got_chat_left(gc, id);
}
static int gaym_chat_send(GaimConnection *gc, int id, const char *what)
{
	struct gaym_conn *gaym = gc->proto_data;
	GaimConversation *convo = gaim_find_chat(gc, id);
	const char *args[2];
	char *tmp;

	if (!convo) {
		gaim_debug(GAIM_DEBUG_ERROR, "gaym", "chat send on nonexistent chat\n");
		return -EINVAL;
	}
#if 0
	if (*what == '/') {
		return gaym_parse_cmd(gaym, convo->name, what + 1);
	}
#endif
	args[0] = convo->name;
	args[1] = what;

	gaym_cmd_privmsg(gaym, "msg", NULL, args);

	tmp = gaim_escape_html(what);
	serv_got_chat_in(gc, id, gaim_connection_get_display_name(gc), 0, tmp, time(NULL));
	g_free(tmp);
	return 0;
}

static guint gaym_nick_hash(const char *nick)
{
	char *lc;
	guint bucket;

	lc = g_utf8_strdown(nick, -1);
	bucket = g_str_hash(lc);
	g_free(lc);

	return bucket;
}

static gboolean gaym_nick_equal(const char *nick1, const char *nick2)
{
	return (gaim_utf8_strcasecmp(nick1, nick2) == 0);
}

static void gaym_buddy_free(struct gaym_buddy *ib)
{
	g_free(ib->name);
	g_free(ib);
}


static GaimRoomlist *gaym_roomlist_get_list(GaimConnection *gc)
{
		struct gaym_conn *gaym;
		GaimRoomlistField *f=NULL;
		GList *fields=NULL;

		gaym = gc->proto_data;

		if(gaym->roomlist)
				gaim_roomlist_unref(gaym->roomlist);

        gaym->roomlist = gaim_roomlist_new(gaim_connection_get_account(gc));
  
        
  		//Add an initial ref so the list is never destroyed.
        gaim_roomlist_ref(gaym->roomlist);
  
        f = gaim_roomlist_field_new(GAIM_ROOMLIST_FIELD_STRING, "Long Name", "name", TRUE);
        fields = g_list_append(fields, f);

        f = gaim_roomlist_field_new(GAIM_ROOMLIST_FIELD_STRING, _("IRC Name"), "ircname", FALSE);
        fields = g_list_append(fields, f);

        gaim_roomlist_set_fields(gaym->roomlist, fields);
	gaym_get_roomlist_from_text(gaym);	
	
        gaim_roomlist_set_in_progress(gaym->roomlist, TRUE);
        return gaym->roomlist;
}

static void gaym_roomlist_cancel(GaimRoomlist *list)
{
	GaimConnection *gc = gaim_account_get_connection(list->account);
	struct gaym_conn *gaym;

	if (gc == NULL)
		return;

	gaym = gc->proto_data;

	gaim_roomlist_set_in_progress(list, FALSE);

	if (gaym->roomlist == list) {
		gaym->roomlist = NULL;
		gaim_roomlist_unref(list);
	}
}

static GaimPluginProtocolInfo prpl_info =
{
	OPT_PROTO_PASSWORD_OPTIONAL,
	NULL,					/* user_splits */
	NULL,					/* protocol_options */
	{"jpg",55,75,55,75},			/* icon_spec */
	gaym_blist_icon,			/* list_icon */
	gaym_blist_emblems,		/* list_emblems */
	NULL,					/* status_text */
	NULL,					/* tooltip_text */
	gaym_away_states,		/* away_states */
	NULL,					/* blist_node_menu */
	gaym_chat_join_info,		/* chat_info */
	gaym_chat_info_defaults,	/* chat_info_defaults */
	gaym_login,				/* login */
	gaym_close,				/* close */
	gaym_im_send,			/* send_im */
	gaym_set_info,					/* set_info */
	NULL,					/* send_typing */
	gaym_get_info,			/* get_info */
	gaym_set_away,			/* set_away */
	NULL,					/* set_idle */
	NULL,					/* change_passwd */
	gaym_add_buddy,			/* add_buddy */
	NULL,					/* add_buddies */
	gaym_remove_buddy,		/* remove_buddy */
	NULL,					/* remove_buddies */
	NULL,					/* add_permit */
	NULL,				/* add_deny */
	NULL,					/* rem_permit */
	NULL,			/* rem_deny */
	NULL,					/* set_permit_deny */
	NULL,					/* warn */
	gaym_chat_join,			/* join_chat */
	NULL,					/* reject_chat */
	gaym_get_chat_name,		/* get_chat_name */
	gaym_chat_invite,		/* chat_invite */
	gaym_chat_leave,			/* chat_leave */
	NULL,					/* chat_whisper */
	gaym_chat_send,			/* chat_send */
	NULL,					/* keepalive */
	NULL,					/* register_user */
	NULL,					/* get_cb_info */
	NULL,					/* get_cb_away */
	NULL,					/* alias_buddy */
	NULL,					/* group_buddy */
	NULL,					/* rename_group */
	NULL,					/* buddy_free */
	NULL,					/* convo_closed */
	NULL,					/* normalize */
	NULL,					/* set_buddy_icon */
	NULL,					/* remove_group */
	NULL,					/* get_cb_real_name */
	NULL,					/* set_chat_topic */
	NULL,	/* find_blist_chat */
	gaym_roomlist_get_list,	/* roomlist_get_list */
	gaym_roomlist_cancel,	/* roomlist_cancel */
	NULL,					/* roomlist_expand_category */
	NULL,					/* can_receive_file */
	gaym_dccsend_send_file	/* send_file */
};

static
int gaym_kill_entrance_msgs(GaimConversation* conv, char* name) {


	gaim_debug_misc("gaym","Now in kill entrance function\n");
	if(gaim_prefs_get_bool("/plugins/prpl/gaym/show_entrance_exit_msgs")) 
    {
		if(gaim_prefs_get_bool("/plugins/prpl/gaym/hide_bot_entrance_exit_msgs"))
			if(gaim_conv_chat_get_ignored_user(conv->u.chat, name))
			{
				gaim_debug_misc("gaym","%s is ignored!",name);
				return 1;
			}
		return 0;
	}
	else
		return 1;

}

static
char* gaym_get_generic_name(const char* name, char* subroom) {
	
	char* nameptr = name;
	int root_length=0;
	
	if(!name)
		return name;
	
	//If first char isn't a #, then we don't care.
	if(*nameptr++ != '#')
		return g_strdup(name);


	while(isdigit(*nameptr)) nameptr++;

	//If an equals sign doesn't follow the numbers, it's the wrong form.
	if(*(nameptr) != '=')
		return g_strdup(name);

	*subroom=*(nameptr+1);

	root_length=nameptr-name+1;

	return(g_strdup_printf("%.*s*",root_length,name));
	
	

}
static 
void gaym_fix_room_title(GaimConversation* conv) {
	
	char* full_name;
	char subroom;
	char* generic_name = gaym_get_generic_name(conv->name, &subroom);
	
				
	gaim_debug_misc("gaym","Fixing conversation title for %s\n",generic_name);
	
	GaimBuddy* chat_node = gaym_find_chat_by_ircname(conv->account, generic_name);

	if(!chat_node)
		return;
	full_name = g_strdup_printf("%s - (%c)", chat_node->name, subroom);
	gaim_conversation_set_title(conv, full_name);

	g_free(generic_name);
	g_free(full_name);
	
}
static
void gaym_get_photo_info(GaimConversation* conv) {
	char *buf;
	char *name;
	gaim_debug_misc("gaym","Got conversation-created signal\n");
        if(strncmp(conv->account->protocol_id,"prpl-gaym",9) == 0 && gaim_conversation_get_type(conv) == GAIM_CONV_IM) {

	struct gaym_conn *gaym;
	
	GaimConnection *gc = gaim_conversation_get_gc(conv);
	gaym = (struct gaym_conn*) gc->proto_data;

	gaym->info_window_needed=FALSE;
        name = g_strdup(conv->name);
	
        gaym->whois.nick = g_strdup(name);
	gaym_convert_nick_to_gaycom(name);
	buf = gaym_format(gaym, "vn", "WHOIS", name);
        gaim_debug_misc("gaym","Conversation triggered command: %s\n",buf);
	gaym_send(gaym, buf);
	g_free(buf);
        }
	
}

static void gaym_join_subroom(GaimBlistNode * node, gpointer data) {
  
	struct gaym_conn* gaym=(struct gaym_conn*)((GaimChat*)node)->account->gc->proto_data;
	char *buf;

	
	if (!data)
		return;

	buf = gaym_format(gaym, "cv", "JOIN", data);
	gaim_debug_misc("gaym","Command: %s\n", buf);
	gaym_send(gaym, buf);
	g_free(buf);

	
 } 
static void gaym_add_subchannels(GaimBlistNode *node, GList **menu)
{
  
	char *label,*room;	
  int i;
  
  struct gaym_conn *gaym;
  GaimChat *chat=(GaimChat*)node;
  
  if(node->type != GAIM_BLIST_CHAT_NODE)
    return;
  
  gaym=chat->account->gc->proto_data;
  
  room=g_hash_table_lookup(chat->components,"ircname");
  
  if(!room)
  	return;
  
  for(i=1; i<3; i++)
  {
    
    label = g_strdup_printf("%s %d",chat->alias,i);
    room=g_strdup_printf("%.*s%d",strlen(room)-1,room,i);
    GaimBlistNodeAction* act=gaim_blist_node_action_new(label,
        gaym_join_subroom,
        room);
  
    *menu=g_list_append(*menu,act);
    //g_free(label);
  }
}
  
  
static GaimPluginPrefFrame *
    get_plugin_pref_frame(GaimPlugin *plugin)
{
  GaimPluginPrefFrame *frame;
  GaimPluginPref *ppref;

  gaim_debug_misc("gaym","get pref frame...\n");
  frame = gaim_plugin_pref_frame_new();

  ppref = gaim_plugin_pref_new_with_label(_("Conversations"));
  gaim_plugin_pref_frame_add(frame, ppref);

  ppref = gaim_plugin_pref_new_with_name_and_label(
      "/plugins/prpl/gaym/show_entrance_exit_msgs",
  _("Show entance/exit messages"));
  gaim_plugin_pref_frame_add(frame, ppref);
  
  ppref = gaim_plugin_pref_new_with_name_and_label(
      "/plugins/prpl/gaym/show_bio_with_join",
  _("Show bioline for users joining the room."));
  gaim_plugin_pref_frame_add(frame, ppref);
  
  ppref = gaim_plugin_pref_new_with_name_and_label(
      "/plugins/prpl/gaym/hide_bot_entrance_exit_msgs",
  _("Don't show entrance/exit messages for detected bots."));
  gaim_plugin_pref_frame_add(frame, ppref);

  //ppref = gaim_plugin_pref_new_with_name_and_label(
	//	  "/plugins/prpl/gaym/bot_lines",
  //_("Space-seperated list of terms that bots use. <Not yet implemented>"));
  //gaim_plugin_pref_frame_add(frame, ppref);
  return frame;
}


static GaimPluginUiInfo prefs_info = {
  get_plugin_pref_frame
};


static GaimPluginInfo info =
{
	GAIM_PLUGIN_MAGIC,
	GAIM_MAJOR_VERSION,
	GAIM_MINOR_VERSION,
	GAIM_PLUGIN_PROTOCOL,                             /**< type           */
	NULL,                                             /**< ui_requirement */
	NULL,                     /**< flags          */
	NULL,                                             /**< dependencies   */
	GAIM_PRIORITY_DEFAULT,                            /**< priority       */

	"prpl-gaym",                                      /**< id             */
	"GAYM",                                           /**< name           */
	VERSION,                                          /**< version        */
	N_("GAYM Protocol Plugin"),                       /**  summary        */
	N_("Gay.com Protocol based on IRC"),              /**  description    */
	NULL,                                             /**< author         */
	"http://gaym.sourceforge.org",      		  /**< homepage      */

	NULL,                                             /**< load           */
	NULL,                                             /**< unload         */
	NULL,                                             /**< destroy        */

	NULL,                                             /**< ui_info        */
	&prpl_info,                                       /**< extra_info     */
	&prefs_info,
	gaym_actions
};



static void _init_plugin(GaimPlugin *plugin)
{
	
	GaimAccountOption *option;

	option = gaim_account_option_string_new(_("Server"), "server", IRC_DEFAULT_SERVER);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);

	option = gaim_account_option_int_new(_("Port"), "port", IRC_DEFAULT_PORT);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);

	
	option = gaim_account_option_string_new(_("Bio Line"), "bioline", "");
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);

        //We have to pull thumbnails, since they aren't pushed like with other protocols.
      gaim_signal_connect(gaim_conversations_get_handle(),
                            "conversation-created",
                            plugin, GAIM_CALLBACK(gaym_get_photo_info), NULL);

	  gaim_signal_connect(gaim_conversations_get_handle(),
	  						"conversation-updated",
							plugin, GAIM_CALLBACK(gaym_fix_room_title), NULL);
							
      gaim_signal_connect(gaim_conversations_get_handle(),
                            "chat-buddy-joining",
                            plugin, GAIM_CALLBACK(gaym_kill_entrance_msgs), NULL);
      gaim_signal_connect(gaim_conversations_get_handle(),
                            "chat-buddy-leaving",
                            plugin, GAIM_CALLBACK(gaym_kill_entrance_msgs), NULL);
      gaim_signal_connect(gaim_blist_get_handle(),
                          "blist-node-extended-menu",
                          plugin, GAIM_CALLBACK(gaym_add_subchannels), NULL);
     
      gaim_prefs_add_none("/plugins/prpl/gaym");
      gaim_prefs_add_bool("/plugins/prpl/gaym/show_bio_with_join",   TRUE);
      gaim_prefs_add_bool("/plugins/prpl/gaym/show_entrance_exit_msgs",   TRUE);
      gaim_prefs_add_bool("/plugins/prpl/gaym/hide_bot_entrance_exit_msgs",   TRUE);
      gaim_prefs_add_string("/plugins/prpl/gaym/bot_lines",NULL);

        
	_gaym_plugin = plugin;

	gaym_register_commands();
}

GAIM_INIT_PLUGIN(gaym, _init_plugin, info);
