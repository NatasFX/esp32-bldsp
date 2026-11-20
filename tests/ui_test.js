// Exercise the actual embedded application, using fake DOM/network/clock.
const fs=require('node:fs'),vm=require('node:vm'),assert=require('node:assert/strict'),path=require('node:path');
const html=fs.readFileSync(path.join(__dirname,'../main/index.html'),'utf8');
const elements=new Map(),bodyClasses=new Map(),document={body:{classList:{toggle(name,on){bodyClasses.set(name,on);}}},getElementById:element,createElement:()=>({}),querySelectorAll:()=>[],documentElement:{style:{setProperty(){}}}};
function element(id){if(!elements.has(id))elements.set(id,{value:'',checked:false,disabled:false,textContent:'',append(){},classList:{toggle(){}},focus(){document.activeElement=this;},select(){},blur(){document.activeElement=null;this.onblur?.();},setAttribute(k,v){this[k]=v;},getContext(){return null;},addEventListener(name,fn){this[name]=fn;},reportValidity(){return true;}});return elements.get(id);}
const seed=JSON.parse(fs.readFileSync(path.join(__dirname,'../preset1_acp.json'),'utf8'));
let bank={sub:seed.sub.slice(),stereo:seed.stereo.slice()},calls=[],confirms=0,holdNext=false,release;
const storage=new Map(),timers=new Map();let timerId=0;
const context=vm.createContext({URL,document,window:{addEventListener(){}},console,confirm:()=>{confirms++;return true;},
 localStorage:{getItem:k=>storage.get(k)||null,setItem:(k,v)=>storage.set(k,v),removeItem:k=>storage.delete(k)},
 setTimeout:(fn,ms)=>{const id=++timerId;timers.set(id,{fn,ms});return id;},clearTimeout:id=>timers.delete(id),
 fetch:async(url,opts)=>{
  calls.push({url,opts});if(url==='/api/status')return {ok:true,json:async()=>({demo:true})};
  if(url==='/api/source/pulse')return {ok:true,json:async()=>({accepted:true,pulse_ms:100,demo:true})};
  if(url==='/api/clock')return {ok:true,json:async()=>({lighting:{clock_valid:true}})};
  if(url==='/api/lighting')return {ok:true,json:async()=>({lighting:{...JSON.parse(opts.body),clock_valid:true,unix_seconds:1795000000,utc_offset_minutes:-180,solar_valid:true,sunrise_minute:330,sunset_minute:1150,applied:true,brightness:60}})};
  if(url==='/api/strip')return {ok:true,json:async()=>({strip:JSON.parse(opts.body)})};
  if(url==='/api/wifi')return {ok:true,json:async()=>({wifi:{available:true,power_qdbm:JSON.parse(opts.body).power_qdbm,power_dbm:JSON.parse(opts.body).power_qdbm/4}})};
  if(url==='/api/panel')return {ok:true,json:async()=>({panel:{...JSON.parse(opts.body),online:true,revision:1}})};
  if(url==='/api/protection')return {ok:true,json:async()=>({config:JSON.parse(opts.body).config||{enabled:false},attenuation_db:0,state:'disabled'})};
  if(url==='/api/telemetry')return {ok:true,json:async()=>({temperature:{status:'unavailable',celsius:null},clip:{status:'uncalibrated',active:null}})};
  if(url==='/api/telemetry/history')return {ok:true,json:async()=>({uptime_ms:300000,samples:[{at_ms:298000,temperature_c:20},{at_ms:299000,temperature_c:null},{at_ms:300000,temperature_c:30}],events:[{at_ms:500,code:'started',detail:0},{at_ms:120000,code:'clip_on',detail:0}]})};
  if(url==='/api/telemetry/ldr/capture')return {ok:true,json:async()=>({uptime_ms:5000,threshold:2600,samples:[{at_ms:3000,raw:2000},{at_ms:4000,raw:300},{at_ms:5000,raw:2100}]})};
  if(url==='/api/telemetry/ldr'&&opts?.method==='POST')return {ok:true,json:async()=>({temperature:{status:'estimated',celsius:25},clip:{status:'provisional',manual:true,threshold:JSON.parse(opts.body).threshold,raw:2000,active:false}})};
  const key=url.includes('stereo')?'stereo':'sub';
  if(opts?.method==='POST'){
   const words=JSON.parse(opts.body).words.slice();
   if(holdNext){holdNext=false;await new Promise(resolve=>release=resolve);}
   bank[key]=words.slice();return {ok:true,json:async()=>({words,matches:true,demo:true})};
  }
  throw Error('Unexpected read '+url);
 }});
vm.runInContext(html.match(/<script>([\s\S]*?)<\/script>/)[1],context);
const run=code=>vm.runInContext(code,context),flush=()=>run('flushOutputs()');
const posts=()=>calls.filter(c=>c.opts?.method==='POST');
const settle=async()=>{for(let i=0;i<8;i++)await new Promise(resolve=>setImmediate(resolve));};
function change(id,value){element(id).value=String(value);element('editor').input();}
(async()=>{
 await settle();assert.equal(element('sync-status')['data-state'],'pending');assert.equal(run('pending.size'),2);for(let i=0;i<10;i++)assert.equal(element('gain-slider'+i).disabled,true,'right slider starts locked');assert.equal(posts().length,0);
 assert.deepEqual(JSON.parse(storage.get('bldsp-preset-v1-1')).sub,seed.sub);
 await flush();assert.equal(element('sync-status')['data-state'],'synced');assert.equal(run('dirty'),false);assert.equal(posts().length,2);assert.deepEqual(bank.sub,seed.sub);assert.deepEqual(bank.stereo,seed.stereo);
 assert.ok(!html.includes('id="read"')&&!html.includes('id="apply"')&&!html.includes('id="sample-rate"'));
 const start=posts().length;change('gain5',1);change('gain5',2);change('gain5',3);
 assert.equal(run('pending.size'),1);assert.equal(posts().length,start);await flush();assert.equal(posts().length,start+1);assert.equal(bank.sub[32],768);assert.equal(run('dirty'),false);
 change('freq5',23);assert.equal(element('sync-status')['data-state'],'error');await flush();assert.equal(posts().length,start+1,'invalid edit cannot be sent');change('freq5',35);await flush();assert.equal(element('sync-status')['data-state'],'synced');
 // In-flight acknowledgement of an old slider value must not replace newer input.
 change('gain5',4);holdNext=true;const inFlight=flush();assert.equal(typeof release,'function');
 change('gain5',5);release();await inFlight;assert.equal(bank.sub[32],1280);assert.equal(Number(element('gain5').value),5);
 element('en0').checked=false;element('en0').onchange();assert.equal(element('band-body0').hidden,true);await flush();assert.equal(bank.sub[3],0);
 element('en0').checked=true;element('en0').onchange();await flush();assert.equal(bank.sub[3],1);
 element('gain2').onclick({timeStamp:0});assert.equal(element('gain-slider2').disabled,false);
 element('q0').onclick({timeStamp:100});assert.equal(element('gain-slider2').disabled,true,'selecting another parameter relocks the previous slider');assert.equal(element('q0').readOnly,true);assert.equal(element('gain-slider0')['data-param'],'q');
 element('q0').onclick({timeStamp:200,preventDefault(){}});assert.equal(element('q0').readOnly,false);
 element('q0').value='0.123456';element('q0').blur();assert.equal(element('q0').value,'0.123');
 element('gain-slider0').value=0;element('gain-slider0').oninput();element('editor').input();await flush();assert.equal(bank.sub[6],1);
 element('gain-slider0').value=1000;element('gain-slider0').oninput();element('editor').input();await flush();assert.equal(bank.sub[6],10240);await run("switchOutput('stereo')");for(let i=0;i<10;i++)assert.equal(element('gain-slider'+i).disabled,true);await run("switchOutput('sub')");
 assert.equal(run('pregainPosition(0)'),667);for(const pos of [621,640,667,690,721])assert.equal(run(`pregainSliderDb(${pos})`),0);
 element('pregain-slider').value=1000;element('pregain-slider').oninput();await flush();assert.equal(bank.sub[1],768);
 element('pregain-slider').value=0;element('pregain-slider').oninput();await flush();assert.equal(bank.sub[1],32768);
 element('pregain-slider').value=667;element('pregain-slider').oninput();await flush();
 // Saving a preset must not issue USB/eq or DSP-flash commands.
 const beforeSave=posts().length;await element('preset-save').onclick();assert.equal(posts().length,beforeSave);
 const saved=JSON.parse(storage.get('bldsp-preset-v1-1'));assert.equal(saved.sub[1],0);
 change('gain5',-2);const beforeConfirm=confirms;await element('preset-1').onclick();assert.equal(confirms,beforeConfirm);await flush();assert.equal(bank.sub[32],saved.sub[32]);
 await element('preset-reset').onclick();await flush();assert.equal(bank.sub[3],0);assert.equal(bank.stereo[3],0);assert.equal(bank.sub[1],0);
 for(let slot=1;slot<=4;slot++)run(`demoPreset('/api/preset?slot=${slot}',{action:'save',sub:flatWords(),stereo:flatWords()})`);
 assert.equal([...storage.keys()].filter(k=>k.startsWith('bldsp-preset-v1-')).length,4);
 assert.throws(()=>run("demoPreset('/api/preset?slot=5')"));
 for(const c of posts()){assert.ok(c.url.startsWith('/api/eq?output='));assert.equal(c.opts.headers['X-BLDSP'],'1');}
 assert.ok(!calls.some(c=>c.url==='/api/save'));
 // Source is a one-shot toggle: never invoked at boot or retried automatically.
 assert.equal(calls.filter(c=>c.url==='/api/source/pulse').length,0);
 const pulse=element('source-pulse').onclick();await element('source-pulse').onclick();await pulse;
 const sourceCalls=calls.filter(c=>c.url==='/api/source/pulse');assert.equal(sourceCalls.length,1);
 assert.equal(sourceCalls[0].opts.body,'{}');assert.equal(sourceCalls[0].opts.headers['X-BLDSP'],'1');
 assert.equal(element('source-pulse').disabled,true);
 const sourceUnlock=[...timers.values()].find(t=>t.ms===1500);assert.ok(sourceUnlock);sourceUnlock.fn();
 assert.equal(element('source-pulse').disabled,false);
 // Opening monitoring must preserve unsaved EQ and never interpret absent sensors as zero/normal.
 const draftBefore=run('JSON.stringify(outputs.sub.draft)'),beforeMonitorPosts=posts().length;
 element('view-monitor').onclick();await settle();
 assert.equal(element('eq-view').hidden,true);assert.equal(element('monitor-view').hidden,false);
 assert.equal(element('monitor-temperature').textContent,'—');assert.equal(element('monitor-clip').textContent,'Sem leitura');
 assert.equal(element('monitor-temp-min').textContent,'20,0 °C');assert.equal(element('monitor-temp-avg').textContent,'25,0 °C');assert.equal(element('monitor-temp-max').textContent,'30,0 °C');
 assert.ok(element('monitor-events').textContent.includes('00:02:00 · LED CLIP acendeu'),'events from before page load must appear');
 assert.equal(posts().length,beforeMonitorPosts);
 assert.equal(calls.filter(c=>c.url==='/api/telemetry/ldr/capture').length,0,'RAW capture must be manual');
 await element('monitor-raw-refresh').onclick();
 assert.ok(element('monitor-raw-extremes').textContent.includes('Mín: 300 · Máx: 2100'));
 assert.ok(element('monitor-raw-range').textContent.includes('limiar RAW: 2600'));
 const frozenRaw=element('monitor-raw-path').d;
 run("renderMonitor({temperature:{status:'estimated',celsius:25},clip:{status:'warming_up',baseline_raw:4000,active:null,count:0}})");
 assert.equal(element('monitor-raw-path').d,frozenRaw,'live polling must preserve the frozen capture');
 assert.ok(element('monitor-cal-state').textContent.includes('60 s'));
 assert.equal((element('monitor-temp-path').d.match(/M/g)||[]).length,2,'missing samples create a gap');
 assert.equal(run('JSON.stringify(outputs.sub.draft)'),draftBefore);
 run("renderMonitor({temperature:{status:'estimated',celsius:32.4,resistance_ohm:7200,millivolts:1380},clip:{status:'ok',active:true,count:3}})");
 assert.equal(element('monitor-temperature').textContent,'32,4');assert.equal(element('monitor-clip').textContent,'CLIP detectado');
 run("renderMonitor({temperature:{status:'estimated',celsius:32.4},coil:{status:'estimated',celsius:110.5,ntc_mean10_c:32.1,ambient_c:27.8},clip:{status:'ok',active:false}})");
 assert.equal(element('monitor-coil').textContent,'110,5');assert.equal(Number(element('monitor-ambient').value),27.8);
 run("renderMonitor({temperature:{status:'estimated',celsius:32.4},coil:{status:'warming_up',celsius:null},clip:{status:'ok',active:false}})");
 assert.equal(element('monitor-coil').textContent,'—');
 run('monitorUnavailable()');assert.equal(element('monitor-coil').textContent,'—');assert.equal(element('monitor-temperature').textContent,'—');assert.equal(element('monitor-clip').textContent,'Sem leitura');
 run("renderGlobalClip({clip:{status:'rail_high',active:false}})");assert.equal(element('clip-banner')['data-state'],'warning');assert.equal(bodyClasses.get('clip-flashing'),false);
 run("renderGlobalClip({clip:{status:'rail_high',active:true}})");assert.equal(bodyClasses.get('clip-flashing'),false,'isolated clip only colors pill');assert.equal(element('clip-banner')['data-state'],'clip');
 run("renderGlobalClip({clip:{status:'ok',active:true,flash:true}})");assert.equal(bodyClasses.get('clip-flashing'),true);
 run("renderGlobalClip({clip:{status:'ok',active:false,flash:false},coil:{status:'estimated',celsius:62.4},temperature:{status:'estimated',celsius:34.2}})");assert.equal(element('main-coil').textContent,'62,4 °C');assert.equal(element('main-ntc').textContent,'34,2 °C');
 run("renderGlobalClip(null)");assert.equal(bodyClasses.get('clip-flashing'),false);assert.equal(element('clip-banner')['data-state'],'unknown');
 const settings={'protect-events':3,'protect-window':2,'protect-step':.5,'protect-interval':1,'protect-max':12,'protect-quiet':8,'protect-recover-step':.25,'protect-recover-interval':2};
 for(const [id,value] of Object.entries(settings))element(id).value=String(value);
 element('protect-enabled').checked=false;element('protect-recovery').checked=true;
 await element('protect-save').onclick();assert.equal(calls.at(-1).url,'/api/protection');assert.equal(JSON.parse(calls.at(-1).opts.body).config.enabled,false);
 assert.equal(JSON.parse(calls.at(-1).opts.body).config.quiet_ms,8000);
 element('protect-step').value='99';const countBefore=calls.length;await element('protect-save').onclick();assert.equal(calls.length,countBefore,'invalid protection settings never send');
 element('view-eq').onclick();assert.equal(element('monitor-view').hidden,true);assert.equal(element('eq-view').hidden,false);
 element('view-panel').onclick();assert.equal(element('panel-view').hidden,false);assert.equal(element('eq-view').hidden,true);assert.equal(element('monitor-view').hidden,true);assert.equal(element('view-panel')['aria-selected'],'true');assert.equal(element('page-title').textContent,'Painel');
 element('view-monitor').onclick();assert.equal(element('panel-view').hidden,true);
 element('view-eq').onclick();assert.equal(element('panel-view').hidden,true);
 // Physical panel mapping persists separately and must never issue EQ writes.
 run("renderPanel({online:true,enabled:false,revision:1,bindings:Array.from({length:5},(_,i)=>({module:i?145:147,selector:i?7+5*(i-1):1,min_db:i?-12:-60,max_db:i?12:0,position:32768,gain_db:0,picked:false,valid:true}))})");
 assert.ok(element('panel-link').textContent.includes('conectado'));
 assert.equal(element('pot-percent0').textContent,'50,0 %');assert.equal(element('pot-meter0').value,32768);
 assert.match(element('pot-state0').textContent,/Painel desativado/);
 const beforeLiveEQ=calls.filter(c=>c.url.startsWith('/api/eq')).length;
 run("renderPanel({online:true,enabled:true,sequence:42,age_ms:30,revision:1,bindings:[{module:147,selector:1,min_db:-60,max_db:0,position:16384,adc_raw:1200,adc_mv:620,adc_calibrated:true,target_gain_db:-45,gain_db:-10,picked:false,valid:true,active:true}]})");
 assert.match(element('pot-adc0').textContent,/ADC 1200 \/ 4095 · 620 mV · calibrado/);assert.equal(element('pot-percent0').textContent,'25,0 %');assert.equal(element('pot-request0').textContent,'-45,00 dB');assert.equal(element('pot-dsp0').textContent,'-10,00 dB');assert.match(element('pot-pickup0').textContent,/aumentar/);assert.match(element('panel-live-state').textContent,/42/);
 element('pot-min0').value='-50';element('pot-min0').oninput();
 run("renderPanel({online:true,bindings:[],vcc:{valid:true,volts:14.4,mv:1846,raw:3000,duration_us:65,calibrated:false}})");
 assert.match(element('panel-vcc').textContent,/14\.40 V/);
 assert.match(element('panel-vcc').textContent,/aproximada/);
 run("renderPanel({online:false,bindings:[],vcc:{valid:true,volts:14.4,error:'Sem amostra recente'}})");
 assert.match(element('panel-vcc').textContent,/indisponível/);
 run("renderPanel({online:true,bindings:[],vcc:{valid:false,error:'ESP_ERR_TIMEOUT'}})");
 assert.match(element('panel-vcc').textContent,/ESP_ERR_TIMEOUT/);
 run("renderPanel({online:true,enabled:true,revision:1,bindings:[{module:147,selector:1,min_db:-60,max_db:0,position:49151,target_gain_db:-15,gain_db:-10,picked:true,valid:true,active:true}]})");
 assert.equal(element('pot-min0').value,'-50','live readings preserve unsaved mapping');assert.match(element('pot-state0').textContent,/Controlando/);assert.equal(element('pot-pickup0').textContent,'');
 run("renderPanel({online:false,enabled:true,revision:1,bindings:[{module:147,selector:1,min_db:-60,max_db:0,position:49151,gain_db:null,picked:false,valid:false}]})");
 assert.equal(element('pot-live0')['data-stale'],true);assert.equal(element('pot-dsp0').textContent,'— dB');assert.match(element('pot-state0').textContent,/Sem enlace/);
 run("renderPanel({online:true,enabled:true,revision:1,bindings:[{module:147,selector:1,min_db:-60,max_db:0,position:0,adc_raw:4095,adc_mv:2938,adc_calibrated:true,gain_db:0,picked:false,valid:true}]})");
 assert.match(element('pot-adc0').textContent,/ADC saturado/);assert.equal(element('pot-percent0').textContent,'0,0 %');
 assert.equal(calls.filter(c=>c.url.startsWith('/api/eq')).length,beforeLiveEQ,'debug rendering never applies gains');
 // Firmware snapshots may retain zero frequency in disabled bands.
 // Normalize before validation, update both outputs, and never echo telemetry.
 run("{const w=outputs.sub.draft.slice();w[3]=0;w[5]=0;w[32]=6*256;renderPanel({revision:900,bindings:[],sub:w});}");
 assert.equal(Number(element('gain5').value),6,'physical filter gain reaches editor');
 assert.equal(run('outputs.sub.draft[5]'),24,'disabled zero frequency is normalized');
 assert.equal(run('outputs.sub.draft[32]'),1536);
 change('gain5',7);
 run("{const w=outputs.sub.words.slice();w[32]=2*256;renderPanel({revision:901,bindings:[],sub:w});}");
 assert.equal(Number(element('gain5').value),7,'telemetry cannot overwrite pending local edits');
 await flush();
 run("{const w=outputs.sub.words.slice();w[32]=8*256;renderPanel({revision:902,bindings:[],sub:w});}");
 assert.equal(Number(element('gain5').value),8,'next confirmed revision reaches editor');
 element('pot-min0').value='-60';

 const beforePanelEQ=calls.filter(c=>c.url.startsWith('/api/eq')).length;
 element('panel-enabled').checked=true;await element('panel-save').onclick();
 assert.equal(calls.at(-1).url,'/api/panel');assert.equal(JSON.parse(calls.at(-1).opts.body).bindings.length,5);
 assert.equal(calls.filter(c=>c.url.startsWith('/api/eq')).length,beforePanelEQ);
 element('pot-target2').value=element('pot-target1').value;const beforeDuplicate=calls.length;await element('panel-save').onclick();assert.equal(calls.length,beforeDuplicate);assert.ok(element('panel-message').textContent.includes('apenas um'));
 // Wi-Fi power uses real driver steps, preserves pending edits, and has a 2dBm shortcut.
 run("renderWifi({available:true,power_qdbm:8,power_dbm:2})");assert.equal(element('wifi-power').value,'8');
 element('wifi-power').value='20';element('wifi-power').oninput();run("renderWifi({available:true,power_qdbm:8,power_dbm:2})");assert.equal(element('wifi-power').value,'20');
 await element('wifi-save').onclick();assert.equal(calls.at(-1).url,'/api/wifi');assert.equal(JSON.parse(calls.at(-1).opts.body).power_qdbm,20);assert.ok(element('wifi-message').textContent.includes('5 dBm'));
 await element('wifi-low').onclick();assert.equal(JSON.parse(calls.at(-1).opts.body).power_qdbm,8);
 const invalidPowerCalls=calls.length;await run('applyWifiPower(9)');assert.equal(calls.length,invalidPowerCalls);
 run('renderStrip({enabled:true,count:30,brightness:30,mode:0})');assert.equal(Number(element('strip-count').value),30);
 await element('strip-save').onclick();assert.equal(calls.at(-1).url,'/api/strip');assert.equal(JSON.parse(calls.at(-1).opts.body).count,30);
 element('strip-palette').value='3';element('strip-palette').oninput();assert.match(element('strip-palette-preview').style,/linear-gradient/);await element('strip-save').onclick();assert.equal(JSON.parse(calls.at(-1).opts.body).palette,3);
 element('strip-palette').value='99';const beforeBadPalette=calls.length;await element('strip-save').onclick();assert.equal(calls.length,beforeBadPalette);element('strip-palette').value='0';
 element('strip-count').value='151';const beforeInvalidStrip=calls.length;await element('strip-save').onclick();assert.equal(calls.length,beforeInvalidStrip);
 assert.equal(run("parseLightSteps('sunrise,60\\n12:00,100\\n17:00,60\\nsunset+30,15').length"),4);
 assert.throws(()=>run("parseLightSteps('25:00,60\\n12:00,100')"));
 assert.throws(()=>run("parseLightSteps('12:00,101\\nsunset,15')"));
 assert.throws(()=>run("parseLightSteps('12:00,60\\n12:00,100')"));
 element('light-enabled').checked=true;element('light-smooth').checked=true;element('light-lat').value='-30';element('light-lon').value='-53';element('light-steps').value='sunrise-1,15\nsunrise,60\n12:00,100\n17:00,60\nsunset+30,15';
 await element('light-save').onclick();assert.equal(calls.at(-1).url,'/api/lighting');assert.equal(JSON.parse(calls.at(-1).opts.body).steps.length,5);
 assert.match(element('light-live').textContent,/Automático: 60%/);
 run("demo=false;clockAt=0");await run('syncClock()');assert.equal(calls.at(-1).url,'/api/clock');const clockRequests=calls.length;await run('syncClock()');assert.equal(calls.length,clockRequests,'clock sync does not spam telemetry cadence');
 const beforeBootWrites=calls.filter(c=>c.url.startsWith('/api/eq')).length;
 run("{const live=outputs.sub.words.slice();live[1]=(-12*256)&65535;initializeEQ({sub:flatWords(),stereo:flatWords()},{sub:live,stereo:outputs.stereo.words},false);}");
 assert.equal(Number(element('pregain').value),-12);assert.equal(run('pending.size'),0);
 assert.equal(calls.filter(c=>c.url.startsWith('/api/eq')).length,beforeBootWrites,'portal boot never overwrites physical panel state');
 run("initializeEQ({sub:flatWords(),stereo:flatWords()},null,false)");assert.equal(run('pending.size'),0,'no startup write even while DSP snapshot is not ready');
 console.log('UI: preset-1 boot, automatic two-output apply, coalescing, invalid-edit suppression, in-flight latest-wins, toggles, Q, double tap, volume, preset save isolation and reset OK');
})().catch(e=>{console.error(e);process.exitCode=1;});
