#include "pch.h"
#include "obstacle_variants.h"

#include "logger.h"
#include "hook.h"
#include "obstacle_button_data.h"
#include "obstacle_classification.h"
#include "obstacle_def_images.h"
#include "obstacle_filter.h"
#include "obstacle_preview.h"
#include "obstacle_selection_geometry.h"

#include <commctrl.h>
#include <cstdio>
#include <cstring>
#include <new>

namespace
{
    constexpr char kVariantUiEnvVar[] = "HOTA_MAPED_PLUGIN_OBSTACLE_VARIANTS";
    constexpr UINT kVanillaPlaceObstaclesCommand = 32927;
    constexpr UINT kPlaceThickObstaclesCommand = 0x7001;
    constexpr UINT kPlaceThinObstaclesCommand = 0x7002;
    constexpr int kCategoryButtonExtent = 40;
    constexpr int kSelectionButtonExtent = 18;
    constexpr int kSelectionRowHeight = 22;
    constexpr int kSelectionCommandBase = 4100;
    constexpr int kGlobalSelectionAction = kObstacleCategoryCount * 2;
    constexpr int kExpansionAction = kGlobalSelectionAction + 2;
    constexpr int kSelectionActionCount = kExpansionAction + 1;
    constexpr int kGlobalSelectionButtonExtent = 28;
    constexpr int kGlobalSelectionRowHeight = 36;
    constexpr int kCategoryGlyphExtent = 24;
    constexpr int kCategoryRowGap = 4;
    constexpr int kCategoryHeaderHeight = 24;
    constexpr int kCategoryHeaderCommandBase = 4200;
    constexpr int kScrollWheelStep = kCategoryButtonExtent + kCategoryRowGap;
    constexpr int kMapLayerCount = 2;
    constexpr int kObstaclePaintCellState = 2;
    constexpr DWORD kPollIntervalMs = 50;
    constexpr DWORD kObstacleUpdateCaptureAddress = 0x00484C9A;
    constexpr BYTE kObstacleUpdateCaptureExpected[] = { 0x8B, 0x46, 0x64, 0x85, 0xC0 };
    constexpr DWORD kObstacleUpdateCaptureResume = 0x00484C9F;
    constexpr DWORD kCategoryRefreshTimerMs = 75;
    constexpr UINT_PTR kCategoryRefreshTimerId = 1;
    constexpr UINT_PTR kCategoryPreviewTimerId = 2;
    constexpr DWORD kCategoryPreviewDelayMs = 250;
    char kPlaceOverlayTooltip[] = "Place Obstacles [Shift+O]";

    struct VariantPanel
    {
        HWND parent = nullptr;
        HWND sourceToolbar = nullptr;
        HWND placeOverlay = nullptr;
        HWND categoryLabel = nullptr;
        HWND scrollHost = nullptr;
        HWND modeLabel = nullptr;
        HWND modeHost = nullptr;
        HWND categoryButtons[kObstacleButtonCount] = {};
        DWORD threadId = 0;
        bool layoutApplied = false;
    };

    struct CategoryButtonState
    {
        HWND sourceToolbar = nullptr;
        bool checked = true;
        bool enabled = true;
        bool hot = false;
        bool pressed = false;
        bool mouseInside = false;
        int index = 0;
    };

    struct PlaceOverlayState
    {
        HWND parent = nullptr;
        HWND sourceToolbar = nullptr;
        HWND tooltip = nullptr;
        bool enabled = false;
        bool hot = false;
        bool pressed = false;
    };

    struct ScrollHostState
    {
        HWND headers[kObstacleCategoryCount] = {};
        int headerY[kObstacleCategoryCount] = {};
        int selectedCounts[kObstacleCategoryCount] = {};
        HFONT headerFont = nullptr;
        HFONT headerBoldFont = nullptr;
        HWND selectionButtons[kSelectionActionCount] = {};
        HWND selectionTooltip = nullptr;
        HWND sourceToolbar = nullptr;
        int selectionY[kObstacleCategoryCount] = {};
        HWND buttons[kObstacleButtonCount] = {};
        int buttonX[kObstacleButtonCount] = {};
        int buttonY[kObstacleButtonCount] = {};
        int contentHeight = 0;
        int scrollPosition = 0;
    };

    struct ModeHostState
    {
        HWND radios[2] = {};
    };

    VariantPanel s_panels[8] = {};
    CRITICAL_SECTION s_panelLock = {};
    bool s_panelLockInitialized = false;
    HMODULE s_pluginModule = nullptr;
    HHOOK s_getMessageHook = nullptr;
    DWORD s_getMessageHookThreadId = 0;
    ATOM s_categoryButtonClass = 0;
    ATOM s_scrollHostClass = 0;
    ATOM s_placeOverlayClass = 0;
    BYTE s_updateCaptureOriginal[5] = {};
    volatile DWORD s_lastObstacleUiContext = 0;
    DWORD s_lastLoggedAppliedTerrainMask = 0xFFFFFFFF;
    DWORD s_lastLoggedAppliedNativeEnabled = 0xFFFFFFFF;
    DWORD s_lastLoggedMapWidth = 0;
    DWORD s_lastLoggedMapHeight = 0;
    char s_lastCategoryTraceSummary[4096] = {};
    bool s_categoryChecked[kObstacleButtonCount] = {};
    bool s_categoryCheckedInitialized = false;
    // Session state is independent of the editor's disposable tool windows.
    bool s_categoryExpanded[kObstacleCategoryCount] = {};

    using SelectCollectionFn = DWORD(__thiscall*)(void*, int);
    using TileAtFn = DWORD* (__thiscall*)(void*, int, int);
    using CellStateAtFn = int(__thiscall*)(void*, unsigned int, unsigned int, unsigned char);

    struct SelectionAccess
    {
        SelectCollectionFn selectCollection;
        TileAtFn tileAt;
        CellStateAtFn cellStateAt;
    };

    struct GridSelectionScanStats
    {
        DWORD mask = 0;
        DWORD paintedCells = 0;
        unsigned int minX = 0xFFFFFFFF;
        unsigned int minY = 0xFFFFFFFF;
        unsigned int maxX = 0;
        unsigned int maxY = 0;
        DWORD stateCounts[256] = {};
        DWORD stateMasks[256] = {};
        DWORD outOfRangeStateCount = 0;
    };

    struct PaintedTerrainScan
    {
        DWORD terrainMask = 0;
        DWORD paintedCells = 0;
    };

    SelectCollectionFn SelectCollection()
    {
        return reinterpret_cast<SelectCollectionFn>(0x00429EBD);
    }

    TileAtFn TileAt()
    {
        return reinterpret_cast<TileAtFn>(0x0042B23E);
    }

    CellStateAtFn CellStateAt()
    {
        return reinterpret_cast<CellStateAtFn>(0x0043C114);
    }

    bool CategoryTraceEnabled()
    {
        static int enabled = -1;
        if (enabled >= 0)
            return enabled != 0;

        char value[16] = {};
        const DWORD length = GetEnvironmentVariableA(
            "HOTA_MAPED_PLUGIN_CATEGORY_TRACE",
            value,
            sizeof(value));
        enabled =
            length > 0 &&
            length < sizeof(value) &&
            (_stricmp(value, "1") == 0 ||
                _stricmp(value, "true") == 0 ||
                _stricmp(value, "yes") == 0 ||
                _stricmp(value, "on") == 0);
        return enabled != 0;
    }

    UINT CategoryLayoutMessage()
    {
        static UINT message = RegisterWindowMessageA("HotaMapEdPlugin.InstallObstacleCategoryButtons");
        return message;
    }

    UINT CategoryRefreshMessage()
    {
        static UINT message = RegisterWindowMessageA("HotaMapEdPlugin.RefreshObstacleCategoryButtons");
        return message;
    }

    bool AddVariantToolbarButtons(VariantPanel& panel);
    CategoryButtonState* CategoryButtonStateFromWindow(HWND hwnd);
    PlaceOverlayState* PlaceOverlayStateFromWindow(HWND hwnd);
    void RefreshCategoryButtonsForToolbar(HWND toolbar);
    void RefreshAllCategoryButtons();
    void RefreshCategoryHeaders(HWND host);
    void InitializeScrollHostLayout(ScrollHostState& state);
    bool RectInParent(HWND parent, HWND child, RECT& rect);
    void RunObstacleVariantCommand(
        HWND parent,
        HWND sourceToolbar,
        ObstacleGenerationFilterMode mode);

    void EnsureCategoryCheckedInitialized()
    {
        if (s_categoryCheckedInitialized)
            return;
        for (int index = 0; index < kObstacleButtonCount; ++index)
        {
            const ObstacleButtonDefinition* definition =
                ObstacleButtonDefinitionAt(index);
            s_categoryChecked[index] = definition && definition->defaultSelected;
        }
        s_categoryCheckedInitialized = true;
    }

    void GetSelectedButtonMasks(DWORD& lowMask, DWORD& highMask, DWORD& extendedMask, DWORD& upperMask)
    {
        EnsureCategoryCheckedInitialized();
        lowMask = 0;
        highMask = 0;
        extendedMask = 0;
        upperMask = 0;
        for (int index = 0; index < kObstacleButtonCount; ++index)
        {
            if (!s_categoryChecked[index])
                continue;
            if (index < 32)
                lowMask |= 1u << index;
            else if (index < 64)
                highMask |= 1u << (index - 32);
            else if (index < 96)
                extendedMask |= 1u << (index - 64);
            else
                upperMask |= 1u << (index - 96);
        }
    }

    void __stdcall CaptureObstacleUiContext(DWORD context)
    {
        s_lastObstacleUiContext = context;
    }

    __declspec(naked) void ObstacleUpdateCaptureHook()
    {
        __asm
        {
            pushad
            push esi
            call CaptureObstacleUiContext
            popad
            mov eax, [esi+64h]
            test eax, eax
            jmp kObstacleUpdateCaptureResume
        }
    }

    bool SafeReadDword(DWORD address, DWORD& value)
    {
        __try
        {
            value = *reinterpret_cast<DWORD*>(address);
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            value = 0;
            return false;
        }
    }

    bool AddGridSelectionTerrainMask(
        void* selection,
        void* terrainCollection,
        int collectionIndex,
        unsigned int mapWidth,
        unsigned int mapHeight,
        GridSelectionScanStats& stats,
        const SelectionAccess& access)
    {
        bool found = false;

        __try
        {
            for (unsigned int y = 0; y < mapHeight; ++y)
            {
                for (unsigned int x = 0; x < mapWidth; ++x)
                {
                    const int cellState =
                        access.cellStateAt(selection, x, y, static_cast<unsigned char>(collectionIndex));
                    if (cellState == 0)
                        continue;

                    ++stats.paintedCells;
                    if (cellState >= 0 && cellState < 256)
                        ++stats.stateCounts[cellState];
                    else
                        ++stats.outOfRangeStateCount;

                    if (x < stats.minX)
                        stats.minX = x;
                    if (y < stats.minY)
                        stats.minY = y;
                    if (x > stats.maxX)
                        stats.maxX = x;
                    if (y > stats.maxY)
                        stats.maxY = y;

                    DWORD* tile = access.tileAt(terrainCollection, static_cast<int>(x), static_cast<int>(y));
                    if (tile)
                    {
                        const DWORD tileValue = *tile;
                        const DWORD terrainBit = 1u << (tileValue & 0x0F);
                        stats.mask |= terrainBit;
                        if (cellState >= 0 && cellState < 256)
                            stats.stateMasks[cellState] |= terrainBit;

                        found = true;
                    }

                }
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return false;
        }

        return found;
    }

    void AppendTraceText(char* buffer, size_t bufferSize, size_t& offset, const char* format, ...)
    {
        if (offset >= bufferSize)
            return;

        va_list args;
        va_start(args, format);
        const int written =
            _vsnprintf_s(buffer + offset, bufferSize - offset, _TRUNCATE, format, args);
        va_end(args);

        if (written < 0)
            offset = bufferSize;
        else
            offset += static_cast<size_t>(written);
    }

    void TraceGridSelectionStats(DWORD mask, const GridSelectionScanStats* stats)
    {
        if (!CategoryTraceEnabled())
            return;

        char summary[4096] = {};
        size_t offset = 0;
        AppendTraceText(summary, sizeof(summary), offset, "mask=0x%08lX", mask);

        DWORD totalCells = 0;
        for (int index = 0; index < kMapLayerCount; ++index)
            totalCells += stats[index].paintedCells;
        AppendTraceText(summary, sizeof(summary), offset, " totalCells=%lu", totalCells);

        for (int index = 0; index < kMapLayerCount; ++index)
        {
            const GridSelectionScanStats& stat = stats[index];
            if (stat.paintedCells == 0)
                continue;

            AppendTraceText(
                summary,
                sizeof(summary),
                offset,
                " c%d cells=%lu mask=0x%08lX bounds=(%u,%u)-(%u,%u) states=",
                index,
                stat.paintedCells,
                stat.mask,
                stat.minX,
                stat.minY,
                stat.maxX,
                stat.maxY);

            bool firstState = true;
            int loggedStates = 0;
            for (int stateValue = 0; stateValue < 256; ++stateValue)
            {
                if (stat.stateCounts[stateValue] == 0)
                    continue;

                AppendTraceText(
                    summary,
                    sizeof(summary),
                    offset,
                    "%s%d:%lu/0x%08lX",
                    firstState ? "" : ",",
                    stateValue,
                    stat.stateCounts[stateValue],
                    stat.stateMasks[stateValue]);
                firstState = false;
                ++loggedStates;
                if (loggedStates >= 12)
                    break;
            }

            if (stat.outOfRangeStateCount)
            {
                AppendTraceText(
                    summary,
                    sizeof(summary),
                    offset,
                    "%sother:%lu",
                    firstState ? "" : ",",
                    stat.outOfRangeStateCount);
            }
        }

        if (strcmp(summary, s_lastCategoryTraceSummary) == 0)
            return;

        strcpy_s(s_lastCategoryTraceSummary, summary);
        LogFormat("Obstacle variants category trace: %s", summary);
    }

    bool TryReadMapDimensions(DWORD terrainCollection, unsigned int& width, unsigned int& height)
    {
        width = 0;
        height = 0;

        DWORD terrainData = 0;
        DWORD terrainGrid = 0;
        DWORD chunkColumns = 0;
        DWORD chunksBegin = 0;
        DWORD chunksEnd = 0;
        // Follow the same grid as native TileAt (0x42B23E -> 0x42A0FF).
        if (!SafeReadDword(terrainCollection, terrainData) ||
            !terrainData ||
            !SafeReadDword(terrainData + 0x08, terrainGrid) ||
            !terrainGrid ||
            !SafeReadDword(terrainGrid + 0x04, chunkColumns) ||
            !SafeReadDword(terrainGrid + 0x0C, chunksBegin) ||
            !SafeReadDword(terrainGrid + 0x10, chunksEnd) ||
            !TryComputeTerrainMapDimensions(chunkColumns, chunksBegin, chunksEnd, width, height))
            return false;

        if (width != s_lastLoggedMapWidth || height != s_lastLoggedMapHeight)
        {
            s_lastLoggedMapWidth = width;
            s_lastLoggedMapHeight = height;
            LogFormat(
                "Obstacle variants category: map scan dimensions=%ux%u terrainGrid=%08lX.",
                width,
                height,
                terrainGrid);
        }
        return true;
    }

    PaintedTerrainScan ComputePaintedTerrainScan(DWORD context, const SelectionAccess& access)
    {
        PaintedTerrainScan result = {};
        if (!context)
            return result;

        DWORD mode = 0;
        DWORD selection = 0;
        DWORD mapContext = 0;
        DWORD selectionManager = 0;
        DWORD mapData = 0;
        DWORD undergroundFlags = 0;
        unsigned int mapWidth = 0;
        unsigned int mapHeight = 0;
        if (!SafeReadDword(context + 0x98, mode) ||
            mode != 6 ||
            !SafeReadDword(context + 0x64, selection) ||
            !selection ||
            !SafeReadDword(context + 0x3C, mapContext) ||
            !mapContext ||
            !SafeReadDword(mapContext + 0x54, selectionManager) ||
            !selectionManager ||
            !SafeReadDword(selectionManager, mapData) ||
            !mapData ||
            !SafeReadDword(mapData + 0x10, undergroundFlags))
            return result;

        // Match native command 0x484B07: generate each painted layer, with layer 1
        // present only when the map's underground byte (0x429E28) is nonzero.
        GridSelectionScanStats stats[kMapLayerCount] = {};
        const int layerCount = (undergroundFlags & 0xFF) != 0 ? 2 : 1;
        for (int index = 0; index < layerCount; ++index)
        {
            void* terrainCollection = reinterpret_cast<void*>(
                access.selectCollection(reinterpret_cast<void*>(selectionManager), index));
            if (!terrainCollection ||
                !TryReadMapDimensions(reinterpret_cast<DWORD>(terrainCollection), mapWidth, mapHeight))
                return PaintedTerrainScan{};

            AddGridSelectionTerrainMask(
                reinterpret_cast<void*>(selection),
                terrainCollection,
                index,
                mapWidth,
                mapHeight,
                stats[index],
                access);
            result.terrainMask |= stats[index].stateMasks[kObstaclePaintCellState];
            result.paintedCells += stats[index].stateCounts[kObstaclePaintCellState];
        }

        TraceGridSelectionStats(result.terrainMask, stats);
        return result;
    }

    PaintedTerrainScan ComputePaintedTerrainScan()
    {
        return ComputePaintedTerrainScan(s_lastObstacleUiContext,
            { SelectCollection(), TileAt(), CellStateAt() });
    }

    bool NativePlaceObstaclesEnabled(HWND toolbar)
    {
        if (!toolbar)
            return false;

        const LRESULT state = SendMessageA(toolbar, TB_GETSTATE, kVanillaPlaceObstaclesCommand, 0);
        if (state < 0)
            return false;

        return (state & TBSTATE_ENABLED) != 0;
    }

    void SetCategoryButtonEnabled(HWND hwnd, bool enabled)
    {
        CategoryButtonState* state = CategoryButtonStateFromWindow(hwnd);
        if (!state || state->enabled == enabled)
            return;

        state->enabled = enabled;
        state->hot = false;
        state->pressed = false;
        InvalidateRect(hwnd, nullptr, FALSE);
    }

    void SetPlaceOverlayEnabled(HWND hwnd, bool enabled)
    {
        PlaceOverlayState* state = PlaceOverlayStateFromWindow(hwnd);
        if (!state || state->enabled == enabled)
            return;

        state->enabled = enabled;
        state->hot = false;
        state->pressed = false;
        InvalidateRect(hwnd, nullptr, FALSE);
    }

    struct CategoryEnablement
    {
        bool hasPaintedCells = false;
        DWORD terrainMask = 0;
    };

    bool CategoryAppliesToPaintedTerrain(int index, const CategoryEnablement& enablement)
    {
        if (!enablement.hasPaintedCells)
            return true;
        const ObstacleButtonDefinition* definition =
            ObstacleButtonDefinitionAt(index);
        return definition &&
            (enablement.terrainMask & definition->terrainMask) != 0;
    }

    bool CategoryCanContributeToGeneration(int index, const CategoryEnablement& enablement)
    {
        const ObstacleButtonDefinition* definition =
            ObstacleButtonDefinitionAt(index);
        return enablement.hasPaintedCells &&
            definition &&
            (enablement.terrainMask & definition->terrainMask) != 0 &&
            s_categoryChecked[index];
    }

    bool AnyCheckedApplicableCategory(const CategoryEnablement& enablement)
    {
        EnsureCategoryCheckedInitialized();
        for (int index = 0; index < kObstacleButtonCount; ++index)
        {
            if (CategoryCanContributeToGeneration(index, enablement))
                return true;
        }
        return false;
    }

    CategoryEnablement GetCategoryEnablement(HWND toolbar)
    {
        CategoryEnablement enablement = {};
        const PaintedTerrainScan scan = ComputePaintedTerrainScan();
        const DWORD rawTerrainMask = scan.terrainMask;
        const bool nativePlaceEnabled = NativePlaceObstaclesEnabled(toolbar);

        enablement.terrainMask = rawTerrainMask;
        enablement.hasPaintedCells = scan.paintedCells != 0;

        const DWORD paintedValue = enablement.hasPaintedCells ? 1u : 0u;
        if (enablement.terrainMask != s_lastLoggedAppliedTerrainMask ||
            paintedValue != s_lastLoggedAppliedNativeEnabled)
        {
            s_lastLoggedAppliedTerrainMask = enablement.terrainMask;
            s_lastLoggedAppliedNativeEnabled = paintedValue;
            LogFormat(
                "Obstacle variants category: applied terrain mask=0x%08lX painted=%lu raw=0x%08lX nativeEnabled=%lu.",
                enablement.terrainMask,
                paintedValue,
                rawTerrainMask,
                nativePlaceEnabled ? 1u : 0u);
        }

        return enablement;
    }

    void RefreshCategoryButton(HWND hwnd, const CategoryEnablement& enablement)
    {
        CategoryButtonState* state = CategoryButtonStateFromWindow(hwnd);
        if (!state)
            return;

        SetCategoryButtonEnabled(
            hwnd,
            CategoryAppliesToPaintedTerrain(state->index, enablement));
    }

    void RefreshPlaceOverlay(VariantPanel& panel, const CategoryEnablement& enablement)
    {
        if (!panel.placeOverlay)
            return;

        SetWindowPos(
            panel.placeOverlay,
            HWND_TOP,
            0,
            0,
            0,
            0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        SetPlaceOverlayEnabled(
            panel.placeOverlay,
            AnyCheckedApplicableCategory(enablement));
    }

    void RefreshCategoryButtons(VariantPanel& panel)
    {
        if (!panel.layoutApplied || !panel.sourceToolbar)
            return;

        const bool custom = CustomObstacleModeEnabled();
        if (!custom && IsWindowVisible(panel.scrollHost))
        {
            HideObstaclePreview(nullptr);
            for (HWND button : panel.categoryButtons)
                if (button) SendMessageA(button, WM_MOUSELEAVE, 0, 0);
        }
        ShowWindow(panel.categoryLabel, custom ? SW_SHOWNA : SW_HIDE);
        ShowWindow(panel.scrollHost, custom ? SW_SHOWNA : SW_HIDE);
        ShowWindow(panel.placeOverlay, custom ? SW_SHOWNA : SW_HIDE);
        if (panel.modeHost)
        {
            auto* state = reinterpret_cast<ModeHostState*>(GetWindowLongPtrA(panel.modeHost, GWLP_USERDATA));
            if (state)
                for (int index = 0; index < 2; ++index)
                    SendMessageA(state->radios[index], BM_SETCHECK,
                        custom == (index == 1) ? BST_CHECKED : BST_UNCHECKED, 0);
        }
        if (!custom) return;

        if (panel.scrollHost && IsWindow(panel.scrollHost))
        {
            RECT hostRect = {};
            RECT parentRect = {};
            GetClientRect(panel.parent, &parentRect);
            if (RectInParent(panel.parent, panel.scrollHost, hostRect))
            {
                const int width = parentRect.right - hostRect.left - 6;
                const int height = parentRect.bottom - hostRect.top - 6;
                if (width <= 0 || height <= 0)
                    return;
                if (hostRect.right - hostRect.left != width ||
                    hostRect.bottom - hostRect.top != height)
                {
                    SetWindowPos(
                        panel.scrollHost,
                        nullptr,
                        hostRect.left,
                        hostRect.top,
                        width,
                        height,
                        SWP_NOZORDER | SWP_NOACTIVATE);
                }
            }
        }

        const CategoryEnablement enablement = GetCategoryEnablement(panel.sourceToolbar);
        for (HWND button : panel.categoryButtons)
            RefreshCategoryButton(button, enablement);
        RefreshPlaceOverlay(panel, enablement);
        RefreshCategoryHeaders(panel.scrollHost);
    }

    bool EnvValueEnabled(const char* value)
    {
        return _stricmp(value, "1") == 0 ||
            _stricmp(value, "true") == 0 ||
            _stricmp(value, "yes") == 0 ||
            _stricmp(value, "on") == 0;
    }

    bool VariantUiEnvEnabled()
    {
        char value[16] = {};
        const DWORD length = GetEnvironmentVariableA(kVariantUiEnvVar, value, sizeof(value));
        return length > 0 && length < sizeof(value) && EnvValueEnabled(value);
    }

    bool IsToolbarClass(HWND hwnd)
    {
        char className[64] = {};
        GetClassNameA(hwnd, className, sizeof(className));
        return strcmp(className, TOOLBARCLASSNAMEA) == 0;
    }

    bool ToolbarHasCommand(HWND toolbar, UINT commandId)
    {
        if (!IsToolbarClass(toolbar))
            return false;

        const LRESULT count = SendMessageA(toolbar, TB_BUTTONCOUNT, 0, 0);
        if (count <= 0 || count > 64)
            return false;

        for (int i = 0; i < count; ++i)
        {
            TBBUTTON button = {};
            if (!SendMessageA(toolbar, TB_GETBUTTON, i, reinterpret_cast<LPARAM>(&button)))
                continue;
            if (button.idCommand == static_cast<int>(commandId))
                return true;
        }

        return false;
    }

    int ToolbarCommandIndex(HWND toolbar, UINT commandId)
    {
        if (!IsToolbarClass(toolbar))
            return -1;

        const LRESULT count = SendMessageA(toolbar, TB_BUTTONCOUNT, 0, 0);
        if (count <= 0 || count > 128)
            return -1;

        for (int i = 0; i < count; ++i)
        {
            TBBUTTON button = {};
            if (!SendMessageA(toolbar, TB_GETBUTTON, i, reinterpret_cast<LPARAM>(&button)))
                continue;
            if (button.idCommand == static_cast<int>(commandId))
                return i;
        }

        return -1;
    }

    bool ToolbarCommandButton(HWND toolbar, UINT commandId, TBBUTTON& outButton)
    {
        const int index = ToolbarCommandIndex(toolbar, commandId);
        if (index < 0)
            return false;

        return SendMessageA(
            toolbar,
            TB_GETBUTTON,
            static_cast<WPARAM>(index),
            reinterpret_cast<LPARAM>(&outButton)) != 0;
    }

    bool ToolbarCommandRect(HWND toolbar, UINT commandId, RECT& rect)
    {
        const int index = ToolbarCommandIndex(toolbar, commandId);
        if (index < 0)
            return false;

        if (!SendMessageA(toolbar, TB_GETITEMRECT, static_cast<WPARAM>(index), reinterpret_cast<LPARAM>(&rect)))
            return false;

        return rect.right > rect.left && rect.bottom > rect.top;
    }

    bool RectInParent(HWND parent, HWND child, RECT& rect)
    {
        if (!GetWindowRect(child, &rect))
            return false;

        MapWindowPoints(HWND_DESKTOP, parent, reinterpret_cast<POINT*>(&rect), 2);
        return true;
    }

    HWND FirstDirectChild(HWND parent)
    {
        return GetWindow(parent, GW_CHILD);
    }

    HWND NextSibling(HWND child)
    {
        return GetWindow(child, GW_HWNDNEXT);
    }

    bool WindowTextEquals(HWND hwnd, const char* expected)
    {
        char text[64] = {};
        GetWindowTextA(hwnd, text, sizeof(text));
        return strcmp(text, expected) == 0;
    }

    bool FindBrushSection(
        HWND parent,
        int minTop,
        HWND& brushLabel,
        RECT& brushLabelRect,
        RECT& brushToolbarRect)
    {
        HWND bestLabel = nullptr;
        RECT bestLabelRect = {};
        int bestTop = INT_MAX;

        for (HWND child = FirstDirectChild(parent); child; child = NextSibling(child))
        {
            if (!IsWindowVisible(child) || !WindowTextEquals(child, "Brush"))
                continue;

            RECT rect = {};
            if (!RectInParent(parent, child, rect))
                continue;

            if (rect.top >= minTop && rect.top < bestTop)
            {
                bestTop = rect.top;
                bestLabel = child;
                bestLabelRect = rect;
            }
        }

        if (!bestLabel)
            return false;

        HWND bestToolbar = nullptr;
        RECT bestToolbarRect = {};
        bestTop = INT_MAX;

        for (HWND child = FirstDirectChild(parent); child; child = NextSibling(child))
        {
            if (!IsWindowVisible(child) || !IsToolbarClass(child))
                continue;

            RECT rect = {};
            if (!RectInParent(parent, child, rect))
                continue;

            if (rect.top > bestLabelRect.top && rect.top < bestTop)
            {
                bestTop = rect.top;
                bestToolbar = child;
                bestToolbarRect = rect;
            }
        }

        if (!bestToolbar)
            return false;

        brushLabel = bestLabel;
        brushLabelRect = bestLabelRect;
        brushToolbarRect = bestToolbarRect;
        return true;
    }

    CategoryButtonState* CategoryButtonStateFromWindow(HWND hwnd)
    {
        return reinterpret_cast<CategoryButtonState*>(GetWindowLongPtrA(hwnd, GWLP_USERDATA));
    }

    PlaceOverlayState* PlaceOverlayStateFromWindow(HWND hwnd)
    {
        return reinterpret_cast<PlaceOverlayState*>(GetWindowLongPtrA(hwnd, GWLP_USERDATA));
    }

    void DrawNativePlaceIcon(PlaceOverlayState* state, HDC hdc, const RECT& rect, bool enabled)
    {
        if (!state || !state->sourceToolbar)
            return;

        TBBUTTON button = {};
        if (!ToolbarCommandButton(state->sourceToolbar, kVanillaPlaceObstaclesCommand, button))
            return;

        HIMAGELIST imageList = reinterpret_cast<HIMAGELIST>(SendMessageA(
            state->sourceToolbar,
            enabled ? TB_GETIMAGELIST : TB_GETDISABLEDIMAGELIST,
            0,
            0));
        const bool usingDisabledImageList = imageList != nullptr;
        if (!imageList)
        {
            imageList = reinterpret_cast<HIMAGELIST>(SendMessageA(
                state->sourceToolbar,
                TB_GETIMAGELIST,
                0,
                0));
        }
        if (!imageList)
            return;

        int iconWidth = 0;
        int iconHeight = 0;
        if (!ImageList_GetIconSize(imageList, &iconWidth, &iconHeight) ||
            iconWidth <= 0 ||
            iconHeight <= 0)
            return;

        const int x = rect.left + ((rect.right - rect.left) - iconWidth) / 2;
        const int y = rect.top + ((rect.bottom - rect.top) - iconHeight) / 2;
        if (enabled || usingDisabledImageList)
        {
            ImageList_DrawEx(
                imageList,
                button.iBitmap,
                hdc,
                x,
                y,
                iconWidth,
                iconHeight,
                CLR_NONE,
                CLR_NONE,
                ILD_TRANSPARENT);
            return;
        }

        HICON icon = ImageList_GetIcon(imageList, button.iBitmap, ILD_TRANSPARENT);
        if (!icon)
            return;

        DrawStateA(
            hdc,
            nullptr,
            nullptr,
            reinterpret_cast<LPARAM>(icon),
            0,
            x,
            y,
            iconWidth,
            iconHeight,
            DST_ICON | DSS_DISABLED);
        DestroyIcon(icon);
    }

    void DrawPlaceOverlay(HWND hwnd, HDC hdc)
    {
        RECT rect = {};
        GetClientRect(hwnd, &rect);

        PlaceOverlayState* state = PlaceOverlayStateFromWindow(hwnd);
        const bool enabled = state && state->enabled;
        const bool hot = enabled && state->hot;
        const bool pressed = enabled && state->pressed;

        HBRUSH background = CreateSolidBrush(GetSysColor(COLOR_BTNFACE));
        FillRect(hdc, &rect, background);
        DeleteObject(background);

        if (enabled && (hot || pressed))
        {
            RECT fillRect = rect;
            InflateRect(&fillRect, -1, -1);
            HBRUSH hotBrush = CreateSolidBrush(pressed ? RGB(204, 232, 255) : RGB(229, 245, 255));
            FillRect(hdc, &fillRect, hotBrush);
            DeleteObject(hotBrush);

            HPEN borderPen = CreatePen(PS_SOLID, 1, RGB(190, 232, 255));
            HGDIOBJ oldPen = SelectObject(hdc, borderPen);
            HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
            RoundRect(hdc, rect.left, rect.top, rect.right, rect.bottom, 3, 3);
            SelectObject(hdc, oldBrush);
            SelectObject(hdc, oldPen);
            DeleteObject(borderPen);
        }

        DrawNativePlaceIcon(state, hdc, rect, enabled);
    }

    bool DrawObstacleIcon(
        HDC hdc,
        const RECT& targetRect,
        const ObstacleRenderedImage* icon,
        COLORREF background)
    {
        constexpr int kIconExtent = 36;
        if (!icon || icon->width != kIconExtent || icon->height != kIconExtent ||
            icon->bgra.size() != kIconExtent * kIconExtent * 4)
            return false;

        BYTE pixels[kIconExtent * kIconExtent * 4] = {};
        const unsigned int backgroundRed = GetRValue(background);
        const unsigned int backgroundGreen = GetGValue(background);
        const unsigned int backgroundBlue = GetBValue(background);

        for (int pixel = 0; pixel < kIconExtent * kIconExtent; ++pixel)
        {
            const unsigned int sourceOffset = pixel * 4;
            const unsigned int alpha = icon->bgra[sourceOffset + 3];
            const unsigned int inverseAlpha = 255 - alpha;
            pixels[sourceOffset + 0] = static_cast<BYTE>(
                (icon->bgra[sourceOffset] * alpha + backgroundBlue * inverseAlpha + 127) / 255);
            pixels[sourceOffset + 1] = static_cast<BYTE>(
                (icon->bgra[sourceOffset + 1] * alpha + backgroundGreen * inverseAlpha + 127) / 255);
            pixels[sourceOffset + 2] = static_cast<BYTE>(
                (icon->bgra[sourceOffset + 2] * alpha + backgroundRed * inverseAlpha + 127) / 255);
        }

        BITMAPINFO bitmapInfo = {};
        bitmapInfo.bmiHeader.biSize = sizeof(bitmapInfo.bmiHeader);
        bitmapInfo.bmiHeader.biWidth = kIconExtent;
        bitmapInfo.bmiHeader.biHeight = -kIconExtent;
        bitmapInfo.bmiHeader.biPlanes = 1;
        bitmapInfo.bmiHeader.biBitCount = 32;
        bitmapInfo.bmiHeader.biCompression = BI_RGB;

        const int x = targetRect.left + (targetRect.right - targetRect.left - kIconExtent) / 2;
        const int y = targetRect.top + (targetRect.bottom - targetRect.top - kIconExtent) / 2;
        return SetDIBitsToDevice(
                   hdc,
                   x,
                   y,
                   kIconExtent,
                   kIconExtent,
                   0,
                   0,
                   0,
                   kIconExtent,
                   pixels,
                   &bitmapInfo,
                   DIB_RGB_COLORS) != 0;
    }

    void DrawCategoryButton(HWND hwnd, HDC hdc)
    {
        RECT rect = {};
        GetClientRect(hwnd, &rect);

        CategoryButtonState* state = CategoryButtonStateFromWindow(hwnd);
        const bool checked = !state || state->checked;
        const bool enabled = state && state->enabled;
        const bool hot = enabled && state->hot;
        const bool pressed = enabled && state->pressed;

        COLORREF buttonBackground = GetSysColor(COLOR_BTNFACE);
        HBRUSH background = CreateSolidBrush(buttonBackground);
        FillRect(hdc, &rect, background);
        DeleteObject(background);

        if (enabled && (checked || hot || pressed))
        {
            const COLORREF fill =
                pressed ? RGB(174, 216, 247) :
                hot ? (checked ? RGB(198, 228, 250) : RGB(214, 238, 255)) :
                RGB(210, 235, 252);
            buttonBackground = fill;
            HBRUSH fillBrush = CreateSolidBrush(fill);
            RECT fillRect = rect;
            InflateRect(&fillRect, -1, -1);
            FillRect(hdc, &fillRect, fillBrush);
            DeleteObject(fillBrush);

            HPEN framePen = CreatePen(PS_SOLID, 1, RGB(171, 222, 252));
            HGDIOBJ oldPen = SelectObject(hdc, framePen);
            HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
            RoundRect(hdc, rect.left, rect.top, rect.right, rect.bottom, 3, 3);
            SelectObject(hdc, oldBrush);
            SelectObject(hdc, oldPen);
            DeleteObject(framePen);
        }

        const int glyph = kCategoryGlyphExtent;
        RECT glyphRect =
        {
            (rect.right - glyph) / 2,
            (rect.bottom - glyph) / 2,
            (rect.right + glyph) / 2,
            (rect.bottom + glyph) / 2,
        };

        const ObstacleRenderedImage* icon = ObstacleDefIconForButton(
            state ? state->index : -1,
            enabled);
        if (DrawObstacleIcon(hdc, glyphRect, icon, buttonBackground))
            return;

        HPEN outline = CreatePen(
            PS_SOLID,
            2,
            enabled ? RGB(128, 128, 255) : RGB(176, 176, 176));
        HGDIOBJ oldPen = SelectObject(hdc, outline);
        HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
        Rectangle(hdc, glyphRect.left + 4, glyphRect.top + 4, glyphRect.right - 4, glyphRect.bottom - 4);
        SelectObject(hdc, oldBrush);
        SelectObject(hdc, oldPen);
        DeleteObject(outline);
    }

    LRESULT CALLBACK CategoryButtonWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        switch (message)
        {
        case WM_NCCREATE:
        {
            CREATESTRUCTA* create = reinterpret_cast<CREATESTRUCTA*>(lParam);
            SetWindowLongPtrA(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
            return TRUE;
        }
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT:
        {
            PAINTSTRUCT ps = {};
            HDC hdc = BeginPaint(hwnd, &ps);
            DrawCategoryButton(hwnd, hdc);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_TIMER:
        {
            CategoryButtonState* state = CategoryButtonStateFromWindow(hwnd);
            if (wParam == kCategoryRefreshTimerId && state && state->index == 0)
                RefreshCategoryButtonsForToolbar(state->sourceToolbar);
            else if (wParam == kCategoryPreviewTimerId)
            {
                KillTimer(hwnd, kCategoryPreviewTimerId);
                if (state && state->mouseInside && IsWindowVisible(hwnd))
                    ShowObstaclePreview(s_pluginModule, hwnd, state->index);
            }
            return 0;
        }
        case WM_MOUSEMOVE:
        {
            CategoryButtonState* state = CategoryButtonStateFromWindow(hwnd);
            if (state && !state->mouseInside)
            {
                state->mouseInside = true;
                TRACKMOUSEEVENT track = {};
                track.cbSize = sizeof(track);
                track.dwFlags = TME_LEAVE;
                track.hwndTrack = hwnd;
                TrackMouseEvent(&track);
                if (ObstaclePreviewAvailable(state->index))
                {
                    SetTimer(
                        hwnd,
                        kCategoryPreviewTimerId,
                        kCategoryPreviewDelayMs,
                        nullptr);
                }
            }
            if (state && state->enabled && !state->hot)
            {
                state->hot = true;
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        }
        case WM_SHOWWINDOW:
            if (wParam) break;
            // A collapsed section must not leave a pending hover preview.
            [[fallthrough]];
        case WM_MOUSELEAVE:
        {
            CategoryButtonState* state = CategoryButtonStateFromWindow(hwnd);
            if (state)
            {
                state->mouseInside = false;
                state->hot = false;
                state->pressed = false;
                KillTimer(hwnd, kCategoryPreviewTimerId);
                HideObstaclePreview(hwnd);
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        }
        case WM_MOUSEWHEEL:
        {
            CategoryButtonState* state = CategoryButtonStateFromWindow(hwnd);
            if (state)
            {
                state->mouseInside = false;
                state->hot = false;
                KillTimer(hwnd, kCategoryPreviewTimerId);
                HideObstaclePreview(hwnd);
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return SendMessageA(GetParent(hwnd), message, wParam, lParam);
        }
        case WM_MOUSEACTIVATE:
            return MA_NOACTIVATE;
        case WM_LBUTTONDOWN:
        {
            CategoryButtonState* state = CategoryButtonStateFromWindow(hwnd);
            if (state && state->enabled)
            {
                KillTimer(hwnd, kCategoryPreviewTimerId);
                HideObstaclePreview(hwnd);
                state->pressed = true;
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        }
        case WM_LBUTTONUP:
        {
            CategoryButtonState* state = CategoryButtonStateFromWindow(hwnd);
            if (state && state->enabled)
            {
                POINT point =
                {
                    static_cast<SHORT>(LOWORD(lParam)),
                    static_cast<SHORT>(HIWORD(lParam)),
                };
                RECT rect = {};
                GetClientRect(hwnd, &rect);
                if (state->pressed && PtInRect(&rect, point))
                {
                    state->checked = !state->checked;
                    if (state->index >= 0 && state->index < kObstacleButtonCount)
                        s_categoryChecked[state->index] = state->checked;
                    RefreshCategoryHeaders(GetParent(hwnd));
                    RefreshCategoryButtonsForToolbar(state->sourceToolbar);
                }
                state->pressed = false;
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        }
        case WM_CANCELMODE:
        {
            CategoryButtonState* state = CategoryButtonStateFromWindow(hwnd);
            if (state)
            {
                state->pressed = false;
                KillTimer(hwnd, kCategoryPreviewTimerId);
                HideObstaclePreview(hwnd);
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        }
        case WM_NCDESTROY:
        {
            KillTimer(hwnd, kCategoryRefreshTimerId);
            KillTimer(hwnd, kCategoryPreviewTimerId);
            HideObstaclePreview(hwnd);
            CategoryButtonState* state = CategoryButtonStateFromWindow(hwnd);
            SetWindowLongPtrA(hwnd, GWLP_USERDATA, 0);
            delete state;
            return 0;
        }
        }

        return DefWindowProcA(hwnd, message, wParam, lParam);
    }

    LRESULT CALLBACK PlaceOverlayWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        switch (message)
        {
        case WM_NCCREATE:
        {
            CREATESTRUCTA* create = reinterpret_cast<CREATESTRUCTA*>(lParam);
            SetWindowLongPtrA(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
            return TRUE;
        }
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT:
        {
            PAINTSTRUCT ps = {};
            HDC hdc = BeginPaint(hwnd, &ps);
            DrawPlaceOverlay(hwnd, hdc);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_MOUSEMOVE:
        {
            PlaceOverlayState* state = PlaceOverlayStateFromWindow(hwnd);
            if (state && state->enabled && !state->hot)
            {
                state->hot = true;
                TRACKMOUSEEVENT track = {};
                track.cbSize = sizeof(track);
                track.dwFlags = TME_LEAVE;
                track.hwndTrack = hwnd;
                TrackMouseEvent(&track);
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        }
        case WM_MOUSELEAVE:
        {
            PlaceOverlayState* state = PlaceOverlayStateFromWindow(hwnd);
            if (state)
            {
                state->hot = false;
                state->pressed = false;
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        }
        case WM_MOUSEACTIVATE:
            return MA_NOACTIVATE;
        case WM_LBUTTONDOWN:
        {
            PlaceOverlayState* state = PlaceOverlayStateFromWindow(hwnd);
            if (state && state->enabled)
            {
                state->pressed = true;
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        }
        case WM_LBUTTONUP:
        {
            PlaceOverlayState* state = PlaceOverlayStateFromWindow(hwnd);
            if (state && state->enabled)
            {
                POINT point =
                {
                    static_cast<SHORT>(LOWORD(lParam)),
                    static_cast<SHORT>(HIWORD(lParam)),
                };
                RECT rect = {};
                GetClientRect(hwnd, &rect);
                const bool shouldRun = state->pressed && PtInRect(&rect, point);
                state->pressed = false;
                InvalidateRect(hwnd, nullptr, FALSE);
                if (shouldRun)
                {
                    RunObstacleVariantCommand(
                        state->parent,
                        state->sourceToolbar,
                        ObstacleGenerationFilterMode::ButtonSelection);
                    RefreshCategoryButtonsForToolbar(state->sourceToolbar);
                }
            }
            return 0;
        }
        case WM_CANCELMODE:
        {
            PlaceOverlayState* state = PlaceOverlayStateFromWindow(hwnd);
            if (state)
            {
                state->pressed = false;
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        }
        case WM_NCDESTROY:
        {
            PlaceOverlayState* state = PlaceOverlayStateFromWindow(hwnd);
            if (state && state->tooltip)
            {
                DestroyWindow(state->tooltip);
                state->tooltip = nullptr;
            }
            SetWindowLongPtrA(hwnd, GWLP_USERDATA, 0);
            delete state;
            return 0;
        }
        }

        return DefWindowProcA(hwnd, message, wParam, lParam);
    }

    ScrollHostState* ScrollHostStateFromWindow(HWND hwnd)
    {
        return reinterpret_cast<ScrollHostState*>(GetWindowLongPtrA(hwnd, GWLP_USERDATA));
    }

    int SelectedCountForCategory(int category)
    {
        EnsureCategoryCheckedInitialized();
        int count = 0;
        for (int index = 0; index < kObstacleButtonCount; ++index)
        {
            const auto* definition = ObstacleButtonDefinitionAt(index);
            if (definition && definition->categoryIndex == category && s_categoryChecked[index])
                ++count;
        }
        return count;
    }

    void RefreshCategoryHeaders(HWND host)
    {
        auto* state = ScrollHostStateFromWindow(host);
        if (!state) return;
        for (int category = 0; category < kObstacleCategoryCount; ++category)
        {
            const int count = SelectedCountForCategory(category);
            if (count == state->selectedCounts[category]) continue;
            state->selectedCounts[category] = count;
            char caption[128] = {};
            sprintf_s(caption, "%s (%d)", ObstacleCategoryName(category), count);
            SetWindowTextA(state->headers[category], caption);
            InvalidateRect(state->headers[category], nullptr, FALSE);
        }
    }

    bool AllCategoriesExpanded()
    {
        for (bool expanded : s_categoryExpanded)
            if (!expanded) return false;
        return true;
    }

    const char* ExpansionActionText()
    {
        return AllCategoriesExpanded() ? "Collapse All" : "Expand All";
    }

    void LayoutScrollHost(HWND hwnd, ScrollHostState& state)
    {
        InitializeScrollHostLayout(state);
        RECT client = {};
        GetClientRect(hwnd, &client);
        const int page = client.bottom - client.top;
        const int maximum = state.contentHeight > page ? state.contentHeight - page : 0;
        if (state.scrollPosition > maximum)
            state.scrollPosition = maximum;
        if (state.scrollPosition < 0)
            state.scrollPosition = 0;

        SCROLLINFO info = {};
        info.cbSize = sizeof(info);
        info.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
        info.nMin = 0;
        info.nMax = state.contentHeight > 0 ? state.contentHeight - 1 : 0;
        info.nPage = page > 0 ? static_cast<UINT>(page) : 0;
        info.nPos = state.scrollPosition;
        SetScrollInfo(hwnd, SB_VERT, &info, TRUE);
        // Scrollbar visibility may change the available header width.
        GetClientRect(hwnd, &client);
        // Reserve the scrollbar's space even while it is hidden so the global
        // expansion control keeps the same horizontal position on expansion.
        const int expansionRight = client.right -
            ((GetWindowLongA(hwnd, GWL_STYLE) & WS_VSCROLL) ? 0 : GetSystemMetrics(SM_CXVSCROLL));

        const UINT positionFlags = SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOREDRAW;
        for (int index = 0; index < kObstacleButtonCount; ++index)
        {
            if (state.buttons[index] &&
                !s_categoryExpanded[ObstacleButtonDefinitionAt(index)->categoryIndex])
                SendMessageA(state.buttons[index], WM_MOUSELEAVE, 0, 0);
        }
        HDWP deferred = BeginDeferWindowPos(kObstacleButtonCount);
        if (deferred)
        {
            for (int index = 0; index < kObstacleButtonCount; ++index)
            {
                if (!state.buttons[index])
                    continue;
                deferred = DeferWindowPos(
                    deferred,
                    state.buttons[index],
                    nullptr,
                    state.buttonX[index],
                    state.buttonY[index] - state.scrollPosition,
                    kCategoryButtonExtent,
                    kCategoryButtonExtent,
                    positionFlags | (s_categoryExpanded[ObstacleButtonDefinitionAt(index)->categoryIndex]
                        ? SWP_SHOWWINDOW : SWP_HIDEWINDOW));
                if (!deferred)
                    break;
            }
        }

        bool positioned = deferred && EndDeferWindowPos(deferred) != FALSE;
        if (!positioned)
        {
            for (int index = 0; index < kObstacleButtonCount; ++index)
            {
                if (!state.buttons[index])
                    continue;
                SetWindowPos(
                    state.buttons[index],
                    nullptr,
                    state.buttonX[index],
                    state.buttonY[index] - state.scrollPosition,
                    kCategoryButtonExtent,
                    kCategoryButtonExtent,
                    positionFlags | (s_categoryExpanded[ObstacleButtonDefinitionAt(index)->categoryIndex]
                        ? SWP_SHOWWINDOW : SWP_HIDEWINDOW));
            }
        }

        for (int index = 0; index < kSelectionActionCount; ++index)
        {
            const bool global = index >= kGlobalSelectionAction;
            const int extent = global ? kGlobalSelectionButtonExtent : kSelectionButtonExtent;
            if (state.selectionButtons[index])
                SetWindowPos(state.selectionButtons[index], nullptr,
                    index == kExpansionAction ? (expansionRight > extent ? expansionRight - extent : 0) :
                        (index % 2) * (extent + (global ? 12 : 6)),
                    (global ? 0 : state.selectionY[index / 2]) - state.scrollPosition,
                    extent, extent, positionFlags |
                    (global || s_categoryExpanded[index / 2] ? SWP_SHOWWINDOW : SWP_HIDEWINDOW));
        }

        if (state.selectionTooltip && state.selectionButtons[kExpansionAction])
        {
            TOOLINFOA tool = {};
            tool.cbSize = sizeof(tool);
            tool.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
            tool.hwnd = hwnd;
            tool.uId = reinterpret_cast<UINT_PTR>(state.selectionButtons[kExpansionAction]);
            tool.lpszText = const_cast<char*>(ExpansionActionText());
            SendMessageA(state.selectionTooltip, TTM_UPDATETIPTEXTA, 0, reinterpret_cast<LPARAM>(&tool));
        }

        for (int category = 0; category < kObstacleCategoryCount; ++category)
        {
            if (state.headers[category])
                SetWindowPos(state.headers[category], nullptr, 0,
                    state.headerY[category] - state.scrollPosition,
                    client.right, kCategoryHeaderHeight, positionFlags);
        }

        RedrawWindow(
            hwnd,
            nullptr,
            nullptr,
            RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
    }

    void ScrollHostTo(HWND hwnd, int position)
    {
        ScrollHostState* state = ScrollHostStateFromWindow(hwnd);
        if (!state || state->scrollPosition == position)
            return;
        state->scrollPosition = position;
        LayoutScrollHost(hwnd, *state);
    }

    LRESULT CALLBACK SelectionButtonSubclass(
        HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam,
        UINT_PTR subclassId, DWORD_PTR)
    {
        // Keep native drawing, but do not let BUTTON's mouse handler take focus
        // away from the editor's accelerator target.
        switch (message)
        {
        case WM_MOUSEACTIVATE:
            return MA_NOACTIVATE;
        case WM_LBUTTONDOWN:
        case WM_LBUTTONDBLCLK:
            SetCapture(hwnd);
            SendMessageA(hwnd, BM_SETSTATE, TRUE, 0);
            return 0;
        case WM_MOUSEMOVE:
            if (GetCapture() == hwnd)
            {
                RECT rect = {};
                GetClientRect(hwnd, &rect);
                POINT point = { static_cast<SHORT>(LOWORD(lParam)), static_cast<SHORT>(HIWORD(lParam)) };
                SendMessageA(hwnd, BM_SETSTATE, PtInRect(&rect, point), 0);
                return 0;
            }
            break;
        case WM_LBUTTONUP:
            if (GetCapture() == hwnd)
            {
                RECT rect = {};
                GetClientRect(hwnd, &rect);
                POINT point = { static_cast<SHORT>(LOWORD(lParam)), static_cast<SHORT>(HIWORD(lParam)) };
                const bool clicked = PtInRect(&rect, point) != FALSE;
                ReleaseCapture();
                SendMessageA(hwnd, BM_SETSTATE, FALSE, 0);
                if (clicked)
                    SendMessageA(GetParent(hwnd), WM_COMMAND,
                        MAKEWPARAM(GetDlgCtrlID(hwnd), BN_CLICKED), reinterpret_cast<LPARAM>(hwnd));
            }
            return 0;
        case WM_CANCELMODE:
            if (GetCapture() == hwnd) ReleaseCapture();
            SendMessageA(hwnd, BM_SETSTATE, FALSE, 0);
            break;
        case WM_CAPTURECHANGED:
            SendMessageA(hwnd, BM_SETSTATE, FALSE, 0);
            break;
        case WM_MOUSEWHEEL:
            return SendMessageA(GetParent(hwnd), message, wParam, lParam);
        case WM_NCDESTROY:
            RemoveWindowSubclass(hwnd, SelectionButtonSubclass, subclassId);
            break;
        }
        return DefSubclassProc(hwnd, message, wParam, lParam);
    }

    LRESULT CALLBACK ModeHostWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        auto* state = reinterpret_cast<ModeHostState*>(GetWindowLongPtrA(hwnd, GWLP_USERDATA));
        if (message == WM_NCCREATE)
        {
            const auto* create = reinterpret_cast<CREATESTRUCTA*>(lParam);
            SetWindowLongPtrA(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
            return TRUE;
        }
        if (message == WM_COMMAND && HIWORD(wParam) == BN_CLICKED && state)
        {
            const int index = LOWORD(wParam) - 1;
            if (index >= 0 && index < 2 && reinterpret_cast<HWND>(lParam) == state->radios[index])
            {
                HideObstaclePreview(nullptr);
                SetCustomObstacleMode(index == 1);
                for (int radio = 0; radio < 2; ++radio)
                    SendMessageA(state->radios[radio], BM_SETCHECK,
                        radio == index ? BST_CHECKED : BST_UNCHECKED, 0);
                RefreshAllCategoryButtons();
                // The minimap color policy also follows the selected mode.
                RedrawWindow(GetAncestor(hwnd, GA_ROOT), nullptr, nullptr,
                    RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
                return 0;
            }
        }
        if (message == WM_NCDESTROY)
        {
            SetWindowLongPtrA(hwnd, GWLP_USERDATA, 0);
            delete state;
        }
        return DefWindowProcA(hwnd, message, wParam, lParam);
    }

    HWND CreateModeHost(HWND parent, int x, int y, int width, HFONT font)
    {
        WNDCLASSEXA windowClass = {};
        windowClass.cbSize = sizeof(windowClass);
        windowClass.lpfnWndProc = ModeHostWndProc;
        windowClass.hInstance = s_pluginModule;
        windowClass.hCursor = LoadCursorA(nullptr, IDC_ARROW);
        windowClass.hbrBackground = GetSysColorBrush(COLOR_BTNFACE);
        windowClass.lpszClassName = "HotaMapEdPluginObstacleMode";
        if (!RegisterClassExA(&windowClass) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
            return nullptr;
        auto* state = new (std::nothrow) ModeHostState();
        if (!state) return nullptr;
        HWND host = CreateWindowExA(0, windowClass.lpszClassName, "",
            WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN, x, y, width, 22,
            parent, nullptr, s_pluginModule, state);
        if (!host) { delete state; return nullptr; }
        bool created = true;
        const int radioWidth = width / 2 < 88 ? width / 2 : 88;
        for (int index = 0; index < 2; ++index)
        {
            // Keep the options together on wide panels and fit narrow panels.
            state->radios[index] = CreateWindowExA(0, "BUTTON", index ? "Custom" : "Default",
                WS_CHILD | WS_VISIBLE | BS_RADIOBUTTON, index * radioWidth, 0,
                radioWidth, 22, host, reinterpret_cast<HMENU>(static_cast<INT_PTR>(index + 1)),
                s_pluginModule, nullptr);
            HWND radio = state->radios[index];
            if (!radio || !SetWindowSubclass(radio, SelectionButtonSubclass, 1, 0))
            { created = false; break; }
            SendMessageA(radio, WM_SETFONT,
                reinterpret_cast<WPARAM>(font ? font : GetStockObject(DEFAULT_GUI_FONT)), TRUE);
            SendMessageA(radio, BM_SETCHECK,
                CustomObstacleModeEnabled() == (index == 1) ? BST_CHECKED : BST_UNCHECKED, 0);
        }
        if (!created) { DestroyWindow(host); return nullptr; }
        return host;
    }

    LRESULT CALLBACK ScrollHostWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        switch (message)
        {
        case WM_NCCREATE:
        {
            CREATESTRUCTA* create = reinterpret_cast<CREATESTRUCTA*>(lParam);
            SetWindowLongPtrA(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
            return TRUE;
        }
        case WM_ERASEBKGND:
            return 1;
        case WM_NOTIFY:
        {
            const auto* draw = reinterpret_cast<NMCUSTOMDRAW*>(lParam);
            ScrollHostState* state = ScrollHostStateFromWindow(hwnd);
            if (!draw || !state || draw->hdr.code != NM_CUSTOMDRAW) break;
            const int action = static_cast<int>(draw->hdr.idFrom) - kSelectionCommandBase;
            if (action < 0 || action >= kSelectionActionCount ||
                draw->hdr.hwndFrom != state->selectionButtons[action]) break;
            if (draw->dwDrawStage == CDDS_PREPAINT) return CDRF_NOTIFYPOSTPAINT;
            if (draw->dwDrawStage == CDDS_POSTPAINT)
            {
                RECT client = {};
                GetClientRect(draw->hdr.hwndFrom, &client);
                const int offset = (draw->uItemState & CDIS_SELECTED) ? 1 : 0;
                const int x = (client.right - 1) / 2 + offset;
                const int y = (client.bottom - 1) / 2 + offset;
                HBRUSH ink = GetSysColorBrush((draw->uItemState & CDIS_DISABLED)
                    ? COLOR_GRAYTEXT : COLOR_BTNTEXT);
                if (action == kExpansionAction)
                {
                    HPEN pen = CreatePen(PS_SOLID, 2, GetSysColor(
                        (draw->uItemState & CDIS_DISABLED) ? COLOR_GRAYTEXT : COLOR_BTNTEXT));
                    if (pen)
                    {
                        HGDIOBJ old = SelectObject(draw->hdc, pen);
                        const int direction = AllCategoriesExpanded() ? -1 : 1;
                        for (int row = -1; row <= 1; row += 2)
                        {
                            const int centerY = y + row * 3;
                            POINT points[] = { { x - 5, centerY - direction * 2 },
                                { x, centerY + direction * 2 }, { x + 5, centerY - direction * 2 } };
                            Polyline(draw->hdc, points, 3);
                        }
                        SelectObject(draw->hdc, old);
                        DeleteObject(pen);
                    }
                    return CDRF_DODEFAULT;
                }
                const bool global = action >= kGlobalSelectionAction;
                const int radius = global ? 6 : 3;
                const int thickness = global ? 2 : 1;
                RECT horizontal = { x - radius, y, x + radius + thickness, y + thickness };
                FillRect(draw->hdc, &horizontal, ink);
                if (action % 2 == 0)
                {
                    RECT vertical = { x, y - radius, x + thickness, y + radius + thickness };
                    FillRect(draw->hdc, &vertical, ink);
                }
                return CDRF_DODEFAULT;
            }
            break;
        }
        case WM_COMMAND:
        {
            ScrollHostState* state = ScrollHostStateFromWindow(hwnd);
            const int category = LOWORD(wParam) - kCategoryHeaderCommandBase;
            if (state && HIWORD(wParam) == BN_CLICKED && category >= 0 &&
                category < kObstacleCategoryCount &&
                reinterpret_cast<HWND>(lParam) == state->headers[category])
            {
                HideObstaclePreview(nullptr);
                s_categoryExpanded[category] = !s_categoryExpanded[category];
                LayoutScrollHost(hwnd, *state);
                return 0;
            }
            const int action = LOWORD(wParam) - kSelectionCommandBase;
            if (!state || HIWORD(wParam) != BN_CLICKED || action < 0 ||
                action >= kSelectionActionCount ||
                reinterpret_cast<HWND>(lParam) != state->selectionButtons[action])
                break;
            HideObstaclePreview(nullptr);
            if (action == kExpansionAction)
            {
                const bool expand = !AllCategoriesExpanded();
                for (bool& expanded : s_categoryExpanded) expanded = expand;
                LayoutScrollHost(hwnd, *state);
                return 0;
            }
            EnsureCategoryCheckedInitialized();
            const bool checked = action % 2 == 0;
            for (int index = 0; index < kObstacleButtonCount; ++index)
            {
                const auto* definition = ObstacleButtonDefinitionAt(index);
                if (!definition || (action < kGlobalSelectionAction &&
                    definition->categoryIndex != action / 2)) continue;
                s_categoryChecked[index] = checked;
                auto* button = CategoryButtonStateFromWindow(state->buttons[index]);
                if (button) button->checked = checked;
                InvalidateRect(state->buttons[index], nullptr, FALSE);
            }
            RefreshCategoryHeaders(hwnd);
            RefreshCategoryButtonsForToolbar(state->sourceToolbar);
            return 0;
        }
        case WM_DRAWITEM:
        {
            auto* state = ScrollHostStateFromWindow(hwnd);
            const auto* draw = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
            if (!state || !draw || draw->CtlType != ODT_BUTTON) break;
            const int category = static_cast<int>(draw->CtlID) - kCategoryHeaderCommandBase;
            if (category < 0 || category >= kObstacleCategoryCount ||
                draw->hwndItem != state->headers[category]) break;
            RECT rect = draw->rcItem;
            HBRUSH background = CreateSolidBrush(RGB(192, 192, 192));
            FillRect(draw->hDC, &rect, background);
            DeleteObject(background);
            RECT separator = { rect.left, rect.bottom - 1, rect.right, rect.bottom };
            FillRect(draw->hDC, &separator, GetSysColorBrush(COLOR_BTNFACE));
            const int middle = (rect.top + rect.bottom) / 2;
            POINT arrow[3];
            if (s_categoryExpanded[category])
            {
                arrow[0] = { 5, middle - 2 }; arrow[1] = { 13, middle - 2 };
                arrow[2] = { 9, middle + 2 };
            }
            else
            {
                arrow[0] = { 7, middle - 4 }; arrow[1] = { 7, middle + 4 };
                arrow[2] = { 11, middle };
            }
            HGDIOBJ oldBrush = SelectObject(draw->hDC, GetSysColorBrush(COLOR_BTNTEXT));
            HGDIOBJ oldPen = SelectObject(draw->hDC, GetStockObject(NULL_PEN));
            Polygon(draw->hDC, arrow, 3);
            SelectObject(draw->hDC, oldPen);
            SelectObject(draw->hDC, oldBrush);
            HFONT font = state->selectedCounts[category] ? state->headerBoldFont : state->headerFont;
            HGDIOBJ oldFont = SelectObject(draw->hDC, font ? font : GetStockObject(DEFAULT_GUI_FONT));
            SetBkMode(draw->hDC, TRANSPARENT);
            SetTextColor(draw->hDC, GetSysColor(COLOR_BTNTEXT));
            char caption[128] = {};
            GetWindowTextA(draw->hwndItem, caption, _countof(caption));
            rect.left += 19;
            DrawTextA(draw->hDC, caption, -1, &rect,
                DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
            SelectObject(draw->hDC, oldFont);
            return TRUE;
        }
        case WM_SIZE:
        {
            ScrollHostState* state = ScrollHostStateFromWindow(hwnd);
            if (state)
                LayoutScrollHost(hwnd, *state);
            return 0;
        }
        case WM_VSCROLL:
        {
            ScrollHostState* state = ScrollHostStateFromWindow(hwnd);
            if (!state)
                return 0;
            RECT client = {};
            GetClientRect(hwnd, &client);
            int position = state->scrollPosition;
            switch (LOWORD(wParam))
            {
            case SB_LINEUP: position -= kScrollWheelStep; break;
            case SB_LINEDOWN: position += kScrollWheelStep; break;
            case SB_PAGEUP: position -= client.bottom - client.top; break;
            case SB_PAGEDOWN: position += client.bottom - client.top; break;
            case SB_THUMBPOSITION:
            case SB_THUMBTRACK:
            {
                SCROLLINFO info = {};
                info.cbSize = sizeof(info);
                info.fMask = SIF_TRACKPOS;
                GetScrollInfo(hwnd, SB_VERT, &info);
                position = info.nTrackPos;
                break;
            }
            case SB_TOP: position = 0; break;
            case SB_BOTTOM: position = state->contentHeight; break;
            default: return 0;
            }
            ScrollHostTo(hwnd, position);
            return 0;
        }
        case WM_MOUSEWHEEL:
        {
            HideObstaclePreview(nullptr);
            ScrollHostState* state = ScrollHostStateFromWindow(hwnd);
            if (state)
            {
                const int notches = GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA;
                ScrollHostTo(hwnd, state->scrollPosition - notches * kScrollWheelStep * 3);
            }
            return 0;
        }
        case WM_PAINT:
        {
            PAINTSTRUCT paint = {};
            HDC hdc = BeginPaint(hwnd, &paint);
            RECT client = {};
            GetClientRect(hwnd, &client);
            FillRect(hdc, &client, reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1));
            EndPaint(hwnd, &paint);
            return 0;
        }
        case WM_NCDESTROY:
        {
            ScrollHostState* state = ScrollHostStateFromWindow(hwnd);
            if (state && state->selectionTooltip) DestroyWindow(state->selectionTooltip);
            if (state && state->headerFont) DeleteObject(state->headerFont);
            if (state && state->headerBoldFont) DeleteObject(state->headerBoldFont);
            SetWindowLongPtrA(hwnd, GWLP_USERDATA, 0);
            delete state;
            return 0;
        }
        }
        return DefWindowProcA(hwnd, message, wParam, lParam);
    }

    bool EnsureScrollHostClass()
    {
        if (s_scrollHostClass)
            return true;

        WNDCLASSEXA windowClass = {};
        windowClass.cbSize = sizeof(windowClass);
        windowClass.lpfnWndProc = ScrollHostWndProc;
        windowClass.hInstance = s_pluginModule;
        windowClass.hCursor = LoadCursorA(nullptr, IDC_ARROW);
        windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
        windowClass.lpszClassName = "HotaMapEdPluginObstacleScrollHost";
        s_scrollHostClass = RegisterClassExA(&windowClass);
        if (!s_scrollHostClass && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        {
            LogFormat("Obstacle variants: failed to register scroll host class, gle=%lu.", GetLastError());
            return false;
        }
        if (!s_scrollHostClass)
            s_scrollHostClass = 1;
        return true;
    }

    bool EnsureCategoryButtonClass()
    {
        if (s_categoryButtonClass)
            return true;

        WNDCLASSEXA windowClass = {};
        windowClass.cbSize = sizeof(windowClass);
        windowClass.lpfnWndProc = CategoryButtonWndProc;
        windowClass.hInstance = s_pluginModule;
        windowClass.hCursor = LoadCursorA(nullptr, IDC_ARROW);
        windowClass.lpszClassName = "HotaMapEdPluginCategoryButton";

        s_categoryButtonClass = RegisterClassExA(&windowClass);
        if (!s_categoryButtonClass && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        {
            LogFormat("Obstacle variants: failed to register category button class, gle=%lu.", GetLastError());
            return false;
        }

        if (!s_categoryButtonClass)
            s_categoryButtonClass = 1;
        return true;
    }

    bool EnsurePlaceOverlayClass()
    {
        if (s_placeOverlayClass)
            return true;

        WNDCLASSEXA windowClass = {};
        windowClass.cbSize = sizeof(windowClass);
        windowClass.lpfnWndProc = PlaceOverlayWndProc;
        windowClass.hInstance = s_pluginModule;
        windowClass.hCursor = LoadCursorA(nullptr, IDC_ARROW);
        windowClass.lpszClassName = "HotaMapEdPluginPlaceOverlay";

        s_placeOverlayClass = RegisterClassExA(&windowClass);
        if (!s_placeOverlayClass && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        {
            LogFormat("Obstacle variants: failed to register place overlay class, gle=%lu.", GetLastError());
            return false;
        }

        if (!s_placeOverlayClass)
            s_placeOverlayClass = 1;
        return true;
    }

    void InitializeScrollHostLayout(ScrollHostState& state)
    {
        int rowCounts[kObstacleCategoryCount] = {};
        for (int index = 0; index < kObstacleButtonCount; ++index)
        {
            const ObstacleButtonDefinition* definition =
                ObstacleButtonDefinitionAt(index);
            if (definition && definition->categoryIndex < kObstacleCategoryCount)
            {
                const int rows = definition->row + 1;
                if (rows > rowCounts[definition->categoryIndex])
                    rowCounts[definition->categoryIndex] = rows;
            }
        }

        int categoryY[kObstacleCategoryCount] = {};
        int y = kGlobalSelectionRowHeight;
        for (int category = 0; category < kObstacleCategoryCount; ++category)
        {
            state.headerY[category] = y;
            y += kCategoryHeaderHeight;
            state.selectionY[category] = y + 4;
            categoryY[category] = y + 4 + kSelectionRowHeight;
            if (!s_categoryExpanded[category]) continue;
            y += 4;
            y += kSelectionRowHeight;
            const int categoryHeight =
                rowCounts[category] * kCategoryButtonExtent +
                (rowCounts[category] > 0 ? rowCounts[category] - 1 : 0) * kCategoryRowGap;
            y += categoryHeight + 8;
        }
        state.contentHeight = y;

        for (int index = 0; index < kObstacleButtonCount; ++index)
        {
            const ObstacleButtonDefinition* definition =
                ObstacleButtonDefinitionAt(index);
            if (!definition || definition->categoryIndex >= kObstacleCategoryCount)
                continue;
            state.buttonX[index] = definition->column * kCategoryButtonExtent;
            state.buttonY[index] =
                categoryY[definition->categoryIndex] +
                definition->row * (kCategoryButtonExtent + kCategoryRowGap);
        }
    }

    bool CreateCategoryHeaders(HWND host, HFONT sourceFont)
    {
        auto* scrollState = ScrollHostStateFromWindow(host);
        if (!scrollState) return false;
        RECT client = {};
        GetClientRect(host, &client);
        LOGFONTA font = {};
        if (!sourceFont) sourceFont = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
        bool createdAll = GetObjectA(sourceFont, sizeof(font), &font) != 0;
        font.lfWeight = FW_NORMAL;
        scrollState->headerFont = CreateFontIndirectA(&font);
        font.lfWeight = FW_BOLD;
        scrollState->headerBoldFont = CreateFontIndirectA(&font);
        createdAll = createdAll && scrollState->headerFont && scrollState->headerBoldFont;
        for (int category = 0; createdAll && category < kObstacleCategoryCount; ++category)
        {
            HWND header = CreateWindowExA(0, "BUTTON", "",
                WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                0, scrollState->headerY[category], client.right,
                kCategoryHeaderHeight, host,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(kCategoryHeaderCommandBase + category)),
                s_pluginModule, nullptr);
            scrollState->headers[category] = header;
            scrollState->selectedCounts[category] = -1;
            createdAll = header && SetWindowSubclass(header, SelectionButtonSubclass, 1, 0);
        }
        if (createdAll) RefreshCategoryHeaders(host);
        return createdAll;
    }

    HWND CreateScrollHost(HWND parent, const RECT& rect)
    {
        ScrollHostState* state = new (std::nothrow) ScrollHostState();
        if (!state)
            return nullptr;
        InitializeScrollHostLayout(*state);

        HWND hwnd = CreateWindowExA(
            0,
            "HotaMapEdPluginObstacleScrollHost",
            nullptr,
            // Keep the entire Custom subtree hidden until construction and
            // layout finish and RefreshCategoryButtons applies the mode.
            WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_VSCROLL,
            rect.left,
            rect.top,
            rect.right - rect.left,
            rect.bottom - rect.top,
            parent,
            nullptr,
            s_pluginModule,
            state);
        if (!hwnd)
        {
            delete state;
            return nullptr;
        }
        LayoutScrollHost(hwnd, *state);
        return hwnd;
    }

    HWND CreateCategoryButton(HWND parent, HWND sourceToolbar, int index, int x, int y)
    {
        EnsureCategoryCheckedInitialized();
        CategoryButtonState* state = new (std::nothrow) CategoryButtonState();
        if (!state)
            return nullptr;
        state->sourceToolbar = sourceToolbar;
        state->index = index;
        if (index >= 0 && index < kObstacleButtonCount)
            state->checked = s_categoryChecked[index];

        HWND hwnd = CreateWindowExA(
            0,
            "HotaMapEdPluginCategoryButton",
            nullptr,
            WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
            x,
            y,
            kCategoryButtonExtent,
            kCategoryButtonExtent,
            parent,
            nullptr,
            s_pluginModule,
            state);
        if (!hwnd)
        {
            delete state;
            return nullptr;
        }

        SetWindowPos(
            hwnd,
            HWND_TOP,
            x,
            y,
            kCategoryButtonExtent,
            kCategoryButtonExtent,
            SWP_SHOWWINDOW | SWP_NOACTIVATE);
        if (index == 0)
            SetTimer(hwnd, kCategoryRefreshTimerId, kCategoryRefreshTimerMs, nullptr);
        return hwnd;
    }

    HWND CreatePlaceOverlayTooltip(HWND sourceToolbar, HWND overlay)
    {
        HWND tooltip = CreateWindowExA(
            WS_EX_TOPMOST,
            TOOLTIPS_CLASSA,
            nullptr,
            WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            sourceToolbar,
            nullptr,
            s_pluginModule,
            nullptr);
        if (!tooltip)
            return nullptr;

        TOOLINFOA tool = {};
        tool.cbSize = sizeof(tool);
        tool.uFlags = TTF_SUBCLASS | TTF_IDISHWND;
        tool.hwnd = sourceToolbar;
        tool.uId = reinterpret_cast<UINT_PTR>(overlay);
        tool.lpszText = kPlaceOverlayTooltip;
        if (!SendMessageA(tooltip, TTM_ADDTOOLA, 0, reinterpret_cast<LPARAM>(&tool)))
        {
            DestroyWindow(tooltip);
            return nullptr;
        }

        return tooltip;
    }

    HWND CreatePlaceOverlay(HWND parent, HWND sourceToolbar, const RECT& rect)
    {
        PlaceOverlayState* state = new (std::nothrow) PlaceOverlayState();
        if (!state)
            return nullptr;
        state->parent = parent;
        state->sourceToolbar = sourceToolbar;

        HWND hwnd = CreateWindowExA(
            0,
            "HotaMapEdPluginPlaceOverlay",
            nullptr,
            WS_CHILD | WS_CLIPSIBLINGS,
            rect.left,
            rect.top,
            rect.right - rect.left,
            rect.bottom - rect.top,
            sourceToolbar,
            nullptr,
            s_pluginModule,
            state);
        if (!hwnd)
        {
            delete state;
            return nullptr;
        }

        state->tooltip = CreatePlaceOverlayTooltip(sourceToolbar, hwnd);
        if (!state->tooltip)
            LogMessage("Obstacle variants: failed to create Place Obstacles overlay tooltip.");

        SetWindowPos(
            hwnd,
            HWND_TOP,
            rect.left,
            rect.top,
            rect.right - rect.left,
            rect.bottom - rect.top,
            SWP_NOACTIVATE);
        InvalidateRect(hwnd, nullptr, FALSE);
        UpdateWindow(hwnd);
        return hwnd;
    }

    VariantPanel* FindPanelLocked(HWND toolbar)
    {
        for (VariantPanel& panel : s_panels)
        {
            if (panel.sourceToolbar == toolbar)
                return &panel;
        }
        return nullptr;
    }

    void RefreshCategoryButtonsForToolbar(HWND toolbar)
    {
        if (!s_panelLockInitialized || !toolbar)
            return;

        EnterCriticalSection(&s_panelLock);
        VariantPanel* panel = FindPanelLocked(toolbar);
        if (panel)
            RefreshCategoryButtons(*panel);
        LeaveCriticalSection(&s_panelLock);
    }

    void RefreshAllCategoryButtons()
    {
        if (!s_panelLockInitialized)
            return;

        EnterCriticalSection(&s_panelLock);
        for (VariantPanel& panel : s_panels)
        {
            if (panel.sourceToolbar && IsWindow(panel.sourceToolbar))
                RefreshCategoryButtons(panel);
        }
        LeaveCriticalSection(&s_panelLock);
    }

    bool ShouldBlockNativePlaceObstaclesCommand()
    {
        if (!CustomObstacleModeEnabled()) return false;
        if (!s_panelLockInitialized)
            return false;

        VariantPanel panels[8] = {};
        int panelCount = 0;
        EnterCriticalSection(&s_panelLock);
        for (VariantPanel& panel : s_panels)
        {
            if (panel.parent && panel.sourceToolbar && panel.layoutApplied && panelCount < 8)
                panels[panelCount++] = panel;
        }
        LeaveCriticalSection(&s_panelLock);

        bool sawVisibleObstaclePanel = false;
        bool anyPanelCanPlace = false;
        for (int index = 0; index < panelCount; ++index)
        {
            VariantPanel& panel = panels[index];
            if (!IsWindow(panel.parent) ||
                !IsWindow(panel.sourceToolbar) ||
                !IsWindowVisible(panel.parent))
                continue;

            sawVisibleObstaclePanel = true;
            const CategoryEnablement enablement = GetCategoryEnablement(panel.sourceToolbar);
            if (AnyCheckedApplicableCategory(enablement))
            {
                anyPanelCanPlace = true;
                break;
            }
        }

        return sawVisibleObstaclePanel && !anyPanelCanPlace;
    }

    bool MessageCanAffectObstacleSelection(const MSG* message)
    {
        if (!message)
            return false;

        return message->message == WM_LBUTTONDOWN ||
            message->message == WM_LBUTTONUP ||
            (message->message == WM_MOUSEMOVE && (message->wParam & MK_LBUTTON) != 0) ||
            message->message == WM_KEYDOWN ||
            message->message == WM_KEYUP;
    }

    void PostCategoryRefreshMessages()
    {
        if (!s_panelLockInitialized)
            return;

        EnterCriticalSection(&s_panelLock);
        for (VariantPanel& panel : s_panels)
        {
            if (panel.parent && IsWindow(panel.parent))
                PostMessageA(panel.parent, CategoryRefreshMessage(), 0, 0);
        }
        LeaveCriticalSection(&s_panelLock);
    }

    VariantPanel* FindFreePanelLocked()
    {
        for (VariantPanel& panel : s_panels)
        {
            if (!panel.parent)
                return &panel;
        }
        return nullptr;
    }

    void RunObstacleVariantCommand(HWND parent, HWND sourceToolbar, ObstacleGenerationFilterMode mode)
    {
        if (!CustomObstacleModeEnabled())
        {
            SetObstacleGenerationFilterMode(ObstacleGenerationFilterMode::None);
            SendMessageA(parent, WM_COMMAND, MAKEWPARAM(kVanillaPlaceObstaclesCommand, 0),
                reinterpret_cast<LPARAM>(sourceToolbar));
            return;
        }
        if ((mode == ObstacleGenerationFilterMode::ThickOnly ||
                mode == ObstacleGenerationFilterMode::ThinOnly) &&
            !ObstacleThicknessTableAvailable())
        {
            MessageBoxA(
                parent,
                "Obstacle thickness data has not been generated yet.\n\n"
                "Classify obstacles.xlsx, then regenerate the plugin's "
                "DEF/template thickness table.",
                "HotaMapEdPlugin",
                MB_ICONINFORMATION);
            return;
        }

        if (mode == ObstacleGenerationFilterMode::ButtonSelection &&
            !ObstacleButtonDataAvailable())
        {
            LogMessage("Obstacle variants: button metadata is unavailable; placement was not started.");
            return;
        }

        const char* modeName =
            mode == ObstacleGenerationFilterMode::ThickOnly ? "thick" :
            mode == ObstacleGenerationFilterMode::ThinOnly ? "thin" :
            "selected-button";
        LogFormat("Obstacle variants: running %s obstacle generation.", modeName);

        DWORD lowMask = 0;
        DWORD highMask = 0;
        DWORD extendedMask = 0;
        DWORD upperMask = 0;
        GetSelectedButtonMasks(lowMask, highMask, extendedMask, upperMask);
        SetObstacleButtonSelection(lowMask, highMask, extendedMask, upperMask);
        SetObstacleGenerationFilterMode(mode);
        SendMessageA(
            parent,
            WM_COMMAND,
            MAKEWPARAM(kVanillaPlaceObstaclesCommand, 0),
            reinterpret_cast<LPARAM>(sourceToolbar));
        SetObstacleGenerationFilterMode(ObstacleGenerationFilterMode::None);

        LogFormat("Obstacle variants: finished %s obstacle generation.", modeName);
    }

    bool HitTestVariantToolbarButton(
        HWND toolbar,
        LPARAM lParam,
        ObstacleGenerationFilterMode& mode)
    {
        POINT point =
        {
            static_cast<SHORT>(LOWORD(lParam)),
            static_cast<SHORT>(HIWORD(lParam)),
        };
        const LRESULT hit = SendMessageA(toolbar, TB_HITTEST, 0, reinterpret_cast<LPARAM>(&point));
        if (hit < 0)
            return false;

        TBBUTTON button = {};
        if (!SendMessageA(toolbar, TB_GETBUTTON, static_cast<WPARAM>(hit), reinterpret_cast<LPARAM>(&button)))
            return false;

        if (button.idCommand == static_cast<int>(kPlaceThickObstaclesCommand))
        {
            mode = ObstacleGenerationFilterMode::ThickOnly;
            return true;
        }
        if (button.idCommand == static_cast<int>(kPlaceThinObstaclesCommand))
        {
            mode = ObstacleGenerationFilterMode::ThinOnly;
            return true;
        }

        return false;
    }

    LRESULT CALLBACK VariantGetMessageHook(int code, WPARAM wParam, LPARAM lParam)
    {
        if (code >= 0 && wParam == PM_REMOVE)
        {
            MSG* message = reinterpret_cast<MSG*>(lParam);
            if (message && message->message == CategoryLayoutMessage())
            {
                HWND toolbar = message->hwnd;
                message->message = WM_NULL;
                message->wParam = 0;
                message->lParam = 0;

                VariantPanel* panel = nullptr;
                EnterCriticalSection(&s_panelLock);
                panel = FindPanelLocked(toolbar);
                LeaveCriticalSection(&s_panelLock);

                if (panel && IsWindow(panel->sourceToolbar))
                {
                    AddVariantToolbarButtons(*panel);
                    RefreshCategoryButtons(*panel);
                }
            }
            else if (message && message->message == CategoryRefreshMessage())
            {
                message->message = WM_NULL;
                message->wParam = 0;
                message->lParam = 0;
                RefreshAllCategoryButtons();
            }
            else if (message &&
                message->message == WM_COMMAND &&
                LOWORD(message->wParam) == kVanillaPlaceObstaclesCommand &&
                CustomObstacleModeEnabled())
            {
                VariantPanel context;
                bool haveContext = false;
                EnterCriticalSection(&s_panelLock);
                for (VariantPanel& panel : s_panels)
                {
                    if (panel.layoutApplied &&
                        IsWindow(panel.sourceToolbar) &&
                        (panel.parent == message->hwnd ||
                            panel.sourceToolbar == reinterpret_cast<HWND>(message->lParam)))
                    {
                        context = panel;
                        haveContext = true;
                        break;
                    }
                }
                if (!haveContext)
                {
                    for (VariantPanel& panel : s_panels)
                    {
                        if (panel.layoutApplied &&
                            panel.parent &&
                            IsWindow(panel.parent) &&
                            IsWindow(panel.sourceToolbar) &&
                            IsWindowVisible(panel.parent))
                        {
                            context = panel;
                            haveContext = true;
                            break;
                        }
                    }
                }
                LeaveCriticalSection(&s_panelLock);

                if (haveContext)
                {
                    message->message = WM_NULL;
                    message->wParam = 0;
                    message->lParam = 0;
                    if (ShouldBlockNativePlaceObstaclesCommand())
                    {
                        LogMessage("Obstacle variants: blocked native Place Obstacles command because the effective button pool is empty.");
                    }
                    else
                    {
                        RunObstacleVariantCommand(
                            context.parent,
                            context.sourceToolbar,
                            ObstacleGenerationFilterMode::ButtonSelection);
                    }
                    PostCategoryRefreshMessages();
                }
            }
            else if (message &&
                (message->message == WM_LBUTTONDOWN || message->message == WM_LBUTTONUP))
            {
                const UINT originalMessage = message->message;
                VariantPanel context;
                bool haveContext = false;

                EnterCriticalSection(&s_panelLock);
                VariantPanel* panel = FindPanelLocked(message->hwnd);
                if (panel)
                {
                    context = *panel;
                    haveContext = true;
                }
                LeaveCriticalSection(&s_panelLock);

                if (haveContext && IsWindow(context.sourceToolbar))
                {
                    ObstacleGenerationFilterMode mode = ObstacleGenerationFilterMode::None;
                    if (CustomObstacleModeEnabled() &&
                        HitTestVariantToolbarButton(context.sourceToolbar, message->lParam, mode))
                    {
                        message->message = WM_NULL;
                        message->wParam = 0;
                        message->lParam = 0;
                        if (originalMessage == WM_LBUTTONUP)
                            RunObstacleVariantCommand(context.parent, context.sourceToolbar, mode);
                    }
                }

                PostCategoryRefreshMessages();
            }
            else if (MessageCanAffectObstacleSelection(message))
            {
                PostCategoryRefreshMessages();
            }
        }

        return CallNextHookEx(s_getMessageHook, code, wParam, lParam);
    }

    bool AddVariantToolbarButtons(VariantPanel& panel)
    {
        HWND toolbar = panel.sourceToolbar;
        HWND parent = panel.parent;
        if (!toolbar || !parent)
            return false;

        if (panel.layoutApplied)
            return true;

        RECT placeRect = {};
        if (!RectInParent(parent, toolbar, placeRect))
            return false;

        RECT placeButtonRect = {};
        if (!ToolbarCommandRect(toolbar, kVanillaPlaceObstaclesCommand, placeButtonRect))
        {
            LogMessage("Obstacle variants: could not locate native Place Obstacles button for overlay.");
            return false;
        }

        HWND brushLabel = nullptr;
        RECT brushLabelRect = {};
        RECT brushToolbarRect = {};
        if (!FindBrushSection(parent, placeRect.top, brushLabel, brushLabelRect, brushToolbarRect))
        {
            LogMessage("Obstacle variants: could not find visible Brush section for category buttons.");
            return false;
        }

        constexpr int sectionGap = 10;
        constexpr int labelToButtonsGap = 4;

        RECT parentClient = {};
        GetClientRect(parent, &parentClient);
        const int x = brushToolbarRect.left;
        const int modeLabelY = brushToolbarRect.bottom + sectionGap;
        const int labelHeight = brushLabelRect.bottom - brushLabelRect.top;
        const int labelWidth = parentClient.right - brushLabelRect.left - 6;
        const int labelY = modeLabelY + labelHeight + 22 + sectionGap;
        const int firstRowY = labelY + labelHeight + labelToButtonsGap;
        RECT scrollRect =
        {
            x,
            firstRowY,
            parentClient.right - 6,
            parentClient.bottom - 6,
        };
        if (scrollRect.right <= scrollRect.left || scrollRect.bottom <= scrollRect.top ||
            !EnsureCategoryButtonClass() ||
            !EnsurePlaceOverlayClass() ||
            !EnsureScrollHostClass())
            return false;

        panel.placeOverlay = CreatePlaceOverlay(parent, toolbar, placeButtonRect);
        if (!panel.placeOverlay)
        {
            LogMessage("Obstacle variants: failed to create Place Obstacles overlay.");
            return false;
        }

        panel.categoryLabel = CreateWindowExA(
            0,
            "STATIC",
            "Obstacle Types",
            WS_CHILD | WS_CLIPSIBLINGS | SS_LEFT | SS_NOPREFIX,
            brushLabelRect.left,
            labelY,
            labelWidth,
            labelHeight,
            parent,
            nullptr,
            s_pluginModule,
            nullptr);
        if (panel.categoryLabel)
        {
            HFONT brushFont = reinterpret_cast<HFONT>(
                SendMessageA(brushLabel, WM_GETFONT, 0, 0));
            if (brushFont)
                SendMessageA(panel.categoryLabel, WM_SETFONT, reinterpret_cast<WPARAM>(brushFont), TRUE);
        }

        panel.scrollHost = CreateScrollHost(parent, scrollRect);
        HFONT brushFont = reinterpret_cast<HFONT>(SendMessageA(brushLabel, WM_GETFONT, 0, 0));
        panel.modeLabel = CreateWindowExA(0, "STATIC", "Mode",
            WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX,
            brushLabelRect.left, modeLabelY, labelWidth, labelHeight, parent, nullptr, s_pluginModule, nullptr);
        if (panel.modeLabel && brushFont)
            SendMessageA(panel.modeLabel, WM_SETFONT, reinterpret_cast<WPARAM>(brushFont), TRUE);
        panel.modeHost = CreateModeHost(parent, x, modeLabelY + labelHeight,
            scrollRect.right - scrollRect.left, brushFont);
        bool createdAll = panel.categoryLabel && panel.scrollHost && panel.modeLabel && panel.modeHost;
        ScrollHostState* scrollState = panel.scrollHost
            ? ScrollHostStateFromWindow(panel.scrollHost)
            : nullptr;
        if (createdAll && scrollState)
            createdAll = CreateCategoryHeaders(panel.scrollHost,
                reinterpret_cast<HFONT>(SendMessageA(brushLabel, WM_GETFONT, 0, 0)));
        for (int i = 0; i < kObstacleButtonCount; ++i)
        {
            if (!createdAll || !scrollState)
                break;

            panel.categoryButtons[i] = CreateCategoryButton(
                panel.scrollHost,
                toolbar,
                i,
                scrollState->buttonX[i],
                scrollState->buttonY[i]);
            if (!panel.categoryButtons[i])
            {
                createdAll = false;
                break;
            }
            scrollState->buttons[i] = panel.categoryButtons[i];
        }

        if (createdAll && scrollState)
        {
            scrollState->sourceToolbar = toolbar;
            scrollState->selectionTooltip = CreateWindowExA(WS_EX_TOPMOST,
                TOOLTIPS_CLASSA, nullptr, WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX,
                CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                panel.scrollHost, nullptr, s_pluginModule, nullptr);
            createdAll = scrollState->selectionTooltip != nullptr;
            static char allText[] = "Select All";
            static char noneText[] = "Select None";
            for (int action = 0; createdAll && action < kSelectionActionCount; ++action)
            {
                const bool global = action >= kGlobalSelectionAction;
                const int extent = global ? kGlobalSelectionButtonExtent : kSelectionButtonExtent;
                HWND button = CreateWindowExA(0, "BUTTON", "",
                    WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                    (action % 2) * (extent + (global ? 12 : 6)),
                    global ? 0 : scrollState->selectionY[action / 2],
                    extent, extent, panel.scrollHost,
                    reinterpret_cast<HMENU>(static_cast<INT_PTR>(kSelectionCommandBase + action)),
                    s_pluginModule, nullptr);
                scrollState->selectionButtons[action] = button;
                createdAll = button != nullptr;
                if (!createdAll) break;
                createdAll = SetWindowSubclass(button, SelectionButtonSubclass, 1, 0) != FALSE;
                if (!createdAll) break;
                SendMessageA(button, WM_SETFONT,
                    reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
                TOOLINFOA tool = {};
                tool.cbSize = sizeof(tool);
                tool.uFlags = TTF_SUBCLASS | TTF_IDISHWND;
                tool.hwnd = panel.scrollHost;
                tool.uId = reinterpret_cast<UINT_PTR>(button);
                tool.lpszText = action == kExpansionAction ? const_cast<char*>(ExpansionActionText()) :
                    action % 2 ? noneText : allText;
                createdAll = SendMessageA(scrollState->selectionTooltip, TTM_ADDTOOLA,
                    0, reinterpret_cast<LPARAM>(&tool)) != FALSE;
            }
        }

        if (!createdAll)
        {
            LogMessage("Obstacle variants: failed to create obstacle type buttons.");
            if (panel.modeHost) { DestroyWindow(panel.modeHost); panel.modeHost = nullptr; }
            if (panel.modeLabel) { DestroyWindow(panel.modeLabel); panel.modeLabel = nullptr; }
            if (panel.placeOverlay)
            {
                DestroyWindow(panel.placeOverlay);
                panel.placeOverlay = nullptr;
            }
            if (panel.categoryLabel)
            {
                DestroyWindow(panel.categoryLabel);
                panel.categoryLabel = nullptr;
            }
            if (panel.scrollHost)
            {
                DestroyWindow(panel.scrollHost);
                panel.scrollHost = nullptr;
            }
            for (HWND& button : panel.categoryButtons)
                button = nullptr;
            return false;
        }

        LayoutScrollHost(panel.scrollHost, *scrollState);
        panel.layoutApplied = true;
        RefreshCategoryButtons(panel);
        RedrawWindow(
            panel.scrollHost,
            nullptr,
            nullptr,
            RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
        LogFormat(
            "Obstacle variants: created %d obstacle type buttons below Brush parent=%p placeOverlay=%p label=%p scrollHost=%p first=%p x=%d y=%d contentHeight=%d",
            kObstacleButtonCount,
            parent,
            panel.placeOverlay,
            panel.categoryLabel,
            panel.scrollHost,
            panel.categoryButtons[0],
            x,
            firstRowY,
            scrollState->contentHeight);
        return true;
    }

    bool EnsureGetMessageHook(DWORD threadId)
    {
        if (s_getMessageHook && s_getMessageHookThreadId == threadId)
            return true;

        if (s_getMessageHook)
        {
            LogMessage("Obstacle variants: unexpected second UI thread for toolbar hook.");
            return false;
        }

        s_getMessageHook = SetWindowsHookExA(
            WH_GETMESSAGE,
            VariantGetMessageHook,
            s_pluginModule,
            threadId);
        if (!s_getMessageHook)
        {
            LogFormat(
                "Obstacle variants: failed to install toolbar message hook, gle=%lu.",
                GetLastError());
            return false;
        }

        s_getMessageHookThreadId = threadId;
        return true;
    }

    bool RequestCategoryLayout(HWND toolbar)
    {
        const UINT message = CategoryLayoutMessage();
        if (!message)
        {
            LogFormat(
                "Obstacle variants: failed to register category layout message, gle=%lu.",
                GetLastError());
            return false;
        }

        if (!PostMessageA(toolbar, message, 0, 0))
        {
            LogFormat(
                "Obstacle variants: failed to post category layout message, gle=%lu.",
                GetLastError());
            return false;
        }

        return true;
    }

    bool InstallObstacleUpdateCaptureHook(std::string& error)
    {
        HookSpec spec =
        {
            reinterpret_cast<void*>(kObstacleUpdateCaptureAddress),
            kObstacleUpdateCaptureExpected,
            sizeof(kObstacleUpdateCaptureExpected),
        };

        const HookResult result =
            HookInstall(spec, ObstacleUpdateCaptureHook, s_updateCaptureOriginal);
        if (result != HookResult::Success)
        {
            error = "could not install obstacle update capture hook: ";
            error += HookResultName(result);
            return false;
        }

        LogFormat(
            "Obstacle variants: installed obstacle update capture hook at 0x%08lX.",
            kObstacleUpdateCaptureAddress);
        return true;
    }

    bool InstallPanelForToolbar(HWND toolbar)
    {
        HWND parent = GetParent(toolbar);
        if (!parent)
            return false;

        DWORD processId = 0;
        const DWORD threadId = GetWindowThreadProcessId(toolbar, &processId);
        if (!threadId || processId != GetCurrentProcessId())
            return false;

        EnterCriticalSection(&s_panelLock);
        VariantPanel* existingPanel = FindPanelLocked(toolbar);
        if (existingPanel)
        {
            const bool alreadyApplied = existingPanel->layoutApplied;
            LeaveCriticalSection(&s_panelLock);
            if (!alreadyApplied)
            {
                if (!EnsureGetMessageHook(threadId))
                    return false;
                RequestCategoryLayout(toolbar);
            }
            return true;
        }
        VariantPanel* panel = FindFreePanelLocked();
        if (!panel)
        {
            LeaveCriticalSection(&s_panelLock);
            LogMessage("Obstacle variants: panel capacity exhausted.");
            return false;
        }
        panel->parent = parent;
        panel->sourceToolbar = toolbar;
        panel->threadId = threadId;
        LeaveCriticalSection(&s_panelLock);

        if (!EnsureGetMessageHook(threadId))
        {
            EnterCriticalSection(&s_panelLock);
            VariantPanel* failedPanel = FindPanelLocked(toolbar);
            if (failedPanel)
                *failedPanel = VariantPanel();
            LeaveCriticalSection(&s_panelLock);
            return false;
        }

        if (!RequestCategoryLayout(toolbar))
        {
            EnterCriticalSection(&s_panelLock);
            VariantPanel* failedPanel = FindPanelLocked(toolbar);
            if (failedPanel)
                *failedPanel = VariantPanel();
            LeaveCriticalSection(&s_panelLock);
            return false;
        }

        LogFormat(
            "Obstacle variants: registered toolbar buttons parent=%p toolbar=%p thicknessTable=%s",
            parent,
            toolbar,
            ObstacleThicknessTableAvailable() ? "available" : "missing");
        return true;
    }

    BOOL CALLBACK EnumChildProc(HWND hwnd, LPARAM)
    {
        if (IsWindowVisible(hwnd) && ToolbarHasCommand(hwnd, kVanillaPlaceObstaclesCommand))
            InstallPanelForToolbar(hwnd);
        return TRUE;
    }

    BOOL CALLBACK EnumTopLevelProc(HWND hwnd, LPARAM)
    {
        DWORD processId = 0;
        GetWindowThreadProcessId(hwnd, &processId);
        if (processId == GetCurrentProcessId())
            EnumChildWindows(hwnd, EnumChildProc, 0);
        return TRUE;
    }

    DWORD WINAPI ObstacleVariantUiThread(LPVOID)
    {
        LogMessage("Obstacle variants: UI discovery thread started.");
        for (;;)
        {
            EnumWindows(EnumTopLevelProc, 0);
            Sleep(kPollIntervalMs);
        }
    }
}

bool ObstacleVariantsEnabled()
{
    return ObstacleButtonDataAvailable() ||
        ObstacleThicknessTableAvailable() ||
        VariantUiEnvEnabled();
}

bool StartObstacleVariantUi(std::string& error)
{
    if (!s_pluginModule &&
        !GetModuleHandleExA(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCSTR>(&StartObstacleVariantUi),
            &s_pluginModule))
    {
        error = "could not resolve plugin module handle for obstacle variant UI hook";
        return false;
    }

    if (!s_panelLockInitialized)
    {
        InitializeCriticalSection(&s_panelLock);
        s_panelLockInitialized = true;
    }

    if (!s_updateCaptureOriginal[0] && !InstallObstacleUpdateCaptureHook(error))
        return false;

    HANDLE thread = CreateThread(
        nullptr,
        0,
        ObstacleVariantUiThread,
        nullptr,
        0,
        nullptr);
    if (!thread)
    {
        error = "could not create obstacle variant UI thread";
        return false;
    }

    CloseHandle(thread);
    return true;
}
