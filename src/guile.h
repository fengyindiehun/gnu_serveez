/*
 * guile.h - interface to guile core library declarations
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
 * $Id: guile.h,v 1.11 2002/03/02 13:45:20 ela Exp $
 *
 */

#ifndef __GUILE_H__
#define __GUILE_H__ 1

/*
 * Converts @code{SCM} into @code{char *} no matter if it is string or 
 * symbol. Returns @code{NULL} if it was neither. The new string must be 
 * explicitly @code{free()}d.
 */
#define guile_to_string(cell)                                   \
  (SCM_IMP (cell) || SCM_NULLP (cell) ? NULL :                  \
  (SCM_STRINGP (cell) ? gh_scm2newstr (cell, NULL) :            \
  (SCM_SYMBOLP (cell) ? gh_symbol2newstr (cell, NULL) : NULL)))

/* FAIL breaks to the label `out' and sets an error condition. */
#define FAIL() do { err = -1; goto out; } while(0)

/* Export these functions. */
int guile_to_integer (SCM, int *);
int guile_to_boolean (SCM, int *);
svz_array_t *guile_to_intarray (SCM, char *);
svz_array_t *guile_to_strarray (SCM, char *);
svz_hash_t *guile_to_hash (SCM, char *);
svz_hash_t *guile_to_optionhash (SCM, char *, int);
SCM guile_strarray_to_guile (svz_array_t *);
SCM guile_intarray_to_guile (svz_array_t *);
SCM guile_hash_to_guile (svz_hash_t *);
void guile_error (char *, ...);
int guile_load_config (char *);

int optionhash_validate (svz_hash_t *, int, char *, char *);
void optionhash_destroy (svz_hash_t *);
SCM optionhash_get (svz_hash_t *, char *);
int optionhash_extract_string (svz_hash_t *, char *, int, char *, char **, 
			       char *);

#endif /* not __GUILE_H__ */
