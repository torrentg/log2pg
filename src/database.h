
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

#ifndef DATABASE_H
#define DATABASE_H

#include "vector.h"
#include <sys/time.h>
#include <libconfig.h>
#include <libpq-fe.h>

/**************************************************************************//**
 * @brief Database connection.
 */
typedef struct database_t
{
  //! Database connection
  PGconn *conn;
  //! Connection lost retry interval (in millis)
  size_t retryinterval;
  //! Maximum number of inserts per transaction
  size_t ts_maxinserts;
  //! Maximum transaction duration (in millis)
  size_t ts_maxduration;
  //! Maximum transaction idle time (in millis)
  size_t ts_idletimeout;
  //! Transaction starting time (0 means no transaction in progress).
  struct timeval ts_timeval;
  //! Number of inserts pending to commit.
  size_t ts_numinserts;
} database_t;

/**************************************************************************
 * Function declarations.
 */
extern int database_init(database_t *db, const config_t *cfg, vector_t *tables);
extern void database_reset(database_t *db);
extern void* database_run(void *ptr);

#endif

