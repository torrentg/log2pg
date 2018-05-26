
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

#ifndef CONFIG_H
#define CONFIG_H

#include <libconfig.h>

extern int setting_read_uint(const config_setting_t *parent, const char *name, size_t *value);
extern config_setting_t* setting_get_list(const config_t *cfg, const char *name);
extern int init_config(config_t *cfg, const char *filename);
extern int setting_check_childs(const config_setting_t *setting, const char **childnames);

#endif

