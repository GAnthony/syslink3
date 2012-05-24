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
 *  @file       SysLink.c
 *
 *  @brief      Initializes and finalizes user side SysLink
 *              All setup/destroy APIs on user side will be call from this 
 *              module.
 *
 *  @ver        0002
 *
 */

/* Standard headers */
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <Std.h>

/* Common IPC headers: */
#include <ti/ipc/NameServer.h>

/* User side headers */
#include <ladclient.h>
#include <SysLink.h>

/* IPC startup/shutdown stuff: */
#include <ti/ipc/MultiProc.h>
#include <_MessageQ.h>
#include <_NameServer.h>

static LAD_ClientHandle ladHandle;

static void cleanup(int arg);

/** ============================================================================
 *  Functions
 *  ============================================================================
 */
/* Function to initialize SysLink. */
Int SysLink_setup (Void)
{
    MessageQ_Config   msgqCfg;
    Int32             status = 0;
    LAD_Status        ladStatus;
    UInt16            rprocId;

    /* Catch ctrl-C, and cleanup: */
    (void) signal(SIGINT, cleanup);

    ladStatus = LAD_connect(&ladHandle);
    if (ladStatus != LAD_SUCCESS) {
        printf("SysLink_setup: LAD_connect() failed: %d\n", ladStatus);
        status = SysLink_E_RESOURCE;
        goto exit;
    }

    status = NameServer_setup();
    if (status >= 0) {
        MessageQ_getConfig(&msgqCfg);
        MessageQ_setup(&msgqCfg);
       
        /* Now attach to all remote processors, assuming they are up. */
        for (rprocId = 0; 
             (rprocId < MultiProc_getNumProcessors()) && (status >= 0); 
             rprocId++) {
           if (0 == rprocId) {
               /* Skip host, which should always be 0th entry. */
               continue;
           }
           status = MessageQ_attach (rprocId, NULL);
           if (status < 0) {
              printf("SysLink_setup: MessageQ_attach(%d) failed: %d\n", 
                     rprocId, status);
           }
        }
    }

exit:
    return (status);
}


/* Function to finalize SysLink. */
Void SysLink_destroy (Void)
{
    Int32             status = 0;
    LAD_Status        ladStatus;
    UInt16            rprocId;

    /* Now detach from all remote processors, assuming they are up. */
    for (rprocId = 0;
         (rprocId < MultiProc_getNumProcessors()) && (status >= 0);
         rprocId++) {
       if (0 == rprocId) {
          /* Skip host, which should always be 0th entry. */
          continue;
       }
       status = MessageQ_detach(rprocId);
       if (status < 0) {
          printf("SysLink_destroy: MessageQ_detach(%d) failed: %d\n",
                 rprocId, status);
       }
    }

    status = MessageQ_destroy();
    if (status < 0) {
       printf("SysLink_destroy: MessageQ_destroy() failed: %d\n", status);
    }

    status = NameServer_destroy();
    if (status < 0) {
       printf("SysLink_destroy: NameServer_destroy() failed: %d\n", status);
    }

    ladStatus = LAD_disconnect(ladHandle);
    if (ladStatus != LAD_SUCCESS) {
        printf("LAD_disconnect() failed: %d\n", ladStatus);
    }
}

static void cleanup(int arg)
{
    printf("SysLink: Caught SIGINT, calling SysLink_destroy...\n");
    SysLink_destroy();
    exit(0);
}
