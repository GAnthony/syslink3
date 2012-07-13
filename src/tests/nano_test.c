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
 *  @file   nano_test.c
 *
 *  @brief  Customer use case test application. 
 *
 *  Usage:  See etc/omapl138/nano_test.sh
 *
 *  Notes:  There is an overrun reported by arecord in the very begining of
 *          the test.  This does not appear to be due to DSP communication,
 *          as there are no overruns once the test gets past initialization.
 *          One possible cause is a startup effect of using the shell
 *          pipe command.
 *
 *  ============================================================================
 */

/* Standard headers */
#include <stdio.h>

/* SysLink/IPC Headers: */
#include <Std.h>
#include <SysLink.h>
#include <ti/ipc/MessageQ.h>

#include <ti/sdo/linuxutils/cmem/include/cmem.h>

struct MyMsg {
    MessageQ_MsgHeader header;
    unsigned long bufPhys;
};
typedef struct MyMsg MyMsg;

/* App defines:  Must match on remote proc side: */
#define NUM_SLAVE_MSGS_PER_HOST_MSG   4
#define PAYLOADSIZE         (8192)

#define MSGSIZE             (sizeof(MyMsg))

#define HEAPID              0u
#define CORE0_MESSAGEQNAME  "SLAVE"
#define MPU_MESSAGEQNAME    "HOST"

Int
Nanotest_execute ()
{
    Int32                    status     = 0;
    MessageQ_Msg             msg        = NULL;
    MessageQ_Params          msgParams;
    MessageQ_QueueId         queueId = MessageQ_INVALIDMESSAGEQ;
    MessageQ_Handle          msgqHandle;
    int                      i, j;
    int                      ret;
    void                     *payload;
    unsigned long            payloadPhys;
    CMEM_AllocParams         cmemAttrs;
    MyMsg                    *myMsgPtr;

    printf ("Entered Nanotest_execute\n");

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
        printf ("Error in MessageQ_open [%d]\n", status);
        goto cleanup_create;
    }
    else {
        printf ("Remote queueId  [0x%x]\n", queueId);
    }

    cmemAttrs.type = CMEM_HEAP;
    cmemAttrs.flags = CMEM_NONCACHED;
    cmemAttrs.alignment = 0;
    payload = CMEM_alloc(PAYLOADSIZE, &cmemAttrs);
    if (payload == NULL) {
        printf("CMEM_alloc() failed (returned NULL)\n");
        goto cleanup_close;
    }
    payloadPhys = CMEM_getPhys(payload);

    printf ("\nExchanging messages with remote processor...\n");
    for (i = 0 ; ; i++) {

        /* read a block of data from stdin */
        ret = fread(payload, 1, PAYLOADSIZE, stdin);
        if (ret < PAYLOADSIZE) {
            printf ("EOS: Exiting without sending remaining %d bytes\n", ret);
            break;
        }

        /* Allocate message. */
        msg = MessageQ_alloc (HEAPID, MSGSIZE);
        if (msg == NULL) {
            printf ("Error in MessageQ_alloc\n");
            goto cleanup_cmem;
        }

        MessageQ_setMsgId (msg, i);

        /* Have the remote proc reply to this message queue */
        MessageQ_setReplyQueue (msgqHandle, msg);

        myMsgPtr = (MyMsg *)msg;
        myMsgPtr->bufPhys = payloadPhys;

        printf("Sending msgId: %d, size: %d, *msg: 0x%lx\n", i,
               MessageQ_getMsgSize(msg), myMsgPtr->bufPhys);

        status = MessageQ_put(queueId, msg);
        if (status < 0) {
            printf ("Error in MessageQ_put [%d]\n", status);
            break;
        }

        for (j = 0 ; j < NUM_SLAVE_MSGS_PER_HOST_MSG; j++) {
           status = MessageQ_get(msgqHandle, &msg, MessageQ_FOREVER);
           if (status < 0) {
               printf ("Error in MessageQ_get [%d]\n", status);
               status = MessageQ_free(msg);
               goto cleanup_close;
           }
           else {
               myMsgPtr = (MyMsg *)msg;
#ifdef VERBOSE
               printf ("Received msgId: %d, size: %d, *msg: 0x%lx\n",
                       MessageQ_getMsgId(msg), MessageQ_getMsgSize(msg),
                       myMsgPtr->bufPhys);
#endif

               /* Validate the returned message. */
               if ((msg != NULL) && (MessageQ_getMsgId(msg) != j)) {
                   printf ("Data integrity failure:\n"
                        "    Expected %d\n"
                        "    Received %d\n",
                        j, MessageQ_getMsgId (msg));
                   MessageQ_free(msg);
                   goto cleanup_close;
               }
               status = MessageQ_free(msg);
               if (status < 0) {
                   printf ("Error in MessageQ_free [%d]\n", status);
               }
           }
        }
    }

    printf ("Exchanged messages: tx %d, rx %d\n",
             i, i*NUM_SLAVE_MSGS_PER_HOST_MSG);
    printf ("Transferred %d KB\n", (i * PAYLOADSIZE) / 1024);

    if (status >= 0) {
        printf ("Sample application successfully completed!\n");
    }

cleanup_cmem:
    CMEM_free(payload, &cmemAttrs);

cleanup_close:
    MessageQ_close (&queueId);

cleanup_create:
    /* Clean-up */
    status = MessageQ_delete (&msgqHandle);
    if (status < 0) {
        printf ("Error in MessageQ_delete [%d]\n", status);
    }

exit:
    printf ("Leaving Nanotest_execute\n\n");

    return (status);
}

int main (int argc, char ** argv)
{
    Int32   status = 0;

    if (CMEM_init() < 0) {
        printf("CMEM_init failed\n");

        return(-1);
    }

    status = SysLink_setup();

    if (status >= 0) {
        Nanotest_execute();
        SysLink_destroy();
    }
    else {
        printf ("SysLink_setup failed: status = 0x%x\n", status);
    }

    return(0);
}
