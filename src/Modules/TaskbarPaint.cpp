/**
 * @file TaskbarPaint.cpp
 *
 * @version 1.0.0
 * @author BleckWolf25
 * @license MIT
 *
 * @summary Implementation of the Liquid Glass dock frame renderer
 *
 * @description
 * Composites the glass tint and vertical sheen, the top specular rim and inner bevel, then the
 * Start prism, the optional Task View glyph, section dividers, the overflow chevron, promoted
 * tray icons, app lenses with running indicators and the shadowed clock stack. Drag rendering
 * shifts neighbouring slots and draws the dragged icon at its offset so the drop target is
 * visible. All positions come from TaskbarLayout; all colours come from GlassBar.
 *
 * @since 18/11/2026
 * @updated 18/11/2026
 */
// ---------- IMPORTS
#include "Modules/TaskbarPaint.h"
#include "Render/Glass.h"
// ---------- IMPLEMENTATION
HRESULT paintTaskbar(Renderer& renderer, const TaskbarPaintState& state) {
    return renderer.draw([&renderer, &state](ID2D1DeviceContext& dc) {
        const D2D1_SIZE_F size = dc.GetSize();
        const TaskbarLayout& l = state.layout;
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
            const D2D1_ROUNDED_RECT lens = D2D1::RoundedRect(rect, kSlotRadius, kSlotRadius);
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
                    D2D1::RectF(divX, 10.0f, divX + kDividerWidth, size.height - 10.0f),
                    brush.Get());
            }
        };
        const D2D1_RECT_F startRect =
            D2D1::RectF(l.startLeft, l.slotTop, l.startLeft + kSlotSize, l.slotTop + kSlotSize);
        const bool startHovered = (state.hover.zone == TaskbarZone::Start);
        drawLens(startRect, false, startHovered);
        const float cx = (startRect.left + startRect.right) * 0.5f;
        const float cy = (startRect.top + startRect.bottom) * 0.5f;
        constexpr float kPane = 8.0f;
        constexpr float kPaneGap = 0.75f;
        constexpr float kOuterRadius = 2.2f;
        constexpr float kInnerRadius = 0.4f;
        const D2D1_RECT_F panes[4]{
            D2D1::RectF(cx - kPaneGap - kPane, cy - kPaneGap - kPane, cx - kPaneGap, cy - kPaneGap),
            D2D1::RectF(cx + kPaneGap, cy - kPaneGap - kPane, cx + kPaneGap + kPane, cy - kPaneGap),
            D2D1::RectF(cx - kPaneGap - kPane, cy + kPaneGap, cx - kPaneGap, cy + kPaneGap + kPane),
            D2D1::RectF(cx + kPaneGap, cy + kPaneGap, cx + kPaneGap + kPane, cy + kPaneGap + kPane),
        };
        const float corners[4][4]{
            {kOuterRadius, kInnerRadius, kInnerRadius, kInnerRadius},
            {kInnerRadius, kOuterRadius, kInnerRadius, kInnerRadius},
            {kInnerRadius, kInnerRadius, kInnerRadius, kOuterRadius},
            {kInnerRadius, kInnerRadius, kOuterRadius, kInnerRadius},
        };
        auto paneGradient = renderer.verticalGradient(
            cy - kPaneGap - kPane, cy + kPaneGap + kPane,
            startHovered ? GlassBar::kWinPaneHoverTop : GlassBar::kWinPaneTop,
            startHovered ? GlassBar::kWinPaneHoverMid : GlassBar::kWinPaneMid, 0.48f,
            startHovered ? GlassBar::kWinPaneHoverBottom : GlassBar::kWinPaneBottom);
        for (int p = 0; p < 4; ++p) {
            auto geometry = createAsymmetricRoundRect(renderer.factory(), panes[p], corners[p][0],
                                                      corners[p][1], corners[p][2], corners[p][3]);
            if (geometry) {
                ID2D1Brush* fill = paneGradient
                                       ? static_cast<ID2D1Brush*>(paneGradient.Get())
                                       : static_cast<ID2D1Brush*>(renderer.brush(GlassBar::kWinPaneMid));
                dc.FillGeometry(geometry.Get(), fill);
                dc.DrawGeometry(geometry.Get(),
                                renderer.brush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.24f)), 0.5f);
            }
        }
        drawDivider(l.startLeft + kSlotSize + kSectionGap);
        if (l.showTaskView) {
            const D2D1_RECT_F viewRect = D2D1::RectF(l.taskViewLeft, l.slotTop,
                                                     l.taskViewLeft + kSlotSize,
                                                     l.slotTop + kSlotSize);
            const bool viewHovered = (state.hover.zone == TaskbarZone::TaskView);
            drawLens(viewRect, false, viewHovered);
            const float viewCx = (viewRect.left + viewRect.right) * 0.5f;
            const float viewCy = (viewRect.top + viewRect.bottom) * 0.5f;
            const D2D1_ROUNDED_RECT back = D2D1::RoundedRect(
                D2D1::RectF(viewCx - 7.0f, viewCy - 6.0f, viewCx + 5.0f, viewCy + 4.0f), 2.0f, 2.0f);
            const D2D1_ROUNDED_RECT front = D2D1::RoundedRect(
                D2D1::RectF(viewCx - 3.0f, viewCy - 2.0f, viewCx + 8.0f, viewCy + 7.0f), 2.0f, 2.0f);
            dc.DrawRoundedRectangle(back, renderer.brush(GlassBar::kMuted), 1.3f);
            dc.FillRoundedRectangle(front, renderer.brush(GlassBar::kBaseTint));
            dc.DrawRoundedRectangle(
                front, renderer.brush(viewHovered ? GlassBar::kAccent : GlassBar::kMuted), 1.3f);
        }
        drawDivider(l.clockLeft - kSectionGap - kDividerWidth);
        const D2D1_RECT_F chevronRect = D2D1::RectF(l.chevronLeft, l.traySlotTop,
                                                    l.traySectionRight,
                                                    l.traySlotTop + kTraySlotSize);
        drawLens(chevronRect, state.trayOverflowOpen,
                 state.hover.zone == TaskbarZone::TrayOverflow);
        const float chevCx = (chevronRect.left + chevronRect.right) * 0.5f;
        const float chevCy = (chevronRect.top + chevronRect.bottom) * 0.5f;
        dc.DrawLine(D2D1::Point2F(chevCx - 3.5f, chevCy + 1.8f),
                    D2D1::Point2F(chevCx, chevCy - 1.8f), renderer.brush(GlassBar::kMuted), 1.4f);
        dc.DrawLine(D2D1::Point2F(chevCx, chevCy - 1.8f),
                    D2D1::Point2F(chevCx + 3.5f, chevCy + 1.8f), renderer.brush(GlassBar::kMuted),
                    1.4f);
        const auto drawTraySlot = [&](int index, float drawX, bool dragged) {
            const TrayItem& item = state.tray[static_cast<size_t>(index)];
            const D2D1_RECT_F rect = D2D1::RectF(drawX, l.traySlotTop, drawX + kTraySlotSize,
                                                 l.traySlotTop + kTraySlotSize);
            const bool hovered = (!state.dragging && state.hover.zone == TaskbarZone::Tray &&
                                  state.hover.index == index);
            drawLens(rect, false, hovered, dragged);
            if (item.icon) {
                const float offset = (kTraySlotSize - kTrayIconSize) * 0.5f;
                dc.DrawBitmap(item.icon.Get(),
                              D2D1::RectF(drawX + offset, l.traySlotTop + offset,
                                          drawX + offset + kTrayIconSize,
                                          l.traySlotTop + offset + kTrayIconSize),
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
            const D2D1_RECT_F rect =
                D2D1::RectF(drawX, l.slotTop, drawX + kSlotSize, l.slotTop + kSlotSize);
            const bool hovered = (!state.dragging && state.hover.zone == TaskbarZone::App &&
                                  state.hover.index == index);
            drawLens(rect, item.active, hovered, dragged);
            if (item.icon) {
                const float offset = (kSlotSize - kIconSize) * 0.5f;
                const float opacity = (item.running || hovered || dragged) ? 1.0f : 0.92f;
                dc.DrawBitmap(item.icon.Get(),
                              D2D1::RectF(drawX + offset, l.slotTop + offset - 0.5f,
                                          drawX + offset + kIconSize,
                                          l.slotTop + offset + kIconSize - 0.5f),
                              opacity, D2D1_INTERPOLATION_MODE_HIGH_QUALITY_CUBIC);
            }
            if (!item.running) {
                return;
            }
            if (item.windows.size() > 1) {
                constexpr float kGroupWidth = 14.0f;
                const float leftX = drawX + (kSlotSize - kGroupWidth) * 0.5f;
                dc.FillRoundedRectangle(
                    D2D1::RoundedRect(D2D1::RectF(leftX, size.height - 4.5f, leftX + 8.0f,
                                                  size.height - 2.5f),
                                      1.0f, 1.0f),
                    renderer.brush(item.active ? GlassBar::kAccent : GlassBar::kMuted));
                dc.FillRoundedRectangle(
                    D2D1::RoundedRect(D2D1::RectF(leftX + 10.0f, size.height - 4.5f,
                                                  leftX + kGroupWidth, size.height - 2.5f),
                                      1.0f, 1.0f),
                    renderer.brush(GlassBar::kMuted));
                return;
            }
            const float pillWidth = item.active ? 12.0f : 4.5f;
            const float pillX = drawX + (kSlotSize - pillWidth) * 0.5f;
            if (item.active) {
                dc.FillRoundedRectangle(
                    D2D1::RoundedRect(D2D1::RectF(pillX - 2.0f, size.height - 5.5f,
                                                  pillX + pillWidth + 2.0f, size.height - 1.5f),
                                      2.0f, 2.0f),
                    renderer.brush(GlassBar::kAccentGlow));
            }
            dc.FillRoundedRectangle(
                D2D1::RoundedRect(
                    D2D1::RectF(pillX, size.height - 4.5f, pillX + pillWidth, size.height - 2.5f),
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
                                 l.slotTop + kSlotSize),
                     false, true);
        }
        const D2D1_RECT_F timeRect = D2D1::RectF(l.clockLeft, 6.0f, l.clockRight, 24.0f);
        const D2D1_RECT_F dateRect = D2D1::RectF(l.clockLeft, 23.0f, l.clockRight, 39.0f);
        const D2D1_RECT_F timeShadow = D2D1::RectF(l.clockLeft, 7.0f, l.clockRight, 25.0f);
        const D2D1_RECT_F dateShadow = D2D1::RectF(l.clockLeft, 24.0f, l.clockRight, 40.0f);
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