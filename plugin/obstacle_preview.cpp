#include "pch.h"
#include "obstacle_preview.h"

#include "logger.h"
#include "obstacle_button_data.h"
#include "obstacle_def_images.h"

#include <cassert>
#include <vector>

namespace
{
    constexpr int kPreviewImageExtent = 160;
    constexpr int kPreviewBorderExtent = 2;
    constexpr int kPreviewImageAreaExtent =
        kPreviewImageExtent + 2 * kPreviewBorderExtent;
    constexpr int kPreviewImageOffset = kPreviewBorderExtent;
    constexpr int kPreviewCaptionExtent = 23;
    constexpr int kPreviewCaptionHorizontalPadding = 8;
    constexpr int kPreviewWindowWidth = kPreviewImageAreaExtent;
    constexpr int kPreviewMinimumHeight =
        kPreviewImageAreaExtent + kPreviewCaptionExtent;
    constexpr int kPreviewAnchorGap = 8;
    constexpr int kTerrainBitmapResourceId = 141;
    constexpr int kTerrainBitmapWidth = 288;
    constexpr int kTerrainBitmapHeight = 24;
    constexpr int kTerrainIconExtent = 24;
    constexpr int kTerrainIconsPerRow = 6;
    constexpr int kTerrainIconGap = 2;
    constexpr int kTerrainHorizontalPadding = 3;
    constexpr int kTerrainTopPadding = 4;
    constexpr int kTerrainBottomPadding = 4;
    constexpr int kTerrainRuleExtent = 1;
    constexpr int kPreviewCaptionBottom =
        kPreviewImageAreaExtent + kPreviewCaptionExtent - kPreviewBorderExtent;
    constexpr int kTerrainRuleBottom = kPreviewCaptionBottom + kTerrainRuleExtent;
    constexpr int kTerrainIconTop = kTerrainRuleBottom + kTerrainTopPadding;
    struct TerrainBitmapCell
    {
        BYTE terrainBit;
        BYTE bitmapCell;
    };

    // Resource cells 8..10 do not follow the editor's terrain-bit order.
    constexpr TerrainBitmapCell kTerrainBitmapCells[] =
    {
        { 0, 0 },
        { 1, 1 },
        { 2, 2 },
        { 3, 3 },
        { 4, 4 },
        { 5, 5 },
        { 6, 6 },
        { 7, 7 },
        { 8, 10 },
        { 10, 8 },
        { 11, 9 },
    };
    constexpr size_t kTerrainTypeCount =
        sizeof(kTerrainBitmapCells) / sizeof(kTerrainBitmapCells[0]);

    struct TerrainIconPlacement
    {
        BYTE terrainBit = 0;
        BYTE bitmapCell = 0;
        RECT destination = {};
    };

    struct TerrainIconLayout
    {
        TerrainIconPlacement placements[kTerrainTypeCount] = {};
        int count = 0;
        int rowCount = 0;
    };

    HMODULE s_pluginModule = nullptr;
    ATOM s_previewWindowClass = 0;
    HWND s_previewWindow = nullptr;
    HWND s_previewAnchor = nullptr;
    int s_previewButtonIndex = -1;
    bool s_terrainBitmapAttempted = false;
    HBITMAP s_terrainBitmap = nullptr;

    const char* PreviewCaptionForButton(int buttonIndex)
    {
        return ObstacleButtonName(buttonIndex);
    }

    TerrainIconLayout BuildTerrainIconLayout(DWORD terrainMask)
    {
        TerrainIconLayout layout;
        for (const TerrainBitmapCell& terrain : kTerrainBitmapCells)
        {
            if ((terrainMask & (1u << terrain.terrainBit)) == 0)
                continue;

            const int row = layout.count / kTerrainIconsPerRow;
            const int column = layout.count % kTerrainIconsPerRow;
            RECT destination =
            {
                kPreviewBorderExtent + kTerrainHorizontalPadding +
                    column * (kTerrainIconExtent + kTerrainIconGap),
                kTerrainIconTop + row * (kTerrainIconExtent + kTerrainIconGap),
                0,
                0,
            };
            destination.right = destination.left + kTerrainIconExtent;
            destination.bottom = destination.top + kTerrainIconExtent;
            layout.placements[layout.count++] =
                { terrain.terrainBit, terrain.bitmapCell, destination };
        }
        layout.rowCount =
            (layout.count + kTerrainIconsPerRow - 1) / kTerrainIconsPerRow;
        return layout;
    }

    int PreviewWindowHeightForRows(int rowCount)
    {
        if (rowCount <= 0)
            return kPreviewMinimumHeight;
        return kTerrainIconTop + rowCount * kTerrainIconExtent +
            (rowCount - 1) * kTerrainIconGap + kTerrainBottomPadding +
            kPreviewBorderExtent;
    }

    int PreviewWindowHeightForButton(int buttonIndex)
    {
        const ObstacleButtonDefinition* definition =
            ObstacleButtonDefinitionAt(buttonIndex);
        if (!definition)
            return kPreviewMinimumHeight;
        return PreviewWindowHeightForRows(
            BuildTerrainIconLayout(definition->terrainMask).rowCount);
    }

#ifdef _DEBUG
    void ValidateTerrainIconLayout()
    {
        static bool validated = false;
        if (validated)
            return;
        validated = true;

        DWORD mask = 0;
        for (int count = 1; count <= static_cast<int>(kTerrainTypeCount); ++count)
        {
            mask |= 1u << kTerrainBitmapCells[count - 1].terrainBit;
            const TerrainIconLayout layout = BuildTerrainIconLayout(mask);
            assert(layout.count == count);
            assert(layout.rowCount ==
                (count + kTerrainIconsPerRow - 1) / kTerrainIconsPerRow);
            assert(PreviewWindowHeightForRows(layout.rowCount) ==
                (count <= kTerrainIconsPerRow ? 220 : 246));

            for (int index = 0; index < layout.count; ++index)
            {
                const TerrainIconPlacement& placement = layout.placements[index];
                const int row = index / kTerrainIconsPerRow;
                const int column = index % kTerrainIconsPerRow;
                assert(placement.terrainBit ==
                    kTerrainBitmapCells[index].terrainBit);
                assert(placement.bitmapCell ==
                    kTerrainBitmapCells[index].bitmapCell);
                assert(placement.destination.left ==
                    kPreviewBorderExtent + kTerrainHorizontalPadding +
                        column * (kTerrainIconExtent + kTerrainIconGap));
                assert(placement.destination.top ==
                    kTerrainIconTop + row * (kTerrainIconExtent + kTerrainIconGap));
                assert(placement.destination.right <=
                    kPreviewWindowWidth - kPreviewBorderExtent);
                assert(placement.destination.bottom <=
                    PreviewWindowHeightForRows(layout.rowCount) -
                        kTerrainBottomPadding - kPreviewBorderExtent);
            }
        }

        const DWORD rockOnlyMask = 1u << 9;
        assert(BuildTerrainIconLayout(rockOnlyMask).count == 0);

        const TerrainIconLayout sparseLayout = BuildTerrainIconLayout(
            (1u << 0) | (1u << 3) | (1u << 5) | rockOnlyMask);
        assert(sparseLayout.count == 3);
        assert(sparseLayout.placements[0].terrainBit == 0);
        assert(sparseLayout.placements[1].terrainBit == 3);
        assert(sparseLayout.placements[2].terrainBit == 5);
        assert(sparseLayout.placements[2].destination.left ==
            kPreviewBorderExtent + kTerrainHorizontalPadding +
                2 * (kTerrainIconExtent + kTerrainIconGap));
    }
#endif

    HBITMAP TerrainBitmap()
    {
        if (s_terrainBitmapAttempted)
            return s_terrainBitmap;
        s_terrainBitmapAttempted = true;

        HMODULE hotaMe = GetModuleHandleA("hota_me.dll");
        if (!hotaMe)
        {
            LogMessage("Obstacle preview: hota_me.dll is not loaded for terrain icons.");
            return nullptr;
        }

        s_terrainBitmap = reinterpret_cast<HBITMAP>(LoadImageA(
            hotaMe,
            MAKEINTRESOURCEA(kTerrainBitmapResourceId),
            IMAGE_BITMAP,
            0,
            0,
            LR_DEFAULTCOLOR | LR_SHARED));
        BITMAP bitmap = {};
        if (!s_terrainBitmap ||
            GetObjectA(s_terrainBitmap, sizeof(bitmap), &bitmap) != sizeof(bitmap) ||
            bitmap.bmWidth != kTerrainBitmapWidth ||
            bitmap.bmHeight != kTerrainBitmapHeight)
        {
            LogFormat(
                "Obstacle preview: terrain bitmap resource %d is unavailable or not %dx%d.",
                kTerrainBitmapResourceId,
                kTerrainBitmapWidth,
                kTerrainBitmapHeight);
            s_terrainBitmap = nullptr;
        }
        return s_terrainBitmap;
    }

    void DrawTerrainIcons(HDC hdc, DWORD terrainMask)
    {
        const TerrainIconLayout layout = BuildTerrainIconLayout(terrainMask);
        if (!layout.count)
            return;
        HBITMAP bitmap = TerrainBitmap();
        if (!bitmap)
            return;

        HDC source = CreateCompatibleDC(hdc);
        if (!source)
            return;
        HGDIOBJ oldBitmap = SelectObject(source, bitmap);
        for (int index = 0; index < layout.count; ++index)
        {
            const TerrainIconPlacement& placement = layout.placements[index];
            BitBlt(
                hdc,
                placement.destination.left,
                placement.destination.top,
                kTerrainIconExtent,
                kTerrainIconExtent,
                source,
                placement.bitmapCell * kTerrainIconExtent,
                0,
                SRCCOPY);
        }
        SelectObject(source, oldBitmap);
        DeleteDC(source);
    }

    bool DrawPreviewPixels(HDC hdc, const ObstacleRenderedImage& preview, COLORREF background)
    {
        if (preview.bgra.size() != kPreviewImageExtent * kPreviewImageExtent * 4)
            return false;

        std::vector<BYTE> composited(preview.bgra.size());
        const unsigned int backgroundRed = GetRValue(background);
        const unsigned int backgroundGreen = GetGValue(background);
        const unsigned int backgroundBlue = GetBValue(background);
        for (int pixel = 0; pixel < kPreviewImageExtent * kPreviewImageExtent; ++pixel)
        {
            const int offset = pixel * 4;
            const unsigned int alpha = preview.bgra[offset + 3];
            const unsigned int inverseAlpha = 255 - alpha;
            composited[offset + 0] = static_cast<BYTE>(
                (preview.bgra[offset + 0] * alpha + backgroundBlue * inverseAlpha + 127) / 255);
            composited[offset + 1] = static_cast<BYTE>(
                (preview.bgra[offset + 1] * alpha + backgroundGreen * inverseAlpha + 127) / 255);
            composited[offset + 2] = static_cast<BYTE>(
                (preview.bgra[offset + 2] * alpha + backgroundRed * inverseAlpha + 127) / 255);
        }

        BITMAPINFO bitmapInfo = {};
        bitmapInfo.bmiHeader.biSize = sizeof(bitmapInfo.bmiHeader);
        bitmapInfo.bmiHeader.biWidth = kPreviewImageExtent;
        bitmapInfo.bmiHeader.biHeight = -kPreviewImageExtent;
        bitmapInfo.bmiHeader.biPlanes = 1;
        bitmapInfo.bmiHeader.biBitCount = 32;
        bitmapInfo.bmiHeader.biCompression = BI_RGB;
        return SetDIBitsToDevice(
                   hdc,
                   kPreviewImageOffset,
                   kPreviewImageOffset,
                   kPreviewImageExtent,
                   kPreviewImageExtent,
                   0,
                   0,
                   0,
                   kPreviewImageExtent,
                   composited.data(),
                   &bitmapInfo,
                   DIB_RGB_COLORS) != 0;
    }

    void DrawPreviewWindow(HDC hdc)
    {
        RECT rect = {};
        GetClientRect(s_previewWindow, &rect);

        HBRUSH border = CreateSolidBrush(RGB(136, 136, 136));
        FillRect(hdc, &rect, border);
        DeleteObject(border);

        const COLORREF backgroundColor = RGB(255, 255, 255);
        RECT backgroundRect =
        {
            kPreviewBorderExtent,
            kPreviewBorderExtent,
            rect.right - kPreviewBorderExtent,
            kPreviewImageAreaExtent - kPreviewBorderExtent,
        };
        HBRUSH background = CreateSolidBrush(backgroundColor);
        FillRect(hdc, &backgroundRect, background);
        DeleteObject(background);

        const ObstacleRenderedImage* preview =
            ObstacleDefPreviewForButton(s_previewButtonIndex);
        if (preview)
            DrawPreviewPixels(hdc, *preview, backgroundColor);
        else
        {
            const int placeholderExtent = 48;
            const int left = (kPreviewImageAreaExtent - placeholderExtent) / 2;
            const int top = (kPreviewImageAreaExtent - placeholderExtent) / 2;
            HPEN outline = CreatePen(PS_SOLID, 2, RGB(128, 128, 255));
            HGDIOBJ oldPen = SelectObject(hdc, outline);
            HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
            Rectangle(
                hdc,
                left,
                top,
                left + placeholderExtent,
                top + placeholderExtent);
            SelectObject(hdc, oldBrush);
            SelectObject(hdc, oldPen);
            DeleteObject(outline);
        }

        RECT dividerRect =
        {
            kPreviewBorderExtent,
            kPreviewImageAreaExtent - kPreviewBorderExtent,
            rect.right - kPreviewBorderExtent,
            kPreviewImageAreaExtent,
        };
        HBRUSH divider = CreateSolidBrush(RGB(192, 192, 192));
        FillRect(hdc, &dividerRect, divider);
        DeleteObject(divider);

        RECT lowerPanelRect =
        {
            kPreviewBorderExtent,
            kPreviewImageAreaExtent,
            rect.right - kPreviewBorderExtent,
            rect.bottom - kPreviewBorderExtent,
        };
        HBRUSH lowerPanel = CreateSolidBrush(RGB(192, 192, 192));
        FillRect(hdc, &lowerPanelRect, lowerPanel);
        DeleteObject(lowerPanel);

        RECT captionRuleRect =
        {
            kPreviewBorderExtent,
            kPreviewCaptionBottom,
            rect.right - kPreviewBorderExtent,
            kTerrainRuleBottom,
        };
        HBRUSH captionRule = CreateSolidBrush(RGB(160, 160, 160));
        FillRect(hdc, &captionRuleRect, captionRule);
        DeleteObject(captionRule);

        const char* caption = PreviewCaptionForButton(s_previewButtonIndex);
        if (caption)
        {
            RECT captionRect =
            {
                kPreviewBorderExtent + kPreviewCaptionHorizontalPadding,
                kPreviewImageAreaExtent,
                rect.right - kPreviewBorderExtent - kPreviewCaptionHorizontalPadding,
                kPreviewCaptionBottom,
            };
            HFONT font = reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
            HGDIOBJ oldFont = SelectObject(hdc, font);
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(0, 0, 0));
            DrawTextA(
                hdc,
                caption,
                -1,
                &captionRect,
                DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            SelectObject(hdc, oldFont);
        }

        const ObstacleButtonDefinition* definition =
            ObstacleButtonDefinitionAt(s_previewButtonIndex);
        if (definition)
            DrawTerrainIcons(hdc, definition->terrainMask);
    }

    LRESULT CALLBACK PreviewWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        switch (message)
        {
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT:
        {
            PAINTSTRUCT paint = {};
            HDC hdc = BeginPaint(hwnd, &paint);
            DrawPreviewWindow(hdc);
            EndPaint(hwnd, &paint);
            return 0;
        }
        case WM_MOUSEACTIVATE:
            return MA_NOACTIVATE;
        case WM_NCHITTEST:
            return HTTRANSPARENT;
        case WM_NCDESTROY:
            s_previewWindow = nullptr;
            s_previewAnchor = nullptr;
            s_previewButtonIndex = -1;
            return 0;
        }
        return DefWindowProcA(hwnd, message, wParam, lParam);
    }

    bool EnsurePreviewWindow(HMODULE pluginModule, HWND anchor)
    {
        if (!pluginModule || !anchor)
            return false;

#ifdef _DEBUG
        ValidateTerrainIconLayout();
#endif

        s_pluginModule = pluginModule;
        if (!s_previewWindowClass)
        {
            WNDCLASSEXA windowClass = {};
            windowClass.cbSize = sizeof(windowClass);
            windowClass.lpfnWndProc = PreviewWindowProc;
            windowClass.hInstance = s_pluginModule;
            windowClass.hCursor = LoadCursorA(nullptr, IDC_ARROW);
            windowClass.lpszClassName = "HotaMapEdPluginObstaclePreview";
            s_previewWindowClass = RegisterClassExA(&windowClass);
            if (!s_previewWindowClass && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
                return false;
            if (!s_previewWindowClass)
                s_previewWindowClass = 1;
        }

        const HWND owner = GetAncestor(anchor, GA_ROOT);
        if (!s_previewWindow)
        {
            s_previewWindow = CreateWindowExA(
                WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
                "HotaMapEdPluginObstaclePreview",
                nullptr,
                WS_POPUP,
                0,
                0,
                kPreviewWindowWidth,
                kPreviewMinimumHeight,
                owner,
                nullptr,
                s_pluginModule,
                nullptr);
        }
        else if (GetWindow(s_previewWindow, GW_OWNER) != owner)
        {
            SetWindowLongPtrA(
                s_previewWindow,
                GWLP_HWNDPARENT,
                reinterpret_cast<LONG_PTR>(owner));
        }
        return s_previewWindow != nullptr;
    }

    void PositionPreview(HWND anchor, int windowHeight, int& x, int& y)
    {
        RECT anchorRect = {};
        GetWindowRect(anchor, &anchorRect);

        MONITORINFO monitorInfo = {};
        monitorInfo.cbSize = sizeof(monitorInfo);
        GetMonitorInfoA(
            MonitorFromWindow(anchor, MONITOR_DEFAULTTONEAREST),
            &monitorInfo);

        x = anchorRect.left - kPreviewWindowWidth - kPreviewAnchorGap;
        if (x < monitorInfo.rcWork.left)
            x = anchorRect.right + kPreviewAnchorGap;
        y = anchorRect.top +
            ((anchorRect.bottom - anchorRect.top) - windowHeight) / 2;

        if (x + kPreviewWindowWidth > monitorInfo.rcWork.right)
            x = monitorInfo.rcWork.right - kPreviewWindowWidth;
        if (x < monitorInfo.rcWork.left)
            x = monitorInfo.rcWork.left;
        if (y + windowHeight > monitorInfo.rcWork.bottom)
            y = monitorInfo.rcWork.bottom - windowHeight;
        if (y < monitorInfo.rcWork.top)
            y = monitorInfo.rcWork.top;
    }
}

bool ObstaclePreviewAvailable(int buttonIndex)
{
    return ObstacleButtonName(buttonIndex) != nullptr;
}

bool ShowObstaclePreview(HMODULE pluginModule, HWND anchor, int buttonIndex)
{
    if (!ObstaclePreviewAvailable(buttonIndex) ||
        !EnsurePreviewWindow(pluginModule, anchor))
    {
        HideObstaclePreview(nullptr);
        return false;
    }

    s_previewAnchor = anchor;
    s_previewButtonIndex = buttonIndex;
    const int windowHeight = PreviewWindowHeightForButton(buttonIndex);
    int x = 0;
    int y = 0;
    PositionPreview(anchor, windowHeight, x, y);
    SetWindowPos(
        s_previewWindow,
        HWND_TOP,
        x,
        y,
        kPreviewWindowWidth,
        windowHeight,
        SWP_NOACTIVATE | SWP_SHOWWINDOW);
    InvalidateRect(s_previewWindow, nullptr, FALSE);
    UpdateWindow(s_previewWindow);
    return true;
}

void HideObstaclePreview(HWND anchor)
{
    if (anchor && anchor != s_previewAnchor)
        return;

    if (s_previewWindow)
        ShowWindow(s_previewWindow, SW_HIDE);
    s_previewAnchor = nullptr;
    s_previewButtonIndex = -1;
}
