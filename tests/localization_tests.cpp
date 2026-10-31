// Include production preview code to verify wrapped Cyrillic captions and icons.
#include "../plugin/obstacle_preview.cpp"
#include "../shared/localization.h"
#include <cstdio>
#include <cstring>
#define CHECK_LANG(x) do { if (!(x)) { std::fprintf(stderr, "Localization failed line %d: %s\n", __LINE__, #x); ExitProcess(1); } } while (false)

void TestLocalization()
{
    SetUiLanguage(UiLanguage::English);
    CHECK_LANG(wcscmp(UiText("Standard"), L"Standard") == 0);
    CHECK_LANG(wcscmp(UiText("Untranslated future button"), L"Untranslated future button") == 0);
    const wchar_t* fallback = UiText("Untranslated future button");
    UiText("Another future button");
    CHECK_LANG(wcscmp(fallback, L"Untranslated future button") == 0);

    char temporary[MAX_PATH]{}, unique[MAX_PATH]{};
    CHECK_LANG(GetTempPathA(MAX_PATH, temporary));
    CHECK_LANG(GetTempFileNameA(temporary, "cot", 0, unique));
    CHECK_LANG(DeleteFileA(unique));
    CHECK_LANG(CreateDirectoryA(unique, nullptr));
    std::string ini = std::string(unique) + "\\HotA_Setup.ini";
    CHECK_LANG(LanguageForEditorDirectory(unique) == UiLanguage::English);
    for (const char* value : { "1", "0", "99" })
    {
        CHECK_LANG(WritePrivateProfileStringA("Global Settings", "Language", value, ini.c_str()));
        CHECK_LANG(LanguageForEditorDirectory(unique) ==
            (strcmp(value, "1") == 0 ? UiLanguage::Russian : UiLanguage::English));
    }
    CHECK_LANG(DeleteFileA(ini.c_str()));
    CHECK_LANG(RemoveDirectoryA(unique));

    HWND owner = CreateWindowExW(0, L"STATIC", L"Language test", WS_OVERLAPPEDWINDOW,
        100, 100, 700, 500, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    CHECK_LANG(owner);
    HMENU menu = CreateMenu(), popup = CreatePopupMenu();
    CHECK_LANG(AppendMenuW(popup, MF_STRING, 0xE140, L"Native About"));
    CHECK_LANG(AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(popup), L"&Help"));
    CHECK_LANG(SetMenu(owner, menu));
    SetUiLanguage(UiLanguage::Russian);
    CHECK_LANG(DetectEditorUiLanguage(owner));
    CHECK_LANG(CurrentUiLanguage() == UiLanguage::English);
    CHECK_LANG(ModifyMenuW(menu, 0, MF_BYPOSITION | MF_POPUP,
        reinterpret_cast<UINT_PTR>(popup), L"\u0421\u043f\u0440\u0430\u0432\u043a\u0430"));
    CHECK_LANG(DetectEditorUiLanguage(owner));
    CHECK_LANG(CurrentUiLanguage() == UiLanguage::Russian);
    CHECK_LANG(ModifyMenuA(menu, 0, MF_BYPOSITION | MF_POPUP,
        reinterpret_cast<UINT_PTR>(popup), "\xD4\xE0\xE9\xEB"));
    SetUiLanguage(UiLanguage::English);
    CHECK_LANG(DetectEditorUiLanguage(owner));
    CHECK_LANG(CurrentUiLanguage() == UiLanguage::Russian);
    DestroyWindow(owner);

    CHECK_LANG(wcscmp(UiText("Standard"), L"Standard") != 0);
    for (int category = 0; category < kObstacleCategoryCount; ++category)
        CHECK_LANG(std::wstring(UiText(ObstacleCategoryName(category))) != UiWide(ObstacleCategoryName(category)));
    int wrapped = 0;
    for (int button = 0; button < kObstacleButtonCount; ++button)
    {
        CHECK_LANG(std::wstring(UiText(ObstacleButtonName(button))) != UiWide(ObstacleButtonName(button)));
        const int caption = PreviewCaptionHeightForButton(button);
        CHECK_LANG(caption >= kPreviewCaptionExtent);
        if (caption > kPreviewCaptionExtent) ++wrapped;
        const auto layout = BuildTerrainIconLayout(EffectiveObstacleTerrainMask(
            ObstacleButtonDefinitionAt(button)->terrainMask), caption);
        const int height = PreviewWindowHeightForButton(button);
        for (int i = 0; i < layout.count; ++i)
        {
            CHECK_LANG(layout.placements[i].destination.top >= kPreviewImageAreaExtent + caption);
            CHECK_LANG(layout.placements[i].destination.bottom + kTerrainBottomPadding <= height);
        }
        CHECK_LANG(caption < 100);
    }
    CHECK_LANG(wrapped > 0);
    const std::string error = "VirtualAllocEx failed.\n\nThe editor will continue without the plugin.";
    CHECK_LANG(UiMessage(error) == std::wstring(UiText("VirtualAllocEx failed.")) +
        L"\n\n" + UiText("The editor will continue without the plugin."));
    SetUiLanguage(UiLanguage::English);
    CHECK_LANG(UiMessage(error) == UiWide(error.c_str()));
    std::puts("PASS: editor language overrides startup hint, Unicode Russian names, wrapped previews, launcher paragraphs and English fallback.");
}
