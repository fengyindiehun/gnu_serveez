/*
 * foo-proto.c - example server implementation
 *
 * Copyright (C) 2000 Stefan Jahn <stefan@lkcc.org>
 * Copyright (C) 2000 Raimund Jacob <raimi@lkcc.org>
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
 * $Id: foo-proto.c,v 1.23 2001/04/04 14:23:14 ela Exp $
 *
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>

#ifdef __MINGW32__
# include <winsock2.h>
#endif

#include "libserveez.h"
#include "foo-proto.h"

/* 
 * Packet specification for `sock_check_request ()'.
 */
char *foo_packet_delim = "\r\n";
int foo_packet_delim_len = 2;

/*
 * Default value definitions for the server configuration.
 */
struct portcfg some_default_port = 
{
  PROTO_TCP,      /* we are tcp */
  42421,          /* standard port to listen on */
  "*",            /* bind all local addresses */
  NULL,           /* calculated from above values later */
  NULL,           /* no inpipe for us */
  NULL            /* no outpipe for us */
};

int some_default_intarray[] = 
{
  4,
  1,
  2,
  3
};

char *some_default_strarray[] = 
{
  "Hello",
  "This",
  "is",
  "a",
  "default",
  "string",
  "array",
  NULL
};

svz_hash_t *some_default_hash = NULL;

/*
 * Demonstrate how our private configuration looks like and provide
 * default values.
 */
struct foo_config mycfg = 
{
  -42,
  some_default_strarray,
  "Default reply",
  some_default_intarray,
  42,
  &some_default_port,
  &some_default_hash
};

/*
 * Defining configuration file associations with key-value-pairs.
 */
svz_key_value_pair_t foo_config_prototype [] = 
{
  REGISTER_INT ("bar", mycfg.bar, NOTDEFAULTABLE),
  REGISTER_STR ("reply", mycfg.reply, DEFAULTABLE),
  REGISTER_STRARRAY ("messages", mycfg.messages, DEFAULTABLE),
  REGISTER_INTARRAY ("ports", mycfg.ports, DEFAULTABLE),
  REGISTER_HASH ("assoc", mycfg.assoc, DEFAULTABLE),
  REGISTER_PORTCFG ("port", mycfg.port, DEFAULTABLE),
  REGISTER_END ()
};

/*
 * Definition of this server.
 */
svz_servertype_t foo_server_definition =
{
  "foo example server",
  "foo",
  foo_global_init,
  foo_init,
  foo_detect_proto,
  foo_connect_socket,
  foo_finalize,
  foo_global_finalize,
  NULL,
  foo_info_server,
  NULL,
  NULL,
  &mycfg,
  sizeof (mycfg),
  foo_config_prototype
};

/* ************* Networking functions ************************* */

/*
 * This callback is used when a coserver asynchronously resolved the
 * client's ip to a name.
 */
int
foo_handle_coserver_result (char *host, int id, int version)
{
  socket_t sock = sock_find (id, version);

  if (host && sock)
    sock_printf (sock, "You are `%s'\r\n", host);
  return 0;
}

/*
 * Handle a single request as found by the `sock_check_request ()'.
 */
int 
foo_handle_request (socket_t sock, char *request, int len)
{
  struct foo_config *cfg = sock->cfg;

  return sock_printf (sock, "%s: %d\r\n", cfg->reply, len);
}

/*
 * This callback gets called whenever some unknown client connects and
 * sends some data. We check for some string that identifies the foo
 * protocol.
 */
int
foo_detect_proto (void *cfg, socket_t sock)
{
  /* see if the stream starts with our identification string */
  if (sock->recv_buffer_fill >= 5 &&
      !memcmp (sock->recv_buffer, "foo\r\n", 5))
    {

      /* it's us: forget the id string and signal success */
      sock_reduce_recv (sock, 5);
      return -1;
    }

  /* not us... */
  return 0;
}

/*
 * Our detect proto thinks that sock is a foo connection, so install
 * the callbacks we need.
 */
int
foo_connect_socket (void *acfg, socket_t sock)
{
  struct foo_config *cfg = (struct foo_config *) acfg;
  int i;
  int r;

  /*
   * we uses a default routine to split incoming data into packets
   * (which happen to be lines)
   */
  sock->boundary = foo_packet_delim;
  sock->boundary_size = foo_packet_delim_len;
  sock->check_request = sock_check_request;
  sock->handle_request = foo_handle_request;

  log_printf (LOG_NOTICE, "foo client detected\n");
  
  if (cfg->messages) 
    {
      for (i = 0; cfg->messages[i]; i++)
	{
	  r = sock_printf (sock, "%s\r\n", cfg->messages[i]);
	  if (r)
	    return r;
	}
    }

  /*
   * Ask a coserver to resolve the client's ip
   */
  sock_printf (sock, "starting reverse lookup...\r\n");
  coserver_reverse (sock->remote_addr, foo_handle_coserver_result,
		    sock->id, sock->version);
  sock_printf (sock, "...waiting...\r\n");
  return 0;
}

/* ************************** Initialization ************************** */

/*
 * Called once of the foo server type. we use it to create the default
 * hash.
 */
int
foo_global_init (void)
{
  some_default_hash = svz_hash_create (4);
  svz_hash_put (some_default_hash, "grass", "green");
  svz_hash_put (some_default_hash, "cow", "milk");
  svz_hash_put (some_default_hash, "sun", "light");
  svz_hash_put (some_default_hash, "moon", "tide");
  svz_hash_put (some_default_hash, "gnu", "good");
  return 0;
}

/*
 * Called once for foo servers, free our default hash.
 */
int
foo_global_finalize (void)
{
  svz_hash_destroy (some_default_hash);
  return 0;
}

/*
 * A single foo server instance gets destroyed. Free the hash
 * unless it is the default hash.
 */
int
foo_finalize (svz_server_t *server)
{
  struct foo_config *c = server->cfg;
  char **values;
  int n;

  log_printf (LOG_NOTICE, "destroying %s\n", server->name);

  /*
   * Free our hash but be careful not to free it if was the
   * default value.
   */
  if (*(c->assoc) != some_default_hash)
    {
      if ((values = (char **) svz_hash_values (*(c->assoc))) != NULL)
	{
	  for (n = 0; n < svz_hash_size (*(c->assoc)); n++)
	    svz_free (values[n]);
	  svz_hash_xfree (values);
	}
      svz_hash_destroy (*(c->assoc));
    }
  
  return 0;
}

/*
 * Initialize a foo server instance.
 */
int
foo_init (svz_server_t *server)
{
  struct foo_config *c = server->cfg;

  fprintf (stderr, "foo: binding on port %s:%d\n", 
	   c->port->ipaddr, c->port->port);
  
  if (c->port->proto != PROTO_TCP) 
    {
      fprintf (stderr, "foo: server can handle TCP only !\n");
      return -1;
    }

  /*
   * Bind this instance to the given port.
   */
  server_bind (server, c->port);

  return 0;
}

/*
 * Server info callback. We use it here to print the
 * whole configuration once.
 */
char *
foo_info_server (svz_server_t *server)
{
  struct foo_config *cfg = server->cfg;
  static char info[80*16], text[80];
  char **s = cfg->messages;
  int *j = cfg->ports;
  int i;
  char **keys;
  svz_hash_t *h;

  sprintf (text, 
	   " reply : %s\r\n"
	   " bar   : %d\r\n",
	   cfg->reply, cfg->bar);
  strcpy (info, text);

  if (s != NULL) 
    {
      for (i = 0; s[i] != NULL; i++)
	{
	  sprintf (text, " messages[%d] : %s\r\n", i, s[i]);
	  strcat (info, text);
	}
    } 
  else 
    {
      sprintf (text, " messages : NULL\r\n");
      strcat (info, text);
    }

  if (j != NULL) 
    {
      for (i = 1; i < j[0]; i++)
	{
	  sprintf (text, " ports[%d] : %d\r\n", i, j[i]);
	  strcat (info, text);
	}
    } 
  else 
    {
      sprintf (text, " ports : NULL\r\n");
      strcat (info, text);
    }
  
  h = *(cfg->assoc);
  if (h != NULL) 
    {
      keys = svz_hash_keys (h);

      for (i = 0; i < h->keys; i++)
	{
	  sprintf (text, " assoc[%d] : `%s' => `%s'\r\n",
		   i, keys[i], (char *) svz_hash_get (h, keys[i]));
	  strcat (info, text);
	}
      svz_hash_xfree (keys);
    } 
  else 
    {
      sprintf (text, " assoc : NULL\r\n");
      strcat (info, text);
    }

  return info;
}
