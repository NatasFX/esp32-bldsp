import numpy as np,json,csv
from pathlib import Path
p=Path(__file__).resolve().parent.parent
V=p/'validation17_live' if (p/'validation17_live').exists() else p/'validation17';m=json.loads((p/'joint_v2/model_joint_v2.json').read_text());th=m['thermal'];wc=th['sensor_coil_weight'];tau=th['sensor_tau_s'];c=m['normalized_body_coefficients']['coil_to_body'];d=m['normalized_body_coefficients']['body_loss'];k=c/wc;lam=d+k
for run in [1,2]:
 if run==1:
  rs=list(csv.DictReader((p/'samples_raw.csv').open(encoding='utf-8-sig')));t=np.array([float(r['time_s']) for r in rs]);n=np.array([float(r['ntc_c']) if r['ntc_c'] else np.nan for r in rs]);n[747]=51.4;ta=27.86
  rr=list(csv.DictReader((p/'means_10_samples.csv').open()));tc=np.array([float(r['time_s']) for r in rr if r['complete_10']=='True']);obs=np.array([float(r['camera_mean_c']) for r in rr if r['complete_10']=='True'])
 else:
  rs=list(csv.DictReader((V/'serial_capture_snapshot.csv').open(encoding='utf-8-sig')));rs=[r for r in rs if r['kind']=='THERMAL' and r['boot']=='2'];off=json.loads((V/'alignment.json').read_text())['video_to_esp_uptime_offset_s'];t=np.array([float(r['uptime_ms'])/1000-off for r in rs]);n=np.array([float(r['ntc_c']) for r in rs]);ta=27.8;mask=(t>=0)&(t<1383);t=t[mask];n=n[mask]
  rr=list(csv.DictReader((V/'camera_manual.csv').open()));tc=np.array([float(r['video_s']) for r in rr if float(r['video_s'])!=900.5]);obs=np.array([float(r['hotspot_c']) for r in rr if float(r['video_s'])!=900.5])
 z=0;prev=None;filtered=None;pred=[];hist=[]
 for ti,nt in zip(t,n):
  hist.append(nt);hist=hist[-10:];avg=np.mean(hist);nn=avg-ta
  if prev is None:
   filtered=nn;z=(1-wc)*nn-k*tau*nn;prev=ti
  dt=ti-prev;prev=ti
  if dt>0:
   old=filtered;filtered+=(-np.expm1(-dt/12))*(nn-filtered);slope=(filtered-old)/dt
   target=k*(1-lam*tau)*filtered/lam;z=target+(z-target)*np.exp(-lam*dt)
   x=((1-k*tau)*filtered+tau*slope-z)/wc
  else:x=nn
  pred.append(ta+max(0,x))
 pred=np.array(pred);er=np.interp(tc,t,pred)-obs
 print(run,'mae',np.mean(abs(er)),'rms',np.sqrt(np.mean(er**2)),'max',max(abs(er)))
 with (p/f'joint_v2/observer_run{run}.csv').open('w',newline='') as f:
  w=csv.writer(f);w.writerow(['video_s','ntc_c','coil_estimate_c']);w.writerows(zip(t,n,pred))
