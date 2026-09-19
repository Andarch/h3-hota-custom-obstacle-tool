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
struct HotaModuleTargetProfile;
bool InstallObstacleFilterHooks(const HotaModuleTargetProfile& profile, std::string& error);
void SetObstacleGenerationFilterMode(ObstacleGenerationFilterMode mode);
void SetObstacleButtonSelection(DWORD lowMask, DWORD highMask);
