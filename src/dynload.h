/*
 * src/dynload.c - dynamic server loading interface
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
 * $Id: dynload.h,v 1.2 2001/01/28 03:26:54 ela Exp $
 *
 */

#ifndef __DYNLOAD_H__
#define __DYNLOAD_H__ 1

#if HAVE_CONFIG_H
# include <config.h>
#endif

#ifdef __MINGW32__
# define DYNLOAD_SUFFIX "dll"
#else
# define DYNLOAD_SUFFIX "so"
#endif

server_definition_t * dyn_load (char *description);

#endif /* not __DYNLOAD_H__ */
