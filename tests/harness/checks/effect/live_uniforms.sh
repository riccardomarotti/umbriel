#!/usr/bin/env bash
# harness: outputs=2
# Generic live screen uniforms: typed values, ownership and disconnect fallback.
set -euo pipefail

cat > "$UMBRIEL_RUNTIME_DIR/live.glsl" <<'GLSL'
uniform float external_level;
uniform float umbriel_live_available;
vec4 screen(vec2 uv) {
  if (umbriel_live_available < 0.5) return umbriel_sample(uv);
  return vec4(external_level, umbriel_live_available, 0.0, 1.0);
}
GLSL
cat >> "$UMBRIEL_CONFIG" <<'TOML'
[animation]
enabled = false
[colors]
backdrop = "#000000FF"
[effects]
screen = "live"
in_capture = true
max_fps = 0
[effects.preset.live]
kind = "screen"
shader = "live.glsl"
[output.HEADLESS-2]
screen_effect = "off"
TOML
"$UMBRIEL" msg config-reload > /dev/null
"$UMBRIEL" settle > /dev/null

python3 - <<'PY'
import json
import os
from pathlib import Path
import socket
import subprocess
import threading
import time

exe = os.environ['UMBRIEL']
image = str(Path(os.environ['UMBRIEL_RUNTIME_DIR']) / 'live.png')

def run(*args):
    return subprocess.check_output(args, text=True, timeout=10)

def connect():
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s.settimeout(2)
    s.connect(os.environ['UMBRIEL_SOCKET'])
    return s

def send(s, output, value):
    request = {
        'cmd': 'effect-uniforms', 'version': 1, 'kind': 'screen', 'output': output,
        'uniforms': [{'name': 'external_level', 'type': 'float', 'value': value}],
    }
    s.sendall((json.dumps(request) + '\n').encode())
    response = b''
    while not response.endswith(b'\n'):
        part = s.recv(256)
        if not part:
            raise EOFError('producer disconnected')
        response += part
    return json.loads(response)

def pixel(expected, output='HEADLESS-1'):
    actual = None
    for _ in range(90):
        run('grim', '-s', '1', '-o', output, image)
        actual = tuple(map(int, run(os.environ['UMBRIEL_PIXEL_PROBE'], image, 'pixel', '900', '600').split()))
        if all(abs(a-b) < 8 for a,b in zip(actual,expected)):
            return
        time.sleep(0.04)
    raise AssertionError(f'{output}: got {actual}, expected {expected}')

class Producer:
    def __init__(self, level):
        self.level = level
        self.stop = threading.Event()
        self.socket = connect()
        assert send(self.socket, 'HEADLESS-1', self.level) == {'ok': True}
        self.thread = threading.Thread(target=self.feed, daemon=True)
        self.thread.start()

    def feed(self):
        while not self.stop.wait(0.04):
            assert send(self.socket, 'HEADLESS-1', self.level) == {'ok': True}

    def close(self):
        self.stop.set()
        self.thread.join(timeout=2)
        assert not self.thread.is_alive()
        self.socket.close()

pixel((0,0,0))
producer = Producer(0.25)
pixel((64,255,0))
pixel((0,0,0), 'HEADLESS-2')
other = connect()
assert 'err' in send(other, 'HEADLESS-1', 1.0), 'second producer stole the slot'
other.close()
producer.level = 0.75
pixel((191,255,0))
producer.close()
pixel((0,0,0))
producer = Producer(1.0)
pixel((255,255,0))
producer.close()
pixel((0,0,0))
PY
