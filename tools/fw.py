#!/usr/bin/env python3
"""Compact local build/deploy CLI. See docs/pipeline.md."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import time

# Serial tools may emit text in the Windows console encoding. Never let a
# diagnostic character hide the original deployment failure.
for stream in (sys.stdout, sys.stderr):
    if hasattr(stream, 'reconfigure'):
        stream.reconfigure(errors='replace')

ROOT = Path(__file__).resolve().parents[1]
RUNS = ROOT / 'build' / 'pipeline'
RUNS.mkdir(parents=True, exist_ok=True)
IDF = Path(os.environ.get('IDF_PATH', str(Path.home() / 'Documents/esp-idf')))
PYTHON = Path(os.environ.get('BLDSP_IDF_PYTHON', str(Path.home() / '.espressif/python_env/idf5.4_py3.11_env/Scripts/python.exe')))


def run(name, command, cwd=ROOT, env=None):
    log = RUNS / (name + '.log')
    print(f'{name}: running', flush=True)
    with log.open('w', encoding='utf-8') as out:
        result = subprocess.run(command, cwd=cwd, stdout=out, stderr=subprocess.STDOUT, env=env)
    if result.returncode:
        lines = log.read_text(encoding='utf-8', errors='replace').splitlines()
        errors = [s for s in lines if re.search(r'error:|FAILED:|Error |fatal|denied|could not', s, re.I)]
        print('\n'.join((errors or lines[-12:])[:12]))
        raise RuntimeError(f'{name}: exit {result.returncode}; full log: {log}')
    print(f'{name}: OK; log: {log}', flush=True)


def build(target, bridge=False):
    project = ROOT if target == 's3' else ROOT / 'panel-c3'
    args = ['-B', str(ROOT / 'build/ota-bridge')] if bridge else []
    if target == 's3':
        args += ['-DBLDSP_OTA_BRIDGE_IMAGE=' + (str(ROOT / 'panel-c3/build/bldsp_panel_c3.bin').replace('\\', '/') if bridge else ''), '-DPANEL_OTA_SMOKE_IMAGE=']
    args += ['build']
    env = os.environ.copy()
    env.update(BLDSP_EXPORT=str(IDF / 'export.ps1'), BLDSP_BUILD_ARGS=json.dumps(args),
               BLDSP_IDF_CLI=str(IDF / 'tools/idf.py'), BLDSP_PYTHON=str(PYTHON))
    script = ". $env:BLDSP_EXPORT; if(!$?) {exit 1}; $a=ConvertFrom-Json $env:BLDSP_BUILD_ARGS; & $env:BLDSP_PYTHON $env:BLDSP_IDF_CLI @a; exit $LASTEXITCODE"
    run('build-' + ('bridge' if bridge else target), ['powershell', '-NoProfile', '-ExecutionPolicy', 'Bypass', '-Command', script], project, env)
    return ROOT / 'build/ota-bridge' if bridge else project / 'build'


def require_port(port):
    from serial.tools.list_ports import comports
    available = [p.device.upper() for p in comports()]
    if port.upper() not in available:
        raise RuntimeError(f'{port} absent; reconnect USB-UART. Available: {", ".join(available) or "none"}')


def release_monitor(port):
    # Close only PuTTY owning the requested COM, never arbitrary processes.
    env = os.environ.copy()
    env['BLDSP_PORT'] = port
    script = "$pattern='\\b'+[regex]::Escape($env:BLDSP_PORT)+'\\b'; Get-CimInstance Win32_Process | Where-Object {$_.Name -eq 'putty.exe' -and $_.CommandLine -match $pattern} | ForEach-Object {Stop-Process -Id $_.ProcessId}"
    run('release-' + port, ['powershell', '-NoProfile', '-Command', script], env=env)


def flash(target, port, directory=None, full=False):
    directory = directory or (ROOT / 'build' if target == 's3' else ROOT / 'panel-c3/build')
    app = 'bldsp_control.bin' if target == 's3' else 'bldsp_panel_c3.bin'
    binary = directory / app
    if not binary.exists():
        raise RuntimeError(f'Build first: {binary}')
    command = [str(PYTHON), '-m', 'esptool', '--chip', 'esp32' + target,
               '--port', port, '-b', '460800', '--before', 'default_reset', '--after', 'hard_reset']
    if target == 'c3':
        command += ['--no-stub']  # Proven ROM flashing path for the ABrobot C3.
    command += ['write_flash']
    if full:
        command += ['@flash_args']
    else:
        command += ['--flash_mode', 'dio', '--flash_freq', '80m', '--flash_size', '16MB' if target == 's3' else '4MB', '0x10000', app]
        if target == 's3' and (directory / 'ota_data_initial.bin').exists():
            command += ['0x910000', 'ota_data_initial.bin']  # USB install selects factory again.
    run('flash-' + target, command, directory)


def monitor(port, seconds, pattern, marker=False):
    import serial
    path = RUNS / ('serial-' + time.strftime('%Y%m%d-%H%M%S') + '.log')
    print(f'Serial {port} -> {path}', flush=True)
    regex = re.compile(pattern)
    deadline = time.monotonic() + seconds
    # Opening the console must not reset the device again.
    stream = serial.Serial(port=None, baudrate=115200, timeout=.25)
    stream.dtr = False
    stream.rts = False
    stream.port = port
    stream.open()
    count = 0
    pending = b''
    try:
        with path.open('wb') as out:
            while time.monotonic() < deadline:
                block = stream.read(stream.in_waiting or 1)
                if not block:
                    continue
                out.write(block)
                pending += block
                while b'\n' in pending:
                    raw, pending = pending.split(b'\n', 1)
                    line = raw.decode('utf-8', errors='replace').strip()
                    if regex.search(line):
                        count += 1
                        if count <= 40:
                            print(line, flush=True)
                    if marker and 'ota_smoke' in line:
                        if 'PASS:' in line:
                            return
                        if 'FAIL:' in line or 'C3 absent' in line:
                            raise RuntimeError(f'OTA not confirmed: {line}; {path}')
            if marker:
                raise RuntimeError(f'OTA confirmation timeout; inspect {path}')
    finally:
        stream.close()
    print(f'Serial complete: {count} matches (printed at most 40); log: {path}')


def ota(port, prepare_only=False):
    if not prepare_only:
        require_port(port)
    # Rebuild production first; bridge uses a different build tree and never edits source.
    production = build('s3')
    build('c3')
    bridge = build('s3', bridge=True)
    normal_bin = production / 'bldsp_control.bin'
    if b'ota_smoke' in normal_bin.read_bytes():
        raise RuntimeError('Production image contains deployment bridge; refusing update')
    if (production / 'partition_table/partition-table.bin').read_bytes() != (bridge / 'partition_table/partition-table.bin').read_bytes():
        raise RuntimeError('Bridge partition table differs; refusing app-only flash')
    if prepare_only:
        print("OTA artifacts ready; no device was touched.")
        return
    saved_hash = hashlib.sha256(normal_bin.read_bytes()).hexdigest()
    release_monitor(port)
    started = False
    try:
        started = True
        flash('s3', port, bridge)
        monitor(port, 240, r'ota_smoke', marker=True)
    finally:
        if started:
            print('Restoring production S3 (also on OTA failure)...', flush=True)
            if hashlib.sha256(normal_bin.read_bytes()).hexdigest() != saved_hash:
                raise RuntimeError('Production image changed during OTA; restoration requires manual review')
            flash('s3', port, production)
    print('OTA C3 confirmed; production S3 restored. NVS preserved.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest='action', required=True)
    for action in ('build', 'flash', 'deploy'):
        p = sub.add_parser(action)
        p.add_argument('target', choices=('s3', 'c3'))
        if action != 'build':
            p.add_argument('--port')
            p.add_argument('--full', action='store_true', help='Also bootloader/partitions (C3 resets OTA metadata); initial USB installation only')
    p = sub.add_parser('ota', help='C3 OTA through USB-connected S3, then restore S3 production')
    p.add_argument('--port', default='COM7')
    p.add_argument('--prepare-only', action='store_true', help='Build and validate all OTA artifacts without touching devices')
    p = sub.add_parser('wireless', help='Update S3 or C3 through the production Wi-Fi portal, no USB')
    p.add_argument('target', choices=('s3', 'c3'))
    p.add_argument('--host', default='192.168.4.1')
    p.add_argument('--no-build', action='store_true')
    p.add_argument('--wifi-profile', help='Windows WLAN profile to reconnect after S3 reboot')
    p = sub.add_parser('logs')
    p.add_argument('--port', default='COM7')
    p.add_argument('--seconds', type=int, default=10)
    p.add_argument('--match', default=r'http_eq|EQ TX|EQ RX|panel_input|ota_smoke|Ready:|Preset 1|FAIL|panic')
    sub.add_parser('test')
    sub.add_parser('ports')
    args = parser.parse_args()
    if getattr(args, 'port', None) and not re.fullmatch(r'COM[1-9][0-9]*', args.port, re.I):
        parser.error('Port must be COM<number>')
    if args.action == 'ports':
        from serial.tools.list_ports import comports
        for p in comports():
            print(f'{p.device}: {p.description}')
    elif args.action == 'wireless':
        from wireless import deploy
        if not args.no_build:
            build(args.target)
        image = ROOT / ('build/bldsp_control.bin' if args.target == 's3' else 'panel-c3/build/bldsp_panel_c3.bin')
        deploy(args.target, image, args.host, args.wifi_profile)
    elif args.action == 'test':
        run('tests', ['cmd', '/c', 'tests\\run_native.cmd'])
        run('tests-pipeline', [sys.executable, '-m', 'unittest', 'discover', '-s', 'tests', '-p', '*_test.py'])
    elif args.action == 'logs':
        if not 1 <= args.seconds <= 300:
            parser.error('Use 1..300 seconds per capture')
        release_monitor(args.port)
        monitor(args.port, args.seconds, args.match)
    elif args.action == 'ota':
        ota(args.port, args.prepare_only)
    else:
        if args.action in ('build', 'deploy'):
            build(args.target)
        if args.action in ('flash', 'deploy'):
            port = args.port or ('COM7' if args.target == 's3' else 'COM8')
            if args.target == 'c3' and not args.full:
                parser.error('For a C3 already using OTA, use ota. USB C3 flashing requires explicit --full (OTA metadata reset).')
            release_monitor(port)
            flash(args.target, port, full=args.full)
    return 0


if __name__ == '__main__':
    try:
        sys.exit(main())
    except (RuntimeError, OSError, KeyboardInterrupt) as error:
        print(f'ERROR: {error}', file=sys.stderr)
        sys.exit(1)
