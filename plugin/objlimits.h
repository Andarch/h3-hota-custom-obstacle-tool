#pragma once

#include <string>

struct TargetProfile;

bool InstallObjectLimitHooks(const TargetProfile& profile, std::string& error);
