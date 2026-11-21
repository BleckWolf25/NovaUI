/**
 * @file win32_compat.h (test shim)
 *
 * @version 1.0.0
 * @license MIT
 *
 * @summary Bridges the tests/shim Windows.h surface to the declarations NovaUI sources expect
 *
 * @description
 * Included only by test translation units after the shim headers resolve <Windows.h>. Supplies
 * the Win32 function prototypes, D2D1/DWrite/ComPtr stand-in types and small inline fakes used by
 * the pure-logic sources under test (WinString conversions, TaskbarLayout geometry,
 * MusicVisualizer config round-trips). The real SDK headers are used unchanged on MSVC builds;
 * this file exists so the same assertions run on any host compiler.
 */
#pragma once
// ---------- IMPORTS
#include "Windows.h"
#include <cwchar>
#include <string>
#include <vector>
// ---------- CONSTANTS
#define WINBASEAPI
#define WINGDIAPI
typedef void* HMODULE_REAL;
struct IUnknown {
    virtual ~IUnknown() = default;
};
// ---------- WINSTRING DEPENDENCIES
DWORD ExpandEnvironmentStringsW(LPCWSTR src, LPWSTR dst, DWORD size);
int WideCharToMultiByte(UINT codePage, DWORD flags, LPCWSTR wide, int wideCount, LPSTR utf8,
                        int utf8Count, LPCSTR defaultChar, BOOL* usedDefaultChar);
int MultiByteToWideChar(UINT codePage, DWORD flags, LPCSTR utf8, int utf8Count, LPWSTR wide,
                        int wideCount);
// ---------- KERNEL / HANDLE STUBS
void* CreateEventW(void*, BOOL, BOOL, const wchar_t*);
BOOL CloseHandle(void*);
BOOL SetEvent(void*);
unsigned long WaitForMultipleObjects(unsigned long count, void* const* handles, BOOL waitAll,
                                     unsigned long ms);
unsigned long long GetTickCount64();
// ---------- D2D1 SHIM TYPES (geometry math only; no rendering is exercised)
struct D2D1_RECT_F {
    float left = 0.0f;
    float top = 0.0f;
    float right = 0.0f;
    float bottom = 0.0f;
};
struct D2D1_POINT_2F {
    float x = 0.0f;
    float y = 0.0f;
};
namespace D2D1 {
inline D2D1_RECT_F RectF(float l, float t, float r, float b) {
    return D2D1_RECT_F{l, t, r, b};
}
}
struct ID2D1Bitmap : IUnknown {};
struct IDWriteTextFormat : IUnknown {};
// ---------- WRL ComPtr SHIM (pointer semantics are enough for unit-level state tests)
namespace Microsoft::WRL {
template <typename T>
class ComPtr {
public:
    ComPtr() noexcept = default;
    ComPtr(T* ptr) noexcept : m_ptr(ptr) {}
    T* Get() const noexcept { return m_ptr; }
    T** GetAddressOf() noexcept { return &m_ptr; }
    explicit operator bool() const noexcept { return m_ptr != nullptr; }
    T* operator->() const noexcept { return m_ptr; }
    T& operator*() const noexcept { return *m_ptr; }
    void Reset() noexcept { m_ptr = nullptr; }
    bool operator==(const ComPtr& other) const noexcept { return m_ptr == other.m_ptr; }
private:
    T* m_ptr = nullptr;
};
}
// ---------- MMSYS STUB TYPES (MusicVisualizer translation unit compiles without the audio SDK)
struct WAVEFORMATEX {
    WORD wFormatTag;
    WORD nChannels;
    DWORD nSamplesPerSec;
    DWORD nAvgBytesPerSec;
    WORD nBlockAlign;
    WORD wBitsPerSample;
    WORD cbSize;
};
struct WAVEFORMATEXTENSIBLE {
    WAVEFORMATEX Format;
    union {
        WORD wValidBitsPerSample;
        WORD wSamplesPerBlock;
        WORD wReserved;
    } Samples;
    DWORD dwChannelMask;
    struct GUID {
        unsigned long Data1;
        unsigned short Data2;
        unsigned short Data3;
        unsigned char Data4[8];
    } SubFormat;
};
struct GUID_SHIM {
    unsigned long Data1;
    unsigned short Data2;
    unsigned short Data3;
    unsigned char Data4[8];
};
#define WAVE_FORMAT_IEEE_FLOAT 0x0003
#define WAVE_FORMAT_EXTENSIBLE 0xFFFE
inline GUID_SHIM KSDATAFORMAT_SUBTYPE_IEEE_FLOAT = {0x00000003, 0, 0,
                                                    {0, 0, 0, 0, 0, 0, 0, 0}};
