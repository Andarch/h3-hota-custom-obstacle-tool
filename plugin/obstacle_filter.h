#pragma once

#include <string>

enum class ObstacleGenerationFilterMode
{
    None = 0,
    ThickOnly = 1,
    ThinOnly = 2,
};

bool ObstacleFilterEnabled();
bool InstallObstacleFilterHooks(std::string& error);
void SetObstacleGenerationFilterMode(ObstacleGenerationFilterMode mode);
