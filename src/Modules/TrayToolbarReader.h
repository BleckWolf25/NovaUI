/**
 * @file TrayToolbarReader.h
 *
 * @version 1.0.0
 * @author BleckWolf25
 * @license MIT
 *
 * @summary Backend-neutral notification icon enumeration for the tray module
 *
 * @description
 * Declares RawTrayIcon, the identity and imagery a tray entry needs, and readTrayIcons, the single
 * aggregation point every backend feeds. The toolbar backend reads Explorer's classic
 * notification toolbars; entries carry their source so a later automation backend can supply
 * icons that have no native callback window without changing TrayTracker or the flyout.
 *
 * @since 18/11/2026
 * @updated 18/11/2026
 */
#pragma once
// ---------- IMPORTS
#include <Windows.h>
#include <string>
#include <vector>
// ---------- TYPES
enum class TrayIconSource { Toolbar, Automation };

struct RawTrayIcon {
    TrayIconSource source = TrayIconSource::Toolbar;
    HWND ownerHwnd = nullptr;
    UINT uID = 0;
    UINT uCallbackMessage = 0;
    HICON hIcon = nullptr;
    std::wstring tooltip;
    std::wstring persistKey;
};
// ---------- DECLARATIONS
std::vector<RawTrayIcon> readTrayIcons();