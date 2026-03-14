#include "defines.h"
#include "win.h"
#include "host/host_options.h"

#include "shader_catalog.h"
#include "shaders/sphere_tracing.h"
#include "shaders/kinetic_orbs.h"
#include "../pocs/capture_animation/animated_sprite.h"
#include "../pocs/capture_animation/orbit_stars.h"
#include "shaders/glass_lenses.h"
#include "shaders/glass_disks.h"
#include "shaders/crystal_hall.h"
#include "shaders/master_class.h"
#include "shaders/master_class_scrgb.h"
#include "shaders/master_class_hdr10.h"
#include "shaders/colorspace_sdr_ui.h"
#include "shaders/colorspace_hdr_linear.h"
#include "shaders/colorspace_hdr10_pq.h"
#include "../pocs/sdf_fixed/sdf_fixed_hello_world.h"
#include "../pocs/mtsdf/mtsdf_hello_world.h"
#include "shaders/hsv_picker.h"
#include "../pocs/blue_wall_scene/blue_wall_v2_A.h"
#include "../pocs/blue_wall_scene/blue_wall_v2_B.h"
#include "../pocs/blue_wall_scene/blue_wall_v2_C.h"
#include "../pocs/blue_wall_scene/blue_wall_v2_D.h"
#include "../pocs/blue_wall_scene/blue_wall_v2_E.h"

#define SHADER_ENTRY(id, display_name, entry, buffers_init, color_space, features, width, height, blurb) \
    {#id, display_name, blurb, entry, buffers_init, color_space, features, width, height},

static const shader_desc_t g_shader_catalog[] = {
    SPHERE_TRACING_SHADER(SHADER_ENTRY)
    KINETIC_ORBS_SHADER(SHADER_ENTRY)
    ORBIT_STARS_SHADER(SHADER_ENTRY)
    ANIMATED_SPRITE_SHADER(SHADER_ENTRY)
    GLASS_LENSES_SHADER(SHADER_ENTRY)
    GLASS_DISKS_SHADER(SHADER_ENTRY)
    CRYSTAL_HALL_SHADER(SHADER_ENTRY)
    MASTER_CLASS_SHADER(SHADER_ENTRY)
    MASTER_CLASS_SCRGB_SHADER(SHADER_ENTRY)
    MASTER_CLASS_HDR10_SHADER(SHADER_ENTRY)
    COLORSPACE_SDR_UI_SHADER(SHADER_ENTRY)
    COLORSPACE_HDR_LINEAR_SHADER(SHADER_ENTRY)
    COLORSPACE_HDR10_PQ_SHADER(SHADER_ENTRY)
    SDF_FIXED_HELLO_WORLD_SHADER(SHADER_ENTRY)
    MTSDF_HELLO_WORLD_SHADER(SHADER_ENTRY)
    HSV_PICKER_SHADER(SHADER_ENTRY)
    BLUE_WALL_V2_A_SHADER(SHADER_ENTRY)
    BLUE_WALL_V2_B_SHADER(SHADER_ENTRY)
    BLUE_WALL_V2_C_SHADER(SHADER_ENTRY)
    BLUE_WALL_V2_D_SHADER(SHADER_ENTRY)
    BLUE_WALL_V2_E_SHADER(SHADER_ENTRY)
};

#undef SHADER_ENTRY

static const int g_shader_count = (int)(sizeof(g_shader_catalog) / sizeof(g_shader_catalog[0]));
static int       g_selected_shader_index = -1;
static int       g_active_shader_index = -1;

static int shader_find_index_by_id(const char *id)
{
    if (id == NULL || id[0] == '\0') {
        return -1;
    }

    for (int index = 0; index < g_shader_count; index++) {
        if (strcmp(g_shader_catalog[index].id, id) == 0) {
            return index;
        }
    }

    return -1;
}

static const shader_desc_t *shader_get_by_index(int index)
{
    if (index < 0 || index >= g_shader_count) {
        return NULL;
    }

    return &g_shader_catalog[index];
}

static const shader_desc_t *shader_get_catalog(int *count_out)
{
    if (count_out != NULL) {
        *count_out = g_shader_count;
    }

    return g_shader_catalog;
}

static int shader_get_selected_index(void)
{
    return g_selected_shader_index;
}

static const shader_desc_t *shader_get_selected_shader(void)
{
    return shader_get_by_index(g_selected_shader_index);
}

static const shader_desc_t *shader_get_active_shader(void)
{
    return shader_get_by_index(g_active_shader_index);
}

static void shader_select(int index)
{
    if (index >= 0 && index < g_shader_count) {
        g_selected_shader_index = index;
    }
}

static bool shader_execute_selected(void)
{
    if (g_selected_shader_index < 0 || g_selected_shader_index >= g_shader_count) {
        return false;
    }

    g_active_shader_index = g_selected_shader_index;
    return true;
}

static void shader_stop_active(void)
{
    g_active_shader_index = -1;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
    shader_host_callbacks_t shader_host = {0};
    host_options_t options = {0};
    char option_error[256];
    int default_shader_index;

    (void)hInstance;
    (void)hPrevInstance;
    (void)nShowCmd;
    (void)lpCmdLine;

    if (!host_options_parse(&options, option_error, sizeof(option_error))) {
        MessageBoxA(NULL, option_error, "Renderer", MB_OK | MB_ICONERROR);
        return 1;
    }

    default_shader_index = shader_find_index_by_id("sdf_fixed_hello_world");
    g_selected_shader_index = (default_shader_index >= 0) ? default_shader_index : 0;

    if (!window_create("Renderer", 1024, 1024, options.backend_kind)) {
        return 1;
    }

    shader_host.get_catalog = shader_get_catalog;
    shader_host.get_selected_index = shader_get_selected_index;
    shader_host.get_selected_shader = shader_get_selected_shader;
    shader_host.get_active_shader = shader_get_active_shader;
    shader_host.select_shader = shader_select;
    shader_host.execute_selected_shader = shader_execute_selected;
    shader_host.stop_active_shader = shader_stop_active;
    window_set_shader_host(&shader_host);

    window_run(30);
    return 0;
}
