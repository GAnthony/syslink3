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
 *  @file       SocketFxns.c
 *  @brief      Shared socket functions.
 *
 */

/* Standard headers */
#include <Std.h>

/* Socket Headers */
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* SysLink Socket Protocol Family */
#include <net/rpmsg.h>

/* For PRINTVERBOSE* */
#include <_lad.h>

static Bool verbose = FALSE;

int ConnectSocket(int sock, UInt16 procId, int dst)
{
    int                   err;
    struct sockaddr_rpmsg srcAddr, dstAddr;
    socklen_t             len;

    /* connect to remote service */
    memset(&dstAddr, 0, sizeof(dstAddr));
    dstAddr.family     = AF_RPMSG;
     /* rpmsg "vproc_id" is one less than the MultiProc ID: */
    dstAddr.vproc_id   = procId - 1;
    dstAddr.addr       = dst;

    len = sizeof(struct sockaddr_rpmsg);
    err = connect(sock, (struct sockaddr *)&dstAddr, len);
    if (err < 0) {
         printf("connect failed: %s (%d)\n", strerror(errno), errno);
         return (-1);
    }

    /* let's see what local address we got */
    err = getsockname(sock, (struct sockaddr *)&srcAddr, &len);
    if (err < 0) {
         printf("getpeername failed: %s (%d)\n", strerror(errno), errno);
         return (-1);
    }

    PRINTVERBOSE3("Connected over sock: %d\n\tdst vproc_id: %d, dst addr: %d\n",
                  sock, dstAddr.vproc_id, dstAddr.addr)
    PRINTVERBOSE2("\tsrc vproc_id: %d, src addr: %d\n",
                  srcAddr.vproc_id, srcAddr.addr)

    return(0);
}

int SocketBindAddr(int fd, UInt16 rprocId, UInt32 localAddr)
{
    int         err;
    socklen_t    len;
    struct sockaddr_rpmsg srcAddr;

    /* Now bind to the source address.   */
    memset(&srcAddr, 0, sizeof(srcAddr));
    srcAddr.family = AF_RPMSG;
    /* We bind the remote proc ID, but local address! */
    srcAddr.vproc_id = (rprocId - 1);
    srcAddr.addr  = localAddr;

    len = sizeof(struct sockaddr_rpmsg);
    err = bind(fd, (struct sockaddr *)&srcAddr, len);
    if (err >= 0) {
        PRINTVERBOSE3("socket_bind_addr: bound sock: %d\n\tto dst "
                      "vproc_id: %d, src addr: %d\n",
                      fd, srcAddr.vproc_id, srcAddr.addr)

        /* let's see what local address we got */
        err = getsockname(fd, (struct sockaddr *)&srcAddr, &len);
        if (err < 0) {
            printf("getsockname failed: %s (%d)\n", strerror(errno), errno);
        }
        else {
            PRINTVERBOSE2("\tsrc vproc_id: %d, src addr: %d\n",
                          srcAddr.vproc_id, srcAddr.addr)
        }
    }

    return (err);
}
