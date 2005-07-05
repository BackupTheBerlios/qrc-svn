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

/* config.h */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/* system headers */
#include <glib.h>

/* gaim headers for this plugin */
#include "util.h"
#include "debug.h"
#include "account.h"
#include "privacy.h"

/* local headers for this plugin */
#include "gaympriv.h"
#include "gaym.h"

void synchronize_deny_list(GaimConnection * gc, const char *configtxt)
{
    char *srvdeny = NULL;
    char *start = NULL;
    char *end = NULL;
    gchar **srvdenylist = NULL;
    GSList *list;
    gint i = 0;
    gboolean needsync = FALSE;

    start =
        g_strstr_len(configtxt, -1, "connect-list.ignore.members=") + 28;
    end = g_strstr_len(start, -1, "\n");
    srvdeny = g_strndup(start, end - start);

    srvdenylist = g_strsplit(srvdeny, ",", -1);

    /* Add server deny list from config.txt to local deny list */
    for (i = 0; srvdenylist[i]; i++) {
        needsync = TRUE;
        for (list = gc->account->deny; list != NULL; list = list->next) {
            if (!gaim_utf8_strcasecmp
                (srvdenylist[i],
                 gaim_normalize(gc->account, (char *) list->data))) {
                needsync = FALSE;
                break;
            }
        }
        if (needsync) {
            if (!gaim_privacy_deny_add(gc->account, srvdenylist[i], TRUE)) {
                gaim_debug_error("gaym",
                                 "Failed to add %s to local deny list from server.\n",
                                 srvdenylist[i]);
            } else {
                gaim_debug_misc("gaym",
                                "Added %s to local deny list from server.\n",
                                srvdenylist[i]);
            }
        }
    }

    /* Add local deny list not found in config.txt to server deny list */
    for (list = gc->account->deny; list != NULL; list = list->next) {
        needsync = TRUE;
        for (i = 0; srvdenylist[i]; i++) {
            if (!gaim_utf8_strcasecmp
                (srvdenylist[i],
                 gaim_normalize(gc->account, (char *) list->data))) {
                needsync = FALSE;
                break;
            }
        }
        if (needsync) {
            gaym_server_store_deny(gc, (char *) list->data, TRUE);
        }
    }

    g_strfreev(srvdenylist);
    g_free(srvdeny);
    return;
}

void process_configtxt(GaimConnection * gc, const char *configtxt)
{
    synchronize_deny_list(gc, configtxt);
    return;
}

/**
 * vim:tabstop=4:shiftwidth=4:expandtab:
 */
