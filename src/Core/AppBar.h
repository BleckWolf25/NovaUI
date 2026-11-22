/**
 * @file AppBar.h
 *
 * @version 1.1.0
 * @author BleckWolf25
 * @license MIT
 *
 * @summary Screen-edge reservation for the taskbar through the shell AppBar API
 *
 * @description
 * Registers a window as an AppBar on the bottom edge of the primary monitor so maximized windows
 * stop at its boundary. Uses SHAppBarMessage (ABM_NEW, ABM_QUERYPOS, ABM_SETPOS, ABM_REMOVE).
 * The shell reports layout changes through the WM_APP_APPBAR callback message.
 *
 * @since 17/11/2026
 * @updated 20/11/2026
 */
#pragma once
// ---------- IMPORTS
#include <Windows.h>
#include "TaskbarSettings.h"

// ---------- CONSTANTS
inline constexpr UINT WM_APP_APPBAR = WM_APP + 1;

// ---------- DECLARATIONS
class AppBar {
public:
    bool add(HWND hwnd, UINT callbackMessage);
    RECT dock(HWND hwnd, int thickness, TaskbarEdge edge);
    void remove(HWND hwnd);

private:
    bool m_registered = false;
};
