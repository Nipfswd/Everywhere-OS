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

VOID         PagingInitialize(VOID);
unsigned int PagingGetCr3(VOID);