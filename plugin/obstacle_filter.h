#pragma once

#include <string>

enum class ObstacleGenerationFilterMode
{
    None = 0,
    ThickOnly = 1,
    ThinOnly = 2,
    ButtonSelection = 3,
};

bool ObstacleFilterEnabled();
bool CustomObstacleModeEnabled();
void SetCustomObstacleMode(bool enabled);
struct HotaModuleTargetProfile;
bool InstallObstacleFilterHooks(const HotaModuleTargetProfile& profile, std::string& error);
struct TargetProfile;
bool InstallDuneFillHook(const TargetProfile& profile, std::string& error);
void SetObstacleGenerationFilterMode(ObstacleGenerationFilterMode mode);
void SetObstacleButtonSelection(DWORD lowMask, DWORD highMask);
