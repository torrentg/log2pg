
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
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <assert.h>
#include "config.h"
#include "format.h"

#define FORMAT_PARAM_NAME "name"
#define FORMAT_PARAM_MAXLENGTH "maxlength"
#define FORMAT_PARAM_STARTS "starts"
#define FORMAT_PARAM_ENDS "ends"
#define FORMAT_PARAM_VALUES "values"

#define FORMAT_DEFAULT_MAXLENGTH 10000
#define MAX_NUM_PARAMS 99

static const char *FORMAT_DEFAULT_ENDS = "\\n";
static const char *FORMAT_PARAMS[] = {
    FORMAT_PARAM_NAME,
    FORMAT_PARAM_MAXLENGTH,
    FORMAT_PARAM_STARTS,
    FORMAT_PARAM_ENDS,
    FORMAT_PARAM_VALUES,
    NULL
};

/**************************************************************************//**
 * @brief Returns the list of named substrings in a regular expression.
 * @details "^(?<key>[[:alpha:]]\w*)\s*=\s*(?<value>.*)$" -> [key, value]
 * @see Code based on pcre2demo.c
 * @param[in] regex A compiled regular expression.
 * @param[in,out] lst List of parameters (initially empty).
 * @return 0=OK, otherwise=KO.
 */
static int regex_get_parameters(pcre2_code *regex, vector_t *lst)
{
  if (regex == NULL || lst == NULL || lst->size > 0) {
    assert(false);
    return(1);
  }

  uint32_t namecount;
  uint32_t name_entry_size;
  PCRE2_SPTR tabptr;

  // extracts the count of named parentheses from the pattern
  pcre2_pattern_info(
      regex,                    /* the compiled pattern */
      PCRE2_INFO_NAMECOUNT,     /* get the number of named substrings */
      &namecount);              /* where to put the answer */

  if (namecount == 0) {
    return(0);
  }

  for(size_t i=0; i<namecount; i++) {
    vector_insert(lst, NULL);
  }

  // extracts the table for translating names to numbers
  pcre2_pattern_info(
      regex,                    /* the compiled pattern */
      PCRE2_INFO_NAMETABLE,     /* address of the table */
      &tabptr);                 /* where to put the answer */

  // extracts the size of each entry in the table
  pcre2_pattern_info(
      regex,                    /* the compiled pattern */
      PCRE2_INFO_NAMEENTRYSIZE, /* size of each entry in the table */
      &name_entry_size);        /* where to put the answer */

  // retrieves substring by name from table
  for (uint32_t i=0; i<namecount; i++) {
    int n = (tabptr[0] << 8) | tabptr[1];
    lst->data[n-1] = strdup((const char*)tabptr+2);
    tabptr += name_entry_size;
  }

  return(0);
}

/**************************************************************************//**
 * @brief Allocate and initialize a format object.
 * @param[in] name Format name.
 * @param[in] maxlength Maximum line length.
 * @param[in] re_starts Regular expression pattern.
 * @param[in] re_ends Regular expression pattern.
 * @param[in] re_values Regular expression pattern.
 * @return Initialized object or NULL if error.
 */
static format_t* format_alloc(const char *name, size_t maxlength, pcre2_code *re_starts, pcre2_code *re_ends, pcre2_code *re_values,
                              const char *pattern_starts, const char *pattern_ends, const char *pattern_values)
{
  assert(name != NULL);
  assert(maxlength > 0);
  assert(re_starts != NULL || re_ends != NULL);
  assert(re_values != NULL);

  format_t *ret = (format_t *) calloc(1, sizeof(format_t));
  if (ret == NULL) {
    return(NULL);
  }

  ret->name = strdup(name);
  ret->maxlength = maxlength;
  ret->re_starts = re_starts;
  ret->re_ends = re_ends;
  ret->re_values = re_values;
  vector_reset(&(ret->parameters), NULL);
  regex_get_parameters(re_values, &(ret->parameters));

  char *str = vector_print(&(ret->parameters));
  syslog(LOG_DEBUG, "created format [address=%p, name=%s, maxlength=%zu, starts=%s, ends=%s, values=%s, parameters=%s]",
         (void *)ret, name, maxlength, pattern_starts, pattern_ends, pattern_values, str);
  free(str);

  return(ret);
}

/**************************************************************************//**
 * @brief Frees memory space pointed by ptr.
 * @param[in] ptr Pointer to format object.
 */
void format_free(void *ptr)
{
  if (ptr == NULL) return;
  format_t *obj = (format_t *) ptr;

  syslog(LOG_DEBUG, "removed format [address=%p, name=%s]", ptr, obj->name);

  free(obj->name);
  pcre2_code_free(obj->re_starts);
  pcre2_code_free(obj->re_ends);
  pcre2_code_free(obj->re_values);
  vector_reset(&(obj->parameters), free);
  free(obj);
}

/**************************************************************************//**
 * @brief Compiles a regexp pattern.
 * @see https://www.pcre.org/current/doc/html/pcre2_compile.html
 * @param[in] pattern Regular expression pattern.
 * @param[in] name Format name.
 * @param[in] filename Configuration file filename.
 * @param[in] line Line where format is declared in config file.
 * @return regular expression or NULL if error.
 */
static pcre2_code* format_compile_regex(const char *pattern, const config_setting_t *setting)
{
  if (pattern == NULL) {
    return (NULL);
  }

  pcre2_code *ret = NULL;
  PCRE2_SIZE erroroffset = 0;
  int errornumber = 0;

  ret = pcre2_compile(
      (PCRE2_SPTR) pattern,  /* the pattern */
      PCRE2_ZERO_TERMINATED, /* indicates pattern is zero-terminated */
      PCRE2_MULTILINE|PCRE2_NO_AUTO_CAPTURE,  /* options */
      &errornumber,          /* for error number */
      &erroroffset,          /* for error offset */
      NULL);                 /* use default compile context */

  if (ret == NULL && setting != NULL) {
    PCRE2_UCHAR buffer[256];
    pcre2_get_error_message(errornumber, buffer, sizeof(buffer));
    syslog(LOG_ERR, "invalid regular expression at %s:%d - %s.",
           config_setting_source_file(setting),
           config_setting_source_line(setting),
           buffer);
  }

  return(ret);
}

/**************************************************************************//**
 * @brief Parse a format entry and adds to table list.
 * @param[in,out] lst List of formats.
 * @param[in] setting Configuration setting.
 * @return 0=OK, otherwise=KO.
 */
static int formats_parse_item(vector_t *lst, const config_setting_t *setting)
{
  assert(lst != NULL);
  assert(setting != NULL);

  int rc = 0;
  const char *name = NULL;
  const char *pattern_starts = NULL;
  const char *pattern_ends = NULL;
  const char *pattern_values = NULL;
  size_t maxlength = FORMAT_DEFAULT_MAXLENGTH;

  // check attributes
  rc = setting_check_childs(setting, FORMAT_PARAMS);

  // retrieving attributes
  config_setting_lookup_string(setting, FORMAT_PARAM_NAME, &name);
  setting_read_uint(setting, FORMAT_PARAM_MAXLENGTH, &maxlength);
  config_setting_lookup_string(setting, FORMAT_PARAM_STARTS, &pattern_starts);
  config_setting_lookup_string(setting, FORMAT_PARAM_ENDS, &pattern_ends);
  config_setting_lookup_string(setting, FORMAT_PARAM_VALUES, &pattern_values);

  // check if attributes are set
  if (name == NULL) {
    syslog(LOG_ERR, "format without " FORMAT_PARAM_NAME " at %s:%d.",
           config_setting_source_file(setting),
           config_setting_source_line(setting));
    rc = 1;
  }
  if (pattern_values == NULL) {
    config_setting_t *aux = config_setting_get_member(setting, FORMAT_PARAM_NAME);
    syslog(LOG_ERR, "format without " FORMAT_PARAM_VALUES " at %s:%d.",
           config_setting_source_file(aux),
           config_setting_source_line(aux));
    rc = 1;
  }

  // check maximum length
  if (maxlength < 32) {
    config_setting_t *aux = config_setting_get_member(setting, FORMAT_PARAM_MAXLENGTH);
    syslog(LOG_ERR, "format with " FORMAT_PARAM_MAXLENGTH " < 32 at %s:%d.",
           config_setting_source_file(aux),
           config_setting_source_line(aux));
    rc = 1;
  }

  // check if event bounds are set
  if (pattern_starts == NULL && pattern_ends == NULL) {
    pattern_ends = FORMAT_DEFAULT_ENDS;
  }

  // check if name exists
  int index = vector_find(lst, name);
  if (index >= 0) {
    config_setting_t *aux = config_setting_get_member(setting, FORMAT_PARAM_NAME);
    syslog(LOG_ERR, "duplicated format " FORMAT_PARAM_NAME " '%s' at %s:%d.", name,
           config_setting_source_file(aux),
           config_setting_source_line(aux));
    rc = 1;
  }

  // compile patterns
  pcre2_code *re_starts = format_compile_regex(pattern_starts,
                          config_setting_get_member(setting, FORMAT_PARAM_STARTS));
  pcre2_code *re_ends   = format_compile_regex(pattern_ends,
                          config_setting_get_member(setting, FORMAT_PARAM_ENDS));
  pcre2_code *re_values  = format_compile_regex(pattern_values,
                          config_setting_get_member(setting, FORMAT_PARAM_VALUES));
  if ((pattern_starts != NULL && re_starts == NULL) ||
      (pattern_ends != NULL && re_ends == NULL)   ||
      (pattern_values != NULL && re_values == NULL)) {
    rc = 1;
  }

  // exit if errors
  if (rc != 0) {
    pcre2_code_free(re_starts);
    pcre2_code_free(re_ends);
    pcre2_code_free(re_values);
    return(rc);
  }

  // create format
  format_t *item = format_alloc(name, maxlength, re_starts, re_ends, re_values, pattern_starts, pattern_ends, pattern_values);
  if (item == NULL) {
    pcre2_code_free(re_starts);
    pcre2_code_free(re_ends);
    pcre2_code_free(re_values);
    return(1);
  }

  // check the number of parameters
  if (item->parameters.size > MAX_NUM_PARAMS) {
    config_setting_t *aux = config_setting_get_member(setting, FORMAT_PARAM_VALUES);
    syslog(LOG_ERR, FORMAT_PARAM_VALUES " with more than %d parameters at %s:%d.",
           MAX_NUM_PARAMS,
           config_setting_source_file(aux),
           config_setting_source_line(aux));
    format_free(item);
    return(1);
  }

  // append format to list
  rc = vector_insert(lst, item);
  if (rc != 0) {
    format_free(item);
    return(1);
  }

  return(rc);
}

/**************************************************************************//**
 * @brief Initialize the list of formats.
 * @param[in,out] lst Empty list of formats.
 * @param[in] cfg Configuration file.
 * @return 0=OK, otherwise an error ocurred.
 */
int formats_init(vector_t *lst, const config_t *cfg)
{
  assert(cfg != NULL);
  assert(lst != NULL);
  if (cfg == NULL || lst == NULL || lst->size > 0) {
    return(1);
  }

  // getting tables entry in configuration file
  config_setting_t *parent = setting_get_list(cfg, "formats");
  if (parent == NULL) {
    return(1);
  }

  int rc = 0;
  int len = config_setting_length(parent);

  // parsing each list entry
  for(int i=0; i<len; i++) {
    config_setting_t *setting = config_setting_get_elem(parent, i);
    rc |= formats_parse_item(lst, setting);
  }

  return(rc);
}
