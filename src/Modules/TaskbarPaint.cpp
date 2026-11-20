/**
 * @file TaskbarPaint.cpp
 *
 * @version 1.1.0
 * @author BleckWolf25
 * @license MIT
 *
 * @summary Implementation of the settings-driven Liquid Glass dock frame renderer
 *
 * @description
 * Composites the glass body and rims, the Start prism, the enabled Task View, search, widgets
 * and Resume glyphs, dividers, the overflow chevron, promoted tray icons, the visualizer strip,
 * app lenses with running pills, attention highlights and badges, and the shadowed clock stack.
 * Every position and size derives from the scaled TaskbarMetrics so the dock follows the Windows
 * size setting, and badge or attention drawing is skipped when the user disabled it.
 *
 * @since 18/11/2026
 * @updated 18/11/2026
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
        dc.FillRectangle(D2D1::RectF(0.0f, 0.0f, size.width, 1.0f),
                         renderer.brush(GlassBar::kTopSpecular));
        dc.FillRectangle(D2D1::RectF(0.0f, 1.0f, size.width, 2.0f),
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
        const auto drawDivider = [&](float divX) {
            if (auto brush = renderer.verticalGradient(
                    10.0f, size.height - 10.0f, D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.01f),
                    D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.14f), 0.5f,
                    D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.01f))) {
                dc.FillRectangle(
                    D2D1::RectF(divX, 10.0f, divX + m.dividerWidth, size.height - 10.0f),
                    brush.Get());
            }
        };
        const auto slotRect = [&](float left) {
            return D2D1::RectF(left, l.slotTop, left + m.slotSize, l.slotTop + m.slotSize);
        };
        const D2D1_RECT_F startRect = slotRect(l.startLeft);
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
            const D2D1_RECT_F rect = slotRect(l.taskViewLeft);
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
            const D2D1_RECT_F rect = slotRect(l.searchLeft);
            const bool hovered = (state.hover.zone == TaskbarZone::Search);
            drawLens(rect, false, hovered);
            const float gx = (rect.left + rect.right) * 0.5f;
            const float gy = (rect.top + rect.bottom) * 0.5f;
            const float radius = 5.0f * scale;
            dc.DrawEllipse(D2D1::Ellipse(D2D1::Point2F(gx - 1.5f * scale, gy - 1.5f * scale),
                                         radius, radius),
                           renderer.brush(hovered ? GlassBar::kAccent : GlassBar::kMuted), 1.4f);
            dc.DrawLine(D2D1::Point2F(gx + 2.4f * scale, gy + 2.4f * scale),
                        D2D1::Point2F(gx + 6.0f * scale, gy + 6.0f * scale),
                        renderer.brush(hovered ? GlassBar::kAccent : GlassBar::kMuted), 1.6f);
        }
        if (l.settings.showWidgets) {
            const D2D1_RECT_F rect = slotRect(l.widgetsLeft);
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
            const D2D1_RECT_F rect = slotRect(l.resumeLeft);
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
            const float baseline = size.height - 8.0f * scale;
            const float maxHeight = std::max(4.0f, size.height - 18.0f * scale);
            const size_t bars = state.visualizerLevels.size();
            const float span = l.visualizerRight - l.visualizerLeft;
            if (bars > 0) {
                const float stride = span / static_cast<float>(bars);
                const float barWidth = std::max(1.5f, stride * 0.55f);
                for (size_t i = 0; i < bars; ++i) {
                    const float level = std::clamp(state.visualizerLevels[i], 0.0f, 1.0f);
                    const float barHeight = std::max(1.5f, level * maxHeight);
                    const float left = l.visualizerLeft + static_cast<float>(i) * stride;
                    dc.FillRoundedRectangle(
                        D2D1::RoundedRect(D2D1::RectF(left, baseline - barHeight,
                                                      left + barWidth, baseline),
                                          1.0f, 1.0f),
                        renderer.brush(level > 0.02f ? GlassBar::kAccent : GlassBar::kMuted));
                }
            } else {
                dc.DrawLine(D2D1::Point2F(l.visualizerLeft, baseline),
                            D2D1::Point2F(l.visualizerRight, baseline),
                            renderer.brush(GlassBar::kMuted), 1.2f);
            }
        }
        drawDivider(l.clockLeft - m.sectionGap - m.dividerWidth);
        const D2D1_RECT_F chevronRect = D2D1::RectF(l.chevronLeft, l.traySlotTop,
                                                    l.traySectionRight,
                                                    l.traySlotTop + m.traySlotSize);
        drawLens(chevronRect, state.trayOverflowOpen,
                 state.hover.zone == TaskbarZone::TrayOverflow);
        const float chevCx = (chevronRect.left + chevronRect.right) * 0.5f;
        const float chevCy = (chevronRect.top + chevronRect.bottom) * 0.5f;
        dc.DrawLine(D2D1::Point2F(chevCx - 3.5f * scale, chevCy + 1.8f * scale),
                    D2D1::Point2F(chevCx, chevCy - 1.8f * scale),
                    renderer.brush(GlassBar::kMuted), 1.4f);
        dc.DrawLine(D2D1::Point2F(chevCx, chevCy - 1.8f * scale),
                    D2D1::Point2F(chevCx + 3.5f * scale, chevCy + 1.8f * scale),
                    renderer.brush(GlassBar::kMuted), 1.4f);
        const auto drawTraySlot = [&](int index, float drawX, bool dragged) {
            const TrayItem& item = state.tray[static_cast<size_t>(index)];
            const D2D1_RECT_F rect = D2D1::RectF(drawX, l.traySlotTop, drawX + m.traySlotSize,
                                                 l.traySlotTop + m.traySlotSize);
            const bool hovered = (!state.dragging && state.hover.zone == TaskbarZone::Tray &&
                                  state.hover.index == index);
            drawLens(rect, false, hovered, dragged);
            if (item.icon) {
                const float offset = (m.traySlotSize - m.trayIconSize) * 0.5f;
                dc.DrawBitmap(item.icon.Get(),
                              D2D1::RectF(drawX + offset, l.traySlotTop + offset,
                                          drawX + offset + m.trayIconSize,
                                          l.traySlotTop + offset + m.trayIconSize),
                              1.0f, D2D1_INTERPOLATION_MODE_HIGH_QUALITY_CUBIC);
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
        const auto drawAppSlot = [&](int index, float drawX, bool dragged) {
            const TaskItem& item = state.apps[static_cast<size_t>(index)];
            const D2D1_RECT_F rect = slotRect(drawX);
            const bool hovered = (!state.dragging && state.hover.zone == TaskbarZone::App &&
                                  state.hover.index == index);
            const bool attention = item.attention && l.settings.showFlashing;
            drawLens(rect, item.active, hovered, dragged);
            if (attention) {
                const D2D1_ROUNDED_RECT lens =
                    D2D1::RoundedRect(rect, m.slotRadius, m.slotRadius);
                dc.FillRoundedRectangle(lens, renderer.brush(GlassBar::kAttentionFill));
                dc.DrawRoundedRectangle(lens, renderer.brush(GlassBar::kAttentionRim), 1.2f);
            }
            if (item.icon) {
                const float offset = (m.slotSize - m.iconSize) * 0.5f;
                const float opacity = (item.running || hovered || dragged) ? 1.0f : 0.92f;
                dc.DrawBitmap(item.icon.Get(),
                              D2D1::RectF(drawX + offset, l.slotTop + offset - 0.5f,
                                          drawX + offset + m.iconSize,
                                          l.slotTop + offset + m.iconSize - 0.5f),
                              opacity, D2D1_INTERPOLATION_MODE_HIGH_QUALITY_CUBIC);
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
                const float groupWidth = 14.0f * scale;
                const float leftX = drawX + (m.slotSize - groupWidth) * 0.5f;
                dc.FillRoundedRectangle(
                    D2D1::RoundedRect(D2D1::RectF(leftX, size.height - 4.5f * scale,
                                                  leftX + 8.0f * scale,
                                                  size.height - 2.5f * scale),
                                      1.0f, 1.0f),
                    renderer.brush(item.active ? GlassBar::kAccent : GlassBar::kMuted));
                dc.FillRoundedRectangle(
                    D2D1::RoundedRect(D2D1::RectF(leftX + 10.0f * scale,
                                                  size.height - 4.5f * scale,
                                                  leftX + groupWidth,
                                                  size.height - 2.5f * scale),
                                      1.0f, 1.0f),
                    renderer.brush(GlassBar::kMuted));
                return;
            }
            const float pillWidth = (item.active ? 12.0f : 4.5f) * scale;
            const float pillX = drawX + (m.slotSize - pillWidth) * 0.5f;
            if (item.active) {
                dc.FillRoundedRectangle(
                    D2D1::RoundedRect(D2D1::RectF(pillX - 2.0f * scale,
                                                  size.height - 5.5f * scale,
                                                  pillX + pillWidth + 2.0f * scale,
                                                  size.height - 1.5f * scale),
                                      2.0f, 2.0f),
                    renderer.brush(GlassBar::kAccentGlow));
            }
            dc.FillRoundedRectangle(
                D2D1::RoundedRect(D2D1::RectF(pillX, size.height - 4.5f * scale,
                                              pillX + pillWidth, size.height - 2.5f * scale),
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
            const float drawX = l.appSlotX(visualSlot);
            if (drawX > l.maxAppX) {
                break;
            }
            drawAppSlot(i, drawX, false);
        }
        if (state.dragging && state.dragZone == TaskbarZone::App && state.dragFromIndex >= 0 &&
            state.dragFromIndex < static_cast<int>(state.apps.size())) {
            drawAppSlot(state.dragFromIndex,
                        l.appSlotX(state.dragFromIndex) + state.dragDeltaDip, true);
        }
        if (state.hover.zone == TaskbarZone::Clock) {
            drawLens(D2D1::RectF(l.clockLeft - 4.0f, l.slotTop, l.clockRight + 4.0f,
                                 l.slotTop + m.slotSize),
                     false, true);
        }
        const float thick = m.thickness;
        const D2D1_RECT_F timeRect =
            D2D1::RectF(l.clockLeft, thick * 0.14f, l.clockRight, thick * 0.55f);
        const D2D1_RECT_F dateRect =
            D2D1::RectF(l.clockLeft, thick * 0.52f, l.clockRight, thick * 0.89f);
        const D2D1_RECT_F timeShadow =
            D2D1::RectF(l.clockLeft, thick * 0.14f + 1.0f, l.clockRight, thick * 0.55f + 1.0f);
        const D2D1_RECT_F dateShadow =
            D2D1::RectF(l.clockLeft, thick * 0.52f + 1.0f, l.clockRight, thick * 0.89f + 1.0f);
        const auto timeLength = static_cast<UINT32>(state.time.size());
        const auto dateLength = static_cast<UINT32>(state.date.size());
        dc.DrawText(state.time.data(), timeLength, state.timeFormat, timeShadow,
                    renderer.brush(GlassBar::kTextShadow));
        dc.DrawText(state.date.data(), dateLength, state.dateFormat, dateShadow,
                    renderer.brush(GlassBar::kTextShadow));
        dc.DrawText(state.time.data(), timeLength, state.timeFormat, timeRect,
                    renderer.brush(GlassBar::kText));
        dc.DrawText(state.date.data(), dateLength, state.dateFormat, dateRect,
                    renderer.brush(GlassBar::kMuted));
    });
}