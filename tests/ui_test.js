// Exercise the actual embedded application, using fake DOM/network/clock.
const fs=require('node:fs'),vm=require('node:vm'),assert=require('node:assert/strict'),path=require('node:path');
const html=fs.readFileSync(path.join(__dirname,'../main/index.html'),'utf8');
const elements=new Map(),document={getElementById:element,createElement:()=>({}),querySelectorAll:()=>[],documentElement:{style:{setProperty(){}}}};
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
  if(url==='/api/telemetry')return {ok:true,json:async()=>({temperature:{status:'unavailable',celsius:null},clip:{status:'uncalibrated',active:null}})};
  if(url==='/api/telemetry/history')return {ok:true,json:async()=>({uptime_ms:300000,samples:[{at_ms:298000,temperature_c:20},{at_ms:299000,temperature_c:null},{at_ms:300000,temperature_c:30}],events:[{at_ms:500,code:'started',detail:0},{at_ms:120000,code:'clip_on',detail:0}]})};
  if(url==='/api/telemetry/ldr/capture')return {ok:true,json:async()=>({uptime_ms:5000,samples:[{at_ms:3000,raw:2000},{at_ms:4000,raw:300},{at_ms:5000,raw:2100}]})};
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
 element('monitor-limit').value='450';await element('monitor-raw-refresh').onclick();
 assert.ok(element('monitor-raw-extremes').textContent.includes('Mín: 300 · Máx: 2100'));
 const frozenRaw=element('monitor-raw-path').d;
 element('monitor-limit-slider').value='600';element('monitor-limit-slider').oninput();
 assert.equal(Number(element('monitor-limit').value),600);assert.equal(element('monitor-raw-path').d,frozenRaw,'threshold editing must not refresh samples');
 await element('monitor-limit-save').onclick();assert.equal(JSON.parse(calls.at(-1).opts.body).threshold,600);
 assert.equal((element('monitor-temp-path').d.match(/M/g)||[]).length,2,'missing samples create a gap');
 assert.equal(run('JSON.stringify(outputs.sub.draft)'),draftBefore);
 run("renderMonitor({temperature:{status:'estimated',celsius:32.4,resistance_ohm:7200,millivolts:1380},clip:{status:'ok',active:true,count:3}})");
 assert.equal(element('monitor-temperature').textContent,'32,4');assert.equal(element('monitor-clip').textContent,'CLIP detectado');
 run('monitorUnavailable()');assert.equal(element('monitor-temperature').textContent,'—');assert.equal(element('monitor-clip').textContent,'Sem leitura');
 element('view-eq').onclick();assert.equal(element('monitor-view').hidden,true);assert.equal(element('eq-view').hidden,false);
 console.log('UI: preset-1 boot, automatic two-output apply, coalescing, invalid-edit suppression, in-flight latest-wins, toggles, Q, double tap, volume, preset save isolation and reset OK');
})().catch(e=>{console.error(e);process.exitCode=1;});
