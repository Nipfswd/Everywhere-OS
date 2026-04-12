/*++

Module Name:

    paging.h

Abstract:

    Public interface for paging initialisation.

Author:

    Noah Juopperi <nipfswd@gmail.com>

--*/

#pragma once

#include "types.h"
#include "multiboot.h"

VOID         PagingInitialize(MULTIBOOT_INFO *MbInfo);
unsigned int PagingGetCr3(VOID);