#pragma once

struct ObstacleIconData
{
    int width;
    int height;
    const unsigned char* rgba;
};

const ObstacleIconData* ObstacleIconForButton(int buttonIndex, bool enabled);
