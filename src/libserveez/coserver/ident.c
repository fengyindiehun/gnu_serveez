/*
 * ident.c - ident coserver implementation
 *
 * Copyright (C) 2000, 2001 Stefan Jahn <stefan@lkcc.org>
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
 * $Id: ident.c,v 1.2 2001/02/02 11:26:24 ela Exp $
 *
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#if ENABLE_IDENT

#define _GNU_SOURCE
#include <string.h>
#include <errno.h>
#if HAVE_UNISTD_H
# include <unistd.h>
#endif
#ifdef __MINGW32__
# include <winsock.h>
#endif

#ifndef __MINGW32__
# include <sys/types.h>
# include <sys/socket.h>
# include <netinet/in.h>
# include <netdb.h>
# include <arpa/inet.h>
#endif

#include "libserveez/socket.h"
#include "libserveez/util.h"
#include "libserveez/coserver/coserver.h"
#include "ident.h"

#define IDENT_PORT 113 /* the identd port */

/*
 * The following routine takes the input buffer in the format "%s:%u:%u"
 * which is the remote internet address and the remote and local network 
 * port. Then it is processing an ident request and parses the respond to
 * gain the users name.
 */
char *
ident_handle_request (char *inbuf)
{
  SOCKET sock;
  struct sockaddr_in server;
  unsigned long addr;
  unsigned lport, rport;
  static char ident_response[COSERVER_BUFSIZE];
  char *p_end;
  char user[64];
  char *p, *u;
  int r;
  char *rp;

  /* Parse internet address first. */
  p = inbuf;
  while (*p && *p != ':')
    p++;
  if (!*p)
    {
      log_printf (LOG_ERROR, "ident: invalid request `%s'\n", inbuf);
      return NULL;
    }
  *p = '\0';
  p++;
  addr = inet_addr (inbuf);
  
  /* Parse remote and local port afterwards. */
  if (2 != sscanf (p, "%u:%u", &rport, &lport))
    {
      log_printf (LOG_ERROR, "ident: invalid request `%s'\n", inbuf);
      return NULL;
    }
	 
   /* Create a socket for communication with the ident server. */
  if ((sock = socket (AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET)
    {
      log_printf (LOG_ERROR, "ident: socket: %s\n", NET_ERROR);
      return NULL;
    }

  /* Connect to the server. */
  memset (&server, 0, sizeof (server));
  server.sin_family = AF_INET;
  server.sin_addr.s_addr = addr;
  server.sin_port = htons (IDENT_PORT);
  if (connect (sock, (struct sockaddr *) &server, sizeof (server)) == -1)
    {
      log_printf (LOG_ERROR, "ident: connect: %s\n", NET_ERROR);
      closesocket (sock);
      return NULL;
    }

  /* Send the request to the server and receive response. */
  sprintf (ident_response, "%d , %d\r\n", rport, lport);
  send (sock, ident_response, strlen (ident_response), 0);

  r = 1;
  rp = ident_response;
  while (rp < ident_response + COSERVER_BUFSIZE && r != 0)
    {
      if ((r = recv (sock, rp, 
		     COSERVER_BUFSIZE - (rp - ident_response), 0)) < 0)
	{
	  log_printf (LOG_ERROR, "ident: recv: %s\n", NET_ERROR);
	  closesocket (sock);
	  return NULL;
	}
      rp += r;
    }

  /* Now close the socket and notify the response. */
  if (shutdown (sock, 2) == -1)
    log_printf (LOG_ERROR, "ident: shutdown: %s\n", NET_ERROR);
  if (closesocket (sock) < 0)
    log_printf (LOG_ERROR, "ident: close: %s\n", NET_ERROR);

  log_printf (LOG_NOTICE, "ident: %s", ident_response);

  p = ident_response;
  p_end = p + strlen (p);

  /* Parse client port. */
  if (p >= p_end || !(*p >= '0' && *p <= '9'))
    return NULL;
  while (p < p_end && *p >= '0' && *p <= '9')
    p++;

  /* Skip whitespace and separating comma. */
  while (p < p_end && *p == ' ')
    p++;
  if (p >= p_end || *p != ',')
    return NULL;
  p++;
  while (p < p_end && *p == ' ')
    p++;

  /* Parse server port. */
  if (p >= p_end || !(*p >= '0' && *p <= '9'))
    return NULL;
  while (p < p_end && *p >= '0' && *p <= '9')
    p++;

  /* Skip whitespace and separating colon. */
  while (p < p_end && *p == ' ')
    p++;
  if (p >= p_end || *p != ':')
    return NULL;
  p++;
  while (p < p_end && *p == ' ')
    p++;

  /* Parse response type. (USERID or ERROR possible) */
  if (memcmp (p, "USERID", 6))
    return NULL;
  while (p < p_end && *p != ' ')
    p++;

  /* Skip whitespace and separating colon. */
  while (p < p_end && *p == ' ')
    p++;
  if (p >= p_end || *p != ':')
    return NULL;
  p++;
  while (p < p_end && *p == ' ')
    p++;
  if (p >= p_end)
    return NULL;

  /* Parse OS type. */
  while (p < p_end && *p != ' ')
    p++;

  /* Skip whitespace and separating colon. */
  while (p < p_end && *p == ' ')
    p++;
  if (p >= p_end || *p != ':')
    return NULL;
  p++;
  while (p < p_end && *p == ' ')
    p++;

  /* Finally parse the user name. */
  u = user;
  while (p < p_end && *p != '\0' && *p != '\n' && *p != '\r')
    {
      if (u < user + (sizeof (user) - 1))
        *u++ = *p;
      p++;
    }
  *u = '\0';

#if ENABLE_DEBUG
  log_printf (LOG_DEBUG, "ident: received identified user `%s'\n", user);
#endif

  sprintf (ident_response, "%s", user);
  return ident_response;
}

int have_ident = 1;

#else /* ENABLE_IDENT */

int have_ident = 0; /* Shut compiler warnings up, remember for runtime. */

#endif /* not ENABLE_IDENT */
