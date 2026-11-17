/**
 * @file TaskTracker.h
 *
 * @version 1.2.0
 * @author BleckWolf25
 * @license MIT
 *
 * @summary Event-driven open window and foreground application tracker
 *
 * @description
 * Enumerates top-level desktop windows, preserves stable slot order across focus switches,
 * extracts high-DPI application icons with executable fallback and listens to WinEvent hooks
 * without polling.
 *
 * @since 17/11/2026
 * @updated 17/11/2026
 */
#pragma once
// ---------- IMPORTS
#include <Windows.h>
#include <d2d1.h>
#include <wrl/client.h>

#include <functional>
#include <string>
#include <vector>

// ---------- DECLARATIONS
struct TaskItem {
    HWND hwnd = nullptr;
    std::wstring title;
    std::wstring shortcutPath;
    Microsoft::WRL::ComPtr<ID2D1Bitmap> icon;
    bool active = false;
    bool running = false;
    bool pinned = false;
};

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
    void middleClickItem(const TaskItem& item) const;
    void toggleWindow(HWND target) const;
    void closeWindow(HWND target) const;

    const std::vector<TaskItem>& items() const noexcept { return m_items; }

private:
    struct PinnedEntry {
        std::wstring name;
        std::wstring shortcutPath;
        std::wstring targetPathLower;
        std::wstring appUserModelIdLower;
        Microsoft::WRL::ComPtr<ID2D1Bitmap> icon;
    };

    static bool isTaskbarWindow(HWND hwnd, HWND selfHwnd);
    static void launchShortcut(const std::wstring& shortcutPath);
    Microsoft::WRL::ComPtr<ID2D1Bitmap> loadIconForWindow(HWND hwnd) const;
    Microsoft::WRL::ComPtr<ID2D1Bitmap> loadIconForShortcut(const std::wstring& shortcutPath) const;

    void startPinWatcher(const std::wstring& pinnedDir);
    void stopPinWatcher();
    static void CALLBACK onPinDirChanged(PVOID context, BOOLEAN timerOrWaitFired);

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

    HANDLE m_pinDirHandle = INVALID_HANDLE_VALUE;
    HANDLE m_pinWaitHandle = nullptr;

    static TaskTracker* s_instance;
};