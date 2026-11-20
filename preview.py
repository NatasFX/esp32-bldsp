"""LAN preview: simulated DSP only; never opens USB or serves workspace files."""
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import json
import threading
import argparse
import time
import math
from urllib.parse import urlsplit, parse_qs

ROOT = Path(__file__).resolve().parent
snapshot = json.loads((ROOT / 'preset1_acp.json').read_text())
words = {output: snapshot[output].copy() for output in ('sub', 'stereo')}
lock = threading.RLock()
strip_preview=dict(enabled=True,count=22,brightness=30,mode=0,palette=0)
wifi_preview=dict(available=True,power_qdbm=8,power_dbm=2)

protection_config = dict(enabled=False,recovery_enabled=True,event_count=3,window_ms=2000,step_db=.5,interval_ms=1000,max_atten_db=12,quiet_ms=8000,recovery_step_db=.25,recovery_interval_ms=2000)
demo = dict(start=time.monotonic(),clip_until=0,last_clip=0,count=0,rail=False,attenuation=0,last_recovery=0,hard_started=None,hard_bin=-1,restoring=False)
def protection_snapshot():
    now=time.monotonic()
    if ((protection_config['recovery_enabled'] and protection_config['enabled']) or demo['restoring']) and not demo['rail'] and now-demo['last_clip']>=protection_config['quiet_ms']/1000 and now-demo['last_recovery']>=protection_config['recovery_interval_ms']/1000:
        demo['attenuation']=max(0,demo['attenuation']-protection_config['recovery_step_db']);demo['last_recovery']=now
    if not demo['attenuation']:demo['restoring']=False
    state='sensor_warning' if demo['rail'] else 'recovering' if demo['restoring'] else 'disabled' if not protection_config['enabled'] else 'reduced' if demo['attenuation'] else 'ready'
    return dict(config=protection_config.copy(),attenuation_db=demo['attenuation'],target_attenuation_db=demo['attenuation'],state=state,sensor_ready=True,sensor_warning=demo['rail'],restoring=demo['restoring'],error='')
def telemetry_preview():
    now=time.monotonic();active=now<demo['clip_until'];flash=False
    if demo['hard_started'] is not None:
        elapsed=now-demo['hard_started'];active=elapsed<8;flash=5<=elapsed<8
        bucket=min(5,int(elapsed))
        if bucket>demo['hard_bin'] and elapsed<6:
            added=bucket-demo['hard_bin'];demo['hard_bin']=bucket;demo['count']+=added;demo['last_clip']=now
            if protection_config['enabled']:demo['attenuation']=min(protection_config['max_atten_db'],demo['attenuation']+protection_config['step_db'])
        if elapsed>=8:demo['hard_started']=None
    rail=demo['rail'];baseline=3600
    return dict(uptime_ms=int((now-demo['start'])*1000),automatic_protection=protection_config['enabled'],
        temperature=dict(status='estimated',celsius=34.2,resistance_ohm=6700,millivolts=1320,raw=1600),
        coil=dict(status='estimated',celsius=62.4,ntc_mean10_c=34.1,ambient_c=27.8),
        clip=dict(status='rail_high' if rail else 'ok',active=active,flash=flash,raw=4095 if rail else 2100 if active else 3600,millivolts=3097 if rail else 2900,baseline_raw=baseline,threshold=2880,release_raw=3240,count=demo['count']),
        protection=protection_snapshot(),wifi=wifi_preview.copy(),strip=strip_preview.copy())

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
        elif route == '/thermal-test':
            self.reply((ROOT / 'main/thermal-test.html').read_text(encoding='utf-8'), mime='text/html; charset=utf-8')
        elif route in ('/api/thermal-test', '/api/thermal-test/export'):
            self.reply({'model_version': 'pre17_v1', 'storage': 'ram', 'session_id': 0,
                        'uptime_ms': 0, 'start_ms': 0, 'phase': 'idle', 'recording': False,
                        'model_valid': False, 'elapsed_ms': 0, 'samples': 0, 'capacity': 2700,
                        'events': 0, 'event_capacity': 96, 'ntc_mean10_c': None, 'mean_count': 0,
                        'ntc_c': None, 'coil_pred_c': None, 'ntc_pred_c': None, 'body_effect_c': None,
                        'ambient_c': None, 'source_v': None, 'current_limit_a': None,
                        'model_current_a': None, 'reference_location': '', 'free_heap_bytes': 0,
                        'records': [], 'marks': []})
        elif route == '/api/strip':
            with lock:self.reply(dict(strip=strip_preview.copy()))
        elif route == '/api/wifi':
            with lock:self.reply(dict(wifi=wifi_preview.copy()))
        elif route == '/api/status':
            self.reply({'demo': True})
        elif route == '/api/telemetry':
            with lock:
                self.reply(telemetry_preview())
        elif route == '/api/protection':
            with lock:
                self.reply(protection_snapshot())
        elif route == '/api/telemetry/history':
            now=int((time.monotonic()-demo['start'])*1000)
            self.reply({'uptime_ms':now,'samples':[{'at_ms':now-(99-i)*1000,'temperature_c':33.5+i*.007+math.sin(i/5)*.3} for i in range(100) if now-(99-i)*1000>=0],'events':[],'storage':'ram'})
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
            if route == '/api/demo/clip':
                with lock:
                    mode=data.get('mode');now=time.monotonic()
                    if mode=='clip':
                        demo['hard_started']=None;demo['restoring']=False;demo['rail']=False;demo['clip_until']=now+2.5;demo['last_clip']=now;demo['count']+=1
                        if protection_config['enabled'] and demo['count']%protection_config['event_count']==0:
                            demo['attenuation']=min(protection_config['max_atten_db'],demo['attenuation']+protection_config['step_db'])
                    elif mode=='hard':demo['rail']=False;demo['hard_started']=now;demo['hard_bin']=-1;demo['restoring']=False
                    elif mode=='rail':demo['rail']=True;demo['clip_until']=0;demo['hard_started']=None
                    elif mode=='clear':demo['rail']=False;demo['clip_until']=0;demo['hard_started']=None
                    else:raise ValueError('Unknown simulation')
                    self.reply(telemetry_preview())
            elif route == '/api/strip':
                with lock:
                    strip_preview.update(data)
                    self.reply(dict(strip=strip_preview.copy()))
            elif route == '/api/wifi':
                value=data.get('power_qdbm')
                if type(value) is not int or value not in (8,20,28,34,44,52,56,60,66,72,80):raise ValueError('Invalid power')
                with lock:
                    wifi_preview.update(power_qdbm=value,power_dbm=value/4)
                    self.reply(dict(wifi=wifi_preview.copy()))
            elif route == '/api/protection':
                with lock:
                    if data.get('action')=='configure':protection_config.update(data['config'])
                    elif data.get('action')=='restore':demo['restoring']=True
                    else:raise ValueError('Unknown action')
                    self.reply(protection_snapshot())
            elif route == '/api/telemetry/thermal':
                self.reply(telemetry_preview())
            elif route == '/api/eq':
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
            elif route == '/api/thermal-test':
                self.reply({'error': 'Conecte ao ESP32 para registrar o ensaio.'}, 400)
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
