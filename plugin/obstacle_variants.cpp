#include "pch.h"
#include "obstacle_variants.h"

#include "logger.h"
#include "hook.h"
#include "obstacle_classification.h"
#include "obstacle_filter.h"
#include "obstacle_icon_data.h"
#include "obstacle_preview.h"

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
    constexpr int kCategoryPlaceholderCount = 8;
    constexpr bool kUseLargeCategoryButtonPreview = false;
    constexpr int kCategoryButtonsPerRow = kUseLargeCategoryButtonPreview ? 3 : 4;
    constexpr int kCategoryButtonExtent = kUseLargeCategoryButtonPreview ? 40 : 32;
    constexpr int kCategoryGlyphExtent = kUseLargeCategoryButtonPreview ? 32 : 24;
    constexpr int kGridSelectionCollectionCount = 16;
    constexpr int kObstaclePaintCollectionIndex = 0;
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
        HWND categoryButtons[kCategoryPlaceholderCount] = {};
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

    VariantPanel s_panels[8] = {};
    CRITICAL_SECTION s_panelLock = {};
    bool s_panelLockInitialized = false;
    HMODULE s_pluginModule = nullptr;
    HHOOK s_getMessageHook = nullptr;
    DWORD s_getMessageHookThreadId = 0;
    ATOM s_categoryButtonClass = 0;
    ATOM s_placeOverlayClass = 0;
    BYTE s_updateCaptureOriginal[5] = {};
    volatile DWORD s_lastObstacleUiContext = 0;
    DWORD s_lastLoggedAppliedTerrainMask = 0xFFFFFFFF;
    DWORD s_lastLoggedAppliedNativeEnabled = 0xFFFFFFFF;
    DWORD s_lastLoggedMapWidth = 0;
    DWORD s_lastLoggedMapHeight = 0;
    char s_lastCategoryTraceSummary[4096] = {};
    bool s_categoryChecked[kCategoryPlaceholderCount] =
    {
        true, true, true, true,
        true, true, true, true,
    };

    using SelectCollectionFn = DWORD(__thiscall*)(void*, int);
    using TileAtFn = DWORD* (__thiscall*)(void*, int, int);
    using CellStateAtFn = int(__thiscall*)(void*, unsigned int, unsigned int, unsigned char);

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
        GridSelectionScanStats& stats)
    {
        bool found = false;

        __try
        {
            for (unsigned int y = 0; y < mapHeight; ++y)
            {
                for (unsigned int x = 0; x < mapWidth; ++x)
                {
                    const int cellState =
                        CellStateAt()(selection, x, y, static_cast<unsigned char>(collectionIndex));
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

                    DWORD* tile = TileAt()(terrainCollection, static_cast<int>(x), static_cast<int>(y));
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
        for (int index = 0; index < kGridSelectionCollectionCount; ++index)
            totalCells += stats[index].paintedCells;
        AppendTraceText(summary, sizeof(summary), offset, " totalCells=%lu", totalCells);

        for (int index = 0; index < kGridSelectionCollectionCount; ++index)
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

    bool IsKnownMapDimension(DWORD value)
    {
        return value == 36 ||
            value == 72 ||
            value == 108 ||
            value == 144 ||
            value == 180 ||
            value == 216 ||
            value == 252;
    }

    bool TryReadMapDimensions(DWORD context, unsigned int& width, unsigned int& height)
    {
        width = 0;
        height = 0;

        DWORD mapSizeHolder = 0;
        DWORD rawWidth = 0;
        DWORD rawHeight = 0;
        if (!SafeReadDword(context + 0x54, mapSizeHolder) ||
            !mapSizeHolder ||
            !SafeReadDword(mapSizeHolder + 0x54, rawWidth) ||
            !SafeReadDword(mapSizeHolder + 0x58, rawHeight) ||
            !IsKnownMapDimension(rawWidth) ||
            !IsKnownMapDimension(rawHeight))
            return false;

        width = rawWidth;
        height = rawHeight;
        if (rawWidth != s_lastLoggedMapWidth || rawHeight != s_lastLoggedMapHeight)
        {
            s_lastLoggedMapWidth = rawWidth;
            s_lastLoggedMapHeight = rawHeight;
            LogFormat(
                "Obstacle variants category: map scan dimensions=%lux%lu holder=%08lX.",
                rawWidth,
                rawHeight,
                mapSizeHolder);
        }
        return true;
    }

    PaintedTerrainScan ComputePaintedTerrainScan()
    {
        PaintedTerrainScan result = {};
        DWORD context = s_lastObstacleUiContext;
        if (!context)
            return result;

        DWORD mode = 0;
        DWORD selection = 0;
        DWORD mapContext = 0;
        DWORD selectionManager = 0;
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
            !TryReadMapDimensions(context, mapWidth, mapHeight))
            return result;

        void* terrainCollection = reinterpret_cast<void*>(
            SelectCollection()(reinterpret_cast<void*>(selectionManager), 0));
        if (!terrainCollection)
            return result;

        GridSelectionScanStats stats[kGridSelectionCollectionCount] = {};
        const bool trace = CategoryTraceEnabled();
        const int firstCollection = trace ? 0 : kObstaclePaintCollectionIndex;
        const int endCollection = trace ? kGridSelectionCollectionCount : kObstaclePaintCollectionIndex + 1;
        for (int index = firstCollection; index < endCollection; ++index)
        {
            AddGridSelectionTerrainMask(
                reinterpret_cast<void*>(selection),
                terrainCollection,
                index,
                mapWidth,
                mapHeight,
                stats[index]);
        }

        const GridSelectionScanStats& obstacleStats = stats[kObstaclePaintCollectionIndex];
        result.terrainMask = obstacleStats.stateMasks[kObstaclePaintCellState];
        result.paintedCells = obstacleStats.stateCounts[kObstaclePaintCellState];
        TraceGridSelectionStats(result.terrainMask, stats);
        return result;
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
        return !enablement.hasPaintedCells ||
            (index >= 0 &&
                index < 32 &&
                (enablement.terrainMask & (1u << index)) != 0);
    }

    bool CategoryCanContributeToGeneration(int index, const CategoryEnablement& enablement)
    {
        return enablement.hasPaintedCells &&
            index >= 0 &&
            index < 32 &&
            (enablement.terrainMask & (1u << index)) != 0 &&
            index < kCategoryPlaceholderCount &&
            s_categoryChecked[index];
    }

    bool AnyCheckedApplicableCategory(const CategoryEnablement& enablement)
    {
        for (int index = 0; index < kCategoryPlaceholderCount; ++index)
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

        const CategoryEnablement enablement = GetCategoryEnablement(panel.sourceToolbar);
        for (HWND button : panel.categoryButtons)
            RefreshCategoryButton(button, enablement);
        RefreshPlaceOverlay(panel, enablement);
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
        const ObstacleIconData* icon,
        COLORREF background)
    {
        constexpr int kIconExtent = 24;
        if (!icon || !icon->rgba || icon->width != kIconExtent || icon->height != kIconExtent)
            return false;

        BYTE pixels[kIconExtent * kIconExtent * 4] = {};
        const unsigned int backgroundRed = GetRValue(background);
        const unsigned int backgroundGreen = GetGValue(background);
        const unsigned int backgroundBlue = GetBValue(background);

        for (int pixel = 0; pixel < kIconExtent * kIconExtent; ++pixel)
        {
            const unsigned int sourceOffset = pixel * 4;
            const unsigned int alpha = icon->rgba[sourceOffset + 3];
            const unsigned int inverseAlpha = 255 - alpha;
            pixels[sourceOffset + 0] = static_cast<BYTE>(
                (icon->rgba[sourceOffset + 2] * alpha + backgroundBlue * inverseAlpha + 127) / 255);
            pixels[sourceOffset + 1] = static_cast<BYTE>(
                (icon->rgba[sourceOffset + 1] * alpha + backgroundGreen * inverseAlpha + 127) / 255);
            pixels[sourceOffset + 2] = static_cast<BYTE>(
                (icon->rgba[sourceOffset + 0] * alpha + backgroundRed * inverseAlpha + 127) / 255);
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
                pressed ? RGB(186, 226, 255) :
                hot ? RGB(214, 238, 255) :
                RGB(229, 245, 255);
            buttonBackground = fill;
            HBRUSH fillBrush = CreateSolidBrush(fill);
            RECT fillRect = rect;
            InflateRect(&fillRect, -1, -1);
            FillRect(hdc, &fillRect, fillBrush);
            DeleteObject(fillBrush);

            HPEN framePen = CreatePen(PS_SOLID, 1, RGB(190, 232, 255));
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

        const ObstacleIconData* icon = ObstacleIconForButton(
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
                if (state && state->mouseInside)
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
                    if (state->index >= 0 && state->index < kCategoryPlaceholderCount)
                        s_categoryChecked[state->index] = state->checked;
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
                    SendMessageA(
                        state->parent,
                        WM_COMMAND,
                        MAKEWPARAM(kVanillaPlaceObstaclesCommand, 0),
                        reinterpret_cast<LPARAM>(state->sourceToolbar));
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

    HWND CreateCategoryButton(HWND parent, HWND sourceToolbar, int index, int x, int y)
    {
        CategoryButtonState* state = new (std::nothrow) CategoryButtonState();
        if (!state)
            return nullptr;
        state->sourceToolbar = sourceToolbar;
        state->index = index;
        if (index >= 0 && index < kCategoryPlaceholderCount)
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
        InvalidateRect(hwnd, nullptr, FALSE);
        UpdateWindow(hwnd);
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
            WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
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
            SWP_SHOWWINDOW | SWP_NOACTIVATE);
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
        if (!ObstacleThicknessTableAvailable())
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

        const char* modeName =
            mode == ObstacleGenerationFilterMode::ThickOnly ? "thick" : "thin";
        LogFormat("Obstacle variants: running %s obstacle generation.", modeName);

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
                ShouldBlockNativePlaceObstaclesCommand())
            {
                message->message = WM_NULL;
                message->wParam = 0;
                message->lParam = 0;
                LogMessage("Obstacle variants: blocked native Place Obstacles command because the effective category pool is empty.");
                PostCategoryRefreshMessages();
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
                    if (HitTestVariantToolbarButton(context.sourceToolbar, message->lParam, mode))
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

        constexpr int rowGap = 4;
        constexpr int sectionGap = 10;
        constexpr int labelToButtonsGap = 4;

        const int x = brushToolbarRect.left;
        const int labelY = brushToolbarRect.bottom + sectionGap;
        const int labelHeight = brushLabelRect.bottom - brushLabelRect.top;
        const int labelWidth = brushToolbarRect.right - brushLabelRect.left;
        const int firstRowY = labelY + labelHeight + labelToButtonsGap;
        if (!EnsureCategoryButtonClass() || !EnsurePlaceOverlayClass())
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
            WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | SS_LEFT | SS_NOPREFIX,
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

        bool createdAll = panel.categoryLabel != nullptr;
        for (int i = 0; i < kCategoryPlaceholderCount; ++i)
        {
            if (!createdAll)
                break;

            const int row = i / kCategoryButtonsPerRow;
            const int col = i % kCategoryButtonsPerRow;
            const int buttonX = x + col * kCategoryButtonExtent;
            const int buttonY = firstRowY + row * (kCategoryButtonExtent + rowGap);
            panel.categoryButtons[i] = CreateCategoryButton(parent, toolbar, i, buttonX, buttonY);
            if (!panel.categoryButtons[i])
            {
                createdAll = false;
                break;
            }
        }

        if (!createdAll)
        {
            LogMessage("Obstacle variants: failed to create category placeholder buttons.");
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
            for (HWND& button : panel.categoryButtons)
            {
                if (button)
                {
                    DestroyWindow(button);
                    button = nullptr;
                }
            }
            return false;
        }

        panel.layoutApplied = true;
        RefreshCategoryButtons(panel);
        InvalidateRect(parent, nullptr, TRUE);
        for (HWND button : panel.categoryButtons)
        {
            InvalidateRect(button, nullptr, FALSE);
            UpdateWindow(button);
        }
        LogFormat(
            "Obstacle variants: created category placeholder buttons below Brush parent=%p placeOverlay=%p label=%p first=%p x=%d y=%d",
            parent,
            panel.placeOverlay,
            panel.categoryLabel,
            panel.categoryButtons[0],
            x,
            firstRowY);
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
    return ObstacleThicknessTableAvailable() || VariantUiEnvEnabled();
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
