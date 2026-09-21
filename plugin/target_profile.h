#pragma once

#include "pch.h"

#include <string>

struct HookSite
{
    const char* name;
    DWORD address;
    BYTE expectedBytes[9];
    SIZE_T expectedLength;
};

struct ObjectLimitTargetProfile
{
    HookSite createCheck;
    HookSite predicateCheck;
    HookSite placeCheck;
    DWORD createAllow;
    DWORD createDeny;
    DWORD predicateAllow;
    DWORD predicateDeny;
    DWORD placeAllow;
    DWORD placeDeny;
};

struct EditorResourceTargetProfile
{
    DWORD findArchive;
    DWORD readSelectedEntry;
};

struct TargetProfile
{
    const char* name;
    const char* executableSha256;
    ObjectLimitTargetProfile objectLimits;
    EditorResourceTargetProfile editorResources;
    HookSite minimapObstacleColor;
};

constexpr DWORD kMaxRandTrnRowCount = 201;

struct HotaModuleTargetProfile
{
    const char* name;
    const char* sha256;
    DWORD obstacleWeightPredicateRva;
    DWORD parserEntryRva;
    DWORD parserStoreRva;
    DWORD parserFallbackRva;
    BYTE parserFallbackExpected[8];
    DWORD randTrnRowCount;
    DWORD glacierRandTrnRow;
    DWORD terrainAccumulatorRva;
    DWORD terrainViewTableRva;
    DWORD mountainOrderCheckRva;
};

bool ComputeCurrentExecutableSha256(char output[65]);
bool ComputeFileSha256(const char* path, char output[65]);
const TargetProfile* FindTargetProfile(const char* executableSha256);
const HotaModuleTargetProfile* FindHotaModuleTargetProfile(const char* sha256);
const HotaModuleTargetProfile* FindLoadedHotaModuleTargetProfile(std::string& error);
DWORD LegacyRandTrnRowIndex(const HotaModuleTargetProfile& profile, DWORD rowId);
