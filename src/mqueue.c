
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
static msg_t msg_create(int type, void *data)
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
static const char* msg_type_str(int type)
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
  mqueue->max_capacity = max_capacity;
  mqueue->capacity = 0U;
  mqueue->front = 0U;
  mqueue->length = 0U;
  mqueue->open = false;
  mqueue->num_incoming_msgs = 0U;
  mqueue->num_delivered_msgs = 0U;
  mqueue->millis_waiting_push = 0U;
  mqueue->millis_waiting_pop = 0U;

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

  mqueue->capacity = (max_capacity==0?INITIAL_CAPACITY:MIN(max_capacity, INITIAL_CAPACITY));
  mqueue->buffer = (msg_t *) calloc(mqueue->capacity, sizeof(msg_t));
  if (mqueue->buffer == NULL) {
    rc = 1;
    goto mqueue_init_err5;
  }

  mqueue->open = true;
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
  if (mqueue == NULL) {
    return;
  }

  syslog(LOG_DEBUG, "mqueue - %s reseted [incoming_msgs=%zu, delivered_msgs=%zu, millis_push=%zu, millis_pop=%zu]",
         mqueue->name, mqueue->num_incoming_msgs, mqueue->num_delivered_msgs,
         mqueue->millis_waiting_push, mqueue->millis_waiting_pop);

  // free objects (only if item_free not NULL)
  if (mqueue->buffer != NULL && item_free != NULL) {
    size_t len = mqueue->length;
    for(size_t i=0; i<len; i++) {
      size_t pos = (mqueue->front+i)%(mqueue->capacity);
      item_free(mqueue->buffer[pos].data);
      mqueue->buffer[pos].data = NULL;
    }
  }
  free(mqueue->buffer);
  mqueue->buffer = NULL;
  mqueue->capacity = 0;
  mqueue->front = 0;
  mqueue->length = 0;
  mqueue->max_capacity = 0;
  mqueue->open = false;
  mqueue->num_incoming_msgs = 0U;
  mqueue->num_delivered_msgs = 0U;
  mqueue->millis_waiting_push = 0U;
  mqueue->millis_waiting_pop = 0U;
  pthread_cond_destroy(&(mqueue->tcond1));
  pthread_cond_destroy(&(mqueue->tcond2));
  pthread_mutex_destroy(&(mqueue->mutex));
  free(mqueue->name);
  mqueue->name = NULL;
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

  size_t len = mqueue->length;
  for(size_t i=0; i<len; i++) {
    size_t pos = (mqueue->front+i)%(mqueue->capacity);
    new_buffer[i] = mqueue->buffer[pos];
  }

  syslog(LOG_DEBUG, "mqueue - %s resized [%zu -> %zu]", mqueue->name, mqueue->capacity, new_capacity);

  mqueue->front = 0;
  mqueue->length = len;
  free(mqueue->buffer);
  mqueue->buffer = new_buffer;
  mqueue->capacity = new_capacity;

  return(0);
}

/**************************************************************************//**
 * @brief Returns current time + millis
 * @param millis Milliseconds to add to current time.
 * @return timespec struct filled.
 */
static struct timespec mqueue_timespec(size_t millis)
{
  struct timespec ts = {0};

  clock_gettime(CLOCK_REALTIME, &ts);

  if (millis > 0) {
    size_t seconds = millis/1000;
    ts.tv_sec += seconds;
    ts.tv_nsec += (millis-seconds*1000)*1000000;
    if (ts.tv_nsec >= 1000000000) {
      ts.tv_sec += 1;
      ts.tv_nsec -= 1000000000;
    }
  }

  return ts;
}

/**************************************************************************//**
 * @brief Waits until condition unlocks.
 * @param[in,out] tcond Condition variable.
 * @param[in,out] mutex Mutex blocking read/write.
 * @param[in] ts Time to stop (NULL = non timedout).
 * @return 0 = mutex unlocked,
 *         MSG_TYPE_TIMEOUT = wait timedout,
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
  size_t len = mqueue->length;

  for(size_t i=0; i<len; i++) {
    size_t pos = (mqueue->front+i)%(mqueue->capacity);
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
  if (mqueue == NULL) {
    assert(false);
    return(MSG_TYPE_ERROR);
  }
  else if (!mqueue->open) {
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
  while (rc == 0 && mqueue->open &&
         mqueue->max_capacity > 0 && mqueue->length >= mqueue->max_capacity) {
    rc = mqueue_cond_wait(&(mqueue->tcond2), &(mqueue->mutex), pts);
  }

  if (rc != 0) {
    goto mqueue_push_exit;
  }
  if (!mqueue->open) {
    rc = MSG_TYPE_CLOSE;
    goto mqueue_push_exit;
  }

  // check if obj exist in mqueue
  if (unique && mqueue_update_msg_type(mqueue, obj, type) > 0) {
    rc = MSG_TYPE_EXISTS;
    goto mqueue_push_exit;
  }

  // if mqueue is full then increase capacity
  if (mqueue->length >= mqueue->capacity) {
    rc = mqueue_resize(mqueue);
    if (rc != 0) {
      rc = MSG_TYPE_ERROR;
      goto mqueue_push_exit;
    }
  }

  // reassign front (not stricly necessary)
  if (mqueue->length == 0) {
    mqueue->front = 0;
  }

  // copy new message in queue
  size_t pos = (mqueue->front+mqueue->length)%(mqueue->capacity);
  mqueue->buffer[pos].type = type;
  mqueue->buffer[pos].data = obj;
  mqueue->length++;
  mqueue->num_incoming_msgs++;
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
  mqueue->millis_waiting_push += difftimeval(&t1, &t2);

  if (loglevel == LOG_DEBUG) {
    syslog(LOG_DEBUG, "mqueue - %s.push(%s, %p, %s, %zu) = %s, etime = %.3lf sec",
         mqueue->name,
         msg_type_str(type),
         obj,
         (unique?"true":"false"),
         millis,
         (rc==0?"OK":msg_type_str(rc)),
         difftimeval(&t1, &t2));
  }

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
  if (mqueue == NULL) {
    assert(false);
    return(msg_create(MSG_TYPE_ERROR, NULL));
  }
  else if (!mqueue->open) {
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
  while (rc == 0 && mqueue->open && mqueue->length == 0) {
    rc = mqueue_cond_wait(&(mqueue->tcond1), &(mqueue->mutex), pts);
  }

  if (rc != 0) {
    ret = msg_create(rc, NULL);
    goto mqueue_pop_exit;
  }
  else if (!mqueue->open) {
    ret = msg_create(MSG_TYPE_CLOSE, NULL);
    goto mqueue_pop_exit;
  }

  // retrieve message
  assert(mqueue->length > 0);
  ret = mqueue->buffer[mqueue->front];

  // reset slot (not strictly necessary)
  mqueue->buffer[mqueue->front].type = MSG_TYPE_NULL;
  mqueue->buffer[mqueue->front].data = NULL;

  // update front and counters
  mqueue->front = (mqueue->front+1)%mqueue->capacity;
  mqueue->length--;
  mqueue->num_delivered_msgs++;

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
  mqueue->millis_waiting_pop += difftimeval(&t1, &t2);

  if (loglevel == LOG_DEBUG) {
    syslog(LOG_DEBUG, "mqueue - %s.pop(%zu) = [%s, %p], etime = %.3lf sec",
         mqueue->name,
         millis,
         msg_type_str(ret.type),
         ret.data,
         difftimeval(&t1, &t2));
  }

  return ret;
}

/**************************************************************************//**
 * @brief Close the queue.
 * @details Caution, threads waiting for push/pop are not affected by close.
 * @param[in,out] mqueue Message queue object.
 */
void mqueue_close(mqueue_t *mqueue)
{
  if (mqueue == NULL) {
    assert(false);
    return;
  }
  pthread_mutex_lock(&(mqueue->mutex));
  mqueue->open = false;
  pthread_cond_broadcast(&(mqueue->tcond1));
  pthread_cond_broadcast(&(mqueue->tcond2));
  pthread_mutex_unlock(&(mqueue->mutex));
  syslog(LOG_DEBUG, "mqueue - %s closed", mqueue->name);
}
