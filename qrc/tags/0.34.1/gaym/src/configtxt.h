/**
 * @file configtxt.h GayM Privacy API
 *
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
#ifndef _GAIM_GAYM_CONFIGTXT_H_
#define _GAIM_GAYM_CONFIGTXT_H_

#include "connection.h"

/**
 * Try to synchronize the server's deny list with gaim's local deny
 * list.  Because there may be a limit to the number of people you can
 * store on the server's deny list, this function's goal so to put the
 * superset of the two on both.
 *
 * @param gc        The connection.
 * @param configtxt The config.txt java properties retrieved from Gay.com.
 */
void synchronize_deny_list(GaimConnection * gc, const char *configtxt);

/**
 * This is the driver function to do all the processing of the config.txt
 * java properties file upon retrieval from Gay.com.  It should perform
 * each of the above functions.
 *
 * @param gc        The connection.
 * @param configtxt The config.txt java properties retrieved from Gay.com.
 */
void process_configtxt(GaimConnection * gc, const char *configtxt);

#endif                          /* _GAIM_GAYM_CONFIGTXT_H_ */

/**
 * vim:tabstop=4:shiftwidth=4:expandtab:
 */
