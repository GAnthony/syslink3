/*
 *  Syslink-IPC for TI OMAP Processors
 *
 *  Copyright (c) 2008-2010, Texas Instruments Incorporated
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
 *  @file   _MultiProc.h
 *
 *  @brief  Header file for_MultiProc on HLOS side
 *  ============================================================================
 */


#ifndef _MULTIPROC_H_0XB522
#define _MULTIPROC_H_0XB522


#if defined (__cplusplus)
extern "C" {
#endif


/*!
 *  @brief  Max name length for a processor name.
 */
#define MultiProc_MAXNAMELENGTH 32

/*!
 *  @brief  Max number of processors supported.
 */
#define MultiProc_MAXPROCESSORS 10


/*!
 *  @brief  Configuration structure for MultiProc module
 */
typedef struct MultiProc_Config_tag {
    Int32  numProcessors;
    /*!< Max number of procs for particular system */
    Char   nameList [MultiProc_MAXPROCESSORS][MultiProc_MAXNAMELENGTH];
    /*!< Name List for processors in the system */
    UInt16 id;
    /*!< Local Proc ID. This needs to be set before calling any other APIs */
} MultiProc_Config;

/* This exported from family/<PLATFORM_DIR>/MultiProc_cfg.c: */
extern MultiProc_Config _MultiProc_cfg;

/* =============================================================================
 *  APIs
 * =============================================================================
 */
/*!
 *  @brief      Get the default configuration for the MultiProc module.
 *
 *              This function can be called by the application to get their
 *              configuration parameter to MultiProc_setup filled in by the
 *              MultiProc module with the default parameters.
 *
 *  @param      cfg        Pointer to the MultiProc module configuration
 *                         structure in which the default config is to be
 *                         returned.
 *
 *  @sa         MultiProc_setup
 */
Void MultiProc_getConfig (MultiProc_Config * cfg);

/*!
 *  @brief      Setup the MultiProc module.
 *
 *              This function sets up the MultiProc module. This function
 *              must be called before any other instance-level APIs can be
 *              invoked.
 *              Module-level configuration needs to be provided to this
 *              function. If the user wishes to change some specific config
 *              parameters, then MultiProc_getConfig can be called to get the
 *              configuration filled with the default values. After this, only
 *              the required configuration values can be changed.
 *
 *  @param      cfg   MultiProc module configuration.
 *
 *  @sa         MultiProc_destroy
 */
Int MultiProc_setup (MultiProc_Config * cfg);

/*!
 *  @brief      Destroy the MultiProc module.
 *
 *              Once this function is called, other MultiProc module APIs,
 *              except for the MultiProc_getConfig API cannot be called
 *              anymore.
 *
 *  @sa         MultiProc_setup
 */
Int MultiProc_destroy (Void);

#if defined (__cplusplus)
}
#endif

#endif
