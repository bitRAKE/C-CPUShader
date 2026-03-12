# MTSDF Text POC

This POC demonstrates shader-owned text assets using an MTSDF atlas.

## Goal

Build a text shader the same way other static-asset shaders should work in this renderer:

- generate an atlas offline
- convert metrics into C-friendly data
- load the atlas once through `buffers_init()`
- render text entirely inside the shader

The first shader target is:

- `hello_world!`
- rainbow fill
- soft bubble outline
- no host-side per-frame text preprocessing
- giant animated glyph layers that exceed the viewport to stress MTSDF scaling

## Inputs

- Font:
  - `pocs/mtsdf/NotoSans[wdth,wght].ttf`
- Source:
  - <https://github.com/Chlumsky/msdfgen>

## Generated Outputs

- `pocs/mtsdf/ascii_mtsdf.png`
- `pocs/mtsdf/ascii_mtsdf.json`
- `pocs/mtsdf/ascii_mtsdf_font.h`

The atlas is generated for printable ASCII (`0x20` through `0x7E`) as MTSDF.

## Regeneration

From the repo root:

```powershell
python pocs\mtsdf\generate_ascii_atlas.py
```

That script:

- runs the local `msdf-atlas-gen` executable from the `msdfgen` toolchain
- emits the atlas image and JSON
- converts the metrics into a C header

## Local Support Files

- `pocs/mtsdf/mtsdf_text.h`
  - local MTSDF lookup, layout, and sampling helpers
- `pocs/mtsdf/mtsdf_hello_world.c`
  - the shader itself
- `pocs/mtsdf/mtsdf_hello_world.h`
  - shader metadata and buffer init hook

## Current Showcase

The live `mtsdf_hello_world` shader now does two things at once:

- a centered `hello_world!` foreground title
- a slow-moving layer of oversized glyphs behind it

Those background glyphs are intentionally large enough to spill beyond the screen bounds. That makes the POC a better flex for what MTSDF is good at: keeping edges stable even when a character becomes a scene-sized graphic instead of a small text label.

For now, all MTSDF-specific code stays inside this POC folder. If later text work proves some helpers to be truly reusable, that utility can be extracted afterward.

## Reference

- msdfgen: <https://github.com/Chlumsky/msdfgen>
