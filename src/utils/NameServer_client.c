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

typedef struct _LAD_ClientInfo {
    Bool connectedToLAD;               /* connection status */
    UInt PID;                                     /* client's process ID */
    Char responseFIFOName[LAD_MAXLENGTHFIFONAME]; /* response FIFO name */
    FILE *responseFIFOFilePtr;                    /* FIFO file pointer */
} _LAD_ClientInfo;

static Bool initialized = FALSE;
static String commandFIFOFileName = LAD_COMMANDFIFO;
static FILE *commandFIFOFilePtr = NULL;
static _LAD_ClientInfo clientInfo[LAD_MAXNUMCLIENTS];

static LAD_Status putCommand(struct LAD_CommandObj *cmd);
static LAD_Status getResponse(LAD_ClientHandle handle,
                              union LAD_ResponseObj *rsp);
static LAD_Status initWrappers(Void);
static Bool openCommandFIFO(Void);
// only _NP (non-portable) type available in CG tools which we're using
static pthread_mutex_t modGate  = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
// static pthread_mutex_t modGate  = PTHREAD_RECURSIVE_MUTEX_INITIALIZER;


/*
 * findHandle() - finds the LAD_ClientHandle for the calling pid (process ID).
 *
 * Assumes that there is only one client per process, which has to be the
 * case since the pid is used to construct the responseFIFOFileName.
 *
 * Multiple threads within a process can all connect since each thread gets
 * its own pid (which might be an OS-specific thing, some OSes (even some
 * Linux implementations) use the same process pid for every thread within
 * a process).
 *
 * Returns either the found "handle", or LAD_MAXNUMCLIENTS if the handle
 * can't be found.
 */
LAD_ClientHandle findHandle(Void)
{
    Int i;
    Int pid;

    pid = getpid();

    for (i = 0; i < LAD_MAXNUMCLIENTS; i++) {
        if (clientInfo[i].PID == pid &&
            clientInfo[i].connectedToLAD == TRUE) {
            break;
        }
    }

    return i;
}

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

    handle = findHandle();
    if (handle == LAD_MAXNUMCLIENTS) {
        PRINTVERBOSE1(
          "NameServer_setup: can't find connection to daemon for pid %d\n",
           getpid())

        return NameServer_E_RESOURCE;
    }

    cmd.cmd = LAD_NAMESERVER_SETUP;
    cmd.clientId = handle;

    if ((status = putCommand(&cmd)) != LAD_SUCCESS) {
        PRINTVERBOSE1(
          "NameServer_setup: sending LAD command failed, status=%d\n", status)
        return NameServer_E_FAIL;
    }

    if ((status = getResponse(handle, &rsp)) != LAD_SUCCESS) {
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

    handle = findHandle();
    if (handle == LAD_MAXNUMCLIENTS) {
        PRINTVERBOSE1(
          "NameServer_destroy: can't find connection to daemon for pid %d\n",
          getpid())

        return NameServer_E_RESOURCE;
    }

    cmd.cmd = LAD_NAMESERVER_DESTROY;
    cmd.clientId = handle;

    if ((status = putCommand(&cmd)) != LAD_SUCCESS) {
        PRINTVERBOSE1(
          "NameServer_destroy: sending LAD command failed, status=%d\n", status)
        return NameServer_E_FAIL;
    }

    if ((status = getResponse(handle, &rsp)) != LAD_SUCCESS) {
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

    handle = findHandle();
    if (handle == LAD_MAXNUMCLIENTS) {
        PRINTVERBOSE1(
         "NameServer_Params_init: can't find connection to daemon for pid %d\n",
         getpid())

        return;
    }

    cmd.cmd = LAD_NAMESERVER_PARAMS_INIT;
    cmd.clientId = handle;

    if ((status = putCommand(&cmd)) != LAD_SUCCESS) {
        PRINTVERBOSE1(
          "NameServer_Params_init: sending LAD command failed, status=%d\n",
          status)
        return;
    }

    if ((status = getResponse(handle, &rsp)) != LAD_SUCCESS) {
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

    handle = findHandle();
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

    if ((status = putCommand(&cmd)) != LAD_SUCCESS) {
        PRINTVERBOSE1(
          "NameServer_create: sending LAD command failed, status=%d\n",
          status)
        return NULL;
    }

    if ((status = getResponse(handle, &rsp)) != LAD_SUCCESS) {
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

    clHandle = findHandle();
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

    if ((status = putCommand(&cmd)) != LAD_SUCCESS) {
        PRINTVERBOSE1(
          "NameServer_addUInt32: sending LAD command failed, status=%d\n",
          status)
        return NULL;
    }

    if ((status = getResponse(clHandle, &rsp)) != LAD_SUCCESS) {
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

    clHandle = findHandle();
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

    if ((status = putCommand(&cmd)) != LAD_SUCCESS) {
        PRINTVERBOSE1(
           "NameServer_getUInt32: sending LAD command failed, status=%d\n",
            status)
        return NameServer_E_FAIL;
    }

    if ((status = getResponse(clHandle, &rsp)) != LAD_SUCCESS) {
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

    clHandle = findHandle();
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

    if ((status = putCommand(&cmd)) != LAD_SUCCESS) {
        PRINTVERBOSE1(
         "NameServer_remove: sending LAD command failed, status=%d\n",
         status)
        return NameServer_E_FAIL;
    }

    if ((status = getResponse(clHandle, &rsp)) != LAD_SUCCESS) {
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

    clHandle = findHandle();
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

    if ((status = putCommand(&cmd)) != LAD_SUCCESS) {
        PRINTVERBOSE1(
          "NameServer_removeEntry: sending LAD command failed, status=%d\n",
          status)
        return NameServer_E_FAIL;
    }

    if ((status = getResponse(clHandle, &rsp)) != LAD_SUCCESS) {
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

    clHandle = findHandle();
    if (clHandle == LAD_MAXNUMCLIENTS) {
        PRINTVERBOSE1(
          "NameServer_delete: can't find connection to daemon for pid %d\n",
          getpid())

        return NameServer_E_RESOURCE;
    }

    cmd.cmd = LAD_NAMESERVER_DELETE;
    cmd.clientId = clHandle;
    cmd.args.delete.handle = *nsHandle;

    if ((status = putCommand(&cmd)) != LAD_SUCCESS) {
        PRINTVERBOSE1(
          "NameServer_delete: sending LAD command failed, status=%d\n",
          status)
        return NameServer_E_FAIL;
    }

    if ((status = getResponse(clHandle, &rsp)) != LAD_SUCCESS) {
        PRINTVERBOSE1("NameServer_delete: no LAD response, status=%d\n", status)
        return NameServer_E_FAIL;
    }

    *nsHandle = rsp.delete.handle;
    status = rsp.status;

    PRINTVERBOSE1("NameServer_delete: got LAD response for client %d\n",
                   clHandle)

    return status;
}


/*
 *  ======== LAD_connect ========
 */
LAD_Status  LAD_connect(LAD_ClientHandle * handle)
{
    Char responseFIFOName[LAD_MAXLENGTHFIFONAME];
    LAD_Status status = LAD_SUCCESS;
    time_t currentTime;
    time_t startTime;
    struct stat statBuf;
    double delta;
    Int assignedId;
    FILE * filePtr;
    Int n;
    Int pid;
    struct LAD_CommandObj cmd;
    union LAD_ResponseObj rsp;

    /* sanity check arg */
    if (handle == NULL) {
        return(LAD_INVALIDARG);
    }

    /* check and initialize on first connect request */
    if (initialized == FALSE) {

        /* TODO:M does this need to be atomized? */
        status = initWrappers();
        if (status != LAD_SUCCESS) {
            return(status);
        }
        initialized = TRUE;
    }

    /* get caller's process ID */
    pid = getpid();

    /* form name for dedicated response FIFO */
    sprintf(responseFIFOName, "%s%d", LAD_RESPONSEFIFOPATH, pid);

    PRINTVERBOSE2("\nLAD_connect: PID = %d, fifoName = %s\n", pid,
        responseFIFOName)

    /* check if FIFO already exists; if yes, reject the request */
    if (stat(responseFIFOName, &statBuf) == 0) {
        PRINTVERBOSE0("\nLAD_connect: already connected; request denied!\n")
        return(LAD_ACCESSDENIED);
    }

    cmd.cmd = LAD_CONNECT;
    strcpy(cmd.args.connect.name, responseFIFOName);
    strcpy(cmd.args.connect.protocol, LAD_PROTOCOLVERSION);
    cmd.args.connect.pid = pid;

    if ((status = putCommand(&cmd)) != LAD_SUCCESS) {
        return(status);
    }

    /* now open the dedicated response FIFO for this client */
    startTime = time ((time_t *) 0);
    while ((filePtr = fopen(responseFIFOName, "r")) == NULL) {
        /* insert wait to yield, so LAD can process connect command sooner */
        usleep(100);
        currentTime = time ((time_t *) 0);
        delta = difftime(currentTime, startTime);
        if (delta > LAD_CONNECTTIMEOUT) {
            pthread_mutex_unlock(&modGate);

            return(LAD_IOFAILURE);
        }
    }

    /* now get LAD's response to the connection request */
    n = fread(&rsp, LAD_RESPONSELENGTH, 1, filePtr);

    /* need to unlock mutex obtained by putCommand() */
    pthread_mutex_unlock(&modGate);

    if (n) {
        PRINTVERBOSE0("\nLAD_connect: got response\n")

        /* extract LAD's response code and the client ID */
        status = rsp.connect.status;

        /* if a successful connect ... */
        if (status == LAD_SUCCESS) {
            assignedId = rsp.connect.assignedId;
            *handle = assignedId;

            /* setup client info */
            clientInfo[assignedId].PID = pid;
            clientInfo[assignedId].responseFIFOFilePtr = filePtr;
            strcpy(clientInfo[assignedId].responseFIFOName, responseFIFOName);
            clientInfo[assignedId].connectedToLAD = TRUE;

            PRINTVERBOSE1("    status == LAD_SUCCESS, assignedId=%d\n",
                          assignedId);
        }
        else {
            PRINTVERBOSE1("    status != LAD_SUCCESS (status=%d)\n", status);
        }
    }
    else {
        PRINTVERBOSE0(
          "\nLAD_connect: 0 bytes read when getting LAD response!\n")
        status = LAD_IOFAILURE;
    }

    /* if connect failed, close client side of FIFO (LAD closes its side) */
    if (status != LAD_SUCCESS) {
        PRINTVERBOSE0("\nLAD_connect failed: closing client-side of FIFO...\n")
        fclose(filePtr);
    }

    return(status);
}


/*
 *  ======== LAD_disconnect ========
 */
LAD_Status  LAD_disconnect(LAD_ClientHandle handle)
{
    LAD_Status status = LAD_SUCCESS;
    Bool waiting = TRUE;
    struct stat statBuf;
    time_t currentTime;
    time_t startTime;
    double delta;
    struct LAD_CommandObj cmd;

    /* sanity check args */
    if (handle >= LAD_MAXNUMCLIENTS) {
        return (LAD_INVALIDARG);
    }

    /* check for initialization and connection */
    if ((initialized == FALSE) ||
        (clientInfo[handle].connectedToLAD == FALSE)) {
        return (LAD_NOTCONNECTED);
    }

    cmd.cmd = LAD_DISCONNECT;
    cmd.clientId = handle;

    if ((status = putCommand(&cmd)) != LAD_SUCCESS) {
        return(status);
    }

    /* on success, close the dedicated response FIFO */
    fclose(clientInfo[handle].responseFIFOFilePtr);

    /* need to unlock mutex obtained by putCommand() */
    pthread_mutex_unlock(&modGate);

    /* now wait for LAD to close the connection ... */
    startTime = time ((time_t *) 0);
    while (waiting == TRUE) {

        /* do a minimal wait, to yield, so LAD can disconnect */
        usleep(1);
        currentTime = time ((time_t *) 0);

        /* check to see if LAD has shutdown FIFO yet... */
        if (stat(clientInfo[handle].responseFIFOName, &statBuf) != 0) {
            waiting = FALSE;            /* yes, so done */
        }
        /* if not, check for timeout */
        else {
            delta = difftime(currentTime, startTime);
            if (delta > LAD_DISCONNECTTIMEOUT) {
                PRINTVERBOSE0("\nLAD_disconnect: timeout waiting for LAD!\n")
                return(LAD_IOFAILURE);
            }
        }
    }

    /* reset connection status flag */
    clientInfo[handle].connectedToLAD = FALSE;

    return(status);
}

/*
 *  ======== getResponse ========
 */
static LAD_Status getResponse(LAD_ClientHandle handle,
                              union LAD_ResponseObj *rsp)
{
    LAD_Status status = LAD_SUCCESS;
    Int n;

    PRINTVERBOSE1("getResponse: client = %d\n", handle)

    n = fread(rsp, LAD_RESPONSELENGTH, 1,
             clientInfo[handle].responseFIFOFilePtr);

    pthread_mutex_unlock(&modGate);

    if (n == 0) {
        PRINTVERBOSE0("getResponse: n = 0!\n")
        status = LAD_IOFAILURE;
    }
    else {
        PRINTVERBOSE0("getResponse: got response\n")
    }

    return(status);
}


/*
 *  ======== initWrappers ========
 */
static LAD_Status initWrappers(Void)
{
    Int i;

    /* initialize the client info structures */
    for (i = 0; i < LAD_MAXNUMCLIENTS; i++) {
        clientInfo[i].connectedToLAD = FALSE;
        clientInfo[i].responseFIFOFilePtr = NULL;
    }

    /* now open LAD's command FIFO */
    if (openCommandFIFO() == FALSE) {
        return(LAD_IOFAILURE);
    }
    else {
        return(LAD_SUCCESS);
    }
}


/*
 *  ======== openCommandFIFO ========
 */
static Bool openCommandFIFO(Void)
{
    /* open a file for writing to FIFO */
    commandFIFOFilePtr = fopen(commandFIFOFileName, "w");

    if (commandFIFOFilePtr == NULL) {
        PRINTVERBOSE2("\nERROR: failed to open %s, errno = %x\n",
            commandFIFOFileName, errno)
        return(FALSE);
    }

    return(TRUE);
}


/*
 *  ======== putCommand ========
 */
static LAD_Status putCommand(struct LAD_CommandObj *cmd)
{
    LAD_Status status = LAD_SUCCESS;
    Int stat;
    Int n;

    PRINTVERBOSE0("\nputCommand:\n")

    pthread_mutex_lock(&modGate);

    n = fwrite(cmd, LAD_COMMANDLENGTH, 1, commandFIFOFilePtr);

    if (n == 0) {
        PRINTVERBOSE0("\nputCommand: fwrite returned 0!\n")
        status = LAD_IOFAILURE;
    }
    else {
        stat = fflush(commandFIFOFilePtr);

        if (stat == (Int) EOF) {
            PRINTVERBOSE0("\nputCommand: stat for fflush = EOF!\n")
            status = LAD_IOFAILURE;
        }
    }

    if (status != LAD_SUCCESS) {
        pthread_mutex_unlock(&modGate);
    }

    PRINTVERBOSE1("putCommand: status = %d\n", status)

    return(status);
}

