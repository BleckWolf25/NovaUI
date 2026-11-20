/**
 * @file TaskbarUI.h
 *
 * @version 1.8.0
 * @author BleckWolf25
 * @license MIT
 *
 * @summary Floating Liquid Glass dock replacing the native Windows taskbar on any screen edge
 *
 * @description
 * Owns the dock window and wires the shell together: preference snapshot and restore guard,
 * ShellTaskbar suppression, AppBar reservation on the configured edge, the task and tray
 * trackers, the clock, the flyout, the visibility guard and the optional music visualizer.
 * Windows personalization drives metrics, alignment and which shell slots appear. Input is
 * mapped into the layout's long-axis space so one gesture path serves horizontal and vertical
 * docks, and flyouts are anchored on the bar's inner edge.
 *
 * @since 17/11/2026
 * @updated 20/11/2026
 */
#pragma once
// ---------- IMPORTS
#include "Core/AppBar.h"
#include "Core/ShellTaskbar.h"
#include "Core/TaskbarSettings.h"
#include "Core/Window.h"
#include "Modules/ClockWidget.h"
#include "Modules/MusicVisualizer.h"
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
    FlyoutSide flyoutSide() const;
    POINT anchorFromAlong(float along) const;
    void applyClockAlignment();
    void dock();
    void toggleVisualizer();
    void showBarContextMenu(POINT anchorScreen);
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
    MusicVisualizer m_visualizer;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> m_timeFormat;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> m_dateFormat;
    TaskbarSettings m_settings{};
    float m_barLengthDip = 0.0f;
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