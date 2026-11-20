/**
 * @file TaskbarUI.cpp
 *
 * @version 1.7.0
 * @author BleckWolf25
 * @license MIT
 *
 * @summary Dock window lifetime, message routing, input policy and shell integration
 *
 * @description
 * Snapshots the user's shell preferences and spawns the restore guard, starts the trackers
 * against the live shell before suppressing Explorer, and reserves the configured screen edge at
 * the thickness derived from the Windows size setting. Routes input to hover, click, middle-click,
 * right-click and drag gestures, delegating geometry to TaskbarLayout and drawing to paintTaskbar.
 * Empty-area right-click opens the styled glass taskbar menu; WM_SETTINGCHANGE re-reads
 * personalization and WM_APP_VISUALIZER pulls new audio levels.
 *
 * @since 17/11/2026
 * @updated 20/11/2026
 */
// ---------- IMPORTS
#include "Modules/TaskbarUI.h"
#include "Core/ShellRestore.h"
#include <shellapi.h>
#include <windowsx.h>
#include <algorithm>
#include <cmath>
// ---------- CONSTANTS
namespace {
constexpr UINT kMsgReassert = WM_APP + 4;
}
// ---------- IMPLEMENTATION
TaskbarUI::~TaskbarUI() {
    m_guard.stop();
    m_visualizer.stop();
    m_tray.stop();
    m_tasks.stop();
    m_shellTaskbar.restore();
    ShellRestore::apply();
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
    }
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
    m_settings = readTaskbarSettings();
    ShellRestore::capture();
    ShellRestore::spawnGuard(GetCurrentProcessId());
    m_visualizer.loadConfig();
    TaskbarFlyout::Callbacks callbacks{};
    callbacks.onFocusWindow = [this](HWND target) { m_tasks.focusWindow(target); };
    callbacks.onCloseWindow = [this](HWND target) { m_tasks.closeWindow(target); };
    callbacks.onLaunchNew = [this](size_t index) {
        if (index < m_tasks.items().size()) {
            m_tasks.launchNewInstance(m_tasks.items()[index]);
        }
    };
    callbacks.onTogglePinApp = [this](size_t index) {
        if (index < m_tasks.items().size()) {
            const auto& item = m_tasks.items()[index];
            if (item.pinned) {
                m_tasks.unpinItem(item);
            } else {
                m_tasks.pinItem(item);
            }
        }
    };
    callbacks.onCloseAllWindows = [this](size_t index) {
        if (index < m_tasks.items().size()) {
            m_tasks.closeAllWindows(m_tasks.items()[index]);
        }
    };
    callbacks.onTrayLeftClick = [this](const TrayItem& item) { m_tray.leftClick(item); };
    callbacks.onTrayRightClick = [this](const TrayItem& item) { m_tray.rightClick(item); };
    callbacks.onTrayTogglePinToBar = [this](const std::wstring& key) {
        m_tray.togglePinnedToBar(key);
    };
    callbacks.onOpenSettings = [] { ShellTaskbar::openTaskbarSettings(); };
    callbacks.onOpenTaskManager = [] { ShellTaskbar::openTaskManager(); };
    callbacks.onToggleVisualizer = [this] { toggleVisualizer(); };
    m_flyout.init(m_hwnd, std::move(callbacks));
    m_flyout.setPlacedBelowBar(m_settings.edge == TaskbarEdge::Top);
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
    m_guard.start(m_hwnd, kMsgReassert);
    if (m_visualizer.enabled()) {
        m_visualizer.start(m_hwnd);
    }
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
    case WM_SETTINGCHANGE:
        m_settings = readTaskbarSettings();
        m_flyout.setPlacedBelowBar(m_settings.edge == TaskbarEdge::Top);
        dock();
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
        if (!m_dragging && m_hover.zone != TaskbarZone::None) {
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
            m_dragZone = TaskbarZone::None;
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
        HDROP drop = reinterpret_cast<HDROP>(wp);
        const UINT count = DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
        for (UINT i = 0; i < count; ++i) {
            wchar_t path[MAX_PATH]{};
            if (DragQueryFileW(drop, i, path, MAX_PATH) > 0) {
                m_tasks.pinExternalPath(path);
            }
        }
        DragFinish(drop);
        return 0;
    }
    case WM_APP_APPBAR:
        if (wp == ABN_POSCHANGED) {
            dock();
        } else if (wp == ABN_FULLSCREENAPP) {
            const HWND zOrder = (lp != 0) ? HWND_BOTTOM : HWND_TOPMOST;
            SetWindowPos(m_hwnd, zOrder, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        }
        return 0;
    case TaskTracker::kMsgPinsChanged:
        m_tasks.reloadPinnedApps();
        return 0;
    case WM_APP_VISUALIZER:
        m_visualizer.pullLevels();
        InvalidateRect(m_hwnd, nullptr, FALSE);
        return 0;
    case kMsgReassert:
        reassertTaskbarWindow(m_hwnd);
        return 0;
    case WM_DESTROY:
        m_guard.stop();
        m_visualizer.stop();
        m_tray.stop();
        m_tasks.stop();
        m_clock.stop();
        m_appBar.remove(m_hwnd);
        m_shellTaskbar.restore();
        ShellRestore::apply();
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
    const float thicknessDip = metricsForSize(m_settings.sizeIndex).thickness;
    const int thicknessPx = static_cast<int>(std::lround(thicknessDip * scale));
    const RECT rc = m_appBar.dock(m_hwnd, thicknessPx, m_settings.edge);
    const int widthPx = rc.right - rc.left;
    const int heightPx = rc.bottom - rc.top;
    m_barWidthDip = static_cast<float>(widthPx) / scale;
    SetWindowPos(m_hwnd, HWND_TOPMOST, rc.left, rc.top, widthPx, heightPx, SWP_NOACTIVATE);
}

TaskbarLayout TaskbarUI::currentLayout() const {
    return computeTaskbarLayout(m_barWidthDip, m_settings,
                                static_cast<int>(m_tasks.items().size()),
                                static_cast<int>(m_tray.pinnedItems().size()),
                                m_visualizer.enabled());
}

void TaskbarUI::toggleVisualizer() {
    m_visualizer.setEnabled(m_hwnd, !m_visualizer.enabled());
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

int TaskbarUI::screenXFromDip(float dipX) const {
    RECT rc{};
    GetWindowRect(m_hwnd, &rc);
    const float scale = static_cast<float>(GetDpiForWindow(m_hwnd)) / 96.0f;
    return rc.left + static_cast<int>(std::lround(dipX * scale));
}

int TaskbarUI::barEdgeScreen() const {
    RECT rc{};
    GetWindowRect(m_hwnd, &rc);
    return m_settings.edge == TaskbarEdge::Top ? rc.bottom : rc.top;
}

void TaskbarUI::showHoverFlyout(const TaskbarHit& hit) {
    if (m_flyout.isInteractive() || m_dragging) {
        return;
    }
    const int anchorX = screenXFromDip(taskbarSlotCenterX(currentLayout(), hit));
    const int edge = barEdgeScreen();
    switch (hit.zone) {
    case TaskbarZone::Start:
        m_flyout.showTooltip(anchorX, edge, L"Start");
        break;
    case TaskbarZone::TaskView:
        m_flyout.showTooltip(anchorX, edge, L"Task View");
        break;
    case TaskbarZone::Search:
        m_flyout.showTooltip(anchorX, edge, L"Search");
        break;
    case TaskbarZone::Widgets:
        m_flyout.showTooltip(anchorX, edge, L"Widgets");
        break;
    case TaskbarZone::Resume:
        m_flyout.showTooltip(anchorX, edge, L"Resume");
        break;
    case TaskbarZone::Visualizer:
        m_flyout.showTooltip(anchorX, edge, L"Music visualizer");
        break;
    case TaskbarZone::TrayOverflow:
        m_flyout.showTooltip(anchorX, edge, L"Show hidden icons");
        break;
    case TaskbarZone::Tray:
        if (hit.index >= 0 && hit.index < static_cast<int>(m_tray.pinnedItems().size())) {
            m_flyout.showTooltip(anchorX, edge,
                                 m_tray.pinnedItems()[static_cast<size_t>(hit.index)].tooltip);
        }
        break;
    case TaskbarZone::App:
        if (hit.index >= 0 && hit.index < static_cast<int>(m_tasks.items().size())) {
            const auto& item = m_tasks.items()[static_cast<size_t>(hit.index)];
            if (item.windows.size() > 1) {
                m_flyout.showWindowList(anchorX, edge, static_cast<size_t>(hit.index), item);
            } else {
                m_flyout.showTooltip(anchorX, edge, item.displayTitle());
            }
        }
        break;
    case TaskbarZone::Clock:
    case TaskbarZone::None:
        m_flyout.hide();
        break;
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
    const UINT command = TrackPopupMenu(sysMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON, screenX, screenY,
                                        0, m_hwnd, nullptr);
    if (command != 0) {
        PostMessageW(target, WM_SYSCOMMAND, command, 0);
    }
}

void TaskbarUI::showBarContextMenu(int anchorScreenX) {
    m_flyout.showBarContextMenu(anchorScreenX, barEdgeScreen(), m_visualizer.enabled());
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
    const TaskbarLayout layout = currentLayout();
    if (m_pointerDown) {
        const int dx = xPx - m_dragStartPx.x;
        const int dy = yPx - m_dragStartPx.y;
        const int threshold = GetSystemMetrics(SM_CXDRAG);
        if (!m_dragging && (std::abs(dx) > threshold || std::abs(dy) > threshold)) {
            const bool appDrag = m_dragZone == TaskbarZone::App && m_dragFromIndex >= 0 &&
                                 m_dragFromIndex < static_cast<int>(m_tasks.items().size());
            const bool trayDrag = m_dragZone == TaskbarZone::Tray && m_dragFromIndex >= 0;
            if (appDrag || trayDrag) {
                m_dragging = true;
                m_flyout.hide();
            }
        }
        if (m_dragging) {
            m_dragDeltaDip = static_cast<float>(dx) / scale;
            int pinnedCount = 0;
            for (const auto& item : m_tasks.items()) {
                if (item.pinned) {
                    ++pinnedCount;
                }
            }
            m_dragToIndex = taskbarDragTargetIndex(layout, m_dragZone, dipX, pinnedCount);
            InvalidateRect(m_hwnd, nullptr, FALSE);
            return;
        }
    }
    const TaskbarHit hit = hitTestTaskbar(layout, dipX, dipY);
    if (hit != m_hover) {
        m_hover = hit;
        showHoverFlyout(hit);
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
}

void TaskbarUI::onLeftDown(int xPx, int yPx) {
    m_flyout.hide();
    const float scale = static_cast<float>(GetDpiForWindow(m_hwnd)) / 96.0f;
    const TaskbarHit hit = hitTestTaskbar(currentLayout(), static_cast<float>(xPx) / scale,
                                          static_cast<float>(yPx) / scale);
    m_pointerDown = true;
    m_dragging = false;
    m_dragStartPx = POINT{xPx, yPx};
    m_dragZone = hit.zone;
    m_dragFromIndex = hit.index;
    m_dragToIndex = hit.index;
    m_dragDeltaDip = 0.0f;
    m_dragWasPinned = false;
    if (hit.zone == TaskbarZone::App && hit.index >= 0 &&
        hit.index < static_cast<int>(m_tasks.items().size())) {
        m_dragWasPinned = m_tasks.items()[static_cast<size_t>(hit.index)].pinned;
    }
    SetCapture(m_hwnd);
}

void TaskbarUI::onLeftUp(int xPx, int yPx) {
    const bool wasDragging = m_dragging;
    const TaskbarZone dragZone = m_dragZone;
    const int fromIndex = m_dragFromIndex;
    const int toIndex = m_dragToIndex;
    const bool wasPinned = m_dragWasPinned;
    m_pointerDown = false;
    m_dragging = false;
    m_dragZone = TaskbarZone::None;
    m_dragFromIndex = -1;
    m_dragToIndex = -1;
    m_dragDeltaDip = 0.0f;
    if (GetCapture() == m_hwnd) {
        ReleaseCapture();
    }
    if (wasDragging) {
        if (dragZone == TaskbarZone::App && fromIndex >= 0 && toIndex >= 0) {
            if (wasPinned) {
                if (fromIndex != toIndex) {
                    m_tasks.movePinnedItem(static_cast<size_t>(fromIndex),
                                           static_cast<size_t>(toIndex));
                }
            } else if (fromIndex < static_cast<int>(m_tasks.items().size())) {
                // Dragging a running unpinned app pins it at the drop position.
                const TaskItem item = m_tasks.items()[static_cast<size_t>(fromIndex)];
                if (m_tasks.pinItem(item) && !m_tasks.items().empty()) {
                    const size_t last = m_tasks.items().size() - 1;
                    const size_t target = std::min(static_cast<size_t>(toIndex), last);
                    if (last != target) {
                        m_tasks.movePinnedItem(last, target);
                    }
                }
            }
        } else if (dragZone == TaskbarZone::Tray && fromIndex >= 0 && toIndex >= 0 &&
                   fromIndex != toIndex) {
            m_tray.movePinnedTrayItem(static_cast<size_t>(fromIndex), static_cast<size_t>(toIndex));
        }
        InvalidateRect(m_hwnd, nullptr, FALSE);
        return;
    }
    const float scale = static_cast<float>(GetDpiForWindow(m_hwnd)) / 96.0f;
    const TaskbarLayout layout = currentLayout();
    const TaskbarHit hit =
        hitTestTaskbar(layout, static_cast<float>(xPx) / scale, static_cast<float>(yPx) / scale);
    switch (hit.zone) {
    case TaskbarZone::Start:
        m_flyout.hide();
        ShellTaskbar::openStartMenu();
        break;
    case TaskbarZone::TaskView:
        m_flyout.hide();
        ShellTaskbar::openTaskView();
        break;
    case TaskbarZone::Search:
        m_flyout.hide();
        ShellTaskbar::openSearch();
        break;
    case TaskbarZone::Widgets:
        m_flyout.hide();
        ShellTaskbar::openWidgets();
        break;
    case TaskbarZone::Resume:
        m_flyout.hide();
        ShellTaskbar::openResume();
        break;
    case TaskbarZone::App:
        m_flyout.hide();
        if (hit.index >= 0 && hit.index < static_cast<int>(m_tasks.items().size())) {
            m_tasks.activateOrLaunch(m_tasks.items()[static_cast<size_t>(hit.index)]);
        }
        break;
    case TaskbarZone::Tray:
        m_flyout.hide();
        if (hit.index >= 0 && hit.index < static_cast<int>(m_tray.pinnedItems().size())) {
            m_tray.leftClick(m_tray.pinnedItems()[static_cast<size_t>(hit.index)]);
        }
        break;
    case TaskbarZone::TrayOverflow:
        if (m_flyout.mode() == TaskbarFlyout::Mode::TrayOverflow) {
            m_flyout.hide();
        } else {
            m_tray.refresh();
            m_flyout.showTrayOverflow(screenXFromDip(taskbarSlotCenterX(layout, hit)),
                                      barEdgeScreen(), m_tray.overflowItems());
        }
        break;
    case TaskbarZone::Visualizer:
        showBarContextMenu(screenXFromDip(taskbarSlotCenterX(layout, hit)));
        break;
    case TaskbarZone::Clock:
        m_flyout.hide();
        ShellTaskbar::openNotificationCenter();
        break;
    case TaskbarZone::None:
        m_flyout.hide();
        break;
    }
}

void TaskbarUI::onLeftDoubleClick(int xPx, int yPx) {
    const float scale = static_cast<float>(GetDpiForWindow(m_hwnd)) / 96.0f;
    const TaskbarHit hit = hitTestTaskbar(currentLayout(), static_cast<float>(xPx) / scale,
                                          static_cast<float>(yPx) / scale);
    if (hit.zone == TaskbarZone::Tray && hit.index >= 0 &&
        hit.index < static_cast<int>(m_tray.pinnedItems().size())) {
        m_tray.doubleClick(m_tray.pinnedItems()[static_cast<size_t>(hit.index)]);
    }
}

void TaskbarUI::onRightUp(int xPx, int yPx) {
    const float scale = static_cast<float>(GetDpiForWindow(m_hwnd)) / 96.0f;
    const float dipX = static_cast<float>(xPx) / scale;
    const TaskbarLayout layout = currentLayout();
    const TaskbarHit hit = hitTestTaskbar(layout, dipX, static_cast<float>(yPx) / scale);
    if (hit.zone == TaskbarZone::App && hit.index >= 0 &&
        hit.index < static_cast<int>(m_tasks.items().size())) {
        const auto& item = m_tasks.items()[static_cast<size_t>(hit.index)];
        const bool shiftHeld = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        if (shiftHeld && item.running && item.primaryHwnd()) {
            POINT pt{xPx, yPx};
            ClientToScreen(m_hwnd, &pt);
            showNativeSystemMenu(item.primaryHwnd(), pt.x, pt.y);
        } else {
            m_flyout.showAppContextMenu(screenXFromDip(taskbarSlotCenterX(layout, hit)),
                                        barEdgeScreen(), static_cast<size_t>(hit.index), item,
                                        m_tasks.canPin(item));
        }
        return;
    }
    if (hit.zone == TaskbarZone::Tray && hit.index >= 0 &&
        hit.index < static_cast<int>(m_tray.pinnedItems().size())) {
        m_flyout.hide();
        m_tray.rightClick(m_tray.pinnedItems()[static_cast<size_t>(hit.index)]);
        return;
    }
    if (hit.zone == TaskbarZone::None || hit.zone == TaskbarZone::Visualizer) {
        m_flyout.hide();
        showBarContextMenu(screenXFromDip(dipX));
    }
}

void TaskbarUI::onMiddleUp(int xPx, int yPx) {
    const float scale = static_cast<float>(GetDpiForWindow(m_hwnd)) / 96.0f;
    const TaskbarHit hit = hitTestTaskbar(currentLayout(), static_cast<float>(xPx) / scale,
                                          static_cast<float>(yPx) / scale);
    if (hit.zone == TaskbarZone::App && hit.index >= 0 &&
        hit.index < static_cast<int>(m_tasks.items().size())) {
        const auto& item = m_tasks.items()[static_cast<size_t>(hit.index)];
        if (item.running) {
            m_tasks.closeAllWindows(item);
        } else {
            m_tasks.launchNewInstance(item);
        }
    } else if (hit.zone == TaskbarZone::Tray && hit.index >= 0 &&
               hit.index < static_cast<int>(m_tray.pinnedItems().size())) {
        m_tray.setPinnedToBar(m_tray.pinnedItems()[static_cast<size_t>(hit.index)].persistKey,
                              false);
    } else if (hit.zone == TaskbarZone::Visualizer) {
        toggleVisualizer();
    }
}

void TaskbarUI::paint() {
    PAINTSTRUCT ps{};
    BeginPaint(m_hwnd, &ps);
    TaskbarPaintState state{};
    state.layout = currentLayout();
    state.apps = std::span<const TaskItem>(m_tasks.items());
    state.tray = std::span<const TrayItem>(m_tray.pinnedItems());
    state.visualizerLevels = std::span<const float>(m_visualizer.levels());
    state.hover = m_hover;
    state.trayOverflowOpen = (m_flyout.mode() == TaskbarFlyout::Mode::TrayOverflow);
    state.dragging = m_dragging;
    state.dragZone = m_dragZone;
    state.dragFromIndex = m_dragFromIndex;
    state.dragToIndex = m_dragToIndex;
    state.dragWasPinned = m_dragWasPinned;
    state.dragDeltaDip = m_dragDeltaDip;
    state.time = m_clock.time();
    state.date = m_clock.date();
    state.timeFormat = m_timeFormat.Get();
    state.dateFormat = m_dateFormat.Get();
    const HRESULT hr = paintTaskbar(m_renderer, state);
    EndPaint(m_hwnd, &ps);
    if (hr == D2DERR_RECREATE_TARGET) {
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
}