/*
 * Copyright (c) 2012, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 *  @file       MessageQ.c
 *  @brief      Prototype Mapping of SysLink MessageQ to Socket ABI
 *              (SysLink 3).
 *
 *  @ver        02.00.00.51_alpha2 (kernel code is basis for this module)
 *
 */
/*============================================================================
 *  @file   MessageQ.c
 *
 *  @brief  MessageQ module "server" implementation
 *
 *  This implementation is geared for use in a "client/server" model, whereby
 *  system-wide data is maintained here as needed and process-specific data
 *  is handled at the "client" level.  At the moment, LAD is the only "user"
 *  of this implementation.
 *
 *  The MessageQ module supports the structured sending and receiving of
 *  variable length messages. This module can be used for homogeneous or
 *  heterogeneous multi-processor messaging.
 *
 *  MessageQ provides more sophisticated messaging than other modules. It is
 *  typically used for complex situations such as multi-processor messaging.
 *
 *  The following are key features of the MessageQ module:
 *  -Writers and readers can be relocated to another processor with no
 *   runtime code changes.
 *  -Timeouts are allowed when receiving messages.
 *  -Readers can determine the writer and reply back.
 *  -Receiving a message is deterministic when the timeout is zero.
 *  -Messages can reside on any message queue.
 *  -Supports zero-copy transfers.
 *  -Can send and receive from any type of thread.
 *  -Notification mechanism is specified by application.
 *  -Allows QoS (quality of service) on message buffer pools. For example,
 *   using specific buffer pools for specific message queues.
 *
 *  Messages are sent and received via a message queue. A reader is a thread
 *  that gets (reads) messages from a message queue. A writer is a thread that
 *  puts (writes) a message to a message queue. Each message queue has one
 *  reader and can have many writers. A thread may read from or write to multiple
 *  message queues.
 *
 *  Conceptually, the reader thread owns a message queue. The reader thread
 *  creates a message queue. Writer threads  a created message queues to
 *  get access to them.
 *
 *  Message queues are identified by a system-wide unique name. Internally,
 *  MessageQ uses the NameServermodule for managing
 *  these names. The names are used for opening a message queue. Using
 *  names is not required.
 *
 *  Messages must be allocated from the MessageQ module. Once a message is
 *  allocated, it can be sent on any message queue. Once a message is sent, the
 *  writer loses ownership of the message and should not attempt to modify the
 *  message. Once the reader receives the message, it owns the message. It
 *  may either free the message or re-use the message.
 *
 *  Messages in a message queue can be of variable length. The only
 *  requirement is that the first field in the definition of a message must be a
 *  MsgHeader structure. For example:
 *  typedef struct MyMsg {
 *      MessageQ_MsgHeader header;
 *      ...
 *  } MyMsg;
 *
 *  The MessageQ API uses the MessageQ_MsgHeader internally. Your application
 *  should not modify or directly access the fields in the MessageQ_MsgHeader.
 *
 *  All messages sent via the MessageQ module must be allocated from a
 *  Heap implementation. The heap can be used for
 *  other memory allocation not related to MessageQ.
 *
 *  An application can use multiple heaps. The purpose of having multiple
 *  heaps is to allow an application to regulate its message usage. For
 *  example, an application can allocate critical messages from one heap of fast
 *  on-chip memory and non-critical messages from another heap of slower
 *  external memory
 *
 *  MessageQ does support the usage of messages that allocated via the
 *  alloc function. Please refer to the staticMsgInit
 *  function description for more details.
 *
 *  In a multiple processor system, MessageQ communications to other
 *  processors via MessageQTransport instances. There must be one and
 *  only one MessageQTransport instance for each processor where communication
 *  is desired.
 *  So on a four processor system, each processor must have three
 *  MessageQTransport instance.
 *
 *  The user only needs to create the MessageQTransport instances. The instances
 *  are responsible for registering themselves with MessageQ.
 *  This is accomplished via the registerTransport function.
 *
 *  ============================================================================
 */


/* Standard headers */
#include <Std.h>

/* Linux specific header files, replacing OSAL: */
#include <pthread.h>

/* Socket Headers */
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/eventfd.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>

/* SysLink Socket Protocol Family */
#include <net/rpmsg.h>

/* Module level headers */
#include <ti/ipc/NameServer.h>
#include <ti/ipc/MultiProc.h>
#include <_MultiProc.h>
#include <ti/ipc/MessageQ.h>
#include <_MessageQ.h>

#include <_lad.h>

/* =============================================================================
 * Macros/Constants
 * =============================================================================
 */

/*!
 *  @brief  Name of the reserved NameServer used for MessageQ.
 */
#define MessageQ_NAMESERVER  "MessageQ"

/* Slot 0 reserved for NameServer messages: */
#define RESERVED_MSGQ_INDEX  1

/* Define BENCHMARK to quiet key MessageQ APIs: */
//#define BENCHMARK

/* =============================================================================
 * Structures & Enums
 * =============================================================================
 */

/* structure for MessageQ module state */
typedef struct MessageQ_ModuleObject {
    Int                 refCount;
    /*!< Reference count */
    NameServer_Handle   nameServer;
    /*!< Handle to the local NameServer used for storing GP objects */
    pthread_mutex_t     gate;
    /*!< Handle of gate to be used for local thread safety */
    MessageQ_Config     cfg;
    /*!< Current config values */
    MessageQ_Config     defaultCfg;
    /*!< Default config values */
    MessageQ_Params     defaultInstParams;
    /*!< Default instance creation parameters */
    MessageQ_Handle *   queues;
    /*!< Global array of message queues */
    UInt16              numQueues;
    /*!< Initial number of messageQ objects allowed */
    Bool                canFreeQueues;
    /*!< Grow option */
    Bits16              seqNum;
    /*!< sequence number */
} MessageQ_ModuleObject;

/*!
 *  @brief  Structure for the Handle for the MessageQ.
 */
typedef struct MessageQ_Object {
    MessageQ_Params         params;
    /*! Instance specific creation parameters */
    MessageQ_QueueId        queue;
    /* Unique id */
    Ptr                     nsKey;
    /* NameServer key */
    Int                     ownerPid;
    /* Process ID of owner */
} MessageQ_Object;


/* =============================================================================
 *  Globals
 * =============================================================================
 */
static MessageQ_ModuleObject MessageQ_state =
{
    .refCount               = 0,
    .nameServer             = NULL,
    .queues                 = NULL,
    .numQueues              = 2u,
    .canFreeQueues          = FALSE,
    .gate                   = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP,
    .defaultCfg.traceFlag   = FALSE,
    .defaultCfg.maxRuntimeEntries = 32u,
    .defaultCfg.maxNameLen    = 32u,
};

/*!
 *  @var    MessageQ_module
 *
 *  @brief  Pointer to the MessageQ module state.
 */
MessageQ_ModuleObject * MessageQ_module = &MessageQ_state;


/* =============================================================================
 * Forward declarations of internal functions
 * =============================================================================
 */
/* Grow the MessageQ table */
static UInt16 _MessageQ_grow(MessageQ_Object * obj);

/* =============================================================================
 * APIS
 * =============================================================================
 */
/* Function to get default configuration for the MessageQ module.
 *
 */
Void MessageQ_getConfig(MessageQ_Config * cfg)
{
    assert(cfg != NULL);

    /* If setup has not yet been called... */
    if (MessageQ_module->refCount < 1) {
        memcpy(cfg, &MessageQ_module->defaultCfg, sizeof(MessageQ_Config));
    }
    else {
        memcpy(cfg, &MessageQ_module->cfg, sizeof(MessageQ_Config));
    }
}

/* Function to setup the MessageQ module. */
Int MessageQ_setup(const MessageQ_Config * cfg)
{
    Int                    status = MessageQ_S_SUCCESS;
    NameServer_Params      params;

    pthread_mutex_lock(&(MessageQ_module->gate));

    LOG1("MessageQ_setup: entered, refCount=%d\n", MessageQ_module->refCount)

    MessageQ_module->refCount++;
    if (MessageQ_module->refCount > 1) {
        status = MessageQ_S_ALREADYSETUP;
        LOG1("MessageQ module has been already setup, refCount=%d\n", MessageQ_module->refCount)

        goto exitSetup;
    }

    /* Initialize the parameters */
    NameServer_Params_init(&params);
    params.maxValueLen = sizeof(UInt32);
    params.maxNameLen  = cfg->maxNameLen;

    /* Create the nameserver for modules */
    MessageQ_module->nameServer = NameServer_create(MessageQ_NAMESERVER,
                                                    &params);

    memcpy(&MessageQ_module->cfg, (void *)cfg, sizeof(MessageQ_Config));

    MessageQ_module->seqNum = 0;
    MessageQ_module->numQueues = cfg->maxRuntimeEntries;
    MessageQ_module->queues = (MessageQ_Handle *)
        calloc(1, sizeof(MessageQ_Handle) * MessageQ_module->numQueues);

exitSetup:
    LOG1("MessageQ_setup: exiting, refCount=%d\n", MessageQ_module->refCount)

    pthread_mutex_unlock(&(MessageQ_module->gate));

    return (status);
}

/*
 * Function to destroy the MessageQ module.
 */
Int MessageQ_destroy(void)
{
    Int    status    = MessageQ_S_SUCCESS;
    UInt32 i;

    pthread_mutex_lock(&(MessageQ_module->gate));

    LOG1("MessageQ_destroy: entered, refCount=%d\n", MessageQ_module->refCount)

    /* Decrease the refCount */
    MessageQ_module->refCount--;
    if (MessageQ_module->refCount > 0) {
        goto exitDestroy;
    }

    /* Delete any Message Queues that have not been deleted so far. */
    for (i = 0; i< MessageQ_module->numQueues; i++) {
        if (MessageQ_module->queues [i] != NULL) {
            MessageQ_delete(&(MessageQ_module->queues [i]));
        }
    }

    if (MessageQ_module->nameServer != NULL) {
        /* Delete the nameserver for modules */
        status = NameServer_delete(&MessageQ_module->nameServer);
    }

    /* Since MessageQ_module->gate was not allocated, no need to delete. */

    if (MessageQ_module->queues != NULL) {
        free(MessageQ_module->queues);
        MessageQ_module->queues = NULL;
    }

    memset(&MessageQ_module->cfg, 0, sizeof(MessageQ_Config));
    MessageQ_module->numQueues  = 0u;
    MessageQ_module->canFreeQueues = TRUE;

exitDestroy:
    LOG1("MessageQ_destroy: exiting, refCount=%d\n", MessageQ_module->refCount)

    pthread_mutex_unlock(&(MessageQ_module->gate));

    return (status);
}

/* Function to initialize the parameters for the MessageQ instance. */
Void MessageQ_Params_init(MessageQ_Params * params)
{
    memcpy(params, &(MessageQ_module->defaultInstParams),
           sizeof(MessageQ_Params));

    return;
}

/*
 *   Function to create a MessageQ object for receiving.
 */
MessageQ_Handle MessageQ_create(String name, const MessageQ_Params * params)
{
    Int                 status    = MessageQ_S_SUCCESS;
    MessageQ_Object   * obj    = NULL;
    Bool                found  = FALSE;
    UInt16              count  = 0;
    UInt16              queueIndex = 0u;
    UInt16              procId;
    int                 i;

    LOG1("MessageQ_create: creating '%s'\n", name)

    /* Create the generic obj */
    obj = (MessageQ_Object *)calloc(1, sizeof(MessageQ_Object));

    pthread_mutex_lock(&(MessageQ_module->gate));

    count = MessageQ_module->numQueues;

    /* Search the dynamic array for any holes */
    /* We start from 1, as 0 is reserved for binding NameServer: */
    for (i = RESERVED_MSGQ_INDEX; i < count ; i++) {
        if (MessageQ_module->queues [i] == NULL) {
            MessageQ_module->queues [i] = (MessageQ_Handle)obj;
            queueIndex = i;
            found = TRUE;
            break;
        }
    }

    if (found == FALSE) {
        /* Growth is always allowed. */
        queueIndex = _MessageQ_grow(obj);
    }

    pthread_mutex_unlock(&(MessageQ_module->gate));

    if (params != NULL) {
       /* Populate the params member */
        memcpy((Ptr)&obj->params, (Ptr)params, sizeof(MessageQ_Params));
    }

    procId = MultiProc_self();
    /* create globally unique messageQ ID: */
    obj->queue = (MessageQ_QueueId)(((UInt32)procId << 16) | queueIndex);
    obj->ownerPid = 0;

    if (name != NULL) {
        obj->nsKey = NameServer_addUInt32(MessageQ_module->nameServer, name,
                                          obj->queue);
    }

    /* Cleanup if fail */
    if (status < 0) {
        MessageQ_delete((MessageQ_Handle *)&obj);
    }

    LOG1("MessageQ_create: returning %p\n", obj)

    return ((MessageQ_Handle)obj);
}

/*
 * Function to delete a MessageQ object for a specific slave processor.
 */
Int MessageQ_delete(MessageQ_Handle * handlePtr)
{
    Int              status = MessageQ_S_SUCCESS;
    MessageQ_Object *obj;
    MessageQ_Handle queue;

    obj = (MessageQ_Object *)(*handlePtr);

    LOG1("MessageQ_delete: deleting %p\n", obj)

    queue = MessageQ_module->queues[(MessageQ_QueueIndex)(obj->queue)];
    if (queue != obj) {
        LOG1("    ERROR: obj != MessageQ_module->queues[%d]\n", (MessageQ_QueueIndex)(obj->queue))
    }

    if (obj->nsKey != NULL) {
        /* Remove from the name server */
        status = NameServer_removeEntry(MessageQ_module->nameServer,
                                         obj->nsKey);
        if (status < 0) {
            /* Override with a MessageQ status code. */
            status = MessageQ_E_FAIL;
        }
        else {
            status = MessageQ_S_SUCCESS;
        }
    }

    pthread_mutex_lock(&(MessageQ_module->gate));

    /* Clear the MessageQ obj from array. */
    MessageQ_module->queues[(MessageQ_QueueIndex)(obj->queue)] = NULL;

    /* Release the local lock */
    pthread_mutex_unlock(&(MessageQ_module->gate));

    /* Now free the obj */
    free(obj);
    *handlePtr = NULL;

    LOG1("MessageQ_delete: returning %d\n", status)

    return (status);
}

/* Returns the MessageQ_QueueId associated with the handle. */
MessageQ_QueueId MessageQ_getQueueId(MessageQ_Handle handle)
{
    MessageQ_Object * obj = (MessageQ_Object *)handle;
    UInt32            queueId;

    queueId = (obj->queue);

    return queueId;
}

/*!
 *  @brief   Grow the MessageQ table
 *
 *  @param   obj     Pointer to the MessageQ object.
 *
 *  @sa      _MessageQ_grow
 *
 */
static UInt16 _MessageQ_grow(MessageQ_Object * obj)
{
    UInt16            queueIndex = MessageQ_module->numQueues;
    UInt16            oldSize;
    MessageQ_Handle * queues;
    MessageQ_Handle * oldQueues;

    /* No parameter validation required since this is an internal function. */
    oldSize = (MessageQ_module->numQueues) * sizeof(MessageQ_Handle);

    /* Allocate larger table */
    queues = calloc(1, oldSize + sizeof(MessageQ_Handle));

    /* Copy contents into new table */
    memcpy(queues, MessageQ_module->queues, oldSize);

    /* Fill in the new entry */
    queues[queueIndex] = (MessageQ_Handle)obj;

    /* Hook-up new table */
    oldQueues = MessageQ_module->queues;
    MessageQ_module->queues = queues;
    MessageQ_module->numQueues++;

    /* Delete old table if not statically defined */
    if (MessageQ_module->canFreeQueues == TRUE) {
        free(oldQueues);
    }
    else {
        MessageQ_module->canFreeQueues = TRUE;
    }

    LOG1("_MessageQ_grow: queueIndex: 0x%x\n", queueIndex)

    return (queueIndex);
}

/*
 * This is a helper function to initialize a message.
 */
Void MessageQ_msgInit(MessageQ_Msg msg)
{
    msg->reserved0 = 0;  /* We set this to distinguish from NameServerMsg */
    msg->replyId   = (UInt16)MessageQ_INVALIDMESSAGEQ;
    msg->msgId     = MessageQ_INVALIDMSGID;
    msg->dstId     = (UInt16)MessageQ_INVALIDMESSAGEQ;
    msg->flags     = MessageQ_HEADERVERSION | MessageQ_NORMALPRI;
    msg->srcProc   = MultiProc_self();

    pthread_mutex_lock(&(MessageQ_module->gate));
    msg->seqNum  = MessageQ_module->seqNum++;
    pthread_mutex_unlock(&(MessageQ_module->gate));
}

NameServer_Handle MessageQ_getNameServerHandle(void)
{
    return MessageQ_module->nameServer;
}

Void MessageQ_setQueueOwner(MessageQ_Handle handle, Int pid)
{
    handle->ownerPid = pid;

    return;
}

Void MessageQ_cleanupOwner(Int pid)
{
    MessageQ_Handle queue;
    Int i;

    for (i = 0; i < MessageQ_module->numQueues; i++) {
        queue = MessageQ_module->queues[i];
        if (queue != NULL && queue->ownerPid == pid) {
            MessageQ_delete(&queue);
        }
    }
}
