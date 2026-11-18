/**
 * @file TaskbarLayout.h
 *
 * @version 1.0.0
 * @author BleckWolf25
 * @license MIT
 *
 * @summary Single source of truth for dock metrics, slot geometry, hit-testing and drag indexing
 *
 * @description
 * Owns every DIP metric of the dock and derives one TaskbarLayout from the bar size, the Task
 * View preference and the current app and tray counts. Hit-testing, flyout anchoring, drag
 * target indexing and painting all consume that same structure, so a slot that is drawn is
 * always a slot that can be clicked. Pure geometry: no window handles, no rendering, no state.
 *
 * @since 18/11/2026
 * @updated 18/11/2026
 */
#pragma once
// ---------- CONSTANTS
inline constexpr int kBarHeightDip = 44;
inline constexpr float kOuterPadX = 12.0f;
inline constexpr float kSlotSize = 34.0f;
inline constexpr float kSlotGap = 6.0f;
inline constexpr float kSlotRadius = 10.0f;
inline constexpr float kIconSize = 20.0f;
inline constexpr float kTraySlotSize = 26.0f;
inline constexpr float kTraySlotGap = 4.0f;
inline constexpr float kTrayIconSize = 16.0f;
inline constexpr float kChevronWidth = 22.0f;
inline constexpr float kSectionGap = 10.0f;
inline constexpr float kDividerWidth = 1.0f;
inline constexpr float kClockWidth = 92.0f;
// ---------- TYPES
enum class TaskbarZone { None, Start, TaskView, App, Tray, TrayOverflow, Clock };

struct TaskbarHit {
    TaskbarZone zone = TaskbarZone::None;
    int index = -1;
    bool operator==(const TaskbarHit& other) const noexcept {
        return zone == other.zone && index == other.index;
    }
    bool operator!=(const TaskbarHit& other) const noexcept { return !(*this == other); }
};

struct TaskbarLayout {
    float startLeft = 0.0f;
    float taskViewLeft = 0.0f;
    float appsStartX = 0.0f;
    float maxAppX = 0.0f;
    float trayLeft = 0.0f;
    float chevronLeft = 0.0f;
    float traySectionRight = 0.0f;
    float clockLeft = 0.0f;
    float clockRight = 0.0f;
    float slotTop = 0.0f;
    float traySlotTop = 0.0f;
    bool showTaskView = false;
    int appCount = 0;
    int trayCount = 0;
    float appSlotX(int index) const noexcept;
    float traySlotX(int index) const noexcept;
};
// ---------- DECLARATIONS
TaskbarLayout computeTaskbarLayout(float widthDip, float heightDip, bool showTaskView,
                                   int appCount, int trayCount);
TaskbarHit hitTestTaskbar(const TaskbarLayout& layout, float dipX, float dipY);
float taskbarSlotCenterX(const TaskbarLayout& layout, const TaskbarHit& hit);
int taskbarDragTargetIndex(const TaskbarLayout& layout, TaskbarZone zone, float dipX,
                           int pinnedAppCount);