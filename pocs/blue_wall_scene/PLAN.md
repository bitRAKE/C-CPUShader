# Blue Wall POC Plan

## Intent

This plan replaces the old loose stage story with a stricter goal:

- accurately reproduce the Blender scene in analytic shader form
- leave behind a reusable method for future Blender-scene POCs

The first Blue Wall attempt under `src/shaders/blue_wall_*` has been retired. The active implementation now stays entirely under `pocs/blue_wall_scene` until any utility proves reusable enough to extract.

The Blue Wall work should now operate on two tracks at the same time:

1. Blue Wall accuracy
2. reusable scene-to-shader tooling

## Design Principles

### 1. The Blender scene is the authority

If the shader, the generated data, and the Blender file disagree, the Blender file wins.

### 2. Stages must remain honest

Each shader stage should have one job. Later-stage quality must not leak backward through shared helpers.

### 3. Generated data should carry structure, not just numbers

The extractor should not merely dump facts. It should provide enough structure to build stages intentionally.

### 4. Handwritten code should focus on proxy families and transport

The human-authored part should be:

- light transport
- proxy intersection logic
- material simplification
- final aesthetic judgement

The machine-generated part should be:

- scene facts
- groupings
- stage membership
- C data tables
- scaffolding for reconstruction

## Deliverables

### Blue Wall deliverables

- a corrected A/B/C/D sequence
- scene-specific reduction recipe
- per-stage fidelity checklist
- improved iteration notes

### Reusable deliverables

- extractor v2
- recipe-driven reduction pipeline
- code/data generation step for C
- a repeatable "new Blender scene" playbook

## Target Stage Definitions

These stage contracts should become strict.

### Stage A: Camera + Room + Lights

Contains only:

- extracted camera
- room shell
- extracted light model reduced to scene-level sources

Does not contain:

- visible furniture
- fixture geometry
- hero props

Purpose:

- prove framing
- prove scale
- prove the cool/warm light balance

### Stage B: Scene Layout Debug

Contains:

- Stage A
- generated oriented or axis-aligned layout volumes for major scene objects

Purpose:

- validate composition
- validate extracted object importance
- expose missing or misclassified assets quickly

This stage should stay ugly on purpose.

### Stage C: Generated Reconstruction Skeleton

Contains:

- Stage A lighting and shell
- stage-generated fixture and furniture proxies
- one proxy family per grouped object set

Examples:

- bookshelves
- sideboard
- chair
- tables
- lamp
- painting frame/canvas

Purpose:

- turn layout data into recognizable scene structure
- prove the reduction recipe works

### Stage D: Authored Hero Pass

Contains:

- Stage C
- selected hand-authored refinements where the generated skeleton is not enough

Examples:

- better bookshelf book rhythms
- improved chair shape
- improved lamp shade treatment
- improved sideboard-top hero props

Purpose:

- close the gap between readable reconstruction and convincing still image

### Stage E: Texture / Material Identity Pass

Contains:

- Stage D
- first high-value texture integrations

Priority:

1. `painting.jpg`
2. the chair if needed
3. selective wood or prop textures only if they materially improve identity

Purpose:

- add the smallest amount of texture support that changes scene recognition

## Workstreams

### Workstream 1: Extraction v2

Goal:

- turn the current extractor into a better raw scene data source

Required additions:

- oriented bounds or object basis data
- stable object IDs
- full parent/child relationships
- object group candidates by proximity and naming
- light-to-fixture association hints
- scene-role confidence instead of only hardcoded role naming
- stage-eligibility metrics

Useful outputs:

- `scene_extract_raw.json`
- `scene_extract_summary.md`
- `scene_extract_generated.h`

### Workstream 2: Scene Recipe Layer

Goal:

- separate generic extraction from Blue Wall-specific reduction

Add a Blue Wall-specific recipe file, for example:

- `blue_wall_recipe.json`

That recipe should define:

- object role overrides
- merge groups
- fixture definitions
- proxy family assignments
- stage inclusion rules
- importance weights
- manual exclusions

Examples:

- lamp fixture = stand + hub + four shades + four point lights + four spot lights
- painting fixture = frame + canvas + texture target
- shelf fixture = frame + shelf planes + book clusters

### Workstream 3: Code/Data Generation

Goal:

- generate reconstruction-friendly C data rather than hand-transcribing scene facts

Outputs should include:

- grouped fixtures
- stage membership tables
- generated proxy descriptors
- light groups
- optional code stubs for proxy dispatch

Candidate files:

- `scene_groups.h`
- `scene_stage_A.h`
- `scene_stage_B.h`
- `scene_stage_C.h`

The current `scene_extract.h` can remain, but it should become just one output among several.

### Workstream 4: Reconstruction Algorithms

Goal:

- codify how extracted data becomes shader-side proxies

Needed algorithms:

- object grouping by name, parent, proximity, and role
- light-to-visible-fixture pairing
- importance scoring using visible area and semantic weight
- proxy-family selection
- stage filtering
- broad-phase routing volumes

Important rule:

The routing data should be generated. The proxy math can remain handwritten.

### Workstream 5: Fidelity Loop

Goal:

- make scene accuracy measurable enough to guide decisions

Per-stage checklist:

- does the camera match the Blender shot?
- does the room read correctly before props?
- are the required hero objects present?
- are visible lights justified by visible fixtures?
- is any major object still missing or wrongly grouped?
- is a new detail improving fidelity or just adding style?

Artifacts:

- saved reference capture from Blender
- saved shader captures per stage
- short written comparison note in `ITERATIONS.md`

### Workstream 6: Future Scene Playbook

Goal:

- make the next Blender scene less ad hoc

The reusable process should be:

1. unpack the Blender scene into `pocs/{scene_name}`
2. run the generic extractor
3. inspect the summary
4. author a scene recipe
5. generate grouped/staged C data
6. implement Stage A through Stage D
7. document fidelity decisions per stage

## Immediate Blue Wall Actions

### Action 1: Reset stage ownership

Move visible fixture and furniture geometry out of shared helpers that are supposed to represent only the room/light baseline.

Result:

- the active `blue_wall_v2_A` through `blue_wall_v2_E` sequence remains truthful without reintroducing a second Blue Wall track

### Action 2: Build extraction v2 before more hero refinement

Do not keep adding handcrafted scene detail on top of the current lossy extraction layer.

Result:

- future improvements come from better structure, not more ad hoc fixes

### Action 3: Create `blue_wall_recipe.json`

Start expressing scene knowledge in data.

First recipe groups:

- room shell
- window light
- practical floor lamp
- bookshelves
- sideboard
- painting
- chair
- tables
- sideboard-top props

### Action 4: Generate stage tables

Produce generated tables for:

- Stage A lights and room
- Stage B layout volumes
- Stage C grouped proxy descriptors

### Action 5: Rebuild the active POC shader series against those tables

Only after the above should the shader stages be rewritten or cleaned up.

## Documentation Plan

The docs in this folder should become:

- `README.md`
  - overview and links
- `AUDIT.md`
  - what went wrong and why
- `PLAN.md`
  - corrected direction
- `IMPLEMENTATION.md`
  - concrete stage implementation notes
- `ITERATIONS.md`
  - dated stage-by-stage history

## Success Criteria

The plan is successful when:

- Stage A is truly baseline and visually useful
- Stage B instantly explains scene layout
- Stage C is recognizably the Blender scene without manual detective work
- Stage D feels like refinement, not rescue
- a second Blender scene could reuse the same extraction/recipe/generation flow

## Near-Term Priority Order

1. extraction v2
2. scene recipe
3. generated stage tables
4. reset Blue Wall A/B/C/D to match the generated structure
5. resume hero refinement
6. add the first texture milestone

That order is important. The current POC already proved that hand-authored refinement works. The missing proof is that the method itself can stay accurate and reusable.
