# Monitor Diagnostic

Nine pixel-precise test patterns for evaluating display quality.
Each shader targets a specific property of the panel, backlight, or
signal chain. Run them full-screen at native resolution with no
desktop scaling for meaningful results.

---

## RGB Gradient

**What it shows**
Four horizontal rows, each a full 0–255 ramp:

| Row | Channel |
|-----|---------|
| Bottom | Red only |
| 2 | Green only |
| 3 | Blue only |
| Top | Combined white ramp with subtle hue shift |

Small tick marks appear at every 32-level boundary along the top
edge of each row.

**What to look for**

- *Smooth ramps vs visible steps* — banding in any single channel
  means the panel cannot resolve 8-bit gradients on that primary.
- *Channel balance* — the combined row should read neutral gray at
  midpoint. A warm or cool cast reveals white-point offset.
- *Shadow crush* — the leftmost ~5 % of each row should show
  distinct dark tones, not solid black. If they vanish, brightness
  or black-level is set too high.
- *Highlight clip* — the rightmost ~5 % should not blow out to flat
  white. If it does, contrast or backlight is too aggressive.

---

## HSV Wheel

**What it shows**
A full hue/saturation disc. Saturation increases from center (white)
to edge (fully saturated). Two concentric reference rings sit at
r ≈ 0.55 (V = 0.5) and r ≈ 0.92 (V = 1.0). Six radial lines and
dots mark the primary (R / G / B) and secondary (C / M / Y) hues.

**What to look for**

- *Hue uniformity* — transitions between adjacent hues should be
  smooth. Abrupt bands suggest quantization or a narrow gamut
  clipping certain hues.
- *Saturation falloff* — the gradient from edge to center should be
  continuous. A visible ring or plateau means the panel's gamma
  curve distorts low-saturation colors.
- *Primary dots* — compare each dot's color to its radial slice.
  They should match. A mismatch means the panel's primaries don't
  align with sRGB.
- *Center white* — should appear perfectly neutral, not warm or cool.

---

## Banding Steps

**What it shows**
A 6 × 4 grid. Rows (bottom to top) use increasing quantization
levels: 8, 16, 32, 64, 128, 256. Columns (left to right): gray,
red, green, blue.

**What to look for**

- *How far up can you see steps?* — The 256-level row (top) should
  look perfectly smooth. If it doesn't, the panel has fewer than
  8 bits of effective depth.
- *Per-channel differences* — some VA and TN panels resolve green
  better than blue. Compare the blue column to the green column at
  the 64-level row.
- *Dithering artifacts* — temporal or spatial dithering (FRC) may
  appear as faint shimmer or noise in the higher rows. This is
  normal for 6-bit+FRC panels but should be invisible on true 8-bit.
- *Row 5 (128 levels)* — this is the litmus test for 7-bit vs 8-bit
  panels. If you can still see faint steps here, the panel is 8-bit.
  If it looks smooth at 128 but stepped at 256, it's likely 7-bit+FRC.

---

## Gamma Ramp

**What it shows**
Five horizontal rows, each applying a different transfer function to
a linear 0–1 input:

| Row | Transfer |
|-----|----------|
| Bottom | Linear (γ 1.0) |
| 2 | sRGB (piecewise, ~γ 2.2) |
| 3 | γ 1.8 (classic Mac) |
| 4 | γ 2.2 (CRT reference) |
| 5 (top) | γ 2.6 (DCI cinema) |

The bottom 20 % of each row is a 1 × 1 checkerboard alternating
black and the ramp value. A thin gray line separates the
checkerboard from the smooth ramp above it.

**What to look for**

- *Gamma match* — find the row where the checkerboard strip blends
  most seamlessly into the smooth ramp above it at the horizontal
  midpoint. That row's gamma is closest to your display's actual
  gamma. Most desktop monitors should match row 2 (sRGB) or row 4
  (γ 2.2). If row 3 (1.8) matches better, your display runs bright.
- *Shadow detail* — the bottom (linear) row should look distinctly
  darker in its left half than the other rows. If it doesn't, your
  display's black level may be elevated.
- *Highlight separation* — in the top (γ 2.6) row, the right quarter
  should still show a visible gradient, not flat white. Clipping
  here means the contrast ratio is too high for the panel.

---

## Motion Response

**What it shows**
Four horizontal lanes on a dark background. Each lane has a colored
block moving left-to-right at a fixed integer speed:

| Lane | Color | Speed |
|------|-------|-------|
| Bottom | White | 1 px/frame |
| 2 | Red | 2 px/frame |
| 3 | Green | 4 px/frame |
| 4 (top) | Blue | 8 px/frame |

A dimmer static reference block sits at the left edge of each lane.
Vertical grid lines every 120 pixels provide position landmarks.

**What to look for**

- *Ghosting / smearing* — a visible trail behind the moving block
  means the panel's pixel response time is slower than the frame
  interval. The faster lanes (green, blue) stress this harder.
- *Color-dependent response* — compare ghosting across lanes.
  Many VA panels ghost more on dark-to-light transitions than
  light-to-dark, and blue transitions are often slowest.
- *Overshoot* — a bright halo *ahead* of the block (opposite the
  trail) indicates aggressive overdrive in the monitor's firmware.
- *Reference comparison* — the static block in each lane shows what
  the color should look like at rest. If the moving block appears
  dimmer or tinted differently, response-time artifacts are altering
  perceived color.

---

## Strobe Flicker

**What it shows**
Six vertical columns, each alternating black and white at a
different frame period:

| Column | Period | At 60 Hz |
|--------|--------|----------|
| 1 (left) | every frame | 60 Hz flicker |
| 2 | 2 frames | 30 Hz |
| 3 | 3 frames | 20 Hz |
| 4 | 4 frames | 15 Hz |
| 5 | 6 frames | 10 Hz |
| 6 (right) | 8 frames | 7.5 Hz |

The top half shows normal polarity (black → white). The bottom half
is inverted (white → black). A narrow center strip strobes at
period-2 across the full width.

**What to look for**

- *Flicker perception threshold* — the left columns (higher
  frequency) should appear as steady gray. The point where you
  start seeing distinct flashes reveals your flicker fusion
  threshold for this display's brightness.
- *PWM backlight interaction* — if columns that should look steady
  instead show a beating or moire pattern, the backlight is using
  PWM dimming that interferes with the frame strobing.
- *Persistence* — compare top half to bottom half at the same
  column. If one polarity appears brighter, the panel has
  asymmetric response (common on IPS — black-to-white is faster
  than white-to-black).
- *Center strip* — a pure reference strobe. If it appears
  significantly brighter or dimmer than the period-2 column above
  and below it, the column separators may be influencing your
  perception.

---

## Sub-pixel Grid

**What it shows**
A 4 × 3 grid of checkerboard test zones:

| | 1 px spacing | 2 px spacing | 4 px spacing |
|---|---|---|---|
| **Top** | Blue | Blue | Blue |
| **3** | Green | Green | Green |
| **2** | Red | Red | Red |
| **Bottom** | White | White | White |

Each zone fills with a checkerboard at the labeled pixel spacing
using only the labeled color channel (or all three for white).

**What to look for**

- *Scaling detection* — the 1 px column (left) should appear as
  uniform 50 % intensity. If you see a moire pattern, the display
  is not running at native resolution or Windows scaling is active.
  The 2 px and 4 px columns should show clean, sharp checks.
- *Dead / stuck pixels* — in the 1 px white zone, a single stuck
  sub-pixel appears as a colored dot against the gray field. In the
  per-channel zones, a dead sub-pixel appears as a dark dot.
- *Sub-pixel layout* — in the 4 px zones, the individual colored
  squares should be crisp and square. If they appear rectangular
  or have colored fringes, sub-pixel rendering or ClearType may be
  interfering.
- *Channel uniformity* — compare the perceived brightness of the
  three per-channel 1 px zones. They should all appear as roughly
  equal mid-tones. If blue appears significantly darker, the panel
  has lower blue luminance (common on wide-gamut displays).

---

## HDR Clipping

> *Requires an HDR display with HDR enabled in Windows Display Settings.
> These shaders output scene-linear values through an scRGB swap chain.
> On an SDR display they will be grayed out in the treeview (once the
> shader policy is in place) or will appear as flat white.*

**What it shows**
A luminance step ladder spanning the useful HDR range. Twelve
rectangular patches per row, each at a fixed linear intensity:

| Patch | Linear | ≈ Nits | Role |
|-------|--------|--------|------|
| 1 | 0.05 | 4 | Near-black |
| 2 | 0.18 | 14 | 18 % photographic gray |
| 3 | 0.50 | 40 | Dim midtone |
| 4 | 1.00 | 80 | SDR reference white |
| 5 | 1.50 | 120 | |
| 6 | 2.00 | 160 | |
| 7 | 2.54 | 203 | SDR content white (in HDR context) |
| 8 | 4.00 | 320 | |
| 9 | 6.00 | 480 | |
| 10 | 8.00 | 640 | |
| 11 | 10.00 | 800 | |
| 12 | 12.50 | 1000 | Typical HDR peak |

Four rows present this data different ways:

- **Top / bottom strips** — smooth cubic gradient from 0 to 12.5 linear.
- **Gray patches** — the twelve steps as neutral gray.
- **Warm patches** — the same twelve steps with a warm tint, to test
  that HDR luminance applies equally to colored surfaces.
- **SDR reference** — the same twelve steps, clamped to 1.0. This row
  is the control: on an SDR display, all rows look like this one.

**What to look for**

- *Is HDR active?* — compare the gray row to the SDR reference row.
  If they look identical, HDR is not reaching the panel. Check that
  Windows HDR is enabled and the application is using the DX12 backend.
- *Peak luminance* — walk along the gray patches from left to right.
  The point where two adjacent patches stop looking different marks
  the display's effective peak brightness. A 400-nit display will
  merge patches above ~5.0 linear; a 1000-nit display should
  differentiate all twelve.
- *Tonal separation in the shadows* — patches 1–3 (0.05, 0.18, 0.50)
  should be clearly distinct against the near-black background. If
  they blend together, the display's black level is too high or its
  HDR tone curve crushes shadows.
- *Warm vs gray* — at each luminance level, the warm and gray patches
  should appear equally bright but with different hue. If the warm
  patches look dimmer or brighter, the display's HDR processing has
  a color-dependent luminance response.

---

## HDR Wide Gamut

**What it shows**
Three rows of six saturated color patches — red, green, blue, cyan,
magenta, yellow — plus a D65 white reference on the right. Each row
uses a different color gamut expressed through scRGB negative values:

| Row | Gamut | Primaries | scRGB range |
|-----|-------|-----------|-------------|
| Top | BT.709 | Standard sRGB | [0, 1] only |
| Middle | DCI-P3 | Cinema wide-gamut | Small negatives |
| Bottom | BT.2020 | Ultra-wide-gamut | Large negatives |

scRGB extends the BT.709 coordinate system beyond [0, 1]. Colors
more saturated than BT.709 primaries are encoded with negative values
on the other channels. For example, DCI-P3's green needs negative R
and negative B to express its position outside the BT.709 triangle.
BT.2020's green needs even larger negatives.

When these values pass through the scRGB swap chain, the OS compositor
maps them to whatever gamut the panel physically supports. Negative
values that fall outside the panel's gamut are clipped at the display
boundary — which is exactly what this test measures.

**What to look for**

- *Row-to-row difference* — if all three rows look identical, the
  display covers only BT.709 (or HDR is not active, and negatives are
  clamping to zero). This is normal for most consumer sRGB monitors.
- *P3 vs BT.709* — on a wide-gamut display (most modern laptops and
  many gaming monitors), the middle row's patches should appear
  noticeably more vivid than the top row. Red becomes deeper, green
  more emerald, blue more electric. The difference is the proof that
  the panel's gamut extends beyond sRGB.
- *BT.2020 vs P3* — very few consumer panels cover BT.2020 fully. On
  a P3-class display, the bottom row will look slightly more saturated
  than the middle row in some hues (particularly green) but may not
  differ much in others. On a true wide-gamut reference monitor, the
  difference is dramatic.
- *White reference patches* — the rightmost patch in each row is
  D65 white at 1.0 (80 nits). All three should look identical — gamut
  affects chromaticity, not the white point. If the white patches
  differ, something is wrong with the color pipeline.
- *Hue shifts* — compare the same column across rows. P3 red should
  be a deeper, more orange-shifted red than BT.709 red. If it shifts
  toward magenta instead, the display's color management may be
  misinterpreting the scRGB negatives.

A thin tinted label strip on the left edge of each row helps identify
them: neutral for BT.709, warm for P3, cool for BT.2020.

---

## Tips

- Run at **native resolution, 100 % scaling** — any interpolation
  defeats the pixel-precision tests.
- Use the `--scale=1` flag or press the window to full-screen so
  the shader output maps 1:1 to display pixels.
- Disable night-light / f.lux / blue-light filters before running
  color tests.
- Let the display warm up for 15–20 minutes before judging color
  accuracy.
- The static tests (gradient, wheel, banding, gamma, sub-pixel) can
  be captured with `--capture` for documentation. The animated tests
  (motion, strobe) are best evaluated live.
- The HDR tests (clipping, wide gamut) require **HDR enabled in
  Windows Display Settings** and the **DX12 backend**. If HDR is not
  available, the shaders will still render but everything above 1.0
  clips and all three gamut rows collapse to BT.709 — which is itself
  a useful observation.
- HDR captures produce EXR files (scene-linear float). Use
  `tools\exr_view.exe` to view them on an HDR display, or open in
  a color-managed application like GIMP or Nuke.
