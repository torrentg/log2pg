
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

#ifndef WDATA_H
#define WDATA_H

#include "witem.h"

/**************************************************************************//**
 * @brief Values parsed by processor to be persisted in DB.
 * @details Variable length depending on param values. We do it this way to
 *          minimize the number of malloc/free calls.
 * @details To retrieve the pointer to param values do "char *ptr = &x;"
 */
typedef struct wdata_t
{
  //! Watched item.
  witem_t *item;
  //! Table param values (sorted, separated by '\0').
  char x;
} wdata_t;

/**************************************************************************
 * Function declarations.
 */
extern wdata_t* wdata_alloc(witem_t *item, const char *str);
extern void wdata_free(void *obj);

#endif
