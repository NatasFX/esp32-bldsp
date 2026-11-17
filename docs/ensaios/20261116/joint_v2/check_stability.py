"""Dissipation and bounded-input stress checks; no MCU writes."""
from pathlib import Path
import json,numpy as np
from scipy.integrate import solve_ivp
from scipy.optimize import brentq
from scipy.linalg import expm
import matplotlib;matplotlib.use('Agg')
import matplotlib.pyplot as plt
P=Path(__file__).resolve().parent;m=json.loads((P/'model_joint_v2.json').read_text());th=m['thermal'];e=m['electrical'];cc=th['coil_capacity_j_k'];cb=th['body_capacity_j_k'];ga=th['coil_ambient_w_k'];gc=th['coil_body_w_k'];gb=th['body_ambient_w_k'];tau=th['sensor_tau_s'];wc=th['sensor_coil_weight'];wb=th['sensor_body_weight'];ta=27.8
A=np.array([[-(ga+gc)/cc,gc/cc,0],[gc/cb,-(gc+gb)/cb,0],[wc/tau,wb/tau,-1/tau]])
def power(x,v):
 rt=e['r_at_reference_ohm']+e['slope_ohm_per_c']*(ta+x-e['reference_c']);return (v/rt)**2*(rt-e['wire_r_assumed_ohm'])
def rhs(t,y,v):
 x,b,n=y;p=power(x,v);return [(p-ga*x-gc*(x-b))/cc,(gc*(x-b)-gb*b)/cb,(wc*x+wb*b-n)/tau]
res={'eigenvalues_per_s':np.linalg.eigvals(A).tolist(),'time_constants_s':(-1/np.linalg.eigvals(A)).tolist(),'positive_conductances':bool(min(ga,gc,gb)>0),'positive_capacities':bool(min(cc,cb)>0),'equilibria':{},'stress':[]}
for v in [11,15,17,24]:
 g=ga+gc*gb/(gc+gb);x=brentq(lambda x:power(x,v)-g*x,0,2000);b=gc/(gc+gb)*x;n=wc*x+wb*b
 s=solve_ivp(lambda t,y:rhs(t,y,v),(0,72*3600),[0,0,0],rtol=1e-8,atol=1e-9,max_step=30)
 err=float(max(abs(s.y[:,-1]-[x,b,n])));assert err<1e-5 and np.isfinite(s.y).all()
 res['equilibria'][str(v)]={'coil_c':ta+x,'body_equivalent_c':ta+b,'ntc_c':ta+n,'after_72h_error_to_equilibrium_c':err}
# Continuous 72h cycle simulation, including final 12h without heating.
state=np.array([1.2,(2.2-wc*1.2)/wb,2.2]);all_t=[];all_y=[]
for hour in range(72):
 v=17 if hour<60 and hour%2==0 else 0
 sol=solve_ivp(lambda t,y:rhs(t,y,v),(hour*3600,(hour+1)*3600),state,rtol=1e-8,atol=1e-9,max_step=20,dense_output=True);state=sol.y[:,-1];tt=np.linspace(hour*3600,(hour+1)*3600,61);all_t.extend(tt);all_y.extend(sol.sol(tt).T)
assert max(abs(state))<1e-5
res['stress'].append({'scenario':'60h alternating 1h 17V / 1h off, then 12h off','final_delta_c':state.tolist(),'peak_delta_c':np.max(all_y,axis=0).tolist()})
# Off-state contraction: deliberately wrong hidden body +60C.
err0=np.array([0.,60.,0.]);steps=[0,60,300,900,1800,3600,7200];errs=[(expm(A*t)@err0).tolist() for t in steps]
res['initial_body_error_60c_decay']=[{'time_s':t,'coil_error_c':r[0],'body_error_c':r[1],'ntc_error_c':r[2]} for t,r in zip(steps,errs)]
assert max(abs(np.array(errs[-1])))<1e-5
# Energy proof for zero power: E'= -Gca*x² -Gba*b² -Gcb*(x-b)².
res['stability_argument']='For zero input, derivative of 0.5*(Cc*x^2+Cb*b^2) is -Gca*x^2-Gba*b^2-Gcb*(x-b)^2 <0 outside origin. Sensor is a stable first-order follower. Positive bounded heating gives bounded states; this is not a bound on model error against real hardware.'
(P/'stability_checks.json').write_text(json.dumps(res,indent=2))
fig,ax=plt.subplots(2,1,figsize=(11,7));tt=np.array(all_t)/3600;yy=np.array(all_y)+ta
for j,label in enumerate(['Bobina','Estrutura equivalente','NTC']):ax[0].plot(tt,yy[:,j],label=label)
ax[0].set(xlabel='Horas',ylabel='°C',title='Teste numérico: 60 h de ciclos + 12 h desligado');ax[0].legend()
zz=np.arange(0,7201,10);ee=np.array([expm(A*t)@err0 for t in zz]);ax[1].plot(zz/60,ee[:,0],label='Erro transferido à bobina');ax[1].plot(zz/60,ee[:,1],label='Erro inicial da estrutura +60 °C');ax[1].plot(zz/60,ee[:,2],label='Erro no NTC');ax[1].set(xlabel='Minutos sem potência',ylabel='Erro (°C)',title='O erro inicial decai; pode gerar um transitório antes de desaparecer');ax[1].legend()
for a in ax:a.grid(alpha=.2)
fig.tight_layout();fig.savefig(P/'stability_checks.png',dpi=150);print(json.dumps(res,indent=2))
