# Phase 2 Foundation

This phase keeps the plugin foundation intentionally small while making the
active garrison feature fail closed.

## Initialization Contract

- `DllMain` only records the plugin module and disables thread callbacks.
- The launcher injects the DLL with `LoadLibraryA`, resolves the exported
  `HotaMapEdPluginInitialize` function by RVA, and invokes it on a second remote
  thread.
- Unsupported-target and pre-initialization failures leave the editor running
  without an active plugin and display a useful launcher error.
- Hook-installation or interrupted-initialization failures close the newly
  launched editor when its patch state cannot be proven safe.
- `HotaMapEdPlugin.log` is written next to the launcher/DLL when possible.

## Supported Target

The only supported target profile is the statically verified HotA 1.8.0 map
editor:

- SHA-256:
  `CCF1E8424D913AA409ECC168F87FB20EDD64C3A56D412F2878065FC261BC6113`
- Create hook: `0x004212A4`, expected bytes `3B 48 08 72 28`
- UI predicate hook: `0x004232D4`, expected bytes
  `3B 46 08 0F 83 A8 01 00 00`
- Place hook: `0x00426AFA`, expected bytes `3B 48 08 72 28`

The plugin hashes the running executable before selecting this profile. Unknown
hashes are rejected before hook validation or patching.

## Hook Safety

- All three hook sites are validated before any patch is written.
- Each install validates its site again immediately before patching.
- Hook writes check page-protection changes and flush the instruction cache.
- A failed install rolls back previously installed garrison hooks and reports
  rollback failures.
- The garrison feature is the only active feature.

## Build Layout

- Plugin and launcher intermediate directories are explicitly separated.
- Linker PDBs use distinct names while the shipped files remain
  `HotaMapEdPlugin.dll` and `HotaMapEdPlugin.exe`.

## Verification

Statically verified:

- The supported profile matches the local target executable and verified hook
  bytes.
- The Debug DLL exports `HotaMapEdPluginInitialize`.
- The unfinished repeated-obstacle experiment is not initialized.

Runtime verified on 2026-06-15:

- The Debug launcher started the supported editor successfully.
- Explicit plugin initialization selected the HotA 1.8.0 profile by SHA-256.
- The log reported successful garrison hook installation.
- Loading the plugin into an unsupported x86 host returned
  `kPluginInitializeUnsupportedTarget` and logged that no hooks were installed.
- Debug Win32 and Release Win32 solution builds succeeded sequentially.
- A parallel Debug Win32 solution rebuild succeeded with the separated
  intermediate and PDB paths.
