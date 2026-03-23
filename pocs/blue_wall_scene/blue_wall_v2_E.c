#include "blue_wall_v2_E.h"

#include "blue_wall_v2_D.h"
#include "scene_extract_generated.h"

#include <string.h>

#ifdef SHADER_PLUGIN_BUILD
#include "shader_host_services.h"
HMODULE plugin_get_module(void);
#define PAINTING_TEXTURE_RESOURCE "PAINTING_JPG"
#endif

static const blue_wall_generated_object_t *blue_wall_v2_E_find_object(const char *name)
{
    for (int index = 0; index < BLUE_WALL_GENERATED_OBJECT_COUNT; index++) {
        if (strcmp(g_blue_wall_generated_objects[index].name, name) == 0) {
            return &g_blue_wall_generated_objects[index];
        }
    }

    return NULL;
}

ShaderBuffersCleanupFunc blue_wall_v2_E_buffers_init(shader_buffers_t *buffers, const shader_host_services_t *services, char *error, size_t error_size)
{
#ifdef SHADER_PLUGIN_BUILD
    HMODULE module = plugin_get_module();
    (void)blue_wall_v2_E_find_object;  /* not needed for resource-based loading */
    if (services->load_texture_resource(
            buffers,
            "painting",
            module,
            PAINTING_TEXTURE_RESOURCE,
            SHADER_TEXEL_FORMAT_RGBA8_UNORM,
            error,
            error_size) == NULL)
    {
        return NULL;
    }
    return services->default_cleanup;
#else
    (void)services;
    const blue_wall_generated_object_t *painting = blue_wall_v2_E_find_object("CanvasPainting_01");
    char relative_texture_path[512];

    if (painting == NULL || painting->texture_hint[0] == '\0') {
        snprintf(error, error_size, "Blue Wall v2 E could not resolve painting texture metadata.");
        return NULL;
    }

    snprintf(relative_texture_path, sizeof(relative_texture_path), "pocs/blue_wall_scene/blue_wall/textures/%s", painting->texture_hint);
    if (shader_buffers_load_texture_module_relative(
            buffers,
            "painting",
            relative_texture_path,
            SHADER_TEXEL_FORMAT_RGBA8_UNORM,
            error,
            error_size) == NULL)
    {
        return NULL;
    }

    return shader_buffers_default_cleanup;
#endif
}

vec4_t blue_wall_v2_E_main(vec2_t fragCoord, const shader_uniforms_t *uniforms)
{
    return blue_wall_v2_D_main(fragCoord, uniforms);
}
