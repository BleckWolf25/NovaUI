/**
 * @file TrayTracker.cpp
 *
 * @version 1.2.0
 * @author BleckWolf25
 * @license MIT
 *
 * @summary Implementation of tray icon promotion, ordering and click routing
 *
 * @description
 * Fingerprints the reader's output so unchanged enumerations skip UI invalidation entirely,
 * caches decoded icon bitmaps per persistence key, and sorts promoted icons by the saved drag
 * order. Clicks grant AllowSetForegroundWindow to the owning process and post its registered
 * callback message with the shell's notification identifiers. Icon fallbacks copy the raw tray
 * HICON before decoding because the handle belongs to another process.
 *
 * @since 17/11/2026
 * @updated 18/11/2026
 */
// ---------- IMPORTS
#include "Modules/TrayTracker.h"
#include "Modules/TrayAutomationReader.h"
#include "Modules/TrayToolbarReader.h"
#include "Core/ShellPaths.h"
#include "Core/WinResources.h"
#include "Core/WinString.h"
#include <shellapi.h>
#include <algorithm>
#include <fstream>
#include <unordered_map>
// ---------- CONSTANTS
namespace {
constexpr UINT_PTR kTrayCoalesceTimerId = 2;
constexpr UINT kTrayCoalesceMs = 250;
constexpr UINT kIconTimeoutMs = 30;
constexpr wchar_t kTrayConfigFile[] = L"tray_pins.txt";
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
    refresh();
}

void TrayTracker::stop() {
    if (m_selfHwnd) {
        KillTimer(m_selfHwnd, kTrayCoalesceTimerId);
    }
    m_refreshPending = false;
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

void TrayTracker::armRefresh() {
    if (m_refreshPending || !m_selfHwnd) {
        return;
    }
    m_refreshPending = true;
    SetTimer(m_selfHwnd, kTrayCoalesceTimerId, kTrayCoalesceMs, nullptr);
}

bool TrayTracker::onTimer(WPARAM timerId) {
    if (timerId != kTrayCoalesceTimerId) {
        return false;
    }
    KillTimer(m_selfHwnd, kTrayCoalesceTimerId);
    m_refreshPending = false;
    refresh();
    return true;
}

void TrayTracker::refresh() {
    std::vector<RawTrayIcon> discovered = readTrayIcons();
    std::wstring fingerprint;
    for (const auto& icon : discovered) {
        fingerprint += icon.persistKey;
        fingerprint += L"|";
        fingerprint += icon.tooltip;
        fingerprint += L"|";
        fingerprint += std::to_wstring(reinterpret_cast<uintptr_t>(icon.hIcon));
        fingerprint += L";";
    }
    for (const auto& key : m_pinnedOrder) {
        fingerprint += L"P:" + key + L";";
    }
    if (fingerprint == m_lastFingerprint) {
        return;
    }
    m_lastFingerprint = std::move(fingerprint);
    std::vector<TrayItem> nextPinned;
    std::vector<TrayItem> nextOverflow;
    for (auto& icon : discovered) {
        TrayItem item{};
        item.persistKey = icon.persistKey;
        item.ownerHwnd = icon.ownerHwnd;
        item.uID = icon.uID;
        item.uCallbackMessage = icon.uCallbackMessage;
        item.tooltip = std::move(icon.tooltip);
        item.pinnedToBar = m_pinnedSet.contains(item.persistKey);
        item.automation = (icon.source == TrayIconSource::Automation);
        const auto cached = m_iconCache.find(item.persistKey);
        if (cached != m_iconCache.end() && cached->second) {
            item.icon = cached->second;
        } else {
            item.icon = loadIconForTray(icon.hIcon, icon.ownerHwnd);
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
    std::unordered_map<std::wstring, size_t> pinRank;
    for (size_t i = 0; i < m_pinnedOrder.size(); ++i) {
        pinRank.try_emplace(m_pinnedOrder[i], i);
    }
    std::stable_sort(nextPinned.begin(), nextPinned.end(),
                     [&](const TrayItem& a, const TrayItem& b) {
                         const auto itA = pinRank.find(a.persistKey);
                         const auto itB = pinRank.find(b.persistKey);
                         const size_t rankA = (itA != pinRank.end()) ? itA->second : SIZE_MAX;
                         const size_t rankB = (itB != pinRank.end()) ? itB->second : SIZE_MAX;
                         return rankA < rankB;
                     });
    m_pinnedItems = std::move(nextPinned);
    m_overflowItems = std::move(nextOverflow);
    if (m_onChanged) {
        m_onChanged();
    }
}

void TrayTracker::leftClick(const TrayItem& item) const {
    if (item.automation) {
        invokeAutomationTrayIcon(item.persistKey);
        return;
    }
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
    if (item.automation) {
        invokeAutomationTrayIcon(item.persistKey);
        return;
    }
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
    if (item.automation) {
        invokeAutomationTrayIcon(item.persistKey);
        return;
    }
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
    const std::wstring path = novaConfigFilePath(kTrayConfigFile);
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
    const std::wstring path = novaConfigFilePath(kTrayConfigFile);
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
            const UniqueIcon owned(copied);
            if (auto bitmap = m_iconLoader(owned.get())) {
                return bitmap;
            }
        }
        if (auto bitmap = m_iconLoader(rawIcon)) {
            return bitmap;
        }
    }
    if (IsWindow(ownerHwnd)) {
        HICON windowIcon = nullptr;
        SendMessageTimeoutW(ownerHwnd, WM_GETICON, ICON_SMALL2, 0, SMTO_ABORTIFHUNG,
                            kIconTimeoutMs, reinterpret_cast<PDWORD_PTR>(&windowIcon));
        if (!windowIcon) {
            windowIcon = reinterpret_cast<HICON>(GetClassLongPtrW(ownerHwnd, GCLP_HICONSM));
        }
        if (windowIcon) {
            if (auto bitmap = m_iconLoader(windowIcon)) {
                return bitmap;
            }
        }
        DWORD pid = 0;
        GetWindowThreadProcessId(ownerHwnd, &pid);
        if (pid != 0) {
            const UniqueHandle process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid));
            if (process.valid()) {
                wchar_t exePath[MAX_PATH]{};
                DWORD size = MAX_PATH;
                if (QueryFullProcessImageNameW(process.get(), 0, exePath, &size)) {
                    HICON extracted = nullptr;
                    if (ExtractIconExW(exePath, 0, &extracted, nullptr, 1) > 0 && extracted) {
                        const UniqueIcon owned(extracted);
                        if (auto bitmap = m_iconLoader(owned.get())) {
                            return bitmap;
                        }
                    }
                }
            }
        }
    }
    return m_iconLoader(LoadIconW(nullptr, IDI_APPLICATION));
}

void CALLBACK TrayTracker::onExplorerWinEvent(HWINEVENTHOOK, DWORD, HWND, LONG, LONG, DWORD,
                                              DWORD) {
    if (s_instance) {
        s_instance->armRefresh();
    }
}