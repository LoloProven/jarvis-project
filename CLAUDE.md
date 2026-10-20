# Proyecto JARVIS
Reloj con asistente de IA sobre Waveshare ESP32-S3-Touch-AMOLED-2.06.
- Pantalla CO5300 (410x502, QSPI), touch FT3168, IMU QMI8658, audio ES8311, PMU AXP2101.
- Flash 32MB, PSRAM OPI 8MB. Framework Arduino, plataforma pioarduino (core 3.x).
- Los ejemplos de referencia que YA funcionan en el hardware real están en referencia-waveshare/.
- Pines: pin_config.h. Secretos: include/secrets.h (nunca subirlo ni mostrarlo).
- Estado: pantalla, IMU y audio probados con los ejemplos; el proyecto propio todavía no compila.
- Backend: n8n (carpeta n8n-servidor).