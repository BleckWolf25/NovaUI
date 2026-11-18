/**
 * @file AppBar.cpp
 *
 * @version 1.0.1
 * @author BleckWolf25
 * @license MIT
 *
 * @summary Implementation of the shell AppBar registration and edge reservation
 *
 * @description
 * Negotiates the bottom-edge rectangle with the shell: queries the allowed position, pins the
 * thickness to the bottom of the primary monitor and commits it. The caller moves its window to
 * the returned rectangle. APPBARDATA and SHAppBarMessage come from shellapi.h, which the lean
 * Windows.h excludes, so it is included explicitly.
 *
 * @since 17/11/2026
 * @updated 18/11/2026
 */
// ---------- IMPORTS
#include "Core/AppBar.h"
#include <shellapi.h>
// ---------- IMPLEMENTATION
namespace {
APPBARDATA makeData(HWND hwnd) {
    APPBARDATA abd{};
    abd.cbSize = sizeof(abd);
    abd.hWnd = hwnd;
    return abd;
}
}
bool AppBar::add(HWND hwnd, UINT callbackMessage) {
    APPBARDATA abd = makeData(hwnd);
    abd.uCallbackMessage = callbackMessage;
    m_registered = SHAppBarMessage(ABM_NEW, &abd) != FALSE;
    return m_registered;
}
RECT AppBar::dock(HWND hwnd, int thickness) {
    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    GetMonitorInfoW(MonitorFromPoint(POINT{0, 0}, MONITOR_DEFAULTTOPRIMARY), &mi);
    APPBARDATA abd = makeData(hwnd);
    abd.uEdge = ABE_BOTTOM;
    abd.rc = mi.rcMonitor;
    abd.rc.top = abd.rc.bottom - thickness;
    SHAppBarMessage(ABM_QUERYPOS, &abd);
    abd.rc.top = abd.rc.bottom - thickness;
    SHAppBarMessage(ABM_SETPOS, &abd);
    return abd.rc;
}
void AppBar::remove(HWND hwnd) {
    if (!m_registered) {
        return;
    }
    APPBARDATA abd = makeData(hwnd);
    SHAppBarMessage(ABM_REMOVE, &abd);
    m_registered = false;
}