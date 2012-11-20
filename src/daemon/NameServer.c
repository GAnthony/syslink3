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
/*!
 *  @file       NameServer.c
 *
 *  @brief      NameServer Manager
 *
 */


/* Standard headers */
#include <Std.h>

/* Linux specific header files, replacing OSAL: */
#include <pthread.h>

/* Socket Headers */
#include <sys/queue.h>
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

/* SysLink Socket Protocol        Family */
#include <net/rpmsg.h>

/* Module level headers */
#include <ti/ipc/NameServer.h>
#include <ti/ipc/MultiProc.h>
#include <_MultiProc.h>

/* Internal stuff: */
#include <_NameServer.h>
#include <_NameServerRemoteRpmsg.h>

/* Socket utils: */
#include <SocketFxns.h>

#include <_lad.h>

#define MESSAGEQ_RPMSG_PORT       61
#define NAME_SERVER_RPMSG_ADDR    0

#define INVALIDSOCKET     (-1)

#if defined (__cplusplus)
extern "C" {
#endif


/* =============================================================================
 * Structures & Enums
 * =============================================================================
 */

/* Structure of entry in Name/Value table */
typedef struct NameServer_TableEntry_tag {
    CIRCLEQ_ENTRY(NameServer_TableEntry_tag) elem;
    /* List element */
    UInt32                    hash;
    /* Hash value */
    String                    name;
    /* Name portion of the name/value pair. */
    UInt                      len;
    /* Length of the value field. */
    Ptr                       value;
    /* Value portion of the name/value entry. */
    Bool                      collide;
    /* Does the hash collide? */
    struct NameServer_TableEntry_tag * next;
    /* Pointer to the next entry, used incase of collision only */
} NameServer_TableEntry;

/* Structure defining object for the NameServer */
struct NameServer_Object {
    CIRCLEQ_ENTRY(NameServer_Object) elem;
    CIRCLEQ_HEAD(dummy2, NameServer_TableEntry_tag) nameList;
    String             name;            /* name of the instance */
    NameServer_Params  params;          /* the parameter structure */
    UInt32             count;           /* count of entries */
    pthread_mutex_t    gate;            /* crit sect gate */
} NameServer_Object;

/* structure for NameServer module state */
typedef struct NameServer_ModuleObject {
    CIRCLEQ_HEAD(dummy1, NameServer_Object) objList;
    Int32               refCount;
    int                 sendSock[MultiProc_MAXPROCESSORS];
    /* Sockets for sending to remote proc nameserver ports: */
    int                 recvSock[MultiProc_MAXPROCESSORS];
    /* Sockets for recving from remote proc nameserver ports: */
    pthread_t           listener;
    /* Listener thread for NameServer replies and requests. */
    int                 unblockFd;
    /* Event to post to exit listener. */
    int                 waitFd;
    /* Event to post to NameServer_get. */
    NameServerMsg       nsMsg;
    /* NameServer Message cache. */
    NameServer_Params   defInstParams;
    /* Default instance paramters */
    pthread_mutex_t     modGate;
} NameServer_ModuleObject;

#define CIRCLEQ_destruct(head) { \
        (head)->cqh_first = NULL; \
        (head)->cqh_last = NULL; \
}

#define CIRCLEQ_elemClear(elem) { \
        (elem)->cqe_next = (elem)->cqe_prev = (Void *)(elem); \
}

#define CIRCLEQ_traverse(x, y, tag) \
        for (x = (y)->cqh_first; x != (struct tag *)(y); x = x->elem.cqe_next)

/* =============================================================================
 *  Globals
 * =============================================================================
 */
/*
 * NameServer_state
 *
 * Make the module gate "recursive" since NameServer_getHandle() needs to
 * use it and NameServer_create() needs to hold it around its call to
 * NameServer_getHandle().  Also, use the static initializer instead of a
 * run-time init call, so we can use this gate immediately in _setup().
 */
static NameServer_ModuleObject NameServer_state = {
    .defInstParams.maxRuntimeEntries = 0u,
    .defInstParams.tableHeap         = NULL,
    .defInstParams.checkExisting     = TRUE,
    .defInstParams.maxValueLen       = 0u,
    .defInstParams.maxNameLen        = 16u,
// only _NP (non-portable) type available in CG tools which we're using
    .modGate                         = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP,
//    .modGate                         = PTHREAD_RECURSIVE_MUTEX_INITIALIZER,
    .refCount                        = 0
};

static NameServer_ModuleObject * NameServer_module = &NameServer_state;

static const UInt32 stringCrcTab[256u] = {
  0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f,
  0xe963a535, 0x9e6495a3, 0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
  0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,
  0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
  0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9,
  0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
  0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b, 0x35b5a8fa, 0x42b2986c,
  0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
  0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,
  0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
  0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d, 0x76dc4190, 0x01db7106,
  0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
  0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d,
  0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
  0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
  0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
  0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7,
  0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
  0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa,
  0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
  0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
  0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
  0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84,
  0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
  0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
  0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
  0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e,
  0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
  0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55,
  0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
  0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28,
  0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
  0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f,
  0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
  0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
  0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
  0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69,
  0x616bffd3, 0x166ccf45, 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
  0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc,
  0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
  0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693,
  0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
  0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d,
};

static UInt32 stringHash(String s)
{
    UInt32 hash = strlen(s);
    UInt32 i;

    for (i = 0; i < strlen(s); i++) {
        hash = (hash >> 8u) ^ stringCrcTab[(hash & 0xff)] ^ s[i];
    }

    return (hash);
}

static void NameServerRemote_processMessage(NameServerMsg * msg, UInt16 procId)
{
    NameServer_Handle handle;
    Int               status = NameServer_E_FAIL;
    int               err;
    uint64_t          buf = 1;
    int               numBytes;
    int               waitFd = NameServer_module->waitFd;

    if (msg->request == NAMESERVER_REQUEST) {
        LOG2("NameServer Request: instanceName: %s, name: %s\n",
             (String)msg->instanceName, (String)msg->name)

        /*
         *  Message is a request. Lookup name in NameServer table.
         *  Send a response message back to source processor.
         */
        handle = NameServer_getHandle((String)msg->instanceName);

        if (handle != NULL) {
            /* Search for the NameServer entry */
            LOG0("Calling NameServer_getLocalUInt32...\n")
            status = NameServer_getLocalUInt32(handle,
                     (String)msg->name, &msg->value);
        }

        LOG2("NameServer Response: instanceName: %s, name: %s,",
             (String)msg->instanceName, (String)msg->name)
        /* set the request status */
        if (status < 0) {
            LOG1(" Value not found, status: %d\n", status)
            msg->requestStatus = 0;
        }
        else {
            msg->requestStatus = 1;
            LOG1(" Value: 0x%x\n", msg->value)
        }

        /* specify message as a response */
        msg->request = NAMESERVER_RESPONSE;
        msg->reserved = NAMESERVER_MSG_TOKEN;

        /* send response message to remote processor */
        err = send(NameServer_module->sendSock[procId], msg,
                   sizeof(NameServerMsg), 0);
        if (err < 0) {
            LOG2("NameServer: send failed: %d, %s\n", errno, strerror(errno))
        }
    }
    else {
        LOG2("NameServer Reply: instanceName: %s, name: %s",
             (String)msg->instanceName, (String)msg->name)
        LOG1(", value: 0x%x\n", msg->value)

        /* Save the response message.  */
        memcpy(&NameServer_module->nsMsg, msg, sizeof(NameServerMsg));

        /* Post the eventfd upon which NameServer_get() is waiting */
        numBytes = write(waitFd, &buf, sizeof(uint64_t));
    }
}


static void *listener_cb(void *arg)
{
    fd_set rfds;
    int ret = 0, maxfd;
    UInt16 procId;
    struct  sockaddr_rpmsg  fromAddr;
    unsigned int len;
    NameServerMsg msg;
    int     byteCount;
    UInt16  numProcs = MultiProc_getNumProcessors();
    int     sock;

    LOG0("listener_cb: Entered Listener thread.\n")

    do {
        /* Wait for NameServer messages or unblockFd notification */
        FD_ZERO(&rfds);
        FD_SET(NameServer_module->unblockFd, &rfds);
        maxfd = NameServer_module->unblockFd;
        for (procId = 0; procId < numProcs; procId++) {
            if (procId == MultiProc_self() ||
                NameServer_module->recvSock[procId] == INVALIDSOCKET) {
                continue;
            }
            sock = NameServer_module->recvSock[procId];
            FD_SET(sock, &rfds);
            maxfd = MAX(sock, maxfd);
        }

        maxfd = maxfd + 1;
        LOG2("NameServer: waiting for unblockFd: %d, and socks: maxfd: %d\n",
             NameServer_module->unblockFd, maxfd)
        ret = select(maxfd, &rfds, NULL, NULL, NULL);
        if (ret == -1) {
            LOG0("listener_cb: select failed.")
            break;
        }
        LOG0("NameServer: back from select()\n")

        for (procId = 0; procId < numProcs; procId++) {
            if (procId == MultiProc_self() ||
                NameServer_module->recvSock[procId] == INVALIDSOCKET) {
                continue;
            }
            sock = NameServer_module->recvSock[procId];
            if (FD_ISSET(sock, &rfds)) {
                LOG1("NameServer: Listener got NameServer message "
                     "from sock: %d!\n", sock);
                /* Get NameServer message and process: */
                memset(&fromAddr, 0, sizeof(fromAddr));
                len = sizeof(fromAddr);

                byteCount = recvfrom(sock, &msg, sizeof(NameServerMsg), 0,
                                (struct sockaddr *)&fromAddr, &len);
                if (len != sizeof(fromAddr)) {
                    LOG1("recvfrom: got bad addr len (%d)\n", len)
                    break;
                }
                if (byteCount < 0) {
                    LOG2("recvfrom failed: %s (%d)\n", strerror(errno), errno)
                    break;
                }
                else {
                    LOG1("listener_cb: recvfrom socket: fd: %d\n", sock)
                    LOG2("\tReceived ns msg: byteCount: %d, from addr: %d, ",
                         byteCount, fromAddr.addr)
                    LOG1("from vproc: %d\n", fromAddr.vproc_id)
                    NameServerRemote_processMessage(&msg, procId);
                }
            }
        }
        if (FD_ISSET(NameServer_module->unblockFd, &rfds)) {
            /* We are told to unblock and exit: */
            LOG0("NameServer: Listener thread exiting\n")
            break;
        }
    } while (1);

    return ((void *)ret);
}

/* =============================================================================
 * APIS
 * =============================================================================
 */

/* Function to setup the nameserver module. */
Int NameServer_setup(Void)
{
    Int    status = NameServer_S_SUCCESS;
    int    err;
    int    sock;
    int    ret;
    UInt16 procId;
    UInt16 numProcs;

    pthread_mutex_lock(&NameServer_module->modGate);

    if (NameServer_module->refCount != 0) {
        status = NameServer_S_ALREADYSETUP;
        goto exit;
    }
    NameServer_module->refCount++;

    numProcs = MultiProc_getNumProcessors();

    NameServer_module->unblockFd = eventfd(0, 0);
    if (NameServer_module->unblockFd < 0) {
        status = NameServer_E_FAIL;
        LOG0("NameServer_setup: failed to create unblockFd.\n")
        goto exit;
    }

    NameServer_module->waitFd = eventfd(0, 0);
    if (NameServer_module->waitFd < 0) {
        status = NameServer_E_FAIL;
        LOG0("NameServer_setup: failed to create waitFd.\n")
        goto exit;
    }

    for (procId = 0; procId < numProcs; procId++) {
        NameServer_module->sendSock[procId] = INVALIDSOCKET;
        NameServer_module->recvSock[procId] = INVALIDSOCKET;

        /* Only support NameServer to remote procs: */
        if (procId == MultiProc_self()) {
            continue;
        }

        /* Create the socket for sending messages to each remote proc: */
        sock = socket(AF_RPMSG, SOCK_SEQPACKET, 0);
        if (sock < 0) {
            status = NameServer_E_FAIL;
            LOG2("NameServer_setup: socket failed: %d, %s\n",
                 errno, strerror(errno))
        }
        else  {
            LOG1("NameServer_setup: created send socket: %d\n", sock)
            err = ConnectSocket(sock, procId, MESSAGEQ_RPMSG_PORT);
            if (err < 0) {
                status = NameServer_E_FAIL;
                LOG2("NameServer_setup: connect failed: %d, %s\n",
                     errno, strerror(errno))

                LOG1("    closing send socket: %d\n", sock)
                close(sock);
            }
            else {
                NameServer_module->sendSock[procId] = sock;
            }
        }

        /* Create the socket for recving messages from each remote proc: */
        sock = socket(AF_RPMSG, SOCK_SEQPACKET, 0);
        if (sock < 0) {
            status = NameServer_E_FAIL;
            LOG2("NameServer_setup: socket failed: %d, %s\n",
                 errno, strerror(errno))
        }
        else  {
            LOG1("NameServer_setup: created recv socket: %d\n", sock)

            err = SocketBindAddr(sock, procId, NAME_SERVER_RPMSG_ADDR);
            if (err < 0) {
               status = NameServer_E_FAIL;
               LOG2("NameServer_setup: bind failed: %d, %s\n",
                    errno, strerror(errno))

                LOG1("    closing recv socket: %d\n", sock)
                close(sock);
            }
            else {
               NameServer_module->recvSock[procId] = sock;
            }
        }
    }

    /* Construct the list object */
    CIRCLEQ_INIT(&NameServer_module->objList);

    /* Create the listener thread: */
    LOG0("NameServer_setup: creating listener thread\n")
    ret = pthread_create(&NameServer_module->listener, NULL, listener_cb, NULL);
    if (ret) {
        LOG1("NameServer_setup: can't spawn thread: %s\n", strerror(ret))
        LOG0("NameServer_setup: eventfd failed");

        status = NameServer_E_FAIL;
    }

exit:
    pthread_mutex_unlock(&NameServer_module->modGate);

    return (status);
}

/*! Function to destroy the nameserver module. */
Int NameServer_destroy(void)
{
    Int      status    = NameServer_S_SUCCESS;
    UInt16   numProcs = MultiProc_getNumProcessors();
    UInt16   procId;
    int      sock;
    uint64_t buf = 1;
    int      numBytes;

    pthread_mutex_lock(&NameServer_module->modGate);

    if (NameServer_module->refCount == 0) {
        LOG0("NameServer_destroy(): bad refCount: 0 (should be > 0)\n")
        status = NameServer_E_FAIL;

       goto exit;
    }

    for (procId = 0; procId < numProcs; procId++) {
        /* Only support NameServer to remote procs: */
        if (procId == MultiProc_self()) {
            continue;
        }

        /* Close the socket: */
        sock = NameServer_module->sendSock[procId];
        if (sock != INVALIDSOCKET) {
            LOG1("NameServer_destroy: closing socket: %d\n", sock)
            close(sock);
            NameServer_module->sendSock[procId] = INVALIDSOCKET;
        }
        /* Close the socket: */
        sock = NameServer_module->recvSock[procId];
        if (sock != INVALIDSOCKET) {
            LOG1("NameServer_destroy: closing socket: %d\n", sock)
            close(sock);
            NameServer_module->recvSock[procId] = INVALIDSOCKET;
        }
    }

    CIRCLEQ_destruct(&NameServer_module->objList);

    /* Unblock the NameServer listener thread: */
    LOG0("NameServer_destroy: unblocking listener...\n")
    numBytes = write(NameServer_module->unblockFd, &buf, sizeof(uint64_t));

    /* Join: */
    LOG0("NameServer_destroy: joining listener thread...\n")
    pthread_join(NameServer_module->listener, NULL);

    close(NameServer_module->unblockFd);
    close(NameServer_module->waitFd);

    NameServer_module->refCount--;

exit:
    pthread_mutex_unlock(&NameServer_module->modGate);

    return (status);
}

/* Function to retrieve a NameServer handle from name. */
NameServer_Handle NameServer_getHandle(String name)
{
    NameServer_Handle handle = NULL;
    Bool              found = FALSE;
    struct NameServer_Object * elem;

    assert(name != NULL);
    assert(NameServer_module->refCount != 0);

    pthread_mutex_lock(&NameServer_module->modGate);

    /* Lookup handle from name: */
    CIRCLEQ_traverse(elem, &NameServer_module->objList, NameServer_Object) {
        handle = (NameServer_Handle) elem;
        if (strcmp(handle->name, name) == 0) {
            found = TRUE;
            break;
        }
    }

    if (found == FALSE) {
        handle = NULL;
    }

    pthread_mutex_unlock(&NameServer_module->modGate);

    return (handle);
}


/* Function to create a name server. */
NameServer_Handle NameServer_create(String name,
                                    const NameServer_Params * params)
{
    NameServer_Handle handle = NULL;
    pthread_mutexattr_t mutex_attr;

    assert(params != NULL);
    assert(name != NULL);
    assert(NameServer_module->refCount != 0);

    LOG1("NameServer_create(): '%s'\n", name)

    pthread_mutex_lock(&NameServer_module->modGate);

    if (params->maxValueLen > sizeof(UInt32)) {
        LOG1("NameServer_create: params->maxValueLen (%d) too big for now\n", params->maxValueLen)
       /* Can't handle more than UInt32 at this time: */
       goto leave;
    }

    /* check if the name is already created or not */
    if (NameServer_getHandle(name)) {
        LOG0("NameServer_create NameServer_E_INVALIDARG Name is in use!\n")
        handle = NULL;
        goto leave;
    }
    else {
        handle = (NameServer_Handle)calloc(1, sizeof(NameServer_Object));
    }

    if (!handle) {
        LOG0("NameServer_create: NameServer_Handle alloc failed\n")
        goto leave;
    }

    handle->name = (String)malloc(strlen(name) + 1u);
    if (!handle->name) {
        LOG0("NameServer_create: instance name alloc failed\n")
        goto cleanup;
    }
    strncpy(handle->name, name, strlen (name) + 1u);
    memcpy((Ptr) &handle->params, (Ptr) params, sizeof(NameServer_Params));

    if (params->maxValueLen < sizeof(UInt32)) {
        handle->params.maxValueLen = sizeof(UInt32);
    }
    else {
        handle->params.maxValueLen = params->maxValueLen;
    }

    CIRCLEQ_INIT(&handle->nameList);
    handle->count = 0u;

    /* Put in the local list */
    CIRCLEQ_elemClear(&handle->elem);
    CIRCLEQ_INSERT_HEAD(&NameServer_module->objList, handle, elem);

    /*
     * NameServer_removeEntry() enters gate and is called by
     * NameServer_remove() while holding the gate.
     */
    pthread_mutexattr_settype(&mutex_attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&handle->gate, &mutex_attr);

    goto leave;

cleanup:
    free(handle);
    handle = NULL;

leave:
    pthread_mutex_unlock(&NameServer_module->modGate);

    return (handle);
}


/* Function to delete a name server. */
Int NameServer_delete(NameServer_Handle * handle)
{
    Int status = NameServer_S_SUCCESS;

    assert(handle != NULL);
    assert(*handle != NULL);
    assert((*handle)->count == 0);
    assert(NameServer_module->refCount != 0);

    pthread_mutex_lock(&NameServer_module->modGate);

    if ((*handle)->count == 0) {
        CIRCLEQ_REMOVE(&NameServer_module->objList, *handle, elem);

        if ((*handle)->name != NULL) {
            free((*handle)->name);
            (*handle)->name = NULL;
        }

        CIRCLEQ_destruct(&(*handle)->nameList);

        free((*handle));
        (*handle) = NULL;
    }

    pthread_mutex_unlock(&NameServer_module->modGate);

    return (status);
}

/* Adds a variable length value into the local NameServer table */
Ptr NameServer_add(NameServer_Handle handle, String name, Ptr buf, UInt len)
{
    Int                 status = NameServer_S_SUCCESS;
    NameServer_TableEntry * node = NULL;
    NameServer_TableEntry * new_node = NULL;
    Bool                found = FALSE;
    UInt32              hash;

    assert(handle != NULL);
    assert(name     != NULL);
    assert(buf      != NULL);
    assert(len      != 0);
    assert(NameServer_module->refCount != 0);

    /* Calculate the hash */
    hash = stringHash(name);

    pthread_mutex_lock(&handle->gate);

    /* Traverse the list to find duplicate check */
    CIRCLEQ_traverse(node, &handle->nameList, NameServer_TableEntry_tag) {
        /* Hash matches */
        if (node->hash == hash) {
            /* If the name matches, incase hash is duplicate */
            if (strcmp(node->name, name) == 0) {
                if (handle->params.checkExisting == TRUE) {
                    status = NameServer_E_INVALIDARG;
                    LOG1("NameServer_add: '%s' - duplicate entry found!\n", name)
                    break;
                }
            }
            else {
                found = TRUE;
                break;
            } /* name does not match */
        } /* hash does not match */
    } /* CIRCLEQ_traverse */

    if (status != NameServer_S_SUCCESS) {
        new_node = NULL;
        goto exit;
    }

    /* Now add the new entry. */
    new_node = (NameServer_TableEntry *)malloc(sizeof(NameServer_TableEntry));
    if (new_node == NULL) {
        status = NameServer_E_MEMORY;
        LOG1("NameServer_add: %d - malloc new_node failed!\n", status)

        goto exit;
    }

    new_node->hash    = hash;
    new_node->collide = found; /* Indicate if there is a collision*/
    new_node->len     = len;
    new_node->next    = NULL;
    new_node->name = (String)malloc(strlen(name) + 1u);
    new_node->value  = (Ptr)malloc(len);
    strncpy(new_node->name, name, strlen(name) + 1u);
    memcpy((Ptr)new_node->value, (Ptr)buf, len);

    if (found == TRUE) {
        /* If hash is found, need to stitch the list to link the
         * new node to the existing node with the same hash.
         */
        new_node->next = node->next;
        node->next = new_node;
    }
    else {
        /* put the new node into the list */
        CIRCLEQ_INSERT_HEAD(&handle->nameList, new_node, elem);
    }

    handle->count++;

    LOG2("NameServer_add: Entered key: '%s', data: 0x%x\n",
         name, *(UInt32 *)buf)

exit:
    pthread_mutex_unlock(&handle->gate);

    return (new_node);
}


/* Function to add a UInt32 value into a name server. */
Ptr NameServer_addUInt32(NameServer_Handle handle, String name, UInt32 value)
{
    Ptr entry = NULL;

    assert(handle != NULL);
    assert(name   != NULL);
    assert(NameServer_module->refCount != 0);

    entry = NameServer_add(handle, name, &value, sizeof(UInt32));

    return (entry);
}

/* Function to remove a name/value pair from a name server. */
Int NameServer_remove(NameServer_Handle handle, String name)
{
    Int                 status = NameServer_S_SUCCESS;
    NameServer_TableEntry *prev = NULL;
    NameServer_TableEntry *temp = NULL;
    NameServer_TableEntry *node = NULL;
    Bool                done   = FALSE;
    UInt32              hash;

    assert(handle != NULL);
    assert(name   != NULL);
    assert(NameServer_module->refCount != 0);

    /* Calculate the hash */
    hash = stringHash(name);

    pthread_mutex_lock(&handle->gate);

    /* Traverse the list to find duplicate check */
    CIRCLEQ_traverse(node, &handle->nameList, NameServer_TableEntry_tag) {
        /* Hash matchs */
        if (node->hash == hash) {
            if (node->collide == TRUE) {
                if (strcmp(node->name, name) == 0){
                    free(node->value);
                    free(node->name);
                    memcpy((Ptr)node, (Ptr) node->next,
                           sizeof(NameServer_TableEntry));
                    node->next = node->next->next;
                    free(node->next);
                    handle->count--;
                    done = TRUE;
                    break;
                }
                else {
                    prev = node;
                    temp = node->next;
                    while (temp) {
                        if (strcmp(temp->name, name) == 0){
                            free(temp->value);
                            free(temp->name);
                            prev->next = temp->next;
                            free(temp);
                            handle->count--;
                            done = TRUE;
                            break;
                        }
                        temp = temp->next;
                    }
                    break;
                }
            }
            else {
                NameServer_removeEntry(handle, (Ptr)node);

                done = TRUE;
                break;
            }
        }
    }

    if (done == FALSE) {
        status = NameServer_E_INVALIDARG;
        LOG1("NameServer_remove %d Entry not found!\n", status)
    }

    pthread_mutex_unlock(&handle->gate);

    return (status);
}

/* Function to remove a name/value pair from a name server. */
Int NameServer_removeEntry(NameServer_Handle handle, Ptr entry)
{
    Int  status = NameServer_S_SUCCESS;
    NameServer_TableEntry * node;

    assert(handle != NULL);
    assert(entry  != NULL);
    assert(NameServer_module->refCount != 0);

    pthread_mutex_lock(&handle->gate);

    node = (NameServer_TableEntry *)entry;

    free(node->value);
    free(node->name);
    CIRCLEQ_REMOVE(&handle->nameList, node, elem);
    free(node);
    handle->count--;

    pthread_mutex_unlock(&handle->gate);

    return (status);
}


/* Initialize this config-params structure with supplier-specified
 * defaults before instance creation.
 */
Void NameServer_Params_init(NameServer_Params * params)
{
    assert(params != NULL);

    memcpy(params, &(NameServer_module->defInstParams),
           sizeof (NameServer_Params));
}


Int NameServer_getRemote(NameServer_Handle handle,
                     String            name,
                     Ptr               value,
                     UInt32 *          len,
                     UInt16            procId)
{
    Int status = NameServer_S_SUCCESS;
    struct NameServer_Object *obj = (struct NameServer_Object *)(handle);
    NameServerMsg nsMsg;
    NameServerMsg *replyMsg;
    fd_set rfds;
    int ret = 0, sock, maxfd, waitFd;
    struct timeval tv;
    uint64_t buf = 1;
    int numBytes;
    int err;

    /* Set Timeout to wait: */
    tv.tv_sec = NAMESERVER_GET_TIMEOUT;
    tv.tv_usec = 0;

    /* Create request message and send to remote: */
    sock = NameServer_module->sendSock[procId];
    LOG1("NameServer_getRemote: Sending request via sock: %d\n", sock)

    /* Create request message and send to remote processor: */
    nsMsg.reserved = NAMESERVER_MSG_TOKEN;
    nsMsg.request = NAMESERVER_REQUEST;
    nsMsg.requestStatus = 0;

    strncpy((char *)nsMsg.instanceName, obj->name, strlen(obj->name) + 1);
    strncpy((char *)nsMsg.name, name, strlen(name) + 1);

    LOG2("NameServer_getRemote: Requesting from procId %d, %s:",
           procId, (String)nsMsg.instanceName)
    LOG1("%s...\n", (String)nsMsg.name)

    err = send(sock, &nsMsg, sizeof(NameServerMsg), 0);
    if (err < 0) {
        LOG2("NameServer_getRemote: send failed: %d, %s\n",
             errno, strerror(errno))
        status = NameServer_E_FAIL;
        goto exit;
    }

    /* Block on waitFd for signal from listener thread: */
    waitFd = NameServer_module->waitFd;
    FD_ZERO(&rfds);
    FD_SET(waitFd, &rfds);
    maxfd = waitFd + 1;
    LOG1("NameServer_getRemote: pending on waitFd: %d\n", waitFd)
    ret = select(maxfd, &rfds, NULL, NULL, &tv);
    if (ret == -1) {
        LOG0("NameServer_getRemote: select failed.")
        status = NameServer_E_FAIL;
        goto exit;
    }
    else if (!ret) {
        LOG0("NameServer_getRemote: select timed out.\n")
        status = NameServer_E_TIMEOUT;
        goto exit;
    }

    if (FD_ISSET(waitFd, &rfds)) {
        /* Read, just to balance the write: */
        numBytes = read(waitFd, &buf, sizeof(uint64_t));

        /* Process response: */
        replyMsg = &NameServer_module->nsMsg;

        if (replyMsg->requestStatus) {
            /* name is found */
            /* set the contents of value */
            *(UInt32 *)value = (UInt32)replyMsg->value;

            LOG2("NameServer_getRemote: Reply from: %d, %s:",
                 procId, (String)replyMsg->instanceName)
            LOG2("%s, value: 0x%x...\n",
                 (String)replyMsg->name, *(UInt32 *)value)
            goto exit;
        }
        else {
            /* name is not found */
            LOG2("NameServer_getRemote: value for %s:%s not found.\n",
                 (String)replyMsg->instanceName, (String)replyMsg->name)

            /* set status to not found */
            status = NameServer_E_NOTFOUND;
        }
    }

exit:
    return (status);
}

/* Function to retrieve the value portion of a name/value pair from
 * local table.
 */
Int NameServer_get(NameServer_Handle handle,
               String            name,
               Ptr               value,
               UInt32 *          len,
               UInt16            procId[])
{
    Int status = NameServer_S_SUCCESS;
    UInt16 numProcs = MultiProc_getNumProcessors();
    UInt32 i;

    /*
     * BIOS side uses a gate (mutex) to protect NameServer_module->nsMsg, but
     * since this goes in a daemon, it will not be necessary.
     */

    if (procId == NULL) {
        status = NameServer_getLocal(handle, name, value, len);
        if (status == NameServer_E_NOTFOUND) {
            for (i = 0; i < numProcs; i++) {
                /* getLocal call already covers "self", keep going */
                if (i == MultiProc_self()) {
                    continue;
                }

                status = NameServer_getRemote(handle, name, value, len, i);

                if ((status >= 0) ||
                    ((status < 0) && (status != NameServer_E_NOTFOUND))) {
                    break;
                }
            }
        }
    }
    else {
        /*
         *  Search the query list. It might contain the local proc
         *  somewhere in the list.
         */
        i = 0;
        while (procId[i] != MultiProc_INVALIDID) {
            if (procId[i] == MultiProc_self()) {
                status = NameServer_getLocal(handle, name, value, len);
            }
            else {
                status = NameServer_getRemote(handle, name, value, len, i);
            }

            if ((status >= 0) ||
                ((status < 0) && (status != NameServer_E_NOTFOUND))) {
                break;
            }

            i++;
        }
    }

    return (status);
}

/* Gets a 32-bit value by name */
Int NameServer_getUInt32(NameServer_Handle handle,
                     String            name,
                     Ptr               value,
                     UInt16            procId[])
{
    Int  status;
    UInt32 len = sizeof(UInt32);

    assert(handle != NULL);
    assert(name   != NULL);
    assert(value  != NULL);
    assert(NameServer_module->refCount != 0);

    status = NameServer_get(handle, name, value, &len, procId);

    return (status);
}

/* Function to Retrieve the value portion of a name/value pair from
 * local table.
 */
Int NameServer_getLocal(NameServer_Handle handle,
                    String            name,
                    Ptr               value,
                    UInt32 *          len)
{
    Int status = NameServer_E_NOTFOUND;
    NameServer_TableEntry * node = NULL;
    NameServer_TableEntry * temp = NULL;
    Bool done   = FALSE;
    UInt32 length;
    UInt32 hash;

    assert(handle != NULL);
    assert(name   != NULL);
    assert(value  != NULL);
    assert(len    != NULL);
    assert(NameServer_module->refCount != 0);

    length = *len;

    /* Calculate the hash */
    hash = stringHash(name);

    pthread_mutex_lock(&handle->gate);

    /* Traverse the list to find duplicate check */
    CIRCLEQ_traverse(node, &handle->nameList, NameServer_TableEntry_tag) {
        if (node->hash == hash) {
            if (node->collide == TRUE) {
                temp = node;
                while (temp) {
                    if (strcmp(temp->name, name) == 0u){
                        if (length <= node->len) {
                            memcpy(value, node->value, length);
                            *len = length;
                        }
                        else {
                            memcpy(value, node->value, node->len);
                            *len = node->len;
                        }
                        done = TRUE;
                        break;
                    }
                    temp = temp->next;
                }
                break;
            }
            else {
                if (length <= node->len) {
                    memcpy(value, node->value, length);
                    *len = length;
                }
                else {
                    memcpy(value, node->value, node->len);
                    *len = node->len;
                }
                done = TRUE;
                break;
            }
        }
    }

    pthread_mutex_unlock(&handle->gate);

    if (done == FALSE) {
        LOG1("NameServer_getLocal: entry key: '%s' not found!\n", name)
    }
    else {
        LOG2("NameServer_getLocal: Found entry key: '%s', data: 0x%x\n",
             node->name, (UInt32)node->value)
        status = NameServer_S_SUCCESS;
    }

    return (status);
}

/*
 *  Gets a 32-bit value by name from the local table
 *
 *  If the name is found, the 32-bit value is copied into the value
 *  argument and a success status is returned.
 *
 *  If the name is not found, zero is returned in len and the contents
 *  of value are not modified. Not finding a name is not considered
 *  an error.
 *
 *  This function only searches the local name/value table.
 *
 */
Int NameServer_getLocalUInt32(NameServer_Handle handle, String name, Ptr value)
{
    Int                 status;
    UInt32              len    = sizeof(UInt32);

    assert(handle != NULL);
    assert(name   != NULL);
    assert(value  != NULL);
    assert(NameServer_module->refCount != 0);

    LOG0("NameServer_getLocalUInt32: calling NameServer_getLocal()...\n")
    status = NameServer_getLocal(handle, name, value, &len);

    return (status);
}


#if defined (__cplusplus)
}
#endif /* defined (__cplusplus) */

