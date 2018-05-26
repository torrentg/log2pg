
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

#ifndef VECTOR_H
#define VECTOR_H

#include <stddef.h>
#include <stdbool.h>

/**************************************************************************//**
 * @brief List of objects.
 */
typedef struct vector_t
{
  //! Pointers to objects where first member is a string.
  void **data;
  //! List size.
  size_t size;
  //! List capacity
  size_t capacity;
} vector_t;

/**************************************************************************
 * Function declarations.
 */
extern int vector_find(const vector_t *lst, const char *value);
extern bool vector_contains(const vector_t *lst, const void *obj);
extern int vector_insert(vector_t *lst, void *obj);
extern int vector_remove(vector_t *lst, int pos, void (*item_free)(void *));
extern void vector_reset(vector_t *lst, void (*item_free)(void*));
extern char* vector_print(const vector_t *lst);
extern int vector_reserve(vector_t *lst, size_t capacity);

#endif

