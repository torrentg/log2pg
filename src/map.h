
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

/**************************************************************************//**
 * @brief Map bucket with positive int key.
 */
typedef struct map_bucket_t
{
  //! Bucket key (negative values = not assigned).
  int key;
  //! Bucket value.
  void *value;
} map_bucket_t;

/**************************************************************************//**
 * @brief Basic hashmap whith positive int keys.
 * @details Collisions are resolved using open addressing.
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
extern void* map_find(const map_t *map, int key);
extern int map_insert(map_t *map, int key, void *value);
extern int map_remove(map_t *map, int key, void (*item_free)(void *));
extern void map_reset(map_t *map, void (*item_free)(void *));

#endif

