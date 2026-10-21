#ifndef PALETTE_H
#define PALETTE_H

#include <stdint.h>

// Paleta pastel del reloj, en RGB565 (formato de la pantalla).
// Conversión: ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
// Regla: sobre los colores pastel va PAL_TEXTO (oscuro); sobre el fondo
// negro o PAL_TARJETA va PAL_TEXTO_CLARO.

// --- BASE ---
static const uint16_t PAL_CELESTE  = 0xAEDD;  // #A8D8EA
static const uint16_t PAL_ROSA     = 0xF619;  // #F6C1CC
static const uint16_t PAL_MENTA    = 0xBF1A;  // #B8E0D2
static const uint16_t PAL_LILA     = 0xCDBB;  // #CDB4DB
static const uint16_t PAL_AMARILLO = 0xFF96;  // #FFF1B6
static const uint16_t PAL_CORAL    = 0xF450;  // #F28B82 (coral suave)

// --- ROLES ---
static const uint16_t PAL_FONDO       = 0x0000;        // negro: en AMOLED apaga el píxel y ahorra batería
static const uint16_t PAL_TARJETA     = 0x39E9;        // #3A3D4F, elementos inactivos sobre el fondo
static const uint16_t PAL_ACENTO      = PAL_CELESTE;   // pestaña activa, botones principales
static const uint16_t PAL_TEXTO       = 0x2968;        // #2B2D42, texto sobre pastel
static const uint16_t PAL_TEXTO_CLARO = 0xF79D;        // #F5F3EE, texto y bordes sobre negro
static const uint16_t PAL_OK          = PAL_MENTA;
static const uint16_t PAL_ALERTA      = PAL_CORAL;

#endif
