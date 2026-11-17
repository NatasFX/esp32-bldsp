"""Joint calibration, not independent validation. Preserves pre17_v1 unchanged."""
from pathlib import Path
import json,csv,numpy as np
from scipy.optimize import least_squares
from scipy.integrate import solve_ivp
import matplotlib;matplotlib.use('Agg')
import matplotlib.pyplot as plt
P=Path(__file__).resolve().parent;R=P.parent
old=json.loads((R/'multinode/model_pre17_v1.json').read_text());E=old['electrical'];rw=E['wire_r_assumed_ohm']
V=R/'validation17_live' if (R/'validation17_live').exists() else R/'validation17'
def rows(path):return list(csv.DictReader(path.open(encoding='utf-8-sig')))
f=rows(R/'means_10_samples.csv');t1=np.array([float(x['time_s']) for x in f]);n1=np.array([float(x['ntc_mean_c']) for x in f]);h1=np.array([float(x['camera_mean_c']) for x in f]);ok=np.array([x['complete_10']=='True' for x in f]);
f=rows(V/'ntc_10second_means.csv');t2=np.array([float(x['video_s']) for x in f]);n2=np.array([float(x['ntc_mean_c']) for x in f]);f=rows(V/'camera_manual.csv');tc2=np.array([float(x['video_s']) for x in f]);h2=np.array([float(x['hotspot_c']) for x in f]);ok2=tc2!=900.5;tc2=tc2[ok2];h2=h2[ok2]
f=rows(R/'multinode/electrical_fit_points.csv');pv=np.array([[float(x['time_s']),float(x['coil_power_w'])] for x in f]);f=rows(R/'power_manual.csv');power=np.array([[float(x['time_s']),float(x['source_power_w'])] for x in f]);
for j,(t,p) in enumerate(power):
 if p>0:
  if t>=30.5:power[j,1]=np.interp(t,pv[:,0],pv[:,1])
  else:power[j,1]=p-(p/(14.4 if 9<=t<=12 else 15))**2*rw
# Gauge choice only: effective hidden body capacity is not separately identified.
CB=500.
def unpack(z):
 cc,ga,gc,gb,tau=np.exp(z[:5]);w=np.exp(np.r_[z[5:],0]-max(0,*z[5:]));w/=sum(w)
 return cc,ga,gc,gb,tau,w[0],w[1]
def sim(z,run,query):
 cc,ga,gc,gb,tau,wc,wb=unpack(z);ta=27.86 if run==1 else 27.8
 y0=[32.1-ta,0,n1[0]-ta] if run==1 else [1.2,max(0,(2.2-wc*1.2)/wb),2.2]
 def fun(t,y,on):
  x,b,n=y
  if run==1:p=np.interp(t,power[:,0],power[:,1])
  else:
   rt=E['r_at_reference_ohm']+E['slope_ohm_per_c']*(x+ta-E['reference_c']);i=17/rt if on else 0;p=i*i*(rt-rw)
  return [(p-ga*x-gc*(x-b))/cc,(gc*(x-b)-gb*b)/CB,(wc*x+wb*b-n)/tau]
 breaks=[0,8,785,1346] if run==1 else [0,8,901,1383];result=np.empty((3,len(query)))
 for j,(lo,hi) in enumerate(zip(breaks[:-1],breaks[1:])):
  s=solve_ivp(lambda t,y:fun(t,y,j==1),(lo,hi),y0,rtol=2e-6,atol=1e-7,dense_output=True,max_step=12);y0=s.y[:,-1];mask=(query>=lo)&(query<=hi)
  if mask.any():result[:,mask]=s.sol(query[mask])
 return result+ta
def resid(z):
 a=sim(z,1,t1);q=np.unique(np.r_[t2,tc2]);b=sim(z,2,q);c2=np.interp(tc2,q,b[0]);nn2=np.interp(t2,q,b[2])
 # Equalize runs and camera/NTC; robust loss handles residual NTC fluctuations.
 return np.r_[(a[0,ok]-h1[ok])/3*np.sqrt(100/ok.sum()),(a[2]-n1)*np.sqrt(100/len(t1)),(c2-h2)/3*np.sqrt(100/len(tc2)),(nn2-n2)*np.sqrt(100/len(t2))]
th=old['thermal'];start=np.r_[np.log([th['coil_capacity_j_k'],th['coil_ambient_w_k'],th['coil_body_w_k'],th['body_ambient_w_k'],th['sensor_tau_s']]),np.log([th['sensor_coil_weight']/th['sensor_ambient_weight'],th['sensor_body_weight']/th['sensor_ambient_weight']])]
lo=np.r_[np.log([10,.02,.02,.05,1]),[-5,-5]];hi=np.r_[np.log([200,2,3,8,120]),[5,5]]
rng=np.random.default_rng(20261116);fits=[]
for k in range(3):
 z=start if k==0 else start+rng.normal(0,.25,len(start));f=least_squares(resid,np.clip(z,lo+.001,hi-.001),bounds=(lo,hi),loss='soft_l1',max_nfev=140,diff_step=.0002,ftol=1e-6,xtol=1e-6,gtol=1e-6)
 fits.append(f);print('start',k,'cost',f.cost,'nfev',f.nfev,flush=True)
f=min(fits,key=lambda f:f.cost);z=f.x;cc,ga,gc,gb,tau,wc,wb=unpack(z)
A=np.array([[-(ga+gc)/cc,gc/cc,0],[gc/CB,-(gc+gb)/CB,0],[wc/tau,wb/tau,-1/tau]])
result={'version':'joint_v2','status':'Calibration on BOTH runs; not independent validation','electrical':E,'thermal':dict(coil_capacity_j_k=cc,body_capacity_j_k=CB,coil_ambient_w_k=ga,coil_body_w_k=gc,body_ambient_w_k=gb,sensor_tau_s=tau,sensor_coil_weight=wc,sensor_body_weight=wb,sensor_ambient_weight=1-wc-wb),'linear_eigenvalues_per_s':np.linalg.eigvals(A).tolist(),'normalized_body_coefficients':dict(coil_loss=(ga+gc)/cc,body_to_coil=gc/(cc*wb),coil_to_body=wb*gc/CB,body_loss=(gc+gb)/CB),'multistart_costs':[x.cost for x in fits],'note':'Body capacity fixed as a gauge; equivalent hidden state, not measured frame temperature. 17V assumed, electrical law unchanged.'}
fig,axs=plt.subplots(2,2,figsize=(12,8));metrics={}
for run,t,n,tc,h in [(1,t1,n1,t1[ok],h1[ok]),(2,t2,n2,tc2,h2)]:
 grid=np.arange(.5,1346 if run==1 else 1383,1);v=sim(z,run,grid);pc=np.interp(tc,grid,v[0]);pn=np.interp(t,grid,v[2]);metrics[str(run)]={'coil_rmse_c':float(np.sqrt(np.mean((pc-h)**2))),'coil_mae_c':float(np.mean(abs(pc-h))),'ntc_rmse_c':float(np.sqrt(np.mean((pn-n)**2))),'ntc_mae_c':float(np.mean(abs(pn-n)))}
 axs[0,run-1].plot(grid/60,v[0],label='Bobina v2');axs[0,run-1].plot(tc/60,h,'.',label='Câmera');axs[0,run-1].plot(grid/60,v[1],ls='--',label='Estado térmico equivalente');axs[0,run-1].set(title=f'Ensaio {run} — '+('15 V' if run==1 else '17 V assumidos'),ylabel='°C')
 axs[1,run-1].plot(grid/60,v[2],label='NTC v2');axs[1,run-1].plot(t/60,n,label='NTC medido',alpha=.8);axs[1,run-1].set(xlabel='Tempo vídeo (min)',ylabel='°C')
 with (P/f'fit_run{run}.csv').open('w',newline='') as ff:
  w=csv.writer(ff);w.writerow(['video_s','coil_c','body_equivalent_c','ntc_c']);w.writerows(zip(grid,*v))
for ax in axs.flat:ax.grid(alpha=.2);ax.legend(fontsize=8)
fig.tight_layout();fig.savefig(P/'joint_fit.png',dpi=160);result['fit_metrics']=metrics
(P/'model_joint_v2.json').write_text(json.dumps(result,indent=2));np.save(P/'parameters.npy',z);print(json.dumps(result,indent=2),flush=True)
