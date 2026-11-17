/**
 * @file Renderer.h
 *
 * @version 1.0.0
 * @author BleckWolf25
 * @license MIT
 *
 * @summary Direct2D and DirectWrite render context for a single NovaUI window
 *
 * @description
 * Owns the D2D and DirectWrite factories, a per-pixel-alpha HWND render target and one reusable
 * solid brush. The target is created lazily and rebuilt after D2DERR_RECREATE_TARGET. draw runs
 * a paint callback between BeginDraw and EndDraw on a transparent clear so the DWM backdrop shows
 * through. Rendering happens only when the owning window is invalidated.
 *
 * @since 17/11/2026
 * @updated 17/11/2026
 */
#pragma once
// ---------- IMPORTS
#include <Windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <wrl/client.h>

// ---------- DECLARATIONS
class Renderer {
public:
    HRESULT init(HWND hwnd);
    void resize(UINT width, UINT height);
    void setDpi(float dpi);

    ID2D1SolidColorBrush* brush(const D2D1_COLOR_F& color);
    Microsoft::WRL::ComPtr<IDWriteTextFormat> createFormat(float sizeDip, DWRITE_FONT_WEIGHT weight,
                                                           DWRITE_TEXT_ALIGNMENT align) const;

    template <typename Paint>
    HRESULT draw(Paint&& paint) {
        HRESULT hr = ensureTarget();
        if (FAILED(hr)) {
            return hr;
        }
        m_target->BeginDraw();
        m_target->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f));
        paint(*m_target.Get());
        hr = m_target->EndDraw();
        if (hr == D2DERR_RECREATE_TARGET) {
            m_brush.Reset();
            m_target.Reset();
        }
        return hr;
    }

private:
    HRESULT ensureTarget();

    HWND m_hwnd = nullptr;
    float m_dpi = 96.0f;
    Microsoft::WRL::ComPtr<ID2D1Factory> m_d2d;
    Microsoft::WRL::ComPtr<IDWriteFactory> m_dwrite;
    Microsoft::WRL::ComPtr<ID2D1HwndRenderTarget> m_target;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_brush;
};
