
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

#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <errno.h>
#include <assert.h>
#include "entities.h"
#include "witem.h"

/**************************************************************************//**
 * @brief Frees memory space pointed by ptr.
 * @param[in] ptr Pointer to witem object.
 */
void witem_free(void *ptr)
{
  if (ptr == NULL) return;
  witem_t *obj = (witem_t *) ptr;

  syslog(LOG_DEBUG, "removed witem [address=%p, filename=%s, type=%s]",
         ptr, obj->filename, (obj->type==WITEM_DIR?"dir":"file"));

  free(obj->filename);
  if (obj->file != NULL) {
    fclose(obj->file);
  }
  free(obj->buffer);
  pcre2_match_data_free(obj->md_starts);
  pcre2_match_data_free(obj->md_ends);
  pcre2_match_data_free(obj->md_values);
  free(obj->param_pos);
  free(ptr);
}

/**************************************************************************//**
 * @brief Initialize buffer and data linked to regex match.
 * @param[in,out] item Watched item to initialize.
 * @return 0=OK, otherwise=error.
 */
static int witem_init(witem_t *item)
{
  assert(item != NULL);
  assert(item->ptr != NULL);
  assert(item->file == NULL);
  assert(item->buffer == NULL);

  if (item->type != WITEM_FILE) {
    return(0);
  }

  // open file
  item->file = fopen(item->filename, "r");
  if (item->file == NULL) {
    syslog(LOG_WARNING, "error opening file '%s' - %s", item->filename, strerror(errno));
    return(1);
  }

  format_t *format = ((file_t *) item->ptr)->format;
  assert(format != NULL);

  item->buffer_length = format->maxlength;
  item->buffer_pos = 0;
  item->buffer = calloc(item->buffer_length, sizeof(char));
  if (item->buffer == NULL) {
    syslog(LOG_ERR, "%s", strerror(errno));
    return(1);
  }
  if (format->re_starts != NULL) {
    item->md_starts = pcre2_match_data_create_from_pattern(format->re_starts, NULL);
    if (item->md_starts == NULL) return(1);
  }
  if (format->re_ends != NULL) {
    item->md_ends = pcre2_match_data_create_from_pattern(format->re_ends, NULL);
    if (item->md_ends == NULL) return(1);
  }
  if (format->re_values != NULL) {
    item->md_values = pcre2_match_data_create_from_pattern(format->re_values, NULL);
    if (item->md_values == NULL) return(1);
  }

  table_t *table = ((file_t *) item->ptr)->table;
  assert(table != NULL);

  item->num_params = 0;
  if (table->parameters.size > 0) {
    item->param_pos = calloc(table->parameters.size, sizeof(size_t));
    for(size_t j=0; j<table->parameters.size; j++) {
      bool found = false;
      for(size_t i=0; i<format->parameters.size; i++) {
        if (strcmp(format->parameters.data[i], table->parameters.data[j]) == 0) {
          item->param_pos[item->num_params] = i;
          item->num_params++;
          found = true;
          break;
        }
      }
      if (!found) {
        syslog(LOG_ERR, "witem - table param not found in values");
        assert(false); // checked in dirs_check_parameters()
        return(1);
      }
    }
  }

  return(0);
}

/**************************************************************************//**
 * @brief Allocate and initialize a witem of type file.
 * @param[in] filename File name.
 * @param[in] type Type of item.
 * @param[in] ptr Pointer to wdir or wfile.
 * @return Initialized object or NULL if error.
 */
witem_t* witem_alloc(const char *filename, witem_type_e type, void *ptr)
{
  if (filename == NULL) {
    assert(false);
    return(NULL);
  }

  witem_t *ret = (witem_t *) calloc(1, sizeof(witem_t));
  if (ret == NULL) {
    syslog(LOG_ERR, "%s", strerror(errno));
    return(NULL);
  }

  ret->filename = strdup(filename);
  ret->type = type;
  ret->ptr = ptr;
  ret->file = NULL;
  ret->buffer = NULL;
  ret->buffer_length = 0;
  ret->buffer_pos = 0;
  ret->md_starts = NULL;
  ret->md_ends = NULL;
  ret->md_values = NULL;
  ret->num_params = 0;
  ret->param_pos = NULL;

  int rc = witem_init(ret);
  if (rc != 0 || ret->filename == NULL) {
    witem_free(ret);
    return(NULL);
  }

  if (type == WITEM_DIR) {
    syslog(LOG_DEBUG, "created witem [address=%p, filename=%s, type=dir]",
           (void *)ret, filename);
  }
  else {
    file_t *file = (file_t *)(ptr);
    syslog(LOG_DEBUG, "created witem [address=%p, filename=%s, type=file, format=%s, table=%s]",
           (void *)ret, filename, file->format->name, file->table->name);
  }

  return(ret);
}
