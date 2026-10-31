# Sand-dune gap filling with Allow all terrains

## Reproduction and cause

The user reported 2-3 empty tiles next to Void after generating only Sand Dunes
with Allow all terrains enabled. They supplied `C:\Users\Andy\Repos\obstacle_test.h3m`
and closed the editor. All inspection used a read-only copy under ignored scratch;
the source map was not edited.

The map is 72x72 with two levels. Its surface contains twelve 18x24 terrain
blocks; underground is Void. In particular, Lava borders the top edge of the
surface Void block and Water borders its left edge. A disposable native-generator
probe using the saved terrain/view/flip data reproduced gaps on those boundaries,
including (56,47,0) and (63,47,0). Some runs also left a gap inside Water.

Runtime verified before the fix: twelve all-terrain runs left 33 eligible cells
in total, 2-3 per run. Small-dune placement was absent from these non-Sand gaps.

The filler predated Allow all terrains and still captured/revisited only Sand.
Simply widening that loop would not suffice: the small dunes belong to the
separate Sand Hills button, so they also needed parent-approved template masks,
scoring weights and actual-terrain edge handling when Sand Hills is unchecked.

## Change

- Small dunes remain deferred until after the selected large-dune pass.
- Sand Dunes selection authorizes its three native filler templates independently
  of Sand Hills selection. Other unselected obstacle types remain excluded.
- At template parsing, an all-terrain run broadens only generator-owned requested
  filler templates after native row matching. Original firstTerrain and row IDs
  are preserved; no shared map/editor template cache is changed.
- During filling, the existing private all-terrain scoring policy applies to the
  fillers. Their native overlap arrays and footprints are preserved.
- The paint snapshot and each subsequent attempt use the active run's captured
  terrain option: Sand-only when off, all eleven non-void terrains when on.
- The approved filler can use actual-terrain edge handling through its selected
  Sand Dunes parent. Actual Void remains forbidden, including in native blocked
  footprint checks. Painting scope, existing blocked tiles and native eligibility
  are rechecked before each attempt.
- Standard mode and the other obstacle families are unchanged. This does not
  introduce a general fallback for arbitrary obstacle families or narrow shapes.

No hook addresses or expected original bytes changed.

## Verification

- Debug and Release solution builds and compatibility suites pass.
- Unit cases cover off/on/off, captured paint scope, original blocked cells,
  native eligibility, repeat-call guarding, Void, unselected-parent exclusion,
  Sand Hills independence, private weights, unchanged source/overlap arrays,
  parser identity preservation, filler-only edge authorization and Standard bypass.
- Repeated native runs of the saved terrain layout after the change leave zero
  eligible gaps. Debug and Release English-module probes and the Release Russian-
  module probe each cover twelve runs. Auditing each placed object's native blocked
  footprint found zero cells on Void. Native object-limit bytes remain unchanged.
- The existing all-terrain native probe still passes all 892 mapped wrappers and
  its terrain/underground/Void/Sand-dune off/on/off scenarios.

Ignored evidence is under `scratch/dune_boundary_before.txt`,
`scratch/dune_boundary_{debug,release,russian}.txt` and associated startup logs.
The snapshot and extracted terrain records are not committed.
