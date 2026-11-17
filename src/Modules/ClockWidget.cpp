/**
 * @file ClockWidget.cpp
 *
 * @version 1.0.0
 * @author BleckWolf25
 * @license MIT
 *
 * @summary Implementation of the minute-aligned clock model
 *
 * @description
 * Each refresh reformats the strings, re-arms the timer for the remaining milliseconds of the
 * current minute and invalidates the owner window. Re-arming with the same timer id replaces the
 * pending timer, so a system clock change simply calls refresh to realign.
 *
 * @since 17/11/2026
 * @updated 17/11/2026
 */
// ---------- IMPORTS
#include "Modules/ClockWidget.h"

#include <iterator>

// ---------- CONSTANTS
namespace {
constexpr UINT_PTR kTimerId = 1;
constexpr UINT kMinuteMs = 60'000;
constexpr UINT kBoundarySlackMs = 50;
}

// ---------- IMPLEMENTATION
void ClockWidget::start(HWND owner) {
    m_owner = owner;
    refresh();
}

void ClockWidget::stop() {
    if (m_owner) {
        KillTimer(m_owner, kTimerId);
    }
}

void ClockWidget::refresh() {
    wchar_t buffer[64]{};
    const int capacity = static_cast<int>(std::size(buffer));

    if (GetTimeFormatEx(LOCALE_NAME_USER_DEFAULT, TIME_NOSECONDS, nullptr, nullptr, buffer,
                        capacity) > 0) {
        m_time = buffer;
    }
    if (GetDateFormatEx(LOCALE_NAME_USER_DEFAULT, 0, nullptr, L"ddd, d MMM", buffer, capacity,
                        nullptr) > 0) {
        m_date = buffer;
    }

    SYSTEMTIME now{};
    GetLocalTime(&now);
    const UINT elapsed = static_cast<UINT>(now.wSecond) * 1000u + now.wMilliseconds;
    SetTimer(m_owner, kTimerId, kMinuteMs - elapsed + kBoundarySlackMs, nullptr);

    InvalidateRect(m_owner, nullptr, FALSE);
}

bool ClockWidget::onTimer(WPARAM timerId) {
    if (timerId != kTimerId) {
        return false;
    }
    refresh();
    return true;
}
