
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

#ifndef FORMATS_H
#define FORMATS_H

#include <stddef.h>
#include <libconfig.h>
#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>
#include "vector.h"

/**************************************************************************//**
 * @brief Format defined in configuration file.
 * @details First member is 'char *' to be searchable.
 * @see https://www.pcre.org/current/doc/html/pcre2api.html
 */
typedef struct format_t
{
  //! Format name.
  char *name;
  //! Maximum length.
  size_t maxlength;
  //! Regular expression.
  pcre2_code *re_starts;
  //! Regular expression.
  pcre2_code *re_ends;
  //! Regular expression.
  pcre2_code *re_values;
  //! Format parameters (strings).
  vector_t parameters;
} format_t;

/**************************************************************************
 * Function declarations.
 */
extern int formats_init(vector_t *lst, const config_t *cfg);
extern void format_free(void *obj);

#endif

