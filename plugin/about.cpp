#include "pch.h"
#include "about.h"
#include "../shared/localization.h"
#include "about_resource.h"
#include "../resources/version.h"
#include "logger.h"
#include <commctrl.h>
#include <shellapi.h>
#include <cstdlib>

#pragma comment(lib, "shell32.lib")

namespace
{
    constexpr UINT kAboutCommand = 0xEFC1;
    constexpr UINT kNativeAboutCommand = 0xE140; // MFC ID_APP_ABOUT, same EXE in both languages.
    constexpr UINT_PTR kSubclass = 0x43544F41;
    constexpr wchar_t kInstalled[] = L"HotACustomObstacleTool.About";

    INT_PTR CALLBACK AboutDialog(HWND dialog, UINT message, WPARAM wParam, LPARAM lParam)
    {
        if (message == WM_INITDIALOG)
        {
            SetWindowTextW(dialog, UiText("About Custom Obstacle Tool"));
#ifdef _DEBUG
            SetDlgItemTextW(dialog, IDC_TOOL_BUILD, UiText("Debug Build"));
#else
            SetDlgItemTextW(dialog, IDC_TOOL_BUILD, UiText("Release Build"));
#endif
            SetDlgItemTextW(dialog, IDC_TOOL_AUTHOR, UiText(TOOL_CREDIT));
            SetDlgItemTextW(dialog, IDC_TOOL_DESCRIPTION, UiText(TOOL_DESCRIPTION));
            SetDlgItemTextW(dialog, IDOK, UiText("OK"));
            const std::wstring link = std::wstring(L"<a>") + UiText("GitHub repository") + L"</a>";
            SetDlgItemTextW(dialog, IDC_TOOL_REPOSITORY, link.c_str());
            RECT bounds{}, ownerBounds{};
            GetWindowRect(dialog, &bounds);
            HWND owner = GetParent(dialog);
            GetWindowRect(owner, &ownerBounds);
            MONITORINFO monitor{ sizeof(monitor) };
            GetMonitorInfoW(MonitorFromWindow(owner, MONITOR_DEFAULTTONEAREST), &monitor);
            const int width = bounds.right - bounds.left;
            const int height = bounds.bottom - bounds.top;
            int x = ownerBounds.left + (ownerBounds.right - ownerBounds.left - width) / 2;
            int y = ownerBounds.top + (ownerBounds.bottom - ownerBounds.top - height) / 2;
            x = max(monitor.rcWork.left, min(x, monitor.rcWork.right - width));
            y = max(monitor.rcWork.top, min(y, monitor.rcWork.bottom - height));
            SetWindowPos(dialog, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
            return TRUE;
        }
        if (message == WM_COMMAND && (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL))
        {
            EndDialog(dialog, LOWORD(wParam));
            return TRUE;
        }
        if (message == WM_CLOSE)
        {
            EndDialog(dialog, IDCANCEL);
            return TRUE;
        }
        if (message == WM_NOTIFY)
        {
            const auto* notification = reinterpret_cast<const NMHDR*>(lParam);
            if (notification->idFrom == IDC_TOOL_REPOSITORY &&
                (notification->code == NM_CLICK || notification->code == NM_RETURN))
            {
                ShellExecuteA(dialog, "open", TOOL_REPOSITORY, nullptr, nullptr, SW_SHOWNORMAL);
                return TRUE;
            }
        }
        return FALSE;
    }

    LRESULT CALLBACK AboutWindow(HWND window, UINT message, WPARAM wParam,
        LPARAM lParam, UINT_PTR, DWORD_PTR)
    {
        if (message == WM_COMMAND && LOWORD(wParam) == kAboutCommand && lParam == 0)
        {
            HMODULE module = nullptr;
            GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                reinterpret_cast<LPCWSTR>(&AboutWindow), &module);
            // SysLink requires common-controls v6 even if the host has no manifest.
            wchar_t modulePath[32768]{};
            GetModuleFileNameW(module, modulePath, _countof(modulePath));
            ACTCTXW context{ sizeof(context) };
            context.dwFlags = ACTCTX_FLAG_RESOURCE_NAME_VALID | ACTCTX_FLAG_HMODULE_VALID;
            context.lpSource = modulePath;
            context.lpResourceName = MAKEINTRESOURCEW(IDR_ABOUT_MANIFEST);
            context.hModule = module;
            HANDLE activation = CreateActCtxW(&context);
            ULONG_PTR cookie = 0;
            const bool activated = activation != INVALID_HANDLE_VALUE && ActivateActCtx(activation, &cookie);
            INITCOMMONCONTROLSEX controls{ sizeof(controls), ICC_LINK_CLASS };
            InitCommonControlsEx(&controls);
            if (DialogBoxParamW(module, MAKEINTRESOURCEW(IDD_TOOL_ABOUT), window, AboutDialog, 0) == -1)
                LogFormat("About dialog failed, gle=%lu.", GetLastError());
            if (activated) DeactivateActCtx(0, cookie);
            if (activation != INVALID_HANDLE_VALUE) ReleaseActCtx(activation);
            return 0;
        }
        if (message == WM_INITMENUPOPUP && !HIWORD(lParam))
        {
            // The editor disables commands without a native update handler.
            // Let it update its own items first, then enable our command only.
            const LRESULT result = DefSubclassProc(window, message, wParam, lParam);
            HMENU popup = reinterpret_cast<HMENU>(wParam);
            if (GetMenuState(popup, kAboutCommand, MF_BYCOMMAND) != UINT(-1))
                EnableMenuItem(popup, kAboutCommand, MF_BYCOMMAND | MF_ENABLED);
            return result;
        }
        if (message == WM_NCDESTROY)
        {
            RemovePropW(window, kInstalled);
            RemoveWindowSubclass(window, AboutWindow, kSubclass);
        }
        return DefSubclassProc(window, message, wParam, lParam);
    }
}

UINT AboutInstallMessage()
{
    static const UINT message = RegisterWindowMessageW(L"HotACustomObstacleTool.InstallAbout");
    return message;
}

bool InstallAboutMenu(HWND owner)
{
    // Called only by the owning UI thread, including when Standard mode is active.
    DetectEditorUiLanguage(owner);
    if (GetPropW(owner, kInstalled))
        return true;
    HMENU menu = GetMenu(owner);
    if (!menu || GetMenuState(menu, kAboutCommand, MF_BYCOMMAND) != UINT(-1))
        return false;
    HMENU help = nullptr;
    for (int i = 0; i < GetMenuItemCount(menu); ++i)
    {
        HMENU submenu = GetSubMenu(menu, i);
        if (submenu && GetMenuState(submenu, kNativeAboutCommand, MF_BYCOMMAND) != UINT(-1))
            help = submenu;
    }
    if (!help || !SetWindowSubclass(owner, AboutWindow, kSubclass, 0))
        return false;
    if (!SetPropW(owner, kInstalled, reinterpret_cast<HANDLE>(1)))
    {
        RemoveWindowSubclass(owner, AboutWindow, kSubclass);
        return false;
    }
    if (!AppendMenuW(help, MF_STRING, kAboutCommand, UiText("About Custom Obstacle Tool...")))
    {
        RemovePropW(owner, kInstalled);
        RemoveWindowSubclass(owner, AboutWindow, kSubclass);
        return false;
    }
    DrawMenuBar(owner);
    LogMessage("About Custom Obstacle Tool menu installed.");
    return true;
}
