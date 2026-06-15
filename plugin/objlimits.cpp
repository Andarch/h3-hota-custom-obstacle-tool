#include "pch.h"
#include "objlimits.h"
#include "hook.h"

// New per-map limit for garrison objects.
static DWORD s_garrisonLimit = 255;

// The DWORD at [+0] in a type_info struct is a packed type+subId identifier.
// Garrison has type=0x21 and subId=0, so its packed identifier is 0x00000021.

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
        cmp     dword ptr [eax], 021h       ; Garrison
        jne     cr_other
        cmp     ecx, dword ptr [s_garrisonLimit]
        jb      cr_allow
        mov     dword ptr [eax+8], 0FFh
        jmp     cr_deny
    cr_other:
        cmp     ecx, dword ptr [eax+8]
        jb      cr_allow
    cr_deny:
        jmp     dword ptr [s_createDeny]
    cr_allow:
        jmp     dword ptr [s_createAllow]
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
        cmp     dword ptr [eax], 021h       ; Garrison
        jne     pl_other
        cmp     ecx, dword ptr [s_garrisonLimit]
        jb      pl_allow
        mov     dword ptr [eax+8], 0FFh
        jmp     pl_deny
    pl_other:
        cmp     ecx, dword ptr [eax+8]
        jb      pl_allow
    pl_deny:
        jmp     dword ptr [s_placeDeny]
    pl_allow:
        jmp     dword ptr [s_placeAllow]
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
        cmp     dword ptr [esi], 021h       ; Garrison
        jne     pr_other
        cmp     eax, dword ptr [s_garrisonLimit]
        jnb     pr_deny
        jmp     pr_allow
    pr_other:
        cmp     eax, dword ptr [esi+8]
        jnb     pr_deny
    pr_allow:
        jmp     dword ptr [s_predAllow]
    pr_deny:
        jmp     dword ptr [s_predDeny]
    }
}

// ---------------------------------------------------------------------------
void InstallObjectLimitHooks()
{
    HookInstall(reinterpret_cast<void*>(0x4212A4), CreateCheckHook, s_createOrig);
    HookInstall(reinterpret_cast<void*>(0x426AFA), PlaceCheckHook,  s_placeOrig);
    HookInstall(reinterpret_cast<void*>(0x4232D4), PredCheckHook,   s_predOrig);
}
