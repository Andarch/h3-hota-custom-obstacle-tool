# Glacier generation gaps

## Cause

Runtime verified on supported HotA 1.8.1: Glaciers (AVLGLC01 through AVLGLC06.def,
native type 140/subtype 14) can leave gaps inside painted terrain as well as beside
Void. A read-only copy of the user's saved obstacle_test.h3m supplied the 72x72,
two-level terrain/view/flip layout. Two baseline native runs left 20 and 23 eligible
gaps; repeating cell generation three times did not fill them.

The native scorer's drawing-order conflict check rejected placements against
existing glaciers even when their overlap weight permitted them (30). Runtime
breakpoints reached English DLL RVA 0xCCD45 from the order check at 0xCCC2F.
The existing mountain adjustment excluded glaciers because their native type is
140 rather than 134. In this native probe their wrappers used scoring row 56;
do not infer the live wrapper row solely from the named row in RandTrn.

## Fix

Reuse the existing guarded order hook for the current selected glacier candidate.
Both pieces must be type 140/subtype 14 and map to the current selected button.
Keep the native overlap-weight veto. Other type-140 families, including dunes,
retain their native drawing-order checks. Existing mountain/Highlands handling
remains in place, and Standard bypasses the adjustment.

No additional filler pass, terrain-mask changes, footprint changes, new hooks or
byte-guard changes are needed. Original game files and the user's map are unchanged.

## Verification

- Debug and Release builds and compatibility suites pass.
- Unit tests cover all 36 pairings of the six glacier DEFs; wrong types/subtypes,
  wrong groups, missing mappings, forbidden overlap, invalid row IDs, stale
  candidate context, unselected buttons and Standard bypass remain rejected.
- English Debug: twelve native saved-layout runs with zero eligible gaps and
  zero blocked footprints on Void.
- English and Russian Release: twenty-four native saved-layout runs each with
  zero eligible gaps and zero blocked footprints on Void. No retry placements
  were needed; native object-limit guards passed.
- Four additional Release runs with the non-Void terrain changed to Snow in the
  disposable grid and Allow all terrains off likewise have zero gaps and no
  blocked footprints on Void.
- Highlands and sand-dune native regression probes retain zero eligible gaps
  and zero blocked footprints on Void.

Ignored evidence: scratch/glacier_before.txt, scratch/glacier_boundary_probe.cpp,
scratch/glacier_release_english.txt, scratch/glacier_release_russian.txt and
scratch/glacier_snow_probe.txt. Raw terrain records, saved maps and game binaries
are not committed.
