#include "pch.h"
#include "logger.h"
#include "objlimits.h"
#include "obstacle_diagnostics.h"
#include "obstacle_filter.h"
#include "plugin_api.h"
#include "target_profile.h"

#include <string>

namespace
{
    HMODULE s_pluginModule = nullptr;
    volatile LONG s_initializeState = 0;
    DWORD s_initializeResult = kPluginInitializeAlreadyFailed;
}

#pragma comment(linker, "/EXPORT:HotaMapEdPluginInitialize=_HotaMapEdPluginInitialize@4")

extern "C" DWORD WINAPI HotaMapEdPluginInitialize(LPVOID)
{
    const LONG previousState = InterlockedCompareExchange(&s_initializeState, 1, 0);
    if (previousState == 1)
        return kPluginInitializeBusy;
    if (previousState == 2)
        return kPluginInitializeSuccess;
    if (previousState == 3)
        return s_initializeResult;

    LoggerInitialize(s_pluginModule);
    LogMessage("Starting explicit plugin initialization.");

    char executableHash[65] = {};
    if (!ComputeCurrentExecutableSha256(executableHash))
    {
        LogMessage("Failed to compute the target executable SHA-256.");
        s_initializeResult = kPluginInitializeHashFailed;
        InterlockedExchange(&s_initializeState, 3);
        return s_initializeResult;
    }
    LogFormat("Target executable SHA-256: %s", executableHash);

    const TargetProfile* profile = FindTargetProfile(executableHash);
    if (!profile)
    {
        LogMessage("Unsupported target executable; no hooks were installed.");
        s_initializeResult = kPluginInitializeUnsupportedTarget;
        InterlockedExchange(&s_initializeState, 3);
        return s_initializeResult;
    }
    LogFormat("Selected target profile: %s", profile->name);

    std::string error;
    if (!InstallObjectLimitHooks(*profile, error))
    {
        LogFormat("Garrison feature installation failed: %s", error.c_str());
        s_initializeResult = kPluginInitializeHookFailed;
        InterlockedExchange(&s_initializeState, 3);
        return s_initializeResult;
    }

    LogMessage("Garrison feature installed successfully.");

    if (ObstacleFilterEnabled())
    {
        LogMessage("Obstacle filter requested by obstacle filter environment settings.");
        if (!InstallObstacleFilterHooks(error))
        {
            LogFormat("Obstacle filter installation failed: %s", error.c_str());
            s_initializeResult = kPluginInitializeHookFailed;
            InterlockedExchange(&s_initializeState, 3);
            return s_initializeResult;
        }
    }
    else
    {
        LogMessage("Obstacle filter disabled.");
    }

    if (ObstacleDiagnosticsEnabled())
    {
        LogMessage("Obstacle diagnostics requested by HOTA_MAPED_PLUGIN_DIAGNOSTICS.");
        if (!InstallObstacleDiagnosticsHooks(error))
        {
            LogFormat("Obstacle diagnostics installation failed: %s", error.c_str());
            s_initializeResult = kPluginInitializeHookFailed;
            InterlockedExchange(&s_initializeState, 3);
            return s_initializeResult;
        }
    }
    else
    {
        LogMessage("Obstacle diagnostics disabled.");
    }

    s_initializeResult = kPluginInitializeSuccess;
    InterlockedExchange(&s_initializeState, 2);
    return s_initializeResult;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID)
{
    if (fdwReason == DLL_PROCESS_ATTACH)
    {
        s_pluginModule = hModule;
        DisableThreadLibraryCalls(hModule);
    }
    return TRUE;
}
