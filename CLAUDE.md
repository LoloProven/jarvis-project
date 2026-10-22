# Proyecto JARVIS
Reloj con asistente de IA sobre Waveshare ESP32-S3-Touch-AMOLED-2.06.
- Pantalla CO5300 (410x502, QSPI), touch FT3168, IMU QMI8658, audio ES8311, PMU AXP2101, RTC PCF85063.
- Botones: PWR (va al AXP2101) y BOOT (GPIO 0).
- Flash 32MB, PSRAM OPI 8MB. Framework Arduino, plataforma pioarduino (core 3.3.12).
- Los ejemplos de referencia que YA funcionan en el hardware real están en referencia-waveshare/.
- Pines: include/pin_config.h y include/config.h. Secretos: include/secrets.h (nunca subirlo ni mostrarlo;
  para saber si existe una clave, contarla con grep sin imprimir el valor).
- Backend: n8n (carpeta n8n-servidor).
- Respondé siempre en español rioplatense, claro y sin jerga innecesaria (también los avisos cortos).

## Estado (todo probado en el reloj)
- Pantalla con paleta pastel (include/palette.h), encabezado con nombre, "Conectado/Sin conexion" y hora.
- Pestañas: AGENDA (alarmas diarias, guardadas en NVS), CRONO (selector CRONOMETRO | TEMPORIZADOR,
  + pasos, km y velocidad), +ALARMA, MUSICA (control de Spotify por Bluetooth + nombre de la canción).
  Barra inferior: batería y Salir.
- Menú de perfiles: botón AJUSTES (brillo, volumen de la alarma, configurar WiFi; guardados en NVS).
- Alarma y temporizador suenan con DisplayUI::ringScreen(); la alarma permite POSPONER 5 min.
- Levantar la muñeca con la pantalla apagada (no en modo dormir): hora grande en blanco 5 s
  ("vistazo": solo el panel, sin WiFi ni CPU a 240 MHz). Detección en la tarea de pasos.
- Hora argentina por NTP, guardada en el RTC. WiFi múltiple: casa, portal y hotspot del iPhone.
- Pasos por software con el acelerómetro (el podómetro del chip no cuenta). El acelerómetro guarda
  las mediciones en su FIFO (128 a 62,5 Hz) y se leen de a tandas: cada 200 ms despierto y en cada
  despertar del modo dormir (1 s), así se cuentan pasos también durmiendo.
- Distancia: cada paso suma un largo según el ritmo (0,74 cm por paso/min, 55-140 cm), calibrado
  con Strava (8056 pasos, 7,67 km, 63 min). La barra de abajo muestra pasos/km del día (vuelven a 0
  a la medianoche); en CRONO hay una "Sesion" atada al cronómetro (INICIAR/PAUSA/RESET).
- No entra solo en modo dormir si hubo pasos en los últimos 3 min o corre el crono/temporizador.
- Energía: PWR corto = pantalla; PWR 1,5 s o 30 min apagada = modo dormir (sueño liviano, apaga
  WiFi y Bluetooth, las alarmas suenan); PWR 4 s = apagado total por hardware.
- Audio por el códec ES8311 (alarma). La grabación de voz (AudioManager::recordAndSend) existe pero
  no se probó con n8n.
- Pendiente: BATERÍA (el usuario dice que se descarga demasiado rápido; llegó a 0 % en una salida
  de 1 h). Plan: medir el % por hora en cada estado (pantalla prendida, apagada, modo dormir) y
  recortar: Bluetooth solo cuando se usa MÚSICA, dormir antes cuando no caminás, pantalla 15 s,
  brillo por defecto más bajo, WiFi solo cuando hace falta. También: verificar los pasos con otra
  salida de Strava (caminando, usando la "Sesion" del CRONO); agente de IA.
- Pendiente: NOTIFICACIONES del iPhone (WhatsApp, mensajes, llamadas) con ANCS. Diseño pensado:
  - Mismo esquema que AMS en media_remote.cpp: tras la vinculación cifrada, una tarea busca el
    servicio ANCS (7905F431-B5CE-4E99-A40F-4B1E122D00D0) por la misma conexión y se suscribe a
    Notification Source (9FBF120D-6301-42D9-8C58-25E699A21DBD) y Data Source
    (22EAC6E9-24D6-4BB5-BE44-B36ACE7C7BFB); los datos se piden escribiendo en Control Point
    (69D1D8F3-45E1-49A8-9821-9BBDFDAAD9D9): app, título y mensaje. Guía: ejemplo ANCS de NimBLE-Arduino
    (.pio/libdeps/.../NimBLE-Arduino/examples/ANCS).
  - El iPhone pide permiso: en Ajustes > Bluetooth > JARVIS Reloj activar "Compartir notificaciones".
  - Pantalla: aviso arriba con app + título + mensaje (texto tamaño 2, sin tildes, cortado) y pitido
    corto (el reloj no vibra); se cierra tocando o a los ~8 s. Llamada entrante: nombre + botones
    RECHAZAR (acción negativa de ANCS) y cerrar. Opcional: lista de las últimas notificaciones.
  - Respetar: no despertar del modo dormir salvo llamadas; el Bluetooth en modo dormir está apagado
    (las notificaciones llegan al despertar).
  Idea acordada para el agente: unir AGENDA y +ALARMA en "ALARMAS" y usar la pestaña libre para
  "JARVIS" (hablar, respuesta, próximos eventos de Calendar); mantener BOOT = hablar desde cualquier
  pantalla. El reloj graba y muestra; la IA y el acceso a Calendar van en n8n.

## Compilar y subir (Windows)
- PlatformIO: C:\Users\Admin\.platformio\penv\Scripts\platformio.exe (C:\Users\Admin\.platformio es un
  enlace a D:\.platformio). C: tiene poco espacio: en cada comando definir
  $env:TEMP="D:\temp"; $env:TMP="D:\temp"; $env:PIP_CACHE_DIR="D:\pip-cache"
- Compilar: platformio.exe run    Subir: platformio.exe run -t upload --upload-port COM3
- El reloj tiene que estar despierto para subir (en modo dormir desaparece el puerto USB). Cada subida
  lo reinicia y corta el Bluetooth. Si falla con "No serial data received" o no aparece el puerto:
  desenchufar, mantener BOOT, enchufar, soltar BOOT y volver a subir. El puerto puede no ser COM3:
  buscar el que tenga VID 303A en "platformio.exe device list".
- No correr dos compilaciones a la vez (comparten .pio y se pisan). Cambiar build flags recompila todo (~9 min).
- Flags útiles por variable de entorno (no tocar platformio.ini):
  PLATFORMIO_BUILD_FLAGS="-DTEST_ONLY_HOTSPOT" (solo hotspot + diagnóstico WiFi por USB),
  PLATFORMIO_BUILD_FLAGS="-DCORE_DEBUG_LEVEL=3" (logs internos, p. ej. de Bluetooth).
- Leer el USB: System.IO.Ports.SerialPort COM3 115200 con DtrEnable/RtsEnable en false.
- Cambios: rama nueva, un commit por tema compilando antes, probar en el reloj, PR desde el navegador
  del usuario (gh no está instalado), merge con "Create a merge commit".

## Trampas conocidas
- I2C compartido (touch, AXP2101, RTC, QMI8658, ES8311) entre el loop y la tarea de pasos: todo acceso
  en marcha va dentro de I2CLock (include/i2c_lock.h). Sin eso el AXP2101 leía toques fantasma de PWR
  y la pantalla se prendía y apagaba sola.
- Touch: leer el FT3168 solo cuando avisa por TP_INT (GPIO 38); si se le pregunta siempre, el log se
  llena de errores I2C.
- Apagar la pantalla = displayOff() del CO5300. Nunca cortar DC1 del AXP2101 (reinicia el reloj).
  Las tensiones del AXP2101 se dejan de fábrica (ALDO4 es 1,8 V).
- Esquinas redondeadas: tapan ~50 px a cada lado arriba y abajo; usar SAFE_MARGIN_X/TOP (config.h).
  Texto tamaño 1 es ilegible en esta pantalla: mínimo tamaño 2. La letra no tiene tildes ni ñ.
- Redibujar solo lo que cambia (texto con fondo, sin borrar antes) o la pantalla parpadea.
- Bluetooth: usar NimBLE-Arduino (h2zero), no la librería BLE del core: con NimBLE descarta la
  segunda característica HID 0x2A4D (handle 65535 y cuelgue al notificar). iOS necesita teclado +
  multimedia y una pausa de ~30 ms entre apretar y soltar la tecla. Al cambiar el descriptor HID hay
  que "Olvidar este dispositivo" en el iPhone y volver a vincular.
- Con Bluetooth prendido el chip no puede entrar en sueño liviano (se traba): el modo dormir hace
  MediaRemote::stop() y al despertar MediaRemote::init().
- SensorLib 0.2.x usa init() en vez de begin() (los ejemplos de Waveshare usan una versión más nueva).
- El hotspot del iPhone: nombre exacto con mayúsculas ("iPhoneLolo"), "Maximizar compatibilidad"
  activado y la pantalla de Compartir Internet abierta para que se vea.
