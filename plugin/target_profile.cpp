#include "pch.h"
#include "target_profile.h"

#include <bcrypt.h>
#include <cstring>
#include <vector>

#pragma comment(lib, "bcrypt.lib")

namespace
{
    constexpr TargetProfile kSupportedProfile =
    {
        "HotA map editor 1.8.0",
        "CCF1E8424D913AA409ECC168F87FB20EDD64C3A56D412F2878065FC261BC6113",
        {
            { "garrison create check", 0x004212A4,
                { 0x3B, 0x48, 0x08, 0x72, 0x28 }, 5 },
            { "garrison UI predicate", 0x004232D4,
                { 0x3B, 0x46, 0x08, 0x0F, 0x83, 0xA8, 0x01, 0x00, 0x00 }, 9 },
            { "garrison place check", 0x00426AFA,
                { 0x3B, 0x48, 0x08, 0x72, 0x28 }, 5 },
            0x004212D1,
            0x004212A9,
            0x004232DD,
            0x00423485,
            0x00426B27,
            0x00426AFF,
        },
        {
            0x004DB240,
            0x004E4960,
        },
    };

    bool HashFile(const char* path, BYTE output[32])
    {
        HANDLE file = CreateFileA(path, GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE)
            return false;

        BCRYPT_ALG_HANDLE algorithm = nullptr;
        BCRYPT_HASH_HANDLE hash = nullptr;
        std::vector<BYTE> hashObject;
        bool success = false;

        DWORD objectLength = 0;
        DWORD resultLength = 0;
        if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0 ||
            BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH,
                reinterpret_cast<BYTE*>(&objectLength), sizeof(objectLength),
                &resultLength, 0) < 0)
        {
            CloseHandle(file);
            if (algorithm)
                BCryptCloseAlgorithmProvider(algorithm, 0);
            return false;
        }

        hashObject.resize(objectLength);
        if (BCryptCreateHash(algorithm, &hash, hashObject.data(), objectLength,
                nullptr, 0, 0) >= 0)
        {
            BYTE buffer[64 * 1024];
            for (;;)
            {
                DWORD bytesRead = 0;
                if (!ReadFile(file, buffer, sizeof(buffer), &bytesRead, nullptr))
                    break;

                if (bytesRead == 0)
                {
                    success = BCryptFinishHash(hash, output, 32, 0) >= 0;
                    break;
                }

                if (BCryptHashData(hash, buffer, bytesRead, 0) < 0)
                    break;
            }
        }

        if (hash)
            BCryptDestroyHash(hash);
        BCryptCloseAlgorithmProvider(algorithm, 0);
        CloseHandle(file);
        return success;
    }
}

bool ComputeCurrentExecutableSha256(char output[65])
{
    char path[MAX_PATH] = {};
    const DWORD length = GetModuleFileNameA(nullptr, path, MAX_PATH);
    if (length == 0 || length == MAX_PATH)
        return false;

    return ComputeFileSha256(path, output);
}

bool ComputeFileSha256(const char* path, char output[65])
{
    if (!path || !output)
        return false;

    BYTE hash[32] = {};
    if (!HashFile(path, hash))
        return false;

    constexpr char kHex[] = "0123456789ABCDEF";
    for (size_t i = 0; i < sizeof(hash); ++i)
    {
        output[i * 2] = kHex[hash[i] >> 4];
        output[i * 2 + 1] = kHex[hash[i] & 0x0F];
    }
    output[64] = '\0';
    return true;
}

const TargetProfile* FindTargetProfile(const char* executableSha256)
{
    if (executableSha256 &&
        strcmp(executableSha256, kSupportedProfile.executableSha256) == 0)
    {
        return &kSupportedProfile;
    }

    return nullptr;
}
