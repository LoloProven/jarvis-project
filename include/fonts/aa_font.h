#pragma once
#include <stdint.h>

// Letra suavizada (16 tonos por píxel, 2 píxeles por byte), generada con tools/fontgen.py.
struct AAGlyph {
    uint32_t offset;    // dónde empieza en el bitmap
    uint8_t w, h;       // tamaño de la letra dibujada
    uint8_t adv;        // cuánto avanza el cursor
    int8_t xo;          // corrimiento horizontal desde el cursor
    uint8_t yo;         // cuánto baja desde el borde de arriba del renglón
};

struct AAFont {
    const uint8_t* bitmap;
    const AAGlyph* glyphs;
    uint16_t first, last;   // códigos Latin-1 que tiene
    uint8_t ascent;         // del borde de arriba del renglón a la línea de base
    uint8_t height;         // alto del renglón (con tildes y colas de g, j, p...)
};
