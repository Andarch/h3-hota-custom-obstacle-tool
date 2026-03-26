#include "pch.h"
#include "objlimits.h"
#include "hook.h"

// New per-map limit for all affected object types.
static DWORD s_limit = 255;

// The DWORD at [+0] in a type_info struct is a packed type+subId identifier.
// For objects with subId=0 it equals the H3 type ID directly.
// Colosseum of the Magi (type=0x90, subId=0x02) encodes as 0x00900002.

// ---------------------------------------------------------------------------
// Create check  (sub_421222 @ 0x4212A4)
//   Original bytes: cmp ecx, [eax+8]  (3 bytes)
//                   jb  loc_4212D1    (2 bytes)
//   eax = type_info ptr, ecx = current count
// ---------------------------------------------------------------------------
static BYTE  s_createOrig[5];
static DWORD s_createAllow = 0x4212D1;
static DWORD s_createDeny  = 0x4212A9;

__declspec(naked) static void CreateCheckHook()
{
    __asm {
        cmp     dword ptr [eax], 004h       ; Arena
        je      cr_lim
        cmp     dword ptr [eax], 017h       ; Marletto Tower
        je      cr_lim
        cmp     dword ptr [eax], 020h       ; Garden of Revelation
        je      cr_lim
        cmp     dword ptr [eax], 021h       ; Garrison
        je      cr_lim
        cmp     dword ptr [eax], 029h       ; Library of Enlightenment
        je      cr_lim
        cmp     dword ptr [eax], 02Fh       ; School of Magic
        je      cr_lim
        cmp     dword ptr [eax], 033h       ; Mercenary Camp
        je      cr_lim
        cmp     dword ptr [eax], 03Dh       ; Star Axis
        je      cr_lim
        cmp     dword ptr [eax], 064h       ; Learning Stone
        je      cr_lim
        cmp     dword ptr [eax], 06Bh       ; School of War
        je      cr_lim
        cmp     dword ptr [eax], 00900002h  ; Colosseum of the Magi
        je      cr_lim
        cmp     ecx, dword ptr [eax+8]
        jb      cr_allow
    cr_deny:
        jmp     dword ptr [s_createDeny]
    cr_allow:
        jmp     dword ptr [s_createAllow]
    cr_lim:
        cmp     ecx, dword ptr [s_limit]
        jb      cr_allow
        mov     dword ptr [eax+8], 0FFh
        jmp     cr_deny
    }
}

// ---------------------------------------------------------------------------
// Place check  (sub_426A49 @ 0x426AFA)
//   Original bytes: cmp ecx, [eax+8]  (3 bytes)
//                   jb  loc_426B27    (2 bytes)
//   eax = type_info ptr, ecx = current count
// ---------------------------------------------------------------------------
static BYTE  s_placeOrig[5];
static DWORD s_placeAllow = 0x426B27;
static DWORD s_placeDeny  = 0x426AFF;

__declspec(naked) static void PlaceCheckHook()
{
    __asm {
        cmp     dword ptr [eax], 004h       ; Arena
        je      pl_lim
        cmp     dword ptr [eax], 017h       ; Marletto Tower
        je      pl_lim
        cmp     dword ptr [eax], 020h       ; Garden of Revelation
        je      pl_lim
        cmp     dword ptr [eax], 021h       ; Garrison
        je      pl_lim
        cmp     dword ptr [eax], 029h       ; Library of Enlightenment
        je      pl_lim
        cmp     dword ptr [eax], 02Fh       ; School of Magic
        je      pl_lim
        cmp     dword ptr [eax], 033h       ; Mercenary Camp
        je      pl_lim
        cmp     dword ptr [eax], 03Dh       ; Star Axis
        je      pl_lim
        cmp     dword ptr [eax], 064h       ; Learning Stone
        je      pl_lim
        cmp     dword ptr [eax], 06Bh       ; School of War
        je      pl_lim
        cmp     dword ptr [eax], 00900002h  ; Colosseum of the Magi
        je      pl_lim
        cmp     ecx, dword ptr [eax+8]
        jb      pl_allow
    pl_deny:
        jmp     dword ptr [s_placeDeny]
    pl_allow:
        jmp     dword ptr [s_placeAllow]
    pl_lim:
        cmp     ecx, dword ptr [s_limit]
        jb      pl_allow
        mov     dword ptr [eax+8], 0FFh
        jmp     pl_deny
    }
}

// ---------------------------------------------------------------------------
// "Can place?" predicate  (sub_423278 @ 0x4232D4)
//   Original bytes: cmp eax, [esi+8]  (3 bytes)
//                   jnb loc_423485    (6 bytes, near jump)
//   esi = type_info ptr, eax = current count
//   Note: jnb direction is reversed — deny if >= max.
// ---------------------------------------------------------------------------
static BYTE  s_predOrig[5];
static DWORD s_predAllow = 0x4232DD;
static DWORD s_predDeny  = 0x423485;

__declspec(naked) static void PredCheckHook()
{
    __asm {
        cmp     dword ptr [esi], 004h       ; Arena
        je      pr_lim
        cmp     dword ptr [esi], 017h       ; Marletto Tower
        je      pr_lim
        cmp     dword ptr [esi], 020h       ; Garden of Revelation
        je      pr_lim
        cmp     dword ptr [esi], 021h       ; Garrison
        je      pr_lim
        cmp     dword ptr [esi], 029h       ; Library of Enlightenment
        je      pr_lim
        cmp     dword ptr [esi], 02Fh       ; School of Magic
        je      pr_lim
        cmp     dword ptr [esi], 033h       ; Mercenary Camp
        je      pr_lim
        cmp     dword ptr [esi], 03Dh       ; Star Axis
        je      pr_lim
        cmp     dword ptr [esi], 064h       ; Learning Stone
        je      pr_lim
        cmp     dword ptr [esi], 06Bh       ; School of War
        je      pr_lim
        cmp     dword ptr [esi], 00900002h  ; Colosseum of the Magi
        je      pr_lim
        cmp     eax, dword ptr [esi+8]
        jnb     pr_deny
    pr_allow:
        jmp     dword ptr [s_predAllow]
    pr_deny:
        jmp     dword ptr [s_predDeny]
    pr_lim:
        cmp     eax, dword ptr [s_limit]
        jnb     pr_deny
        jmp     pr_allow
    }
}

// ---------------------------------------------------------------------------
void InstallObjectLimitHooks()
{
    HookInstall(reinterpret_cast<void*>(0x4212A4), CreateCheckHook, s_createOrig);
    HookInstall(reinterpret_cast<void*>(0x426AFA), PlaceCheckHook,  s_placeOrig);
    HookInstall(reinterpret_cast<void*>(0x4232D4), PredCheckHook,   s_predOrig);
}
