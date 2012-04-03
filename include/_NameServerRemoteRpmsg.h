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

#define NAMESERVER_REQUEST    0
#define NAMESERVER_RESPONSE   1

#define NAME_SERVER_RPMSG_ADDR	0

#define MAXNAMEINCHAR	80
#define NAMEARRAYSZIE   (((MAXNAMEINCHAR - 1) / sizeof(Bits32)) + 1)

#define NAMESERVER_GET_TIMEOUT  120 /* Seconds */

#define NAMESERVER_MSG_TOKEN   0x5678abcd

/* message sent to remote procId */
typedef struct NameServerMsg {
    Bits32  reserved;           /* reserved field: must be first!   */
    Bits32  value;              /* holds value                      */
    Bits32  request;            /* whether its a request/response   */
    Bits32  requestStatus;      /* status of request                */
                                /* name of NameServer instance      */
    Bits32  instanceName[NAMEARRAYSZIE];
                                /* name of NameServer entry         */
    Bits32  name[NAMEARRAYSZIE];
} NameServerMsg;

