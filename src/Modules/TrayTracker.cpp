/**
 * @file TrayTracker.cpp
 *
 * @version 1.0.0
 * @author BleckWolf25
 * @license MIT
 *
 * @summary Implementation of Explorer tray icon discovery, taskbar promotion and click routing
 *
 * @description
 * Inspects Explorer's hidden tray toolbars using cross-process VirtualAllocEx reads, routes
 * left, double and right clicks with foreground permission grants to the owning tray process,
 * and persists which tray icons are promoted onto NovaUI's main taskbar.
 *
 * @since 17/11/2026
 * @updated 17/11/2026
 */
// ---------- IMPORTS
#include "Modules/TrayTracker.h"

#include <CommCtrl.h>
#include <shellapi.h>
#include <shlobj.h>

#include <algorithm>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <memory>

// ---------- CONSTANTS
namespace {
constexpr UINT_PTR kTraySyncTimerId = 2;
constexpr UINT kTraySyncIntervalMs = 1500;

struct TrayButtonData {
    HWND hwnd;
    UINT uID;
    UINT uCallbackMessage;
    DWORD reserved[2];
    HICON hIcon;
};

std::wstring toLower(std::wstring text) {
    for (wchar_t& ch : text) {
        ch = static_cast<wchar_t>(std::towlower(ch));
    }
    return text;
}

std::string wideToUtf8(const std::wstring& wide) {
    if (wide.empty()) {
        return {};
    }
    const int bytes = WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()),
                                          nullptr, 0, nullptr, nullptr);
    if (bytes <= 0) {
        return {};
    }
    std::string utf8(static_cast<size_t>(bytes), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), utf8.data(), bytes,
                        nullptr, nullptr);
    return utf8;
}

std::wstring utf8ToWide(const std::string& utf8) {
    if (utf8.empty()) {
        return {};
    }
    const int chars =
        MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
    if (chars <= 0) {
        return {};
    }
    std::wstring wide(static_cast<size_t>(chars), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), wide.data(), chars);
    return wide;
}

std::wstring getConfigFilePath() {
    PWSTR roamingRaw = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &roamingRaw)) ||
        !roamingRaw) {
        return {};
    }
    std::filesystem::path dir(roamingRaw);
    CoTaskMemFree(roamingRaw);
    dir /= L"NovaUI";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    return (dir / L"tray_pins.txt").wstring();
}

HWND findChildByClass(HWND parent, const wchar_t* className) {
    if (!parent) {
        return nullptr;
    }
    return FindWindowExW(parent, nullptr, className, nullptr);
}

std::vector<HWND> findTrayToolbars() {
    std::vector<HWND> toolbars;

    if (HWND shellTray = FindWindowW(L"Shell_TrayWnd", nullptr)) {
        HWND trayNotify = findChildByClass(shellTray, L"TrayNotifyWnd");
        HWND sysPager = findChildByClass(trayNotify, L"SysPager");
        if (HWND tb = findChildByClass(sysPager, L"ToolbarWindow32")) {
            toolbars.push_back(tb);
        } else if (HWND directTb = findChildByClass(trayNotify, L"ToolbarWindow32")) {
            toolbars.push_back(directTb);
        }
    }

    if (HWND overflow = FindWindowW(L"NotifyIconOverflowWindow", nullptr)) {
        if (HWND tb = findChildByClass(overflow, L"ToolbarWindow32")) {
            toolbars.push_back(tb);
        }
    }

    return toolbars;
}

std::wstring buildPersistKey(HWND ownerHwnd, UINT uID, const std::wstring& tooltip) {
    DWORD pid = 0;
    GetWindowThreadProcessId(ownerHwnd, &pid);
    if (pid != 0) {
        if (HANDLE proc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid)) {
            wchar_t exePath[MAX_PATH]{};
            DWORD size = MAX_PATH;
            if (QueryFullProcessImageNameW(proc, 0, exePath, &size)) {
                CloseHandle(proc);
                return toLower(exePath) + L"#" + std::to_wstring(uID);
            }
            CloseHandle(proc);
        }
    }
    return L"tip:" + toLower(tooltip) + L"#" + std::to_wstring(uID);
}
}

TrayTracker* TrayTracker::s_instance = nullptr;

// ---------- IMPLEMENTATION
TrayTracker::~TrayTracker() {
    stop();
}

void TrayTracker::start(HWND selfHwnd, IconLoader iconLoader, ChangeCallback onChanged) {
    m_selfHwnd = selfHwnd;
    m_iconLoader = std::move(iconLoader);
    m_onChanged = std::move(onChanged);
    s_instance = this;

    loadConfig();

    if (HWND shellTray = FindWindowW(L"Shell_TrayWnd", nullptr)) {
        DWORD explorerPid = 0;
        GetWindowThreadProcessId(shellTray, &explorerPid);
        if (explorerPid != 0) {
            m_explorerHook = SetWinEventHook(
                EVENT_OBJECT_CREATE, EVENT_OBJECT_NAMECHANGE, nullptr,
                &TrayTracker::onExplorerWinEvent, explorerPid, 0,
                WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
        }
    }

    if (m_selfHwnd) {
        SetTimer(m_selfHwnd, kTraySyncTimerId, kTraySyncIntervalMs, nullptr);
    }

    refresh();
}

void TrayTracker::stop() {
    if (m_selfHwnd) {
        KillTimer(m_selfHwnd, kTraySyncTimerId);
    }
    if (m_explorerHook) {
        UnhookWinEvent(m_explorerHook);
        m_explorerHook = nullptr;
    }
    m_pinnedItems.clear();
    m_overflowItems.clear();
    m_iconCache.clear();
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

bool TrayTracker::onTimer(WPARAM timerId) {
    if (timerId != kTraySyncTimerId) {
        return false;
    }
    refresh();
    return true;
}

void TrayTracker::refresh() {
    struct RawTrayIcon {
        HWND ownerHwnd = nullptr;
        UINT uID = 0;
        UINT uCallbackMessage = 0;
        HICON hIcon = nullptr;
        std::wstring tooltip;
        std::wstring persistKey;
    };

    std::vector<RawTrayIcon> discovered;
    std::unordered_set<std::wstring> seenKeys;

    for (HWND toolbar : findTrayToolbars()) {
        DWORD explorerPid = 0;
        GetWindowThreadProcessId(toolbar, &explorerPid);
        if (explorerPid == 0) {
            continue;
        }

        HANDLE proc = OpenProcess(PROCESS_VM_OPERATION | PROCESS_VM_READ | PROCESS_VM_WRITE,
                                  FALSE, explorerPid);
        if (!proc) {
            continue;
        }

        constexpr SIZE_T kRemoteBytes = sizeof(TBBUTTON) + sizeof(TrayButtonData) + 512 * sizeof(wchar_t);
        void* remoteMem =
            VirtualAllocEx(proc, nullptr, kRemoteBytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (!remoteMem) {
            CloseHandle(proc);
            continue;
        }

        auto* remoteBtn = static_cast<BYTE*>(remoteMem);
        auto* remoteText = remoteBtn + sizeof(TBBUTTON) + sizeof(TrayButtonData);

        LRESULT countRes = 0;
        if (SendMessageTimeoutW(toolbar, TB_BUTTONCOUNT, 0, 0, SMTO_ABORTIFHUNG, 50,
                                reinterpret_cast<PDWORD_PTR>(&countRes)) != 0) {
            const int count = static_cast<int>(countRes);
            for (int i = 0; i < count; ++i) {
                LRESULT ok = 0;
                if (SendMessageTimeoutW(toolbar, TB_GETBUTTON, static_cast<WPARAM>(i),
                                        reinterpret_cast<LPARAM>(remoteBtn), SMTO_ABORTIFHUNG, 50,
                                        reinterpret_cast<PDWORD_PTR>(&ok)) == 0 ||
                    ok == 0) {
                    continue;
                }

                TBBUTTON btn{};
                SIZE_T bytesRead = 0;
                if (!ReadProcessMemory(proc, remoteBtn, &btn, sizeof(btn), &bytesRead) ||
                    bytesRead != sizeof(btn) || btn.dwData == 0) {
                    continue;
                }

                if ((btn.fsState & TBSTATE_HIDDEN) != 0) {
                    continue;
                }

                TrayButtonData trayData{};
                if (!ReadProcessMemory(proc, reinterpret_cast<LPCVOID>(btn.dwData), &trayData,
                                       sizeof(trayData), &bytesRead) ||
                    bytesRead != sizeof(trayData)) {
                    continue;
                }

                if (!trayData.hwnd || !IsWindow(trayData.hwnd)) {
                    continue;
                }

                wchar_t tipBuf[256]{};
                LRESULT textLen = 0;
                if (SendMessageTimeoutW(toolbar, TB_GETBUTTONTEXTW,
                                        static_cast<WPARAM>(btn.idCommand),
                                        reinterpret_cast<LPARAM>(remoteText), SMTO_ABORTIFHUNG, 50,
                                        reinterpret_cast<PDWORD_PTR>(&textLen)) != 0 &&
                    textLen > 0) {
                    const size_t charsToRead =
                        std::min<size_t>(static_cast<size_t>(textLen), 255);
                    ReadProcessMemory(proc, remoteText, tipBuf, charsToRead * sizeof(wchar_t),
                                      &bytesRead);
                }

                RawTrayIcon raw{};
                raw.ownerHwnd = trayData.hwnd;
                raw.uID = trayData.uID;
                raw.uCallbackMessage = trayData.uCallbackMessage;
                raw.hIcon = trayData.hIcon;
                raw.tooltip = tipBuf[0] != L'\0' ? tipBuf : L"System Tray App";
                raw.persistKey = buildPersistKey(raw.ownerHwnd, raw.uID, raw.tooltip);

                if (seenKeys.insert(raw.persistKey).second) {
                    discovered.push_back(std::move(raw));
                }
            }
        }

        VirtualFreeEx(proc, remoteMem, 0, MEM_RELEASE);
        CloseHandle(proc);
    }

    // Build fingerprint to skip redundant UI invalidations when nothing changed
    std::wstring fingerprint;
    for (const auto& d : discovered) {
        fingerprint += d.persistKey;
        fingerprint += L"|";
        fingerprint += d.tooltip;
        fingerprint += L"|";
        fingerprint += std::to_wstring(reinterpret_cast<uintptr_t>(d.hIcon));
        fingerprint += L";";
    }
    for (const auto& k : m_pinnedOrder) {
        fingerprint += L"P:" + k + L";";
    }

    if (fingerprint == m_lastFingerprint) {
        return;
    }
    m_lastFingerprint = std::move(fingerprint);

    std::vector<TrayItem> nextPinned;
    std::vector<TrayItem> nextOverflow;

    for (auto& d : discovered) {
        TrayItem item{};
        item.persistKey = d.persistKey;
        item.ownerHwnd = d.ownerHwnd;
        item.uID = d.uID;
        item.uCallbackMessage = d.uCallbackMessage;
        item.tooltip = std::move(d.tooltip);
        item.pinnedToBar = m_pinnedSet.contains(item.persistKey);

        auto cacheIt = m_iconCache.find(item.persistKey);
        if (cacheIt != m_iconCache.end() && cacheIt->second) {
            item.icon = cacheIt->second;
        } else {
            item.icon = loadIconForTray(d.hIcon, d.ownerHwnd);
            if (item.icon) {
                m_iconCache[item.persistKey] = item.icon;
            }
        }

        if (item.pinnedToBar) {
            nextPinned.push_back(std::move(item));
        } else {
            nextOverflow.push_back(std::move(item));
        }
    }

    // Sort taskbar-pinned tray icons according to user's saved drag order
    std::unordered_map<std::wstring, size_t> pinRank;
    for (size_t i = 0; i < m_pinnedOrder.size(); ++i) {
        pinRank.try_emplace(m_pinnedOrder[i], i);
    }
    std::stable_sort(nextPinned.begin(), nextPinned.end(),
                     [&](const TrayItem& a, const TrayItem& b) {
                         const auto itA = pinRank.find(a.persistKey);
                         const auto itB = pinRank.find(b.persistKey);
                         const size_t rA = (itA != pinRank.end()) ? itA->second : SIZE_MAX;
                         const size_t rB = (itB != pinRank.end()) ? itB->second : SIZE_MAX;
                         return rA < rB;
                     });

    m_pinnedItems = std::move(nextPinned);
    m_overflowItems = std::move(nextOverflow);

    if (m_onChanged) {
        m_onChanged();
    }
}

void TrayTracker::leftClick(const TrayItem& item) const {
    if (!IsWindow(item.ownerHwnd)) {
        return;
    }
    DWORD pid = 0;
    GetWindowThreadProcessId(item.ownerHwnd, &pid);
    if (pid != 0) {
        AllowSetForegroundWindow(pid);
    }
    PostMessageW(item.ownerHwnd, item.uCallbackMessage, static_cast<WPARAM>(item.uID),
                 WM_LBUTTONDOWN);
    PostMessageW(item.ownerHwnd, item.uCallbackMessage, static_cast<WPARAM>(item.uID),
                 WM_LBUTTONUP);
    PostMessageW(item.ownerHwnd, item.uCallbackMessage, static_cast<WPARAM>(item.uID), NIN_SELECT);
}

void TrayTracker::doubleClick(const TrayItem& item) const {
    if (!IsWindow(item.ownerHwnd)) {
        return;
    }
    DWORD pid = 0;
    GetWindowThreadProcessId(item.ownerHwnd, &pid);
    if (pid != 0) {
        AllowSetForegroundWindow(pid);
    }
    PostMessageW(item.ownerHwnd, item.uCallbackMessage, static_cast<WPARAM>(item.uID),
                 WM_LBUTTONDBLCLK);
    PostMessageW(item.ownerHwnd, item.uCallbackMessage, static_cast<WPARAM>(item.uID),
                 WM_LBUTTONUP);
}

void TrayTracker::rightClick(const TrayItem& item) const {
    if (!IsWindow(item.ownerHwnd)) {
        return;
    }
    DWORD pid = 0;
    GetWindowThreadProcessId(item.ownerHwnd, &pid);
    if (pid != 0) {
        AllowSetForegroundWindow(pid);
    }
    SetForegroundWindow(item.ownerHwnd);
    PostMessageW(item.ownerHwnd, item.uCallbackMessage, static_cast<WPARAM>(item.uID),
                 WM_RBUTTONDOWN);
    PostMessageW(item.ownerHwnd, item.uCallbackMessage, static_cast<WPARAM>(item.uID),
                 WM_RBUTTONUP);
    PostMessageW(item.ownerHwnd, item.uCallbackMessage, static_cast<WPARAM>(item.uID),
                 WM_CONTEXTMENU);
}

void TrayTracker::setPinnedToBar(const std::wstring& persistKey, bool pinned) {
    if (persistKey.empty()) {
        return;
    }
    if (pinned) {
        if (m_pinnedSet.insert(persistKey).second) {
            m_pinnedOrder.push_back(persistKey);
        }
    } else {
        m_pinnedSet.erase(persistKey);
        m_pinnedOrder.erase(std::remove(m_pinnedOrder.begin(), m_pinnedOrder.end(), persistKey),
                            m_pinnedOrder.end());
    }
    saveConfig();
    m_lastFingerprint.clear();
    refresh();
}

void TrayTracker::togglePinnedToBar(const std::wstring& persistKey) {
    setPinnedToBar(persistKey, !m_pinnedSet.contains(persistKey));
}

void TrayTracker::movePinnedTrayItem(size_t fromIndex, size_t toIndex) {
    if (fromIndex >= m_pinnedItems.size() || toIndex >= m_pinnedItems.size() ||
        fromIndex == toIndex) {
        return;
    }

    TrayItem moved = std::move(m_pinnedItems[fromIndex]);
    m_pinnedItems.erase(m_pinnedItems.begin() + static_cast<ptrdiff_t>(fromIndex));
    m_pinnedItems.insert(m_pinnedItems.begin() + static_cast<ptrdiff_t>(toIndex),
                         std::move(moved));

    m_pinnedOrder.clear();
    for (const auto& item : m_pinnedItems) {
        m_pinnedOrder.push_back(item.persistKey);
    }
    saveConfig();
    if (m_onChanged) {
        m_onChanged();
    }
}

void TrayTracker::loadConfig() {
    m_pinnedOrder.clear();
    m_pinnedSet.clear();

    const std::wstring path = getConfigFilePath();
    if (path.empty()) {
        return;
    }
    std::ifstream in(path, std::ios::binary);
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (!line.empty()) {
            std::wstring key = utf8ToWide(line);
            if (m_pinnedSet.insert(key).second) {
                m_pinnedOrder.push_back(std::move(key));
            }
        }
    }
}

void TrayTracker::saveConfig() const {
    const std::wstring path = getConfigFilePath();
    if (path.empty()) {
        return;
    }
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        return;
    }
    for (const auto& key : m_pinnedOrder) {
        out << wideToUtf8(key) << "\n";
    }
}

Microsoft::WRL::ComPtr<ID2D1Bitmap> TrayTracker::loadIconForTray(HICON rawIcon,
                                                                 HWND ownerHwnd) const {
    if (!m_iconLoader) {
        return nullptr;
    }

    if (rawIcon) {
        if (HICON copied = CopyIcon(rawIcon)) {
            auto bmp = m_iconLoader(copied);
            DestroyIcon(copied);
            if (bmp) {
                return bmp;
            }
        }
        if (auto bmp = m_iconLoader(rawIcon)) {
            return bmp;
        }
    }

    if (IsWindow(ownerHwnd)) {
        HICON wndIcon = nullptr;
        SendMessageTimeoutW(ownerHwnd, WM_GETICON, ICON_SMALL2, 0, SMTO_ABORTIFHUNG, 30,
                            reinterpret_cast<PDWORD_PTR>(&wndIcon));
        if (!wndIcon) {
            wndIcon = reinterpret_cast<HICON>(GetClassLongPtrW(ownerHwnd, GCLP_HICONSM));
        }
        if (wndIcon) {
            if (auto bmp = m_iconLoader(wndIcon)) {
                return bmp;
            }
        }

        DWORD pid = 0;
        GetWindowThreadProcessId(ownerHwnd, &pid);
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
    }

    return m_iconLoader(LoadIconW(nullptr, IDI_APPLICATION));
}

void CALLBACK TrayTracker::onExplorerWinEvent(HWINEVENTHOOK, DWORD, HWND, LONG, LONG, DWORD,
                                              DWORD) {
    if (s_instance) {
        s_instance->refresh();
    }
}