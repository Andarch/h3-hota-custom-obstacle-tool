# Grass Plants generation gaps

## Cause

Runtime verified on supported HotA 1.8.1: Grass Plants includes six DEFs across
three native object identities and scoring rows:

| DEFs | Native type/subtype | Native row |
| --- | --- | --- |
| AVLsh1g0/2g0.def | 122/0 | 53 |
| AVLsh3g0/4g0/5g0.def | 115/0 | 52 |
| AVLsh6g0.def | 138/0 | 51 |

These rows forbid overlap within the group, including cross-row pairs. Native
scoring of a gap reached the overlap rejection at English DLL RVA 0xCCD45 against
another Grass Plant (weight -5000). Changing only self-overlap would leave
cross-row conflicts.

The existing saved test map supplied a read-only 72x72, two-level terrain/view/
flip layout for disposable generation. A baseline pass across all fifteen
Undergrowth buttons left 167 eligible gaps for Grass Plants and zero for every
other button. An earlier separate Grass Plants run left 154. These are randomized
counts. Three extra cell passes did not fill the gaps; no blocked footprints
were on Void.

## Fix

Recognize type 115/0, 122/0 or 138/0 with preserved native wrapper firstTerrain 2.
Use the existing parser row-store observer to capture their native row IDs for
the selected button. In private selected-object rules, promote forbidden overlap
entries only for these captured same-button rows to 30. Preserve positive weights,
original source arrays, row identities and unrelated overlap entries. The row IDs
are discovered from native wrappers rather than hardcoded into production logic.

Reset captured rows each generation run. Both terrain-option settings use this
adjustment; Standard bypasses it. Other plant terrains and tree identities are
excluded. Native drawing-order, footprint, terrain/paint and Void checks remain.
No new hooks, byte-guard changes, footprint edits or filler pass are needed.
Original game files and the saved map are unchanged.

## Verification

- Debug and Release builds and compatibility suites pass.
- Unit tests cover the three-by-three overlap matrix, preserved positive weights
  and unrelated overlap bans, source arrays and row IDs, native terrain/type/
  subtype and selection gates, same-button matching, all-terrain off/on/off,
  masks and Void, run reset and Standard bypass.
- English and Russian Release: twelve full native saved-layout Grass Plants runs
  each, zero eligible gaps and zero blocked footprints on Void. No retry placements
  were needed; native object-limit guards pass.
- Four further Release runs on Grass with Allow all terrains off likewise leave
  zero eligible gaps and no blocked footprints on Void.
- A Release pass over all fifteen Undergrowth buttons has zero eligible gaps and
  zero blocked footprints on Void for each. The user also reports Trees passing;
  this change does not target tree identities.

Ignored evidence: scratch/undergrowth_family_before.txt,
scratch/grass_plant_release_english.txt, scratch/grass_plant_release_russian.txt,
scratch/grass_plant_native_terrain_probe.txt and scratch/undergrowth_family_fixed.txt.
Raw terrain records, saved maps and game binaries are not committed.
