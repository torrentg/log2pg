
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

#ifndef LOG_H
#define LOG_H

#include <libconfig.h>

/**************************************************************************//**
 * @brief Log struct.
 */
typedef struct log_t
{
    //! Syslog tag.
    char *tag;
  //! Syslog facility.
  int facility;
  //! Syslog level.
  int level;

} log_t;

/**************************************************************************
 * Function declarations.
 */
extern void log_init(log_t *log, const config_t *cfg);
extern void log_reset(log_t *log);

#endif

