# Codex Project Handoff

Last updated: 2026-10-29

## Next Chat Starting Point

- User accepted the latest workbook updates and Reefs ordering on 2026-10-29,
  and requested this checkpoint be committed and pushed. Current UI has 91
  buttons in twelve categories, with Reefs second, Lakes, Craters and Fissures.
  This supersedes pending visual notes for these category/icon updates. Broader
  painted-map and underground regression status remains as recorded below.
- Reefs moved to the second category (2026-10-29), immediately after Mountains,
  by regenerating from the updated v2 workbook with the standard CLI. Current
  order: Mountains, Reefs, Hills, Mounds, Lakes, Craters, Fissures, Trees,
  Undergrowth, Debris, Animals, Miscellaneous. Counts remain 91 buttons / twelve
  categories. Statically verified masks, selections, visuals, all 880 DEF-to-button
  assignments and 28 override payloads are unchanged after resolving reordered
  indices. Debug/Release Win32 builds and compatibility suites pass.
- Craters / Fissures update (2026-10-29): current v2 workbook mappings split
  Damaged Ground into Craters (8) and Fissures (6), rename the six `Cracks`
  buttons to `Fissures`, and rename Liquids to Lakes (11). Regenerated metadata
  contains 91 buttons in twelve categories. 40x40 buttons / 36x36 icons and
  expansion controls are retained. User accepted the update.
- User corrected Button Layout (2026-10-29). The standard generator now succeeds
  directly against the workbook and produces byte-identical header/source files
  to the tested Craters/Fissures update. No temporary normalization is needed.
- Before that correction, generation replaced the stale layout's `Liquids`
  header with `Lakes`, `Damaged Ground` with `Craters`, changed ` Cracks` to
  ` Fissures` in button names, and inserted a `Fissures` header immediately
  before the `Dirt Fissures` row, all in memory. Other layout order is preserved.
  Workbook was untouched by Codex; this temporary normalization is now historical.
- Statically verified against the preceding generated data: the same 880 DEF
  hashes, borrow indices, 28 override payloads, terrain masks, default selections
  and icon/preview definitions are retained after accounting for the renames.
  Debug/Release Win32 builds and compatibility suites pass; both DLLs initialize
  in isolated 1.8.1 editors with native object-limit bytes unchanged.
- Liquids / Damaged Ground update (2026-10-29): regenerated from the current
  `obstacles_1.8.1_v2.xlsx` using `HotA_1.8.1_txt/edobjts.txt`. Lowered Ground
  is replaced by Liquids (11 buttons) and Damaged Ground (14 buttons), giving
  91 buttons in eleven categories. Layout, terrain masks and icon/preview DEFs
  follow the workbook. Lava is now Lava-only; Subterranean Lava has its own
  button. The workbook's Hills header is corrected, so the normal generator
  CLI now works without the earlier in-memory A8 correction.
- Statically verified: all 880 mapped DEF hashes and borrow-source indices,
  28 derived template override payloads and 12 borrowed sources are retained;
  layout-dependent button indices are regenerated. No raw assets or workbook
  edits. Accepted 40x40 buttons / 36x36 icons and expansion controls remain.
- Verification: Debug/Release Win32 builds and compatibility suites pass,
  including independent packing and candidate selection for all 91 buttons.
  Both DLLs initialize in isolated 1.8.1 editors with native object-limit bytes
  unchanged. User accepted the new categories/icons.
- User accepted the workbook/category updates, 40x40 buttons with 36x36 icons,
  and expansion toggle on 2026-10-29, and requested this checkpoint be committed
  and pushed. That earlier panel had 70 buttons in ten categories. Visual acceptance
  supersedes the pending UI notes below; broader painted-map regression and
  underground visual acceptance remain limited to the evidence already recorded.
- Added a 28x28 Expand All / Collapse All toggle (2026-10-29), right-aligned
  on the global Select All/None row. Double down chevrons expand every category
  if any is collapsed; double up chevrons collapse when all are expanded.
  Tooltip and glyph reflect individual header changes. Reuses non-focus mouse
  handling and scroll layout; selections and generation are unchanged. All
  sections still start collapsed. Debug/Release Win32 builds and compatibility
  suites pass, including real tooltip updates, mixed-state clicks, visibility,
  focus/selection preservation and scroll clamping. Both DLLs initialize in
  isolated 1.8.1 editors with native object-limit bytes unchanged. User accepted
  the new control in the editor.
- Hills/Mounds split (2026-10-29): regenerated from the updated v2 workbook
  as separate Hills (7 buttons) and Mounds (4 buttons), for ten categories and
  the same 70 buttons. Only category membership, category names and Mounds row
  positions changed; icons, masks, selections and generation data are retained.
  At that time, Button Layout A8 still said `Hills/Mounds`, while Button Mapping and
  Obstacles say `Hills`. Generation used `read_workbook_sheets`, replaced only
  `sheets["Button Layout"][7]` with `["Hills"]` in memory, then ran the normal
  parse/validation/render functions. Workbook is untouched; the CLI generator
  rejected the stale heading until A8 was corrected in the subsequent Liquids
  update above. Debug/Release Win32
  builds and compatibility suites pass; user accepted the category split.
- Obstacle button size trial (2026-10-29): buttons are now 40x40 with 36x36
  runtime-rendered icons, providing a two-pixel inset. Previous 48x48 buttons
  with 44x44 icons were accepted; user requested this smaller size for comparison.
  User accepted 40x40 buttons with 36x36 icons. Four-column workbook
  layout and scroll sizing use the new button extent. Select All/None controls
  and 160x160 hover previews retain their sizes. Debug/Release Win32 builds and
  compatibility suites pass.
- Current workbook (supersedes all older workbook paths below):
  `C:\Users\Andy\My Drive\Heroes Stuff\HotA lod\obstacles_1.8.1_v2.xlsx`.
  Use it in place for future requested updates; do not edit the workbook.
- Initial update from v2 on 2026-10-29: 70 buttons across nine categories. Raised
  Ground is split into Mountains (18), Hills/Mounds (11), and Reefs (2).
  Generated button layout, terrain masks, DEF mappings, icons and previews
  follow the workbook, validated with `HotA_1.8.1_txt/edobjts.txt`.
- Statically verified workbook comparison by DEF: all 880 DEFs are retained;
  only Category (242 rows) and Button (117 rows) changed on Obstacles.
  Density, masks, native identities and borrowed rules are unchanged. The
  generated 28 minimal template overrides and 12 borrowed sources are retained;
  their button indices follow the new layout. No raw game assets are included.
- Extended selection storage/packing from two DWORDs to three (capacity 96),
  with generator and compile-time guards. This is required for buttons 64..69
  (Barrels through Crumbled Structures); no shifts exceed 31 bits. Categories
  still start collapsed; Default retains native behavior.
- Verification (2026-10-29): normal Debug/Release Win32 solution builds and
  compatibility suites pass. Added independent packing checks for all 70 UI
  selections and 70-by-70 candidate acceptance checks, covering both DWORD
  boundaries and inactive-filter bypass. Existing disposable-window tests pass
  with nine categories, header counts, collapsed startup, Select All/None,
  mode controls, focus and scroll preservation.
- Runtime verified: both rebuilt DLLs initialize in disposable private-desktop
  1.8.1 editors; native object-limit bytes remain unchanged at 0x4212A4,
  0x4232D4 and 0x426AFA. Evidence: existing ignored
  `scratch/smoke/obstacle_only_smoke.exe`. Full painted-map placement and
  visual acceptance of the new button set are not established by these checks.

- Tightened Default/Custom radio spacing: option width is capped at 88 pixels
  instead of stretching each option across half the panel; narrow panels still
  use half-width options.
- User accepted the panel updates on 2026-10-28, including collapsed startup and
  the first-visit flash fix, and requested this checkpoint be committed/pushed.
  This checkpoint includes obstacle-only scope (garrison hooks removed),
  category headers, global selection controls, and Default/Custom modes.
  Panel visual acceptance supersedes the pending UI notes below; broader
  painted-map regression checks and underground visual acceptance are not
  established by this feedback. Both Win32 builds and compatibility suites pass.
- All seven categories now start collapsed (2026-10-28); user expansion still
  persists across mode/tool switches. Fixed first-visit Custom UI flash: the
  Obstacle Types label, scroll host and Place overlay are created hidden,
  including the overlay's positioning call. They are shown only after all
  controls and layout are ready and the current mode is applied. Previously
  these windows were visible throughout construction, then hidden in Default.
  Debug/Release Win32 builds and compatibility suites pass, including collapsed
  startup and hidden-at-creation scroll host/real Place overlay checks.
- Added Mode / Default / Custom controls below Brush on 2026-10-28. Default
  starts every fresh editor session; mode selection persists across tool/panel
  recreation. Default hides the entire Obstacle Types label/viewport and Place
  overlay, leaving the native toolbar/command enablement intact. Custom restores
  saved button selections, category expansion and scroll state. Radio controls
  reuse non-focus mouse handling, with no persistent native-parent subclass.
  Default bypasses diagnostic/workbook row filters, prevents custom generation
  modes, and therefore uses native object sources, parser rows, scoring/order
  checks and placement passes. The volcano minimap correction is now Custom-only;
  changing mode invalidates the editor view. Hooks remain installed but take
  their native paths in Default. No map objects are removed or changed on toggle.
  Debug/Release Win32 builds and compatibility suites pass. Native-window tests
  cover radio clicks/check states, hidden controls, preserved selections and
  expansion, panel recreation, focus and unmodified queued Default commands.
  Tests also cover Default diagnostic-filter/overlay bypass and the minimap
  thunk's type/flag/register preservation in both modes. Runtime verified native
  color calls for all 232 types on all 12 terrains in disposable 1.8.1 editors:
  Debug and Release Default match vanilla (2,784 cases); Custom changes only
  type 157 to type 158's colors, and returning Default restores native results.
  Evidence: ignored `scratch/obstacle_mode_probe.cpp/.vcxproj` and
  `scratch/obstacle_mode_{vanilla,debug,release}.txt`. The Release probe toggles
  the flag via its PDB data symbol because optimized setter symbols are absent.
  User accepted the mode controls visually; full painted-map regression remains
  unconfirmed.
- Added global 28x28 Select All/None controls above the first category on
  2026-10-28, with larger plus/minus strokes and the existing tooltips and
  non-focus mouse handling. The row scrolls with the categories. These actions
  update all 60 saved selections, including collapsed and terrain-disabled
  buttons, refresh counts/bold headers and Place enablement immediately, and
  preserve expansion state. Per-category controls remain unchanged.
  Debug/Release Win32 builds and compatibility suites pass; native-window tests
  cover global clicks, dimensions, disabled/collapsed selections, focus and
  unchanged expansion. User accepted the global controls visually.
- Category headers implemented on 2026-10-28: gray clickable rows with a small
  disclosure arrow, workbook category name, and checked-button count. Text is
  bold above zero, regular at zero; counts include terrain-disabled selections.
  Sections expand independently. All categories start collapsed; session-level
  state survives tool/panel recreation, but is not saved across editor restarts.
  Collapsing hides selection controls and buttons
  without changing selected masks or generation. Headers reuse the existing
  non-focus mouse handling to preserve editor accelerators.
  Category names are now emitted by the workbook generator; other generated
  metadata is unchanged. Generator validation, both sequential Win32 builds,
  and both compatibility suites pass. Native-window tests cover header clicks,
  counts, font weights, visibility, unchanged selection masks, panel recreation,
  focus preservation, preview clearing, and scrollbar clamping. Header rendering
  was inspected with a local GDI preview and accepted by the user in the editor.
  Debug/Release initialization passes in disposable 1.8.1 editors.
- Scope changed on 2026-10-28: this plugin is specific to the obstacle tool.
  Removed the garrison limit feature, its initialization, source files, project
  entries, and target-profile hook sites. Native object limits now apply,
  including the 48-garrison limit. Older garrison/multi-feature plans below
  are historical and superseded by this decision. Existing maps are not edited.
  Verification: sequential Debug/Release Win32 solution builds and compatibility
  suites pass. Runtime verified in disposable private-desktop 1.8.1 editors:
  both rebuilt DLLs initialize successfully, obstacle hooks still install, and
  all original bytes at 0x4212A4, 0x4232D4, and 0x426AFA remain unchanged.
  Evidence: ignored `scratch/obstacle_only_smoke.cpp/.vcxproj`. No painted-map
  visual regression test was performed for this removal.
- All 60 buttons now have workbook-defined icons and previews. The final 14
  buttons were regenerated on 2026-10-28; only visual metadata changed.
  Generator validation and Debug/Release Win32 builds pass. The user accepted
  the icon updates and requested this checkpoint be committed and pushed.
- Underground Place Obstacles enablement is corrected (2026-10-27); see below.
  Both Win32 builds and automated tests pass; visual editor acceptance is pending.
- Work on `main`, targeting HotA 1.8.1 only. The latest small-dune filler pass
  is user-tested and accepted; no further placement changes are requested.
- The user confirmed the final dune result with both normal and passability
  views: the previously remaining gaps are filled and the appearance is good.
  This supersedes the accepted two-gap limitation documented on September 20.
- Preserve the mountain-only drawing-order exception. Dunes must retain native
  drawing-order checks; their rejected bypass must not be restored.
- Previous workbook: `C:\Users\Andy\My Drive\Heroes Stuff\HotA lod\obstacles_1.8.1.xlsx`.
  Read it in place for future requested icon/data updates; no workbook edits
  were needed for this filler feature. Test map: `C:\Users\Andy\Repos\obstacle_test.h3m`.
- Latest checkpoint includes the underground scanner fix, its regression tests,
  and the completed button visuals. Small-dune placement remains user-accepted;
  underground visual testing is still pending as recorded below.
- Leave unrelated `tools/__pycache__/` files out of commits. Do not modify
  original game binaries, map files or workbook without a specific request.

## Remaining Button Visuals (2026-10-28)

- Regenerated from `obstacles_1.8.1.xlsx` with `HotA_1.8.1_txt/edobjts.txt`.
  Added icons/previews for Rocks, Tree Remains, Bones, Frogs, Chickens,
  Roosters, Pigs, Crates, Barrels, Sacks, Ropes, Abandoned Boats, Burnt
  Structures, and Crumbled Structures.
- Verified exactly 14 visual-only row changes from the previous generated
  file. All 60 buttons now have icon/preview definitions. Terrain masks,
  layout, selected defaults, DEF mappings, and generation overrides unchanged.
- Generator validation and sequential Debug/Release Win32 solution builds
  pass. Workbook read in place without edits; no extracted assets included.
  The user accepted the completed icon updates and requested commit/push.

## Flowers and Mushrooms Button Visuals (2026-10-28)

- Regenerated from `obstacles_1.8.1.xlsx` with `HotA_1.8.1_txt/edobjts.txt`.
  This update changes only Flowers and Mushrooms visual metadata, preserving
  the previous Bushes update and all generation data.
- Flowers icon: `avgflh13.def`. Preview: `avgflh13.def`, `avgflh19.def`,
  `avgflh07.def`, `avgflh10.def`.
- Mushrooms icon: `avrmsh06.def`. Preview: `avrmsh06.def`, `swpmsh05.def`,
  `avlms100.def`, `avlms120.def`.
- Generator validation and Debug/Release Win32 solution builds pass. Workbook
  read in place without edits; no extracted assets included. User accepted the
  update before proceeding to the remaining buttons.

## Bushes Button Visuals (2026-10-28)

- Regenerated button data from the user's updated `obstacles_1.8.1.xlsx`, using
  `HotA_1.8.1_txt/edobjts.txt` for validation. Only the Bushes row changed.
- Icon: `avls09s0.def`. Preview: `avls09s0.def`, `avls04s0.def`,
  `avls01s0.def`, `avlsh7d0.def`. Native resource loading supplies the images;
  no extracted assets are included. Terrain masks and generation data unchanged.
- Generator validation and Debug/Release Win32 solution builds pass. Workbook
  read in place without edits. User accepted the update before proceeding to
  Flowers and Mushrooms.

## Underground Obstacle Enablement (2026-10-27)

- User reported that painting underground on a two-layer map never enabled
  Place Obstacles. The plugin scanner was hard-coded to terrain collection 0
  and selection layer 0. This also omitted underground category applicability.
- Statically verified in existing IDA analysis: native UI update `0x484C8E`
  enables for surface selection or, when underground exists, underground
  selection. Native command `0x484B07` generates both painted layers, regardless
  of which layer is currently displayed. Helper `0x429E28` reads the underground
  byte at `[*mapManager + 0x10]`; `0x429EBD` selects terrain collection 0/1.
  Native input predicate `0x48016A` tests selection state 2 for its layer via
  `0x43C114`. These EXE addresses are unchanged in the supported 1.8.1 profile.
- `obstacle_variants.cpp` now scans each existing layer with its corresponding
  terrain collection and dimensions, ORs terrain masks, and sums state-2 counts.
  Single-layer maps never query layer 1. Category tracing now inspects only the
  existing map layers rather than 16 presumed collections. No hooks or native
  placement behavior changed.
- Automated verification: Debug/Release Win32 solution builds and compatibility
  suites pass. New tests exercise the production scanner with native-layout
  headers and adapters, covering underground-only, combined, surface-only,
  cleared/non-obstacle paint, unchecked categories, and single-layer exclusion.
- Runtime verified in disposable 1.8.1 editor processes on a private desktop:
  Debug and Release plugin initialization passed; a scanner probe calling the
  real `0x429EBD`, `0x42B23E`, and `0x43C114` with synthetic two-layer grids
  detected underground-only Subterranean paint, combined it with surface Snow,
  excluded underground on single-layer maps, and returned empty after clearing.
  Local evidence: ignored `scratch/underground_probe.cpp/.vcxproj` and existing
  `scratch/smoke/hota181_smoke.exe`. No user maps or original binaries modified.
- Pending: visual acceptance of painting, category enablement, and generation
  underground in the user's normal editor workflow. Synthetic native-grid
  verification does not establish visual placement acceptance.

## Small Dune Gap Fill (2026-09-21)

- Implemented and user visually accepted: when Sand Dunes is selected,
  defer type 148/subtype 0 (AVLdun10/20/30.def) until a second native pass.
  Their workbook Hills classification is unchanged. Hills-only generation
  retains normal first-pass behavior; with Sand Dunes selected, these three
  templates are reserved for gap filling even if Hills is also selected.
- Statically verified: EXE 0x49E8DF runs native per-cell placement 0x49E452
  before clearing selection eligibility. A guarded entry hook at 0x49E8DF
  (`55 8B EC 83 EC 14`, resume +6) snapshots painted Sand cells. Native
  placement consumes flag 0x04000000 even when it leaves a gap, so checking
  that flag only after generation is insufficient (runtime verified).
- Guarded hook 0x49E9E4 (`39 7B 20 8B 73 14`, resume +6) runs the filler
  loop before native cleanup. Both hook thunks preserve registers and flags
  and replay stolen instructions. Addresses and bytes are in TargetProfile.
- Each saved painted Sand cell is rechecked with native eligibility 0x499F70
  and unpainted flag 0x08000000 before calling 0x49E452. That routine enumerates
  only candidate anchors whose blocked mask covers the target cell. During
  this pass, the existing candidate filter allows only type 148/subtype 0,
  independently of Hills selection. No new overlap or drawing-order bypass
  is used; native small-dune overlap and adjacency weights are retained.
- Runtime verified in a disposable native generator: first-pass-only test
  generated 20 large dunes and left two gaps; filler added two small dunes
  and closed both. A repeat fill attempt added nothing. An actual hooked
  0x49E8DF run generated 19 large plus three small dunes with zero gaps.
  Random runs vary. Evidence: ignored scratch/dune_fill_probe.cpp/.txt and
  dune_pass_analysis.py/.txt/.asm. These isolated probes were followed by
  successful user-map visual testing on 2026-09-21.
- Regression tests cover first-pass deferral, Hills-only behavior, filler type
  restrictions and painted-Sand snapshot exclusions. No workbook/map changes.

## Accepted Placement Fixes (2026-09-20)

This status supersedes the pending/revert recommendations in the chronological
investigation notes below. User testing accepted the terrain-transition fix,
mountain-only drawing-order exception, volcano minimap correction, and revised
sand dune self-overlap adjustment.

- Mountains now fill across terrain transitions and the tested residual gaps.
- Volcano type 157 uses the dark minimap shade, without passability changes.
- Sand dunes retain native drawing-order checks; their private selected-mode
  self-overlap weight is 30. The dune drawing-order bypass was rejected and
  removed after it produced placements that manual dragging also rejected.
- User confirmed the revised dune generation has no visible ordering issues
  and leaves only two bottom-row gaps in the test. Manual placement of dunes
  in those spots is also rejected. These gaps are accepted; this does not
  establish that every possible layout is maximally filled.
- Further object-specific adjustments are deferred. Do not broaden the
  mountain exception to dunes or other families without new evidence.
- Debug/Release Win32 builds, compatibility suites and isolated probes passed.
  Source maps, workbook, game binaries and extracted assets are not changed
  or included in this checkpoint.

## Purpose

This repository builds an x86 DLL plugin and launcher for enhancing the
Heroes III: Horn of the Abyss map editor (`h3hota_maped.exe`).

The practical goal is to add editor features while preserving compatibility
with maps played by the official HotA game. The current direction is to keep
using an injected DLL, but strengthen the plugin foundation before adding more
features.

Do not begin by recreating the entire map editor. A standalone map library or
tool may become useful later, but the official editor plugin is currently the
lowest-cost path for features that need HotA's native editor behavior.

## Current Compatibility: HotA 1.8.1

The current plugin targets **1.8.1 only**, per the user's decision to track the
latest HotA rather than retain older-version support. Older 1.8.0 addresses and
runtime findings below are historical; this section supersedes their target
profile information. Work remains on `main`.

Statically verified against the installed binaries:

- `h3hota_maped.exe` is unchanged (2,772,992 bytes), SHA-256
  `CCF1E8424D913AA409ECC168F87FB20EDD64C3A56D412F2878065FC261BC6113`.
- `hota_me.dll` changed (2,749,440 bytes), SHA-256
  `365EB0C5C01ADBEAEC6A46E12D49BA2C710150270600ACE496496EB432DEA240`.
- The September 19 startup failure was the old DLL-hash guard rejecting this
  new module, after the garrison hooks had already installed. Initialization
  now checks both hashes before installing any hooks. Unknown module versions,
  including 1.8.0, are rejected without patching. Original-byte guards remain.
- EXE garrison, resource, object-registration, candidate, and UI hook addresses
  remain unchanged. DLL addresses are centralized in `target_profile.cpp` and
  shared with the optional diagnostic hooks.

| DLL site | 1.8.1 RVA | Original bytes replayed |
| --- | --- | --- |
| Weight predicate | `0xCC230` | `8B 41 04 56 8B 30` |
| Parser entry (diagnostics) | `0xDD4B0` | `55 8B EC 83 E4 F8` |
| Wrapper row store | `0xDDB54` | `8B 44 24 30 8B 40 04 8B 04 88 89 43 10` |
| Wrapper fallback | `0xDDB61` | `8B 4C 24 14 8B 54 24 1C` |

The fallback thunk must reload ECX from `[esp+14h]` and EDX from `[esp+1Ch]`,
then resume at RVA `0xDDB69`. Keeping the old stack offsets while changing only
the hash/addresses is unsafe. Wrapper `+0x10` still holds its RandTrn row, and
native row records remain 76 bytes. The parser's row-vector begin/end remain
at generator `+0xEB8/+0xEBC`; the terrain-count global moved to RVA `0x17B6DC`.

Native RandTrn now has 201 rows. Zero-based row 199 is the inserted snow glacier
bucket (`140/14`), shifting lava outcropping (`136/0`) from 199 to 200. The
optional legacy 200-row allowlist/terrain table is translated at lookup time;
trace and explicit block-ID storage cover all 201 native rows. The added row
keeps native eligibility. This is an index correction, not a workbook update.

Verification on 2026-09-19:

- Debug and Release Win32 solution builds passed.
- `tests/compatibility_tests.vcxproj` passed in Debug and Release: supported and
  unsupported profiles, old fallback-byte rejection, all row-index mappings,
  snow/lava boundaries, explicit block IDs 200/201 and invalid 202, trace bounds,
  parser-store/fallback stack and flag preservation, and predicate accept/reject
  thunk paths. See `tests/README.md` for commands. No game files are required.
- Runtime verified in disposable editor processes on a private desktop, without
  Computer Use or interacting with the user's editor: Debug/Release plugin
  initialization, expected live hook bytes, and the real 1.8.1 parser with
  synthetic dirt/snow mountain, unmatched, lava outcropping, and glacier wrappers.
  Vanilla and hooked runs both produced 201 rows and assignments `6,56,null,200,199`.
  Every row's adjacent/overlap vectors contained 201 entries. The optional row
  predicate hook also installed and executed in an isolated Debug run.
- Local evidence remains in ignored `scratch/ida_hota_181_hooks.json`,
  `scratch/hota_me_181.i64`, and `scratch/hota181_*` smoke-harness files. The old
  game-directory IDA databases and original game binaries were not modified.
- Pending user runtime verification: visible panel, DEF icons/previews, checked
  button painting, borrowed-row generation, native placement, undo/redo, and
  scrollbar behavior. Automated parser tests do not establish full map-editing
  or visual acceptance.

The subsequent 1.8.1 workbook update has now been regenerated against
`HotA_1.8.1_txt/edobjts.txt`. Raised Ground follows the revised workbook order,
including Ice Formations; Debris no longer has a separate Ice Blocks button.
The native ice-formation and glacier buckets have all terrain weights set to
`-5000`; workbook donor assignments remain the explicit source of borrowed
generation weights. The user accepted the updated layout and visuals on
2026-09-19 and supplied a successful snow-obstacle placement screenshot.
Debug and Release Win32 builds and compatibility tests passed after regeneration.

### Painted-Terrain Detection Correction

The first 1.8.1 user test found that categories never greyed out and Place
Obstacles stayed disabled. The capture hook was working: a read-only live
diagnostic found a valid context with mode 6 and a valid selection. However,
the old `[[context+0x54]+0x54/+0x58]` lookup returned `19x26` on the user's
`72x72` map. These UI-context fields are not a reliable map-size source; the
rejected dimensions made the scanner report no painted cells.

Statically verified from the unchanged EXE's native tile access:

- `0x42B23E` follows `*terrainCollection -> +0x08` to the terrain grid, then
  calls `0x42A0FF` with `grid+0x04`.
- The grid stores 6x6-tile chunks: column count at `grid+0x04`, chunk-pointer
  vector begin/end at `grid+0x0C/+0x10`.
- Width is `chunkColumns*6`; height is `(chunkCount/chunkColumns)*6`. The
  plugin validates the vector and supported dimensions before scanning. It
  does not use selection bounds, cursor/UI fields, or fixed 252x252 bounds.

Runtime verified by read-only inspection on 2026-09-19: that grid had 12 chunk
columns and 144 chunks, giving 72x72. Scanning the actual collection-0/state-2
paint with those bounds found 59 cells and terrain mask `0x33` (Dirt, Sand,
Swamp, Rough), consistent with the user's painted area. The editor and map
were not modified by the diagnostic.

`obstacle_selection_geometry.h` contains the shared dimension calculation.
Compatibility tests cover every supported size (36 through 252), the live
72x72 layout, null/empty/reversed/unaligned vectors, incomplete rows, unsupported
sizes, and overflow-sized inputs. The user confirmed that greying and placement
work with the rebuilt DLL.

## Current Obstacle Button Implementation

### Mountain Boundary Diagnostics (2026-09-20)

The user reports one-tile gaps along terrain transitions with mountains only.
The terrain-edge predicate is runtime verified to reject some edge candidates;
its contribution to the final gaps is not established by that result alone.

When the existing `HOTA_MAPED_PLUGIN_OBSTACLE_VARIANT_TRACE=1` flag is set,
the guarded HotA predicate entry now wraps the original native predicate and
records its original result before the experimental override below. It records selected-candidate row/button,
caller position triple, terrain/view index, and terrain/Dirt/Sand weights.
Detailed samples are capped at 256 row/terrain/view combinations; per-row and
terrain acceptance/rejection totals are logged at generation end. The existing
final-candidate hook captures the caller position from EBP+8/+12/+16; ordinary
launches now also install the predicate wrapper when candidate filtering is enabled.
The previously unsafe score-return hook remains untouched.

Debug/Release builds and compatibility tests passed, including trace-wrapper
true/false result preservation. Both trace-enabled builds passed isolated native
parser/predicate smoke tests.

Runtime verified from the user's 2026-09-20 reproduction and read-only live
terrain-view table inspection:
- At position (17,14,0), Dirt Mountains row 6 on Dirt/view 5 have current
  weight 10 but nativeAllowed=0. Views 4/5/6/7 resolve to edge type 9, which
  additionally requires Sand weight > -5000; this row's Sand weight is -5000.
  Interior Dirt/view 25 resolves to edge type 0 and is accepted.
- Swamp Mountains row 69 on Swamp/view 8/9/11 have current weight 10 but
  nativeAllowed=0. Those views resolve to type 4, which also checks Dirt;
  this row's Dirt weight is -5000. Swamp/view 78 is type 27, checking both
  Dirt and Sand, and is also rejected.
- Own-terrain rejection totals include Dirt row 6: 192, Grass row 38: 432,
  Swamp row 69: 696, Subterranean row 106: 384, Wasteland row 187: 384.
  These are predicate calls, not unique map tiles. Sand, Snow, Rough, Lava,
  and Highlands mountain rows had no own-terrain predicate rejections in
  this run, so terrain-edge rejection alone does not explain every gap.
Evidence: ignored `scratch/mountain-boundary-trace-2026-09-20.log` and
`scratch/read_boundary_types.ps1`.

Experimental terrain-edge relaxation (runtime executed, no visible improvement):
- Only in ButtonSelection mode, an otherwise rejected predicate may pass for
  the current allowed candidate row when its button remains selected, both
  template and button permit the tile terrain, and its current-terrain weight
  is greater than -4000. Void and out-of-range terrain IDs remain rejected.
- The original native predicate executes first. Footprint, overlap, weights,
  and subsequent native scoring are unchanged. No score-return hook is added.
- Candidate context is maintained without the trace flag. Generation cleanup
  clears it and logs the number of relaxed checks.
- Debug/Release Win32 builds, compatibility tests, and isolated native parser
  smoke tests passed. Tests cover wrong rows/terrains, disabled weights,
  deselection, and unchanged native results outside ButtonSelection mode.
- User repeated mountain-only placement on 2026-09-20 and reported the same
  boundary strips. The Debug log (initialization 21:21:38) confirms 2,389
  selected-object edge checks were relaxed. Thus this override executes but
  is insufficient to remove the visible gaps; the remaining cause is unproven.
- Do not broaden the bypass into scoring, footprint, or overlap checks without
  further evidence. Recommend reverting the behavioral experiment; it remains
  present and uncommitted pending that decision. Diagnostic work is separable.

Read-only follow-up, statically verified against the 1.8.1 IDA database:
- `hota_me.dll` RVA `0xCC3F0` is HotA's scorer. Its adapter at approximately
  RVA `0xCE1F0` calls it at `0xCE206` with the last argument zero. Another
  caller at `0xCD167` uses the alternate decorative-object path. Decompiler
  call signatures omit register arguments in places; inspect assembly before
  implementing any hook.
- The scorer checks each applicable footprint cell's actual terrain against
  the template mask at +0x14 (not the plugin button mask at +0x18), rejects
  Void, and separately checks tile flags. Do not remove these checks.
- At RVA `0xCC6BA`, it calls terrain accumulation helper `0xCC2E0` with the
  row, actual terrain, view, and a temporary terrain-flags array. Flag bit 2
  marks actual terrain; bit 1 marks terrain whose weight must be counted.
  Transition types add Dirt/Sand to bit 1 even when they are not the cell's
  actual terrain, independently of the previously wrapped predicate.
- For an accumulated terrain with weight <= -4000, multi-cell candidates
  return hard-failure score -2000000001. Thus Dirt weight 10 plus an edge-
  derived Sand weight -5000 still fails after the first predicate is relaxed.
  Single-cell candidates have a distinct low-score fallback. This mechanism
  is statically proven; no scorer-return trace was installed in this session.
- Adjacent/overlap tables and ordering checks run separately afterward. A
  possible narrow experiment would accumulate only actual terrain for eligible
  selected candidates, retaining masks, flags, collision checks, and later
  scoring. Do not clear a shared accumulated terrain indiscriminately: another
  footprint cell may actually use it. Implemented experimentally below.
- Evidence: ignored `scratch/ida_boundary_score.py/.json`, analyzed against
  the existing scratch 1.8.1 database. No production code or binaries changed
  during this investigation. A controlled placement test is still needed to
  prove this removes the strip; arbitrary gap-free packing is not guaranteed.

Second boundary experiment implemented (2026-09-20, pending visual acceptance):
- The terrain accumulator at RVA 0xCC2E0 now uses actual-terrain flags only
  for the current allowed selected candidate, gated by the same row, selected
  button, mask intersection, non-Void terrain and usable weight checks as the
  predicate experiment. It ORs flags with 3 without clearing earlier cells.
- All other calls use the native helper. Entry bytes are validated as
  `A1 C4 BA 17 10` at the preferred base; the embedded address is adjusted to
  the loaded module base. The thunk replays the table load and resumes +5.
  RVAs are stored in the 1.8.1 module profile. Installation failures propagate.
- No actual-terrain template mask, tile flag, overlap, ordering, or subsequent
  score checks are bypassed. Generation logs report both experimental counters.
- Debug/Release Win32 solution builds and compatibility tests passed. Tests
  cover gating, preserved accumulated flags, excluded terrains, and native
  fastcall fallback stack cleanup. Isolated Debug/Release editor probes passed:
  real Dirt/view 5 accumulates Dirt=3/Sand=1 natively and Dirt=3/Sand=0 through
  the selected helper; fallback restores native behavior. Full painted-map
  placement remains for user testing. Nothing committed.

Residual gap investigation (2026-09-20):
- User confirmed the transition strips are resolved, but isolated holes remain.
  Reproduction: `C:\Users\Andy\Repos\obstacle_test.h3m`, surface (4,23,0),
  manually placeable `avlmtdr1.def`; the saved map leaves the hole empty.
- Read-only binary inspection in ignored `scratch/inspect_gap_map.py` decoded
  147 templates and 1,202 static obstacle records, with 128 trailing bytes.
  The inferred object-section layout validates every template index and all
  five reserved bytes per record. Reconstructed local blocked cells match the
  screenshot: rows 20-22 and 24-25 filled, only x=4 empty on row 23 nearby.
- At anchor (4,23), this DEF blocks 12 cells: (0..3,21), (0..4,22),
  (2..4,23). Eleven already overlap mountains; only (4,23) is newly blocked.
- Extracted 1.8.1 RandTrn Dirt mountain weights: Dirt mountain adjacency 100,
  overlap 30; Swamp mountain adjacency/overlap -5000. Swamp mountains directly
  below the candidate make adjacency a specific hypothesis, not a proven
  runtime rejection. Drawing-order conflicts and candidate traversal remain
  alternatives. Same-terrain overlap is not categorically prohibited.
- No scorer trace for this exact saved state has run yet. Do not claim the
  final rejection cause or change collision/adjacency rules based solely on
  the static snapshot. No map, workbook, or production code was changed.

Targeted isolated scorer probe (supersedes the untraced status above):
- Runtime verified in a disposable private-desktop editor, not the user's
  open editor. `scratch/gap_probe.cpp/.vcxproj` initializes the native generator
  with 0x49D666, resolves wrappers by validated source-table order, and adds
  125 nearby saved obstacles through native 0x49B7DB/0x49E41F. The fixture
  contains source terrain/view data for the surface and neighbors within
  x<=20, y=10..36; it is not a full loaded-map reproduction. Original saved
  object order is preserved. No candidate object is placed or map saved.
- Constructor cells default to unpainted (flag 0x08000000). The probe clears
  that flag to reproduce painted eligibility. Initial unpainted runs failed
  before terrain accumulation and are not evidence of the residual-gap cause.
- Native scorer 1.8.1 RVA 0xCC3F0 returns -2000000001 at (4,23,0). With the
  same actual-terrain accumulator behavior as the plugin, it processes all
  12 blocked cells and still returns that hard-failure score.
- An observational hook at RVA 0xCCD45 replays expected bytes
  `C7 44 24 10 FF 6B CA 88` and resumes +8 without changing flags or score.
  It captures the rejected existing object's state before native cleanup:
  source template index 460 (`avlmtdr2.def`), anchor (5,23), flags +20..23
  = 01 01 01 01, overlap weight EAX=30. Both depth-order flags are set;
  the conditional branch at RVA 0xCCC2F therefore rejects this placement.
- Read-only comparisons using native 0x49B5EE depth matrices also find a
  conflict with `avlmtdr1.def` at (2,23). The runtime observed first rejection
  is the (5,23) object, not an adjacency-weight rejection.
- Conclusion: this exact candidate has a generator drawing-order conflict in
  the reconstructed final state, despite manual placement acceptance. This
  does not prove it was attempted in the original generation run or diagnose
  all remaining holes. Do not bypass ordering globally. No production hooks,
  builds, workbook, saved map, or user editor were modified for this probe.
  Evidence remains ignored in `scratch/gap_fixture.bin`, `gap_probe.txt`,
  `gap_helpers_*.json`, and the probe sources.

Mountain-only order experiment (2026-09-20, pending user visual testing):
- Added a separate guarded hook at module-profile RVA 0xCCC2F. Expected
  bytes `0F 85 10 01 00 00`; original fallthrough is +6, rejection +0x116.
  The thunk preserves registers/flags and checks the original condition first.
- Only active in ButtonSelection with the current allowed candidate template
  and row and a selected button. Both candidate and conflicting object must
  have native type 134 (mountain), and the candidate row's overlap weight for
  the existing object's row must be greater than -5000. Other types, missing
  rows, prohibited overlaps, and modes retain native drawing-order behavior.
- No adjacency penalties, terrain/paint restrictions, or later score handling
  are changed. Generation logs count permitted mountain-pair conflicts.
- Isolated probe now compiles the production helper/thunk directly. For the
  saved gap reconstruction, score changes from hard rejection -2000000001 to
  low-priority fallback -2000000000 with one exception; there is no subsequent
  hard rejection. Outside ButtonSelection the hard rejection returns. This is
  not a positive score or proof of eventual placement; native fallback choice
  and adjacency penalties still apply.
- Debug/Release Win32 builds, compatibility tests, and isolated initialization/
  parser probes pass. Regression tests cover non-mountains, prohibited overlap,
  row bounds, deselection, wrong candidate context, and inactive mode. No map
  was modified and nothing committed. Keep this experiment separate from the
  successful transition fix when deciding what to retain.

Minimap Lava discrepancy (2026-09-20):
- User accepted the mountain-only order experiment visually: remaining actual
  holes are in dunes. Lighter Lava minimap spots persist after save/reopen.
- Read-only inspection of the latest `obstacle_test.h3m` (1,246 objects)
  finds all 432 actual Lava tiles covered by saved blocked footprints.
- Statically verified: minimap paint 0x487B8F examines blocking object masks,
  then asks a selected object's virtual color method. Default object color
  0x4421A6 uses an object-type bitset, not simply aggregate passability.
  Initializer 0x441E0D uses 48 type IDs at 0x536890..0x536950. Mountain 134,
  dunes 148 and volcano 158 are included; volcano 157 is absent.
- Runtime verified in an isolated HotA 1.8.1 editor WITHOUT the plugin:
  real color method 0x4421A6 returns Lava 0x2529 for type 157 (same as type 0),
  versus 0x14A5 for types 134,148,158. The fixture exercises the native method
  with minimal type-bearing objects; it does not render the full minimap.
- Saved volcano types include 157 (AVLvol10/20/40/60.def) and 158
  (AVLvol30/50.def). Thus blocking volcanoes of type 157 can use the light
  passable-looking shade. This is native color classification, not evidence
  of missing Lava collision coverage or a transient refresh problem.
- Implemented a separate display-only fix in `plugin/minimap.cpp`. Guarded
  EXE hook 0x4421DE expects `81 FE E8 00 00 00` (`cmp esi,232`) and resumes
  at 0x4421E4. For local color-list index 157 only, substitutes 158, then
  replays the comparison. Native palette selection, other registers, stack
  and the comparison flags are preserved. No object metadata, placement,
  passability or map footprints change. Both target hashes are checked first;
  hook installation failures propagate through explicit initialization.
- Runtime verified: isolated native color calls for all 232 types across all
  12 terrain IDs, without plugin and with Debug/Release builds. Only type 157
  changes (Void has identical colors); Lava now returns dark 0x14A5. Debug
  and Release match exactly. Both Win32 builds and compatibility suites pass.
  User confirmed the full minimap looks correct.
  Evidence: ignored `scratch/ida_minimap.py/.json`, `minimap_probe.cpp/.txt`,
  and updated read-only map inspector. Map and production binaries unchanged.

Sand dune overlap experiment (2026-09-20):
- Latest revision: user observed bad dune placement that manual dragging also
  rejects. Removed the dune drawing-order exception; only mountain pairs may
  bypass that check again. Retained the private dune self-overlap weight of 30.
  The initial combined experiment below is historical, not the current policy.
- Revised isolated scorer probe permits 115 additional placements (previously
  152), all with zero drawing-order exceptions. Debug/Release builds and both
  compatibility suites pass, including rejection of dune-pair order bypass.
  User subsequently accepted full-map appearance, with two remaining bottom
  gaps also rejecting manual dune placement. General equivalence to manual
  placement for every layout is not claimed.
- Correction to the shorthand above: the workbook's Sand Dunes are the six
  AVLmtdn1..6.def templates, native type 140/subtype 6, not type 148.
- Runtime verified in an isolated native generator: all six use RandTrn row
  165, Sand weight 10, and self-overlap -5000. Therefore the mountain-only
  drawing-order exception alone cannot fix their gaps.
- Selected generation now builds a private dune row through the existing
  selected-object rules mechanism. Only its self-overlap weight becomes 30
  (matching mountains). Native rows, other overlap entries, adjacency rules,
  terrain restrictions, and footprints remain unchanged.
- The guarded drawing-order helper additionally accepts dune-to-dune pairs
  (both type 140/subtype 6), still requiring a permitted overlap weight and
  selected candidate context. Mixed dune/mountain and other type 140 pairs
  remain excluded. No new hook sites were added.
- Runtime probe `scratch/dune_probe.cpp` places one native dune on Sand in a
  disposable generator, then scores six dune candidates at 64 anchors each.
  152 previously hard-rejected placements become accepted; examples score 43
  with one drawing-order exception. This is a synthetic scorer test, not a
  guarantee that full-map generation fills every hole. User testing pending.
- Debug/Release Win32 builds, compatibility tests and isolated plugin startup
  pass. Tests cover dune subtype checks, mixed-pair exclusion, prohibited
  overlap, private self-overlap adjustment and unchanged source/other weights.
  No workbook or saved map changes. Nothing committed.

### Maple Generation and Shortcut Regression (2026-09-19)

- Statically verified: workbook Maple Trees use native type/subtype 140/12,
  but 1.8.1 RandTrn has no matching row. The selected-object fallback now also
  covers mapped objects with a null native row, not just custom-template
  buttons. Existing non-custom native rows remain unchanged. Regression tests
  cover Maples on Dirt only and preservation of an existing native row.
- The user reported Ctrl+Z, Ctrl+S, and Shift+O stop working after placement
  and do not recover after clicking the map. Hypothesis requiring validation:
  the newly introduced focusable category action BUTTONs disrupt accelerator
  routing, a previously documented editor integration problem. Their mouse
  handling now uses capture without taking focus; tab-stop style is removed.
  Native drawing and click notifications remain, with drag cancellation and
  wheel forwarding. No focus is forced to the editor's side toolbar.
- Debug/Release builds and compatibility tests pass. The user confirmed both
  Maple generation and keyboard shortcuts are fixed in a fresh editor session.
- Selected, enabled obstacle buttons now use a slightly darker blue fill
  (`#D2EBFC`), with corresponding hover/press shades and a 1px `#ABDEFC` border.
  Disabled appearance is unchanged. The user approved the colors on 2026-09-19.

### Tree Visuals and Preview Scaling (2026-09-19)

Workbook regeneration now includes icons and previews for all eighteen Trees
buttons. Willow Trees uses three previews and Carnivorous Plants uses one;
the others use four. The missing `avlhpn10.def` and `TPMTREE3.def` are mapped
to Spruces and Predatory Plants respectively and included in their previews.
Terrain masks are unchanged. The user approved the completed visuals.

Preview rendering now caps scaling at 1:1 source pixels, while still shrinking
larger images to fit and centering each image within its existing cell. Icon
scaling, popup dimensions, and one-to-four-image layouts are unchanged. The user
approved the no-upscaling preview on 2026-09-19. Debug and Release Win32 builds
passed. Source workbook: `obstacles_1.8.1.xlsx` at the authoritative path below.

### Category Selection Controls (2026-09-19)

Each category now starts with two native 18x18 push buttons, `+` and `-`, in a
22px-high row. Their tooltips are exactly `Select All` and `Select None`.
They change the saved selection of every obstacle button in that category,
including terrain-disabled buttons, without overriding terrain applicability.
The existing generation-button enablement is refreshed immediately. Controls
scroll with their category; category names remain hidden and separators remain.
The user confirmed the controls work and requested improved symbol centering.
Symbols now use centered 7px geometric strokes in native button custom-draw
postpaint, retaining native borders, hover, and pressed behavior instead
of relying on font baselines. Debug and Release Win32 builds passed.
A matching gray horizontal rule now starts the scrolling content below the
Obstacle Types header, with 8px before the first category selection controls.
The user approved the final appearance, including the separator, on 2026-09-19.

### Custom Animal/Miscellaneous Generation Repair (2026-09-19)

The all-land Animals/Miscellaneous test exposed two independent failures:
the installed game no longer had a loose `Data/objects.txt`, and widened masks
do not widen native RandTrn weights. The log confirmed every overlay attempt
failed to read the loose file. Frogs consequently retained native Swamp-only
generation; plugin-only templates were absent entirely.

- Overlay input now falls back to the editor's native text loader at
  `0x004DCCE0` (cdecl), preserving native archive decoding. Text resources expose
  row-pointer begin/end at `+32/+36`; rows expose cell begin/end at `+4/+8`.
  Release uses virtual method `vtable+4`, as in the native RandTrn parser.
- Statically verified: the final candidate hook at `0x0049E592` runs before
  rejection of a null RandTrn pointer, so it can repair unmatched candidates.
- In selected-button mode, buttons with workbook template overrides receive
  private per-template scoring rows at that hook. Explicit donors take priority,
  followed by the native row; unmatched objects use the captured native
  dirt-rock row (zero-based 9). This also avoids parser-order donor lookup gaps.
- The template mask intersected with the button mask controls terrain support;
  Rock/Void is always excluded. Newly enabled terrain weights use 5. When terrain
  support expands, disabled adjacency modifiers become 5; existing overlap
  restrictions and native object footprints are retained. Shared native rows
  are not modified. Copies remain stable through generation and reset before
  the next run. Unselected candidates remain rejected; vanilla mode bypasses
  the repair. Workbook files and generated metadata are unchanged.
- Debug/Release builds and compatibility tests passed, including private-copy
  isolation, water/void exclusion, sparse masks, null/malformed rows, selection
  rejection, stable row reuse, and vanilla bypass.
- Runtime verified in an isolated editor: native archive text loading (1,883
  object rows), temporary overlay loading through the native loader, and native
  terrain-predicate acceptance on ten land terrains for representative DEFs
  from all eleven Animals/Miscellaneous buttons. Water is rejected; Void is
  checked by unit tests because it has no native terrain-edge table.
- The user confirmed successful painted-map placement on 2026-09-19 and
  supplied a screenshot showing animals and miscellaneous objects across
  multiple land terrains. Undo/redo and mixed-category regression checks have
  not been separately confirmed. Smoke evidence is in ignored `scratch` files.

The workbook-driven 60-button implementation is now in the source tree. Its
authoritative inputs are the `Obstacles`, `Button Layout`, and `Button Mapping`
tabs in:

`C:\Users\Andy\My Drive\Heroes Stuff\HotA lod\obstacles_1.8.1.xlsx`

`tools/generate_obstacle_buttons.py` emits
`plugin/obstacle_button_data.{h,cpp}`. The generated data contains 60 buttons,
7 visual sections, 880 DEF-to-button mappings, 28 selected-mode `objects.txt`
template overrides, and 12 explicit RandTrn borrow sources. Regenerate it after
changing any of those workbook tabs; do not edit the generated files by hand.

Implemented UI behavior:

- the native Brush section and plugin-owned Place Obstacles overlay remain
  fixed;
- `Obstacle Types` is followed by a plugin-owned vertical scrolling viewport;
- the viewport preserves the workbook's four-column layout and row wrapping;
- thin rules separate the seven sections, without category text labels;
- viewport scrolling batches all child-window moves and repaints once, avoiding
  partial/intermediate button layouts while dragging the scrollbar;
- checked state comes from `Selected By Default`, persists independently of
  enabled state, and survives side-panel recreation/tool switches;
- painted-terrain applicability comes from `Button Mapping`; Rock remains
  unsupported;
- all 60 hover popups use the workbook button name. Buttons without workbook
  DEF visual selections use the existing square placeholder;
- every hover popup shows its button's valid terrain types from the generated
  terrain mask, excluding Rock/Void;
- only DEFs mapped to checked buttons pass the final native candidate hook;
- selected plugin/custom rows are replaced or appended in a temporary
  per-generation `objects.txt` overlay;
- explicit DEF-name RandTrn borrowing is generalized from the workbook.
  `self` and `native bucket` leave HotA's native row assignment unchanged.

All 20 `Raised Ground` buttons currently render their icon and preview from the
DEFs selected in `Button Mapping`. Icons are generated at 30x30; previews are
generated at 160x160. The popup remains 164 pixels wide with a 164x164 image
area and fixed caption position. Its `#c0c0c0` lower panel now includes the
caption and one or two rows of valid-terrain icons, making the total height 220
or 246 pixels. All 20 `Raised Ground` buttons use four DEFs in a 2x2 preview
grid. Preview order is left-to-right and then top-to-bottom by `Preview Def 1`
through `Preview Def 4`. The compositor continues to support one-, two-, and
three-DEF previews for later categories. The prior embedded-PNG implementation
was checkpointed in commit `8624e7b` and then removed.

All four `Lowered Ground` buttons (`Lakes`, `Lava`, `Tar Pits`, and `Broken
Ground`) also have workbook-selected icons and four-DEF previews.

Verification on 2026-07-13:

- workbook generation passed with the counts above;
- clean sequential Debug Win32 and Release Win32 rebuilds passed after the
  terrain indicators were added; the resulting DLLs are 784,896 bytes and
  132,608 bytes respectively;
- the editor started through the Debug launcher and all expected hook bytes,
  executable hashes, and metadata availability checks passed;
- user runtime testing confirmed all buttons paint immediately, scrollbar-thumb
  dragging is smooth, and painted terrain activates/deactivates the expected
  buttons;
- with `Sand Mountains` unchecked, generation placed only `Sand Dunes`,
  runtime-verifying checked-button candidate filtering;
- the first eight workbook-selected DEFs rendered as button icons and hover
  previews from both `H3sprite.lod` and keyed `HotA.lod` resources;
- metadata generation and sequential Debug/Release Win32 builds passed after
  populating DEF visuals for the remaining 12 `Raised Ground` buttons; runtime
  testing accepted the four-DEF 2x2 layout;
- workbook generation and sequential Debug/Release Win32 builds passed after
  expanding every `Raised Ground` button to four previews; runtime validation
  of the complete updated set is pending user testing;
- workbook generation and sequential Debug/Release Win32 builds passed after
  adding `grsmnt05.def`, revising selected third/fourth preview DEFs, and making
  `Dirt Mountains` a temporary three-preview layout example;
- workbook generation and sequential Debug/Release Win32 builds passed after
  restoring `Dirt Mountains` to four previews; runtime validation of the final
  updated set is pending user testing;
- user runtime testing confirmed the button viewport works both without a
  scrollbar at 4K/175% scaling and with only the final row scrollable at
  3440x1440/125% scaling;
- workbook generation and sequential Debug/Release Win32 builds passed after
  populating all four `Lowered Ground` icon and preview selections; runtime
  validation of those visuals is pending user testing;
- user visual testing accepted the runtime-rendered size, crop, shadows, and
  corrected low-palette black details;
- user runtime testing accepted the one-row and two-row terrain panels,
  including the corrected Highlands/Wasteland/Water strip mapping and the
  10-icon `6+4` layout shown by `Crumbled Structures`;
- user visual testing accepted increasing DEF-rendered button icons from 24x24
  to 30x30, leaving a one-pixel gap inside each 32x32 button frame; a temporary
  1.25x centered zoom-and-crop experiment was rejected and removed;
- generalized RandTrn borrowing still needs an isolated runtime test.

## User Goals And Decisions

- Keep the successful garrison object-limit increase.
- Revert the primary-skill-booster limit increases.
  - Raising those limits above 32 allowed additional objects to be placed, but
    testing showed that the additional objects behaved as though a hero had
    already visited them.
- Add more editor enhancements over time, not just one obstacle feature.
- Preserve vanilla editor behavior unless a new feature is explicitly active.
- The user now has access to the real HotA 1.8.0 `objects.txt`, obtained through
  separate LOD archive work. It is not currently present in this repository.
- For a future thick-obstacle mode, "thick" means visually thick/tall or useful
  for forming borders, such as trees and mountains. It is not reliably inferred
  from footprint size, blocked-tile count, or object type.
- The preferred thick-obstacle design is to derive alternate eligibility values
  from a custom `objects.txt`, embed the required values in the DLL, and apply
  them only while the custom mode is active. Do not ship an external custom
  `objects.txt`.
- The user provided a curated obstacle workbook:
  `C:\Users\Andy\My Drive\Heroes Stuff\HotA lod\obstacles.xlsx`.
  The workbook has evolved into the source of truth for the planned category
  obstacle UI. Use the `Obstacles` tab for DEF/category/filter metadata; do not
  infer current behavior from the early four-column export mentioned in older
  notes.
- Repeated placement is a separate behavior:
  - additional activations should try to add obstacles over/among existing
    obstacles wherever the editor's native placement validator permits;
  - it must not merely avoid all existing obstacles.
- Computer Use is recommended for operating IDA and testing the map editor GUI,
  but direct binary analysis and scripted diagnostics should still be preferred
  when they are more repeatable.

## High-Level Recommendation

Continue in this repository, but treat the next work as a plugin-foundation
cleanup before implementing more features.

Reuse:

- the launcher/injection concept;
- the proven garrison hook sites;
- the existing IDA databases and recovered reverse-engineering findings;
- the current Visual Studio x86 solution.

Replace or harden:

- unvalidated absolute-address hooks;
- initialization performed directly from `DllMain`;
- any future repeated-obstacle/cache-swapping approach, which must be
  revalidated before use;
- the inherited garrison-cap mutation pattern before copying it to any other
  feature.

## Repository State

Repository:

`C:\Users\Andy\Repos\h3-hota-map-editor-plugin`

Branch:

`main`, tracking `origin/main`

Branch policy:

Keep active work on `main` unless the user explicitly asks for a separate
branch. The previous `codex/phase-1-foundation` and
`codex/obstacle-row-filter` branches were consolidated into `main` on
2026-06-19.

Important earlier commit:

`0aa4616 Rename project and raise garrison limit from 48 to 255`

The garrison-only implementation can be recovered from that commit. Do not
blindly reset or revert the worktree; preserve the user's uncommitted work.

### Thick/Thin Follow-Up Work

The follow-up work after the obstacle row filter adds thick/thin obstacle variant
infrastructure:

- `plugin/obstacle_classification.h`
- `plugin/obstacle_classification.cpp`
- `plugin/obstacle_variants.h`
- `plugin/obstacle_variants.cpp`
- `tools/generate_obstacle_classification.py`
- updates to `plugin/dllmain.cpp`, `plugin/obstacle_filter.*`,
  `plugin/plugin.vcxproj`, and this handoff file.

Ignored local artifacts:

- `HotA_1.7.3.lod`
- `HotA_1.8.0.lod`
- `outputs/`
- `scratch/`

The inactive repeated-obstacle/cache-swapping scaffold was deleted on
2026-06-19 before committing because it was experimental, unused at runtime, and
not part of the verified obstacle-row filter. The LOD archives are proprietary
game assets and must not be committed.

### Current Build State

Both configurations build successfully when built sequentially:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe' `
  HotaMapEdPlugin.sln /m:1 /p:Configuration=Debug /p:Platform=Win32 /v:minimal

& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe' `
  HotaMapEdPlugin.sln /m:1 /p:Configuration=Release /p:Platform=Win32 /v:minimal
```

The plugin project now writes `HotaMapEdPlugin.plugin.pdb`, which avoids the
previous shared-PDB collision with the launcher. Sequential builds are still the
documented safe command path.

## What The Current Code Actually Does

### Launcher

`launcher/launcher.cpp`:

- finds the HotA install directory;
- launches `h3hota_maped.exe` normally;
- waits for the editor input loop with `WaitForInputIdle`;
- injects `HotaMapEdPlugin.dll` using remote `LoadLibraryA`.

Launching normally and waiting before injection fixed a keyboard-routing problem
caused by the earlier suspended-process injection approach.

### Current Object-Limit Hooks

`plugin/objlimits.cpp` is now garrison-only. The unsafe primary-skill-booster
limit increases are not active.

The garrison hooks still mutate `[type_info+8]` to `0xFF` when the custom limit
is reached. That behavior is inherited from the earlier working implementation;
review whether the mutation is actually required before copying the pattern to
any other feature.

### Current Obstacle Scaffold

The old repeated-obstacle/cache-swapping scaffold was removed before the
obstacle row-filter work was committed. Do not resurrect that path without new
evidence. Current obstacle work is based on the verified HotA row-predicate hook
and new side-panel command plumbing.

## Target Binaries And Local Analysis Assets

Game directory:

`C:\Users\Andy\Games\3DO\Heroes3`

IDA Professional:

`C:\Program Files\IDA Professional 9.3\ida.exe`

### Target Binary Profile

`h3hota_maped.exe`

- x86 PE
- fixed image base: `0x00400000`
- relocations stripped
- size: `2,772,992`
- SHA-256:
  `CCF1E8424D913AA409ECC168F87FB20EDD64C3A56D412F2878065FC261BC6113`
- IDA database:
  `C:\Users\Andy\Games\3DO\Heroes3\h3hota_maped.exe.i64`

`hota_me.dll`

- size: `2,715,648`
- SHA-256:
  `3CE4489A0B891E35C13848C33D1124F750D1A0C96D0B52761CA87BFFF5BC0D8E`
- exports `MainProc`
- IDA database:
  `C:\Users\Andy\Games\3DO\Heroes3\hota_me.dll.i64`

`HotA.dll`

- size: `2,513,920`
- SHA-256:
  `0A1DAA1D8F29870B5CB72EBBA54A88C43A366473530B908BD23FAFC7968223A7`
- IDA database:
  `C:\Users\Andy\Games\3DO\Heroes3\HotA.dll.i64`

Treat the original binaries as read-only. IDA databases may be updated with
better names, types, comments, and analysis.

## Verified Garrison/Object-Limit Hook Sites

These addresses and instructions were confirmed directly against the target
`h3hota_maped.exe` hash above.

### Create Check

- site: `0x004212A4`
- containing function previously named `sub_421222`
- registers: `eax = type_info*`, `ecx = current count`
- original bytes/instructions:

```asm
3B 48 08       cmp ecx, [eax+8]
72 28          jb  0x004212D1
```

### UI Predicate

- site: `0x004232D4`
- containing function previously named `sub_423278`
- registers: `esi = type_info*`, `eax = current count`
- original instruction sequence begins:

```asm
3B 46 08             cmp eax, [esi+8]
0F 83 A8 01 00 00    jae 0x00423485
```

### Place Check

- site: `0x00426AFA`
- containing function previously named `sub_426A49`
- registers: `eax = type_info*`, `ecx = current count`
- original bytes/instructions:

```asm
3B 48 08       cmp ecx, [eax+8]
72 28          jb  0x00426B27
```

The packed identifier at `[type_info+0]` combines type and subtype. Garrison is
`0x00000021`. Colosseum of the Magi was observed as `0x00900002`.

## Recovered Obstacle Reverse Engineering

The following was recovered from prior Visual Studio Copilot chats and should be
treated as strong leads, not final truth, until checked in the existing IDA
database or with runtime tests.

### Static Findings From 2026-06-18 Rand-Trn Pass

These findings were checked with IDAPython against copied IDA databases, leaving
the user's open IDA session untouched.

- `h3hota_maped.exe` message map entry `0x0053F990` maps:
  - `WM_COMMAND` (`273`);
  - command ID `32927` (vanilla obstacle command / `Shift+O`);
  - handler `sub_484B07` via function pointer cell `0x0053F9A4`.
- `sub_484B07` is therefore statically verified as the vanilla obstacle command
  handler, despite IDA showing no direct xrefs to the function.
- `sub_484B07` calls `sub_4AFBED` twice under map/tool-state conditions.
- `sub_4AFBED` stack-constructs the random object placer with `sub_4AF5B7`,
  calls `sub_4AFA64`, then destructs it.
- `sub_4AF5B7` calls `sub_49D666`, the random-generator base initializer.
- `sub_49D666` calls `sub_49D76E`.
- `sub_49D76E` loads `objects.txt`, filters/copies object records, then calls
  `sub_49DA04` to parse `rand_trn.txt`.
- The base executable parser `sub_49DA04` still has the original terrain-column
  shape: it reads object ID from column 3, subtype from column 4, terrain ID
  from column 6, and the original 9 terrain modifier columns.
- HotA's editor DLL has an expanded parser at `hota_me.dll:0x100D67A0`.
  It reads the same row identity fields, reads modifier columns for internal
  terrain IDs `0..8`, forces internal terrain ID `9` to `-5000`, then reads
  additional modifier columns for IDs `10+`.
- `hota_me.dll` terrain-count global `dword_10173ABC` is initialized to `10`
  but written in `sub_100D79C0` as the max terrain ID plus one from the terrain
  definitions. In the current `rand_trn.txt`, Highlands rows use terrain ID
  `10` and Wasteland rows use terrain ID `11`, so runtime count is expected to
  become `12`.
- With that count, `rand_trn.txt` has 11 visible modifier columns because
  internal terrain ID `9` is hidden/reserved. The `Adjacent` matrix begins after
  those visible modifiers, and `Overlap` begins after the `Adjacent` matrix.

Still requiring runtime validation:

- Confirm by breakpoint/logging that HotA's expanded parser and terrain count
  are active before obstacle placement in the live editor.
- Confirm whether `Adjacent` and `Overlap` are both consulted during editor
  obstacle placement, not only during other random generation workflows.
- Identify the narrowest safe hook for thick/thin candidate filtering without
  replacing the global object/cache structures.

### Static Findings From 2026-06-18 Rand-Trn Continuation

These findings were checked with IDAPython against copied IDA databases in
`scratch\h3hota_maped_batch.i64` and `scratch\hota_me_batch.i64`, leaving the
user's open IDA session untouched.

- Computer Use was attempted but was unavailable in this session because the
  Computer Use client failed during module loading with an `@oai/sky` package
  export mismatch. The analysis below is therefore static IDAPython evidence,
  not GUI/runtime evidence.
- `hota_me.dll:MainProc` calls `sub_100D79C0` at `0x100F8F12`.
- `sub_100D79C0` is more than a terrain-count setter:
  - it scans terrain definitions and raises `dword_10173ABC` to the max terrain
    ID plus one;
  - it reallocates terrain-sized tables using `dword_10173ABC`;
  - it builds expanded message/accelerator-like tables from base editor data;
  - it references `sub_100D9760`, a small wrapper that calls the expanded
    `rand_trn.txt` parser `sub_100D67A0`.
- The base executable has no plain operand references inside `hota_me.dll` to
  the old parser/placer addresses (`0x0049DA04`, `0x0049D76E`,
  `0x0049D666`, `0x004AF5B7`, `0x004AFBED`, `0x004AFA64`, or
  `0x00484B07`). HotA's patch engine uses encoded/generated patch data, so the
  absence of plain operands is not evidence that no runtime patch exists.
- Base editor `sub_49D76E` loads `objects.txt`, filters copied object-template
  records by object type, and stores per-type runtime wrapper pointers in
  vectors rooted at `this + 0x34 + 0x10 * object_type`.
- Base editor `sub_49DA04` assigns two important wrapper fields:
  - at `0x0049DDC0`, `wrapper[+0x04] = first eligible terrain bit` from the
    object template terrain mask at `object_template + 0x18`;
  - at `0x0049DE35`, `wrapper[+0x10] = rand_trn-derived value` selected by
    object type/group, terrain, and object identity.
- HotA's expanded parser `sub_100D67A0` performs the analogous assignments with
  the expanded terrain count:
  - at `0x100D6DA3`, `wrapper[+0x04] = first eligible terrain bit`, searching
    up to `dword_10173ABC`;
  - at `0x100D6DFE`, `wrapper[+0x10] = expanded rand_trn-derived value`.
- `sub_4AF8D9`, called by the random object placer constructor, looks up or
  creates placement/cache records for object templates. It matches existing
  records by:
  - first eligible terrain (`wrapper[+0x04]`);
  - object identity field at `object_template + 0x20`;
  - a 0x4C-byte object-template prefix comparison.
  When it creates a duplicate record, it copies the existing
  `wrapper[+0x10]` value into the duplicate at `0x004AF9CD`.
- This makes the narrowest currently identified thick/thin hook candidate the
  parser-time assignment of `wrapper[+0x10]`, not replacing the whole global
  object/template cache. The HotA-aware candidate site is
  `hota_me.dll:0x100D6DFE`; the base-editor analogue is
  `h3hota_maped.exe:0x0049DE35`.

Still requiring validation:

- Runtime-verify that HotA's generated patch path routes obstacle placement
  through `hota_me.dll:sub_100D67A0` before using `0x100D6DFE` as a live hook
  site.
- Determine the exact semantics and allowed value range of `wrapper[+0x10]`
  for obstacle inclusion/exclusion before changing it.
- Compare vanilla HotA 1.8.0 `objects.txt` with the intended custom table to
  derive a small committed eligibility table, not a full external
  `objects.txt`.

### Runtime Findings From 2026-06-18 Rand-Trn Diagnostics

These findings were produced with temporary diagnostics gated by
`HOTA_MAPED_PLUGIN_DIAGNOSTICS=1`. The diagnostics are useful investigation
scaffolding, not a finished feature.

Runtime verified:

- `hota_me.dll:0x100D67A0` is entered during obstacle generation.
- `hota_me.dll:0x100D6DFE` stores HotA-expanded `rand_trn.txt` row records into
  wrapper `+0x10`.
- Wrapper `+0x10` is not a direct scalar terrain weight. It points to a
  76-byte HotA row record.
- HotA row record layout observed at runtime matches the static constructor
  analysis:
  - row `+0x00` is the row id;
  - row `+0x04` is a holder pointer;
  - `*(row + 0x04)` is the actual terrain-weight array pointer;
  - `*(*(row + 0x04) + terrain * 4)` gives the terrain modifier.
- Final candidate records in `sub_49E452` still load candidate record `+0x10`
  at `h3hota_maped.exe:0x0049E592`. This hook was runtime verified active.
- Final candidate records with `firstTerrain=10` and `firstTerrain=11` reached
  the final candidate path on the user's all-terrain test map.

Statically verified from the live-generated patch dispatch:

- The live bytes at `h3hota_maped.exe:0x0049E5A3` are not the base executable
  terrain-weight compare. HotA replaces that site with a near jump into
  generated patch-wrapper code.
- The generated wrapper dispatches through obfuscated stubs back into
  `hota_me.dll` functions, including targets near:
  - `0x100C6F90`;
  - `0x100C7330..0x100C77F0`;
  - `0x1010EE50..0x1010F0B0`.
- `hota_me.dll:0x100C53B0` is a HotA-aware terrain/weight predicate. Its
  decompiled form reads the actual weight vector through `**(a1 + 4)`, checks
  terrain-indexed weights against `-5000`/`-4000`, and consults
  `dword_10173AC0` terrain relationship data.

Important corrected assumptions:

- The old base-executable interpretation of row `+0x04` as an inline weight
  slot is wrong for HotA 1.8.0.
- The old static compare at `0x0049E5A0` is not a reliable hook point in the
  live HotA editor because HotA has already patched over the compare.
- The current diagnostic `terrain`/`cellTerrain` field in
  `finalCandidate[+0x10]` logs is not reliable; it was observed as `0` even
  when the test map used all terrain types. Do not base feature logic on that
  capture without fixing the function-frame interpretation.

Hypotheses requiring validation:

- HotA's generated wrapper around `0x0049E5A3` appears to handle or skip the
  original inline compare path; a diagnostic hook on the generated copy of the
  original compare installed successfully but did not fire during the user's
  obstacle-generation pass.
- The narrow hook for future thick/thin filtering should probably be either:
  - the parser-time row selection/store at `hota_me.dll:0x100D6DFE`; or
  - the HotA-aware predicate/dispatch path around `hota_me.dll:0x100C53B0`,
    after proving the patch-callback ABI.
  Do not hook the base `0x0049E5A0` compare.

### Workbook-Based Obstacle Filter Prototype From 2026-06-19

The user-provided workbook was compared against the live HotA 1.8.0 data files:

- workbook:
  `C:\Users\Andy\My Drive\Heroes Stuff\HotA lod\obstacles.xlsx`;
- `rand_trn.txt`:
  `C:\Users\Andy\Games\3DO\Heroes3\Data\rand_trn.txt`;
- `objects.txt`:
  `C:\Users\Andy\Games\3DO\Heroes3\Data\objects.txt`.

The comparison script is `scratch/analyze_obstacle_sources.py`; its output is
`scratch/obstacle_source_analysis.json`.

Statically verified from data comparison:

- workbook rows: 855;
- workbook unique DEFs: 844;
- workbook unique `(object ID, subtype)` pairs: 66;
- `rand_trn.txt` rows: 200;
- `rand_trn.txt` unique `(object ID, subtype)` pairs: 73;
- `objects.txt` declared rows: 1865;
- `objects.txt` unique DEFs: 1847.

`rand_trn.txt` row counts by terrain ID:

- 0: 24
- 1: 25
- 2: 25
- 3: 16
- 4: 26
- 5: 18
- 6: 17
- 7: 14
- 8: 2
- 10: 17
- 11: 16

Workbook object keys absent from `rand_trn.txt`:

- `(139, 20)` Carnivorous Plant / `avswplnt.def`
- `(140, 12)` Maple Trees / multiple tree DEFs
- `(140, 13)` Natural Arch / `avlmtwl7.def`
- `(140, 14)` Glacier / `avlglc01.def` through `avlglc06.def`
- `(177, 0)` Lake 2 / `avllk1r.def`
- `(199, 0)` Trees 2 / multiple tree DEFs
- `(204, 0)` Yucca Trees 2 / `avlyuc40.def`, `avlyuc50.def`,
  `avlyuc60.def`

`rand_trn.txt` object keys absent from the workbook:

- `(53, 0)` sawmills, 10 terrain rows
- `(53, 4)` crystal mines, 10 terrain rows
- `(53, 6)` gold mines, 10 terrain rows
- `(139, 0)` sand crates box
- `(139, 1)` sand crates boxes
- `(139, 2)` sand crates sack
- `(139, 3)` sand crates casks
- `(139, 5)` sand crates rope
- `(139, 8)` dirt hnh
- `(139, 9)` dirt rooster
- `(139, 13)` dirt pig
- `(139, 23)` sand crates sacks
- `(139, 24)` highlands puddle
- `(139, 26)` wasteland limestone puddle

User correction on 2026-06-19: sawmills, crystal/gold mines, crates, and dirt
animals are not considered obstacles by the map editor's obstacle tool even
though they appear in `rand_trn.txt`. Do not use those rows as visual evidence
when validating the obstacle-tool feature. The raw `rand_trn.txt` comparison is
still useful data, but the true obstacle-tool candidate set is narrower than
all rows in `rand_trn.txt`.

Other data mismatches:

- Workbook DEFs absent from the installed `Data\objects.txt`:
  `avlbon01.def`, `avlglc01.def`, `avlglc02.def`, `avlglc03.def`,
  `avlglc04.def`, `avlglc05.def`, `avlglc06.def`, and `avlmtwl7.def`.
  The six `avlglc*.def` glacier rows are now plugin-supplied during Thick
  obstacle generation only. `avlbon01.def` and experimental `avlmtwl7.def`
  support are now plugin-supplied during Thin obstacle generation.
- Workbook row `avlr11r0.def` says Pile of Stones is object ID 140 subtype 4,
  but HotA 1.8.0 `objects.txt` has that DEF as object ID 152 subtype 4.

Implementation added:

- `plugin/obstacle_filter.h`
- `plugin/obstacle_filter.cpp`
- `plugin/dllmain.cpp` initialization for the filter
- `plugin/plugin.vcxproj` entries for the filter files

The filter is disabled by default. It is enabled only when
`HOTA_MAPED_PLUGIN_OBSTACLE_FILTER` is set to `1`, `true`, `yes`, or `on`.
For targeted runtime probes, the same hook can also be enabled with
`HOTA_MAPED_PLUGIN_BLOCK_OBSTACLE_IDS`, a comma/semicolon/whitespace-separated
list of one-based `rand_trn.txt` obstacle IDs to reject. When only this test
list is set, the workbook-derived allowlist remains off.

Runtime/startup verified:

- target module: `hota_me.dll`;
- required SHA-256:
  `3CE4489A0B891E35C13848C33D1124F750D1A0C96D0B52761CA87BFFF5BC0D8E`;
- hook site: `hota_me.dll` RVA `0x000C53B0`;
- absolute address for the verified HotA 1.8.0 profile:
  `hota_me.dll:0x100C53B0`;
- expected first bytes:
  `8B 41 04 56 8B 30`;
- original instructions emulated by the hook:

```asm
mov eax, [ecx+4]
push esi
mov esi, [eax]
```

The hook checks the HotA expanded `rand_trn.txt` row ID before the native
terrain/weight predicate runs. The embedded table allows 159 of the 200
`rand_trn.txt` rows and rejects the 41 rows whose `(object ID, subtype)` key is
absent from the user workbook. Unknown, unreadable, or out-of-range rows are
allowed to preserve vanilla behavior.

Startup verification passed on 2026-06-19:

```powershell
$env:HOTA_MAPED_PLUGIN_OBSTACLE_FILTER = '1'
.\Debug\HotaMapEdPlugin.exe
```

Fresh log excerpt:

```text
Obstacle filter requested by HOTA_MAPED_PLUGIN_OBSTACLE_FILTER.
Obstacle filter: hota_me.dll SHA-256: 3CE4489A0B891E35C13848C33D1124F750D1A0C96D0B52761CA87BFFF5BC0D8E
Obstacle filter installed: predicate=100C53B0 allowedRows=159 rejectedRows=41
```

GUI/runtime validation on the user's all-terrain test map passed far enough to
prove the hook is in the live obstacle-generation path. With
`HOTA_MAPED_PLUGIN_OBSTACLE_FILTER=1`, the filter rejected:

- row ID 172 / obstacle ID 173: `highlands puddle`, object ID 139 subtype 24;
- row ID 183 / obstacle ID 184: `wasteland limestone puddle`, object ID 139
  subtype 26.

The user reported that nothing looked strange at first glance after generating
obstacles. This is not a full visual acceptance test, but it is runtime evidence
that the predicate hook can filter HotA 1.8.0 expanded `rand_trn.txt` rows during
the editor obstacle command.

Visible targeted validation on 2026-06-19:

- test environment:
  - `HOTA_MAPED_PLUGIN_OBSTACLE_FILTER` unset;
  - `HOTA_MAPED_PLUGIN_BLOCK_OBSTACLE_IDS=7`;
- `rand_trn.txt` obstacle ID 7 is `dirt mountain`, object ID 134 subtype 0,
  terrain ID 0;
- startup log confirmed `workbookFilter=off` and `extraBlockedRows=1`;
- runtime log repeatedly rejected `rowId=6 obstacleId=7
  reason=extra-block-list`;
- the user visually confirmed that blocking dirt mountains worked: dirt
  obstacles still generated, but the large dirt mountains did not appear.

This is runtime and visual evidence that returning false from
`hota_me.dll:0x100C53B0` for a selected `rand_trn.txt` row prevents that obstacle
row from being generated by the map editor obstacle command.

Still requiring validation:

- Do a more careful visual pass if the filter is promoted from prototype to
  feature, especially on highlands and wasteland where the confirmed rejected
  rows came from.
- Inspect the resulting map for whether any obstacle-tool candidates outside
  the user's workbook still appear. Do not count sawmills, mines, crates, or
  dirt animals here; the user confirmed those are not obstacle-tool objects.
- Confirm vanilla obstacle generation remains unchanged when the environment
  variable is unset.
- This prototype only filters existing `rand_trn.txt` rows. It does not add the
  workbook objects that are absent from `rand_trn.txt`, such as Maple Trees,
  Glaciers, Trees 2, or Yucca Trees 2.

Files added or changed during this diagnostic pass:

- `plugin/obstacle_diagnostics.h`
- `plugin/obstacle_diagnostics.cpp`
- `plugin/dllmain.cpp`
- `plugin/target_profile.h`
- `plugin/target_profile.cpp`
- `plugin/logger.cpp`
- `plugin/plugin.vcxproj`
- `scratch/ida_obstacle_consumer_probe.py`
- `scratch/ida_hota_row_structure_probe.py`
- `scratch/ida_hota_generated_patch_targets.py`

Build verification after the diagnostic changes:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe' `
  HotaMapEdPlugin.sln /m:1 /p:Configuration=Debug /p:Platform=Win32 /v:minimal

& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe' `
  HotaMapEdPlugin.sln /m:1 /p:Configuration=Release /p:Platform=Win32 /v:minimal
```

Both builds succeeded on 2026-06-18 for the diagnostic pass, and again on
2026-06-19 after adding the env-gated obstacle filter.
They also succeeded on the new system on 2026-06-20 with Visual Studio 18
Community at the path shown above.

### UI And Validation Path

- Vanilla obstacle command ID: `32927` (`Shift+O`).
- Static UI resource findings from 2026-06-19:
  - PE resource type `241`, ID `352`, is a toolbar template with one command:
    `32927`.
  - PE dialog resource ID `347` is the `TObstacleToolkit` side-panel dialog.
    It has one static placeholder control at dialog units
    `x=7, y=7, w=106, h=8`, control ID `1001`.
  - `h3hota_maped.exe:0x00492680` constructs the obstacle toolkit. It creates
    the one-button toolbar from resource `352` and a second toolbar from
    resource `350`, then lays them out under the placeholder.
  - Helper `h3hota_maped.exe:0x004C0C81` constructs the toolkit toolbar,
    loads the toolbar resource, sends `TB_SETROWS` (`0x427`) to size it, and
    moves the toolbar window to the returned rectangle.
  - This matches the side-panel button in the screenshot whose tooltip is
    `Place Obstacles [Shift+O]`.
- Practical implication for thick/thin buttons:
  - The new buttons should attach to the `TObstacleToolkit` side-panel, next to
    the existing resource-352 toolbar button.
  - Click handling should set a temporary filter mode and invoke command
    `32927` through the same command path used by the existing side-panel
    generation button.
  - Do not model this as a separate obstacle-painting tool mode. The user flow
    is: select obstacle tool, paint the area, then choose the normal/thick/thin
    generation button in the side panel.

Implementation scaffold added on 2026-06-19 and DEF-aware filtering added on
2026-06-20:

- `plugin/obstacle_variants.cpp` discovers the side-panel toolbar containing
  command `32927`.
- It subclasses the toolkit panel and posts a private registered message so the
  actual Thick/Thin button creation runs on the editor UI thread, then restores
  the original panel window procedure immediately.
- It uses command IDs `0x7001` and `0x7002` for the plugin's Thick/Thin side
  buttons, avoiding the old `0xE100` MFC collision.
- Clicking a variant button sets `ObstacleGenerationFilterMode::ThickOnly` or
  `ObstacleGenerationFilterMode::ThinOnly`, sends the vanilla command `32927`,
  then clears the mode.
- The visible Thick/Thin controls are non-focus `STATIC` child controls with
  `SS_NOTIFY`; they subclass only themselves for mouse clicks. The plugin no
  longer persistently subclasses the editor's obstacle toolkit panel because
  that broke keyboard shortcuts/accelerators during manual testing. Plain
  static controls without `SS_NOTIFY` were not clickable.
- The UI is enabled only when a generated thickness table is present, or when
  `HOTA_MAPED_PLUGIN_OBSTACLE_VARIANTS=1` is set for diagnostics.
- `tools/generate_obstacle_classification.py` now generates a compact
  DEF-name classification table from:
  - the user-curated `obstacles.xlsx` with `Def` and `Thickness` columns;
  - the local HotA 1.8.0 `objects.txt`, used only as a warning/sanity source
    for workbook DEFs that are not in that file.
- The generated table is numeric `FNV-1a(lowercase DEF filename) -> thickness`;
  it does not embed local paths, raw `objects.txt` rows, workbook rows, or DEF
  filenames.
- The plugin registers the editor's runtime `DEF key -> thickness` mapping
  while `objects.txt` is parsed. Do not try to reproduce the editor's private
  DEF-key assignment offline from file order.
- The thick/thin lookup intentionally does not use `(object ID, subtype)` as
  the discriminator. Multiple visual variants can share those values, and at
  least one workbook/editor mismatch (`avlr11r0.def`) would be dropped by an
  exact ID/subtype match.
- Current generated table:
  - 844 workbook DEF classifications;
  - 844 generated DEF-name hashes;
  - 8 plugin-supplied DEFs absent from HotA 1.8.0 `objects.txt`:
    `avlbon01.def`, `avlmtwl7.def`, and `avlglc01.def` through
    `avlglc06.def`;
  - 0 workbook DEFs missing from either `objects.txt` or the plugin overlay.
- `plugin/obstacle_filter.cpp` keeps the HotA `rand_trn.txt` row predicate for
  the workbook allowlist and `HOTA_MAPED_PLUGIN_BLOCK_OBSTACLE_IDS`
  diagnostics only. Thick/thin filtering moved out of that row hook because
  row identity is too coarse.

Rejected hook attempt:

- `h3hota_maped.exe:0x004AF82F` was statically identified as a candidate
  predicate with object-template access, but runtime testing showed it is not
  in the live obstacle generation path reached by the side-panel command.
- Do not use `0x004AF82F` for thick/thin filtering without new runtime evidence.

Current DEF-aware registration and final-candidate hooks:

- runtime registration hook site: `h3hota_maped.exe:0x0049061D`;
- expected first bytes:

```asm
8B F0           mov  esi, eax
8D 4D CC        lea  ecx, [ebp-34h]
```

- this site is immediately after `sub_490162(a2, Buf2)` returns in the
  `objects.txt` row parser `sub_490543`;
- at that point, `eax` is the object-template pointer and `[ebp-64h]` is the
  editor's small-string object containing the DEF filename;
- the hook registers the object template's actual `+0x00` DEF key against the
  generated DEF-name hash table, then emulates the two stolen instructions and
  resumes at `0x00490622`;
- status: runtime verified on 2026-06-20. Temporary diagnostics showed real
  DEF names being registered, for example `avlautr0.def` as thick and
  `avlbuzr0.def` as thin.

- hook site: `h3hota_maped.exe:0x0049E592`;
- expected first bytes:

```asm
8B 40 10        mov  eax, [eax+10h]
3B C3           cmp  eax, ebx
```

- before the hook, `eax` is the final candidate record and `edi` has already
  loaded `record[+0x00]`, the copied object-template pointer;
- record `+0x00` is the copied object-template pointer;
- record `+0x10` is the HotA expanded `rand_trn.txt` row pointer;
- object template `+0x00` is the DEF-derived key;
- object template `+0x1C` is object ID;
- object template `+0x20` is subtype;
- disallowed Thick/Thin candidates jump to `0x0049E69E`, matching the nearby
  native rejection/continue path used by row equality and terrain-weight
  rejection.

Runtime verified on 2026-06-20:

- Debug startup/injection succeeded on the new system.
- The earlier candidate hook installed at `0x004AF82F`, but later manual
  testing showed no candidate observations/rejections there.
- The runtime DEF-key registration hook installed at `0x0049061D`.
- The corrected final-candidate hook installed at `0x0049E592` with the
  generated table present.
- The obstacle variant UI thread started and created the side-panel Thick/Thin
  controls:

```text
Obstacle variants installed: objectDefRegistration=0049061D finalCandidateRecord=0049E592 thicknessTable=available trace=off
Obstacle variants: installed side-panel buttons ... thicknessTable=available
```
- Manual Thick generation on the user's four-terrain test area produced
  visually thick results with the expected sand and snow mountain families,
  plus dense trees/mountains. The user described it as doing "a pretty good
  job".
- Manual Thin generation produced visually thin/sparse results and rejected
  thick mountain candidates in the log.
- Keyboard shortcuts remained working after clicking Thick/Thin once the plugin
  stopped forcing focus back to the side-panel toolbar.
- Static follow-up on the Thin boundary gaps identified
  `h3hota_maped.exe:0x0049DF11` as the native candidate scoring/overlap
  function called from `sub_49E452` after the current DEF filter point. It reads
  the candidate record's `rand_trn.txt` row pointer at record `+0x10`,
  accumulates terrain weights, consults overlap/depth state for already placed
  nearby objects, and returns a score. Scores `<= 0` are not accepted by the
  caller at `0x0049E642`. This supports the current hypothesis that some Thin
  gaps are caused by native overlap/depth scoring after our filter, not by
  wrong workbook classification.
- Detailed final-candidate accepted/rejected logs are now gated by
  `HOTA_MAPED_PLUGIN_OBSTACLE_VARIANT_TRACE=1`; keep this off for ordinary
  manual testing because repeated clicks can produce large logs.
- Runtime startup with trace enabled showed that HotA has already replaced
  `0x0049E642` with a near jump into generated patch code. An attempted trace
  hook that patched this live jump and chained to HotA's generated target caused
  Thin generation to place no objects and produced summaries with
  `scoreSeen=0` for every row. Treat the HotA jump-chain score hook as
  disproven. The current code contains no score-path hook and uses
  final-candidate tracing only. Expected startup log example:

```text
Obstacle variants trace: score-path hooks disabled; using final-candidate trace only.
Obstacle variants installed: ... trace=on
```

- Computer Use could list the editor windows on 2026-06-20, but the traced
  editor window captured as a black surface and could not be activated
  reliably. A manual Thin generation with the unsafe score hook enabled proved
  the score hook was invasive, not that Thin mode has no valid candidates.
  Further runtime tracing should use final-candidate summaries only until a
  safe score observation point inside HotA's generated wrapper is identified.
- Remaining observed visual artifacts from a zoomed-out all-terrain test where
  every tile was painted: Thick filled terrain well overall, with water mostly
  filled but showing a few gaps. Thin filled terrain generally but left one-tile
  boundary gaps around/between some terrains and a two-tile gap between snow and
  lava; the user also observed a mostly repeated one-tile vertical line of trees
  near the center boundary. Treat these as follow-up placement/candidate-order
  issues, not as evidence that the workbook DEF classification is wrong.
- Static `rand_trn.txt` analysis strongly supports the native-score hypothesis:
  many visually thin snow rows have very weak overlap matrices, for example
  snow frozen lake/stump/shrub have `0` positive overlap entries and snow
  outcropping/rock/mound/stone have only `4`, while snow mountain has `41` and
  snow pine tree has `26`. The thin snow rows also have `-5000` overlap values
  against tree-like rows. This explains why the default tool can fill some
  tight boundary cells with trees while Thin leaves gaps after those tree DEFs
  are filtered out.
- Similar row-level patterns exist on other terrains: flat/crater/lake rows
  often have `0..5` positive overlap entries, while tree/mountain/palm/rock
  rows have much broader positive overlap. Letting "other thin objects" fill
  those native boundary gaps would require changing the score/overlap behavior
  for variant generation, not merely correcting the DEF classification table.
- A row-borrowing experiment changed allowed Thin final candidates' scoring row
  pointer before native scoring. Broad all-terrain borrowing caused regressions:
  dirt generated nothing and new terrain-border gaps appeared. The current
  narrowed experiment borrows only:
  - grass terrain `2` from row `32` (`grass oak tree`);
  - snow terrain `3` from row `61` (`snow pine tree`);
  - lava terrain `7` from row `119` (`lava mountain`);
  - wasteland terrain `11` from row `194` (`AVLrws03.def` stone, used for the
    `avlmtwl7.def` arch overlay).
  Manual testing verified snow-side, lava-side, and grass-side boundary gap
  cells can generate after the narrowed row borrowing. On 2026-06-21, the user
  confirmed an all-terrain Thin test had all tiles covered after the grass row
  borrow was added.
- Some `rand_trn.txt` object families are mixed at the DEF level. Examples:
  `(119,0)` has 40 thick and 1 thin DEF, `(140,2)` has 42 thick and 12 thin,
  `(147,0)` has 7 thick and 21 thin, and several terrain decoration families
  split thick/thin within the same ID/SubID. This is expected and is why the
  final-candidate filter must remain DEF-key based. It also means a row-level
  trace can show tree-like row names even when the final placed DEF is a
  thin-classified visual variant.
- The generator and enum now support a dormant `both` thickness value. Current
  workbook data did not use it when regenerated, so current behavior should not
  change. If trace evidence proves the native generator can only fill certain
  thin-mode boundary cells with tree-like rows, `both` can be used as the small
  fallback for selected terrain tree DEFs while still preferring a cleaner
  native-candidate fix first.

Extra object-template overlay support added on 2026-06-20:

- The workbook includes `avlglc01.def` through `avlglc06.def` as thick glacier
  mountains, object ID `140`, subtype `14`. These DEF files exist in the user's
  extracted HotA 1.8.0 assets, and the extracted HotA 1.8.0 `edobjts.txt`
  contains explicit object-template rows for them, but the installed
  `Data\objects.txt` and `Data\rand_trn.txt` do not.
- `AVLmtsn1.def` through `AVLmtsn6.def` are the existing snowy mountains. They
  use the same terrain/visit/editor-section masks as the glacier rows, but use
  object ID `134`, subtype `0`. Native `rand_trn.txt` row `57` (`snow
  mountain`) is also object ID `134`, subtype `0`.
- To avoid shipping a full custom `objects.txt` or expanding the 200-row
  `rand_trn.txt` matrix, `plugin/obstacle_filter.cpp` now:
  - redirects the editor `objects.txt` loader at `h3hota_maped.exe:0x004909C6`
    only while an active generation mode needs an overlay row;
  - writes a temporary `Data\HotaMapEdPlugin_objects_<pid>.txt` copied from
    the installed `Data\objects.txt`, then passes only that resource filename
    to the editor loader. Legacy Thick/Thin rows append as before. The 60-button
    mode replaces existing selected plugin/custom DEF rows and appends selected
    rows that are absent, updating the row count accordingly;
  - captures the native snow-mountain row pointer at
    `hota_me.dll:0x100D6DF4` while HotA assigns wrapper `+0x10`;
  - assigns that captured row pointer to glacier wrappers at
    `hota_me.dll:0x100D6E01` if HotA leaves their wrapper `+0x10` unset.
- The 60-button mode generalizes the two parser hooks: it records row pointers
  for the 12 explicit source DEFs generated from `Leveraged RandTrn` and assigns
  them to the corresponding target DEF wrappers. Rows marked `self` or
  `native bucket` do not receive plugin row assignment.
- Glacier support is intentionally Thick-only. `avlbon01.def` is supplied only
  during Thin mode and uses the same object-template masks and native
  object/scoring identity as `avlbon02.def` (`139/22`), so no special
  `rand_trn.txt` fallback is expected for it.
- `avlmtwl7.def` is supplied only during Thin mode. It uses its extracted HotA
  1.8.0 object-template row (`140/13`) and receives the native wasteland stone
  row pointer by borrowing the row assigned to `AVLrws03.def` (`140/1`,
  wasteland terrain `11`). This deliberately avoids thick mountain scoring for
  the disconnected arch-shaped footprint.
- Runtime/user verification on 2026-06-20 confirmed `avlbon01.def` and
  `avlmtwl7.def` both generate in Thin mode. The arch behavior is accepted:
  if the tile under the arch is painted along with the supports, the generator
  may place a small one-tile object under it; if only the two support tiles are
  painted, the arch can still generate cleanly without filling the center.
- Vanilla/default obstacle generation continues to load the stock `objects.txt`.
  The overlay file is deleted when variant generation finishes.
- Important runtime correction: an earlier version wrote the overlay under
  `%TEMP%` and passed the absolute path. The editor's ResourceManager treated
  that as a text resource name and raised `ResourceManager::GetText could not
  find the 'text' resource ...`, followed by a fatal runtime error. Keep the
  overlay in `Data` and pass only the filename.
- Startup smoke on 2026-06-20 verified the new hook sites install in Release:
  `objectsOverlayRedirect=004909C6`, `hotaRowCapture=100D6DF4`, and
  `hotaGlacierFallback=100D6E01`. Manual Thick generation later verified that
  glacier DEFs appear in editor output.

Runtime correction after visual Thick/Thin testing on 2026-06-20:

- The user clarified that "thick" means visually solid border objects such as
  trees and mountains; "thin" means visually passable/sparse/flat objects such
  as mushrooms, lakes, craters, and small plants.
- Manual screenshots showed the final-candidate hook was active but still used
  some wrong object families:
  - Thick still admitted a few thin objects and left a one-tile void between
    terrain regions.
  - Thick snow heavily favored dead trees and missed green snow trees/snow
    mountains.
  - Thick sand favored rocky mountains and missed sand dunes.
  - Thin still admitted dirt/swamp mountains and trees, while it received some
    sand/snow mountain families that should belong to Thick.
- Runtime logs showed a concrete key mismatch. Example:

```text
Obstacle variants: observed final candidate ... defKey=990 objectId=114 subtype=0 thickness=unknown mode=thick-only
```

  Offline `objects.txt` first-seen index `990` is not the runtime candidate's
  object family. This disproves the old offline first-seen DEF-key assumption.
- Corrected implementation direction: keep the DEF-name classification from the
  workbook, but register actual runtime DEF keys as the editor parses
  `objects.txt`.

Runtime correction after first manual Thick/Thin test:

- The user reported that both Thick and Thin buttons behaved like the default
  obstacle button.
- The log showed Thick/Thin click handlers were entered, but no candidate
  rejection logs appeared before the mode was cleared:

```text
Obstacle variants: running thick obstacle generation.
Obstacle variants: finished thick obstacle generation.
```

- First hypothesis was deferred command execution, so a timer-based mode clear
  was tested. The user reported no behavioral change, and the log still showed
  no observed candidates.
- Corrected interpretation: `0x004AF82F` is not the live candidate path for this
  workflow. Thick/thin filtering was moved to the runtime-verified
  final-candidate record path at `0x0049E592`.
- The persistent side-panel subclass and focusable `BUTTON` controls were
  removed because manual testing showed keyboard shortcuts were broken while
  that UI path was active. Non-focus `STATIC` buttons were later clickable, but
  another manual test showed keyboard shortcuts again failed while the custom
  side-panel controls were active.
- A diagnostic build with obstacle variant UI disabled restored keyboard
  shortcuts, confirming the remaining shortcut regression is in the UI layer,
  not the candidate/filter hooks.
- The working UI adds Thick/Thin as real text buttons on the existing obstacle
  toolbar, but does not subclass the editor parent or toolbar window. Earlier
  parent/toolbar subclass attempts broke editor keyboard shortcuts, and
  child-window or painted-panel attempts either broke shortcuts or introduced
  painting/focus issues.
- The verified implementation installs a UI-thread `WH_GETMESSAGE` hook,
  watches queued mouse clicks for only the obstacle toolbar, uses
  `TB_HITTEST`/`TB_GETBUTTON` to detect the Thick/Thin command slots, swallows
  those clicks, and directly runs the variant generation command. The editor's
  normal shortcut/accelerator routing remains intact. Startup smoke on
  2026-06-21 logged:

```text
Obstacle variants: installed toolbar buttons parent=... toolbar=... thicknessTable=available
```

  Manual runtime validation on 2026-06-21 confirmed:
  - the default Place Obstacles label is normal;
  - Thick/Thin buttons are visible and clickable;
  - Thick/Thin generation works;
  - keyboard shortcuts work before and after clicking the new buttons.
- The accepted eight-button prototype has been replaced by the 60 generated
  32x32 buttons described above. `Obstacle Types` and the native Brush controls
  remain fixed while only the button viewport scrolls. Workbook section breaks
  render as thin rules without category labels.
- Terrain-specific category-button enablement now hooks the native update-UI
  handler at `h3hota_maped.exe:0x00484C9A` only to capture the live obstacle UI
  context from `esi`. The hook preserves the original `mov eax, [esi+64h]` /
  `test eax, eax` sequence and resumes at `0x00484C9F`.
- Statically/runtime verified context fields used by the category UI:
  - command `32927` uses command handler `0x00484B07`;
  - update-UI handler `0x00484C8E` checks obstacle mode
    (`[esi+0x98] == 6`) and selection/tool pointer (`[esi+0x64]`);
  - selection holder is `[context+0x64]`;
  - map context is `[context+0x3C]`;
  - selection manager is `[[context+0x3C]+0x54]`.
- Painted-terrain detection uses native helper calls instead of custom map
  parsing:
  - `0x00429EBD` selects map/selection collections from the selection manager;
  - `0x0043C114` tests whether a selection-grid cell is painted for collection
    indices `0..15`; runtime testing showed one-tile obstacle brush marks can
    be reported outside collections `0`/`1`;
  - terrain must still be read from the map terrain collection selected with
    index `0`; using the painted selection collection as the tile collection
    produced zero terrain masks for some row-two one-tile marks;
  - do not trust the selection object's chunk dimensions as scan bounds. A
    one-tile brush mark below map tile row `35` could be visibly painted but
    missed when the scan height came from the selection metadata;
  - current scan dimensions come from the native terrain grid's 6x6 chunks;
    see the September 19 correction above. The former UI-context `+0x54`
    pointer-chain inference was unreliable and has been removed. Dimensions
    must still be one of `36`, `72`, `108`, `144`, `180`, `216`, or `252`;
  - the earlier fixed `252x252` scan produced out-of-range selection aliases on
    a `72x72` map. Those aliases caused painted Highlands/Wasteland cells to
    spuriously add legacy Swamp/Rough bits. Runtime testing on 2026-07-12
    confirmed that real map bounds remove both aliases while still detecting a
    one-tile Swamp mark below row `35`;
  - `0x0042B23E` returns the map terrain tile for `(x, y)`, and the terrain ID
    is `tile & 0x0F`.
- Runtime trace on 2026-07-05 verified that the active obstacle-paint layer for
  category enablement is collection `0`, cell state `2`. Earlier broad scans
  that OR'ed all collections were noisy because collections `7`/`8` can cover
  the full map, and collection `0` can contain transient non-paint states. The
  production path now scans only collection `0` and uses only state `2`; setting
  `HOTA_MAPED_PLUGIN_CATEGORY_TRACE=1` temporarily logs all `0..15` collection
  counts, terrain masks, bounds, and state masks for diagnostics.
- Category-button enablement no longer uses the native Place Obstacles toolbar
  state to decide whether the scanned terrain mask is meaningful. It derives
  "paint exists" directly from collection `0` / state `2` cell count. Before
  painting, or after generation/clear/undo leaves no active obstacle paint
  area, all obstacle type buttons remain enabled so the user can set intent
  before painting. While an active painted area exists, each button is enabled
  when its generated `Button Mapping` terrain mask intersects the painted mask.
- Category-button checked state is independent of enabled state and is stored
  outside the child windows. This preserves user customization when buttons
  temporarily gray out or the editor recreates the side panel/buttons after
  generating obstacles or switching tools.
- A plugin-owned overlay now sits over the native Place Obstacles toolbar
  button. The native button remains underneath so editor internals stay
  undisturbed, but user clicks hit the overlay. The overlay is a child of the
  native toolbar and is positioned from the native command `32927` item
  rectangle, which keeps it visible over toolbar repaints. The overlay draws
  the native toolbar image-list icon and owns its own
  `Place Obstacles [Shift+O]` tooltip, so `comctl32.lib` is an explicit plugin
  linker dependency.
- The Place overlay is enabled only when a painted area exists and at least one
  terrain-applicable category button is checked. If the effective category pool
  is empty, the overlay draws disabled and click does nothing. The UI-thread
  message hook also blocks native command `32927` in that empty-pool state, so
  `Shift+O`/accelerators cannot bypass the overlay gate.
- Runtime validation of the eight-button prototype through Computer Use on
  2026-06-23:
  - category buttons are gray before obstacle painting;
  - painting only Dirt enables only placeholder button `0`;
  - painting only Sand enables only placeholder button `1`;
  - painting only Grass enables only placeholder button `2`;
  - painting only Snow enables only placeholder button `3`;
  - painting only Swamp enables only placeholder button `4`;
  - painting only Rough enables only placeholder button `5`;
  - painting only Subterranean enables only placeholder button `6`;
  - painting only Lava enables only placeholder button `7`;
  - undo disables the buttons again;
  - `Ctrl+Z` still worked during this test.
- Runtime validation of the eight-button prototype through Computer Use on
  2026-06-25 after the selection scan fix:
  - one-tile brush marks across terrain IDs `0..7` produced raw terrain mask
    `0x000000FF` and enabled all eight placeholder category buttons;
  - undoing all marks disabled the category buttons;
  - repainting only second-row test terrains (`4..7`) produced raw terrain
    mask `0x000000F0`, leaving the top placeholder row disabled and the bottom
    placeholder row enabled.
- Manual validation on 2026-06-25 found that second-row one-tile marks failed
  below tile row `35`. A broad fixed-size scan initially fixed those marks but
  was superseded on 2026-07-12 by the authoritative runtime map bounds above.
- Manual validation on 2026-07-02 confirmed the newer prototype initial-state
  behavior: all buttons are enabled before painting; painting gates them by
  terrain; user-checked state survives generating obstacles, undo, clearing,
  and returning to the no-painted-area state.
- Category visuals are generated at runtime by `plugin/obstacle_def_images.cpp`.
  No PNGs, raw DEFs, LOD data, or other proprietary image assets are embedded
  in the plugin.
- Statically verified native resource calls for this executable profile:
  - `h3hota_maped.exe:0x004DB240` searches the editor's active archive list and
    selects a named entry;
  - `h3hota_maped.exe:0x004E4960` reads from the selected entry and maintains a
    partial-read cursor.
- Runtime verified: HotA patches the archive path so the same calls can select
  and read keyed `HotA.lod` entries. Those directory records are not plaintext,
  so the plugin deliberately does not inspect archive metadata. It reads the
  fixed DEF/group headers, discovers the first-frame offset and payload size,
  and then reads exactly through that frame using the native partial reader.
- The bounded decoder currently supports the required adventure-object DEF
  type `0x43`, compression format 3, and the first frame of the first group.
  Unsupported or malformed resources log an error and retain the square
  placeholder.
- Reserved palette handling matches the required editor behavior: index `0`
  is transparent; indices `1` and `4` are 25/50 percent black shadows; indices
  `2` and `3` become shadows only when their palette colors are the reserved
  magentas, otherwise they remain opaque palette colors. This distinction is
  required for black detail in `Sand Mountains` and `Grass Mountains 3`.
- Visible alpha bounds are cropped and aspect-fit into 24x24 icons or 160x160
  previews with area resampling. Disabled icons are generated as grayscale at
  half alpha. Preview selections of two, three, or four DEFs use deterministic
  2-cell, 2+1, or 2x2 layouts within the same 160x160 image area.
- Terrain indicators use `BITMAP` resource `141` from the loaded `hota_me.dll`.
  The installed HotA 1.8.0 resource was independently verified as a 288x24,
  32-bit strip containing twelve 24x24 cells. Cells `0..7` match terrain bits;
  cells `8`, `9`, and `10` are Highlands, Wasteland, and Water respectively,
  while cell `11` is Rock/Void. The plugin caches that native bitmap and embeds
  no terrain image assets.
- Popup terrain order is Dirt, Sand, Grass, Snow, Swamp, Rough, Subterranean,
  Lava, Water, Highlands, and Wasteland. Rock/Void bit `9` is skipped and valid
  cells are compacted left with six per row. Counts `1..6` produce a 220-pixel
  popup; counts `7..11` produce a 246-pixel popup.
- Current workbook masks cover icon counts `1,2,3,4,5,7,8,9,10`. Debug-only
  assertions exercise all counts `1..11`, the six/seven row boundary, sparse
  ordering, Rock omission, and destination bounds without changing workbook
  metadata.
- Runtime verified on 2026-07-13: one- and two-row popups render at the intended
  sizes; Snow remains visible on `#c0c0c0`; Highlands, Wasteland, and Water use
  strip cells `8`, `9`, and `10`; and a 10-terrain mask renders as `6+4`.
- Manual validation on 2026-07-05 confirmed the stabilized trace path and Place
  overlay:
  - one-tile Dirt/Sand/Grass/Snow marks produced applied terrain mask
    `0x0000000F` from collection `0` / state `2`;
  - one-tile marks across terrain IDs `0..7` reached `0x000000FF` and then
    returned to `0x00000000` after clear/undo;
  - unchecking the only applicable terrain category disabled the Place overlay
    and clicking it generated nothing;
  - re-checking that category enabled the overlay and generation worked.

### Current Obstacle Workbook Contract

As of 2026-07-13, the workbook at
`C:\Users\Andy\My Drive\Heroes Stuff\HotA lod\obstacles.xlsx` is the source of
truth for the obstacle type UI and selected-button generation filter.

Current `Obstacles` tab audit:

- 866 obstacle rows;
- 7 categories;
- 60 buttons;
- `Source` counts: 837 `hota`, 29 `plugin`;
- 5 intentional custom random masks;
- 56 `native bucket` RandTrn entries;
- duplicate `edobjts.txt` keys are expected only for the dead-tree family and
  `AVLf07g0.def`; combine those duplicate `Mask 12 B` values with bitwise OR.

Important column meanings:

- `Category`: the group heading in the obstacle panel, analogous to `Brush`.
- `Button`: the category-toggle button under that heading.
- `Source`: `hota` for native HotA objects, `plugin` for objects whose obstacle
  generation behavior must be added or altered by this plugin.
- `Leveraged RandTrn`: `self` means an exact native `rand_trn.txt` row exists;
  `native bucket` means HotA already generates that object through a native
  family/bucket even without an exact `rand_trn.txt` ID/SubID row; a DEF name
  means borrow that DEF's native `rand_trn.txt` row behavior.
- `Random Mask`: the random-obstacle/editor-section terrain mask the category
  system should use.
- `Is Custom`: true when `Random Mask` intentionally differs from
  `edobjts Random Mask`.
- `edobjts Random Mask`: the source mask from HotA 1.8.0 `edobjts.txt`, after
  duplicate rows are OR-combined where needed.

The 12 terrain bit columns are ordered:

`WST`, `HIG`, `RCK`, `WTR`, `LAV`, `SUB`, `RGH`, `SWP`, `SNW`, `GRS`, `SND`,
`DRT`.

Use `edobjts.txt`, not `objects.txt`, as the trusted source for semantic
editor-section/random-mask values. `objects.txt` is still useful for native
template/footprint data, but some of its 12-bit random masks appear stale or
wrong for editor behavior. All workbook DEFs currently exist in `edobjts.txt`,
so the old ambiguous `Leveraged Template` column was removed.

`Manual Mask` columns were also removed from the workbook. Manual placement is
not part of the current customization plan, and changing it would be riskier
because it is tied to native editor/game validation behavior.

Known plugin/borrowed RandTrn entries:

- `AVLGLC01.def` through `AVLGLC06.def` borrow `AVLmtsn1.def` through
  `AVLmtsn6.def`;
- `AVLMTWL7.def` borrows `AVLrws03.def`;
- `AVLbon01.def` borrows `AVLbon02.def`;
- the new burnt/crumbled/miscellaneous plugin rows borrow the DEFs recorded in
  the workbook (`avlxsu04.def`, `avlxsu09.def`, and `AVLskul0.def` where
  applicable).

`Button Mapping` drives category membership, button names, default selection,
the 12 terrain applicability columns, and runtime DEF visuals. `Icon Def`
selects one representative DEF for the 24x24 button. `Preview Def 1` through
`Preview Def 4` select one to four examples for the hover preview; unused cells
may be blank or `n/a`, and populated preview cells must be contiguous. An icon
and `Preview Def 1` must either both be supplied or both be unused. Every visual
DEF must also belong to that button on `Obstacles`, which the generator
validates. `Button Layout` drives visual order, four-column wrapping, and
section breaks. Rock is void space; the generator rejects any button that
enables it.

Regenerate button metadata and plugin/custom template rows with:

```powershell
python tools\generate_obstacle_buttons.py `
  --workbook 'C:\Users\Andy\My Drive\Heroes Stuff\HotA lod\obstacles.xlsx' `
  --edobjts 'C:\Users\Andy\My Drive\Heroes Stuff\HotA lod\edobjts.txt'
```

The generator validates required sheets/columns, one-to-one layout coverage,
terrain values, DEF hash collisions, DEF-to-button coverage, explicit borrow
sources, and exact ID/SubID matches for plugin/custom `edobjts.txt` rows.
The 34 committed override strings are the minimum derived template rows needed
for this feature: each comes from the matching HotA 1.8.0 `edobjts.txt` row with
only the workbook `Random Mask` substituted. The raw extracted source files are
not committed.

Existing Thick/Thin generator command after the workbook is classified:

```powershell
& 'C:\Users\Andy\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe' `
  tools\generate_obstacle_classification.py `
  --workbook 'C:\Users\Andy\My Drive\Heroes Stuff\HotA lod\obstacles.xlsx' `
  --objects 'C:\Users\Andy\My Drive\Heroes Stuff\HotA lod\objects.txt' `
  --output plugin\obstacle_classification.cpp
```

The extracted HotA source text files (`objects.txt`, `edobjts.txt`, and
`rand_trn.txt`) are currently kept beside the workbook in
`C:\Users\Andy\My Drive\Heroes Stuff\HotA lod`. They should not be copied into
`Heroes3\Data` unless intentionally testing LOD override behavior.

Current Thick/Thin generator validation:

- After adding trace-only candidate-score instrumentation, dormant `both`
  support, the Thick-only glacier overlay, and the Thin-only `avlbon01.def` /
  `avlmtwl7.def` overlay, the generator still produced 844 classified DEF-name
  hashes. It now reports 8 plugin-supplied DEFs and 0 unresolved `objects.txt`
  warnings.
- Debug Win32 and Release Win32 both built successfully on 2026-06-20 after the
  lingering map editor process was closed normally. A prior Debug build failed
  with `LNK1168` while `Debug\HotaMapEdPlugin.dll` was still pinned by an open
  editor process.
- Debug Win32 and Release Win32 builds passed after the runtime registration
  hook and focus fix were added. Debug and Release builds also passed after the
  glacier overlay/fallback hooks were added. Rebuild again after any code
  changes; close `h3hota_maped.exe` first because it pins the Debug DLL.
- On 2026-06-20, the workbook was updated so one-tile water rocks
  `avlrfx06.def` and `AVLrk2w0.def` classify as Thick instead of Thin. The
  generated classification changed exactly those two DEF hashes from `1` to
  `2`, intended to reduce intermittent Thick-mode water gaps. Debug Win32 and
  Release Win32 builds passed after regeneration, and the user later reported
  the Thick water-gap behavior looked good.
- Thin terrain-border gaps were fixed by the narrowed row-borrowing path and
  committed as `948c593 Fill thin grass boundary gaps`. The user validated an
  all-terrain Thin test where every painted tile was covered.

Follow-up notes:

- Confirm normal `Place Obstacles [Shift+O]` behavior is unchanged when variant
  mode is inactive if the old Thick/Thin command path remains exposed.
- Thin generation was manually verified to place `avlbon01.def` and
  `avlmtwl7.def`.
- Debug Win32 and Release Win32 builds passed after the `avlmtwl7.def`
  wasteland-stone fallback was added.
- Revalidate the old Thick/Thin commands only if they remain exposed after the
  category-button UI replaces them.
- During drag preview, a wrapper previously named `sub_42A069` calls the real
  placement validator previously named `sub_42616A`.
- `sub_42616A` appears to:
  - iterate an object's placement footprint;
  - check map bounds and terrain compatibility;
  - collect overlapping objects;
  - apply object-object overlap rules through multiple masks/bands.

Recovered helper interpretations:

- `sub_42C8BC`: tests the main footprint/active-cell mask.
- `sub_41E5FE`: renamed `IsMaskA`; tests another per-cell mask.
- `sub_426547`: tests another per-cell mask.
- `sub_41E4F8`: builds a per-cell rank/band map used by overlap validation.
- `sub_426597`: special-case logic involving object type 87 and nearby terrain;
  likely unrelated to general mountain overlap.

### Conceptual Overlap Model

Observed placement behavior disproved a simple "occupied tiles may never
overlap" rule.

The best recovered model is:

- the `.msk` footprint defines occupied local cells;
- the first 48-bit mask field associated with `objects.txt` helps derive a
  local contour/depth band map;
- overlap validity depends on maintaining a consistent relative front/back
  relationship across the overlap region;
- placement becomes invalid when the overlap would require contradictory depth
  ordering;
- therefore allowed overlap is not a universal count and can depend on object
  pair, relative position, and row/Y alignment.

This native validator should be reused, not reimplemented, for repeated
placement.

### Important Uncertainty About `objects.txt`

Prior analysis drifted between `objects.txt`, `objtmplt.txt`, and a guessed
runtime object-template cache. Do not assume the current scaffold identified
the correct cache or loader.

The preferred next investigation is narrower:

1. Compare vanilla and custom HotA 1.8.0 `objects.txt`.
2. Identify exactly which field controls obstacle-tool eligibility per terrain.
3. Trace where the native obstacle generator reads that eligibility value.
4. Hook only that selection decision while custom thick mode is active.
5. Leave native randomness, density, placement validation, and overlap logic
   untouched.

This is safer than swapping an entire global template cache.

## Recovered Copilot Chats

Visual Studio stores this solution's chats as MessagePack session files here:

`C:\Users\Andy\Repos\h3-hota-map-editor-plugin\.vs\HotaMapEdPlugin\copilot-chat\263b7174\sessions`

Relevant session IDs:

- `f728af56-8d2a-4764-bd2d-e2897054ccfc`
  - plugin onboarding and object-limit work
- `2aae507a-0e84-44c9-aaaa-5909a82dd1c5`
  - initial thick/repeated obstacle feature discussion
- `cb17dcdb-fa49-4c37-b44e-9d21f127dd32`
  - detailed obstacle mask, contour, banding, and overlap investigation
- `1197e384-f51d-4b72-af6e-af70de2a060d`
  - latest obstacle implementation attempt and failed handoff request

Other sessions discuss separate HotA LOD encryption/decompression work. Do not
confuse that project with this map-editor plugin.

The `.vs` directory is disposable Visual Studio state. This handoff should be
the primary context source going forward.

## Foundation Work Recommended Next

### Phase 1: Preserve And Correct Behavior

1. Keep and verify the garrison-only increase.
2. Keep the unsafe skill-booster limit increases out of active behavior.
3. Keep the old repeated-obstacle/cache-swapping experiment deleted unless its
   design is revalidated.
4. Use the smoke-test checklist for vanilla editor behavior and the garrison
   cap after significant changes.

### Phase 2: Harden The Plugin

1. Add a target-version profile keyed by executable hash/version.
2. Validate expected original bytes before every hook.
3. Refuse to patch unsupported binaries and report a useful error.
4. Flush the instruction cache after patching.
5. Return and check hook-installation errors.
6. Add a small file logger suitable for runtime diagnostics.
7. Move substantial initialization and all UI work out of `DllMain`.
8. Separate features into independently enabled modules.
9. Fix build output/PDB collisions.

### Phase 3: Thick Obstacle Mode

1. Obtain the user's custom/alternate HotA 1.8.0 `objects.txt`.
2. Parse and diff it offline against vanilla data.
3. Prove the native obstacle candidate-eligibility read in IDA/runtime.
4. Add a separate custom command using a verified unused ID.
5. Scope alternate eligibility to custom mode only.
6. Verify vanilla obstacle placement remains byte-for-byte behaviorally
   unchanged when custom mode is inactive.

### Phase 4: Repeated Placement

1. Treat repeated placement separately from thick filtering.
2. Find the native successful-placement completion and painted-selection clear
   path.
3. Preserve/rearm the intended area only after verified placement completion.
4. Continue using the native placement validator for existing-object overlap.
5. Offer combinations only after both independent behaviors are proven:
   vanilla, thick-only, repeated, and thick-plus-repeated.

## Working Principles

- Prefer direct binary analysis, IDAPython, logs, and diagnostic hooks over
  asking the user to manually inspect IDA.
- Use Computer Use for GUI-only workflows when installed and appropriate.
- Distinguish verified static facts, verified runtime facts, and hypotheses.
- Do not translate decompiled code wholesale into a replacement editor.
- Do not commit game binaries, LOD archives, raw extracted proprietary assets,
  or a full custom `objects.txt`. Small derived tables required by a plugin
  feature may be committed after their purpose and provenance are documented.
- Keep changes narrow and preserve vanilla behavior outside explicitly enabled
  features.
