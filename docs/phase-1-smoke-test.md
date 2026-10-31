# Editor and Obstacle Smoke Tests

Build Debug and Release Win32 sequentially, then run the compatibility suites
using `tests/README.md`. Compare a direct editor launch with a fresh launch through
`HotACustomObstacleTool.exe`, with its matching DLL beside it.

## Editor regression

- Startup, shortcuts, new/open/save/save-as, map validation and close.
- Native object limits and normal manual object placement remain unchanged.
- Standard startup: native obstacle placement, command enablement and minimap colors.
- Standard/Custom clicks and tooltips preserve focus, shortcuts and remembered settings.

## Custom obstacle controls

- All 121 buttons and twelve categories match the workbook, including icons/previews.
- Each section starts collapsed; counts/bold headers include unavailable selections.
- Global and category Select All/None, mixed-state expansion toggle and fixed row.
- Scroll to the bottom and operate all three global controls; verify no horizontal jump.
- Remembered selections/expansion and clean panel recreation after tool/mode switches.
- Painted-terrain availability and Place Obstacles/Shift+O on both map layers and
  multiple map sizes; no selected applicable button means no generation.
- Generate, clear, undo/redo, and Ctrl+Z/Ctrl+S/Shift+O accelerator behavior.
- Hover cards show correct terrain icons and all selected representative DEFs.

## Placement adjustments

- Mountains across terrain transitions and the accepted mountain-only order behavior.
- Sand dune overlap and small-dune second-pass gap filling: Sand-only normally,
  all painted non-void terrains with Allow all terrains enabled. Include
  Lava/Void and Water/Void boundaries with only Sand Dunes selected.
- Workbook-only additions, borrowed rules and missing-row objects such as Maples.
- Custom volcano minimap shade; return Standard to restore native colors.
- Allow all terrains: unchecked at startup, clickable before/after painting, every
  button applies to non-void paint and previews show all eleven terrains.
- Exercise mountains on Water, reefs on Grass, underground, mixed terrains and Void.
  Uncheck and generate again to confirm original restrictions return.
- Mode/terrain setting changes do not edit previously placed objects. Save/reopen a
  disposable map and inspect results in the game before claiming game-level coverage.

These are verification scenarios, not a record that every visual workflow has been
retested at each checkpoint. Current evidence and acceptance limits are in the handoff.
