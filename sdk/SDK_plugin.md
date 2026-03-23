# Plugin Creation Guide

This guide walks through creating a CPU_Shader plugin from scratch. A plugin
is a DLL that contains one or more shaders, discovered and loaded by the host
at startup.

## Plugin Directory Structure

A typical plugin directory looks like this:

```
my_plugin/
    collection.c          # Plugin exports and shader registration
    plugin_defines.c      # SDK implementation instantiation
    plugin.mak            # Build rules (when using the project Makefile)
    my_shader.h           # Shader header: entry point + X-macro
    my_shader.c           # Shader implementation
    another_shader.h      # Additional shaders...
    another_shader.c
```

## Step 1: Write the Shader

Each shader has a header (`.h`) and an implementation (`.c`). The header
declares the entry point and the registration X-macro. The implementation
contains the rendering logic.

See **[SDK_shader.md](SDK_shader.md)** for the full shader authoring guide.

**my_shader.h** -- minimal example:
```c
#pragma once

#include "shader_defines.h"

vec4_t my_shader_main(vec2_t fragCoord, const shader_uniforms_t *uniforms);

#define MY_SHADER(X) X( \
    my_shader, "My Shader", my_shader_main, \
    SHADER_BUFFER_NONE, \
    SHADER_VARIABLE_NONE, \
    SHADER_COLOR_SPACE_SDR_DISPLAY, \
    SHADER_FEATURE_NONE, \
    800, 600, \
    "A simple gradient test." \
)
```

**my_shader.c**:
```c
#include "my_shader.h"

vec4_t my_shader_main(vec2_t fragCoord, const shader_uniforms_t *uniforms)
{
    vec2_t uv = vec2(fragCoord.x / uniforms->resolution.x,
                     fragCoord.y / uniforms->resolution.y);
    return vec4(uv.x, uv.y, 0.5f, 1.0f);
}
```

## Step 2: Write collection.c

The collection module registers all shaders in the plugin and implements
the two required DLL exports.

```c
#include "shader_plugin.h"

#include "my_shader.h"
/* #include "another_shader.h" */

/* ---- X-macro expansion to build the shader descriptor table ---- */

#define ENTRY(...) ENTRY_IMPL(__VA_ARGS__)
#define ENTRY_IMPL(id, display_name, entry, buffers_init, vars, var_count, var_size, \
                   color_space, features, width, height, blurb) \
    {#id, display_name, blurb, entry, buffers_init, vars, var_count, var_size, \
     color_space, features, width, height},

static const shader_desc_t g_shaders[] = {
    MY_SHADER(ENTRY)
    /* ANOTHER_SHADER(ENTRY) */
};

#undef ENTRY
#undef ENTRY_IMPL

static const int g_shader_count = (int)(sizeof(g_shaders) / sizeof(g_shaders[0]));

/* ---- Plugin identity and ABI ---- */

static shader_collection_info_t g_info = {0};

__declspec(dllexport)
const shader_collection_info_t *shader_collection_query(void)
{
    g_info.collection_name    = "My Plugin";
    g_info.collection_version = "1.0";
    g_info.collection_blurb   = "Example plugin collection.";
    g_info.abi_version        = SHADER_PLUGIN_ABI_VERSION;
    GetModuleHandleExA(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
        GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (LPCSTR)shader_collection_query,
        &g_info.module_handle);
    return &g_info;
}

__declspec(dllexport)
const shader_desc_t *shader_collection_load(int *count_out)
{
    if (count_out != NULL) {
        *count_out = g_shader_count;
    }
    return g_shaders;
}
```

### The ENTRY Macro

The `ENTRY` / `ENTRY_IMPL` two-stage macro is necessary because the shader
X-macro passes its arguments as a single group. `ENTRY_IMPL` unpacks them
into the `shader_desc_t` struct initializer fields.

The `#id` in `ENTRY_IMPL` stringifies the shader's C identifier, giving the
host a unique string key for each shader.

### Adding Shaders

To add a shader to a plugin:

1. Create the shader `.h` and `.c` files
2. `#include` the header in `collection.c`
3. Add `SHADER_NAME(ENTRY)` to the `g_shaders` array
4. Add the `.obj` to your build

## Step 3: Write plugin_defines.c

This file instantiates all SDK library implementations. Every plugin needs
exactly one copy:

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

This is boilerplate. Copy it exactly.

## Step 4: Build

### Command-Line Build

```
clang -std=c17 -DUNICODE -D_UNICODE -DSHADER_PLUGIN_BUILD -Isdk ^
      -O3 -shared -o my_plugin.dll ^
      collection.c plugin_defines.c my_shader.c ^
      -fuse-ld=lld -lkernel32
```

Build requirements:
- **`-Isdk`** -- the only include path. Never include host headers from `src/`.
- **`-DSHADER_PLUGIN_BUILD`** -- required define.
- **`-shared`** -- produces a DLL.
- **`-lkernel32`** -- the only system library. Plugins are freestanding; no CRT.
- **`-fuse-ld=lld`** -- use the LLVM linker (matches the project convention).

### Project Makefile Integration (plugin.mak)

When integrating with the project's build system, create a `plugin.mak` file
in your plugin directory. The main `Makefile` includes it via `!INCLUDE`.

**Simple plugin** (no local headers, no resources):
```nmake
MY_PLUGIN = path\to\my_plugin.dll
MY_PLUGIN_OBJS = \
    $(PLUGIN_OBJ_DIR)\my_plugin\collection.obj \
    $(PLUGIN_OBJ_DIR)\my_plugin\plugin_defines.obj \
    $(PLUGIN_OBJ_DIR)\my_plugin\my_shader.obj

{path\to\my_plugin}.c{$(PLUGIN_OBJ_DIR)\my_plugin}.obj:
    @if not exist "$(@D)" mkdir "$(@D)"
    $(CC) $(PLUGIN_CFLAGS) $(DEPFLAGS) -MF "$(@D)\$(@B).d" -MT $@ -c $< -o $@

$(MY_PLUGIN): $(MY_PLUGIN_OBJS)
    $(CC) $(PLUGIN_LINKFLAGS) -shared -o $@ $(MY_PLUGIN_OBJS)
```

**Plugin with local include path** (for local headers):
```nmake
{plugins\my_plugin}.c{$(PLUGIN_OBJ_DIR)\my_plugin}.obj:
    @if not exist "$(@D)" mkdir "$(@D)"
    $(CC) $(PLUGIN_CFLAGS) -Iplugins\my_plugin $(DEPFLAGS) -MF "$(@D)\$(@B).d" -MT $@ -c $< -o $@
```

**Plugin with embedded resources** (textures):
```nmake
MY_PLUGIN_RES = $(PLUGIN_OBJ_DIR)\my_plugin\textures.res

$(MY_PLUGIN_RES): path\to\my_plugin\textures.rc path\to\my_plugin\image.png
    @if not exist "$(@D)" mkdir "$(@D)"
    $(RC) /fo $@ path\to\my_plugin\textures.rc

$(MY_PLUGIN): $(MY_PLUGIN_OBJS) $(MY_PLUGIN_RES)
    $(CC) $(PLUGIN_LINKFLAGS) -shared -o $@ $(MY_PLUGIN_OBJS) $(MY_PLUGIN_RES)
```

The NMake inference rule `{srcdir}.c{objdir}.obj:` eliminates per-file
compile rules. Adding a new shader only requires adding its `.obj` to the
`OBJS` list.

## Textures and Buffers

Shaders that need texture data implement a `buffers_init` function. This
function is called once when the shader is first selected, before any
rendering begins.

### Embedding Textures as DLL Resources

1. Create a resource file (`textures.rc`):
   ```rc
   MY_TEXTURE  RCDATA  "my_image.png"
   ```

2. Expose the plugin's module handle from `collection.c`:
   ```c
   HMODULE plugin_get_module(void)
   {
       return g_info.module_handle;
   }
   ```

3. Implement `buffers_init` in your shader:
   ```c
   #include "shader_host_services.h"

   HMODULE plugin_get_module(void);  /* from collection.c */

   ShaderBuffersCleanupFunc my_buffers_init(
       shader_buffers_t *buffers,
       const shader_host_services_t *services,
       char *error, size_t error_size)
   {
       if (!services || !services->load_texture_resource) {
           return NULL;
       }

       if (!services->load_texture_resource(
               buffers, "my_tex", plugin_get_module(),
               "MY_TEXTURE", SHADER_TEXEL_FORMAT_RGBA8_UNORM,
               error, error_size)) {
           return NULL;
       }

       return services->default_cleanup;
   }
   ```

4. Register the init function in your X-macro (replaces `SHADER_BUFFER_NONE`):
   ```c
   #define MY_SHADER(X) X( \
       my_shader, "My Shader", my_shader_main, \
       my_buffers_init, \
       ...
   )
   ```

5. Declare the init function in your shader header:
   ```c
   ShaderBuffersCleanupFunc my_buffers_init(
       shader_buffers_t *buffers,
       const shader_host_services_t *services,
       char *error, size_t error_size);
   ```

### Sampling Textures in the Shader

```c
#include "u_texture.h"

vec4_t my_shader_main(vec2_t fragCoord, const shader_uniforms_t *uniforms)
{
    const shader_buffer_t *tex = u_buffer_named(uniforms, "my_tex");
    vec2_t uv = vec2(fragCoord.x / uniforms->resolution.x,
                     fragCoord.y / uniforms->resolution.y);

    if (!u_texture_valid(tex)) {
        return vec4(1.0f, 0.0f, 1.0f, 1.0f);  /* magenta fallback */
    }

    return u_texture_sample_bilinear(tex, uv, U_TEXTURE_ADDRESS_CLAMP);
}
```

**Samplers**: `u_texture_sample_nearest`, `u_texture_sample_bilinear`.
**Address modes**: `U_TEXTURE_ADDRESS_CLAMP`, `U_TEXTURE_ADDRESS_WRAP`.
**Texel formats**: `SHADER_TEXEL_FORMAT_R8_UNORM`, `SHADER_TEXEL_FORMAT_RGBA8_UNORM`, `SHADER_TEXEL_FORMAT_BGRA8_UNORM`.

### Host Service Functions

The `shader_host_services_t` table is provided during `buffers_init`:

| Function | Purpose |
|---|---|
| `alloc_bytes` | Allocate a raw byte buffer |
| `load_texture_file` | Load texture from an absolute file path |
| `load_texture_relative` | Load texture relative to the host executable |
| `load_texture_resource` | Load texture from DLL-embedded RCDATA resource |
| `default_cleanup` | Cleanup function to return from `buffers_init` on success |

Always return `services->default_cleanup` on success, or `NULL` on failure
(write a diagnostic to the `error` buffer).

## Shader Variables

Variables expose tunable parameters for scripted (`--capture`) execution.
The host parses variable values from command-line arguments and populates
the variable struct before rendering.

### Defining Variables

1. Define the variable struct in your shader header:
   ```c
   typedef struct {
       vec3_t primary_color;
       float  depression;
   } my_vars_t;
   ```

2. Create the variable descriptor array:
   ```c
   enum { MY_VARIABLE_COUNT = 2 };

   extern const shader_variable_desc_t g_my_variables[MY_VARIABLE_COUNT];
   ```

3. Define the descriptors in your shader `.c` file:
   ```c
   const shader_variable_desc_t g_my_variables[MY_VARIABLE_COUNT] = {
       {"primary_color", SHADER_VARIABLE_TYPE_VEC3, offsetof(my_vars_t, primary_color), "0.3,0.5,0.8"},
       {"depression",    SHADER_VARIABLE_TYPE_FLOAT, offsetof(my_vars_t, depression),   "0.0"},
   };
   ```

4. Register them in the X-macro:
   ```c
   #define MY_SHADER(X) X( \
       my_shader, "My Shader", my_shader_main, \
       SHADER_BUFFER_NONE, \
       g_my_variables, MY_VARIABLE_COUNT, sizeof(my_vars_t), \
       ...
   )
   ```

   The three fields after `SHADER_BUFFER_NONE` replace `SHADER_VARIABLE_NONE`.

### Accessing Variables in the Shader

```c
#include "u_vars.h"

vec4_t my_shader_main(vec2_t fragCoord, const shader_uniforms_t *uniforms)
{
    const my_vars_t *v = U_VARS_AS(my_vars_t, uniforms);
    vec3_t color = v ? v->primary_color : vec3(0.3f, 0.5f, 0.8f);
    float depression = v ? v->depression : 0.0f;
    /* ... */
}
```

Always provide a static fallback for when `v` is `NULL` (interactive mode
without scripted values).

### Variable Types

| Enum | C Type | Default Format |
|---|---|---|
| `SHADER_VARIABLE_TYPE_FLOAT` | `float` | `"0.5"` |
| `SHADER_VARIABLE_TYPE_INT` | `int` | `"8"` |
| `SHADER_VARIABLE_TYPE_BOOL` | `bool` (int) | `"1"` |
| `SHADER_VARIABLE_TYPE_VEC2` | `vec2_t` | `"1.0,0.5"` |
| `SHADER_VARIABLE_TYPE_VEC3` | `vec3_t` | `"1.0,0.5,0.2"` |
| `SHADER_VARIABLE_TYPE_VEC4` | `vec4_t` | `"1.0,0.5,0.2,1.0"` |

## The X-Macro Registration Pattern

Every shader header defines a registration macro. This macro is the contract
between the shader and the plugin's `collection.c`.

```c
#define MY_SHADER(X) X( \
    shader_id,                      /* unique C identifier            */ \
    "Display Name",                 /* shown in host treeview         */ \
    shader_main_func,               /* RenderFunc pointer             */ \
    buffers_init_or_NULL,           /* ShaderBuffersInitFunc or NULL  */ \
    var_array, var_count, var_size, /* or SHADER_VARIABLE_NONE        */ \
    SHADER_COLOR_SPACE_SDR_DISPLAY, /* output color space             */ \
    SHADER_FEATURE_TIME,            /* feature flags (combine with |) */ \
    800, 600,                       /* preferred viewport dimensions  */ \
    "Short description."            /* shown in host UI               */ \
)
```

| Argument | Description |
|---|---|
| `shader_id` | Unique C identifier. Stringified as the shader's key. |
| `"Display Name"` | Human-readable name shown in the host treeview. |
| `shader_main_func` | The `RenderFunc` entry point. |
| `buffers_init` | `ShaderBuffersInitFunc` pointer, or `SHADER_BUFFER_NONE` (`NULL`). |
| `var_array` | Pointer to `shader_variable_desc_t[]`, or part of `SHADER_VARIABLE_NONE`. |
| `var_count` | Number of variables, or part of `SHADER_VARIABLE_NONE`. |
| `var_size` | `sizeof` the variable struct, or part of `SHADER_VARIABLE_NONE`. |
| `color_space` | `shader_color_space_t` enum value. |
| `feature_flags` | Bitfield of `SHADER_FEATURE_*` values. |
| `width, height` | Preferred viewport dimensions in pixels. |
| `"blurb"` | Short description. Supports HTML links. |

Convenience macros:
- `SHADER_BUFFER_NONE` expands to `NULL` (no buffers_init function).
- `SHADER_VARIABLE_NONE` expands to `NULL, 0, 0` (no variables).

## Plugin Deployment

Place the built DLL inside `{exe}/plugins/` (or a subdirectory thereof).
The host scans this directory recursively at startup.

To load plugins from additional locations, use the `--plugin-dir` command-line
option (repeatable):

```
bin.exe --plugin-dir "C:\my_shaders" --plugin-dir "D:\more_shaders"
```

Relative paths are resolved against the executable's directory:

```
bin.exe --plugin-dir pocs
```

The host logs which plugins it discovers and loads. Check the diagnostics
in the stats dialog if your plugin is not appearing in the shader treeview.
