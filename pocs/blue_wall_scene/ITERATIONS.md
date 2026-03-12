# Blue Wall POC Iterations

This file records the meaningful stages of the Blue Wall shader study. The goal is to preserve the reasoning behind each complexity increase, not just the latest state of the shader.

## Iteration 0: Reference + Extraction Baseline

- Focus:
  - stop guessing from the blog image alone
  - treat the unpacked Blender scene as the authoritative source
- Data used:
  - `blue_wall.blend`
  - `scene_extract.json`
  - `scene_extract.md`
  - `scene_extract.h`
- Shader changes:
  - none yet in this iteration log
- Visual improvement:
  - not applicable
- Cost / convergence impact:
  - none in the runtime yet
- Open problems:
  - the first Blue Wall attempt already showed promise, but it had not yet become a disciplined Blender-driven reconstruction method
  - lighting still needed to be rebuilt from the extracted room/light model
  - object staging beyond the first proxy group still needed to become disciplined and data-driven
- Next step:
  - evaluate the early Blue Wall work against the Blender reference and decide which proxy families deserved the next upgrade

## Iteration 1: Extraction v2 + Recipe Layer

- Focus:
  - turn the extractor into a reconstruction pipeline instead of a one-shot reporting script
  - separate raw Blender facts from Blue Wall-specific reduction choices
- Data used:
  - `blue_wall.blend`
  - `blue_wall_recipe.json`
  - the new `scene_extract_raw.json` -> `scene_extract.json` host-side processing flow
- Shader changes:
  - none in the live Blue Wall shaders yet
  - this iteration is about the data pipeline and generated scaffolding
- Visual improvement:
  - indirect only
  - the lamp, grouped fixtures, and hero objects are now represented as explicit scene structure in the extracted data
- Cost / convergence impact:
  - none in the runtime yet
  - extractor cost increased slightly because it now generates more outputs
- Open problems:
  - the live A/B/C/D shaders still need to be rebuilt against the generated stage tables
  - Stage A still needs to be reset so room/light baseline does not inherit later fixture detail
- Next step:
  - use the v2 outputs to reset stage ownership and rebuild the live Blue Wall sequence honestly

## Iteration 2: Room + Light Model

- Focus:
  - begin actual shader construction against the v2 extraction outputs
  - keep the new series local to the POC while the stage contracts settle
- Data used:
  - `scene_extract.h`
  - `scene_extract_generated.h`
  - `blue_wall_recipe.json`
  - generated group/object tables from the v2 pipeline
- Shader changes:
  - added `blue_wall_v2_A.c`, `blue_wall_v2_B.c`, and `blue_wall_v2_C.c` under `pocs/blue_wall_scene`
  - added `blue_wall_v2_common.h` as the local transport, room-shell, and generated-data helper layer
  - wired the new shaders into the build/catalog without replacing the existing `src/shaders/blue_wall_*` line
- Visual improvement:
  - Stage A is now available as a room/light baseline that does not inherit the lamp fixture geometry
  - Stage B now shows generated oriented layout volumes rather than the older extracted-proxy box pass
  - Stage C starts reconstructing the room from grouped proxies, including the floor lamp cluster from generated scene data
- Cost / convergence impact:
  - the v2 series is expected to be comparable to the old Blue Wall line for A/B and a bit more expensive for C
- Open problems:
  - Stage B is still driven by filtered object volumes rather than a dedicated generated Stage B table
  - Stage C still uses several hand-authored proxy families; it is generated-driven, but not yet generated-composed
- Next step:
  - compare `blue_wall_v2_A/B/C` against the Blender reference and decide whether the next pass is stage-table refinement, floor-lamp proxy cleanup, or bringing the painting toward texture-ready handling

## Iteration 3: Stage D Hero Pass Without Host Buffers

- Focus:
  - complete the first honest Stage D in the v2 track without adding any new host-side buffer features
- Data used:
  - `scene_extract_generated.h`
  - `scene_groups.h`
  - generated Stage D membership and the `sideboard_props` object set from the recipe pipeline
- Shader changes:
  - added `blue_wall_v2_D.c` and `blue_wall_v2_D.h`
  - added local OBB-space helper routines in `blue_wall_v2_common.h` so oriented extracted objects can host smaller authored sub-shapes
  - extended the extractor workflow to emit `scene_stage_D.h`
- Visual improvement:
  - Stage D now has a real authored hero pass in the v2 line
  - the sideboard-top area can gain clock / plant / lantern / camera / statue identity without waiting for texture support
  - refined lamp, bookshelf, chair, and sideboard work stays on the shader side rather than leaking into the host
- Cost / convergence impact:
  - slightly more expensive than `blue_wall_v2_C`, but still bounded by selective prop refinement instead of a generic leftover-object sweep
- Open problems:
  - no texture support yet, so the painting and several prop materials still rely on procedural or flat stand-ins
  - some small prop orientation and silhouette choices are still heuristic rather than extracted mesh reduction
- Next step:
  - compare `blue_wall_v2_C` and `blue_wall_v2_D` visually against the Blender frame, then decide whether the next milestone is Stage E texture identity or more object-specific reduction helpers

## Iteration 4: Stage E Shader-Owned Texture Buffers

- Focus:
  - add static asset support in the shader layer without turning the host into a per-shader asset manager
  - bring the first real Blue Wall texture into the v2 track
- Data used:
  - `scene_extract_generated.h`
  - `scene_stage_E.h`
  - `blue_wall/textures/painting.jpg`
  - shared runtime helpers in `src/shader_buffers.*` and `src/u_texture.h`
- Shader changes:
  - added `blue_wall_v2_E.c` and `blue_wall_v2_E.h`
  - widened the shader uniform contract so shaders can read a named buffer set through `uniforms->buffers`
  - added reusable raw-buffer and texture-loading helpers plus shared clamp/wrap texture sampling
  - kept Stage E visually anchored to `blue_wall_v2_D` by reusing the Stage D render path and swapping the painting albedo over to a real texture sample
- Visual improvement:
  - the painting now carries real image identity instead of a procedural stand-in
  - Stage E proves that static asset-backed fidelity can be added without inflating per-frame work
- Cost / convergence impact:
  - small one-time execute cost for WIC decode and buffer creation
  - no meaningful per-frame CPU penalty beyond the texture lookups in the painting path
- Open problems:
  - only the painting uses the new buffer path so far
  - future textured props still need prioritization so the scene does not become asset-heavy without enough visual payoff
- Next step:
  - decide which remaining materials deserve real texture identity next, and which should stay analytic even in later stages

## Checkpoint: Blue Wall POC Success State

- Focus:
  - stop active Blue Wall tweaking at a useful architectural checkpoint
  - preserve the comparison target for the next Blender-scene conversion
- Data used:
  - `reference.jpg`
  - `blue_wall_v2_E`
- Outcome:
  - the POC now demonstrates the full intended arc:
    - Blender extraction v2
    - staged reconstruction shaders
    - shader-owned static buffer initialization
    - real texture sampling through shared helpers
  - the result is not a finished asset-faithful reconstruction, but it is a successful renderer and workflow proof
- Hand-off value:
  - future Blender conversions should start from the v2 extraction pipeline and the Stage E buffer model rather than the earlier ad hoc Blue Wall path
  - `reference.jpg` is the saved fidelity benchmark for judging how close the current Blue Wall result feels

## Historical Note: First Attempt

- Focus:
  - preserve what the first Blue Wall attempt taught before the Blender-driven retargeting replaced it
- Data used:
  - the original `src/shaders/blue_wall_*` series
  - early extraction and proxy hints before extraction v2 and shader-owned buffers existed
- Outcome:
  - the first attempt proved the scene could come alive in this renderer
  - it also proved that a parallel shader line outside the POC folder would accumulate tech debt quickly
- Lasting lesson:
  - keep Blender-scene work local to `pocs/{scene}` until the structure, data products, and helper APIs are mature enough to extract cleanly
