
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "vector.h"
#include "utils.h"

#define INITIAL_CAPACITY 1
#define RESIZE_FACTOR 2

/**************************************************************************//**
 * @brief Reset list content (frees content but not object).
 * @details If item_free function is not NULL then deallocs all items.
 * @param[in,out] vector List to reset.
 * @param[in] item_free Function to free an item (can be NULL).
 */
void vector_reset(vector_t *vector, void (*item_free)(void*))
{
  if (vector == NULL) return;
  if (vector->data != NULL && item_free != NULL) {
    for(uint32_t i=0; i<vector->size; i++) {
      if (vector->data[i] != NULL) {
        item_free(vector->data[i]);
      }
    }
  }
  free(vector->data);
  vector->data = NULL;
  vector->size = 0;
  vector->capacity = 0;
}

/**************************************************************************//**
 * @brief Returns the position of the item having the given name.
 * @details Assumes that vector objects are structs where first field is a 'char *'.
 * @param[in] vector List of objects.
 * @param[in] value Searched values (can be null).
 * @return Index of the requested item, negative value if not found.
 */
int vector_find(const vector_t *vector, const char *value)
{
  assert(vector != NULL);
  if (vector == NULL || vector->data == NULL) {
    return -1;
  }

  for(uint32_t i=0; i<vector->size; i++) {
    if (vector->data[i] == NULL) continue;
    char **str = (char **) vector->data[i];
    if ((*str == NULL && value == NULL) ||
        (*str != NULL && strcmp(*str, value) == 0))
    {
      return(i);
    }
  }

  return(-1);
}

/**************************************************************************//**
 * @brief Check if list contains the given object.
 * @param[in] vector List of objects.
 * @param[in] obj Object to search.
 * @return Index of the requested item, negative value if not found.
 */
bool vector_contains(const vector_t *vector, const void *obj)
{
  if (vector == NULL || vector->data == NULL) {
    return false;
  }

  for(uint32_t i=0; i<vector->size; i++) {
    if (vector->data[i] == obj) return(true);
  }

  return(false);
}

/**************************************************************************//**
 * @brief Resize a vector.
 * @param[in,out] vector List of objects.
 * @param[in] new_size The new list capacity.
 * @return 0=OK, 1=KO.
 */
static int vector_resize(vector_t *vector, uint32_t new_capacity)
{
  void **tmp = (void**) realloc(vector->data, (new_capacity)*sizeof(void*));
  if (tmp == NULL) {
    return(1);
  }

  for(uint32_t i=vector->size; i<new_capacity; i++) {
    tmp[i] = NULL;
  }
  vector->data = tmp;
  vector->size = MIN(vector->size, new_capacity);
  vector->capacity = new_capacity;
  return(0);
}

/**************************************************************************//**
 * @brief Inserts an item at the end of the list.
 * @param[in,out] vector List of objects.
 * @param[in] obj Item to append (can be NULL).
 * @return 0=OK, otherwise=not appended.
 */
int vector_insert(vector_t *vector, void *obj)
{
  if (vector == NULL || vector->size > vector->capacity) {
    assert(false);
    return(1);
  }

  int rc = 0;

  if (vector->data == NULL || vector->capacity == 0) {
    vector->size = 0;
    vector->capacity = 0;
    rc = vector_resize(vector, INITIAL_CAPACITY);
  }
  else if (vector->size == vector->capacity) {
    size_t new_capacity = RESIZE_FACTOR*(size_t)(vector->capacity);
    if (new_capacity > UINT32_MAX) {
      rc = 1;
    }
    else {
      rc = vector_resize(vector, new_capacity);
    }
  }

  if (rc == 0) {
    assert(vector->size < vector->capacity);
    vector->data[vector->size] = obj;
    vector->size++;
  }

  return(rc);
}

/**************************************************************************//**
 * @brief Remove object from list.
 * @param[in,out] vector List of objects.
 * @param[in] pos Position of the object to remove.
 * @param[in] item_free Function to free an item (can be NULL).
 * @return 0=OK, 1=KO.
 */
int vector_remove(vector_t *vector, int pos, void (*item_free)(void *))
{
  if (vector == NULL || pos < 0 || (int)vector->size <= pos) {
    assert(false);
    return(1);
  }
  if (vector->data == NULL || vector->size == 0) {
    return(1);
  }

  if (item_free != NULL) {
    item_free(vector->data[pos]);
  }

  vector->data[pos] = NULL;
  if (pos < (int)(vector->size)-1) {
    memmove(vector->data+pos, vector->data+pos+1, (vector->size-1-pos)*sizeof(void *));
  }
  vector->size--;

  return(0);
}

/**************************************************************************//**
 * @brief Remove all objects from list (preserving capacity).
 * @param[in,out] vector List of objects.
 * @param[in] item_free Function to free an item (can be NULL).
 * @return 0=OK, 1=KO.
 */
int vector_clear(vector_t *vector, void (*item_free)(void *))
{
  if (vector == NULL) {
    assert(false);
    return(1);
  }

  if (vector->data == NULL || vector->size == 0) {
    return(0);
  }

  for(uint32_t i=0; i<vector->size; i++) {
    if (vector->data[i] != NULL && item_free != NULL) {
      item_free(vector->data[i]);
    }
    vector->data[i] = NULL;
  }

  vector->size = 0;
  return(0);
}

/**************************************************************************//**
 * @brief Request a change in capacity.
 * @details Requests that the vector capacity be at least enough to contain n elements.
 * @param[in,out] vector List of objects.
 * @param[in] capacity Minimum capacity for the vector.
 * @return 0=OK, 1=KO.
 */
int vector_reserve(vector_t *vector, uint32_t capacity)
{
  if (vector == NULL) {
    assert(false);
    return(1);
  }

  if (capacity <= vector->capacity) {
    return(0);
  }

  return vector_resize(vector, capacity);
}

/**************************************************************************//**
 * @brief Prints vector contents in a string as [obj1, obj2, obj3].
 * @details Assumes that vector objects are strings.
 *          Memory for the returned string is obtained with calloc, and
 *          can be freed with free(3).
 * @param[in,out] vector List of strings.
 * @return String representing the vector contents, NULL if error.
 */
char* vector_print(const vector_t *vector)
{
  if (vector == NULL) {
    assert(false);
    return(NULL);
  }

  int len = 2; // initial '[' + ending  ']'
  for (uint32_t i=0; i<vector->size; i++) {
    len += strlen((char*)(vector->data[i])) + 2; // ', ' takes 2 places
  }

  char *ret = (char *) calloc(len, sizeof(char));
  if (ret == NULL) {
    return(NULL);
  }

  char *ptr = ret+1;
  sprintf(ret, "[");
  for (uint32_t i=0; i<vector->size; i++) {
    sprintf(ptr, "%s", (char*)(vector->data[i]));
    ptr += strlen((char*)(vector->data[i]));
    if (i+1<vector->size) {
      sprintf(ptr, ", ");
      ptr += 2;
    }
  }
  sprintf(ptr, "]");
  return(ret);
}

/**************************************************************************//**
 * @brief Return a new vector with the same content.
 * @param[in] vector Vector to clone.
 * @return The cloned vector, empty vector if error.
 */
vector_t vector_clone(const vector_t *vector)
{
  vector_t ret = {0};

  if (vector->data != NULL && vector->capacity > 0 && vector->size > 0) {
    ret.data = memdup(vector->data, vector->size);
    if (ret.data != NULL) {
      ret.size = vector->size;
      ret.capacity = vector->size;
    }
  }

  return(ret);
}

/**************************************************************************//**
 * @brief Swap vector contents.
 * @param[in,out] vector1 First vector (not NULL).
 * @param[in,out] vector2 Second vector (not NULL).
 */
void vector_swap(vector_t *vector1, vector_t *vector2)
{
  if (vector1 == NULL || vector2 == NULL) {
    assert(false);
    return;
  }

  vector_t aux = *vector1;
  *vector1 = *vector2;
  *vector2 = aux;
}
