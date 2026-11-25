# Ícono de la página de ajustes (para "Agregar a inicio" en el celu): círculo lima con una
# "J" negra en Barlow Condensed, 180x180. Genera include/web_icon.h.
# Uso: python tools/icongen.py fonts/BarlowCondensed-ExtraBold.ttf include/web_icon.h
import sys, zlib, struct
import freetype

S = 180
LIME = (0xC6, 0xF4, 0x32)
ttf, out = sys.argv[1], sys.argv[2]

alpha = [[0.0] * S for _ in range(S)]       # cobertura del círculo
for y in range(S):
    for x in range(S):
        # 4x4 submuestras para el borde suave
        n = 0
        for sy in range(4):
            for sx in range(4):
                dx = x + (sx + 0.5) / 4 - S / 2
                dy = y + (sy + 0.5) / 4 - S / 2
                if dx * dx + dy * dy <= (S / 2 - 2) ** 2: n += 1
        alpha[y][x] = n / 16

face = freetype.Face(ttf)
face.set_pixel_sizes(0, 130)
face.load_char("J", freetype.FT_LOAD_RENDER)
bm = face.glyph.bitmap
ox = (S - bm.width) // 2
oy = (S - bm.rows) // 2
ink = [[0.0] * S for _ in range(S)]
for r in range(bm.rows):
    for c in range(bm.width):
        ink[oy + r][ox + c] = bm.buffer[r * bm.pitch + c] / 255

raw = bytearray()
for y in range(S):
    raw.append(0)
    for x in range(S):
        a, k = alpha[y][x], ink[y][x]
        # fondo negro, círculo lima, letra negra
        for ch in LIME:
            raw.append(round(ch * a * (1 - k)))
def chunk(t, d):
    return struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xffffffff)
png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", S, S, 8, 2, 0, 0, 0)) \
    + chunk(b"IDAT", zlib.compress(bytes(raw), 9)) + chunk(b"IEND", b"")
with open(out, "w", newline="\n") as f:
    f.write("// Generado con tools/icongen.py. No editar a mano.\n#pragma once\n#include <stdint.h>\n\n")
    f.write("static const uint8_t WEB_ICON_PNG[] = {\n")
    for i in range(0, len(png), 24):
        f.write("    " + ",".join(f"0x{b:02X}" for b in png[i:i + 24]) + ",\n")
    f.write("};\n")
if "--png" in sys.argv:  # para mirarlo
    open(out.replace(".h", ".png"), "wb").write(png)
print(len(png), "bytes")
