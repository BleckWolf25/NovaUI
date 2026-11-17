/**
 * @file TaskbarUI.h
 *
 * @version 1.0.0
 * @author BleckWolf25
 * @license MIT
 *
 * @summary Bottom-docked taskbar window with Acrylic backdrop, launcher mark and clock
 *
 * @description
 * Composes the AppBar reservation, the Direct2D renderer and the clock widget into one window.
 * The window repaints only when invalidated by a resize, DPI change or clock tick, and reserves
 * its screen edge so maximized windows respect its boundary.
 *
 * @since 17/11/2026
 * @updated 17/11/2026
 */
#pragma once
// ---------- IMPORTS
#include "Core/AppBar.h"
#include "Core/Window.h"
#include "Modules/ClockWidget.h"
#include "Render/Renderer.h"

// ---------- DECLARATIONS
class TaskbarUI final : public Window {
public:
    ~TaskbarUI() override;
    bool start();

protected:
    LRESULT onMessage(UINT msg, WPARAM wp, LPARAM lp) override;

private:
    void dock();
    void paint();

    AppBar m_appBar;
    Renderer m_renderer;
    ClockWidget m_clock;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> m_glyphFormat;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> m_timeFormat;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> m_dateFormat;
};
