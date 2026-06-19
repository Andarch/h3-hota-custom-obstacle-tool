#include "pch.h"
#include "obstacle_classification.h"

namespace
{
    // Generated from the user-curated obstacles.xlsx after a Thickness column is
    // classified. Zero means unclassified and is rejected by thick/thin modes.
    constexpr BYTE kObstacleThicknessByRandTrnRow[200] = {};
    constexpr DWORD kObstacleThicknessRowCount =
        sizeof(kObstacleThicknessByRandTrnRow) / sizeof(kObstacleThicknessByRandTrnRow[0]);
    constexpr DWORD kClassifiedObstacleRows = 0;
}

bool ObstacleThicknessTableAvailable()
{
    return kClassifiedObstacleRows > 0;
}

ObstacleThickness GetObstacleThicknessForRandTrnRow(DWORD rowId)
{
    if (rowId >= kObstacleThicknessRowCount)
        return ObstacleThickness::Unknown;

    return static_cast<ObstacleThickness>(kObstacleThicknessByRandTrnRow[rowId]);
}

const char* ObstacleThicknessName(ObstacleThickness thickness)
{
    switch (thickness)
    {
    case ObstacleThickness::Thin:
        return "thin";
    case ObstacleThickness::Thick:
        return "thick";
    default:
        return "unknown";
    }
}
