"""Genera las imágenes del README desde las capturas del arnés.

    sh build_host.sh                                  # deja las BMP en out/
    python herramientas/gen_imagenes_readme.py [out] [docs/img]

Hace, en docs/img:
  * las capturas que usan el README y el manual, a ×2 sin suavizar (cada
    píxel de la calculadora, un cuadrado de 2×2);
  * banner.png: la portada del repositorio, con el título, la sigla, el lema
    y la miniatura de la portada del programa (recortados de su pantalla,
    ×3 y ×2) y el osciloscopio a la derecha;
  * demo.gif: un recorrido por las pantallas, con un subtítulo en cada una.

Usa Pillow y una fuente TrueType para los subtítulos (Segoe UI en Windows;
si no, DejaVu Sans). Las capturas son de la versión pública.
"""
import os
import sys

from PIL import Image, ImageDraw, ImageFilter, ImageFont

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ORIGEN = sys.argv[1] if len(sys.argv) > 1 else os.path.join(RAIZ, 'out')
DESTINO = sys.argv[2] if len(sys.argv) > 2 else os.path.join(RAIZ, 'docs', 'img')

FONDO = (16, 16, 24)          # C_FONDO tal como sale en la pantalla (RGB565)
BARRA = (40, 70, 130)         # C_BARRA
AMARILLO = (250, 205, 40)     # C_FUENTE
TEXTO = (228, 230, 236)       # C_TEXTO
AZUL = (120, 200, 255)        # C_TITULO
TENUE = (96, 104, 118)        # C_TENUE

CAPTURAS = {
    'menu': '001_menu', 'taller': 'cm_taller_4',
    'cc_resultado': '016_propio_resultado', 'cc_paso': '591_j_paso01',
    'osciloscopio': '623_emu_osc', 'ca_fasores': '647_ca_resultado',
    'ca_calculadora': '664_ca_calc_resultado', 'bode': 'bode_tanque_cursores',
    'fasorial': 'fasorial_potencias', 'barrido_led': 'curva_led_20mA',
    'bjt_familia': 'e4_bjt_familia', 'capacitores_datos': 'cm_datos_ejemplo',
    'capacitores_resultado': 'cm_res_17', 'capacitores_dibujo': 'cm_dib_mixto',
    'editor': '011_editor_problema_propio', 'semiconductores': 'e4_esq_todos',
}

RECORRIDO = [
    ('000_portada', 'Hakaru CES: emulador y calculadora de circuitos'),
    ('001_menu', 'Menú principal: Taller, Aprende y Ayuda'),
    ('011_editor_problema_propio', 'Edición del circuito sobre la rejilla'),
    ('016_propio_resultado', 'Resolución en corriente continua'),
    ('591_j_paso01', 'Procedimiento paso a paso'),
    ('623_emu_osc', 'Emulación en el tiempo: osciloscopio de dos canales'),
    ('647_ca_resultado', 'Corriente alterna: fasores y potencias'),
    ('fasorial_potencias', 'Diagrama fasorial y triángulo de potencias'),
    ('bode_tanque_cursores', 'Trazador de Bode con cursores y medidas'),
    ('e4_esq_todos', 'Diodos, LED, amplificador operacional y transistores'),
    ('e4_res_led', 'Punto de operación por el método de Newton'),
    ('curva_led_20mA', 'Barrido de CC con lectura en el cursor'),
    ('e4_bjt_familia', 'Curvas características del BJT'),
    ('cm_res_17', 'Capacitores equivalentes: opciones ordenadas'),
    ('cm_dib_mixto', 'Diagrama de conexión de la opción elegida'),
]


def fuente(tam, negrita=False):
    nombres = (['segoeuib.ttf', 'DejaVuSans-Bold.ttf'] if negrita else ['segoeui.ttf', 'DejaVuSans.ttf'])
    dirs = [r'C:\Windows\Fonts', '/mnt/c/Windows/Fonts', '/usr/share/fonts/truetype/dejavu']
    for d in dirs:
        for n in nombres:
            r = os.path.join(d, n)
            if os.path.isfile(r):
                return ImageFont.truetype(r, tam)
    return ImageFont.load_default()


def bmp(nombre):
    return Image.open(os.path.join(ORIGEN, nombre + '.bmp')).convert('RGB')


def x2(im, k=2):
    return im.resize((im.width * k, im.height * k), Image.NEAREST)


def capturas():
    for dst, src in CAPTURAS.items():
        x2(bmp(src)).save(os.path.join(DESTINO, dst + '.png'), optimize=True)


def rejilla(im, paso=24, color=(26, 28, 38)):
    """Una rejilla de puntos, como la de una protoboard o un osciloscopio"""
    d = ImageDraw.Draw(im)
    for y in range(paso // 2, im.height, paso):
        for x in range(paso // 2, im.width, paso):
            d.rectangle((x, y, x + 1, y + 1), fill=color)


def con_marco(im, radio=14, borde=3):
    """La captura con esquinas redondeadas, borde y sombra; devuelve RGBA"""
    W, H = im.width + 2 * borde, im.height + 2 * borde
    marco = Image.new('RGBA', (W, H), (0, 0, 0, 0))
    m = Image.new('L', (W, H), 0)
    ImageDraw.Draw(m).rounded_rectangle((0, 0, W - 1, H - 1), radius=radio, fill=255)
    fondo = Image.new('RGBA', (W, H), BARRA + (255,))
    marco.paste(fondo, (0, 0), m)
    m2 = Image.new('L', im.size, 0)
    ImageDraw.Draw(m2).rounded_rectangle((0, 0, im.width - 1, im.height - 1), radius=radio - borde, fill=255)
    marco.paste(im, (borde, borde), m2)
    sombra = Image.new('RGBA', (W + 60, H + 60), (0, 0, 0, 0))
    ms = Image.new('L', (W + 60, H + 60), 0)
    ImageDraw.Draw(ms).rounded_rectangle((30, 36, W + 29, H + 35), radius=radio, fill=170)
    sombra.putalpha(ms.filter(ImageFilter.GaussianBlur(14)))
    sombra.alpha_composite(marco, (30, 30))
    return sombra


def banner():
    W, H = 1400, 560
    im = Image.new('RGB', (W, H), FONDO)
    rejilla(im)
    p = bmp('000_portada')
    titulo = x2(p.crop((73, 15, 247, 45)), 3)       # «HAKARU» y 測る
    sigla = x2(p.crop((67, 51, 252, 60)), 2)        # «CES · Circuit Emulator & Solver»
    lema = x2(p.crop((22, 65, 297, 76)), 2)         # «Emulador y calculadora...»
    mini = x2(p.crop((42, 94, 250, 180)), 2)        # el emisor común de la portada
    x0 = 70
    im.paste(titulo, (x0, 60))
    im.paste(sigla, (x0, 172))
    im.paste(lema, (x0, 208))
    im.paste(mini, (x0 + 40, 262))
    d = ImageDraw.Draw(im)
    f = fuente(24)
    d.text((x0, 470), 'TI-Nspire CX II  ·  Ndless  ·  C99', font=f, fill=TENUE)
    osc = con_marco(x2(bmp('623_emu_osc')))
    rgba = im.convert('RGBA')
    rgba.alpha_composite(osc, (W - osc.width + 6, (H - osc.height) // 2 + 4))
    rgba.convert('RGB').save(os.path.join(DESTINO, 'banner.png'), optimize=True)


def demo():
    cuadros = []
    f = fuente(23, negrita=True)
    fn = fuente(23, negrita=True)
    n = len(RECORRIDO)
    for k, (src, texto) in enumerate(RECORRIDO):
        pant = x2(bmp(src))
        im = Image.new('RGB', (pant.width, pant.height + 64), FONDO)
        im.paste(pant, (0, 0))
        d = ImageDraw.Draw(im)
        y = pant.height
        d.rectangle((0, y, im.width, y + 2), fill=BARRA)
        num = f'{k + 1}'
        d.text((18, y + 12), num, font=fn, fill=AMARILLO)
        d.text((18 + d.textlength('00', font=fn) + 6, y + 12), texto, font=f, fill=TEXTO)
        # los puntos del avance
        ancho = n * 14
        x = im.width - ancho - 14
        for i in range(n):
            c = AMARILLO if i == k else (TENUE if i < k else (44, 48, 60))
            d.ellipse((x + i * 14, y + 50, x + i * 14 + 6, y + 56), fill=c)
        cuadros.append(im.quantize(colors=256, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE))
    dur = [2400] + [1900] * (n - 2) + [2600]
    cuadros[0].save(os.path.join(DESTINO, 'demo.gif'), save_all=True, append_images=cuadros[1:],
                    duration=dur, loop=0, optimize=True, disposal=1)


def main():
    os.makedirs(DESTINO, exist_ok=True)
    capturas()
    banner()
    demo()
    for n in ('banner.png', 'demo.gif'):
        print(n, os.path.getsize(os.path.join(DESTINO, n)), 'bytes')


if __name__ == '__main__':
    main()
