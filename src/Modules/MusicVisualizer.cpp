/**
 * @file MusicVisualizer.cpp
 *
 * @version 1.0.0
 * @author BleckWolf25
 * @license MIT
 *
 * @summary Implementation of the event-driven loopback level meter
 *
 * @description
 * Activates IAudioClient on the default render endpoint with AUDCLNT_STREAMFLAGS_LOOPBACK and
 * AUDCLNT_STREAMFLAGS_EVENTCALLBACK, then blocks in WaitForMultipleObjects on the audio event and
 * the stop event. Each packet is drained with IAudioCaptureClient, folded into band peaks and
 * published under a mutex; a post is emitted only when at least 125 ms have elapsed since the last
 * one. Configuration lives in the NovaUI roaming folder and is read once and written on change.
 *
 * @since 18/11/2026
 * @updated 18/11/2026
 */
// ---------- IMPORTS
#include "Modules/MusicVisualizer.h"
#include "Core/ShellPaths.h"
#include "Core/WinString.h"
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <wrl/client.h>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <string>
// ---------- CONSTANTS
namespace {
constexpr wchar_t kConfigFile[] = L"visualizer.txt";
constexpr ULONGLONG kPostIntervalMs = 125;
constexpr int kMinBars = 4;
constexpr int kMaxBars = 32;
}
// ---------- IMPLEMENTATION
MusicVisualizer::~MusicVisualizer() {
    stop();
}

void MusicVisualizer::loadConfig() {
    const std::wstring path = novaConfigFilePath(kConfigFile);
    if (path.empty()) {
        return;
    }
    std::ifstream in(path);
    if (!in.is_open()) {
        return;
    }
    int enabled = 0;
    int bars = m_barCount;
    float gain = m_gain;
    if (in >> enabled >> bars >> gain) {
        m_enabled.store(enabled != 0, std::memory_order_relaxed);
        m_barCount = std::clamp(bars, kMinBars, kMaxBars);
        m_gain = std::clamp(gain, 0.2f, 6.0f);
    }
}

void MusicVisualizer::saveConfig() const {
    const std::wstring path = novaConfigFilePath(kConfigFile);
    if (path.empty()) {
        return;
    }
    std::ofstream out(path, std::ios::trunc);
    if (!out.is_open()) {
        return;
    }
    out << (m_enabled.load(std::memory_order_relaxed) ? 1 : 0) << " " << m_barCount << " "
        << m_gain << "\n";
}

void MusicVisualizer::start(HWND target) {
    if (m_worker.joinable() || !target) {
        return;
    }
    m_target = target;
    UniqueHandle stopEvent(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    if (!stopEvent.valid()) {
        return;
    }
    m_stopEvent = std::move(stopEvent);
    m_worker = std::jthread([this](std::stop_token token) {
        try {
            run(token);
        } catch (...) {
        }
    });
}

void MusicVisualizer::stop() {
    if (m_worker.joinable()) {
        m_worker.request_stop();
        if (m_stopEvent.valid()) {
            SetEvent(m_stopEvent.get());
        }
        m_worker.join();
    }
    m_stopEvent.reset();
    m_target = nullptr;
    std::lock_guard<std::mutex> lock(m_mutex);
    m_shared.clear();
    m_levels.clear();
}

void MusicVisualizer::setEnabled(HWND target, bool enabled) {
    if (enabled) {
        m_enabled.store(true, std::memory_order_relaxed);
        start(target);
    } else {
        m_enabled.store(false, std::memory_order_relaxed);
        stop();
    }
    saveConfig();
}

void MusicVisualizer::setBarCount(int count) {
    m_barCount = std::clamp(count, kMinBars, kMaxBars);
    saveConfig();
}

void MusicVisualizer::setGain(float gain) {
    m_gain = std::clamp(gain, 0.2f, 6.0f);
    saveConfig();
}

void MusicVisualizer::pullLevels() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_levels = m_shared;
}

void MusicVisualizer::run(std::stop_token stopToken) {
    if (FAILED(CoInitializeEx(nullptr, COINIT_MULTITHREADED))) {
        return;
    }
    Microsoft::WRL::ComPtr<IMMDeviceEnumerator> enumerator;
    Microsoft::WRL::ComPtr<IMMDevice> device;
    Microsoft::WRL::ComPtr<IAudioClient> client;
    Microsoft::WRL::ComPtr<IAudioCaptureClient> capture;
    WAVEFORMATEX* mixFormat = nullptr;
    UniqueHandle audioEvent(CreateEventW(nullptr, FALSE, FALSE, nullptr));
    bool running = false;
    if (audioEvent.valid() &&
        SUCCEEDED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                   IID_PPV_ARGS(enumerator.GetAddressOf()))) &&
        SUCCEEDED(enumerator->GetDefaultAudioEndpoint(eRender, eConsole,
                                                      device.GetAddressOf())) &&
        SUCCEEDED(device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                                   IID_PPV_ARGS_Helper(client.GetAddressOf()))) &&
        SUCCEEDED(client->GetMixFormat(&mixFormat)) && mixFormat &&
        SUCCEEDED(client->Initialize(AUDCLNT_SHAREMODE_SHARED,
                                     AUDCLNT_STREAMFLAGS_LOOPBACK |
                                         AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
                                     0, 0, mixFormat, nullptr)) &&
        SUCCEEDED(client->SetEventHandle(audioEvent.get())) &&
        SUCCEEDED(client->GetService(IID_PPV_ARGS(capture.GetAddressOf()))) &&
        SUCCEEDED(client->Start())) {
        running = true;
    }
    if (!running) {
        if (mixFormat) {
            CoTaskMemFree(mixFormat);
        }
        CoUninitialize();
        return;
    }
    const bool isFloat = mixFormat->wFormatTag == WAVE_FORMAT_IEEE_FLOAT ||
                         (mixFormat->wFormatTag == WAVE_FORMAT_EXTENSIBLE &&
                          reinterpret_cast<WAVEFORMATEXTENSIBLE*>(mixFormat)->SubFormat ==
                              KSDATAFORMAT_SUBTYPE_IEEE_FLOAT);
    const UINT32 channels = mixFormat->nChannels;
    ULONGLONG lastPost = 0;
    const HANDLE waits[2] = {audioEvent.get(), m_stopEvent.get()};
    while (!stopToken.stop_requested()) {
        const DWORD result = WaitForMultipleObjects(2, waits, FALSE, INFINITE);
        if (result != WAIT_OBJECT_0) {
            break;
        }
        UINT32 packetFrames = 0;
        if (FAILED(capture->GetNextPacketSize(&packetFrames))) {
            break;
        }
        std::vector<float> bands(static_cast<size_t>(m_barCount), 0.0f);
        while (packetFrames > 0 && !stopToken.stop_requested()) {
            BYTE* data = nullptr;
            UINT32 frames = 0;
            DWORD flags = 0;
            if (FAILED(capture->GetBuffer(&data, &frames, &flags, nullptr, nullptr))) {
                break;
            }
            if (isFloat && channels > 0 && (flags & AUDCLNT_BUFFERFLAGS_SILENT) == 0) {
                const auto* samples = reinterpret_cast<const float*>(data);
                const size_t total = static_cast<size_t>(frames) * channels;
                const size_t perBand = std::max<size_t>(1, total / bands.size());
                for (size_t i = 0; i < total; ++i) {
                    const size_t band = std::min(bands.size() - 1, i / perBand);
                    bands[band] = std::max(bands[band], std::fabs(samples[i]));
                }
            }
            capture->ReleaseBuffer(frames);
            if (FAILED(capture->GetNextPacketSize(&packetFrames))) {
                packetFrames = 0;
            }
        }
        for (float& band : bands) {
            band = std::clamp(band * m_gain, 0.0f, 1.0f);
        }
        const ULONGLONG now = GetTickCount64();
        if (now - lastPost >= kPostIntervalMs && m_target) {
            lastPost = now;
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_shared = std::move(bands);
            }
            PostMessageW(m_target, WM_APP_VISUALIZER, 0, 0);
        }
    }
    client->Stop();
    CoTaskMemFree(mixFormat);
    CoUninitialize();
}