
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
#include "utils.h"
#include "monitor.h"

#ifdef __GNUC__
#  define ALIGNAS(TYPE) __attribute__ ((aligned(__alignof__(TYPE))))
#else
#  define ALIGNAS(TYPE) /* empty */
#endif

#define EVENT_SIZE (sizeof(struct inotify_event) + NAME_MAX + 1)
#define BUFFER_LEN (256*(EVENT_SIZE))

/**************************************************************************//**
 * @brief Inotify codes.
 */
typedef struct code_t {
  const char *name;
  int value;
} code_t;

static code_t eventnames[] = {
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
 * @param[in,out] monitor Monitor parameters.
 * @param[in] item Item to monitor.
 * @param[in] freeonerror Free item if inotify fails to monitor it.
 * @return 1=watch added succesfully, 0=otherwise.
 */
static int monitor_add_watch(monitor_t *monitor, witem_t *item, bool freeonerror)
{
  assert(monitor != NULL);
  assert(monitor->ifd > 0);
  if (item == NULL) {
    return(0);
  }

  int wd = 0;
  uint32_t mask = 0;

  if (item->type == WITEM_FILE) {
    mask = IN_MODIFY;
  }
  else { // directory
    mask = IN_CREATE|IN_MOVE_SELF|IN_MOVED_FROM|IN_MOVED_TO|IN_EXCL_UNLINK|IN_ONLYDIR|IN_DELETE;
  }

  wd = inotify_add_watch(monitor->ifd, item->filename, mask);
  if (wd < 0) {
    syslog(LOG_ALERT, "monitor - failed to monitor %s '%s' - %s",
           (item->type==WITEM_DIR?"directory":"file"), item->filename, strerror(errno));
    if (freeonerror) {
      witem_free(item);
    }
    return(0);
  }
  else {
    item->wd = wd;
    // update dictionaries for fast retrieval
    map_int_insert(&(monitor->dict1), item->wd, item);
    map_str_insert(&(monitor->dict2), item->filename, item);
    // notifies to other threads that a new file is available
    if (item->type == WITEM_FILE) {
      mqueue_push(monitor->mqueue, MSG_TYPE_FILE0, item, true, 0);
    }
    // trace
    syslog(LOG_INFO, "monitor - monitoring %s '%s' on WD #%d",
           (item->type==WITEM_DIR?"directory":"file"), item->filename, wd);
    return(1);
  }
}

/**************************************************************************//**
 * @brief Add inotify watches for all files matching a pattern.
 * @param[in,out] monitor Monitor parameters.
 * @param[in] dir Directory to monitor.
 * @param[in] file File pattern to monitor.
 * @return Number of added watches.
 */
static int monitor_add_dir_pattern(monitor_t *monitor, dir_t *dir, file_t *file)
{
  assert(monitor != NULL);
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

  // adding matched files
  for(int i=0; globbuf.gl_pathv[i]!=NULL; i++)
  {
    const char *realfilename = globbuf.gl_pathv[i];

    witem_t *item = map_str_find(&(monitor->dict2), realfilename);
    if (item != NULL) {
      syslog(LOG_WARNING, "monitor - file '%s' matched twice. Only first match applies", realfilename);
      continue;
    }

    if (is_readable_file(realfilename)) {
      item = witem_alloc(realfilename, WITEM_FILE, file, monitor->seek0);
      num_watches += monitor_add_watch(monitor, item, true);
    }
    else {
      syslog(LOG_WARNING, "monitor - cannot access file %s", realfilename);
    }
  }

monitor_add_dir_pattern_exit:
  free(filename);
  globfree(&globbuf);
  return(num_watches);
}

/**************************************************************************//**
 * @brief Add inotify watches on a directory and its patterns.
 * @param[in,out] monitor Monitor parameters.
 * @param[in] dir Directory to monitor.
 * @return Number of added watches.
 */
static int monitor_add_dir(monitor_t *monitor, dir_t *dir)
{
  assert(monitor != NULL);
  assert(dir != NULL);

  if (!is_readable_dir(dir->path)) {
    syslog(LOG_WARNING, "monitor - cannot access directory %s", dir->path);
    return(0);
  }

  int num_watches = 0;

  // adding directory
  witem_t *item = witem_alloc(dir->path, WITEM_DIR, dir, monitor->seek0);
  num_watches += monitor_add_watch(monitor, item, true);

  // adding files matching patterns
  if (num_watches > 0) {
    for(uint32_t i=0; i<dir->files.size; i++) {
      file_t *file = (file_t *) dir->files.data[i];
      num_watches += monitor_add_dir_pattern(monitor, dir, file);
    }
  }

  return num_watches;
}

/**************************************************************************//**
 * @brief Add inotify watches.
 * @param[in,out] monitor Monitor parameters.
 * @return Number of added watches.
 */
static int monitor_add_dirs(monitor_t *monitor, const vector_t *dirs)
{
  assert(monitor != NULL);
  assert(dirs != NULL);

  int num_watches = 0;

  for(uint32_t i=0; i<dirs->size; i++) {
    dir_t *dir = (dir_t *) dirs->data[i];
    num_watches += monitor_add_dir(monitor, dir);
  }

  return(num_watches);
}

/**************************************************************************//**
 * @brief Remove inotify watch.
 * @param[in,out] monitor Monitor parameters.
 * @param[in] wd Inotify watch descriptor.
 */
static void monitor_rm_watch(monitor_t *monitor, int wd)
{
  assert(monitor != NULL);
  assert(monitor->ifd > 0);
  assert(wd >= 0);

  // retrieve watched item
  witem_t *item = (witem_t *) map_int_find(&(monitor->dict1), wd);
  if (item == NULL) {
    syslog(LOG_WARNING, "monitor - non-existing witem for WD #%d", wd);
    assert(false);
    return;
  }

  syslog(LOG_INFO, "monitor - stop monitoring %s '%s' on WD #%d",
         (item->type==WITEM_DIR?"directory":"file"), item->filename, wd);

  // remove inotify watch
  inotify_rm_watch(monitor->ifd, wd);

  // remove item from map
  map_int_remove(&(monitor->dict1), item->wd, NULL);
  map_str_remove(&(monitor->dict2), item->filename, NULL);

  // notify that something has changed
  if (item->type == WITEM_FILE && monitor->mqueue->open) {
    mqueue_push(monitor->mqueue, MSG_TYPE_FILE1, item, true, 0);
  }
  else {
    witem_free(item);
  }
}

/**************************************************************************//**
 * @brief Remove inotify watches.
 * @param[in,out] monitor Monitor parameters.
 */
static void monitor_rm_watches(monitor_t *monitor)
{
  assert(monitor != NULL);

  map_int_bucket_t *bucket = NULL;
  map_int_iterator_t it = {0};

  while((bucket = map_int_next(&(monitor->dict1), &it)) != NULL) {
    monitor_rm_watch(monitor, bucket->key);
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
  code_t *code = eventnames;
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
 * @param[in,out] monitor Monitor parameters.
 * @param[in] event Event to process.
 * @param[in] item Watched item.
 */
static void process_event_file(monitor_t *monitor, const struct inotify_event *event, witem_t *item)
{
  if (event->mask & IN_MODIFY) {
    mqueue_push(monitor->mqueue, MSG_TYPE_FILE0, item, true, 0);
  }
}

/**************************************************************************//**
 * @brief Process IN_CREATE event on a directory.
 * @param[in] monitor Monitor parameters.
 * @param[in] dir Directory where event ocurres.
 * @param[in] name File name (not a pattern).
 */
static void process_event_dir_create(monitor_t *monitor, dir_t *dir, const char *name)
{
  // checks if dir patterns match file name
  int ipos = dir_file_match(dir, name);
  if (ipos < 0) {
    return;
  }

  file_t *file = dir->files.data[ipos];
  char *filename = concat(3, dir->path, "/", name);

  // create a new witem and monitor it
  if (is_readable_file(filename)) {
    witem_t *item = witem_alloc(filename, WITEM_FILE, file, monitor->seek0);
    monitor_add_watch(monitor, item, true);
  }
  else {
    syslog(LOG_INFO, "monitor - '%s' is not a readable file", filename);
  }

  free(filename);
}

/**************************************************************************//**
 * @brief Process IN_DELETE|IN_MOVED_FROM event on a directory.
 * @param[in] monitor Monitor parameters.
 * @param[in] dir Directory where event ocurres.
 * @param[in] name File name (not a pattern).
 */
static void process_event_dir_delete(monitor_t *monitor, dir_t *dir, const char *name)
{
  char *filename = concat(3, dir->path, "/", name);
  witem_t *item = map_str_find(&(monitor->dict2), filename);
  if (item != NULL) {
    monitor_rm_watch(monitor, item->wd);
  }
  free(filename);
}

/**************************************************************************//**
 * @brief Process IN_MOVE_SELF event on a directory.
 * @details Removes all watched entries starting by directory path.
 * @param[in] monitor Monitor parameters.
 * @param[in] wd Watch descriptor of current event.
 * @param[in] dir Directory where event ocurres.
 */
static void process_event_dir_move_self(monitor_t *monitor, int wd, dir_t *dir)
{
  char *path = concat(2, dir->path, "/");
  map_int_bucket_t *bucket = NULL;
  map_int_iterator_t it = {0};

  // removes watch from directory
  monitor_rm_watch(monitor, wd);

  // removes watch for all watched files in directory
  while((bucket = map_int_next(&(monitor->dict1), &it)) != NULL) {
    witem_t *item = (witem_t *) bucket->value;
    if (item == NULL || item->filename == NULL) {
      assert(false);
      continue;
    }
    if (starts_with(path, item->filename)) {
      monitor_rm_watch(monitor, bucket->key);
      it.pos = it.pos - (it.pos==0?0:1);
      it.num--;
    }
  }

  free(path);
}

/**************************************************************************//**
 * @brief Process an inotify event.
 * @param[in] monitor Monitor parameters.
 * @param[in] event Event to process.
 * @param[in] item Event watched item.
 */
static void process_event_dir(monitor_t *monitor, const struct inotify_event *event, witem_t *item)
{
  // new file in this directory
  if (event->mask & IN_CREATE) {
    const char *name = event->name;
    process_event_dir_create(monitor, item->ptr, name);
    return;
  }

  // file renamed or moved from this directory
  if (event->mask & IN_MOVED_FROM) {
    const char *name = event->name;
    process_event_dir_delete(monitor, item->ptr, name);
    return;
  }

  // file renamed or moved to this directory
  if (event->mask & IN_MOVED_TO) {
    const char *name = event->name;
    process_event_dir_create(monitor, item->ptr, name);
    return;
  }

  // file removed from this directory
  if (event->mask & IN_DELETE) {
    const char *name = event->name;
    process_event_dir_delete(monitor, item->ptr, name);
    return;
  }

  // current directory renamed or moved
  if (event->mask & IN_MOVE_SELF) {
    // removes directory watch and all watched items inside this directory
    // nobody cares if directory is re-created
    process_event_dir_move_self(monitor, event->wd, item->ptr);
    return;
  }
}

/**************************************************************************//**
 * @brief Process an inotify event.
 * @param[in] monitor Monitor parameters.
 * @param[in] event Event to process.
 */
static void process_event(monitor_t *monitor, const struct inotify_event *event)
{
  if (event == NULL || event->wd < 0) {
    assert(false);
    return;
  }

  witem_t *item = map_int_find(&(monitor->dict1), event->wd);
  trace_event(event, item);

  if (item == NULL) {
    return;
  }

  if (event->mask & IN_IGNORED) {
    monitor_rm_watch(monitor, event->wd);
  }
  else if (item->type == WITEM_FILE) {
    process_event_file(monitor, event, item);
  }
  else {
    process_event_dir(monitor, event, item);
  }
}

/**************************************************************************//**
 * @brief Reset params linked to monitor.
 * @param[in,out] monitor Monitor parameters.
 * @return 0=OK, otherwise=KO.
 */
void monitor_reset(monitor_t *monitor)
{
  if (monitor == NULL) {
    assert(false);
    return;
  }

  if (monitor->ifd > 0) {
    // close dirs and files
    monitor_rm_watches(monitor);
    // stops inotify
    close(monitor->ifd);
    syslog(LOG_DEBUG, "monitor - inotify stopped");
  }

  monitor->ifd = 0;
  monitor->mqueue = NULL;
  map_int_reset(&(monitor->dict1), witem_free);
  map_str_reset(&(monitor->dict2), NULL);
}

/**************************************************************************//**
 * @brief Initialize inotify.
 * @param[in,out] monitor Monitor parameters.
 * @param[in] dirs User defined dir/patterns declared in config file.
 * @param[in] mqueue Message queue (monitor -> processor).
 * @param[in] seek0 Open files position.
 * @return 0=OK, otherwise=KO.
 */
int monitor_init(monitor_t *monitor, const vector_t *dirs, mqueue_t *mqueue, bool seek0)
{
  if (monitor == NULL || dirs == NULL || mqueue == NULL) {
    assert(false);
    return(1);
  }

  int rc = 0;

  monitor->dict1 = (map_int_t){0};
  monitor->dict2 = (map_str_t){0};
  monitor->mqueue = mqueue;
  monitor->seek0 = seek0;

  monitor->ifd = inotify_init();
  if (monitor->ifd <= 0) {
    syslog(LOG_CRIT, "monitor - %s", strerror(errno));
    rc = EXIT_FAILURE;
    goto monitor_init_err;
  }
  syslog(LOG_DEBUG, "monitor - inotify started [fd=%d]", monitor->ifd);

  // add files declared in config file
  monitor_add_dirs(monitor, dirs);
  if (monitor->dict1.size == 0) {
    syslog(LOG_ERR, "monitor - no items to monitor");
    rc = EXIT_FAILURE;
    goto monitor_init_err;
  }

  return(EXIT_SUCCESS);

monitor_init_err:
  monitor_reset(monitor);
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
  monitor_t *monitor = (monitor_t *) ptr;
  if (monitor == NULL || monitor->ifd <= 0 || monitor->mqueue == NULL) {
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

  while(monitor->dict1.size > 0 && keep_running)
  {
    // wait for inotify events
    ssize_t len = read(monitor->ifd, buffer, sizeof(buffer));
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
      process_event(monitor, event);
    }
  }

  // notifies that there are no more messages
  mqueue_close(monitor->mqueue);

  syslog(LOG_DEBUG, "monitor - thread ended");
  return(NULL);
}
