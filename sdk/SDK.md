# CPU_Shader Plugin SDK

Write shader plugins as standalone DLLs that the host discovers at startup.
Each plugin is a *shader collection* -- a DLL containing one or more shaders
that the host loads, displays in its treeview, and renders per-pixel on the CPU.

For step-by-step plugin creation, see **[SDK_plugin.md](SDK_plugin.md)**.
For the shader authoring API reference, see **[SDK_shader.md](SDK_shader.md)**.

## Architecture Overview

```
 Host (CPU_Shader.exe)
   |
   +-- scans {exe}/plugins/ recursively for *.dll
   |     additional dirs via: --plugin-dir "<dir>"
   |
   +-- calls shader_collection_query()
   |     checks ABI version == SHADER_PLUGIN_ABI_VERSION (1)
   |
   +-- calls shader_collection_load()
   |     receives shader_desc_t array
   |
   +-- for each pixel, each frame:
         calls shader_desc_t.render(fragCoord, uniforms)
```

Plugins compile with `-Isdk` only. They never include host headers from `src/`.

## SDK Headers

| Header | Purpose |
|---|---|
| **shader_defines.h** | Gateway header. Includes vmath and all utility libraries. Defines `shader_uniforms_t`, feature flags, color spaces, `bool`/`uint` types. |
| **shader_vmath.h** | `vec2_t`, `vec3_t`, `vec4_t` types with full arithmetic, `PI`, scalar utilities (`saturate`, `lerpf`, `smoothstepf`, `clampf`). Header-only; define `VMATH_IMPL` in one `.c` file. |
| **shader_random.h** | PCG-based deterministic PRNG: `shader_rand_1`, `shader_rand_1_nd` (normal distribution), `shader_rand_dir` (uniform sphere). Define `SHADER_RANDOM_IMPL`. |
| **shader_color.h** | `shader_hsv_to_rgb`, `shader_aces_tonemap`, `shader_gamma_encode`/`decode`. Define `SHADER_COLOR_IMPL`. |
| **shader_ray.h** | Ray-sphere, ray-plane, ray-circle intersections (return `t` or `-1.0f`). Schlick Fresnel approximation. Define `SHADER_RAY_IMPL`. |
| **shader_composite.h** | Porter-Duff `shader_layer_over`, SDF helpers `shader_sdf_fill`/`shader_sdf_band`. Define `SHADER_COMPOSITE_IMPL`. |
| **shader_plugin.h** | Plugin contract: `shader_collection_info_t`, ABI version, export names. |
| **shader_catalog.h** | `shader_desc_t` and `RenderFunc` typedef. |
| **shader_buffers.h** | `shader_buffer_t`, `shader_buffers_t`, `ShaderBuffersInitFunc`. Buffer types and texel formats. |
| **shader_variables.h** | `shader_variable_desc_t` for exposing tunable parameters. |
| **shader_host_services.h** | Callback table for texture loading and buffer allocation. Only needed by shaders with buffers. |
| **u_texture.h** | Texture lookup and sampling (nearest, bilinear). Address modes: clamp, wrap. |
| **u_vars.h** | `U_VARS_AS(type, uniforms)` macro for typed access to shader variables. |

## Header-Only Implementation Pattern

Every SDK utility library (vmath, random, color, ray, composite) uses the
same pattern: declarations are always visible, but implementations are gated
behind a `*_IMPL` define. Exactly one `.c` file per DLL defines all the
`*_IMPL` macros and includes the headers to create the function bodies.

This file is conventionally called **`plugin_defines.c`**:

```c
#define VMATH_IMPL
#define SHADER_RANDOM_IMPL
#define SHADER_COLOR_IMPL
#define SHADER_RAY_IMPL
#define SHADER_COMPOSITE_IMPL

#include "shader_defines.h"
#include "shader_random.h"
#include "shader_color.h"
#include "shader_ray.h"
#include "shader_composite.h"
```

Since `shader_defines.h` includes all utility headers for declarations,
every shader `.c` file automatically sees the function signatures. Only
`plugin_defines.c` generates the actual compiled code.

## Plugin Contract

A plugin DLL exports exactly two functions:

```c
__declspec(dllexport)
const shader_collection_info_t *shader_collection_query(void);

__declspec(dllexport)
const shader_desc_t *shader_collection_load(int *count_out);
```

The host calls `shader_collection_query` first. If `abi_version` does not
match `SHADER_PLUGIN_ABI_VERSION` (currently **1**), the plugin is skipped
with a diagnostic message.

## Shader Contract

Every shader is a pure function:

```c
vec4_t my_shader(vec2_t fragCoord, const shader_uniforms_t *uniforms);
```

The host calls this once per pixel, per frame. The return value is RGBA.
Shaders must not call OS APIs, allocate memory, or write to global state
during rendering.

### Uniforms

| Field | Type | Description |
|---|---|---|
| `resolution` | `vec2_t` | Viewport size in pixels |
| `time` | `float` | Elapsed seconds |
| `frame` | `uint` | Frame counter (0-based) |
| `mouse` | `vec4_t` | xy = current position, zw = click position |
| `keys` | `shader_keys_t` | Keyboard state bitfield (256 bits) |
| `buffers` | `const shader_buffers_t*` | Texture/buffer array (NULL if none) |
| `variables` | `const void*` | User-defined variable struct (NULL if none) |

### Feature Flags

| Flag | Meaning |
|---|---|
| `SHADER_FEATURE_NONE` | Static image, no animation |
| `SHADER_FEATURE_TIME` | Uses `time` and `frame` |
| `SHADER_FEATURE_MOUSE` | Uses `mouse` |
| `SHADER_FEATURE_KEYS` | Uses `keys` |
| `SHADER_FEATURE_TEMPORAL_ACCUMULATION` | Progressive rendering (frame resets on camera change) |

Combine with `|`. The host uses these to optimize scheduling.

### Color Spaces

| Value | Output |
|---|---|
| `SHADER_COLOR_SPACE_SDR_DISPLAY` | sRGB, 0-1 range |
| `SHADER_COLOR_SPACE_SCENE_LINEAR` | Linear, scRGB for HDR monitors |
| `SHADER_COLOR_SPACE_HDR10_ST2084` | PQ transfer function, Rec.2020 |

## Building

```
clang -std=c17 -DUNICODE -D_UNICODE -DSHADER_PLUGIN_BUILD -Isdk ^
      -O3 -shared -o my_plugin.dll ^
      collection.c plugin_defines.c my_shader.c ^
      -fuse-ld=lld -lkernel32
```

Key constraints:
- `-Isdk` only. Never `-Isrc`.
- `-DSHADER_PLUGIN_BUILD` is required.
- Link only `-lkernel32`. No CRT, no other system libraries.
- Place the DLL anywhere under the CPU_Shader directory tree.

## ABI Versioning

The current ABI version is **1** (`SHADER_PLUGIN_ABI_VERSION`).

The host checks this value before calling `shader_collection_load`. Plugins
built against a different ABI version are skipped. Bump this value when
struct layouts or function signatures change in a breaking way.
