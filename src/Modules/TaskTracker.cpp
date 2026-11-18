/**
 * @file TaskTracker.cpp
 *
 * @version 1.4.0
 * @author BleckWolf25
 * @license MIT
 *
 * @summary Implementation of the event-driven window list and activation switcher
 *
 * @description
 * Builds pinned slots first by matching each shortcut's key against running window identities,
 * then groups the remaining windows by application key while preserving their previous slot
 * positions so icons never jump on focus change. Icons are cached per key across rebuilds and
 * extracted through WM_GETICON, class icons or the executable image. Activation toggles minimize
 * for a single foreground window and cycles within a group. All rebuilds are triggered by
 * WinEvents or pin folder notifications.
 *
 * @since 17/11/2026
 * @updated 18/11/2026
 */
// ---------- IMPORTS
#include "Modules/TaskTracker.h"
#include "Core/ShellPaths.h"
#include "Core/WinResources.h"
#include "Core/WinString.h"
#include "Modules/ShortcutStore.h"
#include "Modules/WindowIdentity.h"
#include <shellapi.h>
#include <shobjidl.h>
#include <algorithm>
#include <filesystem>
#include <unordered_map>
#include <utility>
// ---------- CONSTANTS
namespace {
constexpr DWORD kHookFlags = WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS;
constexpr UINT kIconTimeoutMs = 50;
}
TaskTracker* TaskTracker::s_instance = nullptr;
// ---------- IMPLEMENTATION
TaskTracker::~TaskTracker() {
    stop();
}

void TaskTracker::start(HWND selfHwnd, IconLoader iconLoader, HBitmapLoader hbitmapLoader,
                        ChangeCallback onChanged) {
    m_selfHwnd = selfHwnd;
    m_iconLoader = std::move(iconLoader);
    m_hbitmapLoader = std::move(hbitmapLoader);
    m_onChanged = std::move(onChanged);
    s_instance = this;
    const DWORD ranges[][2] = {
        {EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND},
        {EVENT_OBJECT_CREATE, EVENT_OBJECT_HIDE},
        {EVENT_OBJECT_NAMECHANGE, EVENT_OBJECT_NAMECHANGE},
        {EVENT_OBJECT_CLOAKED, EVENT_OBJECT_UNCLOAKED},
        {EVENT_SYSTEM_DESKTOPSWITCH, EVENT_SYSTEM_DESKTOPSWITCH},
    };
    for (const auto& range : ranges) {
        if (HWINEVENTHOOK hook = SetWinEventHook(range[0], range[1], nullptr,
                                               &TaskTracker::onWinEvent, 0, 0, kHookFlags)) {
            m_hooks.push_back(hook);
        }
    }
    m_watcher.start(m_selfHwnd, kMsgPinsChanged, pinnedTaskbarDirectory());
    reloadPinnedApps();
}

void TaskTracker::stop() {
    m_watcher.stop();
    for (HWINEVENTHOOK hook : m_hooks) {
        UnhookWinEvent(hook);
    }
    m_hooks.clear();
    m_items.clear();
    m_pinned.clear();
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

void TaskTracker::reloadPinnedApps() {
    m_pinned.clear();
    const std::wstring pinnedDir = pinnedTaskbarDirectory();
    if (pinnedDir.empty()) {
        refresh();
        return;
    }
    std::error_code ec;
    if (!std::filesystem::exists(pinnedDir, ec)) {
        refresh();
        return;
    }
    std::vector<PinnedEntry> discovered;
    for (const auto& entry : std::filesystem::directory_iterator(pinnedDir, ec)) {
        if (ec || !entry.is_regular_file(ec)) {
            continue;
        }
        const auto& path = entry.path();
        if (toLowerWide(path.extension().wstring()) != L".lnk") {
            continue;
        }
        const DWORD attributes = GetFileAttributesW(path.c_str());
        if (attributes != INVALID_FILE_ATTRIBUTES &&
            (attributes & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM)) != 0) {
            continue;
        }
        PinnedEntry pinned{};
        pinned.name = path.stem().wstring();
        pinned.shortcutPath = path.wstring();
        const ShortcutMetadata metadata = resolveShortcutMetadata(pinned.shortcutPath);
        pinned.targetPathLower = metadata.targetPathLower;
        pinned.appUserModelIdLower = metadata.appUserModelIdLower;
        pinned.key = makeShortcutKey(metadata, pinned.shortcutPath);
        pinned.icon = loadIconForShortcut(pinned.shortcutPath);
        discovered.push_back(std::move(pinned));
    }
    // NovaUI's saved drag order takes precedence, then Explorer's Taskband Favorites order.
    const std::vector<std::wstring> savedOrder = readPinOrder();
    const std::vector<std::wstring> favoriteOrder = readTaskbandFavoriteNames();
    std::unordered_map<std::wstring, size_t> savedRank;
    for (size_t i = 0; i < savedOrder.size(); ++i) {
        savedRank.try_emplace(toLowerWide(savedOrder[i]), i);
    }
    std::unordered_map<std::wstring, size_t> favoriteRank;
    for (size_t i = 0; i < favoriteOrder.size(); ++i) {
        favoriteRank.try_emplace(favoriteOrder[i], i);
    }
    std::stable_sort(discovered.begin(), discovered.end(),
                     [&](const PinnedEntry& a, const PinnedEntry& b) {
                         const auto sa = savedRank.find(toLowerWide(a.name));
                         const auto sb = savedRank.find(toLowerWide(b.name));
                         const size_t rankA = (sa != savedRank.end()) ? sa->second : SIZE_MAX;
                         const size_t rankB = (sb != savedRank.end()) ? sb->second : SIZE_MAX;
                         if (rankA != rankB) {
                             return rankA < rankB;
                         }
                         const auto fa = favoriteRank.find(toLowerWide(a.name));
                         const auto fb = favoriteRank.find(toLowerWide(b.name));
                         const size_t favA = (fa != favoriteRank.end()) ? fa->second : SIZE_MAX;
                         const size_t favB = (fb != favoriteRank.end()) ? fb->second : SIZE_MAX;
                         return favA < favB;
                     });
    m_pinned = std::move(discovered);
    refresh();
}

void TaskTracker::refresh() {
    const std::vector<TaskItem> oldItems = std::move(m_items);
    m_items.clear();
    std::unordered_map<std::wstring, Microsoft::WRL::ComPtr<ID2D1Bitmap>> cachedIcons;
    std::vector<std::wstring> previousUnpinnedOrder;
    for (const auto& old : oldItems) {
        if (old.icon) {
            cachedIcons.try_emplace(old.key, old.icon);
        }
        if (!old.pinned && !old.key.empty()) {
            previousUnpinnedOrder.push_back(old.key);
        }
    }
    struct RawWindow {
        HWND hwnd = nullptr;
        std::wstring title;
        WindowIdentity identity;
        bool claimed = false;
    };
    struct EnumContext {
        HWND selfHwnd;
        std::vector<RawWindow>* out;
    };
    std::vector<RawWindow> openWindows;
    EnumContext context{m_selfHwnd, &openWindows};
    EnumWindows(
        [](HWND hwnd, LPARAM lp) -> BOOL {
            auto* ctx = reinterpret_cast<EnumContext*>(lp);
            if (!isTaskbarWindow(hwnd, ctx->selfHwnd)) {
                return TRUE;
            }
            wchar_t title[256]{};
            GetWindowTextW(hwnd, title, 256);
            RawWindow raw{};
            raw.hwnd = hwnd;
            raw.title = title;
            raw.identity = queryWindowIdentity(hwnd);
            ctx->out->push_back(std::move(raw));
            return TRUE;
        },
        reinterpret_cast<LPARAM>(&context));
    const auto matchesPinned = [](const PinnedEntry& pin, const RawWindow& raw) {
        if (!pin.appUserModelIdLower.empty() && !raw.identity.aumidLower.empty() &&
            pin.appUserModelIdLower == raw.identity.aumidLower) {
            return true;
        }
        return !pin.targetPathLower.empty() && !raw.identity.exePathLower.empty() &&
               pin.targetPathLower == raw.identity.exePathLower;
    };
    const HWND foreground = GetForegroundWindow();
    for (const auto& pin : m_pinned) {
        TaskItem item{};
        item.key = pin.key;
        item.appName = pin.name;
        item.shortcutPath = pin.shortcutPath;
        item.targetPath = pin.targetPathLower;
        item.appUserModelId = pin.appUserModelIdLower;
        item.icon = pin.icon;
        item.pinned = true;
        for (auto& raw : openWindows) {
            if (raw.claimed || !matchesPinned(pin, raw)) {
                continue;
            }
            raw.claimed = true;
            WindowEntry entry{};
            entry.hwnd = raw.hwnd;
            entry.title = raw.title;
            entry.active = (raw.hwnd == foreground);
            if (entry.active) {
                item.active = true;
                item.activeWindowIndex = item.windows.size();
            }
            if (item.targetPath.empty() && !raw.identity.exePath.empty()) {
                item.targetPath = raw.identity.exePath;
            }
            if (item.appUserModelId.empty() && !raw.identity.aumid.empty()) {
                item.appUserModelId = raw.identity.aumid;
            }
            item.windows.push_back(std::move(entry));
        }
        if (!item.windows.empty()) {
            item.running = true;
            if (!item.icon) {
                const auto cached = cachedIcons.find(item.key);
                item.icon = (cached != cachedIcons.end()) ? cached->second
                                                          : loadIconForWindow(item.windows[0].hwnd);
            }
        }
        m_items.push_back(std::move(item));
    }
    std::vector<TaskItem> unpinnedGroups;
    for (auto& raw : openWindows) {
        if (raw.claimed) {
            continue;
        }
        raw.claimed = true;
        auto group = std::find_if(unpinnedGroups.begin(), unpinnedGroups.end(),
                                  [&](const TaskItem& candidate) {
                                      return candidate.key == raw.identity.groupKey;
                                  });
        if (group == unpinnedGroups.end()) {
            TaskItem item{};
            item.key = raw.identity.groupKey;
            item.targetPath = raw.identity.exePath;
            item.appUserModelId = raw.identity.aumid;
            if (!raw.identity.exePath.empty()) {
                item.appName = std::filesystem::path(raw.identity.exePath).stem().wstring();
            }
            if (item.appName.empty()) {
                item.appName = raw.title;
            }
            item.running = true;
            item.pinned = false;
            const auto cached = cachedIcons.find(item.key);
            item.icon =
                (cached != cachedIcons.end()) ? cached->second : loadIconForWindow(raw.hwnd);
            unpinnedGroups.push_back(std::move(item));
            group = std::prev(unpinnedGroups.end());
        }
        WindowEntry entry{};
        entry.hwnd = raw.hwnd;
        entry.title = std::move(raw.title);
        entry.active = (raw.hwnd == foreground);
        if (entry.active) {
            group->active = true;
            group->activeWindowIndex = group->windows.size();
        }
        group->windows.push_back(std::move(entry));
    }
    std::unordered_map<std::wstring, size_t> unpinnedRank;
    for (size_t i = 0; i < previousUnpinnedOrder.size(); ++i) {
        unpinnedRank.try_emplace(previousUnpinnedOrder[i], i);
    }
    std::stable_sort(unpinnedGroups.begin(), unpinnedGroups.end(),
                     [&](const TaskItem& a, const TaskItem& b) {
                         const auto itA = unpinnedRank.find(a.key);
                         const auto itB = unpinnedRank.find(b.key);
                         const size_t rankA = (itA != unpinnedRank.end()) ? itA->second : SIZE_MAX;
                         const size_t rankB = (itB != unpinnedRank.end()) ? itB->second : SIZE_MAX;
                         return rankA < rankB;
                     });
    for (auto& group : unpinnedGroups) {
        m_items.push_back(std::move(group));
    }
    if (m_onChanged) {
        m_onChanged();
    }
}

void TaskTracker::activateOrLaunch(const TaskItem& item) const {
    if (!item.running || item.windows.empty()) {
        launchNewInstance(item);
        return;
    }
    if (item.windows.size() == 1) {
        const HWND target = item.windows[0].hwnd;
        if (!IsWindow(target)) {
            return;
        }
        if (GetForegroundWindow() == target && !IsIconic(target)) {
            ShowWindowAsync(target, SW_MINIMIZE);
            return;
        }
        focusWindow(target);
        return;
    }
    const size_t currentIndex =
        item.activeWindowIndex < item.windows.size() ? item.activeWindowIndex : 0;
    if (!item.active) {
        focusWindow(item.windows[currentIndex].hwnd);
    } else {
        focusWindow(item.windows[(currentIndex + 1) % item.windows.size()].hwnd);
    }
}

void TaskTracker::launchNewInstance(const TaskItem& item) const {
    if (!item.shortcutPath.empty()) {
        launchPath(item.shortcutPath);
    } else if (!item.targetPath.empty()) {
        launchPath(item.targetPath);
    }
}

void TaskTracker::closeAllWindows(const TaskItem& item) const {
    for (const auto& window : item.windows) {
        closeWindow(window.hwnd);
    }
}

void TaskTracker::focusWindow(HWND target) const {
    if (!IsWindow(target)) {
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

bool TaskTracker::canPin(const TaskItem& item) const noexcept {
    return !item.pinned && (!item.targetPath.empty() || !item.appUserModelId.empty());
}

bool TaskTracker::pinItem(const TaskItem& item) {
    if (!canPin(item)) {
        return false;
    }
    const std::wstring pinnedDir = pinnedTaskbarDirectory();
    if (pinnedDir.empty()) {
        return false;
    }
    std::error_code ec;
    std::filesystem::create_directories(pinnedDir, ec);
    const std::filesystem::path shortcut = uniqueShortcutPath(pinnedDir, item.appName);
    m_watcher.setSuppressed(true);
    const bool created = createShortcutFile(shortcut.wstring(), item.targetPath,
                                            item.appUserModelId, item.appName);
    m_watcher.setSuppressed(false);
    if (created) {
        reloadPinnedApps();
        savePinOrder();
    }
    return created;
}

void TaskTracker::unpinItem(const TaskItem& item) {
    if (!item.pinned || item.shortcutPath.empty()) {
        return;
    }
    m_watcher.setSuppressed(true);
    std::error_code ec;
    std::filesystem::remove(item.shortcutPath, ec);
    m_watcher.setSuppressed(false);
    reloadPinnedApps();
    savePinOrder();
}

void TaskTracker::pinExternalPath(const std::wstring& filePath) {
    if (filePath.empty()) {
        return;
    }
    const std::wstring pinnedDir = pinnedTaskbarDirectory();
    if (pinnedDir.empty()) {
        return;
    }
    std::error_code ec;
    std::filesystem::create_directories(pinnedDir, ec);
    const std::filesystem::path source(filePath);
    const std::wstring extension = toLowerWide(source.extension().wstring());
    const std::wstring stem = sanitizeFileName(source.stem().wstring());
    const std::filesystem::path destination = uniqueShortcutPath(pinnedDir, stem);
    m_watcher.setSuppressed(true);
    bool accepted = false;
    if (extension == L".lnk") {
        accepted = std::filesystem::copy_file(source, destination, ec);
    } else if (extension == L".exe") {
        accepted = createShortcutFile(destination.wstring(), source.wstring(), {}, stem);
    }
    m_watcher.setSuppressed(false);
    if (accepted) {
        reloadPinnedApps();
        savePinOrder();
    }
}

void TaskTracker::movePinnedItem(size_t fromIndex, size_t toIndex) {
    if (fromIndex >= m_pinned.size() || toIndex >= m_pinned.size() || fromIndex == toIndex) {
        return;
    }
    PinnedEntry moved = std::move(m_pinned[fromIndex]);
    m_pinned.erase(m_pinned.begin() + static_cast<ptrdiff_t>(fromIndex));
    m_pinned.insert(m_pinned.begin() + static_cast<ptrdiff_t>(toIndex), std::move(moved));
    savePinOrder();
    refresh();
}

void TaskTracker::launchPath(const std::wstring& path) {
    SHELLEXECUTEINFOW info{};
    info.cbSize = sizeof(info);
    info.fMask = SEE_MASK_FLAG_NO_UI;
    info.lpVerb = L"open";
    info.lpFile = path.c_str();
    info.nShow = SW_SHOWNORMAL;
    ShellExecuteExW(&info);
}

void TaskTracker::savePinOrder() const {
    std::vector<std::wstring> names;
    names.reserve(m_pinned.size());
    for (const auto& pin : m_pinned) {
        names.push_back(pin.name);
    }
    writePinOrder(names);
}

Microsoft::WRL::ComPtr<ID2D1Bitmap> TaskTracker::loadIconForWindow(HWND hwnd) const {
    if (!m_iconLoader) {
        return nullptr;
    }
    HICON icon = nullptr;
    SendMessageTimeoutW(hwnd, WM_GETICON, ICON_BIG, 0, SMTO_ABORTIFHUNG, kIconTimeoutMs,
                        reinterpret_cast<PDWORD_PTR>(&icon));
    if (!icon) {
        icon = reinterpret_cast<HICON>(GetClassLongPtrW(hwnd, GCLP_HICON));
    }
    if (!icon) {
        SendMessageTimeoutW(hwnd, WM_GETICON, ICON_SMALL2, 0, SMTO_ABORTIFHUNG, kIconTimeoutMs,
                            reinterpret_cast<PDWORD_PTR>(&icon));
    }
    if (!icon) {
        icon = reinterpret_cast<HICON>(GetClassLongPtrW(hwnd, GCLP_HICONSM));
    }
    if (icon) {
        if (auto bitmap = m_iconLoader(icon)) {
            return bitmap;
        }
    }
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
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
    return m_iconLoader(LoadIconW(nullptr, IDI_APPLICATION));
}

Microsoft::WRL::ComPtr<ID2D1Bitmap> TaskTracker::loadIconForShortcut(
    const std::wstring& shortcutPath) const {
    if (m_hbitmapLoader) {
        Microsoft::WRL::ComPtr<IShellItemImageFactory> imageFactory;
        if (SUCCEEDED(SHCreateItemFromParsingName(shortcutPath.c_str(), nullptr,
                                                  IID_PPV_ARGS(imageFactory.GetAddressOf())))) {
            HBITMAP rawBitmap = nullptr;
            if (SUCCEEDED(imageFactory->GetImage(SIZE{64, 64},
                                                 SIIGBF_ICONONLY | SIIGBF_BIGGERSIZEOK,
                                                 &rawBitmap)) &&
                rawBitmap) {
                const UniqueBitmap owned(rawBitmap);
                if (auto bitmap = m_hbitmapLoader(owned.get())) {
                    return bitmap;
                }
            }
        }
    }
    if (m_iconLoader) {
        SHFILEINFOW fileInfo{};
        if (SHGetFileInfoW(shortcutPath.c_str(), 0, &fileInfo, sizeof(fileInfo),
                           SHGFI_ICON | SHGFI_LARGEICON) != 0 &&
            fileInfo.hIcon) {
            const UniqueIcon owned(fileInfo.hIcon);
            if (auto bitmap = m_iconLoader(owned.get())) {
                return bitmap;
            }
        }
        return m_iconLoader(LoadIconW(nullptr, IDI_APPLICATION));
    }
    return nullptr;
}

void CALLBACK TaskTracker::onWinEvent(HWINEVENTHOOK, DWORD event, HWND hwnd, LONG idObject,
                                      LONG idChild, DWORD, DWORD) {
    if (!s_instance || !hwnd) {
        return;
    }
    if (event == EVENT_SYSTEM_DESKTOPSWITCH) {
        s_instance->refresh();
        return;
    }
    if (idObject != OBJID_WINDOW || idChild != CHILDID_SELF) {
        return;
    }
    if (event == EVENT_SYSTEM_FOREGROUND || event == EVENT_OBJECT_DESTROY ||
        event == EVENT_OBJECT_HIDE || event == EVENT_OBJECT_CLOAKED ||
        isTaskbarWindow(hwnd, s_instance->m_selfHwnd)) {
        s_instance->refresh();
    }
}