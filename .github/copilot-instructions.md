# Copilot Instructions

Read `AGENTS.md` and `CODEX_HANDOFF.md` before suggesting changes.

Project-specific notes:

- Target the official HotA map editor through the injected x86 DLL.
- Preserve vanilla behavior unless a feature is explicitly active.
- The garrison feature raises the number of garrison objects allowed on the map
  from 48 to 255; it does not change creature stack sizes.
- The desired object-limit behavior is currently garrison-only. Do not re-enable
  the primary-skill-booster limit increases without new editor and game
  verification.
- Do not commit game binaries, LOD archives, raw extracted proprietary assets,
  generated reverse-engineering output, or a full custom `objects.txt`.
