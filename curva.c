/* ============================================================================
 *  curva.c  -  La pantalla del barrido de CC  (ver curva.h; Etapa 4, §4.8)
 *
 *  Orden del archivo:
 *    1. funciones puras: columnas, filas, valor interpolado, autoescala y
 *       cruces
 *    2. formato y avisos
 *    3. la pantalla: cabecera, retícula, rótulos, trazas, marcas, cursores,
 *       eje x, panel y pie
 *    4. las teclas, en el orden de la especificación (§4.8)
 * ========================================================================= */
#include <string.h>
#include <math.h>
#include "curva.h"
#include "osc.h"                            /* ESCALA y osc_escala_ge */
#include "numfmt.h"
#include "ui.h"

#define CV_X1       (CV_X0 + 10 * CV_DX)    /* 314: borde derecho            */
#define CV_Y1       (CV_Y0 + 8 * CV_DY)     /* 161: abajo                    */
#define CV_W        (CV_X1 - CV_X0)         /* 280                           */
#define COLS        52                      /* columnas de texto en x = 4    */
#define MIN_VISTA   20                      /* menos puntos a la vista: aviso */
#define LOG_ARRIBA_MIN (-4)                 /* log: la línea de arriba, de 1e-4 */
#define LOG_ARRIBA_MAX 3                    /* a 1e3 (se ve de 1 pA a 1 kA)   */
#define POS_Y       8                       /* posición del cero: medias divisiones */
#define POS_X       10                      /* la del eje x en X-Y            */
#define LIM_PX      200                     /* al dibujar, tan fuera como mucho */
#define MAX_CRUCES  64
#define TOL_REL     1e-9                    /* dos x son el mismo            */
#define ORO         0.61803398874989485     /* sección áurea                 */
#define IT_ORO      40

/* ==========================================================================
 *  1. Funciones puras
 * ======================================================================= */
static int hay_malla(const BarrReg *g)
{
    return g && g->n >= 2 && g->ntr > 0 && g->x[g->n - 1] > g->x[0];
}

/* x de lo barrido en unidades de la ventana (log10 en una malla logarítmica) */
static double ux(const Curva *z, double x)
{
    if (!z->log_x) return x;
    return x > 0.0 ? log10(x) : -1e300;
}
static double de_ux(const Curva *z, double u) { return z->log_x ? pow(10.0, u) : u; }

static double ancho(const Curva *z)
{
    double w = z->x1 - z->x0;
    return w > 0.0 ? w : 1.0;
}

/* Los extremos del barrido, en unidades de la ventana */
static double lim0(const Curva *z, const BarrReg *g) { return ux(z, g->x[0]); }
static double lim1(const Curva *z, const BarrReg *g) { return ux(z, g->x[g->n - 1]); }

int cv_x(const Curva *z, double x)
{
    double d = (ux(z, x) - z->x0) / ancho(z) * CV_W;       /* NaN o -inf: al borde */
    if (!(d >= -1.0)) d = -1.0;
    if (d > CV_W + 1.0) d = CV_W + 1.0;
    return CV_X0 + (int)floor(d + 0.5);
}

/* La columna de u (unidades de la ventana) para dibujar: fuera, lo justo para
 * que la pendiente de una línea que sale sea la buena */
static int col_u(const Curva *z, double u)
{
    double d = (u - z->x0) / ancho(z) * CV_W;
    if (!(d >= -LIM_PX)) d = -LIM_PX;
    if (d > CV_W + LIM_PX) d = CV_W + LIM_PX;
    return CV_X0 + (int)floor(d + 0.5);
}

static double vdiv(const Curva *z, int tr)
{
    int i = z->idiv[tr] < 0 ? 0 : z->idiv[tr] >= N_ESCALA ? N_ESCALA - 1 : z->idiv[tr];
    return ESCALA[i];
}

/* Fila de y con la escala de la traza tr, sin limitar (log: el 0 y los
 * negativos, muy abajo) */
static double fila_d(const Curva *z, int tr, double y)
{
    if (z->log_y[tr]) {
        if (!(y > 0.0)) return 1e9;
        return CV_Y0 + ((double)z->pos[tr] - log10(y)) * CV_DY;
    }
    return CV_YC - (y / vdiv(z, tr) + 0.5 * z->pos[tr]) * CV_DY;
}

int cv_y(const Curva *z, int tr, double y)
{
    double f;
    if (tr < 0 || tr >= BARR_TRAZAS) return CV_Y0 - 1;
    f = fila_d(z, tr, y);
    if (z->log_y[tr] && !(f <= CV_Y1)) f = CV_Y1;         /* el 0, los negativos y lo de abajo: al pie */
    if (!(f >= CV_Y0 - 1)) f = CV_Y0 - 1;                 /* también un NaN */
    if (f > CV_Y1 + 1) f = CV_Y1 + 1;
    return (int)floor(f + 0.5);
}

/* La fila para dibujar (como col_u) */
static int fila_px(const Curva *z, int tr, double y)
{
    double f = fila_d(z, tr, y);
    if (z->log_y[tr] && !(f <= CV_Y1)) f = CV_Y1;
    if (!(f >= CV_Y0 - LIM_PX)) f = CV_Y0 - LIM_PX;
    if (f > CV_Y1 + LIM_PX) f = CV_Y1 + LIM_PX;
    return (int)floor(f + 0.5);
}

/* En X-Y, la columna del valor v del canal 1 (la traza t1 lleva su escala:
 * 10 divisiones y el cero en medias divisiones desde el centro) */
static double col_xy_d(const Curva *z, int t1, double v)
{
    return CV_X0 + 5 * CV_DX + (v / vdiv(z, t1) + 0.5 * z->pos[t1]) * CV_DX;
}

static int col_xy(const Curva *z, int t1, double v)
{
    double d = col_xy_d(z, t1, v);
    if (!(d >= CV_X0 - LIM_PX)) d = CV_X0 - LIM_PX;
    if (d > CV_X1 + LIM_PX) d = CV_X1 + LIM_PX;
    return (int)floor(d + 0.5);
}

double cv_valor(const BarrReg *g, int tr, double x)
{
    int lo, hi, n;
    double tol, t;
    if (!hay_malla(g) || tr < 0 || tr >= g->ntr || x != x) return NAN;
    n = g->n;
    tol = TOL_REL * (g->x[n - 1] - g->x[0]);
    /* los bordes de la ventana pueden caer un pelo fuera */
    if (x < g->x[0]) { if (x < g->x[0] - tol) return NAN; x = g->x[0]; }
    if (x > g->x[n - 1]) { if (x > g->x[n - 1] + tol) return NAN; x = g->x[n - 1]; }
    lo = 0; hi = n - 1;                                    /* x[lo] <= x <= x[hi] */
    while (hi - lo > 1) { int m = (lo + hi) / 2; if (g->x[m] <= x) lo = m; else hi = m; }
    if (x == g->x[lo]) return g->ok[tr][lo] ? g->y[tr][lo] : NAN;
    if (x == g->x[hi]) return g->ok[tr][hi] ? g->y[tr][hi] : NAN;
    if (!g->ok[tr][lo] || !g->ok[tr][hi]) return NAN;       /* hueco */
    t = (x - g->x[lo]) / (g->x[hi] - g->x[lo]);
    return g->y[tr][lo] + t * (g->y[tr][hi] - g->y[tr][lo]);
}

/* ¿Cae el punto i de la malla en la ventana? */
static int en_vista(const Curva *z, const BarrReg *g, int i)
{
    double u = ux(z, g->x[i]), tol = TOL_REL * ancho(z);
    return u >= z->x0 - tol && u <= z->x1 + tol;
}

static int n_en_vista(const Curva *z, const BarrReg *g)
{
    int i, n = 0;
    for (i = 0; i < g->n; i++) n += en_vista(z, g, i);
    return n;
}

static int hay_canal_c(const BarrReg *g, int ch)
{
    int t;
    for (t = 0; t < g->ntr; t++) if (g->canal[t] == ch) return 1;
    return 0;
}

/* La traza del canal ch que va con tr (el mismo recorrido: la ida o la vuelta) */
static int pareja(const BarrReg *g, int tr, int ch)
{
    int t;
    for (t = 0; t < g->ntr; t++) if (g->canal[t] == ch && g->recorrido[t] == g->recorrido[tr]) return t;
    return -1;
}

/* Las trazas del mismo canal van con la misma escala */
static void pon_escala(Curva *z, const BarrReg *g, int ch, int iv, int p)
{
    int t;
    for (t = 0; t < g->ntr; t++)
        if (g->canal[t] == ch) { z->idiv[t] = (signed char)iv; z->pos[t] = (signed char)p; }
}

static void pon_log(Curva *z, const BarrReg *g, int ch, int lg)
{
    int t;
    for (t = 0; t < g->ntr; t++) if (g->canal[t] == ch) z->log_y[t] = (unsigned char)lg;
}

/* Lineal: [lo, hi] (con el cero) en ndiv - 2 divisiones y el centro en el
 * medio, con el cero en una línea de la retícula (divisiones enteras: los
 * rótulos son múltiplos de la escala; el centro se va media división como
 * mucho y todo sigue cabiendo) */
static void escala_lin(double lo, double hi, int ndiv, int pmax, int *iv, int *p)
{
    double vd, pd;
    if (lo > 0.0) lo = 0.0;
    if (hi < 0.0) hi = 0.0;
    vd = hi - lo > 0.0 ? (hi - lo) / (double)(ndiv - 2) : 1e-3;
    *iv = osc_escala_ge(vd);
    pd = 2.0 * floor(-0.5 * (hi + lo) / ESCALA[*iv] + 0.5);
    if (pd > pmax) pd = pmax;
    if (pd < -pmax) pd = -pmax;
    *p = (int)pd;
}

/* Las escalas del canal ch con lo que se ve de la ventana (y sus bordes) */
static void autoescala_canal(Curva *z, const BarrReg *g, int ch)
{
    double lo = 1e300, hi = -1e300, amax = 0.0;
    int t, i, k, iv, p, t0 = -1;
    for (t = 0; t < g->ntr; t++) if (g->canal[t] == ch && t0 < 0) t0 = t;
    if (t0 < 0) return;
    if (hay_malla(g))
        for (t = 0; t < g->ntr; t++) {
            if (g->canal[t] != ch) continue;
            for (i = 0; i < g->n; i++) {
                double v;
                if (!g->ok[t][i] || !en_vista(z, g, i)) continue;
                v = g->y[t][i];
                if (v < lo) lo = v;
                if (v > hi) hi = v;
                if (v > amax) amax = v;
            }
            for (k = 0; k < 2; k++) {                      /* los bordes, entre dos puntos */
                double v = cv_valor(g, t, de_ux(z, k ? z->x1 : z->x0));
                if (v != v) continue;
                if (v < lo) lo = v;
                if (v > hi) hi = v;
                if (v > amax) amax = v;
            }
        }
    if (hi < lo) lo = hi = 0.0;
    if (z->log_y[t0]) {
        double a = amax > 0.0 ? ceil(log10(amax) - 1e-9) : LOG_ARRIBA_MIN;
        if (a < LOG_ARRIBA_MIN) a = LOG_ARRIBA_MIN;
        if (a > LOG_ARRIBA_MAX) a = LOG_ARRIBA_MAX;
        pon_escala(z, g, ch, z->idiv[t0], (int)a);
        return;
    }
    if (z->xy && ch == 0) escala_lin(lo, hi, 10, POS_X, &iv, &p);
    else escala_lin(lo, hi, 8, POS_Y, &iv, &p);
    pon_escala(z, g, ch, iv, p);
}

void cv_autoescala(Curva *z, const BarrReg *g)
{
    autoescala_canal(z, g, 0);
    autoescala_canal(z, g, 1);
}

/* Los cruces, con su intervalo de la malla [ia, ib] (los huecos se saltan,
 * como en barr_nivel; un punto justo en el nivel cuenta una vez) */
static int cruces_int(const BarrReg *g, int tr, double nivel, double *xs, int *ia, int *ib, int max)
{
    int i, prev = -1, n = 0;
    if (!g || tr < 0 || tr >= g->ntr) return 0;
    for (i = 0; i < g->n; i++) {
        double f0, f1 = 0.0, x;
        int a = i;
        if (!g->ok[tr][i]) continue;
        f1 = g->y[tr][i] - nivel;
        if (prev < 0) {
            prev = i;
            if (f1 != 0.0) continue;
            x = g->x[i];
        } else {
            a = prev;
            prev = i;
            f0 = g->y[tr][a] - nivel;
            if (f0 == 0.0 || !(f1 == 0.0 || (f0 < 0.0) != (f1 < 0.0))) continue;
            x = f1 == 0.0 ? g->x[i] : g->x[a] + (g->x[i] - g->x[a]) * f0 / (f0 - f1);
        }
        if (n < max) {
            if (xs) xs[n] = x;
            if (ia) ia[n] = a;
            if (ib) ib[n] = i;
        }
        n++;
    }
    return n;
}

int cv_cruces(const BarrReg *g, int tr, double nivel, double *xs, int max)
{
    return cruces_int(g, tr, nivel, xs, 0, 0, max);
}

void cv_ventana(const Curva *z, double *xa, double *xb)
{
    *xa = de_ux(z, z->x0);
    *xb = de_ux(z, z->x1);
}

/* ==========================================================================
 *  2. Formato y avisos
 * ======================================================================= */
/* 'cifras' significativas con prefijo del SI y SIN quitar los ceros (como en
 * trazador.c): "1,250 V", "360,0 Ω", "20,00 mA" con la unidad " V" */
static char *fija(char *buf, size_t cap, double v, int cifras, const char *unidad)
{
    static const char *const PRE[8] = { "p", "n", S_MU, "m", "", "k", "M", "G" };
    double a = fabs(v), m;
    int e3 = 0, dec, ent;
    char t[40];
    Buf b;
    buf_init(&b, buf, cap);
    if (!(a < 1e300)) { buf_str(&b, "?"); return buf; }
    if (a > 0.0) {
        e3 = (int)floor(log10(a) / 3.0) * 3;
        if (e3 < -12) e3 = -12;
        if (e3 > 9) e3 = 9;
    }
    m = v / pow(10.0, (double)e3);
    ent = fabs(m) >= 1.0 ? (int)floor(log10(fabs(m))) + 1 : 1;
    dec = cifras - ent;
    if (dec < 0) dec = 0;
    /* el redondeo puede dar una cifra entera más: 9,99996 es 10,00 */
    if (floor(fabs(m) * pow(10.0, (double)dec) + 0.5) >= pow(10.0, (double)(ent + dec))) {
        ent++;
        dec = cifras - ent > 0 ? cifras - ent : 0;
    }
    if (ent > 3 && e3 < 9) {                               /* 999,96 pasa a 1,000k */
        e3 += 3; m /= 1000.0;
        dec = cifras - 1;
    }
    buf_str(&b, num_txt(t, sizeof t, m, dec));
    if (unidad && unidad[0] == ' ') { buf_ch(&b, ' '); unidad++; }
    buf_str(&b, PRE[e3 / 3 + 4]);
    if (unidad) buf_str(&b, unidad);
    return buf;
}

char *cv_fija(char *buf, size_t cap, double v, int cifras, const char *unidad)
{
    return fija(buf, cap, v, cifras, unidad);
}

/* Cuatro cifras significativas sin prefijo (una ganancia): "-9,999", "10,00" */
static char *sig4(char *buf, size_t cap, double v)
{
    double a = fabs(v);
    int dec;
    if (!(a < 1e300)) { Buf b; buf_init(&b, buf, cap); buf_str(&b, "?"); return buf; }
    if (a == 0.0) return num_txt(buf, cap, 0.0, 0);
    if (a < 1e-3 || a >= 1e4) return fija(buf, cap, v, 4, "");
    dec = 3 - (int)floor(log10(a));
    if (floor(a * pow(10.0, (double)dec) + 0.5) >= 1e4) dec--;    /* 0,99999 es 1,000 */
    if (dec < 0) return fija(buf, cap, v, 4, "");
    return num_txt(buf, cap, v, dec);
}

/* La unidad con su espacio delante (" V"), o nada */
static const char *con_esp(char *buf, size_t cap, const char *u)
{
    Buf b;
    buf_init(&b, buf, cap);
    if (u && u[0]) { buf_ch(&b, ' '); buf_str(&b, u); }
    return buf;
}

static int es_prefijo(char c)
{
    return c == 'p' || c == 'n' || c == S_MU[0] || c == 'm' || c == 'k' || c == 'M' || c == 'G';
}

/* «-5 a 5 V», «0 a 2 mA», «10Ω a 10kΩ»: 3 cifras y el prefijo una vez si es
 * el mismo (el 0 va con cualquiera) */
static char *rango_txt(char *buf, size_t cap, double a, double b, const char *uni)
{
    char ta[24], tb[24], pa = 0, pb = 0;
    size_t la, lb;
    Buf q;
    num_pref(ta, sizeof ta, a, 3, "");
    num_pref(tb, sizeof tb, b, 3, "");
    la = strlen(ta); lb = strlen(tb);
    if (la > 1 && es_prefijo(ta[la - 1])) { pa = ta[la - 1]; ta[la - 1] = 0; }
    if (lb > 1 && es_prefijo(tb[lb - 1])) { pb = tb[lb - 1]; tb[lb - 1] = 0; }
    buf_init(&q, buf, cap);
    if (pa == pb || a == 0.0 || b == 0.0) {
        buf_str(&q, ta); buf_str(&q, " a "); buf_str(&q, tb);
        if (pa || pb || uni[0]) buf_ch(&q, ' ');
        if (pa || pb) buf_ch(&q, pa ? pa : pb);
        buf_str(&q, uni);
    } else {
        buf_str(&q, ta); if (pa) buf_ch(&q, pa); buf_str(&q, uni);
        buf_str(&q, " a ");
        buf_str(&q, tb); if (pb) buf_ch(&q, pb); buf_str(&q, uni);
    }
    return buf;
}

static void avisa(Curva *z, const char *txt, u16 col)
{
    Buf b;
    buf_init(&b, z->aviso, sizeof z->aviso);
    buf_str(&b, txt);
    z->aviso_col = col;
}

static const char *nombre_canal(const BarrRes *r, int ch)
{
    if (ch < 0 || ch > 1) return "y";
    return r->canal[ch].nombre[0] ? r->canal[ch].nombre : (ch ? "canal 2" : "canal 1");
}
static const char *nombre_tr(const BarrRes *r, const BarrReg *g, int tr) { return nombre_canal(r, g->canal[tr]); }
static const char *uni_tr(const BarrRes *r, const BarrReg *g, int tr)
{
    return r->canal[g->canal[tr]].tipo == CANAL_I ? "A" : "V";
}
static const char *uni_x(const BarrRes *r) { return r->uni_x; }

static u16 color_canal(int ch) { return ch ? C_CANAL2 : C_CANAL1; }
static u16 color_tr(const Curva *z, const BarrReg *g, const BarrRes *r, int t)
{
    if (r->modo == BARR_FAMILIA) return t == z->activo ? C_CURSOR : C_CANAL1;
    return color_canal(g->canal[t]);
}

/* «I(D1) = 20,00 mA en R1 = 360,0 Ω (1 de 2)», más corto si no cabe; en un
 * salto (la curva no pasa por el nivel: lo cruza de golpe), «salta por» */
static void aviso_cruce(Curva *z, const BarrReg *g, const BarrRes *r, int tr, double nivel, double x, int i, int n, int salta)
{
    char t[112], u[24], e[8];
    int intento;
    Buf b;
    for (intento = 0; intento < 3; intento++) {
        buf_init(&b, t, sizeof t);
        if (intento < 2) { buf_str(&b, nombre_tr(r, g, tr)); buf_ch(&b, ' '); }
        buf_str(&b, salta ? "salta por " : "= ");
        buf_str(&b, fija(u, sizeof u, nivel, 4, con_esp(e, sizeof e, uni_tr(r, g, tr))));
        buf_str(&b, " en "); buf_str(&b, r->nom_x); buf_str(&b, " = ");
        buf_str(&b, fija(u, sizeof u, x, 4, con_esp(e, sizeof e, uni_x(r))));
        if (n > 1 && intento == 0) {
            buf_str(&b, " ("); buf_int(&b, i + 1); buf_str(&b, " de "); buf_int(&b, n); buf_ch(&b, ')');
        }
        if (strlen(t) <= COLS) break;
    }
    avisa(z, t, C_CURSOR);
}

/* «I(D1) no llega a 50 mA: va de 0 a 21,8 mA.» (o «no baja a») */
static void aviso_sin_cruce(Curva *z, const BarrReg *g, const BarrRes *r, int tr, double nivel)
{
    char t[112], u[32], e[8];
    double lo = 1e300, hi = -1e300;
    int i, intento;
    Buf b;
    for (i = 0; i < g->n; i++) {
        if (!g->ok[tr][i]) continue;
        if (g->y[tr][i] < lo) lo = g->y[tr][i];
        if (g->y[tr][i] > hi) hi = g->y[tr][i];
    }
    for (intento = 0; intento < 2; intento++) {
        buf_init(&b, t, sizeof t);
        if (intento == 0) { buf_str(&b, nombre_tr(r, g, tr)); buf_ch(&b, ' '); buf_str(&b, nivel < lo ? "no baja a " : "no llega a "); }
        else buf_str(&b, nivel < lo ? "No baja a " : "No llega a ");
        buf_str(&b, num_pref(u, sizeof u, nivel, 3, con_esp(e, sizeof e, uni_tr(r, g, tr))));
        if (hi >= lo) { buf_str(&b, ": va de "); buf_str(&b, rango_txt(u, sizeof u, lo, hi, uni_tr(r, g, tr))); }
        buf_ch(&b, '.');
        if (strlen(t) <= COLS) break;
    }
    avisa(z, t, C_ERROR);
}

/* El cruce de la malla j, exacto (Brent en su intervalo) */
static double refina(const BarrReg *g, int tr, double nivel, int a, int b, double xi)
{
    double x;
    if (a == b) return g->x[a];
    if (g->y[tr][b] == nivel) return g->x[b];
    if (barr_nivel(tr, nivel, g->x[a], g->x[b], &x)) return x;
    return xi;
}

/* El cruce de nivel siguiente a 'desde' (estrictamente a su derecha), dando
 * la vuelta. En *x el exacto, en *i cuál es y en *n cuántos hay. */
static int cruce_sig(const BarrReg *g, int tr, double nivel, double desde, double *x, int *i, int *n)
{
    static double xs[MAX_CRUCES];
    static int ia[MAX_CRUCES], ib[MAX_CRUCES];
    int m = cruces_int(g, tr, nivel, xs, ia, ib, MAX_CRUCES), j;
    double tol = 1e-12 * (fabs(desde) + (g->x[g->n - 1] - g->x[0]));
    *n = m;
    if (m == 0) return 0;
    if (m > MAX_CRUCES) m = MAX_CRUCES;
    for (j = 0; j < m; j++) {
        double xr;
        if (g->x[ib[j]] < desde) continue;                 /* todo a la izquierda   */
        if (g->x[ia[j]] > desde) break;                    /* todo a la derecha: este */
        xr = refina(g, tr, nivel, ia[j], ib[j], xs[j]);    /* el cursor está en su intervalo */
        if (xr > desde + tol) { *x = xr; *i = j; return 1; }
    }
    if (j >= m) j = 0;                                     /* la vuelta */
    *x = refina(g, tr, nivel, ia[j], ib[j], xs[j]);
    *i = j;
    return 1;
}

/* La ventana: el barrido entero, o dentro de él con su ancho */
static void inicio(Curva *z, const BarrReg *g)
{
    z->x0 = lim0(z, g); z->x1 = lim1(z, g);
}

static void encaja(Curva *z, const BarrReg *g)
{
    double A = lim0(z, g), B = lim1(z, g), w = z->x1 - z->x0;
    if (!(w > 0.0) || w >= (B - A) * (1.0 - 1e-12)) { inicio(z, g); return; }
    if (z->x0 < A) { z->x0 = A; z->x1 = A + w; }
    if (z->x1 > B) { z->x1 = B; z->x0 = B - w; }
}

/* Que se vea u: centrada en él, o lo justo para que entre */
static void que_se_vea(Curva *z, const BarrReg *g, double u, int centra)
{
    double w = z->x1 - z->x0;
    if (u >= z->x0 && u <= z->x1) return;
    if (centra) { z->x0 = u - 0.5 * w; z->x1 = u + 0.5 * w; }
    else if (u < z->x0) { z->x0 = u; z->x1 = u + w; }
    else { z->x1 = u; z->x0 = u - w; }
    encaja(z, g);
}

static void lleva(Curva *z, const BarrReg *g, double x, int centra)
{
    z->xc[z->cur] = x;
    que_se_vea(z, g, ux(z, x), centra);
}

int cv_ir_nivel(Curva *z, const BarrReg *g, const BarrRes *r, int tr, double nivel, int dato)
{
    static double xs[MAX_CRUCES];
    static int ia[MAX_CRUCES], ib[MAX_CRUCES];
    double x;
    int i = 0, n = 0, sg = 1;
    if (!hay_malla(g) || tr < 0 || tr >= g->ntr) return 0;
    z->activo = (unsigned char)tr;
    if (z->xy && g->canal[tr] != 1) z->xy = 0;             /* el nivel va en el eje y */
    z->nivel_on = 1;
    if (dato) {                                            /* una magnitud: +m y, si no, -m */
        double m = fabs(nivel);
        z->nivel = m;
        if (!barr_nivel_dato(tr, m, g->x[0], g->x[g->n - 1], &x, &sg)) { aviso_sin_cruce(z, g, r, tr, m); return 0; }
        z->nivel = sg * m;
        n = cruces_int(g, tr, z->nivel, xs, ia, ib, MAX_CRUCES);
        for (i = 0; i < n && i < MAX_CRUCES; i++) if (x <= g->x[ib[i]]) break;
        if (i >= n || i >= MAX_CRUCES) i = 0;
    } else {
        z->nivel = nivel;
        if (!cruce_sig(g, tr, nivel, z->xc[z->cur], &x, &i, &n)) { aviso_sin_cruce(z, g, r, tr, nivel); return 0; }
    }
    lleva(z, g, x, 1);
    {
        /* ¿pasa por el nivel o lo salta? (Brent acaba igual en un salto) */
        double y, lo = 1e300, hi = -1e300;
        int q, salta = 0;
        for (q = 0; q < g->n; q++) {
            if (!g->ok[tr][q]) continue;
            if (g->y[tr][q] < lo) lo = g->y[tr][q];
            if (g->y[tr][q] > hi) hi = g->y[tr][q];
        }
        if (hi > lo && barr_en(tr, x, &y, 0) && fabs(y - z->nivel) > 1e-6 * (hi - lo)) salta = 1;
        aviso_cruce(z, g, r, tr, z->nivel, x, i, n, salta);
    }
    return 1;
}

/* El último x hecho (un barrido cortado): el de más a la derecha de la ida
 * o, si la vuelta empezó, el de más a la izquierda de la vuelta */
static int ultimo_hecho(const BarrReg *g, double *x)
{
    int t, i, iv = -1, ii = -1;
    for (t = 0; t < g->ntr; t++)
        for (i = 0; i < g->n; i++) {
            if (!g->ok[t][i]) continue;
            if (g->recorrido[t] == 1) { if (iv < 0 || i < iv) iv = i; }
            else if (i > ii) ii = i;
        }
    if (iv >= 0) { *x = g->x[iv]; return 1; }
    if (ii >= 0) { *x = g->x[ii]; return 1; }
    return 0;
}

/* Dónde salta por primera vez: el último x de la rama de antes */
static int primer_salto(const BarrReg *g, const BarrRes *r, double *x)
{
    int t, i;
    for (t = 0; t < g->ntr; t++) {
        if (r->vuelta && g->recorrido[t] == 1) continue;
        for (i = 1; i < g->n; i++) if (g->salto[t][i]) { *x = g->x[i - 1]; return 1; }
    }
    for (t = 0; t < g->ntr; t++) {
        if (!(r->vuelta && g->recorrido[t] == 1)) continue;
        for (i = g->n - 2; i >= 0; i--) if (g->salto[t][i]) { *x = g->x[i + 1]; return 1; }
    }
    return 0;
}

/* El aviso de lo barrido (sin dato): cortado, huecos, salto, sin sondas, CC */
static void aviso_barrido(Curva *z, const BarrReg *g, const BarrRes *r)
{
    char t[112], u[24], e[8];
    double x;
    Buf b;
    buf_init(&b, t, sizeof t);
    if (r->fin == FIN_CORTADO && ultimo_hecho(g, &x)) {
        buf_str(&b, "Cortado en "); buf_str(&b, r->nom_x); buf_str(&b, " = ");
        buf_str(&b, fija(u, sizeof u, x, 4, con_esp(e, sizeof e, uni_x(r)))); buf_ch(&b, '.');
        avisa(z, t, C_CURSOR);
    } else if (r->huecos > 0) {
        buf_str(&b, "La curva tiene "); buf_int(&b, r->huecos);
        buf_str(&b, r->huecos == 1 ? " hueco: ah" S_ii " Newton no converge." : " huecos: ah" S_ii " Newton no converge.");
        avisa(z, t, C_CURSOR);
    } else if ((r->d.avisos & SIM_AV_SALTO) && primer_salto(g, r, &x)) {
        buf_str(&b, "La curva salta en "); buf_str(&b, r->nom_x); buf_str(&b, " = ");
        buf_str(&b, fija(u, sizeof u, x, 3, con_esp(e, sizeof e, uni_x(r)))); buf_str(&b, " (hist" S_ee "resis).");
        avisa(z, t, C_CURSOR);
    } else if (r->modo == BARR_CIRCUITO && r->canal[0].automatico) {
        int intento;
        for (intento = 0; intento < 3; intento++) {
            buf_init(&b, t, sizeof t);
            if (intento == 2) { buf_str(&b, "Canales autom" S_aa "ticos: pon sondas con [O]."); break; }
            buf_str(&b, "Sin sondas: "); buf_str(&b, nombre_canal(r, 0));
            if (r->canal[1].tipo != CANAL_NADA) { buf_str(&b, " y "); buf_str(&b, nombre_canal(r, 1)); }
            buf_str(&b, intento ? ". Ponlas con [O]." : ". Pon las tuyas con [O].");
            if (strlen(t) <= COLS) break;
        }
        avisa(z, t, C_SUAVE);
    } else if (r->d.avisos & SIM_AV_CC_BARRIDO) avisa(z, "Barrido de CC: C abiertos, L cables.", C_SUAVE);
}

void cv_nuevo(Curva *z, const BarrReg *g, const BarrRes *r)
{
    int t, k, nuevos, dos_canales = g && hay_canal_c(g, 0) && hay_canal_c(g, 1);
    z->log_x = r->log_x;
    z->entrada[0] = 0;
    z->de_nivel = 0;
    z->aviso[0] = 0;
    z->aviso_col = C_SUAVE;
    if (hay_malla(g)) inicio(z, g);
    else if (r->x1 > r->x0 && (!r->log_x || r->x0 > 0.0)) { z->x0 = ux(z, r->x0); z->x1 = ux(z, r->x1); }
    else { z->x0 = 0.0; z->x1 = 1.0; }
    for (t = 0; t < BARR_TRAZAS; t++) {
        z->ver[t] = (unsigned char)(g && t < g->ntr);
        if (!g || t >= g->ntr) z->log_y[t] = 0;
    }
    if (g)                                                 /* la escala |y| log sigue a su canal */
        for (k = 0; k < 2; k++) {
            int t0 = -1;
            for (t = 0; t < g->ntr; t++) if (g->canal[t] == k && t0 < 0) t0 = t;
            if (t0 >= 0) pon_log(z, g, k, z->log_y[t0]);
        }
    z->xy = (unsigned char)((r->xy || z->xy) && dos_canales && r->modo != BARR_FAMILIA);
    if (g && (z->activo >= g->ntr || (z->xy && g->canal[z->activo] != 1))) {
        z->activo = 0;
        if (z->xy) for (t = 0; t < g->ntr; t++) if (g->canal[t] == 1) { z->activo = (unsigned char)t; break; }
    }
    if (z->xy && g) pon_log(z, g, 0, 0);                   /* el eje x de X-Y es lineal */
    if (z->dos > 1) z->dos = 1;
    if (!z->dos || z->cur > 1) z->cur = 0;
    /* los cursores que caen dentro se quedan; los de una Curva nueva (los dos
     * en 0, como en osc.c) o los de fuera, al centro y a tres cuartos */
    nuevos = z->xc[0] == 0.0 && z->xc[1] == 0.0;
    for (k = 0; k < 2; k++)
        if (nuevos || !hay_malla(g) || !(z->xc[k] >= g->x[0]) || !(z->xc[k] <= g->x[g->n - 1]))
            z->xc[k] = de_ux(z, z->x0 + (k ? 0.75 : 0.5) * (z->x1 - z->x0));
    if (!hay_malla(g) || r->fin == FIN_ERROR) return;
    if (r->fin == FIN_CORTADO) {                           /* cortado en la ida: los cursores, en lo hecho */
        double xu;
        int vuelta_hecha = 0;
        for (t = 0; t < g->ntr; t++)
            if (g->recorrido[t] == 1) for (k = 0; k < g->n; k++) vuelta_hecha |= g->ok[t][k];
        if (!vuelta_hecha && ultimo_hecho(g, &xu))
            for (k = 0; k < 2; k++) if (z->xc[k] > xu) z->xc[k] = xu;
    }
    cv_autoescala(z, g);
    if (r->tr_dato >= 0 && r->tr_dato < g->ntr) {          /* «¿qué R da 20 mA?» */
        cv_ir_nivel(z, g, r, r->tr_dato, r->nivel_dato, 1);
        return;
    }
    aviso_barrido(z, g, r);
}

/* ==========================================================================
 *  3. La pantalla
 * ======================================================================= */
/* s partido en líneas de 52 columnas por los espacios (el corte de ui.c) */
static void parrafo(int x, int y, const char *s, u16 col, int nmax)
{
    char lin[COLS + 1];
    int nl = 0;
    while (*s && nl < nmax) {
        int n = 0, corte = 0;
        while (s[n] && s[n] != '\n' && n < COLS) { if (s[n] == ' ') corte = n; n++; }
        if (s[n] && s[n] != '\n' && corte > 0) n = corte;
        memcpy(lin, s, (size_t)n);
        lin[n] = 0;
        gfx_text(x, y + 13 * nl, lin, col, 1);
        nl++;
        s += n;
        if (*s == '\n') s++;
        else while (*s == ' ') s++;
    }
}

/* El título: «I(D1) y V(D1) frente a E1»; en X-Y, «I(D1) frente a V(D1)»;
 * el del trazador. Más corto en cada intento. */
static void titulo(char *t, size_t cap, const Curva *z, const BarrReg *g, const BarrRes *r, int intento)
{
    Buf b;
    int dos = hay_canal_c(g, 0) && hay_canal_c(g, 1);
    buf_init(&b, t, cap);
    if (r->titulo[0]) { buf_str(&b, r->titulo); return; }
    if (z->xy) {
        buf_str(&b, nombre_canal(r, 1));
        if (intento < 2) { buf_str(&b, " frente a "); buf_str(&b, nombre_canal(r, 0)); }
        return;
    }
    buf_str(&b, nombre_canal(r, 0));
    if (dos && intento == 0) { buf_str(&b, " y "); buf_str(&b, nombre_canal(r, 1)); }
    if (intento < 2) { buf_str(&b, " frente a "); buf_str(&b, r->nom_x); }
}

static void cabecera(const Curva *z, const BarrReg *g, const BarrRes *r)
{
    char t[80], u[40];
    int xr, cab, intento;
    gfx_fill(0, 0, SCR_W, 15, C_PANEL);
    /* lo barrido en x = 200 (o antes, si es largo: desde 268, el aviso del shift) */
    rango_txt(u, sizeof u, r->x0, r->x1, r->uni_x);
    xr = 268 - gfx_text_w(u, 1);
    if (xr > 200) xr = 200;
    gfx_text(xr, 3, u, C_SUAVE, 1);
    cab = (xr - 6 - 4) / FONT_W;                           /* columnas del título */
    for (intento = 0; intento < 3; intento++) {
        titulo(t, sizeof t, z, g, r, intento);
        if ((int)strlen(t) <= cab) break;
    }
    if ((int)strlen(t) > cab) t[cab > 0 ? cab : 0] = 0;
    gfx_text(4, 3, t, C_TEXTO, 1);
}

static void punteada_v(int x, int paso)
{
    int y;
    for (y = CV_Y0; y <= CV_Y1; y += paso) gfx_px(x, y, C_REJILLA);
}

/* Décadas de la ventana logarítmica: de d0 a d1 (a lo sumo 40) */
static void decadas(const Curva *z, int *d0, int *d1)
{
    double a = floor(z->x0 + 1e-9), b = floor(z->x1 + 1e-9);
    if (a < -20.0) a = -20.0;
    if (b > 20.0) b = 20.0;
    if (b < a) b = a;
    *d0 = (int)a; *d1 = (int)b;
}

static int en_ventana(const Curva *z, double u)
{
    double tol = 1e-9 * ancho(z);
    return u >= z->x0 - tol && u <= z->x1 + tol;
}

static void reticula(const Curva *z, const BarrReg *g)
{
    int i, x, y, t1 = z->xy ? pareja(g, z->activo, 0) : -1;
    for (i = 0; i <= 8; i++)
        for (x = CV_X0; x <= CV_X1; x += 3) gfx_px(x, CV_Y0 + i * CV_DY, C_REJILLA);
    if (z->log_x && !z->xy) {                              /* una vertical por década, marcas de 2 a 9 */
        int d, d0, d1, m, fino = z->x1 - z->x0 <= 1.5;
        punteada_v(CV_X0, 3);
        punteada_v(CV_X1, 3);
        decadas(z, &d0, &d1);
        for (d = d0; d <= d1; d++)
            for (m = 1; m <= 9; m++) {
                double lf = (double)d + log10((double)m);
                if (!en_ventana(z, lf)) continue;
                x = col_u(z, lf);
                if (m == 1) { punteada_v(x, 3); continue; }
                if (fino) punteada_v(x, 6);
                gfx_vline(x, CV_Y1 - 2, 3, C_TENUE);
            }
    } else for (i = 0; i <= 10; i++) punteada_v(CV_X0 + i * CV_DX, 3);
    /* los ejes: y = 0 del canal activo y x = 0, si caen dentro */
    if (!z->log_y[z->activo]) {
        double f = fila_d(z, z->activo, 0.0);
        y = (int)floor(f + 0.5);
        if (y > CV_Y0 && y < CV_Y1) gfx_hline(CV_X0, y, CV_W + 1, C_TENUE);
    }
    x = -1;
    if (z->xy) { if (t1 >= 0) x = (int)floor(col_xy_d(z, t1, 0.0) + 0.5); }
    else if (!z->log_x && en_ventana(z, 0.0)) x = col_u(z, 0.0);
    if (x > CV_X0 && x < CV_X1) gfx_vline(x, CV_Y0, 8 * CV_DY + 1, C_TENUE);
}

/* Las divisiones con rótulo (cada 2): las de la paridad de la del cero, si
 * el cero cae en una línea (pos par); si no, desde la primera */
static int paridad_cero(int centro, int pos)
{
    if (pos & 1) return 0;
    return ((centro + pos / 2) % 2 + 2) % 2;
}

/* Los rótulos de y del canal activo, cada 2 divisiones, a la derecha de x = 31 */
static void rotulos_y(const Curva *z, const BarrReg *g)
{
    int i, tr = z->activo;
    u16 col = color_canal(g->canal[tr]);
    for (i = z->log_y[tr] ? 0 : paridad_cero(4, z->pos[tr]); i <= 8; i += 2) {
        char t[16];
        int x, y = CV_Y0 + i * CV_DY - 3;
        if (z->log_y[tr]) num_pref(t, sizeof t, pow(10.0, (double)(z->pos[tr] - i)), 3, "");
        else {
            double vd = vdiv(z, tr), v = (4.0 - i - 0.5 * z->pos[tr]) * vd;
            if (fabs(v) < 1e-9 * vd) v = 0.0;
            num_pref(t, sizeof t, v, 3, "");
        }
        if (y < 15) y = 15;                                /* bajo la cabecera */
        x = 32 - gfx_text_w(t, 1);
        gfx_text_p(x > 0 ? x : 0, y, t, col);
    }
}

/* Un trazo con discontinuidad que sigue de un tramo al siguiente (la vuelta):
 * 3 px sí y 2 no, Bresenham entero. El primer píxel de un tramo es el último
 * del anterior: no cuenta dos veces. */
static void linea_fase(int x0, int y0, int x1, int y1, u16 c, int *fase)
{
    int dx = (x1 > x0 ? x1 - x0 : x0 - x1);
    int dy = -(y1 > y0 ? y1 - y0 : y0 - y1);
    int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2, primero = *fase > 0;
    for (;;) {
        if (!primero) {
            if (*fase % 5 < 3) gfx_px(x0, y0, c);
            (*fase)++;
        }
        primero = 0;
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

static void segmento(int x0, int y0, int x1, int y1, u16 c, int disc, int *fase)
{
    if (disc) linea_fase(x0, y0, x1, y1, c, fase);
    else gfx_line(x0, y0, x1, y1, c);
}

/* La columna de cada punto de la malla (y(x)); en cada cuadro, una vez */
static short col_malla[BARR_N];

/* Una traza: línea por los puntos de la malla; un hueco la corta; un salto,
 * la rama de antes hasta el x del salto y un trazo vertical punteado; la
 * vuelta, discontinua; en una familia, su rótulo a la izquierda de su final */
static void traza(const Curva *z, const BarrReg *g, const BarrRes *r, int t)
{
    int i, i0 = -1, i1 = -1, hay = 0, xa = 0, ya = 0, fase = 0, xf = -1, yf = 0;
    int t1 = z->xy ? pareja(g, t, 0) : -1;
    int vuelta = r->vuelta && r->modo != BARR_FAMILIA && g->recorrido[t] == 1;
    u16 col = color_tr(z, g, r, t);
    if (z->xy && t1 < 0) return;
    for (i = 0; i < g->n; i++) if (en_vista(z, g, i)) { if (i0 < 0) i0 = i; i1 = i; }
    if (i0 < 0) return;
    if (!z->xy) {                                          /* un punto más por cada lado: la línea llega al borde */
        if (i0 > 0) i0--;
        if (i1 < g->n - 1) i1++;
    }
    for (i = i0; i <= i1; i++) {
        int x, y;
        if (!g->ok[t][i] || (t1 >= 0 && !g->ok[t1][i])) { hay = 0; continue; }
        x = z->xy ? col_xy(z, t1, g->y[t1][i]) : col_malla[i];
        y = fila_px(z, t, g->y[t][i]);
        if (!hay) gfx_px(x, y, col);
        else if (vuelta ? g->salto[t][i - 1] : g->salto[t][i]) {
            /* de donde sale (d) a donde llega (l): la ida llega a i; la vuelta, a i - 1 */
            int dx = vuelta ? x : xa, dy = vuelta ? y : ya, lx = vuelta ? xa : x, ly = vuelta ? ya : y;
            segmento(dx, dy, lx, dy, col, vuelta, &fase);
            gfx_line_dash(lx, dy, lx, ly, col, 1, 2);
        } else segmento(xa, ya, x, y, col, vuelta, &fase);
        xa = x; ya = y; hay = 1;
        if (en_vista(z, g, i)) { xf = x; yf = y; }
    }
    if (r->modo == BARR_FAMILIA && xf >= 0) {              /* «40µ», el valor de su IB o VGS */
        const char *p = strchr(r->nom_tr[t], '=');
        const char *s = p ? p + 1 : r->nom_tr[t];
        int w = gfx_text_w(s, 1), x = xf - w - 2, y = yf - 4;
        if (x < CV_X0 + 1) x = CV_X0 + 1;
        if (y < CV_Y0 + 1) y = CV_Y0 + 1;
        if (y > CV_Y1 - 8) y = CV_Y1 - 8;
        gfx_text_p_bg(x, y, s, col, C_FONDO);
    }
}

static void trazas(const Curva *z, const BarrReg *g, const BarrRes *r)
{
    int i, q;
    if (!z->xy) for (i = 0; i < g->n; i++) col_malla[i] = (short)col_u(z, ux(z, g->x[i]));
    gfx_clip(CV_X0, CV_Y0, CV_W + 1, 8 * CV_DY + 1);
    for (q = 0; q <= g->ntr; q++) {                        /* la activa, la última (encima) */
        int t = q < g->ntr ? q : z->activo;
        if ((q < g->ntr && t == z->activo) || !z->ver[t]) continue;
        if (z->xy && g->canal[t] != 1) continue;           /* en X-Y, el canal 2 frente al 1 */
        traza(z, g, r, t);
    }
    gfx_clip_reset();
}

/* El cero de cada canal (un triángulo en x = 315) y el nivel de [N] */
static void marcas(const Curva *z, const BarrReg *g, const BarrRes *r)
{
    int ch, x, y;
    for (ch = 1; ch >= 0; ch--) {
        int t, t0 = -1, alguna = 0;
        for (t = 0; t < g->ntr; t++)
            if (g->canal[t] == ch) { if (t0 < 0) t0 = t; if (z->ver[t]) alguna = 1; }
        if (t0 < 0 || !alguna || z->log_y[t0] || (z->xy && ch == 0)) continue;
        y = (int)floor(fila_d(z, t0, 0.0) + 0.5);
        if (y < CV_Y0 + 3) y = CV_Y0 + 3;
        if (y > CV_Y1 - 3) y = CV_Y1 - 3;
        gfx_tri(319, y - 3, 319, y + 3, 315, y, r->modo == BARR_FAMILIA ? C_CANAL1 : color_canal(ch));
    }
    if (z->nivel_on) {
        u16 col = color_canal(g->canal[z->activo]);
        y = cv_y(z, z->activo, z->nivel);
        if (y >= CV_Y0 && y <= CV_Y1) {
            for (x = CV_X0; x <= CV_X1; x += 2) gfx_px(x, y, col);
            gfx_text_p_bg(36, y - 9 >= CV_Y0 ? y - 9 : y + 2, "N", col, C_FONDO);
        }
    }
}

static void cursores(const Curva *z, const BarrReg *g)
{
    int q, t;
    for (q = 0; q < 2; q++) {
        int k = q == 0 ? 1 - z->cur : z->cur, x;             /* el activo, encima */
        u16 col = k == z->cur ? C_TEXTO : C_SUAVE;
        char n[4], u[4];
        Buf b;
        if (k == 1 && !z->dos) continue;
        n[0] = 0;
        if (z->dos) { n[0] = (char)('1' + k); n[1] = 0; }
        if (z->xy) {
            int t1 = pareja(g, z->activo, 0);
            double v1 = t1 >= 0 ? cv_valor(g, t1, z->xc[k]) : NAN;
            if (v1 != v1) continue;
            x = (int)floor(col_xy_d(z, t1, v1) + 0.5);
            if (x < CV_X0 || x > CV_X1) continue;
        } else {
            x = cv_x(z, z->xc[k]);
            if (x < CV_X0 || x > CV_X1) {                  /* fuera de la ventana: una flecha */
                buf_init(&b, u, sizeof u);
                if (x < CV_X0) { buf_str(&b, S_LARR); buf_str(&b, n); gfx_text_p_bg(CV_X0 + 2, CV_Y1 - 9 - 9 * k, u, col, C_FONDO); }
                else { buf_str(&b, n); buf_str(&b, S_RARR); gfx_text_p_bg(CV_X1 - 1 - gfx_text_w(u, 1), CV_Y1 - 9 - 9 * k, u, col, C_FONDO); }
                continue;
            }
        }
        gfx_line_dash(x, CV_Y0, x, CV_Y1, col, 3, 2);
        for (t = 0; t < g->ntr; t++) {                     /* un cuadrito en cada traza */
            double v;
            int y;
            if (!z->ver[t] || (z->xy && g->canal[t] != 1)) continue;
            v = cv_valor(g, t, z->xc[k]);
            if (v != v) continue;
            y = cv_y(z, t, v);
            if (z->xy) {
                int t1 = pareja(g, t, 0);
                double v1 = t1 >= 0 ? cv_valor(g, t1, z->xc[k]) : NAN;
                int xx;
                if (v1 != v1) continue;
                xx = (int)floor(col_xy_d(z, t1, v1) + 0.5);
                if (xx >= CV_X0 && xx <= CV_X1 && y >= CV_Y0 && y <= CV_Y1) gfx_rect(xx - 1, y - 1, 3, 3, col);
            } else if (y >= CV_Y0 && y <= CV_Y1) gfx_rect(x - 1, y - 1, 3, 3, col);
        }
        if (n[0]) gfx_text_p_bg(x <= CV_X1 - 9 ? x + 2 : x - 7, CV_Y0 + 2, n, col, C_FONDO);
    }
}

/* Rótulos del eje x (6×8, y = 163): ninguno a menos de 24 px de otro ni
 * pegado a otro; el primero, a la derecha de los rótulos de y */
static int rot_x[24], rot_a[24], rot_b[24], n_rot;

static int rotula(int x, const char *t)
{
    int i, w = gfx_text_w(t, 1), xl = x - w / 2;
    if (n_rot >= 24) return 0;
    if (xl < CV_X0 - 1) xl = CV_X0 - 1;
    if (xl + w > SCR_W) xl = SCR_W - w;
    for (i = 0; i < n_rot; i++) {
        if (x - rot_x[i] < 24 && rot_x[i] - x < 24) return 0;
        if (xl < rot_b[i] + 6 && xl + w + 6 > rot_a[i]) return 0;
    }
    gfx_text_p(xl, CV_EJE_Y, t, C_SUAVE);
    rot_x[n_rot] = x; rot_a[n_rot] = xl; rot_b[n_rot] = xl + w;
    n_rot++;
    return 1;
}

/* Las divisiones desde, desde + 2, ... 10 de v0 (la 0) a v1 (la 10), con las
 * cifras justas para que dos rótulos seguidos no digan lo mismo */
static void eje_lineal(double v0, double v1, int desde)
{
    char t[6][16];
    int cif, i, n = (10 - desde) / 2 + 1;
    for (cif = 3; cif <= 6; cif++) {
        int distintos = 1;
        for (i = 0; i < n; i++) {
            double v = v0 + (v1 - v0) * (double)(desde + 2 * i) / 10.0;
            if (fabs(v) < 1e-9 * fabs(v1 - v0)) v = 0.0;
            num_pref(t[i], sizeof t[i], v, cif, "");
            if (i && strcmp(t[i], t[i - 1]) == 0) distintos = 0;
        }
        if (distintos) break;
    }
    for (i = 0; i < n; i++) rotula(CV_X0 + (desde + 2 * i) * CV_DX, t[i]);
}

static void eje_x(const Curva *z, const BarrReg *g)
{
    n_rot = 0;
    if (z->xy) {
        int t1 = pareja(g, z->activo, 0);
        double vd, p;
        if (t1 < 0) return;
        vd = vdiv(z, t1); p = 0.5 * z->pos[t1];
        eje_lineal((-5.0 - p) * vd, (5.0 - p) * vd, paridad_cero(5, z->pos[t1]));
    } else if (z->log_x) {                                 /* las décadas; si son pocas, de 2 a 9; los bordes */
        int d, d0, d1, m, pasada, n = 0;
        char t[16];
        decadas(z, &d0, &d1);
        for (pasada = 0; pasada < 2; pasada++) {
            if (pasada == 1 && n >= 2) break;
            for (d = d0; d <= d1; d++)
                for (m = pasada ? 2 : 1; m <= (pasada ? 9 : 1); m++) {
                    double lf = (double)d + log10((double)m);
                    if (en_ventana(z, lf)) n += rotula(col_u(z, lf), num_pref(t, sizeof t, pow(10.0, lf), 3, ""));
                }
        }
        if (n < 2) {
            rotula(CV_X0, num_pref(t, sizeof t, pow(10.0, z->x0), 3, ""));
            rotula(CV_X1, num_pref(t, sizeof t, pow(10.0, z->x1), 3, ""));
        }
    } else eje_lineal(z->x0, z->x1, 0);
}

/* ---- El panel ---- */
static int escribiendo(const Curva *z) { return z->entrada[0] || z->de_nivel; }

/* «→ E1 = 2,5_   (= 2,500 V)» o «→ I(D1) = 20m_   (= 20,00 mA)» */
static void txt_entrada(char *t, size_t cap, const Curva *z, const BarrReg *g, const BarrRes *r)
{
    char u[24], e[8];
    int ok;
    double v = num_lee_ing(z->entrada, &ok);
    const char *uni = z->de_nivel ? uni_tr(r, g, z->activo) : uni_x(r);
    Buf b;
    buf_init(&b, t, cap);
    buf_str(&b, S_RARR " ");
    buf_str(&b, z->de_nivel ? nombre_tr(r, g, z->activo) : r->nom_x);
    buf_str(&b, " = "); buf_str(&b, z->entrada); buf_ch(&b, '_');
    if (ok) { buf_str(&b, "   (= "); buf_str(&b, fija(u, sizeof u, v, 4, con_esp(e, sizeof e, uni))); buf_ch(&b, ')'); }
}

/* Las trazas que se leen en el cursor: las dos del recorrido de la activa (en
 * una familia, solo la activa) */
static int trazas_leidas(const BarrReg *g, const BarrRes *r, int activo, int *ts)
{
    int n = 0, ch;
    if (r->modo == BARR_FAMILIA) { ts[0] = activo; return 1; }
    for (ch = 0; ch < 2; ch++) {
        int t = pareja(g, activo, ch);
        if (t >= 0) ts[n++] = t;
    }
    return n;
}

/* «E1 = 1,250 V  I(D1) = 3,400 mA  V(D1) = 612,3 mV» (de barr_en) */
static void txt_lectura(char *t, size_t cap, const Curva *z, const BarrReg *g, const BarrRes *r)
{
    char u[24], e[8];
    double x = z->xc[z->cur];
    int ts[2], nt = trazas_leidas(g, r, z->activo, ts), sep, q;
    Buf b;
    for (sep = 2; sep >= 0; sep--) {
        buf_init(&b, t, cap);
        buf_str(&b, r->nom_x); buf_str(&b, " = ");
        buf_str(&b, fija(u, sizeof u, x, 4, con_esp(e, sizeof e, uni_x(r))));
        for (q = 0; q < nt; q++) {
            double y, d;
            if (sep == 0 && ts[q] != z->activo) continue;  /* no cabe: solo la activa */
            buf_str(&b, sep == 1 ? " " : "  ");
            buf_str(&b, nombre_tr(r, g, ts[q])); buf_str(&b, z->rafaga ? " " S_APROX " " : " = ");
            if (z->rafaga) {                               /* de la malla: la exacta, al soltar */
                y = cv_valor(g, ts[q], x);
                if (y == y) buf_str(&b, fija(u, sizeof u, y, 4, con_esp(e, sizeof e, uni_tr(r, g, ts[q]))));
                else buf_ch(&b, '?');
            }
            else if (barr_en(ts[q], x, &y, &d)) buf_str(&b, fija(u, sizeof u, y, 4, con_esp(e, sizeof e, uni_tr(r, g, ts[q]))));
            else buf_ch(&b, '?');
        }
        if (strlen(t) <= COLS) return;
    }
}

/* «x1 = 500,0 mV  x2 = 1,500 V  Δ = 1,000 V» */
static void txt_dos_x(char *t, size_t cap, const Curva *z, const BarrRes *r)
{
    char u[24], e[8];
    const char *uni = con_esp(e, sizeof e, uni_x(r));
    Buf b;
    buf_init(&b, t, cap);
    buf_str(&b, "x1 = "); buf_str(&b, fija(u, sizeof u, z->xc[0], 4, uni));
    buf_str(&b, "  x2 = "); buf_str(&b, fija(u, sizeof u, z->xc[1], 4, uni));
    buf_str(&b, "  " S_DELTA " = "); buf_str(&b, fija(u, sizeof u, z->xc[1] - z->xc[0], 4, uni));
}

/* La unidad de dy/dx: S (A/V), Ω (V/A), nada (la misma), la de y (x sin
 * unidad: una ganancia) o y/x */
static const char *uni_cociente(char *buf, size_t cap, const char *uy, const char *uxx)
{
    Buf b;
    buf_init(&b, buf, cap);
    if (strcmp(uy, uxx) == 0) return buf;
    if (strcmp(uy, "A") == 0 && strcmp(uxx, "V") == 0) buf_str(&b, " S");
    else if (strcmp(uy, "V") == 0 && strcmp(uxx, "A") == 0) buf_str(&b, " " S_OHM);
    else { buf_ch(&b, ' '); buf_str(&b, uy); if (uxx[0]) { buf_ch(&b, '/'); buf_str(&b, uxx); } }
    return buf;
}

/* La derivada exacta del canal activo en el cursor: «ganancia dy/dx = -9,999»,
 * «g = dI/dV = 38,21 mS   r = 26,17 Ω», «r = dV/dI = …», «dy/dx = …» */
static void txt_derivada(char *t, size_t cap, const Curva *z, const BarrReg *g, const BarrRes *r)
{
    char u[24], w[24], e[12];
    double x = z->xc[z->cur], y, d, y1, d1;
    int tr = z->activo;
    const char *uy = uni_tr(r, g, tr), *uxx = uni_x(r);
    Buf b;
    buf_init(&b, t, cap);
    if (z->rafaga) { buf_str(&b, S_APROX " de la malla: la derivada exacta, al soltar."); return; }
    if (!barr_en(tr, x, &y, &d)) { buf_str(&b, "Aqu" S_ii " Newton no converge: sin lectura."); return; }
    if (r->modo == BARR_FAMILIA && r->fam_bjt) {           /* «β = IC/IB = 100,0   r0 = dVCE/dIC = 38,46 kΩ» */
        double ib = tr >= 0 && tr < BARR_TRAZAS ? r->fam[tr] : 0.0;
        if (ib > 0.0) { buf_str(&b, S_BETA " = IC/IB = "); buf_str(&b, sig4(u, sizeof u, y / ib)); }
        else buf_str(&b, "IB = 0: en corte");
        buf_str(&b, "   r0 = ");
        /* sin Early, infinita en activa (y en corte); saturado, la curva sube
         * con su pendiente exacta */
        if ((!r->fam_early && barr_region() != REG_SATURACION) || !(fabs(d) > 0.0)) buf_str(&b, S_INF " (sin Early)");
        else { buf_str(&b, "dVCE/dIC = "); buf_str(&b, fija(w, sizeof w, 1.0 / d, 4, " " S_OHM)); }
        return;
    }
    if (r->modo == BARR_FAMILIA) {                         /* «ID/vov² = 1,000 mA/V²   r0 = dVDS/dID = 50,00 kΩ» */
        double vov = tr >= 0 && tr < BARR_TRAZAS ? r->fam[tr] - r->fam_vt : 0.0;
        if (vov > 0.0) { buf_str(&b, "ID/vov" S_SUP2 " = "); buf_str(&b, fija(u, sizeof u, y / (vov * vov), 4, " A/V" S_SUP2)); }
        else buf_str(&b, "vov = 0: en corte");
        buf_str(&b, "   r0 = ");
        /* sin VA, infinita en saturación (y en corte); en la óhmica, la
         * pendiente exacta */
        if ((!r->fam_early && barr_region() != REG_OHMICA) || !(fabs(d) > 0.0)) buf_str(&b, S_INF " (sin VA)");
        else { buf_str(&b, "dVDS/dID = "); buf_str(&b, fija(w, sizeof w, 1.0 / d, 4, " " S_OHM)); }
        return;
    }
    if (z->xy) {                                           /* dy/dx = (dy/dp)/(dx/dp) */
        int t1 = pareja(g, tr, 0);
        if (t1 < 0 || !barr_en(t1, x, &y1, &d1)) { buf_str(&b, "Aqu" S_ii " Newton no converge: sin lectura."); return; }
        uxx = uni_tr(r, g, t1);
        d = d1 != 0.0 ? d / d1 : (d != 0.0 ? HUGE_VAL : 0.0);
    }
    if (strcmp(uy, "A") == 0 && strcmp(uxx, "V") == 0) {
        buf_str(&b, "g = dI/dV = ");
        buf_str(&b, fabs(d) < 1e300 ? fija(u, sizeof u, d, 4, " S") : S_INF);
        buf_str(&b, "   r = ");
        buf_str(&b, d == 0.0 ? S_INF : fabs(d) < 1e300 ? fija(w, sizeof w, 1.0 / d, 4, " " S_OHM) : "0 " S_OHM);
    } else if (strcmp(uy, "V") == 0 && strcmp(uxx, "A") == 0) {
        buf_str(&b, "r = dV/dI = ");
        buf_str(&b, fabs(d) < 1e300 ? fija(u, sizeof u, d, 4, " " S_OHM) : S_INF);
        buf_str(&b, "   g = ");
        buf_str(&b, d == 0.0 ? S_INF : fabs(d) < 1e300 ? fija(w, sizeof w, 1.0 / d, 4, " S") : "0 S");
    } else if (strcmp(uy, uxx) == 0) {
        buf_str(&b, "ganancia dy/dx = "); buf_str(&b, fabs(d) < 1e300 ? sig4(u, sizeof u, d) : S_INF);
    } else {
        buf_str(&b, "dy/dx = ");
        buf_str(&b, fabs(d) < 1e300 ? fija(u, sizeof u, d, 4, uni_cociente(e, sizeof e, uy, uxx)) : S_INF);
    }
}

/* «Δ I(D1) = 5,210 mA   Δy/Δx = 5,210 mS» */
static void txt_delta(char *t, size_t cap, const Curva *z, const BarrReg *g, const BarrRes *r)
{
    char u[24], e[12];
    double y[2], x[2];
    int k, tr = z->activo, t1 = z->xy ? pareja(g, tr, 0) : -1;
    const char *uy = uni_tr(r, g, tr), *uxx = z->xy && t1 >= 0 ? uni_tr(r, g, t1) : uni_x(r);
    Buf b;
    buf_init(&b, t, cap);
    for (k = 0; k < 2; k++) {
        if (z->rafaga) {                                   /* de la malla mientras corre una ráfaga */
            y[k] = cv_valor(g, tr, z->xc[k]);
            x[k] = z->xy ? (t1 >= 0 ? cv_valor(g, t1, z->xc[k]) : NAN) : z->xc[k];
            if (!(y[k] == y[k] && x[k] == x[k])) { buf_str(&b, S_APROX ": un hueco de la malla (al soltar, la exacta)."); return; }
            continue;
        }
        if (!barr_en(tr, z->xc[k], &y[k], 0)) { buf_str(&b, "Aqu" S_ii " Newton no converge: sin lectura."); return; }
        x[k] = z->xc[k];
        if (z->xy && (t1 < 0 || !barr_en(t1, z->xc[k], &x[k], 0))) { buf_str(&b, "Aqu" S_ii " Newton no converge: sin lectura."); return; }
    }
    buf_str(&b, S_DELTA " "); buf_str(&b, nombre_tr(r, g, tr)); buf_str(&b, z->rafaga ? " " S_APROX " " : " = ");
    buf_str(&b, fija(u, sizeof u, y[1] - y[0], 4, con_esp(e, sizeof e, uy)));
    if (x[1] != x[0]) {
        double q = (y[1] - y[0]) / (x[1] - x[0]);
        buf_str(&b, "   " S_DELTA "y/" S_DELTA "x = ");
        if (strcmp(uy, uxx) == 0) buf_str(&b, sig4(u, sizeof u, q));
        else buf_str(&b, fija(u, sizeof u, q, 4, uni_cociente(e, sizeof e, uy, uxx)));
    }
    if (strlen(t) > COLS) {                                /* sin el nombre */
        buf_init(&b, t, cap);
        buf_str(&b, S_DELTA "y = "); buf_str(&b, fija(u, sizeof u, y[1] - y[0], 4, con_esp(e, sizeof e, uy)));
    }
}

/* «C1 2 mA/div   C2 200 mV/div   x 500 mV/div» */
static void txt_escalas(char *t, size_t cap, const Curva *z, const BarrReg *g, const BarrRes *r)
{
    char u[24], e[16];
    int sep, ch;
    Buf b;
    for (sep = 3; sep >= 1; sep -= 2) {
        const char *s = sep == 3 ? "   " : " ";
        int primero = 1;
        buf_init(&b, t, cap);
        for (ch = 0; ch < 2; ch++) {
            int t0 = -1, q;
            for (q = 0; q < g->ntr; q++) if (g->canal[q] == ch && t0 < 0) t0 = q;
            if (t0 < 0) continue;
            if (!primero) buf_str(&b, s);
            primero = 0;
            buf_ch(&b, 'C'); buf_ch(&b, (char)('1' + ch)); buf_ch(&b, ' ');
            if (z->log_y[t0]) buf_str(&b, "1 d" S_ee "c/div");
            else {
                Buf q2;
                buf_init(&q2, e, sizeof e);
                buf_ch(&q2, ' '); buf_str(&q2, uni_tr(r, g, t0)); buf_str(&q2, "/div");
                buf_str(&b, num_pref(u, sizeof u, vdiv(z, t0), 3, e));
            }
            if (z->xy && ch == 0) buf_str(&b, " (x)");
        }
        if (!z->xy) {
            double w = (z->x1 - z->x0) / 10.0;
            buf_str(&b, s); buf_str(&b, "x ");
            if (z->log_x) {                                /* dos cifras: «0,3», «0,0043» */
                int dec = w > 0.0 ? 1 - (int)floor(log10(w)) : 2;
                if (dec < 2) dec = 2;
                if (dec > 6) dec = 6;
                buf_str(&b, num_auto(u, sizeof u, w, dec)); buf_str(&b, " d" S_ee "c/div");
            }
            else if (uni_x(r)[0]) {
                Buf q2;
                buf_init(&q2, e, sizeof e);
                buf_ch(&q2, ' '); buf_str(&q2, uni_x(r)); buf_str(&q2, "/div");
                buf_str(&b, num_pref(u, sizeof u, w, 3, e));
            } else { buf_str(&b, num_pref(u, sizeof u, w, 3, "")); buf_str(&b, "/div"); }
        }
        if (strlen(t) <= COLS) return;
    }
}

static void panel(const Curva *z, const BarrReg *g, const BarrRes *r)
{
    char t[128];
    int y = CV_PANEL_Y;
    gfx_fill(0, CV_PANEL_Y - 2, SCR_W, 226 - (CV_PANEL_Y - 2), C_PANEL);
    if (hay_malla(g)) {
        /* 1: lo que se escribe, la lectura o los dos x */
        if (escribiendo(z)) { txt_entrada(t, sizeof t, z, g, r); gfx_text(4, y, t, C_CURSOR, 1); }
        else {
            if (z->dos) txt_dos_x(t, sizeof t, z, r);
            else txt_lectura(t, sizeof t, z, g, r);
            gfx_text(4, y, t, C_TEXTO, 1);
        }
        /* 2: la derivada exacta, o la diferencia entre los cursores */
        if (z->dos) txt_delta(t, sizeof t, z, g, r);
        else txt_derivada(t, sizeof t, z, g, r);
        gfx_text(4, y + 13, t, C_TEXTO, 1);
        /* 3: las escalas */
        txt_escalas(t, sizeof t, z, g, r);
        gfx_text(4, y + 26, t, C_SUAVE, 1);
    }
    /* 4: el aviso, o las teclas */
    if (z->aviso[0]) gfx_text(4, y + 39, z->aviso, z->aviso_col, 1);
    else gfx_text(4, y + 39, "[C] canal [N] nivel [X] 2 cursores [Y] X-Y [L] log", C_SUAVE, 1);
}

static void pie(const char *t)
{
    gfx_fill(0, 226, SCR_W, 14, C_PANEL);
    gfx_text(4, 227, t, C_SUAVE, 1);
}

void cv_dibuja(Curva *z, const BarrReg *g, const BarrRes *r)
{
    gfx_clear(C_FONDO);
    if (r->fin == FIN_ERROR) {                             /* el primer punto no sale: el porqué */
        char t[260];
        gfx_fill(0, 0, SCR_W, 15, C_PANEL);
        gfx_text(4, 3, "Barrido de CC", C_TEXTO, 1);
        sim_mensaje_circ(barr_circuito(), 0, &r->d, t, sizeof t);
        parrafo(4, 24, t, C_ERROR, 6);
        pie("[esc] volver");
        return;
    }
    cabecera(z, g, r);
    if (hay_malla(g)) {
        reticula(z, g);
        rotulos_y(z, g);
        trazas(z, g, r);
        marcas(z, g, r);
        cursores(z, g);
        eje_x(z, g);
    } else gfx_text(CV_X0 + 20, CV_Y0 + 60, "Sin puntos: [=] barre de nuevo.", C_SUAVE, 1);
    panel(z, g, r);
    if (escribiendo(z)) pie("[enter] ir ah" S_ii "  [del] borrar  [esc] dejarlo");
    else if (r->modo == BARR_CIRCUITO) pie("[izq/der] cursor [N] nivel [+-] zoom [tab] circuito");
    else pie("[izq/der] cursor [N] nivel [+-] zoom [esc] volver");
}

/* ==========================================================================
 *  4. Teclas
 * ======================================================================= */
static void escribe(Curva *z, char c)
{
    size_t n = strlen(z->entrada);
    if (n + 1 < sizeof z->entrada) { z->entrada[n] = c; z->entrada[n + 1] = 0; }
}

/* [enter] escribiendo: un x lleva el cursor ahí; un nivel busca su cruce */
static void acepta(Curva *z, const BarrReg *g, const BarrRes *r)
{
    int ok, nivel = z->de_nivel, vacio = !z->entrada[0];
    double v = num_lee_ing(z->entrada, &ok), tol;
    z->entrada[0] = 0;
    z->de_nivel = 0;
    if (vacio) return;
    if (!ok) { avisa(z, nivel ? "Eso no es un nivel." : "Eso no es un valor.", C_ERROR); return; }
    if (nivel) { cv_ir_nivel(z, g, r, z->activo, v, 0); return; }
    tol = TOL_REL * (g->x[g->n - 1] - g->x[0]);
    if (v < g->x[0] - tol || v > g->x[g->n - 1] + tol || (z->log_x && !(v > 0.0))) {
        avisa(z, "Fuera del barrido: [menu] cambia el rango.", C_ERROR);
        return;
    }
    if (v < g->x[0]) v = g->x[0];
    if (v > g->x[g->n - 1]) v = g->x[g->n - 1];
    lleva(z, g, v, 1);
}

static void tecla_entrada(Curva *z, const BarrReg *g, const BarrRes *r, int t, int sh)
{
    size_t n = strlen(z->entrada);
    char ult = n ? z->entrada[n - 1] : 0;
    if (t >= T_DIG(0) && t <= T_DIG(9)) { escribe(z, (char)('0' + (t - T_DIG0))); return; }
    switch (t) {
    case T_PUNTO: escribe(z, '.'); return;
    case T_EE: escribe(z, 'E'); return;
    case T_SIGNO: case T_MENOS: if (n == 0 || ult == 'E') escribe(z, '-'); return;
    case T_BORRAR: if (n) z->entrada[n - 1] = 0; else z->de_nivel = 0; return;
    case T_ESC: z->entrada[0] = 0; z->de_nivel = 0; return;
    case T_ENTER: acepta(z, g, r); return;
    default: break;
    }
    /* los prefijos; lo demás no hace nada mientras se escribe */
    if (t == T_LETRA('P')) escribe(z, 'p');
    else if (t == T_LETRA('N')) escribe(z, 'n');
    else if (t == T_LETRA('U')) escribe(z, 'u');
    else if (t == T_LETRA('M')) escribe(z, sh ? 'M' : 'm');
    else if (t == T_LETRA('K')) escribe(z, 'k');
    else if (t == T_LETRA('G')) escribe(z, 'G');
}

/* El punto de la malla siguiente (sg = +1) o anterior (-1) a x; -1 si no hay */
static int vecino(const Curva *z, const BarrReg *g, double x, int sg)
{
    double u = ux(z, x), tol = TOL_REL * (lim1(z, g) - lim0(z, g));
    int k;
    if (sg > 0) { for (k = 0; k < g->n; k++) if (ux(z, g->x[k]) > u + tol) return k; }
    else { for (k = g->n - 1; k >= 0; k--) if (ux(z, g->x[k]) < u - tol) return k; }
    return -1;
}

/* Otra traza activa: el nivel de [N] es de su canal (otra unidad): si cambia
 * de canal, se quita */
static void cambia_activo(Curva *z, const BarrReg *g, int q)
{
    if (q < 0 || q >= g->ntr) return;
    if (g->canal[q] != g->canal[z->activo]) z->nivel_on = 0;
    z->activo = (unsigned char)q;
}

/* El valor de la traza en x que maximiza la sección áurea (sg = -1: el mínimo) */
static double f_oro(int tr, double x, double sg)
{
    double y;
    return barr_en(tr, x, &y, 0) ? sg * y : -HUGE_VAL;
}

/* [P]: el máximo de la traza activa (sección áurea sobre barr_en entre los
 * vecinos del mayor punto de la malla); [shift][P]: el mínimo */
static void extremo(Curva *z, const BarrReg *g, const BarrRes *r, int maximo)
{
    char t[112], u[24], e[8];
    double sg = maximo ? 1.0 : -1.0, xm, ym, a, b;
    int tr = z->activo, k, kb = -1, intento;
    Buf q;
    for (k = 0; k < g->n; k++)
        if (g->ok[tr][k] && (kb < 0 || sg * g->y[tr][k] > sg * g->y[tr][kb])) kb = k;
    if (kb < 0) return;
    xm = g->x[kb]; ym = g->y[tr][kb];
    a = g->x[kb > 0 && g->ok[tr][kb - 1] ? kb - 1 : kb];
    b = g->x[kb < g->n - 1 && g->ok[tr][kb + 1] ? kb + 1 : kb];
    if (b > a) {
        double c = b - ORO * (b - a), d = a + ORO * (b - a), fc = f_oro(tr, c, sg), fd = f_oro(tr, d, sg);
        int it;
        for (it = 0; it < IT_ORO; it++) {
            if (fc > fd) { b = d; d = c; fd = fc; c = b - ORO * (b - a); fc = f_oro(tr, c, sg); }
            else { a = c; c = d; fc = fd; d = a + ORO * (b - a); fd = f_oro(tr, d, sg); }
        }
        if (fc > fd) { d = c; fd = fc; }
        if (fd > sg * ym) { xm = d; ym = sg * fd; }        /* si no, el punto de la malla (un borde) */
    }
    lleva(z, g, xm, 1);
    for (intento = 0; intento < 3; intento++) {
        buf_init(&q, t, sizeof t);
        buf_str(&q, intento == 0 ? (maximo ? "M" S_aa "ximo: " : "M" S_ii "nimo: ") : (maximo ? "M" S_aa "x. " : "M" S_ii "n. "));
        if (intento < 2) { buf_str(&q, nombre_tr(r, g, tr)); buf_ch(&q, ' '); }
        buf_str(&q, "= "); buf_str(&q, fija(u, sizeof u, ym, 4, con_esp(e, sizeof e, uni_tr(r, g, tr))));
        buf_str(&q, " en "); buf_str(&q, r->nom_x); buf_str(&q, " = ");
        buf_str(&q, fija(u, sizeof u, xm, 4, con_esp(e, sizeof e, uni_x(r))));
        if (strlen(t) <= COLS) break;
    }
    avisa(z, t, C_CURSOR);
}

int cv_tecla(Curva *z, const BarrReg *g, const BarrRes *r, int tk)
{
    int sh = (tk & T_CON_SHIFT) != 0, t = tk & ~T_MODS;
    int circ = r->modo == BARR_CIRCUITO;

    z->aviso[0] = 0;
    if (r->fin == FIN_ERROR) {                             /* solo volver (o cambiar lo barrido) */
        if (t == T_ESC) return CV_SALIR;
        if (t == T_MENU && circ) return CV_AJUSTES;
        return CV_NADA;
    }
    /* 1. escribiendo un x o un nivel */
    if (escribiendo(z)) { tecla_entrada(z, g, r, t, sh); return CV_NADA; }
    if (hay_malla(g)) {
        int tr = z->activo, ch = g->canal[tr];
        /* 2. el cursor: un punto de la malla; con shift, una división */
        if (t == T_IZQ || t == T_DER) {
            int sg = t == T_DER ? 1 : -1, k = vecino(z, g, z->xc[z->cur], sg);
            if (tk & T_CON_CTRL) {
                /* [ctrl]: al borde de la ventana (su último punto de la malla);
                 * si ya está ahí, al extremo del barrido */
                double u = ux(z, z->xc[z->cur]), b = sg > 0 ? z->x1 : z->x0, tol = TOL_REL * (lim1(z, g) - lim0(z, g));
                if (sg > 0) { for (k = g->n - 1; k > 0 && ux(z, g->x[k]) > b + tol; k--) {} }
                else { for (k = 0; k < g->n - 1 && ux(z, g->x[k]) < b - tol; k++) {} }
                if (fabs(ux(z, g->x[k]) - u) <= tol) k = sg > 0 ? g->n - 1 : 0;
                lleva(z, g, g->x[k], 0);
                return CV_NADA;
            }
            if (k < 0) return CV_NADA;                     /* en el borde */
            if (sh) {
                int paso = (g->n - 1) / 10;
                k += sg * (paso > 1 ? paso - 1 : 0);
                if (k < 0) k = 0;
                if (k > g->n - 1) k = g->n - 1;
            }
            lleva(z, g, g->x[k], 0);
            return CV_NADA;
        }
        /* 3. la escala del canal activo; con shift, su posición (en log, la década) */
        if (t == T_ARRIBA || t == T_ABAJO) {
            if (z->log_y[tr]) {
                int p = z->pos[tr] + (t == T_ARRIBA ? -1 : 1);
                if (p >= LOG_ARRIBA_MIN && p <= LOG_ARRIBA_MAX) pon_escala(z, g, ch, z->idiv[tr], p);
            } else if (sh) {
                int p = z->pos[tr] + (t == T_ARRIBA ? 1 : -1);
                if (p >= -POS_Y && p <= POS_Y) pon_escala(z, g, ch, z->idiv[tr], p);
            } else {
                int i = z->idiv[tr] + (t == T_ARRIBA ? -1 : 1);
                if (i >= 0 && i < N_ESCALA) pon_escala(z, g, ch, i, z->pos[tr]);
            }
            return CV_NADA;
        }
        /* 4. la ventana a la mitad o al doble alrededor del cursor */
        if (t == T_MAS || t == T_MENOS) {
            double A = lim0(z, g), B = lim1(z, g), w = z->x1 - z->x0;
            double wmin = 4.0 * (B - A) / (double)(g->n - 1), c = ux(z, z->xc[z->cur]);
            double nw = t == T_MAS ? 0.5 * w : 2.0 * w;
            if (!(c >= A && c <= B)) c = 0.5 * (z->x0 + z->x1);
            if (nw < wmin) nw = wmin;
            if (nw >= (B - A) * (1.0 - 1e-12)) inicio(z, g);
            else { z->x0 = c - 0.5 * nw; z->x1 = c + 0.5 * nw; encaja(z, g); }
            if (n_en_vista(z, g) < MIN_VISTA) avisa(z, "Pocas muestras aqu" S_ii ": [=] afina lo que se ve.", C_CURSOR);
            return CV_NADA;
        }
        /* 5. el canal (o la traza) activo, verlo o no, autoescala */
        if (t == T_LETRA('C')) {
            int k;
            for (k = 1; k < g->ntr; k++) {
                int q = (tr + k) % g->ntr;
                if (z->xy && g->canal[q] != 1) continue;
                cambia_activo(z, g, q);
                return CV_NADA;
            }
            avisa(z, circ ? "No hay otra traza: pon otra sonda con [O]." : "No hay otra traza.", C_CURSOR);
            return CV_NADA;
        }
        if (t == T_BORRAR) { z->ver[tr] = (unsigned char)!z->ver[tr]; return CV_NADA; }
        if (t == T_LETRA('A')) { cv_autoescala(z, g); return CV_NADA; }
        /* 6. un nivel ([shift]: el cruce siguiente del mismo); una cifra empieza un x */
        if (t == T_LETRA('N')) {
            if (!sh) { z->de_nivel = 1; z->entrada[0] = 0; return CV_NADA; }
            if (!z->nivel_on) { avisa(z, "[N] escribe un nivel primero.", C_CURSOR); return CV_NADA; }
            cv_ir_nivel(z, g, r, tr, z->nivel, 0);
            return CV_NADA;
        }
        if ((t >= T_DIG(0) && t <= T_DIG(9)) || t == T_PUNTO || t == T_SIGNO) {
            z->de_nivel = 0;
            escribe(z, t == T_PUNTO ? '.' : t == T_SIGNO ? '-' : (char)('0' + (t - T_DIG0)));
            return CV_NADA;
        }
        /* 7. el segundo cursor y cuál se mueve */
        if (t == T_LETRA('X')) {
            if (z->dos) { z->dos = 0; z->cur = 0; return CV_NADA; }
            if (!(ux(z, z->xc[1]) >= z->x0 && ux(z, z->xc[1]) <= z->x1) || z->xc[1] == z->xc[0]) {
                double u0 = ux(z, z->xc[0]), w = z->x1 - z->x0, u1 = u0 + 0.25 * w;
                if (u1 > z->x1) u1 = u0 - 0.25 * w;
                if (u1 < z->x0) u1 = z->x0;
                z->xc[1] = de_ux(z, u1);
                if (z->xc[1] < g->x[0]) z->xc[1] = g->x[0];
                if (z->xc[1] > g->x[g->n - 1]) z->xc[1] = g->x[g->n - 1];
            }
            z->dos = 1; z->cur = 1;
            return CV_NADA;
        }
        if (t == T_ENTER) {
            if (z->dos) z->cur = (unsigned char)(1 - z->cur);
            else avisa(z, "[X] enciende el segundo cursor.", C_SUAVE);
            return CV_NADA;
        }
        /* 8. el máximo ([shift]: el mínimo) */
        if (t == T_LETRA('P')) { extremo(z, g, r, !sh); return CV_NADA; }
        /* 9. X-Y (canal 2 frente al 1) o y(x) */
        if (t == T_LETRA('Y')) {
            if (z->xy) z->xy = 0;
            else if (!(hay_canal_c(g, 0) && hay_canal_c(g, 1)) || r->modo == BARR_FAMILIA) {
                avisa(z, "X-Y pide dos canales: pon otra sonda con [O].", C_CURSOR);
                return CV_NADA;
            } else {
                z->xy = 1;
                if (g->canal[tr] != 1) cambia_activo(z, g, pareja(g, tr, 1));
                pon_log(z, g, 0, 0);
            }
            cv_autoescala(z, g);
            return CV_NADA;
        }
        /* 10. |y| del canal activo en escala logarítmica */
        if (t == T_LETRA('L')) {
            pon_log(z, g, ch, !z->log_y[tr]);
            autoescala_canal(z, g, ch);
            return CV_NADA;
        }
    }
    /* 11. lo que hace ui.c (la curva de un componente no tiene circuito ni ajustes) */
    if (t == T_IGUAL || t == T_TAB || t == T_MENU) {
        if (!circ) { avisa(z, "La curva de un componente no tiene circuito.", C_CURSOR); return CV_NADA; }
        return t == T_IGUAL ? CV_BARRER : t == T_TAB ? CV_CIRCUITO : CV_AJUSTES;
    }
    if (t == T_ESC) return CV_SALIR;
    return CV_NADA;
}
