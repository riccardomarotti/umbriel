# Live effect uniforms — generic design proposal

## Purpose

External applications can supply low-latency numeric uniforms to effect
shaders: sensor data, motion controls, accessibility inputs, remote state,
reactive visualizations or custom animation parameters. The compositor must
not know the meaning or source of the values.

The existing audio-effect channel is specialized (64 bands, 8-bit precision,
60 updates/s). It must remain independent of this new interface.

## Proposed IPC protocol

A persistent Unix connection sends complete, typed snapshots:

~~~json
{
  "cmd": "effect-uniforms",
  "version": 1,
  "kind": "screen",
  "output": "DP-2",
  "uniforms": [
    {"name": "external_orientation", "type": "vec4", "value": [0, 0, 0, 1]},
    {"name": "external_scale", "type": "float", "value": 1.0}
  ]
}
~~~

The names and data are illustrative, not special-purpose. Types supported
initially: float, vec2, vec3, vec4. Strictly validate finite numbers,
component counts, GLSL identifier names and small bounded payloads. Reserve
the umbriel_ prefix for built-in uniforms. The existing fx_uniform mechanism
supports typed values without a new renderer abstraction.

Scope only to screen effects and one named output in v1; future kinds need
their own target/ownership contract. A built-in float uniform named
umbriel_live_available is 1 while a valid producer owns the values, 0
otherwise. A shader can use it to fall back to the original image.

## Lifetime and ownership

- One producer owns a given output's screen-effect input at a time.
- A second writer gets an explicit conflict, never overrides silently.
- The first accepted message claims the output for that connection.
- A disconnected, timed-out or locked producer loses its values and ownership.
- Disconnects, preset changes, hotplug and compositor reloads must not leave
  a stale transform running.
- The values do not themselves select or modify an effect preset.
- Snapshots are complete; the compositor retains the newest valid snapshot
  and coalesces intermediate messages between output frames.
- Updates receive acknowledgments and must not produce an unbounded queue.

## Rendering and latency

Inject the latest uniform values into the selected screen effect's
fx_animation_parameters on the target output immediately before rendering,
rather than storing them in an audio texture or relying on a fixed-rate
screen effect clock.

An incoming value requests a redraw; the output's refresh/vblank cadence
determines when it actually draws. Multiple input packets per refresh must
result in at most the latest values being used. Other outputs, workspaces,
window arrangement, Noctalia layers and unrelated effects stay untouched.

This removes the transport's 60 Hz ceiling but does NOT promise VR-like
asynchronous timewarp. Future timestamping, target presentation-time
prediction, or other reprojection should be general extensions.

## Example shader

~~~glsl
uniform vec4 external_orientation;
uniform float external_scale;
uniform float umbriel_live_available;

vec4 screen(vec2 uv) {
  if (umbriel_live_available < 0.5) return umbriel_sample(uv);
  // The effect decides what its inputs mean.
  return umbriel_sample(uv);
}
~~~

No headset type, vendor SDK, USB ID, field of view, coordinate convention,
recenter action, filtering or follow policy belongs inside Umbriel.
Consumers own those semantics.

## Implementation sequence

1. Add a bounded, tested IPC parser and explicit per-output ownership.
2. Connect live uniforms to the existing screen-effect parameter pipeline.
   Read the newest sample during each relevant output frame.
3. Test invalid input, concurrent producers, multiple outputs, disconnect
   fallback, session locks, hotplug, effect selection and high-frequency
   input coalescing.
4. Validate in nested/headless Umbriel before using a modified compositor
   as the primary desktop session.
5. Migrate an independent producer onto the new API and measure latency.

Cursor coordinate remapping, display filtering, virtual desktops, software
reprojection and soft follow are deliberately NOT part of this protocol.
