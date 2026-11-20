/**
 * @file TaskbarLayout.h
 *
 * @version 1.1.0
 * @author BleckWolf25
 * @license MIT
 *
 * @summary Single source of truth for dock metrics, slot geometry, hit-testing and drag indexing
 *
 * @description
 * Derives scaled metrics from the Windows taskbar size setting and lays out the Start emblem, the
 * optional Task View, search, widgets and Resume slots, the app run, the visualizer strip and the
 * tray and clock cluster. App slots centre or left-align according to the Windows alignment
 * setting. Hit-testing, flyout anchoring, drag indexing and painting all consume this one
 * structure so a drawn slot is always a clickable slot.
 *
 * @since 18/11/2026
 * @updated 18/11/2026
 */
#pragma once
// ---------- IMPORTS
#include "Core/TaskbarSettings.h"
// ---------- TYPES
enum class TaskbarZone {
    None, Start, TaskView, Search, Widgets, Resume, App, Tray, TrayOverflow, Clock, Visualizer
};

struct TaskbarHit {
    TaskbarZone zone = TaskbarZone::None;
    int index = -1;
    bool operator==(const TaskbarHit& other) const noexcept {
        return zone == other.zone && index == other.index;
    }
    bool operator!=(const TaskbarHit& other) const noexcept { return !(*this == other); }
};

struct TaskbarMetrics {
    float outerPadX = 12.0f;
    float slotSize = 34.0f;
    float slotGap = 6.0f;
    float slotRadius = 10.0f;
    float iconSize = 20.0f;
    float traySlotSize = 26.0f;
    float traySlotGap = 4.0f;
    float trayIconSize = 16.0f;
    float chevronWidth = 22.0f;
    float sectionGap = 10.0f;
    float dividerWidth = 1.0f;
    float clockWidth = 92.0f;
    float thickness = 44.0f;
};

struct TaskbarLayout {
    TaskbarMetrics m{};
    TaskbarSettings settings{};
    float startLeft = 0.0f;
    float taskViewLeft = 0.0f;
    float searchLeft = 0.0f;
    float widgetsLeft = 0.0f;
    float resumeLeft = 0.0f;
    float appsStartX = 0.0f;
    float maxAppX = 0.0f;
    float visualizerLeft = 0.0f;
    float visualizerRight = 0.0f;
    float trayLeft = 0.0f;
    float chevronLeft = 0.0f;
    float traySectionRight = 0.0f;
    float clockLeft = 0.0f;
    float clockRight = 0.0f;
    float slotTop = 0.0f;
    float traySlotTop = 0.0f;
    bool showVisualizer = false;
    int appCount = 0;
    int trayCount = 0;
    float appSlotX(int index) const noexcept;
    float traySlotX(int index) const noexcept;
};
// ---------- DECLARATIONS
TaskbarMetrics metricsForSize(int sizeIndex);
TaskbarLayout computeTaskbarLayout(float widthDip, const TaskbarSettings& settings, int appCount,
                                   int trayCount, bool showVisualizer);
TaskbarHit hitTestTaskbar(const TaskbarLayout& layout, float dipX, float dipY);
float taskbarSlotCenterX(const TaskbarLayout& layout, const TaskbarHit& hit);
int taskbarDragTargetIndex(const TaskbarLayout& layout, TaskbarZone zone, float dipX,
                           int pinnedAppCount);