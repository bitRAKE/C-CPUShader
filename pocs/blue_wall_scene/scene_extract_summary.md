# Blue Wall Scene Extract v2

Generated from `blue_wall.blend` through Blender plus host-side recipe processing.

## Scene Summary

- Scene: `Scene`
- Objects: `208` total, `194` meshes
- Render resolution: `2560 x 1440`
- Recipe groups: `13`

## Stage Groups

| Group | Stage | Proxy | Members | Bounds |
| --- | --- | --- | ---: | --- |
| `Room Shell` | `A` | `room_shell` | `7` | `[5.3965, 4.0254, 2.6919]` |
| `Window Key Light` | `A` | `area_light` | `1` | `[0.0, 0.0, 0.0]` |
| `Ceiling Fill` | `A` | `light_cluster` | `2` | `[0.0, 1.2336, 0.0]` |
| `Floor Lamp` | `C` | `floor_lamp_cluster` | `13` | `[0.4188, 0.4188, 1.8997]` |
| `Bookshelf Left` | `C` | `box_frame` | `1` | `[0.6194, 0.4341, 1.9972]` |
| `Bookshelf Right` | `C` | `box_frame` | `1` | `[0.6194, 0.4341, 1.9972]` |
| `Sideboard` | `C` | `box_stack` | `1` | `[1.7654, 0.4441, 0.8616]` |
| `Painting` | `C` | `framed_textured_quad` | `1` | `[1.5592, 0.0683, 1.0483]` |
| `Chair` | `C` | `chair_boxes` | `1` | `[0.9426, 0.9415, 1.0585]` |
| `Tall Table` | `C` | `cylinder_table` | `1` | `[0.543, 0.5431, 0.7611]` |
| `Small Table` | `C` | `cylinder_table` | `1` | `[0.344, 0.3441, 0.4177]` |
| `Ukulele` | `C` | `capsule_plus_boxes` | `1` | `[0.2457, 0.2594, 0.5266]` |
| `Sideboard Top Props` | `D` | `hero_prop_cluster` | `8` | `[2.0548, 0.3357, 0.3948]` |

## Light To Fixture Hints

### `Window`

- No nearby fixture candidates.

### `Point`

- `Cylinder.005` role `lamp_fixture` distance `0.0069` score `1.0`
- `Cylinder.004` role `lamp_fixture` distance `0.1855` score `1.0`
- `Cylinder.002` role `lamp_fixture` distance `0.1913` score `1.0`

### `Point.001`

- `Cylinder.004` role `lamp_fixture` distance `0.0085` score `1.0`
- `Cylinder.003` role `lamp_fixture` distance `0.1868` score `1.0`
- `Cylinder.005` role `lamp_fixture` distance `0.1927` score `1.0`

### `Point.002`

- `Cylinder.003` role `lamp_fixture` distance `0.0108` score `1.0`
- `Cylinder.002` role `lamp_fixture` distance `0.1883` score `1.0`
- `Cylinder.004` role `lamp_fixture` distance `0.1946` score `1.0`

### `Point.003`

- `Cylinder.002` role `lamp_fixture` distance `0.0126` score `1.0`
- `Cylinder.005` role `lamp_fixture` distance `0.1898` score `1.0`
- `Cylinder.003` role `lamp_fixture` distance `0.1961` score `1.0`

### `Spot`

- `Cylinder.005` role `lamp_fixture` distance `0.0134` score `1.0`
- `Cylinder.004` role `lamp_fixture` distance `0.1858` score `1.0`
- `Cylinder.002` role `lamp_fixture` distance `0.1916` score `1.0`

### `Spot.001`

- `Cylinder.002` role `lamp_fixture` distance `0.0153` score `1.0`
- `Cylinder.005` role `lamp_fixture` distance `0.1866` score `1.0`
- `Cylinder.003` role `lamp_fixture` distance `0.1948` score `1.0`

### `Spot.002`

- `Cylinder.003` role `lamp_fixture` distance `0.0182` score `1.0`
- `Cylinder.002` role `lamp_fixture` distance `0.1934` score `1.0`
- `Cylinder.004` role `lamp_fixture` distance `0.1963` score `1.0`

### `Spot.003`

- `Cylinder.004` role `lamp_fixture` distance `0.0143` score `1.0`
- `Cylinder.003` role `lamp_fixture` distance `0.1879` score `1.0`
- `Cylinder.005` role `lamp_fixture` distance `0.1927` score `1.0`

### `Ceiling`

- No nearby fixture candidates.

### `Ceiling Spot`

- `Room` role `room_shell` distance `0.765` score `0.388`

## Heuristic Group Candidates

| Candidate | Reason | Members |
| --- | --- | ---: |
| `name_root:Books_proxy` | `shared_name_root` | `159` |
| `name_root:Cornice` | `shared_name_root` | `3` |
| `name_root:Cylinder` | `shared_name_root` | `5` |
| `name_root:Point` | `shared_name_root` | `4` |
| `name_root:Skirting` | `shared_name_root` | `3` |
| `name_root:Spot` | `shared_name_root` | `4` |
| `name_root:VerticalBookShelf_01` | `shared_name_root` | `2` |
| `proximity:Magazine 175x254x10mm+Magazine 216x279x7.5mm` | `proximity_cluster` | `2` |
| `proximity:Point+Spot` | `proximity_cluster` | `8` |
| `proximity:Magazine 210x297x4mm+Magazine 231x297x5mm` | `proximity_cluster` | `2` |
| `proximity:Magazine 216x279x7.5mm+Magazine 231x297x5mm` | `proximity_cluster` | `2` |
| `proximity:Magazine 175x254x10mm+Magazine 231x297x5mm` | `proximity_cluster` | `2` |
| `proximity:Magazine 175x254x10mm+Magazine 210x297x4mm` | `proximity_cluster` | `2` |
| `proximity:Magazine 210x297x4mm+Magazine 216x279x7.5mm` | `proximity_cluster` | `2` |
| `proximity:Lantern_01+Lantern_01_glass` | `proximity_cluster` | `2` |
| `proximity:Books_proxy+VerticalBookShelf_01` | `proximity_cluster` | `161` |

## Top Stage C Targets

| Object | Role | Proxy | Stage C Score | Texture |
| --- | --- | --- | ---: | --- |
| `CanvasPainting_01` | `painting` | `framed_textured_quad` | `0.5633` | `painting.jpg` |
| `Sideboard_01` | `sideboard` | `box_stack` | `0.5576` | `Wood030_4K_Color.jpg` |
| `VerticalBookShelf_01.004` | `bookshelf` | `box_frame` | `0.5572` | `Wood030_4K_Color.jpg` |
| `VerticalBookShelf_01.003` | `bookshelf` | `box_frame` | `0.5568` | `Wood030_4K_Color.jpg` |
| `GreenChair_01.002` | `chair` | `chair_boxes` | `0.5495` | `GreenChair_01_diff_1k.jpg` |
| `Cylinder` | `lamp_fixture` | `floor_lamp_stand` | `0.5464` | `-` |
| `Cylinder.002` | `lamp_fixture` | `floor_lamp_box_shade` | `0.5281` | `-` |
| `Cylinder.005` | `lamp_fixture` | `floor_lamp_box_shade` | `0.5280` | `-` |
| `Cylinder.003` | `lamp_fixture` | `floor_lamp_box_shade` | `0.5279` | `-` |
| `Cylinder.004` | `lamp_fixture` | `floor_lamp_box_shade` | `0.5278` | `-` |
| `Ukulele_01.001` | `instrument` | `capsule_plus_boxes` | `0.5015` | `Ukulele_01_diff_1k.jpg` |
| `side_table_tall_01.001` | `table` | `cylinder_table` | `0.3766` | `side_table_tall_01_diff_1k.jpg` |
| `WoodenTable_02.002` | `table` | `cylinder_table` | `0.3701` | `WoodenTable_02_diff_1k.jpg` |
| `potted_plant_04.001` | `plant` | `pot_plus_foliage_cluster` | `0.3471` | `potted_plant_04_diff_1k.jpg` |
| `mantel_clock_01.001` | `clock` | `hero_prop_proxy` | `0.3430` | `mantel_clock_01_diff_1k.jpg` |
| `mantel_clock_01_glass` | `clock` | `hero_prop_proxy` | `0.3420` | `mantel_clock_01_diff_1k.jpg` |

## Output Notes

- `scene_extract_raw.json` is the direct Blender dump.
- `scene_extract.json` is the processed reconstruction-oriented dataset.
- `scene_extract_generated.h`, `scene_groups.h`, and `scene_stage_*.h` are generated C-facing outputs for future shader staging work.
- `scene_extract.h` is preserved as a compatibility header for the Blue Wall POC while helper extraction remains local to this folder.
