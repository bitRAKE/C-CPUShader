# Shader Guide

This project treats each shader as a pure CPU-side pixel program:

```c
vec4_t shader_main(vec2_t fragCoord, const shader_uniforms_t *uniforms);
```

The renderer calls that function once per pixel, in parallel, across multiple threads. The returned `vec4_t` is written straight into a mapped DX12 upload buffer and displayed by the GPU. There is no CPU-side RGB conversion anymore.

## Shader Contract

- `fragCoord` is the pixel coordinate for the current sample.
- `uniforms->resolution` is the full image size in pixels.
- `uniforms->time` is seconds since startup when the shader declares `SHADER_FEATURE_TIME`.
- `uniforms->frame` is the running frame counter when the shader declares `SHADER_FEATURE_FRAME`.
- `uniforms->mouse` is the shared mouse vec4 when the shader declares `SHADER_FEATURE_MOUSE`.
- `uniforms->keys` is the shared key-state bitfield when the shader declares `SHADER_FEATURE_KEYS`.
- `uniforms->buffers` is the optional named shader-buffer set prepared by the active shader's `buffers_init()` hook.
- Return color in `vec4_t`.
- Use `alpha = 1.0f` unless you have a specific reason not to.

Color-space note:

- Most shaders here return scene-linear color.
- `master_class` intentionally includes its own display transform to keep highlight rolloff inside the shader example.

Important display convention:

- `fragCoord.y = 0` is the bottom of the displayed image.
- This matches the original GDI behavior and is preserved by the DX12 presentation path.

Important threading convention:

- Shaders must be thread-safe.
- Do not mutate shared global state.
- Keep random state local, usually derived from `fragCoord`, `resolution`, and `frame`.

Important accumulation convention:

- Temporal accumulation happens outside the shader in `win.c`.
- Each shader opts into or out of accumulation through feature flags in its own header macro.
- The shader should normally produce one sample for the current frame, not average across frames itself.
- If the shader animates heavily with `time`, mark it as non-accumulating or it will trail badly.

Important buffer convention:

- Static shader assets should be prepared once through the shader metadata `buffers_init()` hook, not reloaded inside the per-pixel path.
- `buffers_init()` returns the cleanup function for the allocated buffers. Returning `NULL` means initialization failed and the shader should not execute.
- Shared helpers live in `src/shader_buffers.h`, `src/shader_buffers.c`, and `src/u_texture.h`.
- Use named lookup from `uniforms->buffers` so the shader code stays decoupled from host-global storage.

## Current Shaders

### `sphere_tracing`

Files:

- `src/shaders/sphere_tracing.c`
- `src/shaders/sphere_tracing.h`

What it is:

- A compact 3D path-tracing style shader using sphere intersections only.
- The scene acts like a simple Cornell-box-like room made from very large spheres.
- One small emissive sphere acts as the light source.

What it demonstrates:

- 3D ray/sphere intersection
- Surface normals from analytic geometry
- Diffuse vs reflective bounce choice
- Per-pixel random sampling driven by `frame`
- A static scene that accumulates nicely over time

Why it is useful as a template:

- It is the simplest full 3D lighting example in the repo.
- If you want to build a new 3D tracer, this is the easiest starting point.

Accumulation fit:

- Enabled. This shader benefits from accumulation.

### `kinetic_orbs`

Files:

- `src/shaders/kinetic_orbs.c`
- `src/shaders/kinetic_orbs.h`

What it is:

- A lightweight animated 2D glow shader with orbiting emissive forms and warped bands.
- It is deliberately time-dependent so the renderer has a clear non-accumulating test case.

What it demonstrates:

- Fast animated procedural color work
- Per-shader accumulation control
- Why temporal accumulation is the wrong default for continuously moving content

Why it is useful as a template:

- It is the simplest reference if you want a dynamic shader that should stay crisp frame to frame.
- It exercises the runtime path where accumulation is disabled.

Accumulation fit:

- Disabled. The animation is meant to read cleanly per-frame, not smear across history.

### `hsv_picker`

Files:

- `src/shaders/hsv_picker.c`
- `src/shaders/hsv_picker.h`
- `pocs/hsv_picker_tool.md`

What it is:

- An interactive HSV wheel-and-triangle picker that doubles as a reusable application component.
- It is the clearest proof that this shader library can also power tool UI, not just fullscreen studies.

What it demonstrates:

- Mouse-driven uniforms
- Shader logic split from application-owned persistent state
- Shared visual code reused by both the main renderer and a standalone GDI proof of concept

Why it is useful as a template:

- It is the best reference when a shader needs host interaction instead of passive animation.
- It shows how far a shader can travel outside the main DX12 presentation path without a rewrite.

Accumulation fit:

- Disabled. The picker is interactive and should respond immediately.

### `glass_lenses`

Files:

- `src/shaders/glass_lenses.c`
- `src/shaders/glass_lenses.h`

What it is:

- A 2D refractive lens-field shader built from separated circles.
- It uses the same improved Fresnel / Beer-Lambert transport model as `glass_disks`.
- The scene is arranged so each element still reads as an individual optic.

What it demonstrates:

- How geometry layout changes the feel of the same transport model
- Isolated lens caustics instead of a tightly coupled refractive cluster
- A cheaper way to explore dispersion and refraction behavior

Why it is useful as a template:

- It is the best starting point for lens-like optics studies.
- It keeps the lighting model coherent without forcing a dense scene layout.

Accumulation fit:

- Enabled. This shader is static and converges well over time.

### `glass_disks`

Files:

- `src/shaders/glass_disks.c`
- `src/shaders/glass_disks.h`

What it is:

- A 2D ray-optics shader with circles acting as glass disks.
- Rays refract and reflect through a tighter, more coupled cluster of shapes.
- Color comes from a dispersion-style trick that changes index of refraction per sample.

What it demonstrates:

- 2D ray/circle intersection
- Reflection and refraction in a lighter-weight setting
- Fresnel-style branching
- Spectral variation without building a full wavelength tracer
- How clustered geometry turns the same transport rules into broader caustic ribbons

Why it is useful as a template:

- It is cheaper than the 3D shaders.
- It is good for experimenting with more aggressive refractive coupling before moving to 3D geometry.

Accumulation fit:

- Enabled. This shader is static and benefits from accumulation.

### `crystal_hall`

Files:

- `src/shaders/crystal_hall.c`
- `src/shaders/crystal_hall.h`

What it is:

- A more advanced 3D ray-tracing scene that extends the earlier ideas.
- The scene uses planes for the room and spheres for the main objects.
- It mixes diffuse, mirror, glass, and emissive materials in one tracer.

What it demonstrates:

- Material dispatch by hit type
- Plane and sphere intersection in one scene
- Glass handling with Schlick-style reflectance
- A slightly more cinematic camera setup
- A scene layout that is easier to expand into a larger ray-tracing playground

Why it is useful as a template:

- It is the best current example if you want a richer 3D scene.
- It shows a clean way to separate intersection logic, hit data, and material response.

Accumulation fit:

- Disabled. The animated camera is supposed to stay live, not blend into a time-smear.

### `master_class`

Files:

- `src/shaders/master_class.c`
- `src/shaders/master_class.h`
- `docs/MASTER_CLASS.md`

What it is:

- A flagship static 3D interior scene built to converge well under accumulation.
- It adds direct sampling of a ceiling area light instead of relying only on random hits.
- It uses a deliberate display transform so highlights roll off instead of flattening into white blocks.

What it demonstrates:

- A cleaner “hero” composition for this renderer
- Area-light sampling for faster diffuse convergence
- Glass absorption, glossy metal, colored walls, and a procedural floor in one compact tracer
- Static camera plus subpixel jitter, so accumulation improves detail without the motion smear seen in `crystal_hall`

Why it is useful as a template:

- It is the best reference if you want to build a polished final image instead of a smaller study.
- It shows where to spend code on transport quality before reaching for more geometry.

Accumulation fit:

- Enabled. The static camera and sampled transport are designed for accumulation.

The original Blue Wall attempt under `src/shaders/blue_wall_*` has been retired. The active Blue Wall work now lives entirely in `pocs/blue_wall_scene`, where the Blender-driven retargeting, stage reset, and texture-support work can evolve without leaving parallel tech debt behind.

### `blue_wall_v2_A`

Files:

- `pocs/blue_wall_scene/blue_wall_v2_A.c`
- `pocs/blue_wall_scene/blue_wall_v2_A.h`
- `pocs/blue_wall_scene/blue_wall_v2_common.h`

What it is:

- The first local shader in the Blue Wall v2 reconstruction track.
- It resets Stage A so the room, camera, and light baseline are honest again.

What it demonstrates:

- separating the new reconstruction track from the older live Blue Wall series
- using the v2 extraction outputs without letting later fixture geometry leak into the shell baseline
- keeping the new work local to the POC until the stage contracts settle

Accumulation fit:

- Enabled. This is a static-camera still baseline.

### `blue_wall_v2_B`

Files:

- `pocs/blue_wall_scene/blue_wall_v2_B.c`
- `pocs/blue_wall_scene/blue_wall_v2_B.h`
- `pocs/blue_wall_scene/blue_wall_v2_common.h`

What it is:

- The layout-debug stage of the Blue Wall v2 track.
- It uses generated object orientation and stage filtering to render layout volumes directly from the new extraction data.

What it demonstrates:

- moving from older extracted proxy boxes to generated oriented layout volumes
- using grouped and filtered object metadata instead of one-off hand selection
- validating staging before more proxy detail is added

Accumulation fit:

- Enabled. The camera is static and the scene is still a progressive still.

### `blue_wall_v2_C`

Files:

- `pocs/blue_wall_scene/blue_wall_v2_C.c`
- `pocs/blue_wall_scene/blue_wall_v2_C.h`
- `pocs/blue_wall_scene/blue_wall_v2_common.h`

What it is:

- The first generated reconstruction skeleton in the Blue Wall v2 track.
- It rebuilds the main grouped scene elements with local proxy families driven by the new grouped data.

What it demonstrates:

- grouped scene reconstruction from the v2 extraction pipeline
- a floor lamp built from grouped fixture objects instead of room-helper leakage
- local POC-side shader construction that can mature before it replaces the old series

Accumulation fit:

- Enabled. This remains a static-camera reconstruction study.

### `blue_wall_v2_D`

Files:

- `pocs/blue_wall_scene/blue_wall_v2_D.c`
- `pocs/blue_wall_scene/blue_wall_v2_D.h`
- `pocs/blue_wall_scene/blue_wall_v2_common.h`

What it is:

- The first authored hero pass in the Blue Wall v2 track.
- It keeps the generated reconstruction skeleton from `blue_wall_v2_C`, then refines selected furniture and sideboard-top props without introducing host-side texture or buffer features.

What it demonstrates:

- Oriented object-local reconstruction helpers for Blender-derived scene data
- Curated prop detail on top of generated group membership
- How to keep late-stage scene improvements inside the shader layer instead of pushing complexity into the host

Why it is useful as a template:

- It is the clearest v2 example of mixing generated scene structure with handwritten hero refinements.
- It shows a practical path from Stage C skeletons to Stage D still-image quality without waiting for Stage E texture support.

Accumulation fit:

- Enabled. This remains a static-camera reference study.

### `blue_wall_v2_E`

Files:

- `pocs/blue_wall_scene/blue_wall_v2_E.c`
- `pocs/blue_wall_scene/blue_wall_v2_E.h`
- `src/shader_buffers.h`
- `src/shader_buffers.c`
- `src/u_texture.h`

What it is:

- The first texture-backed stage in the Blue Wall v2 track.
- It keeps the Stage D reconstruction, but moves static asset ownership into shader-local buffer initialization.

What it demonstrates:

- shader-owned asset setup through `buffers_init()`
- module-relative texture loading so the shader does not depend on process working directory
- shared texture lookup helpers instead of one-off per-shader image code

Why it is useful as a template:

- It is the reference for bringing static textures into this renderer without ballooning per-frame runtime cost.
- It shows the intended split between host-managed execution and shader-managed asset requirements.

Accumulation fit:

- Enabled. This remains a static-camera reference study.

### `mtsdf_hello_world`

Files:

- `pocs/mtsdf/mtsdf_hello_world.c`
- `pocs/mtsdf/mtsdf_hello_world.h`
- `pocs/mtsdf/mtsdf_text.h`
- `pocs/mtsdf/ascii_mtsdf_font.h`
- `pocs/mtsdf/README.md`

What it is:

- A text-rendering proof of concept built from a shader-owned MTSDF atlas.
- It renders `hello_world!` as rainbow bubble lettering with giant animated glyph layers using local atlas metrics and texture sampling helpers.

What it demonstrates:

- atlas generation through the `msdfgen` toolchain
- converting font metrics into C-friendly generated tables
- shader-owned static texture assets through `buffers_init()`
- reusable MTSDF lookup, layout, and sampling code that stays local to the POC
- resolution-independent oversized glyph rendering driven by the same atlas

Why it is useful as a template:

- It is the reference path for future text shaders, labels, and MSDF or MTSDF-backed UI experiments.
- It proves that the new shader buffer system can support richer static assets without pushing per-frame cost into the host.
- The host blurb links back to the upstream source: <https://github.com/Chlumsky/msdfgen>

Accumulation fit:

- Disabled. This is a single-frame text study.

### `sdf_fixed_hello_world`

Files:

- `pocs/sdf_fixed/sdf_fixed_hello_world.c`
- `pocs/sdf_fixed/sdf_fixed_hello_world.h`
- `pocs/sdf_fixed/README.md`

What it is:

- A lower-tech SDF text proof of concept that uses a fixed ASCII grid instead of per-glyph metrics.
- It treats the atlas as a hardcoded array of sub-images and renders a giant `SDF` backdrop plus a `hello_world!` banner.

What it demonstrates:

- a single-channel SDF atlas loaded as a shader-owned texture
- fixed-grid glyph indexing with no JSON processing in the runtime path
- the simplest plausible text shader architecture for this renderer

Why it is useful as a template:

- It is the baseline reference when you want “text from a texture atlas” without a richer font-layout layer.
- It gives the project a deliberately simpler comparison point next to the MTSDF work.

Accumulation fit:

- Disabled. This is a static single-frame text study.

## How To Add A New Shader

### 1. Create the pair of shader files

Add:

- `src/shaders/your_shader.c`
- `src/shaders/your_shader.h`

Header pattern:

```c
#pragma once

#include "../defines.h"

vec4_t your_shader_main(vec2_t fragCoord, const shader_uniforms_t *uniforms);

#define YOUR_SHADER(X) X( \
    your_shader, "Your Shader", your_shader_main, \
    NULL, \
    SHADER_FEATURE_TEMPORAL_ACCUMULATION | \
    SHADER_FEATURE_FRAME, \
    1024, 1024, \
    "Short blurb for the host catalog." \
)
```

### 2. Keep the shader self-contained

Good shader structure:

- local helper math
- local intersection routines
- local `HitData` or equivalent scene state
- one public `*_main(...)` entrypoint

Avoid:

- cross-thread mutable globals
- hidden frame history inside the shader
- dependencies on windowing or DX12 code

If the shader needs static assets:

- declare a `buffers_init()` function in the shader header and pass it in the metadata macro
- use `shader_buffers_alloc_bytes(...)` for raw data or `shader_buffers_load_texture_module_relative(...)` for textures
- sample textures through `src/u_texture.h`
- keep initialization out of `*_main(...)`

### 3. Wire it into the app

In `src/main.c`:

- include the new shader header
- add the shader macro to the catalog table

Current pattern:

```c
static const shader_desc_t g_shader_catalog[] = {
    YOUR_SHADER(SHADER_ENTRY)
};
```

Use feature flags in the header macro instead of a separate accumulation boolean.

### 4. Add it to the build

In `main.response`, add:

```text
src/shaders/your_shader.c
```

### 5. Decide whether accumulation should stay on

Good fit for accumulation:

- static cameras
- static geometry
- noisy Monte Carlo sampling where each frame adds information

Bad fit for accumulation without extra handling:

- fast animation
- moving cameras
- scene changes that should not smear over time

## Practical Design Tips

- Start from `sphere_tracing` if the idea is mostly about 3D lighting.
- Start from `glass_lenses` if the idea is about individual optics acting like lenses.
- Start from `glass_disks` if the idea is about reflection/refraction behavior in a coupled cluster.
- Start from `crystal_hall` if the idea needs multiple materials and a more built-out scene.
- Start from `master_class` if the goal is a high-quality still scene with better convergence and presentation.
- Keep the first version small. A clean tracer with three primitives is more useful than a huge tracer with tangled logic.
- Prefer explicit helper functions for intersections and material response. That makes it much easier for the next person to extend your shader.
- If you use randomness, seed from pixel coordinates and `frame` so the image converges under accumulation.

## Quick Checklist

- Compiles as plain C
- Has one `*_main(...)` entrypoint
- Does not mutate shared global state
- Returns `vec4_t`
- Uses local RNG state when needed
- Added to `main.response`
- Added to the shader catalog in `main.c`
- Looks correct with the bottom-origin coordinate convention
