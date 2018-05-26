
//===========================================================================
//
// log2pg - File forwarder to Postgresql database
// Copyright (C) 2018 Gerard Torrent
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
//
//===========================================================================

#ifndef STRING_H
#define STRING_H

#include <stddef.h>

/**************************************************************************//**
 * @brief Basic string object.
 * @details Created to avoid the cascade of callocs in string concatenation.
 */
typedef struct string_t
{
  //! String content ('\0' ended).
  char *data;
  //! String length.
  size_t length;
  //! String capacity.
  size_t capacity;
} string_t;

/**************************************************************************
 * Function declarations.
 */
extern int string_append(string_t *obj, const char *str);
extern int string_append_n(string_t *obj, const char *str, size_t len);
extern void string_reset(string_t *obj);

#endif
