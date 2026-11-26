import fs from 'node:fs';
import {performance} from 'node:perf_hooks';
import {create176} from './powered-wave.mjs';
import {magneticSelfTest} from './magnetic-core.mjs';
const manifest=JSON.parse(fs.readFileSync(new URL('mismatch-manifest.json',import.meta.url)));
const [freqS='100',dbS='12',lossS='5',loadS='600',enabledS='1',rateS='176400',magS='1',ratioS='4']=process.argv.slice(2);
const freq=+freqS,db=+dbS,loss=+lossS,load=+loadS,enabled=+enabledS===1,rate=+rateS,magnetics=+magS===1,ratio=+ratioS;
const amp=2*Math.SQRT2*.775*10**(db/20);
const engine=create176(manifest,{fs:rate,ratio,loss,load,enabled,magnetics});
const t0=performance.now(),duration=1.4,start=1.1;
let data=[],keys=null,n=0,maxIt=0,error=null;
console.log(JSON.stringify({inventory:engine.inventory,primitive:magneticSelfTest()}));
for(;n<Math.round(duration*rate);n++){
 const t=n/rate,x=amp*Math.sin(2*Math.PI*freq*t);
 try{
  const r=engine.step(x);
  if(!Object.values(r).every(Number.isFinite))throw Error('nonfinite');
  maxIt=Math.max(maxIt,r.iterations);
  if(t>=start){if(keys===null)keys=Object.keys(r);data.push([t,x,...keys.map(k=>r[k])].join(','));}
 }catch(e){error=String(e);break;}
}
fs.writeFileSync('magnetic-wave.csv',['time,input',...(keys??[])].join(',')+'\n'+data.join('\n')+'\n');
const report={freq,db,loss,load,enabled,rate,magnetics,ratio,n,maxIt,error,elapsedMs:performance.now()-t0,core:engine.magneticDiagnostics()};
fs.writeFileSync('magnetic-meta.json',JSON.stringify(report,null,2));console.log(JSON.stringify(report,null,2));
if(error)process.exitCode=1;
