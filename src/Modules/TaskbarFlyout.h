/**
 * @file TaskbarFlyout.h
 *
 * @version 1.4.0
 * @author BleckWolf25
 * @license MIT
 *
 * @summary Blur-free glass popup overlay for tooltips, window lists, context menus and tray overflow
 *
 * @description
 * Renders a lightweight Direct2D/DirectComposition popup anchored to the dock. Modes cover hover
 * tooltips, multi-window switcher lists with close buttons, icon-bearing app context menus, a
 * System Tray overflow grid and a styled taskbar menu for settings, Task Manager and the
 * visualizer toggle. While an interactive mode is visible a FlyoutDismissHook posts
 * WM_APP_FLYOUT_DISMISS on pointer exit or outside clicks, and is released the instant the popup
 * hides so idle CPU stays at zero.
 *
 * @since 17/11/2026
 * @updated 20/11/2026
 */
#pragma once
// ---------- IMPORTS
#include "Core/Window.h"
#include "Modules/FlyoutDismissHook.h"
#include "Modules/TaskTracker.h"
#include "Modules/TrayTracker.h"
#include "Render/Renderer.h"
#include <functional>
#include <string>
#include <vector>
// ---------- CONSTANTS
inline constexpr UINT WM_APP_FLYOUT_DISMISS = WM_APP + 3;
// ---------- DECLARATIONS
class TaskbarFlyout final : public Window {
public:
    enum class Mode { Hidden, Tooltip, WindowList, AppContextMenu, TrayOverflow, BarMenu };
    struct Callbacks {
        std::function<void(HWND)> onFocusWindow;
        std::function<void(HWND)> onCloseWindow;
        std::function<void(size_t appIndex)> onLaunchNew;
        std::function<void(size_t appIndex)> onTogglePinApp;
        std::function<void(size_t appIndex)> onCloseAllWindows;
        std::function<void(const TrayItem&)> onTrayLeftClick;
        std::function<void(const TrayItem&)> onTrayRightClick;
        std::function<void(const std::wstring& persistKey)> onTrayTogglePinToBar;
        std::function<void()> onOpenSettings;
        std::function<void()> onOpenTaskManager;
        std::function<void()> onToggleVisualizer;
    };
    ~TaskbarFlyout() override;
    bool init(HWND taskbarHwnd, Callbacks callbacks);
    void showTooltip(int anchorCenterXScreen, int barEdgeScreen, const std::wstring& text);
    void showWindowList(int anchorCenterXScreen, int barEdgeScreen, size_t appIndex,
                        const TaskItem& item);
    void showAppContextMenu(int anchorCenterXScreen, int barEdgeScreen, size_t appIndex,
                            const TaskItem& item, bool canPin);
    void showTrayOverflow(int anchorCenterXScreen, int barEdgeScreen,
                          const std::vector<TrayItem>& overflowItems);
    void showBarContextMenu(int anchorCenterXScreen, int barEdgeScreen, bool visualizerEnabled);
    void setPlacedBelowBar(bool below) noexcept { m_belowBar = below; }
    void hide();
    Mode mode() const noexcept { return m_mode; }
    bool isInteractive() const noexcept {
        return m_mode == Mode::WindowList || m_mode == Mode::AppContextMenu ||
               m_mode == Mode::TrayOverflow || m_mode == Mode::BarMenu;
    }
protected:
    LRESULT onMessage(UINT msg, WPARAM wp, LPARAM lp) override;
private:
    struct MenuAction {
        enum class Kind { LaunchNew, TogglePin, CloseAll, Settings, TaskManager, Visualizer };
        Kind kind = Kind::LaunchNew;
        std::wstring label;
    };
    static void drawMenuGlyph(ID2D1DeviceContext& dc, Renderer& renderer, MenuAction::Kind kind,
                              const D2D1_RECT_F& rect, const D2D1_COLOR_F& tint);
    void positionAndShow(int anchorCenterXScreen, int barEdgeScreen, float widthDip,
                         float heightDip, bool activate);
    void showMenuRows(int anchorCenterXScreen, int barEdgeScreen, float widthDip);
    void onMouseMove(int xPx, int yPx);
    void onLeftClick(int xPx, int yPx);
    void onRightClick(int xPx, int yPx);
    void paint();
    HWND m_taskbarHwnd = nullptr;
    Callbacks m_callbacks;
    Renderer m_renderer;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> m_titleFormat;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> m_itemFormat;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> m_badgeFormat;
    Mode m_mode = Mode::Hidden;
    std::wstring m_tooltipText;
    size_t m_appIndex = 0;
    std::wstring m_headerTitle;
    std::vector<WindowEntry> m_windows;
    std::vector<MenuAction> m_menuActions;
    std::vector<TrayItem> m_trayItems;
    int m_hoverRow = -1;
    bool m_hoverCloseBtn = false;
    bool m_hoverPinBtn = false;
    bool m_trackingMouse = false;
    bool m_belowBar = false;
    FlyoutDismissHook m_dismiss;
};