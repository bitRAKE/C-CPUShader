# Fixed Grid SDF POC

This POC deliberately takes a lower-tech path than the MTSDF work.

## Goal

Build a text shader with:

- one plain SDF atlas
- no JSON processing
- a fixed ASCII map
- hardcoded sub-image math in the shader

The point is not maximum text quality. The point is to keep the pipeline simple enough that the shader can treat the atlas as a fixed array of glyph cells.

## Atlas Shape

- character set: printable ASCII `0x20` through `0x7E`
- atlas type: `sdf`
- grid: `16 x 6`
- cell size: `64 x 96`
- atlas size: `1024 x 576`

Glyph lookup is just:

- `space (0x20)` is advance-only and has no sampled atlas cell
- `index = codepoint - 33` for visible glyphs
- `column = index % 16`
- `row = index / 16`

## Regeneration

From the repo root:

```powershell
python pocs\sdf_fixed\generate_ascii_sdf_grid.py
```

That script only emits:

- `pocs/sdf_fixed/ascii_sdf_grid.png`

There is no generated metrics header and no JSON dependency in the runtime shader path.

## Files

- `pocs/sdf_fixed/generate_ascii_sdf_grid.py`
  - offline atlas generation
- `pocs/sdf_fixed/sdf_fixed_hello_world.c`
  - the fixed-grid shader
- `pocs/sdf_fixed/sdf_fixed_hello_world.h`
  - shader metadata and buffer init hook

## Source

- msdfgen: <https://github.com/Chlumsky/msdfgen>
