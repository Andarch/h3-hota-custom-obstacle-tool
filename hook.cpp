#include "pch.h"
#include "hook.h"
#include <cstring>

bool HookInstall(void* target, void* hookFn, BYTE* originalBytes) {
    DWORD oldProtect;
    if (!VirtualProtect(target, 5, PAGE_EXECUTE_READWRITE, &oldProtect))
        return false;

    if (originalBytes)
        memcpy(originalBytes, target, 5);

    // Write:  JMP rel32  (E9 xx xx xx xx)
    BYTE* p = static_cast<BYTE*>(target);
    p[0] = 0xE9;
    *reinterpret_cast<DWORD*>(p + 1) =
        static_cast<DWORD>(static_cast<BYTE*>(hookFn) - p - 5);

    VirtualProtect(target, 5, oldProtect, &oldProtect);
    return true;
}

bool HookRemove(void* target, const BYTE* originalBytes) {
    DWORD oldProtect;
    if (!VirtualProtect(target, 5, PAGE_EXECUTE_READWRITE, &oldProtect))
        return false;
    memcpy(target, originalBytes, 5);
    VirtualProtect(target, 5, oldProtect, &oldProtect);
    return true;
}
