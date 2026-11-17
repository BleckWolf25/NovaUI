/**
- @file main.cpp
 *
- @version 1.1.0
- @author BleckWolf25
- @license MIT
 *
- @summary NovaUI entry point and blocking Win32 message loop
 *
- @description
- Enables per-monitor DPI awareness, initializes apartment-threaded COM for WIC icon decoding,
- starts the replacement taskbar and blocks in GetMessageW for 0% idle CPU usage.
 *
- @since 17/11/2026
- @updated 17/11/2026
 */
// ---------- IMPORTS
#include <Windows.h>
#include <objbase.h>
#include <memory>
#include "Modules/TaskbarUI.h"

// ---------- IMPLEMENTATION
int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

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