# Shader Authoring Guide

A shader is a pure function that runs once per pixel, per frame.
This guide covers the SDK types, utilities, and patterns available
when writing shaders for CPU_Shader plugins.

## The Shader Function

```c
vec4_t my_shader(vec2_t fragCoord, const shader_uniforms_t *uniforms);
```

- **`fragCoord`** -- integer pixel coordinates (0-based). Add 0.5 for pixel center.
- **`uniforms`** -- read-only state provided by the host each frame.
- **Returns** -- `vec4_t` RGBA color. Components are 0-1 for SDR.

Shaders must be **pure**: no OS API calls, no memory allocation, no writing
to global state. The host calls shaders from multiple threads concurrently.

## Coordinate System

```
(0,0)              (width,0)
  +--------------------+
  |                    |
  |    fragCoord       |
  |    (x, y)          |
  |                    |
  +--------------------+
(0,height)       (width,height)
```

The standard UV computation:

```c
vec2_t uv = vec2(fragCoord.x / uniforms->resolution.x,
                 fragCoord.y / uniforms->resolution.y);
```

For centered coordinates (useful for circular/radial effects):

```c
vec2_t pixel = vec2(fragCoord.x + 0.5f, fragCoord.y + 0.5f);
vec2_t centered = vec2(pixel.x - uniforms->resolution.x * 0.5f,
                       pixel.y - uniforms->resolution.y * 0.5f);
/* Normalize by height for aspect-correct coordinates: */
centered = vec2(centered.x / uniforms->resolution.y,
                centered.y / uniforms->resolution.y);
```

## Uniforms Reference

```c
typedef struct {
    vec2_t         resolution;   /* viewport size in pixels          */
    float          time;         /* elapsed seconds                  */
    uint           frame;        /* frame counter (0-based)          */
    vec4_t         mouse;        /* xy = current pos, zw = click pos */
    shader_keys_t  keys;         /* keyboard state (256-bit field)   */
    const shader_buffers_t *buffers;   /* texture/buffer array       */
    const void    *variables;    /* user-defined variable struct     */
} shader_uniforms_t;
```

Declare which fields your shader reads via feature flags in the X-macro.

## Vector Math (shader_vmath.h)

All vector types and operations are available through `shader_defines.h`.

### Types

| Type | Fields | Constructors |
|---|---|---|
| `vec2_t` | `.x .y` | `vec2(x,y)` `vec2_o(a)` |
| `vec3_t` | `.x .y .z` | `vec3(x,y,z)` `vec3_o(a)` |
| `vec4_t` | `.x .y .z .w` | `vec4(x,y,z,w)` `vec4_o(a)` |

The `_o` (one) variants broadcast a single value: `vec3_o(1.0f)` = `{1,1,1}`.

### Scalar Functions

| Function | Description |
|---|---|
| `clampf(x, lo, hi)` | Clamp to range |
| `saturate(x)` | Clamp to 0-1 |
| `lerpf(a, b, t)` | Linear interpolation |
| `smoothstepf(e0, e1, x)` | Hermite interpolation with clamping |
| `safe_tanhf(x)` | Overflow-safe hyperbolic tangent |

### Vec2 Operations

| Function | Description |
|---|---|
| `v2_add(a, b)` | Component-wise add |
| `v2_sub(a, b)` | Component-wise subtract |
| `v2_mul(a, b)` | Component-wise multiply |
| `v2_mul1(v, s)` | Scalar multiply |
| `v2_div1(v, s)` | Scalar divide |
| `v2_dot(a, b)` | Dot product |
| `v2_length(v)` | Euclidean length |
| `v2_length_sq(v)` | Squared length (no sqrt) |
| `v2_distance(a, b)` | Distance between points |
| `v2_normalize(v)` | Unit vector (returns zero if degenerate) |
| `v2_lerp(a, b, t)` | Component-wise lerp |
| `v2_fract(v)` | Fractional parts |
| `v2_floor(v)` | Floor each component |
| `v2_abs(v)` | Absolute value each component |
| `v2_perp(v)` | Perpendicular (rotated 90 degrees) |
| `v2_reflect(v, n)` | Reflect vector around normal |
| `v2_refract(v, n, eta)` | Snell's law refraction |
| `v2_rotate(p, angle)` | Rotate point by angle (radians) |

### Vec3 Operations

| Function | Description |
|---|---|
| `v3_add(a, b)` | Component-wise add |
| `v3_sub(a, b)` | Component-wise subtract |
| `v3_mul(a, b)` | Component-wise multiply |
| `v3_add1(v, s)` | Add scalar to all components |
| `v3_mul1(v, s)` | Scalar multiply |
| `v3_div1(v, s)` | Scalar divide |
| `v3_dot(a, b)` | Dot product |
| `v3_length(v)` | Euclidean length |
| `v3_length_sq(v)` | Squared length (no sqrt) |
| `v3_distance(a, b)` | Distance between points |
| `v3_normalize(v)` | Unit vector (returns zero if degenerate) |
| `v3_safe_normalize(v)` | Unit vector (returns `{0,0,1}` if degenerate) |
| `v3_cross(a, b)` | Cross product |
| `v3_lerp(a, b, t)` | Component-wise lerp |
| `v3_fract(v)` | Fractional parts |
| `v3_floor(v)` | Floor each component |
| `v3_abs(v)` | Absolute value each component |
| `v3_saturate(v)` | Clamp each component to 0-1 |
| `v3_reflect(v, n)` | Reflect vector around normal |
| `v3_refract(v, n, eta)` | Snell's law refraction |

### Vec4 Operations

| Function | Description |
|---|---|
| `v4_add(a, b)` | Component-wise add |
| `v4_sub(a, b)` | Component-wise subtract |
| `v4_mul(a, b)` | Component-wise multiply |
| `v4_mul1(v, s)` | Scalar multiply |
| `v4_div1(v, s)` | Scalar divide |
| `v4_dot(a, b)` | Dot product |
| `v4_length(v)` | Euclidean length |
| `v4_length_sq(v)` | Squared length |
| `v4_normalize(v)` | Unit vector |
| `v4_lerp(a, b, t)` | Component-wise lerp |
| `v4_fract(v)` | Fractional parts |
| `v4_floor(v)` | Floor each component |
| `v4_abs(v)` | Absolute value each component |
| `v4_sqrt(v)` | Square root each component |

### Constants

| Name | Value |
|---|---|
| `PI` | `3.1415926f` |

## Random Numbers (shader_random.h)

PCG-based deterministic PRNG. Seed from pixel coordinates for per-pixel
randomness that is reproducible across frames.

```c
uint state = (uint)(fragCoord.x * 1973 + fragCoord.y * 9277 + uniforms->frame * 26699);
```

| Function | Description |
|---|---|
| `shader_next_rand(&state)` | Raw 32-bit PCG step |
| `shader_rand_1(&state)` | Uniform float in [0, 1] |
| `shader_rand_1_nd(&state)` | Normal distribution (Box-Muller) |
| `shader_rand_dir(&state)` | Uniform random direction on unit sphere |

All functions advance the `state`. Declared in `shader_random.h`, visible
through `shader_defines.h`.

## Color Utilities (shader_color.h)

| Function | Description |
|---|---|
| `shader_hsv_to_rgb(h, s, v)` | HSV to RGB. `h` in [0, 1]. |
| `shader_hsv_to_rgb_v(hsv)` | Vec3 variant: `{h, s, v}` |
| `shader_aces_tonemap(color)` | ACES filmic tonemapping (linear to display) |
| `shader_gamma_encode(color)` | Linear to sRGB gamma (pow 1/2.2) |
| `shader_gamma_decode(color)` | sRGB gamma to linear (pow 2.2) |

### Typical HDR-to-SDR Pipeline

```c
vec3_t light = trace(ro, rd);           /* scene-linear radiance */
light = shader_aces_tonemap(light);     /* compress to 0-1       */
light = shader_gamma_encode(light);     /* apply display gamma   */
return vec4(light.x, light.y, light.z, 1.0f);
```

## Ray Intersections (shader_ray.h)

All intersection functions return the hit distance `t`, or `-1.0f` on miss.

| Function | Description |
|---|---|
| `shader_ray_sphere(ro, rd, center, radius)` | Ray-sphere intersection. Returns nearest positive `t`. |
| `shader_ray_plane(ro, rd, point, normal)` | Ray-plane intersection. |
| `shader_ray_circle(ro2d, rd2d, center, radius)` | 2D ray-circle intersection. |
| `shader_schlick(cosine, eta_i, eta_t)` | Schlick's Fresnel approximation. |

### Ray Tracing Pattern

```c
float t = shader_ray_sphere(ro, rd, sphere_center, sphere_radius);
if (t > 0.0f) {
    vec3_t hit_point = v3_add(ro, v3_mul1(rd, t));
    vec3_t normal = v3_normalize(v3_sub(hit_point, sphere_center));
    /* shade... */
}
```

### Glass / Refraction Pattern

```c
float cosine = saturate(v3_dot(v3_mul1(rd, -1.0f), normal));
float reflectance = shader_schlick(cosine, 1.0f, 1.45f);  /* air to glass */

vec3_t reflected = v3_reflect(rd, normal);
vec3_t refracted = v3_refract(rd, normal, 1.0f / 1.45f);

if (v3_length_sq(refracted) == 0.0f || reflectance > threshold) {
    rd = reflected;
    ro = v3_add(point, v3_mul1(normal, epsilon));
} else {
    rd = refracted;
    ro = v3_sub(point, v3_mul1(normal, epsilon));
}
```

## Compositing (shader_composite.h)

| Function | Description |
|---|---|
| `shader_layer_over(dst, src)` | Porter-Duff "over" alpha composite |
| `shader_sdf_fill(sdf, aa)` | SDF to alpha with anti-aliasing width `aa` |
| `shader_sdf_band(sdf, inner, outer)` | SDF ring/band alpha |

### Layered UI Pattern

```c
vec4_t background = vec4(0.1f, 0.1f, 0.12f, 1.0f);
float circle_sdf = v2_length(v2_sub(uv, center)) - radius;
float alpha = shader_sdf_fill(circle_sdf, aa_width);
vec4_t circle = vec4(0.3f, 0.6f, 0.9f, alpha);
return shader_layer_over(background, circle);
```

## Shader File Organization

### Header (.h)

The header declares the entry point and the X-macro. Keep it minimal:

```c
#pragma once

#include "shader_defines.h"

vec4_t my_shader_main(vec2_t fragCoord, const shader_uniforms_t *uniforms);

#define MY_SHADER(X) X( \
    my_shader, "My Shader", my_shader_main, \
    SHADER_BUFFER_NONE, \
    SHADER_VARIABLE_NONE, \
    SHADER_COLOR_SPACE_SDR_DISPLAY, \
    SHADER_FEATURE_TIME, \
    800, 600, \
    "Short description." \
)
```

If the shader uses variables, add the variable struct, descriptor array
declaration, and count enum to the header (see **[SDK_plugin.md](SDK_plugin.md)**).

If the shader uses buffers, add the `ShaderBuffersCleanupFunc` declaration.

### Implementation (.c)

The implementation includes its own header and contains the rendering logic:

```c
#include "my_shader.h"

vec4_t my_shader_main(vec2_t fragCoord, const shader_uniforms_t *uniforms)
{
    /* All rendering logic here */
}
```

Use `static` functions and `static const` data freely for helpers. The shader
runs in a freestanding environment with no CRT -- `<math.h>` is available
through `shader_vmath.h`, but no `<stdio.h>`, `<stdlib.h>`, or `<string.h>`.

### What's Available

Since `shader_defines.h` includes all SDK utility headers, every shader
automatically has access to:

- All vector math (`v2_*`, `v3_*`, `v4_*`, scalar utilities)
- Random number generation (`shader_rand_*`)
- Color conversions and tonemapping (`shader_hsv_to_rgb`, `shader_aces_tonemap`, etc.)
- Ray intersections and optics (`shader_ray_sphere`, `shader_schlick`, etc.)
- Compositing and SDF helpers (`shader_layer_over`, `shader_sdf_fill`, etc.)
- `min(a,b)` / `max(a,b)` macros
- `PI` constant

No additional `#include` is needed for these unless the shader also needs
texture sampling (`u_texture.h`) or variable access (`u_vars.h`).

## Color Space Guidelines

### SDR Display (`SHADER_COLOR_SPACE_SDR_DISPLAY`)

Output sRGB values in [0, 1]. This is the default for most shaders.
If working in linear light, apply tonemapping and gamma encoding before output:

```c
color = shader_aces_tonemap(color);
color = shader_gamma_encode(color);
```

### Scene Linear (`SHADER_COLOR_SPACE_SCENE_LINEAR`)

Output linear-light values. The host handles the display transfer function.
Values can exceed 1.0 for HDR content on capable monitors.

### HDR10 ST2084 (`SHADER_COLOR_SPACE_HDR10_ST2084`)

Output in PQ transfer function, Rec.2020 color space. For advanced HDR
shaders targeting wide-gamut displays.

## Temporal Accumulation

Shaders that set `SHADER_FEATURE_TEMPORAL_ACCUMULATION` benefit from
frame-over-frame progressive rendering. The host accumulates results
across frames and resets the accumulation when the camera or viewport
changes.

Use `uniforms->frame` to vary sampling patterns across frames:

```c
uint seed = (uint)(fragCoord.x * 1973 + fragCoord.y * 9277
                   + uniforms->frame * 26699 + 1);
```

This gives each frame a different random seed, and the accumulated average
converges to a clean result over time.

## Common Patterns

### Aspect-Correct UV

```c
vec2_t uv = vec2((fragCoord.x + 0.5f - uniforms->resolution.x * 0.5f) / uniforms->resolution.y,
                 (fragCoord.y + 0.5f - uniforms->resolution.y * 0.5f) / uniforms->resolution.y);
```

### Camera Setup (3D)

```c
vec3_t ro = vec3(0.0f, 0.0f, -5.0f);
vec3_t target = vec3(0.0f, 0.0f, 0.0f);
vec3_t forward = v3_normalize(v3_sub(target, ro));
vec3_t right = v3_normalize(v3_cross(vec3(0.0f, 1.0f, 0.0f), forward));
vec3_t up = v3_cross(forward, right);
float fov = 1.35f;
vec3_t rd = v3_normalize(v3_add(forward,
    v3_add(v3_mul1(right, uv.x * fov), v3_mul1(up, uv.y * fov))));
```

### Smooth Animation

```c
float t = uniforms->time;
float x = sinf(t * 0.35f) * 0.45f;
float y = 0.10f + 0.08f * sinf(t * 0.21f);
```

Use `SHADER_FEATURE_TIME` in feature flags when accessing `uniforms->time`.
