#pragma once
#include "pch.h"

struct HookSpec
{
    void* target;
    const BYTE* expectedBytes;
    SIZE_T expectedLength;
};

enum class HookResult
{
    Success,
    InvalidArguments,
    UnexpectedBytes,
    ProtectFailed,
    RestoreProtectionFailed,
    FlushInstructionCacheFailed,
    RollbackFailed,
};

HookResult HookValidate(const HookSpec& spec);
HookResult HookInstall(const HookSpec& spec, void* hookFn, BYTE originalBytes[5]);
HookResult HookRemove(void* target, const BYTE originalBytes[5]);
const char* HookResultName(HookResult result);
