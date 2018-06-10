
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <ctype.h>
#include <assert.h>
#include "config.h"
#include "table.h"
#include "stringbuf.h"

#define TABLE_PARAM_NAME "name"
#define TABLE_PARAM_SQL "sql"

#define MAX_NUM_PARAMS 99
#define PARAMETER_PREFIX '$'
#define PARAMETER_MAX_SIZE 32

static const char *TABLE_PARAMS[] = {
    TABLE_PARAM_NAME,
    TABLE_PARAM_SQL,
    NULL
};

/**************************************************************************//**
 * @brief Returns the list of parameters (identifiers prefixed by ':').
 * @details Parameters sorted by order of appearance.
 * @details Parameter identifier consist of up to 32 alphanumeric characters
 *          and underscores, but must start with a non-digit.
 * @example str="INSERT INTO table VALUES(:id, :name)"
 * @param[in] sql Sql string.
 * @param[in,out] lst List of parameters (initially empty).
 * @return 0=OK, otherwise=KO.
 */
static int sql_get_parameters(const char *sql, vector_t *lst)
{
  if (sql == NULL || lst == NULL || lst->size > 0) {
    assert(0);
    return(1);
  }

  const char *ptr1 = sql;
  while((ptr1 = strchr(ptr1, PARAMETER_PREFIX)) != NULL)
  {
    const char *ptr2 = ptr1+1;
    // start with a non-digit
    if (!isalpha(*ptr2)) {
      ptr1 = ptr2;
      continue;
    }
    // alphanumeric characters and underscores
    while(isalnum(*ptr2) || *ptr2 == '_') ptr2++;
    // length up to 32 characters
    if (ptr2-ptr1-1 <= PARAMETER_MAX_SIZE) {
      char *var = strndup(ptr1+1, (ptr2-ptr1-1));
      if (var == NULL) {
        return(1);
      }
      //TODO: avoid duplicates
      vector_insert(lst, var);
    }
    ptr1 = ptr2;
  }
  return(0);
}

/**************************************************************************//**
 * @brief Allocate and initialize a table object.
 * @param[in] name Table name.
 * @param[in] sql Sql command.
 * @return Initialized object or NULL if error.
 */
static table_t* table_alloc(const char *name, const char *sql)
{
  assert(name != NULL);
  assert(sql != NULL);

  table_t *ret = (table_t *) calloc(1, sizeof(table_t));
  if (ret == NULL) {
    return(NULL);
  }

  ret->name = strdup(name);
  ret->sql = strdup(sql);
  vector_reset(&(ret->parameters), NULL);
  sql_get_parameters(sql, &(ret->parameters));

  char *str = vector_print(&(ret->parameters));
  syslog(LOG_DEBUG, "created table [address=%p, name=%s, sql=%s, parameters=%s]",
         (void *)ret, name, sql, str);
  free(str);

  return(ret);
}

/**************************************************************************//**
 * @brief Frees memory space pointed by ptr.
 * @param[in] ptr Pointer to table object.
 */
void table_free(void *ptr)
{
  if (ptr == NULL) return;
  table_t *obj = (table_t *) ptr;

  syslog(LOG_DEBUG, "removed table [address=%p, name=%s]", ptr, obj->name);

  free(obj->name);
  free(obj->sql);
  vector_reset(&(obj->parameters), free);
  free(obj);
}

/**************************************************************************//**
 * @brief Replace parameters (':') by numeric identifiers ('$').
 * @example 'values(:timestamp, :msg)' -> 'values($1, $2)'
 * @param[in] table Table object.
 * @return String with numeric identifiers (to be freed by caller), NULL if error.
 */
char* table_get_stmt(const table_t *table)
{
  if (table == NULL || table->sql == NULL) {
    assert(false);
    return(NULL);
  }

  stringbuf_t ret = {0};
  char param_name[PARAMETER_MAX_SIZE+2] = {0};
  char param_id[6] = {0};

  stringbuf_append(&ret, table->sql);
  param_name[0] = PARAMETER_PREFIX;

  // performs the replacement ($variable -> $1)
  for(size_t i=0; i<table->parameters.size; i++) {
    sprintf(param_id, "$%zu", i+1);
    strncpy(param_name+1, (char*)(table->parameters.data[i]), PARAMETER_MAX_SIZE);
    stringbuf_replace(&ret, param_name, param_id);
  }

  return(ret.data);
}

/**************************************************************************//**
 * @brief Parse a table entry and adds to table list.
 * @detail setting format: { name="xxx"; sql="yyy"; }
 * @param[in,out] lst List of tables.
 * @param[in] setting Configuration setting.
 * @return 0=OK, otherwise=KO.
 */
static int tables_parse_item(vector_t *lst, const config_setting_t *setting)
{
  assert(lst != NULL);
  assert(setting != NULL);

  int rc = 0;
  const char *name = NULL;
  const char *sql = NULL;

  // check attributes
  rc = setting_check_childs(setting, TABLE_PARAMS);

  // retrieving attributes
  config_setting_lookup_string(setting, TABLE_PARAM_NAME, &name);
  config_setting_lookup_string(setting, TABLE_PARAM_SQL, &sql);

  // check if attributes are set
  if (name == NULL) {
    syslog(LOG_ERR, "table without " TABLE_PARAM_NAME " at %s:%d.",
           config_setting_source_file(setting),
           config_setting_source_line(setting));
    rc = 1;
  }
  if (sql == NULL) {
    config_setting_t *aux = config_setting_get_member(setting, TABLE_PARAM_SQL);
    syslog(LOG_ERR, "table without " TABLE_PARAM_SQL " at %s:%d.",
           config_setting_source_file(aux),
           config_setting_source_line(aux));
    rc = 1;
  }

  // check if name exists
  int index = vector_find(lst, name);
  if (index >= 0) {
    config_setting_t *aux = config_setting_get_member(setting, TABLE_PARAM_NAME);
    syslog(LOG_ERR, "duplicated table " TABLE_PARAM_NAME " '%s' at %s:%d.", name,
           config_setting_source_file(aux),
           config_setting_source_line(aux));
    rc = 1;
  }

  // exit if errors
  if (rc != 0) {
    return(rc);
  }

  // create table
  table_t *item = table_alloc(name, sql);
  if (item == NULL) {
    return(1);
  }

  // check the number of parameters
  if (item->parameters.size > MAX_NUM_PARAMS) {
    config_setting_t *aux = config_setting_get_member(setting, TABLE_PARAM_SQL);
    syslog(LOG_ERR, TABLE_PARAM_SQL " with more than %d parameters at %s:%d.",
           MAX_NUM_PARAMS,
           config_setting_source_file(aux),
           config_setting_source_line(aux));
    table_free(item);
    return(1);
  }

  // append table to list
  rc = vector_insert(lst, item);
  if (rc != 0) {
    table_free(item);
    return(1);
  }

  return(rc);
}

/**************************************************************************//**
 * @brief Initialize the list of tables.
 * @param[in,out] lst List of tables.
 * @param[in] cfg Configuration file.
 * @return 0=OK, otherwise an error ocurred.
 */
int tables_init(vector_t *lst, const config_t *cfg)
{
  assert(cfg != NULL);
  assert(lst != NULL);
  if (cfg == NULL || lst == NULL || lst->size > 0) {
    return(1);
  }

  // getting tables entry in configuration file
  config_setting_t *parent = setting_get_list(cfg, "tables");
  if (parent == NULL) {
    return(1);
  }

  int rc = 0;
  int len = config_setting_length(parent);

  // parsing each list entry
  for(int i=0; i<len; i++) {
    config_setting_t *setting = config_setting_get_elem(parent, i);
    rc |= tables_parse_item(lst, setting);
  }

  return(rc);
}
