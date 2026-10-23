# Voz de respaldo para JARVIS cuando se acaba el uso gratis de Gemini: la voz en español de
# Windows. Recibe el texto en base64 (así no hay problemas con comillas ni tildes) y escribe
# por la salida el audio en base64: PCM mono de 16 bits a 24 kHz, lo mismo que Gemini.
param([string]$TextoBase64)

Add-Type -AssemblyName System.Speech
$texto = [Text.Encoding]::UTF8.GetString([Convert]::FromBase64String($TextoBase64))
$voz = New-Object System.Speech.Synthesis.SpeechSynthesizer
$es = $voz.GetInstalledVoices() | Where-Object { $_.VoiceInfo.Culture.Name -like 'es*' } | Select-Object -First 1
if ($es) { $voz.SelectVoice($es.VoiceInfo.Name) }
$formato = New-Object System.Speech.AudioFormat.SpeechAudioFormatInfo 24000, `
    ([System.Speech.AudioFormat.AudioBitsPerSample]::Sixteen), ([System.Speech.AudioFormat.AudioChannel]::Mono)
$memoria = New-Object System.IO.MemoryStream
$voz.SetOutputToAudioStream($memoria, $formato)    # PCM crudo, sin encabezado WAV
$voz.Speak($texto)
$voz.Dispose()
[Console]::Out.Write([Convert]::ToBase64String($memoria.ToArray()))
