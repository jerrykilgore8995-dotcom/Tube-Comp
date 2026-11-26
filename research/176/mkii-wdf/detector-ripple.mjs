import fs from 'node:fs';
import {create176} from './powered-wave.mjs';
const manifest=JSON.parse(fs.readFileSync(new URL('mismatch-manifest.json',import.meta.url)));
const [fS='20',dbS='4',ratioS='4',attackS='.5',releaseS='.8',rateS='176400',oldS='0',onS='1',lossS='20',model='GE']=process.argv.slice(2);
const f=+fS,db=+dbS,ratio=+ratioS,attack=+attackS,release=+releaseS,rate=+rateS,oldAttack=+oldS===1,enabled=+onS===1,loss=+lossS;
const engine=create176(manifest,{fs:rate,ratio,attack,release,oldAttack,enabled,loss,magnetics:false,detectorModel:model});
const amp=2*Math.SQRT2*.775*10**(db/20),duration=2.5,start=2;
let rows=['time,audio,cv,bplus,reg'],n=0,error=null;
for(;n<Math.round(duration*rate);n++){const t=n/rate;try{const r=engine.step(amp*Math.sin(2*Math.PI*f*t));if(t>=start)rows.push([t,r.audio,r.cv,r.bplus,r.reg].join(','));}catch(e){error=String(e);break;}}
fs.writeFileSync('ripple.csv',rows.join('\n')+'\n');
console.log(JSON.stringify({f,db,ratio,attack,release,rate,oldAttack,enabled,loss,model,n,error}));
if(error)process.exitCode=1;
