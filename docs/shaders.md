# Shader Guide

This project treats each shader as a pure CPU-side pixel program:

```c
vec4_t shader_main(vec2_t fragCoord, vec2_t resolution, float time, uint frame);
```

The renderer calls that function once per pixel, in parallel, across multiple threads. The returned `vec4_t` is written straight into a mapped DX12 upload buffer and displayed by the GPU. There is no CPU-side RGB conversion anymore.

## Shader Contract

- `fragCoord` is the pixel coordinate for the current sample.
- `resolution` is the full image size in pixels.
- `time` is seconds since startup.
- `frame` is the running frame counter.
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
- Each shader opts into or out of accumulation through metadata in `src/main.c`.
- The shader should normally produce one sample for the current frame, not average across frames itself.
- If the shader animates heavily with `time`, mark it as non-accumulating or it will trail badly.

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
- `MASTER_CLASS.md`

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

## How To Add A New Shader

### 1. Create the pair of shader files

Add:

- `src/shaders/your_shader.c`
- `src/shaders/your_shader.h`

Header pattern:

```c
#pragma once

#include "../defines.h"

vec4_t your_shader_main(vec2_t fragCoord, vec2_t resolution, float time, uint frame);
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

### 3. Wire it into the app

In `src/main.c`:

- include the new shader header
- add the shader to the registry table

Current pattern:

```c
static ShaderEntry g_shaders[] = {
    {"your_shader", your_shader_main, true},
};
```

Set the third field to `false` for animated or otherwise non-accumulating shaders.

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
- Selected from `main.c`
- Looks correct with the bottom-origin coordinate convention
