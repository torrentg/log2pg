
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
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <assert.h>
#include "utils.h"

/**************************************************************************//**
 * @brief Concatenate a variable number of string.
 * @details The caller should  deallocate the returned pointer using free.
 * @param[in] vanum Number of strings to concatenate.
 * @param[in] ... Variable number of arguments of type char*.
 * @return str1+str2+..., NULL if error.
 */
char* concat(size_t vanum, ...)
{
  if (vanum == 0) {
    assert(false);
    return(NULL);
  }

  va_list valist;

  size_t len = 0;
  va_start(valist, vanum);
  for (size_t i=0; i<vanum; i++) {
    char *str = va_arg(valist, char*);
    if (str != NULL) {
      len += strlen(str);
    }
  }
  va_end(valist);

  char *ret = (char *) malloc(len+1);
  if (ret == NULL) {
    return(NULL);
  }

  char *ptr = ret;
  va_start(valist, vanum);
  for (size_t i=0; i<vanum; i++) {
    char *str = va_arg(valist, char*);
    if (str != NULL) {
      strcpy(ptr, str);
      ptr += strlen(str);
    }
  }
  va_end(valist);

  *ptr = 0;
  return(ret);
}

/**************************************************************************//**
 * @brief Check if str2 starts with str1.
 * @param[in] str1 Prefix string.
 * @param[in] str2 Full string.
 * @return true=str2 starts with str1, false=otherwise.
 */
bool starts_with(const char *str1, const char *str2)
{
  assert(str1 != NULL);
  assert(str2 != NULL);
  if (str1 == NULL || str2 == NULL) {
    return(false);
  }

  size_t len1 = strlen(str1);
  size_t len2 = strlen(str2);
  if (len2 < len1) {
    return(false);
  }
  else {
    return(strncmp(str1, str2, len1)==0);
  }
}

/**************************************************************************//**
 * @brief Check if filename is a readable file.
 * @param[in] filename Filename to check.
 * @return true=it is a readable file, false=otherwise.
 */
bool is_readable_file(const char *filename)
{
  if (access(filename, R_OK) == 0) {
    struct stat path_stat;
    stat(filename, &path_stat);
    return S_ISREG(path_stat.st_mode);
  }
  return(false);
}

/**************************************************************************//**
 * @brief Check if filename is a readable directory.
 * @param[in] filename Directory path.
 * @return true=it is a readable directory, false=otherwise.
 */
bool is_readable_dir(const char *filename)
{
  if (access(filename, R_OK) == 0) {
    struct stat path_stat;
    stat(filename, &path_stat);
    return S_ISDIR(path_stat.st_mode);
  }
  return(false);
}

/**************************************************************************//**
 * @brief Replace char 'old' by char 'new' in string 'str'.
 * @param[in,out] str String to modify.
 * @param[in] old Character to replace.
 * @param[in] new New character to set.
 * @return Number of characters modified.
 */
int replace_char(char *str, char old, char new)
{
  if (str == NULL) {
    assert(false);
    return(0);
  }

  int ret = 0;
  size_t len = strlen(str);

  for(size_t i=0; i<len; i++) {
    if (str[i] == old) {
      str[i] = new;
      ret++;
    }
  }

  return(ret);
}

/**************************************************************************//**
 * @brief Returns number of seconds between two timevals.
 * @param[in] t1 Timeval1.
 * @param[in] t2 Timeval2, where t2 >= t1 .
 * @return t2 - t1 in seconds.
 */
double difftimeval(const struct timeval *t1, const struct timeval *t2)
{
  struct timeval d = {0};

  d.tv_sec = t2->tv_sec - t1->tv_sec;
  d.tv_usec = t2->tv_usec - t1->tv_usec;

  if (d.tv_usec < 0) {
    d.tv_usec += 1000000;
    d.tv_sec -= 1;
    if (d.tv_sec < 0) {
      d.tv_sec = 0;
      d.tv_usec = 0;
    }
  }

  return (double)(d.tv_sec) + (double)(d.tv_usec)/1000000.0;
}

/**************************************************************************//**
 * @brief Returns the number of millis from tv to now.
 * @param[in] t1 Timeval1 (t1 < now).
 * @return now - t1 in millis.
 */
size_t elapsed_millis(const struct timeval *t1)
{
  struct timeval d = {0};
  struct timeval now = {0};
  gettimeofday(&now, NULL);

  d.tv_sec = now.tv_sec - t1->tv_sec;
  d.tv_usec = now.tv_usec - t1->tv_usec;

  if (d.tv_usec < 0) {
    d.tv_usec += 1000000;
    d.tv_sec -= 1;
    if (d.tv_sec < 0) {
      d.tv_sec = 0;
      d.tv_usec = 0;
    }
  }

  return (size_t)(d.tv_sec*1000) + (size_t)(d.tv_usec)/1000;
}
