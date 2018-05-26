
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

#include <glob.h>
#include <syslog.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <sys/inotify.h>
#include <limits.h>
#include <stdlib.h>
#include <signal.h>
#include <assert.h>
#include "witem.h"
#include "map.h"
#include "utils.h"
#include "monitor.h"

#ifdef __GNUC__
#  define ALIGNAS(TYPE) __attribute__ ((aligned(__alignof__(TYPE))))
#else
#  define ALIGNAS(TYPE) /* empty */
#endif

#define UNUSED(x) (void)(x)
#define EVENT_SIZE (sizeof(struct inotify_event) + NAME_MAX + 1)
#define BUFFER_LEN (256*(EVENT_SIZE))

// external declarations
extern volatile sig_atomic_t keep_running;

/**************************************************************************//**
 * @brief Inotify codes.
 */
typedef struct _code {
  const char *name;
  int value;
} CODE;

static CODE eventnames[] = {
  { "ACCESS", IN_ACCESS },
  { "MODIFY", IN_MODIFY },
  { "ATTRIB", IN_ATTRIB },
  { "CLOSE_WRITE", IN_CLOSE_WRITE },
  { "CLOSE_NOWRITE", IN_CLOSE_NOWRITE },
  { "OPEN", IN_OPEN },
  { "MOVED_FROM", IN_MOVED_FROM },
  { "MOVED_TO", IN_MOVED_TO },
  { "CREATE", IN_CREATE },
  { "DELETE", IN_DELETE },
  { "DELETE_SELF", IN_DELETE_SELF },
  { "MOVE_SELF", IN_MOVE_SELF },
  { "UNMOUNT", IN_UNMOUNT },
  { "Q_OVERFLOW", IN_Q_OVERFLOW },
  { "IGNORED", IN_IGNORED },
  { NULL, -1 }
};

/**************************************************************************//**
 * @brief Add a inotify watch.
 * @param[in,out] params Monitor parameters.
 * @param[in] item Item to monitor.
 * @param[in] freeonerror Free item if inotify fails to monitor it.
 * @return 1=watch added succesfully, 0=otherwise.
 */
static int monitor_add_watch(params_t *params, witem_t *item, bool freeonerror)
{
  assert(params != NULL);
  assert(params->ifd >= 0);
  if (item == NULL) {
    return(0);
  }

  int wd = 0;
  uint32_t mask = 0;

  if (item->type == WITEM_FILE) {
    mask = IN_MODIFY;
  }
  else { // directory
    mask = IN_CREATE|IN_MOVE_SELF|IN_MOVED_FROM|IN_MOVED_TO|IN_EXCL_UNLINK|IN_ONLYDIR;
  }

  wd = inotify_add_watch(params->ifd, item->filename, mask);
  if (wd < 0) {
    syslog(LOG_ALERT, "monitor - failed to monitor %s '%s' - %s",
           (item->type==WITEM_DIR?"directory":"file"), item->filename, strerror(errno));
    if (freeonerror) {
      witem_free(item);
    }
    return(0);
  }
  else {
    // insert item in the table of declared items
    if (!vector_contains(params->witems, item)) {
      vector_insert(params->witems, item);
    }
    // update dictionary for fast retrieval
    map_insert(params->dict, wd, item);
    // notifies to other threads that a new file is available
    if (item->type == WITEM_FILE) {
      mqueue_push(params->queue1, MSG_TYPE_FILE0, item, true, 0);
    }
    // trace
    syslog(LOG_INFO, "monitor - monitoring %s '%s' on WD #%d",
           (item->type==WITEM_DIR?"directory":"file"), item->filename, wd);
    return(1);
  }
}

/**************************************************************************//**
 * @brief Add inotify watches for all files matching a pattern.
 * @param[in,out] params Monitor parameters.
 * @param[in] dir Directory to monitor.
 * @param[in] file File pattern to monitor.
 * @return Number of added watches.
 */
static int monitor_add_dir_pattern(params_t *params, dir_t *dir, file_t *file)
{
  assert(params != NULL);
  assert(dir != NULL);
  assert(file != NULL);

  int rc = 0;
  int num_watches = 0;
  glob_t globbuf = {0};
  char *filename = NULL;

  filename = concat(3, dir->path, "/", file->pattern);
  rc = glob(filename, GLOB_BRACE, NULL, &globbuf);
  if (rc != 0) {
    if (rc != GLOB_NOMATCH) {
      syslog(LOG_ERR, "monitor - %s", strerror(errno));
    }
    goto monitor_add_dir_pattern_exit;
  }

  // adding existing matched files
  for(int i=0; globbuf.gl_pathv[i]!=NULL; i++) {
    const char *realfilename = globbuf.gl_pathv[i];
    int ipos = vector_find(params->witems, realfilename);
    if (ipos >= 0) {
      syslog(LOG_WARNING, "monitor - file '%s' matched twice. Only first match applies", realfilename);
    }
    else {
      if (is_readable_file(realfilename)) {
        witem_t *item = witem_alloc(realfilename, WITEM_FILE, file);
        num_watches += monitor_add_watch(params, item, true);
      }
    }
  }

monitor_add_dir_pattern_exit:
  free(filename);
  globfree(&globbuf);
  return(num_watches);
}

/**************************************************************************//**
 * @brief Add inotify watches on a directory and its patterns.
 * @param[in,out] params Monitor parameters.
 * @param[in] dir Directory to monitor.
 * @return Number of added watches.
 */
static int monitor_add_dir(params_t *params, dir_t *dir)
{
  assert(params != NULL);
  assert(dir != NULL);

  if (!is_readable_dir(dir->path)) {
    return(0);
  }

  int num_watches = 0;

  // adding directory
  witem_t *item = witem_alloc(dir->path, WITEM_DIR, dir);
  num_watches += monitor_add_watch(params, item, true);

  // adding files matching patterns
  if (num_watches > 0) {
    for(size_t i=0; i<dir->files.size; i++) {
      file_t *file = (file_t *) dir->files.data[i];
      num_watches += monitor_add_dir_pattern(params, dir, file);
    }
  }

  return num_watches;
}

/**************************************************************************//**
 * @brief Add inotify watches.
 * @param[in,out] params Monitor parameters.
 * @return Number of added watches.
 */
static int monitor_add_dirs(params_t *params, const vector_t *dirs)
{
  assert(params != NULL);
  assert(dirs != NULL);

  int num_watches = 0;

  for(size_t i=0; i<dirs->size; i++) {
    dir_t *dir = (dir_t *) dirs->data[i];
    num_watches += monitor_add_dir(params, dir);
  }

  return(num_watches);
}

/**************************************************************************//**
 * @brief Remove inotify watch.
 * @param[in,out] params Monitor parameters.
 * @param[in] wd Inotify watch descriptor.
 */
static void monitor_rm_watch(params_t *params, int wd)
{
  assert(params != NULL);
  assert(params->ifd >= 0);
  assert(params->dict != NULL);
  assert(wd >= 0);

  // retrieve watched item
  witem_t *item = (witem_t *) map_find(params->dict, wd);
  if (item == NULL) {
    syslog(LOG_WARNING, "monitor - non-existing witem for WD #%d", wd);
    assert(false);
    return;
  }

  syslog(LOG_INFO, "monitor - stop monitoring %s '%s' on WD #%d",
         (item->type==WITEM_DIR?"directory":"file"), item->filename, wd);

  // remove inotify watch
  inotify_rm_watch(params->ifd, wd);

  // remove item from map
  map_remove(params->dict, wd, NULL);
  // notify that something has changed
  if (item->type == WITEM_FILE) {
    mqueue_push(params->queue1, MSG_TYPE_FILE1, item, true, 0);
  }
}

/**************************************************************************//**
 * @brief Remove inotify watches.
 * @param[in,out] params Monitor parameters.
 */
static void monitor_rm_watches(params_t *params)
{
  assert(params != NULL);
  assert(params->dict != NULL);

  map_bucket_t *bucket = NULL;
  map_iterator_t it = {0};

  while((bucket = map_next(params->dict, &it)) != NULL) {
    monitor_rm_watch(params, bucket->key);
    it.pos = it.pos - (it.pos==0?0:1);
    it.num--;
  }
}

/**************************************************************************//**
 * @brief Trace inotify event info to stderr.
 * @details Use only for debug purposes.
 * @param[in] event Inotify event.
 * @param[in] item Related watched item.
 */
static void trace_event(const struct inotify_event *event, witem_t *item)
{
  assert(event != NULL);

  const char *aux = NULL;
  CODE *code = eventnames;
  while(1) {
    if (code->name == NULL) {
      break;
    }
    if (event->mask & code->value) {
      aux = code->name;
      break;
    }
    code++;
  }

  const char *ecode = (aux==NULL?"UNKNOW":aux);
  const char *ename = (event->len==0?"NULL":event->name);
  const char *iname = (item==NULL?"NULL":item->filename);

  syslog(LOG_DEBUG, "monitor - event = [code=%s, name=%s, wd=%d, item=%s]", ecode, ename, event->wd, iname);
}

/**************************************************************************//**
 * @brief Process an inotify event.
 * @param[in,out] params Monitor parameters.
 * @param[in] event Event to process.
 * @param[in] item Watched item.
 */
static void process_event_file(params_t *params, const struct inotify_event *event, witem_t *item)
{
  if (event->mask & IN_MODIFY) {
    mqueue_push(params->queue1, MSG_TYPE_FILE0, item, true, 0);
  }
}

/**************************************************************************//**
 * @brief Process IN_CREATE event on a directory.
 * @param[in] params Monitor parameters.
 * @param[in] dir Directory where event ocurres.
 * @param[in] name File name (not a pattern).
 */
static void process_event_dir_create(params_t *params, dir_t *dir, const char *name)
{
  // checks if dir patterns match file name
  int ipos = dir_file_match(dir, name);
  if (ipos < 0) {
    return;
  }

  file_t *file = dir->files.data[ipos];
  char *filename = concat(3, dir->path, "/", name);

  // if witem was previously declared ...
  ipos = vector_find(params->witems, filename);
  if (ipos >= 0) {
    syslog(LOG_DEBUG, "monitor - file '%s' was previously declared", filename);
    witem_t *item = (witem_t *) params->witems->data[ipos];
    assert(item->type == WITEM_FILE);
    assert(item->ptr == file);
    monitor_add_watch(params, item, false);
    goto process_event_dir_create_exit;
  }

  // create a new witem and monitor it
  if (is_readable_file(filename)) {
    witem_t *item = witem_alloc(filename, WITEM_FILE, file);
    monitor_add_watch(params, item, true);
  }
  else {
    syslog(LOG_INFO, "monitor - '%s' is not a readable file", filename);
  }

process_event_dir_create_exit:
  free(filename);
}

/**************************************************************************//**
 * @brief Process IN_MOVED_FROM event on a directory.
 * @param[in] params Monitor parameters.
 * @param[in] dir Directory where event ocurres.
 * @param[in] name File name (not a pattern).
 */
static void process_event_dir_moved_from(params_t *params, dir_t *dir, const char *name)
{
  char *filename = concat(3, dir->path, "/", name);
  map_bucket_t *bucket = NULL;
  map_iterator_t it = {0};

  while((bucket = map_next(params->dict, &it)) != NULL) {
    witem_t *item = (witem_t *) bucket->value;
    assert(item != NULL);
    assert(item->filename != NULL);
    if (item != NULL && item->filename != NULL && strcmp(filename, item->filename) == 0) {
      monitor_rm_watch(params, bucket->key);
      break;
    }
  }

  free(filename);
}

/**************************************************************************//**
 * @brief Process IN_MOVE_SELF event on a directory.
 * @details Removes all watched entries starting by directory path.
 * @param[in] params Monitor parameters.
 * @param[in] wd Watch descriptor of current event.
 * @param[in] dir Directory where event ocurres.
 */
static void process_event_dir_move_self(params_t *params, int wd, dir_t *dir)
{
  char *path = concat(2, dir->path, "/");
  map_bucket_t *bucket = NULL;
  map_iterator_t it = {0};

  // removes watch from directory
  monitor_rm_watch(params, wd);

  // removes watch for all watched files in directory
  while((bucket = map_next(params->dict, &it)) != NULL) {
    witem_t *item = (witem_t *) bucket->value;
    assert(item != NULL);
    assert(item->filename != NULL);
    if (item != NULL && item->filename != NULL && starts_with(path, item->filename)) {
      monitor_rm_watch(params, bucket->key);
      it.pos = it.pos - (it.pos==0?0:1);
      it.num--;
    }
  }

  free(path);
}

/**************************************************************************//**
 * @brief Process an inotify event.
 * @param[in] params Monitor parameters.
 * @param[in] event Event to process.
 * @param[in] item Event watched item.
 */
static void process_event_dir(params_t *params, const struct inotify_event *event, witem_t *item)
{
  // new file in this directory
  if (event->mask & IN_CREATE) {
    const char *name = event->name;
    process_event_dir_create(params, item->ptr, name);
    return;
  }

  // file renamed or moved from this directory
  if (event->mask & IN_MOVED_FROM) {
    const char *name = event->name;
    process_event_dir_moved_from(params, item->ptr, name);
    return;
  }

  // file renamed or moved to this directory
  if (event->mask & IN_MOVED_TO) {
    const char *name = event->name;
    process_event_dir_create(params, item->ptr, name);
    return;
  }

  // current directory renamed or moved
  if (event->mask & IN_MOVE_SELF) {
    // removes directory watch and all watched items inside this directory
    // nobody cares if directory is re-created
    process_event_dir_move_self(params, event->wd, item->ptr);
    return;
  }
}

/**************************************************************************//**
 * @brief Process an inotify event.
 * @param[in] params Monitor parameters.
 * @param[in] event Event to process.
 */
static void process_event(params_t *params, const struct inotify_event *event)
{
  assert(event != NULL);

  if (event == NULL || event->wd < 0 || params->dict == NULL) {
    assert(false);
    return;
  }

  witem_t *item = map_find(params->dict, event->wd);
  trace_event(event, item);

  if (item == NULL) {
    return;
  }

  if (event->mask & IN_IGNORED) {
    monitor_rm_watch(params, event->wd);
  }
  else if (item->type == WITEM_FILE) {
    process_event_file(params, event, item);
  }
  else {
    process_event_dir(params, event, item);
  }
}

/**************************************************************************//**
 * @brief Reset params linked to monitor.
 * @param[in,out] params Monitor parameters.
 * @return 0=OK, otherwise=KO.
 */
static void monitor_reset(params_t *params)
{
  if (params == NULL || params->witems == NULL || params->dict == NULL || params->queue1 == NULL) {
    assert(false);
    return;
  }

  if (params->ifd >= 0) {
    // close dirs and files
    monitor_rm_watches(params);
    // stops inotify
    close(params->ifd);
    syslog(LOG_DEBUG, "monitor - inotify stopped");
    params->ifd = -1;
  }

  map_reset(params->dict, NULL);
}

/**************************************************************************//**
 * @brief Initialize inotify.
 * @param[in] dirs User defined dir/patterns declared in config file.
 * @param[in,out] params Monitor parameters.
 * @return 0=OK, otherwise=KO.
 */
int monitor_init(const vector_t *dirs, params_t *params)
{
  if (dirs == NULL || params == NULL ||
      params->ifd >= 0 || params->witems == NULL || params->dict == NULL || params->queue1 == NULL ||
      params->witems->size > 0 || params->dict->size > 0 || params->queue1->capacity == 0) {
    assert(false);
    return(1);
  }

  int rc = 0;

  params->ifd = inotify_init();
  if (params->ifd < 0) {
    syslog(LOG_CRIT, "monitor - %s", strerror(errno));
    rc = EXIT_FAILURE;
    goto monitor_init_err;
  }
  syslog(LOG_DEBUG, "monitor - inotify started");

  // add files declared in config file
  monitor_add_dirs(params, dirs);
  if (params->dict->size == 0) {
    syslog(LOG_ERR, "monitor - no items to monitor");
    rc = EXIT_FAILURE;
    goto monitor_init_err;
  }

  return(EXIT_SUCCESS);

monitor_init_err:
  monitor_reset(params);
  return(rc);
}

/**************************************************************************//**
 * @brief Process inotify events.
 * @details This function block the current thread until signal is received.
 *          Inotify events are processed in-the-fly. Heavy tasks (like
 *          file-read, database-send, etc.) are made by another thread.
 * @param[in] ptr Monitor parameters.
 */
void *monitor_run(void *ptr)
{
  params_t *params = (params_t *) ptr;
  if (params == NULL || params->ifd < 0 || params->witems == NULL || params->dict == NULL || params->queue1 == NULL) {
    assert(false);
    return(NULL);
  }

  // this thread receives signals that interrupts read() with EINTR
  sigset_t signals_to_catch = {0};
  sigaddset(&signals_to_catch, SIGINT);
  sigaddset(&signals_to_catch, SIGABRT);
  sigaddset(&signals_to_catch, SIGTERM);
  pthread_sigmask(SIG_UNBLOCK, &signals_to_catch, NULL);

  char buffer[BUFFER_LEN] ALIGNAS(struct inotify_event);
  const struct inotify_event *event = NULL;

  syslog(LOG_DEBUG, "monitor - thread started");

  while(params->dict->size > 0 && keep_running)
  {
    // wait for inotify events
    ssize_t len = read(params->ifd, buffer, sizeof(buffer));
    if (len < 0) {
      if (errno != EINTR) {
        syslog(LOG_ERR, "monitor - %s", strerror(errno));
      }
      break;
    }

    // process the list of readed events
    for (char *ptr=buffer; ptr<buffer+len; ptr += sizeof(struct inotify_event)+event->len) {
      event = (const struct inotify_event *) ptr;
      if (event->wd < 0 || event->mask & IN_Q_OVERFLOW) {
        continue;
      }
      process_event(params, event);
    }
  }

  // notifies that there are no more messages
  mqueue_close(params->queue1);
  // release structs used uniquely by monitor
  monitor_reset(params);

  syslog(LOG_DEBUG, "monitor - thread ended");
  return(NULL);
}
