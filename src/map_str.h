
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

#ifndef MAP_STR_H
#define MAP_STR_H

#include <stdint.h>
#include <stdbool.h>

/**************************************************************************//**
 * @brief Map bucket where key is a string.
 */
typedef struct map_str_bucket_t
{
  //! Bucket key.
  const char *key;
  //! Bucket value (NULL = not assigned).
  void *value;
} map_str_bucket_t;

/**************************************************************************//**
 * @brief Basic hashmap whith string keys.
 * @details Implements open addressing with linear probing and single-slot stepping.
 * @see https://en.wikipedia.org/wiki/Open_addressing
 */
typedef struct map_str_t
{
  //! Map buckets.
  map_str_bucket_t *data;
  //! Number of buckets.
  uint32_t capacity;
  //! Number of entries.
  uint32_t size;
} map_str_t;

/**************************************************************************//**
 * @brief Map iterator.
 * @details map_insert() and map_remove() can invalidate iterators.
 */
typedef struct map_str_iterator_t
{
  //! Last visited index.
  uint32_t pos;
  //! Number of visited elements (0 = fresh iterator).
  uint32_t num;
} map_str_iterator_t;

/**************************************************************************
 * Function declarations.
 */
extern map_str_bucket_t* map_str_next(const map_str_t *map, map_str_iterator_t *it);
extern void* map_str_find(const map_str_t *map, const char *key);
extern bool map_str_insert(map_str_t *map, const char *key, void *value);
extern bool map_str_remove(map_str_t *map, const char *key, void (*item_free)(void *));
extern void map_str_reset(map_str_t *map, void (*item_free)(void *));

#endif

