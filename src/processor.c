
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
#include <errno.h>
#include <string.h>
#include <stdbool.h>
#include "time.h"
#include <syslog.h>
#include <pcre2.h>
#include <assert.h>
#include "stringbuf.h"
#include "witem.h"
#include "wdata.h"
#include "utils.h"
#include "processor.h"

/**************************************************************************//**
 * @brief Types of database connection status.
 */
typedef enum {
  DISCARD_BUFFER_FULL,      // Buffer full.
  DISCARD_NO_MATCH_PATTERN, // Values not found.
  DISCARD_INTER_CHUNK       // Inter-chunk content.
} discard_cause_e;

/**************************************************************************//**
 * @brief Initialize the processor.
 * @param[in,out] processor Processor object.
 * @param[in] mqueue1 Message queue (monitor -> processor).
 * @param[in] mqueue2 Message queue (processor -> database).
 * @return 0=OK, otherwise an error ocurred.
 */
int processor_init(processor_t *processor, mqueue_t *mqueue1, mqueue_t *mqueue2)
{
  if (processor == NULL || mqueue1 == NULL || mqueue2 == NULL) {
    assert(false);
    return(1);
  }

  processor->mqueue1 = mqueue1;
  processor->mqueue2 = mqueue2;

  return(0);
}

/**************************************************************************//**
 * @brief Reset a processor object.
 * @param[in,out] processor Processor object.
 */
void processor_reset(processor_t *processor)
{
  if (processor != NULL) {
    processor->mqueue1 = NULL;
    processor->mqueue2 = NULL;
  }
}

/**************************************************************************//**
 * @brief Discarded content is appended to discard file (if set).
 * @param[in,out] item Watched item.
 * @param[in] str Discarded content.
 * @param[in] len Discarded content length.
 */
static void processor_discard(witem_t *item, discard_cause_e cause, const char *ptr, size_t len)
{
  syslog(LOG_DEBUG, "processor - discarded content '%.*s'", (int)(len), ptr);

  if (item->discard == NULL) {
    char *filename = witem_discard_filename(item);
    if (filename != NULL) {
      item->discard = fopen(filename, "a");
      if (item->discard == NULL) {
        syslog(LOG_WARNING, "error opening file '%s' - %s", filename, strerror(errno));
      }
      free(filename);
    }
  }

  if (item->discard == NULL) {
    return;
  }

  FILE *file = item->discard;
  const char *cause_str = NULL;
  time_t timer;
  char timestamp[26];
  struct tm* tm_info;

  time(&timer);
  tm_info = localtime(&timer);
  strftime(timestamp, 26, "%Y-%m-%d %H:%M:%S", tm_info);

  switch(cause) {
    case DISCARD_BUFFER_FULL:
      cause_str = "buffer full";
      break;
    case DISCARD_NO_MATCH_PATTERN:
      cause_str = "pattern values no match";
      break;
    case DISCARD_INTER_CHUNK:
      cause_str = "inter-chunk content";
      break;
  }

  //TODO: add line number
  fprintf(file, "%s - file=%s, cause=%s\n", timestamp, item->filename, cause_str);
  fwrite(ptr, sizeof(char), len, file);
  fflush(file);
}

/**************************************************************************//**
 * @brief Returns the position in str where match ocurres.
 * @see https://www.pcre.org/current/doc/html/pcre2api.html#SEC31
 * @param[in] md Match data.
 * @param[in] pos Desired position (0=begin, 1=end).
 * @return Match position.
 */
static int get_match_pos(pcre2_match_data *md, size_t pos)
{
  PCRE2_SIZE *ovector = pcre2_get_ovector_pointer(md);
  return (int)ovector[pos];
}

/**************************************************************************//**
 * @brief Initialize buffer and data linked to regex match.
 * @param[in,out] item Watched item to initialize.
 * @return 0=OK, otherwise=error.
 */
static int witem_flush_buffer(witem_t *item)
{
  if (item == NULL || item->buffer == NULL) {
    assert(false);
    return(9);
  }

  item->buffer_pos = 0;
  item->buffer[0] = '\0';
  return(0);
}

/**************************************************************************//**
 * @brief Trace chunk values.
 * @param[in] md Match data.
 * @param[in] format File format.
 */
static void trace_chunk_values(const char *str, pcre2_match_data *md, const format_t *format)
{
  if (loglevel != LOG_DEBUG) {
    return;
  }

  stringbuf_t aux = {0};
  PCRE2_SIZE *ovector = pcre2_get_ovector_pointer(md);

  stringbuf_append(&aux, "[");

  for(uint32_t i=0; i<format->parameters.size; i++)
  {
    if (aux.length > 1) {
      stringbuf_append(&aux, ", ");
    }

    stringbuf_append(&aux, format->parameters.data[i]);
    stringbuf_append(&aux, "=");
    size_t pos = ovector[2*(i+1)];
    int len = (int)(ovector[2*(i+1)+1] - ovector[2*(i+1)]);
    stringbuf_append_n(&aux, str+pos, len);
  }

  stringbuf_append(&aux, "]");

  syslog(LOG_DEBUG, "processor - values=%s", aux.data);
  stringbuf_reset(&aux);
}

/**************************************************************************//**
 * @brief Process an event.
 * @see https://www.pcre.org/current/doc/html/pcre2api.html#SEC31
 * @see https://www.pcre.org/current/doc/html/pcre2_match.html
 * @param[in] processor Processor parameters.
 * @param[in,out] item Witem to process.
 * @param[in] str String to process (not \0 terminated).
 * @param[in] len Length of the string.
 */
static void process_chunk(processor_t *processor, witem_t *item, const char *str, size_t len)
{
  assert(processor != NULL);
  assert(item != NULL);
  assert(str != NULL);
  assert(len > 0);

  syslog(LOG_DEBUG, "processor - processing chunk '%.*s'", (int)(len), str);

  int rc = 0;
  format_t *format = ((file_t *) item->ptr)->format;
  assert(format != NULL);

  // regex matching
  rc = pcre2_match(format->re_values, (PCRE2_SPTR)str, (PCRE2_SIZE)len, 0, PCRE2_NOTEMPTY, item->md_values, NULL);
  if (rc < 0) {
    processor_discard(item, DISCARD_NO_MATCH_PATTERN, str, len);
    return;
  }

  trace_chunk_values(str, item->md_values, format);
  wdata_t *data = wdata_alloc(item, str);
  mqueue_push(processor->mqueue2, MSG_TYPE_MATCH1, data, false, 0);
}

/**************************************************************************//**
 * @brief Process witem buffer identifying chunks.
 * @details Supports three modes:
 *    only-starts : only declared regex starts.
 *    only-ends : only declared regex ends.
 *    starts-ends : both regex declared.
 * @param[in] processor Processor parameters.
 * @param[in,out] item Witem to process.
 */
static void process_buffer(processor_t *processor, witem_t *item)
{
  assert(processor != NULL);
  assert(item != NULL);
  assert(item->ptr != NULL);

  int rc = 0;
  format_t *format = ((file_t *) item->ptr)->format;
  const char *str = item->buffer;
  size_t len = item->buffer_pos;
  int lpm1 = 0; // length previous match1 (only applies in case only-starts)
  int pos1 = 0; // current chunk starts (relative to str)
  int pos2 = 0; // current chunk ends (relative to str)
  const char *str_chunk = NULL;
  size_t len_chunk = 0;

  while(len > 0)
  {
    pos1 = -1;
    if (item->md_starts != NULL) {
      rc = pcre2_match(format->re_starts, (PCRE2_SPTR)str, (PCRE2_SIZE)len, (PCRE2_SIZE)lpm1,
                       PCRE2_NOTEMPTY|PCRE2_NOTBOL|PCRE2_NOTEOL, item->md_starts, NULL);
      if (rc < 0) break;
      pos1 = get_match_pos(item->md_starts, 0);
    }

    pos2 = -1;
    if (item->md_ends != NULL) {
      size_t offset = (pos1>=0?get_match_pos(item->md_starts, 1):0);
      rc = pcre2_match(format->re_ends, (PCRE2_SPTR)str, (PCRE2_SIZE)len, (PCRE2_SIZE)offset,
                       PCRE2_NOTEMPTY|PCRE2_NOTBOL|PCRE2_NOTEOL, item->md_ends, NULL);
      if (rc < 0) break;
      pos2 = get_match_pos(item->md_ends, 1);
    }

    assert(pos1 >= 0 || pos2 >= 0);

    if (pos1 < 0) { // case only ends
      str_chunk = str;
      len_chunk = pos2;
      str += pos2;
      len -= pos2;
    }
    else if (pos2 < 0) { // case only starts
      str_chunk = str;
      len_chunk = pos1;
      str += pos1;
      len -= pos1;
      lpm1 = get_match_pos(item->md_starts, 1) - get_match_pos(item->md_starts, 0);
    }
    else { // case starts and ends
      assert(pos1 < pos2);
      str_chunk = str + pos1;
      len_chunk = pos2 - pos1;
      str += pos2;
      len -= pos2;
      //TODO: process discarded content (if any)
    }

    // len_chunk=0 happends when only-starts and process_buffer()
    // is called twice because lpm1 is reseted.
    if (len_chunk > 0) {
      process_chunk(processor, item, str_chunk, len_chunk);
    }
  }

  memmove(item->buffer, str, len);
  item->buffer_pos = len;
}

/**************************************************************************//**
 * @brief Read and parses new info appended to file.
 * @param[in] processor Processor parameters.
 * @param[in,out] item Witem to process.
 */
static void process_witem(processor_t *processor, witem_t *item)
{
  assert(processor != NULL);
  assert(item != NULL);
  assert(item->type == WITEM_FILE);
  assert(item->file != NULL);
  assert(item->buffer != NULL);

  bool more = true;

  syslog(LOG_DEBUG, "processor - processing file %s", item->filename);

  // read and process new data until EOF
  while(more)
  {
    if (item->buffer_pos >= item->buffer_length-1) {
      processor_discard(item, DISCARD_BUFFER_FULL, item->buffer, item->buffer_length-1);
      witem_flush_buffer(item);
    }

    size_t buffer_free_bytes = item->buffer_length - item->buffer_pos - 1;
    size_t len = fread(item->buffer + item->buffer_pos, 1, buffer_free_bytes, item->file);
    if (len == 0) {
      break;
    }
    //TODO: if error at fread -> try to reopen ?

    more = (len == buffer_free_bytes);
    item->buffer_pos += len;
    item->buffer[item->buffer_pos] = '\0';

    process_buffer(processor, item);
  }
}

/**************************************************************************//**
 * @brief Process file events readed from queue.
 * @details This function block the current thread until NULL event is received.
 * @param[in] ptr Monitor parameters.
 */
void* processor_run(void *ptr)
{
  processor_t *processor = (processor_t *) ptr;
  if (processor == NULL || processor->mqueue1 == NULL || processor->mqueue2 == NULL) {
    assert(false);
    return(NULL);
  }
  syslog(LOG_DEBUG, "processor - thread started");

  while(true)
  {
    // waiting for a new message
    msg_t msg = mqueue_pop(processor->mqueue1, 0);

    // processing message
    if (msg.type == MSG_TYPE_ERROR) {
      terminate(EXIT_FAILURE);
      break;
    }
    else if(msg.type == MSG_TYPE_CLOSE) {
      break;
    }
    else if (msg.type == MSG_TYPE_EINTR || msg.type == MSG_TYPE_NULL) {
      assert(false);
      continue;
    }
    else {
      assert(msg.data != NULL);
      witem_t *item = (witem_t *) msg.data;

      process_witem(processor, item);

      if (msg.type == MSG_TYPE_FILE1) {
        witem_free(item);
      }
    }
  }

  // sends termination signal to database thread
  mqueue_close(processor->mqueue2);

  syslog(LOG_DEBUG, "processor - thread ended");
  return(NULL);
}

