# Blue Wall Scene POC

## Purpose

Use Poly Haven's **Blue Wall Scene File** by **Greg Zaal** as a visual target for a reduced CPU-shader scene, not as a mesh-for-mesh reproduction. The goal is to capture the composition, lighting logic, color balance, and material read with a small set of cheap analytic forms.

## Reference Choice

This is a good reference because it is a controlled interior with:

- a static, readable camera
- large architectural shapes
- a strong blue wall that makes bounce color easy to judge
- a limited number of hero objects
- lighting that reads clearly without needing a huge asset count

That makes it a better fit for a CPU shader study than a cluttered scan-heavy scene or an outdoor environment with lots of foliage and texture dependence.

## Why It Fits This CPU Shader Model

This renderer benefits most from scenes that are:

- mostly static
- composed from a few large analytic primitives
- lit by one or two explicit light sources
- evaluated by convergence, silhouette, and value structure before fine detail

Blue Wall fits that model well. The room shell can be reduced to planes and boxes, the furniture can be blocked as boxes/cylinders/rounded boxes, and only a few objects need higher attention for the image to read correctly.

## Unpacked Scene Notes

The unpacked Blender scene in `pocs/blue_wall_scene/blue_wall` is more useful than the blog image alone because it exposes the actual asset set being staged in the reference composition.

Notable unpacked assets and signals:

- `painting.jpg` confirms the wall picture is not just a color block. That is now the first real texture milestone in `blue_wall_v2_E`, loaded through shader-owned buffers instead of a per-frame host path.
- `GreenChair_01_*` confirms the right-side chair is a distinct hero asset, not just a generic seat placeholder.
- `side_table_tall_01_*` and `WoodenTable_02_*` confirm at least one of the side tables should read as a designed object rather than a plain primitive.
- `Ukulele_01_*` confirms the leaning instrument is closer to a small guitar / ukulele family prop than a generic violin silhouette.
- `Lantern_01_*`, `mantel_clock_01_*`, `Camera_01_*`, `potted_plant_04_*`, and `horse_statue_01_*` confirm the sideboard top is populated with recognizable decor, not just empty negative space.
- `hamburg_canal_1k.hdr` suggests the Blender lighting is not purely a local lamp setup; environment contribution matters.

This means the current shader POC should be judged in layers:

1. composition and staging
2. large-form lighting
3. hero-object silhouette
4. texture/material fidelity

## Current Benchmark Image

The current visual benchmark capture is:

- `pocs/blue_wall_scene/reference.jpg`

Use that image as the immediate comparison target for the v2 Blue Wall stages. It is especially useful for:

- the overall camera framing and wall balance
- the floor lamp read and its relationship to the wall light
- sideboard width and top-prop spacing
- bookshelf density and chair/table placement
- how much the painting carries the scene once texture identity is present

## Blender Extraction Workflow

The `.blend` file should be treated as the authoritative scene description for this POC. Rather than guessing from the blog image, use Blender to extract the camera, lights, hero-asset dimensions, material cues, and texture references into shader-friendly reports.

Run the extractor from the repo root with:

```powershell
python pocs\blue_wall_scene\extract_scene.py
```

Blender discovery is now:

- `%BLENDER_EXE%` if set
- otherwise `blender` / `blender.exe` from `PATH`

You can also override it explicitly:

```powershell
python pocs\blue_wall_scene\extract_scene.py --blender "C:\path\to\blender.exe"
```

That now runs in two phases:

- `pocs/blue_wall_scene/extract_scene_blender.py` inside Blender for the raw dump
- `pocs/blue_wall_scene/scene_extract_post.py` on the host side for recipe processing and code generation

Primary outputs:

- `pocs/blue_wall_scene/scene_extract_raw.json`
  - direct Blender dump with stable IDs, hierarchy, bounds, and material data
- `pocs/blue_wall_scene/scene_extract.json`
  - processed reconstruction-oriented dataset with resolved roles, groups, stage metrics, and light-to-fixture hints
- `pocs/blue_wall_scene/scene_extract_summary.md`
  - v2 human-readable summary
- `pocs/blue_wall_scene/scene_extract_generated.h`
  - generated object/group tables for future shader staging work
- `pocs/blue_wall_scene/scene_groups.h`
- `pocs/blue_wall_scene/scene_stage_A.h`
- `pocs/blue_wall_scene/scene_stage_B.h`
- `pocs/blue_wall_scene/scene_stage_C.h`
- `pocs/blue_wall_scene/scene_stage_D.h`
- `pocs/blue_wall_scene/scene_stage_E.h`
- `pocs/blue_wall_scene/scene_extract.h`
  - compatibility camera/light/proxy header used by the Blue Wall POC while utility extraction is still local to this folder
- `pocs/blue_wall_scene/blue_wall_recipe.json`
  - scene-specific reduction recipe that groups fixtures, assigns proxy families, and defines stage intent

The extractor is deliberately reduction-oriented. It does not try to preserve mesh detail. Instead, it emits the kinds of information this renderer can actually use:

- camera placement, lens, and field of view
- authored local lights plus environment image references
- named hero objects, dimensions, camera coverage, and oriented bounds
- material base color / metallic / roughness cues
- texture file references
- proxy hints such as `box_frame`, `framed_textured_quad`, `chair_boxes`, and `capsule_plus_boxes`
- group candidates by naming and proximity
- light-to-fixture association hints
- generated stage membership data for future A/B/C reconstruction passes
- generated stage membership data for future A/B/C/D/E reconstruction passes

That makes it practical to map complex Blender assets into analytic C-shader stand-ins:

- bookshelves become framed box structures plus book clusters
- the painting becomes a framed textured quad, first with a procedural placeholder and now in Stage E with the extracted `painting.jpg`
- the chair becomes a small set of seat/back/leg solids
- the ukulele becomes a capsule-plus-box silhouette proxy
- decor props can be simplified or dropped based on screen area

For the active POC shader work:

- use `scene_extract.h` as the compatibility data source where the local helpers still expect it

For the corrected reconstruction workflow:

- use `scene_extract.json` plus `blue_wall_recipe.json` as the main data sources
- use `scene_extract_summary.md` as the faster human report
- use `scene_extract_generated.h`, `scene_groups.h`, and `scene_stage_*.h` as the generated C-facing staging layer

## Implementation Strategy

The staged shader-construction plan is in `pocs/blue_wall_scene/IMPLEMENTATION.md`.

The current Blue Wall work has drifted enough that the original staged writeup should no longer be treated as the sole source of truth. Use these documents together:

- `pocs/blue_wall_scene/AUDIT.md`
- `pocs/blue_wall_scene/PLAN.md`
- `pocs/blue_wall_scene/IMPLEMENTATION.md`
- `pocs/blue_wall_scene/ITERATIONS.md`

That document covers:

- the recommended build order from room/light baseline to role-specific analytic proxies
- why colored bounding boxes should exist as an explicit debug and composition phase
- how boxes can evolve into hit-test filters for more detailed object functions
- which extraction tools are Blender-general versus Blue Wall-specific
- how to keep a running iteration record as the shader becomes more complex

The running POC notes live in `pocs/blue_wall_scene/ITERATIONS.md`.

Active shader mapping:

- `blue_wall_v2_A` = honest room/camera/light baseline rebuilt from the extraction v2 pipeline
- `blue_wall_v2_B` = generated layout-volume debug view
- `blue_wall_v2_C` = grouped reconstruction skeleton
- `blue_wall_v2_D` = authored hero refinement pass
- `blue_wall_v2_E` = Stage D plus shader-owned texture buffers, starting with the painting

Historical note:

- the first Blue Wall attempt lived under `src/shaders/blue_wall_*`
- it was useful, but drifted from the Blender target and grew parallel tech debt
- the active sequence now lives entirely in `pocs/blue_wall_scene`

For now, keep all Blue Wall-specific code, data products, and docs in this folder. If future Blender scene work proves some helpers to be genuinely generic, extract that utility later instead of maintaining two live Blue Wall tracks.

## License and Handling

- Poly Haven states that its downloadable assets are **CC0**, and the Blue Wall scene post labels this scene file as **CC0**.
- That makes the scene itself safe to use as reference material and, if needed later, as source material.
- Do **not** assume the website is all CC0. Poly Haven's license page says non-asset website content includes things like logos, thumbnails/example renders, text, and metadata.
- For this POC, keep usage conservative: use short attribution plus source links, and avoid copying preview renders or site copy into the repo.

Suggested attribution line:

`Reference scene: Blue Wall Scene File, Greg Zaal, Poly Haven.`

## Scene Reduction Strategy

1. Lock one reference view and treat it as the benchmark image.
2. Reduce the room shell first: blue wall, side walls, floor, ceiling, trim, and major openings as planes or thin boxes.
3. Reduce furniture next: tables, shelves, and cabinets as boxes or rounded boxes; legs and lamp stems as cylinders or capsules.
4. Reduce props aggressively: keep only the objects that matter to silhouette, reflection, or color contrast.
5. Reduce lighting to the minimum readable setup: one key area source or window contribution, plus only the fill needed to preserve the scene's value structure.
6. Reduce materials to signature cues: blue painted wall, light neutral surfaces, dark accents, and only a small number of reflective or refractive objects.
7. Stop when the image reads correctly at the large-form level. Do not chase tiny decor or texture detail unless the composition breaks without it.

## Texture Support Status

The first texture milestone has now landed:

- `blue_wall_v2_E` uses shader-owned buffers to load `painting.jpg` once at execute time, then sample it from the shader through the shared texture helpers

That same host/shader split should next be extended to:

- the framed artwork
- chair upholstery
- the side table surface
- the lantern, camera, clock, and brass vase
- the instrument body

The remaining best use of effort is:

- get the camera and staging right
- get the hidden/key-light balance right
- keep making the bookshelves, chair, stool, sideboard, lamp, and instrument read clearly
- broaden texture use only where it materially improves fidelity

## Suggested Next Steps

- Capture one working reference frame and note the camera/framing assumptions.
- Write down the actual prop inventory from the unpacked Blender scene before simplifying it away.
- Write a primitive inventory with approximate dimensions and material tags.
- Build a gray-box version first and check composition, contact shadows, and bounce behavior.
- Add color and key lighting before adding more props.
- Use `painting.jpg` as the pattern for future shader-owned texture assets in this POC.
- If any direct asset import is considered later, record the exact Poly Haven source URLs and download dates in this folder.

## Current Status

For now, this POC should be treated as a success checkpoint rather than an open-ended fidelity chase.

- `blue_wall_v2_E` proved the new shader-owned buffer model with a real texture-backed asset.
- `reference.jpg` gives the next Blender conversion a concrete visual benchmark.
- The next scene conversion can reuse the extraction v2 pipeline, the staged A/B/C/D/E structure, and the shared buffer/texture helpers instead of re-solving those host-side questions.

## Sources

- Blue Wall Scene File: <https://blog.polyhaven.com/blue-wall-scene-file/>
- Poly Haven License: <https://polyhaven.com/license>
