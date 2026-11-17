/**
 * @file TaskTracker.cpp
 *
 * @version 1.3.0
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

#include <appmodel.h>
#include <dwmapi.h>
#include <propkey.h>
#include <propsys.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>

#include <algorithm>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <unordered_map>
#include <utility>

// ---------- CONSTANTS
namespace {
constexpr DWORD kHookFlags = WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS;

std::wstring toLower(std::wstring text) {
    for (wchar_t& ch : text) {
        ch = static_cast<wchar_t>(std::towlower(ch));
    }
    return text;
}

std::wstring expandEnvVars(const std::wstring& input) {
    if (input.empty()) {
        return {};
    }
    const DWORD required = ExpandEnvironmentStringsW(input.c_str(), nullptr, 0);
    if (required == 0) {
        return input;
    }
    std::wstring expanded(required, L'\0');
    if (ExpandEnvironmentStringsW(input.c_str(), expanded.data(), required) == 0) {
        return input;
    }
    if (!expanded.empty() && expanded.back() == L'\0') {
        expanded.pop_back();
    }
    return expanded;
}

std::wstring getRoamingDirectory() {
    PWSTR roamingRaw = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &roamingRaw)) ||
        !roamingRaw) {
        return {};
    }
    std::wstring dir(roamingRaw);
    CoTaskMemFree(roamingRaw);
    return dir;
}

std::wstring getPinnedTaskbarDirectory() {
    std::wstring dir = getRoamingDirectory();
    if (dir.empty()) {
        return {};
    }
    dir += L"\\Microsoft\\Internet Explorer\\Quick Launch\\User Pinned\\TaskBar";
    return dir;
}

std::wstring getNovaConfigDirectory() {
    std::wstring dir = getRoamingDirectory();
    if (dir.empty()) {
        return {};
    }
    dir += L"\\NovaUI";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    return dir;
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

std::vector<std::wstring> readTaskbandFavoriteNames() {
    std::vector<std::wstring> names;

    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
                      L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Taskband", 0,
                      KEY_READ, &key) != ERROR_SUCCESS) {
        return names;
    }

    DWORD type = 0;
    DWORD byteCount = 0;
    if (RegQueryValueExW(key, L"Favorites", nullptr, &type, nullptr, &byteCount) != ERROR_SUCCESS ||
        type != REG_BINARY || byteCount < 4) {
        RegCloseKey(key);
        return names;
    }

    std::vector<BYTE> blob(byteCount);
    if (RegQueryValueExW(key, L"Favorites", nullptr, nullptr, blob.data(), &byteCount) !=
        ERROR_SUCCESS) {
        RegCloseKey(key);
        return names;
    }
    RegCloseKey(key);

    size_t pos = 0;
    while (pos < blob.size()) {
        if (blob[pos] == 0xFF) {
            break;
        }
        if (blob[pos] != 0x00) {
            ++pos;
            continue;
        }
        ++pos;

        const size_t pidlStart = pos;
        bool validPidl = false;
        while (pos + sizeof(USHORT) <= blob.size()) {
            USHORT cb = 0;
            std::memcpy(&cb, blob.data() + pos, sizeof(USHORT));
            pos += sizeof(USHORT);
            if (cb == 0) {
                validPidl = (pos > pidlStart + sizeof(USHORT));
                break;
            }
            if (cb < sizeof(USHORT) || pos + (cb - sizeof(USHORT)) > blob.size()) {
                validPidl = false;
                pos = blob.size();
                break;
            }
            pos += (cb - sizeof(USHORT));
        }

        if (validPidl) {
            const auto* pidl = reinterpret_cast<LPCITEMIDLIST>(blob.data() + pidlStart);
            PWSTR displayName = nullptr;
            if (SUCCEEDED(SHGetNameFromIDList(pidl, SIGDN_NORMALDISPLAY, &displayName)) &&
                displayName) {
                names.push_back(toLower(displayName));
                CoTaskMemFree(displayName);
            }
        }
    }

    return names;
}

void resolveShortcutMetadata(const std::wstring& lnkPath, std::wstring& outTargetLower,
                             std::wstring& outAumidLower) {
    outTargetLower.clear();
    outAumidLower.clear();

    Microsoft::WRL::ComPtr<IShellLinkW> shellLink;
    if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(shellLink.GetAddressOf())))) {
        return;
    }

    Microsoft::WRL::ComPtr<IPersistFile> persistFile;
    if (FAILED(shellLink.As(&persistFile)) ||
        FAILED(persistFile->Load(lnkPath.c_str(), STGM_READ))) {
        return;
    }

    wchar_t targetBuf[MAX_PATH]{};
    if (SUCCEEDED(shellLink->GetPath(targetBuf, MAX_PATH, nullptr, SLGP_RAWPATH)) &&
        targetBuf[0] != L'\0') {
        outTargetLower = toLower(expandEnvVars(targetBuf));
    }

    Microsoft::WRL::ComPtr<IPropertyStore> propStore;
    if (SUCCEEDED(shellLink.As(&propStore))) {
        PROPVARIANT pv{};
        PropVariantInit(&pv);
        if (SUCCEEDED(propStore->GetValue(PKEY_AppUserModel_ID, &pv))) {
            if (pv.vt == VT_LPWSTR && pv.pwszVal != nullptr) {
                outAumidLower = toLower(pv.pwszVal);
            }
            PropVariantClear(&pv);
        }
    }
}

bool createShortcutFile(const std::wstring& lnkPath, const std::wstring& targetPath,
                        const std::wstring& aumid, const std::wstring& description) {
    Microsoft::WRL::ComPtr<IShellLinkW> shellLink;
    if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(shellLink.GetAddressOf())))) {
        return false;
    }

    if (!targetPath.empty()) {
        shellLink->SetPath(targetPath.c_str());
        std::filesystem::path p(targetPath);
        if (p.has_parent_path()) {
            shellLink->SetWorkingDirectory(p.parent_path().c_str());
        }
    }
    if (!description.empty()) {
        shellLink->SetDescription(description.c_str());
    }

    if (!aumid.empty()) {
        Microsoft::WRL::ComPtr<IPropertyStore> propStore;
        if (SUCCEEDED(shellLink.As(&propStore))) {
            PROPVARIANT pv{};
            pv.vt = VT_LPWSTR;
            const size_t bytes = (aumid.size() + 1) * sizeof(wchar_t);
            pv.pwszVal = static_cast<LPWSTR>(CoTaskMemAlloc(bytes));
            if (pv.pwszVal) {
                std::memcpy(pv.pwszVal, aumid.c_str(), bytes);
                propStore->SetValue(PKEY_AppUserModel_ID, pv);
                propStore->Commit();
            }
            PropVariantClear(&pv);
        }
    }

    Microsoft::WRL::ComPtr<IPersistFile> persistFile;
    if (FAILED(shellLink.As(&persistFile))) {
        return false;
    }
    return SUCCEEDED(persistFile->Save(lnkPath.c_str(), TRUE));
}

struct WindowIdentity {
    std::wstring exePath;
    std::wstring exePathLower;
    std::wstring aumid;
    std::wstring aumidLower;
    std::wstring groupKey;
};

WindowIdentity queryWindowIdentity(HWND hwnd) {
    WindowIdentity id{};

    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == 0) {
        return id;
    }

    auto queryProcess = [&](DWORD targetPid) {
        HANDLE proc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, targetPid);
        if (!proc) {
            return;
        }
        wchar_t exePath[MAX_PATH]{};
        DWORD size = MAX_PATH;
        if (QueryFullProcessImageNameW(proc, 0, exePath, &size)) {
            id.exePath = exePath;
            id.exePathLower = toLower(id.exePath);
        }

        UINT32 aumidLen = 0;
        if (GetApplicationUserModelId(proc, &aumidLen, nullptr) == ERROR_INSUFFICIENT_BUFFER &&
            aumidLen > 1) {
            std::wstring aumid(aumidLen, L'\0');
            if (GetApplicationUserModelId(proc, &aumidLen, aumid.data()) == ERROR_SUCCESS) {
                if (!aumid.empty() && aumid.back() == L'\0') {
                    aumid.pop_back();
                }
                id.aumid = aumid;
                id.aumidLower = toLower(std::move(aumid));
            }
        }
        CloseHandle(proc);
    };

    queryProcess(pid);

    if (id.exePathLower.ends_with(L"\\applicationframehost.exe")) {
        struct ChildCtx {
            DWORD hostPid;
            DWORD innerPid;
        } ctx{pid, 0};

        EnumChildWindows(
            hwnd,
            [](HWND child, LPARAM lp) -> BOOL {
                auto* c = reinterpret_cast<ChildCtx*>(lp);
                DWORD childPid = 0;
                GetWindowThreadProcessId(child, &childPid);
                if (childPid != 0 && childPid != c->hostPid) {
                    c->innerPid = childPid;
                    return FALSE;
                }
                return TRUE;
            },
            reinterpret_cast<LPARAM>(&ctx));

        if (ctx.innerPid != 0) {
            queryProcess(ctx.innerPid);
        }
    }

    if (!id.aumidLower.empty()) {
        id.groupKey = L"aumid:" + id.aumidLower;
    } else if (!id.exePathLower.empty()) {
        id.groupKey = L"exe:" + id.exePathLower;
    } else {
        id.groupKey = L"hwnd:" + std::to_wstring(reinterpret_cast<uintptr_t>(hwnd));
    }

    return id;
}

std::wstring sanitizeFileName(std::wstring name) {
    for (wchar_t& ch : name) {
        if (ch == L'\\' || ch == L'/' || ch == L':' || ch == L'*' || ch == L'?' ||
            ch == L'"' || ch == L'<' || ch == L'>' || ch == L'|') {
            ch = L'_';
        }
    }
    while (!name.empty() && (name.back() == L' ' || name.back() == L'.')) {
        name.pop_back();
    }
    return name.empty() ? L"PinnedApp" : name;
}
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
    };
    for (const auto& range : ranges) {
        if (HWINEVENTHOOK hook = SetWinEventHook(range[0], range[1], nullptr,
                                                 &TaskTracker::onWinEvent, 0, 0, kHookFlags)) {
            m_hooks.push_back(hook);
        }
    }

    const std::wstring pinnedDir = getPinnedTaskbarDirectory();
    if (!pinnedDir.empty()) {
        startPinWatcher(pinnedDir);
    }

    reloadPinnedApps();
}

void TaskTracker::stop() {
    stopPinWatcher();

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

    const std::wstring pinnedDir = getPinnedTaskbarDirectory();
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
        if (toLower(path.extension().wstring()) != L".lnk") {
            continue;
        }

        const DWORD attrs = GetFileAttributesW(path.c_str());
        if (attrs != INVALID_FILE_ATTRIBUTES &&
            (attrs & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM)) != 0) {
            continue;
        }

        PinnedEntry pinned{};
        pinned.name = path.stem().wstring();
        pinned.shortcutPath = path.wstring();
        resolveShortcutMetadata(pinned.shortcutPath, pinned.targetPathLower,
                                pinned.appUserModelIdLower);
        pinned.key = !pinned.appUserModelIdLower.empty()
                         ? (L"aumid:" + pinned.appUserModelIdLower)
                         : (!pinned.targetPathLower.empty()
                                ? (L"exe:" + pinned.targetPathLower)
                                : (L"lnk:" + toLower(pinned.shortcutPath)));
        pinned.icon = loadIconForShortcut(pinned.shortcutPath);
        discovered.push_back(std::move(pinned));
    }

    // Prefer NovaUI's saved drag order first, then Windows Taskband Favorites order
    const std::vector<std::wstring> savedOrder = loadSavedPinOrder();
    const std::vector<std::wstring> favoriteOrder = readTaskbandFavoriteNames();

    std::unordered_map<std::wstring, size_t> savedRank;
    for (size_t i = 0; i < savedOrder.size(); ++i) {
        savedRank.try_emplace(toLower(savedOrder[i]), i);
    }

    std::unordered_map<std::wstring, size_t> favRank;
    for (size_t i = 0; i < favoriteOrder.size(); ++i) {
        favRank.try_emplace(favoriteOrder[i], i);
    }

    std::stable_sort(discovered.begin(), discovered.end(),
                     [&](const PinnedEntry& a, const PinnedEntry& b) {
                         const auto sa = savedRank.find(toLower(a.name));
                         const auto sb = savedRank.find(toLower(b.name));
                         const size_t rA = (sa != savedRank.end()) ? sa->second : SIZE_MAX;
                         const size_t rB = (sb != savedRank.end()) ? sb->second : SIZE_MAX;
                         if (rA != rB) {
                             return rA < rB;
                         }
                         const auto fa = favRank.find(toLower(a.name));
                         const auto fb = favRank.find(toLower(b.name));
                         const size_t fA = (fa != favRank.end()) ? fa->second : SIZE_MAX;
                         const size_t fB = (fb != favRank.end()) ? fb->second : SIZE_MAX;
                         return fA < fB;
                     });

    m_pinned = std::move(discovered);
    refresh();
}

void TaskTracker::refresh() {
    std::vector<TaskItem> oldItems = std::move(m_items);
    m_items.clear();

    // Cache existing icons and unpinned group order from previous frame
    std::unordered_map<std::wstring, Microsoft::WRL::ComPtr<ID2D1Bitmap>> cachedIcons;
    std::vector<std::wstring> previousUnpinnedOrder;
    for (auto& old : oldItems) {
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

    std::vector<TaskItem> enumerated;
    struct EnumCtx {
        HWND selfHwnd;
        std::vector<TaskItem>* out;
    } ctx{m_selfHwnd, &enumerated};

    EnumWindows(&TaskTracker::enumWindowsProc, reinterpret_cast<LPARAM>(&ctx));

    std::vector<RawWindow> openWindows;
    openWindows.reserve(enumerated.size());
    for (auto& e : enumerated) {
        RawWindow rw{};
        rw.hwnd = e.primaryHwnd();
        rw.title = std::move(e.appName);
        rw.identity = queryWindowIdentity(rw.hwnd);
        openWindows.push_back(std::move(rw));
    }

    auto matchesPinned = [](const PinnedEntry& pin, const RawWindow& rw) -> bool {
        if (!pin.appUserModelIdLower.empty() && !rw.identity.aumidLower.empty() &&
            pin.appUserModelIdLower == rw.identity.aumidLower) {
            return true;
        }
        if (!pin.targetPathLower.empty() && !rw.identity.exePathLower.empty() &&
            pin.targetPathLower == rw.identity.exePathLower) {
            return true;
        }
        return false;
    };

    const HWND foreground = GetForegroundWindow();

    // 1. Build pinned slots first, grouping all open windows of each pinned app together
    for (const auto& pin : m_pinned) {
        TaskItem item{};
        item.key = pin.key;
        item.appName = pin.name;
        item.shortcutPath = pin.shortcutPath;
        item.targetPath = pin.targetPathLower;
        item.appUserModelId = pin.appUserModelIdLower;
        item.icon = pin.icon;
        item.pinned = true;

        for (auto& rw : openWindows) {
            if (rw.claimed || !matchesPinned(pin, rw)) {
                continue;
            }
            rw.claimed = true;

            WindowEntry we{};
            we.hwnd = rw.hwnd;
            we.title = rw.title;
            we.active = (rw.hwnd == foreground);
            if (we.active) {
                item.active = true;
                item.activeWindowIndex = item.windows.size();
            }
            if (item.targetPath.empty() && !rw.identity.exePath.empty()) {
                item.targetPath = rw.identity.exePath;
            }
            if (item.appUserModelId.empty() && !rw.identity.aumid.empty()) {
                item.appUserModelId = rw.identity.aumid;
            }
            item.windows.push_back(std::move(we));
        }

        if (!item.windows.empty()) {
            item.running = true;
            if (!item.icon) {
                auto cachedIt = cachedIcons.find(item.key);
                item.icon = (cachedIt != cachedIcons.end())
                                ? cachedIt->second
                                : loadIconForWindow(item.windows[0].hwnd);
            }
        }

        m_items.push_back(std::move(item));
    }

    // 2. Group remaining unpinned windows by application key
    std::vector<TaskItem> unpinnedGroups;
    for (auto& rw : openWindows) {
        if (rw.claimed) {
            continue;
        }
        rw.claimed = true;

        auto grpIt = std::find_if(unpinnedGroups.begin(), unpinnedGroups.end(),
                                  [&](const TaskItem& g) { return g.key == rw.identity.groupKey; });
        if (grpIt == unpinnedGroups.end()) {
            TaskItem item{};
            item.key = rw.identity.groupKey;
            item.targetPath = rw.identity.exePath;
            item.appUserModelId = rw.identity.aumid;
            if (!rw.identity.exePath.empty()) {
                item.appName = std::filesystem::path(rw.identity.exePath).stem().wstring();
            }
            if (item.appName.empty()) {
                item.appName = rw.title;
            }
            item.running = true;
            item.pinned = false;

            auto cachedIt = cachedIcons.find(item.key);
            item.icon = (cachedIt != cachedIcons.end()) ? cachedIt->second
                                                        : loadIconForWindow(rw.hwnd);

            unpinnedGroups.push_back(std::move(item));
            grpIt = std::prev(unpinnedGroups.end());
        }

        WindowEntry we{};
        we.hwnd = rw.hwnd;
        we.title = std::move(rw.title);
        we.active = (rw.hwnd == foreground);
        if (we.active) {
            grpIt->active = true;
            grpIt->activeWindowIndex = grpIt->windows.size();
        }
        grpIt->windows.push_back(std::move(we));
    }

    // Preserve previous unpinned slot positions so icons never jump on focus change
    std::unordered_map<std::wstring, size_t> unpinnedRank;
    for (size_t i = 0; i < previousUnpinnedOrder.size(); ++i) {
        unpinnedRank.try_emplace(previousUnpinnedOrder[i], i);
    }
    std::stable_sort(unpinnedGroups.begin(), unpinnedGroups.end(),
                     [&](const TaskItem& a, const TaskItem& b) {
                         const auto itA = unpinnedRank.find(a.key);
                         const auto itB = unpinnedRank.find(b.key);
                         const size_t rA = (itA != unpinnedRank.end()) ? itA->second : SIZE_MAX;
                         const size_t rB = (itB != unpinnedRank.end()) ? itB->second : SIZE_MAX;
                         return rA < rB;
                     });

    for (auto& grp : unpinnedGroups) {
        m_items.push_back(std::move(grp));
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
        HWND target = item.windows[0].hwnd;
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

    // Multi-window group: if inactive, focus the last active window; if active, cycle next
    const size_t currentIdx =
        item.activeWindowIndex < item.windows.size() ? item.activeWindowIndex : 0;
    if (!item.active) {
        focusWindow(item.windows[currentIdx].hwnd);
    } else {
        const size_t nextIdx = (currentIdx + 1) % item.windows.size();
        focusWindow(item.windows[nextIdx].hwnd);
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
    for (const auto& win : item.windows) {
        closeWindow(win.hwnd);
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

void TaskTracker::pinItem(const TaskItem& item) {
    if (!canPin(item)) {
        return;
    }

    const std::wstring pinnedDir = getPinnedTaskbarDirectory();
    if (pinnedDir.empty()) {
        return;
    }

    std::error_code ec;
    std::filesystem::create_directories(pinnedDir, ec);

    const std::wstring baseName = sanitizeFileName(item.appName);
    std::filesystem::path lnkPath = std::filesystem::path(pinnedDir) / (baseName + L".lnk");
    int suffix = 2;
    while (std::filesystem::exists(lnkPath, ec)) {
        lnkPath = std::filesystem::path(pinnedDir) /
                  (baseName + L" (" + std::to_wstring(suffix++) + L").lnk");
    }

    m_suppressWatcher = true;
    const bool created =
        createShortcutFile(lnkPath.wstring(), item.targetPath, item.appUserModelId, item.appName);
    m_suppressWatcher = false;

    if (created) {
        reloadPinnedApps();
        savePinOrder();
    }
}

void TaskTracker::unpinItem(const TaskItem& item) {
    if (!item.pinned || item.shortcutPath.empty()) {
        return;
    }

    m_suppressWatcher = true;
    std::error_code ec;
    std::filesystem::remove(item.shortcutPath, ec);
    m_suppressWatcher = false;

    reloadPinnedApps();
    savePinOrder();
}

void TaskTracker::pinExternalPath(const std::wstring& filePath) {
    if (filePath.empty()) {
        return;
    }

    const std::wstring pinnedDir = getPinnedTaskbarDirectory();
    if (pinnedDir.empty()) {
        return;
    }

    std::error_code ec;
    std::filesystem::create_directories(pinnedDir, ec);

    const std::filesystem::path src(filePath);
    const std::wstring ext = toLower(src.extension().wstring());
    const std::wstring stem = sanitizeFileName(src.stem().wstring());

    std::filesystem::path dest = std::filesystem::path(pinnedDir) / (stem + L".lnk");
    int suffix = 2;
    while (std::filesystem::exists(dest, ec)) {
        dest = std::filesystem::path(pinnedDir) /
               (stem + L" (" + std::to_wstring(suffix++) + L").lnk");
    }

    m_suppressWatcher = true;
    bool ok = false;
    if (ext == L".lnk") {
        ok = std::filesystem::copy_file(src, dest, ec);
    } else if (ext == L".exe") {
        ok = createShortcutFile(dest.wstring(), src.wstring(), {}, stem);
    }
    m_suppressWatcher = false;

    if (ok) {
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

void TaskTracker::launchPath(const std::wstring& path) {
    SHELLEXECUTEINFOW sei{};
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_FLAG_NO_UI;
    sei.lpVerb = L"open";
    sei.lpFile = path.c_str();
    sei.nShow = SW_SHOWNORMAL;
    ShellExecuteExW(&sei);
}

void TaskTracker::savePinOrder() const {
    const std::wstring cfgDir = getNovaConfigDirectory();
    if (cfgDir.empty()) {
        return;
    }
    std::ofstream out(std::filesystem::path(cfgDir) / L"pins.txt", std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        return;
    }
    for (const auto& pin : m_pinned) {
        out << wideToUtf8(pin.name) << "\n";
    }
}

std::vector<std::wstring> TaskTracker::loadSavedPinOrder() const {
    std::vector<std::wstring> order;
    const std::wstring cfgDir = getNovaConfigDirectory();
    if (cfgDir.empty()) {
        return order;
    }
    std::ifstream in(std::filesystem::path(cfgDir) / L"pins.txt", std::ios::binary);
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (!line.empty()) {
            order.push_back(utf8ToWide(line));
        }
    }
    return order;
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

Microsoft::WRL::ComPtr<ID2D1Bitmap> TaskTracker::loadIconForShortcut(
    const std::wstring& shortcutPath) const {
    if (m_hbitmapLoader) {
        Microsoft::WRL::ComPtr<IShellItemImageFactory> imageFactory;
        if (SUCCEEDED(SHCreateItemFromParsingName(shortcutPath.c_str(), nullptr,
                                                  IID_PPV_ARGS(imageFactory.GetAddressOf())))) {
            HBITMAP hbm = nullptr;
            if (SUCCEEDED(imageFactory->GetImage(SIZE{64, 64},
                                                 SIIGBF_ICONONLY | SIIGBF_BIGGERSIZEOK, &hbm)) &&
                hbm) {
                auto bmp = m_hbitmapLoader(hbm);
                DeleteObject(hbm);
                if (bmp) {
                    return bmp;
                }
            }
        }
    }

    if (m_iconLoader) {
        SHFILEINFOW sfi{};
        if (SHGetFileInfoW(shortcutPath.c_str(), 0, &sfi, sizeof(sfi),
                           SHGFI_ICON | SHGFI_LARGEICON) != 0 &&
            sfi.hIcon) {
            auto bmp = m_iconLoader(sfi.hIcon);
            DestroyIcon(sfi.hIcon);
            if (bmp) {
                return bmp;
            }
        }
        return m_iconLoader(LoadIconW(nullptr, IDI_APPLICATION));
    }

    return nullptr;
}

void TaskTracker::startPinWatcher(const std::wstring& pinnedDir) {
    m_pinDirHandle = FindFirstChangeNotificationW(
        pinnedDir.c_str(), FALSE, FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE);
    if (m_pinDirHandle == INVALID_HANDLE_VALUE) {
        return;
    }

    RegisterWaitForSingleObject(&m_pinWaitHandle, m_pinDirHandle, &TaskTracker::onPinDirChanged,
                                this, INFINITE, WT_EXECUTEDEFAULT);
}

void TaskTracker::stopPinWatcher() {
    if (m_pinWaitHandle) {
        UnregisterWaitEx(m_pinWaitHandle, INVALID_HANDLE_VALUE);
        m_pinWaitHandle = nullptr;
    }
    if (m_pinDirHandle != INVALID_HANDLE_VALUE) {
        FindCloseChangeNotification(m_pinDirHandle);
        m_pinDirHandle = INVALID_HANDLE_VALUE;
    }
}

void CALLBACK TaskTracker::onPinDirChanged(PVOID context, BOOLEAN) {
    auto* self = static_cast<TaskTracker*>(context);
    if (!self) {
        return;
    }
    if (self->m_pinDirHandle != INVALID_HANDLE_VALUE) {
        FindNextChangeNotification(self->m_pinDirHandle);
    }
    if (!self->m_suppressWatcher && self->m_selfHwnd) {
        PostMessageW(self->m_selfHwnd, kMsgPinsChanged, 0, 0);
    }
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
    item.appName = title;
    item.windows.push_back(WindowEntry{hwnd, title, false});
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