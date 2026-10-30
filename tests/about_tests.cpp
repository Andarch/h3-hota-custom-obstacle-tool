// Exercise native menu routing and the actual modal dialog resource.
#include "../plugin/about.cpp"
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#define CHECK_ABOUT(x) do { if (!(x)) { std::fprintf(stderr, "About test failed: %s at %d\n", #x, __LINE__); ExitProcess(1); } } while (false)
namespace
{
    bool nativePopupUpdated;
    LRESULT CALLBACK NativeMenuUpdate(HWND window, UINT message, WPARAM wParam,
        LPARAM lParam, UINT_PTR, DWORD_PTR)
    {
        if (message == WM_INITMENUPOPUP)
        {
            nativePopupUpdated = true;
            HMENU popup = reinterpret_cast<HMENU>(wParam);
            EnableMenuItem(popup, kNativeAboutCommand, MF_BYCOMMAND | MF_GRAYED);
            EnableMenuItem(popup, kAboutCommand, MF_BYCOMMAND | MF_GRAYED);
            return 123;
        }
        return DefSubclassProc(window, message, wParam, lParam);
    }
    int closeCommand;
    bool foundVersion, foundAuthor, foundDescription, foundBuild;
    BOOL CALLBACK InspectText(HWND control, LPARAM)
    {
        wchar_t text[300]{};
        GetWindowTextW(control, text, _countof(text));
#ifdef _DEBUG
        foundBuild |= wcscmp(text, L"Debug Build") == 0;
#else
        foundBuild |= wcscmp(text, L"Release Build") == 0;
#endif
        foundVersion |= wcscmp(text, L"v0.9.1") == 0;
        foundAuthor |= wcscmp(text, L"By Andarch") == 0;
        foundDescription |= wcscmp(text,
            L"Customizable obstacle generation for the HotA Map Editor") == 0;
        return TRUE;
    }
    bool inspected;
    void CALLBACK InspectDialog(HWND owner, UINT, UINT_PTR timer, DWORD)
    {
        HWND dialog = GetWindow(owner, GW_ENABLEDPOPUP);
        if (dialog == owner) return;
        KillTimer(owner, timer);
        wchar_t title[100]{};
        GetWindowTextW(dialog, title, 100);
        CHECK_ABOUT(wcscmp(title, L"About Custom Obstacle Tool") == 0);
        CHECK_ABOUT(GetDlgItem(dialog, IDOK));
        CHECK_ABOUT(GetDlgItem(dialog, IDC_TOOL_REPOSITORY));
        foundVersion = foundAuthor = foundDescription = foundBuild = false;
        EnumChildWindows(dialog, InspectText, 0);
        CHECK_ABOUT(foundVersion && foundAuthor && foundDescription && foundBuild);
        CHECK_ABOUT(!IsWindowEnabled(owner));
        RECT bounds{}, parent{};
        GetWindowRect(dialog, &bounds);
        GetWindowRect(owner, &parent);
        CHECK_ABOUT(abs((bounds.left + bounds.right) - (parent.left + parent.right)) <= 2);
        CHECK_ABOUT(abs((bounds.top + bounds.bottom) - (parent.top + parent.bottom)) <= 2);
        inspected = true;
        if (closeCommand == IDCANCEL)
            PostMessageW(dialog, WM_KEYDOWN, VK_ESCAPE, 0);
        else
            PostMessageW(dialog, WM_COMMAND, IDOK, 0);
    }
}
void TestAboutCaption(const wchar_t* helpCaption)
{
    HWND owner = CreateWindowExW(0, L"STATIC", L"About test owner", WS_OVERLAPPEDWINDOW,
        100, 100, 700, 500, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    CHECK_ABOUT(owner);
    HMENU menu = CreateMenu(), help = CreatePopupMenu();
    AppendMenuW(help, MF_STRING, 100, L"Help Topics");
    AppendMenuW(help, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(help, MF_STRING, kNativeAboutCommand, L"About Map Editor...");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(help), helpCaption);
    SetMenu(owner, menu);
    CHECK_ABOUT(SetWindowSubclass(owner, NativeMenuUpdate, 1, 0));
    CHECK_ABOUT(InstallAboutMenu(owner));
    CHECK_ABOUT(InstallAboutMenu(owner));
    CHECK_ABOUT(GetMenuItemCount(help) == 4);
    CHECK_ABOUT(GetMenuItemID(help, 2) == kNativeAboutCommand);
    CHECK_ABOUT(GetMenuItemID(help, 3) == kAboutCommand);
    for (int command : { IDOK, IDCANCEL })
    {
        nativePopupUpdated = false;
        CHECK_ABOUT(SendMessageW(owner, WM_INITMENUPOPUP,
            reinterpret_cast<WPARAM>(help), MAKELPARAM(0, FALSE)) == 123);
        CHECK_ABOUT(nativePopupUpdated);
        CHECK_ABOUT(!(GetMenuState(help, kAboutCommand, MF_BYCOMMAND) & (MF_GRAYED | MF_DISABLED)));
        CHECK_ABOUT(GetMenuState(help, kNativeAboutCommand, MF_BYCOMMAND) & MF_GRAYED);
        inspected = false;
        closeCommand = command;
        SetTimer(owner, 1, 50, InspectDialog);
        SendMessageW(owner, WM_COMMAND, kAboutCommand, 0);
        CHECK_ABOUT(inspected);
        CHECK_ABOUT(IsWindowEnabled(owner));
    }
    DestroyWindow(owner);
    std::puts("PASS: About menu preserves native entries/states, stays enabled after native popup updates, installs once, opens centered modal dialog and closes with OK/Escape.");
}

void TestAboutUi()
{
    TestAboutCaption(L"&Help");
    TestAboutCaption(L"\u0421\u043f\u0440\u0430\u0432\u043a\u0430");
}
