
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

#ifndef MONITOR_H
#define MONITOR_H

#include "map.h"
#include "vector.h"
#include "mqueue.h"

/**************************************************************************//**
 * @brief Convenience struct used to pass parameters to functions.
 */
typedef struct monitor_t
{
  //! Inotify file descriptor.
  int ifd;
  //! Master table of watched items.
  vector_t *witems;
  //! Access to witems by watch descriptor.
  map_t *dict;
  //! Events queue between monitor and processor.
  mqueue_t *mqueue;
} monitor_t;

/**************************************************************************
 * Function declarations.
 */
extern int monitor_init(const vector_t *dirs, monitor_t *params);
extern void* monitor_run(void *ptr);

#endif

