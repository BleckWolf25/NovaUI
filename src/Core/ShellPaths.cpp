/**
 * @file ShellPaths.cpp
 *
 * @version 1.0.0
 * @author BleckWolf25
 * @license MIT
 *
 * @summary Implementation of shell and application directory resolution
 *
 * @description
 * Queries FOLDERID_RoamingAppData once per call and releases the shell-allocated buffer through
 * CoTaskMemString. The NovaUI configuration directory is created on demand with an error_code so
 * a missing or read-only profile degrades to an empty path instead of throwing across a Win32
 * boundary.
 *
 * @since 18/11/2026
 * @updated 18/11/2026
 */
// ---------- IMPORTS
#include "Core/ShellPaths.h"
#include "Core/WinResources.h"
#include <shlobj.h>
#include <filesystem>
// ---------- IMPLEMENTATION
std::wstring roamingAppDataDirectory() {
    PWSTR raw = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &raw)) || !raw) {
        return {};
    }
    const CoTaskMemString owned(raw);
    return std::wstring(owned.get());
}

std::wstring pinnedTaskbarDirectory() {
    const std::wstring root = roamingAppDataDirectory();
    if (root.empty()) {
        return {};
    }
    return root + L"\\Microsoft\\Internet Explorer\\Quick Launch\\User Pinned\\TaskBar";
}

std::wstring novaConfigDirectory() {
    const std::wstring root = roamingAppDataDirectory();
    if (root.empty()) {
        return {};
    }
    const std::filesystem::path dir = std::filesystem::path(root) / L"NovaUI";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    return dir.wstring();
}

std::wstring novaConfigFilePath(std::wstring_view fileName) {
    const std::wstring dir = novaConfigDirectory();
    if (dir.empty() || fileName.empty()) {
        return {};
    }
    return (std::filesystem::path(dir) / std::wstring(fileName)).wstring();
}