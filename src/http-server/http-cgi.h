/*
 * http-cgi.h - http cgi header file
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
 * $Id: http-cgi.h,v 1.8 2001/03/04 13:13:40 ela Exp $
 *
 */

#ifndef __HTTP_CGI_H__
#define __HTTP_CGI_H__

#if HAVE_CONFIG_H
# include <config.h>
#endif

#define _GNU_SOURCE

#include "http-proto.h"

#define MAX_CGI_DIR_LEN  256 /* length of a cgi script directory */

#define POST_METHOD 0            /* POST id */
#define GET_METHOD  1            /* GET id */
#define HTTP_NO_CGI ((char *)-1) /* 'no cgi' pointer */

#ifdef __MINGW32__
# define ENV_BLOCK_SIZE 2048 /* max. environment block size */
#else
# define ENV_LENGTH     256  /* length of an environment variable */
# define ENV_ENTRIES    64   /* max. amount of environment variables */
#endif

#define CGI_VERSION "CGI/1.0"

char *http_check_cgi (socket_t sock, char *request);
int http_cgi_exec (socket_t, HANDLE, HANDLE, char *, char *, int);
int http_post_response (socket_t sock, char *request, int flags);
int http_cgi_get_response (socket_t sock, char *request, int flags);
int http_cgi_write (socket_t sock);
int http_cgi_read (socket_t sock);
int http_cgi_disconnect (socket_t sock);
int http_cgi_died (socket_t sock);
void http_gen_cgi_apps (http_config_t *cfg);
void http_free_cgi_apps (http_config_t *cfg);

#endif /* __HTTP_CGI_H__ */
