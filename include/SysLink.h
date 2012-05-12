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
 *  @file       SysLink.h
 *
 *  @brief      This module contains startup/shutdown functions.
 *
 *  @ver        0002  (adapted from SysLink 2 GA product). 
 *
 */


#ifndef _SysLink_H_
#define _SysLink_H_


#if defined (__cplusplus)
extern "C" {
#endif

/*!
 *  @def    SysLink_S_ALREADYSETUP
 *  @brief  The module has been already setup
 */
#define SysLink_S_ALREADYSETUP      1

/*!
 *  @def    SysLink_S_SUCCESS
 *  @brief  Operation is successful.
 */
#define SysLink_S_SUCCESS           0

/*!
 *  @def    SysLink_E_FAIL
 *  @brief  Generic failure.
 */
#define SysLink_E_FAIL             -1

/*!
 *  @def    SysLink_E_ALREADYEXISTS
 *  @brief  The specified entity already exists.
 */
#define SysLink_E_ALREADYEXISTS    -2

/*!
 *  @def    SysLink_E_RESOURCE
 *  @brief  Specified resource is not available
 */
#define SysLink_E_RESOURCE         -3



/* =============================================================================
 * APIs
 * =============================================================================
 */
/**
 *  @brief      Function to initialize SysLink.
 *
 *              This function must be called in every user process before making
 *              calls to any other SysLink APIs.
 *
 *  @sa         SysLink_destroy()
 */
Int SysLink_setup (Void);

/**
 *  @brief      Function to finalize SysLink.
 *
 *              This function must be called in every user process at the end
 *              after all usage of SysLink in that process is complete.
 *
 *  @sa         SysLink_setup()
 */
Void SysLink_destroy (Void);

#if defined (__cplusplus)
}
#endif

#endif /*_SysLink_H_*/
