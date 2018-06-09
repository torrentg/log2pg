
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

#ifndef UTILS_H
#define UTILS_H

#include <stdbool.h>
#include <stddef.h>
#include <sys/time.h>

extern char* concat(size_t vanum, ...);
extern bool starts_with(const char *str1, const char *str2);
extern bool is_readable_file(const char *filename);
extern bool is_readable_dir(const char *filename);
extern int replace_char(char *str, char old, char new);
extern double difftimeval(const struct timeval *tv1, const struct timeval *tv2);
extern size_t elapsed_millis(const struct timeval *tv);
extern void* memdup(const void* ptr, size_t size);
extern char* replace_str(const char *str, const char *from, const char *to);
extern const char *filename_ext(const char *filename);

#endif

