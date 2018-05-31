
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
 * @param[out] syslog_tag Syslog tag name (to be free by caller).
 * @param[in] cfg Configuration.
 */
void init_syslog(char **syslog_tag, const config_t *cfg)
{
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

  int ifacility = get_code_value(facilitynames, facility);
  if (ifacility < 0) {
    fprintf(stderr, "Warning: invalid syslog.facility value in config file.\n");
    ifacility = LOG_LOCAL7;
  }

  int ilevel = get_code_value(prioritynames, level);
  if (ilevel < 0) {
    fprintf(stderr, "Warning: invalid syslog.level value in config file.\n");
    ilevel = LOG_INFO;
  }

  if (strlen(tag) == 0) {
    fprintf(stderr, "Warning: syslog.tag is void in config file.\n");
    tag = DEFAULT_SYSLOG_TAG;
  }

  *syslog_tag = strdup(tag);
  setlogmask(LOG_UPTO(ilevel));
  openlog(*syslog_tag, LOG_PERROR|LOG_CONS|LOG_NDELAY, ifacility);
  syslog(LOG_DEBUG, "syslog enabled [facility=%s, level=%s, tag=%s]", facility, level, tag);
}

