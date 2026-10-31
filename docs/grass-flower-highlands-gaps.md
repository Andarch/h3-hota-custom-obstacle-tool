# Grass Flowers gaps on Highlands

## Cause (runtime verified)

Only AVLf07g0.def in Grass Flowers supports Highlands in the workbook. The
native generator matches it as type 120/subtype 0, firstTerrain 2, RandTrn row 35.
Its footprint is 2x1 and row 35 forbids self-overlap (-5000). Other Grass Flowers
variants include 1x1 pieces but are Grass-only, so Highlands cannot use them.

Two native 72x72 Grass/Highlands controls with Allow all terrains off left 146
and 148 eligible Highlands gaps, while Grass controls had none. An independent
scorer trace reached English DLL RVA 0xCCD45 against an existing copy of the
same Highlands-capable flower, row 35, weight -5000. This is a self-overlap veto,
not a missing terrain or zero-weight problem.

## Fix

Extend the existing private forbidden-self-overlap adjustment to type 120/0,
native firstTerrain 2, whose per-DEF workbook mask supports Highlands. Set only
a forbidden private self-overlap entry to 30; preserve existing permitted values.
Other flower variants and cross-row overlap entries remain unchanged. This
allows the Highlands flower to fill gaps using its own footprint, without adding
Grass-only flowers to Highlands or introducing a filler pass.

Native source rows, row IDs, firstTerrain, footprints, paint boundaries, terrain
restrictions, collision checks and Standard mode are preserved. There are no new
hooks, byte-guard changes or changes to game files.

## Verification

- Debug and Release builds and compatibility suites pass. Tests cover the
  Highlands variant, Grass-only and Dirt flower controls, preserved permitted
  overlap values, off/on/off, selection, Standard bypass, cross-row/source
  preservation, wrong type/subtype and all twelve firstTerrain values.
- English and Russian Release: twelve native runs each, alternating Grass and
  Highlands through off/on/off twice, have zero eligible gaps and zero invalid
  blocked cells (Void, outside the painted area, or forbidden DEF terrain).
  Native object-limit guards pass.

Ignored evidence: `scratch/grass_flower_before.txt`,
`scratch/grass_flower_trace.txt`, `scratch/grass_flower_fixed_english.txt`,
`scratch/grass_flower_fixed_russian.txt` and their startup logs. Probes use
synthetic disposable maps; the user's saved map is unchanged.
