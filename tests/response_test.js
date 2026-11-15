const assert=require('node:assert/strict'),eq=require('../main/eq-response.js');
const near=(a,b,eps=1e-5)=>assert.ok(Math.abs(a-b)<eps,`${a} != ${b}`);
for(const fs of [44100,48000]){
 const db=(type,f,q,g,x)=>eq.magnitude(eq.coefficients(type,f,q,g,fs),x,fs);
 near(db(0,1000,2,6,1000),6);near(db(0,1000,2,-6,1000),-6);
 near(db(3,1000,Math.SQRT1_2,0,1000),-3.01029995664);
 near(db(4,1000,Math.SQRT1_2,0,1000),-3.01029995664);
 near(db(5,1000,2,0,1000),0);
 assert.ok(db(6,1000,2,0,1000)<-100);
 near(db(1,1000,Math.SQRT1_2,6,1),6,0.001);
 near(db(2,1000,Math.SQRT1_2,6,fs/2-1),6,0.001);
 for(let type=0;type<=6;type++)for(const f of [10,26,35,80,1000,20000])
  for(const q of [1/1024,.1,.6,2,10])for(const x of [10,35,1000,20000])assert.ok(Number.isFinite(db(type,f,q,6,x)));
}
const w=Array(53).fill(0);w[0]=1;w[1]=256*2;w[3]=1;w[4]=0;w[5]=1000;w[6]=2048;w[7]=256*6;
near(eq.total(w,1000),8);w[0]=0;near(eq.total(w,1000),0);
assert.equal(eq.coefficients(0,0,1,0),null);
console.log('Response: peak, LP/HP, bandpass, notch, shelves, bypass, pregain and finite extrema OK');
