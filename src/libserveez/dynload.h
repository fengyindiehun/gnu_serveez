/*
 * dynload.h - dynamic server loading interface
 *
 * Copyright (C) 2001 Stefan Jahn <stefan@lkcc.org>
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
 * $Id: dynload.h,v 1.2 2001/04/04 14:23:14 ela Exp $
 *
 */

#ifndef __DYNLOAD_H__
#define __DYNLOAD_H__ 1

#include "libserveez/defines.h"

/* Structure holding a shared libraries info. */
typedef struct
{
  void *handle; /* its handle */
  char *file;   /* the shared libraries filename */
  int ref;      /* reference counter */
}
dyn_library_t;

__BEGIN_DECLS

SERVEEZ_API void svz_dynload_init __P ((void));
SERVEEZ_API void svz_dynload_finalize __P ((void));

SERVEEZ_API svz_servertype_t *server_load __P ((char *description));
SERVEEZ_API int server_unload __P ((char *description));

__END_DECLS

#endif /* not __DYNLOAD_H__ */
