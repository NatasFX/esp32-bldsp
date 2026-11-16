"""Pre-registered DC predictions from the 2026-11-16 15 V recording only.
Units: W, V, ohm, degC, seconds, J/K, W/K. No hardware writes.
"""
from pathlib import Path
import csv,json
import numpy as np
from scipy.optimize import least_squares
from scipy.linalg import expm
from scipy.signal import lfilter
from scipy.integrate import solve_ivp
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
ROOT=Path(__file__).resolve().parent
OUT=ROOT/'multinode';OUT.mkdir(exist_ok=True)
means=list(csv.DictReader((ROOT/'means_10_samples.csv').open(encoding='utf-8')))
raw=list(csv.DictReader((ROOT/'samples_raw.csv').open(encoding='utf-8')))
bt=np.array([float(r['time_s']) for r in means]);nn=np.array([float(r['ntc_mean_c']) for r in means])
hh=np.array([float(r['camera_mean_c']) for r in means]);valid=np.array([r['complete_10']=='True' for r in means])
times=np.arange(0,1347,dtype=float)
power=np.array([[float(r['time_s']),float(r['source_power_w'])] for r in csv.DictReader((ROOT/'power_manual.csv').open(encoding='utf-8'))])
TA=nn[0];TC0=32.1
# Power is manually sampled. Only source readings after the CC/start transient are used.
# Restrict fit to 30.5..720.5 s: source set/display is 15.00 V, no off-transition bin.
# Direct visual transcription of actual V/I displays, not nominal speaker ohms.
vi=np.array([(30.5,15,3.893),(35.5,15,3.838),(40.5,15,3.790),(45.5,15,3.745),
 (50.5,15,3.706),(55.5,15,3.669),(60.5,15,3.635),(120.5,15,3.403),
 (180.5,15,3.312),(240.5,15,3.269),(300.5,15,3.249),(360.5,15,3.235),
 (420.5,15,3.228),(480.5,15,3.221),(540.5,15,3.216),(600.5,15,3.212),
 (660.5,15,3.208),(720.5,15,3.204),(780.5,15,3.200),(784.5,15,3.199)])
RWIRE=4./3.199**2  # user estimate: ~4 W lost in leads at end of heating
hr=np.array([float(r['camera_hotspot_c']) if r['camera_hotspot_c'] else np.nan for r in raw])
tr=np.array([float(r['time_s']) for r in raw]);vg=np.isfinite(hr)
eh=np.interp(vi[:,0],tr[vg],hr[vg]);er=vi[:,1]/vi[:,2]
X=np.column_stack([np.ones(len(eh)),eh-TA]);coef=np.linalg.lstsq(X,er,rcond=None)[0]
R0,slope=coef;alpha=slope/R0
# Heat delivered to the coil, excluding the variable I²R loss in the leads.
# Before 30.5s the display is in a transient/adjustment; use the watt line already transcribed.
power_coil=power.copy()
for i,(ts,p) in enumerate(power_coil):
    if p<=0:continue
    if ts>=30.5:
        vv=np.interp(ts,vi[:,0],vi[:,1]);ii=np.interp(ts,vi[:,0],vi[:,2]);p=vv*ii
    else:ii=p/max(14.4 if 9<=ts<=12 else 15.,.1)
    power_coil[i,1]=p-ii*ii*RWIRE
pu=np.interp(times,power_coil[:,0],power_coil[:,1])
with (OUT/'electrical_fit_points.csv').open('w',newline='',encoding='utf-8') as f:
    w=csv.writer(f);w.writerow(['time_s','measured_source_v','measured_current_a','camera_coil_c','total_r_vi_ohm','coil_r_subtract_leads_ohm','source_power_vi_w','lead_power_w','coil_power_w','fit_r_ohm'])
    for (ts,v,ii),h,r in zip(vi,eh,er):w.writerow([ts,v,ii,h,r,r-RWIRE,v*ii,ii*ii*RWIRE,v*ii-ii*ii*RWIRE,R0+slope*(h-TA)])

def unpack(z):
    Cc,Cb,Gca,Gcb,Gba,tn=np.exp(z[:6]);wc,wb,wa=np.exp(np.r_[z[6:8],0]-max(0,*z[6:8]));s=wc+wb+wa
    return Cc,Cb,Gca,Gcb,Gba,tn,wc/s,wb/s
def matrices(z):
    Cc,Cb,Gca,Gcb,Gba,tn,wc,wb=unpack(z)
    A=np.array([[-(Gca+Gcb)/Cc,Gcb/Cc,0],[Gcb/Cb,-(Gcb+Gba)/Cb,0],[wc/tn,wb/tn,-1/tn]])
    B=np.array([1/Cc,0,0]);return A,B
def linear_sim(z,ta=TA):
    A,B=matrices(z);Ad=expm(A);Bd=np.linalg.solve(A,(Ad-np.eye(3))@B)
    eig,V=np.linalg.eig(Ad);iv=np.linalg.inv(V);zi=iv@np.array([TC0-ta,0,nn[0]-ta]);bu=iv@Bd
    modal=np.array([zi[i]*eig[i]**times+lfilter([0,bu[i]],[1,-eig[i]],pu) for i in range(3)])
    return np.real((V@modal).T)+ta
def observations(pred):
    # Integrate the predicted 1 s signal at the same half-second instants as the video.
    at=np.array([np.interp(tr,times,pred[:,i]) for i in range(3)]).T
    return np.array([at[i:i+10].mean(axis=0) for i in range(0,len(at),10)])
def residual(z,ta=TA):
    p=observations(linear_sim(z,ta));return np.r_[(p[:,0]-hh)[valid]/1.0,(p[:,2]-nn)/.6]
lo=np.r_[np.log([1,10,.0001,.001,.0001,.3]),[-7,-7]]
hi=np.r_[np.log([1000,100000,5,5,10,600]),[7,7]]
rng=np.random.default_rng(16112026);fits=[]
for i in range(16):
    initial=np.r_[np.log([40,1200,.15,.4,.3,8]),[-1,0]]
    if i:initial+=rng.normal(0,1.0,8)
    f=least_squares(residual,np.clip(initial,lo+1e-5,hi-1e-5),bounds=(lo,hi),max_nfev=350,ftol=1e-7,xtol=1e-7,gtol=1e-7)
    fits.append((float(np.mean(f.fun**2)),f.x))
    print('fit',i,'objective',round(fits[-1][0],5),flush=True)
fits.sort(key=lambda a:a[0]);cost,z=fits[0];pred=linear_sim(z);ob=observations(pred)
names=['coil_capacity_j_k','body_capacity_j_k','coil_ambient_w_k','coil_body_w_k','body_ambient_w_k','sensor_tau_s','sensor_coil_weight','sensor_body_weight']
params=dict(zip(names,map(float,unpack(z))));params['sensor_ambient_weight']=1-params['sensor_coil_weight']-params['sensor_body_weight']
def nonlinear(v,z=z,ta=TA,ilim=None):
    Cc,Cb,Gca,Gcb,Gba,tn,wc,wb=unpack(z)
    def ode(t,y):
        tc,tb,nt=y;r=max(.1,R0+slope*(tc-TA));on=0<=t<777
        ii=min(v/r,ilim) if on and ilim else (v/r if on else 0)
        p=ii*ii*(r-RWIRE)
        return [(p-Gca*(tc-ta)-Gcb*(tc-tb))/Cc,(Gcb*(tc-tb)-Gba*(tb-ta))/Cb,(ta+wc*(tc-ta)+wb*(tb-ta)-nt)/tn]
    tt=np.arange(0,1348,dtype=float)
    sol=solve_ivp(ode,(0,tt[-1]),[ta,ta,ta],t_eval=tt,rtol=1e-7,atol=1e-8,max_step=2)
    yy=sol.y.T;r=R0+slope*(yy[:,0]-TA);current=np.minimum(v/r,ilim) if ilim else v/r
    current[tt>=777]=0;pp=current**2*(r-RWIRE);actual_v=current*r
    return np.column_stack([tt,yy,r,current,pp,actual_v,current**2*r,current**2*RWIRE])
scenarios={};curves={}
for label,v,limit in [('11V',11,None),('15V',15,None),('17V',17,None),('17V_limit4A',17,4.)]:
    cur=nonlinear(v,ilim=limit);curves[label]=cur
    with (OUT/f'prediction_{label}.csv').open('w',newline='',encoding='utf-8') as f:
        w=csv.writer(f);w.writerow(['time_since_on_s','coil_c','body_equivalent_c','ntc_c','total_r_ohm','current_a','coil_power_w','actual_source_v','source_power_w','wire_power_w']);w.writerows(cur)
    scenarios[label]=[]
    for sec in [0,30,60,120,300,600,776,837,897,1077,1317]:
        a=cur[sec];scenarios[label].append(dict(zip(['time_s','coil_c','body_c','ntc_c','r_ohm','current_a','coil_power_w','source_v','source_power_w','wire_power_w'],map(float,a))))
result={'version':'pre17_v1','ambient_c':TA,'coil_start_video_c':TC0,'heating_duration_prediction_s':777,
 'electrical':{'r_at_reference_ohm':float(R0),'reference_c':TA,'slope_ohm_per_c':float(slope),'alpha_effective_per_c':float(alpha),
 'r_at_20c_ohm':float(R0+slope*(20-TA)),'fit_rmse_ohm':float(np.sqrt(np.mean((X@coef-er)**2))),
 'wire_r_assumed_ohm':RWIRE,'coil_r_at_reference_ohm':float(R0-RWIRE),'coil_r_at_20c_ohm':float(R0+slope*(20-TA)-RWIRE),
 'coil_alpha_effective_20c_per_c':float(slope/(R0+slope*(20-TA)-RWIRE)),
 'warning':'V/I from source; lead resistance uses user estimate of 4W at 3.199A; hotspot treated as coil temperature per user.'},
 'thermal':params,'fit':{'objective':cost,'camera_rmse_c':float(np.sqrt(np.mean((ob[:,0]-hh)[valid]**2))),
 'ntc_rmse_c':float(np.sqrt(np.mean((ob[:,2]-nn)**2))),'body_final_video_c':float(pred[-1,1]),
 'multistart_objectives':[float(a[0]) for a in fits]},'scenarios':scenarios,
 'assumptions':['Same stationary exposed-DC geometry as video; no forced air','New run starts fully cold at ambient reference',
 '777 s powered then 570 s cooldown','Source V means actual output; 4A-limited scenario separately provided',
 'Body is hidden equivalent thermal state, not a measured frame temperature','No independent validation or statistical prediction interval']}
(OUT/'model_pre17_v1.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
# Save model vectors and all starts to allow sensitivity/profiling without refitting.
np.savez(OUT/'fit_cache.npz',z=z,z_all=np.array([a[1] for a in fits]),cost_all=np.array([a[0] for a in fits]),lo=lo,hi=hi)
fig,axs=plt.subplots(2,1,figsize=(11,8),sharex=True)
axs[0].plot(bt[valid]/60,hh[valid],'.',label='Câmera: médias completas');axs[0].plot(times/60,pred[:,0],label='Modelo: bobina')
axs[0].plot(times/60,pred[:,1],label='Carcaça equivalente (não medida)',ls='--');axs[0].set(ylabel='Temperatura (°C)',title='Modelo de três estados — ajuste ao ensaio de 15 V')
axs[1].plot(bt/60,nn,'.',label='NTC: médias de 10');axs[1].plot(times/60,pred[:,2],label='Modelo: NTC');axs[1].set(xlabel='Tempo do vídeo (min)',ylabel='Temperatura NTC (°C)')
for ax in axs:ax.legend(fontsize=8);ax.grid(alpha=.2);ax.axvline(785/60,ls=':',color='gray')
fig.tight_layout();fig.savefig(OUT/'fit_three_states.png',dpi=160);plt.close(fig)
fig,axs=plt.subplots(3,1,figsize=(11,10),sharex=True)
for label,cur in curves.items():
    for ax,col in zip(axs,[1,3,6]):ax.plot(cur[:,0]/60,cur[:,col],label=label,ls='--' if 'limit' in label else '-')
for ax in axs:ax.legend(fontsize=8);ax.grid(alpha=.2);ax.axvline(777/60,ls=':',color='gray')
axs[0].set(ylabel='Bobina (°C)',title='Previsões antes do segundo ensaio — início frio, DC parado')
axs[1].set(ylabel='NTC (°C)');axs[2].set(ylabel='Potência na bobina (W)',xlabel='Tempo desde ligar (min)')
fig.tight_layout();fig.savefig(OUT/'predictions_11_17v.png',dpi=160);plt.close(fig)
fig,ax=plt.subplots(figsize=(8,4));ax.scatter(eh,er,label='V / I do mostrador');ax.scatter(eh,er-RWIRE,label='Bobina: descontando resistência dos fios');xx=np.linspace(25,150,100);ax.plot(xx,R0+slope*(xx-TA),label='Reta total ajustada');ax.plot(xx,R0+slope*(xx-TA)-RWIRE,label='Reta bobina',ls='--');ax.set(xlabel='Temperatura da bobina, câmera (°C)',ylabel='Resistência (Ω)',title='Ajuste elétrico — V/I real, sem o transiente inicial');ax.legend(fontsize=8);ax.grid(alpha=.2);fig.tight_layout();fig.savefig(OUT/'resistance_fit.png',dpi=150)
print(json.dumps({'electrical':result['electrical'],'thermal':params,'fit':result['fit'],'scenarios':scenarios},indent=2),flush=True)
