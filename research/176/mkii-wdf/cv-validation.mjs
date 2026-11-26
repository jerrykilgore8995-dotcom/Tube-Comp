import {create176} from './powered-wave.mjs';
import fs from 'node:fs';
const m=JSON.parse(fs.readFileSync(new URL('./mismatch-manifest.json',import.meta.url)));
const ratio=Number(process.argv[2]), mode=process.argv[3], rate=Number(process.env.FS||44100), f=1000, period=Math.round(rate/f), omega=2*Math.PI*f/rate;
const config={fs:rate,ratio,attack:mode==='slow'?0:1,release:mode==='slow'?0:1,detectorModel:'legacy',cvRippleHz:2*f,loss:20,magnetics:true};
const amp=db=>2*Math.SQRT2*.775*10**(db/20),dbu=x=>20*Math.log10(Math.max(1e-20,x)/Math.SQRT2/.775);
function block(e,db,seconds){let x=0,y=0,n=0;const count=Math.round(seconds*rate/period)*period;for(let i=0;i<count;i++){let ph=omega*i,r=e.step(amp(db)*Math.sin(ph));if(i>=count-10*period){x+=r.audio*Math.cos(ph);y+=r.audio*Math.sin(ph);n++;}}return dbu(2*Math.hypot(x,y)/n)}
const off=create176(m,{...config,enabled:false}),on=create176(m,{...config,enabled:true});
block(off,-30,1);block(on,-30,1);
if(mode==='ratio'){
 for(let db=-24;db<=12;db+=2){try{let a=block(off,db,.3),b=block(on,db,.3);console.log(JSON.stringify({ratio,db,off:a,on:b,gr:a-b}));}catch(e){console.log(JSON.stringify({ratio,db,error:String(e)}));break;}}
}else{
 let low=-24,hi=8,best;
 for(let j=0;j<9;j++){let db=(low+hi)/2,a=block(off,db,.5),b=block(on,db,mode==='slow'?2:.5),gr=a-b;best={db,a,b,gr};console.log(JSON.stringify({calibration:best,mode}));if(gr<10)low=db;else hi=db;}
 const db=best.db,base=best.a;block(on,-30,mode==='slow'?4:1);block(off,-30,1);
 let traces=[];for(const phase of ['attack','release']){let duration=phase==='attack'?(mode==='slow'?2:.3):(mode==='slow'?8:1);let blocks=Math.ceil(duration*rate/period);for(let k=0;k<blocks;k++){let x=0,y=0,u=0,v=0;for(let i=0;i<period;i++){let ph=omega*(k*period+i),inDb=phase==='attack'?db:db-24,signal=amp(inDb)*Math.sin(ph),r=on.step(signal),q=off.step(signal);x+=r.audio*Math.cos(ph);y+=r.audio*Math.sin(ph);u+=q.audio*Math.cos(ph);v+=q.audio*Math.sin(ph);}traces.push({phase,ms:1000*(k+1)*period/rate,gr:dbu(2*Math.hypot(u,v)/period)-dbu(2*Math.hypot(x,y)/period)});}}
 const at=traces.filter(x=>x.phase==='attack'),rel=traces.filter(x=>x.phase==='release'),target=at.at(-1).gr;console.log(JSON.stringify({mode,actualGR:target,attack63:at.find(x=>x.gr>=target*.632)?.ms,attack90:at.find(x=>x.gr>=target*.9)?.ms,release63:rel.find(x=>x.gr<=target*.368)?.ms,release90:rel.find(x=>x.gr<=target*.1)?.ms}));fs.writeFileSync('timing-'+mode+'.json',JSON.stringify(traces));
}
