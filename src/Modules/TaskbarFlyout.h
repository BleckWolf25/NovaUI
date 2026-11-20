/**
 * @file TaskbarFlyout.h
 *
 * @version 1.5.0
 * @author BleckWolf25
 * @license MIT
 *
 * @summary Blur-free glass popup overlay for tooltips, window lists, context menus and tray overflow
 *
 * @description
 * Renders a lightweight Direct2D/DirectComposition popup anchored to a point on the dock's inner
 * edge and grown away from the bar on the side selected by setPlacement, so the same popup serves
 * a dock on any screen edge. Modes cover hover tooltips, multi-window switcher lists with close
 * buttons, icon-bearing app context menus, a System Tray overflow grid and a styled taskbar menu.
 * While an interactive mode is visible a FlyoutDismissHook posts WM_APP_FLYOUT_DISMISS on pointer
 * exit or outside clicks and is released the instant the popup hides.
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
// ---------- TYPES
enum class FlyoutSide { Above, Below, LeftOf, RightOf };
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
    void setPlacement(FlyoutSide side) noexcept { m_side = side; }
    void showTooltip(int anchorXScreen, int anchorYScreen, const std::wstring& text);
    void showWindowList(int anchorXScreen, int anchorYScreen, size_t appIndex,
                        const TaskItem& item);
    void showAppContextMenu(int anchorXScreen, int anchorYScreen, size_t appIndex,
                            const TaskItem& item, bool canPin);
    void showTrayOverflow(int anchorXScreen, int anchorYScreen,
                          const std::vector<TrayItem>& overflowItems);
    void showBarContextMenu(int anchorXScreen, int anchorYScreen, bool visualizerEnabled);
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
    void positionAndShow(int anchorXScreen, int anchorYScreen, float widthDip, float heightDip,
                         bool activate);
    void showMenuRows(int anchorXScreen, int anchorYScreen, float widthDip);
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
    FlyoutSide m_side = FlyoutSide::Above;
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
    FlyoutDismissHook m_dismiss;
};