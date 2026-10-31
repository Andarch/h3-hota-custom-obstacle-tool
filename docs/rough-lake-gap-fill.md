# Rough Lakes corner gap filling

## Behavior

When Rough Lakes is selected in Custom mode, place lakes first and use Rough
Rocks only for remaining painted eligible cells. Rough Rocks need not be checked.
If both buttons are checked, rocks are still deferred until this filler pass.
With only Rough Rocks selected, normal selected generation is unchanged.

With Allow all terrains off, filling is Rough-only. With it on, the captured run
setting allows all eleven non-Void terrains. Standard bypasses this feature.
Filler identity comes from the Rough Rocks workbook mapping; the workbook and
button selections are not modified.

## Implementation

The existing EXE capture hook at 0x49E8DF and pre-cleanup hook at 0x49E9E4 now
dispatch separate dune and Rough Lake passes through shared bounded helpers.
Each family has its own paint snapshot, generator identity, terrain policy and
active flag. A pass consumes its own snapshot and cannot reuse the other pass's
paint or admit the other family's filler candidates.

Capture only originally painted, unprotected eligible terrain cells. Before each
native cell-generation call, recheck terrain, protected/blocked flags and native
cell eligibility. The native candidate loop still requires the blocked footprint
to cover that target cell and retains placement/overlap/paint restrictions.

All-terrain expansion recognizes parent-requested Rough Rock templates even when
their own button is unchecked. Only generator-owned template masks expand; private
scoring rows follow the existing terrain policy. Preserve native firstTerrain,
row identity, overlap arrays, footprints and source data. Parent-approved terrain
edge handling applies only during the appropriate filler pass. Original game files
and user maps are unchanged. No new hook sites or expected-byte changes are needed.

## Verification

Runtime verified on supported HotA 1.8.1 using a disposable painted 20x30 rectangle
on both map levels, with surrounding cells protected from placement. Baseline
Rough-Lakes-only runs leave the two upper corners per level empty: four gaps per
run, 24 over six runs, and no blocked footprints outside the painted rectangle.

After the fix:

- Debug and Release builds and compatibility suites pass.
- Unit cases cover off/on/off, unchecked/checked Rough Rocks, first-pass deferral,
  standalone Rocks, parent absence, paint/blocked/eligibility/Void guards, private
  all-terrain scoring, preserved source and overlap data, native row/firstTerrain,
  terrain edges, one-use snapshots, independent dune/lake phases and Standard.
- English and Russian Release: eighteen native runs each, alternating Rough/off,
  Lava/on and Rough/off with Rocks checked. Half include a painted Void column
  along the rectangle boundary. All leave zero eligible gaps; four rocks per run
  fill the corners. No blocked footprints extend onto Void or outside paint.
- Native object-limit guards remain intact on both builds.
- Twelve-run dune boundary regression retains zero eligible gaps and no blocked
  footprints on Void.
- A native mixed Sand/Rough rectangle with Sand Dunes and Rough Lakes selected
  and both filler buttons unchecked has zero gaps and no blocked footprints
  outside paint. Both filler stages coexist without consuming each other's scope.

Ignored evidence: scratch/rough_lake_fill_before.txt,
scratch/rough_lake_release_english.txt, scratch/rough_lake_release_russian.txt,
scratch/rough_lake_both_fillers.txt and scratch/dune_boundary_probe.txt. Test grids,
logs and game binaries are not committed.
