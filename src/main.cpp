/**
 * @file main.cpp
 *
 * @version 1.2.0
 * @author BleckWolf25
 * @license MIT
 *
 * @summary NovaUI entry point, restore-guard dispatch and blocking Win32 message loop
 *
 * @description
 * Enables per-monitor DPI awareness, then either runs the detached preference-restore guard when
 * launched with a guard argument or starts the replacement taskbar. The shell path takes the
 * single-instance mutex, initializes apartment-threaded COM for WIC and UI Automation, and blocks
 * in GetMessageW so idle CPU stays at zero.
 *
 * @since 17/11/2026
 * @updated 20/11/2026
 */
// ---------- IMPORTS
#include "Core/ShellRestore.h"
#include "Modules/TaskbarUI.h"
#include <Windows.h>
#include <cwchar>
#include <memory>
// ---------- CONSTANTS
namespace {
constexpr wchar_t kGuardPrefix[] = L"--guard ";
constexpr size_t kGuardPrefixLength = 8;
}
// ---------- IMPLEMENTATION
int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR cmdLine, int) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    if (cmdLine && std::wcsncmp(cmdLine, kGuardPrefix, kGuardPrefixLength) == 0) {
        const unsigned long pid = std::wcstoul(cmdLine + kGuardPrefixLength, nullptr, 10);
        return ShellRestore::guardMain(static_cast<DWORD>(pid));
    }
    HANDLE mutex = CreateMutexW(nullptr, FALSE, L"Local\\NovaUI.Instance");
    const bool alreadyRunning = GetLastError() == ERROR_ALREADY_EXISTS;
    const std::unique_ptr<void, decltype(&CloseHandle)> instance(mutex, &CloseHandle);
    if (!mutex || alreadyRunning) {
        return 0;
    }
    const HRESULT comHr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    int exitCode = 0;
    {
        TaskbarUI taskbar;
        if (!taskbar.start()) {
            if (SUCCEEDED(comHr)) {
                CoUninitialize();
            }
            return 1;
        }
        MSG msg{};
        while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        exitCode = static_cast<int>(msg.wParam);
    }
    if (SUCCEEDED(comHr)) {
        CoUninitialize();
    }
    return exitCode;
}