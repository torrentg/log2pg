
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
#include <glob.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <libgen.h>
#include <fnmatch.h>
#include <assert.h>
#include "utils.h"
#include "config.h"
#include "entities.h"

#define FILE_PARAM_PATH "path"
#define FILE_PARAM_FORMAT "format"
#define FILE_PARAM_TABLE "table"
#define FILE_PARAM_DISCARD "discard"

static const char *FILE_PARAMS[] = {
    FILE_PARAM_PATH,
    FILE_PARAM_FORMAT,
    FILE_PARAM_TABLE,
    FILE_PARAM_DISCARD,
    NULL
};

/**************************************************************************//**
 * @brief Allocate and initialize a wfile object.
 * @param[in] name File name.
 * @param[in] format Pointer to format.
 * @param[in] table Pointer to table.
 * @param[in] discard Discard filename (can be NULL).
 * @return Initialized object or NULL if error.
 */
static file_t* wfile_alloc(const char *pattern, format_t *format, table_t *table, const char *discard)
{
  assert(pattern != NULL);
  assert(format != NULL);
  assert(table != NULL);

  file_t *ret = (file_t *) malloc(sizeof(file_t));
  if (ret == NULL) {
    return(NULL);
  }

  syslog(LOG_DEBUG, "created wfile [address=%p, pattern=%s, format=%s, table=%s, discard=%s]",
         (void *)ret, pattern, format->name, table->name, discard);

  ret->pattern = strdup(pattern);
  ret->format = format;
  ret->table = table;
  ret->discard = NULL;

  if (discard != NULL) {
    ret->discard = strdup(discard);
  }

  return(ret);
}

/**************************************************************************//**
 * @brief Frees memory space pointed by ptr.
 * @param[in] ptr Pointer to wfile object.
 */
static void wfile_free(void *ptr)
{
  if (ptr == NULL) return;
  file_t *obj = (file_t *) ptr;

  syslog(LOG_DEBUG, "removed wfile [address=%p, pattern=%s, format=%s, table=%s, discard=%s]",
         ptr, obj->pattern, obj->format->name, obj->table->name, obj->discard);

  free(obj->pattern);
  free(obj->discard);
  free(ptr);
}

/**************************************************************************//**
 * @brief Allocate and initialize a wdir object.
 * @param[in] path Directory path.
 * @return Initialized object or NULL if error.
 */
static dir_t *wdir_alloc(const char *path)
{
  assert(path != NULL);

  dir_t *ret = (dir_t *) malloc(sizeof(dir_t));
  if (ret == NULL) {
    return(NULL);
  }

  syslog(LOG_DEBUG, "created wdir [address=%p, path=%s]", (void *)ret, path);

  ret->path = strdup(path);
  ret->files.data = NULL;
  ret->files.capacity = 0;
  ret->files.size = 0;

  return(ret);
}

/**************************************************************************//**
 * @brief Frees memory space pointed by ptr.
 * @param[in] ptr Pointer to wdir object.
 */
void dir_free(void *ptr)
{
  if (ptr == NULL) return;
  dir_t *obj = (dir_t *) ptr;

  syslog(LOG_DEBUG, "removed wdir [address=%p, path=%s]", ptr, obj->path);

  free(obj->path);
  vector_reset(&(obj->files), wfile_free);
  free(ptr);
}

/**************************************************************************//**
 * @brief Add config entry into dirs list.
 * @param[in,out] list List of directories.
 * @param[in] path Directory path.
 * @param[in] pattern File name (possibly a pattern).
 * @param[in] format Pointer to format.
 * @param[in] table Pointer to table.
 * @param[in] discard Discard filename (can be NULL).
 * @return 0=OK, otherwise = an error ocurred.
 */
static int dirs_add(vector_t *lst, const char *path, const char *pattern, format_t *format,
                    table_t *table, const char *discard)
{
  assert(lst != NULL);
  assert(path != NULL);
  assert(pattern != NULL);
  assert(format != NULL);
  assert(table != NULL);

  int rc = 0;
  char *realdir = NULL;
  dir_t *dir = NULL;

  // Adding the directory to list of watched dirs. If file is
  // removed and recreated, the directory monitor will catch the
  // file creation event and reinstalls the file watch.
  realdir = realpath(path, NULL);
  int ipos = vector_find(lst, realdir);
  if (ipos >= 0)  {
    dir = (dir_t *) lst->data[ipos];
  }
  else {
    dir = wdir_alloc(realdir);
    vector_insert(lst, dir);
  }

  // check if pattern exists
  ipos = vector_find(&(dir->files), pattern);
  if (ipos >= 0)  {
    syslog(LOG_WARNING, "duplicated file pattern '%s/%s' - first entry applies", realdir, pattern);
    rc = 1;
    goto dirs_add_exit;
  }

  // Adding the file pattern to directory. Every file creation
  // will be matched with the list of file patterns to determine
  // if the new file need to be watched.
  file_t *file = wfile_alloc(pattern, format, table, discard);
  vector_insert(&(dir->files), file);

dirs_add_exit:
  free(realdir);
  return(rc);
}

/**************************************************************************//**
 * @brief Checks that table parameters are included in format parameters.
 * @param[in] format Format object.
 * @param[in] table Table object.
 * @return 0=OK, otherwise=KO.
 */
static int dirs_check_parameters(config_setting_t *setting, format_t *format, table_t *table)
{
  assert(setting != NULL);
  assert(format != NULL);
  assert(table != NULL);

  int rc = 0;

  for(uint32_t j=0; j<table->parameters.size; j++) {
    bool found = false;
    for(uint32_t i=0; i<format->parameters.size; i++) {
      if (strcmp(format->parameters.data[i], table->parameters.data[j]) == 0) {
        found = true;
        break;
      }
    }
    if (!found) {
      config_setting_t *aux = config_setting_lookup(setting, FILE_PARAM_TABLE);
      syslog(LOG_ERR, "error at %s:%d - parameter '%s' declared in table '%s' not found in '%s' format regex",
             config_setting_source_file(aux),
             config_setting_source_line(aux),
             (char*)(table->parameters.data[j]),
             table->name, format->name);
      rc = 1;
    }
  }

  return(rc);
}

/**************************************************************************//**
 * @brief Parse a monitor entry and adds to list.
 * @detail setting format: { path="xxx"; format="yyy"; table="zzz" }
 * @param[in,out] list List of watched items.
 * @param[in] setting Configuration setting.
 * @param[in] formats List of user-defined formats.
 * @param[in] tables List of user-defined tables.
 * @return 0=OK, otherwise=KO.
 */
static int dirs_parse(vector_t *lst, config_setting_t *setting, vector_t *formats, vector_t *tables)
{
  assert(lst != NULL);
  assert(setting != NULL);
  assert(formats != NULL);
  assert(tables != NULL);

  int rc = 0;
  char *path_aux = NULL;
  glob_t globbuf = {0};
  const char *path = NULL;
  const char *format = NULL;
  const char *table = NULL;
  const char *discard = NULL;

  // check attributes
  rc = setting_check_childs(setting, FILE_PARAMS);

  // reading attributes
  config_setting_lookup_string(setting, "path", &path);
  if (path == NULL) {
    syslog(LOG_ERR, "file without path at %s:%d.",
           config_setting_source_file(setting),
           config_setting_source_line(setting));
    rc = 1;
  }

  config_setting_lookup_string(setting, "format", &format);
  if (format == NULL) {
    syslog(LOG_ERR, "file without format at %s:%d.",
           config_setting_source_file(setting),
           config_setting_source_line(setting));
    rc = 1;
  }

  config_setting_lookup_string(setting, "table", &table);
  if (table == NULL) {
    syslog(LOG_ERR, "file without table at %s:%d.",
           config_setting_source_file(setting),
           config_setting_source_line(setting));
    rc = 1;
  }

  config_setting_lookup_string(setting, "discard", &discard);

  if (rc != 0) {
    goto files_parse_item_exit;
  }

  // obtaining format index
  int iformat = vector_find(formats, format);
  if (iformat < 0) {
    config_setting_t *aux = config_setting_lookup(setting, FILE_PARAM_FORMAT);
    syslog(LOG_ERR, "unrecognized format identifier '%s' at %s:%d.", format,
           config_setting_source_file(aux),
           config_setting_source_line(aux));
    rc = 1;
    goto files_parse_item_exit;
  }
  format_t *oformat = (format_t *) formats->data[iformat];

  // obtaining table index
  int itable = vector_find(tables, table);
  if (itable < 0) {
    config_setting_t *aux = config_setting_lookup(setting, FILE_PARAM_TABLE);
    syslog(LOG_ERR, "unrecognized table identifier '%s' at %s:%d.", table,
           config_setting_source_file(aux),
           config_setting_source_line(aux));
    rc = 1;
    goto files_parse_item_exit;
  }
  table_t *otable = (table_t *) tables->data[itable];

  // splitting file in dir + filepattern
  path_aux = strdup(path);
  const char *filepattern = basename(path_aux);
  const char *dir = dirname(path_aux);

  // checking file pattern
  int len = strlen(filepattern);
  if (len > 0 && (filepattern[len-1] == '/' || filepattern[len-1] == '.')) {
    config_setting_t *aux = config_setting_lookup(setting, FILE_PARAM_PATH);
    syslog(LOG_ERR, "invalid filename '%s' at %s:%d", filepattern,
           config_setting_source_file(aux),
           config_setting_source_line(aux));
    rc = 1;
    goto files_parse_item_exit;
  }

  // check that table parameters are included in format parameters
  rc = dirs_check_parameters(setting, oformat, otable);
  if (rc != 0) {
    goto files_parse_item_exit;
  }

  // searching matched directories
  rc = glob(dir, GLOB_ONLYDIR|GLOB_BRACE, NULL, &globbuf);
  if (rc == GLOB_NOMATCH) {
    syslog(LOG_ALERT, "directory '%s' not found", dir);
    rc = 0;
    goto files_parse_item_exit;
  }
  else if (rc != 0) {
    rc = 1;
    goto files_parse_item_exit;
  }

  // adding directories and files
  for(int i=0; globbuf.gl_pathv[i]!=NULL; i++) {
    dirs_add(lst, globbuf.gl_pathv[i], filepattern, oformat, otable, discard);
  }

files_parse_item_exit:
  globfree(&globbuf);
  free(path_aux);
  return(rc);
}

/**************************************************************************//**
 * @brief Initialize the list of objects to monitor.
 * @param[in,out] lst List of watched directories.
 * @param[in] cfg Configuration file.
 * @param[in] formats List of user-defined formats.
 * @param[in] tables List of user-defined tables.
 * @return 0=OK, otherwise an error ocurred.
 */
int dirs_init(vector_t *lst, const config_t *cfg, vector_t *formats, vector_t *tables)
{
  assert(lst != NULL);
  assert(cfg != NULL);
  assert(formats != NULL);
  assert(tables != NULL);
  if (lst == NULL || cfg == NULL || formats == NULL || tables == NULL) {
    return(1);
  }

  // getting monitor entry in configuration file
  config_setting_t *parent = setting_get_list(cfg, "files");
  if (parent == NULL) {
    return(1);
  }

  int rc = 0;
  int len = config_setting_length(parent);

  // parsing each list entry
  for(int i=0; i<len; i++) {
    config_setting_t *setting = config_setting_get_elem(parent, i);
    rc |= dirs_parse(lst, setting, formats, tables);
  }

  return(rc);
}

/**************************************************************************//**
 * @brief Check if the given filename match a pattern.
 * @param[in] dir Wdir object.
 * @param[in] name File name to check against file patterns.
 * @return Index of matched file, negative value if not matched.
 */
int dir_file_match(dir_t *dir, const char *name)
{
  assert(dir != NULL);
  assert(name != NULL);
  if (dir == NULL || name == NULL) {
    return(-1);
  }

  for(uint32_t i=0; i<dir->files.size; i++) {
    file_t *file = (file_t *) dir->files.data[i];
    assert(file != NULL);
    assert(file->pattern != NULL);
    if (file != NULL && file->pattern != NULL) {
      if (fnmatch(file->pattern, name, FNM_PATHNAME|FNM_PERIOD) == 0) {
        return(i);
      }
    }
  }
  return(-1);
}

