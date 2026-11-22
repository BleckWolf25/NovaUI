/**
 * @file TaskbarSettings.cpp
 *
 * @version 1.1.0
 * @author BleckWolf25
 * @license MIT
 *
 * @summary Implementation of the Windows taskbar settings reader
 *
 * @description
 * Asks the shell for the taskbar rectangle with ABM_GETTASKBARPOS and maps uEdge to TaskbarEdge.
 * Registry fallbacks open each key through UniqueRegistryKey and query DWORD values with explicit
 * type checks. The StuckRects3 Settings blob is read as REG_BINARY with byte 12 selecting the
 * edge. Every failure path returns a usable default so the dock always renders.
 *
 * @since 18/11/2026
 * @updated 20/11/2026
 */
// ---------- IMPORTS
#include "Core/TaskbarSettings.h"
#include "Core/WinResources.h"
#include <shellapi.h>
// ---------- CONSTANTS
namespace {
constexpr wchar_t kAdvancedKey[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced";
constexpr wchar_t kSearchKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Search";
constexpr wchar_t kStuckRectsKey[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StuckRects3";
constexpr size_t kEdgeByteOffset = 12;

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

TaskbarEdge edgeFromCode(DWORD code) {
    switch (code) {
    case ABE_LEFT:
        return TaskbarEdge::Left;
    case ABE_TOP:
        return TaskbarEdge::Top;
    case ABE_RIGHT:
        return TaskbarEdge::Right;
    default:
        return TaskbarEdge::Bottom;
    }
}

TaskbarEdge readStuckRectsEdge() {
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
    return edgeFromCode(blob[kEdgeByteOffset]);
}

TaskbarEdge readEdge() {
    if (HWND tray = FindWindowW(L"Shell_TrayWnd", nullptr)) {
        APPBARDATA abd{};
        abd.cbSize = sizeof(abd);
        abd.hWnd = tray;
        if (SHAppBarMessage(ABM_GETTASKBARPOS, &abd) != FALSE) {
            return edgeFromCode(abd.uEdge);
        }
    }
    HKEY rawKey = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kAdvancedKey, 0, KEY_READ, &rawKey) == ERROR_SUCCESS) {
        const UniqueRegistryKey key(rawKey);
        DWORD data = 0;
        DWORD bytes = sizeof(data);
        DWORD type = REG_DWORD;
        if (RegQueryValueExW(key.get(), L"TaskbarLocation", nullptr, &type,
                             reinterpret_cast<BYTE*>(&data), &bytes) == ERROR_SUCCESS &&
            type == REG_DWORD && data <= ABE_BOTTOM) {
            return edgeFromCode(data);
        }
    }
    return readStuckRectsEdge();
}
}
// ---------- IMPLEMENTATION
TaskbarSettings readTaskbarSettings() {
    TaskbarSettings settings{};
    settings.edge = readEdge();
    settings.alignment = readDword(kAdvancedKey, L"TaskbarAl", 1) == 0 ? TaskbarAlignment::Start
                                                                      : TaskbarAlignment::Center;
    const DWORD size = readDword(kAdvancedKey, L"TaskbarSi", 1);
    settings.sizeIndex = size > 2 ? 1 : static_cast<int>(size);
    settings.showTaskView = readDword(kAdvancedKey, L"ShowTaskViewButton", 1) != 0;
    settings.showSearch = readDword(kSearchKey, L"SearchboxTaskbarMode", 1) != 0;
    settings.showWidgets = readDword(kAdvancedKey, L"TaskbarDa", 0) != 0;
    settings.showResume = readDword(kAdvancedKey, L"IsEnabled", 0) != 0;
    settings.showBadges = readDword(kAdvancedKey, L"TaskbarBadges", 1) != 0;
    settings.showFlashing = readDword(kAdvancedKey, L"TaskbarFlashing", 1) != 0;
    return settings;
}