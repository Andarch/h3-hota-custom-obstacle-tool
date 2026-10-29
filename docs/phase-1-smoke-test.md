# Obstacle Plugin Smoke Test

Current scope (2026-10-28): obstacle-tool enhancements only. The former
255-garrison feature was removed; all object limits use native behavior.
Historical acceptance results are recorded in `CODEX_HANDOFF.md`.

Build Debug and Release Win32 sequentially and run compatibility suites using
`tests/README.md`. Compare a direct editor launch with a fresh launch through
the rebuilt plugin launcher.

## Editor Regression Checks

- Editor startup, shortcuts, new/open/save/save-as, validation, and close.
- Native garrison limit: the 49th garrison is rejected as in the direct-launch baseline.
- Skill-booster limits remain native.
- No repeated-placement command or automatic selection rearming.

## Obstacle Checks

- All 60 buttons, icons, previews, category selection controls, and scrolling.
- Painted-terrain applicability and Place Obstacles enablement on both layers.
- Checked-button filtering, generation, clearing, undo/redo, and Shift+O.
- Accepted mountain transition/order behavior, small-dune gap filling, and
  volcano minimap colors.
- Vanilla generation when custom filtering is inactive.
- Default/Custom radios: Default hides Obstacle Types and retains native
  placement/enablement/colors; Custom restores selections and expanded sections.

These are live regression checks, not newly recorded passes. Underground visual
acceptance remains pending as described in the handoff.
