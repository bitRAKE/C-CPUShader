#include "defines.h"
#include "win.h"
#include "shaders/sphere_tracing.h"
#include "shaders/kinetic_orbs.h"
#include "shaders/hsv_picker.h"
#include "shaders/glass_lenses.h"
#include "shaders/glass_disks.h"
#include "shaders/crystal_hall.h"
#include "shaders/master_class.h"

typedef struct {
    const char *name;
    RenderFunc  render;
    bool        temporal_accumulation;
} ShaderEntry;

static ShaderEntry g_shaders[] = {
    // display name     shader entry-point      accumulation
    {"sphere_tracing",  sphere_tracing_main,    true},
    {"kinetic_orbs",    kinetic_orbs_main,      false},
    {"hsv_picker",      hsv_picker_main,        false},
    {"glass_lenses",    glass_lenses_main,      true},
    {"glass_disks",     glass_disks_main,       true},
    {"crystal_hall",    crystal_hall_main,      false},
    {"master_class",    master_class_main,      true}
};

static const int g_shader_count = (int)(sizeof(g_shaders) / sizeof(g_shaders[0]));
static int       g_current_shader_index = 6;
static RenderFunc g_current_shader = master_class_main;

static const char *shader_current_name(void)
{
    return g_shaders[g_current_shader_index].name;
}

static bool shader_current_accumulates(void)
{
    return g_shaders[g_current_shader_index].temporal_accumulation;
}

static void shader_cycle(int direction)
{
    int next_index = g_current_shader_index + direction;

    if (next_index < 0) {
        next_index = g_shader_count - 1;
    } else if (next_index >= g_shader_count) {
        next_index = 0;
    }

    g_current_shader_index = next_index;
    g_current_shader = g_shaders[g_current_shader_index].render;
}

vec4_t main_image(vec2_t fragCoord, const shader_uniforms_t *uniforms)
{
    return g_current_shader(fragCoord, uniforms);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd) {
    (void)hInstance;
    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nShowCmd;

    //Init Window
    if (!window_create("Renderer", 1024, 1024)) {
        return 1;
    }

    window_set_shader_switcher(shader_cycle, shader_current_name, shader_current_accumulates);

    //Render loop with 31 dedicated render workers plus a separate GUI thread
    window_run(main_image, 31);

    return 0;
}
