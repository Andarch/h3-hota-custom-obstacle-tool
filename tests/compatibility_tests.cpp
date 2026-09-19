// Include the implementation to exercise internal hook thunks without test exports.
#include "../plugin/obstacle_filter.cpp"
#include "../plugin/obstacle_selection_geometry.h"

#include <cstdio>
#include <cstdlib>

#define CHECK(expression) do { if (!(expression)) { \
    std::fprintf(stderr, "FAILED line %d: %s\n", __LINE__, #expression); \
    std::abort(); } } while (false)

namespace
{
    void TestSelectedRules()
    {
        DWORD source[19] = {};
        int weights[12];
        int adjacent[201];
        int overlap[201];
        for (int& value : weights) value = -5000;
        weights[4] = 5;
        for (int& value : adjacent) value = -5000;
        for (int& value : overlap) value = -5000;
        DWORD holder = reinterpret_cast<DWORD>(weights);
        source[0] = 137;
        source[1] = reinterpret_cast<DWORD>(&holder);
        source[12] = reinterpret_cast<DWORD>(adjacent);
        source[13] = source[12] + sizeof(adjacent);
        source[16] = reinterpret_cast<DWORD>(overlap);
        source[17] = source[16] + sizeof(overlap);
        SelectedObjectRules rules;
        CHECK(BuildSelectedObjectRules(reinterpret_cast<DWORD>(source), 0xCFF, rules));
        for (DWORD i = 0; i < 12; ++i)
            CHECK(rules.weights[i] == ((i == 8 || i == 9) ? -5000 : 5));
        CHECK(rules.adjacent[0] == 5 && rules.overlap[0] == -5000);
        CHECK(weights[0] == -5000 && adjacent[0] == -5000);
        CHECK(rules.row[1] == reinterpret_cast<DWORD>(&rules.weightPointer));
        CHECK(BuildSelectedObjectRules(reinterpret_cast<DWORD>(source), 1u << 4, rules));
        CHECK(rules.adjacent[0] == -5000);
        CHECK(!BuildSelectedObjectRules(0, 0xCFF, rules));
        source[13] -= 4;
        CHECK(!BuildSelectedObjectRules(reinterpret_cast<DWORD>(source), 0xCFF, rules));
        source[13] += 4;
        DWORD object[19] = {};
        DWORD candidate[5] = {};
        object[0] = 7000;
        object[6] = 0xCFF;
        RegisterObstacleButtonDefName(reinterpret_cast<DWORD>(object), reinterpret_cast<DWORD>("crates04.def"));
        candidate[0] = reinterpret_cast<DWORD>(object);
        SetObstacleGenerationFilterMode(ObstacleGenerationFilterMode::ButtonSelection);
        s_defaultSelectedRow = reinterpret_cast<DWORD>(source);
        SetObstacleButtonSelection(0, 0);
        CHECK(!ShouldAllowFinalCandidateRecord(reinterpret_cast<DWORD>(candidate)));
        CHECK(candidate[4] == 0);
        SetObstacleButtonSelection(MAXDWORD, MAXDWORD);
        CHECK(ShouldAllowFinalCandidateRecord(reinterpret_cast<DWORD>(candidate)));
        CHECK(candidate[4] != 0);
        const DWORD privateRow = candidate[4];
        CHECK(ShouldAllowFinalCandidateRecord(reinterpret_cast<DWORD>(candidate)));
        CHECK(candidate[4] == privateRow);
        SetObstacleGenerationFilterMode(ObstacleGenerationFilterMode::None);
        candidate[4] = 0;
        CHECK(ShouldAllowFinalCandidateRecord(reinterpret_cast<DWORD>(candidate)));
        CHECK(candidate[4] == 0);
    }
    DWORD testStackBefore;
    DWORD testStackAfter;
    DWORD testFlagsBefore;
    DWORD testFlagsAfter;
    DWORD testEcx;
    DWORD testEdx;
    DWORD testWrapper[5] = {};
    DWORD testRow[19] = {};
    DWORD testRows[] = { reinterpret_cast<DWORD>(testRow) };
    DWORD testVector[] = { 0, reinterpret_cast<DWORD>(testRows) };

    __declspec(naked) void PredicateResumeStub()
    {
        __asm
        {
            pop esi
            mov al, 1
            ret 4
        }
    }

    void TestPredicateThunk()
    {
        DWORD weights[12] = {};
        DWORD weightsHolder = reinterpret_cast<DWORD>(weights);
        testRow[1] = reinterpret_cast<DWORD>(&weightsHolder);
        s_predicateResume = reinterpret_cast<DWORD>(PredicateResumeStub);
        s_workbookFilterEnabled = true;
        const auto predicate = reinterpret_cast<bool (__fastcall*)(DWORD, DWORD, DWORD)>(
            HotaObstacleWeightPredicateHook);
        testRow[0] = 6;
        CHECK(predicate(reinterpret_cast<DWORD>(testRow), 0, 0));
        testRow[0] = 14;
        CHECK(!predicate(reinterpret_cast<DWORD>(testRow), 0, 0));
        s_workbookFilterEnabled = false;
        s_predicateResume = 0;
    }

    // Assembly jumps back to local labels; keep this harness unoptimized.
#pragma optimize("", off)
    void TestParserThunks()
    {
        // The call pushes one DWORD, matching the native hook's stack offsets.
        __asm
        {
            pushfd
            pushad
            mov testStackBefore, esp
            sub esp, 40h
            mov eax, offset storeReturned
            mov s_hotaParserStoreResume, eax
            mov eax, offset testVector
            mov [esp+2Ch], eax
            mov ebx, offset testWrapper
            xor ecx, ecx
            stc
            pushfd
            pop testFlagsBefore
            call HotaParserStoreHook
        storeReturned:
            mov testStackAfter, esp
            pushfd
            pop testFlagsAfter
            add esp, 44h
            popad
            popfd
        }
        CHECK(testWrapper[4] == reinterpret_cast<DWORD>(testRow));
        CHECK(testStackAfter == testStackBefore - 0x44);
        CHECK((testFlagsBefore & 0xCD5) == (testFlagsAfter & 0xCD5));

        __asm
        {
            pushfd
            pushad
            mov testStackBefore, esp
            sub esp, 20h
            mov eax, offset fallbackReturned
            mov s_hotaParserWrapperFallbackResume, eax
            mov dword ptr [esp+10h], 14141414h
            mov dword ptr [esp+14h], 18181818h
            mov dword ptr [esp+18h], 1C1C1C1Ch
            mov ebx, offset testWrapper
            stc
            pushfd
            pop testFlagsBefore
            call HotaParserWrapperFallbackHook
        fallbackReturned:
            mov testEcx, ecx
            mov testEdx, edx
            mov testStackAfter, esp
            pushfd
            pop testFlagsAfter
            add esp, 24h
            popad
            popfd
        }
        CHECK(testEcx == 0x14141414);
        CHECK(testEdx == 0x1C1C1C1C);
        CHECK(testStackAfter == testStackBefore - 0x24);
        CHECK((testFlagsBefore & 0xCD5) == (testFlagsAfter & 0xCD5));
        s_hotaParserStoreResume = 0;
        s_hotaParserWrapperFallbackResume = 0;
    }
#pragma optimize("", on)
}

int main()
{
    unsigned int width = 0;
    unsigned int height = 0;
    for (DWORD size = 36; size <= 252; size += 36)
    {
        const DWORD columns = size / 6;
        CHECK(TryComputeTerrainMapDimensions(columns, 0x1000,
            0x1000 + 4 * columns * columns, width, height));
        CHECK(width == size && height == size);
    }
    CHECK(TryComputeTerrainMapDimensions(12, 0x1000, 0x1240, width, height));
    CHECK(width == 72 && height == 72);
    CHECK(!TryComputeTerrainMapDimensions(0, 0x1000, 0x1240, width, height));
    CHECK(!TryComputeTerrainMapDimensions(12, 0, 0x240, width, height));
    CHECK(!TryComputeTerrainMapDimensions(12, 0x1000, 0x1000, width, height));
    CHECK(!TryComputeTerrainMapDimensions(12, 0x1240, 0x1000, width, height));
    CHECK(!TryComputeTerrainMapDimensions(12, 0x1000, 0x1241, width, height));
    CHECK(!TryComputeTerrainMapDimensions(12, 0x1000, 0x1244, width, height));
    CHECK(!TryComputeTerrainMapDimensions(12, 0x1000, 0x1270, width, height));
    CHECK(!TryComputeTerrainMapDimensions(48, 0x1000, 0x3400, width, height));
    CHECK(!TryComputeTerrainMapDimensions(12, 0x1000, 0x1900, width, height));
    CHECK(!TryComputeTerrainMapDimensions(MAXDWORD, 4, MAXDWORD, width, height));
    CHECK(width == 0 && height == 0);

    CHECK(!FindTargetProfile(nullptr));
    CHECK(!FindTargetProfile("unknown"));
    CHECK(FindTargetProfile("CCF1E8424D913AA409ECC168F87FB20EDD64C3A56D412F2878065FC261BC6113"));
    CHECK(!FindHotaModuleTargetProfile(nullptr));
    CHECK(!FindHotaModuleTargetProfile("unknown"));
    CHECK(!FindHotaModuleTargetProfile("3CE4489A0B891E35C13848C33D1124F750D1A0C96D0B52761CA87BFFF5BC0D8E"));
    const auto* profile = FindHotaModuleTargetProfile(
        "365EB0C5C01ADBEAEC6A46E12D49BA2C710150270600ACE496496EB432DEA240");
    CHECK(profile);
    CHECK(profile->randTrnRowCount == 201);
    CHECK(profile->parserFallbackRva == profile->parserStoreRva + 13);
    CHECK(profile->parserFallbackExpected[3] == 0x14);
    CHECK(profile->parserFallbackExpected[7] == 0x1C);
    BYTE oldFallback[] = { 0x8B, 0x4C, 0x24, 0x1C, 0x8B, 0x54, 0x24, 0x18 };
    BYTE saved[5] = {};
    const HookSpec wrongStackLayout = { oldFallback, profile->parserFallbackExpected, 8 };
    CHECK(HookInstall(wrongStackLayout, HotaParserWrapperFallbackHook, saved) ==
        HookResult::UnexpectedBytes);
    CHECK(oldFallback[0] == 0x8B && oldFallback[3] == 0x1C && oldFallback[7] == 0x18);
    s_hotaProfile = profile;

    for (DWORD row = 0; row < 199; ++row)
    {
        CHECK(LegacyRandTrnRowIndex(*profile, row) == row);
        CHECK(RandTrnTerrain(row) == kRandTrnTerrainByRow[row]);
    }
    CHECK(LegacyRandTrnRowIndex(*profile, 199) == MAXDWORD);
    CHECK(LegacyRandTrnRowIndex(*profile, 200) == 199);
    CHECK(LegacyRandTrnRowIndex(*profile, 201) == MAXDWORD);
    CHECK(LegacyRandTrnRowIndex(*profile, MAXDWORD) == MAXDWORD);
    CHECK(RandTrnTerrain(199) == 3);
    CHECK(RandTrnTerrain(200) == 7);
    CHECK(RandTrnTerrain(201) == kNoReferenceRow);

    s_workbookFilterEnabled = true;
    for (DWORD row = 0; row < 201; ++row)
    {
        const DWORD legacy = LegacyRandTrnRowIndex(*profile, row);
        const bool expected = legacy == MAXDWORD || kAllowedRandTrnRows[legacy] != 0;
        CHECK(ShouldAllowRandTrnRow(reinterpret_cast<DWORD>(&row)) == expected);
    }
    SetEnvironmentVariableA(kExtraBlockedObstacleIdsEnvVar, "200,201,202");
    InitializeExtraBlockedRows();
    CHECK(s_extraBlockedRowCount == 2);
    CHECK(s_extraBlockedRandTrnRows[199] && s_extraBlockedRandTrnRows[200]);
    DWORD row = 200;
    CHECK(!ShouldAllowRandTrnRow(reinterpret_cast<DWORD>(&row)));
    s_workbookFilterEnabled = false;
    CHECK(!ShouldAllowRandTrnRow(reinterpret_cast<DWORD>(&row)));
    s_extraBlockedRandTrnRows[200] = 0;
    CHECK(ShouldAllowRandTrnRow(reinterpret_cast<DWORD>(&row)));
    row = 201;
    CHECK(ShouldAllowRandTrnRow(reinterpret_cast<DWORD>(&row)));

    DWORD wrapper[5] = {};
    wrapper[4] = reinterpret_cast<DWORD>(&row);
    DWORD actual = 0;
    CHECK(!TryReadCandidateRecordRowId(reinterpret_cast<DWORD>(wrapper), actual));
    row = 200;
    CHECK(TryReadCandidateRecordRowId(reinterpret_cast<DWORD>(wrapper), actual));
    CHECK(actual == 200);

    TestSelectedRules();
    TestParserThunks();
    TestPredicateThunk();
    std::puts("PASS: terrain geometry, 1.8.1 profiles, byte guards, row filters/indexing, and x86 hook thunks.");
    return 0;
}
