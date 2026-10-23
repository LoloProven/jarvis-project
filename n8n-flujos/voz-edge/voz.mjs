// Voz de JARVIS: Tomás (voz neural argentina de Microsoft, es-AR-TomasNeural).
// Recibe el texto en base64 y escribe por la salida el audio en base64: PCM mono de 16 bits
// a 24 kHz, lo que reproduce el reloj. Prueba en orden:
//   1. Azure Speech (vía oficial, plan gratis F0): clave y región en azure.env (no se sube).
//      Entrega el PCM directo, sin convertir.
//   2. El servicio de "Lectura en voz alta" de Edge (vía NO oficial; Microsoft lo cortó el
//      23/10/2026). Entrega MP3 y se decodifica acá.
//   3. La voz de Windows (voz-windows.ps1).
import { MsEdgeTTS, OUTPUT_FORMAT } from 'msedge-tts';
import { MPEGDecoder } from 'mpg123-decoder';
import { execFileSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import { readFileSync, existsSync } from 'node:fs';
import path from 'node:path';

const VOICE = 'es-AR-TomasNeural';
const HERE = path.dirname(fileURLToPath(import.meta.url));
const textB64 = process.argv[2] || '';
const text = Buffer.from(textB64, 'base64').toString('utf8');

function azureConfig() {
    const file = path.join(HERE, 'azure.env');
    if (!existsSync(file)) return null;
    const cfg = Object.fromEntries(readFileSync(file, 'utf8').split(/\r?\n/)
        .map((l) => l.split('=').map((s) => s.trim())).filter((kv) => kv.length === 2 && kv[1]));
    const key = cfg.AZURE_SPEECH_KEY, region = cfg.AZURE_SPEECH_REGION;
    return key && region && !key.includes('PEGAR') ? { key, region } : null;
}

const xmlEscape = (s) => s.replace(/[<>&'"]/g, (c) => ({ '<': '&lt;', '>': '&gt;', '&': '&amp;', "'": '&apos;', '"': '&quot;' }[c]));

async function azurePcm(t, { key, region }) {
    const ssml = `<speak version="1.0" xml:lang="es-AR"><voice name="${VOICE}">${xmlEscape(t)}</voice></speak>`;
    const res = await fetch(`https://${region}.tts.speech.microsoft.com/cognitiveservices/v1`, {
        method: 'POST',
        headers: {
            'Ocp-Apim-Subscription-Key': key,
            'Content-Type': 'application/ssml+xml',
            'X-Microsoft-OutputFormat': 'raw-24khz-16bit-mono-pcm',
            'User-Agent': 'jarvis-reloj',
        },
        body: ssml,
        signal: AbortSignal.timeout(15000),
    });
    if (!res.ok) throw new Error(`Azure respondio ${res.status}`);
    const pcm = Buffer.from(await res.arrayBuffer());
    if (pcm.length < 100) throw new Error('Azure devolvio audio vacio');
    return pcm;
}

async function edgeMp3(t) {
    const tts = new MsEdgeTTS();
    await tts.setMetadata(VOICE, OUTPUT_FORMAT.AUDIO_24KHZ_48KBITRATE_MONO_MP3);
    const { audioStream } = tts.toStream(t);
    const chunks = [];
    await new Promise((ok, fail) => {
        const timer = setTimeout(() => fail(new Error('Edge tardo demasiado')), 20000);
        audioStream.on('data', (c) => chunks.push(c));
        audioStream.on('end', () => { clearTimeout(timer); ok(); });
        audioStream.on('error', (e) => { clearTimeout(timer); fail(e); });
    });
    tts.close();
    return Buffer.concat(chunks);
}

async function mp3ToPcm16(mp3) {
    const decoder = new MPEGDecoder();
    await decoder.ready;
    const { channelData, samplesDecoded, sampleRate } = decoder.decode(new Uint8Array(mp3));
    decoder.free();
    if (sampleRate !== 24000 || samplesDecoded === 0) throw new Error(`audio inesperado (${sampleRate} Hz)`);
    const pcm = Buffer.alloc(samplesDecoded * 2);
    const mono = channelData[0];
    for (let i = 0; i < samplesDecoded; i++) {
        pcm.writeInt16LE(Math.max(-32768, Math.min(32767, Math.round(mono[i] * 32767))), i * 2);
    }
    return pcm;
}

async function speak() {
    const azure = azureConfig();
    if (azure) {
        try { return await azurePcm(text, azure); }
        catch (e) { process.stderr.write(`Voz de Azure fallo (${e.message}); pruebo Edge\n`); }
    }
    try { return await mp3ToPcm16(await edgeMp3(text)); }
    catch (e) { process.stderr.write(`Voz de Edge fallo (${e.message}); uso la de Windows\n`); }
    return null;
}

const pcm = await speak();
if (pcm) {
    process.stdout.write(pcm.toString('base64'));
} else {
    const ps1 = path.join(HERE, '..', 'voz-windows.ps1');
    const out = execFileSync('powershell.exe', ['-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', ps1, textB64],
                             { maxBuffer: 20 * 1024 * 1024 });
    process.stdout.write(out);
}
