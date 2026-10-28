/* ============================================================================
 *  nolin.c  -  Los modelos no lineales, sin estado  (ver nolin.h)
 *
 *  Orden del archivo:
 *    1. catálogo y contador de operaciones
 *    2. la exponencial con recta y los límites de SPICE3 (pnjlim, fetlim,
 *       limvds y los del MOS como mos1load)
 *    3. el diodo por tramos: rectas, márgenes y vecinos (D3, D5)
 *    4. la unión exponencial con Rs condensada (D4, D5)
 *    5. el BJT (Ebers-Moll de transporte con Early, D9) y el MOSFET
 *       (Shichman-Hodges, D10)
 *    6. la linealización en puertos (nl_evalua) y el arranque
 *    7. el AO, las regiones y la escala
 *
 *  Los m[] de cada modelo son los del contrato del §2.6: el adaptador (sim.c)
 *  pone los vacíos, el catálogo y las cuentas; aquí solo se leen números.
 *    NL_TRAMOS  Vγ, Ron (> 0), Vz0 (0 = sin ruptura), Rz (> 0)
 *    NL_UNION   Is, n, Rs, BV (0 = sin ruptura), Ibv, Vcrit
 *    NL_AO_*    A (ideal: 0), V+, V-, Ri (0 = infinita), Ro
 *    NL_BJT     Is, βF, βR, VA (0 = sin Early), -, Vcrit
 *    NL_MOS     Vt (>= 0), K, VA (0 = sin modulación del canal)
 * ========================================================================= */
#include <string.h>
#include <math.h>
#include "nolin.h"
#include "gfx.h"

#define RAIZ2_NL 1.41421356237309504880

/* ==========================================================================
 *  1. Catálogo y contador
 * ======================================================================= */
/* Los modelos de LTspice (standard.dio), reducidos a Is, n y Rs (§3.A) */
const NlDiodo NL_1N4148 = { "1N4148", 2.52e-9, 1.752, 0.568 };
const NlDiodo NL_1N4007 = { "1N4007", 7.02767e-9, 1.80803, 0.0341512 };

/* D6: la Vf de cada color a If (20 mA por omisión), su n y su Rs; el rgb es el
 * del LED encendido (§2.12). La Is la calcula el adaptador para que la curva
 * pase justo por (Vf, If) con GMIN incluida. */
const NlColor NL_COLOR[LED_N] = {
    { 1.8, 2.0,  5.0, RGB(255,  50,  40) },      /* rojo     */
    { 2.0, 2.0,  5.0, RGB(255, 140,  30) },      /* naranja  */
    { 2.1, 2.0,  5.0, RGB(255, 230,  40) },      /* amarillo */
    { 2.2, 2.0,  5.0, RGB( 60, 235,  80) },      /* verde    */
    { 3.2, 3.0, 15.0, RGB( 60, 120, 255) },      /* azul     */
    { 3.2, 3.0, 15.0, RGB(245, 245, 255) }       /* blanco   */
};

/* Operaciones de coma flotante, para el modelo de tiempos (sim_ops las suma).
 * Una exp() o un log() cuentan NL_OPS_EXP (medido con BN_EXP); lo demás, a
 * ojo, como en sim.c (la tabla del §3.L). exps_: cuántas van (el arnés
 * cuenta exp() sin depender de NL_OPS_EXP). */
static unsigned long ops_, exps_;

unsigned long nl_ops(void)       { return ops_; }
unsigned long nl_exps(void)      { return exps_; }
void          nl_ops_reset(void) { ops_ = exps_ = 0; }

static double pol_de(const SimEl *e) { return e->pol < 0 ? -1.0 : 1.0; }

/* ==========================================================================
 *  2. La exponencial y los límites de SPICE3
 * ======================================================================= */
/* e^x. Por encima de NL_XLIM sigue por su tangente, e^80·(1 + x - 80): una
 * iteración que se pasa de largo no da infinito, y la curva sigue continua y
 * creciente, con derivada continua. Por debajo de -40 no se llama a exp():
 * e^-40 = 4,2e-18 se pierde contra el 1 de Is·(e^x - 1) (media ulp de 1 es
 * 1,1e-16), así que la corriente de la unión sale igual bit a bit, y su
 * derivada queda muy por debajo de GMIN (con Is = 1e-3, el tope de la ficha,
 * 3e-19 S frente a 1e-12 S): se toma 0. Es la «inversa profunda sin exp» del
 * §8.1. */
double nl_exp(double x, double *d)
{
    double e;
    if (x < -40.0) { *d = 0.0; return 0.0; }
    ops_ += NL_OPS_EXP; exps_++;
    if (x > NL_XLIM) {
        e = exp(NL_XLIM);
        *d = e;
        ops_ += 2;
        return e * (1.0 + (x - NL_XLIM));
    }
    e = exp(x);
    *d = e;
    return e;
}

/* DEVpnjlim de SPICE3f5, sin tocar: si la tensión de una unión sube por
 * encima de Vcrit más de 2·n·VT de golpe, se sustituye el salto por el que
 * daría la corriente (la exponencial se invierte: vo + nvt·ln(1 + Δ/nvt)). */
double nl_pnjlim(double vn, double vo, double nvt, double vcrit, int *lim)
{
    *lim = 0;
    ops_ += 2;
    if (vn > vcrit && fabs(vn - vo) > 2.0 * nvt) {
        *lim = 1;
        if (vo > 0.0) {
            double arg = 1.0 + (vn - vo) / nvt;
            if (arg <= 0.0) return vcrit;
            ops_ += NL_OPS_EXP + 3; exps_++;
            return vo + nvt * log(arg);
        }
        ops_ += NL_OPS_EXP + 2; exps_++;
        return nvt * log(vn / nvt);
    }
    return vn;
}

/* DEVfetlim de SPICE3f5 (la puerta de un FET): pasos acotados según lo
 * encendido que estaba; las mismas ramas, con MAX y MIN escritos como if */
double nl_fetlim(double vn, double vo, double vto)
{
    double hi = fabs(2.0 * (vo - vto)) + 2.0, lo = hi / 2.0 + 2.0, vtox = vto + 3.5, dv = vn - vo;
    ops_ += 6;
    if (vo >= vto) {
        if (vo >= vtox) {                                /* muy encendido */
            if (dv <= 0.0) {                             /* se apaga */
                if (vn >= vtox) { if (-dv > lo) vn = vo - lo; }
                else if (vn < vto + 2.0) vn = vto + 2.0;
            } else if (dv >= hi) vn = vo + hi;           /* sigue encendido */
        } else {                                         /* zona media */
            if (dv <= 0.0) { if (vn < vto - 0.5) vn = vto - 0.5; }
            else if (vn > vto + 4.0) vn = vto + 4.0;
        }
    } else {                                             /* apagado */
        if (dv <= 0.0) { if (-dv > hi) vn = vo - hi; }
        else {
            double vt = vto + 0.5;
            if (vn <= vt) { if (dv > lo) vn = vo + lo; }
            else vn = vt;
        }
    }
    return vn;
}

/* DEVlimvds de SPICE3f5 (drenador-surtidor) */
double nl_limvds(double vn, double vo)
{
    ops_ += 1;
    if (vo >= 3.5) {
        if (vn > vo) { if (vn > 3.0 * vo + 2.0) vn = 3.0 * vo + 2.0; }
        else if (vn < 3.5 && vn < 2.0) vn = 2.0;
    } else {
        if (vn > vo) { if (vn > 4.0) vn = 4.0; }
        else if (vn < -0.5) vn = -0.5;
    }
    return vn;
}

/* Los límites del MOS como mos1load de SPICE3: con el vDS de antes >= 0 se
 * limita vGS con fetlim y vDS con limvds (modo directo); con vDS < 0, D y S
 * están cambiados y se limita vGD con fetlim y -vDS con limvds (modo
 * inverso). Sin el inverso, un MOSFET dibujado al revés tendría vDS < -0,5 V
 * recortada a -0,5 en cada iteración y Newton no convergería nunca
 * (hallazgo 9 del §9; réplica en e4juez2/r4_mos_inv.py). */
int nl_mos_lim(double *x, double *y, double xo, double yo, double vt)
{
    double x0 = *x, y0 = *y, xgd = *x - *y;              /* vGD nueva, sin limitar */
    ops_ += 3;
    if (yo >= 0.0) {                                     /* modo directo */
        *x = nl_fetlim(*x, xo, vt);
        *y = *x - xgd;                                   /* vDS = vGS (limitada) - vGD */
        *y = nl_limvds(*y, yo);
    } else {                                             /* modo inverso: D y S cambian */
        xgd = nl_fetlim(xgd, xo - yo, vt);
        *y = *x - xgd;
        *y = -nl_limvds(-*y, -yo);
        *x = xgd + *y;
    }
    return *x != x0 || *y != y0;
}

/* La tensión de una unión a partir de la cual la corriente crece más deprisa
 * que la tensión (la curvatura de SPICE3): ahí empieza a actuar pnjlim */
double nl_vcrit(double Is, double nvt)
{
    ops_ += NL_OPS_EXP + 3; exps_++;
    return nvt * log(nvt / (RAIZ2_NL * Is));
}

/* ==========================================================================
 *  3. El diodo por tramos (D3; el zener de libro, D5)
 *
 *  Tres rectas que se tocan: corte i = GMIN·v; conducción i = (v - Vγ)/Ron +
 *  GMIN·Vγ; ruptura i = (v + Vz0)/Rz - GMIN·Vz0. En v = Vγ las dos primeras
 *  dan GMIN·Vγ y en v = -Vz0 la primera y la tercera dan -GMIN·Vz0: la curva
 *  es continua y creciente, y el paso de Katzenelson converge en un número
 *  finito de cambios. Nunca es una rama ideal: el análisis estructural no
 *  depende del tramo.
 * ======================================================================= */
void nl_tr_recta(const SimEl *e, int tramo, double *g, double *i0)
{
    const double vg = e->m[0], ron = e->m[1], vz0 = e->m[2], rz = e->m[3];
    ops_ += 3;
    switch (tramo) {
    case TR_CONDUCE: *g = 1.0 / ron; *i0 = NL_GMIN * vg - vg / ron;  break;
    case TR_RUPTURA: *g = 1.0 / rz;  *i0 = vz0 / rz - NL_GMIN * vz0; break;
    default:         *g = NL_GMIN;   *i0 = 0.0;                      break;
    }
}

/* Cuánto falta para salir del tramo (>= 0 dentro). Afín en v y exacto en la
 * esquina: v - Vγ, Vγ - v, v + Vz0 y -Vz0 - v valen 0 justo ahí (la resta de
 * dos números iguales es exacta) y cambian de signo en el ulp de al lado. */
double nl_tr_margen(const SimEl *e, int tramo, double v)
{
    const double vg = e->m[0], vz0 = e->m[2];
    ops_ += 3;
    switch (tramo) {
    case TR_CONDUCE: return v - vg;
    case TR_RUPTURA: return -vz0 - v;
    default: {
        double m = vg - v;
        if (vz0 > 0.0 && v + vz0 < m) m = v + vz0;
        return m;
    }
    }
}

/* En las esquinas, el de reposo (corte): así un diodo que está justo en su
 * codo no cambia de tramo sin motivo */
int nl_tr_de(const SimEl *e, double v)
{
    if (v > e->m[0]) return TR_CONDUCE;
    if (e->m[2] > 0.0 && v < -e->m[2]) return TR_RUPTURA;
    return TR_CORTE;
}

/* De conducción o ruptura se sale siempre a corte. De corte, al lado que ata
 * su margen (el menor de Vγ - v y v + Vz0): lejos de la esquina es «v > Vγ»
 * o «v < -Vz0» (§3.C), y en la esquina misma (el x del paso de Katzenelson,
 * con el margen en 0 salvo redondeo) no depende de que v haya quedado a un
 * ulp de dentro o de fuera. */
int nl_tr_vecino(const SimEl *e, int tramo, double v)
{
    const double vg = e->m[0], vz0 = e->m[2];
    ops_ += 3;
    if (tramo != TR_CORTE) return TR_CORTE;
    if (vz0 > 0.0 && v + vz0 < vg - v) return TR_RUPTURA;
    return TR_CONDUCE;
}

/* ==========================================================================
 *  4. La unión exponencial con Rs condensada (D4; la ruptura del zener, D5)
 *
 *  ij(vd) = Is·(e^(vd/(n·VT)) - 1) - Ibv·e^(-(vd + BV)/VT) (el segundo
 *  término, solo con BV > 0); en bornes v = vd + Rs·ij e i = ij + GMIN·v.
 *  El nodo interno de SPICE entre la unión y Rs se elimina en cada
 *  iteración: con el compañero de la unión en vd, ij ≈ ij0 + gj·(vd - vd0),
 *  y v = vd + Rs·ij, queda i = G·v + Ieq con
 *      G = gj/(1 + Rs·gj) + GMIN,   Ieq = (ij0 - gj·vd0)/(1 + Rs·gj).
 *  Tras resolver, la vd nueva sale de ese mismo modelo lineal y se limita con
 *  pnjlim: son las iteraciones de SPICE con el nodo, sin una incógnita más.
 * ======================================================================= */
void nl_union_j(const SimEl *e, double vd, double *ij, double *gj)
{
    const double Is = e->m[0], nvt = e->m[1] * NL_VT, BV = e->m[3], Ibv = e->m[4];
    double d, ev = nl_exp(vd / nvt, &d);
    *ij = Is * (ev - 1.0);
    *gj = Is * d / nvt;
    ops_ += 6;
    if (BV > 0.0) {
        double dr, er = nl_exp(-(vd + BV) / NL_VT, &dr);
        *ij -= Ibv * er;
        *gj += Ibv * dr / NL_VT;
        ops_ += 6;
    }
}

void nl_union_comp(const SimEl *e, double vd, double *G, double *Ieq)
{
    double ij, gj, d;
    nl_union_j(e, vd, &ij, &gj);
    d = 1.0 + e->m[2] * gj;
    *G = gj / d + NL_GMIN;
    *Ieq = (ij - gj * vd) / d;
    ops_ += 6;
}

/* La vd nueva: la del modelo lineal con el que se resolvió, vd = v - Rs·(i -
 * GMIN·v) con i = G·v + Ieq, limitada respecto a vd_ant. En ruptura (como
 * dioload de SPICE3: vd < mín(0, -BV + 10·VT)) se limita la tensión
 * reflejada -(vd + BV) con VT, la pendiente de la ruptura, y la misma Vcrit;
 * el mín con 0 es de SPICE3 (el §3.D no lo dice): con una BV menor que
 * 10·VT, una vd en directa no se trata como ruptura. */
double nl_union_vd(const SimEl *e, double vd_ant, double v, double G, double Ieq, int *lim)
{
    const double nvt = e->m[1] * NL_VT, BV = e->m[3], vcrit = e->m[5];
    double vd = v - e->m[2] * (G * v + Ieq - NL_GMIN * v), tope = -BV + 10.0 * NL_VT;
    ops_ += 7;
    if (tope > 0.0) tope = 0.0;
    if (BV > 0.0 && vd < tope) {
        double t = nl_pnjlim(-(vd + BV), -(vd_ant + BV), NL_VT, vcrit, lim);
        return *lim ? -(t + BV) : vd;
    }
    return nl_pnjlim(vd, vd_ant, nvt, vcrit, lim);
}

/* ==========================================================================
 *  5. El BJT y el MOSFET
 * ======================================================================= */
/* Ebers-Moll de transporte con Early (el Gummel-Poon de SPICE con VAF y sin
 * IKF ni ISE), en el sentido del NPN: x = pol·vBE, y = pol·vBC.
 *   iF = Is·(e^(x/VT) - 1), iR = Is·(e^(y/VT) - 1), q = 1 - y/VA (>= 0,05)
 *   ic = (iF - iR)·q - iR/βR - GMIN·y,   ib = iF/βF + iR/βR + GMIN·(x + y)
 * IC = pol·ic e IB = pol·ib. Las derivadas respecto a vBE y vBC son las
 * mismas en NPN y PNP (pol² = 1). En el tope de q, dq = 0. */
void nl_bjt(const SimEl *e, double vbe, double vbc, NlBjt *o)
{
    const double Is = e->m[0], bf = e->m[1], br = e->m[2], va = e->m[3], pol = pol_de(e);
    double x = pol * vbe, y = pol * vbc, dF, dR, iF, iR, gF, gR, q = 1.0, dq = 0.0, ic, ib;
    iF = Is * (nl_exp(x / NL_VT, &dF) - 1.0);
    iR = Is * (nl_exp(y / NL_VT, &dR) - 1.0);
    gF = Is * dF / NL_VT;
    gR = Is * dR / NL_VT;
    if (va > 0.0) {
        q = 1.0 - y / va;
        dq = -1.0 / va;
        if (q < 0.05) { q = 0.05; dq = 0.0; }
    }
    ic = (iF - iR) * q - iR / br - NL_GMIN * y;
    ib = iF / bf + iR / br + NL_GMIN * (x + y);
    o->ic = pol * ic;
    o->ib = pol * ib;
    o->dic_be = gF * q;
    o->dic_bc = -gR * q + (iF - iR) * dq - gR / br - NL_GMIN;
    o->dib_be = gF / bf + NL_GMIN;
    o->dib_bc = gR / br + NL_GMIN;
    ops_ += 20;
}

/* Shichman-Hodges (nivel 1) con el convenio de Boylestad, ID = K·vov² en
 * saturación, en el sentido del canal N: x = pol·vGS, y = pol·vDS, λ = 1/VA.
 * Simétrico: con y < 0, D y S cambian (x -> x - y, y -> -y) y al final se
 * deshace (id -> -id, gds -> gm + gds, gm -> -gm). GMIN va entre D y S. */
void nl_mos(const SimEl *e, double vgs, double vds, NlMos *o)
{
    const double vt = e->m[0], K = e->m[1], lam = e->m[2] > 0.0 ? 1.0 / e->m[2] : 0.0, pol = pol_de(e);
    double x = pol * vgs, y = pol * vds, vov, f, id, gm, gds;
    int cambia = y < 0.0;
    if (cambia) { x -= y; y = -y; }
    vov = x - vt;
    f = 1.0 + lam * y;
    if (vov <= 0.0) id = gm = gds = 0.0;                 /* corte */
    else if (y < vov) {                                  /* óhmica */
        double p = 2.0 * vov * y - y * y;
        id = K * p * f;
        gm = 2.0 * K * y * f;
        gds = 2.0 * K * (vov - y) * f + lam * K * p;
    } else {                                             /* saturación */
        id = K * vov * vov * f;
        gm = 2.0 * K * vov * f;
        gds = lam * K * vov * vov;
    }
    id += NL_GMIN * y;
    gds += NL_GMIN;
    if (cambia) { id = -id; gds += gm; gm = -gm; }
    o->id = pol * id;
    o->gm = gm;
    o->gds = gds;
    ops_ += 15;
}

/* ==========================================================================
 *  6. La linealización en puertos
 *
 *  Puertos (§3.C): la unión, (a, b); el BJT, (B, E) con IB y (C, E) con IC;
 *  el MOS, (G, S) sin corriente y (D, S) con ID. g[p][q] = di_p/dv_q.
 * ======================================================================= */
static int evalua_union(const SimEl *e, const double *vp, double vold[2], NlLin *L)
{
    const double Rs = e->m[2];
    double vd = vold[0], ij, gj, d;
    int lim = 0;
    if (vp) {
        double G = 0.0, Ieq = 0.0;                       /* con Rs = 0 no se usan: vd = v */
        if (Rs > 0.0) {
            if (L->g[0][0] > 0.0 && L->vd == vold[0]) {  /* el compañero con que se resolvió */
                G = L->g[0][0];
                Ieq = L->i[0] - G * L->v[0];
                ops_ += 2;
            } else nl_union_comp(e, vold[0], &G, &Ieq);  /* sin él: se rehace en vold (una exp más) */
        }
        vd = nl_union_vd(e, vold[0], vp[0], G, Ieq, &lim);
    }
    nl_union_j(e, vd, &ij, &gj);                         /* la única exp de la iteración */
    d = 1.0 + Rs * gj;
    memset(L, 0, sizeof *L);
    L->vd = vd;
    L->v[0] = vd + Rs * ij;
    L->g[0][0] = gj / d + NL_GMIN;
    L->i[0] = ij + NL_GMIN * L->v[0];
    vold[0] = vd;
    vold[1] = 0.0;
    ops_ += 6;
    return lim;
}

/* El BJT limita vBE y vBC con pnjlim (VT y su Vcrit), como bjtload */
static int evalua_bjt(const SimEl *e, const double *vp, double vold[2], NlLin *L)
{
    const double pol = pol_de(e), vcrit = e->m[5];
    double x = vold[0], y = vold[1];
    int l1 = 0, l2 = 0;
    NlBjt o;
    if (vp) {
        x = nl_pnjlim(pol * vp[0], vold[0], NL_VT, vcrit, &l1);
        y = nl_pnjlim(pol * (vp[0] - vp[1]), vold[1], NL_VT, vcrit, &l2);
    }
    nl_bjt(e, pol * x, pol * y, &o);
    memset(L, 0, sizeof *L);
    L->v[0] = pol * x;                                   /* vBE */
    L->v[1] = pol * (x - y);                             /* vCE = vBE - vBC */
    L->i[0] = o.ib;
    L->i[1] = o.ic;
    L->g[0][0] = o.dib_be + o.dib_bc;                    /* vBC = vBE - vCE */
    L->g[0][1] = -o.dib_bc;
    L->g[1][0] = o.dic_be + o.dic_bc;
    L->g[1][1] = -o.dic_bc;
    vold[0] = x;
    vold[1] = y;
    ops_ += 8;
    return l1 || l2;
}

/* El MOS limita como mos1load (nl_mos_lim). Para Newton, «limitó» es que
 * fetlim o limvds movieron algo: la vuelta de mos1load (vDS = vGS - vGD en el
 * directo; vGS = vGD + vDS en el inverso) cambia unos ulp aunque no limiten
 * (con vDS << vGS la resta no es exacta), nl_mos_lim lo cuenta y, con eso,
 * Newton no daría nunca por buena la iteración (en la zona óhmica de la
 * familia del trazador, 152 huecos). Esos ulp, < 2e-15·(|vGS| + |vDS|), no
 * cuentan: la linealización sigue en los valores de mos1load. */
static int evalua_mos(const SimEl *e, const double *vp, double vold[2], NlLin *L)
{
    const double pol = pol_de(e);
    double x = vold[0], y = vold[1];
    int lim = 0;
    NlMos o;
    if (vp) {
        double x0 = pol * vp[0], y0 = pol * vp[1], tol;
        x = x0;
        y = y0;
        if (nl_mos_lim(&x, &y, vold[0], vold[1], e->m[0])) {
            tol = 2e-15 * (fabs(x0) + fabs(y0));
            lim = fabs(x - x0) > tol || fabs(y - y0) > tol;
            ops_ += 4;
        }
    }
    nl_mos(e, pol * x, pol * y, &o);
    memset(L, 0, sizeof *L);
    L->v[0] = pol * x;                                   /* vGS: la puerta no lleva corriente */
    L->v[1] = pol * y;                                   /* vDS */
    L->i[1] = o.id;
    L->g[1][0] = o.gm;
    L->g[1][1] = o.gds;
    vold[0] = x;
    vold[1] = y;
    return lim;
}

int nl_evalua(const SimEl *e, const double vpuerto[2], double vold[2], NlLin *L)
{
    switch (e->modelo) {
    case NL_UNION: return evalua_union(e, vpuerto, vold, L);
    case NL_BJT:   return evalua_bjt(e, vpuerto, vold, L);
    case NL_MOS:   return evalua_mos(e, vpuerto, vold, L);
    default:       memset(L, 0, sizeof *L); return 0;   /* los por tramos no se linealizan */
    }
}

/* El punto de partida de SPICE3 (MODEINITJCT): las uniones en Vcrit, el BJT
 * con la de base-emisor en Vcrit y la de base-colector en 0, y el MOS con vGS
 * en el umbral y vDS en 0. Luego nl_evalua(e, NULL, vold, L). */
void nl_arranque(const SimEl *e, double vold[2])
{
    vold[0] = vold[1] = 0.0;
    switch (e->modelo) {
    case NL_UNION:
    case NL_BJT: vold[0] = e->m[5]; break;
    case NL_MOS: vold[0] = e->m[0]; break;
    default: break;
    }
}

/* ==========================================================================
 *  7. El AO, las regiones y la escala
 * ======================================================================= */
/* Los márgenes de los tres tramos del AO (§3.C), >= 0 dentro. vd = V+ - V-
 * (la entrada) y vsal, la salida respecto a su referencia. En el ideal el
 * lineal se mide con la salida (Vd = 0 ahí) y las saturaciones con Vd; en el
 * real, todo con u = A·vd. En la frontera los dos márgenes valen 0: Vsal = V+
 * con Vd = 0 en el ideal, u = V+ en el real. */
double nl_ao_margen(const SimEl *e, int tramo, double vd, double vsal)
{
    const double vp = e->m[1], vn = e->m[2];
    double u = e->modelo == NL_AO_IDEAL ? vsal : e->m[0] * vd;
    ops_ += 3;
    switch (tramo) {
    case TR_SATP: return e->modelo == NL_AO_IDEAL ? vd : u - vp;
    case TR_SATN: return e->modelo == NL_AO_IDEAL ? -vd : vn - u;
    default:      return vp - u < u - vn ? vp - u : u - vn;
    }
}

/* La región de un no lineal en una solución (solo para enseñar, §3.H), con
 * los Valores del §2.8: V e I del puerto principal y V2 del segundo. */
int nl_region(const SimEl *e, const Valores *v, int k)
{
    const double V = v->V[k], I = v->I[k], V2 = v->V2[k], pol = pol_de(e);
    switch (e->modelo) {
    case NL_TRAMOS:                                      /* la del tramo */
        switch (nl_tr_de(e, V)) {
        case TR_CONDUCE: return REG_CONDUCE;
        case TR_RUPTURA: return REG_RUPTURA;
        default:         return REG_CORTE;
        }
    case NL_UNION:                                       /* Vz = BV + Rs·Ibv (D5) */
        if (I > 1e-6) return REG_CONDUCE;
        if (I < -1e-6 && e->m[3] > 0.0 && V < -0.5 * (e->m[3] + e->m[2] * e->m[4])) return REG_RUPTURA;
        return REG_CORTE;
    case NL_BJT: {
        /* V = VCE, I = IC, V2 = VBE. Corte con |IC| < 1 µA: el §3.H dice
         * pol·IC, pero en inversa IC sale con el signo cambiado y la región
         * inversa no llegaría nunca */
        double be = pol * V2, bc = pol * (V2 - V);
        if (fabs(I) < 1e-6) return REG_CORTE;
        if (be > 0.4 && bc > 0.4) return REG_SATURACION;
        if (bc > 0.4) return REG_INVERSA;
        return REG_ACTIVA;
    }
    case NL_MOS: {
        /* V = VDS, V2 = VGS; simétrico como el modelo: con vDS < 0, D y S cambian */
        double x = pol * V2, y = pol * V, vov;
        if (y < 0.0) { x -= y; y = -y; }
        vov = x - e->m[0];
        if (vov <= 0.0) return REG_CORTE;
        return y < vov ? REG_OHMICA : REG_SATURACION;
    }
    case NL_AO_IDEAL: {
        /* V = Vsal, V2 = Vd. En el lineal el nulor deja Vd en 0 (salvo
         * redondeo) y la salida entre los raíles; si no, satura del lado de
         * Vd, o del de la salida si Vd es 0 (un comparador con las entradas
         * iguales, que la regla del §F manda a V+) */
        double vp = e->m[1], vn = e->m[2], tol = 1e-9 * (vp - vn > 1.0 ? vp - vn : 1.0);
        if (fabs(V2) > tol) return V2 > 0.0 ? REG_SAT_MAS : REG_SAT_MENOS;
        if (V >= vp - tol) return REG_SAT_MAS;
        if (V <= vn + tol) return REG_SAT_MENOS;
        return REG_LINEAL;
    }
    case NL_AO_REAL: {                                   /* u = A·Vd, recortada */
        double u = e->m[0] * V2;
        if (u >= e->m[1]) return REG_SAT_MAS;
        if (u <= e->m[2]) return REG_SAT_MENOS;
        return REG_LINEAL;
    }
    default:
        return REG_NADA;
    }
}

/* La escala de tensión que aporta el elemento a Vesc (§2.6): Vγ o Vz0 en los
 * por tramos (Vz0 = Vz - rz·Iz: Iz no está en m[], y para una escala basta),
 * Vz = BV + Rs·Ibv en una unión que rompe, V+ - V- en el AO. Los
 * transistores no fijan escala: la dan sus fuentes. */
double nl_escala_v(const SimEl *e)
{
    switch (e->modelo) {
    case NL_TRAMOS:   return e->m[0] > e->m[2] ? e->m[0] : e->m[2];
    case NL_UNION:    return e->m[3] > 0.0 ? e->m[3] + e->m[2] * e->m[4] : 0.0;
    case NL_AO_IDEAL:
    case NL_AO_REAL:  return e->m[1] - e->m[2];
    default:          return 0.0;
    }
}
