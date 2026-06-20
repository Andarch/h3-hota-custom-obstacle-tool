#!/usr/bin/env python3
"""Generate the DEF-name obstacle thickness table for the plugin.

The input workbook is expected to contain the columns:

    Def, Name, Thickness, ID, SubID

Only Def and Thickness determine the generated lookup. ID/SubID are parsed only
as workbook sanity checks; they are not reliable enough to distinguish visual
variants that share the same object identity.

The plugin registers the editor's runtime DEF key while objects.txt is parsed,
so the generated table stores lowercase DEF-name hashes instead of trying to
reproduce the editor's private key assignment offline.
"""

from __future__ import annotations

import argparse
import re
import sys
import textwrap
import zipfile
from pathlib import Path
from xml.etree import ElementTree


XLSX_NS = {
    "a": "http://schemas.openxmlformats.org/spreadsheetml/2006/main",
}

HEADER_ALIASES = {
    "def": "def",
    "deffilename": "def",
    "filename": "def",
    "name": "name",
    "objectname": "name",
    "thickness": "thickness",
    "id": "id",
    "objectid": "id",
    "subid": "subid",
    "subtype": "subid",
    "objectsubid": "subid",
    "objectsubtype": "subid",
}

THICKNESS_TO_BYTE = {
    "thin": 1,
    "thick": 2,
}


def column_index(cell_ref: str) -> int:
    letters = re.match(r"([A-Z]+)", cell_ref.upper())
    if not letters:
        raise ValueError(f"invalid cell reference: {cell_ref}")

    index = 0
    for char in letters.group(1):
        index = index * 26 + ord(char) - ord("A") + 1
    return index - 1


def load_shared_strings(workbook_zip: zipfile.ZipFile) -> list[str]:
    try:
        data = workbook_zip.read("xl/sharedStrings.xml")
    except KeyError:
        return []

    root = ElementTree.fromstring(data)
    strings: list[str] = []
    for item in root.findall("a:si", XLSX_NS):
        text_parts = [
            text.text or ""
            for text in item.findall(".//a:t", XLSX_NS)
        ]
        strings.append("".join(text_parts))
    return strings


def cell_value(cell: ElementTree.Element, shared_strings: list[str]) -> str:
    cell_type = cell.attrib.get("t")
    if cell_type == "inlineStr":
        return "".join(
            text.text or ""
            for text in cell.findall(".//a:t", XLSX_NS)
        )

    value = cell.find("a:v", XLSX_NS)
    if value is None or value.text is None:
        return ""

    raw = value.text
    if cell_type == "s":
        return shared_strings[int(raw)]
    return raw


def read_first_sheet_rows(workbook_path: Path) -> list[list[str]]:
    with zipfile.ZipFile(workbook_path) as workbook_zip:
        shared_strings = load_shared_strings(workbook_zip)
        sheet_xml = workbook_zip.read("xl/worksheets/sheet1.xml")

    root = ElementTree.fromstring(sheet_xml)
    rows: list[list[str]] = []
    for row in root.findall(".//a:sheetData/a:row", XLSX_NS):
        values: list[str] = []
        for cell in row.findall("a:c", XLSX_NS):
            ref = cell.attrib.get("r", "")
            index = column_index(ref)
            while len(values) <= index:
                values.append("")
            values[index] = cell_value(cell, shared_strings).strip()
        rows.append(values)
    return rows


def normalize_header(value: str) -> str:
    return re.sub(r"[^a-z0-9]+", "", value.lower())


def normalize_def_name(value: str) -> str:
    return value.strip().lower()


def parse_int_cell(value: str, row_number: int, column_name: str) -> int:
    try:
        return int(float(value))
    except ValueError as exc:
        raise ValueError(
            f"row {row_number}: invalid {column_name} value {value!r}"
        ) from exc


def parse_thickness(value: str, row_number: int) -> str | None:
    normalized = value.strip().lower()
    if not normalized:
        return None
    if normalized in {"thick", "tall"}:
        return "thick"
    if normalized in {"thin", "low"}:
        return "thin"
    raise ValueError(
        f"row {row_number}: Thickness must be 'thick' or 'thin', got {value!r}"
    )


def workbook_headers(header_row: list[str]) -> dict[str, int]:
    headers: dict[str, int] = {}
    for index, value in enumerate(header_row):
        canonical = HEADER_ALIASES.get(normalize_header(value))
        if canonical and canonical not in headers:
            headers[canonical] = index
    return headers


def workbook_classifications(workbook_path: Path) -> dict[str, str]:
    rows = read_first_sheet_rows(workbook_path)
    if not rows:
        raise ValueError("workbook is empty")

    headers = workbook_headers(rows[0])
    required = {
        "def": "Def",
        "id": "ID",
        "subid": "SubID",
        "thickness": "Thickness",
    }
    missing = [
        display_name
        for canonical, display_name in required.items()
        if canonical not in headers
    ]
    if missing:
        raise ValueError(
            "workbook is missing required column(s): " + ", ".join(missing)
        )

    by_def: dict[str, tuple[str, int]] = {}
    by_exact_row: dict[tuple[str, int, int], tuple[str, int]] = {}
    for row_index, row in enumerate(rows[1:], start=2):
        if not any(row):
            continue

        def get(name: str) -> str:
            index = headers[name]
            return row[index] if index < len(row) else ""

        thickness = parse_thickness(get("thickness"), row_index)
        if not thickness:
            continue

        def_name = normalize_def_name(get("def"))
        if not def_name:
            raise ValueError(f"row {row_index}: Def is required when Thickness is set")

        object_id = parse_int_cell(get("id"), row_index, "ID")
        subtype = parse_int_cell(get("subid"), row_index, "SubID")

        exact_key = (def_name, object_id, subtype)
        existing_exact = by_exact_row.get(exact_key)
        if existing_exact and existing_exact[0] != thickness:
            raise ValueError(
                f"conflicting Thickness for {def_name} ID/SubID "
                f"({object_id}, {subtype}): row {existing_exact[1]} is "
                f"{existing_exact[0]!r}, row {row_index} is {thickness!r}"
            )
        by_exact_row[exact_key] = (thickness, row_index)

        existing_def = by_def.get(def_name)
        if existing_def and existing_def[0] != thickness:
            raise ValueError(
                f"conflicting Thickness for DEF {def_name}: "
                f"row {existing_def[1]} is {existing_def[0]!r}, "
                f"row {row_index} is {thickness!r}"
            )
        by_def[def_name] = (thickness, row_index)

    return {key: value for key, (value, _row) in by_def.items()}


def object_def_names(objects_path: Path) -> set[str]:
    lines = objects_path.read_text(encoding="utf-8-sig").splitlines()
    if not lines:
        raise ValueError("objects.txt is empty")

    try:
        declared_rows = int(lines[0].strip())
    except ValueError as exc:
        raise ValueError("objects.txt first line is not a row count") from exc

    data_lines = [line for line in lines[1:] if line.strip()]
    if declared_rows != len(data_lines):
        print(
            f"warning: objects.txt declares {declared_rows} rows but "
            f"contains {len(data_lines)} data rows",
            file=sys.stderr,
        )

    def_names: set[str] = set()
    for line_number, line in enumerate(data_lines, start=2):
        fields = line.split()
        if len(fields) < 8:
            raise ValueError(f"objects.txt line {line_number}: expected at least 8 fields")

        def_name = normalize_def_name(fields[0])
        def_names.add(def_name)

    return def_names


def fnv1a_32(value: str) -> int:
    hash_value = 0x811C9DC5
    for byte in value.encode("ascii"):
        hash_value ^= byte
        hash_value = (hash_value * 0x01000193) & 0xFFFFFFFF
    return hash_value


def render_cpp(table: list[tuple[int, int]], classified_defs: int, source_name: str) -> str:
    entries = []
    for index in range(0, len(table), 4):
        chunk = ", ".join(
            f"{{ 0x{def_hash:08X}u, {thickness} }}"
            for def_hash, thickness in table[index:index + 4]
        )
        entries.append(f"        {chunk},")

    return textwrap.dedent(
        f"""\
        #include "pch.h"
        #include "obstacle_classification.h"

        namespace
        {{
            struct ObstacleDefNameClassification
            {{
                DWORD defNameHash;
                BYTE thickness;
            }};

            // Generated by tools/generate_obstacle_classification.py from {source_name}.
            // Hashes are FNV-1a 32-bit hashes of lowercase DEF filenames.
            // 0 = unknown/unclassified, 1 = thin, 2 = thick.
            constexpr ObstacleDefNameClassification kObstacleDefNameClassifications[] =
            {{
        """
    ) + "\n".join(entries) + textwrap.dedent(
        f"""
            }};
            constexpr DWORD kClassifiedObstacleDefNames = {classified_defs};
            constexpr DWORD kRuntimeDefKeyCapacity = 8192;

            BYTE s_runtimeThicknessByDefKey[kRuntimeDefKeyCapacity] = {{}};

            bool SafeReadDword(DWORD address, DWORD& value)
            {{
                __try
                {{
                    value = *reinterpret_cast<const DWORD*>(address);
                    return true;
                }}
                __except (EXCEPTION_EXECUTE_HANDLER)
                {{
                    value = 0;
                    return false;
                }}
            }}

            bool TryHashCStringAt(DWORD textAddress, DWORD& hash)
            {{
                if (!textAddress)
                {{
                    hash = 0;
                    return false;
                }}

                __try
                {{
                    const char* cursor = reinterpret_cast<const char*>(textAddress);
                    hash = 2166136261u;
                    DWORD length = 0;
                    bool hasDot = false;
                    while (*cursor && length < 64)
                    {{
                        BYTE value = static_cast<BYTE>(*cursor);
                        if (value < 0x21 || value > 0x7Eu)
                        {{
                            hash = 0;
                            return false;
                        }}

                        if (value == '.')
                            hasDot = true;

                        if (value >= 'A' && value <= 'Z')
                            value = static_cast<BYTE>(value - 'A' + 'a');

                        hash ^= value;
                        hash *= 16777619u;
                        ++cursor;
                        ++length;
                    }}

                    return length > 0 && length < 64 && hasDot;
                }}
                __except (EXCEPTION_EXECUTE_HANDLER)
                {{
                    hash = 0;
                    return false;
                }}
            }}

            bool TryHashEditorString(DWORD editorString, DWORD& hash)
            {{
                if (TryHashCStringAt(editorString, hash))
                    return true;

                const DWORD pointerOffsets[] = {{ 0u, 4u, 8u, 12u, 16u }};
                for (DWORD index = 0;
                    index < sizeof(pointerOffsets) / sizeof(pointerOffsets[0]);
                    ++index)
                {{
                    DWORD candidate = 0;
                    if (SafeReadDword(editorString + pointerOffsets[index], candidate) &&
                        TryHashCStringAt(candidate, hash))
                    {{
                        return true;
                    }}
                }}

                hash = 0;
                return false;
            }}

            ObstacleThickness GetObstacleThicknessForDefNameHash(DWORD defNameHash)
            {{
                DWORD low = 0;
                DWORD high = kClassifiedObstacleDefNames;
                while (low < high)
                {{
                    const DWORD mid = low + (high - low) / 2;
                    const ObstacleDefNameClassification& candidate =
                        kObstacleDefNameClassifications[mid];
                    if (candidate.defNameHash < defNameHash)
                    {{
                        low = mid + 1;
                    }}
                    else if (candidate.defNameHash > defNameHash)
                    {{
                        high = mid;
                    }}
                    else
                    {{
                        return static_cast<ObstacleThickness>(candidate.thickness);
                    }}
                }}

                return ObstacleThickness::Unknown;
            }}
        }}

        bool ObstacleThicknessTableAvailable()
        {{
            return kClassifiedObstacleDefNames > 0;
        }}

        ObstacleThickness GetObstacleThicknessForRandTrnRow(DWORD)
        {{
            return ObstacleThickness::Unknown;
        }}

        void __stdcall RegisterObstacleTemplateDefName(DWORD objectTemplate, DWORD editorString)
        {{
            DWORD defKey = 0;
            if (!SafeReadDword(objectTemplate, defKey) || defKey >= kRuntimeDefKeyCapacity)
                return;

            DWORD defNameHash = 0;
            if (!TryHashEditorString(editorString, defNameHash))
                return;

            const ObstacleThickness thickness =
                GetObstacleThicknessForDefNameHash(defNameHash);
            if (thickness == ObstacleThickness::Unknown)
                return;

            s_runtimeThicknessByDefKey[defKey] = static_cast<BYTE>(thickness);
        }}

        ObstacleThickness GetObstacleThicknessForObjectTemplate(DWORD objectTemplate)
        {{
            DWORD defKey = 0;
            if (!SafeReadDword(objectTemplate, defKey))
                return ObstacleThickness::Unknown;

            if (defKey < kRuntimeDefKeyCapacity)
                return static_cast<ObstacleThickness>(s_runtimeThicknessByDefKey[defKey]);

            return ObstacleThickness::Unknown;
        }}

        const char* ObstacleThicknessName(ObstacleThickness thickness)
        {{
            switch (thickness)
            {{
            case ObstacleThickness::Thin:
                return "thin";
            case ObstacleThickness::Thick:
                return "thick";
            default:
                return "unknown";
            }}
        }}
        """
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--workbook", required=True, type=Path)
    parser.add_argument("--objects", required=True, type=Path)
    parser.add_argument(
        "--rand-trn",
        type=Path,
        help="deprecated; kept for compatibility and ignored",
    )
    parser.add_argument(
        "--output",
        default=Path("plugin/obstacle_classification.cpp"),
        type=Path,
    )
    args = parser.parse_args()

    classifications = workbook_classifications(args.workbook)
    object_defs = object_def_names(args.objects)

    missing_defs = sorted(def_name for def_name in classifications if def_name not in object_defs)
    for def_name in missing_defs:
        print(
            f"warning: workbook DEF {def_name} was not found in objects.txt",
            file=sys.stderr,
        )

    hashes: dict[int, tuple[str, str]] = {}
    table: list[tuple[int, int]] = []
    for def_name, thickness in classifications.items():
        def_hash = fnv1a_32(def_name)
        existing = hashes.get(def_hash)
        if existing and existing[1] != thickness:
            raise ValueError(
                f"hash collision between {existing[0]} ({existing[1]}) and "
                f"{def_name} ({thickness})"
            )
        hashes[def_hash] = (def_name, thickness)
        table.append((def_hash, THICKNESS_TO_BYTE[thickness]))

    table.sort()

    args.output.write_text(
        render_cpp(table, len(table), f"{args.workbook.name} + {args.objects.name}"),
        encoding="utf-8",
        newline="\n",
    )
    print(
        f"wrote {args.output}: classified {len(table)} DEF-name hashes "
        f"from {len(classifications)} workbook DEF classifications "
        f"({len(missing_defs)} missing in objects.txt)"
    )
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"error: {exc}", file=sys.stderr)
        raise SystemExit(1)
