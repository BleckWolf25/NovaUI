/**
 * @file MusicVisualizer.h
 *
 * @version 1.0.0
 * @author BleckWolf25
 * @license MIT
 *
 * @summary Toggleable WASAPI loopback level meter for the dock
 *
 * @description
 * Captures the default render endpoint in loopback mode with an event-driven IAudioClient on a
 * std::jthread, reduces each packet to a small set of band magnitudes and posts WM_APP_VISUALIZER
 * at most eight times per second. The UI thread copies the shared bands under a mutex and
 * invalidates once. Stopping requests cancellation through the stop_token and joins the worker, so
 * a disabled visualizer leaves no thread, no audio client and no wake-ups behind.
 *
 * @since 18/11/2026
 * @updated 18/11/2026
 */
#pragma once
// ---------- IMPORTS
#include "Core/WinResources.h"
#include <Windows.h>
#include <atomic>
#include <mutex>
#include <stop_token>
#include <thread>
#include <vector>
// ---------- CONSTANTS
inline constexpr UINT WM_APP_VISUALIZER = WM_APP + 5;
// ---------- DECLARATIONS
class MusicVisualizer {
public:
    MusicVisualizer() = default;
    ~MusicVisualizer();
    MusicVisualizer(const MusicVisualizer&) = delete;
    MusicVisualizer& operator=(const MusicVisualizer&) = delete;
    void loadConfig();
    void saveConfig() const;
    void start(HWND target);
    void stop();
    void setEnabled(HWND target, bool enabled);
    void setBarCount(int count);
    void setGain(float gain);
    bool enabled() const noexcept { return m_enabled.load(std::memory_order_relaxed); }
    int barCount() const noexcept { return m_barCount; }
    float gain() const noexcept { return m_gain; }
    void pullLevels();
    const std::vector<float>& levels() const noexcept { return m_levels; }
private:
    void run(std::stop_token stopToken);
    HWND m_target = nullptr;
    std::atomic<bool> m_enabled{false};
    int m_barCount = 12;
    float m_gain = 1.6f;
    std::mutex m_mutex;
    std::vector<float> m_shared;
    std::vector<float> m_levels;
    UniqueHandle m_stopEvent;
    std::jthread m_worker;
};