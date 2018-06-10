
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
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <stdbool.h>
#include <assert.h>
#include "config.h"
#include "table.h"
#include "wdata.h"
#include "utils.h"
#include "database.h"

#define MAX_NUM_PARAMS 100

#define DEFAULT_MAX_INSERTS 1000
#define DEFAULT_MAX_DURATION 10000
#define DEFAULT_IDLE_TIMEOUT 1000
#define DEFAULT_RETRY_INTERVAL 30000
#define DEFAULT_MAX_FAILSRECON 3

#define DB_PARAM_CONNECTION_URL "connection-url"
#define DB_PARAM_RETRY_INTERVAL "retry-interval"
#define DB_PARAM_TRANSACTION "transaction"
#define DB_PARAM_MAX_FAILSRECON "max-failed-reconnections"
#define TS_PARAM_MAX_INSERTS "max-inserts"
#define TS_PARAM_MAX_DURATION "max-duration"
#define TS_PARAM_IDLE_TIMEOUT "idle-timeout"

static const char *DB_PARAMS[] = {
    DB_PARAM_CONNECTION_URL,
    DB_PARAM_RETRY_INTERVAL,
    DB_PARAM_MAX_FAILSRECON,
    DB_PARAM_TRANSACTION,
    NULL
};

static const char *TS_PARAMS[] = {
    TS_PARAM_MAX_INSERTS,
    TS_PARAM_MAX_DURATION,
    TS_PARAM_IDLE_TIMEOUT,
    NULL
};

/**************************************************************************//**
 * @brief Close connection to database.
 * @param[in,out] database Database parameters.
 */
static void database_close(database_t *database)
{
  if (database->conn != NULL) {
    PQfinish(database->conn);
    database->conn = NULL;
    syslog(LOG_DEBUG, "database - connection to database closed");
  }

  database->status = DB_STATUS_ERROR;
}

/**************************************************************************//**
 * @brief Reset a database object.
 * @param[in,out] database Database object.
 */
void database_reset(database_t *database)
{
  if (database == NULL) {
    assert(0);
    return;
  }

  database_close(database);
  free(database->conn_str);
  database->conn_str = NULL;
  database->status = DB_STATUS_UNINITIALIZED;
  database->retryinterval = 0;
  database->ts_maxduration = 0;
  database->ts_maxinserts = 0;
  database->ts_numinserts = 0;
  database->ts_timeval = (struct timeval){0};
  database->ts_idletimeout = 0;
  database->tables = NULL;
  database->mqueue = NULL;
  vector_reset(&(database->pending), free);
}

/**************************************************************************//**
 * @brief Creates a prepared statement for the Sql of the given table.
 * @see https://www.postgresql.org/docs/9.3/static/libpq-exec.html
 * @param[in,out] database Database object.
 * @param[in] table Table object.
 * @return true=OK, false=KO.
 */
static bool database_prepare_stmt(database_t *database, table_t *table)
{
  assert(database != NULL);
  assert(table != NULL);

  bool done = true;
  PGresult *res = NULL;
  char *query = table_get_stmt(table);

  res = PQprepare(database->conn, table->name, query, 0, NULL);
  if (res == NULL || PQresultStatus(res) != PGRES_COMMAND_OK) {
    char *msg = PQerrorMessage(database->conn);
    replace_char(msg, '\n', '\0');
    syslog(LOG_ERR, "database - error preparing statement '%s'=[%s] - %s", table->name, query, msg);
    done = false;
  }
  else {
    syslog(LOG_DEBUG, "database - prepared statement created '%s'=[%s]", table->name, query);
  }

  free(query);
  PQclear(res);
  return(done);
}

/**************************************************************************//**
 * @brief Creates prepared statements.
 * @param[in,out] database Database object.
 * @return true=OK, false=KO.
 */
static bool database_create_stmts(database_t *database)
{
  assert(database != NULL);
  assert(database->status == DB_STATUS_CONNECTED);

  bool done = true;

  // create prepared statements
  for(size_t i=0; i<database->tables->size; i++) {
    table_t *table = (table_t*)(database->tables->data[i]);
    done &= database_prepare_stmt(database, table);
  }

  if (!done) {
    database_close(database);
  }

  return(done);
}

/**************************************************************************//**
 * @brief Connect to database.
 * @param[in,out] database Database parameters.
 * @param[in] tables List of tables.
 * @return true=OK, false=KO.
 */
static bool database_connect(database_t *database)
{
  if (database == NULL || database->conn_str == NULL || database->tables == NULL ||
      database->status == DB_STATUS_CONNECTED || database->status == DB_STATUS_TRANSACTION) {
    assert(false);
    return(false);
  }

  // closing current connection
  if (database->conn != NULL) {
    database_close(database);
  }

  // Connecting to database
  database->conn = PQconnectdb(database->conn_str);
  if (database->conn == NULL || PQstatus(database->conn) != CONNECTION_OK) {
    syslog(LOG_ERR, "database - %s", PQerrorMessage(database->conn));
    database_close(database);
    return(false);
  }

  database->status = DB_STATUS_CONNECTED;
  syslog(LOG_DEBUG, "database - connection to database succeeded");

  return(true);
}

/**************************************************************************//**
 * @brief Initialize the database.
 * @param[in,out] database Database object.
 * @param[in] cfg Configuration file.
 * @param[in] tables List of tables.
 * @param[in] mqueue Message queue (processor -> database).
 * @return 0=OK, otherwise an error ocurred.
 */
int database_init(database_t *database, const config_t *cfg, vector_t *tables, mqueue_t *mqueue)
{
  if (database == NULL || database->status != DB_STATUS_UNINITIALIZED ||
      cfg == NULL || tables == NULL || mqueue == NULL) {
    assert(false);
    return(1);
  }

  // setting default values
  database->conn = NULL;
  database->conn_str = NULL;
  database->retryinterval = DEFAULT_RETRY_INTERVAL;
  database->maxfailsrecon = DEFAULT_MAX_FAILSRECON;
  database->ts_maxinserts = DEFAULT_MAX_INSERTS;
  database->ts_maxduration = DEFAULT_MAX_DURATION;
  database->ts_idletimeout = DEFAULT_IDLE_TIMEOUT;
  database->ts_numinserts = 0;
  database->ts_timeval = (struct timeval){0};
  database->status = DB_STATUS_UNINITIALIZED;
  database->tables = NULL;
  database->mqueue = NULL;
  database->pending = (vector_t){0};

  // getting database entry in configuration file
  config_setting_t *parent = config_lookup(cfg, "database");
  if (parent == NULL) {
    config_setting_t *root = config_root_setting(cfg);
    const char *filename = config_setting_source_file(root);
    syslog(LOG_ERR, "database entry not found at %s.", filename);
    return(1);
  }

  // check attributes
  int rc = setting_check_childs(parent, DB_PARAMS);

  // getting database connection string
  const char *connstr = NULL;
  config_setting_lookup_string(parent, DB_PARAM_CONNECTION_URL, &connstr);
  if (connstr == NULL) {
    syslog(LOG_ERR, "database without " DB_PARAM_CONNECTION_URL " at %s:%d.",
           config_setting_source_file(parent),
           config_setting_source_line(parent));
    rc |= 1;
  }

  // getting transaction attributes from config
  rc |= setting_read_uint(parent, DB_PARAM_RETRY_INTERVAL, &(database->retryinterval));
  rc |= setting_read_uint(parent, DB_PARAM_MAX_FAILSRECON, &(database->maxfailsrecon));
  config_setting_t *children1 = config_setting_lookup(parent, DB_PARAM_TRANSACTION);
  if (children1 != NULL) {
    rc |= setting_check_childs(children1, TS_PARAMS);
    rc |= setting_read_uint(children1, TS_PARAM_MAX_INSERTS, &(database->ts_maxinserts));
    rc |= setting_read_uint(children1, TS_PARAM_MAX_DURATION, &(database->ts_maxduration));
    rc |= setting_read_uint(children1, TS_PARAM_IDLE_TIMEOUT, &(database->ts_idletimeout));

    if (database->ts_idletimeout > database->ts_maxduration) {
        config_setting_t *aux = config_setting_lookup(children1, TS_PARAM_IDLE_TIMEOUT);
        syslog(LOG_ERR, TS_PARAM_IDLE_TIMEOUT " greater than " TS_PARAM_MAX_DURATION " at %s:%d.",
               config_setting_source_file(aux),
               config_setting_source_line(aux));
        rc |= 1;
    }
  }
  if (rc != 0) {
    return(rc);
  }

  syslog(LOG_DEBUG, "database - params = [conn=%s, maxinserts=%zu, maxduration=%zu, idletimeout=%zu, "
         "retryinterval=%zu, maxfailsrecon=%zu]", connstr, database->ts_maxinserts, database->ts_maxduration,
         database->ts_idletimeout, database->retryinterval, database->maxfailsrecon);

  // initializing values
  database->mqueue = mqueue;
  vector_reserve(&(database->pending), database->ts_maxinserts);
  database->conn_str = strdup(connstr);
  database->tables = tables;

  // connecting to database
  if (database_connect(database) && database_create_stmts(database)) {
    return(0);
  }
  else {
    database_reset(database);
    return(1);
  }
}

/**************************************************************************//**
 * @brief Process database error.
 * @param[in,out] database Database parameters.
 */
static void database_process_error(database_t *database)
{
  syslog(LOG_WARNING, "database - %s", PQerrorMessage(database->conn));
  database->status = DB_STATUS_ERROR;
}

/**************************************************************************//**
 * @brief Starts a transaction.
 * @param[in,out] database Database parameters.
 * @return true=OK, false=KO.
 */
static bool database_begin(database_t *database)
{
  if (database->status != DB_STATUS_CONNECTED) {
    assert(false);
    return(false);
  }

  bool done = true;

  PGresult* res = PQexec(database->conn, "BEGIN");
  if (PQresultStatus(res) != PGRES_COMMAND_OK && PQresultStatus(res) != PGRES_NONFATAL_ERROR) {
    database_process_error(database);
    done = false;
  }
  else {
    database->ts_numinserts = 0;
    gettimeofday(&(database->ts_timeval), NULL);
    database->status = DB_STATUS_TRANSACTION;
    syslog(LOG_DEBUG, "database - begin");
  }

  PQclear(res);
  return(done);
}

/**************************************************************************//**
 * @brief Commits the current transaction.
 * @param[in,out] database Database parameters.
 * @return true=OK, false=KO.
 */
static bool database_commit(database_t *database)
{
  if (database->status != DB_STATUS_TRANSACTION) {
    assert(false);
    return(false);
  }

  bool done = true;

  PGresult* res = PQexec(database->conn, "COMMIT");
  if (PQresultStatus(res) != PGRES_COMMAND_OK && PQresultStatus(res) != PGRES_NONFATAL_ERROR) {
    database_process_error(database);
    done = false;
  }
  else {
    database->ts_numinserts = 0;
    database->ts_timeval = (struct timeval){0};
    database->status = DB_STATUS_CONNECTED;
    vector_clear(&(database->pending), free);
    syslog(LOG_DEBUG, "database - commit");
  }

  PQclear(res);
  return(done);
}

/**************************************************************************//**
 * @brief Inserts data to database.
 * @param[in,out] database Database parameters.
 * @param[in] item Witem's data owner.
 * @param[in] ptr Table param values (sorted, separated by '\0').
 * @return true=inserted, false=otherwise.
 */
static bool database_exec(database_t *database, wdata_t *data)
{
  assert(database != NULL);
  assert(data != NULL);

  witem_t *item = data->item;
  assert(item != NULL);
  const char *ptr = &(data->x);
  table_t *table = ((file_t *) item->ptr)->table;
  assert(table != NULL);
  int numParams = table->parameters.size;
  const char *paramValues[MAX_NUM_PARAMS];

  vector_insert(&(database->pending), data);

  // ensures that a transaction exists
  if (database->status == DB_STATUS_CONNECTED) {
    database_begin(database);
  }
  if (database->status == DB_STATUS_ERROR) {
    return(false);
  }

  syslog(LOG_DEBUG, "database - exec [table=%s, file=%s, values=%p]",
         table->name, item->filename, (void *)(ptr));

  for(int i=0; i<numParams; i++) {
    size_t len = strlen(ptr);
    paramValues[i] = ptr;
    ptr += len + 1;
  }

  bool done = true;
  PGresult* res = PQexecPrepared(database->conn, table->name, numParams, paramValues, NULL, NULL, 0);
  if (PQresultStatus(res) == PGRES_NONFATAL_ERROR) {
    syslog(LOG_WARNING, "database - %s", PQerrorMessage(database->conn));
  }
  if (PQresultStatus(res) == PGRES_NONFATAL_ERROR || PQresultStatus(res) == PGRES_COMMAND_OK) {
    database->ts_numinserts++;
  }
  else {
    database_process_error(database);
    done = false;
  }

  PQclear(res);
  return(done);
}

/**************************************************************************//**
 * @brief Re-execute the inserts in pending list.
 * @param[in,out] database Database parameters.
 * @return true if pending inserts are commited, false otherwise.
 */
static bool database_process_pending(database_t *database)
{
  bool done = true;
  vector_t aux = {0};

  vector_reserve(&aux, database->ts_maxinserts);
  vector_swap(&aux, &(database->pending));

  for(size_t i=0; i<aux.size; i++) {
    wdata_t *data = (wdata_t *) aux.data[i];
    done = database_exec(database, data);
    if (!done) {
      break;
    }
  }

  if (done) {
    done = database_commit(database);
  }

  if (done) {
    // we set NULL as second argument because free is done by database_commit()
    vector_reset(&aux, NULL);
  }
  else {
    // restore the list of pending
    vector_swap(&aux, &(database->pending));
    // we set NULL as second argument because data is still referenced by pending
    vector_reset(&aux, NULL);
  }

  return(done);
}

/**************************************************************************//**
 * @brief Try to reconnect to database.
 * @param[in,out] database Database parameters.
 */
static void database_reconnect(database_t *database)
{
  assert(database != NULL);
  assert(database->status == DB_STATUS_ERROR);

  size_t num_fails = 0;

  while(true)
  {
    if (database_connect(database)) {
      if (!database_create_stmts(database)) {
        num_fails++;
      }
      else if (!database_process_pending(database)) {
        num_fails++;
      }
      else {
        break;
      }
    }

    if (num_fails >= database->maxfailsrecon) {
      syslog(LOG_ERR, "database - %zu failed reconnections", database->maxfailsrecon);
      terminate(EXIT_FAILURE);
      return;
    }

    struct timespec ts = {0};
    ts.tv_sec = database->retryinterval / 1000;
    ts.tv_nsec = (database->retryinterval % 1000) * 1000000;

    nanosleep(&ts, NULL);
  }
}

/**************************************************************************//**
 * @brief Process processor events readed from queue.
 * @details This function block the current thread until NULL event is received.
 * @details There is not transaction if not required.
 * @param[in,out] ptr Database parameters.
 */
void* database_run(void *ptr)
{
  database_t *database = (database_t *) ptr;
  if (database == NULL || database->mqueue == NULL ||  database->status == DB_STATUS_UNINITIALIZED || database->tables == NULL) {
    assert(false);
    return(NULL);
  }

  syslog(LOG_DEBUG, "database - thread started");

  while(true)
  {
    size_t millisToWait = 0;

    if (database->status == DB_STATUS_TRANSACTION)
    {
      size_t millis = elapsed_millis(&(database->ts_timeval));

      if (database->ts_numinserts >= database->ts_maxinserts) {
        database_commit(database);
      }
      else if (millis >= database->ts_maxduration) {
        database_commit(database);
      }
      else {
        size_t millis_to_maxduration = database->ts_maxduration - millis;
        millisToWait = MIN(millis_to_maxduration, database->ts_idletimeout);
      }
    }
    else if (database->status == DB_STATUS_ERROR) {
      database_reconnect(database);
    }

    // waiting for a new message
    msg_t msg = mqueue_pop(database->mqueue, millisToWait);

    // processing message
    if (msg.type == MSG_TYPE_ERROR) {
      terminate(EXIT_FAILURE);
      break;
    }
    else if (msg.type == MSG_TYPE_CLOSE) {
      break;
    }
    else if (msg.type == MSG_TYPE_EINTR || msg.type == MSG_TYPE_NULL) {
      continue;
    }
    else if (msg.type == MSG_TYPE_TIMEOUT) {
      database_commit(database);
      continue;
    }
    else {
      assert(msg.type == MSG_TYPE_MATCH1);
      wdata_t *data = (wdata_t *) msg.data;
      assert(data != NULL);
      database_exec(database, data);
    }
  }

  // we commit if there is a transaction in progress
  if (database->status == DB_STATUS_TRANSACTION) {
    database_commit(database);
  }

  syslog(LOG_DEBUG, "database - thread ended");
  return(NULL);
}
