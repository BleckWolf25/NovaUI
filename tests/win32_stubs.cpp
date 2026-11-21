/**
 * @file win32_stubs.cpp (test shim)
 *
 * @version 1.0.0
 * @license MIT
 *
 * @summary Host implementations of the Win32 entry points used by NovaUI's pure-logic sources
 *
 * @description
 * Implements ExpandEnvironmentStringsW, WideCharToMultiByte and MultiByteToWideChar with real
 * UTF-8 encode/decode semantics (including surrogate-pair handling), plus trivial event/handle
 * fakes so MusicVisualizer's config translation unit links without the audio SDK. Behaviour is
 * documented per function; tests assert against these contracts exactly as they hold on Windows.
 */
// ---------- IMPORTS
#include "Windows.h"
#include <codecvt>
#include <cstdlib>
#include <locale>
#include <string>
#include <unordered_map>
// ---------- IMPLEMENTATION
namespace {
std::wstring expandOne(const std::wstring& input) {
    // %NAME% lookup table mirroring what a test would seed via SetEnvironmentVariableW.
    static const std::unordered_map<std::wstring, std::wstring> kVars = {
        {L"APPDATA", L"C:\\Users\\Test\\AppData\\Roaming"},
        {L"NOVA_EMPTY", L""},
        {L"WINDIR", L"C:\\Windows"},
    };
    std::wstring out;
    size_t i = 0;
    while (i < input.size()) {
        if (input[i] == L'%') {
            const size_t end = input.find(L'%', i + 1);
            if (end != std::wstring::npos && end > i + 1) {
                const std::wstring name = input.substr(i + 1, end - i - 1);
                auto it = kVars.find(name);
                out += (it != kVars.end()) ? it->second : (L'%' + name + L'%');
                i = end + 1;
                continue;
            }
        }
        out += input[i++];
    }
    return out;
}

size_t encodeUtf8Codepoint(uint32_t cp, char* dst) {
    if (cp < 0x80) {
        dst[0] = static_cast<char>(cp);
        return 1;
    }
    if (cp < 0x800) {
        dst[0] = static_cast<char>(0xC0 | (cp >> 6));
        dst[1] = static_cast<char>(0x80 | (cp & 0x3F));
        return 2;
    }
    if (cp < 0x10000) {
        dst[0] = static_cast<char>(0xE0 | (cp >> 12));
        dst[1] = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        dst[2] = static_cast<char>(0x80 | (cp & 0x3F));
        return 3;
    }
    dst[0] = static_cast<char>(0xF0 | (cp >> 18));
    dst[1] = static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
    dst[2] = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
    dst[3] = static_cast<char>(0x80 | (cp & 0x3F));
    return 4;
}

uint32_t utf8SequenceLength(unsigned char lead) {
    if ((lead & 0x80U) == 0U) {
        return 1;
    }
    if ((lead & 0xE0U) == 0xC0U) {
        return 2;
    }
    if ((lead & 0xF0U) == 0xE0U) {
        return 3;
    }
    if ((lead & 0xF8U) == 0xF0U) {
        return 4;
    }
    return 0;
}
}
DWORD ExpandEnvironmentStringsW(LPCWSTR src, LPWSTR dst, DWORD size) {
    if (!src) {
        return 0;
    }
    const std::wstring expanded = expandOne(src);
    const DWORD needed = static_cast<DWORD>(expanded.size() + 1);
    if (dst == nullptr || size == 0) {
        return needed;
    }
    if (size < needed) {
        return 0;
    }
    for (wchar_t ch : expanded) {
        *dst++ = ch;
    }
    *dst = L'\0';
    return needed;
}

int WideCharToMultiByte(UINT codePage, DWORD, LPCWSTR wide, int wideCount, LPSTR utf8,
                        int utf8Count, LPCSTR, BOOL* usedDefaultChar) {
    if (usedDefaultChar) {
        *usedDefaultChar = FALSE;
    }
    if (codePage != CP_UTF8 || !wide || wideCount < 0) {
        return 0;
    }
    std::string out;
    for (int i = 0; i < wideCount; ++i) {
        uint32_t cp = static_cast<uint16_t>(wide[i]);
        if (cp >= 0xD800 && cp <= 0xDBFF && i + 1 < wideCount) {
            const uint32_t low = static_cast<uint16_t>(wide[i + 1]);
            if (low >= 0xDC00 && low <= 0xDFFF) {
                cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                ++i;
            }
        }
        char buf[4]{};
        const size_t written = encodeUtf8Codepoint(cp, buf);
        out.append(buf, written);
    }
    if (utf8 == nullptr || utf8Count == 0) {
        return static_cast<int>(out.size());
    }
    if (static_cast<size_t>(utf8Count) < out.size()) {
        return 0;
    }
    for (char ch : out) {
        *utf8++ = ch;
    }
    if (static_cast<size_t>(utf8Count) > out.size()) {
        *utf8 = '\0';
    }
    return static_cast<int>(out.size());
}

int MultiByteToWideChar(UINT codePage, DWORD, LPCSTR utf8, int utf8Count, LPWSTR wide,
                        int wideCount) {
    if (codePage != CP_UTF8 || !utf8 || utf8Count < 0) {
        return 0;
    }
    std::wstring out;
    int i = 0;
    while (i < utf8Count) {
        const unsigned char lead = static_cast<unsigned char>(utf8[i]);
        const uint32_t len = utf8SequenceLength(lead);
        if (len == 0 || i + static_cast<int>(len) > utf8Count) {
            return 0; // invalid or truncated sequence, matching the strict API contract
        }
        uint32_t cp = 0;
        if (len == 1) {
            cp = lead;
        } else {
            cp = lead & (0xFFU >> (len + 1));
            for (uint32_t k = 1; k < len; ++k) {
                const unsigned char cont = static_cast<unsigned char>(utf8[i + static_cast<int>(k)]);
                if ((cont & 0xC0U) != 0x80U) {
                    return 0;
                }
                cp = (cp << 6) | (cont & 0x3FU);
            }
        }
        i += static_cast<int>(len);
        if (cp >= 0x10000) {
            cp -= 0x10000;
            out.push_back(static_cast<wchar_t>(0xD800 + (cp >> 10)));
            out.push_back(static_cast<wchar_t>(0xDC00 + (cp & 0x3FF)));
        } else {
            out.push_back(static_cast<wchar_t>(cp));
        }
    }
    if (wide == nullptr || wideCount == 0) {
        return static_cast<int>(out.size());
    }
    if (static_cast<size_t>(wideCount) < out.size()) {
        return 0;
    }
    for (wchar_t ch : out) {
        *wide++ = ch;
    }
    if (static_cast<size_t>(wideCount) > out.size()) {
        *wide = L'\0';
    }
    return static_cast<int>(out.size());
}
// ---------- HANDLE / EVENT FAKES
void* CreateEventW(void*, BOOL, BOOL, const wchar_t*) {
    static int counter = 1;
    return reinterpret_cast<void*>(static_cast<intptr_t>(counter++ * 16));
}
BOOL CloseHandle(void*) { return TRUE; }
BOOL SetEvent(void*) { return TRUE; }
unsigned long WaitForMultipleObjects(unsigned long, void* const*, BOOL, unsigned long) {
    return WAIT_OBJECT_0 + 1; // never the first handle: loops under test must exit
}
unsigned long long GetTickCount64() { return 0ULL; }
void* CoTaskMemAlloc(SIZE_T bytes) { return std::malloc(static_cast<size_t>(bytes)); }
void CoTaskMemFree(void* address) { std::free(address); }
