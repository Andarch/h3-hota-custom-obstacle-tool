#pragma once

#include <string>

enum class ObstacleGenerationFilterMode
{
    None = 0,
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
void SetObstacleButtonSelection(DWORD lowMask, DWORD highMask, DWORD extendedMask, DWORD upperMask);

constexpr DWORD kAllObstacleTerrainMask = 0xDFFu; // Twelve terrains, excluding Rock/Void (9).
bool AllObstacleTerrainsEnabled();
void SetAllObstacleTerrains(bool enabled);
DWORD EffectiveObstacleTerrainMask(DWORD workbookMask);
