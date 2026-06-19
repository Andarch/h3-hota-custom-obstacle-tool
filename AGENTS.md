# Project Instructions

Read `CODEX_HANDOFF.md` before planning or changing this project.

## Scope

- Target the official HotA map editor through an x86 injected DLL.
- Preserve vanilla behavior unless a feature is explicitly active.
- Build a maintainable multi-feature plugin foundation; do not attempt a full
  editor rewrite unless the user explicitly changes direction.
- Keep features independent so each can be enabled, disabled, and tested alone.

## Current Safety Requirements

- The desired object-limit behavior is currently garrison-only.
- Do not retain or re-enable the skill-booster limit increases without new
  evidence that both editor and game behavior are correct.
- Treat the uncommitted repeated-obstacle/cache-swapping code as an experiment,
  not a verified implementation.
- Do not delete, revert, or overwrite the user's dirty worktree.
- Do not modify original game executables or DLLs on disk.
- Do not commit game binaries, LOD archives, raw extracted proprietary assets,
  or a full custom `objects.txt`. Small derived tables required by a plugin
  feature may be committed after documenting their purpose and provenance.

## Reverse Engineering

- Use the existing IDA 9.3 databases in
  `C:\Users\Andy\Games\3DO\Heroes3` before starting analysis from scratch.
- Prefer direct binary analysis, IDAPython, scripted diagnostics, and runtime
  logging over asking the user to perform manual IDA steps.
- Computer Use may operate IDA and `h3hota_maped.exe` for GUI-only debugging
  and testing when available.
- Label findings as one of:
  - statically verified;
  - runtime verified;
  - hypothesis requiring validation.
- Every absolute address is version-specific. Check the target profile and
  expected original bytes before installing a hook.
- Reuse the editor's native validation and behavior whenever possible instead
  of reimplementing complicated rules.

## Hook And Initialization Standards

- Refuse to patch unsupported executable versions.
- Validate original bytes before writing.
- Check and propagate installation failures.
- Flush the instruction cache after code patches.
- Keep substantial initialization and UI work out of `DllMain`.
- Avoid global cache replacement until the cache ownership, lifetime, and all
  consumers are proven.

## Build And Verification

Build Win32 only:

```powershell
& 'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\MSBuild\Current\Bin\MSBuild.exe' `
  HotaMapEdPlugin.sln /m:1 /p:Configuration=Debug /p:Platform=Win32 /v:minimal

& 'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\MSBuild\Current\Bin\MSBuild.exe' `
  HotaMapEdPlugin.sln /m:1 /p:Configuration=Release /p:Platform=Win32 /v:minimal
```

- Use sequential solution builds until the shared PDB/output collision is fixed.
- After significant changes, verify both the feature and unaffected vanilla
  editor behavior.
- Record newly proven addresses, structures, expected bytes, and behavior in
  `CODEX_HANDOFF.md` or a dedicated reverse-engineering document.

## Communication

- Keep reverse-engineering explanations concise and evidence-focused.
- Correct uncertain assumptions directly.
- Ask the user for manual testing only when the workflow cannot reasonably be
  automated or controlled by Codex.
