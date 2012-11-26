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
 *  ======== ladclient.h ========
 */

/**
 *  @file       ti/dsplink/utils/ladclient/ladclient.h
 *
 *  @brief      The Link Arbiter Daemon (LAD) communication interface.
 *          Provides wrapper functions to communicate with LAD, allowing
 *              a client to: establish a connection to LAD; and disconnect
 *              from LAD.
 */
/**
 *  @defgroup   _ladclient_LAD LAD - Link Arbiter Client Interface
 *
 *  This module provides an API to enable communication with the Link Arbiter
 *  Daemon (LAD).
 */

#ifndef _ladclient_LAD_
#define _ladclient_LAD_

#ifdef __cplusplus
extern "C" {
#endif

#include <_lad.h>

/* LAD return codes */
typedef enum {
    LAD_SUCCESS = 0,       /**< success */
    LAD_FAILURE,           /**< general failure */
    LAD_INVALIDARG,        /**< invalid argument */
    LAD_ACCESSDENIED,      /**< the request was denied */
    LAD_IOFAILURE,         /**< communication failure */
    LAD_NOTCONNECTED,      /**< not connected to LAD yet */
    LAD_INVALIDVERSION     /**< unsupported communication protocol */
} LAD_Status;

typedef UInt LAD_ClientHandle;  /**< handle for communicating with LAD  */


/*
 *  ======== LAD_connect ========
 */
/**
 *  @brief      Connect to LAD.
 *
 *  @param[out] handle    The new client handle, as defined by LAD.
 *
 *  @retval     LAD_SUCCESS    Success.
 *  @retval     LAD_INVALIDARG    The handle pointer is NULL.
 *  @retval     LAD_ACCESSDENIED    Returned on either of two conditions: this
 *              client is trying to establish a second active connection to
 *              LAD, and the request is denied; or, the total number of
 *              simultaneous client connections to LAD has been reached, and
 *              no more client handles are available.
 *  @retval     LAD_IOFAILURE    Unable to communicate with LAD, due to an
 *              OS-level I/O failure.  A full system reboot may be necessary.
 *  @retval     LAD_INVALIDVERSION    Unable to communicate with LAD due to a
 *              mismatch in the communication protocol between the client and
 *              LAD.
 *
 *  @sa         LAD_disconnect().
 */
extern LAD_Status LAD_connect(LAD_ClientHandle * handle);

/*
 *  ======== LAD_disconnect ========
 */
/**
 *  @brief      Disconnect from LAD.
 *
 *  @param[in]  handle    The client handle, as returned from previous call to
 *                        LAD_connect().
 *
 *  @retval     LAD_SUCCESS    Success.
 *  @retval     LAD_INVALIDARG    Invalid client handle.
 *  @retval     LAD_NOTCONNECTED    Not currently connected to LAD.
 *  @retval     LAD_STILLRUNNING    This client has previously started the DSP
 *              via a call to LAD_startupDsp, and must call LAD_releaseDsp
 *              before attempting to disconnect from LAD.
 *  @retval     LAD_IOFAILURE    Unable to communicate with LAD, due to an
 *              OS-level I/O failure, or timeout.  A full system reboot may be
 *              necessary.
 *
 *  @sa         LAD_connect().
 */
extern LAD_Status LAD_disconnect(LAD_ClientHandle handle);

extern LAD_ClientHandle LAD_findHandle(Void);
extern LAD_Status LAD_getResponse(LAD_ClientHandle handle, union LAD_ResponseObj *rsp);
extern LAD_Status LAD_putCommand(struct LAD_CommandObj *cmd);

#ifdef __cplusplus
}
#endif

#endif
