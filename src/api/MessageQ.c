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
 *  @brief  MessageQ module implementation
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

/* Module level headers */
#include <ti/ipc/NameServer.h>
#include <ti/ipc/MultiProc.h>
#include <_MultiProc.h>
#include <ti/ipc/MessageQ.h>
#include <_MessageQ.h>

/* Socket Headers */
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>

/* SysLink Socket Protocol Family */
#include <net/rpmsg.h>

/* Socket utils: */
#include <SocketFxns.h>

/* =============================================================================
 * Macros/Constants
 * =============================================================================
 */

//#define VERBOSE

/*!
 *  @brief  Name of the reserved NameServer used for MessageQ.
 */
#define MessageQ_NAMESERVER  "MessageQ"

/*!
 *  @brief  Value of an invalid socket ID:
 */
#define Transport_INVALIDSOCKET  (0xFFFFFFFF)

/* More magic rpmsg port numbers: */
#define MESSAGEQ_RPMSG_PORT       61
#define MESSAGEQ_RPMSG_MAXSIZE   512

/* Trace flag settings: */
#define TRACESHIFT    12
#define TRACEMASK     0x1000

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
    int                 sock[MultiProc_MAXPROCESSORS];
    /*!< Sockets to for sending to each remote processor */
    MessageQ_Handle *   queues;
    /*!< Global array of message queues */
    UInt16              numQueues;
    /*!< Initial number of messageQ objects allowed */
    UInt16              numHeaps;
    /*!< Number of Heaps */
    Bool                canFreeQueues;
    /*!< Grow option */
    Bits16              seqNum;
    /*!< sequence number. */
} MessageQ_ModuleObject;

/*!
 *  @brief  Structure for the Handle for the MessageQ.
 */
typedef struct MessageQ_Object_tag {
    MessageQ_Params         params;
    /*! Instance specific creation parameters */
    MessageQ_QueueId        queue;
    /* Unique id */
    Ptr                     nsKey;
    /* NameServer key */
    int                     fd[MultiProc_MAXPROCESSORS];
    /* File Descriptor to block on messages from remote processors. */
    int                     unblockFd;
    /* Write this fd to unblock the select() call in MessageQ _get() */
} MessageQ_Object;


/* =============================================================================
 *  Globals
 * =============================================================================
 */
static MessageQ_ModuleObject MessageQ_state =
{
    .nameServer             = NULL,
    .queues                 = NULL,
    .numQueues              = 2u,
    .numHeaps               = 1u,
    .canFreeQueues          = FALSE,
    .defaultCfg.traceFlag   = FALSE,
    .defaultCfg.numHeaps    = 1u,
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
static UInt16 _MessageQ_grow (MessageQ_Object * obj);

/* This is a helper function to initialize a message. */
static Void MessageQ_msgInit (MessageQ_Msg msg);
static Int transportCreateEndpoint(int * fd, UInt16 rprocId, UInt16 queueIndex);
static Int transportCloseEndpoint(int fd);
static Int transportGet(int sock, MessageQ_Msg * retMsg);
static Int transportPut(MessageQ_Msg msg, UInt16 dstId, UInt16 dstProcId);

/* =============================================================================
 * APIS
 * =============================================================================
 */
/* Function to get default configuration for the MessageQ module.
 *
 */
Void MessageQ_getConfig (MessageQ_Config * cfg)
{
    assert (cfg != NULL);

    /* If setup has not yet been called... */
    if (MessageQ_module->refCount < 1)  {
        memcpy (cfg, &MessageQ_module->defaultCfg, sizeof (MessageQ_Config));
    }
    else {
        memcpy (cfg, &MessageQ_module->cfg, sizeof (MessageQ_Config));
    }
}

/* Function to setup the MessageQ module. */
Int MessageQ_setup (const MessageQ_Config * cfg)
{
    Int                    status = MessageQ_S_SUCCESS;
    NameServer_Params      params;
    int                    i;

    /* TBD: Protect from multiple threads. */
    MessageQ_module->refCount++;
    if (MessageQ_module->refCount > 1) {
        status = MessageQ_S_ALREADYSETUP;
        printf ("MessageQ module has been already setup in this process.\n"
                   "    RefCount: [%d]\n", MessageQ_module->refCount);
        goto exit;
    }

    /* Initialize the parameters */
    NameServer_Params_init (&params);
    params.maxValueLen = sizeof (UInt32);
    params.maxNameLen  = cfg->maxNameLen;

    /* Create the nameserver for modules */
    MessageQ_module->nameServer = NameServer_create (MessageQ_NAMESERVER,
                                &params);

    /* Create a default local gate. */
    pthread_mutex_init (&(MessageQ_module->gate), NULL);

    memcpy (&MessageQ_module->cfg, (void *) cfg, sizeof (MessageQ_Config));

    MessageQ_module->seqNum = 0;

    MessageQ_module->numHeaps = cfg->numHeaps;

    MessageQ_module->numQueues = cfg->maxRuntimeEntries;
    MessageQ_module->queues = (MessageQ_Handle *)
        calloc (1, sizeof (MessageQ_Handle) * MessageQ_module->numQueues);

    /* Clear sockets array. */
    for (i = 0; i < MultiProc_MAXPROCESSORS; i++) {
       MessageQ_module->sock[i]      = Transport_INVALIDSOCKET;
    }

exit:
    return (status);
}

/*
 * Function to destroy the MessageQ module.
 * Destroys socket/protocol maps;  sockets themselves should have been
 * destroyed in MessageQ_delete() and MessageQ_detach() calls.
 */
Int MessageQ_destroy (void)
{
    Int    status    = MessageQ_S_SUCCESS;
    Int    tmpStatus = MessageQ_S_SUCCESS;
    UInt32 i;

#ifdef VERBOSE
    printf ("MessageQ_destroy: Entered.\n");
#endif

    if (MessageQ_module->refCount > 0) {
        /* Delete any Message Queues that have not been deleted so far. */
        for (i = 0; i< MessageQ_module->numQueues; i++) {
            if (MessageQ_module->queues [i] != NULL) {
                tmpStatus = MessageQ_delete (&(MessageQ_module->queues [i]));
            }
        }
    }

    /* Decrease the refCount */
    MessageQ_module->refCount--;

    if (MessageQ_module->nameServer != NULL) {
        /* Delete the nameserver for modules */
        tmpStatus = NameServer_delete (&MessageQ_module->nameServer);
        if ((tmpStatus < 0) && (status >= 0)) {
              status = tmpStatus;
        }
    }

    /* Since MessageQ_module->gate was not allocated, no need to delete. */

    /* Assert that all sockets have already been closed. */
    for (i = 0; i < MultiProc_MAXPROCESSORS; i++) {
        assert(MessageQ_module->sock[i] == Transport_INVALIDSOCKET);
    }

    if (MessageQ_module->queues != NULL) {
        free (MessageQ_module->queues);
        MessageQ_module->queues = NULL;
    }

    memset (&MessageQ_module->cfg, 0, sizeof (MessageQ_Config));
    MessageQ_module->numQueues  = 0u;
    MessageQ_module->numHeaps   = 1u;
    MessageQ_module->canFreeQueues = TRUE;

#ifdef VERBOSE
    printf ("MessageQ_destroy: Exited.\n");
#endif

    return (status);
}

/* Function to initialize the parameters for the MessageQ instance. */
Void MessageQ_Params_init (MessageQ_Params * params)
{
    memcpy (params, &(MessageQ_module->defaultInstParams),
            sizeof (MessageQ_Params));

    return;
}

/*
 *   Function to create a MessageQ object for receiving.
 *
 *   Create a socket and bind the source address (local ProcId/MessageQ ID) in
 *   order to get messages dispatched to this messageQ.
 */
MessageQ_Handle MessageQ_create (String name, const MessageQ_Params * params)
{
    Int                 status    = MessageQ_S_SUCCESS;
    MessageQ_Object   * obj    = NULL;
    Bool                found  = FALSE;
    UInt16              count  = 0;
    UInt16              queueIndex = 0u;
    UInt16              procId;
    int                 i;
    UInt16              rprocId;

    /* Create the generic obj */
    obj = (MessageQ_Object *) calloc (1, sizeof (MessageQ_Object));

    pthread_mutex_lock (&(MessageQ_module->gate));
    count = MessageQ_module->numQueues;

    /* Search the dynamic array for any holes */
    /* We start from 1, as 0 is reserved for binding NameServer: */
    for (i = RESERVED_MSGQ_INDEX; i < count ; i++) {
        if (MessageQ_module->queues [i] == NULL) {
            MessageQ_module->queues [i] = (MessageQ_Handle) obj;
            queueIndex = i;
            found = TRUE;
            break;
        }
    }

    if (found == FALSE) {
        /* Growth is always allowed. */
        queueIndex = _MessageQ_grow (obj);
    }

    pthread_mutex_unlock (&(MessageQ_module->gate));

    if (params != NULL) {
       /* Populate the params member */
        memcpy((Ptr) &obj->params, (Ptr)params, sizeof (MessageQ_Params));
    }

    procId = MultiProc_self ();
    /* create globally unique messageQ ID: */
    obj->queue = (MessageQ_QueueId)(((UInt32)(procId) << 16) | queueIndex);

    if (name != NULL) {
        obj->nsKey = NameServer_addUInt32(MessageQ_module->nameServer, name,
                                          obj->queue);
    }

    /*
     * Create a set of communication endpoints (one per each remote proc),
     * and return the socket as target for MessageQ_put() calls, and as
     * a file descriptor to close during MessageQ_delete().
     */
    for (rprocId = 0; rprocId < MultiProc_getNumProcessors(); rprocId++) {
        obj->fd[rprocId] = Transport_INVALIDSOCKET;
        if (procId == rprocId) {
            /* Skip creating an endpoint for ourself. */
            continue;
        }
#ifdef VERBOSE
        printf ("MessageQ_create: creating endpoint for: %s, rprocId: %d\n",
                name, rprocId);
#endif
        status = transportCreateEndpoint(&obj->fd[rprocId], rprocId,
                                           queueIndex);
        if (status < 0) {
           goto cleanup;
        }
    }

    /*
     * Now, to support MessageQ_unblock() functionality, create an event object.
     * Writing to this event will unblock the select() call in MessageQ_get().
     */
    obj->unblockFd = eventfd(0, 0);
    if (obj->unblockFd == -1)  {
        printf ("MessageQ_create: eventfd creation failed: %d, %s\n",
                   errno, strerror(errno));
        status = MessageQ_E_FAIL;
    }

cleanup:
    /* Cleanup if fail: */
    if (status < 0) {
        MessageQ_delete((MessageQ_Handle *)&obj);
    }

    return ((MessageQ_Handle) obj);
}

/*
 * Function to delete a MessageQ object for a specific slave processor.
 *
 * Deletes the socket associated with this MessageQ object.
 */
Int MessageQ_delete (MessageQ_Handle * handlePtr)
{
    Int               status    = MessageQ_S_SUCCESS;
    MessageQ_Object * obj       = NULL;
    UInt16            rprocId;

    obj = (MessageQ_Object *) (*handlePtr);

    /* Close the event used for MessageQ_unblock(): */
    close(obj->unblockFd);

    /* Close the communication endpoint: */
    for (rprocId = 0; rprocId < MultiProc_getNumProcessors(); rprocId++) {
        if (obj->fd[rprocId] != Transport_INVALIDSOCKET) {
            status = transportCloseEndpoint(obj->fd[rprocId]);
        }
    }

    if (obj->nsKey != NULL) {
        /* Remove from the name server */
        status = NameServer_removeEntry (MessageQ_module->nameServer,
                                         obj->nsKey);
        if (status < 0) {
            /* Override with a MessageQ status code. */
            status = MessageQ_E_FAIL;
        }
        else {
            status = MessageQ_S_SUCCESS;
        }
    }

    pthread_mutex_lock (&(MessageQ_module->gate));

    /* Clear the MessageQ obj from array. */
    MessageQ_module->queues [(MessageQ_QueueIndex) (obj->queue)] = NULL;

    /* Release the local lock */
    pthread_mutex_unlock (&(MessageQ_module->gate));

    /* Now free the obj */
    free (obj);
    *handlePtr = NULL;

    return (status);
}

/*
 *  Opens an instance of MessageQ for sending.
 *
 *  We need not create a socket here; the sockets for all remote processors
 *  were created during MessageQ_attach(), and will be
 *  retrieved during MessageQ_put().
 */
Int MessageQ_open (String name, MessageQ_QueueId * queueId)
{
    Int status = MessageQ_S_SUCCESS;

    status = NameServer_getUInt32 (MessageQ_module->nameServer,
                                     name, queueId, NULL);

    if (status == NameServer_E_NOTFOUND) {
        /* Set return queue ID to invalid. */
        *queueId = MessageQ_INVALIDMESSAGEQ;
        status = MessageQ_E_NOTFOUND;
    }
    else if (status >= 0) {
        /* Override with a MessageQ status code. */
        status = MessageQ_S_SUCCESS;
    }
    else {
        /* Set return queue ID to invalid. */
        *queueId = MessageQ_INVALIDMESSAGEQ;
        /* Override with a MessageQ status code. */
        if (status == NameServer_E_TIMEOUT) {
            status = MessageQ_E_TIMEOUT;
        }
        else {
            status = MessageQ_E_FAIL;
        }
    }

    return (status);
}

/* Closes previously opened instance of MessageQ module. */
Int MessageQ_close (MessageQ_QueueId * queueId)
{
    Int32 status = MessageQ_S_SUCCESS;

    /* Nothing more to be done for closing the MessageQ. */
    *queueId = MessageQ_INVALIDMESSAGEQ;

    return (status);
}

/*
 * Place a message onto a message queue.
 *
 * Calls TransportShm_put(), which handles the sending of the message using the
 * appropriate kernel interface (socket, device ioctl) call for the remote
 * procId encoded in the queueId argument.
 *
 */
Int MessageQ_put (MessageQ_QueueId queueId, MessageQ_Msg msg)
{
    Int      status;
    UInt16   dstProcId  = (UInt16)(queueId >> 16);
    UInt16   queueIndex = (MessageQ_QueueIndex)(queueId & 0x0000ffff);

    msg->dstId     = queueIndex;
    msg->dstProc   = dstProcId;

    status = transportPut(msg, queueIndex, dstProcId);

    return (status);
}

/*
 * Gets a message for a message queue and blocks if the queue is empty.
 * If a message is present, it returns it.  Otherwise it blocks
 * waiting for a message to arrive.
 * When a message is returned, it is owned by the caller.
 *
 * We block using select() on the receiving socket's file descriptor, then
 * get the waiting message via the socket API recvfrom().
 * We use the socket stored in the messageQ object via a previous call to
 * MessageQ_create().
 *
 */
Int MessageQ_get (MessageQ_Handle handle, MessageQ_Msg * msg ,UInt timeout)
{
    Int     status = MessageQ_S_SUCCESS;
    Int     tmpStatus;
    MessageQ_Object * obj = (MessageQ_Object *) handle;
    int     retval;
    int     nfds;
    fd_set  rfds;
    struct  timeval tv;
    void    *timevalPtr;
    UInt16  rprocId;
    int     maxfd = 0;

    /* Wait (with timeout) and retreive message from socket: */
    FD_ZERO(&rfds);
    for (rprocId = 0; rprocId < MultiProc_getNumProcessors(); rprocId++) {
        if (rprocId == MultiProc_self()) {
            continue;
        }
        maxfd = MAX(maxfd, obj->fd[rprocId]);
        FD_SET(obj->fd[rprocId], &rfds);
    }

    /* Wait also on the event fd, which may be written by MessageQ_unblock(): */
    FD_SET(obj->unblockFd, &rfds);

    if (timeout == MessageQ_FOREVER) {
        timevalPtr = NULL;
    }
    else {
        /* Timeout given in msec: convert:  */
        tv.tv_sec = timeout / 1000;
        tv.tv_usec = (timeout % 1000) * 1000;
        timevalPtr = &tv;
    }
    /* Add one to last fd created: */
    nfds = MAX(maxfd, obj->unblockFd) + 1;

    retval = select(nfds, &rfds, NULL, NULL, timevalPtr);
    if (retval)  {
        if (FD_ISSET(obj->unblockFd, &rfds))  {
            /*
             * Our event was signalled by MessageQ_unblock().
             *
             * This is typically done during a shutdown sequence, where
             * the intention of the client would be to ignore (i.e. not fetch)
             * any pending messages in the transport's queue.
             * Thus, we shall not check for nor return any messages.
             */
            *msg = NULL;
            status = MessageQ_E_UNBLOCKED;
        }
        else {
            for (rprocId = 0; rprocId < MultiProc_getNumProcessors();
                 rprocId++) {
                if (rprocId == MultiProc_self()) {
                    continue;
                }
                if (FD_ISSET(obj->fd[rprocId], &rfds)) {
                    /* Our transport's fd was signalled: Get the message: */
                    tmpStatus = transportGet(obj->fd[rprocId], msg);
                    if (tmpStatus < 0) {
                        printf ("MessageQ_get: tranposrtshm_get failed.");
                        status = MessageQ_E_FAIL;
                    }
                }
            }
        }
    }
    else if (retval == 0) {
        *msg = NULL;
        status = MessageQ_E_TIMEOUT;
    }

    return (status);
}

/*
 * Return a count of the number of messages in the queue
 *
 * TBD: Implement as a socket ioctl, using getsockopt().  Return -1 for now.
 */
Int MessageQ_count (MessageQ_Handle handle)
{
    Int               count = -1;
#if 0
    MessageQ_Object * obj   = (MessageQ_Object *) handle;
    socklen_t         optlen;

    /*
     * TBD: Need to find a way to implement (if anyone uses it!), and
     * push down into transport..
     */

    /*
     * 2nd arg to getsockopt should be transport independent, but using
     *  SSKPROTO_SHMFIFO for now:
     */
    getsockopt(obj->fd, SSKPROTO_SHMFIFO, SSKGETOPT_GETMSGQCOUNT,
                 &count, &optlen);
#endif

    return (count);
}

/* Initializes a message not obtained from MessageQ_alloc. */
Void MessageQ_staticMsgInit (MessageQ_Msg msg, UInt32 size)
{
    /* Fill in the fields of the message */
    MessageQ_msgInit (msg);
    msg->heapId  = MessageQ_STATICMSG;
    msg->msgSize = size;
}

/*
 * Allocate a message and initialize the needed fields (note some
 * of the fields in the header are set via other APIs or in the
 * MessageQ_put function,
 */
MessageQ_Msg MessageQ_alloc (UInt16 heapId, UInt32 size)
{
    MessageQ_Msg msg       = NULL;

    /*
     * heapId not used for local alloc (as this is over a copy transport), but
     * we need to send to other side as heapId is used in BIOS transport:
     */
    msg = (MessageQ_Msg)calloc (1, size);
    MessageQ_msgInit (msg);
    msg->msgSize = size;
    msg->heapId  = heapId;

    return msg;
}

/* Frees the message back to the heap that was used to allocate it. */
Int MessageQ_free (MessageQ_Msg msg)
{
    UInt32         status = MessageQ_S_SUCCESS;

    /* Check to ensure this was not allocated by user: */
    if (msg->heapId == MessageQ_STATICMSG)  {
        status =  MessageQ_E_CANNOTFREESTATICMSG;
    }
    else {
        free (msg);
    }

    return status;
}

/* Register a heap with MessageQ. */
Int MessageQ_registerHeap (Ptr heap, UInt16 heapId)
{
    Int  status = MessageQ_S_SUCCESS;

    /* Do nothing, as this uses a copy transport: */

    return status;
}

/* Unregister a heap with MessageQ. */
Int MessageQ_unregisterHeap (UInt16 heapId)
{
    Int  status = MessageQ_S_SUCCESS;

    /* Do nothing, as this uses a copy transport: */

    return status;
}

/* Unblocks a MessageQ */
Void MessageQ_unblock (MessageQ_Handle handle)
{
    MessageQ_Object * obj   = (MessageQ_Object *) handle;
    uint64_t     buf = 1;
    int          numBytes;

    /* Write 8 bytes to awaken any threads blocked on this messageQ: */
    numBytes = write(obj->unblockFd, &buf, sizeof(buf));
}

/* Embeds a source message queue into a message. */
Void MessageQ_setReplyQueue (MessageQ_Handle handle, MessageQ_Msg msg)
{
    MessageQ_Object * obj   = (MessageQ_Object *) handle;

    msg->replyId   = (UInt16)(obj->queue);
    msg->replyProc = (UInt16)(obj->queue >> 16);
}

/* Returns the QueueId associated with the handle. */
MessageQ_QueueId MessageQ_getQueueId (MessageQ_Handle handle)
{
    MessageQ_Object * obj = (MessageQ_Object *) handle;
    UInt32            queueId;

    queueId = (obj->queue);

    return queueId;
}

/* Sets the tracing of a message */
Void MessageQ_setMsgTrace (MessageQ_Msg msg, Bool traceFlag)
{
    msg->flags = (msg->flags & ~TRACEMASK) |   (traceFlag << TRACESHIFT);
}

/*
 *  Returns the amount of shared memory used by one transport instance.
 *
 *  The MessageQ module itself does not use any shared memory but the
 *  underlying transport may use some shared memory.
 */
SizeT MessageQ_sharedMemReq (Ptr sharedAddr)
{
    SizeT memReq = 0u;

    /* Do nothing, as this is a copy transport. */

    return (memReq);
}

/*
 *  Create a socket for this remote proc, and attempt to connect.
 *
 *  Only creates a socket if one does not already exist for this procId.
 *
 *  Note: remoteProcId may be MultiProc_Self() for loopback case.
 */
Int MessageQ_attach (UInt16 remoteProcId, Ptr sharedAddr)
{
    Int     status = MessageQ_S_SUCCESS;
    int     sock;

#ifdef VERBOSE
    printf ("MessageQ_attach: remoteProcId: %d\n", remoteProcId);
#endif

    if (remoteProcId >= MultiProc_MAXPROCESSORS) {
        status = MessageQ_E_INVALIDPROCID;
        goto exit;
    }

    pthread_mutex_lock (&(MessageQ_module->gate));

    /* Only create a socket if one doesn't exist: */
    if (MessageQ_module->sock[remoteProcId] == Transport_INVALIDSOCKET)  {
        /* Create the socket for sending messages to the remote proc: */
        sock = socket(AF_RPMSG, SOCK_SEQPACKET, 0);
        if (sock < 0) {
            status = MessageQ_E_FAIL;
            printf ("MessageQ_attach: socket failed: %d, %s\n",
                       errno, strerror(errno));
        }
        else  {
#ifdef VERBOSE
            printf ("MessageQ_attach: created send socket: %d\n", sock);
#endif
            MessageQ_module->sock[remoteProcId] = sock;
            /* Attempt to connect: */
            ConnectSocket(sock, remoteProcId, MESSAGEQ_RPMSG_PORT);
        }
    }
    else {
        status = MessageQ_E_ALREADYEXISTS;
    }

    pthread_mutex_unlock (&(MessageQ_module->gate));

exit:
    return (status);
}

/*
 *  Close the socket for this remote proc.
 *
 */
Int MessageQ_detach (UInt16 remoteProcId)
{
    Int status = MessageQ_S_SUCCESS;
    int sock;

    if (remoteProcId >= MultiProc_MAXPROCESSORS) {
        status = MessageQ_E_INVALIDPROCID;
        goto exit;
    }

    pthread_mutex_lock (&(MessageQ_module->gate));

    sock = MessageQ_module->sock[remoteProcId];
    if (close (sock)) {
        status = MessageQ_E_OSFAILURE;
        printf ("MessageQ_detach: close failed: %d, %s\n",
                       errno, strerror(errno));
    }
    else {
#ifdef VERBOSE
        printf ("MessageQ_detach: closed socket: %d\n", sock);
#endif
        MessageQ_module->sock[remoteProcId] = Transport_INVALIDSOCKET;
    }

    pthread_mutex_unlock (&(MessageQ_module->gate));

exit:
    return (status);
}

/*!
 *  @brief   Grow the MessageQ table
 *
 *  @param   obj     Pointer to the MessageQ object.
 *
 *  @sa      _MessageQ_grow
 *
 */
static UInt16 _MessageQ_grow (MessageQ_Object * obj)
{
    UInt16            queueIndex = MessageQ_module->numQueues;
    UInt16            oldSize;
    MessageQ_Handle * queues;
    MessageQ_Handle * oldQueues;

    /* No parameter validation required since this is an internal function. */
    oldSize = (MessageQ_module->numQueues) * sizeof (MessageQ_Handle);

    /* Allocate larger table */
    queues = calloc (1, (oldSize + sizeof (MessageQ_Handle)));

    /* Copy contents into new table */
    memcpy (queues, MessageQ_module->queues, oldSize);

    /* Fill in the new entry */
    queues[queueIndex] = (MessageQ_Handle)obj;

    /* Hook-up new table */
    oldQueues = MessageQ_module->queues;
    MessageQ_module->queues = queues;
    MessageQ_module->numQueues++;

    /* Delete old table if not statically defined */
    if (MessageQ_module->canFreeQueues == TRUE) {
        free (oldQueues);
    }
    else {
        MessageQ_module->canFreeQueues = TRUE;
    }

#ifdef VERBOSE
    printf ("_MessageQ_grow: queueIndex: 0x%x\n", queueIndex);
#endif

    return (queueIndex);
}

/*
 * This is a helper function to initialize a message.
 */
static Void MessageQ_msgInit (MessageQ_Msg msg)
{
    msg->reserved0 = 0;  /* We set this to distinguish from NameServerMsg */
    msg->replyId   = (UInt16) MessageQ_INVALIDMESSAGEQ;
    msg->msgId     = MessageQ_INVALIDMSGID;
    msg->dstId     = (UInt16) MessageQ_INVALIDMESSAGEQ;
    msg->flags     =   MessageQ_HEADERVERSION | MessageQ_NORMALPRI;
    msg->srcProc   = MultiProc_self ();

    pthread_mutex_lock (&(MessageQ_module->gate));
    msg->seqNum  = MessageQ_module->seqNum++;
    pthread_mutex_unlock (&(MessageQ_module->gate));
}

/*
 * =============================================================================
 * Transport: Fxns kept here until need for a transport layer is realized.
 * =============================================================================
 */
/*
 * ======== transportCreateEndpoint ========
 *
 * Create a communication endpoint to receive messages.
 */
static Int transportCreateEndpoint(int * fd, UInt16 rprocId, UInt16 queueIndex)
{
    Int          status    = MessageQ_S_SUCCESS;
    int         err;

    /*  Create the socket to receive messages for this messageQ. */
    *fd = socket(AF_RPMSG, SOCK_SEQPACKET, 0);
    if (*fd < 0) {
        status = MessageQ_E_FAIL;
        printf ("transportCreateEndpoint: socket call failed: %d, %s\n",
                  errno, strerror(errno));
        goto exit;
    }

#ifdef VERBOSE
    printf ("transportCreateEndpoint: created socket: fd: %d\n", *fd);
#endif

    err = SocketBindAddr(*fd, rprocId, (UInt32)queueIndex);
    if (err < 0) {
        status = MessageQ_E_FAIL;
        printf ("transportCreateEndpoint: bind failed: %d, %s\n",
                  errno, strerror(errno));
    }

exit:
    return (status);
}

/*
 * ======== transportCloseEndpoint ========
 *
 *  Close the communication endpoint.
 */
static Int transportCloseEndpoint(int fd)
{
    Int status = MessageQ_S_SUCCESS;

#ifdef VERBOSE
    printf ("transportCloseEndpoint: closing socket: %d\n", fd);
#endif

    /* Stop communication to this socket:  */
    close(fd);

    return (status);
}

/*
 * ======== transportGet ========
 *  Retrieve a message waiting in the socket's queue.
*/
static Int transportGet(int sock, MessageQ_Msg * retMsg)
{
    Int           status    = MessageQ_S_SUCCESS;
    MessageQ_Msg  msg;
    struct sockaddr_rpmsg fromAddr;  // [Socket address of sender]
    unsigned int  len;
    int           byteCount;

    /*
     * We have no way of peeking to see what message size we'll get, so we
     * allocate a message of max size to receive contents from the rpmsg socket
     * (currently, a copy transport)
     */
    msg = MessageQ_alloc (0, MESSAGEQ_RPMSG_MAXSIZE);
    if (!msg)  {
        status = MessageQ_E_MEMORY;
        goto exit;
    }

    memset(&fromAddr, 0, sizeof(fromAddr));
    len = sizeof(fromAddr);

    byteCount = recvfrom(sock, msg, MESSAGEQ_RPMSG_MAXSIZE, 0,
                      (struct sockaddr *)&fromAddr, &len);
    if (len != sizeof(fromAddr)) {
        printf("recvfrom: got bad addr len (%d)\n", len);
        status = MessageQ_E_FAIL;
        goto exit;
    }
    if (byteCount < 0) {
        printf("recvfrom failed: %s (%d)\n", strerror(errno), errno);
        status = MessageQ_E_FAIL;
        goto exit;
    }
    else {
         /* Update the allocated message size (even though this may waste space
          * when the actual message is smaller than the maximum rpmsg size,
          * the message will be freed soon anyway, and it avoids an extra copy).
          */
         msg->msgSize = byteCount;

         /*
          * If the message received was statically allocated, reset the
          * heapId, so the app can free it.
          */
         if (msg->heapId == MessageQ_STATICMSG)  {
             msg->heapId = 0;  /* for a copy transport, heap id is 0. */
         }
    }

#ifdef VERBOSE
    printf ("transportGet: recvfrom socket: fd: %d\n", sock);
    printf("\tReceived a msg: byteCount: %d, rpmsg addr: %d, rpmsg proc: %d\n",
                 byteCount, fromAddr.addr, fromAddr.vproc_id);
    printf("\tMessage Id: %d, Message size: %d\n", msg->msgId, msg->msgSize);
#endif

    *retMsg = msg;

exit:
    return (status);
}

/*
 * ======== transportPut ========
 *
 * Calls the socket API sendto() on the socket associated with
 * with this destination procID.
 * Currently, both local and remote messages are sent via the Socket ABI, so
 * no local object lists are maintained here.
*/
static Int transportPut(MessageQ_Msg msg, UInt16 dstId, UInt16 dstProcId)
{
    Int     status    = MessageQ_S_SUCCESS;
    int     sock;
    int     err;

    /*
     * Retrieve the socket for the AF_SYSLINK protocol associated with this
     * transport.
     */
    sock = MessageQ_module->sock[dstProcId];

#ifdef VERBOSE
    printf("Sending msgId: %d via sock: %d\n", msg->msgId, sock);
#endif
    err = send(sock, msg, msg->msgSize, 0);
    if (err < 0) {
        printf ("transportPut: send failed: %d, %s\n",
                  errno, strerror(errno));
        status = MessageQ_E_FAIL;
    }

    /*
     * Free the message, as this is a copy transport, we maintain MessageQ
     * semantics.
     */
    MessageQ_free (msg);

    return (status);
}
