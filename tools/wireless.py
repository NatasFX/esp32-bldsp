"""HTTP OTA for production S3 and its C3 bridge. Standard library only."""
import json
import time
import os
import subprocess
import urllib.request
import urllib.error
from pathlib import Path

def validate_image(data,target):
    chip,project=(9,b'bldsp_control') if target=='s3' else (5,b'bldsp_panel_c3')
    limit=3*1024*1024 if target=='s3' else 0x1f0000
    if not 288<=len(data)<=limit or data[0]!=0xe9 or int.from_bytes(data[12:14],'little')!=chip or data[23]!=1 or data[32:36]!=bytes.fromhex('3254cdab') or data[80:112].split(b'\0')[0]!=project:
        raise RuntimeError('Wrong application image for '+target)
    return data[-32:].hex()

def deploy(target,image,host='192.168.4.1',wifi_profile=None):
    if not host or any(c not in 'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.-:' for c in host):
        raise RuntimeError('Invalid host')
    base='http://'+host;data=Path(image).read_bytes();sha=validate_image(data,target)
    # Direct local connection, independent of system proxy settings.
    opener=urllib.request.build_opener(urllib.request.ProxyHandler({}))
    def api(path,body=None,headers=None,timeout=30):
        hdr={'X-BLDSP':'1'}
        if isinstance(body,dict):body=json.dumps(body).encode();hdr['Content-Type']='application/json'
        if headers:hdr.update(headers)
        req=urllib.request.Request(base+path,data=body,headers=hdr)
        try:
            with opener.open(req,timeout=timeout) as response:return json.load(response)
        except urllib.error.HTTPError as e:
            msg=e.read(1024).decode(errors='replace')
            raise RuntimeError(f'HTTP {e.code}: {msg}') from e
    route='/api/s3/ota' if target=='s3' else '/api/panel/ota'
    def reconnect():
        if wifi_profile and os.name=='nt':
            subprocess.run(['netsh','wlan','connect','name='+wifi_profile],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,timeout=10)
    try:before=api(route,timeout=5)
    except OSError:
        if not wifi_profile:raise
        reconnect();time.sleep(2);before=api(route,timeout=5)
    if before.get('pending') or before.get('busy') or (target=='c3' and not before.get('online')) or (target=='s3' and not before.get('supported')):
        raise RuntimeError('Device not ready: '+str(before))
    print(f'Wireless {target}: {len(data)} bytes, before bank=0x{before["address"]:x}',flush=True)
    session=None;committed=False
    try:
        if target=='s3':
            try:api(route,data,{'Content-Type':'application/octet-stream','X-Image-SHA256':sha},120)
            except (OSError,urllib.error.URLError):pass # Lost response: confirm new boot below.
            committed=True
        else:
            start=api(route+'/begin',{'size':len(data)});session=start['session'];offset=0;shown=-1;failures=0
            while offset<len(data):
                block=data[offset:offset+4096]
                try:
                    s=api(route+f'/data?session={session}&offset={offset}',block,{'Content-Type':'application/octet-stream'})
                    new=int(s['offset'])
                    if not offset<new<=len(data):raise RuntimeError('Invalid OTA progress')
                    offset=new;failures=0
                except (OSError,RuntimeError):
                    failures+=1
                    if failures>3:raise
                    time.sleep(.5);s=api(route)
                    if s['session']!=session or not s['busy'] or not offset<=s['offset']<=len(data):raise RuntimeError('OTA session lost')
                    offset=s['offset']
                percent=offset*100//len(data)//25*25
                if percent!=shown:print(f'Wireless c3: {percent}%',flush=True);shown=percent
            try:api(route+'/end',{'session':session})
            except (OSError,RuntimeError):pass
            committed=True
        if target=='s3' and wifi_profile:
            print('Wireless s3: rebooting; reconnecting Wi-Fi profile...',flush=True)
            time.sleep(3);reconnect();time.sleep(3)
        last_reconnect=time.monotonic()
        deadline=time.monotonic()+120
        while time.monotonic()<deadline:
            time.sleep(1.5)
            try:
                s=api(route,timeout=3)
                if s.get('boot')!=before['boot'] and s.get('sha256')==sha and not s.get('pending') and (target=='s3' or s.get('online')):
                    print(f'Wireless {target}: confirmed new boot, SHA256, bank=0x{s["address"]:x}',flush=True)
                    return s
            except (OSError,RuntimeError):
                # Repeated connect requests can restart Windows association/DHCP.
                if time.monotonic()-last_reconnect>=20:
                    reconnect();last_reconnect=time.monotonic()
        raise RuntimeError('New firmware not confirmed; reconnect Wi-Fi and inspect device')
    finally:
        if session and not committed:
            try:api(route+'/abort',{'session':session})
            except (OSError,RuntimeError):pass
