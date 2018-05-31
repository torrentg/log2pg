
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
#include <string.h>
#include <syslog.h>
#include <errno.h>
#include <assert.h>
#include "entities.h"
#include "string.h"
#include "wdata.h"

/**************************************************************************//**
 * @brief Frees memory space pointed by ptr.
 * @param[in] ptr Pointer to wdata object.
 */
void wdata_free(void *ptr)
{
  if (ptr == NULL) return;
  wdata_t *obj = (wdata_t *) ptr;

  syslog(LOG_DEBUG, "removed wdata [address=%p, item=%p, values=%p]",
         ptr, (void *)(obj->item), (void *)(&(obj->x)));

  free(ptr);
}

/**************************************************************************//**
 * @brief Serialize wdata values.
 * @param[in] item Witem object.
 * @param[in] str Current string.
 */
static char* wdata_values_str(witem_t *item, const char *str)
{
  string_t ret = {0};
  table_t *table = ((file_t *) item->ptr)->table;
  PCRE2_SIZE *ovector = pcre2_get_ovector_pointer(item->md_values);

  assert(table->parameters.size == item->num_params);

  string_append(&ret, "[");

  for(size_t i=0; i<table->parameters.size; i++)
  {
    if (ret.length > 1) {
      string_append(&ret, ", ");
    }

    string_append(&ret, table->parameters.data[i]);
    string_append(&ret, "=");
    size_t j= item->param_pos[i];
    size_t pos = ovector[2*(j+1)];
    int len = (int)(ovector[2*(j+1)+1] - ovector[2*(j+1)]);
    assert(len > 0);
    string_append_n(&ret, str+pos, len);
  }

  string_append(&ret, "]");
  return(ret.data);
}

/**************************************************************************//**
 * @brief Allocate and initialize a wdata.
 * @param[in] item Witem object.
 * @param[in] str Current string.
 * @return Initialized object or NULL if error.
 */
wdata_t* wdata_alloc(witem_t *item, const char *str)
{
  if (item == NULL || item->ptr == NULL) {
    assert(false);
    return(NULL);
  }

  size_t num_bytes = 0;
  PCRE2_SIZE *ovector = pcre2_get_ovector_pointer(item->md_values);

  // computing size to alloc
  for(size_t i=0; i<item->num_params; i++) {
    size_t j = item->param_pos[i];
    int len = (int)(ovector[2*(j+1)+1] - ovector[2*(j+1)]) + 1;
    assert(len > 0);
    num_bytes += len;
  }

  if (num_bytes == 0) {
    num_bytes++;
  }

  num_bytes += sizeof(witem_t *);

  // allocating object
  wdata_t *ret = (wdata_t *) malloc(num_bytes);
  if (ret == NULL) {
    return(NULL);
  }

  // setting values
  ret->item = item;
  char *ptr = &(ret->x);

  for(size_t i=0; i<item->num_params; i++) {
    size_t j = item->param_pos[i];
    int len = (int)(ovector[2*(j+1)+1] - ovector[2*(j+1)]);
    strncpy(ptr, str + ovector[2*(j+1)], len);
    ptr += len;
    *ptr = '\0';
    ptr++;
  }

  char *aux = wdata_values_str(item, str);
  syslog(LOG_DEBUG, "created wdata [address=%p, item=%p, values=%s]", (void *)ret, (void *)item, aux);
  free(aux);

  return(ret);
}
