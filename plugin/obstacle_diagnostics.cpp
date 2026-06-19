#include "pch.h"
#include "obstacle_diagnostics.h"

#include "hook.h"
#include "logger.h"
#include "target_profile.h"

#include <cstdio>
#include <cstring>
#include <string>

namespace
{
    constexpr char kDiagnosticsEnvVar[] = "HOTA_MAPED_PLUGIN_DIAGNOSTICS";
    constexpr char kExpectedHotaMeSha256[] =
        "3CE4489A0B891E35C13848C33D1124F750D1A0C96D0B52761CA87BFFF5BC0D8E";

    constexpr DWORD kHotaParserEntryRva = 0x000D67A0;
    constexpr DWORD kHotaParserStoreRva = 0x000D6DF4;
    constexpr DWORD kParserEntryStolenLength = 6;
    constexpr DWORD kParserStoreStolenLength = 13;
    constexpr DWORD kFinalCandidateRecordSite = 0x0049E592;
    constexpr DWORD kFinalCandidateRecordStolenLength = 5;
    constexpr DWORD kFinalCandidateWeightSite = 0x0049E5A0;
    constexpr DWORD kFinalCandidateWeightStolenLength = 11;
    constexpr DWORD kFinalCandidateByteWindow = 0x0049E580;
    constexpr DWORD kFinalCandidateByteWindowLength = 0x60;
    constexpr DWORD kCandidateWeightHelperByteWindow = 0x0049DF11;
    constexpr DWORD kCandidateWeightHelperByteWindowLength = 0x70;
    constexpr DWORD kCandidateWeightHelperPatchJump = 0x0049DF11;
    constexpr DWORD kFinalCandidateWeightPatchJump = 0x0049E5A3;
    constexpr DWORD kPatchTargetByteWindowLength = 0xC0;

    const BYTE kParserEntryExpected[] =
    {
        0x55,                   // push ebp
        0x8B, 0xEC,             // mov ebp, esp
        0x83, 0xE4, 0xF8,       // and esp, 0FFFFFFF8h
    };

    const BYTE kParserStoreExpected[] =
    {
        0x8B, 0x44, 0x24, 0x30, // mov eax, [esp+30h]
        0x8B, 0x40, 0x04,       // mov eax, [eax+4]
        0x8B, 0x04, 0x88,       // mov eax, [eax+ecx*4]
        0x89, 0x43, 0x10,       // mov [ebx+10h], eax
    };

    const BYTE kFinalCandidateRecordExpected[] =
    {
        0x8B, 0x40, 0x10,       // mov eax, [eax+10h]
        0x3B, 0xC3,             // cmp eax, ebx
    };

    const BYTE kFinalCandidateWeightExpected[] =
    {
        0x8B, 0x75, 0xEC,       // mov esi, [ebp-14h]
        0x81, 0x7C, 0xB0, 0x04, // cmp dword ptr [eax+esi*4+4], -5000
        0x78, 0xEC, 0xFF, 0xFF,
    };

    const BYTE kFinalCandidatePatchedWeightExpected[] =
    {
        0x81, 0x7C, 0xB0, 0x04, // cmp dword ptr [eax+esi*4+4], -5000
        0x78, 0xEC, 0xFF, 0xFF,
    };

    BYTE s_parserEntryOriginal[5] = {};
    BYTE s_parserStoreOriginal[5] = {};
    BYTE s_finalCandidateRecordOriginal[5] = {};
    BYTE s_finalCandidateWeightOriginal[5] = {};
    DWORD s_parserEntryResume = 0;
    DWORD s_parserStoreResume = 0;
    DWORD s_finalCandidateRecordResume = 0;
    DWORD s_finalCandidateWeightResume = 0;
    volatile LONG s_parserEntryHits = 0;
    volatile LONG s_parserStoreHits = 0;
    volatile LONG s_finalCandidateRecordHits = 0;
    volatile LONG s_finalCandidateWeightHits = 0;

    bool EnvValueEnabled(const char* value)
    {
        return _stricmp(value, "1") == 0 ||
            _stricmp(value, "true") == 0 ||
            _stricmp(value, "yes") == 0 ||
            _stricmp(value, "on") == 0;
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

    bool SafeReadByte(DWORD address, BYTE& value)
    {
        __try
        {
            value = *reinterpret_cast<const BYTE*>(address);
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            value = 0;
            return false;
        }
    }

    std::string FormatByteArray(const BYTE* bytes, SIZE_T length)
    {
        std::string result;
        result.reserve(length * 3);
        for (SIZE_T i = 0; i < length; ++i)
        {
            char text[4] = {};
            std::snprintf(text, sizeof(text), "%02X", bytes[i]);
            if (!result.empty())
                result += ' ';
            result += text;
        }
        return result;
    }

    std::string FormatMemoryBytes(DWORD address, SIZE_T length)
    {
        std::string result;
        result.reserve(length * 3);
        for (SIZE_T i = 0; i < length; ++i)
        {
            BYTE value = 0;
            if (!result.empty())
                result += ' ';

            if (SafeReadByte(address + static_cast<DWORD>(i), value))
            {
                char text[4] = {};
                std::snprintf(text, sizeof(text), "%02X", value);
                result += text;
            }
            else
            {
                result += "??";
            }
        }
        return result;
    }

    void LogCodeBytes(const char* name, DWORD address, SIZE_T length)
    {
        const std::string bytes = FormatMemoryBytes(address, length);
        LogFormat("Obstacle diagnostics: live bytes %s address=0x%08lX length=%lu [%s]",
            name,
            address,
            static_cast<unsigned long>(length),
            bytes.c_str());
    }

    bool TryReadRelativeJumpTarget(DWORD address, DWORD& target)
    {
        target = 0;

        BYTE opcode = 0;
        DWORD relative = 0;
        if (!SafeReadByte(address, opcode) || opcode != 0xE9 ||
            !SafeReadDword(address + 1, relative))
        {
            return false;
        }

        target = address + 5 + relative;
        return true;
    }

    void LogRelativeJumpTargetBytes(const char* name, DWORD jumpAddress, SIZE_T length)
    {
        DWORD target = 0;
        if (!TryReadRelativeJumpTarget(jumpAddress, target))
        {
            LogFormat("Obstacle diagnostics: %s at 0x%08lX is not a readable near jump",
                name,
                jumpAddress);
            return;
        }

        LogFormat("Obstacle diagnostics: %s jump address=0x%08lX target=0x%08lX",
            name,
            jumpAddress,
            target);
        LogCodeBytes(name, target, length);
    }

    void LogFinalCandidatePatchHandlerBytes(DWORD patchTarget)
    {
        BYTE descriptorPush = 0;
        BYTE handlerPush = 0;
        DWORD descriptor = 0;
        DWORD handler = 0;
        if (!SafeReadByte(patchTarget + 0x23, descriptorPush) ||
            !SafeReadDword(patchTarget + 0x24, descriptor) ||
            !SafeReadByte(patchTarget + 0x28, handlerPush) ||
            !SafeReadDword(patchTarget + 0x29, handler) ||
            descriptorPush != 0x68 ||
            handlerPush != 0x68)
        {
            LogFormat(
                "Obstacle diagnostics: final candidate patch handler operands not recognized at target=0x%08lX",
                patchTarget);
            return;
        }

        LogFormat(
            "Obstacle diagnostics: final candidate patch handler descriptor=0x%08lX handler=0x%08lX",
            descriptor,
            handler);
        LogCodeBytes("final candidate patch descriptor", descriptor, 0x80);
        LogCodeBytes("final candidate patch handler", handler, 0x180);
    }

    DWORD TryReadMapCellTerrain(DWORD generator, DWORD x, DWORD y, DWORD z, DWORD& cell);

    bool TryReadWeightAt(DWORD weightBase, DWORD terrain, DWORD& value)
    {
        value = 0;
        if (weightBase == 0 || terrain >= 64)
            return false;

        return SafeReadDword(weightBase + terrain * 4, value);
    }

    bool FindPatternInMemory(
        DWORD start,
        SIZE_T length,
        const BYTE* pattern,
        SIZE_T patternLength,
        DWORD& address)
    {
        address = 0;
        if (!pattern || patternLength == 0 || length < patternLength)
            return false;

        for (SIZE_T offset = 0; offset <= length - patternLength; ++offset)
        {
            bool matched = true;
            for (SIZE_T i = 0; i < patternLength; ++i)
            {
                BYTE value = 0;
                if (!SafeReadByte(start + static_cast<DWORD>(offset + i), value) ||
                    value != pattern[i])
                {
                    matched = false;
                    break;
                }
            }

            if (matched)
            {
                address = start + static_cast<DWORD>(offset);
                return true;
            }
        }

        return false;
    }

    void __stdcall LogParserEntryHit(DWORD thisPointer)
    {
        const LONG hit = InterlockedIncrement(&s_parserEntryHits);
        if (hit <= 16)
        {
            LogFormat("Obstacle diagnostics: HotA rand_trn parser entered hit=%ld this=0x%08lX",
                hit, thisPointer);
        }
        else if (hit == 17)
        {
            LogMessage("Obstacle diagnostics: further HotA rand_trn parser entry logs suppressed.");
        }
    }

    void __stdcall LogParserStoreHit(DWORD wrapper, DWORD value, DWORD index)
    {
        const LONG hit = InterlockedIncrement(&s_parserStoreHits);
        if (hit > 256)
        {
            if (hit == 257)
                LogMessage("Obstacle diagnostics: further wrapper[+0x10] store logs suppressed.");
            return;
        }

        DWORD objectTemplate = 0;
        DWORD objectType = 0;
        DWORD objectSubtype = 0;
        DWORD firstTerrain = 0;
        const bool haveTemplate = SafeReadDword(wrapper, objectTemplate);
        SafeReadDword(wrapper + 4, firstTerrain);
        if (haveTemplate)
        {
            SafeReadDword(objectTemplate + 28, objectType);
            SafeReadDword(objectTemplate + 32, objectSubtype);
        }

        DWORD stored0 = 0;
        DWORD stored4 = 0;
        DWORD stored8 = 0;
        DWORD stored12 = 0;
        DWORD stored16 = 0;
        DWORD stored20 = 0;
        DWORD stored24 = 0;
        DWORD stored28 = 0;
        const bool haveStoredRecord =
            SafeReadDword(value + 0, stored0) &&
            SafeReadDword(value + 4, stored4) &&
            SafeReadDword(value + 8, stored8) &&
            SafeReadDword(value + 12, stored12) &&
            SafeReadDword(value + 16, stored16) &&
            SafeReadDword(value + 20, stored20) &&
            SafeReadDword(value + 24, stored24) &&
            SafeReadDword(value + 28, stored28);

        LogFormat(
            "Obstacle diagnostics: wrapper[+0x10] hit=%ld wrapper=0x%08lX template=0x%08lX "
            "type=%lu subtype=%ld firstTerrain=%ld value=0x%08lX searchIndex=%lu "
            "storedRecord=%s [%08lX %08lX %08lX %08lX %08lX %08lX %08lX %08lX]",
            hit,
            wrapper,
            objectTemplate,
            objectType,
            static_cast<LONG>(objectSubtype),
            static_cast<LONG>(firstTerrain),
            value,
            index,
            haveStoredRecord ? "ok" : "unreadable",
            stored0,
            stored4,
            stored8,
            stored12,
            stored16,
            stored20,
            stored24,
            stored28);
    }

    void __stdcall LogFinalCandidateRecordHit(
        DWORD generator,
        DWORD record,
        DWORD row,
        DWORD stackTerrain,
        DWORD x,
        DWORD y,
        DWORD z)
    {
        const LONG hit = InterlockedIncrement(&s_finalCandidateRecordHits);
        if (hit > 128)
        {
            if (hit == 129)
                LogMessage("Obstacle diagnostics: further final candidate record logs suppressed.");
            return;
        }

        DWORD objectTemplate = 0;
        DWORD firstTerrain = 0;
        DWORD objectType = 0;
        DWORD objectSubtype = 0;
        DWORD candidateX = 0;
        DWORD candidateY = 0;
        DWORD candidateZ = 0;
        SafeReadDword(record + 0, objectTemplate);
        SafeReadDword(record + 4, firstTerrain);
        SafeReadDword(record + 8, candidateX);
        SafeReadDword(record + 12, candidateY);
        SafeReadDword(record + 16, candidateZ);
        if (objectTemplate)
        {
            SafeReadDword(objectTemplate + 28, objectType);
            SafeReadDword(objectTemplate + 32, objectSubtype);
        }

        DWORD rowDwords[12] = {};
        bool rowReadable = row != 0;
        for (size_t i = 0; i < sizeof(rowDwords) / sizeof(rowDwords[0]); ++i)
        {
            if (!SafeReadDword(row + static_cast<DWORD>(i * sizeof(DWORD)), rowDwords[i]))
                rowReadable = false;
        }

        DWORD cell = 0;
        const DWORD cellTerrain = TryReadMapCellTerrain(generator, x, y, z, cell);

        DWORD directStackValue = 0;
        const bool directStackReadable = TryReadWeightAt(row + 4, stackTerrain, directStackValue);

        DWORD directCellValue = 0;
        const bool directCellReadable =
            cellTerrain != 0xFFFFFFFF && TryReadWeightAt(row + 4, cellTerrain, directCellValue);

        DWORD row4 = 0;
        DWORD row4Deref0 = 0;
        SafeReadDword(row + 4, row4);
        const bool haveRow4Deref0 = row4 != 0 && SafeReadDword(row4, row4Deref0);

        DWORD row4StackValue = 0;
        const bool row4StackReadable = TryReadWeightAt(row4, stackTerrain, row4StackValue);

        DWORD row4CellValue = 0;
        const bool row4CellReadable =
            cellTerrain != 0xFFFFFFFF && TryReadWeightAt(row4, cellTerrain, row4CellValue);

        DWORD row4DerefStackValue = 0;
        const bool row4DerefStackReadable =
            haveRow4Deref0 && TryReadWeightAt(row4Deref0, stackTerrain, row4DerefStackValue);

        DWORD row4DerefCellValue = 0;
        const bool row4DerefCellReadable =
            haveRow4Deref0 && cellTerrain != 0xFFFFFFFF &&
            TryReadWeightAt(row4Deref0, cellTerrain, row4DerefCellValue);

        LogFormat(
            "Obstacle diagnostics: finalCandidate[+0x10] hit=%ld record=0x%08lX template=0x%08lX "
            "type=%lu subtype=%ld firstTerrain=%ld row=0x%08lX terrain=%ld "
            "cellTerrain=%ld cell=0x%08lX cellXYZ=(%ld,%ld,%ld) candidateXYZ=(%ld,%ld,%ld) "
            "directStackValue=%s%ld directCellValue=%s%ld row4=0x%08lX row4Deref0=%s0x%08lX "
            "row4StackValue=%s%ld row4CellValue=%s%ld "
            "row4DerefStackValue=%s%ld row4DerefCellValue=%s%ld rowData=%s "
            "[%08lX %08lX %08lX %08lX %08lX %08lX %08lX %08lX %08lX %08lX %08lX %08lX]",
            hit,
            record,
            objectTemplate,
            objectType,
            static_cast<LONG>(objectSubtype),
            static_cast<LONG>(firstTerrain),
            row,
            static_cast<LONG>(stackTerrain),
            cellTerrain == 0xFFFFFFFF ? -1 : static_cast<LONG>(cellTerrain),
            cell,
            static_cast<LONG>(x),
            static_cast<LONG>(y),
            static_cast<LONG>(z),
            static_cast<LONG>(candidateX),
            static_cast<LONG>(candidateY),
            static_cast<LONG>(candidateZ),
            directStackReadable ? "" : "unreadable:",
            directStackReadable ? static_cast<LONG>(directStackValue) : 0,
            directCellReadable ? "" : "unreadable:",
            directCellReadable ? static_cast<LONG>(directCellValue) : 0,
            row4,
            haveRow4Deref0 ? "" : "unreadable:",
            row4Deref0,
            row4StackReadable ? "" : "unreadable:",
            row4StackReadable ? static_cast<LONG>(row4StackValue) : 0,
            row4CellReadable ? "" : "unreadable:",
            row4CellReadable ? static_cast<LONG>(row4CellValue) : 0,
            row4DerefStackReadable ? "" : "unreadable:",
            row4DerefStackReadable ? static_cast<LONG>(row4DerefStackValue) : 0,
            row4DerefCellReadable ? "" : "unreadable:",
            row4DerefCellReadable ? static_cast<LONG>(row4DerefCellValue) : 0,
            rowReadable ? "ok" : "partial",
            rowDwords[0],
            rowDwords[1],
            rowDwords[2],
            rowDwords[3],
            rowDwords[4],
            rowDwords[5],
            rowDwords[6],
            rowDwords[7],
            rowDwords[8],
            rowDwords[9],
            rowDwords[10],
            rowDwords[11]);
    }

    DWORD TryReadMapCellTerrain(DWORD generator, DWORD x, DWORD y, DWORD z, DWORD& cell)
    {
        cell = 0;

        DWORD cellBase = 0;
        DWORD width = 0;
        DWORD height = 0;
        DWORD terrainAndFlags = 0;
        if (!SafeReadDword(generator + 0x14, cellBase) ||
            !SafeReadDword(generator + 0x18, width) ||
            !SafeReadDword(generator + 0x1C, height) ||
            width == 0 ||
            height == 0)
        {
            return 0xFFFFFFFF;
        }

        cell = cellBase + 48 * (x + width * (y + z * height));
        if (!SafeReadDword(cell + 36, terrainAndFlags))
            return 0xFFFFFFFF;

        return terrainAndFlags & 0x3F;
    }

    void __stdcall LogFinalCandidateWeightHit(
        DWORD generator,
        DWORD record,
        DWORD row,
        DWORD stackTerrain,
        DWORD comparedValue,
        DWORD x,
        DWORD y,
        DWORD z)
    {
        const LONG hit = InterlockedIncrement(&s_finalCandidateWeightHits);
        if (hit > 128)
        {
            if (hit == 129)
                LogMessage("Obstacle diagnostics: further final candidate weight logs suppressed.");
            return;
        }

        DWORD objectTemplate = 0;
        DWORD firstTerrain = 0;
        DWORD objectType = 0;
        DWORD objectSubtype = 0;
        DWORD candidateX = 0;
        DWORD candidateY = 0;
        DWORD candidateZ = 0;
        SafeReadDword(record + 0, objectTemplate);
        SafeReadDword(record + 4, firstTerrain);
        SafeReadDword(record + 8, candidateX);
        SafeReadDword(record + 12, candidateY);
        SafeReadDword(record + 16, candidateZ);
        if (objectTemplate)
        {
            SafeReadDword(objectTemplate + 28, objectType);
            SafeReadDword(objectTemplate + 32, objectSubtype);
        }

        DWORD rowId = 0;
        DWORD weightPointer = 0;
        SafeReadDword(row + 0, rowId);
        SafeReadDword(row + 4, weightPointer);

        DWORD cell = 0;
        const DWORD cellTerrain = TryReadMapCellTerrain(generator, x, y, z, cell);

        DWORD pointedWeight = 0;
        const bool havePointedWeight =
            weightPointer != 0 && SafeReadDword(weightPointer + stackTerrain * 4, pointedWeight);

        DWORD cellPointedWeight = 0;
        const bool haveCellPointedWeight =
            weightPointer != 0 && cellTerrain != 0xFFFFFFFF &&
            SafeReadDword(weightPointer + cellTerrain * 4, cellPointedWeight);

        DWORD weights[12] = {};
        bool weightsReadable = weightPointer != 0;
        for (size_t i = 0; i < sizeof(weights) / sizeof(weights[0]); ++i)
        {
            if (!SafeReadDword(weightPointer + static_cast<DWORD>(i * sizeof(DWORD)), weights[i]))
                weightsReadable = false;
        }

        LogFormat(
            "Obstacle diagnostics: finalCandidateWeight hit=%ld record=0x%08lX template=0x%08lX "
            "type=%lu subtype=%ld firstTerrain=%ld row=0x%08lX rowId=%lu "
            "stackTerrain=%ld cellTerrain=%ld cell=0x%08lX cellXYZ=(%ld,%ld,%ld) "
            "candidateXYZ=(%ld,%ld,%ld) cmpValue=%ld weightPtr=0x%08lX "
            "pointedWeight=%s%ld cellPointedWeight=%s%ld weights=%s "
            "[%ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld]",
            hit,
            record,
            objectTemplate,
            objectType,
            static_cast<LONG>(objectSubtype),
            static_cast<LONG>(firstTerrain),
            row,
            rowId,
            static_cast<LONG>(stackTerrain),
            cellTerrain == 0xFFFFFFFF ? -1 : static_cast<LONG>(cellTerrain),
            cell,
            static_cast<LONG>(x),
            static_cast<LONG>(y),
            static_cast<LONG>(z),
            static_cast<LONG>(candidateX),
            static_cast<LONG>(candidateY),
            static_cast<LONG>(candidateZ),
            static_cast<LONG>(comparedValue),
            weightPointer,
            havePointedWeight ? "" : "unreadable:",
            havePointedWeight ? static_cast<LONG>(pointedWeight) : 0,
            haveCellPointedWeight ? "" : "unreadable:",
            haveCellPointedWeight ? static_cast<LONG>(cellPointedWeight) : 0,
            weightsReadable ? "ok" : "partial",
            static_cast<LONG>(weights[0]),
            static_cast<LONG>(weights[1]),
            static_cast<LONG>(weights[2]),
            static_cast<LONG>(weights[3]),
            static_cast<LONG>(weights[4]),
            static_cast<LONG>(weights[5]),
            static_cast<LONG>(weights[6]),
            static_cast<LONG>(weights[7]),
            static_cast<LONG>(weights[8]),
            static_cast<LONG>(weights[9]),
            static_cast<LONG>(weights[10]),
            static_cast<LONG>(weights[11]));
    }

    __declspec(naked) void HotaParserEntryHook()
    {
        __asm
        {
            pushfd
            pushad
            push    ecx
            call    LogParserEntryHit
            popad
            popfd

            push    ebp
            mov     ebp, esp
            and     esp, 0FFFFFFF8h
            jmp     dword ptr [s_parserEntryResume]
        }
    }

    __declspec(naked) void HotaParserStoreHook()
    {
        __asm
        {
            mov     eax, [esp+30h]
            mov     eax, [eax+4]
            mov     eax, [eax+ecx*4]
            mov     [ebx+10h], eax

            pushfd
            pushad
            push    ecx
            push    eax
            push    ebx
            call    LogParserStoreHit
            popad
            popfd

            jmp     dword ptr [s_parserStoreResume]
        }
    }

    __declspec(naked) void FinalCandidateRecordHook()
    {
        __asm
        {
            push    eax
            mov     eax, [eax+10h]
            cmp     eax, ebx
            pushfd
            pushad

            mov     ecx, [esp+36]
            mov     edx, [ebp-14h]
            push    dword ptr [ebp+10h]
            push    dword ptr [ebp+0Ch]
            push    dword ptr [ebp+08h]
            push    edx
            push    eax
            push    ecx
            mov     edx, [ebp-24h]
            push    edx
            call    LogFinalCandidateRecordHit

            popad
            popfd
            lea     esp, [esp+4]
            jmp     dword ptr [s_finalCandidateRecordResume]
        }
    }

    __declspec(naked) void FinalCandidateWeightHook()
    {
        __asm
        {
            mov     esi, [ebp-14h]
            cmp     dword ptr [eax+esi*4+4], 0FFFFEC78h

            pushfd
            pushad

            mov     ecx, [ebp-28h]
            mov     edx, [eax+esi*4+4]
            push    dword ptr [ebp+10h]
            push    dword ptr [ebp+0Ch]
            push    dword ptr [ebp+08h]
            push    edx
            push    esi
            push    eax
            push    ecx
            mov     edx, [ebp-24h]
            push    edx
            call    LogFinalCandidateWeightHit

            popad
            popfd
            jmp     dword ptr [s_finalCandidateWeightResume]
        }
    }

    __declspec(naked) void FinalCandidatePatchedWeightHook()
    {
        __asm
        {
            cmp     dword ptr [eax+esi*4+4], 0FFFFEC78h

            pushfd
            pushad

            mov     ecx, [ebp-28h]
            mov     edx, [eax+esi*4+4]
            push    dword ptr [ebp+10h]
            push    dword ptr [ebp+0Ch]
            push    dword ptr [ebp+08h]
            push    edx
            push    esi
            push    eax
            push    ecx
            mov     edx, [ebp-24h]
            push    edx
            call    LogFinalCandidateWeightHit

            popad
            popfd
            jmp     dword ptr [s_finalCandidateWeightResume]
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

        LogFormat("Obstacle diagnostics: hota_me.dll SHA-256: %s", moduleHash);
        if (strcmp(moduleHash, kExpectedHotaMeSha256) != 0)
        {
            error = "unsupported hota_me.dll hash";
            return false;
        }

        return true;
    }

    bool InstallHookOrError(
        const char* name,
        void* target,
        const BYTE* expectedBytes,
        SIZE_T expectedLength,
        void* hookFn,
        BYTE originalBytes[5],
        std::string& error)
    {
        const HookSpec spec =
        {
            target,
            expectedBytes,
            expectedLength,
        };

        const HookResult result = HookInstall(spec, hookFn, originalBytes);
        if (result == HookResult::Success)
            return true;

        error = std::string(name) + ": " + HookResultName(result);
        if (result == HookResult::UnexpectedBytes)
        {
            error += " expected=[" + FormatByteArray(expectedBytes, expectedLength) + "]";
            error += " actual=[" + FormatMemoryBytes(
                static_cast<DWORD>(reinterpret_cast<DWORD_PTR>(target)),
                expectedLength) + "]";
        }
        return false;
    }

    void TryInstallFinalCandidateRecordHooks()
    {
        LogCodeBytes(
            "final candidate selection window",
            kFinalCandidateByteWindow,
            kFinalCandidateByteWindowLength);
        LogCodeBytes(
            "candidate weight helper window",
            kCandidateWeightHelperByteWindow,
            kCandidateWeightHelperByteWindowLength);
        LogRelativeJumpTargetBytes(
            "candidate weight helper patch target",
            kCandidateWeightHelperPatchJump,
            kPatchTargetByteWindowLength);
        LogRelativeJumpTargetBytes(
            "final candidate weight patch target",
            kFinalCandidateWeightPatchJump,
            kPatchTargetByteWindowLength);
        {
            DWORD finalWeightPatchTarget = 0;
            if (TryReadRelativeJumpTarget(kFinalCandidateWeightPatchJump, finalWeightPatchTarget))
                LogFinalCandidatePatchHandlerBytes(finalWeightPatchTarget);
        }

        void* const finalCandidateRecordSite =
            reinterpret_cast<void*>(kFinalCandidateRecordSite);
        s_finalCandidateRecordResume =
            kFinalCandidateRecordSite + kFinalCandidateRecordStolenLength;

        std::string finalHookError;
        if (InstallHookOrError(
            "final candidate record[+0x10] load",
            finalCandidateRecordSite,
            kFinalCandidateRecordExpected,
            sizeof(kFinalCandidateRecordExpected),
            FinalCandidateRecordHook,
            s_finalCandidateRecordOriginal,
            finalHookError))
        {
            LogFormat("Obstacle diagnostics installed: finalCandidateRecord=%p",
                finalCandidateRecordSite);
        }
        else
        {
            LogFormat("Obstacle diagnostics: final candidate record hook not installed: %s",
                finalHookError.c_str());
        }

        void* const finalCandidateWeightSite =
            reinterpret_cast<void*>(kFinalCandidateWeightSite);
        s_finalCandidateWeightResume =
            kFinalCandidateWeightSite + kFinalCandidateWeightStolenLength;

        std::string weightHookError;
        if (InstallHookOrError(
            "final candidate terrain weight compare",
            finalCandidateWeightSite,
            kFinalCandidateWeightExpected,
            sizeof(kFinalCandidateWeightExpected),
            FinalCandidateWeightHook,
            s_finalCandidateWeightOriginal,
            weightHookError))
        {
            LogFormat("Obstacle diagnostics installed: finalCandidateWeight=%p",
                finalCandidateWeightSite);
            return;
        }

        LogFormat("Obstacle diagnostics: final candidate weight hook not installed: %s",
            weightHookError.c_str());

        DWORD patchTarget = 0;
        if (!TryReadRelativeJumpTarget(kFinalCandidateWeightPatchJump, patchTarget))
        {
            LogMessage("Obstacle diagnostics: final candidate generated weight hook not installed: patch jump not found.");
            return;
        }

        DWORD generatedCompare = 0;
        if (!FindPatternInMemory(
            patchTarget,
            kPatchTargetByteWindowLength,
            kFinalCandidatePatchedWeightExpected,
            sizeof(kFinalCandidatePatchedWeightExpected),
            generatedCompare))
        {
            LogMessage("Obstacle diagnostics: final candidate generated weight hook not installed: compare pattern not found.");
            return;
        }

        s_finalCandidateWeightResume =
            generatedCompare + sizeof(kFinalCandidatePatchedWeightExpected);

        std::string generatedHookError;
        if (InstallHookOrError(
            "final candidate generated terrain weight compare",
            reinterpret_cast<void*>(generatedCompare),
            kFinalCandidatePatchedWeightExpected,
            sizeof(kFinalCandidatePatchedWeightExpected),
            FinalCandidatePatchedWeightHook,
            s_finalCandidateWeightOriginal,
            generatedHookError))
        {
            LogFormat("Obstacle diagnostics installed: finalCandidateGeneratedWeight=%p",
                reinterpret_cast<void*>(generatedCompare));
            return;
        }

        LogFormat("Obstacle diagnostics: final candidate generated weight hook not installed: %s",
            generatedHookError.c_str());
    }
}

bool ObstacleDiagnosticsEnabled()
{
    char value[16] = {};
    const DWORD length = GetEnvironmentVariableA(kDiagnosticsEnvVar, value, sizeof(value));
    return length > 0 && length < sizeof(value) && EnvValueEnabled(value);
}

bool InstallObstacleDiagnosticsHooks(std::string& error)
{
    HMODULE hotaMe = GetModuleHandleA("hota_me.dll");
    if (!hotaMe)
    {
        error = "hota_me.dll is not loaded";
        return false;
    }

    if (!ValidateHotaMeModule(hotaMe, error))
        return false;

    BYTE* const moduleBase = reinterpret_cast<BYTE*>(hotaMe);
    void* const parserEntry = moduleBase + kHotaParserEntryRva;
    void* const parserStore = moduleBase + kHotaParserStoreRva;
    s_parserEntryResume = reinterpret_cast<DWORD>(parserEntry) + kParserEntryStolenLength;
    s_parserStoreResume = reinterpret_cast<DWORD>(parserStore) + kParserStoreStolenLength;

    if (!InstallHookOrError(
        "HotA rand_trn parser entry",
        parserEntry,
        kParserEntryExpected,
        sizeof(kParserEntryExpected),
        HotaParserEntryHook,
        s_parserEntryOriginal,
        error))
    {
        return false;
    }

    if (!InstallHookOrError(
        "HotA wrapper[+0x10] store",
        parserStore,
        kParserStoreExpected,
        sizeof(kParserStoreExpected),
        HotaParserStoreHook,
        s_parserStoreOriginal,
        error))
    {
        const HookResult rollback = HookRemove(parserEntry, s_parserEntryOriginal);
        if (rollback != HookResult::Success)
        {
            error += std::string("; rollback failed for HotA rand_trn parser entry: ") +
                HookResultName(rollback);
        }
        return false;
    }

    TryInstallFinalCandidateRecordHooks();

    LogFormat("Obstacle diagnostics installed: parserEntry=%p parserStore=%p",
        parserEntry, parserStore);
    return true;
}
