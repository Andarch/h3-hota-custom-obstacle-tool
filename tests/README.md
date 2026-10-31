# Compatibility Tests

These x86 tests exercise the real internal hook implementations without loading
or modifying the editor. They include `obstacle_filter.cpp` in the test translation
unit so production code needs no test-only exports or runtime overrides.

Run from the repository root. Builds are sequential and outputs stay under the
ignored `scratch/tests` directory:

```powershell
$msbuild = 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe'
& $msbuild tests\compatibility_tests.vcxproj /m:1 /p:Configuration=Debug /p:Platform=Win32 /v:minimal
.\scratch\tests\Debug\compatibility_tests.exe
& $msbuild tests\compatibility_tests.vcxproj /m:1 /p:Configuration=Release /p:Platform=Win32 /v:minimal
.\scratch\tests\Release\compatibility_tests.exe
```

Coverage includes profile rejection, expected-byte guards, native RandTrn row
bounds, explicit diagnostic block IDs, parser-thunk stack offsets and
flag preservation, and the fastcall predicate's accept/reject paths.
Terrain geometry tests cover all seven map sizes and reject malformed chunk
vectors, unsupported dimensions, and overflow-sized values. This calculation
is shared with the production painted-terrain scanner.

`obstacle_selection_tests.cpp` exercises the production UI scanner using native-layout
map headers and tile/selection adapters. Cases cover underground-only paint,
both layers painted at matching coordinates with different terrains, surface-only
paint, cleared/non-obstacle selections, unchecked categories, inactive tools,
and single-layer maps that must never query underground.

Native-window section tests cover category header clicks, independent expansion,
checked counts (including terrain-disabled buttons), regular/bold font weights,
hidden controls, preserved generation selections and focus, cleared hover state,
panel recreation, and scrollbar clamping when sections collapse. They use
disposable windows and do not load a game or change maps.
Global Select All/None cases verify larger control dimensions, all 121 selections
including collapsed/terrain-disabled buttons, header counts, preserved expansion,
and focus-preserving clicks.

The expansion toggle tests cover stable alignment with scrollbar space reserved,
unchanged horizontal position as the scrollbar appears/disappears, Expand All/Collapse All
tooltips, individual header changes, mixed states, obstacle visibility, unchanged
selections and focus, and scrollbar clamping after collapsing every section.
The three global controls have a separate fixed host above the category viewport.
Cases verify unchanged positions while scrolled, all three actions at the bottom,
wheel forwarding, host cleanup on recreation and Default/Custom visibility.

Mode tests cover Standard startup, radio captions/tooltips, tooltip cleanup and
recreation, radio check states and clicks, hidden custom
controls/native toolbar visibility, remembered mode and selections, preserved
focus, and unchanged queued Default placement commands. Filter tests verify that
Default bypasses diagnostic exclusions and custom generation/overlay requests.
Minimap thunk tests cover both modes for indices 0..233, preserving EAX and
native comparison flags while substituting index 157 only in Custom mode.

Selected-object rule tests cover private weight/adjacency copies, preserved
overlap bans, land versus water/void masks, sparse masks, malformed native rows,
unselected candidate rejection, row reuse, and vanilla-mode bypass.

These tests do not replace live hook-byte validation or editor testing. The
version-specific native parser/startup smoke results and remaining manual checks
are recorded in `CODEX_HANDOFF.md`.

Selection capacity tests pack each of the 121 UI buttons independently and check
candidate acceptance/rejection against all 121 selections, including indices
31/32, 63/64 and 95/96, invalid indices, and inactive-filter bypass. The workbook
generator and plugin guard the four-DWORD capacity (128 buttons).

Run generator layout regressions with:

```powershell
python tests\test_generate_obstacle_buttons.py
```

These cover a section and button sharing the name Rocks, while preserving
duplicate category/button rejection.

Allow-all-terrain cases exercise checkbox clicks without taking focus, availability
independent of painting, all button applicability masks (including Water and Void),
mode/panel persistence, and Default bypass. Generation tests cover selected-only
template changes after native row matching, private terrain weights, unchanged
native overlap arrays, per-run option snapshots, failure rejection and restoration
when unchecked. Preview sizing and terrain icons use the same effective-mask helper.
Isolated native-generation probe evidence and visual acceptance limits are recorded
in `CODEX_HANDOFF.md`.

`about_tests.cpp` exercises the production About code and resource in disposable
native windows: Help menu insertion, preserved native About entry, repeated
installation, preservation of native menu updates/disabled states while the tool's
entry stays enabled after WM_INITMENUPOPUP, exact version/author/description,
centered modal ownership, OK and keyboard Escape. It activates its own common-controls v6 context, so the
SysLink works without relying on a host manifest. Tests do not open the browser.

Russian 1.8.1 regression cases verify the separate module profile and shifted
hook RVAs, unchanged table/stack layout, native Russian brush-label discovery,
and Help discovery by the native About command with either English or Russian
captions. Runtime evidence and limits are in
[Russian compatibility notes](../docs/russian-1.8.1-compatibility.md).

Localization tests use both English and Russian menus, including native ANSI
Windows-1251 captions, and verify that actual menus override installation hints.
They cover missing/invalid INI values, English fallback with stable text pointers,
all 121 obstacle names and twelve categories, Cyrillic radios/checkboxes/tooltips,
radio text fit, wrapped caption/terrain-icon bounds, localized About content,
and composite launcher errors. No Windows locale is changed by these tests.

Check translation coverage and generated artifacts with:

```powershell
python tools/generate_translations.py --check
```

Edit Russian values in `translations/ru.json`, then run the same command without
`--check` to regenerate the compiled table and tester review list.

All-terrain dune regressions cover Sand-only/all-terrain/Sand-only run snapshots,
independence from Sand Hills selection, original painted scope, blocked and
native-ineligible tiles, Void rejection, generator-owned filler-mask expansion,
preserved native firstTerrain/row IDs, private scoring weights, unchanged overlap
arrays, parent-approved terrain-edge handling only during the filler pass, and
Standard-mode bypass. Native saved-terrain reproduction evidence is recorded in
[the dune gap-fill notes](../docs/dune-all-terrain-gap-fill.md).

Nonpositive all-terrain scoring regressions cover native zero and soft-negative
weights, preserved positive weights, unchanged ordinary Custom behavior, expanded
adjacency handling, preserved source/overlap arrays and row IDs, explicit terrain
masks, and Void rejection even with a positive native Void weight. Native Grass
Mountains 1/2/3 off/on/off and saved-terrain evidence is in
[the zero-weight notes](../docs/all-terrain-zero-weights.md).

Highlands drawing-order regressions cover all three mapped waterfall pieces,
subtype/group mismatches, forbidden overlap, unselected candidates and Standard
bypass, alongside existing mountain-pair and unrelated-family checks. Native
saved-terrain evidence is in [the Highlands notes](../docs/highlands-boundary-gaps.md).

Glacier order tests cover all 36 pairings of the six mapped DEFs, wrong types,
subtypes and groups, missing mappings, forbidden overlap, row bounds, stale
candidate context, selection and Standard bypass. See [glacier gap notes](../docs/glacier-gaps.md)
for native all-terrain and Snow-only validation.

Ice Arch order tests cover all four variant pairings, rejection of cross-subtype
pairs (including glaciers), unchanged Wasteland Arch behavior, wrong types/groups,
forbidden overlap, selection and Standard bypass. Native separate/mixed variant,
Wasteland control, English/Russian and Snow-only results are recorded in
[the Ice Arch notes](../docs/ice-arch-gaps.md).
