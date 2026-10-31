# Grass Mountains on Dirt: zero terrain weights

## Cause

Runtime verified on the supported HotA 1.8.1 builds: all three Grass Mountains
button groups use native RandTrn row 38, with Dirt weight 0 and Grass weight 5.
With Allow all terrains enabled, their generator-owned terrain masks correctly
include Dirt. The old private-row code promoted forbidden weights (<= -4000)
to 5, but retained zero and soft-negative weights.

The existing English IDA analysis of the native scorer at DLL RVA 0xCC3F0 shows
that it requires positive terrain contributions (the v34/v35 gates). On pure
Dirt, a zero weight fails that gate regardless of the expanded template mask.
Native probes reproduced zero Dirt objects for all three groups with the option
on, while native Grass placement still worked.

## Fix

Pass the captured run's all-terrain flag explicitly into private-rule creation.
For an all-terrain run, promote allowed weights <= 0 to 5, and count these cases
as terrain expansion for the existing adjacency policy. Preserve positive weights.

Ordinary Custom scoring retains its previous treatment of zero/soft negatives.
Void remains -5000 regardless of the source weight or requested mask. Native
row IDs, template firstTerrain matching, overlap arrays, footprints and collision
checks are unchanged. Only generation-private scoring rows are modified; source
rows and shared editor/map caches are not changed. Standard still bypasses Custom
rule creation. There are no new native hooks or expected-byte changes.

This fixes the shared all-terrain weight policy rather than adding a special
case for the names of these three mountain groups.

## Verification

- Debug and Release builds and compatibility suites pass.
- Unit cases cover off/on/off, zero and soft-negative weights, preserved positive
  weights, conditional adjacency expansion, original source/overlap arrays and
  row identities, masks and Void rejection with a positive source Void weight.
- English Debug native tests placed 59, 60 and 62 objects on a painted 16x16 Dirt
  area with the respective mountain groups; all three placed zero with the option
  off before/after that run. Grass controls continued to place objects in every mode.
- Release Russian-module native tests likewise pass Dirt and Grass off/on/off.
  Template matching checks still cover all 892 mapped native wrappers.
- A read-only copy of the user's updated `obstacle_test.h3m` supplied the actual
  72x72, two-level terrain/view/flip layout. Separate Release runs for the three
  groups placed 82, 87 and 92 Dirt-anchored mountains, respectively, with zero
  remaining eligible gaps in this layout and zero blocked footprints on Void.
- The earlier dune-boundary probe still passes twelve runs with zero eligible
  gaps and zero blocked footprints on Void. Native object-limit bytes remain intact.

Ignored probe evidence: `scratch/grass_mountain_before.txt`,
`scratch/grass_mountain_debug.txt`, `scratch/grass_mountain_russian.txt`,
`scratch/grass_saved_layout_probe.txt` and `scratch/grass_fix_dune_regression.txt`.
Saved maps, raw terrain records and game binaries are not committed.
