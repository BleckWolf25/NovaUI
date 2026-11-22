/**
 * @file Glass.cpp
 *
 * @version 1.0.0
 * @author BleckWolf25
 * @license MIT
 *
 * @summary Implementation of per-corner rounded geometry for the Liquid Glass dock
 *
 * @description
 * Walks a rectangle clockwise emitting a line and an optional arc per corner so each corner can
 * carry its own radius. The resulting ID2D1PathGeometry is filled and stroked by the Start emblem
 * renderer. Returns an empty ComPtr when the factory is unavailable or any sink step fails.
 *
 * @since 18/11/2026
 * @updated 18/11/2026
 */
// ---------- IMPORTS
#include "Render/Glass.h"
// ---------- IMPLEMENTATION
Microsoft::WRL::ComPtr<ID2D1PathGeometry> createAsymmetricRoundRect(
    ID2D1Factory1* factory, const D2D1_RECT_F& rect, float radiusTopLeft, float radiusTopRight,
    float radiusBottomRight, float radiusBottomLeft) {
    if (!factory) {
        return nullptr;
    }
    Microsoft::WRL::ComPtr<ID2D1PathGeometry> geometry;
    if (FAILED(factory->CreatePathGeometry(geometry.GetAddressOf()))) {
        return nullptr;
    }
    Microsoft::WRL::ComPtr<ID2D1GeometrySink> sink;
    if (FAILED(geometry->Open(sink.GetAddressOf()))) {
        return nullptr;
    }
    sink->BeginFigure(D2D1::Point2F(rect.left + radiusTopLeft, rect.top),
                      D2D1_FIGURE_BEGIN_FILLED);
    sink->AddLine(D2D1::Point2F(rect.right - radiusTopRight, rect.top));
    if (radiusTopRight > 0.0f) {
        sink->AddArc(D2D1::ArcSegment(D2D1::Point2F(rect.right, rect.top + radiusTopRight),
                                      D2D1::SizeF(radiusTopRight, radiusTopRight), 0.0f,
                                      D2D1_SWEEP_DIRECTION_CLOCKWISE, D2D1_ARC_SIZE_SMALL));
    }
    sink->AddLine(D2D1::Point2F(rect.right, rect.bottom - radiusBottomRight));
    if (radiusBottomRight > 0.0f) {
        sink->AddArc(D2D1::ArcSegment(D2D1::Point2F(rect.right - radiusBottomRight, rect.bottom),
                                      D2D1::SizeF(radiusBottomRight, radiusBottomRight), 0.0f,
                                      D2D1_SWEEP_DIRECTION_CLOCKWISE, D2D1_ARC_SIZE_SMALL));
    }
    sink->AddLine(D2D1::Point2F(rect.left + radiusBottomLeft, rect.bottom));
    if (radiusBottomLeft > 0.0f) {
        sink->AddArc(D2D1::ArcSegment(D2D1::Point2F(rect.left, rect.bottom - radiusBottomLeft),
                                      D2D1::SizeF(radiusBottomLeft, radiusBottomLeft), 0.0f,
                                      D2D1_SWEEP_DIRECTION_CLOCKWISE, D2D1_ARC_SIZE_SMALL));
    }
    sink->AddLine(D2D1::Point2F(rect.left, rect.top + radiusTopLeft));
    if (radiusTopLeft > 0.0f) {
        sink->AddArc(D2D1::ArcSegment(D2D1::Point2F(rect.left + radiusTopLeft, rect.top),
                                      D2D1::SizeF(radiusTopLeft, radiusTopLeft), 0.0f,
                                      D2D1_SWEEP_DIRECTION_CLOCKWISE, D2D1_ARC_SIZE_SMALL));
    }
    sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    if (FAILED(sink->Close())) {
        return nullptr;
    }
    return geometry;
}