#pragma once
#include <windows.h>
#include <string>

enum class UiLanguage { English, Russian };
UiLanguage CurrentUiLanguage();
void SetUiLanguage(UiLanguage language);
// Missing/unknown settings fall back to English. Never use Windows locale once
// an editor installation is known. Native menu captions take precedence over INI.
UiLanguage LanguageForEditorDirectory(const char* directory);
bool DetectEditorUiLanguage(HWND editor);
const wchar_t* UiText(const char* english);
std::wstring UiMessage(const std::string& english);
std::wstring UiWide(const char* utf8);
