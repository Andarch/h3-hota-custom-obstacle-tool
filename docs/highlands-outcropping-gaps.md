# Highlands Outcroppings generation gaps

## Cause

Runtime verified on supported HotA 1.8.1: AVLHiTr0/1/2.def are native type
136/subtype 0, using row 170 with self-overlap -5000. The overlap veto prevents
further placements needed to cover painted gaps. Direct native scoring reached
English DLL RVA 0xCCD45 against another row-170 outcropping, weight -5000.

A read-only copy of the saved example supplied the 72x72, two-level terrain/view/
flip layout. A separate diagnostic run left 104 eligible gaps; a baseline pass
across all fifteen Rocks buttons left 201 for Highlands Outcroppings and zero for
each other button. These are randomized counts. Three retry passes did not add
objects. All runs had zero blocked footprints on Void.

## Fix

Extend the private forbidden-self-overlap policy specifically to type 136/0 with
preserved native wrapper firstTerrain 10 (Highlands). Change only forbidden
private self-overlap to 30. Other outcropping terrains and Rocks types are excluded.

Preserve native source rows, row IDs, cross-row overlap entries, drawing-order,
footprint, paint, terrain and Void checks. Both terrain-option settings work;
Standard bypasses the adjustment. There is no new hook, changed byte guard,
footprint edit or filler pass. Game files and the user's saved map are unchanged.

## Verification

- Debug and Release builds and compatibility suites pass, including prior Grass
  Plants, fissure, crater and lake regressions.
- Unit tests cover all three Highlands DEFs, all six other outcropping terrains,
  the twelve-value firstTerrain scope with widened masks, wrong type/subtype,
  off/on/off, source/cross-row preservation, row IDs, Void, selection and Standard.
- English and Russian Release: twelve full native saved-layout runs each, zero
  eligible gaps and zero blocked footprints on Void, without retry placements.
  Native object-limit guards pass.
- Four further Release runs on Highlands with Allow all terrains off also have
  zero eligible gaps and zero blocked footprints on Void.
- A Release pass across all fifteen Rocks buttons has zero eligible gaps and zero
  blocked footprints on Void for every button.

Ignored evidence: scratch/highland_outcrop_before.txt,
scratch/rock_family_before.txt, scratch/highland_outcrop_release_english.txt,
scratch/highland_outcrop_release_russian.txt,
scratch/highland_outcrop_native_terrain_probe.txt and scratch/rock_family_fixed.txt.
Raw terrain records, saved maps and game binaries are not committed.
