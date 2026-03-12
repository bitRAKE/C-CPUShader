from __future__ import annotations

import subprocess
from pathlib import Path


HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
TOOL = Path(r"msdf-atlas-gen.exe")
FONT = ROOT / "pocs" / "mtsdf" / "NotoSans[wdth,wght].ttf"
ATLAS_PNG = HERE / "ascii_sdf_grid.png"


def generate_atlas() -> None:
    command = [
        str(TOOL),
        "-varfont",
        f"{FONT}?wght=800&wdth=100",
        "-fontname",
        "Noto Sans ASCII Grid",
        "-chars",
        "[0x20, 0x7E]",
        "-type",
        "sdf",
        "-size",
        "64",
        "-pxrange",
        "6.1",
        "-uniformgrid",
        "-uniformcols",
        "16",
        "-uniformcell",
        "64",
        "96",
        "-uniformorigin",
        "on",
        "-yorigin",
        "bottom",
        "-imageout",
        str(ATLAS_PNG),
    ]

    subprocess.run(command, check=True, cwd=ROOT)


def main() -> None:
    if not TOOL.exists():
        raise SystemExit(f"msdf-atlas-gen not found: {TOOL}")
    if not FONT.exists():
        raise SystemExit(f"Font not found: {FONT}")

    generate_atlas()
    print(f"Wrote {ATLAS_PNG}")


if __name__ == "__main__":
    main()
