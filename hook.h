#pragma once
#include "pch.h"

// Install a 5-byte JMP hook at `target`, redirecting execution to `hookFn`.
// Saves the original 5 bytes into `originalBytes` (caller supplies >= 5 byte buffer).
// Returns false if VirtualProtect fails.
bool HookInstall(void* target, void* hookFn, BYTE* originalBytes = nullptr);

// Restore the original 5 bytes at `target` to remove the hook.
bool HookRemove(void* target, const BYTE* originalBytes);
