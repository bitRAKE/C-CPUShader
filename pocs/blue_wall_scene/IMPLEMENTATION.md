# Blue Wall Shader Implementation Strategy

This document captures the staged build logic for the Blue Wall POC as it exists after the Blender-driven retargeting. The first `src/shaders/blue_wall_*` attempt has been retired; the active sequence now lives entirely in `pocs/blue_wall_scene`. Use this document together with:

- `pocs/blue_wall_scene/AUDIT.md`
- `pocs/blue_wall_scene/PLAN.md`
- `pocs/blue_wall_scene/ITERATIONS.md`

## Intent

This POC should not jump straight from Blender extraction data to a “finished” hero shader. The better path is staged reduction:

1. establish the light model and room shell
2. prove composition with cheap stand-in geometry
3. replace stand-ins with role-aware analytic proxies
4. spend detail only where it materially improves the image

The extracted Blender data is not the final rendering format. It is the planning scaffold that tells the shader what matters.

## Working Rule

At every stage, prefer the cheapest representation that still improves one of:

- framing
- silhouette
- lighting
- color balance
- convergence

If a new object or shading detail does not improve one of those, it should wait.

## Data Sources

The current source artifacts serve different purposes:

- `scene_extract_raw.json`
  - direct Blender dump
  - use when you need authoritative per-object hierarchy, bounds, and material data
- `scene_extract.json`
  - processed reconstruction dataset
  - use for resolved roles, group membership, stage metrics, and light-to-fixture hints
- `scene_extract_summary.md`
  - v2 human triage report
  - use to decide what matters visually
- `scene_extract_generated.h`
  - generated object/group reconstruction tables
  - use when building stage-driven shader data
- `scene_groups.h` and `scene_stage_*.h`
  - generated staging tables
  - use when rebuilding the active A/B/C/D/E sequence against generated data
- `scene_extract.h`
  - compatibility transfer layer
  - use where the local Blue Wall helpers still expect it, until any reusable utility is extracted cleanly

The `.blend` file remains the authority. If the extraction and the scene disagree, the Blender scene wins.

## Build Order

### Stage 0: Camera + Exposure Baseline

Goal:

- match the extracted camera as closely as practical
- establish a sane output transform
- confirm the room reads at the correct scale

Implementation:

- use `g_blue_wall_camera` from `scene_extract.h`
- keep the scene almost empty
- render only the room shell and a neutral reference floor
- keep the camera static and accumulation enabled

Success looks like:

- the blue wall plane fills the shot correctly
- object insertion later will not require another full camera rewrite

### Stage 1: Light Model + Room Box

Goal:

- make the image read as the Blue Wall interior before any furniture is added

Implementation:

- build the room from planes and thin boxes
- represent the main `Window` area light explicitly
- add a cheap environment fill based on the HDRI cue
- optionally add one reduced “warm practicals” contribution from the lamp cluster rather than all local lights at once

Notes:

- this is where most of the scene mood should be established
- if the room does not read here, more objects will only hide the problem

Success looks like:

- wall/floor/side-wall relationship reads correctly
- key side lighting and warm fill feel plausible

### Stage 2: Colored Bounding Boxes

Goal:

- validate layout, depth ordering, and object importance cheaply

Implementation:

- instantiate visible hero assets from `scene_extract.h`
- use world centers and dimensions to draw axis-aligned stand-ins
- tint them with extracted proxy colors
- ignore detailed materials and most secondary lights

Recommended first set:

- both bookshelves
- sideboard
- painting
- chair
- tall side table
- small round table
- ukulele

Why this stage matters:

- it proves composition fast
- it gives a visual answer for “what is missing?”
- it provides a cheap debug mode that should remain available even after the shader gets fancier

Success looks like:

- the composition is recognizable as the source scene even though everything is still boxy

### Stage 3: Bounding Boxes Become Filters

Goal:

- convert boxes from “final geometry” into routing volumes

Implementation:

- keep the Stage 2 boxes as coarse spatial ownership regions
- each box dispatches to a role-specific object function when hit
- if the detailed function misses, fall back to the coarse box or empty space as appropriate

Pattern:

```c
if (hit_box(ray, shelf_bounds, &t)) {
    hit = intersect_bookshelf_proxy(ray, shelf_data);
}
```

This keeps object selection cheap while allowing detail to grow locally.

Success looks like:

- the shader still converges well
- complexity is concentrated inside a few important regions instead of the entire scene

### Stage 4: Role-Specific Analytic Proxies

Goal:

- replace generic boxes with reusable proxy families

Priority order:

1. `framed_textured_quad`
2. `box_frame`
3. `box_stack`
4. `chair_boxes`
5. `cylinder_table`
6. `capsule_plus_boxes`
7. `cylinder_with_emissive_core`

Examples:

- painting
  - thin frame + inset canvas plane
- bookshelf
  - outer frame + shelves + repeated book blocks
- sideboard
  - main cabinet mass + leg base + door seams
- chair
  - seat box + back box + leg cylinders/boxes
- ukulele
  - body capsule/oval proxy + neck box

Success looks like:

- silhouette improves without a major bounce-cost spike
- object families become reusable in other scene studies

### Stage 5: Light Refinement

Goal:

- move from “plausible room lighting” to “source-informed room lighting”

Implementation:

- keep the window area light as the primary contributor
- add reduced practical-light contributions for the lamp cluster
- add a ceiling fill if the image collapses without it
- keep the HDRI as a low-frequency ambient cue, not a full environment-importance-sampling project

Important rule:

- do not recreate every Blender light literally unless it materially improves the image

Success looks like:

- the shot has the cool/warm interplay of the source
- props read better without flattening the scene

### Stage 6: Material Pass

Goal:

- upgrade from flat proxy color to signature material cues

Implementation:

- blue wall gets painted-plaster variation
- wood assets share a family of warm wood shading
- chair gets darker upholstered shading
- lamp and small props get metal/dielectric distinction
- painting remains procedural until texture support arrives

Success looks like:

- the image reads as designed interior space, not colored geometry

### Stage 7: Texture Support Milestones

Goal:

- introduce texture support only where it has strong visual payoff

Priority:

1. `painting.jpg`
2. chair diffuse
3. wood diffuse for shelves / sideboard if still needed

Reason:

- the framed wall art is the cleanest high-value texture target
- it materially affects identity
- it is easy to isolate behind a single proxy type

### Stage 8: Prop Triage

Goal:

- decide which small props deserve permanent inclusion

Keep only if they materially help:

- the lamp
- the ukulele
- one or two sideboard-top props
- possibly the plant if the silhouette needs organic contrast

Likely expendable early:

- camera strap and tiny tabletop clutter
- micro-props with negligible screen area

## Documentation Cadence

This POC should be documented incrementally as the shader evolves. Each complexity increase should add a short section or dated note covering:

- what changed
- why it was worth adding
- what data from Blender drove the change
- what the cost was
- what the next simplification or refinement target is

Recommended pattern:

### Iteration Note Template

#### Iteration N: Name

- Focus:
- Data used:
- Shader changes:
- Visual improvement:
- Cost / convergence impact:
- Open problems:
- Next step:

The point is to preserve decision history, not just the final shader.

## Tooling Split

### Blender-General Tools

These should be reusable for future scene POCs:

- generic scene extractor runner
- camera extractor
- light extractor
- material summary extractor
- texture reference extractor
- world-image / HDRI extractor
- bounding box and camera-coverage metrics
- C-header export for camera/light/proxy tables

These belong to a general “Blender to analytic shader” toolkit.

### Blue Wall-Specific Tools

These encode assumptions about this one scene:

- role-name heuristics such as `VerticalBookShelf`, `CanvasPainting`, `Ukulele`
- hero-asset filtering rules
- proxy-hint mapping tuned to this asset set
- object priority ordering for this composition
- any future handcrafted reduction tables for this exact room

These should stay local to `pocs/blue_wall_scene` until they prove general enough to extract.

## Near-Term Execution Plan

The next practical shader path should be:

1. keep `blue_wall_v2_A.c` as the Stage 1 baseline: room shell + key light + warm fill
2. keep `blue_wall_v2_B.c` as the persistent Stage 2 layout debug view
3. keep `blue_wall_v2_C.c` as the first routing-volume and proxy-family stage
4. use `blue_wall_v2_D.c` as the curated hero pass: selected props, stronger materials, and no generic leftover-object sweep
5. use `blue_wall_v2_E.c` as the first shader-owned texture/material identity pass
6. stop and evaluate before extracting any reusable utility out of the POC folder

That sequence will tell us whether the extraction is genuinely useful or merely informative.

## Core Principle

The Blender scene gives us permission to be accurate about importance without being literal about geometry.

That is the entire value of this POC.
