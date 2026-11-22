/**
 * @file TrayAutomationReader.h
 *
 * @version 1.0.0
 * @author BleckWolf25
 * @license MIT
 *
 * @summary UI Automation backend for the modern XAML notification area
 *
 * @description
 * Enumerates tray buttons through IUIAutomation when Explorer hosts the notification area as a
 * XAML island instead of a classic toolbar. Reads each element's name, bounding rectangle and
 * owning process so entries carry a tooltip, an executable icon and a persistence key. Clicks are
 * dispatched with the UI Automation invoke pattern. Runs on the UI thread only when the tray
 * module reacts to a shell event.
 *
 * @since 18/11/2026
 * @updated 18/11/2026
 */
#pragma once
// ---------- IMPORTS
#include "Modules/TrayToolbarReader.h"
#include <vector>
// ---------- DECLARATIONS
std::vector<RawTrayIcon> readAutomationTrayIcons();
bool invokeAutomationTrayIcon(const std::wstring& persistKey);