/* --------------------------------------------------------------------------
 *  67. Redes no lineales al azar para el oráculo (out/oraculo_nl)
 *
 *  El cierre de la Etapa 4 (paso 15 del §7, §6.13). Como [29] y [29b], el
 *  arnés arma redes al azar, las resuelve con el motor y escribe en
 *  out/oraculo_nl:
 *    caso_NNN.net  la red en CESNET 3 (herramientas/oraculo_nl.py)
 *    caso_NNN.ces  lo que da el programa. En CC (casos 000 a 199, con
 *                  sim_op_cc_red): el potencial de cada nodo, la corriente de
 *                  cada elemento y la de base de cada BJT. En el tiempo
 *                  (casos 200 a 259, con sim_emular y el paso automático):
 *                  t, canal 1 y canal 2 en el punto calculado más cercano a
 *                  cada k·tfin/400 (en un evento, el de después, como [29]).
 *    caso_NNN_h4.ces  lo mismo con un cuarto de ese paso (dato): el oráculo
 *                  mira si el error baja, como el h/2 de [29].
 *    rechazada_NNN.net  (y .ces) una red del tiempo que el motor no emula
 *                  (NO_CONVERGE), con su mensaje; si con 50 iteraciones de
 *                  Newton por paso (sim_nl_topes) sí, lo que da así. No
 *                  cuentan entre las 60: el oráculo dice si él la integra.
 *    rechazada_cc_NNN.net  una red de CC en la que el punto de operación no
 *                  converge: el oráculo dice si tiene solución.
 *  herramientas/oraculo_nl.py resuelve cada red con su propio MNA no lineal y
 *  compara. Aquí solo se comprueba que el motor acepta las redes.
 *
 *  La semilla es propia (ora_sem): azar() no se toca, así que las secciones
 *  de después ven la misma secuencia que sin esta.
 *
 *  Las redes de CC son del todo al azar: R, E (ideal o con r) y J, diodos
 *  exponenciales y de catálogo, zener exponencial y 1N4733A, LED, AMI y VMI,
 *  E y J controladas por la V o la I de otro elemento de dos bornes, AO ideal
 *  y real (realimentación negativa, positiva o ninguna) con su salida medida
 *  desde la tierra, y BJT NPN y PNP polarizados con R al azar. Pueden tener
 *  varias soluciones (el Schmitt): el oráculo busca las suyas y la del
 *  motor tiene que ser una de ellas.
 *
 *  Las del tiempo tienen un núcleo pasivo (R, C, L, diodos con su R en serie,
 *  interruptores con su R, E y J con forma; un C que cerraría un lazo con la E
 *  ideal o con otros C va con una R en serie) y etapas que lo leen sin
 *  realimentarlo: un AO inversor (+ a tierra) o no inversor (R1 a tierra),
 *  con o sin C en paralelo con Rf; un emisor común con su VCC, divisor y C de
 *  acoplo (tras una R) o RB; una E o J controlada por la V o la I de un
 *  elemento del núcleo. Un AO que realimenta el núcleo por R1 lo vería como una
 *  controlada y el algebraico podría tener varias soluciones en un instante;
 *  así, el del oráculo tiene una sola. Se descartan las que el motor avisa
 *  como degeneradas (C_DESC, L_SIN_I, CI_SALTA, IMPULSO, CARGA, FLUJO, FLOTA,
 *  BIESTABLE, ARRANQUE): ahí el estado salta en 0+ o el algebraico es
 *  singular, y el oráculo no lo trata (el [29] sí, con otro oráculo).
 *
 *  Sin diodos ideales, con Vγ ni zener de libro (los por tramos van por los
 *  casos cerrados, §6.13). El MOSFET, tras ORA_CON_MOS (paso 14).
 * ----------------------------------------------------------------------- */
#include "nolin.h"

/* 1 desde que el motor tiene el MOSFET (paso 14): el generador lo pone en las
 * redes de CC y en las etapas del tiempo */
#define ORA_CON_MOS 1

#define ORA_NCC 200             /* redes de CC                              */
#define ORA_NTR 60              /* redes en el tiempo                       */
#define ORA_MUESTRAS 400        /* instantes del tiempo: k·tfin/400          */
#define ORA_AV_MAL (SIM_AV_C_DESC | SIM_AV_L_SIN_I | SIM_AV_CI_SALTA | SIM_AV_IMPULSO | SIM_AV_CARGA | \
                    SIM_AV_FLUJO | SIM_AV_FLOTA | SIM_AV_BIESTABLE | SIM_AV_ARRANQUE)

/* De [29], más abajo en host_test.c */
static void escribe_forma(FILE *f, const SimEl *e);
static int nodo_net(const SimRed *s, int n);
static double estado_inicial(const SimRed *s, int k);

static SimRed ors;                            /* la red del caso             */
/* Las redes en el tiempo que entran (y sus ajustes), para [75i] I26 */
static SimRed ti_ora_red[ORA_NTR];
static SimAjustes ti_ora_aj[ORA_NTR];
static int ti_ora_n;
static unsigned ora_sem = 20260926u;          /* semilla propia (LCG)        */

static double ora_u(void) { ora_sem = ora_sem * 1103515245u + 12345u; return (double)((ora_sem >> 8) & 0xFFFFFF) / 16777216.0; }
static int ora_n(int n) { return (int)(ora_u() * (double)n); }
static double ora_log(double a, double b) { return a * pow(b / a, ora_u()); }   /* log-uniforme en [a, b) */
static double ora_sg(void) { return ora_u() < 0.5 ? -1.0 : 1.0; }

/* Un nodo nuevo (las redes no pasan de 30: MAX_NODOS es 70) */
static int ora_nodo(void) { return ors.nn++; }

static int ora_el(int tipo, int a, int b, double val)
{
    SimEl *e = &ors.el[ors.nel];
    memset(e, 0, sizeof *e);
    e->tipo = (unsigned char)tipo; e->a = (short)a; e->b = (short)b; e->val = val;
    e->k = -1; e->c = e->nref = -1; e->ctl = -1; e->t1 = e->t2 = -1.0;
    return ors.nel++;
}

static int ora_r(int a, int b, double lo, double hi) { return ora_el(SE_R, a, b, ora_log(lo, hi)); }

static int ora_e_cc(int a, int b, double V, double r)
{
    int k = ora_el(SE_E, a, b, r);
    ors.el[k].forma = ONDA_CC; ors.el[k].p[0] = V;
    return k;
}

static int ora_j_cc(int a, int b, double I)
{
    int k = ora_el(SE_J, a, b, 0.0);
    ors.el[k].forma = ONDA_CC; ors.el[k].p[0] = I;
    return k;
}

/* Una unión (D4) con los m[] del contrato del §2.6: Is, n, Rs, BV, Ibv, Vcrit */
static int ora_union(int a, int b, double Is, double n, double Rs, double BV, double Ibv)
{
    int k = ora_el(SE_D, a, b, 0.0);
    SimEl *x = &ors.el[k];
    x->modelo = NL_UNION;
    x->m[0] = Is; x->m[1] = n; x->m[2] = Rs; x->m[3] = BV; x->m[4] = Ibv;
    x->m[5] = nl_vcrit(Is, n * NL_VT);
    return k;
}

/* Un diodo (cual 0: exponencial al azar o de catálogo), un zener (1:
 * exponencial al azar o 1N4733A, D5) o un LED (2: un color al azar, D6), con
 * las cuentas del adaptador (§3.A) */
static int ora_diodo(int a, int b, int cual)
{
    if (cual == 0) {
        const NlDiodo *cat;
        if (ora_u() < 0.6)
            return ora_union(a, b, ora_log(1e-15, 1e-8), 1.0 + ora_u(), ora_u() < 0.4 ? 0.0 : ora_log(0.1, 20.0), 0.0, 0.0);
        cat = ora_u() < 0.5 ? &NL_1N4148 : &NL_1N4007;
        return ora_union(a, b, cat->Is, cat->n, cat->Rs, 0.0, 0.0);
    }
    if (cual == 1) {
        double vz = 5.1, iz = 49e-3, rz = 7.0, rs;
        if (ora_u() < 0.6) { vz = 2.4 + 10.0 * ora_u(); iz = ora_log(1e-3, 50e-3); rz = ora_log(1.0, 50.0); }
        rs = rz - NL_VT / iz > 0.0 ? rz - NL_VT / iz : 0.0;
        return ora_union(a, b, 1e-14, 1.0, rs, vz - rs * iz, iz);
    }
    {
        const NlColor *co = &NL_COLOR[ora_n(LED_N)];
        double ij = NL_IF - NL_GMIN * co->Vf, vd = co->Vf - co->Rs * ij;
        int k = ora_union(a, b, ij / (exp(vd / (co->n * NL_VT)) - 1.0), co->n, co->Rs, 0.0, 0.0);
        ors.el[k].p[0] = NL_IM; ors.el[k].p[1] = NL_IF;
        return k;
    }
}

/* El diodo entre a y b, al derecho o al revés; con_r: con una R en serie (un
 * nodo nuevo entre los dos) */
static int ora_diodo_en(int a, int b, int cual, int con_r)
{
    int t;
    if (ora_u() < 0.5) { t = a; a = b; b = t; }
    if (con_r) {
        int m = ora_nodo();
        ora_r(a, m, 100.0, 1e4);
        a = m;
    }
    return ora_diodo(a, b, cual);
}

/* Un AO: + en mas, - en menos, la salida en sal, medida desde la tierra */
static int ora_ao(int mas, int menos, int sal, int real)
{
    static const double RAIL[4] = { 5.0, 10.0, 12.0, 15.0 };
    int k = ora_el(SE_AO, mas, menos, 0.0);
    SimEl *x = &ors.el[k];
    double vp = RAIL[ora_n(4)], u = ora_u();
    x->c = (short)sal; x->nref = 0;
    x->modelo = real ? NL_AO_REAL : NL_AO_IDEAL;
    x->m[1] = vp; x->m[2] = u < 0.6 ? -vp : u < 0.8 ? 0.0 : -5.0;
    if (real) {
        x->m[0] = ora_log(1e3, 1e6);
        x->m[3] = ora_u() < 0.5 ? 0.0 : ora_log(1e5, 1e7);
        x->m[4] = ora_u() < 0.5 ? 0.0 : ora_log(10.0, 200.0);
    }
    return k;
}

/* Un BJT (Ebers-Moll de transporte con Early, D9): el colector en nc, la base
 * en nb y el emisor en ne; NPN si pol > 0 */
static int ora_bjt(int nc, int nb, int ne, int pol)
{
    int k = ora_el(SE_BJT, nc, ne, 0.0);
    SimEl *x = &ors.el[k];
    x->c = (short)nb;
    x->modelo = NL_BJT;
    x->pol = (signed char)(pol > 0 ? 1 : -1);
    x->m[0] = ora_log(1e-16, 1e-13); x->m[1] = ora_log(50.0, 300.0); x->m[2] = ora_log(0.5, 5.0);
    x->m[3] = ora_u() < 0.5 ? 0.0 : ora_log(20.0, 200.0);
    x->m[5] = nl_vcrit(x->m[0], NL_VT);
    return k;
}

/* Un MOSFET (Shichman-Hodges, D10): el drenador en nd, la puerta en ng y el
 * surtidor en ns; canal N si pol > 0. Solo con ORA_CON_MOS. */
static int ora_mos(int nd, int ng, int ns, int pol)
{
    int k = ora_el(SE_MOS, nd, ns, 0.0);
    SimEl *x = &ors.el[k];
    x->c = (short)ng;
    x->modelo = NL_MOS;
    x->pol = (signed char)(pol > 0 ? 1 : -1);
    x->m[0] = 0.5 + 2.5 * ora_u(); x->m[1] = ora_log(1e-4, 1e-2);
    x->m[2] = ora_u() < 0.5 ? 0.0 : ora_log(20.0, 200.0);
    return k;
}

/* Un elemento de dos bornes de los primeros `hasta` que puede mandar a una
 * controlada (D8): por su V, cualquiera; por su I, ni diodos ni J
 * controladas. -1 si no hay. */
static int ora_control(int ctl_i, int hasta)
{
    int lista[MAX_EL], n = 0, k;
    for (k = 0; k < hasta && k < ors.nel; k++) {
        int t = ors.el[k].tipo;
        if (t == SE_AO || t == SE_BJT || t == SE_MOS) continue;
        if (ctl_i && (t == SE_D || t == SE_JC || t == SE_Z)) continue;
        lista[n++] = k;
    }
    return n ? lista[ora_n(n)] : -1;
}

/* Una E (tipo SE_EC, + en b) o J (SE_JC, de a hacia b) controlada por la V o
 * la I de uno de los primeros `hasta` elementos; la ganancia, con signo al
 * azar: μ de 0,1 a 10, r de 10 Ω a 10 kΩ, g de gmin a gmax, β de 0,1 a 10 */
static int ora_dep(int tipo, int a, int b, int ctl_i, int hasta, double gmin, double gmax)
{
    int c = ora_control(ctl_i, hasta), k;
    double g;
    if (c < 0) return -1;
    if (tipo == SE_EC) g = ctl_i ? ora_log(10.0, 1e4) : ora_log(0.1, 10.0);
    else g = ctl_i ? ora_log(0.1, 10.0) : ora_log(gmin, gmax);
    k = ora_el(tipo, a, b, ora_sg() * g);
    ors.el[k].ctl = (signed char)c;
    ors.el[k].ctl_i = (unsigned char)ctl_i;
    return k;
}

/* ---- Las redes de CC ---- */
/* Un AO sobre los nodos 0..nn-1: realimentación negativa (7 de 10: - en m, R1
 * de q a m, Rf de m a la salida), positiva (2 de 10: + en m) o ninguna (un
 * comparador entre p y q); la salida, un nodo nuevo con su carga */
static void ora_etapa_ao_cc(int nn)
{
    int real = ora_u() < 0.5, p = ora_n(nn), q = ora_n(nn), o, m, tipo = ora_n(10);
    if (tipo == 9) {
        if (q == p) q = (p + 1) % nn;
        o = ora_nodo();
        ora_ao(p, q, o, real);
    } else {
        m = ora_nodo();
        o = ora_nodo();
        if (tipo < 7) ora_ao(p, m, o, real);
        else ora_ao(m, p, o, real);
        ora_r(q, m, 1e3, 1e5);
        ora_r(m, o, 1e3, 1e5);
    }
    ora_r(o, ora_n(nn), 1e3, 1e5);
}

/* Un BJT polarizado al azar: RB de un nodo a la base, RC de otro al colector
 * y el emisor en un nodo o por RE */
static void ora_etapa_bjt_cc(int nn)
{
    int b = ora_nodo(), c = ora_nodo(), e;
    ora_r(ora_n(nn), b, 1e3, 1e6);
    ora_r(ora_n(nn), c, 100.0, 1e4);
    if (ora_u() < 0.5) e = ora_n(nn);
    else {
        e = ora_nodo();
        ora_r(e, ora_n(nn), 10.0, 1e3);
    }
    ora_bjt(c, b, e, ora_u() < 0.5 ? 1 : -1);
}

/* Un MOSFET polarizado al azar, como el BJT (solo con ORA_CON_MOS) */
static void ora_etapa_mos_cc(int nn)
{
    int g = ora_nodo(), d = ora_nodo(), s;
    ora_r(ora_n(nn), g, 1e3, 1e6);
    ora_r(ora_n(nn), d, 100.0, 1e4);
    if (ora_u() < 0.5) s = ora_n(nn);
    else {
        s = ora_nodo();
        ora_r(s, ora_n(nn), 10.0, 1e3);
    }
    ora_mos(d, g, s, ora_u() < 0.5 ? 1 : -1);
}

static void ora_red_cc(void)
{
    int nn = 3 + ora_n(5), nex = 2 + ora_n(7), i;
    memset(&ors, 0, sizeof ors);
    ors.nn = nn; ors.ref[0] = 1;
    ora_e_cc(0, 1, ora_sg() * (1.0 + ora_n(20)), ora_n(3) == 0 ? ora_log(1.0, 100.0) : 0.0);
    for (i = 2; i < nn; i++) ora_r(ora_n(i), i, 100.0, 1e5);          /* un árbol: nada flota */
    for (i = 0; i < nex && ors.nel < MAX_EL - 6 && ors.nn < 26; i++) {
        int a = ora_n(nn), b = ora_n(nn), t = ora_n(ORA_CON_MOS ? 15 : 14), k;
        if (b == a) b = (a + 1 + ora_n(nn - 1)) % nn;
        switch (t) {
        case 0: ora_r(a, b, 100.0, 1e5); break;
        case 1: ora_e_cc(a, b, ora_sg() * (1.0 + ora_n(20)), ora_u() < 0.5 ? ora_log(1.0, 100.0) : 0.0); break;
        case 2: ora_j_cc(a, b, ora_sg() * ora_log(1e-4, 2e-2)); break;
        case 3: case 4: ora_diodo_en(a, b, 0, ora_u() < 0.5); break;
        case 5: ora_diodo_en(a, b, 1, ora_u() < 0.5); break;
        case 6: ora_diodo_en(a, b, 2, ora_u() < 0.5); break;
        case 7:
            k = ora_el(ora_u() < 0.5 ? SE_AMI : SE_VMI, a, b, 0.0);
            ors.el[k].instr = ors.el[k].tipo == SE_AMI ? 'A' : 'V';
            break;
        case 8: {                                /* E controlada con su carga */
            int o = ora_nodo();
            ora_dep(SE_EC, a, o, ora_u() < 0.5, ors.nel, 0.0, 0.0);
            ora_r(o, b, 100.0, 1e4);
            break;
        }
        case 9: ora_dep(SE_JC, a, b, ora_u() < 0.5, ors.nel, 1e-5, 1e-2); break;
        case 10: case 11: ora_etapa_ao_cc(nn); break;
        case 12: case 13: ora_etapa_bjt_cc(nn); break;
        default: ora_etapa_mos_cc(nn); break;
        }
    }
}

/* ---- Las redes en el tiempo ---- */
/* Una forma al azar (la escala de tiempo tau, la de amplitud esc) */
static void ora_forma(SimEl *e, double tau, double esc)
{
    double f = 1.0 / (tau * (0.5 + 0.1 * (double)ora_n(40)));
    int fo = ora_n(5);
    e->forma = (unsigned char)fo;
    memset(e->p, 0, sizeof e->p);
    switch (fo) {
    case ONDA_SENO:
        e->p[0] = esc * (1.0 + ora_n(9)); e->p[1] = f; e->p[2] = 15.0 * (double)ora_n(24); e->p[3] = esc * (double)(ora_n(5) - 2);
        break;
    case ONDA_CUADRADA:
    case ONDA_TRIANGULAR:
        e->p[0] = esc * (1.0 + ora_n(9)); e->p[1] = f; e->p[2] = 10.0 * (double)(1 + ora_n(9)); e->p[3] = esc * (double)(ora_n(5) - 2);
        break;
    case ONDA_PULSO:
        e->p[0] = esc * (double)ora_n(3); e->p[1] = esc * (1.0 + ora_n(9));
        e->p[2] = tau * 0.1 * (double)ora_n(10);
        e->p[3] = ora_n(3) ? tau * 0.1 * (double)(1 + ora_n(20)) : 0.0;
        e->p[4] = (e->p[3] > 0.0 && ora_n(2)) ? e->p[3] * (1.5 + 0.5 * (double)ora_n(4)) : 0.0;
        break;
    default:
        e->p[0] = ora_sg() * esc * (1.0 + ora_n(20));
        break;
    }
}

/* Un C: tau/R típico por un factor; con V0 una de cada cuatro veces */
static int ora_c(int a, int b, double tau, int con_v0)
{
    int k = ora_el(SE_C, a, b, tau / (100.0 * (1.0 + ora_n(50))) * (0.5 + ora_n(3)));
    if (con_v0 && ora_n(4) == 0) { ors.el[k].ci = 1; ors.el[k].x0 = (double)ora_n(11) - 5.0; }
    return k;
}

/* AO que lee el núcleo sin realimentarlo: inversor (+ a tierra, R1 de un nodo
 * del núcleo a -) o no inversor (+ en el núcleo, R1 de - a tierra), Rf con C
 * en paralelo a veces, y la carga (R, y a veces un RC) */
static void ora_etapa_ao_tr(int ncn, double tau)
{
    int real = ora_u() < 0.5, q = 1 + ora_n(ncn - 1), m = ora_nodo(), o = ora_nodo(), kf;
    if (ora_u() < 0.5) {
        ora_ao(0, m, o, real);
        ora_r(q, m, 1e3, 1e5);
    } else {
        ora_ao(q, m, o, real);
        ora_r(m, 0, 1e3, 1e5);
    }
    kf = ora_r(m, o, 1e3, 1e5);
    if (ora_u() < 0.4) ora_el(SE_C, m, o, tau / ors.el[kf].val * (0.2 + ora_u()));
    ora_r(o, 0, 1e3, 1e5);
    if (ora_u() < 0.5) {
        int w = ora_nodo(), kr = ora_r(o, w, 1e3, 1e4);
        ora_el(SE_C, w, 0, tau / ors.el[kr].val * (0.2 + ora_u()));
    }
}

/* Emisor común: VCC (NPN positiva, PNP negativa), divisor R1-R2 en la base,
 * la señal por un C de acoplo (con su R de fuente) o por RB desde el núcleo,
 * RC, y el emisor a tierra o por RE (con C de desacoplo a veces). Sin la R de
 * fuente, un flanco del núcleo (o un S que cierra) pasaría entero por el C a
 * la base: la unión B-E quedaría forzada a voltios en directa entre dos C, sin
 * nada que limite la corriente más que la exponencial (en el tiempo exacto, un
 * impulso: ni el oráculo ni el motor lo resuelven con sentido) */
static void ora_etapa_bjt_tr(int ncn, double tau)
{
    int vcc = ora_nodo(), b = ora_nodo(), c = ora_nodo(), q = 1 + ora_n(ncn - 1), pol = ora_u() < 0.5 ? 1 : -1, e, k1;
    ora_e_cc(0, vcc, (double)pol * (5.0 + ora_n(11)), 0.0);
    k1 = ora_r(vcc, b, 1e4, 1e5);
    ora_el(SE_R, b, 0, ors.el[k1].val * (0.1 + 0.3 * ora_u()));
    if (ora_u() < 0.5) {
        int m = ora_nodo();
        ora_r(q, m, 100.0, 1e4);
        ora_el(SE_C, m, b, tau / 1e3 * (1.0 + ora_n(10)));
    } else ora_r(q, b, 1e3, 1e5);
    ora_r(vcc, c, 1e3, 1e4);
    if (ora_u() < 0.5) e = 0;
    else {
        int kr;
        e = ora_nodo();
        kr = ora_r(e, 0, 100.0, 1e3);
        if (ora_u() < 0.5) ora_el(SE_C, e, 0, tau / ors.el[kr].val * (0.5 + ora_u()));
    }
    ora_bjt(c, b, e, pol);
    if (ora_u() < 0.3) ora_c(c, 0, tau, 0);
}

/* MOSFET en fuente común, como el emisor común (solo con ORA_CON_MOS) */
static void ora_etapa_mos_tr(int ncn, double tau)
{
    int vdd = ora_nodo(), g = ora_nodo(), d = ora_nodo(), q = 1 + ora_n(ncn - 1), pol = ora_u() < 0.5 ? 1 : -1, k1;
    ora_e_cc(0, vdd, (double)pol * (5.0 + ora_n(11)), 0.0);
    k1 = ora_r(vdd, g, 1e4, 1e5);
    ora_el(SE_R, g, 0, ors.el[k1].val * (0.2 + 0.6 * ora_u()));
    if (ora_u() < 0.5) ora_el(SE_C, q, g, tau / 1e3 * (1.0 + ora_n(10)));
    else ora_r(q, g, 1e3, 1e5);
    ora_r(vdd, d, 1e3, 1e4);
    ora_mos(d, g, 0, pol);
}

/* Una E o J controlada por un elemento del núcleo (los primeros ncel), que
 * alimenta su carga */
static void ora_etapa_dep_tr(int ncel, double tau)
{
    int o = ora_nodo(), ctl_i = ora_u() < 0.5;
    if (ora_u() < 0.5) ora_dep(SE_EC, 0, o, ctl_i, ncel, 0.0, 0.0);
    else ora_dep(SE_JC, 0, o, ctl_i, ncel, 1e-5, 1e-3);
    ora_r(o, 0, 1e3, 1e5);
    if (ora_u() < 0.5) {
        int w = ora_nodo(), kr = ora_r(o, w, 1e3, 1e4);
        ora_el(SE_C, w, 0, tau / ors.el[kr].val * (0.2 + ora_u()));
    }
}

/* Los nodos del núcleo unidos por ramas de tensión (la E ideal y los C): un C
 * entre dos nodos ya unidos cerraría un lazo de C (con la E o con otros C), y
 * el algebraico del oráculo, con los C como fuentes de tensión, sería
 * singular (el motor lo resuelve igual: sus C son compañeros) */
static signed char ora_uf[MAX_NODOS];

static int ora_uf_raiz(int n)
{
    while (ora_uf[n] != n) n = ora_uf[n] = ora_uf[ora_uf[n]];
    return n;
}

/* La red del tiempo; en *c1 el nodo del canal 1 y en *c2 el elemento cuya
 * corriente es el canal 2 */
static void ora_red_tr(double tau, int *c1, int *c2)
{
    int nn = 3 + ora_n(4), nex = 1 + ora_n(5), i, ideal = ora_u() < 0.6, ncn, ncel, nst, fuente, lista[MAX_EL], n;
    memset(&ors, 0, sizeof ors);
    ors.nn = nn; ors.ref[0] = 1;
    for (i = 0; i < MAX_NODOS; i++) ora_uf[i] = (signed char)i;
    fuente = ora_el(SE_E, 0, 1, ideal ? 0.0 : ora_log(1.0, 100.0));
    ora_forma(&ors.el[fuente], tau, 1.0);
    if (ideal) ora_uf[1] = 0;
    for (i = 2; i < nn; i++) ora_r(ora_n(i), i, 100.0, 1e4);
    for (i = 0; i < nex; i++) {
        int a = ora_n(nn), b = ora_n(nn), t = ora_n(9), k;
        if (b == a) b = (a + 1 + ora_n(nn - 1)) % nn;
        switch (t) {
        case 0: ora_r(a, b, 100.0, 1e4); break;
        case 1: case 2:                          /* un C (con una R en serie si cerraría un lazo) */
            if (ora_uf_raiz(a) == ora_uf_raiz(b)) {
                int m = ora_nodo();
                ora_r(a, m, 100.0, 1e4);
                a = m;
            }
            ora_uf[ora_uf_raiz(a)] = (signed char)ora_uf_raiz(b);
            ora_c(a, b, tau, 1);
            break;
        case 3:
            k = ora_el(SE_L, a, b, tau * 100.0 * (1.0 + ora_n(50)) * (0.5 + ora_n(3)));
            if (ora_n(4) == 0) { ors.el[k].ci = 1; ors.el[k].x0 = 0.01 * ((double)ora_n(11) - 5.0); }
            break;
        case 4: case 5: ora_diodo_en(a, b, ora_n(3), 1); break;
        case 6: {                                /* un interruptor con su R */
            int s = ora_nodo();
            k = ora_el(SE_S, a, s, 0.0);
            ors.el[k].cerrado0 = (unsigned char)ora_n(2);
            ors.el[k].t1 = tau * (0.5 + 0.1 * (double)ora_n(40));
            if (ora_n(2)) ors.el[k].t2 = ors.el[k].t1 + tau * (0.5 + 0.1 * (double)ora_n(30));
            ora_r(s, b, 100.0, 1e4);
            break;
        }
        case 7:                                  /* una J con forma, de mA (los tiempos no se escalan) */
            k = ora_el(SE_J, a, b, 0.0);
            ora_forma(&ors.el[k], tau, 1e-3);
            break;
        default: ora_diodo_en(a, b, 0, 1); break;
        }
    }
    ncn = ors.nn;
    ncel = ors.nel;
    for (i = 0, nst = ora_n(3); i < nst && ors.nel <= MAX_EL - 10; i++) {  /* una etapa pone 10 como mucho */
        int s = ora_n(ORA_CON_MOS ? 4 : 3);
        if (s == 0) ora_etapa_ao_tr(ncn, tau);
        else if (s == 1) ora_etapa_bjt_tr(ncn, tau);
        else if (s == 2) ora_etapa_dep_tr(ncel, tau);
        else ora_etapa_mos_tr(ncn, tau);
    }
    *c1 = 1 + ora_n(ors.nn - 1);
    for (i = n = 0; i < ors.nel; i++) {
        int t = ors.el[i].tipo;
        if (t == SE_R || t == SE_C || t == SE_L || t == SE_D || t == SE_EC || t == SE_JC || t == SE_AO ||
            t == SE_BJT || t == SE_MOS || i == fuente)
            lista[n++] = i;
    }
    *c2 = lista[ora_n(n)];
}

/* ---- La exportación (CESNET 3) ---- */
static const char *ora_nom(const SimRed *s, int k, char *buf)
{
    static const char *const LETRA[SE_N] = { "R", "V", "I", "C", "L", "S", "A", "VM", "Z", "D", "E", "J", "U", "Q", "M" };
    sprintf(buf, "%s%d", LETRA[s->el[k].tipo], k);
    return buf;
}

/* La red; tran = 0: CC (sin C, L ni S), 1: el tiempo (tfin, h, estado inicial
 * de C y L de la última emulación y los dos canales). nota: una línea de
 * comentario con lo que hizo el motor. */
static void ora_escribe_net(const char *ruta, const SimRed *s, int tran, double tfin, const SimAjustes *aj, const char *nota)
{
    FILE *f = fopen(ruta, "w");
    char nom[16], nc[16];
    int k;
    if (!f) return;
    /* la cabecera nombra el programa; la nota va en «# motor: », la línea que
     * lee rechazada() de oraculo_nl.py (no se parece a la cabecera) */
    fprintf(f, "# Hakaru CES: red no lineal al azar para el oraculo no lineal (texto ASCII)\n");
    fprintf(f, "# motor: %s\n", nota);
    fprintf(f, "cesnet 3\n");
    if (tran) fprintf(f, "tran %.17g %.17g\n", tfin, tfin / ORA_MUESTRAS);
    else fprintf(f, "dc\n");
    for (k = 0; k < s->nel; k++) {
        const SimEl *e = &s->el[k];
        int a = nodo_net(s, e->a), b = nodo_net(s, e->b), c = e->c >= 0 ? nodo_net(s, e->c) : -1;
        const double *m = e->m;
        ora_nom(s, k, nom);
        switch (e->tipo) {
        case SE_R: fprintf(f, "R %s %d %d %.17g\n", nom, a, b, e->val); break;
        case SE_E: fprintf(f, "V %s %d %d %.17g ", nom, a, b, e->val); escribe_forma(f, e); fprintf(f, "\n"); break;
        case SE_J: fprintf(f, "I %s %d %d ", nom, a, b); escribe_forma(f, e); fprintf(f, "\n"); break;
        case SE_C: fprintf(f, "C %s %d %d %.17g %.17g\n", nom, a, b, e->val, estado_inicial(s, k)); break;
        case SE_L: fprintf(f, "L %s %d %d %.17g %.17g\n", nom, a, b, e->val, estado_inicial(s, k)); break;
        case SE_S:                              /* aquí t1 < 0 es «nunca»; en CESNET, 0 */
            fprintf(f, "S %s %d %d %d %.17g %.17g\n", nom, a, b, e->cerrado0 ? 1 : 0,
                    e->t1 > 0.0 ? e->t1 : 0.0, e->t2 > 0.0 ? e->t2 : 0.0);
            break;
        case SE_AMI: fprintf(f, "AM %s %d %d 0\n", nom, a, b); break;
        case SE_VMI: fprintf(f, "VM %s %d %d 0\n", nom, a, b); break;
        case SE_D:
            if (e->modelo == NL_TRAMOS) fprintf(f, "DT %s %d %d %.17g %.17g %.17g %.17g\n", nom, a, b, m[0], m[1], m[2], m[3]);
            else if (m[3] > 0.0) fprintf(f, "DZ %s %d %d %.17g %.17g %.17g %.17g %.17g\n", nom, a, b, m[0], m[1], m[2], m[3], m[4]);
            else fprintf(f, "D %s %d %d %.17g %.17g %.17g\n", nom, a, b, m[0], m[1], m[2]);
            break;
        case SE_EC: case SE_JC:
            fprintf(f, "%s%s %s %d %d %s %.17g\n", e->tipo == SE_EC ? "E" : "J", e->ctl_i ? "I" : "V", nom, a, b,
                    ora_nom(s, e->ctl, nc), e->val);
            break;
        case SE_AO:
            fprintf(f, "AO %s %d %d %d %d %.17g %.17g %.17g %.17g %.17g\n", nom, a, b, c, nodo_net(s, e->nref),
                    e->modelo == NL_AO_REAL ? m[0] : 0.0, m[1], m[2], e->modelo == NL_AO_REAL ? m[3] : 0.0,
                    e->modelo == NL_AO_REAL ? m[4] : 0.0);
            break;
        case SE_BJT:
            fprintf(f, "Q %s %d %d %d %s %.17g %.17g %.17g %.17g\n", nom, a, c, b, e->pol > 0 ? "NPN" : "PNP", m[0], m[1], m[2], m[3]);
            break;
        case SE_MOS:
            fprintf(f, "M %s %d %d %d %s %.17g %.17g %.17g\n", nom, a, c, b, e->pol > 0 ? "N" : "P", m[0], m[1], m[2]);
            break;
        default: break;
        }
    }
    if (tran)
        for (k = 0; k < 2; k++) {
            const SimCanal *ch = &aj->canal[k];
            if (ch->tipo == CANAL_NODO) fprintf(f, "# canal %d V %d\n", k + 1, nodo_net(s, ch->nodo));
            else fprintf(f, "# canal %d I %s\n", k + 1, ora_nom(s, ch->elem, nom));
        }
    fclose(f);
}

/* Lo que da el motor en CC: V <nodo> <phi>, I <nom> <I> e IB <nom> <IB> */
static void ora_escribe_cc(const char *ruta, const SimRed *s, const Valores *v)
{
    FILE *f = fopen(ruta, "w");
    char nom[16];
    int k, n;
    if (!f) return;
    for (n = 0; n < s->nn; n++) if (!s->ref[n]) fprintf(f, "V %d %.17g\n", nodo_net(s, n), v->phi[n]);
    for (k = 0; k < s->nel; k++) {
        fprintf(f, "I %s %.17g\n", ora_nom(s, k, nom), v->I[k]);
        if (s->el[k].tipo == SE_BJT) fprintf(f, "IB %s %.17g\n", nom, v->I2[k]);
    }
    fclose(f);
}

/* El punto del registro más cercano a t (a igual distancia, el de después);
 * en un evento (dos puntos con el mismo t), el de después, como [29] */
static int ora_punto(const SimReg *g, double t)
{
    int lo = 0, hi = g->n - 1;
    while (lo < hi) { int m = (lo + hi) / 2; if (g->t[m] < t) lo = m + 1; else hi = m; }
    if (lo > 0 && t - g->t[lo - 1] < g->t[lo] - t) lo--;
    while (lo + 1 < g->n && g->t[lo + 1] == g->t[lo]) lo++;
    return lo;
}

/* Lo que da el motor en el tiempo: t, canal 1, canal 2 en el punto que
 * calculó más cercano a k·tfin/400, con su t. No la interpolación lineal
 * entre dos pasos (sim_canal_en, la de la pantalla): con el paso agrandado
 * por el tope de 2 s (SIM_AV_PASO_LIM) los pasos no caen en k·tfin/400, y la
 * recta entre dos pasos que abrazan el codo de un BJT o un MOSFET que se
 * enciende difiere de la curva hasta un 0,7 % de la escala aunque los dos
 * pasos estén bien a 3e-4 */
static void ora_escribe_tr(const char *ruta, double tfin)
{
    const SimReg *g = sim_reg();
    FILE *f = fopen(ruta, "w");
    int i;
    if (!f) return;
    fprintf(f, "t,y1,y2\n");
    for (i = 0; i <= ORA_MUESTRAS; i++) {
        int j = ora_punto(g, tfin * i / ORA_MUESTRAS);
        fprintf(f, "%.17g,%.17g,%.17g\n", g->t[j], g->y[0][j], g->y[1][j]);
    }
    fprintf(f, "# h %.6g, %ld pasos, %ld subpasos, avisos 0x%x\n", sim_res()->h, sim_res()->pasos, sim_res()->nw.sub,
            sim_res()->d.avisos);
    fclose(f);
}

static void test_e4_ora(void)
{
    static Valores v;
    static const char *const TIPO[SE_N] = { "R", "E", "J", "C", "L", "S", "AMI", "VMI", "Z", "D", "EC", "JC", "AO", "BJT", "MOS" };
    char dir[300], ruta[340], nota[400];
    int intentos, n_cc = 0, n_tr = 0, n_malos = 0, n_tope = 0, rech[SIM_AO_REF + 1], cuenta[SE_N], k, i, homot = 0;
    long iter = 0;
    double res_max = 0.0;

    printf("[67] Redes no lineales al azar para el oráculo (out/oraculo_nl)\n");
    sprintf(dir, "%s/oraculo_nl", dir_out);
    mkdir(dir, 0755);
    memset(rech, 0, sizeof rech);
    memset(cuenta, 0, sizeof cuenta);

    /* En CC: sim_op_cc_red (la vista de [P]) */
    for (intentos = 0; n_cc < ORA_NCC && intentos < 20 * ORA_NCC; intentos++) {
        SimDiag d;
        SimNewton nw;
        ora_red_cc();
        if (sim_op_cc_red(&ors, &v, &d, &nw) != SIM_OK) {
            if (d.estado >= 0 && d.estado <= SIM_AO_REF) rech[d.estado]++;
            if (d.estado == SIM_NO_CONVERGE) {  /* aparte: el oráculo dice si tiene solución */
                char m[200];
                sim_mensaje(&ors, &d, m, sizeof m);
                sprintf(nota, "NO_CONVERGE (%s)", m);
                for (i = 0; nota[i]; i++) if ((unsigned char)nota[i] >= 0x80 || nota[i] == '\n') nota[i] = '?';
                printf("    rechazada_cc_%03d: %s\n", intentos, nota);
                sprintf(ruta, "%s/rechazada_cc_%03d.net", dir, intentos);
                ora_escribe_net(ruta, &ors, 0, 0.0, NULL, nota);
            }
            continue;
        }
        for (k = 0; k < ors.nel; k++) cuenta[ors.el[k].tipo]++;
        iter += nw.iter;
        homot += nw.homotopia > 0;
        if (nw.residuo_max > res_max) res_max = nw.residuo_max;
        sprintf(nota, "%d elementos, %d nodos; Newton %ld iteraciones, homotopia %d, residuo %.3g",
                ors.nel, ors.nn, nw.iter, (int)nw.homotopia, nw.residuo_max);
        sprintf(ruta, "%s/caso_%03d.net", dir, n_cc);
        ora_escribe_net(ruta, &ors, 0, 0.0, NULL, nota);
        sprintf(ruta, "%s/caso_%03d.ces", dir, n_cc);
        ora_escribe_cc(ruta, &ors, &v);
        n_cc++;
    }
    printf("    CC: %d redes en %d intentos; Newton %ld iteraciones, %d con homotopía, residuo máximo %.2g\n",
           n_cc, intentos, iter, homot, res_max);
    printf("    rechazadas:");
    for (i = 0; i <= SIM_AO_REF; i++) if (rech[i]) printf(" %d (estado %d)", rech[i], i);
    printf("\n    elementos:");
    for (i = 0; i < SE_N; i++) if (cuenta[i]) printf(" %s %d", TIPO[i], cuenta[i]);
    printf("\n");

    /* En el tiempo: sim_emular con el paso automático, sin que el control del
     * gasto acorte la ventana (rev9: el oráculo mira la ventana entera, y el
     * corte se prueba en [75e] y [75i]; acortada, la muestra de .ces pasado
     * t_fin repetía la última) */
    memset(rech, 0, sizeof rech);
    memset(cuenta, 0, sizeof cuenta);
    sim_gasto_sin_corte(1);
    for (intentos = 0; n_tr < ORA_NTR && intentos < 20 * ORA_NTR; intentos++) {
        double tau = 1e-3 * (0.2 + 0.1 * (double)ora_n(30));
        SimAjustes aj;
        const SimRes *r;
        int c1, c2;
        memset(&aj, 0, sizeof aj);
        ora_red_tr(tau, &c1, &c2);
        canal_de(&aj, &ors, 0, CANAL_NODO, c1);
        canal_de(&aj, &ors, 1, CANAL_I, c2);
        aj.t_fin = 10.0 * tau;
        if (sim_emular(&ors, &aj) != SIM_OK) {
            int e = sim_res()->d.estado;
            if (e >= 0 && e <= SIM_AO_REF) rech[e]++;
            if (e == SIM_NO_CONVERGE) {
                /* Aparte, para mirarla con el oráculo: la red, y si con 50
                 * iteraciones por paso (sim_nl_topes) sí se emula, lo que da
                 * (rechazada_NNN.ces). No cuenta entre las 60. */
                char m[200];
                int e2;
                sim_mensaje(&ors, &sim_res()->d, m, sizeof m);
                sprintf(nota, "NO_CONVERGE en t = %.6g (%s)", sim_res()->d.t, m);
                for (i = 0; nota[i]; i++) if ((unsigned char)nota[i] >= 0x80 || nota[i] == '\n') nota[i] = '?';
                sim_nl_topes(0, 50);
                e2 = sim_emular(&ors, &aj);
                sim_nl_topes(0, 0);
                printf("    rechazada_%03d: %s; con 50 iteraciones por paso, %s\n", intentos, nota,
                       e2 == SIM_OK ? "se emula" : "tampoco");
                sprintf(ruta, "%s/rechazada_%03d.net", dir, intentos);
                ora_escribe_net(ruta, &ors, 1, aj.t_fin, &aj, nota);
                if (e2 == SIM_OK) {
                    sprintf(ruta, "%s/rechazada_%03d.ces", dir, intentos);
                    ora_escribe_tr(ruta, aj.t_fin);
                }
            }
            continue;
        }
        r = sim_res();
        if (r->fin != FIN_COMPLETO) { n_tope++; continue; }
        if (r->d.avisos & ORA_AV_MAL) { n_malos++; continue; }
        for (k = 0; k < ors.nel; k++) cuenta[ors.el[k].tipo]++;
        sprintf(nota, "%d elementos, %d nodos; h %.6g, %ld pasos, %ld iteraciones, %ld subpasos, %d eventos, avisos 0x%x",
                ors.nel, ors.nn, r->h, r->pasos, r->nw.iter, r->nw.sub, r->nw.eventos, r->d.avisos);
        sprintf(ruta, "%s/caso_%03d.net", dir, ORA_NCC + n_tr);
        ora_escribe_net(ruta, &ors, 1, aj.t_fin, &aj, nota);
        sprintf(ruta, "%s/caso_%03d.ces", dir, ORA_NCC + n_tr);
        ora_escribe_tr(ruta, aj.t_fin);
        if (ti_ora_n < ORA_NTR) { ti_ora_red[ti_ora_n] = ors; ti_ora_aj[ti_ora_n] = aj; ti_ora_n++; }
        /* Con un cuarto del paso (dato): el oráculo mira si el error baja, como
         * el h/2 de [29] (el trapecio es de orden 2: una cuarta parte del paso,
         * un dieciseisavo del error, salvo en los flancos) */
        aj.h = r->h / 4.0;
        aj.max_pasos = 8L * SIM_MAX_PASOS;
        if (sim_emular(&ors, &aj) == SIM_OK && sim_res()->fin == FIN_COMPLETO) {
            sprintf(ruta, "%s/caso_%03d_h4.ces", dir, ORA_NCC + n_tr);
            ora_escribe_tr(ruta, aj.t_fin);
        }
        n_tr++;
    }
    sim_gasto_sin_corte(0);
    printf("    tiempo: %d redes en %d intentos; descartadas %d por avisos de red degenerada y %d por el tope\n",
           n_tr, intentos, n_malos, n_tope);
    printf("    rechazadas:");
    for (i = 0; i <= SIM_AO_REF; i++) if (rech[i]) printf(" %d (estado %d)", rech[i], i);
    printf("\n    elementos:");
    for (i = 0; i < SE_N; i++) if (cuenta[i]) printf(" %s %d", TIPO[i], cuenta[i]);
    printf("\n");

    pruebas++;
    if (n_cc < ORA_NCC) falla("oráculo no lineal", "el motor acepta 200 redes de CC al azar", n_cc, ORA_NCC);
    pruebas++;
    if (n_tr < ORA_NTR) falla("oráculo no lineal", "el motor emula 60 redes al azar en el tiempo", n_tr, ORA_NTR);
}
