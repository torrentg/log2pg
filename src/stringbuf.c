
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
#include "stringbuf.h"

#define RESIZE_FACTOR 2

/**************************************************************************//**
 * @brief Resize a string.
 * @details Can be a fresh string (data=NULL).
 * @param[in,out] obj The stringbuf object.
 * @param[in] new_capacity The new string capacity.
 * @return true=OK, false=KO.
 */
static bool stringbuf_resize(stringbuf_t *obj, size_t new_capacity)
{
  assert(obj != NULL);
  assert(obj->capacity < new_capacity);

  char *tmp = (char*) realloc(obj->data, (new_capacity)*sizeof(char));
  if (tmp == NULL) {
    return(false);
  }

  obj->data = tmp;
  obj->capacity = new_capacity;
  return(true);
}

/**************************************************************************//**
 * @brief Appends new contents to string.
 * @details Resize data if required.
 * @param[in,out] obj The stringbuf object.
 * @param[in] str Content to append.
 * @param len Content length
 * @return 0=OK, 1=KO.
 */
int stringbuf_append_n(stringbuf_t *obj, const char *str, size_t len)
{
  assert(obj != NULL);
  assert(obj->data != NULL || obj->capacity == 0);
  assert(obj->length < obj->capacity || (obj->capacity == 0 && obj->length == 0));

  if (obj == NULL || str == NULL || len == 0) {
    assert(false);
    return(1);
  }

  if (obj->capacity < obj->length + len + 1) {
    size_t new_capacity = MAX(RESIZE_FACTOR*obj->capacity, obj->length+len+1);
    if (!stringbuf_resize(obj, new_capacity)) {
      return(1);
    }
  }

  strncpy(obj->data + obj->length, str, len);
  obj->length += len;
  obj->data[obj->length] = '\0';

  return(0);
}

/**************************************************************************//**
 * @brief Appends new contents to string.
 * @details Resize data if required.
 * @param[in,out] obj The stringbuf object.
 * @param[in] str Content to append.
 * @return 0=OK, 1=KO.
 */
int stringbuf_append(stringbuf_t *obj, const char *str)
{
  return stringbuf_append_n(obj, str, strlen(str));
}

/**************************************************************************//**
 * @brief Reset string content (frees content but not object).
 * @param[in,out] obj String to reset.
 */
void stringbuf_reset(stringbuf_t *obj)
{
  if (obj == NULL) return;
  free(obj->data);
  obj->data = NULL;
  obj->length = 0;
  obj->capacity = 0;
}

/**************************************************************************//**
 * @brief Replace a substring with the given value.
 * @details Minimize memory alloc and bytes movements.
 * @param[in] obj The stringbuf object.
 * @param[in] from Substring to search and replace.
 * @param[in] to Substring to put in place of from (can be NULL = '').
 * @return Number of replacements done, negative value if error.
 */
int stringbuf_replace(stringbuf_t *obj, const char *from, const char *to)
{
  if (obj == NULL || from == NULL) {
    assert(false);
    return(0);
  }
  if (obj->data == NULL || obj->length == 0 || *from == '\0') {
    return(0);
  }
  if (to == NULL) {
    to = "";
  }

  char *ptr1;
  char *ptr2;
  size_t count = 0;
  size_t len1 = strlen(from);
  size_t len2 = strlen(to);

  // counting the number of replacements to do
  count = 0;
  ptr1 = (char *) obj->data;
  for (count=0; (ptr2=strstr(ptr1, from)); ++count) {
    ptr1 = ptr2 + len1;
  }

  // return if no matches
  if (count == 0) {
    return(0);
  }

  // memory management to ensure capacity
  size_t required_len = obj->length + count*(len2-len1);
  if (required_len+1 > obj->capacity) {
    size_t new_capacity = MAX(RESIZE_FACTOR*obj->capacity, required_len+1);
    if(!stringbuf_resize(obj, new_capacity)) {
      return(-1);
    }
  }

  // make the replacements
  ptr1 = obj->data;
  ptr2 = obj->data;

  if (len1 < len2) {
    ptr2 += required_len - obj->length;
    memmove(ptr2, ptr1, obj->length+1);
  }

  while (count--) {
    char *tmp = strstr(ptr2, from);
    size_t len = tmp-ptr2;
    if (len > 0) {
      memmove(ptr1, ptr2, len);
      ptr1 += len;
      ptr2 += len;
    }
    strncpy(ptr1, to, len2);
    ptr1 += len2;
    ptr2 += len1;
  }

  if (len1 >= len2) {
    memmove(ptr1, ptr2, strlen(ptr2)+1);
  }

  // set length and return
  obj->length = required_len;
  return(count);
}
