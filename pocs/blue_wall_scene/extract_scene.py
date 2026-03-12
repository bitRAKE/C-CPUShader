from __future__ import annotations

import argparse
import os
import subprocess
import sys
from pathlib import Path

from scene_extract_post import postprocess


def default_blender_path() -> Path:
    env_value = os.environ.get("BLENDER_EXE")
    if env_value:
        return Path(env_value)
    return Path(r"blender.exe")


def main() -> int:
    root = Path(__file__).resolve().parent
    default_blend = root / "blue_wall" / "blue_wall.blend"
    raw_json = root / "scene_extract_raw.json"
    processed_json = root / "scene_extract.json"
    summary_md = root / "scene_extract_summary.md"
    legacy_md = root / "scene_extract.md"
    generated_h = root / "scene_extract_generated.h"
    legacy_h = root / "scene_extract.h"
    groups_h = root / "scene_groups.h"
    stage_a_h = root / "scene_stage_A.h"
    stage_b_h = root / "scene_stage_B.h"
    stage_c_h = root / "scene_stage_C.h"
    stage_d_h = root / "scene_stage_D.h"
    stage_e_h = root / "scene_stage_E.h"
    recipe_path = root / "blue_wall_recipe.json"
    blender_script = root / "extract_scene_blender.py"

    parser = argparse.ArgumentParser(description="Extract Blue Wall scene metrics through Blender.")
    parser.add_argument("--blender", type=Path, default=default_blender_path(), help="Path to blender.exe")
    parser.add_argument("--blend", type=Path, default=default_blend, help="Path to the .blend file")
    parser.add_argument("--raw-json-out", type=Path, default=raw_json, help="Raw Blender JSON output path")
    parser.add_argument("--json-out", type=Path, default=processed_json, help="Processed JSON output path")
    parser.add_argument("--summary-md-out", type=Path, default=summary_md, help="Human-readable summary Markdown path")
    parser.add_argument("--md-out", type=Path, default=legacy_md, help="Compatibility Markdown path")
    parser.add_argument("--generated-h-out", type=Path, default=generated_h, help="Generated reconstruction header path")
    parser.add_argument("--c-out", type=Path, default=legacy_h, help="Compatibility C header path")
    parser.add_argument("--groups-h-out", type=Path, default=groups_h, help="Generated group header path")
    parser.add_argument("--stage-a-h-out", type=Path, default=stage_a_h, help="Generated Stage A header path")
    parser.add_argument("--stage-b-h-out", type=Path, default=stage_b_h, help="Generated Stage B header path")
    parser.add_argument("--stage-c-h-out", type=Path, default=stage_c_h, help="Generated Stage C header path")
    parser.add_argument("--stage-d-h-out", type=Path, default=stage_d_h, help="Generated Stage D header path")
    parser.add_argument("--stage-e-h-out", type=Path, default=stage_e_h, help="Generated Stage E header path")
    parser.add_argument("--recipe", type=Path, default=recipe_path, help="Scene reduction recipe path")
    args = parser.parse_args()

    if not args.blender.is_file():
        print(f"Blender executable not found: {args.blender}", file=sys.stderr)
        return 1

    if not args.blend.is_file():
        print(f"Blend file not found: {args.blend}", file=sys.stderr)
        return 1

    command = [
        str(args.blender),
        "-b",
        str(args.blend),
        "--python",
        str(blender_script),
        "--",
        "--raw-json-out",
        str(args.raw_json_out),
    ]

    completed = subprocess.run(command, cwd=root)
    if completed.returncode != 0:
        return completed.returncode

    postprocess(
        raw_json_path=args.raw_json_out,
        processed_json_path=args.json_out,
        summary_md_path=args.summary_md_out,
        generated_header_path=args.generated_h_out,
        groups_header_path=args.groups_h_out,
        stage_a_header_path=args.stage_a_h_out,
        stage_b_header_path=args.stage_b_h_out,
        stage_c_header_path=args.stage_c_h_out,
        stage_d_header_path=args.stage_d_h_out,
        stage_e_header_path=args.stage_e_h_out,
        legacy_md_path=args.md_out,
        legacy_header_path=args.c_out,
        recipe_path=args.recipe,
    )

    print(f"Wrote {args.raw_json_out}")
    print(f"Wrote {args.json_out}")
    print(f"Wrote {args.summary_md_out}")
    print(f"Wrote {args.generated_h_out}")
    print(f"Wrote {args.groups_h_out}")
    print(f"Wrote {args.stage_a_h_out}")
    print(f"Wrote {args.stage_b_h_out}")
    print(f"Wrote {args.stage_c_h_out}")
    print(f"Wrote {args.stage_d_h_out}")
    print(f"Wrote {args.stage_e_h_out}")
    print(f"Wrote {args.md_out}")
    print(f"Wrote {args.c_out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
