/* ============================================================================
 *  osc.c  -  El osciloscopio  (ver osc.h)
 * ========================================================================= */
#include <string.h>
#include <math.h>
#include "osc.h"
#include "numfmt.h"
#include "ui.h"

const double ESCALA[N_ESCALA] = {
    1e-9, 2e-9, 5e-9, 1e-8, 2e-8, 5e-8, 1e-7, 2e-7, 5e-7,
    1e-6, 2e-6, 5e-6, 1e-5, 2e-5, 5e-5, 1e-4, 2e-4, 5e-4,
    1e-3, 2e-3, 5e-3, 1e-2, 2e-2, 5e-2, 1e-1, 2e-1, 5e-1,
    1.0, 2.0, 5.0, 1e1, 2e1, 5e1, 1e2, 2e2, 5e2,
    1e3, 2e3, 5e3, 1e4, 2e4, 5e4, 1e5, 2e5, 5e5,
    1e6, 2e6, 5e6
};

#define IT_MIN 0                    /* 1 ns/div  */
#define IT_MAX 30                   /* 10 s/div  */

/* ==========================================================================
 *  Funciones puras
 * ======================================================================= */
/* Primera muestra con t[j] > t */
static int primera_mayor(const SimReg *r, double t)
{
    int lo = 0, hi = r->n;
    while (lo < hi) { int m = (lo + hi) / 2; if (r->t[m] > t) hi = m; else lo = m + 1; }
    return lo;
}

/* El valor en t con j = primera_mayor(r, t) ya buscado */
static double valor_en(const SimReg *r, const double *y, int j, double t)
{
    if (j <= 0) return y[0];
    if (j >= r->n) return y[r->n - 1];
    if (r->t[j - 1] == t) return y[j - 1];       /* en un evento: el de después */
    return y[j - 1] + (y[j] - y[j - 1]) * (t - r->t[j - 1]) / (r->t[j] - r->t[j - 1]);
}

double osc_valor(const SimReg *r, int k, double t)
{
    if (r->n <= 0) return 0.0;
    return valor_en(r, r->y[k ? 1 : 0], primera_mayor(r, t), t);
}

int osc_columnas(const SimReg *r, int k, double tizq, double dcol, int ncol,
                 double *mn, double *mx, unsigned char *ok)
{
    const double *t = r->t, *y = r->y[k ? 1 : 0];
    int c, j, j1, nok = 0, hay_b = 0;
    double tult, tb = 0.0, vb = 0.0;
    if (r->n <= 0) { memset(ok, 0, (size_t)ncol); return 0; }
    tult = t[r->n - 1];
    j = primera_mayor(r, tizq);
    for (c = 0; c < ncol; c++) {
        double t0 = tizq + c * dcol, t1 = tizq + (c + 1) * dcol, ta, a, b, lo, hi;
        if (t0 >= tult || t1 <= t[0]) { ok[c] = 0; hay_b = 0; continue; }
        if (t1 > tult) t1 = tult;
        /* el valor del borde izquierdo es el del derecho de la columna de
         * antes (el mismo t: los mismos bits); el del derecho sale del
         * índice que ya avanza (j1: la primera muestra pasado t1, la que
         * buscaría osc_valor) */
        ta = t0 > t[0] ? t0 : t[0];
        a = hay_b && tb == ta ? vb : osc_valor(r, k, ta);
        while (j < r->n && t[j] <= t0) j++;
        for (j1 = j; j1 < r->n && t[j1] <= t1; j1++) {}
        b = valor_en(r, y, j1, t1);
        lo = a < b ? a : b; hi = a < b ? b : a;
        /* las muestras de dentro, incluidas las de t1 (la de antes de un
         * evento en el borde queda en esta columna; la de después empieza
         * la siguiente) */
        for (; j < j1; j++) {
            if (y[j] < lo) lo = y[j];
            if (y[j] > hi) hi = y[j];
        }
        mn[c] = lo; mx[c] = hi; ok[c] = 1;
        nok++;
        hay_b = 1; tb = t1; vb = b;
    }
    return nok;
}

/* ∫y y ∫y² desde el principio del registro hasta t */
static void integral(const SimReg *r, int k, double t, double *s1, double *s2)
{
    int j = primera_mayor(r, t), i;
    *s1 = *s2 = 0.0;
    if (j <= 0) return;
    i = j - 1;
    *s1 = r->s1[k][i]; *s2 = r->s2[k][i];
    if (j < r->n && t > r->t[i]) {
        double y0 = r->y[k][i], dt = t - r->t[i];
        double yt = y0 + (r->y[k][j] - y0) * dt / (r->t[j] - r->t[i]);
        *s1 += 0.5 * (y0 + yt) * dt;
        *s2 += 0.5 * (y0 * y0 + yt * yt) * dt;
    }
}

void osc_medidas(const SimReg *r, int k, double ta, double tb, Medidas *m)
{
    const double *t = r->t, *y;
    double ya, yb, a1, a2, b1, b2, nivel, hist, tp, yp, t_pri = 0.0, t_ult = 0.0;
    int j, ncr = 0, armado;

    memset(m, 0, sizeof *m);
    k = k ? 1 : 0;
    y = r->y[k];
    if (r->n < 2) return;
    if (ta < t[0]) ta = t[0];
    if (tb > t[r->n - 1]) tb = t[r->n - 1];
    if (!(tb > ta)) return;
    ya = osc_valor(r, k, ta); yb = osc_valor(r, k, tb);
    m->max = ya > yb ? ya : yb; m->min = ya < yb ? ya : yb;
    for (j = primera_mayor(r, ta); j < r->n && t[j] <= tb; j++) {
        if (y[j] > m->max) m->max = y[j];
        if (y[j] < m->min) m->min = y[j];
    }
    m->pp = m->max - m->min;
    integral(r, k, ta, &a1, &a2);
    integral(r, k, tb, &b1, &b2);
    m->media = (b1 - a1) / (tb - ta);
    m->eficaz = (b2 - a2) / (tb - ta);
    m->eficaz = sqrt(m->eficaz > 0.0 ? m->eficaz : 0.0);
    m->hay = 1;

    /* Frecuencia: cruces de subida por el nivel medio, con histéresis del 5 % */
    nivel = 0.5 * (m->max + m->min);
    hist = 0.05 * m->pp;
    if (!(m->pp > 0.0)) return;
    armado = ya <= nivel - hist;
    tp = ta; yp = ya;
    for (j = primera_mayor(r, ta); j <= r->n; j++) {
        double tt, yy;
        if (j < r->n && t[j] < tb) { tt = t[j]; yy = y[j]; }
        else { tt = tb; yy = yb; }
        if (armado && yp < nivel && yy >= nivel) {
            double tc = tt > tp ? tp + (nivel - yp) * (tt - tp) / (yy - yp) : tt;
            if (ncr == 0) t_pri = tc;
            t_ult = tc;
            ncr++;
            armado = 0;
        }
        if (yy <= nivel - hist) armado = 1;
        tp = tt; yp = yy;
        if (tt >= tb) break;
    }
    if (ncr >= 2 && t_ult > t_pri) { m->frec = (double)(ncr - 1) / (t_ult - t_pri); m->hay_frec = 1; }
}

int osc_pico(const SimReg *r, int k, double desde, int maximo, double *tp, double *vp)
{
    const double *y;
    double prom, lo, hi = 0.0, sg = maximo ? 1.0 : -1.0;
    int j, j0, hij = -1;
    k = k ? 1 : 0;
    y = r->y[k];
    if (r->n < 3) return 0;
    prom = 0.01 * (r->ymax[k] - r->ymin[k]);
    if (!(prom > 0.0)) return 0;
    j0 = primera_mayor(r, desde);
    if (j0 >= r->n) return 0;
    lo = sg * y[j0];
    for (j = j0; j < r->n; j++) {
        double x = sg * y[j];
        if (hij < 0) {                        /* sube lo bastante desde un valle */
            if (x < lo) lo = x;
            if (x - lo >= prom) { hij = j; hi = x; }
        } else if (x > hi) { hi = x; hij = j; }
        else if (hi - x >= prom) break;      /* y baja lo bastante: es un pico */
    }
    if (hij < 0 || j >= r->n) return 0;
    *tp = r->t[hij]; *vp = y[hij];
    /* vértice de la parábola por tres muestras equiespaciadas */
    if (hij > 0 && hij + 1 < r->n) {
        double d0 = r->t[hij] - r->t[hij - 1], d1 = r->t[hij + 1] - r->t[hij];
        double c2 = y[hij - 1] - 2.0 * y[hij] + y[hij + 1];
        if (d0 > 0.0 && fabs(d1 - d0) <= 1e-6 * d0 && c2 != 0.0) {
            double off = 0.5 * (y[hij - 1] - y[hij + 1]) / c2;
            if (off > -1.0 && off < 1.0) {
                *tp = r->t[hij] + off * d0;
                *vp = y[hij] - 0.25 * (y[hij - 1] - y[hij + 1]) * off;
            }
        }
    }
    return 1;
}

/* ¿Hay un cruce de subida (en el sentido de la señal) entre las muestras j-1
 * y j? En t, el instante: primero por la recta y luego, si hay cuatro
 * muestras equiespaciadas alrededor, por la cúbica que pasa por ellas (un
 * cruce suele caer en un punto de inflexión, donde la recta tiene un error
 * de tercer orden) */
static int cruce(const SimReg *r, const double *y, int j, double nivel, double sg, double *t)
{
    double a = sg * y[j - 1], b = sg * y[j], nv = sg * nivel, d;
    int i, q;
    if (!(a < nv && b >= nv)) return 0;
    d = r->t[j] - r->t[j - 1];
    if (!(d > 0.0)) { *t = r->t[j]; return 1; }
    *t = r->t[j - 1] + (nv - a) * d / (b - a);
    if (j < 2 || j + 1 >= r->n) return 1;
    for (i = j - 1; i <= j + 1; i++)
        if (fabs((r->t[i] - r->t[i - 1]) - d) > 1e-6 * d) return 1;
    {
        /* cúbica de Lagrange por j-2, j-1, j, j+1 en u = (t - t[j-1])/d; Newton */
        double yy[4], u = (*t - r->t[j - 1]) / d;
        for (q = 0; q < 4; q++) yy[q] = sg * y[j - 2 + q] - nv;
        for (q = 0; q < 8; q++) {
            double x0 = u + 1.0, x1 = u, x2 = u - 1.0, x3 = u - 2.0;
            double f = -yy[0] * x1 * x2 * x3 / 6.0 + yy[1] * x0 * x2 * x3 / 2.0
                       - yy[2] * x0 * x1 * x3 / 2.0 + yy[3] * x0 * x1 * x2 / 6.0;
            double df = -yy[0] * (x2 * x3 + x1 * x3 + x1 * x2) / 6.0 + yy[1] * (x2 * x3 + x0 * x3 + x0 * x2) / 2.0
                        - yy[2] * (x1 * x3 + x0 * x3 + x0 * x1) / 2.0 + yy[3] * (x1 * x2 + x0 * x2 + x0 * x1) / 6.0;
            if (!(df > 0.0)) return 1;              /* se queda con la recta */
            u -= f / df;
            if (u < 0.0 || u > 1.0) return 1;
        }
        *t = r->t[j - 1] + u * d;
    }
    return 1;
}

int osc_evento(const SimReg *r, int k, double nivel, int subida, double desde, int sentido, double *t)
{
    const double *y;
    double hist, sg = subida ? 1.0 : -1.0, nv;
    int j, i;
    k = k ? 1 : 0;
    y = r->y[k];
    if (r->n < 2) return 0;
    hist = 0.01 * (r->ymax[k] - r->ymin[k]);
    nv = sg * nivel;
    if (sentido >= 0) {
        int armado = 0;
        j = primera_mayor(r, desde);
        if (j < 1) j = 1;
        if (sg * y[j - 1] <= nv - hist) armado = 1;
        for (; j < r->n; j++) {
            if (armado && cruce(r, y, j, nivel, sg, t) && *t > desde) return 1;
            if (sg * y[j] <= nv - hist) armado = 1;
            else if (sg * y[j] >= nv) armado = 0;
        }
        return 0;
    }
    /* hacia atrás: un cruce antes de `desde`, con la señal por debajo del
     * nivel menos la histéresis antes de él */
    for (j = primera_mayor(r, desde) - 1; j >= 1; j--) {
        if (!cruce(r, y, j, nivel, sg, t) || !(*t < desde)) continue;
        for (i = j - 1; i >= 0; i--) {
            if (sg * y[i] <= nv - hist) return 1;
            if (sg * y[i] >= nv) break;
        }
    }
    return 0;
}

int osc_ultimo_periodo(const SimReg *r, int k, double *ta, double *tb)
{
    double t0, tf, w;
    int i;
    k = k ? 1 : 0;
    if (r->n < 4) return 0;
    t0 = r->t[0]; tf = r->t[r->n - 1];
    if (!(tf > t0)) return 0;
    /* la cola, cada vez más corta: el arranque (la carga del C de un
     * filtro) deja el nivel medio del registro entero fuera del rizado. Y
     * por último el registro entero: con menos de 4 periodos, la mitad
     * final no tiene los dos cruces que pide osc_medidas */
    for (i = 0; i < 13; i++) {
        Medidas m;
        double nivel, t1, t2, T;
        w = i < 12 ? 0.5 * (tf - t0) / (double)(1L << i) : tf - t0;
        osc_medidas(r, k, tf - w, tf, &m);
        if (!m.hay_frec) continue;
        nivel = 0.5 * (m.max + m.min);
        if (!osc_evento(r, k, nivel, 1, tf, -1, &t2)) continue;       /* el último cruce de subida */
        if (!osc_evento(r, k, nivel, 1, t2, -1, &t1)) continue;       /* y el de antes */
        T = t2 - t1;
        if (!(T > 0.0) || tf - t2 > T * (1.0 + 1e-9)) continue;       /* ya no cruza al final */
        if (fabs(T * m.frec - 1.0) > 0.05) continue;                  /* no se repite con ese T */
        *ta = tf - T; *tb = tf;
        return 1;
    }
    return 0;
}

int osc_escala_ge(double x)
{
    int i;
    if (!(x > 0.0)) return 0;
    for (i = 0; i < N_ESCALA; i++) if (ESCALA[i] >= x * (1.0 - 1e-9)) return i;
    return N_ESCALA - 1;
}

int osc_pix(double v, double vdiv, int pos)
{
    double y = OSC_YC - (v / vdiv + 0.5 * pos) * OSC_DY;
    if (!(y >= OSC_Y0 - 1)) y = OSC_Y0 - 1;      /* también un NaN */
    if (y > OSC_Y0 + 169) y = OSC_Y0 + 169;
    return (int)floor(y + 0.5);
}

static int hay_canal(const SimRes *s, int k) { return s->canal[k].tipo != CANAL_NADA; }
static int es_corriente(const SimRes *s, int k) { return s->canal[k].tipo == CANAL_I; }

void osc_autoescala(Osc *o, const SimReg *r, const SimRes *s)
{
    int k, q, it = osc_escala_ge(s->t_fin / 10.0);
    if (it < IT_MIN) it = IT_MIN;
    if (it > IT_MAX) it = IT_MAX;
    o->itdiv = (signed char)it;
    o->tizq = 0.0;
    for (k = 0; k < 2; k++) {
        double lo, hi, vd, c;
        int iv, p;
        if (!hay_canal(s, k)) continue;
        lo = r->n > 0 ? r->ymin[k] : 0.0;
        hi = r->n > 0 ? r->ymax[k] : 0.0;
        /* los dos canales de la misma magnitud comparten escala y cero */
        q = 1 - k;
        if (hay_canal(s, q) && es_corriente(s, q) == es_corriente(s, k) && r->n > 0) {
            if (r->ymin[q] < lo) lo = r->ymin[q];
            if (r->ymax[q] > hi) hi = r->ymax[q];
        }
        if (hi - lo <= 1e-12 * (fabs(hi) + fabs(lo))) {            /* constante */
            c = hi;
            vd = c != 0.0 ? fabs(c) / 3.0 : (es_corriente(s, k) ? 1e-6 : 1e-3);
            if (lo > 0.0) lo = 0.0;
            if (hi < 0.0) hi = 0.0;
        } else {
            if (lo > 0.0) lo = 0.0;
            if (hi < 0.0) hi = 0.0;
            vd = (hi - lo) / 6.0;
        }
        iv = osc_escala_ge(vd);
        o->ivdiv[k] = (signed char)iv;
        {
            double pd = floor(-(hi + lo) / ESCALA[iv] + 0.5);
            if (pd > 8.0) pd = 8.0;
            if (pd < -8.0) pd = -8.0;
            p = (int)pd;
        }
        o->pos[k] = (signed char)p;
    }
    o->c_gen = 0;
}

/* ==========================================================================
 *  La pantalla
 * ======================================================================= */
static double tdiv_de(const Osc *o) { return ESCALA[o->itdiv]; }

static void avisa(Osc *o, const char *txt, u16 col)
{
    Buf b;
    buf_init(&b, o->aviso, sizeof o->aviso);
    buf_str(&b, txt);
    o->aviso_col = col;
}

/* El nivel del disparo: el punto medio del canal k en la ventana que se ve
 * (en el rizado de un filtro, el del rizado y no el del arranque); sin
 * muestras en la ventana, el del registro entero */
static double nivel_ventana(const Osc *o, const SimReg *r, int k)
{
    Medidas m;
    osc_medidas(r, k, o->tizq, o->tizq + 10.0 * tdiv_de(o), &m);
    if (m.hay) return 0.5 * (m.max + m.min);
    return 0.5 * (r->ymax[k ? 1 : 0] + r->ymin[k ? 1 : 0]);
}

void osc_nuevo(Osc *o, const SimReg *r, const SimRes *s, int autoescala)
{
    int k;
    if (autoescala) {
        memset(o, 0, sizeof *o);
        o->panel = OSC_MEDIDAS;
        osc_autoescala(o, r, s);
    }
    for (k = 0; k < 2; k++) o->ver[k] = (unsigned char)hay_canal(s, k);
    if (!o->ver[o->activo]) o->activo = (unsigned char)(o->ver[0] ? 0 : 1);
    if (o->tizq > s->t_fin) o->tizq = 0.0;
    for (k = 0; k < 2; k++) if (o->tc[k] > s->t_fin) o->tc[k] = s->t_fin;
    /* con las escalas a mano el disparo sigue puesto: su nivel, el de la
     * señal nueva (el de antes puede ser de otro circuito y no cruzarla) */
    if (o->disparo != DISP_LIBRE) o->nivel_disp = nivel_ventana(o, r, o->activo);
    o->u_ta = o->u_tb = 0.0;                   /* el periodo de [U] era de la emulación de antes */
    o->entrada[0] = 0;
    o->aviso[0] = 0;
    o->c_gen = 0;
}

/* ¿Las medidas van entre los cursores? (con los cursores encendidos y
 * separados; si no, la ventana entera) */
int osc_mide_cursores(const Osc *o) { return o->cursores && o->tc[0] != o->tc[1]; }

/* Columnas y medidas de la ventana (o de [T1, T2]), si cambió algo */
static void prepara(Osc *o, const SimReg *r, const SimRes *s)
{
    int k;
    double tdiv = tdiv_de(o), ta, tb, va, vb;
    va = o->tizq; vb = o->tizq + 10.0 * tdiv;
    if (osc_mide_cursores(o)) {                       /* ordenados: T2 puede ir antes que T1 */
        ta = o->tc[0] < o->tc[1] ? o->tc[0] : o->tc[1];
        tb = o->tc[0] < o->tc[1] ? o->tc[1] : o->tc[0];
    } else { ta = va; tb = vb; }
    if (o->c_gen == r->gen && o->c_itdiv == o->itdiv && o->c_tizq == o->tizq && o->c_ta == ta && o->c_tb == tb) return;
    for (k = 0; k < 2; k++) {
        if (hay_canal(s, k)) osc_columnas(r, k, o->tizq, tdiv / OSC_DX, OSC_NCOL, o->mn[k], o->mx[k], o->ok[k]);
        else memset(o->ok[k], 0, sizeof o->ok[k]);
        o->f_ok[k] = 0;                              /* sus filas, por hacer */
    }
    for (k = 0; k < 2; k++) {
        if (hay_canal(s, k)) osc_medidas(r, k, ta, tb, &o->med[k]);
        else memset(&o->med[k], 0, sizeof o->med[k]);
    }
    /* un solo periodo, el de [U], tiene un solo cruce de subida y
     * osc_medidas no da frecuencia: es 1/T (en el canal en el que se buscó,
     * mientras T1 y T2 sigan donde los puso) */
    k = o->u_canal ? 1 : 0;
    if (osc_mide_cursores(o) && o->u_tb > o->u_ta && o->tc[0] == o->u_ta && o->tc[1] == o->u_tb && o->med[k].hay &&
        !o->med[k].hay_frec) {
        o->med[k].frec = 1.0 / (o->u_tb - o->u_ta);
        o->med[k].hay_frec = 1;
    }
    o->n_vent = primera_mayor(r, vb) - primera_mayor(r, va);
    o->n_med = primera_mayor(r, tb) - primera_mayor(r, ta);
    o->c_gen = r->gen; o->c_itdiv = o->itdiv; o->c_tizq = o->tizq; o->c_ta = ta; o->c_tb = tb;
}

void osc_prepara(Osc *o, const SimReg *r, const SimRes *s) { prepara(o, r, s); }

static const char *unidad(const SimRes *s, int k) { return es_corriente(s, k) ? "A" : "V"; }
static u16 color_canal(int k) { return k ? C_CANAL2 : C_CANAL1; }

static int x_de(const Osc *o, double t)
{
    double d = floor((t - o->tizq) / tdiv_de(o) * OSC_DX + 0.5);
    if (d > 10000.0) d = 10000.0;                 /* en double: un int se desbordaría */
    if (d < -10000.0) d = -10000.0;
    return OSC_X0 + (int)d;
}

static void reticula(void)
{
    int i, x, y;
    for (i = 0; i <= 10; i++)
        for (y = OSC_Y0; y <= OSC_Y0 + 8 * OSC_DY; y += 3) gfx_px(OSC_X0 + i * OSC_DX, y, C_REJILLA);
    for (i = 0; i <= 8; i++)
        for (x = OSC_X0; x <= OSC_X0 + 10 * OSC_DX; x += 3) gfx_px(x, OSC_Y0 + i * OSC_DY, C_REJILLA);
    /* ejes centrales con marcas de 1/5 de división */
    for (x = OSC_X0; x <= OSC_X0 + 10 * OSC_DX; x += 6) gfx_vline(x, OSC_YC - 1, 3, C_TENUE);
    for (i = 0; i <= 40; i++) gfx_hline(OSC_XC - 1, OSC_Y0 + (i * OSC_DY) / 5, 3, C_TENUE);
}

/* Las filas de las columnas (osc_pix: una ddiv y un floor cada una) se
 * guardan en el Osc: el cuadro siguiente, si prepara no rehízo las
 * columnas y la escala y la posición del canal son las mismas, solo pinta */
static void traza(Osc *o, const SimRes *s, int k)
{
    int c;
    double vd = ESCALA[o->ivdiv[k]];
    u16 col = color_canal(k);
    (void)s;
    if (!(o->f_ok[k] && o->f_iv[k] == o->ivdiv[k] && o->f_pos[k] == o->pos[k])) {
        for (c = 0; c < OSC_NCOL; c++)
            if (o->ok[k][c]) {
                o->f_y0[k][c] = (unsigned char)osc_pix(o->mx[k][c], vd, o->pos[k]);
                o->f_y1[k][c] = (unsigned char)osc_pix(o->mn[k][c], vd, o->pos[k]);
            }
        o->f_ok[k] = 1; o->f_iv[k] = o->ivdiv[k]; o->f_pos[k] = o->pos[k];
    }
    for (c = 0; c < OSC_NCOL; c++) {
        int y0, y1;
        if (!o->ok[k][c]) continue;
        y0 = o->f_y0[k][c];
        y1 = o->f_y1[k][c];
        if (y0 < OSC_Y0) y0 = OSC_Y0;
        if (y1 > OSC_Y0 + 8 * OSC_DY) y1 = OSC_Y0 + 8 * OSC_DY;
        if (y1 >= y0) gfx_vline(OSC_X0 + c, y0, y1 - y0 + 1, col);
    }
}

static void cabecera(const Osc *o, const SimRes *s)
{
    char t[32], u[16];
    Buf b;
    int k;
    gfx_fill(0, 0, SCR_W, 15, C_PANEL);
    for (k = 0; k < 2; k++) {
        int x = 4 + 72 * k;
        char w[16];
        if (!hay_canal(s, k)) continue;
        buf_init(&b, u, sizeof u);
        buf_str(&b, unidad(s, k)); buf_str(&b, "/div");
        buf_init(&b, t, sizeof t);
        buf_ch(&b, (char)('1' + k)); buf_ch(&b, ':');
        buf_str(&b, num_pref(w, sizeof w, ESCALA[o->ivdiv[k]], 3, u));
        gfx_text(x, 3, t, o->ver[k] ? color_canal(k) : C_TENUE, 1);
        if (o->activo == k) gfx_rect(x - 2, 1, gfx_text_w(t, 1) + 4, 13, color_canal(k));
    }
    num_pref(t, sizeof t, tdiv_de(o), 3, "s/div");
    gfx_text(148, 3, t, C_TEXTO, 1);
    if (o->disparo == DISP_LIBRE) gfx_text(208, 3, "libre", C_SUAVE, 1);
    else {
        buf_init(&b, t, sizeof t);
        buf_str(&b, o->disparo == DISP_SUBIDA ? S_UARR : S_DARR);
        buf_ch(&b, (char)('1' + o->activo));
        gfx_text(208, 3, t, color_canal(o->activo), 1);
    }
}

static void barra_registro(const Osc *o, const SimReg *r, const SimRes *s)
{
    double tdiv = tdiv_de(o), der = o->tizq + 10.0 * tdiv, total = s->t_pedido > s->t_fin ? s->t_pedido : s->t_fin;
    double t_hecho = r->n > 0 ? r->t[r->n - 1] : 0.0;
    int x0, x1, k;
    if (der > total) total = der;
    if (!(total > 0.0)) return;
    x1 = (int)(t_hecho / total * SCR_W);
    gfx_fill(0, 15, x1, 2, C_TENUE);
    if (s->fin == FIN_CORTADO || s->fin == FIN_TOPE) {
        x0 = (int)(s->t_pedido / total * SCR_W);
        if (x0 > x1) gfx_fill(x1, 15, x0 - x1, 2, C_ERROR);
    }
    x0 = (int)((o->tizq > 0.0 ? o->tizq : 0.0) / total * SCR_W);
    x1 = (int)(der / total * SCR_W);
    if (x1 > SCR_W) x1 = SCR_W;
    if (x1 > x0) gfx_fill(x0, 15, x1 - x0, 2, C_SUAVE);
    if (o->cursores)
        for (k = 0; k < 2; k++) gfx_vline((int)(o->tc[k] / total * (SCR_W - 1)), 15, 2, C_TEXTO);
}

static void marcas(const Osc *o, const SimReg *r, const SimRes *s)
{
    int k;
    for (k = 1; k >= 0; k--) {
        int y;
        if (!hay_canal(s, k) || !o->ver[k]) continue;
        y = osc_pix(0.0, ESCALA[o->ivdiv[k]], o->pos[k]);
        if (y < OSC_Y0 + 3) y = OSC_Y0 + 3;
        if (y > OSC_Y0 + 8 * OSC_DY - 3) y = OSC_Y0 + 8 * OSC_DY - 3;
        gfx_tri(1, y - 3, 1, y + 3, 7, y, color_canal(k));
    }
    (void)r;
    if (o->disparo != DISP_LIBRE) {
        int y = osc_pix(o->nivel_disp, ESCALA[o->ivdiv[o->activo]], o->pos[o->activo]);
        gfx_tri(OSC_X0 + OSC_DX - 3, OSC_Y0, OSC_X0 + OSC_DX + 3, OSC_Y0, OSC_X0 + OSC_DX, OSC_Y0 + 4, C_TEXTO);
        gfx_tri(318, y - 3, 318, y + 3, 312, y, color_canal(o->activo));
    }
}

static void leyenda(const Osc *o, const SimReg *r, const SimRes *s)
{
    char t[48];
    Buf b;
    int k, x = OSC_X0 + 2;
    for (k = 0; k < 2; k++) {
        if (!hay_canal(s, k)) continue;
        buf_init(&b, t, sizeof t);
        buf_ch(&b, (char)('1' + k)); buf_ch(&b, ' ');
        buf_str(&b, s->canal[k].nombre);
        x += gfx_text_bg(x, OSC_Y0 + 2, t, o->ver[k] ? color_canal(k) : C_TENUE, C_FONDO, 1) + 6;
    }
    if (s->canal[0].automatico) gfx_text_bg(x, OSC_Y0 + 2, "auto", C_SUAVE, C_FONDO, 1);
    if (osc_mide_cursores(o) && o->panel == OSC_MEDIDAS) {        /* las medidas son de [T1, T2] */
        const char *p = o->n_med < 150 && r->n > 0 ? "T1-T2: pocas muestras [=]" : "medidas en T1-T2";
        gfx_text_bg(OSC_X0 + 10 * OSC_DX - 2 - gfx_text_w(p, 1), OSC_Y0 + 2, p, C_TEXTO, C_FONDO, 1);
    } else if (o->n_vent < 150 && r->n > 0) {
        const char *p = "pocas muestras [=]";
        gfx_text_bg(OSC_X0 + 10 * OSC_DX - 2 - gfx_text_w(p, 1), OSC_Y0 + 2, p, C_SUAVE, C_FONDO, 1);
    }
    /* los tiempos de los bordes */
    num_pref(t, sizeof t, o->tizq, 3, "s");
    gfx_text_bg(OSC_X0 + 2, 177, t, C_SUAVE, C_FONDO, 1);
    num_pref(t, sizeof t, o->tizq + 10.0 * tdiv_de(o), 3, "s");
    gfx_text_bg(OSC_X0 + 10 * OSC_DX - 2 - gfx_text_w(t, 1), 177, t, C_SUAVE, C_FONDO, 1);
}

/* Las líneas de T1 y T2 y sus cajitas «1» y «2» (van antes que la leyenda:
 * los rótulos, con su fondo, quedan encima de las líneas). Si las dos
 * líneas caen a menos de una caja, la de T2 sube una caja y no se montan */
#define OSC_CAJA_Y (OSC_Y0 + 8 * OSC_DY - 22)
static void cursores(const Osc *o, const SimReg *r, const SimRes *s)
{
    int q, k, x1 = x_de(o, o->tc[0]), x2 = x_de(o, o->tc[1]);
    int juntas = x1 - x2 < 10 && x2 - x1 < 10;
    for (q = 1; q >= 0; q--) {
        int ct = o->cur == 0 ? q : 1 - q, x, yc = OSC_CAJA_Y - (ct == 1 && juntas ? 11 : 0);
        u16 col = ct == o->cur ? C_TEXTO : C_SUAVE;
        char t[4];
        double tc = o->tc[ct];
        t[0] = (char)('1' + ct); t[1] = 0;
        x = x_de(o, tc);
        if (x < OSC_X0) { char u[4]; u[0] = S_LARR[0]; u[1] = t[0]; u[2] = 0; gfx_text_bg(OSC_X0 + 1, 160 - 10 * ct, u, col, C_FONDO, 1); continue; }
        if (x > OSC_X0 + 10 * OSC_DX) { char u[4]; u[0] = t[0]; u[1] = S_RARR[0]; u[2] = 0; gfx_text_bg(OSC_X0 + 10 * OSC_DX - 13, 160 - 10 * ct, u, col, C_FONDO, 1); continue; }
        gfx_line_dash(x, OSC_Y0, x, OSC_Y0 + 8 * OSC_DY, col, 3, 2);
        gfx_fill(x - 4, yc, 9, 10, col);
        gfx_text(x - 2, yc + 1, t, C_FONDO, 1);
        for (k = 0; k < 2; k++) {
            int y;
            if (!hay_canal(s, k) || !o->ver[k] || tc > (r->n > 0 ? r->t[r->n - 1] : 0.0)) continue;
            y = osc_pix(osc_valor(r, k, tc), ESCALA[o->ivdiv[k]], o->pos[k]);
            gfx_rect(x - 1, y - 1, 3, 3, color_canal(k));
        }
    }
}

/* Lo que no llega a una millonésima de división no se lee: el ruido del
 * redondeo (-1e-14 V a 5 V/div salía «-0,01pV»), 0 */
static double a_escala(double v, double vdiv) { return fabs(v) < 1e-6 * vdiv ? 0.0 : v; }

static void panel(const Osc *o, const SimReg *r, const SimRes *s)
{
    static const char *const ROT[6] = { "máx", "mín", "p-p", "media", "eficaz", "frec" };
    char t[64], u[20];
    Buf b;
    int j, k;
    gfx_fill(0, OSC_PANEL_Y, SCR_W, 38, C_PANEL);
    if (o->entrada[0]) {
        int ok;
        double v = num_lee_ing(o->entrada, &ok);
        buf_init(&b, t, sizeof t);
        buf_str(&b, S_RARR "T"); buf_ch(&b, (char)('1' + o->cur)); buf_str(&b, " = ");
        buf_str(&b, o->entrada); buf_ch(&b, '_');
        if (ok) { buf_str(&b, "  (= "); buf_str(&b, num_pref(u, sizeof u, v, 4, " s")); buf_ch(&b, ')'); }
        gfx_text(4, OSC_PANEL_Y + 13, t, C_CURSOR, 1);
        return;
    }
    if (o->panel == OSC_CURSORES && o->cursores) {
        double v[2][2];
        for (j = 0; j < 2; j++) {
            buf_init(&b, t, sizeof t);
            buf_str(&b, o->cur == j ? S_RARR "T" : " T"); buf_ch(&b, (char)('1' + j)); buf_ch(&b, ' ');
            buf_str(&b, num_pref(u, sizeof u, o->tc[j], 4, "s"));
            for (k = 0; k < 2; k++) {
                if (!hay_canal(s, k)) continue;
                v[j][k] = a_escala(osc_valor(r, k, o->tc[j]), ESCALA[o->ivdiv[k]]);
                buf_str(&b, "  "); buf_ch(&b, (char)('1' + k)); buf_str(&b, ": ");
                buf_str(&b, num_pref(u, sizeof u, v[j][k], 4, unidad(s, k)));
            }
            gfx_text(4, OSC_PANEL_Y + 1 + 13 * j, t, o->cur == j ? C_TEXTO : C_SUAVE, 1);
        }
        buf_init(&b, t, sizeof t);
        buf_str(&b, S_DELTA "T "); buf_str(&b, num_pref(u, sizeof u, o->tc[1] - o->tc[0], 4, "s"));
        if (o->tc[1] != o->tc[0]) { buf_str(&b, "  1/" S_DELTA "T "); buf_str(&b, num_pref(u, sizeof u, 1.0 / fabs(o->tc[1] - o->tc[0]), 4, "Hz")); }
        for (k = 0; k < 2; k++) {
            if (!hay_canal(s, k)) continue;
            buf_str(&b, "  " S_DELTA); buf_ch(&b, (char)('1' + k)); buf_ch(&b, ' ');
            buf_str(&b, num_pref(u, sizeof u, a_escala(v[1][k] - v[0][k], ESCALA[o->ivdiv[k]]), 3, unidad(s, k)));
        }
        gfx_text(4, OSC_PANEL_Y + 27, t, C_SUAVE, 1);
        return;
    }
    for (j = 0; j < 6; j++) gfx_text(16 + 50 * j, OSC_PANEL_Y + 1, ROT[j], C_SUAVE, 1);
    for (k = 0; k < 2; k++) {
        const Medidas *m = &o->med[k];
        double val[6];
        int y = OSC_PANEL_Y + 14 + 12 * k;
        if (!hay_canal(s, k)) continue;
        t[0] = (char)('1' + k); t[1] = 0;
        gfx_text(4, y, t, color_canal(k), 1);
        if (!m->hay) continue;
        val[0] = m->max; val[1] = m->min; val[2] = m->pp; val[3] = m->media; val[4] = m->eficaz; val[5] = m->frec;
        for (j = 0; j < 5; j++) val[j] = a_escala(val[j], ESCALA[o->ivdiv[k]]);
        for (j = 0; j < 6; j++) {
            if (j == 5) { if (m->hay_frec) gfx_text(16 + 50 * j, y, num_pref(u, sizeof u, val[5], 3, "Hz"), C_TEXTO, 1); continue; }
            gfx_text(16 + 50 * j, y, num_pref(u, sizeof u, val[j], 3, unidad(s, k)), C_TEXTO, 1);
        }
    }
}

void osc_dibuja(Osc *o, const SimReg *r, const SimRes *s)
{
    double tdiv, der;
    int k;
    prepara(o, r, s);
    tdiv = tdiv_de(o); der = o->tizq + 10.0 * tdiv;
    gfx_clear(C_FONDO);
    cabecera(o, s);
    barra_registro(o, r, s);
    if (osc_mide_cursores(o) && o->panel == OSC_MEDIDAS) {     /* el tramo que se mide, sombreado */
        int xa = x_de(o, o->tc[0] < o->tc[1] ? o->tc[0] : o->tc[1]);
        int xb = x_de(o, o->tc[0] < o->tc[1] ? o->tc[1] : o->tc[0]);
        if (xa < OSC_X0) xa = OSC_X0;
        if (xb > OSC_X0 + 10 * OSC_DX) xb = OSC_X0 + 10 * OSC_DX;
        if (xb > xa) gfx_fill(xa, OSC_Y0, xb - xa, 8 * OSC_DY, C_PANEL);
    }
    reticula();
    /* después de lo emulado: rayado y cómo seguir */
    if (der > s->t_fin * (1.0 + 1e-9)) {
        int x = x_de(o, s->t_fin);
        char t[40], u[16];
        Buf b;
        if (x < OSC_X0) x = OSC_X0;
        gfx_hatch(x, OSC_Y0, OSC_X0 + 10 * OSC_DX - x, 8 * OSC_DY, C_TENUE, 6);
        buf_init(&b, t, sizeof t);
        buf_str(&b, "[=] emular hasta ");
        buf_str(&b, num_pref(u, sizeof u, der, 3, "s"));
        if (OSC_X0 + 10 * OSC_DX - x > gfx_text_w(t, 1) + 4)
            gfx_text_bg(OSC_X0 + 10 * OSC_DX - 2 - gfx_text_w(t, 1), OSC_YC + 30, t, C_TEXTO, C_FONDO, 1);
    }
    for (k = 0; k < 2; k++) {                 /* primero el canal inactivo */
        int q = k == 0 ? 1 - o->activo : o->activo;
        if (hay_canal(s, q) && o->ver[q]) traza(o, s, q);
    }
    marcas(o, r, s);
    if (o->cursores) cursores(o, r, s);
    leyenda(o, r, s);                         /* sus rótulos, encima de las líneas de los cursores */
    panel(o, r, s);
    gfx_fill(0, 226, SCR_W, 14, C_PANEL);
    if (o->aviso[0]) gfx_text(4, 227, o->aviso, o->aviso_col, 1);
    else if (o->entrada[0]) gfx_text(4, 227, "[enter] llevar T1 ahí  [del] borrar  [esc] dejarlo", C_SUAVE, 1);
    else if (o->cursores) gfx_text(4, 227, S_LARR S_RARR " mover  enter T1/T2  núm enter: ir a t  P pico", C_SUAVE, 1);
    else gfx_text(4, 227, "+- t/div  " S_UARR S_DARR " V/div  C canal  X cursores  tab tabla", C_SUAVE, 1);
}

/* ==========================================================================
 *  Teclas
 * ======================================================================= */
static double t_ult(const SimReg *r) { return r->n > 0 ? r->t[r->n - 1] : 0.0; }

/* Que el instante t se vea: la ventana se mueve divisiones enteras */
static void que_se_vea(Osc *o, double t)
{
    double tdiv = tdiv_de(o);
    /* divisiones enteras, de una vez (con 1 ns/div y el cursor a 1 s serían
     * mil millones de vueltas) */
    if (t < o->tizq) o->tizq -= ceil((o->tizq - t) / tdiv) * tdiv;
    else if (t > o->tizq + 10.0 * tdiv) o->tizq += ceil((t - o->tizq - 10.0 * tdiv) / tdiv) * tdiv;
    if (o->tizq < 0.0 && t >= 0.0 && o->disparo == DISP_LIBRE) o->tizq = 0.0;
}

static void centra_en(Osc *o, double t)
{
    double tdiv = tdiv_de(o);
    if (t >= o->tizq && t <= o->tizq + 10.0 * tdiv) return;
    o->tizq = floor((t - 5.0 * tdiv) / tdiv + 0.5) * tdiv;
    if (o->tizq < 0.0) o->tizq = 0.0;
}

static void no_cruza(Osc *o, const SimRes *s, double nivel, double desde, int sentido)
{
    char t[64], u[20];
    Buf b;
    buf_init(&b, t, sizeof t);
    buf_str(&b, "La señal no cruza ");
    buf_str(&b, num_pref(u, sizeof u, nivel, 3, unidad(s, o->activo)));
    buf_str(&b, o->disparo == DISP_SUBIDA ? " subiendo" : " bajando");
    buf_str(&b, sentido < 0 ? " antes de " : " tras ");
    buf_str(&b, num_pref(u, sizeof u, desde, 3, "s"));
    avisa(o, t, C_ERROR);
}

/* Con disparo: el evento más cercano después (o antes) del instante del
 * disparo; la ventana lo deja a 1 división del borde izquierdo */
static int dispara(Osc *o, const SimReg *r, const SimRes *s, int sentido, int veces)
{
    double tdiv = tdiv_de(o), nivel = o->nivel_disp, t = o->tizq + tdiv, te = t;
    int i;
    for (i = 0; i < veces; i++) {
        double d = sentido >= 0 ? t + 1e-9 * tdiv : t - 1e-9 * tdiv;
        if (!osc_evento(r, o->activo, nivel, o->disparo == DISP_SUBIDA, d, sentido, &te)) {
            if (i == 0) { no_cruza(o, s, nivel, t, sentido); return 0; }
            break;
        }
        t = te;
    }
    o->tizq = t - tdiv;
    return 1;
}

static void escribe(Osc *o, char c)
{
    size_t n = strlen(o->entrada);
    if (n + 1 < sizeof o->entrada) { o->entrada[n] = c; o->entrada[n + 1] = 0; }
}

static void tecla_entrada(Osc *o, const SimReg *r, const SimRes *s, int t, int sh)
{
    size_t n = strlen(o->entrada);
    char ult = n ? o->entrada[n - 1] : 0;
    if (t >= T_DIG(0) && t <= T_DIG(9)) { escribe(o, (char)('0' + (t - T_DIG0))); return; }
    switch (t) {
    case T_PUNTO: escribe(o, '.'); return;
    case T_EE: escribe(o, 'E'); return;
    case T_SIGNO: case T_MENOS: if (ult == 'E') escribe(o, '-'); return;
    case T_BORRAR: if (n) o->entrada[n - 1] = 0; return;
    case T_ESC: o->entrada[0] = 0; return;
    case T_ENTER: {
        int ok;
        double v = num_lee_ing(o->entrada, &ok);
        o->entrada[0] = 0;
        if (!ok || !(v >= 0.0)) { avisa(o, "Eso no es un tiempo.", C_ERROR); return; }
        if (v > s->t_fin) v = s->t_fin;
        o->tc[o->cur] = v;
        centra_en(o, v);
        (void)r;
        return;
    }
    default: break;
    }
    /* los prefijos; lo demás no hace nada mientras se escribe */
    if (t == T_LETRA('P')) escribe(o, 'p');
    else if (t == T_LETRA('N')) escribe(o, 'n');
    else if (t == T_LETRA('U')) escribe(o, 'u');
    else if (t == T_LETRA('M')) escribe(o, sh ? 'M' : 'm');
    else if (t == T_LETRA('K')) escribe(o, 'k');
    else if (t == T_LETRA('G')) escribe(o, 'G');
}

int osc_tecla(Osc *o, const SimReg *r, const SimRes *s, int tk)
{
    int sh = (tk & T_CON_SHIFT) != 0, t = tk & ~T_MODS;
    double tdiv = tdiv_de(o);

    o->aviso[0] = 0;
    /* 1. escribiendo un tiempo */
    if (o->entrada[0]) { tecla_entrada(o, r, s, t, sh); return OSC_NADA; }

    /* 2. t/div */
    if (t == T_MAS || t == T_MENOS) {
        int it = o->itdiv + (t == T_MAS ? -1 : 1);
        double centro = o->tizq + 5.0 * tdiv;
        if (it < IT_MIN || it > IT_MAX) return OSC_NADA;
        o->itdiv = (signed char)it;
        if (o->disparo != DISP_LIBRE) { double td = o->tizq + tdiv; o->tizq = td - tdiv_de(o); }
        else if (o->tizq != 0.0) { o->tizq = centro - 5.0 * tdiv_de(o); if (o->tizq < 0.0) o->tizq = 0.0; }
        return OSC_NADA;
    }
    /* 3. V/div y posición */
    if (t == T_ARRIBA || t == T_ABAJO) {
        int k = o->activo;
        if (!hay_canal(s, k)) return OSC_NADA;
        if (sh) {
            int p = o->pos[k] + (t == T_ARRIBA ? 1 : -1);
            if (p >= -8 && p <= 8) o->pos[k] = (signed char)p;
        } else {
            int iv = o->ivdiv[k] + (t == T_ARRIBA ? -1 : 1);
            if (iv >= 0 && iv < N_ESCALA) o->ivdiv[k] = (signed char)iv;
        }
        o->ajustado = 1;
        return OSC_NADA;
    }
    /* 4. izquierda y derecha */
    if (t == T_IZQ || t == T_DER) {
        int sg = t == T_DER ? 1 : -1;
        if (tk & T_CON_CTRL) {
            /* [ctrl]: el cursor al borde de la ventana; sin cursores, la
             * ventana al principio o al final del registro */
            if (o->cursores) {
                double v = sg > 0 ? o->tizq + 10.0 * tdiv : o->tizq;
                if (v < 0.0) v = 0.0;
                if (v > s->t_fin) v = s->t_fin;
                o->tc[o->cur] = v;
            } else {
                double fin = s->t_fin - 10.0 * tdiv;
                o->tizq = sg > 0 && fin > 0.0 ? fin : 0.0;
            }
            return OSC_NADA;
        }
        if (o->cursores) {
            double v = o->tc[o->cur] + sg * (sh ? tdiv : tdiv / 10.0);
            if (v < 0.0) v = 0.0;
            if (v > s->t_fin) v = s->t_fin;
            o->tc[o->cur] = v;
            que_se_vea(o, v);
        } else if (o->disparo != DISP_LIBRE) dispara(o, r, s, sg, sh ? 10 : 1);
        else {
            double tope = s->t_fin - tdiv > 0.0 ? s->t_fin - tdiv : 0.0;
            o->tizq += sg * (sh ? 10.0 : 1.0) * tdiv;
            if (o->tizq > tope) o->tizq = tope;
            if (o->tizq < 0.0) o->tizq = 0.0;
        }
        return OSC_NADA;
    }
    /* 5. canal */
    if (t == T_LETRA('C')) {
        if (!hay_canal(s, 1 - o->activo)) {
            avisa(o, o->activo ? "No hay canal 1: pon otra sonda con [O]" : "No hay canal 2: pon otra sonda con [O]", C_CURSOR);
            return OSC_NADA;
        }
        o->activo = (unsigned char)(1 - o->activo);
        if (o->disparo != DISP_LIBRE) o->nivel_disp = nivel_ventana(o, r, o->activo);   /* el disparo sigue al activo */
        return OSC_NADA;
    }
    if (t == T_BORRAR) { o->ver[o->activo] = (unsigned char)!o->ver[o->activo]; return OSC_NADA; }
    /* 6. cursores */
    if (t == T_LETRA('X')) {
        o->cursores = (unsigned char)!o->cursores;
        if (o->cursores) {
            if (o->tc[0] == 0.0 && o->tc[1] == 0.0) {
                o->tc[0] = o->tizq + 2.0 * tdiv; o->tc[1] = o->tizq + 8.0 * tdiv;
                if (o->tc[0] > s->t_fin) o->tc[0] = s->t_fin;
                if (o->tc[1] > s->t_fin) o->tc[1] = s->t_fin;
                if (o->tc[0] < 0.0) o->tc[0] = 0.0;
            }
            o->panel = OSC_CURSORES;
        } else o->panel = OSC_MEDIDAS;
        return OSC_NADA;
    }
    if (t == T_ENTER) { if (o->cursores) o->cur = (unsigned char)(1 - o->cur); return OSC_NADA; }
    if ((t >= T_DIG(0) && t <= T_DIG(9)) || t == T_PUNTO) {
        if (!o->cursores) {
            int q;
            o->cursores = 1;
            o->tc[0] = o->tizq + 2.0 * tdiv; o->tc[1] = o->tizq + 8.0 * tdiv;
            for (q = 0; q < 2; q++) {
                if (o->tc[q] > s->t_fin) o->tc[q] = s->t_fin;
                if (o->tc[q] < 0.0) o->tc[q] = 0.0;
            }
        }
        o->panel = OSC_CURSORES;
        o->cur = 0;
        escribe(o, t == T_PUNTO ? '.' : (char)('0' + (t - T_DIG0)));
        return OSC_NADA;
    }
    /* 7. picos */
    if (t == T_LETRA('P')) {
        double desde = o->cursores ? o->tc[o->cur] : o->tizq, tp, vp;
        if (!hay_canal(s, o->activo)) return OSC_NADA;
        if (!osc_pico(r, o->activo, desde + 1e-12 * (tdiv > 0.0 ? tdiv : 1.0), !sh, &tp, &vp)) {
            char m[64], u[20];
            Buf b;
            buf_init(&b, m, sizeof m);
            buf_str(&b, sh ? "No hay más mínimos después de " : "No hay más picos después de ");
            buf_str(&b, num_pref(u, sizeof u, desde, 3, "s"));
            avisa(o, m, C_CURSOR);
            return OSC_NADA;
        }
        if (!o->cursores) { o->cursores = 1; o->tc[1] = tp; o->cur = 0; }
        o->panel = OSC_CURSORES;
        o->tc[o->cur] = tp;
        que_se_vea(o, tp);
        return OSC_NADA;
    }
    /* 8. panel */
    if (t == T_LETRA('M')) {
        if (o->cursores) o->panel = (unsigned char)(o->panel == OSC_MEDIDAS ? OSC_CURSORES : OSC_MEDIDAS);
        return OSC_NADA;
    }
    /* 9. disparo */
    if (t == T_LETRA('D')) {
        if (!hay_canal(s, o->activo)) return OSC_NADA;
        o->disparo = (unsigned char)((o->disparo + 1) % 3);
        if (o->disparo != DISP_LIBRE) {
            double t0 = o->tizq;
            o->nivel_disp = nivel_ventana(o, r, o->activo);   /* el de lo que se ve, fijo desde ahora */
            o->tizq = t0 - tdiv;                 /* que busque desde el borde izquierdo */
            if (!dispara(o, r, s, 1, 1)) {
                if (o->disparo == DISP_SUBIDA) {        /* no sube nunca: ¿baja? */
                    o->disparo = DISP_BAJADA;
                    o->tizq = t0 - tdiv;
                    if (dispara(o, r, s, 1, 1)) { o->aviso[0] = 0; return OSC_NADA; }
                }
                o->tizq = t0; o->disparo = DISP_LIBRE;
            }
        } else if (o->tizq < 0.0) o->tizq = 0.0;
        return OSC_NADA;
    }
    /* 9b. el último periodo (el régimen permanente): T1 y T2 en sus bordes,
     * las medidas entre ellos y la ventana acabando en el final */
    if (t == T_LETRA('U')) {
        double ta, tb, T;
        char m[48], u[20];
        Buf b;
        int it;
        if (!hay_canal(s, o->activo)) return OSC_NADA;
        if (!osc_ultimo_periodo(r, o->activo, &ta, &tb)) {
            avisa(o, "Sin periodo: la señal no se repite.", C_CURSOR);
            return OSC_NADA;
        }
        T = tb - ta;
        o->cursores = 1; o->tc[0] = ta; o->tc[1] = tb;
        o->u_ta = ta; o->u_tb = tb; o->u_canal = o->activo;   /* la frec de la medida: 1/T */
        o->panel = OSC_MEDIDAS;
        it = osc_escala_ge(T / 8.0);              /* el periodo, en 3,2 a 8 divisiones */
        if (it < IT_MIN) it = IT_MIN;
        if (it > IT_MAX) it = IT_MAX;
        o->itdiv = (signed char)it;
        o->tizq = tb - 10.0 * ESCALA[it];
        if (o->tizq < 0.0) o->tizq = 0.0;
        buf_init(&b, m, sizeof m);
        buf_str(&b, "Último periodo: ");
        buf_str(&b, num_pref(u, sizeof u, T, 4, " s"));
        buf_str(&b, "  [M] cursores");
        avisa(o, m, C_OK);
        return OSC_NADA;
    }
    /* 10. autoescala */
    if (t == T_LETRA('A')) { osc_autoescala(o, r, s); o->ajustado = 0; o->disparo = DISP_LIBRE; return OSC_NADA; }
    /* 11 y 12 */
    if (t == T_IGUAL) return OSC_EMULAR;
    if (t == T_TAB) return OSC_TABLA;
    if (t == T_MENU) return OSC_AJUSTES;
    if (t == T_ESC) return OSC_SALIR;
    (void)t_ult;
    return OSC_NADA;
}
