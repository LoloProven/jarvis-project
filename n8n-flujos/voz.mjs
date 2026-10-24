// Voz de JARVIS. Recibe el texto en base64 y escribe por la salida el audio en base64: PCM mono
// de 16 bits a 24 kHz, lo que reproduce el reloj. Lo usa el nodo "Voz" del flujo de n8n.
// Prueba en orden:
//   1. El servidor de voz (voz-servidor.mjs, lo prende iniciar-n8n.bat): Piper con la voz
//      Sharvard (hombre) ya cargada, ~0,3 s.
//   2. Piper "en frío" (carga la voz cada vez, ~2 s), si el servidor no está prendido.
//   3. La voz de Windows (voz-windows.ps1), si Piper no está instalado.
// (Antes: Tomás de Edge, por una vía no oficial que Microsoft cortó el 23/10/2026; Azure, la
// vía oficial, no aceptó la tarjeta.)
import { execFileSync } from 'node:child_process';
import { readFileSync, existsSync, unlinkSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import path from 'node:path';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const PIPER_DIR = path.join(HERE, 'voz-piper');
const textB64 = process.argv[2] || '';
const text = Buffer.from(textB64, 'base64').toString('utf8');

async function fromServer() {
    const res = await fetch('http://127.0.0.1:5690/voz', {
        method: 'POST', body: text, signal: AbortSignal.timeout(15000),
    });
    if (!res.ok) throw new Error(`servidor de voz: ${res.status}`);
    return Buffer.from(await res.arrayBuffer());
}

// Igual que el servidor, pero cargando Piper solo para esta frase.
function coldPiper() {
    const exe = path.join(PIPER_DIR, 'piper', 'piper.exe');
    if (!existsSync(exe)) throw new Error('Piper no esta instalado');
    const file = path.join('D:\\temp', `voz-frio-${Date.now()}.wav`);
    execFileSync(exe, ['--model', path.join(PIPER_DIR, 'es_ES-sharvard-medium.onnx'), '--speaker', '0',
                       '--quiet', '--output_file', file],
                 { input: text, cwd: path.dirname(exe), timeout: 20000 });
    const wav = readFileSync(file);
    unlinkSync(file);
    const dataAt = wav.indexOf('data') + 8, rate = wav.readUInt32LE(24), n = (wav.length - dataAt) >> 1;
    const outN = Math.floor(n * 24000 / rate), out = Buffer.alloc(outN * 2);
    for (let i = 0; i < outN; i++) {                       // 22 050 Hz -> 24 kHz
        const pos = i * rate / 24000, j = Math.floor(pos), f = pos - j;
        const a = wav.readInt16LE(dataAt + j * 2), b = j + 1 < n ? wav.readInt16LE(dataAt + (j + 1) * 2) : a;
        out.writeInt16LE(Math.round(a + (b - a) * f), i * 2);
    }
    return out;
}

async function speak() {
    try { return await fromServer(); }
    catch (e) { process.stderr.write(`Servidor de voz no disponible (${e.message}); Piper en frio\n`); }
    try { return coldPiper(); }
    catch (e) { process.stderr.write(`Piper fallo (${e.message}); uso la de Windows\n`); }
    return null;
}

const pcm = await speak();
if (pcm) {
    process.stdout.write(pcm.toString('base64'));
} else {
    const out = execFileSync('powershell.exe', ['-NoProfile', '-ExecutionPolicy', 'Bypass', '-File',
                             path.join(HERE, 'voz-windows.ps1'), textB64], { maxBuffer: 20 * 1024 * 1024 });
    process.stdout.write(out);
}
