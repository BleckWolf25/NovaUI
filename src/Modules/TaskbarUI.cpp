/**
 * @file TaskbarUI.cpp
 *
 * @version 1.3.0
 * @author BleckWolf25
 * @license MIT
 *
 * @summary Implementation of the floating Liquid Glass dock, hit-testing and painting
 *
 * @description
 * Positions a dynamically sized floating stadium capsule centered above the bottom screen edge.
 * Clips DWM Acrylic to the exact pill silhouette via SetWindowRgn and composites translucent
 * glass refraction gradients, Fresnel rim highlights, vector Start prism geometry, high-DPI
 * app icons inside liquid lenses, and an interactive clock stack.
 *
 * @since 17/11/2026
 * @updated 17/11/2026
 */
// ---------- IMPORTS
#include "Modules/TaskbarUI.h"

#include <windowsx.h>
#include <cmath>

// ---------- CONSTANTS
namespace {
constexpr int kBarHeightDip = 44;

constexpr float kOuterPadX = 12.0f;
constexpr float kSlotSize = 34.0f;
constexpr float kSlotGap = 6.0f;
constexpr float kSlotRadius = 10.0f;
constexpr float kIconSize = 20.0f;
constexpr float kSectionGap = 10.0f;
constexpr float kDividerWidth = 1.0f;
constexpr float kClockWidth = 92.0f;

// Ultra-sheer blur-free glass palette
constexpr D2D1_COLOR_F kGlassBaseTint{0.04f, 0.055f, 0.08f, 0.02f};
constexpr D2D1_COLOR_F kSheenTop{1.0f, 1.0f, 1.0f, 0.045f};
constexpr D2D1_COLOR_F kSheenMid{1.0f, 1.0f, 1.0f, 0.005f};
constexpr D2D1_COLOR_F kSheenBottom{0.78f, 0.86f, 1.0f, 0.015f};

constexpr D2D1_COLOR_F kTopSpecular{1.0f, 1.0f, 1.0f, 0.18f};
constexpr D2D1_COLOR_F kInnerBevel{1.0f, 1.0f, 1.0f, 0.03f};

constexpr D2D1_COLOR_F kLensHoverFill{1.0f, 1.0f, 1.0f, 0.07f};
constexpr D2D1_COLOR_F kLensActiveFill{1.0f, 1.0f, 1.0f, 0.11f};
constexpr D2D1_COLOR_F kLensRimTop{1.0f, 1.0f, 1.0f, 0.26f};
constexpr D2D1_COLOR_F kLensRimMid{1.0f, 1.0f, 1.0f, 0.09f};
constexpr D2D1_COLOR_F kLensRimBottom{1.0f, 1.0f, 1.0f, 0.06f};

// Windows 11 Start emblem crystalline palette
constexpr D2D1_COLOR_F kWinPaneTop{0.50f, 0.89f, 1.0f, 0.98f};
constexpr D2D1_COLOR_F kWinPaneMid{0.31f, 0.75f, 0.98f, 0.96f};
constexpr D2D1_COLOR_F kWinPaneBottom{0.17f, 0.60f, 0.94f, 0.96f};
constexpr D2D1_COLOR_F kWinPaneHoverTop{0.64f, 0.94f, 1.0f, 1.0f};
constexpr D2D1_COLOR_F kWinPaneHoverMid{0.42f, 0.82f, 1.0f, 0.98f};
constexpr D2D1_COLOR_F kWinPaneHoverBottom{0.25f, 0.68f, 0.98f, 0.98f};

constexpr D2D1_COLOR_F kAccent{0.46f, 0.78f, 1.0f, 1.0f};
constexpr D2D1_COLOR_F kAccentGlow{0.46f, 0.78f, 1.0f, 0.30f};
constexpr D2D1_COLOR_F kText{1.0f, 1.0f, 1.0f, 0.97f};
constexpr D2D1_COLOR_F kTextShadow{0.0f, 0.0f, 0.0f, 0.45f};
constexpr D2D1_COLOR_F kMuted{1.0f, 1.0f, 1.0f, 0.68f};

// Builds a rectangle path where each corner can have an independent radius
Microsoft::WRL::ComPtr<ID2D1PathGeometry> createAsymmetricRoundRect(
    ID2D1Factory1* factory, const D2D1_RECT_F& r, float rTL, float rTR, float rBR, float rBL) {
    if (!factory) {
        return nullptr;
    }

    Microsoft::WRL::ComPtr<ID2D1PathGeometry> geometry;
    if (FAILED(factory->CreatePathGeometry(geometry.GetAddressOf()))) {
        return nullptr;
    }

    Microsoft::WRL::ComPtr<ID2D1GeometrySink> sink;
    if (FAILED(geometry->Open(sink.GetAddressOf()))) {
        return nullptr;
    }

    sink->BeginFigure(D2D1::Point2F(r.left + rTL, r.top), D2D1_FIGURE_BEGIN_FILLED);

    // Top edge -> Top-Right arc
    sink->AddLine(D2D1::Point2F(r.right - rTR, r.top));
    if (rTR > 0.0f) {
        sink->AddArc(D2D1::ArcSegment(D2D1::Point2F(r.right, r.top + rTR), D2D1::SizeF(rTR, rTR),
                                      0.0f, D2D1_SWEEP_DIRECTION_CLOCKWISE,
                                      D2D1_ARC_SIZE_SMALL));
    }

    // Right edge -> Bottom-Right arc
    sink->AddLine(D2D1::Point2F(r.right, r.bottom - rBR));
    if (rBR > 0.0f) {
        sink->AddArc(D2D1::ArcSegment(D2D1::Point2F(r.right - rBR, r.bottom),
                                      D2D1::SizeF(rBR, rBR), 0.0f, D2D1_SWEEP_DIRECTION_CLOCKWISE,
                                      D2D1_ARC_SIZE_SMALL));
    }

    // Bottom edge -> Bottom-Left arc
    sink->AddLine(D2D1::Point2F(r.left + rBL, r.bottom));
    if (rBL > 0.0f) {
        sink->AddArc(D2D1::ArcSegment(D2D1::Point2F(r.left, r.bottom - rBL), D2D1::SizeF(rBL, rBL),
                                      0.0f, D2D1_SWEEP_DIRECTION_CLOCKWISE,
                                      D2D1_ARC_SIZE_SMALL));
    }

    // Left edge -> Top-Left arc
    sink->AddLine(D2D1::Point2F(r.left, r.top + rTL));
    if (rTL > 0.0f) {
        sink->AddArc(D2D1::ArcSegment(D2D1::Point2F(r.left + rTL, r.top), D2D1::SizeF(rTL, rTL),
                                      0.0f, D2D1_SWEEP_DIRECTION_CLOCKWISE,
                                      D2D1_ARC_SIZE_SMALL));
    }

    sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    if (FAILED(sink->Close())) {
        return nullptr;
    }
    return geometry;
}
}

// ---------- IMPLEMENTATION
TaskbarUI::~TaskbarUI() {
    m_tasks.stop();
    m_shellTaskbar.restore();
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
    }
}

bool TaskbarUI::start() {
    constexpr DWORD exStyle =
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_NOREDIRECTIONBITMAP;
    if (!create(L"NovaUI.Taskbar", WS_POPUP, exStyle)) {
        return false;
    }
    applyBackdrop();

    if (FAILED(m_renderer.init(m_hwnd))) {
        return false;
    }
    m_renderer.setDpi(static_cast<float>(GetDpiForWindow(m_hwnd)));

    m_timeFormat =
        m_renderer.createFormat(13.0f, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_TEXT_ALIGNMENT_TRAILING);
    m_dateFormat =
        m_renderer.createFormat(10.0f, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_TRAILING);
    if (!m_timeFormat || !m_dateFormat) {
        return false;
    }

    m_shellTaskbar.suppress();
    m_appBar.add(m_hwnd, WM_APP_APPBAR);
    dock();

    m_tasks.start(
        m_hwnd,
        [this](HICON icon) -> Microsoft::WRL::ComPtr<ID2D1Bitmap> {
            return m_renderer.createIconBitmap(icon);
        },
        [this](HBITMAP bitmap) -> Microsoft::WRL::ComPtr<ID2D1Bitmap> {
            return m_renderer.createBitmapFromHBitmap(bitmap);
        },
        [this]() {
            if (m_hwnd) {
                InvalidateRect(m_hwnd, nullptr, FALSE);
            }
        });

    m_clock.start(m_hwnd);
    ShowWindow(m_hwnd, SW_SHOWNOACTIVATE);
    return true;
}

LRESULT TaskbarUI::onMessage(UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_PAINT:
        paint();
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_SIZE:
        m_renderer.resize(LOWORD(lp), HIWORD(lp));
        return 0;
    case WM_DPICHANGED:
        m_renderer.setDpi(static_cast<float>(HIWORD(wp)));
        dock();
        return 0;
    case WM_DISPLAYCHANGE:
        dock();
        return 0;
    case WM_TIMECHANGE:
        m_clock.refresh();
        return 0;
    case WM_TIMER:
        if (m_clock.onTimer(wp)) {
            return 0;
        }
        break;
    case WM_MOUSEMOVE:
        onMouseMove(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
        return 0;
    case WM_MOUSELEAVE:
        m_trackingMouse = false;
        if (m_hover.zone != HitZone::None) {
            m_hover = {};
            InvalidateRect(m_hwnd, nullptr, FALSE);
        }
        return 0;
    case WM_LBUTTONUP:
        onLeftClick(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
        return 0;
    case WM_MBUTTONUP:
        onMiddleClick(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
        return 0;
    case WM_APP_APPBAR:
        if (wp == ABN_POSCHANGED) {
            dock();
        }
        return 0;
    case TaskTracker::kMsgPinsChanged:
        m_tasks.reloadPinnedApps();
        return 0;
    case WM_DESTROY:
        m_tasks.stop();
        m_clock.stop();
        m_appBar.remove(m_hwnd);
        m_shellTaskbar.restore();
        PostQuitMessage(0);
        return 0;
    default:
        break;
    }
    return Window::onMessage(msg, wp, lp);
}

void TaskbarUI::dock() {
    const int dpi = static_cast<int>(GetDpiForWindow(m_hwnd));
    const float scale = static_cast<float>(dpi) / static_cast<float>(USER_DEFAULT_SCREEN_DPI);
    const int thicknessPx = static_cast<int>(std::lround(static_cast<float>(kBarHeightDip) * scale));

    const RECT rc = m_appBar.dock(m_hwnd, thicknessPx);
    const int widthPx = rc.right - rc.left;
    const int heightPx = rc.bottom - rc.top;
    m_barWidthDip = static_cast<float>(widthPx) / scale;

    SetWindowPos(m_hwnd, HWND_TOPMOST, rc.left, rc.top, widthPx, heightPx, SWP_NOACTIVATE);
}

TaskbarUI::HitResult TaskbarUI::hitTest(float dipX, float dipY) const {
    const float barHeightDip = static_cast<float>(kBarHeightDip);
    const float slotTop = (barHeightDip - kSlotSize) / 2.0f;
    if (dipY < slotTop || dipY > slotTop + kSlotSize) {
        return {};
    }

    float x = kOuterPadX;
    if (dipX >= x && dipX <= x + kSlotSize) {
        return {HitZone::Start, -1};
    }

    x += kSlotSize + kSectionGap + kDividerWidth + kSectionGap;

    const auto& items = m_tasks.items();
    const float maxAppX = m_barWidthDip - kOuterPadX - kClockWidth - kSectionGap -
                          kDividerWidth - kSectionGap - kSlotSize;
    for (int i = 0; i < static_cast<int>(items.size()); ++i) {
        if (x > maxAppX) {
            break;
        }
        if (dipX >= x && dipX <= x + kSlotSize) {
            return {HitZone::App, i};
        }
        x += kSlotSize + kSlotGap;
    }

    const float clockRight = m_barWidthDip - kOuterPadX;
    const float clockLeft = clockRight - kClockWidth;
    if (dipX >= clockLeft - 6.0f && dipX <= clockRight + 4.0f) {
        return {HitZone::Clock, -1};
    }

    return {};
}

void TaskbarUI::onMouseMove(int xPx, int yPx) {
    if (!m_trackingMouse) {
        TRACKMOUSEEVENT tme{};
        tme.cbSize = sizeof(tme);
        tme.dwFlags = TME_LEAVE;
        tme.hwndTrack = m_hwnd;
        TrackMouseEvent(&tme);
        m_trackingMouse = true;
    }

    const float scale = static_cast<float>(GetDpiForWindow(m_hwnd)) / 96.0f;
    const HitResult hit = hitTest(static_cast<float>(xPx) / scale, static_cast<float>(yPx) / scale);
    if (hit != m_hover) {
        m_hover = hit;
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
}

void TaskbarUI::onLeftClick(int xPx, int yPx) {
    const float scale = static_cast<float>(GetDpiForWindow(m_hwnd)) / 96.0f;
    const HitResult hit = hitTest(static_cast<float>(xPx) / scale, static_cast<float>(yPx) / scale);
    switch (hit.zone) {
    case HitZone::Start:
        ShellTaskbar::openStartMenu();
        break;
    case HitZone::App:
        if (hit.index >= 0 && hit.index < static_cast<int>(m_tasks.items().size())) {
            m_tasks.activateOrLaunch(m_tasks.items()[static_cast<size_t>(hit.index)]);
        }
        break;
    case HitZone::Clock:
        ShellTaskbar::openNotificationCenter();
        break;
    case HitZone::None:
        break;
    }
}

void TaskbarUI::onMiddleClick(int xPx, int yPx) {
    const float scale = static_cast<float>(GetDpiForWindow(m_hwnd)) / 96.0f;
    const HitResult hit = hitTest(static_cast<float>(xPx) / scale, static_cast<float>(yPx) / scale);
    if (hit.zone == HitZone::App && hit.index >= 0 &&
        hit.index < static_cast<int>(m_tasks.items().size())) {
        m_tasks.middleClickItem(m_tasks.items()[static_cast<size_t>(hit.index)]);
    }
}

void TaskbarUI::paint() {
    PAINTSTRUCT ps{};
    BeginPaint(m_hwnd, &ps);

    const HRESULT hr = m_renderer.draw([this](ID2D1DeviceContext& dc) {
        const D2D1_SIZE_F size = dc.GetSize();
        const D2D1_RECT_F fullRect = D2D1::RectF(0.0f, 0.0f, size.width, size.height);

        // 1. Ultra-sheer blur-free glass tint + faint vertical specular sheen
        dc.FillRectangle(fullRect, m_renderer.brush(kGlassBaseTint));
        if (auto sheenBrush = m_renderer.verticalGradient(0.0f, size.height, kSheenTop, kSheenMid,
                                                          0.42f, kSheenBottom)) {
            dc.FillRectangle(fullRect, sheenBrush.Get());
        }

        // 2. Delicate top specular glass rim + inner thickness bevel
        dc.FillRectangle(D2D1::RectF(0.0f, 0.0f, size.width, 1.0f),
                         m_renderer.brush(kTopSpecular));
        dc.FillRectangle(D2D1::RectF(0.0f, 1.0f, size.width, 2.0f),
                         m_renderer.brush(kInnerBevel));

        const float slotTop = (size.height - kSlotSize) / 2.0f;
        const auto drawGlassLens = [&](const D2D1_RECT_F& rect, bool active, bool hovered) {
            if (!active && !hovered) {
                return;
            }
            const D2D1_ROUNDED_RECT lens = D2D1::RoundedRect(rect, kSlotRadius, kSlotRadius);
            dc.FillRoundedRectangle(
                lens, m_renderer.brush(active ? kLensActiveFill : kLensHoverFill));
            if (auto lensRim = m_renderer.verticalGradient(
                    rect.top, rect.bottom, active ? kLensRimTop : kLensRimMid, kInnerBevel, 0.5f,
                    kLensRimBottom)) {
                dc.DrawRoundedRectangle(lens, lensRim.Get(), 1.0f);
            }
        };

        const auto drawGlassDivider = [&](float divX) {
            if (auto divBrush = m_renderer.verticalGradient(
                    10.0f, size.height - 10.0f, D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.01f),
                    D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.14f), 0.5f,
                    D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.01f))) {
                dc.FillRectangle(D2D1::RectF(divX, 10.0f, divX + kDividerWidth, size.height - 10.0f),
                                 divBrush.Get());
            }
        };

        // 3. Sculpted Windows 11 Start Emblem
        float x = kOuterPadX;
        const D2D1_RECT_F startRect = D2D1::RectF(x, slotTop, x + kSlotSize, slotTop + kSlotSize);
        const bool startHovered = (m_hover.zone == HitZone::Start);
        drawGlassLens(startRect, false, startHovered);

        const float cx = (startRect.left + startRect.right) / 2.0f;
        const float cy = (startRect.top + startRect.bottom) / 2.0f;
        const float pane = 8.0f;
        const float gap = 0.75f; // 1.5 DIP total crosshair seam
        const float outerR = 2.2f;
        const float innerR = 0.4f;

        const D2D1_RECT_F panes[4]{
            D2D1::RectF(cx - gap - pane, cy - gap - pane, cx - gap, cy - gap), // Top-Left
            D2D1::RectF(cx + gap, cy - gap - pane, cx + gap + pane, cy - gap), // Top-Right
            D2D1::RectF(cx - gap - pane, cy + gap, cx - gap, cy + gap + pane), // Bottom-Left
            D2D1::RectF(cx + gap, cy + gap, cx + gap + pane, cy + gap + pane), // Bottom-Right
        };
        const float corners[4][4]{
            {outerR, innerR, innerR, innerR},
            {innerR, outerR, innerR, innerR},
            {innerR, innerR, innerR, outerR},
            {innerR, innerR, outerR, innerR},
        };

        auto winGradient = m_renderer.verticalGradient(
            cy - gap - pane, cy + gap + pane, startHovered ? kWinPaneHoverTop : kWinPaneTop,
            startHovered ? kWinPaneHoverMid : kWinPaneMid, 0.48f,
            startHovered ? kWinPaneHoverBottom : kWinPaneBottom);

        for (int p = 0; p < 4; ++p) {
            auto geo = createAsymmetricRoundRect(m_renderer.factory(), panes[p], corners[p][0],
                                                 corners[p][1], corners[p][2], corners[p][3]);
            if (geo) {
                ID2D1Brush* fillBrush =
                    winGradient ? static_cast<ID2D1Brush*>(winGradient.Get())
                                : static_cast<ID2D1Brush*>(m_renderer.brush(kWinPaneMid));
                dc.FillGeometry(geo.Get(), fillBrush);
                dc.DrawGeometry(geo.Get(),
                                m_renderer.brush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.24f)), 0.5f);
            }
        }

        // Left glass divider
        x += kSlotSize + kSectionGap;
        drawGlassDivider(x);
        x += kDividerWidth + kSectionGap;

        // 4. Pinned & Running application icons inside Liquid Glass lenses
        const auto& items = m_tasks.items();
        const float maxAppX = size.width - kOuterPadX - kClockWidth - kSectionGap -
                              kDividerWidth - kSectionGap - kSlotSize;
        for (int i = 0; i < static_cast<int>(items.size()); ++i) {
            if (x > maxAppX) {
                break;
            }
            const auto& item = items[static_cast<size_t>(i)];
            const D2D1_RECT_F slotRect =
                D2D1::RectF(x, slotTop, x + kSlotSize, slotTop + kSlotSize);
            const bool hovered = (m_hover.zone == HitZone::App && m_hover.index == i);

            drawGlassLens(slotRect, item.active, hovered);

            if (item.icon) {
                const float iconOffset = (kSlotSize - kIconSize) / 2.0f;
                const D2D1_RECT_F iconRect =
                    D2D1::RectF(x + iconOffset, slotTop + iconOffset - 0.5f,
                                x + iconOffset + kIconSize,
                                slotTop + iconOffset + kIconSize - 0.5f);
                const float iconOpacity = (item.running || hovered) ? 1.0f : 0.92f;
                dc.DrawBitmap(item.icon.Get(), iconRect, iconOpacity,
                              D2D1_INTERPOLATION_MODE_HIGH_QUALITY_CUBIC);
            }

            // Luminous active/open indicator capsule (hidden for unlaunched pinned apps)
            if (item.running) {
                const float pillWidth = item.active ? 12.0f : 4.5f;
                const float pillX = x + (kSlotSize - pillWidth) / 2.0f;
                if (item.active) {
                    const D2D1_RECT_F glowRect =
                        D2D1::RectF(pillX - 2.0f, size.height - 5.5f, pillX + pillWidth + 2.0f,
                                    size.height - 1.5f);
                    dc.FillRoundedRectangle(D2D1::RoundedRect(glowRect, 2.0f, 2.0f),
                                            m_renderer.brush(kAccentGlow));
                }
                const D2D1_RECT_F pillRect =
                    D2D1::RectF(pillX, size.height - 4.5f, pillX + pillWidth, size.height - 2.5f);
                dc.FillRoundedRectangle(D2D1::RoundedRect(pillRect, 1.0f, 1.0f),
                                        m_renderer.brush(item.active ? kAccent : kMuted));
            }

            x += kSlotSize + kSlotGap;
        }

        // Right glass divider & Clock cluster
        const float clockRight = size.width - kOuterPadX;
        const float clockLeft = clockRight - kClockWidth;
        drawGlassDivider(clockLeft - kSectionGap - kDividerWidth);

        if (m_hover.zone == HitZone::Clock) {
            const D2D1_RECT_F clockHoverRect =
                D2D1::RectF(clockLeft - 6.0f, slotTop, clockRight + 4.0f, slotTop + kSlotSize);
            drawGlassLens(clockHoverRect, false, true);
        }

        const D2D1_RECT_F timeRect = D2D1::RectF(clockLeft, 6.0f, clockRight, 24.0f);
        const D2D1_RECT_F dateRect = D2D1::RectF(clockLeft, 23.0f, clockRight, 39.0f);
        const D2D1_RECT_F timeShadowRect = D2D1::RectF(clockLeft, 7.0f, clockRight, 25.0f);
        const D2D1_RECT_F dateShadowRect = D2D1::RectF(clockLeft, 24.0f, clockRight, 40.0f);

        dc.DrawText(m_clock.time().c_str(), static_cast<UINT32>(m_clock.time().size()),
                    m_timeFormat.Get(), timeShadowRect, m_renderer.brush(kTextShadow));
        dc.DrawText(m_clock.date().c_str(), static_cast<UINT32>(m_clock.date().size()),
                    m_dateFormat.Get(), dateShadowRect, m_renderer.brush(kTextShadow));

        dc.DrawText(m_clock.time().c_str(), static_cast<UINT32>(m_clock.time().size()),
                    m_timeFormat.Get(), timeRect, m_renderer.brush(kText));
        dc.DrawText(m_clock.date().c_str(), static_cast<UINT32>(m_clock.date().size()),
                    m_dateFormat.Get(), dateRect, m_renderer.brush(kMuted));
    });

    EndPaint(m_hwnd, &ps);
    if (hr == D2DERR_RECREATE_TARGET) {
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
}