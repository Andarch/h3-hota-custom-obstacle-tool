#include "pch.h"
#include "minimap.h"
#include "hook.h"
#include "logger.h"
#include "obstacle_filter.h"

namespace
{
    DWORD s_colorContinue = 0;
    BYTE s_originalBytes[5] = {};

    __declspec(naked) void MinimapColorHook()
    {
        __asm
        {
            pushfd
            pushad
            call CustomObstacleModeEnabled
            test al, al
            jz defaultMode
            popad
            popfd
            // ESI is a local color-list index, not the object's stored type.
            cmp esi, 157
            jne nativeCheck
            mov esi, 158
        nativeCheck:
            cmp esi, 0E8h
            jmp dword ptr [s_colorContinue]
        defaultMode:
            popad
            popfd
            cmp esi, 0E8h
            jmp dword ptr [s_colorContinue]
        }
    }
}

bool InstallMinimapColorHook(const TargetProfile& profile, std::string& error)
{
    const auto& site = profile.minimapObstacleColor;
    s_colorContinue = site.address + static_cast<DWORD>(site.expectedLength);
    const HookSpec spec = { reinterpret_cast<void*>(site.address),
        site.expectedBytes, site.expectedLength };
    const auto result = HookInstall(spec, MinimapColorHook, s_originalBytes);
    if (result != HookResult::Success)
    {
        error = std::string(site.name) + ": " + HookResultName(result);
        return false;
    }
    LogMessage("Minimap volcano color hook installed; correction is active only in Custom mode.");
    return true;
}
