// Provisional datasheet-anchor fits, not full manufacturer curve digitization.
const sp=x=>Math.max(0,x)+Math.log1p(Math.exp(-Math.abs(x)));
const pos=x=>Math.max(0,x);
export function plateCurrent(type,p,g,variant='candidate'){
 if(variant==='legacy'){
 if(type==='6BC8')return 20.19891536e-6*pos(p*sp(1.794225622*(1+54.16463004*g/Math.sqrt(922.2704089+p*p)))/1.794225622)**1.439822572;
 if(type==='12AX7')return 2*pos(p/600*sp(600*(.01+g/Math.sqrt(300+p*p))))**1.4/1060;
 if(type==='12BH7')return 2*pos(p/185.4*sp(185.4*(1/18.02+(g-.6206)/Math.sqrt(1247.69+p*p))))**1.181/945.75;
 }
 if(type==='6BC8'){
 // Existing repository V6BC8_REMOTE_EXP candidate; monotone deep-cutoff tail.
 return 19.65702722e-6*pos(p*sp(1.794225622*(1+61.17656889*g/Math.sqrt(922.2704089+p*p)))/1.794225622)**1.439822572+.35980862e-3*pos(p/150)**1.69329672*Math.exp(Math.min(30,(g+13)/7.23198212));
 }
 if(type==='12AX7')return 0.0016974781511472805*pos(p/285*sp(285*(1/110+g/Math.sqrt(300+p*p))))**1.25;
 if(type==='12BH7')return 0.002181850801915106*pos(p/185*sp(185*(1/16.600000000000005+(g-.6206)/Math.sqrt(1247.69+p*p))))**1.1749999999999998;
 throw Error('unknown tube '+type);
}
export function tubeMetrics(type,p,g,variant){
 const f=(a,b)=>plateCurrent(type,a,b,variant);
 const gm=(f(p,g+.001)-f(p,g-.001))/.002,gp=(f(p+.01,g)-f(p-.01,g))/.02;
 return {p,g,ip_mA:1000*f(p,g),gm_mS:1000*gm,rp_kOhm:1/gp/1000,mu:gm/gp};
}
