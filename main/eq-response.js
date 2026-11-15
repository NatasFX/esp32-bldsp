/* Theoretical RBJ biquads, following https://www.w3.org/TR/audio-eq-cookbook/ .
 * Not a measurement of the DSP, crossover chain, amplifier or loudspeaker.
 * Shelf uses Q; BPF uses constant 0 dB peak. Firmware equivalence is unverified.
 */
(function(root){
 'use strict';
 const signed=w=>w>32767?w-65536:w;
 function coefficients(type,f,q,gain,fs=48000){
  if(!Number.isFinite(f)||!Number.isFinite(q)||f<=0||f>=fs/2||q<=0)return null;
  const w=2*Math.PI*f/fs,c=Math.cos(w),s=Math.sin(w),a=s/(2*q),A=10**(gain/40),t=2*Math.sqrt(A)*a;
  switch(type){
   case 0:return [1+a*A,-2*c,1-a*A,1+a/A,-2*c,1-a/A];
   case 1:return [A*((A+1)-(A-1)*c+t),2*A*((A-1)-(A+1)*c),A*((A+1)-(A-1)*c-t),(A+1)+(A-1)*c+t,-2*((A-1)+(A+1)*c),(A+1)+(A-1)*c-t];
   case 2:return [A*((A+1)+(A-1)*c+t),-2*A*((A-1)+(A+1)*c),A*((A+1)+(A-1)*c-t),(A+1)-(A-1)*c+t,2*((A-1)-(A+1)*c),(A+1)-(A-1)*c-t];
   case 3:return [(1-c)/2,1-c,(1-c)/2,1+a,-2*c,1-a];
   case 4:return [(1+c)/2,-(1+c),(1+c)/2,1+a,-2*c,1-a];
   case 5:return [a,0,-a,1+a,-2*c,1-a];
   case 6:return [1,-2*c,1,1+a,-2*c,1-a];
   default:return null;
  }
 }
 function magnitude(b,f,fs=48000){
  if(!b)return 0;
  const w=2*Math.PI*f/fs,c=Math.cos(w),s=Math.sin(w),c2=Math.cos(2*w),s2=Math.sin(2*w);
  const nr=b[0]+b[1]*c+b[2]*c2,ni=-b[1]*s-b[2]*s2;
  const dr=b[3]+b[4]*c+b[5]*c2,di=-b[4]*s-b[5]*s2;
  return 10*Math.log10(Math.max(1e-24,(nr*nr+ni*ni)/Math.max(1e-24,dr*dr+di*di)));
 }
 function total(words,f,fs=48000){if(!words||!words[0])return 0;let db=signed(words[1])/256;for(let i=0;i<10;i++)db+=band(words,i,f,fs);return db;}
 // Gain words use Q8.8, including when evaluating an individual band.
 function band(words,i,f,fs=48000){const k=3+5*i;return words[k]?magnitude(coefficients(words[k+1],words[k+2],words[k+3]/1024,signed(words[k+4])/256,fs),f,fs):0;}
 const api={coefficients,magnitude,band,total};
 if(typeof module!=='undefined'&&module.exports)module.exports=api;
 else root.EqResponse=api;
})(typeof globalThis!=='undefined'?globalThis:this);
