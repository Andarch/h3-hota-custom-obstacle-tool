# Lava minimap colors

## Cause and affected objects

Statically verified using the existing editor IDA analysis: EXE function 0x4421A6
reads an object's type into local ESI, then uses a bitset to choose the underlying
terrain's normal or blocked minimap shade. The guarded hook at 0x4421DE already
mapped local color index 157 to 158 in Custom mode for the volcano correction.

Native color probes show index 127 uses the normal shade while 128 uses the
blocked shade. On Lava terrain these are 0x2529 and 0x14A5, respectively. Both
indices occur within the workbook's Lava and Subterranean Lava buttons:

| Button | Wrong index 127 | Already-correct index 128 |
| --- | --- | --- |
| Lava | 20 DEFs | 6 DEFs |
| Subterranean Lava | 3 DEFs | 2 DEFs |

The affected Lava DEFs are AVLlav20/30/40/50/60/70/90.def and
AVLlv100/120/130/160/170/180/190/200/220/230/240/250/260.def. The affected
Subterranean Lava DEFs are AVLlv1u0/2u0/3u0.def. Counts and membership were
cross-checked against the existing generated workbook mappings and native
HotA 1.8.1 edobjts identities; no workbook edits or extra DEF mappings are needed.

## Current fix and verification

The initial lava fix maps local index 127 to the blocked index 128 in Custom
mode, alongside the original volcano correction. The subsequent full minimap
audit extends this policy to other affected obstacle classes and checks the
actual definition's blocking mask to preserve fully passable decorations.
See [the full audit](minimap-color-audit.md) for the current implementation,
coverage and native validation.

Stored object types, map data, passability, generation and shared palettes remain
unchanged. Standard retains native colors. The existing hook and expected bytes
are unchanged; original game files are not modified.

Initial lava-only evidence remains under ignored scratch/lava_minimap_probe.cpp,
scratch/lava_minimap_release_english.txt and scratch/lava_minimap_release_russian.txt.
The full native audit now supersedes that narrower color-index comparison.
