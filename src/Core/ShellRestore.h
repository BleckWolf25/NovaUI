/**
 * @file ShellRestore.h
 *
 * @version 1.0.0
 * @author BleckWolf25
 * @license MIT
 *
 * @summary Persisted shell preference snapshot and out-of-process restore guard
 *
 * @description
 * Writes the user's StuckRects3 Settings blob and Explorer AppBar state to the NovaUI
 * configuration folder before suppression, and restores them verbatim afterwards. The guard entry
 * point runs as a detached instance of this executable, blocks on the parent process handle and
 * replays the snapshot, so preferences return even when the shell process is terminated. Restores
 * are idempotent and the snapshot file is removed once applied.
 *
 * @since 18/11/2026
 * @updated 18/11/2026
 */
#pragma once
// ---------- IMPORTS
#include <Windows.h>
// ---------- DECLARATIONS
namespace ShellRestore {
void capture();
void apply();
void spawnGuard(DWORD parentPid);
int guardMain(DWORD parentPid);
}