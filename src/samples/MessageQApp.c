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
#include <sys/types.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

/* SysLink Standard Header: */
#include <Std.h>

/* Module level headers */
#include <ti/ipc/MessageQ.h>
#include <ti/ipc/MultiProc.h>

#include <_MessageQ.h>

/* App defines */
#define MSGSIZE                     64u
/* Must match on remote proc side: */
#define HEAPID                      0u

#define DUCATI_CORE0_MESSAGEQNAME   "CORE0"
#define ARM_MESSAGEQNAME            "HOST"

/** ============================================================================
 *  Macros and types
 *  ============================================================================
 */

/*!
 *  @brief  Number of transfers to be tested.
 */
#define  NUM_LOOPS  100

/** ============================================================================
 *  Globals
 *  ============================================================================
 */
MessageQ_Handle                MessageQApp_messageQ;
MessageQ_QueueId               MessageQApp_queueId = MessageQ_INVALIDMESSAGEQ;
UInt16                         MessageQApp_procId;

/** ============================================================================
 *  Functions
 *  ============================================================================
 */
Int
MessageQApp_startup ()
{
    Int32             status = 0;
    MessageQ_Config   cfg;

    printf ("Entered MessageQApp_startup\n");

    /* SysLink 2 Backplane stuff: Also need NameServer and MultiProc config. */
    MessageQ_getConfig (&cfg);
    MessageQ_setup (&cfg);
    MessageQApp_procId = MultiProc_getId("SysM3");
    status = MessageQ_attach (MessageQApp_procId, NULL);

    printf ("Leaving MessageQApp_startup: status = 0x%x\n", status);

    return (status);
}


Int
MessageQApp_execute ()
{
    Int32                    status     = 0;
    MessageQ_Msg             msg        = NULL;
    MessageQ_Params          msgParams;
    UInt16                   i;

    printf ("Entered MessageQApp_execute\n");

    /* Create the local Message Queue for receiving. */
    MessageQ_Params_init (&msgParams);
    MessageQApp_messageQ = MessageQ_create (ARM_MESSAGEQNAME, &msgParams);
    if (MessageQApp_messageQ == NULL) {
        printf ("Error in MessageQ_create\n");
        goto exit;
    }
    else {
        printf ("Local messageQ id: 0x%x\n",
            MessageQ_getQueueId(MessageQApp_messageQ));
    }

    /* Wait until remote side has it's messageQ created before we send: */
    do {
        status = MessageQ_open (DUCATI_CORE0_MESSAGEQNAME,
                       &MessageQApp_queueId);
    } while (status == MessageQ_E_NOTFOUND);
    if (status < 0) {
        printf ("Error in MessageQ_open [0x%x]\n", status);
        goto cleanup;
    }
    else {
        printf ("Remote MessageQApp_queueId  [0x%x]\n", MessageQApp_queueId);
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
          MessageQ_setReplyQueue (MessageQApp_messageQ, msg);

          status = MessageQ_put (MessageQApp_queueId, msg);
          if (status < 0) {
              printf ("Error in MessageQ_put [0x%x]\n", status);
              break;
          }

          if (i == 0) {
		/* TEMP: Need a little delay on first socket recvfrom() call: */
		sleep (1);
          }

          status = MessageQ_get(MessageQApp_messageQ, &msg, MessageQ_FOREVER);
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

          printf ("Exchanged %d messages with remote processor\n", (i+1));
    }

    printf ("Sample application successfully completed!\n");

    MessageQ_close (&MessageQApp_queueId);

cleanup:
    /* Clean-up */
    status = MessageQ_delete (&MessageQApp_messageQ);
    if (status < 0) {
        printf ("Error in MessageQ_delete [0x%x]\n", status);
    }

exit:
    printf ("Leaving MessageQApp_execute\n\n");

    return (status);
}

Int
MessageQApp_shutdown ()
{
    Int32               status = 0;

    printf ("Entered MessageQApp_shutdown()\n");

    /* SysLink 2 Backplane stuff: move to ???? */
    status = MessageQ_detach (MessageQApp_procId);
    MessageQ_destroy ();

    printf ("Leave MessageQApp_shutdown()\n");

    return (status);
}

int
main (int argc, char ** argv)
{
    MessageQApp_startup ();
    MessageQApp_execute ();
    MessageQApp_shutdown ();

    return(0);
}

