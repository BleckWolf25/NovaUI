/**
 * @file ShellPaths.h
 *
 * @version 1.0.0
 * @author BleckWolf25
 * @license MIT
 *
 * @summary Canonical shell and application directories used by the taskbar modules
 *
 * @description
 * Resolves the roaming AppData root through SHGetKnownFolderPath and derives Explorer's
 * User Pinned\TaskBar folder plus NovaUI's own configuration folder and file paths. Owning the
 * path layout here keeps pin storage, tray promotion state and shortcut generation agreeing on
 * one location, and every returned path is UTF-16 for direct use with the wide Win32 API.
 *
 * @since 18/11/2026
 * @updated 18/11/2026
 */
#pragma once
// ---------- IMPORTS
#include <string>
#include <string_view>
// ---------- DECLARATIONS
std::wstring roamingAppDataDirectory();
std::wstring pinnedTaskbarDirectory();
std::wstring novaConfigDirectory();
std::wstring novaConfigFilePath(std::wstring_view fileName);