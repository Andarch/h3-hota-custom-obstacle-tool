#include "pch.h"
#include "obstacle_filter.h"

#include "hook.h"
#include "logger.h"
#include "target_profile.h"

#include <cstdlib>
#include <cstring>
#include <string>

namespace
{
    constexpr char kFilterEnvVar[] = "HOTA_MAPED_PLUGIN_OBSTACLE_FILTER";
    constexpr char kExtraBlockedObstacleIdsEnvVar[] =
        "HOTA_MAPED_PLUGIN_BLOCK_OBSTACLE_IDS";
    constexpr char kExpectedHotaMeSha256[] =
        "3CE4489A0B891E35C13848C33D1124F750D1A0C96D0B52761CA87BFFF5BC0D8E";
    constexpr DWORD kHotaObstacleWeightPredicateRva = 0x000C53B0;
    constexpr DWORD kPredicateStolenLength = 6;

    const BYTE kPredicateExpected[] =
    {
        0x8B, 0x41, 0x04,       // mov eax, [ecx+4]
        0x56,                   // push esi
        0x8B, 0x30,             // mov esi, [eax]
    };

    const BYTE kAllowedRandTrnRows[] =
    {
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1,
        1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1,
        1, 1, 1, 0, 0, 0, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1,
    };

    BYTE s_predicateOriginal[5] = {};
    BYTE s_extraBlockedRandTrnRows[sizeof(kAllowedRandTrnRows)] = {};
    DWORD s_predicateResume = 0;
    DWORD s_extraBlockedRowCount = 0;
    bool s_workbookFilterEnabled = false;
    volatile LONG s_rejectedRows = 0;

    bool EnvValueEnabled(const char* value)
    {
        return _stricmp(value, "1") == 0 ||
            _stricmp(value, "true") == 0 ||
            _stricmp(value, "yes") == 0 ||
            _stricmp(value, "on") == 0;
    }

    bool WorkbookFilterEnvEnabled()
    {
        char value[16] = {};
        const DWORD length = GetEnvironmentVariableA(kFilterEnvVar, value, sizeof(value));
        return length > 0 && length < sizeof(value) && EnvValueEnabled(value);
    }

    bool ExtraBlockListEnvPresent()
    {
        char value[2] = {};
        const DWORD length = GetEnvironmentVariableA(
            kExtraBlockedObstacleIdsEnvVar,
            value,
            sizeof(value));
        return length > 0;
    }

    bool SafeReadDword(DWORD address, DWORD& value)
    {
        __try
        {
            value = *reinterpret_cast<const DWORD*>(address);
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            value = 0;
            return false;
        }
    }

    bool ValidateHotaMeModule(HMODULE module, std::string& error)
    {
        char modulePath[MAX_PATH] = {};
        const DWORD pathLength = GetModuleFileNameA(module, modulePath, MAX_PATH);
        if (pathLength == 0 || pathLength == MAX_PATH)
        {
            error = "could not resolve hota_me.dll path";
            return false;
        }

        char moduleHash[65] = {};
        if (!ComputeFileSha256(modulePath, moduleHash))
        {
            error = "could not hash hota_me.dll";
            return false;
        }

        LogFormat("Obstacle filter: hota_me.dll SHA-256: %s", moduleHash);
        if (strcmp(moduleHash, kExpectedHotaMeSha256) != 0)
        {
            error = "unsupported hota_me.dll hash";
            return false;
        }

        return true;
    }

    bool IsListSeparator(char value)
    {
        return value == ' ' || value == '\t' || value == ',' || value == ';';
    }

    void InitializeExtraBlockedRows()
    {
        char value[512] = {};
        const DWORD length = GetEnvironmentVariableA(
            kExtraBlockedObstacleIdsEnvVar,
            value,
            sizeof(value));
        if (length == 0)
            return;

        if (length >= sizeof(value))
        {
            LogFormat("Obstacle filter: %s is too long; ignoring extra block list.",
                kExtraBlockedObstacleIdsEnvVar);
            return;
        }

        const char* cursor = value;
        while (*cursor)
        {
            while (*cursor && IsListSeparator(*cursor))
                ++cursor;
            if (!*cursor)
                break;

            char* end = nullptr;
            const unsigned long obstacleId = std::strtoul(cursor, &end, 10);
            if (end == cursor)
            {
                LogFormat("Obstacle filter: could not parse %s near \"%s\".",
                    kExtraBlockedObstacleIdsEnvVar,
                    cursor);
                break;
            }

            if (obstacleId == 0 || obstacleId > sizeof(kAllowedRandTrnRows))
            {
                LogFormat("Obstacle filter: ignored out-of-range obstacleId=%lu from %s.",
                    obstacleId,
                    kExtraBlockedObstacleIdsEnvVar);
            }
            else
            {
                const DWORD rowId = static_cast<DWORD>(obstacleId - 1);
                if (s_extraBlockedRandTrnRows[rowId] == 0)
                {
                    s_extraBlockedRandTrnRows[rowId] = 1;
                    ++s_extraBlockedRowCount;
                }
            }

            cursor = end;
        }

        LogFormat("Obstacle filter: extra blocked obstacle IDs from %s: %lu",
            kExtraBlockedObstacleIdsEnvVar,
            s_extraBlockedRowCount);
    }

    bool __stdcall ShouldAllowRandTrnRow(DWORD row)
    {
        DWORD rowId = 0;
        if (!SafeReadDword(row, rowId))
            return true;

        if (rowId >= sizeof(kAllowedRandTrnRows))
            return true;

        const bool workbookAllowed =
            !s_workbookFilterEnabled || kAllowedRandTrnRows[rowId] != 0;
        const bool extraBlocked = s_extraBlockedRandTrnRows[rowId] != 0;
        if (!workbookAllowed || extraBlocked)
        {
            const LONG hit = InterlockedIncrement(&s_rejectedRows);
            if (hit <= 32)
            {
                LogFormat("Obstacle filter: rejected rand_trn rowId=%lu obstacleId=%lu reason=%s",
                    rowId,
                    rowId + 1,
                    extraBlocked ? "extra-block-list" : "workbook-filter");
            }
            else if (hit == 33)
            {
                LogMessage("Obstacle filter: further rejection logs suppressed.");
            }
        }

        return workbookAllowed && !extraBlocked;
    }

    __declspec(naked) void HotaObstacleWeightPredicateHook()
    {
        __asm
        {
            push    edx
            push    ecx
            push    ecx
            call    ShouldAllowRandTrnRow
            pop     ecx
            pop     edx
            test    al, al
            jz      reject

            mov     eax, [ecx+4]
            push    esi
            mov     esi, [eax]
            jmp     dword ptr [s_predicateResume]

        reject:
            xor     al, al
            ret     4
        }
    }
}

bool ObstacleFilterEnabled()
{
    return WorkbookFilterEnvEnabled() || ExtraBlockListEnvPresent();
}

bool InstallObstacleFilterHooks(std::string& error)
{
    HMODULE hotaMe = GetModuleHandleA("hota_me.dll");
    if (!hotaMe)
    {
        error = "hota_me.dll is not loaded";
        return false;
    }

    if (!ValidateHotaMeModule(hotaMe, error))
        return false;

    s_workbookFilterEnabled = WorkbookFilterEnvEnabled();
    InitializeExtraBlockedRows();

    BYTE* const moduleBase = reinterpret_cast<BYTE*>(hotaMe);
    void* const predicate = moduleBase + kHotaObstacleWeightPredicateRva;
    s_predicateResume = reinterpret_cast<DWORD>(predicate) + kPredicateStolenLength;

    const HookSpec spec =
    {
        predicate,
        kPredicateExpected,
        sizeof(kPredicateExpected),
    };

    const HookResult result =
        HookInstall(spec, HotaObstacleWeightPredicateHook, s_predicateOriginal);
    if (result != HookResult::Success)
    {
        error = std::string("HotA obstacle weight predicate: ") + HookResultName(result);
        return false;
    }

    LogFormat("Obstacle filter installed: predicate=%p workbookFilter=%s allowedRows=159 rejectedRows=41 extraBlockedRows=%lu",
        predicate,
        s_workbookFilterEnabled ? "on" : "off",
        s_extraBlockedRowCount);
    return true;
}
