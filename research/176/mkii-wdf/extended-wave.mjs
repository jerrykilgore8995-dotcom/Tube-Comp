import fs from 'node:fs';
import {performance} from 'node:perf_hooks';
import {create176} from './powered-wave.mjs';
const manifest=JSON.parse(fs.readFileSync(new URL('mismatch-manifest.json',import.meta.url)));
const [rateS,ratioS,freqS,dbS,mode='tone',attackS='.5',releaseS='.8']=process.argv.slice(2);
const fsr=+rateS,ratio=+ratioS,f=+freqS,db=+dbS,attack=+attackS,release=+releaseS;
const enabled=mode!=='off',amp=2*Math.SQRT2*.775*10**(db/20);
const engine=create176(manifest,{fs:fsr,ratio,enabled,attack,release});
const duration=mode==='burst'?3:1.4, start=mode==='burst'?.8:1.1;
let rows=['time,input,audio,cv,rect,reg,bplus,iterations'],maxIt=0,failures=0,n=0;
const t0=performance.now();
for(;n<Math.round(duration*fsr);n++){
 const t=n/fsr,level=mode==='burst'&&(t<1||t>=1.5)?amp*.001:amp;
 const x=level*Math.sin(2*Math.PI*f*t);
 try{
 const r=engine.step(x);
 if(!Object.values(r).every(Number.isFinite))throw Error('nonfinite');
 maxIt=Math.max(maxIt,r.iterations);
 if(t>=start)rows.push([t,x,r.audio,r.cv,r.rect,r.reg,r.bplus,r.iterations].join(','));
 }catch(e){failures++;console.error(n,t,String(e));break;}
}
fs.writeFileSync('wave.csv',rows.join('\n')+'\n');
fs.writeFileSync('wave-meta.json',JSON.stringify({fsr,ratio,f,db,mode,attack,release,amp,n,maxIt,failures,elapsed_ms:performance.now()-t0},null,2));
if(failures)process.exitCode=1;
