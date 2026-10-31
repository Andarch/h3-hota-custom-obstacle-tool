# Custom Obstacle Tool Handoff

Last updated: 2026-10-31

## Current checkpoint

- Scope: obstacle-tool enhancements inside the official HotA 1.8.1 map editor,
  delivered as a Win32 injected DLL and launcher. All native object limits apply.
- Repository: `C:\Users\Andy\Repos\h3-hota-custom-obstacle-tool`.
  Remote: `https://github.com/Andarch/h3-hota-custom-obstacle-tool.git`.
  Work on `main`; preserve the user's uncommitted changes.
- Outputs: `Debug/HotACustomObstacleTool.exe/.dll` and
  `Release/HotACustomObstacleTool.exe/.dll`. Ship matching EXE/DLL files together.
  The launcher expects the DLL beside it. INI/log filenames remain
  `HotaMapEdPlugin.ini` and `HotaMapEdPlugin.log`; the internal initialization
  export remains `HotaMapEdPluginInitialize`.
- The solution remains `HotaMapEdPlugin.sln`; project/output references are relative.
  If this chat retains the pre-rename directory, pass the current root explicitly.
- Latest cleanup removes obsolete generation modes, classifications, hard-coded
  overlay rows, scoring substitutions, toolbar commands and fixed row allowlists.
  Current workbook-selected generation, overlays, borrowing and diagnostics remain.
  The source tree and current docs describe supported behavior; earlier
  chronological investigations are recoverable from Git history.

## User interface

- Native Help lookup uses ID_APP_ABOUT (0xE140), independent of the menu language.
  Brush discovery accepts English and native Russian Windows-1251 text.
- Draft English/Russian UI localization is implemented in shared/localization.*.
  Native File/Help menu captions (Unicode or native Windows-1251 bytes) determine
  plugin language; HotA_Setup.ini [Global Settings] Language=0/1 is the startup
  fallback. Launcher messages use the selected installation setting, with Windows
  UI language only before an installation is known. Do not infer UI language from
  the module hash: a Russian DLL with English data can show English menus.
- translations/ru.json is the editable translation source: 176 strings, including
  121 obstacle names and twelve categories. Run tools/generate_translations.py to
  regenerate shared/localization_strings.inc and docs/russian-translation-review.md;
  --check validates coverage and generated files. English metadata/DEF identities
  stay unchanged. Unknown names fall back to English. Logs remain English.
- Plugin labels, owner-drawn headers/previews and all tooltips use Unicode.
  Runtime verified: the host's common-controls v5 rejects sizeof(TOOLINFOW)=48 for
  TTM_ADDTOOLW but accepts TTTOOLINFOW_V2_SIZE=44. Use V2 for these tooltip messages.
  Preview captions wrap to measured height and move terrain icons down accordingly.
  Mode radio widths account for translated text. Russian phrasing awaits tester review.
- Help > About Custom Obstacle Tool... is available in both modes, after the
  existing native About item. The owner-thread menu subclass opens a centered
  modal native dialog with v0.9.1, build configuration, author Andarch, description,
  GitHub repository link and OK. Escape closes it. The compact dialog is
  250x128 dialog units, with version and build on one row and 10-unit side margins.
  A private activation context enables the SysLink control independently of the
  editor's common-controls version.
  Runtime verified: native WM_INITMENUPOPUP disables unknown commands (state 3).
  The subclass delegates first, then enables only the tool's About command, leaving
  native command states intact. Startup/direct WM_COMMAND tests alone miss this.
- `resources/version.h` defines the shared dialog/product version and binary
  metadata. EXE and DLL have numeric file version 0.9.1.0 and product version v0.9.1.
  `resources/version.rc` is compiled into both; `plugin/about.rc` holds the dialog.

- Standard is the startup mode: native generation, enablement and minimap colors;
  all Custom controls are hidden. Custom is opt-in.
- Tooltips: Standard = "Use the editor's native obstacle placement." Custom =
  "Customize which obstacle types can be generated."
- 121 buttons / 880 DEF mappings in twelve categories, in order:
  Mountains (20), Reefs (2), Hills (7), Mounds (4), Lakes (11), Craters (8),
  Fissures (6), Trees (21), Undergrowth (15), Rocks (15), Animals (4), Miscellaneous (8).
- Buttons are 40x40 with 36x36 icons, four columns per row. Hover cards show
  160x160 previews (one to four DEFs), names and terrain icons; Rock/Void is omitted.
  Native archives and HotA bitmap resource 141 supply artwork; embed no raw assets.
- Each category starts collapsed, has a selected count and independent expansion,
  and uses bold header text when its count is nonzero. Counts include unavailable
  selections. All 121 buttons start selected, as defined by the workbook.
- Fixed controls below Mode: Allow all terrains, global Select All/None and a
  28x28 double-chevron Expand All/Collapse All button. Category controls are 18x18.
  Only categories scroll. Reserve scrollbar width so the toggle never jumps sideways.
  There is no Obstacle Types label. Global selections include collapsed/disabled buttons.
- Mode, selected buttons, expansion and terrain option survive mode/tool/panel
  changes within a session; they are not saved across editor restarts.
- Before painting, buttons remain available for setup. After painting, availability
  follows applicable terrain masks. Surface and underground paint are both scanned.
  Custom Place Obstacles and Shift+O require eligible paint and a selected applicable
  button. Non-focus click handling preserves editor accelerators.
- Allow all terrains starts unchecked and is usable regardless of paint state.
  In Custom it makes all selected obstacles eligible on all eleven non-void terrains;
  buttons/previews use mask 0xDFF. Unchecking restores workbook restrictions for
  subsequent runs. Existing objects are not edited. Standard bypasses the option.

## Workbook and generated metadata

Source of truth (read in place; do not edit without a request):
`C:\Users\Andy\My Drive\Heroes Stuff\HotA lod\obstacles_1.8.1_v2.xlsx`.

`Obstacles` supplies DEF membership, source, random masks, native identities and
borrowed RandTrn sources. `Button Mapping` supplies categories, masks, default
selections and icons/previews. `Button Layout` supplies order and row/column positions.
The 12 terrain bits are DRT=0, SND=1, GRS=2, SNW=3, SWP=4, RGH=5, SUB=6,
LAV=7, WTR=8, RCK/Void=9, HIG=10, WST=11. Button masks may not enable Void.

Regenerate with:

```powershell
& 'C:\Users\Andy\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe' `
  tools\generate_obstacle_buttons.py `
  --workbook 'C:\Users\Andy\My Drive\Heroes Stuff\HotA lod\obstacles_1.8.1_v2.xlsx' `
  --edobjts 'C:\Users\Andy\My Drive\Heroes Stuff\HotA lod\HotA_1.8.1_txt\edobjts.txt'
```

- Generator validation covers layout, masks, unique mappings, hash collisions,
  native ID/SubID matching, donor presence and icon/preview ownership. A category
  and its button may share a name. The workbook currently needs no normalization.
- 28 minimal derived template rows and 12 borrowed sources are committed. Rows
  derive from matching HotA 1.8.1 edobjts entries with requested random masks;
  full extracted tables, DEFs and LOD archives must not be committed.
- Four DWORDs support up to 128 selections; generator and compile-time guards
  enforce this capacity. Add more capacity before accepting a larger workbook.

## Generation behavior to preserve

- Custom filters the native generator by selected button. Native rows are reused
  unless a mapped object requires an override, borrowing, missing-row fallback,
  dune adjustment or the all-terrain option. Missing rows (for example Maples)
  can use captured native dirt-rock row 9. Preserve the workbook donor mappings.
- Selected custom templates use a temporary objects overlay read by the native
  loader. Original game files are not changed; the temporary file is cleaned up.
- Private scoring rows copy native row IDs, weights, adjacency and overlap arrays.
  All-terrain runs promote every allowed nonpositive terrain weight to 5, preserving
  existing positive weights. Zero/soft negative weights otherwise fail the native
  scorer's positive-terrain gate (Grass Mountains 1/2/3 on Dirt, shared row 38).
  Ordinary Custom runs retain their existing zero/soft-negative scoring behavior.
  Expanded-terrain adjacency handling is reused; Void remains excluded. Overlap
  entries are preserved except for the family-specific self-overlap fixes below.
  See docs/all-terrain-zero-weights.md for reproduction and validation.
- Terrain-transition fix: eligible selected candidates use actual footprint terrains,
  rather than extra Dirt/Sand implied solely by transition artwork. Preserve actual
  terrain masks, tile flags, footprint checks and native collision handling.
- Mountain drawing-order adjustment requires a selected/current type-134 candidate
  and a permitted native overlap weight. Existing pieces must be type 134 or a
  type-139/subtype-17 waterfall mapped to the same selected button (the Highlands
  Mountains AVLwtf00/01/02 pieces). Other families remain excluded. The latter
  avoids intermittent gaps beside Void; see docs/highlands-boundary-gaps.md.
  Selected glacier pairs (type 140/subtype 14, both mapped to the current button)
  also use this adjustment; see docs/glacier-gaps.md. Ice Arch pairs (type
  140/subtype 3) now use the same same-subtype/selected-button gate, avoiding
  internal and boundary gaps. Other type-140 families (including Wasteland Arches
  and dunes) keep native order checks. Native overlap bans and footprint/Void
  checks remain. See docs/ice-arch-gaps.md for isolated variant comparisons.
- Snow Hills (type 140/subtype 5, native row 162) also need private self-overlap
  weight 30 to avoid internal gaps. Only their own row's overlap entry changes;
  other overlap entries, source rows, footprints, Void and drawing-order checks
  remain native. This applies to selected Custom runs with either terrain-option
  setting; Standard bypasses it. See docs/snow-hill-gaps.md.
- Lakes with forbidden native self-overlap use private self-overlap weight 30:
  Dirt/Grass/Swamp/Subterranean Lakes (type 126/subtype 0), Frozen Lakes (121/0),
  Limestone Lakes (140/8) and Tar Pits (154/0). Positive native self-overlap is
  preserved, including Highlands Lakes (126/0, row 171, weight 5). Other overlap
  entries, source rows, drawing-order checks, footprints and Void remain unchanged.
  Selected Custom runs support both terrain-option settings; Standard bypasses it.
  See docs/lake-gaps.md for the eleven-family native controls and row IDs.
- Craters (type 118/subtype 0 and the Sand Craters type-149/subtype-0 variant)
  reuse the forbidden-self-overlap private-row policy. Preserve Snow Craters'
  positive overlap weight 10 from the existing fallback row 9. Other crater rows
  change only their private self-overlap entry to 30. Native cross-row bans,
  footprints, drawing-order, paint and Void checks remain; Standard bypasses it.
  See docs/crater-gaps.md for all eight controls and Sand's two scoring rows.
- Dirt/Grass/Wasteland Fissures use private self-overlap weight 30 when forbidden.
  Recognize native 139/29 or 117/0 plus preserved wrapper firstTerrain 0/2/11;
  Rough/Subterranean/Lava Fissures keep their native overlap rules. Wasteland's
  three 139/29 variants have no native row. Capture a selected same-button 117/0
  wrapper's native row during parsing and use it before the generic row-9 fallback
  only for missing-row Wasteland candidates. Explicit workbook donors and existing
  native rows take precedence; capture resets each run. This unifies the Wasteland
  group on row 186 without changing source rows or cross-row overlap arrays.
  See docs/fissure-gaps.md for baseline, controls and English/Russian validation.
- Large Sand Dunes (type 140/subtype 6) use private self-overlap weight 30 while
  retaining native drawing-order checks. Selected Sand Dunes defer the small
  AVLdun10/20/30 templates (type 148/subtype 0) to a native second pass over remaining
  painted eligible cells, independently of Sand Hills selection. With Allow all
  terrains off, this remains Sand-only. With it on, the captured run setting allows
  all eleven non-void terrains. Filler templates inherit the selected Sand Dunes
  parent's expanded masks, private scoring rows and actual-terrain edge handling.
  Native firstTerrain/row matching, overlap bans, footprints, paint scope and Void
  rejection remain in force. See docs/dune-all-terrain-gap-fill.md for reproduction.
- Custom-only minimap fix: local type/color index 157 uses 158's blocked shade.
  Only display colors change; object types, footprints and passability are untouched.
- Allow all terrains snapshots its setting per selected-generation run. After native
  firstTerrain/RandTrn matching, the existing parser fallback hook broadens selected
  generator-owned template masks at +0x14/+0x18 to 0xDFF. Every selected candidate
  receives private all-terrain weights. Do not mutate shared editor/map caches or
  choose native row donors from widened masks. Failed mask writes reject the run's
  candidates. New runs with the option off restore normal source masks/rules.

## Version-specific native facts

Statically verified supported hashes:

- `h3hota_maped.exe`: `CCF1E8424D913AA409ECC168F87FB20EDD64C3A56D412F2878065FC261BC6113`.
- English `hota_me.dll`: `365EB0C5C01ADBEAEC6A46E12D49BA2C710150270600ACE496496EB432DEA240`.
- Russian `hota_me.dll`: `86D9F9471163F36F58AA9E42A3020C456C66D39344062F44224F2A7F7B20D72B`.
  Same EXE; module hooks move by -0x20, terrain-view table remains 0x17BAC4.
  See `docs/russian-1.8.1-compatibility.md` for static comparison and runtime evidence.
  The address table below describes the English DLL; use profiles for either build.

Both are validated before hooks; original-byte guards and failure propagation
remain mandatory. Current profiles live in `plugin/target_profile.cpp`.

| Location | Meaning / expected original bytes |
| --- | --- |
| EXE 0x4909C6 | objects source loader; B8 17 9B 52 00 |
| EXE 0x49061D | template DEF registration; 8B F0 8D 4D CC |
| EXE 0x49E592 | final candidate; 8B 40 10 3B C3 |
| EXE 0x484C9A | obstacle UI context capture; 8B 46 64 85 C0 |
| EXE 0x4421DE | minimap color; 81 FE E8 00 00 00 |
| EXE 0x49E8DF | painted-cell snapshot; 55 8B EC 83 EC 14 |
| EXE 0x49E9E4 | filler before cleanup; 39 7B 20 8B 73 14 |
| DLL RVA 0xCC230 | terrain-weight predicate; 8B 41 04 56 8B 30 |
| DLL RVA 0xDDB54 | wrapper row store; 8B 44 24 30 8B 40 04 8B 04 88 89 43 10 |
| DLL RVA 0xDDB61 | wrapper fallback; 8B 4C 24 14 8B 54 24 1C |
| DLL RVA 0xCC2E0 | terrain accumulator; A1 plus relocated terrain-view-table address |
| DLL RVA 0xCCC2F | mountain order check; 0F 85 10 01 00 00 |

- Native parser entry is DLL RVA 0xDD4B0; terrain-view table RVA 0x17BAC4.
  Parser fallback reloads ECX from [esp+14h], EDX from [esp+1Ch], then resumes +8.
  Wrapper +0x10 points to a 76-byte RandTrn row; row +4 points to the weight holder.
  Native 1.8.1 has 201 rows; glacier is row 199 and lava outcropping row 200.
- Native generator constructor 0x49D666 owns its 76-byte template vector at
  +0x28/+0x2C. Wrapper +0 is template, +4 is firstTerrain, +0x10 is native row.
  Generator cells are 48 bytes; +36 is terrain and +40 flags. Native generation
  is 0x49E8DF, cell placement 0x49E452, cell eligibility 0x499F70.
- Map terrain uses 6x6 chunks: columns at grid+4, chunk pointers at +0x0C/+0x10.
  Dimensions must derive from the validated vector, not UI fields. Scan only actual
  layers with matching terrain/selection collections. Native helpers 0x429EBD,
  0x42B23E and 0x43C114 are used by the production scanner.

## Verification and release readiness

- Grass Mountains/Dirt: native tests reproduced zero placements for all three
  groups with Allow all terrains on. Promoting nonpositive weights in run-private
  scoring rows fixes all three; off/on/off still rejects Dirt with the option off
  and keeps native Grass generation. Debug/Release suites, Russian-module native
  tests and the saved-map terrain-layout reproduction pass. The dune boundary
  regression still has zero eligible gaps and zero blocked footprints on Void.

- Dune boundary fix: reproduced 2-3 gaps per run using the terrain/view/flip data
  from the user's saved 72x72 two-level obstacle_test.h3m (read-only copy). Gaps were
  on non-Sand terrains, especially Lava/Water beside Void, excluded by the old
  Sand-only filler. Patched repeated native runs have zero eligible gaps and zero
  blocked footprints on Void. Debug/Release suites, English/Russian native probes,
  and the existing all-terrain off/on/off regression pass. No new native patch
  sites or modified native footprints/overlap arrays. Normal Debug/Release rebuilt.

- Russian compatibility: Debug/Release tests and native startup/generation probes
  pass with the supplied Russian DLL and local English game data. User confirmed
  the compatibility build works in the Russian VM. This is functional acceptance,
  not exhaustive saved-map regression evidence. v0.9.1 includes compatibility;
  draft translations are now included in current working builds. Published releases
  have not been replaced. The normal Debug/Release output folders were rebuilt
  after the editor closed. Older compatibility-only test pairs remain under
  outputs/v0.9.1/ and scratch/russian_build/Release/; do not use those for translation tests.

- Localization checks: Debug/Release builds and suites cover detection overriding
  startup hints, English fallback, every translated category/button, Cyrillic
  controls and tooltips, mode-label fit, wrapped caption/icon bounds, both About
  languages and composite launcher errors. The actual Release launcher missing-DLL
  dialog was verified in Russian on English Windows using installation Language=1.
  Russian About/panel/preview layouts were visually inspected. Isolated production
  startup passes with both module profiles and English data. User confirmed the
  Release localization build works in the fully Russian VM; native-speaker wording
  review remains pending. A missing ucrtbased.dll error came from using Debug;
  verified Release EXE/DLL imports do not depend on the debug runtime.
- About addition: Debug/Release builds and compatibility suites pass, including
  native-menu preservation, duplicate-install prevention, actual modal resource,
  exact content, centering, OK and Escape. Runtime verified in an isolated 1.8.1
  editor: Help insertion, dialog opening/modal ownership/closing, continued editor
  activity and unchanged native object-limit bytes. EXE/DLL metadata checked for
  both configurations. User accepted the compact dialog and confirmed the repository
  link after resolving GitHub sign-in/visibility. Automated tests do not open a browser.

Build sequential Win32 Debug/Release using `AGENTS.md`; compatibility and generator
commands are in `tests/README.md`. Native smoke scenarios are in
`docs/phase-1-smoke-test.md`; initialization contract is in `docs/phase-2-foundation.md`.

- User accepted current obstacle categories/icons, 40x40/36x36 sizing, fixed global
  controls and all-terrain feature. Standard naming and radio tooltips are latest
  UI changes; functional builds/tests passed. Do not infer exhaustive saved-map or
  game regression from visual feedback.
- Runtime verified in disposable 1.8.1 editors: all 892 mapped native wrappers
  (880 DEFs, including duplicate rows) preserve firstTerrain/native row IDs; modified
  template pointers lie inside each generator-owned vector. Off/on/off probes cover
  mountains on Water, reefs on Grass, Snow Trees on Lava/underground Grass, dunes
  on Grass, mixed Dirt/Water, Void rejection, and native Sand dune generation.
- Cleanup verification: both Win32 builds and compatibility suites pass; native
  probe results are under ignored `scratch/all_terrains_cleanup_{debug,release}.txt`.
  Probes use disposable maps/grids; original user maps and game binaries are untouched.
- Keep logs and optional bounded diagnostics for troubleshooting. Explicit one-based
  row blocking remains `HOTA_MAPED_PLUGIN_BLOCK_OBSTACLE_IDS`; generation tracing
  remains `HOTA_MAPED_PLUGIN_OBSTACLE_VARIANT_TRACE`. Standard bypasses diagnostics.
- `README.md` is the user guide: requirements, installation/discovery/optional INI,
  features, usage, troubleshooting/log location, updates and removal.
  Release EXE/DLL imports were inspected: both depend on MSVCP140/VCRUNTIME140.
  The guide links Microsoft's latest v14 x86 installer, required even on x64 Windows.
  Include the guide and root `LICENSE` in the release ZIP. User selected MIT;
  copyright holder is Andarch, year 2026. README links the license and distinguishes
  the project license from third-party game assets.
- Before public distribution, verify x86 VC++ runtime packaging on a fresh PC, Unicode/long paths,
  temporary Data-directory write access and installation discovery on other PCs.
  These are pending release tasks, not changes included in cleanup.
- Test map: `C:\Users\Andy\Repos\obstacle_test.h3m` (do not overwrite).
  Existing IDA 9.3 databases are under `C:\Users\Andy\Games\3DO\Heroes3`.
  Ignored scratch probes may need absolute-path updates after the repository rename.
  Leave unrelated `tools/__pycache__/` files out of commits.
