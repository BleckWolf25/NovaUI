/**
- @file TaskbarUI.h
 *
- @version 1.1.0
- @author BleckWolf25
- @license MIT
 *
- @summary Bottom-docked glass taskbar replacing the native Windows shell bar
 *
- @description
- Suppresses Explorer's taskbar, reserves the bottom screen edge via AppBar, renders a sleek
- translucent glass bar with DirectComposition and handles interactive hover and click events
- for the Start launcher, running windows and minute-aligned clock.
 *
- @since 17/11/2026
- @updated 17/11/2026
 */
#pragma once
// ---------- IMPORTS
#include "Core/AppBar.h"
#include "Core/ShellTaskbar.h"
#include "Core/Window.h"
#include "Modules/ClockWidget.h"
#include "Modules/TaskTracker.h"
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
    int hitTestSlot(float dipX, float dipY) const;
    void onMouseMove(int xPx, int yPx);
    void onClick(int xPx, int yPx);

    ShellTaskbar m_shellTaskbar;
    AppBar m_appBar;
    Renderer m_renderer;
    TaskTracker m_tasks;
    ClockWidget m_clock;

    Microsoft::WRL::ComPtr<IDWriteTextFormat> m_glyphFormat;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> m_timeFormat;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> m_dateFormat;

    int m_hoverSlot = -2; // -2: none, -1: Start button, >= 0: task index
    bool m_trackingMouse = false;
};