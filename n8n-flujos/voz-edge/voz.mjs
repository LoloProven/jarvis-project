// Voz de JARVIS: Tomás (voz neural argentina de Microsoft Edge, es-AR-TomasNeural).
// Recibe el texto en base64 y escribe por la salida el audio en base64: PCM mono de 16 bits
// a 24 kHz, lo que reproduce el reloj. Edge solo entrega MP3, así que se decodifica acá.
// Usa el servicio de "Lectura en voz alta" de Edge por una vía no oficial: si falla
// (Microsoft lo cambia, no hay internet...), se usa la voz de Windows (voz-windows.ps1).
import { MsEdgeTTS, OUTPUT_FORMAT } from 'msedge-tts';
import { MPEGDecoder } from 'mpg123-decoder';
import { execFileSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import path from 'node:path';

const VOICE = 'es-AR-TomasNeural';
const textB64 = process.argv[2] || '';
const text = Buffer.from(textB64, 'base64').toString('utf8');

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

try {
    const pcm = await mp3ToPcm16(await edgeMp3(text));
    process.stdout.write(pcm.toString('base64'));
} catch (e) {
    process.stderr.write(`Voz de Edge fallo (${e.message}); uso la de Windows\n`);
    const ps1 = path.join(path.dirname(fileURLToPath(import.meta.url)), '..', 'voz-windows.ps1');
    const out = execFileSync('powershell.exe', ['-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', ps1, textB64],
                             { maxBuffer: 20 * 1024 * 1024 });
    process.stdout.write(out);
}
