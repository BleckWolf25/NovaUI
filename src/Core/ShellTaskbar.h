/**
 * @file ShellTaskbar.h
 *
 * @version 1.3.0
 * @author BleckWolf25
 * @license MIT
 *
 * @summary Suppresses and restores the native Windows Explorer taskbar and shell shortcuts
 *
 * @description
 * Hides Shell_TrayWnd and Shell_SecondaryTrayWnd, switches Explorer's AppBar state to auto-hide
 * so the desktop work area is surrendered to NovaUI, and installs a zero-polling WinEvent hook
 * to keep Explorer from re-showing its taskbar until NovaUI exits. Captures the user's persisted
 * StuckRects3 auto-hide preference and writes it back verbatim on restore. Exposes shell launch
 * helpers for Start, Task View, Notification Center, Taskbar settings and Task Manager, plus a
 * one-shot read of the ShowTaskViewButton personalization setting.
 *
 * @since 17/11/2026
 * @updated 20/11/2026
 */
#pragma once
// ---------- IMPORTS
#include <Windows.h>
#include <vector>
// ---------- DECLARATIONS
class ShellTaskbar {
public:
    ShellTaskbar() = default;
    ~ShellTaskbar();
    ShellTaskbar(const ShellTaskbar&) = delete;
    ShellTaskbar& operator=(const ShellTaskbar&) = delete;
    void suppress();
    void restore();
    static void openStartMenu();
    static void openNotificationCenter();
    static void openTaskView();
    static void openTaskbarSettings();
    static void openTaskManager();
    static bool isTaskViewEnabled();
    static void openSearch();
    static void openWidgets();
    static void openResume();
private:
    static void hideAllTrays();
    static void showAllTrays();
    static bool isNativeTray(HWND hwnd);
    static void CALLBACK onWinEvent(HWINEVENTHOOK hook, DWORD event, HWND hwnd, LONG idObject,
                                    LONG idChild, DWORD eventThread, DWORD eventTime);
    void captureAutoHidePreference();
    void restoreAutoHidePreference() const;
    bool m_suppressed = false;
    UINT_PTR m_previousState = 0;
    HWINEVENTHOOK m_showHook = nullptr;
    std::vector<BYTE> m_stuckRects;
};