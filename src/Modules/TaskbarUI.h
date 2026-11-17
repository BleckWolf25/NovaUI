/**
 * @file TaskbarUI.h
 *
 * @version 1.2.0
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
#include "Render/Renderer.h"

// ---------- DECLARATIONS
class TaskbarUI final : public Window {
public:
    ~TaskbarUI() override;
    bool start();

protected:
    LRESULT onMessage(UINT msg, WPARAM wp, LPARAM lp) override;

private:
    enum class HitZone { None, Start, App, Clock };

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
    void onMouseMove(int xPx, int yPx);
    void onLeftClick(int xPx, int yPx);
    void onMiddleClick(int xPx, int yPx);
    void paint();

    ShellTaskbar m_shellTaskbar;
    AppBar m_appBar;
    Renderer m_renderer;
    TaskTracker m_tasks;
    ClockWidget m_clock;

    Microsoft::WRL::ComPtr<IDWriteTextFormat> m_timeFormat;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> m_dateFormat;

    float m_barWidthDip = 0.0f;
    HitResult m_hover{};
    bool m_trackingMouse = false;
};