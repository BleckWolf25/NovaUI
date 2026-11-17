/**
- @file ShellTaskbar.h
 *
- @version 1.0.0
- @author BleckWolf25
- @license MIT
 *
- @summary Suppresses and restores the native Windows Explorer taskbar
 *
- @description
- Hides Shell_TrayWnd and Shell_SecondaryTrayWnd, switches Explorer's AppBar state to auto-hide
- so the desktop work area is surrendered to NovaUI, and installs a zero-polling WinEvent hook
- to keep Explorer from re-showing its taskbar until NovaUI exits.
 *
- @since 17/11/2026
- @updated 17/11/2026
 */
#pragma once
// ---------- IMPORTS
#include <Windows.h>

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

private:
    static void hideAllTrays();
    static void showAllTrays();
    static bool isNativeTray(HWND hwnd);
    static void CALLBACK onWinEvent(HWINEVENTHOOK hook, DWORD event, HWND hwnd, LONG idObject,
                                    LONG idChild, DWORD eventThread, DWORD eventTime);

    bool m_suppressed = false;
    UINT_PTR m_previousState = 0;
    HWINEVENTHOOK m_showHook = nullptr;
};