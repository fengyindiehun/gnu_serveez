/*
 * alist.h - array list interface
 *
 * Copyright (C) 2000, 2001 Stefan Jahn <stefan@lkcc.org>
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
 * $Id: alist.h,v 1.2 2001/01/29 22:41:32 ela Exp $
 *
 */

#ifndef __ALIST_H__
#define __ALIST_H__ 1

#include "libserveez/defines.h"

/* general array list defines */
#define ARRAY_BITS 4                       /* values 1 .. 6 possible */
#define ARRAY_SIZE (1 << ARRAY_BITS)       /* values 1 .. 64 possible */
#define ARRAY_MASK ((1 << ARRAY_SIZE) - 1)

/* 
 * On 32 bit architectures ARRAY_SIZE is no larger than 32 and on 64 bit
 * architectures it is no larger than 64. It specifies the number of bits
 * the `alist->fill' (unsigned long) field can hold.
 */

/* array chunk structure */
typedef struct array_chunk array_t;
struct array_chunk
{
  array_t *next;           /* pointer to next array chunk */
  array_t *prev;           /* pointer to previous array chunk */
  unsigned long offset;    /* first array index in this chunk */
  unsigned long fill;      /* usage bit-field */
  unsigned long size;      /* size of this chunk */
  void *value[ARRAY_SIZE]; /* value storage */
};

/* top level array list structure */
typedef struct array_list alist_t;
struct array_list
{
  unsigned long length; /* size of the array (last index plus one) */
  unsigned long size;   /* element count */
  array_t *first;       /* first array chunk */
  array_t *last;        /* last array chunk */
};

__BEGIN_DECLS

/* 
 * Exported array list functions. An array list is a kind of data array 
 * which grows and shrinks on demand. It unifies the advantages of chained
 * lists (less memory usage than simple arrays) and arrays (faster access 
 * to specific elements). This implementation can handle gaps in between
 * the array elements.
 */

SERVEEZ_API alist_t * alist_create __P ((void));
SERVEEZ_API void alist_destroy __P ((alist_t *list));
SERVEEZ_API void alist_add __P ((alist_t *list, void *value));
SERVEEZ_API void alist_clear __P ((alist_t *list));
SERVEEZ_API unsigned long alist_contains __P ((alist_t *list, void *value));
SERVEEZ_API void * alist_get __P ((alist_t *list, unsigned long index));
SERVEEZ_API unsigned long alist_index __P ((alist_t *list, void *value));
SERVEEZ_API void * alist_delete __P ((alist_t *list, unsigned long index));
SERVEEZ_API unsigned long alist_delete_range __P ((alist_t *, unsigned long, 
						   unsigned long));
SERVEEZ_API void * alist_set __P ((alist_t *, unsigned long, void *));
SERVEEZ_API void * alist_unset __P ((alist_t *list, unsigned long index));
SERVEEZ_API unsigned long alist_size __P ((alist_t *list));
SERVEEZ_API unsigned long alist_length __P ((alist_t *list));
SERVEEZ_API void alist_insert __P ((alist_t *, unsigned long, void *));
SERVEEZ_API void ** alist_values __P ((alist_t *list));
SERVEEZ_API void alist_pack __P ((alist_t *list));

__END_DECLS

#endif /* not __ALIST_H__ */