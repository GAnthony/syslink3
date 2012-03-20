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

/* Hash table: */
#include <search.h>

/* SysLink Socket Protocol	Family */
#include <net/rpmsg.h>

/* Module level headers */
#include <ti/ipc/NameServer.h>
#include <ti/ipc/MultiProc.h>
#include <_MultiProc.h>

/* Internal stuff: */
#include <_NameServer.h>
#include <_NameServerRemoteRpmsg.h>

/* Socket utils: */
#include "socketfxns.h"

#define MESSAGEQ_RPMSG_PORT       61
#define NAME_SERVER_RPMSG_ADDR    0

/* Hash size should be tuned to expected number of items created, but... */
#define NUM_HASH_ELEMENTS 100
#define INVALIDSOCKET     (-1)

#if defined (__cplusplus)
extern "C" {
#endif


/* =============================================================================
 * Structures & Enums
 * =============================================================================
 */

/* Structure defining object for the NameServer */
struct NameServer_Object {
    String             name;            /* name of the instance */
    struct hsearch_data *htab;		/* Hash table to store name/values */
    NameServer_Params  params;          /* the parameter structure */
    UInt32             count;           /* count of entries */
} NameServer_Object;

/* structure for NameServer module state */
typedef struct NameServer_ModuleObject {
    Int                 send_sock[MultiProc_MAXPROCESSORS];
    /* Sockets for sending to remote proc nameserver ports: */
    Int                 recv_sock[MultiProc_MAXPROCESSORS];
    /* Sockets for recving from remote proc nameserver ports: */
    pthread_t           listener;
    /* Listener thread for NameServer replies and requests. */
    int                 unblock_fd;
    /* Event to post to exit listener. */
    int                 wait_fd;
    /* Event to post to NameServer_get. */
    NameServerMsg       nsMsg;
    /* NameServer Message cache. */
    NameServer_Params   defInstParams;
    /* Default instance paramters */
    NameServer_Config   defCfg;
    /* Default module configuration */
    NameServer_Config   cfg;
    /* Module configuration */
    NameServer_Handle   handle;  // ONLY HAVE ONE FOR NOW.
} NameServer_ModuleObject;

/* =============================================================================
 *  Globals
 * =============================================================================
 */
static NameServer_ModuleObject NameServer_state =
{
    .defInstParams.maxRuntimeEntries = 0u,
    .defInstParams.tableHeap         = NULL,
    .defInstParams.checkExisting     = TRUE,
    .defInstParams.maxValueLen       = 0u,
    .defInstParams.maxNameLen        = 16u
};

static NameServer_ModuleObject * NameServer_module = &NameServer_state;

static void NameServerRemote_processMessage(NameServerMsg * msg, UInt16 procId)
{
    NameServer_Handle handle;
    Int               status = NameServer_E_FAIL;
    int               err;
    uint64_t          buf = 1;
    int               num_bytes;
    int               wait_fd = NameServer_module->wait_fd;

    if (msg->request == NAMESERVER_REQUEST) {
        printf("NameServer Request: instanceName: %s, name: %s\n",
               (String)msg->instanceName, (String)msg->name);

        /*
         *  Message is a request. Lookup name in NameServer table.
         *  Send a response message back to source processor.
         */
        handle = NameServer_getHandle((String)msg->instanceName);

        if (handle != NULL) {
            /* Search for the NameServer entry */
            printf("Calling NameServer_getLocalUInt32...\n");
            status = NameServer_getLocalUInt32(handle,
                     (String)msg->name, &msg->value);
        }

        printf("NameServer Response: instanceName: %s, name: %s,", 
               (String)msg->instanceName, (String)msg->name);
        /* set the request status */
        if (status < 0) {
            printf(" Found status: %d\n");
            msg->requestStatus = 0;
        }
        else {
            msg->requestStatus = 1;
            printf(" Value: 0x%x\n", msg->value);
        }

        /* specify message as a response */
        msg->request = NAMESERVER_RESPONSE;
        msg->reserved = NAMESERVER_MSG_TOKEN;

        /* send response message to remote processor */
        err = send(NameServer_module->send_sock[procId], msg, 
                   sizeof(NameServerMsg), 0);
        if (err < 0) {
            printf ("NameServer: send failed: %d, %s\n",
                  errno, strerror(errno));
        }
    }
    else {
        printf("NameServer Reply: instanceName: %s, name: %s, value: 0x%x\n",
               (String)msg->instanceName, (String)msg->name, msg->value);
        /* Save the response message.  */
        memcpy(&NameServer_module->nsMsg, msg,
                sizeof (NameServerMsg));
        /* Post the eventfd upon which NameServer_get() is waiting */
        num_bytes = write(wait_fd, &buf, sizeof(uint64_t));
    }
}


static void *listener_cb(void *arg)
{
    fd_set rfds;
    int ret = 0, maxfd;
    UInt16 procId;
    struct  sockaddr_rpmsg  from_addr;
    unsigned int len;
    NameServerMsg msg;
    int     byte_count;
    UInt16  numProcs = MultiProc_getNumProcessors();
    int     sock;
        
    printf("listener_cb: Entered Listener thread.\n");
    do {
        /* Wait for NameServer messages or unblock_fd notification */
        FD_ZERO(&rfds);
        FD_SET(NameServer_module->unblock_fd, &rfds);
        maxfd = NameServer_module->unblock_fd;
        for (procId = 0; procId < numProcs; procId++) {
            if (procId == MultiProc_self()) {
                continue;
            }
            sock = NameServer_module->recv_sock[procId];
            FD_SET(sock, &rfds);
            maxfd = MAX(sock, maxfd);
        }

        maxfd = maxfd + 1;
	printf("NameServer: waiting for unblock_fd: %d, and socks: maxfd: %d\n",
               NameServer_module->unblock_fd, maxfd);
        ret = select(maxfd, &rfds, NULL, NULL, NULL);
        if (ret == -1) {
            printf("listener_cb: select failed.");
            break;
        }

        for (procId = 0; procId < numProcs; procId++) {
            if (procId == MultiProc_self()) {
                continue;
            }
            if (FD_ISSET(NameServer_module->recv_sock[procId], &rfds)) {
	        printf("NameServer: Listener got NameServer message!\n");
                /* Get NameServer message and process: */
	        memset(&from_addr, 0, sizeof(from_addr));
	        len = sizeof(from_addr);

	        byte_count = recvfrom(sock, &msg, sizeof(NameServerMsg), 0,
				(struct sockaddr *)&from_addr, &len);
    	        if (len != sizeof(from_addr)) {
		    printf("recvfrom: got bad addr len (%d)\n", len);
                    break;
	        }
	        if (byte_count < 0) {
		    printf("recvfrom failed: %s (%d)\n", 
                            strerror(errno), errno);
                    break;
	        }
	        else {
		    printf ("listener_cb: recvfrom socket: fd: %d\n", sock);
		    printf("\tReceived ns msg: byte_count: %d, from addr: %d, "
                           "from vproc: %d\n", byte_count, from_addr.addr, 
                           from_addr.vproc_id);
                    NameServerRemote_processMessage(&msg, procId);
	        }
	    }
        }
        if (FD_ISSET(NameServer_module->unblock_fd, &rfds)) {
            /* We are told to unblock and exit: */
	    printf("NameServer: Listener thread exiting\n");
	    break;
        }
    } while (1);

    return (void *)ret;
}

/* =============================================================================
 * APIS
 * =============================================================================
 */

/* Function to setup the nameserver module. */
Int
NameServer_setup (Void)
{
    Int status    = NameServer_S_SUCCESS;
    UInt16 procId;
    int    err;
    int	   sock;
    int    ret;
    UInt16 numProcs = MultiProc_getNumProcessors();

    NameServer_module->unblock_fd = eventfd(0, 0);
    if (NameServer_module->unblock_fd < 0) {
        status = NameServer_E_FAIL;
        printf("NameServer_setup: failed to create unblock_fd.\n");
        goto exit;
    }

    NameServer_module->wait_fd = eventfd(0, 0);
    if (NameServer_module->wait_fd < 0) {
        status = NameServer_E_FAIL;
        printf("NameServer_setup: failed to create wait_fd.\n");
        goto exit;
    }

    for (procId = 0; procId < numProcs; procId++) {
        /* Only support NameServer to remote procs: */
        if (procId == MultiProc_self()) {
            NameServer_module->send_sock[procId] = INVALIDSOCKET;
            NameServer_module->recv_sock[procId] = INVALIDSOCKET;
            continue;
        }

        /* Create the socket for sending messages to each remote proc: */
        sock = socket(AF_RPMSG, SOCK_SEQPACKET, 0);
        if (sock < 0) {
            status = NameServer_E_FAIL;
            printf ("NameServer_setup: socket failed: %d, %s\n",
                   errno, strerror(errno));
        }
        else  {
	    printf ("NameServer_setup: created send socket: %d\n", sock);
            err = connect_socket(sock, procId, MESSAGEQ_RPMSG_PORT);
	    if (err < 0) {
	       status = NameServer_E_FAIL;
	       printf ("NameServer_setup: connect failed: %d, %s\n",
			  errno, strerror(errno));
	    }
            else {
               NameServer_module->send_sock[procId] = sock;
            }
        }

        /* Create the socket for recving messages from each remote proc: */
        sock = socket(AF_RPMSG, SOCK_SEQPACKET, 0);
        if (sock < 0) {
            status = NameServer_E_FAIL;
            printf ("NameServer_setup: socket failed: %d, %s\n",
                   errno, strerror(errno));
        }
        else  {
	    printf ("NameServer_setup: created recv socket: %d\n", sock);

            err = socket_bind_addr(sock, procId, NAME_SERVER_RPMSG_ADDR);
	    if (err < 0) {
	       status = NameServer_E_FAIL;
	       printf ("NameServer_setup: bind failed: %d, %s\n",
			  errno, strerror(errno));
	    }
            else {
               NameServer_module->recv_sock[procId] = sock;
            }
        }
    }

    /* Create the listener thread: */
    printf ("NameServer_setup: creating listener thread\n");
    ret = pthread_create(&NameServer_module->listener, NULL, listener_cb, NULL);
    if (ret) {
        printf("NameServer_setup: can't spawn thread: %s\n", strerror(ret));
        printf("NameServer_setup: eventfd failed");
    }

exit:
    return status;
}

/*! Function to destroy the nameserver module. */
Int NameServer_destroy (void)
{
    Int status    = NameServer_S_SUCCESS;
    UInt16 procId;
    int	   sock;
    uint64_t     buf = 1;
    int          num_bytes;
    int ret;
    UInt16 numProcs = MultiProc_getNumProcessors();

    for (procId = 0; procId < numProcs; procId++) {
        /* Only support NameServer to remote procs: */
        if (procId == MultiProc_self()) {
            continue;
        }

        /* Close the socket: */
        sock = NameServer_module->send_sock[procId];
        if (sock != INVALIDSOCKET) {
            printf ("NameServer_destroy: closing socket: %d\n", sock);
            close(sock);
            NameServer_module->send_sock[procId] = INVALIDSOCKET;
        }
        /* Close the socket: */
        sock = NameServer_module->recv_sock[procId];
        if (sock != INVALIDSOCKET) {
            printf ("NameServer_destroy: closing socket: %d\n", sock);
            close(sock);
            NameServer_module->recv_sock[procId] = INVALIDSOCKET;
        }
    }

    /* Unblock the NameServer listener thread: */
    printf("NameServer_destroy: unblocking listener...\n");
    num_bytes = write(NameServer_module->unblock_fd, &buf, sizeof(uint64_t));

    /* Join: */
    printf("NameServer_destroy: joining listener thread...\n");
    ret = pthread_join(NameServer_module->listener, NULL);

    close(NameServer_module->unblock_fd);
    close(NameServer_module->wait_fd);
    return status;
}

/* Function to retrieve a NameServer handle from name. */
NameServer_Handle
NameServer_getHandle (String name)
{
    NameServer_Handle handle = NULL;
    Bool              found = FALSE;

    /* Lookup handle from name: */
    handle = NameServer_module->handle;
    found = TRUE;

    if (found == FALSE) {
        handle = NULL;
    }

    return handle;
}


/* Function to create a name server. */
NameServer_Handle
NameServer_create (String name, const NameServer_Params * params)

{
    NameServer_Handle handle = NULL;
    UInt              name_len;
    int	              rval;
    String	      temp_name = NULL;
    struct hsearch_data *htab = NULL;

    /* See if a name server of this name already exists: */
    handle = NameServer_getHandle(name);
    if (handle) {
        goto leave;
    }

    if(params->maxValueLen > sizeof(UInt32)) {
       /* Can't handle more than UInt32 at this time: */
       goto leave;
    }

    /* Create a NameServer Object: */
    handle = calloc(1,sizeof(NameServer_Object));
    name_len = strlen(name)+1u;
    temp_name = (String)calloc(1, name_len);
    htab = calloc(1,sizeof(struct hsearch_data));
    if (handle && temp_name && htab) {
        handle->name = temp_name;
        strncpy(handle->name, name, name_len);
        memcpy((Ptr)&handle->params,(Ptr)params, sizeof(NameServer_Params));

	handle->params.maxValueLen = sizeof(UInt32);

        /* Create the hash table: */
        handle->htab = htab;
	rval = hcreate_r(NUM_HASH_ELEMENTS, handle->htab);
        if (!rval) {
            goto cleanup;
        }
        else {
            NameServer_module->handle = handle;
        }
        
        goto leave;
    }

cleanup:
    if (temp_name) {
        free(temp_name);
    }
    if (htab) {
        free(htab);
    }
    if (handle) {
        free(handle);
        handle = NULL;
    }

leave:
    return handle;
}


/* Function to delete a name server. */
Int
NameServer_delete (NameServer_Handle * handle)
{
    Int                   status = NameServer_S_SUCCESS;
    struct NameServer_Object *obj = (struct NameServer_Object *)(*handle);

    hdestroy_r(obj->htab);
    free(obj->htab);
    obj->htab = NULL;
    free(obj->name);

    // TBD: we only have one instance for now!: 
    NameServer_module->handle = NULL;

    /* Overwrite the user's handle to NULL! */
    *handle = NULL;
    return status;
}


/* Function to add a UInt32 value into a name server. */
Ptr
NameServer_addUInt32 (NameServer_Handle handle, String name, UInt32 value)
{
    ENTRY *entry = NULL, item;
    int   len;
    struct NameServer_Object *obj = (struct NameServer_Object *)(handle);

    /* Add to hash table: */
    len = strlen(name)+1;
    item.key = calloc(1, len);
    if (item.key) {
        memcpy(item.key, name, len);
        item.data = (void *)value;
        if (!hsearch_r(item, ENTER, &entry, obj->htab)) {
            printf("NameServer_addUInt32: hsearch_r failed to add: %s\n", name);
        }
        else {
            printf("NameServer_addUInt32: Entered key: %s, data: 0x%x\n",
               entry->key, (UInt32)entry->data);
        }
    }
    return (Ptr)entry;
}


/* Function to remove a name/value pair from a name server. */
Int
NameServer_removeEntry (NameServer_Handle handle, Ptr entry)
{
    Int   status = NameServer_S_SUCCESS;

    /* Can't really remove a data item from this hash table, but we can at
     * least deallocate the memory previously allocated for it
     */
    free(((ENTRY *)entry)->key);

    return (status);
}


/* Initialize this config-params structure with supplier-specified
 * defaults before instance creation.
 */
Void
NameServer_Params_init (NameServer_Params * params)
{
    memcpy(params, &(NameServer_module->defInstParams),
                     sizeof (NameServer_Params));
}



/* Gets a 32-bit value by name */
Int
NameServer_getUInt32 (NameServer_Handle handle,
                      String            name,
                      Ptr               value,
                      UInt16            procIds[])
{
    Int    status = NameServer_S_SUCCESS;
    UInt16 procId;
    NameServerMsg    nsMsg;
    NameServerMsg    *reply_msg;
    fd_set rfds;
    int ret = 0, sock, maxfd, wait_fd;
    int len;
    struct timeval tv;
    struct NameServer_Object *obj = (struct NameServer_Object *)(handle);
    uint64_t buf = 1;
    int    num_bytes;
    int    err;
    UInt16 numProcs = MultiProc_getNumProcessors();

    /* BIOS side uses a gate (mutex) to protect NameServer_module->nsMsg, but
     * since this goes in a daemon, it will not be necessary.
     */

    /* Set Timeout to wait: */
    tv.tv_sec = NAMESERVER_GET_TIMEOUT;
    tv.tv_usec = 0;

    /* For now, just check remote directly: */

    for (procId = 0; procId < numProcs; procId++) {
        /* Only support NameServer to remote procs: */
        if (procId == MultiProc_self()){
            continue;
        }

        /* Create request message and send to remote: */
        sock = NameServer_module->send_sock[procId];
        printf("NameServer_get: Sending request via sock: %d\n", sock);

        /* Create request message and send to remote processor: */
        nsMsg.reserved = NAMESERVER_MSG_TOKEN;
        nsMsg.request = NAMESERVER_REQUEST;
        nsMsg.requestStatus = 0;

        len = strlen(obj->name) + 1;
        strncpy((char *)nsMsg.instanceName, obj->name, len);

        len = strlen(name) + 1;
        strncpy((char *)nsMsg.name, name, len);

        printf("NameServer_getUInt32: Requesting from procId %d, %s:%s...\n",
               procId, (String)nsMsg.instanceName, (String)nsMsg.name);
        err = send(sock, &nsMsg,sizeof(NameServerMsg), 0);
        if (err < 0) {
            printf ("NameServer_getUInt32: send failed: %d, %s\n",
                  errno, strerror(errno));
            status = NameServer_E_FAIL;
            break;
        }

        /* Block on wait_fd for signal from listener thread: */
        wait_fd = NameServer_module->wait_fd;
        FD_ZERO(&rfds);
        FD_SET(wait_fd, &rfds);
        maxfd = wait_fd + 1;
        printf("NameServer_get: pending on wait_fd: %d\n", wait_fd);
        ret = select(maxfd, &rfds, NULL, NULL, &tv);
        if (ret == -1) {
            printf("listener_cb: select failed.");
            status = NameServer_E_FAIL;
            break;
        }
        else if (!ret) {
            printf("listener_cb: select timed out.\n");
            status = NameServer_E_TIMEOUT;
            break;
        }

        if (FD_ISSET(wait_fd, &rfds)) {
            /* Read, just to balance the write: */
            num_bytes = read(wait_fd, &buf, sizeof(uint64_t));

	    /* Process response: */
            reply_msg = &NameServer_module->nsMsg;

            if (reply_msg->requestStatus) {
                /* name is found */
                /* set the contents of value */
                *(UInt32 *)value = (UInt32)reply_msg->value;

                printf("NameServer_getUInt32: Reply from: %d, %s:%s,"
                 " value: 0x%x...\n", procId, (String)reply_msg->instanceName, 
                 (String)reply_msg->name, *(UInt32 *)value);
                break;
            }
            else {
                /* name is not found */
                printf("NameServer_getUInt32: value for %s:%s not found.\n",
                   (String)reply_msg->instanceName, (String)reply_msg->name);

                /* set status to not found */
                status = NameServer_E_NOTFOUND;
            }
        }
    }

    return status;
}

/* Gets a 32-bit value by name from the local table */
Int
NameServer_getLocalUInt32 (NameServer_Handle handle,
                           String            name,
                           Ptr               value)
{
    ENTRY  *entry = NULL, item;
    struct NameServer_Object *obj = (struct NameServer_Object *)(handle);
    Int    status = NameServer_S_SUCCESS;

    /* Search hash table for key (name): */
    item.key = name;
    if (hsearch_r(item, FIND, &entry, obj->htab)) {
        /* Assumed that data is only of size UInt32 */
        printf("NameServer_getLocalUInt32: Found entry key: %s, data: 0x%x\n",
               entry->key, (UInt32)entry->data);
        *(UInt32 *)value = (UInt32)entry->data;
    }
    else {
        printf("NameServer_getLocalUInt32: entry key: %s not found! %d, %s\n",
                  item.key, errno, strerror(errno));
        status = NameServer_E_NOTFOUND;
    }

    return status;
}


#if defined (__cplusplus)
}
#endif /* defined (__cplusplus) */
