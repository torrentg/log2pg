
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

#ifndef LOG2PG_H
#define LOG2PG_H

#include <signal.h>

/**************************************************************************
 * Required by PCRE2.
 */
#define PCRE2_CODE_UNIT_WIDTH 8

/**************************************************************************
 * Public values.
 */
#define PACKAGE_NAME "log2pg"
#define PACKAGE_VERSION "0.2.0"

/**************************************************************************
 * Public macros.
 */
#define UNUSED(x) ((void)(x))
#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

/**************************************************************************
 * Public variables.
 */
extern volatile sig_atomic_t keep_running;
//TODO: add loglevel

/**************************************************************************
 * Public functions.
 */
extern void terminate(int exitcode);

#endif

