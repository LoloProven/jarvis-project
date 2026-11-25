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
.sub{font-size:14px;letter-spacing:1px;color:#666;margin-left:6px}
.dia{font-size:15px;font-weight:700;letter-spacing:2px;color:var(--acento);margin:14px 0 6px}
.evento{display:flex;gap:12px;align-items:baseline}
.evento .hs{font-size:22px;font-weight:800;min-width:58px;font-variant-numeric:tabular-nums}
.evento .t{font-size:20px;font-weight:600;flex:1}
.jv{display:inline-block;background:var(--acento);color:#000;font-size:12px;font-weight:800;letter-spacing:1px;border-radius:5px;padding:0 6px;margin-left:6px;vertical-align:middle}
.charla .q{color:var(--gris);font-size:17px}
.charla .r{font-size:19px;margin-top:4px;line-height:1.25}
.charla .meta{display:flex;justify-content:space-between;color:#666;font-size:14px;letter-spacing:1px;margin-bottom:4px}
.hizo{display:inline-block;border:2px solid var(--acento);color:var(--acento);border-radius:6px;font-size:14px;font-weight:700;padding:0 6px;margin:6px 6px 0 0}
.toca{font-size:34px;font-weight:800;color:var(--acento);line-height:1}
.ej{font-size:19px;padding:4px 0;border-bottom:1px solid #1a1a1a}
.ej:last-child{border-bottom:0}
.examen{display:flex;gap:12px;align-items:center;padding:6px 0;border-bottom:1px solid #1a1a1a}
.examen:last-child{border-bottom:0}
.examen .n{font-size:30px;font-weight:800;min-width:52px;text-align:center;font-variant-numeric:tabular-nums;line-height:1}
.examen .n small{display:block;font-size:12px;color:var(--gris);letter-spacing:1px}
.examen .n.cerca{color:var(--rojo)}
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

  <h2>Agenda <span class="sub" id="agenda-cuando"></span></h2>
  <div id="agenda"><div class="card aviso">Cargando la agenda...</div></div>

  <h2>Lo que hizo JARVIS</h2>
  <div id="historial"></div>

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

  <h2>Gimnasio <span class="sub" id="planilla-cuando"></span></h2>
  <div id="gimnasio"><div class="card aviso">Cargando la planilla...</div></div>

  <h2>Exámenes</h2>
  <div id="examenes"></div>

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

const esc = s => String(s).replace(/[&<>"]/g, c => ({'&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;'}[c]));
const DIAS = ['DOMINGO', 'LUNES', 'MARTES', 'MIÉRCOLES', 'JUEVES', 'VIERNES', 'SÁBADO'];
const MESES = ['ENE', 'FEB', 'MAR', 'ABR', 'MAY', 'JUN', 'JUL', 'AGO', 'SEP', 'OCT', 'NOV', 'DIC'];
const hhmm = d => String(d.getHours()).padStart(2, '0') + ':' + String(d.getMinutes()).padStart(2, '0');
function nombreDia(d) {
  const hoy = new Date(); hoy.setHours(0, 0, 0, 0);
  const x = new Date(d); x.setHours(0, 0, 0, 0);
  const dif = Math.round((x - hoy) / 864e5);
  const base = `${DIAS[x.getDay()]} ${x.getDate()} ${MESES[x.getMonth()]}`;
  return dif === 0 ? 'HOY · ' + base : dif === 1 ? 'MAÑANA · ' + base : base;
}

// Lo que el reloj trae de n8n: si contesta "cargando", lo está pidiendo; volver a preguntar.
async function desdeN8n(ruta, nueva) {
  let q = nueva ? '?nueva=1' : '';
  for (let i = 0; i < 20; i++) {
    const j = await (await fetch(ruta + q, {cache: 'no-store'})).json();
    if (!j.cargando) return j;
    await new Promise(r => setTimeout(r, 2000));
    q = '?vuelta=1';
  }
  return {error: -5};
}

// Agenda de Google Calendar (hoy y las próximas 2 semanas), vía el reloj y n8n.
async function cargarAgenda(nueva) {
  try {
    const a = await desdeN8n('/api/agenda', nueva);
    if (a.error !== undefined) {
      $('agenda').innerHTML = `<div class="card aviso">No pude traer la agenda (n8n ${a.error < 0 ? 'no responde' : 'respondió ' + a.error}). Tiene que estar la compu prendida con las 3 ventanas de JARVIS abiertas (ngrok, voz y n8n).</div>`;
      return;
    }
    $('agenda-cuando').textContent = 'actualizada ' + hhmm(new Date());
    if (!a.eventos.length) { $('agenda').innerHTML = '<div class="card aviso">Nada en las próximas 2 semanas.</div>'; return; }
    let html = '', dia = '';
    for (const e of a.eventos) {
      const ini = e.todoElDia ? new Date(e.inicio + 'T00:00') : new Date(e.inicio);
      const nd = nombreDia(ini);
      if (nd !== dia) { if (dia) html += '</div>'; html += `<div class="dia">${nd}</div><div class="card">`; dia = nd; }
      html += `<div class="evento"><div class="hs">${e.todoElDia ? 'TODO' : hhmm(ini)}</div><div class="t">${esc(e.titulo)}${e.jarvis ? '<span class="jv">JARVIS</span>' : ''}</div></div>`;
    }
    $('agenda').innerHTML = html + '</div>';
  } catch (err) {
    $('agenda').innerHTML = '<div class="card aviso">No pude traer la agenda.</div>';
  }
}

// Planilla de Google Sheets (gimnasio y exámenes), vía el reloj y n8n. La edita JARVIS.
async function cargarPlanilla() {
  try {
    const p = await desdeN8n('/api/planilla');
    if (p.error !== undefined) {
      $('gimnasio').innerHTML = `<div class="card aviso">No pude traer la planilla (n8n ${p.error < 0 ? 'no responde' : 'respondió ' + p.error}).</div>`;
      $('examenes').innerHTML = '';
      return;
    }
    $('planilla-cuando').textContent = 'actualizada ' + hhmm(new Date());
    let g = '';
    if (p.toca) {
      g += `<div class="card"><div class="fila"><div><div class="desc">HOY TOCA</div><div class="toca">${esc(p.toca.dia).toUpperCase()}</div></div>
        <div class="desc" style="text-align:right">${esc(p.toca.grupo)}<br>${esc(p.toca.series)}</div></div>
        <div style="margin-top:8px">${p.toca.ejercicios.map(e => `<div class="ej">${esc(e)}</div>`).join('')}</div>
        ${p.toca.extras ? `<div class="desc" style="margin-top:6px">+ ${esc(p.toca.extras)}</div>` : ''}
        ${p.ultimoDia ? `<div class="desc" style="margin-top:6px">Último: ${esc(p.ultimoDia.dia)} el ${esc(p.ultimoDia.fecha)}</div>` : ''}</div>`;
    }
    if (p.registro.length) {
      g += `<div class="card" style="overflow-x:auto"><table><thead><tr><th>Fecha</th><th>Ejercicio</th><th>Kg</th><th>Series</th><th>Reps</th></tr></thead><tbody>
        ${p.registro.map(r => `<tr><td>${esc(r.fecha.slice(0, 5))}</td><td style="text-align:left">${esc(r.ejercicio)}</td><td>${esc(r.peso)}</td><td>${esc(r.series)}</td><td>${esc(r.reps)}</td></tr>`).join('')}
        </tbody></table></div>`;
    } else {
      g += '<div class="card aviso">Todavía no anotaste pesos. Decile a JARVIS, por ejemplo: “hice jalón al pecho 50 kilos, 4 series de 10”.</div>';
    }
    $('gimnasio').innerHTML = g;
    $('examenes').innerHTML = p.materias.length ? '<div class="card">' + p.materias.map(m =>
      `<div class="examen"><div class="n ${m.faltan !== null && m.faltan <= 7 ? 'cerca' : ''}">${m.faltan === null ? '?' : m.faltan}<small>${m.faltan === 1 ? 'DÍA' : 'DÍAS'}</small></div>
        <div><div class="nombre" style="font-size:20px">${esc(m.materia)}</div><div class="desc">${esc(m.evaluacion)} · ${esc(m.fecha)}${m.notas ? ' · ' + esc(m.notas) : ''}</div></div></div>`).join('') + '</div>'
      : '<div class="card aviso">No hay exámenes anotados.</div>';
  } catch (err) {
    $('gimnasio').innerHTML = '<div class="card aviso">No pude traer la planilla.</div>';
  }
}

// Últimas charlas con JARVIS (las guarda el reloj).
const ACCIONES = [[/^ALARMA (\d+:\d+)/, m => 'Alarma ' + m[1]], [/^BORRAR (\d+:\d+)/, m => 'Borró alarma ' + m[1]],
  [/^TIMER (\d+)/, m => 'Temporizador ' + Math.round(m[1] / 60) + ' min'], [/^CRONO (\w+)/, m => 'Cronómetro: ' + m[1].toLowerCase()]];
async function cargarHistorial() {
  try {
    const h = await (await fetch('/api/historial', {cache: 'no-store'})).json();
    if (!h.charlas.length) { $('historial').innerHTML = '<div class="card aviso">Todavía no le hablaste. Mantené BOOT apretado y hablá.</div>'; return; }
    $('historial').innerHTML = h.charlas.map(c => {
      const d = new Date(c.cuando);
      const hizo = (c.acciones || '').split(';').map(s => s.trim()).filter(Boolean).map(a => {
        for (const [re, f] of ACCIONES) { const m = a.match(re); if (m) return f(m); }
        return a;
      });
      return `<div class="card charla"><div class="meta"><span>${nombreDia(d)}</span><span>${hhmm(d)}</span></div>
        <div class="q">“${esc(c.pregunta)}”</div><div class="r">${esc(c.respuesta)}</div>
        ${hizo.map(x => `<span class="hizo">${esc(x)}</span>`).join('')}</div>`;
    }).join('');
  } catch (err) {}
}

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
cargarHistorial();
cargarAgenda().then(cargarPlanilla);   // de a uno: el reloj hace un pedido a n8n por vez
setInterval(() => { if (!document.hidden) cargarPlanilla(); }, 5 * 60000);
setInterval(() => { if (Date.now() - editando > 3000 && !document.hidden) cargar(); }, 10000);
setInterval(() => { if (!document.hidden) cargarHistorial(); }, 30000);
setInterval(() => { if (!document.hidden) cargarAgenda(); }, 5 * 60000);
document.addEventListener('visibilitychange', () => { if (!document.hidden) { cargar(); cargarHistorial(); } });
</script>
</body>
</html>
)rawliteral";

static const char WEB_MANIFEST[] = R"rawliteral({"name":"JARVIS Reloj","short_name":"JARVIS","start_url":"/","display":"standalone","background_color":"#000000","theme_color":"#000000","icons":[{"src":"/icono.png","sizes":"180x180","type":"image/png"}]})rawliteral";
