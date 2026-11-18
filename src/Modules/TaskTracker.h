/**
 * @file TaskTracker.h
 *
 * @version 1.4.0
 * @author BleckWolf25
 * @license MIT
 *
 * @summary Event-driven open window and foreground application tracker
 *
 * @description
 * Enumerates eligible top-level windows, groups them against Explorer's pinned shortcuts and
 * preserves stable slot order across focus switches. Identity comes from WindowIdentity, pin
 * metadata and ordering from ShortcutStore, folder change notification from PinFolderWatcher and
 * icon decoding from caller-supplied loaders. Rebuilds only on shell WinEvents, never on a timer.
 *
 * @since 17/11/2026
 * @updated 18/11/2026
 */
#pragma once
// ---------- IMPORTS
#include "Modules/PinFolderWatcher.h"
#include <Windows.h>
#include <d2d1.h>
#include <wrl/client.h>
#include <functional>
#include <string>
#include <vector>
// ---------- TYPES
struct WindowEntry {
    HWND hwnd = nullptr;
    std::wstring title;
    bool active = false;
};

struct TaskItem {
    std::wstring key;
    std::wstring appName;
    std::wstring shortcutPath;
    std::wstring targetPath;
    std::wstring appUserModelId;
    Microsoft::WRL::ComPtr<ID2D1Bitmap> icon;
    std::vector<WindowEntry> windows;
    size_t activeWindowIndex = 0;
    bool active = false;
    bool running = false;
    bool pinned = false;
    HWND primaryHwnd() const noexcept {
        if (windows.empty()) {
            return nullptr;
        }
        const size_t idx = activeWindowIndex < windows.size() ? activeWindowIndex : 0;
        return windows[idx].hwnd;
    }
    const std::wstring& displayTitle() const noexcept {
        if (!windows.empty()) {
            const size_t idx = activeWindowIndex < windows.size() ? activeWindowIndex : 0;
            if (!windows[idx].title.empty()) {
                return windows[idx].title;
            }
        }
        return appName;
    }
};
// ---------- DECLARATIONS
class TaskTracker {
public:
    static constexpr UINT kMsgPinsChanged = WM_APP + 2;
    using IconLoader = std::function<Microsoft::WRL::ComPtr<ID2D1Bitmap>(HICON)>;
    using HBitmapLoader = std::function<Microsoft::WRL::ComPtr<ID2D1Bitmap>(HBITMAP)>;
    using ChangeCallback = std::function<void()>;
    TaskTracker() = default;
    ~TaskTracker();
    void start(HWND selfHwnd, IconLoader iconLoader, HBitmapLoader hbitmapLoader,
               ChangeCallback onChanged);
    void stop();
    void reloadPinnedApps();
    void refresh();
    void activateOrLaunch(const TaskItem& item) const;
    void launchNewInstance(const TaskItem& item) const;
    void closeAllWindows(const TaskItem& item) const;
    void focusWindow(HWND target) const;
    void closeWindow(HWND target) const;
    bool canPin(const TaskItem& item) const noexcept;
    bool pinItem(const TaskItem& item);
    void unpinItem(const TaskItem& item);
    void pinExternalPath(const std::wstring& filePath);
    void movePinnedItem(size_t fromIndex, size_t toIndex);
    const std::vector<TaskItem>& items() const noexcept { return m_items; }
private:
    struct PinnedEntry {
        std::wstring key;
        std::wstring name;
        std::wstring shortcutPath;
        std::wstring targetPathLower;
        std::wstring appUserModelIdLower;
        Microsoft::WRL::ComPtr<ID2D1Bitmap> icon;
    };
    static void launchPath(const std::wstring& path);
    void savePinOrder() const;
    Microsoft::WRL::ComPtr<ID2D1Bitmap> loadIconForWindow(HWND hwnd) const;
    Microsoft::WRL::ComPtr<ID2D1Bitmap> loadIconForShortcut(const std::wstring& shortcutPath) const;
    static BOOL CALLBACK enumWindowsProc(HWND hwnd, LPARAM lp);
    static void CALLBACK onWinEvent(HWINEVENTHOOK hook, DWORD event, HWND hwnd, LONG idObject,
                                    LONG idChild, DWORD eventThread, DWORD eventTime);
    HWND m_selfHwnd = nullptr;
    IconLoader m_iconLoader;
    HBitmapLoader m_hbitmapLoader;
    ChangeCallback m_onChanged;
    std::vector<PinnedEntry> m_pinned;
    std::vector<TaskItem> m_items;
    std::vector<HWINEVENTHOOK> m_hooks;
    PinFolderWatcher m_watcher;
    static TaskTracker* s_instance;
};