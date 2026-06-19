#pragma once

#include "pch.h"

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

struct TargetProfile
{
    const char* name;
    const char* executableSha256;
    ObjectLimitTargetProfile objectLimits;
};

bool ComputeCurrentExecutableSha256(char output[65]);
bool ComputeFileSha256(const char* path, char output[65]);
const TargetProfile* FindTargetProfile(const char* executableSha256);
