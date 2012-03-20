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

/* This must match on BIOS side. This will occupy queueIndex 0 of the MessageQ
 * module queues array, forcing MessageQ indicies to start from 1.
 */
#define NAME_SERVER_RPMSG_ADDR 0

/* =============================================================================
 * Struct & Enums
 * =============================================================================
 */
/*!
 *  @brief  Module configuration structure.
 */
typedef struct NameServer_Config_tag {
    UInt32 reserved;
    /*!< Reserved value. */
} NameServer_Config;


/* =============================================================================
 * APIs
 * =============================================================================
 */
/*!
 *  @brief      Get the default configuration for the NameServer module.
 *
 *              This function can be called by the application to get their
 *              configuration parameter to NameServer_setup filled in by the
 *              NameServer module with the default parameters. If the user
 *              does not wish to make any change in the default parameters, this
 *              API is not required to be called.
 *
 *  @param      cfg        Pointer to the NameServer module configuration
 *                         structure in which the default config is to be
 *                         returned.
 *
 *  @sa         NameServer_setup
 */
Void NameServer_getConfig (NameServer_Config * cfg);

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
#endif /* defined (__cplusplus) */

#endif /* NameServer_H_0X5B4D */
