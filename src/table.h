
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

#ifndef TABLE_H
#define TABLE_H

#include <libconfig.h>
#include "vector.h"

/**************************************************************************//**
 * @brief Table defined in configuration file.
 * @details First member is 'char *' to be searchable.
 */
typedef struct table_t
{
  //! Table name.
  char *name;
  //! SQL command.
  char *sql;
  //! Table parameters (strings).
  vector_t parameters;
} table_t;

/**************************************************************************
 * Function declarations.
 */
extern int tables_init(vector_t *lst, const config_t *cfg);
extern void table_free(void *obj);
char* table_get_stmt(const table_t *table);

#endif

