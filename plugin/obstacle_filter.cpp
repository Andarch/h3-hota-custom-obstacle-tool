#include "pch.h"
#include "obstacle_filter.h"

#include "hook.h"
#include "logger.h"
#include "obstacle_button_data.h"
#include "target_profile.h"

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <string>
#include <map>

namespace
{
    constexpr char kExtraBlockedObstacleIdsEnvVar[] =
        "HOTA_MAPED_PLUGIN_BLOCK_OBSTACLE_IDS";
    constexpr char kVariantTraceEnvVar[] =
        "HOTA_MAPED_PLUGIN_OBSTACLE_VARIANT_TRACE";
    constexpr DWORD kPredicateStolenLength = 6;
    constexpr DWORD kHotaParserStoreStolenLength = 13;
    constexpr DWORD kHotaParserWrapperFallbackStolenLength = 8;
    constexpr DWORD kObjectsTxtLoaderSite = 0x004909C6;
    constexpr DWORD kObjectsTxtLoaderStolenLength = 5;
    constexpr DWORD kObjectRecordAssignReturnSite = 0x0049061D;
    constexpr DWORD kObjectRecordAssignReturnStolenLength = 5;
    constexpr DWORD kFinalCandidateRecordSite = 0x0049E592;
    constexpr DWORD kFinalCandidateRecordStolenLength = 5;
    constexpr DWORD kFinalCandidateRejectTarget = 0x0049E69E;
    constexpr DWORD kTerrainCount = 12;
    constexpr DWORD kNativeTextLoader = 0x004DCCE0;

    const BYTE kPredicateExpected[] =
    {
        0x8B, 0x41, 0x04,       // mov eax, [ecx+4]
        0x56,                   // push esi
        0x8B, 0x30,             // mov esi, [eax]
    };

    const BYTE kObjectsTxtLoaderExpected[] =
    {
        0xB8, 0x17, 0x9B, 0x52, 0x00, // mov eax, offset loc_529B17
    };

    const BYTE kHotaParserStoreExpected[] =
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

    const BYTE kObjectRecordAssignReturnExpected[] =
    {
        0x8B, 0xF0,             // mov esi, eax
        0x8D, 0x4D, 0xCC,       // lea ecx, [ebp-34h]
    };

    BYTE s_predicateOriginal[5] = {};
    BYTE s_objectsTxtLoaderOriginal[5] = {};
    BYTE s_hotaParserStoreOriginal[5] = {};
    BYTE s_hotaParserWrapperFallbackOriginal[5] = {};
    BYTE s_objectRecordAssignReturnOriginal[5] = {};
    BYTE s_finalCandidateRecordOriginal[5] = {};
    const HotaModuleTargetProfile* s_hotaProfile = nullptr;
    BYTE s_extraBlockedRandTrnRows[kMaxRandTrnRowCount] = {};
    DWORD s_predicateResume = 0;
    DWORD s_objectsTxtLoaderResume = 0;
    DWORD s_hotaParserStoreResume = 0;
    DWORD s_hotaParserWrapperFallbackResume = 0;
    DWORD s_objectRecordAssignReturnResume = 0;
    DWORD s_finalCandidateRecordResume = 0;
    DWORD s_finalCandidateReject = kFinalCandidateRejectTarget;
    DWORD s_extraBlockedRowCount = 0;
    bool s_variantTraceEnabled = false;
    bool s_objectsOverlayReady = false;
    bool s_objectsOverlayAttempted = false;
    char s_objectsOverlayPath[MAX_PATH] = {};
    char s_objectsOverlayResourceName[MAX_PATH] = {};
    volatile LONG s_rejectedRows = 0;
    volatile LONG s_observedCandidates = 0;
    volatile LONG s_rejectedCandidates = 0;
    volatile LONG s_generationFilterMode = 0;
    volatile LONG s_customObstacleMode = 0;
    volatile LONG s_allObstacleTerrains = 0;
    bool s_generationAllTerrains = false;
    bool s_allTerrainTemplateFailure = false;
    volatile LONG s_selectedButtonMaskLow = 0;
    volatile LONG s_selectedButtonMaskHigh = 0;
    volatile LONG s_selectedButtonMaskExtended = 0;
    volatile LONG s_selectedButtonMaskUpper = 0;
    static_assert(kObstacleButtonCount <= 128, "Button selection masks need more capacity");
    volatile LONG s_buttonBorrowAssignments = 0;

    struct VariantTraceRowStats
    {
        LONG finalSeen;
        LONG finalAllowed;
        LONG finalRejected;
    };

    VariantTraceRowStats s_variantTraceRows[kMaxRandTrnRowCount] = {};
    DWORD s_traceCandidateRow = 0;
    bool s_duneFillActive = false;
    DWORD s_duneFillContinue = 0;
    DWORD s_generateObstacleCell = 0;
    DWORD s_obstacleCellEligible = 0;
    BYTE s_duneFillOriginal[5] = {};
    BYTE s_duneCaptureOriginal[5] = {};
    DWORD s_duneCaptureContinue = 0;
    DWORD s_duneCapturedGenerator = 0;
    BYTE s_dunePaintedCells[252 * 252 * 2] = {};
    int s_traceCandidateButton = -1;
    DWORD s_candidateTerrainMask = 0;
    DWORD s_candidateObjectTemplate = 0;
    DWORD s_mountainOrderContinue = 0, s_mountainOrderReject = 0;
    DWORD s_mountainOrderExceptions = 0;
    BYTE s_mountainOrderOriginal[5] = {};
    DWORD s_accumulatorResume = 0;
    DWORD s_terrainViewTableAddress = 0;
    BYTE s_accumulatorOriginal[5] = {};
    DWORD s_accumulatorRelaxations = 0;
    DWORD s_edgeRelaxations = 0;
    DWORD s_tracePosition[3] = {};
    DWORD s_terrainPredicateCounts[kMaxRandTrnRowCount][kTerrainCount][2] = {};
    std::map<unsigned long long, bool> s_terrainPredicateSamples;

    bool TryReadCandidateRecordRowId(DWORD record, DWORD& rowId);
    void ClearVariantTraceRows();
    void DumpVariantTraceRows(ObstacleGenerationFilterMode mode);

    bool EnvValueEnabled(const char* value)
    {
        return _stricmp(value, "1") == 0 ||
            _stricmp(value, "true") == 0 ||
            _stricmp(value, "yes") == 0 ||
            _stricmp(value, "on") == 0;
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

    std::string FormatBytes(const void* address, SIZE_T length)
    {
        if (!address || length == 0)
            return "";

        std::string result;
        result.reserve(length * 3);

        const BYTE* const bytes = static_cast<const BYTE*>(address);
        for (SIZE_T index = 0; index < length; ++index)
        {
            BYTE value = 0;
            if (!SafeReadByte(reinterpret_cast<DWORD>(bytes + index), value))
                return result + "<unreadable>";

            char fragment[4] = {};
            sprintf_s(fragment, "%02X", value);
            if (!result.empty())
                result += ' ';
            result += fragment;
        }

        return result;
    }

    bool SafeWriteDword(DWORD address, DWORD value)
    {
        __try
        {
            *reinterpret_cast<DWORD*>(address) = value;
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return false;
        }
    }

    const char* FilterModeName(ObstacleGenerationFilterMode mode)
    {
        if (mode == ObstacleGenerationFilterMode::ButtonSelection)
            return "button-selection";
        return "none";
    }

    bool IsObstacleButtonSelected(int buttonIndex)
    {
        if (buttonIndex < 0 || buttonIndex >= kObstacleButtonCount)
            return false;
        const DWORD mask = static_cast<DWORD>(InterlockedCompareExchange(
            buttonIndex < 32 ? &s_selectedButtonMaskLow :
                buttonIndex < 64 ? &s_selectedButtonMaskHigh :
                buttonIndex < 96 ? &s_selectedButtonMaskExtended : &s_selectedButtonMaskUpper,
            0,
            0));
        return (mask & (1u << (buttonIndex & 31))) != 0;
    }

    bool ReadWholeFile(const char* path, std::string& contents)
    {
        contents.clear();

        FILE* file = nullptr;
        if (fopen_s(&file, path, "rb") != 0 || !file)
            return false;

        fseek(file, 0, SEEK_END);
        const long size = ftell(file);
        if (size < 0)
        {
            fclose(file);
            return false;
        }

        fseek(file, 0, SEEK_SET);
        contents.resize(static_cast<size_t>(size));
        if (size > 0 &&
            fread(&contents[0], 1, static_cast<size_t>(size), file) !=
                static_cast<size_t>(size))
        {
            fclose(file);
            contents.clear();
            return false;
        }

        fclose(file);
        return true;
    }

    DWORD LoadNativeText(const char* name)
    {
        __try { return reinterpret_cast<DWORD (__cdecl*)(const char*)>(kNativeTextLoader)(name); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
    }

    void ReleaseNativeText(DWORD resource)
    {
        __try
        {
            const DWORD vtable = *reinterpret_cast<DWORD*>(resource);
            reinterpret_cast<void (__thiscall*)(DWORD)>(
                *reinterpret_cast<DWORD*>(vtable + 4))(resource);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
    }

    bool CopyNativeTextCell(DWORD address, char* buffer, size_t capacity)
    {
        __try
        {
            if (!address) { buffer[0] = 0; return true; }
            for (size_t i = 0; i < capacity; ++i)
            {
                buffer[i] = reinterpret_cast<const char*>(address)[i];
                if (!buffer[i]) return true;
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
        return false;
    }

    bool ReadNativeObjectsText(std::string& contents)
    {
        // Use the editor's text resource loader so loose files and encrypted LODs
        // follow the same precedence and decoding as native obstacle generation.
        const DWORD resource = LoadNativeText("objects.txt");
        if (!resource) return false;
        DWORD begin = 0, end = 0;
        bool ok = SafeReadDword(resource + 32, begin) &&
            SafeReadDword(resource + 36, end) && begin && end >= begin &&
            (end - begin) % 4 == 0 && (end - begin) / 4 <= 20000;
        contents.clear();
        for (DWORD p = begin; ok && p < end; p += 4)
        {
            DWORD row = 0, cells = 0, cellsEnd = 0;
            ok = SafeReadDword(p, row) && row &&
                SafeReadDword(row + 4, cells) && SafeReadDword(row + 8, cellsEnd) &&
                cellsEnd >= cells && (cellsEnd - cells) % 4 == 0 &&
                (cellsEnd - cells) / 4 <= 32;
            for (DWORD c = cells; ok && c < cellsEnd; c += 4)
            {
                DWORD cell = 0;
                char text[1024] = {};
                ok = SafeReadDword(c, cell) && CopyNativeTextCell(cell, text, sizeof(text));
                if (ok)
                {
                    if (c != cells) contents += '\t';
                    contents += text;
                }
            }
            contents += "\r\n";
        }
        ReleaseNativeText(resource);
        if (!ok) contents.clear();
        return ok;
    }

    bool WriteWholeFile(const char* path, const std::string& contents)
    {
        FILE* file = nullptr;
        if (fopen_s(&file, path, "wb") != 0 || !file)
            return false;

        const bool ok =
            contents.empty() ||
            fwrite(contents.data(), 1, contents.size(), file) == contents.size();
        fclose(file);
        return ok;
    }

    bool BuildDataFilePath(const char* fileName, char path[MAX_PATH])
    {
        char exePath[MAX_PATH] = {};
        const DWORD length = GetModuleFileNameA(nullptr, exePath, MAX_PATH);
        if (length == 0 || length >= MAX_PATH)
            return false;

        char* slash = strrchr(exePath, '\\');
        if (!slash)
            slash = strrchr(exePath, '/');
        if (!slash)
            return false;

        *slash = '\0';
        return sprintf_s(path, MAX_PATH, "%s\\Data\\%s", exePath, fileName) > 0;
    }

    bool ParseObjectsRowCount(const std::string& contents, size_t& lineEnd, DWORD& rowCount)
    {
        lineEnd = contents.find('\n');
        if (lineEnd == std::string::npos)
            return false;

        size_t start = 0;
        if (contents.size() >= 3 &&
            static_cast<unsigned char>(contents[0]) == 0xEF &&
            static_cast<unsigned char>(contents[1]) == 0xBB &&
            static_cast<unsigned char>(contents[2]) == 0xBF)
        {
            start = 3;
        }

        std::string firstLine = contents.substr(start, lineEnd - start);
        if (!firstLine.empty() && firstLine[firstLine.size() - 1] == '\r')
            firstLine.resize(firstLine.size() - 1);

        char* end = nullptr;
        const unsigned long parsed = std::strtoul(firstLine.c_str(), &end, 10);
        if (end == firstLine.c_str() || *end != '\0')
            return false;

        rowCount = static_cast<DWORD>(parsed);
        return true;
    }

    const ObstacleTemplateOverride* SelectedTemplateOverrideForLine(
        const std::string& line,
        DWORD& overrideIndex)
    {
        const size_t tokenEnd = line.find_first_of(" \t\r");
        const std::string defName = line.substr(0, tokenEnd);
        for (DWORD index = 0; index < ObstacleTemplateOverrideCount(); ++index)
        {
            const ObstacleTemplateOverride* item = ObstacleTemplateOverrideAt(index);
            if (item &&
                IsObstacleButtonSelected(item->buttonIndex) &&
                _stricmp(defName.c_str(), item->defName) == 0)
            {
                overrideIndex = index;
                return item;
            }
        }
        return nullptr;
    }

    bool BuildButtonSelectionOverlay(
        const std::string& source,
        size_t firstLineEnd,
        DWORD baseRowCount,
        std::string& overlay,
        DWORD& resultingRowCount,
        DWORD& replacedRows,
        DWORD& appendedRows)
    {
        constexpr DWORD kMaximumTemplateOverrides = 128;
        const DWORD overrideCount = ObstacleTemplateOverrideCount();
        if (overrideCount > kMaximumTemplateOverrides)
            return false;

        bool used[kMaximumTemplateOverrides] = {};
        replacedRows = 0;
        appendedRows = 0;
        overlay.clear();
        overlay.reserve(source.size() + overrideCount * 256);

        const size_t bodyStart = firstLineEnd + 1;
        size_t position = bodyStart;
        while (position < source.size())
        {
            const size_t newline = source.find('\n', position);
            const size_t lineEnd = newline == std::string::npos ? source.size() : newline;
            size_t textEnd = lineEnd;
            if (textEnd > position && source[textEnd - 1] == '\r')
                --textEnd;
            const std::string line = source.substr(position, textEnd - position);

            DWORD overrideIndex = 0;
            const ObstacleTemplateOverride* replacement =
                line.empty() ? nullptr : SelectedTemplateOverrideForLine(line, overrideIndex);
            if (replacement)
            {
                overlay += replacement->row;
                overlay += "\r\n";
                used[overrideIndex] = true;
                ++replacedRows;
            }
            else
            {
                overlay.append(source, position, lineEnd - position);
                if (newline != std::string::npos)
                    overlay += '\n';
            }

            if (newline == std::string::npos)
                break;
            position = newline + 1;
        }

        if (!overlay.empty() && overlay[overlay.size() - 1] != '\n')
            overlay += "\r\n";
        for (DWORD index = 0; index < overrideCount; ++index)
        {
            const ObstacleTemplateOverride* item = ObstacleTemplateOverrideAt(index);
            if (!used[index] && item && IsObstacleButtonSelected(item->buttonIndex))
            {
                overlay += item->row;
                overlay += "\r\n";
                ++appendedRows;
            }
        }

        resultingRowCount = baseRowCount + appendedRows;
        char countText[32] = {};
        sprintf_s(countText, "%lu", resultingRowCount);
        const std::string headerSuffix = source.substr(firstLineEnd, bodyStart - firstLineEnd);
        overlay.insert(0, headerSuffix);
        overlay.insert(0, countText);
        return true;
    }

    bool ButtonSelectionNeedsObjectsOverlay()
    {
        for (DWORD index = 0; index < ObstacleTemplateOverrideCount(); ++index)
        {
            const ObstacleTemplateOverride* item = ObstacleTemplateOverrideAt(index);
            if (item && IsObstacleButtonSelected(item->buttonIndex))
                return true;
        }
        return false;
    }

    bool EnsureObjectsOverlayFile(ObstacleGenerationFilterMode mode)
    {
        if (mode != ObstacleGenerationFilterMode::ButtonSelection) return false;
        if (s_objectsOverlayReady)
            return true;
        if (s_objectsOverlayAttempted)
            return false;

        s_objectsOverlayAttempted = true;

        char sourcePath[MAX_PATH] = {};
        if (!BuildDataFilePath("objects.txt", sourcePath))
        {
            LogMessage("Obstacle variants: could not resolve Data\\objects.txt path for object overlay.");
            return false;
        }

        std::string source;
        if (!ReadWholeFile(sourcePath, source) && !ReadNativeObjectsText(source))
        {
            LogFormat("Obstacle variants: could not read %s for object overlay.", sourcePath);
            return false;
        }

        size_t firstLineEnd = 0;
        DWORD rowCount = 0;
        if (!ParseObjectsRowCount(source, firstLineEnd, rowCount))
        {
            LogFormat("Obstacle variants: could not parse row count in %s.", sourcePath);
            return false;
        }

        if (sprintf_s(
                s_objectsOverlayResourceName,
                MAX_PATH,
                "HotaMapEdPlugin_objects_%lu.txt",
                GetCurrentProcessId()) <= 0)
        {
            LogMessage("Obstacle variants: could not format object overlay resource name.");
            return false;
        }

        if (!BuildDataFilePath(s_objectsOverlayResourceName, s_objectsOverlayPath))
        {
            LogMessage("Obstacle variants: could not resolve Data path for object overlay.");
            return false;
        }

        std::string overlay;
        DWORD extraRows = 0;
        DWORD replacedRows = 0;
        DWORD resultingRowCount = rowCount;
        if (!BuildButtonSelectionOverlay(source, firstLineEnd, rowCount, overlay,
                resultingRowCount, replacedRows, extraRows))
        {
            LogMessage("Obstacle variants: could not build button-selection object overlay.");
            return false;
        }

        if (!WriteWholeFile(s_objectsOverlayPath, overlay))
        {
            LogFormat("Obstacle variants: could not write object overlay %s.",
                s_objectsOverlayPath);
            s_objectsOverlayPath[0] = '\0';
            return false;
        }

        s_objectsOverlayReady = true;
        LogFormat(
            "Obstacle variants: object overlay ready mode=%ld resource=%s path=%s baseRows=%lu rows=%lu replacedRows=%lu appendedRows=%lu",
            static_cast<LONG>(mode),
            s_objectsOverlayResourceName,
            s_objectsOverlayPath,
            rowCount,
            resultingRowCount,
            replacedRows,
            extraRows);
        return true;
    }

    void CleanupObjectsOverlayFile()
    {
        if (!s_objectsOverlayAttempted)
            return;

        if (s_objectsOverlayPath[0] && DeleteFileA(s_objectsOverlayPath))
        {
            LogFormat("Obstacle variants: deleted object overlay path=%s",
                s_objectsOverlayPath);
        }
        else if (s_objectsOverlayReady && s_objectsOverlayPath[0])
        {
            LogFormat("Obstacle variants: could not delete object overlay path=%s error=%lu",
                s_objectsOverlayPath,
                GetLastError());
        }

        s_objectsOverlayReady = false;
        s_objectsOverlayAttempted = false;
        s_objectsOverlayPath[0] = '\0';
        s_objectsOverlayResourceName[0] = '\0';
    }

    const char* __stdcall MaybeRedirectObjectsTxtSource(const char* source)
    {
        if (!source || _stricmp(source, "objects.txt") != 0)
            return source;

        const auto mode = static_cast<ObstacleGenerationFilterMode>(
            InterlockedCompareExchange(&s_generationFilterMode, 0, 0));
        if (mode == ObstacleGenerationFilterMode::ButtonSelection)
        {
            if (!ButtonSelectionNeedsObjectsOverlay())
                return source;
            return EnsureObjectsOverlayFile(mode) ? s_objectsOverlayResourceName : source;
        }

        return source;
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

            if (obstacleId == 0 || obstacleId > s_hotaProfile->randTrnRowCount)
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
        if (!CustomObstacleModeEnabled()) return true;
        DWORD rowId = 0;
        if (!SafeReadDword(row, rowId))
            return true;

        if (rowId >= s_hotaProfile->randTrnRowCount)
            return true;

        const bool extraBlocked = s_extraBlockedRandTrnRows[rowId] != 0;

        if (extraBlocked)
        {
            const LONG hit = InterlockedIncrement(&s_rejectedRows);
            if (hit <= 32)
            {
                LogFormat("Obstacle filter: rejected rand_trn rowId=%lu obstacleId=%lu reason=%s",
                    rowId,
                    rowId + 1,
                    "extra-block-list");
            }
            else if (hit == 33)
            {
                LogMessage("Obstacle filter: further rejection logs suppressed.");
            }
        }

        return !extraBlocked;
    }

    struct SelectedObjectRules
    {
        DWORD row[19] = {};
        DWORD weightPointer = 0;
        int weights[kTerrainCount] = {};
        int adjacent[kMaxRandTrnRowCount] = {};
        int overlap[kMaxRandTrnRowCount] = {};
    };

    std::map<DWORD, SelectedObjectRules> s_selectedObjectRules;
    DWORD s_defaultSelectedRow = 0;
    bool s_selectedRulesFailureLogged = false;

    bool BuildSelectedObjectRules(DWORD source, DWORD mask, SelectedObjectRules& rules, bool allTerrains)
    {
        DWORD holder = 0, weights = 0;
        if (!source || !SafeReadDword(source + 4, holder) ||
            !SafeReadDword(holder, weights)) return false;
        for (DWORD i = 0; i < 19; ++i)
            if (!SafeReadDword(source + i * 4, rules.row[i])) return false;
        if (!s_hotaProfile || rules.row[0] >= s_hotaProfile->randTrnRowCount)
            return false;
        bool expanded = false;
        for (DWORD terrain = 0; terrain < kTerrainCount; ++terrain)
        {
            DWORD value = 0;
            if (!SafeReadDword(weights + terrain * 4, value)) return false;
            const int nativeWeight = static_cast<int>(value);
            const bool allowed = terrain != 9 && (mask & (1u << terrain)) != 0;
            // The native scorer requires a positive actual-terrain weight.
            // Zero and soft negative weights can still prevent placement even
            // after the template mask permits the terrain (Grass Mountains/Dirt).
            const bool promoteWeight = nativeWeight <= -4000 || (allTerrains && nativeWeight <= 0);
            expanded |= allowed && promoteWeight;
            rules.weights[terrain] = allowed ? (promoteWeight ? 5 : nativeWeight) : -5000;
        }
        const DWORD count = s_hotaProfile->randTrnRowCount;
        if (rules.row[13] < rules.row[12] || rules.row[17] < rules.row[16] ||
            (rules.row[13] - rules.row[12]) / 4 != count ||
            (rules.row[17] - rules.row[16]) / 4 != count) return false;
        for (DWORD i = 0; i < count; ++i)
        {
            DWORD adjacent = 0, overlap = 0;
            if (!SafeReadDword(rules.row[12] + i * 4, adjacent) ||
                !SafeReadDword(rules.row[16] + i * 4, overlap)) return false;
            rules.adjacent[i] = static_cast<int>(adjacent);
            // Expanded terrain support must not retain donor cross-terrain
            // adjacency bans. Keep overlap restrictions and native footprints.
            if (expanded && rules.adjacent[i] <= -4000) rules.adjacent[i] = 5;
            rules.overlap[i] = static_cast<int>(overlap);
        }
        rules.weightPointer = reinterpret_cast<DWORD>(rules.weights);
        rules.row[1] = reinterpret_cast<DWORD>(&rules.weightPointer);
        rules.row[12] = reinterpret_cast<DWORD>(rules.adjacent);
        rules.row[13] = rules.row[14] = rules.row[12] + count * 4;
        rules.row[16] = reinterpret_cast<DWORD>(rules.overlap);
        rules.row[17] = rules.row[18] = rules.row[16] + count * 4;
        return true;
    }

    bool IsSandDuneTemplate(DWORD objectTemplate)
    {
        DWORD type = 0, subtype = 0;
        return objectTemplate && SafeReadDword(objectTemplate + 28, type) && type == 140 &&
            SafeReadDword(objectTemplate + 32, subtype) && subtype == 6;
    }

    bool IsSnowHillTemplate(DWORD objectTemplate)
    {
        DWORD type = 0, subtype = 0;
        return objectTemplate && SafeReadDword(objectTemplate + 28, type) && type == 140 &&
            SafeReadDword(objectTemplate + 32, subtype) && subtype == 5;
    }

    bool IsLakeOrCraterTemplate(DWORD objectTemplate)
    {
        DWORD type = 0, subtype = 0;
        if (!objectTemplate || !SafeReadDword(objectTemplate + 28, type) ||
            !SafeReadDword(objectTemplate + 32, subtype)) return false;
        return (subtype == 0 && (type == 118 || type == 121 || type == 126 || type == 149 || type == 154)) ||
            (type == 140 && subtype == 8);
    }

    bool IsDuneFillerTemplate(DWORD objectTemplate)
    {
        DWORD type = 0, subtype = 0;
        return objectTemplate && SafeReadDword(objectTemplate + 28, type) && type == 148 &&
            SafeReadDword(objectTemplate + 32, subtype) && subtype == 0;
    }

    bool SandDunesSelected()
    {
        for (int i = 0; i < kObstacleButtonCount; ++i)
            if (strcmp(ObstacleButtonName(i), "Sand Dunes") == 0)
                return IsObstacleButtonSelected(i);
        return false;
    }

    bool IsRequestedDuneFiller(DWORD objectTemplate)
    {
        return SandDunesSelected() && IsDuneFillerTemplate(objectTemplate);
    }

    bool DuneFillTerrainAllowed(DWORD terrain)
    {
        terrain &= 63;
        return terrain < kTerrainCount && terrain != 9 &&
            (s_generationAllTerrains || terrain == 1);
    }

    bool ApplySelectedObjectRules(DWORD record, DWORD objectTemplate, int buttonIndex)
    {
        bool custom = false;
        for (DWORD i = 0; i < ObstacleTemplateOverrideCount(); ++i)
        {
            const auto* item = ObstacleTemplateOverrideAt(i);
            if (item && item->buttonIndex == buttonIndex) { custom = true; break; }
        }
        DWORD native = 0;
        if (!SafeReadDword(record + 0x10, native)) return false;
        // A workbook-mapped native object can have no RandTrn subtype row
        // (for example Maple Trees, 140/12). Repair only missing rows here.
        bool restrictedOverlap = IsLakeOrCraterTemplate(objectTemplate);
        if (restrictedOverlap && native)
        {
            DWORD rowId = 0, overlaps = 0, weight = 0;
            if (!SafeReadDword(native, rowId) || rowId >= kMaxRandTrnRowCount ||
                !SafeReadDword(native + 64, overlaps) || !overlaps ||
                !SafeReadDword(overlaps + rowId * 4, weight)) return false;
            restrictedOverlap = static_cast<int>(weight) <= -5000;
        }
        const bool selfOverlap = IsSandDuneTemplate(objectTemplate) ||
            IsSnowHillTemplate(objectTemplate) || restrictedOverlap;
        if (!s_generationAllTerrains && !custom && native && !selfOverlap) return true;
        auto found = s_selectedObjectRules.find(objectTemplate);
        if (found == s_selectedObjectRules.end())
        {
            DWORD source = ObstacleBorrowedRowForObjectTemplate(objectTemplate);
            DWORD mask = 0;
            if (!SafeReadDword(objectTemplate + 0x18, mask)) return false;
            if (!source) source = native ? native : s_defaultSelectedRow;
            const auto* button = ObstacleButtonDefinitionAt(buttonIndex);
            if (!button) return false;
            auto inserted = s_selectedObjectRules.emplace(objectTemplate, SelectedObjectRules{});
            found = inserted.first;
            if (!BuildSelectedObjectRules(source, s_generationAllTerrains ? kAllObstacleTerrainMask : mask & button->terrainMask, found->second, s_generationAllTerrains))
            {
                s_selectedObjectRules.erase(found);
                if (!s_selectedRulesFailureLogged)
                {
                    LogMessage("Obstacle variants: selected-object scoring rules unavailable; rejecting candidate.");
                    s_selectedRulesFailureLogged = true;
                }
                return false;
            }
            // Dunes, Snow Hills, lakes and craters can forbid self-overlap. Change only
            // a private row, using the same self-overlap weight as mountains.
            if (selfOverlap && (!restrictedOverlap || found->second.overlap[found->second.row[0]] <= -5000))
                found->second.overlap[found->second.row[0]] = 30;
            if (s_selectedObjectRules.size() <= 12)
                LogFormat("Obstacle variants: private selected-object rules button=%s mask=%03lX sourceRow=%lu fallback=%d",
                    button->name, s_generationAllTerrains ? kAllObstacleTerrainMask : mask & button->terrainMask, found->second.row[0], !native && !ObstacleBorrowedRowForObjectTemplate(objectTemplate));
        }
        return SafeWriteDword(record + 0x10, reinterpret_cast<DWORD>(found->second.row));
    }

    bool __stdcall ShouldAllowFinalCandidateRecord(DWORD record)
    {
        const auto mode = static_cast<ObstacleGenerationFilterMode>(
            InterlockedCompareExchange(&s_generationFilterMode, 0, 0));
        if (mode == ObstacleGenerationFilterMode::None)
            return true;
        if (s_generationAllTerrains && s_allTerrainTemplateFailure) return false;

        DWORD objectTemplate = 0;
        if (!SafeReadDword(record, objectTemplate))
        {
            const LONG hit = InterlockedIncrement(&s_rejectedCandidates);
            if (hit <= 32)
            {
                LogFormat("Obstacle variants: rejected unreadable final candidate record=%p reason=%s",
                    reinterpret_cast<void*>(record),
                    FilterModeName(mode));
            }
            else if (hit == 33)
            {
                LogMessage("Obstacle variants: further candidate rejection logs suppressed.");
            }
            return false;
        }

        const int buttonIndex = ObstacleButtonIndexForObjectTemplate(objectTemplate);
        const bool allowed = s_duneFillActive
            ? IsRequestedDuneFiller(objectTemplate) &&
                (!s_generationAllTerrains || ApplySelectedObjectRules(record, objectTemplate, buttonIndex))
            : !(SandDunesSelected() && IsDuneFillerTemplate(objectTemplate)) &&
                IsObstacleButtonSelected(buttonIndex) &&
                ApplySelectedObjectRules(record, objectTemplate, buttonIndex);

        DWORD rowId = 0;
        const bool haveRowId = TryReadCandidateRecordRowId(record, rowId);
        s_traceCandidateRow = 0;
        s_traceCandidateButton = buttonIndex;
        s_candidateTerrainMask = 0;
        s_candidateObjectTemplate = 0;
        if (allowed && mode == ObstacleGenerationFilterMode::ButtonSelection)
        {
            s_candidateObjectTemplate = objectTemplate;
            SafeReadDword(record + 0x10, s_traceCandidateRow);
            const auto* button = ObstacleButtonDefinitionAt(buttonIndex);
            DWORD templateMask = 0;
            if (button && SafeReadDword(objectTemplate + 0x18, templateMask))
                s_candidateTerrainMask = s_generationAllTerrains ? kAllObstacleTerrainMask : templateMask & button->terrainMask;
        }
        if (s_variantTraceEnabled && haveRowId)
        {
            InterlockedIncrement(&s_variantTraceRows[rowId].finalSeen);
            InterlockedIncrement(
                allowed
                    ? &s_variantTraceRows[rowId].finalAllowed
                    : &s_variantTraceRows[rowId].finalRejected);
        }

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
                "Obstacle variants: observed final candidate record=%p template=%p defKey=%lu objectId=%lu subtype=%lu button=%d mode=%s",
                reinterpret_cast<void*>(record),
                reinterpret_cast<void*>(objectTemplate),
                defKey,
                objectId,
                subtype,
                buttonIndex,
                FilterModeName(mode));
        }

        if (!allowed)
        {
            const LONG hit = InterlockedIncrement(&s_rejectedCandidates);
            if (s_variantTraceEnabled && hit <= 32)
            {
                LogFormat(
                    "Obstacle variants: rejected final candidate record=%p template=%p defKey=%lu objectId=%lu subtype=%lu button=%d reason=%s",
                    reinterpret_cast<void*>(record),
                    reinterpret_cast<void*>(objectTemplate),
                    defKey,
                    objectId,
                    subtype,
                    buttonIndex,
                    FilterModeName(mode));
            }
            else if (s_variantTraceEnabled && hit == 33)
            {
                LogMessage("Obstacle variants: further candidate rejection logs suppressed.");
            }
        }
        return allowed;
    }

    void __stdcall ObserveHotaParserWrapperRow(DWORD wrapper, DWORD rowRecord)
    {
        if (rowRecord == 0)
            return;

        const auto mode = static_cast<ObstacleGenerationFilterMode>(
            InterlockedCompareExchange(&s_generationFilterMode, 0, 0));

        if (mode == ObstacleGenerationFilterMode::ButtonSelection)
        {
            DWORD rowId = 0;
            if (SafeReadDword(rowRecord, rowId) && rowId == 9)
                s_defaultSelectedRow = rowRecord; // Native dirt-rock scoring fallback.
            DWORD objectTemplate = 0;
            if (SafeReadDword(wrapper, objectTemplate) && objectTemplate)
                ObserveObstacleBorrowSourceRow(objectTemplate, rowRecord);
        }

    }

    bool ExpandSelectedTemplateTerrains(DWORD objectTemplate)
    {
        if (!s_generationAllTerrains ||
            s_generationFilterMode != static_cast<LONG>(ObstacleGenerationFilterMode::ButtonSelection)) return true;
        if (!IsObstacleButtonSelected(ObstacleButtonIndexForObjectTemplate(objectTemplate)) &&
            !IsRequestedDuneFiller(objectTemplate)) return true;
        // The generator loads its own template records; never change editor/map caches.
        // Keep wrapper firstTerrain intact: native RandTrn matching has already finished.
        return SafeWriteDword(objectTemplate + 0x14, kAllObstacleTerrainMask) &&
            SafeWriteDword(objectTemplate + 0x18, kAllObstacleTerrainMask);
    }

    void __stdcall ApplyExtraObjectRandTrnFallback(DWORD wrapper)
    {
        const auto mode = static_cast<ObstacleGenerationFilterMode>(
            InterlockedCompareExchange(&s_generationFilterMode, 0, 0));

        if (mode == ObstacleGenerationFilterMode::ButtonSelection)
        {
            DWORD objectTemplate = 0;
            if (!SafeReadDword(wrapper, objectTemplate) || !objectTemplate)
                return;
            const DWORD sourceRowRecord =
                ObstacleBorrowedRowForObjectTemplate(objectTemplate);
            if (!ExpandSelectedTemplateTerrains(objectTemplate))
            {
                s_allTerrainTemplateFailure = true;
                LogMessage("Allow all terrains: could not expand generation template masks; placement will be rejected.");
                return;
            }
            DWORD existingRow = 0;
            if (sourceRowRecord && SafeReadDword(wrapper + 0x10, existingRow) &&
                existingRow != sourceRowRecord &&
                SafeWriteDword(wrapper + 0x10, sourceRowRecord))
            {
                const LONG hit = InterlockedIncrement(&s_buttonBorrowAssignments);
                if (s_variantTraceEnabled && hit <= 32)
                {
                    LogFormat(
                        "Obstacle variants: assigned explicit borrowed rand_trn row wrapper=%p template=%p row=%p",
                        reinterpret_cast<void*>(wrapper),
                        reinterpret_cast<void*>(objectTemplate),
                        reinterpret_cast<void*>(sourceRowRecord));
                }
            }
            return;
        }

    }

    __declspec(naked) bool __fastcall NativeTerrainPredicate(DWORD, DWORD, DWORD)
    {
        __asm
        {
            mov eax, [ecx+4]
            push esi
            mov esi, [eax]
            jmp dword ptr [s_predicateResume]
        }
    }

    bool __stdcall AllowMountainOrder(DWORD candidateTemplate, DWORD row, DWORD existing)
    {
        if (s_generationFilterMode != static_cast<LONG>(ObstacleGenerationFilterMode::ButtonSelection) ||
            !candidateTemplate || candidateTemplate != s_candidateObjectTemplate ||
            !row || row != s_traceCandidateRow || !existing ||
            !IsObstacleButtonSelected(s_traceCandidateButton)) return false;
        DWORD type = 0, wrapper = 0, otherTemplate = 0, otherType = 0;
        DWORD otherRow = 0, rowId = 0, overlaps = 0, weight = 0, subtype = 0, otherSubtype = 0;
        if (!SafeReadDword(candidateTemplate + 28, type) ||
            !SafeReadDword(candidateTemplate + 32, subtype) ||
            (type != 134 && (type != 140 || (subtype != 14 && subtype != 3))) ||
            !SafeReadDword(existing + 4, wrapper) || !wrapper ||
            !SafeReadDword(wrapper, otherTemplate) || !otherTemplate ||
            !SafeReadDword(otherTemplate + 28, otherType)) return false;
        if (type == 140)
        {
            // Glaciers and Ice Arches have the mountain order issue with native type 140.
            // Limit the exception to same-subtype pairs from the selected button.
            if (otherType != 140 || !SafeReadDword(otherTemplate + 32, otherSubtype) ||
                otherSubtype != subtype ||
                ObstacleButtonIndexForObjectTemplate(candidateTemplate) != s_traceCandidateButton ||
                ObstacleButtonIndexForObjectTemplate(otherTemplate) != s_traceCandidateButton)
                return false;
        }
        // Highlands Mountains includes native waterfall pieces (AVLwtf00/01/02).
        // Permit their order conflicts only with a mountain from the same button.
        else if (otherType != 134 &&
            (otherType != 139 || !SafeReadDword(otherTemplate + 32, otherSubtype) ||
             otherSubtype != 17 ||
             ObstacleButtonIndexForObjectTemplate(candidateTemplate) != s_traceCandidateButton ||
             ObstacleButtonIndexForObjectTemplate(otherTemplate) != s_traceCandidateButton))
            return false;
        if (!SafeReadDword(wrapper + 16, otherRow) || !otherRow ||
            !SafeReadDword(otherRow, rowId) || rowId >= kMaxRandTrnRowCount ||
            !SafeReadDword(row + 64, overlaps) || !overlaps ||
            !SafeReadDword(overlaps + rowId * 4, weight) || static_cast<int>(weight) <= -5000)
            return false;
        ++s_mountainOrderExceptions;
        return true;
    }

    void __stdcall CaptureDunePaint(DWORD generator)
    {
        s_duneCapturedGenerator = 0;
        if (s_generationFilterMode != static_cast<LONG>(ObstacleGenerationFilterMode::ButtonSelection) ||
            !SandDunesSelected()) return;
        DWORD grid = 0, width = 0, height = 0, levels = 0;
        if (!SafeReadDword(generator + 20, grid) || !grid ||
            !SafeReadDword(generator + 24, width) || width == 0 || width > 252 ||
            !SafeReadDword(generator + 28, height) || height == 0 || height > 252 ||
            !SafeReadDword(generator + 32, levels) || levels == 0 || levels > 2) return;
        ZeroMemory(s_dunePaintedCells, sizeof(s_dunePaintedCells));
        for (DWORD i = 0; i < width * height * levels; ++i)
        {
            DWORD terrain = 0, flags = 0;
            if (!SafeReadDword(grid + 48 * i + 36, terrain) ||
                !SafeReadDword(grid + 48 * i + 40, flags)) return;
            s_dunePaintedCells[i] = DuneFillTerrainAllowed(terrain) &&
                (flags & 0x04000000) && !(flags & 0x08000000);
        }
        s_duneCapturedGenerator = generator;
    }

    __declspec(naked) void DuneCaptureHook()
    {
        __asm
        {
            pushfd
            pushad
            push ecx
            call CaptureDunePaint
            popad
            popfd
            push ebp
            mov ebp, esp
            sub esp, 14h
            jmp dword ptr [s_duneCaptureContinue]
        }
    }

    void __stdcall FillDuneGaps(DWORD generator)
    {
        if (s_duneFillActive || generator != s_duneCapturedGenerator ||
            s_generationFilterMode != static_cast<LONG>(ObstacleGenerationFilterMode::ButtonSelection) ||
            !SandDunesSelected()) return;
        DWORD grid = 0, width = 0, height = 0, levels = 0;
        if (!SafeReadDword(generator + 20, grid) || !grid ||
            !SafeReadDword(generator + 24, width) || width == 0 || width > 252 ||
            !SafeReadDword(generator + 28, height) || height == 0 || height > 252 ||
            !SafeReadDword(generator + 32, levels) || levels == 0 || levels > 2) return;
        DWORD attempted = 0;
        s_duneFillActive = true;
        __try
        {
            for (DWORD z = 0; z < levels; ++z)
                for (DWORD y = 0; y < height; ++y)
                    for (DWORD x = 0; x < width; ++x)
                    {
                        const DWORD index = x + width * (y + height * z);
                        if (!s_dunePaintedCells[index]) continue;
                        const DWORD cell = grid + 48 * index;
                        DWORD terrain = 0, flags = 0;
                        if (!SafeReadDword(cell + 36, terrain) || !DuneFillTerrainAllowed(terrain) ||
                            !SafeReadDword(cell + 40, flags) ||
                            (flags & 0x08000000)) continue;
                        // Recheck native eligibility after every placement. The
                        // native candidate loop requires its blocked mask to cover this cell.
                        if (!reinterpret_cast<BOOL (__thiscall*)(DWORD)>(s_obstacleCellEligible)(cell))
                            continue;
                        ++attempted;
                        reinterpret_cast<void (__thiscall*)(DWORD, int, int, int, int)>(
                            s_generateObstacleCell)(generator, x, y, z, 0);
                    }
        }
        __finally
        {
            s_duneFillActive = false;
            s_duneCapturedGenerator = 0;
            s_candidateObjectTemplate = 0;
            s_traceCandidateRow = 0;
            s_candidateTerrainMask = 0;
        }
        LogFormat("Dune gap-fill pass: attempted %lu remaining painted eligible cells.", attempted);
    }

    __declspec(naked) void DuneFillPassHook()
    {
        __asm
        {
            pushfd
            pushad
            push ebx
            call FillDuneGaps
            popad
            popfd
            cmp [ebx+20h], edi
            mov esi, [ebx+14h]
            jmp dword ptr [s_duneFillContinue]
        }
    }

    __declspec(naked) void MountainOrderHook()
    {
        __asm {
            jz allowed
            pushfd
            pushad
            // Native scorer locals: template at ESP+18h, row at ESP+58h.
            mov eax, [esp+3Ch]
            mov ecx, [esp+7Ch]
            push edx
            push ecx
            push eax
            call AllowMountainOrder
            test al, al
            jnz relaxed
            popad
            popfd
            jmp dword ptr [s_mountainOrderReject]
        relaxed:
            popad
            popfd
        allowed:
            jmp dword ptr [s_mountainOrderContinue]
        }
    }

    bool SelectedTerrainEdgeAllowed(DWORD row, DWORD terrain)
    {
        if (s_generationFilterMode != static_cast<LONG>(ObstacleGenerationFilterMode::ButtonSelection) ||
            !row || row != s_traceCandidateRow || terrain >= kTerrainCount || terrain == 9 ||
            !(s_candidateTerrainMask & (1u << terrain)) ||
            !(IsObstacleButtonSelected(s_traceCandidateButton) ||
                (s_duneFillActive && IsRequestedDuneFiller(s_candidateObjectTemplate)))) return false;
        DWORD holder = 0, weights = 0, weight = 0;
        return SafeReadDword(row + 4, holder) && SafeReadDword(holder, weights) &&
            SafeReadDword(weights + terrain * 4, weight) && static_cast<int>(weight) > -4000;
    }

    __declspec(naked) DWORD __fastcall NativeTerrainAccumulator(DWORD, DWORD, DWORD, BYTE*)
    {
        __asm
        {
            mov eax, dword ptr [s_terrainViewTableAddress]
            mov eax, [eax]
            jmp dword ptr [s_accumulatorResume]
        }
    }

    bool AccumulateSelectedTerrain(DWORD row, DWORD terrain, BYTE* flags)
    {
        if (!flags || !SelectedTerrainEdgeAllowed(row, terrain)) return false;
        // Preserve other cells' accumulated terrain bits; add only this cell's
        // actual terrain, without Dirt/Sand inferred from its transition art.
        flags[terrain] |= 3;
        ++s_accumulatorRelaxations;
        return true;
    }

    DWORD __fastcall SelectedTerrainAccumulator(DWORD row, DWORD terrain, DWORD view, BYTE* flags)
    {
        if (AccumulateSelectedTerrain(row, terrain, flags)) return 0;
        return NativeTerrainAccumulator(row, terrain, view, flags);
    }

    bool __fastcall TracedTerrainPredicate(DWORD row, DWORD terrain, DWORD view)
    {
        if (!ShouldAllowRandTrnRow(row)) return false;
        const bool result = NativeTerrainPredicate(row, terrain, view);
        if (s_variantTraceEnabled && row == s_traceCandidateRow &&
            s_traceCandidateButton >= 0 && terrain < kTerrainCount &&
            s_generationFilterMode == static_cast<LONG>(ObstacleGenerationFilterMode::ButtonSelection))
        {
            DWORD rowId = 0;
            if (SafeReadDword(row, rowId) && rowId < kMaxRandTrnRowCount)
            {
                ++s_terrainPredicateCounts[rowId][terrain][result ? 1 : 0];
                const unsigned long long key = (static_cast<unsigned long long>(rowId) << 40) |
                    (static_cast<unsigned long long>(terrain) << 32) | view;
                if (s_terrainPredicateSamples.size() < 256 &&
                    s_terrainPredicateSamples.emplace(key, result).second)
                {
                    DWORD holder = 0, weights = 0, dirt = 0, sand = 0, current = 0;
                    SafeReadDword(row + 4, holder);
                    SafeReadDword(holder, weights);
                    SafeReadDword(weights, dirt);
                    SafeReadDword(weights + 4, sand);
                    SafeReadDword(weights + terrain * 4, current);
                    LogFormat("Obstacle boundary trace: position=(%ld,%ld,%ld) button=%s row=%lu terrain=%lu view=%lu nativeAllowed=%d weights=(current:%ld dirt:%ld sand:%ld)",
                        s_tracePosition[0], s_tracePosition[1], s_tracePosition[2],
                        ObstacleButtonName(s_traceCandidateButton), rowId, terrain, view,
                        result, current, dirt, sand);
                }
            }
        }
        // Experimental: relax only the secondary terrain-edge gate. Native
        // footprints, stored terrain weights and overlap checks are untouched.
        if (!result && SelectedTerrainEdgeAllowed(row, terrain))
        {
            ++s_edgeRelaxations;
            return true;
        }
        return result;
    }

    void __stdcall TraceCandidatePosition(DWORD frame)
    {
        if (!s_variantTraceEnabled) return;
        for (DWORD i = 0; i < 3; ++i)
            SafeReadDword(frame + 8 + i * 4, s_tracePosition[i]);
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

    __declspec(naked) void ObjectsTxtLoaderHook()
    {
        __asm
        {
            push    eax
            push    ecx
            push    dword ptr [esp+0Ch]
            call    MaybeRedirectObjectsTxtSource
            mov     [esp+0Ch], eax
            pop     ecx
            pop     eax

            mov     eax, 00529B17h
            jmp     dword ptr [s_objectsTxtLoaderResume]
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
            push    eax
            push    ebx
            call    ObserveHotaParserWrapperRow
            popad
            popfd

            jmp     dword ptr [s_hotaParserStoreResume]
        }
    }

    __declspec(naked) void HotaParserWrapperFallbackHook()
    {
        __asm
        {
            pushfd
            pushad
            push    ebx
            call    ApplyExtraObjectRandTrnFallback
            popad
            popfd

            mov     ecx, [esp+14h]
            mov     edx, [esp+1Ch]
            jmp     dword ptr [s_hotaParserWrapperFallbackResume]
        }
    }

    bool TryReadCandidateRecordRowId(DWORD record, DWORD& rowId)
    {
        DWORD row = 0;
        if (!SafeReadDword(record + 0x10, row))
            return false;
        return SafeReadDword(row, rowId) && rowId < s_hotaProfile->randTrnRowCount;
    }

    void ClearVariantTraceRows()
    {
        s_duneCapturedGenerator = 0;
        s_duneFillActive = false;
        ZeroMemory(s_variantTraceRows, sizeof(s_variantTraceRows));
        ZeroMemory(s_terrainPredicateCounts, sizeof(s_terrainPredicateCounts));
        s_terrainPredicateSamples.clear();
        s_traceCandidateRow = 0;
        s_traceCandidateButton = -1;
        s_candidateTerrainMask = 0;
        s_edgeRelaxations = 0;
        s_candidateObjectTemplate = 0;
        s_mountainOrderExceptions = 0;
        s_accumulatorRelaxations = 0;
    }

    void DumpVariantTraceRows(ObstacleGenerationFilterMode mode)
    {
        if (!s_variantTraceEnabled)
            return;

        const char* const modeName = FilterModeName(mode);

        LogFormat("Obstacle variants trace summary begin mode=%s", modeName);

        for (DWORD rowId = 0; rowId < s_hotaProfile->randTrnRowCount; ++rowId)
        {
            const VariantTraceRowStats& stats = s_variantTraceRows[rowId];
            if (stats.finalSeen == 0)
                continue;

            LogFormat(
                "Obstacle variants trace rowId=%lu obstacleId=%lu finalSeen=%ld finalAllowed=%ld finalRejected=%ld",
                rowId,
                rowId + 1,
                stats.finalSeen,
                stats.finalAllowed,
                stats.finalRejected);
        }

        LogMessage("Obstacle variants trace summary end");
        for (DWORD row = 0; row < kMaxRandTrnRowCount; ++row)
            for (DWORD terrain = 0; terrain < kTerrainCount; ++terrain)
                if (s_terrainPredicateCounts[row][terrain][0] || s_terrainPredicateCounts[row][terrain][1])
                    LogFormat("Obstacle boundary summary: row=%lu terrain=%lu nativeAllowed=%lu nativeRejected=%lu",
                        row, terrain, s_terrainPredicateCounts[row][terrain][1],
                        s_terrainPredicateCounts[row][terrain][0]);
    }

    __declspec(naked) void FinalCandidateRecordHook()
    {
        __asm
        {
            push    eax
            pushad
            push    ebp
            call    TraceCandidatePosition
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
            mov     ecx, [esp+1Ch]
            lea     edx, [ebp-64h]
            push    edx
            push    ecx
            call    RegisterObstacleButtonDefName
            popad

            mov     esi, eax
            lea     ecx, [ebp-34h]
            jmp     dword ptr [s_objectRecordAssignReturnResume]
        }
    }

}

bool ObstacleFilterEnabled()
{
    return ExtraBlockListEnvPresent();
}

bool InstallDuneFillHook(const TargetProfile& profile, std::string& error)
{
    const auto& capture = profile.duneFillCapture;
    s_duneCaptureContinue = capture.address + static_cast<DWORD>(capture.expectedLength);
    const HookSpec captureSpec = { reinterpret_cast<void*>(capture.address), capture.expectedBytes, capture.expectedLength };
    const auto captureResult = HookInstall(captureSpec, DuneCaptureHook, s_duneCaptureOriginal);
    if (captureResult != HookResult::Success)
    {
        error = std::string(capture.name) + ": " + HookResultName(captureResult);
        return false;
    }
    const auto& site = profile.duneFillPass;
    s_duneFillContinue = site.address + static_cast<DWORD>(site.expectedLength);
    s_generateObstacleCell = profile.generateObstacleCell;
    s_obstacleCellEligible = profile.obstacleCellEligible;
    const HookSpec spec = { reinterpret_cast<void*>(site.address), site.expectedBytes, site.expectedLength };
    const auto result = HookInstall(spec, DuneFillPassHook, s_duneFillOriginal);
    if (result != HookResult::Success)
    {
        error = std::string(site.name) + ": " + HookResultName(result);
        return false;
    }
    LogMessage("Dune gap-fill pass installed before native selection cleanup.");
    return true;
}

bool InstallObstacleFilterHooks(const HotaModuleTargetProfile& profile, std::string& error)
{
    const bool rowFilterEnabled = ObstacleFilterEnabled();
    const bool candidateFilterEnabled =
        ObstacleButtonDataAvailable();
    if (!rowFilterEnabled && !candidateFilterEnabled)
    {
        LogMessage("Obstacle filter: no row or candidate hooks requested.");
        return true;
    }

    HMODULE hotaMe = GetModuleHandleA("hota_me.dll");
    if (!hotaMe)
    {
        error = "hota_me.dll is not loaded";
        return false;
    }
    s_hotaProfile = &profile;

    s_variantTraceEnabled = VariantTraceEnvEnabled();
    if (rowFilterEnabled || candidateFilterEnabled || s_variantTraceEnabled)
    {
        InitializeExtraBlockedRows();

        BYTE* const moduleBase = reinterpret_cast<BYTE*>(hotaMe);
        void* const predicate = moduleBase + profile.obstacleWeightPredicateRva;
        s_predicateResume = reinterpret_cast<DWORD>(predicate) + kPredicateStolenLength;

        const HookSpec spec =
        {
            predicate,
            kPredicateExpected,
            sizeof(kPredicateExpected),
        };

        const HookResult result =
            HookInstall(spec, (candidateFilterEnabled || s_variantTraceEnabled)
                ? reinterpret_cast<void*>(TracedTerrainPredicate)
                : reinterpret_cast<void*>(HotaObstacleWeightPredicateHook), s_predicateOriginal);
        if (result != HookResult::Success)
        {
            error = std::string("HotA obstacle weight predicate: ") + HookResultName(result);
            return false;
        }

        LogFormat("Obstacle filter installed: predicate=%p nativeRows=%lu extraBlockedRows=%lu",
            predicate,
            profile.randTrnRowCount,
            s_extraBlockedRowCount);
    }

    if (candidateFilterEnabled)
    {
        BYTE expected[5] = { 0xA1 };
        s_terrainViewTableAddress = reinterpret_cast<DWORD>(hotaMe) + profile.terrainViewTableRva;
        memcpy(expected + 1, &s_terrainViewTableAddress, sizeof(DWORD));
        void* accumulator = reinterpret_cast<BYTE*>(hotaMe) + profile.terrainAccumulatorRva;
        s_accumulatorResume = reinterpret_cast<DWORD>(accumulator) + sizeof(expected);
        const HookSpec accumulatorSpec = { accumulator, expected, sizeof(expected) };
        const HookResult accumulatorResult = HookInstall(accumulatorSpec,
            reinterpret_cast<void*>(SelectedTerrainAccumulator), s_accumulatorOriginal);
        if (accumulatorResult != HookResult::Success)
        {
            error = std::string("HotA terrain accumulator: ") + HookResultName(accumulatorResult);
            return false;
        }
        LogFormat("Obstacle terrain accumulator installed at %p", accumulator);
        const BYTE orderExpected[] = { 0x0F, 0x85, 0x10, 0x01, 0x00, 0x00 };
        BYTE* orderCheck = reinterpret_cast<BYTE*>(hotaMe) + profile.mountainOrderCheckRva;
        s_mountainOrderContinue = reinterpret_cast<DWORD>(orderCheck) + 6;
        s_mountainOrderReject = s_mountainOrderContinue + 0x110;
        const HookSpec orderSpec = { orderCheck, orderExpected, sizeof(orderExpected) };
        const HookResult orderResult = HookInstall(orderSpec,
            reinterpret_cast<void*>(MountainOrderHook), s_mountainOrderOriginal);
        if (orderResult != HookResult::Success)
        {
            error = std::string("HotA mountain order adjustment: ") + HookResultName(orderResult);
            return false;
        }
        s_variantTraceEnabled = VariantTraceEnvEnabled();

        void* const objectsTxtLoader =
            reinterpret_cast<void*>(kObjectsTxtLoaderSite);
        s_objectsTxtLoaderResume =
            kObjectsTxtLoaderSite + kObjectsTxtLoaderStolenLength;

        const HookSpec objectsLoaderSpec =
        {
            objectsTxtLoader,
            kObjectsTxtLoaderExpected,
            sizeof(kObjectsTxtLoaderExpected),
        };

        HookResult result =
            HookInstall(
                objectsLoaderSpec,
                ObjectsTxtLoaderHook,
                s_objectsTxtLoaderOriginal);
        if (result != HookResult::Success)
        {
            error = std::string("map editor objects.txt glacier overlay redirect: ") +
                HookResultName(result);
            return false;
        }

        BYTE* const hotaMeBase = reinterpret_cast<BYTE*>(hotaMe);
        void* const hotaParserStore = hotaMeBase + profile.parserStoreRva;
        s_hotaParserStoreResume =
            reinterpret_cast<DWORD>(hotaParserStore) + kHotaParserStoreStolenLength;

        const HookSpec hotaParserStoreSpec =
        {
            hotaParserStore,
            kHotaParserStoreExpected,
            sizeof(kHotaParserStoreExpected),
        };

        result =
            HookInstall(
                hotaParserStoreSpec,
                HotaParserStoreHook,
                s_hotaParserStoreOriginal);
        if (result != HookResult::Success)
        {
            error = std::string("HotA glacier snow-mountain row capture: ") +
                HookResultName(result);
            HookRemove(objectsTxtLoader, s_objectsTxtLoaderOriginal);
            return false;
        }

        void* const hotaParserWrapperFallback =
            hotaMeBase + profile.parserFallbackRva;
        s_hotaParserWrapperFallbackResume =
            reinterpret_cast<DWORD>(hotaParserWrapperFallback) +
            kHotaParserWrapperFallbackStolenLength;

        const HookSpec hotaParserWrapperFallbackSpec =
        {
            hotaParserWrapperFallback,
            profile.parserFallbackExpected,
            sizeof(profile.parserFallbackExpected),
        };

        result =
            HookInstall(
                hotaParserWrapperFallbackSpec,
                HotaParserWrapperFallbackHook,
                s_hotaParserWrapperFallbackOriginal);
        if (result != HookResult::Success)
        {
            error = std::string("HotA glacier rand_trn fallback assignment: ") +
                HookResultName(result);
            HookRemove(hotaParserStore, s_hotaParserStoreOriginal);
            HookRemove(objectsTxtLoader, s_objectsTxtLoaderOriginal);
            return false;
        }

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

        result =
            HookInstall(
                objectRecordSpec,
                ObjectRecordAssignReturnHook,
                s_objectRecordAssignReturnOriginal);
        if (result != HookResult::Success)
        {
            error = std::string("map editor object DEF-key registration: ") +
                HookResultName(result);
            HookRemove(hotaParserWrapperFallback, s_hotaParserWrapperFallbackOriginal);
            HookRemove(hotaParserStore, s_hotaParserStoreOriginal);
            HookRemove(objectsTxtLoader, s_objectsTxtLoaderOriginal);
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
            HookRemove(hotaParserWrapperFallback, s_hotaParserWrapperFallbackOriginal);
            HookRemove(hotaParserStore, s_hotaParserStoreOriginal);
            HookRemove(objectsTxtLoader, s_objectsTxtLoaderOriginal);
            return false;
        }

        if (s_variantTraceEnabled)
            LogMessage("Obstacle variants trace: score-path hooks disabled; using final-candidate trace only.");

        LogFormat(
            "Obstacle variants installed: objectsOverlayRedirect=%p hotaRowCapture=%p hotaFallback=%p objectDefRegistration=%p finalCandidateRecord=%p buttonData=%s trace=%s",
            objectsTxtLoader,
            hotaParserStore,
            hotaParserWrapperFallback,
            objectRecordAssignReturn,
            predicate,
            ObstacleButtonDataAvailable() ? "available" : "missing",
            s_variantTraceEnabled ? "on" : "off");
    }

    return true;
}

void SetObstacleGenerationFilterMode(ObstacleGenerationFilterMode mode)
{
    if (!CustomObstacleModeEnabled()) mode = ObstacleGenerationFilterMode::None;
    const auto previousMode = static_cast<ObstacleGenerationFilterMode>(
        InterlockedCompareExchange(&s_generationFilterMode, 0, 0));

    if (mode == ObstacleGenerationFilterMode::None &&
        previousMode != ObstacleGenerationFilterMode::None)
    {
        if (s_edgeRelaxations)
            LogFormat("Obstacle terrain transitions: relaxed %lu predicate checks; accumulated %lu actual-terrain cells; native collision checks retained.", s_edgeRelaxations, s_accumulatorRelaxations);
        DumpVariantTraceRows(previousMode);
        LogFormat("Mountain order adjustment: %lu permitted mountain-piece conflicts; other native scoring retained.", s_mountainOrderExceptions);
        CleanupObjectsOverlayFile();
        s_traceCandidateRow = 0;
        s_candidateTerrainMask = 0;
        s_candidateObjectTemplate = 0;
    }

    if (mode != ObstacleGenerationFilterMode::None)
    {
        InterlockedExchange(&s_observedCandidates, 0);
        InterlockedExchange(&s_rejectedCandidates, 0);
        InterlockedExchange(&s_buttonBorrowAssignments, 0);
        ResetObstacleBorrowedRows();
        s_selectedObjectRules.clear();
        s_defaultSelectedRow = 0;
        s_selectedRulesFailureLogged = false;
        ClearVariantTraceRows();
    }

    s_generationAllTerrains = mode == ObstacleGenerationFilterMode::ButtonSelection && AllObstacleTerrainsEnabled();
    s_allTerrainTemplateFailure = false;
    InterlockedExchange(&s_generationFilterMode, static_cast<LONG>(mode));
}

void SetObstacleButtonSelection(DWORD lowMask, DWORD highMask, DWORD extendedMask, DWORD upperMask)
{
    InterlockedExchange(&s_selectedButtonMaskLow, static_cast<LONG>(lowMask));
    InterlockedExchange(&s_selectedButtonMaskHigh, static_cast<LONG>(highMask));
    InterlockedExchange(&s_selectedButtonMaskExtended, static_cast<LONG>(extendedMask));
    InterlockedExchange(&s_selectedButtonMaskUpper, static_cast<LONG>(upperMask));
}

bool CustomObstacleModeEnabled()
{
    return InterlockedCompareExchange(&s_customObstacleMode, 0, 0) != 0;
}

void SetCustomObstacleMode(bool enabled)
{
    if (!enabled) SetObstacleGenerationFilterMode(ObstacleGenerationFilterMode::None);
    InterlockedExchange(&s_customObstacleMode, enabled ? 1 : 0);
}

bool AllObstacleTerrainsEnabled()
{
    return CustomObstacleModeEnabled() && InterlockedCompareExchange(&s_allObstacleTerrains, 0, 0) != 0;
}

void SetAllObstacleTerrains(bool enabled)
{
    InterlockedExchange(&s_allObstacleTerrains, enabled ? 1 : 0);
}

DWORD EffectiveObstacleTerrainMask(DWORD workbookMask)
{
    return AllObstacleTerrainsEnabled() ? kAllObstacleTerrainMask : workbookMask;
}
