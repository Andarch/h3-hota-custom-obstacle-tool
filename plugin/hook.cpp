#include "pch.h"
#include "hook.h"

#include <cstring>

namespace
{
    constexpr SIZE_T kJumpSize = 5;

    HookResult WriteBytes(void* target, const BYTE bytes[kJumpSize])
    {
        DWORD oldProtect = 0;
        if (!VirtualProtect(target, kJumpSize, PAGE_EXECUTE_READWRITE, &oldProtect))
            return HookResult::ProtectFailed;

        memcpy(target, bytes, kJumpSize);

        DWORD unusedProtect = 0;
        const bool protectionRestored =
            VirtualProtect(target, kJumpSize, oldProtect, &unusedProtect) != FALSE;
        const bool cacheFlushed =
            FlushInstructionCache(GetCurrentProcess(), target, kJumpSize) != FALSE;

        if (!protectionRestored)
            return HookResult::RestoreProtectionFailed;
        if (!cacheFlushed)
            return HookResult::FlushInstructionCacheFailed;
        return HookResult::Success;
    }
}

HookResult HookValidate(const HookSpec& spec)
{
    if (!spec.target || !spec.expectedBytes || spec.expectedLength < kJumpSize)
        return HookResult::InvalidArguments;

    return memcmp(spec.target, spec.expectedBytes, spec.expectedLength) == 0
        ? HookResult::Success
        : HookResult::UnexpectedBytes;
}

HookResult HookInstall(const HookSpec& spec, void* hookFn, BYTE originalBytes[5])
{
    if (!hookFn || !originalBytes)
        return HookResult::InvalidArguments;

    const HookResult validation = HookValidate(spec);
    if (validation != HookResult::Success)
        return validation;

    memcpy(originalBytes, spec.target, kJumpSize);

    BYTE patch[kJumpSize] = { 0xE9 };
    const DWORD relativeJump = static_cast<DWORD>(
        reinterpret_cast<DWORD_PTR>(hookFn) -
        reinterpret_cast<DWORD_PTR>(spec.target) -
        kJumpSize);
    memcpy(patch + 1, &relativeJump, sizeof(relativeJump));

    const HookResult result = WriteBytes(spec.target, patch);
    if (result == HookResult::RestoreProtectionFailed ||
        result == HookResult::FlushInstructionCacheFailed)
    {
        if (WriteBytes(spec.target, originalBytes) != HookResult::Success)
            return HookResult::RollbackFailed;
    }
    return result;
}

HookResult HookRemove(void* target, const BYTE originalBytes[5])
{
    if (!target || !originalBytes)
        return HookResult::InvalidArguments;

    return WriteBytes(target, originalBytes);
}

const char* HookResultName(HookResult result)
{
    switch (result)
    {
    case HookResult::Success:
        return "success";
    case HookResult::InvalidArguments:
        return "invalid arguments";
    case HookResult::UnexpectedBytes:
        return "unexpected original bytes";
    case HookResult::ProtectFailed:
        return "VirtualProtect failed";
    case HookResult::RestoreProtectionFailed:
        return "restoring page protection failed";
    case HookResult::FlushInstructionCacheFailed:
        return "FlushInstructionCache failed";
    case HookResult::RollbackFailed:
        return "rollback failed after writing the hook";
    default:
        return "unknown hook error";
    }
}
