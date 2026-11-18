/**
 * @file TrayTracker.h
 *
 * @version 1.2.0
 * @author BleckWolf25
 * @license MIT
 *
 * @summary System tray icon promotion, ordering and native click dispatch
 *
 * @description
 * Consumes RawTrayIcon records from TrayToolbarReader, splits them into taskbar-promoted and
 * overflow sets, persists promotion and drag order in the NovaUI configuration folder, and
 * forwards left, double and right clicks to the owning tray process with foreground permission.
 * Explorer WinEvents arm one coalescing timer, so no cross-process reads happen while idle.
 *
 * @since 17/11/2026
 * @updated 18/11/2026
 */
#pragma once
// ---------- IMPORTS
#include <Windows.h>
#include <d2d1.h>
#include <wrl/client.h>
#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
// ---------- TYPES
struct TrayItem {
    std::wstring persistKey;
    HWND ownerHwnd = nullptr;
    UINT uID = 0;
    UINT uCallbackMessage = 0;
    std::wstring tooltip;
    Microsoft::WRL::ComPtr<ID2D1Bitmap> icon;
    bool pinnedToBar = false;
};
// ---------- DECLARATIONS
class TrayTracker {
public:
    using IconLoader = std::function<Microsoft::WRL::ComPtr<ID2D1Bitmap>(HICON)>;
    using ChangeCallback = std::function<void()>;
    TrayTracker() = default;
    ~TrayTracker();
    void start(HWND selfHwnd, IconLoader iconLoader, ChangeCallback onChanged);
    void stop();
    void refresh();
    bool onTimer(WPARAM timerId);
    void leftClick(const TrayItem& item) const;
    void doubleClick(const TrayItem& item) const;
    void rightClick(const TrayItem& item) const;
    void setPinnedToBar(const std::wstring& persistKey, bool pinned);
    void togglePinnedToBar(const std::wstring& persistKey);
    void movePinnedTrayItem(size_t fromIndex, size_t toIndex);
    const std::vector<TrayItem>& pinnedItems() const noexcept { return m_pinnedItems; }
    const std::vector<TrayItem>& overflowItems() const noexcept { return m_overflowItems; }
private:
    void armRefresh();
    void loadConfig();
    void saveConfig() const;
    Microsoft::WRL::ComPtr<ID2D1Bitmap> loadIconForTray(HICON rawIcon, HWND ownerHwnd) const;
    static void CALLBACK onExplorerWinEvent(HWINEVENTHOOK hook, DWORD event, HWND hwnd,
                                            LONG idObject, LONG idChild, DWORD eventThread,
                                            DWORD eventTime);
    HWND m_selfHwnd = nullptr;
    IconLoader m_iconLoader;
    ChangeCallback m_onChanged;
    std::vector<TrayItem> m_pinnedItems;
    std::vector<TrayItem> m_overflowItems;
    std::vector<std::wstring> m_pinnedOrder;
    std::unordered_set<std::wstring> m_pinnedSet;
    std::unordered_map<std::wstring, Microsoft::WRL::ComPtr<ID2D1Bitmap>> m_iconCache;
    std::wstring m_lastFingerprint;
    HWINEVENTHOOK m_explorerHook = nullptr;
    bool m_refreshPending = false;
    static TrayTracker* s_instance;
};