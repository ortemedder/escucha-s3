// web.h - pagina web del escucha: la S3 solo sirve esta pagina fija; el navegador lee /faccion.json y /estado.json
#pragma once

const char PAGINA[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="es"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Escucha CDSN</title>
<style>
:root{--f:#0d0904;--p:#1a1208;--l:#3a2a12;--n:#ff8c00;--nm:#b06a10;--v:#5adc78;--a:#ffd228;--r:#ff4632;--az:#5aaaff;--ro:#ff6ec8;--b:#f2e6d6}
*{box-sizing:border-box}body{margin:0;background:var(--f);color:var(--b);font:14px/1.35 system-ui,Segoe UI,Roboto,sans-serif}
header{padding:12px 16px;border-bottom:1px solid var(--l);display:flex;flex-wrap:wrap;gap:8px 20px;align-items:baseline}
h1{margin:0;font-size:20px;color:var(--az);letter-spacing:.5px}#est{color:var(--ro);font-size:13px}
.cards{display:flex;flex-wrap:wrap;gap:10px;padding:12px 16px}
.card{background:var(--p);border:1px solid var(--l);border-radius:8px;padding:8px 14px;min-width:120px}
.card b{display:block;font-size:22px}.card span{color:var(--v);font-size:12px;text-transform:uppercase}
.ctl{display:flex;flex-wrap:wrap;gap:12px;padding:0 16px 10px;align-items:center}
input[type=search]{background:var(--p);border:1px solid var(--l);color:var(--b);padding:6px 10px;border-radius:6px;min-width:200px}
label{color:var(--nm);cursor:pointer}
.tw{overflow-x:auto;padding:0 8px 20px}table{border-collapse:collapse;width:100%;min-width:1100px}
th{position:sticky;top:0;background:var(--p);color:var(--v);text-align:left;font-weight:600;font-size:12px;text-transform:uppercase;padding:8px 6px;cursor:pointer;white-space:nowrap;border-bottom:1px solid var(--l)}
th.o::after{content:" \25BE"}th.o.as::after{content:" \25B4"}
td{padding:6px;border-bottom:1px solid #22180a;vertical-align:top}tr.s{cursor:pointer}tr.s:hover{background:#1f160a}
.num{text-align:right;white-space:nowrap}.ctrl{color:var(--n)}.sub{color:var(--nm);font-size:12px}
.g2{border-left:3px solid var(--r)}.g1{border-left:3px solid var(--a)}.g0{border-left:3px solid transparent}
.tag{display:inline-block;padding:1px 6px;border-radius:4px;font-size:11px;font-weight:700;margin:1px 2px 1px 0}
.t2{background:var(--r);color:#000}.t1{background:var(--a);color:#000}
.up{color:var(--v)}.dn{color:var(--r)}.pk{color:var(--ro)}.az{color:var(--az)}
.det td{background:#140e06;padding:10px 14px}.fac{display:grid;grid-template-columns:minmax(150px,260px) 1fr 60px;gap:3px 10px;align-items:center;max-width:720px}
.bar{height:10px;background:var(--l);border-radius:3px;overflow:hidden}.bar i{display:block;height:100%;background:var(--nm)}.bar i.m{background:var(--n)}
.dg{display:flex;flex-wrap:wrap;gap:20px}.dg>div{min-width:260px}h3{margin:4px 0 6px;color:var(--v);font-size:12px;text-transform:uppercase}
svg{background:#0f0a04;border:1px solid var(--l);border-radius:6px}footer{color:var(--l);padding:10px 16px;font-size:12px}
</style></head><body>
<header><h1>ESCUCHA CDSN</h1><span id="est">cargando...</span></header>
<div class="cards" id="cards"></div>
<div class="ctl"><input type="search" id="q" placeholder="Buscar sistema..."><label><input type="checkbox" id="fc"> solo controlados</label>
<label><input type="checkbox" id="fa"> solo alertas</label><span class="sub" id="cnt"></span></div>
<div class="tw"><table><thead><tr id="cab"></tr></thead><tbody id="tb"></tbody></table></div>
<div id="zona"></div>
<footer id="pie"></footer>
<script>
const FAC="Caballeros de Sion";
const ES={Expansion:"Expansión",Retreat:"Retirada",War:"Guerra",CivilWar:"Guerra civil",Election:"Elecciones",Boom:"Auge",Bust:"Crisis",
CivilUnrest:"Disturbios",CivilLiberty:"Libertad civil",Famine:"Hambruna",Outbreak:"Epidemia",Lockdown:"Confinamiento",PirateAttack:"Ataque pirata",
InfrastructureFailure:"Fallo infraestr.",NaturalDisaster:"Desastre",PublicHoliday:"Festivo",Investment:"Inversión",Drought:"Sequía",Blight:"Plaga",
TerroristAttack:"Atentado",None:"—"};
const SEG={"$SYSTEM_SECURITY_high;":"Alta","$SYSTEM_SECURITY_medium;":"Media","$SYSTEM_SECURITY_low;":"Baja","$GAlAXY_MAP_INFO_state_anarchy;":"Anarquía","$GALAXY_MAP_INFO_state_lawless;":"Sin ley"};
const limpia=x=>x==null?"—":String(x).replace(/^\$(economy|government|SYSTEM_SECURITY|GALAXY_MAP_INFO_state)_/i,"").replace(/;$/,"").replace(/_/g," ");
const pob=n=>n==null?"—":n>=1e9?(n/1e9).toFixed(1).replace(".",",")+" mil mill.":n>=1e6?(n/1e6).toFixed(1).replace(".",",")+" mill.":n.toLocaleString("es-ES");
const pct=v=>v==null?"—":(v*100).toFixed(1).replace(".",",")+" %";
const ZC={Low:"baja",Med:"media",High:"alta"};
const TIPO={war:"Guerra",civilwar:"Guerra civil",election:"Elecciones"};
let D=null,E=null,orden="inf",asc=false,abiertos=new Set();
const es=x=>ES[x]||x, c1=v=>v==null||isNaN(v)?"—":v.toFixed(1).replace(".",","), now=()=>Date.now()/1000;
const esc=s=>String(s??"").replace(/[&<>"]/g,c=>({"&":"&amp;","<":"&lt;",">":"&gt;",'"':"&quot;"}[c]));
function hace(t){if(!t)return"—";let m=Math.round((now()-t)/60);if(m<1)return"ahora";if(m<120)return m+" min";if(m<2880)return Math.round(m/60)+" h";return Math.round(m/1440)+" d"}
function hora(t){return t?new Date(t*1000).toLocaleTimeString("es-ES",{hour:"2-digit",minute:"2-digit"}):"—"}
function hoyUTC(){return new Date().toISOString().slice(0,10)}
function proxTick(s){let p=s.tick_prox;if(!p)return 0;while(p<now()-1800)p+=86400;return p}
// alertas: mismas reglas que el radar
function alerta(s){
  for(const c of s.conflictos||[]){const t=TIPO[(c.tipo||"").toLowerCase()]||"Conflicto";
    if(c.estado=="active")return[2,t.toUpperCase(),`contra ${c.contra} (${c.ganados||0}-${c.perdidos||0})`];
    if(c.estado=="pending")return[2,t.toUpperCase()+" PEND.",`contra ${c.contra}`];}
  if((s.activos||[]).includes("Retreat")||(s.pendientes||[]).includes("Retreat"))return[2,"RETIRADA",`influencia ${c1(s.inf)} %`];
  const vent=s.inf-(s.segunda_inf||0);
  if(s.ctrl&&vent<5)return[2,"VENTAJA BAJA",`solo ${c1(vent)} sobre ${s.segunda}`];
  if(s.prev!=null&&s.prev-s.inf>=3)return[1,"BAJADA",`${c1(s.inf-s.prev)} desde ayer`];
  if((s.pendientes||[]).includes("Expansion"))return[1,"EXPANSIÓN PEND.",""];
  return[0,"",""];
}
const COLS=[["sys","Sistema"],["inf","Infl."],["d","Ayer"],["vent","Ventaja"],["est","Estados"],["conf","Conflicto / alerta"],["traf","Visitas hoy"],["pob","Población"],["pp","Powerplay"],["t","Dato"],["tick","Próx. tick"]];
function filas(){
  const hoy=hoyUTC();
  return Object.values(D.sistemas||{}).filter(s=>!s.fuera&&s.inf!=null).map(s=>{
    const a=alerta(s);
    return{s,a,sys:s.sys,inf:s.inf,d:s.prev!=null?s.inf-s.prev:null,vent:s.inf-(s.segunda_inf||0),
      est:[...(s.activos||[]).map(es),...(s.pendientes||[]).map(x=>es(x)+" (pend.)")].join(", "),
      conf:a[0]?a[1]:"",pob:(s.sis||{}).pob||0,pp:(s.pp||{}).pot||"",traf:(s.trafico||{})[hoy]||0,t:s.t||0,tick:proxTick(s),niv:a[0]};
  });
}
function pinta(){
  if(!D)return;
  let F=filas();const q=document.getElementById("q").value.toLowerCase();
  const tot=F.length,ctrl=F.filter(f=>f.s.ctrl).length,al=F.filter(f=>f.niv).length,traf=F.reduce((a,f)=>a+f.traf,0);
  let tg=0;for(const f of F)if(f.tick&&f.s.tick_disp!=null&&f.s.tick_disp<=180&&(!tg||f.tick<tg))tg=f.tick;
  if(!tg&&D.tick_global){tg=D.tick_global+86400;while(tg<now()-1800)tg+=86400}
  document.getElementById("cards").innerHTML=[["Sistemas",tot],["Controlados",ctrl],["Alertas",al],["Tráfico hoy",traf],["Próx. tick",tg?"~"+hora(tg):"aprendiendo"]]
    .map(([a,b])=>`<div class="card"><span>${a}</span><b>${b}</b></div>`).join("");
  if(q)F=F.filter(f=>f.sys.toLowerCase().includes(q));
  if(document.getElementById("fc").checked)F=F.filter(f=>f.s.ctrl);
  if(document.getElementById("fa").checked)F=F.filter(f=>f.niv);
  F.sort((x,y)=>{let a=x[orden],b=y[orden];if(orden=="conf"){a=x.niv;b=y.niv}
    if(a==null)a=-1e9;if(b==null)b=-1e9;const r=typeof a=="string"?a.localeCompare(b):a-b;return asc?r:-r});
  document.getElementById("cnt").textContent=`${F.length} de ${tot}`;
  document.getElementById("cab").innerHTML=COLS.map(([k,n])=>`<th data-k="${k}" class="${k==orden?"o"+(asc?" as":""):""}">${n}</th>`).join("");
  let h="";
  for(const f of F){const s=f.s;
    h+=`<tr class="s g${f.niv}" data-s="${esc(s.sys)}"><td><span class="${s.ctrl?"ctrl":""}">${s.ctrl?"■ ":"· "}${esc(s.sys)}</span></td>
    <td class="num">${c1(s.inf)} %</td><td class="num ${f.d>0?"up":f.d<0?"dn":""}">${f.d==null?"—":(f.d>0?"+":"")+c1(f.d)}</td>
    <td class="num ${f.vent<5?"dn":f.vent<15?"":"up"}">${c1(f.vent)}</td><td class="sub">${esc(f.est)||"—"}</td>
    <td>${f.niv?`<span class="tag t${f.niv}">${esc(f.a[1])}</span> <span class="sub">${esc(f.a[2])}</span>`:""}</td>
    <td class="num az">${f.traf||"—"}</td><td class="num sub">${pob(f.pob||null)}</td><td class="sub">${esc(f.pp)||"—"}</td><td class="num sub">${hace(f.t)}</td><td class="num pk">${f.tick?"~"+hora(f.tick):"—"}</td></tr>`;
    if(abiertos.has(s.sys))h+=`<tr class="det"><td colspan="11">${detalle(s)}</td></tr>`;}
  document.getElementById("tb").innerHTML=h;
  document.getElementById("zona").innerHTML=zona();
}
function grafica(h){
  if(!h||h.length<2)return'<span class="sub">histórico: aprendiendo...</span>';
  const W=320,H=90,t0=h[0][0],t1=Math.max(h[h.length-1][0]+3600,now());
  let mn=Math.min(...h.map(p=>p[1])),mx=Math.max(...h.map(p=>p[1]));if(mx-mn<2){const m=(mx+mn)/2;mn=m-1;mx=m+1}mn-=.5;mx+=.5;
  const X=t=>30+(t-t0)/(t1-t0)*(W-36),Y=v=>6+(1-(v-mn)/(mx-mn))*(H-20);
  let d="";h.forEach((p,i)=>{const x=X(p[0]),y=Y(p[1]),xn=i+1<h.length?X(h[i+1][0]):W-6;d+=(i?"L":"M")+x+","+y+"L"+xn+","+y});
  const f=t=>new Date(t*1000).toLocaleDateString("es-ES",{day:"2-digit",month:"2-digit"});
  return`<svg width="${W}" height="${H}"><path d="${d}" fill="none" stroke="#ff8c00" stroke-width="2"/>
  <text x="2" y="12" fill="#b06a10" font-size="10">${c1(mx)}</text><text x="2" y="${H-14}" fill="#b06a10" font-size="10">${c1(mn)}</text>
  <text x="30" y="${H-2}" fill="#b06a10" font-size="10">${f(t0)}</text><text x="${W-40}" y="${H-2}" fill="#b06a10" font-size="10">${f(t1)}</text></svg>`;
}
function detalle(s){
  const fac=(s.todas||[]).map(x=>{const m=x.n==FAC;
    return`<span class="${m?"ctrl":""}">${esc(x.n)}${x.jug?" <span class='sub'>(jugadores)</span>":""}<br><span class="sub">${esc(x.gov)} · ${esc(x.alg)}${(x.act||[]).length?" · "+x.act.map(es).join(", "):""}${(x.pend||[]).length?" · pend.: "+x.pend.map(es).join(", "):""}</span></span>
    <div class="bar"><i class="${m?"m":""}" style="width:${Math.min(100,x.inf)}%"></i></div><span class="num">${c1(x.inf)} %</span>`}).join("");
  const con=(s.conflictos||[]).map(c=>`${TIPO[(c.tipo||"").toLowerCase()]||c.tipo} (${c.estado=="active"?"activo":c.estado=="pending"?"pendiente":c.estado}) contra <b>${esc(c.contra)}</b>: ${"<span class=up>■</span>".repeat(c.ganados||0)}${"<span class=dn>■</span>".repeat(c.perdidos||0)} ${c.ganados||0}-${c.perdidos||0}`).join("<br>")||"ninguno";
  const tr=Object.entries(s.trafico||{}).sort().slice(-8).map(([d,n])=>`${d.slice(8)}/${d.slice(5,7)}: <b class=az>${n}</b>`).join(" · ")||"—";
  const k=s.tk||{},vent=(k.vent||[]).map(v=>`${new Date(v[0]*1000).toLocaleDateString("es-ES",{day:"2-digit",month:"2-digit"})} ${hora(v[0])}–${hora(v[1])}`).join("<br>");
  const tk=`último: ${s.tick_ult?hora(s.tick_ult)+(s.tick_marg!=null?" (±"+s.tick_marg+" min)":""):"aún no visto"}<br>
    próximo: <span class=pk>${proxTick(s)?"~"+hora(proxTick(s))+(s.tick_disp!=null?" (±"+s.tick_disp+" min)":""):"—"}</span><br>
    aprendidos: ${s.tick_n||0} bien acotados · ${(k.vent||[]).length} ventanas${vent?"<br><span class=sub>"+vent+"</span>":""}`;
  const si=s.sis||{},pp=s.pp||{};
  const sis=`población: ${pob(si.pob)}<br>economía: ${limpia(si.eco)}${si.eco2&&si.eco2!="$economy_None;"?" / "+limpia(si.eco2):""}<br>
    seguridad: ${SEG[si.seg]||limpia(si.seg)} · gobierno: ${limpia(si.gov)} · alianza: ${esc(si.alg||"—")}<br>
    coordenadas: ${(s.pos||[]).map(v=>v==null?"?":(+v).toFixed(1)).join(" / ")||"—"}${s.ctrl_n?"<br>controla: "+esc(s.ctrl_n):""}`;
  const pw=pp.pot||(pp.pots||[]).length?`potencia: <b>${esc(pp.pot||"ninguna")}</b>${pp.est?" ("+esc(pp.est)+")":""}<br>
    progreso: ${pct(pp.prog)} · refuerzo: ${pp.ref??"—"} · socavamiento: ${pp.soc??"—"}${(pp.pots||[]).length?"<br><span class=sub>presentes: "+pp.pots.map(esc).join(", ")+"</span>":""}`:"sin Powerplay";
  const zc=(s.zc||[]).map(z=>`${ZC[z.i]||z.i} <span class=sub>(${hace(z.t)})</span>`).join(" · ")||"no vistas en 24 h";
  const pn=Object.entries(s.pn||{}).map(([c,v])=>`<b>${esc(c)}</b> ${esc(v.n||"")} <span class=sub>(${hace(v.t)})</span>`).join("<br>")||"no vistos en 7 días";
  const ay=s.prev!=null?`${c1(s.prev)} % → ${c1(s.inf)} % (${(s.inf-s.prev>0?"+":"")+c1(s.inf-s.prev)}) · cambio visto hace ${hace(s.t)}${s.prev_t?" · dato anterior hace "+hace(s.prev_t):""}`:"—";
  const B=(t,c)=>`<h3 style="margin-top:10px">${t}</h3>${c}`;
  return`<div class="dg"><div><h3>Facciones</h3><div class="fac">${fac}</div>${B("Sistema",sis)}${B("Powerplay",pw)}</div>
  <div><h3>Influencia</h3>${grafica(s.hinf)}${B("Desde ayer",ay)}${B("Conflictos",con)}
  ${B("Estados de CDSN","activos: "+((s.activos||[]).map(es).join(", ")||"—")+"<br>pendientes: "+((s.pendientes||[]).map(es).join(", ")||"—")+"<br>recuperando: "+((s.recuperando||[]).map(es).join(", ")||"—"))}</div>
  <div>${B("Tick",tk)}${B("Visitas (pilotos distintos por día)",tr)}${B("Zonas de conflicto",zc)}${B("Portanaves vistos",pn)}
  ${B("Dato","fuente "+esc(s.src||"")+" · hace "+hace(s.t))}</div></div>`;
}
function zona(){
  const Z=Object.entries(D.zona||{});if(!Z.length)return"";
  Z.sort((a,b)=>(a[1].pot||"~").localeCompare(b[1].pot||"~")||a[0].localeCompare(b[0]));
  return`<h3 style="padding:0 16px">Powerplay alrededor (a menos de 20 ly de tus sistemas)</h3><div class="tw"><table style="min-width:600px"><tr><th>Sistema</th><th>Potencia</th><th>Estado</th><th class=num>Progreso</th><th>Presentes</th><th class=num>Dato</th></tr>`+
   Z.map(([n,z])=>`<tr><td>${esc(n)}</td><td>${esc(z.pot||"—")}</td><td>${esc(z.est||"—")}</td><td class=num>${pct(z.prog)}</td><td class=sub>${(z.pots||[]).map(esc).join(", ")}</td><td class="num sub">${hace(z.t)}</td></tr>`).join("")+"</table></div>";
}
async function carga(){
  try{
    const [d,e]=await Promise.all([fetch("/faccion.json").then(r=>r.json()),fetch("/estado.json").then(r=>r.json())]);
    D=d;E=e;
    document.getElementById("est").textContent=`EDDN ${E.conectado?"conectado":"DESCONECTADO"} · ${E.msgmin} msg/min · CDSN ${E.mios} (${E.actualizados} act.) · encendido ${E.min} min`;
    document.getElementById("pie").textContent=`${E.version} · RAM ${E.ram} KB · PSRAM ${E.psram} KB · datos de partida: ${E.semilla?"sí":"no"} · actualizado ${new Date().toLocaleTimeString("es-ES")}`;
    pinta();
  }catch(x){document.getElementById("est").textContent="sin conexión con la S3: "+x}
}
document.getElementById("cab").onclick=e=>{const k=e.target.dataset.k;if(!k)return;if(k==orden)asc=!asc;else{orden=k;asc=k=="sys"}pinta()};
document.getElementById("tb").onclick=e=>{const tr=e.target.closest("tr.s");if(!tr)return;const n=tr.dataset.s;abiertos.has(n)?abiertos.delete(n):abiertos.add(n);pinta()};
for(const id of["q","fc","fa"])document.getElementById(id).oninput=pinta;
carga();setInterval(carga,60000);
</script></body></html>)rawliteral";
