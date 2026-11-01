"""Package the supplied RGBA PNG sizes into the launcher's Windows icon."""

from pathlib import Path
import struct


ROOT = Path(__file__).resolve().parents[1]
SIZES = (16, 20, 24, 30, 32, 36, 40, 48, 60, 64, 72, 80, 96, 256)


def main():
    images = []
    for size in SIZES:
        path = ROOT / "resources" / "icon-source" / f"{size}.png"
        data = path.read_bytes()
        if (data[:8] != b"\x89PNG\r\n\x1a\n"
                or data[12:16] != b"IHDR" or len(data) < 33):
            raise ValueError(f"Invalid PNG header: {path}")
        width, height, depth, color = struct.unpack(">IIBB", data[16:26])
        if (width, height, depth, color) != (size, size, 8, 6):
            raise ValueError(f"Expected {size}x{size} 8-bit RGBA PNG: {path}")
        images.append(data)

    # ICO supports PNG payloads on the supported Windows 10/11 versions.
    # Preserve each supplied image exactly, including its alpha channel.
    offset = 6 + 16 * len(images)
    entries = []
    for size, data in zip(SIZES, images):
        dimension = size if size < 256 else 0
        entries.append(struct.pack(
            "<BBBBHHII", dimension, dimension, 0, 0, 1, 32, len(data), offset))
        offset += len(data)
    output = ROOT / "resources" / "app.ico"
    output.write_bytes(struct.pack("<HHH", 0, 1, len(images))
                       + b"".join(entries) + b"".join(images))
    print(f"Wrote {output} ({len(images)} sizes, {offset} bytes)")


if __name__ == "__main__":
    main()
