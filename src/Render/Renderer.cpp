/**
- @file Renderer.cpp
 *
- @version 1.1.0
- @author BleckWolf25
- @license MIT
 *
- @summary Implementation of the DirectComposition and Direct2D render context
 *
- @description
- Builds a D3D11 device, Direct2D device context, premultiplied-alpha composition swap chain
- and DirectComposition visual tree. Resizing recreates the swap chain target buffer in place.
 *
- @since 17/11/2026
- @updated 17/11/2026
 */
// ---------- IMPORTS
#include "Render/Renderer.h"

#include <algorithm>

// ---------- CONSTANTS
namespace {
constexpr const wchar_t* kFontFamily = L"Segoe UI Variable Text";
}

// ---------- IMPLEMENTATION
HRESULT Renderer::init(HWND hwnd) {
    m_hwnd = hwnd;

    HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,
                                   IID_PPV_ARGS(m_d2dFactory.GetAddressOf()));
    if (SUCCEEDED(hr)) {
        hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                                 reinterpret_cast<IUnknown**>(m_dwriteFactory.GetAddressOf()));
    }
    if (SUCCEEDED(hr)) {
        hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                              IID_PPV_ARGS(m_wicFactory.GetAddressOf()));
    }
    return hr;
}

void Renderer::resize(UINT width, UINT height) {
    if (!m_swapChain || width == 0 || height == 0) {
        return;
    }
    m_d2dContext->SetTarget(nullptr);
    m_targetBitmap.Reset();

    if (SUCCEEDED(m_swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0))) {
        createSwapChainTarget(width, height);
        if (m_dcompDevice) {
            m_dcompDevice->Commit();
        }
    }
}

void Renderer::setDpi(float dpi) {
    m_dpi = dpi;
    if (m_d2dContext) {
        m_d2dContext->SetDpi(dpi, dpi);
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
    if (FAILED(m_dwriteFactory->CreateTextFormat(kFontFamily, nullptr, weight, DWRITE_FONT_STYLE_NORMAL,
                                                 DWRITE_FONT_STRETCH_NORMAL, sizeDip, L"en-us",
                                                 format.GetAddressOf()))) {
        return nullptr;
    }
    format->SetTextAlignment(align);
    format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    return format;
}

Microsoft::WRL::ComPtr<ID2D1Bitmap> Renderer::createIconBitmap(HICON icon) {
    if (!icon || !m_wicFactory || FAILED(ensureDevice())) {
        return nullptr;
    }

    Microsoft::WRL::ComPtr<IWICBitmap> wicBitmap;
    if (FAILED(m_wicFactory->CreateBitmapFromHICON(icon, wicBitmap.GetAddressOf()))) {
        return nullptr;
    }

    Microsoft::WRL::ComPtr<IWICFormatConverter> converter;
    if (FAILED(m_wicFactory->CreateFormatConverter(converter.GetAddressOf()))) {
        return nullptr;
    }

    if (FAILED(converter->Initialize(wicBitmap.Get(), GUID_WICPixelFormat32bppPBGRA,
                                     WICBitmapDitherTypeNone, nullptr, 0.0,
                                     WICBitmapPaletteTypeCustom))) {
        return nullptr;
    }

    Microsoft::WRL::ComPtr<ID2D1Bitmap> bitmap;
    m_d2dContext->CreateBitmapFromWicBitmap(converter.Get(), nullptr, bitmap.GetAddressOf());
    return bitmap;
}

HRESULT Renderer::ensureDevice() {
    if (m_d2dContext && m_targetBitmap) {
        return S_OK;
    }

    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, nullptr, 0,
                                   D3D11_SDK_VERSION, m_d3dDevice.GetAddressOf(), nullptr, nullptr);
    if (FAILED(hr)) {
        hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, flags, nullptr, 0,
                               D3D11_SDK_VERSION, m_d3dDevice.GetAddressOf(), nullptr, nullptr);
    }
    if (FAILED(hr)) {
        return hr;
    }

    Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
    hr = m_d3dDevice.As(&dxgiDevice);
    if (FAILED(hr)) {
        resetDevice();
        return hr;
    }

    hr = m_d2dFactory->CreateDevice(dxgiDevice.Get(), m_d2dDevice.GetAddressOf());
    if (SUCCEEDED(hr)) {
        hr = m_d2dDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE,
                                              m_d2dContext.GetAddressOf());
    }
    if (SUCCEEDED(hr)) {
        m_d2dContext->SetDpi(m_dpi, m_dpi);
        m_d2dContext->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
        hr = m_d2dContext->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White),
                                                 m_brush.GetAddressOf());
    }
    if (FAILED(hr)) {
        resetDevice();
        return hr;
    }

    Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
    Microsoft::WRL::ComPtr<IDXGIFactory2> dxgiFactory;
    hr = dxgiDevice->GetAdapter(adapter.GetAddressOf());
    if (SUCCEEDED(hr)) {
        hr = adapter->GetParent(IID_PPV_ARGS(dxgiFactory.GetAddressOf()));
    }
    if (FAILED(hr)) {
        resetDevice();
        return hr;
    }

    RECT rc{};
    GetClientRect(m_hwnd, &rc);
    const UINT width = rc.right > rc.left
                           ? static_cast<UINT>(rc.right - rc.left)
                           : 1u;
    const UINT height = rc.bottom > rc.top
                            ? static_cast<UINT>(rc.bottom - rc.top)
                            : 1u;

    DXGI_SWAP_CHAIN_DESC1 desc{};
    desc.Width = width;
    desc.Height = height;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = 2;
    desc.Scaling = DXGI_SCALING_STRETCH;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    desc.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;

    hr = dxgiFactory->CreateSwapChainForComposition(dxgiDevice.Get(), &desc, nullptr,
                                                    m_swapChain.GetAddressOf());
    if (SUCCEEDED(hr)) {
        hr = createSwapChainTarget(width, height);
    }
    if (SUCCEEDED(hr)) {
        hr = DCompositionCreateDevice(dxgiDevice.Get(), IID_PPV_ARGS(m_dcompDevice.GetAddressOf()));
    }
    if (SUCCEEDED(hr)) {
        hr = m_dcompDevice->CreateTargetForHwnd(m_hwnd, TRUE, m_dcompTarget.GetAddressOf());
    }
    if (SUCCEEDED(hr)) {
        hr = m_dcompDevice->CreateVisual(m_dcompVisual.GetAddressOf());
    }
    if (SUCCEEDED(hr)) {
        hr = m_dcompVisual->SetContent(m_swapChain.Get());
    }
    if (SUCCEEDED(hr)) {
        hr = m_dcompTarget->SetRoot(m_dcompVisual.Get());
    }
    if (SUCCEEDED(hr)) {
        hr = m_dcompDevice->Commit();
    }

    if (FAILED(hr)) {
        resetDevice();
    }
    return hr;
}

HRESULT Renderer::createSwapChainTarget(UINT, UINT) {
    Microsoft::WRL::ComPtr<IDXGISurface> surface;
    HRESULT hr = m_swapChain->GetBuffer(0, IID_PPV_ARGS(surface.GetAddressOf()));
    if (FAILED(hr)) {
        return hr;
    }

    const D2D1_BITMAP_PROPERTIES1 props = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED), m_dpi, m_dpi);

    hr = m_d2dContext->CreateBitmapFromDxgiSurface(surface.Get(), &props,
                                                   m_targetBitmap.GetAddressOf());
    if (SUCCEEDED(hr)) {
        m_d2dContext->SetTarget(m_targetBitmap.Get());
    }
    return hr;
}

void Renderer::resetDevice() {
    m_brush.Reset();
    m_targetBitmap.Reset();
    m_dcompVisual.Reset();
    m_dcompTarget.Reset();
    m_dcompDevice.Reset();
    m_swapChain.Reset();
    if (m_d2dContext) {
        m_d2dContext->SetTarget(nullptr);
    }
    m_d2dContext.Reset();
    m_d2dDevice.Reset();
    m_d3dDevice.Reset();
}