/**
 * @file Renderer.cpp
 *
 * @version 1.0.0
 * @author BleckWolf25
 * @license MIT
 *
 * @summary Implementation of the Direct2D and DirectWrite render context
 *
 * @description
 * Creates the device-independent factories once and the device-dependent HWND render target on
 * demand with premultiplied alpha and grayscale text antialiasing, which is required for text on
 * a transparent surface. Text formats are device-independent and survive target recreation.
 *
 * @since 17/11/2026
 * @updated 17/11/2026
 */
// ---------- IMPORTS
#include "Render/Renderer.h"

// ---------- CONSTANTS
namespace {
constexpr const wchar_t* kFontFamily = L"Segoe UI Variable Text";
}

// ---------- IMPLEMENTATION
HRESULT Renderer::init(HWND hwnd) {
    m_hwnd = hwnd;
    HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, m_d2d.GetAddressOf());
    if (SUCCEEDED(hr)) {
        hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                                 reinterpret_cast<IUnknown**>(m_dwrite.GetAddressOf()));
    }
    return hr;
}

void Renderer::resize(UINT width, UINT height) {
    if (m_target && width > 0 && height > 0) {
        m_target->Resize(D2D1::SizeU(width, height));
    }
}

void Renderer::setDpi(float dpi) {
    m_dpi = dpi;
    if (m_target) {
        m_target->SetDpi(dpi, dpi);
    }
}

ID2D1SolidColorBrush* Renderer::brush(const D2D1_COLOR_F& color) {
    m_brush->SetColor(color);
    return m_brush.Get();
}

Microsoft::WRL::ComPtr<IDWriteTextFormat> Renderer::createFormat(float sizeDip,
                                                                 DWRITE_FONT_WEIGHT weight,
                                                                 DWRITE_TEXT_ALIGNMENT align) const {
    Microsoft::WRL::ComPtr<IDWriteTextFormat> format;
    if (FAILED(m_dwrite->CreateTextFormat(kFontFamily, nullptr, weight, DWRITE_FONT_STYLE_NORMAL,
                                          DWRITE_FONT_STRETCH_NORMAL, sizeDip, L"en-us",
                                          format.GetAddressOf()))) {
        return nullptr;
    }
    format->SetTextAlignment(align);
    format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    return format;
}

HRESULT Renderer::ensureTarget() {
    if (m_target) {
        return S_OK;
    }

    RECT rc{};
    GetClientRect(m_hwnd, &rc);
    const auto props = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_DEFAULT,
        D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_PREMULTIPLIED), m_dpi, m_dpi);
    const auto hwndProps = D2D1::HwndRenderTargetProperties(
        m_hwnd, D2D1::SizeU(static_cast<UINT32>(rc.right - rc.left),
                            static_cast<UINT32>(rc.bottom - rc.top)));

    HRESULT hr = m_d2d->CreateHwndRenderTarget(props, hwndProps, m_target.GetAddressOf());
    if (SUCCEEDED(hr)) {
        m_target->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
        hr = m_target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White),
                                             m_brush.GetAddressOf());
    }
    if (FAILED(hr)) {
        m_brush.Reset();
        m_target.Reset();
    }
    return hr;
}
