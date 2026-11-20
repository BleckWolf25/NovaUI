/**
 * @file TaskbarSettings.cpp
 *
 * @version 1.0.0
 * @author BleckWolf25
 * @license MIT
 *
 * @summary Implementation of the Windows taskbar settings reader
 *
 * @description
 * Opens each registry key through UniqueRegistryKey and queries DWORD values with explicit type
 * checks, falling back to the Windows defaults when a value is absent. The StuckRects3 Settings
 * blob is read as REG_BINARY and byte 12 selects the edge; only the top and bottom values are
 * accepted. Every failure path returns a usable default so the dock always renders.
 *
 * @since 18/11/2026
 * @updated 18/11/2026
 */
// ---------- IMPORTS
#include "Core/TaskbarSettings.h"
#include "Core/WinResources.h"
// ---------- CONSTANTS
namespace {
constexpr wchar_t kAdvancedKey[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced";
constexpr wchar_t kSearchKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Search";
constexpr wchar_t kStuckRectsKey[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StuckRects3";
constexpr size_t kEdgeByteOffset = 12;

bool readFlag(const wchar_t* subKey, const wchar_t* valueName, bool fallback) {
    HKEY rawKey = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, subKey, 0, KEY_READ, &rawKey) != ERROR_SUCCESS) {
        return fallback;
    }
    const UniqueRegistryKey key(rawKey);
    DWORD data = 0;
    DWORD bytes = sizeof(data);
    DWORD type = REG_DWORD;
    if (RegQueryValueExW(key.get(), valueName, nullptr, &type, reinterpret_cast<BYTE*>(&data),
                         &bytes) != ERROR_SUCCESS ||
        type != REG_DWORD) {
        return fallback;
    }
    return data != 0;
}

DWORD readDword(const wchar_t* subKey, const wchar_t* valueName, DWORD fallback) {
    HKEY rawKey = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, subKey, 0, KEY_READ, &rawKey) != ERROR_SUCCESS) {
        return fallback;
    }
    const UniqueRegistryKey key(rawKey);
    DWORD data = 0;
    DWORD bytes = sizeof(data);
    DWORD type = REG_DWORD;
    if (RegQueryValueExW(key.get(), valueName, nullptr, &type, reinterpret_cast<BYTE*>(&data),
                         &bytes) != ERROR_SUCCESS ||
        type != REG_DWORD) {
        return fallback;
    }
    return data;
}

TaskbarEdge readEdge() {
    HKEY rawKey = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kStuckRectsKey, 0, KEY_READ, &rawKey) != ERROR_SUCCESS) {
        return TaskbarEdge::Bottom;
    }
    const UniqueRegistryKey key(rawKey);
    DWORD type = 0;
    DWORD bytes = 0;
    if (RegQueryValueExW(key.get(), L"Settings", nullptr, &type, nullptr, &bytes) !=
            ERROR_SUCCESS ||
        type != REG_BINARY || bytes <= kEdgeByteOffset) {
        return TaskbarEdge::Bottom;
    }
    BYTE blob[64]{};
    DWORD capacity = static_cast<DWORD>(sizeof(blob));
    if (RegQueryValueExW(key.get(), L"Settings", nullptr, nullptr, blob, &capacity) !=
            ERROR_SUCCESS ||
        capacity <= kEdgeByteOffset) {
        return TaskbarEdge::Bottom;
    }
    return blob[kEdgeByteOffset] == 1 ? TaskbarEdge::Top : TaskbarEdge::Bottom;
}
}
// ---------- IMPLEMENTATION
TaskbarSettings readTaskbarSettings() {
    TaskbarSettings settings{};
    settings.alignment =
        readDword(kAdvancedKey, L"TaskbarAl", 1) == 0 ? TaskbarAlignment::Left
                                                      : TaskbarAlignment::Center;
    const DWORD size = readDword(kAdvancedKey, L"TaskbarSi", 1);
    settings.sizeIndex = size > 2 ? 1 : static_cast<int>(size);
    settings.edge = readEdge();
    settings.showTaskView = readFlag(kAdvancedKey, L"ShowTaskViewButton", true);
    settings.showSearch = readDword(kSearchKey, L"SearchboxTaskbarMode", 1) != 0;
    settings.showWidgets = readFlag(kAdvancedKey, L"TaskbarDa", false);
    settings.showResume = readFlag(kAdvancedKey, L"IsEnabled", false);
    settings.showBadges = readFlag(kAdvancedKey, L"TaskbarBadges", true);
    settings.showFlashing = readFlag(kAdvancedKey, L"TaskbarFlashing", true);
    return settings;
}