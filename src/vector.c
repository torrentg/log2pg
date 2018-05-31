
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

#define INITIAL_CAPACITY 1
#define RESIZE_FACTOR 2

/**************************************************************************//**
 * @brief Reset list content (frees content but not object).
 * @details If item_free function is not NULL then deallocs all items.
 * @param[in,out] lst List to reset.
 * @param[in] item_free Function to free an item (can be NULL).
 */
void vector_reset(vector_t *lst, void (*item_free)(void*))
{
  if (lst == NULL) return;
  if (lst->data != NULL && item_free != NULL) {
    for(size_t i=0; i<lst->size; i++) {
      if (lst->data[i] != NULL) {
        item_free(lst->data[i]);
      }
    }
  }
  free(lst->data);
  lst->data = NULL;
  lst->size = 0;
  lst->capacity = 0;
}

/**************************************************************************//**
 * @brief Returns the position of the item having the given name.
 * @details Assumes that vector objects are structs where first field is a 'char *'.
 * @param[in] lst List of objects.
 * @param[in] value Searched values (can be null).
 * @return Index of the requested item, negative value if not found.
 */
int vector_find(const vector_t *lst, const char *value)
{
  assert(lst != NULL);
  if (lst == NULL || lst->data == NULL) {
    return -1;
  }

  for(size_t i=0; i<lst->size; i++) {
    if (lst->data[i] == NULL) continue;
    char **str = (char **) lst->data[i];
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
 * @param[in] lst List of objects.
 * @param[in] obj Object to search.
 * @return Index of the requested item, negative value if not found.
 */
bool vector_contains(const vector_t *lst, const void *obj)
{
  if (lst == NULL || lst->data == NULL) {
    return false;
  }

  for(size_t i=0; i<lst->size; i++) {
    if (lst->data[i] == obj) return(true);
  }

  return(false);
}

/**************************************************************************//**
 * @brief Resize a vector.
 * @param[in,out] lst List of objects.
 * @param[in] new_size The new list capacity.
 * @return 0=OK, 1=KO.
 */
static int vector_resize(vector_t *lst, size_t new_capacity)
{
  void **tmp = (void**) realloc(lst->data, (new_capacity)*sizeof(void*));
  if (tmp == NULL) {
    return(1);
  }

  for(size_t i=lst->size; i<new_capacity; i++) {
    tmp[i] = NULL;
  }
  lst->data = tmp;
  lst->size = MIN(lst->size, new_capacity);
  lst->capacity = new_capacity;
  return(0);
}

/**************************************************************************//**
 * @brief Inserts an item at the end of the list.
 * @param[in,out] lst List of objects.
 * @param[in] obj Item to append (can be NULL).
 * @return 0=OK, otherwise=not appended.
 */
int vector_insert(vector_t *lst, void *obj)
{
  assert(lst != NULL);
  assert(lst->size <= lst->capacity);
  if (lst == NULL) {
    return(1);
  }

  int rc = 0;

  if (lst->data == NULL || lst->capacity == 0) {
    lst->size = 0;
    lst->capacity = 0;
    rc = vector_resize(lst, INITIAL_CAPACITY);
  }
  else if (lst->size == lst->capacity) {
    rc = vector_resize(lst, RESIZE_FACTOR*lst->capacity);
  }

  if (rc == 0) {
    assert(lst->size < lst->capacity);
    lst->data[lst->size] = obj;
    lst->size++;
  }

  return(rc);
}

/**************************************************************************//**
 * @brief Remove object from list.
 * @param[in,out] lst List of objects.
 * @param[in] pos Position of the object to remove.
 * @param[in] item_free Function to free an item (can be NULL).
 * @return 0=OK, 1=KO.
 */
int vector_remove(vector_t *lst, int pos, void (*item_free)(void *))
{
  assert(lst != NULL);
  assert(lst->size <= lst->capacity);
  assert(pos >= 0);
  assert(pos < (int)lst->size);
  if (lst == NULL || pos < 0 || (int)lst->size <= pos) {
    return(1);
  }

  if (lst->data != NULL && item_free != NULL) {
    item_free(lst->data[pos]);
  }

  lst->data[pos] = NULL;
  if (pos < (int)(lst->size)-1) {
    memmove(lst->data+pos, lst->data+pos+1, (lst->size-1-pos)*sizeof(void *));
  }
  lst->size--;

  return(0);
}

/**************************************************************************//**
 * @brief Request a change in capacity.
 * @details Requests that the vector capacity be at least enough to contain n elements.
 * @param[in,out] lst List of objects.
 * @param[in] capacity Minimum capacity for the vector.
 * @return 0=OK, 1=KO.
 */
int vector_reserve(vector_t *lst, size_t capacity)
{
  if (lst == NULL) {
    assert(false);
    return(1);
  }

  if (capacity <= lst->capacity) {
    return(0);
  }

  return vector_resize(lst, capacity);
}

/**************************************************************************//**
 * @brief Prints vector contents in a string as [obj1, obj2, obj3].
 * @details Assumes that vector objects are strings.
 *          Memory for the returned string is obtained with calloc, and
 *          can be freed with free(3).
 * @param[in,out] lst List of strings.
 * @return String representing the vector contents, NULL if error.
 */
char* vector_print(const vector_t *lst)
{
  if (lst == NULL) {
    assert(false);
    return(NULL);
  }

  size_t len = 2; // initial '[' + ending  ']'
  for (size_t i=0; i<lst->size; i++) {
    len += strlen((char*)(lst->data[i])) + 2; // ', ' takes 2 places
  }

  char *ret = (char *) calloc(len, sizeof(char));
  if (ret == NULL) {
    return(NULL);
  }

  char *ptr = ret+1;
  sprintf(ret, "[");
  for (size_t i=0; i<lst->size; i++) {
    sprintf(ptr, "%s", (char*)(lst->data[i]));
    ptr += strlen((char*)(lst->data[i]));
    if (i+1<lst->size) {
      sprintf(ptr, ", ");
      ptr += 2;
    }
  }
  sprintf(ptr, "]");
  return(ret);
}
