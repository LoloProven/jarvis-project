#ifndef AA_TEXT_H
#define AA_TEXT_H

#include <Arduino_GFX_Library.h>
#include "fonts/aa_font.h"

// Letras de la interfaz (Barlow Condensed suavizada, con tildes y ñ). Tamaños en px.
extern const AAFont& FONT_S;      // 17, semi negrita: etiquetas
extern const AAFont& FONT_M;      // 21, semi negrita: textos y botones
extern const AAFont& FONT_L;      // 28, negrita: valores
extern const AAFont& FONT_XL;     // 40, negrita: títulos y horas de alarmas
extern const AAFont& FONT_XXL;    // 64, negrita: cronómetro
extern const AAFont& FONT_HUGE;   // 120, extra negrita, solo números: la hora

// Los textos van en UTF-8 (se pueden escribir con tildes en el código). Cada dibujo pinta
// el renglón entero con su fondo de una sola vez: se puede escribir encima del texto
// anterior sin borrar antes, así no parpadea. "y" es el borde de arriba del renglón
// (alto = font.height). "spacing" separa más las letras (las etiquetas en mayúsculas).
namespace AAText {
    enum Align { LEFT, CENTER, RIGHT };

    int width(const AAFont& f, const char* s, int spacing = 0);
    // Texto dentro de una caja de ancho w (el resto de la caja se pinta con el fondo).
    void drawBox(Arduino_GFX* g, const AAFont& f, int x, int y, int w, const char* s,
                 uint16_t fg, uint16_t bg, Align align = LEFT, int spacing = 0);
    // Texto del ancho justo; devuelve el ancho.
    int draw(Arduino_GFX* g, const AAFont& f, int x, int y, const char* s,
             uint16_t fg, uint16_t bg, int spacing = 0);
    int drawCentered(Arduino_GFX* g, const AAFont& f, int cx, int y, const char* s,
                     uint16_t fg, uint16_t bg, int spacing = 0);
    int drawRight(Arduino_GFX* g, const AAFont& f, int right, int y, const char* s,
                  uint16_t fg, uint16_t bg, int spacing = 0);
    // Varios renglones de hasta maxW px, cortando en los espacios; si no entra en
    // maxLines, el último termina en "...". Devuelve la y siguiente.
    int drawWrapped(Arduino_GFX* g, const AAFont& f, int x, int y, int maxW, int maxLines,
                    const char* s, uint16_t fg, uint16_t bg, int lineGap = 2);
}

#endif
