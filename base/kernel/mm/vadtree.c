/*++

Module Name:

    vadtree.c

Abstract:

    VAD (Virtual Address Descriptor) binary search tree operations.
    Each node describes one contiguous region of reserved or committed
    virtual address space. The tree is keyed by StartVpn.

Author:

    Noah Juopperi <nipfswd@gmail.com>

--*/

#include "mi.h"

#define MI_VAD_POOL_SIZE 256

static MM_VAD MiVadPool[MI_VAD_POOL_SIZE];
static UCHAR  MiVadInUse[MI_VAD_POOL_SIZE];

PMM_VAD
MiAllocateVad (
    VOID
    )
{
    ULONG  Index;
    UCHAR *p;
    ULONG  i;

    for (Index = 0; Index < MI_VAD_POOL_SIZE; Index++) {
        if (!MiVadInUse[Index]) {
            MiVadInUse[Index] = 1;
            p = (UCHAR *)&MiVadPool[Index];
            for (i = 0; i < sizeof(MM_VAD); i++) {
                p[i] = 0;
            }
            return &MiVadPool[Index];
        }
    }

    return NULL;
}

VOID
MiFreeVad (
    PMM_VAD Vad
    )
{
    ULONG Index;

    if (Vad == NULL) {
        return;
    }

    Index = (ULONG)(Vad - MiVadPool);

    if (Index < MI_VAD_POOL_SIZE) {
        MiVadInUse[Index] = 0;
    }
}

VOID
MiInsertVad (
    PMM_VAD **Root,
    PMM_VAD   Vad
    )
{
    PMM_VAD *Current;
    PMM_VAD  Parent;

    if (Root == NULL || Vad == NULL) {
        return;
    }

    Current = (PMM_VAD *)*Root;
    Parent  = NULL;

    while (*Current != NULL) {
        Parent = *Current;
        if (Vad->StartVpn < (*Current)->StartVpn) {
            Current = &(*Current)->Left;
        } else {
            Current = &(*Current)->Right;
        }
    }

    Vad->Parent = Parent;
    Vad->Left   = NULL;
    Vad->Right  = NULL;
    *Current    = Vad;
}

static VOID
MiVadTransplant (
    PMM_VAD **Root,
    PMM_VAD   U,
    PMM_VAD   V
    )
{
    if (U->Parent == NULL) {
        *(PMM_VAD *)*Root = V;
    } else if (U == U->Parent->Left) {
        U->Parent->Left = V;
    } else {
        U->Parent->Right = V;
    }

    if (V != NULL) {
        V->Parent = U->Parent;
    }
}

static PMM_VAD
MiVadMinimum (
    PMM_VAD Node
    )
{
    if (Node == NULL) {
        return NULL;
    }

    while (Node->Left != NULL) {
        Node = Node->Left;
    }

    return Node;
}

VOID
MiRemoveVad (
    PMM_VAD **Root,
    PMM_VAD   Vad
    )
{
    PMM_VAD Successor;

    if (Root == NULL || Vad == NULL) {
        return;
    }

    if (Vad->Left == NULL) {
        MiVadTransplant(Root, Vad, Vad->Right);

    } else if (Vad->Right == NULL) {
        MiVadTransplant(Root, Vad, Vad->Left);

    } else {
        Successor = MiVadMinimum(Vad->Right);

        if (Successor->Parent != Vad) {
            MiVadTransplant(Root, Successor, Successor->Right);
            Successor->Right         = Vad->Right;
            Successor->Right->Parent = Successor;
        }

        MiVadTransplant(Root, Vad, Successor);
        Successor->Left         = Vad->Left;
        Successor->Left->Parent = Successor;
    }
}

PMM_VAD
MiFindVad (
    PMM_VAD   Root,
    ULONG_PTR Vpn
    )
{
    PMM_VAD Current;

    Current = Root;

    while (Current != NULL) {
        if (Vpn < Current->StartVpn) {
            Current = Current->Left;
        } else if (Vpn > Current->EndVpn) {
            Current = Current->Right;
        } else {
            return Current;
        }
    }

    return NULL;
}

PMM_VAD
MiFindVadRange (
    PMM_VAD   Root,
    ULONG_PTR StartVpn,
    ULONG_PTR EndVpn
    )
{
    PMM_VAD Current;

    Current = Root;

    while (Current != NULL) {
        if (EndVpn < Current->StartVpn) {
            Current = Current->Left;
        } else if (StartVpn > Current->EndVpn) {
            Current = Current->Right;
        } else {
            return Current;
        }
    }

    return NULL;
}