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
 *  @file   NameServerApp.c
 *
 *  @brief  Sample application for NameServer module between MPU and Remote Proc
 *
 *  ============================================================================
 */

#define USE_NSD

/* Standard headers */
#include <stdio.h>

/* SysLink Standard Header: */
#include <Std.h>

#include <_NameServer.h>
#ifdef USE_NSD
#include <ladclient.h>
#endif

/* Module level headers */
#include <ti/ipc/NameServer.h>


/** ============================================================================
 *  Macros and types
 *  ============================================================================
 */
#define NSNAME "MessageQ"
#define NSNAME2 "MessageQ2"

/** ============================================================================
 *  Globals
 *  ============================================================================
 */

Int testNS(NameServer_Handle nsHandle, String name)
{
    Int32 status = 0;
    Ptr ptr;
    UInt32 val;
    char key[16];
    Int i;

    ptr = NameServer_addUInt32(nsHandle, name, 0xdeadbeef);
    if (ptr == NULL) {
        printf("Failed to NameServer_addUInt32()\n");
        return -1;
    }
    else {
        printf("NameServer_addUInt32() returned %p\n", ptr);
    }

    printf("Trying to add same key (should fail)...\n");
    ptr = NameServer_addUInt32(nsHandle, name, 0xdeadc0de);
    if (ptr == NULL) {
        printf("    ...got expected Failure from NameServer_addUInt32()\n");
    }
    else {
        printf("    Error: NameServer_addUInt32() returned non-NULL %p (but was expected to fail)\n", ptr);
        return -1;
    }

    val = 0x00c0ffee;
    status = NameServer_getUInt32(nsHandle, name, &val, NULL);
    printf("NameServer_getUInt32() returned %d, val=0x%x (was 0x00c0ffee)\n", status, val);

    printf("Removing 0xdeadbeef w/ NameServer_remove()...\n");
    status = NameServer_remove(nsHandle, name);
    if (status < 0) {
        printf("NameServer_remove() failed: %d\n", status);
        return -1;
    }

    ptr = NameServer_addUInt32(nsHandle, name, 0xdeadc0de);
    if (ptr == NULL) {
        printf("Error: NameServer_addUInt32() failed\n");
        return -1;
    }
    else {
        printf("NameServer_addUInt32(0xdeadc0de) succeeded\n");
    }

    val = 0x00c0ffee;
    status = NameServer_getUInt32(nsHandle, name, &val, NULL);
    printf("NameServer_getUInt32() returned %d, val=0x%x (was 0x00c0ffee)\n", status, val);

    printf("Removing 0xdeadc0de w/ NameServer_removeEntry()...\n");
    status = NameServer_removeEntry(nsHandle, ptr);
    if (status < 0) {
        printf("NameServer_remove() failed: %d\n", status);
        return -1;
    }

    ptr = NameServer_addUInt32(nsHandle, name, 0x0badc0de);
    if (ptr == NULL) {
        printf("Error: NameServer_addUInt32() failed\n");
        return -1;
    }
    else {
        printf("NameServer_addUInt32(0x0badc0de) succeeded\n");
    }

    val = 0x00c0ffee;
    status = NameServer_getUInt32(nsHandle, name, &val, NULL);
    printf("NameServer_getUInt32() returned %d, val=0x%x (was 0x00c0ffee)\n", status, val);

    status = NameServer_remove(nsHandle, name);
    if (status < 0) {
        printf("Error: NameServer_remove() failed\n");
        return -1;
    }
    else {
        printf("NameServer_remove(%s) succeeded\n", name);
    }

    for (i = 0; i < 10; i++) {
        sprintf(key, "foobar%d", i);

        ptr = NameServer_addUInt32(nsHandle, key, 0x0badc0de + i);
        if (ptr == NULL) {
            printf("Error: NameServer_addUInt32() failed\n");
            return -1;
        }
        else {
            printf("NameServer_addUInt32(%s, 0x%x) succeeded\n", key, 0x0badc0de + i);
        }

        val = 0x00c0ffee;
        status = NameServer_getUInt32(nsHandle, key, &val, NULL);
        printf("NameServer_getUInt32(%s) returned %d, val=0x%x (was 0x00c0ffee)\n", key, status, val);

        if (val != (0x0badc0de + i)) {
            printf("get val (0x%x) != add val (0x%x)!\n", val, 0x0badc0de + i);
        }
    }

    for (i = 0; i < 10; i++) {
        sprintf(key, "foobar%d", i);

        status = NameServer_remove(nsHandle, key);
        if (status < 0) {
            printf("Error: NameServer_remove() failed\n");
            return -1;
        }
        else {
            printf("NameServer_remove(%s) succeeded\n", key);
        }
    }

    return 0;
}

/** ============================================================================
 *  Functions
 *  ============================================================================
 */
Int
NameServerApp_startup()
{
    Int32 status = 0;
    NameServer_Params params;
    NameServer_Handle nsHandle;
    NameServer_Handle nsHandle2;
    Int iteration = 0;
#ifdef USE_NSD
    LAD_ClientHandle ladHandle;
    LAD_Status ladStatus;
#endif

    printf ("Entered NameServerApp_startup\n");

#ifdef USE_NSD
    ladStatus = LAD_connect(&ladHandle);
    if (ladStatus != LAD_SUCCESS) {
        printf("LAD_connect() failed: %d\n", ladStatus);
        return -1;
    }
    else {
        printf("LAD_connect() succeeded: ladHandle=%d\n", ladHandle);
    }
#endif

    printf("Calling NameServer_setup()...\n");
    NameServer_setup();

again:
    NameServer_Params_init(&params);
    printf("params.maxValueLen=%d\n", params.maxValueLen);
    printf("params.maxNameLen=%d\n", params.maxNameLen);
    printf("params.checkExisting=%d\n", params.checkExisting);

    params.maxValueLen = sizeof(UInt32);
    params.maxNameLen = 32;
    nsHandle = NameServer_create(NSNAME, &params);
    if (nsHandle == NULL) {
        printf("Failed to create NameServer '%s'\n", NSNAME);
        return -1;
    }
    else {
        printf("Created NameServer '%s'\n", NSNAME);
    }

    NameServer_Params_init(&params);

    params.maxValueLen = sizeof(UInt32);
    params.maxNameLen = 32;
    nsHandle2 = NameServer_create(NSNAME2, &params);
    if (nsHandle2 == NULL) {
        printf("Failed to create NameServer '%s'\n", NSNAME2);
        return -1;
    }
    else {
        printf("Created NameServer '%s'\n", NSNAME2);
    }

    status = testNS(nsHandle, "Key");
    status = testNS(nsHandle2, "Key");

    printf("Deleting nsHandle and nsHandle2...\n");
    NameServer_delete(&nsHandle);
    NameServer_delete(&nsHandle2);

    iteration++;
    if (iteration < 2) {
        goto again;
    }

    printf("Calling NameServer_destroy()...\n");
    NameServer_destroy();

#ifdef USE_NSD
    ladStatus = LAD_disconnect(ladHandle);
    if (ladStatus != LAD_SUCCESS) {
        printf("LAD_disconnect() failed: %d\n", ladStatus);
        return -1;
    }
    else {
        printf("LAD_disconnect() succeeded\n");
    }
#endif

    printf ("Leaving NameServerApp_startup: status = 0x%x\n", status);

    return status;
}


Int
NameServerApp_execute()
{
    Int32 status = 0;

    printf ("Entered NameServerApp_execute\n");

    printf ("Leaving NameServerApp_execute\n\n");

    return status;
}

Int
NameServerApp_shutdown()
{
    Int32 status = 0;

    printf ("Entered NameServerApp_shutdown()\n");

    printf ("Leave NameServerApp_shutdown()\n");

    return status;
}

int
main (int argc, char ** argv)
{
    NameServerApp_startup();
    NameServerApp_execute();
    NameServerApp_shutdown();

    return(0);
}

