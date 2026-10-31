# Copilot Instructions

Read `AGENTS.md` and `CODEX_HANDOFF.md` before suggesting changes.

Project-specific notes:

- Target the official HotA map editor through the injected x86 DLL.
- Preserve vanilla behavior unless a feature is explicitly active.
- Scope is the obstacle tool. Preserve native object limits and Standard-mode behavior.
- Support the exact validated English and Russian HotA 1.8.1 profiles; keep hash
  and original-byte guards in place.
- Edit Russian UI wording in `translations/ru.json`, then regenerate with
  `tools/generate_translations.py`. Keep workbook names and DEF identities unchanged.
- Do not commit game binaries, LOD archives, raw extracted proprietary assets,
  generated reverse-engineering output, or a full custom `objects.txt`.
