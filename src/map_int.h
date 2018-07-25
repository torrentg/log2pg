
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

#ifndef MAP_INT_H
#define MAP_INT_H

#include <stdint.h>
#include <stdbool.h>

/**************************************************************************//**
 * @brief Map bucket where key is an int.
 */
typedef struct map_bucket_int_t
{
  //! Bucket key.
  int key;
  //! Bucket value (NULL = not assigned).
  void *value;
} map_bucket_int_t;

/**************************************************************************//**
 * @brief Basic hashmap whith int keys.
 * @details Implements open addressing with linear probing and single-slot stepping.
 * @see https://en.wikipedia.org/wiki/Open_addressing
 */
typedef struct map_int_t
{
  //! Map buckets.
  map_bucket_int_t *data;
  //! Number of buckets.
  uint32_t capacity;
  //! Number of entries.
  uint32_t size;
} map_int_t;

/**************************************************************************//**
 * @brief Map iterator.
 * @details map_insert() and map_remove() can invalidate iterators.
 */
typedef struct map_iterator_t
{
  //! Last visited index.
  uint32_t pos;
  //! Number of visited elements (0 = fresh iterator).
  uint32_t num;
} map_iterator_t;

/**************************************************************************
 * Function declarations.
 */
extern map_bucket_int_t* map_int_next(const map_int_t *map, map_iterator_t *it);
extern void* map_int_find(const map_int_t *map, int key);
extern bool map_int_insert(map_int_t *map, int key, void *value);
extern bool map_int_remove(map_int_t *map, int key, void (*item_free)(void *));
extern void map_int_reset(map_int_t *map, void (*item_free)(void *));

#endif

