/*
 * connect.c - socket connection implementation
 *
 * Copyright (C) 1999 Martin Grabmueller <mgrabmue@cs.tu-berlin.de>
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
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/time.h>

#ifdef __MINGW32__
# include <winsock.h>
#endif

#ifndef __MINGW32__
# include <sys/socket.h>
# include <netinet/in.h>
# include <netdb.h>
#endif

#include "alloc.h"
#include "util.h"
#include "socket.h"
#include "server-core.h"

/*
 * Create a TCP connection to host HOST and set the socket descriptor in
 * structure SOCK to the resulting socket. Return a zero value on error.
 */
socket_t
sock_connect(unsigned host, int port)
{
  struct sockaddr_in server;
  SOCKET client_socket;
  socket_t sock;

  /*
   * first, create a socket for communication with the server
   */
  if ((client_socket = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET)
    {
      log_printf(LOG_ERROR, "socket: %s\n", NET_ERROR);
      return NULL;
    }

  /*
   * second, connect to the server
   */
  server.sin_family = AF_INET;
  server.sin_addr.s_addr = host;
  server.sin_port = port;
  
  if (connect(client_socket, (struct sockaddr *) &server,
	      sizeof(server)) == -1)
    {
      log_printf(LOG_ERROR, "connect: %s\n", NET_ERROR);
      if (CLOSE_SOCKET(client_socket) < 0)
	{
	  log_printf(LOG_ERROR, "close: %s\n", NET_ERROR);
	}
      return NULL;
    }

  /*
   * third, create socket structure and enqueue it
   */
  if((sock = sock_create (client_socket)))
    {
      sock_enqueue(sock);
      connected_sockets++;
      sock->flags |= SOCK_FLAG_CONNECTED;
    }

  return sock;
}
