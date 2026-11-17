/**
 * @file TaskbarFlyout.h
 *
 * @version 1.0.0
 * @author BleckWolf25
 * @license MIT
 *
 * @summary Blur-free glass popup overlay for tooltips, window lists, context menus and tray overflow
 *
 * @description
 * Renders a lightweight Direct2D/DirectComposition popup window anchored above the taskbar.
 * Supports four modes: single-line hover tooltips, multi-window switcher lists with close
 * buttons, app right-click context menus (Launch, Pin/Unpin, Close), and a System Tray
 * overflow grid where tray apps can be clicked, right-clicked, or pinned onto the taskbar.
 *
 * @since 17/11/2026
 * @updated 17/11/2026
 */
#pragma once
// ---------- IMPORTS
#include "Core/Window.h"
#include "Modules/TaskTracker.h"
#include "Modules/TrayTracker.h"
#include "Render/Renderer.h"

#include <functional>
#include <string>
#include <vector>

// ---------- DECLARATIONS
class TaskbarFlyout final : public Window {
public:
    enum class Mode {
        Hidden,
        Tooltip,
        WindowList,
        AppContextMenu,
        TrayOverflow,
    };

    struct Callbacks {
        std::function<void(HWND)> onFocusWindow;
        std::function<void(HWND)> onCloseWindow;
        std::function<void(size_t appIndex)> onLaunchNew;
        std::function<void(size_t appIndex)> onTogglePinApp;
        std::function<void(size_t appIndex)> onCloseAllWindows;
        std::function<void(const TrayItem&)> onTrayLeftClick;
        std::function<void(const TrayItem&)> onTrayRightClick;
        std::function<void(const std::wstring& persistKey)> onTrayTogglePinToBar;
    };

    ~TaskbarFlyout() override;
    bool init(HWND taskbarHwnd, Callbacks callbacks);

    void showTooltip(int anchorCenterXScreen, int taskbarTopScreen, const std::wstring& text);
    void showWindowList(int anchorCenterXScreen, int taskbarTopScreen, size_t appIndex,
                        const TaskItem& item);
    void showAppContextMenu(int anchorCenterXScreen, int taskbarTopScreen, size_t appIndex,
                            const TaskItem& item, bool canPin);
    void showTrayOverflow(int anchorCenterXScreen, int taskbarTopScreen,
                          const std::vector<TrayItem>& overflowItems);
    void hide();

    Mode mode() const noexcept { return m_mode; }
    bool isInteractive() const noexcept {
        return m_mode == Mode::WindowList || m_mode == Mode::AppContextMenu ||
               m_mode == Mode::TrayOverflow;
    }

protected:
    LRESULT onMessage(UINT msg, WPARAM wp, LPARAM lp) override;

private:
    struct MenuAction {
        enum class Kind { LaunchNew, TogglePin, CloseAll };
        Kind kind = Kind::LaunchNew;
        std::wstring label;
    };

    void positionAndShow(int anchorCenterXScreen, int taskbarTopScreen, float widthDip,
                         float heightDip, bool activate);
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
};