import json,csv,numpy as np
from pathlib import Path
from scipy.optimize import minimize_scalar
import argparse
parser=argparse.ArgumentParser(description='Align video OCR to the archived serial NTC means. Original OCR stays outside Git.')
parser.add_argument('--ocr',type=Path,required=True,help='Local ocr_raw.jsonl from the original video')
args=parser.parse_args()
p=Path(__file__).resolve().parent;rs=list(csv.DictReader((p/'serial_capture_snapshot.csv').open(encoding='utf-8-sig')))
r=[r for r in rs if r['kind']=='THERMAL' and r['boot']=='2'];t=np.array([float(x['uptime_ms'])/1000 for x in r]);n=np.array([float(x['mean10_c']) for x in r]);obs=[]
for l in args.ocr.read_text(encoding='utf-8').splitlines():
 z=json.loads(l);sec=int(z['file'][7:12])-.5
 for line in z['lines']:
  w=line['words'][0]
  if w['x']<250 and ((1730<w['y']<1800) or (1900<w['y']<1960)):
   import re
   m=re.match(r'(\d{2})[,.](\d)',line['text'])
   if m:obs.append((sec,float(m[1]+'.'+m[2])))
v,y=np.array(obs).T
# Align observed display average to same mean10 from serial, never model.
def score(off):
 e=np.interp(v+off,t,n)-y;return np.mean(np.minimum(e*e,4))
grid=np.arange(200,400,.1);best=grid[np.argmin([score(o) for o in grid])];opt=minimize_scalar(score,bounds=(best-.2,best+.2),method='bounded')
o=opt.x;e=np.interp(v+o,t,n)-y
result={'video_to_esp_uptime_offset_s':o,'display_alignment_rmse_c':float(np.sqrt(np.mean(e*e))),'median_abs_error_c':float(np.median(abs(e))),'display_samples':len(y),'note':'Constant offset from NTC displayed mean vs serial mean10; browser refresh introduces subsecond jitter. No thermal model used.'}
(p/'alignment.json').write_text(json.dumps(result,indent=2));print(result)
