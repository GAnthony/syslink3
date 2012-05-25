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
 *  ======== MultiProc.c ========
 *
 *  Implementation of functions to access processor IDs configured on BIOS side.
 */

#include <Std.h>

#include <assert.h>
#include <string.h>

#include <ti/ipc/MultiProc.h>
#include <_MultiProc.h>

/*
 *  ======== MultiProc_getId ========
 */
UInt16 MultiProc_getId(String name)
{
    Int    i;
    UInt16 id;

    assert(name != NULL);

    id = MultiProc_INVALIDID;
    for (i = 0; i < _MultiProc_cfg.numProcessors; i++) {
        if ((_MultiProc_cfg.nameList[i] != NULL) &&
                (strcmp(name, _MultiProc_cfg.nameList[i]) == 0)) {
            id = i;
        }
    }
    return (id);
}

/*
 *  ======== MultiProc_getName ========
 */
String MultiProc_getName(UInt16 id)
{
    assert(id < _MultiProc_cfg.numProcessors);

    return (_MultiProc_cfg.nameList[id]);
}

/*
 *  ======== MultiProc_getNumProcessors ========
 */
UInt16 MultiProc_getNumProcessors()
{
    return (_MultiProc_cfg.numProcessors);
}


/*
 *  ======== MultiProc_self ========
 */
UInt16 MultiProc_self()
{
    return (_MultiProc_cfg.id);
}

/*
 *  ======== MultiProc_setLocalId ========
 */
Int MultiProc_setLocalId(UInt16 id)
{
    /* id must be less than the number of processors */
    assert(id < _MultiProc_cfg.numProcessors);

    /*
     *  Check the following
     *  1. Make sure the statically configured constant was invalid.
     *     To call setLocalId, the id must have been set to invalid.
     *  2. Make sure the call is made before module startup
     */
    if ((_MultiProc_cfg.id == MultiProc_INVALIDID) /* &&
        (Startup_rtsDone() == FALSE) */  )  {
        /* It is ok to set the id */
        _MultiProc_cfg.id = id;
        return (MultiProc_S_SUCCESS);
    }

    return (MultiProc_E_FAIL);
}

