# Fissure generation gaps

## Cause

Runtime verified on supported HotA 1.8.1 using the terrain/view/flip layout from
a read-only copy of the user's saved Dirt Fissures example (72x72, two levels).
Dirt and Grass Fissures use type 139/subtype 29, native rows 17 and 50. Both rows
forbid self-overlap (-5000), preventing placements needed to cover remaining
painted cells. Direct Dirt Fissure scoring reached English DLL RVA 0xCCD45 with
weight -5000 against another Dirt Fissure. Three retry passes did not fill gaps.

| Button | Representative scoring row | Baseline gaps |
| --- | --- | --- |
| Dirt Fissures | 17 | 234 |
| Grass Fissures | 50 | 222 |
| Rough Fissures | 99 | 0 |
| Subterranean Fissures | 113 | 0 |
| Lava Fissures | 124 | 0 |
| Wasteland Fissures | 186 plus generic fallback 9 | 34 |

These are individual randomized runs. All have zero blocked footprints on Void.
The three working groups also have overlap bans but include smaller variants
that cover the gaps, so their overlap behavior is preserved.

Wasteland Fissures mixes two native types. AVLwcr04/05.def (117/0) use row 186,
with self-overlap -5000. AVLwcr01/02/03.def (139/29) have no row, so the existing
missing-row policy used generic dirt-rock row 9. Merely allowing row-186
self-overlap still left 5-7 gaps in separate diagnostic runs: the generic row-9
variants reject overlap with row-186 pieces. Direct scoring verified this veto.

## Fix

Apply the private forbidden-self-overlap policy only to 139/29 or 117/0 templates
whose preserved native wrapper firstTerrain is Dirt (0), Grass (2) or Wasteland
(11). Change their forbidden private self-overlap entry to 30. Preserve Rough,
Subterranean and Lava Fissures' existing overlap rules.

For Wasteland's missing-row pieces, observe the existing native parser row-store
hook and capture a selected type-117/subtype-0 wrapper with firstTerrain 11. Use
its native row as the private-rule source for missing-row candidates mapped to
the same selected button, before the generic fallback. Keep explicit workbook
donors and existing native rows ahead of this fallback. Capture is reset for
every generation run. No row ID or pointer is hardcoded for this donor.

The Wasteland group consequently uses native row 186 in the supported build,
with private self-overlap 30. Source rows and cross-row overlap arrays are not
edited. Preserve original firstTerrain, native drawing-order/footprint/paint/
terrain/Void checks and Standard bypass. There are no new hook sites, changed
byte guards, footprint edits or filler passes. Game files and the saved map
are unchanged.

## Verification

- Debug and Release builds and compatibility suites pass.
- Restricted-overlap unit cases cover affected/working fissure groups alongside
  previous lake/crater controls, all-terrain off/on/off, masks and Void, row IDs,
  preserved source/cross-row data, selection and Standard bypass.
- Wasteland fallback tests cover all three missing-row DEFs, donor type/terrain/
  selection checks, same-button matching, private row 186, preserved firstTerrain,
  unchanged source/cross-row weights, run reset and Standard bypass.
- English Debug: all six saved-layout controls have zero eligible gaps and zero
  blocked footprints on Void after both fixes.
- English and Russian Release: three full native saved-layout runs per button
  (18 per build), zero eligible gaps and zero blocked footprints on Void. No
  retry placements were needed. Native object-limit guards pass.
- Six further Release runs, each on its normal terrain with Allow all terrains
  off, likewise have zero eligible gaps and zero blocked footprints on Void.
  Working families retain their original self-overlap bans.

Ignored evidence: scratch/fissure_boundary_probe.txt,
scratch/fissure_family_before.txt, scratch/wasteland_fissure_probe.txt,
scratch/fissure_release_english.txt, scratch/fissure_release_russian.txt and
scratch/fissure_native_terrain_probe.txt. Raw terrain records, saved maps and
game binaries are not committed.
