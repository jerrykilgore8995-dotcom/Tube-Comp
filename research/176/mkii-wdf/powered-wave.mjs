// Fixed-control offline 176 full-topology WDF prototype. See model differences in README.
import {adaptor,nonlinearStep,prepareNonlinear} from './wave-core.mjs';
import {magneticCore} from './magnetic-core.mjs';
function build176(adaptor,prepareNonlinear,manifest,opts={}){
 const fs=opts.fs??44100,dt=1/fs,ratio=opts.ratio??2,loss=opts.loss??20,attack=opts.attack??.5,release=opts.release??.8;
 const factors=Object.fromEntries(manifest.records.filter(x=>x.fraction!==undefined).map(x=>[x.name,1+x.fraction]));
 const fac=(name)=>factors[name]??1;
 const nodes=new Map(),ports=[],constraints=[],mutual=[],devices=[],magnetics=[];
 const node=n=>{if(n==="0")return -1;if(!nodes.has(n))nodes.set(n,nodes.size);return nodes.get(n)};
 const branch=(a,b)=>[[node(a),1],[node(b),-1]].filter(x=>x[0]>=0);
 const port=(name,a,b,z,type)=>{if(!(z>0))throw Error(name);ports.push({name,inc:branch(a,b),z,type});return ports.length-1};
 const R=(n,a,b,r,physical=true)=>port(n,a,b,r*(physical?fac(n):1),"R");
 const C=(n,a,b,c,physical=true)=>port(n,a,b,dt/(2*c*(physical?fac(n):1)),"C");
 const L=(n,a,b,l)=>port(n,a,b,2*l/dt,"L");
 const constraint=(name,inc,voltage)=>constraints.push({name,inc,voltage});
 const V=(n,a,b,v)=>constraint(n,branch(a,b),v);
 const T=(n,a,b,c,d,k)=>constraint(n,[...branch(c,d),...branch(a,b).map(([i,x])=>[i,-k*x])],()=>0);
 const N=(n,a,b,z)=>port(n,a,b,z,"N");
 const device=(indices,fn)=>devices.push({indices,fn});
 const sp=x=>Math.max(0,x)+Math.log1p(Math.exp(-Math.abs(x)));
 const pos=x=>Math.max(0,x);
 const diode=(n,a,b,is,ideal,rs,cj,tt)=>{
   let junction=b;
   if(rs>0){junction=n+":J";R(n+":Rs",a,junction,rs,false);a=junction;}
   const p=N(n,a,b,1000);
   // Initial reference uses the nominal junction capacitance; voltage-dependent
   // depletion and transit charge are not implemented yet.
   if(cj)C(n+":Cj",a,b,cj,false);
   device([p],v=>[is*Math.expm1(Math.min(50,v[0]/(.0258649*ideal)))]);
 };
 const triode=(n,p,g,k,type,scale)=>{
   const a=N(n+":P",p,k,10000),b=N(n+":G",g,k,1000);
   R(n+":leak",p,k,1e9,false);
   const pars=type==="6BC8"?[3.9,3.7,1.8]:type==="12AX7"?[2.3,2.4,.9]:[2.9,2.5,.5];
   C(n+":Cgk",g,k,pars[0]*1e-12,false);C(n+":Cgp",g,p,pars[1]*1e-12,false);C(n+":Cpk",p,k,pars[2]*1e-12,false);
   device([a,b],v=>{const [vp,vg]=v;let ip=0,ig=0;
    if(type==="6BC8")ip=20.19891536e-6*Math.pow(pos(vp*sp(1.794225622*(1+54.16463004*vg/Math.sqrt(922.2704089+vp*vp)))/1.794225622),1.439822572);
    if(type==="12AX7")ip=2*Math.pow(pos(vp/600*sp(600*(.01+vg/Math.sqrt(300+vp*vp)))),1.4)/1060;
    if(type==="12BH7"){ip=2*Math.pow(pos(vp/185.4*sp(185.4*(1/18.02+(vg-.6206)/Math.sqrt(1247.69+vp*vp)))),1.181)/945.75;ig=(8.2e-4+.162/Math.max(1,6.624+vp))*(2*18.02/945.75)*Math.pow(pos(vg+1.03),1.3);}
    return [scale*ip,ig];
   });
   if(type==="6BC8")diode(n+":grid",g,k,1e-9,1.5,2000,1e-12,1e-9);
   if(type==="12AX7"){R(n+":RG",g,n+":GD",2000,false);diode(n+":grid",n+":GD",k,1e-9,1,1,10e-12,1e-9);}
 };
 const addCore=(name,a,b,L,lambdaKnee,settings={})=>{
  if(opts.magnetics===false)return;
  const config={fs,L,lambdaKnee,...settings,...(opts.cores?.[name]??{})};
  const core=magneticCore(config),p=N(name+":MAG",a,b,Math.max(1,2*L/dt/1000));
  device([p],v=>[core.current(v[0])]);magnetics.push({name,p,core});
 };
 const transformer=(n,ip,inn,op,on,p)=>{
  const f=x=>n+":"+x;
  R(f("RP1"),ip,f("P1"),p.rp/2,false);R(f("RP2"),inn,f("PN"),p.rp/2,false);
  L(f("Lleak"),f("P1"),f("PP"),p.ll);L(f("Lmag"),f("PP"),f("PN"),p.lm);
  R(f("Rcore"),f("PP"),f("PN"),p.rc,false);
  // Catalog level converted to an initial 50 Hz flux anchor, not a measured saturation threshold.
  if(n==="T1")addCore(n,f("PP"),f("PN"),p.lm,Math.SQRT2*.775*10**(8/20)/(2*Math.PI*50),{saturation:.35});
  if(n==="T2")addCore(n,f("PP"),f("PN"),p.lm,Math.SQRT2*Math.sqrt(.001*10**(15/10)*15000)*2.3/(2*Math.PI*50),{saturation:.35});
  T(f("turns"),f("PP"),f("PN"),f("SPLUS"),f("SMINUS"),p.n);
  R(f("RS1"),f("SPLUS"),op,p.rs/2,false);R(f("RS2"),f("SMINUS"),on,p.rs/2,false);
  C(f("Csec"),op,on,p.cs,false);C(f("Cwin1"),f("PP"),op,p.cw,false);C(f("Cwin2"),f("PN"),on,p.cw,false);
 };
 const kpad=10**(loss/20),arm=Math.max(1e-6,300*(kpad-1)/(kpad+1));
 V("VDRIVE","DRIVEP","DRIVEN",(t,input)=>input);
 R("RCOMMON","DRIVEN","0",1e9,false);R("RSOURCEP","DRIVEP","INP",(opts.source??600)/2,false);R("RSOURCEN","DRIVEN","INN",(opts.source??600)/2,false);
 R("PAD:RINP","INP","PAD:MP",arm,false);R("PAD:RINN","INN","PAD:MN",arm,false);
 R("PAD:ROUTP","PAD:MP","PAD_P",arm,false);R("PAD:ROUTN","PAD:MN","TN",arm,false);R("PAD:RSHUNT","PAD:MP","PAD:MN",1200*kpad/Math.max(1e-9,kpad*kpad-1),false);
 R("R1","PAD_P","TP",Math.max(1e-6,300*(opts.vernier??0)));
 transformer("T1","TP","TN","SEC_P","SEC_N",{n:10,rp:52,rs:3900,ll:.002,lm:3.3,rc:1e5,cs:92e-12,cw:15e-12});
 R("R2","SEC_P","TAP_P",1e4);R("R3","TAP_P","CV",6800);R("R4","CV","TAP_N",6800);R("R5","TAP_N","SEC_N",1e4);
 const hi=opts.high??true;
 R("RG_HI_P","SEC_P","GP",hi?.001:1e12,false);R("RG_LO_P","TAP_P","GP",hi?1e12:.001,false);
 R("RG_HI_N","SEC_N","GN",hi?.001:1e12,false);R("RG_LO_N","TAP_N","GN",hi?1e12:.001,false);
 C("C5","CV","0",.1e-6);
 triode("V1A","PA","GP","KA","6BC8",1);triode("V1B","PB","GN","KB","6BC8",1);
 R("R6A","KA","KRETURN",50);R("R6B","KB","KRETURN",50);R("R7","KA","KRETURN",47);R("R8","KB","KRETURN",47);R("R9","KRETURN","0",330);
 R("R10A","KRETURN","METER",500);R("R10B","METER","R11TOP",500);R("R11","R11TOP","0",330);
 C("C10","REG","0",.1e-6);
 R("R12","PA","PLATE_BAL_A",4700);R("R13A","PLATE_BAL_A","REG",500);R("R13B","REG","PLATE_BAL_B",500);R("R14","PLATE_BAL_B","PB",4700);
 transformer("T2","PA","PB","NEXT_P","NEXT_N",{n:1/2.3,rp:1e4,rs:2900,ll:.1,lm:346,rc:4e5,cs:60e-12,cw:15e-12});
 V("VT4A","HV_A_OC","0",t=>460*Math.sin(2*Math.PI*60*t));V("VT4B","0","HV_B_OC",t=>460*Math.sin(2*Math.PI*60*t));
 R("RT4A","HV_A_OC","HV_A",100,false);R("RT4B","HV_B_OC","HV_B",100,false);
 for(const side of ["A","B"]){let p=N("V5"+side,"HV_"+side,"RECT",100);device([p],v=>[.00331*pos(v[0])**1.5]);}
 C("C11D","RECT","0",20e-6);L("L1","RECT","CHOKE_END",10);R("RL1","CHOKE_END","FILTER",150,false);
 for(const s of ["A","B","C"])C("C11"+s,"FILTER","0",20e-6);
 R("R44","FILTER","T3_CT",1e3);C("C8B","T3_CT","0",20e-6);R("R43","T3_CT","REG",1e4);
 let p=N("V6","REG","0",100);device([p],v=>[pos(v[0]-107.55)/33]);
 R("R15","NEXT_P","0",2e4);R("R17","NEXT_N","0",2e4);
 triode("V2A","V2PA","NEXT_P","V2KA","12AX7",1.025);triode("V2B","V2PB","NEXT_N","V2KB","12AX7",.975);
 R("R19","V2KA","V2_RETURN",2200);R("R20","V2KB","V2_RETURN",2200);R("R16","V2_RETURN","0",2200);
 R("R22","V3PA","FEEDBACK_A",opts.fbres??180e3);R("R18","FEEDBACK_A","0",180e3);C("C1","FEEDBACK_A","V2KA",.47e-6);
 R("R25","V3PB","FEEDBACK_B",opts.fbres??180e3);R("R21","FEEDBACK_B","0",180e3);C("C2","FEEDBACK_B","V2KB",.47e-6);
 R("R23","V2PA","V2_SUPPLY",220e3);R("R24","V2PB","V2_SUPPLY",220e3);C("C3","V2PA","V3GA",.1e-6);C("C4","V2PB","V3GB",.1e-6);
 R("R27","V3GA","0",1.2e6);R("R29","V3GB","0",1.2e6);
 triode("V3A","V3PA","V3GA","V3K","12BH7",1.025);triode("V3B","V3PB","V3GB","V3K","12BH7",.975);R("R28","V3K","0",330);
 C("C9B","T3_CT","0",10e-6);R("R26","T3_CT","V2_SUPPLY",47e3);C("C9A","V2_SUPPLY","0",10e-6);
 R("T3:RPA","V3PA","T3:PLEAKA",525,false);R("T3:RPB","V3PB","T3:PLEAKB",525,false);
 L("T3:LLA","T3:PLEAKA","T3:HALF_A",.05);L("T3:LLB","T3:PLEAKB","T3:HALF_B",.05);
 const lp=[62.5,62.5,250/81],li=[L("T3:LPA","T3:HALF_A","T3_CT",lp[0]),L("T3:LPB","T3_CT","T3:HALF_B",lp[1]),L("T3:LS","T3:OUT_RAW","T3_N",lp[2])];
 for(let i=0;i<3;i++)for(let j=i+1;j<3;j++)mutual.push([li[i],li[j],2*.99999*Math.sqrt(lp[i]*lp[j])/dt]);
 // Full primary flux is common to both half-windings; excess current is reflected by the coupled network.
 addCore("T3","T3:HALF_A","T3:HALF_B",250,Math.SQRT2*.775*10**(28/20)*9/(2*Math.PI*50),{saturation:.35});
 R("T3:RC","T3:HALF_A","T3:HALF_B",500e3,false);R("T3:RS","T3:OUT_RAW","T3_P",1,false);C("T3:CS","T3_P","T3_N",80e-12,false);
 const sel={2:[.12,1000,"R47"],4:[.29,6800,"R46"],8:[.58,20000,"R45"],12:[1,47000,"R39"]}[ratio];if(!sel)throw Error("invalid ratio");
 T("T3:TAPA","V3PA","T3_CT","TAP_A","T3_CT",opts.tap??sel[0]);T("T3:TAPB","V3PB","T3_CT","TAP_B","T3_CT",opts.tap??sel[0]);
 R("R30","T3_P","OUT_P",Math.max(.001,300*(opts.outvernier??0)));const output=R("RLOAD","OUT_P","T3_N",opts.load??600,false);
 V("VOUT_REF","T3_N","0",()=>0);
 C("C6","TAP_A","DET_K1",.5e-6);C("C7","TAP_B","DET_K2",.5e-6);
 R("R36","DET_K1","DET_BIAS",220e3);R("R37","DET_BIAS","DET_K2",220e3);R("R38","REG","DET_BIAS",82e3);
 R("R_S5","DET_BIAS","0",(opts.bias??sel[1])*fac(sel[2]),false);C("C8A","DET_BIAS","0",20e-6);
 // Invert the original 6AL5 series voltage law including the 12.7G shunt.
 const detector=(n,a,k)=>{
  const p=N(n,a,k,1e4);C(n+":CAK",a,k,3.2e-12,false);
  device([p],v=>{
   const target=v[0],vt=.0258649*3.15;
   function currentAt(u){return 66e-12*Math.expm1(Math.min(50,(u+.867)/vt))+u/12.7e9}
   function drop(i){return (pos(i/.0032)+1e-9)**.775-(1e-9)**.775}
   let lo=Math.min(target,-1)-1,hi=target;
   for(let j=0;j<44;j++){let mid=(lo+hi)/2;if(mid+drop(currentAt(mid))>target)hi=mid;else lo=mid;}
   return [currentAt((lo+hi)/2)];
  });
 };
 detector("V4A","DET_A","DET_K1");detector("V4B","DET_A","DET_K2");
 const enabled=opts.enabled??true;
 R("R_S4_ON","CV","ATK_IN",enabled?.001:1e12,false);R("R_S4_OFF","CV","0",enabled?1e12:.001,false);
 // C-10476: R32 shunts only CCW-to-wiper section of R33.
 if(opts.oldAttack){R("R32","ATK_IN","DET_A",1e4);R("R33","ATK_IN","DET_A",Math.max(.001,25000*(1-attack)));}
 else {R("R32","ATK_IN","ATK_WIPER",1e4);R("R33","ATK_IN","ATK_WIPER",Math.max(.001,25000*attack));R("R33","ATK_WIPER","DET_A",Math.max(.001,25000*(1-attack)));}
 R("R34A","DET_A","REL_WIPER",Math.max(.001,5e6*(1-release)));R("R34B","REL_WIPER","0",Math.max(.001,5e6*release));R("R35","REL_WIPER","0",270e3);
 // Observe differential winding voltages via existing passive port voltages.
 const obsNames=["T1:Lmag","T1:Csec","T2:Lmag","T2:Csec","T3:RC","T3:CS"];
 const obs=obsNames.map(name=>{const index=ports.findIndex(p=>p.name===name);if(index<0)throw Error(name);return index;});
 const cv=ports.findIndex(p=>p.name==="C5"),rect=ports.findIndex(p=>p.name==="C11D"),reg=ports.findIndex(p=>p.name==="C10"),bplus=ports.findIndex(p=>p.name==="C8B");
 const mat=(n,m)=>Array.from({length:n},()=>Array(m).fill(0));
 const A=mat(nodes.size,ports.length),Z=mat(ports.length,ports.length),Q=mat(nodes.size,constraints.length);
 for(let j=0;j<ports.length;j++){Z[j][j]=ports[j].z;for(const [i,x] of ports[j].inc)A[i][j]+=x;}
 for(const [i,j,z] of mutual)Z[i][j]=Z[j][i]=z;
 for(let j=0;j<constraints.length;j++)for(const [i,x] of constraints[j].inc)Q[i][j]+=x;
 const ad=adaptor(A,Z,Q);
 const indices=ports.flatMap((p,i)=>p.type==="N"?[i]:[]);
 const indexMap=new Map(indices.map((x,i)=>[x,i]));
 const current=v=>{const i=Array(v.length).fill(0);for(const d of devices){const ix=d.indices.map(x=>indexMap.get(x)),out=d.fn(ix.map(x=>v[x]));ix.forEach((p,j)=>i[p]+=out[j]);}return i};
 const solveStep=prepareNonlinear(ad,indices,current,opts.tol??1e-6);
 let b=Array(ports.length).fill(0),guess=Array(indices.length).fill(0),n=0;
 function step(input){
  const t=n/fs,e=constraints.map(x=>x.voltage(t,input));
  const r=solveStep(b,e,guess);
  guess=r.v;n++;
  for(const m of magnetics)m.core.commit((r.a[m.p]+r.b[m.p])/2);
  const voltage=i=>(r.a[i]+r.b[i])/2;
  for(let j=0;j<ports.length;j++){if(ports[j].type==="C")b[j]=r.a[j];else if(ports[j].type==="L")b[j]=-r.a[j];}
  return {audio:voltage(output),cv:voltage(cv),rect:voltage(rect),reg:voltage(reg),bplus:voltage(bplus),iterations:r.iterations,...Object.fromEntries(obsNames.map((name,i)=>[name,voltage(obs[i])])),...Object.fromEntries(magnetics.map(m=>[m.name+":Iexcess",(r.a[m.p]-r.b[m.p])/(2*ports[m.p].z)]))};
 }
 return {step,magneticDiagnostics:()=>Object.fromEntries(magnetics.map(m=>[m.name,m.core.diagnostics()])),inventory:{nodes:nodes.size,ports:ports.length,nonlinear:indices.length,idealConstraints:constraints.length,couplings:mutual.length},ports:ports.map(p=>p.name)};
}
export function create176(manifest,opts){return build176(adaptor,prepareNonlinear,manifest,opts);}
