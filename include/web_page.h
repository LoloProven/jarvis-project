#pragma once
// Página que sirve el reloj (web_settings.cpp) en http://jarvis.local/.
// Estilo "B · Deportivo", igual que la pantalla: negro, blanco, gris y acento lima, letra
// Barlow Condensed y una barra de pestañas abajo como los botones del reloj (HOY, GIMNASIO,
// COMIDAS, ESTUDIO, RELOJ). Lee /api/estado cada 10 s; la planilla (/api/planilla) y la agenda
// (/api/agenda) las trae el reloj de n8n.

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
:root{--bg:#000;--card:#0b0b0b;--card2:#141414;--borde:#222;--gris:#9a9a9a;--gris2:#5f5f5f;--blanco:#fff;--acento:#c6f432;--acento-bajo:rgba(198,244,50,.12);--rojo:#ff5a4d;--naranja:#ffb020;--azul:#5ac8fa}
*{box-sizing:border-box;-webkit-tap-highlight-color:transparent}
html,body{margin:0;background:var(--bg);color:var(--blanco);font-family:'Barlow Condensed','Arial Narrow',system-ui,sans-serif;font-size:19px;-webkit-text-size-adjust:100%}
body{padding-bottom:calc(84px + env(safe-area-inset-bottom))}
/* Barra de arriba */
.barra{position:sticky;top:0;z-index:5;background:rgba(0,0,0,.88);backdrop-filter:blur(12px);-webkit-backdrop-filter:blur(12px);border-bottom:1px solid var(--borde);padding:calc(10px + env(safe-area-inset-top)) 16px 10px}
.barra .in{max-width:560px;margin:0 auto;display:flex;align-items:center;gap:10px}
.marca{font-size:26px;font-weight:800;letter-spacing:2px;display:flex;align-items:center;gap:8px}
.marca i{width:10px;height:10px;border-radius:50%;background:var(--acento);box-shadow:0 0 10px var(--acento)}
.marca i.off{background:var(--rojo);box-shadow:0 0 10px var(--rojo)}
.barra .hora{margin-left:auto;font-size:24px;font-weight:800;font-variant-numeric:tabular-nums}
.pila{display:flex;align-items:center;gap:4px;font-size:16px;font-weight:700;color:var(--gris)}
.pila b{display:inline-block;width:26px;height:13px;border:2px solid var(--gris2);border-radius:3px;position:relative;padding:1px}
.pila b span{display:block;height:100%;background:var(--acento);border-radius:1px}
main{max-width:560px;margin:0 auto;padding:6px 16px 24px}
section{display:none;animation:entra .25s ease}
section.activa{display:block}
@keyframes entra{from{opacity:0;transform:translateY(6px)}to{opacity:1;transform:none}}
/* Títulos estilo reloj */
.titulo{margin:18px 0 14px}
.titulo .chico{font-size:15px;font-weight:700;letter-spacing:3px;color:var(--acento)}
.titulo h1{font-size:46px;font-weight:800;margin:0;line-height:1;letter-spacing:1px}
h2{font-size:14px;font-weight:700;letter-spacing:3px;color:var(--gris);margin:26px 0 10px;text-transform:uppercase;display:flex;align-items:baseline;gap:8px}
h2 .sub{font-size:13px;letter-spacing:1px;color:var(--gris2);font-weight:600;margin-left:auto}
.card{border:1px solid var(--borde);border-radius:18px;padding:14px 16px;margin-bottom:10px;background:var(--card)}
.card.acento{border-color:rgba(198,244,50,.45);background:linear-gradient(160deg,var(--acento-bajo),var(--card) 60%)}
.toque{cursor:pointer}
.toque:active{transform:scale(.985)}
.fila{display:flex;align-items:center;justify-content:space-between;gap:12px}
.nombre{font-size:21px;font-weight:700}
.desc{color:var(--gris);font-size:16px;margin-top:2px;line-height:1.25}
.aviso{color:var(--gris);font-size:16px;line-height:1.35}
.etq{font-size:13px;font-weight:700;letter-spacing:2px;color:var(--gris)}
.lima{color:var(--acento)}.rojo{color:var(--rojo)}
.grande{font-size:40px;font-weight:800;line-height:1;font-variant-numeric:tabular-nums}
.mini-grid{display:grid;grid-template-columns:1fr 1fr;gap:10px;margin-bottom:10px}
.mini-grid .card{margin:0}
.flecha{color:var(--gris2);font-size:22px}
/* Barras de progreso */
.barra-p{height:10px;border-radius:6px;background:var(--card2);overflow:hidden;margin-top:8px}
.barra-p span{display:block;height:100%;border-radius:6px;background:var(--acento);transition:width .5s}
.barra-p.pasada span{background:var(--rojo)}
/* Anillo */
.anillo{position:relative;width:150px;height:150px;flex:none}
.anillo svg{transform:rotate(-90deg)}
.anillo .c{position:absolute;inset:0;display:flex;flex-direction:column;align-items:center;justify-content:center;text-align:center}
.anillo .c b{font-size:38px;font-weight:800;line-height:1;font-variant-numeric:tabular-nums}
.anillo .c span{font-size:14px;color:var(--gris);letter-spacing:1px}
.macros{flex:1;display:flex;flex-direction:column;gap:12px}
.macro .fila{font-size:16px}
.macro b{font-size:20px;font-variant-numeric:tabular-nums}
/* Gimnasio */
.ej{padding:12px 0;border-bottom:1px solid var(--borde)}
.ej:last-child{border-bottom:0;padding-bottom:2px}
.ej:first-child{padding-top:2px}
.ej .prox{font-size:24px;font-weight:800;color:var(--acento);font-variant-numeric:tabular-nums}
.ej .datos{display:flex;gap:14px;flex-wrap:wrap;font-size:15px;color:var(--gris);margin-top:4px}
.ej .datos b{color:var(--blanco)}
.spark{display:block}
.prog{display:flex;align-items:center;gap:12px;padding:10px 0;border-bottom:1px solid var(--borde)}
.prog:last-child{border-bottom:0}
.prog .n{flex:1;min-width:0}
.prog .n div:first-child{font-size:19px;font-weight:700;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.delta{font-size:15px;font-weight:700;color:var(--acento)}
/* Comidas */
.comida{display:flex;gap:12px;align-items:center;padding:9px 0;border-bottom:1px solid var(--borde)}
.comida:last-child{border-bottom:0}
.comida .h{font-size:15px;color:var(--gris2);min-width:42px;font-variant-numeric:tabular-nums}
.comida .t{flex:1;font-size:18px;font-weight:600;line-height:1.2}
.comida .t small{display:block;font-size:14px;color:var(--gris);font-weight:500}
.comida .k{font-size:20px;font-weight:800;font-variant-numeric:tabular-nums}
/* Agenda y exámenes */
.dia{font-size:14px;font-weight:700;letter-spacing:2px;color:var(--acento);margin:14px 0 6px}
.evento{display:flex;gap:12px;align-items:baseline;padding:4px 0}
.evento .hs{font-size:20px;font-weight:800;min-width:56px;font-variant-numeric:tabular-nums}
.evento .t{font-size:19px;font-weight:600;flex:1}
.jv{display:inline-block;background:var(--acento);color:#000;font-size:12px;font-weight:800;letter-spacing:1px;border-radius:5px;padding:0 6px;margin-left:6px;vertical-align:middle}
.examen{display:flex;gap:14px;align-items:center;padding:10px 0;border-bottom:1px solid var(--borde)}
.examen:last-child{border-bottom:0}
.examen .n{min-width:58px;text-align:center;border-radius:14px;background:var(--card2);padding:6px 4px}
.examen .n b{display:block;font-size:30px;font-weight:800;line-height:1;font-variant-numeric:tabular-nums}
.examen .n small{font-size:11px;color:var(--gris);letter-spacing:1px}
.examen .n.cerca{background:rgba(255,90,77,.15)}.examen .n.cerca b{color:var(--rojo)}
/* JARVIS */
.charla .q{color:var(--gris);font-size:17px}
.charla .r{font-size:19px;margin-top:4px;line-height:1.25}
.charla .meta{display:flex;justify-content:space-between;color:var(--gris2);font-size:14px;letter-spacing:1px;margin-bottom:4px}
.hizo{display:inline-block;border:1px solid var(--acento);color:var(--acento);border-radius:6px;font-size:14px;font-weight:700;padding:0 6px;margin:6px 6px 0 0}
/* Ajustes */
.tag{display:inline-block;font-size:12px;font-weight:700;letter-spacing:1px;border-radius:6px;padding:1px 7px;margin-left:6px;vertical-align:middle}
.alto{background:var(--rojo);color:#000}.medio{background:var(--naranja);color:#000}.bajo{background:var(--borde);color:var(--gris)}
.sw{position:relative;width:56px;height:32px;flex:none}
.sw input{opacity:0;width:0;height:0}
.sw span{position:absolute;inset:0;background:var(--borde);border-radius:16px;transition:.2s}
.sw span:before{content:"";position:absolute;width:24px;height:24px;left:4px;top:4px;background:var(--gris);border-radius:50%;transition:.2s}
.sw input:checked+span{background:var(--acento)}
.sw input:checked+span:before{transform:translateX(24px);background:#000}
.consejo{display:flex;gap:10px;align-items:flex-start;padding:8px 0;border-bottom:1px solid var(--borde);line-height:1.25}
.consejo:last-child{border-bottom:0}
.punto{width:10px;height:10px;border-radius:50%;flex:none;margin-top:7px}
input[type=range]{width:100%;accent-color:var(--acento);height:30px}
select,input[type=number],input[type=time]{background:#000;color:var(--blanco);border:1px solid var(--borde);border-radius:10px;font:inherit;font-size:20px;padding:6px 10px}
.valor{font-size:22px;font-weight:700;min-width:60px;text-align:right}
button{font:inherit;font-weight:700;font-size:19px;border-radius:12px;padding:8px 16px;border:1px solid var(--borde);background:#000;color:var(--blanco);cursor:pointer}
button.lleno{background:var(--acento);border-color:var(--acento);color:#000}
.alarma{display:flex;align-items:center;gap:12px}
.alarma .h{font-size:34px;font-weight:800;flex:1;font-variant-numeric:tabular-nums}
.alarma.off .h{color:var(--gris2)}
.x{color:var(--rojo);font-size:20px;padding:4px 14px}
table{width:100%;border-collapse:collapse;font-size:16px;font-variant-numeric:tabular-nums}
th{color:var(--gris);font-weight:600;text-align:right;padding:4px 3px;border-bottom:1px solid var(--borde)}
td{text-align:right;padding:5px 3px;border-bottom:1px solid #161616}
th:first-child,td:first-child{text-align:left}
.gasto{display:grid;grid-template-columns:repeat(3,1fr);gap:8px}
.gasto div{border:1px solid var(--borde);border-radius:14px;padding:10px 6px;text-align:center;background:var(--card)}
.gasto b{display:block;font-size:26px}
.gasto span{color:var(--gris);font-size:13px;letter-spacing:1px}
#error{display:none;background:var(--rojo);color:#000;font-weight:700;border-radius:14px;padding:10px 14px;margin-top:12px}
/* Pestañas de abajo, como los botones del reloj */
nav{position:fixed;left:0;right:0;bottom:0;z-index:5;background:rgba(0,0,0,.92);backdrop-filter:blur(12px);-webkit-backdrop-filter:blur(12px);border-top:1px solid var(--borde);padding:6px 8px calc(6px + env(safe-area-inset-bottom))}
nav .in{max-width:560px;margin:0 auto;display:grid;grid-template-columns:repeat(5,1fr);gap:4px}
nav button{border:0;background:none;color:var(--gris2);display:flex;flex-direction:column;align-items:center;gap:2px;padding:6px 0;font-size:12px;letter-spacing:1px;border-radius:14px}
nav button svg{width:26px;height:26px}
nav button.activa{color:var(--acento);background:var(--acento-bajo)}
</style>
</head>
<body>
<div class="barra"><div class="in">
  <div class="marca"><i id="punto"></i>JARVIS</div>
  <div class="hora" id="hora">--:--</div>
  <div class="pila" id="pila"><b><span style="width:0"></span></b><em></em></div>
</div></div>

<main>
  <div id="error">No contesta el reloj. ¿Tiene la pantalla prendida y está en el mismo WiFi?</div>

  <!-- HOY -->
  <section id="s-hoy">
    <div class="titulo"><div class="chico" id="hoy-fecha">HOY</div><h1 id="hoy-saludo">Hola</h1></div>
    <div class="card">
      <div class="fila"><div><div class="etq">PASOS</div><div class="grande" id="hoy-pasos">0</div></div>
        <div style="text-align:right"><div class="etq">DISTANCIA</div><div class="nombre" id="hoy-km">0,0 km</div></div></div>
      <div class="barra-p"><span id="hoy-pasos-b" style="width:0"></span></div>
      <div class="desc" id="hoy-meta"></div>
    </div>
    <div class="mini-grid">
      <div class="card toque" data-ir="comidas"><div class="etq">KCAL HOY</div><div class="grande" id="hoy-kcal">–</div><div class="desc" id="hoy-kcal-d">de –</div></div>
      <div class="card toque" data-ir="gym"><div class="etq">HOY TOCA</div><div class="grande lima" id="hoy-toca">–</div><div class="desc" id="hoy-toca-d"></div></div>
    </div>
    <div class="mini-grid">
      <div class="card"><div class="etq">CLIMA</div><div class="nombre" id="hoy-clima">–</div></div>
      <div class="card"><div class="etq">PRÓXIMA ALARMA</div><div class="nombre" id="hoy-alarma">–</div></div>
    </div>
    <div class="card toque" data-ir="comidas">
      <div class="fila"><div class="etq">AGUA</div><div class="nombre"><span id="hoy-agua">–</span> <span class="desc" id="hoy-agua-d"></span></div></div>
      <div class="barra-p"><span id="hoy-agua-b" style="width:0;background:var(--azul)"></span></div>
    </div>
    <div class="card toque" data-ir="estudio" id="hoy-examen" style="display:none"></div>
    <h2>Agenda de hoy <span class="sub" id="agenda-cuando"></span></h2>
    <div id="hoy-agenda"><div class="card aviso">Cargando...</div></div>
    <h2>Lo que hizo JARVIS</h2>
    <div id="historial"></div>
  </section>

  <!-- GIMNASIO -->
  <section id="s-gym">
    <div class="titulo"><div class="chico">GIMNASIO · SOBRECARGA PROGRESIVA</div><h1 id="gym-dia">HOY TOCA</h1></div>
    <div id="gym-hoy"><div class="card aviso">Cargando la planilla...</div></div>
    <h2>Tus entrenamientos <span class="sub" id="planilla-cuando"></span></h2>
    <div id="gym-prog"></div>
    <div class="card aviso">Cada peso que le decís a JARVIS (“hice jalón al pecho 55 kilos, 4 series de 10”) queda guardado para siempre en tu planilla de Google, pestaña <b>Registro</b>. Con eso calcula con cuánto tirar la próxima: si en todas las series llegaste al tope de repeticiones, subís el peso; si no, mismo peso y una repetición más.</div>
  </section>

  <!-- COMIDAS -->
  <section id="s-comidas">
    <div class="titulo"><div class="chico">DEFINICIÓN</div><h1>TUS COMIDAS HOY</h1></div>
    <div class="card acento" id="com-resumen"><div class="aviso">Cargando...</div></div>
    <h2>Agua</h2>
    <div class="card" id="com-agua"><div class="aviso">Cargando...</div></div>
    <h2>Lo que comiste</h2>
    <div id="com-lista"></div>
    <h2>Tu objetivo</h2>
    <div class="card aviso" id="com-obj"></div>
  </section>

  <!-- ESTUDIO -->
  <section id="s-estudio">
    <div class="titulo"><div class="chico">FINALES Y PARCIALES</div><h1>ESTUDIO</h1></div>
    <div id="examenes"><div class="card aviso">Cargando...</div></div>
    <h2>Agenda · 2 semanas</h2>
    <div id="agenda"></div>
    <div class="card aviso">Para estudiar: “JARVIS, tomame examen de Inteligencia Artificial”. Para cargar temas o fechas, decíselos y los anota en la planilla.</div>
  </section>

  <!-- RELOJ -->
  <section id="s-reloj">
    <div class="titulo"><div class="chico">AJUSTES</div><h1>RELOJ</h1></div>
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
    <div class="card aviso">En el iPhone, tocá <b>Compartir</b> y después <b>Agregar a inicio</b>: queda un ícono de JARVIS para abrir esta página de un toque (el reloj tiene que estar en el mismo WiFi o en el hotspot del celu).</div>
  </section>
</main>

<nav><div class="in">
  <button data-tab="hoy"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><circle cx="12" cy="12" r="9"/><path d="M12 7v5l3 2"/></svg>HOY</button>
  <button data-tab="gym"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M6 7v10M18 7v10M3 10v4M21 10v4M6 12h12"/></svg>GIMNASIO</button>
  <button data-tab="comidas"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M7 3v8a2 2 0 0 0 2 2v8M5 3v6M9 3v6M17 21V3c-2 0-3 3-3 7h3"/></svg>COMIDAS</button>
  <button data-tab="estudio"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M4 5a2 2 0 0 1 2-2h13v16H6a2 2 0 0 0-2 2z"/><path d="M4 19V5M9 7h6"/></svg>ESTUDIO</button>
  <button data-tab="reloj"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><rect x="6" y="6" width="12" height="12" rx="3"/><path d="M9 6l1-3h4l1 3M9 18l1 3h4l1-3"/></svg>RELOJ</button>
</div></nav>

<script>
let est = null, plan = null, agendaDatos = null, editando = 0, planCuando = 0, planPidiendo = false;
const $ = id => document.getElementById(id);
const esc = s => String(s ?? '').replace(/[&<>"]/g, c => ({'&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;'}[c]));
const DIAS = ['DOMINGO', 'LUNES', 'MARTES', 'MIÉRCOLES', 'JUEVES', 'VIERNES', 'SÁBADO'];
const MESES = ['ENE', 'FEB', 'MAR', 'ABR', 'MAY', 'JUN', 'JUL', 'AGO', 'SEP', 'OCT', 'NOV', 'DIC'];
const hhmm = d => String(d.getHours()).padStart(2, '0') + ':' + String(d.getMinutes()).padStart(2, '0');
const miles = n => Number(n || 0).toLocaleString('es-AR');
const dec = v => v.toLocaleString('es-AR', {minimumFractionDigits: 1, maximumFractionDigits: 1});
const kg = v => String(Math.round(v * 10) / 10).replace('.', ',');
function nombreDia(d) {
  const hoy = new Date(); hoy.setHours(0, 0, 0, 0);
  const x = new Date(d); x.setHours(0, 0, 0, 0);
  const dif = Math.round((x - hoy) / 864e5);
  const base = `${DIAS[x.getDay()]} ${x.getDate()} ${MESES[x.getMonth()]}`;
  return dif === 0 ? 'HOY · ' + base : dif === 1 ? 'MAÑANA · ' + base : base;
}
function fallo() { $('error').style.display = 'block'; }

// --- Pestañas ---
function mostrar(tab) {
  if (!$('s-' + tab)) tab = 'hoy';
  document.querySelectorAll('section').forEach(s => s.classList.toggle('activa', s.id === 's-' + tab));
  document.querySelectorAll('nav button').forEach(b => b.classList.toggle('activa', b.dataset.tab === tab));
  try { localStorage.setItem('pestana', tab); } catch (e) {}
  // Gimnasio, comidas y estudio: si lo que hay tiene más de 30 s, pedirlo de nuevo.
  if (['gym', 'comidas', 'estudio'].includes(tab) && planCuando &&Date.now() - planCuando > 30000) cargarPlanilla(true);
  window.scrollTo(0, 0);
}
document.querySelectorAll('nav button').forEach(b => b.onclick = () => mostrar(b.dataset.tab));
document.querySelectorAll('[data-ir]').forEach(c => c.onclick = () => mostrar(c.dataset.ir));
let inicial = 'hoy';
try { inicial = localStorage.getItem('pestana') || 'hoy'; } catch (e) {}
mostrar(inicial);

// --- Estado del reloj ---
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

function pintar() {
  const e = est, a = e.ajustes;
  $('hora').textContent = e.hora;
  $('punto').className = e.wifi.red ? '' : 'off';
  const pct = Math.max(0, e.bateria);
  $('pila').innerHTML = `<b><span style="width:${pct}%;background:${e.bateria >= 0 && e.bateria < 20 && !e.cargando ? 'var(--rojo)' : 'var(--acento)'}"></span></b>${e.bateria < 0 ? '--' : e.bateria + '%'}${e.cargando ? ' ⚡' : ''}`;

  // HOY
  const ahora = new Date();
  $('hoy-fecha').textContent = `${DIAS[ahora.getDay()]} ${ahora.getDate()} ${MESES[ahora.getMonth()]}`;
  const h = ahora.getHours();
  $('hoy-saludo').textContent = h < 6 ? 'BUENAS NOCHES' : h < 13 ? 'BUEN DÍA' : h < 20 ? 'BUENAS TARDES' : 'BUENAS NOCHES';
  $('hoy-pasos').textContent = miles(e.pasos);
  $('hoy-km').textContent = dec(e.km || 0) + ' km';
  $('hoy-pasos-b').style.width = Math.min(100, e.pasos / a.meta * 100) + '%';
  $('hoy-meta').textContent = e.pasos >= a.meta ? '¡Meta cumplida!' : `Faltan ${miles(a.meta - e.pasos)} para ${miles(a.meta)}`;
  $('hoy-clima').textContent = e.clima.ok ? `${e.clima.temp}° ${e.clima.desc}` : '–';
  const act = e.alarmas.filter(x => x.on).map(x => x.h * 60 + x.m).sort((p, q) => p - q);
  const min = h * 60 + ahora.getMinutes();
  const prox = act.find(x => x > min) ?? act[0];
  $('hoy-alarma').textContent = prox === undefined ? 'Ninguna' : String(Math.floor(prox / 60)).padStart(2, '0') + ':' + String(prox % 60).padStart(2, '0');

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
  $('consejos').innerHTML = '<div class="etq" style="margin-bottom:4px">CONSEJOS DE BATERÍA</div>' +
    c.map(([col, t]) => `<div class="consejo"><div class="punto" style="background:${col}"></div><div>${t}</div></div>`).join('');

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

let ultimaCharla = null;
async function cargar() {
  try {
    est = await (await fetch('/api/estado', {cache: 'no-store'})).json();
    $('error').style.display = 'none';
    pintar();
    // Habló con JARVIS: puede haber anotado una comida o un peso; traer todo al toque.
    if (ultimaCharla !== null && est.ultimaCharla !== ultimaCharla) { cargarHistorial(); cargarPlanilla(true); }
    ultimaCharla = est.ultimaCharla;
  } catch (e) { fallo(); }
}

// --- Lo que el reloj trae de n8n: si contesta "cargando", lo está pidiendo; volver a preguntar ---
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
const sinN8n = (que, err) => `<div class="card aviso">No pude traer ${que} (n8n ${err < 0 ? 'no responde' : 'respondió ' + err}). Tiene que estar la compu prendida con las 3 ventanas de JARVIS abiertas (ngrok, voz y n8n).</div>`;

// Gráfico chiquito de los pesos de un ejercicio.
function spark(v, w = 90, h = 30) {
  if (!v || v.length < 2) return '';
  const mn = Math.min(...v), mx = Math.max(...v), r = mx - mn || 1;
  const p = v.map((y, i) => `${(i / (v.length - 1) * (w - 6) + 3).toFixed(1)},${(h - 3 - (y - mn) / r * (h - 6)).toFixed(1)}`);
  const u = p[p.length - 1].split(',');
  return `<svg class="spark" width="${w}" height="${h}" viewBox="0 0 ${w} ${h}"><polyline points="${p.join(' ')}" fill="none" stroke="var(--acento)" stroke-width="2" stroke-linejoin="round" stroke-linecap="round"/><circle cx="${u[0]}" cy="${u[1]}" r="3" fill="var(--acento)"/></svg>`;
}

function anillo(valor, total) {
  const R = 64, L = 2 * Math.PI * R, f = Math.min(1, valor / total);
  const col = valor > total ? 'var(--rojo)' : 'var(--acento)';
  return `<div class="anillo"><svg width="150" height="150" viewBox="0 0 150 150"><circle cx="75" cy="75" r="${R}" fill="none" stroke="var(--card2)" stroke-width="12"/>
    <circle cx="75" cy="75" r="${R}" fill="none" stroke="${col}" stroke-width="12" stroke-linecap="round" stroke-dasharray="${(L * f).toFixed(1)} ${L.toFixed(1)}"/></svg>
    <div class="c"><b>${miles(valor)}</b><span>/ ${miles(total)} KCAL</span></div></div>`;
}

function macro(nombre, v, obj, col) {
  const pas = v > obj;
  return `<div class="macro"><div class="fila"><span class="etq">${nombre}</span><span><b>${Math.round(v)}</b> <span class="desc">/ ${obj} g</span></span></div>
    <div class="barra-p ${pas ? 'pasada' : ''}"><span style="width:${Math.min(100, v / obj * 100)}%;${pas ? '' : 'background:' + col}"></span></div></div>`;
}

function pintarPlanilla() {
  const p = plan;
  if (!p) return;
  if (p.error !== undefined) {
    $('gym-hoy').innerHTML = sinN8n('la planilla', p.error);
    $('com-resumen').innerHTML = '<div class="aviso">Sin datos de la planilla.</div>';
    return;
  }
  $('planilla-cuando').textContent = 'actualizada ' + hhmm(new Date());

  // Gimnasio
  const t = p.toca;
  $('gym-dia').innerHTML = t ? `HOY TOCA <span class="lima">${esc(t.dia).toUpperCase()}</span>` : 'GIMNASIO';
  $('hoy-toca').textContent = t ? t.dia.toUpperCase() : '–';
  $('hoy-toca-d').textContent = t ? t.grupo : '';
  if (t) {
    $('gym-hoy').innerHTML = `<div class="card acento"><div class="fila" style="margin-bottom:6px"><div class="etq">${esc(t.grupo).toUpperCase()}</div><div class="etq">${esc(t.series).toUpperCase()}</div></div>
      ${t.ejercicios.map(x => `<div class="ej"><div class="fila"><div class="nombre">${esc(x.nombre)}</div>${spark(x.historia)}</div>
        <div class="etq" style="margin-top:6px">PRÓXIMA</div><div class="prox" ${x.ultimo ? '' : 'style="font-size:18px;color:var(--gris)"'}>${esc(x.proxima)}</div>
        ${x.ultimo ? `<div class="datos"><span>Última <b>${kg(x.ultimo.peso)} kg · ${x.ultimo.series}x${esc(x.ultimo.reps)}</b> (${esc(x.ultimo.fecha.slice(0, 5))})</span><span>Récord <b>${kg(x.mejor)} kg</b></span></div>`
          : '<div class="datos">Todavía sin anotar</div>'}</div>`).join('')}
      ${t.extras ? `<div class="desc" style="margin-top:10px">+ ${esc(t.extras)}</div>` : ''}</div>
      ${p.ultimoDia ? `<div class="aviso" style="margin:0 4px 6px">Último entrenamiento: ${esc(p.ultimoDia.dia)} el ${esc(p.ultimoDia.fecha)}. Orden: Push → Pull → Piernas.</div>` : ''}`;
  }
  // Un cuadro por entrenamiento (el más nuevo arriba), como una planilla.
  const ses = p.sesiones || [];
  $('gym-prog').innerHTML = ses.length ? ses.map(s => {
    const [d, m, a] = s.fecha.split('/').map(Number);
    const f = new Date(a, m - 1, d), hace = Math.round((new Date().setHours(0, 0, 0, 0) - f) / 864e5);
    return `<div class="card"><div class="fila" style="margin-bottom:6px"><div><span class="nombre lima">${esc(s.dia || 'Entrenamiento').toUpperCase()}</span>
        <span class="etq" style="margin-left:6px">${DIAS[f.getDay()].slice(0, 3)} ${d} ${MESES[m - 1]}</span></div>
        <div class="etq">${hace === 0 ? 'HOY' : hace === 1 ? 'AYER' : 'HACE ' + hace + ' DÍAS'}</div></div>
      <table><thead><tr><th>Ejercicio</th><th>Kg</th><th>Series</th><th>Reps</th></tr></thead><tbody>
      ${s.ejercicios.map(x => `<tr><td style="font-size:17px">${esc(x.ejercicio)}${x.notas ? `<div class="desc" style="font-size:13px">${esc(x.notas)}</div>` : ''}</td>
        <td style="font-weight:700;white-space:nowrap">${kg(x.peso)}${x.cambio === 'sube' ? ' <span class="lima">▲</span>' : x.cambio === 'baja' ? ' <span class="rojo">▼</span>' : ''}</td>
        <td>${x.series || ''}</td><td>${esc(x.reps)}</td></tr>`).join('')}
      </tbody></table></div>`;
  }).join('') : '<div class="card aviso">Todavía no anotaste pesos. Al terminar cada ejercicio, decile a JARVIS cuánto hiciste (“press banca 70 kilos, 3 series de 8”).</div>';

  // Comidas
  const c = p.comidas, o = c.objetivo, tot = c.total;
  $('hoy-kcal').textContent = miles(tot.kcal);
  $('hoy-kcal-d').textContent = `de ${miles(o.kcal)} · faltan ${miles(Math.max(0, c.falta.kcal))}`;
  $('com-resumen').innerHTML = `<div class="fila" style="align-items:center;gap:18px">${anillo(tot.kcal, o.kcal)}
    <div class="macros">${macro('PROTEÍNA', tot.p, o.p, 'var(--acento)')}${macro('CARBOS', tot.c, o.c, 'var(--azul)')}${macro('GRASAS', tot.g, o.g, 'var(--naranja)')}</div></div>
    <div class="desc" style="margin-top:12px;text-align:center">${c.falta.kcal >= 0 ? `Te quedan <b style="color:var(--blanco)">${miles(c.falta.kcal)} kcal</b> y <b style="color:var(--blanco)">${Math.max(0, c.falta.p)} g de proteína</b> para hoy.` : `<span class="rojo">Te pasaste ${miles(-c.falta.kcal)} kcal del objetivo.</span>`}</div>`;
  // Agua: vasitos de 250 ml que se van llenando.
  const ag = p.agua || {hoy: [], total: 0, objetivo: 4000, falta: 4000};
  const lt = ml => (Math.round(ml / 100) / 10).toLocaleString('es-AR', {minimumFractionDigits: 1, maximumFractionDigits: 1});
  const vasos = Math.ceil(ag.objetivo / 250), llenos = Math.floor(ag.total / 250);
  $('hoy-agua').textContent = lt(ag.total) + ' L';
  $('hoy-agua-d').textContent = '/ ' + lt(ag.objetivo) + ' L';
  $('hoy-agua-b').style.width = Math.min(100, ag.total / ag.objetivo * 100) + '%';
  $('com-agua').innerHTML = `<div class="fila"><div><span class="grande" style="color:var(--azul)">${lt(ag.total)}</span> <span class="desc">/ ${lt(ag.objetivo)} litros</span></div>
      <div class="desc" style="text-align:right">${ag.falta > 0 ? `Faltan <b style="color:var(--blanco)">${lt(ag.falta)} L</b><br>≈ ${Math.ceil(ag.falta / 250)} vasos` : '<b class="lima">¡Objetivo cumplido!</b>'}</div></div>
    <div style="display:grid;grid-template-columns:repeat(8,1fr);gap:6px;margin-top:12px">${Array.from({length: vasos}, (_, i) =>
      `<svg viewBox="0 0 20 24" style="width:100%;height:28px"><path d="M3 2h14l-2 20H5z" fill="${i < llenos ? 'var(--azul)' : 'none'}" stroke="${i < llenos ? 'var(--azul)' : 'var(--gris2)'}" stroke-width="1.6" stroke-linejoin="round"/></svg>`).join('')}</div>
    ${ag.hoy.length ? `<div class="desc" style="margin-top:10px">${ag.hoy.map(x => `${esc(x.hora)} · ${miles(x.ml)} ml`).join(' &nbsp; ')}</div>` : '<div class="desc" style="margin-top:10px">Decile a JARVIS “tomé un vaso de agua” o “tomé una botella de medio litro”.</div>'}`;

  $('com-lista').innerHTML = c.hoy.length ? '<div class="card">' + c.hoy.map(x => `<div class="comida"><div class="h">${esc(x.hora)}</div>
      <div class="t">${esc(x.comida)}<small>P ${Math.round(x.p)} g · C ${Math.round(x.c)} g · G ${Math.round(x.g)} g</small></div><div class="k">${miles(Math.round(x.kcal))}</div></div>`).join('') + '</div>'
    : '<div class="card aviso">Todavía no anotaste nada hoy. Decile a JARVIS lo que comiste (“comí dos empanadas de carne y una manzana”) y estima las kcal y los macros.</div>';
  $('com-obj').innerHTML = `<b style="color:var(--blanco)">${miles(o.kcal)} kcal · ${o.p} g proteína · ${o.c} g carbos · ${o.g} g grasas</b><br>
    Calculado para definición con 20 años, 84 kg y 184 cm: mantenimiento ~2.940 kcal (gimnasio 4-5 veces por semana) menos 20 %. Proteína alta (2 g por kg) para no perder músculo. Agua: ${(p.agua ? p.agua.objetivo : 4000) / 1000} L por día (84 kg × 40 ml más lo que perdés entrenando; con tanta proteína conviene no quedarse corto). Lo podés cambiar en la pestaña <b>Objetivos</b> de la planilla.`;

  // Estudio
  const ex = p.materias;
  $('examenes').innerHTML = ex.length ? '<div class="card">' + ex.map(m => `<div class="examen"><div class="n ${m.faltan !== null && m.faltan <= 7 ? 'cerca' : ''}"><b>${m.faltan === null ? '?' : m.faltan}</b><small>${m.faltan === 1 ? 'DÍA' : 'DÍAS'}</small></div>
      <div><div class="nombre">${esc(m.materia)}</div><div class="desc">${esc(m.evaluacion)} · ${esc(m.fecha)}${m.notas ? ' · ' + esc(m.notas) : ''}</div></div></div>`).join('') + '</div>'
    : '<div class="card aviso">No hay exámenes anotados.</div>';
  if (ex.length) {
    const m = ex[0];
    $('hoy-examen').style.display = '';
    $('hoy-examen').innerHTML = `<div class="fila"><div><div class="etq">PRÓXIMO EXAMEN</div><div class="nombre">${esc(m.materia)}</div><div class="desc">${esc(m.evaluacion)} · ${esc(m.fecha)}</div></div>
      <div style="text-align:right"><div class="grande ${m.faltan !== null && m.faltan <= 7 ? 'rojo' : 'lima'}">${m.faltan ?? '?'}</div><div class="etq">${m.faltan === 1 ? 'DÍA' : 'DÍAS'}</div></div></div>`;
  }
}

async function cargarPlanilla(nueva) {
  if (planPidiendo) return;
  planPidiendo = true;
  try { plan = await desdeN8n('/api/planilla', nueva); } catch (e) { plan = {error: -1}; }
  planPidiendo = false;
  planCuando = Date.now();
  pintarPlanilla();
}

// Agenda de Google Calendar (hoy y las próximas 2 semanas), vía el reloj y n8n.
function listaAgenda(eventos) {
  let html = '', dia = '';
  for (const e of eventos) {
    const ini = e.todoElDia ? new Date(e.inicio + 'T00:00') : new Date(e.inicio);
    const nd = nombreDia(ini);
    if (nd !== dia) { if (dia) html += '</div>'; html += `<div class="dia">${nd}</div><div class="card">`; dia = nd; }
    html += `<div class="evento"><div class="hs">${e.todoElDia ? 'TODO' : hhmm(ini)}</div><div class="t">${esc(e.titulo)}${e.jarvis ? '<span class="jv">JARVIS</span>' : ''}</div></div>`;
  }
  return html + (dia ? '</div>' : '');
}
async function cargarAgenda(nueva) {
  let a;
  try { a = await desdeN8n('/api/agenda', nueva); } catch (e) { a = {error: -1}; }
  if (a.error !== undefined) {
    $('agenda').innerHTML = $('hoy-agenda').innerHTML = sinN8n('la agenda', a.error);
    return;
  }
  $('agenda-cuando').textContent = 'actualizada ' + hhmm(new Date());
  const hoy = new Date().toDateString();
  const deHoy = a.eventos.filter(e => new Date(e.todoElDia ? e.inicio + 'T00:00' : e.inicio).toDateString() === hoy);
  $('hoy-agenda').innerHTML = deHoy.length ? '<div class="card">' + listaAgenda(deHoy).replace(/^<div class="dia">.*?<\/div><div class="card">/, '') :
    '<div class="card aviso">Nada en el calendario para hoy.</div>';
  $('agenda').innerHTML = a.eventos.length ? listaAgenda(a.eventos) : '<div class="card aviso">Nada en las próximas 2 semanas.</div>';
}

// Últimas charlas con JARVIS (las guarda el reloj).
const ACCIONES = [[/^ALARMA (\d+:\d+)/, m => 'Alarma ' + m[1]], [/^BORRAR (\d+:\d+)/, m => 'Borró alarma ' + m[1]],
  [/^TIMER (\d+)/, m => 'Temporizador ' + Math.round(m[1] / 60) + ' min'], [/^CRONO (\w+)/, m => 'Cronómetro: ' + m[1].toLowerCase()]];
async function cargarHistorial() {
  try {
    const h = await (await fetch('/api/historial', {cache: 'no-store'})).json();
    if (!h.charlas.length) { $('historial').innerHTML = '<div class="card aviso">Todavía no le hablaste. Mantené BOOT apretado y hablá.</div>'; return; }
    $('historial').innerHTML = h.charlas.slice(0, 5).map(c => {
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
cargarPlanilla().then(() => cargarAgenda());   // de a uno: el reloj hace un pedido a n8n por vez
setInterval(() => { if (Date.now() - editando > 3000 && !document.hidden) cargar(); }, 10000);
setInterval(() => { if (!document.hidden) cargarHistorial(); }, 30000);
setInterval(() => { if (!document.hidden) cargarPlanilla().then(() => cargarAgenda()); }, 3 * 60000);
document.addEventListener('visibilitychange', () => {
  if (!document.hidden) { cargar(); cargarHistorial(); cargarPlanilla(true).then(() => cargarAgenda()); }
});
</script>
</body>
</html>
)rawliteral";

static const char WEB_MANIFEST[] = R"rawliteral({"name":"JARVIS Reloj","short_name":"JARVIS","start_url":"/","display":"standalone","background_color":"#000000","theme_color":"#000000","icons":[{"src":"/icono.png","sizes":"180x180","type":"image/png"}]})rawliteral";
