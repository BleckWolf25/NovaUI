/**
 * @file TaskTracker.cpp
 *
 * @version 1.2.0
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

std::wstring getPinnedTaskbarDirectory() {
    PWSTR roamingRaw = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &roamingRaw)) ||
        !roamingRaw) {
        return {};
    }
    std::wstring dir(roamingRaw);
    CoTaskMemFree(roamingRaw);
    dir += L"\\Microsoft\\Internet Explorer\\Quick Launch\\User Pinned\\TaskBar";
    return dir;
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

    // Parse serialized ITEMIDLIST entries separated by 0x00 markers (terminated by 0xFF)
    size_t pos = 0;
    while (pos < blob.size()) {
        if (blob[pos] == 0xFF) {
            break;
        }
        if (blob[pos] != 0x00) {
            ++pos;
            continue;
        }
        ++pos; // Advance past 0x00 entry prefix

        // Walk SHITEMID chain until 2-byte null terminator
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

struct WindowIdentity {
    std::wstring exePathLower;
    std::wstring aumidLower;
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
            id.exePathLower = toLower(exePath);
        }

        UINT32 aumidLen = 0;
        if (GetApplicationUserModelId(proc, &aumidLen, nullptr) == ERROR_INSUFFICIENT_BUFFER &&
            aumidLen > 1) {
            std::wstring aumid(aumidLen, L'\0');
            if (GetApplicationUserModelId(proc, &aumidLen, aumid.data()) == ERROR_SUCCESS) {
                if (!aumid.empty() && aumid.back() == L'\0') {
                    aumid.pop_back();
                }
                id.aumidLower = toLower(std::move(aumid));
            }
        }
        CloseHandle(proc);
    };

    queryProcess(pid);

    // Unwrap UWP ApplicationFrameHost windows to inspect the hosted CoreWindow process
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

    return id;
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
        pinned.icon = loadIconForShortcut(pinned.shortcutPath);
        discovered.push_back(std::move(pinned));
    }

    // Order discovered shortcuts according to Shell Taskband Favorites when available
    const std::vector<std::wstring> favoriteOrder = readTaskbandFavoriteNames();
    if (!favoriteOrder.empty()) {
        std::unordered_map<std::wstring, size_t> rankByName;
        rankByName.reserve(favoriteOrder.size());
        for (size_t i = 0; i < favoriteOrder.size(); ++i) {
            rankByName.try_emplace(favoriteOrder[i], i);
        }

        std::stable_sort(discovered.begin(), discovered.end(),
                         [&](const PinnedEntry& a, const PinnedEntry& b) {
                             const auto itA = rankByName.find(toLower(a.name));
                             const auto itB = rankByName.find(toLower(b.name));
                             const size_t rankA =
                                 (itA != rankByName.end()) ? itA->second : SIZE_MAX;
                             const size_t rankB =
                                 (itB != rankByName.end()) ? itB->second : SIZE_MAX;
                             return rankA < rankB;
                         });
    }

    m_pinned = std::move(discovered);
    refresh();
}

void TaskTracker::refresh() {
    std::vector<TaskItem> oldItems = std::move(m_items);
    m_items.clear();

    struct OpenWindow {
        HWND hwnd = nullptr;
        std::wstring title;
        WindowIdentity identity;
        Microsoft::WRL::ComPtr<ID2D1Bitmap> cachedIcon;
        bool claimed = false;
    };

    std::vector<TaskItem> rawWindows;
    struct EnumCtx {
        HWND selfHwnd;
        std::vector<TaskItem>* out;
    } ctx{m_selfHwnd, &rawWindows};

    EnumWindows(&TaskTracker::enumWindowsProc, reinterpret_cast<LPARAM>(&ctx));

    // Preserve previous slot order for unpinned windows and reuse cached window icons
    std::vector<OpenWindow> openWindows;
    openWindows.reserve(rawWindows.size());

    auto appendOpenWindow = [&](HWND hwnd, std::wstring&& title,
                                Microsoft::WRL::ComPtr<ID2D1Bitmap>&& cachedIcon) {
        OpenWindow ow{};
        ow.hwnd = hwnd;
        ow.title = std::move(title);
        ow.identity = queryWindowIdentity(hwnd);
        ow.cachedIcon = std::move(cachedIcon);
        openWindows.push_back(std::move(ow));
    };

    for (auto& old : oldItems) {
        if (!old.running || !old.hwnd) {
            continue;
        }
        auto it = std::find_if(rawWindows.begin(), rawWindows.end(),
                               [&](const TaskItem& d) { return d.hwnd == old.hwnd; });
        if (it != rawWindows.end()) {
            appendOpenWindow(old.hwnd, std::move(it->title), std::move(old.icon));
            rawWindows.erase(it);
        }
    }
    for (auto& fresh : rawWindows) {
        appendOpenWindow(fresh.hwnd, std::move(fresh.title), nullptr);
    }

    auto matchesPinned = [](const PinnedEntry& pin, const OpenWindow& ow) -> bool {
        if (!pin.appUserModelIdLower.empty() && !ow.identity.aumidLower.empty() &&
            pin.appUserModelIdLower == ow.identity.aumidLower) {
            return true;
        }
        if (!pin.targetPathLower.empty() && !ow.identity.exePathLower.empty() &&
            pin.targetPathLower == ow.identity.exePathLower) {
            return true;
        }
        return false;
    };

    const HWND foreground = GetForegroundWindow();

    // 1. Populate pinned slots first, binding the primary matching open window if running
    for (const auto& pin : m_pinned) {
        TaskItem item{};
        item.title = pin.name;
        item.shortcutPath = pin.shortcutPath;
        item.icon = pin.icon;
        item.pinned = true;

        auto matchIt = std::find_if(openWindows.begin(), openWindows.end(), [&](const OpenWindow& ow) {
            return !ow.claimed && matchesPinned(pin, ow);
        });
        if (matchIt != openWindows.end()) {
            matchIt->claimed = true;
            item.hwnd = matchIt->hwnd;
            item.title = matchIt->title;
            item.running = true;
            item.active = (matchIt->hwnd == foreground);
            if (!item.icon) {
                item.icon = matchIt->cachedIcon ? matchIt->cachedIcon
                                                : loadIconForWindow(matchIt->hwnd);
            }
        }

        m_items.push_back(std::move(item));
    }

    // 2. Append remaining open windows (unpinned apps + extra instances of pinned apps)
    for (auto& ow : openWindows) {
        if (ow.claimed) {
            continue;
        }
        TaskItem item{};
        item.hwnd = ow.hwnd;
        item.title = std::move(ow.title);
        item.running = true;
        item.active = (ow.hwnd == foreground);
        item.icon = ow.cachedIcon ? std::move(ow.cachedIcon) : loadIconForWindow(ow.hwnd);
        m_items.push_back(std::move(item));
    }

    if (m_onChanged) {
        m_onChanged();
    }
}

void TaskTracker::activateOrLaunch(const TaskItem& item) const {
    if (item.running && item.hwnd) {
        toggleWindow(item.hwnd);
        return;
    }
    if (!item.shortcutPath.empty()) {
        launchShortcut(item.shortcutPath);
    }
}

void TaskTracker::middleClickItem(const TaskItem& item) const {
    if (item.running && item.hwnd) {
        closeWindow(item.hwnd);
        return;
    }
    if (!item.shortcutPath.empty()) {
        launchShortcut(item.shortcutPath);
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

void TaskTracker::launchShortcut(const std::wstring& shortcutPath) {
    SHELLEXECUTEINFOW sei{};
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_FLAG_NO_UI;
    sei.lpVerb = L"open";
    sei.lpFile = shortcutPath.c_str();
    sei.nShow = SW_SHOWNORMAL;
    ShellExecuteExW(&sei);
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
    if (self->m_selfHwnd) {
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