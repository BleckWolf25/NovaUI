/**
 * @file TaskbarSettings.h
 *
 * @version 1.0.0
 * @author BleckWolf25
 * @license MIT
 *
 * @summary Reader for the user's Windows taskbar personalization and behaviour settings
 *
 * @description
 * Reads HKCU Explorer\Advanced for alignment, size, Task View, search, widgets, Resume, badges
 * and flashing, plus the StuckRects3 position byte for the reserved screen edge. Values are
 * sampled once at startup and again on WM_SETTINGCHANGE, so no registry or disk access happens
 * during steady-state rendering. Side edges are reported as bottom because Windows 11 breaks the
 * shell for those StuckRects3 values.
 *
 * @since 18/11/2026
 * @updated 18/11/2026
 */
#pragma once
// ---------- IMPORTS
#include <Windows.h>
// ---------- TYPES
enum class TaskbarAlignment { Left, Center };
enum class TaskbarEdge { Top, Bottom };

struct TaskbarSettings {
    TaskbarAlignment alignment = TaskbarAlignment::Center;
    TaskbarEdge edge = TaskbarEdge::Bottom;
    int sizeIndex = 1;
    bool showTaskView = true;
    bool showSearch = true;
    bool showWidgets = false;
    bool showResume = false;
    bool showBadges = true;
    bool showFlashing = true;
};
// ---------- DECLARATIONS
TaskbarSettings readTaskbarSettings();