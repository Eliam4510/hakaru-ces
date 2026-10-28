/* --------------------------------------------------------------------------
 *  65p. Etapa 4: el punto de operación en la tercera revisión (grupo op_a)
 *
 *  Una prueba por hallazgo (cada una falla sin su arreglo, comprobado por
 *  mutación): dos AO ideales con uno dentro del lazo del otro (la regla de
 *  la realimentación con S = 0, §3.F, y a qué saturación va un AO cuyo
 *  lineal es singular) y un diodo por tramos que se quedaba en conducción
 *  (o en ruptura) con la corriente del revés (la tolerancia de su margen:
 *  Katzenelson en el OP y el final de un paso del transitorio). Los valores
 *  esperados salen de la teoría del circuito o de mpmath (40 cifras). Se
 *  incluye tras test_rev2_editor.inc (usa sr, saj, sr_init, sr_el, canal_de,
 *  a4_ao, a4_cc, c4_init, c4_el, d4_d, d4_par, d4_i_tramos, nuevo_el, rc, rr,
 *  cierra_red y los chk).
 * ----------------------------------------------------------------------- */

/* Un AO ideal con raíles de ±15 V (a4_ao con real = 0) */
static int r3oa_ao(int mas, int menos, int sal)
{
    return a4_ao(mas, menos, sal, 0, 0, 0.0);
}

/* La unión 1N4148 de la ficha (Is, n, Rs) entre a y b */
static int r3oa_dexp(int a, int b)
{
    int k = sr_el(SE_D, a, b, 0.0);
    SimEl *x = &sr.el[k];
    x->modelo = NL_UNION;
    x->m[0] = 2.52e-9; x->m[1] = 1.752; x->m[2] = 0.568;
    x->m[5] = nl_vcrit(x->m[0], x->m[1] * NL_VT);
    return k;
}

/* El detector de pico con seguidor en sr: E1 (2 V de CC) en la + de U1, su
 * salida (2) a D1, el cátodo (3) a C1 de 1 µF con V0 = 0 y a la + de U2,
 * seguidor (4) a la - de U1. D1: la unión 1N4148 (vg = 0) o con Vγ (vg = 1) */
static void r3oa_pico(int vg)
{
    int k;
    sr_init(5);
    a4_cc(0, 1, 2.0);
    r3oa_ao(1, 4, 2);
    if (vg) {
        k = sr_el(SE_D, 2, 3, 0.0);
        sr.el[k].modelo = NL_TRAMOS; sr.el[k].m[0] = 0.7; sr.el[k].m[1] = NL_RON; sr.el[k].m[3] = NL_RON;
    } else r3oa_dexp(2, 3);
    k = sr_el(SE_C, 3, 0, 1e-6); sr.el[k].ci = 1; sr.el[k].x0 = 0.0;
    r3oa_ao(3, 4, 4);
}

/* E1 pasa a una senoidal de 2 V y 1 kHz; 2 ms con h = 10 µs, el canal V(C1) */
static void r3oa_pico_seno(void)
{
    SimEl *e = &sr.el[0];
    e->forma = ONDA_SENO; e->p[0] = 2.0; e->p[1] = 1e3; e->p[2] = 0.0; e->p[3] = 0.0;
    memset(&saj, 0, sizeof saj);
    saj.t_fin = 2e-3; saj.h = 1e-5;
    canal_de(&saj, &sr, 0, CANAL_NODO, 3);
}

/* ==========================================================================
 *  1. Dos AO ideales, uno dentro del lazo del otro
 * ======================================================================= */
static void r3oa_ao_anidados(void)
{
    static Valores v;
    SimDiag d;
    SimNewton nw;
    char m[300];
    const char *q = "AO anidados";
    int k, kU1, kU2, kU3, e;

    /* (a) no inversor de ganancia 2 (E1 = 1 V en +, R1 = R2 = 10 kΩ) con un
     *     seguidor U2 dentro del lazo y RL de 10 kΩ: Vsal = 2 V con U1 y U2
     *     en su lineal. La regla de U2 sola da S = 0 exacto (el nulador de U1
     *     fija su salida): el lineal es el límite del AO real (A → ∞), que
     *     converge, y los dos deben quedar en su lineal */
    sr_init(6);
    a4_cc(0, 1, 1.0);
    kU1 = r3oa_ao(1, 5, 3);
    kU2 = r3oa_ao(3, 4, 4);
    sr_el(SE_R, 4, 5, 10e3); sr_el(SE_R, 5, 0, 10e3);
    sr_el(SE_R, 3, 0, 10e3);
    memset(&nw, 0, sizeof nw);
    e = sim_op_cc_red(&sr, &v, &d, &nw);
    sim_mensaje_circ(0, &sr, &d, m, sizeof m);
    printf("    [P] seguidor en el lazo: estado %d (%s), %ld iteraciones, Vsal = %.12g V\n", e, e == SIM_OK ? "" : m,
           nw.iter, v.phi[4]);
    chk_cond(q, "[P] del no inversor con un seguidor en el lazo: converge", e == SIM_OK);
    chk_cond(q, "Vsal = 2 V (U1 y U2 en su lineal)", e == SIM_OK && fabs(v.phi[4] - 2.0) < 1e-12 &&
             fabs(v.phi[3] - 2.0) < 1e-12 && v.reg[kU1] == REG_LINEAL && v.reg[kU2] == REG_LINEAL);
    e = sim_op_red(&sr, &v, &d);
    chk_cond(q, "el OP del emulador: converge con Vsal = 2 V", e == SIM_OK && fabs(v.phi[4] - 2.0) < 1e-12);
    /* el mismo con los dos AO reales (A = 1e5): converge (el control) */
    sr.el[kU1].modelo = NL_AO_REAL; sr.el[kU1].m[0] = 1e5;
    sr.el[kU2].modelo = NL_AO_REAL; sr.el[kU2].m[0] = 1e5;
    e = sim_op_cc_red(&sr, &v, &d, &nw);
    printf("    con AO reales: estado %d, Vsal = %.9g V\n", e, v.phi[4]);
    chk_cond(q, "con AO reales (el control): converge a 2 V", e == SIM_OK && fabs(v.phi[4] - 2.0) < 1e-3);

    /* (b) el mismo, con nombres de verdad (Circuito): sin «no converge» */
    c4_init(6);
    c4_el(EL_FUENTE, 0, 1, 1.0);
    kU1 = nuevo_el(EL_AO, 1, 5);
    kU2 = nuevo_el(EL_AO, 3, 4);
    c4_el(EL_RES, 4, 5, 10e3); c4_el(EL_RES, 5, 0, 10e3); c4_el(EL_RES, 3, 0, 10e3);
    for (k = 0; k < rc.nel; k++) rr.c[k] = -1;
    rr.c[kU1] = 3; rr.c[kU2] = 4;
    cierra_red();
    memset(&nw, 0, sizeof nw);
    e = sim_punto_op_cc(&rc, &rr, &v, &d, &nw);
    sim_mensaje_circ(&rc, 0, &d, m, sizeof m);
    printf("    Circuito: estado %d: %s\n", e, m);
    chk_cond(q, "Circuito con U1 y U2 ideales: converge (sin «no converge»)",
             e == SIM_OK && !strstr(m, "no converge") && fabs(v.phi[4] - 2.0) < 1e-12);

    /* (c) dos seguidores en el lazo (U2 y U3): la regla de U2 y la de U3 dan
     *     S = 0; los tres en su lineal, Vsal = 2 V */
    sr_init(7);
    a4_cc(0, 1, 1.0);
    kU1 = r3oa_ao(1, 6, 3);
    kU2 = r3oa_ao(3, 4, 4);
    kU3 = r3oa_ao(4, 5, 5);
    sr_el(SE_R, 5, 6, 10e3); sr_el(SE_R, 6, 0, 10e3);
    memset(&nw, 0, sizeof nw);
    e = sim_op_cc_red(&sr, &v, &d, &nw);
    printf("    [P] dos seguidores en el lazo: estado %d, Vsal = %.12g V\n", e, v.phi[5]);
    chk_cond(q, "dos seguidores en el lazo: los tres en su lineal, Vsal = 2 V", e == SIM_OK &&
             fabs(v.phi[5] - 2.0) < 1e-12 && v.reg[kU1] == REG_LINEAL && v.reg[kU2] == REG_LINEAL &&
             v.reg[kU3] == REG_LINEAL);

    /* (d) dos AO cruzados con realimentación negativa (U1: + = 1 V, - = la
     *     salida de U2; U2: + = la salida de U1, - = tierra): cada salida la
     *     fija el nulador del otro (S = 0 en los dos). Con AO reales la
     *     ganancia del lazo es -A² (vale): U1 en 0 V, U2 en 1 V */
    sr_init(5);
    a4_cc(0, 1, 1.0);
    kU1 = r3oa_ao(1, 4, 3);
    kU2 = r3oa_ao(3, 0, 4);
    sr_el(SE_R, 3, 0, 10e3); sr_el(SE_R, 4, 0, 10e3);
    memset(&nw, 0, sizeof nw);
    e = sim_op_cc_red(&sr, &v, &d, &nw);
    printf("    [P] cruzados (lazo -A" S_SUP2 "): estado %d, V3 = %.9g V, V4 = %.9g V, regiones %d %d\n", e, v.phi[3],
           v.phi[4], v.reg[kU1], v.reg[kU2]);
    chk_cond(q, "cruzados con lazo negativo: los dos en su lineal (V3 = 0, V4 = 1 V)", e == SIM_OK &&
             fabs(v.phi[3]) < 1e-12 && fabs(v.phi[4] - 1.0) < 1e-12 && v.reg[kU1] == REG_LINEAL && v.reg[kU2] == REG_LINEAL);
    sr.el[kU1].modelo = NL_AO_REAL; sr.el[kU1].m[0] = 1e5;
    sr.el[kU2].modelo = NL_AO_REAL; sr.el[kU2].m[0] = 1e5;
    e = sim_op_cc_red(&sr, &v, &d, &nw);
    printf("    con AO reales: estado %d, V3 = %.9g V, V4 = %.9g V\n", e, v.phi[3], v.phi[4]);
    chk_cond(q, "cruzados, con AO reales (el control): V4 = 1 V", e == SIM_OK && fabs(v.phi[4] - 1.0) < 1e-6);

    /* (e) los mismos con realimentación positiva (U2: - = la salida de U1):
     *     lazo +A², el lineal no vale (S = 0 en los dos, pero el término
     *     siguiente es positivo): un biestable, cada AO en un raíl */
    sr_init(5);
    a4_cc(0, 1, 1.0);
    kU1 = r3oa_ao(1, 4, 3);
    kU2 = r3oa_ao(0, 3, 4);
    sr_el(SE_R, 3, 0, 10e3); sr_el(SE_R, 4, 0, 10e3);
    memset(&nw, 0, sizeof nw);
    e = sim_op_cc_red(&sr, &v, &d, &nw);
    printf("    [P] cruzados (lazo +A" S_SUP2 "): estado %d, V3 = %.9g V, V4 = %.9g V, regiones %d %d, avisos %#x\n", e,
           v.phi[3], v.phi[4], v.reg[kU1], v.reg[kU2], d.avisos);
    chk_cond(q, "cruzados con lazo positivo: satura (no el equilibrio inestable V3 = 0, V4 = 1 V)", e == SIM_OK &&
             fabs(fabs(v.phi[3]) - 15.0) < 1e-9 && fabs(fabs(v.phi[4]) - 15.0) < 1e-9 && v.phi[3] * v.phi[4] < 0.0);

    /* (f) un Schmitt con el seguidor en el lazo (U1: - = la entrada, + = la
     *     mitad de la salida de U2): el lineal de U1 sigue sin valer */
    sr_init(6);
    a4_cc(0, 1, 0.0);
    kU1 = r3oa_ao(5, 1, 3);
    kU2 = r3oa_ao(3, 4, 4);
    sr_el(SE_R, 4, 5, 10e3); sr_el(SE_R, 5, 0, 10e3);
    memset(&nw, 0, sizeof nw);
    e = sim_op_cc_red(&sr, &v, &d, &nw);
    printf("    [P] Schmitt con seguidor: estado %d, Vsal = %.9g V, avisos %#x\n", e, v.phi[4], d.avisos);
    chk_cond(q, "Schmitt con seguidor: U1 en V+ con el aviso BIESTABLE", e == SIM_OK && fabs(v.phi[4] - 15.0) < 1e-9 &&
             v.reg[kU1] == REG_SAT_MAS && (d.avisos & SIM_AV_BIESTABLE));

    /* (g) un seguidor U1 (+ en 2, - y salida en 3) cuya salida fija el
     *     nulador de U2 (+ en E2 = -20 V, - en 3, salida en 2) fuera de los
     *     raíles: con los dos en su lineal, U1 daría -20 V. La regla de U1 da
     *     S = 0 y el término siguiente, < 0 (vale), así que sale por su raíl;
     *     entonces la LU con U2 en LIN es singular, y U2 tiene que ir a la
     *     saturación del signo de su salida (V-), no siempre a V+: si no,
     *     Katzenelson da vueltas (U1 y U2 entran y salen) hasta NLR_CICLA.
     *     Con AO reales (A = 1e6): U2 en V-, U1 lo sigue: V3 = -15 V */
    sr_init(5);
    a4_cc(0, 1, 10.0);
    a4_cc(0, 4, -20.0);
    sr_el(SE_R, 2, 1, 2e3); sr_el(SE_R, 3, 1, 1.6e3);
    kU1 = r3oa_ao(2, 3, 3);
    kU2 = r3oa_ao(4, 3, 2);
    memset(&nw, 0, sizeof nw);
    e = sim_op_cc_red(&sr, &v, &d, &nw);
    printf("    [P] seguidor con la salida fijada fuera de los ra" S_ii "les: estado %d, V2 = %.9g V, V3 = %.9g V, "
           "regiones %d %d\n", e, v.phi[2], v.phi[3], v.reg[kU1], v.reg[kU2]);
    chk_cond(q, "seguidor fijado fuera de los ra" S_ii "les: converge con U2 en V- (V2 = V3 = -15 V)", e == SIM_OK &&
             fabs(v.phi[2] + 15.0) < 1e-9 && fabs(v.phi[3] + 15.0) < 1e-9 && v.reg[kU2] == REG_SAT_MENOS);
    sr.el[kU1].modelo = NL_AO_REAL; sr.el[kU1].m[0] = 1e6;
    sr.el[kU2].modelo = NL_AO_REAL; sr.el[kU2].m[0] = 1e6;
    e = sim_op_cc_red(&sr, &v, &d, &nw);
    printf("    con AO reales: estado %d, V2 = %.9g V, V3 = %.9g V\n", e, v.phi[2], v.phi[3]);

    /* (h) detector de pico con seguidor (U1, 1N4148, C1 de 1 µF con V0 = 0,
     *     U2 seguidor a la - de U1): con 2 V de CC, C1 abierto: todo en 2 V.
     *     Con una senoidal de 2 V y 1 kHz, [=] arranca: su OP converge. (El
     *     paso del pico, con la unión que el nulador tiene que apagar, falla
     *     igual sin el seguidor: es el del superdiodo, del transitorio.) */
    r3oa_pico(0);
    memset(&nw, 0, sizeof nw);
    e = sim_op_cc_red(&sr, &v, &d, &nw);
    printf("    [P] detector de pico: estado %d, %ld iteraciones, V3 = %.12g V, V4 = %.12g V\n", e, nw.iter, v.phi[3], v.phi[4]);
    chk_cond(q, "[P] del detector de pico con seguidor: converge (V(C1) = 2 V)",
             e == SIM_OK && fabs(v.phi[3] - 2.0) < 1e-9 && fabs(v.phi[4] - 2.0) < 1e-12);
    r3oa_pico_seno();
    e = sim_op_red(&sr, &v, &d);
    chk_cond(q, "detector de pico con una senoidal: el OP del emulador converge (V(C1) = 0)",
             e == SIM_OK && fabs(v.phi[3]) < 1e-12 && fabs(v.phi[4]) < 1e-12);
    e = sim_emular(&sr, &saj);
    sim_mensaje_circ(0, &sr, &sim_res()->d, m, sizeof m);
    printf("    [=] detector de pico con una senoidal: estado %d, t = %g s (%s)\n", e, sim_res()->d.t, e == SIM_OK ? "" : m);
    chk_cond(q, "[=] del detector de pico con seguidor: arranca (no se queda en el OP)",
             e == SIM_OK || (sim_res()->d.campo != NC_OP && sim_res()->d.t > 0.0));

    /* (i) el mismo con D1 con Vγ (por tramos): se emula entero y C1 se
     *     carga al pico, 2 V, y ahí se queda */
    r3oa_pico(1);
    r3oa_pico_seno();
    e = sim_emular(&sr, &saj);
    sim_mensaje_circ(0, &sr, &sim_res()->d, m, sizeof m);
    {
        const SimReg *g = sim_reg();
        double mx = -1e9;
        int i;
        for (i = 0; e == SIM_OK && i < g->n; i++) if (g->y[0][i] > mx) mx = g->y[0][i];
        printf("    [=] detector de pico con Vg: estado %d (%s), V(C1) m" S_aa "x %.12g V, al final %.12g V\n", e,
               e == SIM_OK ? "" : m, mx, e == SIM_OK && g->n > 0 ? g->y[0][g->n - 1] : 0.0);
        chk_cond(q, "[=] del detector de pico con seguidor y Vg: se emula y C1 se queda en el pico (2 V)",
                 e == SIM_OK && g->n > 0 && fabs(mx - 2.0) < 1e-6 && fabs(g->y[0][g->n - 1] - 2.0) < 1e-6);
    }
}

/* ==========================================================================
 *  2. Un diodo por tramos en conducción (o en ruptura) con la corriente del
 *     revés: Katzenelson lo daba por bueno
 * ======================================================================= */

/* El regulador: E1 (E) en 1, R1 (Rs) de 1 a 2, D2 zener exponencial de 5,1
 * V (Iz 5 mA, rz vacía) de tierra a 2, y el divisor R2 (2-3), R3 (3-0) */
static void r3oa_regulador(int nn, double E, double Rs, double R2, double R3)
{
    c4_init(nn);
    c4_el(EL_FUENTE, 0, 1, E);
    c4_el(EL_RES, 1, 2, Rs);
    d4_d(EL_ZENER, 0, 2, ZEN_EXP);
    c4_el(EL_RES, 2, 3, R2);
    c4_el(EL_RES, 3, 0, R3);
}

/* Resuelve [P] y el OP del emulador, y comprueba que D1 (kD) está en corte
 * con V(D1) = vd y V3 = v3 (mpmath) en los dos, con su ley (GMIN·V) */
static void r3oa_corte(const char *det, int kD, double vd, double v3)
{
    static Valores v, w;
    static SimRed s;
    SimDiag d;
    SimNewton nw;
    char t[120];
    int e, e2;
    cierra_red();
    e = sim_red_de(&rc, &rr, &s, &d);
    memset(&nw, 0, sizeof nw);
    e = e == SIM_OK ? sim_op_cc_red(&s, &v, &d, &nw) : e;
    e2 = sim_op_red(&s, &w, &d);
    printf("    %s: estado %d, V(D1) = %.12g V (mpmath %.12g), I(D1) = %.6g A, P(D1) = %.3g W, V3 = %.12g V (mpmath %.12g); "
           "OP del emulador: V3 = %.12g V\n", det, e, v.V[kD], vd, v.I[kD], v.P[kD], v.phi[3], v3, w.phi[3]);
    sprintf(t, "%s: V(D1) y V3 los de mpmath (D1 en corte)", det);
    chk_cond("diodo por tramos", t, e == SIM_OK && fabs(v.V[kD] - vd) < 1e-9 && fabs(v.phi[3] - v3) < 1e-9);
    sprintf(t, "%s: I(D1) cumple su ley (GMIN" S_CDOT "V) y no entrega potencia", det);
    chk_cond("diodo por tramos", t, e == SIM_OK && fabs(v.I[kD] - d4_i_tramos(s.el[kD].m, v.V[kD])) <= 1e-15 && v.P[kD] >= 0.0);
    sprintf(t, "%s: el OP del emulador, igual", det);
    chk_cond("diodo por tramos", t, e2 == SIM_OK && fabs(w.V[kD] - vd) < 1e-9 && fabs(w.phi[3] - v3) < 1e-9);
}

static void r3oa_diodo_pila(void)
{
    int kD;

    /* (a) el del buscador: E1 12 V, R1 220 Ω, R2 10 kΩ y R3 68 kΩ, y D1 con
     *     Vγ (0,7 V, rd vacía) en paralelo con R2. En conducción (una pila de
     *     Vγ con Ron = 1 mΩ) llevaría -4,6 µA, -4,6e-9 V en su margen, dentro
     *     del -1e-9·Vesc que se toleraba: V3 salía 40 mV por debajo */
    r3oa_regulador(4, 12.0, 220.0, 10e3, 68e3);
    kD = d4_d(EL_DIODO, 2, 3, DIO_VG);
    r3oa_corte("D1 con Vg en R2", kD, 0.65990523067462841, 4.4873556134610289);

    /* (b) el diodo ideal: E1 9 V, R1 100 Ω, R2 4,7 kΩ, R3 22 kΩ, y otro
     *     divisor de E1 (R4 y R5 de 68 kΩ, 4,5 V en 4); D1 ideal de 3 a 4.
     *     En corte V(D1) = -0,254 V; en conducción los unía con -6,7 µA
     *     (-6,7e-9 V de margen) y V3 salía 26 mV por encima */
    r3oa_regulador(5, 9.0, 100.0, 4.7e3, 22e3);
    c4_el(EL_RES, 1, 4, 68e3);
    c4_el(EL_RES, 4, 0, 68e3);
    kD = d4_d(EL_DIODO, 3, 4, DIO_IDEAL);
    r3oa_corte("D1 ideal entre dos divisores", kD, -0.25437230072772092, 4.2456276906236209);

    /* (c) la ruptura del zener de libro: el mismo regulador y D1 zener de
     *     libro (Vz = 4,25 V, rz vacía) de tierra a 3. En corte V3 = 4,2456 V
     *     (4,4 mV por debajo de Vz0); en ruptura hacía de pila de Vz0 con la
     *     corriente del sentido directo */
    r3oa_regulador(4, 9.0, 100.0, 4.7e3, 22e3);
    kD = d4_d(EL_ZENER, 0, 3, ZEN_LIBRO);
    d4_par(kD, QZ_VZ, 4.25);
    r3oa_corte("D1 zener de libro en el divisor", kD, -4.2456276731946043, 4.2456276731946043);

    /* (d) lo mismo al final de un paso con los tramos congelados: el
     *     regulador de (a) con C1 de 1 µF (V0 = 0) en R3, 0,5 s con h = 1 ms.
     *     D1 arranca conduciendo (C1 en 0 V); su corriente baja y pasa por 0
     *     cuando V3 llega a V2 - Vγ, y D1 tiene que cortarse ahí (un evento):
     *     V3 sube hasta los 4,48735561346 V de (a). Con la tolerancia en V se
     *     quedaba conduciendo con -4,6 µA hasta el final (V3 = 4,4473 V) */
    r3oa_regulador(4, 12.0, 220.0, 10e3, 68e3);
    kD = d4_d(EL_DIODO, 2, 3, DIO_VG);
    cierra_red();
    {
        static SimRed s;
        SimDiag d;
        SimEl *c;
        const SimReg *g;
        int e = sim_red_de(&rc, &rr, &s, &d);
        c = &s.el[s.nel++];
        memset(c, 0, sizeof *c);
        c->tipo = SE_C; c->a = 3; c->b = 0; c->val = 1e-6; c->k = -1; c->t1 = c->t2 = -1.0;
        c->ci = 1; c->x0 = 0.0;
        memset(&saj, 0, sizeof saj);
        saj.t_fin = 0.5; saj.h = 1e-3;
        canal_de(&saj, &s, 0, CANAL_NODO, 3);
        canal_de(&saj, &s, 1, CANAL_I, kD);
        if (e == SIM_OK) e = sim_emular(&s, &saj);
        g = sim_reg();
        printf("    D1 con Vg en R2 y C1 en R3 ([=], 0,5 s): estado %d, %d cambios de tramo, al final V3 = %.12g V, "
               "I(D1) = %.6g A\n", e, sim_res()->nw.eventos, g->n ? g->y[0][g->n - 1] : 0.0, g->n ? g->y[1][g->n - 1] : 0.0);
        chk_cond("diodo por tramos", "[=] con C1 en R3: D1 se corta y V3 acaba en 4,48735561346 V (la de (a))",
                 e == SIM_OK && g->n > 0 && sim_res()->nw.eventos >= 1 && fabs(g->y[0][g->n - 1] - 4.4873556134610289) < 1e-9 &&
                 fabs(g->y[1][g->n - 1]) < 1e-12);
    }
}

static void test_rev3_op_a(void)
{
    printf("[65p] Etapa 4: el punto de operaci" S_oo "n (tercera revisi" S_oo "n, grupo op_a)\n");
    r3oa_ao_anidados();
    r3oa_diodo_pila();
}
