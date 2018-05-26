
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <syslog.h>
#include <assert.h>
#include "config.h"

/**************************************************************************//**
 * @brief Returns a string setting value.
 * @details If requested property is not found then returns default value.
 *          If value can't be cast to size_t, then NULL is returned.
 * @param[in] parent Parent setting.
 * @param[in] name Setting name.
 * @param[out] value Returned value.
 * @return 0=OK (valid value or setting not found), 1=KO (invalid type or range).
 */
int setting_read_uint(const config_setting_t *parent, const char *name, size_t *value)
{
  assert(parent != NULL);
  assert(name != NULL);
  assert(value != NULL);

  config_setting_t *setting = config_setting_get_member(parent, name);
  if (setting == NULL) {
    return(0);
  }

  if (config_setting_type(setting) != CONFIG_TYPE_INT) {
    syslog(LOG_ERR, "%s is an invalid integer at %s:%d.", name,
           config_setting_source_file(setting),
           config_setting_source_line(setting));
    return(1);
  }

  int num = config_setting_get_int(setting);
  if (num < 0) {
    syslog(LOG_ERR, "%s is a negative value at %s:%d.", name,
           config_setting_source_file(setting),
           config_setting_source_line(setting));
    return(1);
  }

  *value = num;
  return(0);
}

/**************************************************************************//**
 * @brief Return the setting with the given name checking that it is a
 *        non-empty list.
 * @param[in] cfg Configuration object.
 * @param[in] name Setting name.
 * @return The setting, NULL if error.
 */
config_setting_t* setting_get_list(const config_t *cfg, const char *name)
{
  assert(cfg != NULL);
  assert(name != NULL);

  // getting setting from configuration file
  config_setting_t *ret = config_lookup(cfg, name);
  if (ret == NULL) {
    config_setting_t *root = config_root_setting(cfg);
    syslog(LOG_ERR, "%s entry not found at %s.", name,
           config_setting_source_file(root));
    return(NULL);
  }

  // checking that it is a list
  if (config_setting_is_list(ret) != CONFIG_TRUE) {
    syslog(LOG_ERR, "%s is not a list at %s:%d.", name,
           config_setting_source_file(ret),
           config_setting_source_line(ret));
    return(NULL);
  }

  // checking that list is not empty
  int len = config_setting_length(ret);
  if (len <= 0) {
    syslog(LOG_ERR, "%s is empty at %s:%d.", name,
           config_setting_source_file(ret),
           config_setting_source_line(ret));
    return(NULL);
  }

  return(ret);
}

/**************************************************************************//**
 * @brief Read configuration file.
 * @param[in,out] cfg Configuration object to be initialized.
 * @param[in] filename Configuration filename.
 * @return 0=OK, otherwise an error ocurred.
 */
int init_config(config_t *cfg, const char *filename)
{
  assert(filename != NULL);
  assert(cfg->root == NULL);

  if (filename == NULL) {
    return EXIT_FAILURE;
  }

  config_init(cfg);

  if (access(filename, R_OK) != 0) {
     perror(filename);
     return(EXIT_FAILURE);
  }

  int rc = config_read_file(cfg, filename);
  if (rc == CONFIG_FALSE) {
     fprintf(stderr, "Error: %s at %s:%d.\n", config_error_text(cfg), config_error_file(cfg), config_error_line(cfg));
     return(EXIT_FAILURE);
  }

  return(EXIT_SUCCESS);
}

/**************************************************************************//**
 * @brief Checks if a string appears in a list of strings.
 * @param[in] str String to search.
 * @param[in] values List of values (ended with NULL).
 * @return 1=found, 0=not found.
 */
static int is_in_list(const char *str, const char **values)
{
  if (str == NULL || values == NULL) {
    return(0);
  }

  while(*values != NULL) {
    const char *value = *values;
    if (strcmp(str, value) == 0) {
       return(1);
    }
    values++;
  }

  return(0);
}

/**************************************************************************//**
 * @brief Checks that childs settings are in values.
 * @param[in] setting Configuration setting.
 * @param[in] childnames Allowed childnames (ended with NULL).
 * @return 0=OK, otherwise an error ocurred.
 */
int setting_check_childs(const config_setting_t *setting, const char **childnames)
{
  if (setting == NULL || childnames == NULL) {
    assert(0);
    return(1);
  }

  int rc = 0;
  int len = config_setting_length(setting);

  for(int i=0; i<len; i++) {
    config_setting_t *child = config_setting_get_elem(setting, i);
    const char *name = config_setting_name(child);
    if (!is_in_list(name, childnames)) {
      syslog(LOG_ERR, "unkown entry '%s' at %s:%d.", name,
             config_setting_source_file(child),
             config_setting_source_line(child));
      rc = 1;
    }
  }

  return(rc);
}
