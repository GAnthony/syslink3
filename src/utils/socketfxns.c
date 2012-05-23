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
 *  @file       socketfxns.c
 *  @brief      misc socket functions.
 *
 */

/* Standard headers */
#include <Std.h>

/* Socket Headers */
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>

/* SysLink Socket Protocol	Family */
#include <net/rpmsg.h>

Int connect_socket(int sock, UInt16 procId, int dst)
{
	int                   err;
	struct sockaddr_rpmsg src_addr, dst_addr;
	socklen_t             len;

	/* connect to remote service */
	memset(&dst_addr, 0, sizeof(dst_addr));
	dst_addr.family     = AF_RPMSG;
	dst_addr.vproc_id   = procId - 1;
	dst_addr.addr       = dst;

	len = sizeof(struct sockaddr_rpmsg);
	err = connect(sock, (struct sockaddr *)&dst_addr, len);
	if (err < 0) {
		printf("connect failed: %s (%d)\n", strerror(errno), errno);
		return (-1);
	}

	/* let's see what local address we got */
	err = getsockname(sock, (struct sockaddr *)&src_addr, &len);
	if (err < 0) {
		printf("getpeername failed: %s (%d)\n", strerror(errno), errno);
		return (-1);
	}

#ifdef VERBOSE
	printf("Connected over sock: %d\n\tdst vproc_id: %d, dst addr: %d\n",
		     sock, dst_addr.vproc_id, dst_addr.addr);

	printf("\tsrc vproc_id: %d, src addr: %d\n",
			src_addr.vproc_id, src_addr.addr);
#endif

       return(0);
}

int socket_bind_addr(int fd, UInt16 rproc_id, UInt32 local_addr)
{
    int         err;
    socklen_t 	len;
    struct sockaddr_rpmsg src_addr;

    /* Now bind to the source address.   */
    memset(&src_addr, 0, sizeof(src_addr));
    src_addr.family = AF_RPMSG;
    /* We bind the remote proc ID, but local address! */
    src_addr.vproc_id = (rproc_id - 1);
    src_addr.addr  = local_addr;

    len = sizeof(struct sockaddr_rpmsg);
    err = bind(fd, (struct sockaddr *)&src_addr, len);
    if (err >= 0) {
#ifdef VERBOSE
        printf("socket_bind_addr: bound sock: %d\n\tto dst vproc_id: %d, "
		"src addr: %d\n", fd, src_addr.vproc_id, src_addr.addr);
#endif
        /* let's see what local address we got */
        err = getsockname(fd, (struct sockaddr *)&src_addr, &len);
        if (err < 0) {
	    printf("getsockname failed: %s (%d)\n", strerror(errno), errno);
        }
#ifdef VERBOSE
        else {
            printf("\tsrc vproc_id: %d, src addr: %d\n",
		     src_addr.vproc_id, src_addr.addr);
        }
#endif
    }

    return (err);
}
 
