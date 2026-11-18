/**
 * @file FlyoutDismissHook.cpp
 *
 * @version 1.0.0
 * @author BleckWolf25
 * @license MIT
 *
 * @summary Implementation of the popup outside-dismiss mouse hook
 *
 * @description
 * Installs and removes a WH_MOUSE_LL hook bound to the active popup. The callback performs only
 * two GetWindowRect calls, a UnionRect, a PtInRect test and a PostMessageW, keeping low-level
 * mouse latency negligible, and unconditionally chains to CallNextHookEx.
 *
 * @since 18/11/2026
 * @updated 18/11/2026
 */
// ---------- IMPORTS
#include "Modules/FlyoutDismissHook.h"
// ---------- IMPLEMENTATION
FlyoutDismissHook* FlyoutDismissHook::s_active = nullptr;

FlyoutDismissHook::~FlyoutDismissHook() {
    disarm();
}

void FlyoutDismissHook::arm(HWND popup, HWND anchor, UINT dismissMessage) {
    if (m_hook || !popup) {
        return;
    }
    m_popup = popup;
    m_anchor = anchor;
    m_dismissMessage = dismissMessage;
    s_active = this;
    m_hook = SetWindowsHookExW(WH_MOUSE_LL, &FlyoutDismissHook::mouseHook,
                               GetModuleHandleW(nullptr), 0);
    if (!m_hook) {
        s_active = nullptr;
    }
}

void FlyoutDismissHook::disarm() {
    if (m_hook) {
        UnhookWindowsHookEx(m_hook);
        m_hook = nullptr;
    }
    if (s_active == this) {
        s_active = nullptr;
    }
    m_popup = nullptr;
    m_anchor = nullptr;
    m_dismissMessage = 0;
}

LRESULT CALLBACK FlyoutDismissHook::mouseHook(int code, WPARAM wp, LPARAM lp) {
    if (code == HC_ACTION && s_active && s_active->m_popup) {
        const auto* info = reinterpret_cast<MSLLHOOKSTRUCT*>(lp);
        RECT popupRect{};
        RECT anchorRect{};
        GetWindowRect(s_active->m_popup, &popupRect);
        if (s_active->m_anchor) {
            GetWindowRect(s_active->m_anchor, &anchorRect);
        } else {
            anchorRect = popupRect;
        }
        RECT keepAlive{};
        UnionRect(&keepAlive, &popupRect, &anchorRect);
        const DWORD message = static_cast<DWORD>(wp);
        const bool isPress = message == WM_LBUTTONDOWN || message == WM_RBUTTONDOWN ||
                             message == WM_MBUTTONDOWN;
        if (!PtInRect(&keepAlive, info->pt) && (isPress || message == WM_MOUSEMOVE)) {
            PostMessageW(s_active->m_popup, s_active->m_dismissMessage, 0, 0);
        }
    }
    return CallNextHookEx(nullptr, code, wp, lp);
}