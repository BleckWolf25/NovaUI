/**
 * @file TaskbarPaint.h
 *
 * @version 1.0.0
 * @author BleckWolf25
 * @license MIT
 *
 * @summary Direct2D painting of the dock body, lenses, emblems, indicators and clock
 *
 * @description
 * Renders one dock frame from an immutable TaskbarPaintState: the shared layout, spans over the
 * current app and tray items, hover and drag state, and the clock strings with their cached
 * IDWriteTextFormat objects. Draws through Renderer::draw into the premultiplied composition
 * swap chain and returns its HRESULT so the caller can react to device loss. Allocates nothing
 * beyond the transient gradient brushes the renderer requires.
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