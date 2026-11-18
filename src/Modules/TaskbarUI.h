/**
 * @file TaskbarUI.h
 *
 * @version 1.5.0
 * @author BleckWolf25
 * @license MIT
 *
 * @summary Floating Liquid Glass dock replacing the native Windows taskbar
 *
 * @description
 * Suppresses Explorer's taskbar, reserves the bottom screen edge via AppBar, and renders a
 * centered floating dock with Liquid Glass specular highlights, app lenses, an optional Task
 * View button, a tray cluster and a clickable clock. A single shared layout drives hit-testing,
 * painting and drag math so every drawn slot is reachable. An event-driven visibility guard
 * re-asserts the bar after Show Desktop and shell overlays, and empty-area right-click opens a
 * native menu for Taskbar settings and Task Manager.
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
#include "Modules/TrayTracker.h"
#include "Render/Renderer.h"
#include <vector>
// ---------- DECLARATIONS
class TaskbarUI final : public Window {
public:
    ~TaskbarUI() override;
    bool start();
protected:
    LRESULT onMessage(UINT msg, WPARAM wp, LPARAM lp) override;
private:
    enum class HitZone { None, Start, TaskView, App, Tray, TrayOverflow, Clock };
    struct HitResult {
        HitZone zone = HitZone::None;
        int index = -1;
        bool operator==(const HitResult& other) const noexcept {
            return zone == other.zone && index == other.index;
        }
        bool operator!=(const HitResult& other) const noexcept { return !(*this == other); }
    };
    struct Layout {
        float startLeft = 0.0f;
        float taskViewLeft = 0.0f;
        float appsStartX = 0.0f;
        float maxAppX = 0.0f;
        float trayLeft = 0.0f;
        float chevronLeft = 0.0f;
        float traySectionRight = 0.0f;
        float clockLeft = 0.0f;
        float clockRight = 0.0f;
        float slotTop = 0.0f;
        float traySlotTop = 0.0f;
    };
    Layout computeLayout(float widthDip, float heightDip) const;
    void dock();
    void installGuard();
    void removeGuard();
    void reassert();
    void showBarContextMenu(int screenX, int screenY);
    HitResult hitTest(float dipX, float dipY) const;
    float slotCenterDipX(const HitResult& hit) const;
    int screenXFromDip(float dipX) const;
    int taskbarTopScreen() const;
    void showHoverFlyout(const HitResult& hit);
    void showNativeSystemMenu(HWND target, int screenX, int screenY);
    void onMouseMove(int xPx, int yPx);
    void onLeftDown(int xPx, int yPx);
    void onLeftUp(int xPx, int yPx);
    void onLeftDoubleClick(int xPx, int yPx);
    void onRightUp(int xPx, int yPx);
    void onMiddleUp(int xPx, int yPx);
    void paint();
    static void CALLBACK onGuardEvent(HWINEVENTHOOK hook, DWORD event, HWND hwnd, LONG idObject,
                                      LONG idChild, DWORD eventThread, DWORD eventTime);
    ShellTaskbar m_shellTaskbar;
    AppBar m_appBar;
    Renderer m_renderer;
    TaskTracker m_tasks;
    TrayTracker m_tray;
    TaskbarFlyout m_flyout;
    ClockWidget m_clock;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> m_timeFormat;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> m_dateFormat;
    float m_barWidthDip = 0.0f;
    bool m_showTaskView = true;
    HitResult m_hover{};
    bool m_trackingMouse = false;
    bool m_pointerDown = false;
    bool m_dragging = false;
    bool m_dragWasPinned = false;
    POINT m_dragStartPx{};
    HitZone m_dragZone = HitZone::None;
    int m_dragFromIndex = -1;
    int m_dragToIndex = -1;
    float m_dragDeltaDip = 0.0f;
    std::vector<HWINEVENTHOOK> m_guardHooks;
    static TaskbarUI* s_instance;
};