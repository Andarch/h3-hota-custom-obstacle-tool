# Lake generation gaps

## Cause

Runtime verified on supported HotA 1.8.1: the seven reported lake groups have
native self-overlap weight -5000. Their larger shapes leave painted gaps because
the scorer forbids further same-row overlap. Repeating ordinary cell generation
does not fill these gaps. The four working lake groups already permit self-overlap.
"Snow Lakes" corresponds to the existing Frozen Lakes button; no names were changed.

A read-only copy of the saved Dirt Lakes example supplied the 72x72 two-level
terrain/view/flip layout. Direct scoring of Dirt Lakes reached the native overlap
rejection at English DLL RVA 0xCCD45 against an existing row-4 lake, weight -5000.
The drawing-order exception is not involved.

| Button | Native type/subtype | Scoring row | Original self-overlap | Baseline gaps |
| --- | --- | --- | --- | --- |
| Dirt Lakes | 126/0 | 4 | -5000 | 119 |
| Grass Lakes | 126/0 | 36 | -5000 | 442 |
| Frozen Lakes | 121/0 | 55 | -5000 | 75 |
| Swamp Lakes | 126/0 | 67 | -5000 | 280 |
| Rough Lakes | 177/0 | 9 | 10 | 0 |
| Subterranean Lakes | 126/0 | 103 | -5000 | 241 |
| Highlands Lakes | 126/0 | 171 | 5 | 0 |
| Limestone Lakes | 140/8 | 188 | -5000 | 297 |
| Lava | 127/0 | 118 | 30 | 0 |
| Subterranean Lava | 128/0 | 104 | 30 | 0 |
| Tar Pits | 154/0 | 189 | -5000 | 193 |

Baseline counts are one randomized run per button, not fixed totals. An earlier
separate Dirt Lakes diagnostic run left 103 gaps. All runs had zero blocked
footprints on Void.

## Fix

Recognize the native lake identities and inspect their own scoring row's
self-overlap weight. Create private selected-object rules when this entry is
forbidden, even with Allow all terrains off. Change only that private forbidden
self-overlap entry to 30, matching the dune/Snow Hill policy. Preserve positive
self-overlap weights, including Highlands Lakes, which shares type 126/0 but
uses a different row. Preserve all cross-row overlap entries and original rows.

Native drawing-order, footprint, terrain/paint and Void checks remain. Standard
bypasses the change. There is no new hook, changed expected byte sequence,
footprint edit or filler pass. Original game files and the saved map are unchanged.

## Verification

- Debug and Release builds and compatibility suites pass.
- Unit cases cover seven affected families and four working controls, all-terrain
  off/on/off, preserved positive self-overlap/cross-row/source data and row IDs,
  terrain masks, Void, repeated candidates, selection and Standard bypass.
- English Debug: one full native saved-layout run for each of the eleven buttons,
  zero eligible gaps and zero blocked footprints on Void.
- English and Russian Release: three full saved-layout runs per button (33 runs
  per build), zero eligible gaps and zero blocked footprints on Void. No retry
  placements were needed; native object-limit guards pass.
- Eleven additional Release runs with Allow all terrains off, each on its normal
  terrain in a disposable grid, also have zero eligible gaps and zero blocked
  footprints on Void. Working families retain self-overlap weights 10/5/30/30.

Ignored evidence: scratch/lake_boundary_probe.txt, scratch/lake_family_before.txt,
scratch/lake_release_english.txt, scratch/lake_release_russian.txt and
scratch/lake_native_terrain_probe.txt. Raw terrain records, saved maps and game
binaries remain outside version control.
