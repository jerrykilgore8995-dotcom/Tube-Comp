// Flux-linkage model: electrical units Wb-turn, A, H. No output waveshaping.
// Parallel excess magnetizing current; the existing linear winding model remains.
export function magneticCore({fs,L,lambdaKnee,saturation=.35,play=[.015,.06,.2],weights=[.012,.018,.025],eddy=.0}){
 if(!(fs>0&&L>0&&lambdaKnee>0)||play.length!==weights.length||weights.some(x=>x<0)||play.some(x=>x<=0)||saturation<0||eddy<0)throw Error('invalid magnetic parameters');
 const dt=1/fs;let flux=0,vprev=0,memory=play.map(()=>0),energy=0,peak=0;
 const clip=(x,a,b)=>Math.min(b,Math.max(a,x));
 function trial(v){
  const lambda=flux+dt*.5*(v+vprev),x=lambda/lambdaKnee;
  // Continuous monotone odd excess current with growing slope, bounded
  // high-field incremental inductance. No arbitrary voltage clipping.
  const ax=Math.abs(x),sat=ax<=3?x**5:Math.sign(x)*(243+405*(ax-3));
  const next=play.map((r,j)=>clip(memory[j],x-r,x+r));
  const h=weights.reduce((sum,k,j)=>sum+k*(x-next[j]),0);
  const i=lambdaKnee/L*(saturation*sat+h)+eddy*v;
  return {lambda,x,next,i};
 }
 function commit(v){const q=trial(v);
  // Dissipated energy of the stop elements is nonnegative.
  for(let j=0;j<play.length;j++)energy+=lambdaKnee**2/L*weights[j]*play[j]*Math.abs(q.next[j]-memory[j]);
  energy+=eddy*dt*v*v;flux=q.lambda;vprev=v;memory=q.next;peak=Math.max(peak,Math.abs(q.x));return q.i;
 }
 return {current:v=>trial(v).i,commit,diagnostics:()=>({fluxLinkage:flux,normalizedPeakFlux:peak,hysteresisAndEddyJ:energy})};
}
export function magneticSelfTest(){
 const fs=176400,f=50,L=3.3,lambdaKnee=.0088;
 const core=magneticCore({fs,L,lambdaKnee});let p=0,odd=0,peak=0;
 const n=Math.round(fs/f);
 for(let k=0;k<5*n;k++){const v=2*Math.PI*f*lambdaKnee*Math.cos(2*Math.PI*f*k/fs);const i=core.current(v);if(!Number.isFinite(i))throw Error('nonfinite core');if(core.current(v)!==i)throw Error('trial mutated state');core.commit(v);if(k>=4*n){p+=i*Math.sin(2*Math.PI*f*k/fs);odd+=i*Math.sin(6*Math.PI*f*k/fs);peak=Math.max(peak,Math.abs(i));}}
 const d=core.diagnostics();if(!(d.hysteresisAndEddyJ>0&&Math.abs(odd)>0&&peak>0))throw Error('magnetic response missing');return {...d,thirdCurrentRelativeDb:20*Math.log10(Math.abs(odd/p))};
}
