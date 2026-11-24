#pragma once
// Página de ajustes que sirve el reloj (web_settings.cpp) en http://jarvis.local/.
// Estilo "B · Deportivo", igual que la pantalla. Lee /api/estado cada 10 s y cambia las cosas
// con /api/ajuste y /api/alarma.

static const char WEB_PAGE[] = R"rawliteral(<!doctype html>
<html lang="es">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="apple-mobile-web-app-capable" content="yes">
<meta name="mobile-web-app-capable" content="yes">
<meta name="apple-mobile-web-app-status-bar-style" content="black">
<meta name="apple-mobile-web-app-title" content="JARVIS">
<meta name="theme-color" content="#000000">
<link rel="apple-touch-icon" href="/icono.png">
<link rel="icon" href="/icono.png">
<link rel="manifest" href="/manifest.json">
<title>JARVIS Reloj</title>
<link rel="preconnect" href="https://fonts.googleapis.com">
<link href="https://fonts.googleapis.com/css2?family=Barlow+Condensed:wght@500;600;700;800&display=swap" rel="stylesheet">
<style>
:root{--bg:#000;--card:#0d0d0d;--borde:#262626;--gris:#a6a6a6;--blanco:#fff;--acento:#c6f432;--rojo:#ff5a4d;--naranja:#ffb020}
*{box-sizing:border-box}
html,body{margin:0;background:var(--bg);color:var(--blanco);font-family:'Barlow Condensed','Arial Narrow',system-ui,sans-serif;font-size:19px;-webkit-text-size-adjust:100%}
main{max-width:520px;margin:0 auto;padding:calc(14px + env(safe-area-inset-top)) 16px 40px}
h1{font-size:40px;font-weight:800;margin:0;letter-spacing:1px}
h2{font-size:15px;font-weight:600;letter-spacing:2px;color:var(--gris);margin:26px 0 10px;text-transform:uppercase}
.top{display:flex;justify-content:space-between;align-items:center}
.hora{font-size:40px;font-weight:800;color:var(--acento)}
.chips{display:flex;flex-wrap:wrap;gap:8px;margin-top:10px}
.chip{border:2px solid var(--borde);border-radius:999px;padding:4px 12px;font-weight:600;font-size:16px;color:var(--gris)}
.chip b{color:var(--blanco)}
.chip.ok b{color:var(--acento)}.chip.mal b{color:var(--rojo)}
.card{border:2px solid var(--borde);border-radius:16px;padding:12px 14px;margin-bottom:10px;background:var(--card)}
.fila{display:flex;align-items:center;justify-content:space-between;gap:12px}
.nombre{font-size:22px;font-weight:700}
.desc{color:var(--gris);font-size:16px;margin-top:2px;line-height:1.25}
.tag{display:inline-block;font-size:13px;font-weight:700;letter-spacing:1px;border-radius:6px;padding:1px 7px;margin-left:6px;vertical-align:middle}
.alto{background:var(--rojo);color:#000}.medio{background:var(--naranja);color:#000}.bajo{background:var(--borde);color:var(--gris)}
.sw{position:relative;width:58px;height:32px;flex:none}
.sw input{opacity:0;width:0;height:0}
.sw span{position:absolute;inset:0;background:var(--borde);border-radius:16px;transition:.2s}
.sw span:before{content:"";position:absolute;width:24px;height:24px;left:4px;top:4px;background:var(--gris);border-radius:50%;transition:.2s}
.sw input:checked+span{background:var(--acento)}
.sw input:checked+span:before{transform:translateX(26px);background:#000}
.consejo{display:flex;gap:10px;align-items:flex-start;padding:8px 0;border-bottom:1px solid var(--borde);line-height:1.25}
.consejo:last-child{border-bottom:0}
.punto{width:10px;height:10px;border-radius:50%;flex:none;margin-top:7px}
input[type=range]{width:100%;accent-color:var(--acento);height:30px}
select,input[type=number],input[type=time]{background:#000;color:var(--blanco);border:2px solid var(--borde);border-radius:10px;font:inherit;font-size:20px;padding:6px 10px}
.valor{font-size:22px;font-weight:700;min-width:60px;text-align:right}
button{font:inherit;font-weight:700;font-size:19px;border-radius:12px;padding:8px 16px;border:2px solid var(--borde);background:#000;color:var(--blanco);cursor:pointer}
button.lleno{background:var(--acento);border-color:var(--acento);color:#000}
.alarma{display:flex;align-items:center;gap:12px}
.alarma .h{font-size:30px;font-weight:800;flex:1}
.alarma.off .h{color:var(--gris)}
.x{color:var(--rojo);font-size:22px;padding:4px 14px}
table{width:100%;border-collapse:collapse;font-size:16px;font-variant-numeric:tabular-nums}
th{color:var(--gris);font-weight:600;text-align:right;padding:4px 3px;border-bottom:2px solid var(--borde)}
td{text-align:right;padding:5px 3px;border-bottom:1px solid #1a1a1a}
th:first-child,td:first-child{text-align:left}
.gasto{display:grid;grid-template-columns:repeat(3,1fr);gap:8px}
.gasto div{border:2px solid var(--borde);border-radius:12px;padding:8px;text-align:center}
.gasto b{display:block;font-size:26px}
.gasto span{color:var(--gris);font-size:14px;letter-spacing:1px}
.aviso{color:var(--gris);font-size:16px;line-height:1.3}
#error{display:none;background:var(--rojo);color:#000;font-weight:700;border-radius:12px;padding:10px 14px;margin-top:12px}
</style>
</head>
<body>
<main>
  <div class="top"><h1>JARVIS</h1><div class="hora" id="hora">--:--</div></div>
  <div class="chips" id="chips"></div>
  <div id="error">No contesta el reloj. ¿Tiene la pantalla prendida y está en el mismo WiFi?</div>

  <h2>Consejos</h2>
  <div class="card" id="consejos"></div>

  <h2>Funciones</h2>
  <div id="funciones"></div>

  <h2>Pantalla y energía</h2>
  <div class="card">
    <div class="fila"><div class="nombre">Brillo</div><div class="valor" id="v-brillo"></div></div>
    <input type="range" min="1" max="5" step="1" id="brillo">
    <div class="desc">La pantalla es lo que más gasta. 60 % se ve bien y ahorra.</div>
  </div>
  <div class="card fila">
    <div><div class="nombre">Apagar la pantalla</div><div class="desc">Sin tocarla.</div></div>
    <select id="pantalla"><option value="10">10 s</option><option value="15">15 s</option><option value="30">30 s</option><option value="60">1 min</option><option value="120">2 min</option></select>
  </div>
  <div class="card fila">
    <div><div class="nombre">Modo dormir solo</div><div class="desc">Con la pantalla apagada. Gasta muchísimo menos; las alarmas suenan igual.</div></div>
    <select id="dormir"><option value="5">5 min</option><option value="10">10 min</option><option value="20">20 min</option><option value="30">30 min</option><option value="0">Nunca</option></select>
  </div>
  <div class="card">
    <div class="fila"><div class="nombre">Volumen de la alarma</div><div class="valor" id="v-volumen"></div></div>
    <input type="range" min="0" max="10" step="1" id="volumen">
  </div>
  <div class="card fila">
    <div><div class="nombre">Meta de pasos</div><div class="desc">La barra de INICIO.</div></div>
    <input type="number" id="meta" min="1000" max="50000" step="500" style="width:120px">
  </div>

  <h2>Alarmas</h2>
  <div id="alarmas"></div>
  <div class="card fila">
    <input type="time" id="nueva" value="07:00">
    <button class="lleno" id="agregar">+ AGREGAR</button>
  </div>

  <h2>Batería</h2>
  <div class="gasto" id="gasto"></div>
  <div class="card" style="margin-top:10px;overflow-x:auto">
    <table><thead><tr><th>Hora</th><th>%</th><th>mV</th><th title="Pantalla prendida">Pren.</th><th title="Pantalla apagada">Apag.</th><th title="Modo dormir">Dorm.</th><th title="Del tiempo dormido, cuánto durmió el chip de verdad">S%</th></tr></thead>
    <tbody id="registro"></tbody></table>
    <div class="aviso" style="margin-top:8px">Cada 15 min se anota una línea (minutos en cada estado). ⚡ = enchufado.</div>
  </div>

  <h2>Acceso rápido</h2>
  <div class="card aviso">En el iPhone, tocá <b>Compartir</b> y después <b>Agregar a inicio</b>: queda un ícono de JARVIS para abrir esta página de un toque (el reloj tiene que estar con la pantalla prendida y en el mismo WiFi o en el hotspot del celu).</div>
</main>
<script>
let est = null, editando = 0;
const $ = id => document.getElementById(id);

const FUNCIONES = [
  {k: 'bt', nombre: 'Bluetooth', gasto: 'alto',
   desc: 'Música (Spotify) y avisos del iPhone. Gasta todo el tiempo que el reloj está despierto, también con la pantalla apagada.'},
  {k: 'avisos', nombre: 'Avisos del iPhone', gasto: 'bajo',
   desc: 'WhatsApp, mensajes y llamadas en la pantalla. Cada aviso prende la pantalla. Necesita el Bluetooth.'},
  {k: 'muneca', nombre: 'Levantar la muñeca', gasto: 'medio',
   desc: 'Muestra la hora grande 5 s. Prende la pantalla cada vez que movés el brazo así.'},
  {k: 'clima', nombre: 'Clima', gasto: 'bajo',
   desc: 'Pide la temperatura cada 30 min (solo con WiFi) para la tarjeta de INICIO.'},
];

async function api(url, datos) {
  editando = Date.now();
  const r = await fetch(url, {method: 'POST', body: new URLSearchParams(datos)});
  if (!r.ok) throw new Error(r.status);
  await cargar();
}

const dec = v => v.toLocaleString('es-AR', {minimumFractionDigits: 1, maximumFractionDigits: 1});
function chip(txt, val, clase) { return `<div class="chip ${clase || ''}">${txt} <b>${val}</b></div>`; }

function pintar() {
  const e = est, a = e.ajustes;
  $('hora').textContent = e.hora;
  let bat = e.bateria < 0 ? '--' : e.bateria + '%' + (e.cargando ? ' ⚡' : '');
  $('chips').innerHTML =
    chip('Batería', bat, e.bateria >= 0 && e.bateria < 20 && !e.cargando ? 'mal' : 'ok') +
    chip('WiFi', e.wifi.red || '-', 'ok') +
    chip('Bluetooth', !a.bt ? 'apagado' : e.bt.conectado ? 'conectado' : 'sin celu', a.bt && !e.bt.conectado ? 'mal' : '') +
    chip('Pasos', e.pasos.toLocaleString('es-AR'), '') +
    (e.clima.ok ? chip('Clima', e.clima.temp + '° ' + e.clima.desc, '') : '');

  // Consejos según lo que está prendido y lo que gastó.
  const c = [];
  const rojo = 'var(--rojo)', nar = 'var(--naranja)', lima = 'var(--acento)';
  if (e.bateria >= 0 && e.bateria < 20 && !e.cargando) c.push([rojo, 'Batería baja: cargalo pronto o poné el modo dormir (PWR 1,5 s).']);
  if (a.bt && !e.bt.conectado) c.push([rojo, 'El Bluetooth está prendido pero el celu no está conectado: gasta igual. Si no vas a usar música ni avisos, apagalo.']);
  if (a.avisos && !a.bt) c.push([nar, 'Los avisos del iPhone necesitan el Bluetooth prendido: así no van a llegar.']);
  if (a.brillo >= 4) c.push([nar, `Brillo en ${a.brillo * 20} %: bajarlo a 60 % ahorra batería y casi no se nota.`]);
  if (a.pantalla >= 30) c.push([nar, `La pantalla tarda ${a.pantalla >= 60 ? a.pantalla / 60 + ' min' : a.pantalla + ' s'} en apagarse: con 15 s alcanza.`]);
  if (a.dormir == 0) c.push([rojo, 'El modo dormir automático está apagado: con la pantalla apagada el reloj gasta varias veces más. Ponelo en 10 min.']);
  else if (a.dormir >= 20) c.push([nar, `Entra en modo dormir recién a los ${a.dormir} min: con 10 min dura más la batería.`]);
  if (a.muneca) c.push([lima, 'Levantar la muñeca está prendido: si casi no lo usás, apagalo y ahorrás un poco.']);
  const g = e.gasto;
  if (g[1] > 0 && g[2] > 0 && g[1] > g[2] * 2) c.push([nar, `Con la pantalla apagada gasta ${dec(g[1])} %/h y dormido ${dec(g[2])} %/h: el modo dormir más temprano rinde.`]);
  if (!c.length) c.push([lima, 'Todo en orden: la configuración ya está pensada para que dure la batería.']);
  $('consejos').innerHTML = c.map(([col, t]) => `<div class="consejo"><div class="punto" style="background:${col}"></div><div>${t}</div></div>`).join('');

  $('funciones').innerHTML = FUNCIONES.map(f => `
    <div class="card fila"><div><div class="nombre">${f.nombre}<span class="tag ${f.gasto}">GASTO ${f.gasto.toUpperCase()}</span></div>
    <div class="desc">${f.desc}</div></div>
    <label class="sw"><input type="checkbox" data-k="${f.k}" ${a[f.k] ? 'checked' : ''}><span></span></label></div>`).join('');
  document.querySelectorAll('#funciones input').forEach(i =>
    i.onchange = () => api('/api/ajuste', {k: i.dataset.k, v: i.checked ? 1 : 0}).catch(fallo));

  if (document.activeElement.tagName !== 'INPUT' && document.activeElement.tagName !== 'SELECT') {
    $('brillo').value = a.brillo; $('volumen').value = a.volumen;
    $('pantalla').value = a.pantalla; $('dormir').value = a.dormir; $('meta').value = a.meta;
  }
  $('v-brillo').textContent = $('brillo').value * 20 + ' %';
  $('v-volumen').textContent = $('volumen').value + '/10';

  $('alarmas').innerHTML = e.alarmas.length ? e.alarmas.map((al, i) => `
    <div class="card alarma ${al.on ? '' : 'off'}"><div class="h">${String(al.h).padStart(2, '0')}:${String(al.m).padStart(2, '0')}</div>
    <label class="sw"><input type="checkbox" data-i="${i}" ${al.on ? 'checked' : ''}><span></span></label>
    <button class="x" data-b="${i}">✕</button></div>`).join('') :
    '<div class="card aviso">No hay alarmas. También se las podés pedir a JARVIS.</div>';
  document.querySelectorAll('#alarmas input').forEach(i =>
    i.onchange = () => api('/api/alarma', {accion: 'cambiar', i: i.dataset.i}).catch(fallo));
  document.querySelectorAll('#alarmas button').forEach(b =>
    b.onclick = () => api('/api/alarma', {accion: 'borrar', i: b.dataset.b}).catch(fallo));
  $('agregar').disabled = e.alarmas.length >= e.maxAlarmas;

  const nombres = ['PRENDIDA', 'APAGADA', 'DORMIDO'];
  $('gasto').innerHTML = g.map((v, i) => `<div><b>${v < 0 ? '–' : dec(v)}</b><span>${nombres[i]} %/H</span></div>`).join('');
  $('registro').innerHTML = e.registro.map(r => `<tr><td>${r.t}${r.c ? ' ⚡' : ''}</td><td>${r.pct}</td><td>${r.mv}</td><td>${r.p}</td><td>${r.a}</td><td>${r.d}</td><td>${r.d ? r.s : ''}</td></tr>`).join('') ||
    '<tr><td colspan="7" class="aviso">Todavía no hay datos.</td></tr>';
}

function fallo() { $('error').style.display = 'block'; }

async function cargar() {
  try {
    const r = await fetch('/api/estado', {cache: 'no-store'});
    est = await r.json();
    $('error').style.display = 'none';
    pintar();
  } catch (e) { fallo(); }
}

const ajuste = (id, k, ev) => $(id).addEventListener(ev || 'change', () => api('/api/ajuste', {k: k || id, v: $(id).value}).catch(fallo));
ajuste('brillo'); ajuste('volumen'); ajuste('pantalla'); ajuste('dormir'); ajuste('meta');
$('brillo').oninput = () => $('v-brillo').textContent = $('brillo').value * 20 + ' %';
$('volumen').oninput = () => $('v-volumen').textContent = $('volumen').value + '/10';
$('agregar').onclick = () => {
  const [h, m] = $('nueva').value.split(':');
  api('/api/alarma', {accion: 'nueva', h: +h, m: +m}).catch(fallo);
};
cargar();
setInterval(() => { if (Date.now() - editando > 3000 && !document.hidden) cargar(); }, 10000);
document.addEventListener('visibilitychange', () => { if (!document.hidden) cargar(); });
</script>
</body>
</html>
)rawliteral";

static const char WEB_MANIFEST[] = R"rawliteral({"name":"JARVIS Reloj","short_name":"JARVIS","start_url":"/","display":"standalone","background_color":"#000000","theme_color":"#000000","icons":[{"src":"/icono.png","sizes":"180x180","type":"image/png"}]})rawliteral";
