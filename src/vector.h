
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
extern int vector_find(const vector_t *vector, const char *value);
extern bool vector_contains(const vector_t *vector, const void *obj);
extern int vector_insert(vector_t *vector, void *obj);
extern int vector_remove(vector_t *vector, int pos, void (*item_free)(void *));
extern int vector_clear(vector_t *vector, void (*item_free)(void *));
extern void vector_reset(vector_t *vector, void (*item_free)(void*));
extern char* vector_print(const vector_t *vector);
extern int vector_reserve(vector_t *vector, size_t capacity);
extern vector_t vector_clone(const vector_t *vector);
extern void vector_swap(vector_t *vector1, vector_t *vector2);

#endif

