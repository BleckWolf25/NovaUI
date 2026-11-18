/**
 * @file TaskbarLayout.cpp
 *
 * @version 1.0.0
 * @author BleckWolf25
 * @license MIT
 *
 * @summary Implementation of dock slot geometry, hit resolution and drag target indexing
 *
 * @description
 * lays out the Start emblem, the optional Task View slot and the app run from the left edge,
 * then the clock, overflow chevron and promoted tray icons from the right edge, clamping the app
 * run to the space that remains. Hit resolution walks those same positions in priority order and
 * drag indexing converts a pointer X into a clamped destination slot.
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
    return appsStartX + static_cast<float>(index) * (kSlotSize + kSlotGap);
}

float TaskbarLayout::traySlotX(int index) const noexcept {
    return trayLeft + static_cast<float>(index) * (kTraySlotSize + kTraySlotGap);
}

TaskbarLayout computeTaskbarLayout(float widthDip, float heightDip, bool showTaskView,
                                   int appCount, int trayCount) {
    TaskbarLayout layout{};
    layout.showTaskView = showTaskView;
    layout.appCount = appCount;
    layout.trayCount = trayCount;
    layout.slotTop = (heightDip - kSlotSize) / 2.0f;
    layout.traySlotTop = (heightDip - kTraySlotSize) / 2.0f;
    layout.startLeft = kOuterPadX;
    float x = kOuterPadX + kSlotSize + kSectionGap + kDividerWidth + kSectionGap;
    layout.taskViewLeft = x;
    if (showTaskView) {
        x += kSlotSize + kSlotGap;
    }
    layout.appsStartX = x;
    layout.clockRight = widthDip - kOuterPadX;
    layout.clockLeft = layout.clockRight - kClockWidth;
    layout.traySectionRight = layout.clockLeft - kSectionGap - kDividerWidth - kSectionGap;
    layout.chevronLeft = layout.traySectionRight - kChevronWidth;
    const float trayTotalW =
        trayCount <= 0 ? 0.0f
                       : static_cast<float>(trayCount) * kTraySlotSize +
                             static_cast<float>(trayCount - 1) * kTraySlotGap + kTraySlotGap;
    layout.trayLeft = layout.chevronLeft - trayTotalW;
    layout.maxAppX = layout.trayLeft - kSectionGap - kSlotSize;
    return layout;
}

TaskbarHit hitTestTaskbar(const TaskbarLayout& layout, float dipX, float dipY) {
    if (dipY < layout.slotTop - 2.0f || dipY > layout.slotTop + kSlotSize + 2.0f) {
        return {};
    }
    if (dipX >= layout.startLeft && dipX <= layout.startLeft + kSlotSize) {
        return {TaskbarZone::Start, -1};
    }
    if (layout.showTaskView && dipX >= layout.taskViewLeft &&
        dipX <= layout.taskViewLeft + kSlotSize) {
        return {TaskbarZone::TaskView, -1};
    }
    if (dipX >= layout.clockLeft - 4.0f && dipX <= layout.clockRight + 4.0f) {
        return {TaskbarZone::Clock, -1};
    }
    if (dipX >= layout.chevronLeft && dipX <= layout.traySectionRight) {
        return {TaskbarZone::TrayOverflow, -1};
    }
    if (layout.trayCount > 0 && dipX >= layout.trayLeft && dipX < layout.chevronLeft) {
        for (int i = 0; i < layout.trayCount; ++i) {
            const float slotX = layout.traySlotX(i);
            if (dipX >= slotX && dipX <= slotX + kTraySlotSize) {
                return {TaskbarZone::Tray, i};
            }
        }
    }
    for (int i = 0; i < layout.appCount; ++i) {
        const float slotX = layout.appSlotX(i);
        if (slotX > layout.maxAppX) {
            break;
        }
        if (dipX >= slotX && dipX <= slotX + kSlotSize) {
            return {TaskbarZone::App, i};
        }
    }
    return {};
}

float taskbarSlotCenterX(const TaskbarLayout& layout, const TaskbarHit& hit) {
    switch (hit.zone) {
    case TaskbarZone::Start:
        return layout.startLeft + kSlotSize * 0.5f;
    case TaskbarZone::TaskView:
        return layout.taskViewLeft + kSlotSize * 0.5f;
    case TaskbarZone::App:
        if (hit.index >= 0) {
            return layout.appSlotX(hit.index) + kSlotSize * 0.5f;
        }
        break;
    case TaskbarZone::Clock:
        return (layout.clockLeft + layout.clockRight) * 0.5f;
    case TaskbarZone::TrayOverflow:
        return layout.chevronLeft + kChevronWidth * 0.5f;
    case TaskbarZone::Tray:
        if (hit.index >= 0) {
            return layout.traySlotX(hit.index) + kTraySlotSize * 0.5f;
        }
        break;
    default:
        break;
    }
    return layout.clockRight * 0.5f;
}

int taskbarDragTargetIndex(const TaskbarLayout& layout, TaskbarZone zone, float dipX,
                           int pinnedAppCount) {
    if (zone == TaskbarZone::App) {
        const int rawIndex = static_cast<int>(std::lround(
            (dipX - layout.appsStartX - kSlotSize * 0.5f) / (kSlotSize + kSlotGap)));
        return pinnedAppCount > 0 ? std::clamp(rawIndex, 0, pinnedAppCount - 1) : 0;
    }
    if (zone == TaskbarZone::Tray && layout.trayCount > 0) {
        const int rawIndex = static_cast<int>(std::lround(
            (dipX - layout.trayLeft - kTraySlotSize * 0.5f) / (kTraySlotSize + kTraySlotGap)));
        return std::clamp(rawIndex, 0, layout.trayCount - 1);
    }
    return -1;
}