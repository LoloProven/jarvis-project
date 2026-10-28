#ifndef PALETTE_H
#define PALETTE_H

#include <stdint.h>

// Paleta del reloj, estilo "B · Deportivo" (maqueta https://claude.ai/artifact/MQgD5vN2F1FXvBWUrtpyGj),
// en RGB565 (formato de la pantalla). Conversión: ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
// Regla: fondo negro, texto blanco, etiquetas en gris y un solo color de acento. Sobre el
// acento va texto negro.

static const uint16_t PAL_FONDO  = 0x0000;   // negro puro: en AMOLED apaga el píxel y ahorra batería
static const uint16_t PAL_BLANCO = 0xFFFF;   // texto principal
static const uint16_t PAL_GRIS   = 0xA534;   // #A6A6A6, etiquetas y textos secundarios
static const uint16_t PAL_BORDE  = 0x2124;   // #262626, bordes de tarjetas y botones, barras vacías
static const uint16_t PAL_ACENTO = 0xC7A6;   // #C6F432 lima (otros probados en la maqueta: naranja
                                             // #FF7A1A = 0xFBC3, celeste #33D1FF = 0x369F, rosa #FF4D8D = 0xFA71)
static const uint16_t PAL_ALERTA = 0xFAC9;   // #FF5A4D rojo: borrar, rechazar, hora no confiable

#endif
