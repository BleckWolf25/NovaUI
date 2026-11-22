/**
 * @file TaskbarSettings.h
 *
 * @version 1.1.0
 * @author BleckWolf25
 * @license MIT
 *
 * @summary Reader for the user's Windows taskbar personalization and behaviour settings
 *
 * @description
 * Resolves the reserved screen edge through ABM_GETTASKBARPOS on Shell_TrayWnd, falling back to
 * the TaskbarLocation value and then the StuckRects3 position byte when the shell cannot answer.
 * Reads HKCU Explorer\Advanced for alignment, size, Task View, search, widgets, Resume, badges
 * and flashing. Values are sampled at startup and on WM_SETTINGCHANGE, so no registry or disk
 * access happens during steady-state rendering.
 *
 * @since 18/11/2026
 * @updated 20/11/2026
 */
#pragma once
// ---------- IMPORTS
#include <Windows.h>
// ---------- TYPES
enum class TaskbarAlignment { Start, Center };
enum class TaskbarEdge { Left, Top, Right, Bottom };

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
    bool vertical() const noexcept {
        return edge == TaskbarEdge::Left || edge == TaskbarEdge::Right;
    }
};
// ---------- DECLARATIONS
TaskbarSettings readTaskbarSettings();