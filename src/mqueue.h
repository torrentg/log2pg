
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

#ifndef MQUEUE_H
#define MQUEUE_H

#include <stdbool.h>
#include <pthread.h>

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
  //! Condition variable to control pop().
  pthread_cond_t tcond1;
  //! Condition variable to control push().
  pthread_cond_t tcond2;
  //! Circular queue buffer.
  msg_t *buffer;
  //! Circular queue current capacity.
  size_t capacity;
  //! Circular queue front index.
  size_t front;
  //! Circular queue length.
  size_t length;
  //! Message queue maximum capacity (0=unlimited).
  size_t max_capacity;
  //! Number of received messages (push).
  size_t num_incoming_msgs;
  //! Number of delivered messages (pop).
  size_t num_delivered_msgs;
  //! Elapsed time push waited because queue is full.
  size_t millis_waiting_push;
  //! Elapsed time pop waiting because queue is empty.
  size_t millis_waiting_pop;
  //! Indicate if mqueue is open or closed.
  bool open;
} mqueue_t;

/**************************************************************************
 * Function declarations.
 */
extern int mqueue_init(mqueue_t *mqueue, const char *name, size_t max_capacity);
extern int mqueue_push(mqueue_t *mqueue, short type, void *obj, bool unique, size_t millis);
extern msg_t mqueue_pop(mqueue_t *mqueue, size_t millis);
extern void mqueue_reset(mqueue_t *mqueue, void (*item_free)(void*));
extern void mqueue_close(mqueue_t *mqueue);

/**************************************************************************
 * Defines used by application.
 */
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

#endif
