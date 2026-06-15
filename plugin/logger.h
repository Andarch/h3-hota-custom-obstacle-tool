#pragma once

#include "pch.h"

bool LoggerInitialize(HMODULE pluginModule);
void LogMessage(const char* message);
void LogFormat(const char* format, ...);
