/**
 * @file WindowIdentity.cpp
 *
 * @version 1.0.0
 * @author BleckWolf25
 * @license MIT
 *
 * @summary Implementation of window identity resolution and taskbar eligibility
 *
 * @description
 * Opens the owning process with PROCESS_QUERY_LIMITED_INFORMATION through UniqueHandle, reads
 * its image path with QueryFullProcessImageNameW and its packaged identity with
 * GetApplicationUserModelId. When the owner is ApplicationFrameHost, EnumChildWindows locates the
 * hosted process so the identity reflects the real app. Eligibility uses GetWindowLongPtrW,
 * GetWindow and DwmGetWindowAttribute with DWMWA_CLOAKED.
 *
 * @since 18/11/2026
 * @updated 18/11/2026
 */
// ---------- IMPORTS
#include "Modules/WindowIdentity.h"
#include "Core/WinResources.h"
#include "Core/WinString.h"
#include <appmodel.h>
#include <dwmapi.h>
#include <cstdint>
#include <utility>
// ---------- IMPLEMENTATION
WindowIdentity queryWindowIdentity(HWND hwnd) {
    WindowIdentity id{};
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == 0) {
        return id;
    }
    const auto queryProcess = [&id](DWORD targetPid) {
        const UniqueHandle process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, targetPid));
        if (!process.valid()) {
            return;
        }
        wchar_t exePath[MAX_PATH]{};
        DWORD size = MAX_PATH;
        if (QueryFullProcessImageNameW(process.get(), 0, exePath, &size)) {
            id.exePath = exePath;
            id.exePathLower = toLowerWide(id.exePath);
        }
        UINT32 aumidLength = 0;
        if (GetApplicationUserModelId(process.get(), &aumidLength, nullptr) ==
                ERROR_INSUFFICIENT_BUFFER &&
            aumidLength > 1) {
            std::wstring aumid(aumidLength, L'\0');
            if (GetApplicationUserModelId(process.get(), &aumidLength, aumid.data()) ==
                ERROR_SUCCESS) {
                if (!aumid.empty() && aumid.back() == L'\0') {
                    aumid.pop_back();
                }
                id.aumid = aumid;
                id.aumidLower = toLowerWide(std::move(aumid));
            }
        }
    };
    queryProcess(pid);
    if (id.exePathLower.ends_with(L"\\applicationframehost.exe")) {
        struct ChildContext {
            DWORD hostPid;
            DWORD innerPid;
        } context{pid, 0};
        EnumChildWindows(
            hwnd,
            [](HWND child, LPARAM lp) -> BOOL {
                auto* ctx = reinterpret_cast<ChildContext*>(lp);
                DWORD childPid = 0;
                GetWindowThreadProcessId(child, &childPid);
                if (childPid != 0 && childPid != ctx->hostPid) {
                    ctx->innerPid = childPid;
                    return FALSE;
                }
                return TRUE;
            },
            reinterpret_cast<LPARAM>(&context));
        if (context.innerPid != 0) {
            queryProcess(context.innerPid);
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

bool isTaskbarWindow(HWND hwnd, HWND selfHwnd) {
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
    if (SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked))) &&
        cloaked) {
        return false;
    }
    return GetWindowTextLengthW(hwnd) > 0;
}