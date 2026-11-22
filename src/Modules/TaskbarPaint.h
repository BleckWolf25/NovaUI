/**
 * @file TaskbarPaint.h
 *
 * @version 1.1.0
 * @author BleckWolf25
 * @license MIT
 *
 * @summary Direct2D painting of the dock body, shell glyphs, lenses, indicators and clock
 *
 * @description
 * Renders one dock frame from an immutable TaskbarPaintState: the shared layout carrying the
 * Windows settings and scaled metrics, spans over the app, tray and visualizer data, hover and
 * drag state, and the clock strings with their cached IDWriteTextFormat objects. Badge and
 * attention rendering are gated by the user's taskbar settings. Returns the renderer HRESULT so
 * the caller can react to device loss.
 *
 * @since 18/11/2026
 * @updated 18/11/2026
 */
#pragma once
// ---------- IMPORTS
#include "Modules/TaskTracker.h"
#include "Modules/TaskbarLayout.h"
#include "Modules/TrayTracker.h"
#include "Render/Renderer.h"
#include <span>
#include <string_view>
// ---------- TYPES
struct TaskbarPaintState {
    TaskbarLayout layout{};
    std::span<const TaskItem> apps;
    std::span<const TrayItem> tray;
    std::span<const float> visualizerLevels;
    TaskbarHit hover{};
    bool trayOverflowOpen = false;
    bool dragging = false;
    TaskbarZone dragZone = TaskbarZone::None;
    int dragFromIndex = -1;
    int dragToIndex = -1;
    bool dragWasPinned = false;
    float dragDeltaDip = 0.0f;
    std::wstring_view time;
    std::wstring_view date;
    IDWriteTextFormat* timeFormat = nullptr;
    IDWriteTextFormat* dateFormat = nullptr;
};
// ---------- DECLARATIONS
HRESULT paintTaskbar(Renderer& renderer, const TaskbarPaintState& state);