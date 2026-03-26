#include "pch.h"
#include "hook.h"
#include "garrison.h"

// ---------------------------------------------------------------------------
// Add your hooks here.  Injected before the main thread starts, so hota_me.dll
// is already mapped — use GetModuleHandleA("hota_me.dll") to resolve addresses.
// Do NOT call LoadLibrary or create windows here (still under loader lock).
// ---------------------------------------------------------------------------
static void InitPlugin()
{
    InstallGarrisonHooks();
}

// ---------------------------------------------------------------------------
BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID)
{
    if (fdwReason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(hModule);
        InitPlugin();
    }
    return TRUE;
}
