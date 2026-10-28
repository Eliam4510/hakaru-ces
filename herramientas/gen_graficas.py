"""Genera la gráfica del README (docs/img/grafica_optimizacion_{claro,oscuro}.svg).

    python herramientas/gen_graficas.py [fuente.ttf]

Barras horizontales, una serie, en versión clara y oscura (el README elige
con <picture> y prefers-color-scheme): cuánto trabajo menos hace la
calculadora tras la fase 1 de la optimización (docs/DESIGN.md §23.3). Lo
que mide GitHub por su cuenta (los lenguajes, el tamaño) va en sus propios
widgets, no aquí.

Fondo transparente (se ve sobre el de GitHub), texto convertido en trazos
(el SVG no depende de las fuentes del que lo mira), barras finas con el
extremo de datos redondeado (4 px) y el valor escrito al final de cada una.
Los colores de las barras, validados contra los fondos de GitHub (#ffffff y
#0d1117). Necesita matplotlib.
"""
import os
import sys

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt  # noqa: E402
from matplotlib import font_manager  # noqa: E402
from matplotlib.patches import FancyBboxPatch, Rectangle  # noqa: E402

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SALIDA = os.path.join(RAIZ, 'docs', 'img')

TEMAS = {
    'claro': {'barra': '#2a78d6', 'texto': '#1f2328', 'tenue': '#59636e', 'base': '#d1d9e0'},
    'oscuro': {'barra': '#3987e5', 'texto': '#f0f6fc', 'tenue': '#9198a1', 'base': '#3d444d'},
}


def fuente():
    cand = sys.argv[1:] + [r'C:\Windows\Fonts\segoeui.ttf', '/mnt/c/Windows/Fonts/segoeui.ttf',
                           '/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf']
    for f in cand:
        if os.path.isfile(f):
            font_manager.fontManager.addfont(f)
            return font_manager.FontProperties(fname=f).get_name()
    return 'DejaVu Sans'


# DESIGN.md §23.3: el perfil del ARM frente a la candidata de antes
OPTIMIZACION = [
    ('Bode, pantalla quieta', 89),
    ('Osciloscopio (dibujo)', 79),
    ('Ejemplo de CC resuelto', 67),
    ('Punto de CA', 60),
    ('Pantalla de CA', 56),
    ('Peor caso de CC', 53),
    ('Bode de un RLC', 48),
    ('Editor (dibujo)', 47),
    ('Paso del puente con 1N4007', 26),
    ('Paso del emulador', 15),
]


def barras(filas, titulo, subtitulo, etiqueta, xmax, archivo, tema, familia):
    c = TEMAS[tema]
    n = len(filas)
    alto_fila = 0.42                       # pulgadas por barra
    fig = plt.figure(figsize=(9.0, 1.25 + alto_fila * n), dpi=100)
    fig.patch.set_alpha(0)
    izq, der = 0.30, 0.90
    abajo = 0.25 / fig.get_figheight()
    arriba = 1 - 1.05 / fig.get_figheight()
    ax = fig.add_axes([izq, abajo, der - izq, arriba - abajo])
    ax.set_facecolor('none')
    ax.set_xlim(0, xmax)
    ax.set_ylim(n - 0.5, -0.5)
    for s in ax.spines.values():
        s.set_visible(False)
    ax.tick_params(left=False, bottom=False, labelbottom=False)
    ax.set_yticks(range(n))
    ax.set_yticklabels([f[0] for f in filas], fontsize=12.5, color=c['texto'], family=familia)

    # 4 px de redondeo en el extremo de datos, en unidades de los datos
    w_px = (der - izq) * fig.get_figwidth() * fig.dpi
    h_px = (arriba - abajo) * fig.get_figheight() * fig.dpi
    rx = 4 * xmax / w_px
    ry = 4 * n / h_px
    grosor = 0.56
    ax.axvline(0, color=c['base'], lw=1.0, zorder=1)
    for i, (_, v) in enumerate(filas):
        y0 = i - grosor / 2
        if v > 2 * rx:
            ax.add_patch(FancyBboxPatch((v - 2 * rx, y0), 2 * rx, grosor,
                                        boxstyle=f'round,pad=0,rounding_size={rx}',
                                        mutation_aspect=ry / rx, lw=0, fc=c['barra'], zorder=2))
            ax.add_patch(Rectangle((0, y0), v - rx, grosor, lw=0, fc=c['barra'], zorder=2))
        ax.text(v + xmax * 0.012, i, etiqueta(v), va='center', ha='left', fontsize=12.5,
                color=c['texto'], family=familia, fontweight='bold')

    fig.text(0.03, 1 - 0.36 / fig.get_figheight(), titulo, fontsize=17, color=c['texto'],
             family=familia, fontweight='bold', va='center')
    fig.text(0.03, 1 - 0.70 / fig.get_figheight(), subtitulo, fontsize=12, color=c['tenue'],
             family=familia, va='center')
    fig.savefig(archivo, format='svg', transparent=True)
    plt.close(fig)


def main():
    familia = fuente()
    plt.rcParams['svg.fonttype'] = 'path'
    os.makedirs(SALIDA, exist_ok=True)
    for tema in TEMAS:
        barras(OPTIMIZACION,
               'La optimización: cuánto trabajo menos hace la calculadora',
               'Reducción medida en un perfil fiel del ARM (fase 1). Los resultados no cambian ni un bit.',
               lambda v: f'\u2212{v}\u202f%', 100,
               os.path.join(SALIDA, f'grafica_optimizacion_{tema}.svg'), tema, familia)


if __name__ == '__main__':
    main()
