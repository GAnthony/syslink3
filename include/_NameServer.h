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
