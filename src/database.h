
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

#include <sys/time.h>
#include <libconfig.h>
#include <libpq-fe.h>
#include "vector.h"
#include "mqueue.h"

/**************************************************************************//**
 * @brief Types of database connection status.
 */
typedef enum {
  DB_STATUS_UNINITIALIZED = 0, // Database not initialized.
  DB_STATUS_CONNECTED,         // Database connected + no transaction in progress.
  DB_STATUS_TRANSACTION,       // Database connected + transaction in progress.
  DB_STATUS_ERRCON             // Database connection error (can be connected or not).
} db_status_e;

/**************************************************************************//**
 * @brief Database thread.
 */
typedef struct database_t
{
  //! Messages received from processor.
  mqueue_t *mqueue;
  //! Database status.
  db_status_e status;
  //! Database connection
  PGconn *conn;
  //! Database connection string.
  char *conn_str;
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
  //! List of tables.
  vector_t *tables;
} database_t;

/**************************************************************************
 * Function declarations.
 */
extern int database_init(database_t *db, const config_t *cfg, vector_t *tables);
extern void database_reset(database_t *db);
extern void* database_run(void *ptr);

#endif

