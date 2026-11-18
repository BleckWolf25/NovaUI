/**
 * @file TaskbarFlyout.cpp
 *
 * @version 1.2.0
 * @author BleckWolf25
 * @license MIT
 *
 * @summary Implementation of the blur-free glass flyout popup window
 *
 * @description
 * Renders crisp, ultra-transparent glass popups above the taskbar without blur. Interactive
 * modes arm a FlyoutDismissHook that posts WM_APP_FLYOUT_DISMISS when the pointer leaves the
 * popup/taskbar region or clicks outside, and focus loss (WM_ACTIVATE == WA_INACTIVE) also
 * dismisses. Routes clicks to window management, app pinning, or system tray applications.
 *
 * @since 17/11/2026
 * @updated 18/11/2026
 */
// ---------- IMPORTS
#include "Modules/TaskbarFlyout.h"

#include <windowsx.h>
#include <algorithm>
#include <cmath>

// ---------- CONSTANTS
namespace {
constexpr float kGapAboveBarDip = 8.0f;
constexpr float kCornerRadius = 12.0f;
constexpr float kPadX = 10.0f;
constexpr float kPadY = 8.0f;
constexpr float kRowHeight = 32.0f;
constexpr float kTrayCellSize = 40.0f;
constexpr float kTrayCellGap = 6.0f;
constexpr int kTrayCols = 4;

// Ultra-sheer blur-free dark glass palette
constexpr D2D1_COLOR_F kBgTint{0.05f, 0.065f, 0.09f, 0.72f};
constexpr D2D1_COLOR_F kSheenTop{1.0f, 1.0f, 1.0f, 0.09f};
constexpr D2D1_COLOR_F kSheenMid{1.0f, 1.0f, 1.0f, 0.01f};
constexpr D2D1_COLOR_F kSheenBottom{0.78f, 0.86f, 1.0f, 0.03f};
constexpr D2D1_COLOR_F kBorderRim{1.0f, 1.0f, 1.0f, 0.24f};
constexpr D2D1_COLOR_F kRowHover{1.0f, 1.0f, 1.0f, 0.12f};
constexpr D2D1_COLOR_F kCloseHover{0.92f, 0.30f, 0.34f, 0.82f};
constexpr D2D1_COLOR_F kPinBadgeBg{0.46f, 0.78f, 1.0f, 0.26f};
constexpr D2D1_COLOR_F kAccent{0.46f, 0.78f, 1.0f, 1.0f};
constexpr D2D1_COLOR_F kText{1.0f, 1.0f, 1.0f, 0.96f};
constexpr D2D1_COLOR_F kMuted{1.0f, 1.0f, 1.0f, 0.65f};
}

// ---------- IMPLEMENTATION
TaskbarFlyout::~TaskbarFlyout() {
    m_dismiss.disarm();
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
    }
}

bool TaskbarFlyout::init(HWND taskbarHwnd, Callbacks callbacks) {
    m_taskbarHwnd = taskbarHwnd;
    m_callbacks = std::move(callbacks);

    constexpr DWORD exStyle = WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOREDIRECTIONBITMAP;
    if (!create(L"NovaUI.Flyout", WS_POPUP, exStyle)) {
        return false;
    }
    applyBackdrop();

    if (FAILED(m_renderer.init(m_hwnd))) {
        return false;
    }
    m_renderer.setDpi(static_cast<float>(GetDpiForWindow(m_hwnd)));

    m_titleFormat =
        m_renderer.createFormat(11.5f, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_TEXT_ALIGNMENT_LEADING);
    m_itemFormat =
        m_renderer.createFormat(12.5f, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_LEADING);
    m_badgeFormat =
        m_renderer.createFormat(10.0f, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_TEXT_ALIGNMENT_CENTER);

    return m_titleFormat && m_itemFormat && m_badgeFormat;
}

void TaskbarFlyout::showTooltip(int anchorCenterXScreen, int taskbarTopScreen,
                                const std::wstring& text) {
    if (text.empty()) {
        hide();
        return;
    }
    m_mode = Mode::Tooltip;
    m_tooltipText = text;
    m_hoverRow = -1;

    const float estChars = static_cast<float>(std::min<size_t>(text.size(), 44));
    const float widthDip = std::clamp(estChars * 6.8f + 24.0f, 84.0f, 320.0f);
    const float heightDip = 30.0f;

    positionAndShow(anchorCenterXScreen, taskbarTopScreen, widthDip, heightDip, false);
}

void TaskbarFlyout::showWindowList(int anchorCenterXScreen, int taskbarTopScreen, size_t appIndex,
                                   const TaskItem& item) {
    m_mode = Mode::WindowList;
    m_appIndex = appIndex;
    m_headerTitle = item.appName;
    m_windows = item.windows;
    m_hoverRow = -1;
    m_hoverCloseBtn = false;

    const float widthDip = 256.0f;
    const float heightDip =
        kPadY * 2.0f + 22.0f + static_cast<float>(m_windows.size()) * kRowHeight;

    positionAndShow(anchorCenterXScreen, taskbarTopScreen, widthDip, heightDip, false);
}

void TaskbarFlyout::showAppContextMenu(int anchorCenterXScreen, int taskbarTopScreen,
                                       size_t appIndex, const TaskItem& item, bool canPin) {
    m_mode = Mode::AppContextMenu;
    m_appIndex = appIndex;
    m_headerTitle = item.appName;
    m_menuActions.clear();
    m_hoverRow = -1;

    if (!item.shortcutPath.empty() || !item.targetPath.empty()) {
        m_menuActions.push_back(
            {MenuAction::Kind::LaunchNew, item.running ? L"Open new window" : L"Open"});
    }
    if (item.pinned) {
        m_menuActions.push_back({MenuAction::Kind::TogglePin, L"Unpin from taskbar"});
    } else if (canPin) {
        m_menuActions.push_back({MenuAction::Kind::TogglePin, L"Pin to taskbar"});
    }
    if (item.running) {
        m_menuActions.push_back(
            {MenuAction::Kind::CloseAll,
             item.windows.size() > 1 ? L"Close all windows" : L"Close window"});
    }

    if (m_menuActions.empty()) {
        hide();
        return;
    }

    const float widthDip = 204.0f;
    const float heightDip =
        kPadY * 2.0f + 22.0f + static_cast<float>(m_menuActions.size()) * kRowHeight;

    positionAndShow(anchorCenterXScreen, taskbarTopScreen, widthDip, heightDip, true);
}

void TaskbarFlyout::showTrayOverflow(int anchorCenterXScreen, int taskbarTopScreen,
                                     const std::vector<TrayItem>& overflowItems) {
    m_mode = Mode::TrayOverflow;
    m_trayItems = overflowItems;
    m_hoverRow = -1;
    m_hoverPinBtn = false;

    const int count = std::max(1, static_cast<int>(m_trayItems.size()));
    const int cols = std::min(kTrayCols, count);
    const int rows = (count + kTrayCols - 1) / kTrayCols;

    const float gridW =
        static_cast<float>(cols) * kTrayCellSize + static_cast<float>(cols - 1) * kTrayCellGap;
    const float widthDip = std::max(176.0f, kPadX * 2.0f + gridW);
    const float heightDip = kPadY * 2.0f + 24.0f + static_cast<float>(rows) * kTrayCellSize +
                            static_cast<float>(std::max(0, rows - 1)) * kTrayCellGap;

    positionAndShow(anchorCenterXScreen, taskbarTopScreen, widthDip, heightDip, true);
}

void TaskbarFlyout::hide() {
    if (m_mode == Mode::Hidden) {
        return;
    }
    m_mode = Mode::Hidden;
    m_hoverRow = -1;
    m_hoverCloseBtn = false;
    m_hoverPinBtn = false;
    m_dismiss.disarm();
    if (m_hwnd) {
        ShowWindow(m_hwnd, SW_HIDE);
    }
}

LRESULT TaskbarFlyout::onMessage(UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_PAINT:
        paint();
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_SIZE:
        m_renderer.resize(LOWORD(lp), HIWORD(lp));
        return 0;
    case WM_ACTIVATE:
        if (LOWORD(wp) == WA_INACTIVE && isInteractive()) {
            hide();
        }
        return 0;
    case WM_KEYDOWN:
        if (wp == VK_ESCAPE) {
            hide();
            return 0;
        }
        break;
    case WM_MOUSEMOVE:
        onMouseMove(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
        return 0;
    case WM_MOUSELEAVE:
        m_trackingMouse = false;
        m_hoverRow = -1;
        m_hoverCloseBtn = false;
        m_hoverPinBtn = false;
        if (m_mode == Mode::WindowList) {
            hide();
        } else {
            InvalidateRect(m_hwnd, nullptr, FALSE);
        }
        return 0;
    case WM_LBUTTONUP:
        onLeftClick(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
        return 0;
    case WM_RBUTTONUP:
        onRightClick(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
        return 0;
    case WM_APP_FLYOUT_DISMISS:
        hide();
        return 0;
    default:
        break;
    }
    return Window::onMessage(msg, wp, lp);
}

void TaskbarFlyout::positionAndShow(int anchorCenterXScreen, int taskbarTopScreen, float widthDip,
                                    float heightDip, bool activate) {
    const HMONITOR mon =
        MonitorFromPoint(POINT{anchorCenterXScreen, taskbarTopScreen}, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    GetMonitorInfoW(mon, &mi);

    const UINT dpi = GetDpiForWindow(m_taskbarHwnd ? m_taskbarHwnd : m_hwnd);
    m_renderer.setDpi(static_cast<float>(dpi));
    const float scale = static_cast<float>(dpi) / 96.0f;

    const int widthPx = static_cast<int>(std::lround(widthDip * scale));
    const int heightPx = static_cast<int>(std::lround(heightDip * scale));
    const int gapPx = static_cast<int>(std::lround(kGapAboveBarDip * scale));

    int leftPx = anchorCenterXScreen - widthPx / 2;
    leftPx = std::clamp(leftPx, static_cast<int>(mi.rcWork.left) + 6,
                        static_cast<int>(mi.rcWork.right) - widthPx - 6);
    const int topPx = taskbarTopScreen - heightPx - gapPx;

    SetWindowPos(m_hwnd, HWND_TOPMOST, leftPx, topPx, widthPx, heightPx,
                 activate ? SWP_SHOWWINDOW : (SWP_NOACTIVATE | SWP_SHOWWINDOW));
    if (activate) {
        SetForegroundWindow(m_hwnd);
    }
    if (isInteractive()) {
        m_dismiss.arm(m_hwnd, m_taskbarHwnd, WM_APP_FLYOUT_DISMISS);
    } else {
        m_dismiss.disarm();
    }
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void TaskbarFlyout::onMouseMove(int xPx, int yPx) {
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

    RECT rc{};
    GetClientRect(m_hwnd, &rc);
    const float widthDip = static_cast<float>(rc.right - rc.left) / scale;

    int nextRow = -1;
    bool nextClose = false;
    bool nextPin = false;

    if (m_mode == Mode::WindowList || m_mode == Mode::AppContextMenu) {
        const float listTop = kPadY + 22.0f;
        const int maxRows = (m_mode == Mode::WindowList)
                                ? static_cast<int>(m_windows.size())
                                : static_cast<int>(m_menuActions.size());
        if (dipY >= listTop) {
            const int candidate = static_cast<int>((dipY - listTop) / kRowHeight);
            if (candidate >= 0 && candidate < maxRows) {
                nextRow = candidate;
                if (m_mode == Mode::WindowList && dipX >= widthDip - kPadX - 24.0f) {
                    nextClose = true;
                }
            }
        }
    } else if (m_mode == Mode::TrayOverflow && !m_trayItems.empty()) {
        const float gridTop = kPadY + 24.0f;
        if (dipY >= gridTop && dipX >= kPadX) {
            const int col = static_cast<int>((dipX - kPadX) / (kTrayCellSize + kTrayCellGap));
            const int row = static_cast<int>((dipY - gridTop) / (kTrayCellSize + kTrayCellGap));
            const float cellX =
                kPadX + static_cast<float>(col) * (kTrayCellSize + kTrayCellGap);
            const float cellY =
                gridTop + static_cast<float>(row) * (kTrayCellSize + kTrayCellGap);

            if (col >= 0 && col < kTrayCols && dipX <= cellX + kTrayCellSize &&
                dipY <= cellY + kTrayCellSize) {
                const int idx = row * kTrayCols + col;
                if (idx >= 0 && idx < static_cast<int>(m_trayItems.size())) {
                    nextRow = idx;
                    if (dipX >= cellX + kTrayCellSize - 14.0f && dipY <= cellY + 14.0f) {
                        nextPin = true;
                    }
                }
            }
        }
    }

    if (nextRow != m_hoverRow || nextClose != m_hoverCloseBtn || nextPin != m_hoverPinBtn) {
        m_hoverRow = nextRow;
        m_hoverCloseBtn = nextClose;
        m_hoverPinBtn = nextPin;
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
}

void TaskbarFlyout::onLeftClick(int xPx, int yPx) {
    onMouseMove(xPx, yPx);

    if (m_mode == Mode::WindowList && m_hoverRow >= 0 &&
        m_hoverRow < static_cast<int>(m_windows.size())) {
        HWND target = m_windows[static_cast<size_t>(m_hoverRow)].hwnd;
        hide();
        if (m_hoverCloseBtn) {
            if (m_callbacks.onCloseWindow) {
                m_callbacks.onCloseWindow(target);
            }
        } else if (m_callbacks.onFocusWindow) {
            m_callbacks.onFocusWindow(target);
        }
        return;
    }

    if (m_mode == Mode::AppContextMenu && m_hoverRow >= 0 &&
        m_hoverRow < static_cast<int>(m_menuActions.size())) {
        const auto kind = m_menuActions[static_cast<size_t>(m_hoverRow)].kind;
        const size_t appIdx = m_appIndex;
        hide();
        if (kind == MenuAction::Kind::LaunchNew && m_callbacks.onLaunchNew) {
            m_callbacks.onLaunchNew(appIdx);
        } else if (kind == MenuAction::Kind::TogglePin && m_callbacks.onTogglePinApp) {
            m_callbacks.onTogglePinApp(appIdx);
        } else if (kind == MenuAction::Kind::CloseAll && m_callbacks.onCloseAllWindows) {
            m_callbacks.onCloseAllWindows(appIdx);
        }
        return;
    }

    if (m_mode == Mode::TrayOverflow && m_hoverRow >= 0 &&
        m_hoverRow < static_cast<int>(m_trayItems.size())) {
        const TrayItem selected = m_trayItems[static_cast<size_t>(m_hoverRow)];
        const bool pinClicked = m_hoverPinBtn;
        hide();
        if (pinClicked) {
            if (m_callbacks.onTrayTogglePinToBar) {
                m_callbacks.onTrayTogglePinToBar(selected.persistKey);
            }
        } else if (m_callbacks.onTrayLeftClick) {
            m_callbacks.onTrayLeftClick(selected);
        }
    }
}

void TaskbarFlyout::onRightClick(int xPx, int yPx) {
    onMouseMove(xPx, yPx);

    if (m_mode == Mode::TrayOverflow && m_hoverRow >= 0 &&
        m_hoverRow < static_cast<int>(m_trayItems.size())) {
        const TrayItem selected = m_trayItems[static_cast<size_t>(m_hoverRow)];
        hide();
        if (m_callbacks.onTrayRightClick) {
            m_callbacks.onTrayRightClick(selected);
        }
    }
}

void TaskbarFlyout::paint() {
    PAINTSTRUCT ps{};
    BeginPaint(m_hwnd, &ps);

    const HRESULT hr = m_renderer.draw([this](ID2D1DeviceContext& dc) {
        const D2D1_SIZE_F size = dc.GetSize();
        const D2D1_ROUNDED_RECT panel = D2D1::RoundedRect(
            D2D1::RectF(0.5f, 0.5f, size.width - 0.5f, size.height - 0.5f), kCornerRadius,
            kCornerRadius);

        dc.FillRoundedRectangle(panel, m_renderer.brush(kBgTint));
        if (auto sheen = m_renderer.verticalGradient(0.0f, size.height, kSheenTop, kSheenMid, 0.45f,
                                                     kSheenBottom)) {
            dc.FillRoundedRectangle(panel, sheen.Get());
        }
        dc.DrawRoundedRectangle(panel, m_renderer.brush(kBorderRim), 1.0f);

        if (m_mode == Mode::Tooltip) {
            const D2D1_RECT_F textRect =
                D2D1::RectF(12.0f, 2.0f, size.width - 12.0f, size.height - 2.0f);
            m_titleFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            dc.DrawText(m_tooltipText.c_str(), static_cast<UINT32>(m_tooltipText.size()),
                        m_titleFormat.Get(), textRect, m_renderer.brush(kText));
            m_titleFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            return;
        }

        // Header label for WindowList, AppContextMenu, and TrayOverflow
        const std::wstring header =
            (m_mode == Mode::TrayOverflow)
                ? (m_hoverRow >= 0 && m_hoverRow < static_cast<int>(m_trayItems.size())
                       ? m_trayItems[static_cast<size_t>(m_hoverRow)].tooltip
                       : L"Hidden Tray Icons")
                : m_headerTitle;

        const D2D1_RECT_F headerRect = D2D1::RectF(kPadX + 4.0f, kPadY, size.width - kPadX, kPadY + 18.0f);
        dc.DrawText(header.c_str(), static_cast<UINT32>(header.size()), m_titleFormat.Get(),
                    headerRect, m_renderer.brush(kMuted));

        if (m_mode == Mode::WindowList) {
            float y = kPadY + 22.0f;
            for (int i = 0; i < static_cast<int>(m_windows.size()); ++i) {
                const auto& win = m_windows[static_cast<size_t>(i)];
                const D2D1_RECT_F rowRect = D2D1::RectF(kPadX, y, size.width - kPadX, y + kRowHeight);
                if (m_hoverRow == i) {
                    dc.FillRoundedRectangle(D2D1::RoundedRect(rowRect, 7.0f, 7.0f),
                                            m_renderer.brush(kRowHover));
                }
                if (win.active) {
                    dc.FillRoundedRectangle(
                        D2D1::RoundedRect(
                            D2D1::RectF(kPadX + 3.0f, y + 9.0f, kPadX + 6.0f, y + kRowHeight - 9.0f),
                            1.5f, 1.5f),
                        m_renderer.brush(kAccent));
                }

                const D2D1_RECT_F labelRect =
                    D2D1::RectF(kPadX + 12.0f, y, size.width - kPadX - 28.0f, y + kRowHeight);
                dc.DrawText(win.title.c_str(), static_cast<UINT32>(win.title.size()),
                            m_itemFormat.Get(), labelRect, m_renderer.brush(kText));

                const D2D1_RECT_F closeRect =
                    D2D1::RectF(size.width - kPadX - 22.0f, y + 6.0f, size.width - kPadX - 2.0f,
                                y + kRowHeight - 6.0f);
                if (m_hoverRow == i && m_hoverCloseBtn) {
                    dc.FillRoundedRectangle(D2D1::RoundedRect(closeRect, 5.0f, 5.0f),
                                            m_renderer.brush(kCloseHover));
                }
                dc.DrawText(L"\u00D7", 1, m_badgeFormat.Get(), closeRect, m_renderer.brush(kText));
                y += kRowHeight;
            }
            return;
        }

        if (m_mode == Mode::AppContextMenu) {
            float y = kPadY + 22.0f;
            for (int i = 0; i < static_cast<int>(m_menuActions.size()); ++i) {
                const auto& act = m_menuActions[static_cast<size_t>(i)];
                const D2D1_RECT_F rowRect = D2D1::RectF(kPadX, y, size.width - kPadX, y + kRowHeight);
                if (m_hoverRow == i) {
                    dc.FillRoundedRectangle(D2D1::RoundedRect(rowRect, 7.0f, 7.0f),
                                            m_renderer.brush(kRowHover));
                }
                const D2D1_RECT_F labelRect =
                    D2D1::RectF(kPadX + 10.0f, y, size.width - kPadX - 8.0f, y + kRowHeight);
                dc.DrawText(act.label.c_str(), static_cast<UINT32>(act.label.size()),
                            m_itemFormat.Get(), labelRect, m_renderer.brush(kText));
                y += kRowHeight;
            }
            return;
        }

        if (m_mode == Mode::TrayOverflow) {
            if (m_trayItems.empty()) {
                const D2D1_RECT_F emptyRect =
                    D2D1::RectF(kPadX, kPadY + 24.0f, size.width - kPadX, size.height - kPadY);
                dc.DrawText(L"All tray icons are on taskbar", 29, m_itemFormat.Get(), emptyRect,
                            m_renderer.brush(kMuted));
                return;
            }

            const float gridTop = kPadY + 24.0f;
            for (int i = 0; i < static_cast<int>(m_trayItems.size()); ++i) {
                const int col = i % kTrayCols;
                const int row = i / kTrayCols;
                const float cellX =
                    kPadX + static_cast<float>(col) * (kTrayCellSize + kTrayCellGap);
                const float cellY =
                    gridTop + static_cast<float>(row) * (kTrayCellSize + kTrayCellGap);

                const D2D1_RECT_F cellRect =
                    D2D1::RectF(cellX, cellY, cellX + kTrayCellSize, cellY + kTrayCellSize);
                if (m_hoverRow == i) {
                    dc.FillRoundedRectangle(D2D1::RoundedRect(cellRect, 8.0f, 8.0f),
                                            m_renderer.brush(kRowHover));
                }

                const auto& item = m_trayItems[static_cast<size_t>(i)];
                if (item.icon) {
                    const float iconOff = (kTrayCellSize - 18.0f) / 2.0f;
                    const D2D1_RECT_F iconRect =
                        D2D1::RectF(cellX + iconOff, cellY + iconOff, cellX + iconOff + 18.0f,
                                    cellY + iconOff + 18.0f);
                    dc.DrawBitmap(item.icon.Get(), iconRect, 1.0f,
                                  D2D1_INTERPOLATION_MODE_HIGH_QUALITY_CUBIC);
                }

                // Top-right "+" badge on hover to promote tray icon directly onto taskbar
                if (m_hoverRow == i) {
                    const D2D1_RECT_F pinBadge = D2D1::RectF(
                        cellX + kTrayCellSize - 14.0f, cellY + 2.0f, cellX + kTrayCellSize - 2.0f,
                        cellY + 14.0f);
                    dc.FillRoundedRectangle(
                        D2D1::RoundedRect(pinBadge, 4.0f, 4.0f),
                        m_renderer.brush(m_hoverPinBtn ? kAccent : kPinBadgeBg));
                    dc.DrawText(L"+", 1, m_badgeFormat.Get(), pinBadge, m_renderer.brush(kText));
                }
            }
        }
    });

    EndPaint(m_hwnd, &ps);
    if (hr == D2DERR_RECREATE_TARGET) {
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
}