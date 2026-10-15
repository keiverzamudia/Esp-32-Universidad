(function(){
"use strict";
var KEY_LS="tracex.key";
var POLL_MS=2500,POLL_FAIL_MS=6000;
var PHASES=["scenario","expected","education","result","capture","steps","raw","investigate"];
var PHASE_TITLES={
  scenario:"ESCENARIO DE PRUEBA",
  expected:"RESULTADO ESPERADO",
  education:"SECUENCIA EDUCATIVA",
  result:"ANÁLISIS DE RESULTADO",
  capture:"CAPTURA DE EVIDENCIA",
  steps:"PASOS REGISTRADOS",
  raw:"INFORME ORIGINAL",
  investigate:"INVESTIGACIÓN HABILITADA"
};
var EDU_SCRIPT=[
  "Playwright ejecuta el test en un navegador real (Chromium).",
  "Cada acción queda registrada: clics, navegación, expectativas.",
  "Si algo falla, el runner congela el contexto y guarda el estado.",
  "La traza (.zip) incluye snapshots DOM, red y llamadas al DOM.",
  "La captura de pantalla es una pieza de evidencia, no el fallo completo.",
  "El informe HTML enlaza traza, video y screenshots por paso."
];

var S={v:-1,phase:"idle",sidx:0,poll:0,view:"home",detail:null,conn:"init",gallery:null,evCache:{},lastPhase:null};
var $=function(id){return document.getElementById(id)};

function esc(s){return String(s==null?"":s).replace(/[&<>"']/g,function(c){
  return {"&":"&amp;","<":"&lt;",">":"&gt;",'"':"&quot;","'":"&#39;"}[c];});}

function getKey(){try{return localStorage.getItem(KEY_LS)||"";}catch(e){return "";}}
function setKey(k){try{localStorage.setItem(KEY_LS,k);}catch(e){}}

function post(url){
  var k=getKey();
  if(!k){k=prompt("Clave de control del reactor:");if(k!==null){setKey(k.trim());k=k.trim();}else{return;}}
  fetch(url,{method:"POST",headers:{"X-TraceX-Key":k}}).then(function(r){
    if(r.status===401){setKey("");alert("Clave incorrecta. Inténtalo de nuevo.");}
  }).catch(function(){setConn("bad");});
}

function setConn(st){
  if(S.conn===st)return;S.conn=st;
  var c=$("conn");c.className="conn "+(st==="ok"?"ok":st==="bad"?"bad":"");
  $("connTxt").textContent=st==="ok"?"ENLACE ACTIVO":st==="bad"?"SIN ENLACE":"ENLAZANDO";
}

function route(){
  var h=location.hash.replace(/^#/,"");
  if(h.indexOf("/evidence/")===0){S.detail=h.slice(10);showView("gallery");openDetail(S.detail);return;}
  if(h==="/gallery"){S.detail=null;closeDetail();showView("gallery");return;}
  if(h==="/present"){showView("present");return;}
  S.detail=null;closeDetail();showView("home");
}
function go(hash){location.hash=hash;}

function showView(v){
  S.view=v;
  $("view-home").hidden=v!=="home";
  $("view-present").hidden=v!=="present";
  $("view-gallery").hidden=v!=="gallery";
  if(v==="gallery")renderGallery();
  if(v==="present")renderPresent(true);
  if(v==="home")renderHome();
}

function stat(b,val,cls){
  return '<div class="stat"><b>'+esc(b)+'</b><span class="'+(cls||"")+'">'+esc(val)+"</span></div>";
}

function renderHome(){
  var ph=S.phase==="idle"?"EN ESPERA":S.phase.toUpperCase();
  var cls=S.phase==="idle"?"am":"cy";
  $("homeStats").innerHTML=
    stat("FIRMWARE",S.fw||"—","cy")+
    stat("FASE DEL REACTOR",ph,cls)+
    stat("CLIENTES ACTIVOS",S.clients!=null?String(S.clients):"—",S.clients>0?"gr":"am")+
    stat("MEMORIA LIBRE",S.heap!=null?Math.round(S.heap/1024)+" KB":"—","");
  if(S.phase!=="idle"){
    $("homeHint").textContent="Secuencia en curso ("+S.phase+"). Toca INICIAR para continuar la presentación.";
    $("btnStart").textContent="CONTINUAR PRESENTACIÓN ▸";
  }else{
    $("btnStart").textContent="INICIAR EXPERIENCIA ▸";
  }
  if(S.gallery&&S.gallery.items){
    var ul=$("homeScenarios"),html="";
    S.gallery.items.forEach(function(it){
      html+='<li><span class="sid">'+esc(it.id)+'</span><span class="sname">'+esc(it.name)+
        '</span><span class="pill '+esc(it.status)+'">'+esc(it.status)+"</span></li>";
    });
    ul.innerHTML=html;
  }
}

function buildPhasebar(fail){
  var idx=PHASES.indexOf(S.phase);
  var html="";
  for(var i=0;i<PHASES.length;i++){
    var cls=i<idx?"done":i===idx?"on"+(fail&&PHASES[i]==="result"?" fail":""):"";
    html+='<i class="'+cls+'"></i>';
  }
  $("phasebar").innerHTML=html;
}

function curEv(){
  var g=S.gallery;if(!g||!g.items)return null;
  return g.items[S.sidx]||g.items[0];
}

function evDetail(id,cb){
  if(S.evCache[id]){cb(S.evCache[id]);return;}
  fetch("/api/evidence/"+id).then(function(r){return r.json();}).then(function(j){
    S.evCache[id]=j;cb(j);
  }).catch(function(){cb(null);});
}

function renderPresent(force){
  var ev=curEv();
  var fail=ev&&ev.status==="FAIL";
  buildPhasebar(fail);
  if(S.phase==="idle"){
    $("presentBody").innerHTML='<div class="stage"><div class="stage-num">REACTOR EN ESPERA</div>'+
      '<div class="big" style="color:var(--cy)">STANDBY</div>'+
      "<p>El presentador (o el botón BOOT) iniciará la secuencia de investigación.</p></div>";
    return;
  }
  var body=$("presentBody");
  var ph=S.phase;
  var head='<div class="stage"><div class="stage-num">PASO '+
    (PHASES.indexOf(ph)+1)+"/"+PHASES.length+" · "+esc(PHASE_TITLES[ph]||ph)+"</div>";
  var tail="</div>";

  if(ph==="scenario"){
    body.innerHTML=head+
      "<h2>"+esc(ev?ev.name:"Escenario")+"</h2>"+
      '<p class="mut">Identificador de evidencia: '+esc(ev?ev.id:"—")+"</p>"+
      "<p>Playwright intentará validar este escenario en la aplicación bajo prueba. "+
      "Observa qué espera el test antes de ejecutarlo.</p>"+
      (ev?"":'<p class="mut">Sin escenarios cargados. ¿Firmware sin manifiesto?</p>')+
      tail;
  }else if(ph==="expected"){
    body.innerHTML=head+
      "<h2>¿Qué debería ocurrir?</h2>"+
      '<div class="quote gr">'+esc(ev?ev.expected:"(sin resultado esperado registrado)")+"</div>"+
      "<p>Esta es la expectativa codificada en el test automatizado.</p>"+
      tail;
  }else if(ph==="education"){
    var lis="";
    for(var i=0;i<EDU_SCRIPT.length;i++){
      lis+='<li style="animation-delay:'+(i*120)+'ms">'+esc(EDU_SCRIPT[i])+"</li>";
    }
    body.innerHTML=head+
      "<h2>Cómo Playwright construye evidencia</h2>"+
      '<ol class="steps">'+lis+"</ol>"+tail;
  }else if(ph==="result"){
    var st=ev?ev.status:"FAIL";
    body.innerHTML=head+
      '<div class="big '+esc(st)+'">'+esc(st==="PASS"?"PRUEBA APROBADA":"FALLO DETECTADO")+"</div>"+
      (st==="FAIL"?'<p style="text-align:center;color:var(--red)">«Un fallo no es el final. Es el comienzo de la investigación.»</p>':
       '<p style="text-align:center;color:var(--green)">El resultado coincide con la expectativa.</p>')+
      tail;
    if(st==="FAIL"&&(force||S.lastPhase!==ph)){flashFail();}
  }else if(ph==="capture"){
    var img=ev?'<div class="capture"><img src="/img/'+esc(ev.id)+
      '_full.jpg" alt="Captura de evidencia '+esc(ev.id)+'" loading="lazy" decoding="async"'+
      ' onerror="this.parentElement.innerHTML=\'<p style=&quot;padding:20px&quot;>Captura no disponible</p>\'">'+
      '<div class="cap-bar"><span>'+esc(ev.id)+"_full.jpg</span><span>EVIDENCIA GRÁFICA</span></div></div>":
      '<p class="mut">Sin captura asociada.</p>';
    body.innerHTML=head+
      "<h2>Captura registrada por el runner</h2>"+img+
      "<p>"+esc(ev?ev.explanation:"")+"</p>"+tail;
  }else if(ph==="steps"){
    evDetail(ev?ev.id:"",function(d){
      var steps=(d&&d.steps&&d.steps.length)?d.steps:["(sin pasos registrados)"];
      var lis="";
      steps.forEach(function(s,i){lis+='<li style="animation-delay:'+(i*90)+'ms">'+esc(s)+"</li>";});
      body.innerHTML=head+
        "<h2>Traza de ejecución</h2>"+
        '<ol class="steps">'+lis+"</ol>"+tail;
    });
    body.innerHTML=head+"<h2>Traza de ejecución</h2><p class='mut'>Revelando pasos…</p>"+tail;
  }else if(ph==="raw"){
    body.innerHTML=head+
      "<h2>Resultado original del runner</h2>"+
      '<div class="duo"><div class="exp"><b>ESPERADO</b><p>'+esc(ev?ev.expected:"—")+
      '</p></div><div class="obs"><b>OBSERVADO</b><p>'+esc(ev?ev.observed:"—")+"</p></div></div>"+
      '<p class="mut" style="margin-top:10px;font-size:.66rem">Fuente: '+esc(ev?ev.source:"—")+"</p>"+
      tail;
  }else if(ph==="investigate"){
    var refs="";
    if(ev&&ev.related&&ev.related.length){
      refs='<div class="refs">';ev.related.forEach(function(r){
        refs+='<button class="ref" data-ev="'+esc(r)+'">EVIDENCIA '+esc(r)+" ▸</button>";
      });refs+="</div>";
    }
    body.innerHTML=head+
      "<h2>Investiga la evidencia</h2>"+
      "<p>El fallo es una puerta: abre la galería, examina la captura y los pasos, y reconstruye lo que ocurrió.</p>"+
      refs+
      '<div class="cta-row"><button class="btn primary" id="btnOpenGal">ABRIR GALERÍA ▸</button></div>'+
      tail;
    var b=$("btnOpenGal");if(b)b.onclick=function(){go("/gallery");};
    Array.prototype.forEach.call(document.querySelectorAll(".ref"),function(el){
      el.onclick=function(){go("/evidence/"+el.getAttribute("data-ev"));};
    });
  }
  S.lastPhase=ph;
}

function flashFail(){
  var f=$("flash");f.classList.remove("on");void f.offsetWidth;f.classList.add("on");
}

function renderGallery(){
  var g=S.gallery;
  if(!g||!g.items){
    $("galGrid").innerHTML="";$("galNote").textContent="Cargando manifiesto de evidencias…";return;
  }
  $("galNote").textContent=g.items.some(function(i){return !i.authentic;})?
    "DEMOSTRACIÓN EDUCATIVA: algunas evidencias son ilustrativas. No representan ejecuciones en vivo del dispositivo.":
    "Evidencias preimportadas. Ninguna se genera en el dispositivo durante la exposición.";
  var html="";
  g.items.forEach(function(it,i){
    html+='<button class="card" data-ev="'+esc(it.id)+'" style="animation-delay:'+(i*40)+'ms">'+
      '<img src="/img/'+esc(it.id)+'_thumb.jpg" alt="'+esc(it.name)+'" loading="lazy" decoding="async">'+
      '<span class="cmeta"><span class="cname">'+esc(it.name)+'</span>'+
      '<span class="crow"><span class="cid">'+esc(it.id)+'</span>'+
      '<span class="pill '+esc(it.status)+'">'+esc(it.status)+"</span></span></span></button>";
  });
  $("galGrid").innerHTML=html;
  Array.prototype.forEach.call(document.querySelectorAll(".card"),function(el){
    el.onclick=function(){go("/evidence/"+el.getAttribute("data-ev"));};
  });
}

function openDetail(id){
  var g=S.gallery;
  var meta=null;
  if(g&&g.items){g.items.forEach(function(it){if(it.id===id)meta=it;});}
  $("detailBadge").className="badge "+(meta&&meta.authentic?"real":"demo");
  $("detailBadge").textContent=meta&&meta.authentic?"EVIDENCIA PREIMPORTADA":"DEMOSTRACIÓN EDUCATIVA";
  $("detailBody").innerHTML="<p class='mut'>Cargando evidencia "+esc(id)+"…</p>";
  $("detail").hidden=false;
  evDetail(id,function(d){
    if(!d){$("detailBody").innerHTML="<p class='mut'>Evidencia no encontrada.</p>";return;}
    var steps="";if(d.steps&&d.steps.length){
      steps='<div class="panel"><h3 class="panel-t">PASOS REGISTRADOS</h3><ol class="steps">';
      d.steps.forEach(function(s,i){steps+='<li style="animation-delay:'+(i*70)+'ms">'+esc(s)+"</li>";});
      steps+="</ol></div>";
    }
    var refs="";if(d.related&&d.related.length){
      refs='<div class="refs">';d.related.forEach(function(r){
        refs+='<button class="ref" data-ev="'+esc(r)+'">→ '+esc(r)+"</button>";
      });refs+="</div>";
    }
    $("detailBody").innerHTML=
      '<div class="capture"><img src="/img/'+esc(d.id)+'_full.jpg" alt="'+esc(d.name)+
      '" loading="lazy" decoding="async" onerror="this.parentElement.innerHTML=\'<p style=&quot;padding:20px&quot;>Captura no disponible</p>\'">'+
      '<div class="cap-bar"><span>'+esc(d.id)+"_full.jpg</span><span>"+
      esc(d.status)+"</span></div></div>"+
      '<div class="panel"><h3 class="panel-t">'+esc(d.id)+" · "+esc(d.status)+"</h3>"+
      "<h2 style='font-size:1.1rem;margin-bottom:8px'>"+esc(d.name)+"</h2>"+
      "<p style='font-size:.8rem'>"+esc(d.explanation)+"</p></div>"+
      '<div class="panel"><div class="duo"><div class="exp"><b>ESPERADO</b><p>'+esc(d.expected)+
      '</p></div><div class="obs"><b>OBSERVADO</b><p>'+esc(d.observed)+"</p></div></div></div>"+
      steps+
      '<p class="mut" style="font-size:.62rem">Procedencia: '+esc(d.source)+
      (d.note?" · "+esc(d.note):"")+"</p>"+
      refs;
    Array.prototype.forEach.call($("detailBody").querySelectorAll(".ref"),function(el){
      el.onclick=function(){go("/evidence/"+el.getAttribute("data-ev"));};
    });
  });
}
function closeDetail(){$("detail").hidden=true;S.detail=null;}

function applyState(j){
  S.v=j.v;S.phase=j.phase;S.sidx=j.scenario_idx||0;S.fw=j.fw;
  if(j.clients!=null)S.clients=j.clients;
  if(j.free_heap!=null)S.heap=j.free_heap;
  setConn("ok");
  var newPhase=(j.phase!==S.lastPhase)||false;
  if(S.view==="home")renderHome();
  if(S.view==="present")renderPresent(true);
  $("footL").textContent="TRACE-X "+(S.fw||"")+" · FASE "+String(S.phase).toUpperCase();
}

function poll(){
  fetch("/api/state?since="+S.v).then(function(r){
    if(!r.ok)throw new Error("http "+r.status);
    return r.json();
  }).then(function(j){
    if(j&&j.v!=null&&j.v!==S.v){applyState(j);S.poll=POLL_MS;}
    else{setConn("ok");if(j&&j.clients!=null){S.clients=j.clients;$("footR").textContent="CLI "+j.clients+" · 192.168.4.1";}}
  }).catch(function(){
    setConn("bad");S.v=-1;S.poll=POLL_FAIL_MS;
  }).then(function(){setTimeout(poll,S.poll||POLL_MS);});
}

function loadGallery(then){
  if(S.gallery){if(then)then();return;}
  fetch("/api/gallery").then(function(r){return r.json();}).then(function(j){
    S.gallery=j;setConn("ok");if(then)then();
    if(S.view==="home")renderHome();
    if(S.view==="gallery")renderGallery();
  }).catch(function(){if(then)then();});
}

$("btnStart").onclick=function(){loadGallery(function(){post("/api/advance");go("/present");});};
$("btnNext").onclick=function(){post("/api/advance");};
$("btnReset").onclick=function(){post("/api/reset");};
$("btnHome").onclick=function(){go("/");};
$("btnGallery").onclick=function(){go("/gallery");};
$("btnBack").onclick=function(){go("/");};
$("detailClose").onclick=function(){closeDetail();go("/gallery");};
window.addEventListener("hashchange",route);

loadGallery();
route();
poll();
})();
