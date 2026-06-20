#include "pch.h"
#include "obstacle_filter.h"

#include "hook.h"
#include "logger.h"
#include "obstacle_classification.h"
#include "target_profile.h"

#include <cstdlib>
#include <cstring>
#include <string>

namespace
{
    constexpr char kFilterEnvVar[] = "HOTA_MAPED_PLUGIN_OBSTACLE_FILTER";
    constexpr char kExtraBlockedObstacleIdsEnvVar[] =
        "HOTA_MAPED_PLUGIN_BLOCK_OBSTACLE_IDS";
    constexpr char kVariantTraceEnvVar[] =
        "HOTA_MAPED_PLUGIN_OBSTACLE_VARIANT_TRACE";
    constexpr char kExpectedHotaMeSha256[] =
        "3CE4489A0B891E35C13848C33D1124F750D1A0C96D0B52761CA87BFFF5BC0D8E";
    constexpr DWORD kHotaObstacleWeightPredicateRva = 0x000C53B0;
    constexpr DWORD kPredicateStolenLength = 6;
    constexpr DWORD kObjectRecordAssignReturnSite = 0x0049061D;
    constexpr DWORD kObjectRecordAssignReturnStolenLength = 5;
    constexpr DWORD kFinalCandidateRecordSite = 0x0049E592;
    constexpr DWORD kFinalCandidateRecordStolenLength = 5;
    constexpr DWORD kFinalCandidateRejectTarget = 0x0049E69E;

    const BYTE kPredicateExpected[] =
    {
        0x8B, 0x41, 0x04,       // mov eax, [ecx+4]
        0x56,                   // push esi
        0x8B, 0x30,             // mov esi, [eax]
    };

    const BYTE kFinalCandidateRecordExpected[] =
    {
        0x8B, 0x40, 0x10,       // mov eax, [eax+10h]
        0x3B, 0xC3,             // cmp eax, ebx
    };

    const BYTE kObjectRecordAssignReturnExpected[] =
    {
        0x8B, 0xF0,             // mov esi, eax
        0x8D, 0x4D, 0xCC,       // lea ecx, [ebp-34h]
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
    BYTE s_objectRecordAssignReturnOriginal[5] = {};
    BYTE s_finalCandidateRecordOriginal[5] = {};
    BYTE s_extraBlockedRandTrnRows[sizeof(kAllowedRandTrnRows)] = {};
    DWORD s_predicateResume = 0;
    DWORD s_objectRecordAssignReturnResume = 0;
    DWORD s_finalCandidateRecordResume = 0;
    DWORD s_finalCandidateReject = kFinalCandidateRejectTarget;
    DWORD s_extraBlockedRowCount = 0;
    bool s_workbookFilterEnabled = false;
    bool s_variantTraceEnabled = false;
    volatile LONG s_rejectedRows = 0;
    volatile LONG s_observedCandidates = 0;
    volatile LONG s_rejectedCandidates = 0;
    volatile LONG s_generationFilterMode = 0;

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

    bool VariantTraceEnvEnabled()
    {
        char value[16] = {};
        const DWORD length = GetEnvironmentVariableA(kVariantTraceEnvVar, value, sizeof(value));
        return length > 0 && length < sizeof(value) && EnvValueEnabled(value);
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
                    extraBlocked ? "extra-block-list" :
                        "workbook-filter");
            }
            else if (hit == 33)
            {
                LogMessage("Obstacle filter: further rejection logs suppressed.");
            }
        }

        return workbookAllowed && !extraBlocked;
    }

    bool __stdcall ShouldAllowFinalCandidateRecord(DWORD record)
    {
        const auto mode = static_cast<ObstacleGenerationFilterMode>(
            InterlockedCompareExchange(&s_generationFilterMode, 0, 0));
        if (mode == ObstacleGenerationFilterMode::None)
            return true;

        DWORD objectTemplate = 0;
        if (!SafeReadDword(record, objectTemplate))
        {
            const LONG hit = InterlockedIncrement(&s_rejectedCandidates);
            if (hit <= 32)
            {
                LogFormat("Obstacle variants: rejected unreadable final candidate record=%p reason=%s",
                    reinterpret_cast<void*>(record),
                    mode == ObstacleGenerationFilterMode::ThickOnly ? "thick-only" : "thin-only");
            }
            else if (hit == 33)
            {
                LogMessage("Obstacle variants: further candidate rejection logs suppressed.");
            }
            return false;
        }

        const ObstacleThickness thickness =
            GetObstacleThicknessForObjectTemplate(objectTemplate);
        const bool allowed =
            mode == ObstacleGenerationFilterMode::ThickOnly
                ? thickness == ObstacleThickness::Thick
                : thickness == ObstacleThickness::Thin;

        DWORD defKey = 0;
        DWORD objectId = 0;
        DWORD subtype = 0;
        SafeReadDword(objectTemplate, defKey);
        SafeReadDword(objectTemplate + 0x1C, objectId);
        SafeReadDword(objectTemplate + 0x20, subtype);

        const LONG seen = InterlockedIncrement(&s_observedCandidates);
        if (s_variantTraceEnabled && seen <= 16)
        {
            LogFormat(
                "Obstacle variants: observed final candidate record=%p template=%p defKey=%lu objectId=%lu subtype=%lu thickness=%s mode=%s",
                reinterpret_cast<void*>(record),
                reinterpret_cast<void*>(objectTemplate),
                defKey,
                objectId,
                subtype,
                ObstacleThicknessName(thickness),
                mode == ObstacleGenerationFilterMode::ThickOnly ? "thick-only" : "thin-only");
        }

        if (!allowed)
        {
            const LONG hit = InterlockedIncrement(&s_rejectedCandidates);
            if (s_variantTraceEnabled && hit <= 32)
            {
                LogFormat(
                    "Obstacle variants: rejected final candidate record=%p template=%p defKey=%lu objectId=%lu subtype=%lu thickness=%s reason=%s",
                    reinterpret_cast<void*>(record),
                    reinterpret_cast<void*>(objectTemplate),
                    defKey,
                    objectId,
                    subtype,
                    ObstacleThicknessName(thickness),
                    mode == ObstacleGenerationFilterMode::ThickOnly ? "thick-only" : "thin-only");
            }
            else if (s_variantTraceEnabled && hit == 33)
            {
                LogMessage("Obstacle variants: further candidate rejection logs suppressed.");
            }
        }

        return allowed;
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

    __declspec(naked) void FinalCandidateRecordHook()
    {
        __asm
        {
            push    eax
            pushad
            push    dword ptr [esp+28]
            call    ShouldAllowFinalCandidateRecord
            test    al, al
            popad
            pop     eax
            jz      reject

            mov     eax, [eax+10h]
            cmp     eax, ebx
            jmp     dword ptr [s_finalCandidateRecordResume]

        reject:
            mov     [ebp-48h], edi
            jmp     dword ptr [s_finalCandidateReject]
        }
    }

    __declspec(naked) void ObjectRecordAssignReturnHook()
    {
        __asm
        {
            pushad
            lea     edx, [ebp-64h]
            push    edx
            push    eax
            call    RegisterObstacleTemplateDefName
            popad

            mov     esi, eax
            lea     ecx, [ebp-34h]
            jmp     dword ptr [s_objectRecordAssignReturnResume]
        }
    }
}

bool ObstacleFilterEnabled()
{
    return WorkbookFilterEnvEnabled() || ExtraBlockListEnvPresent();
}

bool InstallObstacleFilterHooks(std::string& error)
{
    const bool rowFilterEnabled = ObstacleFilterEnabled();
    const bool candidateFilterEnabled = ObstacleThicknessTableAvailable();
    if (!rowFilterEnabled && !candidateFilterEnabled)
    {
        LogMessage("Obstacle filter: no row or candidate hooks requested.");
        return true;
    }

    if (rowFilterEnabled)
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
    }

    if (candidateFilterEnabled)
    {
        s_variantTraceEnabled = VariantTraceEnvEnabled();

        void* const objectRecordAssignReturn =
            reinterpret_cast<void*>(kObjectRecordAssignReturnSite);
        s_objectRecordAssignReturnResume =
            kObjectRecordAssignReturnSite + kObjectRecordAssignReturnStolenLength;

        const HookSpec objectRecordSpec =
        {
            objectRecordAssignReturn,
            kObjectRecordAssignReturnExpected,
            sizeof(kObjectRecordAssignReturnExpected),
        };

        HookResult result =
            HookInstall(
                objectRecordSpec,
                ObjectRecordAssignReturnHook,
                s_objectRecordAssignReturnOriginal);
        if (result != HookResult::Success)
        {
            error = std::string("map editor object DEF-key registration: ") +
                HookResultName(result);
            return false;
        }

        void* const predicate = reinterpret_cast<void*>(kFinalCandidateRecordSite);
        s_finalCandidateRecordResume =
            kFinalCandidateRecordSite + kFinalCandidateRecordStolenLength;

        const HookSpec spec =
        {
            predicate,
            kFinalCandidateRecordExpected,
            sizeof(kFinalCandidateRecordExpected),
        };

        result =
            HookInstall(spec, FinalCandidateRecordHook, s_finalCandidateRecordOriginal);
        if (result != HookResult::Success)
        {
            error = std::string("map editor final obstacle candidate record: ") +
                HookResultName(result);
            const HookResult rollback =
                HookRemove(objectRecordAssignReturn, s_objectRecordAssignReturnOriginal);
            if (rollback != HookResult::Success)
            {
                error += std::string("; rollback failed for object DEF-key registration: ") +
                    HookResultName(rollback);
            }
            return false;
        }

        LogFormat(
            "Obstacle variants installed: objectDefRegistration=%p finalCandidateRecord=%p thicknessTable=available trace=%s",
            objectRecordAssignReturn,
            predicate,
            s_variantTraceEnabled ? "on" : "off");
    }

    return true;
}

void SetObstacleGenerationFilterMode(ObstacleGenerationFilterMode mode)
{
    if (mode != ObstacleGenerationFilterMode::None)
    {
        InterlockedExchange(&s_observedCandidates, 0);
        InterlockedExchange(&s_rejectedCandidates, 0);
    }

    InterlockedExchange(&s_generationFilterMode, static_cast<LONG>(mode));
}
