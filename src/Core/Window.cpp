/**
 * @file Window.cpp
 *
 * @version 1.1.0
 * @author BleckWolf25
 * @license MIT
 *
 * @summary Implementation of the base Win32 window wrapper
 *
 * @description
 * Binds the HWND to its owning Window instance during WM_NCCREATE, dispatches messages to
 * onMessage inside a try/catch so no exception reaches the OS, and unbinds on WM_NCDESTROY.
 * applyBackdrop enables borderless dark Acrylic translucency for Liquid Glass compositing.
 *
 * @since 17/11/2026
 * @updated 17/11/2026
 */
// ---------- IMPORTS
#include "Core/Window.h"

#include <dwmapi.h>

// ---------- CONSTANTS
namespace {
constexpr auto kDwmBorderColorAttr = static_cast<DWMWINDOWATTRIBUTE>(34);
constexpr COLORREF kDwmColorNone = 0xFFFFFFFEu;

enum class WindowCompositionAttribute : DWORD {
    AccentPolicy = 19,
};

struct AccentPolicy {
    DWORD accentState;
    DWORD accentFlags;
    DWORD gradientColor;
    DWORD animationId;
};

struct WindowCompositionAttribData {
    WindowCompositionAttribute attribute;
    void* data;
    SIZE_T sizeOfData;
};

constexpr DWORD kLiquidGlassAcrylicTint = 0x0414100Cu;
}

// ---------- IMPLEMENTATION
bool Window::create(const wchar_t* className, DWORD style, DWORD exStyle) {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = &Window::wndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = className;
    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }

    return CreateWindowExW(exStyle, className, L"NovaUI", style, 0, 0, 0, 0, nullptr, nullptr,
                           wc.hInstance, this) != nullptr;
}

void Window::applyBackdrop() const {
    const BOOL dark = TRUE;
    const DWORD corners = DWMWCP_DONOTROUND;
    const MARGINS margins{-1, -1, -1, -1};

    DwmSetWindowAttribute(m_hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
    DwmSetWindowAttribute(m_hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &corners, sizeof(corners));
    DwmSetWindowAttribute(m_hwnd, kDwmBorderColorAttr, &kDwmColorNone, sizeof(kDwmColorNone));
    DwmExtendFrameIntoClientArea(m_hwnd, &margins);

    using SetWindowCompositionAttributeFn = BOOL(WINAPI*)(HWND, WindowCompositionAttribData*);
    if (HMODULE user32 = GetModuleHandleW(L"user32.dll")) {
        void* rawProc =
            reinterpret_cast<void*>(GetProcAddress(user32, "SetWindowCompositionAttribute"));
        if (rawProc) {
            auto setCompositionAttribute =
                reinterpret_cast<SetWindowCompositionAttributeFn>(rawProc);
            WindowCompositionAttribData data{WindowCompositionAttribute::AccentPolicy};
            setCompositionAttribute(m_hwnd, &data);
        }
    }
}

LRESULT Window::onMessage(UINT msg, WPARAM wp, LPARAM lp) {
    return DefWindowProcW(m_hwnd, msg, wp, lp);
}

LRESULT CALLBACK Window::wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    Window* self = nullptr;
    if (msg == WM_NCCREATE) {
        self = static_cast<Window*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);
        self->m_hwnd = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<Window*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }
    if (!self) {
        return DefWindowProcW(hwnd, msg, wp, lp);
    }

    LRESULT result = 0;
    try {
        result = self->onMessage(msg, wp, lp);
    } catch (...) {
        result = DefWindowProcW(hwnd, msg, wp, lp);
    }

    if (msg == WM_NCDESTROY) {
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        self->m_hwnd = nullptr;
    }
    return result;
}