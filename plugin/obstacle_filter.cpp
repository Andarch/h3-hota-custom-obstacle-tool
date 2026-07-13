#include "pch.h"
#include "obstacle_filter.h"

#include "hook.h"
#include "logger.h"
#include "obstacle_button_data.h"
#include "obstacle_classification.h"
#include "target_profile.h"

#include <cstdlib>
#include <cstdio>
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
    constexpr DWORD kHotaParserStoreRva = 0x000D6DF4;
    constexpr DWORD kHotaParserWrapperFallbackRva = 0x000D6E01;
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
    constexpr BYTE kNoReferenceRow = 0xFF;

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

    const BYTE kHotaParserWrapperFallbackExpected[] =
    {
        0x8B, 0x4C, 0x24, 0x1C, // mov ecx, [esp+1Ch]
        0x8B, 0x54, 0x24, 0x18, // mov edx, [esp+18h]
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
    constexpr DWORD kRandTrnRowCount =
        sizeof(kAllowedRandTrnRows) / sizeof(kAllowedRandTrnRows[0]);

    const BYTE kRandTrnTerrainByRow[kRandTrnRowCount] =
    {
          0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   1,
          1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   2,   2,   2,   2,   2,   2,   2,   2,
          2,   2,   2,   2,   2,   2,   2,   2,   2,   2,   2,   2,   2,   2,   3,   3,   3,   3,   3,   3,
          3,   3,   3,   3,   3,   4,   4,   4,   4,   4,   4,   4,   4,   4,   4,   4,   4,   4,   4,   4,
          4,   4,   4,   4,   5,   5,   5,   5,   5,   5,   5,   5,   5,   5,   5,   5,   5,   5,   5,   5,
          6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   7,   7,   7,   7,   7,
          7,   7,   7,   7,   7,   7,   8,   8,   4,   4,   1,   1,   1,   1,   1,   1,   1,   4,   4,   0,
          0,   0,   2,   1,   4,   7,   7,   1,   3,   3,   0,   0,   2,   2,   3,   3,   4,   4,   5,   5,
          6,   6,   3,   1,   1,   1,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,
         10,  10,  10,  11,  11,  11,  11,  11,  11,  11,  11,  11,  11,  11,  11,  11,  11,  11,  11,   7,
    };

    const BYTE kThinScoringReferenceRowByTerrain[kTerrainCount] =
    {
        kNoReferenceRow,    // dirt
        kNoReferenceRow,    // sand
        32,                 // grass oak tree
        61,                 // snow pine tree
        kNoReferenceRow,    // swamp
        kNoReferenceRow,    // rough
        kNoReferenceRow,    // cave
        119,                // lava mountain
        kNoReferenceRow,    // water
        kNoReferenceRow,    // reserved terrain 9
        kNoReferenceRow,    // highlands
        194,                // wasteland stone, used by the arch overlay
    };

    const char* const kExtraThickObjectRows[] =
    {
        "AVLGLC01.def 000111110000011110000111111111111111111111111111 000000000000000000000000000000000000000000000000 110111111111 000000001000 140 14 0 0",
        "AVLGLC02.def 110001110000011100001111111111111111111111111111 000000000000000000000000000000000000000000000000 110111111111 000000001000 140 14 0 0",
        "AVLGLC03.def 001111110001111111111111111111111111111111111111 000000000000000000000000000000000000000000000000 110111111111 000000001000 140 14 0 0",
        "AVLGLC04.def 100111110001111111111111111111111111111111111111 000000000000000000000000000000000000000000000000 110111111111 000000001000 140 14 0 0",
        "AVLGLC05.def 001111111001111111111111111111111111111111111111 000000000000000000000000000000000000000000000000 110111111111 000000001000 140 14 0 0",
        "AVLGLC06.def 100111110011111111111111111111111111111111111111 000000000000000000000000000000000000000000000000 110111111111 000000001000 140 14 0 0",
    };

    const char* const kExtraThinObjectRows[] =
    {
        "AVLbon01.def 001111111111111111111111111111111111111111111111 000000000000000000000000000000000000000000000000 110111111111 100000100010 139 22 0 0",
        "AVLMTWL7.def 010111111111111111111111111111111111111111111111 000000000000000000000000000000000000000000000000 110111111111 100000000000 140 13 0 0",
    };

    BYTE s_predicateOriginal[5] = {};
    BYTE s_objectsTxtLoaderOriginal[5] = {};
    BYTE s_hotaParserStoreOriginal[5] = {};
    BYTE s_hotaParserWrapperFallbackOriginal[5] = {};
    BYTE s_objectRecordAssignReturnOriginal[5] = {};
    BYTE s_finalCandidateRecordOriginal[5] = {};
    BYTE s_extraBlockedRandTrnRows[kRandTrnRowCount] = {};
    DWORD s_predicateResume = 0;
    DWORD s_objectsTxtLoaderResume = 0;
    DWORD s_hotaParserStoreResume = 0;
    DWORD s_hotaParserWrapperFallbackResume = 0;
    DWORD s_objectRecordAssignReturnResume = 0;
    DWORD s_finalCandidateRecordResume = 0;
    DWORD s_finalCandidateReject = kFinalCandidateRejectTarget;
    DWORD s_snowMountainRowRecord = 0;
    DWORD s_wastelandStoneRowRecord = 0;
    DWORD s_thinScoringReferenceRows[kTerrainCount] = {};
    DWORD s_extraBlockedRowCount = 0;
    bool s_workbookFilterEnabled = false;
    bool s_variantTraceEnabled = false;
    bool s_objectsOverlayReady = false;
    bool s_objectsOverlayAttempted = false;
    char s_objectsOverlayPath[MAX_PATH] = {};
    char s_objectsOverlayResourceName[MAX_PATH] = {};
    volatile LONG s_rejectedRows = 0;
    volatile LONG s_observedCandidates = 0;
    volatile LONG s_rejectedCandidates = 0;
    volatile LONG s_glacierFallbackAssignments = 0;
    volatile LONG s_archFallbackAssignments = 0;
    volatile LONG s_thinScoringRowOverrides = 0;
    volatile LONG s_generationFilterMode = 0;
    volatile LONG s_selectedButtonMaskLow = 0;
    volatile LONG s_selectedButtonMaskHigh = 0;
    volatile LONG s_buttonBorrowAssignments = 0;

    struct VariantTraceRowStats
    {
        LONG finalSeen;
        LONG finalAllowed;
        LONG finalRejected;
    };

    VariantTraceRowStats s_variantTraceRows[kRandTrnRowCount] = {};

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
        if (mode == ObstacleGenerationFilterMode::ThickOnly)
            return "thick-only";
        if (mode == ObstacleGenerationFilterMode::ThinOnly)
            return "thin-only";
        if (mode == ObstacleGenerationFilterMode::ButtonSelection)
            return "button-selection";
        return "none";
    }

    bool IsObstacleButtonSelected(int buttonIndex)
    {
        if (buttonIndex < 0 || buttonIndex >= 64)
            return false;
        const DWORD mask = static_cast<DWORD>(InterlockedCompareExchange(
            buttonIndex < 32 ? &s_selectedButtonMaskLow : &s_selectedButtonMaskHigh,
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

    const char* const* ExtraObjectRowsForMode(
        ObstacleGenerationFilterMode mode,
        DWORD& rowCount)
    {
        if (mode == ObstacleGenerationFilterMode::ThickOnly)
        {
            rowCount = static_cast<DWORD>(
                sizeof(kExtraThickObjectRows) / sizeof(kExtraThickObjectRows[0]));
            return kExtraThickObjectRows;
        }

        if (mode == ObstacleGenerationFilterMode::ThinOnly)
        {
            rowCount = static_cast<DWORD>(
                sizeof(kExtraThinObjectRows) / sizeof(kExtraThinObjectRows[0]));
            return kExtraThinObjectRows;
        }

        rowCount = 0;
        return nullptr;
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
        if (!ReadWholeFile(sourcePath, source))
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
        if (mode == ObstacleGenerationFilterMode::ButtonSelection)
        {
            if (!BuildButtonSelectionOverlay(
                    source,
                    firstLineEnd,
                    rowCount,
                    overlay,
                    resultingRowCount,
                    replacedRows,
                    extraRows))
            {
                LogMessage("Obstacle variants: could not build button-selection object overlay.");
                return false;
            }
        }
        else
        {
            const char* const* rows = ExtraObjectRowsForMode(mode, extraRows);
            if (!rows || extraRows == 0)
                return false;

            char newCount[32] = {};
            sprintf_s(newCount, "%lu", rowCount + extraRows);
            resultingRowCount = rowCount + extraRows;
            overlay.reserve(source.size() + 1024);
            overlay.assign(source.data(), source.data() + firstLineEnd);
            overlay.replace(0, overlay.size(), newCount);
            overlay.append(source.data() + firstLineEnd, source.data() + source.size());
            if (!overlay.empty() && overlay[overlay.size() - 1] != '\n')
                overlay += "\r\n";
            for (DWORD index = 0; index < extraRows; ++index)
            {
                overlay += rows[index];
                overlay += "\r\n";
            }
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

        DWORD extraRows = 0;
        if (!ExtraObjectRowsForMode(mode, extraRows) || extraRows == 0)
            return source;

        return EnsureObjectsOverlayFile(mode) ? s_objectsOverlayResourceName : source;
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

            if (obstacleId == 0 || obstacleId > kRandTrnRowCount)
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

        if (rowId >= kRandTrnRowCount)
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

    bool ObstacleThicknessAllowedInMode(
        ObstacleThickness thickness,
        ObstacleGenerationFilterMode mode)
    {
        if (thickness == ObstacleThickness::Both)
            return true;
        if (mode == ObstacleGenerationFilterMode::ThickOnly)
            return thickness == ObstacleThickness::Thick;
        if (mode == ObstacleGenerationFilterMode::ThinOnly)
            return thickness == ObstacleThickness::Thin;
        return true;
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
                    FilterModeName(mode));
            }
            else if (hit == 33)
            {
                LogMessage("Obstacle variants: further candidate rejection logs suppressed.");
            }
            return false;
        }

        const ObstacleThickness thickness =
            GetObstacleThicknessForObjectTemplate(objectTemplate);
        const int buttonIndex = ObstacleButtonIndexForObjectTemplate(objectTemplate);
        const bool allowed = mode == ObstacleGenerationFilterMode::ButtonSelection
            ? IsObstacleButtonSelected(buttonIndex)
            : ObstacleThicknessAllowedInMode(thickness, mode);

        DWORD rowId = 0;
        const bool haveRowId = TryReadCandidateRecordRowId(record, rowId);
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
                "Obstacle variants: observed final candidate record=%p template=%p defKey=%lu objectId=%lu subtype=%lu button=%d thickness=%s mode=%s",
                reinterpret_cast<void*>(record),
                reinterpret_cast<void*>(objectTemplate),
                defKey,
                objectId,
                subtype,
                buttonIndex,
                ObstacleThicknessName(thickness),
                FilterModeName(mode));
        }

        if (!allowed)
        {
            const LONG hit = InterlockedIncrement(&s_rejectedCandidates);
            if (s_variantTraceEnabled && hit <= 32)
            {
                LogFormat(
                    "Obstacle variants: rejected final candidate record=%p template=%p defKey=%lu objectId=%lu subtype=%lu button=%d thickness=%s reason=%s",
                    reinterpret_cast<void*>(record),
                    reinterpret_cast<void*>(objectTemplate),
                    defKey,
                    objectId,
                    subtype,
                    buttonIndex,
                    ObstacleThicknessName(thickness),
                    FilterModeName(mode));
            }
            else if (s_variantTraceEnabled && hit == 33)
            {
                LogMessage("Obstacle variants: further candidate rejection logs suppressed.");
            }
        }
        else if (mode == ObstacleGenerationFilterMode::ThinOnly &&
            thickness == ObstacleThickness::Thin &&
            haveRowId &&
            rowId < kRandTrnRowCount)
        {
            const BYTE terrain = kRandTrnTerrainByRow[rowId];
            if (terrain < kTerrainCount)
            {
                const DWORD referenceRow = s_thinScoringReferenceRows[terrain];
                DWORD currentRow = 0;
                if (referenceRow != 0 &&
                    SafeReadDword(record + 0x10, currentRow) &&
                    currentRow != referenceRow &&
                    SafeWriteDword(record + 0x10, referenceRow))
                {
                    const LONG hit = InterlockedIncrement(&s_thinScoringRowOverrides);
                    if (s_variantTraceEnabled && hit <= 32)
                    {
                        DWORD referenceRowId = 0;
                        SafeReadDword(referenceRow, referenceRowId);
                        LogFormat(
                            "Obstacle variants: thin scoring row override record=%p terrain=%lu rowId=%lu referenceRowId=%lu",
                            reinterpret_cast<void*>(record),
                            static_cast<DWORD>(terrain),
                            rowId,
                            referenceRowId);
                    }
                    else if (s_variantTraceEnabled && hit == 33)
                    {
                        LogMessage("Obstacle variants: further thin scoring row override logs suppressed.");
                    }
                }
            }
        }

        return allowed;
    }

    bool IsSnowMountainWrapper(DWORD wrapper)
    {
        DWORD objectTemplate = 0;
        DWORD firstTerrain = 0;
        DWORD objectId = 0;
        DWORD subtype = 0;
        return SafeReadDword(wrapper, objectTemplate) &&
            SafeReadDword(wrapper + 4, firstTerrain) &&
            firstTerrain == 3 &&
            objectTemplate != 0 &&
            SafeReadDword(objectTemplate + 0x1C, objectId) &&
            SafeReadDword(objectTemplate + 0x20, subtype) &&
            objectId == 134 &&
            subtype == 0;
    }

    bool IsGlacierWrapper(DWORD wrapper)
    {
        DWORD objectTemplate = 0;
        DWORD firstTerrain = 0;
        DWORD objectId = 0;
        DWORD subtype = 0;
        return SafeReadDword(wrapper, objectTemplate) &&
            SafeReadDword(wrapper + 4, firstTerrain) &&
            firstTerrain == 3 &&
            objectTemplate != 0 &&
            SafeReadDword(objectTemplate + 0x1C, objectId) &&
            SafeReadDword(objectTemplate + 0x20, subtype) &&
            objectId == 140 &&
            subtype == 14;
    }

    bool IsWastelandStoneWrapper(DWORD wrapper)
    {
        DWORD objectTemplate = 0;
        DWORD firstTerrain = 0;
        DWORD objectId = 0;
        DWORD subtype = 0;
        return SafeReadDword(wrapper, objectTemplate) &&
            SafeReadDword(wrapper + 4, firstTerrain) &&
            firstTerrain == 11 &&
            objectTemplate != 0 &&
            SafeReadDword(objectTemplate + 0x1C, objectId) &&
            SafeReadDword(objectTemplate + 0x20, subtype) &&
            objectId == 140 &&
            subtype == 1;
    }

    bool IsWastelandArchWrapper(DWORD wrapper)
    {
        DWORD objectTemplate = 0;
        DWORD firstTerrain = 0;
        DWORD objectId = 0;
        DWORD subtype = 0;
        return SafeReadDword(wrapper, objectTemplate) &&
            SafeReadDword(wrapper + 4, firstTerrain) &&
            firstTerrain == 11 &&
            objectTemplate != 0 &&
            SafeReadDword(objectTemplate + 0x1C, objectId) &&
            SafeReadDword(objectTemplate + 0x20, subtype) &&
            objectId == 140 &&
            subtype == 13;
    }

    void __stdcall ObserveHotaParserWrapperRow(DWORD wrapper, DWORD rowRecord)
    {
        if (rowRecord == 0)
            return;

        const auto mode = static_cast<ObstacleGenerationFilterMode>(
            InterlockedCompareExchange(&s_generationFilterMode, 0, 0));

        if (mode == ObstacleGenerationFilterMode::ButtonSelection)
        {
            DWORD objectTemplate = 0;
            if (SafeReadDword(wrapper, objectTemplate) && objectTemplate)
                ObserveObstacleBorrowSourceRow(objectTemplate, rowRecord);
        }

        if (mode == ObstacleGenerationFilterMode::ThickOnly &&
            IsSnowMountainWrapper(wrapper) &&
            s_snowMountainRowRecord == 0)
        {
            s_snowMountainRowRecord = rowRecord;
            DWORD rowId = 0;
            SafeReadDword(rowRecord, rowId);
            LogFormat(
                "Obstacle variants: captured snow mountain rand_trn row for glacier overlay rowRecord=%p rowId=%lu",
                reinterpret_cast<void*>(rowRecord),
                rowId);
        }

        if (mode == ObstacleGenerationFilterMode::ThinOnly &&
            IsWastelandStoneWrapper(wrapper) &&
            s_wastelandStoneRowRecord == 0)
        {
            s_wastelandStoneRowRecord = rowRecord;
            DWORD rowId = 0;
            SafeReadDword(rowRecord, rowId);
            LogFormat(
                "Obstacle variants: captured wasteland stone rand_trn row for arch overlay rowRecord=%p rowId=%lu",
                reinterpret_cast<void*>(rowRecord),
                rowId);
        }

        if (mode == ObstacleGenerationFilterMode::ThinOnly)
        {
            DWORD rowId = 0;
            if (SafeReadDword(rowRecord, rowId) && rowId < kRandTrnRowCount)
            {
                for (DWORD terrain = 0; terrain < kTerrainCount; ++terrain)
                {
                    if (kThinScoringReferenceRowByTerrain[terrain] == rowId &&
                        s_thinScoringReferenceRows[terrain] == 0)
                    {
                        s_thinScoringReferenceRows[terrain] = rowRecord;
                        LogFormat(
                            "Obstacle variants: captured thin scoring reference terrain=%lu rowRecord=%p rowId=%lu",
                            terrain,
                            reinterpret_cast<void*>(rowRecord),
                            rowId);
                        break;
                    }
                }
            }
        }
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
            if (!sourceRowRecord)
                return;

            DWORD existingRow = 0;
            if (SafeReadDword(wrapper + 0x10, existingRow) &&
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

        DWORD sourceRowRecord = 0;
        bool isTarget = false;
        const char* targetName = "";
        const char* sourceName = "";
        volatile LONG* assignmentCounter = nullptr;

        if (mode == ObstacleGenerationFilterMode::ThickOnly &&
            s_snowMountainRowRecord != 0 &&
            IsGlacierWrapper(wrapper))
        {
            sourceRowRecord = s_snowMountainRowRecord;
            isTarget = true;
            targetName = "glacier";
            sourceName = "snow mountain";
            assignmentCounter = &s_glacierFallbackAssignments;
        }
        else if (mode == ObstacleGenerationFilterMode::ThinOnly &&
            s_wastelandStoneRowRecord != 0 &&
            IsWastelandArchWrapper(wrapper))
        {
            sourceRowRecord = s_wastelandStoneRowRecord;
            isTarget = true;
            targetName = "wasteland arch";
            sourceName = "wasteland stone";
            assignmentCounter = &s_archFallbackAssignments;
        }

        if (!isTarget || sourceRowRecord == 0 || !assignmentCounter)
            return;

        DWORD existingRow = 0;
        if (SafeReadDword(wrapper + 0x10, existingRow) && existingRow != 0)
            return;

        if (!SafeWriteDword(wrapper + 0x10, sourceRowRecord))
            return;

        const LONG hit = InterlockedIncrement(assignmentCounter);
        if (hit <= 8)
        {
            DWORD objectTemplate = 0;
            DWORD defKey = 0;
            DWORD rowId = 0;
            SafeReadDword(wrapper, objectTemplate);
            if (objectTemplate)
                SafeReadDword(objectTemplate, defKey);
            SafeReadDword(sourceRowRecord, rowId);
            LogFormat(
                "Obstacle variants: assigned %s wrapper=%p template=%p defKey=%lu to %s rand_trn rowId=%lu",
                targetName,
                reinterpret_cast<void*>(wrapper),
                reinterpret_cast<void*>(objectTemplate),
                defKey,
                sourceName,
                rowId);
        }
        else if (hit == 9)
        {
            LogFormat("Obstacle variants: further %s rand_trn fallback logs suppressed.",
                targetName);
        }
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

            mov     ecx, [esp+1Ch]
            mov     edx, [esp+18h]
            jmp     dword ptr [s_hotaParserWrapperFallbackResume]
        }
    }

    bool TryReadCandidateRecordRowId(DWORD record, DWORD& rowId)
    {
        DWORD row = 0;
        if (!SafeReadDword(record + 0x10, row))
            return false;
        return SafeReadDword(row, rowId) && rowId < kRandTrnRowCount;
    }

    void ClearVariantTraceRows()
    {
        ZeroMemory(s_variantTraceRows, sizeof(s_variantTraceRows));
    }

    void DumpVariantTraceRows(ObstacleGenerationFilterMode mode)
    {
        if (!s_variantTraceEnabled)
            return;

        const char* const modeName = FilterModeName(mode);

        LogFormat("Obstacle variants trace summary begin mode=%s", modeName);

        for (DWORD rowId = 0; rowId < kRandTrnRowCount; ++rowId)
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
            mov     ecx, [esp+1Ch]
            lea     edx, [ebp-64h]
            push    edx
            push    ecx
            call    RegisterObstacleTemplateDefName

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
    return WorkbookFilterEnvEnabled() || ExtraBlockListEnvPresent();
}

bool InstallObstacleFilterHooks(std::string& error)
{
    const bool rowFilterEnabled = ObstacleFilterEnabled();
    const bool candidateFilterEnabled =
        ObstacleThicknessTableAvailable() || ObstacleButtonDataAvailable();
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

        HMODULE hotaMe = GetModuleHandleA("hota_me.dll");
        if (!hotaMe)
        {
            error = "hota_me.dll is not loaded for glacier rand_trn fallback";
            HookRemove(objectsTxtLoader, s_objectsTxtLoaderOriginal);
            return false;
        }

        if (!ValidateHotaMeModule(hotaMe, error))
        {
            error = std::string("glacier rand_trn fallback: ") + error;
            HookRemove(objectsTxtLoader, s_objectsTxtLoaderOriginal);
            return false;
        }

        BYTE* const hotaMeBase = reinterpret_cast<BYTE*>(hotaMe);
        void* const hotaParserStore = hotaMeBase + kHotaParserStoreRva;
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
            hotaMeBase + kHotaParserWrapperFallbackRva;
        s_hotaParserWrapperFallbackResume =
            reinterpret_cast<DWORD>(hotaParserWrapperFallback) +
            kHotaParserWrapperFallbackStolenLength;

        const HookSpec hotaParserWrapperFallbackSpec =
        {
            hotaParserWrapperFallback,
            kHotaParserWrapperFallbackExpected,
            sizeof(kHotaParserWrapperFallbackExpected),
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
            "Obstacle variants installed: objectsOverlayRedirect=%p hotaRowCapture=%p hotaFallback=%p objectDefRegistration=%p finalCandidateRecord=%p thicknessTable=%s buttonData=%s trace=%s",
            objectsTxtLoader,
            hotaParserStore,
            hotaParserWrapperFallback,
            objectRecordAssignReturn,
            predicate,
            ObstacleThicknessTableAvailable() ? "available" : "missing",
            ObstacleButtonDataAvailable() ? "available" : "missing",
            s_variantTraceEnabled ? "on" : "off");
    }

    return true;
}

void SetObstacleGenerationFilterMode(ObstacleGenerationFilterMode mode)
{
    const auto previousMode = static_cast<ObstacleGenerationFilterMode>(
        InterlockedCompareExchange(&s_generationFilterMode, 0, 0));

    if (mode == ObstacleGenerationFilterMode::None &&
        previousMode != ObstacleGenerationFilterMode::None)
    {
        DumpVariantTraceRows(previousMode);
        CleanupObjectsOverlayFile();
    }

    if (mode != ObstacleGenerationFilterMode::None)
    {
        InterlockedExchange(&s_observedCandidates, 0);
        InterlockedExchange(&s_rejectedCandidates, 0);
        InterlockedExchange(&s_glacierFallbackAssignments, 0);
        InterlockedExchange(&s_archFallbackAssignments, 0);
        InterlockedExchange(&s_buttonBorrowAssignments, 0);
        InterlockedExchange(&s_thinScoringRowOverrides, 0);
        s_snowMountainRowRecord = 0;
        s_wastelandStoneRowRecord = 0;
        ZeroMemory(s_thinScoringReferenceRows, sizeof(s_thinScoringReferenceRows));
        ResetObstacleBorrowedRows();
        ClearVariantTraceRows();
    }

    InterlockedExchange(&s_generationFilterMode, static_cast<LONG>(mode));
}

void SetObstacleButtonSelection(DWORD lowMask, DWORD highMask)
{
    InterlockedExchange(&s_selectedButtonMaskLow, static_cast<LONG>(lowMask));
    InterlockedExchange(&s_selectedButtonMaskHigh, static_cast<LONG>(highMask));
}
