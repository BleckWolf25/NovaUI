/**
 * @file FlyoutDismissHook.h
 *
 * @version 1.0.0
 * @author BleckWolf25
 * @license MIT
 *
 * @summary RAII low-level mouse hook that dismisses a popup when the pointer leaves its region
 *
 * @description
 * Arms a WH_MOUSE_LL hook only while an interactive popup is visible. The keep-alive region is
 * the union of the popup and its anchor window, so crossing the gap between dock and flyout does
 * not dismiss it. On a pointer move or button press outside that union the callback posts the
 * owner's dismiss message and always forwards through CallNextHookEx. The hook is released by
 * disarm and by the destructor, leaving zero installed hooks while the shell is idle.
 *
 * @since 18/11/2026
 * @updated 18/11/2026
 */
#pragma once
// ---------- IMPORTS
#include <Windows.h>
// ---------- DECLARATIONS
class FlyoutDismissHook {
public:
    FlyoutDismissHook() = default;
    ~FlyoutDismissHook();
    FlyoutDismissHook(const FlyoutDismissHook&) = delete;
    FlyoutDismissHook& operator=(const FlyoutDismissHook&) = delete;
    void arm(HWND popup, HWND anchor, UINT dismissMessage);
    void disarm();
private:
    static LRESULT CALLBACK mouseHook(int code, WPARAM wp, LPARAM lp);
    HHOOK m_hook = nullptr;
    HWND m_popup = nullptr;
    HWND m_anchor = nullptr;
    UINT m_dismissMessage = 0;
    static FlyoutDismissHook* s_active;
};