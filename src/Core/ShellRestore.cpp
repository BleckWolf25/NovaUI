/**
 * @file ShellRestore.cpp
 *
 * @version 1.0.0
 * @author BleckWolf25
 * @license MIT
 *
 * @summary Implementation of the shell preference snapshot and restore guard
 *
 * @description
 * Serialises the AppBar state word followed by the raw StuckRects3 blob into a single
 * configuration file. apply writes the blob back with RegSetValueExW, re-applies ABM_SETSTATE and
 * re-shows the native trays. spawnGuard launches this executable with a guard argument through
 * CreateProcessW; guardMain waits on the parent handle with WaitForSingleObject and then applies
 * the snapshot, consuming no CPU while the shell runs.
 *
 * @since 18/11/2026
 * @updated 18/11/2026
 */
// ---------- IMPORTS
#include "Core/ShellRestore.h"
#include "Core/ShellPaths.h"
#include "Core/WinResources.h"
#include <shellapi.h>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

// ---------- CONSTANTS
namespace {
constexpr wchar_t kSnapshotFile[] = L"shell_restore.bin";
constexpr wchar_t kStuckRectsKey[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StuckRects3";
constexpr wchar_t kGuardArgument[] = L" --guard ";

std::vector<BYTE> readStuckRects() {
    std::vector<BYTE> blob;
    HKEY rawKey = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kStuckRectsKey, 0, KEY_READ, &rawKey) != ERROR_SUCCESS) {
        return blob;
    }
    const UniqueRegistryKey key(rawKey);
    DWORD type = 0;
    DWORD bytes = 0;
    if (RegQueryValueExW(key.get(), L"Settings", nullptr, &type, nullptr, &bytes) !=
            ERROR_SUCCESS ||
        type != REG_BINARY || bytes == 0) {
        return blob;
    }
    blob.resize(bytes);
    if (RegQueryValueExW(key.get(), L"Settings", nullptr, nullptr, blob.data(), &bytes) !=
        ERROR_SUCCESS) {
        blob.clear();
        return blob;
    }
    blob.resize(bytes);
    return blob;
}

void writeStuckRects(const std::vector<BYTE>& blob) {
    if (blob.empty()) {
        return;
    }
    HKEY rawKey = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kStuckRectsKey, 0, nullptr, 0, KEY_SET_VALUE, nullptr,
                        &rawKey, nullptr) != ERROR_SUCCESS) {
        return;
    }
    const UniqueRegistryKey key(rawKey);
    RegSetValueExW(key.get(), L"Settings", 0, REG_BINARY, blob.data(),
                   static_cast<DWORD>(blob.size()));
}

void showNativeTrays() {
    if (HWND primary = FindWindowW(L"Shell_TrayWnd", nullptr)) {
        ShowWindow(primary, SW_SHOW);
    }
    HWND secondary = nullptr;
    while ((secondary = FindWindowExW(nullptr, secondary, L"Shell_SecondaryTrayWnd", nullptr)) !=
           nullptr) {
        ShowWindow(secondary, SW_SHOW);
    }
}
}
// ---------- IMPLEMENTATION
namespace ShellRestore {

void capture() {
    const std::wstring path = novaConfigFilePath(kSnapshotFile);
    if (path.empty()) {
        return;
    }
    const std::vector<BYTE> blob = readStuckRects();
    UINT_PTR state = 0;
    if (HWND primary = FindWindowW(L"Shell_TrayWnd", nullptr)) {
        APPBARDATA abd{};
        abd.cbSize = sizeof(abd);
        abd.hWnd = primary;
        state = SHAppBarMessage(ABM_GETSTATE, &abd);
    }
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        return;
    }
    out.write(reinterpret_cast<const char*>(&state), sizeof(state));
    if (!blob.empty()) {
        out.write(reinterpret_cast<const char*>(blob.data()),
                  static_cast<std::streamsize>(blob.size()));
    }
}

void apply() {
    const std::wstring path = novaConfigFilePath(kSnapshotFile);
    if (path.empty()) {
        return;
    }
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) {
        return;
    }
    UINT_PTR state = 0;
    in.read(reinterpret_cast<char*>(&state), sizeof(state));
    std::vector<BYTE> blob((std::istreambuf_iterator<char>(in)),
                           std::istreambuf_iterator<char>());
    in.close();
    writeStuckRects(blob);
    if (HWND primary = FindWindowW(L"Shell_TrayWnd", nullptr)) {
        APPBARDATA abd{};
        abd.cbSize = sizeof(abd);
        abd.hWnd = primary;
        abd.lParam = static_cast<LPARAM>(state);
        SHAppBarMessage(ABM_SETSTATE, &abd);
    }
    showNativeTrays();
    std::error_code ec;
    std::filesystem::remove(path, ec);
}

void spawnGuard(DWORD parentPid) {
    wchar_t modulePath[MAX_PATH]{};
    if (GetModuleFileNameW(nullptr, modulePath, MAX_PATH) == 0) {
        return;
    }
    std::wstring commandLine = L"\"";
    commandLine += modulePath;
    commandLine += L"\"";
    commandLine += kGuardArgument;
    commandLine += std::to_wstring(parentPid);
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (CreateProcessW(modulePath, commandLine.data(), nullptr, nullptr, FALSE,
                       CREATE_NO_WINDOW | DETACHED_PROCESS, nullptr, nullptr, &startup,
                       &process)) {
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
    }
}

int guardMain(DWORD parentPid) {
    const UniqueHandle parent(OpenProcess(SYNCHRONIZE, FALSE, parentPid));
    if (parent.valid()) {
        WaitForSingleObject(parent.get(), INFINITE);
    }
    apply();
    return 0;
}

}