# Blue Wall POC Audit

## Scope

This audit covers the original Blue Wall proof of concept before the Blender-driven retargeting. That first attempt lived partly under `src/shaders/blue_wall_*` and partly under `pocs/blue_wall_scene`.

Those `src/shaders/blue_wall_*` files have now been retired. Keep this audit as the rationale for the current POC-only structure rather than as a description of the active code layout.

The question is not whether the Blue Wall shaders are visually interesting. They are. The question is whether the POC still does what the documentation says it does, and whether the current workflow is strong enough to support accurate Blender-scene reconstruction and future scene studies.

## Executive Summary

The POC has proven that Blender-driven scene reduction can produce compelling CPU shaders. It has also drifted away from its own staged story.

The main issue is structural: the live shader series is no longer a clean progression from extracted room shell to increasingly specific scene reconstruction. Shared helpers now contain scene-detail decisions, so early stages inherit late-stage quality. At the same time, the Blender extraction pipeline is still a reporting tool, not a reconstruction pipeline. It tells us useful facts, but it does not yet generate the right data products for faithful staged implementation.

The next phase should optimize for two goals at once:

1. reproduce the Blender scene more accurately
2. turn the Blue Wall work into a reusable Blender-to-analytic-shader method

## Primary Findings

### 1. Stage boundaries are broken

The first attempt's Stage A still read like a baseline, but its shared room helper chain had already started injecting later-stage fixture detail.

That means Stage A is no longer "room shell plus light model." It already contains a scene-specific fixture proxy. The same problem applies to later stages because they all inherit the shared room helper.

Effect:

- the series no longer teaches a clean build order
- visual gains in early stages can come from late-stage detail
- debugging gets harder because stage meaning is no longer stable

### 2. The staged shader story drifted from "reconstruction" toward "curated hero rendering"

The current A/B/C/D series is still useful, but it no longer matches the stronger goal of accurate reproduction from the Blender scene.

Examples:

- the first attempt's `blue_wall_B` was a valid box-debug stage
- the first attempt's `blue_wall_C` mixed routing volumes, handcrafted proxies, and a leftover simple-proxy sweep
- the first attempt's `blue_wall_D` was a curated quality pass, not a measured next reconstruction stage

That makes the series good for exploration, but weak as a documented reconstruction method.

### 3. The extractor is scene-aware enough to help, but too lossy to drive accurate reconstruction

The current Blender extractor provides:

- camera
- world lights
- world-axis bounds
- proxy colors
- visible area
- texture hints

That is enough to inspire a shader. It is not enough to systematically recreate one.

Important missing or weakly represented data:

- oriented bounds instead of only world AABBs
- stable basis vectors / object orientation
- grouped fixtures such as "lamp stand + hub + shades + associated point lights"
- explicit parent/cluster reductions
- per-object semantic importance beyond string-matched role names
- generated stage membership
- reduction recipes separate from extraction

This is why the floor lamp required manual detective work even though the Blender scene already contained the needed pieces.

### 4. Generic tooling and Blue Wall-specific heuristics are mixed together

`extract_scene_blender.py` currently does both of these jobs:

- generic extraction
- Blue Wall-specific naming heuristics

That is workable for one scene, but it will age poorly across multiple Blender-based POCs. The scene-specific assumptions need to move into a recipe/config layer.

### 5. The generated C header is convenient, but it is not the right abstraction boundary yet

`scene_extract.h` is good for quick shader-side experiments. It is not yet a sufficient scene package.

Current limitations:

- hero selection is baked into extraction output
- objects outside `named_assets` are harder to reach in shader code
- there is no generated grouping by fixture, furniture family, or stage
- there is no generated proxy routing data

The header is a transfer artifact, not a scene-construction artifact.

### 6. The documentation no longer matches the live shader sequence

The docs still describe a disciplined flow:

- A = shell/light
- B = boxes
- C = routing + first proxies
- D = curated refinement

That is close to the code, but no longer exact enough to trust. The lamp problem is the clearest example: a final-quality fixture landed in the earliest shared stage.

`ITERATIONS.md` is also incomplete. It does not fully record the real changes that happened after extraction and fixture refinement.

### 7. There is no formal fidelity loop against the Blender source

The POC currently lacks a repeatable method for answering:

- how close is the camera framing?
- which objects are mandatory for recognition?
- which lights are visible vs invisible but important?
- which materials are merely plausible, and which are scene-identity-critical?

Without that loop, the work tends to drift toward attractive rendering rather than faithful scene reconstruction.

## What Is Working Well

These parts should be preserved:

- the Blender scene is now treated as the authority
- the extraction workflow is already useful and fast enough to iterate with
- the A/B/C/D series did prove that staged reduction is the right conceptual model
- the room, shelf, chair, table, painting, and lamp work show that analytic proxies can carry a lot of scene identity
- the POC folder structure is already a good home for scene-specific documentation and tooling

The lamp correction is a good sign, not a setback. It proves the method works when the scene data is interrogated directly.

## Root Cause

The root problem is that the current pipeline skips a middle layer.

Right now the flow is effectively:

1. Blender scene
2. generic-ish extraction report
3. handwritten shader decisions

What is missing is:

4. a reduction recipe and generated reconstruction data

That missing layer is where the POC should decide:

- which objects belong to which stage
- which objects form one fixture or furniture group
- which proxy family each group uses
- which lights map to visible fixtures
- which data should be generated into C and which should stay manual

## Directional Recommendation

The Blue Wall POC should pivot from "shader series as evolving artwork" to "shader series as audited reconstruction pipeline."

That means:

- make stage contracts strict again
- split generic extraction from scene-specific reduction
- generate more of the stage data
- reserve hand-authored shader work for proxy families and final quality passes

## Recommended Immediate Corrections

1. Treat the first `blue_wall_A` through `blue_wall_D` set as a valuable prototype, not the final staged architecture.
2. Re-baseline the series so Stage A is truly room/camera/light only.
3. Move scene-detail fixtures out of shared room helpers unless the stage explicitly owns them.
4. Introduce a scene recipe file for Blue Wall rather than encoding scene assumptions in shader code and extractor heuristics.
5. Expand Blender outputs so they support grouping, orientation, and stage generation.
6. Document fidelity checks per stage, not just implementation ideas.

## Audit Verdict

The POC is successful as an exploration and partially successful as a method demonstration.

It is not yet disciplined enough to serve as the reference blueprint for future Blender-scene shaders.

That is a solvable problem. The next step is not to abandon the work. The next step is to formalize the missing middle layer so the shaders, the generated data, and the documentation all tell the same story.
