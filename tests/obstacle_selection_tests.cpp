// Exercise the production scanner with native-layout map headers and adapters.
#include "../plugin/obstacle_variants.cpp"
#include <cstdlib>

#define CHECK_SELECTION(expression) do { if (!(expression)) { \
    std::fprintf(stderr, "Selection test failed line %d: %s\n", __LINE__, #expression); \
    std::abort(); } } while (false)

namespace
{
    struct LayerFixture
    {
        DWORD terrain[4] = {};
        DWORD grid[5] = { 0, 6, 0, 0x1000, 0x1090 }; // 36x36
        DWORD tile = 0;
        int state = 0;
    };

    LayerFixture layers[2];
    DWORD collections[2];
    int collectionCalls;
    int stateCalls[2];

    DWORD __fastcall SelectLayer(void*, void*, int layer)
    {
        CHECK_SELECTION(layer >= 0 && layer < 2);
        ++collectionCalls;
        return reinterpret_cast<DWORD>(&collections[layer]);
    }

    DWORD* __fastcall ReadTile(void* collection, void*, int, int)
    {
        const int layer = static_cast<DWORD*>(collection) - collections;
        return &layers[layer].tile;
    }

    int __fastcall ReadState(void*, void*, unsigned int x, unsigned int y, unsigned char layer)
    {
        CHECK_SELECTION(layer < 2);
        ++stateCalls[layer];
        return x == 5 && y == 7 ? layers[layer].state : 0;
    }
}

void TestObstacleSelectionLayers()
{
    DWORD mapData[8] = {};
    DWORD manager = reinterpret_cast<DWORD>(mapData);
    DWORD mapContext[22] = {};
    DWORD context[39] = {};
    context[0x98 / 4] = 6;
    context[0x64 / 4] = 1; // Opaque to the injected state adapter.
    context[0x3C / 4] = reinterpret_cast<DWORD>(mapContext);
    mapContext[0x54 / 4] = reinterpret_cast<DWORD>(&manager);
    for (int layer = 0; layer < 2; ++layer)
    {
        layers[layer].terrain[2] = reinterpret_cast<DWORD>(layers[layer].grid);
        collections[layer] = reinterpret_cast<DWORD>(layers[layer].terrain);
    }
    const SelectionAccess access = {
        reinterpret_cast<SelectCollectionFn>(&SelectLayer),
        reinterpret_cast<TileAtFn>(&ReadTile),
        reinterpret_cast<CellStateAtFn>(&ReadState)
    };
    auto scan = [&]() { return ComputePaintedTerrainScan(reinterpret_cast<DWORD>(context), access); };

    // An underground-only selection was invisible to the surface-only scanner.
    mapData[4] = 1;
    layers[1].tile = 6; // Subterranean; surface at the same coordinate is Dirt.
    layers[1].state = 2;
    auto result = scan();
    CHECK_SELECTION(result.paintedCells == 1 && result.terrainMask == (1u << 6));
    CategoryEnablement enablement = { true, result.terrainMask };
    EnsureCategoryCheckedInitialized();
    CHECK_SELECTION(AnyCheckedApplicableCategory(enablement));
    bool checked[kObstacleButtonCount];
    memcpy(checked, s_categoryChecked, sizeof(checked));
    ZeroMemory(s_categoryChecked, sizeof(s_categoryChecked));
    CHECK_SELECTION(!AnyCheckedApplicableCategory(enablement));
    memcpy(s_categoryChecked, checked, sizeof(checked));

    layers[0].state = 2;
    layers[0].tile = 3; // Different terrain at the same x/y on each layer.
    result = scan();
    CHECK_SELECTION(result.paintedCells == 2 && result.terrainMask == ((1u << 3) | (1u << 6)));

    layers[1].state = 0;
    result = scan();
    CHECK_SELECTION(result.paintedCells == 1 && result.terrainMask == (1u << 3));
    layers[0].state = 1; // Non-obstacle selection must not enable placement.
    result = scan();
    CHECK_SELECTION(result.paintedCells == 0 && result.terrainMask == 0);

    // Single-layer maps must never query underground; only the low byte is native.
    mapData[4] = 0xAABBCC00;
    layers[0].state = 2;
    layers[1].state = 2;
    collectionCalls = stateCalls[0] = stateCalls[1] = 0;
    result = scan();
    CHECK_SELECTION(result.paintedCells == 1 && result.terrainMask == (1u << 3));
    CHECK_SELECTION(collectionCalls == 1 && stateCalls[0] == 36 * 36 && stateCalls[1] == 0);

    context[0x98 / 4] = 0;
    CHECK_SELECTION(scan().paintedCells == 0);
    context[0x98 / 4] = 6;
    context[0x64 / 4] = 0;
    CHECK_SELECTION(scan().paintedCells == 0);
    CHECK_SELECTION(ComputePaintedTerrainScan(0, access).paintedCells == 0);
    enablement = {};
    CHECK_SELECTION(!AnyCheckedApplicableCategory(enablement));
}
