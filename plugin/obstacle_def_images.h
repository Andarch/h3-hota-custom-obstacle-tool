#pragma once

#include "pch.h"

#include <string>
#include <vector>

struct TargetProfile;

struct ObstacleRenderedImage
{
    int width = 0;
    int height = 0;
    std::vector<BYTE> bgra;
};

bool InitializeObstacleDefImages(const TargetProfile& profile, std::string& error);
const ObstacleRenderedImage* ObstacleDefIconForButton(int buttonIndex, bool enabled);
const ObstacleRenderedImage* ObstacleDefPreviewForButton(int buttonIndex);
