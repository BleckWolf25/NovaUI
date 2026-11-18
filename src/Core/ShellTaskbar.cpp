/**
 * @file ShellTaskbar.cpp
 *
 * @version 1.2.0
 * @author BleckWolf25
 * @license MIT
 *
 * @summary Implementation of native taskbar suppression, preference restore and shell shortcuts
 *
 * @description
 * Finds primary and secondary Explorer taskbars, records the previous AppBar state and the raw
 * StuckRects3 Settings blob, enables ABS_AUTOHIDE on Shell_TrayWnd so maximized windows expand to
 * NovaUI's boundary, and keeps native bars hidden via EVENT_OBJECT_SHOW. Restore writes the saved
 * blob back so the user's auto-hide preference survives a NovaUI session. Shell shortcuts are
 * synthesized with SendInput or launched through ShellExecuteW.
 *
 * @since 17/11/2026
 * @updated 18/11/2026
 */
// ---------- IMPORTS
#include "Core/ShellTaskbar.h"
#include <shellapi.h>
#include <cwchar>
// ---------- CONSTANTS
namespace {
constexpr wchar_t kAdvancedKey[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced";
constexpr wchar_t kStuckRectsKey[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StuckRects3";

void sendChord(WORD modifier, WORD key) {
    INPUT inputs[4]{};
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = modifier;
    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = key;
    inputs[2].type = INPUT_KEYBOARD;
    inputs[2].ki.wVk = key;
    inputs[2].ki.dwFlags = KEYEVENTF_KEYUP;
    inputs[3].type = INPUT_KEYBOARD;
    inputs[3].ki.wVk = modifier;
    inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(4, inputs, sizeof(INPUT));
}

void launchUri(const wchar_t* uri) {
    SHELLEXECUTEINFOW sei{};
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_FLAG_NO_UI;
    sei.lpVerb = L"open";
    sei.lpFile = uri;
    sei.nShow = SW_SHOWNORMAL;
    ShellExecuteExW(&sei);
}
}
// ---------- IMPLEMENTATION
ShellTaskbar::~ShellTaskbar() {
    restore();
}

void ShellTaskbar::suppress() {
    if (m_suppressed) {
        return;
    }
    captureAutoHidePreference();
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
    restoreAutoHidePreference();
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

void ShellTaskbar::openNotificationCenter() {
    sendChord(VK_LWIN, 'N');
}

void ShellTaskbar::openTaskView() {
    sendChord(VK_LWIN, VK_TAB);
}

void ShellTaskbar::openTaskbarSettings() {
    launchUri(L"ms-settings:taskbar");
}

void ShellTaskbar::openTaskManager() {
    launchUri(L"taskmgr.exe");
}

bool ShellTaskbar::isTaskViewEnabled() {
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kAdvancedKey, 0, KEY_READ, &key) != ERROR_SUCCESS) {
        return true;
    }
    DWORD value = 1;
    DWORD size = sizeof(value);
    DWORD type = REG_DWORD;
    const LSTATUS status = RegQueryValueExW(key, L"ShowTaskViewButton", nullptr, &type,
                                            reinterpret_cast<BYTE*>(&value), &size);
    RegCloseKey(key);
    if (status != ERROR_SUCCESS || type != REG_DWORD) {
        return true;
    }
    return value != 0;
}

void ShellTaskbar::captureAutoHidePreference() {
    m_stuckRects.clear();
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kStuckRectsKey, 0, KEY_READ, &key) != ERROR_SUCCESS) {
        return;
    }
    DWORD type = 0;
    DWORD bytes = 0;
    if (RegQueryValueExW(key, L"Settings", nullptr, &type, nullptr, &bytes) == ERROR_SUCCESS &&
        type == REG_BINARY && bytes > 0) {
        m_stuckRects.resize(bytes);
        if (RegQueryValueExW(key, L"Settings", nullptr, nullptr, m_stuckRects.data(), &bytes) !=
            ERROR_SUCCESS) {
            m_stuckRects.clear();
        }
    }
    RegCloseKey(key);
}

void ShellTaskbar::restoreAutoHidePreference() const {
    if (m_stuckRects.empty()) {
        return;
    }
    HKEY key = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kStuckRectsKey, 0, nullptr, 0, KEY_SET_VALUE, nullptr,
                        &key, nullptr) != ERROR_SUCCESS) {
        return;
    }
    RegSetValueExW(key, L"Settings", 0, REG_BINARY, m_stuckRects.data(),
                   static_cast<DWORD>(m_stuckRects.size()));
    RegCloseKey(key);
}

void ShellTaskbar::hideAllTrays() {
    if (HWND primary = FindWindowW(L"Shell_TrayWnd", nullptr)) {
        ShowWindow(primary, SW_HIDE);
    }
    HWND secondary = nullptr;
    while ((secondary = FindWindowExW(nullptr, secondary, L"Shell_SecondaryTrayWnd", nullptr)) !=
           nullptr) {
        ShowWindow(secondary, SW_HIDE);
    }
}

void ShellTaskbar::showAllTrays() {
    if (HWND primary = FindWindowW(L"Shell_TrayWnd", nullptr)) {
        ShowWindow(primary, SW_SHOW);
    }
    HWND secondary = nullptr;
    while ((secondary = FindWindowExW(nullptr, secondary, L"Shell_SecondaryTrayWnd", nullptr)) !=
           nullptr) {
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