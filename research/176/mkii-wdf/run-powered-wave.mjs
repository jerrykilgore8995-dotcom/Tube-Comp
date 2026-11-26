import fs from 'node:fs';
import {performance} from 'node:perf_hooks';
import {create176} from './powered-wave.mjs';
import {selfTest} from './wave-core.mjs';
const dir=new URL('.',import.meta.url);
const manifest=JSON.parse(fs.readFileSync(new URL('mismatch-manifest.json',dir),'utf8'));
const [rateArg='44100',ratioArg='2',ampArg='3.474135043921514',out='wave.csv',enabledArg='1',freqArg='1000',lossArg='20']=process.argv.slice(2);
const rate=Number(rateArg),ratio=Number(ratioArg),amp=Number(ampArg),enabled=Number(enabledArg)!==0,freq=Number(freqArg),loss=Number(lossArg);
const engine=create176(manifest,{fs:rate,ratio,enabled,loss});
const samples=Math.ceil(1.2*rate),saveStart=Math.floor(1.0*rate);
let lines=['time,audio,cv,rect,reg,bplus,iterations'],maxIt=0,failures=0;
const start=performance.now();
console.log(JSON.stringify({primitiveTests:selfTest(),inventory:engine.inventory,rate,ratio,amp,enabled,freq,loss}));
for(let n=0;n<=samples;n++){
 const t=n/rate;
 try{
 const r=engine.step(amp*Math.sin(2*Math.PI*freq*t));maxIt=Math.max(maxIt,r.iterations);
 if(!Object.values(r).every(Number.isFinite))throw Error('nonfinite output');
 if(n>=saveStart)lines.push([t,r.audio,r.cv,r.rect,r.reg,r.bplus,r.iterations].join(','));
 }catch(e){failures++;console.error(JSON.stringify({sample:n,time:t,error:String(e)}));break;}
}
fs.writeFileSync(out,lines.join('\n')+'\n');
const elapsed=performance.now()-start;
fs.writeFileSync(out+'.json',JSON.stringify({rate,ratio,amp,enabled,freq,loss,samples,failures,maxIt,elapsedMs:elapsed},null,2));
if(failures)process.exitCode=1;
