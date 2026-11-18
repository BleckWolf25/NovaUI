/**
 * @file PinFolderWatcher.h
 *
 * @version 1.0.0
 * @author BleckWolf25
 * @license MIT
 *
 * @summary Background watcher that reports Explorer pin folder changes to the UI thread
 *
 * @description
 * Owns a std::jthread that blocks in WaitForMultipleObjects on a FindFirstChangeNotification
 * handle and a manual-reset stop event, so the worker consumes no CPU while the folder is quiet.
 * A detected change posts the caller's WM_APP message and re-arms the notification; nothing is
 * polled. Suppression lets the owner write its own shortcuts without reacting to them, and stop
 * requests cancellation through the stop_token before joining and releasing both handles.
 *
 * @since 18/11/2026
 * @updated 18/11/2026
 */
#pragma once
// ---------- IMPORTS
#include "Core/WinResources.h"
#include <Windows.h>
#include <atomic>
#include <stop_token>
#include <string>
#include <thread>
// ---------- DECLARATIONS
class PinFolderWatcher {
public:
    PinFolderWatcher() = default;
    ~PinFolderWatcher();
    PinFolderWatcher(const PinFolderWatcher&) = delete;
    PinFolderWatcher& operator=(const PinFolderWatcher&) = delete;
    void start(HWND target, UINT message, const std::wstring& directory);
    void stop();
    void setSuppressed(bool suppressed) noexcept;
private:
    void watch(std::stop_token stopToken, HANDLE changeHandle, HANDLE stopEvent);
    HWND m_target = nullptr;
    UINT m_message = 0;
    std::atomic<bool> m_suppressed{false};
    UniqueChangeNotification m_changeHandle;
    UniqueHandle m_stopEvent;
    std::jthread m_worker;
};