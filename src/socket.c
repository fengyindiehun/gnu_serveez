/*
 * socket.c - socket management implementation
 *
 * Copyright (C) 2000 Stefan Jahn <stefan@lkcc.org>
 * Copyright (C) 1999 Martin Grabmueller <mgrabmue@cs.tu-berlin.de>
 *
 * This is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 * 
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this package; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * $Id: socket.c,v 1.3 2000/06/12 13:59:37 ela Exp $
 *
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
#include <signal.h>

#ifdef __MINGW32__
# include <winsock.h>
#endif

#ifndef __MINGW32__
# include <sys/socket.h>
# include <netinet/in.h>
# include <netdb.h>
#endif

#include "snprintf.h"
#include "alloc.h"
#include "util.h"
#include "socket.h"
#include "server-socket.h"
#include "server.h"

#if ENABLE_HTTP_PROTO
# include "http-server/http-proto.h"
#endif 

#if ENABLE_CONTROL_PROTO
# include "ctrl-server/control-proto.h"
#endif

#if ENABLE_IRC_PROTO
# include "irc-core/irc-core.h"
# include "irc-server/irc-proto.h"
#endif

/*
 * Count the number of currently connected sockets.
 */
SOCKET connected_sockets = 0;

/*
 * SOCK_LOOKUP_TABLE is used to speed up references to socket
 * structures by socket's id.
 */
socket_t sock_lookup_table[SOCKET_MAX_IDS];
int socket_id = 0;

/*
 * Default function for writing to socket SOCK.  Simply flushes
 * the output buffer to the network.
 * Write as much as possible into the socket SOCK.  Writing
 * is performed non-blocking, so only as much as fits into
 * the network buffer will be written on each call.
 */
static int
default_write(socket_t sock)
{
  int num_written;
  int do_write;
  SOCKET desc;

  desc = sock->sock_desc;

  /* 
   * Write as many bytes as possible, remember how many
   * were actually sent. Limit the maximum sent bytes to
   * SOCK_MAX_WRITE.
   */
  do_write = sock->send_buffer_fill;
  if(do_write > SOCK_MAX_WRITE) do_write = SOCK_MAX_WRITE;
  num_written = send(desc, sock->send_buffer, do_write, 0);

#if 0
  dump_request(stdout, "sent", desc, sock->send_buffer, num_written);
#endif

  /* Some data has been written. */
  if (num_written > 0)
    {
      sock->last_send = time(NULL);

      /*
       * Shuffle the data in the output buffer around, so that
       * new data can get stuffed into it.
       */
      if(sock->send_buffer_fill > num_written)
	{
	  memmove(sock->send_buffer, 
		  sock->send_buffer + num_written,
		  sock->send_buffer_fill - num_written);
	}
      sock->send_buffer_fill -= num_written;
    }
  /* error occured while writing */
  else if (num_written < 0)
    {
      log_printf(LOG_ERROR, "sock write: %s\n", NET_ERROR);
      if(last_errno == SOCK_UNAVAILABLE)
	{
	  sock->unavailable = time(NULL) + RELAX_FD_TIME;
	  num_written = 0;
	}
    }

  /*
   * Return a non-zero value if an error occured.
   */
  return (num_written < 0) ? -1 : 0;
}

/*
 * Default function for reading from the socket SOCK.
 * This function only reads all data from the socket and
 * calls the CHECK_REQUEST function for the socket, if set.
 * Returns -1 if the socket has died, returns 0 otherwise.
 */
int
default_read (socket_t sock)
{
  int num_read;
  int ret;
  int do_read;
  SOCKET desc;

  desc = sock->sock_desc;

  /* 
   * Calculate how many bytes fit into the receive buffer.
   */
  do_read = sock->recv_buffer_size - sock->recv_buffer_fill;

  /*
   * Check if enough space is left in the buffer, kick the socket
   * if not. The main loop will kill the socket if we return a non-zero
   * value.
   */
  if (do_read <= 0)
    {
      log_printf(LOG_ERROR, "receive buffer overflow on socket %d\n",
		 desc);

      if (sock->kicked_socket)
	sock->kicked_socket(sock, 0);

      return -1;
    }

  /*
   * Try to read as much data as possible.
   */
  num_read = recv(desc,
		  sock->recv_buffer + sock->recv_buffer_fill,
		  do_read, 0);

  /* error occured while reading */
  if (num_read < 0)
    {
      /*
       * This means that the socket was shut down. Close the socket
       * in this case, which the main loop will do for us if we
       * return a non-zero value.
       */
      log_printf(LOG_ERROR, "sock read: %s\n", NET_ERROR);
      if(last_errno == SOCK_UNAVAILABLE)
	{
	  num_read = 0;
	}
      else
	{
	  return -1;
	}
    }
  /* some data has been read */
  else if (num_read > 0)
    {
      sock->last_recv = time(NULL);

#if ENABLE_FLOOD_PROTECTION
      if (!(sock->flags & SOCK_FLAG_NOFLOOD))
	{
	  sock->flood_points += 1 + (num_read / 50);
	  
	  if (sock->flood_points > sock->flood_limit)
	    {
	      log_printf(LOG_ERROR, "kicking socket %d (flood)\n",
			 desc);
	      
	      if (sock->kicked_socket)
		sock->kicked_socket(sock, 0);
	      
	      return -1;
	    }
	}
#endif /* ENABLE_FLOOD_PROTECTION */

#if 0
      dump_request(stdout, "received", desc,
		   sock->recv_buffer + sock->recv_buffer_fill,
		   num_read);
#endif

      sock->recv_buffer_fill += num_read;

      if (sock->check_request)
	{
	  ret = sock->check_request(sock);
	  if (ret)
	    return ret;
	}
    }
  /* the socket was selected but there is no data */
  else
    {
      log_printf(LOG_ERROR, "read: no data on socket %d\n", desc);
      return -1;
    }
  
  return 0;
}

/*
 * The default function which gets called when a client shuts down
 * its socket. SOCK is the socket which was closed.
 */
static int
default_disconnect (socket_t sock)
{
#if ENABLE_DEBUG
  log_printf (LOG_DEBUG, "socket %d disconnected\n",
	      sock->sock_desc);
#endif

  return 0;
}

/*
 * DEFAULT_CHECK_REQUEST gets called whenever data is read from a
 * client network socket.
 */
int
default_detect_proto (socket_t sock)
{
  int n;
  server_t *server;

  if (sock->data == NULL)
    return -1;

  for (n = 0; (server = SERVER (sock->data, n)); n++)
    {
      if (server->detect_proto (server->cfg, sock))
	{
	  sock->idle_func = NULL;
	  sock->cfg = server->cfg;
	  if (server->connect_socket (server->cfg, sock))
	    return -1;
	  return sock->check_request (sock);
	}
      
      /*
       * Discard this socket if there were not any valid protocol
       * detected and its receive buffer fill exceeds a maximum value.
       */
      if (sock->recv_buffer_fill > MAX_DETECTION_FILL)
	{
#if ENABLE_DEBUG
	  log_printf (LOG_DEBUG, "socket id %d detection failed\n",
		      sock->socket_id);
#endif
	  return -1;
	}
    }
  return 0;
}

/*
 * Default idle function.
 */
int
default_idle_func (socket_t sock)
{
  if (time(NULL) - sock->last_recv > MAX_DETECTION_WAIT)
    {
#if ENABLE_DEBUG
      log_printf (LOG_DEBUG, "socket id %d detection failed\n",
		  sock->socket_id);
#endif
      return -1;
    }

  sock->idle_counter = 1;
  return 0;
}

/*
 * This check_request () routine could be used by any protocol to 
 * detect and finally handle packets depending on a specific packet 
 * boundary. The apropiate handle_request () is called for each packet
 * explicitly with the packet length inclusive the packet boundary.
 */
int
default_check_request (socket_t sock)
{
  int len = 0;
  char *p, *packet, *end;

  assert (sock->boundary);
  assert (sock->boundary_size);

  p = sock->recv_buffer;
  end = p + sock->recv_buffer_fill - sock->boundary_size + 1;
  packet = p;

  do
    {
      /* Find packet boundary in the receive buffer. */
      while (p < end && memcmp (p, sock->boundary, sock->boundary_size))
        p++;

      /* Found ? */
      if (p < end && !memcmp (p, sock->boundary, sock->boundary_size))
        {
          p += sock->boundary_size;
          len += (p - packet);

	  /* Call the handle request callback. */
	  if (sock->handle_request)
	    {
	      if (sock->handle_request (sock, packet, p - packet))
		return -1;
	    }
	  packet = p;
        }
    }
  while (p < end);
  
  /* Shuffle data in the receive buffer around. */
  if (len > 0 && sock->recv_buffer_fill > len)
    {
      memmove (sock->recv_buffer, packet,
               sock->recv_buffer_fill - len);
    }
  sock->recv_buffer_fill -= len;
  
  return 0;
}

/*
 * Allocate a structure of type socket_t and initialize
 * its fields.
 */
socket_t
sock_alloc (void)
{
  socket_t sock;
  char * in;
  char * out;

  sock = xmalloc(sizeof(socket_data_t));
  in   = xmalloc(RECV_BUF_SIZE);
  out  = xmalloc(SEND_BUF_SIZE);

  sock->next = NULL;
  sock->prev = NULL;

  sock->sflags = SOCK_FLAG_INIT;
  sock->flags = SOCK_FLAG_INIT | SOCK_FLAG_INBUF | SOCK_FLAG_OUTBUF;
  sock->file_desc = -1;
  sock->sock_desc = -1;
  sock->pipe_desc[READ] = INVALID_HANDLE;
  sock->pipe_desc[WRITE] = INVALID_HANDLE;
  sock->parent = NULL;
  sock->recv_pipe = NULL;
  sock->send_pipe = NULL;

  sock->boundary = NULL;
  sock->boundary_size = 0;

  sock->remote_port = 0;
  sock->remote_addr = 0;
  sock->remote_host = NULL;
  sock->local_port  = 0;
  sock->local_addr  = 0;
  sock->local_host = NULL;

  sock->read_socket = default_read;
  sock->write_socket = default_write;
  sock->check_request = default_detect_proto;
  sock->disconnected_socket = default_disconnect;
  sock->handle_request = NULL;
  sock->kicked_socket = NULL;
  sock->idle_func = NULL;
  sock->idle_counter = 0;

  sock->recv_buffer = in;
  sock->recv_buffer_size = RECV_BUF_SIZE;
  sock->recv_buffer_fill = 0;
  sock->send_buffer = out;
  sock->send_buffer_size = SEND_BUF_SIZE;
  sock->send_buffer_fill = 0;
  sock->last_send = time(NULL);
  sock->last_recv = time(NULL);
#if ENABLE_FLOOD_PROTECTION
  sock->flood_points = 0;
  sock->flood_limit = 100;
#endif /* ENABLE_FLOOD_PROTECTION */
  sock->unavailable = 0;
  
  sock->data = NULL;

#if ENABLE_HTTP_PROTO
  sock->http = NULL;
#endif

  return sock;
}

/*
 * Resize the send and receive buffers for the socket SOCK.  SEND_BUF_SIZE
 * is the new size for the send buffer, RECV_BUF_SIZE for the receive
 * buffer.  Note that data may be lost when the buffers shrink.
 */
int 
sock_resize_buffers (socket_t sock, int send_buf_size, int recv_buf_size)
{
  char * send, * recv;

  send = xrealloc(sock->send_buffer, send_buf_size);
  recv = xrealloc(sock->recv_buffer, recv_buf_size);

  sock->send_buffer = send;
  sock->recv_buffer = recv;
  sock->send_buffer_size = send_buf_size;
  sock->recv_buffer_size = recv_buf_size;

  return 0;
}

/*
 * Free the socket structure SOCK. Return a non-zero value on error.
 */
int
sock_free (socket_t sock)
{
  if (sock->remote_host)
    xfree (sock->remote_host);
  if (sock->local_host)
    xfree (sock->local_host);
  if (sock->recv_buffer)
    xfree(sock->recv_buffer);
  if (sock->send_buffer)
    xfree(sock->send_buffer);
  if (sock->flags & SOCK_FLAG_LISTENING && sock->data)
    xfree (sock->data);
  if (sock->recv_pipe)
    xfree (sock->recv_pipe);
  if (sock->send_pipe)
    xfree (sock->send_pipe);
  xfree(sock);

  return 0;
}

/*
 * Get local and remote addresses/ports of socket SOCK and save them
 * into the socket structure.
 */
int
sock_intern_connection_info (socket_t sock)
{
  struct sockaddr_in s;
  socklen_t size = sizeof(s);
  unsigned short port;
  unsigned addr;

  if (!getpeername(sock->sock_desc, (struct sockaddr *) &s, &size))
    {
      addr = ntohl(s.sin_addr.s_addr);
      port = ntohs(s.sin_port);
    }
  else
    {
      addr = INADDR_LOOPBACK;
      port = 0;
    }
  sock->remote_port = port;
  sock->remote_addr = addr;

  size = sizeof(s);
  if (!getsockname(sock->sock_desc, (struct sockaddr *) &s, &size))
    {
      addr = ntohl(s.sin_addr.s_addr);
      port = ntohs(s.sin_port);
    }
  else
    {
      addr = INADDR_LOOPBACK;
      port = 0;
    }
  sock->local_port = port;
  sock->local_addr = addr;

  return 0;
}

/*
 * Create a socket structure from the file descriptor FD.  Return NULL
 * on error.
 */
socket_t
sock_create (int fd)
{
  socket_t sock;

#ifdef __MINGW32__
  unsigned long blockMode = 1;

  if(ioctlsocket(fd, FIONBIO, &blockMode) == SOCKET_ERROR)
    {
      log_printf(LOG_ERROR, "ioctlsocket: %s\n", NET_ERROR);
      return NULL;
    }
#else
  if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0)
    {
      log_printf(LOG_ERROR, "fcntl: %s\n", NET_ERROR);
      return NULL;
    }
#endif

  sock = sock_alloc();
  if (sock)
    {
      do
	{
	  socket_id++;
	  socket_id &= (SOCKET_MAX_IDS-1);
	}
      while(sock_lookup_table[socket_id]);

      sock->socket_id = socket_id;
      sock->sock_desc = fd;
      sock->flags |= SOCK_FLAG_CONNECTED;
      sock_intern_connection_info (sock);
    }
  return sock;
}

#ifndef __MINGW32__

/*
 * Create a socket structure from the two file descriptors in FD.
 * Return NULL on error.
 */
socket_t
pipe_create (int read_fd, int write_fd)
{
  socket_t sock;

  if (fcntl(read_fd, F_SETFL, O_NONBLOCK) < 0)
    {
      log_printf(LOG_ERROR, "fcntl: %s\n", SYS_ERROR);
      return NULL;
    }

  if (fcntl(write_fd, F_SETFL, O_NONBLOCK) < 0)
    {
      log_printf(LOG_ERROR, "fcntl: %s\n", SYS_ERROR);
      return NULL;
    }

  sock = sock_alloc();
  if (sock)
    {
      do
	{
	  socket_id++;
	  socket_id &= (SOCKET_MAX_IDS-1);
	}
      while(sock_lookup_table[socket_id]);
      sock->socket_id = socket_id;
      sock->pipe_desc[READ] = read_fd;
      sock->pipe_desc[WRITE] = write_fd;
      sock->flags |= SOCK_FLAG_PIPE;
    }

  return sock;
}

#endif /* __MINGW32__ */

/*
 * Disconnect the socket SOCK from the network and calls the
 * disconnect function for the socket if set.  Return a non-zero
 * value on error.
 */
int
sock_disconnect (socket_t sock)
{
#ifndef __MINGW32__
  /* shutdown the pipe */
  if(sock->flags & SOCK_FLAG_PIPE)
    {
      if (!(sock->flags & SOCK_FLAG_LISTENING))
	{
	  if(close(sock->pipe_desc[WRITE]) < 0)
	    log_printf(LOG_ERROR, "close: %s\n", SYS_ERROR);
	  if(close(sock->pipe_desc[READ]) < 0)
	    log_printf(LOG_ERROR, "close: %s\n", SYS_ERROR);
	  sock->pipe_desc[READ] = INVALID_HANDLE;
	  sock->pipe_desc[WRITE] = INVALID_HANDLE;
	}

      /* restart pipe server listening */
      if (sock->parent)
	sock->parent->flags &= ~SOCK_FLAG_INITED;
    }
  else
#endif
    /* shutdown the network socket */
  if(sock->sock_desc != INVALID_SOCKET)
    {
      /* shutdown client connection */
      if(sock->flags & SOCK_FLAG_CONNECTED)
	{
	  if(shutdown(sock->sock_desc, 2) < 0)
	    log_printf(LOG_ERROR, "shutdown: %s\n", NET_ERROR);
	  connected_sockets--;
	}
      
      /* close the server/client socket */
      if(CLOSE_SOCKET(sock->sock_desc) < 0)
	log_printf(LOG_ERROR, "close: %s\n", NET_ERROR);

      sock->sock_desc = INVALID_SOCKET;
    }
  else
    {
      log_printf(LOG_ERROR, "disconnecting unconnected socket\n");
      return 0;
    }

  return 0;
}

/*
 * Write LEN bytes from the memory location pointed to by BUF
 * to the output buffer of the socket SOCK.  Also try to flush the
 * buffer to the network socket of SOCK if possible.  Return a non-zero
 * value on error, which normally means a buffer overflow.
 */
int
sock_write (socket_t sock, char * buf, int len)
{
  int ret;
  int space;

  if (sock->flags & SOCK_FLAG_KILLED)
    return 0;

  while (len > 0)
    {
      /* Try to flush the queue of this socket */
      if(sock->write_socket && !sock->unavailable)
	{
	  if ((ret = sock->write_socket(sock)) != 0)
	    return ret;
	}

      if (sock->send_buffer_fill >= sock->send_buffer_size)
	{
	  /* Queue is full, unlucky socket ... */
	  log_printf(LOG_ERROR,
		     "send buffer overflow on socket %d\n",
		     sock->sock_desc);
	
	  if (sock->kicked_socket)
	    sock->kicked_socket(sock, 1);

	  return -1;
	}
    
      /* Now move as much of BUF into the send queue */
      if (sock->send_buffer_fill + len < sock->send_buffer_size)
	{
	  memcpy(sock->send_buffer + sock->send_buffer_fill, buf, len);
	  sock->send_buffer_fill+=len;
	  len = 0;
	}
      else
	{
	  space = sock->send_buffer_size - sock->send_buffer_fill;
	  memcpy(sock->send_buffer + sock->send_buffer_fill, buf, space);
	  sock->send_buffer_fill+=space;
	  len -= space;
	  buf += space;
	}
    }

  return 0;
}

/*
 * Print a formatted string on the socket SOCK.  FMT is the printf()-
 * style format string, which describes how to format the optional
 * arguments.  See the printf(3) manual page for details.
 */
int
sock_printf(socket_t sock, const char *fmt, ...)
{
  va_list args;
  static char buffer[VSNPRINTF_BUF_SIZE];
  unsigned len;

  if (sock->flags & SOCK_FLAG_KILLED)
    return 0;

  va_start (args, fmt);
  len = vsnprintf (buffer, VSNPRINTF_BUF_SIZE, fmt, args);
  va_end (args);

  /* Just to be sure... */
  if (len > sizeof(buffer))
    len = sizeof(buffer);

  return sock_write(sock, buffer, len);
}
