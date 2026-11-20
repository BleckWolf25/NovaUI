/**
 * @file TaskbarPaint.cpp
 *
 * @version 1.2.0
 * @author BleckWolf25
 * @license MIT
 *
 * @summary Implementation of the axis-mapped Liquid Glass dock frame renderer
 *
 * @description
 * Composites the glass body and inner rims, the Start prism, the enabled Task View, search,
 * widgets and Resume glyphs, dividers, the overflow chevron, promoted tray icons, the visualizer
 * strip, app lenses with running pills, attention highlights and badges, and the shadowed clock.
 * Every rectangle is produced by TaskbarLayout::band so the same code draws a horizontal dock on
 * the top or bottom edge and a vertical dock on the left or right edge, with icons and text kept
 * upright. Badge and attention drawing are gated by the user's taskbar settings.
 *
 * @since 18/11/2026
 * @updated 20/11/2026
 */
// ---------- IMPORTS
#include "Modules/TaskbarPaint.h"
#include "Render/Glass.h"
#include <algorithm>
// ---------- IMPLEMENTATION
HRESULT paintTaskbar(Renderer& renderer, const TaskbarPaintState& state) {
    return renderer.draw([&renderer, &state](ID2D1DeviceContext& dc) {
        const D2D1_SIZE_F size = dc.GetSize();
        const TaskbarLayout& l = state.layout;
        const TaskbarMetrics& m = l.m;
        const float scale = m.slotSize / 34.0f;
        const D2D1_RECT_F fullRect = D2D1::RectF(0.0f, 0.0f, size.width, size.height);
        dc.FillRectangle(fullRect, renderer.brush(GlassBar::kBaseTint));
        if (auto sheen = renderer.verticalGradient(0.0f, size.height, GlassBar::kSheenTop,
                                                  GlassBar::kSheenMid, 0.42f,
                                                  GlassBar::kSheenBottom)) {
            dc.FillRectangle(fullRect, sheen.Get());
        }
        dc.FillRectangle(l.band(0.0f, l.length, l.cross - 1.0f, l.cross),
                         renderer.brush(GlassBar::kTopSpecular));
        dc.FillRectangle(l.band(0.0f, l.length, l.cross - 2.0f, l.cross - 1.0f),
                         renderer.brush(GlassBar::kInnerBevel));
        const auto drawLens = [&](const D2D1_RECT_F& rect, bool active, bool hovered,
                                  bool dragged = false) {
            if (!active && !hovered && !dragged) {
                return;
            }
            const D2D1_ROUNDED_RECT lens = D2D1::RoundedRect(rect, m.slotRadius, m.slotRadius);
            const D2D1_COLOR_F fill = dragged ? GlassBar::kLensDragFill
                                              : (active ? GlassBar::kLensActiveFill
                                                        : GlassBar::kLensHoverFill);
            dc.FillRoundedRectangle(lens, renderer.brush(fill));
            if (auto rim = renderer.verticalGradient(
                    rect.top, rect.bottom,
                    (active || dragged) ? GlassBar::kLensRimTop : GlassBar::kLensRimMid,
                    GlassBar::kInnerBevel, 0.5f, GlassBar::kLensRimBottom)) {
                dc.DrawRoundedRectangle(lens, rim.Get(), 1.0f);
            }
        };
        const auto drawDivider = [&](float along) {
            const D2D1_RECT_F rect = l.band(along, along + m.dividerWidth, 10.0f, l.cross - 10.0f);
            if (auto brush = renderer.verticalGradient(
                    rect.top, rect.bottom, D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.01f),
                    D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.14f), 0.5f,
                    D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.01f))) {
                dc.FillRectangle(rect, brush.Get());
            }
        };
        const auto insetSquare = [](const D2D1_RECT_F& rect, float inset) {
            return D2D1::RectF(rect.left + inset, rect.top + inset, rect.right - inset,
                               rect.bottom - inset);
        };
        const auto shadowOf = [](const D2D1_RECT_F& rect) {
            return D2D1::RectF(rect.left + 1.0f, rect.top + 1.0f, rect.right + 1.0f,
                               rect.bottom + 1.0f);
        };
        const D2D1_RECT_F startRect = l.slot(l.startLeft, m.slotSize);
        const bool startHovered = (state.hover.zone == TaskbarZone::Start);
        drawLens(startRect, false, startHovered);
        const float cx = (startRect.left + startRect.right) * 0.5f;
        const float cy = (startRect.top + startRect.bottom) * 0.5f;
        const float pane = 8.0f * scale;
        const float paneGap = 0.75f * scale;
        const float outerRadius = 2.2f * scale;
        const float innerRadius = 0.4f * scale;
        const D2D1_RECT_F panes[4]{
            D2D1::RectF(cx - paneGap - pane, cy - paneGap - pane, cx - paneGap, cy - paneGap),
            D2D1::RectF(cx + paneGap, cy - paneGap - pane, cx + paneGap + pane, cy - paneGap),
            D2D1::RectF(cx - paneGap - pane, cy + paneGap, cx - paneGap, cy + paneGap + pane),
            D2D1::RectF(cx + paneGap, cy + paneGap, cx + paneGap + pane, cy + paneGap + pane),
        };
        const float corners[4][4]{
            {outerRadius, innerRadius, innerRadius, innerRadius},
            {innerRadius, outerRadius, innerRadius, innerRadius},
            {innerRadius, innerRadius, innerRadius, outerRadius},
            {innerRadius, innerRadius, outerRadius, innerRadius},
        };
        auto paneGradient = renderer.verticalGradient(
            cy - paneGap - pane, cy + paneGap + pane,
            startHovered ? GlassBar::kWinPaneHoverTop : GlassBar::kWinPaneTop,
            startHovered ? GlassBar::kWinPaneHoverMid : GlassBar::kWinPaneMid, 0.48f,
            startHovered ? GlassBar::kWinPaneHoverBottom : GlassBar::kWinPaneBottom);
        for (int p = 0; p < 4; ++p) {
            auto geometry = createAsymmetricRoundRect(renderer.factory(), panes[p], corners[p][0],
                                                      corners[p][1], corners[p][2], corners[p][3]);
            if (geometry) {
                ID2D1Brush* fill = paneGradient
                                       ? static_cast<ID2D1Brush*>(paneGradient.Get())
                                       : static_cast<ID2D1Brush*>(
                                             renderer.brush(GlassBar::kWinPaneMid));
                dc.FillGeometry(geometry.Get(), fill);
                dc.DrawGeometry(geometry.Get(),
                                renderer.brush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.24f)), 0.5f);
            }
        }
        drawDivider(l.startLeft + m.slotSize + m.sectionGap);
        if (l.settings.showTaskView) {
            const D2D1_RECT_F rect = l.slot(l.taskViewLeft, m.slotSize);
            const bool hovered = (state.hover.zone == TaskbarZone::TaskView);
            drawLens(rect, false, hovered);
            const float gx = (rect.left + rect.right) * 0.5f;
            const float gy = (rect.top + rect.bottom) * 0.5f;
            dc.DrawRoundedRectangle(
                D2D1::RoundedRect(D2D1::RectF(gx - 7.0f * scale, gy - 6.0f * scale,
                                              gx + 5.0f * scale, gy + 4.0f * scale),
                                  2.0f * scale, 2.0f * scale),
                renderer.brush(GlassBar::kMuted), 1.3f);
            const D2D1_ROUNDED_RECT front = D2D1::RoundedRect(
                D2D1::RectF(gx - 3.0f * scale, gy - 2.0f * scale, gx + 8.0f * scale,
                            gy + 7.0f * scale),
                2.0f * scale, 2.0f * scale);
            dc.FillRoundedRectangle(front, renderer.brush(GlassBar::kBaseTint));
            dc.DrawRoundedRectangle(
                front, renderer.brush(hovered ? GlassBar::kAccent : GlassBar::kMuted), 1.3f);
        }
        if (l.settings.showSearch) {
            const D2D1_RECT_F rect = l.slot(l.searchLeft, m.slotSize);
            const bool hovered = (state.hover.zone == TaskbarZone::Search);
            drawLens(rect, false, hovered);
            const float gx = (rect.left + rect.right) * 0.5f;
            const float gy = (rect.top + rect.bottom) * 0.5f;
            const float radius = 5.0f * scale;
            const D2D1_COLOR_F tint = hovered ? GlassBar::kAccent : GlassBar::kMuted;
            dc.DrawEllipse(D2D1::Ellipse(D2D1::Point2F(gx - 1.5f * scale, gy - 1.5f * scale),
                                         radius, radius),
                           renderer.brush(tint), 1.4f);
            dc.DrawLine(D2D1::Point2F(gx + 2.4f * scale, gy + 2.4f * scale),
                        D2D1::Point2F(gx + 6.0f * scale, gy + 6.0f * scale),
                        renderer.brush(tint), 1.6f);
        }
        if (l.settings.showWidgets) {
            const D2D1_RECT_F rect = l.slot(l.widgetsLeft, m.slotSize);
            const bool hovered = (state.hover.zone == TaskbarZone::Widgets);
            drawLens(rect, false, hovered);
            const float gx = (rect.left + rect.right) * 0.5f;
            const float gy = (rect.top + rect.bottom) * 0.5f;
            const float half = 5.5f * scale;
            const D2D1_COLOR_F tint = hovered ? GlassBar::kAccent : GlassBar::kMuted;
            dc.FillRoundedRectangle(
                D2D1::RoundedRect(D2D1::RectF(gx - half, gy - half, gx - 0.8f * scale, gy + half),
                                  1.6f * scale, 1.6f * scale),
                renderer.brush(tint));
            dc.DrawRoundedRectangle(
                D2D1::RoundedRect(D2D1::RectF(gx + 0.8f * scale, gy - half, gx + half, gy + half),
                                  1.6f * scale, 1.6f * scale),
                renderer.brush(tint), 1.3f);
        }
        if (l.settings.showResume) {
            const D2D1_RECT_F rect = l.slot(l.resumeLeft, m.slotSize);
            const bool hovered = (state.hover.zone == TaskbarZone::Resume);
            drawLens(rect, false, hovered);
            const float gx = (rect.left + rect.right) * 0.5f;
            const float gy = (rect.top + rect.bottom) * 0.5f;
            const D2D1_COLOR_F tint = hovered ? GlassBar::kAccent : GlassBar::kMuted;
            dc.DrawRoundedRectangle(
                D2D1::RoundedRect(D2D1::RectF(gx - 6.5f * scale, gy - 5.0f * scale,
                                              gx + 6.5f * scale, gy + 5.0f * scale),
                                  2.0f * scale, 2.0f * scale),
                renderer.brush(tint), 1.3f);
            dc.DrawLine(D2D1::Point2F(gx - 2.0f * scale, gy),
                        D2D1::Point2F(gx + 3.0f * scale, gy), renderer.brush(tint), 1.4f);
            dc.DrawLine(D2D1::Point2F(gx + 1.0f * scale, gy - 2.0f * scale),
                        D2D1::Point2F(gx + 3.0f * scale, gy), renderer.brush(tint), 1.4f);
            dc.DrawLine(D2D1::Point2F(gx + 1.0f * scale, gy + 2.0f * scale),
                        D2D1::Point2F(gx + 3.0f * scale, gy), renderer.brush(tint), 1.4f);
        }
        if (l.showVisualizer && l.visualizerRight > l.visualizerLeft) {
            const float baseline = 8.0f * scale;
            const float maxHeight = std::max(4.0f, l.cross - 18.0f * scale);
            const size_t bars = state.visualizerLevels.size();
            const float span = l.visualizerRight - l.visualizerLeft;
            if (bars > 0) {
                const float stride = span / static_cast<float>(bars);
                const float barWidth = std::max(1.5f, stride * 0.55f);
                for (size_t i = 0; i < bars; ++i) {
                    const float level = std::clamp(state.visualizerLevels[i], 0.0f, 1.0f);
                    const float barLength = std::max(1.5f, level * maxHeight);
                    const float along = l.visualizerLeft + static_cast<float>(i) * stride;
                    dc.FillRoundedRectangle(
                        D2D1::RoundedRect(l.band(along, along + barWidth, baseline,
                                                 baseline + barLength),
                                          1.0f, 1.0f),
                        renderer.brush(level > 0.02f ? GlassBar::kAccent : GlassBar::kMuted));
                }
            } else {
                const D2D1_RECT_F idle = l.band(l.visualizerLeft, l.visualizerRight, baseline,
                                                baseline + 1.2f);
                dc.FillRectangle(idle, renderer.brush(GlassBar::kMuted));
            }
        }
        drawDivider(l.clockLeft - m.sectionGap - m.dividerWidth);
        const D2D1_RECT_F chevronRect = l.band(l.chevronLeft, l.traySectionRight, l.traySlotTop,
                                               l.traySlotTop + m.traySlotSize);
        drawLens(chevronRect, state.trayOverflowOpen,
                 state.hover.zone == TaskbarZone::TrayOverflow);
        const float chevCx = (chevronRect.left + chevronRect.right) * 0.5f;
        const float chevCy = (chevronRect.top + chevronRect.bottom) * 0.5f;
        const float chevArm = 3.5f * scale;
        const float chevRise = 1.8f * scale;
        ID2D1SolidColorBrush* chevBrush = renderer.brush(GlassBar::kMuted);
        if (l.vertical) {
            const float dir = l.mirrorCross ? -1.0f : 1.0f;
            dc.DrawLine(D2D1::Point2F(chevCx - dir * chevRise, chevCy - chevArm),
                        D2D1::Point2F(chevCx + dir * chevRise, chevCy), chevBrush, 1.4f);
            dc.DrawLine(D2D1::Point2F(chevCx + dir * chevRise, chevCy),
                        D2D1::Point2F(chevCx - dir * chevRise, chevCy + chevArm), chevBrush,
                        1.4f);
        } else {
            const float dir = l.mirrorCross ? 1.0f : -1.0f;
            dc.DrawLine(D2D1::Point2F(chevCx - chevArm, chevCy - dir * chevRise),
                        D2D1::Point2F(chevCx, chevCy + dir * chevRise), chevBrush, 1.4f);
            dc.DrawLine(D2D1::Point2F(chevCx, chevCy + dir * chevRise),
                        D2D1::Point2F(chevCx + chevArm, chevCy - dir * chevRise), chevBrush,
                        1.4f);
        }
        const auto drawTraySlot = [&](int index, float along, bool dragged) {
            const TrayItem& item = state.tray[static_cast<size_t>(index)];
            const D2D1_RECT_F rect = l.band(along, along + m.traySlotSize, l.traySlotTop,
                                            l.traySlotTop + m.traySlotSize);
            const bool hovered = (!state.dragging && state.hover.zone == TaskbarZone::Tray &&
                                  state.hover.index == index);
            drawLens(rect, false, hovered, dragged);
            if (item.icon) {
                const float offset = (m.traySlotSize - m.trayIconSize) * 0.5f;
                dc.DrawBitmap(item.icon.Get(), insetSquare(rect, offset), 1.0f,
                              D2D1_INTERPOLATION_MODE_HIGH_QUALITY_CUBIC);
            }
        };
        for (int i = 0; i < static_cast<int>(state.tray.size()); ++i) {
            if (state.dragging && state.dragZone == TaskbarZone::Tray && i == state.dragFromIndex) {
                continue;
            }
            int visualSlot = i;
            if (state.dragging && state.dragZone == TaskbarZone::Tray) {
                if (state.dragFromIndex < state.dragToIndex && i > state.dragFromIndex &&
                    i <= state.dragToIndex) {
                    visualSlot = i - 1;
                } else if (state.dragFromIndex > state.dragToIndex && i >= state.dragToIndex &&
                           i < state.dragFromIndex) {
                    visualSlot = i + 1;
                }
            }
            drawTraySlot(i, l.traySlotX(visualSlot), false);
        }
        if (state.dragging && state.dragZone == TaskbarZone::Tray && state.dragFromIndex >= 0 &&
            state.dragFromIndex < static_cast<int>(state.tray.size())) {
            drawTraySlot(state.dragFromIndex,
                         l.traySlotX(state.dragFromIndex) + state.dragDeltaDip, true);
        }
        const auto drawAppSlot = [&](int index, float along, bool dragged) {
            const TaskItem& item = state.apps[static_cast<size_t>(index)];
            const D2D1_RECT_F rect = l.slot(along, m.slotSize);
            const bool hovered = (!state.dragging && state.hover.zone == TaskbarZone::App &&
                                  state.hover.index == index);
            drawLens(rect, item.active, hovered, dragged);
            if (item.attention && l.settings.showFlashing) {
                const D2D1_ROUNDED_RECT lens =
                    D2D1::RoundedRect(rect, m.slotRadius, m.slotRadius);
                dc.FillRoundedRectangle(lens, renderer.brush(GlassBar::kAttentionFill));
                dc.DrawRoundedRectangle(lens, renderer.brush(GlassBar::kAttentionRim), 1.2f);
            }
            if (item.icon) {
                const float offset = (m.slotSize - m.iconSize) * 0.5f;
                const float opacity = (item.running || hovered || dragged) ? 1.0f : 0.92f;
                dc.DrawBitmap(item.icon.Get(), insetSquare(rect, offset), opacity,
                              D2D1_INTERPOLATION_MODE_HIGH_QUALITY_CUBIC);
            }
            if (item.attention && l.settings.showBadges) {
                const float radius = 3.2f * scale;
                dc.FillEllipse(
                    D2D1::Ellipse(D2D1::Point2F(rect.right - 5.0f * scale,
                                                rect.top + 5.0f * scale),
                                  radius, radius),
                    renderer.brush(GlassBar::kBadgeFill));
            }
            if (!item.running) {
                return;
            }
            if (item.windows.size() > 1) {
                const float groupLength = 14.0f * scale;
                const float first = along + (m.slotSize - groupLength) * 0.5f;
                dc.FillRoundedRectangle(
                    D2D1::RoundedRect(l.band(first, first + 8.0f * scale, 2.5f * scale,
                                             4.5f * scale),
                                      1.0f, 1.0f),
                    renderer.brush(item.active ? GlassBar::kAccent : GlassBar::kMuted));
                dc.FillRoundedRectangle(
                    D2D1::RoundedRect(l.band(first + 10.0f * scale, first + groupLength,
                                             2.5f * scale, 4.5f * scale),
                                      1.0f, 1.0f),
                    renderer.brush(GlassBar::kMuted));
                return;
            }
            const float pillLength = (item.active ? 12.0f : 4.5f) * scale;
            const float pillAlong = along + (m.slotSize - pillLength) * 0.5f;
            if (item.active) {
                dc.FillRoundedRectangle(
                    D2D1::RoundedRect(l.band(pillAlong - 2.0f * scale,
                                             pillAlong + pillLength + 2.0f * scale,
                                             1.5f * scale, 5.5f * scale),
                                      2.0f, 2.0f),
                    renderer.brush(GlassBar::kAccentGlow));
            }
            dc.FillRoundedRectangle(
                D2D1::RoundedRect(l.band(pillAlong, pillAlong + pillLength, 2.5f * scale,
                                         4.5f * scale),
                                  1.0f, 1.0f),
                renderer.brush(item.active ? GlassBar::kAccent : GlassBar::kMuted));
        };
        for (int i = 0; i < static_cast<int>(state.apps.size()); ++i) {
            if (state.dragging && state.dragZone == TaskbarZone::App && i == state.dragFromIndex) {
                continue;
            }
            int visualSlot = i;
            if (state.dragging && state.dragZone == TaskbarZone::App && state.dragWasPinned) {
                if (state.dragFromIndex < state.dragToIndex && i > state.dragFromIndex &&
                    i <= state.dragToIndex) {
                    visualSlot = i - 1;
                } else if (state.dragFromIndex > state.dragToIndex && i >= state.dragToIndex &&
                           i < state.dragFromIndex) {
                    visualSlot = i + 1;
                }
            }
            const float along = l.appSlotX(visualSlot);
            if (along > l.maxAppX) {
                break;
            }
            drawAppSlot(i, along, false);
        }
        if (state.dragging && state.dragZone == TaskbarZone::App && state.dragFromIndex >= 0 &&
            state.dragFromIndex < static_cast<int>(state.apps.size())) {
            drawAppSlot(state.dragFromIndex,
                        l.appSlotX(state.dragFromIndex) + state.dragDeltaDip, true);
        }
        if (state.hover.zone == TaskbarZone::Clock) {
            drawLens(l.band(l.clockLeft - 4.0f, l.clockRight + 4.0f, l.slotTop,
                            l.slotTop + m.slotSize),
                     false, true);
        }
        const float clockPad = 4.0f * scale;
        const float clockMid = (l.clockLeft + l.clockRight) * 0.5f;
        const D2D1_RECT_F timeRect =
            l.vertical ? l.band(l.clockLeft, clockMid, clockPad, l.cross - clockPad)
                       : l.band(l.clockLeft, l.clockRight, l.cross * 0.45f, l.cross * 0.86f);
        const D2D1_RECT_F dateRect =
            l.vertical ? l.band(clockMid, l.clockRight, clockPad, l.cross - clockPad)
                       : l.band(l.clockLeft, l.clockRight, l.cross * 0.11f, l.cross * 0.48f);
        const auto timeLength = static_cast<UINT32>(state.time.size());
        const auto dateLength = static_cast<UINT32>(state.date.size());
        dc.DrawText(state.time.data(), timeLength, state.timeFormat, shadowOf(timeRect),
                    renderer.brush(GlassBar::kTextShadow));
        dc.DrawText(state.date.data(), dateLength, state.dateFormat, shadowOf(dateRect),
                    renderer.brush(GlassBar::kTextShadow));
        dc.DrawText(state.time.data(), timeLength, state.timeFormat, timeRect,
                    renderer.brush(GlassBar::kText));
        dc.DrawText(state.date.data(), dateLength, state.dateFormat, dateRect,
                    renderer.brush(GlassBar::kMuted));
    });
}