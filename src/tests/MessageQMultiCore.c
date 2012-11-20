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
/* =============================================================================
 *  @file   MessageQMultiCore.c
 *
 *  @brief  Round Trip test between a shared HOST MessageQ and all remote COREs.
 *
 *  ============================================================================
 */

/* Standard headers */
#include <pthread.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

/* SysLink/IPC Headers: */
#include <Std.h>
#include <SysLink.h>
#include <ti/ipc/MessageQ.h>

/* App defines: Must match on remote proc side: */
#define MSGSIZE                     64u
#define HEAPID                      0u
#define SLAVE_MESSAGEQNAME          "SLAVE"
#define HOST_MESSAGEQNAME           "HOST"

/** ============================================================================
 *  Macros and types
 *  ============================================================================
 */

#define  NUM_LOOPS_DFLT   1000
#define  NUM_THREADS_DFLT 1
#define  MAX_NUM_THREADS  50
#define  ONE_PROCESS_ONLY (-1)

/** ============================================================================
 *  Globals
 *  ============================================================================
 */

static UInt32  numLoops = NUM_LOOPS_DFLT;
static MessageQ_Handle  hHost;

struct thread_info {    /* Used as argument to thread_start() */
    pthread_t thread_id;        /* ID returned by pthread_create() */
    int       procId;       /* Application-defined thread # */
};

static void * pingThreadFxn(void *arg);

/** ============================================================================
 *  Functions
 *  ============================================================================
 */

static Void * pingThreadFxn(void *arg)
{
    Int                      procId = *(int *)arg;
    Int32                    status     = 0;
    MessageQ_Msg             msg        = NULL;
    UInt16                   i;
    MessageQ_QueueId         queueId = MessageQ_INVALIDMESSAGEQ;
    char                     remoteQueueName[64];

    printf ("Entered pingThreadFxn for %s\n", MultiProc_getName(procId));

    sprintf(remoteQueueName, "%s_%s", SLAVE_MESSAGEQNAME,
            MultiProc_getName(procId));

    /* Poll until remote side has it's messageQ created before we send: */
    do {
        status = MessageQ_open (remoteQueueName, &queueId);
        sleep (1);
    } while (status == MessageQ_E_NOTFOUND);
    if (status < 0) {
        printf ("Error in MessageQ_open [0x%x]\n", status);
        goto exit;
    }
    else {
        printf ("Remote queue: %s, QId: 0x%x\n",
                 MultiProc_getName(procId), queueId);
    }

    printf("Exchanging %d messages with remote processor %s...\n",
           numLoops, MultiProc_getName(procId));

    for (i = 0 ; i < numLoops ; i++) {
        /* Allocate message. */
        msg = MessageQ_alloc (HEAPID, MSGSIZE);
        if (msg == NULL) {
            printf ("Error in MessageQ_alloc\n");
            break;
        }

        MessageQ_setMsgId (msg, i);

        /* Have the remote proc reply to this message queue */
        MessageQ_setReplyQueue (hHost, msg);

        status = MessageQ_put (queueId, msg);
        if (status < 0) {
            printf ("Error in MessageQ_put [0x%x]\n", status);
            break;
        }

        status = MessageQ_get(hHost, &msg, MessageQ_FOREVER);
        if (status < 0) {
            printf ("Error in MessageQ_get [0x%x]\n", status);
            break;
        }
        else {
#if 0
            /* Validate the returned message. */
            if ((msg != NULL) && (MessageQ_getMsgId (msg) != i)) {
                printf ("Data integrity failure!\n"
                        "    Expected %d\n"
                        "    Received %d\n",
                        i, MessageQ_getMsgId (msg));
                break;
            }
#endif

            status = MessageQ_free (msg);
        }

        printf ("Core %s: Exchanged %d msgs\n",
                MultiProc_getName(procId), (i+1));
    }

    printf ("pingThreadFxn for core %s successfully completed!\n",
             MultiProc_getName(procId));
    MessageQ_close (&queueId);

exit:
    return ((void *)status);
}

int main (int argc, char ** argv)
{
    struct thread_info threads[MAX_NUM_THREADS];
    int ret,i;
    void *res;
    Int32   status = 0;
    MessageQ_Params msgParams;

    /* Parse args: */
    if (argc > 1) {
        numLoops = strtoul(argv[1], NULL, 0);
    }

    if (argc > 2) {
        printf("Usage: %s [<numLoops>]\n", argv[0]);
        printf("\tDefaults: numLoops: %d\n", NUM_LOOPS_DFLT);
        exit(0);
    }

    printf("Using numLoops: %d\n", numLoops);

    status = SysLink_setup();
    if (status < 0) {
        printf ("SysLink_setup failed: status = 0x%x\n", status);
        goto exit;
    }

    /* Create the local Message Queue for receiving. */
    MessageQ_Params_init (&msgParams);
    hHost = MessageQ_create (HOST_MESSAGEQNAME, &msgParams);
    if (hHost == NULL) {
        printf ("Error in MessageQ_create\n");
        goto exit;
    }
    else {
        printf ("Host MessageQ: %s, QId: 0x%x\n",
            HOST_MESSAGEQNAME, MessageQ_getQueueId(hHost));
    }

    /* Launch a single thread for each processor: */
    for (i = 1; i < MultiProc_getNumProcessors(); i++) {
        /* Create the test thread: */
        printf ("creating pingThreadFxn for CORE %s\n", MultiProc_getName(i));
	threads[i].procId = i;
        ret = pthread_create(&threads[i].thread_id, NULL, &pingThreadFxn,
                           &(threads[i].procId));
        if (ret) {
            printf("MessageQMulti: can't spawn thread: %d, %s\n",
                    i, strerror(ret));
        }
    }

    /* Join all threads: */
    for (i = 1; i < MultiProc_getNumProcessors(); i++) {
        ret = pthread_join(threads[i].thread_id, &res);
        if (ret != 0) {
            printf("MessageQMulti: failed to join thread: %d, %s\n",
                    i, strerror(ret));
        }
        printf("MessageQMulti: Joined with thread %d; returned value was %s\n",
                threads[i].procId, (char *) res);
        free(res);      /* Free memory allocated by thread */
    }

    /* Clean-up */
    status = MessageQ_delete (&hHost);
    if (status < 0) {
        printf ("Error in MessageQ_delete [0x%x]\n", status);
    }


    SysLink_destroy();

exit:

    return (0);
}
