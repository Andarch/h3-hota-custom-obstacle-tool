#pragma once

#include "pch.h"

inline bool IsSupportedTerrainMapDimension(DWORD value)
{
    return value >= 36 && value <= 252 && value % 36 == 0;
}

// The native terrain grid stores a rectangular vector of 6x6-tile chunks.
inline bool TryComputeTerrainMapDimensions(
    DWORD chunkColumns,
    DWORD chunksBegin,
    DWORD chunksEnd,
    unsigned int& width,
    unsigned int& height)
{
    width = 0;
    height = 0;
    constexpr DWORD kChunkExtent = 6;
    if (!chunkColumns || chunkColumns > 252 / kChunkExtent ||
        !chunksBegin || chunksEnd <= chunksBegin ||
        (chunksEnd - chunksBegin) % sizeof(DWORD) != 0)
        return false;

    const DWORD chunkCount = (chunksEnd - chunksBegin) / sizeof(DWORD);
    if (chunkCount % chunkColumns != 0)
        return false;
    const DWORD chunkRows = chunkCount / chunkColumns;
    if (chunkRows > 252 / kChunkExtent ||
        !IsSupportedTerrainMapDimension(chunkColumns * kChunkExtent) ||
        !IsSupportedTerrainMapDimension(chunkRows * kChunkExtent))
        return false;

    width = chunkColumns * kChunkExtent;
    height = chunkRows * kChunkExtent;
    return true;
}
