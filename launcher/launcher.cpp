#include <windows.h>
#include <shlobj.h>
#include <string>

#define APP_NAME    "HotaMapEdPlugin"
#define EDITOR_EXE  "h3hota_maped.exe"
#define PLUGIN_DLL  "HotaMapEdPlugin.dll"
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
    return GetFileAttributesA((dir + EDITOR_EXE).c_str()) != INVALID_FILE_ATTRIBUTES;
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

    // 4. Let the user browse
    BROWSEINFOA bi = {};
    bi.lpszTitle = "Select the HotA installation folder (containing " EDITOR_EXE ")";
    bi.ulFlags   = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    LPITEMIDLIST pidl = SHBrowseForFolderA(&bi);
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
            MessageBoxA(nullptr,
                EDITOR_EXE " was not found in the selected folder.",
                APP_NAME, MB_ICONERROR);
        }
    }

    return {};
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    std::string launchDir = LauncherDir();
    std::string dllPath   = launchDir + PLUGIN_DLL;

    if (GetFileAttributesA(dllPath.c_str()) == INVALID_FILE_ATTRIBUTES)
    {
        MessageBoxA(nullptr,
            PLUGIN_DLL " not found next to the launcher.",
            APP_NAME, MB_ICONERROR);
        return 1;
    }

    std::string hotaDir = FindHotaDir();
    if (hotaDir.empty())
    {
        MessageBoxA(nullptr,
            "Could not find the HotA installation folder.\n\n"
            "Add a line to " INI_FILE " next to the launcher:\n"
            "  [Settings]\n"
            "  HotaPath=C:\\Path\\To\\HotA",
            APP_NAME, MB_ICONERROR);
        return 1;
    }

    std::string exePath = hotaDir + EDITOR_EXE;

    // Launch the editor suspended so we can inject before any code runs.
    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {};
    if (!CreateProcessA(exePath.c_str(), nullptr, nullptr, nullptr, FALSE,
                        CREATE_SUSPENDED, nullptr, hotaDir.c_str(), &si, &pi))
    {
        MessageBoxA(nullptr, "Failed to launch " EDITOR_EXE ".", APP_NAME, MB_ICONERROR);
        return 1;
    }

    // Write the DLL path string into the target process.
    SIZE_T pathLen  = dllPath.size() + 1;
    LPVOID remoteStr = VirtualAllocEx(pi.hProcess, nullptr, pathLen,
                                      MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remoteStr)
    {
        TerminateProcess(pi.hProcess, 1);
        CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
        MessageBoxA(nullptr, "VirtualAllocEx failed.", APP_NAME, MB_ICONERROR);
        return 1;
    }
    WriteProcessMemory(pi.hProcess, remoteStr, dllPath.c_str(), pathLen, nullptr);

    // Call LoadLibraryA in the target process to load our plugin.
    FARPROC loadLib = GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryA");
    HANDLE hThread  = CreateRemoteThread(pi.hProcess, nullptr, 0,
        reinterpret_cast<LPTHREAD_START_ROUTINE>(loadLib), remoteStr, 0, nullptr);
    if (!hThread)
    {
        VirtualFreeEx(pi.hProcess, remoteStr, 0, MEM_RELEASE);
        TerminateProcess(pi.hProcess, 1);
        CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
        MessageBoxA(nullptr, "CreateRemoteThread failed.", APP_NAME, MB_ICONERROR);
        return 1;
    }

    // Wait for our DLL to finish loading, then resume the main thread.
    WaitForSingleObject(hThread, INFINITE);
    CloseHandle(hThread);
    VirtualFreeEx(pi.hProcess, remoteStr, 0, MEM_RELEASE);

    ResumeThread(pi.hThread);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return 0;
}
