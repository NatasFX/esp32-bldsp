const fs=require('node:fs'),vm=require('node:vm'),assert=require('node:assert/strict');
const source=fs.readFileSync(require('node:path').join(__dirname,'../main/panel-update.html'),'utf8').match(/<script>([\s\S]*?)<\/script>/)[1];
function harness({chip=5,commit=true}={}){
 const elements=new Map(),e=id=>{if(!elements.has(id))elements.set(id,{disabled:false,textContent:'',value:0,files:[]});return elements.get(id);};
 let now=0,session=0,offset=0,busy=false,boot=1,pending=false,polls=0,lost=false,finished=false;
 const calls=[],image=new Uint8Array(9000);image[0]=0xe9;image[12]=chip;image[23]=1;image.set(Buffer.from('bldsp_panel_c3'),80);image.fill(0xab,image.length-32);
 const hash='ab'.repeat(32),status=()=>{
  if(finished&&commit){boot=2;pending=++polls<3;}
  return {online:true,pending,busy,session,offset,total:image.length,boot,address:0x10000,sha256:boot===2?hash:'00'.repeat(32)};
 };
 e('firmware').files=[{name:'bldsp_panel_c3.bin',size:image.length,arrayBuffer:async()=>image.buffer}];
 const ctx=vm.createContext({document:{getElementById:e},Uint8Array,DataView,Date:{now:()=>now},setTimeout:(fn,ms)=>{now+=ms;fn();},console,
 fetch:async(url,opt={})=>{
  calls.push({url,opt});const reply=j=>({ok:true,json:async()=>j});
  if(url==='/api/panel/ota')return reply(status());
  if(url.endsWith('/begin')){session=123;busy=true;return reply(status());}
  if(url.includes('/data?')){
   const start=Number(new URL('http://esp'+url).searchParams.get('offset'));
   assert.equal(start,offset,'retry resumes from device-confirmed position');
   if(!lost){offset=2000;lost=true;throw Error('response lost after partial block');}
   offset+=opt.body.length;return reply(status());
  }
  if(url.endsWith('/end')){assert.equal(offset,image.length);finished=true;busy=false;throw Error('final response lost');}
  if(url.endsWith('/abort')){busy=false;return reply(status());}
  throw Error('unexpected '+url);
 }});vm.runInContext(source,ctx);
 return {e,calls,run:code=>vm.runInContext(code,ctx)};
}
(async()=>{
 const good=harness();await good.run('refresh()');await good.e('firmware').onchange();assert.equal(good.e('upload').disabled,false);
 await good.e('upload').onclick();assert.match(good.e('message').textContent,/concluída e confirmada/);assert.equal(good.e('progress').value,100);
 const bad=harness({chip:9});await bad.run('refresh()');await bad.e('firmware').onchange();assert.equal(bad.e('upload').disabled,true);assert.match(bad.e('file-info').textContent,/Selecione/);assert.equal(bad.calls.filter(c=>c.url.endsWith('/begin')).length,0);
 const rollback=harness({commit:false});await rollback.run('refresh()');await rollback.e('firmware').onchange();await rollback.e('upload').onclick();assert.match(rollback.e('message').textContent,/não foi confirmada/);
 console.log('OTA UI: C3/S3 file gating, partial-block resume, lost finish response, new-boot hash confirmation and failed boot passed');
})().catch(e=>{console.error(e);process.exitCode=1;});
