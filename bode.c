/* ============================================================================
 *  bode.c  -  El trazador de Bode  (ver bode.h)
 *
 *  Orden del archivo:
 *    1. la red, la entrada y la salida; una evaluación de H(f)
 *    2. el rango automático (el barrido grueso)
 *    3. las medidas afinadas (sección áurea y Brent)
 *    4. la máquina por trozos
 * ========================================================================= */
#include <string.h>
#include <math.h>
#include "bode.h"
#include "luc.h"
#include "numfmt.h"
#include "gfx.h"

#define PI_B        3.14159265358979323846
#define RAIZ2_B     1.41421356237309504880
#define NS_OP_B     (CA_NS_OP * 1e-9)  /* s por operación de ca_ops en la calculadora (ca.h) */
#define RESERVA_MED 120                /* evaluaciones de las medidas, para el progreso */

static BodeReg reg_;
static BodeRes res_;
static unsigned gen_;

static struct {
    int     activo;
    int     fase;                      /* 0 = barrido grueso, 1 = malla, 2 = medidas */
    int     i;
    int     N;
    SimRed  s;                         /* la red, con las demás fuentes apagadas */
    CaExc   x;                         /* la entrada a 1∠0 */
    SimCanal sal;
    int     ab, na, nb;                /* salida V(AB) */
    double  frec_circ;
    double  pre_f[BODE_PRE];
    Cx      pre_H[BODE_PRE];
    unsigned char pre_ok[BODE_PRE];
    SimLin  lin;                       /* su linealización en Q (Etapa 4, §3.I) */
} B;

const BodeRes *bode_res(void) { return &res_; }
const BodeReg *bode_reg(void) { return &reg_; }

/* ==========================================================================
 *  1. Una evaluación
 * ======================================================================= */
static Cx salida_de(const Cx *x, double f)
{
    const SimCanal *c = &B.sal;
    if (B.ab) return cx_menos(ca_phi(x, B.na), ca_phi(x, B.nb));
    if (c->tipo == CANAL_NODO) return ca_phi(x, c->nodo);
    if (c->tipo == CANAL_V && c->elem >= 0 && c->elem < B.s.nel) {
        const SimEl *e = &B.s.el[c->elem];
        return cx_esc(cx_menos(ca_phi(x, e->a), ca_phi(x, e->b)), (double)c->sg);
    }
    if (c->tipo == CANAL_I && c->elem >= 0 && c->elem < B.s.nel) {
        Cx V, I;
        ca_vi(&B.s, &B.x, x, f, c->elem, &V, &I);
        return cx_esc(I, (double)c->sg);
    }
    return cx(0.0, 0.0);
}

/* H(f) con la red del barrido. 0 si la matriz es singular a f (una
 * resonancia ideal justo ahí). La topología se vuelve a poner si otro
 * análisis de CA usó la de ca.c desde la nuestra (su generación cambió):
 * entre dos trozos, o entre dos lecturas de bode_en. B.s y B.lin solo
 * cambian al iniciar, que pone topo_mia a 0: vale ca_ensambla_fija */
static unsigned topo_mia;                      /* ca_topo_gen() de la nuestra; 0 = rehacerla */
static int H_en(double f, Cx *H)
{
    static Cx b[LUC_MAX], x[LUC_MAX];
    SimDiag d;
    if (!topo_mia || ca_topo_gen() != topo_mia) {
        memset(&d, 0, sizeof d);
        d.elem = d.nodo = d.elem_aviso = -1;
        ca_topologia(&B.s, &d);
        topo_mia = ca_topo_gen();
    }
    ca_pon_q(B.lin.hay ? &B.lin : 0);          /* la suya: otro análisis pudo poner otra */
    res_.evaluaciones++;
    if (ca_ensambla_fija(&B.s, f, &d) != SIM_OK) { *H = cx(0.0, 0.0); return 0; }
    ca_b_fuentes(&B.s, &B.x, b);
    ca_sustituye(b, x);
    *H = salida_de(x, f);
    return 1;
}

int bode_en(double f, Cx *H) { return H_en(f, H); }

/* dB de un módulo ya calculado (m = cx_abs(H): quien lo tiene no repite la
 * hypot) */
static double db_mod(double m) { return m > 0.0 ? 20.0 * log10(m) : -300.0; }
static double db_de(Cx H) { return db_mod(cx_abs(H)); }

/* La fase desenrollada: el salto a la anterior, en (-180, 180]. Con d +
 * 180 en [0, 360) el floor es 0 y d - 360·0 es d: sin la división */
static double desenrolla(double g, double ant)
{
    double d = g - ant, t = d + 180.0;
    if (!(t >= 0.0 && t < 360.0)) d -= 360.0 * floor(t / 360.0);
    return ant + d;
}

/* ==========================================================================
 *  2. El rango automático
 * ======================================================================= */
static int pre_hecho;                  /* B.pre_f ya está (no cambia: se hace una vez) */

/* 10^d: el del barrido grueso si cae en él (-3 + k/4 con k = 4(d + 3) es d
 * exacto: es la misma pow); si no, pow */
static double pot10(int d)
{
    int k = 4 * (d + 3);
    return pre_hecho && k >= 0 && k < BODE_PRE ? B.pre_f[k] : pow(10.0, (double)d);
}

static void rango_auto(double *f0, double *f1)
{
    double fa[BODE_PRE], db[BODE_PRE];
    int i, primero = -1, ultimo = -1, kmax = 0, d0, d1;
    double cmax = -1.0;
    for (i = 0; i < BODE_PRE; i++) {
        db[i] = B.pre_ok[i] ? db_de(B.pre_H[i]) : -300.0;
        fa[i] = B.pre_ok[i] ? cx_arg(B.pre_H[i]) * 180.0 / PI_B : 0.0;
        if (i > 0) fa[i] = desenrolla(fa[i], fa[i - 1]);
    }
    for (i = 0; i < BODE_PRE - 1; i++) {
        double s2 = i >= 1 ? fabs(db[i + 1] - 2.0 * db[i] + db[i - 1]) : 0.0, cf = fabs(fa[i + 1] - fa[i]);
        if (cf > 3.0 || s2 > 0.5) { if (primero < 0) primero = i; ultimo = i; }
        if (cf > cmax) { cmax = cf; kmax = i; }
    }
    if (primero < 0) {
        double c = B.frec_circ > 0.0 ? B.frec_circ : 1e3;
        *f0 = c / 100.0; *f1 = c * 100.0;
        return;
    }
    d0 = (int)floor(log10(B.pre_f[primero]) + 1e-9);
    d1 = (int)ceil(log10(B.pre_f[ultimo + 1]) - 1e-9);
    while (d1 - d0 < 2) { d0--; d1++; }
    if (d1 - d0 > 8) {                          /* 8 décadas, en el mayor cambio de fase */
        int c = (int)floor(log10(B.pre_f[kmax]) + 0.125 + 0.5);
        d0 = c - 4; d1 = c + 4;
    }
    if (d0 < -3) d0 = -3;
    if (d1 > 9) d1 = 9;
    *f0 = pot10(d0);
    *f1 = pot10(d1);
}

/* ==========================================================================
 *  3. Las medidas
 * ======================================================================= */
static int (*Hm)(double, Cx *);
static double lref_m;                  /* ln(ref/√2), una vez por referencia (pon_ref) */
static int modo_g;                     /* 0: ln|H| - ln(ref/√2); 1: Im H / |H| */

static void pon_ref(double ref) { lref_m = log(ref / RAIZ2_B); }

/* g de H, con su módulo m = cx_abs(H) ya calculado */
static double g_de_H(Cx H, double m)
{
    if (modo_g == 1) return m > 0.0 ? H.im / m : 0.0;
    return m > 0.0 ? log(m) - lref_m : -1e300;
}

static int g_en(double f, double *v)
{
    Cx H;
    if (!Hm(f, &H)) return 0;
    *v = g_de_H(H, cx_abs(H));
    return 1;
}

/* Brent (Numerical Recipes): la raíz de g entre a y b, g(a)·g(b) < 0, con
 * tolerancia 1e-14·a en f y 60 evaluaciones como mucho */
static double brent(double a, double b, double fa, double fb)
{
    double c = b, fc = fb, d = b - a, e = d, tol0 = 1e-14 * a;
    int it;
    for (it = 0; it < 60; it++) {
        double tol, m, p, q, r, s;
        if ((fb > 0.0 && fc > 0.0) || (fb < 0.0 && fc < 0.0)) { c = a; fc = fa; e = d = b - a; }
        if (fabs(fc) < fabs(fb)) { a = b; b = c; c = a; fa = fb; fb = fc; fc = fa; }
        tol = tol0 + 4e-16 * fabs(b);
        m = 0.5 * (c - b);
        if (fabs(m) <= tol || fb == 0.0) return b;
        if (fabs(e) >= tol && fabs(fa) > fabs(fb)) {
            s = fb / fa;
            if (a == c) { p = 2.0 * m * s; q = 1.0 - s; }
            else {
                q = fa / fc; r = fb / fc;
                p = s * (2.0 * m * q * (q - r) - (b - a) * (r - 1.0));
                q = (q - 1.0) * (r - 1.0) * (s - 1.0);
            }
            if (p > 0.0) q = -q;
            p = fabs(p);
            if (2.0 * p < fmin(3.0 * m * q - fabs(tol * q), fabs(e * q))) { e = d; d = p / q; }
            else { d = m; e = m; }
        } else { d = m; e = m; }
        a = b; fa = fb;
        b += fabs(d) > tol ? d : (m > 0.0 ? tol : -tol);
        if (!g_en(b, &fb)) return b;
    }
    return b;
}

/* |H| en ln f; singular = infinita (para el máximo) o nula */
static double mod_en(double lf, int maximo, Cx *H)
{
    if (!Hm(exp(lf), H)) { *H = cx(0.0, 0.0); return maximo ? 1e300 : 0.0; }
    return cx_abs(*H);
}

/* Sección áurea en ln f entre l0 y l1: el máximo (o el mínimo) de |H| */
static double aurea(double l0, double l1, int maximo, Cx *Hmejor)
{
    const double R = 0.61803398874989484820;
    double x1 = l1 - R * (l1 - l0), x2 = l0 + R * (l1 - l0), f1, f2, lf;
    Cx H;
    int it;
    f1 = mod_en(x1, maximo, &H);
    f2 = mod_en(x2, maximo, &H);
    for (it = 0; it < 60; it++) {
        if (maximo ? f1 > f2 : f1 < f2) { l1 = x2; x2 = x1; f2 = f1; x1 = l1 - R * (l1 - l0); f1 = mod_en(x1, maximo, &H); }
        else { l0 = x1; x1 = x2; f1 = f2; x2 = l0 + R * (l1 - l0); f2 = mod_en(x2, maximo, &H); }
    }
    lf = 0.5 * (l0 + l1);
    /* en una resonancia sin pérdidas el máximo cae justo en la f singular:
     * se mide un pelo al lado (|H| enorme pero finita, como con Q -> ∞) */
    if (!Hm(exp(lf), Hmejor) && !Hm(exp(lf) * (1.0 + 1e-9), Hmejor)) *Hmejor = cx(0.0, 0.0);
    return exp(lf);
}

/* Los puntos del barrido (los que valen) con el pico y el valle añadidos, y
 * el módulo de cada H (el de bode_medidas: los cruces no repiten la hypot) */
static int npt;
static double pf[BODE_N + 2], pM[BODE_N + 2];
static Cx pH[BODE_N + 2];

/* |H| de los mod_n_ primeros puntos de reg_ (la malla los calcula para el dB) */
static double mod_[BODE_N];
static int mod_n_;

static void pon_punto(double f, Cx H, double m)
{
    int i = npt;
    while (i > 0 && pf[i - 1] > f) { pf[i] = pf[i - 1]; pH[i] = pH[i - 1]; pM[i] = pM[i - 1]; i--; }
    pf[i] = f; pH[i] = H; pM[i] = m;
    npt++;
}

/* Los cruces de ref/√2 (modo 0) o los ceros de Im H (modo 1). La gb de un
 * tramo es la ga del siguiente: cada g se calcula una vez */
static int cruces(double *c, int max)
{
    int i, n = 0;
    double gb = npt > 0 ? g_de_H(pH[0], pM[0]) : 0.0;
    for (i = 0; i + 1 < npt && n < max; i++) {
        double ga = gb;
        gb = g_de_H(pH[i + 1], pM[i + 1]);
        if (!(ga * gb < 0.0)) continue;
        c[n++] = brent(pf[i], pf[i + 1], ga, gb);
    }
    return n;
}

void bode_medidas(const BodeReg *g, int (*H)(double, Cx *), BodeMedidas *m)
{
    double M[BODE_N], Mmax = 0.0, Mmin = 1e300, Mp = 0.0, Mv = 0.0, c[BODE_N + 2];
    int k, n = g->n, kmax = -1, kmin = -1, nc;
    Cx Hx, Hp = cx(0.0, 0.0), Hv = cx(0.0, 0.0);

    memset(m, 0, sizeof *m);
    Hm = H;
    if (n < 2) return;
    for (k = 0; k < n; k++) {
        M[k] = g == &reg_ && k < mod_n_ ? mod_[k] : cx_abs(g->H[k]);   /* el nuestro ya lo tiene */
        if (!g->ok[k]) continue;
        if (M[k] > Mmax) Mmax = M[k];
        if (M[k] < Mmin) Mmin = M[k];
    }
    m->M0 = H(g->f[0] / 1e4, &Hx) ? cx_abs(Hx) : M[0];
    m->Minf = H(g->f[n - 1] * 1e4, &Hx) ? cx_abs(Hx) : M[n - 1];
    /* la entrada es 1∠0: una salida que no pasa de 1e-12 es nula (un puente
     * equilibrado, la sonda en la referencia) y su ruido no es una forma */
    if (fmax(Mmax, fmax(m->M0, m->Minf)) <= 1e-12) { m->tipo = BODE_NULA; return; }
    for (k = 1; k < n - 1; k++) {
        if (!g->ok[k]) continue;
        if (kmax < 0 || M[k] > M[kmax]) kmax = k;
        if (kmin < 0 || M[k] < M[kmin]) kmin = k;
    }
    /* el pico y el valle interiores, afinados */
    /* un pico o un valle de verdad, no el ruido de redondeo de una curva plana */
    if (kmax > 0 && g->ok[kmax - 1] && g->ok[kmax + 1] &&
        M[kmax] > M[kmax - 1] * (1.0 + 1e-9) && M[kmax] > M[kmax + 1] * (1.0 + 1e-9)) {
        m->f_pico = aurea(log(g->f[kmax - 1]), log(g->f[kmax + 1]), 1, &Hp);
        Mp = cx_abs(Hp);
        m->db_pico = db_mod(Mp);
        if (Mp > Mmax) Mmax = Mp;
    }
    if (kmin > 0 && g->ok[kmin - 1] && g->ok[kmin + 1] &&
        M[kmin] < M[kmin - 1] * (1.0 - 1e-9) && M[kmin] < M[kmin + 1] * (1.0 - 1e-9)) {
        m->f_valle = aurea(log(g->f[kmin - 1]), log(g->f[kmin + 1]), 0, &Hv);
        Mv = cx_abs(Hv);
        m->db_valle = db_mod(Mv);
    }
    npt = 0;
    for (k = 0; k < n; k++) if (g->ok[k]) pon_punto(g->f[k], g->H[k], M[k]);
    if (m->f_pico > 0.0) pon_punto(m->f_pico, Hp, Mp);
    if (m->f_valle > 0.0) pon_punto(m->f_valle, Hv, Mv);
    /* pendientes en los extremos */
    if (n >= 2 && g->ok[0] && g->ok[1]) m->pend_ini = (g->db[1] - g->db[0]) / log10(g->f[1] / g->f[0]);
    if (n >= 2 && g->ok[n - 1] && g->ok[n - 2]) m->pend_fin = (g->db[n - 1] - g->db[n - 2]) / log10(g->f[n - 1] / g->f[n - 2]);

    modo_g = 0;
    if (m->M0 >= RAIZ2_B * m->Minf && m->M0 >= Mmax / 10.0) {
        m->tipo = BODE_PB;
        pon_ref(m->M0);
        nc = cruces(c, BODE_N + 2);
        m->fc = nc > 0 ? c[nc - 1] : 0.0;                      /* el más alto */
    } else if (m->Minf >= RAIZ2_B * m->M0 && m->Minf >= Mmax / 10.0) {
        m->tipo = BODE_PA;
        pon_ref(m->Minf);
        nc = cruces(c, BODE_N + 2);
        m->fc = nc > 0 ? c[0] : 0.0;                           /* el más bajo */
    } else if (m->f_pico > 0.0 && Mp >= RAIZ2_B * fmax(m->M0, m->Minf)) {
        int i;
        double lo = 0.0, hi = 0.0, mejor = 1e300;
        m->tipo = BODE_PBANDA;
        pon_ref(Mp);
        nc = cruces(c, BODE_N + 2);
        for (i = 0; i < nc; i++) {
            if (c[i] < m->f_pico && c[i] > lo) lo = c[i];
            if (c[i] > m->f_pico && (hi == 0.0 || c[i] < hi)) hi = c[i];
        }
        if (lo > 0.0 && hi > 0.0) { m->f1 = lo; m->f2 = hi; m->bw = hi - lo; m->q = m->f_pico / m->bw; }
        /* la fase 0: H real, el cruce más cerca del pico (en log f) */
        modo_g = 1;
        nc = cruces(c, BODE_N + 2);
        for (i = 0; i < nc; i++)
            if (fabs(log(c[i] / m->f_pico)) < mejor) { mejor = fabs(log(c[i] / m->f_pico)); m->f_fase0 = c[i]; }
        modo_g = 0;
    } else if (m->f_valle > 0.0 && Mv <= fmin(m->M0, m->Minf) / RAIZ2_B &&
               m->M0 > 0.0 && m->Minf > 0.0 && fabs(20.0 * log10(m->M0 / m->Minf)) < 3.0) {
        int i;
        double lo = 0.0, hi = 0.0;
        m->tipo = BODE_RB;
        pon_ref(sqrt(m->M0 * m->Minf));
        nc = cruces(c, BODE_N + 2);
        for (i = 0; i < nc; i++) {
            if (c[i] < m->f_valle && c[i] > lo) lo = c[i];
            if (c[i] > m->f_valle && (hi == 0.0 || c[i] < hi)) hi = c[i];
        }
        if (lo > 0.0 && hi > 0.0) { m->f1 = lo; m->f2 = hi; m->bw = hi - lo; m->q = m->f_valle / m->bw; }
    } else if (Mmax / (Mmin > 1e-300 ? Mmin : 1e-300) < RAIZ2_B) m->tipo = BODE_PLANO;
    else m->tipo = BODE_OTRO;
}

/* ==========================================================================
 *  4. La máquina por trozos
 * ======================================================================= */
static void nombre_red(const SimRed *s, int k, char *t, int cap)
{
    static const char *const LETRA = "REJCLSAVZDEJUQM";      /* una por cada SE_* */
    Buf b;
    buf_init(&b, t, (size_t)cap);
    if (k < 0 || k >= s->nel) { buf_str(&b, "?"); return; }
    buf_ch(&b, LETRA[s->el[k].tipo < SE_N ? s->el[k].tipo : 0]);
    buf_int(&b, k + 1);
}

int bode_iniciar_red(const SimRed *s, int ent, const SimCanal *sal, int na, int nb,
                     double f_ini, double f_fin, double frec_circ)
{
    SimDiag d;
    Cx H;
    int k, otras = 0, e;
    unsigned long o0;
    double t1;

    memset(&res_, 0, sizeof res_);
    memset(&reg_, 0, sizeof reg_); mod_n_ = 0;
    res_.gen = ++gen_;
    res_.d.elem = res_.d.nodo = res_.d.elem_aviso = -1;
    res_.ent = ent;
    B.activo = 0;
    topo_mia = 0;
    if (ent < 0 || ent >= s->nel || (s->el[ent].tipo != SE_E && s->el[ent].tipo != SE_J)) {
        res_.d.estado = SIM_SIN_FUENTE; res_.fin = FIN_ERROR; return res_.d.estado;
    }
    B.s = *s;
    /* el punto Q con todas las fuentes de CC, antes de apagar las demás */
    if (ca_punto_q(&B.s, &B.lin, &res_.d) != SIM_OK) { res_.fin = FIN_ERROR; return res_.d.estado; }
    memset(&B.x, 0, sizeof B.x);
    B.x.U[ent] = cx(1.0, 0.0);
    B.x.activa = 1u << ent;
    for (k = 0; k < B.s.nel; k++)
        if (k != ent && (B.s.el[k].tipo == SE_E || B.s.el[k].tipo == SE_J)) otras = 1;
    ca_apaga(&B.s, B.x.activa);
    B.ab = 0; B.na = na; B.nb = nb;
    memset(&B.sal, 0, sizeof B.sal);
    B.sal.elem = B.sal.nodo = -1;
    if (sal && sal->tipo != CANAL_NADA) B.sal = *sal;
    else if (na >= 0 && nb >= 0) B.ab = 1;
    res_.sal = B.sal;
    B.frec_circ = frec_circ;

    e = ca_topologia(&B.s, &d);
    d.avisos |= res_.d.avisos;
    if (d.elem_aviso < 0) d.elem_aviso = res_.d.elem_aviso;
    res_.d = d;
    if (e != SIM_OK) { res_.fin = FIN_ERROR; return e; }
    topo_mia = ca_topo_gen();
    if (otras) res_.d.avisos |= SIM_AV_OTRAS;
    for (k = 0; k < B.s.nel; k++) if (B.s.el[k].tipo == SE_Z) res_.d.avisos |= SIM_AV_Z_ESCALA;

    /* la unidad de H: V/V, A/V (S), V/A (ohm) o A/A */
    {
        int sal_i = !B.ab && B.sal.tipo == CANAL_I, fuente_j = s->el[ent].tipo == SE_J;
        Buf b;
        buf_init(&b, res_.unidad, sizeof res_.unidad);
        if (!fuente_j && sal_i) buf_str(&b, "S");
        else if (fuente_j && !sal_i) buf_str(&b, S_OHM);
    }
    nombre_red(s, ent, res_.nom_ent, sizeof res_.nom_ent);
    if (B.ab) { Buf b; buf_init(&b, res_.nom_sal, sizeof res_.nom_sal); buf_str(&b, "V(AB)"); }
    else if (B.sal.nombre[0]) { Buf b; buf_init(&b, res_.nom_sal, sizeof res_.nom_sal); buf_str(&b, B.sal.nombre); }
    else { Buf b; buf_init(&b, res_.nom_sal, sizeof res_.nom_sal); buf_str(&b, "?"); }

    res_.auto_f = (unsigned char)!(f_ini > 0.0 && f_fin > f_ini);
    res_.f_ini = f_ini; res_.f_fin = f_fin;

    /* el costo de una evaluación decide la malla: 277, 139 o 70 puntos */
    o0 = ca_ops();
    H_en(frec_circ > 0.0 ? frec_circ : 1e3, &H);
    t1 = (double)(ca_ops() - o0) * NS_OP_B;
    B.N = t1 * 450.0 > 6.0 ? 70 : t1 * 450.0 > 3.0 ? 139 : BODE_N;
    res_.evaluaciones = 0;
    res_.t_est = t1 * (double)((res_.auto_f ? BODE_PRE : 0) + B.N + RESERVA_MED);

    B.fase = res_.auto_f ? 0 : 1;
    B.i = 0;
    if (!pre_hecho) {                           /* no cambian: una vez */
        for (k = 0; k < BODE_PRE; k++) B.pre_f[k] = pow(10.0, -3.0 + (double)k / 4.0);
        pre_hecho = 1;
    }
    B.activo = 1;
    return SIM_OK;
}

/* La salida elegida del circuito: una sonda, V(AB) o el canal automático */
int bode_iniciar(const Circuito *c, const Red *r, const BodeAjustes *aj)
{
    static SimRed s;
    SimCanal ch[2];
    SimDiag d;
    int ent = -1, k, na = -1, nb = -1, e, hay_ab;
    const SimCanal *sal = 0;

    memset(&res_, 0, sizeof res_);
    memset(&reg_, 0, sizeof reg_); mod_n_ = 0;
    res_.gen = ++gen_;
    B.activo = 0;
    /* la entrada: la pedida, o la primera alterna o senoidal, o la primera E o J */
    if (aj && aj->ent >= 0 && aj->ent < c->nel && (c->el[aj->ent].tipo == EL_FUENTE || c->el[aj->ent].tipo == EL_CORR)) ent = aj->ent;
    for (k = 0; k < c->nel && ent < 0; k++)
        if ((c->el[k].tipo == EL_FUENTE || c->el[k].tipo == EL_CORR) &&
            (c->el[k].variante == FORMA_FASOR || c->el[k].variante == ONDA_SENO)) ent = k;
    for (k = 0; k < c->nel && ent < 0; k++)
        if (c->el[k].tipo == EL_FUENTE || c->el[k].tipo == EL_CORR) ent = k;
    if (ent < 0) {
        res_.d.estado = SIM_SIN_FUENTE; res_.d.elem = res_.d.nodo = res_.d.elem_aviso = -1;
        res_.fin = FIN_ERROR;
        return res_.d.estado;
    }
    e = sim_red_de_ex(c, r, &s, &d, 0);
    if (e != SIM_OK) { res_.d = d; res_.fin = FIN_ERROR; return e; }
    sim_canales(c, r, ch);
    hay_ab = c->term_x[0] >= 0 && c->term_y[0] >= 0 && c->term_x[1] >= 0 && c->term_y[1] >= 0;
    if (hay_ab) {
        na = r->punto[c->term_y[0] * REJ_COLS + c->term_x[0]];
        nb = r->punto[c->term_y[1] * REJ_COLS + c->term_x[1]];
        if (na < 0 || nb < 0) hay_ab = 0;
    }
    switch (aj ? aj->sal : BS_AUTO) {
    case BS_SONDA1: if (c->sonda[0].tipo != SONDA_NO) sal = &ch[0]; break;
    case BS_SONDA2: if (c->sonda[1].tipo != SONDA_NO) sal = &ch[1]; break;
    case BS_AB: break;
    default:
        if (c->sonda[0].tipo != SONDA_NO) sal = &ch[0];
        else if (!hay_ab) sal = ch[0].tipo != CANAL_NADA ? &ch[0] : &ch[1];
        break;
    }
    if (!sal && !hay_ab) sal = ch[0].tipo != CANAL_NADA ? &ch[0] : &ch[1];
    e = bode_iniciar_red(&s, ent, sal, sal ? -1 : na, sal ? -1 : nb,
                         aj ? aj->f_ini : 0.0, aj ? aj->f_fin : 0.0, c->frec.est == M_DATO ? c->frec.v : 0.0);
    {
        Buf b;
        buf_init(&b, res_.nom_ent, sizeof res_.nom_ent);
        buf_str(&b, c->el[ent].etiq);
    }
    return e;
}

static void prepara_malla(void)
{
    double r = res_.f_fin / res_.f_ini;
    int k;
    reg_.n = B.N;
    for (k = 0; k < B.N; k++)
        reg_.f[k] = res_.f_ini * pow(r, (double)k / (double)(B.N - 1));
    reg_.f[0] = res_.f_ini; reg_.f[B.N - 1] = res_.f_fin;
}

/* ops, por diferencia con el contador (de 32 bits en la calculadora: ver
 * sim_avanzar) */
int bode_avanzar(unsigned long ops)
{
    unsigned long ahora = ca_ops();
    if (!B.activo) return 0;
    do {
        if (B.fase == 0) {
            B.pre_ok[B.i] = (unsigned char)H_en(B.pre_f[B.i], &B.pre_H[B.i]);
            if (++B.i == BODE_PRE) {
                rango_auto(&res_.f_ini, &res_.f_fin);
                B.fase = 1; B.i = 0;
            }
        } else if (B.fase == 1) {
            int k = B.i;
            if (k == 0) prepara_malla();
            reg_.ok[k] = (unsigned char)H_en(reg_.f[k], &reg_.H[k]);
            mod_[k] = reg_.ok[k] ? cx_abs(reg_.H[k]) : 0.0;      /* la H singular es 0: hypot(0, 0) */
            mod_n_ = k + 1;
            reg_.db[k] = reg_.ok[k] ? db_mod(mod_[k]) : -300.0;
            reg_.fase[k] = reg_.ok[k] ? cx_arg(reg_.H[k]) * 180.0 / PI_B : (k > 0 ? reg_.fase[k - 1] : 0.0);
            if (k > 0 && reg_.ok[k]) reg_.fase[k] = desenrolla(reg_.fase[k], reg_.fase[k - 1]);
            if (++B.i == B.N) B.fase = 2;
        } else {
            bode_medidas(&reg_, H_en, &res_.m);
            res_.fin = FIN_COMPLETO;
            res_.d.estado = SIM_OK;
            B.activo = 0;
            return 0;
        }
    } while (ca_ops() - ahora < ops);
    return 1;
}

void bode_cortar(void)
{
    if (!B.activo) return;
    B.activo = 0;
    res_.fin = FIN_CORTADO;
    if (B.fase == 1) reg_.n = B.i;               /* lo barrido vale */
    else if (B.fase == 0) reg_.n = 0;
    memset(&res_.m, 0, sizeof res_.m);
}

double bode_progreso(void)
{
    double total = (double)((res_.auto_f ? BODE_PRE : 0) + B.N + RESERVA_MED), hechas;
    if (!B.activo) return res_.fin == FIN_COMPLETO ? 1.0 : 0.0;
    hechas = B.fase == 0 ? (double)B.i : (double)((res_.auto_f ? BODE_PRE : 0) + (B.fase == 1 ? B.i : B.N));
    return total > 0.0 ? hechas / total : 0.0;
}
