/* ============================================================================
 *  esquema.c  -  Dibujo del circuito  (ver esquema.h)
 *
 *  Todo en enteros: ni una operacion en coma flotante dentro del trazado. Los
 *  numeros de los rotulos se formatean una vez por elemento, fuera de bucles
 *  de pixeles.
 * ========================================================================= */
#include <math.h>
#include <string.h>
#include "esquema.h"
#include "gfx.h"
#include "numfmt.h"
#include "ca.h"
#include "nolin.h"                  /* NL_COLOR, el catálogo y los vacíos de la Etapa 4 */

/* El convenio de los fasores que se enseñan (Circuito.conv) */
static int conv_;
void esq_convenio(int conv) { conv_ = conv; }

#define ED_X0 16
#define ED_Y0 31
#define ED_PX 32
#define ED_PY 24

void esq_vista(Vista *v)
{
    v->resalta = 0;
    v->atenuar = 0;
    v->capa = CAPA_VALORES;
    v->rejilla = 0;
    v->val = 0;
    v->red = 0;
    v->cur_x = v->cur_y = -1;
    v->sel = -1;
    v->sondas = 0;
    v->lectura = 0;
    v->ca = 0;
    v->conv = v->rect = 0;
    v->brillo = 0;
    v->marca_x = v->marca_y = -1;
    v->ctl_de = -1;
    v->letras = 0;
    v->nodo_resalta = -1;
}

void esq_marco_editor(Marco *m)
{
    m->x0 = ED_X0; m->y0 = ED_Y0; m->px = ED_PX; m->py = ED_PY;
}

void esq_pt(const Marco *m, int gx, int gy, int *sx, int *sy)
{
    *sx = m->x0 + gx * m->px;
    *sy = m->y0 + gy * m->py;
}

void esq_marco_ajustado(const Circuito *c, int x, int y, int w, int h, Marco *m)
{
    int c0 = REJ_COLS, c1 = -1, r0 = REJ_FILAS, r1 = -1, gx, gy, d, k, abajo;
    int anch, alt, uw, uh;

    for (gy = 0; gy < REJ_FILAS; gy++)
        for (gx = 0; gx < REJ_COLS; gx++)
            for (d = 0; d < 2; d++)                 /* derecha y abajo */
                if (circ_seg(c, gx, gy, d ? DIR_ABA : DIR_DER) > 0) {
                    int nx = gx + (d ? 0 : 1), ny = gy + (d ? 1 : 0);
                    if (gx < c0) c0 = gx;
                    if (gy < r0) r0 = gy;
                    if (nx > c1) c1 = nx;
                    if (ny > r1) r1 = ny;
                }
    /* la tierra y los terminales también tienen que verse */
    {
        signed char px[3], py[3];
        int i;
        px[0] = c->tierra_x; py[0] = c->tierra_y;
        px[1] = c->term_x[0]; py[1] = c->term_y[0];
        px[2] = c->term_x[1]; py[2] = c->term_y[1];
        for (i = 0; i < 3 && c1 >= 0; i++) {
            if (px[i] < 0 || py[i] < 0) continue;
            if (px[i] < c0) c0 = px[i];
            if (px[i] > c1) c1 = px[i];
            if (py[i] < r0) r0 = py[i];
            if (py[i] > r1) r1 = py[i];
        }
    }
    /* y las sondas de nodo */
    for (d = 0; d < 2 && c1 >= 0; d++)
        if (c->sonda[d].tipo == SONDA_NODO) {
            if (c->sonda[d].x < c0) c0 = c->sonda[d].x;
            if (c->sonda[d].x > c1) c1 = c->sonda[d].x;
            if (c->sonda[d].y < r0) r0 = c->sonda[d].y;
            if (c->sonda[d].y > r1) r1 = c->sonda[d].y;
        }
    if (c1 < 0) { c0 = 0; c1 = REJ_COLS - 1; r0 = 0; r1 = REJ_FILAS - 1; }

    /* margenes: rotulos a la derecha de las verticales y encima/debajo de
     * las horizontales. Un diodo, zener, LED o controlada horizontal en la
     * fila de abajo lleva debajo su segunda línea de 12 (la V, el % o la
     * ley; la del LED y la controlada, más abajo: rotula): más margen */
    abajo = 14;
    for (k = 0; k < c->nel && c1 >= 0; k++) {
        const Elemento *e = &c->el[k];
        int t = e->tipo, n;
        if (t != EL_DIODO && t != EL_ZENER && t != EL_LED && t != EL_EDEP && t != EL_JDEP) continue;
        if (e->ya != e->yb || e->ya != r1) continue;
        n = 5 + (t == EL_LED ? 7 : (t == EL_EDEP || t == EL_JDEP) ? 4 : 0) + FONT_H;
        if (n > abajo) abajo = n;
    }
    uw = w - 12 - 36;
    uh = h - 16 - abajo;
    m->px = (c1 > c0) ? uw / (c1 - c0) : 40;
    m->py = (r1 > r0) ? uh / (r1 - r0) : 30;
    if (m->px > 44) m->px = 44;
    if (m->py > 32) m->py = 32;
    if (m->px < 12) m->px = 12;
    if (m->py < 12) m->py = 12;
    anch = m->px * (c1 - c0);
    alt  = m->py * (r1 - r0);
    m->x0 = x + 12 + (uw - anch) / 2 - c0 * m->px;
    m->y0 = y + 16 + (uh - alt) / 2 - r0 * m->py;
}

const char *esq_capa_nombre(int capa)
{
    switch (capa) {
        case CAPA_NOMBRES:    return "Nombres";
        case CAPA_VALORES:    return "Valores";
        case CAPA_CORRIENTES: return "Corrientes";
        case CAPA_TENSIONES:  return "Tensiones";
        case CAPA_POTENCIALES: return "Potenciales";
        default:              return "Potencias";
    }
}

/* --------------------------------------------------------------------------
 *  Simbolos
 * ----------------------------------------------------------------------- */
/* Punto a lo largo del segmento: t en [0, L] desde a, desplazado 'n'
 * pixeles en perpendicular. (dx,dy) es el sentido unitario de a hacia b. */
#define AL(t, n, ox, oy) do { ox = xa + dx * (t) - dy * (n); oy = ya + dy * (t) + dx * (n); } while (0)

static void resistencia(int xa, int ya, int xb, int yb, u16 col)
{
    int dx = (xb > xa) - (xb < xa), dy = (yb > ya) - (yb < ya);
    int L = (xb - xa) * dx + (yb - ya) * dy;
    int Z = L - 10, s, i, px, py, qx, qy;
    static const int OFS[7] = { 0, 1, -1, 1, -1, 1, 0 };

    if (Z > 18) Z = 18;
    if (Z < 6) Z = 6;
    s = (L - Z) / 2;

    AL(s, 0, px, py);
    gfx_line(xa, ya, px, py, col);
    for (i = 1; i <= 6; i++) {
        AL(s + Z * i / 6, OFS[i] * 4, qx, qy);
        gfx_line(px, py, qx, qy, col);
        px = qx; py = qy;
    }
    gfx_line(px, py, xb, yb, col);
}

/* Fuente de corriente: un círculo con una flecha de a hacia b */
static void fuente_i(int xa, int ya, int xb, int yb, u16 col)
{
    int dx = (xb > xa) - (xb < xa), dy = (yb > ya) - (yb < ya);
    int L = (xb - xa) * dx + (yb - ya) * dy;
    int c = L / 2, r = 7, px, py, qx, qy, tx, ty, ux, uy;

    AL(c - r, 0, px, py);
    gfx_line(xa, ya, px, py, col);
    AL(c + r, 0, px, py);
    gfx_line(px, py, xb, yb, col);
    AL(c, 0, px, py);
    gfx_circle(px, py, r, col, 0);
    AL(c - 4, 0, px, py);                       /* la flecha, hacia b */
    AL(c + 4, 0, qx, qy);
    gfx_line(px, py, qx, qy, col);
    AL(c + 1, -3, tx, ty);
    AL(c + 1, 3, ux, uy);
    gfx_tri(qx, qy, tx, ty, ux, uy, col);
}

/* Instrumento: un círculo con una V o una A */
static void instrumento(int xa, int ya, int xb, int yb, u16 col, const char *letra)
{
    int dx = (xb > xa) - (xb < xa), dy = (yb > ya) - (yb < ya);
    int L = (xb - xa) * dx + (yb - ya) * dy;
    int c = L / 2, r = 7, px, py;

    AL(c - r, 0, px, py);
    gfx_line(xa, ya, px, py, col);
    AL(c + r, 0, px, py);
    gfx_line(px, py, xb, yb, col);
    AL(c, 0, px, py);
    gfx_circle(px, py, r, C_FONDO, 1);
    gfx_circle(px, py, r, col, 0);
    gfx_text(px - 2, py - 5, letra, col, 1);
}

/* Condensador: dos placas perpendiculares; un + junto a la del extremo a
 * cuando empieza cargado (V0 dato y no 0) */
static void condensador(int xa, int ya, int xb, int yb, u16 col, int mas)
{
    int dx = (xb > xa) - (xb < xa), dy = (yb > ya) - (yb < ya);
    int L = (xb - xa) * dx + (yb - ya) * dy;
    int c = L / 2, px, py, qx, qy;

    AL(c - 2, 0, px, py);
    gfx_line(xa, ya, px, py, col);
    AL(c + 2, 0, px, py);
    gfx_line(px, py, xb, yb, col);
    AL(c - 2, -7, px, py); AL(c - 2, 7, qx, qy); gfx_line(px, py, qx, qy, col);
    AL(c + 2, -7, px, py); AL(c + 2, 7, qx, qy); gfx_line(px, py, qx, qy, col);
    if (mas) {
        AL(c - 7, -6, px, py);
        gfx_hline(px - 2, py, 5, col);
        gfx_vline(px, py - 2, 5, col);
    }
}

/* Bobina: tres arcos a un lado del eje; una flecha pequeña de a hacia b
 * cuando empieza con corriente (I0 dato y no 0) */
static void bobina(int xa, int ya, int xb, int yb, u16 col, int flecha_i0)
{
    int dx = (xb > xa) - (xb < xa), dy = (yb > ya) - (yb < ya);
    int L = (xb - xa) * dx + (yb - ya) * dy;
    int c = L / 2, px, py, i;

    AL(c - 9, 0, px, py);
    gfx_line(xa, ya, px, py, col);
    AL(c + 9, 0, px, py);
    gfx_line(px, py, xb, yb, col);
    for (i = -1; i <= 1; i++) {
        AL(c + 6 * i, 0, px, py);
        if (dy == 0) gfx_arc(px, py, 3, 0, 180, col);          /* horizontal: arriba */
        else gfx_arc(px, py, 3, -90, 90, col);                  /* vertical: a la derecha */
    }
    if (flecha_i0) {
        int tx, ty, ux, uy, qx, qy;
        AL(c - 4, -7, px, py); AL(c + 4, -7, qx, qy);
        gfx_line(px, py, qx, qy, col);
        AL(c + 1, -10, tx, ty); AL(c + 1, -4, ux, uy);
        gfx_tri(qx, qy, tx, ty, ux, uy, col);
    }
}

/* Interruptor: pivote, contacto y la cuchilla (sobre el eje si está
 * cerrado; a 30 grados si está abierto) */
static void interruptor(int xa, int ya, int xb, int yb, u16 col, int cerrado)
{
    int dx = (xb > xa) - (xb < xa), dy = (yb > ya) - (yb < ya);
    int L = (xb - xa) * dx + (yb - ya) * dy;
    int c = L / 2, px, py, qx, qy;

    AL(c - 6, 0, px, py);
    gfx_line(xa, ya, px, py, col);
    gfx_circle(px, py, 2, col, 1);
    AL(c + 6, 0, qx, qy);
    gfx_line(qx, qy, xb, yb, col);
    gfx_circle(qx, qy, 2, col, 0);
    if (cerrado) gfx_line(px, py, qx, qy, col);
    else { int ex, ey; AL(c + 5, -6, ex, ey); gfx_line(px, py, ex, ey, col); }
}

/* El dibujito de la forma de onda, centrado en (cx, cy), de unos 8 x 6 px */
static void glifo_onda(int cx, int cy, int forma, u16 col)
{
    switch (forma) {
    case ONDA_SENO:
        gfx_arc(cx - 2, cy, 2, 0, 180, col);
        gfx_arc(cx + 2, cy, 2, 180, 360, col);
        break;
    case ONDA_CUADRADA:
        gfx_vline(cx - 4, cy - 3, 4, col); gfx_hline(cx - 4, cy - 3, 4, col);
        gfx_vline(cx, cy - 3, 7, col); gfx_hline(cx, cy + 3, 4, col);
        gfx_vline(cx + 4, cy - 3, 7, col);
        break;
    case ONDA_TRIANGULAR:
        gfx_line(cx - 4, cy + 3, cx - 2, cy - 3, col);
        gfx_line(cx - 2, cy - 3, cx + 1, cy + 3, col);
        gfx_line(cx + 1, cy + 3, cx + 4, cy - 3, col);
        break;
    case ONDA_PULSO:
        gfx_hline(cx - 4, cy + 3, 3, col); gfx_vline(cx - 1, cy - 3, 7, col);
        gfx_hline(cx - 1, cy - 3, 5, col);
        break;
    default:
        break;
    }
}

/* Generador (una E con forma): un círculo con la forma dentro y el + del
 * lado del borne b */
static void generador(int xa, int ya, int xb, int yb, u16 col, int forma)
{
    int dx = (xb > xa) - (xb < xa), dy = (yb > ya) - (yb < ya);
    int L = (xb - xa) * dx + (yb - ya) * dy;
    int c = L / 2, r = 7, px, py;

    AL(c - r, 0, px, py);
    gfx_line(xa, ya, px, py, col);
    AL(c + r, 0, px, py);
    gfx_line(px, py, xb, yb, col);
    AL(c, 0, px, py);
    gfx_circle(px, py, r, C_FONDO, 1);
    gfx_circle(px, py, r, col, 0);
    glifo_onda(px, py, forma, col);
    AL(c + r + 3, -5, px, py);                     /* el + junto al borne b */
    gfx_hline(px - 2, py, 5, col);
    gfx_vline(px, py - 2, 5, col);
}

/* lado: donde va el rotulo de una fuente vertical (+1 derecha, -1
 * izquierda); el signo + se pone en el lado contrario para no pisarse. */
static void fuente(int xa, int ya, int xb, int yb, u16 col, int lado)
{
    int dx = (xb > xa) - (xb < xa), dy = (yb > ya) - (yb < ya);
    int L = (xb - xa) * dx + (yb - ya) * dy;
    int c = L / 2, m1x, m1y, m2x, m2y, p1x, p1y, p2x, p2y, i;

    /* borne - (placa corta) hacia a, borne + (placa larga) hacia b */
    AL(c - 2, 0, m1x, m1y);
    gfx_line(xa, ya, m1x, m1y, col);
    AL(c + 2, 0, m2x, m2y);
    gfx_line(m2x, m2y, xb, yb, col);

    for (i = 0; i < 2; i++) {                       /* placa corta, gruesa */
        AL(c - 2 - i, -4, p1x, p1y); AL(c - 2 - i, 4, p2x, p2y);
        gfx_line(p1x, p1y, p2x, p2y, col);
    }
    AL(c + 2, -8, p1x, p1y); AL(c + 2, 8, p2x, p2y);    /* placa larga */
    gfx_line(p1x, p1y, p2x, p2y, col);

    /* el + junto a la placa larga, del lado contrario al rotulo: en una
     * vertical, al otro lado; en una horizontal, debajo (el rotulo va
     * encima) */
    AL(c + 2, 0, p1x, p1y);
    if (dy != 0) { p1x -= 9 * lado; p1y += 4 * dy; }
    else         { p1x += 4 * dx; p1y += 9; }
    gfx_hline(p1x - 2, p1y, 5, col);
    gfx_vline(p1x, p1y - 2, 5, col);
}

/* Lado del rotulo de un elemento vertical: +1 a la derecha, -1 a la
 * izquierda. A la derecha salvo que la columna de al lado tenga algo en las
 * mismas filas (dos resistencias en paralelo pegadas) y la de la izquierda
 * este libre: asi los rotulos no se pisan con el elemento vecino. */
static int lado_vertical(const Circuito *c, const Elemento *e)
{
    int x = e->xa, y = e->ya < e->yb ? e->ya : e->yb;
    int dcha = (x == REJ_COLS - 1) || circ_seg(c, x + 1, y, DIR_ABA) > 0;
    int izq  = (x == 0) || circ_seg(c, x - 1, y, DIR_ABA) > 0;
    if (!dcha) return 1;
    if (!izq) return -1;
    return (x == REJ_COLS - 1) ? -1 : 1;
}

/* Flecha de corriente en el sentido real, del lado contrario al rotulo:
 * debajo de un elemento horizontal, al otro lado de uno vertical. */
static void flecha(int xa, int ya, int xb, int yb, int sentido, int lado, u16 col)
{
    if (ya == yb) {
        int cx = (xa + xb) / 2, y = ya + 9;
        int d = ((xb > xa) ? 1 : -1) * sentido;
        gfx_hline(cx - 6, y, 13, col);
        gfx_tri(cx + 7 * d, y, cx + 2 * d, y - 3, cx + 2 * d, y + 3, col);
    } else {
        int cy = (ya + yb) / 2, x = xa - 10 * lado;
        int d = ((yb > ya) ? 1 : -1) * sentido;
        gfx_vline(x, cy - 6, 13, col);
        gfx_tri(x, cy + 7 * d, x - 3, cy + 2 * d, x + 3, cy + 2 * d, col);
    }
}

/* La impedancia: un rectángulo de 16x8 relleno de fondo, con sus hilos */
static void impedancia(int xa, int ya, int xb, int yb, u16 col)
{
    if (ya == yb) {
        int cx = (xa + xb) / 2, x0 = xa < xb ? xa : xb, x1 = xa < xb ? xb : xa;
        gfx_hline(x0, ya, cx - 8 - x0, col);
        gfx_hline(cx + 8, ya, x1 - cx - 8 + 1, col);
        gfx_fill(cx - 8, ya - 4, 16, 8, C_FONDO);
        gfx_rect(cx - 8, ya - 4, 16, 8, col);
    } else {
        int cy = (ya + yb) / 2, y0 = ya < yb ? ya : yb, y1 = ya < yb ? yb : ya;
        gfx_vline(xa, y0, cy - 8 - y0, col);
        gfx_vline(xa, cy + 8, y1 - cy - 8 + 1, col);
        gfx_fill(xa - 4, cy - 8, 8, 16, C_FONDO);
        gfx_rect(xa - 4, cy - 8, 8, 16, col);
    }
}

/* La bobina de tensión del vatímetro: desde su borne b, una línea punteada
 * de 6 px en perpendicular (al lado contrario del rótulo) hasta una tierra
 * pequeña de tres rayas (7, 5 y 3 px) */
static void bobina_tension(int xb, int yb, int xa, int ya, u16 col, int lado)
{
    int i;
    if (ya == yb) {                                   /* horizontal: hacia abajo */
        gfx_line_dash(xb, yb, xb, yb + 6, col, 1, 1);
        for (i = 0; i < 3; i++) gfx_hline(xb - 3 + i, yb + 7 + 2 * i, 7 - 2 * i, col);
    } else {                                          /* vertical: al lado contrario del rótulo */
        int sg = lado < 0 ? 1 : -1;
        gfx_line_dash(xb, yb, xb + 6 * sg, yb, col, 1, 1);
        for (i = 0; i < 3; i++) gfx_vline(xb + (7 + 2 * i) * sg, yb - 3 + i, 7 - 2 * i, col);
    }
    (void)xa;
}

/* --------------------------------------------------------------------------
 *  Etapa 4: diodos, fuentes controladas y componentes de 3 patas (§4.6, §4.7)
 * ----------------------------------------------------------------------- */
static int dentro(int x, int w);

/* (f·(256 - t) + c·t)/256 por canal: el color del LED sobre el fondo */
u16 esq_mezcla(u16 fondo, u16 col, int t256)
{
    int t = t256 < 0 ? 0 : t256 > 256 ? 256 : t256, s = 256 - t;
    int r = (((fondo >> 11) & 31) * s + ((col >> 11) & 31) * t) / 256;
    int g = (((fondo >> 5) & 63) * s + ((col >> 5) & 63) * t) / 256;
    int b = ((fondo & 31) * s + (col & 31) * t) / 256;
    return (u16)((r << 11) | (g << 5) | b);
}

int esq_led_nivel(double I, double If, double Im)
{
    double x;
    int n;
    if (!(If > 0.0) || !(I >= If / 100.0)) return 0;   /* también NaN */
    if (I > Im) return 9;
    x = 8.0 * I / If + 0.5;
    if (!(x < 8.5)) return 8;
    n = (int)x;
    return n < 1 ? 1 : n;
}

/* Un parámetro del emulador, o su valor por omisión si está vacío */
static double par_o(const Elemento *e, int q, double omision)
{
    const Mag *m = &e->par[q - Q_P1];
    return m->est == M_DATO ? m->v : omision;
}

void esq_led_if_im(const Elemento *e, double *If, double *Im)
{
    *If = par_o(e, QL_IF, NL_IF);
    *Im = par_o(e, QL_IM, NL_IM);
    if (!(*If > 0.0)) *If = NL_IF;
    if (!(*Im > 0.0)) *Im = NL_IM;
}

void esq_brillos(const Circuito *c, const double *I, unsigned char br[MAX_EL])
{
    int k;
    double If, Im;
    for (k = 0; k < MAX_EL; k++) br[k] = 0;
    for (k = 0; k < c->nel && k < MAX_EL; k++)
        if (c->el[k].tipo == EL_LED) {
            esq_led_if_im(&c->el[k], &If, &Im);
            br[k] = (unsigned char)esq_led_nivel(I[k], If, Im);
        }
}

/* El color del LED encendido (el de su variante) */
static u16 led_rgb(const Elemento *e)
{
    return NL_COLOR[e->variante < LED_N ? e->variante : LED_ROJO].rgb;
}

/* Diodo sobre su segmento (c = el centro): hilo hasta c - r, triángulo con la
 * base de ±r en c - r y la punta en c + r - 1, y la raya de ±r en c + r - 1
 * (r = 5 en el esquema). ctri: el contorno del triángulo; rell: relleno en
 * crell (0 = hueco); z: los ganchos del zener, de 2 px en las puntas de la
 * raya (el de arriba hacia el ánodo y el de abajo hacia fuera) */
static void diodo(int xa, int ya, int xb, int yb, u16 col, u16 ctri, int rell, u16 crell, int r, int z)
{
    int dx = (xb > xa) - (xb < xa), dy = (yb > ya) - (yb < ya);
    int L = (xb - xa) * dx + (yb - ya) * dy;
    int c = L / 2, px, py, qx, qy, t1x, t1y, t2x, t2y;

    AL(c - r, 0, px, py);
    gfx_line(xa, ya, px, py, col);
    AL(c + r - 1, 0, qx, qy);                        /* la punta, en la raya */
    gfx_line(qx, qy, xb, yb, col);
    AL(c - r, -r, t1x, t1y);
    AL(c - r, r, t2x, t2y);
    if (rell) gfx_tri(t1x, t1y, t2x, t2y, qx, qy, crell);
    gfx_line(t1x, t1y, t2x, t2y, ctri);
    gfx_line(t1x, t1y, qx, qy, ctri);
    gfx_line(t2x, t2y, qx, qy, ctri);
    AL(c + r - 1, -r, t1x, t1y);                     /* la raya */
    AL(c + r - 1, r, t2x, t2y);
    gfx_line(t1x, t1y, t2x, t2y, col);
    if (z) {
        AL(c + r - 3, -r, px, py); gfx_line(t1x, t1y, px, py, col);
        AL(c + r + 1, r, px, py);  gfx_line(t2x, t2y, px, py, col);
    }
}

/* Las dos flechitas del LED, a 45° hacia fuera: salen a r + 1 del eje en
 * c - 3 y c + 1, avanzan k px a lo largo y k hacia fuera, y acaban en una
 * punta de largo h (las alas en (h, 1) y (1, h) hacia atrás: ~50°).
 * (cx, cy): el centro; (ux, uy): el eje de a hacia b; (nx, ny): hacia
 * fuera. Todos unitarios. */
static void flechitas(int cx, int cy, int ux, int uy, int nx, int ny, int r, int k, int h, u16 col)
{
    int i;
    for (i = 0; i < 2; i++) {
        int t = i ? 1 : -3;
        int x0 = cx + t * ux + (r + 1) * nx, y0 = cy + t * uy + (r + 1) * ny;
        int x1 = x0 + k * (ux + nx), y1 = y0 + k * (uy + ny);
        int tx = x1 + ux + nx, ty = y1 + uy + ny;
        gfx_line(x0, y0, x1, y1, col);
        gfx_tri(tx, ty, tx - h * ux - nx, ty - h * uy - ny, tx - ux - h * nx, ty - uy - h * ny, col);
    }
}

/* El LED (§4.6): el diodo con sus flechitas, del lado contrario al rótulo
 * (debajo si es horizontal; lado: el del rótulo de uno vertical). nivel < 0:
 * sin encender (el editor), todo en col. Con nivel, cl es el color del LED:
 * 0 apagado (el contorno y las flechas, apagados); 1 a 8 el triángulo
 * relleno, más fuerte cuanto más brilla, y un halo desde el 4 (dos en el 8);
 * 9 se quemaría: relleno blanco, halo rojo y un «!» junto al cátodo. Los
 * hilos y la raya, en col. */
static void led(int xa, int ya, int xb, int yb, u16 col, u16 cl, int nivel, int lado)
{
    int dx = (xb > xa) - (xb < xa), dy = (yb > ya) - (yb < ya);
    int L = (xb - xa) * dx + (yb - ya) * dy;
    int c = L / 2, cx = xa + dx * c, cy = ya + dy * c;
    int nx = dy == 0 ? 0 : -lado, ny = dy == 0 ? 1 : 0;
    u16 cc;

    if (nivel < 0) {
        diodo(xa, ya, xb, yb, col, col, 0, 0, 5, 0);
        flechitas(cx, cy, dx, dy, nx, ny, 5, 4, 3, col);
        return;
    }
    if (nivel == 0) {
        cc = esq_mezcla(C_FONDO, cl, 150);
        diodo(xa, ya, xb, yb, col, cc, 0, 0, 5, 0);
        flechitas(cx, cy, dx, dy, nx, ny, 5, 4, 3, cc);
        return;
    }
    if (nivel >= 9) {
        int ex = cx + 9 * dx + 7 * nx, ey = cy + 9 * dy + 7 * ny;
        gfx_circle(cx, cy, 14, C_ERROR, 0);
        diodo(xa, ya, xb, yb, col, cl, 1, C_TEXTO, 5, 0);
        flechitas(cx, cy, dx, dy, nx, ny, 5, 4, 3, cl);
        gfx_text_p(dentro(ex - 2, 6), ey - 4, "!", C_ERROR);
        return;
    }
    if (nivel >= 4) {
        cc = esq_mezcla(C_FONDO, cl, 16 * nivel);
        gfx_circle(cx, cy, 10 + nivel / 2, cc, 0);
        if (nivel == 8) gfx_circle(cx, cy, 10 + nivel / 2 - 2, cc, 0);
    }
    diodo(xa, ya, xb, yb, col, cl, 1, esq_mezcla(C_FONDO, cl, 64 + 24 * nivel), 5, 0);
    flechitas(cx, cy, dx, dy, nx, ny, 5, 4, 3, cl);
}

/* Fuente controlada: un rombo de puntas a ±9 px a lo largo y ±w de lado
 * (w = 7 en el esquema), con hilos hasta las puntas. La E, con + en c + 4
 * (lado b) y - en c - 4; la J, con una flecha de c - 5 a c + 5. El color, el
 * de su independiente: el color dice el tipo y la forma, que es controlada. */
static void rombo(int xa, int ya, int xb, int yb, u16 col, int w, int es_j)
{
    int dx = (xb > xa) - (xb < xa), dy = (yb > ya) - (yb < ya);
    int L = (xb - xa) * dx + (yb - ya) * dy;
    int c = L / 2, p1x, p1y, p2x, p2y, qx, qy, rx, ry;

    AL(c - 9, 0, p1x, p1y);
    gfx_line(xa, ya, p1x, p1y, col);
    AL(c + 9, 0, p2x, p2y);
    gfx_line(p2x, p2y, xb, yb, col);
    AL(c, -w, qx, qy);
    AL(c, w, rx, ry);
    gfx_line(p1x, p1y, qx, qy, col); gfx_line(qx, qy, p2x, p2y, col);
    gfx_line(p2x, p2y, rx, ry, col); gfx_line(rx, ry, p1x, p1y, col);
    if (es_j) {                                      /* la flecha, hacia b */
        AL(c - 5, 0, p1x, p1y);
        AL(c + 5, 0, p2x, p2y);
        gfx_line(p1x, p1y, p2x, p2y, col);
        AL(c + 2, -3, qx, qy);
        AL(c + 2, 3, rx, ry);
        gfx_tri(p2x, p2y, qx, qy, rx, ry, col);
    } else {                                         /* los signos: 5 px (3 en el icono) */
        int s = w >= 7 ? 2 : 1;
        AL(c + 4, 0, qx, qy);                        /* + */
        gfx_hline(qx - s, qy, 2 * s + 1, col);
        gfx_vline(qx, qy - s, 2 * s + 1, col);
        AL(c - 4, 0, qx, qy);                        /* - */
        gfx_hline(qx - s, qy, 2 * s + 1, col);
    }
}

/* ---- Marco local de los 3 patas (enteros, en dieciseisavos de paso) ----
 * (ox, oy): la pantalla del origen de la huella; U = (ux, uy) y V = (vx, vy):
 * un paso a lo largo de la flecha y de lado (V hacia abajo si la flecha es
 * horizontal y hacia la derecha si es vertical); S = 16·(v0 + v1): con
 * espejo, v pasa a S - v. Los símbolos escalan con el paso del Marco. */
typedef struct { int ox, oy, ux, uy, vx, vy, S, esp; } Local;

static int d16(int n) { return n >= 0 ? (n + 8) / 16 : -((-n + 8) / 16); }

static void lp(const Local *L, int u, int v, int *x, int *y)
{
    if (L->esp) v = L->S - v;
    *x = L->ox + d16(u * L->ux + v * L->vx);
    *y = L->oy + d16(u * L->uy + v * L->vy);
}

/* El marco local de un 3 patas colocado: la flecha y el espejo de
 * circ_orienta. 0 si sus patas no forman una huella (no se dibuja). */
static int local_de(const Marco *m, const Elemento *e, Local *L)
{
    static const int DX[4] = { 1, 0, -1, 0 }, DY[4] = { 0, 1, 0, -1 };
    const Huella *h = tipo_info(e->tipo)->huella;
    int d, esp, ox, oy;
    if (!h || !circ_orienta(e, &d, &esp, &ox, &oy)) return 0;
    esq_pt(m, ox, oy, &L->ox, &L->oy);
    L->ux = DX[d] * m->px;  L->uy = DY[d] * m->py;
    L->vx = (DY[d] != 0) * m->px;  L->vy = (DX[d] != 0) * m->py;
    L->S = 16 * (h->v0 + h->v1);
    L->esp = esp;
    return 1;
}

static int isqrt_(int n) { int r = 0; while ((r + 1) * (r + 1) <= n) r++; return r; }
static int div_r(int a, int b) { return a >= 0 ? (a + b / 2) / b : -((-a + b / 2) / b); }   /* b > 0 */

/* Punta de flecha rellena de s px, con la punta en (tx, ty) y en la
 * dirección (dx, dy) (de cualquier largo) */
static void punta(int tx, int ty, int dx, int dy, int s, u16 col)
{
    int L = isqrt_(dx * dx + dy * dy), w = (3 * s + 2) / 5, bx, by, ox, oy;
    if (L == 0) return;
    bx = tx - div_r(s * dx, L);
    by = ty - div_r(s * dy, L);
    ox = div_r(-w * dy, L);
    oy = div_r(w * dx, L);
    gfx_tri(tx, ty, bx + ox, by + oy, bx - ox, by - oy, col);
}

static void lin_l(const Local *L, int u0, int v0, int u1, int v1, u16 col)
{
    int x0, y0, x1, y1;
    lp(L, u0, v0, &x0, &y0);
    lp(L, u1, v1, &x1, &y1);
    gfx_line(x0, y0, x1, y1, col);
}

/* AO (- en (0,0), + en (0,32), salida en (32,16)): el triángulo relleno de
 * fondo y sus hilos; los signos, junto a su entrada (con espejo, lp los
 * cambia de sitio). En el icono, sin signos. */
static void ao_sim(const Local *L, u16 col, int signos)
{
    int x0, y0, x1, y1, x2, y2, px, py;
    lp(L, 4, -4, &x0, &y0);
    lp(L, 4, 36, &x1, &y1);
    lp(L, 28, 16, &x2, &y2);
    gfx_tri(x0, y0, x1, y1, x2, y2, C_FONDO);
    gfx_line(x0, y0, x1, y1, col);
    gfx_line(x1, y1, x2, y2, col);
    gfx_line(x2, y2, x0, y0, col);
    lin_l(L, 0, 0, 4, 0, col);
    lin_l(L, 0, 32, 4, 32, col);
    lin_l(L, 28, 16, 32, 16, col);
    if (signos) {
        lp(L, 8, 8, &px, &py);                       /* - */
        gfx_hline(px - 2, py, 5, col);
        lp(L, 8, 24, &px, &py);                      /* + */
        gfx_hline(px - 2, py, 5, col);
        gfx_vline(px, py - 2, 5, col);
    }
}

/* BJT (B en (0,0), C en (16,-16), E en (16,16)): la base, la barra de 2 px en
 * u = 6, el colector y el emisor, con la punta de s px al 80 % del emisor:
 * hacia fuera en el NPN y hacia la barra en el PNP */
static void bjt_sim(const Local *L, u16 col, int pnp, int s)
{
    int ax, ay, bx, by, sx = (L->ux < 0) - (L->ux > 0), sy = (L->uy < 0) - (L->uy > 0);
    lin_l(L, 0, 0, 6, 0, col);
    lp(L, 6, -6, &ax, &ay);                          /* la barra, un px más hacia la base */
    lp(L, 6, 6, &bx, &by);
    gfx_line(ax, ay, bx, by, col);
    gfx_line(ax + sx, ay + sy, bx + sx, by + sy, col);
    lin_l(L, 6, -3, 16, -10, col);                   /* colector */
    lin_l(L, 16, -10, 16, -16, col);
    lin_l(L, 16, 10, 16, 16, col);                   /* emisor */
    lp(L, 6, 3, &ax, &ay);
    lp(L, 16, 10, &bx, &by);
    gfx_line(ax, ay, bx, by, col);
    {
        int ddx = bx - ax, ddy = by - ay, n = isqrt_(ddx * ddx + ddy * ddy);
        int tx = ax + div_r(4 * ddx, 5), ty = ay + div_r(4 * ddy, 5);
        if (!pnp) punta(tx, ty, ddx, ddy, s, col);
        else if (n > 0) punta(tx - div_r(s * ddx, n), ty - div_r(s * ddy, n), -ddx, -ddy, s, col);
    }
}

/* MOSFET (G en (0,0), D en (16,-16), S en (16,16)): la puerta y su placa, el
 * canal partido en tres, D y S, y el cuerpo unido a S, con la punta de s px
 * hacia el canal en el canal N y hacia fuera en el P */
static void mos_sim(const Local *L, u16 col, int canal_p, int s)
{
    int ax, ay, bx, by;
    lin_l(L, 0, 0, 5, 0, col);
    lin_l(L, 5, -7, 5, 7, col);
    lin_l(L, 8, -9, 8, -4, col);
    lin_l(L, 8, -2, 8, 2, col);
    lin_l(L, 8, 4, 8, 9, col);
    lin_l(L, 8, -6, 16, -6, col);                    /* D */
    lin_l(L, 16, -6, 16, -16, col);
    lin_l(L, 8, 6, 16, 6, col);                      /* S */
    lin_l(L, 16, 6, 16, 16, col);
    lin_l(L, 8, 0, 16, 0, col);                      /* el cuerpo */
    lin_l(L, 16, 0, 16, 6, col);
    lp(L, 8, 0, &ax, &ay);
    lp(L, 16, 0, &bx, &by);
    if (!canal_p) { int tx, ty; lp(L, 9, 0, &tx, &ty); punta(tx, ty, ax - bx, ay - by, s, col); }
    else { int tx, ty; lp(L, 15, 0, &tx, &ty); punta(tx, ty, bx - ax, by - ay, s, col); }
}

/* El centro de un elemento en pantalla: el de su segmento o el de su caja */
static void centro(const Marco *m, const Elemento *e, int *x, int *y)
{
    int x0, y0, x1, y1;
    if (circ_npatas(e) == 3) circ_caja(e, &x0, &y0, &x1, &y1);
    else { x0 = e->xa; y0 = e->ya; x1 = e->xb; y1 = e->yb; }
    esq_pt(m, x0, y0, &x0, &y0);
    esq_pt(m, x1, y1, &x1, &y1);
    *x = (x0 + x1) / 2;
    *y = (y0 + y1) / 2;
}

/* --------------------------------------------------------------------------
 *  Rotulos
 * ----------------------------------------------------------------------- */
/* Número corto con su unidad; con signo solo si se pide */
static void con_unidad_s(char *buf, int cap, double v, const char *u, int signo)
{
    char t[24];
    Buf b;
    num_corto(t, sizeof t, signo ? v : fabs(v));
    buf_init(&b, buf, (size_t)cap);
    buf_str(&b, t);
    buf_str(&b, u);
}

static void con_unidad(char *buf, int cap, double v, const char *u) { con_unidad_s(buf, cap, v, u, 0); }

/* Tres cifras con prefijo, con sus ceros y su signo: «5,00V», «21,8mA»,
 * «-10,0V» (los rótulos resueltos de la Etapa 4) */
static void num3(char *buf, int cap, double v, const char *u)
{
    static const char *const PRE[8] = { "p", "n", S_MU, "m", "", "k", "M", "G" };
    double a = fabs(v), x;
    int e3 = 0, dec = 2;
    Buf b;
    buf_init(&b, buf, (size_t)cap);
    if (!(a < 1e300)) { buf_str(&b, "?"); return; }          /* NaN o infinito */
    if (a > 0.0) {
        e3 = (int)floor(log10(a) / 3.0) * 3;
        if (e3 < -12) e3 = -12;
        if (e3 > 9) e3 = 9;
    }
    x = v / pow(10.0, (double)e3);
    if (fabs(x) >= 9.995) dec = 1;
    if (fabs(x) >= 99.95) dec = 0;
    if (fabs(x) >= 999.5 && e3 < 9) { e3 += 3; x /= 1000.0; dec = 2; }
    buf_num(&b, x, dec);
    buf_str(&b, PRE[e3 / 3 + 4]);
    buf_str(&b, u);
}

/* Un fasor guardado (eficaz, coseno, grados) en el convenio: "120∠0°V" */
static void fasor_guardado(char *buf, int cap, const Mag *m, const char *u)
{
    double mod, ang;
    ca_canon_a_conv(conv_, m->v, m->est_ang == M_DATO ? m->ang : 0.0, &mod, &ang);
    num_fasor(buf, (size_t)cap, mod, ang, 0, 1, u);
}

/* ---- Etapa 4 ---- */
const char *const NOM_REG[REG_N] = {
    "", "corte", "conduce", "ruptura", "activa", "saturaci" S_oo "n",
    "inversa", S_oo "hmica", "lineal", "satura +", "satura -"
};

const char *esq_region(int reg) { return reg >= 0 && reg < REG_N ? NOM_REG[reg] : ""; }

u16 esq_region_col(int reg)
{
    if (reg == REG_CONDUCE) return C_OK;
    if (reg <= REG_CORTE || reg >= REG_N) return C_SUAVE;
    return C_CURSOR;
}

/* La ganancia de una controlada: la de la solución (v), el dato con su
 * signo, "?" si se busca, o su nombre (μ, r, g, β) si está vacía */
static void ganancia(const Elemento *e, const Valores *v, int k, Buf *b)
{
    char t[16];
    if (v) { num_corto(t, sizeof t, v->val[k]); buf_str(b, t); }
    else if (e->val.est == M_DATO) { num_corto(t, sizeof t, e->val.v); buf_str(b, t); }
    else if (e->val.est == M_BUSCAR) buf_ch(b, '?');
    else {
        const CampoInfo *f = tipo_campo_v(e->tipo, e->variante, Q_VAL);
        buf_str(b, f ? f->nombre : "k");
    }
}

/* esq_valor de los tipos de la Etapa 4. Sin resolver: su modelo (diodo
 * «1N4148», «Vγ0,7V», «ideal» o «exp.»; zener «Vz5,1V»; LED su color;
 * controlada su ganancia con su unidad; AO «ideal» o «A100k»; BJT «NPN β100»;
 * MOSFET «N Vt2V»). Resuelto: la V (diodos, E controlada, la Vsal del AO) o
 * la I (LED, J controlada, IC, ID), con su signo. */
static void valor_e4(const Elemento *e, const Valores *v, int k, char *buf, int cap)
{
    Buf b;
    char t[16];
    int var = e->variante;
    if (v) {
        int es_v = e->tipo == EL_DIODO || e->tipo == EL_ZENER || e->tipo == EL_EDEP || e->tipo == EL_AO;
        con_unidad_s(buf, cap, es_v ? v->V[k] : v->I[k], es_v ? "V" : "A", 1);
        return;
    }
    buf_init(&b, buf, (size_t)cap);
    switch (e->tipo) {
    case EL_DIODO:
        if (var == DIO_IDEAL) buf_str(&b, "ideal");
        else if (var == DIO_VG) {
            buf_str(&b, "V" S_GAMMA);
            num_corto(t, sizeof t, par_o(e, QD_VG, NL_VG)); buf_str(&b, t); buf_ch(&b, 'V');
        }
        else if (var == DIO_EXP) buf_str(&b, "exp.");
        else if (var == DIO_1N4148) buf_str(&b, NL_1N4148.nombre);
        else if (var == DIO_1N4007) buf_str(&b, NL_1N4007.nombre);
        break;
    case EL_ZENER:
        if (var == ZEN_1N4733A) buf_str(&b, "1N4733A");
        else if (var < ZEN_N) {
            buf_str(&b, "Vz");
            num_corto(t, sizeof t, par_o(e, QZ_VZ, NL_VZ)); buf_str(&b, t); buf_ch(&b, 'V');
        }
        break;
    case EL_LED:
        if (var < LED_N) buf_str(&b, NOM_LED[var]);
        break;
    case EL_EDEP: case EL_JDEP: {
        const CampoInfo *f = tipo_campo_v(e->tipo, var, Q_VAL);
        ganancia(e, 0, k, &b);
        if (e->val.est == M_DATO && f) buf_str(&b, f->unidad);
        break;
    }
    case EL_AO:
        if (var == AO_IDEAL) buf_str(&b, "ideal");
        else if (var == AO_REAL) { buf_ch(&b, 'A'); num_corto(t, sizeof t, par_o(e, QA_A, NL_A)); buf_str(&b, t); }
        break;
    case EL_BJT:
        if (var < BJT_N) {
            buf_str(&b, NOM_BJT[var]); buf_str(&b, " " S_BETA);
            num_corto(t, sizeof t, par_o(e, QB_BF, NL_BF)); buf_str(&b, t);
        }
        break;
    default:                                         /* MOSFET */
        if (var < MOS_N) {
            buf_str(&b, var == MOS_CP ? "P Vt" : "N Vt");
            num_corto(t, sizeof t, par_o(e, QM_VT, NL_VT_M)); buf_str(&b, t); buf_ch(&b, 'V');
        }
        break;
    }
}

void esq_valor_c(const Circuito *c, const Valores *v, int k, char *buf, int cap)
{
    const Elemento *e;
    Buf b;
    if (k < 0 || k >= c->nel) { if (cap > 0) buf[0] = 0; return; }
    e = &c->el[k];
    if (e->tipo != EL_EDEP && e->tipo != EL_JDEP) { esq_valor(e, v, k, buf, cap); return; }
    buf_init(&b, buf, (size_t)cap);                 /* «2·V(R2)», «1k·I(R1)» */
    ganancia(e, v, k, &b);
    buf_str(&b, S_CDOT);
    buf_str(&b, e->variante == DEP_I ? "I(" : "V(");
    buf_str(&b, e->ref >= 0 && e->ref < c->nel ? c->el[e->ref].etiq : "?");
    buf_ch(&b, ')');
}

void esq_valor(const Elemento *e, const Valores *v, int k, char *buf, int cap)
{
    const char *u = tipo_uni_val(e->tipo);
    int signo = tipo_info(e->tipo)->val_min < 0.0 || e->tipo == EL_VATI;   /* una E o J negativa se ve */
    if (e->tipo >= EL_DIODO) { valor_e4(e, v, k, buf, cap); return; }
    if (e->tipo == EL_INTER) { buf[0] = 0; return; }  /* el interruptor no tiene valor */
    if (!v && e->val.est == M_DATO && (e->tipo == EL_FUENTE || e->tipo == EL_CORR) && e->variante == FORMA_FASOR) {
        fasor_guardado(buf, cap, &e->val, u);
        return;
    }
    if (!v && e->val.est == M_DATO && e->tipo == EL_IMP) {       /* siempre R + jX */
        double a = e->val.est_ang == M_DATO ? e->val.ang * 3.14159265358979323846 / 180.0 : 0.0;
        num_fasor(buf, (size_t)cap, e->val.v, a * 180.0 / 3.14159265358979323846, 1, 1, S_OHM);
        return;
    }
    if (!v && e->tipo == EL_VATI) {
        Buf b;
        char t[16];
        buf_init(&b, buf, (size_t)cap);
        if (e->val.est == M_DATO) { buf_str(&b, "P="); num_corto(t, sizeof t, e->val.v); buf_str(&b, t); buf_ch(&b, 'W'); }
        else if (e->val.est == M_BUSCAR) buf_str(&b, "P=?");
        return;
    }
    /* Un instrumento resuelto enseña lo que marca */
    if (v && e->tipo == EL_VOLT) { con_unidad(buf, cap, v->V[k], "V"); return; }
    if (v && e->tipo == EL_AMP)  { con_unidad(buf, cap, v->I[k], "A"); return; }
    if (v) { con_unidad_s(buf, cap, v->val[k], u, signo); return; }
    if (e->val.est == M_DATO) { con_unidad_s(buf, cap, e->val.v, u, signo); return; }
    if (e->val.est == M_BUSCAR) { buf[0] = '?'; buf[1] = 0; return; }
    buf[0] = 0;
}

/* La f del circuito que se dibuja (para el rótulo de la fuente alterna) */
static const Mag *frec_;

/* Linea extra: los datos o incognitas de V e I que no son la magnitud
 * principal (en el pizarron, la I de la fuente). Solo sin resolver. */
static void extra(const Elemento *e, char *buf, int cap, u16 *col)
{
    Buf b;
    char t[16];
    buf_init(&b, buf, (size_t)cap);
    /* C, L y S: lo del emulador (V0, I0, cuándo conmuta) */
    if (e->tipo == EL_COND && e->par[Q_V0 - Q_P1].est == M_DATO) {
        buf_str(&b, "V" S_SUB0 "="); num_corto(t, sizeof t, e->par[Q_V0 - Q_P1].v); buf_str(&b, t); buf_ch(&b, 'V');
        *col = C_TENSION; return;
    }
    if (e->tipo == EL_BOB && e->par[Q_I0 - Q_P1].est == M_DATO) {
        buf_str(&b, "I" S_SUB0 "="); num_corto(t, sizeof t, e->par[Q_I0 - Q_P1].v); buf_str(&b, t); buf_ch(&b, 'A');
        *col = C_CORRIENTE; return;
    }
    if (e->tipo == EL_INTER && e->par[Q_T1 - Q_P1].est == M_DATO) {
        buf_str(&b, "t="); num_corto(t, sizeof t, e->par[Q_T1 - Q_P1].v); buf_str(&b, t);
        if (e->par[Q_T2 - Q_P1].est == M_DATO) {
            buf_ch(&b, ';'); num_corto(t, sizeof t, e->par[Q_T2 - Q_P1].v); buf_str(&b, t);
        }
        *col = C_COTA; return;
    }
    if (e->tipo == EL_VATI && e->aux.est == M_DATO) {
        buf_str(&b, "fp="); num_auto(t, sizeof t, fabs(e->aux.v), 2); buf_str(&b, t); buf_ch(&b, e->aux.v < 0.0 ? 'c' : 'i');
        *col = C_COTA; return;
    }
    if ((e->tipo == EL_FUENTE || e->tipo == EL_CORR) && e->variante == FORMA_FASOR) {
        if (frec_ && frec_->est == M_DATO) { num_corto(t, sizeof t, frec_->v); buf_str(&b, t); buf_str(&b, "Hz"); }
        else buf_str(&b, "f=?");
        *col = C_COTA; return;
    }
    if ((e->I.est == M_DATO && e->I.est_ang == M_DATO) || (e->V.est == M_DATO && e->V.est_ang == M_DATO)) {
        const Mag *m = e->I.est == M_DATO && e->I.est_ang == M_DATO ? &e->I : &e->V;
        char f[24];
        buf_str(&b, m == &e->I ? "I=" : "V=");
        fasor_guardado(f, sizeof f, m, "");
        buf_str(&b, f);
        *col = m == &e->I ? C_CORRIENTE : C_TENSION;
        return;
    }
    /* E y J que no son continuas: su frecuencia o cuándo empieza el pulso */
    if ((e->tipo == EL_FUENTE || e->tipo == EL_CORR) && e->variante != ONDA_CC && e->variante < ONDA_N) {
        if (e->variante == ONDA_PULSO) {
            buf_str(&b, "t" S_SUB0 "=");
            num_corto(t, sizeof t, e->par[Q_T0 - Q_P1].est == M_DATO ? e->par[Q_T0 - Q_P1].v : 0.0);
            buf_str(&b, t); buf_ch(&b, 's');
        } else if (e->par[Q_FREC - Q_P1].est == M_DATO) {
            num_corto(t, sizeof t, e->par[Q_FREC - Q_P1].v); buf_str(&b, t); buf_str(&b, "Hz");
        } else buf_str(&b, "f=?");
        *col = C_COTA; return;
    }
    if (e->tipo == EL_FUENTE && e->aux.est == M_DATO) {
        buf_str(&b, "r="); num_corto(t, sizeof t, e->aux.v); buf_str(&b, t); buf_str(&b, S_OHM);
        *col = C_TEXTO;
    }
    else if (e->tipo == EL_FUENTE && e->aux.est == M_BUSCAR) { buf_str(&b, "r=?"); *col = C_INCOG; }
    else if (e->I.est == M_DATO)       { buf_str(&b, "I="); num_corto(t, sizeof t, fabs(e->I.v)); buf_str(&b, t); buf_ch(&b, 'A'); *col = C_CORRIENTE; }
    else if (e->I.est == M_BUSCAR) { buf_str(&b, "I=?"); *col = C_INCOG; }
    else if (e->V.est == M_DATO)  { buf_str(&b, "V="); num_corto(t, sizeof t, fabs(e->V.v)); buf_str(&b, t); buf_ch(&b, 'V'); *col = C_TENSION; }
    else if (e->V.est == M_BUSCAR) { buf_str(&b, "V=?"); *col = C_INCOG; }
}

/* Un rotulo nunca se sale de la pantalla: se arrima al borde. */
static int dentro(int x, int w)
{
    if (x + w > SCR_W - 1) x = SCR_W - 1 - w;
    if (x < 1) x = 1;
    return x;
}

/* x del rotulo de un elemento vertical en x = xa: en su lado, y si ahi no
 * cabe (cerca del borde, con un valor largo como "2,69kW"), en el otro. */
static int lado_x(int xa, int w, int izq, int sep)
{
    int d = xa + sep, i = xa - sep - w;
    if (izq) return (i >= 1) ? i : dentro(d, w);
    return (d + w <= SCR_W - 1) ? d : dentro(i, w);
}

/* Las cajas de los rótulos de los elementos del último esq_dibuja (el
 * recuadro del elegido las abarca; el arnés mira que no se pisen); rot_k_,
 * el elemento del rótulo */
static int rot_k_ = -1;
static EsqCaja cajas_[2 * MAX_EL];
static int ncajas_;
static int medir_;                        /* 1: los rótulos se miden (sus cajas) y no se pintan */
int esq_cajas(const EsqCaja **c) { *c = cajas_; return ncajas_; }
static void anota_caja(int x, int y, int w, int h)
{
    if (ncajas_ >= 2 * MAX_EL) return;
    cajas_[ncajas_].x = (short)x; cajas_[ncajas_].y = (short)y;
    cajas_[ncajas_].w = (short)w; cajas_[ncajas_].h = (short)h;
    cajas_[ncajas_].k = (short)rot_k_;
    ncajas_++;
}

/* Dos lineas como mucho: a la derecha de un elemento vertical (a la
 * izquierda en la ultima columna), encima y debajo de uno horizontal. */
static void rotula(const Marco *m, const Circuito *c, const Elemento *e,
                   const char *l1, u16 c1, const char *l2, u16 c2)
{
    int xa, ya, xb, yb, w1, w2;
    /* Letra grande (Terminus 6x12) siempre que quepa; la 6x8 solo cuando la
     * rejilla esta tan apretada que dos lineas de 12 no caben entre filas. */
    int chica = (m->py < 24 && l1[0] && l2[0]) || m->py < 16;
    int h = chica ? FONT_H_P : FONT_H;
    /* el rombo de una controlada es más ancho (±7) y las flechitas de un LED
     * horizontal van debajo: sus rótulos, un poco más lejos */
    int ancho = (e->tipo == EL_EDEP || e->tipo == EL_JDEP) ? 4 : 0;
    int bajo = e->tipo == EL_LED ? 7 : ancho;
    esq_pt(m, e->xa, e->ya, &xa, &ya);
    esq_pt(m, e->xb, e->yb, &xb, &yb);
    w1 = gfx_text_w(l1, 1);
    w2 = gfx_text_w(l2, 1);

#define ROTULO(x, y, s, c) do { int x_ = (x), y_ = (y); \
                                if (medir_) {} \
                                else if (chica) gfx_text_p_bg(x_, y_, s, c, C_FONDO); \
                                else gfx_text_bg(x_, y_, s, c, C_FONDO, 1); \
                                anota_caja(x_, y_, gfx_text_w(s, 1), h); } while (0)
    if (ya == yb) {
        /* la de arriba no sale por encima del recorte (la barra de título,
         * con el elemento en la primera fila): sola, va debajo; con la
         * segunda, se arrima al borde */
        int cx = (xa + xb) / 2, y1 = ya - h - 3 - ancho;
        if (y1 < gfx_clip_arriba()) y1 = l2[0] ? gfx_clip_arriba() : ya + 5 + bajo;
        if (l1[0]) ROTULO(dentro(cx - w1 / 2, w1), y1, l1, c1);
        if (l2[0]) ROTULO(dentro(cx - w2 / 2, w2), ya + 5 + bajo, l2, c2);
    } else {
        int cy = (ya + yb) / 2;
        int dos = l1[0] && l2[0];
        int izq = (lado_vertical(c, e) < 0);
        if (l1[0]) ROTULO(lado_x(xa, w1, izq, 7 + ancho), dos ? cy - h : cy - h / 2, l1, c1);
        if (l2[0]) ROTULO(lado_x(xa, w2, izq, 7 + ancho), dos ? cy : cy - h / 2, l2, c2);
    }
#undef ROTULO
}

/* Los rótulos de un 3 patas (§4.7): el AO, dentro del triángulo y centrados
 * en el local (16,16) (la 6×12 si caben en 3 caracteres; si no, la 6×8 con
 * fondo); el BJT y el MOSFET, al lado +u de la línea C-E, a la altura de la
 * base */
static void rotula3(const Marco *m, const Elemento *e, const char *l1, u16 c1, const char *l2, u16 c2)
{
    Local L;
    int px, py, w1, w2, h, x1, x2, y1, y2, chica, ao = e->tipo == EL_AO;
    int dos = l1[0] && l2[0];
    if (!local_de(m, e, &L)) return;
    if (ao) {
        chica = m->py < 16 || strlen(l1) > 3 || strlen(l2) > 3;
        lp(&L, 16, 16, &px, &py);
    } else {
        chica = (m->py < 24 && dos) || m->py < 16;
        lp(&L, 16, 0, &px, &py);
    }
    h = chica ? FONT_H_P : FONT_H;
    w1 = gfx_text_w(l1, 1);
    w2 = gfx_text_w(l2, 1);
    if (ao || L.ux == 0) { x1 = px - w1 / 2; x2 = px - w2 / 2; }      /* centrados */
    else if (L.ux > 0)   { x1 = x2 = px + 4; }
    else                 { x1 = px - 4 - w1; x2 = px - 4 - w2; }
    if (ao || L.uy == 0) { y1 = dos ? py - h : py - h / 2; y2 = dos ? py : py - h / 2; }
    else if (L.uy > 0)   { y1 = py + 4; y2 = dos ? py + 4 + h : py + 4; }
    else                 { y2 = py - 4 - h; y1 = dos ? y2 - h : y2; }
    {                                                /* nada por encima del recorte: se baja */
        int arr = gfx_clip_arriba(), ymin = l1[0] ? y1 : y2;
        if (ymin < arr) { y1 += arr - ymin; y2 += arr - ymin; }
    }
#define ROTULO3(x, y, s, c) do { int x_ = (x), y_ = (y); \
                                 if (medir_) {} \
                                 else if (chica) gfx_text_p_bg(x_, y_, s, c, C_FONDO); \
                                 else if (ao) gfx_text(x_, y_, s, c, 1); \
                                 else gfx_text_bg(x_, y_, s, c, C_FONDO, 1); \
                                 anota_caja(x_, y_, gfx_text_w(s, 1), h); } while (0)
    if (l1[0]) ROTULO3(dentro(x1, w1), y1, l1, c1);
    if (l2[0]) ROTULO3(dentro(x2, w2), y2, l2, c2);
#undef ROTULO3
}

/* Los rótulos de la capa Valores de los tipos de la Etapa 4. Sin resolver:
 * esq_valor_c (el LED, su color en su color) y la línea extra de siempre.
 * Resueltos: diodo y zener, su región y su V; LED, su I y su brillo en %
 * («apagado», «¡quema!»); controlada, su V (E) o su I (J) y su ley; AO, «Vo»
 * y su región; BJT y MOSFET, su región y su IC o su ID. */
static void rotulo_e4(const Circuito *c, const Vista *v, int k, char *l1, char *l2, u16 *c1, u16 *c2)
{
    const Elemento *e = &c->el[k];
    const Valores *s = v->val;
    Buf b;
    char t[24];
    if (!s) {
        esq_valor_c(c, 0, k, l1, 24);
        if (e->tipo == EL_LED) *c1 = led_rgb(e);
        if (e->val.est == M_BUSCAR) *c1 = C_INCOG;
        extra(e, l2, 24, c2);
        return;
    }
    switch (e->tipo) {
    case EL_DIODO: case EL_ZENER:
        buf_init(&b, l1, 24); buf_str(&b, esq_region(s->reg[k]));
        *c1 = esq_region_col(s->reg[k]);
        con_unidad_s(l2, 24, s->V[k], "V", 1);
        *c2 = C_TENSION;
        break;
    case EL_LED: {
        double If, Im;
        int n;
        esq_led_if_im(e, &If, &Im);
        n = esq_led_nivel(s->I[k], If, Im);
        num3(l1, 24, fabs(s->I[k]), "A");
        *c1 = C_CORRIENTE;
        buf_init(&b, l2, 24);
        if (n == 0) { buf_str(&b, "apagado"); *c2 = C_SUAVE; }
        else if (n == 9) { buf_str(&b, S_IE "quema!"); *c2 = C_ERROR; }
        else {
            double p = 100.0 * s->I[k] / If;
            buf_int(&b, p < 999.0 ? (long)(p + 0.5) : 999L);
            buf_str(&b, " %");
            *c2 = C_TEXTO;
        }
        break;
    }
    case EL_EDEP: case EL_JDEP:
        if (e->tipo == EL_EDEP) con_unidad_s(l1, 24, s->V[k], "V", 1);
        else con_unidad_s(l1, 24, s->I[k], "A", 1);
        esq_valor_c(c, s, k, l2, 24);
        *c2 = e->val.est == M_BUSCAR ? C_INCOG : C_TEXTO;
        break;
    case EL_AO:
        buf_init(&b, l1, 24); buf_str(&b, "Vo ");
        num3(t, sizeof t, s->V[k], "V"); buf_str(&b, t);
        *c1 = C_TENSION;
        buf_init(&b, l2, 24); buf_str(&b, esq_region(s->reg[k]));
        *c2 = esq_region_col(s->reg[k]);
        break;
    default:                                         /* BJT, MOSFET */
        buf_init(&b, l1, 24); buf_str(&b, esq_region(s->reg[k]));
        *c1 = esq_region_col(s->reg[k]);
        buf_init(&b, l2, 24); buf_str(&b, e->tipo == EL_BJT ? "Ic " : "Id ");
        num3(t, sizeof t, s->I[k], "A"); buf_str(&b, t);
        *c2 = C_CORRIENTE;
        break;
    }
}

/* Un fasor interno de la CA en el convenio de la vista, corto */
static void fasor_vista(char *buf, int cap, const Vista *v, Cx X, const char *u)
{
    double mod, ang;
    ca_a_conv(v->conv, X, &mod, &ang);
    num_fasor(buf, (size_t)cap, mod, ang, v->rect, 1, u);
}

/* Los rótulos de un resultado de CA (P_CA) */
static void rotulo_ca(const Vista *v, const Circuito *c, int k, char *l1, char *l2, u16 *c1, u16 *c2)
{
    const Elemento *e = &c->el[k];
    const CaRes *r = v->ca;
    char t[24];
    Buf b;
    int hallado = (r->buscado >> k) & 1u;
    switch (v->capa) {
    case CAPA_VALORES:
        if (e->tipo >= EL_DIODO) {                   /* Etapa 4: su modelo o su ley, como sin resolver */
            esq_valor_c(c, 0, k, l1, 24);
            if (e->tipo == EL_LED) *c1 = led_rgb(e);
            break;
        }
        switch (e->tipo) {
        case EL_FUENTE: case EL_CORR: {
            Mag m;
            m.v = r->val[k]; m.ang = r->val_ang[k]; m.est = M_DATO; m.est_ang = M_DATO;
            fasor_guardado(l1, 24, &m, tipo_uni_val(e->tipo));
            buf_init(&b, l2, 24); num_corto(t, sizeof t, r->f); buf_str(&b, t); buf_str(&b, "Hz");
            *c2 = C_SUAVE;
            break;
        }
        case EL_IMP:
            num_fasor(l1, 24, cx_abs(r->Z[k]), cx_arg(r->Z[k]) * 180.0 / 3.14159265358979323846, 1, 1, S_OHM);
            break;
        case EL_VOLT: case EL_AMP:
            buf_init(&b, l1, 24); buf_ch(&b, '~');
            num_pref(t, sizeof t, r->lectura[k], 3, e->tipo == EL_VOLT ? "V" : "A"); buf_str(&b, t);
            *c1 = C_INSTR;
            break;
        case EL_VATI:
            num_pref(l1, 24, r->lectura[k], 3, "W");
            buf_init(&b, l2, 24); buf_str(&b, "fp"); num_auto(t, sizeof t, fabs(r->fp[k]), 2); buf_str(&b, t);
            buf_ch(&b, r->fp[k] < 0.0 ? 'c' : 'i');
            *c1 = C_INSTR; *c2 = C_SUAVE;
            break;
        case EL_INTER:
            break;
        default:
            num_pref(l1, 24, r->val[k], 3, tipo_uni_val(e->tipo));
            if (e->tipo == EL_COND || e->tipo == EL_BOB) {
                buf_init(&b, l2, 24);
                if (r->Z[k].im < 0.0) buf_ch(&b, '-');
                buf_ch(&b, 'j');
                num_pref(t, sizeof t, fabs(r->Z[k].im), 3, S_OHM); buf_str(&b, t);
                *c2 = C_SUAVE;
            }
            if (hallado) *c1 = C_INCOG;
            break;
        }
        break;
    case CAPA_CORRIENTES: fasor_vista(l1, 24, v, r->I[k], "A"); *c1 = C_CORRIENTE; break;
    case CAPA_TENSIONES:  fasor_vista(l1, 24, v, r->V[k], "V"); *c1 = C_TENSION; break;
    case CAPA_POTENCIAS: {
        /* sin el ruido del redondeo (1e-12 de la escala), y solo lo que no es 0 */
        double esc = cx_abs(r->Sfuentes) + cx_abs(r->Scargas) + 1e-300;
        double P = fabs(r->S[k].re) <= 1e-12 * esc ? 0.0 : r->S[k].re, Q = fabs(r->S[k].im) <= 1e-12 * esc ? 0.0 : r->S[k].im;
        char *l = l1;
        u16 *cl = c1;
        if (e->tipo == EL_AMP || e->tipo == EL_INTER || e->tipo == EL_VATI || (e->tipo == EL_VOLT && e->val.est != M_DATO)) break;
        if (P != 0.0 || Q == 0.0) {
            buf_init(&b, l, 24); buf_str(&b, "P "); num_pref(t, sizeof t, P, 3, "W"); buf_str(&b, t);
            *cl = C_POTENCIA; l = l2; cl = c2;
        }
        if (Q != 0.0) { buf_init(&b, l, 24); buf_str(&b, "Q "); num_pref(t, sizeof t, Q, 3, "var"); buf_str(&b, t); *cl = C_REACT; }
        break;
    }
    default:
        break;
    }
}

static void rotulo_elemento(const Marco *m, const Circuito *c, const Vista *v, int k, u16 base)
{
    const Elemento *e = &c->el[k];
    char l1[24] = "", l2[24] = "";
    u16 c1 = base, c2 = C_TEXTO;
    int busc = (e->val.est == M_BUSCAR);

    rot_k_ = k;
    frec_ = &c->frec;
    if (v->ca && v->capa != CAPA_NOMBRES && v->capa != CAPA_POTENCIALES) {
        rotulo_ca(v, c, k, l1, l2, &c1, &c2);
        if (v->atenuar && !(v->resalta & (1u << k))) { c1 = C_TENUE; c2 = C_TENUE; }
        if (circ_npatas(e) == 3) rotula3(m, e, l1, c1, l2, c2);
        else rotula(m, c, e, l1, c1, l2, c2);
        return;
    }

    switch (v->capa) {
    case CAPA_POTENCIALES:                   /* los números van en los nodos */
        break;
    case CAPA_NOMBRES:
        {
            int i = 0;
            while (e->etiq[i] && i < 5) { l1[i] = e->etiq[i]; i++; }
            l1[i] = 0;
        }
        c1 = C_COTA;
        break;
    case CAPA_VALORES:
        if (e->tipo >= EL_DIODO) { rotulo_e4(c, v, k, l1, l2, &c1, &c2); break; }
        esq_valor(e, v->val, k, l1, sizeof l1);
        if (v->lectura && (e->tipo == EL_VOLT || e->tipo == EL_AMP)) {
            /* tras emular, lo que marca en su modo (~ en alterna) */
            char t[20];
            Buf b;
            con_unidad_s(t, sizeof t, v->lectura[k], e->tipo == EL_VOLT ? "V" : "A", 1);
            buf_init(&b, l1, sizeof l1);
            if (e->variante == MED_CA) buf_ch(&b, '~');
            buf_str(&b, t);
        }
        if (busc) c1 = C_INCOG;
        if (!v->val) extra(e, l2, sizeof l2, &c2);
        else if (e->tipo == EL_FUENTE && e->aux.est != M_LIBRE) {   /* la r de una fuente real */
            char t[16];
            Buf b;
            num_corto(t, sizeof t, v->val->aux[k]);
            buf_init(&b, l2, sizeof l2);
            buf_str(&b, "r="); buf_str(&b, t); buf_str(&b, S_OHM);
            c2 = e->aux.est == M_BUSCAR ? C_INCOG : C_TEXTO;
        }
        break;
    case CAPA_CORRIENTES:
        if (v->val) con_unidad(l1, sizeof l1, v->val->I[k], "A");
        c1 = C_CORRIENTE;
        break;
    case CAPA_TENSIONES:
        if (v->val) con_unidad(l1, sizeof l1, v->val->V[k], "V");
        c1 = C_TENSION;
        break;
    default:
        if (v->val) con_unidad(l1, sizeof l1, v->val->P[k], "W");
        c1 = C_POTENCIA;
        break;
    }
    if (v->atenuar && !(v->resalta & (1u << k))) { c1 = C_TENUE; c2 = C_TENUE; }
    if (circ_npatas(e) == 3) rotula3(m, e, l1, c1, l2, c2);
    else rotula(m, c, e, l1, c1, l2, c2);
}

/* Las sondas: NODO, un disco con una banderita y su cifra; I, una flecha
 * en el sentido positivo; V, el + y el - junto a los extremos. La banderita
 * va arriba a la derecha; si ahí tapa algo (el rótulo de al lado: «1kΩ»; un
 * punto de la rejilla no cuenta), arriba a la izquierda, abajo a la derecha
 * o abajo a la izquierda: la que menos tape */
static void sondas(const Circuito *c, const Marco *m)
{
    static const signed char FX[4] = { 1, -1, 1, -1 }, FY[4] = { -1, -1, 1, 1 };
    int i;
    for (i = 0; i < 2; i++) {
        const Sonda *s = &c->sonda[i];
        u16 col = i ? C_CANAL2 : C_CANAL1;
        char t[2];
        int sx, sy;
        t[0] = (char)('1' + i); t[1] = 0;
        if (s->tipo == SONDA_NODO) {
            int d, mejor = -1, bx = 0, by = 0, px = 0;
            esq_pt(m, s->x, s->y, &sx, &sy);
            for (d = 0; d < 4 && mejor != 0; d++) {
                int x = FX[d] > 0 ? sx + 4 : sx - 11, y = FY[d] < 0 ? sy - 13 : sy + 5;
                int nota = gfx_ocupados(x, y, 8, 9, C_FONDO);
                if (d == 0 && nota <= 2) nota = 0;
                if (mejor < 0 || nota < mejor) { mejor = nota; bx = x; by = y; px = sx + 3 * FX[d]; }
            }
            gfx_circle(sx, sy, 3, col, 1);
            gfx_vline(px, by < sy ? sy - 13 : sy + 3, 11, col);
            gfx_fill(bx, by, 8, 9, col);
            gfx_text_p(bx + 1, by, t, C_FONDO);
        } else if ((s->tipo == SONDA_I || s->tipo == SONDA_V) && s->el >= 0 && s->el < c->nel &&
                   circ_npatas(&c->el[s->el]) == 2) {   /* sin sondas en las patas (D18) */
            const Elemento *e = &c->el[s->el];
            int xa, ya, xb, yb, lado;
            esq_pt(m, e->xa, e->ya, &xa, &ya);
            esq_pt(m, e->xb, e->yb, &xb, &yb);
            lado = ya == yb ? 1 : lado_vertical(c, e);
            if (s->tipo == SONDA_I) {
                flecha(xa, ya, xb, yb, s->sg < 0 ? -1 : 1, lado, col);
                if (ya == yb) gfx_text_p((xa + xb) / 2 + 9, ya + 5, t, col);
                else gfx_text_p(xa - 10 * lado - 3, (ya + yb) / 2 + 9, t, col);
            } else {
                /* + en a si sg = +1 (V = phi(a) - phi(b)); si no, en b */
                int px = s->sg < 0 ? xb : xa, py = s->sg < 0 ? yb : ya, qx = s->sg < 0 ? xa : xb, qy = s->sg < 0 ? ya : yb;
                int dx = (qx > px) - (qx < px), dy = (qy > py) - (qy < py);
                int ox = ya == yb ? 0 : -9 * lado, oy = ya == yb ? 8 : 0;
                gfx_text_p(px + 6 * dx + ox - 2, py + 6 * dy + oy - 4, "+", col);
                gfx_text_p(qx - 6 * dx + ox - 2, qy - 6 * dy + oy - 4, "-", col);
                gfx_text_p((xa + xb) / 2 + ox - 2 + (ya == yb ? 0 : 0), (ya + yb) / 2 + oy - 4 + (ya == yb ? 0 : 0), t, col);
            }
        }
    }
}

/* Las letras de los nodos en los pasos de Kirchhoff (las ecuaciones hablan
 * de VA, VB...). El esquema de los pasos va apretado y lleno de rótulos: se
 * dibujan al final, encima de todo, y cada una donde no tape nada. Se
 * prueban, punto a punto del nodo (los que tienen algo conectado), cuatro
 * sitios en diagonal. Vale el primero libre del todo (con una columna de
 * aire a la izquierda: el glifo ya deja la de la derecha y las filas de
 * arriba y abajo) y con dos columnas más de aire a cada lado (que no se
 * lea pegada a un rótulo: «BR1»); si no, el primero libre; si ninguno lo
 * está, el que menos tapa, con su fondo. El nodo del paso, el primero y en
 * C_RESALTA; los demás en C_TENSION, como los potenciales (ni gris, como
 * los nombres, ni amarilla, como los terminales). Un punto marca el sitio
 * del nodo al que va la letra. */
static void letras_pasos(const Circuito *c, const Marco *m, const Vista *v)
{
    /* los cuatro sitios en diagonal y, detrás, los mismos 2 px más afuera:
     * con un símbolo al lado del punto (la R de «Dos fuentes»), la letra no
     * se queda pegada a él («B≡R1») si 2 px más allá tiene aire */
    static const signed char DX[8] = { 4, 4, -10, -10, 6, 6, -12, -12 }, DY[8] = { -14, 3, -14, 3, -14, 3, -14, 3 };
    int orden[MAX_NODOS], no = 0, i, nn = v->red->nn < MAX_NODOS ? v->red->nn : MAX_NODOS;
    if (v->nodo_resalta >= 0 && v->nodo_resalta < nn) orden[no++] = v->nodo_resalta;
    for (i = 0; i < nn; i++) if (i != v->nodo_resalta) orden[no++] = i;
    for (i = 0; i < no; i++) {
        int n = orden[i], p, d, r, sx, sy, mejor = -1, bx = 0, by = 0, bpx = 0, bpy = 0;
        u16 cl = n == v->nodo_resalta ? C_RESALTA : C_TENSION;
        char l[2];
        for (r = 0; r < 2 && mejor != 0; r++)          /* los de siempre en todos los puntos; luego, los de afuera */
            for (p = 0; p < REJ_PUNTOS && mejor != 0; p++) {
                if (v->red->punto[p] != n || circ_grado(c, p % REJ_COLS, p / REJ_COLS) <= 0) continue;
                esq_pt(m, p % REJ_COLS, p / REJ_COLS, &sx, &sy);
                for (d = 4 * r; d < 4 * r + 4 && mejor != 0; d++) {
                    int x = sx + DX[d], y = sy + DY[d];
                    int nota = 2 * gfx_ocupados(x - 1, y, FONT_W + 1, FONT_H, C_FONDO) +
                               (gfx_ocupados(x - 2, y, FONT_W + 4, FONT_H, C_FONDO) > 0);
                    if (mejor < 0 || nota < mejor) { mejor = nota; bx = x; by = y; bpx = sx; bpy = sy; }
                }
            }
        if (mejor < 0) continue;                   /* sin puntos conectados */
        gfx_circle(bpx, bpy, 2, cl, 1);
        if (mejor >= 2) gfx_fill(bx, by, FONT_W, FONT_H, C_FONDO);  /* tapa algo: que se lea */
        l[0] = red_letra(v->red, n); l[1] = 0;
        gfx_text(bx, by, l, cl, 1);
    }
}

/* El recuadro del elemento elegido (el editor y su ficha): su símbolo con
 * ±10 px (±13 lo que asoma más: el + de la pila, las flechitas del LED, la
 * tierra del vatímetro; ±18 la forma de una J; la caja de un 3 patas, ±8)
 * y sus rótulos con ±2, que se miden sin pintarlos. Va antes que los
 * elementos: los símbolos y los rótulos quedan encima de su borde, y los
 * suyos, dentro */
static void recuadro(const Circuito *c, const Marco *m, const Vista *v, int k)
{
    const Elemento *e = &c->el[k];
    int x0, y0, x1, y1, i, n0 = ncajas_;
    if (circ_npatas(e) == 3) {
        circ_caja(e, &x0, &y0, &x1, &y1);
        esq_pt(m, x0, y0, &x0, &y0);
        esq_pt(m, x1, y1, &x1, &y1);
        x0 -= 8; y0 -= 8; x1 += 8; y1 += 8;
    } else {
        int xa, ya, xb, yb, mg = 10;
        if ((e->tipo == EL_FUENTE && e->variante != FORMA_FASOR && (e->variante == ONDA_CC || e->variante >= ONDA_N)) ||
            e->tipo == EL_LED || e->tipo == EL_VATI) mg = 13;
        else if (e->tipo == EL_CORR && e->variante != ONDA_CC && e->variante < FORMA_N) mg = 18;
        esq_pt(m, e->xa, e->ya, &xa, &ya);
        esq_pt(m, e->xb, e->yb, &xb, &yb);
        x0 = (xa < xb ? xa : xb) - mg; y0 = (ya < yb ? ya : yb) - mg;
        x1 = (xa < xb ? xb : xa) + mg; y1 = (ya < yb ? yb : ya) + mg;
    }
    medir_ = 1;
    rotulo_elemento(m, c, v, k, tipo_info(e->tipo)->color);
    medir_ = 0;
    for (i = n0; i < ncajas_; i++) {
        const EsqCaja *b = &cajas_[i];
        if (b->x - 2 < x0) x0 = b->x - 2;
        if (b->y - 2 < y0) y0 = b->y - 2;
        if (b->x + b->w + 1 > x1) x1 = b->x + b->w + 1;
        if (b->y + b->h + 1 > y1) y1 = b->y + b->h + 1;
    }
    ncajas_ = n0;                         /* se anotan al pintarlos, al final */
    if (y0 < gfx_clip_arriba()) y0 = gfx_clip_arriba();  /* su borde de arriba, a la vista (bajo la barra) */
    gfx_rect(x0, y0, x1 - x0 + 1, y1 - y0 + 1, C_CURSOR);
}

/* --------------------------------------------------------------------------
 *  Dibujo
 * ----------------------------------------------------------------------- */
void esq_dibuja(const Circuito *c, const Marco *m, const Vista *v)
{
    int gx, gy, k, sx, sy, tx, ty;
    int tierra_d = -1;                    /* hacia dónde sale el símbolo de tierra */
    signed char dueno[REJ_PUNTOS], pata[REJ_PUNTOS];

    circ_mapa_huellas(c, dueno, pata);    /* los puntos interiores de una caja */
    ncajas_ = 0;
    if (v->rejilla)
        for (gy = 0; gy < REJ_FILAS; gy++)
            for (gx = 0; gx < REJ_COLS; gx++) {
                int i = gy * REJ_COLS + gx;
                if (dueno[i] >= 0 && pata[i] < 0) continue;     /* interior: no se pinta */
                esq_pt(m, gx, gy, &sx, &sy);
                gfx_px(sx, sy, C_REJILLA);
                gfx_px(sx + 1, sy, C_REJILLA);
                gfx_px(sx, sy + 1, C_REJILLA);
                gfx_px(sx + 1, sy + 1, C_REJILLA);
            }

    /* Cables */
    for (gy = 0; gy < REJ_FILAS; gy++)
        for (gx = 0; gx < REJ_COLS; gx++) {
            esq_pt(m, gx, gy, &sx, &sy);
            if (gx < REJ_COLS - 1 && c->h[gy][gx] == SEG_CABLE) {
                esq_pt(m, gx + 1, gy, &tx, &ty);
                gfx_line(sx, sy, tx, ty, C_CABLE);
            }
            if (gy < REJ_FILAS - 1 && c->v[gy][gx] == SEG_CABLE) {
                esq_pt(m, gx, gy + 1, &tx, &ty);
                gfx_line(sx, sy, tx, ty, C_CABLE);
            }
        }

    /* El recuadro del elegido, debajo de todo lo demás */
    if (v->sel >= 0 && v->sel < c->nel) recuadro(c, m, v, v->sel);

    /* Elementos */
    for (k = 0; k < c->nel; k++) {
        const Elemento *e = &c->el[k];
        u16 col = tipo_info(e->tipo)->color;
        int xa, ya, xb, yb;

        if (v->resalta & (1u << k)) col = C_RESALTA;
        else if (v->atenuar) col = C_TENUE;

        if (circ_npatas(e) == 3) {                  /* AO, BJT, MOSFET: su caja, su marco local */
            Local L;
            if (local_de(m, e, &L)) {
                if (e->tipo == EL_AO) ao_sim(&L, col, 1);
                else if (e->tipo == EL_BJT) bjt_sim(&L, col, e->variante == BJT_PNP, 5);
                else mos_sim(&L, col, e->variante == MOS_CP, 5);
            }
            continue;                               /* sin flechas de corriente */
        }

        esq_pt(m, e->xa, e->ya, &xa, &ya);
        esq_pt(m, e->xb, e->yb, &xb, &yb);

        switch (e->tipo) {                          /* el símbolo de cada tipo */
        case EL_FUENTE:
            if (e->variante == FORMA_FASOR) generador(xa, ya, xb, yb, col, ONDA_SENO);
            else if (e->variante != ONDA_CC && e->variante < ONDA_N) generador(xa, ya, xb, yb, col, e->variante);
            else fuente(xa, ya, xb, yb, col, ya == yb ? 1 : lado_vertical(c, e));
            break;
        case EL_CORR:
            fuente_i(xa, ya, xb, yb, col);
            if (e->variante != ONDA_CC && e->variante < FORMA_N) {
                /* la forma, fuera del círculo, del lado contrario al rótulo */
                int f = e->variante == FORMA_FASOR ? ONDA_SENO : e->variante;
                if (ya == yb) glifo_onda((xa + xb) / 2, ya + 13, f, col);
                else glifo_onda(xa - 13 * lado_vertical(c, e), (ya + yb) / 2, f, col);
            }
            break;
        case EL_IMP:    impedancia(xa, ya, xb, yb, col); break;
        case EL_VATI:
            instrumento(xa, ya, xb, yb, col, "W");
            bobina_tension(xb, yb, xa, ya, col, ya == yb ? 1 : lado_vertical(c, e));
            break;
        case EL_VOLT:   instrumento(xa, ya, xb, yb, col, "V"); break;
        case EL_AMP:    instrumento(xa, ya, xb, yb, col, "A"); break;
        case EL_COND:
            condensador(xa, ya, xb, yb, col, e->par[Q_V0 - Q_P1].est == M_DATO && e->par[Q_V0 - Q_P1].v != 0.0);
            break;
        case EL_BOB:
            bobina(xa, ya, xb, yb, col, e->par[Q_I0 - Q_P1].est == M_DATO && e->par[Q_I0 - Q_P1].v != 0.0);
            break;
        case EL_INTER:  interruptor(xa, ya, xb, yb, col, e->variante == INTER_CERRADO); break;
        case EL_DIODO:  diodo(xa, ya, xb, yb, col, col, 0, 0, 5, 0); break;
        case EL_ZENER:  diodo(xa, ya, xb, yb, col, col, 0, 0, 5, 1); break;
        case EL_LED: {                              /* encendido solo con brillo (y sin atenuar) */
            int niv = v->brillo && !(v->atenuar && !(v->resalta & (1u << k))) ? v->brillo[k] : -1;
            led(xa, ya, xb, yb, col, led_rgb(e), niv, ya == yb ? 1 : lado_vertical(c, e));
            break;
        }
        case EL_EDEP:   rombo(xa, ya, xb, yb, col, 7, 0); break;
        case EL_JDEP:   rombo(xa, ya, xb, yb, col, 7, 1); break;
        default:        resistencia(xa, ya, xb, yb, col); break;
        }

        /* En la fuente no hace falta: sale por el + y su rotulo lo dice */
        if (v->capa == CAPA_CORRIENTES && v->val && !v->ca && circ_v_convenio(e->tipo) != -1) {
            double I = v->val->I[k];
            if (fabs(I) > 1e-12)
                flecha(xa, ya, xb, yb, I > 0 ? 1 : -1, ya == yb ? 1 : lado_vertical(c, e),
                       (v->atenuar && !(v->resalta & (1u << k))) ? C_TENUE : C_CORRIENTE);
        }
    }

    /* Tierra: el punto de 0 V. El símbolo sale hacia un lado libre (abajo,
     * arriba, derecha o izquierda) para no pisar lo que llega al punto. */
    if (c->tierra_x >= 0 && c->tierra_y >= 0) {
        static const int ORDEN[4] = { DIR_ABA, DIR_ARR, DIR_DER, DIR_IZQ };
        int i, d = DIR_ABA, sg;
        for (i = 0; i < 4; i++) if (circ_seg(c, c->tierra_x, c->tierra_y, ORDEN[i]) <= 0) { d = ORDEN[i]; break; }
        tierra_d = d;
        esq_pt(m, c->tierra_x, c->tierra_y, &sx, &sy);
        sg = (d == DIR_ABA || d == DIR_DER) ? 1 : -1;
        if (d == DIR_ABA || d == DIR_ARR) {
            gfx_vline(sx, sg > 0 ? sy : sy - 7, 8, C_TEXTO);
            for (i = 0; i < 3; i++) gfx_hline(sx - 6 + 2 * i, sy + sg * (7 + 2 * i), 13 - 4 * i, C_TEXTO);
        } else {
            gfx_hline(sg > 0 ? sx : sx - 7, sy, 8, C_TEXTO);
            for (i = 0; i < 3; i++) gfx_vline(sx + sg * (7 + 2 * i), sy - 6 + 2 * i, 13 - 4 * i, C_TEXTO);
        }
    }

    /* Terminales A y B: un anillo (la letra, después de los rótulos) */
    for (k = 0; k < 2; k++)
        if (c->term_x[k] >= 0 && c->term_y[k] >= 0) {
            esq_pt(m, c->term_x[k], c->term_y[k], &sx, &sy);
            gfx_circle(sx, sy, 3, C_CURSOR, 0);
        }

    /* Potenciales: el de cada nodo con su letra delante («B 7,5V»: la de
     * los pasos, V(AB), la leyenda del osciloscopio y la tabla de CA),
     * junto a su primer punto; todo en C_TENSION, que la letra no se tome
     * por la de un terminal (A y B, en C_CURSOR). Primero los puntos y
     * luego los rótulos: cada uno va arriba a la derecha (o donde no pise
     * la tierra) y, si ahí pisa el rótulo o el punto de otro nodo (los
     * fasores de la CA son largos), abajo a la derecha, arriba a la
     * izquierda o abajo a la izquierda. De esos, el primero con aire (un
     * píxel libre alrededor: que no se pegue a un símbolo, «C 12V +»); si
     * ninguno lo tiene, los mismos 2 px más afuera; si tampoco, el primero
     * que no pisa. (Las letras solas de los pasos de Kirchhoff van al
     * final: letras_pasos.) */
    if (v->red && v->capa == CAPA_POTENCIALES && (v->val || v->ca)) {
        static short px_[MAX_NODOS], py_[MAX_NODOS], pp_[MAX_NODOS], ox[MAX_NODOS], oy[MAX_NODOS], ow[MAX_NODOS];
        int n, p, i, j, nr = 0;
        int nn = v->red->nn < MAX_NODOS ? v->red->nn : MAX_NODOS;
        for (n = 0; n < nn; n++) {
            for (p = 0; p < REJ_PUNTOS; p++) if (v->red->punto[p] == n) break;
            pp_[n] = -1;
            if (p < REJ_PUNTOS) {
                esq_pt(m, p % REJ_COLS, p / REJ_COLS, &sx, &sy);
                px_[n] = (short)sx; py_[n] = (short)sy; pp_[n] = (short)p;
                gfx_circle(sx, sy, 2, n == v->nodo_resalta ? C_RESALTA : C_TENSION, 1);
            }
        }
        for (n = 0; n < nn; n++) {
            char t[24], l[2];
            int w, cx[8], cy[8], primero = -1, elegido = -1;
            u16 cl = n == v->nodo_resalta ? C_RESALTA : C_TENSION;
            if (pp_[n] < 0) continue;
            sx = px_[n]; sy = py_[n]; p = pp_[n];
            l[0] = red_letra(v->red, n); l[1] = 0;
            if (v->ca) fasor_vista(t, sizeof t, v, v->ca->phi[n], "V");
            else con_unidad_s(t, sizeof t, v->val->phi[n], "V", 1);   /* con signo: es un potencial (y sin «-0») */
            w = FONT_W + 3 + gfx_text_w(t, 1);
            /* sin pisar el símbolo de tierra, si sale hacia arriba o a la derecha */
            if (p == c->tierra_y * REJ_COLS + c->tierra_x && tierra_d == DIR_ARR) { cx[0] = dentro(sx + 4, w); cy[0] = sy + 3; }
            else if (p == c->tierra_y * REJ_COLS + c->tierra_x && tierra_d == DIR_DER) { cx[0] = dentro(sx - 4 - w, w); cy[0] = sy - 14; }
            else { cx[0] = dentro(sx + 4, w); cy[0] = sy - 14; }
            cx[1] = dentro(sx + 4, w);     cy[1] = sy + 3;
            cx[2] = dentro(sx - 4 - w, w); cy[2] = sy - 14;
            cx[3] = cx[2];                 cy[3] = sy + 3;
            for (i = 0; i < 4; i++) {             /* los mismos, 2 px más afuera */
                cx[i + 4] = dentro(cx[i] < sx ? cx[i] - 2 : cx[i] + 2, w);
                cy[i + 4] = cy[i];
            }
            for (i = 0; i < 8 && elegido < 0; i++) {
                for (j = 0; j < nr; j++)          /* un rótulo ya puesto */
                    if (cx[i] < ox[j] + ow[j] && ox[j] < cx[i] + w && cy[i] < oy[j] + FONT_H && oy[j] < cy[i] + FONT_H) break;
                if (j < nr) continue;
                for (j = 0; j < nn; j++)          /* el punto de otro nodo */
                    if (j != n && pp_[j] >= 0 && px_[j] + 2 >= cx[i] && px_[j] - 2 < cx[i] + w &&
                        py_[j] + 2 >= cy[i] && py_[j] - 2 < cy[i] + FONT_H) break;
                if (j < nn) continue;
                if (i < 4 && primero < 0) primero = i;
                if (gfx_ocupados(cx[i] - 1, cy[i] - 1, w + 2, FONT_H + 2, C_FONDO) == 0) elegido = i;
            }
            i = elegido >= 0 ? elegido : primero >= 0 ? primero : 0;   /* no cabe en ninguno: el de siempre */
            ox[nr] = (short)cx[i]; oy[nr] = (short)cy[i]; ow[nr] = (short)w; nr++;
            gfx_fill(cx[i], cy[i], w, FONT_H, C_FONDO);
            gfx_text(cx[i], cy[i], l, cl, 1);             /* la letra y el valor, dos textos */
            gfx_text(cx[i] + FONT_W + 3, cy[i], t, C_TENSION, 1);
        }
    }

    /* Uniones: tres o mas segmentos en un punto */
    for (gy = 0; gy < REJ_FILAS; gy++)
        for (gx = 0; gx < REJ_COLS; gx++)
            if (circ_grado(c, gx, gy) >= 3) {
                esq_pt(m, gx, gy, &sx, &sy);
                gfx_circle(sx, sy, 2, C_TEXTO, 1);
            }

    /* El nodo del paso: todos sus puntos con algo conectado, encima de las
     * uniones (se ve el nodo entero, cables incluidos) */
    if (v->red && v->nodo_resalta >= 0 && v->nodo_resalta < v->red->nn)
        for (gy = 0; gy < REJ_FILAS; gy++)
            for (gx = 0; gx < REJ_COLS; gx++)
                if (v->red->punto[gy * REJ_COLS + gx] == v->nodo_resalta && circ_grado(c, gx, gy) > 0) {
                    esq_pt(m, gx, gy, &sx, &sy);
                    gfx_circle(sx, sy, 2, C_RESALTA, 1);
                }

    /* La línea del control de una controlada (la ficha y el editor): del
     * centro del rombo al centro del que la manda, discontinua */
    if (v->ctl_de >= 0 && v->ctl_de < c->nel &&
        (c->el[v->ctl_de].tipo == EL_EDEP || c->el[v->ctl_de].tipo == EL_JDEP)) {
        int r = c->el[v->ctl_de].ref;
        if (r >= 0 && r < c->nel && r != v->ctl_de) {
            centro(m, &c->el[v->ctl_de], &sx, &sy);
            centro(m, &c->el[r], &tx, &ty);
            gfx_line_dash(sx, sy, tx, ty, C_CURSOR, 2, 2);
        }
    }

    /* Rotulos al final, encima de todo */
    for (k = 0; k < c->nel; k++)
        rotulo_elemento(m, c, v, k, tipo_info(c->el[k].tipo)->color);
    if (v->sondas) sondas(c, m);

    /* Y encima de ellos, la letra de cada terminal (que no la tape un
     * rótulo): arriba a la izquierda del anillo o, si ahí pisa algo (el
     * rótulo de la fuente: «I=B»), arriba a la derecha, abajo a la
     * izquierda o abajo a la derecha; si todas pisan, la que menos */
    for (k = 0; k < 2; k++)
        if (c->term_x[k] >= 0 && c->term_y[k] >= 0) {
            static const signed char TX[4] = { -11, 5, -11, 5 }, TY[4] = { -14, -14, 3, 3 };
            int d, mejor = -1, bx = 0, by = 0;
            esq_pt(m, c->term_x[k], c->term_y[k], &sx, &sy);
            if (sx < 0 || sx >= SCR_W) continue;
            for (d = 0; d < 4 && mejor != 0; d++) {
                int x = dentro(sx + TX[d], 6), y = sy + TY[d];
                int nota = gfx_ocupados(x - 1, y, FONT_W + 1, FONT_H, C_FONDO);
                if (mejor < 0 || nota < mejor) { mejor = nota; bx = x; by = y; }
            }
            gfx_text_bg(bx, by, k ? "B" : "A", C_CURSOR, C_FONDO, 1);
        }

    /* Las letras de los nodos de los pasos de Kirchhoff, lo último */
    if (v->red && v->letras && !(v->capa == CAPA_POTENCIALES && (v->val || v->ca))) letras_pasos(c, m, v);

    /* Cursor del editor */
    if (v->cur_x >= 0) {
        esq_pt(m, v->cur_x, v->cur_y, &sx, &sy);
        gfx_rect(sx - 3, sy - 3, 7, 7, C_CURSOR);
        gfx_px(sx, sy, C_CURSOR);
    }

    /* El punto que estorba al colocar un 3 patas: un cuadro rojo de 7×7,
     * encima del cursor (a menudo es el mismo punto) */
    if (v->marca_x >= 0 && v->marca_y >= 0) {
        esq_pt(m, v->marca_x, v->marca_y, &sx, &sy);
        gfx_fill(sx - 3, sy - 3, 7, 7, C_ERROR);
    }
}

/* --------------------------------------------------------------------------
 *  Iconos de la barra de herramientas
 * ----------------------------------------------------------------------- */
void esq_icono(int ico, int x, int y, u16 col)
{
    int cy = y + 7, i;
    switch (ico) {
    case ICO_RES:
        resistencia(x, cy, x + 20, cy, col);
        break;
    case ICO_FUENTE:                         /* version corta del simbolo */
        gfx_hline(x, cy, 8, col);
        gfx_vline(x + 8, cy - 3, 7, col);
        gfx_vline(x + 9, cy - 3, 7, col);
        gfx_vline(x + 12, cy - 6, 13, col);
        gfx_hline(x + 12, cy, 8, col);
        gfx_hline(x + 15, cy - 5, 3, col);   /* + */
        gfx_vline(x + 16, cy - 6, 3, col);
        break;
    case ICO_CORR:
        fuente_i(x, cy, x + 20, cy, col);
        break;
    case ICO_VOLT:
        instrumento(x, cy, x + 20, cy, col, "V");
        break;
    case ICO_AMP:
        instrumento(x, cy, x + 20, cy, col, "A");
        break;
    case ICO_TIERRA:
        gfx_vline(x + 10, cy - 6, 7, col);
        for (i = 0; i < 3; i++) gfx_hline(x + 4 + 2 * i, cy + 1 + 2 * i, 13 - 4 * i, col);
        break;
    case ICO_TERM:
        gfx_circle(x + 10, cy, 3, col, 0);
        gfx_text(x + 1, cy - 6, "A", col, 1);
        break;
    case ICO_COND:
        condensador(x, cy, x + 20, cy, col, 0);
        break;
    case ICO_BOB:
        bobina(x, cy + 2, x + 20, cy + 2, col, 0);
        break;
    case ICO_INTER:
        interruptor(x, cy + 2, x + 20, cy + 2, col, 0);
        break;
    case ICO_SONDA:                          /* la punta de una sonda con su gancho */
        gfx_line(x + 3, cy + 5, x + 14, cy - 4, col);
        gfx_line(x + 4, cy + 6, x + 15, cy - 3, col);
        gfx_circle(x + 16, cy - 5, 2, col, 0);
        gfx_circle(x + 3, cy + 5, 1, col, 1);
        break;
    case ICO_IMP:
        impedancia(x, cy, x + 20, cy, col);
        break;
    case ICO_VATI:
        instrumento(x, cy, x + 20, cy, col, "W");
        break;
    case ICO_PALETA:                         /* cuatro casillas */
        for (i = 0; i < 4; i++) gfx_rect(x + 3 + (i % 2) * 8, cy - 6 + (i / 2) * 7, 6, 5, col);
        break;
    case ICO_CABLE:
        gfx_hline(x + 2, cy, 16, col);
        gfx_circle(x + 2, cy, 1, col, 1);
        gfx_circle(x + 18, cy, 1, col, 1);
        break;
    case ICO_BORRAR:
        for (i = 0; i < 2; i++) {
            gfx_line(x + 5 + i, cy - 5, x + 14 + i, cy + 5, col);
            gfx_line(x + 14 + i, cy - 5, x + 5 + i, cy + 5, col);
        }
        break;
    case ICO_EDITAR:                         /* una ficha con renglones */
        gfx_rect(x + 4, cy - 6, 12, 13, col);
        gfx_hline(x + 6, cy - 3, 8, col);
        gfx_hline(x + 6, cy, 8, col);
        gfx_hline(x + 6, cy + 3, 5, col);
        break;
    /* Etapa 4: el símbolo en horizontal, de x a x + 19 (nada fuera de la celda) */
    case ICO_DIODO:
        diodo(x, cy, x + 19, cy, col, col, 0, 0, 5, 0);
        break;
    case ICO_ZENER:
        diodo(x, cy, x + 19, cy, col, col, 0, 0, 5, 1);
        break;
    case ICO_LED:                            /* más pequeño, con las flechitas arriba */
        diodo(x, cy + 2, x + 19, cy + 2, col, col, 0, 0, 4, 0);
        flechitas(x + 9, cy + 2, 1, 0, 0, -1, 4, 3, 2, col);
        break;
    case ICO_EDEP:
        rombo(x, cy, x + 19, cy, col, 6, 0);
        break;
    case ICO_JDEP:
        rombo(x, cy, x + 19, cy, col, 6, 1);
        break;
    case ICO_AO: {                           /* de x + 2 a x + 18 y de y + 1 a y + 13 */
        Local L = { 0, 0, 8, 0, 0, 5, 32, 0 };
        L.ox = x + 2; L.oy = y + 2;
        ao_sim(&L, col, 0);
        break;
    }
    case ICO_BJT: case ICO_MOS: {            /* de x + 3 a x + 17 y de y + 1 a y + 13 */
        Local L = { 0, 0, 14, 0, 0, 6, 0, 0 };
        L.ox = x + 3; L.oy = y + 7;
        if (ico == ICO_BJT) bjt_sim(&L, col, 0, 3);
        else mos_sim(&L, col, 0, 3);
        break;
    }
    default:                                 /* resolver: un signo = */
        gfx_fill(x + 4, cy - 3, 12, 2, col);
        gfx_fill(x + 4, cy + 2, 12, 2, col);
        break;
    }
}

void esq_flechas_cursor_m(int sx, int sy, unsigned mascara, u16 col, u16 col_no)
{
#define COL_D(d) (((mascara >> (d)) & 1u) ? col : col_no)
    gfx_tri(sx + 8, sy - 3, sx + 8, sy + 3, sx + 11, sy, COL_D(DIR_DER));   /* derecha   */
    gfx_tri(sx - 8, sy - 3, sx - 8, sy + 3, sx - 11, sy, COL_D(DIR_IZQ));   /* izquierda */
    gfx_tri(sx - 3, sy + 8, sx + 3, sy + 8, sx, sy + 11, COL_D(DIR_ABA));   /* abajo     */
    gfx_tri(sx - 3, sy - 8, sx + 3, sy - 8, sx, sy - 11, COL_D(DIR_ARR));   /* arriba    */
#undef COL_D
}

void esq_flechas_cursor(int sx, int sy, u16 col)
{
    esq_flechas_cursor_m(sx, sy, 0xFu, col, col);
}
