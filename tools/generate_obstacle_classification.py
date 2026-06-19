#!/usr/bin/env python3
"""Generate the small rand_trn obstacle thickness table for the plugin.

The input workbook is expected to contain the columns:

    Def, Name, ID, SubID, Thickness

Only ID, SubID, and Thickness are used. Thickness values must be "thick" or
"thin" when present. Blank rows are treated as unclassified.
"""

from __future__ import annotations

import argparse
import csv
import re
import sys
import textwrap
import zipfile
from pathlib import Path
from xml.etree import ElementTree


XLSX_NS = {
    "a": "http://schemas.openxmlformats.org/spreadsheetml/2006/main",
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


def workbook_classifications(workbook_path: Path) -> dict[tuple[int, int], str]:
    rows = read_first_sheet_rows(workbook_path)
    if not rows:
        raise ValueError("workbook is empty")

    headers = {normalize_header(value): index for index, value in enumerate(rows[0])}
    required = {
        "id": "ID",
        "subid": "SubID",
        "thickness": "Thickness",
    }
    missing = [
        display_name
        for normalized, display_name in required.items()
        if normalized not in headers
    ]
    if missing:
        raise ValueError(
            "workbook is missing required column(s): " + ", ".join(missing)
        )

    by_key: dict[tuple[int, int], tuple[str, int]] = {}
    for row_index, row in enumerate(rows[1:], start=2):
        if not any(row):
            continue

        def get(name: str) -> str:
            index = headers[name]
            return row[index] if index < len(row) else ""

        thickness = parse_thickness(get("thickness"), row_index)
        if not thickness:
            continue

        key = (
            parse_int_cell(get("id"), row_index, "ID"),
            parse_int_cell(get("subid"), row_index, "SubID"),
        )
        existing = by_key.get(key)
        if existing and existing[0] != thickness:
            raise ValueError(
                f"conflicting Thickness for ID/SubID {key}: "
                f"row {existing[1]} is {existing[0]!r}, row {row_index} is {thickness!r}"
            )
        by_key[key] = (thickness, row_index)

    return {key: value for key, (value, _row) in by_key.items()}


def rand_trn_rows(rand_trn_path: Path) -> list[tuple[int, str, int, int]]:
    rows: list[tuple[int, str, int, int]] = []
    with rand_trn_path.open("r", encoding="utf-8-sig", newline="") as handle:
        reader = csv.reader(handle, delimiter="\t")
        for fields in reader:
            if len(fields) < 5:
                continue
            try:
                obstacle_id = int(fields[1])
                object_id = int(fields[3])
                subtype = int(fields[4])
            except ValueError:
                continue
            rows.append((obstacle_id, fields[0], object_id, subtype))
    return rows


def render_cpp(table: list[int], classified_rows: int, source_name: str) -> str:
    chunks = []
    for index in range(0, len(table), 25):
        line = ", ".join(str(value) for value in table[index:index + 25])
        chunks.append(f"        {line},")

    return textwrap.dedent(
        f"""\
        #include "pch.h"
        #include "obstacle_classification.h"

        namespace
        {{
            // Generated by tools/generate_obstacle_classification.py from {source_name}.
            // 0 = unknown/unclassified, 1 = thin, 2 = thick.
            constexpr BYTE kObstacleThicknessByRandTrnRow[200] =
            {{
        """
    ) + "\n".join(chunks) + textwrap.dedent(
        f"""
            }};
            constexpr DWORD kObstacleThicknessRowCount =
                sizeof(kObstacleThicknessByRandTrnRow) / sizeof(kObstacleThicknessByRandTrnRow[0]);
            constexpr DWORD kClassifiedObstacleRows = {classified_rows};
        }}

        bool ObstacleThicknessTableAvailable()
        {{
            return kClassifiedObstacleRows > 0;
        }}

        ObstacleThickness GetObstacleThicknessForRandTrnRow(DWORD rowId)
        {{
            if (rowId >= kObstacleThicknessRowCount)
                return ObstacleThickness::Unknown;

            return static_cast<ObstacleThickness>(kObstacleThicknessByRandTrnRow[rowId]);
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
    parser.add_argument("--rand-trn", required=True, type=Path)
    parser.add_argument(
        "--output",
        default=Path("plugin/obstacle_classification.cpp"),
        type=Path,
    )
    args = parser.parse_args()

    classifications = workbook_classifications(args.workbook)
    rand_rows = rand_trn_rows(args.rand_trn)
    if len(rand_rows) != 200:
        raise ValueError(f"expected 200 rand_trn rows, found {len(rand_rows)}")

    table = [0] * 200
    matched = 0
    for obstacle_id, _name, object_id, subtype in rand_rows:
        if obstacle_id < 1 or obstacle_id > 200:
            raise ValueError(f"out-of-range rand_trn obstacle id: {obstacle_id}")
        thickness = classifications.get((object_id, subtype))
        if thickness == "thin":
            table[obstacle_id - 1] = 1
            matched += 1
        elif thickness == "thick":
            table[obstacle_id - 1] = 2
            matched += 1

    args.output.write_text(
        render_cpp(table, matched, args.workbook.name),
        encoding="utf-8",
        newline="\n",
    )
    print(
        f"wrote {args.output}: classified {matched}/200 rand_trn rows "
        f"from {len(classifications)} workbook ID/SubID classifications"
    )
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"error: {exc}", file=sys.stderr)
        raise SystemExit(1)
