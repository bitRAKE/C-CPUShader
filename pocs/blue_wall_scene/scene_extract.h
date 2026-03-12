/* Generated from blue_wall.blend through Blender Python. */
#pragma once

typedef struct blue_wall_extract_camera_t {
    const char *name;
    float location[3];
    float rotation_euler[3];
    float lens_mm;
    float angle_x;
    float angle_y;
} blue_wall_extract_camera_t;

typedef struct blue_wall_extract_light_t {
    const char *name;
    const char *light_type;
    float location[3];
    float color[3];
    float energy;
    float size_x;
    float size_y;
} blue_wall_extract_light_t;

typedef struct blue_wall_extract_proxy_t {
    const char *name;
    const char *role;
    const char *proxy_hint;
    float world_center[3];
    float camera_center[3];
    float dimensions[3];
    float proxy_color[3];
    float visible_area;
    const char *texture_hint;
} blue_wall_extract_proxy_t;

static const blue_wall_extract_camera_t g_blue_wall_camera = {
    "Camera",
    { 0.0000f, -3.2265f, 1.2357f },
    { 1.5708f, -0.0000f, 0.0000f },
    20.0000f,
    1.4656f,
    1.0808f,
};

enum { BLUE_WALL_ENVIRONMENT_IMAGE_COUNT = 1 };
static const char *const g_blue_wall_environment_images[BLUE_WALL_ENVIRONMENT_IMAGE_COUNT] = {
    "hamburg_canal_1k.hdr",
};

enum { BLUE_WALL_LIGHT_COUNT = 11 };
static const blue_wall_extract_light_t g_blue_wall_lights[BLUE_WALL_LIGHT_COUNT] = {
    {
        "Window",
        "AREA",
        { 2.8028f, -1.9704f, 1.2062f },
        { 0.5538f, 0.6712f, 1.0000f },
        157.0796f,
        1.3200f,
        2.2010f,
    },
    {
        "Point",
        "POINT",
        { -1.3626f, -0.1463f, 1.8069f },
        { 1.0000f, 0.7041f, 0.3185f },
        3.0000f,
        0.0000f,
        0.0000f,
    },
    {
        "Point.001",
        "POINT",
        { -1.4028f, 0.0431f, 1.8069f },
        { 1.0000f, 0.7041f, 0.3185f },
        3.0000f,
        0.0000f,
        0.0000f,
    },
    {
        "Point.002",
        "POINT",
        { -1.5954f, 0.0023f, 1.8069f },
        { 1.0000f, 0.7041f, 0.3185f },
        3.0000f,
        0.0000f,
        0.0000f,
    },
    {
        "Point.003",
        "POINT",
        { -1.5536f, -0.1929f, 1.8069f },
        { 1.0000f, 0.7041f, 0.3185f },
        3.0000f,
        0.0000f,
        0.0000f,
    },
    {
        "Spot",
        "SPOT",
        { -1.3626f, -0.1463f, 1.8184f },
        { 1.0000f, 0.7041f, 0.3185f },
        20.0000f,
        0.0000f,
        0.0000f,
    },
    {
        "Spot.001",
        "SPOT",
        { -1.5505f, -0.1907f, 1.8184f },
        { 1.0000f, 0.7041f, 0.3185f },
        20.0000f,
        0.0000f,
        0.0000f,
    },
    {
        "Spot.002",
        "SPOT",
        { -1.5975f, 0.0067f, 1.8184f },
        { 1.0000f, 0.7041f, 0.3185f },
        20.0000f,
        0.0000f,
        0.0000f,
    },
    {
        "Spot.003",
        "SPOT",
        { -1.4020f, 0.0429f, 1.8184f },
        { 1.0000f, 0.7041f, 0.3185f },
        20.0000f,
        0.0000f,
        0.0000f,
    },
    {
        "Ceiling",
        "POINT",
        { 0.0000f, -3.1373f, 2.1007f },
        { 1.0000f, 0.8820f, 0.6921f },
        80.0000f,
        0.0000f,
        0.0000f,
    },
    {
        "Ceiling Spot",
        "SPOT",
        { 0.0000f, -1.9037f, 2.1007f },
        { 1.0000f, 0.8820f, 0.6921f },
        100.0000f,
        0.0000f,
        0.0000f,
    },
};

enum { BLUE_WALL_PROXY_COUNT = 18 };
static const blue_wall_extract_proxy_t g_blue_wall_proxies[BLUE_WALL_PROXY_COUNT] = {
    {
        "VerticalBookShelf_01.004",
        "bookshelf",
        "box_frame",
        { 2.2267f, -0.1072f, 0.9974f },
        { 6.1509f, -0.6583f, -8.6162f },
        { 0.6194f, 0.4340f, 1.9972f },
        { 0.5083f, 0.2626f, 0.0422f },
        0.113034f,
        "Wood030_4K_Color.jpg",
    },
    {
        "VerticalBookShelf_01.003",
        "bookshelf",
        "box_frame",
        { -2.1619f, -0.1073f, 0.9974f },
        { -5.9718f, -0.6583f, -8.6162f },
        { 0.6194f, 0.4340f, 1.9972f },
        { 0.5083f, 0.2626f, 0.0422f },
        0.111939f,
        "Wood030_4K_Color.jpg",
    },
    {
        "Sideboard_01",
        "sideboard",
        "box_stack",
        { -0.0000f, -0.0835f, 0.4308f },
        { -0.0000f, -2.2233f, -8.6819f },
        { 1.7654f, 0.4441f, 0.8616f },
        { 0.5083f, 0.2626f, 0.0422f },
        0.103423f,
        "Wood030_4K_Color.jpg",
    },
    {
        "CanvasPainting_01",
        "painting",
        "framed_textured_quad",
        { 0.0287f, 0.1373f, 1.7274f },
        { 0.0792f, 1.3584f, -9.2918f },
        { 1.5592f, 0.0450f, 1.0475f },
        { 0.3610f, 0.1339f, 0.1186f },
        0.080797f,
        "painting.jpg",
    },
    {
        "GreenChair_01.002",
        "chair",
        "chair_boxes",
        { 1.4889f, -0.1998f, 0.5287f },
        { 4.1127f, -1.9528f, -8.3605f },
        { 0.6731f, 0.6644f, 1.0585f },
        { 0.1651f, 0.1315f, 0.1009f },
        0.070977f,
        "GreenChair_01_diff_1k.jpg",
    },
    {
        "side_table_tall_01.001",
        "table",
        "cylinder_table",
        { -1.1141f, -0.0940f, 0.3806f },
        { -3.0774f, -2.3621f, -8.6527f },
        { 0.3840f, 0.3840f, 0.7612f },
        { 0.5131f, 0.5058f, 0.9800f },
        0.027824f,
        "side_table_tall_01_diff_1k.jpg",
    },
    {
        "WoodenTable_02.002",
        "table",
        "cylinder_table",
        { 1.1087f, -0.0052f, 0.2088f },
        { 3.0625f, -2.8364f, -8.8980f },
        { 0.3007f, 0.3007f, 0.4177f },
        { 0.2382f, 0.1508f, 0.1108f },
        0.010328f,
        "WoodenTable_02_diff_1k.jpg",
    },
    {
        "Ukulele_01.001",
        "instrument",
        "capsule_plus_boxes",
        { -1.7602f, 0.0924f, 0.2572f },
        { -4.8622f, -2.7028f, -9.1678f },
        { 0.1779f, 0.0508f, 0.5270f },
        { 0.5577f, 0.3741f, 0.2337f },
        0.008262f,
        "Ukulele_01_diff_1k.jpg",
    },
    {
        "potted_plant_04.001",
        "plant",
        "pot_plus_foliage_cluster",
        { -0.6761f, -0.1137f, 0.9954f },
        { -1.8677f, -0.6636f, -8.5983f },
        { 0.1703f, 0.1859f, 0.2676f },
        { 0.4358f, 0.3914f, 0.2892f },
        0.003370f,
        "potted_plant_04_diff_1k.jpg",
    },
    {
        "mantel_clock_01.001",
        "clock",
        "hero_prop_proxy",
        { -0.5119f, 0.0458f, 0.9423f },
        { -1.4140f, -0.8103f, -9.0390f },
        { 0.3423f, 0.1262f, 0.1605f },
        { 0.1664f, 0.1047f, 0.0773f },
        0.003190f,
        "mantel_clock_01_diff_1k.jpg",
    },
    {
        "Lantern_01.002",
        "lamp",
        "cylinder_with_emissive_core",
        { 0.7689f, -0.1558f, 1.0087f },
        { 2.1240f, -0.6269f, -8.4821f },
        { 0.1221f, 0.0972f, 0.2942f },
        { 0.2949f, 0.2562f, 0.2181f },
        0.002851f,
        "Lantern_01_brass_diff_1k.jpg",
    },
    {
        "horse_statue_01.001",
        "statue",
        "hero_prop_proxy",
        { -1.1166f, -0.0972f, 0.8711f },
        { -3.0843f, -1.0069f, -8.6439f },
        { 0.1638f, 0.1078f, 0.2200f },
        { 0.5114f, 0.5036f, 0.9956f },
        0.002835f,
        "horse_statue_01_diff_1k.jpg",
    },
    {
        "Camera_01_strap",
        "camera_prop",
        "hero_prop_proxy",
        { 0.1662f, -0.0602f, 0.8902f },
        { 0.4590f, -0.9542f, -8.7460f },
        { 0.2139f, 0.2628f, 0.0578f },
        { 0.2394f, 0.1723f, 0.1537f },
        0.001533f,
        "Camera_01_strap_diff_1k.jpg",
    },
    {
        "CheeseBox_01",
        "box_prop",
        "hero_prop_proxy",
        { 0.4881f, -0.1213f, 0.8939f },
        { 1.3483f, -0.9442f, -8.5773f },
        { 0.2404f, 0.1064f, 0.0640f },
        { 0.5273f, 0.4086f, 0.2980f },
        0.001187f,
        "CheeseBox_01_diff_1k.jpg",
    },
    {
        "CheeseBox_01_lid",
        "box_prop",
        "hero_prop_proxy",
        { 0.4166f, -0.0341f, 0.8991f },
        { 1.1508f, -0.9298f, -8.8182f },
        { 0.2385f, 0.1054f, 0.0039f },
        { 0.5273f, 0.4086f, 0.2980f },
        0.001067f,
        "CheeseBox_01_diff_1k.jpg",
    },
    {
        "Camera_01",
        "camera_prop",
        "hero_prop_proxy",
        { 0.2113f, -0.0317f, 0.9004f },
        { 0.5838f, -0.9261f, -8.8250f },
        { 0.1473f, 0.0888f, 0.0776f },
        { 0.2061f, 0.1923f, 0.1799f },
        0.000847f,
        "Camera_01_lens_body_diff_1k.jpg",
    },
    {
        "mantel_clock_01_glass",
        "clock",
        "hero_prop_proxy",
        { -0.5119f, -0.0102f, 0.9531f },
        { -1.4140f, -0.7805f, -8.8843f },
        { 0.1080f, 0.0043f, 0.1080f },
        { 0.1664f, 0.1047f, 0.0773f },
        0.000624f,
        "mantel_clock_01_diff_1k.jpg",
    },
    {
        "Lantern_01_glass.001",
        "lamp",
        "cylinder_with_emissive_core",
        { 0.7689f, -0.1558f, 0.9571f },
        { 2.1240f, -0.7694f, -8.4821f },
        { 0.0705f, 0.0705f, 0.0683f },
        { 0.5535f, 0.5069f, 0.9811f },
        0.000459f,
        "Lantern_01_brass_diff_1k.jpg",
    },
};
