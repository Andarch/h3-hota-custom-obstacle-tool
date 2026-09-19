# Compatibility Tests

These x86 tests exercise the real internal hook implementations without loading
or modifying the editor. They include `obstacle_filter.cpp` in the test translation
unit so production code needs no test-only exports or runtime overrides.

Run from the repository root. Builds are sequential and outputs stay under the
ignored `scratch/tests` directory:

```powershell
$msbuild = 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe'
& $msbuild tests\compatibility_tests.vcxproj /m:1 /p:Configuration=Debug /p:Platform=Win32 /v:minimal
.\scratch\tests\Debug\compatibility_tests.exe
& $msbuild tests\compatibility_tests.vcxproj /m:1 /p:Configuration=Release /p:Platform=Win32 /v:minimal
.\scratch\tests\Release\compatibility_tests.exe
```

Coverage includes profile rejection, expected-byte guards, RandTrn row insertion
and bounds, terrain lookup, explicit block IDs, parser-thunk stack offsets and
flag preservation, and the fastcall predicate's accept/reject paths.
Terrain geometry tests cover all seven map sizes and reject malformed chunk
vectors, unsupported dimensions, and overflow-sized values. This calculation
is shared with the production painted-terrain scanner.

Selected-object rule tests cover private weight/adjacency copies, preserved
overlap bans, land versus water/void masks, sparse masks, malformed native rows,
unselected candidate rejection, row reuse, and vanilla-mode bypass.

These tests do not replace live hook-byte validation or editor testing. The
version-specific native parser/startup smoke results and remaining manual checks
are recorded in `CODEX_HANDOFF.md`.
