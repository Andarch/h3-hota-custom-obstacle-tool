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
    // Every UI selection must pack independently, including every DWORD boundary.
    for (int selected = 0; selected < kObstacleButtonCount; ++selected)
    {
        ZeroMemory(s_categoryChecked, sizeof(s_categoryChecked));
        s_categoryChecked[selected] = true;
        DWORD low, high, extended, upper;
        GetSelectedButtonMasks(low, high, extended, upper);
        DWORD masks[] = { low, high, extended, upper };
        for (int word = 0; word < 4; ++word)
            CHECK_SELECTION(masks[word] == (word == selected / 32 ? 1u << (selected & 31) : 0));
    }
    memcpy(s_categoryChecked, savedChecked, sizeof(savedChecked));
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
                WS_CHILD | WS_VISIBLE, 0, 0, 18, 18, action >= kGlobalSelectionAction ? state->globalHost : host,
                reinterpret_cast<HMENU>(kSelectionCommandBase + action), s_pluginModule, nullptr);
            CHECK_SELECTION(state->selectionButtons[action]);
            CHECK_SELECTION(SetWindowSubclass(state->selectionButtons[action], SelectionButtonSubclass, 1, 0));
        }
        state->selectionTooltip = CreateWindowExW(0, TOOLTIPS_CLASSW, nullptr,
            WS_POPUP | TTS_ALWAYSTIP, 0, 0, 0, 0, host, nullptr, s_pluginModule, nullptr);
        CHECK_SELECTION(state->selectionTooltip);
        TOOLINFOW tool = {};
        tool.cbSize = TTTOOLINFOW_V2_SIZE;
        tool.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
        tool.hwnd = state->globalHost;
        tool.uId = reinterpret_cast<UINT_PTR>(state->selectionButtons[kExpansionAction]);
        tool.lpszText = const_cast<wchar_t*>(UiText(ExpansionActionText()));
        CHECK_SELECTION(SendMessageW(state->selectionTooltip, TTM_ADDTOOLW, 0,
            reinterpret_cast<LPARAM>(&tool)));
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
    DWORD lowBefore, highBefore, extendedBefore, upperBefore, lowAfter, highAfter, extendedAfter, upperAfter;
    GetSelectedButtonMasks(lowBefore, highBefore, extendedBefore, upperBefore);
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
    CHECK_SELECTION(state->headerY[1] == kCategoryHeaderHeight &&
        firstHeight > kCategoryHeaderHeight);
    GetSelectedButtonMasks(lowAfter, highAfter, extendedAfter, upperAfter);
    CHECK_SELECTION(lowBefore == lowAfter && highBefore == highAfter && extendedBefore == extendedAfter && upperBefore == upperAfter);
    // Hidden, terrain-disabled selections still count; select-none updates only its section.
    SetCategoryButtonEnabled(state->buttons[0], false);
    CHECK_SELECTION(SelectedCountForCategory(0) > 0);
    HWND none = state->selectionButtons[1];
    SendMessageA(host, WM_COMMAND, MAKEWPARAM(kSelectionCommandBase + 1, BN_CLICKED), reinterpret_cast<LPARAM>(none));
    CHECK_SELECTION(SelectedCountForCategory(0) == 0 && SelectedCountForCategory(1) > 0);
    char caption[128] = {};
    GetWindowTextA(state->headers[0], caption, sizeof(caption));
    CHECK_SELECTION(strcmp(caption, "Mountains (0)") == 0);
    LOGFONTA font = {};
    GetObjectA(state->headerFont, sizeof(font), &font);
    CHECK_SELECTION(font.lfWeight == FW_NORMAL);
    GetObjectA(state->headerBoldFont, sizeof(font), &font);
    CHECK_SELECTION(font.lfWeight == FW_BOLD);
    // Recreate the disposable panel as the editor does when switching tools.
    HWND oldGlobalHost = state->globalHost;
    DestroyWindow(host);
    CHECK_SELECTION(!IsWindow(oldGlobalHost));
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
    CHECK_SELECTION(state->contentHeight == kObstacleCategoryCount * kCategoryHeaderHeight);
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
    // Expansion controls preserve every selection and focus, including mixed states.
    bool checkedBeforeToggle[kObstacleButtonCount];
    memcpy(checkedBeforeToggle, s_categoryChecked, sizeof(checkedBeforeToggle));
    HWND toggle = state->selectionButtons[kExpansionAction];
    CHECK_SELECTION(visibleStyle(toggle));
    CHECK_SELECTION(GetParent(toggle) == state->globalHost);
    CHECK_SELECTION(GetParent(state->globalHost) == parent);
    CHECK_SELECTION(GetParent(host) == parent);
    RECT fixedRow = {}, viewport = {};
    GetWindowRect(state->globalHost, &fixedRow);
    GetWindowRect(host, &viewport);
    CHECK_SELECTION(fixedRow.bottom == viewport.top);
    RECT toggleRect = {}, hostRect = {};
    GetWindowRect(toggle, &toggleRect);
    GetClientRect(host, &hostRect);
    MapWindowPoints(nullptr, host, reinterpret_cast<POINT*>(&toggleRect), 2);
    CHECK_SELECTION(!(GetWindowLongA(host, GWL_STYLE) & WS_VSCROLL));
    CHECK_SELECTION(toggleRect.right == hostRect.right - GetSystemMetrics(SM_CXVSCROLL));
    CHECK_SELECTION(toggleRect.right - toggleRect.left == kGlobalSelectionButtonExtent);
    const int toggleLeft = toggleRect.left;
    const int toggleTop = toggleRect.top;
    auto checkTogglePosition = [&]() {
        RECT rect = {};
        GetWindowRect(toggle, &rect);
        MapWindowPoints(nullptr, host, reinterpret_cast<POINT*>(&rect), 2);
        CHECK_SELECTION(rect.left == toggleLeft);
        CHECK_SELECTION(rect.top == toggleTop);
    };
    auto clickToggle = [&]() {
        SendMessageA(toggle, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(5, 5));
        SendMessageA(toggle, WM_LBUTTONUP, 0, MAKELPARAM(5, 5));
        CHECK_SELECTION(GetFocus() == focusBefore);
        CHECK_SELECTION(memcmp(checkedBeforeToggle, s_categoryChecked, sizeof(checkedBeforeToggle)) == 0);
    };
    auto checkTooltip = [&]() {
        wchar_t text[80] = {};
        TOOLINFOW tool = {};
        tool.cbSize = TTTOOLINFOW_V2_SIZE;
        tool.hwnd = state->globalHost;
        tool.uId = reinterpret_cast<UINT_PTR>(toggle);
        tool.lpszText = text;
        SendMessageW(state->selectionTooltip, TTM_GETTEXTW, _countof(text), reinterpret_cast<LPARAM>(&tool));
        CHECK_SELECTION(wcscmp(text, UiText(ExpansionActionText())) == 0);
    };
    CHECK_SELECTION(strcmp(ExpansionActionText(), "Expand All") == 0);
    checkTooltip();
    clickToggle();
    CHECK_SELECTION(AllCategoriesExpanded());
    CHECK_SELECTION(GetWindowLongA(host, GWL_STYLE) & WS_VSCROLL);
    checkTogglePosition();
    CHECK_SELECTION(strcmp(ExpansionActionText(), "Collapse All") == 0);
    checkTooltip();
    for (HWND obstacle : state->buttons) CHECK_SELECTION(visibleStyle(obstacle));
    clickHeader(1);
    CHECK_SELECTION(!AllCategoriesExpanded());
    checkTogglePosition();
    CHECK_SELECTION(strcmp(ExpansionActionText(), "Expand All") == 0);
    checkTooltip();
    clickToggle();
    CHECK_SELECTION(AllCategoriesExpanded());
    ScrollHostTo(host, state->contentHeight);
    CHECK_SELECTION(state->scrollPosition > 0);
    checkTogglePosition();
    const int bottomPosition = state->scrollPosition;
    SendMessageA(toggle, WM_MOUSEWHEEL, MAKEWPARAM(0, WHEEL_DELTA), 0);
    CHECK_SELECTION(state->scrollPosition < bottomPosition);
    checkTogglePosition();
    ScrollHostTo(host, state->contentHeight);
    RECT fixedBefore[3] = {};
    for (int action = kGlobalSelectionAction; action < kSelectionActionCount; ++action)
    {
        HWND control = state->selectionButtons[action];
        GetWindowRect(control, &fixedBefore[action - kGlobalSelectionAction]);
        CHECK_SELECTION(GetParent(control) == state->globalHost);
        CHECK_SELECTION(visibleStyle(control));
    }
    // The fixed row's global actions remain usable while scrolled to the bottom.
    for (int action : { kGlobalSelectionAction + 1, kGlobalSelectionAction })
    {
        HWND control = state->selectionButtons[action];
        SendMessageA(control, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(5, 5));
        SendMessageA(control, WM_LBUTTONUP, 0, MAKELPARAM(5, 5));
        CHECK_SELECTION(GetFocus() == focusBefore);
        CHECK_SELECTION(state->scrollPosition > 0);
        CHECK_SELECTION(AllCategoriesExpanded());
        for (bool checked : s_categoryChecked) CHECK_SELECTION(checked == (action % 2 == 0));
        RECT actual = {};
        GetWindowRect(control, &actual);
        CHECK_SELECTION(EqualRect(&actual, &fixedBefore[action - kGlobalSelectionAction]));
    }
    clickToggle();
    CHECK_SELECTION(state->scrollPosition == 0);
    CHECK_SELECTION(!(GetWindowLongA(host, GWL_STYLE) & WS_VSCROLL));
    checkTogglePosition();
    for (bool expanded : s_categoryExpanded) CHECK_SELECTION(!expanded);
    for (HWND obstacle : state->buttons) CHECK_SELECTION(!visibleStyle(obstacle));
    CHECK_SELECTION(strcmp(ExpansionActionText(), "Expand All") == 0);
    DestroyWindow(parent);
    memcpy(s_categoryChecked, savedChecked, sizeof(savedChecked));
    memcpy(s_categoryExpanded, savedExpanded, sizeof(savedExpanded));
}

void TestObstacleModeControls()
{
    HWND nativeLabel = CreateWindowExA(0, "STATIC", "Brush", WS_POPUP,
        0, 0, 100, 20, nullptr, nullptr, GetModuleHandleA(nullptr), nullptr);
    CHECK_SELECTION(nativeLabel && IsNativeBrushLabel(nativeLabel));
    SetWindowTextA(nativeLabel, "\xCA\xE8\xF1\xF2\xFC");
    CHECK_SELECTION(IsNativeBrushLabel(nativeLabel));
    SetWindowTextA(nativeLabel, "Terrain Type");
    CHECK_SELECTION(!IsNativeBrushLabel(nativeLabel));
    DestroyWindow(nativeLabel);

    CHECK_SELECTION(!CustomObstacleModeEnabled());
    CHECK_SELECTION(EnsurePlaceOverlayClass());
    HWND parent = CreateWindowExA(0, "STATIC", "", WS_POPUP, 0, 0, 300, 600,
        nullptr, nullptr, s_pluginModule, nullptr);
    CHECK_SELECTION(parent);
    VariantPanel panel;
    panel.parent = parent;
    panel.sourceToolbar = CreateWindowExA(0, "STATIC", "", WS_CHILD | WS_VISIBLE,
        0, 0, 100, 20, parent, nullptr, s_pluginModule, nullptr);
    RECT placeRect = {0, 0, 100, 20};
    panel.placeOverlay = CreatePlaceOverlay(parent, panel.sourceToolbar, placeRect);
    CHECK_SELECTION(panel.placeOverlay && !(GetWindowLongA(panel.placeOverlay, GWL_STYLE) & WS_VISIBLE));
    RECT rect = {0, 100, 280, 500};
    panel.scrollHost = CreateScrollHost(parent, rect);
    panel.modeHost = CreateModeHost(parent, 0, 70, 280, nullptr);
    panel.layoutApplied = true;
    CHECK_SELECTION(panel.modeHost && panel.scrollHost);
    auto visibleStyle = [](HWND window) { return (GetWindowLongA(window, GWL_STYLE) & WS_VISIBLE) != 0; };
    auto* panelScrollState = ScrollHostStateFromWindow(panel.scrollHost);
    CHECK_SELECTION(!visibleStyle(panelScrollState->globalHost));
    RefreshCategoryButtons(panel);
    CHECK_SELECTION(!visibleStyle(panelScrollState->globalHost));
    CHECK_SELECTION(!visibleStyle(panel.scrollHost));
    CHECK_SELECTION(!visibleStyle(panel.placeOverlay) && visibleStyle(panel.sourceToolbar));
    CHECK_SELECTION(!ShouldBlockNativePlaceObstaclesCommand());
    auto* mode = reinterpret_cast<ModeHostState*>(GetWindowLongPtrA(panel.modeHost, GWLP_USERDATA));
    auto checkModeTooltips = [&]() {
        CHECK_SELECTION(IsWindow(mode->tooltip));
        CHECK_SELECTION(SendMessageA(mode->tooltip, TTM_GETTOOLCOUNT, 0, 0) == 2);
        for (int index = 0; index < 2; ++index)
        {
            char caption[32] = {};
            wchar_t tip[128] = {};
            GetWindowTextA(mode->radios[index], caption, sizeof(caption));
            CHECK_SELECTION(strcmp(caption, index ? "Custom" : "Standard") == 0);
            TOOLINFOW tool = {};
            tool.cbSize = TTTOOLINFOW_V2_SIZE;
            tool.hwnd = panel.modeHost;
            tool.uId = reinterpret_cast<UINT_PTR>(mode->radios[index]);
            tool.lpszText = tip;
            SendMessageW(mode->tooltip, TTM_GETTEXTW, _countof(tip), reinterpret_cast<LPARAM>(&tool));
            CHECK_SELECTION(wcscmp(tip, UiText(index ? "Customize which obstacle types can be generated." :
                "Use the editor's native obstacle placement.")) == 0);
        }
    };
    checkModeTooltips();
    CHECK_SELECTION(SendMessageA(mode->radios[0], BM_GETCHECK, 0, 0) == BST_CHECKED);
    bool checked[kObstacleButtonCount], expanded[kObstacleCategoryCount];
    memcpy(checked, s_categoryChecked, sizeof(checked));
    memcpy(expanded, s_categoryExpanded, sizeof(expanded));
    HWND focus = GetFocus();
    bool optionSet = false;
    for (int index : {1, 0, 1})
    {
        SendMessageA(mode->radios[index], WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(5, 5));
        SendMessageA(mode->radios[index], WM_LBUTTONUP, 0, MAKELPARAM(5, 5));
        RefreshCategoryButtons(panel);
        CHECK_SELECTION(CustomObstacleModeEnabled() == (index == 1));
        HWND checkbox = panelScrollState->allTerrainsCheckbox;
        CHECK_SELECTION(IsWindowEnabled(checkbox));
        if (index == 1 && !optionSet)
        {
            CHECK_SELECTION(SendMessageA(checkbox, BM_GETCHECK, 0, 0) == BST_UNCHECKED);
            SendMessageA(checkbox, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(5, 5));
            SendMessageA(checkbox, WM_LBUTTONUP, 0, MAKELPARAM(5, 5));
            CHECK_SELECTION(AllObstacleTerrainsEnabled());
            CategoryEnablement water = { true, 1u << 8 }, rock = { true, 1u << 9 }, empty = {};
            for (int button = 0; button < kObstacleButtonCount; ++button)
            {
                CHECK_SELECTION(CategoryAppliesToPaintedTerrain(button, water));
                CHECK_SELECTION(!CategoryAppliesToPaintedTerrain(button, rock));
                CHECK_SELECTION(CategoryAppliesToPaintedTerrain(button, empty));
            }
            // The same checkbox remains interactive after painting.
            SendMessageA(checkbox, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(5, 5));
            SendMessageA(checkbox, WM_LBUTTONUP, 0, MAKELPARAM(5, 5));
            CHECK_SELECTION(!AllObstacleTerrainsEnabled());
            CHECK_SELECTION(!CategoryAppliesToPaintedTerrain(0, water));
            SendMessageA(checkbox, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(5, 5));
            SendMessageA(checkbox, WM_LBUTTONUP, 0, MAKELPARAM(5, 5));
            optionSet = true;
        }
        CHECK_SELECTION(AllObstacleTerrainsEnabled() == (index == 1));
        CHECK_SELECTION(SendMessageA(checkbox, BM_GETCHECK, 0, 0) == (index == 1 ? BST_CHECKED : BST_UNCHECKED));
        CHECK_SELECTION(GetFocus() == focus);
        CHECK_SELECTION(visibleStyle(panel.scrollHost) == (index == 1));
        CHECK_SELECTION(visibleStyle(panelScrollState->globalHost) == (index == 1));
        CHECK_SELECTION(visibleStyle(panel.placeOverlay) == (index == 1));
        CHECK_SELECTION(SendMessageA(mode->radios[index], BM_GETCHECK, 0, 0) == BST_CHECKED);
        CHECK_SELECTION(SendMessageA(mode->radios[1-index], BM_GETCHECK, 0, 0) == BST_UNCHECKED);
        CHECK_SELECTION(memcmp(checked, s_categoryChecked, sizeof(checked)) == 0);
        CHECK_SELECTION(memcmp(expanded, s_categoryExpanded, sizeof(expanded)) == 0);
    }
    HWND oldModeTooltip = mode->tooltip;
    DestroyWindow(panel.modeHost);
    CHECK_SELECTION(!IsWindow(oldModeTooltip));
    panel.modeHost = CreateModeHost(parent, 0, 70, 280, nullptr);
    mode = reinterpret_cast<ModeHostState*>(GetWindowLongPtrA(panel.modeHost, GWLP_USERDATA));
    checkModeTooltips();
    CHECK_SELECTION(CustomObstacleModeEnabled());
    RECT recreatedRect = {0, 100, 280, 500};
    HWND recreated = CreateScrollHost(parent, recreatedRect);
    CHECK_SELECTION(recreated);
    CHECK_SELECTION(SendMessageA(ScrollHostStateFromWindow(recreated)->allTerrainsCheckbox,
        BM_GETCHECK, 0, 0) == BST_CHECKED);
    DestroyWindow(recreated);
    CHECK_SELECTION(SendMessageA(mode->radios[1], BM_GETCHECK, 0, 0) == BST_CHECKED);
    SetCustomObstacleMode(false);
    SetAllObstacleTerrains(false);
    // Default Shift+O commands are left intact by the queued-message hook.
    MSG command = {}; command.hwnd = parent; command.message = WM_COMMAND;
    command.wParam = kVanillaPlaceObstaclesCommand;
    VariantGetMessageHook(0, PM_REMOVE, reinterpret_cast<LPARAM>(&command));
    CHECK_SELECTION(command.message == WM_COMMAND && command.wParam == kVanillaPlaceObstaclesCommand);
    DestroyWindow(parent);
}

void TestRussianObstacleControls()
{
    SetUiLanguage(UiLanguage::Russian);
    HWND owner = CreateWindowExW(0, L"STATIC", L"Russian controls", WS_OVERLAPPEDWINDOW,
        50, 50, 350, 650, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    CHECK_SELECTION(owner);
    HWND modeHost = CreateModeHost(owner, 10, 10, 200, nullptr);
    CHECK_SELECTION(modeHost);
    auto* mode = reinterpret_cast<ModeHostState*>(GetWindowLongPtrA(modeHost, GWLP_USERDATA));
    for (int i = 0; i < 2; ++i)
    {
        wchar_t caption[128]{}, tip[256]{};
        GetWindowTextW(mode->radios[i], caption, 128);
        CHECK_SELECTION(wcscmp(caption, UiText(i ? "Custom" : "Standard")) == 0);
        TOOLINFOW info{}; info.cbSize = TTTOOLINFOW_V2_SIZE; info.hwnd = modeHost;
        info.uId = reinterpret_cast<UINT_PTR>(mode->radios[i]); info.lpszText = tip;
        SendMessageW(mode->tooltip, TTM_GETTEXTW, 256, reinterpret_cast<LPARAM>(&info));
        CHECK_SELECTION(wcscmp(tip, UiText(i ? "Customize which obstacle types can be generated." :
            "Use the editor's native obstacle placement.")) == 0);
        HDC dc = GetDC(mode->radios[i]);
        auto font = SelectObject(dc, GetStockObject(DEFAULT_GUI_FONT));
        SIZE text{}; GetTextExtentPoint32W(dc, caption, static_cast<int>(wcslen(caption)), &text);
        RECT bounds{}; GetClientRect(mode->radios[i], &bounds);
        CHECK_SELECTION(text.cx + 18 <= bounds.right);
        SelectObject(dc, font); ReleaseDC(mode->radios[i], dc);
    }
    RECT viewport{ 10, 50, 210, 600 };
    HWND host = CreateScrollHost(owner, viewport);
    CHECK_SELECTION(host && CreateCategoryHeaders(host, nullptr));
    auto* state = ScrollHostStateFromWindow(host);
    wchar_t caption[160]{};
    GetWindowTextW(state->allTerrainsCheckbox, caption, 160);
    CHECK_SELECTION(wcscmp(caption, UiText("Allow all terrains")) == 0);
    GetWindowTextW(state->headers[0], caption, 160);
    CHECK_SELECTION(wcsstr(caption, UiText("Mountains")) == caption);
    const char* original = ObstacleButtonName(0);
    CHECK_SELECTION(strcmp(original, "Dirt Mountains") == 0);
    DestroyWindow(owner);
    SetUiLanguage(UiLanguage::English);
}
