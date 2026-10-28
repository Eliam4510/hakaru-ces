/* ============================================================================
 *  capmotor_dib.c  -  El cableado de los capacitores equivalentes, como lo
 *  dibuja el profe (figuras 2-4 del PDF del usuario y el pizarrón)
 *
 *    - la clavija arriba a la izquierda (un rectángulo con dos patitas) y su
 *      cable hacia la derecha;
 *    - del extremo del cable, la línea PUNTEADA va siempre directo a C, y la
 *      SÓLIDA llega a la terminal del arreglo que se puentea a T; de ese lado
 *      sale el cable a T;
 *    - la terminal opuesta del arreglo va a A;
 *    - C, A y T son puntos abajo a la izquierda (C un poco más arriba);
 *    - los capacitores, óvalos con dos terminales cuadradas y un circulito,
 *      con su valor al lado («7 MF»); el menor a la izquierda (el lado de A).
 *
 *  Con tres (§5.5 de la especificación v3):
 *    - paralelo de tres: los tres apilados, con las izquierdas unidas por
 *      fuera (a A) y las derechas también (a T);
 *    - serie de tres: la cadena, con dos puntos medios;
 *    - mixto 1, (a ∥ b) en serie con c: como en la v2 (la figura 4);
 *    - mixto 2, (a en serie con b) ∥ c: a y b en serie arriba y c abajo,
 *      unidos por los dos extremos.
 *
 *  Las líneas son curvas de Bézier en tramos rectos, con enteros (sin coma
 *  flotante en el rasterizado: la CX II no tiene FPU). Las coordenadas son
 *  las de la pantalla con el dibujo empezando en y0 = 17 (debajo de la
 *  barra); con otro y0 todo baja o sube igual.
 * ========================================================================= */
#include <string.h>
#include "capmotor.h"
#include "gfx.h"

#define OV_W   60                   /* el óvalo                              */
#define OV_H   22
#define TE_DX  13                   /* las terminales, a ±13 del centro      */
#define TE_L   10                   /* su lado                               */

static int dy;                      /* y0 - 17                               */
static int guion;                   /* la punteada: píxeles desde su inicio  */

#ifndef NDLESS
static CpmCaja cajas[16];
static int ncajas;
int cpm_cajas(CpmCaja *c, int max)
{
    int i;
    for (i = 0; i < ncajas && i < max; i++) c[i] = cajas[i];
    return ncajas;
}
#endif

/* Un tramo recto; punteada: 4 encendidos y 3 apagados, seguidos de un tramo
 * al siguiente (el primer píxel de cada tramo es el último del anterior) */
static void tramo(int x0, int y0, int x1, int y1, u16 c, int punteada, int primero)
{
    int ax = x1 > x0 ? x1 - x0 : x0 - x1, ay = -(y1 > y0 ? y1 - y0 : y0 - y1);
    int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, err = ax + ay, e2;
    for (;;) {
        if (primero) {
            if (!punteada || guion % 7 < 4) gfx_px(x0, y0, c);
            guion++;
        }
        primero = 1;
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= ay) { err += ay; x0 += sx; }
        if (e2 <= ax) { err += ax; y0 += sy; }
    }
}

/* Bézier cúbica de (x0,y0) a (x3,y3) con los puntos de control 1 y 2, en 24
 * tramos (las y, ya con dy) */
static void curva(int x0, int y0, int x1, int y1, int x2, int y2, int x3, int y3, u16 c, int punteada)
{
    enum { N = 24 };
    const long N3 = (long)N * N * N;
    int i, px = x0, py = y0 + dy;
    y0 += dy; y1 += dy; y2 += dy; y3 += dy;
    guion = 0;
    for (i = 1; i <= N; i++) {
        long s = N - i, a = s * s * s, b = 3 * s * s * i, d = (long)i * i * i, e = 3 * s * i * i;
        int x = (int)((a * x0 + b * x1 + e * x2 + d * x3 + N3 / 2) / N3);
        int y = (int)((a * y0 + b * y1 + e * y2 + d * y3 + N3 / 2) / N3);
        tramo(px, py, x, y, c, punteada, i == 1);
        px = x; py = y;
    }
}

/* Los rótulos van al final, encima de todo: se apuntan aquí y los pinta
 * rotulos() (y el arnés mira que debajo de cada uno no hubiera nada) */
#define ROT_N 12
static struct { int x, y, chico; u16 c; char s[24]; } rot[ROT_N];
static int nrot;

static void rotulo(int x, int y, const char *s, u16 c, int chico)
{
    if (nrot >= ROT_N) return;
    rot[nrot].x = x; rot[nrot].y = y + dy; rot[nrot].chico = chico; rot[nrot].c = c;
    strncpy(rot[nrot].s, s, sizeof rot[nrot].s - 1);
    rot[nrot].s[sizeof rot[nrot].s - 1] = 0;
    nrot++;
}

static void rotulos(void)
{
    int i;
    for (i = 0; i < nrot; i++) {
        int x = rot[i].x, y = rot[i].y;
#ifndef NDLESS
        if (ncajas < 16) {                                /* la caja de la tinta: de y + 1 a y + 10 */
            int w = rot[i].chico ? (int)strlen(rot[i].s) * FONT_W : gfx_text_w(rot[i].s, 1);
            int h = rot[i].chico ? FONT_H_P : 10;
            cajas[ncajas].x = x; cajas[ncajas].y = y + 1; cajas[ncajas].w = w; cajas[ncajas].h = h;
            cajas[ncajas].ocupados = gfx_ocupados(x, y + 1, w, h, C_FONDO);
            ncajas++;
        }
#endif
        if (rot[i].chico) gfx_text_p(x, y, rot[i].s, rot[i].c);
        else gfx_text(x, y, rot[i].s, rot[i].c, 1);
    }
    nrot = 0;
}

/* «N MF» */
static void valor(char *t, size_t cap, double v)
{
    size_t n;
    cpm_num(t, cap, v, 3);
    n = strlen(t);
    if (n + 4 < cap) memcpy(t + n, " MF", 4);
}

/* El rótulo del valor de un óvalo de centro (x, y): a la izquierda (lado <
 * 0), a la derecha (1), debajo (0) o, el de arriba del mixto (2), debajo si
 * cabe entre la curva de las izquierdas (x - 31) y su derecha y si no
 * encima del óvalo («120.25 MF» debajo pisaría esa curva y el cable de A;
 * entre los dos óvalos no caben dos líneas). Si a un lado no cabe en una
 * línea («99999.999 MF»: a la derecha se saldría de la pantalla; a la
 * izquierda llegaría a la punteada, que a esa altura pasa por x < 58), en
 * dos: el número y «MF» */
static void rot_valor(int x, int y, double v, int lado)
{
    char t[24], u[20];
    int w, wv, xr;
    valor(t, sizeof t, v);
    cpm_num(u, sizeof u, v, 3);
    w = gfx_text_w(t, 1);
    wv = gfx_text_w(u, 1);
    if (lado < 0) {
        xr = x - OV_W / 2 - 6;                            /* su borde derecho */
        if (xr - w >= 64) rotulo(xr - w, y - 6, t, C_COND, 0);
        else { rotulo(xr - wv, y - 12, u, C_COND, 0); rotulo(xr - 2 * FONT_W, y + 1, "MF", C_COND, 0); }
    } else if (lado == 1) {
        xr = x + OV_W / 2 + 6;
        if (xr + w <= SCR_W - 2) rotulo(xr, y - 6, t, C_COND, 0);
        else {
            if (xr + wv > SCR_W - 2) xr = SCR_W - 2 - wv;
            rotulo(xr, y - 12, u, C_COND, 0);
            rotulo(xr, y + 1, "MF", C_COND, 0);
        }
    } else if (lado == 2 && x - w / 2 < x - TE_DX - 18 + 6) rotulo(x - w / 2, y - OV_H / 2 - 14, t, C_COND, 0);
    else rotulo(x - w / 2, y + OV_H / 2 + 2, t, C_COND, 0);
}

/* Los rótulos de la serie de tres (los óvalos en xs[0..2], a la altura y).
 * Cada uno va de una de estas maneras (m):
 *   0  a la izquierda del óvalo, en una línea (solo el 1.º);
 *   1  a la izquierda, en dos: el número y «MF» (solo el 1.º);
 *   2  encima, en una línea (la fila de abajo, y - 25);
 *   3  encima, en dos: el número en la fila de arriba (y - 37) y «MF» debajo
 *      (el 1.º y el 2.º).
 * A la izquierda, sin llegar a la punteada (que ahí pasa por x < 58); encima,
 * tampoco (x < 72 en esas filas). El 3.º va siempre encima en una línea, sin
 * pasar de xmax (la sólida baja por ahí): el más largo, «99999.999 MF»
 * (72), cabe de 221 a 293, a la derecha del «MF» del 2.º. El número del 2.º
 * en dos líneas acaba antes de x = 234, lejos de la sólida (x > 280 en esa
 * fila). Se prueban por este orden: el 1.º a la izquierda en una línea, en
 * dos, encima en una y en dos; y para cada uno, el 2.º en una y en dos. La
 * primera manera en que nada se monta es la buena (con los valores que
 * admite la app, siempre hay una) */
typedef struct { int x, y, w; const char *s; } Rot;

static int se_montan(const Rot *a, const Rot *b) { return a->y == b->y && a->x < b->x + b->w + 4 && b->x < a->x + a->w + 4; }

static void rot_ser3(const int *xs, const double *vs, int y, int xmax)
{
    char t[3][24], u[3][20];
    int w[3], wv[3], m0, m1, i, j, k;
    for (i = 0; i < 3; i++) {
        valor(t[i], sizeof t[i], vs[i]);
        cpm_num(u[i], sizeof u[i], vs[i], 3);
        w[i] = gfx_text_w(t[i], 1);
        wv[i] = gfx_text_w(u[i], 1);
    }
    for (m0 = 0; m0 < 4; m0++)
        for (m1 = 2; m1 < 4; m1++) {
            Rot r[6];
            int n = 0, ok = 1, m[3];
            m[0] = m0; m[1] = m1; m[2] = 2;
            for (i = 0; i < 3; i++) {
                int x = xs[i], xr = x - OV_W / 2 - 6, a;
                switch (m[i]) {
                case 0:
                    r[n].x = xr - w[i]; r[n].y = y - 6; r[n].w = w[i]; r[n].s = t[i];
                    if (r[n++].x < 64) ok = 0;
                    break;
                case 1:
                    r[n].x = xr - wv[i]; r[n].y = y - 12; r[n].w = wv[i]; r[n].s = u[i];
                    if (r[n++].x < 60) ok = 0;
                    r[n].x = xr - 2 * FONT_W; r[n].y = y + 1; r[n].w = 2 * FONT_W; r[n++].s = "MF";
                    break;
                case 2:
                    a = x - w[i] / 2;
                    if (a + w[i] > xmax) a = xmax - w[i];
                    r[n].x = a; r[n].y = y - OV_H / 2 - 14; r[n].w = w[i]; r[n].s = t[i];
                    if (r[n++].x < 70) ok = 0;
                    break;
                default:
                    a = x - wv[i] / 2;
                    r[n].x = a; r[n].y = y - OV_H / 2 - 26; r[n].w = wv[i]; r[n].s = u[i];
                    if (r[n++].x < 76) ok = 0;
                    r[n].x = x - FONT_W; r[n].y = y - OV_H / 2 - 14; r[n].w = 2 * FONT_W; r[n++].s = "MF";
                    break;
                }
            }
            for (j = 0; ok && j < n; j++)
                for (k = j + 1; k < n; k++) if (se_montan(&r[j], &r[k])) ok = 0;
            if (ok || (m0 == 3 && m1 == 3)) {                 /* (la última, aunque no quepa) */
                for (j = 0; j < n; j++) rotulo(r[j].x, r[j].y, r[j].s, C_COND, 0);
                return;
            }
        }
}

/* El rótulo del de arriba a la izquierda del mixto 2 (centro x, y): a su
 * izquierda si no llega a la punteada (que a esa altura pasa por x < 78);
 * si no, debajo y a la izquierda de su terminal (de ahí baja la curva a c),
 * en una línea o en dos (el número y «MF») */
static void rot_mix2a(int x, int y, double v)
{
    char t[24], u[20];
    int w, wv, xr = x - OV_W / 2 - 6;
    valor(t, sizeof t, v);
    cpm_num(u, sizeof u, v, 3);
    w = gfx_text_w(t, 1);
    wv = gfx_text_w(u, 1);
    if (xr - w >= 78) { rotulo(xr - w, y - 6, t, C_COND, 0); return; }
    xr = x - TE_DX - 7;
    if (xr - w >= 66) { rotulo(xr - w, y + OV_H / 2 + 2, t, C_COND, 0); return; }
    rotulo(xr - wv, y + OV_H / 2 + 2, u, C_COND, 0);
    rotulo(xr - 2 * FONT_W, y + OV_H / 2 + 15, "MF", C_COND, 0);
}

/* Medio círculo, el derecho (der = 1) o el izquierdo, por el punto medio:
 * sus puntas son (cx, cy ± r), donde empiezan las rectas del óvalo */
static void medio(int cx, int cy, int r, int der, u16 c)
{
    int x = r, yy = 0, err = 1 - r, s = der ? 1 : -1;
    while (x >= yy) {
        gfx_px(cx + s * x, cy + yy, c); gfx_px(cx + s * x, cy - yy, c);
        gfx_px(cx + s * yy, cy + x, c); gfx_px(cx + s * yy, cy - x, c);
        yy++;
        if (err < 0) err += 2 * yy + 1;
        else { x--; err += 2 * (yy - x) + 1; }
    }
}

/* El óvalo de centro (x, y) (y sin dy): dos semicírculos y dos rectas, las
 * dos terminales cuadradas y el circulito de arriba */
static void ovalo(int x, int y)
{
    int r = OV_H / 2, xc = OV_W / 2 - r;
#ifndef NDLESS
    if (ncajas < 16) {
        cajas[ncajas].x = x - OV_W / 2; cajas[ncajas].y = y + dy - r; cajas[ncajas].w = OV_W; cajas[ncajas].h = OV_H + 1;
        cajas[ncajas].ocupados = -1;                       /* un óvalo: no es un rótulo */
        ncajas++;
    }
#endif
    y += dy;
    medio(x - xc, y, r, 0, C_COND);
    medio(x + xc, y, r, 1, C_COND);
    gfx_hline(x - xc, y - r, 2 * xc + 1, C_COND);
    gfx_hline(x - xc, y + r, 2 * xc + 1, C_COND);
    gfx_rect(x - TE_DX - TE_L / 2, y - 4, TE_L, TE_L, C_COND);
    gfx_rect(x + TE_DX - TE_L / 2, y - 4, TE_L, TE_L, C_COND);
    gfx_circle(x, y - 7, 1, C_COND, 0);
}

/* Las terminales del óvalo de centro (x, y): sus x, y los bordes de arriba
 * (y - 4) y de abajo (y + 5) del cuadrado, donde llegan los cables */
#define TI(x) ((x) - TE_DX)         /* x de la izquierda */
#define TD(x) ((x) + TE_DX)         /* x de la derecha   */
#define T_ARR(y) ((y) - 4)
#define T_ABA(y) ((y) + 5)

/* Un punto con su letra */
static void punto(int x, int y, const char *letra, int izq)
{
    gfx_circle(x, y + dy, 2, C_TEXTO, 1);
    if (izq) rotulo(x - 12, y - 6, letra, C_TEXTO, 0);
    else rotulo(x - 2, y + 4, letra, C_TEXTO, 0);
}

/* Lo común: la clavija con su cable, C, A y T, y la punteada a C. Del
 * extremo del cable (CAB_X, CAB_Y) sale también la sólida (a_t) */
#define CAB_X 112                   /* el extremo del cable          */
#define CAB_Y 35
#define C_X   44                    /* los puntos C, A y T           */
#define C_Y   150
#define A_X   30
#define A_Y   194
#define TT_X  66
static void comun(void)
{
    gfx_rect(8, 28 + dy, 17, 16, C_TEXTO);                /* la clavija */
    gfx_vline(12, 44 + dy, 8, C_TEXTO);                    /* sus patitas */
    gfx_vline(20, 44 + dy, 8, C_TEXTO);
    gfx_rect(24, 31 + dy, CAB_X - 24 + 1, 9, C_TEXTO);    /* el cable */
    rotulo(40, 19, "clavija y cable", C_SUAVE, 1);
    punto(C_X, C_Y, "C", 1);
    punto(A_X, A_Y, "A", 0);
    punto(TT_X, A_Y, "T", 0);
    /* la punteada: del extremo del cable, abajo y luego a la izquierda, a C */
    curva(CAB_X + 1, CAB_Y + 4, CAB_X + 5, CAB_Y + 30, C_X + 10, C_Y - 72, C_X, C_Y - 3, C_TEXTO, 1);
}

/* La sólida, del extremo del cable al punto (x, y) por arriba: sale hacia
 * la derecha casi plana (su primer control, 40 antes de x y nunca a menos
 * de 60 del cable) y baja */
static void solida(int x, int y)
{
    curva(CAB_X + 1, CAB_Y, x - 40 > CAB_X + 60 ? x - 40 : CAB_X + 60, CAB_Y - 6, x + 8, CAB_Y + 26, x, y, C_TEXTO, 0);
}

/* El cable de T al punto (x, y), por abajo */
static void cable_t(int x, int y)
{
    curva(TT_X + 2, A_Y, TT_X + 60, A_Y - 6, x + 6, A_Y - 10, x, y, C_TEXTO, 0);
}

/* La sólida a la terminal x (la que va a T) por arriba, y el cable de T a
 * esa terminal, por abajo */
static void a_t(int x, int y)
{
    solida(x, T_ARR(y));
    cable_t(x, T_ABA(y));
}

/* El cable de A a la terminal (x, y) de abajo */
static void a_a(int x, int y)
{
    curva(A_X, A_Y - 2, A_X + 2, A_Y - 26, x - 8, A_Y - 32, x, y, C_TEXTO, 0);
}

/* Las terminales de un lado de los óvalos apilados en x (ys, n), unidas por
 * fuera con curvas que salen por el costado del óvalo (izq: por la
 * izquierda, hasta TI - 24, a 7 del óvalo; si no, por la derecha) */
#define FUERA 24
static void une_por_fuera(int x, const int *ys, int n, int izq)
{
    int i, xt = izq ? TI(x) - 5 : TD(x) + 5, xm = izq ? TI(x) - FUERA : TD(x) + FUERA;
    for (i = 0; i + 1 < n; i++) curva(xt, ys[i] + 1, xm, ys[i] + 1, xm, ys[i + 1] + 1, xt, ys[i + 1] + 1, C_TEXTO, 0);
}

/* El rótulo a la derecha de x0 (fuera de la curva de las derechas), en una
 * línea o, si se saldría de la pantalla, en dos */
static void rot_derecha(int x0, int y, double v)
{
    char t[24], u[20];
    int w, wv;
    valor(t, sizeof t, v);
    cpm_num(u, sizeof u, v, 3);
    w = gfx_text_w(t, 1);
    wv = gfx_text_w(u, 1);
    if (x0 + w <= SCR_W - 2) { rotulo(x0, y - 6, t, C_COND, 0); return; }
    if (x0 + wv > SCR_W - 2) x0 = SCR_W - 2 - wv;
    rotulo(x0, y - 12, u, C_COND, 0);
    rotulo(x0, y + 1, "MF", C_COND, 0);
}

void cpm_dibuja(const CpmRes *r, const CpmComb *k, int y0, int y1, int no_entra)
{
    const char *ley = cpm_leyenda(r, k);
    dy = y0 - 17;
    nrot = 0;
#ifndef NDLESS
    ncajas = 0;
#endif
    comun();
    switch (k->tipo) {
    case CPM_UNO: {
        int x = 190, y = 118;
        ovalo(x, y);
        a_a(TI(x), T_ABA(y));
        a_t(TD(x), y);
        rot_valor(x, y, r->v[k->a], 1);
        break;
    }
    case CPM_PAR: case CPM_SER: {
        int x1 = 150, x2 = 232, y = 118;
        ovalo(x1, y);
        ovalo(x2, y);
        if (k->tipo == CPM_PAR) {
            /* las izquierdas unidas por abajo y las derechas por arriba: cada
             * capacitor toca A y T */
            curva(TI(x1), T_ABA(y), TI(x1), y + 32, TI(x2), y + 32, TI(x2), T_ABA(y), C_TEXTO, 0);
            curva(TD(x1), T_ARR(y), TD(x1), y - 30, TD(x2), y - 30, TD(x2), T_ARR(y), C_TEXTO, 0);
        } else {
            /* solo el punto medio: la derecha del primero con la izquierda
             * del segundo, que no toca nada más */
            curva(TD(x1), T_ABA(y), TD(x1), y + 28, TI(x2), y + 28, TI(x2), T_ABA(y), C_TEXTO, 0);
            rotulo((TD(x1) + TI(x2)) / 2 - 33, y + 25, "punto medio", C_SUAVE, 1);
        }
        a_a(TI(x1), T_ABA(y));
        a_t(TD(x2), y);
        rot_valor(x1, y, r->v[k->a], -1);
        rot_valor(x2, y, r->v[k->b], 1);
        break;
    }
    case CPM_PAR3: {                /* los tres apilados, unidos por fuera a los dos lados */
        static const int YS[3] = { 66, 112, 158 };
        int x = 172, xm = TI(x) - FUERA, xr = TD(x) + FUERA;
        ovalo(x, YS[0]);
        ovalo(x, YS[1]);
        ovalo(x, YS[2]);
        une_por_fuera(x, YS, 3, 1);
        une_por_fuera(x, YS, 3, 0);
        /* A, a la curva de las izquierdas (su punto medio, entre el 2.º y el 3.º) */
        a_a((TI(x) - 5 + 3 * xm) / 4, (YS[1] + YS[2]) / 2 + 1);
        solida(TD(x), T_ARR(YS[0]));                         /* la sólida, a la derecha del de arriba */
        cable_t(TD(x), T_ABA(YS[2]));                        /* y T, a la del de abajo: las une la curva */
        rot_derecha(xr + 8, YS[0], r->v[k->a]);
        rot_derecha(xr + 8, YS[1], r->v[k->b]);
        rot_derecha(xr + 8, YS[2], r->v[k->c]);
        break;
    }
    case CPM_SER3: {                /* la cadena: dos puntos medios */
        int x1 = 132, x2 = 206, x3 = 280, y = 118;
        ovalo(x1, y);
        ovalo(x2, y);
        ovalo(x3, y);
        curva(TD(x1), T_ABA(y), TD(x1), y + 28, TI(x2), y + 28, TI(x2), T_ABA(y), C_TEXTO, 0);
        curva(TD(x2), T_ABA(y), TD(x2), y + 28, TI(x3), y + 28, TI(x3), T_ABA(y), C_TEXTO, 0);
        rotulo((TD(x1) + TI(x2)) / 2 - 33, y + 25, "punto medio", C_SUAVE, 1);
        rotulo((TD(x2) + TI(x3)) / 2 - 33, y + 25, "punto medio", C_SUAVE, 1);
        a_a(TI(x1), T_ABA(y));
        /* la sólida entra por la derecha del último (encima va su valor) */
        curva(CAB_X + 1, CAB_Y, 250, CAB_Y - 10, SCR_W - 4, y - 30, TD(x3) + 5, y + 1, C_TEXTO, 0);
        cable_t(TD(x3), T_ABA(y));
        {
            int xs[3];
            double vs[3];
            xs[0] = x1; xs[1] = x2; xs[2] = x3;
            vs[0] = r->v[k->a]; vs[1] = r->v[k->b]; vs[2] = r->v[k->c];
            rot_ser3(xs, vs, y, TD(x3));
        }
        break;
    }
    case CPM_MIX1: {                /* mixto: (a ∥ b) uno encima del otro, y c */
        int xp = 148, ya = 100, yb = 140, xc = 232, yc = 120, xm = TI(xp) - 18;
        ovalo(xp, ya);
        ovalo(xp, yb);
        ovalo(xc, yc);
        /* las izquierdas, unidas por fuera; A llega a esa curva */
        curva(TI(xp) - 5, ya + 1, xm, ya + 1, xm, yb + 1, TI(xp) - 5, yb + 1, C_TEXTO, 0);
        a_a(xm + 4, (ya + yb) / 2 + 1);
        /* las derechas, a la izquierda de c (el punto medio) */
        curva(TD(xp) + 5, ya + 1, TD(xp) + 30, ya + 1, TI(xc) - 30, yc + 1, TI(xc) - 5, yc + 1, C_TEXTO, 0);
        curva(TD(xp) + 5, yb + 1, TD(xp) + 30, yb + 1, TI(xc) - 30, yc + 1, TI(xc) - 5, yc + 1, C_TEXTO, 0);
        a_t(TD(xc), yc);
        rot_valor(xp, ya, r->v[k->a], 2);
        rot_valor(xp, yb, r->v[k->b], 0);
        rot_valor(xc, yc, r->v[k->c], 1);
        break;
    }
    default: {                      /* mixto 2: a y b en serie arriba, c abajo, unidos por los extremos */
        int xa = 150, xb = 232, y = 96, xc = 191, yc = 152;
        ovalo(xa, y);
        ovalo(xb, y);
        ovalo(xc, yc);
        /* el punto medio, por arriba */
        curva(TD(xa), T_ARR(y), TD(xa), y - 28, TI(xb), y - 28, TI(xb), T_ARR(y), C_TEXTO, 0);
        rotulo((TD(xa) + TI(xb)) / 2 - 33, y - 38, "punto medio", C_SUAVE, 1);
        /* los extremos: la izquierda de a con la de c (y A); la derecha de
         * b con la de c (y T) */
        curva(TI(xa), T_ABA(y), TI(xa), y + 30, TI(xc), yc - 30, TI(xc), T_ARR(yc), C_TEXTO, 0);
        curva(TD(xb), T_ABA(y), TD(xb), y + 30, TD(xc), yc - 30, TD(xc), T_ARR(yc), C_TEXTO, 0);
        a_a(TI(xc), T_ABA(yc));
        solida(TD(xb), T_ARR(y));
        cable_t(TD(xc), T_ABA(yc));
        rot_mix2a(xa, y, r->v[k->a]);
        rot_valor(xb, y, r->v[k->b], 1);
        rot_valor(xc, yc, r->v[k->c], 1);
        break;
    }
    }
    rotulos();
    /* la leyenda, abajo a la derecha (lo más cercano de un irresoluble, en
     * rojo y con su diferencia) */
    if (no_entra) {                 /* «NO ENTRA: lo más cercano, dif. -1.8 MF.» */
        static char t[80];
        char d[40];
        strcpy(t, "NO ENTRA: lo m" S_aa "s cercano, ");
        cpm_dif(r, k->ct, d, sizeof d);
        strcat(t, d);
        strcat(t, " MF.");
        ley = t;
    }
#ifndef NDLESS
    if (ncajas < 16) {
        cajas[ncajas].x = SCR_W - 4 - gfx_text_w(ley, 1); cajas[ncajas].y = y1 - 11;
        cajas[ncajas].w = gfx_text_w(ley, 1); cajas[ncajas].h = 10;
        cajas[ncajas].ocupados = gfx_ocupados(cajas[ncajas].x, cajas[ncajas].y, cajas[ncajas].w, 10, C_FONDO);
        ncajas++;
    }
#endif
    gfx_text(SCR_W - 4 - gfx_text_w(ley, 1), y1 - 12, ley, no_entra ? C_ERROR : C_SUAVE, 1);
}
