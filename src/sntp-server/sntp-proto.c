/*
 * sntp-proto.c - simple network time protocol implementation
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
 * $Id: sntp-proto.c,v 1.7 2001/04/28 12:37:06 ela Exp $
 *
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#if ENABLE_SNTP_PROTO

#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#if HAVE_UNISTD_H
# include <unistd.h>
#endif
#if HAVE_SYS_TIME_H
# include <sys/time.h>
#endif

#ifdef __MINGW32__
# include <winsock2.h>
#endif

#ifndef __MINGW32__
# include <sys/types.h>
# include <netinet/in.h>
#endif

#include "libserveez.h"
#include "sntp-proto.h"

/*
 * Simple network time server configuration.
 */
sntp_config_t sntp_config = 
{
  0, /* default nothing */
};

/*
 * Defining configuration file associations with key-value-pairs.
 */
svz_key_value_pair_t sntp_config_prototype [] = 
{
  REGISTER_END ()
};

/*
 * Definition of this server.
 */
svz_servertype_t sntp_server_definition =
{
  "Simple Network Time Protocol server",
  "sntp",
  NULL,
  sntp_init,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  sntp_handle_request,
  &sntp_config,
  sizeof (sntp_config),
  sntp_config_prototype
};

/*
 * Initialize a SNTP server instance.
 */
int
sntp_init (svz_server_t *server)
{
  sntp_config_t *cfg = server->cfg;

  return 0;
}

/*
 * The packet processor for the SNTP server.
 */
int
sntp_handle_request (socket_t sock, char *packet, int len)
{
  unsigned long date;
  unsigned char reply[8];
#if HAVE_GETTIMEOFDAY
  struct timeval t;
#else
  time_t t;
#endif

#if 0
  svz_hexdump (stdout, "sntp packet", sock->sock_desc, packet, len, 0);
#endif

#if HAVE_GETTIMEOFDAY
  gettimeofday (&t, NULL);
  date = htonl (2208988800u + t.tv_sec);
  memcpy (reply, &date, 4);
  date = htonl (t.tv_usec);
  memcpy (&reply[4], &date, 4);
  udp_printf (sock, "%c%c%c%c", 
	      reply[0], reply[1], reply[2], reply[3]);
  udp_printf (sock, "%c%c%c%c", 
	      reply[4], reply[5], reply[6], reply[7]);
#else /* not HAVE_GETTIMEOFDAY */
  t = time (NULL);
  date = htonl (2208988800u + t);
  memcpy (reply, &date, 4);
  udp_printf (sock, "%c%c%c%c", reply[0], reply[1], reply[2], reply[3]);
#endif /* not HAVE_GETTIMEOFDAY */

  return 0;
}

#else /* not ENABLE_SNTP_PROTO */

int sntp_dummy; /* Shut up compiler. */

#endif /* not ENABLE_SNTP_PROTO */
