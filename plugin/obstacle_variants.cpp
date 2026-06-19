#include "pch.h"
#include "obstacle_variants.h"

#include "logger.h"
#include "obstacle_classification.h"
#include "obstacle_filter.h"

#include <commctrl.h>
#include <cstring>

namespace
{
    constexpr char kVariantUiEnvVar[] = "HOTA_MAPED_PLUGIN_OBSTACLE_VARIANTS";
    constexpr UINT kVanillaPlaceObstaclesCommand = 32927;
    constexpr UINT kPlaceThickObstaclesCommand = 0x7001;
    constexpr UINT kPlaceThinObstaclesCommand = 0x7002;
    constexpr DWORD kPollIntervalMs = 500;

    struct VariantPanel
    {
        HWND parent = nullptr;
        HWND sourceToolbar = nullptr;
        HWND thickButton = nullptr;
        HWND thinButton = nullptr;
        WNDPROC oldWndProc = nullptr;
    };

    VariantPanel s_panels[8] = {};
    CRITICAL_SECTION s_panelLock = {};
    bool s_panelLockInitialized = false;
    UINT s_installButtonsMessage = 0;

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

    VariantPanel* FindPanelLocked(HWND parent)
    {
        for (VariantPanel& panel : s_panels)
        {
            if (panel.parent == parent)
                return &panel;
        }
        return nullptr;
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

    void AddTooltip(HWND parent, HWND control, const char* text)
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
            parent,
            nullptr,
            nullptr,
            nullptr);
        if (!tooltip)
            return;

        TOOLINFOA info = {};
        info.cbSize = sizeof(info);
        info.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
        info.hwnd = parent;
        info.uId = reinterpret_cast<UINT_PTR>(control);
        info.lpszText = const_cast<char*>(text);
        SendMessageA(tooltip, TTM_ADDTOOLA, 0, reinterpret_cast<LPARAM>(&info));
    }

    void RunObstacleVariantCommand(VariantPanel& panel, ObstacleGenerationFilterMode mode)
    {
        if (!ObstacleThicknessTableAvailable())
        {
            MessageBoxA(
                panel.parent,
                "Obstacle thickness data has not been generated yet.\n\n"
                "Classify obstacles.xlsx, then regenerate the plugin's small "
                "rand_trn thickness table.",
                "HotaMapEdPlugin",
                MB_ICONINFORMATION);
            return;
        }

        const char* modeName =
            mode == ObstacleGenerationFilterMode::ThickOnly ? "thick" : "thin";
        LogFormat("Obstacle variants: running %s obstacle generation.", modeName);

        SetObstacleGenerationFilterMode(mode);
        SendMessageA(
            panel.parent,
            WM_COMMAND,
            MAKEWPARAM(kVanillaPlaceObstaclesCommand, 0),
            reinterpret_cast<LPARAM>(panel.sourceToolbar));
        SetObstacleGenerationFilterMode(ObstacleGenerationFilterMode::None);

        LogFormat("Obstacle variants: finished %s obstacle generation.", modeName);
    }

    bool CreatePanelButtons(VariantPanel& panel);

    LRESULT CALLBACK VariantPanelWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        WNDPROC oldWndProc = nullptr;
        VariantPanel* panel = nullptr;

        EnterCriticalSection(&s_panelLock);
        panel = FindPanelLocked(hwnd);
        if (panel)
            oldWndProc = panel->oldWndProc;
        LeaveCriticalSection(&s_panelLock);

        if (!oldWndProc)
            return DefWindowProcA(hwnd, message, wParam, lParam);

        if (message == s_installButtonsMessage && panel)
        {
            CreatePanelButtons(*panel);
            return 0;
        }

        if (message == WM_COMMAND)
        {
            const UINT command = LOWORD(wParam);
            const UINT notification = HIWORD(wParam);
            if (notification == BN_CLICKED &&
                (command == kPlaceThickObstaclesCommand ||
                    command == kPlaceThinObstaclesCommand) &&
                panel)
            {
                RunObstacleVariantCommand(
                    *panel,
                    command == kPlaceThickObstaclesCommand
                        ? ObstacleGenerationFilterMode::ThickOnly
                        : ObstacleGenerationFilterMode::ThinOnly);
                return 0;
            }
        }

        if (message == WM_NCDESTROY)
        {
            LRESULT result = CallWindowProcA(oldWndProc, hwnd, message, wParam, lParam);
            EnterCriticalSection(&s_panelLock);
            VariantPanel* deadPanel = FindPanelLocked(hwnd);
            if (deadPanel)
                *deadPanel = VariantPanel();
            LeaveCriticalSection(&s_panelLock);
            return result;
        }

        return CallWindowProcA(oldWndProc, hwnd, message, wParam, lParam);
    }

    bool CreatePanelButtons(VariantPanel& panel)
    {
        if (panel.thickButton && IsWindow(panel.thickButton) &&
            panel.thinButton && IsWindow(panel.thinButton))
        {
            return true;
        }

        HWND parent = panel.parent;
        HWND toolbar = panel.sourceToolbar;

        EnterCriticalSection(&s_panelLock);
        const bool knownPanel = FindPanelLocked(parent) == &panel;
        LeaveCriticalSection(&s_panelLock);
        if (!knownPanel || !IsWindow(parent) || !IsWindow(toolbar))
            return false;

        RECT toolbarRect = {};
        RECT clientRect = {};
        if (!GetWindowRect(toolbar, &toolbarRect) || !GetClientRect(parent, &clientRect))
        {
            return false;
        }
        MapWindowPoints(HWND_DESKTOP, parent, reinterpret_cast<POINT*>(&toolbarRect), 2);

        const int buttonHeight = max(20, toolbarRect.bottom - toolbarRect.top);
        int buttonWidth = 44;
        const int gap = 4;
        int x = toolbarRect.right + 8;
        const int y = toolbarRect.top;
        if (x + buttonWidth * 2 + gap > clientRect.right - 4)
        {
            buttonWidth = buttonHeight + 8;
            x = toolbarRect.right + 4;
        }

        const DWORD style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON;
        HWND thickButton = CreateWindowExA(
            0,
            "BUTTON",
            buttonWidth >= 40 ? "Thick" : "T",
            style,
            x,
            y,
            buttonWidth,
            buttonHeight,
            parent,
            reinterpret_cast<HMENU>(kPlaceThickObstaclesCommand),
            nullptr,
            nullptr);
        HWND thinButton = CreateWindowExA(
            0,
            "BUTTON",
            buttonWidth >= 40 ? "Thin" : "t",
            style,
            x + buttonWidth + gap,
            y,
            buttonWidth,
            buttonHeight,
            parent,
            reinterpret_cast<HMENU>(kPlaceThinObstaclesCommand),
            nullptr,
            nullptr);

        if (!thickButton || !thinButton)
        {
            LogMessage("Obstacle variants: failed to create side-panel buttons.");
            if (thickButton)
                DestroyWindow(thickButton);
            if (thinButton)
                DestroyWindow(thinButton);
            return false;
        }

        const bool tableAvailable = ObstacleThicknessTableAvailable();
        EnableWindow(thickButton, tableAvailable);
        EnableWindow(thinButton, tableAvailable);
        AddTooltip(parent, thickButton, "Place Thick Obstacles");
        AddTooltip(parent, thinButton, "Place Thin Obstacles");

        EnterCriticalSection(&s_panelLock);
        VariantPanel* installedPanel = FindPanelLocked(parent);
        if (installedPanel == &panel)
        {
            installedPanel->thickButton = thickButton;
            installedPanel->thinButton = thinButton;
        }
        LeaveCriticalSection(&s_panelLock);

        LogFormat(
            "Obstacle variants: installed side-panel buttons parent=%p toolbar=%p thicknessTable=%s",
            parent,
            toolbar,
            tableAvailable ? "available" : "missing");
        return true;
    }

    bool InstallPanelForToolbar(HWND toolbar)
    {
        HWND parent = GetParent(toolbar);
        if (!parent)
            return false;

        EnterCriticalSection(&s_panelLock);
        if (FindPanelLocked(parent))
        {
            LeaveCriticalSection(&s_panelLock);
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
        LeaveCriticalSection(&s_panelLock);

        WNDPROC oldWndProc = reinterpret_cast<WNDPROC>(
            SetWindowLongPtrA(parent, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(VariantPanelWndProc)));
        if (!oldWndProc)
        {
            LogMessage("Obstacle variants: failed to subclass obstacle side panel.");
            EnterCriticalSection(&s_panelLock);
            VariantPanel* failedPanel = FindPanelLocked(parent);
            if (failedPanel)
                *failedPanel = VariantPanel();
            LeaveCriticalSection(&s_panelLock);
            return false;
        }

        EnterCriticalSection(&s_panelLock);
        VariantPanel* installedPanel = FindPanelLocked(parent);
        if (installedPanel)
        {
            installedPanel->oldWndProc = oldWndProc;
        }
        LeaveCriticalSection(&s_panelLock);

        if (!PostMessageA(parent, s_installButtonsMessage, 0, 0))
            LogMessage("Obstacle variants: failed to post side-panel button creation message.");
        return true;
    }

    BOOL CALLBACK EnumChildProc(HWND hwnd, LPARAM)
    {
        if (ToolbarHasCommand(hwnd, kVanillaPlaceObstaclesCommand))
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
    s_installButtonsMessage =
        RegisterWindowMessageA("HotaMapEdPlugin.ObstacleVariants.InstallButtons");
    if (!s_installButtonsMessage)
    {
        error = "could not register obstacle variant UI message";
        return false;
    }

    if (!s_panelLockInitialized)
    {
        InitializeCriticalSection(&s_panelLock);
        s_panelLockInitialized = true;
    }

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
