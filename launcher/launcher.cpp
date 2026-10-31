#include <windows.h>
#include <shlobj.h>
#include <string>

#include "../plugin/plugin_api.h"
#include "../shared/localization.h"

#define APP_NAME    "HotACustomObstacleTool"
#define APP_NAME_W  L"HotACustomObstacleTool"
#define EDITOR_EXE  "h3hota_maped.exe"
#define PLUGIN_DLL  "HotACustomObstacleTool.dll"
#define INI_FILE    "HotaMapEdPlugin.ini"

// Returns the directory containing this launcher exe (with trailing backslash).
static std::string LauncherDir()
{
    char buf[MAX_PATH];
    GetModuleFileNameA(nullptr, buf, MAX_PATH);
    std::string s = buf;
    return s.substr(0, s.rfind('\\') + 1);
}

static bool HasEditor(const std::string& dir)
{
    const bool found = GetFileAttributesA((dir + EDITOR_EXE).c_str()) != INVALID_FILE_ATTRIBUTES;
    if (found) SetUiLanguage(LanguageForEditorDirectory(dir.c_str()));
    return found;
}

// Searches for the HotA install directory in priority order.
// Returns the directory with a trailing backslash, or empty string.
static std::string FindHotaDir()
{
    std::string launchDir = LauncherDir();

    // 1. Editor in the same directory as the launcher
    if (HasEditor(launchDir))
        return launchDir;

    // 2. Path from INI file
    char iniValue[MAX_PATH] = {};
    GetPrivateProfileStringA("Settings", "HotaPath", "",
        iniValue, MAX_PATH, (launchDir + INI_FILE).c_str());
    if (iniValue[0])
    {
        std::string p = iniValue;
        if (p.back() != '\\') p += '\\';
        if (HasEditor(p)) return p;
    }

    // 3. Windows registry — known install key locations
    const struct { HKEY root; const char* key; const char* value; } reg[] =
    {
        { HKEY_LOCAL_MACHINE, "SOFTWARE\\HotA",                             "InstallPath" },
        { HKEY_LOCAL_MACHINE, "SOFTWARE\\WOW6432Node\\HotA",               "InstallPath" },
        { HKEY_LOCAL_MACHINE,
          "SOFTWARE\\New World Computing\\Heroes of Might and Magic 3\\1.0","AppPath" },
        { HKEY_LOCAL_MACHINE,
          "SOFTWARE\\WOW6432Node\\New World Computing\\"
          "Heroes of Might and Magic 3\\1.0",                              "AppPath" },
    };
    for (const auto& r : reg)
    {
        HKEY hKey;
        if (RegOpenKeyExA(r.root, r.key, 0, KEY_READ, &hKey) != ERROR_SUCCESS)
            continue;
        char buf[MAX_PATH] = {};
        DWORD sz = MAX_PATH;
        bool ok = RegQueryValueExA(hKey, r.value, nullptr, nullptr,
                                   reinterpret_cast<LPBYTE>(buf), &sz) == ERROR_SUCCESS;
        RegCloseKey(hKey);
        if (ok && buf[0])
        {
            std::string p = buf;
            if (p.back() != '\\') p += '\\';
            if (HasEditor(p)) return p;
        }
    }

    // 4. Scan Add/Remove Programs registry for any HotA / H3 installer entry.
    //    Catches GOG, custom installers, and anything else that registers itself.
    {
        const struct { HKEY root; const char* key; } hives[] =
        {
            { HKEY_LOCAL_MACHINE,
              "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall" },
            { HKEY_LOCAL_MACHINE,
              "SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall" },
            { HKEY_CURRENT_USER,
              "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall" },
        };
        for (const auto& hive : hives)
        {
            HKEY hUninstall;
            if (RegOpenKeyExA(hive.root, hive.key, 0,
                              KEY_READ | KEY_ENUMERATE_SUB_KEYS, &hUninstall) != ERROR_SUCCESS)
                continue;

            char subkeyName[256];
            for (DWORD i = 0; ; ++i)
            {
                DWORD nameLen = sizeof(subkeyName);
                if (RegEnumKeyExA(hUninstall, i, subkeyName, &nameLen,
                                  nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS)
                    break;

                HKEY hApp;
                if (RegOpenKeyExA(hUninstall, subkeyName, 0, KEY_READ, &hApp) != ERROR_SUCCESS)
                    continue;

                char displayName[256] = {};
                DWORD sz = sizeof(displayName);
                RegQueryValueExA(hApp, "DisplayName", nullptr, nullptr,
                                 reinterpret_cast<LPBYTE>(displayName), &sz);

                bool relevant =
                    strstr(displayName, "Horn of the Abyss") != nullptr ||
                    strstr(displayName, "HotA")              != nullptr ||
                    strstr(subkeyName,  "HotA")              != nullptr;

                std::string location;
                if (relevant)
                {
                    char loc[MAX_PATH] = {};
                    sz = sizeof(loc);
                    RegQueryValueExA(hApp, "InstallLocation", nullptr, nullptr,
                                     reinterpret_cast<LPBYTE>(loc), &sz);
                    if (loc[0]) location = loc;
                }
                RegCloseKey(hApp);

                if (!location.empty())
                {
                    if (location.back() != '\\') location += '\\';
                    if (HasEditor(location)) return location;
                }
            }
            RegCloseKey(hUninstall);
        }
    }

    // 5. Common install paths — based on known installer defaults.
    //    HotA installs on top of Heroes 3 (no separate directory of its own).
    //    The classic 3DO installer defaults to C:\3DO\Heroes3 or a user-chosen path.
    //    GOG installs to C:\GOG Games\Heroes of Might and Magic 3 Complete HD.
    //    Steam (HD Edition) installs under steamapps\common.
    {
        // Per-user paths: the 3DO installer lets users pick any location.
        // Check %USERPROFILE% subdirectories commonly used for games.
        char userProfile[MAX_PATH] = {};
        SHGetFolderPathA(nullptr, CSIDL_PROFILE, nullptr, 0, userProfile);
        if (userProfile[0])
        {
            std::string up = std::string(userProfile) + "\\";
            const char* upRelative[] =
            {
                "3DO\\Heroes3\\",
                "Heroes of Might and Magic 3 Complete HD\\",
            };
            for (const char* rel : upRelative)
            {
                std::string p = up + rel;
                if (HasEditor(p)) return p;
            }
        }

        // System-level paths
        const char* systemDirs[] =
        {
            "C:\\3DO\\Heroes3\\",
            "C:\\GOG Games\\Heroes of Might and Magic 3 Complete HD\\",
        };
        for (const char* d : systemDirs)
            if (HasEditor(d)) return d;

        // Program Files — GOG and Steam both install here by default
        char pf[MAX_PATH] = {}, pf86[MAX_PATH] = {};
        SHGetFolderPathA(nullptr, CSIDL_PROGRAM_FILES,    nullptr, 0, pf);
        SHGetFolderPathA(nullptr, CSIDL_PROGRAM_FILESX86, nullptr, 0, pf86);

        const char* pfRelative[] =
        {
            "Heroes of Might and Magic 3 Complete HD\\",
            "Heroes of Might and Magic III\\",
            "Steam\\steamapps\\common\\Heroes of Might & Magic III - HD Edition\\",
        };
        for (const char* rel : pfRelative)
        {
            for (const char* base : { pf86, pf })
            {
                if (!base[0]) continue;
                std::string p = std::string(base) + "\\" + rel;
                if (HasEditor(p)) return p;
            }
        }
    }

    // 6. Let the user browse
    BROWSEINFOW bi = {};
    bi.lpszTitle = UiText("Select the HotA installation folder (containing " EDITOR_EXE ")");
    bi.ulFlags   = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
    if (pidl)
    {
        char browsed[MAX_PATH] = {};
        SHGetPathFromIDListA(pidl, browsed);
        CoTaskMemFree(pidl);
        if (browsed[0])
        {
            std::string p = browsed;
            if (p.back() != '\\') p += '\\';
            if (HasEditor(p))
            {
                WritePrivateProfileStringA("Settings", "HotaPath", p.c_str(),
                    (launchDir + INI_FILE).c_str());
                return p;
            }
            MessageBoxW(nullptr,
                UiText(EDITOR_EXE " was not found in the selected folder."),
                APP_NAME_W, MB_ICONERROR);
        }
    }

    return {};
}

static bool WaitForRemoteThread(HANDLE thread, DWORD& exitCode)
{
    return WaitForSingleObject(thread, INFINITE) == WAIT_OBJECT_0 &&
        GetExitCodeThread(thread, &exitCode) != FALSE;
}

static const char* InitializeFailureMessage(DWORD result)
{
    switch (result)
    {
    case kPluginInitializeHashFailed:
        return "The plugin could not hash the map editor executable.";
    case kPluginInitializeUnsupportedTarget:
        return "This map editor executable or hota_me.dll version is not supported by the plugin.";
    case kPluginInitializeHookFailed:
        return "The plugin found unexpected hook bytes or could not install its hooks.";
    case kPluginInitializeAlreadyFailed:
        return "Plugin initialization previously failed.";
    case kPluginInitializeBusy:
        return "Plugin initialization is already in progress.";
    default:
        return "Plugin initialization returned an unknown error.";
    }
}

static bool InjectAndInitializePlugin(
    HANDLE process,
    const std::string& dllPath,
    std::string& error,
    bool& targetStateUncertain)
{
    targetStateUncertain = false;

    const SIZE_T pathLength = dllPath.size() + 1;
    LPVOID remotePath = VirtualAllocEx(process, nullptr, pathLength,
        MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remotePath)
    {
        error = "VirtualAllocEx failed.";
        return false;
    }

    SIZE_T bytesWritten = 0;
    if (!WriteProcessMemory(process, remotePath, dllPath.c_str(), pathLength, &bytesWritten) ||
        bytesWritten != pathLength)
    {
        VirtualFreeEx(process, remotePath, 0, MEM_RELEASE);
        error = "WriteProcessMemory failed.";
        return false;
    }

    FARPROC loadLibrary = GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryA");
    if (!loadLibrary)
    {
        VirtualFreeEx(process, remotePath, 0, MEM_RELEASE);
        error = "Could not resolve LoadLibraryA.";
        return false;
    }

    HANDLE loadThread = CreateRemoteThread(process, nullptr, 0,
        reinterpret_cast<LPTHREAD_START_ROUTINE>(loadLibrary), remotePath, 0, nullptr);
    if (!loadThread)
    {
        VirtualFreeEx(process, remotePath, 0, MEM_RELEASE);
        error = "CreateRemoteThread for LoadLibraryA failed.";
        return false;
    }

    DWORD remoteModule = 0;
    const bool loadCompleted = WaitForRemoteThread(loadThread, remoteModule);
    CloseHandle(loadThread);
    VirtualFreeEx(process, remotePath, 0, MEM_RELEASE);
    if (!loadCompleted || remoteModule == 0)
    {
        error = "Loading " PLUGIN_DLL " in the editor failed.";
        return false;
    }

    HMODULE localModule = LoadLibraryExA(dllPath.c_str(), nullptr, DONT_RESOLVE_DLL_REFERENCES);
    if (!localModule)
    {
        error = "Loading the local plugin image for export lookup failed.";
        return false;
    }

    FARPROC localInitialize = GetProcAddress(localModule, kPluginInitializeExport);
    if (!localInitialize)
        localInitialize = GetProcAddress(localModule, "_HotaMapEdPluginInitialize@4");
    if (!localInitialize)
    {
        FreeLibrary(localModule);
        error = "The plugin initialization export was not found.";
        return false;
    }

    const DWORD_PTR initializeOffset =
        reinterpret_cast<DWORD_PTR>(localInitialize) - reinterpret_cast<DWORD_PTR>(localModule);
    FreeLibrary(localModule);

    const auto remoteInitialize = reinterpret_cast<LPTHREAD_START_ROUTINE>(
        static_cast<DWORD_PTR>(remoteModule) + initializeOffset);
    HANDLE initializeThread =
        CreateRemoteThread(process, nullptr, 0, remoteInitialize, nullptr, 0, nullptr);
    if (!initializeThread)
    {
        error = "CreateRemoteThread for plugin initialization failed.";
        return false;
    }

    targetStateUncertain = true;
    DWORD initializeResult = kPluginInitializeAlreadyFailed;
    const bool initializeCompleted = WaitForRemoteThread(initializeThread, initializeResult);
    CloseHandle(initializeThread);
    if (!initializeCompleted)
    {
        error = "Waiting for plugin initialization failed.";
        return false;
    }
    if (initializeResult != kPluginInitializeSuccess)
    {
        targetStateUncertain =
            initializeResult != kPluginInitializeHashFailed &&
            initializeResult != kPluginInitializeUnsupportedTarget;
        error = InitializeFailureMessage(initializeResult);
        error += "\n\nSee HotaMapEdPlugin.log next to the launcher for details.";
        return false;
    }

    targetStateUncertain = false;
    return true;
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    // Early prompts have no editor language to inspect yet. Prefer the known
    // installation next to the launcher or in its saved path over Windows UI.
    SetUiLanguage(PRIMARYLANGID(GetUserDefaultUILanguage()) == LANG_RUSSIAN
        ? UiLanguage::Russian : UiLanguage::English);
    std::string launchDir = LauncherDir();
    if (!HasEditor(launchDir))
    {
        char savedPath[MAX_PATH]{};
        GetPrivateProfileStringA("Settings", "HotaPath", "", savedPath, MAX_PATH,
            (launchDir + INI_FILE).c_str());
        if (savedPath[0])
        {
            std::string saved = savedPath;
            if (saved.back() != '\\') saved += '\\';
            HasEditor(saved);
        }
    }
    std::string dllPath = launchDir + PLUGIN_DLL;

    if (GetFileAttributesA(dllPath.c_str()) == INVALID_FILE_ATTRIBUTES)
    {
        MessageBoxW(nullptr,
            UiText(PLUGIN_DLL " not found next to the launcher."),
            APP_NAME_W, MB_ICONERROR);
        return 1;
    }

    std::string hotaDir = FindHotaDir();
    if (hotaDir.empty())
    {
        MessageBoxW(nullptr,
            UiText("Could not find the HotA installation folder.\n\n"
            "Place the launcher in your HotA directory, or add a line to\n"
            INI_FILE " next to the launcher:\n"
            "  [Settings]\n"
            "  HotaPath=C:\\Path\\To\\HotA"),
            APP_NAME_W, MB_ICONERROR);
        return 1;
    }

    std::string exePath = hotaDir + EDITOR_EXE;

    // Launch the editor and wait until its message loop is running before injecting.
    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {};
    if (!CreateProcessA(exePath.c_str(), nullptr, nullptr, nullptr, FALSE,
                        0, nullptr, hotaDir.c_str(), &si, &pi))
    {
        MessageBoxW(nullptr, UiText("Failed to launch " EDITOR_EXE "."), APP_NAME_W, MB_ICONERROR);
        return 1;
    }

    // Block until the editor is idle (message loop running, window visible).
    // This ensures the GUI thread is fully initialized before we inject,
    // which prevents the keyboard-input routing issues caused by injecting
    // into a suspended process before USER32 is set up on the main thread.
    WaitForInputIdle(pi.hProcess, 10000);

    std::string injectError;
    bool targetStateUncertain = false;
    if (!InjectAndInitializePlugin(pi.hProcess, dllPath, injectError, targetStateUncertain))
    {
        if (targetStateUncertain)
        {
            TerminateProcess(pi.hProcess, 1);
            injectError += "\n\nThe editor was closed because its patch state could not be proven safe.";
        }
        else
        {
            injectError += "\n\nThe editor will continue without the plugin.";
        }
        MessageBoxW(nullptr, UiMessage(injectError).c_str(), APP_NAME_W, MB_ICONERROR);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return 1;
    }

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return 0;
}
