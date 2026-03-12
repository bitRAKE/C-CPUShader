/* Generated Stage B view for Blue Wall scene reconstruction. */
#pragma once

typedef struct blue_wall_stage_group_t {
    const char *id;
    const char *label;
    const char *proxy_family;
    float center[3];
    float dimensions[3];
} blue_wall_stage_group_t;

enum { BLUE_WALL_STAGE_B_GROUP_COUNT_V2 = 3 };
static const blue_wall_stage_group_t g_blue_wall_stage_B_groups_v2[BLUE_WALL_STAGE_B_GROUP_COUNT_V2] = {
    {
        "room_shell",
        "Room Shell",
        "room_shell",
        { 0.1045f, -1.8360f, 1.3459f },
        { 5.3965f, 4.0254f, 2.6919f },
    },
    {
        "window_key_light",
        "Window Key Light",
        "area_light",
        { 2.8028f, -1.9704f, 1.2062f },
        { 0.0000f, 0.0000f, 0.0000f },
    },
    {
        "ceiling_fill",
        "Ceiling Fill",
        "light_cluster",
        { 0.0000f, -2.5205f, 2.1007f },
        { 0.0000f, 1.2336f, 0.0000f },
    },
};
