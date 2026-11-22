/**
 * @file PinFolderWatcher.cpp
 *
 * @version 1.0.0
 * @author BleckWolf25
 * @license MIT
 *
 * @summary Implementation of the pin folder change watcher
 *
 * @description
 * Arms FindFirstChangeNotificationW for file-name and last-write changes, then runs a jthread
 * that waits on the notification and the stop event together. The loop exits on stop request or
 * when re-arming fails, and the thread body catches everything so no exception escapes a thread
 * entry point. Handles are released only after the worker joins.
 *
 * @since 18/11/2026
 * @updated 18/11/2026
 */
// ---------- IMPORTS
#include "Modules/PinFolderWatcher.h"
// ---------- IMPLEMENTATION
PinFolderWatcher::~PinFolderWatcher() {
    stop();
}

void PinFolderWatcher::start(HWND target, UINT message, const std::wstring& directory) {
    if (m_worker.joinable() || !target || directory.empty()) {
        return;
    }
    UniqueChangeNotification change(FindFirstChangeNotificationW(
        directory.c_str(), FALSE, FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE));
    if (!change.valid()) {
        return;
    }
    UniqueHandle stopEvent(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    if (!stopEvent.valid()) {
        return;
    }
    m_target = target;
    m_message = message;
    m_changeHandle = std::move(change);
    m_stopEvent = std::move(stopEvent);
    m_worker = std::jthread([this](std::stop_token token) {
        try {
            watch(token, m_changeHandle.get(), m_stopEvent.get());
        } catch (...) {
        }
    });
}

void PinFolderWatcher::stop() {
    if (m_worker.joinable()) {
        m_worker.request_stop();
        if (m_stopEvent.valid()) {
            SetEvent(m_stopEvent.get());
        }
        m_worker.join();
    }
    m_changeHandle.reset();
    m_stopEvent.reset();
    m_target = nullptr;
    m_message = 0;
}

void PinFolderWatcher::setSuppressed(bool suppressed) noexcept {
    m_suppressed.store(suppressed, std::memory_order_relaxed);
}

void PinFolderWatcher::watch(std::stop_token stopToken, HANDLE changeHandle, HANDLE stopEvent) {
    const HANDLE waits[2] = {changeHandle, stopEvent};
    while (!stopToken.stop_requested()) {
        const DWORD result = WaitForMultipleObjects(2, waits, FALSE, INFINITE);
        if (result != WAIT_OBJECT_0) {
            return;
        }
        if (!m_suppressed.load(std::memory_order_relaxed) && m_target) {
            PostMessageW(m_target, m_message, 0, 0);
        }
        if (!FindNextChangeNotification(changeHandle)) {
            return;
        }
    }
}