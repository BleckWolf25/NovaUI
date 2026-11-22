/**
 * @file TaskbarGuard.h
 *
 * @version 1.0.0
 * @author BleckWolf25
 * @license MIT
 *
 * @summary Event-driven visibility guard that keeps the replacement dock on screen
 *
 * @description
 * Installs out-of-context WinEvent hooks for the process's own minimize and hide events, for
 * shell overlay foreground changes (Task View, Alt-Tab staging, CoreWindow) and for virtual
 * desktop switches. The callback only posts a caller-supplied WM_APP message, never touching
 * window state directly. reassertTaskbarWindow restores and re-tops the dock in response.
 * Hooks are released by stop and by the destructor; nothing polls while the shell is idle.
 *
 * @since 18/11/2026
 * @updated 18/11/2026
 */
#pragma once
// ---------- IMPORTS
#include <Windows.h>
#include <vector>
// ---------- DECLARATIONS
class TaskbarGuard {
public:
    TaskbarGuard() = default;
    ~TaskbarGuard();
    TaskbarGuard(const TaskbarGuard&) = delete;
    TaskbarGuard& operator=(const TaskbarGuard&) = delete;
    void start(HWND target, UINT message);
    void stop();
private:
    static bool isShellOverlay(HWND hwnd);
    static void CALLBACK onWinEvent(HWINEVENTHOOK hook, DWORD event, HWND hwnd, LONG idObject,
                                    LONG idChild, DWORD eventThread, DWORD eventTime);
    HWND m_target = nullptr;
    UINT m_message = 0;
    std::vector<HWINEVENTHOOK> m_hooks;
    static TaskbarGuard* s_instance;
};

void reassertTaskbarWindow(HWND hwnd);