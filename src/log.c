
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
#include <string.h>
#include <stdlib.h>
#define SYSLOG_NAMES
#include <syslog.h>
#include <assert.h>
#include "config.h"
#include "log.h"

#define DEFAULT_SYSLOG_FACILITY "local7"
#define DEFAULT_SYSLOG_LEVEL "info"
#define DEFAULT_SYSLOG_TAG "log2pg"

#define LOG_PARAM_FACILITY "facility"
#define LOG_PARAM_LEVEL "level"
#define LOG_PARAM_TAG "tag"

/**************************************************************************//**
 * @brief Search a code (see syslog.h header) in a list.
 * @param code Pointer to the first element.
 * @param name Code name searched.
 * @return Code value, -1 if not found.
 */
static int get_code_value(CODE *code, const char *name)
{
  assert(code != NULL);
  assert(name != NULL);
  while(1) {
    if (code->c_name == NULL) {
      return(-1);
    }
    else if (strcmp(name, code->c_name) == 0) {
      return(code->c_val);
    }
    else {
      code++;
    }
  }
}

/**************************************************************************//**
 * @brief Initializes syslog.
 * @param[in,out] log Syslog object.
 * @param[in] cfg Configuration.
 */
void log_init(log_t *log, const config_t *cfg)
{
  log->facility = LOG_LOCAL7;
  log->level = LOG_INFO;
  log->tag = NULL;

  const char *facility = DEFAULT_SYSLOG_FACILITY;
  const char *level = DEFAULT_SYSLOG_LEVEL;
  const char *tag = DEFAULT_SYSLOG_TAG;

  config_setting_t *parent = config_lookup(cfg, "syslog");
  if (parent == NULL) {
    fprintf(stderr, "Warning: syslog section not found in config file.\n");
  }
  else {
    config_setting_lookup_string(parent, LOG_PARAM_FACILITY, &facility);
    config_setting_lookup_string(parent, LOG_PARAM_LEVEL, &level);
    config_setting_lookup_string(parent, LOG_PARAM_TAG, &tag);
  }

  log->facility = get_code_value(facilitynames, facility);
  if (log->facility < 0) {
    fprintf(stderr, "Warning: invalid syslog.facility value in config file.\n");
    log->facility = LOG_LOCAL7;
  }

  log->level = get_code_value(prioritynames, level);
  if (log->level < 0) {
    fprintf(stderr, "Warning: invalid syslog.level value in config file.\n");
    log->level = LOG_INFO;
  }

  if (strlen(tag) == 0) {
    fprintf(stderr, "Warning: syslog.tag is void in config file.\n");
    tag = DEFAULT_SYSLOG_TAG;
  }

  log->tag = strdup(tag);
  setlogmask(LOG_UPTO(log->level));
  openlog(log->tag, LOG_PERROR|LOG_CONS|LOG_NDELAY, log->facility);
  syslog(LOG_DEBUG, "syslog enabled [facility=%s, level=%s, tag=%s]", facility, level, tag);
}

/**************************************************************************//**
 * @brief Reset a log object.
 * @param[in,out] log Log object.
 */
void log_reset(log_t *log)
{
  if (log == NULL) {
    assert(0);
    return;
  }

  free(log->tag);
  log->tag = NULL;
}
