#include "pch.h"
#include "garrison.h"
#include "hook.h"

// Type ID for "Garrison" in the H3 object type system.
static const DWORD kGarrisonTypeId = 0x21;

// New per-map limit for garrison objects (game engine supports up to 255).
static DWORD s_garrisonLimit = 255;

// ---------------------------------------------------------------------------
// Create check  (sub_421222 @ 0x4212A4)
//   Original bytes: cmp ecx, [eax+8]  (3 bytes)
//                   jb  loc_4212D1    (2 bytes)
// ---------------------------------------------------------------------------
static BYTE  s_createOrig[5];
static DWORD s_createAllow = 0x4212D1;
static DWORD s_createDeny  = 0x4212A9;

__declspec(naked) static void CreateCheckHook()
{
    __asm {
        cmp     dword ptr [eax], 021h
        jne     chk_other
        cmp     ecx, dword ptr [s_garrisonLimit]
        jb      chk_allow
        mov     dword ptr [eax+8], 0FFh
        jmp     chk_deny
    chk_other:
        cmp     ecx, dword ptr [eax+8]
        jb      chk_allow
    chk_deny:
        jmp     dword ptr [s_createDeny]
    chk_allow:
        jmp     dword ptr [s_createAllow]
    }
}

// ---------------------------------------------------------------------------
// Place check  (sub_426A49 @ 0x426AFA)
//   Original bytes: cmp ecx, [eax+8]  (3 bytes)
//                   jb  loc_426B27    (2 bytes)
// ---------------------------------------------------------------------------
static BYTE  s_placeOrig[5];
static DWORD s_placeAllow = 0x426B27;
static DWORD s_placeDeny  = 0x426AFF;

__declspec(naked) static void PlaceCheckHook()
{
    __asm {
        cmp     dword ptr [eax], 021h
        jne     plc_other
        cmp     ecx, dword ptr [s_garrisonLimit]
        jb      plc_allow
        mov     dword ptr [eax+8], 0FFh
        jmp     plc_deny
    plc_other:
        cmp     ecx, dword ptr [eax+8]
        jb      plc_allow
    plc_deny:
        jmp     dword ptr [s_placeDeny]
    plc_allow:
        jmp     dword ptr [s_placeAllow]
    }
}

// ---------------------------------------------------------------------------
// "Can place?" predicate  (sub_423278 @ 0x4232D4)
//   Original bytes: cmp eax, [esi+8]  (3 bytes)
//                   jnb loc_423485    (6 bytes, near jump)
//   Note: esi = type_info ptr, eax = current count.
//         jnb direction is reversed — deny if >= max.
// ---------------------------------------------------------------------------
static BYTE  s_predOrig[5];
static DWORD s_predAllow = 0x4232DD;
static DWORD s_predDeny  = 0x423485;

__declspec(naked) static void PredCheckHook()
{
    __asm {
        cmp     dword ptr [esi], 021h
        jne     prd_other
        cmp     eax, dword ptr [s_garrisonLimit]
        jnb     prd_deny
        jmp     prd_allow
    prd_other:
        cmp     eax, dword ptr [esi+8]
        jnb     prd_deny
    prd_allow:
        jmp     dword ptr [s_predAllow]
    prd_deny:
        jmp     dword ptr [s_predDeny]
    }
}

// ---------------------------------------------------------------------------
void InstallGarrisonHooks()
{
    HookInstall(reinterpret_cast<void*>(0x4212A4), CreateCheckHook, s_createOrig);
    HookInstall(reinterpret_cast<void*>(0x426AFA), PlaceCheckHook,  s_placeOrig);
    HookInstall(reinterpret_cast<void*>(0x4232D4), PredCheckHook,   s_predOrig);
}
