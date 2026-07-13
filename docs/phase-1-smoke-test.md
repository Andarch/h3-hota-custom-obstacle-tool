# Phase 1 Smoke Test

Use this checklist after building the Win32 Debug or Release solution. Run the
editor once directly for a vanilla baseline, then run it through the plugin
launcher and compare the same actions.

## Automated Preconditions

- [x] `h3hota_maped.exe` has SHA-256
  `CCF1E8424D913AA409ECC168F87FB20EDD64C3A56D412F2878065FC261BC6113`.
- [x] The expected original bytes still exist at all three garrison hook sites:
  - `0x004212A4`: `3B 48 08 72 28`
  - `0x004232D4`: `3B 46 08 0F 83 A8 01 00 00`
  - `0x00426AFA`: `3B 48 08 72 28`
- [x] Debug Win32 and Release Win32 solution builds both succeed sequentially.

Automated preconditions last verified: 2026-06-15.

## Vanilla Behavior

- [x] The editor starts normally and keyboard shortcuts still work.
- [x] New, open, save, save-as, map validation, and editor close behave as in
  the direct-launch baseline.
- [x] Vanilla `Shift+O` obstacle placement behaves as in the baseline.
- [x] No `Place Obstacle (Repeated)` command appears and obstacle placement
  does not automatically rearm.
- [x] Arena, Marletto Tower, Garden of Revelation, Library of Enlightenment,
  School of Magic, Mercenary Camp, Star Axis, Learning Stone, School of War,
  and Colosseum of the Magi retain their native per-map limits.

## Garrison Limit

- [x] The 49th garrison can be placed.
- [x] A map with more than 48 garrisons can be saved, reopened, and validated
  in the official HotA game.
- [x] The 255th garrison can be placed.
- [x] A 256th garrison is rejected.
- [ ] Removing a garrison after reaching the cap allows another to be placed.

Runtime behavior last verified by the user: 2026-06-15.

## Current Evidence

- Statically verified: the local target executable matches the supported hash
  and the three hook sites match the expected bytes.
- Statically verified: active custom object-limit branches now match only the
  packed garrison identifier `0x00000021`.
- Statically verified: the repeated-obstacle experiment is not present in the
  active source tree.
- Runtime verified: every checkbox under Vanilla Behavior and the checked
  items under Garrison Limit.

The create and place hooks retain the previously successful write of `0xFF` to
`[type_info+8]` when the garrison count reaches 255. Its necessity has not been
independently proven; changing it is deferred until runtime behavior and all
consumers of that field can be checked.
