/**
 * @file ShortcutStore.cpp
 *
 * @version 1.0.0
 * @author BleckWolf25
 * @license MIT
 *
 * @summary Implementation of shortcut metadata, pin creation and pin order persistence
 *
 * @description
 * Loads shell links with IPersistFile in STGM_READ, resolves raw targets and the
 * PKEY_AppUserModel_ID property, and writes pins with SetPath, SetWorkingDirectory and a
 * CoTaskMem-allocated PROPVARIANT that PropVariantClear releases. The Taskband Favorites blob is
 * walked as a chain of SHITEMID records terminated by a zero cb, so every entry stays aligned and
 * each name comes from SHGetNameFromIDList. Registry and shell strings are RAII-owned.
 *
 * @since 18/11/2026
 * @updated 18/11/2026
 */
// ---------- IMPORTS
#include "Modules/ShortcutStore.h"
#include "Core/ShellPaths.h"
#include "Core/WinResources.h"
#include "Core/WinString.h"
#include <Windows.h>
#include <propkey.h>
#include <propsys.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <wrl/client.h>
#include <cstring>
#include <fstream>

// ---------- CONSTANTS
namespace {
constexpr wchar_t kTaskbandKey[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Taskband";
constexpr wchar_t kPinOrderFile[] = L"pins.txt";
}
// ---------- IMPLEMENTATION
ShortcutMetadata resolveShortcutMetadata(const std::wstring& shortcutPath) {
    ShortcutMetadata metadata{};
    Microsoft::WRL::ComPtr<IShellLinkW> shellLink;
    if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(shellLink.GetAddressOf())))) {
        return metadata;
    }
    Microsoft::WRL::ComPtr<IPersistFile> persistFile;
    if (FAILED(shellLink.As(&persistFile)) ||
        FAILED(persistFile->Load(shortcutPath.c_str(), STGM_READ))) {
        return metadata;
    }
    wchar_t targetBuffer[MAX_PATH]{};
    if (SUCCEEDED(shellLink->GetPath(targetBuffer, MAX_PATH, nullptr, SLGP_RAWPATH)) &&
        targetBuffer[0] != L'\0') {
        metadata.targetPathLower = toLowerWide(expandEnvironmentStrings(targetBuffer));
    }
    Microsoft::WRL::ComPtr<IPropertyStore> propertyStore;
    if (SUCCEEDED(shellLink.As(&propertyStore))) {
        PROPVARIANT variant{};
        PropVariantInit(&variant);
        if (SUCCEEDED(propertyStore->GetValue(PKEY_AppUserModel_ID, &variant)) &&
            variant.vt == VT_LPWSTR && variant.pwszVal != nullptr) {
            metadata.appUserModelIdLower = toLowerWide(variant.pwszVal);
        }
        PropVariantClear(&variant);
    }
    return metadata;
}

std::wstring makeShortcutKey(const ShortcutMetadata& metadata, const std::wstring& shortcutPath) {
    if (!metadata.appUserModelIdLower.empty()) {
        return L"aumid:" + metadata.appUserModelIdLower;
    }
    if (!metadata.targetPathLower.empty()) {
        return L"exe:" + metadata.targetPathLower;
    }
    return L"lnk:" + toLowerWide(shortcutPath);
}

bool createShortcutFile(const std::wstring& shortcutPath, const std::wstring& targetPath,
                        const std::wstring& appUserModelId, const std::wstring& description) {
    Microsoft::WRL::ComPtr<IShellLinkW> shellLink;
    if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(shellLink.GetAddressOf())))) {
        return false;
    }
    if (!targetPath.empty()) {
        shellLink->SetPath(targetPath.c_str());
        const std::filesystem::path target(targetPath);
        if (target.has_parent_path()) {
            shellLink->SetWorkingDirectory(target.parent_path().c_str());
        }
    }
    if (!description.empty()) {
        shellLink->SetDescription(description.c_str());
    }
    if (!appUserModelId.empty()) {
        Microsoft::WRL::ComPtr<IPropertyStore> propertyStore;
        if (SUCCEEDED(shellLink.As(&propertyStore))) {
            PROPVARIANT variant{};
            variant.vt = VT_LPWSTR;
            const size_t bytes = (appUserModelId.size() + 1) * sizeof(wchar_t);
            variant.pwszVal = static_cast<LPWSTR>(CoTaskMemAlloc(bytes));
            if (variant.pwszVal) {
                std::memcpy(variant.pwszVal, appUserModelId.c_str(), bytes);
                propertyStore->SetValue(PKEY_AppUserModel_ID, variant);
                propertyStore->Commit();
            }
            PropVariantClear(&variant);
        }
    }
    Microsoft::WRL::ComPtr<IPersistFile> persistFile;
    if (FAILED(shellLink.As(&persistFile))) {
        return false;
    }
    return SUCCEEDED(persistFile->Save(shortcutPath.c_str(), TRUE));
}

std::filesystem::path uniqueShortcutPath(const std::wstring& directory,
                                         const std::wstring& baseName) {
    std::error_code ec;
    const std::wstring stem = sanitizeFileName(baseName);
    std::filesystem::path candidate = std::filesystem::path(directory) / (stem + L".lnk");
    int suffix = 2;
    while (std::filesystem::exists(candidate, ec)) {
        candidate = std::filesystem::path(directory) /
                    (stem + L" (" + std::to_wstring(suffix++) + L").lnk");
    }
    return candidate;
}

std::vector<std::wstring> readTaskbandFavoriteNames() {
    std::vector<std::wstring> names;
    HKEY rawKey = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kTaskbandKey, 0, KEY_READ, &rawKey) != ERROR_SUCCESS) {
        return names;
    }
    const UniqueRegistryKey key(rawKey);
    DWORD type = 0;
    DWORD byteCount = 0;
    if (RegQueryValueExW(key.get(), L"Favorites", nullptr, &type, nullptr, &byteCount) !=
            ERROR_SUCCESS ||
        type != REG_BINARY || byteCount < 4) {
        return names;
    }
    std::vector<BYTE> blob(byteCount);
    if (RegQueryValueExW(key.get(), L"Favorites", nullptr, nullptr, blob.data(), &byteCount) !=
        ERROR_SUCCESS) {
        return names;
    }
    blob.resize(byteCount);
    size_t pos = 0;
    while (pos + sizeof(USHORT) <= blob.size()) {
        if (blob[pos] == 0xFF) {
            break;
        }
        USHORT first = 0;
        std::memcpy(&first, blob.data() + pos, sizeof(USHORT));
        if (first == 0) {
            pos += sizeof(USHORT);
            continue;
        }
        const size_t pidlStart = pos;
        bool valid = false;
        while (pos + sizeof(USHORT) <= blob.size()) {
            USHORT cb = 0;
            std::memcpy(&cb, blob.data() + pos, sizeof(USHORT));
            if (cb == 0) {
                pos += sizeof(USHORT);
                valid = pos > pidlStart + sizeof(USHORT);
                break;
            }
            if (cb < sizeof(USHORT) || pos + cb > blob.size()) {
                pos = blob.size();
                break;
            }
            pos += cb;
        }
        if (!valid) {
            break;
        }
        const auto* pidl = reinterpret_cast<LPCITEMIDLIST>(blob.data() + pidlStart);
        PWSTR displayName = nullptr;
        if (SUCCEEDED(SHGetNameFromIDList(pidl, SIGDN_NORMALDISPLAY, &displayName)) &&
            displayName) {
            const CoTaskMemString owned(displayName);
            names.push_back(toLowerWide(owned.get()));
        }
    }
    return names;
}

std::vector<std::wstring> readPinOrder() {
    std::vector<std::wstring> order;
    const std::wstring path = novaConfigFilePath(kPinOrderFile);
    if (path.empty()) {
        return order;
    }
    std::ifstream in(path, std::ios::binary);
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (!line.empty()) {
            order.push_back(utf8ToWide(line));
        }
    }
    return order;
}

void writePinOrder(std::span<const std::wstring> names) {
    const std::wstring path = novaConfigFilePath(kPinOrderFile);
    if (path.empty()) {
        return;
    }
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        return;
    }
    for (const std::wstring& name : names) {
        out << wideToUtf8(name) << "\n";
    }
}