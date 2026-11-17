/**
 * @file TaskTracker.cpp
 *
 * @version 1.1.0
 * @author BleckWolf25
 * @license MIT
 *
 * @summary Implementation of the event-driven window list and activation switcher
 *
 * @description
 * Maintains a stable-ordered list of visible top-level windows and their Direct2D icon bitmaps.
 * Rebuilds only when the shell fires window lifecycle or foreground WinEvents.
 *
 * @since 17/11/2026
 * @updated 17/11/2026
 */
// ---------- IMPORTS
#include "Modules/TaskTracker.h"

#include <dwmapi.h>
#include <shellapi.h>
#include <algorithm>
#include <utility>

// ---------- CONSTANTS
namespace {
constexpr DWORD kHookFlags = WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS;
}

TaskTracker* TaskTracker::s_instance = nullptr;

// ---------- IMPLEMENTATION
TaskTracker::~TaskTracker() {
    stop();
}

void TaskTracker::start(HWND selfHwnd, IconLoader iconLoader, ChangeCallback onChanged) {
    m_selfHwnd = selfHwnd;
    m_iconLoader = std::move(iconLoader);
    m_onChanged = std::move(onChanged);
    s_instance = this;

    const DWORD ranges[][2] = {
        {EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND},
        {EVENT_OBJECT_CREATE, EVENT_OBJECT_HIDE},
        {EVENT_OBJECT_NAMECHANGE, EVENT_OBJECT_NAMECHANGE},
        {EVENT_OBJECT_CLOAKED, EVENT_OBJECT_UNCLOAKED},
    };
    for (const auto& range : ranges) {
        if (HWINEVENTHOOK hook = SetWinEventHook(range[0], range[1], nullptr,
                                                 &TaskTracker::onWinEvent, 0, 0, kHookFlags)) {
            m_hooks.push_back(hook);
        }
    }

    refresh();
}

void TaskTracker::stop() {
    for (HWINEVENTHOOK hook : m_hooks) {
        UnhookWinEvent(hook);
    }
    m_hooks.clear();
    m_items.clear();
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

void TaskTracker::refresh() {
    std::vector<TaskItem> oldItems = std::move(m_items);
    m_items.clear();

    std::vector<TaskItem> discovered;
    struct EnumCtx {
        HWND selfHwnd;
        std::vector<TaskItem>* out;
    } ctx{m_selfHwnd, &discovered};

    EnumWindows(&TaskTracker::enumWindowsProc, reinterpret_cast<LPARAM>(&ctx));

    // Keep existing windows in their original slot order so icons never shuffle on focus
    for (auto& old : oldItems) {
        auto it = std::find_if(discovered.begin(), discovered.end(),
                               [&](const TaskItem& d) { return d.hwnd == old.hwnd; });
        if (it != discovered.end()) {
            old.title = std::move(it->title);
            m_items.push_back(std::move(old));
            discovered.erase(it);
        }
    }
    for (auto& fresh : discovered) {
        m_items.push_back(std::move(fresh));
    }

    const HWND foreground = GetForegroundWindow();
    for (auto& item : m_items) {
        item.active = (item.hwnd == foreground);
        if (!item.icon) {
            item.icon = loadIconForWindow(item.hwnd);
        }
    }

    if (m_onChanged) {
        m_onChanged();
    }
}

void TaskTracker::toggleWindow(HWND target) const {
    if (!IsWindow(target)) {
        return;
    }
    if (GetForegroundWindow() == target && !IsIconic(target)) {
        ShowWindowAsync(target, SW_MINIMIZE);
        return;
    }
    if (IsIconic(target)) {
        ShowWindowAsync(target, SW_RESTORE);
    }
    SetForegroundWindow(target);
}

void TaskTracker::closeWindow(HWND target) const {
    if (IsWindow(target)) {
        PostMessageW(target, WM_CLOSE, 0, 0);
    }
}

bool TaskTracker::isTaskbarWindow(HWND hwnd, HWND selfHwnd) {
    if (!hwnd || hwnd == selfHwnd || !IsWindowVisible(hwnd)) {
        return false;
    }

    const LONG_PTR exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    if ((exStyle & WS_EX_TOOLWINDOW) != 0) {
        return false;
    }
    if (GetWindow(hwnd, GW_OWNER) != nullptr && (exStyle & WS_EX_APPWINDOW) == 0) {
        return false;
    }

    BOOL cloaked = FALSE;
    if (SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked))) && cloaked) {
        return false;
    }

    return GetWindowTextLengthW(hwnd) > 0;
}

Microsoft::WRL::ComPtr<ID2D1Bitmap> TaskTracker::loadIconForWindow(HWND hwnd) const {
    if (!m_iconLoader) {
        return nullptr;
    }

    HICON icon = nullptr;
    SendMessageTimeoutW(hwnd, WM_GETICON, ICON_BIG, 0, SMTO_ABORTIFHUNG, 50,
                        reinterpret_cast<PDWORD_PTR>(&icon));
    if (!icon) {
        icon = reinterpret_cast<HICON>(GetClassLongPtrW(hwnd, GCLP_HICON));
    }
    if (!icon) {
        SendMessageTimeoutW(hwnd, WM_GETICON, ICON_SMALL2, 0, SMTO_ABORTIFHUNG, 50,
                            reinterpret_cast<PDWORD_PTR>(&icon));
    }
    if (!icon) {
        icon = reinterpret_cast<HICON>(GetClassLongPtrW(hwnd, GCLP_HICONSM));
    }
    if (icon) {
        if (auto bmp = m_iconLoader(icon)) {
            return bmp;
        }
    }

    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid != 0) {
        if (HANDLE proc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid)) {
            wchar_t exePath[MAX_PATH]{};
            DWORD size = MAX_PATH;
            if (QueryFullProcessImageNameW(proc, 0, exePath, &size)) {
                HICON extracted = nullptr;
                if (ExtractIconExW(exePath, 0, &extracted, nullptr, 1) > 0 && extracted) {
                    auto bmp = m_iconLoader(extracted);
                    DestroyIcon(extracted);
                    CloseHandle(proc);
                    if (bmp) {
                        return bmp;
                    }
                }
            }
            CloseHandle(proc);
        }
    }

    return m_iconLoader(LoadIconW(nullptr, IDI_APPLICATION));
}

BOOL CALLBACK TaskTracker::enumWindowsProc(HWND hwnd, LPARAM lp) {
    struct EnumCtx {
        HWND selfHwnd;
        std::vector<TaskItem>* out;
    };
    auto* ctx = reinterpret_cast<EnumCtx*>(lp);
    if (!isTaskbarWindow(hwnd, ctx->selfHwnd)) {
        return TRUE;
    }

    wchar_t title[256]{};
    GetWindowTextW(hwnd, title, 256);

    TaskItem item{};
    item.hwnd = hwnd;
    item.title = title;
    ctx->out->push_back(std::move(item));
    return TRUE;
}

void CALLBACK TaskTracker::onWinEvent(HWINEVENTHOOK, DWORD event, HWND hwnd, LONG idObject,
                                      LONG idChild, DWORD, DWORD) {
    if (!s_instance || idObject != OBJID_WINDOW || idChild != CHILDID_SELF || !hwnd) {
        return;
    }
    if (event == EVENT_SYSTEM_FOREGROUND || isTaskbarWindow(hwnd, s_instance->m_selfHwnd) ||
        event == EVENT_OBJECT_DESTROY || event == EVENT_OBJECT_HIDE ||
        event == EVENT_OBJECT_CLOAKED) {
        s_instance->refresh();
    }
}