
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

#ifndef MAP_H
#define MAP_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/**************************************************************************//**
 * @brief Map bucket where key is a hash.
 */
typedef struct map_bucket_t
{
  //! Bucket key.
  size_t key;
  //! Bucket value (NULL = not assigned).
  void *value;
} map_bucket_t;

/**************************************************************************//**
 * @brief Basic hashmap whith positive keys.
 * @details Implements open addressing with linear probing and single-slot stepping.
 * @see https://en.wikipedia.org/wiki/Open_addressing
 */
typedef struct map_t
{
  //! Map buckets.
  map_bucket_t *data;
  //! Number of buckets.
  uint32_t capacity;
  //! Number of entries.
  uint32_t size;
} map_t;

/**************************************************************************//**
 * @brief Map iterator.
 * @details Insert or remove invalidates iterator.
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
extern map_bucket_t* map_next(const map_t *map, map_iterator_t *it);
extern void* map_find(const map_t *map, size_t key);
extern bool map_insert(map_t *map, size_t key, void *value);
extern bool map_remove(map_t *map, size_t key, void (*item_free)(void *));
extern void map_reset(map_t *map, void (*item_free)(void *));

#endif

