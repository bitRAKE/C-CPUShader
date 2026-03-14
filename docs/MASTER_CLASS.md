# `master_class` Guide

## Purpose

This note explains what `master_class` is doing, why it converges more cleanly than the earlier 3D shaders, which parts are physically motivated, which parts are pragmatic, and where the best upgrade paths are.

The code lives in:

- `src/shaders/master_class.c`
- `src/shaders/master_class.h`

The newer HDR companions live in:

- `src/shaders/master_class_scrgb.c`
- `src/shaders/master_class_scrgb.h`
- `src/shaders/master_class_hdr10.c`
- `src/shaders/master_class_hdr10.h`

Those sibling shaders keep the overall room/object composition idea, but change the output contract:

- `master_class` stays SDR-display authored and tone-maps in-shader
- `master_class_scrgb` stays scene-linear and lets the backend present HDR
- `master_class_hdr10` encodes BT.2020 / ST.2084 in-shader and therefore avoids host-side temporal accumulation

## What This Shader Is

`master_class` is a compact CPU path tracer designed to be a stronger "final image" example than the earlier studies.

Its goals are:

- a static camera that works well with temporal accumulation
- a scene with more deliberate composition
- faster diffuse convergence through direct light sampling
- cleaner highlight presentation through a display transform
- enough material variety to be educational without becoming bloated

It is not a full physically based renderer.

It is a compact study that deliberately spends code in the places that matter most for image quality.

## Big Picture

At a high level, one pixel goes through this process:

```text
pixel + subpixel jitter
  -> camera ray
  -> nearest-hit query against room + objects + area light
  -> material response
       diffuse: direct-light sample + cosine hemisphere bounce
       metal: glossy reflected bounce
       glass: Fresnel reflection/refraction + Beer-Lambert attenuation
  -> optional Russian roulette after a few bounces
  -> accumulate radiance from light hits or sky
  -> ACES-like tone map
  -> gamma encode
  -> hand result back to the renderer
  -> renderer temporally accumulates across frames
```

That separation matters:

- transport reduces noise
- tone mapping improves presentation
- temporal accumulation improves sample count

Those are different jobs.

## Why It Converges Relatively Fast

This is the most important question.

The short answer is:

- the camera is static
- the scene is simple and readable
- diffuse bounces explicitly sample the light
- the light is large
- the display transform keeps bright energy visually manageable

### 1. The camera does not move

Unlike `crystal_hall`, `master_class` ignores `time` and keeps a fixed camera.

That means temporal accumulation is adding more samples of the same image, not blending different camera states together.

This is the single biggest reason it feels stable.

### 2. The shader uses subpixel jitter, not scene animation

Each frame jitters the sample point slightly inside the pixel footprint.

That gives:

- anti-aliasing
- Monte Carlo sample diversity
- convergence instead of motion smear

This is the good kind of temporal variation for an accumulated still image.

### 3. Diffuse surfaces use direct light sampling

The most important variance reduction step in the shader is explicit sampling of the ceiling area light from diffuse hits.

Without that, diffuse transport would depend much more heavily on randomly bouncing into the light by chance.

That is exactly the sort of thing that path tracing does poorly at low sample counts.

In practical terms, this is why the room illumination settles so much faster than the earlier "white emitter somewhere in the scene" studies.

### 4. The light is a finite area, not a tiny point-like target

The ceiling light covers a meaningful patch of the scene instead of being a tiny emissive bead.

That helps in two ways:

- direct-light samples have lower variance
- the lighting is softer and more forgiving perceptually

It is easier to converge a large coherent source than a tiny intense source.

### 5. The scene is composed from large analytic primitives

The room uses planes and a few spheres.

That means:

- intersections are cheap
- normals are exact
- there is little geometric clutter
- the dominant lighting paths are readable

Simple geometry is doing real work here. The shader is not wasting samples on accidental complexity.

### 6. Tone mapping helps the image look settled earlier

This is important:

tone mapping does not make the estimator converge faster.

What it does do is:

- compress very bright values
- keep highlights from blowing out into flat white slabs
- make residual variance less visually harsh

So the transport is doing the convergence work.
The tone map is helping the eye accept the result earlier.

### 7. Russian roulette is used conservatively

After the first few bounces, the shader uses throughput-based Russian roulette.

That keeps long low-value paths from consuming too much work while preserving unbiasedness in the Monte Carlo sense, as long as surviving paths are reweighted correctly.

In this shader, roulette is not the main reason it looks clean.
It is a cost-control mechanism that helps keep extra bounces affordable.

## Why It Looks Cleaner Than `crystal_hall`

`crystal_hall` is softer and noisier for structural reasons:

- it moves the camera over time
- it jitters the pixel each frame
- it accumulates those moving samples together
- it relies more on random transport and less on explicit direct-light sampling
- it uses a small bright emissive sphere, which is harder to sample well

`master_class` is better aligned with the accumulation model used by the host renderer.

That is not accidental. It was designed to fit the renderer instead of fighting it.

## Process Walkthrough

## 1. Camera and Sampling

The shader seeds a local RNG from pixel position and frame number, jitters the pixel, and builds a camera ray from a fixed origin and target.

This gives:

- stable framing
- anti-aliasing
- per-frame Monte Carlo diversity

Because the camera is fixed, the accumulation in `win.c` is genuinely helping instead of smearing.

## 2. Scene Query

The scene consists of:

- a rectangular ceiling light
- room planes
- a procedural floor
- a large glass sphere
- a warm glossy metal sphere
- a smaller diffuse sphere
- a second cooler metal accent sphere

This mix is intentional:

- diffuse response shows direct-light sampling clearly
- metal shows controlled specular reflection
- glass shows Fresnel plus transmission and absorption

## 3. Material Response

### Diffuse

Diffuse surfaces do two things:

- add a direct-light estimate from the area light
- continue the path with cosine-weighted hemisphere sampling

This is the main "quality per line of code" win in the shader.

### Metal

Metal reflects around the perfect mirror direction with a small roughness-driven perturbation.

This is a pragmatic glossy model, not a full microfacet BRDF.

It is simple, visually useful, and cheap.

### Glass

Glass uses:

- Schlick-style Fresnel branching
- reflection / refraction
- Beer-Lambert attenuation when a segment exits the medium

That gives a believable tinted glass effect without building a full spectral or volumetric system.

## 4. Direct-Light Logic

The shader samples a point on the area light, casts a shadow ray, and adds the contribution if the light is visible.

One subtle but important detail:

the code avoids double-counting emissive contribution after a diffuse bounce that already used explicit direct lighting.

That is what the `last_bounce_specular` flag is for.

In other words:

- diffuse paths get light through next-event estimation
- specular paths are still allowed to collect emitted radiance directly when they hit the light

That split is part of why the image is both cleaner and more numerically sensible than a naive hybrid.

## 5. Display Transform

The shader ends with:

- an ACES-like tone-mapping fit
- gamma encoding

This is deliberately part of the shader example rather than the common presentation path because the point of `master_class` is not only transport, but also a complete image-making recipe.

Important caveat:

this is not the full Academy ACES Output Transform.

It is a compact filmic approximation used to get much better highlight rolloff than raw linear display.

## Important Questions

## Does tone mapping make it converge faster?

No.

It makes the result look better at low sample counts, especially in highlights, but the variance reduction is coming from:

- direct light sampling
- stable accumulation
- scene design
- reasonable path termination

## Is this shader unbiased?

Not in the strict "full path tracer with exact PDFs and MIS everywhere" sense.

It is best described as a practical compact path tracer with physically motivated pieces.

The direct-light estimate and Russian roulette are standard Monte Carlo ideas, but the overall shader is still a handcrafted educational renderer, not a formal reference integrator.

## Why no MIS?

Because the current shader is trying to maximize clarity and value per line of code.

MIS would be one of the best next upgrades, especially if the shader grows more complex glossy response or more varied light transport.

## Why sample direct light only on diffuse hits?

Because that is where it gives the biggest quality payoff for the least complexity.

Specular paths have different sampling needs and quickly lead toward a more fully featured BSDF/integrator design.

## Why does the light use warm RGB values instead of white?

Because raw white emitters made the earlier 3D shaders look sterile and harsh.

The warm-biased light helps:

- color separation
- material readability
- highlight character
- overall scene mood

It is an artistic choice layered on top of physically motivated transport.

## Why does the glass still converge decently?

Because there is only one major glass object, the scene is otherwise simple, and the diffuse room lighting is already stabilized by direct-light sampling.

The shader is not trying to solve a room full of complex caustic objects at once.

## What This Shader Still Does Not Do

- no multiple importance sampling
- no microfacet BRDFs
- no depth of field
- no spectral transport
- no textured UV materials
- no BVH or complex geometry
- no denoiser
- no separate beauty / albedo / normal outputs
- no formal reference comparison mode

That list is not a criticism.

It is what keeps the shader readable.

## Best Directions For Advancement

## 1. Add debug views

The most valuable next upgrade is not another material.

It is debug output for:

- albedo
- normals
- direct-light term
- indirect-only term
- path depth
- roulette survival
- glass attenuation

This would make the shader much easier to teach from.

## 2. Add MIS

If the goal is quality rather than minimalism, MIS is the biggest integrator upgrade.

It would improve how BSDF-sampled and light-sampled paths are combined and reduce variance further.

## 3. Replace the glossy metal with a microfacet model

The current glossy lobe is a pragmatic hack around the reflection vector.

A real GGX-style model would make the material section more educational and more extensible.

## 4. Separate scene-linear output from display output

Right now `master_class` intentionally includes its own display transform.

That is good for the example, but if the renderer grows more serious, a better long-term structure is:

- shader returns scene-linear radiance
- presentation pass handles output transform
- debug views can inspect both

## 5. Add a reference mode

A slow mode with:

- more bounces
- no rough approximations
- more samples per frame

would make it easier to compare "pretty fast" against "more trusted."

## 6. Add a scene note beside the code

If the scene evolves, it may be worth documenting the compositional intent:

- why the glass sphere is left-of-center
- why the warm metal sits opposite it
- why the room colors are asymmetrical

That would keep future edits from flattening the image accidentally.

## Recommended Reading

For the main ideas behind this shader:

- *Physically Based Rendering: From Theory to Implementation*  
  Path tracing and Russian roulette overview: <https://pbr-book.org/4ed/Light_Transport_I_Surface_Reflection/Path_Tracing>
- *Physically Based Rendering*  
  Direct lighting motivation and estimator structure: <https://pbr-book.org/3ed-2018/Light_Transport_I_Surface_Reflection/Direct_Lighting>
- Zander Majercik, *The Schlick Fresnel Approximation*, in *Ray Tracing Gems II*  
  <https://www.researchgate.net/publication/354065225_The_Schlick_Fresnel_Approximation>
- ACES Documentation, Output Transforms  
  <https://docs.acescentral.com/system-components/output-transforms/>

## Practical Takeaway

`master_class` converges well not because it is doing something magical, but because it aligns several good decisions at once:

- stable camera
- meaningful sample jitter
- explicit direct lighting
- a large area source
- simple analytic geometry
- restrained material variety
- a display transform that preserves highlight character

That combination is the real lesson of the shader.

It is a rendering example that was designed around image quality and teaching value, not just around adding more effects.
