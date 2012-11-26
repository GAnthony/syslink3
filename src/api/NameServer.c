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
 *  ======== NameServer_client.c ========
 */
#include <Std.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>

#include <ti/ipc/NameServer.h>

#include <ladclient.h>
#include <_lad.h>

static Bool verbose = FALSE;


/*
 * The NameServer_*() APIs are reproduced here.  These versions are just
 * front-ends for communicating with the actual NameServer module (currently
 * implemented as a daemon process ala LAD).
 */

Int NameServer_setup(Void)
{
    Int status;
    LAD_ClientHandle handle;
    struct LAD_CommandObj cmd;
    union LAD_ResponseObj rsp;

    handle = LAD_findHandle();
    if (handle == LAD_MAXNUMCLIENTS) {
        PRINTVERBOSE1(
          "NameServer_setup: can't find connection to daemon for pid %d\n",
           getpid())

        return NameServer_E_RESOURCE;
    }

    cmd.cmd = LAD_NAMESERVER_SETUP;
    cmd.clientId = handle;

    if ((status = LAD_putCommand(&cmd)) != LAD_SUCCESS) {
        PRINTVERBOSE1(
          "NameServer_setup: sending LAD command failed, status=%d\n", status)
        return NameServer_E_FAIL;
    }

    if ((status = LAD_getResponse(handle, &rsp)) != LAD_SUCCESS) {
        PRINTVERBOSE1("NameServer_setup: no LAD response, status=%d\n", status)
        return(status);
    }
    status = rsp.status;

    PRINTVERBOSE2(
      "NameServer_setup: got LAD response for client %d, status=%d\n",
      handle, status)

    return status;
}

Int NameServer_destroy(Void)
{
    Int status;
    LAD_ClientHandle handle;
    struct LAD_CommandObj cmd;
    union LAD_ResponseObj rsp;

    PRINTVERBOSE0("NameServer_destroy: entered\n")

    handle = LAD_findHandle();
    if (handle == LAD_MAXNUMCLIENTS) {
        PRINTVERBOSE1(
          "NameServer_destroy: can't find connection to daemon for pid %d\n",
          getpid())

        return NameServer_E_RESOURCE;
    }

    cmd.cmd = LAD_NAMESERVER_DESTROY;
    cmd.clientId = handle;

    if ((status = LAD_putCommand(&cmd)) != LAD_SUCCESS) {
        PRINTVERBOSE1(
          "NameServer_destroy: sending LAD command failed, status=%d\n", status)
        return NameServer_E_FAIL;
    }

    if ((status = LAD_getResponse(handle, &rsp)) != LAD_SUCCESS) {
        PRINTVERBOSE1(
          "NameServer_destroy: no LAD response, status=%d\n", status)
        return(status);
    }
    status = rsp.status;

    PRINTVERBOSE2(
     "NameServer_destroy: got LAD response for client %d, status=%d\n",
     handle, status)

    return status;
}

Void NameServer_Params_init(NameServer_Params *params)
{
    Int status;
    LAD_ClientHandle handle;
    struct LAD_CommandObj cmd;
    union LAD_ResponseObj rsp;

    handle = LAD_findHandle();
    if (handle == LAD_MAXNUMCLIENTS) {
        PRINTVERBOSE1(
         "NameServer_Params_init: can't find connection to daemon for pid %d\n",
         getpid())

        return;
    }

    cmd.cmd = LAD_NAMESERVER_PARAMS_INIT;
    cmd.clientId = handle;

    if ((status = LAD_putCommand(&cmd)) != LAD_SUCCESS) {
        PRINTVERBOSE1(
          "NameServer_Params_init: sending LAD command failed, status=%d\n",
          status)
        return;
    }

    if ((status = LAD_getResponse(handle, &rsp)) != LAD_SUCCESS) {
        PRINTVERBOSE1(
          "NameServer_Params_init: no LAD response, status=%d\n", status)
        return;
    }

    PRINTVERBOSE1("NameServer_Params_init: got LAD response for client %d\n",
                  handle)

    memcpy(params, &rsp.params, sizeof(NameServer_Params));

    return;
}

NameServer_Handle NameServer_create(String name,
                                    const NameServer_Params *params)
{
    Int status;
    LAD_ClientHandle handle;
    struct LAD_CommandObj cmd;
    union LAD_ResponseObj rsp;

    handle = LAD_findHandle();
    if (handle == LAD_MAXNUMCLIENTS) {
        PRINTVERBOSE1(
          "NameServer_create: can't find connection to daemon for pid %d\n",
          getpid())

        return NULL;
    }

    cmd.cmd = LAD_NAMESERVER_CREATE;
    cmd.clientId = handle;
    strncpy(cmd.args.create.name, name, NameServer_Params_MAXNAMELEN);
    memcpy(&cmd.args.create.params, params, sizeof(NameServer_Params));

    if ((status = LAD_putCommand(&cmd)) != LAD_SUCCESS) {
        PRINTVERBOSE1(
          "NameServer_create: sending LAD command failed, status=%d\n",
          status)
        return NULL;
    }

    if ((status = LAD_getResponse(handle, &rsp)) != LAD_SUCCESS) {
        PRINTVERBOSE1("NameServer_create: no LAD response, status=%d\n", status)
        return NULL;
    }

    PRINTVERBOSE1("NameServer_create: got LAD response for client %d\n", handle)

    return rsp.handle;
}

Ptr NameServer_addUInt32(NameServer_Handle nsHandle, String name, UInt32 value)
{
    Int status;
    LAD_ClientHandle clHandle;
    struct LAD_CommandObj cmd;
    union LAD_ResponseObj rsp;

    clHandle = LAD_findHandle();
    if (clHandle == LAD_MAXNUMCLIENTS) {
        PRINTVERBOSE1(
          "NameServer_addUInt32: can't find connection to daemon for pid %d\n",
          getpid())

        return NULL;
    }

    cmd.cmd = LAD_NAMESERVER_ADDUINT32;
    cmd.clientId = clHandle;
    cmd.args.addUInt32.handle = nsHandle;
    strncpy(cmd.args.addUInt32.name, name, NameServer_Params_MAXNAMELEN);
    cmd.args.addUInt32.val = value;

    if ((status = LAD_putCommand(&cmd)) != LAD_SUCCESS) {
        PRINTVERBOSE1(
          "NameServer_addUInt32: sending LAD command failed, status=%d\n",
          status)
        return NULL;
    }

    if ((status = LAD_getResponse(clHandle, &rsp)) != LAD_SUCCESS) {
        PRINTVERBOSE1(
           "NameServer_addUInt32: no LAD response, status=%d\n", status)
        return NULL;
    }

    PRINTVERBOSE1(
       "NameServer_addUInt32: got LAD response for client %d\n", clHandle)

    return rsp.entryPtr;
}

Int NameServer_getUInt32(NameServer_Handle nsHandle, String name, Ptr buf,
                          UInt16 procId[])
{
    Int status;
    LAD_ClientHandle clHandle;
    UInt32 *val;
    struct LAD_CommandObj cmd;
    union LAD_ResponseObj rsp;

    clHandle = LAD_findHandle();
    if (clHandle == LAD_MAXNUMCLIENTS) {
        PRINTVERBOSE1(
          "NameServer_getUInt32: can't find connection to daemon for pid %d\n",
           getpid())

        return NameServer_E_RESOURCE;
    }

    cmd.cmd = LAD_NAMESERVER_GETUINT32;
    cmd.clientId = clHandle;
    cmd.args.getUInt32.handle = nsHandle;
    strncpy(cmd.args.getUInt32.name, name, NameServer_Params_MAXNAMELEN);
    if (procId != NULL) {
        memcpy(cmd.args.getUInt32.procId, procId,
               sizeof(UInt16) * MultiProc_MAXPROCESSORS);
    }
    else {
        cmd.args.getUInt32.procId[0] = (UInt16)-1;
    }

    if ((status = LAD_putCommand(&cmd)) != LAD_SUCCESS) {
        PRINTVERBOSE1(
           "NameServer_getUInt32: sending LAD command failed, status=%d\n",
            status)
        return NameServer_E_FAIL;
    }

    if ((status = LAD_getResponse(clHandle, &rsp)) != LAD_SUCCESS) {
        PRINTVERBOSE1("NameServer_getUInt32: no LAD response, status=%d\n",
                       status)
        return NameServer_E_FAIL;
    }

    val = (UInt32 *)buf;
    *val = rsp.getUInt32.val;
    status = rsp.status;

    PRINTVERBOSE1("NameServer_getUInt32: got LAD response for client %d\n",
                   clHandle)

    return status;
}

Int NameServer_remove(NameServer_Handle nsHandle, String name)
{
    Int status;
    LAD_ClientHandle clHandle;
    struct LAD_CommandObj cmd;
    union LAD_ResponseObj rsp;

    clHandle = LAD_findHandle();
    if (clHandle == LAD_MAXNUMCLIENTS) {
        PRINTVERBOSE1(
         "NameServer_remove: can't find connection to daemon for pid %d\n",
         getpid())

        return NameServer_E_RESOURCE;
    }

    cmd.cmd = LAD_NAMESERVER_REMOVE;
    cmd.clientId = clHandle;
    cmd.args.remove.handle = nsHandle;
    strncpy(cmd.args.remove.name, name, NameServer_Params_MAXNAMELEN);

    if ((status = LAD_putCommand(&cmd)) != LAD_SUCCESS) {
        PRINTVERBOSE1(
         "NameServer_remove: sending LAD command failed, status=%d\n",
         status)
        return NameServer_E_FAIL;
    }

    if ((status = LAD_getResponse(clHandle, &rsp)) != LAD_SUCCESS) {
        PRINTVERBOSE1("NameServer_remove: no LAD response, status=%d\n", status)
        return NameServer_E_FAIL;
    }

    status = rsp.status;

    PRINTVERBOSE1("NameServer_remove: got LAD response for client %d\n",
                   clHandle)

    return status;
}

Int NameServer_removeEntry(NameServer_Handle nsHandle, Ptr entry)
{
    Int status;
    LAD_ClientHandle clHandle;
    struct LAD_CommandObj cmd;
    union LAD_ResponseObj rsp;

    clHandle = LAD_findHandle();
    if (clHandle == LAD_MAXNUMCLIENTS) {
        PRINTVERBOSE1(
         "NameServer_removeEntry: can't find connection to daemon for pid %d\n",
         getpid())

        return NameServer_E_RESOURCE;
    }

    cmd.cmd = LAD_NAMESERVER_REMOVEENTRY;
    cmd.clientId = clHandle;
    cmd.args.removeEntry.handle = nsHandle;
    cmd.args.removeEntry.entryPtr = entry;

    if ((status = LAD_putCommand(&cmd)) != LAD_SUCCESS) {
        PRINTVERBOSE1(
          "NameServer_removeEntry: sending LAD command failed, status=%d\n",
          status)
        return NameServer_E_FAIL;
    }

    if ((status = LAD_getResponse(clHandle, &rsp)) != LAD_SUCCESS) {
        PRINTVERBOSE1("NameServer_removeEntry: no LAD response, status=%d\n",
                       status)
        return NameServer_E_FAIL;
    }

    status = rsp.status;

    PRINTVERBOSE1("NameServer_removeEntry: got LAD response for client %d\n",
                   clHandle)

    return status;
}

Int NameServer_delete(NameServer_Handle *nsHandle)
{
    Int status;
    LAD_ClientHandle clHandle;
    struct LAD_CommandObj cmd;
    union LAD_ResponseObj rsp;

    clHandle = LAD_findHandle();
    if (clHandle == LAD_MAXNUMCLIENTS) {
        PRINTVERBOSE1(
          "NameServer_delete: can't find connection to daemon for pid %d\n",
          getpid())

        return NameServer_E_RESOURCE;
    }

    cmd.cmd = LAD_NAMESERVER_DELETE;
    cmd.clientId = clHandle;
    cmd.args.delete.handle = *nsHandle;

    if ((status = LAD_putCommand(&cmd)) != LAD_SUCCESS) {
        PRINTVERBOSE1(
          "NameServer_delete: sending LAD command failed, status=%d\n",
          status)
        return NameServer_E_FAIL;
    }

    if ((status = LAD_getResponse(clHandle, &rsp)) != LAD_SUCCESS) {
        PRINTVERBOSE1("NameServer_delete: no LAD response, status=%d\n", status)
        return NameServer_E_FAIL;
    }

    *nsHandle = rsp.delete.handle;
    status = rsp.status;

    PRINTVERBOSE1("NameServer_delete: got LAD response for client %d\n",
                   clHandle)

    return status;
}
