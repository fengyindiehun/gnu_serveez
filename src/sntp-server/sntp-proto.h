/*
 * sntp-proto.h - simple network time server definitions
 *
 * Copyright (C) 2000 Stefan Jahn <stefan@lkcc.org>
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 * 
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this package; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.  
 *
 * $Id: sntp-proto.h,v 1.1 2000/12/15 14:13:18 ela Exp $
 *
 */

#ifndef __SNTP_PROTO_H__
#define __SNTP_PROTO_H__

#if HAVE_CONFIG_H
# include <config.h>
#endif

#define _GNU_SOURCE
#include "server.h"
#include "socket.h"

/*
 * Protocol server specific configuration.
 */
typedef struct
{
  portcfg_t *port;
}
sntp_config_t;

int sntp_init (server_t *server);
int sntp_handle_request (socket_t sock, char *packet, int len);

/*
 * This server's definition.
 */
extern server_definition_t sntp_server_definition;

#endif /* __SNTP_PROTO_H__ */
