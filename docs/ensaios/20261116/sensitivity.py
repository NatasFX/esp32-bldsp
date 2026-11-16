"""Structural/sensitivity checks; all curves frozen before observing the 17 V test."""
import importlib.util,json,csv
from pathlib import Path
import numpy as np
from scipy.linalg import expm
from scipy.signal import lfilter
from scipy.optimize import least_squares
from scipy.integrate import solve_ivp
R=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('study',R/'study_multinode.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
out=m.OUT
base=m.z.copy();basewire=m.RWIRE;basepu=m.pu.copy()
allz=np.array([a[1] for a in m.fits]);allp=np.array([m.unpack(z) for z in allz])
structural={name:{'min':float(allp[:,i].min()),'max':float(allp[:,i].max())} for i,name in enumerate(m.names)}
# Identifiable state H is the body contribution to the NTC, not body degrees.
def normalized(z):
 cc,cb,gca,gcb,gba,tn,wc,wb=m.unpack(z)
 return {'coil_capacity_j_k':cc,'coil_decay_per_s':(gca+gcb)/cc,'body_feedback_per_s':gcb/(cc*wb),
 'body_charge_per_s':wb*gcb/cb,'body_decay_per_s':(gcb+gba)/cb,'ntc_coil_weight':wc,'ntc_tau_s':tn}
ident=normalized(base)
samples=[]
for label,wire,ambient in [('base',basewire,m.TA),('wire3W',3/3.199**2,m.TA),('wire5W',5/3.199**2,m.TA),
                           ('ambient_minus2',basewire,m.TA-2),('ambient_plus2',basewire,m.TA+2)]:
 m.RWIRE=wire
 # Heat input sensitivity to uncertain lead loss, using the same observed V/I path.
 ii=np.interp(m.times,m.vi[:,0],m.vi[:,2]);ii[m.times<8]=0;ii[m.times>=786]=0
 m.pu=basepu+(basewire-wire)*ii**2
 f=least_squares(lambda z:m.residual(z,ambient),base,bounds=(m.lo,m.hi),max_nfev=250)
 predictions={}
 for v in [11,17]:
  # New test starts at the measured initial 27.86 C for comparison across models.
  cur=m.nonlinear(v,z=f.x,ta=m.TA)
  predictions[str(v)]={str(s):{'coil_c':float(cur[s,1]),'ntc_c':float(cur[s,3])} for s in [60,120,300,600,776]}
 samples.append({'case':label,'fit_objective':float(np.mean(f.fun**2)),'predictions':predictions})
m.RWIRE=basewire;m.pu=basepu
# Explicit bulk mass: local structure exchanges heat with a massive reservoir.
# Total mass and material mixture are not known; Cf is profiled, never estimated as exact mass.
def unpack4(z):
 cc,cb,gca,gcb,gbf,gfa,tn=np.exp(z[:7]);w=np.exp(np.r_[z[7:9],0]-max(0,*z[7:9]));w/=w.sum()
 return cc,cb,gca,gcb,gbf,gfa,tn,w[0],w[1]
def matrix4(z,cf):
 cc,cb,gca,gcb,gbf,gfa,tn,wc,wb=unpack4(z)
 A=np.array([[-(gca+gcb)/cc,gcb/cc,0,0],[gcb/cb,-(gcb+gbf)/cb,gbf/cb,0],
 [0,gbf/cf,-(gbf+gfa)/cf,0],[wc/tn,wb/tn,0,-1/tn]])
 return A,np.array([1/cc,0,0,0])
def sim4(z,cf):
 A,B=matrix4(z,cf);ad=expm(A);bd=np.linalg.solve(A,(ad-np.eye(4))@B);eig,V=np.linalg.eig(ad);iv=np.linalg.inv(V)
 zi=iv@np.array([m.TC0-m.TA,0,0,0]);bu=iv@bd
 modal=np.array([zi[i]*eig[i]**m.times+lfilter([0,bu[i]],[1,-eig[i]],m.pu) for i in range(4)])
 return np.real((V@modal).T)+m.TA
def obs4(pr):
 at=np.array([np.interp(m.tr,m.times,pr[:,i]) for i in range(4)]).T
 return np.array([at[i:i+10].mean(axis=0) for i in range(0,len(at),10)])
def res4(z,cf):
 p=obs4(sim4(z,cf));return np.r_[(p[:,0]-m.hh)[m.valid],(p[:,3]-m.nn)/.6]
lower=np.r_[np.log([1,10,.0001,.001,.0001,.01,.3]),[-7,-7]]
upper=np.r_[np.log([1000,100000,5,5,20,100,600]),[7,7]]
cc,cb,gca,gcb,gba,tn,wc,wb=m.unpack(base)
start=np.r_[np.log([cc,cb,gca,gcb,gba,3,tn]),base[6:8]]
bulk=[]
for cf in [4000.,7250.,12000.]:
 best=None
 for g0 in [.3,3.,30.]:
  st=start.copy();st[5]=np.log(g0)
  f=least_squares(lambda z:res4(z,cf),st,bounds=(lower,upper),max_nfev=450)
  if best is None or (f.fun@f.fun)<(best.fun@best.fun):best=f
 zz=best.x;pr=sim4(zz,cf);ob=obs4(pr)
 predictions={}
 for v in [11,17]:
  def ode(t,y):
   cc,cb,gca,gcb,gbf,gfa,tn,wc,wb=unpack4(zz);tc,tb,tf,nt=y
   r=m.R0+m.slope*(tc-m.TA);ii=v/r if t<777 else 0;power=ii**2*(r-m.RWIRE)
   return [(power-gca*(tc-m.TA)-gcb*(tc-tb))/cc,(gcb*(tc-tb)-gbf*(tb-tf))/cb,
           (gbf*(tb-tf)-gfa*(tf-m.TA))/cf,(m.TA+wc*(tc-m.TA)+wb*(tb-m.TA)-nt)/tn]
  tt=np.arange(1348);s=solve_ivp(ode,(0,1347),[m.TA]*4,t_eval=tt,rtol=1e-7,atol=1e-8,max_step=2)
  with (out/f'prediction_bulk{int(cf)}_{v}V.csv').open('w',newline='',encoding='utf-8') as f:
   w=csv.writer(f);w.writerow(['time_s','coil_c','local_structure_c','bulk_c','ntc_c']);w.writerows(np.column_stack([tt,s.y.T]))
  predictions[str(v)]={str(sec):{'coil_c':float(s.y[0,sec]),'ntc_c':float(s.y[3,sec]),'bulk_c':float(s.y[2,sec])} for sec in [60,120,300,600,776]}
 bulk.append({'bulk_capacity_j_k':cf,'fit_objective':float(np.mean(best.fun**2)),
 'coil_rmse_c':float(np.sqrt(np.mean((ob[:,0]-m.hh)[m.valid]**2))),
 'ntc_rmse_c':float(np.sqrt(np.mean((ob[:,3]-m.nn)**2))),
 'bulk_final_video_c':float(pr[-1,2]),'parameters':list(map(float,unpack4(zz))),'predictions':predictions})
 print('bulk',cf,bulk[-1]['fit_objective'],flush=True)
result={'normalized_identifiable_model':ident,'physical_multistart_ranges':structural,
 'sensitivity_cases':samples,'bulk_models':bulk,
 'warning':'Scenario spread is not a confidence interval. Mass and frame temperature not measured. No 17 V data used.'}
(out/'sensitivity_pre17_v1.json').write_text(json.dumps(result,indent=2,ensure_ascii=False),encoding='utf-8')
print(json.dumps(result,indent=2),flush=True)
