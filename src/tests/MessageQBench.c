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
 *  @file   MessageQBench.c
 *
 *  @brief  Benchmark application for MessageQ module between MPU/Remote Proc
 *
 *  ============================================================================
 */

/* Standard headers */
#include <stdio.h>
#include <time.h>

/* SysLink/IPC Headers: */
#include <Std.h>
#include <SysLink.h>
#include <ti/ipc/MessageQ.h>

/* App defines: Must match on remote proc side: */
#define NUM_LOOPS           100     /* Number of transfers to be tested. */
#define MSGSIZE             64u
#define HEAPID              0u
#define CORE0_MESSAGEQNAME  "SLAVE"
#define MPU_MESSAGEQNAME    "HOST"


long diff(struct timespec start, struct timespec end)
{
    long    usecs;

    struct timespec temp;
    if ((end.tv_nsec-start.tv_nsec)<0) {
        temp.tv_sec = end.tv_sec-start.tv_sec-1;
        temp.tv_nsec = 1000000000+end.tv_nsec-start.tv_nsec;
    } else {
        temp.tv_sec = end.tv_sec-start.tv_sec;
        temp.tv_nsec = end.tv_nsec-start.tv_nsec;
    }
    usecs = temp.tv_sec * 1000 + temp.tv_nsec / 1000;
    return usecs;
}

Int
MessageQApp_execute ()
{
    Int32                    status     = 0;
    MessageQ_Msg             msg        = NULL;
    MessageQ_Params          msgParams;
    int                      i;
    MessageQ_QueueId         queueId = MessageQ_INVALIDMESSAGEQ;
    MessageQ_Handle          msgqHandle;
    struct timespec          start,end;
    long                     elapsed=0,delta;

    printf ("Entered MessageQApp_execute\n");

    /* Create the local Message Queue for receiving. */
    MessageQ_Params_init (&msgParams);
    msgqHandle = MessageQ_create (MPU_MESSAGEQNAME, &msgParams);
    if (msgqHandle == NULL) {
        printf ("Error in MessageQ_create\n");
        goto exit;
    }
    else {
        printf ("Local MessageQId: 0x%x\n",
            MessageQ_getQueueId(msgqHandle));
    }

    /* Poll until remote side has it's messageQ created before we send: */
    do {
        status = MessageQ_open (CORE0_MESSAGEQNAME, &queueId);
        sleep (1);
    } while (status == MessageQ_E_NOTFOUND);
    if (status < 0) {
        printf ("Error in MessageQ_open [0x%x]\n", status);
        goto cleanup;
    }
    else {
        printf ("Remote queueId  [0x%x]\n", queueId);
    }

    printf ("\nExchanging messages with remote processor...\n");
    for (i = 0 ; i < NUM_LOOPS ; i++) {

        /* Allocate message. */
        msg = MessageQ_alloc (HEAPID, MSGSIZE);
        if (msg == NULL) {
            printf ("Error in MessageQ_alloc\n");
            break;
        }


        MessageQ_setMsgId (msg, i);

        /* Have the remote proc reply to this message queue */
        MessageQ_setReplyQueue (msgqHandle, msg);

        //if (i) clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &start);
        if (i) clock_gettime(CLOCK_REALTIME, &start);

        status = MessageQ_put (queueId, msg);
        if (status < 0) {
            printf ("Error in MessageQ_put [0x%x]\n", status);
            break;
        }

        status = MessageQ_get(msgqHandle, &msg, MessageQ_FOREVER);
        if (status < 0) {
            printf ("Error in MessageQ_get [0x%x]\n", status);
            break;
        }
        else {
            /* Validate the returned message. */
            if ((msg != NULL) && (MessageQ_getMsgId (msg) != i)) {
                printf ("Data integrity failure!\n"
                        "    Expected %d\n"
                        "    Received %d\n",
                        i, MessageQ_getMsgId (msg));
                break;
            }

            if (i) {
           /* we avoid the first datum, 'cause it's an outlier: */
           //clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &end);
           clock_gettime(CLOCK_REALTIME, &end);
               delta = diff(start,end);
               elapsed += delta;
           }

            status = MessageQ_free (msg);

        }
        printf ("Message (%d) time: %ld usecs\n", i, delta);
    }

    printf ("Sample application successfully completed!\n");

    MessageQ_close (&queueId);
    printf("Avg round trip time: %ld usecs\n", elapsed/(i-1));


cleanup:
    /* Clean-up */
    status = MessageQ_delete (&msgqHandle);
    if (status < 0) {
        printf ("Error in MessageQ_delete [0x%x]\n", status);
    }

exit:
    printf ("Leaving MessageQApp_execute\n\n");

    return (status);
}

int
main (int argc, char ** argv)
{
    Int32   status = 0;

    status = SysLink_setup();

    if (status >= 0) {
        MessageQApp_execute();
        SysLink_destroy();
    }
    else {
        printf ("SysLink_setup failed: status = 0x%x\n", status);
    }

    return(0);
}
