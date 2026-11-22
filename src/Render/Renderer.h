/**
 * @file Renderer.h
 *
 * @version 1.3.0
 * @author BleckWolf25
 * @license MIT
 *
 * @summary Direct3D 11, DirectComposition and Direct2D per-pixel-alpha render context
 *
 * @description
 * Renders into a DXGI_ALPHA_MODE_PREMULTIPLIED composition swap chain bound to the window via
 * DirectComposition. Exposes ID2D1DeviceContext, multi-stop vertical gradient brushes for
 * Liquid Glass specular highlights and Fresnel rims, and WIC high-DPI icon conversion.
 *
 * @since 17/11/2026
 * @updated 17/11/2026
 */
#pragma once
// ---------- IMPORTS
#include <Windows.h>
#include <d2d1_1.h>
#include <d3d11.h>
#include <dcomp.h>
#include <dxgi1_2.h>
#include <dwrite.h>
#include <wincodec.h>
#include <wrl/client.h>

// ---------- DECLARATIONS
class Renderer {
public:
    HRESULT init(HWND hwnd);
    void resize(UINT width, UINT height);
    void setDpi(float dpi);

    ID2D1Factory1* factory() const noexcept { return m_d2dFactory.Get(); }

    ID2D1SolidColorBrush* brush(const D2D1_COLOR_F& color);
    Microsoft::WRL::ComPtr<ID2D1LinearGradientBrush> verticalGradient(
        float topY, float bottomY, const D2D1_COLOR_F& topColor, const D2D1_COLOR_F& midColor,
        float midStop, const D2D1_COLOR_F& bottomColor) const;

    Microsoft::WRL::ComPtr<IDWriteTextFormat> createFormat(float sizeDip, DWRITE_FONT_WEIGHT weight,
                                                           DWRITE_TEXT_ALIGNMENT align) const;
    Microsoft::WRL::ComPtr<ID2D1Bitmap> createIconBitmap(HICON icon);
    Microsoft::WRL::ComPtr<ID2D1Bitmap> createBitmapFromHBitmap(HBITMAP bitmap);

    template <typename Paint>
    HRESULT draw(Paint&& paint) {
        HRESULT hr = ensureDevice();
        if (FAILED(hr)) {
            return hr;
        }
        m_d2dContext->BeginDraw();
        m_d2dContext->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f));
        paint(*m_d2dContext.Get());
        hr = m_d2dContext->EndDraw();

        if (SUCCEEDED(hr)) {
            hr = m_swapChain->Present(1, 0);
        }
        if (hr == D2DERR_RECREATE_TARGET || hr == DXGI_ERROR_DEVICE_REMOVED ||
            hr == DXGI_ERROR_DEVICE_RESET) {
            resetDevice();
            return D2DERR_RECREATE_TARGET;
        }
        return hr;
    }

private:
    HRESULT ensureDevice();
    HRESULT createSwapChainTarget(UINT width, UINT height);
    void resetDevice();

    HWND m_hwnd = nullptr;
    float m_dpi = 96.0f;

    Microsoft::WRL::ComPtr<ID2D1Factory1> m_d2dFactory;
    Microsoft::WRL::ComPtr<IDWriteFactory> m_dwriteFactory;
    Microsoft::WRL::ComPtr<IWICImagingFactory> m_wicFactory;

    Microsoft::WRL::ComPtr<ID3D11Device> m_d3dDevice;
    Microsoft::WRL::ComPtr<ID2D1Device> m_d2dDevice;
    Microsoft::WRL::ComPtr<ID2D1DeviceContext> m_d2dContext;
    Microsoft::WRL::ComPtr<IDXGISwapChain1> m_swapChain;
    Microsoft::WRL::ComPtr<ID2D1Bitmap1> m_targetBitmap;
    Microsoft::WRL::ComPtr<IDCompositionDevice> m_dcompDevice;
    Microsoft::WRL::ComPtr<IDCompositionTarget> m_dcompTarget;
    Microsoft::WRL::ComPtr<IDCompositionVisual> m_dcompVisual;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_brush;
};