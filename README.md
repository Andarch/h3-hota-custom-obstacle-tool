# HotA Custom Obstacle Tool

Customizable obstacle generation for the HotA Map Editor.

**Version:** v0.9 · **Author:** Andarch

Requires HotA 1.8.1.

The tool adds a Custom mode to the editor's obstacle brush so you can choose which
obstacles it generates. Standard mode keeps the editor's native obstacle placement.

## Requirements

- Windows 10 or 11 with the Microsoft runtime listed below.
- An existing Heroes of Might and Magic III installation with the **official
  Horn of the Abyss 1.8.1 map editor** (`h3hota_maped.exe` and `hota_me.dll`).
  The plugin checks both binaries and rejects unsupported versions or modified builds.
- The latest **Microsoft Visual C++ v14 Redistributable (x86)**:
  [download the x86 installer from Microsoft](https://aka.ms/vc14/vc_redist.x86.exe).
  Install or update it before starting the tool. **The x86 package is required
  even on 64-bit Windows; the x64 package alone is insufficient.**

Both the launcher and plugin are 32-bit and use the Microsoft C++ runtime.
Microsoft requires the runtime to be at least as recent as the build tools used
for the release. See [Microsoft's runtime requirements and downloads](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist/).
You do not need Visual Studio, Excel, or the development workbook to use the tool.

## Download and installation

1. Open [Releases](https://github.com/Andarch/h3-hota-custom-obstacle-tool/releases)
   and download the tool's compiled release ZIP. GitHub's automatic **Source code**
   downloads contain the project sources, not the ready-to-run tool.
2. Extract the ZIP before launching. Keep these matching files together:
   - `HotACustomObstacleTool.exe`
   - `HotACustomObstacleTool.dll`
3. Place them in your HotA installation folder, beside `h3hota_maped.exe`, or keep
   them together in a separate writable folder.
4. Run **`HotACustomObstacleTool.exe`**. It starts the map editor with the plugin.
   It looks beside the launcher first, then checks a saved path, registry entries,
   and common installation locations. If it cannot locate HotA, select the folder
   containing `h3hota_maped.exe` when prompted.
5. Open the obstacle brush and select **Custom** to access the added controls.
   The editor starts in **Standard** mode each session.

The tool loads into the editor it starts; it does not attach to an editor you
already have open. Launching `h3hota_maped.exe` directly starts the normal editor.

If needed, set the installation path manually in `HotaMapEdPlugin.ini` beside the
launcher. Use the installation folder, not the path to the EXE:

```ini
[Settings]
HotaPath=C:\Games\Heroes3
```

The configuration filename retains its original name for compatibility. A manual
INI is optional; selecting a folder saves the path when the launcher folder is writable.
The editor's `Data` folder must also be writable for Custom generation's temporary
files. The tool does not overwrite original game executables, DLLs, or data tables.

## Features

- **Standard and Custom modes**, with tooltips explaining each mode. Standard
  preserves native placement; Custom enables obstacle selection and its placement
  adjustments.
- **121 selectable obstacle buttons** covering 880 obstacle DEFs, organized into
  Mountains, Reefs, Hills, Mounds, Lakes, Craters, Fissures, Trees, Undergrowth,
  Rocks, Animals, and Miscellaneous.
- **Compact artwork buttons and hover previews** showing obstacle names, larger
  artwork, and applicable terrains. After painting, buttons unavailable for the
  painted terrain are greyed out. Painting on either map level is considered.
- **Collapsible categories**, selected counts, per-category Select All/None,
  global Select All/None, and Expand All/Collapse All. The global controls stay
  visible while the categories scroll. All obstacle buttons start selected and
  all categories start collapsed.
- **Allow all terrains**, an optional Custom setting that permits selected
  obstacles on every terrain except Rock/Void. It starts unchecked and can be
  changed before or after painting. Availability and hover previews reflect the
  option. It affects subsequent generation, not objects already placed.
- **Selections and category expansion remembered within the session**, including
  when switching modes or leaving and reopening the obstacle tool.
- **Custom placement adjustments** for actual terrain at transitions, mountain
  overlap/drawing order, and sand dunes. Selected Sand Dunes can fill remaining
  eligible painted Sand cells with small dunes. Native collision, passability,
  and object-limit checks remain in effect.
- **A Custom-mode minimap color correction for volcano obstacles** and a separate
  **Help → About Custom Obstacle Tool...** dialog with version, author, and project link.

## Using Custom mode

1. Select **Custom** in the obstacle brush.
2. Expand categories and choose the obstacle buttons you want generated. Use the
   global minus button to clear everything if you want to start with a small selection.
3. Optionally check **Allow all terrains**.
4. Paint the area to fill, then use **Place Obstacles** or **Shift+O**.

You can set up selections before painting. Generation needs eligible painted
terrain and at least one selected obstacle applicable to it. Changing selections,
mode, or the terrain option does not remove or replace existing objects.

## Limitations

- Support is limited to the validated official HotA 1.8.1 editor binaries.
  Other editor versions and compatibility with other editor plugins are unverified.
- This is an editor tool. Recipients do not need this plugin simply to open a
  resulting map, but they still need a game version compatible with the map's
  HotA format and objects.
- Mode, obstacle selections, expanded categories, and the all-terrain option are
  not saved across editor restarts. The launcher can remember the installation path.
- Very long paths and paths containing characters outside the system's Windows
  code page are not verified. If loading fails, try a shorter path with basic
  Latin characters for both the tool and the game.
- Fresh-PC installation and protected installation folders still need broader
  testing. A writable tool folder and writable editor `Data` folder are required.

## Troubleshooting

| Problem | What to check |
| --- | --- |
| Missing `MSVCP140.dll` or `VCRUNTIME140.dll`, or the launcher cannot start | Install or repair the latest Microsoft Visual C++ v14 **x86** Redistributable linked above. |
| Plugin DLL not found | Extract the ZIP and keep the matching EXE and DLL in the same folder. |
| HotA cannot be found | Select the folder containing `h3hota_maped.exe`, or set `HotaPath` in the INI. |
| Unsupported editor or DLL version | Use the official HotA 1.8.1 editor and its matching `hota_me.dll`. Modified binaries may fail validation. |
| Custom controls are missing | Start the editor through `HotACustomObstacleTool.exe`, open the obstacle brush, and select Custom. |
| Place Obstacles is unavailable | Paint an eligible area and select at least one applicable obstacle button. Check the painted terrain and the Allow all terrains setting. |
| Custom generation fails | Check write access to the editor's `Data` folder and inspect the plugin log. |

The plugin writes **`HotaMapEdPlugin.log`** beside the DLL and replaces it at the
next startup. When [reporting an issue](https://github.com/Andarch/h3-hota-custom-obstacle-tool/issues),
include the tool version, HotA version, steps to reproduce, and the log from the
affected session, if available.

## Updating or removing the tool

Close the editor before replacing the EXE and DLL with a matching newer pair.
To stop using the plugin, start the editor directly. To remove the tool, close
the editor and delete its EXE, DLL, and any tool-specific INI or log you no longer need.

## Development

The source builds as Win32 using `HotaMapEdPlugin.sln`. Build and project guidance
is in [AGENTS.md](AGENTS.md); verification commands are in [tests/README.md](tests/README.md).
Release packages use the matching EXE/DLL from the `Release` directory.

## License

[MIT License](LICENSE). Copyright (c) 2026 Andarch.

Heroes III and Horn of the Abyss game assets remain the property of their
respective owners and are not licensed under this project's MIT license.
