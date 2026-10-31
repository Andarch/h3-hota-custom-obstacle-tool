# Highlands Mountains gaps beside Void

## Cause

Runtime verified on HotA 1.8.1: the Highlands Mountains button includes native
mountains (type 134, RandTrn row 168) and waterfall pieces AVLwtf00/01/02.def
(type 139/subtype 17, row 177). These mappings come from the workbook.

A read-only copy of the saved obstacle_test.h3m provided the 72x72, two-level
terrain/view/flip layout for disposable native generation. Twelve baseline runs
left thirteen eligible gaps, all along the Void boundary. Retrying native cell
generation three times per gap did not add any objects.

Direct native scoring and runtime breakpoints traced the hard rejection to the
existing drawing-order check at English DLL RVA 0xCCC2F. The colliding existing
piece was a mapped Highlands waterfall. The private row permitted overlap with
row 177 (weight 30), and the proposed blocked footprints stayed off Void. The
previous mountain-order adjustment accepted only type-134 pairs, so it excluded
these waterfall constituents.

## Fix

Reuse the existing guarded mountain-order hook. The candidate must still be the
current selected type-134 mountain. An existing piece can additionally be type
139/subtype 17 only when both templates map to the current selected button.
Native overlap weights must still permit the pair. Other object families,
subtypes and button groups remain excluded. Standard bypasses this adjustment.

This changes neither terrain masks nor collision/footprint validation, and adds
no hook sites or byte-guard changes. Original game files and the saved map are
unchanged. No new filler pass is needed.

## Verification

- Debug and Release builds and compatibility suites pass.
- Unit tests cover all three waterfall DEF mappings, wrong subtype/group,
  forbidden overlap, unselected candidates, Standard bypass and existing checks
  for mountain pairs and unrelated object types.
- English Debug: twelve full native runs, zero eligible gaps and zero blocked
  footprints on Void after the fix.
- English and Russian Release: twenty-four full native runs each on the saved
  terrain layout, zero eligible gaps and zero blocked footprints on Void. These
  runs needed no retry placements. The native object-limit guards remain intact.
- Grass Mountains 1/2/3 saved-layout and twelve-run sand-dune boundary regressions
  retain zero eligible gaps and zero blocked footprints on Void.

Ignored evidence: scratch/highlands_release_english.txt,
scratch/highlands_release_russian.txt, scratch/highlands_boundary_probe.cpp,
and the existing Grass Mountains and dune native probes. Game binaries, maps
and extracted terrain records remain outside version control.
