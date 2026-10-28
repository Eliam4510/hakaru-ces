"""Genera font6x12.c a partir de la fuente de consola Terminus 6x12.

    python genfont.py Uni2-Terminus12x6.psf.gz font6x12.c

El .psf.gz esta en /usr/share/consolefonts de cualquier Ubuntu (paquete
console-setup-linux). El mapa de caracteres es el de gfx.h:

    0x20-0x7E  ASCII
    0x80-0x9D  simbolos propios (ohmio, paralelo, flechas, subindices...)
    0xA0-0xFF  Latin-1: tildes, enye, abre-interrogacion, grado, cuadrado...

El texto del programa se escribe en UTF-8 y el compilador lo pasa a Latin-1
(-fexec-charset=ISO-8859-1), asi que "tension" con tilde se escribe tal cual.
"""
import sys
from fuente_psf import carga

w, h, g, m = carga(sys.argv[1])
salida = sys.argv[2]

PROPIOS = {
    0x80: (0x3a9, 'Omega (ohmio)'), 0x81: (None, 'paralelo'), 0x82: (0x2248, 'aproximado'),
    0x83: (None, 'visto bueno'), 0x84: (0x2014, 'raya'), 0x85: (0x2192, 'flecha der'),
    0x86: (0x2190, 'flecha izq'), 0x87: (0x2191, 'flecha arriba'), 0x88: (0x2193, 'flecha abajo'),
    0x89: (0x221a, 'raiz'), 0x8A: (0x2264, 'menor o igual'), 0x8B: (0x2265, 'mayor o igual'),
    0x8C: (0x2260, 'distinto'), 0x8D: (0x394, 'Delta'), 0x8E: (0x3a3, 'Sigma'),
    0x8F: (None, 'sub 1'), 0x90: (None, 'sub 2'), 0x91: (None, 'sub 3'), 0x92: (None, 'sub 4'),
    0x93: (None, 'sub 0'), 0x94: (0x221e, 'infinito'), 0x95: (0x3b8, 'theta'), 0x96: (0x3c0, 'pi'),
    0x97: (0x3b1, 'alfa'), 0x98: (0x3b2, 'beta'), 0x99: (0x3b3, 'gamma'), 0x9A: (0x3c1, 'rho'),
    0x9B: (0x3c6, 'fi'), 0x9C: (0x3c9, 'omega'), 0x9D: (None, 'integral'),
}

A_MANO = {
    0x93: ["......"] * 6 + [".##...", "#..#..", "#..#..", "#..#..", ".##...", "......"],
    0x8F: ["......"] * 6 + ["..#...", ".##...", "..#...", "..#...", ".###..", "......"],
    0x90: ["......"] * 6 + [".##...", "#..#..", "..#...", ".#....", "####..", "......"],
    0x91: ["......"] * 6 + ["###...", "...#..", ".##...", "...#..", "###...", "......"],
    0x92: ["......"] * 6 + ["#..#..", "#..#..", "####..", "...#..", "...#..", "......"],
    0x9D: ["...##.", "..#...", "..#...", "..#...", "..#...", "..#...",
           "..#...", "..#...", "..#...", "..#...", "##....", "......"],
    0x81: ["......", "......", ".#.#..", ".#.#..", ".#.#..", ".#.#..",
           ".#.#..", ".#.#..", ".#.#..", ".#.#..", "......", "......"],
    0x83: ["......", "......", "......", ".....#", "....#.", "....#.",
           "#..#..", ".#.#..", "..#...", "......", "......", "......"],
}

def filas(u):
    return [''.join('#' if r & (0x80 >> x) else '.' for x in range(w)) for r in g[m[u]]]

con = []
for cp in range(0x20, 0x7F):
    con.append((cp, filas(cp), repr(chr(cp))))
for cp in range(0x80, 0x9E):
    u, nom = PROPIOS[cp]
    if cp in A_MANO:
        con.append((cp, A_MANO[cp], nom + ' (a mano)'))
    elif u in m:
        con.append((cp, filas(u), nom))
    else:
        print('SIN GLIFO', hex(cp))
for cp in range(0xA1, 0x100):                  # Latin-1 imprimible
    if cp in m:
        con.append((cp, filas(cp), 'U+%04X' % cp))
    else:
        print('Latin-1 sin glifo', hex(cp))

with open(salida, 'w', newline='\n', encoding='ascii') as f:
    f.write('''/* ============================================================================
 *  font6x12.c  -  Fuente del texto de Hakaru CES: Terminus 6x12
 *
 *  GENERADO por genfont.py a partir de Uni2-Terminus12x6.psf (console-setup).
 *  Declarada en ASCII-art, como font6x8.c, para poder revisarla y retocarla a
 *  mano. Las letras usan cinco columnas y la sexta es separacion; las
 *  mayusculas tienen 8 filas y hay 2 para los descendentes: es lo que la hace
 *  mucho mas legible que la 5x7 en la pantalla de la calculadora.
 *
 *  Terminus Font: Copyright (c) 2010 Dimitar Toshkov Zhekov, con el nombre
 *  reservado "Terminus Font". Licencia SIL Open Font License 1.1.
 *
 *  Paralelo, visto bueno, subindices e integral no estan en Terminus: se
 *  dibujaron a mano en la misma rejilla.
 * ========================================================================= */
#include "gfx.h"
#include "font.h"

typedef struct {
    unsigned short cp;
    const char    *r[12];
} Glifo12;

#define G(cp,a,b,c,d,e,f,g,h,i,j,k,l) { cp, { a,b,c,d,e,f,g,h,i,j,k,l } }

static const Glifo12 GLIFOS12[] = {
''')
    for cp, fs, nom in con:
        args = ','.join('"%s"' % s for s in fs)
        f.write('G(0x%02X,%s), /* %s */\n' % (cp, args, nom.replace('*/', '* /')))
    f.write('''};

#define N_GLIFOS12 ((int)(sizeof GLIFOS12 / sizeof GLIFOS12[0]))

/* Tabla compilada: seis columnas caben en un byte. Bit x = columna x. */
static u8 bits12[256][12];
static int listo12 = 0;

static void init12(void)
{
    int i, y, x, cp;
    for (i = 0; i < 256; i++)
        for (y = 0; y < 12; y++) bits12[i][y] = 0;
    for (i = 0; i < N_GLIFOS12; i++) {
        cp = GLIFOS12[i].cp;
        for (y = 0; y < 12; y++) {
            const char *fila = GLIFOS12[i].r[y];
            u8 msk = 0;
            for (x = 0; x < 6 && fila[x]; x++)
                if (fila[x] != '.') msk |= (u8)(1u << x);
            bits12[cp][y] = msk;
        }
    }
    /* Codigo sin glifo: recuadro visible, como en la 6x8 */
    for (cp = 0x21; cp <= 0xFF; cp++) {
        int vacio = 1;
        if (cp == 0xA0) continue;                   /* espacio duro */
        for (y = 0; y < 12; y++) if (bits12[cp][y]) { vacio = 0; break; }
        if (vacio) {
            bits12[cp][2] = 0x1F; bits12[cp][9] = 0x1F;
            for (y = 3; y < 9; y++) bits12[cp][y] = 0x11;
        }
    }
    listo12 = 1;
}

const u8 *font_glifo12(unsigned c)
{
    if (!listo12) init12();
    return bits12[c & 0xFFu];
}
''')
print('glifos:', len(con))
