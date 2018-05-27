
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
#include <time.h>
#include <assert.h>
#include "config.h"
#include "table.h"
#include "params.h"
#include "wdata.h"
#include "utils.h"
#include "database.h"

#define MIN(a,b) (((a)<(b))?(a):(b))

#define MAX_NUM_PARAMS 100

#define DEFAULT_MAX_INSERTS 1000
#define DEFAULT_MAX_DURATION 10000
#define DEFAULT_IDLE_TIMEOUT 1000
#define DEFAULT_RETRY_INTERVAL 30000

#define DB_PARAM_CONNECTION_URL "connection-url"
#define DB_PARAM_RETRY_INTERVAL "retry-interval"
#define DB_PARAM_TRANSACTION "transaction"
#define TS_PARAM_MAX_INSERTS "max-inserts"
#define TS_PARAM_MAX_DURATION "max-duration"
#define TS_PARAM_IDLE_TIMEOUT "idle-timeout"

static const char *DB_PARAMS[] = {
    DB_PARAM_CONNECTION_URL,
    DB_PARAM_RETRY_INTERVAL,
    DB_PARAM_TRANSACTION,
    NULL
};

static const char *TS_PARAMS[] = {
    TS_PARAM_MAX_INSERTS,
    TS_PARAM_MAX_DURATION,
    TS_PARAM_IDLE_TIMEOUT,
    NULL
};

// external declarations
extern void terminate(void);

/**************************************************************************//**
 * @brief Close connection to database.
 * @param[in,out] db Database parameters.
 */
static void database_close(database_t *db)
{
  if (db->conn != NULL) {
    PQfinish(db->conn);
    db->conn = NULL;
    syslog(LOG_DEBUG, "database - connection to database closed");
  }

  db->status = DB_STATUS_ERRCON;
}

/**************************************************************************//**
 * @brief Reset a database struct.
 * @param[in,out] db Database struct.
 */
void database_reset(database_t *db)
{
  if (db == NULL) {
    assert(0);
    return;
  }

  database_close(db);
  free(db->conn_str);
  db->conn_str = NULL;
  db->status = DB_STATUS_UNINITIALIZED;
  db->retryinterval = 0;
  db->ts_maxduration = 0;
  db->ts_maxinserts = 0;
  db->ts_numinserts = 0;
  db->ts_timeval = (struct timeval){0};
  db->ts_idletimeout = 0;
}

/**************************************************************************//**
 * @brief Creates a prepared statement for the Sql of the given table.
 * @see https://www.postgresql.org/docs/9.3/static/libpq-exec.html
 * @param[in,out] db Database object.
 * @param[in] table Table object.
 * @return 0=OK, otherwise an error ocurred.
 */
static int database_prepare_stmt(database_t *db, table_t *table)
{
  assert(db != NULL);
  assert(table != NULL);

  int rc = 0;
  PGresult *res = NULL;
  char *query = table_get_stmt(table);

  res = PQprepare(db->conn, table->name, query, 0, NULL);
  if (res == NULL || PQresultStatus(res) != PGRES_COMMAND_OK) {
    char *msg = PQerrorMessage(db->conn);
    replace_char(msg, '\n', '\0');
    syslog(LOG_ERR, "database - error preparing statement '%s' - %s", table->name, msg);
    rc = 1;
  }
  else {
    syslog(LOG_DEBUG, "database - prepared statement '%s' created", table->name);
  }

  free(query);
  PQclear(res);
  return(rc);
}

/**************************************************************************//**
 * @brief Connect to database.
 * @param[in,out] db Database parameters.
 * @param[in] tables List of tables.
 * @return 0=OK, otherwise an error ocurred.
 */
static int database_connect(database_t *db, vector_t *tables)
{
  if (db == NULL || db->conn_str == NULL || tables == NULL ||
      db->status == DB_STATUS_CONNECTED || db->status == DB_STATUS_TRANSACTION) {
    assert(false);
    return(1);
  }

  // closing current connection
  if (db->conn != NULL) {
    database_close(db);
  }

  // Connecting to database
  db->conn = PQconnectdb(db->conn_str);
  if (db->conn == NULL || PQstatus(db->conn) != CONNECTION_OK) {
    syslog(LOG_ERR, "database - %s", PQerrorMessage(db->conn));
    database_close(db);
    return(1);
  }

  syslog(LOG_DEBUG, "database - connection to database succeeded");

  int rc = 0;

  // create prepared statements
  for(size_t i=0; i<tables->size; i++) {
    table_t *table = (table_t*)(tables->data[i]);
    rc |= database_prepare_stmt(db, table);
  }

  if (rc != 0) {
    database_close(db);
  }
  else {
    db->status = DB_STATUS_CONNECTED;
  }

  return(rc);
}

/**************************************************************************//**
 * @brief Initialize the database.
 * @param[in,out] db Database object.
 * @param[in] cfg Configuration file.
 * @param[in] tables List of tables.
 * @return 0=OK, otherwise an error ocurred.
 */
int database_init(database_t *db, const config_t *cfg, vector_t *tables)
{
  if (db == NULL || db->status != DB_STATUS_UNINITIALIZED || cfg == NULL || tables == NULL) {
    assert(false);
    return(1);
  }

  int rc = 0;

  // getting database entry in configuration file
  config_setting_t *parent = config_lookup(cfg, "database");
  if (parent == NULL) {
    config_setting_t *root = config_root_setting(cfg);
    const char *filename = config_setting_source_file(root);
    syslog(LOG_ERR, "database entry not found at %s.", filename);
    return(1);
  }

  // check attributes
  rc = setting_check_childs(parent, DB_PARAMS);

  // getting database connection string
  const char *connstr = NULL;
  config_setting_lookup_string(parent, DB_PARAM_CONNECTION_URL, &connstr);
  if (connstr == NULL) {
    syslog(LOG_ERR, "database without connection-url at %s:%d.",
           config_setting_source_file(parent),
           config_setting_source_line(parent));
    rc = 1;
  }

  // setting default values
  db->conn = NULL;
  db->conn_str = NULL;
  db->retryinterval = DEFAULT_RETRY_INTERVAL;
  db->ts_maxinserts = DEFAULT_MAX_INSERTS;
  db->ts_maxduration = DEFAULT_MAX_DURATION;
  db->ts_idletimeout = DEFAULT_IDLE_TIMEOUT;
  db->ts_numinserts = 0;
  db->ts_timeval = (struct timeval){0};
  db->status = DB_STATUS_UNINITIALIZED;

  // getting transaction attributes from config
  rc |= setting_read_uint(parent, DB_PARAM_RETRY_INTERVAL, &(db->retryinterval));
  config_setting_t *children1 = config_setting_lookup(parent, DB_PARAM_TRANSACTION);
  if (children1 != NULL) {
    rc |= setting_check_childs(children1, TS_PARAMS);
    rc |= setting_read_uint(children1, TS_PARAM_MAX_INSERTS, &(db->ts_maxinserts));
    rc |= setting_read_uint(children1, TS_PARAM_MAX_DURATION, &(db->ts_maxduration));
    rc |= setting_read_uint(children1, TS_PARAM_IDLE_TIMEOUT, &(db->ts_idletimeout));

    if (db->ts_idletimeout > db->ts_maxduration) {
        config_setting_t *aux = config_setting_lookup(children1, TS_PARAM_IDLE_TIMEOUT);
        syslog(LOG_ERR, TS_PARAM_IDLE_TIMEOUT " greater than " TS_PARAM_MAX_DURATION " at %s:%d.",
               config_setting_source_file(aux),
               config_setting_source_line(aux));
        rc = 1;
    }
  }
  if (rc != 0) {
    return(rc);
  }

  syslog(LOG_DEBUG, "database - params = [conn=%s, maxinserts=%zu, maxduration=%zu, idletimeout=%zu, retryinterval=%zu]",
         connstr, db->ts_maxinserts, db->ts_maxduration, db->ts_idletimeout, db->retryinterval);

  // connecting to database
  db->conn_str = strdup(connstr);
  database_connect(db, tables);
  if (db->status != DB_STATUS_CONNECTED) {
    database_reset(db);
    rc = 1;
  }

  return(rc);
}

/**************************************************************************//**
 * @brief Process database error.
 * @param[in,out] db Database parameters.
 */
static void database_process_error(database_t *db)
{
  syslog(LOG_WARNING, "database - %s", PQerrorMessage(db->conn));
  db->status = DB_STATUS_ERRCON;
}

/**************************************************************************//**
 * @brief Starts a transaction.
 * @param[in,out] db Database parameters.
 */
static void database_begin(database_t *db)
{
  if (db->status != DB_STATUS_CONNECTED) {
    assert(false);
    return;
  }

  PGresult* res = PQexec(db->conn, "BEGIN");
  if (PQresultStatus(res) != PGRES_COMMAND_OK && PQresultStatus(res) != PGRES_NONFATAL_ERROR) {
    database_process_error(db);
  }
  else {
    db->ts_numinserts = 0;
    gettimeofday(&(db->ts_timeval), NULL);
    db->status = DB_STATUS_TRANSACTION;
    syslog(LOG_DEBUG, "database - begin");
  }

  PQclear(res);
}

/**************************************************************************//**
 * @brief Commits the current transaction.
 * @param[in,out] db Database parameters.
 */
static void database_commit(database_t *db)
{
  if (db->status != DB_STATUS_TRANSACTION) {
    assert(false);
    return;
  }

  PGresult* res = PQexec(db->conn, "COMMIT");
  if (PQresultStatus(res) != PGRES_COMMAND_OK && PQresultStatus(res) != PGRES_NONFATAL_ERROR) {
    database_process_error(db);
  }
  else {
    db->ts_numinserts = 0;
    db->ts_timeval = (struct timeval){0};
    db->status = DB_STATUS_CONNECTED;
    syslog(LOG_DEBUG, "database - commit");
  }

  PQclear(res);
}

/**************************************************************************//**
 * @brief Inserts data to database.
 * @param[in,out] db Database parameters.
 * @param[in] item Witem's data owner.
 * @param[in] ptr Table param values (sorted, separated by '\0').
 */
static void database_exec(database_t *db, witem_t *item, const char *ptr)
{
  assert(db != NULL);
  assert(item != NULL);
  assert(ptr != NULL);

  table_t *table = ((file_t *) item->ptr)->table;
  assert(table != NULL);
  int numParams = table->parameters.size;
  const char *paramValues[MAX_NUM_PARAMS];

  // ensures that a transaction exists
  if (db->status == DB_STATUS_CONNECTED) {
    database_begin(db);
  }

  syslog(LOG_DEBUG, "database - exec [table=%s, file=%s, values=%p]",
         table->name, item->filename, (void *)(ptr));

  for(int i=0; i<numParams; i++) {
    size_t len = strlen(ptr);
    paramValues[i] = ptr;
    ptr += len + 1;
  }

  PGresult* res = PQexecPrepared(db->conn, table->name, numParams, paramValues, NULL, NULL, 0);
  if (PQresultStatus(res) == PGRES_NONFATAL_ERROR) {
    syslog(LOG_WARNING, "database - warning : %s", PQerrorMessage(db->conn));
  }
  if (PQresultStatus(res) == PGRES_NONFATAL_ERROR || PQresultStatus(res) == PGRES_COMMAND_OK) {
    db->ts_numinserts++;
  }
  else {
    database_process_error(db);
  }

  PQclear(res);
}

/**************************************************************************//**
 * @brief Try to reconnect to database.
 * @param[in,out] db Database parameters.
 */
static void database_reconnect(database_t *db)
{
  assert(db != NULL);
  assert(db->status == DB_STATUS_ERRCON);

  while(true)
  {
    //TODO: retrieve tables and uncomment line
    //database_connect(db, tables);
    if (db->status == DB_STATUS_CONNECTED) {
      break;
    }

    struct timespec ts = {0};
    ts.tv_sec = db->retryinterval / 1000;
    ts.tv_nsec = (db->retryinterval % 1000) * 1000000;

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
  params_t *params = (params_t *) ptr;
  if (params == NULL || params->queue2 == NULL || params->db == NULL || params->db->status == DB_STATUS_UNINITIALIZED) {
    assert(false);
    return(NULL);
  }

  syslog(LOG_DEBUG, "database - thread started");

  database_t *db = params->db;

  while(true)
  {
    size_t millisToWait = 0;

    if (db->status == DB_STATUS_TRANSACTION)
    {
      size_t millis = elapsed_millis(&(db->ts_timeval));

      if (db->ts_numinserts >= db->ts_maxinserts) {
        database_commit(db);
      }
      else if (millis >= db->ts_maxduration) {
        database_commit(db);
      }
      else {
        size_t millis_to_maxduration = db->ts_maxduration - millis;
        millisToWait = MIN(millis_to_maxduration, db->ts_idletimeout);
      }
    }
    else if (db->status == DB_STATUS_ERRCON) {
      database_reconnect(db);
    }

    // waiting for a new message
    msg_t msg = mqueue_pop(params->queue2, millisToWait);

    if (msg.type == MSG_TYPE_ERROR || msg.type == MSG_TYPE_CLOSE) {
      terminate();
      break;
    }
    else if (msg.type == MSG_TYPE_EINTR || msg.type == MSG_TYPE_NULL) {
      continue;
    }
    else if (msg.type == MSG_TYPE_TIMEOUT) {
      database_commit(db);
      continue;
    }
    else {
      assert(msg.type == MSG_TYPE_MATCH1);
      wdata_t *data = (wdata_t *) msg.data;
      assert(data != NULL);
      database_exec(db, data->item, &(data->x));
      free(data);
    }
  }

  // we commit if there is a transaction in progress
  if (db->status == DB_STATUS_TRANSACTION) {
    database_commit(db);
  }

  syslog(LOG_DEBUG, "database - thread ended");
  return(NULL);
}
