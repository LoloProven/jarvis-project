# 🤖 JARVIS - Smartwatch AI Assistant (ESP32-S3 + n8n)

JARVIS is a wearable personal assistant, smartwatch-style, powered by an ESP32-S3 board. It was created with the idea of having a daily routine companion you can interact with by voice — from checking or scheduling your calendar to getting smart, instant answers.

The project is built around the idea of comfortable wearable hardware + intelligence always within reach: the board handles all the physical side (the AMOLED display, buttons, sensors, and audio recording), while memory and intelligence are delegated to an n8n server that receives requests over the internet or the phone's mobile data.

It's designed as a modular system: we're starting by tackling calendar organization, but the architecture is already set up to add new features over time (habit tracking, music, notes, nutrition, etc.).

##🛠️ Hardware Specifications

Device: Waveshare ESP32-S3 Touch AMOLED 2.06" (Smartwatch-style).
MCU: ESP32-S3R8 Dual-Core Xtensa LX7 at 240 MHz (Wi-Fi 2.4 GHz + BT 5 LE).
Memory: 512 KB SRAM, 384 KB ROM, 8 MB PSRAM, 32 MB Flash.
Display: 2.06" Capacitive Touch AMOLED (410 × 502 px, CO5300 + FT3168 controller).
Audio: Dual digital microphones, ES8311 codec with ES7210 echo cancellation and built-in speaker.
Sensors & PMIC: 6-axis IMU (QMI8658), RTC (PCF85063), AXP2101 power management with 3.7V Li-ion battery.

