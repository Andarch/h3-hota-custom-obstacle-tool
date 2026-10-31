# Crater generation gaps

## Cause

Runtime verified on supported HotA 1.8.1: crater types other than Snow Craters
use native rows with self-overlap -5000, which rejects further placements needed
to fill painted gaps. The Dirt Craters diagnostic reached the overlap rejection
at English DLL RVA 0xCCD45 against another Dirt Crater (row 1, weight -5000).
This is not a drawing-order failure.

A read-only snapshot of the saved Dirt Craters example supplied the 72x72,
two-level terrain/view/flip layout. Baseline native runs left these eligible gaps:

| Button | Scoring row(s) | Original self-overlap | Baseline gaps |
| --- | --- | --- | --- |
| Dirt Craters | 1 | -5000 | 106 |
| Sand Craters | 20 and 25 | -5000 | 313 |
| Grass Craters | 33 | -5000 | 93 |
| Snow Craters | 9 (existing fallback) | 10 | 0 |
| Swamp Craters | 65 | -5000 | 127 |
| Rough Craters | 86 | -5000 | 90 |
| Lava Craters | 115 | -5000 | 90 |
| Highlands Craters | 169 | -5000 | 83 |

These are individual randomized counts. Three additional native cell passes per
gap did not add objects. All runs had zero blocked footprints on Void.

All eight crater DEFs are native type 118/subtype 0. Sand Craters additionally
includes AVLspit0.def, type 149/subtype 0, using row 25. Snow Craters has no row
in its freshly constructed wrapper; the existing missing-row policy supplies
row 9, whose positive overlap weight explains the working behavior.

## Fix

Extend the existing lake policy to these crater identities. Selected Custom
candidates with forbidden self-overlap receive private self-overlap weight 30.
Keep positive weights intact, including Snow Craters' fallback weight 10. Only
the private row's own overlap entry changes; other overlap entries, source rows,
row identities and native drawing-order/footprint/terrain/paint/Void checks remain.

Both terrain-option settings use this adjustment; Standard bypasses it. No new
hooks, expected-byte changes, footprint edits or filler pass are needed. Original
game files and the user's saved map are unchanged.

## Verification

- Debug and Release builds and compatibility suites pass. The combined rule test
  covers all crater identities (including both Sand variants) alongside all lake
  controls, off/on/off, preserved positive/source/cross-row data, row IDs, masks,
  Void, repeated candidates, selection and Standard bypass.
- English and Russian Release: three full saved-layout native runs per crater
  button (24 runs per build), zero eligible gaps and zero blocked footprints on
  Void. No retry placements were needed. Native object-limit guards pass.
- Eight additional Release runs with Allow all terrains off, each using its
  normal terrain in a disposable grid, also have zero eligible gaps and zero
  blocked footprints on Void. Snow Craters retains overlap weight 10.

Ignored evidence: scratch/crater_boundary_probe.txt,
scratch/crater_family_before.txt, scratch/crater_release_english.txt,
scratch/crater_release_russian.txt and scratch/crater_native_terrain_probe.txt.
Raw terrain records, saved maps and game binaries are not committed.
