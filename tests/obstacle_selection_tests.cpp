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

void TestObstacleCategorySections()
{
    bool savedChecked[kObstacleButtonCount];
    bool savedExpanded[kObstacleCategoryCount];
    EnsureCategoryCheckedInitialized();
    memcpy(savedChecked, s_categoryChecked, sizeof(savedChecked));
    memcpy(savedExpanded, s_categoryExpanded, sizeof(savedExpanded));
    s_pluginModule = GetModuleHandleA(nullptr);
    CHECK_SELECTION(EnsureScrollHostClass() && EnsureCategoryButtonClass());
    HWND parent = CreateWindowExA(0, "STATIC", "", WS_POPUP, 0, 0, 300, 600,
        nullptr, nullptr, s_pluginModule, nullptr);
    CHECK_SELECTION(parent);
    auto createHost = [&]() {
        RECT rect = {0, 0, 280, 500};
        HWND host = CreateScrollHost(parent, rect);
        CHECK_SELECTION(!(GetWindowLongA(host, GWL_STYLE) & WS_VISIBLE));
        CHECK_SELECTION(host && CreateCategoryHeaders(host, nullptr));
        auto* state = ScrollHostStateFromWindow(host);
        for (int index = 0; index < kObstacleButtonCount; ++index)
        {
            state->buttons[index] = CreateCategoryButton(host, nullptr, index, 0, 0);
            CHECK_SELECTION(state->buttons[index]);
        }
        for (int action = 0; action < kSelectionActionCount; ++action)
        {
            state->selectionButtons[action] = CreateWindowExA(0, "BUTTON", "",
                WS_CHILD | WS_VISIBLE, 0, 0, 18, 18, host,
                reinterpret_cast<HMENU>(kSelectionCommandBase + action), s_pluginModule, nullptr);
            CHECK_SELECTION(state->selectionButtons[action]);
            CHECK_SELECTION(SetWindowSubclass(state->selectionButtons[action], SelectionButtonSubclass, 1, 0));
        }
        LayoutScrollHost(host, *state);
        return host;
    };
    auto visibleStyle = [](HWND window) { return (GetWindowLongA(window, GWL_STYLE) & WS_VISIBLE) != 0; };
    HWND host = createHost();
    auto* state = ScrollHostStateFromWindow(host);
    auto clickHeader = [&](int category) {
        HWND header = state->headers[category];
        SendMessageA(header, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(5, 5));
        SendMessageA(header, WM_LBUTTONUP, 0, MAKELPARAM(5, 5));
    };
    for (bool expanded : s_categoryExpanded) CHECK_SELECTION(!expanded);
    CHECK_SELECTION(!visibleStyle(state->buttons[0]));
    clickHeader(0);
    CHECK_SELECTION(s_categoryExpanded[0] && !s_categoryExpanded[1]);
    CHECK_SELECTION(visibleStyle(state->buttons[0]));
    const int firstHeight = state->headerY[1];
    DWORD lowBefore, highBefore, lowAfter, highAfter;
    GetSelectedButtonMasks(lowBefore, highBefore);
    HWND focusBefore = GetFocus();
    clickHeader(1);
    CHECK_SELECTION(s_categoryExpanded[0] && s_categoryExpanded[1]);
    CHECK_SELECTION(GetFocus() == focusBefore);
    int secondButton = -1;
    for (int index = 0; index < kObstacleButtonCount; ++index)
        if (ObstacleButtonDefinitionAt(index)->categoryIndex == 1) { secondButton = index; break; }
    CHECK_SELECTION(secondButton >= 0 && visibleStyle(state->buttons[secondButton]));
    CHECK_SELECTION(visibleStyle(state->selectionButtons[2]));
    auto* button = CategoryButtonStateFromWindow(state->buttons[0]);
    button->mouseInside = true;
    button->hot = true;
    clickHeader(0);
    CHECK_SELECTION(!s_categoryExpanded[0] && s_categoryExpanded[1]);
    CHECK_SELECTION(!visibleStyle(state->buttons[0]) && !visibleStyle(state->selectionButtons[0]));
    CHECK_SELECTION(!button->mouseInside && !button->hot);
    CHECK_SELECTION(state->headerY[1] == kGlobalSelectionRowHeight + kCategoryHeaderHeight &&
        firstHeight > kGlobalSelectionRowHeight + kCategoryHeaderHeight);
    GetSelectedButtonMasks(lowAfter, highAfter);
    CHECK_SELECTION(lowBefore == lowAfter && highBefore == highAfter);
    // Hidden, terrain-disabled selections still count; select-none updates only its section.
    SetCategoryButtonEnabled(state->buttons[0], false);
    CHECK_SELECTION(SelectedCountForCategory(0) > 0);
    HWND none = state->selectionButtons[1];
    SendMessageA(host, WM_COMMAND, MAKEWPARAM(kSelectionCommandBase + 1, BN_CLICKED), reinterpret_cast<LPARAM>(none));
    CHECK_SELECTION(SelectedCountForCategory(0) == 0 && SelectedCountForCategory(1) > 0);
    char caption[128] = {};
    GetWindowTextA(state->headers[0], caption, sizeof(caption));
    CHECK_SELECTION(strcmp(caption, "Raised Ground (0)") == 0);
    LOGFONTA font = {};
    GetObjectA(state->headerFont, sizeof(font), &font);
    CHECK_SELECTION(font.lfWeight == FW_NORMAL);
    GetObjectA(state->headerBoldFont, sizeof(font), &font);
    CHECK_SELECTION(font.lfWeight == FW_BOLD);
    // Recreate the disposable panel as the editor does when switching tools.
    DestroyWindow(host);
    host = createHost();
    state = ScrollHostStateFromWindow(host);
    CHECK_SELECTION(!s_categoryExpanded[0] && s_categoryExpanded[1]);
    CHECK_SELECTION(!visibleStyle(state->buttons[0]) && visibleStyle(state->buttons[secondButton]));
    CHECK_SELECTION(state->selectedCounts[0] == 0);
    for (bool& expanded : s_categoryExpanded) expanded = true;
    LayoutScrollHost(host, *state);
    ScrollHostTo(host, state->contentHeight);
    CHECK_SELECTION(state->scrollPosition > 0);
    for (bool& expanded : s_categoryExpanded) expanded = false;
    LayoutScrollHost(host, *state);
    CHECK_SELECTION(state->scrollPosition == 0);
    CHECK_SELECTION(state->contentHeight == kGlobalSelectionRowHeight + kObstacleCategoryCount * kCategoryHeaderHeight);
    // Global controls include collapsed and terrain-disabled selections without
    // expanding sections or taking focus from the editor.
    CHECK_SELECTION(visibleStyle(state->selectionButtons[kGlobalSelectionAction]));
    SetCategoryButtonEnabled(state->buttons[0], false);
    RECT globalRect = {};
    GetWindowRect(state->selectionButtons[kGlobalSelectionAction], &globalRect);
    CHECK_SELECTION(globalRect.right - globalRect.left == kGlobalSelectionButtonExtent);
    for (int action = kGlobalSelectionAction + 1; action >= kGlobalSelectionAction; --action)
    {
        HWND global = state->selectionButtons[action];
        SendMessageA(global, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(5, 5));
        SendMessageA(global, WM_LBUTTONUP, 0, MAKELPARAM(5, 5));
        CHECK_SELECTION(GetFocus() == focusBefore);
        for (int index = 0; index < kObstacleButtonCount; ++index)
        {
            const bool expected = action % 2 == 0;
            CHECK_SELECTION(s_categoryChecked[index] == expected);
            CHECK_SELECTION(CategoryButtonStateFromWindow(state->buttons[index])->checked == expected);
        }
        for (int category = 0; category < kObstacleCategoryCount; ++category)
        {
            CHECK_SELECTION(!s_categoryExpanded[category]);
            CHECK_SELECTION(state->selectedCounts[category] == SelectedCountForCategory(category));
        }
    }
    DestroyWindow(parent);
    memcpy(s_categoryChecked, savedChecked, sizeof(savedChecked));
    memcpy(s_categoryExpanded, savedExpanded, sizeof(savedExpanded));
}

void TestObstacleModeControls()
{
    CHECK_SELECTION(!CustomObstacleModeEnabled());
    CHECK_SELECTION(EnsurePlaceOverlayClass());
    HWND parent = CreateWindowExA(0, "STATIC", "", WS_POPUP, 0, 0, 300, 600,
        nullptr, nullptr, s_pluginModule, nullptr);
    CHECK_SELECTION(parent);
    VariantPanel panel;
    panel.parent = parent;
    panel.sourceToolbar = CreateWindowExA(0, "STATIC", "", WS_CHILD | WS_VISIBLE,
        0, 0, 100, 20, parent, nullptr, s_pluginModule, nullptr);
    panel.categoryLabel = CreateWindowExA(0, "STATIC", "Obstacle Types", WS_CHILD | WS_VISIBLE,
        0, 50, 200, 20, parent, nullptr, s_pluginModule, nullptr);
    RECT placeRect = {0, 0, 100, 20};
    panel.placeOverlay = CreatePlaceOverlay(parent, panel.sourceToolbar, placeRect);
    CHECK_SELECTION(panel.placeOverlay && !(GetWindowLongA(panel.placeOverlay, GWL_STYLE) & WS_VISIBLE));
    RECT rect = {0, 100, 280, 500};
    panel.scrollHost = CreateScrollHost(parent, rect);
    panel.modeHost = CreateModeHost(parent, 0, 70, 280, nullptr);
    panel.layoutApplied = true;
    CHECK_SELECTION(panel.modeHost && panel.scrollHost);
    auto visibleStyle = [](HWND window) { return (GetWindowLongA(window, GWL_STYLE) & WS_VISIBLE) != 0; };
    RefreshCategoryButtons(panel);
    CHECK_SELECTION(!visibleStyle(panel.categoryLabel) && !visibleStyle(panel.scrollHost));
    CHECK_SELECTION(!visibleStyle(panel.placeOverlay) && visibleStyle(panel.sourceToolbar));
    CHECK_SELECTION(!ShouldBlockNativePlaceObstaclesCommand());
    auto* mode = reinterpret_cast<ModeHostState*>(GetWindowLongPtrA(panel.modeHost, GWLP_USERDATA));
    CHECK_SELECTION(SendMessageA(mode->radios[0], BM_GETCHECK, 0, 0) == BST_CHECKED);
    bool checked[kObstacleButtonCount], expanded[kObstacleCategoryCount];
    memcpy(checked, s_categoryChecked, sizeof(checked));
    memcpy(expanded, s_categoryExpanded, sizeof(expanded));
    HWND focus = GetFocus();
    for (int index : {1, 0, 1})
    {
        SendMessageA(mode->radios[index], WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(5, 5));
        SendMessageA(mode->radios[index], WM_LBUTTONUP, 0, MAKELPARAM(5, 5));
        RefreshCategoryButtons(panel);
        CHECK_SELECTION(CustomObstacleModeEnabled() == (index == 1));
        CHECK_SELECTION(GetFocus() == focus);
        CHECK_SELECTION(visibleStyle(panel.categoryLabel) == (index == 1));
        CHECK_SELECTION(visibleStyle(panel.scrollHost) == (index == 1));
        CHECK_SELECTION(visibleStyle(panel.placeOverlay) == (index == 1));
        CHECK_SELECTION(SendMessageA(mode->radios[index], BM_GETCHECK, 0, 0) == BST_CHECKED);
        CHECK_SELECTION(SendMessageA(mode->radios[1-index], BM_GETCHECK, 0, 0) == BST_UNCHECKED);
        CHECK_SELECTION(memcmp(checked, s_categoryChecked, sizeof(checked)) == 0);
        CHECK_SELECTION(memcmp(expanded, s_categoryExpanded, sizeof(expanded)) == 0);
    }
    DestroyWindow(panel.modeHost);
    panel.modeHost = CreateModeHost(parent, 0, 70, 280, nullptr);
    mode = reinterpret_cast<ModeHostState*>(GetWindowLongPtrA(panel.modeHost, GWLP_USERDATA));
    CHECK_SELECTION(CustomObstacleModeEnabled());
    CHECK_SELECTION(SendMessageA(mode->radios[1], BM_GETCHECK, 0, 0) == BST_CHECKED);
    SetCustomObstacleMode(false);
    // Default Shift+O commands are left intact by the queued-message hook.
    MSG command = {}; command.hwnd = parent; command.message = WM_COMMAND;
    command.wParam = kVanillaPlaceObstaclesCommand;
    VariantGetMessageHook(0, PM_REMOVE, reinterpret_cast<LPARAM>(&command));
    CHECK_SELECTION(command.message == WM_COMMAND && command.wParam == kVanillaPlaceObstaclesCommand);
    DestroyWindow(parent);
}
