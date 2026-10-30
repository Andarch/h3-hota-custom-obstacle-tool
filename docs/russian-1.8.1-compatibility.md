# Russian HotA 1.8.1 compatibility

## Diagnosis

The user supplied copies from a Russian installation under
`C:\Users\Andy\My Drive\Heroes Stuff\Russian`. The rejected plugin log identifies
an unsupported `hota_me.dll` hash, before any hooks were installed.

Statically verified:

- `h3hota_maped.exe` is byte-for-byte identical to the supported English EXE:
  `CCF1E8424D913AA409ECC168F87FB20EDD64C3A56D412F2878065FC261BC6113`.
- Russian `hota_me.dll` SHA-256:
  `86D9F9471163F36F58AA9E42A3020C456C66D39344062F44224F2A7F7B20D72B`.
- The module is a separate build, not merely a resource-only change. Its relevant
  obstacle functions move by -0x20. Never accept its hash with the English offsets.
- The supplied `HotA_Setup.ini` has `Language=1`; the local English installation
  has `Language=0`. These are observed installer settings, not a verified runtime
  language-detection contract.

## Hook mapping

All addresses below are module RVAs. Existing EXE hooks are unchanged.

| Purpose | English | Russian |
| --- | --- | --- |
| Terrain-weight predicate | 0xCC230 | 0xCC210 |
| Terrain accumulator | 0xCC2E0 | 0xCC2C0 |
| Mountain drawing-order check | 0xCCC2F | 0xCCC0F |
| Parser entry (diagnostics) | 0xDD4B0 | 0xDD490 |
| Parser row store | 0xDDB54 | 0xDDB34 |
| Parser fallback | 0xDDB61 | 0xDDB41 |
| Terrain-view table | 0x17BAC4 | 0x17BAC4 |

The existing English IDA database exports under ignored `scratch/` supplied
function boundaries and structure evidence. Direct PE/disassembly comparison of
the supplied module verified the following:

- The predicate's complete 130-byte function differs only in two absolute
  references to its correspondingly shifted switch tables.
- Accumulator instructions through its return match except for absolute switch
  table references; the table destinations move with the function.
- The mountain-check surrounding range 0xCCB00..0xCCDD6 matches byte-for-byte
  against Russian 0xCCAE0..0xCCDB6, including the conditional branch displacement.
- The complete parser range 0xDD4B0..0xDDC4E has matching instruction lengths and
  operations. Seven absolute code-address operands move by -0x20.
- The parser fallback still restores ECX from [ESP+0x14] and EDX from [ESP+0x1C].
  Wrapper +0x10, generator templates and 201-row scoring layout remain compatible.

The new profile remains an exact-hash allowlist entry. All existing hook-byte
checks and initialization failure handling remain in force.

## Native UI discovery

The Help submenu is now identified by native MFC `ID_APP_ABOUT` (0xE140), rather
than the caption `Help`. The shared EXE supplies this command in both languages.
The tool's About command is still enabled after native popup updates.

Brush-section discovery accepts both `Brush` and the native Russian Windows-1251
bytes CA E8 F1 F2 FC (Кисть). This is discovery compatibility, not translation of
plugin controls. Plugin text remains English.

## Verification and remaining limits

Debug and Release compatibility suites pass, including both exact profiles,
unknown-hash rejection, English/Russian Help captions and native brush labels.

Runtime verified in disposable editors on a private desktop:

- Production plugin initialization with the supplied Russian module in Debug
  and Release, plus English-module startup regression.
- About menu insertion, native popup enablement, modal ownership and close.
- Native object-limit bytes remain unchanged at 0x4212A4, 0x4232D4 and 0x426AFA.
- Native-generation probes in Debug and Release preserve firstTerrain and row
  identities for 892 mapped wrappers (880 DEFs). All-terrain off/on/off cases
  cover mountains on Water, reefs on Grass, Snow Trees on Lava and underground
  Grass, dunes on Grass, mixed Dirt/Water, Void rejection and native Sand dunes.

The isolated runtime combines copied Russian EXE/DLL files with local English
game data, `HotA.dat`, and `patcher_x86.dll`; its native UI remains English.
The user confirmed that the compatibility build works in the fully Russian VM.
That confirms functional use, not exhaustive saved-map regression coverage.
The Russian files and local archives remain untracked.
No original installed game binary was replaced and no user map was edited.

Ignored evidence: `scratch/russian_generation_{debug,release}.txt`,
`scratch/russian_release_startup.txt`, and `scratch/english_compatibility_startup.txt`.

The regular Release DLL was locked by a running editor during this work. The
earlier Release test pair is in `scratch/russian_build/Release/`.
Version v0.9.1 includes this compatibility update; translations are deferred.
Its matching Release EXE/DLL pair is in `outputs/v0.9.1/` because the normal
Release DLL remains locked. Debug/Release builds, suites and v0.9.1 metadata pass.
The published v0.9 release has not been replaced.
