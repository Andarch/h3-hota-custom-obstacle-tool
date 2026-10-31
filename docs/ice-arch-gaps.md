# Ice Arches generation gaps

## Cause

Runtime verified on supported HotA 1.8.1: the two Ice Arch DEFs AVLifar0.def and
AVLifar1.def use native type 140/subtype 3. Their native drawing-order checks can
reject otherwise permitted placements against existing Ice Arches. The existing
order adjustment covered mountains and glaciers but excluded Ice Arches.

A read-only snapshot of the saved obstacle_test.h3m supplied the 72x72 two-level
terrain/view/flip layout. Two baseline mixed-variant native runs left 278 and 281
eligible gaps, with no blocked footprints on Void. Three extra native cell passes
per gap did not place more objects. Native scorer breakpoints reached the order
rejection at English DLL RVA 0xCCD45; the conflicting piece was another Ice Arch.
The wrapper's private scoring row 56 allowed self-overlap (weight 30).

Separate baseline controls explain why the variant count alone does not predict
the problem: AVLifar0 alone left zero gaps, AVLifar1 alone left 2,347, and the
single Wasteland Arch (AVLMTWL7.def, type 140/subtype 13) left zero. These are
individual randomized runs, not universal counts. The failure occurs even with
only the second Ice Arch variant available.

## Fix

Extend the existing guarded mountain/glacier drawing-order adjustment to Ice
Arches. Candidate and existing piece must both be type 140/subtype 3 and map to
the current selected button. Glacier handling keeps its own same-subtype gate;
cross-subtype pairs remain excluded. The native overlap-weight veto is preserved.
Wasteland Arches and other type-140 families retain native order checks.

Standard bypasses this adjustment. No new hooks, changed expected bytes,
footprint/mask changes or extra filler pass are needed. Native Void rejection and
collision checks remain, and the saved map and original game files are unchanged.

## Verification

- Debug and Release builds and compatibility suites pass, including all four
  Ice Arch variant pairings and rejected types/subtypes/groups, forbidden overlap,
  unselected candidates and Standard bypass. Existing glacier/mountain tests pass.
- English Debug: twelve full native saved-layout runs with zero eligible gaps
  and zero blocked footprints on Void, without retry placements.
- English and Russian Release: twelve full native saved-layout runs each with
  zero eligible gaps and zero blocked footprints on Void. Native object-limit
  guards pass.
- Release separate-variant controls now have zero gaps for each Ice Arch variant;
  mixed Ice Arches and the Wasteland Arch control also have zero gaps. All four
  runs have zero blocked footprints on Void.
- Four Snow-only Release runs with Allow all terrains off cover mixed/separate
  Ice Arch variants and likewise leave zero gaps and no blocked footprints on Void.

Ignored evidence: scratch/ice_arch_before.txt, scratch/ice_arch_release_english.txt,
scratch/ice_arch_release_russian.txt, scratch/ice_arch_variants_fixed.txt and
scratch/ice_arch_snow_probe.txt. Raw terrain records, saved maps and game binaries
are not committed.
