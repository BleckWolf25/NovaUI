/**
 * @file WinString.h
 *
 * @version 1.0.0
 * @author BleckWolf25
 * @license MIT
 *
 * @summary Shared UTF-16 string helpers for shell paths, identity keys and config files
 *
 * @description
 * Centralises case folding for identity comparison, environment expansion of shortcut targets,
 * UTF-8 conversion for on-disk configuration and file-name sanitisation for generated shortcuts.
 * Conversion helpers use WideCharToMultiByte and MultiByteToWideChar with explicit lengths so
 * embedded nulls never truncate a path, and accept string views to avoid copies at call sites.
 *
 * @since 18/11/2026
 * @updated 18/11/2026
 */
#pragma once
// ---------- IMPORTS
#include <string>
#include <string_view>
// ---------- DECLARATIONS
std::wstring toLowerWide(std::wstring text);
std::wstring expandEnvironmentStrings(const std::wstring& input);
std::string wideToUtf8(std::wstring_view wide);
std::wstring utf8ToWide(std::string_view utf8);
std::wstring sanitizeFileName(std::wstring name);