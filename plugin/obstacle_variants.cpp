#include "pch.h"
#include "obstacle_variants.h"

#include "logger.h"
#include "hook.h"
#include "obstacle_classification.h"
#include "obstacle_filter.h"

#include <commctrl.h>
#include <cstring>
#include <new>

namespace
{
    constexpr char kVariantUiEnvVar[] = "HOTA_MAPED_PLUGIN_OBSTACLE_VARIANTS";
    constexpr UINT kVanillaPlaceObstaclesCommand = 32927;
    constexpr UINT kPlaceThickObstaclesCommand = 0x7001;
    constexpr UINT kPlaceThinObstaclesCommand = 0x7002;
    constexpr int kCategoryPlaceholderCount = 8;
    constexpr int kCategoryButtonsPerRow = 4;
    constexpr int kCategoryButtonSize = 24;
    constexpr int kGridSelectionCollectionCount = 16;
    constexpr unsigned int kMaxMapDimensionTiles = 252;
    constexpr DWORD kPollIntervalMs = 50;
    constexpr DWORD kObstacleUpdateCaptureAddress = 0x00484C9A;
    constexpr BYTE kObstacleUpdateCaptureExpected[] = { 0x8B, 0x46, 0x64, 0x85, 0xC0 };
    constexpr DWORD kObstacleUpdateCaptureResume = 0x00484C9F;
    constexpr DWORD kCategoryRefreshTimerMs = 75;

    struct VariantPanel
    {
        HWND parent = nullptr;
        HWND sourceToolbar = nullptr;
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
        int index = 0;
    };

    VariantPanel s_panels[8] = {};
    CRITICAL_SECTION s_panelLock = {};
    bool s_panelLockInitialized = false;
    HMODULE s_pluginModule = nullptr;
    HHOOK s_getMessageHook = nullptr;
    DWORD s_getMessageHookThreadId = 0;
    ATOM s_categoryButtonClass = 0;
    BYTE s_updateCaptureOriginal[5] = {};
    volatile DWORD s_lastObstacleUiContext = 0;
    DWORD s_lastLoggedAppliedTerrainMask = 0xFFFFFFFF;
    DWORD s_lastLoggedAppliedNativeEnabled = 0xFFFFFFFF;
    DWORD s_lastStablePaintedTerrainMask = 0;
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
        GridSelectionScanStats& stats)
    {
        bool found = false;

        __try
        {
            for (unsigned int y = 0; y < kMaxMapDimensionTiles; ++y)
            {
                for (unsigned int x = 0; x < kMaxMapDimensionTiles; ++x)
                {
                    if (CellStateAt()(selection, x, y, static_cast<unsigned char>(collectionIndex)) == 0)
                        continue;

                    ++stats.paintedCells;
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
                        stats.mask |= 1u << ((*tile) & 0x0F);
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

    DWORD ComputePaintedTerrainMask()
    {
        DWORD context = s_lastObstacleUiContext;
        if (!context)
            return 0;

        DWORD mode = 0;
        DWORD selection = 0;
        DWORD mapContext = 0;
        DWORD selectionManager = 0;
        if (!SafeReadDword(context + 0x98, mode) ||
            mode != 6 ||
            !SafeReadDword(context + 0x64, selection) ||
            !selection ||
            !SafeReadDword(context + 0x3C, mapContext) ||
            !mapContext ||
            !SafeReadDword(mapContext + 0x54, selectionManager) ||
            !selectionManager)
            return 0;

        void* terrainCollection = reinterpret_cast<void*>(
            SelectCollection()(reinterpret_cast<void*>(selectionManager), 0));
        if (!terrainCollection)
            return 0;

        GridSelectionScanStats stats[kGridSelectionCollectionCount] = {};
        DWORD mask = 0;
        for (int index = 0; index < kGridSelectionCollectionCount; ++index)
        {
            AddGridSelectionTerrainMask(
                reinterpret_cast<void*>(selection),
                terrainCollection,
                index,
                stats[index]);
            mask |= stats[index].mask;
        }

        return mask;
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

    struct CategoryEnablement
    {
        bool hasPaintedCells = false;
        DWORD terrainMask = 0;
    };

    CategoryEnablement GetCategoryEnablement(HWND toolbar)
    {
        CategoryEnablement enablement = {};
        const DWORD rawTerrainMask = ComputePaintedTerrainMask();
        const bool nativePlaceEnabled = NativePlaceObstaclesEnabled(toolbar);
        if (nativePlaceEnabled)
        {
            if (rawTerrainMask != 0)
                s_lastStablePaintedTerrainMask = rawTerrainMask;
        }
        else
        {
            s_lastStablePaintedTerrainMask = 0;
        }

        enablement.terrainMask = nativePlaceEnabled ? s_lastStablePaintedTerrainMask : 0;
        enablement.hasPaintedCells = enablement.terrainMask != 0;

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
            !enablement.hasPaintedCells ||
            (state->index < 32 &&
                (enablement.terrainMask & (1u << state->index)) != 0));
    }

    void RefreshCategoryButtons(VariantPanel& panel)
    {
        if (!panel.layoutApplied || !panel.sourceToolbar)
            return;

        const CategoryEnablement enablement = GetCategoryEnablement(panel.sourceToolbar);
        for (HWND button : panel.categoryButtons)
            RefreshCategoryButton(button, enablement);
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

    bool FindBrushSection(HWND parent, int minTop, RECT& brushLabelRect, RECT& brushToolbarRect)
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

        brushLabelRect = bestLabelRect;
        brushToolbarRect = bestToolbarRect;
        return true;
    }

    void MoveVisibleBrushSectionDown(HWND parent, int startTop, int delta)
    {
        for (HWND child = FirstDirectChild(parent); child; child = NextSibling(child))
        {
            if (!IsWindowVisible(child))
                continue;

            RECT rect = {};
            if (!RectInParent(parent, child, rect))
                continue;

            if (rect.top < startTop)
                continue;

            SetWindowPos(
                child,
                nullptr,
                rect.left,
                rect.top + delta,
                rect.right - rect.left,
                rect.bottom - rect.top,
                SWP_NOZORDER | SWP_NOACTIVATE);
        }
    }

    CategoryButtonState* CategoryButtonStateFromWindow(HWND hwnd)
    {
        return reinterpret_cast<CategoryButtonState*>(GetWindowLongPtrA(hwnd, GWLP_USERDATA));
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

        HBRUSH background = CreateSolidBrush(GetSysColor(COLOR_BTNFACE));
        FillRect(hdc, &rect, background);
        DeleteObject(background);

        if (enabled && (checked || hot || pressed))
        {
            const COLORREF fill =
                pressed ? RGB(186, 226, 255) :
                hot ? RGB(214, 238, 255) :
                RGB(229, 245, 255);
            HBRUSH fillBrush = CreateSolidBrush(fill);
            RECT fillRect = rect;
            InflateRect(&fillRect, -1, -1);
            FillRect(hdc, &fillRect, fillBrush);
            DeleteObject(fillBrush);

            HPEN framePen = CreatePen(PS_SOLID, 1, RGB(148, 213, 255));
            HGDIOBJ oldPen = SelectObject(hdc, framePen);
            HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
            RoundRect(hdc, rect.left, rect.top, rect.right, rect.bottom, 4, 4);
            SelectObject(hdc, oldBrush);
            SelectObject(hdc, oldPen);
            DeleteObject(framePen);
        }

        const int glyph = kCategoryButtonSize;
        RECT glyphRect =
        {
            (rect.right - glyph) / 2,
            (rect.bottom - glyph) / 2,
            (rect.right + glyph) / 2,
            (rect.bottom + glyph) / 2,
        };

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
            if (state && state->index == 0)
                RefreshCategoryButtonsForToolbar(state->sourceToolbar);
            return 0;
        }
        case WM_MOUSEMOVE:
        {
            CategoryButtonState* state = CategoryButtonStateFromWindow(hwnd);
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
            CategoryButtonState* state = CategoryButtonStateFromWindow(hwnd);
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
            CategoryButtonState* state = CategoryButtonStateFromWindow(hwnd);
            if (state && state->enabled)
            {
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
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        }
        case WM_NCDESTROY:
        {
            KillTimer(hwnd, 1);
            CategoryButtonState* state = CategoryButtonStateFromWindow(hwnd);
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
            32,
            32,
            parent,
            nullptr,
            s_pluginModule,
            state);
        if (!hwnd)
        {
            delete state;
            return nullptr;
        }

        SetWindowPos(hwnd, HWND_TOP, x, y, 32, 32, SWP_SHOWWINDOW | SWP_NOACTIVATE);
        if (index == 0)
            SetTimer(hwnd, 1, kCategoryRefreshTimerMs, nullptr);
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

    bool MessageCanAffectObstacleSelection(UINT message)
    {
        return message == WM_LBUTTONDOWN ||
            message == WM_LBUTTONUP ||
            message == WM_MOUSEMOVE ||
            message == WM_KEYDOWN ||
            message == WM_KEYUP;
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
            else if (message && MessageCanAffectObstacleSelection(message->message))
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

        RECT brushLabelRect = {};
        RECT brushToolbarRect = {};
        if (!FindBrushSection(parent, placeRect.top, brushLabelRect, brushToolbarRect))
        {
            LogMessage("Obstacle variants: could not find visible Brush section for category buttons.");
            return false;
        }

        constexpr int rowHeight = 32;
        constexpr int rowGap = 4;
        constexpr int sectionGap = 10;
        const int totalCategoryHeight = rowHeight * 2 + rowGap + sectionGap;

        MoveVisibleBrushSectionDown(parent, brushLabelRect.top, totalCategoryHeight);

        const int x = brushToolbarRect.left;
        const int firstRowY = brushLabelRect.top;
        if (!EnsureCategoryButtonClass())
        {
            MoveVisibleBrushSectionDown(parent, brushLabelRect.top + totalCategoryHeight, -totalCategoryHeight);
            return false;
        }

        bool createdAll = true;
        for (int i = 0; i < kCategoryPlaceholderCount; ++i)
        {
            const int row = i / kCategoryButtonsPerRow;
            const int col = i % kCategoryButtonsPerRow;
            const int buttonX = x + col * rowHeight;
            const int buttonY = firstRowY + row * (rowHeight + rowGap);
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
            for (HWND& button : panel.categoryButtons)
            {
                if (button)
                {
                    DestroyWindow(button);
                    button = nullptr;
                }
            }
            MoveVisibleBrushSectionDown(parent, brushLabelRect.top + totalCategoryHeight, -totalCategoryHeight);
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
            "Obstacle variants: created category placeholder buttons parent=%p first=%p x=%ld y=%ld movedBrushBy=%ld",
            parent,
            panel.categoryButtons[0],
            x,
            firstRowY,
            totalCategoryHeight);
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
