/**
 * @file WindowIdentity.h
 *
 * @version 1.0.0
 * @author BleckWolf25
 * @license MIT
 *
 * @summary Application identity and taskbar-eligibility rules for top-level windows
 *
 * @description
 * Derives a stable grouping key for a window from its ApplicationUserModelId or executable path,
 * unwrapping ApplicationFrameHost to the hosted UWP process so packaged apps group with their
 * pins. isTaskbarWindow applies the shell's eligibility rules: visible, not a tool window, not an
 * unforced owned window, not DWM-cloaked and carrying a title. Query only; holds no state.
 *
 * @since 18/11/2026
 * @updated 18/11/2026
 */
#pragma once
// ---------- IMPORTS
#include <Windows.h>
#include <string>
// ---------- TYPES
struct WindowIdentity {
    std::wstring exePath;
    std::wstring exePathLower;
    std::wstring aumid;
    std::wstring aumidLower;
    std::wstring groupKey;
};
// ---------- DECLARATIONS
WindowIdentity queryWindowIdentity(HWND hwnd);
bool isTaskbarWindow(HWND hwnd, HWND selfHwnd);