#pragma once

#include "pch.h"

enum class ObstacleThickness : BYTE
{
    Unknown = 0,
    Thin = 1,
    Thick = 2,
};

bool ObstacleThicknessTableAvailable();
ObstacleThickness GetObstacleThicknessForRandTrnRow(DWORD rowId);
void __stdcall RegisterObstacleTemplateDefName(DWORD objectTemplate, DWORD editorString);
ObstacleThickness GetObstacleThicknessForObjectTemplate(DWORD objectTemplate);
const char* ObstacleThicknessName(ObstacleThickness thickness);
