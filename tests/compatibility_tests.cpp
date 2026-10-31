// Include the implementation to exercise internal hook thunks without test exports.
#include "../plugin/obstacle_filter.cpp"
#include "../plugin/obstacle_selection_geometry.h"
#include "../plugin/minimap.cpp"

#include <cstdio>
#include <cstdlib>

#define CHECK(expression) do { if (!(expression)) { \
    std::fprintf(stderr, "FAILED line %d: %s\n", __LINE__, #expression); \
    std::abort(); } } while (false)

namespace
{
    void TestAllTerrainGeneration()
    {
        SetCustomObstacleMode(true);
        SetAllObstacleTerrains(false);
        CHECK(EffectiveObstacleTerrainMask(1) == 1);
        DWORD source[19] = {};
        int weights[12], adjacent[201], overlap[201];
        for (int& value : weights) value = -5000;
        weights[0] = 10;
        for (int& value : adjacent) value = -5000;
        for (int& value : overlap) value = -5000;
        DWORD holder = reinterpret_cast<DWORD>(weights);
        source[0] = 6;
        source[1] = reinterpret_cast<DWORD>(&holder);
        source[12] = reinterpret_cast<DWORD>(adjacent);
        source[13] = source[12] + sizeof(adjacent);
        source[16] = reinterpret_cast<DWORD>(overlap);
        source[17] = source[16] + sizeof(overlap);
        DWORD object[19] = {};
        object[0] = 7300; object[5] = 1; object[6] = 1; object[7] = 134;
        RegisterObstacleButtonDefName(reinterpret_cast<DWORD>(object), reinterpret_cast<DWORD>("avlmtdr1.def"));
        DWORD wrapper[5] = { reinterpret_cast<DWORD>(object), 0, 0, 0, reinterpret_cast<DWORD>(source) };
        SetObstacleButtonSelection(MAXDWORD, MAXDWORD, MAXDWORD, MAXDWORD);
        SetObstacleGenerationFilterMode(ObstacleGenerationFilterMode::ButtonSelection);
        ApplyExtraObjectRandTrnFallback(reinterpret_cast<DWORD>(wrapper));
        CHECK(object[5] == 1 && object[6] == 1);
        CHECK(ShouldAllowFinalCandidateRecord(reinterpret_cast<DWORD>(wrapper)));
        CHECK(wrapper[4] == reinterpret_cast<DWORD>(source));
        SetObstacleGenerationFilterMode(ObstacleGenerationFilterMode::None);
        SetAllObstacleTerrains(true);
        CHECK(EffectiveObstacleTerrainMask(1) == kAllObstacleTerrainMask);
        SetObstacleGenerationFilterMode(ObstacleGenerationFilterMode::ButtonSelection);
        CHECK(s_generationAllTerrains);
        SetObstacleButtonSelection(0, 0, 0, 0);
        ApplyExtraObjectRandTrnFallback(reinterpret_cast<DWORD>(wrapper));
        CHECK(object[5] == 1 && object[6] == 1);
        CHECK(!ShouldAllowFinalCandidateRecord(reinterpret_cast<DWORD>(wrapper)));
        SetObstacleButtonSelection(MAXDWORD, MAXDWORD, MAXDWORD, MAXDWORD);
        ApplyExtraObjectRandTrnFallback(reinterpret_cast<DWORD>(wrapper));
        CHECK(object[5] == kAllObstacleTerrainMask && object[6] == kAllObstacleTerrainMask);
        CHECK(wrapper[1] == 0 && wrapper[4] == reinterpret_cast<DWORD>(source));
        CHECK(ShouldAllowFinalCandidateRecord(reinterpret_cast<DWORD>(wrapper)));
        const auto& rules = s_selectedObjectRules.at(reinterpret_cast<DWORD>(object));
        for (int terrain = 0; terrain < 12; ++terrain)
        {
            CHECK(rules.weights[terrain] == (terrain == 9 ? -5000 : terrain == 0 ? 10 : 5));
            CHECK(weights[terrain] == (terrain == 0 ? 10 : -5000));
        }
        for (int index = 0; index < 201; ++index) CHECK(rules.overlap[index] == overlap[index]);
        SetAllObstacleTerrains(false); // The active run keeps its captured option.
        CHECK(s_generationAllTerrains && EffectiveObstacleTerrainMask(1) == 1);
        s_allTerrainTemplateFailure = true;
        CHECK(!ShouldAllowFinalCandidateRecord(reinterpret_cast<DWORD>(wrapper)));
        SetObstacleGenerationFilterMode(ObstacleGenerationFilterMode::None);
        CHECK(!s_generationAllTerrains);
        object[5] = object[6] = 1;
        wrapper[4] = reinterpret_cast<DWORD>(source);
        SetObstacleGenerationFilterMode(ObstacleGenerationFilterMode::ButtonSelection);
        CHECK(ShouldAllowFinalCandidateRecord(reinterpret_cast<DWORD>(wrapper)));
        CHECK(wrapper[4] == reinterpret_cast<DWORD>(source));
        SetObstacleGenerationFilterMode(ObstacleGenerationFilterMode::None);
        SetAllObstacleTerrains(true);
        SetCustomObstacleMode(false);
        CHECK(!AllObstacleTerrainsEnabled() && EffectiveObstacleTerrainMask(1) == 1);
        SetObstacleGenerationFilterMode(ObstacleGenerationFilterMode::ButtonSelection);
        CHECK(!s_generationAllTerrains);
        CHECK(ShouldAllowFinalCandidateRecord(reinterpret_cast<DWORD>(wrapper)));
        SetAllObstacleTerrains(false);
        SetCustomObstacleMode(true);
    }

    void TestButtonSelectionCapacity()
    {
        DWORD object[19] = {};
        DWORD row[19] = {};
        int weights[12], adjacent[201] = {}, overlap[201] = {};
        for (int& weight : weights) weight = 10;
        DWORD holder = reinterpret_cast<DWORD>(weights);
        row[1] = reinterpret_cast<DWORD>(&holder);
        row[12] = reinterpret_cast<DWORD>(adjacent);
        row[13] = row[12] + sizeof(adjacent);
        row[16] = reinterpret_cast<DWORD>(overlap);
        row[17] = row[16] + sizeof(overlap);
        DWORD candidate[5] = {};
        object[0] = 7200;
        candidate[0] = reinterpret_cast<DWORD>(object);
        candidate[4] = reinterpret_cast<DWORD>(row);
        SetObstacleGenerationFilterMode(ObstacleGenerationFilterMode::ButtonSelection);
        for (int selected = 0; selected < kObstacleButtonCount; ++selected)
        {
            DWORD masks[4] = {};
            masks[selected / 32] = 1u << (selected & 31);
            SetObstacleButtonSelection(masks[0], masks[1], masks[2], masks[3]);
            CHECK(!IsObstacleButtonSelected(-1));
            CHECK(!IsObstacleButtonSelected(kObstacleButtonCount));
            for (int index = 0; index < kObstacleButtonCount; ++index)
            {
                CHECK(IsObstacleButtonSelected(index) == (index == selected));
                const auto* button = ObstacleButtonDefinitionAt(index);
                s_selectedObjectRules.clear();
                candidate[4] = reinterpret_cast<DWORD>(row);
                object[6] = button->terrainMask;
                RegisterObstacleButtonDefName(reinterpret_cast<DWORD>(object),
                    reinterpret_cast<DWORD>(button->iconDefName));
                CHECK(ObstacleButtonIndexForObjectTemplate(reinterpret_cast<DWORD>(object)) == index);
                CHECK(ShouldAllowFinalCandidateRecord(reinterpret_cast<DWORD>(candidate)) == (index == selected));
            }
        }
        s_selectedObjectRules.clear();
        SetObstacleButtonSelection(0, 0, 0, 0);
        SetObstacleGenerationFilterMode(ObstacleGenerationFilterMode::None);
        CHECK(ShouldAllowFinalCandidateRecord(reinterpret_cast<DWORD>(candidate)));
    }

    void TestNonPositiveAllTerrainWeights()
    {
        DWORD row[19] = {};
        int weights[12], adjacent[201], overlap[201];
        for (int& weight : weights) weight = 10;
        weights[0] = 0; // Grass mountain native Dirt weight.
        weights[1] = -7; // Soft penalty, not a forbidden-terrain sentinel.
        weights[2] = 5;
        weights[9] = 20; // Void remains forbidden even with a positive source weight.
        for (int& value : adjacent) value = -5000;
        for (int& value : overlap) value = -5000;
        adjacent[38] = 30; overlap[38] = 30;
        DWORD holder = reinterpret_cast<DWORD>(weights);
        row[0] = 38; row[1] = reinterpret_cast<DWORD>(&holder);
        row[12] = reinterpret_cast<DWORD>(adjacent); row[13] = row[12] + sizeof(adjacent);
        row[16] = reinterpret_cast<DWORD>(overlap); row[17] = row[16] + sizeof(overlap);
        for (bool expanded : { false, true, false })
        {
            SelectedObjectRules rules;
            CHECK(BuildSelectedObjectRules(reinterpret_cast<DWORD>(row), 0xFFF, rules, expanded));
            CHECK(rules.weights[0] == (expanded ? 5 : 0));
            CHECK(rules.weights[1] == (expanded ? 5 : -7));
            CHECK(rules.weights[2] == 5 && rules.weights[3] == 10);
            CHECK(rules.weights[9] == -5000);
            CHECK(rules.adjacent[0] == (expanded ? 5 : -5000));
            CHECK(rules.adjacent[38] == 30);
            for (int i = 0; i < 201; ++i) CHECK(rules.overlap[i] == overlap[i]);
            CHECK(weights[0] == 0 && weights[1] == -7 && weights[9] == 20);
            CHECK(adjacent[0] == -5000 && row[0] == 38);
        }
        SelectedObjectRules grassOnly;
        CHECK(BuildSelectedObjectRules(reinterpret_cast<DWORD>(row), 4, grassOnly, true));
        CHECK(grassOnly.weights[0] == -5000 && grassOnly.weights[1] == -5000 && grassOnly.weights[2] == 5);
    }

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
        CHECK(BuildSelectedObjectRules(reinterpret_cast<DWORD>(source), 0xCFF, rules, false));
        for (DWORD i = 0; i < 12; ++i)
            CHECK(rules.weights[i] == ((i == 8 || i == 9) ? -5000 : 5));
        CHECK(rules.adjacent[0] == 5 && rules.overlap[0] == -5000);
        CHECK(weights[0] == -5000 && adjacent[0] == -5000);
        CHECK(rules.row[1] == reinterpret_cast<DWORD>(&rules.weightPointer));
        CHECK(BuildSelectedObjectRules(reinterpret_cast<DWORD>(source), 1u << 4, rules, false));
        CHECK(rules.adjacent[0] == -5000);
        CHECK(!BuildSelectedObjectRules(0, 0xCFF, rules, false));
        source[13] -= 4;
        CHECK(!BuildSelectedObjectRules(reinterpret_cast<DWORD>(source), 0xCFF, rules, false));
        source[13] += 4;
        DWORD object[19] = {};
        DWORD candidate[5] = {};
        object[0] = 7000;
        object[6] = 0xCFF;
        RegisterObstacleButtonDefName(reinterpret_cast<DWORD>(object), reinterpret_cast<DWORD>("crates04.def"));
        candidate[0] = reinterpret_cast<DWORD>(object);
        SetObstacleGenerationFilterMode(ObstacleGenerationFilterMode::ButtonSelection);
        s_defaultSelectedRow = reinterpret_cast<DWORD>(source);
        SetObstacleButtonSelection(0, 0, 0, 0);
        CHECK(!ShouldAllowFinalCandidateRecord(reinterpret_cast<DWORD>(candidate)));
        CHECK(candidate[4] == 0);
        SetObstacleButtonSelection(MAXDWORD, MAXDWORD, MAXDWORD, MAXDWORD);
        CHECK(ShouldAllowFinalCandidateRecord(reinterpret_cast<DWORD>(candidate)));
        CHECK(candidate[4] != 0);
        const DWORD privateRow = candidate[4];
        CHECK(ShouldAllowFinalCandidateRecord(reinterpret_cast<DWORD>(candidate)));
        CHECK(candidate[4] == privateRow);
        SetObstacleGenerationFilterMode(ObstacleGenerationFilterMode::ButtonSelection);
        s_defaultSelectedRow = reinterpret_cast<DWORD>(source);
        object[0] = 7001;
        object[6] = 1;
        RegisterObstacleButtonDefName(reinterpret_cast<DWORD>(object), reinterpret_cast<DWORD>("avlswt22.def"));
        candidate[4] = 0;
        CHECK(ShouldAllowFinalCandidateRecord(reinterpret_cast<DWORD>(candidate)));
        CHECK(candidate[4] != 0);
        const auto& maple = s_selectedObjectRules.at(reinterpret_cast<DWORD>(object));
        CHECK(maple.weights[0] == 5);
        for (int terrain = 1; terrain < 12; ++terrain) CHECK(maple.weights[terrain] == -5000);
        candidate[4] = reinterpret_cast<DWORD>(source);
        CHECK(ShouldAllowFinalCandidateRecord(reinterpret_cast<DWORD>(candidate)));
        CHECK(candidate[4] == reinterpret_cast<DWORD>(source));
        SetObstacleGenerationFilterMode(ObstacleGenerationFilterMode::ButtonSelection);
        object[7] = 140; object[8] = 6; object[6] = 2;
        source[0] = 165; weights[1] = 10;
        candidate[4] = reinterpret_cast<DWORD>(source);
        CHECK(ApplySelectedObjectRules(reinterpret_cast<DWORD>(candidate),
            reinterpret_cast<DWORD>(object), 2));
        const auto& dune = s_selectedObjectRules.at(reinterpret_cast<DWORD>(object));
        CHECK(candidate[4] == reinterpret_cast<DWORD>(dune.row));
        CHECK(dune.overlap[165] == 30 && overlap[165] == -5000);
        for (int i = 0; i < 201; ++i)
        {
            if (i != 165) CHECK(dune.overlap[i] == overlap[i]);
            CHECK(dune.adjacent[i] == adjacent[i]);
        }
        object[8] = 12;
        CHECK(!IsSandDuneTemplate(reinterpret_cast<DWORD>(object)));
        SetObstacleGenerationFilterMode(ObstacleGenerationFilterMode::None);
        candidate[4] = 0;
        CHECK(ShouldAllowFinalCandidateRecord(reinterpret_cast<DWORD>(candidate)));
        CHECK(candidate[4] == 0);
    }
    void TestSnowHillRules()
    {
        DWORD source[19] = {}, object[19] = {}, candidate[5] = {};
        int weights[12], adjacent[201], overlap[201];
        for (int& value : weights) value = -5000;
        weights[3] = 5;
        for (int& value : adjacent) value = 5;
        for (int& value : overlap) value = -5000;
        DWORD holder = reinterpret_cast<DWORD>(weights);
        source[0] = 162; source[1] = reinterpret_cast<DWORD>(&holder);
        source[12] = reinterpret_cast<DWORD>(adjacent); source[13] = source[12] + sizeof(adjacent);
        source[16] = reinterpret_cast<DWORD>(overlap); source[17] = source[16] + sizeof(overlap);
        object[0] = 8008; object[7] = 140; object[8] = 5; object[6] = 1u << 3;
        candidate[0] = reinterpret_cast<DWORD>(object);
        SetObstacleButtonSelection(MAXDWORD, MAXDWORD, MAXDWORD, MAXDWORD);
        for (bool allTerrains : { false, true, false })
        {
            for (const char* name : { "avlxsn01.def", "avlxsn02.def", "avlxsn03.def" })
            {
                SetAllObstacleTerrains(allTerrains);
                SetObstacleGenerationFilterMode(ObstacleGenerationFilterMode::ButtonSelection);
                RegisterObstacleButtonDefName(reinterpret_cast<DWORD>(object), reinterpret_cast<DWORD>(name));
                candidate[4] = reinterpret_cast<DWORD>(source);
                CHECK(ShouldAllowFinalCandidateRecord(reinterpret_cast<DWORD>(candidate)));
                CHECK(IsSnowHillTemplate(reinterpret_cast<DWORD>(object)));
                const auto& rules = s_selectedObjectRules.at(reinterpret_cast<DWORD>(object));
                CHECK(candidate[4] == reinterpret_cast<DWORD>(rules.row));
                CHECK(rules.row[0] == 162 && rules.overlap[162] == 30);
                for (int i = 0; i < 201; ++i)
                {
                    if (i != 162) CHECK(rules.overlap[i] == overlap[i]);
                    CHECK(rules.adjacent[i] == adjacent[i] && overlap[i] == -5000);
                }
                for (int terrain = 0; terrain < 12; ++terrain)
                    CHECK(rules.weights[terrain] == (terrain != 9 && (allTerrains || terrain == 3) ? 5 : -5000));
                CHECK(weights[0] == -5000 && weights[3] == 5);
            }
        }
        SetObstacleGenerationFilterMode(ObstacleGenerationFilterMode::ButtonSelection);
        SetObstacleButtonSelection(0, 0, 0, 0);
        candidate[4] = reinterpret_cast<DWORD>(source);
        CHECK(!ShouldAllowFinalCandidateRecord(reinterpret_cast<DWORD>(candidate)));
        CHECK(candidate[4] == reinterpret_cast<DWORD>(source));
        SetObstacleGenerationFilterMode(ObstacleGenerationFilterMode::None);
        CHECK(ShouldAllowFinalCandidateRecord(reinterpret_cast<DWORD>(candidate)));
        CHECK(candidate[4] == reinterpret_cast<DWORD>(source) && s_selectedObjectRules.empty());
        object[8] = 6; CHECK(!IsSnowHillTemplate(reinterpret_cast<DWORD>(object)));
        object[8] = 5; object[7] = 148; CHECK(!IsSnowHillTemplate(reinterpret_cast<DWORD>(object)));
        SetObstacleButtonSelection(MAXDWORD, MAXDWORD, MAXDWORD, MAXDWORD);
    }

    DWORD s_fillAttemptMask = 0;
    BOOL __fastcall DuneCellEligibleStub(DWORD cell, DWORD)
    {
        return (reinterpret_cast<DWORD*>(cell)[10] & 0x02000000) != 0;
    }
    void __fastcall DuneFillAttemptStub(DWORD generator, DWORD, int x, int, int, int)
    {
        s_fillAttemptMask |= 1u << x;
        auto* cells = reinterpret_cast<DWORD*>(reinterpret_cast<DWORD*>(generator)[5]);
        cells[12 * x + 10] &= ~0x02000000u;
    }

    void TestAllTerrainDuneFill()
    {
        SetCustomObstacleMode(true);
        DWORD generator[9] = {};
        DWORD cells[12][12] = {};
        generator[5] = reinterpret_cast<DWORD>(cells);
        generator[6] = 12; generator[7] = generator[8] = 1;
        const DWORD oldGenerate = s_generateObstacleCell, oldEligible = s_obstacleCellEligible;
        s_generateObstacleCell = reinterpret_cast<DWORD>(&DuneFillAttemptStub);
        s_obstacleCellEligible = reinterpret_cast<DWORD>(&DuneCellEligibleStub);
        for (bool enabled : { false, true, false })
        {
            SetAllObstacleTerrains(enabled);
            SetObstacleButtonSelection(4, 0, 0, 0); // Dunes only; Sand Hills unchecked.
            SetObstacleGenerationFilterMode(ObstacleGenerationFilterMode::ButtonSelection);
            for (DWORD terrain = 0; terrain < 12; ++terrain)
            {
                cells[terrain][9] = terrain | (23 << 6); // Transition view must not change actual terrain.
                cells[terrain][10] = 0x06000000;
                CHECK(DuneFillTerrainAllowed(terrain) == (terrain != 9 && (enabled || terrain == 1)));
            }
            CHECK(!DuneFillTerrainAllowed(12));
            cells[2][10] |= 0x08000000; // Existing blocked tile.
            cells[3][10] = 0x02000000;  // Not painted.
            cells[10][10] = 0x04000000; // Painted, but native eligibility rejects it.
            CaptureDunePaint(reinterpret_cast<DWORD>(generator));
            CHECK(!s_dunePaintedCells[9] && !s_dunePaintedCells[2] && !s_dunePaintedCells[3]);
            s_fillAttemptMask = 0;
            FillDuneGaps(reinterpret_cast<DWORD>(generator));
            const DWORD expected = enabled ? (0xDFFu & ~((1u<<2)|(1u<<3)|(1u<<10))) : 2;
            CHECK(s_fillAttemptMask == expected);
            CHECK(cells[9][10] == 0x06000000 && cells[2][10] == 0x0E000000 && cells[3][10] == 0x02000000);
            CHECK(!s_duneFillActive && !s_duneCapturedGenerator);
            FillDuneGaps(reinterpret_cast<DWORD>(generator));
            CHECK(s_fillAttemptMask == expected); // Snapshot cannot be reused.
            SetObstacleGenerationFilterMode(ObstacleGenerationFilterMode::None);
        }
        s_generateObstacleCell = oldGenerate; s_obstacleCellEligible = oldEligible;

        DWORD object[19] = {}, row[19] = {}, candidate[5] = {};
        int weights[12], adjacent[201], overlap[201];
        for (int& w : weights) w = -5000; weights[1] = 10;
        for (int& w : adjacent) w = -5000;
        for (int& w : overlap) w = -5000;
        overlap[165] = 10; // Native small-dune/large-dune overlap allowance.
        DWORD holder = reinterpret_cast<DWORD>(weights);
        row[0] = 21; row[1] = reinterpret_cast<DWORD>(&holder);
        row[12] = reinterpret_cast<DWORD>(adjacent); row[13] = row[12] + sizeof(adjacent);
        row[16] = reinterpret_cast<DWORD>(overlap); row[17] = row[16] + sizeof(overlap);
        object[0] = 7101; object[5] = 0xDFF; object[6] = 2; object[7] = 148;
        RegisterObstacleButtonDefName(reinterpret_cast<DWORD>(object), reinterpret_cast<DWORD>("AVLdun10.def"));
        candidate[0] = reinterpret_cast<DWORD>(object); candidate[1] = 1; candidate[4] = reinterpret_cast<DWORD>(row);
        SetAllObstacleTerrains(true);
        SetObstacleGenerationFilterMode(ObstacleGenerationFilterMode::ButtonSelection);
        SetObstacleButtonSelection(0, 0, 0, 0);
        ApplyExtraObjectRandTrnFallback(reinterpret_cast<DWORD>(candidate));
        CHECK(object[6] == 2); // No parent selection: do not widen unselected filler.
        SetObstacleButtonSelection(4, 0, 0, 0);
        ApplyExtraObjectRandTrnFallback(reinterpret_cast<DWORD>(candidate));
        CHECK(object[5] == 0xDFF && object[6] == 0xDFF);
        CHECK(candidate[1] == 1 && candidate[4] == reinterpret_cast<DWORD>(row));
        CHECK(!ShouldAllowFinalCandidateRecord(reinterpret_cast<DWORD>(candidate))); // Main pass defers filler.
        s_duneFillActive = true;
        CHECK(ShouldAllowFinalCandidateRecord(reinterpret_cast<DWORD>(candidate)));
        CHECK(candidate[4] != reinterpret_cast<DWORD>(row));
        for (DWORD terrain = 0; terrain < 12; ++terrain)
            CHECK(SelectedTerrainEdgeAllowed(candidate[4], terrain) == (terrain != 9));
        const auto& rules = s_selectedObjectRules.at(reinterpret_cast<DWORD>(object));
        for (DWORD terrain = 0; terrain < 12; ++terrain)
            CHECK(rules.weights[terrain] == (terrain == 9 ? -5000 : terrain == 1 ? 10 : 5));
        for (int i = 0; i < 201; ++i) CHECK(rules.overlap[i] == overlap[i]);
        CHECK(weights[7] == -5000 && overlap[165] == 10);
        s_duneFillActive = false;
        CHECK(!SelectedTerrainEdgeAllowed(candidate[4], 7)); // Parent is only used during filler pass.
        SetObstacleGenerationFilterMode(ObstacleGenerationFilterMode::None);
        SetAllObstacleTerrains(false);
        SetCustomObstacleMode(false);
        SetObstacleGenerationFilterMode(ObstacleGenerationFilterMode::ButtonSelection);
        CaptureDunePaint(reinterpret_cast<DWORD>(generator));
        CHECK(!s_duneCapturedGenerator && ShouldAllowFinalCandidateRecord(reinterpret_cast<DWORD>(candidate)));
        SetCustomObstacleMode(true);
    }

    void TestDuneFillPolicy()
    {
        DWORD object[19] = {};
        DWORD row[19] = {};
        DWORD candidate[5] = {};
        object[0] = 7100; object[7] = 148; object[8] = 0; object[6] = 2;
        candidate[0] = reinterpret_cast<DWORD>(object);
        candidate[4] = reinterpret_cast<DWORD>(row);
        RegisterObstacleButtonDefName(reinterpret_cast<DWORD>(object),
            reinterpret_cast<DWORD>("AVLdun10.def"));
        SetObstacleGenerationFilterMode(ObstacleGenerationFilterMode::ButtonSelection);
        SetObstacleButtonSelection(MAXDWORD, MAXDWORD, MAXDWORD, MAXDWORD);
        CHECK(!ShouldAllowFinalCandidateRecord(reinterpret_cast<DWORD>(candidate)));
        SetObstacleButtonSelection(4, 0, 0, 0);
        s_duneFillActive = true;
        CHECK(ShouldAllowFinalCandidateRecord(reinterpret_cast<DWORD>(candidate)));
        object[8] = 1; CHECK(!ShouldAllowFinalCandidateRecord(reinterpret_cast<DWORD>(candidate)));
        object[7] = 140; object[8] = 6;
        CHECK(!ShouldAllowFinalCandidateRecord(reinterpret_cast<DWORD>(candidate)));
        object[7] = 148; object[8] = 0;
        s_duneFillActive = false;
        SetObstacleButtonSelection(MAXDWORD & ~4u, MAXDWORD, MAXDWORD, MAXDWORD);
        CHECK(ShouldAllowFinalCandidateRecord(reinterpret_cast<DWORD>(candidate)));

        DWORD generator[9] = {};
        DWORD cells[4][12] = {};
        generator[5] = reinterpret_cast<DWORD>(cells);
        generator[6] = 4; generator[7] = 1; generator[8] = 1;
        for (auto& cell : cells) { cell[9] = 1; cell[10] = 0x06000000; }
        cells[1][9] = 0;
        cells[2][10] |= 0x08000000;
        cells[3][10] = 0x02000000;
        SetObstacleButtonSelection(4, 0, 0, 0);
        CaptureDunePaint(reinterpret_cast<DWORD>(generator));
        CHECK(s_duneCapturedGenerator == reinterpret_cast<DWORD>(generator));
        CHECK(s_dunePaintedCells[0] && !s_dunePaintedCells[1] &&
            !s_dunePaintedCells[2] && !s_dunePaintedCells[3]);
        SetObstacleButtonSelection(0, 0, 0, 0);
        CaptureDunePaint(reinterpret_cast<DWORD>(generator));
        CHECK(!s_duneCapturedGenerator);
        SetObstacleGenerationFilterMode(ObstacleGenerationFilterMode::None);
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

    __declspec(naked) void PredicateRejectStub()
    {
        __asm { pop esi }
        __asm { xor al, al }
        __asm { ret 4 }
    }

    __declspec(naked) void AccumulatorResumeStub()
    {
        __asm { mov eax, 1234h }
        __asm { ret 8 }
    }

    void TestMountainOrder()
    {
        DWORD candidate[19] = {}, other[19] = {}, row[19] = {}, otherRow[19] = {};
        DWORD wrapper[5] = {}, object[7] = {};
        int overlaps[201] = {};
        candidate[7] = other[7] = 134;
        otherRow[0] = 6; overlaps[6] = 30;
        row[16] = reinterpret_cast<DWORD>(overlaps);
        wrapper[0] = reinterpret_cast<DWORD>(other);
        wrapper[4] = reinterpret_cast<DWORD>(otherRow);
        object[1] = reinterpret_cast<DWORD>(wrapper);
        s_candidateObjectTemplate = reinterpret_cast<DWORD>(candidate);
        s_traceCandidateRow = reinterpret_cast<DWORD>(row);
        s_traceCandidateButton = 0;
        s_generationFilterMode = static_cast<LONG>(ObstacleGenerationFilterMode::ButtonSelection);
        SetObstacleButtonSelection(MAXDWORD, MAXDWORD, MAXDWORD, MAXDWORD);
        auto allowed = [&]() { return AllowMountainOrder(reinterpret_cast<DWORD>(candidate),
            reinterpret_cast<DWORD>(row), reinterpret_cast<DWORD>(object)); };
        CHECK(allowed());
        candidate[0] = 8000; other[0] = 8001;
        RegisterObstacleButtonDefName(reinterpret_cast<DWORD>(candidate), reinterpret_cast<DWORD>("avlmhs00.def"));
        RegisterObstacleButtonDefName(reinterpret_cast<DWORD>(other), reinterpret_cast<DWORD>("AVLwtf00.def"));
        s_traceCandidateButton = ObstacleButtonIndexForObjectTemplate(reinterpret_cast<DWORD>(candidate));
        other[7] = 139; other[8] = 17;
        for (const char* name : { "AVLwtf00.def", "AVLwtf01.def", "AVLwtf02.def" })
        {
            RegisterObstacleButtonDefName(reinterpret_cast<DWORD>(other), reinterpret_cast<DWORD>(name));
            CHECK(allowed());
        }
        other[8] = 18; CHECK(!allowed()); other[8] = 17;
        overlaps[6] = -5000; CHECK(!allowed()); overlaps[6] = 30;
        SetObstacleButtonSelection(0, 0, 0, 0); CHECK(!allowed());
        SetObstacleButtonSelection(MAXDWORD, MAXDWORD, MAXDWORD, MAXDWORD);
        s_generationFilterMode = 0; CHECK(!allowed());
        s_generationFilterMode = static_cast<LONG>(ObstacleGenerationFilterMode::ButtonSelection);
        s_traceCandidateButton = 0; CHECK(!allowed());
        s_traceCandidateButton = ObstacleButtonIndexForObjectTemplate(reinterpret_cast<DWORD>(candidate));
        RegisterObstacleButtonDefName(reinterpret_cast<DWORD>(other), reinterpret_cast<DWORD>("avlmtdr1.def"));
        CHECK(!allowed());
        other[7] = 134; CHECK(allowed());
        s_traceCandidateButton = 0;
        other[7] = 148; CHECK(!allowed()); other[7] = 134;
        candidate[7] = 140; CHECK(!allowed()); candidate[7] = 134;
        candidate[7] = other[7] = 140;
        candidate[8] = other[8] = 6; CHECK(!allowed());
        other[8] = 12; CHECK(!allowed()); other[8] = 6;
        candidate[8] = 12; CHECK(!allowed()); candidate[8] = 6;
        overlaps[6] = -5000; CHECK(!allowed()); overlaps[6] = 30;
        other[7] = 134; CHECK(!allowed());
        candidate[7] = 134; other[7] = 140; CHECK(!allowed());
        other[7] = 134;
        overlaps[6] = -5000; CHECK(!allowed()); overlaps[6] = 30;
        otherRow[0] = 201; CHECK(!allowed()); otherRow[0] = 6;
        SetObstacleButtonSelection(0, 0, 0, 0); CHECK(!allowed());
        SetObstacleButtonSelection(MAXDWORD, MAXDWORD, MAXDWORD, MAXDWORD);
        s_generationFilterMode = 0; CHECK(!allowed());
        s_generationFilterMode = static_cast<LONG>(ObstacleGenerationFilterMode::ButtonSelection);
        s_candidateObjectTemplate = 0; CHECK(!allowed());
        s_generationFilterMode = 0;
        ClearVariantTraceRows();
    }

    void TestGlacierOrder()
    {
        DWORD candidate[19] = {}, other[19] = {}, row[19] = {}, otherRow[19] = {};
        DWORD wrapper[5] = {}, object[7] = {};
        int overlaps[201] = {};
        candidate[0] = 8002; other[0] = 8003;
        candidate[7] = other[7] = 140;
        candidate[8] = other[8] = 14;
        otherRow[0] = 56; overlaps[56] = 30;
        row[16] = reinterpret_cast<DWORD>(overlaps);
        wrapper[0] = reinterpret_cast<DWORD>(other);
        wrapper[4] = reinterpret_cast<DWORD>(otherRow);
        object[1] = reinterpret_cast<DWORD>(wrapper);
        s_candidateObjectTemplate = reinterpret_cast<DWORD>(candidate);
        s_traceCandidateRow = reinterpret_cast<DWORD>(row);
        s_generationFilterMode = static_cast<LONG>(ObstacleGenerationFilterMode::ButtonSelection);
        SetObstacleButtonSelection(MAXDWORD, MAXDWORD, MAXDWORD, MAXDWORD);
        auto allowed = [&]() { return AllowMountainOrder(reinterpret_cast<DWORD>(candidate),
            reinterpret_cast<DWORD>(row), reinterpret_cast<DWORD>(object)); };
        const char* names[] = { "AVLGLC01.def", "AVLGLC02.def", "AVLGLC03.def",
            "AVLGLC04.def", "AVLGLC05.def", "AVLGLC06.def" };
        for (const char* candidateName : names)
        {
            RegisterObstacleButtonDefName(reinterpret_cast<DWORD>(candidate), reinterpret_cast<DWORD>(candidateName));
            s_traceCandidateButton = ObstacleButtonIndexForObjectTemplate(reinterpret_cast<DWORD>(candidate));
            for (const char* otherName : names)
            {
                RegisterObstacleButtonDefName(reinterpret_cast<DWORD>(other), reinterpret_cast<DWORD>(otherName));
                CHECK(allowed());
            }
        }
        for (DWORD subtype : { 6u, 12u, 15u })
        {
            candidate[8] = subtype; CHECK(!allowed()); candidate[8] = 14;
            other[8] = subtype; CHECK(!allowed()); other[8] = 14;
        }
        other[7] = 134; CHECK(!allowed()); other[7] = 140;
        candidate[7] = 134; CHECK(!allowed()); candidate[7] = 140;
        overlaps[56] = -5000; CHECK(!allowed()); overlaps[56] = 30;
        otherRow[0] = 201; CHECK(!allowed()); otherRow[0] = 56;
        s_traceCandidateButton = 0; CHECK(!allowed());
        s_traceCandidateButton = ObstacleButtonIndexForObjectTemplate(reinterpret_cast<DWORD>(candidate));
        RegisterObstacleButtonDefName(reinterpret_cast<DWORD>(other), reinterpret_cast<DWORD>("avlmtdr1.def"));
        CHECK(!allowed());
        RegisterObstacleButtonDefName(reinterpret_cast<DWORD>(other), reinterpret_cast<DWORD>(names[0]));
        candidate[0] = 8004; CHECK(!allowed()); candidate[0] = 8002;
        other[0] = 8005; CHECK(!allowed()); other[0] = 8003;
        SetObstacleButtonSelection(0, 0, 0, 0); CHECK(!allowed());
        SetObstacleButtonSelection(MAXDWORD, MAXDWORD, MAXDWORD, MAXDWORD);
        s_traceCandidateRow = 0; CHECK(!allowed()); s_traceCandidateRow = reinterpret_cast<DWORD>(row);
        s_candidateObjectTemplate = 0; CHECK(!allowed());
        s_candidateObjectTemplate = reinterpret_cast<DWORD>(candidate);
        s_generationFilterMode = 0; CHECK(!allowed());
        ClearVariantTraceRows();
    }

    void TestIceArchOrder()
    {
        DWORD candidate[19] = {}, other[19] = {}, row[19] = {}, otherRow[19] = {};
        DWORD wrapper[5] = {}, object[7] = {};
        int overlaps[201] = {};
        candidate[0] = 8006; other[0] = 8007;
        candidate[7] = other[7] = 140;
        candidate[8] = other[8] = 3;
        otherRow[0] = 56; overlaps[56] = 30;
        row[16] = reinterpret_cast<DWORD>(overlaps);
        wrapper[0] = reinterpret_cast<DWORD>(other);
        wrapper[4] = reinterpret_cast<DWORD>(otherRow);
        object[1] = reinterpret_cast<DWORD>(wrapper);
        s_candidateObjectTemplate = reinterpret_cast<DWORD>(candidate);
        s_traceCandidateRow = reinterpret_cast<DWORD>(row);
        s_generationFilterMode = static_cast<LONG>(ObstacleGenerationFilterMode::ButtonSelection);
        SetObstacleButtonSelection(MAXDWORD, MAXDWORD, MAXDWORD, MAXDWORD);
        auto allowed = [&]() { return AllowMountainOrder(reinterpret_cast<DWORD>(candidate),
            reinterpret_cast<DWORD>(row), reinterpret_cast<DWORD>(object)); };
        const char* names[] = { "AVLifar0.def", "AVLifar1.def" };
        for (const char* candidateName : names)
        {
            RegisterObstacleButtonDefName(reinterpret_cast<DWORD>(candidate), reinterpret_cast<DWORD>(candidateName));
            s_traceCandidateButton = ObstacleButtonIndexForObjectTemplate(reinterpret_cast<DWORD>(candidate));
            for (const char* otherName : names)
            {
                RegisterObstacleButtonDefName(reinterpret_cast<DWORD>(other), reinterpret_cast<DWORD>(otherName));
                CHECK(allowed());
            }
        }
        for (DWORD subtype : { 6u, 13u, 14u })
        {
            candidate[8] = subtype; CHECK(!allowed()); candidate[8] = 3;
            other[8] = subtype; CHECK(!allowed()); other[8] = 3;
        }
        candidate[8] = other[8] = 13; CHECK(!allowed());
        candidate[8] = other[8] = 3;
        other[7] = 134; CHECK(!allowed()); other[7] = 140;
        candidate[7] = 134; CHECK(!allowed()); candidate[7] = 140;
        overlaps[56] = -5000; CHECK(!allowed()); overlaps[56] = 30;
        s_traceCandidateButton = 0; CHECK(!allowed());
        s_traceCandidateButton = ObstacleButtonIndexForObjectTemplate(reinterpret_cast<DWORD>(candidate));
        RegisterObstacleButtonDefName(reinterpret_cast<DWORD>(other), reinterpret_cast<DWORD>("avlmtwl7.def"));
        CHECK(!allowed());
        RegisterObstacleButtonDefName(reinterpret_cast<DWORD>(other), reinterpret_cast<DWORD>(names[0]));
        SetObstacleButtonSelection(0, 0, 0, 0); CHECK(!allowed());
        SetObstacleButtonSelection(MAXDWORD, MAXDWORD, MAXDWORD, MAXDWORD);
        s_generationFilterMode = 0; CHECK(!allowed());
        ClearVariantTraceRows();
    }

    void TestPredicateThunk()
    {
        DWORD weights[12] = {};
        DWORD weightsHolder = reinterpret_cast<DWORD>(weights);
        testRow[1] = reinterpret_cast<DWORD>(&weightsHolder);
        s_predicateResume = reinterpret_cast<DWORD>(PredicateResumeStub);
        const auto predicate = reinterpret_cast<bool (__fastcall*)(DWORD, DWORD, DWORD)>(
            HotaObstacleWeightPredicateHook);
        testRow[0] = 6;
        CHECK(predicate(reinterpret_cast<DWORD>(testRow), 0, 0));
        testRow[0] = 14;
        s_extraBlockedRandTrnRows[14] = 1;
        CHECK(!predicate(reinterpret_cast<DWORD>(testRow), 0, 0));
        s_extraBlockedRandTrnRows[14] = 0;
        testRow[0] = 6;
        s_variantTraceEnabled = true;
        s_generationFilterMode = static_cast<LONG>(ObstacleGenerationFilterMode::ButtonSelection);
        s_traceCandidateRow = reinterpret_cast<DWORD>(testRow);
        s_traceCandidateButton = 0;
        CHECK(TracedTerrainPredicate(reinterpret_cast<DWORD>(testRow), 0, 0));
        CHECK(s_terrainPredicateCounts[6][0][1] == 1);
        s_predicateResume = reinterpret_cast<DWORD>(PredicateRejectStub);
        CHECK(!TracedTerrainPredicate(reinterpret_cast<DWORD>(testRow), 0, 1));
        CHECK(s_terrainPredicateCounts[6][0][0] == 1);
        s_candidateTerrainMask = 1;
        SetObstacleButtonSelection(MAXDWORD, MAXDWORD, MAXDWORD, MAXDWORD);
        CHECK(TracedTerrainPredicate(reinterpret_cast<DWORD>(testRow), 0, 1));
        CHECK(s_edgeRelaxations == 1);
        BYTE flags[12] = {};
        flags[1] = 3;
        DWORD table = 0;
        s_terrainViewTableAddress = reinterpret_cast<DWORD>(&table);
        s_accumulatorResume = reinterpret_cast<DWORD>(AccumulatorResumeStub);
        CHECK(SelectedTerrainAccumulator(reinterpret_cast<DWORD>(testRow), 0, 5, flags) == 0);
        CHECK(flags[0] == 3 && flags[1] == 3);
        CHECK(s_accumulatorRelaxations == 1);
        CHECK(SelectedTerrainAccumulator(reinterpret_cast<DWORD>(testRow), 1, 5, flags) == 0x1234);
        CHECK(!AccumulateSelectedTerrain(reinterpret_cast<DWORD>(testRow), 9, flags));
        CHECK(!AccumulateSelectedTerrain(reinterpret_cast<DWORD>(testRow), 12, flags));
        CHECK(!AccumulateSelectedTerrain(reinterpret_cast<DWORD>(testRow) + 4, 0, flags));
        CHECK(!AccumulateSelectedTerrain(reinterpret_cast<DWORD>(testRow), 0, nullptr));
        for (int i = 2; i < 12; ++i) CHECK(flags[i] == 0);
        CHECK(!SelectedTerrainEdgeAllowed(reinterpret_cast<DWORD>(testRow), 1));
        CHECK(!SelectedTerrainEdgeAllowed(reinterpret_cast<DWORD>(testRow), 8));
        CHECK(!SelectedTerrainEdgeAllowed(reinterpret_cast<DWORD>(testRow), 9));
        CHECK(!SelectedTerrainEdgeAllowed(reinterpret_cast<DWORD>(testRow), 12));
        CHECK(!SelectedTerrainEdgeAllowed(reinterpret_cast<DWORD>(testRow) + 4, 0));
        weights[0] = static_cast<DWORD>(-4000);
        CHECK(!AccumulateSelectedTerrain(reinterpret_cast<DWORD>(testRow), 0, flags));
        CHECK(!TracedTerrainPredicate(reinterpret_cast<DWORD>(testRow), 0, 1));
        weights[0] = static_cast<DWORD>(-5000);
        CHECK(!SelectedTerrainEdgeAllowed(reinterpret_cast<DWORD>(testRow), 0));
        weights[0] = 10;
        SetObstacleButtonSelection(0, 0, 0, 0);
        CHECK(!AccumulateSelectedTerrain(reinterpret_cast<DWORD>(testRow), 0, flags));
        CHECK(!TracedTerrainPredicate(reinterpret_cast<DWORD>(testRow), 0, 1));
        SetObstacleButtonSelection(MAXDWORD, MAXDWORD, MAXDWORD, MAXDWORD);
        s_generationFilterMode = 0;
        CHECK(SelectedTerrainAccumulator(reinterpret_cast<DWORD>(testRow), 0, 5, flags) == 0x1234);
        CHECK(!TracedTerrainPredicate(reinterpret_cast<DWORD>(testRow), 0, 1));
        s_variantTraceEnabled = false;
        s_generationFilterMode = 0;
        ClearVariantTraceRows();
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

void TestObstacleSelectionLayers();
void TestObstacleCategorySections();
void TestObstacleModeControls();

DWORD minimapSeenIndex, minimapSeenFlags, minimapSeenEax;
__declspec(naked) void MinimapContinueStub()
{
    __asm {
        mov minimapSeenIndex, esi
        mov minimapSeenEax, eax
        pushfd
        pop minimapSeenFlags
        ret
    }
}

void TestMinimapModeThunk()
{
    s_colorContinue = reinterpret_cast<DWORD>(MinimapContinueStub);
    for (int custom = 0; custom < 2; ++custom)
    {
        SetCustomObstacleMode(custom != 0);
        for (DWORD objectType = 0; objectType <= 233; ++objectType)
        {
            __asm {
                pushad
                mov esi, objectType
                mov eax, 12345678h
                call MinimapColorHook
                popad
            }
            const DWORD expected = custom && objectType == 157 ? 158 : objectType;
            CHECK(minimapSeenIndex == expected);
            CHECK(minimapSeenEax == 0x12345678);
            CHECK((minimapSeenFlags & 1) == (expected < 232 ? 1u : 0u));
            CHECK((minimapSeenFlags & 0x40) == (expected == 232 ? 0x40u : 0u));
        }
    }
    SetCustomObstacleMode(false);
}

int main()
{
    CHECK(!CustomObstacleModeEnabled());
    TestMinimapModeThunk();
    TestObstacleSelectionLayers();
    TestObstacleCategorySections();
    TestObstacleModeControls();
    SetCustomObstacleMode(true);
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
    const auto* russianProfile = FindHotaModuleTargetProfile(
        "86D9F9471163F36F58AA9E42A3020C456C66D39344062F44224F2A7F7B20D72B");
    CHECK(russianProfile);
    CHECK(russianProfile->obstacleWeightPredicateRva == 0xCC210);
    CHECK(russianProfile->parserEntryRva == 0xDD490);
    CHECK(russianProfile->parserStoreRva == 0xDDB34);
    CHECK(russianProfile->parserFallbackRva == russianProfile->parserStoreRva + 13);
    CHECK(russianProfile->terrainAccumulatorRva == 0xCC2C0);
    CHECK(russianProfile->terrainViewTableRva == 0x17BAC4);
    CHECK(russianProfile->mountainOrderCheckRva == 0xCCC0F);
    CHECK(russianProfile->randTrnRowCount == 201);
    CHECK(russianProfile->parserFallbackExpected[3] == 0x14);
    CHECK(russianProfile->parserFallbackExpected[7] == 0x1C);
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

    // Default bypasses even explicitly configured diagnostic filters and cannot
    // start a custom generation pass or redirect the native object source.
    DWORD defaultRow = 14;
    s_extraBlockedRandTrnRows[14] = 1;
    CHECK(!ShouldAllowRandTrnRow(reinterpret_cast<DWORD>(&defaultRow)));
    SetCustomObstacleMode(false);
    CHECK(ShouldAllowRandTrnRow(reinterpret_cast<DWORD>(&defaultRow)));
    SetObstacleGenerationFilterMode(ObstacleGenerationFilterMode::ButtonSelection);
    CHECK(s_generationFilterMode == 0);
    CHECK(ShouldAllowFinalCandidateRecord(0));
    const char* nativeSource = "objects.txt";
    CHECK(MaybeRedirectObjectsTxtSource(nativeSource) == nativeSource);
    CHECK(!SelectedTerrainEdgeAllowed(0, 0));
    CHECK(!s_duneFillActive);
    s_extraBlockedRandTrnRows[14] = 0;
    SetCustomObstacleMode(true);

    CHECK(profile->randTrnRowCount == 201);
    for (DWORD row = 0; row < profile->randTrnRowCount; ++row)
        CHECK(ShouldAllowRandTrnRow(reinterpret_cast<DWORD>(&row)));
    SetEnvironmentVariableA(kExtraBlockedObstacleIdsEnvVar, "200,201,202");
    InitializeExtraBlockedRows();
    CHECK(s_extraBlockedRowCount == 2);
    CHECK(s_extraBlockedRandTrnRows[199] && s_extraBlockedRandTrnRows[200]);
    DWORD row = 200;
    CHECK(!ShouldAllowRandTrnRow(reinterpret_cast<DWORD>(&row)));
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

    extern void TestLocalization();
    TestLocalization();
    extern void TestRussianObstacleControls();
    TestRussianObstacleControls();
    extern void TestAboutUi();
    TestAboutUi();
    TestButtonSelectionCapacity();
    TestAllTerrainGeneration();
    TestSelectedRules();
    TestSnowHillRules();
    TestNonPositiveAllTerrainWeights();
    TestParserThunks();
    TestPredicateThunk();
    TestMountainOrder();
    TestGlacierOrder();
    TestIceArchOrder();
    TestDuneFillPolicy();
    TestAllTerrainDuneFill();
    std::puts("PASS: painted map layers, terrain geometry, 1.8.1 profiles, byte guards, row filters/bounds, and x86 hook thunks.");
    return 0;
}
