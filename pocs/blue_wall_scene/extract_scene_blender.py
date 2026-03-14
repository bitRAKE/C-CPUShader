import argparse
import hashlib
import json
import sys
from pathlib import Path

import bpy
from bpy_extras.object_utils import world_to_camera_view
from mathutils import Vector


ROLE_PATTERNS = [
    ("room_shell", ("Room", "Cornice", "Skirting")),
    ("sideboard", ("Sideboard",)),
    ("bookshelf", ("VerticalBookShelf",)),
    ("books", ("Books_proxy",)),
    ("painting", ("CanvasPainting",)),
    ("chair", ("GreenChair",)),
    ("instrument", ("Ukulele",)),
    ("lamp", ("Lantern", "Ceiling", "Window")),
    ("table", ("side_table_tall", "WoodenTable")),
    ("plant", ("potted_plant",)),
    ("clock", ("mantel_clock",)),
    ("camera_prop", ("Camera_01",)),
    ("statue", ("horse_statue",)),
    ("box_prop", ("CheeseBox",)),
]

IMAGE_AVERAGE_CACHE = {}
MATERIAL_CACHE = {}


def round_float(value, digits=4):
    return round(float(value), digits)


def round_vec(value, digits=4):
    return [round_float(component, digits) for component in value]


def parse_args():
    argv = sys.argv[sys.argv.index("--") + 1 :] if "--" in sys.argv else []
    parser = argparse.ArgumentParser(description="Extract raw scene metrics from the Blue Wall .blend file.")
    parser.add_argument("--raw-json-out", required=True)
    return parser.parse_args(argv)


def detect_role(name):
    for role, patterns in ROLE_PATTERNS:
        if any(pattern in name for pattern in patterns):
            return role
    return "mesh"


def proxy_hint(obj, role):
    dimensions = [float(value) for value in obj.dimensions]
    sorted_dims = sorted(dimensions)
    thin_axis_ratio = (sorted_dims[0] / sorted_dims[-1]) if sorted_dims[-1] > 0.0001 else 1.0

    if role == "room_shell":
        return "planes_and_trim"
    if role == "painting":
        return "framed_textured_quad"
    if role == "sideboard":
        return "box_stack"
    if role == "bookshelf":
        return "box_frame"
    if role == "books":
        return "book_box_cluster"
    if role == "chair":
        return "chair_boxes"
    if role == "instrument":
        return "capsule_plus_boxes"
    if role == "lamp":
        return "cylinder_with_emissive_core"
    if role == "table":
        return "cylinder_table"
    if role == "plant":
        return "pot_plus_foliage_cluster"
    if role in {"clock", "camera_prop", "statue", "box_prop"}:
        return "hero_prop_proxy"
    if thin_axis_ratio < 0.12:
        return "thin_box_or_plane"
    return "box"


def stable_id(obj):
    parent_name = obj.parent.name if obj.parent else ""
    seed = f"{obj.type}\0{obj.name}\0{parent_name}".encode("utf-8")
    return hashlib.sha1(seed).hexdigest()[:16]


def parent_chain(obj):
    chain = []
    current = obj.parent
    while current is not None:
        chain.append(current.name)
        current = current.parent
    return chain


def world_bounds(obj):
    corners = [obj.matrix_world @ Vector(corner) for corner in obj.bound_box]
    minimum = Vector((min(corner.x for corner in corners), min(corner.y for corner in corners), min(corner.z for corner in corners)))
    maximum = Vector((max(corner.x for corner in corners), max(corner.y for corner in corners), max(corner.z for corner in corners)))
    return corners, minimum, maximum


def oriented_bounds(obj):
    basis = obj.matrix_world.to_3x3()
    axis_x = basis.col[0]
    axis_y = basis.col[1]
    axis_z = basis.col[2]
    local_center = Vector((0.0, 0.0, 0.0))
    for corner in obj.bound_box:
        local_center += Vector(corner)
    local_center /= max(1, len(obj.bound_box))
    world_center = obj.matrix_world @ local_center
    return {
        "center": round_vec(world_center),
        "half_extent": [round_float(float(value) * 0.5) for value in obj.dimensions],
        "axes": {
            "x": round_vec(axis_x.normalized()) if axis_x.length > 0.000001 else [1.0, 0.0, 0.0],
            "y": round_vec(axis_y.normalized()) if axis_y.length > 0.000001 else [0.0, 1.0, 0.0],
            "z": round_vec(axis_z.normalized()) if axis_z.length > 0.000001 else [0.0, 0.0, 1.0],
        },
        "axis_scale": [round_float(axis_x.length), round_float(axis_y.length), round_float(axis_z.length)],
    }


def camera_bounds(scene, camera_obj, corners):
    if camera_obj is None:
        return None

    projected = [world_to_camera_view(scene, camera_obj, corner) for corner in corners]
    x_values = [point.x for point in projected]
    y_values = [point.y for point in projected]
    z_values = [point.z for point in projected]
    visible_x0 = max(0.0, min(x_values))
    visible_x1 = min(1.0, max(x_values))
    visible_y0 = max(0.0, min(y_values))
    visible_y1 = min(1.0, max(y_values))
    visible_area = 0.0
    if visible_x1 > visible_x0 and visible_y1 > visible_y0:
        visible_area = (visible_x1 - visible_x0) * (visible_y1 - visible_y0)

    return {
        "ndc_min": [round_float(min(x_values)), round_float(min(y_values)), round_float(min(z_values))],
        "ndc_max": [round_float(max(x_values)), round_float(max(y_values)), round_float(max(z_values))],
        "visible_area": round_float(visible_area, 6),
        "in_front": any(value > 0.0 for value in z_values),
    }


def camera_space_bounds(camera_obj, corners):
    if camera_obj is None:
        return None

    matrix = camera_obj.matrix_world.inverted()
    points = [matrix @ corner for corner in corners]
    minimum = Vector((min(point.x for point in points), min(point.y for point in points), min(point.z for point in points)))
    maximum = Vector((max(point.x for point in points), max(point.y for point in points), max(point.z for point in points)))
    return {
        "min": round_vec(minimum),
        "max": round_vec(maximum),
        "center": round_vec((minimum + maximum) * 0.5),
    }


def resolve_image_path(image):
    try:
        resolved = Path(bpy.path.abspath(image.filepath)).resolve()
        root = Path(__file__).resolve().parent
        try:
            return resolved.relative_to(root).as_posix()
        except ValueError:
            return resolved.as_posix()
    except Exception:
        return image.filepath


def estimate_image_average(image, max_samples=64):
    cache_key = resolve_image_path(image) if image is not None else None
    if cache_key in IMAGE_AVERAGE_CACHE:
        return IMAGE_AVERAGE_CACHE[cache_key]

    try:
        if image is None or image.size[0] <= 0 or image.size[1] <= 0:
            return None

        pixel_count = image.size[0] * image.size[1]
        step = max(1, pixel_count // max_samples)
        total_r = 0.0
        total_g = 0.0
        total_b = 0.0
        samples = 0
        for pixel_index in range(0, pixel_count, step):
            base = pixel_index * 4
            total_r += image.pixels[base]
            total_g += image.pixels[base + 1]
            total_b += image.pixels[base + 2]
            samples += 1

        if samples <= 0:
            return None

        result = [round_float(total_r / samples, 4), round_float(total_g / samples, 4), round_float(total_b / samples, 4)]
        IMAGE_AVERAGE_CACHE[cache_key] = result
        return result
    except Exception:
        return None


def material_summary(material):
    if material.name in MATERIAL_CACHE:
        return MATERIAL_CACHE[material.name]

    summary = {
        "name": material.name,
        "uses_nodes": bool(material.node_tree),
        "base_color": None,
        "metallic": None,
        "roughness": None,
        "texture_images": [],
        "proxy_color": None,
    }

    if material.node_tree is None:
        summary["base_color"] = round_vec(material.diffuse_color[:3])
        summary["proxy_color"] = summary["base_color"]
        MATERIAL_CACHE[material.name] = summary
        return summary

    principled = next((node for node in material.node_tree.nodes if node.type == "BSDF_PRINCIPLED"), None)
    if principled is not None:
        summary["base_color"] = round_vec(principled.inputs["Base Color"].default_value[:3])
        summary["metallic"] = round_float(principled.inputs["Metallic"].default_value)
        summary["roughness"] = round_float(principled.inputs["Roughness"].default_value)

    base_color_image = None
    for node in material.node_tree.nodes:
        image = getattr(node, "image", None)
        if node.type != "TEX_IMAGE" or image is None:
            continue

        linked_sockets = []
        for output in node.outputs:
            linked_sockets.extend(link.to_socket.name for link in output.links)

        summary["texture_images"].append(
            {
                "node": node.name,
                "path": resolve_image_path(image),
                "colorspace": image.colorspace_settings.name,
                "linked_sockets": sorted(set(linked_sockets)),
            }
        )

        if base_color_image is None and ("Base Color" in linked_sockets or "Color" in linked_sockets):
            base_color_image = image

    if base_color_image is not None:
        summary["proxy_color"] = estimate_image_average(base_color_image)
    if summary["proxy_color"] is None:
        summary["proxy_color"] = summary["base_color"]

    MATERIAL_CACHE[material.name] = summary
    return summary


def object_summary(scene, camera_obj, obj):
    corners, minimum, maximum = world_bounds(obj)
    role_guess = detect_role(obj.name)
    materials = []
    if obj.type == "MESH" and getattr(obj.data, "materials", None):
        materials = [material_summary(material) for material in obj.data.materials if material is not None]

    summary = {
        "name": obj.name,
        "stable_id": stable_id(obj),
        "type": obj.type,
        "role_guess": role_guess,
        "proxy_hint": proxy_hint(obj, role_guess),
        "collections": [collection.name for collection in obj.users_collection],
        "parent": obj.parent.name if obj.parent else None,
        "parent_chain": parent_chain(obj),
        "children": [],
        "location": round_vec(obj.location),
        "rotation_euler": round_vec(obj.rotation_euler),
        "scale": round_vec(obj.scale),
        "dimensions": round_vec(obj.dimensions),
        "world_bounds": {"min": round_vec(minimum), "max": round_vec(maximum), "center": round_vec((minimum + maximum) * 0.5)},
        "oriented_bounds": oriented_bounds(obj),
        "camera_view": camera_bounds(scene, camera_obj, corners),
        "camera_space_bounds": camera_space_bounds(camera_obj, corners),
        "materials": materials,
    }

    if obj.type == "MESH":
        summary["mesh_stats"] = {
            "vertices": len(obj.data.vertices),
            "edges": len(obj.data.edges),
            "polygons": len(obj.data.polygons),
            "material_slots": len(obj.material_slots),
        }

    if obj.type == "LIGHT":
        data = obj.data
        summary["light"] = {
            "light_type": data.type,
            "energy": round_float(data.energy),
            "color": round_vec(data.color),
            "size": round_float(getattr(data, "size", 0.0)),
            "size_y": round_float(getattr(data, "size_y", 0.0)),
            "spot_size": round_float(getattr(data, "spot_size", 0.0)),
            "spot_blend": round_float(getattr(data, "spot_blend", 0.0)),
        }

    return summary


def collect_scene_data():
    scene = bpy.context.scene
    camera_obj = scene.camera
    world = scene.world
    objects = [object_summary(scene, camera_obj, obj) for obj in scene.objects if obj.type in {"MESH", "LIGHT", "CAMERA", "EMPTY"}]
    by_name = {entry["name"]: entry for entry in objects}
    for entry in objects:
        parent_name = entry["parent"]
        if parent_name in by_name:
            by_name[parent_name]["children"].append(entry["name"])

    mesh_objects = [entry for entry in objects if entry["type"] == "MESH"]
    visible_meshes = [entry for entry in mesh_objects if entry["camera_view"] is not None and entry["camera_view"]["in_front"]]
    visible_meshes.sort(key=lambda entry: entry["camera_view"]["visible_area"], reverse=True)
    named_assets = [entry for entry in visible_meshes if entry["role_guess"] not in {"room_shell", "books", "mesh"}]

    role_counts = {}
    for entry in mesh_objects:
        role_counts[entry["role_guess"]] = role_counts.get(entry["role_guess"], 0) + 1

    data = {
        "version": 2,
        "scene": {
            "name": scene.name,
            "unit_system": scene.unit_settings.system,
            "unit_scale": round_float(scene.unit_settings.scale_length),
            "render_resolution": [scene.render.resolution_x, scene.render.resolution_y],
            "frame_current": scene.frame_current,
            "object_count": len(scene.objects),
            "mesh_count": len(mesh_objects),
            "role_counts": role_counts,
        },
        "camera": None,
        "world": {
            "uses_nodes": bool(world.node_tree) if world else False,
            "node_types": [node.type for node in world.node_tree.nodes] if world and world.node_tree else [],
            "images": [resolve_image_path(node.image) for node in world.node_tree.nodes if getattr(node, "image", None)] if world and world.node_tree else [],
        },
        "objects": objects,
        "hero_meshes": visible_meshes[:32],
        "named_assets": named_assets[:24],
    }

    if camera_obj is not None:
        data["camera"] = {
            "name": camera_obj.name,
            "location": round_vec(camera_obj.location),
            "rotation_euler": round_vec(camera_obj.rotation_euler),
            "lens_mm": round_float(camera_obj.data.lens),
            "angle_x": round_float(camera_obj.data.angle_x),
            "angle_y": round_float(camera_obj.data.angle_y),
            "clip_start": round_float(camera_obj.data.clip_start),
            "clip_end": round_float(camera_obj.data.clip_end),
        }

    return data


def main():
    args = parse_args()
    data = collect_scene_data()
    with open(args.raw_json_out, "w", encoding="utf-8") as handle:
        json.dump(data, handle, indent=2)
    print(f"Wrote {args.raw_json_out}")


if __name__ == "__main__":
    main()
