/*
 *  Copyright (c) 2008-2012, Texas Instruments Incorporated
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *  *  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *
 *  *  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 *  *  Neither the name of Texas Instruments Incorporated nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 *  THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 *  PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 *  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 *  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 *  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 *  OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 *  WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 *  OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 *  EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/** ============================================================================
 *  @file   _MessageQ.h
 *
 *  @brief  Defines MessageQ module.
 *  ============================================================================
 */


#ifndef MESSAGEQ_H_0xded2
#define MESSAGEQ_H_0xded2

/* Utilities headers */
#include <ti/ipc/NameServer.h>


#if defined (__cplusplus)
extern "C" {
#endif


/*!
 *  @def    MessageQ_MODULEID
 *  @brief  Unique module ID.
 */
#define MessageQ_MODULEID               (0xded2)

/*!
 *  @def    MessageQ_ALLOWGROWTH
 *  @brief  Allow runtime growth
 */
#define MessageQ_ALLOWGROWTH             NameServer_ALLOWGROWTH

/*! @brief Number of queues */
#define MessageQ_NUM_PRIORITY_QUEUES  2

/*
 *  Used to denote a message that was initialized
 *  with the #MessageQ_staticMsgInit function.
 */
#define MessageQ_STATICMSG              0xFFFF

/*! Version setting */
#define MessageQ_HEADERVERSION   (UInt) 0x2000

/*! Mask to extract Trace setting */
#define MessageQ_TRACEMASK       (UInt) 0x1000

/*! Shift for Trace setting */
#define MessageQ_TRACESHIFT      (UInt) 12


/*!
 *  @brief  Structure defining config parameters for the MessageQ Buf module.
 */
typedef struct MessageQ_Config_tag {
    Bool   traceFlag;
    /*!< Trace flag
     *  This flag allows the configuration of the default module trace
     *  settings.
     */
    UInt maxRuntimeEntries;
    /*!< Maximum number of MessageQs that can be dynamically created */
    UInt maxNameLen;
    /*!< Maximum length for Message queue names */
} MessageQ_Config;

/* =============================================================================
 *  APIs
 * =============================================================================
 */
/*!
 *  @brief      Function to get the default configuration for the MessageQ
 *              module.
 *
 *              This function can be called by the application to get their
 *              configuration parameter to MessageQ_setup filled in by the
 *              MessageQ module with the default parameters. If the user does
 *              not wish to make any change in the default parameters, this API
 *              is not required to be called.
 *
 *  @param      cfg     Pointer to the MessageQ module configuration structure
 *                      in which the default config is to be returned.
 *
 *  @sa         MessageQ_setup
 */
Void MessageQ_getConfig (MessageQ_Config * cfg);

/*!
 *  @brief      Function to setup the MessageQ module.
 *
 *              This function sets up the MessageQ module. This function must
 *              be called before any other instance-level APIs can be invoked.
 *              Module-level configuration needs to be provided to this
 *              function. If the user wishes to change some specific config
 *              parameters, then MessageQ_getConfig can be called to get the
 *              configuration filled with the default values. After this, only
 *              the required configuration values can be changed. If the user
 *              does not wish to make any change in the default parameters, the
 *              application can simply call MessageQ with NULL parameters.
 *              The default parameters would get automatically used.
 *
 *  @param      cfg   Optional MessageQ module configuration. If provided as
 *                    NULL, default configuration is used.
 *
 *  @sa         MessageQ_destroy
 *              NameServer_create
 *              GateSpinlock_create
 *              Memory_alloc
 */
Int MessageQ_setup (const MessageQ_Config * cfg);

/* Function to destroy the MessageQ module. */
Int MessageQ_destroy (void);

/* Returns the amount of shared memory used by one transport instance.
 *
 *  The MessageQ module itself does not use any shared memory but the
 *  underlying transport may use some shared memory.
 */
SizeT MessageQ_sharedMemReq (Ptr sharedAddr);

/* Calls the SetupProxy function to setup the MessageQ transports. */
Int MessageQ_attach (UInt16 remoteProcId, Ptr sharedAddr);

/* Calls the SetupProxy function to detach the MessageQ transports. */
Int MessageQ_detach (UInt16 remoteProcId);

NameServer_Handle MessageQ_getNameServerHandle(void);

Void MessageQ_setQueueOwner(MessageQ_Handle handle, Int pid);

Void MessageQ_cleanupOwner(Int pid);

Void MessageQ_msgInit(MessageQ_Msg msg);

#if defined (__cplusplus)
}
#endif /* defined (__cplusplus) */


#endif /* MESSAGEQ_H_0xded2 */
