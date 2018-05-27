
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

#ifndef CQUEUE_H
#define CQUEUE_H

#include <stdbool.h>
#include <pthread.h>
#include <semaphore.h>

/**************************************************************************//**
 * @brief Message between two threads.
 */
typedef struct msg_t
{
  //! Type of message (see MSG_TYPE_* macros)
  short type;
  //! Message payload.
  void *data;
  //TODO: add message timestamp
} msg_t;

/**************************************************************************//**
 * @brief Message queue status.
 */
typedef enum {
  MQUEUE_STATUS_UNINITIALIZED = 0, // Uninitialized message queue.
  MQUEUE_STATUS_EMPTY,             // Empty message queue.
  MQUEUE_STATUS_NOTEMPTY,          // Not empty message queue.
  MQUEUE_STATUS_CLOSED             // Closed message queue.
} mqueue_status_e;

/**************************************************************************//**
 * @brief Message queue between 2 threads:
 *        - circular queue
 *        - thread-safe
 *        - automatic resizing until max capacity
 *        - non-repeated elements (optional)
 *        - push() blocks if max capacity exceeded
 *        - pop() blocks if no elements
 *        - timeout support in push()/pop()
 */
typedef struct mqueue_t
{
  //! Message queue name (used in traces).
  char *name;
  //! Mutex to protect data access.
  pthread_mutex_t mutex;
  //! Semaphore to control pop().
  sem_t sem1;
  //! Semaphore to control push().
  sem_t sem2;
  //! Circular queue buffer.
  msg_t *buffer;
  //! Circular queue first index (included).
  size_t pos1;
  //! Circular queue last index (included).
  size_t pos2;
  //! Circular queue current capacity.
  size_t capacity;
  //! Message queue maximum capacity (0=unlimited).
  size_t max_capacity;
  //! Message queue status (internal usage).
  mqueue_status_e status;
  //TODO: num_msgs, avg_time, etc.
} mqueue_t;

// Message type indicating an empty message (data=NULL).
#define MSG_TYPE_NULL 0
// Message type indicating an error.
#define MSG_TYPE_ERROR 1
// Message type indicating an interruption.
#define MSG_TYPE_EINTR 2
// Message type indicating to quit.
#define MSG_TYPE_CLOSE 3
// Message type indicating timeout expired.
#define MSG_TYPE_TIMEOUT 4
// Message type indicating that item already exists.
#define MSG_TYPE_EXISTS 5

// Message type indicating a witem to update.
#define MSG_TYPE_FILE0 21
// Message type indicating a witem to close.
#define MSG_TYPE_FILE1 22

// Message type indicating a matched content.
#define MSG_TYPE_MATCH1 31

/**************************************************************************
 * Function declarations.
 */
extern msg_t msg_create(short type, void *data);
extern const char* msg_type_str(short type);
extern int mqueue_init(mqueue_t *mqueue, const char *name, size_t max_capacity);
extern int mqueue_push(mqueue_t *mqueue, short type, void *obj, bool unique, size_t millis);
extern msg_t mqueue_pop(mqueue_t *mqueue, size_t millis);
extern void mqueue_reset(mqueue_t *mqueue, void (*item_free)(void*));
extern void mqueue_close(mqueue_t *mqueue);

#endif

