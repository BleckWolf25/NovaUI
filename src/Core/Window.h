/**
 * @file Window.h
 *
 * @version 1.0.0
 * @author BleckWolf25
 * @license MIT
 *
 * @summary Base Win32 window wrapper with message dispatch and DWM backdrop support
 *
 * @description
 * Registers a window class, creates the HWND and routes every message to the virtual onMessage
 * handler of the owning object through GWLP_USERDATA. Exceptions never cross the window
 * procedure boundary. applyBackdrop enables the dark Acrylic system backdrop through
 * DwmSetWindowAttribute and DwmExtendFrameIntoClientArea.
 *
 * @since 17/11/2026
 * @updated 17/11/2026
 */
#pragma once
// ---------- IMPORTS
#include <Windows.h>

// ---------- DECLARATIONS
class Window {
public:
    Window() = default;
    virtual ~Window() = default;
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    HWND hwnd() const noexcept { return m_hwnd; }

protected:
    bool create(const wchar_t* className, DWORD style, DWORD exStyle);
    void applyBackdrop() const;
    virtual LRESULT onMessage(UINT msg, WPARAM wp, LPARAM lp);

    HWND m_hwnd = nullptr;

private:
    static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
};
