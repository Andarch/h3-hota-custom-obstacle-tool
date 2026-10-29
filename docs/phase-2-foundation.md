# Plugin Foundation

Current scope (2026-10-28): obstacle-tool enhancements only. The garrison
limit feature and its three hook sites have been removed from the plugin.
Native object limits apply. Earlier foundation verification is historical;
see `CODEX_HANDOFF.md` for chronology and current feature acceptance.

## Initialization Contract

- `DllMain` records the plugin module and disables thread callbacks.
- The launcher injects with `LoadLibraryA`, then calls the exported
  `HotaMapEdPluginInitialize` function on a second remote thread.
- Both the executable and `hota_me.dll` hashes must match the HotA 1.8.1
  profiles before any hook is installed. Unsupported versions are rejected.
- Original-byte validation, checked patch writes, and instruction-cache
  flushing protect hook installation. Installation failures propagate.
- The launcher closes a newly launched editor if interrupted initialization
  or hook installation leaves its patch state uncertain.
- `HotaMapEdPlugin.log` is written next to the launcher/DLL when possible.

## Target And Build Layout

Version-specific hashes, hook addresses, and expected bytes live in
`plugin/target_profile.cpp`; the current compatibility notes are in the handoff.
Do not use the historical 1.8.0 DLL addresses.

Plugin and launcher intermediate directories and linker PDB names are separate.
Build only Win32 Debug and Release, sequentially using the commands in
`AGENTS.md`. Run compatibility tests using `tests/README.md`.
