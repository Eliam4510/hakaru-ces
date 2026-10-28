/* ============================================================================
 *  ca.c  -  La corriente alterna: fasores a una frecuencia  (ver ca.h)
 *
 *  Orden del archivo:
 *    1. frecuencia, excitación y fuentes apagadas
 *    2. ensamblado a ω (la matriz del trapecio con g = jωC, z = jωL) y
 *       resonancia sin pérdidas
 *    3. valores, potencias y lecturas
 *    3b. sensibilidades (el jacobiano exacto de la calculadora)
 *    4. desde el Circuito
 *    5. Thévenin, Norton y ohmímetro
 *    6. convenios y avisos
 * ========================================================================= */
#include <string.h>
#include <math.h>
#include "ca.h"
#include "luc.h"
#include "numfmt.h"
#include "gfx.h"

#define PI_CA   3.14159265358979323846
#define RAIZ2   1.41421356237309504880

static LUC luc_;                     /* ~135 KB: una sola, un sistema a la vez */
static const double cero_ = 0.0;
static SimTopoCA topo_;
static SimMapa mapa_;                /* el de topo_ en int (sim_estampa_lin, sim_ctl_coef) */
static unsigned topo_gen_;           /* sube con cada ca_topologia (topo_ y mapa_ nuevos) */
static int topo_reusa_;              /* la última salió bien y se puede reutilizar       */
static int nanc_;                    /* las columnas de los nodos anclados de topo_      */
static signed char anc_[MAX_NODOS];
static const SimLin *lin_act;        /* la linealización que se estampa (NULL = ninguna) */
static SimLin lin_ca;                /* la de analiza_con                                  */

void ca_pon_q(const SimLin *L) { lin_act = L; }
const SimLin *ca_lin(void) { return lin_act; }
static unsigned long ops_;

unsigned long ca_ops(void)       { return ops_ + 4ul * luc_ops(); }
void          ca_ops_reset(void) { ops_ = 0; luc_ops_reset(); }
#ifndef NDLESS
void          ca_ops_pon(unsigned long v) { ca_ops_reset(); ops_ = v; }   /* el arnés: cerca de la vuelta */
#endif

static void aviso(SimDiag *d, unsigned bit, int k)
{
    if (!(d->avisos & bit) && d->elem_aviso < 0) d->elem_aviso = k;
    d->avisos |= bit;
}

/* ==========================================================================
 *  1. Frecuencia, excitación y fuentes apagadas
 * ======================================================================= */
double ca_frecuencia(const SimRed *s, double frec_circ, SimDiag *d)
{
    double f = 0.0;
    int k, primera = -1;
    for (k = 0; k < s->nel; k++) {
        const SimEl *e = &s->el[k];
        if (e->tipo != SE_E && e->tipo != SE_J) continue;
        if (e->forma == ONDA_SENO) {
            if (f == 0.0) { f = e->p[1]; primera = k; }
            else if (fabs(e->p[1] - f) > CA_TOL_F * f) {
                d->estado = SIM_CA_FRECS; d->elem = k;
                d->lazo = (1u << primera) | (1u << k);
                return 0.0;
            }
        } else if (e->forma != ONDA_CC) { d->estado = SIM_CA_FORMA; d->elem = k; return 0.0; }
    }
    if (f == 0.0) {
        if (frec_circ > 0.0) f = frec_circ;
        else { d->estado = SIM_CA_SIN_F; return 0.0; }
    }
    return f;
}

void ca_excitacion(const SimRed *s, double f, CaExc *x, SimDiag *d)
{
    int k, nl = sim_hay_nl(s);
    memset(x, 0, sizeof *x);
    for (k = 0; k < s->nel; k++) {
        const SimEl *e = &s->el[k];
        switch (e->tipo) {
        case SE_E:
        case SE_J:
            if (e->forma == ONDA_SENO) {
                /* CC + Vp·sen(ωt + θ) = CC + Vp·cos(ωt + θ - 90°): el nivel de
                 * CC se apaga (es de otra frecuencia) */
                x->U[k] = cx_polar(e->p[0], (e->p[2] - 90.0) * PI_CA / 180.0);
                x->activa |= 1u << k;
                if (e->p[3] != 0.0 && !nl) aviso(d, SIM_AV_CC_FUERA, k);   /* con no lineales: PEQ_SENAL */
            } else if (e->forma == ONDA_CC) {
                if (e->p[0] != 0.0 && !nl) aviso(d, SIM_AV_CC_FUERA, k);
            } else if (d->estado == SIM_OK) { d->estado = SIM_CA_FORMA; d->elem = k; }
            break;
        case SE_S:
            if (e->t1 >= 0.0 || e->t2 >= 0.0) aviso(d, SIM_AV_S_FINAL, k);
            break;
        case SE_C:
        case SE_L:
            if (e->ci && e->x0 != 0.0) aviso(d, SIM_AV_CI_NO, k);
            break;
        case SE_Z:
            if (fabs(e->p[1] - f) > CA_TOL_F * f) aviso(d, SIM_AV_Z_ESCALA, k);
            break;
        default:
            break;
        }
    }
    ops_ += (unsigned long)(10 * s->nel);
}

/* Una J apagada no impone nada: pasa a un voltímetro ideal (si no, con A-B
 * abierto daría CORTE_J); se marca con instr = 'j' para dar su V con el
 * convenio de las fuentes. Una E apagada es un corto (ideal, U = 0) o su r
 * (real: ya es una conductancia; sin su inyección). */
void ca_apaga(SimRed *s, unsigned int activa)
{
    int k;
    for (k = 0; k < s->nel; k++) {
        SimEl *e = &s->el[k];
        if ((activa >> k) & 1u) continue;
        if (e->tipo == SE_J) { e->tipo = SE_VMI; e->instr = 'j'; }
    }
}

/* ==========================================================================
 *  2. Ensamblado a ω y resonancia
 * ======================================================================= */
int ca_topologia(const SimRed *s, SimDiag *d)
{
    unsigned long o0 = sim_ops();
    int e = sim_topo_ca(s, &topo_, d), n, k;
    for (n = 0; n < MAX_NODOS; n++) mapa_.phi[n] = topo_.col_phi[n];
    for (k = 0; k < MAX_EL; k++) mapa_.br[k] = topo_.col_br[k];
    for (nanc_ = 0, n = 0; n < MAX_NODOS; n++)
        if (topo_.col_phi[n] >= 0 && topo_.ancla[n]) anc_[nanc_++] = topo_.col_phi[n];
    if (++topo_gen_ == 0) topo_gen_ = 1;
    /* no se reutiliza la que contó operaciones del emulador (fuente_en en
     * un lazo de ramas V): rehacerla las vuelve a contar, y sim_ops() tiene
     * que dar lo mismo */
    topo_reusa_ = e == SIM_OK && sim_ops() == o0;
    return e;
}

unsigned ca_topo_gen(void) { return topo_reusa_ ? topo_gen_ : 0u; }

int ca_n(void) { return topo_.n; }
const SimTopoCA *ca_topo(void) { return &topo_; }

static int cph(int n) { return n >= 0 ? topo_.col_phi[n] : -1; }

Cx ca_z_de(const SimEl *e, double f)
{
    double w = 2.0 * PI_CA * f;
    switch (e->tipo) {
    case SE_R: return cx(e->val, 0.0);
    case SE_C: return cx(0.0, -1.0 / (w * e->val));
    case SE_L: return cx(0.0, w * e->val);
    case SE_Z: {
        /* la Z es la del circuito a f0; a otra f, como R + L o R + C */
        double X = e->p[0], f0 = e->p[1];
        if (X > 0.0 && f != f0) X *= f / f0;
        else if (X < 0.0 && f != f0) X *= f0 / f;
        return cx(e->val, X);
    }
    default: return cx(0.0, 0.0);
    }
}

/* luc_suma con un coeficiente real (el estampado común de sim.c) */
static void suma_cx(void *m, int i, int j, double v) { luc_suma((LUC *)m, i, j, cx(v, 0.0)); }

static void y_ab(int a, int b, Cx Y)
{
    int ca = cph(a), cb = cph(b);
    Cx m = cx(-Y.re, -Y.im);
    luc_suma(&luc_, ca, ca, Y); luc_suma(&luc_, cb, cb, Y);
    luc_suma(&luc_, ca, cb, m); luc_suma(&luc_, cb, ca, m);
}

/* Las filas de rama de C y L: lo único de la matriz que cambia con f (sin
 * Z). Solo las escribe su elemento: la ley de nodos de su corriente va en
 * las filas de sus nodos */
static void estampa_cl(const SimEl *e, int j, int ca, int cb, double w)
{
    if (e->tipo == SE_C) {                                         /* i - jωC (phi_a - phi_b) = 0 */
        luc_suma(&luc_, j, j, cx(1.0, 0.0));
        luc_suma(&luc_, j, ca, cx(0.0, -w * e->val));
        luc_suma(&luc_, j, cb, cx(0.0, w * e->val));
    } else {                                                       /* (phi_a - phi_b) - jωL i = 0 */
        luc_suma(&luc_, j, ca, cx(1.0, 0.0)); luc_suma(&luc_, j, cb, cx(-1.0, 0.0));
        luc_suma(&luc_, j, j, cx(0.0, -w * e->val));
    }
}

/* La matriz a f y su LU. sin_cl = 1: sin las filas de C y L, sin contar
 * y sin factorizar (la base de la red fija, abajo) */
static int ensambla_en(const SimRed *s, double f, int sin_cl)
{
    double w = 2.0 * PI_CA * f;
    const Cx uno = cx(1.0, 0.0), menos = cx(-1.0, 0.0);
    int k, n;
    luc_empieza(&luc_, topo_.n);
    for (k = 0; k < s->nel; k++) {
        const SimEl *e = &s->el[k];
        int a = e->a, b = e->b, j = topo_.col_br[k], ca = cph(a), cb = cph(b);
        if (j >= 0) {                            /* sale de a (del puerto 0: nref y c en el AO) */
            int p = e->tipo == SE_AO ? e->nref : a, q = e->tipo == SE_AO ? e->c : b;
            luc_suma(&luc_, cph(p), j, uno); luc_suma(&luc_, cph(q), j, menos);
        }
        switch (e->tipo) {
        case SE_R:
            y_ab(a, b, cx(1.0 / e->val, 0.0));
            break;
        case SE_E:
            if (j >= 0) { luc_suma(&luc_, j, cb, uno); luc_suma(&luc_, j, ca, menos); }   /* phi_b - phi_a = U */
            else y_ab(a, b, cx(1.0 / e->val, 0.0));                                     /* Norton: 1/r */
            break;
        case SE_AMI:                                                   /* A y W: un cable */
            luc_suma(&luc_, j, ca, uno); luc_suma(&luc_, j, cb, menos);
            break;
        case SE_S:
            if ((topo_.mask >> k) & 1u) { luc_suma(&luc_, j, ca, uno); luc_suma(&luc_, j, cb, menos); }
            else luc_suma(&luc_, j, j, uno);
            break;
        case SE_C:
        case SE_L:
            if (!sin_cl) estampa_cl(e, j, ca, cb, w);
            break;
        case SE_Z:
            y_ab(a, b, cx_inv(ca_z_de(e, f)));
            break;
        default:                                                       /* J, voltímetro ideal; lo de abajo */
            break;
        }
    }
    /* las controladas y los no lineales linealizados en Q (la de lin_act) */
    sim_estampa_lin(s, lin_act, &mapa_, suma_cx, &luc_);
    for (n = 0; n < s->nn; n++) {
        int cn = topo_.col_phi[n];
        if (cn < 0 || !topo_.ancla[n]) continue;
        memset(luc_.a[cn], 0, (size_t)luc_.n * sizeof luc_.a[0][0]);
        luc_.a[cn][cn] = uno;
    }
    if (sin_cl) return 1;
    ops_ += (unsigned long)(12 * s->nel);
    return luc_factoriza(&luc_);
}

/* ---- La red fija (el Bode) ----
 * Con la misma red y la misma linealización en cada f, lo que no depende de
 * f (todo menos las filas de C y L: la G de cada R, las fuentes, las
 * controladas, los no lineales en Q y las anclas) se hace una vez por
 * topología y se guardan sus entradas no nulas, ya sumadas en el orden de
 * ensambla_en. En cada f van a la matriz a cero, y las filas de C y L, que
 * allí también empiezan en cero, se estampan igual: los mismos bits y las
 * mismas operaciones contadas. Una Z suma su Y(f) en entradas compartidas:
 * con una Z no hay base. */
#define BASE_MAX 640                    /* entradas no nulas; con más, sin base */
static struct {
    unsigned gen;                       /* la topología con que se hizo; 0 = ninguna */
    const SimRed *s;
    const SimLin *lin;
    int ok, nb, ncl;
    unsigned char fi[BASE_MAX], co[BASE_MAX], cl[MAX_EL];
    Cx v[BASE_MAX];
} base_;

/* +0 y +0 (lo que deja luc_empieza), por los bits */
static int nulo_bits(const Cx *z)
{
    unsigned long long r, i;
    memcpy(&r, &z->re, sizeof r);
    memcpy(&i, &z->im, sizeof i);
    return (r | i) == 0;
}

static void haz_base(const SimRed *s)
{
    int i, j, k, n = topo_.n;
    base_.gen = topo_gen_; base_.s = s; base_.lin = lin_act;
    base_.ok = base_.nb = base_.ncl = 0;
    for (k = 0; k < s->nel; k++) {
        if (s->el[k].tipo == SE_Z) return;
        if (s->el[k].tipo == SE_C || s->el[k].tipo == SE_L) base_.cl[base_.ncl++] = (unsigned char)k;
    }
    ensambla_en(s, 0.0, 1);
    for (k = 0; k < base_.ncl; k++) {           /* sus filas, vacías: nadie más escribe en ellas */
        int jr = topo_.col_br[base_.cl[k]];
        if (jr >= 0) for (j = 0; j < n; j++) if (!nulo_bits(&luc_.a[jr][j])) return;
    }
    for (i = 0; i < n; i++)
        for (j = 0; j < n; j++) {
            if (nulo_bits(&luc_.a[i][j])) continue;
            if (base_.nb == BASE_MAX) return;
            base_.fi[base_.nb] = (unsigned char)i;
            base_.co[base_.nb] = (unsigned char)j;
            base_.v[base_.nb++] = luc_.a[i][j];
        }
    base_.ok = 1;
}

static int ensambla_base(const SimRed *s, double f)
{
    double w = 2.0 * PI_CA * f;
    int k;
    luc_empieza(&luc_, topo_.n);
    for (k = 0; k < base_.nb; k++) luc_.a[base_.fi[k]][base_.co[k]] = base_.v[k];
    for (k = 0; k < base_.ncl; k++) {
        const SimEl *e = &s->el[base_.cl[k]];
        estampa_cl(e, topo_.col_br[base_.cl[k]], cph(e->a), cph(e->b), w);
    }
    ops_ += (unsigned long)(12 * s->nel);
    return luc_factoriza(&luc_);
}

static int ensambla_f(const SimRed *s, double f, int fija)
{
    return fija ? ensambla_base(s, f) : ensambla_en(s, f, 0);
}

/* Reactivo: +1 si se porta como una L (L, Z con X > 0), -1 como una C
 * (C, Z con X < 0), 0 si no */
static int reactivo(const SimEl *e)
{
    if (e->tipo == SE_L) return 1;
    if (e->tipo == SE_C) return -1;
    if (e->tipo == SE_Z) return e->p[0] > 0.0 ? 1 : e->p[0] < 0.0 ? -1 : 0;
    return 0;
}

/* El reactivo de la columna mala: su rama, la primera que toca su nodo o,
 * si la columna es la de un A, un W, un S o una E, el primero de la red */
static int reactivo_de(const SimRed *s, int col)
{
    int k, n;
    for (k = 0; k < s->nel; k++)
        if (topo_.col_br[k] == col && reactivo(&s->el[k])) return k;
    for (n = 0; n < s->nn; n++) {
        if (topo_.col_phi[n] != col) continue;
        for (k = 0; k < s->nel; k++)
            if ((s->el[k].a == n || s->el[k].b == n) && reactivo(&s->el[k])) return k;
    }
    for (k = 0; k < s->nel; k++) if (reactivo(&s->el[k])) return k;
    return -1;
}

/* Su pareja en la resonancia: la primera del otro signo que comparte un
 * nodo con ella; si no hay, la primera del otro signo */
static int pareja_de(const SimRed *s, int k)
{
    int q, otro = -reactivo(&s->el[k]), prim = -1;
    for (q = 0; q < s->nel; q++) {
        const SimEl *e = &s->el[q];
        if (!otro || reactivo(e) != otro) continue;
        if (prim < 0) prim = q;
        if (e->a == s->el[k].a || e->a == s->el[k].b || e->b == s->el[k].a || e->b == s->el[k].b) return q;
    }
    return prim;
}

static int ensambla_d(const SimRed *s, double f, SimDiag *d, int fija)
{
    int col, k, n;
    if (ensambla_f(s, f, fija)) { d->estado = SIM_OK; return SIM_OK; }
    col = luc_.col_mala;
    /* ¿Resonancia sin pérdidas? A ω(1 + 1e-6) ya no es singular */
    if (ensambla_f(s, f * (1.0 + 1e-6), fija)) {
        int p;
        k = reactivo_de(s, col);
        p = k >= 0 ? pareja_de(s, k) : -1;
        luc_.ok = 0;                              /* la factorización de ω(1 + 1e-6) no vale */
        d->estado = SIM_RESONANCIA; d->elem = k; d->t = f;
        d->lazo = (k >= 0 ? 1u << k : 0u) | (p >= 0 ? 1u << p : 0u);
        return d->estado;
    }
    luc_.ok = 0;
    d->estado = SIM_SINGULAR;
    for (k = 0; k < s->nel; k++) if (topo_.col_br[k] == col) d->elem = k;
    for (n = 0; n < s->nn; n++) if (topo_.col_phi[n] == col) d->nodo = n;
    return d->estado;
}

int ca_ensambla(const SimRed *s, double f, SimDiag *d) { return ensambla_d(s, f, d, 0); }

int ca_ensambla_fija(const SimRed *s, double f, SimDiag *d)
{
    if (!topo_reusa_) return ensambla_d(s, f, d, 0);
    if (base_.gen != topo_gen_ || base_.s != s || base_.lin != lin_act) haz_base(s);
    return ensambla_d(s, f, d, base_.ok);
}

/* Corriente I de a hacia b por dentro (sale de a) */
static void inyecta_cx(Cx *b, int a, int bb, Cx I)
{
    int ca = cph(a), cb = cph(bb);
    if (ca >= 0) b[ca] = cx_menos(b[ca], I);
    if (cb >= 0) b[cb] = cx_mas(b[cb], I);
}

/* La parte conocida del control de la controlada k: la I de una E real o de
 * una J activas (U/r o U); 0 si no hay */
static Cx ctl_c0(const SimRed *s, const CaExc *x, int k)
{
    int col[3], fu;
    double c[3], f0;
    sim_ctl_coef(s, &mapa_, k, col, c, &fu, &f0);
    if (fu < 0 || !((x->activa >> fu) & 1u)) return cx(0.0, 0.0);
    return cx_esc(x->U[fu], f0);
}

/* El control de la controlada k con la solución x */
static Cx ctl_valor(const SimRed *s, const CaExc *e, const Cx *x, int k)
{
    int col[3], i, nt, fu;
    double c[3], f0;
    Cx y = ctl_c0(s, e, k);
    nt = sim_ctl_coef(s, &mapa_, k, col, c, &fu, &f0);
    for (i = 0; i < nt; i++) if (col[i] >= 0) y = cx_mas(y, cx_esc(x[col[i]], c[i]));
    return y;
}

void ca_b_fuentes(const SimRed *s, const CaExc *x, Cx *b)
{
    int k, n;
    for (k = 0; k < topo_.n; k++) b[k] = cx(0.0, 0.0);
    for (k = 0; k < s->nel; k++) {
        const SimEl *e = &s->el[k];
        int j = topo_.col_br[k];
        if (e->tipo == SE_EC || e->tipo == SE_JC) {  /* la ganancia por la parte conocida */
            Cx c0 = cx_esc(ctl_c0(s, x, k), e->val);
            if (e->tipo == SE_EC) b[j] = c0;
            else inyecta_cx(b, e->a, e->b, c0);
            continue;
        }
        if (!((x->activa >> k) & 1u)) continue;
        if (e->tipo == SE_E) {
            if (j >= 0) b[j] = x->U[k];
            else inyecta_cx(b, e->a, e->b, cx_esc(x->U[k], 1.0 / e->val));
        } else if (e->tipo == SE_J) inyecta_cx(b, e->a, e->b, x->U[k]);
    }
    for (n = 0; n < s->nn; n++)
        if (topo_.col_phi[n] >= 0 && topo_.ancla[n]) b[topo_.col_phi[n]] = cx(0.0, 0.0);
}

void ca_b_inyecta(int na, int nb, Cx I, Cx *b)
{
    int ca = cph(na), cb = cph(nb);
    if (ca >= 0) b[ca] = cx_mas(b[ca], I);
    if (cb >= 0) b[cb] = cx_menos(b[cb], I);
}

void ca_sustituye(const Cx *b, Cx *x)
{
    static Cx t[LUC_MAX];
    int k;
    for (k = 0; k < topo_.n; k++) t[k] = b[k];
    for (k = 0; k < nanc_; k++) t[anc_[k]] = cx(0.0, 0.0);      /* las anclas: phi = 0 */
    luc_resuelve(&luc_, t, x);
}

/* ==========================================================================
 *  3. Valores, potencias y lecturas
 * ======================================================================= */
Cx ca_phi(const Cx *x, int n)
{
    int cn = cph(n);
    return cn >= 0 ? x[cn] : cx(0.0, 0.0);
}

void ca_vi(const SimRed *s, const CaExc *e, const Cx *x, double f, int k, Cx *V, Cx *I)
{
    const SimEl *el = &s->el[k];
    Cx pa = ca_phi(x, el->a), pb = ca_phi(x, el->b), U = ((e->activa >> k) & 1u) ? e->U[k] : cx(0.0, 0.0);
    int j = topo_.col_br[k];
    switch (el->tipo) {
    case SE_E:
        *V = cx_menos(pb, pa);
        *I = j >= 0 ? x[j] : cx_esc(cx_menos(U, *V), 1.0 / el->val);
        break;
    case SE_J:
        *V = cx_menos(pb, pa); *I = U;
        break;
    case SE_EC:                                       /* como una fuente: bornes, sale por + */
        *V = cx_menos(pb, pa); *I = x[j];
        break;
    case SE_JC:
        *V = cx_menos(pb, pa); *I = cx_esc(ctl_valor(s, e, x, k), el->val);
        break;
    case SE_VMI:
        *V = el->instr == 'j' ? cx_menos(pb, pa) : cx_menos(pa, pb);   /* J apagada: bornes */
        *I = cx(0.0, 0.0);
        break;
    case SE_AMI:
        *V = el->instr == 'W' ? pb : cx_menos(pa, pb);   /* W: su bobina de tensión */
        *I = x[j];
        break;
    case SE_R:
        *V = cx_menos(pa, pb); *I = cx_esc(*V, 1.0 / el->val);
        break;
    case SE_Z:
        *V = cx_menos(pa, pb); *I = cx_div(*V, ca_z_de(el, f));
        break;
    case SE_AO: {                                     /* la salida: Vsal y la I que entrega por c */
        Cx vc = ca_phi(x, el->c), vr = ca_phi(x, el->nref);
        int lin = !(lin_act && lin_act->hay) || lin_act->tramo[k] == 0;   /* saturado: u fija, 0 en señal */
        *V = cx_menos(vc, vr);
        *I = j >= 0 ? x[j] : cx_esc(cx_menos(lin ? cx_esc(cx_menos(pa, pb), el->m[0]) : cx(0.0, 0.0), *V), 1.0 / el->m[4]);
        break;
    }
    case SE_D:                                        /* por tramos, su rama; la unión, G en Q */
        *V = cx_menos(pa, pb);
        *I = j >= 0 ? x[j] : cx_esc(*V, lin_act && lin_act->hay ? lin_act->g[k][0][0] : 1e-12);
        break;
    case SE_BJT:                                      /* V = VCE e I = IC, con sus g en Q */
    case SE_MOS: {                                    /* V = VDS e I = ID = gm·vgs + gds·vds */
        Cx vbe = cx_menos(ca_phi(x, el->c), pb);
        *V = cx_menos(pa, pb);
        *I = lin_act && lin_act->hay ? cx_mas(cx_esc(vbe, lin_act->g[k][1][0]), cx_esc(*V, lin_act->g[k][1][1]))
                                     : cx(0.0, 0.0);
        break;
    }
    default:                                          /* C, L, S; D por tramos (su rama) */
        *V = cx_menos(pa, pb); *I = j >= 0 ? x[j] : cx(0.0, 0.0);
        break;
    }
}

/* fp con carácter: + inductivo (P y Q del mismo signo), - capacitivo; 0 si
 * S = 0. Una Q de redondeo (1e-12 de |S|) cuenta como 0 */
static double fp_de(Cx S)
{
    double m = cx_abs(S), P = S.re, Q = S.im;
    if (!(m > 0.0)) return 0.0;
    if (fabs(Q) <= 1e-12 * m) Q = 0.0;
    return (P * Q >= 0.0 ? 1.0 : -1.0) * fabs(P) / m;
}

void ca_valores(const SimRed *s, const CaExc *e, const Cx *x, double f, CaRes *res)
{
    double suma = 0.0, nanv = cero_ / cero_;          /* NaN: sin lectura */
    int k, n, nact = 0, fu = -1;
    res->f = f;
    res->w = 2.0 * PI_CA * f;
    res->activa = e->activa;
    res->Sfuentes = res->Scargas = cx(0.0, 0.0);
    for (n = 0; n < s->nn; n++) res->phi[n] = ca_phi(x, n);
    for (k = 0; k < s->nel; k++) {
        const SimEl *el = &s->el[k];
        Cx V, I, S;
        ca_vi(s, e, x, f, k, &V, &I);
        S = cx_esc(cx_por(V, cx_conj(I)), 0.5);
        res->V[k] = V; res->I[k] = I; res->S[k] = S;
        res->Z[k] = ca_z_de(el, f);
        res->lectura[k] = nanv;
        res->fp[k] = 0.0;
        res->val[k] = 0.0; res->val_ang[k] = 0.0;
        switch (el->tipo) {
        case SE_E:
        case SE_J:
            res->Sfuentes = cx_mas(res->Sfuentes, S);
            suma += cx_abs(S);
            res->val[k] = cx_abs(e->U[k]) / RAIZ2;
            res->val_ang[k] = cx_arg(e->U[k]) * 180.0 / PI_CA;
            res->fp[k] = fp_de(S);
            if ((e->activa >> k) & 1u) { nact++; fu = k; }
            break;
        case SE_AO:                                   /* entrega por la salida y absorbe en Ri */
            res->Sfuentes = cx_mas(res->Sfuentes, S);
            suma += cx_abs(S);
            if (el->modelo == NL_AO_REAL && el->m[3] > 0.0) {
                Cx vd = cx_menos(ca_phi(x, el->a), ca_phi(x, el->b));
                Cx si = cx_esc(cx_por(vd, cx_conj(cx_esc(vd, 1.0 / el->m[3]))), 0.5);
                res->Scargas = cx_mas(res->Scargas, si);
                suma += cx_abs(si);
            }
            break;
        case SE_EC:
        case SE_JC:                                   /* entregan (Tellegen); su valor, la ganancia */
            res->Sfuentes = cx_mas(res->Sfuentes, S);
            suma += cx_abs(S);
            res->val[k] = el->val;
            res->fp[k] = fp_de(S);
            break;
        case SE_BJT: {                                /* absorbe por sus dos puertos: C-E y B-E */
            Cx vbe = cx_menos(res->phi[el->c], res->phi[el->b]), ib = cx(0.0, 0.0), s2;
            if (lin_act && lin_act->hay) ib = cx_mas(cx_esc(vbe, lin_act->g[k][0][0]), cx_esc(V, lin_act->g[k][0][1]));
            s2 = cx_esc(cx_por(vbe, cx_conj(ib)), 0.5);
            res->S[k] = cx_mas(S, s2);
            res->Scargas = cx_mas(res->Scargas, res->S[k]);
            suma += cx_abs(S) + cx_abs(s2);
            break;
        }
        case SE_MOS:                                  /* absorbe por D-S (la puerta no lleva corriente) */
            res->Scargas = cx_mas(res->Scargas, S);
            suma += cx_abs(S);
            break;
        case SE_R: case SE_C: case SE_L: case SE_Z: case SE_D:
            res->Scargas = cx_mas(res->Scargas, S);
            suma += cx_abs(S);
            if (el->tipo == SE_Z) { res->val[k] = cx_abs(res->Z[k]); res->val_ang[k] = cx_arg(res->Z[k]) * 180.0 / PI_CA; }
            else res->val[k] = el->val;
            if (el->instr == 'V') res->lectura[k] = cx_abs(V) / RAIZ2;
            else if (el->instr == 'A') res->lectura[k] = cx_abs(I) / RAIZ2;
            break;
        case SE_AMI:
            if (el->instr == 'W') { res->lectura[k] = S.re; res->fp[k] = fp_de(S); }
            else res->lectura[k] = cx_abs(I) / RAIZ2;
            break;
        case SE_VMI:
            if (el->instr == 'V') res->lectura[k] = cx_abs(V) / RAIZ2;
            break;
        default:
            break;
        }
    }
    res->tellegen = suma > 0.0 ? cx_abs(cx_menos(res->Sfuentes, res->Scargas)) / suma : 0.0;
    res->fuente = nact == 1 ? fu : -1;
    res->Zeq = cx(0.0, 0.0);
    if (fu >= 0 && nact == 1 && cx_abs(res->I[fu]) > 0.0) res->Zeq = cx_div(res->V[fu], res->I[fu]);
    ops_ += (unsigned long)(30 * s->nel);
}

int ca_resuelve_red(const SimRed *s, double f, const CaExc *x, CaRes *res)
{
    static Cx b[LUC_MAX], xs[LUC_MAX];
    SimDiag dt;
    unsigned av = res->d.avisos;
    int ea = res->d.elem_aviso, e;

    e = ca_topologia(s, &dt);
    dt.avisos |= av;
    if (dt.elem_aviso < 0) dt.elem_aviso = ea;
    if (e != SIM_OK) { res->d = dt; return e; }
    e = ca_ensambla(s, f, &dt);
    if (e != SIM_OK) { res->d = dt; res->f = f; res->w = 2.0 * PI_CA * f; return e; }
    ca_b_fuentes(s, x, b);
    ca_sustituye(b, xs);
    ca_valores(s, x, xs, f, res);
    dt.estado = SIM_OK;
    res->d = dt;
    return SIM_OK;
}

/* ==========================================================================
 *  3b. Sensibilidades
 *
 *  A(u)·x = b(u)  =>  A·dx + (dA/du)·x = db/du  =>  dx = -A⁻¹·v, con
 *  v = (dA/du)·x - db/du. La matriz ya está factorizada (la de la
 *  evaluación): cada incógnita cuesta una sustitución más, no otra LU. Es lo
 *  que hace exacto el jacobiano de la calculadora sin diferencias finitas.
 * ======================================================================= */
/* La escala de la X de una Z a f (la de ca_z_de): 1 a su f0; f/f0 si X > 0
 * (como una L) y f0/f si X < 0 (como una C) */
static double escala_z(const SimEl *e, double f)
{
    double X = e->p[0], f0 = e->p[1];
    if (X > 0.0 && f != f0) return f / f0;
    if (X < 0.0 && f != f0) return f0 / f;
    return 1.0;
}

/* dY/du de una Z: -Y²·dZ/du */
static Cx dy_de_z(const SimEl *e, double f, int que)
{
    Cx Y = cx_inv(ca_z_de(e, f)), dZ;
    switch (que) {
    case IN_VAL:                                /* |Z| escala R y X por igual: dY = -Y */
        return cx(-Y.re, -Y.im);
    case IN_ANGZ:                               /* R = |Z|cos θ, X = |Z|sen θ·esc */
        dZ = cx(-e->p[0], e->val * escala_z(e, f));
        break;
    case IN_F: {                                /* solo la que escala: X·f/f0 o X·f0/f */
        double X = ca_z_de(e, f).im;
        if (e->p[1] == f) return cx(0.0, 0.0);
        dZ = cx(0.0, e->p[0] > 0.0 ? X : -X);
        break;
    }
    default:
        return cx(0.0, 0.0);
    }
    return cx_esc(cx_por(cx_por(Y, Y), dZ), -1.0);
}

/* Lo que una admitancia que cambia en dY añade a v: dY·(phi_a - phi_b) sale
 * de a y entra en b (las filas de y_ab) */
static void v_admitancia(Cx *v, int a, int b, Cx dY, Cx dphi)
{
    Cx t = cx_por(dY, dphi);
    int ca = cph(a), cb = cph(b);
    if (ca >= 0) v[ca] = cx_mas(v[ca], t);
    if (cb >= 0) v[cb] = cx_menos(v[cb], t);
}

void ca_derivada(const SimRed *s, const CaExc *e, const Cx *x, double f, int k, int que, Cx *v)
{
    double w = 2.0 * PI_CA * f;
    int i, n;
    for (i = 0; i < topo_.n; i++) v[i] = cx(0.0, 0.0);
    for (i = 0; i < s->nel; i++) {
        const SimEl *el = &s->el[i];
        int j = topo_.col_br[i];
        Cx dphi, dU;
        if (que != IN_F && i != k) continue;    /* con la f cambian todos los reactivos */
        dphi = cx_menos(ca_phi(x, el->a), ca_phi(x, el->b));
        switch (el->tipo) {
        case SE_R:                              /* Y = 1/R: dY/d(ln R) = -1/R */
            if (que == IN_VAL) v_admitancia(v, el->a, el->b, cx(-1.0 / el->val, 0.0), dphi);
            break;
        case SE_C:                              /* fila j: i - jωC·Δφ; con ln C o ln f, -jωC·Δφ */
            if (que == IN_VAL || que == IN_F) v[j] = cx_menos(v[j], cx_j(cx_esc(dphi, w * el->val)));
            break;
        case SE_L:                              /* fila j: Δφ - jωL·i; con ln L o ln f, -jωL·i */
            if (que == IN_VAL || que == IN_F) v[j] = cx_menos(v[j], cx_j(cx_esc(x[j], w * el->val)));
            break;
        case SE_Z:
            if (que == IN_VAL || que == IN_ANGZ || que == IN_F) v_admitancia(v, el->a, el->b, dy_de_z(el, f, que), dphi);
            break;
        case SE_E:
        case SE_J:
            if ((que != IN_MODU && que != IN_ANGU) || !((e->activa >> i) & 1u)) break;
            dU = que == IN_MODU ? e->U[i] : cx_j(e->U[i]);         /* U·e^u o U·e^{ju} */
            if (el->tipo == SE_E && j >= 0) v[j] = cx_menos(v[j], dU);   /* b[j] = U */
            else {                                                       /* inyección de a hacia b */
                Cx I = el->tipo == SE_E ? cx_esc(dU, 1.0 / el->val) : dU;
                int ca = cph(el->a), cb = cph(el->b);
                if (ca >= 0) v[ca] = cx_mas(v[ca], I);
                if (cb >= 0) v[cb] = cx_menos(v[cb], I);
            }
            break;
        default:
            break;
        }
    }
    /* Las controladas que manda el elemento k por su I: con ln R cambian sus
     * coeficientes (la I de la R es V/R); con la U de una E real o de una J,
     * su parte conocida */
    if (k >= 0 && que != IN_F && que != IN_ANGZ)
        for (i = 0; i < s->nel; i++) {
            const SimEl *el = &s->el[i];
            const SimEl *x0 = &s->el[k];
            Cx d = cx(0.0, 0.0);                    /* d(control)/du */
            int j = topo_.col_br[i], ca = cph(el->a), cb = cph(el->b);
            if ((el->tipo != SE_EC && el->tipo != SE_JC) || el->ctl != k || !el->ctl_i) continue;
            if (x0->tipo == SE_R && que == IN_VAL) {
                /* los coeficientes son ±1/R: d/du = -c; (dA/du)·x = g·I_R en la fila de la EC */
                Cx IR = cx_esc(cx_menos(ca_phi(x, x0->a), ca_phi(x, x0->b)), 1.0 / x0->val);
                if (el->tipo == SE_EC) v[j] = cx_mas(v[j], cx_esc(IR, el->val));
                else {
                    if (ca >= 0) v[ca] = cx_menos(v[ca], cx_esc(IR, el->val));
                    if (cb >= 0) v[cb] = cx_mas(v[cb], cx_esc(IR, el->val));
                }
                continue;
            }
            if ((que == IN_MODU || que == IN_ANGU) && ((e->activa >> k) & 1u) &&
                (x0->tipo == SE_J || (x0->tipo == SE_E && topo_.col_br[k] < 0))) {
                Cx dU = que == IN_MODU ? e->U[k] : cx_j(e->U[k]);
                d = x0->tipo == SE_E ? cx_esc(dU, 1.0 / x0->val) : dU;
                d = cx_esc(d, el->val);             /* db/du = g·dc0: v -= db/du */
                if (el->tipo == SE_EC) v[j] = cx_menos(v[j], d);
                else {
                    if (ca >= 0) v[ca] = cx_mas(v[ca], d);
                    if (cb >= 0) v[cb] = cx_menos(v[cb], d);
                }
            }
        }
    for (n = 0; n < s->nn; n++)                 /* las filas ancladas son phi = 0: no cambian */
        if (topo_.col_phi[n] >= 0 && topo_.ancla[n]) v[topo_.col_phi[n]] = cx(0.0, 0.0);
    ops_ += (unsigned long)(que == IN_F ? 8 * s->nel : 8);
}

void ca_dvi(const SimRed *s, const CaExc *e, const Cx *x, const Cx *dx, double f,
            int m, int k, int que, Cx *dV, Cx *dI)
{
    const SimEl *el = &s->el[m];
    Cx dpa = ca_phi(dx, el->a), dpb = ca_phi(dx, el->b), dU = cx(0.0, 0.0), V, I;
    int j = topo_.col_br[m];
    if (m == k && ((e->activa >> m) & 1u)) {
        if (que == IN_MODU) dU = e->U[m];
        else if (que == IN_ANGU) dU = cx_j(e->U[m]);
    }
    switch (el->tipo) {
    case SE_E:
        *dV = cx_menos(dpb, dpa);
        *dI = j >= 0 ? dx[j] : cx_esc(cx_menos(dU, *dV), 1.0 / el->val);   /* real: (U - V)/r */
        break;
    case SE_J:
        *dV = cx_menos(dpb, dpa);
        *dI = dU;
        break;
    case SE_EC:
        *dV = cx_menos(dpb, dpa);
        *dI = dx[j];
        break;
    case SE_JC: {
        /* g·(la parte del control que pasa por dx + la explícita de la incógnita) */
        int col[3], i, nt, fu;
        double c[3], f0;
        Cx d = cx(0.0, 0.0);
        nt = sim_ctl_coef(s, &mapa_, m, col, c, &fu, &f0);
        for (i = 0; i < nt; i++) if (col[i] >= 0) d = cx_mas(d, cx_esc(dx[col[i]], c[i]));
        if (el->ctl == k && el->ctl_i && k >= 0) {
            const SimEl *x0 = &s->el[k];
            if (x0->tipo == SE_R && que == IN_VAL)          /* I = V/R: con ln R, -I */
                d = cx_menos(d, cx_esc(cx_menos(ca_phi(x, x0->a), ca_phi(x, x0->b)), 1.0 / x0->val));
            else if (fu == k && ((e->activa >> k) & 1u) && (que == IN_MODU || que == IN_ANGU))
                d = cx_mas(d, cx_esc(que == IN_MODU ? e->U[k] : cx_j(e->U[k]), f0));
        }
        *dV = cx_menos(dpb, dpa);
        *dI = cx_esc(d, el->val);
        break;
    }
    case SE_VMI:
        *dV = el->instr == 'j' ? cx_menos(dpb, dpa) : cx_menos(dpa, dpb);
        *dI = cx(0.0, 0.0);
        break;
    case SE_AMI:
        *dV = el->instr == 'W' ? dpb : cx_menos(dpa, dpb);
        *dI = dx[j];
        break;
    case SE_R:
        *dV = cx_menos(dpa, dpb);
        *dI = cx_esc(*dV, 1.0 / el->val);
        if (m == k && que == IN_VAL) {          /* I = V/R: con ln R, la explícita es -I */
            ca_vi(s, e, x, f, m, &V, &I);
            *dI = cx_menos(*dI, I);
        }
        break;
    case SE_Z:
        *dV = cx_menos(dpa, dpb);
        *dI = cx_por(*dV, cx_inv(ca_z_de(el, f)));
        if ((m == k && (que == IN_VAL || que == IN_ANGZ)) || que == IN_F) {   /* I = V·Y: + V·dY */
            ca_vi(s, e, x, f, m, &V, &I);
            *dI = cx_mas(*dI, cx_por(V, dy_de_z(el, f, que)));
        }
        break;
    default:                                    /* C, L, S: la corriente de su rama */
        *dV = cx_menos(dpa, dpb);
        *dI = dx[j];
        break;
    }
    ops_ += 10;
}

/* ==========================================================================
 *  4. Desde el Circuito
 * ======================================================================= */
int ca_punto_q(const SimRed *s, SimLin *L, SimDiag *d)
{
    SimDiag dq;
    int k, e;
    memset(L, 0, sizeof *L);
    if (!sim_hay_nl(s)) return SIM_OK;
    e = sim_punto_q(s, L, &dq, 0);
    if (e != SIM_OK) { *d = dq; return e; }
    aviso(d, SIM_AV_PEQ_SENAL, -1);
    for (k = 0; k < s->nel; k++)
        if (s->el[k].tipo == SE_AO && L->tramo[k] != 0) aviso(d, SIM_AV_SATURA, k);
    return SIM_OK;
}

static int analiza_con(const Circuito *c, const Red *r, double f_forzada, CaRes *res)
{
    static SimRed s;
    CaExc x;
    double f;
    int k, e;

    memset(res, 0, sizeof *res);
    res->d.elem = res->d.nodo = res->d.elem_aviso = -1;
    res->fuente = -1;
    if (sim_red_de_ex(c, r, &s, &res->d, 0) != SIM_OK) { res->d.elem_aviso = -1; return res->d.estado; }
    res->d.elem_aviso = -1;
    res->d.t = -1.0;
    /* el punto Q con las fuentes de CC, antes de apagarlas (§3.I) */
    if (ca_punto_q(&s, &lin_ca, &res->d) != SIM_OK) return res->d.estado;
    ca_pon_q(lin_ca.hay ? &lin_ca : 0);
    if (f_forzada > 0.0)
        for (k = 0; k < s.nel; k++)
            if ((s.el[k].tipo == SE_E || s.el[k].tipo == SE_J) && s.el[k].forma == ONDA_SENO) s.el[k].p[1] = f_forzada;
    f = ca_frecuencia(&s, f_forzada > 0.0 ? f_forzada : (c->frec.est == M_DATO ? c->frec.v : 0.0), &res->d);
    if (res->d.estado != SIM_OK) { ca_pon_q(0); return res->d.estado; }
    ca_excitacion(&s, f, &x, &res->d);
    if (res->d.estado != SIM_OK) { ca_pon_q(0); return res->d.estado; }
    ca_apaga(&s, x.activa);
    e = ca_resuelve_red(&s, f, &x, res);
    if (lin_ca.hay) memcpy(res->reg, lin_ca.reg, sizeof res->reg);
    ca_pon_q(0);
    return e;
}

int ca_analiza(const Circuito *c, const Red *r, CaRes *res) { return analiza_con(c, r, 0.0, res); }
int ca_analiza_f(const Circuito *c, const Red *r, double f, CaRes *res) { return analiza_con(c, r, f, res); }

int ca_valida(const Circuito *c, SimDiag *d, double *f)
{
    double fc = 0.0;
    int k, primera = -1;
    *f = 0.0;
    if (sim_valida(c, d) != SIM_OK) return d->estado;
    for (k = 0; k < c->nel; k++) {
        const Elemento *e = &c->el[k];
        double fk;
        if (e->tipo != EL_FUENTE && e->tipo != EL_CORR) continue;
        if (e->variante == FORMA_FASOR) fk = c->frec.v;
        else if (e->variante == ONDA_SENO) fk = e->par[Q_FREC - Q_P1].v;
        else if (e->variante == ONDA_CC) continue;
        else { d->estado = SIM_CA_FORMA; d->elem = k; return d->estado; }
        if (fc == 0.0) { fc = fk; primera = k; }
        else if (fabs(fk - fc) > CA_TOL_F * fc) {
            d->estado = SIM_CA_FRECS; d->elem = k; d->lazo = (1u << primera) | (1u << k);
            return d->estado;
        }
    }
    if (fc == 0.0) {
        if (c->frec.est == M_DATO) fc = c->frec.v;
        else { d->estado = SIM_CA_SIN_F; return d->estado; }
    }
    *f = fc;
    return SIM_OK;
}

/* ==========================================================================
 *  5. Thévenin, Norton y ohmímetro
 * ======================================================================= */
int ca_thevenin(const Circuito *c, const Red *r, double f, CaThev *t)
{
    static SimRed s;
    int na, nb;
    memset(t, 0, sizeof *t);
    t->d.elem = t->d.nodo = t->d.elem_aviso = -1;
    t->d.t = -1.0;
    t->f = f; t->na = t->nb = -1;
    if (c->term_x[0] < 0 || c->term_y[0] < 0 || c->term_x[1] < 0 || c->term_y[1] < 0) {
        t->d.estado = SIM_SIN_AB; return t->d.estado;
    }
    na = r->punto[c->term_y[0] * REJ_COLS + c->term_x[0]];
    nb = r->punto[c->term_y[1] * REJ_COLS + c->term_x[1]];
    if (na < 0 || nb < 0) { t->d.estado = SIM_SIN_AB; return t->d.estado; }
    if (r->comp[na] != r->comp[nb]) { t->d.estado = SIM_AB_SEPARADOS; t->na = na; t->nb = nb; return t->d.estado; }
    if (sim_red_de_ex(c, r, &s, &t->d, 0) != SIM_OK) return t->d.estado;
    return ca_thevenin_red(&s, na, nb, f, t);
}

/* La mayor |Z| de la red a f (la escala de "Z nula"); 1 si no hay */
static double zesc_de(const SimRed *s, double f)
{
    double m = 0.0;
    int k;
    for (k = 0; k < s->nel; k++) {
        const SimEl *e = &s->el[k];
        double z;
        if (e->tipo != SE_R && e->tipo != SE_C && e->tipo != SE_L && e->tipo != SE_Z && !(e->tipo == SE_E && e->val > 0.0)) continue;
        z = e->tipo == SE_E ? e->val : (f > 0.0 ? cx_abs(ca_z_de(e, f)) : (e->tipo == SE_R ? e->val : 0.0));
        if (z > m) m = z;
    }
    return m > 0.0 ? m : 1.0;
}

int ca_thevenin_red(const SimRed *s, int na, int nb, double f, CaThev *t)
{
    static SimRed s2;
    static Cx b[LUC_MAX], x[LUC_MAX];
    CaExc ex;
    SimDiag dt;
    double w = 2.0 * PI_CA * f, zesc, Rth;
    int e;

    t->f = f; t->na = na; t->nb = nb;
    t->d.estado = SIM_OK;
    if (na < 0 || nb < 0 || na >= s->nn || nb >= s->nn) { t->d.estado = SIM_SIN_AB; return t->d.estado; }
    if (sim_hay_nl(s)) {                                  /* Thévenin es de redes lineales (D22) */
        int k;
        for (k = 0; k < s->nel && !(s->el[k].tipo == SE_D || s->el[k].tipo >= SE_AO); k++) {}
        t->d.estado = SIM_NO_LINEAL; t->d.elem = k;
        return t->d.estado;
    }

    if (!(f > 0.0)) {                                     /* CC */
        double vth, rth;
        int inf;
        e = sim_thevenin_cc(s, na, nb, &vth, &rth, &inf, &t->d);
        if (e != SIM_OK) return e;
        t->Vth = cx(vth, 0.0);
        if (inf) { t->inf = 1; t->hay_th = 0; t->hay_no = 0; return SIM_OK; }
        t->hay_th = 1;
        zesc = zesc_de(s, 0.0);
        if (fabs(rth) <= 1e-12 * zesc) { t->Zth = cx(0.0, 0.0); t->hay_no = 0; t->max_finito = 0; return SIM_OK; }
        t->Zth = cx(rth, 0.0);
        t->hay_no = 1;
        t->In = cx(vth / rth, 0.0);
        t->max_finito = 1;
        t->Zl = cx(rth, 0.0);
        t->Pmax = vth * vth / (4.0 * rth);
        t->Rsola = rth; t->Psola = t->Pmax;
        return SIM_OK;
    }

    s2 = *s;
    memset(&dt, 0, sizeof dt);
    dt.elem = dt.nodo = dt.elem_aviso = -1;
    ca_excitacion(&s2, f, &ex, &dt);
    if (dt.estado != SIM_OK) { t->d = dt; return dt.estado; }
    ca_apaga(&s2, ex.activa);
    {
        unsigned av = dt.avisos;
        e = ca_topologia(&s2, &dt);
        dt.avisos |= av;
    }
    if (e != SIM_OK) { t->d = dt; return e; }
    if (topo_.pieza[na] != topo_.pieza[nb]) { dt.estado = SIM_AB_SEPARADOS; t->d = dt; return dt.estado; }
    e = ca_ensambla(&s2, f, &dt);
    if (e != SIM_OK) { t->d = dt; return e; }
    ca_b_fuentes(&s2, &ex, b);
    ca_sustituye(b, x);
    t->Vth = cx_menos(ca_phi(x, na), ca_phi(x, nb));
    {
        int k;
        for (k = 0; k < topo_.n; k++) b[k] = cx(0.0, 0.0);
    }
    ca_b_inyecta(na, nb, cx(1.0, 0.0), b);           /* 1 A que entra por A y sale por B */
    ca_sustituye(b, x);
    t->Zth = cx_menos(ca_phi(x, na), ca_phi(x, nb));
    t->d = dt;
    t->d.estado = SIM_OK;
    t->hay_th = 1;
    zesc = zesc_de(&s2, f);
    if (cx_abs(t->Zth) <= 1e-12 * zesc) { t->Zth = cx(0.0, 0.0); t->hay_no = 0; t->max_finito = 0; return SIM_OK; }
    t->hay_no = 1;
    t->In = cx_div(t->Vth, t->Zth);
    Rth = t->Zth.re;
    if (Rth > 1e-12 * cx_abs(t->Zth)) {
        double Xl = -t->Zth.im, vm2 = cx_abs(t->Vth) * cx_abs(t->Vth);
        t->max_finito = 1;
        t->Zl = cx_conj(t->Zth);
        t->Pmax = vm2 / (8.0 * Rth);                   /* pico: ½·|V|²/(4R) */
        if (Xl > 1e-12 * cx_abs(t->Zth)) t->Lc = Xl / w;
        else if (Xl < -1e-12 * cx_abs(t->Zth)) t->Cc = -1.0 / (w * Xl);
        t->Rsola = cx_abs(t->Zth);
        {
            double d2 = cx_abs(cx_mas(t->Zth, cx(t->Rsola, 0.0)));
            t->Psola = 0.5 * vm2 * t->Rsola / (d2 * d2);
        }
    } else t->max_finito = 0;
    ops_ += 40;
    return SIM_OK;
}

/* ==========================================================================
 *  6. Convenios y avisos
 * ======================================================================= */
void ca_a_conv(int conv, Cx X, double *mod, double *ang_grados)
{
    *mod = cx_abs(X) * ((conv & CONV_PICO) ? 1.0 : 1.0 / RAIZ2);
    *ang_grados = num_ang(cx_arg(X) * 180.0 / PI_CA + ((conv & CONV_SENO) ? 90.0 : 0.0));
}

Cx ca_de_conv(int conv, double mod, double ang_grados)
{
    double m = mod * ((conv & CONV_PICO) ? 1.0 : RAIZ2);
    double a = ang_grados - ((conv & CONV_SENO) ? 90.0 : 0.0);
    return cx_polar(m, a * PI_CA / 180.0);
}

void ca_canon_a_conv(int conv, double ef, double ang, double *mod, double *a)
{
    *mod = (conv & CONV_PICO) ? ef * RAIZ2 : ef;
    *a = num_ang(ang + ((conv & CONV_SENO) ? 90.0 : 0.0));
}

void ca_conv_a_canon(int conv, double mod, double a, double *ef, double *ang)
{
    *ef = (conv & CONV_PICO) ? mod / RAIZ2 : mod;
    *ang = num_ang(a - ((conv & CONV_SENO) ? 90.0 : 0.0));
}

/* El primer elemento del circuito que da el aviso (para nombrarlo) */
static int quien(const Circuito *c, unsigned bit)
{
    int k;
    if (!c) return -1;
    for (k = 0; k < c->nel; k++) {
        const Elemento *e = &c->el[k];
        switch (bit) {
        case SIM_AV_CC_FUERA:
            if ((e->tipo == EL_FUENTE || e->tipo == EL_CORR) &&
                ((e->variante == ONDA_CC && e->val.est == M_DATO && e->val.v != 0.0) ||
                 (e->variante == ONDA_SENO && e->par[Q_BASE - Q_P1].est == M_DATO && e->par[Q_BASE - Q_P1].v != 0.0)))
                return k;
            break;
        case SIM_AV_Z_ESCALA:
            if (e->tipo == EL_IMP) return k;
            break;
        default:
            break;
        }
    }
    return -1;
}

const char *ca_aviso(const Circuito *c, const CaRes *s, unsigned bit, char *buf, int cap)
{
    Buf b;
    char u[24];
    int k = quien(c, bit);
    const char *nom = (c && k >= 0) ? c->el[k].etiq : 0;
    buf_init(&b, buf, (size_t)cap);
    switch (bit) {
    case SIM_AV_CC_FUERA:
        buf_str(&b, nom ? nom : "Una fuente");
        buf_str(&b, " es continua (o tiene nivel de CC): en fasores cuenta apagada. Todo junto se ve emulando en el tiempo.");
        break;
    case SIM_AV_S_FINAL:
        buf_str(&b, "Los interruptores cuentan en su estado final.");
        break;
    case SIM_AV_CI_NO:
        buf_str(&b, "V0 e I0 no cuentan: esto es el r" S_ee "gimen permanente.");
        break;
    case SIM_AV_FLOTA:
        buf_str(&b, "Una parte no toca la referencia: sus potenciales se miden desde un punto suyo.");
        break;
    case SIM_AV_Z_ESCALA:
        buf_str(&b, nom ? nom : "La Z");
        buf_str(&b, " est" S_aa " dada a ");
        buf_str(&b, (c && c->frec.est == M_DATO) ? num_pref(u, sizeof u, c->frec.v, 4, "Hz") : "la f del circuito");
        buf_str(&b, ": a otra f se toma como R y L (o R y C).");
        break;
    case SIM_AV_OTRAS:
        buf_str(&b, "Las dem" S_aa "s fuentes se apagan: el Bode es de ");
        buf_str(&b, (c && s && s->fuente >= 0 && s->fuente < c->nel) ? c->el[s->fuente].etiq : "la entrada");
        buf_str(&b, " a la salida.");
        break;
    case SIM_AV_PEQ_SENAL:
        buf_str(&b, "Peque" S_nn "a se" S_nn "al: no lineales linealizados en Q.");
        break;
    case SIM_AV_SATURA: {                       /* el AO de elem_aviso (ca_punto_q); si no, el primero */
        int ka = s ? s->d.elem_aviso : -1, i;
        if (!c || ka < 0 || ka >= c->nel || c->el[ka].tipo != EL_AO)
            for (ka = -1, i = 0; c && i < c->nel; i++) if (c->el[i].tipo == EL_AO) { ka = i; break; }
        buf_str(&b, ka >= 0 ? c->el[ka].etiq : "Un AO");
        buf_str(&b, " satura en Q: en peque" S_nn "a se" S_nn "al su salida es 0.");   /* 52 columnas */
        break;
    }
    default:
        break;
    }
    return buf;
}
