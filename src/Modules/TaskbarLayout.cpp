/**
 * @file TaskbarLayout.cpp
 *
 * @version 1.1.0
 * @author BleckWolf25
 * @license MIT
 *
 * @summary Implementation of scaled dock geometry, hit resolution and drag target indexing
 *
 * @description
 * Scales the base metric set by the Windows size index, walks the enabled left-hand shell slots,
 * then reserves the clock, chevron, promoted tray icons and visualizer strip from the right edge.
 * The remaining span hosts the app run, centred when the alignment setting says centre. Hit
 * resolution walks the same positions in priority order and drag indexing converts a pointer X
 * into a clamped destination slot.
 *
 * @since 18/11/2026
 * @updated 18/11/2026
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

TaskbarMetrics metricsForSize(int sizeIndex) {
    TaskbarMetrics metrics{};
    const float scale = sizeIndex == 0 ? 0.85f : (sizeIndex == 2 ? 1.15f : 1.0f);
    metrics.outerPadX *= scale;
    metrics.slotSize *= scale;
    metrics.slotGap *= scale;
    metrics.slotRadius *= scale;
    metrics.iconSize *= scale;
    metrics.traySlotSize *= scale;
    metrics.traySlotGap *= scale;
    metrics.trayIconSize *= scale;
    metrics.chevronWidth *= scale;
    metrics.sectionGap *= scale;
    metrics.clockWidth *= scale;
    metrics.thickness *= scale;
    return metrics;
}

TaskbarLayout computeTaskbarLayout(float widthDip, const TaskbarSettings& settings, int appCount,
                                   int trayCount, bool showVisualizer) {
    TaskbarLayout layout{};
    layout.m = metricsForSize(settings.sizeIndex);
    layout.settings = settings;
    layout.appCount = appCount;
    layout.trayCount = trayCount;
    layout.showVisualizer = showVisualizer;
    const TaskbarMetrics& m = layout.m;
    layout.slotTop = (m.thickness - m.slotSize) * 0.5f;
    layout.traySlotTop = (m.thickness - m.traySlotSize) * 0.5f;
    layout.startLeft = m.outerPadX;
    float x = m.outerPadX + m.slotSize;
    const auto addShellSlot = [&](bool enabled, float& slotLeft) {
        if (!enabled) {
            slotLeft = -1.0f;
            return;
        }
        x += m.slotGap;
        slotLeft = x;
        x += m.slotSize;
    };
    addShellSlot(settings.showTaskView, layout.taskViewLeft);
    addShellSlot(settings.showSearch, layout.searchLeft);
    addShellSlot(settings.showWidgets, layout.widgetsLeft);
    addShellSlot(settings.showResume, layout.resumeLeft);
    x += m.sectionGap + m.dividerWidth + m.sectionGap;
    const float appsBegin = x;
    layout.clockRight = widthDip - m.outerPadX;
    layout.clockLeft = layout.clockRight - m.clockWidth;
    layout.traySectionRight = layout.clockLeft - m.sectionGap - m.dividerWidth - m.sectionGap;
    layout.chevronLeft = layout.traySectionRight - m.chevronWidth;
    const float trayTotalW =
        trayCount <= 0 ? 0.0f
                       : static_cast<float>(trayCount) * m.traySlotSize +
                             static_cast<float>(trayCount - 1) * m.traySlotGap + m.traySlotGap;
    layout.trayLeft = layout.chevronLeft - trayTotalW;
    float appsEnd = layout.trayLeft - m.sectionGap;
    if (showVisualizer) {
        const float stripWidth = std::min(132.0f, std::max(0.0f, appsEnd - appsBegin - m.slotSize));
        layout.visualizerRight = appsEnd;
        layout.visualizerLeft = appsEnd - stripWidth;
        appsEnd = layout.visualizerLeft - m.sectionGap;
    } else {
        layout.visualizerLeft = 0.0f;
        layout.visualizerRight = 0.0f;
    }
    const float appRunWidth =
        appCount <= 0 ? 0.0f
                      : static_cast<float>(appCount) * m.slotSize +
                            static_cast<float>(appCount - 1) * m.slotGap;
    const float span = std::max(0.0f, appsEnd - appsBegin);
    layout.appsStartX = appsBegin;
    if (settings.alignment == TaskbarAlignment::Center && appRunWidth < span) {
        layout.appsStartX = appsBegin + (span - appRunWidth) * 0.5f;
    }
    layout.maxAppX = appsEnd - m.slotSize;
    return layout;
}

TaskbarHit hitTestTaskbar(const TaskbarLayout& layout, float dipX, float dipY) {
    const TaskbarMetrics& m = layout.m;
    if (dipY < layout.slotTop - 2.0f || dipY > layout.slotTop + m.slotSize + 2.0f) {
        return {};
    }
    const auto inSlot = [&](float left) {
        return left >= 0.0f && dipX >= left && dipX <= left + m.slotSize;
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
    if (dipX >= layout.clockLeft - 4.0f && dipX <= layout.clockRight + 4.0f) {
        return {TaskbarZone::Clock, -1};
    }
    if (dipX >= layout.chevronLeft && dipX <= layout.traySectionRight) {
        return {TaskbarZone::TrayOverflow, -1};
    }
    if (layout.showVisualizer && dipX >= layout.visualizerLeft &&
        dipX <= layout.visualizerRight) {
        return {TaskbarZone::Visualizer, -1};
    }
    if (layout.trayCount > 0 && dipX >= layout.trayLeft && dipX < layout.chevronLeft) {
        for (int i = 0; i < layout.trayCount; ++i) {
            const float slotX = layout.traySlotX(i);
            if (dipX >= slotX && dipX <= slotX + m.traySlotSize) {
                return {TaskbarZone::Tray, i};
            }
        }
    }
    for (int i = 0; i < layout.appCount; ++i) {
        const float slotX = layout.appSlotX(i);
        if (slotX > layout.maxAppX) {
            break;
        }
        if (dipX >= slotX && dipX <= slotX + m.slotSize) {
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
    return layout.clockRight * 0.5f;
}

int taskbarDragTargetIndex(const TaskbarLayout& layout, TaskbarZone zone, float dipX,
                           int pinnedAppCount) {
    const TaskbarMetrics& m = layout.m;
    if (zone == TaskbarZone::App) {
        const int rawIndex = static_cast<int>(std::lround(
            (dipX - layout.appsStartX - m.slotSize * 0.5f) / (m.slotSize + m.slotGap)));
        return pinnedAppCount > 0 ? std::clamp(rawIndex, 0, pinnedAppCount - 1) : 0;
    }
    if (zone == TaskbarZone::Tray && layout.trayCount > 0) {
        const int rawIndex = static_cast<int>(std::lround(
            (dipX - layout.trayLeft - m.traySlotSize * 0.5f) / (m.traySlotSize + m.traySlotGap)));
        return std::clamp(rawIndex, 0, layout.trayCount - 1);
    }
    return -1;
}