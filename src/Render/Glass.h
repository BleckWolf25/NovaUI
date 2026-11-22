/**
 * @file Glass.h
 *
 * @version 1.1.0
 * @author BleckWolf25
 * @license MIT
 *
 * @summary Liquid Glass colour palette and rounded-shape geometry for the dock renderer
 *
 * @description
 * Centralises the premultiplied-alpha colour constants that describe the dock's glass body,
 * specular rims, interactive lenses, Start prism panes and accent indicators, so painting code
 * carries no palette data. Also exposes createAsymmetricRoundRect, which builds an
 * ID2D1PathGeometry with independent per-corner radii through ID2D1Factory1 and
 * ID2D1GeometrySink for the crystalline Start emblem.
 *
 * @since 18/11/2026
 * @updated 20/11/2026
 */
#pragma once
// ---------- IMPORTS
#include <d2d1_1.h>
#include <wrl/client.h>
// ---------- CONSTANTS
namespace GlassBar {
inline constexpr D2D1_COLOR_F kBaseTint{0.04f, 0.055f, 0.08f, 0.02f};
inline constexpr D2D1_COLOR_F kSheenTop{1.0f, 1.0f, 1.0f, 0.045f};
inline constexpr D2D1_COLOR_F kSheenMid{1.0f, 1.0f, 1.0f, 0.005f};
inline constexpr D2D1_COLOR_F kSheenBottom{0.78f, 0.86f, 1.0f, 0.015f};
inline constexpr D2D1_COLOR_F kTopSpecular{1.0f, 1.0f, 1.0f, 0.18f};
inline constexpr D2D1_COLOR_F kInnerBevel{1.0f, 1.0f, 1.0f, 0.03f};
inline constexpr D2D1_COLOR_F kLensHoverFill{1.0f, 1.0f, 1.0f, 0.08f};
inline constexpr D2D1_COLOR_F kLensActiveFill{1.0f, 1.0f, 1.0f, 0.12f};
inline constexpr D2D1_COLOR_F kLensDragFill{0.46f, 0.78f, 1.0f, 0.18f};
inline constexpr D2D1_COLOR_F kLensRimTop{1.0f, 1.0f, 1.0f, 0.28f};
inline constexpr D2D1_COLOR_F kLensRimMid{1.0f, 1.0f, 1.0f, 0.10f};
inline constexpr D2D1_COLOR_F kLensRimBottom{1.0f, 1.0f, 1.0f, 0.06f};
inline constexpr D2D1_COLOR_F kWinPaneTop{0.50f, 0.89f, 1.0f, 0.98f};
inline constexpr D2D1_COLOR_F kWinPaneMid{0.31f, 0.75f, 0.98f, 0.96f};
inline constexpr D2D1_COLOR_F kWinPaneBottom{0.17f, 0.60f, 0.94f, 0.96f};
inline constexpr D2D1_COLOR_F kWinPaneHoverTop{0.64f, 0.94f, 1.0f, 1.0f};
inline constexpr D2D1_COLOR_F kWinPaneHoverMid{0.42f, 0.82f, 1.0f, 0.98f};
inline constexpr D2D1_COLOR_F kWinPaneHoverBottom{0.25f, 0.68f, 0.98f, 0.98f};
inline constexpr D2D1_COLOR_F kAccent{0.46f, 0.78f, 1.0f, 1.0f};
inline constexpr D2D1_COLOR_F kAccentGlow{0.46f, 0.78f, 1.0f, 0.30f};
inline constexpr D2D1_COLOR_F kText{1.0f, 1.0f, 1.0f, 0.97f};
inline constexpr D2D1_COLOR_F kTextShadow{0.0f, 0.0f, 0.0f, 0.45f};
inline constexpr D2D1_COLOR_F kMuted{1.0f, 1.0f, 1.0f, 0.70f};
inline constexpr D2D1_COLOR_F kAttentionFill{0.98f, 0.62f, 0.24f, 0.22f};
inline constexpr D2D1_COLOR_F kAttentionRim{0.98f, 0.72f, 0.36f, 0.55f};
inline constexpr D2D1_COLOR_F kBadgeFill{0.98f, 0.42f, 0.30f, 0.95f};
}
// ---------- DECLARATIONS
Microsoft::WRL::ComPtr<ID2D1PathGeometry> createAsymmetricRoundRect(
    ID2D1Factory1* factory, const D2D1_RECT_F& rect, float radiusTopLeft, float radiusTopRight,
    float radiusBottomRight, float radiusBottomLeft);