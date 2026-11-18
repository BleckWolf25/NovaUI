/**
 * @file TaskbarUI.cpp
 *
 * @version 1.5.0
 * @author BleckWolf25
 * @license MIT
 *
 * @summary Implementation of the floating Liquid Glass dock, hit-testing, painting and shell guard
 *
 * @description
 * Positions a dynamically sized floating dock above the bottom screen edge and clips DWM Acrylic
 * to its silhouette. A single Layout struct feeds hit-testing, slot anchoring, drag indexing and
 * painting so drawn slots and clickable slots can never diverge. Composites glass refraction
 * gradients, Fresnel rims, the Start prism, an optional Task View glyph, high-DPI app icons and a
 * clock stack. An event-driven WinEvent guard re-shows and re-tops the bar after Show Desktop and
 * shell overlays; empty-area right-click opens a native Taskbar settings / Task Manager menu.
 *
 * @since 17/11/2026
 * @updated 18/11/2026
 */
// ---------- IMPORTS
#include "Modules/TaskbarUI.h"
#include <shellapi.h>
#include <windowsx.h>
#include <algorithm>
#include <cmath>
#include <cwchar>
#include <memory>
// ---------- CONSTANTS
namespace {
constexpr UINT kMsgReassert = WM_APP + 4;
constexpr int kBarHeightDip = 44;
constexpr float kOuterPadX = 12.0f;
constexpr float kSlotSize = 34.0f;
constexpr float kSlotGap = 6.0f;
constexpr float kSlotRadius = 10.0f;
constexpr float kIconSize = 20.0f;
constexpr float kTraySlotSize = 26.0f;
constexpr float kTraySlotGap = 4.0f;
constexpr float kTrayIconSize = 16.0f;
constexpr float kChevronWidth = 22.0f;
constexpr float kSectionGap = 10.0f;
constexpr float kDividerWidth = 1.0f;
constexpr float kClockWidth = 92.0f;
constexpr D2D1_COLOR_F kGlassBaseTint{0.04f, 0.055f, 0.08f, 0.02f};
constexpr D2D1_COLOR_F kSheenTop{1.0f, 1.0f, 1.0f, 0.045f};
constexpr D2D1_COLOR_F kSheenMid{1.0f, 1.0f, 1.0f, 0.005f};
constexpr D2D1_COLOR_F kSheenBottom{0.78f, 0.86f, 1.0f, 0.015f};
constexpr D2D1_COLOR_F kTopSpecular{1.0f, 1.0f, 1.0f, 0.18f};
constexpr D2D1_COLOR_F kInnerBevel{1.0f, 1.0f, 1.0f, 0.03f};
constexpr D2D1_COLOR_F kLensHoverFill{1.0f, 1.0f, 1.0f, 0.08f};
constexpr D2D1_COLOR_F kLensActiveFill{1.0f, 1.0f, 1.0f, 0.12f};
constexpr D2D1_COLOR_F kLensDragFill{0.46f, 0.78f, 1.0f, 0.18f};
constexpr D2D1_COLOR_F kLensRimTop{1.0f, 1.0f, 1.0f, 0.28f};
constexpr D2D1_COLOR_F kLensRimMid{1.0f, 1.0f, 1.0f, 0.10f};
constexpr D2D1_COLOR_F kLensRimBottom{1.0f, 1.0f, 1.0f, 0.06f};
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
constexpr D2D1_COLOR_F kMuted{1.0f, 1.0f, 1.0f, 0.70f};

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
    sink->AddLine(D2D1::Point2F(r.right - rTR, r.top));
    if (rTR > 0.0f) {
        sink->AddArc(D2D1::ArcSegment(D2D1::Point2F(r.right, r.top + rTR), D2D1::SizeF(rTR, rTR),
                                      0.0f, D2D1_SWEEP_DIRECTION_CLOCKWISE, D2D1_ARC_SIZE_SMALL));
    }
    sink->AddLine(D2D1::Point2F(r.right, r.bottom - rBR));
    if (rBR > 0.0f) {
        sink->AddArc(D2D1::ArcSegment(D2D1::Point2F(r.right - rBR, r.bottom),
                                      D2D1::SizeF(rBR, rBR), 0.0f, D2D1_SWEEP_DIRECTION_CLOCKWISE,
                                      D2D1_ARC_SIZE_SMALL));
    }
    sink->AddLine(D2D1::Point2F(r.left + rBL, r.bottom));
    if (rBL > 0.0f) {
        sink->AddArc(D2D1::ArcSegment(D2D1::Point2F(r.left, r.bottom - rBL), D2D1::SizeF(rBL, rBL),
                                      0.0f, D2D1_SWEEP_DIRECTION_CLOCKWISE, D2D1_ARC_SIZE_SMALL));
    }
    sink->AddLine(D2D1::Point2F(r.left, r.top + rTL));
    if (rTL > 0.0f) {
        sink->AddArc(D2D1::ArcSegment(D2D1::Point2F(r.left + rTL, r.top), D2D1::SizeF(rTL, rTL),
                                      0.0f, D2D1_SWEEP_DIRECTION_CLOCKWISE, D2D1_ARC_SIZE_SMALL));
    }
    sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    if (FAILED(sink->Close())) {
        return nullptr;
    }
    return geometry;
}
}
// ---------- IMPLEMENTATION
TaskbarUI* TaskbarUI::s_instance = nullptr;

TaskbarUI::~TaskbarUI() {
    removeGuard();
    m_tray.stop();
    m_tasks.stop();
    m_shellTaskbar.restore();
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
    }
    s_instance = nullptr;
}

bool TaskbarUI::start() {
    constexpr DWORD exStyle = WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE |
                              WS_EX_NOREDIRECTIONBITMAP | WS_EX_ACCEPTFILES;
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
    m_showTaskView = ShellTaskbar::isTaskViewEnabled();
    TaskbarFlyout::Callbacks flyoutCallbacks{};
    flyoutCallbacks.onFocusWindow = [this](HWND target) { m_tasks.focusWindow(target); };
    flyoutCallbacks.onCloseWindow = [this](HWND target) { m_tasks.closeWindow(target); };
    flyoutCallbacks.onLaunchNew = [this](size_t idx) {
        if (idx < m_tasks.items().size()) {
            m_tasks.launchNewInstance(m_tasks.items()[idx]);
        }
    };
    flyoutCallbacks.onTogglePinApp = [this](size_t idx) {
        if (idx < m_tasks.items().size()) {
            const auto& item = m_tasks.items()[idx];
            if (item.pinned) {
                m_tasks.unpinItem(item);
            } else {
                m_tasks.pinItem(item);
            }
        }
    };
    flyoutCallbacks.onCloseAllWindows = [this](size_t idx) {
        if (idx < m_tasks.items().size()) {
            m_tasks.closeAllWindows(m_tasks.items()[idx]);
        }
    };
    flyoutCallbacks.onTrayLeftClick = [this](const TrayItem& item) { m_tray.leftClick(item); };
    flyoutCallbacks.onTrayRightClick = [this](const TrayItem& item) { m_tray.rightClick(item); };
    flyoutCallbacks.onTrayTogglePinToBar = [this](const std::wstring& key) {
        m_tray.togglePinnedToBar(key);
    };
    m_flyout.init(m_hwnd, std::move(flyoutCallbacks));
    auto iconLoader = [this](HICON icon) -> Microsoft::WRL::ComPtr<ID2D1Bitmap> {
        return m_renderer.createIconBitmap(icon);
    };
    auto hbitmapLoader = [this](HBITMAP bitmap) -> Microsoft::WRL::ComPtr<ID2D1Bitmap> {
        return m_renderer.createBitmapFromHBitmap(bitmap);
    };
    auto invalidate = [this]() {
        if (m_hwnd) {
            InvalidateRect(m_hwnd, nullptr, FALSE);
        }
    };
    // Trackers read the live shell before it is suppressed so the first tray enumeration succeeds.
    m_tasks.start(m_hwnd, iconLoader, hbitmapLoader, invalidate);
    m_tray.start(m_hwnd, iconLoader, invalidate);
    m_clock.start(m_hwnd);
    m_shellTaskbar.suppress();
    m_appBar.add(m_hwnd, WM_APP_APPBAR);
    dock();
    installGuard();
    s_instance = this;
    ShowWindow(m_hwnd, SW_SHOWNOACTIVATE);
    return true;
}

void TaskbarUI::installGuard() {
    const DWORD ownPid = GetCurrentProcessId();
    if (HWINEVENTHOOK h = SetWinEventHook(EVENT_SYSTEM_MINIMIZESTART, EVENT_SYSTEM_MINIMIZEEND,
                                          nullptr, &TaskbarUI::onGuardEvent, ownPid, 0,
                                          WINEVENT_OUTOFCONTEXT)) {
        m_guardHooks.push_back(h);
    }
    if (HWINEVENTHOOK h = SetWinEventHook(EVENT_OBJECT_HIDE, EVENT_OBJECT_HIDE, nullptr,
                                          &TaskbarUI::onGuardEvent, ownPid, 0,
                                          WINEVENT_OUTOFCONTEXT)) {
        m_guardHooks.push_back(h);
    }
    if (HWINEVENTHOOK h = SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, nullptr,
                                          &TaskbarUI::onGuardEvent, 0, 0,
                                          WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS)) {
        m_guardHooks.push_back(h);
    }
    if (HWINEVENTHOOK h = SetWinEventHook(EVENT_SYSTEM_DESKTOPSWITCH, EVENT_SYSTEM_DESKTOPSWITCH,
                                          nullptr, &TaskbarUI::onGuardEvent, 0, 0,
                                          WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS)) {
        m_guardHooks.push_back(h);
    }
}

void TaskbarUI::removeGuard() {
    for (HWINEVENTHOOK h : m_guardHooks) {
        UnhookWinEvent(h);
    }
    m_guardHooks.clear();
}

void TaskbarUI::reassert() {
    if (!m_hwnd) {
        return;
    }
    if (!IsWindowVisible(m_hwnd) || IsIconic(m_hwnd)) {
        ShowWindow(m_hwnd, SW_SHOWNOACTIVATE);
    }
    SetWindowPos(m_hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

void CALLBACK TaskbarUI::onGuardEvent(HWINEVENTHOOK, DWORD event, HWND hwnd, LONG idObject,
                                      LONG idChild, DWORD, DWORD) {
    if (!s_instance || !s_instance->m_hwnd) {
        return;
    }
    if (event == EVENT_SYSTEM_MINIMIZESTART || event == EVENT_OBJECT_HIDE) {
        if (hwnd == s_instance->m_hwnd && idObject == OBJID_WINDOW && idChild == CHILDID_SELF) {
            PostMessageW(s_instance->m_hwnd, kMsgReassert, 0, 0);
        }
        return;
    }
    if (event == EVENT_SYSTEM_DESKTOPSWITCH) {
        PostMessageW(s_instance->m_hwnd, kMsgReassert, 0, 0);
        return;
    }
    if (event == EVENT_SYSTEM_FOREGROUND && hwnd) {
        wchar_t className[64]{};
        if (GetClassNameW(hwnd, className, 64) != 0 &&
            (std::wcscmp(className, L"XamlExplorerHostIslandWindow") == 0 ||
             std::wcscmp(className, L"ForegroundStaging") == 0 ||
             std::wcscmp(className, L"Windows.UI.Core.CoreWindow") == 0)) {
            PostMessageW(s_instance->m_hwnd, kMsgReassert, 0, 0);
        }
    }
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
    case WM_SETTINGCHANGE:
        m_showTaskView = ShellTaskbar::isTaskViewEnabled();
        InvalidateRect(m_hwnd, nullptr, FALSE);
        return 0;
    case WM_TIMECHANGE:
        m_clock.refresh();
        return 0;
    case WM_SHOWWINDOW:
        if (wp == FALSE) {
            PostMessageW(m_hwnd, kMsgReassert, 0, 0);
        }
        return 0;
    case WM_TIMER:
        if (m_clock.onTimer(wp) || m_tray.onTimer(wp)) {
            return 0;
        }
        break;
    case WM_MOUSEMOVE:
        onMouseMove(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
        return 0;
    case WM_MOUSELEAVE:
        m_trackingMouse = false;
        if (!m_dragging && m_hover.zone != HitZone::None) {
            m_hover = {};
            if (!m_flyout.isInteractive()) {
                m_flyout.hide();
            }
            InvalidateRect(m_hwnd, nullptr, FALSE);
        }
        return 0;
    case WM_LBUTTONDOWN:
        onLeftDown(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
        return 0;
    case WM_LBUTTONUP:
        onLeftUp(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
        return 0;
    case WM_LBUTTONDBLCLK:
        onLeftDoubleClick(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
        return 0;
    case WM_CAPTURECHANGED:
        m_pointerDown = false;
        if (m_dragging) {
            m_dragging = false;
            m_dragZone = HitZone::None;
            m_dragFromIndex = -1;
            m_dragToIndex = -1;
            m_dragDeltaDip = 0.0f;
            InvalidateRect(m_hwnd, nullptr, FALSE);
        }
        return 0;
    case WM_RBUTTONUP:
        onRightUp(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
        return 0;
    case WM_MBUTTONUP:
        onMiddleUp(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
        return 0;
    case WM_DROPFILES: {
        HDROP hDrop = reinterpret_cast<HDROP>(wp);
        const UINT count = DragQueryFileW(hDrop, 0xFFFFFFFF, nullptr, 0);
        for (UINT i = 0; i < count; ++i) {
            wchar_t path[MAX_PATH]{};
            if (DragQueryFileW(hDrop, i, path, MAX_PATH) > 0) {
                m_tasks.pinExternalPath(path);
            }
        }
        DragFinish(hDrop);
        return 0;
    }
    case WM_APP_APPBAR:
        if (wp == ABN_POSCHANGED) {
            dock();
        } else if (wp == ABN_FULLSCREENAPP) {
            HWND zOrder = (lp != 0) ? HWND_BOTTOM : HWND_TOPMOST;
            SetWindowPos(m_hwnd, zOrder, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        }
        return 0;
    case TaskTracker::kMsgPinsChanged:
        m_tasks.reloadPinnedApps();
        return 0;
    case kMsgReassert:
        reassert();
        return 0;
    case WM_DESTROY:
        removeGuard();
        m_tray.stop();
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

TaskbarUI::Layout TaskbarUI::computeLayout(float widthDip, float heightDip) const {
    Layout l{};
    l.slotTop = (heightDip - kSlotSize) / 2.0f;
    l.traySlotTop = (heightDip - kTraySlotSize) / 2.0f;
    l.startLeft = kOuterPadX;
    float x = kOuterPadX + kSlotSize + kSectionGap + kDividerWidth + kSectionGap;
    l.taskViewLeft = x;
    if (m_showTaskView) {
        x += kSlotSize + kSlotGap;
    }
    l.appsStartX = x;
    l.clockRight = widthDip - kOuterPadX;
    l.clockLeft = l.clockRight - kClockWidth;
    l.traySectionRight = l.clockLeft - kSectionGap - kDividerWidth - kSectionGap;
    l.chevronLeft = l.traySectionRight - kChevronWidth;
    const auto& pinnedTray = m_tray.pinnedItems();
    const float trayTotalW =
        pinnedTray.empty()
            ? 0.0f
            : static_cast<float>(pinnedTray.size()) * kTraySlotSize +
                  static_cast<float>(pinnedTray.size() - 1) * kTraySlotGap + kTraySlotGap;
    l.trayLeft = l.chevronLeft - trayTotalW;
    l.maxAppX = l.trayLeft - kSectionGap - kSlotSize;
    return l;
}

TaskbarUI::HitResult TaskbarUI::hitTest(float dipX, float dipY) const {
    const Layout l = computeLayout(m_barWidthDip, static_cast<float>(kBarHeightDip));
    if (dipY < l.slotTop - 2.0f || dipY > l.slotTop + kSlotSize + 2.0f) {
        return {};
    }
    if (dipX >= l.startLeft && dipX <= l.startLeft + kSlotSize) {
        return {HitZone::Start, -1};
    }
    if (m_showTaskView && dipX >= l.taskViewLeft && dipX <= l.taskViewLeft + kSlotSize) {
        return {HitZone::TaskView, -1};
    }
    if (dipX >= l.clockLeft - 4.0f && dipX <= l.clockRight + 4.0f) {
        return {HitZone::Clock, -1};
    }
    if (dipX >= l.chevronLeft && dipX <= l.traySectionRight) {
        return {HitZone::TrayOverflow, -1};
    }
    const auto& pinnedTray = m_tray.pinnedItems();
    if (!pinnedTray.empty() && dipX >= l.trayLeft && dipX < l.chevronLeft) {
        float tx = l.trayLeft;
        for (int i = 0; i < static_cast<int>(pinnedTray.size()); ++i) {
            if (dipX >= tx && dipX <= tx + kTraySlotSize) {
                return {HitZone::Tray, i};
            }
            tx += kTraySlotSize + kTraySlotGap;
        }
    }
    const auto& items = m_tasks.items();
    float x = l.appsStartX;
    for (int i = 0; i < static_cast<int>(items.size()); ++i) {
        if (x > l.maxAppX) {
            break;
        }
        if (dipX >= x && dipX <= x + kSlotSize) {
            return {HitZone::App, i};
        }
        x += kSlotSize + kSlotGap;
    }
    return {};
}

float TaskbarUI::slotCenterDipX(const HitResult& hit) const {
    const Layout l = computeLayout(m_barWidthDip, static_cast<float>(kBarHeightDip));
    switch (hit.zone) {
    case HitZone::Start:
        return l.startLeft + kSlotSize * 0.5f;
    case HitZone::TaskView:
        return l.taskViewLeft + kSlotSize * 0.5f;
    case HitZone::App:
        if (hit.index >= 0) {
            return l.appsStartX + static_cast<float>(hit.index) * (kSlotSize + kSlotGap) +
                   kSlotSize * 0.5f;
        }
        break;
    case HitZone::Clock:
        return (l.clockLeft + l.clockRight) * 0.5f;
    case HitZone::TrayOverflow:
        return l.chevronLeft + kChevronWidth * 0.5f;
    case HitZone::Tray:
        if (hit.index >= 0) {
            return l.trayLeft + static_cast<float>(hit.index) * (kTraySlotSize + kTraySlotGap) +
                   kTraySlotSize * 0.5f;
        }
        break;
    default:
        break;
    }
    return m_barWidthDip * 0.5f;
}

int TaskbarUI::screenXFromDip(float dipX) const {
    RECT wndRc{};
    GetWindowRect(m_hwnd, &wndRc);
    const float scale = static_cast<float>(GetDpiForWindow(m_hwnd)) / 96.0f;
    return wndRc.left + static_cast<int>(std::lround(dipX * scale));
}

int TaskbarUI::taskbarTopScreen() const {
    RECT wndRc{};
    GetWindowRect(m_hwnd, &wndRc);
    return wndRc.top;
}

void TaskbarUI::showHoverFlyout(const HitResult& hit) {
    if (m_flyout.isInteractive() || m_dragging) {
        return;
    }
    const int anchorX = screenXFromDip(slotCenterDipX(hit));
    const int barTop = taskbarTopScreen();
    if (hit.zone == HitZone::Start) {
        m_flyout.showTooltip(anchorX, barTop, L"Start");
    } else if (hit.zone == HitZone::TaskView) {
        m_flyout.showTooltip(anchorX, barTop, L"Task View");
    } else if (hit.zone == HitZone::TrayOverflow) {
        m_flyout.showTooltip(anchorX, barTop, L"Show hidden icons");
    } else if (hit.zone == HitZone::Tray && hit.index >= 0 &&
               hit.index < static_cast<int>(m_tray.pinnedItems().size())) {
        m_flyout.showTooltip(anchorX, barTop,
                             m_tray.pinnedItems()[static_cast<size_t>(hit.index)].tooltip);
    } else if (hit.zone == HitZone::App && hit.index >= 0 &&
               hit.index < static_cast<int>(m_tasks.items().size())) {
        const auto& item = m_tasks.items()[static_cast<size_t>(hit.index)];
        if (item.windows.size() > 1) {
            m_flyout.showWindowList(anchorX, barTop, static_cast<size_t>(hit.index), item);
        } else {
            m_flyout.showTooltip(anchorX, barTop, item.displayTitle());
        }
    } else {
        m_flyout.hide();
    }
}

void TaskbarUI::showNativeSystemMenu(HWND target, int screenX, int screenY) {
    if (!IsWindow(target)) {
        return;
    }
    HMENU sysMenu = GetSystemMenu(target, FALSE);
    if (!sysMenu) {
        return;
    }
    SetForegroundWindow(target);
    const UINT cmd = TrackPopupMenu(sysMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON, screenX, screenY, 0,
                                    m_hwnd, nullptr);
    if (cmd != 0) {
        PostMessageW(target, WM_SYSCOMMAND, cmd, 0);
    }
}

void TaskbarUI::showBarContextMenu(int screenX, int screenY) {
    const std::unique_ptr<std::remove_pointer_t<HMENU>, decltype(&DestroyMenu)> menu(
        CreatePopupMenu(), &DestroyMenu);
    if (!menu) {
        return;
    }
    AppendMenuW(menu.get(), MF_STRING, 1, L"Taskbar settings");
    AppendMenuW(menu.get(), MF_STRING, 2, L"Task Manager");
    SetForegroundWindow(m_hwnd);
    const UINT cmd = TrackPopupMenu(menu.get(), TPM_RETURNCMD | TPM_RIGHTBUTTON, screenX, screenY,
                                    0, m_hwnd, nullptr);
    PostMessageW(m_hwnd, WM_NULL, 0, 0);
    if (cmd == 1) {
        ShellTaskbar::openTaskbarSettings();
    } else if (cmd == 2) {
        ShellTaskbar::openTaskManager();
    }
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
    const float dipX = static_cast<float>(xPx) / scale;
    const float dipY = static_cast<float>(yPx) / scale;
    if (m_pointerDown) {
        const int dx = xPx - m_dragStartPx.x;
        const int dy = yPx - m_dragStartPx.y;
        const int dragThreshold = GetSystemMetrics(SM_CXDRAG);
        if (!m_dragging && (std::abs(dx) > dragThreshold || std::abs(dy) > dragThreshold)) {
            const bool appDrag = m_dragZone == HitZone::App && m_dragFromIndex >= 0 &&
                                 m_dragFromIndex < static_cast<int>(m_tasks.items().size());
            const bool trayDrag = m_dragZone == HitZone::Tray && m_dragFromIndex >= 0;
            if (appDrag || trayDrag) {
                m_dragging = true;
                m_flyout.hide();
            }
        }
        if (m_dragging) {
            m_dragDeltaDip = static_cast<float>(dx) / scale;
            const Layout l = computeLayout(m_barWidthDip, static_cast<float>(kBarHeightDip));
            if (m_dragZone == HitZone::App) {
                int pinnedCount = 0;
                for (const auto& item : m_tasks.items()) {
                    if (item.pinned) {
                        ++pinnedCount;
                    }
                }
                const int rawIdx = static_cast<int>(std::lround(
                    (dipX - l.appsStartX - kSlotSize * 0.5f) / (kSlotSize + kSlotGap)));
                m_dragToIndex = pinnedCount > 0 ? std::clamp(rawIdx, 0, pinnedCount - 1) : 0;
            } else if (m_dragZone == HitZone::Tray) {
                const auto& pinnedTray = m_tray.pinnedItems();
                if (!pinnedTray.empty()) {
                    const int rawIdx = static_cast<int>(std::lround(
                        (dipX - l.trayLeft - kTraySlotSize * 0.5f) / (kTraySlotSize + kTraySlotGap)));
                    m_dragToIndex =
                        std::clamp(rawIdx, 0, static_cast<int>(pinnedTray.size()) - 1);
                }
            }
            InvalidateRect(m_hwnd, nullptr, FALSE);
            return;
        }
    }
    const HitResult hit = hitTest(dipX, dipY);
    if (hit != m_hover) {
        m_hover = hit;
        showHoverFlyout(hit);
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
}

void TaskbarUI::onLeftDown(int xPx, int yPx) {
    m_flyout.hide();
    const float scale = static_cast<float>(GetDpiForWindow(m_hwnd)) / 96.0f;
    const HitResult hit = hitTest(static_cast<float>(xPx) / scale, static_cast<float>(yPx) / scale);
    m_pointerDown = true;
    m_dragging = false;
    m_dragStartPx = POINT{xPx, yPx};
    m_dragZone = hit.zone;
    m_dragFromIndex = hit.index;
    m_dragToIndex = hit.index;
    m_dragDeltaDip = 0.0f;
    m_dragWasPinned = false;
    if (hit.zone == HitZone::App && hit.index >= 0 &&
        hit.index < static_cast<int>(m_tasks.items().size())) {
        m_dragWasPinned = m_tasks.items()[static_cast<size_t>(hit.index)].pinned;
    }
    SetCapture(m_hwnd);
}

void TaskbarUI::onLeftUp(int xPx, int yPx) {
    const bool wasDragging = m_dragging;
    const HitZone dragZone = m_dragZone;
    const int fromIdx = m_dragFromIndex;
    const int toIdx = m_dragToIndex;
    const bool wasPinned = m_dragWasPinned;
    m_pointerDown = false;
    m_dragging = false;
    m_dragZone = HitZone::None;
    m_dragFromIndex = -1;
    m_dragToIndex = -1;
    m_dragDeltaDip = 0.0f;
    if (GetCapture() == m_hwnd) {
        ReleaseCapture();
    }
    if (wasDragging) {
        if (dragZone == HitZone::App && fromIdx >= 0 && toIdx >= 0) {
            if (wasPinned) {
                if (fromIdx != toIdx) {
                    m_tasks.movePinnedItem(static_cast<size_t>(fromIdx), static_cast<size_t>(toIdx));
                }
            } else if (fromIdx < static_cast<int>(m_tasks.items().size())) {
                // Dragging a running unpinned app pins it at the drop position.
                const TaskItem item = m_tasks.items()[static_cast<size_t>(fromIdx)];
                if (m_tasks.pinItem(item) && !m_tasks.items().empty()) {
                    const size_t last = m_tasks.items().size() - 1;
                    const size_t target =
                        std::min(static_cast<size_t>(toIdx), last);
                    if (last != target) {
                        m_tasks.movePinnedItem(last, target);
                    }
                }
            }
        } else if (dragZone == HitZone::Tray && fromIdx >= 0 && toIdx >= 0 && fromIdx != toIdx) {
            m_tray.movePinnedTrayItem(static_cast<size_t>(fromIdx), static_cast<size_t>(toIdx));
        }
        InvalidateRect(m_hwnd, nullptr, FALSE);
        return;
    }
    const float scale = static_cast<float>(GetDpiForWindow(m_hwnd)) / 96.0f;
    const HitResult hit = hitTest(static_cast<float>(xPx) / scale, static_cast<float>(yPx) / scale);
    switch (hit.zone) {
    case HitZone::Start:
        m_flyout.hide();
        ShellTaskbar::openStartMenu();
        break;
    case HitZone::TaskView:
        m_flyout.hide();
        ShellTaskbar::openTaskView();
        break;
    case HitZone::App:
        m_flyout.hide();
        if (hit.index >= 0 && hit.index < static_cast<int>(m_tasks.items().size())) {
            m_tasks.activateOrLaunch(m_tasks.items()[static_cast<size_t>(hit.index)]);
        }
        break;
    case HitZone::Tray:
        m_flyout.hide();
        if (hit.index >= 0 && hit.index < static_cast<int>(m_tray.pinnedItems().size())) {
            m_tray.leftClick(m_tray.pinnedItems()[static_cast<size_t>(hit.index)]);
        }
        break;
    case HitZone::TrayOverflow:
        if (m_flyout.mode() == TaskbarFlyout::Mode::TrayOverflow) {
            m_flyout.hide();
        } else {
            m_tray.refresh();
            m_flyout.showTrayOverflow(screenXFromDip(slotCenterDipX(hit)), taskbarTopScreen(),
                                      m_tray.overflowItems());
        }
        break;
    case HitZone::Clock:
        m_flyout.hide();
        ShellTaskbar::openNotificationCenter();
        break;
    case HitZone::None:
        m_flyout.hide();
        break;
    }
}

void TaskbarUI::onLeftDoubleClick(int xPx, int yPx) {
    const float scale = static_cast<float>(GetDpiForWindow(m_hwnd)) / 96.0f;
    const HitResult hit = hitTest(static_cast<float>(xPx) / scale, static_cast<float>(yPx) / scale);
    if (hit.zone == HitZone::Tray && hit.index >= 0 &&
        hit.index < static_cast<int>(m_tray.pinnedItems().size())) {
        m_tray.doubleClick(m_tray.pinnedItems()[static_cast<size_t>(hit.index)]);
    }
}

void TaskbarUI::onRightUp(int xPx, int yPx) {
    const float scale = static_cast<float>(GetDpiForWindow(m_hwnd)) / 96.0f;
    const HitResult hit = hitTest(static_cast<float>(xPx) / scale, static_cast<float>(yPx) / scale);
    if (hit.zone == HitZone::App && hit.index >= 0 &&
        hit.index < static_cast<int>(m_tasks.items().size())) {
        const auto& item = m_tasks.items()[static_cast<size_t>(hit.index)];
        const bool shiftHeld = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        if (shiftHeld && item.running && item.primaryHwnd()) {
            POINT pt{xPx, yPx};
            ClientToScreen(m_hwnd, &pt);
            showNativeSystemMenu(item.primaryHwnd(), pt.x, pt.y);
        } else {
            m_flyout.showAppContextMenu(screenXFromDip(slotCenterDipX(hit)), taskbarTopScreen(),
                                        static_cast<size_t>(hit.index), item, m_tasks.canPin(item));
        }
        return;
    }
    if (hit.zone == HitZone::Tray && hit.index >= 0 &&
        hit.index < static_cast<int>(m_tray.pinnedItems().size())) {
        m_flyout.hide();
        m_tray.rightClick(m_tray.pinnedItems()[static_cast<size_t>(hit.index)]);
        return;
    }
    if (hit.zone == HitZone::None) {
        m_flyout.hide();
        POINT pt{xPx, yPx};
        ClientToScreen(m_hwnd, &pt);
        showBarContextMenu(pt.x, pt.y);
    }
}

void TaskbarUI::onMiddleUp(int xPx, int yPx) {
    const float scale = static_cast<float>(GetDpiForWindow(m_hwnd)) / 96.0f;
    const HitResult hit = hitTest(static_cast<float>(xPx) / scale, static_cast<float>(yPx) / scale);
    if (hit.zone == HitZone::App && hit.index >= 0 &&
        hit.index < static_cast<int>(m_tasks.items().size())) {
        const auto& item = m_tasks.items()[static_cast<size_t>(hit.index)];
        if (item.running) {
            m_tasks.closeAllWindows(item);
        } else {
            m_tasks.launchNewInstance(item);
        }
    } else if (hit.zone == HitZone::Tray && hit.index >= 0 &&
               hit.index < static_cast<int>(m_tray.pinnedItems().size())) {
        m_tray.setPinnedToBar(m_tray.pinnedItems()[static_cast<size_t>(hit.index)].persistKey,
                              false);
    }
}

void TaskbarUI::paint() {
    PAINTSTRUCT ps{};
    BeginPaint(m_hwnd, &ps);
    const HRESULT hr = m_renderer.draw([this](ID2D1DeviceContext& dc) {
        const D2D1_SIZE_F size = dc.GetSize();
        const Layout l = computeLayout(size.width, size.height);
        const D2D1_RECT_F fullRect = D2D1::RectF(0.0f, 0.0f, size.width, size.height);
        dc.FillRectangle(fullRect, m_renderer.brush(kGlassBaseTint));
        if (auto sheenBrush = m_renderer.verticalGradient(0.0f, size.height, kSheenTop, kSheenMid,
                                                          0.42f, kSheenBottom)) {
            dc.FillRectangle(fullRect, sheenBrush.Get());
        }
        dc.FillRectangle(D2D1::RectF(0.0f, 0.0f, size.width, 1.0f), m_renderer.brush(kTopSpecular));
        dc.FillRectangle(D2D1::RectF(0.0f, 1.0f, size.width, 2.0f), m_renderer.brush(kInnerBevel));
        const auto drawGlassLens = [&](const D2D1_RECT_F& rect, bool active, bool hovered,
                                       bool dragging = false) {
            if (!active && !hovered && !dragging) {
                return;
            }
            const D2D1_ROUNDED_RECT lens = D2D1::RoundedRect(rect, kSlotRadius, kSlotRadius);
            const D2D1_COLOR_F fill =
                dragging ? kLensDragFill : (active ? kLensActiveFill : kLensHoverFill);
            dc.FillRoundedRectangle(lens, m_renderer.brush(fill));
            if (auto lensRim = m_renderer.verticalGradient(
                    rect.top, rect.bottom, (active || dragging) ? kLensRimTop : kLensRimMid,
                    kInnerBevel, 0.5f, kLensRimBottom)) {
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
        // Start emblem
        const D2D1_RECT_F startRect =
            D2D1::RectF(l.startLeft, l.slotTop, l.startLeft + kSlotSize, l.slotTop + kSlotSize);
        const bool startHovered = (m_hover.zone == HitZone::Start);
        drawGlassLens(startRect, false, startHovered);
        const float cx = (startRect.left + startRect.right) / 2.0f;
        const float cy = (startRect.top + startRect.bottom) / 2.0f;
        const float pane = 8.0f;
        const float gap = 0.75f;
        const float outerR = 2.2f;
        const float innerR = 0.4f;
        const D2D1_RECT_F panes[4]{
            D2D1::RectF(cx - gap - pane, cy - gap - pane, cx - gap, cy - gap),
            D2D1::RectF(cx + gap, cy - gap - pane, cx + gap + pane, cy - gap),
            D2D1::RectF(cx - gap - pane, cy + gap, cx - gap, cy + gap + pane),
            D2D1::RectF(cx + gap, cy + gap, cx + gap + pane, cy + gap + pane),
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
                dc.DrawGeometry(geo.Get(), m_renderer.brush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.24f)),
                                0.5f);
            }
        }
        float divX = l.startLeft + kSlotSize + kSectionGap;
        drawGlassDivider(divX);
        // Task View glyph
        if (m_showTaskView) {
            const D2D1_RECT_F tvRect = D2D1::RectF(l.taskViewLeft, l.slotTop,
                                                   l.taskViewLeft + kSlotSize, l.slotTop + kSlotSize);
            const bool tvHovered = (m_hover.zone == HitZone::TaskView);
            drawGlassLens(tvRect, false, tvHovered);
            const float tvCx = (tvRect.left + tvRect.right) * 0.5f;
            const float tvCy = (tvRect.top + tvRect.bottom) * 0.5f;
            const D2D1_ROUNDED_RECT back = D2D1::RoundedRect(
                D2D1::RectF(tvCx - 7.0f, tvCy - 6.0f, tvCx + 5.0f, tvCy + 4.0f), 2.0f, 2.0f);
            const D2D1_ROUNDED_RECT front = D2D1::RoundedRect(
                D2D1::RectF(tvCx - 3.0f, tvCy - 2.0f, tvCx + 8.0f, tvCy + 7.0f), 2.0f, 2.0f);
            dc.DrawRoundedRectangle(back, m_renderer.brush(kMuted), 1.3f);
            dc.FillRoundedRectangle(front, m_renderer.brush(kGlassBaseTint));
            dc.DrawRoundedRectangle(front, m_renderer.brush(tvHovered ? kAccent : kMuted), 1.3f);
        }
        // Right cluster dividers
        drawGlassDivider(l.clockLeft - kSectionGap - kDividerWidth);
        const D2D1_RECT_F chevronRect = D2D1::RectF(l.chevronLeft, l.traySlotTop, l.traySectionRight,
                                                    l.traySlotTop + kTraySlotSize);
        drawGlassLens(chevronRect, m_flyout.mode() == TaskbarFlyout::Mode::TrayOverflow,
                      m_hover.zone == HitZone::TrayOverflow);
        const float chevCx = (chevronRect.left + chevronRect.right) * 0.5f;
        const float chevCy = (chevronRect.top + chevronRect.bottom) * 0.5f;
        dc.DrawLine(D2D1::Point2F(chevCx - 3.5f, chevCy + 1.8f), D2D1::Point2F(chevCx, chevCy - 1.8f),
                    m_renderer.brush(kMuted), 1.4f);
        dc.DrawLine(D2D1::Point2F(chevCx, chevCy - 1.8f), D2D1::Point2F(chevCx + 3.5f, chevCy + 1.8f),
                    m_renderer.brush(kMuted), 1.4f);
        // Pinned tray icons
        const auto& pinnedTray = m_tray.pinnedItems();
        auto drawTraySlot = [&](int i, float drawX, bool isDragged) {
            const auto& tItem = pinnedTray[static_cast<size_t>(i)];
            const D2D1_RECT_F tRect =
                D2D1::RectF(drawX, l.traySlotTop, drawX + kTraySlotSize, l.traySlotTop + kTraySlotSize);
            const bool hovered = (!m_dragging && m_hover.zone == HitZone::Tray && m_hover.index == i);
            drawGlassLens(tRect, false, hovered, isDragged);
            if (tItem.icon) {
                const float off = (kTraySlotSize - kTrayIconSize) * 0.5f;
                const D2D1_RECT_F iRect =
                    D2D1::RectF(drawX + off, l.traySlotTop + off, drawX + off + kTrayIconSize,
                                l.traySlotTop + off + kTrayIconSize);
                dc.DrawBitmap(tItem.icon.Get(), iRect, 1.0f,
                              D2D1_INTERPOLATION_MODE_HIGH_QUALITY_CUBIC);
            }
        };
        for (int i = 0; i < static_cast<int>(pinnedTray.size()); ++i) {
            if (m_dragging && m_dragZone == HitZone::Tray && i == m_dragFromIndex) {
                continue;
            }
            int visualSlot = i;
            if (m_dragging && m_dragZone == HitZone::Tray) {
                if (m_dragFromIndex < m_dragToIndex && i > m_dragFromIndex && i <= m_dragToIndex) {
                    visualSlot = i - 1;
                } else if (m_dragFromIndex > m_dragToIndex && i >= m_dragToIndex &&
                           i < m_dragFromIndex) {
                    visualSlot = i + 1;
                }
            }
            drawTraySlot(i, l.trayLeft + static_cast<float>(visualSlot) * (kTraySlotSize + kTraySlotGap),
                         false);
        }
        if (m_dragging && m_dragZone == HitZone::Tray && m_dragFromIndex >= 0 &&
            m_dragFromIndex < static_cast<int>(pinnedTray.size())) {
            const float baseX =
                l.trayLeft + static_cast<float>(m_dragFromIndex) * (kTraySlotSize + kTraySlotGap);
            drawTraySlot(m_dragFromIndex, baseX + m_dragDeltaDip, true);
        }
        // App slots
        const auto& items = m_tasks.items();
        auto drawAppSlot = [&](int i, float drawX, bool isDragged) {
            const auto& item = items[static_cast<size_t>(i)];
            const D2D1_RECT_F slotRect =
                D2D1::RectF(drawX, l.slotTop, drawX + kSlotSize, l.slotTop + kSlotSize);
            const bool hovered = (!m_dragging && m_hover.zone == HitZone::App && m_hover.index == i);
            drawGlassLens(slotRect, item.active, hovered, isDragged);
            if (item.icon) {
                const float iconOffset = (kSlotSize - kIconSize) * 0.5f;
                const D2D1_RECT_F iconRect =
                    D2D1::RectF(drawX + iconOffset, l.slotTop + iconOffset - 0.5f,
                                drawX + iconOffset + kIconSize, l.slotTop + iconOffset + kIconSize - 0.5f);
                const float iconOpacity = (item.running || hovered || isDragged) ? 1.0f : 0.92f;
                dc.DrawBitmap(item.icon.Get(), iconRect, iconOpacity,
                              D2D1_INTERPOLATION_MODE_HIGH_QUALITY_CUBIC);
            }
            if (item.running) {
                if (item.windows.size() > 1) {
                    const float totalW = 14.0f;
                    const float leftPillX = drawX + (kSlotSize - totalW) * 0.5f;
                    const D2D1_RECT_F p1 = D2D1::RectF(leftPillX, size.height - 4.5f, leftPillX + 8.0f,
                                                       size.height - 2.5f);
                    const D2D1_RECT_F p2 = D2D1::RectF(leftPillX + 10.0f, size.height - 4.5f,
                                                       leftPillX + totalW, size.height - 2.5f);
                    dc.FillRoundedRectangle(D2D1::RoundedRect(p1, 1.0f, 1.0f),
                                            m_renderer.brush(item.active ? kAccent : kMuted));
                    dc.FillRoundedRectangle(D2D1::RoundedRect(p2, 1.0f, 1.0f),
                                            m_renderer.brush(kMuted));
                } else {
                    const float pillWidth = item.active ? 12.0f : 4.5f;
                    const float pillX = drawX + (kSlotSize - pillWidth) * 0.5f;
                    if (item.active) {
                        const D2D1_RECT_F glowRect = D2D1::RectF(pillX - 2.0f, size.height - 5.5f,
                                                                 pillX + pillWidth + 2.0f,
                                                                 size.height - 1.5f);
                        dc.FillRoundedRectangle(D2D1::RoundedRect(glowRect, 2.0f, 2.0f),
                                                m_renderer.brush(kAccentGlow));
                    }
                    const D2D1_RECT_F pillRect = D2D1::RectF(pillX, size.height - 4.5f,
                                                             pillX + pillWidth, size.height - 2.5f);
                    dc.FillRoundedRectangle(D2D1::RoundedRect(pillRect, 1.0f, 1.0f),
                                            m_renderer.brush(item.active ? kAccent : kMuted));
                }
            }
        };
        for (int i = 0; i < static_cast<int>(items.size()); ++i) {
            if (m_dragging && m_dragZone == HitZone::App && i == m_dragFromIndex) {
                continue;
            }
            int visualSlot = i;
            if (m_dragging && m_dragZone == HitZone::App && m_dragWasPinned) {
                if (m_dragFromIndex < m_dragToIndex && i > m_dragFromIndex && i <= m_dragToIndex) {
                    visualSlot = i - 1;
                } else if (m_dragFromIndex > m_dragToIndex && i >= m_dragToIndex &&
                           i < m_dragFromIndex) {
                    visualSlot = i + 1;
                }
            }
            const float drawX = l.appsStartX + static_cast<float>(visualSlot) * (kSlotSize + kSlotGap);
            if (drawX > l.maxAppX) {
                break;
            }
            drawAppSlot(i, drawX, false);
        }
        if (m_dragging && m_dragZone == HitZone::App && m_dragFromIndex >= 0 &&
            m_dragFromIndex < static_cast<int>(items.size())) {
            const float baseX =
                l.appsStartX + static_cast<float>(m_dragFromIndex) * (kSlotSize + kSlotGap);
            drawAppSlot(m_dragFromIndex, baseX + m_dragDeltaDip, true);
        }
        // Clock cluster
        if (m_hover.zone == HitZone::Clock) {
            const D2D1_RECT_F clockHoverRect =
                D2D1::RectF(l.clockLeft - 4.0f, l.slotTop, l.clockRight + 4.0f, l.slotTop + kSlotSize);
            drawGlassLens(clockHoverRect, false, true);
        }
        const D2D1_RECT_F timeRect = D2D1::RectF(l.clockLeft, 6.0f, l.clockRight, 24.0f);
        const D2D1_RECT_F dateRect = D2D1::RectF(l.clockLeft, 23.0f, l.clockRight, 39.0f);
        const D2D1_RECT_F timeShadowRect = D2D1::RectF(l.clockLeft, 7.0f, l.clockRight, 25.0f);
        const D2D1_RECT_F dateShadowRect = D2D1::RectF(l.clockLeft, 24.0f, l.clockRight, 40.0f);
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