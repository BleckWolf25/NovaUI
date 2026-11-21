/**
 * @file Windows.h (test shim)
 *
 * @summary Minimal Win32 type and constant surface for host-side unit tests
 *
 * @description
 * Provides just enough of <Windows.h>, <shellapi.h> and related SDK headers that the pure logic
 * units (WinString, TaskbarLayout, MusicVisualizer config, ShortcutStore keys, TaskTracker item
 * accessors) compile on a non-Windows host. Function declarations are intentionally absent: any
 * test target that pulls in a Win32-dependent translation unit must supply its own stubs.
 */
#pragma once
// ---------- INCLUDES (host)
#include <cstddef>
#include <cstdint>
#include <string>
// ---------- BASIC TYPES
using BYTE = unsigned char;
using WORD = uint16_t;
using DWORD = uint32_t;
using LONG = int32_t;
using ULONG = uint32_t;
using UINT = uint32_t;
using INT = int32_t;
using BOOL = int;
using BOOLEAN = BYTE;
using CHAR = char;
using WCHAR = wchar_t;
using HANDLE = void*;
using HWND = struct HWND__*;
using HMODULE = struct HMODULE__*;
using HINSTANCE = HMODULE;
using HKEY = struct HKEY__*;
using HICON = struct HICON__*;
using HBITMAP = struct HBITMAP__*;
using HGDIOBJ = void*;
using HRESULT = long;
using LPVOID = void*;
using LPCVOID = const void*;
using LPSTR = char*;
using LPCSTR = const char*;
using LPWSTR = wchar_t*;
using LPCWSTR = const wchar_t*;
using WPARAM = uint64_t;
using LPARAM = int64_t;
using LRESULT = int64_t;
using UINT_PTR = uint64_t;
using DWORD_PTR = uint64_t;
using LONG_PTR = int64_t;
using SIZE_T = uint64_t;
#define INVALID_HANDLE_VALUE ((HANDLE)(LONG_PTR)-1)
struct POINT {
    LONG x;
    LONG y;
};
struct RECT {
    LONG left;
    LONG top;
    LONG right;
    LONG bottom;
};
// ---------- CONSTANTS
#ifndef TRUE
#    define TRUE 1
#endif
#ifndef FALSE
#    define FALSE 0
#endif
#define MAX_PATH 260
#define ERROR_SUCCESS 0L
#define ERROR_INSUFFICIENT_BUFFER 122L
#define INFINITE 0xFFFFFFFFU
#define WAIT_OBJECT_0 0
#define WM_APP 0x0400
#define SW_SHOW 5
#define SW_SHOWNOACTIVATE 4
#define ABE_LEFT 0
#define ABE_TOP 1
#define ABE_RIGHT 2
#define ABE_BOTTOM 3
#define KEY_READ 0x20019
#define KEY_SET_VALUE 0x0002
#define REG_DWORD 0x4
#define REG_BINARY 0x3
#define HKEY_CURRENT_USER ((HKEY)nullptr)
#define S_OK ((HRESULT)0L)
#define SUCCEEDED(hr) (((HRESULT)(hr)) >= 0)
#define FAILED(hr) (((HRESULT)(hr)) < 0)
#define CP_UTF8 65001
// ---------- FUNCTION PROTOTYPES (implemented by tests/win32_stubs.cpp)
DWORD ExpandEnvironmentStringsW(LPCWSTR src, LPWSTR dst, DWORD size);
int WideCharToMultiByte(UINT codePage, DWORD flags, LPCWSTR wide, int wideCount, LPSTR utf8,
                        int utf8Count, LPCSTR defaultChar, BOOL* usedDefaultChar);
int MultiByteToWideChar(UINT codePage, DWORD flags, LPCSTR utf8, int utf8Count, LPWSTR wide,
                        int wideCount);
void* CreateEventW(void*, BOOL, BOOL, const wchar_t*);
BOOL CloseHandle(void*);
BOOL SetEvent(void*);
unsigned long WaitForMultipleObjects(unsigned long count, void* const* handles, BOOL waitAll,
                                     unsigned long ms);
unsigned long long GetTickCount64();
void* CoTaskMemAlloc(SIZE_T bytes);
void CoTaskMemFree(void* address);
// ---------- SDK STRUCTS
struct APPBARDATA {
    DWORD cbSize;
    HWND hWnd;
    UINT uCallbackMessage;
    UINT uEdge;
    RECT rc;
    LONG_PTR lParam;
};
