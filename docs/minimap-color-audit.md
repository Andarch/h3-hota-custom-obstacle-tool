# Supported obstacle minimap color audit

## Result

Runtime verified on the supported English and Russian HotA 1.8.1 installations:
all 880 unique supported DEFs, covering all 121 buttons, contain blocked cells.
Their loaded definitions use 47 native color classes. 495 definitions across
18 classes use the normal terrain minimap shade instead of the blocked shade;
385 are already correct. This audit includes the earlier lava and volcano fixes.

The affected classes are 114, 115, 117, 120, 122, 127, 129, 130, 132, 136, 138,
139, 140, 147, 150, 151, 157 and 161. They include flowers, plants, mushrooms,
logs, fissures, outcroppings, reefs, rocks and other decorative obstacles.
Loaded native definitions are authoritative for these counts; the workbook's
compiled metadata establishes membership.

## Implementation and passable decorations

Use the existing guarded hook at EXE 0x4421DE. EAX contains the shared native
object definition; ESI contains the local color index. The shared definition
embeds the 76-byte template at +12, so its 48-bit passability mask is +16/+20
(zero bits denote blocked cells). The definition's type remains at +40.

This layout is statically verified by the existing IDA database: 0x43CEDB copies
76 template bytes into the shared definition and 0x4421A6 reads its type. Native
construction validates DEF key, both passability words and type against the
original template for all 892 supported records (880 unique DEFs).

In Custom mode, map affected classes with blocked cells to the native blocked
color entry (128; retain 157->158 for the existing volcano case). Preserve the
native path for fully passable definitions, other color classes, unreadable
records and Standard mode. This avoids darkening passable decorations that share
classes with supported obstacles. Ignore the unused high sixteen mask bits.

The hook saves registers/flags, passes the definition and local index to a
helper, replaces only saved ESI, restores registers and reproduces the original
comparison. It does not alter stored types, passability, generation, map data,
shared definition/cache/palette data or game files. Existing hook addresses,
expected bytes and supported hashes remain unchanged. Colors follow the actual
terrain's blocked shade, including with Allow all terrains enabled.

## Verification

- Debug and Release builds and compatibility suites pass.
- Thunk tests sweep indices 0..233 in both modes with blocked and fully passable
  definitions, checking exact corrections, unchanged other indices, preserved
  EAX and native comparison flags. Additional cases cover unreadable/null records,
  blocked bits in either mask word and ignored upper bits.
- Debug English and Release English/Russian probes construct real native shared
  definitions from all supported templates and call the native color function
  on twelve terrains in Standard/Custom/Standard order: 31,680 comparisons per
  run, zero failures. All 880 use the correct blocked shade in Custom; returning
  to Standard reproduces native colors exactly. Stored types/masks are unchanged.
- A real native definition constructed with a fully passable mask retains its
  normal shade despite using an affected class (139).
- Native object-limit guards remain intact.

The table below records the unique loaded definitions per button. A corrected
count of zero means that button already had the right native color.

| Button | Supported DEFs | Color corrected in Custom |
| --- | ---: | ---: |
| Dirt Mountains | 8 | 0 |
| Sand Mountains | 6 | 0 |
| Sand Dunes | 6 | 6 |
| Rough Mountains | 6 | 0 |
| Grass Mountains 1 | 6 | 0 |
| Grass Mountains 2 | 6 | 0 |
| Grass Mountains 3 | 6 | 0 |
| Subterranean Mountains | 6 | 0 |
| Lava Mountains | 7 | 0 |
| Dormant Volcanoes | 6 | 0 |
| Active Volcanoes | 6 | 4 |
| Highlands Mountains | 9 | 3 |
| Snow Mountains | 6 | 0 |
| Glaciers | 6 | 6 |
| Ice Formations | 14 | 14 |
| Ice Arches | 2 | 2 |
| Swamp Mountains 1 | 6 | 0 |
| Swamp Mountains 2 | 6 | 0 |
| Wasteland Mountains | 6 | 0 |
| Wasteland Arches | 1 | 1 |
| Large Reefs | 15 | 15 |
| Small Reefs | 8 | 8 |
| Dirt Hills | 18 | 0 |
| Sand Hills | 15 | 0 |
| Grass Hills | 12 | 0 |
| Snow Hills | 3 | 3 |
| Swamp Hills | 11 | 0 |
| Rough Hills | 12 | 0 |
| Subterranean Hills | 12 | 0 |
| Dirt Mounds | 2 | 0 |
| Grass Mounds | 2 | 0 |
| Snow Mounds | 4 | 0 |
| Rough Mounds | 3 | 0 |
| Dirt Lakes | 3 | 0 |
| Grass Lakes | 3 | 0 |
| Frozen Lakes | 3 | 0 |
| Swamp Lakes | 4 | 0 |
| Rough Lakes | 1 | 0 |
| Subterranean Lakes | 3 | 0 |
| Highlands Lakes | 4 | 0 |
| Limestone Lakes | 3 | 3 |
| Lava | 26 | 20 |
| Subterranean Lava | 5 | 3 |
| Tar Pits | 5 | 0 |
| Dirt Craters | 1 | 0 |
| Sand Craters | 2 | 0 |
| Grass Craters | 1 | 0 |
| Snow Craters | 1 | 0 |
| Swamp Craters | 1 | 0 |
| Rough Craters | 1 | 0 |
| Lava Craters | 1 | 0 |
| Highlands Craters | 1 | 0 |
| Dirt Fissures | 5 | 5 |
| Grass Fissures | 4 | 4 |
| Rough Fissures | 10 | 10 |
| Subterranean Fissures | 5 | 5 |
| Lava Fissures | 14 | 14 |
| Wasteland Fissures | 5 | 5 |
| Oak Trees 1 | 10 | 0 |
| Oak Trees 2 | 9 | 0 |
| Pine Trees | 10 | 0 |
| Dead Trees | 10 | 0 |
| Dirt Trees | 11 | 0 |
| Maple Trees | 13 | 13 |
| Snow Trees | 10 | 0 |
| Dead Snow Trees | 17 | 0 |
| Spruces | 14 | 13 |
| Palms | 54 | 54 |
| Swamp Trees | 6 | 0 |
| Dead Swamp Trees | 4 | 0 |
| Rugged Trees | 27 | 0 |
| Willow Trees | 3 | 0 |
| Predatory Plants | 6 | 6 |
| Carnivorous Plants | 1 | 1 |
| Cacti | 25 | 0 |
| Yucca Trees | 6 | 0 |
| Stumps | 3 | 0 |
| Snow Stumps | 2 | 0 |
| Logs | 4 | 4 |
| Dirt Plants | 9 | 9 |
| Sand Plants | 5 | 5 |
| Grass Plants | 6 | 6 |
| Snow Plants | 3 | 3 |
| Swamp Plants | 22 | 17 |
| Rough Plants | 9 | 9 |
| Highlands Plants | 3 | 3 |
| Wasteland Plants | 6 | 6 |
| Dirt Flowers | 9 | 9 |
| Grass Flowers | 12 | 12 |
| Swamp Flowers | 5 | 5 |
| Highlands Flowers | 20 | 20 |
| Grass Mushrooms | 10 | 10 |
| Swamp Mushrooms | 6 | 6 |
| Subterranean Mushrooms | 17 | 17 |
| Dirt Rocks | 5 | 5 |
| Grass Rocks | 11 | 11 |
| Snow Rocks | 8 | 8 |
| Swamp Rocks | 6 | 6 |
| Rough Rocks | 16 | 15 |
| Subterranean Rocks | 22 | 20 |
| Highlands Rocks | 9 | 9 |
| Wasteland Rocks | 3 | 3 |
| Dirt Outcroppings | 3 | 3 |
| Grass Outcroppings | 3 | 3 |
| Snow Outcroppings | 3 | 3 |
| Rough Outcroppings | 4 | 4 |
| Subterranean Outcroppings | 4 | 4 |
| Lava Outcroppings | 3 | 3 |
| Highlands Outcroppings | 3 | 3 |
| Frogs | 3 | 3 |
| Chickens | 1 | 1 |
| Roosters | 1 | 1 |
| Pigs | 1 | 1 |
| Crates | 5 | 5 |
| Barrels | 2 | 2 |
| Sacks | 2 | 2 |
| Ropes | 2 | 2 |
| Abandoned Boats | 2 | 2 |
| Burnt Structures | 3 | 3 |
| Crumbled Structures | 4 | 4 |
| Bones | 10 | 10 |

## Maintenance and evidence

Re-run the audit when workbook membership or supported editor data changes;
check new native classes against their actual blocking masks before expanding
the policy. No terrain restrictions or placement weights are involved.

Ignored evidence: scratch/minimap_supported_rows.json,
scratch/minimap_audit_counts.json, scratch/all_obstacle_minimap_probe.cpp,
scratch/all_obstacle_minimap_release_english.txt and
scratch/all_obstacle_minimap_release_russian.txt. Existing and additional IDA
exports are scratch/ida_minimap.json and scratch/minimap_record_layout*.json.
Raw game tables, native definition dumps and binaries are not committed.
