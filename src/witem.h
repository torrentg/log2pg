
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

#ifndef WITEM_H
#define WITEM_H

#include "log2pg.h"
#include <stdio.h>
#include <pcre2.h>
#include "vector.h"
#include "entities.h"

/**************************************************************************//**
 * @brief Types of witems.
 */
typedef enum {
  WITEM_FILE = 0,
  WITEM_DIR
} witem_type_e;

/**************************************************************************//**
 * @brief Watched item (dir or file).
 * @details First member is 'char *' to be searchable.
 */
typedef struct witem_t
{
  //! Real filename with absolute path.
  char *filename;
  //! Type of item.
  witem_type_e type;
  //! Pointer to wdir or wfile (not owner).
  void *ptr;
  //! File stream.
  FILE *file;
  //! Current line.
  char *buffer;
  //! Buffer length.
  size_t buffer_length;
  //! Current position in buffer.
  size_t buffer_pos;
  //! Data to match regex starts.
  pcre2_match_data *md_starts;
  //! Data to match regex ends.
  pcre2_match_data *md_ends;
  //! Data to match regex values.
  pcre2_match_data *md_values;
  //! Number of table params.
  size_t num_params;
  //! Position in regex_values of table params.
  size_t *param_pos;
} witem_t;

/**************************************************************************
 * Function declarations.
 */
extern witem_t* witem_alloc(const char *filename, witem_type_e type, void *ptr);
extern void witem_free(void *obj);

#endif
