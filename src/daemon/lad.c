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
 *  ======== lad.c ========
 */

#include <Std.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>

#include <ti/ipc/NameServer.h>
#include <_NameServer.h>

#include <ladclient.h>
#include <_lad.h>

#define DAEMON        1           /* 1 = run as a daemon; 0 = run as app */

Bool logFile = FALSE;
FILE *logPtr = NULL;

static String commandFIFOFile = LAD_COMMANDFIFO;
static FILE *commandFIFOFilePtr = NULL;
static String serverDir;

/* LAD client info arrays */
static Bool clientConnected[LAD_MAXNUMCLIENTS];
static UInt clientPID[LAD_MAXNUMCLIENTS];
static Char clientFIFOName[LAD_MAXNUMCLIENTS][LAD_MAXLENGTHFIFONAME];
static FILE * responseFIFOFilePtr[LAD_MAXNUMCLIENTS];

/* local internal routines */
static LAD_ClientHandle assignClientId(Void);
static Void cleanupDepartedClients(Void);
static Int connectToLAD(String clientName, Int pid, String clientProto, Int *clientIdPtr);
static Void disconnectFromLAD(Int clientId);
static Void doDisconnect(Int clientId);

struct LAD_CommandObj cmd;
union LAD_ResponseObj rsp;

/*
 *  ======== main ========
 */
int main(int argc, char * argv[])
{
    UInt16 *procIdPtr;
    Int statusIO;
    Int clientId;
    Int command;
    Int flags;
    Int i;
    Int n;
#if DAEMON
    pid_t pid;
    pid_t sid;
#endif

    /* if more than two args: turn "ON" launch status printfs */
    if (argc > 2) {
        printf("\nLAD starting up...");
    }

    /* check for env variable indicating server exe repository */
    serverDir = getenv("LAD_SERVERPATH");
    if (argc > 2) {
        if (serverDir != NULL) {
            printf("\nLAD_SERVERPATH = %s\n", serverDir);
        }
        else {
            printf("\nLAD_SERVERPATH = <NULL>\n");
        }
    }

    /* change to LAD's working directory */
    if ((chdir(LAD_WORKINGDIR)) < 0) {

        /* if can't change directory assume it needs to be created, do it */
        if ((mkdir(LAD_WORKINGDIR, 0666)) < 0) {
            printf("\nERROR: Failed to create LAD's working directory!\n");
            exit(EXIT_FAILURE);
        }
        /* now change to the new directory */
        if ((chdir(LAD_WORKINGDIR)) < 0) {
            printf("\nERROR: Failed to change to LAD's working directory!\n");
            exit(EXIT_FAILURE);
        }
    }

    /* process command line args */
    if (argc > 1) {
        logPtr = fopen(argv[1], "w");
        if (logPtr == NULL) {
            printf("\nERROR: unable to open log file %s\n", argv[1]);
            exit(EXIT_FAILURE);
        }
        else {
            logFile = TRUE;
            if (argc > 2) {
                printf("\nOpened log file: %s", argv[1]);
            }
            /* close log file upon LAD termination */
            flags = fcntl(fileno(logPtr), F_GETFD);
            if (flags != -1) {
                fcntl(fileno(logPtr), F_SETFD, flags | FD_CLOEXEC);
            }
        }
    }

#if DAEMON
    /* fork off a child process */
    pid = fork();

    /* if fork of child failed then exit immediately; no child created */
    if (pid < 0) {
        printf("\nERROR: Failed to fork child process!");
        exit(EXIT_FAILURE);
    }

    /* if pid > 0 this is the parent; time to die ... */
    if (pid > 0) {
        if (argc > 2) {
            printf("\nSpawned daemon: %s\n\n", argv[0]);
        }
        exit(EXIT_SUCCESS);
    }

    /* child continues from here (pid == 0) ... */

    /* change file mode mask */
    umask(0);

    /* create new session ID for the child */
    sid = setsid();

    /* exit with failure code if failed to get session ID... */
    if (sid < 0) {
        printf("\nERROR: Failed to acquire new session ID!");
        exit(EXIT_FAILURE);
    }

    /* disassociate from the standard file descriptors */
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

#endif

    LOG0("\nInitializing LAD... ")

    /* TODO:L make sure LAD is not already running? */

    /* initialize client info arrays */
    for (i = 0; i < LAD_MAXNUMCLIENTS; i++) {
        clientConnected[i] = FALSE;
        responseFIFOFilePtr[i] = NULL;
    }

    /* if command FIFO exists from previous LAD session delete it now */
    unlink(commandFIFOFile);

    /* create command FIFO */
    statusIO = mkfifo(commandFIFOFile, 0777);
    if (statusIO != 0) {
        LOG2("\nERROR: unable to create %s, errno = %x\n", commandFIFOFile,
            errno)
        return(0);
    }

    /* set FIFO permissions to read/write */
    chmod(commandFIFOFile, 0666);

opencommandFIFO:

    /* now open file for FIFO - will block until writer arrives... */
    LOG1("\n    opening FIFO: %s\n", commandFIFOFile)
    commandFIFOFilePtr = fopen(commandFIFOFile, "r");
    if (commandFIFOFilePtr == NULL) {
        LOG0("\nERROR: unable to open command FIFO\n")
        unlink(commandFIFOFile);
        return(0);
    }

    /* COMMAND PROCESSING LOOP */
    while (1) {
        LOG0("Retrieving command...\n")

        /* read the next command packet */
        n = fread(&cmd, LAD_COMMANDLENGTH, 1, commandFIFOFilePtr);

        /*
         * if last client closes FIFO then it must be closed and reopened ...
         */
        if (!n) {
            LOG1("    EOF detected on FIFO, closing FIFO: %s\n", commandFIFOFile)
            fclose(commandFIFOFilePtr);

            goto opencommandFIFO;
        }

        /* cleanup for any connected/started clients that have departed */
        cleanupDepartedClients();

        command = cmd.cmd;
        clientId = cmd.clientId;

        /* process individual commands */
        switch (command) {
          /*
           * Since cmd is a union of rcv and snd structs, don't write
           * any snd elements before all rcv elements have been referenced
           * (either saved in separate variables or passed to a function).
           *
           * cmd.cmd has already been saved in 'command'
           * cmd.clientId has already been saved in 'clientId'
           */
          case LAD_CONNECT:
            connectToLAD(cmd.args.connect.name, cmd.args.connect.pid,
                         cmd.args.connect.protocol, NULL);

            break;

          case LAD_DISCONNECT:
            disconnectFromLAD(clientId);

            break;

          case LAD_NAMESERVER_SETUP:
            LOG0("LAD_NAMESERVER_SETUP: calling NameServer_setup()...\n")

            rsp.status = NameServer_setup();

            LOG1("    status = %d\n", rsp.status)
            LOG0("DONE\n")

            break;

          case LAD_NAMESERVER_DESTROY:
            LOG0("LAD_NAMESERVER_DESTROY: calling NameServer_destroy()...\n")

            rsp.status = NameServer_destroy();

            LOG1("    status = %d\n", rsp.status)
            LOG0("DONE\n")

            break;

          case LAD_NAMESERVER_PARAMS_INIT:
            LOG0("LAD_NAMESERVER_PARAMS_INIT: calling NameServer_Params_init()...\n")

            NameServer_Params_init(&rsp.params);

            LOG0("DONE\n")

            break;

          case LAD_NAMESERVER_CREATE:
            LOG1("LAD_NAMESERVER_CREATE: calling NameServer_create('%s')...\n", cmd.args.create.name)

            rsp.handle = NameServer_create(cmd.args.create.name,
                                               &cmd.args.create.params);

            LOG1("    handle = %p\n", rsp.handle)
            LOG0("DONE\n")

            break;

          case LAD_NAMESERVER_DELETE:
            LOG1("LAD_NAMESERVER_DELETE: calling NameServer_delete(%p)...\n", cmd.args.delete.handle)

            rsp.delete.handle = cmd.args.delete.handle;
            rsp.delete.status = NameServer_delete(&rsp.delete.handle);

            LOG1("    status = %d\n", rsp.status)
            LOG0("DONE\n")

            break;

          case LAD_NAMESERVER_ADDUINT32:
            LOG1("LAD_NAMESERVER_ADDUINT32: calling NameServer_addUInt32(%p, ", cmd.args.addUInt32.handle)
            LOG2("'%s', 0x%x)...\n", cmd.args.addUInt32.name, cmd.args.addUInt32.val)

            rsp.entryPtr = NameServer_addUInt32(
                cmd.args.addUInt32.handle,
                cmd.args.addUInt32.name,
                cmd.args.addUInt32.val);

            LOG1("    entryPtr = %p\n", rsp.entryPtr)
            LOG0("DONE\n")

            break;

          case LAD_NAMESERVER_GETUINT32:
            LOG2("LAD_NAMESERVER_GETUINT32: calling NameServer_getUInt32(%p, '%s')...\n", cmd.args.getUInt32.handle, cmd.args.getUInt32.name)

            if (cmd.args.getUInt32.procId[0] == (UInt16)-1) {
                procIdPtr = NULL;
            }
            else {
                procIdPtr = cmd.args.getUInt32.procId;
            }
            rsp.status = NameServer_getUInt32(
                cmd.args.getUInt32.handle,
                cmd.args.getUInt32.name,
                &rsp.getUInt32.val,
                procIdPtr);

            LOG1("    value = 0x%x\n", rsp.getUInt32.val)
            LOG1("    status = %d\n", rsp.status)
            LOG0("DONE\n")

            break;

          case LAD_NAMESERVER_REMOVE:
            LOG2("LAD_NAMESERVER_REMOVE: calling NameServer_remove(%p, '%s')...\n", cmd.args.remove.handle, cmd.args.remove.name)

            rsp.status = NameServer_remove(cmd.args.remove.handle,
                                               cmd.args.remove.name);

            LOG1("    status = %d\n", rsp.status)
            LOG0("DONE\n")

            break;

          case LAD_NAMESERVER_REMOVEENTRY:
            LOG2("LAD_NAMESERVER_REMOVEENTRY: calling NameServer_removeEntry(%p, %p)...\n", cmd.args.remove.handle, cmd.args.removeEntry.entryPtr)

            rsp.status = NameServer_removeEntry(
                cmd.args.removeEntry.handle,
                cmd.args.removeEntry.entryPtr);

            LOG1("    status = %d\n", rsp.status)
            LOG0("DONE\n")

            break;

          case LAD_EXIT:
            goto exitNow;

            break;

          default:
            LOG1("\nUnrecognized command: 0x%x\n", command)

            break;
        }

        switch (command) {
          case LAD_CONNECT:
          case LAD_DISCONNECT:
            break;

          case LAD_NAMESERVER_SETUP:
          case LAD_NAMESERVER_DESTROY:
          case LAD_NAMESERVER_PARAMS_INIT:
          case LAD_NAMESERVER_CREATE:
          case LAD_NAMESERVER_DELETE:
          case LAD_NAMESERVER_ADDUINT32:
          case LAD_NAMESERVER_GETUINT32:
          case LAD_NAMESERVER_REMOVE:
          case LAD_NAMESERVER_REMOVEENTRY:
            LOG0("Sending response...\n");

            fwrite(&rsp, LAD_RESPONSELENGTH, 1, responseFIFOFilePtr[clientId]);
            fflush(responseFIFOFilePtr[clientId]);

            break;

          default:
            break;
        }
    }

exitNow:
    if (logFile) {
        LOG0("\n\nLAD IS SELF TERMINATING...\n\n")
        fclose(logPtr);
    }
    unlink(commandFIFOFile);

    return(0);

}


/*
 *  ======== assignClientId ========
 */
static LAD_ClientHandle assignClientId(Void)
{
    Int clientId = -1;
    Int i;

    /* scan connection status flags to acquire a clientId */
    for (i = 0; i < LAD_MAXNUMCLIENTS; i++) {
        /* if slot open then provisionally select this clientId */
        if (clientConnected[i] == FALSE) {
             clientId = i;
             break;
        }
    }

    return(clientId);
}


/*
 *  ======== cleanupDepartedClients ========
 */
static Void cleanupDepartedClients(Void)
{
    Int killStat;
    Int i;

    /* scan all connections to verify client processes still exist... */
    for (i = 0; i < LAD_MAXNUMCLIENTS; i++) {

        /* if connected... */
        if (clientConnected[i] == TRUE) {

            /* check if the client process (PID) still exists */
            /*
             * NOTE - calling kill with signal value of 0 will perform
             * error checking, but not actually send a signal.  Will use this
             * error check to see if PID still exists.  An alternative was
             * to call getpgid.  This worked, but is apparently limited to
             * USE_XOPEN_EXTENDED configurations.
             */
            killStat = kill(clientPID[i], 0);
            if ((killStat == -1) && (errno == ESRCH)) {

                LOG1("\nDETECTED CONNECTED CLIENT #%d HAS DEPARTED!", i)

                /* will always need to do the disconnect... */
                LOG0("\nDoing DISCONNECT on behalf of client...")
                doDisconnect(i);

                LOG0("DONE\n")
            }
        }
    }
}


/*
 *  ======== connectToLAD ========
 */
static Int connectToLAD(String clientName, Int pid, String clientProto, Int *clientIdPtr)
{
    Int clientId = -1;
    Bool connectDenied = FALSE;
    Int status = LAD_SUCCESS;
    FILE * filePtr;
    Int statusIO;

    /*
     * TODO:L here, and everywhere parse FIFO strings: should
     * add full error checking for incomplete or corrupted
     * strings? Since LAD to ladclient comms are "closed", and
     * tested, the likelihood of problems seems very low.
     * But should still consider doing full error checking
     * and, hopefully, recovery.
     */
    LOG0("\nLAD_CONNECT: \n")
    LOG1("    client FIFO name = %s\n", clientName)
    LOG1("    client PID = %d\n", pid)

    /* first check for proper communication protocol */
    if (strcmp(clientProto, LAD_PROTOCOLVERSION) != 0) {

        /* if no match then reject the request */
        LOG0("    ERROR: mismatch in communication protocol!\n")
        LOG1("        LAD protocol = %s\n", LAD_PROTOCOLVERSION)
        LOG1("        client protocol = %s\n", clientProto)
        status = LAD_INVALIDVERSION;

        /* set flag so know to close FIFO after response */
        connectDenied = TRUE;

        /* now jump forward a bit to send response */
        goto openResponseFIFO;
    }

    /* determine this client's ID */
    clientId = assignClientId();

    /* if failed to acquire an ID then exit early */
    if (clientId == -1) {
        LOG0("    no free handle; too many connections!\n")
        status = LAD_ACCESSDENIED;

        /* set flag so know to close FIFO after response */
        connectDenied = TRUE;
    }
    else {
        LOG1("    assigned client handle = %d\n", clientId)

        /* save the client's FIFO name for when disconnect */
        strcpy(clientFIFOName[clientId], clientName);
    }

openResponseFIFO:

    /* create the dedicated response FIFO to the client */
    statusIO = mkfifo(clientName, 0777);
    if (statusIO != 0) {

        LOG2("\nERROR: unable to mkfifo %s, errno = %x\n", clientName, errno)

        status = LAD_IOFAILURE;

        /* send no response; connection request will timeout */
        goto doneconnect;
    }

    LOG1("    FIFO %s created\n", clientName)

    /* set FIFO permissions to read/write */
    chmod(clientName, 0666);

    filePtr = fopen(clientName, "w");
    if (filePtr == NULL) {
        LOG1("\nERROR: unable to open response FIFO %s\n", clientName)

        /* if failed open, still need to delete the FIFO */
        unlink(clientName);

        status = LAD_IOFAILURE;

        /* send no response; connection request will timeout */
        goto doneconnect;
    }

    LOG1("    FIFO %s opened for writing\n", clientName)

    /*
     * set "this client is connected" flag; this client ID is now "owned", and
     * is no longer provisional
     */
    if (connectDenied == FALSE) {
        responseFIFOFilePtr[clientId] = filePtr;
        clientPID[clientId] = pid;
        clientConnected[clientId] = TRUE;
    }

    rsp.connect.assignedId = clientId;
    rsp.status = status;

    /* put response to FIFO */
    fwrite(&rsp, LAD_RESPONSELENGTH, 1, filePtr);
    fflush(filePtr);

    LOG0("    sent response\n")

    /* if connection was denied, must now close FIFO */
    if (connectDenied == TRUE) {
        LOG1("    connect denied; closing FIFO %s\n", clientName)
        fclose(filePtr);
        unlink(clientName);
    }

    LOG0("DONE\n")

doneconnect:
    if (clientIdPtr != NULL) {
        *clientIdPtr = clientId;
    }

    return(status);
}


/*
 *  ======== disconnectFromLAD ========
 */
static Void disconnectFromLAD(Int clientId)
{
    LOG0("\nLAD_DISCONNECT: ")

    LOG1("\n    client handle = %x", clientId)
    doDisconnect(clientId);

    LOG0("DONE\n")

    return;
}


/*
 *  ======== doDisconnect ========
 */
static Void doDisconnect(Int clientId)
{
    /* set "this client is not connected" flag */
    clientConnected[clientId] = FALSE;

    /* close and remove the response FIFO */
    LOG2("\n    closing FIFO %s (filePtr=%p)\n",
        clientFIFOName[clientId], responseFIFOFilePtr[clientId])
    fclose(responseFIFOFilePtr[clientId]);

    LOG1("    done, unlinking %s\n", clientFIFOName[clientId]);
    unlink(clientFIFOName[clientId]);
}


