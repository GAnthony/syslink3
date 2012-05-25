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
 *  @file       _NameServer.h
 *
 *  @brief      HLOS-specific NameServer header
 *
 */


#ifndef NameServer_H_0XF414
#define NameServer_H_0XF414

/* Utilities headers */
#include <ti/ipc/NameServer.h>

#if defined (__cplusplus)
extern "C" {
#endif


/* =============================================================================
 * Macros & Defines
 * =============================================================================
 */

/*
 * This must match on BIOS side. This will occupy queueIndex 0 of the MessageQ
 * module queues array, forcing MessageQ indicies to start from 1.
 */
#define NAME_SERVER_RPMSG_ADDR 0

/* =============================================================================
 * APIs
 * =============================================================================
 */
/*!
 *  @brief      Function to setup the nameserver module.
 *
 *  @sa         NameServer_destroy
 */
Int NameServer_setup (Void);

/*!
 *  @brief      Function to destroy the nameserver module.
 *
 *  @sa         NameServer_setup
 */
Int NameServer_destroy (void);

#if defined (__cplusplus)
}
#endif

#endif
