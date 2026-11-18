/**
 * @file WinString.cpp
 *
 * @version 1.0.0
 * @author BleckWolf25
 * @license MIT
 *
 * @summary Implementation of the shared UTF-16 string helpers
 *
 * @description
 * Folds characters with towlower for locale-stable identity keys, expands environment references
 * through ExpandEnvironmentStringsW using a two-pass size query, converts between UTF-16 and
 * UTF-8 with CP_UTF8, and replaces reserved file-name characters with underscores while trimming
 * trailing spaces and dots that Win32 silently strips.
 *
 * @since 18/11/2026
 * @updated 18/11/2026
 */
// ---------- IMPORTS
#include "Core/WinString.h"
#include <Windows.h>
#include <cwctype>
// ---------- IMPLEMENTATION
std::wstring toLowerWide(std::wstring text) {
    for (wchar_t& ch : text) {
        ch = static_cast<wchar_t>(std::towlower(ch));
    }
    return text;
}

std::wstring expandEnvironmentStrings(const std::wstring& input) {
    if (input.empty()) {
        return {};
    }
    const DWORD required = ExpandEnvironmentStringsW(input.c_str(), nullptr, 0);
    if (required == 0) {
        return input;
    }
    std::wstring expanded(required, L'\0');
    if (ExpandEnvironmentStringsW(input.c_str(), expanded.data(), required) == 0) {
        return input;
    }
    if (!expanded.empty() && expanded.back() == L'\0') {
        expanded.pop_back();
    }
    return expanded;
}

std::string wideToUtf8(std::wstring_view wide) {
    if (wide.empty()) {
        return {};
    }
    const int bytes = WideCharToMultiByte(CP_UTF8, 0, wide.data(),
                                          static_cast<int>(wide.size()), nullptr, 0, nullptr,
                                          nullptr);
    if (bytes <= 0) {
        return {};
    }
    std::string utf8(static_cast<size_t>(bytes), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), utf8.data(), bytes,
                        nullptr, nullptr);
    return utf8;
}

std::wstring utf8ToWide(std::string_view utf8) {
    if (utf8.empty()) {
        return {};
    }
    const int chars =
        MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
    if (chars <= 0) {
        return {};
    }
    std::wstring wide(static_cast<size_t>(chars), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), wide.data(), chars);
    return wide;
}

std::wstring sanitizeFileName(std::wstring name) {
    for (wchar_t& ch : name) {
        if (ch == L'\\' || ch == L'/' || ch == L':' || ch == L'*' || ch == L'?' || ch == L'"' ||
            ch == L'<' || ch == L'>' || ch == L'|') {
            ch = L'_';
        }
    }
    while (!name.empty() && (name.back() == L' ' || name.back() == L'.')) {
        name.pop_back();
    }
    return name.empty() ? L"PinnedApp" : name;
}