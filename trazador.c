/* ============================================================================
 *  trazador.c  -  La pantalla del Bode  (ver trazador.h)
 *
 *  Orden del archivo:
 *    1. funciones puras: columnas, filas, valor interpolado, autoescala,
 *       marcas y lecturas
 *    2. la pantalla: cabecera, retícula, curvas, marcas, cursores, eje de f,
 *       panel y pie
 *    3. las teclas, en el orden de la especificación (§4.7)
 * ========================================================================= */
#include <string.h>
#include <math.h>
#include "trazador.h"
#include "numfmt.h"
#include "ui.h"

const double DBDIV[6] = { 1.0, 2.0, 5.0, 10.0, 20.0, 40.0 };
const double FDIV[4] = { 15.0, 30.0, 45.0, 90.0 };

#define N_DBDIV    6
#define N_FDIV     4
#define BD_X1      (BD_X0 + BD_W)          /* 314: borde derecho            */
#define BD_MY1     (BD_MY0 + 4 * BD_MDY)   /* 101: abajo de la magnitud     */
#define BD_FY1     (BD_FY0 + 4 * BD_FDY)   /* 162: abajo de la fase         */
#define PI_Z       3.14159265358979323846
#define DB_3       3.0102999566398120      /* 20·log10(√2)                  */
#define MIN_VISTA  30                      /* menos puntos a la vista: aviso */
#define TOPE_DIV   127                     /* db_top y fase_top: signed char */
#define MAX_MARCAS 8
#define TOL_REL    1e-9                    /* dos f son la misma            */

/* ==========================================================================
 *  1. Funciones puras
 * ======================================================================= */
static int hay_malla(const BodeReg *g)
{
    return g && g->n >= 2 && g->f[0] > 0.0 && g->f[g->n - 1] > g->f[0];
}
static double lg_ini(const BodeReg *g) { return log10(g->f[0]); }
static double lg_fin(const BodeReg *g) { return log10(g->f[g->n - 1]); }

static double ancho(const Trazador *z)
{
    double w = z->lf1 - z->lf0;
    return w > 0.0 ? w : 1.0;
}

int traz_x(const Trazador *z, double f)
{
    double d = (log10(f) - z->lf0) / ancho(z) * BD_W;    /* f <= 0 o NaN: -inf o NaN */
    if (!(d >= -1.0)) d = -1.0;
    if (d > BD_W + 1.0) d = BD_W + 1.0;
    return BD_X0 + (int)floor(d + 0.5);
}

static double dbdiv(const Trazador *z)
{
    int i = z->idb < 0 ? 0 : z->idb >= N_DBDIV ? N_DBDIV - 1 : z->idb;
    return DBDIV[i];
}

static double fdiv(const Trazador *z)
{
    int i = z->ifase < 0 ? 0 : z->ifase >= N_FDIV ? N_FDIV - 1 : z->ifase;
    return FDIV[i];
}

/* Fila de v con el tope 'tope' (en divisiones) de d por división; limitada a
 * un píxel por fuera de [y0, y0 + 4·dy] */
static int fila(double v, double d, int tope, int y0, int dy)
{
    double y = y0 + ((double)tope * d - v) / d * dy;
    if (!(y >= y0 - 1)) y = y0 - 1;                  /* también un NaN */
    if (y > y0 + 4 * dy + 1) y = y0 + 4 * dy + 1;
    return (int)floor(y + 0.5);
}

int traz_y_db(const Trazador *z, double db) { return fila(db, dbdiv(z), z->db_top, BD_MY0, BD_MDY); }
int traz_y_fase(const Trazador *z, double grados) { return fila(grados, fdiv(z), z->fase_top, BD_FY0, BD_FDY); }

static double valor_k(const BodeReg *g, int k, int fase) { return fase ? g->fase[k] : g->db[k]; }

double traz_valor(const BodeReg *g, double f, int fase)
{
    int lo, hi, n;
    double t;
    if (!hay_malla(g) || !(f > 0.0)) return NAN;
    n = g->n;
    /* los bordes de la ventana (10^lf) pueden caer un pelo fuera */
    if (f < g->f[0]) { if (f < g->f[0] * (1.0 - TOL_REL)) return NAN; f = g->f[0]; }
    if (f > g->f[n - 1]) { if (f > g->f[n - 1] * (1.0 + TOL_REL)) return NAN; f = g->f[n - 1]; }
    lo = 0; hi = n - 1;                              /* f[lo] <= f <= f[hi] */
    while (hi - lo > 1) { int m = (lo + hi) / 2; if (g->f[m] <= f) lo = m; else hi = m; }
    if (f == g->f[lo]) return g->ok[lo] ? valor_k(g, lo, fase) : NAN;
    if (f == g->f[hi]) return g->ok[hi] ? valor_k(g, hi, fase) : NAN;
    if (!g->ok[lo] || !g->ok[hi]) return NAN;        /* hueco: una resonancia ideal */
    t = log(f / g->f[lo]) / log(g->f[hi] / g->f[lo]);
    return valor_k(g, lo, fase) + t * (valor_k(g, hi, fase) - valor_k(g, lo, fase));
}

/* La escala de la tabla esc (n valores) en la que [lo, hi] cabe en 4
 * divisiones: desde la primera >= (hi - lo)/4, la primera en la que cabe con
 * el tope en una división entera; entre los topes que valen, el que centra
 * la curva (empate: el más alto). Si no cabe ni con la mayor, el tope sobre
 * el máximo. */
static void escala(const double *esc, int n, double lo, double hi, signed char *iesc, signed char *tope)
{
    int i = 0;
    double centro = 0.5 * (lo + hi), t;
    while (i < n - 1 && esc[i] < 0.25 * (hi - lo) * (1.0 - 1e-12)) i++;
    for (; i < n; i++) {
        double d = esc[i], ta = ceil(hi / d - 1e-9), tb = floor(lo / d + 1e-9) + 4.0, mejor = 0.0, emin = 1e300;
        if (ta < -TOPE_DIV) ta = -TOPE_DIV;
        if (tb > TOPE_DIV) tb = TOPE_DIV;
        if (ta > tb) continue;
        for (t = ta; t <= tb; t += 1.0) {            /* 5 candidatos como mucho */
            double e = fabs((t - 2.0) * d - centro);
            if (e <= emin + 1e-9 * d) { emin = e; mejor = t; }
        }
        *iesc = (signed char)i;
        *tope = (signed char)mejor;
        return;
    }
    t = ceil(hi / esc[n - 1] - 1e-9);
    if (t > TOPE_DIV) t = TOPE_DIV;
    if (t < -TOPE_DIV) t = -TOPE_DIV;
    *iesc = (signed char)(n - 1);
    *tope = (signed char)t;
}

void traz_autoescala(Trazador *z, const BodeReg *g)
{
    double lo[2] = { 1e300, 1e300 }, hi[2] = { -1e300, -1e300 };
    int k, q;
    if (hay_malla(g)) {
        double fa = pow(10.0, z->lf0) * (1.0 - TOL_REL), fb = pow(10.0, z->lf1) * (1.0 + TOL_REL);
        for (k = 0; k < g->n; k++) {
            if (!g->ok[k] || g->f[k] < fa || g->f[k] > fb) continue;
            for (q = 0; q < 2; q++) {
                double v = valor_k(g, k, q);
                if (q == 0 && v <= -299.0) continue;             /* |H| = 0: -inf dB */
                if (v < lo[q]) lo[q] = v;
                if (v > hi[q]) hi[q] = v;
            }
        }
        /* los bordes de la ventana, entre dos puntos */
        for (k = 0; k < 2; k++)
            for (q = 0; q < 2; q++) {
                double v = traz_valor(g, pow(10.0, k ? z->lf1 : z->lf0), q);
                if (v != v || (q == 0 && v <= -299.0)) continue;
                if (v < lo[q]) lo[q] = v;
                if (v > hi[q]) hi[q] = v;
            }
    }
    if (hi[0] < lo[0]) { z->idb = 3; z->db_top = 0; }
    else escala(DBDIV, N_DBDIV, lo[0], hi[0], &z->idb, &z->db_top);
    if (hi[1] < lo[1]) { z->ifase = 3; z->fase_top = 2; }
    else escala(FDIV, N_FDIV, lo[1], hi[1], &z->ifase, &z->fase_top);
}

static int pon_marca(double *f, const char **nom, int n, int max, double v, const char *t)
{
    if (!(v > 0.0) || n >= max) return n;
    f[n] = v; nom[n] = t;
    return n + 1;
}

int traz_marcas(const BodeRes *r, double *f, const char **nom, int max)
{
    const BodeMedidas *m = &r->m;
    int n = 0, i, j;
    if (r->fin != FIN_COMPLETO) return 0;
    switch (m->tipo) {
    case BODE_PB: case BODE_PA:
        n = pon_marca(f, nom, n, max, m->fc, "fc");
        n = pon_marca(f, nom, n, max, m->f_pico, "pico");
        n = pon_marca(f, nom, n, max, m->f_valle, "valle");
        break;
    case BODE_PBANDA:
        n = pon_marca(f, nom, n, max, m->f1, "fc1");
        n = pon_marca(f, nom, n, max, m->f_pico, "f0");
        n = pon_marca(f, nom, n, max, m->f2, "fc2");
        /* la fase 0, si no es el mismo punto que el pico */
        if (m->f_fase0 > 0.0 && m->f_pico > 0.0 && fabs(log(m->f_fase0 / m->f_pico)) > 1e-6)
            n = pon_marca(f, nom, n, max, m->f_fase0, "fase 0");
        n = pon_marca(f, nom, n, max, m->f_valle, "valle");
        break;
    case BODE_RB:
        n = pon_marca(f, nom, n, max, m->f1, "fc1");
        n = pon_marca(f, nom, n, max, m->f_valle, "f0");
        n = pon_marca(f, nom, n, max, m->f2, "fc2");
        n = pon_marca(f, nom, n, max, m->f_pico, "pico");
        break;
    default:
        n = pon_marca(f, nom, n, max, m->f_pico, "pico");
        n = pon_marca(f, nom, n, max, m->f_valle, "valle");
        break;
    }
    for (i = 1; i < n; i++)                          /* de menor a mayor f */
        for (j = i; j > 0 && f[j] < f[j - 1]; j--) {
            double tf = f[j]; const char *tn = nom[j];
            f[j] = f[j - 1]; nom[j] = nom[j - 1];
            f[j - 1] = tf; nom[j - 1] = tn;
        }
    return n;
}

/* La fase de la curva en f, o la del punto válido más cercano */
static double fase_cerca(const BodeReg *g, double f)
{
    double v = traz_valor(g, f, 1), dmin = 1e300;
    int k;
    if (v == v || !hay_malla(g) || !(f > 0.0)) return v;
    for (k = 0; k < g->n; k++) {
        double d = fabs(log(g->f[k] / f));
        if (g->ok[k] && d < dmin) { dmin = d; v = g->fase[k]; }
    }
    return v;
}

/* H exacta en f, recordando las últimas: bode_en cuesta una factorización y,
 * con dos cursores, cada tecla pediría cuatro. Valen mientras no cambie el
 * barrido (BodeRes.gen). */
#define N_RECUERDA 4
static struct { unsigned gen; double f; int ok; Cx H; } rec_[N_RECUERDA];
static int rec_sig;

static int H_exacta(double f, Cx *H)
{
    unsigned gen = bode_res()->gen;
    int i;
    for (i = 0; i < N_RECUERDA; i++)
        if (gen != 0 && rec_[i].gen == gen && rec_[i].f == f) { *H = rec_[i].H; return rec_[i].ok; }
    i = rec_sig;
    rec_sig = (rec_sig + 1) % N_RECUERDA;
    rec_[i].ok = bode_en(f, &rec_[i].H);
    rec_[i].gen = gen; rec_[i].f = f;
    *H = rec_[i].H;
    return rec_[i].ok;
}

int traz_lee(const BodeReg *g, double f, double *db, double *mod, double *fase)
{
    Cx H;
    double ref;
    *db = *mod = *fase = 0.0;
    if (!(f > 0.0) || !H_exacta(f, &H)) return 0;
    *mod = cx_abs(H);
    *db = *mod > 0.0 ? 20.0 * log10(*mod) : -300.0;
    *fase = cx_arg(H) * 180.0 / PI_Z;
    ref = fase_cerca(g, f);
    if (ref == ref) *fase += 360.0 * floor((ref - *fase) / 360.0 + 0.5);
    return 1;
}

int traz_en_vista(const Trazador *z, const BodeReg *g)
{
    int k, n = 0;
    double fa, fb;
    if (!hay_malla(g)) return 0;
    fa = pow(10.0, z->lf0) * (1.0 - TOL_REL);
    fb = pow(10.0, z->lf1) * (1.0 + TOL_REL);
    for (k = 0; k < g->n; k++) if (g->f[k] >= fa && g->f[k] <= fb) n++;
    return n;
}

double traz_ancho_min(const BodeReg *g)
{
    if (!hay_malla(g)) return 1.0;
    return 4.0 * (lg_fin(g) - lg_ini(g)) / (double)(g->n - 1);
}

/* ==========================================================================
 *  Formato
 * ======================================================================= */
/* 'cifras' significativas con prefijo del SI y SIN quitar los ceros (num_pref
 * los quita): "1,000kHz", "159,2Hz". Con la unidad " Hz", "1,000 kHz". */
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
    if (ent > 3 && e3 < 9) {                         /* 999,96 pasa a 1,000k */
        e3 += 3; m /= 1000.0;
        dec = cifras - 1;
    }
    buf_str(&b, num_txt(t, sizeof t, m, dec));
    if (unidad && unidad[0] == ' ') { buf_ch(&b, ' '); unidad++; }
    buf_str(&b, PRE[e3 / 3 + 4]);
    if (unidad) buf_str(&b, unidad);
    return buf;
}

/* Cuatro cifras significativas sin prefijo (una razón, una Q, |H| en V/V):
 * "0,7071", "10,00"; fuera de [1e-3, 1e4), con prefijo: "12,35k" */
static char *sig4(char *buf, size_t cap, double v)
{
    double a = fabs(v);
    int dec;
    if (a != a) { Buf b; buf_init(&b, buf, cap); buf_str(&b, "?"); return buf; }
    if (a == 0.0) return num_txt(buf, cap, 0.0, 0);
    if (a < 1e-3 || a >= 1e4) return fija(buf, cap, v, 4, "");
    dec = 3 - (int)floor(log10(a));
    if (floor(a * pow(10.0, (double)dec) + 0.5) >= 1e4) dec--;     /* 0,99999 es 1,000 */
    if (dec < 0) return fija(buf, cap, v, 4, "");                    /* 9999,99 es 10,00k */
    return num_txt(buf, cap, v, dec);
}

/* ==========================================================================
 *  2. La pantalla
 * ======================================================================= */
static void cabecera(const BodeReg *g, const BodeRes *r)
{
    char t[48], u[16];
    Buf b;
    gfx_fill(0, 0, SCR_W, 15, C_PANEL);
    buf_init(&b, t, sizeof t);
    buf_str(&b, "Bode "); buf_str(&b, r->nom_sal); buf_ch(&b, '/'); buf_str(&b, r->nom_ent);
    gfx_text(4, 3, t, C_TEXTO, 1);
    if (hay_malla(g)) {
        buf_init(&b, t, sizeof t);
        buf_str(&b, num_pref(u, sizeof u, g->f[0], 3, "Hz"));
        buf_str(&b, " a ");
        buf_str(&b, num_pref(u, sizeof u, g->f[g->n - 1], 3, "Hz"));
        gfx_text(150, 3, t, C_SUAVE, 1);
    }
}

static void punteada_h(int y, u16 c)
{
    int x;
    for (x = BD_X0; x <= BD_X1; x += 3) gfx_px(x, y, c);
}

static void punteada_v(int x, int paso, u16 c)
{
    int y;
    for (y = BD_MY0; y <= BD_MY1; y += paso) gfx_px(x, y, c);
    for (y = BD_FY0; y <= BD_FY1; y += paso) gfx_px(x, y, c);
}

/* Décadas del eje: de d0 a d1 (a lo sumo 40) */
static void decadas(const Trazador *z, int *d0, int *d1)
{
    double a = floor(z->lf0 + 1e-9), b = floor(z->lf1 + 1e-9);
    if (a < -20.0) a = -20.0;
    if (b > 20.0) b = 20.0;
    if (b < a) b = a;
    *d0 = (int)a; *d1 = (int)b;
}

static int en_ventana(const Trazador *z, double lf)
{
    double tol = 1e-9 * ancho(z);
    return lf >= z->lf0 - tol && lf <= z->lf1 + tol;
}

static void reticula(const Trazador *z)
{
    int i, d, d0, d1, m, fino = z->lf1 - z->lf0 <= 1.5;
    for (i = 0; i <= 4; i++) { punteada_h(BD_MY0 + i * BD_MDY, C_REJILLA); punteada_h(BD_FY0 + i * BD_FDY, C_REJILLA); }
    punteada_v(BD_X0, 3, C_REJILLA);
    punteada_v(BD_X1, 3, C_REJILLA);
    decadas(z, &d0, &d1);
    for (d = d0; d <= d1; d++)
        for (m = 1; m <= 9; m++) {
            double lf = (double)d + log10((double)m);
            int x;
            if (!en_ventana(z, lf)) continue;
            x = traz_x(z, pow(10.0, lf));
            if (m == 1) { punteada_v(x, 3, C_REJILLA); continue; }
            /* marcas de 2 a 9 abajo de cada gráfica; de cerca, también la línea */
            if (fino) punteada_v(x, 6, C_REJILLA);
            gfx_vline(x, BD_MY1 - 2, 3, C_TENUE);
            gfx_vline(x, BD_FY1 - 2, 3, C_TENUE);
        }
}

/* Un rótulo de 6×8 alineado a la derecha en x = 35 */
static void rotulo_y(const char *t, int y)
{
    int x = 36 - gfx_text_w(t, 1);
    gfx_text_p(x > 0 ? x : 0, y, t, C_SUAVE);
}

/* Rótulos de las divisiones: los dB, con la unidad («dB», «dBS», «dBΩ»)
 * entre los dos primeros; los grados, cada uno con su «°» (un «°» solo, en
 * la esquina, no se ve con 3 px) */
static void rotulos_y(const Trazador *z, const BodeRes *r)
{
    char t[24];
    Buf b;
    int i, y;
    for (i = 0; i <= 4; i++) {
        num_auto(t, sizeof t, ((double)z->db_top - i) * dbdiv(z), 0);
        y = BD_MY0 + i * BD_MDY - 3;
        if (y < 15) y = 15;                          /* bajo la cabecera */
        if (y > BD_MY1 - 6) y = BD_MY1 - 6;          /* sobre la fase    */
        rotulo_y(t, y);
        num_auto(t, sizeof t, ((double)z->fase_top - i) * fdiv(z), 0);
        buf_init(&b, t + strlen(t), sizeof t - strlen(t));
        buf_str(&b, S_DEG);
        y = BD_FY0 + i * BD_FDY - 3;
        if (y < BD_MY1 + 3) y = BD_MY1 + 3;
        rotulo_y(t, y);
    }
    buf_init(&b, t, sizeof t);
    buf_str(&b, "dB"); buf_str(&b, r->unidad);
    gfx_text_p(36 - gfx_text_w(t, 1), BD_MY0 + BD_MDY / 2 - 3, t, C_TITULO);
}

/* La referencia de -3 dB del tipo: la de M0, Minf, el pico o √(M0·Minf) */
static int ref_db(const BodeRes *r, double *ref)
{
    const BodeMedidas *m = &r->m;
    double v = 0.0;
    if (r->fin != FIN_COMPLETO) return 0;
    switch (m->tipo) {
    case BODE_PB: v = m->M0; break;
    case BODE_PA: v = m->Minf; break;
    case BODE_PBANDA: if (m->f_pico > 0.0) { *ref = m->db_pico; return 1; } return 0;
    case BODE_RB: v = sqrt(m->M0 * m->Minf); break;
    default: return 0;
    }
    if (!(v > 0.0) || !(v < 1e300)) return 0;
    *ref = 20.0 * log10(v);
    return 1;
}

/* La fila de cada columna de las dos curvas (pow y traz_valor por columna
 * eran casi todo el cuadro), guardada mientras no cambien el barrido (el
 * registro, su gen y sus puntos), la ventana ni la escala: el cuadro
 * siguiente (el cursor que se mueve, una marca, el panel) solo la pinta.
 * Solo de un barrido acabado (fin, y el registro ya no cambia hasta otro
 * gen: las evaluaciones no sirven de clave, porque las lecturas exactas
 * de los cursores, bode_en, también las cuentan) y de gen distinto de 0
 * (uno de gen 0 no sale de bode.c: el arnés puede cambiarlo sin que
 * cambie nada de eso) */
typedef struct {
    unsigned char valida, ok[BD_W + 1];        /* ok[c] = 0: un hueco        */
    const BodeReg *g;
    unsigned gen;
    int n;
    double lf0, lf1;
    signed char iesc, tope;                    /* idb y db_top, o ifase y fase_top */
    short y[BD_W + 1];
} Filas;
static Filas filas_[2];                        /* la magnitud y la fase      */
static int filas_hechas;

int traz_filas_hechas(void) { return filas_hechas; }

static void curva(const Trazador *z, const BodeReg *g, const BodeRes *r, int fase)
{
    int c, xa = 0, ya = 0, hay = 0;
    u16 col = fase ? C_CANAL2 : C_CANAL1;
    signed char iesc = fase ? z->ifase : z->idb, tope = fase ? z->fase_top : z->db_top;
    Filas *F = &filas_[fase];
    if (!(F->valida && F->g == g && F->gen == r->gen && F->n == g->n &&
          F->lf0 == z->lf0 && F->lf1 == z->lf1 && F->iesc == iesc && F->tope == tope)) {
        for (c = 0; c <= BD_W; c++) {
            double lf = z->lf0 + (z->lf1 - z->lf0) * c / BD_W, v = traz_valor(g, pow(10.0, lf), fase);
            F->ok[c] = (unsigned char)(v == v);
            if (v == v) F->y[c] = (short)(fase ? traz_y_fase(z, v) : traz_y_db(z, v));
        }
        F->valida = (unsigned char)(r->gen != 0 && r->fin != FIN_NO);
        F->g = g; F->gen = r->gen; F->n = g->n;
        F->lf0 = z->lf0; F->lf1 = z->lf1; F->iesc = iesc; F->tope = tope;
        filas_hechas++;
    }
    if (fase) gfx_clip(BD_X0, BD_FY0, BD_W + 1, 4 * BD_FDY + 1);
    else gfx_clip(BD_X0, BD_MY0, BD_W + 1, 4 * BD_MDY + 1);
    for (c = 0; c <= BD_W; c++) {
        int x = BD_X0 + c, y;
        if (!F->ok[c]) { hay = 0; continue; }       /* hueco */
        y = F->y[c];
        if (hay) gfx_line(xa, ya, x, y, col);
        else gfx_px(x, y, col);
        xa = x; ya = y; hay = 1;
    }
    gfx_clip_reset();
}

/* Un triángulo que apunta al punto (x, y) de la curva: desde arriba (▼) o
 * desde abajo (▲), siempre dentro de la gráfica de la magnitud */
static void triangulo(int x, int y, int desde_arriba)
{
    if (desde_arriba) {
        if (y - 7 < BD_MY0) y = BD_MY0 + 7;
        gfx_tri(x - 3, y - 7, x + 3, y - 7, x, y - 2, C_CURSOR);
    } else {
        if (y + 7 > BD_MY1) y = BD_MY1 - 7;
        gfx_tri(x - 3, y + 7, x + 3, y + 7, x, y + 2, C_CURSOR);
    }
}

/* La marca en f, si se ve y no pisa la del pico (en x_pico) */
static void marca_en(const Trazador *z, const BodeReg *g, double f, int desde_arriba, int x_pico)
{
    double v;
    int x;
    if (!(f > 0.0)) return;
    x = traz_x(z, f);
    if (x < BD_X0 || x > BD_X1) return;
    if (x - x_pico < 7 && x_pico - x < 7) return;    /* se juntarían: de cerca se separan */
    v = traz_valor(g, f, 0);
    if (v != v) return;
    triangulo(x, traz_y_db(z, v), desde_arriba);
}

/* La referencia de -3 dB, en discontinuo (debajo de las curvas) */
static void referencia(const Trazador *z, const BodeRes *r)
{
    double ref;
    if (ref_db(r, &ref)) {
        int y = traz_y_db(z, ref - DB_3);
        if (y >= BD_MY0 && y <= BD_MY1) gfx_line_dash(BD_X0, y, BD_X1, y, C_TENUE, 4, 3);
    }
}

/* ▼ en cada corte (y en el valle) y ▲ en el pico */
static void marcas(const Trazador *z, const BodeReg *g, const BodeRes *r)
{
    const BodeMedidas *m = &r->m;
    int xp = -100;
    if (r->fin != FIN_COMPLETO) return;
    gfx_clip(BD_X0 - 3, BD_MY0, BD_W + 7, 4 * BD_MDY + 1);
    if (m->f_pico > 0.0) {
        marca_en(z, g, m->f_pico, 0, -100);
        xp = traz_x(z, m->f_pico);
    }
    if (m->tipo == BODE_PB || m->tipo == BODE_PA) marca_en(z, g, m->fc, 1, xp);
    if (m->tipo == BODE_PBANDA || m->tipo == BODE_RB) { marca_en(z, g, m->f1, 1, xp); marca_en(z, g, m->f2, 1, xp); }
    marca_en(z, g, m->f_valle, 1, xp);
    gfx_clip_reset();
}

static void cursores(const Trazador *z, const BodeReg *g)
{
    int q;
    for (q = 0; q < 2; q++) {
        int k = q == 0 ? 1 - z->activo : z->activo, x, j;   /* el activo, encima */
        u16 col = k == z->activo ? C_TEXTO : C_SUAVE;
        char t[4], u[4];
        Buf b;
        if (k == 1 && !z->dos) continue;
        x = traz_x(z, z->fcur[k]);
        t[0] = 0;
        if (z->dos) { t[0] = (char)('1' + k); t[1] = 0; }
        if (x < BD_X0 || x > BD_X1) {                /* fuera de la ventana: una flecha */
            buf_init(&b, u, sizeof u);
            if (x < BD_X0) { buf_str(&b, S_LARR); buf_str(&b, t); gfx_text_p_bg(BD_X0 + 2, BD_MY1 - 9 - 9 * k, u, col, C_FONDO); }
            else { buf_str(&b, t); buf_str(&b, S_RARR); gfx_text_p_bg(BD_X1 - 1 - gfx_text_w(u, 1), BD_MY1 - 9 - 9 * k, u, col, C_FONDO); }
            continue;
        }
        gfx_line_dash(x, BD_MY0, x, BD_FY1, col, 2, 2);
        for (j = 0; j < 2; j++) {
            double v = traz_valor(g, z->fcur[k], j);
            int y, y0 = j ? BD_FY0 : BD_MY0, y1 = j ? BD_FY1 : BD_MY1;
            if (v != v) continue;
            y = j ? traz_y_fase(z, v) : traz_y_db(z, v);
            if (y >= y0 && y <= y1) gfx_rect(x - 1, y - 1, 3, 3, col);
        }
        if (t[0]) gfx_text_p_bg(x <= BD_X1 - 9 ? x + 2 : x - 7, BD_MY0 + 2, t, col, C_FONDO);
    }
}

/* Rótulos del eje de f (6×8, y = 164): las décadas; si se ven menos de dos,
 * también de 2 a 9; si aun así, los bordes de la ventana. Ninguno a menos de
 * 24 px de otro, ni pegado a otro (el de un borde va alineado a ese borde). */
static int rot_x[64], rot_a[64], rot_b[64], n_rot;

static int rotula(int x, double f, int alinea)   /* alinea: 0 centrado, 1 izquierda, 2 derecha */
{
    char t[16];
    int i, w, xl;
    if (n_rot >= 64) return 0;
    num_pref(t, sizeof t, f, 3, "");
    w = gfx_text_w(t, 1);
    xl = alinea == 1 ? x : alinea == 2 ? x - w + 1 : x - w / 2;
    if (xl < 37) xl = 37;                             /* a la derecha de los rótulos de y */
    if (xl + w > SCR_W) xl = SCR_W - w;
    for (i = 0; i < n_rot; i++) {
        if (x - rot_x[i] < 24 && rot_x[i] - x < 24) return 0;
        if (xl < rot_b[i] + 6 && xl + w + 6 > rot_a[i]) return 0;
    }
    gfx_text_p(xl, BD_EJE_Y, t, C_SUAVE);
    rot_x[n_rot] = x; rot_a[n_rot] = xl; rot_b[n_rot] = xl + w;
    n_rot++;
    return 1;
}

static void eje(const Trazador *z)
{
    int d, d0, d1, m, pasada, n = 0;
    n_rot = 0;
    decadas(z, &d0, &d1);
    for (pasada = 0; pasada < 2; pasada++) {
        if (pasada == 1 && n >= 2) break;
        for (d = d0; d <= d1; d++)
            for (m = pasada ? 2 : 1; m <= (pasada ? 9 : 1); m++) {
                double lf = (double)d + log10((double)m);
                if (en_ventana(z, lf)) n += rotula(traz_x(z, pow(10.0, lf)), pow(10.0, lf), 0);
            }
    }
    if (n < 2) {
        rotula(BD_X0, pow(10.0, z->lf0), 1);
        rotula(BD_X1, pow(10.0, z->lf1), 2);
    }
}

/* «f 1,000kHz  |H| -3,01dB = 0,7071  φ -45,0°» (pre: "f", "→f1" o " f2").
 * aprox: mientras corre una ráfaga, de la malla y con «≈» (la exacta, al
 * soltar: bode_en cuesta una factorización por lectura) */
static void txt_lectura(char *t, size_t cap, const BodeReg *g, const BodeRes *r, double f, const char *pre, int aprox)
{
    double db, mod, fa;
    char u[24];
    Buf b;
    buf_init(&b, t, cap);
    buf_str(&b, pre); buf_ch(&b, ' ');
    buf_str(&b, fija(u, sizeof u, f, 4, "Hz"));
    if (aprox) {
        db = traz_valor(g, f, 0); fa = traz_valor(g, f, 1);
        if (db != db) { buf_str(&b, "  |H| " S_APROX " ?"); return; }         /* un hueco de la malla */
        buf_str(&b, "  |H| " S_APROX " "); buf_str(&b, num_txt(u, sizeof u, db, 2)); buf_str(&b, "dB"); buf_str(&b, r->unidad);
        if (fa == fa) { buf_str(&b, "  " S_PHI " " S_APROX " "); buf_str(&b, num_txt(u, sizeof u, fa, 1)); buf_str(&b, S_DEG); }
        return;
    }
    if (!traz_lee(g, f, &db, &mod, &fa)) { buf_str(&b, "  singular: resonancia ideal justo aquí"); return; }
    if (!(mod > 0.0)) { buf_str(&b, "  |H| = 0 (-" S_INF " dB)"); return; }
    buf_str(&b, "  |H| "); buf_str(&b, num_txt(u, sizeof u, db, 2)); buf_str(&b, "dB"); buf_str(&b, r->unidad);
    buf_str(&b, " = ");
    buf_str(&b, r->unidad[0] ? fija(u, sizeof u, mod, 4, r->unidad) : sig4(u, sizeof u, mod));
    buf_str(&b, "  " S_PHI " "); buf_str(&b, num_txt(u, sizeof u, fa, 1)); buf_str(&b, S_DEG);
}

/* «Δ f2/f1 = 10,00  ΔdB -20,00  Δφ -44,4°» */
static void txt_delta(char *t, size_t cap, const BodeReg *g, const Trazador *z)
{
    double db[2], mod[2], fa[2];
    int ok = 1, k;
    const char *ap = z->rafaga ? S_APROX " " : "";      /* de la malla mientras corre una ráfaga */
    char u[24];
    Buf b;
    for (k = 0; k < 2; k++) {
        if (z->rafaga) { db[k] = traz_valor(g, z->fcur[k], 0); fa[k] = traz_valor(g, z->fcur[k], 1); ok &= db[k] == db[k] && fa[k] == fa[k]; }
        else ok &= traz_lee(g, z->fcur[k], &db[k], &mod[k], &fa[k]) && mod[k] > 0.0;
    }
    buf_init(&b, t, cap);
    buf_str(&b, S_DELTA " f2/f1 = "); buf_str(&b, sig4(u, sizeof u, z->fcur[1] / z->fcur[0]));
    if (!ok) { buf_str(&b, z->rafaga ? "  (un hueco de la malla)" : "  (H nula o singular)"); return; }
    buf_str(&b, "  " S_DELTA "dB "); buf_str(&b, ap); buf_str(&b, num_txt(u, sizeof u, db[1] - db[0], 2));
    buf_str(&b, "  " S_DELTA S_PHI " "); buf_str(&b, ap); buf_str(&b, num_txt(u, sizeof u, fa[1] - fa[0], 1)); buf_str(&b, S_DEG);
}

/* Línea 2: el tipo */
static void txt_tipo(char *t, size_t cap, const BodeRes *r)
{
    const BodeMedidas *m = &r->m;
    char u[24];
    Buf b;
    double ref;
    buf_init(&b, t, cap);
    if (r->fin == FIN_CORTADO) { buf_str(&b, "Cortado: sin medidas. [=] barre lo que se ve."); return; }
    if (r->fin != FIN_COMPLETO) { buf_str(&b, "Sin barrido."); return; }
    switch (m->tipo) {
    case BODE_PB: case BODE_PA:
        buf_str(&b, m->tipo == BODE_PB ? "Pasa-bajas" : "Pasa-altas");
        if (!(m->fc > 0.0)) { buf_str(&b, "  (el corte cae fuera del barrido)"); break; }
        buf_str(&b, "  fc "); buf_str(&b, fija(u, sizeof u, m->fc, 4, "Hz"));
        if (ref_db(r, &ref)) {
            buf_str(&b, "  (-3 dB de "); buf_str(&b, num_txt(u, sizeof u, ref, 2));
            buf_str(&b, " dB"); buf_str(&b, r->unidad); buf_ch(&b, ')');
        }
        break;
    case BODE_PBANDA:
        buf_str(&b, "Pasa-banda  f0 "); buf_str(&b, fija(u, sizeof u, m->f_pico, 4, "Hz"));
        buf_str(&b, "  pico "); buf_str(&b, num_txt(u, sizeof u, m->db_pico, 2)); buf_str(&b, "dB"); buf_str(&b, r->unidad);
        break;
    case BODE_RB:
        buf_str(&b, "Rechaza-banda  f0 "); buf_str(&b, fija(u, sizeof u, m->f_valle, 4, "Hz"));
        break;
    case BODE_PLANO: buf_str(&b, "Plano"); break;
    case BODE_NULA: buf_str(&b, "Salida nula: no depende de la entrada"); break;
    default: buf_str(&b, "Otra forma (ni pasa-bajas, pasa-altas ni banda)"); break;
    }
}

/* Línea 3: los detalles */
static void txt_detalle(char *t, size_t cap, const BodeReg *g, const BodeRes *r)
{
    const BodeMedidas *m = &r->m;
    char u[24];
    Buf b;
    buf_init(&b, t, cap);
    if (r->fin != FIN_COMPLETO) return;
    if (m->tipo == BODE_PBANDA || m->tipo == BODE_RB) {
        if (!(m->f1 > 0.0 && m->f2 > 0.0)) { buf_str(&b, "Los cortes a -3 dB caen fuera del barrido"); return; }
        buf_str(&b, "fc1 "); buf_str(&b, fija(u, sizeof u, m->f1, 4, "Hz"));
        buf_str(&b, "  fc2 "); buf_str(&b, fija(u, sizeof u, m->f2, 4, "Hz"));
        buf_str(&b, "  BW "); buf_str(&b, fija(u, sizeof u, m->bw, 4, "Hz"));
        buf_str(&b, "  Q "); buf_str(&b, sig4(u, sizeof u, m->q));
        return;
    }
    if ((m->tipo == BODE_PB || m->tipo == BODE_PA) && m->f_pico > 0.0) {
        buf_str(&b, "Pico "); buf_str(&b, fija(u, sizeof u, m->f_pico, 4, "Hz"));
        buf_str(&b, " a "); buf_str(&b, num_txt(u, sizeof u, m->db_pico, 2)); buf_str(&b, "dB"); buf_str(&b, r->unidad);
        return;
    }
    if (m->tipo == BODE_PLANO) {
        double lo = 1e300, hi = -1e300;
        int k;
        for (k = 0; k < g->n; k++) {
            if (!g->ok[k]) continue;
            if (g->db[k] < lo) lo = g->db[k];
            if (g->db[k] > hi) hi = g->db[k];
        }
        if (hi < lo) return;
        buf_str(&b, "|H| ");
        if (hi - lo < 0.005) { buf_str(&b, "= "); buf_str(&b, num_txt(u, sizeof u, hi, 2)); }
        else {
            buf_str(&b, "entre "); buf_str(&b, num_txt(u, sizeof u, lo, 2));
            buf_str(&b, " y "); buf_str(&b, num_txt(u, sizeof u, hi, 2));
        }
        buf_str(&b, " dB"); buf_str(&b, r->unidad); buf_str(&b, " en todo el barrido");
        return;
    }
    buf_str(&b, "Arriba: "); buf_str(&b, num_txt(u, sizeof u, m->pend_fin, 1));
    buf_str(&b, " dB/déc   abajo: "); buf_str(&b, num_txt(u, sizeof u, m->pend_ini, 1)); buf_str(&b, " dB/déc");
}

/* Línea 4 sin aviso: «Entrada E1 [F]   Salida V(C1) [C]   [X] 2 cursores»,
 * más apretada si los nombres son largos */
static void txt_io(char *t, size_t cap, const Trazador *z, const BodeRes *r)
{
    int intento;
    Buf b;
    for (intento = 0; intento < 3; intento++) {
        const char *sep = intento ? " " : "   ";
        buf_init(&b, t, cap);
        buf_str(&b, "Entrada "); buf_str(&b, r->nom_ent); buf_str(&b, " [F]"); buf_str(&b, sep);
        buf_str(&b, "Salida "); buf_str(&b, r->nom_sal); buf_str(&b, " [C]");
        if (intento < 2) { buf_str(&b, sep); buf_str(&b, z->dos ? "[X] 1 cursor" : "[X] 2 cursores"); }
        if (4 + gfx_text_w(t, 1) <= SCR_W - 2) return;
    }
}

static void panel(const Trazador *z, const BodeReg *g, const BodeRes *r)
{
    char t[96], u[24];
    Buf b;
    int y = BD_PANEL_Y;
    gfx_fill(0, BD_PANEL_Y - 2, SCR_W, 226 - (BD_PANEL_Y - 2), C_PANEL);
    /* 1: la lectura, lo que se escribe o, con dos cursores, la diferencia */
    if (z->entrada[0]) {
        int ok;
        double v = num_lee_ing(z->entrada, &ok);
        buf_init(&b, t, sizeof t);
        buf_str(&b, S_RARR "f");
        if (z->dos) buf_ch(&b, (char)('1' + z->activo));
        buf_str(&b, " = "); buf_str(&b, z->entrada); buf_ch(&b, '_');
        if (ok && v > 0.0) { buf_str(&b, "  (= "); buf_str(&b, fija(u, sizeof u, v, 4, " Hz")); buf_ch(&b, ')'); }
        gfx_text(4, y, t, C_CURSOR, 1);
    } else if (hay_malla(g)) {
        if (z->dos) txt_delta(t, sizeof t, g, z);
        else txt_lectura(t, sizeof t, g, r, z->fcur[0], "f", z->rafaga);
        gfx_text(4, y, t, C_TEXTO, 1);
    }
    /* 2 y 3: el tipo y sus detalles; con dos cursores, sus lecturas */
    if (z->dos && hay_malla(g)) {
        int k;
        for (k = 0; k < 2; k++) {
            txt_lectura(t, sizeof t, g, r, z->fcur[k], k == z->activo ? (k ? S_RARR "f2" : S_RARR "f1") : (k ? " f2" : " f1"), z->rafaga);
            gfx_text(4, y + 13 + 13 * k, t, k == z->activo ? C_TEXTO : C_SUAVE, 1);
        }
    } else {
        txt_tipo(t, sizeof t, r);
        gfx_text(4, y + 13, t, C_TITULO, 1);
        txt_detalle(t, sizeof t, g, r);
        if (t[0]) gfx_text(4, y + 26, t, C_TEXTO, 1);
    }
    /* 4: el aviso, o la entrada y la salida */
    if (z->aviso[0]) gfx_text(4, y + 39, z->aviso, z->aviso_col, 1);
    else { txt_io(t, sizeof t, z, r); gfx_text(4, y + 39, t, C_SUAVE, 1); }
}

void traz_dibuja(Trazador *z, const BodeReg *g, const BodeRes *r)
{
    gfx_clear(C_FONDO);
    cabecera(g, r);
    if (hay_malla(g)) {
        reticula(z);
        rotulos_y(z, r);
        referencia(z, r);
        curva(z, g, r, 1);
        curva(z, g, r, 0);
        marcas(z, g, r);
        cursores(z, g);
        eje(z);
    } else gfx_text(BD_X0 + 20, BD_MY0 + 60, "Sin puntos: [=] barre de nuevo.", C_SUAVE, 1);
    panel(z, g, r);
    gfx_fill(0, 226, SCR_W, 14, C_PANEL);
    gfx_text(4, 227, "[izq/der] cursor [+-] zoom [M] marcas [tab] fasores", C_SUAVE, 1);
}

/* ==========================================================================
 *  3. Teclas
 * ======================================================================= */
static void avisa(Trazador *z, const char *txt, u16 col)
{
    Buf b;
    buf_init(&b, z->aviso, sizeof z->aviso);
    buf_str(&b, txt);
    z->aviso_col = col;
}

/* «fc1: -3,01 dB»: el nombre y la lectura exacta en f */
static void avisa_en(Trazador *z, const BodeReg *g, const BodeRes *r, const char *nom, double f)
{
    char t[64], u[24];
    double db, mod, fa;
    Buf b;
    buf_init(&b, t, sizeof t);
    buf_str(&b, nom); buf_str(&b, ": ");
    if (!traz_lee(g, f, &db, &mod, &fa)) buf_str(&b, "singular (resonancia ideal)");
    else if (!(mod > 0.0)) buf_str(&b, "|H| = 0");
    else { buf_str(&b, num_txt(u, sizeof u, db, 2)); buf_str(&b, " dB"); buf_str(&b, r->unidad); }
    avisa(z, t, C_CURSOR);
}

static void inicio(Trazador *z, const BodeReg *g)
{
    z->lf0 = lg_ini(g); z->lf1 = lg_fin(g);
}

/* La ventana dentro del barrido, con su ancho */
static void encaja(Trazador *z, const BodeReg *g)
{
    double L0 = lg_ini(g), L1 = lg_fin(g), w = z->lf1 - z->lf0;
    if (!(w > 0.0) || w >= (L1 - L0) * (1.0 - 1e-12)) { inicio(z, g); return; }
    if (z->lf0 < L0) { z->lf0 = L0; z->lf1 = L0 + w; }
    if (z->lf1 > L1) { z->lf1 = L1; z->lf0 = L1 - w; }
}

/* Que se vea log f: centrada en ella, o lo justo para que entre */
static void que_se_vea(Trazador *z, const BodeReg *g, double lf, int centra)
{
    double w = z->lf1 - z->lf0;
    if (lf >= z->lf0 && lf <= z->lf1) return;
    if (centra) { z->lf0 = lf - 0.5 * w; z->lf1 = lf + 0.5 * w; }
    else if (lf < z->lf0) { z->lf0 = lf; z->lf1 = lf + w; }
    else { z->lf1 = lf; z->lf0 = lf - w; }
    encaja(z, g);
}

static void lleva(Trazador *z, const BodeReg *g, double f, int centra)
{
    z->fcur[z->activo] = f;
    que_se_vea(z, g, log10(f), centra);
}

static void escribe(Trazador *z, char c)
{
    size_t n = strlen(z->entrada);
    if (n + 1 < sizeof z->entrada) { z->entrada[n] = c; z->entrada[n + 1] = 0; }
}

static void tecla_entrada(Trazador *z, const BodeReg *g, int t, int sh)
{
    size_t n = strlen(z->entrada);
    char ult = n ? z->entrada[n - 1] : 0;
    if (t >= T_DIG(0) && t <= T_DIG(9)) { escribe(z, (char)('0' + (t - T_DIG0))); return; }
    switch (t) {
    case T_PUNTO: escribe(z, '.'); return;
    case T_EE: escribe(z, 'E'); return;
    case T_SIGNO: case T_MENOS: if (ult == 'E') escribe(z, '-'); return;
    case T_BORRAR: if (n) z->entrada[n - 1] = 0; return;
    case T_ESC: z->entrada[0] = 0; return;
    case T_ENTER: {
        int ok;
        double v = num_lee_ing(z->entrada, &ok);
        z->entrada[0] = 0;
        if (!ok || !(v > 0.0)) { avisa(z, "Eso no es una frecuencia.", C_ERROR); return; }
        if (!hay_malla(g) || v < g->f[0] * (1.0 - TOL_REL) || v > g->f[g->n - 1] * (1.0 + TOL_REL)) {
            avisa(z, "Fuera del barrido: [menu] cambia el rango.", C_ERROR);
            return;
        }
        if (v < g->f[0]) v = g->f[0];
        if (v > g->f[g->n - 1]) v = g->f[g->n - 1];
        lleva(z, g, v, 1);
        return;
    }
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

/* El punto de la malla siguiente (sg = +1) o anterior (-1) a f; -1 si no hay */
static int k_vecino(const BodeReg *g, double f, int sg)
{
    int k;
    if (sg > 0) { for (k = 0; k < g->n; k++) if (g->f[k] > f * (1.0 + TOL_REL)) return k; }
    else { for (k = g->n - 1; k >= 0; k--) if (g->f[k] < f * (1.0 - TOL_REL)) return k; }
    return -1;
}

/* El máximo (o el mínimo) de la malla: [P] sin pico interior */
static int k_extremo(const BodeReg *g, int maximo)
{
    int k, kb = -1;
    for (k = 0; k < g->n; k++) {
        if (!g->ok[k]) continue;
        if (kb < 0 || (maximo ? g->db[k] > g->db[kb] : g->db[k] < g->db[kb])) kb = k;
    }
    return kb;
}

void traz_nuevo(Trazador *z, const BodeReg *g, const BodeRes *r)
{
    signed char idb = z->idb, top = z->db_top;
    int k;
    z->entrada[0] = 0;
    z->aviso[0] = 0;
    z->aviso_col = C_CURSOR;
    z->imarca = 0;
    if (hay_malla(g)) inicio(z, g);
    else if (r && r->f_ini > 0.0 && r->f_fin > r->f_ini) { z->lf0 = log10(r->f_ini); z->lf1 = log10(r->f_fin); }
    else { z->lf0 = 1.0; z->lf1 = 4.0; }
    for (k = 0; k < 2; k++)
        if (!hay_malla(g) || !(z->fcur[k] >= g->f[0]) || !(z->fcur[k] <= g->f[g->n - 1]))
            z->fcur[k] = pow(10.0, z->lf0 + (k ? 0.75 : 0.5) * (z->lf1 - z->lf0));
    if (z->dos > 1) z->dos = 1;
    if (!z->dos || z->activo > 1) z->activo = 0;
    traz_autoescala(z, g);
    if (z->ajustado && idb >= 0 && idb < N_DBDIV) { z->idb = idb; z->db_top = top; }
}

int traz_tecla(Trazador *z, const BodeReg *g, const BodeRes *r, int tk)
{
    int sh = (tk & T_CON_SHIFT) != 0, t = tk & ~T_MODS;

    z->aviso[0] = 0;
    /* 1. escribiendo una f (una cifra o el punto empiezan) */
    if (z->entrada[0]) { tecla_entrada(z, g, t, sh); return TZ_NADA; }
    if ((t >= T_DIG(0) && t <= T_DIG(9)) || t == T_PUNTO) {
        escribe(z, t == T_PUNTO ? '.' : (char)('0' + (t - T_DIG0)));
        return TZ_NADA;
    }
    if (hay_malla(g)) {
        /* 2. el cursor: un punto de la malla; con shift, una década */
        if (t == T_IZQ || t == T_DER) {
            int sg = t == T_DER ? 1 : -1;
            double f = z->fcur[z->activo];
            if (tk & T_CON_CTRL) {
                /* [ctrl]: al borde de la ventana (su último punto de la malla);
                 * si ya está ahí, al extremo del barrido */
                double fb = pow(10.0, sg > 0 ? z->lf1 : z->lf0);
                int k;
                if (sg > 0) { for (k = g->n - 1; k > 0 && g->f[k] > fb * (1.0 + TOL_REL); k--) {} }
                else { for (k = 0; k < g->n - 1 && g->f[k] < fb * (1.0 - TOL_REL); k++) {} }
                if (fabs(g->f[k] - f) <= TOL_REL * f) k = sg > 0 ? g->n - 1 : 0;
                lleva(z, g, g->f[k], 0);
                return TZ_NADA;
            }
            if (sh) {
                f = sg > 0 ? f * 10.0 : f / 10.0;
                if (f < g->f[0]) f = g->f[0];
                if (f > g->f[g->n - 1]) f = g->f[g->n - 1];
            } else {
                int k = k_vecino(g, f, sg);
                if (k < 0) return TZ_NADA;               /* en el borde */
                f = g->f[k];
            }
            lleva(z, g, f, 0);
            return TZ_NADA;
        }
        /* 3. zoom en log f alrededor del cursor, dentro del barrido */
        if (t == T_MAS || t == T_MENOS) {
            double L0 = lg_ini(g), L1 = lg_fin(g), w = z->lf1 - z->lf0, wmin = traz_ancho_min(g);
            double c = log10(z->fcur[z->activo]), nw = t == T_MAS ? 0.5 * w : 2.0 * w;
            if (!(c >= L0 && c <= L1)) c = 0.5 * (z->lf0 + z->lf1);
            if (nw < wmin) nw = wmin;
            if (nw >= (L1 - L0) * (1.0 - 1e-12)) inicio(z, g);
            else { z->lf0 = c - 0.5 * nw; z->lf1 = c + 0.5 * nw; encaja(z, g); }
            if (traz_en_vista(z, g) < MIN_VISTA) avisa(z, "Pocas muestras aquí: [=] afina lo que se ve.", C_CURSOR);
            return TZ_NADA;
        }
        /* 4. dB por división (con shift, la curva sube o baja una división) */
        if (t == T_ARRIBA || t == T_ABAJO) {
            if (sh) {
                int top = z->db_top + (t == T_ARRIBA ? -1 : 1);
                if (top >= -TOPE_DIV && top <= TOPE_DIV) z->db_top = (signed char)top;
            } else {
                int i = z->idb + (t == T_ARRIBA ? -1 : 1);
                if (i >= 0 && i < N_DBDIV) {
                    /* el centro de la gráfica se queda donde estaba */
                    double centro = ((double)z->db_top - 2.0) * dbdiv(z), top = floor(centro / DBDIV[i] + 2.5);
                    if (top > TOPE_DIV) top = TOPE_DIV;
                    if (top < -TOPE_DIV) top = -TOPE_DIV;
                    z->idb = (signed char)i;
                    z->db_top = (signed char)top;
                }
            }
            z->ajustado = 1;
            return TZ_NADA;
        }
        /* 5. las marcas del tipo, una tras otra */
        if (t == T_LETRA('M')) {
            double f[MAX_MARCAS];
            const char *nom[MAX_MARCAS];
            int n = traz_marcas(r, f, nom, MAX_MARCAS), i;
            if (!n) { avisa(z, "Sin marcas: ni cortes, ni pico, ni valle.", C_CURSOR); return TZ_NADA; }
            i = z->imarca % n;
            lleva(z, g, f[i], 1);
            avisa_en(z, g, r, nom[i], f[i]);
            z->imarca = (unsigned char)((i + 1) % n);
            return TZ_NADA;
        }
        /* 6. el pico; con shift, el valle */
        if (t == T_LETRA('P')) {
            double fp = r->fin == FIN_COMPLETO ? (sh ? r->m.f_valle : r->m.f_pico) : 0.0;
            if (fp > 0.0) { lleva(z, g, fp, 1); avisa_en(z, g, r, sh ? "valle" : "pico", fp); }
            else {
                int k = k_extremo(g, !sh);
                if (k < 0) return TZ_NADA;
                lleva(z, g, g->f[k], 1);
                avisa_en(z, g, r, sh ? "Sin valle; el mínimo" : "Sin pico; el máximo", g->f[k]);
            }
            return TZ_NADA;
        }
        /* 7. el segundo cursor y cuál se mueve */
        if (t == T_LETRA('X')) {
            if (z->dos) { z->dos = 0; z->activo = 0; return TZ_NADA; }
            if (!(z->fcur[1] >= pow(10.0, z->lf0) && z->fcur[1] <= pow(10.0, z->lf1)) || z->fcur[1] == z->fcur[0]) {
                double l1 = log10(z->fcur[0]), w = z->lf1 - z->lf0, l2 = l1 + 0.25 * w;
                if (l2 > z->lf1) l2 = l1 - 0.25 * w;
                if (l2 < z->lf0) l2 = z->lf0;
                z->fcur[1] = pow(10.0, l2);
                if (z->fcur[1] < g->f[0]) z->fcur[1] = g->f[0];
                if (z->fcur[1] > g->f[g->n - 1]) z->fcur[1] = g->f[g->n - 1];
            }
            z->dos = 1; z->activo = 1;
            return TZ_NADA;
        }
        if (t == T_ENTER) {
            if (z->dos) z->activo = (unsigned char)(1 - z->activo);
            else avisa(z, "[X] enciende el segundo cursor.", C_SUAVE);
            return TZ_NADA;
        }
        /* 8. autoescala y vista entera */
        if (t == T_LETRA('A')) {
            inicio(z, g);
            traz_autoescala(z, g);
            z->ajustado = 0;
            return TZ_NADA;
        }
    }
    /* 9 y 10: lo que hace ui.c */
    if (t == T_LETRA('F')) return TZ_ENTRADA;
    if (t == T_LETRA('C')) return TZ_SALIDA;
    if (t == T_IGUAL) return TZ_BARRER;
    if (t == T_TAB) return TZ_FASORES;
    if (t == T_MENU) return TZ_AJUSTES;
    if (t == T_ESC) return TZ_SALIR;
    return TZ_NADA;
}
