"""Genera kanji_img.c: el kanji 測る («medir», hakaru) de la portada.

    python herramientas/gen_kanji.py kanji_img.c [fuente.ttf]

La Nspire no tiene fuentes japonesas y las del programa son de 6 px de
ancho, así que el kanji va como imagen ya rasterizada. Por omisión se usa
Noto Sans JP (C:\\Windows\\Fonts\\NotoSansJP-VF.ttf, SIL Open Font License
1.1; al ser variable se elige el peso PESO); vale cualquier otra que tenga
los dos caracteres (Yu Gothic: YuGothB.ttc).

Se dibuja a SUPER veces el tamaño y se reduce por bloques (la cobertura
exacta de cada píxel), y el antialias se mezcla AQUÍ contra el fondo real
de la portada, C_FONDO de gfx.h, con la misma cuenta que la macro RGB():
en la calculadora basta con copiar las filas al backbuffer. Un píxel sin
tinta sale exactamente C_FONDO, así que el borde no se nota. La imagen se
recorta a la tinta. Si cambian C_FONDO, TINTA o el tamaño, se vuelve a
correr esto (y el arnés lo nota: compara la imagen con la pantalla).
"""
import sys
from PIL import Image, ImageDraw, ImageFont

TEXTO = '\u6e2c\u308b'                    # 測る
TAM = 31                                  # tamaño de la letra, en píxeles
PESO = 700                                # Bold (eje wght de la fuente variable)
SUPER = 8                                 # sobremuestreo
TINTA = (228, 230, 236)                   # C_TEXTO de gfx.h
FONDO = (16, 18, 24)                      # C_FONDO de gfx.h

salida = sys.argv[1]
ruta = sys.argv[2] if len(sys.argv) > 2 else r'C:\Windows\Fonts\NotoSansJP-VF.ttf'

fuente = ImageFont.truetype(ruta, TAM * SUPER)
try:
    fuente.set_variation_by_axes([PESO])
except (OSError, AttributeError):          # no es variable: su peso es el que hay
    pass

x0, y0, x1, y1 = fuente.getbbox(TEXTO)
m = 2 * SUPER
W = (x1 - x0 + 2 * m + SUPER - 1) // SUPER * SUPER
H = (y1 - y0 + 2 * m + SUPER - 1) // SUPER * SUPER
grande = Image.new('L', (W, H), 0)
ImageDraw.Draw(grande).text((m - x0, m - y0), TEXTO, font=fuente, fill=255)
cob = grande.reduce(SUPER)                # media de cada bloque: la cobertura


def rgb565(r, g, b):
    """La macro RGB() de gfx.h"""
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def mezcla(a):
    return rgb565(*(int(f + (t - f) * a / 255.0 + 0.5) for f, t in zip(FONDO, TINTA)))


fondo = rgb565(*FONDO)
w0, h0 = cob.size
px = [[mezcla(cob.getpixel((x, y))) for x in range(w0)] for y in range(h0)]
filas = [y for y in range(h0) if any(v != fondo for v in px[y])]
cols = [x for x in range(w0) if any(px[y][x] != fondo for y in range(h0))]
px = [fila[cols[0]:cols[-1] + 1] for fila in px[filas[0]:filas[-1] + 1]]
KW, KH = len(px[0]), len(px)
todo = [v for fila in px for v in fila]

with open(salida, 'w', newline='\n', encoding='ascii') as f:
    f.write('/* GENERADO por herramientas/gen_kanji.py: no editar a mano. Ver kanji_img.h. */\n')
    f.write('#include "kanji_img.h"\n\n')
    f.write('/* U+6E2C U+308B (hakaru) en %s, %d px, peso %d; tinta RGB(%d, %d, %d)\n'
            ' * mezclada contra C_FONDO RGB(%d, %d, %d), en RGB565 */\n'
            % (fuente.getname()[0], TAM, PESO, *TINTA, *FONDO))
    f.write('const int KANJI_W = %d, KANJI_H = %d;\n\n' % (KW, KH))
    f.write('const unsigned short KANJI_PIX[%d * %d] = {\n' % (KW, KH))
    for i in range(0, len(todo), 12):
        f.write('    ' + ','.join('0x%04X' % v for v in todo[i:i + 12]) + ',\n')
    f.write('};\n')
print('kanji', KW, 'x', KH)
