
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

#ifndef ENTITIES_H
#define ENTITIES_H

#include <libconfig.h>
#include "format.h"
#include "table.h"
#include "vector.h"

/**************************************************************************//**
 * @brief File defined in configuration file.
 * @details First member is 'char *' to be searchable.
 */
typedef struct file_t
{
  //! File name pattern (without path)
  char *pattern;
  //! Pointer to format (not owner)
  format_t *format;
  //! Pointer to table (not owner)
  table_t *table;
  //! Discard file pattern.
  char *discard;
} file_t;

/**************************************************************************//**
 * @brief Directory defined in configuration file.
 * @details First member is 'char *' to be searchable.
 */
typedef struct dir_t
{
  //! Path to directory
  char *path;
  //! List of watched files/patterns
  vector_t files;
} dir_t;

/**************************************************************************
 * Function declarations.
 */
extern int dirs_init(vector_t *lst, const config_t *cfg, vector_t *formats, vector_t *tables);
extern void dir_free(void *obj);
extern int dir_file_match(dir_t *dir, const char *name);

#endif

