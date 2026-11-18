/**
 * @file WinResources.h
 *
 * @version 1.0.0
 * @author BleckWolf25
 * @license MIT
 *
 * @summary RAII ownership wrappers for Win32 kernel, shell and GDI resources
 *
 * @description
 * Provides UniqueHandle for HANDLE values that may be either nullptr or INVALID_HANDLE_VALUE on
 * failure, plus deleter-based aliases for HICON, HBITMAP, HKEY and CoTaskMem-allocated strings.
 * Every wrapper releases through its native API (CloseHandle, DestroyIcon, DeleteObject,
 * RegCloseKey, CoTaskMemFree) so shell enumeration paths cannot leak on early returns.
 * UniqueChangeNotification releases FindFirstChangeNotification handles through
 * FindCloseChangeNotification.
 *
 * @since 18/11/2026
 * @updated 18/11/2026
 */
#pragma once
// ---------- IMPORTS
#include <Windows.h>
#include <objbase.h>
#include <memory>
#include <type_traits>
// ---------- TYPES
class UniqueHandle {
public:
    UniqueHandle() noexcept = default;
    explicit UniqueHandle(HANDLE handle) noexcept : m_handle(handle) {}
    ~UniqueHandle() { reset(); }
    UniqueHandle(const UniqueHandle&) = delete;
    UniqueHandle& operator=(const UniqueHandle&) = delete;
    UniqueHandle(UniqueHandle&& other) noexcept : m_handle(other.release()) {}
    UniqueHandle& operator=(UniqueHandle&& other) noexcept {
        if (this != &other) {
            reset(other.release());
        }
        return *this;
    }
    HANDLE get() const noexcept { return m_handle; }
    bool valid() const noexcept {
        return m_handle != INVALID_HANDLE_VALUE && m_handle != nullptr;
    }
    explicit operator bool() const noexcept { return valid(); }
    void reset(HANDLE handle = INVALID_HANDLE_VALUE) noexcept {
        if (valid()) {
            CloseHandle(m_handle);
        }
        m_handle = handle;
    }
    HANDLE release() noexcept {
        const HANDLE handle = m_handle;
        m_handle = INVALID_HANDLE_VALUE;
        return handle;
    }
private:
    HANDLE m_handle = INVALID_HANDLE_VALUE;
};

class UniqueChangeNotification {
public:
    UniqueChangeNotification() noexcept = default;
    explicit UniqueChangeNotification(HANDLE handle) noexcept : m_handle(handle) {}
    ~UniqueChangeNotification() { reset(); }
    UniqueChangeNotification(const UniqueChangeNotification&) = delete;
    UniqueChangeNotification& operator=(const UniqueChangeNotification&) = delete;
    UniqueChangeNotification(UniqueChangeNotification&& other) noexcept
        : m_handle(other.release()) {}
    UniqueChangeNotification& operator=(UniqueChangeNotification&& other) noexcept {
        if (this != &other) {
            reset(other.release());
        }
        return *this;
    }
    HANDLE get() const noexcept { return m_handle; }
    bool valid() const noexcept {
        return m_handle != INVALID_HANDLE_VALUE && m_handle != nullptr;
    }
    void reset(HANDLE handle = INVALID_HANDLE_VALUE) noexcept {
        if (valid()) {
            FindCloseChangeNotification(m_handle);
        }
        m_handle = handle;
    }
    HANDLE release() noexcept {
        const HANDLE handle = m_handle;
        m_handle = INVALID_HANDLE_VALUE;
        return handle;
    }
private:
    HANDLE m_handle = INVALID_HANDLE_VALUE;
};

struct IconDeleter {
    void operator()(HICON icon) const noexcept {
        if (icon) {
            DestroyIcon(icon);
        }
    }
};

struct BitmapDeleter {
    void operator()(HBITMAP bitmap) const noexcept {
        if (bitmap) {
            DeleteObject(bitmap);
        }
    }
};

struct RegistryDeleter {
    void operator()(HKEY key) const noexcept {
        if (key) {
            RegCloseKey(key);
        }
    }
};

struct CoTaskMemDeleter {
    void operator()(wchar_t* text) const noexcept {
        if (text) {
            CoTaskMemFree(text);
        }
    }
};

using UniqueIcon = std::unique_ptr<std::remove_pointer_t<HICON>, IconDeleter>;
using UniqueBitmap = std::unique_ptr<std::remove_pointer_t<HBITMAP>, BitmapDeleter>;
using UniqueRegistryKey = std::unique_ptr<std::remove_pointer_t<HKEY>, RegistryDeleter>;
using CoTaskMemString = std::unique_ptr<wchar_t, CoTaskMemDeleter>;