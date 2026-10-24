// Servidor de voz de JARVIS: mantiene Piper (voz Sharvard, hombre) siempre cargado, así cada
// respuesta sale en décimas de segundo (cargar la voz de cero tarda ~1,5 s).
// Escucha solo en la compu: POST http://127.0.0.1:5690/voz con el texto (UTF-8) en el cuerpo
// -> PCM mono de 16 bits a 24 kHz (lo que reproduce el reloj; Piper da 22 050 Hz).
// Lo prende iniciar-n8n.bat; lo usa voz-edge/voz.mjs (si no está, ese script usa Piper "en
// frío" o la voz de Windows).
// Piper y la voz están en n8n-flujos/voz-piper/ (no se suben: ver CLAUDE.md para bajarlos).
import http from 'node:http';
import { spawn } from 'node:child_process';
import { readFileSync, unlinkSync, mkdirSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import path from 'node:path';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const DIR = path.join(HERE, 'voz-piper');
const MODEL = path.join(DIR, 'es_ES-sharvard-medium.onnx');
const SPEAKER = 0;                       // Sharvard: 0 = hombre (el elegido), 1 = mujer
const PORT = 5690;
const OUT_DIR = 'D:\\temp\\piper';
mkdirSync(OUT_DIR, { recursive: true });

let piper = null;
let pending = [];                        // pedidos esperando su archivo, en orden
let counter = 0;

function startPiper() {
    piper = spawn(path.join(DIR, 'piper', 'piper.exe'),
                  ['--model', MODEL, '--json-input', '--speaker', String(SPEAKER), '--quiet'],
                  { cwd: path.join(DIR, 'piper') });
    let buf = '';
    piper.stdout.on('data', (d) => {
        buf += d.toString();
        let i;
        while ((i = buf.indexOf('\n')) >= 0) {           // Piper escribe la ruta de cada WAV listo
            const line = buf.slice(0, i).trim();
            buf = buf.slice(i + 1);
            if (line && pending.length) pending.shift().done(line);
        }
    });
    piper.on('exit', () => {                              // si se cae, los pedidos fallan y se reinicia
        for (const p of pending) p.fail(new Error('Piper se cerro'));
        pending = [];
        setTimeout(startPiper, 1000);
    });
}

// WAV de Piper (22 050 Hz) -> PCM 24 kHz con interpolación lineal.
function wavTo24k(file) {
    const wav = readFileSync(file);
    const dataAt = wav.indexOf('data') + 8;
    const rate = wav.readUInt32LE(24);
    const n = (wav.length - dataAt) >> 1;
    const outN = Math.floor(n * 24000 / rate);
    const out = Buffer.alloc(outN * 2);
    for (let i = 0; i < outN; i++) {
        const pos = i * rate / 24000, j = Math.floor(pos), f = pos - j;
        const a = wav.readInt16LE(dataAt + j * 2);
        const b = j + 1 < n ? wav.readInt16LE(dataAt + (j + 1) * 2) : a;
        out.writeInt16LE(Math.round(a + (b - a) * f), i * 2);
    }
    return out;
}

function synth(text) {
    return new Promise((resolve, reject) => {
        const file = path.join(OUT_DIR, `voz-${Date.now()}-${counter++}.wav`);
        const timer = setTimeout(() => reject(new Error('Piper tardo demasiado')), 15000);
        pending.push({
            done: () => { clearTimeout(timer); try { resolve(wavTo24k(file)); } catch (e) { reject(e); } finally { try { unlinkSync(file); } catch {} } },
            fail: (e) => { clearTimeout(timer); reject(e); },
        });
        piper.stdin.write(JSON.stringify({ text, output_file: file }) + '\n');
    });
}

startPiper();
http.createServer((req, res) => {
    if (req.method !== 'POST' || req.url !== '/voz') { res.writeHead(404).end(); return; }
    let body = '';
    req.setEncoding('utf8');
    req.on('data', (c) => { body += c; });
    req.on('end', async () => {
        try {
            const pcm = await synth(body.replace(/\s+/g, ' ').trim() || 'Listo.');
            res.writeHead(200, { 'Content-Type': 'application/octet-stream' }).end(pcm);
        } catch (e) {
            res.writeHead(500).end(String(e.message));
        }
    });
}).listen(PORT, '127.0.0.1', () => console.log(`Voz de JARVIS (Piper, Sharvard) lista en http://127.0.0.1:${PORT}/voz`));
