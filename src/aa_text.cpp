#include "aa_text.h"
#include "config.h"
#include "fonts/barlow17.h"
#include "fonts/barlow21.h"
#include "fonts/barlow28.h"
#include "fonts/barlow40.h"
#include "fonts/barlow64.h"
#include "fonts/barlow120.h"

const AAFont& FONT_S    = barlow17;
const AAFont& FONT_M    = barlow21;
const AAFont& FONT_L    = barlow28;
const AAFont& FONT_XL   = barlow40;
const AAFont& FONT_XXL  = barlow64;
const AAFont& FONT_HUGE = barlow120;

// Renglón armado en la PSRAM y mandado a la pantalla de una vez (el más alto, la hora
// gigante, mide 86 px).
static const int BUF_ROWS = 90;
static uint16_t* buf = nullptr;

// Siguiente letra de un texto UTF-8 como código Latin-1 (lo que no es Latin-1 = '?').
static uint8_t nextChar(const char*& p) {
    uint8_t c = *p++;
    if (c < 0x80) return c < 0x20 ? ' ' : c;
    if ((c == 0xC2 || c == 0xC3) && (*p & 0xC0) == 0x80) {
        uint8_t d = *p++;
        return (c == 0xC2) ? d : d + 0x40;
    }
    while ((*p & 0xC0) == 0x80) p++;      // emojis y otros: se saltean
    return '?';
}

static const AAGlyph* glyph(const AAFont& f, uint8_t c) {
    if (c < f.first || c > f.last) return nullptr;
    const AAGlyph* gl = &f.glyphs[c - f.first];
    return (gl->adv == 0) ? nullptr : gl;
}

int AAText::width(const AAFont& f, const char* s, int spacing) {
    int w = 0;
    bool any = false;
    while (*s) {
        const AAGlyph* gl = glyph(f, nextChar(s));
        if (!gl) gl = glyph(f, '?');
        if (!gl) continue;
        w += gl->adv + spacing;
        any = true;
    }
    return any ? w - spacing : 0;
}

void AAText::drawBox(Arduino_GFX* g, const AAFont& f, int x, int y, int w, const char* s,
                     uint16_t fg, uint16_t bg, Align align, int spacing) {
    int h = f.height;
    if (w <= 0 || h > BUF_ROWS) return;
    if (x < 0) { w += x; x = 0; }
    if (x + w > LCD_WIDTH) w = LCD_WIDTH - x;
    if (w <= 0) return;
    if (!buf) {
        buf = (uint16_t*)heap_caps_malloc(LCD_WIDTH * BUF_ROWS * sizeof(uint16_t), MALLOC_CAP_SPIRAM);
        if (!buf) return;
    }

    // Los 16 tonos entre el fondo y el color de la letra.
    uint16_t lut[16];
    int br = bg >> 11, bgG = (bg >> 5) & 0x3F, bb = bg & 0x1F;
    int fr = fg >> 11, fgG = (fg >> 5) & 0x3F, fb = fg & 0x1F;
    for (int a = 0; a < 16; a++) {
        lut[a] = ((br + (fr - br) * a / 15) << 11) | ((bgG + (fgG - bgG) * a / 15) << 5) | (bb + (fb - bb) * a / 15);
    }
    for (int i = 0; i < w * h; i++) buf[i] = bg;

    int tw = width(f, s, spacing);
    int cx = (align == CENTER) ? (w - tw) / 2 : (align == RIGHT) ? w - tw : 0;
    // Nivel de cada píxel ya pintado, para que dos letras que se tocan no se pisen.
    while (*s) {
        const AAGlyph* gl = glyph(f, nextChar(s));
        if (!gl) gl = glyph(f, '?');
        if (!gl) continue;
        const uint8_t* bits = f.bitmap + gl->offset;
        for (int r = 0; r < gl->h; r++) {
            int py = gl->yo + r;
            if (py < 0 || py >= h) continue;
            for (int c = 0; c < gl->w; c++) {
                int px = cx + gl->xo + c;
                if (px < 0 || px >= w) continue;
                int idx = r * gl->w + c;
                uint8_t a = (idx & 1) ? (bits[idx >> 1] & 0x0F) : (bits[idx >> 1] >> 4);
                if (!a) continue;
                uint16_t& dst = buf[py * w + px];
                // Si ya había letra en ese píxel, queda el tono más fuerte.
                if (dst == bg) dst = lut[a];
                else {
                    for (int k = 15; k > a; k--) if (dst == lut[k]) { a = k; break; }
                    dst = lut[a];
                }
            }
        }
        cx += gl->adv + spacing;
    }
    g->draw16bitRGBBitmap(x, y, buf, w, h);
}

int AAText::draw(Arduino_GFX* g, const AAFont& f, int x, int y, const char* s,
                 uint16_t fg, uint16_t bg, int spacing) {
    int w = width(f, s, spacing);
    drawBox(g, f, x, y, w, s, fg, bg, LEFT, spacing);
    return w;
}

int AAText::drawCentered(Arduino_GFX* g, const AAFont& f, int cx, int y, const char* s,
                         uint16_t fg, uint16_t bg, int spacing) {
    int w = width(f, s, spacing);
    drawBox(g, f, cx - w / 2, y, w, s, fg, bg, LEFT, spacing);
    return w;
}

int AAText::drawRight(Arduino_GFX* g, const AAFont& f, int right, int y, const char* s,
                      uint16_t fg, uint16_t bg, int spacing) {
    int w = width(f, s, spacing);
    drawBox(g, f, right - w, y, w, s, fg, bg, LEFT, spacing);
    return w;
}

int AAText::drawWrapped(Arduino_GFX* g, const AAFont& f, int x, int y, int maxW, int maxLines,
                        const char* s, uint16_t fg, uint16_t bg, int lineGap) {
    char line[160];
    int len = strlen(s), pos = 0;
    for (int n = 0; n < maxLines && pos < len; n++) {
        while (pos < len && s[pos] == ' ') pos++;
        // Se agregan palabras mientras entren.
        int end = pos, lastFit = pos;
        while (end < len) {
            int next = end;
            while (next < len && s[next] == ' ') next++;
            while (next < len && s[next] != ' ') next++;
            int cnt = min(next - pos, (int)sizeof(line) - 4);
            memcpy(line, s + pos, cnt);
            line[cnt] = 0;
            if (width(f, line) > maxW) break;
            lastFit = next;
            end = next;
        }
        if (lastFit == pos) {                       // una palabra sola más larga que el renglón
            lastFit = pos;
            while (lastFit < len && s[lastFit] != ' ') {
                int cnt = min(lastFit + 1 - pos, (int)sizeof(line) - 4);
                memcpy(line, s + pos, cnt);
                line[cnt] = 0;
                if (width(f, line) > maxW) break;
                lastFit++;
            }
            if (lastFit == pos) lastFit = pos + 1;
        }
        int cnt = min(lastFit - pos, (int)sizeof(line) - 4);
        memcpy(line, s + pos, cnt);
        line[cnt] = 0;
        pos = lastFit;
        if (n == maxLines - 1 && pos < len) {       // no entra todo: "..." al final
            strcat(line, "...");
            while (cnt > 0 && width(f, line) > maxW) {
                cnt--;                              // sin cortar una letra con tilde a la mitad
                while (cnt > 0 && ((uint8_t)line[cnt] & 0xC0) == 0x80) cnt--;
                line[cnt] = 0;
                strcat(line, "...");
            }
        }
        draw(g, f, x, y, line, fg, bg);
        y += f.height + lineGap;
    }
    return y;
}
