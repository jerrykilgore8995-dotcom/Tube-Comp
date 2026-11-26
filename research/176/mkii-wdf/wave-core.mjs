// Offline reference WDF adaptor and nonlinear multiport root. Not a VST3 engine.
// Uses voltage waves a=v+Zi, b=v-Zi; topology factorized only when adaptor is constructed.
function solve(A,b){
 const n=b.length,M=A.map((r,i)=>[...r,b[i]]);
 for(let k=0;k<n;k++){
  let p=k;for(let i=k+1;i<n;i++)if(Math.abs(M[i][k])>Math.abs(M[p][k]))p=i;
  if(!Number.isFinite(M[p][k])||Math.abs(M[p][k])<1e-20)throw Error("singular wave adaptor");
  [M[p],M[k]]=[M[k],M[p]];
  for(let i=k+1;i<n;i++){let f=M[i][k]/M[k][k];for(let j=k;j<=n;j++)M[i][j]-=f*M[k][j];}
 }
 const x=Array(n).fill(0);
 for(let i=n-1;i>=0;i--){let q=M[i][n];for(let j=i+1;j<n;j++)q-=M[i][j]*x[j];x[i]=q/M[i][i];}
 return x;
}

function dot(a,b){return a.reduce((s,v,i)=>s+v*b[i],0);}

function mv(A,v){return A.map(r=>dot(r,v));}

function tr(A){return A[0].map((_,i)=>A.map(r=>r[i]));}

function mm(A,B){let T=tr(B);return A.map(r=>T.map(c=>dot(r,c)));}

function zeros(n,m){return Array.from({length:n},()=>Array(m).fill(0));}

function adaptor(A,Z,C){
 // A: node-by-port incidence; Z: symmetric positive port impedance matrix.
 // C: node-by-constraint matrix, for ideal sources and ideal transformer relations.
 // b travels from elements to junction; a travels from junction to elements.
 const n=A.length,p=A[0].length,q=C[0].length;
 const W=tr(Array.from({length:p},(_,i)=>solve(Z,Array.from({length:p},(_,j)=>+(i===j)))));
 const AW=mm(A,W),H=mm(AW,tr(A)),K=zeros(n+q,n+q);
 for(let i=0;i<n;i++){for(let j=0;j<n;j++)K[i][j]=H[i][j];for(let j=0;j<q;j++){K[i][n+j]=C[i][j];K[n+j][i]=C[i][j];}}
 const columns=Array.from({length:p+q},(_,j)=>{
  const rhs=Array.from({length:n+q},(_,i)=>i<n?(j<p?AW[i][j]:0):+(j===p+i-n));
  const v=solve(K,rhs).slice(0,n);
  return mv(tr(A),v).map((x,i)=>2*x-+(j<p&&j===i));
 });
 const G=tr(columns),S=G.map(r=>r.slice(0,p)),T=G.map(r=>r.slice(p));
 return {S,T,Z,waves:(b,e)=>mv(S,b).map((x,i)=>x+dot(T[i],e))};
}

function nonlinearStep(ad,b,e,indices,current,guess,tol=1e-9){
 // Solve only the multiport nonlinear device voltages, not the circuit node voltages.
 // Nonlinear port impedance blocks must not couple into known linear-port states.
 const {S,T,Z}=ad,N=indices.length;
 const z=indices.map(i=>indices.map(j=>Z[i][j]));
 const norm=x=>Math.max(...x.map(Math.abs));
 function evaluate(v){
  const i=current(v),zi=mv(z,i),bb=b.slice();
  indices.forEach((p,k)=>bb[p]=v[k]-zi[k]);
  const a=ad.waves(bb,e);
  return {a,b:bb,f:v.map((x,k)=>x+zi[k]-a[indices[k]])};
 }
 let v=guess.slice(),r=evaluate(v);
 for(let it=0;it<40;it++){
  if(norm(r.f)<tol)return {...r,v,iterations:it};
  const J=zeros(N,N);
  for(let k=0;k<N;k++){
   const h=1e-6*Math.max(1,Math.abs(v[k])),u=v.slice();u[k]+=h;
   const f=evaluate(u).f;for(let j=0;j<N;j++)J[j][k]=(f[j]-r.f[j])/h;
  }
  const d=solve(J,r.f.map(x=>-x));
  let accepted=false;
  for(let s=1;s>=1/4096;s*=.5){
   const u=v.map((x,k)=>x+s*d[k]),t=evaluate(u);
   if(norm(t.f)<norm(r.f)){v=u;r=t;accepted=true;break;}
  }
  if(!accepted)throw Error("nonlinear wave line search failed");
 }
 throw Error("nonlinear wave convergence failed");
}

function selfTest(){
 const close=(a,b,t=1e-8)=>{if(Math.abs(a-b)>t)throw Error(`${a} != ${b}`)};
 // RC driven by a constrained ideal source: port 0 resistor, port 1 capacitor.
 const dt=1/44100,R=1000,C=1e-6,Zc=dt/(2*C);
 let ad=adaptor([[1,0],[-1,1]],[[R,0],[0,Zc]],[[1],[0]]);
 let b=[0,0],previousV=0,previousIn=0;
 for(let n=0;n<100;n++){
  const a=ad.waves(b,[1]),v=(a[1]+b[1])/2;
  const expected=((2*R*C-dt)*previousV+dt*(1+previousIn))/(2*R*C+dt);
  close(v,expected);previousV=v;previousIn=1;b=[0,a[1]];
 }
 // Ideal 2:1 voltage step-up with a 600-ohm secondary load reflects 150 ohms.
 ad=adaptor([[1,0],[-1,0],[0,1]],[[100,0],[0,600]],[[1,0],[0,-2],[0,1]]);
 let a=ad.waves([0,0],[1,0]);close(a[1]/2,1.2);
 // Shockley diode fed through 1k; compare nonlinear wave root with KCL.
 ad=adaptor([[1,0],[-1,1]],[[1000,0],[0,100]],[[1],[0]]);
 let step=nonlinearStep(ad,[0,0],[1],[1],v=>[1e-12*Math.expm1(v[0]/.026)],[.5]);
 close((1-step.v[0])/1000,1e-12*Math.expm1(step.v[0]/.026),1e-10);
 // Coupled inductance block (positive definite) verifies cross-port response.
 ad=adaptor([[1,0],[0,1]],[[2,.5],[.5,3]],[[1,0],[0,1]]);
 a=ad.waves([.4,-.2],[1,2]);close(a[0],1.6);close(a[1],4.2);
 return {rcSamples:100,transformerSecondary:1.2,diodeVoltage:step.v[0],diodeIterations:step.iterations,coupledPortConstraintTest:"pass"};
}

function prepareNonlinear(ad,indices,current,tol=1e-6){
 const n=indices.length,S=indices.map(i=>indices.map(j=>ad.S[i][j])),Z=indices.map(i=>ad.Z[i][i]);
 const set=new Set(indices),linear=ad.S[0].map((_,i)=>i).filter(i=>!set.has(i));
 function evaluate(v,rhs){
  const i=current(v),b=v.map((x,j)=>x-Z[j]*i[j]);
  return {b,f:v.map((x,j)=>{let y=x+Z[j]*i[j]-rhs[j];for(let k=0;k<n;k++)y-=S[j][k]*b[k];return y})};
 }
 const norm=x=>Math.max(...x.map(Math.abs));
 return function step(b,e,guess){
  const rhs=indices.map(i=>{let x=dot(ad.T[i],e);for(const j of linear)x+=ad.S[i][j]*b[j];return x;});
  let v=guess.slice(),r=evaluate(v,rhs);
  for(let it=0;it<40;it++){
   if(norm(r.f)<tol){const bb=b.slice();indices.forEach((p,k)=>bb[p]=r.b[k]);return {v,b:bb,a:ad.waves(bb,e),iterations:it};}
   const J=zeros(n,n);
   for(let k=0;k<n;k++){
    const h=1e-6*Math.max(1,Math.abs(v[k])),u=v.slice();u[k]+=h;
    const f=evaluate(u,rhs).f;for(let j=0;j<n;j++)J[j][k]=(f[j]-r.f[j])/h;
   }
   const d=solve(J,r.f.map(x=>-x));let accepted=false;
   for(let s=1;s>=1/4096;s*=.5){const u=v.map((x,k)=>x+s*d[k]),t=evaluate(u,rhs);if(norm(t.f)<norm(r.f)){v=u;r=t;accepted=true;break;}}
   if(!accepted)throw Error("reduced nonlinear wave line search failed; residual "+norm(r.f));
  }
  throw Error("reduced nonlinear wave convergence failed; residual "+norm(r.f));
 };
}
export {adaptor,nonlinearStep,selfTest,prepareNonlinear};
