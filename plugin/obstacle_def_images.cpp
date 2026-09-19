#include "pch.h"
#include "obstacle_def_images.h"

#include "logger.h"
#include "obstacle_button_data.h"
#include "target_profile.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include <new>

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

namespace
{
    constexpr DWORD kAdventureObjectDefType = 0x43;
    constexpr DWORD kCompressionFormat3 = 3;
    constexpr size_t kDefHeaderSize = 16 + 256 * 3;
    constexpr size_t kGroupHeaderSize = 16;
    constexpr size_t kFrameNameSize = 13;
    constexpr size_t kFrameHeaderSize = 32;
    constexpr size_t kFirstGroupEnd = kDefHeaderSize + kGroupHeaderSize;
    constexpr DWORD kMaximumDefSize = 16 * 1024 * 1024;
    constexpr DWORD kMaximumFrameExtent = 4096;
    constexpr size_t kMaximumFramePixels = 16 * 1024 * 1024;
    constexpr int kIconExtent = 30;
    constexpr int kPreviewExtent = 160;

    using FindArchiveFn = void* (__cdecl*)(const char*);
    using ReadSelectedEntryFn = int(__thiscall*)(void*, void*, DWORD);

    struct NativeResourceFunctions
    {
        FindArchiveFn findArchive = nullptr;
        ReadSelectedEntryFn readSelectedEntry = nullptr;
    };

    struct SourceImageCache
    {
        bool attempted = false;
        bool loaded = false;
        ObstacleRenderedImage image;
    };

    struct ButtonImageCache
    {
        bool iconAttempted = false;
        bool iconLoaded = false;
        ObstacleRenderedImage enabledIcon;
        ObstacleRenderedImage disabledIcon;
        bool previewAttempted = false;
        bool previewLoaded = false;
        ObstacleRenderedImage preview;
    };

    struct PixelBounds
    {
        int left = 0;
        int top = 0;
        int right = 0;
        int bottom = 0;
    };

    NativeResourceFunctions s_nativeResources;
    bool s_initialized = false;
    std::map<std::string, SourceImageCache> s_sourceImages;
    ButtonImageCache s_buttonImages[kObstacleButtonCount] = {};

    bool IsExecutableAddress(DWORD address)
    {
        MEMORY_BASIC_INFORMATION memory = {};
        if (!address || VirtualQuery(reinterpret_cast<const void*>(address), &memory, sizeof(memory)) == 0)
            return false;
        if (memory.State != MEM_COMMIT || (memory.Protect & (PAGE_GUARD | PAGE_NOACCESS)))
            return false;
        const DWORD protection = memory.Protect & 0xFF;
        return protection == PAGE_EXECUTE ||
            protection == PAGE_EXECUTE_READ ||
            protection == PAGE_EXECUTE_READWRITE ||
            protection == PAGE_EXECUTE_WRITECOPY;
    }

    bool RegionAvailable(size_t offset, size_t length, size_t limit);
    bool ReadU32(const std::vector<BYTE>& bytes, size_t offset, DWORD& value);
    bool TryMultiply(size_t left, size_t right, size_t& result);

    bool TrySelectEntry(const char* name, void*& archive)
    {
        archive = nullptr;
        __try
        {
            archive = s_nativeResources.findArchive(name);
            return archive != nullptr;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            archive = nullptr;
            return false;
        }
    }

    bool TryReadEntry(void* archive, BYTE* destination, DWORD size)
    {
        __try
        {
            return s_nativeResources.readSelectedEntry(archive, destination, size) == 0;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return false;
        }
    }

    bool ReadSelectedBytesTo(
        void* archive,
        std::vector<BYTE>& bytes,
        size_t targetSize,
        std::string& error)
    {
        if (targetSize < bytes.size() || targetSize > kMaximumDefSize)
        {
            error = "DEF first-frame extent is outside the supported range";
            return false;
        }

        const size_t oldSize = bytes.size();
        if (targetSize == oldSize)
            return true;
        try
        {
            bytes.resize(targetSize);
        }
        catch (const std::bad_alloc&)
        {
            error = "could not allocate the DEF resource buffer";
            return false;
        }
        const DWORD chunkSize = static_cast<DWORD>(targetSize - oldSize);
        if (!TryReadEntry(archive, bytes.data() + oldSize, chunkSize))
        {
            bytes.resize(oldSize);
            error = "editor resource read failed";
            return false;
        }
        return true;
    }

    bool LoadDefBytes(const char* name, std::vector<BYTE>& bytes, std::string& error)
    {
        if (!s_initialized || !name)
        {
            error = "DEF image service is not initialized";
            return false;
        }

        void* archive = nullptr;
        if (!TrySelectEntry(name, archive))
        {
            error = "editor resource lookup failed";
            return false;
        }

        // HotA's patched reader maintains an entry cursor, so read only enough
        // data to discover and then load the first frame.
        bytes.clear();
        if (!ReadSelectedBytesTo(archive, bytes, kFirstGroupEnd, error))
            return false;

        DWORD type = 0;
        DWORD groupCount = 0;
        DWORD frameCount = 0;
        if (!ReadU32(bytes, 0, type) ||
            !ReadU32(bytes, 12, groupCount) ||
            !ReadU32(bytes, kDefHeaderSize + 4, frameCount) ||
            type != kAdventureObjectDefType || !groupCount || !frameCount)
        {
            error = "resource is not an adventure-object DEF with a first frame";
            return false;
        }

        size_t frameNamesSize = 0;
        size_t frameOffsetsSize = 0;
        if (!TryMultiply(frameCount, kFrameNameSize, frameNamesSize) ||
            !TryMultiply(frameCount, sizeof(DWORD), frameOffsetsSize) ||
            frameNamesSize > kMaximumDefSize - kFirstGroupEnd ||
            frameOffsetsSize > kMaximumDefSize - kFirstGroupEnd - frameNamesSize)
        {
            error = "DEF frame table size overflow";
            return false;
        }
        const size_t frameOffsetsOffset = kFirstGroupEnd + frameNamesSize;
        const size_t frameTableEnd = frameOffsetsOffset + frameOffsetsSize;
        if (!ReadSelectedBytesTo(archive, bytes, frameTableEnd, error))
            return false;

        DWORD frameOffsetValue = 0;
        if (!ReadU32(bytes, frameOffsetsOffset, frameOffsetValue) ||
            frameOffsetValue < frameTableEnd ||
            frameOffsetValue > kMaximumDefSize - kFrameHeaderSize)
        {
            error = "DEF first-frame offset is invalid";
            return false;
        }
        const size_t frameOffset = frameOffsetValue;
        const size_t baseOffset = frameOffset + kFrameHeaderSize;
        if (!ReadSelectedBytesTo(archive, bytes, baseOffset, error))
            return false;

        DWORD frameSize = 0;
        if (!ReadU32(bytes, frameOffset, frameSize) ||
            !frameSize || frameSize > kMaximumDefSize - baseOffset)
        {
            error = "DEF first-frame payload size is invalid";
            return false;
        }
        return ReadSelectedBytesTo(archive, bytes, baseOffset + frameSize, error);
    }

    bool RegionAvailable(size_t offset, size_t length, size_t limit)
    {
        return offset <= limit && length <= limit - offset;
    }

    bool ReadU16(const std::vector<BYTE>& bytes, size_t offset, WORD& value)
    {
        if (!RegionAvailable(offset, sizeof(WORD), bytes.size()))
            return false;
        value = static_cast<WORD>(bytes[offset]) |
            static_cast<WORD>(bytes[offset + 1] << 8);
        return true;
    }

    bool ReadU32(const std::vector<BYTE>& bytes, size_t offset, DWORD& value)
    {
        if (!RegionAvailable(offset, sizeof(DWORD), bytes.size()))
            return false;
        value = static_cast<DWORD>(bytes[offset]) |
            (static_cast<DWORD>(bytes[offset + 1]) << 8) |
            (static_cast<DWORD>(bytes[offset + 2]) << 16) |
            (static_cast<DWORD>(bytes[offset + 3]) << 24);
        return true;
    }

    bool TryMultiply(size_t left, size_t right, size_t& result)
    {
        if (left && right > static_cast<size_t>(-1) / left)
            return false;
        result = left * right;
        return true;
    }

    bool PaletteColorSimilar(
        BYTE red,
        BYTE green,
        BYTE blue,
        BYTE expectedRed,
        BYTE expectedGreen,
        BYTE expectedBlue)
    {
        constexpr int kThreshold = 8;
        return std::abs(static_cast<int>(red) - expectedRed) < kThreshold &&
            std::abs(static_cast<int>(green) - expectedGreen) < kThreshold &&
            std::abs(static_cast<int>(blue) - expectedBlue) < kThreshold;
    }

    void WriteDefPixel(
        ObstacleRenderedImage& image,
        size_t pixelOffset,
        BYTE paletteIndex,
        const std::vector<BYTE>& bytes)
    {
        BYTE blue = 0;
        BYTE green = 0;
        BYTE red = 0;
        BYTE alpha = 0;
        if (paletteIndex != 0)
        {
            const size_t paletteOffset = 16 + static_cast<size_t>(paletteIndex) * 3;
            red = bytes[paletteOffset];
            green = bytes[paletteOffset + 1];
            blue = bytes[paletteOffset + 2];
            alpha = 255;

            if (paletteIndex == 1 ||
                (paletteIndex == 2 && PaletteColorSimilar(red, green, blue, 255, 100, 255)))
            {
                blue = green = red = 0;
                alpha = 64;
            }
            else if (paletteIndex == 4 ||
                (paletteIndex == 3 && PaletteColorSimilar(red, green, blue, 255, 50, 255)))
            {
                blue = green = red = 0;
                alpha = 128;
            }
        }

        image.bgra[pixelOffset] = blue;
        image.bgra[pixelOffset + 1] = green;
        image.bgra[pixelOffset + 2] = red;
        image.bgra[pixelOffset + 3] = alpha;
    }

    bool DecodeFirstDefFrame(
        const std::vector<BYTE>& bytes,
        ObstacleRenderedImage& image,
        std::string& error)
    {
        DWORD type = 0;
        DWORD groupCount = 0;
        if (!ReadU32(bytes, 0, type) || !ReadU32(bytes, 12, groupCount) ||
            type != kAdventureObjectDefType || groupCount == 0)
        {
            error = "resource is not an adventure-object DEF with at least one group";
            return false;
        }

        DWORD frameCount = 0;
        if (!ReadU32(bytes, kDefHeaderSize + 4, frameCount) || frameCount == 0)
        {
            error = "first DEF group has no frames";
            return false;
        }

        size_t frameNamesSize = 0;
        size_t frameOffsetsSize = 0;
        if (!TryMultiply(frameCount, kFrameNameSize, frameNamesSize) ||
            !TryMultiply(frameCount, sizeof(DWORD), frameOffsetsSize))
        {
            error = "DEF frame table size overflow";
            return false;
        }
        const size_t frameOffsetsOffset = kDefHeaderSize + kGroupHeaderSize + frameNamesSize;
        if (!RegionAvailable(frameOffsetsOffset, frameOffsetsSize, bytes.size()))
        {
            error = "DEF frame table is truncated";
            return false;
        }

        DWORD frameOffsetValue = 0;
        if (!ReadU32(bytes, frameOffsetsOffset, frameOffsetValue))
        {
            error = "DEF first-frame offset is missing";
            return false;
        }
        const size_t frameOffset = frameOffsetValue;
        if (!RegionAvailable(frameOffset, kFrameHeaderSize, bytes.size()))
        {
            error = "DEF first-frame header is truncated";
            return false;
        }

        DWORD frameSize = 0;
        DWORD format = 0;
        DWORD fullWidth = 0;
        DWORD fullHeight = 0;
        DWORD width = 0;
        DWORD height = 0;
        DWORD leftMargin = 0;
        DWORD topMargin = 0;
        if (!ReadU32(bytes, frameOffset, frameSize) ||
            !ReadU32(bytes, frameOffset + 4, format) ||
            !ReadU32(bytes, frameOffset + 8, fullWidth) ||
            !ReadU32(bytes, frameOffset + 12, fullHeight) ||
            !ReadU32(bytes, frameOffset + 16, width) ||
            !ReadU32(bytes, frameOffset + 20, height) ||
            !ReadU32(bytes, frameOffset + 24, leftMargin) ||
            !ReadU32(bytes, frameOffset + 28, topMargin))
        {
            error = "DEF first-frame header is truncated";
            return false;
        }
        if (format != kCompressionFormat3)
        {
            error = "DEF first frame does not use compression format 3";
            return false;
        }
        if (!width || !height || !fullWidth || !fullHeight ||
            width % 32 != 0 ||
            fullWidth > kMaximumFrameExtent || fullHeight > kMaximumFrameExtent ||
            width > fullWidth || height > fullHeight ||
            leftMargin > fullWidth - width || topMargin > fullHeight - height)
        {
            error = "DEF first-frame dimensions or margins are invalid";
            return false;
        }

        size_t framePixels = 0;
        size_t imageBytes = 0;
        if (!TryMultiply(fullWidth, fullHeight, framePixels) ||
            framePixels > kMaximumFramePixels ||
            !TryMultiply(framePixels, 4, imageBytes))
        {
            error = "DEF first-frame canvas is too large";
            return false;
        }
        const size_t baseOffset = frameOffset + kFrameHeaderSize;
        if (!frameSize || !RegionAvailable(baseOffset, frameSize, bytes.size()))
        {
            error = "DEF first-frame payload is truncated";
            return false;
        }
        const size_t frameEnd = baseOffset + frameSize;
        const size_t blocksPerRow = width / 32;
        size_t rowTableEntries = 0;
        size_t rowTableSize = 0;
        if (!TryMultiply(height, blocksPerRow, rowTableEntries) ||
            !TryMultiply(rowTableEntries, sizeof(WORD), rowTableSize) ||
            !RegionAvailable(baseOffset, rowTableSize, frameEnd))
        {
            error = "DEF compression-3 row table is truncated";
            return false;
        }

        image.width = static_cast<int>(fullWidth);
        image.height = static_cast<int>(fullHeight);
        try
        {
            image.bgra.assign(imageBytes, 0);
        }
        catch (const std::bad_alloc&)
        {
            error = "could not allocate the decoded DEF image";
            return false;
        }

        for (DWORD row = 0; row < height; ++row)
        {
            WORD relativeOffset = 0;
            const size_t rowOffsetEntry =
                baseOffset + static_cast<size_t>(row) * blocksPerRow * sizeof(WORD);
            if (!ReadU16(bytes, rowOffsetEntry, relativeOffset))
            {
                error = "DEF compression-3 row offset is truncated";
                return false;
            }
            size_t cursor = baseOffset + relativeOffset;
            DWORD column = 0;
            while (column < width)
            {
                if (!RegionAvailable(cursor, 1, frameEnd))
                {
                    char detail[192] = {};
                    sprintf_s(
                        detail,
                        "DEF compression-3 segment header is truncated (row=%lu column=%lu cursor=%zu frameEnd=%zu rowOffset=%u)",
                        row,
                        column,
                        cursor,
                        frameEnd,
                        relativeOffset);
                    error = detail;
                    return false;
                }
                const BYTE segment = bytes[cursor++];
                const BYTE code = segment >> 5;
                const DWORD length = (segment & 31) + 1;
                if (length > width - column)
                {
                    error = "DEF compression-3 segment exceeds its row";
                    return false;
                }
                if (code == 7 && !RegionAvailable(cursor, length, frameEnd))
                {
                    char detail[224] = {};
                    sprintf_s(
                        detail,
                        "DEF compression-3 raw segment is truncated (row=%lu column=%lu cursor=%zu length=%lu frameEnd=%zu rowOffset=%u)",
                        row,
                        column,
                        cursor,
                        length,
                        frameEnd,
                        relativeOffset);
                    error = detail;
                    return false;
                }

                for (DWORD index = 0; index < length; ++index)
                {
                    const BYTE paletteIndex = code == 7 ? bytes[cursor + index] : code;
                    const size_t destinationPixel =
                        (static_cast<size_t>(topMargin + row) * fullWidth +
                            leftMargin + column + index) * 4;
                    WriteDefPixel(image, destinationPixel, paletteIndex, bytes);
                }
                if (code == 7)
                    cursor += length;
                column += length;
            }
        }
        return true;
    }

    const ObstacleRenderedImage* SourceImageForDef(const char* defName)
    {
        if (!defName || !*defName)
            return nullptr;

        SourceImageCache& cache = s_sourceImages[defName];
        if (!cache.attempted)
        {
            cache.attempted = true;
            std::vector<BYTE> bytes;
            std::string error;
            cache.loaded = LoadDefBytes(defName, bytes, error) &&
                DecodeFirstDefFrame(bytes, cache.image, error);
            if (!cache.loaded)
            {
                LogFormat(
                    "Obstacle variants: could not render %s from editor resources: %s.",
                    defName,
                    error.c_str());
            }
        }
        return cache.loaded ? &cache.image : nullptr;
    }

    bool FindVisibleBounds(const ObstacleRenderedImage& image, PixelBounds& bounds)
    {
        if (image.width <= 0 || image.height <= 0 ||
            image.bgra.size() != static_cast<size_t>(image.width) * image.height * 4)
            return false;

        int left = image.width;
        int top = image.height;
        int right = 0;
        int bottom = 0;
        for (int y = 0; y < image.height; ++y)
        {
            for (int x = 0; x < image.width; ++x)
            {
                const size_t offset = (static_cast<size_t>(y) * image.width + x) * 4;
                if (image.bgra[offset + 3] == 0)
                    continue;
                left = std::min(left, x);
                top = std::min(top, y);
                right = std::max(right, x + 1);
                bottom = std::max(bottom, y + 1);
            }
        }
        if (left >= right || top >= bottom)
            return false;
        bounds = { left, top, right, bottom };
        return true;
    }

    void ResampleInto(
        const ObstacleRenderedImage& source,
        const PixelBounds& sourceBounds,
        ObstacleRenderedImage& destination,
        const RECT& target,
        bool allowEnlargement)
    {
        const int targetWidth = target.right - target.left;
        const int targetHeight = target.bottom - target.top;
        const int sourceWidth = sourceBounds.right - sourceBounds.left;
        const int sourceHeight = sourceBounds.bottom - sourceBounds.top;
        if (targetWidth <= 0 || targetHeight <= 0 || sourceWidth <= 0 || sourceHeight <= 0)
            return;

        const double fitScale = std::min(
            static_cast<double>(targetWidth) / sourceWidth,
            static_cast<double>(targetHeight) / sourceHeight);
        const double scale = allowEnlargement ? fitScale : std::min(1.0, fitScale);
        const int outputWidth = std::max(1, static_cast<int>(std::floor(sourceWidth * scale + 0.5)));
        const int outputHeight = std::max(1, static_cast<int>(std::floor(sourceHeight * scale + 0.5)));
        const int outputLeft = target.left + (targetWidth - outputWidth) / 2;
        const int outputTop = target.top + (targetHeight - outputHeight) / 2;

        for (int outputY = 0; outputY < outputHeight; ++outputY)
        {
            const double sourceTop = sourceBounds.top +
                static_cast<double>(outputY) * sourceHeight / outputHeight;
            const double sourceBottom = sourceBounds.top +
                static_cast<double>(outputY + 1) * sourceHeight / outputHeight;
            const int firstSourceY = static_cast<int>(std::floor(sourceTop));
            const int lastSourceY = static_cast<int>(std::ceil(sourceBottom));

            for (int outputX = 0; outputX < outputWidth; ++outputX)
            {
                const double sourceLeft = sourceBounds.left +
                    static_cast<double>(outputX) * sourceWidth / outputWidth;
                const double sourceRight = sourceBounds.left +
                    static_cast<double>(outputX + 1) * sourceWidth / outputWidth;
                const int firstSourceX = static_cast<int>(std::floor(sourceLeft));
                const int lastSourceX = static_cast<int>(std::ceil(sourceRight));

                double totalWeight = 0.0;
                double alphaSum = 0.0;
                double blueSum = 0.0;
                double greenSum = 0.0;
                double redSum = 0.0;
                for (int sourceY = firstSourceY; sourceY < lastSourceY; ++sourceY)
                {
                    const double yWeight = std::max(
                        0.0,
                        std::min(sourceBottom, static_cast<double>(sourceY + 1)) -
                            std::max(sourceTop, static_cast<double>(sourceY)));
                    for (int sourceX = firstSourceX; sourceX < lastSourceX; ++sourceX)
                    {
                        const double xWeight = std::max(
                            0.0,
                            std::min(sourceRight, static_cast<double>(sourceX + 1)) -
                                std::max(sourceLeft, static_cast<double>(sourceX)));
                        const double weight = xWeight * yWeight;
                        if (weight <= 0.0)
                            continue;
                        const size_t sourceOffset =
                            (static_cast<size_t>(sourceY) * source.width + sourceX) * 4;
                        const double alpha = source.bgra[sourceOffset + 3];
                        totalWeight += weight;
                        alphaSum += alpha * weight;
                        blueSum += source.bgra[sourceOffset] * alpha * weight;
                        greenSum += source.bgra[sourceOffset + 1] * alpha * weight;
                        redSum += source.bgra[sourceOffset + 2] * alpha * weight;
                    }
                }

                const size_t destinationOffset =
                    (static_cast<size_t>(outputTop + outputY) * destination.width +
                        outputLeft + outputX) * 4;
                if (totalWeight <= 0.0 || alphaSum <= 0.0)
                    continue;
                destination.bgra[destinationOffset] = static_cast<BYTE>(
                    std::min(255.0, std::floor(blueSum / alphaSum + 0.5)));
                destination.bgra[destinationOffset + 1] = static_cast<BYTE>(
                    std::min(255.0, std::floor(greenSum / alphaSum + 0.5)));
                destination.bgra[destinationOffset + 2] = static_cast<BYTE>(
                    std::min(255.0, std::floor(redSum / alphaSum + 0.5)));
                destination.bgra[destinationOffset + 3] = static_cast<BYTE>(
                    std::min(255.0, std::floor(alphaSum / totalWeight + 0.5)));
            }
        }
    }

    bool RenderSourceInto(
        const ObstacleRenderedImage& source,
        ObstacleRenderedImage& destination,
        RECT target,
        int padding,
        bool allowEnlargement = true)
    {
        PixelBounds bounds;
        if (!FindVisibleBounds(source, bounds))
            return false;
        InflateRect(&target, -padding, -padding);
        if (target.left >= target.right || target.top >= target.bottom)
            return false;
        ResampleInto(source, bounds, destination, target, allowEnlargement);
        return true;
    }

    bool BuildIcon(const ObstacleButtonDefinition& definition, ButtonImageCache& cache)
    {
        const ObstacleRenderedImage* source = SourceImageForDef(definition.iconDefName);
        if (!source)
            return false;

        cache.enabledIcon.width = kIconExtent;
        cache.enabledIcon.height = kIconExtent;
        cache.enabledIcon.bgra.assign(kIconExtent * kIconExtent * 4, 0);
        RECT target = { 0, 0, kIconExtent, kIconExtent };
        if (!RenderSourceInto(*source, cache.enabledIcon, target, 0))
            return false;

        cache.disabledIcon = cache.enabledIcon;
        for (size_t offset = 0; offset < cache.disabledIcon.bgra.size(); offset += 4)
        {
            const BYTE blue = cache.disabledIcon.bgra[offset];
            const BYTE green = cache.disabledIcon.bgra[offset + 1];
            const BYTE red = cache.disabledIcon.bgra[offset + 2];
            const BYTE high = std::max(red, std::max(green, blue));
            const BYTE low = std::min(red, std::min(green, blue));
            const BYTE gray = static_cast<BYTE>((static_cast<unsigned int>(high) + low) / 2);
            cache.disabledIcon.bgra[offset] = gray;
            cache.disabledIcon.bgra[offset + 1] = gray;
            cache.disabledIcon.bgra[offset + 2] = gray;
            cache.disabledIcon.bgra[offset + 3] = static_cast<BYTE>(
                (static_cast<unsigned int>(cache.disabledIcon.bgra[offset + 3]) + 1) / 2);
        }
        return true;
    }

    RECT PreviewCell(int count, int index)
    {
        if (count <= 1)
            return { 0, 0, kPreviewExtent, kPreviewExtent };
        if (count == 2)
            return { index * 80, 40, index * 80 + 80, 120 };
        if (count == 3)
        {
            if (index < 2)
                return { index * 80, 0, index * 80 + 80, 80 };
            return { 40, 80, 120, 160 };
        }
        return { (index % 2) * 80, (index / 2) * 80,
            (index % 2) * 80 + 80, (index / 2) * 80 + 80 };
    }

    bool BuildPreview(const ObstacleButtonDefinition& definition, ButtonImageCache& cache)
    {
        const ObstacleRenderedImage* sources[4] = {};
        int sourceCount = 0;
        for (int index = 0; index < definition.previewDefCount && index < 4; ++index)
        {
            const ObstacleRenderedImage* source =
                SourceImageForDef(definition.previewDefNames[index]);
            if (source)
                sources[sourceCount++] = source;
        }
        if (!sourceCount)
            return false;

        cache.preview.width = kPreviewExtent;
        cache.preview.height = kPreviewExtent;
        cache.preview.bgra.assign(kPreviewExtent * kPreviewExtent * 4, 0);
        for (int index = 0; index < sourceCount; ++index)
        {
            const RECT cell = PreviewCell(sourceCount, index);
            RenderSourceInto(*sources[index], cache.preview, cell, sourceCount > 1 ? 2 : 0, false);
        }
        return true;
    }
}

bool InitializeObstacleDefImages(const TargetProfile& profile, std::string& error)
{
    const EditorResourceTargetProfile& resources = profile.editorResources;
    if (!IsExecutableAddress(resources.findArchive) ||
        !IsExecutableAddress(resources.readSelectedEntry))
    {
        error = "editor DEF resource function address is not executable";
        return false;
    }

    s_nativeResources.findArchive = reinterpret_cast<FindArchiveFn>(resources.findArchive);
    s_nativeResources.readSelectedEntry =
        reinterpret_cast<ReadSelectedEntryFn>(resources.readSelectedEntry);
    s_initialized = true;
    return true;
}

const ObstacleRenderedImage* ObstacleDefIconForButton(int buttonIndex, bool enabled)
{
    const ObstacleButtonDefinition* definition = ObstacleButtonDefinitionAt(buttonIndex);
    if (!definition || !definition->iconDefName)
        return nullptr;

    ButtonImageCache& cache = s_buttonImages[buttonIndex];
    if (!cache.iconAttempted)
    {
        cache.iconAttempted = true;
        try
        {
            cache.iconLoaded = BuildIcon(*definition, cache);
        }
        catch (const std::bad_alloc&)
        {
            cache.iconLoaded = false;
            LogFormat(
                "Obstacle variants: could not allocate rendered icon for category button %d.",
                buttonIndex + 1);
        }
    }
    if (!cache.iconLoaded)
        return nullptr;
    return enabled ? &cache.enabledIcon : &cache.disabledIcon;
}

const ObstacleRenderedImage* ObstacleDefPreviewForButton(int buttonIndex)
{
    const ObstacleButtonDefinition* definition = ObstacleButtonDefinitionAt(buttonIndex);
    if (!definition || !definition->previewDefCount)
        return nullptr;

    ButtonImageCache& cache = s_buttonImages[buttonIndex];
    if (!cache.previewAttempted)
    {
        cache.previewAttempted = true;
        try
        {
            cache.previewLoaded = BuildPreview(*definition, cache);
        }
        catch (const std::bad_alloc&)
        {
            cache.previewLoaded = false;
            LogFormat(
                "Obstacle variants: could not allocate rendered preview for category button %d.",
                buttonIndex + 1);
        }
    }
    return cache.previewLoaded ? &cache.preview : nullptr;
}
