/**
 * @file TaskbarLayout.cpp
 *
 * @version 1.2.0
 * @author BleckWolf25
 * @license MIT
 *
 * @summary Implementation of axis-mapped dock geometry, hit resolution and drag indexing
 *
 * @description
 * Scales the base metric set by the Windows size index and selects thickness from the edge
 * orientation. Walks the enabled shell slots from the start of the long axis, reserves the clock,
 * chevron, promoted tray icons and visualizer strip from the far end, then centres or start-aligns
 * the app run in the remaining span. band() converts logical extents into upright rectangles and
 * mirrors the cross axis when the dock sits on the right or bottom edge.
 *
 * @since 18/11/2026
 * @updated 20/11/2026
 */
// ---------- IMPORTS
#include "Modules/TaskbarLayout.h"
#include <algorithm>
#include <cmath>
// ---------- IMPLEMENTATION
float TaskbarLayout::appSlotX(int index) const noexcept {
    return appsStartX + static_cast<float>(index) * (m.slotSize + m.slotGap);
}

float TaskbarLayout::traySlotX(int index) const noexcept {
    return trayLeft + static_cast<float>(index) * (m.traySlotSize + m.traySlotGap);
}

D2D1_RECT_F TaskbarLayout::band(float alongBegin, float alongEnd, float crossBegin,
                                float crossEnd) const noexcept {
    float c0 = crossBegin;
    float c1 = crossEnd;
    if (mirrorCross) {
        c0 = cross - crossEnd;
        c1 = cross - crossBegin;
    }
    if (vertical) {
        return D2D1::RectF(c0, alongBegin, c1, alongEnd);
    }
    return D2D1::RectF(alongBegin, c0, alongEnd, c1);
}

D2D1_RECT_F TaskbarLayout::slot(float along, float size) const noexcept {
    const float inset = (cross - size) * 0.5f;
    return band(along, along + size, inset, inset + size);
}

float TaskbarLayout::alongOf(float dipX, float dipY) const noexcept {
    return vertical ? dipY : dipX;
}

float TaskbarLayout::crossOf(float dipX, float dipY) const noexcept {
    const float value = vertical ? dipX : dipY;
    return mirrorCross ? cross - value : value;
}

TaskbarMetrics metricsForSize(int sizeIndex) {
    TaskbarMetrics metrics{};
    const float scale = sizeIndex == 0 ? 0.85f : (sizeIndex == 2 ? 1.15f : 1.0f);
    metrics.outerPad *= scale;
    metrics.slotSize *= scale;
    metrics.slotGap *= scale;
    metrics.slotRadius *= scale;
    metrics.iconSize *= scale;
    metrics.traySlotSize *= scale;
    metrics.traySlotGap *= scale;
    metrics.trayIconSize *= scale;
    metrics.chevronWidth *= scale;
    metrics.sectionGap *= scale;
    metrics.clockLength *= scale;
    metrics.thickness *= scale;
    metrics.verticalThickness *= scale;
    return metrics;
}

TaskbarLayout computeTaskbarLayout(float lengthDip, const TaskbarSettings& settings, int appCount,
                                   int trayCount, bool showVisualizer) {
    TaskbarLayout layout{};
    layout.m = metricsForSize(settings.sizeIndex);
    layout.settings = settings;
    layout.vertical = settings.vertical();
    layout.mirrorCross =
        settings.edge == TaskbarEdge::Right || settings.edge == TaskbarEdge::Bottom;
    layout.length = lengthDip;
    layout.cross = layout.vertical ? layout.m.verticalThickness : layout.m.thickness;
    layout.appCount = appCount;
    layout.trayCount = trayCount;
    layout.showVisualizer = showVisualizer;
    const TaskbarMetrics& m = layout.m;
    layout.slotTop = (layout.cross - m.slotSize) * 0.5f;
    layout.traySlotTop = (layout.cross - m.traySlotSize) * 0.5f;
    layout.startLeft = m.outerPad;
    float along = m.outerPad + m.slotSize;
    const auto addShellSlot = [&](bool enabled, float& slotLeft) {
        if (!enabled) {
            slotLeft = -1.0f;
            return;
        }
        along += m.slotGap;
        slotLeft = along;
        along += m.slotSize;
    };
    addShellSlot(settings.showTaskView, layout.taskViewLeft);
    addShellSlot(settings.showSearch, layout.searchLeft);
    addShellSlot(settings.showWidgets, layout.widgetsLeft);
    addShellSlot(settings.showResume, layout.resumeLeft);
    along += m.sectionGap + m.dividerWidth + m.sectionGap;
    const float appsBegin = along;
    layout.clockRight = lengthDip - m.outerPad;
    layout.clockLeft = layout.clockRight - m.clockLength;
    layout.traySectionRight = layout.clockLeft - m.sectionGap - m.dividerWidth - m.sectionGap;
    layout.chevronLeft = layout.traySectionRight - m.chevronWidth;
    const float trayTotal =
        trayCount <= 0 ? 0.0f
                       : static_cast<float>(trayCount) * m.traySlotSize +
                             static_cast<float>(trayCount - 1) * m.traySlotGap + m.traySlotGap;
    layout.trayLeft = layout.chevronLeft - trayTotal;
    float appsEnd = layout.trayLeft - m.sectionGap;
    if (showVisualizer) {
        const float strip = std::min(132.0f, std::max(0.0f, appsEnd - appsBegin - m.slotSize));
        layout.visualizerRight = appsEnd;
        layout.visualizerLeft = appsEnd - strip;
        appsEnd = layout.visualizerLeft - m.sectionGap;
    }
    const float appRun = appCount <= 0 ? 0.0f
                                       : static_cast<float>(appCount) * m.slotSize +
                                             static_cast<float>(appCount - 1) * m.slotGap;
    const float span = std::max(0.0f, appsEnd - appsBegin);
    layout.appsStartX = appsBegin;
    if (settings.alignment == TaskbarAlignment::Center && appRun < span) {
        layout.appsStartX = appsBegin + (span - appRun) * 0.5f;
    }
    layout.maxAppX = appsEnd - m.slotSize;
    return layout;
}

TaskbarHit hitTestTaskbar(const TaskbarLayout& layout, float dipX, float dipY) {
    const TaskbarMetrics& m = layout.m;
    const float along = layout.alongOf(dipX, dipY);
    const float cross = layout.crossOf(dipX, dipY);
    if (cross < layout.slotTop - 2.0f || cross > layout.slotTop + m.slotSize + 2.0f) {
        return {};
    }
    const auto inSlot = [&](float left) {
        return left >= 0.0f && along >= left && along <= left + m.slotSize;
    };
    if (inSlot(layout.startLeft)) {
        return {TaskbarZone::Start, -1};
    }
    if (inSlot(layout.taskViewLeft)) {
        return {TaskbarZone::TaskView, -1};
    }
    if (inSlot(layout.searchLeft)) {
        return {TaskbarZone::Search, -1};
    }
    if (inSlot(layout.widgetsLeft)) {
        return {TaskbarZone::Widgets, -1};
    }
    if (inSlot(layout.resumeLeft)) {
        return {TaskbarZone::Resume, -1};
    }
    if (along >= layout.clockLeft - 4.0f && along <= layout.clockRight + 4.0f) {
        return {TaskbarZone::Clock, -1};
    }
    if (along >= layout.chevronLeft && along <= layout.traySectionRight) {
        return {TaskbarZone::TrayOverflow, -1};
    }
    if (layout.showVisualizer && along >= layout.visualizerLeft &&
        along <= layout.visualizerRight) {
        return {TaskbarZone::Visualizer, -1};
    }
    if (layout.trayCount > 0 && along >= layout.trayLeft && along < layout.chevronLeft) {
        for (int i = 0; i < layout.trayCount; ++i) {
            const float slotAlong = layout.traySlotX(i);
            if (along >= slotAlong && along <= slotAlong + m.traySlotSize) {
                return {TaskbarZone::Tray, i};
            }
        }
    }
    for (int i = 0; i < layout.appCount; ++i) {
        const float slotAlong = layout.appSlotX(i);
        if (slotAlong > layout.maxAppX) {
            break;
        }
        if (along >= slotAlong && along <= slotAlong + m.slotSize) {
            return {TaskbarZone::App, i};
        }
    }
    return {};
}

float taskbarSlotCenterX(const TaskbarLayout& layout, const TaskbarHit& hit) {
    const TaskbarMetrics& m = layout.m;
    switch (hit.zone) {
    case TaskbarZone::Start:
        return layout.startLeft + m.slotSize * 0.5f;
    case TaskbarZone::TaskView:
        return layout.taskViewLeft + m.slotSize * 0.5f;
    case TaskbarZone::Search:
        return layout.searchLeft + m.slotSize * 0.5f;
    case TaskbarZone::Widgets:
        return layout.widgetsLeft + m.slotSize * 0.5f;
    case TaskbarZone::Resume:
        return layout.resumeLeft + m.slotSize * 0.5f;
    case TaskbarZone::App:
        if (hit.index >= 0) {
            return layout.appSlotX(hit.index) + m.slotSize * 0.5f;
        }
        break;
    case TaskbarZone::Clock:
        return (layout.clockLeft + layout.clockRight) * 0.5f;
    case TaskbarZone::TrayOverflow:
        return layout.chevronLeft + m.chevronWidth * 0.5f;
    case TaskbarZone::Visualizer:
        return (layout.visualizerLeft + layout.visualizerRight) * 0.5f;
    case TaskbarZone::Tray:
        if (hit.index >= 0) {
            return layout.traySlotX(hit.index) + m.traySlotSize * 0.5f;
        }
        break;
    default:
        break;
    }
    return layout.length * 0.5f;
}

int taskbarDragTargetIndex(const TaskbarLayout& layout, TaskbarZone zone, float dipAlong,
                           int pinnedAppCount) {
    const TaskbarMetrics& m = layout.m;
    if (zone == TaskbarZone::App) {
        const int raw = static_cast<int>(std::lround(
            (dipAlong - layout.appsStartX - m.slotSize * 0.5f) / (m.slotSize + m.slotGap)));
        return pinnedAppCount > 0 ? std::clamp(raw, 0, pinnedAppCount - 1) : 0;
    }
    if (zone == TaskbarZone::Tray && layout.trayCount > 0) {
        const int raw = static_cast<int>(std::lround(
            (dipAlong - layout.trayLeft - m.traySlotSize * 0.5f) /
            (m.traySlotSize + m.traySlotGap)));
        return std::clamp(raw, 0, layout.trayCount - 1);
    }
    return -1;
}