/**
 * @file TrayToolbarReader.cpp
 *
 * @version 1.0.0
 * @author BleckWolf25
 * @license MIT
 *
 * @summary Implementation of the classic Explorer tray toolbar reader
 *
 * @description
 * Locates TrayNotifyWnd's SysPager toolbar and the NotifyIconOverflowWindow toolbar, then reads
 * their buttons with TB_BUTTONCOUNT, TB_GETBUTTON and TB_GETBUTTONTEXTW against a buffer allocated
 * inside Explorer via VirtualAllocEx. TBBUTTON.dwData points at the shell's tray record holding
 * the owner window, identifier, callback message and icon handle. Process and remote buffers are
 * RAII-owned, hidden buttons are skipped and duplicate persistence keys are collapsed.
 *
 * @since 18/11/2026
 * @updated 18/11/2026
 */
// ---------- IMPORTS
#include "Modules/TrayToolbarReader.h"
#include "Core/WinResources.h"
#include "Core/WinString.h"
#include <CommCtrl.h>
#include <algorithm>
#include <unordered_set>
// ---------- CONSTANTS
namespace {
constexpr UINT kSendTimeoutMs = 50;
constexpr size_t kTipCapacity = 256;

struct TrayButtonData {
    HWND hwnd;
    UINT uID;
    UINT uCallbackMessage;
    DWORD reserved[2];
    HICON hIcon;
};

class RemoteBuffer {
public:
    RemoteBuffer(HANDLE process, SIZE_T bytes) : m_process(process) {
        m_address =
            VirtualAllocEx(process, nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    }
    ~RemoteBuffer() {
        if (m_address) {
            VirtualFreeEx(m_process, m_address, 0, MEM_RELEASE);
        }
    }
    RemoteBuffer(const RemoteBuffer&) = delete;
    RemoteBuffer& operator=(const RemoteBuffer&) = delete;
    BYTE* get() const noexcept { return static_cast<BYTE*>(m_address); }
    bool valid() const noexcept { return m_address != nullptr; }
private:
    HANDLE m_process;
    void* m_address = nullptr;
};

HWND findChildByClass(HWND parent, const wchar_t* className) {
    if (!parent) {
        return nullptr;
    }
    return FindWindowExW(parent, nullptr, className, nullptr);
}

std::vector<HWND> findTrayToolbars() {
    std::vector<HWND> toolbars;
    if (HWND shellTray = FindWindowW(L"Shell_TrayWnd", nullptr)) {
        const HWND trayNotify = findChildByClass(shellTray, L"TrayNotifyWnd");
        const HWND sysPager = findChildByClass(trayNotify, L"SysPager");
        if (HWND toolbar = findChildByClass(sysPager, L"ToolbarWindow32")) {
            toolbars.push_back(toolbar);
        } else if (HWND direct = findChildByClass(trayNotify, L"ToolbarWindow32")) {
            toolbars.push_back(direct);
        }
    }
    if (HWND overflow = FindWindowW(L"NotifyIconOverflowWindow", nullptr)) {
        if (HWND toolbar = findChildByClass(overflow, L"ToolbarWindow32")) {
            toolbars.push_back(toolbar);
        }
    }
    return toolbars;
}

std::wstring buildPersistKey(HWND ownerHwnd, UINT uID, const std::wstring& tooltip) {
    DWORD pid = 0;
    GetWindowThreadProcessId(ownerHwnd, &pid);
    if (pid != 0) {
        const UniqueHandle process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid));
        if (process.valid()) {
            wchar_t exePath[MAX_PATH]{};
            DWORD size = MAX_PATH;
            if (QueryFullProcessImageNameW(process.get(), 0, exePath, &size)) {
                return toLowerWide(exePath) + L"#" + std::to_wstring(uID);
            }
        }
    }
    return L"tip:" + toLowerWide(tooltip) + L"#" + std::to_wstring(uID);
}

void readToolbarInto(HWND toolbar, std::unordered_set<std::wstring>& seenKeys,
                     std::vector<RawTrayIcon>& out) {
    DWORD explorerPid = 0;
    GetWindowThreadProcessId(toolbar, &explorerPid);
    if (explorerPid == 0) {
        return;
    }
    const UniqueHandle process(OpenProcess(
        PROCESS_VM_OPERATION | PROCESS_VM_READ | PROCESS_VM_WRITE, FALSE, explorerPid));
    if (!process.valid()) {
        return;
    }
    constexpr SIZE_T kRemoteBytes =
        sizeof(TBBUTTON) + sizeof(TrayButtonData) + 512 * sizeof(wchar_t);
    const RemoteBuffer remote(process.get(), kRemoteBytes);
    if (!remote.valid()) {
        return;
    }
    BYTE* remoteButton = remote.get();
    BYTE* remoteText = remoteButton + sizeof(TBBUTTON) + sizeof(TrayButtonData);
    LRESULT countResult = 0;
    if (SendMessageTimeoutW(toolbar, TB_BUTTONCOUNT, 0, 0, SMTO_ABORTIFHUNG, kSendTimeoutMs,
                            reinterpret_cast<PDWORD_PTR>(&countResult)) == 0) {
        return;
    }
    const int count = static_cast<int>(countResult);
    for (int i = 0; i < count; ++i) {
        LRESULT accepted = 0;
        if (SendMessageTimeoutW(toolbar, TB_GETBUTTON, static_cast<WPARAM>(i),
                                reinterpret_cast<LPARAM>(remoteButton), SMTO_ABORTIFHUNG,
                                kSendTimeoutMs,
                                reinterpret_cast<PDWORD_PTR>(&accepted)) == 0 ||
            accepted == 0) {
            continue;
        }
        TBBUTTON button{};
        SIZE_T bytesRead = 0;
        if (!ReadProcessMemory(process.get(), remoteButton, &button, sizeof(button), &bytesRead) ||
            bytesRead != sizeof(button) || button.dwData == 0) {
            continue;
        }
        if ((button.fsState & TBSTATE_HIDDEN) != 0) {
            continue;
        }
        TrayButtonData trayData{};
        if (!ReadProcessMemory(process.get(), reinterpret_cast<LPCVOID>(button.dwData), &trayData,
                               sizeof(trayData), &bytesRead) ||
            bytesRead != sizeof(trayData)) {
            continue;
        }
        if (!trayData.hwnd || !IsWindow(trayData.hwnd)) {
            continue;
        }
        wchar_t tipBuffer[kTipCapacity]{};
        LRESULT textLength = 0;
        if (SendMessageTimeoutW(toolbar, TB_GETBUTTONTEXTW,
                                static_cast<WPARAM>(button.idCommand),
                                reinterpret_cast<LPARAM>(remoteText), SMTO_ABORTIFHUNG,
                                kSendTimeoutMs,
                                reinterpret_cast<PDWORD_PTR>(&textLength)) != 0 &&
            textLength > 0) {
            const size_t charsToRead =
                std::min<size_t>(static_cast<size_t>(textLength), kTipCapacity - 1);
            ReadProcessMemory(process.get(), remoteText, tipBuffer,
                              charsToRead * sizeof(wchar_t), &bytesRead);
        }
        RawTrayIcon raw{};
        raw.source = TrayIconSource::Toolbar;
        raw.ownerHwnd = trayData.hwnd;
        raw.uID = trayData.uID;
        raw.uCallbackMessage = trayData.uCallbackMessage;
        raw.hIcon = trayData.hIcon;
        raw.tooltip = tipBuffer[0] != L'\0' ? tipBuffer : L"System Tray App";
        raw.persistKey = buildPersistKey(raw.ownerHwnd, raw.uID, raw.tooltip);
        if (seenKeys.insert(raw.persistKey).second) {
            out.push_back(std::move(raw));
        }
    }
}

std::vector<RawTrayIcon> readToolbarIcons() {
    std::vector<RawTrayIcon> icons;
    std::unordered_set<std::wstring> seenKeys;
    for (HWND toolbar : findTrayToolbars()) {
        readToolbarInto(toolbar, seenKeys, icons);
    }
    return icons;
}
}
// ---------- IMPLEMENTATION
std::vector<RawTrayIcon> readTrayIcons() {
    return readToolbarIcons();
}