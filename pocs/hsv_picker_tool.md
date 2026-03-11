# HSV Picker Tool

This side quest is really a proof of a bigger idea: shader code should not be trapped inside one presentation backend.

The `hsv_picker` logic now exists in a form that can serve at least two audiences:

- the main CPU shader application, where it behaves like any other shader
- a small standalone Win32 tool, where it behaves like a UI component

That split is the point.

## What It Demonstrates

The standalone tool in `poc/hsv_picker_tool.c` reuses the picker shader logic from `src/shaders/hsv_picker.c` directly. It does not re-implement the math in a second place. Instead, it provides a tiny host application that:

- owns persistent picker state
- translates window mouse input into shader-style uniforms
- renders float RGBA pixels on the CPU
- packs them into `BGRA8`
- presents them with a GDI `StretchBlt`

This is useful because it shows that "shader development" can produce assets for more than a fullscreen demo or graphics toy. The same code can become a tool, editor widget, preview pane, test harness, or fallback renderer.

## The Architectural Win

The important separation is:

- `shader_uniforms_t`: frame-local inputs supplied by the host
- `hsv_picker_state_t`: persistent application-owned state

`shader_uniforms_t` carries the transient, renderer-owned values:

- `resolution`
- `time`
- `frame`
- `mouse`

`hsv_picker_state_t` carries the meaningful picker value:

- `hue`
- `sat`
- `val`

That is a healthy split. The shader gets a stable per-frame snapshot of the world, while the application remains responsible for anything that should persist across frames or be committed into the UI model.

## Why The Picker Is A Good Example

The HSV picker sits at a useful boundary:

- it is visual
- it is interactive
- it benefits from shader-style math
- it does not need a GPU to prove the concept

It is exactly the kind of thing that often gets rewritten several times:

- once in a shader toy
- once in a tool
- once in a UI component
- once again in a test utility

This project avoids that waste by letting the host application wrap the shader logic instead of cloning it.

## Reusable Pieces

The picker module exposes a small ladder of reuse:

- `hsv_picker_main`
  A plain shader entry point for the main app.
- `hsv_picker_render`
  Direct rendering with an explicit base state and active interaction region.
- `hsv_picker_hit_test`
  Lets a parent determine whether the mouse is on the hue ring or triangle.
- `hsv_picker_preview_state`
  Lets a parent ask "what would the picker state be under this mouse interaction?"
- `hsv_picker_rgb`
  Converts committed HSV state into display RGB.

That makes the module useful at several levels. A host can start by simply drawing it, then later add interaction, then later commit state into a larger application model.

## What The Standalone Tool Adds

The tool does not try to turn the shader into a full UI framework. It does only the minimum host work needed to make the picker feel real:

- maps client coordinates into render-space pixels
- tracks hover, press anchor, and drag position
- decides whether the interaction started on the hue ring or triangle
- updates preview state while dragging
- blits the translated surface to the window

That is a very transferable pattern.

For many shader-backed tools, the host application really only needs to answer four questions:

1. What is the current frame snapshot?
2. What state belongs to the application?
3. How do input events update that state?
4. How do the generated pixels get onto the screen?

Everything else can stay inside the shader module.

## Broader Utility

This same pattern can support a surprising number of tools:

- gradient editors
- curve editors
- palette designers
- LUT visualizers
- signed-distance-shape editors
- material preview widgets
- noise and pattern explorers
- 2D lighting prototyping tools
- teaching demos for rendering ideas

In all of those cases, the shader code is not "just an effect." It is the rendering brain of a useful application feature.

## A Good Development Habit

This side quest argues for a development style:

- keep the visual math together
- keep frame inputs generic
- keep persistent state outside the shader
- expose a few reusable helper functions around the shader core
- make presentation replaceable

If that discipline is followed, a shader can move between:

- the main experiment app
- a standalone tool window
- a CPU fallback path
- a different renderer
- an eventual GPU version

without being rewritten from scratch.

## Practical Takeaway

The HSV picker is modest, but the lesson is large:

write shaders as portable visual modules, not as code that only makes sense inside one demo.

That makes every shader a candidate for broader utility, and it gives the codebase a path from experimentation to actual tools.
