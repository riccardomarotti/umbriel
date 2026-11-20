#!/usr/bin/env bash
# harness: outputs=1
# A screen-effect homography must redirect real client clicks to displayed content.
set -euo pipefail
source "$UMBRIEL_HARNESS_LIB"
cat > "$UMBRIEL_RUNTIME_DIR/mapping.glsl" <<'GLSL'
uniform float external_scale;
uniform float umbriel_live_available;
vec4 screen(vec2 uv) {
  if (umbriel_live_available < 0.5) return umbriel_sample(uv);
  vec2 source = 0.5 + (uv - 0.5) / external_scale;
  if (any(lessThan(source, vec2(0.0))) || any(greaterThan(source, vec2(1.0)))) {
    return vec4(0.0, 0.0, 0.0, 1.0);
  }
  return umbriel_sample(source);
}
GLSL
cat >> "$UMBRIEL_CONFIG" <<'TOML'
[animation]
enabled = false
[input.focus]
follows_mouse = "off"
[effects]
screen = "mapping"
in_capture = true
max_fps = 0
[effects.preset.mapping]
kind = "screen"
shader = "mapping.glsl"
TOML
"$UMBRIEL" msg config-reload > /dev/null
"$UMBRIEL" settle > /dev/null
log="$UMBRIEL_RUNTIME_DIR/mapping-client.log"
env LOG_POINTER=1 "$UMBRIEL_SEAT_LOG_CLIENT" mapped-client > "$log" 2>&1 &
for _ in $(seq 80); do
  "$UMBRIEL" windows --json | jq -e '.[] | select(.title == "mapped-client")' > /dev/null && break
  sleep 0.05
done
"$UMBRIEL" settle > /dev/null
python3 - <<'PY'
import json
import os
import socket
import subprocess
import threading
import time
from pathlib import Path

exe = os.environ['UMBRIEL']
log = Path(os.environ['UMBRIEL_RUNTIME_DIR']) / 'mapping-client.log'

def run(*args):
    return subprocess.check_output(args, text=True, timeout=10)

def send(sock, zoom, cursor_with_screen=False):
    k = 1 / zoom
    origin = 0.5 * (1 - k)
    request = {
        'cmd': 'effect-uniforms', 'version': 2, 'kind': 'screen', 'output': 'HEADLESS-1',
        'uniforms': [{'name': 'external_scale', 'type': 'float', 'value': zoom}],
        'input_mapping': [k, 0, origin, 0, k, origin, 0, 0, 1],
    }
    if cursor_with_screen:
        request['cursor_with_screen'] = True
    sock.sendall((json.dumps(request) + '\n').encode())
    data = b''
    while not data.endswith(b'\n'):
        part = sock.recv(512)
        assert part, 'producer disconnected'
        data += part
    assert json.loads(data) == {'ok': True}

with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as producer:
    producer.settimeout(3)
    producer.connect(os.environ['UMBRIEL_SOCKET'])
    zoom = 0.65
    send(producer, zoom)
    stop = threading.Event()
    def feed():
        while not stop.wait(0.04):
            send(producer, zoom)
    worker = threading.Thread(target=feed, daemon=True)
    worker.start()
    time.sleep(0.3)
    outputs = json.loads(run(exe, 'outputs', '--json'))
    width = outputs[0]['logical_size']['width']
    height = outputs[0]['logical_size']['height']
    views = json.loads(run(exe, 'windows', '--json'))
    view = next(w for w in views if w['title'] == 'mapped-client')
    x = view['x'] + int(view['w'] * 0.7)
    y = view['y'] + int(view['h'] * 0.4)
    visual_x = round(width * (0.5 + (x / width - 0.5) * zoom))
    visual_y = round(height * (0.5 + (y / height - 0.5) * zoom))
    run(os.environ['UMBRIEL_POINTER_CLIENT'], str(width), str(height),
        'move', str(visual_x), str(visual_y), 'click', '272')
    for _ in range(80):
        content = log.read_text()
        if 'press-position ' in content:
            break
        time.sleep(0.05)
    else:
        raise AssertionError(f'mapped click was not delivered: {log.read_text()}')
    positions = [line for line in content.splitlines() if line.startswith('press-position ')]
    parts = positions[-1].split()
    px, py = float(parts[1].split('=')[1]), float(parts[2].split('=')[1])
    assert abs(px - (x - view['x'])) < 4, (px, x, view)
    assert abs(py - (y - view['y'])) < 4, (py, y, view)
    assert 'pointer-button code=272 state=pressed' in content
    assert 'pointer-button code=272 state=released' in content
    before = len(positions)
    run(os.environ['UMBRIEL_POINTER_CLIENT'], str(width), str(height),
        'move', '1', '1', 'click', '272')
    time.sleep(0.1)
    assert log.read_text().count('press-position ') == before
    stop.set()
    worker.join(timeout=2)
    assert not worker.is_alive()

time.sleep(0.3)
run(os.environ['UMBRIEL_POINTER_CLIENT'], str(width), str(height),
    'move', str(x), str(y), 'click', '272')
for _ in range(80):
    if log.read_text().count('press-position ') > before:
        break
    time.sleep(0.05)
else:
    raise AssertionError('unwarped input did not recover after disconnect')

# When the pointer is drawn before the screen effect, its image is warped
# with the window. Input must use original scene coordinates, not apply
# the input homography a second time.
with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as producer:
    producer.settimeout(3)
    producer.connect(os.environ['UMBRIEL_SOCKET'])
    send(producer, 0.65, cursor_with_screen=True)
    stop = threading.Event()
    def feed_scene_cursor():
        while not stop.wait(0.04):
            send(producer, 0.65, cursor_with_screen=True)
    worker = threading.Thread(target=feed_scene_cursor, daemon=True)
    worker.start()
    time.sleep(0.3)
    before = log.read_text().count('press-position ')
    run(os.environ['UMBRIEL_POINTER_CLIENT'], str(width), str(height),
        'move', str(x), str(y), 'click', '272')
    for _ in range(80):
        if log.read_text().count('press-position ') > before:
            break
        time.sleep(0.05)
    else:
        raise AssertionError('scene-composited cursor was mapped twice')
    parts = [line for line in log.read_text().splitlines() if line.startswith('press-position ')][-1].split()
    px, py = float(parts[1].split('=')[1]), float(parts[2].split('=')[1])
    assert abs(px - (x - view['x'])) < 4, (px, x, view)
    assert abs(py - (y - view['y'])) < 4, (py, y, view)
    stop.set()
    worker.join(timeout=2)
    assert not worker.is_alive()
PY
