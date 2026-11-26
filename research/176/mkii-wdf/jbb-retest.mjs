import fs from 'node:fs';
import {create176} from './powered-wave.mjs';
const manifest=JSON.parse(fs.readFileSync(new URL('mismatch-manifest.json',import.meta.url)));
const [fS='20',ratioS='4',attackS='.5',releaseS='.8',model='GE',rateS='44100',lossS='20']=process.argv.slice(2);
const f=+fS,ratio=+ratioS,attack=+attackS,release=+releaseS,rate=+rateS,loss=+lossS;
const amp=2*Math.SQRT2*.775*10**(.2);
function run(enabled){
 const e=create176(manifest,{fs:rate,ratio,attack,release,loss,enabled,magnetics:true,detectorModel:model});
 const duration=3,start=2.5,hs=Array.from({length:20},()=>[0,0]),cv=[0,0];let n=0,mean=0,min=Infinity,max=-Infinity,maxIt=0;
 const dc={};let last;
 for(let i=0;i<Math.round(duration*rate);i++){
  const t=i/rate,r=e.step(amp*Math.sin(2*Math.PI*f*t));last=r;
  if(!Object.values(r).every(Number.isFinite))throw Error('nonfinite sample '+i);
  maxIt=Math.max(maxIt,r.iterations);
  if(t<start)continue;
  n++;for(let j=0;j<20;j++){let ph=2*Math.PI*f*(j+1)*t;hs[j][0]+=r.audio*Math.cos(ph);hs[j][1]+=r.audio*Math.sin(ph);}
  cv[0]+=r.cv*Math.cos(4*Math.PI*f*t);cv[1]+=r.cv*Math.sin(4*Math.PI*f*t);mean+=r.cv;min=Math.min(min,r.cv);max=Math.max(max,r.cv);
  for(const k of ['bplus','rect','reg'])dc[k]=(dc[k]??0)+r[k];
 }
 const h=hs.map(x=>2*Math.hypot(...x)/n);
 return {enabled,amp:h[0],thd:100*Math.hypot(...h.slice(1))/h[0],harmonics:h.map(v=>20*Math.log10(Math.max(1e-15,v/h[0]))),cv2:2*Math.hypot(...cv)/n,cvMean:mean/n,cvPP:max-min,dc:Object.fromEntries(Object.entries(dc).map(([k,v])=>[k,v/n])),maxIt,n};
}
const off=run(false),on=run(true),result={revision:process.env.GITHUB_SHA??'local',f,ratio,attack,release,model,rate,loss,off,on,gr:20*Math.log10(off.amp/on.amp)};
console.log(JSON.stringify(result));fs.writeFileSync('jbb-results.json',JSON.stringify(result,null,2));
