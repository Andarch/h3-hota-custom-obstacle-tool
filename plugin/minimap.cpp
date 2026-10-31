#include "pch.h"
#include "minimap.h"
#include "hook.h"
#include "logger.h"
#include "obstacle_filter.h"

namespace
{
    DWORD s_colorContinue = 0;
    BYTE s_originalBytes[5] = {};

    DWORD __stdcall CorrectObstacleMinimapIndex(DWORD index, DWORD definition)
    {
        if (!CustomObstacleModeEnabled()) return index;
        // Audited against all 880 supported DEFs in HotA 1.8.1. These native
        // color classes use the normal terrain shade despite blocking cells.
        switch (index)
        {
        case 114: case 115: case 117: case 120: case 122: case 127:
        case 129: case 130: case 132: case 136: case 138: case 139:
        case 140: case 147: case 150: case 151: case 157: case 161:
            break;
        default:
            return index;
        }
        // The editor's shared definition embeds the 76-byte object template
        // at +12; its 48-bit passability mask is at +16/+20 (zero = blocked).
        // Preserve fully passable decorations that share these color classes.
        __try
        {
            if (!definition) return index;
            const auto* record = reinterpret_cast<const DWORD*>(definition);
            if (record[4] == MAXDWORD && (record[5] & 0xFFFF) == 0xFFFF) return index;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return index;
        }
        return index == 157 ? 158 : 128;
    }

    __declspec(naked) void MinimapColorHook()
    {
        __asm
        {
            pushfd
            pushad
            // EAX is the native definition; ESI is only the local color index.
            push eax
            push esi
            call CorrectObstacleMinimapIndex
            mov [esp+4], eax // Replace saved ESI; preserve the definition in EAX.
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
    LogMessage("Minimap obstacle color hook installed; correction is active only in Custom mode.");
    return true;
}
