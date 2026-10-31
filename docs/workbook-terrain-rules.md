# Workbook terrain rules in Custom mode

## Cause (runtime verified)

The generator retained native DEF button assignments but discarded their workbook
Random Masks unless an overlay row was needed. Ordinary Custom generation often
reused native RandTrn rows, whose zero/negative weights rejected some allowed
terrains and whose positive weights could admit unwanted terrains. A button mask
alone cannot express differences between DEFs within the same button.

## Current behavior

- Retain and validate all 880 per-DEF workbook masks. Each must be nonempty,
  exclude Rock/Void and fit within its button mask.
- With Allow all terrains off, use each DEF's workbook mask intersected with its
  button mask. With the option on, use DFF (every terrain except Rock/Void).
- Apply the effective mask to generator-owned allowed/random fields after native
  firstTerrain and RandTrn matching. Do not change shared editor/map templates.
- Give selected main-pass candidates private rows: preserve positive allowed
  weights, promote other allowed weights to 5 and forbid other terrains at -5000.
  Preserve native row IDs, donor selection and existing adjacency expansion policy.
- Preserve footprints, collision checks and existing scoped overlap adjustments.
  Standard bypasses these changes. Sand Dunes and Rough Lakes retain their
  scoped secondary filler passes.
- Regenerate from the updated workbook: Frogs return to Swamp only and Abandoned
  Boats to Sand only. Their five obsolete overlays are removed, leaving 23.

The normal button terrains for the reported groups are:

| Button | Terrains |
| --- | --- |
| Oak Trees 1; Pine Trees | Dirt, Grass |
| Oak Trees 2; Willow Trees | Grass, Swamp |
| Dirt Trees | Dirt, Grass, Swamp |
| Palms | Sand, Grass, Swamp |
| Stumps | Dirt, Grass, Rough |
| Logs | Dirt, Grass, Swamp, Rough |
| Dirt Plants | Dirt, Rough |
| Swamp Plants; Swamp Mushrooms; Frogs | Swamp |
| Dirt Flowers | Dirt |
| Grass Flowers | Grass, Highlands |
| Grass Rocks | Grass |
| Pigs | All except Water and Rock/Void |
| Abandoned Boats | Sand |

These are button unions. Individual DEFs retain their narrower masks: for example,
Sand palms remain Sand-only and only the Highlands-enabled flower variant can
appear on Highlands.

## Validation (runtime verified)

- Debug and Release solution builds and compatibility suites pass. Unit coverage
  includes per-DEF differences, off/on/off masks, positive-weight promotion,
  firstTerrain preservation, original source rows, Standard bypass and Void rejection.
- Python generator tests reject malformed, empty, Void-enabled and conflicting
  duplicate masks, and retain native masks without requiring overlay rows.
- Native English Release checked all 121 buttons separately on a 72x72 grid with
  twelve terrain blocks. Every allowed button terrain was represented; no blocked
  cell appeared on a forbidden terrain, including when checked against the actual
  generated DEF's mask. All 121 passed.
- Seventeen reported/reset groups passed native Release on both English and
  Russian modules with the option off and on. All-terrain runs covered all eleven
  non-Void terrains for every group.
- Mixed one-tile terrain strips reproduced Dirt Flowers spilling onto Grass with
  the previous implementation. The corrected implementation produced no forbidden
  blocked cells for Dirt Flowers, Swamp Plants, Grass Flowers, Swamp Mushrooms or
  Grass Rocks in those controls.
- Sand Dunes: twelve native boundary runs left zero eligible gaps and zero blocked
  footprints on Void. Rough Lakes: eighteen off/on/off runs across both levels
  left zero eligible gaps and zero blocked footprints outside the painted scope
  or on Void. Native object-limit bytes remain intact.

Ignored evidence: `scratch/all_workbook_result_*.txt`,
`scratch/workbook_terrain_release_*.txt`, `scratch/mixed_workbook_terrain_before.txt`,
`scratch/mixed_workbook_terrain_probe.txt`, `scratch/dune_boundary_probe.txt` and
`scratch/rough_lake_fill_probe.txt`. Workbook, maps, raw terrain data and game
binaries are not committed or modified.
