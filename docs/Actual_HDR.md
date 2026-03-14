# Actual HDR

This note exists to keep the host honest.

`A` already has:

- a CPU-owned `vec4_t` / float32 image
- backend selection through `--backend=dx12|ogl|vk|gdi`
- diagnostics when HDR is not actually present to the surface
- real DX12 and Vulkan HDR-oriented presentation attempts

That is a strong foundation, but it also creates the easiest place to become sloppy with language.

The host can:

- compute high-range float pixels
- upload high-range float pixels
- create a backend that is HDR-capable in theory

and still fail to present true HDR to the desktop surface.

This document is the standard the repo should use when it says "HDR".

## Executive Position

The shortest accurate summary is:

1. `ARGBF32` working data is necessary, but it is not enough.
2. Actual HDR is a property of the full output path, not just the shader output or upload path.
3. In this host today:
   - `dx12` is a real HDR candidate and can succeed on the right display/runtime path
   - `vk` is a real HDR candidate and can succeed when the surface exposes the needed format/colorspace pair
   - `ogl` currently reuses the shared DXGI/DirectX 12 presenter instead of proving a standalone WGL HDR path
   - `gdi` is intentionally SDR fallback
4. If the backend does not positively establish HDR at the surface, the host should say so plainly.

## The Four Questions

The easiest way to stay precise is to separate four different claims.

### 1. Is the source image HDR-capable?

If the renderer computes wide-range linear values in float, the source image is HDR-capable.

In `A`, yes. The runtime owns a CPU float32 frame surface and hands that completed image to the backend layer.

### 2. Is the internal backend path HDR-capable?

A backend can preserve float data internally without immediately collapsing to 8-bit SDR.

That is useful, but it still does not prove final HDR presentation.

### 3. Is the presentation surface HDR-capable?

This is the real gate. The swapchain or window surface must be created in a way that the OS, runtime, and driver recognize as HDR-capable.

This is where format selection, colorspace selection, and output probing matter.

### 4. Is the content being delivered in the correct output space?

Even a valid HDR surface can look wrong if the content is in the wrong space.

This is the source of the classic "washed out HDR" problem:

- shader output has already been tone-mapped or gamma-encoded for SDR
- backend treats it as scene-linear HDR content
- midtones and highlights get interpreted incorrectly

## What Counts As "Actual HDR" In This Repo

For `A`, the phrase should mean all of the following:

1. The renderer produced float32 high-range pixels.
2. The backend created an HDR-capable presentation surface.
3. The backend queried the actual output/surface capabilities.
4. The backend selected a supported HDR format/colorspace pair.
5. The host reports the actual result, not only the requested backend.
6. The shader/output-space contract is compatible with HDR presentation.

If any of those are missing, then the host may still be high-quality, but it is not yet "actual HDR".

## Current Backend Truth Table

Based on the current implementation:

| Backend | Current status | Why |
| --- | --- | --- |
| `dx12` | Real HDR candidate | Uses an FP16 swapchain path, probes the output, checks colorspace support, and reports whether HDR is actually present. |
| `vk` | Real HDR candidate | Queries the Win32 Vulkan surface and prefers `VK_FORMAT_R16G16B16A16_SFLOAT + VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT`; otherwise it reports SDR truthfully. |
| `ogl` | DXGI-backed today | Keeps a separate host-facing mode, but currently delegates final presentation to the shared DXGI/DirectX 12 path instead of proving HDR through a standalone WGL window surface. |
| `gdi` | SDR by design | Down-converts the float32 image to `BGRA8` and blits it. This is a fallback/debug presenter, not an HDR path. |

That table reflects current code, not only intention:

- DX12 status reporting comes from [src/dx12/backend_dx12.c](src/dx12/backend_dx12.c)
- Vulkan status reporting comes from [src/vk/backend_vk.c](src/vk/backend_vk.c)
- backend notices are funneled through [src/present/present_backend.c](src/present/present_backend.c)

## Current DX12 Path

The current DX12 implementation is the strongest Windows-side HDR path in the repo.

The important parts are:

- it creates a flip-model swapchain
- it now prefers `DXGI_FORMAT_R16G16B16A16_FLOAT`
- it checks colorspace support
- it attempts to set an HDR/scRGB-style colorspace
- it queries the containing output
- it records whether HDR is actually present to the surface

This is visible in [src/dx12/backend_dx12.c](src/dx12/backend_dx12.c):

- `swap_chain_desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT`
- `IDXGISwapChain4_CheckColorSpaceSupport`
- `IDXGISwapChain4_SetColorSpace1`
- `IDXGIOutput6::GetDesc1`

The host then uses that truth instead of assuming success.

That is the right shape.

## Current Vulkan Path

The Vulkan backend is also a real HDR attempt, not just "Vulkan happened to work."

The key behavior in [src/vk/backend_vk.c](src/vk/backend_vk.c) is:

- enumerate surface formats/colorspaces
- prefer `VK_FORMAT_R16G16B16A16_SFLOAT`
- prefer `VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT`
- report HDR only when that path is actually selected
- stay on Vulkan and report SDR when only SDR surface pairs are available

That is the correct mentality for Vulkan:

- query the real surface
- choose from what the surface exposes
- log what happened
- do not pretend HDR just because Vulkan initialized

## Current OpenGL Path

`ogl` is no longer a standalone WGL presentation experiment in this host.

The backend in [src/ogl/backend_ogl.c](src/ogl/backend_ogl.c) now delegates presentation to the shared DXGI/DirectX 12 path in [src/dx12/backend_dx12.c](src/dx12/backend_dx12.c). That means:

- `ogl` still exists as a user-selectable mode
- the host can still keep separate diagnostics and future room for an API-specific path
- actual presentation truth currently comes from the same DXGI surface/output logic used by `dx12`

This is deliberate. It keeps the host honest about Windows HDR: until a standalone WGL path is explicitly proven, `ogl` should not pretend to validate HDR on its own.

## Current GDI Path

GDI is intentionally a compatibility backend, not a quality/HDR reference backend.

The current implementation in [src/gdi/backend_gdi.c](src/gdi/backend_gdi.c):

- accepts the float32 final image
- down-converts it to `BGRA8`
- presents it with a classic GDI blit path

That makes GDI useful for:

- portability
- fallback behavior
- debugging "does the host still display anything?"

It does not make GDI an HDR backend.

The current host notice is therefore exactly right:

> GDI Blit down-converts the ARGBF32 image to BGRA8; HDR is not present to the surface.

## Why "Monitor HDR Mode" Is Not Enough

One of the easiest mistakes is to assume:

- Windows HDR is on
- therefore the app is presenting HDR

That is not enough.

The monitor being in HDR mode only means the desktop environment can support HDR presentation. The app still needs:

- the right swapchain/surface format
- the right colorspace
- a supported output/backend path

Without those, an app can still be presenting SDR content on an HDR desktop.

That is why the host diagnostics matter.

## Why Washed-Out HDR Happens

The washed-out look that appeared during the backend work is a very useful warning sign.

The likely cause is not "HDR is broken" in a vague sense. It is usually this:

- some shaders return display-referred SDR output
- some shaders return more scene-linear values
- the backend is now trying to preserve HDR range to the surface
- the renderer has not made output space explicit per shader

So SDR-style output gets interpreted as linear HDR-style output.

That produces:

- brighter than intended midtones
- flat-looking contrast
- an overall washed-out read

This means the next important architecture step is not just backend work. It is also shader/output-space metadata.

The host already has shader feature flags. It will likely need output-space flags too.

## Recommended Shader Output Policy

The clean policy for this repo is:

- working image: float32 scene-linear
- presentation backend: responsible for presentation-class handling
- shaders that already contain SDR tone mapping/gamma should declare that explicitly
- HDR-capable backends should not silently assume every shader returns scene-linear HDR-ready values

Without that distinction, the backend cannot do the right thing consistently.

## Why OpenGL Still Needs Careful Language

This is the place where the repo should be deliberately conservative.

There are two different statements:

1. OpenGL can process floating-point color data.
2. A standalone Windows WGL/OpenGL backend would need its own proven HDR desktop presentation path.

The first is true.
The second is not yet established.

That is why the repo should continue to treat:

- DX12 as the primary Windows HDR reference path
- Vulkan as the second serious HDR candidate
- `ogl` as a DXGI-backed mode today, not proof of standalone WGL HDR

## Why GDI Can Never Be More Than Fallback Here

GDI is not "almost HDR because the desktop is HDR."

It is a classic integer blit path. In this host it explicitly down-converts the float image to `BGRA8`.

That means it can:

- display the image
- help debugging
- remain widely compatible

It cannot preserve the renderer's HDR range to the surface.

## The Role Of Diagnostics

The host already moved in the right direction by surfacing backend notices into the dialog.

That should remain a hard rule:

- if HDR is not present to the surface, say so
- if a backend falls back, say so
- if a backend is HDR-capable and succeeded, say so
- do not force the user to infer HDR success from "it looked bright"

This is one of the strongest quality-of-engineering choices already present in the code.

## Recommended Ongoing Rules

These are the rules worth keeping.

### 1. Keep The Runtime Image In Float32

This repo is built around CPU shaders and float accumulation.

That makes `ARGBF32` / float32 the correct truth surface for the host.

### 2. Treat `f16x4` As An Optional Transport Shim

If bandwidth pressure appears later, a backend-local `f32 -> f16` path may help.

That should stay optional and backend-local. It should not replace the host's main working image.

### 3. Distinguish Requested Backend From Actual Presentation Result

`--backend=vk` does not mean HDR succeeded.

It means Vulkan was requested.

The host should always report:

- requested backend
- active backend
- whether HDR is actually present

### 4. Make Shader Output Space Explicit

This is the main missing piece if the goal is trustworthy HDR presentation across diverse shaders.

### 5. Treat OpenGL Claims Conservatively

Until a standalone Windows WGL presentation path is proven, keep `ogl` described as a DXGI-backed mode rather than as evidence that WGL HDR is solved.

## Actual HDR Checklist

When judging any backend in this repo, ask:

1. Is the final image float32 and high-range before presentation?
2. Is the backend using a surface format that can preserve HDR?
3. Did it select an HDR-capable colorspace?
4. Did it query the actual output/surface capabilities?
5. Did it report the real result?
6. Is the shader output in the right space for that presentation path?

If the answer to any of those is no, the backend may still be useful, but it should not be described as "actual HDR".

## Current Practical Reading Of The Repo

The current state of `A` can be summarized like this:

- the host architecture is already good enough for real HDR investigation
- the diagnostics policy is good
- DX12 and Vulkan are the only backends that should currently compete for "actual HDR"
- OpenGL wording should stay precise, and GDI should keep warning truthfully
- shader output-space metadata is the next major quality step

That is a strong place to be.

It means the repo is no longer asking "can we get float pixels onto the screen?" It is asking the better question:

> did the full path actually preserve HDR to the surface, and can the host prove it?

## Sources

Primary references used for the conclusions here:

- Microsoft Learn: [Use DirectX with Advanced Color on high/standard dynamic range displays](https://learn.microsoft.com/en-us/windows/win32/direct3darticles/high-dynamic-range)
- Microsoft Learn: [IDXGIOutput6::GetDesc1](https://learn.microsoft.com/en-us/windows/win32/api/dxgi1_6/nf-dxgi1_6-idxgioutput6-getdesc1)
- Microsoft Learn: [IDXGISwapChain3::CheckColorSpaceSupport](https://learn.microsoft.com/en-us/windows/win32/api/dxgi1_4/nf-dxgi1_4-idxgiswapchain3-checkcolorspacesupport)
- Microsoft Learn: [IDXGISwapChain3::SetColorSpace1](https://learn.microsoft.com/en-us/windows/win32/api/dxgi1_5/nf-dxgi1_5-idxgiswapchain3-setcolorspace1)
- Khronos Vulkan: [VkColorSpaceKHR](https://registry.khronos.org/vulkan/specs/latest/man/html/VkColorSpaceKHR.html)
- Khronos Vulkan: [VK_EXT_swapchain_colorspace](https://registry.khronos.org/vulkan/specs/latest/man/html/VK_EXT_swapchain_colorspace.html)
- Khronos WGL: [WGL_EXT_colorspace](https://registry.khronos.org/OpenGL/extensions/EXT/WGL_EXT_colorspace.txt)
- Khronos WGL: [WGL_ATI_pixel_format_float](https://registry.khronos.org/OpenGL/extensions/ATI/WGL_ATI_pixel_format_float.txt)
- Microsoft Learn: [StretchDIBits](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-stretchdibits)

## Final Standard

The standard the repo should use is:

> **Actual HDR is not a property of float pixels alone. It is a property of the full path from renderer, through presentation backend, through surface format and colorspace selection, to what the OS actually presents to the HDR display.**

That is the bar.
