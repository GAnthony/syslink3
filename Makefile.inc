#
# Copyright (c) 2012, Texas Instruments Incorporated
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# *  Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
#
# *  Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# *  Neither the name of Texas Instruments Incorporated nor the names of
#    its contributors may be used to endorse or promote products derived
#    from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
# THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
# OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
# EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

# Tools REPO:
REPO = /db/atree/gp/repo

# Platform to build (see src/family/ for subdirectories).
PLATFORM_DIR := omap4430

# BIOS IPC Location for Shared API Headers:
IPCPRODVER = ipc_1_23_01_26
IPC_INSTALL_DIR = $(REPO)/$(IPCPRODVER)

LIB_DEPS := ../api/libipc.a ../utils/libutils.a ../family/$(PLATFORM_DIR)/libcfg.a
CROSS  := arm-none-linux-gnueabi-
GCC    := $(CROSS)gcc
AR     := $(CROSS)ar
RANLIB := $(CROSS)ranlib
CFLAGS := -Wall -I../../include -I$(IPC_INSTALL_DIR)/packages -D_GNU_SOURCE
LFLAGS := -lpthread -L../api -lipc -L../utils -lutils -L../family/$(PLATFORM_DIR) -lcfg


%.o: %.c
	$(GCC) $(CFLAGS) -c $< -o $@

