from __future__ import annotations

import json
import math
import re
from pathlib import Path


STAGE_ORDER = ["A", "B", "C", "D", "E"]
STAGE_BITS = {stage: 1 << index for index, stage in enumerate(STAGE_ORDER)}

DEFAULT_ROLE_PATTERNS = [
    ("room_shell", ("Room", "Cornice", "Skirting")),
    ("sideboard", ("Sideboard",)),
    ("bookshelf", ("VerticalBookShelf",)),
    ("books", ("Books_proxy",)),
    ("painting", ("CanvasPainting",)),
    ("chair", ("GreenChair",)),
    ("instrument", ("Ukulele",)),
    ("lamp", ("Lantern",)),
    ("table", ("side_table_tall", "WoodenTable")),
    ("plant", ("potted_plant",)),
    ("clock", ("mantel_clock",)),
    ("camera_prop", ("Camera_01",)),
    ("statue", ("horse_statue",)),
    ("box_prop", ("CheeseBox",)),
    ("light_cluster", ("Point", "Spot", "Window", "Ceiling")),
]

DEFAULT_PROXY_BY_ROLE = {
    "window_light": "area_light",
    "ceiling_light": "light_cluster",
    "lamp_light": "light_cluster",
    "lamp_fixture": "floor_lamp_cluster",
    "room_shell": "room_shell",
    "bookshelf": "box_frame",
    "sideboard": "box_stack",
    "painting": "framed_textured_quad",
    "chair": "chair_boxes",
    "table": "cylinder_table",
    "instrument": "capsule_plus_boxes",
    "plant": "pot_plus_foliage_cluster",
    "clock": "hero_prop_proxy",
    "camera_prop": "hero_prop_proxy",
    "statue": "hero_prop_proxy",
    "box_prop": "hero_prop_proxy",
}

DEFAULT_SEMANTIC_WEIGHTS = {
    "room_shell": 0.45,
    "window_light": 0.90,
    "ceiling_light": 0.65,
    "lamp_light": 0.75,
    "lamp_fixture": 0.92,
    "painting": 0.95,
    "bookshelf": 0.86,
    "sideboard": 0.88,
    "chair": 0.90,
    "table": 0.72,
    "instrument": 0.78,
    "plant": 0.62,
    "clock": 0.60,
    "camera_prop": 0.36,
    "statue": 0.52,
    "box_prop": 0.28,
    "lamp": 0.58,
    "mesh": 0.12,
    "light": 0.35,
    "camera": 0.10,
    "empty": 0.05,
}

DEFAULT_STAGE_THRESHOLDS = {
    "B": 0.16,
    "C": 0.24,
    "D": 0.42,
}


def clamp(value: float, minimum: float = 0.0, maximum: float = 1.0) -> float:
    return max(minimum, min(maximum, value))


def round_float(value: float, digits: int = 4) -> float:
    return round(float(value), digits)


def c_string(value: str | None) -> str:
    text = "" if value is None else str(value)
    text = text.replace("\\", "\\\\").replace('"', '\\"')
    return f"\"{text}\""


def c_vec3(values: list[float]) -> str:
    return "{ " + ", ".join(f"{float(value):.4f}f" for value in values[:3]) + " }"


def c_mask(mask: int) -> str:
    return f"0x{mask:02X}u"


def stage_mask_from_intro(intro_stage: str) -> int:
    start_index = STAGE_ORDER.index(intro_stage)
    mask = 0
    for stage in STAGE_ORDER[start_index:]:
        mask |= STAGE_BITS[stage]
    return mask


def strip_numeric_suffix(name: str) -> str:
    return re.sub(r"\.\d{3}$", "", name)


def first_texture_name(materials: list[dict]) -> str:
    for material in materials:
        for image in material.get("texture_images", []):
            linked = image.get("linked_sockets", [])
            if "Base Color" in linked:
                return Path(image["path"]).name
    for material in materials:
        for image in material.get("texture_images", []):
            image_name = Path(image["path"]).name.lower()
            if "diff" in image_name or "albedo" in image_name or "color" in image_name:
                return Path(image["path"]).name
    for material in materials:
        for image in material.get("texture_images", []):
            return Path(image["path"]).name
    return ""


def normalize_path_string(path_value: str, root: Path) -> str:
    if not path_value:
        return ""

    value = str(path_value).replace("\\", "/")
    candidate = Path(value)

    if candidate.is_absolute():
        try:
            return candidate.resolve().relative_to(root.resolve()).as_posix()
        except ValueError:
            return candidate.resolve().as_posix()

    return Path(value).as_posix()


def normalize_image_list(values: list[str], root: Path) -> list[str]:
    return [normalize_path_string(value, root) for value in values]


def normalize_materials(materials: list[dict], root: Path) -> None:
    for material in materials:
        for image in material.get("texture_images", []):
            image["path"] = normalize_path_string(image.get("path", ""), root)


def normalize_raw_paths(raw: dict, root: Path) -> None:
    world = raw.get("world", {})
    world["images"] = normalize_image_list(world.get("images", []), root)

    for entry in raw.get("objects", []):
        normalize_materials(entry.get("materials", []), root)

    for entry in raw.get("hero_meshes", []):
        normalize_materials(entry.get("materials", []), root)

    for entry in raw.get("named_assets", []):
        normalize_materials(entry.get("materials", []), root)


def first_proxy_color(materials: list[dict]) -> list[float]:
    for material in materials:
        proxy = material.get("proxy_color")
        if proxy:
            return proxy
    for material in materials:
        base = material.get("base_color")
        if base:
            return base
    return [0.5, 0.5, 0.5]


def vec_length(vec: list[float]) -> float:
    return math.sqrt(sum(component * component for component in vec))


def vec_sub(a: list[float], b: list[float]) -> list[float]:
    return [a[index] - b[index] for index in range(3)]


def distance(a: list[float], b: list[float]) -> float:
    return vec_length(vec_sub(a, b))


def get_world_center(entry: dict) -> list[float]:
    if entry.get("world_bounds") is not None:
        return entry["world_bounds"]["center"]
    return entry.get("location", [0.0, 0.0, 0.0])


def get_dimensions(entry: dict) -> list[float]:
    if entry.get("dimensions") is not None:
        return entry["dimensions"]
    if entry.get("world_bounds") is not None:
        minimum = entry["world_bounds"]["min"]
        maximum = entry["world_bounds"]["max"]
        return [maximum[index] - minimum[index] for index in range(3)]
    return [0.0, 0.0, 0.0]


def merge_bounds(entries: list[dict]) -> dict | None:
    if not entries:
        return None

    minimum = [float("inf"), float("inf"), float("inf")]
    maximum = [float("-inf"), float("-inf"), float("-inf")]

    for entry in entries:
        if entry.get("world_bounds") is not None:
            bounds_min = entry["world_bounds"]["min"]
            bounds_max = entry["world_bounds"]["max"]
        else:
            center = get_world_center(entry)
            bounds_min = center
            bounds_max = center

        for axis in range(3):
            minimum[axis] = min(minimum[axis], bounds_min[axis])
            maximum[axis] = max(maximum[axis], bounds_max[axis])

    center = [(minimum[index] + maximum[index]) * 0.5 for index in range(3)]
    dimensions = [maximum[index] - minimum[index] for index in range(3)]
    return {
        "min": [round_float(value) for value in minimum],
        "max": [round_float(value) for value in maximum],
        "center": [round_float(value) for value in center],
        "dimensions": [round_float(value) for value in dimensions],
    }


def load_recipe(recipe_path: Path) -> dict:
    if not recipe_path.is_file():
        return {
            "scene": "",
            "role_overrides": {},
            "proxy_overrides": {},
            "semantic_weights": {},
            "stage_thresholds": {},
            "groups": [],
        }
    return json.loads(recipe_path.read_text(encoding="utf-8"))


def add_candidate(candidates: dict, role: str, score: float, reason: str) -> None:
    entry = candidates.get(role)
    if entry is None:
        entry = {"role": role, "score": score, "reasons": [reason]}
        candidates[role] = entry
        return

    entry["score"] = max(entry["score"], score)
    if reason not in entry["reasons"]:
        entry["reasons"].append(reason)


def role_candidates_for_object(entry: dict, recipe: dict) -> list[dict]:
    candidates = {}
    name = entry["name"]
    obj_type = entry["type"]
    collections = entry.get("collections", [])
    role_guess = entry.get("role_guess", "")
    role_overrides = recipe.get("role_overrides", {})

    if name in role_overrides:
        add_candidate(candidates, role_overrides[name], 1.00, "recipe:name_override")

    if role_guess:
        add_candidate(candidates, role_guess, 0.56, "raw:role_guess")

    for role, patterns in DEFAULT_ROLE_PATTERNS:
        for pattern in patterns:
            if name == pattern:
                add_candidate(candidates, role, 0.92, f"name:eq:{pattern}")
            elif name.startswith(pattern):
                add_candidate(candidates, role, 0.84, f"name:prefix:{pattern}")
            elif pattern in name:
                add_candidate(candidates, role, 0.72, f"name:contains:{pattern}")

            if any(pattern in collection for collection in collections):
                add_candidate(candidates, role, 0.38, f"collection:{pattern}")

    if obj_type == "LIGHT":
        add_candidate(candidates, "light", 0.45, "type:light")
        light_info = entry.get("light", {})
        light_type = light_info.get("light_type", "")
        if light_type == "AREA":
            add_candidate(candidates, "window_light", 0.35, "light_type:area")
        if "Ceiling" in name:
            add_candidate(candidates, "ceiling_light", 0.70, "name:ceiling")
        if name.startswith("Point") or name.startswith("Spot"):
            add_candidate(candidates, "lamp_light", 0.68, "name:practical_cluster")
    elif obj_type == "CAMERA":
        add_candidate(candidates, "camera", 0.30, "type:camera")
    elif obj_type == "EMPTY":
        add_candidate(candidates, "empty", 0.20, "type:empty")
    else:
        add_candidate(candidates, "mesh", 0.10, "type:mesh")

    ordered = sorted(candidates.values(), key=lambda candidate: (-candidate["score"], candidate["role"]))
    for candidate in ordered:
        candidate["score"] = round_float(candidate["score"])
    return ordered


def resolve_proxy_family(entry: dict, resolved_role: str, recipe: dict) -> str:
    proxy_overrides = recipe.get("proxy_overrides", {})
    if entry["name"] in proxy_overrides:
        return proxy_overrides[entry["name"]]

    proxy_hint = entry.get("proxy_hint", "")
    if proxy_hint:
        return proxy_hint

    return DEFAULT_PROXY_BY_ROLE.get(resolved_role, "")


def semantic_weight_for_role(role: str, recipe: dict) -> float:
    weights = dict(DEFAULT_SEMANTIC_WEIGHTS)
    weights.update(recipe.get("semantic_weights", {}))
    return weights.get(role, 0.18)


def stage_scores_for_object(entry: dict, resolved_role: str, proxy_family: str, max_visible_area: float, recipe: dict) -> dict:
    thresholds = dict(DEFAULT_STAGE_THRESHOLDS)
    thresholds.update(recipe.get("stage_thresholds", {}))
    visible_area = 0.0
    if entry.get("camera_view") is not None:
        visible_area = float(entry["camera_view"].get("visible_area", 0.0))
    visible_norm = visible_area / max_visible_area if max_visible_area > 0.0 else 0.0
    semantic_weight = semantic_weight_for_role(resolved_role, recipe)
    texture_hint = first_texture_name(entry.get("materials", []))
    hero_roles = {"painting", "bookshelf", "sideboard", "chair", "instrument", "lamp_fixture"}
    importance = clamp(visible_norm * 0.65 + semantic_weight * 0.35)

    a_score = 0.0
    if resolved_role in {"room_shell", "window_light", "ceiling_light"}:
        a_score = 1.0
    elif entry["type"] == "LIGHT":
        a_score = 0.72

    b_score = clamp(visible_norm * 0.72 + semantic_weight * 0.28 + (0.08 if entry["type"] == "LIGHT" else 0.0))
    c_score = clamp(importance * 0.58 + (0.22 if proxy_family else 0.0) + (0.12 if resolved_role in hero_roles else 0.0))
    d_score = clamp(importance * 0.52 + (0.26 if resolved_role in hero_roles else 0.0) + (0.12 if texture_hint else 0.0))
    e_score = 1.0 if texture_hint else 0.0

    return {
        "importance": round_float(importance),
        "visible_area": round_float(visible_area, 6),
        "visible_norm": round_float(visible_norm),
        "A": {"score": round_float(a_score), "eligible": a_score >= 0.70},
        "B": {"score": round_float(b_score), "eligible": b_score >= thresholds["B"]},
        "C": {"score": round_float(c_score), "eligible": c_score >= thresholds["C"]},
        "D": {"score": round_float(d_score), "eligible": d_score >= thresholds["D"]},
        "E": {"score": round_float(e_score), "eligible": bool(texture_hint)},
    }


def selector_match(entry: dict, selector: dict) -> bool:
    names = selector.get("names", [])
    if names and entry["name"] in names:
        return True

    roles = selector.get("roles", [])
    if roles and entry.get("resolved_role") in roles:
        return True

    name_prefixes = selector.get("name_prefixes", [])
    if name_prefixes and any(entry["name"].startswith(prefix) for prefix in name_prefixes):
        return True

    types = selector.get("types", [])
    if types and entry["type"] in types:
        return True

    roots = selector.get("name_roots", [])
    if roots and strip_numeric_suffix(entry["name"]) in roots:
        return True

    return False


def materialize_groups(enriched_objects: list[dict], recipe: dict) -> list[dict]:
    objects_by_name = {entry["name"]: entry for entry in enriched_objects}
    groups = []

    for group_def in recipe.get("groups", []):
        member_names = []
        for name in group_def.get("members", []):
            if name in objects_by_name and name not in member_names:
                member_names.append(name)

        selector = group_def.get("selector", {})
        if selector:
            for entry in enriched_objects:
                if selector_match(entry, selector) and entry["name"] not in member_names:
                    member_names.append(entry["name"])

        members = [objects_by_name[name] for name in member_names if name in objects_by_name]
        if not members:
            continue

        bounds = merge_bounds(members)
        stage_intro = group_def.get("stage", "C")
        stage_mask = stage_mask_from_intro(stage_intro)
        light_names = [member["name"] for member in members if member["type"] == "LIGHT"]
        mesh_names = [member["name"] for member in members if member["type"] == "MESH"]
        role = group_def.get("role")
        if not role:
            role_counts = {}
            for member in members:
                resolved_role = member.get("resolved_role", "")
                role_counts[resolved_role] = role_counts.get(resolved_role, 0) + 1
            role = sorted(role_counts.items(), key=lambda item: (-item[1], item[0]))[0][0]

        groups.append(
            {
                "id": group_def["id"],
                "label": group_def.get("label", group_def["id"]),
                "role": role,
                "proxy_family": group_def.get("proxy_family", ""),
                "stage_intro": stage_intro,
                "stage_mask": stage_mask,
                "member_names": member_names,
                "light_names": light_names,
                "mesh_names": mesh_names,
                "bounds": bounds,
                "notes": group_def.get("notes", ""),
            }
        )

    return groups


def build_name_root_groups(enriched_objects: list[dict]) -> list[dict]:
    grouped = {}
    for entry in enriched_objects:
        root = strip_numeric_suffix(entry["name"])
        grouped.setdefault(root, []).append(entry)

    results = []
    for root, members in sorted(grouped.items()):
        if len(members) < 2:
            continue
        bounds = merge_bounds(members)
        results.append(
            {
                "id": f"name_root:{root}",
                "reason": "shared_name_root",
                "member_names": [member["name"] for member in members],
                "bounds": bounds,
            }
        )
    return results


def build_light_fixture_hints(enriched_objects: list[dict], recipe_groups: list[dict]) -> list[dict]:
    group_by_member = {}
    for group in recipe_groups:
        for member_name in group["member_names"]:
            group_by_member.setdefault(member_name, []).append(group["id"])

    mesh_entries = [entry for entry in enriched_objects if entry["type"] == "MESH"]
    hints = []

    for light in [entry for entry in enriched_objects if entry["type"] == "LIGHT"]:
        center = get_world_center(light)
        candidates = []

        for mesh in mesh_entries:
            mesh_center = get_world_center(mesh)
            dist = distance(center, mesh_center)
            if dist > 1.25:
                continue

            role_bonus = 0.0
            if mesh.get("resolved_role") in {"lamp_fixture", "lamp", "table", "painting"}:
                role_bonus = 0.18

            score = clamp(1.0 - dist / 1.25 + role_bonus)
            if score < 0.20:
                continue

            candidates.append(
                {
                    "object": mesh["name"],
                    "role": mesh.get("resolved_role", ""),
                    "distance": round_float(dist),
                    "score": round_float(score),
                    "recipe_groups": group_by_member.get(mesh["name"], []),
                }
            )

        candidates.sort(key=lambda candidate: (-candidate["score"], candidate["distance"], candidate["object"]))
        hints.append(
            {
                "light": light["name"],
                "role": light.get("resolved_role", ""),
                "candidates": candidates[:5],
                "recipe_groups": group_by_member.get(light["name"], []),
            }
        )

    return hints


def build_proximity_group_hints(enriched_objects: list[dict]) -> list[dict]:
    root_groups = {}
    for entry in enriched_objects:
        root = strip_numeric_suffix(entry["name"])
        root_groups.setdefault(root, []).append(entry)

    root_centers = {}
    for root, members in root_groups.items():
        bounds = merge_bounds(members)
        if bounds is None:
            continue
        root_centers[root] = bounds["center"]

    roots = sorted(root_centers)
    hints = []
    for left_index, left_root in enumerate(roots):
        left_center = root_centers[left_root]
        for right_root in roots[left_index + 1 :]:
            right_center = root_centers[right_root]
            dist = distance(left_center, right_center)
            if dist > 0.45:
                continue

            member_names = [entry["name"] for entry in root_groups[left_root] + root_groups[right_root]]
            bounds = merge_bounds(root_groups[left_root] + root_groups[right_root])
            hints.append(
                {
                    "id": f"proximity:{left_root}+{right_root}",
                    "reason": "proximity_cluster",
                    "distance": round_float(dist),
                    "member_names": member_names,
                    "bounds": bounds,
                }
            )

    hints.sort(key=lambda hint: (hint["distance"], hint["id"]))
    return hints


def build_stage_views(recipe_groups: list[dict]) -> dict:
    stage_views = {}
    for stage in STAGE_ORDER:
        stage_views[stage] = {
            "groups": [group for group in recipe_groups if group["stage_mask"] & STAGE_BITS[stage]],
        }
    return stage_views


def enrich_objects(raw: dict, recipe: dict) -> list[dict]:
    objects = raw.get("objects", [])
    max_visible_area = 0.0
    for entry in objects:
        camera_view = entry.get("camera_view")
        if camera_view is not None:
            max_visible_area = max(max_visible_area, float(camera_view.get("visible_area", 0.0)))

    enriched = []
    for entry in objects:
        candidate_list = role_candidates_for_object(entry, recipe)
        resolved_role = candidate_list[0]["role"] if candidate_list else entry.get("role_guess", "")
        proxy_family = resolve_proxy_family(entry, resolved_role, recipe)
        stage_scores = stage_scores_for_object(entry, resolved_role, proxy_family, max_visible_area, recipe)
        texture_hint = first_texture_name(entry.get("materials", []))

        enriched.append(
            {
                **entry,
                "role_candidates": candidate_list,
                "resolved_role": resolved_role,
                "resolved_role_confidence": candidate_list[0]["score"] if candidate_list else 0.0,
                "proxy_family": proxy_family,
                "texture_hint": texture_hint,
                "stage_scores": stage_scores,
                "primary_group_id": "",
            }
        )

    return enriched


def assign_groups_to_objects(enriched_objects: list[dict], recipe_groups: list[dict]) -> None:
    memberships = {entry["name"]: [] for entry in enriched_objects}
    for group in recipe_groups:
        for member_name in group["member_names"]:
            memberships.setdefault(member_name, []).append(group["id"])

    for entry in enriched_objects:
        group_ids = memberships.get(entry["name"], [])
        entry["group_ids"] = group_ids
        entry["primary_group_id"] = group_ids[0] if group_ids else ""


def build_processed_data(raw: dict, recipe: dict, recipe_path: Path) -> dict:
    enriched_objects = enrich_objects(raw, recipe)
    recipe_groups = materialize_groups(enriched_objects, recipe)
    assign_groups_to_objects(enriched_objects, recipe_groups)
    light_hints = build_light_fixture_hints(enriched_objects, recipe_groups)
    heuristic_groups = build_name_root_groups(enriched_objects)
    heuristic_groups.extend(build_proximity_group_hints(enriched_objects))
    stage_views = build_stage_views(recipe_groups)

    return {
        "version": 2,
        "scene": raw.get("scene", {}),
        "camera": raw.get("camera"),
        "world": raw.get("world", {}),
        "recipe": {
            "path": recipe_path.name,
            "group_count": len(recipe_groups),
        },
        "lights": [entry for entry in enriched_objects if entry["type"] == "LIGHT"],
        "objects": enriched_objects,
        "groups": recipe_groups,
        "heuristic_groups": heuristic_groups,
        "light_fixture_hints": light_hints,
        "stage_views": stage_views,
        "legacy": {
            "hero_meshes": raw.get("hero_meshes", []),
            "named_assets": raw.get("named_assets", []),
        },
    }


def write_json(path: Path, data: dict) -> None:
    path.write_text(json.dumps(data, indent=2), encoding="utf-8")


def write_summary_markdown(path: Path, processed: dict) -> None:
    lines = []
    lines.append("# Blue Wall Scene Extract v2")
    lines.append("")
    lines.append("Generated from `blue_wall.blend` through Blender plus host-side recipe processing.")
    lines.append("")
    lines.append("## Scene Summary")
    lines.append("")
    scene = processed["scene"]
    lines.append(f"- Scene: `{scene['name']}`")
    lines.append(f"- Objects: `{scene['object_count']}` total, `{scene['mesh_count']}` meshes")
    lines.append(f"- Render resolution: `{scene['render_resolution'][0]} x {scene['render_resolution'][1]}`")
    lines.append(f"- Recipe groups: `{processed['recipe']['group_count']}`")
    lines.append("")
    lines.append("## Stage Groups")
    lines.append("")
    lines.append("| Group | Stage | Proxy | Members | Bounds |")
    lines.append("| --- | --- | --- | ---: | --- |")
    for group in processed["groups"]:
        bounds = group["bounds"]["dimensions"] if group["bounds"] is not None else [0.0, 0.0, 0.0]
        lines.append(
            f"| `{group['label']}` | `{group['stage_intro']}` | `{group['proxy_family']}` | `{len(group['member_names'])}` | `{bounds}` |"
        )
    lines.append("")
    lines.append("## Light To Fixture Hints")
    lines.append("")
    for hint in processed["light_fixture_hints"]:
        lines.append(f"### `{hint['light']}`")
        lines.append("")
        if not hint["candidates"]:
            lines.append("- No nearby fixture candidates.")
        else:
            for candidate in hint["candidates"][:3]:
                lines.append(
                    f"- `{candidate['object']}` role `{candidate['role']}` distance `{candidate['distance']}` score `{candidate['score']}`"
                )
        lines.append("")
    lines.append("## Heuristic Group Candidates")
    lines.append("")
    lines.append("| Candidate | Reason | Members |")
    lines.append("| --- | --- | ---: |")
    for group in processed["heuristic_groups"][:16]:
        lines.append(f"| `{group['id']}` | `{group['reason']}` | `{len(group['member_names'])}` |")
    lines.append("")
    lines.append("## Top Stage C Targets")
    lines.append("")
    lines.append("| Object | Role | Proxy | Stage C Score | Texture |")
    lines.append("| --- | --- | --- | ---: | --- |")
    candidates = [
        entry
        for entry in processed["objects"]
        if entry["type"] == "MESH"
        and entry["stage_scores"]["C"]["eligible"]
        and entry["resolved_role"] not in {"mesh", "room_shell", "books"}
    ]
    candidates.sort(
        key=lambda entry: (
            -entry["stage_scores"]["C"]["score"],
            -entry["stage_scores"]["importance"],
            entry["name"],
        )
    )
    for entry in candidates[:16]:
        lines.append(
            f"| `{entry['name']}` | `{entry['resolved_role']}` | `{entry['proxy_family']}` | `{entry['stage_scores']['C']['score']:.4f}` | `{entry['texture_hint'] or '-'}` |"
        )
    lines.append("")
    lines.append("## Output Notes")
    lines.append("")
    lines.append("- `scene_extract_raw.json` is the direct Blender dump.")
    lines.append("- `scene_extract.json` is the processed reconstruction-oriented dataset.")
    lines.append("- `scene_extract_generated.h`, `scene_groups.h`, and `scene_stage_*.h` are generated C-facing outputs for future shader staging work.")
    lines.append("- `scene_extract.h` is preserved as a compatibility header for the Blue Wall POC while helper extraction remains local to this folder.")
    lines.append("")
    path.write_text("\n".join(lines), encoding="utf-8")


def write_legacy_markdown(path: Path, processed: dict) -> None:
    lines = []
    lines.append("# Blue Wall Scene Extract")
    lines.append("")
    lines.append("This compatibility summary now points at the v2 extraction pipeline.")
    lines.append("")
    lines.append("Primary artifacts:")
    lines.append("")
    lines.append("- `scene_extract_raw.json`")
    lines.append("- `scene_extract.json`")
    lines.append("- `scene_extract_summary.md`")
    lines.append("- `scene_extract_generated.h`")
    lines.append("")
    lines.append("The full v2 human-readable report is in `scene_extract_summary.md`.")
    lines.append("")
    path.write_text("\n".join(lines), encoding="utf-8")


def write_legacy_header(path: Path, processed: dict) -> None:
    camera = processed["camera"]
    lights = processed["lights"]
    named_assets = processed["legacy"]["named_assets"]
    environment_images = [Path(image).name for image in processed["world"].get("images", [])]
    lines = []
    lines.append("/* Generated from blue_wall.blend through Blender Python. */")
    lines.append("#pragma once")
    lines.append("")
    lines.append("typedef struct blue_wall_extract_camera_t {")
    lines.append("    const char *name;")
    lines.append("    float location[3];")
    lines.append("    float rotation_euler[3];")
    lines.append("    float lens_mm;")
    lines.append("    float angle_x;")
    lines.append("    float angle_y;")
    lines.append("} blue_wall_extract_camera_t;")
    lines.append("")
    lines.append("typedef struct blue_wall_extract_light_t {")
    lines.append("    const char *name;")
    lines.append("    const char *light_type;")
    lines.append("    float location[3];")
    lines.append("    float color[3];")
    lines.append("    float energy;")
    lines.append("    float size_x;")
    lines.append("    float size_y;")
    lines.append("} blue_wall_extract_light_t;")
    lines.append("")
    lines.append("typedef struct blue_wall_extract_proxy_t {")
    lines.append("    const char *name;")
    lines.append("    const char *role;")
    lines.append("    const char *proxy_hint;")
    lines.append("    float world_center[3];")
    lines.append("    float camera_center[3];")
    lines.append("    float dimensions[3];")
    lines.append("    float proxy_color[3];")
    lines.append("    float visible_area;")
    lines.append("    const char *texture_hint;")
    lines.append("} blue_wall_extract_proxy_t;")
    lines.append("")
    if camera is None:
        lines.append("static const blue_wall_extract_camera_t g_blue_wall_camera = { 0 };")
    else:
        lines.append("static const blue_wall_extract_camera_t g_blue_wall_camera = {")
        lines.append(f"    {c_string(camera['name'])},")
        lines.append(f"    {c_vec3(camera['location'])},")
        lines.append(f"    {c_vec3(camera['rotation_euler'])},")
        lines.append(f"    {float(camera['lens_mm']):.4f}f,")
        lines.append(f"    {float(camera['angle_x']):.4f}f,")
        lines.append(f"    {float(camera['angle_y']):.4f}f,")
        lines.append("};")
    lines.append("")
    lines.append(f"enum {{ BLUE_WALL_ENVIRONMENT_IMAGE_COUNT = {len(environment_images)} }};")
    if environment_images:
        lines.append("static const char *const g_blue_wall_environment_images[BLUE_WALL_ENVIRONMENT_IMAGE_COUNT] = {")
        for image_name in environment_images:
            lines.append(f"    {c_string(image_name)},")
        lines.append("};")
    else:
        lines.append("static const char *const *const g_blue_wall_environment_images = 0;")
    lines.append("")
    lines.append(f"enum {{ BLUE_WALL_LIGHT_COUNT = {len(lights)} }};")
    lines.append("static const blue_wall_extract_light_t g_blue_wall_lights[BLUE_WALL_LIGHT_COUNT] = {")
    for light in lights:
        info = light.get("light", {})
        lines.append("    {")
        lines.append(f"        {c_string(light['name'])},")
        lines.append(f"        {c_string(info.get('light_type', ''))},")
        lines.append(f"        {c_vec3(light['location'])},")
        lines.append(f"        {c_vec3(info.get('color', [1.0, 1.0, 1.0]))},")
        lines.append(f"        {float(info.get('energy', 0.0)):.4f}f,")
        lines.append(f"        {float(info.get('size', 0.0)):.4f}f,")
        lines.append(f"        {float(info.get('size_y', 0.0)):.4f}f,")
        lines.append("    },")
    lines.append("};")
    lines.append("")
    lines.append(f"enum {{ BLUE_WALL_PROXY_COUNT = {len(named_assets)} }};")
    lines.append("static const blue_wall_extract_proxy_t g_blue_wall_proxies[BLUE_WALL_PROXY_COUNT] = {")
    for entry in named_assets:
        camera_space = entry.get("camera_space_bounds") or {}
        camera_center = camera_space.get("center") or [0.0, 0.0, 0.0]
        visible_area = float(entry.get("camera_view", {}).get("visible_area", 0.0))
        lines.append("    {")
        lines.append(f"        {c_string(entry['name'])},")
        lines.append(f"        {c_string(entry['role_guess'])},")
        lines.append(f"        {c_string(entry['proxy_hint'])},")
        lines.append(f"        {c_vec3(entry['world_bounds']['center'])},")
        lines.append(f"        {c_vec3(camera_center)},")
        lines.append(f"        {c_vec3(entry['dimensions'])},")
        lines.append(f"        {c_vec3(first_proxy_color(entry.get('materials', [])))},")
        lines.append(f"        {visible_area:.6f}f,")
        lines.append(f"        {c_string(first_texture_name(entry.get('materials', [])))},")
        lines.append("    },")
    lines.append("};")
    lines.append("")
    path.write_text("\n".join(lines), encoding="utf-8")


def write_generated_header(path: Path, processed: dict) -> None:
    objects = processed["objects"]
    groups = processed["groups"]
    group_members = []
    for group in groups:
        group["member_start"] = len(group_members)
        group["member_count"] = len(group["member_names"])
        group_members.extend(group["member_names"])

    lines = []
    lines.append("/* Generated from scene_extract_raw.json and blue_wall_recipe.json. */")
    lines.append("#pragma once")
    lines.append("")
    for stage in STAGE_ORDER:
        lines.append(f"#define BLUE_WALL_STAGE_{stage}_BIT {c_mask(STAGE_BITS[stage])}")
    lines.append("")
    lines.append("typedef struct blue_wall_generated_object_t {")
    lines.append("    const char *name;")
    lines.append("    const char *stable_id;")
    lines.append("    const char *type;")
    lines.append("    const char *role;")
    lines.append("    const char *proxy_family;")
    lines.append("    const char *group_id;")
    lines.append("    unsigned int stage_mask;")
    lines.append("    float world_center[3];")
    lines.append("    /* Local extents in basis_x/y/z order, not remapped world axes. */")
    lines.append("    float dimensions[3];")
    lines.append("    float basis_x[3];")
    lines.append("    float basis_y[3];")
    lines.append("    float basis_z[3];")
    lines.append("    float visible_area;")
    lines.append("    const char *texture_hint;")
    lines.append("} blue_wall_generated_object_t;")
    lines.append("")
    lines.append("typedef struct blue_wall_generated_group_t {")
    lines.append("    const char *id;")
    lines.append("    const char *label;")
    lines.append("    const char *role;")
    lines.append("    const char *proxy_family;")
    lines.append("    const char *intro_stage;")
    lines.append("    unsigned int stage_mask;")
    lines.append("    float center[3];")
    lines.append("    float dimensions[3];")
    lines.append("    int member_start;")
    lines.append("    int member_count;")
    lines.append("    int light_count;")
    lines.append("} blue_wall_generated_group_t;")
    lines.append("")
    lines.append(f"enum {{ BLUE_WALL_GENERATED_OBJECT_COUNT = {len(objects)} }};")
    lines.append("static const blue_wall_generated_object_t g_blue_wall_generated_objects[BLUE_WALL_GENERATED_OBJECT_COUNT] = {")
    for entry in objects:
        obb = entry.get("oriented_bounds", {})
        axes = obb.get("axes", {})
        world_center = get_world_center(entry)
        dimensions = get_dimensions(entry)
        visible_area = float(entry.get("camera_view", {}).get("visible_area", 0.0))
        stage_mask = 0
        for stage in STAGE_ORDER:
            if entry["stage_scores"].get(stage, {}).get("eligible"):
                stage_mask |= STAGE_BITS[stage]
        lines.append("    {")
        lines.append(f"        {c_string(entry['name'])},")
        lines.append(f"        {c_string(entry['stable_id'])},")
        lines.append(f"        {c_string(entry['type'])},")
        lines.append(f"        {c_string(entry['resolved_role'])},")
        lines.append(f"        {c_string(entry['proxy_family'])},")
        lines.append(f"        {c_string(entry.get('primary_group_id', ''))},")
        lines.append(f"        {c_mask(stage_mask)},")
        lines.append(f"        {c_vec3(world_center)},")
        lines.append(f"        {c_vec3(dimensions)},")
        lines.append(f"        {c_vec3(axes.get('x', [1.0, 0.0, 0.0]))},")
        lines.append(f"        {c_vec3(axes.get('y', [0.0, 1.0, 0.0]))},")
        lines.append(f"        {c_vec3(axes.get('z', [0.0, 0.0, 1.0]))},")
        lines.append(f"        {visible_area:.6f}f,")
        lines.append(f"        {c_string(entry.get('texture_hint', ''))},")
        lines.append("    },")
    lines.append("};")
    lines.append("")
    lines.append(f"enum {{ BLUE_WALL_GENERATED_GROUP_MEMBER_COUNT = {len(group_members)} }};")
    lines.append("static const char *const g_blue_wall_generated_group_members[BLUE_WALL_GENERATED_GROUP_MEMBER_COUNT] = {")
    for member_name in group_members:
        lines.append(f"    {c_string(member_name)},")
    lines.append("};")
    lines.append("")
    lines.append(f"enum {{ BLUE_WALL_GENERATED_GROUP_COUNT = {len(groups)} }};")
    lines.append("static const blue_wall_generated_group_t g_blue_wall_generated_groups[BLUE_WALL_GENERATED_GROUP_COUNT] = {")
    for group in groups:
        bounds = group.get("bounds") or {"center": [0.0, 0.0, 0.0], "dimensions": [0.0, 0.0, 0.0]}
        lines.append("    {")
        lines.append(f"        {c_string(group['id'])},")
        lines.append(f"        {c_string(group['label'])},")
        lines.append(f"        {c_string(group['role'])},")
        lines.append(f"        {c_string(group['proxy_family'])},")
        lines.append(f"        {c_string(group['stage_intro'])},")
        lines.append(f"        {c_mask(group['stage_mask'])},")
        lines.append(f"        {c_vec3(bounds['center'])},")
        lines.append(f"        {c_vec3(bounds['dimensions'])},")
        lines.append(f"        {group['member_start']},")
        lines.append(f"        {group['member_count']},")
        lines.append(f"        {len(group['light_names'])},")
        lines.append("    },")
    lines.append("};")
    lines.append("")
    path.write_text("\n".join(lines), encoding="utf-8")


def write_groups_header(path: Path, processed: dict) -> None:
    groups = processed["groups"]
    lines = []
    lines.append("/* Generated group descriptors for Blue Wall scene reconstruction. */")
    lines.append("#pragma once")
    lines.append("")
    lines.append("typedef struct blue_wall_group_desc_t {")
    lines.append("    const char *id;")
    lines.append("    const char *label;")
    lines.append("    const char *proxy_family;")
    lines.append("    const char *intro_stage;")
    lines.append("    float center[3];")
    lines.append("    float dimensions[3];")
    lines.append("    int member_count;")
    lines.append("} blue_wall_group_desc_t;")
    lines.append("")
    lines.append(f"enum {{ BLUE_WALL_GROUP_COUNT_V2 = {len(groups)} }};")
    lines.append("static const blue_wall_group_desc_t g_blue_wall_group_descs_v2[BLUE_WALL_GROUP_COUNT_V2] = {")
    for group in groups:
        bounds = group.get("bounds") or {"center": [0.0, 0.0, 0.0], "dimensions": [0.0, 0.0, 0.0]}
        lines.append("    {")
        lines.append(f"        {c_string(group['id'])},")
        lines.append(f"        {c_string(group['label'])},")
        lines.append(f"        {c_string(group['proxy_family'])},")
        lines.append(f"        {c_string(group['stage_intro'])},")
        lines.append(f"        {c_vec3(bounds['center'])},")
        lines.append(f"        {c_vec3(bounds['dimensions'])},")
        lines.append(f"        {len(group['member_names'])},")
        lines.append("    },")
    lines.append("};")
    lines.append("")
    path.write_text("\n".join(lines), encoding="utf-8")


def write_stage_header(path: Path, stage: str, processed: dict) -> None:
    groups = processed["stage_views"][stage]["groups"]
    lines = []
    lines.append(f"/* Generated Stage {stage} view for Blue Wall scene reconstruction. */")
    lines.append("#pragma once")
    lines.append("")
    lines.append("typedef struct blue_wall_stage_group_t {")
    lines.append("    const char *id;")
    lines.append("    const char *label;")
    lines.append("    const char *proxy_family;")
    lines.append("    float center[3];")
    lines.append("    float dimensions[3];")
    lines.append("} blue_wall_stage_group_t;")
    lines.append("")
    lines.append(f"enum {{ BLUE_WALL_STAGE_{stage}_GROUP_COUNT_V2 = {len(groups)} }};")
    lines.append(f"static const blue_wall_stage_group_t g_blue_wall_stage_{stage}_groups_v2[BLUE_WALL_STAGE_{stage}_GROUP_COUNT_V2] = {{")
    for group in groups:
        bounds = group.get("bounds") or {"center": [0.0, 0.0, 0.0], "dimensions": [0.0, 0.0, 0.0]}
        lines.append("    {")
        lines.append(f"        {c_string(group['id'])},")
        lines.append(f"        {c_string(group['label'])},")
        lines.append(f"        {c_string(group['proxy_family'])},")
        lines.append(f"        {c_vec3(bounds['center'])},")
        lines.append(f"        {c_vec3(bounds['dimensions'])},")
        lines.append("    },")
    lines.append("};")
    lines.append("")
    path.write_text("\n".join(lines), encoding="utf-8")


def postprocess(
    raw_json_path: Path,
    processed_json_path: Path,
    summary_md_path: Path,
    generated_header_path: Path,
    groups_header_path: Path,
    stage_a_header_path: Path,
    stage_b_header_path: Path,
    stage_c_header_path: Path,
    stage_d_header_path: Path,
    stage_e_header_path: Path,
    legacy_md_path: Path,
    legacy_header_path: Path,
    recipe_path: Path,
) -> None:
    raw = json.loads(raw_json_path.read_text(encoding="utf-8"))
    normalize_raw_paths(raw, recipe_path.parent)
    recipe = load_recipe(recipe_path)
    processed = build_processed_data(raw, recipe, recipe_path)
    write_json(processed_json_path, processed)
    write_summary_markdown(summary_md_path, processed)
    write_generated_header(generated_header_path, processed)
    write_groups_header(groups_header_path, processed)
    write_stage_header(stage_a_header_path, "A", processed)
    write_stage_header(stage_b_header_path, "B", processed)
    write_stage_header(stage_c_header_path, "C", processed)
    write_stage_header(stage_d_header_path, "D", processed)
    write_stage_header(stage_e_header_path, "E", processed)
    write_legacy_markdown(legacy_md_path, processed)
    write_legacy_header(legacy_header_path, processed)
