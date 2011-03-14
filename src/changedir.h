/* changedir.h --- make ‘chdir’ available
 *
 * Copyright (C) 2011 Thien-Thi Nguyen
 * Copyright (C) 2000 Stefan Jahn <stefan@lkcc.org>
 *
 * This is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this package.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __CHANGEDIR_H__
#define __CHANGEDIR_H__ 1

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifdef __MINGW32__
#define chdir(path)  (SetCurrentDirectory (path) ? 0 : -1)
#endif

#endif  /* !defined __CHANGEDIR_H__ */
