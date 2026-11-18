/**
 * @file ShortcutStore.h
 *
 * @version 1.0.0
 * @author BleckWolf25
 * @license MIT
 *
 * @summary Shell shortcut metadata, pin creation and persisted pin ordering
 *
 * @description
 * Reads a .lnk target and AppUserModelId through IShellLinkW and IPropertyStore, writes new pins
 * with IPersistFile, and derives the grouping key that matches a pin to its running windows.
 * Also decodes Explorer's Taskband Favorites PIDL list for the user's ordering and reads or writes
 * NovaUI's own drag order in the roaming configuration folder. Disk access happens only when the
 * caller reacts to a change event, never during steady-state rendering.
 *
 * @since 18/11/2026
 * @updated 18/11/2026
 */
#pragma once
// ---------- IMPORTS
#include <filesystem>
#include <span>
#include <string>
#include <vector>
// ---------- TYPES
struct ShortcutMetadata {
    std::wstring targetPathLower;
    std::wstring appUserModelIdLower;
};
// ---------- DECLARATIONS
ShortcutMetadata resolveShortcutMetadata(const std::wstring& shortcutPath);
std::wstring makeShortcutKey(const ShortcutMetadata& metadata, const std::wstring& shortcutPath);
bool createShortcutFile(const std::wstring& shortcutPath, const std::wstring& targetPath,
                        const std::wstring& appUserModelId, const std::wstring& description);
std::filesystem::path uniqueShortcutPath(const std::wstring& directory,
                                         const std::wstring& baseName);
std::vector<std::wstring> readTaskbandFavoriteNames();
std::vector<std::wstring> readPinOrder();
void writePinOrder(std::span<const std::wstring> names);