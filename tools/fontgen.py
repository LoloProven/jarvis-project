# Convierte una letra TTF al formato del reloj (include/fonts/*.h): cada letra en 16 tonos
# de gris (4 bits por píxel) para que se vea suavizada, con tildes y ñ (Latin-1).
#
# Uso (con freetype-py instalado; ver tools/README de fuentes en CLAUDE.md):
#   python tools/fontgen.py fonts/BarlowCondensed-Bold.ttf 28 barlow28 include/fonts/barlow28.h
#   python tools/fontgen.py fonts/BarlowCondensed-ExtraBold.ttf 120 barlow120 include/fonts/barlow120.h --digits
#
# --digits: solo números, ":" "-" "." y espacio (para la hora gigante; ocupa mucho menos).
# Los números salen todos del mismo ancho, así la hora y el cronómetro no "bailan".
import sys
import freetype

def main():
    ttf, size, name, out = sys.argv[1], int(sys.argv[2]), sys.argv[3], sys.argv[4]
    digits_only = "--digits" in sys.argv

    face = freetype.Face(ttf)
    face.set_pixel_sizes(0, size)

    if digits_only:
        first, last = 0x20, 0x3A
        wanted = set(ord(c) for c in " 0123456789:-.")
    else:
        first, last = 0x20, 0xFF
        wanted = set(range(0x20, 0x7F)) | set(range(0xA0, 0x100))

    glyphs = {}
    for code in range(first, last + 1):
        if code not in wanted:
            continue
        face.load_char(chr(code), freetype.FT_LOAD_RENDER | freetype.FT_LOAD_TARGET_NORMAL)
        g = face.glyph
        bm = g.bitmap
        rows = [[bm.buffer[r * bm.pitch + c] for c in range(bm.width)] for r in range(bm.rows)]
        glyphs[code] = {"w": bm.width, "h": bm.rows, "xo": g.bitmap_left, "top": g.bitmap_top,
                        "adv": (g.advance.x + 32) >> 6, "rows": rows}

    # Números del mismo ancho (el del más ancho), centrados en su lugar.
    digit_adv = max(glyphs[ord(c)]["adv"] for c in "0123456789")
    for c in "0123456789":
        g = glyphs[ord(c)]
        g["xo"] += (digit_adv - g["adv"]) // 2
        g["adv"] = digit_adv

    ascent = max(g["top"] for g in glyphs.values() if g["h"])
    descent = max(g["h"] - g["top"] for g in glyphs.values() if g["h"])
    descent = max(descent, 0)

    data = bytearray()
    lines = []
    for code in range(first, last + 1):
        g = glyphs.get(code)
        if g is None:
            lines.append("    {0, 0, 0, 0, 0, 0},")
            continue
        offset = len(data)
        nib = []
        for row in g["rows"]:
            for v in row:
                nib.append((v * 15 + 127) // 255)
        if len(nib) % 2:
            nib.append(0)
        for i in range(0, len(nib), 2):
            data.append((nib[i] << 4) | nib[i + 1])
        # yo = cuánto baja el borde de arriba de la letra desde el borde de arriba del renglón.
        yo = ascent - g["top"]
        assert 0 <= g["w"] < 256 and 0 <= g["h"] < 256 and 0 <= g["adv"] < 256
        assert -128 <= g["xo"] < 128 and 0 <= yo < 256
        lines.append(f"    {{{offset}, {g['w']}, {g['h']}, {g['adv']}, {g['xo']}, {yo}}},  // 0x{code:02X}")

    with open(out, "w", encoding="utf-8", newline="\n") as f:
        f.write(f"// Generado con tools/fontgen.py desde {ttf.split('/')[-1].split(chr(92))[-1]}, {size} px. No editar a mano.\n")
        f.write("#pragma once\n#include \"aa_font.h\"\n\n")
        f.write(f"static const uint8_t {name}_bitmap[] = {{\n")
        for i in range(0, len(data), 24):
            f.write("    " + ",".join(f"0x{b:02X}" for b in data[i:i + 24]) + ",\n")
        f.write("};\n\n")
        f.write(f"static const AAGlyph {name}_glyphs[] = {{\n" + "\n".join(lines) + "\n};\n\n")
        f.write(f"static const AAFont {name} = {{{name}_bitmap, {name}_glyphs, 0x{first:02X}, 0x{last:02X}, "
                f"{ascent}, {ascent + descent}}};\n")
    print(f"{out}: {len(data)} bytes, alto {ascent + descent} (arriba {ascent}, abajo {descent})")

main()
