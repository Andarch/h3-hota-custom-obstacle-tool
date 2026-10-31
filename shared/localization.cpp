#include "localization.h"
#include <cstring>
#include <map>

namespace
{
    struct Translation { const char* key; const wchar_t* english; const wchar_t* russian; };
    const Translation kTranslations[] = {
#include "localization_strings.inc"
    };
    volatile LONG s_language = 0;
    SRWLOCK s_fallbackLock = SRWLOCK_INIT;
    std::map<std::string, std::wstring> s_fallbacks;

    const Translation* FindTranslation(const char* key)
    {
        for (const auto& entry : kTranslations)
            if (strcmp(entry.key, key) == 0) return &entry;
        return nullptr;
    }
}

UiLanguage CurrentUiLanguage()
{
    return InterlockedCompareExchange(&s_language, 0, 0) ? UiLanguage::Russian : UiLanguage::English;
}

void SetUiLanguage(UiLanguage language)
{
    InterlockedExchange(&s_language, language == UiLanguage::Russian ? 1 : 0);
}

UiLanguage LanguageForEditorDirectory(const char* directory)
{
    if (!directory || !*directory) return UiLanguage::English;
    std::string path = directory;
    if (path.back() != '\\' && path.back() != '/') path += '\\';
    path += "HotA_Setup.ini";
    return GetPrivateProfileIntA("Global Settings", "Language", -1, path.c_str()) == 1
        ? UiLanguage::Russian : UiLanguage::English;
}

bool DetectEditorUiLanguage(HWND editor)
{
    HMENU menu = GetMenu(editor);
    if (!menu) return false;
    for (int index = 0; index < GetMenuItemCount(menu); ++index)
    {
        wchar_t wide[128]{}; char ansi[128]{};
        GetMenuStringW(menu, index, wide, 128, MF_BYPOSITION);
        GetMenuStringA(menu, index, ansi, 128, MF_BYPOSITION);
        std::wstring text; std::string bytes;
        for (const wchar_t* c = wide; *c && *c != L'\t'; ++c) if (*c != L'&') text += *c;
        for (const char* c = ansi; *c && *c != '\t'; ++c) if (*c != '&') bytes += *c;
        if (text == L"\u0424\u0430\u0439\u043b" || text == L"\u0421\u043f\u0440\u0430\u0432\u043a\u0430" ||
            bytes == "\xD4\xE0\xE9\xEB" || bytes == "\xD1\xEF\xF0\xE0\xE2\xEA\xE0")
        {
            SetUiLanguage(UiLanguage::Russian);
            return true;
        }
        if (text == L"File" || text == L"Help")
        {
            SetUiLanguage(UiLanguage::English);
            return true;
        }
    }
    return false;
}

std::wstring UiWide(const char* utf8)
{
    if (!utf8 || !*utf8) return {};
    const int size = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0);
    if (!size) return {};
    std::wstring text(size, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, &text[0], size);
    text.pop_back();
    return text;
}

const wchar_t* UiText(const char* english)
{
    if (!english) return L"";
    if (const auto* entry = FindTranslation(english))
        return CurrentUiLanguage() == UiLanguage::Russian ? entry->russian : entry->english;
    // Preserve newly added workbook names in English until a translation exists.
    // Map nodes and their stored strings are never removed, so tooltip pointers
    // remain valid even when another UI thread adds a fallback.
    AcquireSRWLockExclusive(&s_fallbackLock);
    auto found = s_fallbacks.find(english);
    if (found == s_fallbacks.end()) found = s_fallbacks.emplace(english, UiWide(english)).first;
    const wchar_t* text = found->second.c_str();
    ReleaseSRWLockExclusive(&s_fallbackLock);
    return text;
}

std::wstring UiMessage(const std::string& english)
{
    if (FindTranslation(english.c_str())) return UiText(english.c_str());
    // Launcher errors append translated paragraphs to a stable English error key.
    std::wstring result;
    size_t begin = 0;
    while (begin < english.size())
    {
        size_t end = english.find("\n\n", begin);
        const std::string paragraph = english.substr(begin, end == std::string::npos ? end : end - begin);
        result += UiText(paragraph.c_str());
        if (end == std::string::npos) break;
        result += L"\n\n";
        begin = end + 2;
    }
    return result;
}
