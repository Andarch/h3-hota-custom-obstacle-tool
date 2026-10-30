#!/usr/bin/env python3
"""Generate obstacle button, DEF, overlay, and RandTrn-borrow metadata."""

from __future__ import annotations

import argparse
import re
import textwrap
import zipfile
from dataclasses import dataclass
from pathlib import Path
from xml.etree import ElementTree


XLSX_NS = {
    "a": "http://schemas.openxmlformats.org/spreadsheetml/2006/main",
    "r": "http://schemas.openxmlformats.org/officeDocument/2006/relationships",
    "p": "http://schemas.openxmlformats.org/package/2006/relationships",
}

TERRAIN_BITS = {
    "DRT": 0,
    "SND": 1,
    "GRS": 2,
    "SNW": 3,
    "SWP": 4,
    "RGH": 5,
    "SUB": 6,
    "LAV": 7,
    "WTR": 8,
    "RCK": 9,
    "HIG": 10,
    "WST": 11,
}


@dataclass(frozen=True)
class ButtonDefinition:
    name: str
    terrain_mask: int
    default_selected: bool
    category_index: int
    row: int
    column: int
    icon_def: str | None
    preview_defs: tuple[str, ...]


@dataclass(frozen=True)
class DefMetadata:
    def_name: str
    def_hash: int
    button_index: int
    borrow_source_hash: int | None


@dataclass(frozen=True)
class TemplateOverride:
    def_name: str
    def_hash: int
    button_index: int
    row: str


def column_index(cell_ref: str) -> int:
    match = re.match(r"([A-Z]+)", cell_ref.upper())
    if not match:
        raise ValueError(f"invalid cell reference: {cell_ref}")

    result = 0
    for char in match.group(1):
        result = result * 26 + ord(char) - ord("A") + 1
    return result - 1


def load_shared_strings(workbook_zip: zipfile.ZipFile) -> list[str]:
    try:
        data = workbook_zip.read("xl/sharedStrings.xml")
    except KeyError:
        return []

    root = ElementTree.fromstring(data)
    return [
        "".join(text.text or "" for text in item.findall(".//a:t", XLSX_NS))
        for item in root.findall("a:si", XLSX_NS)
    ]


def cell_value(cell: ElementTree.Element, shared_strings: list[str]) -> str:
    cell_type = cell.attrib.get("t")
    if cell_type == "inlineStr":
        return "".join(text.text or "" for text in cell.findall(".//a:t", XLSX_NS))

    value = cell.find("a:v", XLSX_NS)
    if value is None or value.text is None:
        return ""
    if cell_type == "s":
        return shared_strings[int(value.text)]
    return value.text


def read_workbook_sheets(workbook_path: Path) -> dict[str, list[list[str]]]:
    with zipfile.ZipFile(workbook_path) as workbook_zip:
        shared_strings = load_shared_strings(workbook_zip)
        workbook = ElementTree.fromstring(workbook_zip.read("xl/workbook.xml"))
        relationships = ElementTree.fromstring(
            workbook_zip.read("xl/_rels/workbook.xml.rels")
        )
        relationship_targets = {
            relationship.attrib["Id"]: relationship.attrib["Target"]
            for relationship in relationships.findall("p:Relationship", XLSX_NS)
        }

        sheets: dict[str, list[list[str]]] = {}
        relationship_id_name = (
            "{http://schemas.openxmlformats.org/officeDocument/2006/relationships}id"
        )
        for sheet in workbook.findall("a:sheets/a:sheet", XLSX_NS):
            name = sheet.attrib["name"]
            target = relationship_targets[sheet.attrib[relationship_id_name]].lstrip("/")
            if not target.startswith("xl/"):
                target = "xl/" + target
            target = target.replace("xl/../", "")

            root = ElementTree.fromstring(workbook_zip.read(target))
            rows: list[list[str]] = []
            for row in root.findall(".//a:sheetData/a:row", XLSX_NS):
                values: list[str] = []
                for cell in row.findall("a:c", XLSX_NS):
                    index = column_index(cell.attrib.get("r", ""))
                    while len(values) <= index:
                        values.append("")
                    values[index] = cell_value(cell, shared_strings).strip()
                rows.append(values)
            sheets[name] = rows
        return sheets


def row_value(row: list[str], headers: dict[str, int], name: str) -> str:
    index = headers[name]
    return row[index] if index < len(row) else ""


def require_headers(rows: list[list[str]], names: list[str], sheet_name: str) -> dict[str, int]:
    if not rows:
        raise ValueError(f"{sheet_name} is empty")
    headers = {value.strip(): index for index, value in enumerate(rows[0]) if value.strip()}
    missing = [name for name in names if name not in headers]
    if missing:
        raise ValueError(f"{sheet_name} is missing columns: {', '.join(missing)}")
    return headers


def parse_bool(value: str, location: str) -> bool:
    normalized = value.strip().lower()
    if normalized in {"1", "true", "yes"}:
        return True
    if normalized in {"0", "false", "no", ""}:
        return False
    raise ValueError(f"{location}: expected 0/1 boolean, got {value!r}")


def parse_int(value: str, location: str) -> int:
    try:
        return int(float(value))
    except ValueError as exc:
        raise ValueError(f"{location}: expected integer, got {value!r}") from exc


def normalize_def_name(value: str) -> str:
    return value.strip().lower()


def parse_optional_def_name(value: str, location: str) -> str | None:
    normalized = normalize_def_name(value)
    if normalized in {"", "n/a"}:
        return None
    try:
        normalized.encode("ascii")
    except UnicodeEncodeError as exc:
        raise ValueError(f"{location}: DEF name must be ASCII") from exc
    if not normalized.endswith(".def"):
        raise ValueError(f"{location}: expected a .def name or n/a, got {value!r}")
    return normalized


def fnv1a_32(value: str) -> int:
    result = 0x811C9DC5
    for byte in value.encode("ascii"):
        result ^= byte
        result = (result * 0x01000193) & 0xFFFFFFFF
    return result


def cpp_string(value: str) -> str:
    return '"' + value.replace("\\", "\\\\").replace('"', '\\"') + '"'


def parse_mapping(
    rows: list[list[str]],
) -> tuple[
    list[str],
    dict[str, tuple[str, bool, int, str | None, tuple[str, ...]]],
]:
    preview_columns = [f"Preview Def {index}" for index in range(1, 5)]
    required = [
        "Category",
        "Button",
        "Selected By Default",
        *TERRAIN_BITS.keys(),
        "Icon Def",
        *preview_columns,
    ]
    headers = require_headers(rows, required, "Button Mapping")
    categories: list[str] = []
    mapping: dict[
        str,
        tuple[str, bool, int, str | None, tuple[str, ...]],
    ] = {}
    for row_number, row in enumerate(rows[1:], start=2):
        button = row_value(row, headers, "Button")
        if not button:
            continue
        category = row_value(row, headers, "Category")
        if not category:
            raise ValueError(f"Button Mapping row {row_number}: Category is required")
        if category not in categories:
            categories.append(category)
        if button in mapping:
            raise ValueError(f"Button Mapping row {row_number}: duplicate button {button!r}")

        terrain_mask = 0
        for terrain, bit in TERRAIN_BITS.items():
            if parse_bool(row_value(row, headers, terrain), f"Button Mapping {button} {terrain}"):
                terrain_mask |= 1 << bit
        if terrain_mask & (1 << TERRAIN_BITS["RCK"]):
            raise ValueError(f"Button Mapping {button}: Rock applicability is not supported")

        icon_def = parse_optional_def_name(
            row_value(row, headers, "Icon Def"),
            f"Button Mapping {button} Icon Def",
        )
        preview_defs: list[str] = []
        found_unused_preview = False
        for column in preview_columns:
            preview_def = parse_optional_def_name(
                row_value(row, headers, column),
                f"Button Mapping {button} {column}",
            )
            if preview_def is None:
                found_unused_preview = True
            elif found_unused_preview:
                raise ValueError(
                    f"Button Mapping {button}: preview DEFs must be contiguous from Preview Def 1"
                )
            else:
                preview_defs.append(preview_def)
        if bool(icon_def) != bool(preview_defs):
            raise ValueError(
                f"Button Mapping {button}: Icon Def and Preview Def 1 must either both be set or both be unused"
            )

        mapping[button] = (
            category,
            parse_bool(
                row_value(row, headers, "Selected By Default"),
                f"Button Mapping {button} Selected By Default",
            ),
            terrain_mask,
            icon_def,
            tuple(preview_defs),
        )
    return categories, mapping


def parse_layout(
    rows: list[list[str]],
    categories: list[str],
    mapping: dict[
        str,
        tuple[str, bool, int, str | None, tuple[str, ...]],
    ],
) -> tuple[list[ButtonDefinition], list[str]]:
    category_lookup = set(categories)
    layout_categories: list[str] = []
    current_category: str | None = None
    category_row = 0
    definitions: list[ButtonDefinition] = []
    seen: set[str] = set()

    for row_number, raw_row in enumerate(rows, start=1):
        row = (raw_row + ["", "", "", ""])[:4]
        nonempty = [value for value in row if value]
        if not nonempty:
            continue
        if len(nonempty) == 1 and row[0] in category_lookup:
            if row[0] in layout_categories:
                raise ValueError(
                    f"Button Layout row {row_number}: duplicate category {row[0]!r}"
                )
            current_category = row[0]
            layout_categories.append(current_category)
            category_row = 0
            continue
        if current_category is None:
            raise ValueError(f"Button Layout row {row_number}: buttons precede a category")

        for column, button in enumerate(row):
            if not button:
                continue
            if button not in mapping:
                raise ValueError(f"Button Layout row {row_number}: unknown button {button!r}")
            (
                mapped_category,
                default_selected,
                terrain_mask,
                icon_def,
                preview_defs,
            ) = mapping[button]
            if mapped_category != current_category:
                raise ValueError(
                    f"Button Layout row {row_number}: {button!r} is under {current_category!r} "
                    f"but maps to {mapped_category!r}"
                )
            if button in seen:
                raise ValueError(f"Button Layout row {row_number}: duplicate button {button!r}")
            seen.add(button)
            definitions.append(
                ButtonDefinition(
                    name=button,
                    terrain_mask=terrain_mask,
                    default_selected=default_selected,
                    category_index=layout_categories.index(current_category),
                    row=category_row,
                    column=column,
                    icon_def=icon_def,
                    preview_defs=preview_defs,
                )
            )
        category_row += 1

    missing = sorted(set(mapping) - seen)
    if missing:
        raise ValueError("Button Layout is missing buttons: " + ", ".join(missing))
    if len(definitions) > 128:
        raise ValueError(f"Button Layout has {len(definitions)} buttons; maximum is 128")
    return definitions, layout_categories


def read_template_rows(path: Path) -> dict[str, list[list[str]]]:
    lines = path.read_text(encoding="utf-8-sig").splitlines()
    if not lines:
        raise ValueError(f"{path} is empty")
    rows: dict[str, list[list[str]]] = {}
    for line_number, line in enumerate(lines, start=1):
        stripped = line.strip()
        if not stripped or stripped.startswith("//"):
            continue
        fields = stripped.split()
        if len(fields) < 9:
            raise ValueError(f"{path.name} line {line_number}: expected at least 9 fields")
        rows.setdefault(normalize_def_name(fields[0]), []).append(fields)
    return rows


def parse_obstacles(
    rows: list[list[str]],
    button_indexes: dict[str, int],
    edobjts_rows: dict[str, list[list[str]]],
) -> tuple[list[DefMetadata], list[TemplateOverride]]:
    required = [
        "Button",
        "Def",
        "ID",
        "SubID",
        "Source",
        "Leveraged RandTrn",
        "Random Mask",
        "Is Custom",
    ]
    headers = require_headers(rows, required, "Obstacles")
    by_def: dict[str, DefMetadata] = {}
    overrides: dict[str, TemplateOverride] = {}

    for row_number, row in enumerate(rows[1:], start=2):
        if not any(row):
            continue
        button = row_value(row, headers, "Button")
        def_name = normalize_def_name(row_value(row, headers, "Def"))
        if not button or not def_name:
            raise ValueError(f"Obstacles row {row_number}: Button and Def are required")
        if button not in button_indexes:
            raise ValueError(f"Obstacles row {row_number}: unknown button {button!r}")

        source = row_value(row, headers, "Source").lower()
        if source not in {"hota", "plugin"}:
            raise ValueError(f"Obstacles row {row_number}: unsupported Source {source!r}")
        leveraged = row_value(row, headers, "Leveraged RandTrn")
        leveraged_normalized = leveraged.lower()
        borrow_source_hash = None
        if leveraged_normalized not in {"self", "native bucket"}:
            if not leveraged:
                raise ValueError(f"Obstacles row {row_number}: Leveraged RandTrn is required")
            borrow_source_hash = fnv1a_32(normalize_def_name(leveraged))

        metadata = DefMetadata(
            def_name=def_name,
            def_hash=fnv1a_32(def_name),
            button_index=button_indexes[button],
            borrow_source_hash=borrow_source_hash,
        )
        existing = by_def.get(def_name)
        if existing and existing != metadata:
            raise ValueError(f"Obstacles row {row_number}: conflicting metadata for {def_name}")
        by_def[def_name] = metadata

        is_custom = parse_bool(row_value(row, headers, "Is Custom"), f"Obstacles row {row_number} Is Custom")
        if source != "plugin" and not is_custom:
            continue

        object_id = parse_int(row_value(row, headers, "ID"), f"Obstacles row {row_number} ID")
        subtype = parse_int(row_value(row, headers, "SubID"), f"Obstacles row {row_number} SubID")
        random_mask = row_value(row, headers, "Random Mask")
        if not re.fullmatch(r"[01]{12}", random_mask):
            raise ValueError(f"Obstacles row {row_number}: invalid Random Mask {random_mask!r}")

        candidates = [
            fields
            for fields in edobjts_rows.get(def_name, [])
            if parse_int(fields[5], f"edobjts {def_name} ID") == object_id
            and parse_int(fields[6], f"edobjts {def_name} SubID") == subtype
        ]
        if len(candidates) != 1:
            raise ValueError(
                f"Obstacles row {row_number}: expected one edobjts row for "
                f"{def_name} ({object_id}, {subtype}), found {len(candidates)}"
            )
        fields = list(candidates[0])
        fields[4] = random_mask
        override = TemplateOverride(
            def_name=def_name,
            def_hash=fnv1a_32(def_name),
            button_index=button_indexes[button],
            row=" ".join(fields),
        )
        existing_override = overrides.get(def_name)
        if existing_override and existing_override != override:
            raise ValueError(f"Obstacles row {row_number}: conflicting override for {def_name}")
        overrides[def_name] = override

    workbook_hashes = {metadata.def_hash for metadata in by_def.values()}
    names_by_hash: dict[int, str] = {}
    for metadata in by_def.values():
        previous_name = names_by_hash.get(metadata.def_hash)
        if previous_name and previous_name != metadata.def_name:
            raise ValueError(
                f"Obstacles: FNV-1a collision between {previous_name!r} and "
                f"{metadata.def_name!r}"
            )
        names_by_hash[metadata.def_hash] = metadata.def_name
        if metadata.borrow_source_hash is not None and metadata.borrow_source_hash not in workbook_hashes:
            raise ValueError(
                f"Obstacles: {metadata.def_name} borrows a DEF not present in the workbook"
            )
    missing_buttons = sorted(set(button_indexes.values()) - {item.button_index for item in by_def.values()})
    if missing_buttons:
        raise ValueError(
            "Obstacles: buttons without DEF mappings: "
            + ", ".join(str(index) for index in missing_buttons)
        )
    return sorted(by_def.values(), key=lambda item: item.def_hash), sorted(
        overrides.values(), key=lambda item: item.def_name
    )


def validate_button_visual_defs(
    buttons: list[ButtonDefinition],
    metadata: list[DefMetadata],
) -> None:
    defs_by_button: dict[int, set[str]] = {}
    for item in metadata:
        defs_by_button.setdefault(item.button_index, set()).add(item.def_name)

    for button_index, button in enumerate(buttons):
        available_defs = defs_by_button.get(button_index, set())
        selections = []
        if button.icon_def:
            selections.append(("Icon Def", button.icon_def))
        selections.extend(
            (f"Preview Def {index}", def_name)
            for index, def_name in enumerate(button.preview_defs, start=1)
        )
        for column, def_name in selections:
            if def_name not in available_defs:
                raise ValueError(
                    f"Button Mapping {button.name} {column}: {def_name!r} is not mapped to this button on Obstacles"
                )


def render_header(button_count: int, category_count: int) -> str:
    return textwrap.dedent(
        f"""\
        #pragma once

        #include "pch.h"

        // Generated by tools/generate_obstacle_buttons.py.
        // Run the generator instead of editing this file by hand.
        constexpr int kObstacleButtonCount = {button_count};
        constexpr int kObstacleCategoryCount = {category_count};
        constexpr int kObstacleButtonsPerRow = 4;

        struct ObstacleButtonDefinition
        {{
            const char* name;
            DWORD terrainMask;
            BYTE categoryIndex;
            BYTE row;
            BYTE column;
            bool defaultSelected;
            const char* iconDefName;
            const char* previewDefNames[4];
            BYTE previewDefCount;
        }};

        struct ObstacleTemplateOverride
        {{
            const char* defName;
            DWORD defNameHash;
            BYTE buttonIndex;
            const char* row;
        }};

        bool ObstacleButtonDataAvailable();
        const char* ObstacleCategoryName(int categoryIndex);
        const ObstacleButtonDefinition* ObstacleButtonDefinitionAt(int buttonIndex);
        const char* ObstacleButtonName(int buttonIndex);
        void __stdcall RegisterObstacleButtonDefName(DWORD objectTemplate, DWORD editorString);
        int ObstacleButtonIndexForObjectTemplate(DWORD objectTemplate);
        void ObserveObstacleBorrowSourceRow(DWORD objectTemplate, DWORD rowRecord);
        DWORD ObstacleBorrowedRowForObjectTemplate(DWORD objectTemplate);
        void ResetObstacleBorrowedRows();
        DWORD ObstacleTemplateOverrideCount();
        const ObstacleTemplateOverride* ObstacleTemplateOverrideAt(DWORD index);
        """
    )


def render_source(
    buttons: list[ButtonDefinition],
    metadata: list[DefMetadata],
    overrides: list[TemplateOverride],
    categories: list[str],
) -> str:
    borrow_hashes = sorted(
        {item.borrow_source_hash for item in metadata if item.borrow_source_hash is not None}
    )
    if len(metadata) >= 0xFFFF:
        raise ValueError("too many DEF mappings for the runtime WORD index")
    if len(borrow_hashes) > 0x7F:
        raise ValueError("too many borrowed RandTrn sources for the runtime index")
    borrow_indexes = {value: index for index, value in enumerate(borrow_hashes)}
    button_rows = []
    for item in buttons:
        preview_defs = list(item.preview_defs) + [None] * (4 - len(item.preview_defs))
        preview_initializer = ", ".join(
            cpp_string(def_name) if def_name else "nullptr"
            for def_name in preview_defs
        )
        button_rows.append(
            f"        {{ {cpp_string(item.name)}, 0x{item.terrain_mask:08X}u, "
            f"{item.category_index}, {item.row}, {item.column}, "
            f"{'true' if item.default_selected else 'false'}, "
            f"{cpp_string(item.icon_def) if item.icon_def else 'nullptr'}, "
            f"{{ {preview_initializer} }}, {len(item.preview_defs)} }},"
        )
    metadata_rows = [
        f"        {{ 0x{item.def_hash:08X}u, {item.button_index}, "
        f"{borrow_indexes[item.borrow_source_hash] if item.borrow_source_hash is not None else -1} }},"
        for item in metadata
    ]
    borrow_rows = [f"        0x{value:08X}u," for value in borrow_hashes]
    override_rows = [
        f"        {{ {cpp_string(item.def_name)}, 0x{item.def_hash:08X}u, "
        f"{item.button_index}, {cpp_string(item.row)} }},"
        for item in overrides
    ]

    return "\n".join(
        [
            '#include "pch.h"',
            '#include "obstacle_button_data.h"',
            "",
            "// Generated by tools/generate_obstacle_buttons.py.",
            "namespace",
            "{",
            "    struct ObstacleDefMetadata",
            "    {",
            "        DWORD defNameHash;",
            "        BYTE buttonIndex;",
            "        signed char borrowSourceIndex;",
            "    };",
            "",
            "    constexpr ObstacleButtonDefinition kObstacleButtons[] =",
            "    {",
            *button_rows,
            "    };",
            "    constexpr const char* kObstacleCategoryNames[] = {",
            *[f"        {cpp_string(name)}," for name in categories],
            "    };",
            "    static_assert(sizeof(kObstacleCategoryNames) / sizeof(kObstacleCategoryNames[0]) == kObstacleCategoryCount);",
            "",
            "    constexpr ObstacleDefMetadata kObstacleDefMetadata[] =",
            "    {",
            *metadata_rows,
            "    };",
            "",
            "    constexpr DWORD kBorrowSourceDefNameHashes[] =",
            "    {",
            *borrow_rows,
            "    };",
            "",
            "    constexpr ObstacleTemplateOverride kObstacleTemplateOverrides[] =",
            "    {",
            *override_rows,
            "    };",
            "",
            "    static_assert(sizeof(kObstacleButtons) / sizeof(kObstacleButtons[0]) == kObstacleButtonCount,",
            '        "generated obstacle button count mismatch");',
            "    static_assert(sizeof(kObstacleDefMetadata) / sizeof(kObstacleDefMetadata[0]) < 0xFFFF,",
            '        "generated obstacle DEF mapping count exceeds runtime storage");',
            "",
            "    constexpr DWORD kRuntimeDefKeyCapacity = 8192;",
            "    WORD s_metadataIndexByDefKey[kRuntimeDefKeyCapacity] = {};",
            f"    DWORD s_borrowSourceRows[{max(1, len(borrow_hashes))}] = {{}};",
            "",
            "    bool SafeReadDword(DWORD address, DWORD& value)",
            "    {",
            "        __try",
            "        {",
            "            value = *reinterpret_cast<const DWORD*>(address);",
            "            return true;",
            "        }",
            "        __except (EXCEPTION_EXECUTE_HANDLER)",
            "        {",
            "            value = 0;",
            "            return false;",
            "        }",
            "    }",
            "",
            "    bool TryHashCStringAt(DWORD textAddress, DWORD& hash)",
            "    {",
            "        if (!textAddress)",
            "            return false;",
            "        __try",
            "        {",
            "            const char* cursor = reinterpret_cast<const char*>(textAddress);",
            "            hash = 2166136261u;",
            "            DWORD length = 0;",
            "            bool hasDot = false;",
            "            while (*cursor && length < 64)",
            "            {",
            "                BYTE value = static_cast<BYTE>(*cursor);",
            "                if (value < 0x21 || value > 0x7Eu)",
            "                    return false;",
            "                if (value == '.')",
            "                    hasDot = true;",
            "                if (value >= 'A' && value <= 'Z')",
            "                    value = static_cast<BYTE>(value - 'A' + 'a');",
            "                hash ^= value;",
            "                hash *= 16777619u;",
            "                ++cursor;",
            "                ++length;",
            "            }",
            "            return length > 0 && length < 64 && hasDot;",
            "        }",
            "        __except (EXCEPTION_EXECUTE_HANDLER)",
            "        {",
            "            hash = 0;",
            "            return false;",
            "        }",
            "    }",
            "",
            "    bool TryHashEditorString(DWORD editorString, DWORD& hash)",
            "    {",
            "        if (TryHashCStringAt(editorString, hash))",
            "            return true;",
            "        const DWORD offsets[] = { 0u, 4u, 8u, 12u, 16u };",
            "        for (DWORD offset : offsets)",
            "        {",
            "            DWORD candidate = 0;",
            "            if (SafeReadDword(editorString + offset, candidate) &&",
            "                TryHashCStringAt(candidate, hash))",
            "                return true;",
            "        }",
            "        hash = 0;",
            "        return false;",
            "    }",
            "",
            "    int FindMetadataIndex(DWORD hash)",
            "    {",
            "        int low = 0;",
            "        int high = static_cast<int>(sizeof(kObstacleDefMetadata) / sizeof(kObstacleDefMetadata[0]));",
            "        while (low < high)",
            "        {",
            "            const int mid = low + (high - low) / 2;",
            "            if (kObstacleDefMetadata[mid].defNameHash < hash)",
            "                low = mid + 1;",
            "            else if (kObstacleDefMetadata[mid].defNameHash > hash)",
            "                high = mid;",
            "            else",
            "                return mid;",
            "        }",
            "        return -1;",
            "    }",
            "",
            "    const ObstacleDefMetadata* MetadataForObjectTemplate(DWORD objectTemplate)",
            "    {",
            "        DWORD defKey = 0;",
            "        if (!SafeReadDword(objectTemplate, defKey) || defKey >= kRuntimeDefKeyCapacity)",
            "            return nullptr;",
            "        const WORD storedIndex = s_metadataIndexByDefKey[defKey];",
            "        if (!storedIndex)",
            "            return nullptr;",
            "        return &kObstacleDefMetadata[storedIndex - 1];",
            "    }",
            "}",
            "",
            "const char* ObstacleCategoryName(int categoryIndex)",
            "{",
            "    return categoryIndex >= 0 && categoryIndex < kObstacleCategoryCount",
            '        ? kObstacleCategoryNames[categoryIndex] : "";',
            "}",
            "",
            "bool ObstacleButtonDataAvailable()",
            "{",
            "    return kObstacleButtonCount > 0 &&",
            "        sizeof(kObstacleButtons) / sizeof(kObstacleButtons[0]) == kObstacleButtonCount;",
            "}",
            "",
            "const ObstacleButtonDefinition* ObstacleButtonDefinitionAt(int buttonIndex)",
            "{",
            "    if (buttonIndex < 0 || buttonIndex >= kObstacleButtonCount)",
            "        return nullptr;",
            "    return &kObstacleButtons[buttonIndex];",
            "}",
            "",
            "const char* ObstacleButtonName(int buttonIndex)",
            "{",
            "    const ObstacleButtonDefinition* definition = ObstacleButtonDefinitionAt(buttonIndex);",
            "    return definition ? definition->name : nullptr;",
            "}",
            "",
            "void __stdcall RegisterObstacleButtonDefName(DWORD objectTemplate, DWORD editorString)",
            "{",
            "    DWORD defKey = 0;",
            "    DWORD hash = 0;",
            "    if (!SafeReadDword(objectTemplate, defKey) || defKey >= kRuntimeDefKeyCapacity ||",
            "        !TryHashEditorString(editorString, hash))",
            "        return;",
            "    const int index = FindMetadataIndex(hash);",
            "    if (index >= 0)",
            "        s_metadataIndexByDefKey[defKey] = static_cast<WORD>(index + 1);",
            "}",
            "",
            "int ObstacleButtonIndexForObjectTemplate(DWORD objectTemplate)",
            "{",
            "    const ObstacleDefMetadata* metadata = MetadataForObjectTemplate(objectTemplate);",
            "    return metadata ? metadata->buttonIndex : -1;",
            "}",
            "",
            "void ObserveObstacleBorrowSourceRow(DWORD objectTemplate, DWORD rowRecord)",
            "{",
            "    if (!rowRecord)",
            "        return;",
            "    const ObstacleDefMetadata* metadata = MetadataForObjectTemplate(objectTemplate);",
            "    if (!metadata)",
            "        return;",
            f"    for (int index = 0; index < {len(borrow_hashes)}; ++index)",
            "    {",
            "        if (kBorrowSourceDefNameHashes[index] == metadata->defNameHash &&",
            "            s_borrowSourceRows[index] == 0)",
            "        {",
            "            s_borrowSourceRows[index] = rowRecord;",
            "            return;",
            "        }",
            "    }",
            "}",
            "",
            "DWORD ObstacleBorrowedRowForObjectTemplate(DWORD objectTemplate)",
            "{",
            "    const ObstacleDefMetadata* metadata = MetadataForObjectTemplate(objectTemplate);",
            "    if (!metadata || metadata->borrowSourceIndex < 0)",
            "        return 0;",
            "    return s_borrowSourceRows[metadata->borrowSourceIndex];",
            "}",
            "",
            "void ResetObstacleBorrowedRows()",
            "{",
            "    ZeroMemory(s_borrowSourceRows, sizeof(s_borrowSourceRows));",
            "}",
            "",
            "DWORD ObstacleTemplateOverrideCount()",
            "{",
            "    return static_cast<DWORD>(sizeof(kObstacleTemplateOverrides) / sizeof(kObstacleTemplateOverrides[0]));",
            "}",
            "",
            "const ObstacleTemplateOverride* ObstacleTemplateOverrideAt(DWORD index)",
            "{",
            "    return index < ObstacleTemplateOverrideCount() ? &kObstacleTemplateOverrides[index] : nullptr;",
            "}",
            "",
        ]
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--workbook", required=True, type=Path)
    parser.add_argument("--edobjts", required=True, type=Path)
    parser.add_argument("--header", default=Path("plugin/obstacle_button_data.h"), type=Path)
    parser.add_argument("--output", default=Path("plugin/obstacle_button_data.cpp"), type=Path)
    args = parser.parse_args()

    sheets = read_workbook_sheets(args.workbook)
    required_sheets = ["Obstacles", "Button Layout", "Button Mapping"]
    missing_sheets = [name for name in required_sheets if name not in sheets]
    if missing_sheets:
        raise ValueError("workbook is missing sheets: " + ", ".join(missing_sheets))

    categories, mapping = parse_mapping(sheets["Button Mapping"])
    buttons, layout_categories = parse_layout(
        sheets["Button Layout"], categories, mapping
    )
    button_indexes = {button.name: index for index, button in enumerate(buttons)}
    edobjts_rows = read_template_rows(args.edobjts)
    metadata, overrides = parse_obstacles(
        sheets["Obstacles"], button_indexes, edobjts_rows
    )
    validate_button_visual_defs(buttons, metadata)

    args.header.write_text(
        render_header(len(buttons), len(layout_categories)), encoding="ascii", newline="\n"
    )
    args.output.write_text(
        render_source(buttons, metadata, overrides, layout_categories), encoding="ascii", newline="\n"
    )
    borrow_count = len(
        {item.borrow_source_hash for item in metadata if item.borrow_source_hash is not None}
    )
    print(
        f"wrote {args.output} and {args.header}: {len(buttons)} buttons, "
        f"{len(layout_categories)} categories, {len(metadata)} DEF mappings, "
        f"{len(overrides)} template overrides, {borrow_count} borrowed RandTrn sources"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
