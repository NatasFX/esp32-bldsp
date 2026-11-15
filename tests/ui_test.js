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
 console.log('UI: preset-1 boot, automatic two-output apply, coalescing, invalid-edit suppression, in-flight latest-wins, toggles, Q, double tap, volume, preset save isolation and reset OK');
})().catch(e=>{console.error(e);process.exitCode=1;});
