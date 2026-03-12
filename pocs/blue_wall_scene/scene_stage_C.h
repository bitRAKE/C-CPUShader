/* Generated Stage C view for Blue Wall scene reconstruction. */
#pragma once

typedef struct blue_wall_stage_group_t {
    const char *id;
    const char *label;
    const char *proxy_family;
    float center[3];
    float dimensions[3];
} blue_wall_stage_group_t;

enum { BLUE_WALL_STAGE_C_GROUP_COUNT_V2 = 12 };
static const blue_wall_stage_group_t g_blue_wall_stage_C_groups_v2[BLUE_WALL_STAGE_C_GROUP_COUNT_V2] = {
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
    {
        "floor_lamp",
        "Floor Lamp",
        "floor_lamp_cluster",
        { -1.4771f, -0.0730f, 0.9498f },
        { 0.4188f, 0.4188f, 1.8997f },
    },
    {
        "bookshelf_left",
        "Bookshelf Left",
        "box_frame",
        { -2.1619f, -0.1072f, 0.9974f },
        { 0.6194f, 0.4341f, 1.9972f },
    },
    {
        "bookshelf_right",
        "Bookshelf Right",
        "box_frame",
        { 2.2267f, -0.1072f, 0.9974f },
        { 0.6194f, 0.4341f, 1.9972f },
    },
    {
        "sideboard",
        "Sideboard",
        "box_stack",
        { 0.0000f, -0.0834f, 0.4308f },
        { 1.7654f, 0.4441f, 0.8616f },
    },
    {
        "painting",
        "Painting",
        "framed_textured_quad",
        { 0.0287f, 0.1373f, 1.7274f },
        { 1.5592f, 0.0683f, 1.0483f },
    },
    {
        "chair",
        "Chair",
        "chair_boxes",
        { 1.4889f, -0.1998f, 0.5288f },
        { 0.9426f, 0.9415f, 1.0585f },
    },
    {
        "table_tall",
        "Tall Table",
        "cylinder_table",
        { -1.1141f, -0.0940f, 0.3805f },
        { 0.5430f, 0.5431f, 0.7611f },
    },
    {
        "table_small",
        "Small Table",
        "cylinder_table",
        { 1.1087f, -0.0053f, 0.2089f },
        { 0.3440f, 0.3441f, 0.4177f },
    },
    {
        "ukulele",
        "Ukulele",
        "capsule_plus_boxes",
        { -1.7603f, 0.0924f, 0.2572f },
        { 0.2457f, 0.2594f, 0.5266f },
    },
};
