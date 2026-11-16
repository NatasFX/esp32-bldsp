"""LAN preview: simulated DSP only; never opens USB or serves workspace files."""
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import json
import threading
import argparse
from urllib.parse import urlsplit, parse_qs

ROOT = Path(__file__).resolve().parent
snapshot = json.loads((ROOT / 'preset1_acp.json').read_text())
words = {output: snapshot[output].copy() for output in ('sub', 'stereo')}
lock = threading.Lock()

class Handler(BaseHTTPRequestHandler):
    def reply(self, data, status=200, mime='application/json; charset=utf-8'):
        body = data.encode('utf-8') if isinstance(data, str) else json.dumps(data).encode()
        self.send_response(status)
        self.send_header('Content-Type', mime)
        self.send_header('Content-Length', str(len(body)))
        self.send_header('Cache-Control', 'no-store')
        self.end_headers()
        self.wfile.write(body)

    def output(self):
        value = parse_qs(urlsplit(self.path).query).get('output', ['sub'])[0]
        if value not in words:
            raise ValueError('Invalid output')
        return value

    def do_GET(self):
        route = urlsplit(self.path).path
        if route in ('/', '/index.html'):
            self.reply((ROOT / 'main/index.html').read_text(encoding='utf-8'), mime='text/html; charset=utf-8')
        elif route == '/eq-response.js':
            self.reply((ROOT / 'main/eq-response.js').read_text(encoding='utf-8'), mime='application/javascript; charset=utf-8')
        elif route == '/api/status':
            self.reply({'demo': True})
        elif route == '/api/telemetry':
            self.reply({'temperature': {'status': 'unavailable', 'celsius': None},
                        'clip': {'status': 'uncalibrated', 'active': None},
                        'automatic_protection': False})
        elif route == '/api/telemetry/history':
            self.reply({'uptime_ms': 0, 'samples': [], 'events': [], 'storage': 'ram'})
        elif route == '/api/telemetry/ldr/capture':
            self.reply({'uptime_ms': 0, 'samples': [], 'window_ms': 2000, 'sample_interval_ms': 20})
        elif route == '/api/eq':
            try:
                output = self.output()
                with lock:
                    self.reply({'words': words[output].copy(), 'matches': True, 'demo': True, 'output': output})
            except ValueError:
                self.reply({'error': 'Invalid output'}, 400)
        else:
            self.reply({'error': 'Not found'}, 404)

    def do_POST(self):
        if self.headers.get('X-BLDSP') != '1':
            return self.reply({'error': 'Missing header'}, 403)
        try:
            size = int(self.headers.get('Content-Length', '0'))
            if not 0 < size <= 1024:
                raise ValueError('Invalid length')
            data = json.loads(self.rfile.read(size))
            route = urlsplit(self.path).path
            if route == '/api/eq':
                output = self.output()
                incoming = data['words']
                if len(incoming) != 53 or any(type(x) is not int or not 0 <= x <= 65535 for x in incoming):
                    raise ValueError('Invalid words')
                with lock:
                    words[output] = incoming.copy()
                    self.reply({'words': words[output].copy(), 'matches': True, 'demo': True, 'output': output})
            elif route == '/api/save':
                self.reply({'sent': False, 'demo': True, 'persistence_verified': False})
            elif route == '/api/source/pulse':
                if data != {}:
                    raise ValueError('Expected empty object')
                self.reply({'accepted': True, 'pulse_ms': 100, 'demo': True}, 202)
            elif route == '/api/telemetry/ldr':
                self.reply({'error': 'Calibre os sensores conectado ao ESP32 real.'}, 400)
            else:
                self.reply({'error': 'Not found'}, 404)
        except (ValueError, KeyError, TypeError):
            self.reply({'error': 'Invalid request'}, 400)

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--host', default='127.0.0.1')
    args = parser.parse_args()
    print(f'BLDSP preview at http://{args.host}:8080 — simulation only', flush=True)
    ThreadingHTTPServer((args.host, 8080), Handler).serve_forever()
