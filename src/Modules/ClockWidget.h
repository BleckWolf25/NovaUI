/**
 * @file ClockWidget.h
 *
 * @version 1.0.0
 * @author BleckWolf25
 * @license MIT
 *
 * @summary Minute-aligned clock model that formats the time and date strings
 *
 * @description
 * Formats locale-aware time and date text with GetTimeFormatEx and GetDateFormatEx and keeps a
 * single one-shot timer aligned to the next minute boundary. The owner window is invalidated
 * only when the text changes, so the process wakes once per minute and sleeps otherwise.
 *
 * @since 17/11/2026
 * @updated 17/11/2026
 */
#pragma once
// ---------- IMPORTS
#include <Windows.h>

#include <string>

// ---------- DECLARATIONS
class ClockWidget {
public:
    void start(HWND owner);
    void stop();
    void refresh();
    bool onTimer(WPARAM timerId);

    const std::wstring& time() const noexcept { return m_time; }
    const std::wstring& date() const noexcept { return m_date; }

private:
    HWND m_owner = nullptr;
    std::wstring m_time;
    std::wstring m_date;
};
