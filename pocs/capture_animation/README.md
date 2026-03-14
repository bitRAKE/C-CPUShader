# Capture Animation POC

This POC closes the loop on deterministic animation work in the host:

- frame-driven shaders
- sequential PNG capture from the dialog
- a Python tool that merges the sequence into a single animation
- an `ffmpeg` workflow for turning captured frames into common animation formats

The immediate goal is to make animation capture practical while keeping the host-side export path simple and authoritative: capture PNG frames first, then assemble richer animation formats offline.

## Files

- `pocs/capture_animation/animated_sprite.c`
- `pocs/capture_animation/animated_sprite.h`
- `pocs/capture_animation/orbit_stars.c`
- `pocs/capture_animation/orbit_stars.h`
- `pocs/capture_animation/png_sequence_to_apng.py`

## Shaders

`animated_sprite` is a transparent, frame-driven sprite study:

- output size: `128 x 64`
- update source: `uniforms->frame`
- color space: `SDR display`
- alpha: dynamic, including the sprite body, exhaust, and shadow
- exact loop length: `48 frames`

It is intentionally deterministic so a captured frame sequence is stable regardless of how quickly the machine renders it.

`orbit_stars` is the companion low-resolution motion study:

- output size: `128 x 64`
- update source: `uniforms->frame`
- color space: `SDR display`
- alpha: dynamic, including the stars and their trails
- exact loop length: `30 frames`

It is the cleaner “diagnostic motion target,” while `animated_sprite` is the stronger “asset-like animation target.”

## Capture Workflow

1. Launch `bin.exe`.
2. Select either `Animated Sprite` or `Orbit Stars`.
3. Set the small capture-count field beside `Capture` to the number of frames you want.
4. Click `Capture` while idle to arm the next run from frame `0`.
5. Click `Execute`.

The host writes PNG frames to `captures/`.

Suggested counts:

- `Animated Sprite`: capture `48` frames for one full loop
- `Orbit Stars`: capture `30` frames for one full loop

Because `Capture` can be armed while idle, the next `Execute` starts from frame `0`. That makes these loop counts predictable.

## Merge Workflow

From the repo root:

```powershell
python pocs\capture_animation\png_sequence_to_apng.py `
  --input-glob "captures\*animated_sprite*.png" `
  --last 48 `
  --fps 12 `
  --output "pocs\capture_animation\animated_sprite.apng"
```

For the star loop:

```powershell
python pocs\capture_animation\png_sequence_to_apng.py `
  --input-glob "captures\*orbit_stars*.png" `
  --last 30 `
  --fps 24 `
  --output "pocs\capture_animation\orbit_stars.apng"
```

The script:

- reads the matching PNG frames in lexicographic order
- validates that width, height, bit depth, and PNG metadata match
- preserves alpha
- preserves the source PNG bit depth, including `16-bit RGBA` captures
- writes an animated PNG without requiring Pillow or other third-party Python packages

## `ffmpeg` Workflow

The most reliable Windows workflow is:

1. capture one complete loop
2. copy just that run into a clean staging directory
3. rename the frames into `frame_0000.png`, `frame_0001.png`, ...
4. let `ffmpeg` assemble the result

Example staging step for `orbit_stars`:

```powershell
$src = Get-ChildItem captures\*orbit_stars*.png | Sort-Object Name | Select-Object -Last 30
$dst = "pocs\capture_animation\work\orbit_stars"
New-Item -ItemType Directory -Force -Path $dst | Out-Null
Remove-Item "$dst\frame_*.png" -ErrorAction SilentlyContinue
for($i = 0; $i -lt $src.Count; $i++) {
    Copy-Item $src[$i].FullName (Join-Path $dst ("frame_{0:D4}.png" -f $i))
}
```

If you specifically need an animated GIF, `ffmpeg` can build one from the captured PNG frames:

```powershell
ffmpeg.exe `
  -y `
  -framerate 24 `
  -i "pocs\capture_animation\work\orbit_stars\frame_%04d.png" `
  -vf "format=rgba,split[s0][s1];[s0]palettegen=reserve_transparent=1[p];[s1][p]paletteuse" `
  "pocs\capture_animation\orbit_stars.gif"
```

Or a looping WebP with alpha:

```powershell
ffmpeg.exe `
  -y `
  -framerate 24 `
  -i "pocs\capture_animation\work\orbit_stars\frame_%04d.png" `
  -loop 0 `
  -lossless 1 `
  "pocs\capture_animation\orbit_stars.webp"
```

For `animated_sprite`, use the same staging flow but `48` frames instead of `30`, and a lower playback rate if you want the motion to read more like a hand-authored sprite loop:

```powershell
$src = Get-ChildItem captures\*animated_sprite*.png | Sort-Object Name | Select-Object -Last 48
$dst = "pocs\capture_animation\work\animated_sprite"
New-Item -ItemType Directory -Force -Path $dst | Out-Null
Remove-Item "$dst\frame_*.png" -ErrorAction SilentlyContinue
for($i = 0; $i -lt $src.Count; $i++) {
    Copy-Item $src[$i].FullName (Join-Path $dst ("frame_{0:D4}.png" -f $i))
}
```

```powershell
ffmpeg.exe `
  -y `
  -framerate 12 `
  -i "pocs\capture_animation\work\animated_sprite\frame_%04d.png" `
  -vf "format=rgba,split[s0][s1];[s0]palettegen=reserve_transparent=1[p];[s1][p]paletteuse" `
  "pocs\capture_animation\animated_sprite.gif"
```

Notes:

- GIF remains available through `ffmpeg` if you need legacy compatibility, but it is palette-limited and SDR-only.
- WebP or APNG preserve the look of these alpha-heavy shaders better and are the preferred single-file outputs here.
- The staging directory keeps one capture run isolated, which avoids accidentally mixing multiple runs that happen to share the same shader name.
- `ffmpeg` can convert APNG to WEBP as well: `ffmpeg -f apng -i input.png -c:v libwebp_anim -lossless 1 -loop 0 output.webp`

## Why APNG

APNG fits this repo well because:

- the host already captures PNG
- alpha is preserved naturally
- `8-bit` and `16-bit` PNG frame paths can stay intact
- the merge step can stay simple and deterministic

That makes it a better first merge target here than pushing the host toward legacy in-process animation formats.

## Direction

For now, the sprite shader and APNG merge logic stay local to this POC.

If later animation work proves some pieces reusable, the most likely extractions are:

- generic frame-sequence naming helpers
- generic APNG assembly tooling
- more sprite-oriented shader helpers
- more finite-loop animation helpers for frame-driven shader studies
