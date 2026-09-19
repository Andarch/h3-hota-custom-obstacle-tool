#pragma once

#include <string>

bool ObstacleDiagnosticsEnabled();
struct HotaModuleTargetProfile;
bool InstallObstacleDiagnosticsHooks(const HotaModuleTargetProfile& profile, std::string& error);
