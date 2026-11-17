/**
- @file TaskbarUI.cpp
 *
- @version 1.1.0
- @author BleckWolf25
- @license MIT
 *
- @summary Implementation of the sleek glass taskbar window, hit-testing and painting
 *
- @description
- Replaces the Windows taskbar with a 42 DIP translucent glass surface over DWM Acrylic.
- Features a minimal Start mark on the left, interactive app icon pills with active indicator
- capsules in the center/left flow, and a clean right-aligned time and date stack.
 *
- @since 17/11/2026
- @updated 17/11/2026
 */
// ---------- IMPORTS
#include "Modules/TaskbarUI.h"

#include <windowsx.h>
#include <algorithm>

// ---------- CONSTANTS
namespace {
constexpr int kBarHeightDip = 42;
constexpr float kPadding = 10.0f;
constexpr float kSlotSize = 32.0f;
constexpr float kSlotGap = 6.0f;
constexpr float kSlotRadius = 8.0f;
constexpr float kIconSize = 18.0f;
constexpr float kClockWidth = 108.0f;

constexpr D2D1_COLOR_F kGlassTint{0.055f, 0.067f, 0.094f, 0.36f};
constexpr D2D1_COLOR_F kTopSpecular{1.0f, 1.0f, 1.0f, 0.14f};
constexpr D2D1_COLOR_F kHoverFill{1.0f, 1.0f, 1.0f, 0.09f};
constexpr D2D1_COLOR_F kActiveFill{1.0f, 1.0f, 1.0f, 0.13f};
constexpr D2D1_COLOR_F kAccent{0.529f, 0.659f, 1.0f, 1.0f};
constexpr D2D1_COLOR_F kInk{0.043f, 0.051f, 0.071f, 1.0f};
constexpr D2D1_COLOR_F kText{1.0f, 1.0f, 1.0f, 0.94f};
constexpr D2D1_COLOR_F kMuted{1.0f, 1.0f, 1.0f, 0.56f};
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

    m_glyphFormat =
        m_renderer.createFormat(14.0f, DWRITE_FONT_WEIGHT_BOLD, DWRITE_TEXT_ALIGNMENT_CENTER);
    m_timeFormat =
        m_renderer.createFormat(13.0f, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_TEXT_ALIGNMENT_TRAILING);
    m_dateFormat =
        m_renderer.createFormat(10.5f, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_TRAILING);
    if (!m_glyphFormat || !m_timeFormat || !m_dateFormat) {
        return false;
    }

    m_shellTaskbar.suppress();
    m_appBar.add(m_hwnd, WM_APP_APPBAR);
    dock();

    m_tasks.start(
        m_hwnd,
        [this](HICON icon) { return m_renderer.createIconBitmap(icon); },
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
        if (m_hoverSlot != -2) {
            m_hoverSlot = -2;
            InvalidateRect(m_hwnd, nullptr, FALSE);
        }
        return 0;
    case WM_LBUTTONUP:
        onClick(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
        return 0;
    case WM_APP_APPBAR:
        if (wp == ABN_POSCHANGED) {
            dock();
        }
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
    const int thickness = MulDiv(kBarHeightDip, dpi, USER_DEFAULT_SCREEN_DPI);
    const RECT rc = m_appBar.dock(m_hwnd, thickness);
    SetWindowPos(m_hwnd, HWND_TOPMOST, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top,
                 SWP_NOACTIVATE);
}

int TaskbarUI::hitTestSlot(float dipX, float dipY) const {
    const float slotTop = (static_cast<float>(kBarHeightDip) - kSlotSize) / 2.0f;
    if (dipY < slotTop || dipY > slotTop + kSlotSize) {
        return -2;
    }

    if (dipX >= kPadding && dipX <= kPadding + kSlotSize) {
        return -1;
    }

    float x = kPadding + kSlotSize + kSlotGap * 2.0f;
    const auto& items = m_tasks.items();
    for (int i = 0; i < static_cast<int>(items.size()); ++i) {
        if (dipX >= x && dipX <= x + kSlotSize) {
            return i;
        }
        x += kSlotSize + kSlotGap;
    }
    return -2;
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
    const int slot = hitTestSlot(static_cast<float>(xPx) / scale, static_cast<float>(yPx) / scale);
    if (slot != m_hoverSlot) {
        m_hoverSlot = slot;
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
}

void TaskbarUI::onClick(int xPx, int yPx) {
    const float scale = static_cast<float>(GetDpiForWindow(m_hwnd)) / 96.0f;
    const int slot = hitTestSlot(static_cast<float>(xPx) / scale, static_cast<float>(yPx) / scale);
    if (slot == -1) {
        ShellTaskbar::openStartMenu();
    } else if (slot >= 0 && slot < static_cast<int>(m_tasks.items().size())) {
        m_tasks.toggleWindow(m_tasks.items()[static_cast<size_t>(slot)].hwnd);
    }
}

void TaskbarUI::paint() {
    PAINTSTRUCT ps{};
    BeginPaint(m_hwnd, &ps);

    const HRESULT hr = m_renderer.draw([this](ID2D1RenderTarget& rt) {
        const D2D1_SIZE_F size = rt.GetSize();

        // Translucent glass base + crisp 1px top specular edge
        rt.FillRectangle(D2D1::RectF(0.0f, 0.0f, size.width, size.height),
                         m_renderer.brush(kGlassTint));
        rt.FillRectangle(D2D1::RectF(0.0f, 0.0f, size.width, 1.0f),
                         m_renderer.brush(kTopSpecular));

        // Minimal Start button
        const float slotTop = (size.height - kSlotSize) / 2.0f;
        const D2D1_RECT_F startRect =
            D2D1::RectF(kPadding, slotTop, kPadding + kSlotSize, slotTop + kSlotSize);
        if (m_hoverSlot == -1) {
            rt.FillRoundedRectangle(D2D1::RoundedRect(startRect, kSlotRadius, kSlotRadius),
                                    m_renderer.brush(kHoverFill));
        }
        const float badgeInset = 4.0f;
        const D2D1_RECT_F badgeRect =
            D2D1::RectF(startRect.left + badgeInset, startRect.top + badgeInset,
                        startRect.right - badgeInset, startRect.bottom - badgeInset);
        rt.FillRoundedRectangle(D2D1::RoundedRect(badgeRect, 7.0f, 7.0f),
                                m_renderer.brush(kAccent));
        rt.DrawText(L"N", 1, m_glyphFormat.Get(), badgeRect, m_renderer.brush(kInk));

        // Running application icons
        float x = kPadding + kSlotSize + kSlotGap * 2.0f;
        const float maxAppX = size.width - kClockWidth - kPadding * 2.0f - kSlotSize;
        const auto& items = m_tasks.items();

        for (int i = 0; i < static_cast<int>(items.size()); ++i) {
            if (x > maxAppX) {
                break;
            }
            const auto& item = items[static_cast<size_t>(i)];
            const D2D1_RECT_F slotRect = D2D1::RectF(x, slotTop, x + kSlotSize, slotTop + kSlotSize);

            if (item.active) {
                rt.FillRoundedRectangle(D2D1::RoundedRect(slotRect, kSlotRadius, kSlotRadius),
                                        m_renderer.brush(kActiveFill));
            } else if (m_hoverSlot == i) {
                rt.FillRoundedRectangle(D2D1::RoundedRect(slotRect, kSlotRadius, kSlotRadius),
                                        m_renderer.brush(kHoverFill));
            }

            if (item.icon) {
                const float iconOffset = (kSlotSize - kIconSize) / 2.0f;
                const D2D1_RECT_F iconRect =
                    D2D1::RectF(x + iconOffset, slotTop + iconOffset,
                                x + iconOffset + kIconSize, slotTop + iconOffset + kIconSize);
                rt.DrawBitmap(item.icon.Get(), iconRect);
            }

            // Sleek active/open indicator capsule
            const float pillWidth = item.active ? 12.0f : 4.0f;
            const float pillX = x + (kSlotSize - pillWidth) / 2.0f;
            const D2D1_RECT_F pillRect =
                D2D1::RectF(pillX, size.height - 3.5f, pillX + pillWidth, size.height - 1.5f);
            rt.FillRoundedRectangle(D2D1::RoundedRect(pillRect, 1.0f, 1.0f),
                                    m_renderer.brush(item.active ? kAccent : kMuted));

            x += kSlotSize + kSlotGap;
        }

        // Right-aligned clock stack
        const float right = size.width - kPadding - 2.0f;
        const D2D1_RECT_F timeRect = D2D1::RectF(right - kClockWidth, 5.0f, right, 23.0f);
        const D2D1_RECT_F dateRect = D2D1::RectF(right - kClockWidth, 22.0f, right, 38.0f);
        rt.DrawText(m_clock.time().c_str(), static_cast<UINT32>(m_clock.time().size()),
                    m_timeFormat.Get(), timeRect, m_renderer.brush(kText));
        rt.DrawText(m_clock.date().c_str(), static_cast<UINT32>(m_clock.date().size()),
                    m_dateFormat.Get(), dateRect, m_renderer.brush(kMuted));
    });

    EndPaint(m_hwnd, &ps);
    if (hr == D2DERR_RECREATE_TARGET) {
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
}