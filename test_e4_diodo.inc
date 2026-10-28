/* --------------------------------------------------------------------------
 *  58. Etapa 4: diodos, el punto de operación (pasos 3 y 4)
 *
 *  Se incluye en host_test.c tras test_e4_ctl.inc (usa rc, rr, c4_init,
 *  c4_el, c4_dep, sr, saj, sr_init, sr_el, canal, lineas y los chk). Los
 *  números de las tablas son los del §6.5 de la especificación (mpmath).
 * ----------------------------------------------------------------------- */
#include "nolin.h"

static int d4_d(int tipo, int a, int b, int var)
{
    int k = c4_el((TipoEl)tipo, a, b, 0.0);
    rc.el[k].variante = (unsigned char)var;
    return k;
}

static void d4_par(int k, int q, double v) { rc.el[k].par[q - Q_P1].v = v; rc.el[k].par[q - Q_P1].est = M_DATO; }

/* La unión del modelo por su cuenta (sin nolin.c): ij(vd) y la corriente en
 * bornes para una V dada, con v = vd + Rs·ij(vd) resuelta por bisección */
static double d4_ij(const double *m, double vd)
{
    double x = vd / (m[1] * NL_VT), r;
    r = m[0] * ((x > 700.0 ? exp(700.0) : exp(x)) - 1.0);
    if (m[3] > 0.0) { double y = -(vd + m[3]) / NL_VT; r -= m[4] * (y > 700.0 ? exp(700.0) : exp(y)); }
    return r;
}

static double d4_i_union(const double *m, double V)
{
    double lo = -200.0, hi = 200.0;
    int i;
    for (i = 0; i < 300; i++) {
        double mid = 0.5 * (lo + hi);
        if (mid + m[2] * d4_ij(m, mid) > V) hi = mid; else lo = mid;
    }
    return d4_ij(m, 0.5 * (lo + hi)) + NL_GMIN * V;
}

static double d4_i_tramos(const double *m, double V)
{
    if (V > m[0]) return (V - m[0]) / m[1] + NL_GMIN * m[0];
    if (m[2] > 0.0 && V < -m[2]) return (V + m[2]) / m[3] - NL_GMIN * m[2];
    return NL_GMIN * V;
}

/* kcl_de y ley_de: la ley de nodos y la de cada diodo con los Valores. En
 * la ley de nodos, cada corriente de una R (una resta de potenciales) lleva
 * su ruido de redondeo en la escala, como el residuo del motor */
static void d4_criterios(const char *q, const SimRed *s, const Valores *v)
{
    static double suma[MAX_NODOS], modu[MAX_NODOS];
    double peor_k = 0.0, peor_l = 0.0;
    int k, n;
    for (n = 0; n < s->nn; n++) suma[n] = modu[n] = 0.0;
    for (k = 0; k < s->nel; k++) {
        const SimEl *e = &s->el[k];
        double m = fabs(v->I[k]);
        if (e->tipo == SE_R) m += 2.2e-4 / e->val * (fabs(v->phi[e->a]) + fabs(v->phi[e->b]));
        suma[e->a] += v->I[k]; modu[e->a] += m;
        suma[e->b] -= v->I[k]; modu[e->b] += m;
        if (e->tipo == SE_D) {
            double im = e->modelo == NL_TRAMOS ? d4_i_tramos(e->m, v->V[k]) : d4_i_union(e->m, v->V[k]);
            double t = fabs(v->I[k] - im) / (fabs(im) + 1e-300);
            if (t > peor_l) peor_l = t;
        }
    }
    for (n = 0; n < s->nn; n++) {                  /* la de la referencia sale de las demás */
        double t = fabs(suma[n]) / (modu[n] + 1e-300);
        if (!s->ref[n] && t > peor_k) peor_k = t;
    }
    pruebas += 2;
    if (!(peor_k <= 1e-10)) falla(q, "kcl_de (el arnés) <= 1e-10", peor_k, 0.0);
    if (!(peor_l <= 1e-10)) falla(q, "ley_de (el arnés) <= 1e-10", peor_l, 0.0);
}

/* El OP por las dos rutas (sim_op_red y sim_op_cc_red): I y V del diodo k */
static void d4_op(const char *q, int k, double I, double V, double tol, int itmax)
{
    static SimRed s;
    static Valores v, w;
    SimDiag d;
    SimNewton nw;
    cierra_red();
    chk_cond(q, "la red del emulador", sim_red_de(&rc, &rr, &s, &d) == SIM_OK);
    chk_cond(q, "sim_op_red converge", sim_op_red(&s, &v, &d) == SIM_OK);
    chk_t(q, "I (sim_op_red)", v.I[k], I, fabs(I), tol);
    if (V != 0.0) chk_t(q, "V (sim_op_red)", v.V[k], V, fabs(V), tol);
    memset(&nw, 0, sizeof nw);
    chk_cond(q, "sim_op_cc_red converge", sim_op_cc_red(&s, &w, &d, &nw) == SIM_OK);
    chk_t(q, "I (sim_op_cc_red)", w.I[k], I, fabs(I), tol);
    if (V != 0.0) chk_t(q, "V (sim_op_cc_red)", w.V[k], V, fabs(V), tol);
    pruebas++;
    if (!(nw.residuo_max <= 1e-12)) falla(q, "residuo de Newton <= 1e-12", nw.residuo_max, 0.0);
    if (itmax > 0) chk_cond(q, "iteraciones del OP", nw.iter_op <= itmax);
    d4_criterios(q, &s, &w);
}

/* E en serie con R y n diodos iguales (del ánodo al cátodo, hacia tierra) */
static int d4_serie(double E, double R, int tipo, int var, int n)
{
    int i, k = -1;
    c4_init(3 + n);
    c4_el(EL_FUENTE, 0, 1, E);
    c4_el(EL_RES, 1, 2, R);
    for (i = 0; i < n; i++) {
        int b = i == n - 1 ? 0 : 3 + i, a = i == 0 ? 2 : 2 + i;
        int kk = d4_d(tipo, a, b, var);
        if (k < 0) k = kk;
    }
    return k;
}

/* E = 12 V, Rs = 220 Ω, el zener con el cátodo arriba y RL (0: sin carga) */
static int d4_zener(int var, double RL, double E, int *kz)
{
    c4_init(3);
    c4_el(EL_FUENTE, 0, 1, E);
    c4_el(EL_RES, 1, 2, 220.0);
    *kz = d4_d(EL_ZENER, 0, 2, var);
    if (RL > 0.0) c4_el(EL_RES, 2, 0, RL);
    return *kz;
}

static Valores d4_v;                   /* los del último d4_zener_chk */

static void d4_zener_chk(const char *q, int kz, double Vo, double Iz, double tol)
{
    static SimRed s;
    SimDiag d;
    cierra_red();
    chk_cond(q, "converge", sim_red_de(&rc, &rr, &s, &d) == SIM_OK && sim_op_cc_red(&s, &d4_v, &d, 0) == SIM_OK);
    chk_t(q, "Vo", d4_v.phi[2], Vo, fabs(Vo), tol);
    if (Iz >= 0.0) chk_t(q, "|I(Dz)|", fabs(d4_v.I[kz]), Iz, Iz, tol);
    d4_criterios(q, &s, &d4_v);
}

static void test_e4_diodo(void)
{
    static const double IS_LED[LED_N] = { 1.0685493590266111e-16, 2.237216788021277e-18, 3.2371648073427147e-19,
                                          4.6840503101921716e-20, 1.1745197291847082e-18, 1.1745197291847082e-18 };
    static SimRed s;
    static Valores v;
    static Solucion so;
    SimDiag d;
    SimNewton nw;
    char q[64], m[400];
    int k, kz, i;
    unsigned sem0 = semilla;

    printf("[58] Etapa 4: diodos, el punto de operaci" S_oo "n\n");

    /* ---- Casos cerrados: E, R y el diodo en serie ---- */
    k = d4_serie(5.0, 1e3, EL_DIODO, DIO_EXP, 1);
    d4_op("exp 5 V 1k", k, 4.3071121676219442e-3, 0.69288783237805583, 1e-12, 0);
    k = d4_serie(5.0, 1e3, EL_DIODO, DIO_1N4148, 1);
    d4_op("1N4148 5 V 1k", k, 4.3467715385002356e-3, 0.65322846149976443, 1e-12, 0);
    k = d4_serie(5.0, 1e3, EL_DIODO, DIO_1N4007, 1);
    d4_op("1N4007 5 V 1k", k, 4.375928266602544e-3, 0.62407173339745597, 1e-12, 0);
    k = d4_serie(10.0, 100.0, EL_DIODO, DIO_1N4007, 1);
    d4_op("1N4007 10 V 100", k, 92.303421120134247e-3, 0.76965788798657532, 1e-12, 0);
    k = d4_serie(-5.0, 1e3, EL_DIODO, DIO_EXP, 1);
    d4_op("exp -5 V 1k", k, -5.00999999499e-12, -4.99999999499, 1e-9, 0);
    k = d4_serie(100.0, 1.0, EL_DIODO, DIO_EXP, 1);
    d4_op("exp 100 V 1 ohm", k, 99.047348503037482, 0.95265149696251774, 1e-12, 40);
    k = d4_serie(1.0, 1e6, EL_DIODO, DIO_EXP, 1);
    d4_op("exp 1 V 1M", k, 539.51149881271225e-9, 0.46048850118728775, 1e-12, 0);
    k = d4_serie(0.3, 1e3, EL_DIODO, DIO_EXP, 1);
    d4_op("exp 0,3 V 1k", k, 1.0898251756675721e-9, 0.29999891017482433, 1e-9, 0);
    k = d4_serie(5.0, 1e3, EL_DIODO, DIO_IDEAL, 1);
    d4_op("ideal 5 V 1k", k, 4.999995000005e-3, 4.999995000005e-6, 1e-12, 0);
    k = d4_serie(-5.0, 1e3, EL_DIODO, DIO_IDEAL, 1);
    d4_op("ideal -5 V 1k", k, -4.999999995e-12, -4.999999995, 1e-9, 0);
    k = d4_serie(5.0, 1e3, EL_DIODO, DIO_VG, 1);
    d4_op("V" S_GAMMA " 0,7 5 V 1k", k, 4.2999957000043007e-3, 0.7000042999956993, 1e-12, 0);
    k = d4_serie(5.0, 1e3, EL_DIODO, DIO_VG, 1);
    d4_par(k, QD_RD, 10.0);
    d4_op("V" S_GAMMA " 0,7 rd 10", k, 4.2574257425811881e-3, 0.74257425741881188, 1e-12, 0);
    k = d4_serie(5.0, 1e3, EL_DIODO, DIO_EXP, 2);
    d4_op("2 exp en serie", k, 3.6231693828557282e-3, 0.68841530857213592, 1e-12, 0);
    k = d4_serie(10.0, 10.0, EL_DIODO, DIO_EXP, 10);
    d4_op("10 exp en serie", k, 206.95689299461996e-3, 0.79304310700538004, 1e-12, 0);

    /* ---- OR de dos ideales ---- */
    {
        int kd1, kd2;
        c4_init(4);
        c4_el(EL_FUENTE, 0, 1, 5.0);
        kd1 = d4_d(EL_DIODO, 1, 3, DIO_IDEAL);
        c4_el(EL_FUENTE, 0, 2, 3.0);
        kd2 = d4_d(EL_DIODO, 2, 3, DIO_IDEAL);
        c4_el(EL_RES, 3, 0, 1e3);
        cierra_red();
        chk_cond("OR", "converge", sim_red_de(&rc, &rr, &s, &d) == SIM_OK && sim_op_cc_red(&s, &v, &d, 0) == SIM_OK);
        chk_t("OR", "Vn", v.phi[3], 4.999995000004998, 5.0, 1e-9);
        chk_t("OR", "I(D1)", v.I[kd1], 4.999995002004993e-3, 5e-3, 1e-9);
        chk_t("OR", "I(D2)", v.I[kd2], -1.999995000004998e-12, 2e-12, 1e-9);
        chk_t("OR", "V(D2)", v.V[kd2], -1.999995000004998, 2.0, 1e-9);
        chk_cond("OR", "regiones: D1 conduce, D2 en corte", v.reg[kd1] == REG_CONDUCE && v.reg[kd2] == REG_CORTE);
        d4_criterios("OR", &s, &v);
    }

    /* ---- Zener ---- */
    d4_zener(ZEN_LIBRO, 1e3, 12.0, &kz);
    d4_zener_chk("zener de libro 1k", kz, 5.1000212635184427, 26.263518447761363e-3, 1e-12);
    chk_cond("zener de libro 1k", "regi" S_oo "n: ruptura", d4_v.reg[kz] == REG_RUPTURA);
    d4_zener(ZEN_LIBRO, 100.0, 12.0, &kz);
    d4_zener_chk("zener de libro 100", kz, 3.7499999997421875, 3.7499999997421875e-12, 1e-9);
    chk_cond("zener de libro 100", "regi" S_oo "n: corte", d4_v.reg[kz] == REG_CORTE);
    d4_zener(ZEN_LIBRO, 0.0, 12.0, &kz);
    d4_zener_chk("zener de libro sin carga", kz, 5.1000263635165244, 31.363516529470344e-3, 1e-12);
    d4_zener(ZEN_LIBRO, 1e3, 12.0, &kz);
    d4_par(kz, QZ_RZ, 7.0); d4_par(kz, QZ_IZ, 49e-3);
    d4_zener_chk("zener de libro rz 7", kz, 4.9467926839619945, 27.113240570756212e-3, 1e-12);
    d4_zener(ZEN_1N4733A, 1e3, 12.0, &kz);
    d4_zener_chk("1N4733A 1k", kz, 4.9431873823062371, 27.133233607210867e-3, 1e-12);
    d4_zener(ZEN_1N4733A, 0.0, 12.0, &kz);
    d4_zener_chk("1N4733A sin carga", kz, 4.9783457050491179, 31.916610431594919e-3, 1e-12);
    d4_zener(ZEN_1N4733A, 1e3, 10.0, &kz);
    d4_zener_chk("1N4733A 10 V", kz, 4.8767051158608403, -1.0, 1e-12);
    d4_zener(ZEN_1N4733A, 1e3, 14.0, &kz);
    d4_zener_chk("1N4733A 14 V", kz, 5.006956672510536, -1.0, 1e-12);
    d4_zener(ZEN_EXP, 1e3, 12.0, &kz);
    d4_par(kz, QZ_VZ, 5.1); d4_par(kz, QZ_IZ, 5e-3);
    d4_zener_chk("zener exponencial", kz, 5.1426692944195435, 26.027015730946168e-3, 1e-12);
    {                                                  /* el 1N4733A en -5,1 V */
        c4_init(2);
        c4_el(EL_FUENTE, 0, 1, 5.1);
        kz = d4_d(EL_ZENER, 0, 1, ZEN_1N4733A);
        cierra_red();
        chk_cond("1N4733A en -5,1 V", "converge", sim_red_de(&rc, &rr, &s, &d) == SIM_OK && sim_op_cc_red(&s, &v, &d, 0) == SIM_OK);
        chk_t("1N4733A en -5,1 V", "I = -49 mA (1e-9)", v.I[kz], -49.000000005100754e-3, 49e-3, 1e-9);
        chk_t("1N4733A", "Rs = 6,4721443717075765", s.el[kz].m[2], 6.4721443717075765, 6.5, 1e-14);
        chk_t("1N4733A", "BV = 4,7828649257863288", s.el[kz].m[3], 4.7828649257863288, 4.8, 1e-14);
    }

    /* ---- LED ---- */
    for (i = 0; i < LED_N; i++) {
        sprintf(q, "LED %d", i);
        c4_init(2);
        c4_el(EL_FUENTE, 0, 1, NL_COLOR[i].Vf);
        k = d4_d(EL_LED, 1, 0, i);
        cierra_red();
        chk_cond(q, "converge", sim_red_de(&rc, &rr, &s, &d) == SIM_OK && sim_op_cc_red(&s, &v, &d, 0) == SIM_OK);
        chk_t(q, "Is (D6)", s.el[k].m[0], IS_LED[i], IS_LED[i], 1e-12);
        chk_t(q, "i(Vf) = 20 mA", v.I[k], 20e-3, 20e-3, 1e-12);
    }
    k = d4_serie(5.0, 160.0, EL_LED, LED_ROJO, 1);
    d4_op("LED rojo 5 V 160", k, 20e-3, 1.8, 1e-12, 0);
    k = d4_serie(9.0, 330.0, EL_LED, LED_ROJO, 1);
    d4_op("LED rojo 9 V 330", k, 21.777894098247443e-3, 1.8132949475783439, 1e-12, 0);
    k = d4_serie(5.0, 1e3, EL_LED, LED_ROJO, 1);
    d4_op("LED rojo 5 V 1k", k, 3.375166452816769e-3, 0, 1e-12, 0);
    k = d4_serie(9.0, 360.0, EL_LED, LED_ROJO, 1);
    d4_op("LED rojo 9 V 360", k, 20e-3, 1.8, 1e-12, 0);

    /* ---- Vcrit ---- */
    {
        static const int VAR[3] = { DIO_EXP, DIO_1N4148, DIO_1N4007 };
        static const double VC[3] = { 0.730289720247, 0.741283172253, 0.71850042798 };
        for (i = 0; i < 3; i++) {
            k = d4_serie(5.0, 1e3, EL_DIODO, VAR[i], 1);
            cierra_red();
            sim_red_de(&rc, &rr, &s, &d);
            chk_t("Vcrit", "la de SPICE3", s.el[k].m[5], VC[i], 1.0, 1e-11);
        }
    }

    /* ---- Homotopías y topes: los 10 diodos en serie ---- */
    k = d4_serie(10.0, 10.0, EL_DIODO, DIO_EXP, 10);
    cierra_red();
    sim_red_de(&rc, &rr, &s, &d);
    sim_nl_topes(5, 0);
    memset(&nw, 0, sizeof nw);
    chk_cond("homotop" S_ii "as", "con 5 iteraciones sale por gmin", sim_op_cc_red(&s, &v, &d, &nw) == SIM_OK && nw.homotopia == 1 &&
             (d.avisos & SIM_AV_HOMOTOPIA));
    chk_t("homotop" S_ii "as", "gmin: la misma I", v.I[k], 206.95689299461996e-3, 0.2, 1e-12);
    sim_nl_prueba(NLP_SIN_GMIN, 0.0);
    memset(&nw, 0, sizeof nw);
    chk_cond("homotop" S_ii "as", "sin gmin sale por la rampa", sim_op_cc_red(&s, &v, &d, &nw) == SIM_OK && nw.homotopia == 2);
    chk_t("homotop" S_ii "as", "rampa: la misma I", v.I[k], 206.95689299461996e-3, 0.2, 1e-12);
    sim_nl_prueba(NLP_SIN_HOMOTOPIA, 0.0);
    sim_nl_topes(1, 0);
    chk_cond("homotop" S_ii "as", "sin homotop" S_ii "a ni iteraciones: NO_CONVERGE en un diodo",
             sim_op_cc_red(&s, &v, &d, 0) == SIM_NO_CONVERGE && d.campo == NC_OP && s.el[d.elem].tipo == SE_D);
    sim_mensaje_circ(&rc, &s, &d, m, sizeof m);
    chk_cond("homotop" S_ii "as", "el mensaje cabe en 4 l" S_ii "neas y nombra el diodo", lineas(m, 52) <= 4 && strstr(m, "D") != 0);
    sim_nl_prueba(NLP_NADA, 0.0);
    sim_nl_topes(1000, 0);
    sim_nl_tope_ops(2000);
    chk_cond("tope", "con el tope de operaciones bajado: NC_TOPE", sim_op_cc_red(&s, &v, &d, 0) == SIM_NO_CONVERGE && d.campo == NC_TOPE);
    sim_mensaje_circ(&rc, &s, &d, m, sizeof m);
    chk_cond("tope", "su mensaje", strstr(m, "tarda demasiado") != 0);
    sim_nl_tope_ops(0);
    sim_nl_topes(0, 0);
    chk_cond("tope", "sin ganchos, otra vez bien", sim_op_cc_red(&s, &v, &d, 0) == SIM_OK);

    /* ---- Errores del modelo: SIM_MODELO, no SIM_FALTA ---- */
    d4_zener(ZEN_LIBRO, 1e3, 12.0, &kz);
    d4_par(kz, QZ_RZ, 2e3); d4_par(kz, QZ_IZ, 5e-3);
    cierra_red();
    chk_cond("SIM_MODELO", "zener de libro con rz" S_CDOT "Iz > Vz: QZ_RZ", sim_red_de(&rc, &rr, &s, &d) == SIM_MODELO && d.campo == QZ_RZ && d.elem == kz);
    sim_mensaje_circ(&rc, &s, &d, m, sizeof m);
    chk_cond("SIM_MODELO", "zener: su texto", lineas(m, 52) <= 4 && strstr(m, "rz") && !strstr(m, "falta"));
    d4_zener(ZEN_EXP, 1e3, 12.0, &kz);
    d4_par(kz, QZ_VZ, 5.1); d4_par(kz, QZ_IZ, 5e-3); d4_par(kz, QZ_RZ, 1100.0);
    cierra_red();
    chk_cond("SIM_MODELO", "zener exponencial con rz = 1100: QZ_RZ", sim_red_de(&rc, &rr, &s, &d) == SIM_MODELO && d.campo == QZ_RZ);
    d4_par(kz, QZ_RZ, 1000.0);
    chk_cond("SIM_MODELO", "con rz = 1000 s" S_ii " rompe (BV > 0)", sim_red_de(&rc, &rr, &s, &d) == SIM_OK && s.el[kz].m[3] > 0.0);
    k = d4_serie(5.0, 1e3, EL_LED, LED_AZUL, 1);
    d4_par(k, QL_IF, 0.25);
    cierra_red();
    chk_cond("SIM_MODELO", "LED azul con If = 250 mA: QL_IF", sim_red_de(&rc, &rr, &s, &d) == SIM_MODELO && d.campo == QL_IF);
    sim_mensaje_circ(&rc, &s, &d, m, sizeof m);
    chk_cond("SIM_MODELO", "LED: su texto", lineas(m, 52) <= 4 && strstr(m, "If") && !strstr(m, "falta"));
    d4_par(k, QL_IF, 0.2);
    chk_cond("SIM_MODELO", "con If = 200 mA vale", sim_red_de(&rc, &rr, &s, &d) == SIM_OK);
    k = d4_serie(5.0, 1e3, EL_DIODO, DIO_EXP, 1);
    rc.el[k].variante = 7;
    cierra_red();
    chk_cond("SIM_MODELO", "un diodo con variante 7: Q_VAL", sim_red_de(&rc, &rr, &s, &d) == SIM_MODELO && d.campo == Q_VAL);
    sim_mensaje_circ(&rc, &s, &d, m, sizeof m);
    chk_cond("SIM_MODELO", "variante: su texto", lineas(m, 52) <= 4 && strstr(m, "no existe") && !strstr(m, "falta"));

    /* ---- La tabla con un diodo; el control por la I de un diodo ---- */
    k = d4_serie(5.0, 1e3, EL_DIODO, DIO_VG, 1);
    cierra_red();
    resolver(&rc, &rr, &so);
    chk_cond("SOL_NO_LINEAL", "la tabla no despeja con un diodo", so.estado == SOL_NO_LINEAL && so.elem == k);
    sol_mensaje(&rc, &rr, &so, m, sizeof m);
    chk_cond("SOL_NO_LINEAL", "su texto: 4 l" S_ii "neas y [I]", lineas(m, 52) <= 4 && strstr(m, "[I]") != 0);
    {
        int ke;
        k = d4_serie(5.0, 1e3, EL_DIODO, DIO_VG, 1);
        ke = c4_dep(EL_EDEP, 0, 3, DEP_I, 1e3, k);
        nodos = 4;
        c4_el(EL_RES, 3, 0, 1e3);
        cierra_red();
        resolver(&rc, &rr, &so);
        chk_cond("CTL_I_NO", "la tabla: SOL_NO_LINEAL antes (hay un diodo)", so.estado == SOL_NO_LINEAL);
        chk_cond("CTL_I_NO", "circ_ctl_motivo con un diodo", circ_ctl_motivo(&rc, ke) == CTL_I_NO);
        chk_cond("CTL_I_NO", "el emulador: SIM_CTL_MAL con el mismo motivo", sim_punto_op_cc(&rc, &rr, &v, &d, 0) == SIM_CTL_MAL &&
                 d.campo == CTL_I_NO && d.elem == ke && d.nodo == k);
    }

    /* ---- Estado intacto: una emulación a medias no se entera de sim_op_cc_red ---- */
    {
        static SimRes res1;
        static SimReg reg1;
        static SimRed red1, otra;
        int kS, kE, kC, kL, e;
        sr_init(5);
        kE = sr_escalon(0, 1, 10.0);
        kS = sr_el(SE_S, 1, 2, 0.0); sr.el[kS].t1 = 1e-3;
        sr_el(SE_R, 2, 3, 100.0);
        kL = sr_el(SE_L, 3, 4, 10e-3);
        kC = sr_el(SE_C, 4, 0, 1e-6);
        (void)kE;
        canal(0, CANAL_V, kC); canal(1, CANAL_I, kL);
        saj.t_fin = 5e-3; saj.h = 0.0;
        /* la de referencia, de un tirón */
        chk_cond("estado intacto", "la emulaci" S_oo "n de referencia", sim_emular(&sr, &saj) == SIM_OK);
        res1 = *sim_res(); reg1 = *sim_reg();
        /* a medias */
        e = sim_iniciar_red(&sr, &saj);
        chk_cond("estado intacto", "empieza", e == SIM_OK && sim_avanzar(20000ul));
        red1 = *sim_red();
        {
            static SimRes r0;
            r0 = *sim_res();
            /* otras redes, con diodos por tramos y exponenciales */
            k = d4_serie(5.0, 1e3, EL_DIODO, DIO_VG, 1); cierra_red(); sim_red_de(&rc, &rr, &otra, &d);
            chk_cond("estado intacto", "OP por tramos", sim_op_cc_red(&otra, &v, &d, 0) == SIM_OK);
            k = d4_serie(10.0, 10.0, EL_DIODO, DIO_EXP, 10); cierra_red(); sim_red_de(&rc, &rr, &otra, &d);
            chk_cond("estado intacto", "OP exponencial", sim_op_cc_red(&otra, &v, &d, 0) == SIM_OK);
            d4_zener(ZEN_LIBRO, 1e3, 12.0, &kz); cierra_red(); sim_red_de(&rc, &rr, &otra, &d);
            chk_cond("estado intacto", "OP del zener", sim_op_cc_red(&otra, &v, &d, 0) == SIM_OK);
            chk_cond("estado intacto", "sim_red() sin tocar", memcmp(&red1, sim_red(), sizeof red1) == 0);
            chk_cond("estado intacto", "sim_res() sin tocar", memcmp(&r0, sim_res(), sizeof r0) == 0);
        }
        while (sim_avanzar(~0ul)) {}
        chk_cond("estado intacto", "acaba", sim_res()->fin == FIN_COMPLETO);
        chk_cond("estado intacto", "el registro, bit a bit el de un tir" S_oo "n",
                 sim_reg()->n == reg1.n && memcmp(sim_reg()->t, reg1.t, sizeof reg1.t) == 0 &&
                 memcmp(sim_reg()->y, reg1.y, sizeof reg1.y) == 0);
        chk_cond("estado intacto", "sim_res(), bit a bit el de un tir" S_oo "n", memcmp(sim_res(), &res1, sizeof res1) == 0);
    }
    semilla = sem0;
}
