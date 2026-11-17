/**
 * @file TaskbarUI.cpp
 *
 * @version 1.0.0
 * @author BleckWolf25
 * @license MIT
 *
 * @summary Implementation of the taskbar window, layout and painting
 *
 * @description
 * Lays out in device-independent pixels: a rounded accent launcher mark on the left and a
 * right-aligned time over date stack. A translucent dark tint and a hairline top edge sit over
 * the Acrylic system backdrop. Docking converts the DIP height to pixels using the window DPI.
 *
 * @since 17/11/2026
 * @updated 17/11/2026
 */
// ---------- IMPORTS
#include "Modules/TaskbarUI.h"

// ---------- CONSTANTS
namespace {
constexpr int kBarHeightDip = 48;
constexpr float kMarkSize = 32.0f;
constexpr float kMarkRadius = 10.0f;
constexpr float kPadding = 12.0f;
constexpr float kClockWidth = 120.0f;

constexpr D2D1_COLOR_F kTint{0.043f, 0.051f, 0.071f, 0.55f};
constexpr D2D1_COLOR_F kHairline{1.0f, 1.0f, 1.0f, 0.08f};
constexpr D2D1_COLOR_F kAccent{0.486f, 0.612f, 1.0f, 1.0f};
constexpr D2D1_COLOR_F kInk{0.043f, 0.051f, 0.071f, 1.0f};
constexpr D2D1_COLOR_F kText{1.0f, 1.0f, 1.0f, 0.95f};
constexpr D2D1_COLOR_F kMuted{1.0f, 1.0f, 1.0f, 0.60f};
}

// ---------- IMPLEMENTATION
TaskbarUI::~TaskbarUI() {
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
    }
}

bool TaskbarUI::start() {
    if (!create(L"NovaUI.Taskbar", WS_POPUP, WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE)) {
        return false;
    }
    applyBackdrop();

    if (FAILED(m_renderer.init(m_hwnd))) {
        return false;
    }
    m_renderer.setDpi(static_cast<float>(GetDpiForWindow(m_hwnd)));

    m_glyphFormat = m_renderer.createFormat(15.0f, DWRITE_FONT_WEIGHT_BOLD, DWRITE_TEXT_ALIGNMENT_CENTER);
    m_timeFormat = m_renderer.createFormat(14.0f, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_TEXT_ALIGNMENT_TRAILING);
    m_dateFormat = m_renderer.createFormat(11.0f, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_TRAILING);
    if (!m_glyphFormat || !m_timeFormat || !m_dateFormat) {
        return false;
    }

    m_appBar.add(m_hwnd, WM_APP_APPBAR);
    dock();
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
    case WM_APP_APPBAR:
        if (wp == ABN_POSCHANGED) {
            dock();
        }
        return 0;
    case WM_DESTROY:
        m_clock.stop();
        m_appBar.remove(m_hwnd);
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

void TaskbarUI::paint() {
    PAINTSTRUCT ps{};
    BeginPaint(m_hwnd, &ps);

    const HRESULT hr = m_renderer.draw([this](ID2D1RenderTarget& rt) {
        const D2D1_SIZE_F size = rt.GetSize();

        rt.FillRectangle(D2D1::RectF(0.0f, 0.0f, size.width, size.height), m_renderer.brush(kTint));
        rt.FillRectangle(D2D1::RectF(0.0f, 0.0f, size.width, 1.0f), m_renderer.brush(kHairline));

        const float markTop = (size.height - kMarkSize) / 2.0f;
        const D2D1_RECT_F mark = D2D1::RectF(kPadding, markTop, kPadding + kMarkSize, markTop + kMarkSize);
        rt.FillRoundedRectangle(D2D1::RoundedRect(mark, kMarkRadius, kMarkRadius), m_renderer.brush(kAccent));
        rt.DrawText(L"N", 1, m_glyphFormat.Get(), mark, m_renderer.brush(kInk));

        const float right = size.width - kPadding - 4.0f;
        const D2D1_RECT_F timeRect = D2D1::RectF(right - kClockWidth, 6.0f, right, 26.0f);
        const D2D1_RECT_F dateRect = D2D1::RectF(right - kClockWidth, 25.0f, right, 42.0f);
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
