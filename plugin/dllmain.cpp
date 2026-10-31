#include "pch.h"
#include "logger.h"
#include "minimap.h"
#include "obstacle_def_images.h"
#include "obstacle_diagnostics.h"
#include "obstacle_filter.h"
#include "obstacle_variants.h"
#include "plugin_api.h"
#include "target_profile.h"
#include "../shared/localization.h"

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
    const auto* hotaProfile = FindLoadedHotaModuleTargetProfile(error);
    if (!hotaProfile)
    {
        LogFormat("Target module validation failed: %s; no hooks were installed.",
            error.c_str());
        s_initializeResult = kPluginInitializeUnsupportedTarget;
        InterlockedExchange(&s_initializeState, 3);
        return s_initializeResult;
    }
    LogFormat("Selected HotA module profile: %s", hotaProfile->name);
    char editorPath[MAX_PATH]{};
    GetModuleFileNameA(nullptr, editorPath, MAX_PATH);
    std::string editorDirectory = editorPath;
    editorDirectory = editorDirectory.substr(0, editorDirectory.find_last_of("\\/"));
    SetUiLanguage(LanguageForEditorDirectory(editorDirectory.c_str()));

    if (!InstallMinimapColorHook(*profile, error))
    {
        LogFormat("Minimap color installation failed: %s", error.c_str());
        s_initializeResult = kPluginInitializeHookFailed;
        InterlockedExchange(&s_initializeState, 3);
        return s_initializeResult;
    }

    if (ObstacleFilterEnabled() || ObstacleVariantsEnabled())
    {
        LogMessage("Obstacle filter hook requested by obstacle filter or variant settings.");
        if (!InstallObstacleFilterHooks(*hotaProfile, error))
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

    if (ObstacleVariantsEnabled())
    {
        if (!InstallDuneFillHook(*profile, error))
        {
            LogFormat("Dune fill installation failed: %s", error.c_str());
            s_initializeResult = kPluginInitializeHookFailed;
            InterlockedExchange(&s_initializeState, 3);
            return s_initializeResult;
        }
        LogMessage("Obstacle variant UI requested.");
        if (!InitializeObstacleDefImages(*profile, error))
        {
            LogFormat("Obstacle DEF image initialization failed: %s", error.c_str());
            s_initializeResult = kPluginInitializeHookFailed;
            InterlockedExchange(&s_initializeState, 3);
            return s_initializeResult;
        }
        if (!StartObstacleVariantUi(error))
        {
            LogFormat("Obstacle variant UI startup failed: %s", error.c_str());
            s_initializeResult = kPluginInitializeHookFailed;
            InterlockedExchange(&s_initializeState, 3);
            return s_initializeResult;
        }
    }
    else
    {
        LogMessage("Obstacle variant UI disabled.");
    }

    if (ObstacleDiagnosticsEnabled())
    {
        LogMessage("Obstacle diagnostics requested by HOTA_MAPED_PLUGIN_DIAGNOSTICS.");
        if (!InstallObstacleDiagnosticsHooks(*hotaProfile, error))
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
