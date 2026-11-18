/**
 * @file TaskbarUI.h
 *
 * @version 1.6.0
 * @author BleckWolf25
 * @license MIT
 *
 * @summary Floating Liquid Glass dock replacing the native Windows taskbar
 *
 * @description
 * Owns the dock window and wires the shell together: ShellTaskbar suppression, AppBar edge
 * reservation, the task and tray trackers, the clock, the flyout and the visibility guard.
 * Geometry comes from TaskbarLayout, frames from paintTaskbar and shell shortcuts from
 * ShellTaskbar, so this type holds only window lifetime, message routing and input policy.
 *
 * @since 17/11/2026
 * @updated 18/11/2026
 */
#pragma once
// ---------- IMPORTS
#include "Core/AppBar.h"
#include "Core/ShellTaskbar.h"
#include "Core/Window.h"
#include "Modules/ClockWidget.h"
#include "Modules/TaskTracker.h"
#include "Modules/TaskbarFlyout.h"
#include "Modules/TaskbarGuard.h"
#include "Modules/TaskbarLayout.h"
#include "Modules/TaskbarPaint.h"
#include "Modules/TrayTracker.h"
#include "Render/Renderer.h"
// ---------- DECLARATIONS
class TaskbarUI final : public Window {
public:
    ~TaskbarUI() override;
    bool start();
protected:
    LRESULT onMessage(UINT msg, WPARAM wp, LPARAM lp) override;
private:
    TaskbarLayout currentLayout() const;
    void dock();
    void showBarContextMenu(int screenX, int screenY);
    int screenXFromDip(float dipX) const;
    int taskbarTopScreen() const;
    void showHoverFlyout(const TaskbarHit& hit);
    void showNativeSystemMenu(HWND target, int screenX, int screenY);
    void onMouseMove(int xPx, int yPx);
    void onLeftDown(int xPx, int yPx);
    void onLeftUp(int xPx, int yPx);
    void onLeftDoubleClick(int xPx, int yPx);
    void onRightUp(int xPx, int yPx);
    void onMiddleUp(int xPx, int yPx);
    void paint();
    ShellTaskbar m_shellTaskbar;
    AppBar m_appBar;
    Renderer m_renderer;
    TaskTracker m_tasks;
    TrayTracker m_tray;
    TaskbarFlyout m_flyout;
    ClockWidget m_clock;
    TaskbarGuard m_guard;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> m_timeFormat;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> m_dateFormat;
    float m_barWidthDip = 0.0f;
    bool m_showTaskView = true;
    TaskbarHit m_hover{};
    bool m_trackingMouse = false;
    bool m_pointerDown = false;
    bool m_dragging = false;
    bool m_dragWasPinned = false;
    POINT m_dragStartPx{};
    TaskbarZone m_dragZone = TaskbarZone::None;
    int m_dragFromIndex = -1;
    int m_dragToIndex = -1;
    float m_dragDeltaDip = 0.0f;
};