
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

#include "log2pg.h"
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include "map.h"

#define INITIAL_NUM_BUCKETS 8
#define MAX_LOAD_FACTOR 0.75
#define RESIZE_FACTOR 2

/**************************************************************************//**
 * @brief Resize a map.
 * @details Can be a fresh map (data=NULL).
 * @param[in,out] map The hash map.
 * @param[in] new_capacity The new map capacity.
 * @return 0=OK, 1=KO.
 */
static int map_resize(map_t *map, size_t new_capacity)
{
  if (map->size > map->capacity || new_capacity <= map->capacity) {
    assert(false);
    return(1);
  }

  map_bucket_t *new_buckets = (map_bucket_t *) calloc(new_capacity, sizeof(map_bucket_t));
  if (new_buckets == NULL) {
    return(1);
  }

  for(size_t i=0; i<new_capacity; i++) {
    new_buckets[i].key = -1;
    new_buckets[i].value = NULL;
  }

  size_t old_capacity = map->capacity;
  map_bucket_t *old_buckets = map->data;

  map->data = new_buckets;
  map->capacity = new_capacity;
  map->size = 0;

  if (old_buckets == NULL || old_capacity == 0) {
    return(0);
  }

  int rc = 0;
  for(size_t i=0; i<old_capacity; i++) {
    if (old_buckets[i].key >= 0) {
      rc |= map_insert(map, old_buckets[i].key, old_buckets[i].value);
    }
  }

  free(old_buckets);
  return(rc);
}

/**************************************************************************//**
 * @brief Reset map content.
 * @details If item_free function is not NULL then deallocs all items.
 * @param[in,out] map The hash map.
 * @param[in] item_free Function to free an item (can be NULL).
 */
void map_reset(map_t *map, void (*item_free)(void*))
{
  if (map == NULL) {
    return;
  }

  assert(map->size <= map->capacity);

  if (item_free != NULL && map->data != NULL) {
    for(size_t i=0; i<map->capacity; i++) {
      if (map->data[i].key >= 0) {
        item_free(map->data[i].value);
        assert(map->size > 0);
        map->size--;
      }
    }
    assert(map->size == 0);
  }

  free(map->data);
  map->data = NULL;
  map->capacity = 0;
  map->size = 0;
}

/**************************************************************************//**
 * @brief Search the bucket for the given key.
 * @details If key found, the value = key, else value < 0.
 * @param[in] map The hash map.
 * @param[in] key The object key.
 * @return Bucket index or a negative value if not found.
 */
static int map_find_bucket(const map_t *map, int key)
{
  assert(key >= 0);
  if (map->capacity == 0) {
    return(-1);
  }

  int ipos0 = key%(map->capacity);
  int ipos1 = ipos0;

  do {
    if (map->data[ipos1].key == key) {
      return(ipos1);
    }
    if (map->data[ipos1].key < 0) {
      return(ipos1);
    }
    ipos1 = (ipos1+1)%(map->capacity);
  } while(ipos1 != ipos0);

  assert(false);
  return(-1);
}

/**************************************************************************//**
 * @brief Returns the value linked to key.
 * @param[in] map The hash map.
 * @param[in] key The object key.
 * @return value or NULL if not found.
 */
void* map_find(const map_t *map, int key)
{
  assert(key >= 0);
  assert(map != NULL);
  assert(map->size <= map->capacity);
  if (map == NULL || key < 0) {
    return(NULL);
  }

  int ipos = map_find_bucket(map, key);
  if (ipos < 0 || map->data[ipos].key != key) {
    return(NULL);
  }
  else {
    return(map->data[ipos].value);
  }
}

/**************************************************************************//**
 * @brief Inserts an object to map.
 * @details Resizes map if required.
 *          If the key exists then replaces the value.
 * @param[in] map The hash map.
 * @param[in] key The object key.
 * @param[in] value The object value.
 * @return 0=OK, 1=KO.
 */
int map_insert(map_t *map, int key, void *value)
{
  assert(key >= 0);
  assert(map != NULL);
  assert(map->size <= map->capacity);
  if (map == NULL || key < 0) {
    return(1);
  }

  int rc = 0;
  if (map->data == NULL || map->capacity == 0) {
    rc = map_resize(map, INITIAL_NUM_BUCKETS);
  }
  else if ((float)(map->size+1)/(float)(map->capacity) > MAX_LOAD_FACTOR) {
    rc = map_resize(map, RESIZE_FACTOR*(map->capacity));
  }
  if (rc != 0) {
    return(1);
  }

  int ipos = map_find_bucket(map, key);
  if (ipos < 0) {
    return(1);
  }

  if (map->data[ipos].key != key) {
    assert(map->data[ipos].key < 0);
    map->data[ipos].key = key;
    map->size++;
  }

  map->data[ipos].value = value;
  return(0);
}

/**************************************************************************//**
 * @brief Remove object from map.
 * @see https://en.wikipedia.org/wiki/Open_addressing
 * @see https://en.wikipedia.org/w/index.php?title=Hash_table&oldid=95275577
 * @param[in] map The hash map.
 * @param[in] key The object key.
 * @param[in] item_free Function to free an item (can be NULL).
 * @return 0=OK, 1=KO.
 */
int map_remove(map_t *map, int key, void (*item_free)(void*))
{
  assert(key >= 0);
  assert(map != NULL);
  assert(map->size <= map->capacity);
  if (map == NULL || key < 0) {
    return(1);
  }

  if (map->data == NULL || map->capacity == 0) {
    return(1);
  }

  int ipos0 = map_find_bucket(map, key);
  if (ipos0 < 0 || map->data[ipos0].key != key) {
    // key is not in the table
    return(1);
  }
  if (item_free != NULL && map->data[ipos0].value != NULL) {
    item_free(map->data[ipos0].value);
  }

  int k = 0;
  size_t num = 0;
  int ipos1 = ipos0;
  while(num < map->capacity)
  {
    ipos1 = (ipos1+1)%(map->capacity);
    num++;

    if (map->data[ipos1].key < 0) {
      break;
    }

    k = (map->data[ipos1].key)%(map->capacity);
    if ((ipos1 > ipos0 && (k <= ipos0 || k > ipos1)) || (ipos1 < ipos0 && (k <= ipos0 && k > ipos1))) {
      map->data[ipos0] = map->data[ipos1];
      ipos0 = ipos1;
    }
  }

  map->data[ipos0].key = -1;
  map->data[ipos0].value = NULL;
  map->size--;
  return(0);
}

/**************************************************************************//**
 * @brief Iterate over map elements.
 * @param[in] map The hash map.
 * @param[in,out] it Map iterator.
 * @return Pointer to next bucket or NULL if error or no more elements.
 */
map_bucket_t* map_next(const map_t *map, map_iterator_t *it)
{
  assert(map != NULL);
  assert(map->size <= map->capacity);
  assert(it != NULL);
  assert(it->pos < map->capacity);
  assert(it->num <= map->size);
  if (map == NULL || it == NULL || it->num > (it->pos+1) || it->pos >= map->capacity || it->num >= map->size) {
    return(NULL);
  }

  for(size_t i=it->pos+(it->num==0?0:1); i<map->capacity; i++) {
    if (map->data[i].key >= 0) {
      it->pos = i;
      it->num++;
      return(&(map->data[i]));
    }
  }

  assert(false);
  return(NULL);
}
