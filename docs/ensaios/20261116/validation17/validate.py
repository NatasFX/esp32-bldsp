from pathlib import Path
import csv,json,numpy as np
from scipy.integrate import solve_ivp
import matplotlib;matplotlib.use('Agg')
import matplotlib.pyplot as plt
p=Path(__file__).resolve().parent
m=json.loads((p.parent/'multinode/model_pre17_v1.json').read_text());a=json.loads((p/'alignment.json').read_text());off=901.;on=8.;ta=27.8
# Readings transcribed from reviewed frames; hotspot only, no white-cross temperature.
anchors=[(30.5,54.9),(60.5,79.4),(120.5,107.3),(180.5,120.0),(240.5,125.9),(300.5,128.9),(360.5,130.7),(480.5,132.8),(600.5,133.5),(720.5,134.4),(840.5,134.7),(899.5,134.8),(900.5,139.7),(930.5,107.2),(990.5,71.0),(1080.5,50.4),(1140.5,42.9),(1200.5,40.4),(1260.5,39.1)]
e=m['electrical'];th=m['thermal'];wc=th['sensor_coil_weight'];wb=th['sensor_body_weight'];cc=th['coil_capacity_j_k'];cb=th['body_capacity_j_k'];gc=th['coil_body_w_k'];ga=th['coil_ambient_w_k'];gb=th['body_ambient_w_k'];tau=th['sensor_tau_s']
y0=[29-ta,(30-ta-wc*(29-ta))/wb,30-ta]
def rhs(t,y,powered):
 x,b,n=y;R=e['r_at_reference_ohm']+e['slope_ohm_per_c']*(x+ta-e['reference_c']);I=17/R if powered else 0;P=I*I*(R-e['wire_r_assumed_ohm'])
 return [(P-ga*x-gc*(x-b))/cc,(gc*(x-b)-gb*b)/cb,(wc*x+wb*b-n)/tau]
sols=[];state=y0
for lo,hi,power in [(0,on,False),(on,off,True),(off,1383,False)]:
 sol=solve_ivp(lambda t,y:rhs(t,y,power),(lo,hi),state,dense_output=True,rtol=1e-9,atol=1e-10,max_step=.5);sols.append((lo,hi,sol));state=sol.y[:,-1]
def pred(t):
 t=np.asarray(t);out=np.empty((3,t.size))
 for lo,hi,s in sols:
  mask=(t>=lo)&(t<=hi)
  if mask.any():out[:,mask]=s.sol(t[mask])
 return out+ta
times=np.arange(.5,1383,1);ys=pred(times)
raw=list(csv.DictReader((p/'serial_capture_snapshot.csv').open(encoding='utf-8-sig')));rr=[r for r in raw if r['kind']=='THERMAL' and r['boot']=='2'];lt=np.array([float(r['uptime_ms'])/1000-a['video_to_esp_uptime_offset_s'] for r in rr]);ln=np.array([float(r['ntc_c']) for r in rr]);means=[]
for start in range(0,1380,10):
 mask=(lt>=start)&(lt<start+10);nn=ln[mask]; tt=lt[mask]
 if len(nn):means.append((float(tt.mean()),float(nn.mean()),len(nn),float(nn.std())))
bt,bn,cnt,std=np.array(means).T;bp=pred(bt)[2];at,ac=np.array(anchors).T;ap=pred(at)[0]
def metrics(obs,pp,mask):
 er=(pp-obs)[mask];return {'n':len(er),'bias_prediction_minus_observed_c':float(er.mean()),'mae_c':float(abs(er).mean()),'rmse_c':float(np.sqrt(np.mean(er**2))),'max_absolute_c':float(abs(er).max())}
res={'frozen_model':'pre17_v1','parameters_refitted':False,'ambient_c':ta,'voltage_assumed_v':17,'power_on_video_s':on,'power_off_video_s':off,'power_off_uncertainty_s':2,'switch_evidence':'Rapid sustained thermal decline starts immediately after 900.5s; source not visible. Off estimated 901 +/-2s, supersedes verbal 16min.','powered_duration_s':off-on,'camera_method':'Manual hotspot readings on reviewed frames, sparse independent references. Not ten-sample camera averages. 900.5s jump treated separately.','ntc_method':'Nonoverlapping ten-second windows of serial instantaneous samples, counts retained; usually 10 samples, occasionally 9/11 due sample timing.','coil_heating':metrics(ac,ap,(at>on)&(at<899.9)),'coil_cooling':metrics(ac,ap,at>off),'ntc_heating':metrics(bn,bp,(bt>on)&(bt<off)),'ntc_cooling':metrics(bn,bp,bt>off),'points':[{'video_s':t,'coil_camera_c':c,'coil_prediction_c':float(pr)} for (t,c),pr in zip(anchors,ap)]}
(p/'validation_metrics.json').write_text(json.dumps(res,indent=2))
with (p/'validation_predictions.csv').open('w',newline='') as f:
 w=csv.writer(f);w.writerow(['video_s','coil_prediction_c','body_state_prediction_c','ntc_prediction_c']);w.writerows(zip(times,*ys))
with (p/'ntc_10second_means.csv').open('w',newline='') as f:
 w=csv.writer(f);w.writerow(['video_s','ntc_mean_c','samples','std_c']);w.writerows(means)
with (p/'camera_manual.csv').open('w',newline='') as f:
 w=csv.writer(f);w.writerow(['video_s','hotspot_c']);w.writerows(anchors)
fig,ax=plt.subplots(3,1,figsize=(11,10),sharex=True)
ax[0].plot(times/60,ys[0],label='Modelo congelado — bobina',color='#d78321');ax[0].plot(at/60,ac,'o',label='Câmera — hotspot transcrito',color='#b82d35');ax[0].set(ylabel='Bobina (°C)',title='Validação independente — 17 V DC assumidos, sem reajuste do modelo')
ax[1].plot(times/60,ys[2],label='Modelo congelado — NTC',color='#9153ad');ax[1].plot(bt/60,bn,label='NTC serial — médias de ~10 amostras',color='#148c89');ax[1].set(ylabel='NTC (°C)')
ax[2].plot(at/60,ap-ac,'o-',label='Erro da bobina',color='#b82d35');ax[2].plot(bt/60,bp-bn,label='Erro do NTC',color='#148c89');ax[2].axhline(0,color='gray',lw=1);ax[2].set(ylabel='Previsão − medição (°C)',xlabel='Tempo do vídeo (min)')
for aa in ax:
 aa.axvline(on/60,color='gray',ls=':');aa.axvline(off/60,color='gray',ls='--',label='Desligamento inferido ~15:01');aa.grid(alpha=.2);aa.legend(fontsize=8);aa.set_xlim(0,23.05)
fig.tight_layout();fig.savefig(p/'validation_pre17_v1.png',dpi=160)
print(json.dumps(res,indent=2))
