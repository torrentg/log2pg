
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
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include "map_str.h"

#define INITIAL_NUM_BUCKETS 8
#define MAX_LOAD_FACTOR 0.75
#define RESIZE_FACTOR 2

// Forward declaration.
static bool map_str_insert_internal(map_str_t *, const char *, void *, bool);

/**************************************************************************//**
 * @brief Hash function for string using the djb2 string hash function.
 * @see http://www.cse.yorku.ca/~oz/hash.html
 * @param[in] str String.
 * @return The string hash.
 */
static uint32_t map_str_hash(const char *str)
{
  uint32_t ret = 5381U;
  const unsigned char *p = (const unsigned char *) str;

  while (*p != '\0') {
    ret = (ret << 5) + ret + *p;
    ++p;
  }

  return ret;
}

/**************************************************************************//**
 * @brief Resize a map.
 * @details Can be a fresh map (data=NULL).
 * @param[in,out] map The hash map.
 * @param[in] new_capacity The new map capacity.
 * @return true=OK, false=KO.
 */
static bool map_str_resize(map_str_t *map, uint32_t new_capacity)
{
  if (map->size > map->capacity || new_capacity <= map->capacity) {
    assert(false);
    return(false);
  }

  map_str_bucket_t *new_buckets = (map_str_bucket_t *) calloc(new_capacity, sizeof(map_str_bucket_t));
  if (new_buckets == NULL) {
    return(false);
  }

  for(uint32_t i=0; i<new_capacity; i++) {
    new_buckets[i].key = NULL;
    new_buckets[i].value = NULL;
  }

  uint32_t old_capacity = map->capacity;
  map_str_bucket_t *old_buckets = map->data;

  map->data = new_buckets;
  map->capacity = new_capacity;
  map->size = 0;

  if (old_buckets == NULL || old_capacity == 0) {
    return(true);
  }

  bool rc = true;
  for(uint32_t i=0; i<old_capacity; i++) {
    if (old_buckets[i].value != NULL) {
      rc &= map_str_insert_internal(map, old_buckets[i].key, old_buckets[i].value, false);
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
void map_str_reset(map_str_t *map, void (*item_free)(void*))
{
  if (map == NULL) {
    return;
  }

  assert(map->size <= map->capacity);

  if (map->data != NULL) {
    for(uint32_t i=0; i<map->capacity; i++) {
      free((char*)map->data[i].key);
      if (map->data[i].value != NULL) {
        if (item_free != NULL) {
          item_free(map->data[i].value);
        }
        assert(map->size > 0);
        map->size--;
      }
    }
  }

  assert(map->size == 0);

  free(map->data);
  map->data = NULL;
  map->capacity = 0;
  map->size = 0;
}

/**************************************************************************//**
 * @brief Search the index bucket for the given key.
 * @param[in] map The hash map.
 * @param[in] key The object key (can exist in map or not).
 * @return Bucket index for the given key, or index bigger than capacity if error.
 */
static uint32_t map_str_find_bucket(const map_str_t *map, const char *key)
{
  assert(key != NULL);

  if (map->capacity == 0) {
    return(UINT32_MAX);
  }

  uint32_t ipos0 = map_str_hash(key)%(map->capacity);
  uint32_t ipos1 = ipos0;

  do {
    if (map->data[ipos1].key != NULL && strcmp(key, map->data[ipos1].key) == 0) {
      return(ipos1);
    }
    if (map->data[ipos1].value == NULL) {
      return(ipos1);
    }
    ipos1 = (ipos1+1)%(map->capacity);
  } while(ipos1 != ipos0);

  assert(false);
  return(UINT32_MAX);
}

/**************************************************************************//**
 * @brief Returns the value linked to key.
 * @param[in] map The hash map.
 * @param[in] key The object key.
 * @return Value or NULL if not found.
 */
void* map_str_find(const map_str_t *map, const char *key)
{
  assert(map->size <= map->capacity);

  if (map == NULL || key == NULL) {
    assert(false);
    return(NULL);
  }

  uint32_t ipos = map_str_find_bucket(map, key);
  if (ipos >= map->capacity) {
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
 * @param[in] fresh Create a copy of key if not found in map.
 * @return true=OK, false=KO.
 */
static bool map_str_insert_internal(map_str_t *map, const char *key, void *value, bool fresh)
{
  assert(map->size <= map->capacity);
  if (map == NULL || key == NULL || value == NULL) {
    assert(false);
    return(false);
  }

  bool enough_mem = true;
  if (map->data == NULL || map->capacity == 0) {
    enough_mem = map_str_resize(map, INITIAL_NUM_BUCKETS);
  }
  else if ((float)(map->size+1)/(float)(map->capacity) > MAX_LOAD_FACTOR) {
    enough_mem = map_str_resize(map, RESIZE_FACTOR*(map->capacity));
  }
  if (!enough_mem) {
    return(false);
  }

  uint32_t ipos = map_str_find_bucket(map, key);
  if (ipos >= map->capacity) {
    return(false);
  }

  if (map->data[ipos].value == NULL) {
    if (fresh) {
      map->data[ipos].key = strdup(key);
    }
    else {
      map->data[ipos].key = key;
    }
    map->size++;
  }

  map->data[ipos].value = value;
  return(map->data[ipos].key != NULL); // check ENOMEM in strdup()
}

/**************************************************************************//**
 * @brief Inserts an object to map.
 * @details Resizes map if required.
 *          If the key exists then replaces the value.
 * @param[in] map The hash map.
 * @param[in] key The object key.
 * @param[in] value The object value.
 * @return true=OK, false=KO.
 */
bool map_str_insert(map_str_t *map, const char *key, void *value)
{
  return map_str_insert_internal(map, key, value, true);
}

/**************************************************************************//**
 * @brief Remove object from map.
 * @see https://en.wikipedia.org/wiki/Open_addressing
 * @see https://en.wikipedia.org/w/index.php?title=Hash_table&oldid=95275577
 * @param[in] map The hash map.
 * @param[in] key The object key.
 * @param[in] item_free Function to free an item (can be NULL).
 * @return true=OK, false=KO.
 */
bool map_str_remove(map_str_t *map, const char *key, void (*item_free)(void*))
{
  assert(map->size <= map->capacity);
  if (map == NULL || key == NULL) {
    assert(false);
    return(false);
  }

  if (map->data == NULL || map->capacity == 0) {
    return(false);
  }

  uint32_t ipos0 = map_str_find_bucket(map, key);
  if (ipos0 >= map->capacity || map->data[ipos0].value == NULL) {
    // key is not in the table
    return(false);
  }

  // remove element
  if (item_free != NULL) {
    item_free(map->data[ipos0].value);
  }
  free((char*)map->data[ipos0].key);
  map->data[ipos0].key = NULL;
  map->data[ipos0].value = NULL;

  uint32_t k = 0;
  uint32_t num = 0;
  uint32_t ipos1 = ipos0;
  while(num < map->capacity)
  {
    ipos1 = (ipos1+1)%(map->capacity);
    num++;

    if (map->data[ipos1].value == NULL) {
      break;
    }

    k = map_str_hash(map->data[ipos1].key)%(map->capacity);
    if ((ipos1 > ipos0 && (k <= ipos0 || k > ipos1)) || (ipos1 < ipos0 && (k <= ipos0 && k > ipos1))) {
      map->data[ipos0] = map->data[ipos1];
      ipos0 = ipos1;
    }
  }

  map->data[ipos0].key = NULL;
  map->data[ipos0].value = NULL;
  map->size--;
  return(true);
}

/**************************************************************************//**
 * @brief Iterate over map elements.
 * @param[in] map The hash map.
 * @param[in,out] it Map iterator.
 * @return Pointer to next bucket or NULL if error or no more elements.
 */
map_str_bucket_t* map_str_next(const map_str_t *map, map_str_iterator_t *it)
{
  assert(map != NULL);
  assert(map->size <= map->capacity);
  assert(it != NULL);
  assert(it->pos < map->capacity);
  assert(it->num <= map->size);
  if (map == NULL || it == NULL || it->num > (it->pos+1) || it->pos >= map->capacity || it->num >= map->size) {
    return(NULL);
  }

  for(uint32_t i=it->pos+(it->num==0?0:1); i<map->capacity; i++) {
    if (map->data[i].value != NULL) {
      it->pos = i;
      it->num++;
      return(&(map->data[i]));
    }
  }

  assert(false);
  return(NULL);
}
