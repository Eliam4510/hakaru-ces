/* --------------------------------------------------------------------------
 *  55n. Etapa 4: los modelos (nolin.c)
 *
 *  Los modelos sin estado del §2.7, antes de que exista el motor que los usa
 *  (el paso 3 del §7). Se comprueban por sus propiedades y contra números que
 *  no salen de nolin.c:
 *    a. constantes y catálogo (D6, §3.A)
 *    b. nl_exp: la recta por encima de 80, continua con su derivada, y sin
 *       exp() por debajo de -40
 *    c. los límites de SPICE3: casos a mano (scratchpad/nolin_numeros.py) y
 *       una transcripción aparte de DEVfetlim, DEVlimvds y mos1load; el
 *       MOSFET al revés de e4juez2/r4_mos_inv.py
 *    d. los por tramos: continuos en sus esquinas, márgenes que cambian de
 *       signo justo en ellas, el tramo y el vecino
 *    e. derivadas analíticas frente a diferencias centradas (unión, BJT, MOS)
 *    f. D5 y D6: el zener exponencial pasa por (-Vz, -Iz) con pendiente rz; la
 *       Is del LED pasa justo por (Vf, If) con GMIN
 *    g. nl_evalua (arranque, límites, una exp por iteración, los puertos), el
 *       AO, las regiones, la escala y el contador
 *    h. los casos cerrados del §6.5, §6.10 y §6.11 que no necesitan el motor,
 *       con un Newton pequeño hecho aquí (potenciales de nodo como incógnitas,
 *       Gauss denso) sobre nl_evalua, nl_tr_* y el estampado del §3.C; los
 *       números, de mpmath a 40 cifras (e4juez/n1.py, n2.py y n7.py)
 * ----------------------------------------------------------------------- */
#include <float.h>
#include "nolin.h"

/* ---- utilidades ---- */
static unsigned long long nlt_s = 0x9E3779B97F4A7C15ULL;
static double nlt_u(void)                        /* xorshift: [0, 1), semilla fija */
{
    nlt_s ^= nlt_s << 13;
    nlt_s ^= nlt_s >> 7;
    nlt_s ^= nlt_s << 17;
    return (double)(nlt_s >> 11) * (1.0 / 9007199254740992.0);
}
static double nlt_entre(double a, double b) { return a + (b - a) * nlt_u(); }
static double nlt_max(double a, double b) { return a > b ? a : b; }
static double nlt_min(double a, double b) { return a < b ? a : b; }

/* |a - b| <= tol·|b| */
static void nlt_rel(const char *q, const char *det, double a, double b, double tol)
{
    chk_t(q, det, a, b, fabs(b), tol);
}

/* ¿Cruza c el intervalo de a a b? (los codos de nl_exp, el tope de q...) */
static int nlt_cruza(double a, double b, double c) { return (a < c) != (b < c); }

/* El peor error relativo de una derivada analítica frente a la numérica. La
 * diferencia centrada no ve nada por debajo de su ruido de redondeo (unos
 * eps·M/h, con M el mayor término de la función: una conductancia de GMIN al
 * lado de una corriente de amperios se pierde), así que ese ruido se
 * descuenta; el piso, GMIN, es la conductancia más pequeña que importa. */
static void nlt_peor(double *peor, double ana, double num, double piso, double ruido)
{
    double e = fabs(ana - num) - ruido;
    e = e > 0.0 ? e / (fabs(ana) + piso) : 0.0;
    if (e > *peor) *peor = e;
}

/* El mayor término de ic e ib (en el sentido del NPN, x = pol·vBE, y =
 * pol·vBC) y el de id (x = pol·vGS, y = pol·vDS): la escala del redondeo */
static double nlt_m_bjt(const SimEl *e, double x, double y)
{
    double q = e->m[3] > 0.0 ? fabs(1.0 - y / e->m[3]) : 1.0;
    return e->m[0] * (exp(nlt_min(x / NL_VT, 700.0)) + exp(nlt_min(y / NL_VT, 700.0)) + 2.0) *
           (q + 1.0 / e->m[1] + 1.0 / e->m[2]) + NL_GMIN * (fabs(x) + fabs(y));
}
static double nlt_m_mos(const SimEl *e, double x, double y)
{
    double lam = e->m[2] > 0.0 ? 1.0 / e->m[2] : 0.0, vov;
    if (y < 0.0) { x -= y; y = -y; }
    vov = fabs(x - e->m[0]);
    return e->m[1] * (vov * vov + 2.0 * vov * y + y * y) * (1.0 + lam * y) + NL_GMIN * y;
}

/* Elementos armados a mano con el contrato de m[] del §2.6 */
static SimEl nlt_el(int tipo, int modelo, int pol)
{
    SimEl e;
    memset(&e, 0, sizeof e);
    e.tipo = (unsigned char)tipo;
    e.modelo = (unsigned char)modelo;
    e.pol = (signed char)pol;
    e.c = e.nref = -1;
    e.ctl = -1;
    return e;
}
static SimEl nlt_union(double Is, double n, double Rs, double BV, double Ibv)
{
    SimEl e = nlt_el(SE_D, NL_UNION, 0);
    e.m[0] = Is; e.m[1] = n; e.m[2] = Rs; e.m[3] = BV; e.m[4] = Ibv;
    e.m[5] = nl_vcrit(Is, n * NL_VT);
    return e;
}
static SimEl nlt_tramos(double vg, double ron, double vz0, double rz)
{
    SimEl e = nlt_el(SE_D, NL_TRAMOS, 0);
    e.m[0] = vg; e.m[1] = ron; e.m[2] = vz0; e.m[3] = rz;
    return e;
}
static SimEl nlt_bjt(int pol, double Is, double bf, double br, double vaf)
{
    SimEl e = nlt_el(SE_BJT, NL_BJT, pol);
    e.m[0] = Is; e.m[1] = bf; e.m[2] = br; e.m[3] = vaf;
    e.m[5] = nl_vcrit(Is, NL_VT);
    return e;
}
static SimEl nlt_mos(int pol, double vt, double K, double vaf)
{
    SimEl e = nlt_el(SE_MOS, NL_MOS, pol);
    e.m[0] = vt; e.m[1] = K; e.m[2] = vaf;
    return e;
}
static SimEl nlt_ao(int real, double A, double vp, double vn)
{
    SimEl e = nlt_el(SE_AO, real ? NL_AO_REAL : NL_AO_IDEAL, 0);
    e.m[0] = real ? A : 0.0; e.m[1] = vp; e.m[2] = vn;
    return e;
}
/* D5, como lo hará el adaptador: la directa con Is 1e-14 y n 1; la ruptura con
 * Ibv = Iz, Rs = máx(rz - VT/Iz, 0) y BV = Vz - Rs·Iz */
static SimEl nlt_zener(double Vz, double Iz, double rz)
{
    double Rs = rz - NL_VT / Iz;
    if (Rs < 0.0) Rs = 0.0;
    return nlt_union(1e-14, 1.0, Rs, Vz - Rs * Iz, Iz);
}
/* D6, como lo hará el adaptador: la Is que pasa por (Vf, If) con GMIN */
static double nlt_led_is(double Vf, double n, double Rs, double If)
{
    double ij = If - NL_GMIN * Vf, vd = Vf - Rs * ij;
    return ij / (exp(vd / (n * NL_VT)) - 1.0);
}
/* La vd de una unión con Rs en la tensión de bornes v: v = vd + Rs·ij(vd)
 * (bisección y Newton; solo para la prueba) */
static double nlt_vd_de(const SimEl *e, double v)
{
    double lo = v - 50.0, hi = v + 1.0, vd = v, ij, gj;
    int i;
    for (i = 0; i < 200; i++) {
        double m = 0.5 * (lo + hi);
        nl_union_j(e, m, &ij, &gj);
        if (m + e->m[2] * ij > v) hi = m; else lo = m;
    }
    vd = 0.5 * (lo + hi);
    for (i = 0; i < 4; i++) {
        nl_union_j(e, vd, &ij, &gj);
        vd -= (vd + e->m[2] * ij - v) / (1.0 + e->m[2] * gj);
    }
    return vd;
}

/* ==========================================================================
 *  a, b. Constantes, catálogo y nl_exp
 * ======================================================================= */
static void nlt_constantes(void)
{
    const char *q = "nolin: constantes";
    static const double COL[LED_N][3] = {            /* D6: Vf, n, Rs */
        { 1.8, 2, 5 }, { 2.0, 2, 5 }, { 2.1, 2, 5 }, { 2.2, 2, 5 }, { 3.2, 3, 15 }, { 3.2, 3, 15 } };
    static const unsigned short RGBS[LED_N] = {       /* §2.12 */
        RGB(255, 50, 40), RGB(255, 140, 30), RGB(255, 230, 40), RGB(60, 235, 80), RGB(60, 120, 255), RGB(245, 245, 255) };
    int c, ok = 1;
    nlt_rel(q, "VT = k·300,15/q (CODATA 2018)", NL_VT, 1.380649e-23 * 300.15 / 1.602176634e-19, 1e-15);
    chk_cond(q, "GMIN 1 pS, RON 1 mohm, recta desde 80, exp de 17 op. (BN_EXP), tope del OP 2,05 M (~0,95 s)",
             NL_GMIN == 1e-12 && NL_RON == 1e-3 && NL_XLIM == 80.0 && NL_OPS_EXP == 17 && NL_OPS_OP == 2050000UL);
    /* el banco sin el USB (2026-09-27): el del cierre iba a 288 MHz */
    chk_cond(q, "lo fijo de una iteración, 214,3 µs (BN_NL); de un paso, 43,45 µs (BN_SIM); una operación, 0,46329 µs",
             NS_ITER == 214300.0 && SIM_NS_PASO == 43450.0 && SIM_NS_OP == 463.29);
    chk_cond(q, "los vacíos del §2.7", NL_VG == 0.7 && NL_IS_D == 1e-14 && NL_N == 1.0 && NL_RS == 0.0 &&
             NL_VZ == 5.1 && NL_IZ == 5e-3 && NL_IF == 20e-3 && NL_IM == 30e-3 && NL_A == 1e5 &&
             NL_VSAT == 15.0 && NL_BF == 100.0 && NL_IS_Q == 1e-15 && NL_BR == 1.0 && NL_VT_M == 2.0 && NL_K == 1e-3);
    chk_cond(q, "1N4148 = {2,52 nA; 1,752; 0,568 ohm} (§3.A)", strcmp(NL_1N4148.nombre, "1N4148") == 0 &&
             NL_1N4148.Is == 2.52e-9 && NL_1N4148.n == 1.752 && NL_1N4148.Rs == 0.568);
    chk_cond(q, "1N4007 = {7,02767 nA; 1,80803; 34,1512 mohm} (§3.A)", strcmp(NL_1N4007.nombre, "1N4007") == 0 &&
             NL_1N4007.Is == 7.02767e-9 && NL_1N4007.n == 1.80803 && NL_1N4007.Rs == 0.0341512);
    for (c = 0; c < LED_N; c++)
        if (!(NL_COLOR[c].Vf == COL[c][0] && NL_COLOR[c].n == COL[c][1] && NL_COLOR[c].Rs == COL[c][2] &&
              NL_COLOR[c].rgb == RGBS[c])) ok = 0;
    chk_cond(q, "NL_COLOR: Vf, n, Rs (D6) y rgb (§2.12) de los 6 colores", ok);
    chk_cond(q, "los enumerados del §2.7", TR_CORTE == 0 && TR_CONDUCE == 1 && TR_RUPTURA == 2 &&
             TR_LIN == 0 && TR_SATP == 1 && TR_SATN == 2 && LED_N == 6 && SIM_NM == 6);
}

static void nlt_exp(void)
{
    const char *q = "nolin: nl_exp";
    static const double X[] = { -39.9, -10.0, -1e-3, 0.0, 1.0, 35.5, 79.9, 80.0 };
    double d, d2, y, ya, yb, h = 1e-6;
    int i, ok = 1;
    for (i = 0; i < (int)(sizeof X / sizeof X[0]); i++) {
        y = nl_exp(X[i], &d);
        if (!(y == exp(X[i]) && d == y)) ok = 0;
    }
    chk_cond(q, "de -40 a 80: exp() bit a bit, y d = e^x", ok);
    y = nl_exp(80.5, &d);
    nlt_rel(q, "en 80,5: e^80·1,5 (mpmath)", y, 8.31093357659026507885676e+34, 1e-15);
    nlt_rel(q, "en la recta, d = e^80 (mpmath)", d, 5.540622384393510052571173e+34, 1e-15);
    ya = nl_exp(80.0 - h, &d);
    yb = nl_exp(80.0 + h, &d2);
    nlt_rel(q, "continua en 80: (f(80+h) - f(80-h))/2h = e^80", (yb - ya) / (2.0 * h), exp(80.0), 1e-6);
    nlt_rel(q, "derivada continua en 80", d2, d, 2e-6);
    y = nl_exp(-40.5, &d);
    chk_cond(q, "por debajo de -40: 0 y d = 0", y == 0.0 && d == 0.0);
    nl_ops_reset();
    nl_exp(1.0, &d);
    chk_cond(q, "una exp() cuenta NL_OPS_EXP", nl_ops() == NL_OPS_EXP && nl_exps() == 1);
    nl_ops_reset();
    nl_exp(-50.0, &d);
    chk_cond(q, "por debajo de -40 no se llama a exp(): 0 operaciones", nl_ops() == 0 && nl_exps() == 0);
}

/* ==========================================================================
 *  c. Los límites de SPICE3
 * ======================================================================= */
/* DEVfetlim, DEVlimvds y el bloque de límites de mos1load (SPICE3f5)
 * transcritos aparte, con MAX y MIN como en el original */
static double nlt_fetlim(double vnew, double vold, double vto)
{
    double vtsthi = fabs(2 * (vold - vto)) + 2, vtstlo = vtsthi / 2 + 2, vtox = vto + 3.5, delv = vnew - vold, vtemp;
    if (vold >= vto) {
        if (vold >= vtox) {
            if (delv <= 0) {
                if (vnew >= vtox) { if (-delv > vtstlo) vnew = vold - vtstlo; }
                else vnew = nlt_max(vnew, vto + 2);
            } else { if (delv >= vtsthi) vnew = vold + vtsthi; }
        } else {
            if (delv <= 0) vnew = nlt_max(vnew, vto - .5);
            else vnew = nlt_min(vnew, vto + 4);
        }
    } else {
        if (delv <= 0) { if (-delv > vtsthi) vnew = vold - vtsthi; }
        else {
            vtemp = vto + .5;
            if (vnew <= vtemp) { if (delv > vtstlo) vnew = vold + vtstlo; }
            else vnew = vtemp;
        }
    }
    return vnew;
}
static double nlt_limvds(double vnew, double vold)
{
    if (vold >= 3.5) {
        if (vnew > vold) vnew = nlt_min(vnew, (3 * vold) + 2);
        else if (vnew < 3.5) vnew = nlt_max(vnew, 2);
    } else {
        if (vnew > vold) vnew = nlt_min(vnew, 4);
        else vnew = nlt_max(vnew, -.5);
    }
    return vnew;
}
static int nlt_mos1load(double *vgs, double *vds, double vgs0, double vds0, double von)
{
    double a = *vgs, b = *vds, vgd = *vgs - *vds, vgdo = vgs0 - vds0;
    if (vds0 >= 0) {
        *vgs = nlt_fetlim(*vgs, vgs0, von);
        *vds = *vgs - vgd;
        *vds = nlt_limvds(*vds, vds0);
    } else {
        vgd = nlt_fetlim(vgd, vgdo, von);
        *vds = *vgs - vgd;
        *vds = -nlt_limvds(-*vds, -vds0);
        *vgs = vgd + *vds;
    }
    return *vgs != a || *vds != b;
}

/* El Newton de e4juez2/r4_mos_inv.py: el MOSFET con D a tierra, S por RD a
 * 12 V y G a 4 V; una incógnita, vS. nuevo = 1: los límites de mos1load; 0:
 * fetlim en vGS y limvds en vDS siempre en directo (lo de antes). Devuelve
 * las iteraciones, o 0 si no converge en 100. */
static int nlt_r4(const SimEl *m, int nuevo, double *vs, double *id)
{
    double xo = m->m[0], yo = 0.0;
    int it;
    NlMos o;
    for (it = 1; it <= 100; it++) {
        double vs0 = -yo, f, df, v, xn, yn, xl, yl;
        int lim;
        nl_mos(m, xo, yo, &o);
        f = (12.0 - vs0) / 2000.0 + o.id;
        df = -1.0 / 2000.0 - o.gm - o.gds;
        v = vs0 - f / df;
        xn = 4.0 - v;
        yn = -v;
        xl = xn;
        yl = yn;
        if (nuevo) lim = nl_mos_lim(&xl, &yl, xo, yo, m->m[0]);
        else {
            xl = nl_fetlim(xn, xo, m->m[0]);
            yl = nl_limvds(yn, yo);
            lim = xl != xn || yl != yn;
        }
        if (!lim && fabs(yl - yo) < 1e-12 * nlt_max(1.0, fabs(yl))) {
            nl_mos(m, xl, yl, &o);
            *vs = -yl;
            *id = o.id;
            return it;
        }
        xo = xl;
        yo = yl;
    }
    nl_mos(m, xo, yo, &o);
    *vs = -yo;
    *id = o.id;
    return 0;
}

static void nlt_limites(void)
{
    const char *q = "nolin: límites de SPICE3";
    /* fetlim con vto = 2 y limvds, rama por rama (nolin_numeros.py). Las dos
     * ramas «vo ± vtstlo» de fetlim no se alcanzan nunca en SPICE3 (vtstlo es
     * mayor que el salto posible): la comparación al azar las cubre igual */
    static const double F[][3] = { { 1, 10, 4 }, { 30, 10, 28 }, { 12, 10, 12 }, { -3, 3, 1.5 }, { 9, 3, 6 },
                                   { -20, 0, -6 }, { 7, 0, 2.5 }, { 1, 0, 1 } };
    static const double LV[][3] = { { 30, 5, 17 }, { 10, 5, 10 }, { 1, 5, 2 }, { 3, 5, 3 }, { 10, 1, 4 },
                                    { -3, 1, -0.5 }, { 0, 1, 0 } };
    int lim, i, ok, nlim = 0, ninv = 0;
    double v, vs, id, vs2, id2;
    SimEl m;

    /* pnjlim: los tres casos que limitan (vold > 0 con arg > 0 y con arg <= 0;
     * vold <= 0) y los dos que no (por debajo de vcrit; salto de menos de 2·nvt) */
    v = nl_pnjlim(1.0, 0.6, 0.025, 0.7, &lim);
    nlt_rel(q, "pnjlim, vold > 0 y arg > 0: vold + nvt·ln(arg)", v, 0.67083033360140540201, 1e-15);
    chk_cond(q, "... y lim = 1", lim == 1);
    v = nl_pnjlim(0.8, 1.0, 0.025, 0.7, &lim);
    chk_cond(q, "pnjlim, vold > 0 y arg <= 0: vcrit, lim = 1", v == 0.7 && lim == 1);
    v = nl_pnjlim(1.0, -1.0, 0.025, 0.7, &lim);
    nlt_rel(q, "pnjlim, vold <= 0: nvt·ln(vnew/nvt)", v, 0.092221986352848407571, 1e-15);
    chk_cond(q, "... y lim = 1", lim == 1);
    v = nl_pnjlim(5.0, 0.0, NL_VT, 0.730289720247, &lim);
    nlt_rel(q, "pnjlim desde 0 hasta 5 V (el primer paso de un diodo con R)", v, 0.13616086738145561046, 1e-15);
    v = nl_pnjlim(0.5, 0.0, 0.025, 0.7, &lim);
    chk_cond(q, "pnjlim por debajo de vcrit: nada, lim = 0", v == 0.5 && lim == 0);
    v = nl_pnjlim(0.74, 0.71, 0.025, 0.7, &lim);
    chk_cond(q, "pnjlim con un salto de menos de 2·nvt: nada, lim = 0", v == 0.74 && lim == 0);

    ok = 1;
    for (i = 0; i < (int)(sizeof F / sizeof F[0]); i++) if (nl_fetlim(F[i][0], F[i][1], 2.0) != F[i][2]) ok = 0;
    chk_cond(q, "fetlim: las 8 ramas a mano (vto = 2)", ok);
    ok = 1;
    for (i = 0; i < (int)(sizeof LV / sizeof LV[0]); i++) if (nl_limvds(LV[i][0], LV[i][1]) != LV[i][2]) ok = 0;
    chk_cond(q, "limvds: las 7 ramas a mano", ok);
    ok = 1;
    for (i = 0; i < 400; i++) {
        double vo = nlt_entre(-10.0, 20.0), vn = nlt_entre(-10.0, 20.0), vto = nlt_entre(0.0, 4.0);
        if (i % 4 == 0) vn = vo + nlt_entre(-0.5, 0.5);
        if (i % 7 == 0) vo = vto + nlt_entre(-0.1, 4.0);
        if (nl_fetlim(vn, vo, vto) != nlt_fetlim(vn, vo, vto) || nl_limvds(vn, vo) != nlt_limvds(vn, vo)) ok = 0;
    }
    chk_cond(q, "fetlim y limvds = SPICE3f5 en 400 pares al azar", ok);

    /* nl_mos_lim frente a mos1load: 50 pares con el vDS de antes >= 0 y 50 con < 0 */
    ok = 1;
    for (i = 0; i < 100; i++) {
        double xo = nlt_entre(-4.0, 12.0), yo = (i < 50 ? 1.0 : -1.0) * nlt_entre(0.0, 10.0), vt = nlt_entre(0.5, 3.0);
        double x1 = nlt_entre(-8.0, 16.0), y1v = nlt_entre(-12.0, 12.0), x2 = x1, y2v = y1v;
        int l1 = nl_mos_lim(&x1, &y1v, xo, yo, vt), l2 = nlt_mos1load(&x2, &y2v, xo, yo, vt);
        if (x1 != x2 || y1v != y2v || l1 != l2) ok = 0;
        nlim += l1;
        ninv += yo < 0.0;
    }
    chk_cond(q, "nl_mos_lim = mos1load en 100 pares (directo e inverso)", ok && ninv == 50);
    chk_cond(q, "... y limita en más de un tercio", nlim > 33);

    /* El MOSFET dibujado al revés (hallazgo 9 del §9) */
    m = nlt_mos(1, 2.0, 1e-3, 0.0);
    i = nlt_r4(&m, 1, &vs, &id);
    chk_cond(q, "MOSFET al revés con los límites de mos1load: converge en 4 (r4_mos_inv.py)", i == 4);
    nlt_rel(q, "... vS = 3,999999992 V", vs, 3.999999992, 1e-12);
    nlt_rel(q, "... ID = -4,000000004 mA", id, -4.000000004e-3, 1e-12);
    i = nlt_r4(&m, 0, &vs2, &id2);
    chk_cond(q, "con limvds siempre en directo: no converge en 100 y se queda en vS = 0,5", i == 0 && vs2 == 0.5);

    /* Vcrit = nvt·ln(nvt/(√2·Is)) (n7.py y nolin_numeros.py) */
    nlt_rel(q, "Vcrit del exponencial (1e-14, n 1)", nl_vcrit(1e-14, NL_VT), 0.73028972024723351736, 1e-14);
    nlt_rel(q, "Vcrit del 1N4148", nl_vcrit(NL_1N4148.Is, NL_1N4148.n * NL_VT), 0.74128317225250973845, 1e-14);
    nlt_rel(q, "Vcrit del 1N4007", nl_vcrit(NL_1N4007.Is, NL_1N4007.n * NL_VT), 0.71850042798038395605, 1e-14);
    nlt_rel(q, "Vcrit del BJT (1e-15)", nl_vcrit(1e-15, NL_VT), 0.78984591279423139252, 1e-14);
}

/* ==========================================================================
 *  d. El diodo por tramos
 * ======================================================================= */
static void nlt_por_tramos(void)
{
    const char *q = "nolin: por tramos";
    static const double D[][4] = {                                /* Vγ, Ron, Vz0, Rz */
        { 0.0, NL_RON, 0.0, NL_RON },                             /* ideal */
        { 0.7, NL_RON, 0.0, NL_RON },                             /* con Vγ */
        { 0.7, 10.0, 0.0, NL_RON },                               /* con Vγ y rd 10 Ω */
        { 0.7, NL_RON, 5.1 - NL_RON * 5e-3, NL_RON },             /* zener de libro, rz e Iz vacías */
        { 0.7, NL_RON, 5.1 - 7.0 * 0.049, 7.0 },                  /* zener de libro, rz 7 Ω, Iz 49 mA */
    };
    int i, j, ok_r = 1, ok_c = 1, ok_m = 1, ok_de = 1, ok_v = 1, ok_k = 1;
    for (i = 0; i < (int)(sizeof D / sizeof D[0]); i++) {
        SimEl e = nlt_tramos(D[i][0], D[i][1], D[i][2], D[i][3]);
        double vg = D[i][0], ron = D[i][1], vz0 = D[i][2], rz = D[i][3], g0, a0, g1, a1, g2, a2, ia, ib, tol, v;
        nl_tr_recta(&e, TR_CORTE, &g0, &a0);
        nl_tr_recta(&e, TR_CONDUCE, &g1, &a1);
        nl_tr_recta(&e, TR_RUPTURA, &g2, &a2);
        /* las rectas del §3.C */
        if (!(g0 == NL_GMIN && a0 == 0.0 && g1 == 1.0 / ron && a1 == NL_GMIN * vg - vg / ron &&
              g2 == 1.0 / rz && a2 == vz0 / rz - NL_GMIN * vz0)) ok_r = 0;
        /* continuas en Vγ: las dos dan GMIN·Vγ, salvo el redondeo de g·v + i0 */
        ia = g0 * vg + a0;
        ib = g1 * vg + a1;
        tol = 8.0 * DBL_EPSILON * (fabs(g1 * vg) + fabs(a1)) + 1e-300;
        if (fabs(ia - ib) > tol || fabs(ib - NL_GMIN * vg) > tol) ok_c = 0;
        /* los márgenes valen 0 justo en la esquina y cambian de signo en el ulp de al lado */
        if (!(nl_tr_margen(&e, TR_CORTE, vg) == 0.0 && nl_tr_margen(&e, TR_CONDUCE, vg) == 0.0)) ok_m = 0;
        v = nextafter(vg, 1e9);
        if (!(nl_tr_margen(&e, TR_CORTE, v) < 0.0 && nl_tr_margen(&e, TR_CONDUCE, v) > 0.0)) ok_m = 0;
        v = nextafter(vg, -1e9);
        if (!(nl_tr_margen(&e, TR_CORTE, v) > 0.0 && nl_tr_margen(&e, TR_CONDUCE, v) < 0.0)) ok_m = 0;
        if (vz0 > 0.0) {
            ia = g0 * -vz0 + a0;
            ib = g2 * -vz0 + a2;
            tol = 8.0 * DBL_EPSILON * (fabs(g2 * vz0) + fabs(a2));
            if (fabs(ia - ib) > tol || fabs(ib + NL_GMIN * vz0) > tol) ok_c = 0;
            if (!(nl_tr_margen(&e, TR_CORTE, -vz0) == 0.0 && nl_tr_margen(&e, TR_RUPTURA, -vz0) == 0.0)) ok_m = 0;
            v = nextafter(-vz0, -1e9);
            if (!(nl_tr_margen(&e, TR_CORTE, v) < 0.0 && nl_tr_margen(&e, TR_RUPTURA, v) > 0.0)) ok_m = 0;
            v = nextafter(-vz0, 1e9);
            if (!(nl_tr_margen(&e, TR_CORTE, v) > 0.0 && nl_tr_margen(&e, TR_RUPTURA, v) < 0.0)) ok_m = 0;
        } else if (nl_tr_margen(&e, TR_CORTE, -1e6) != vg + 1e6) ok_m = 0;       /* sin ruptura: Vγ - v */
        /* nl_tr_de: el margen de su tramo es >= 0; en las esquinas, el corte */
        for (j = 0; j < 200; j++) {
            v = nlt_entre(-vz0 - 3.0, vg + 3.0);
            if (nl_tr_margen(&e, nl_tr_de(&e, v), v) < 0.0) ok_de = 0;
        }
        if (nl_tr_de(&e, vg) != TR_CORTE || nl_tr_de(&e, nextafter(vg, 1e9)) != TR_CONDUCE) ok_de = 0;
        if (vz0 > 0.0 && (nl_tr_de(&e, -vz0) != TR_CORTE || nl_tr_de(&e, nextafter(-vz0, -1e9)) != TR_RUPTURA)) ok_de = 0;
        /* nl_tr_vecino: a corte desde los otros dos; desde corte, por el lado
         * que ata el margen, también a un ulp de dentro de la esquina */
        if (nl_tr_vecino(&e, TR_CONDUCE, vg) != TR_CORTE || nl_tr_vecino(&e, TR_RUPTURA, -vz0 - 1.0) != TR_CORTE) ok_v = 0;
        if (nl_tr_vecino(&e, TR_CORTE, nextafter(vg, -1e9)) != TR_CONDUCE ||
            nl_tr_vecino(&e, TR_CORTE, nextafter(vg, 1e9)) != TR_CONDUCE ||
            nl_tr_vecino(&e, TR_CORTE, vg + 3.0) != TR_CONDUCE) ok_v = 0;
        if (vz0 > 0.0 && (nl_tr_vecino(&e, TR_CORTE, nextafter(-vz0, 1e9)) != TR_RUPTURA ||
                          nl_tr_vecino(&e, TR_CORTE, nextafter(-vz0, -1e9)) != TR_RUPTURA ||
                          nl_tr_vecino(&e, TR_CORTE, -vz0 - 3.0) != TR_RUPTURA)) ok_v = 0;
        /* El margen de CORTE de un zener es el mínimo de dos afines: el paso de
         * Katzenelson interpola con el del lado por el que sale,
         * -nl_tr_margen(e, vecino, v); desde v = 0 hasta -10 V cae justo en -Vz0 */
        if (vz0 > 0.0) {
            int s = nl_tr_vecino(&e, TR_CORTE, -10.0);
            double m0 = -nl_tr_margen(&e, s, 0.0), m1 = -nl_tr_margen(&e, s, -10.0), th = m0 / (m0 - m1);
            if (s != TR_RUPTURA || fabs(-10.0 * th + vz0) > 1e-14 * vz0 ||
                nl_tr_vecino(&e, TR_CORTE, -10.0 * th) != TR_RUPTURA) ok_k = 0;
        }
    }
    chk_cond(q, "las rectas de corte, conducción y ruptura (§3.C)", ok_r);
    chk_cond(q, "continuas en Vg (GMIN·Vg) y en -Vz0 (-GMIN·Vz0)", ok_c);
    chk_cond(q, "márgenes: 0 en la esquina, signo cambiado en el ulp de al lado", ok_m);
    chk_cond(q, "nl_tr_de: su margen >= 0; en la esquina, corte", ok_de);
    chk_cond(q, "nl_tr_vecino: por el lado que ata el margen (también a un ulp)", ok_v);
    chk_cond(q, "Katzenelson con el margen del lado por el que sale: justo en -Vz0", ok_k);
}

/* ==========================================================================
 *  e. Derivadas frente a diferencias centradas
 * ======================================================================= */
static void nlt_union_der(const char *nom, const SimEl *e, double v0, double v1)
{
    char det[140];
    const double nvt = e->m[1] * NL_VT, BV = e->m[3], Rs = e->m[2];
    double pj = 0.0, pG = 0.0, pc = 0.0;
    int i, n = 0;
    for (i = 0; n < 200 && i < 4000; i++) {
        double vd = nlt_entre(v0, v1), h = 1e-6 * nlt_max(fabs(vd), nvt);
        double ija, gja, ijb, gjb, ij, gj, G, Ieq, vA, vB, v, I, e2;
        /* sin cruzar los codos de nl_exp (-40 y 80): ahí salta la derivada segunda */
        if (nlt_cruza((vd - h) / nvt, (vd + h) / nvt, -40.0) || nlt_cruza((vd - h) / nvt, (vd + h) / nvt, 80.0)) continue;
        if (BV > 0.0 && (nlt_cruza(-(vd - h + BV) / NL_VT, -(vd + h + BV) / NL_VT, -40.0) ||
                         nlt_cruza(-(vd - h + BV) / NL_VT, -(vd + h + BV) / NL_VT, 80.0))) continue;
        n++;
        nl_union_j(e, vd - h, &ija, &gja);
        nl_union_j(e, vd + h, &ijb, &gjb);
        nl_union_j(e, vd, &ij, &gj);
        nl_union_comp(e, vd, &G, &Ieq);
        nlt_peor(&pj, gj, (ijb - ija) / (2.0 * h), NL_GMIN, 0.0);
        /* en bornes: v(vd) = vd + Rs·ij, i(vd) = ij + GMIN·v, y di/dv = G */
        vA = vd - h + Rs * ija;
        vB = vd + h + Rs * ijb;
        nlt_peor(&pG, G, ((ijb + NL_GMIN * vB) - (ija + NL_GMIN * vA)) / (vB - vA), NL_GMIN, 0.0);
        /* el compañero pasa por el punto del modelo: G·v + Ieq = i */
        v = vd + Rs * ij;
        I = ij + NL_GMIN * v;
        e2 = fabs(G * v + Ieq - I) / (fabs(G * v) + fabs(Ieq) + fabs(I) + 1e-300);
        if (e2 > pc) pc = e2;
    }
    snprintf(det, sizeof det, "%s: gj = dij/dvd en %d puntos (1e-6)", nom, n);
    chk_t("nolin: derivadas de la unión", det, pj, 0.0, 1.0, 1e-6);
    snprintf(det, sizeof det, "%s: G = di/dv en bornes, con Rs y GMIN (1e-6)", nom);
    chk_t("nolin: derivadas de la unión", det, pG, 0.0, 1.0, 1e-6);
    snprintf(det, sizeof det, "%s: G·v + Ieq pasa por el punto del modelo (1e-14)", nom);
    chk_t("nolin: derivadas de la unión", det, pc, 0.0, 1.0, 1e-14);
    snprintf(det, sizeof det, "%s: 200 puntos", nom);
    chk_cond("nolin: derivadas de la unión", det, n == 200);
}

static void nlt_bjt_der(const char *nom, const SimEl *e, double be0, double be1, double bc0, double bc1)
{
    char det[140];
    const double pol = e->pol < 0 ? -1.0 : 1.0, vaf = e->m[3];
    double p[4] = { 0.0, 0.0, 0.0, 0.0 };
    int i, n = 0, ntope = 0;
    for (i = 0; n < 200 && i < 4000; i++) {
        double vbe = pol * nlt_entre(be0, be1), vbc = pol * nlt_entre(bc0, bc1);
        double h1 = 1e-6 * nlt_max(fabs(vbe), NL_VT), h2 = 1e-6 * nlt_max(fabs(vbc), NL_VT), x = pol * vbe, y = pol * vbc;
        NlBjt o, a, b, c, d;
        double M;
        if (nlt_cruza((x - h1) / NL_VT, (x + h1) / NL_VT, -40.0) || nlt_cruza((x - h1) / NL_VT, (x + h1) / NL_VT, 80.0) ||
            nlt_cruza((y - h2) / NL_VT, (y + h2) / NL_VT, -40.0) || nlt_cruza((y - h2) / NL_VT, (y + h2) / NL_VT, 80.0))
            continue;
        if (vaf > 0.0 && nlt_cruza(1.0 - (y - h2) / vaf, 1.0 - (y + h2) / vaf, 0.05)) continue;   /* el tope de q */
        n++;
        if (vaf > 0.0 && 1.0 - y / vaf < 0.05) ntope++;
        nl_bjt(e, vbe, vbc, &o);
        nl_bjt(e, vbe - h1, vbc, &a);
        nl_bjt(e, vbe + h1, vbc, &b);
        nl_bjt(e, vbe, vbc - h2, &c);
        nl_bjt(e, vbe, vbc + h2, &d);
        M = nlt_m_bjt(e, x, y);
        nlt_peor(&p[0], o.dic_be, (b.ic - a.ic) / (2.0 * h1), NL_GMIN, 16.0 * DBL_EPSILON * M / h1);
        nlt_peor(&p[1], o.dic_bc, (d.ic - c.ic) / (2.0 * h2), NL_GMIN, 16.0 * DBL_EPSILON * M / h2);
        nlt_peor(&p[2], o.dib_be, (b.ib - a.ib) / (2.0 * h1), NL_GMIN, 16.0 * DBL_EPSILON * M / h1);
        nlt_peor(&p[3], o.dib_bc, (d.ib - c.ib) / (2.0 * h2), NL_GMIN, 16.0 * DBL_EPSILON * M / h2);
    }
    snprintf(det, sizeof det, "%s: dIC/dvBE, dIC/dvBC, dIB/dvBE y dIB/dvBC en %d puntos (1e-6)", nom, n);
    chk_t("nolin: derivadas del BJT", det, nlt_max(nlt_max(p[0], p[1]), nlt_max(p[2], p[3])), 0.0, 1.0, 1e-6);
    snprintf(det, sizeof det, "%s: 200 puntos%s", nom, vaf > 0.0 && vaf < 2.0 ? ", algunos en el tope de q" : "");
    chk_cond("nolin: derivadas del BJT", det, n == 200 && (!(vaf > 0.0 && vaf < 2.0) || ntope > 10));
}

/* La zona del MOS en el sentido del canal N: 0 corte, 1 óhmica, 2 saturación;
 * +4 si D y S están cambiados */
static int nlt_mos_zona(double x, double y, double vt)
{
    int s = y < 0.0;
    if (s) { x -= y; y = -y; }
    if (x - vt <= 0.0) return 4 * s;
    return 4 * s + (y < x - vt ? 1 : 2);
}

static void nlt_mos_der(const char *nom, const SimEl *e)
{
    char det[140];
    const double pol = e->pol < 0 ? -1.0 : 1.0, vt = e->m[0];
    double pm = 0.0, pd = 0.0;
    int i, n = 0, zonas = 0;
    for (i = 0; n < 200 && i < 4000; i++) {
        double vgs = pol * nlt_entre(-3.0, 9.0), vds = pol * nlt_entre(-7.0, 7.0);
        double h1 = 1e-6 * nlt_max(fabs(vgs), 1.0), h2 = 1e-6 * nlt_max(fabs(vds), 1.0), x = pol * vgs, y = pol * vds;
        int z = nlt_mos_zona(x, y, vt);
        NlMos o, a, b, c, d;
        double M;
        /* sin cruzar un cambio de zona (vov = 0, vDS = vov, vDS = 0) */
        if (nlt_mos_zona(x - h1, y, vt) != z || nlt_mos_zona(x + h1, y, vt) != z ||
            nlt_mos_zona(x, y - h2, vt) != z || nlt_mos_zona(x, y + h2, vt) != z) continue;
        n++;
        zonas |= 1 << z;
        nl_mos(e, vgs, vds, &o);
        nl_mos(e, vgs - h1, vds, &a);
        nl_mos(e, vgs + h1, vds, &b);
        nl_mos(e, vgs, vds - h2, &c);
        nl_mos(e, vgs, vds + h2, &d);
        M = nlt_m_mos(e, x, y);
        nlt_peor(&pm, o.gm, (b.id - a.id) / (2.0 * h1), NL_GMIN, 16.0 * DBL_EPSILON * M / h1);
        nlt_peor(&pd, o.gds, (d.id - c.id) / (2.0 * h2), NL_GMIN, 16.0 * DBL_EPSILON * M / h2);
    }
    snprintf(det, sizeof det, "%s: gm = dID/dvGS y gds = dID/dvDS en %d puntos (1e-6)", nom, n);
    chk_t("nolin: derivadas del MOS", det, nlt_max(pm, pd), 0.0, 1.0, 1e-6);
    snprintf(det, sizeof det, "%s: 200 puntos en las 6 zonas (corte, óhmica, saturación; directo e invertido)", nom);
    chk_cond("nolin: derivadas del MOS", det, n == 200 && zonas == 0x77);
}

static void nlt_derivadas(void)
{
    const char *q = "nolin: puntos del BJT y del MOS";
    static const double FAM[6] = { 5e-12, 0.250000005e-3, 1.000000005e-3, 2.250000005e-3, 4.000000005e-3, 6.250000005e-3 };
    SimEl n1 = nlt_bjt(1, 1e-15, 100.0, 1.0, 0.0), p1 = nlt_bjt(-1, 1e-15, 100.0, 1.0, 0.0);
    SimEl n50 = nlt_bjt(1, 1e-15, 100.0, 1.0, 50.0), p08 = nlt_bjt(-1, 1e-15, 100.0, 1.0, 0.8);
    SimEl nva = nlt_bjt(1, 1e-15, 100.0, 1.0, 100.0);
    SimEl mn = nlt_mos(1, 2.0, 1e-3, 50.0), mn0 = nlt_mos(1, 2.0, 1e-3, 0.0);
    SimEl mp = nlt_mos(-1, 2.0, 1e-3, 50.0), mp0 = nlt_mos(-1, 2.0, 1e-3, 0.0);
    SimEl u1 = nlt_union(1e-14, 1.0, 0.0, 0.0, 0.0);
    SimEl u2 = nlt_union(NL_1N4148.Is, NL_1N4148.n, NL_1N4148.Rs, 0.0, 0.0);
    SimEl u3 = nlt_union(NL_1N4007.Is, NL_1N4007.n, NL_1N4007.Rs, 0.0, 0.0);
    SimEl u4 = nlt_zener(5.1, 0.049, 7.0);
    SimEl u5 = nlt_zener(5.1, 5e-3, 0.0);
    SimEl u6 = nlt_union(nlt_led_is(3.2, 3.0, 15.0, 0.02), 3.0, 15.0, 0.0, 0.0);
    NlBjt a, b;
    NlMos m, m2;
    char det[100];
    int k, ok;

    /* las uniones: sin Rs, con Rs y con ruptura, hasta la recta de más de 80 */
    nlt_union_der("exponencial (1e-14, n 1)", &u1, -8.0, 2.5);
    nlt_union_der("1N4148", &u2, -8.0, 4.0);
    nlt_union_der("1N4007", &u3, -8.0, 4.0);
    nlt_union_der("1N4733A (Rs y ruptura)", &u4, -8.0, 1.0);
    nlt_union_der("zener exponencial con Rs = 0", &u5, -8.0, 1.0);
    nlt_union_der("LED azul (n 3, Rs 15)", &u6, -5.0, 4.0);

    /* BJT: los puntos de n2.py (NPN, Is 1e-15, β 100, βr 1) */
    nl_bjt(&n1, 0.7, -4.3, &a);
    nlt_rel(q, "NPN en (0,7; -4,3): IC (n2.py)", a.ic, 5.6702947265407799e-4, 1e-12);
    nlt_rel(q, "NPN en (0,7; -4,3): IB", a.ib, 5.6702910825207799e-6, 1e-12);
    nl_bjt(&p1, -0.7, 4.3, &b);
    chk_cond(q, "PNP en (-0,7; 4,3): las mismas cambiadas de signo, derivadas iguales (bit a bit)",
             b.ic == -a.ic && b.ib == -a.ib && b.dic_be == a.dic_be && b.dic_bc == a.dic_bc &&
             b.dib_be == a.dib_be && b.dib_bc == a.dib_bc);
    nl_bjt(&n1, 0.7, 0.6, &a);
    nlt_rel(q, "NPN saturado en (0,7; 0,6): IC", a.ic, 5.4328572891369173e-4, 1e-12);
    nlt_rel(q, "NPN saturado en (0,7; 0,6): IB", a.ib, 1.7542165402713912e-5, 1e-12);
    nl_bjt(&nva, 0.7, -4.3, &a);
    nlt_rel(q, "NPN con VA 100 V en (0,7; -4,3): IC", a.ic, 5.9141173979326034e-4, 1e-12);
    {
        SimEl n08 = nlt_bjt(1, 1e-15, 100.0, 1.0, 0.8);
        double iF = 1e-15 * (exp(0.7 / NL_VT) - 1.0), iR = 1e-15 * (exp(0.9 / NL_VT) - 1.0), gR = 1e-15 * exp(0.9 / NL_VT) / NL_VT;
        nl_bjt(&n08, 0.7, 0.9, &a);                   /* q = 1 - 0,9/0,8 < 0: el tope, 0,05 */
        nlt_rel(q, "VA 0,8 V en (0,7; 0,9): q en su tope de 0,05", a.ic, (iF - iR) * 0.05 - iR - NL_GMIN * 0.9, 1e-13);
        nlt_rel(q, "... y dq = 0 en el tope", a.dic_bc, -gR * 0.05 - gR - NL_GMIN, 1e-13);
    }
    nlt_bjt_der("NPN", &n1, -1.2, 0.95, -8.0, 0.8);
    nlt_bjt_der("PNP", &p1, -1.2, 0.95, -8.0, 0.8);
    nlt_bjt_der("NPN con VA 50 V", &n50, -1.2, 0.95, -8.0, 0.8);
    nlt_bjt_der("PNP con VA 0,8 V", &p08, -1.2, 0.95, 0.3, 0.9);

    /* MOSFET: los puntos de n2.py (K 1 mA/V², Vt 2 V) */
    nl_mos(&mn, 3.0, 0.5, &m);
    nlt_rel(q, "N con VA 50 V en (3; 0,5), óhmica: 0,7575000005 mA", m.id, 0.7575000005e-3, 1e-12);
    nl_mos(&mn, 3.0, 5.0, &m);
    nlt_rel(q, "N con VA 50 V en (3; 5), saturación: 1,100000005 mA", m.id, 1.100000005e-3, 1e-12);
    nl_mos(&mn, 4.0, -1.0, &m);
    nlt_rel(q, "N con VA 50 V en (4; -1), D y S cambiados: -5,100000001 mA", m.id, -5.100000001e-3, 1e-12);
    nl_mos(&mn, 1.0, 5.0, &m);
    nlt_rel(q, "N con VA 50 V en (1; 5), corte: 5 pA (GMIN)", m.id, 5e-12, 1e-12);
    nl_mos(&mp0, -4.0, -4.0, &m);
    nlt_rel(q, "canal P en (-4; -4): -4,000000004 mA", m.id, -4.000000004e-3, 1e-12);
    for (k = 0; k < 6; k++) {                         /* la familia del trazador (§6.10) */
        nl_mos(&mn0, 2.0 + 0.5 * k, 5.0, &m);
        snprintf(det, sizeof det, "familia: VDS 5 V y VGS = Vt + %g V", 0.5 * k);
        nlt_rel(q, det, m.id, FAM[k], 1e-12);
    }
    ok = 1;                                           /* el canal P es el espejo del N */
    for (k = 0; k < 50; k++) {
        double vgs = nlt_entre(-3.0, 9.0), vds = nlt_entre(-7.0, 7.0);
        nl_mos(&mn, vgs, vds, &m);
        nl_mos(&mp, -vgs, -vds, &m2);
        if (!(m2.id == -m.id && m2.gm == m.gm && m2.gds == m.gds)) ok = 0;
    }
    chk_cond(q, "canal P = espejo del N en 50 puntos (bit a bit)", ok);
    ok = 1;                                           /* simétrico: con vDS < 0, D y S cambian */
    for (k = 0; k < 50; k++) {
        double vgs = nlt_entre(-3.0, 9.0), vds = -nlt_entre(1e-3, 7.0);
        nl_mos(&mn, vgs, vds, &m);
        nl_mos(&mn, vgs - vds, -vds, &m2);
        if (!(m.id == -m2.id && m.gm == -m2.gm && m.gds == m2.gm + m2.gds)) ok = 0;
    }
    chk_cond(q, "simétrico: ID(vGS, vDS < 0) = -ID(vGD, -vDS) en 50 puntos (bit a bit)", ok);
    /* continuo en vov = 0 y en vDS = vov (valor y derivadas) */
    nl_mos(&mn, 2.0, 3.0, &m);
    nl_mos(&mn, 2.0 + 1e-9, 3.0, &m2);
    chk_cond(q, "continuo en vov = 0: ID y gm", fabs(m.id - 3e-12) <= 1e-24 && fabs(m2.id - m.id) <= 1e-9 * m.id &&
             m.gm == 0.0 && fabs(m2.gm) <= 1e-11);
    nl_mos(&mn, 5.0, 3.0, &m);                        /* vov = 3: saturación */
    nl_mos(&mn, 5.0, nextafter(3.0, 0.0), &m2);       /* un ulp debajo: óhmica */
    chk_cond(q, "continuo en vDS = vov: ID, gm y gds", fabs(m.id - m2.id) <= 1e-14 * m.id &&
             fabs(m.gm - m2.gm) <= 1e-12 * m.gm && fabs(m.gds - m2.gds) <= 1e-12 * m.gds);
    nlt_mos_der("canal N", &mn0);
    nlt_mos_der("canal N con VA 50 V", &mn);
    nlt_mos_der("canal P", &mp0);
    nlt_mos_der("canal P con VA 50 V", &mp);
}

/* ==========================================================================
 *  f. D5 y D6
 * ======================================================================= */
static void nlt_d5_d6(void)
{
    const char *q = "nolin: zener (D5) y LED (D6)";
    static const char *const NOM[LED_N] = { "rojo", "naranja", "amarillo", "verde", "azul", "blanco" };
    static const double IS_LED[LED_N] = { 1.0685493590266111e-16, 2.237216788021277e-18, 3.2371648073427147e-19,
                                          4.6840503101921716e-20, 1.1745197291847082e-18, 1.1745197291847082e-18 };
    SimEl z = nlt_zener(5.1, 0.049, 7.0);
    double vd, ij, gj, G, Ieq, I;
    char det[120];
    int c;

    nlt_rel(q, "1N4733A: Rs = rz - VT/Iz = 6,4721443717075765 ohm (§3.A)", z.m[2], 6.4721443717075765, 1e-14);
    nlt_rel(q, "1N4733A: BV = Vz - Rs·Iz = 4,7828649257863288 V", z.m[3], 4.7828649257863288, 1e-14);
    chk_cond(q, "1N4733A: Ibv = Iz, Is 1e-14, n 1", z.m[4] == 0.049 && z.m[0] == 1e-14 && z.m[1] == 1.0);
    vd = nlt_vd_de(&z, -5.1);
    nl_union_j(&z, vd, &ij, &gj);
    I = ij + NL_GMIN * -5.1;
    nlt_rel(q, "1N4733A en -5,1 V: -49,000000005100754 mA (n1.py)", I, -49.000000005100754e-3, 1e-12);
    nl_union_comp(&z, vd, &G, &Ieq);
    nlt_rel(q, "1N4733A en -5,1 V: pendiente dv/di = rz = 7 ohm (sin GMIN)", 1.0 / (G - NL_GMIN), 7.0, 1e-9);
    z = nlt_zener(10.0, 0.01, 20.0);                  /* un zener cualquiera */
    vd = nlt_vd_de(&z, -10.0);
    nl_union_j(&z, vd, &ij, &gj);
    nl_union_comp(&z, vd, &G, &Ieq);
    nlt_rel(q, "Vz 10 V, Iz 10 mA, rz 20 ohm: pasa por (-Vz, -Iz)", ij + NL_GMIN * -10.0, -0.01, 2e-9);
    nlt_rel(q, "... con pendiente rz", 1.0 / (G - NL_GMIN), 20.0, 1e-9);
    z = nlt_zener(5.1, 5e-3, 0.0);                    /* rz < VT/Iz: Rs = 0, BV = Vz */
    chk_cond(q, "rz vacía (0 < VT/Iz): Rs = 0 y BV = Vz", z.m[2] == 0.0 && z.m[3] == 5.1);
    nl_union_j(&z, -5.1, &ij, &gj);
    nlt_rel(q, "... y pasa por (-Vz, -Iz) igual", ij + NL_GMIN * -5.1, -5e-3, 2e-9);

    for (c = 0; c < LED_N; c++) {
        const NlColor *k = &NL_COLOR[c];
        double Is = nlt_led_is(k->Vf, k->n, k->Rs, NL_IF), ijs = NL_IF - NL_GMIN * k->Vf, vds = k->Vf - k->Rs * ijs, v;
        SimEl e = nlt_union(Is, k->n, k->Rs, 0.0, 0.0);
        snprintf(det, sizeof det, "LED %s: Is (n1.py)", NOM[c]);
        nlt_rel(q, det, Is, IS_LED[c], 1e-12);
        nl_union_j(&e, vds, &ij, &gj);
        v = vds + k->Rs * ij;
        snprintf(det, sizeof det, "LED %s: en vd* = Vf - Rs·ij* la tensión de bornes es Vf", NOM[c]);
        nlt_rel(q, det, v, k->Vf, 1e-14);
        snprintf(det, sizeof det, "LED %s: e i = ij + GMIN·Vf = If (20 mA)", NOM[c]);
        nlt_rel(q, det, ij + NL_GMIN * v, NL_IF, 1e-12);
    }
}

/* ==========================================================================
 *  g. nl_evalua, el AO, las regiones, la escala y el contador
 * ======================================================================= */
static void nlt_evalua_prueba(void)
{
    const char *q = "nolin: nl_evalua";
    SimEl u = nlt_union(1e-14, 1.0, 0.0, 0.0, 0.0), r = nlt_union(NL_1N4148.Is, NL_1N4148.n, NL_1N4148.Rs, 0.0, 0.0);
    SimEl n1 = nlt_bjt(1, 1e-15, 100.0, 1.0, 0.0), p1 = nlt_bjt(-1, 1e-15, 100.0, 1.0, 0.0);
    SimEl mn = nlt_mos(1, 2.0, 1e-3, 0.0), tr = nlt_tramos(0.7, NL_RON, 0.0, NL_RON);
    double vold[2], v2[2], vp[2], G, Ieq, ij, gj, v, h = 1e-6, pg = 0.0;
    NlLin L, L2, La, Lb;
    NlBjt o;
    NlMos m;
    unsigned long ops, ops1;
    int lim, l2, i;

    /* el arranque (MODEINITJCT): la unión en Vcrit, sin límites */
    nl_arranque(&u, vold);
    chk_cond(q, "arranque de la unión: vold = (Vcrit, 0)", vold[0] == u.m[5] && vold[1] == 0.0);
    memset(&L, 0, sizeof L);
    lim = nl_evalua(&u, NULL, vold, &L);
    nl_union_j(&u, u.m[5], &ij, &gj);
    chk_cond(q, "vpuerto = NULL: se evalúa en vold, sin límite", lim == 0 && L.vd == u.m[5] && L.v[0] == u.m[5] &&
             L.i[0] == ij + NL_GMIN * u.m[5] && L.g[0][0] == gj + NL_GMIN && L.i[1] == 0.0 && L.g[1][1] == 0.0);
    /* con Rs: (v, i, g) es el compañero de D4 sobre la curva */
    nl_arranque(&r, vold);
    memset(&L, 0, sizeof L);
    nl_evalua(&r, NULL, vold, &L);
    nl_union_comp(&r, r.m[5], &G, &Ieq);
    nl_union_j(&r, r.m[5], &ij, &gj);
    nlt_rel(q, "con Rs: g = G = gj/(1 + Rs·gj) + GMIN", L.g[0][0], G, 1e-15);
    nlt_rel(q, "con Rs: i - g·v = Ieq = (ij - gj·vd)/(1 + Rs·gj)", L.i[0] - L.g[0][0] * L.v[0], Ieq, 1e-12);
    nlt_rel(q, "con Rs: v = vd + Rs·ij(vd)", L.v[0], r.m[5] + r.m[2] * ij, 1e-15);
    /* una exp() por iteración con el compañero anterior en L; con L a cero, dos */
    vp[0] = L.v[0] + 0.002;                           /* un paso corto: sin límite */
    v2[0] = vold[0];
    v2[1] = vold[1];
    nl_ops_reset();
    lim = nl_evalua(&r, vp, vold, &L);
    ops1 = nl_ops();
    chk_cond(q, "con el compañero anterior en L: una exp() y sin límite", lim == 0 && nl_exps() == 1 && ops1 >= NL_OPS_EXP);
    nlt_rel(q, "la vd nueva sale del compañero: v - Rs·(G·v + Ieq - GMIN·v)", L.vd,
            vp[0] - r.m[2] * (G * vp[0] + Ieq - NL_GMIN * vp[0]), 1e-14);
    memset(&L2, 0, sizeof L2);
    nl_ops_reset();
    nl_evalua(&r, vp, v2, &L2);
    ops = nl_ops();
    chk_cond(q, "con L a cero: rehace el compañero en vold (una exp() más)", nl_exps() == 2 && ops >= ops1 + NL_OPS_EXP && ops < ops1 + 2 * NL_OPS_EXP);
    nlt_rel(q, "... y da la misma vd", L2.vd, L.vd, 1e-14);
    chk_cond(q, "vold pasa a ser la vd usada", vold[0] == L.vd && v2[0] == L2.vd);
    /* en inversa profunda, ninguna exp() */
    nl_arranque(&u, vold);
    memset(&L, 0, sizeof L);
    nl_evalua(&u, NULL, vold, &L);
    vp[0] = -5.0;
    nl_ops_reset();
    lim = nl_evalua(&u, vp, vold, &L);
    chk_cond(q, "diodo a -5 V: sin exp() y sin límite", nl_exps() == 0 && lim == 0 && L.vd == -5.0);
    /* un salto grande se limita con pnjlim */
    vp[0] = 5.0;
    lim = nl_evalua(&u, vp, vold, &L);
    v = nl_pnjlim(5.0, -5.0, NL_VT, u.m[5], &l2);
    chk_cond(q, "de -5 V a 5 V: limitada como pnjlim", lim == 1 && l2 == 1 && L.vd == v && vold[0] == v);
    /* la ruptura se limita reflejada (zener exponencial con Rs = 0: vd = v) */
    {
        SimEl z = nlt_zener(5.1, 5e-3, 0.0);
        double w[2] = { -5.2, 0.0 }, t;
        vp[0] = -7.0;
        memset(&L, 0, sizeof L);
        lim = nl_evalua(&z, vp, w, &L);
        t = nl_pnjlim(-(-7.0 + 5.1), -(-5.2 + 5.1), NL_VT, z.m[5], &l2);
        chk_cond(q, "ruptura de -5,2 a -7 V: -(pnjlim(-(vd + BV)) + BV) con VT", lim == 1 && l2 == 1 && L.vd == -(t + 5.1));
        w[0] = -5.2;
        vp[0] = -5.21;
        lim = nl_evalua(&z, vp, w, &L);
        chk_cond(q, "ruptura con un paso corto: sin límite, vd = v exacta", lim == 0 && L.vd == -5.21);
        z = nlt_union(1e-14, 2.0, 0.0, 5.1, 5e-3);    /* con n = 2: la ruptura sigue con VT */
        w[0] = -5.2;
        vp[0] = -7.0;
        lim = nl_evalua(&z, vp, w, &L);
        t = nl_pnjlim(1.9, -(-5.2 + 5.1), NL_VT, z.m[5], &l2);
        chk_cond(q, "ruptura con n = 2: se limita con VT, no con n·VT", lim == 1 && L.vd == -(t + 5.1));
    }

    /* BJT: puertos (B, E) con IB y (C, E) con IC */
    nl_arranque(&n1, vold);
    chk_cond(q, "arranque del BJT: (Vcrit, 0)", vold[0] == n1.m[5] && vold[1] == 0.0);
    vold[0] = 0.7;
    vold[1] = -4.3;
    vp[0] = 0.7;
    vp[1] = 5.0;
    lim = nl_evalua(&n1, vp, vold, &L);
    nl_bjt(&n1, 0.7, 0.7 - 5.0, &o);
    chk_cond(q, "BJT sin límite: i = (IB, IC) de nl_bjt", lim == 0 && L.i[0] == o.ib && L.i[1] == o.ic && L.vd == 0.0);
    nlt_rel(q, "BJT: v[0] = vBE", L.v[0], 0.7, 1e-15);
    nlt_rel(q, "BJT: v[1] = vCE", L.v[1], 5.0, 1e-15);
    for (i = 0; i < 2; i++) {                         /* g[p][q] = di_p/dv_q, en los puertos */
        double d0 = i == 0 ? h : 0.0, d1 = i == 1 ? h : 0.0;
        NlBjt oa, ob;
        double ruido;
        nl_bjt(&n1, 0.7 - d0, (0.7 - d0) - (5.0 - d1), &oa);
        nl_bjt(&n1, 0.7 + d0, (0.7 + d0) - (5.0 + d1), &ob);
        ruido = 16.0 * DBL_EPSILON * nlt_m_bjt(&n1, 0.7, -4.3) / h;
        nlt_peor(&pg, L.g[0][i], (ob.ib - oa.ib) / (2.0 * h), NL_GMIN, ruido);
        nlt_peor(&pg, L.g[1][i], (ob.ic - oa.ic) / (2.0 * h), NL_GMIN, ruido);
    }
    chk_t(q, "BJT: g[p][q] = di_p/dv_q frente a diferencias centradas en vBE y vCE", pg, 0.0, 1.0, 1e-6);
    La = L;
    vold[0] = 0.7;
    vold[1] = -4.3;
    vp[0] = -0.7;
    vp[1] = -5.0;
    memset(&Lb, 0, sizeof Lb);
    nl_evalua(&p1, vp, vold, &Lb);
    chk_cond(q, "PNP con las tensiones cambiadas: i cambiadas, g iguales (bit a bit)",
             Lb.i[0] == -La.i[0] && Lb.i[1] == -La.i[1] && Lb.v[0] == -La.v[0] && Lb.v[1] == -La.v[1] &&
             memcmp(Lb.g, La.g, sizeof La.g) == 0);
    vold[0] = 0.7;
    vold[1] = -4.3;
    vp[0] = 2.0;
    vp[1] = 5.0;
    lim = nl_evalua(&n1, vp, vold, &L);
    v = nl_pnjlim(2.0, 0.7, NL_VT, n1.m[5], &l2);
    chk_cond(q, "BJT: un salto de vBE a 2 V se limita con pnjlim (VT, su Vcrit)", lim == 1 && vold[0] == v && L.v[0] == v);

    /* MOS: puertos (G, S) sin corriente y (D, S) con ID */
    nl_arranque(&mn, vold);
    chk_cond(q, "arranque del MOS: (Vt, 0)", vold[0] == 2.0 && vold[1] == 0.0);
    vp[0] = 4.0;
    vp[1] = 4.0;
    lim = nl_evalua(&mn, vp, vold, &L);
    nl_mos(&mn, 4.0, 4.0, &m);
    chk_cond(q, "MOS de (Vt, 0) a (4, 4): sin límite; i = (0, ID), g = (0, 0; gm, gds)",
             lim == 0 && L.i[0] == 0.0 && L.g[0][0] == 0.0 && L.g[0][1] == 0.0 &&
             L.i[1] == m.id && L.g[1][0] == m.gm && L.g[1][1] == m.gds);
    nlt_rel(q, "MOS en (4, 4): 4,000000004 mA", L.i[1], 4.000000004e-3, 1e-12);
    {
        double x = 4.0, y = -5.0, w0 = vold[0], w1 = vold[1];
        int l = nl_mos_lim(&x, &y, w0, w1, 2.0);
        vp[0] = 4.0;
        vp[1] = -5.0;
        lim = nl_evalua(&mn, vp, vold, &L);
        nl_mos(&mn, x, y, &m);
        chk_cond(q, "MOS: los límites son los de nl_mos_lim y la evaluación, en lo limitado",
                 lim == l && vold[0] == x && vold[1] == y && L.v[0] == x && L.v[1] == y && L.i[1] == m.id);
    }
    /* los por tramos no se linealizan */
    memset(&L, 0x5A, sizeof L);
    lim = nl_evalua(&tr, vp, vold, &L);
    memset(&L2, 0, sizeof L2);
    chk_cond(q, "un diodo por tramos: 0 y L a cero", lim == 0 && memcmp(&L, &L2, sizeof L) == 0);
}

static void nlt_ao_regiones(void)
{
    const char *q = "nolin: AO, regiones y escala";
    static Valores v;
    SimEl ai = nlt_ao(0, 0.0, 15.0, -15.0), ar = nlt_ao(1, 1e5, 15.0, -15.0), a16 = nlt_ao(1, 65536.0, 15.0, -15.0);
    SimEl d = nlt_tramos(0.7, NL_RON, 0.0, NL_RON), zl = nlt_tramos(0.7, NL_RON, 5.1 - NL_RON * 5e-3, NL_RON);
    SimEl u = nlt_union(1e-14, 1.0, 0.0, 0.0, 0.0), z = nlt_zener(5.1, 0.049, 7.0);
    SimEl n1 = nlt_bjt(1, 1e-15, 100.0, 1.0, 0.0), p1 = nlt_bjt(-1, 1e-15, 100.0, 1.0, 0.0);
    SimEl mn = nlt_mos(1, 2.0, 1e-3, 0.0), mp = nlt_mos(-1, 2.0, 1e-3, 0.0), ec = nlt_el(SE_EC, NL_NO, 0);

    /* los márgenes del AO (§3.C): >= 0 dentro y 0 los dos en la frontera */
    chk_cond(q, "AO ideal, lineal: mín(V+ - Vsal, Vsal - V-)", nl_ao_margen(&ai, TR_LIN, 0.0, 3.0) == 12.0 &&
             nl_ao_margen(&ai, TR_LIN, 0.0, -14.0) == 1.0 && nl_ao_margen(&ai, TR_LIN, 0.0, 15.5) == -0.5);
    chk_cond(q, "AO ideal, saturado: Vd en SAT+ y -Vd en SAT-", nl_ao_margen(&ai, TR_SATP, 0.25, 15.0) == 0.25 &&
             nl_ao_margen(&ai, TR_SATN, 0.25, -15.0) == -0.25);
    chk_cond(q, "AO ideal en la frontera (Vsal = V+, Vd = 0): los dos márgenes en 0",
             nl_ao_margen(&ai, TR_LIN, 0.0, 15.0) == 0.0 && nl_ao_margen(&ai, TR_SATP, 0.0, 15.0) == 0.0 &&
             nl_ao_margen(&ai, TR_LIN, 0.0, -15.0) == 0.0 && nl_ao_margen(&ai, TR_SATN, 0.0, -15.0) == 0.0);
    nlt_rel(q, "AO real, lineal: mín(V+ - u, u - V-) con u = A·Vd", nl_ao_margen(&ar, TR_LIN, 1e-4, 0.0), 5.0, 1e-12);
    nlt_rel(q, "AO real, SAT+: u - V+", nl_ao_margen(&ar, TR_SATP, 1e-4, 0.0), -5.0, 1e-12);
    nlt_rel(q, "AO real, SAT-: V- - u", nl_ao_margen(&ar, TR_SATN, 1e-4, 0.0), -25.0, 1e-12);
    chk_cond(q, "AO real en la frontera (u = V+, con A = 2^16 para que u sea exacta): los dos márgenes en 0",
             nl_ao_margen(&a16, TR_LIN, 15.0 / 65536.0, 0.0) == 0.0 && nl_ao_margen(&a16, TR_SATP, 15.0 / 65536.0, 0.0) == 0.0 &&
             nl_ao_margen(&a16, TR_LIN, -15.0 / 65536.0, 0.0) == 0.0 && nl_ao_margen(&a16, TR_SATN, -15.0 / 65536.0, 0.0) == 0.0);

    /* las regiones (§3.H) con los Valores del §2.8: V, I y V2 */
#define NLT_REG(el, VV, II, V22, R, det) do { v.V[0] = (VV); v.I[0] = (II); v.V2[0] = (V22); \
        chk_cond(q, det, nl_region(&(el), &v, 0) == (R)); } while (0)
    NLT_REG(d, 0.8, 0.1, 0.0, REG_CONDUCE, "diodo con Vg a 0,8 V: conduce (su tramo)");
    NLT_REG(d, 0.3, 3e-13, 0.0, REG_CORTE, "diodo con Vg a 0,3 V: corte");
    NLT_REG(d, 0.7, 7e-13, 0.0, REG_CORTE, "diodo con Vg justo en el codo: corte");
    NLT_REG(zl, -6.0, -0.9, 0.0, REG_RUPTURA, "zener de libro a -6 V: ruptura");
    NLT_REG(u, 0.69, 4.3e-3, 0.0, REG_CONDUCE, "unión con I > 1 µA: conduce");
    NLT_REG(u, -5.0, -5e-12, 0.0, REG_CORTE, "unión en inversa: corte");
    NLT_REG(z, -4.94, -27e-3, 0.0, REG_RUPTURA, "1N4733A a -4,94 V y -27 mA: ruptura");
    NLT_REG(z, -2.0, -3e-6, 0.0, REG_CORTE, "1N4733A a -2 V (más que -Vz/2): corte aunque I < -1 µA");
    NLT_REG(n1, 5.4357760891729673, 2.4541907177293845e-3, 0.73789576485171988, REG_ACTIVA, "NPN del emisor común: activa");
    NLT_REG(n1, 0.091424804456153236, 21.506917662571294e-3, 0.79559221925032602, REG_SATURACION, "NPN del interruptor: saturación");
    NLT_REG(n1, 5.0, 1e-9, 0.2, REG_CORTE, "NPN con IC < 1 µA: corte");
    NLT_REG(n1, -5.0, -1e-3, -4.3, REG_INVERSA, "NPN con vBC = 0,7 V y vBE < 0: inversa (con |IC|)");
    NLT_REG(p1, -5.4357760891729673, -2.4541907177293845e-3, -0.73789576485171988, REG_ACTIVA, "PNP espejo: activa");
    NLT_REG(p1, 5.0, 1e-3, 4.3, REG_INVERSA, "PNP espejo: inversa");
    NLT_REG(mn, 4.0, 4e-3, 4.0, REG_SATURACION, "MOS N (4; 4): saturación");
    NLT_REG(mn, 0.5, 0.7e-3, 3.0, REG_OHMICA, "MOS N (3; 0,5): óhmica");
    NLT_REG(mn, 5.0, 5e-12, 1.0, REG_CORTE, "MOS N (1; 5): corte");
    NLT_REG(mn, -3.999999992, -4e-3, 8e-9, REG_SATURACION, "MOS N al revés (D y S cambiados): saturación");
    NLT_REG(mp, -4.0, -4e-3, -4.0, REG_SATURACION, "MOS P (-4; -4): saturación");
    NLT_REG(ai, -10.0, 1e-3, 1e-16, REG_LINEAL, "AO ideal con Vd = 0 y la salida dentro: lineal");
    NLT_REG(ai, 15.0, 0.0, 7.5, REG_SAT_MAS, "AO ideal con Vd > 0: satura +");
    NLT_REG(ai, -15.0, 0.0, -0.09, REG_SAT_MENOS, "AO ideal con Vd < 0: satura -");
    NLT_REG(ai, 15.0, 0.0, 0.0, REG_SAT_MAS, "AO ideal con Vd = 0 en V+ (comparador): satura +");
    NLT_REG(ar, -10.0, 0.0, -1e-4, REG_LINEAL, "AO real con u = -10 V: lineal");
    NLT_REG(ar, -15.0, 0.0, -0.0909, REG_SAT_MENOS, "AO real con u < V-: satura -");
    NLT_REG(ar, 15.0, 0.0, 2e-4, REG_SAT_MAS, "AO real con u > V+: satura +");
    NLT_REG(ec, 1.0, 1.0, 0.0, REG_NADA, "una E controlada: sin región");
#undef NLT_REG

    /* la escala de tensión (Vesc) */
    chk_cond(q, "escala: Vg del diodo, Vz0 del zener de libro, V+ - V- del AO, 0 en BJT y MOS",
             nl_escala_v(&d) == 0.7 && nl_escala_v(&zl) == zl.m[2] && nl_escala_v(&ai) == 30.0 &&
             nl_escala_v(&ar) == 30.0 && nl_escala_v(&u) == 0.0 && nl_escala_v(&n1) == 0.0 && nl_escala_v(&mn) == 0.0);
    nlt_rel(q, "escala del 1N4733A: Vz = BV + Rs·Ibv = 5,1 V", nl_escala_v(&z), 5.1, 1e-15);

    /* el contador: una exp() o un log(), NL_OPS_EXP */
    {
        int lim;
        nl_ops_reset();
        nl_pnjlim(1.0, 0.6, 0.025, 0.7, &lim);
        chk_cond(q, "pnjlim que limita: un log()", nl_exps() == 1 && nl_ops() >= NL_OPS_EXP && nl_ops() < 2 * NL_OPS_EXP);
        nl_ops_reset();
        nl_vcrit(1e-14, NL_VT);
        chk_cond(q, "nl_vcrit: un log()", nl_exps() == 1 && nl_ops() >= NL_OPS_EXP && nl_ops() < 2 * NL_OPS_EXP);
        nl_ops_reset();
        nl_fetlim(3.0, 1.0, 2.0);
        nl_limvds(3.0, 1.0);
        nl_tr_margen(&d, TR_CORTE, 0.3);
        chk_cond(q, "fetlim, limvds y un margen: sin exp(), pocas", nl_ops() > 0 && nl_ops() < 12 && nl_exps() == 0);
        nl_ops_reset();
        chk_cond(q, "nl_ops_reset lo deja en 0", nl_ops() == 0 && nl_exps() == 0);
    }
}

/* ==========================================================================
 *  h. Los casos cerrados, con un Newton pequeño
 *
 *  Potenciales de nodo como incógnitas; el nodo 0 es la tierra y un nodo con
 *  una E ideal a tierra se fija (su fila pasa a φ = E). Los no lineales se
 *  estampan como en el §3.C: los por tramos con su recta, los suaves con su
 *  NlLin (g y ieq_p = i_p - Σ g_pq·v_q). El Newton es el del §3.D en pequeño:
 *  el arranque de SPICE3 (nl_arranque y nl_evalua con NULL), los límites de
 *  nl_evalua y el paso de Katzenelson, con el margen de CORTE tomado del lado
 *  por el que sale. Converge sin límites y con el incremento por debajo de
 *  1e-9·Vesc; luego pule 3 iteraciones más.
 * ======================================================================= */
#define NLT_NODOS 14
#define NLT_EL    16
typedef struct {
    char   tipo;                      /* 'R', 'J' (de a hacia b por dentro) o 'N' (no lineal) */
    int    a, b, c;
    double val;
    SimEl  e;
} NltEl;
static struct {
    int    nn, nel;
    unsigned char es_fijo[NLT_NODOS];
    double fijo[NLT_NODOS];
    NltEl  el[NLT_EL];
    double x[NLT_NODOS];
    double vold[NLT_EL][2];
    NlLin  L[NLT_EL];
    unsigned char tramo[NLT_EL];
} nlt_red;

static void nlt_nueva(int nn)
{
    memset(&nlt_red, 0, sizeof nlt_red);
    nlt_red.nn = nn;
    nlt_red.es_fijo[0] = 1;
}
static void nlt_fija(int n, double v) { nlt_red.es_fijo[n] = 1; nlt_red.fijo[n] = v; }
static int nlt_pon(char tipo, int a, int b, int c, double val, const SimEl *e)
{
    NltEl *x = &nlt_red.el[nlt_red.nel];
    x->tipo = tipo;
    x->a = a;
    x->b = b;
    x->c = c;
    x->val = val;
    if (e) x->e = *e;
    return nlt_red.nel++;
}
static int nlt_r(int a, int b, double R) { return nlt_pon('R', a, b, 0, R, NULL); }
static int nlt_j(int a, int b, double J) { return nlt_pon('J', a, b, 0, J, NULL); }
static int nlt_nl(int a, int b, int c, SimEl e) { return nlt_pon('N', a, b, c, 0.0, &e); }
static int nlt_suave(const NltEl *x) { return x->tipo == 'N' && x->e.modelo != NL_TRAMOS; }

/* Puertos (§3.C): la unión (a, b); BJT y MOS (c, b) y (a, b) */
static int nlt_puertos(const NltEl *x, int pm[2], int pn[2])
{
    pm[0] = x->a;
    pn[0] = x->b;
    if (x->e.modelo != NL_BJT && x->e.modelo != NL_MOS) return 1;
    pm[0] = x->c;
    pm[1] = x->a;
    pn[1] = x->b;
    return 2;
}
static void nlt_vp(const NltEl *x, const double *phi, double vp[2])
{
    int pm[2], pn[2], np = nlt_puertos(x, pm, pn), p;
    vp[1] = 0.0;
    for (p = 0; p < np; p++) vp[p] = phi[pm[p]] - phi[pn[p]];
}

static int nlt_gauss(int n, double A[][NLT_NODOS], double *b, double *x)
{
    int i, j, k, p;
    for (k = 0; k < n; k++) {
        p = k;
        for (i = k + 1; i < n; i++) if (fabs(A[i][k]) > fabs(A[p][k])) p = i;
        if (A[p][k] == 0.0) return 0;
        if (p != k) {
            double t;
            for (j = 0; j < n; j++) { t = A[k][j]; A[k][j] = A[p][j]; A[p][j] = t; }
            t = b[k]; b[k] = b[p]; b[p] = t;
        }
        for (i = k + 1; i < n; i++) {
            double f = A[i][k] / A[k][k];
            if (f == 0.0) continue;
            for (j = k; j < n; j++) A[i][j] -= f * A[k][j];
            b[i] -= f * b[k];
        }
    }
    for (i = n - 1; i >= 0; i--) {
        double s = b[i];
        for (j = i + 1; j < n; j++) s -= A[i][j] * x[j];
        x[i] = s / A[i][i];
    }
    return 1;
}

/* Filas = lo que sale de cada nodo; las de los nodos fijos, φ = su valor */
static int nlt_resuelve(double *xn)
{
    static double A[NLT_NODOS][NLT_NODOS];
    double b[NLT_NODOS];
    int n = nlt_red.nn, k, p, r, i, j;
    memset(A, 0, sizeof A);
    memset(b, 0, sizeof b);
    for (k = 0; k < nlt_red.nel; k++) {
        const NltEl *x = &nlt_red.el[k];
        int pm[2], pn[2], np;
        if (x->tipo == 'J') { b[x->a] -= x->val; b[x->b] += x->val; continue; }
        if (!nlt_suave(x)) {                          /* R, o un diodo por tramos en su recta */
            double g = 1.0 / x->val, i0 = 0.0;
            if (x->tipo == 'N') nl_tr_recta(&x->e, nlt_red.tramo[k], &g, &i0);
            A[x->a][x->a] += g; A[x->a][x->b] -= g; A[x->b][x->b] += g; A[x->b][x->a] -= g;
            b[x->a] -= i0; b[x->b] += i0;
            continue;
        }
        np = nlt_puertos(x, pm, pn);
        for (p = 0; p < np; p++) {
            double ieq = nlt_red.L[k].i[p];
            for (r = 0; r < np; r++) {
                double g = nlt_red.L[k].g[p][r];
                A[pm[p]][pm[r]] += g; A[pm[p]][pn[r]] -= g;
                A[pn[p]][pm[r]] -= g; A[pn[p]][pn[r]] += g;
                ieq -= g * nlt_red.L[k].v[r];
            }
            b[pm[p]] -= ieq;
            b[pn[p]] += ieq;
        }
    }
    for (i = 0; i < n; i++)
        if (nlt_red.es_fijo[i]) { for (j = 0; j < n; j++) A[i][j] = 0.0; A[i][i] = 1.0; b[i] = nlt_red.fijo[i]; }
    return nlt_gauss(n, A, b, xn);
}

/* Devuelve la iteración en que convergió, o 0 */
static int nlt_newton(int itmax)
{
    double xn[NLT_NODOS], vesc = 1.0, vp[2];
    int it, k, n = nlt_red.nn, conv = 0, pulido = 0;
    for (k = 0; k < n; k++) {
        nlt_red.x[k] = nlt_red.es_fijo[k] ? nlt_red.fijo[k] : 0.0;
        vesc = nlt_max(vesc, fabs(nlt_red.fijo[k]));
    }
    for (k = 0; k < nlt_red.nel; k++) {
        NltEl *x = &nlt_red.el[k];
        nlt_red.tramo[k] = TR_CORTE;
        if (nlt_suave(x)) {
            nl_arranque(&x->e, nlt_red.vold[k]);
            nl_evalua(&x->e, NULL, nlt_red.vold[k], &nlt_red.L[k]);
        }
    }
    for (it = 1; it <= itmax; it++) {
        double th = 1.0, dx = 0.0;
        int kq = -1, lim = 0;
        if (!nlt_resuelve(xn)) return 0;
        for (k = 0; k < nlt_red.nel; k++) {           /* Katzenelson (§3.D, 3.b) */
            const NltEl *x = &nlt_red.el[k];
            int t = nlt_red.tramo[k];
            double v0, v1, m0, m1, tq;
            if (x->tipo != 'N' || x->e.modelo != NL_TRAMOS) continue;
            v0 = nlt_red.x[x->a] - nlt_red.x[x->b];
            v1 = xn[x->a] - xn[x->b];
            m1 = nl_tr_margen(&x->e, t, v1);
            if (m1 >= -1e-12 * vesc) continue;
            if (t == TR_CORTE) {                      /* el lado por el que sale */
                int s = nl_tr_vecino(&x->e, t, v1);
                m0 = -nl_tr_margen(&x->e, s, v0);
                m1 = -nl_tr_margen(&x->e, s, v1);
            } else m0 = nl_tr_margen(&x->e, t, v0);
            tq = m0 > 0.0 ? m0 / (m0 - m1) : 0.0;
            if (tq < th) { th = tq; kq = k; }
        }
        if (kq >= 0) {
            const NltEl *x = &nlt_red.el[kq];
            for (k = 0; k < n; k++) nlt_red.x[k] += th * (xn[k] - nlt_red.x[k]);
            nlt_red.tramo[kq] = (unsigned char)nl_tr_vecino(&x->e, nlt_red.tramo[kq], nlt_red.x[x->a] - nlt_red.x[x->b]);
            for (k = 0; k < nlt_red.nel; k++)
                if (nlt_suave(&nlt_red.el[k])) {
                    nlt_vp(&nlt_red.el[k], nlt_red.x, vp);
                    nl_evalua(&nlt_red.el[k].e, vp, nlt_red.vold[k], &nlt_red.L[k]);
                }
            conv = pulido = 0;
            continue;
        }
        for (k = 0; k < nlt_red.nel; k++)
            if (nlt_suave(&nlt_red.el[k])) {
                nlt_vp(&nlt_red.el[k], xn, vp);
                lim |= nl_evalua(&nlt_red.el[k].e, vp, nlt_red.vold[k], &nlt_red.L[k]);
            }
        for (k = 0; k < n; k++) dx = nlt_max(dx, fabs(xn[k] - nlt_red.x[k]));
        memcpy(nlt_red.x, xn, sizeof xn);
        if (conv) { if (lim) conv = pulido = 0; else if (++pulido == 3) return conv; }
        else if (!lim && dx <= 1e-9 * vesc) conv = it;
    }
    return 0;
}

/* La corriente de a hacia b (diodos) o la que entra por a (IC, ID) */
static double nlt_i(int k)
{
    const NltEl *x = &nlt_red.el[k];
    if (x->e.modelo == NL_TRAMOS) {
        double g, i0;
        nl_tr_recta(&x->e, nlt_red.tramo[k], &g, &i0);
        return g * (nlt_red.x[x->a] - nlt_red.x[x->b]) + i0;
    }
    return x->e.modelo == NL_UNION ? nlt_red.L[k].i[0] : nlt_red.L[k].i[1];
}
static double nlt_phi(int n) { return nlt_red.x[n]; }
/* El redondeo de esa corriente en un diodo por tramos: con Ron = 1 mΩ, g·v +
 * i0 resta dos números de cientos de amperios para dar mA, y v solo se conoce
 * a eps·|φ|; la I pierde unas 11 cifras. Lo mismo le pasará a valores_de
 * (§3.H). 0 en los suaves, cuya I sale de su modelo en su vd. */
static double nlt_i_ruido(int k)
{
    const NltEl *x = &nlt_red.el[k];
    double g, i0, v;
    if (x->e.modelo != NL_TRAMOS) return 0.0;
    nl_tr_recta(&x->e, nlt_red.tramo[k], &g, &i0);
    v = nlt_red.x[x->a] - nlt_red.x[x->b];
    return 8.0 * DBL_EPSILON * (fabs(g * v) + fabs(i0) + fabs(g) * nlt_max(fabs(nlt_red.x[x->a]), fabs(nlt_red.x[x->b])));
}

/* E en serie con R y el diodo */
static int nlt_diodo_r(SimEl d, double E, double R, double *I, double *V, double *ruido)
{
    int k, it;
    nlt_nueva(3);
    nlt_fija(1, E);
    nlt_r(1, 2, R);
    k = nlt_nl(2, 0, 0, d);
    it = nlt_newton(100);
    *I = nlt_i(k);
    *V = nlt_phi(2);
    *ruido = nlt_i_ruido(k);
    return it;
}

/* El regulador: E 12 V (o la que se diga), Rs 220 Ω, el zener con el cátodo
 * arriba y RL (0 = sin carga) */
static int nlt_regulador(SimEl z, double E, double RL, double *Vo, double *Iz, double *ruido)
{
    int k, it;
    nlt_nueva(3);
    nlt_fija(1, E);
    nlt_r(1, 2, 220.0);
    k = nlt_nl(0, 2, 0, z);
    if (RL > 0.0) nlt_r(2, 0, RL);
    it = nlt_newton(100);
    *Vo = nlt_phi(2);
    *Iz = fabs(nlt_i(k));
    *ruido = nlt_i_ruido(k);
    return it;
}

/* El emisor común con divisor: VCC, R1 (VCC-B), R2 (B-0), RC (VCC-C), RE (E-0) */
static int nlt_emisor_comun(SimEl q, double VCC, double R1, double R2, double RC, double RE, int *kq)
{
    nlt_nueva(5);                                     /* 1 VCC, 2 B, 3 E, 4 C */
    nlt_fija(1, VCC);
    nlt_r(1, 2, R1);
    nlt_r(2, 0, R2);
    nlt_r(1, 4, RC);
    nlt_r(3, 0, RE);
    *kq = nlt_nl(4, 3, 2, q);
    return nlt_newton(100);
}

static void nlt_cerrados(void)
{
    const char *q = "nolin: casos cerrados (Newton de la prueba)";
    static const struct { int m; double E, R, I, V, tol; const char *det; } DR[] = {
        { 0, 5, 1e3, 4.3071121676219442e-3, 0.69288783237805583, 1e-12, "exponencial, 5 V y 1 kohm" },
        { 1, 5, 1e3, 4.3467715385002356e-3, 0.65322846149976443, 1e-12, "1N4148, 5 V y 1 kohm" },
        { 2, 5, 1e3, 4.375928266602544e-3, 0.62407173339745597, 1e-12, "1N4007, 5 V y 1 kohm" },
        { 2, 10, 100, 92.303421120134247e-3, 0.76965788798657532, 1e-12, "1N4007, 10 V y 100 ohm" },
        { 0, -5, 1e3, -5.00999999499e-12, -4.99999999499, 1e-9, "exponencial, -5 V y 1 kohm" },
        { 0, 100, 1, 99.047348503037482, 0.95265149696251774, 1e-12, "exponencial, 100 V y 1 ohm" },
        { 0, 1, 1e6, 539.51149881271225e-9, 0.46048850118728775, 1e-12, "exponencial, 1 V y 1 Mohm" },
        { 0, 0.3, 1e3, 1.0898251756675721e-9, 0.29999891017482433, 1e-9, "exponencial, 0,3 V y 1 kohm" },
        { 3, 5, 1e3, 4.999995000005e-3, 4.999995000005e-6, 1e-12, "ideal, 5 V y 1 kohm" },
        { 3, -5, 1e3, -4.999999995e-12, -4.999999995, 1e-9, "ideal, -5 V y 1 kohm" },
        { 4, 5, 1e3, 4.2999957000043007e-3, 0.7000042999956993, 1e-12, "con Vg 0,7, 5 V y 1 kohm" },
        { 5, 5, 1e3, 4.2574257425811881e-3, 0.74257425741881188, 1e-12, "con Vg 0,7 y rd 10 ohm, 5 V y 1 kohm" },
    };
    static const struct { int m; double E, RL, Vo, Iz; const char *det; } ZR[] = {
        { 0, 12, 1e3, 5.1000212635184427, 26.263518447761363e-3, "zener de libro (rz, Iz vacías), RL 1 kohm" },
        { 0, 12, 100, 3.7499999997421875, 3.7499999997421875e-12, "zener de libro, RL 100 ohm (corte)" },
        { 0, 12, 0, 5.1000263635165244, 31.363516529470344e-3, "zener de libro, sin carga" },
        { 1, 12, 1e3, 4.9467926839619945, 27.113240570756212e-3, "zener de libro rz 7 ohm, Iz 49 mA, RL 1 kohm" },
        { 2, 12, 1e3, 4.9431873823062371, 27.133233607210867e-3, "1N4733A, RL 1 kohm" },
        { 2, 12, 0, 4.9783457050491179, 31.916610431594919e-3, "1N4733A, sin carga" },
        { 2, 10, 1e3, 4.8767051158608403, 18.410998902953522e-3, "1N4733A con E = 10 V" },
        { 2, 14, 1e3, 5.006956672510536, 35.870512997896119e-3, "1N4733A con E = 14 V" },
        { 3, 12, 1e3, 5.1426692944195435, 26.027015730946168e-3, "zener exponencial (Rs = 0), RL 1 kohm" },
    };
    static const struct { double E, R, I, V; const char *det; } LR[] = {
        { 5, 160, 20e-3, 1.8, "LED rojo con 5 V y 160 ohm: 20,000 mA" },
        { 9, 330, 21.777894098247443e-3, 1.8132949475783439, "LED rojo con 9 V y 330 ohm" },
        { 5, 1e3, 3.375166452816769e-3, 1.624833547183231, "LED rojo con 5 V y 1 kohm" },
        { 9, 360, 20e-3, 1.8, "LED rojo con 9 V y 360 ohm: 20 mA" },
    };
    SimEl DIO[6], ZEN[4], led = nlt_union(nlt_led_is(1.8, 2.0, 5.0, NL_IF), 2.0, 5.0, 0.0, 0.0);
    char det[140];
    double I, V, Vo, Iz, ru;
    int i, it, k1, k2, kq, it100 = 0, it10 = 0, itinv = 0;
    static Valores val;

    DIO[0] = nlt_union(1e-14, 1.0, 0.0, 0.0, 0.0);
    DIO[1] = nlt_union(NL_1N4148.Is, NL_1N4148.n, NL_1N4148.Rs, 0.0, 0.0);
    DIO[2] = nlt_union(NL_1N4007.Is, NL_1N4007.n, NL_1N4007.Rs, 0.0, 0.0);
    DIO[3] = nlt_tramos(0.0, NL_RON, 0.0, NL_RON);
    DIO[4] = nlt_tramos(0.7, NL_RON, 0.0, NL_RON);
    DIO[5] = nlt_tramos(0.7, 10.0, 0.0, NL_RON);
    ZEN[0] = nlt_tramos(0.7, NL_RON, 5.1 - NL_RON * NL_IZ, NL_RON);
    ZEN[1] = nlt_tramos(0.7, NL_RON, 5.1 - 7.0 * 0.049, 7.0);
    ZEN[2] = nlt_zener(5.1, 0.049, 7.0);
    ZEN[3] = nlt_zener(5.1, NL_IZ, 0.0);

    /* §6.5: E, R y un diodo */
    for (i = 0; i < (int)(sizeof DR / sizeof DR[0]); i++) {
        it = nlt_diodo_r(DIO[DR[i].m], DR[i].E, DR[i].R, &I, &V, &ru);
        if (DR[i].E == 100) it100 = it;
        snprintf(det, sizeof det, "%s: converge", DR[i].det);
        chk_cond(q, det, it > 0);
        snprintf(det, sizeof det, "%s: I (n1.py; en los por tramos, más el redondeo de su recta)", DR[i].det);
        chk_t(q, det, I, DR[i].I, 1.0, DR[i].tol * fabs(DR[i].I) + ru);
        snprintf(det, sizeof det, "%s: V", DR[i].det);
        nlt_rel(q, det, V, DR[i].V, DR[i].tol);
    }
    chk_cond(q, "exponencial con 100 V y 1 ohm: en 40 iteraciones o menos", it100 > 0 && it100 <= 40);
    /* dos y diez exponenciales en serie */
    nlt_nueva(4);
    nlt_fija(1, 5.0);
    nlt_r(1, 2, 1e3);
    k1 = nlt_nl(2, 3, 0, DIO[0]);
    k2 = nlt_nl(3, 0, 0, DIO[0]);
    chk_cond(q, "2 exponenciales en serie, 5 V y 1 kohm: converge", nlt_newton(100) > 0);
    nlt_rel(q, "2 exponenciales en serie: I", nlt_i(k1), 3.6231693828557282e-3, 1e-12);
    nlt_rel(q, "2 exponenciales en serie: V de cada uno", nlt_phi(2) - nlt_phi(3), 0.68841530857213592, 1e-12);
    nlt_rel(q, "2 exponenciales en serie: V del de abajo", nlt_phi(3), 0.68841530857213592, 1e-12);
    nlt_rel(q, "2 exponenciales en serie: la misma I en los dos", nlt_i(k2), nlt_i(k1), 1e-14);
    nlt_nueva(12);                                    /* 1 = E; los diodos de 2 a 11 y a tierra */
    nlt_fija(1, 10.0);
    nlt_r(1, 2, 10.0);
    for (i = 0; i < 10; i++) k1 = nlt_nl(2 + i, i == 9 ? 0 : 3 + i, 0, DIO[0]);
    it10 = nlt_newton(100);
    chk_cond(q, "10 exponenciales en serie, 10 V y 10 ohm: converge sin homotopía", it10 > 0);
    nlt_rel(q, "10 exponenciales en serie: I (n7.py)", nlt_i(k1), 206.95689299461996e-3, 1e-12);
    nlt_rel(q, "10 exponenciales en serie: V del primero", nlt_phi(2) - nlt_phi(3), 0.79304310700538004, 1e-12);
    nlt_rel(q, "10 exponenciales en serie: V del último", nlt_phi(11), 0.79304310700538004, 1e-12);
    /* el OR de dos ideales: Katzenelson con dos diodos que empiezan fuera */
    nlt_nueva(4);
    nlt_fija(1, 5.0);
    nlt_fija(2, 3.0);
    k1 = nlt_nl(1, 3, 0, DIO[3]);
    k2 = nlt_nl(2, 3, 0, DIO[3]);
    nlt_r(3, 0, 1e3);
    chk_cond(q, "OR de dos ideales: converge", nlt_newton(100) > 0);
    nlt_rel(q, "OR de dos ideales: Vn", nlt_phi(3), 4.999995000004998, 1e-9);
    nlt_rel(q, "OR de dos ideales: I(D1)", nlt_i(k1), 4.999995002004993e-3, 1e-9);
    nlt_rel(q, "OR de dos ideales: I(D2)", nlt_i(k2), -1.999995000004998e-12, 1e-9);
    nlt_rel(q, "OR de dos ideales: V(D2)", nlt_phi(2) - nlt_phi(3), -1.999995000004998, 1e-9);
    val.V[0] = nlt_phi(1) - nlt_phi(3);
    val.V[1] = nlt_phi(2) - nlt_phi(3);
    chk_cond(q, "OR de dos ideales: D1 conduce y D2 en corte",
             nl_region(&DIO[3], &val, 0) == REG_CONDUCE && nl_region(&DIO[3], &val, 1) == REG_CORTE &&
             nlt_red.tramo[k1] == TR_CONDUCE && nlt_red.tramo[k2] == TR_CORTE);

    /* §6.5: el regulador zener */
    for (i = 0; i < (int)(sizeof ZR / sizeof ZR[0]); i++) {
        it = nlt_regulador(ZEN[ZR[i].m], ZR[i].E, ZR[i].RL, &Vo, &Iz, &ru);
        snprintf(det, sizeof det, "%s: converge", ZR[i].det);
        chk_cond(q, det, it > 0);
        snprintf(det, sizeof det, "%s: Vo (n1.py)", ZR[i].det);
        nlt_rel(q, det, Vo, ZR[i].Vo, 1e-12);
        snprintf(det, sizeof det, "%s: |I(Dz)| (más el redondeo de la recta)", ZR[i].det);
        chk_t(q, det, Iz, ZR[i].Iz, 1.0, 1e-12 * ZR[i].Iz + ru);
    }
    /* §6.5: el LED rojo */
    for (i = 0; i < (int)(sizeof LR / sizeof LR[0]); i++) {
        it = nlt_diodo_r(led, LR[i].E, LR[i].R, &I, &V, &ru);
        snprintf(det, sizeof det, "%s: I", LR[i].det);
        chk_cond(q, det, it > 0 && fabs(I - LR[i].I) <= 1e-12 * LR[i].I);
        snprintf(det, sizeof det, "%s: V", LR[i].det);
        nlt_rel(q, det, V, LR[i].V, 1e-12);
    }

    /* §6.11: el BJT */
    it = nlt_emisor_comun(nlt_bjt(1, 1e-15, 140.0, 1.0, 0.0), 22.0, 39e3, 3.9e3, 10e3, 1.5e3, &kq);
    chk_cond(q, "emisor común de Boylestad: converge", it > 0);
    nlt_rel(q, "Boylestad: VBE (n2.py)", nlt_phi(2) - nlt_phi(3), 0.71015607662840807, 1e-9);
    nlt_rel(q, "Boylestad: IB", nlt_red.L[kq].i[0], 5.9979952449108933e-6, 1e-9);
    nlt_rel(q, "Boylestad: IC", nlt_red.L[kq].i[1], 0.83972087399969434e-3, 1e-9);
    nlt_rel(q, "Boylestad: VCE", nlt_phi(4) - nlt_phi(3), 12.334212956136149, 1e-9);
    it = nlt_emisor_comun(nlt_bjt(1, 1e-15, 140.0, 1.0, 100.0), 22.0, 39e3, 3.9e3, 10e3, 1.5e3, &kq);
    nlt_rel(q, "Boylestad con VA 100 V: VBE", nlt_phi(2) - nlt_phi(3), 0.7074386661541133, 1e-9);
    nlt_rel(q, "Boylestad con VA 100 V: IC", nlt_red.L[kq].i[1], 0.84354456360751576e-3, 1e-9);
    nlt_rel(q, "Boylestad con VA 100 V: VCE", nlt_phi(4) - nlt_phi(3), 12.291137805735711, 1e-9);
    it = nlt_emisor_comun(nlt_bjt(1, 1e-15, 100.0, 1.0, 0.0), 12.0, 47e3, 10e3, 2.2e3, 470.0, &kq);
    chk_cond(q, "emisor común de la biblioteca: converge", it > 0);
    {
        double vb = nlt_phi(2), vbe = nlt_phi(2) - nlt_phi(3), ib = nlt_red.L[kq].i[0], ic = nlt_red.L[kq].i[1];
        double vce = nlt_phi(4) - nlt_phi(3);
        NlBjt o;
        nlt_rel(q, "biblioteca: VB", vb, 1.9029000966741065, 1e-9);
        nlt_rel(q, "biblioteca: VBE", vbe, 0.73789576485171988, 1e-9);
        nlt_rel(q, "biblioteca: IB", ib, 24.541903169310483e-6, 1e-9);
        nlt_rel(q, "biblioteca: IC", ic, 2.4541907177293845e-3, 1e-9);
        nlt_rel(q, "biblioteca: VCE", vce, 5.4357760891729673, 1e-9);
        nl_bjt(&nlt_red.el[kq].e, vbe, vbe - vce, &o);
        nlt_rel(q, "biblioteca: gm = dIC/dvBE en Q", o.dic_be, 0.094884892897226069, 1e-9);
        nlt_rel(q, "biblioteca: rpi = 1/(dIB/dvBE) en Q", 1.0 / o.dib_be, 1053.9085500462489, 1e-9);
        val.V[0] = vce; val.I[0] = ic; val.V2[0] = vbe;
        chk_cond(q, "biblioteca: región activa", nl_region(&nlt_red.el[kq].e, &val, 0) == REG_ACTIVA);
        /* el PNP con VCC = -12 V: lo mismo cambiado de signo */
        it = nlt_emisor_comun(nlt_bjt(-1, 1e-15, 100.0, 1.0, 0.0), -12.0, 47e3, 10e3, 2.2e3, 470.0, &kq);
        chk_cond(q, "el PNP con VCC = -12 V: converge", it > 0);
        nlt_rel(q, "PNP: VB = -VB del NPN", nlt_phi(2), -vb, 1e-12);
        nlt_rel(q, "PNP: VBE", nlt_phi(2) - nlt_phi(3), -vbe, 1e-12);
        nlt_rel(q, "PNP: IB", nlt_red.L[kq].i[0], -ib, 1e-12);
        nlt_rel(q, "PNP: IC", nlt_red.L[kq].i[1], -ic, 1e-12);
        nlt_rel(q, "PNP: VCE", nlt_phi(4) - nlt_phi(3), -vce, 1e-12);
    }
    /* el interruptor: 5 V por RB a la base; LED rojo y 330 Ω a 9 V en el colector */
    nlt_nueva(6);                                     /* 1 VBB, 2 B, 3 VCC, 4 ánodo, 5 C */
    nlt_fija(1, 5.0);
    nlt_fija(3, 9.0);
    nlt_r(1, 2, 4.7e3);
    kq = nlt_nl(5, 0, 2, nlt_bjt(1, 1e-15, 100.0, 1.0, 0.0));
    nlt_r(3, 4, 330.0);
    k1 = nlt_nl(4, 5, 0, led);
    chk_cond(q, "interruptor con LED: converge", nlt_newton(100) > 0);
    nlt_rel(q, "interruptor: VBE", nlt_phi(2), 0.79559221925032602, 1e-9);
    nlt_rel(q, "interruptor: VCE", nlt_phi(5), 0.091424804456153236, 1e-9);
    nlt_rel(q, "interruptor: I(LED) = IC", nlt_i(k1), 21.506917662571294e-3, 1e-9);
    nlt_rel(q, "interruptor: IC", nlt_red.L[kq].i[1], 21.506917662571294e-3, 1e-9);
    nlt_rel(q, "interruptor: IB", nlt_red.L[kq].i[0], 0.89455484696801574e-3, 1e-9);
    nlt_rel(q, "interruptor: IC/IB", nlt_red.L[kq].i[1] / nlt_red.L[kq].i[0], 24.042033571743936, 1e-9);
    val.V[0] = nlt_phi(5); val.I[0] = nlt_red.L[kq].i[1]; val.V2[0] = nlt_phi(2);
    chk_cond(q, "interruptor: saturación", nl_region(&nlt_red.el[kq].e, &val, 0) == REG_SATURACION);
    /* la familia del trazador (§6.10): VCE = 5 V e IB fija */
    for (i = 1; i <= 2; i++) {
        static const double IC[3] = { 0, 2.0000003578488944e-3, 4.0000003542453266e-3 };
        static const double VBE[3] = { 0, 0.73260251552082675, 0.75053071359549415 };
        nlt_nueva(3);                                 /* 1 C a 5 V, 2 B */
        nlt_fija(1, 5.0);
        nlt_j(0, 2, 20e-6 * i);
        kq = nlt_nl(1, 0, 2, nlt_bjt(1, 1e-15, 100.0, 1.0, 0.0));
        it = nlt_newton(100);
        snprintf(det, sizeof det, "familia del BJT, IB = %d µA: IC (n2.py)", 20 * i);
        chk_cond(q, det, it > 0 && fabs(nlt_red.L[kq].i[1] - IC[i]) <= 1e-9 * IC[i]);
        snprintf(det, sizeof det, "familia del BJT, IB = %d µA: VBE", 20 * i);
        nlt_rel(q, det, nlt_phi(2), VBE[i], 1e-9);
    }

    /* §6.11: el MOSFET */
    nlt_nueva(4);                                     /* 1 G, 2 VDD, 3 D */
    nlt_fija(1, 4.0);
    nlt_fija(2, 12.0);
    nlt_r(2, 3, 2e3);
    kq = nlt_nl(3, 0, 1, nlt_mos(1, 2.0, 1e-3, 0.0));
    chk_cond(q, "MOSFET a) VGS 4 V, RD 2 kohm, VDD 12 V: converge", nlt_newton(100) > 0);
    nlt_rel(q, "MOSFET a): ID (n2.py)", nlt_i(kq), 4.000000004e-3, 1e-9);
    nlt_rel(q, "MOSFET a): VDS", nlt_phi(3), 3.999999992, 1e-9);
    val.V[0] = nlt_phi(3); val.V2[0] = 4.0;
    chk_cond(q, "MOSFET a): saturación", nl_region(&nlt_red.el[kq].e, &val, 0) == REG_SATURACION);
    nlt_red.el[kq].e.m[2] = 50.0;
    chk_cond(q, "MOSFET a) con VA 50 V: converge", nlt_newton(100) > 0);
    nlt_rel(q, "MOSFET a) con VA 50 V: ID", nlt_i(kq), 4.2758620719381688e-3, 1e-9);
    nlt_rel(q, "MOSFET a) con VA 50 V: VDS", nlt_phi(3), 3.4482758561236623, 1e-9);
    nlt_nueva(5);                                     /* 1 G, 2 VDD, 3 D, 4 S */
    nlt_fija(1, 6.0);
    nlt_fija(2, 12.0);
    nlt_r(2, 3, 2e3);
    nlt_r(4, 0, 1e3);
    kq = nlt_nl(3, 4, 1, nlt_mos(1, 2.0, 1e-3, 0.0));
    chk_cond(q, "MOSFET b) VG 6 V, RS 1 kohm, RD 2 kohm: converge", nlt_newton(100) > 0);
    nlt_rel(q, "MOSFET b): ID", nlt_i(kq), 2.4384471883273663e-3, 1e-9);
    nlt_rel(q, "MOSFET b): VGS", 6.0 - nlt_phi(4), 3.5615528116726337, 1e-9);
    nlt_rel(q, "MOSFET b): VDS", nlt_phi(3) - nlt_phi(4), 4.6846584350179011, 1e-9);
    nlt_nueva(4);                                     /* al revés: D a tierra, 1 G, 2 VDD, 3 S */
    nlt_fija(1, 4.0);
    nlt_fija(2, 12.0);
    nlt_r(2, 3, 2e3);
    kq = nlt_nl(0, 3, 1, nlt_mos(1, 2.0, 1e-3, 0.0));
    itinv = nlt_newton(100);
    chk_cond(q, "MOSFET dibujado al revés: converge sin homotopía en menos de 10 iteraciones", itinv > 0 && itinv < 10);
    nlt_rel(q, "MOSFET al revés: ID = -4,000000004 mA", nlt_i(kq), -4.000000004e-3, 1e-9);
    nlt_rel(q, "MOSFET al revés: VDS = -3,999999992 V", -nlt_phi(3), -3.999999992, 1e-9);
    val.V[0] = -nlt_phi(3); val.V2[0] = 4.0 - nlt_phi(3);
    chk_cond(q, "MOSFET al revés: saturación (con D y S cambiados)", nl_region(&nlt_red.el[kq].e, &val, 0) == REG_SATURACION);

    printf("    Newton de la prueba: 100 V sobre 1 ohm en %d iter.; 10 diodos en serie en %d; MOSFET al revés en %d\n",
           it100, it10, itinv);
}

/* ----------------------------------------------------------------------- */
static void test_nolin(void)
{
    int antes = pruebas;
    printf("[55n] Etapa 4: los modelos (nolin.c)\n");
    nlt_constantes();
    nlt_exp();
    nlt_limites();
    nlt_por_tramos();
    nlt_derivadas();
    nlt_d5_d6();
    nlt_evalua_prueba();
    nlt_ao_regiones();
    nlt_cerrados();
    printf("    %d comprobaciones de los modelos\n", pruebas - antes);
}
