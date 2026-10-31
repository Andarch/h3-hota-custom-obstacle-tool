# Snow Hills generation gaps

## Cause

Runtime verified on HotA 1.8.1: the three Snow Hill DEFs avlxsn01/02/03.def are
native type 140/subtype 5 and use RandTrn row 162. The native row's self-overlap
weight is -5000, a hard ban. It prevents close placements needed to cover remaining
painted cells between the hills' larger footprints. This differs from the recent
glacier/Ice Arch drawing-order failures.

A read-only copy of the user's saved obstacle_test.h3m supplied the 72x72,
two-level terrain/view/flip layout. Two baseline native runs left 664 and 578
eligible gaps, respectively. Three native retry passes did not place more objects.
Direct scoring reached the overlap rejection at English DLL RVA 0xCCD45 with
weight -5000 and an existing Snow Hill from row 162.

## Fix

Use the existing selected-object private-rule mechanism for Snow Hills even when
Allow all terrains is off. Change only the private row's self-overlap entry to
30, matching the existing sand-dune adjustment. Native source rows and all other
overlap entries are unchanged. Terrain weights continue to follow the selected
terrain-option policy, and native row IDs remain intact.

Keep Snow Hills' native drawing-order checks, footprints, terrain/paint checks,
collision handling and Void rejection. Standard bypasses private rule creation.
No new hooks, changed byte guards, template footprint edits or filler pass are
needed. The user's map and original game files are unchanged.

## Verification

- Debug and Release builds and compatibility suites pass.
- Unit tests cover all three DEF mappings with all-terrain off/on/off, private
  self-overlap 30, preserved cross-family overlap/source/adjacency arrays and row
  identity, normal Snow-only weights, all-terrain weights, Void exclusion,
  unselected candidates, wrong type/subtype and Standard bypass.
- English Debug: twelve full native saved-layout runs with zero eligible gaps
  and zero blocked footprints on Void, without retry placements.
- English and Russian Release: twelve full native saved-layout runs each with
  zero eligible gaps and zero blocked footprints on Void. Native object-limit
  guards pass.
- Four additional Snow-only Release runs with Allow all terrains off also leave
  zero eligible gaps and no blocked footprints on Void.
- Separate Release controls for all seven hill buttons (Dirt, Sand, Grass, Snow,
  Swamp, Rough and Subterranean) have zero eligible gaps and zero blocked footprints
  on Void on the saved terrain layout.

Ignored evidence: scratch/snow_hill_before.txt, scratch/snow_hill_release_english.txt,
scratch/snow_hill_release_russian.txt, scratch/snow_hill_snow_probe.txt and
scratch/hill_controls_probe.txt. Raw terrain records, maps and game binaries
are not committed.
