
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
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <limits.h>
#include <syslog.h>
#include <assert.h>
#include "utils.h"
#include "mqueue.h"

#define INITIAL_CAPACITY 8
#define RESIZE_FACTOR 2

/**************************************************************************//**
 * @brief Creates a message.
 * @param[in] type Message type.
 * @param[in] data Message data.
 * @return A message with the given parameters.
 */
msg_t msg_create(short type, void *data)
{
  msg_t ret;
  ret.type = type;
  ret.data = data;
  return(ret);
}

/**************************************************************************//**
 * @brief Converts type to string.
 * @param[in] type Message type.
 * @return String representing the type.
 */
const char* msg_type_str(short type)
{
  switch(type)
  {
    case MSG_TYPE_NULL:
      return "NULL";
    case MSG_TYPE_ERROR:
      return "ERROR";
    case MSG_TYPE_EINTR:
      return "EINTR";
    case MSG_TYPE_CLOSE:
      return "CLOSE";
    case MSG_TYPE_TIMEOUT:
      return "TIMEOUT";
    case MSG_TYPE_EXISTS:
      return "EXISTS";
    case  MSG_TYPE_FILE0:
      return "FILE0";
    case MSG_TYPE_FILE1:
      return "FILE1";
    case MSG_TYPE_MATCH1:
      return "MATCH1";
    default:
      return "UNKNOW";
  }
}

/**************************************************************************//**
 * @brief Return the number of elements in the mqueue.
 * @details Caution, this function is not thread-safe.
 * @param[in] mqueue Message queue object.
 * @return Number of elements.
 */
static size_t mqueue_size(const mqueue_t *mqueue)
{
  if (mqueue->status == MQUEUE_STATUS_EMPTY) {
    return(0);
  }

  int ret = (int)(mqueue->pos2) - (int)(mqueue->pos1) + 1;
  if (ret <= 0) {
    ret += mqueue->capacity;
  }

  assert(ret >= 0);
  assert((size_t)(ret) <= mqueue->capacity);
  return(ret);
}

/**************************************************************************//**
 * @brief Initialize a mqueue struct.
 * @param[in,out] mqueue Message queue object.
 * @param[in] name Message queue name (used in traces).
 * @param[in] max_capacity Maximum queue size (0=unlimited).
 * @return 0=OK, otherwise an error ocurred.
 */
int mqueue_init(mqueue_t *mqueue, const char *name, size_t max_capacity)
{
  if (mqueue == NULL || name == NULL) {
    assert(false);
    return(1);
  }

  int rc = 0;

  mqueue->buffer = NULL;
  mqueue->capacity = 0;
  mqueue->pos1 = 0;
  mqueue->pos2 = 0;
  mqueue->max_capacity = max_capacity;
  mqueue->status = MQUEUE_STATUS_UNINITIALIZED;
  mqueue->name = strdup(name);

  if (mqueue->name == NULL) {
    rc = 1;
    goto mqueue_init_err1;
  }

  rc = pthread_mutex_init(&(mqueue->mutex), NULL);
  if (rc != 0) {
    goto mqueue_init_err2;
  }

  rc = pthread_cond_init(&(mqueue->tcond1), NULL);
  if (rc != 0) {
    goto mqueue_init_err3;
  }

  rc = pthread_cond_init(&(mqueue->tcond2), NULL);
  if (rc != 0) {
    goto mqueue_init_err4;
  }

  size_t initial_capacity = (max_capacity==0?INITIAL_CAPACITY:MIN(max_capacity, INITIAL_CAPACITY));
  mqueue->buffer = (msg_t *) calloc(initial_capacity, sizeof(msg_t));
  if (mqueue->buffer == NULL) {
    rc = 1;
    goto mqueue_init_err5;
  }

  mqueue->pos2 = initial_capacity-1;
  mqueue->capacity = initial_capacity;
  mqueue->status = MQUEUE_STATUS_EMPTY;
  syslog(LOG_DEBUG, "mqueue - %s initialized", mqueue->name);
  return(0);

mqueue_init_err5:
  pthread_cond_destroy(&(mqueue->tcond2));
mqueue_init_err4:
  pthread_cond_destroy(&(mqueue->tcond1));
mqueue_init_err3:
  pthread_mutex_destroy(&(mqueue->mutex));
mqueue_init_err2:
  free(mqueue->name);
mqueue_init_err1:
  return(rc);
}

/**************************************************************************//**
 * @brief Reset a mqueue struct.
 * @param[in,out] mqueue Message queue object.
 * @param[in] item_free Function to free an item (can be NULL).
 */
void mqueue_reset(mqueue_t *mqueue, void (*item_free)(void*))
{
  if (mqueue == NULL || mqueue->status == MQUEUE_STATUS_UNINITIALIZED) {
    return;
  }
  // free objects (only if item_free not NULL)
  if (mqueue->buffer != NULL && item_free != NULL) {
    size_t len = mqueue_size(mqueue);
    for(size_t i=0; i<=len; i++) {
      size_t pos = (mqueue->pos1+i)%(mqueue->capacity);
      item_free(mqueue->buffer[pos].data);
      mqueue->buffer[pos].data = NULL;
    }
  }
  free(mqueue->buffer);
  mqueue->buffer = NULL;
  mqueue->capacity = 0;
  mqueue->pos1 = 0;
  mqueue->pos2 = 0;
  mqueue->max_capacity = 0;
  mqueue->status = MQUEUE_STATUS_UNINITIALIZED;
  pthread_cond_destroy(&(mqueue->tcond1));
  pthread_cond_destroy(&(mqueue->tcond2));
  pthread_mutex_destroy(&(mqueue->mutex));
  syslog(LOG_DEBUG, "mqueue - %s reseted", mqueue->name);
  free(mqueue->name);
}

/**************************************************************************//**
 * @brief Increase the mqueue size reallocating elements.
 * @details Caution, this function is not thread-safe.
 * @param[in,out] mqueue Message queue object.
 * @return 0=OK, 1=KO.
 */
static int mqueue_resize(mqueue_t *mqueue)
{
  assert(mqueue != NULL);
  assert(mqueue->buffer != NULL);

  size_t new_capacity = RESIZE_FACTOR*(mqueue->capacity);
  if (mqueue->max_capacity > 0 && new_capacity > mqueue->max_capacity) {
    new_capacity = mqueue->max_capacity;
  }
  if (mqueue->capacity >= new_capacity) {
    return(1);
  }

  msg_t *new_buffer = (msg_t *) calloc(new_capacity, sizeof(msg_t));
  if (new_buffer == NULL) {
    return(1);
  }

  size_t len = mqueue_size(mqueue);
  for(size_t i=0; i<len; i++) {
    size_t pos = (mqueue->pos1+i)%(mqueue->capacity);
    new_buffer[i] = mqueue->buffer[pos];
  }

  syslog(LOG_DEBUG, "mqueue - %s resized [%zu -> %zu]", mqueue->name, mqueue->capacity, new_capacity);

  mqueue->pos1 = 0;
  mqueue->pos2 = len-1;
  free(mqueue->buffer);
  mqueue->buffer = new_buffer;
  mqueue->capacity = new_capacity;

  return(0);
}

/**
 * @brief Returns current time + millis
 * @param millis Milliseconds to add to current time.
 * @return timespec struct filled.
 */
static struct timespec mqueue_timespec(size_t millis)
{
  struct timespec ts = {0};

  clock_gettime(CLOCK_REALTIME, &ts);

  size_t seconds = millis/1000;
  ts.tv_sec += seconds;
  ts.tv_nsec += (millis-seconds*1000)*1000000;
  if (ts.tv_nsec >= 1000000000) {
    ts.tv_sec += 1;
    ts.tv_nsec -= 1000000000;
  }

  return ts;
}

/**************************************************************************//**
 * @brief Waits until condition unlocks.
 * @param[in,out] tcond Condition variable.
 * @param[in,out] mutex Mutex blocking read/write.
 * @param[in] ts Time to stop (NULL = non timedout).
 * @return 0 = mutex unlocked,
 *         MSG_TYPE_EINTR = signal interrupted wait,
 *         MSG_TYPE_TIMEOUT = wait timedout (not appended),
 *         MSG_TYPE_ERROR = error (not enought memory, not initialized, etc).
 */
static int mqueue_cond_wait(pthread_cond_t *tcond, pthread_mutex_t *mutex, struct timespec *ts)
{
  int rc = 0;

  if (ts == NULL) {
    rc = pthread_cond_wait(tcond, mutex);
  }
  else {
    rc = pthread_cond_timedwait(tcond, mutex, ts);
  }

  if (rc == 0) {
    // nothing to do
  }
  else if (rc == ETIMEDOUT) {
    rc = MSG_TYPE_TIMEOUT;
  }
  else {
    rc = MSG_TYPE_ERROR;
  }

  return rc;
}

/**************************************************************************//**
 * @brief Update messages type having the given data.
 * @details Caution, this function is not thread-safe.
 * @param[in,out] mqueue Message queue object.
 * @param[in] obj Object to update.
 * @param[in] type New message type.
 * @return Number of updated messages.
 */
static int mqueue_update_msg_type(const mqueue_t *mqueue, void *obj, short type)
{
  int ret = 0;
  size_t len = mqueue_size(mqueue);

  for(size_t i=0; i<=len; i++)
  {
    size_t pos = (mqueue->pos1+i)%(mqueue->capacity);
    if (mqueue->buffer[pos].data == obj) {
      mqueue->buffer[pos].type = type;
      ret++;
    }
  }

  return(ret);
}

/**************************************************************************//**
 * @brief Append an object to the message queue if not exists.
 * @details Assumes that this function is called uniquely by publisher.
 * @details If message queue is full, waits until one slot becomes free.
 * @param[in,out] mqueue Message queue object.
 * @param[in] type Message type to insert.
 * @param[in] obj Object to insert (can be NULL).
 * @param[in] unique Avoid duplicates.
 *         true = not insert if exist (but updates message type),
 *         false = can be repeated elements.
 * @param[in] millis Milliseconds to wait before return (0=waits forever).
 * @return 0 = obj appended,
 *         MSG_TYPE_EINTR = signal interrupted wait (not appended),
 *         MSG_TYPE_TIMEOUT = wait timedout (not appended),
 *         MSG_TYPE_ERROR = error (not enought memory, not initialized, etc),
 *         MSG_TYPE_EXISTS = obj exists -only if unique = true- (not appended),
 *         MSG_TYPE_CLOSE = message queue closed (not appended),
 *         otherwise = error (signal received, not enought memory, not initialized, etc).
 */
static inline int mqueue_push_int(mqueue_t *mqueue, short type, void *obj, bool unique, size_t millis)
{
  if (mqueue == NULL || mqueue->status == MQUEUE_STATUS_UNINITIALIZED) {
    assert(false);
    return(MSG_TYPE_ERROR);
  }
  else if (mqueue->status == MQUEUE_STATUS_CLOSED) {
    return(MSG_TYPE_CLOSE);
  }

  int rc = 0;
  struct timespec ts = {0};
  struct timespec *pts = NULL;

  if (millis > 0) {
    ts = mqueue_timespec(millis);
    pts = &ts;
  }

  pthread_mutex_lock(&(mqueue->mutex));
  while (mqueue->max_capacity > 0 && mqueue_size(mqueue) >= mqueue->max_capacity &&
         mqueue->status != MQUEUE_STATUS_CLOSED && rc == 0) {
    rc = mqueue_cond_wait(&(mqueue->tcond2), &(mqueue->mutex), pts);
  }

  if (rc != 0) {
    goto mqueue_push_exit;
  }
  else if (mqueue->status == MQUEUE_STATUS_CLOSED) {
    rc = MSG_TYPE_CLOSE;
    goto mqueue_push_exit;
  }

  // check if obj exist in mqueue
  if (unique && mqueue_update_msg_type(mqueue, obj, type) > 0) {
    rc = MSG_TYPE_EXISTS;
    goto mqueue_push_exit;
  }

  // if mqueue is full then increase capacity
  if ((mqueue->pos2+1)%(mqueue->capacity) == mqueue->pos1) {
    rc = mqueue_resize(mqueue);
    if (rc != 0) {
      rc = MSG_TYPE_ERROR;
      goto mqueue_push_exit;
    }
  }

  // add obj to mqueue
  if (mqueue->status == MQUEUE_STATUS_EMPTY) {
    mqueue->pos1 = 0;
    mqueue->pos2 = 0;
    mqueue->buffer[0].type = type;
    mqueue->buffer[0].data = obj;
    mqueue->status = MQUEUE_STATUS_NOTEMPTY;
  }
  else {
    mqueue->pos2 = (mqueue->pos2+1)%(mqueue->capacity);
    mqueue->buffer[mqueue->pos2].type = type;
    mqueue->buffer[mqueue->pos2].data = obj;
  }

  rc = 0;

mqueue_push_exit:
  pthread_cond_signal(&(mqueue->tcond1));
  pthread_mutex_unlock(&(mqueue->mutex));
  return(rc);
}

/**************************************************************************//**
 * @see mqueue_push_int
 * @details Adds debug trace to mqueue_push_int.
 */
int mqueue_push(mqueue_t *mqueue, short type, void *obj, bool unique, size_t millis)
{
  struct timeval t1 = {0};
  gettimeofday(&t1, NULL);

  int rc = mqueue_push_int(mqueue, type, obj, unique, millis);

  struct timeval t2 = {0};
  gettimeofday(&t2, NULL);

  syslog(LOG_DEBUG, "mqueue - %s.push(%s, %p, %s, %zu) = %s, etime = %.3lf sec",
         mqueue->name,
         msg_type_str(type),
         obj,
         (unique?"true":"false"),
         millis,
         (rc==0?"OK":msg_type_str(rc)),
         difftimeval(&t1, &t2));

  return rc;
}

/**************************************************************************//**
 * @brief Retrieves an object from mqueue.
 * @details Assumes that this function is called uniquely by subscriber.
 * @details Waits until an object is available or timeout is elapsed.
 * @param[in,out] mqueue Message queue object.
 * @param[in] millis Milliseconds to wait before return (0=waits forever).
 * @return First message in queue.
 *         If error then data=NULL and type contains error code :
 *         MQUEUE_NULL = retrieved item can be ignored (is NULL),
 *         MQUEUE_ERROR = an error ocurred,
 *         MQUEUE_EINTR = wait interrupted by signal,
 *         MQUEUE_CLOSE = consumer closes,
 *         MQUEUE_TIMEOUT = wait has timeouted.
 */
static inline msg_t mqueue_pop_int(mqueue_t *mqueue, size_t millis)
{
  if (mqueue == NULL || mqueue->status == MQUEUE_STATUS_UNINITIALIZED) {
    assert(false);
    return(msg_create(MSG_TYPE_ERROR, NULL));
  }
  else if (mqueue->status == MQUEUE_STATUS_CLOSED) {
    return(msg_create(MSG_TYPE_CLOSE, NULL));
  }

  int rc = 0;
  msg_t ret;
  struct timespec ts = {0};
  struct timespec *pts = NULL;

  if (millis > 0) {
    ts = mqueue_timespec(millis);
    pts = &ts;
  }

  pthread_mutex_lock(&(mqueue->mutex));
  while (mqueue_size(mqueue) == 0 && mqueue->status != MQUEUE_STATUS_CLOSED && rc == 0) {
    rc = mqueue_cond_wait(&(mqueue->tcond1), &(mqueue->mutex), pts);
  }

  if (rc != 0) {
    ret = msg_create(rc, NULL);
    goto mqueue_pop_exit;
  }
  else if (mqueue->status == MQUEUE_STATUS_CLOSED) {
    ret = msg_create(MSG_TYPE_CLOSE, NULL);
    goto mqueue_pop_exit;
  }

  ret = mqueue->buffer[mqueue->pos1];
  mqueue->buffer[mqueue->pos1].type = MSG_TYPE_NULL;
  mqueue->buffer[mqueue->pos1].data = NULL;

  if (mqueue->pos1 == mqueue->pos2) {
    mqueue->status = MQUEUE_STATUS_EMPTY;
  }
  else {
    mqueue->pos1 = (mqueue->pos1+1)%(mqueue->capacity);
  }

mqueue_pop_exit:
  pthread_cond_signal(&(mqueue->tcond2));
  pthread_mutex_unlock(&(mqueue->mutex));
  return(ret);
}

/**************************************************************************//**
 * @see mqueue_pop_int
 * @details Adds debug trace to mqueue_pop_int.
 */
msg_t mqueue_pop(mqueue_t *mqueue, size_t millis)
{
  struct timeval t1 = {0};
  gettimeofday(&t1, NULL);

  msg_t ret = mqueue_pop_int(mqueue, millis);

  struct timeval t2 = {0};
  gettimeofday(&t2, NULL);

  syslog(LOG_DEBUG, "mqueue - %s.pop(%zu) = [%s, %p], etime = %.3lf sec",
         mqueue->name,
         millis,
         msg_type_str(ret.type),
         ret.data,
         difftimeval(&t1, &t2));

  return ret;
}

/**************************************************************************//**
 * @brief Close the queue.
 * @details Caution, threads waiting for push/pop are not affected by close.
 * @param[in,out] mqueue Message queue object.
 */
void mqueue_close(mqueue_t *mqueue)
{
  if (mqueue == NULL || mqueue->status == MQUEUE_STATUS_UNINITIALIZED) {
    assert(false);
    return;
  }
  pthread_mutex_lock(&(mqueue->mutex));
  mqueue->status = MQUEUE_STATUS_CLOSED;
  pthread_cond_broadcast(&(mqueue->tcond1));
  pthread_cond_broadcast(&(mqueue->tcond2));
  pthread_mutex_unlock(&(mqueue->mutex));
  syslog(LOG_DEBUG, "mqueue - %s closed", mqueue->name);
}
