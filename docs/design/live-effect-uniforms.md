# Live effect uniforms — experimental implementation

The implementation is on branch `feature/realtime-effect-input`. It has
not yet been compiled or exercised on real hardware. The format and limits
below describe the current v1 prototype, not a stable upstream API.

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

## Current implementation constraints

- The IPC command is `effect-uniforms`, with `version=1`, `kind=screen`,
  `output` (current connector name) and a `uniforms` array.
- At most **four named values** per snapshot. Names must fit the renderer's
  31-character limit; GLSL identifiers only, and no `umbriel_` prefix.
  Each entry has exactly `name`, `type` and `value`.
- Each message is at most **4096 bytes** and contains complete state.
  Values must be finite, within the representable `float` range, and have
  the exact component count for their type.
- A dedicated producer connection must send at least one valid message
  every **250 ms**, even when its values do not change. It must wait for
  `{"ok":true}` before sending the next snapshot.
- A connection can own one output's live input. Disconnecting or exceeding
  the freshness limit releases the slot and requests a fallback frame.
- `umbriel_live_available` is bound to 1 while the producer is present,
  otherwise 0, when the selected shader declares it.
- Uniform capacity is shared with Umbriel's time and palette parameters.
  At most four external values are accepted so the usual built-ins and
  availability flag fit within the renderer's existing fixed-size array.
- No global effects setting is enabled automatically. The screen preset
  must be selected explicitly; the values are inert for unrelated shaders.

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

## Build and validation

Do not install the modified compositor over the working desktop until a
debug build passes:

~~~fish
cd ~/workspace/umbriel
git fetch origin
# Inspect and preserve local modifications before updating:
git status --short
git pull --ff-only
just configure debug
just debug
just test debug
~~~

A successful debug build only verifies compilation and unit tests; IPC
ownership, redraw pacing and disconnect behavior still need targeted
headless/nested integration checks and a hardware trial.

## Optional input mapping (experimental v2)

Version 1 remains unchanged: a producer may send typed screen uniforms only.
Version 2 additionally requires a nine-number `input_mapping` array: a
row-major homography from **visible output UV** to **original scene UV**,
both with a top-left origin.

~~~json
{
  "cmd": "effect-uniforms",
  "version": 2,
  "kind": "screen",
  "output": "DP-2",
  "uniforms": [{"name": "external_value", "type": "float", "value": 0.0}],
  "input_mapping": [1, 0, 0, 0, 1, 0, 0, 0, 1]
}
~~~

The example uses an identity mapping, independently of any hardware or effect.
For visible (u,v), the original source point is
`((m0*u+m1*v+m2)/w, (m3*u+m4*v+m5)/w)` where
`w=m6*u+m7*v+m8`. Invalid, singular, or non-finite matrices are rejected.
The pointer remains visually at its physical position; only the hit-testing
coordinates are transformed. Clicks outside the projected source are not
dispatched to hidden surfaces.

The compositor stores the mapping sampled with each rendered output buffer,
identified by its commit sequence. **Only a successful presentation** activates
the mapping. Incoming snapshots are never used for hit-testing until the
matching frame appears. Producer disconnect keeps the last visible mapping
until an unwarped fallback buffer is presented. A changed mapping triggers
a refresh of hover under a stationary mouse.

The implementation covers regular pointer movement, client clicks,
server-side decorations, tab bars, and ordinary implicit click grabs. Full
pointer-constraint and all data-device-drag paths, touch, and transformed
relative-pointer protocols still require integration testing.

The producer, not Umbriel, owns the effect's projection geometry and any
device-specific parameters. This is an experimental API, not yet upstream.
