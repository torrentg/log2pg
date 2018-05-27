
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

#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include "string.h"

#define RESIZE_FACTOR 2

#define MAX(a,b) (((a)>(b))?(a):(b))

/**************************************************************************//**
 * @brief Resize a string.
 * @details Can be a fresh string (data=NULL).
 * @param[in,out] obj The string object.
 * @param[in] new_capacity The new string capacity.
 * @return 0=OK, 1=KO.
 */
static int string_resize(string_t *obj, size_t new_capacity)
{
  assert(obj != NULL);
  assert(obj->capacity < new_capacity);

  char *tmp = (char*) realloc(obj->data, (new_capacity)*sizeof(char));
  if (tmp == NULL) {
    return(1);
  }

  obj->data = tmp;
  obj->capacity = new_capacity;
  return(0);
}

/**************************************************************************//**
 * @brief Appends new contents to string.
 * @details Resize data if required.
 * @param[in,out] obj The string object.
 * @param[in] str Content to append.
 * @param len Content length
 * @return 0=OK, 1=KO.
 */
int string_append_n(string_t *obj, const char *str, size_t len)
{
  assert(obj != NULL);
  assert(obj->data != NULL || obj->capacity == 0);
  assert(obj->length < obj->capacity || (obj->capacity == 0 && obj->length == 0));

  if (obj == NULL || str == NULL || len == 0) {
    assert(false);
    return(1);
  }

  int rc = 0;

  if (obj->capacity < obj->length + len + 1) {
    size_t new_capacity = MAX(RESIZE_FACTOR*obj->capacity, obj->length+len+1);
    rc = string_resize(obj, new_capacity);
  }

  if (rc == 0) {
    strncpy(obj->data + obj->length, str, len);
    obj->length += len;
    obj->data[obj->length] = '\0';
  }

  return(rc);
}

/**************************************************************************//**
 * @brief Appends new contents to string.
 * @details Resize data if required.
 * @param[in,out] obj The string object.
 * @param[in] str Content to append.
 * @return 0=OK, 1=KO.
 */
int string_append(string_t *obj, const char *str)
{
  return string_append_n(obj, str, strlen(str));
}

/**************************************************************************//**
 * @brief Reset string content (frees content but not object).
 * @param[in,out] obj String to reset.
 */
void string_reset(string_t *obj)
{
  if (obj == NULL) return;
  free(obj->data);
  obj->data = NULL;
  obj->length = 0;
  obj->capacity = 0;
}
