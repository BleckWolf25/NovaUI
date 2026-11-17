/**
 * @file TaskbarUI.h
 *
 * @version 1.4.0
 * @author BleckWolf25
 * @license MIT
 *
 * @summary Floating Liquid Glass dock replacing the native Windows taskbar
 *
 * @description
 * Suppresses Explorer's taskbar, reserves the bottom screen edge via AppBar, and renders a
 * centered floating stadium-pill dock with multi-layered Liquid Glass specular highlights,
 * Fresnel rim gradients, interactive app lenses and a clickable clock cluster.
 *
 * @since 17/11/2026
 * @updated 17/11/2026
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

// ---------- DECLARATIONS
class TaskbarUI final : public Window {
public:
    ~TaskbarUI() override;
    bool start();

protected:
    LRESULT onMessage(UINT msg, WPARAM wp, LPARAM lp) override;

private:
    enum class HitZone { None, Start, App, Tray, TrayOverflow, Clock };

    struct HitResult {
        HitZone zone = HitZone::None;
        int index = -1;

        bool operator==(const HitResult& other) const noexcept {
            return zone == other.zone && index == other.index;
        }
        bool operator!=(const HitResult& other) const noexcept {
            return !(*this == other);
        }
    };

    void dock();
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
    HitResult m_hover{};
    bool m_trackingMouse = false;

    // Drag-and-drop state
    bool m_pointerDown = false;
    bool m_dragging = false;
    POINT m_dragStartPx{};
    HitZone m_dragZone = HitZone::None;
    int m_dragFromIndex = -1;
    int m_dragToIndex = -1;
    float m_dragDeltaDip = 0.0f;
};