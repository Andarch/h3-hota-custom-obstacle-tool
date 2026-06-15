#pragma once

constexpr char kPluginInitializeExport[] = "HotaMapEdPluginInitialize";

enum PluginInitializeResult : unsigned long
{
    kPluginInitializeSuccess = 0,
    kPluginInitializeHashFailed = 1,
    kPluginInitializeUnsupportedTarget = 2,
    kPluginInitializeHookFailed = 3,
    kPluginInitializeAlreadyFailed = 4,
    kPluginInitializeBusy = 5,
};
