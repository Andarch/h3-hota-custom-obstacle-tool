#include "pch.h"
#include "obstacle_preview.h"

#include "logger.h"
#include "obstacle_button_data.h"
#include "obstacle_icon_data.h"

#include <objbase.h>
#include <vector>
#include <wincodec.h>
#include <wrl/client.h>

namespace
{
    constexpr int kPreviewButtonLimit = 64;
    constexpr int kPreviewImageExtent = 160;
    constexpr int kPreviewBorderExtent = 2;
    constexpr int kPreviewImageAreaExtent =
        kPreviewImageExtent + 2 * kPreviewBorderExtent;
    constexpr int kPreviewImageOffset = kPreviewBorderExtent;
    constexpr int kPreviewCaptionExtent = 23;
    constexpr int kPreviewCaptionHorizontalPadding = 8;
    constexpr int kPreviewWindowWidth = kPreviewImageAreaExtent;
    constexpr int kPreviewWindowHeight = kPreviewImageAreaExtent + kPreviewCaptionExtent;
    constexpr int kPreviewAnchorGap = 8;

    struct PreviewCacheEntry
    {
        bool attempted = false;
        bool loaded = false;
        std::vector<BYTE> bgra;
    };

    PreviewCacheEntry s_previewCache[kPreviewButtonLimit] = {};
    HMODULE s_pluginModule = nullptr;
    ATOM s_previewWindowClass = 0;
    HWND s_previewWindow = nullptr;
    HWND s_previewAnchor = nullptr;
    int s_previewButtonIndex = -1;

    const char* PreviewCaptionForButton(int buttonIndex)
    {
        return ObstacleButtonName(buttonIndex);
    }

    bool DecodePreviewResource(int resourceId, std::vector<BYTE>& bgra)
    {
        HRSRC resource = FindResourceA(
            s_pluginModule,
            MAKEINTRESOURCEA(resourceId),
            RT_RCDATA);
        if (!resource)
            return false;

        const DWORD resourceSize = SizeofResource(s_pluginModule, resource);
        HGLOBAL loadedResource = LoadResource(s_pluginModule, resource);
        BYTE* resourceBytes = static_cast<BYTE*>(LockResource(loadedResource));
        if (!resourceSize || !loadedResource || !resourceBytes)
            return false;

        const HRESULT initializeResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        const bool uninitialize = SUCCEEDED(initializeResult);
        if (FAILED(initializeResult) && initializeResult != RPC_E_CHANGED_MODE)
            return false;

        bool decoded = false;
        {
            using Microsoft::WRL::ComPtr;
            ComPtr<IWICImagingFactory> factory;
            ComPtr<IWICStream> stream;
            ComPtr<IWICBitmapDecoder> decoder;
            ComPtr<IWICBitmapFrameDecode> frame;
            ComPtr<IWICFormatConverter> converter;

            HRESULT result = CoCreateInstance(
                CLSID_WICImagingFactory,
                nullptr,
                CLSCTX_INPROC_SERVER,
                IID_PPV_ARGS(factory.GetAddressOf()));
            if (SUCCEEDED(result))
                result = factory->CreateStream(stream.GetAddressOf());
            if (SUCCEEDED(result))
                result = stream->InitializeFromMemory(resourceBytes, resourceSize);
            if (SUCCEEDED(result))
            {
                result = factory->CreateDecoderFromStream(
                    stream.Get(),
                    nullptr,
                    WICDecodeMetadataCacheOnLoad,
                    decoder.GetAddressOf());
            }
            if (SUCCEEDED(result))
                result = decoder->GetFrame(0, frame.GetAddressOf());

            UINT width = 0;
            UINT height = 0;
            if (SUCCEEDED(result))
                result = frame->GetSize(&width, &height);
            if (SUCCEEDED(result) &&
                (width != kPreviewImageExtent || height != kPreviewImageExtent))
                result = E_INVALIDARG;

            if (SUCCEEDED(result))
                result = factory->CreateFormatConverter(converter.GetAddressOf());
            if (SUCCEEDED(result))
            {
                result = converter->Initialize(
                    frame.Get(),
                    GUID_WICPixelFormat32bppBGRA,
                    WICBitmapDitherTypeNone,
                    nullptr,
                    0.0,
                    WICBitmapPaletteTypeCustom);
            }
            if (SUCCEEDED(result))
            {
                const UINT stride = kPreviewImageExtent * 4;
                bgra.resize(stride * kPreviewImageExtent);
                result = converter->CopyPixels(
                    nullptr,
                    stride,
                    static_cast<UINT>(bgra.size()),
                    bgra.data());
            }
            decoded = SUCCEEDED(result);
        }

        if (uninitialize)
            CoUninitialize();
        return decoded;
    }

    PreviewCacheEntry* PreviewForButton(int buttonIndex)
    {
        if (buttonIndex < 0 || buttonIndex >= kPreviewButtonLimit)
            return nullptr;

        const int resourceId = ObstaclePreviewResourceForButton(buttonIndex);
        if (resourceId < 0)
            return nullptr;

        PreviewCacheEntry& cache = s_previewCache[buttonIndex];
        if (!cache.attempted)
        {
            cache.attempted = true;
            cache.loaded = DecodePreviewResource(resourceId, cache.bgra);
            if (!cache.loaded)
            {
                LogFormat(
                    "Obstacle variants: failed to decode preview resource %d for category button %d.",
                    resourceId,
                    buttonIndex + 1);
            }
        }
        return cache.loaded ? &cache : nullptr;
    }

    bool DrawPreviewPixels(HDC hdc, const PreviewCacheEntry& preview, COLORREF background)
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
        RECT backgroundRect = rect;
        InflateRect(&backgroundRect, -kPreviewBorderExtent, -kPreviewBorderExtent);
        HBRUSH background = CreateSolidBrush(backgroundColor);
        FillRect(hdc, &backgroundRect, background);
        DeleteObject(background);

        PreviewCacheEntry* preview = PreviewForButton(s_previewButtonIndex);
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

        RECT captionBackgroundRect =
        {
            kPreviewBorderExtent,
            kPreviewImageAreaExtent,
            rect.right - kPreviewBorderExtent,
            rect.bottom - kPreviewBorderExtent,
        };
        HBRUSH captionBackground = CreateSolidBrush(RGB(240, 240, 240));
        FillRect(hdc, &captionBackgroundRect, captionBackground);
        DeleteObject(captionBackground);

        const char* caption = PreviewCaptionForButton(s_previewButtonIndex);
        if (caption)
        {
            RECT captionRect =
            {
                kPreviewBorderExtent + kPreviewCaptionHorizontalPadding,
                kPreviewImageAreaExtent,
                rect.right - kPreviewBorderExtent - kPreviewCaptionHorizontalPadding,
                rect.bottom - kPreviewBorderExtent,
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
                kPreviewWindowHeight,
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

    void PositionPreview(HWND anchor, int& x, int& y)
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
            ((anchorRect.bottom - anchorRect.top) - kPreviewWindowHeight) / 2;

        if (x + kPreviewWindowWidth > monitorInfo.rcWork.right)
            x = monitorInfo.rcWork.right - kPreviewWindowWidth;
        if (x < monitorInfo.rcWork.left)
            x = monitorInfo.rcWork.left;
        if (y + kPreviewWindowHeight > monitorInfo.rcWork.bottom)
            y = monitorInfo.rcWork.bottom - kPreviewWindowHeight;
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
    int x = 0;
    int y = 0;
    PositionPreview(anchor, x, y);
    SetWindowPos(
        s_previewWindow,
        HWND_TOP,
        x,
        y,
        kPreviewWindowWidth,
        kPreviewWindowHeight,
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
