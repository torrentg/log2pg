
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

#ifndef PROCESSOR_H
#define PROCESSOR_H

#include "mqueue.h"

/**************************************************************************//**
 * @brief Processor thread.
 */
typedef struct processor_t
{
  //! Messages received from monitor thread.
  mqueue_t *mqueue1;
  //! Messages sended to database thread.
  mqueue_t *mqueue2;
} processor_t;

/**************************************************************************
 * Function declarations.
 */
extern int processor_init(processor_t *processor, mqueue_t *mqueue1, mqueue_t *mqueue2);
extern void* processor_run(void *ptr);
extern void processor_reset(processor_t *processor);

#endif
