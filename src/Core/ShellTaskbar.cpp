/**
- @file ShellTaskbar.cpp
 *
- @version 1.0.0
- @author BleckWolf25
- @license MIT
 *
- @summary Implementation of native Windows taskbar suppression and restoration
 *
- @description
- Finds primary and secondary Explorer taskbars, records the previous AppBar state, enables
- ABS_AUTOHIDE on Shell_TrayWnd so maximized windows expand to NovaUI's boundary, and keeps
- native bars hidden via EVENT_OBJECT_SHOW.
 *
- @since 17/11/2026
- @updated 17/11/2026
 */
// ---------- IMPORTS
#include "Core/ShellTaskbar.h"

#include <shellapi.h>
#include <cwchar>

// ---------- IMPLEMENTATION
ShellTaskbar::~ShellTaskbar() {
    restore();
}

void ShellTaskbar::suppress() {
    if (m_suppressed) {
        return;
    }

    if (HWND primary = FindWindowW(L"Shell_TrayWnd", nullptr)) {
        APPBARDATA abd{};
        abd.cbSize = sizeof(abd);
        abd.hWnd = primary;
        m_previousState = SHAppBarMessage(ABM_GETSTATE, &abd);

        abd.lParam = ABS_AUTOHIDE;
        SHAppBarMessage(ABM_SETSTATE, &abd);
    }

    hideAllTrays();

    m_showHook = SetWinEventHook(EVENT_OBJECT_SHOW, EVENT_OBJECT_SHOW, nullptr,
                                 &ShellTaskbar::onWinEvent, 0, 0,
                                 WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
    m_suppressed = true;
}

void ShellTaskbar::restore() {
    if (!m_suppressed) {
        return;
    }

    if (m_showHook) {
        UnhookWinEvent(m_showHook);
        m_showHook = nullptr;
    }

    if (HWND primary = FindWindowW(L"Shell_TrayWnd", nullptr)) {
        APPBARDATA abd{};
        abd.cbSize = sizeof(abd);
        abd.hWnd = primary;
        abd.lParam = static_cast<LPARAM>(m_previousState);
        SHAppBarMessage(ABM_SETSTATE, &abd);
    }

    showAllTrays();
    m_suppressed = false;
}

void ShellTaskbar::openStartMenu() {
    INPUT inputs[2]{};
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = VK_LWIN;

    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = VK_LWIN;
    inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;

    SendInput(2, inputs, sizeof(INPUT));
}

void ShellTaskbar::hideAllTrays() {
    if (HWND primary = FindWindowW(L"Shell_TrayWnd", nullptr)) {
        ShowWindow(primary, SW_HIDE);
    }
    HWND secondary = nullptr;
    while ((secondary = FindWindowExW(nullptr, secondary, L"Shell_SecondaryTrayWnd", nullptr)) != nullptr) {
        ShowWindow(secondary, SW_HIDE);
    }
}

void ShellTaskbar::showAllTrays() {
    if (HWND primary = FindWindowW(L"Shell_TrayWnd", nullptr)) {
        ShowWindow(primary, SW_SHOW);
    }
    HWND secondary = nullptr;
    while ((secondary = FindWindowExW(nullptr, secondary, L"Shell_SecondaryTrayWnd", nullptr)) != nullptr) {
        ShowWindow(secondary, SW_SHOW);
    }
}

bool ShellTaskbar::isNativeTray(HWND hwnd) {
    if (!hwnd) {
        return false;
    }
    wchar_t className[64]{};
    if (GetClassNameW(hwnd, className, 64) == 0) {
        return false;
    }
    return std::wcscmp(className, L"Shell_TrayWnd") == 0 ||
           std::wcscmp(className, L"Shell_SecondaryTrayWnd") == 0;
}

void CALLBACK ShellTaskbar::onWinEvent(HWINEVENTHOOK, DWORD event, HWND hwnd, LONG idObject,
                                       LONG idChild, DWORD, DWORD) {
    if (event == EVENT_OBJECT_SHOW && idObject == OBJID_WINDOW && idChild == CHILDID_SELF &&
        isNativeTray(hwnd)) {
        ShowWindow(hwnd, SW_HIDE);
    }
}