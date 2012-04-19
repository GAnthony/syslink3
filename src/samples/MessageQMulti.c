/*
 * Copyright (c) 2011, Texas Instruments Incorporated
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
 *  @file   MessageQApp.c
 *
 *  @brief  Sample application for MessageQ module between MPU and Remote Proc
 *
 *  ============================================================================
 */

/* Standard headers */
#include <pthread.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

/* SysLink Standard Header: */
#include <Std.h>

/* Module level headers */
#include <ti/ipc/NameServer.h>
#include <ti/ipc/MessageQ.h>
#include <ti/ipc/MultiProc.h>

/* For Backplane IPC startup/shutdown stuff: */
#include <_MultiProc.h>
#include <_MessageQ.h>
#include <_NameServer.h>

#ifdef USE_LAD
#include <ladclient.h>
#endif

/* App defines */
#define MSGSIZE                     64u
/* Must match on remote proc side: */
#define HEAPID                      0u

#define SLAVE_MESSAGEQNAME          "SLAVE"
#define HOST_MESSAGEQNAME           "HOST"

/** ============================================================================
 *  Macros and types
 *  ============================================================================
 */

#define  NUM_LOOPS_DFLT   100
#define  NUM_THREADS_DFLT 10
#define  MAX_NUM_THREADS  50

/** ============================================================================
 *  Globals
 *  ============================================================================
 */
UInt16  MessageQApp_procId;
int     num_loops, num_threads;

struct thread_info {    /* Used as argument to thread_start() */
    pthread_t thread_id;        /* ID returned by pthread_create() */
    int       thread_num;       /* Application-defined thread # */
};

#ifdef USE_LAD
LAD_ClientHandle ladHandle;
LAD_Status ladStatus;
#endif

/** ============================================================================
 *  Functions
 *  ============================================================================
 */
Int
MessageQApp_startup ()
{
    /* This must be setup to match BIOS side MultiProc configuration for the
     * given platform!
     */
    MultiProc_Config MultiProc_cfg =  {
       .numProcessors = 2,
       .nameList[0] = "HOST",
       .nameList[1] = "SysM3",
       .id = 0,                 /* The host is always zero */
    };
    Int32             status = 0;
    MessageQ_Config   cfg;

    printf ("Entered MessageQApp_startup\n");

#ifdef USE_LAD
    ladStatus = LAD_connect(&ladHandle);
    if (ladStatus != LAD_SUCCESS) {
        printf("LAD_connect() failed: %d\n", ladStatus);
        return -1;
    }
    else {
        printf("LAD_connect() succeeded: ladHandle=%d\n", ladHandle);
    }
#endif

    /* SysLink Backplane stuff:  */
    MultiProc_setup(&MultiProc_cfg);
    status = NameServer_setup();
    if (status == NameServer_S_SUCCESS) {
        MessageQ_getConfig(&cfg);
        MessageQ_setup(&cfg);
        MessageQApp_procId = MultiProc_getId("SysM3");
        status = MessageQ_attach (MessageQApp_procId, NULL);
    }

    printf ("Leaving MessageQApp_startup: status = 0x%x\n", status);

    return (status);
}


static void * ping_thread(void *arg)
{
    int                      thread_num = *(int *)arg;
    Int32                    status     = 0;
    MessageQ_Msg             msg        = NULL;
    MessageQ_Params          msgParams;
    UInt16                   i;
    MessageQ_Handle          handle;
    MessageQ_QueueId         queueId = MessageQ_INVALIDMESSAGEQ;

    char             remoteQueueName[64];
    char             hostQueueName[64];

    sprintf(remoteQueueName, "%s_%d", SLAVE_MESSAGEQNAME, thread_num );
    sprintf(hostQueueName,   "%s_%d", HOST_MESSAGEQNAME,  thread_num );

    printf ("thread: %d\n", thread_num);

    /* Create the local Message Queue for receiving. */
    MessageQ_Params_init (&msgParams);
    handle = MessageQ_create (hostQueueName, &msgParams);
    if (handle == NULL) {
        printf ("Error in MessageQ_create\n");
        goto exit;
    }
    else {
        printf ("thread: %d, Local Message: %s, QId: 0x%x\n",
            thread_num, hostQueueName,
             MessageQ_getQueueId(handle));
    }

    /* Poll until remote side has it's messageQ created before we send: */
    do {
        status = MessageQ_open (remoteQueueName, &queueId);
	sleep (1);
    } while (status == MessageQ_E_NOTFOUND);
    if (status < 0) {
        printf ("Error in MessageQ_open [0x%x]\n", status);
        goto cleanup;
    }
    else {
        printf ("thread: %d, Remote queue: %s, QId: 0x%x\n", 
                 thread_num, remoteQueueName, queueId);
    }

    printf ("\nthread: %d: Exchanging messages with remote processor...\n", 
            thread_num);
    for (i = 0 ; i < num_loops ; i++) {
          /* Allocate message. */
          msg = MessageQ_alloc (HEAPID, MSGSIZE);
          if (msg == NULL) {
              printf ("Error in MessageQ_alloc\n");
              break;
          }

          MessageQ_setMsgId (msg, i);

          /* Have the remote proc reply to this message queue */
          MessageQ_setReplyQueue (handle, msg);

          status = MessageQ_put (queueId, msg);
          if (status < 0) {
              printf ("Error in MessageQ_put [0x%x]\n", status);
              break;
          }

          status = MessageQ_get(handle, &msg, MessageQ_FOREVER);
          if (status < 0) {
              printf ("Error in MessageQ_get [0x%x]\n", status);
              break;
          }
          else {
              printf ("MessageQ_get #%d Msg = 0x%x\n", i, (UInt)msg);

              /* Validate the returned message. */
              if ((msg != NULL) && (MessageQ_getMsgId (msg) != i)) {
                  printf ("Data integrity failure!\n"
                          "    Expected %d\n"
                          "    Received %d\n",
                          i, MessageQ_getMsgId (msg));
                  break;
              }

              status = MessageQ_free (msg);
              printf ("MessageQ_free status [0x%x]\n", status);
          }

          printf ("thread: %d: Exchanged %d messages with remote processor\n", 
                   thread_num, (i+1));
    }

    printf ("thread: %d: ping_thread successfully completed!\n", thread_num);

    MessageQ_close (&queueId);

cleanup:
    /* Clean-up */
    status = MessageQ_delete (&handle);
    if (status < 0) {
        printf ("Error in MessageQ_delete [0x%x]\n", status);
    }

exit:

    return (void *)status;
}

Int
MessageQApp_shutdown ()
{
    Int32               status = 0;

    printf ("Entered MessageQApp_shutdown()\n");

    /* SysLink Backplane stuff: */
    status = MessageQ_detach (MessageQApp_procId);
    MessageQ_destroy ();
    NameServer_destroy();
    MultiProc_destroy();

#ifdef USE_LAD
    ladStatus = LAD_disconnect(ladHandle);
    if (ladStatus != LAD_SUCCESS) {
        printf("LAD_disconnect() failed: %d\n", ladStatus);
        return -1;
    }
    else {
        printf("LAD_disconnect() succeeded\n");
    }
#endif

    printf ("Leave MessageQApp_shutdown()\n");

    return (status);
}

int
main (int argc, char ** argv)
{
    struct thread_info threads[MAX_NUM_THREADS];
    int ret,i;
    void *res;

    /* Parse Args: */
    num_loops = NUM_LOOPS_DFLT;
    num_threads = NUM_THREADS_DFLT;
    switch (argc) {
        case 1: 
           /* use defaults */
           break;
        case 2: 
           num_threads = atoi(argv[1]);
           break;
        case 3: 
           num_threads = atoi(argv[1]);
           num_loops   = atoi(argv[2]);
           break;
        default:
           printf("Usage: %s [<num_threads>] [<num_loops>]\n", argv[0]);
           printf("\tDefaults: num_threads: 10, num_loops: 100\n");
           printf("\tMax Threads: 100\n");
           exit(0);
    }
    printf("Using num_threads: %d, num_loops: %d\n", num_threads, num_loops);

    MessageQApp_startup ();

    /* Launch multiple threads: */
    for (i = 0; i < num_threads; i++) {
        /* Create the test thread: */
        printf ("creating ping_thread: %d\n", i);
        threads[i].thread_num = i;
        ret = pthread_create(&threads[i].thread_id, NULL, &ping_thread, 
                           &(threads[i].thread_num));
        if (ret) {
            printf("MessageQMulti: can't spawn thread: %d, %s\n", 
                    i, strerror(ret));
        }
    }

    /* Join all threads: */
    for (i = 0; i < num_threads; i++) {
        ret = pthread_join(threads[i].thread_id, &res);
        if (ret != 0) {
            printf("MessageQMulti: failed to join thread: %d, %s\n", 
                    i, strerror(ret)); 
        }
        printf("MessageQMulti: Joined with thread %d; returned value was %s\n",
                threads[i].thread_num, (char *) res);
        free(res);      /* Free memory allocated by thread */
    }

    MessageQApp_shutdown ();

    return(0);
}

