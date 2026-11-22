/**
 * @file TaskbarGuard.cpp
 *
 * @version 1.0.0
 * @author BleckWolf25
 * @license MIT
 *
 * @summary Implementation of the dock visibility guard and topmost re-assertion
 *
 * @description
 * Registers four WinEvent ranges: own-process minimize, own-process hide, global foreground and
 * global desktop switch. The callback filters to the guarded HWND or to known shell overlay
 * classes and posts the owner's message; all window work happens on the message loop.
 * reassertTaskbarWindow un-minimizes and re-tops the bar without activating it.
 *
 * @since 18/11/2026
 * @updated 18/11/2026
 */
// ---------- IMPORTS
#include "Modules/TaskbarGuard.h"
#include <cwchar>
// ---------- IMPLEMENTATION
TaskbarGuard* TaskbarGuard::s_instance = nullptr;

TaskbarGuard::~TaskbarGuard() {
    stop();
}

void TaskbarGuard::start(HWND target, UINT message) {
    if (!target || m_target) {
        return;
    }
    m_target = target;
    m_message = message;
    s_instance = this;
    const DWORD ownPid = GetCurrentProcessId();
    const DWORD ownFlags = WINEVENT_OUTOFCONTEXT;
    const DWORD globalFlags = WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS;
    if (HWINEVENTHOOK hook =
            SetWinEventHook(EVENT_SYSTEM_MINIMIZESTART, EVENT_SYSTEM_MINIMIZEEND, nullptr,
                            &TaskbarGuard::onWinEvent, ownPid, 0, ownFlags)) {
        m_hooks.push_back(hook);
    }
    if (HWINEVENTHOOK hook = SetWinEventHook(EVENT_OBJECT_HIDE, EVENT_OBJECT_HIDE, nullptr,
                                             &TaskbarGuard::onWinEvent, ownPid, 0, ownFlags)) {
        m_hooks.push_back(hook);
    }
    if (HWINEVENTHOOK hook =
            SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, nullptr,
                            &TaskbarGuard::onWinEvent, 0, 0, globalFlags)) {
        m_hooks.push_back(hook);
    }
    if (HWINEVENTHOOK hook =
            SetWinEventHook(EVENT_SYSTEM_DESKTOPSWITCH, EVENT_SYSTEM_DESKTOPSWITCH, nullptr,
                            &TaskbarGuard::onWinEvent, 0, 0, globalFlags)) {
        m_hooks.push_back(hook);
    }
}

void TaskbarGuard::stop() {
    for (HWINEVENTHOOK hook : m_hooks) {
        UnhookWinEvent(hook);
    }
    m_hooks.clear();
    m_target = nullptr;
    m_message = 0;
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

bool TaskbarGuard::isShellOverlay(HWND hwnd) {
    if (!hwnd) {
        return false;
    }
    wchar_t className[64]{};
    if (GetClassNameW(hwnd, className, 64) == 0) {
        return false;
    }
    return std::wcscmp(className, L"XamlExplorerHostIslandWindow") == 0 ||
           std::wcscmp(className, L"ForegroundStaging") == 0 ||
           std::wcscmp(className, L"Windows.UI.Core.CoreWindow") == 0;
}

void CALLBACK TaskbarGuard::onWinEvent(HWINEVENTHOOK, DWORD event, HWND hwnd, LONG idObject,
                                       LONG idChild, DWORD, DWORD) {
    if (!s_instance || !s_instance->m_target) {
        return;
    }
    if (event == EVENT_SYSTEM_MINIMIZESTART || event == EVENT_OBJECT_HIDE) {
        if (hwnd == s_instance->m_target && idObject == OBJID_WINDOW && idChild == CHILDID_SELF) {
            PostMessageW(s_instance->m_target, s_instance->m_message, 0, 0);
        }
        return;
    }
    if (event == EVENT_SYSTEM_DESKTOPSWITCH || isShellOverlay(hwnd)) {
        PostMessageW(s_instance->m_target, s_instance->m_message, 0, 0);
    }
}

void reassertTaskbarWindow(HWND hwnd) {
    if (!hwnd) {
        return;
    }
    if (!IsWindowVisible(hwnd) || IsIconic(hwnd)) {
        ShowWindow(hwnd, SW_SHOWNOACTIVATE);
    }
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}