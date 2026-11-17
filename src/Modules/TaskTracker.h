/**
 * @file TaskTracker.h
 *
 * @version 1.1.1
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
    Microsoft::WRL::ComPtr<ID2D1Bitmap> icon;
    bool active = false;
};

class TaskTracker {
public:
    using IconLoader = std::function<Microsoft::WRL::ComPtr<ID2D1Bitmap>(HICON)>;
    using ChangeCallback = std::function<void()>;

    TaskTracker() = default;
    ~TaskTracker();

    void start(HWND selfHwnd, IconLoader iconLoader, ChangeCallback onChanged);
    void stop();
    void refresh();
    void toggleWindow(HWND target) const;
    void closeWindow(HWND target) const;

    const std::vector<TaskItem>& items() const noexcept { return m_items; }

private:
    static bool isTaskbarWindow(HWND hwnd, HWND selfHwnd);
    Microsoft::WRL::ComPtr<ID2D1Bitmap> loadIconForWindow(HWND hwnd) const;
    static BOOL CALLBACK enumWindowsProc(HWND hwnd, LPARAM lp);
    static void CALLBACK onWinEvent(HWINEVENTHOOK hook, DWORD event, HWND hwnd, LONG idObject,
                                    LONG idChild, DWORD eventThread, DWORD eventTime);

    HWND m_selfHwnd = nullptr;
    IconLoader m_iconLoader;
    ChangeCallback m_onChanged;
    std::vector<TaskItem> m_items;
    std::vector<HWINEVENTHOOK> m_hooks;

    static TaskTracker* s_instance;
};