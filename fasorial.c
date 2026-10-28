/* ============================================================================
 *  fasorial.c  -  Diagrama fasorial y triángulo de potencias  (ver fasorial.h)
 * ========================================================================= */
#include <string.h>
#include <math.h>
#include "fasorial.h"
#include "osc.h"
#include "numfmt.h"
#include "gfx.h"
#include "ui.h"

#define PI_FZ    3.14159265358979323846
#define FZ_X1    207                /* último x del dibujo                    */
#define FZ_Y0    16                 /* primer y del dibujo                    */
#define FZ_Y1    226                /* último y del dibujo                    */
#define BARRA_H  16
#define PIE_Y    227
#define LEY_X    210                /* leyenda: de x = 210 a 319 (18 col.)    */
#define LEY_Y    18
#define LEY_DY   13
#define LEY_N    10                 /* líneas de la lista                     */
#define INFO_Y   152                /* el elegido, debajo de la lista         */
#define PZ_OX    20.0               /* origen del triángulo de potencias      */
#define PZ_OY    121.0
#define PZ_P     170.0              /* px para la mayor |P|                   */
#define PZ_Q     95.0               /* y para la mayor |Q|                    */
#define PZ_ARCO  22
#define TOL_POT  1e-10              /* |S| por debajo de esto (relativo): nula */
#define TOL_CERO 1e-12              /* fasor nulo, relativo a su familia      */

/* ==========================================================================
 *  Funciones puras
 * ======================================================================= */
static int fam(const Fasorial *z, int i) { return z->que[i] == 'I' ? 1 : 0; }

static Cx fasor(const Fasorial *z, const CaRes *s, int i)
{
    int k = z->el[i];
    return z->que[i] == 'I' ? s->I[k] : s->V[k];
}

static int nel_de(const Circuito *c) { return c->nel < MAX_EL ? c->nel : MAX_EL; }
static int tipo_de(const Circuito *c, int k) { return k >= 0 && k < nel_de(c) ? (int)c->el[k].tipo : -1; }
static int es_fuente(const Circuito *c, int k) { int t = tipo_de(c, k); return t == EL_FUENTE || t == EL_CORR; }
static int es_pasivo(const Circuito *c, int k)
{
    int t = tipo_de(c, k);
    return t == EL_RES || t == EL_COND || t == EL_BOB || t == EL_IMP;
}
/* Lo que entrega (su S en Sfuentes, ca_valores): las fuentes, las
 * controladas y el AO (por su salida) */
static int entrega(const Circuito *c, int k)
{
    int t = tipo_de(c, k);
    return es_fuente(c, k) || t == EL_EDEP || t == EL_JDEP || t == EL_AO;
}
/* Lo que cuenta en Boucherot: lo que absorbe (R, C, L, Z e instrumentos con
 * R). Ni lo que entrega ni el vatímetro (su S es la que pasa, no la que gasta) */
static int es_carga(const Circuito *c, int k) { return tipo_de(c, k) >= 0 && !entrega(c, k) && tipo_de(c, k) != EL_VATI; }
static const char *nom_el(const Circuito *c, int k) { return k >= 0 && k < nel_de(c) ? c->el[k].etiq : "?"; }

static int iesc_de(const Fasorial *z, int f)
{
    int i = z->iesc[f];
    if (i < 0) i = 0;
    if (i >= N_ESCALA) i = N_ESCALA - 1;
    return i;
}

/* Módulo y ángulo del fasor i como se ven: en el convenio y, con una
 * referencia, girado -θ_ref */
static void se_ve(const Fasorial *z, const CaRes *s, int i, int conv, double *m, double *a)
{
    ca_a_conv(conv, fasor(z, s, i), m, a);
    if (z->ref >= 0 && z->ref < z->n) {
        double mr, ar;
        ca_a_conv(conv, fasor(z, s, z->ref), &mr, &ar);
        *a = num_ang(*a - ar);
    }
}

void fz_vector(const Fasorial *z, const CaRes *s, int i, int conv, double *dx, double *dy)
{
    double m, a, L;
    *dx = *dy = 0.0;
    if (i < 0 || i >= z->n) return;
    se_ve(z, s, i, conv, &m, &a);
    L = m / ESCALA[iesc_de(z, fam(z, i))] * FZ_R;
    if (!(L > 0.0) || !(fabs(a) <= 360.0)) return;     /* nulo, o un NaN */
    if (L > 1e6) L = 1e6;
    *dx = L * cos(a * PI_FZ / 180.0);
    *dy = -L * sin(a * PI_FZ / 180.0);                   /* la pantalla crece hacia abajo */
}

/* A píxel, sin desbordar un int */
static int px_de(double v)
{
    if (!(v > -30000.0)) v = -30000.0;                  /* también un NaN */
    if (v > 30000.0) v = 30000.0;
    return (int)floor(v + 0.5);
}

void fz_punta(const Fasorial *z, const CaRes *s, int i, double ox, double oy, int *x, int *y)
{
    double dx, dy;
    fz_vector(z, s, i, 0, &dx, &dy);
    *x = px_de(ox + dx);
    *y = px_de(oy + dy);
}

int fz_busca(const Fasorial *z, int k, int que)
{
    int i;
    for (i = 0; i < z->n; i++) if (z->el[i] == k && z->que[i] == que) return i;
    return -1;
}

/* La familia encadenada, o -1 */
static int fam_cadena(const Fasorial *z) { int f = z->cadena & 3; return f ? f - 1 : -1; }
static int en_cadena(const Fasorial *z, const Circuito *c, int i, int f)
{
    return z->ver[i] && fam(z, i) == f && !es_fuente(c, z->el[i]);
}

int fz_cadena_fin(const Fasorial *z, const Circuito *c, const CaRes *s, int conv, int *x, int *y)
{
    double px = FZ_CX, py = FZ_CY, dx, dy;
    int i, n = 0, f = fam_cadena(z);
    if (f >= 0)
        for (i = 0; i < z->n; i++) {
            if (!en_cadena(z, c, i, f)) continue;
            fz_vector(z, s, i, conv, &dx, &dy);
            px += dx; py += dy; n++;
        }
    *x = px_de(px); *y = px_de(py);
    return n;
}

/* Escala de la familia f: el punto dibujado más lejano (la punta de un
 * fasor, o de un eslabón de la cadena) al 90 % de FZ_R como mucho */
static void auto_fam(Fasorial *z, const Circuito *c, const CaRes *s, int conv, int f)
{
    double mx = 0.0, m, a;
    Cx suma = cx(0.0, 0.0);
    int i, cad = fam_cadena(z) == f;
    for (i = 0; i < z->n; i++) {
        if (fam(z, i) != f || !z->ver[i]) continue;
        if (cad && !es_fuente(c, z->el[i])) {
            suma = cx_mas(suma, fasor(z, s, i));
            ca_a_conv(conv, suma, &m, &a);
        } else ca_a_conv(conv, fasor(z, s, i), &m, &a);
        if (m > mx) mx = m;
    }
    if (!(mx > 0.0))                    /* nada a la vista: con toda la familia */
        for (i = 0; i < z->n; i++) {
            if (fam(z, i) != f) continue;
            ca_a_conv(conv, fasor(z, s, i), &m, &a);
            if (m > mx) mx = m;
        }
    z->iesc[f] = (signed char)osc_escala_ge(mx > 0.0 ? mx / 0.9 : 1.0);
    z->ajustado[f] = 0;
}

void fz_autoescala(Fasorial *z, const Circuito *c, const CaRes *s, int conv)
{
    auto_fam(z, c, s, conv, 0);
    auto_fam(z, c, s, conv, 1);
}

/* ---- Potencias ---- */
static double s_escala(const Circuito *c, const CaRes *s)
{
    double e = cx_abs(s->Sfuentes), m;
    int k;
    for (k = 0; k < nel_de(c); k++) { m = cx_abs(s->S[k]); if (m > e) e = m; }
    return e;
}

static int con_pot(const CaRes *s, int k, double esc) { return cx_abs(s->S[k]) > TOL_POT * esc; }

int fz_npot(const Circuito *c, const CaRes *s)
{
    double esc;
    int k, n = 0;
    if (s->d.estado != SIM_OK) return 0;
    esc = s_escala(c, s);
    for (k = 0; k < nel_de(c); k++) if (con_pot(s, k, esc)) n++;
    return n;
}

int fz_pot_el(const Circuito *c, const CaRes *s, int j)
{
    double esc;
    int k, n = 0;
    if (s->d.estado != SIM_OK || j < 1) return -1;
    esc = s_escala(c, s);
    for (k = 0; k < nel_de(c); k++) if (con_pot(s, k, esc) && ++n == j) return k;
    return -1;
}

static int boucherot(const Fasorial *z) { return (z->cadena & FZ_BOUCHEROT) != 0; }

/* La S del triángulo: la de las fuentes, o la del elemento elegido (con
 * Boucherot, siempre la total: la cadena acaba en ella) */
static Cx s_tri(const Fasorial *z, const Circuito *c, const CaRes *s)
{
    int k;
    if (boucherot(z) || z->selp <= 0) return s->Sfuentes;
    k = fz_pot_el(c, s, z->selp);
    return k >= 0 ? s->S[k] : s->Sfuentes;
}

double fz_escala_pot(const Fasorial *z, const Circuito *c, const CaRes *s)
{
    Cx S, acc = cx(0.0, 0.0);
    double esc, nula, mp, mq, k = 0.0;
    int kk;
    if (s->d.estado != SIM_OK) return 0.0;
    S = s_tri(z, c, s);
    esc = s_escala(c, s);
    nula = 1e-9 * esc;                  /* una P o una Q de redondeo no cuentan */
    mp = fabs(S.re); mq = fabs(S.im);
    if (boucherot(z))
        for (kk = 0; kk < nel_de(c); kk++) {
            if (!es_carga(c, kk) || !con_pot(s, kk, esc)) continue;
            acc = cx_mas(acc, s->S[kk]);
            if (fabs(acc.re) > mp) mp = fabs(acc.re);
            if (fabs(acc.im) > mq) mq = fabs(acc.im);
        }
    if (mp > nula) k = PZ_P / mp;
    if (mq > nula && (k == 0.0 || PZ_Q / mq < k)) k = PZ_Q / mq;
    return k;
}

/* ==========================================================================
 *  Textos
 * ======================================================================= */
static void nombre(const Fasorial *z, const Circuito *c, int i, char *t, size_t cap)
{
    Buf b;
    buf_init(&b, t, cap);
    buf_ch(&b, (char)z->que[i]); buf_ch(&b, '(');
    buf_str(&b, nom_el(c, z->el[i])); buf_ch(&b, ')');
}

/* Una potencia: num_pot, como en P_CA ("1142 VA", "906,1 W") */
#define pot_txt num_pot

/* "fp 0,793 cap." / "fp 0,600 ind." / "fp 1,000" */
static char *fp_txt(char *buf, size_t cap, Cx S)
{
    double m = cx_abs(S), Q = S.im;
    char t[24];
    Buf b;
    buf_init(&b, buf, cap);
    buf_str(&b, "fp ");
    if (!(m > 0.0)) { buf_str(&b, S_RAYA); return buf; }
    buf_str(&b, num_txt(t, sizeof t, fabs(S.re) / m, 3));
    if (fabs(Q) <= 1e-12 * m) return buf;
    buf_str(&b, S.re * Q >= 0.0 ? " ind." : " cap.");
    return buf;
}

/* ==========================================================================
 *  Dibujo: piezas
 * ======================================================================= */
static void barra(const char *titulo, const CaRes *s, int conv)
{
    char t[48], u[24];
    Buf b;
    gfx_fill(0, 0, SCR_W, BARRA_H, C_BARRA);
    gfx_text(5, 2, titulo, C_TEXTO, 1);
    if (!(s->f > 0.0) || s->d.estado != SIM_OK) return;
    buf_init(&b, t, sizeof t);
    buf_str(&b, num_pref(u, sizeof u, s->f, 4, " Hz"));
    buf_str(&b, (conv & CONV_PICO) ? "  pico, " : "  eficaz, ");
    buf_str(&b, (conv & CONV_SENO) ? "seno" : "coseno");
    gfx_text(SCR_W - 5 - gfx_text_w(t, 1), 2, t, C_CURSOR, 1);
}

static void pie(const char *t)
{
    gfx_fill(0, PIE_Y, SCR_W, SCR_H - PIE_Y, C_PANEL);
    gfx_text(5, PIE_Y + 1, t, C_SUAVE, 1);
}

/* ---- Lo ya dibujado (flechas y rótulos), para que los rótulos no lo pisen ---- */
typedef struct { short x, y, w, h; } Caja;
typedef struct { double xa, ya, xb, yb; short x0, y0, x1, y1; } Seg;   /* y su caja, para descartar rápido */
static Caja cajas[FZ_MAX + 24];
static Seg  segs[3 * FZ_MAX + 16];
static int  ncajas, nsegs;

static void ocupa(int x, int y, int w, int h)
{
    if (ncajas >= (int)(sizeof cajas / sizeof cajas[0])) return;
    cajas[ncajas].x = (short)x; cajas[ncajas].y = (short)y;
    cajas[ncajas].w = (short)w; cajas[ncajas].h = (short)h;
    ncajas++;
}

/* Un trazo ya recortado al dibujo (sus extremos caben en un short) */
static void marca(double xa, double ya, double xb, double yb)
{
    Seg *g;
    if (nsegs >= (int)(sizeof segs / sizeof segs[0])) return;
    g = &segs[nsegs++];
    g->xa = xa; g->ya = ya; g->xb = xb; g->yb = yb;
    g->x0 = (short)floor(xa < xb ? xa : xb); g->x1 = (short)ceil(xa < xb ? xb : xa);
    g->y0 = (short)floor(ya < yb ? ya : yb); g->y1 = (short)ceil(ya < yb ? yb : ya);
}

/* Recorta el segmento al rectángulo [x0, x1] x [y0, y1] (Liang-Barsky). 0 si
 * no queda nada (o si hay un NaN) */
static int recorta_en(double *xa, double *ya, double *xb, double *yb, double x0, double y0, double x1, double y1)
{
    double t0 = 0.0, t1 = 1.0, dx = *xb - *xa, dy = *yb - *ya, p[4], q[4];
    int i;
    if (!FINITO(*xa) || !FINITO(*ya) || !FINITO(*xb) || !FINITO(*yb)) return 0;
    p[0] = -dx; q[0] = *xa - x0;
    p[1] = dx;  q[1] = x1 - *xa;
    p[2] = -dy; q[2] = *ya - y0;
    p[3] = dy;  q[3] = y1 - *ya;
    for (i = 0; i < 4; i++) {
        double r;
        if (p[i] == 0.0) { if (q[i] < 0.0) return 0; continue; }
        r = q[i] / p[i];
        if (p[i] < 0.0) { if (r > t1) return 0; if (r > t0) t0 = r; }
        else            { if (r < t0) return 0; if (r < t1) t1 = r; }
    }
    *xb = *xa + t1 * dx; *yb = *ya + t1 * dy;
    *xa = *xa + t0 * dx; *ya = *ya + t0 * dy;
    return 1;
}

/* Al rectángulo del dibujo */
static int recorta(double *xa, double *ya, double *xb, double *yb)
{
    return recorta_en(xa, ya, xb, yb, 0.0, FZ_Y0, FZ_X1, FZ_Y1);
}

enum { TR_GRUESO = 0, TR_DISC, TR_PUNTOS, TR_FINO };

static void traza(double xa, double ya, double xb, double yb, u16 col, int estilo)
{
    int xi = px_de(xa), yi = px_de(ya), xf = px_de(xb), yf = px_de(yb);
    switch (estilo) {
    case TR_GRUESO: gfx_line_w(xi, yi, xf, yf, col, 2); break;
    case TR_DISC:   gfx_line_dash(xi, yi, xf, yf, col, 4, 2); break;
    case TR_PUNTOS: gfx_line_dash(xi, yi, xf, yf, col, 1, 2); break;
    default:        gfx_line(xi, yi, xf, yf, col); break;
    }
}

static int dentro(double x, double y)
{
    return x >= -0.5 && x <= FZ_X1 + 0.5 && y >= FZ_Y0 - 0.5 && y <= FZ_Y1 + 0.5;
}

/* Flecha de (xa, ya) a (xb, yb), recortada al dibujo, con punta si la punta
 * cae dentro. 1 si se ve algo; en (*ex, *ey) el extremo que se ve */
static int flecha(double xa, double ya, double xb, double yb, u16 col, int estilo, double *ex, double *ey)
{
    double ax = xa, ay = ya, bx = xb, by = yb, dx = xb - xa, dy = yb - ya, L = sqrt(dx * dx + dy * dy);
    if (!recorta(&ax, &ay, &bx, &by)) return 0;
    *ex = bx; *ey = by;
    marca(ax, ay, bx, by);
    if (L < 2.0) { gfx_circle(px_de(xa), px_de(ya), 1, col, 1); return 1; }   /* casi nulo: un punto */
    if (dentro(xb, yb) && L >= 4.0) {
        double ux = dx / L, uy = dy / L, hl = L < 14.0 ? 0.5 * L : 7.0;
        double cx0 = xb - ux * hl, cy0 = yb - uy * hl, sx = ax, sy = ay;
        if (recorta(&sx, &sy, &cx0, &cy0)) traza(sx, sy, cx0, cy0, col, estilo);
        cx0 = xb - ux * hl; cy0 = yb - uy * hl;
        gfx_tri(px_de(xb), px_de(yb), px_de(cx0 - uy * 3.5), px_de(cy0 + ux * 3.5),
                px_de(cx0 + uy * 3.5), px_de(cy0 - ux * 3.5), col);
    } else traza(ax, ay, bx, by, col, estilo);
    return 1;
}

static void linea(double xa, double ya, double xb, double yb, u16 col)
{
    if (!recorta(&xa, &ya, &xb, &yb)) return;
    traza(xa, ya, xb, yb, col, TR_FINO);
    marca(xa, ya, xb, yb);
}

/* Un eslabón de una cadena, de (xa, ya) a (xb, yb). Si vuelve sobre el
 * anterior (pdx, pdy) (colineal y al revés, como Q_L y luego Q_C), se
 * dibuja 4 px a un lado, con dos uniones finas, para que no se tapen; la
 * cadena sigue desde (xb, yb). Como flecha(); en (*ox, *oy) el desvío */
static int eslabon(double xa, double ya, double xb, double yb, double pdx, double pdy,
                   u16 col, int estilo, double *ex, double *ey, double *ox, double *oy)
{
    double dx = xb - xa, dy = yb - ya, L = sqrt(dx * dx + dy * dy), P = sqrt(pdx * pdx + pdy * pdy);
    *ox = *oy = 0.0;
    if (L >= 2.0 && P >= 2.0 && dx * pdx + dy * pdy < 0.0 && fabs(dx * pdy - dy * pdx) <= 0.02 * L * P) {
        *ox = -dy / L * 4.0; *oy = dx / L * 4.0;
        linea(xa, ya, xa + *ox, ya + *oy, col);
        linea(xb + *ox, yb + *oy, xb, yb, col);
    }
    return flecha(xa + *ox, ya + *oy, xb + *ox, yb + *oy, col, estilo, ex, ey);
}

static void circulo_punteado(int r, int paso)
{
    int a;
    for (a = 0; a < 360; a += paso)
        gfx_px(FZ_CX + r * gfx_icos(a) / 1024, FZ_CY - r * gfx_isin(a) / 1024, C_REJILLA);
}

/* Lo que pisa un rótulo en (x, y) de w x h (con 1 px de margen): otro
 * rótulo cuenta 4; cada trazo que lo cruza, 1 */
static int coste(int x, int y, int w, int h)
{
    int i, c = 0;
    for (i = 0; i < ncajas; i++) {
        const Caja *q = &cajas[i];
        if (x - 1 < q->x + q->w && q->x < x + w + 1 && y - 1 < q->y + q->h && q->y < y + h + 1) c += 4;
    }
    for (i = 0; i < nsegs; i++) {
        const Seg *g = &segs[i];
        double xa = g->xa, ya = g->ya, xb = g->xb, yb = g->yb;
        if (g->x1 < x - 1 || g->x0 > x + w || g->y1 < y - 1 || g->y0 > y + h) continue;   /* lejos */
        if (recorta_en(&xa, &ya, &xb, &yb, x - 1.0, y - 1.0, (double)(x + w), (double)(y + h))) c++;
    }
    return c;
}

/* El mejor sitio para un rótulo de w x h junto a la punta (tx, ty) de un
 * trazo en la dirección (ux, uy): más allá de la punta o a un lado, donde
 * menos pise y siempre dentro. Mejora (*bx, *by) si cuesta menos que *bc */
static void sitio(double tx, double ty, double ux, double uy, int w, int h, int *bx, int *by, int *bc)
{
    double hw = 0.5 * w, hh = 0.5 * h, L = sqrt(ux * ux + uy * uy), nx, ny;
    int k;
    if (L > 0.0) { ux /= L; uy /= L; } else { ux = 0.7071; uy = -0.7071; }
    nx = -uy; ny = ux;
    for (k = 0; k < 7 && *bc > 0; k++) {
        double cx0, cy0;
        int x, y, cc;
        if (k < 3) {                                  /* más allá de la punta */
            double s = 2.0 + 7.0 * k + hw * fabs(ux) + hh * fabs(uy);
            cx0 = tx + ux * s; cy0 = ty + uy * s;
        } else {                                      /* a un lado o al otro, junto a la punta o más atrás */
            double sg = (k & 1) ? 1.0 : -1.0, s = 3.0 + hw * fabs(nx) + hh * fabs(ny);
            double atras = (k < 5 ? 0.0 : 10.0) + hw * fabs(ux) + hh * fabs(uy);
            cx0 = tx + sg * nx * s - ux * atras; cy0 = ty + sg * ny * s - uy * atras;
        }
        x = px_de(cx0 - hw); y = px_de(cy0 - hh);
        if (x > FZ_X1 + 1 - w) x = FZ_X1 + 1 - w;
        if (x < 0) x = 0;
        if (y > FZ_Y1 + 1 - h) y = FZ_Y1 + 1 - h;
        if (y < FZ_Y0) y = FZ_Y0;
        cc = coste(x, y, w, h);
        if (cc < *bc) { *bc = cc; *bx = x; *by = y; }
    }
}

static void pon_rotulo(int x, int y, const char *t, u16 col, int grande)
{
    ocupa(x, y, gfx_text_w(t, 1), grande ? FONT_H : FONT_H_P);
    if (grande) gfx_text_bg(x, y, t, col, C_FONDO, 1);
    else gfx_text_p_bg(x, y, t, col, C_FONDO);
}

/* Un rótulo junto a la punta (tx, ty) de un trazo en la dirección (ux, uy).
 * Si no queda sitio sin pisar otro rótulo, no se pone (salvo siempre = 1: el
 * elegido); la leyenda dice de quién es cada flecha */
static void rotulo(double tx, double ty, double ux, double uy, const char *t, u16 col, int grande, int siempre)
{
    int bx = 0, by = 0, bc = 1 << 20;
    sitio(tx, ty, ux, uy, gfx_text_w(t, 1), grande ? FONT_H : FONT_H_P, &bx, &by, &bc);
    if (bc < 4 || siempre) pon_rotulo(bx, by, t, col, grande);
}

/* Un rótulo a un lado del tramo de (xa, ya) a (xb, yb), en el punto de él
 * donde menos pise (el de un eslabón de Boucherot) */
static void rotulo_tramo(double xa, double ya, double xb, double yb, const char *t, u16 col, int siempre)
{
    static const double F[5] = { 0.5, 0.3, 0.7, 0.15, 0.85 };
    double dx = xb - xa, dy = yb - ya;
    int i, bx = 0, by = 0, bc = 1 << 20;
    for (i = 0; i < 5 && bc > 0; i++)
        sitio(xa + F[i] * dx, ya + F[i] * dy, -dy, dx, gfx_text_w(t, 1), FONT_H_P, &bx, &by, &bc);
    if (bc < 4 || siempre) pon_rotulo(bx, by, t, col, 0);
}

static u16 color_fam(int f) { return f ? C_CORRIENTE : C_TENSION; }

/* La lista de la derecha: el fondo, y luego las marcas de que hay más */
static void panel_lista(void)
{
    gfx_fill(FZ_X1 + 1, FZ_Y0, SCR_W - FZ_X1 - 1, FZ_Y1 - FZ_Y0 + 1, C_PANEL);
}

static void mas_lista(int ini, int total)
{
    if (ini > 0) gfx_text(310, LEY_Y, S_UARR, C_SUAVE, 1);
    if (ini + LEY_N < total) gfx_text(310, LEY_Y + (LEY_N - 1) * LEY_DY, S_DARR, C_SUAVE, 1);
}

static void mensaje(const char *a, const char *b)
{
    gfx_text(8, 100, a, C_SUAVE, 1);
    if (b) gfx_text(8, 114, b, C_SUAVE, 1);
}

/* ==========================================================================
 *  Vista de fasores
 * ======================================================================= */
static void dibuja_fasores(const Fasorial *z, const Circuito *c, const CaRes *s, int conv, int rect)
{
    static double sx[FZ_MAX], sy[FZ_MAX], ex[FZ_MAX], ey[FZ_MAX], vx[FZ_MAX], vy[FZ_MAX], pvx[FZ_MAX], pvy[FZ_MAX];
    static unsigned char visto[FZ_MAX], encad[FZ_MAX];
    char t[48], u[24];
    Buf b;
    int i, j, pasada, fc = fam_cadena(z), nesl = 0, hay[2] = { 0, 0 }, w;
    double kx = FZ_CX, ky = FZ_CY, sex = 0.0, sey = 0.0, ldx = 0.0, ldy = 0.0;

    ncajas = nsegs = 0;
    gfx_clip(0, FZ_Y0, FZ_X1 + 1, FZ_Y1 - FZ_Y0 + 1);
    gfx_hline(0, FZ_CY, FZ_X1 + 1, C_REJILLA);
    gfx_vline(FZ_CX, FZ_Y0, FZ_Y1 - FZ_Y0 + 1, C_REJILLA);
    circulo_punteado(FZ_R / 2, 6);
    circulo_punteado(FZ_R, 3);
    gfx_text_p(FZ_X1 - 12, FZ_CY - 10, "Re", C_SUAVE); ocupa(FZ_X1 - 12, FZ_CY - 10, 12, 8);
    gfx_text_p(FZ_CX + 4, FZ_Y0 + 2, "Im", C_SUAVE);   ocupa(FZ_CX + 4, FZ_Y0 + 2, 12, 8);

    if (s->d.estado != SIM_OK || z->n <= 0) {
        gfx_clip_reset();
        mensaje(s->d.estado != SIM_OK ? "El análisis de CA no dio" : "No hay fasores que enseñar.",
                s->d.estado != SIM_OK ? "fasores: [esc] y corrígelo." : 0);
        panel_lista();
        return;
    }

    /* la escala del borde, abajo a la izquierda: "500 V  20 A" */
    for (i = 0; i < z->n; i++) if (z->ver[i]) hay[fam(z, i)] = 1;
    buf_init(&b, t, sizeof t);
    for (j = 0; j < 2; j++) {
        if (!hay[j]) continue;
        if (b.n) buf_str(&b, "  ");
        buf_str(&b, num_pref(u, sizeof u, ESCALA[iesc_de(z, j)], 3, j ? " A" : " V"));
    }
    w = gfx_text_w(t, 1);
    if (w) ocupa(2, FZ_Y1 - 12, w, FONT_H);

    /* de dónde sale cada uno: del origen, o del final del eslabón anterior */
    for (i = 0; i < z->n; i++) {
        fz_vector(z, s, i, conv, &vx[i], &vy[i]);
        sx[i] = FZ_CX; sy[i] = FZ_CY; visto[i] = 0; pvx[i] = pvy[i] = 0.0;
        encad[i] = (unsigned char)(fc >= 0 && en_cadena(z, c, i, fc));
        if (encad[i]) {
            sx[i] = kx; sy[i] = ky; pvx[i] = ldx; pvy[i] = ldy;
            kx += vx[i]; ky += vy[i]; ldx = vx[i]; ldy = vy[i];
            nesl++;
        }
    }

    /* las flechas: primero las demás y luego la elegida, encima */
    for (pasada = 0; pasada < 2; pasada++)
        for (i = 0; i < z->n; i++) {
            u16 col;
            int est;
            if (!z->ver[i] || (i == z->sel) != (pasada == 1)) continue;
            col = i == z->sel ? C_CURSOR : color_fam(fam(z, i));
            est = fam(z, i) ? TR_DISC : TR_GRUESO;
            if (encad[i]) {
                double ox, oy;
                visto[i] = (unsigned char)eslabon(sx[i], sy[i], sx[i] + vx[i], sy[i] + vy[i], pvx[i], pvy[i], col, est,
                                                  &ex[i], &ey[i], &ox, &oy);
            } else visto[i] = (unsigned char)flecha(sx[i], sy[i], sx[i] + vx[i], sy[i] + vy[i], col, est, &ex[i], &ey[i]);
        }
    /* la suma, punteada, del origen al final de la cadena */
    if (nesl > 0 && !flecha(FZ_CX, FZ_CY, kx, ky, C_TEXTO, TR_PUNTOS, &sex, &sey)) nesl = 0;

    /* los rótulos: el del elegido y la Σ primero (siempre), para que se
     * queden su sitio; los demás, si caben sin pisar otro */
    if (z->sel >= 0 && z->sel < z->n && visto[z->sel])
        rotulo(ex[z->sel], ey[z->sel], vx[z->sel], vy[z->sel], nom_el(c, z->el[z->sel]), C_CURSOR, 0, 1);
    if (nesl > 0) rotulo(sex, sey, kx - FZ_CX, ky - FZ_CY, S_SIGMA, C_TEXTO, 0, 1);
    for (i = 0; i < z->n; i++)
        if (visto[i] && i != z->sel) rotulo(ex[i], ey[i], vx[i], vy[i], nom_el(c, z->el[i]), color_fam(fam(z, i)), 0, 0);
    gfx_clip_reset();
    if (w) gfx_text_bg(2, FZ_Y1 - 12, t, C_SUAVE, C_FONDO, 1);

    /* la leyenda */
    panel_lista();
    for (j = 0; j < LEY_N && z->ini + j < z->n; j++) {
        int y = LEY_Y + j * LEY_DY;
        u16 cq, cn;
        i = z->ini + j;
        if (i == z->sel) gfx_fill(LEY_X, y - 1, SCR_W - LEY_X, LEY_DY, C_BARRA);
        cq = !z->ver[i] ? C_TENUE : i == z->sel ? C_CURSOR : color_fam(fam(z, i));
        cn = z->ver[i] ? C_TEXTO : C_TENUE;
        gfx_fill(212, y + 3, 6, 6, cq);
        nombre(z, c, i, t, sizeof t);
        gfx_text(222, y, t, cn, 1);
        if (i == z->ref) gfx_text(290, y, "0" S_DEG, C_TEXTO, 1);
    }
    mas_lista(z->ini, z->n);
    /* el elegido: nombre, sus dos formas y las teclas */
    if (z->sel >= 0 && z->sel < z->n) {
        char n1[20], n2[20];
        double m, a;
        i = z->sel;
        nombre(z, c, i, n1, sizeof n1);
        buf_init(&b, t, 19);                            /* 18 columnas */
        buf_str(&b, n1);
        if (z->ref == i) buf_str(&b, " (ref.)");
        else if (z->ref >= 0 && z->ref < z->n) { nombre(z, c, z->ref, n2, sizeof n2); buf_str(&b, " ref "); buf_str(&b, n2); }
        gfx_text(211, INFO_Y, t, color_fam(fam(z, i)), 1);
        se_ve(z, s, i, conv, &m, &a);
        num_fasor(t, sizeof t, m, a, rect, 0, fam(z, i) ? "A" : "V");
        gfx_text(211, INFO_Y + 13, t, C_TEXTO, 1);
        num_fasor(t, sizeof t, m, a, !rect, 0, fam(z, i) ? "A" : "V");
        gfx_text(211, INFO_Y + 26, t, C_SUAVE, 1);
    }
    gfx_text(211, INFO_Y + 39, "[R] ref  [S] suma", C_SUAVE, 1);
    gfx_text(211, INFO_Y + 52, "[+-] zoom [A] auto", C_SUAVE, 1);
}

/* ==========================================================================
 *  Vista de potencias
 * ======================================================================= */
static void dibuja_potencias(const Fasorial *z, const Circuito *c, const CaRes *s)
{
    static double rx[MAX_EL], ry[MAX_EL], rux[MAX_EL], ruy[MAX_EL];   /* los tramos de Boucherot */
    static signed char rel[MAX_EL];
    char t[48], u[24];
    Buf b;
    int np = fz_npot(c, s), selp = z->selp, j, kk, ksel, nr = 0;
    double k = fz_escala_pot(z, c, s), esc = s_escala(c, s), oy = PZ_OY;
    Cx S = s_tri(z, c, s), Sv;

    if (selp < 0 || selp > np) selp = 0;
    ksel = selp > 0 ? fz_pot_el(c, s, selp) : -1;
    ncajas = nsegs = 0;
    gfx_clip(0, FZ_Y0, FZ_X1 + 1, FZ_Y1 - FZ_Y0 + 1);
    gfx_hline(0, (int)PZ_OY, FZ_X1 + 1, C_REJILLA);
    if (s->d.estado != SIM_OK || !(k > 0.0)) {
        gfx_clip_reset();
        mensaje(s->d.estado != SIM_OK ? "El análisis de CA no dio" : "No hay potencia que dibujar.",
                s->d.estado != SIM_OK ? "potencias: [esc] y corrígelo." : 0);
    } else {
        double nula = 1e-9 * esc, p1x, p2y, e1, e2, phi, ox;
        int hayq, hayp;
        ox = (S.re < -nula && !boucherot(z)) ? FZ_X1 - PZ_OX + 1.0 : PZ_OX;   /* P < 0: hacia la izquierda */
        p1x = ox + S.re * k;
        p2y = oy - S.im * k;
        hayp = fabs(S.re) > nula; hayq = fabs(S.im) > nula;
        gfx_vline((int)ox, FZ_Y0, FZ_Y1 - FZ_Y0 + 1, C_REJILLA);
        /* Boucherot: las cargas una tras otra, finas; debajo del triángulo */
        if (boucherot(z)) {
            double ax = ox, ay = oy, ldx = 0.0, ldy = 0.0;
            for (kk = 0; kk < nel_de(c); kk++) {
                double bx, by, qx, qy, dx, dy, ox2, oy2;
                u16 col;
                if (!es_carga(c, kk) || !con_pot(s, kk, esc)) continue;
                dx = s->S[kk].re * k; dy = -s->S[kk].im * k;
                bx = ax + dx; by = ay + dy;
                col = kk == ksel ? C_CURSOR : C_SUAVE;
                if (eslabon(ax, ay, bx, by, ldx, ldy, col, TR_FINO, &qx, &qy, &ox2, &oy2)) {
                    /* su rótulo, a un lado del eslabón (se pone al final) */
                    rx[nr] = ax + ox2; ry[nr] = ay + oy2; rux[nr] = bx + ox2; ruy[nr] = by + oy2;
                    rel[nr] = (signed char)kk; nr++;
                }
                ax = bx; ay = by; ldx = dx; ldy = dy;
            }
        }
        /* el triángulo: S, P y Q */
        if (hayp || hayq) flecha(ox, oy, p1x, p2y, C_TEXTO, TR_GRUESO, &e1, &e2);
        if (hayp) flecha(ox, oy, p1x, oy, C_POTENCIA, TR_GRUESO, &e1, &e2);
        if (hayq) flecha(p1x, oy, p1x, p2y, C_REACT, TR_GRUESO, &e1, &e2);
        /* el ángulo: el arco de P a S y "φ -37,5°" al otro lado de P */
        phi = atan2(S.im, S.re) * 180.0 / PI_FZ;
        if (hayp && hayq) {
            int a0 = S.re >= 0.0 ? 0 : 180, a1 = (int)floor(phi + 0.5);
            if (a0 == 180 && a1 < 0) a1 += 360;
            gfx_arc((int)ox, (int)oy, PZ_ARCO, a0 < a1 ? a0 : a1, a0 < a1 ? a1 : a0, C_SUAVE);
        }
        buf_init(&b, t, sizeof t);
        buf_str(&b, S_PHI " "); buf_str(&b, num_txt(u, sizeof u, hayq ? phi : (S.re < 0.0 ? 180.0 : 0.0), 1)); buf_str(&b, S_DEG);
        {
            int w = gfx_text_w(t, 1), y = S.im < 0.0 ? (int)oy - 14 : (int)oy + 3;
            int x = S.re >= 0.0 || boucherot(z) ? (int)ox + 4 : (int)ox - 4 - w;
            int xp;
            ocupa(x, y, w, FONT_H);
            gfx_text_bg(x, y, t, C_SUAVE, C_FONDO, 1);
            /* P, al mismo lado, más allá del ángulo */
            if (hayp) {
                xp = (int)floor(0.5 * (ox + p1x) + 0.5) - 3;
                if (S.re >= 0.0) { if (xp < x + w + 6) xp = x + w + 6; }
                else if (xp > x - 12) xp = x - 12;
                if (xp > FZ_X1 - 5) xp = FZ_X1 - 5;
                if (xp < 0) xp = 0;
                ocupa(xp, y, 6, FONT_H);
                gfx_text_bg(xp, y, "P", C_POTENCIA, C_FONDO, 1);
            }
        }
        /* Q, fuera del triángulo */
        if (hayq) {
            int xq = S.re >= 0.0 ? (int)floor(p1x + 0.5) + 5 : (int)floor(p1x + 0.5) - 11, yq = px_de(0.5 * (oy + p2y)) - 6;
            if (xq > FZ_X1 - 5) xq = (int)floor(p1x + 0.5) - 11;
            if (xq < 0) xq = 0;
            ocupa(xq, yq, 6, FONT_H);
            gfx_text_bg(xq, yq, "Q", C_REACT, C_FONDO, 1);
        }
        /* S, al otro lado de la hipotenusa; y los de Boucherot */
        if (hayp && hayq) {
            double mx = 0.5 * (ox + p1x), my = 0.5 * (oy + p2y), nx = -(p2y - oy), ny = p1x - ox;
            if (nx * (p1x - mx) + ny * (oy - my) > 0.0) { nx = -nx; ny = -ny; }   /* hacia fuera */
            rotulo(mx, my, nx, ny, "S", C_TEXTO, 1, 1);
        }
        for (j = 0; j < nr; j++)
            rotulo_tramo(rx[j], ry[j], rux[j], ruy[j], nom_el(c, rel[j]), rel[j] == ksel ? C_CURSOR : C_SUAVE, rel[j] == ksel);
    }
    gfx_clip_reset();

    /* la lista: Total y los elementos con S ≠ 0 */
    panel_lista();
    for (j = 0; j < LEY_N && z->inip + j <= np; j++) {
        int it = z->inip + j, y = LEY_Y + j * LEY_DY, ke = it > 0 ? fz_pot_el(c, s, it) : -1;
        if (it == selp) gfx_fill(LEY_X, y - 1, SCR_W - LEY_X, LEY_DY, C_BARRA);
        if (it == 0) { gfx_text(222, y, "Total", C_TEXTO, 1); continue; }
        gfx_text(222, y, nom_el(c, ke), C_TEXTO, 1);
        gfx_text(262, y, entrega(c, ke) ? "entrega" : tipo_de(c, ke) == EL_VATI ? "mide" : "absorbe", C_SUAVE, 1);
    }
    mas_lista(z->inip, np + 1);
    /* lo del elegido */
    if (s->d.estado != SIM_OK) return;
    Sv = ksel >= 0 ? s->S[ksel] : s->Sfuentes;
    if (fabs(Sv.re) <= 1e-9 * esc) Sv.re = 0.0;          /* la P de una L o la Q de una R: 0 */
    if (fabs(Sv.im) <= 1e-9 * esc) Sv.im = 0.0;
    buf_init(&b, t, 19);
    if (ksel < 0) buf_str(&b, boucherot(z) ? "Total (Boucherot)" : "Total entregado");
    else {
        buf_str(&b, nom_el(c, ksel));
        buf_str(&b, entrega(c, ksel) ? " entrega" : tipo_de(c, ksel) == EL_VATI ? " mide" : " absorbe");
    }
    gfx_text(211, INFO_Y, t, C_TITULO, 1);
    buf_init(&b, t, sizeof t); buf_str(&b, "P = "); buf_str(&b, pot_txt(u, sizeof u, Sv.re, "W"));
    gfx_text(211, INFO_Y + 13, t, C_POTENCIA, 1);
    buf_init(&b, t, sizeof t); buf_str(&b, "Q = "); buf_str(&b, pot_txt(u, sizeof u, Sv.im, "var"));
    gfx_text(211, INFO_Y + 26, t, C_REACT, 1);
    buf_init(&b, t, sizeof t); buf_str(&b, "S = "); buf_str(&b, pot_txt(u, sizeof u, cx_abs(Sv), "VA"));
    gfx_text(211, INFO_Y + 39, t, C_TEXTO, 1);
    gfx_text(211, INFO_Y + 52, fp_txt(t, sizeof t, Sv), C_TEXTO, 1);
}

/* ==========================================================================
 *  La pantalla y sus teclas
 * ======================================================================= */
void fz_nuevo(Fasorial *z, const Circuito *c, const CaRes *s)
{
    double mx[2] = { 0.0, 0.0 }, i0 = 0.0, v0 = 0.0;
    int k, q, i, npas = 0, serie = 1, paralelo = 1, nel = nel_de(c);

    memset(z, 0, sizeof *z);
    z->ref = -1;
    if (s->d.estado == SIM_OK) {
        for (k = 0; k < nel; k++) {
            double a = cx_abs(s->V[k]), b = cx_abs(s->I[k]);
            if (a > mx[0]) mx[0] = a;
            if (b > mx[1]) mx[1] = b;
        }
        /* la V y la I de cada elemento, sin las nulas */
        for (k = 0; k < nel; k++)
            for (q = 0; q < 2 && z->n < FZ_MAX; q++) {
                double a = cx_abs(q ? s->I[k] : s->V[k]);
                if (!(a > TOL_CERO * mx[q])) continue;
                z->el[z->n] = (signed char)k;
                z->que[z->n] = (unsigned char)(q ? 'I' : 'V');
                z->n++;
            }
        /* ¿serie (la misma |I|) o paralelo (la misma |V|)? */
        for (k = 0; k < nel; k++) {
            double vi, vv;
            if (!es_pasivo(c, k)) continue;
            vi = cx_abs(s->I[k]); vv = cx_abs(s->V[k]);
            if (npas++ == 0) { i0 = vi; v0 = vv; continue; }
            if (fabs(vi - i0) > 1e-9 * (vi > i0 ? vi : i0)) serie = 0;
            if (fabs(vv - v0) > 1e-9 * (vv > v0 ? vv : v0)) paralelo = 0;
        }
        /* se ven las fuentes y, de los pasivos, sus V (o sus I en paralelo) */
        for (i = 0; i < z->n; i++) {
            k = z->el[i];
            if (es_fuente(c, k)) z->ver[i] = 1;
            else if (es_pasivo(c, k)) z->ver[i] = (unsigned char)(z->que[i] == ((serie || !paralelo) ? 'V' : 'I'));
        }
        for (i = 0; i < z->n; i++) if (z->ver[i]) { z->sel = i; break; }
    }
    fz_autoescala(z, c, s, c->conv);
}

void fz_dibuja(const Fasorial *z, const Circuito *c, const CaRes *s, int conv, int rect)
{
    gfx_clip_reset();
    gfx_clear(C_FONDO);
    if (z->vista == FZ_POTENCIAS) {
        barra("Triángulo de potencias", s, conv);
        dibuja_potencias(z, c, s);
        pie("[izq/der] fasores [arr/ab] elige [S] Boucherot");
    } else {
        barra("Diagrama fasorial", s, conv);
        dibuja_fasores(z, c, s, conv, rect);
        pie("[izq/der] potencias [arr/ab] elige [enter] ver");
    }
}

/* Que la línea sel se vea en la lista */
static void a_la_vista(int sel, int *ini)
{
    if (sel < *ini) *ini = sel;
    if (sel >= *ini + LEY_N) *ini = sel - LEY_N + 1;
    if (*ini < 0) *ini = 0;
}

int fz_tecla(Fasorial *z, const Circuito *c, const CaRes *s, int tk)
{
    int t = tk & ~T_MODS, conv = c->conv, f;

    if (t == T_TAB || t == T_ESC) return FZ_SALIR;
    if (t == T_IZQ || t == T_DER) {
        z->vista = (unsigned char)(z->vista == FZ_FASORES ? FZ_POTENCIAS : FZ_FASORES);
        return FZ_NADA;
    }
    if (z->vista == FZ_POTENCIAS) {
        int np = fz_npot(c, s);
        if (t == T_ARRIBA && z->selp > 0) z->selp--;
        else if (t == T_ABAJO && z->selp < np) z->selp++;
        else if (t == T_LETRA('S')) z->cadena = (unsigned char)(z->cadena ^ FZ_BOUCHEROT);
        if (z->selp > np) z->selp = np;
        if (z->selp < 0) z->selp = 0;
        a_la_vista(z->selp, &z->inip);
        return FZ_NADA;
    }
    if (z->n <= 0) return FZ_NADA;
    if (z->sel < 0 || z->sel >= z->n) z->sel = 0;
    f = fam(z, z->sel);
    if (t == T_ARRIBA || t == T_ABAJO) {
        int sel = z->sel + (t == T_ABAJO ? 1 : -1);
        if (sel >= 0 && sel < z->n) z->sel = sel;
        a_la_vista(z->sel, &z->ini);
    } else if (t == T_ENTER) {
        z->ver[z->sel] = (unsigned char)!z->ver[z->sel];
        if (!z->ajustado[f]) auto_fam(z, c, s, conv, f);
    } else if (t == T_LETRA('R')) {
        z->ref = z->ref == z->sel ? -1 : z->sel;
    } else if (t == T_LETRA('S')) {
        int antes = fam_cadena(z);
        z->cadena = (unsigned char)((z->cadena & ~3) | (antes == f ? 0 : f + 1));
        if (!z->ajustado[0]) auto_fam(z, c, s, conv, 0);
        if (!z->ajustado[1]) auto_fam(z, c, s, conv, 1);
    } else if (t == T_MAS || t == T_MENOS) {
        int ie = iesc_de(z, f) + (t == T_MAS ? -1 : 1);
        if (ie >= 0 && ie < N_ESCALA) z->iesc[f] = (signed char)ie;
        z->ajustado[f] = 1;
    } else if (t == T_LETRA('A')) {
        fz_autoescala(z, c, s, conv);
    }
    return FZ_NADA;
}
