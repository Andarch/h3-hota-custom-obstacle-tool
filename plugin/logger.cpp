#include "pch.h"
#include "logger.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace
{
    HANDLE s_logFile = INVALID_HANDLE_VALUE;

    void WriteRaw(const char* text)
    {
        if (s_logFile == INVALID_HANDLE_VALUE || !text)
            return;

        DWORD written = 0;
        WriteFile(s_logFile, text, static_cast<DWORD>(strlen(text)), &written, nullptr);
        FlushFileBuffers(s_logFile);
    }
}

bool LoggerInitialize(HMODULE pluginModule)
{
    char path[MAX_PATH] = {};
    const DWORD length = GetModuleFileNameA(pluginModule, path, MAX_PATH);
    if (length == 0 || length == MAX_PATH)
        return false;

    char* fileName = strrchr(path, '\\');
    if (!fileName)
        return false;

    strcpy_s(fileName + 1, MAX_PATH - static_cast<size_t>(fileName + 1 - path),
        "HotaMapEdPlugin.log");

    s_logFile = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, nullptr,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (s_logFile == INVALID_HANDLE_VALUE)
        return false;

    SYSTEMTIME time = {};
    GetLocalTime(&time);

    char header[128] = {};
    sprintf_s(header, "HotaMapEdPlugin initialization %04u-%02u-%02u %02u:%02u:%02u\r\n",
        time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond);
    WriteRaw(header);
    return true;
}

void LogMessage(const char* message)
{
    WriteRaw(message);
    WriteRaw("\r\n");
}

void LogFormat(const char* format, ...)
{
    char buffer[4096] = {};

    va_list args;
    va_start(args, format);
    _vsnprintf_s(buffer, sizeof(buffer), _TRUNCATE, format, args);
    va_end(args);

    LogMessage(buffer);
}
