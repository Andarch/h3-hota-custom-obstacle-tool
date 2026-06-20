# Codex Project Handoff

Last updated: 2026-06-20

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
  The workbook data used here contains only DEF filename, object name, object
  ID, and object subtype. Treat its terrain-placement and editor-section source
  values, if recovered later, as untrusted until compared against the live HotA
  editor data.
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
- the unfinished repeated-obstacle/cache-swapping scaffold;
- the current object-limit module, which still enables unsafe booster limits.

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

- Workbook DEFs absent from `objects.txt`: `avlbon01.def`,
  `avlglc01.def`, `avlglc02.def`, `avlglc03.def`, `avlglc04.def`,
  `avlglc05.def`, `avlglc06.def`, and `avlmtwl7.def`.
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
  - 8 workbook DEFs absent from HotA 1.8.0 `objects.txt`:
    `avlbon01.def`, `avlglc01.def` through `avlglc06.def`, and
    `avlmtwl7.def`.
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
- Detailed final-candidate accepted/rejected logs are now gated by
  `HOTA_MAPED_PLUGIN_OBSTACLE_VARIANT_TRACE=1`; keep this off for ordinary
  manual testing because repeated clicks can produce large logs.
- Remaining observed visual artifacts from a zoomed-out all-terrain test where
  every tile was painted: Thick filled terrain well overall, with water mostly
  filled but showing a few gaps. Thin filled terrain generally but left one-tile
  boundary gaps around/between some terrains and a two-tile gap between snow and
  lava; the user also observed a mostly repeated one-tile vertical line of trees
  near the center boundary. Treat these as follow-up placement/candidate-order
  issues, not as evidence that the workbook DEF classification is wrong.

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
- The persistent side-panel subclass and focusable `BUTTON` controls were also
  removed because manual testing showed keyboard shortcuts were broken while
  that UI path was active.
- A follow-up manual test reported the non-focus static controls were not
  clickable; adding `SS_NOTIFY` fixed the likely Win32 hit-test/notification
  issue while preserving the non-focus behavior.
- Debug and Release Win32 builds passed after this correction. A startup-only
  smoke test also passed, but the final-candidate filtering and keyboard
  shortcuts still need a manual map-editor click test.

Generator command after the workbook is classified:

```powershell
& 'C:\Users\Andy\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe' `
  tools\generate_obstacle_classification.py `
  --workbook 'C:\Users\Andy\My Drive\Heroes Stuff\HotA lod\obstacles.xlsx' `
  --objects 'C:\Users\Andy\Games\3DO\Heroes3\Data\objects.txt' `
  --output plugin\obstacle_classification.cpp
```

Current generator validation:

- The generator succeeds against the classified workbook and HotA 1.8.0
  `objects.txt`, producing 844 classified DEF-name hashes.
- Debug Win32 and Release Win32 builds passed after the runtime registration
  hook and focus fix were added. Rebuild again after any code changes; close
  `h3hota_maped.exe` first because it pins the Debug DLL.

Still requiring runtime validation:

- Confirm normal `Place Obstacles [Shift+O]` behavior is unchanged when variant
  mode is inactive.
- Investigate the Thin terrain-boundary gaps, especially the two-tile snow/lava
  gap, and the repeated center tree line. Decide whether this needs a code
  change, a workbook classification adjustment, or acceptance as native
  generator behavior under a filtered candidate set.
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
