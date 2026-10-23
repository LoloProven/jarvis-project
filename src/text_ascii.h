#ifndef TEXT_ASCII_H
#define TEXT_ASCII_H

#include <stdint.h>
#include <stddef.h>

// La letra del reloj no tiene tildes ni ñ: se pasan a la letra sin acento (UTF-8 -> ASCII).
// Emojis y otros caracteres raros quedan como '?'.
inline void toAscii(const uint8_t* in, size_t len, char* out, size_t outLen) {
    size_t o = 0;
    for (size_t i = 0; i < len && o + 1 < outLen; i++) {
        uint8_t c = in[i];
        if (c < 0x80) { out[o++] = (c >= 0x20) ? c : ' '; continue; }
        if (c == 0xC3 && i + 1 < len) {            // Latin-1: á é í ó ú ñ ü ç ...
            uint8_t d = in[++i];
            const char* map = "AAAAAAACEEEEIIIIDNOOOOOxOUUUUYPsaaaaaaaceeeeiiiidnooooo/ouuuuypy";
            out[o++] = (d >= 0x80 && d <= 0xBF) ? map[d - 0x80] : '?';
            continue;
        }
        // Otros caracteres (emojis, etc.): se saltean los bytes de continuación.
        out[o++] = '?';
        while (i + 1 < len && (in[i + 1] & 0xC0) == 0x80) i++;
    }
    out[o] = 0;
}

#endif
