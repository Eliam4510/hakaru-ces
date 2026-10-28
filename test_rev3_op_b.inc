/* --------------------------------------------------------------------------
 *  65q. Etapa 4: los hallazgos del punto de operación de la tercera revisión
 *  (grupo op_b)
 *
 *  Una prueba por hallazgo (cada una falla sin su arreglo, comprobado por
 *  mutación): el «Kirchhoff» que se enseña tras gmin, una unión sin R sobre
 *  una E ideal (o sobre la salida saturada de un AO) y el «Kirchhoff» de un
 *  AO real con Ro > 0. Los valores exactos salen de mpmath (50 cifras). Se
 *  incluye tras test_rev2_editor.inc (usa sr, sr_init, sr_el, a4_cc, q4_bjt,
 *  m4_mos, c4_init, c4_el, d4_d, d4_serie, d4_i_union, nuevo_el, nodos,
 *  cierra_red, rc, rr, r4_serie, r3u_abre, ve, TECLAS y los chk).
 * ----------------------------------------------------------------------- */

/* La ley de nodos de unos Valores, aparte del motor (con el ruido de
 * redondeo de las R en la escala, como d4_criterios), y la peor ley de un
 * diodo o un MOSFET en su V */
static double rob_kcl(const SimRed *s, const Valores *v, double *peor_ley)
{
    static double suma[MAX_NODOS], modu[MAX_NODOS];
    double peor_k = 0.0, peor_l = 0.0, t;
    int k, n;
    for (n = 0; n < s->nn; n++) suma[n] = modu[n] = 0.0;
    for (k = 0; k < s->nel; k++) {
        const SimEl *e = &s->el[k];
        double m = fabs(v->I[k]);
        if (e->tipo == SE_R) m += 2.2e-4 / e->val * (fabs(v->phi[e->a]) + fabs(v->phi[e->b]));
        if (e->tipo == SE_MOS) {                         /* la puerta no lleva corriente */
            NlMos o;
            nl_mos(e, v->V2[k], v->V[k], &o);
            t = fabs(v->I[k] - o.id) / (fabs(o.id) + 1e-12); if (t > peor_l) peor_l = t;
        }
        if (e->tipo == SE_D) {
            double im = d4_i_union(e->m, v->V[k]);
            t = fabs(v->I[k] - im) / (fabs(im) + 1e-12); if (t > peor_l) peor_l = t;
        }
        suma[e->a] += v->I[k]; modu[e->a] += m;
        suma[e->b] -= v->I[k]; modu[e->b] += m;
    }
    for (n = 0; n < s->nn; n++) {
        t = fabs(suma[n]) / (modu[n] + 1e-15);
        if (!s->ref[n] && t > peor_k) peor_k = t;
    }
    if (peor_ley) *peor_ley = peor_l;
    return peor_k;
}

/* ==========================================================================
 *  1. Tras gmin, «Kirchhoff» era el de los pasos con gmin (0,5 a 1), no el
 *     de la solución: en rojo sobre un resultado correcto
 * ======================================================================= */
static void rob_gmin(void)
{
    static Circuito c;
    static SimRed s;
    static Valores v;
    SimDiag d;
    SimNewton nw;
    double kc, ly;
    int e, i, visto = 0;
    char t[24];
    const char *q = "R3 op_b gmin";

    /* (a) una red al azar de la revisión: E real (2,02 Ω, -16,2 V), dos R y
     *     un NMOS (Vt 0,0176 V, K 12,4 A/V²) y un PMOS (Vt 0,306 V, K 186
     *     A/V²) que se alimentan uno a otro: el Newton directo no llega y
     *     sale por gmin */
    sr_init(4);
    e = sr_el(SE_E, 0, 1, 2.0207181977772244); sr.el[e].forma = ONDA_CC; sr.el[e].p[0] = -16.198358535766602;
    sr_el(SE_R, 2, 0, 2213.4207877890449);
    sr_el(SE_R, 3, 0, 5004.5313075661606);
    m4_mos(1, 0, 3, 1, 0.017558147883317662, 12.377488517718351, 0.0);
    m4_mos(2, 3, 1, -1, 0.30598365155188489, 186.47938854420454, 0.0);
    memset(&nw, 0, sizeof nw);
    e = sim_op_cc_red(&sr, &v, &d, &nw);
    kc = rob_kcl(&sr, &v, &ly);
    printf("    NMOS y PMOS: estado %d, homotop" S_ii "a %d, %ld iter.; Kirchhoff del motor %.3g; el del arn" S_ee "s %.3g (ley %.3g)\n",
           e, nw.homotopia, nw.iter, nw.residuo_max, kc, ly);
    chk_cond(q, "NMOS y PMOS: sale por gmin y su soluci" S_oo "n cumple Kirchhoff y las leyes (el arn" S_ee "s)",
             e == SIM_OK && nw.homotopia == 1 && kc <= 1e-9 && ly <= 1e-9);
    chk_cond(q, "NMOS y PMOS: el Kirchhoff del motor es el de su soluci" S_oo "n (<= 1e-9)", e == SIM_OK && nw.residuo_max <= 1e-9);

    /* (b) los 10 diodos exponenciales en serie de [58] (10 V, 10 Ω) con el
     *     tope de 5 iteraciones: también por gmin */
    d4_serie(10.0, 10.0, EL_DIODO, DIO_EXP, 10);
    cierra_red();
    chk_cond(q, "10 diodos: la red", sim_red_de(&rc, &rr, &s, &d) == SIM_OK);
    sim_nl_topes(5, 0);
    memset(&nw, 0, sizeof nw);
    e = sim_op_cc_red(&s, &v, &d, &nw);
    sim_nl_topes(0, 0);
    kc = rob_kcl(&s, &v, &ly);
    printf("    10 diodos: estado %d, homotop" S_ii "a %d; Kirchhoff del motor %.3g; el del arn" S_ee "s %.3g (ley %.3g)\n", e,
           nw.homotopia, nw.residuo_max, kc, ly);
    chk_cond(q, "10 diodos por gmin: la soluci" S_oo "n cumple Kirchhoff (el arn" S_ee "s)",
             e == SIM_OK && nw.homotopia == 1 && kc <= 1e-9 && ly <= 1e-9);
    chk_cond(q, "10 diodos por gmin: el Kirchhoff del motor <= 1e-9", e == SIM_OK && nw.residuo_max <= 1e-9);

    /* (c) en la pantalla: [P] (Newton+gmin) y [tab]: la fila de Kirchhoff
     *     con su marca (sin ella salía «Kirchhoff 9,9E-1» en rojo) */
    r4_serie(&c, 10.0, 10.0, 10, DIO_EXP);
    chk_cond(q, "10 diodos en el editor", r3u_abre(&c));
    sim_nl_topes(5, 0);
    TECLAS(T_LETRA('P'));
    sim_nl_topes(0, 0);
    chk_cond(q, "[P]: Newton+gmin", ui_pantalla() == P_RESULTADO && ve("Newton+gmin"));
    TECLAS(T_TAB);
    for (i = 0; i < 30 && ui_pantalla() == P_TABLA && !ve("Kirchhoff"); i++) TECLAS(T_ABAJO);
    for (i = 9; i <= 30 && !visto; i++) { sprintf(t, "E-%d " S_CHECK "\n", i); if (ve(t)) visto = i; }
    printf("    P_TABLA: la fila de Kirchhoff %s (E-%d con la marca)\n", ve("Kirchhoff") ? "se ve" : "no se ve", visto);
    chk_cond(q, "P_TABLA tras gmin: «Kirchhoff» <= 1e-9 con la marca", ui_pantalla() == P_TABLA && ve("Kirchhoff") && visto > 0);
    TECLAS(T_ESC, T_ESC);
}

/* ==========================================================================
 *  2. Una unión sin R sobre una E ideal: «El circuito no tiene una solución
 *     única (mira E2)», sin homotopías. La G de la unión pasa de 1e20 S y
 *     el pivote de la rama de la E queda bajo PIVOTE_NL: el §3.E pide gmin,
 *     la rampa y, si no, «no converge» con el culpable
 * ======================================================================= */
static void rob_union_e(void)
{
    static Valores v;
    SimDiag d;
    SimNewton nw;
    char m[300];
    int kQ, kQ2, e;
    const char *q = "R3 op_b uni" S_oo "n sobre una E";

    /* E1 12 V con RC 1 kΩ al colector de un NPN; E2 5 V directamente en su
     * base (el error de no poner RB); el emisor a tierra */
    c4_init(4);
    c4_el(EL_FUENTE, 0, 1, 12.0);
    c4_el(EL_FUENTE, 0, 2, 5.0);
    c4_el(EL_RES, 1, 3, 1e3);
    kQ = nuevo_el(EL_BJT, 3, 0);
    rr.c[kQ] = 2;
    cierra_red();
    memset(&nw, 0, sizeof nw);
    e = sim_punto_op_cc(&rc, &rr, &v, &d, &nw);
    sim_mensaje_circ(&rc, 0, &d, m, sizeof m);
    printf("    BJT con la base en E2: estado %d, elem %d, campo %d, homotop" S_ii "a %d, %ld iter.: %s\n", e, d.elem, d.campo,
           nw.homotopia, nw.iter, m);
    chk_cond(q, "la base del BJT en E2 (5 V): no converge (NC_OP), no SIM_SINGULAR", e == SIM_NO_CONVERGE && d.campo == NC_OP);
    chk_cond(q, "y el mensaje nombra al BJT (pon una R en serie), no a E2", strstr(m, rc.el[kQ].etiq) != 0 && !strstr(m, "E2"));

    /* con una segunda etapa (RB 10 kΩ del colector a la base de otro BJT,
     * su colector con otra RC a E1): el culpable sigue siendo el primero,
     * el de mayor G (el de mayor Δv relativo en la última iteración era el
     * segundo) */
    c4_el(EL_RES, 3, 4, 10e3);
    kQ2 = nuevo_el(EL_BJT, 5, 0);
    rr.c[kQ2] = 4;
    c4_el(EL_RES, 1, 5, 1e3);
    nodos = 6;
    cierra_red();
    memset(&nw, 0, sizeof nw);
    e = sim_punto_op_cc(&rc, &rr, &v, &d, &nw);
    sim_mensaje_circ(&rc, 0, &d, m, sizeof m);
    printf("    con una segunda etapa: estado %d, elem %d: %s\n", e, d.elem, m);
    chk_cond(q, "con una segunda etapa: el culpable es el BJT de la base en E2", e == SIM_NO_CONVERGE && d.elem == kQ);

    /* un diodo exponencial (Rs vacía) directamente sobre una E de 3 V */
    c4_init(2);
    c4_el(EL_FUENTE, 0, 1, 3.0);
    kQ = d4_d(EL_DIODO, 1, 0, DIO_EXP);
    cierra_red();
    memset(&nw, 0, sizeof nw);
    e = sim_punto_op_cc(&rc, &rr, &v, &d, &nw);
    sim_mensaje_circ(&rc, 0, &d, m, sizeof m);
    printf("    diodo exponencial sobre 3 V: estado %d, elem %d, homotop" S_ii "a %d: %s\n", e, d.elem, nw.homotopia, m);
    chk_cond(q, "D2 sobre E1 (3 V): no converge (NC_OP) y el mensaje nombra a D2",
             e == SIM_NO_CONVERGE && d.campo == NC_OP && strstr(m, rc.el[kQ].etiq) != 0 && !strstr(m, "E1"));

    /* La salida saturada de un AO también es una E ideal. E1 4,8 V; un
     * comparador (AO ideal, + en 3, - en 1) con R1 560 Ω de su salida a E1;
     * R2 790 Ω de E1 al emisor de un PNP (base a tierra, colector en la
     * salida del AO). La solución: el AO en -15 V y el PNP en activa. El
     * Newton directo pasa por el AO en +15 V, que deja 15 V directos en la
     * unión BC: LU singular. Antes no probaba la rampa (que lo resuelve) y
     * decía «no tiene una solución única».
     * mpmath: V3 = 0,74996495151750693 V, IC = 5,0874921033738256 mA */
    sr_init(4);
    a4_cc(0, 1, 4.8);
    sr_el(SE_R, 2, 1, 560.0);
    sr_el(SE_R, 3, 1, 790.0);
    kQ = sr_el(SE_AO, 3, 1, 0.0);
    sr.el[kQ].c = 2; sr.el[kQ].nref = 0;
    sr.el[kQ].modelo = NL_AO_IDEAL; sr.el[kQ].m[1] = 15.0; sr.el[kQ].m[2] = -15.0;
    kQ2 = q4_bjt(2, 0, 3, -1, 100.0, 50.0);
    memset(&nw, 0, sizeof nw);
    e = sim_op_cc_red(&sr, &v, &d, &nw);
    printf("    comparador y PNP: estado %d, homotop" S_ii "a %d, V3 = %.15g V, IC = %.12g A, Vsal = %.6g V\n", e, nw.homotopia,
           v.phi[3], -v.I[kQ2], v.phi[2]);
    chk_cond(q, "comparador y PNP: converge (el AO en -15 V, el PNP en activa)",
             e == SIM_OK && v.phi[2] == -15.0 && v.reg[kQ] == REG_SAT_MENOS && v.reg[kQ2] == REG_ACTIVA);
    chk_cond(q, "comparador y PNP: V3 e IC los de mpmath (1e-12 relativo)", e == SIM_OK &&
             fabs(v.phi[3] - 0.74996495151750693) < 1e-12 * 0.75 && fabs(-v.I[kQ2] - 5.0874921033738256e-3) < 1e-12 * 5.09e-3);
}

/* ==========================================================================
 *  3. AO real con Ro > 0: «Kirchhoff 2,1E-8» en rojo con la solución exacta
 * ======================================================================= */
static void rob_ro(void)
{
    static Valores v;
    SimDiag d;
    SimNewton nw;
    int k, e;
    const char *q = "R3 op_b Ro";

    /* puente de 10 kΩ con 20 V (las dos mitades en 10 V); restador con R de
     * 100 kΩ y 1 MΩ; U1 real (A = 1e5, Ro = 75 Ω) y RL de 1 MΩ. mpmath:
     * Vsal = 7,1421054139007379e-8 V, V- = 9,0497737624427246 V y V+ =
     * 9,0497737556561086 V */
    sr_init(7);
    a4_cc(0, 1, 20.0);
    sr_el(SE_R, 1, 2, 10e3); sr_el(SE_R, 2, 0, 10e3);
    sr_el(SE_R, 1, 3, 10e3); sr_el(SE_R, 3, 0, 10e3);
    sr_el(SE_R, 2, 4, 100e3); sr_el(SE_R, 4, 6, 1e6);
    sr_el(SE_R, 3, 5, 100e3); sr_el(SE_R, 5, 0, 1e6);
    k = sr_el(SE_AO, 5, 4, 0.0);
    sr.el[k].c = 6; sr.el[k].nref = 0;
    sr.el[k].modelo = NL_AO_REAL; sr.el[k].m[0] = 1e5; sr.el[k].m[1] = 15.0; sr.el[k].m[2] = -15.0; sr.el[k].m[4] = 75.0;
    sr_el(SE_R, 6, 0, 1e6);
    memset(&nw, 0, sizeof nw);
    e = sim_op_cc_red(&sr, &v, &d, &nw);
    printf("    restador con Ro = 75: estado %d, Vsal = %.10g V, V- = %.15g, V+ = %.15g; Kirchhoff del motor %.3g\n", e, v.phi[6],
           v.phi[4], v.phi[5], nw.residuo_max);
    chk_cond(q, "la soluci" S_oo "n es la exacta: Vsal = 7,1421054e-8 V (1e-6 relativo), V- y V+ a 1e-13",
             e == SIM_OK && fabs(v.phi[6] - 7.1421054139007379e-8) < 1e-6 * 7.14e-8 &&
             fabs(v.phi[4] - 9.0497737624427246) < 1e-13 && fabs(v.phi[5] - 9.0497737556561086) < 1e-13);
    chk_cond(q, "y su «Kirchhoff» <= 1e-9 (la marca)", e == SIM_OK && nw.residuo_max <= 1e-9);
}

static void test_rev3_op_b(void)
{
    printf("[65q] Etapa 4: el punto de operaci" S_oo "n (tercera revisi" S_oo "n, grupo op_b)\n");
    rob_gmin();
    rob_union_e();
    rob_ro();
}
