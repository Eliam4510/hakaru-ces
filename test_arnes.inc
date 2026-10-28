/* --------------------------------------------------------------------------
 *  12a. El residuo de Kirchhoff del mono: su acumulador y su métrica
 *
 *  Se incluye al final de host_test.c (usa sr, sr_init, sr_el, a4_ao, b4_ch
 *  y los chk).
 *
 *  El mono comprueba que TODAS las soluciones de Newton de la sesión cumplen
 *  Kirchhoff (sim_residuo_acum <= 1e-9). Dos cosas lo estropeaban:
 *    - el barrido ponía a 0 ese mismo acumulador al empezar (para su
 *      kcl_peor), así que el mono solo veía desde el último barrido. Ahora el
 *      barrido lleva el suyo (sim_residuo_barr);
 *    - con eso, el mono daba «Kirchhoff roto» con redondeo: un nodo sin
 *      corriente cuyo potencial sale de restar otros (φ = φ1 - E) lleva un
 *      ulp de esos (1,1e-16 V con E = 0,99 V), y la métrica del motor lo
 *      mide contra 1e-9·Iesc. El mono mide con el suelo de la escala de la
 *      red (sim.c, residuo_mono); el motor, igual que siempre.
 *  Las dos redes son las que encontró el mono (semilla 11: en la tecla
 *  51 770, 1,1e-7; con ráfagas, 3,7e-9), en el punto 279 de 281 del barrido
 *  de -1 a 1 V (E = 139/140 V). La de verdad: el modelo 25 (la media onda)
 *  sin la iteración de más del paso (el gancho NLP_SIN_PULIDO) deja el
 *  residuo que la motivó, y el mono lo sigue viendo.
 * ----------------------------------------------------------------------- */

/* 1. E ideal (a 1, b 0) con 10 Ω del 0 a tierra (el 2); S abierto del 1 a
 *    tierra y un diodo ideal con un borne suelto: por la R no pasa nada, y
 *    el nodo 0 sale a un ulp de 0 */
static void ra_r_sin_corriente(void)
{
    int k;
    sr_init(4);
    sr.ref[0] = 0; sr.ref[2] = 1;
    k = sr_el(SE_E, 1, 0, 0.0);
    sr.el[k].forma = ONDA_CC; sr.el[k].p[0] = 1.0;
    sr_el(SE_R, 0, 2, 10.0);
    k = sr_el(SE_S, 1, 2, 0.0); sr.el[k].cerrado0 = 0;
    k = sr_el(SE_D, 3, 2, 0.0);
    sr.el[k].modelo = NL_TRAMOS; sr.el[k].m[1] = 1e-3; sr.el[k].m[3] = 1e-3;
}

/* 2. E (a 3, b 2) con 10 kΩ del 2 a tierra; el 3 va solo a la entrada + de
 *    un AO ideal (saturado; en su salida, un C que en CC no cuenta) */
static void ra_ao_saturado(void)
{
    int k;
    sr_init(4);
    k = sr_el(SE_E, 3, 2, 0.0);
    sr.el[k].forma = ONDA_CC; sr.el[k].p[0] = 1.0;
    sr_el(SE_R, 2, 0, 10e3);
    a4_ao(3, 0, 1, 0, 0, 0.0);
    sr_el(SE_C, 0, 1, 1e-7);
}

/* El barrido de la E (el elemento 0) de -1 a 1 V; SIM_OK si se barre entero */
static int ra_barre(void)
{
    int e;
    memset(b4_ch, 0, sizeof b4_ch);
    e = barr_iniciar_red(&sr, 0, BX_E, b4_ch, -1.0, 1.0, 0, 0);
    if (e == SIM_OK) while (barr_avanzar(~0ul)) {}
    return e == SIM_OK && barr_res()->fin == FIN_COMPLETO && barr_res()->huecos == 0 ? SIM_OK : SIM_NO_CONVERGE;
}

/* El modelo 25 (el rectificador de media onda) emulado entero; con el gancho
 * que se pida. El residuo del motor (el mayor de la emulación) */
static double ra_media_onda(const char *q, int gancho)
{
    static Circuito c;
    static Red r;
    int i, e;
    for (i = 0; i < N_MODELOS; i++) if (strcmp(MODELOS[i].titulo, "Rectificador de media onda") == 0) break;
    pruebas++;
    if (i >= N_MODELOS) { falla(q, "el modelo de la media onda", 0, 1); return 0.0; }
    modelo_cargar(i, &c);
    red_extraer(&c, &r);
    sim_nl_prueba(gancho, 0.0);
    e = sim_iniciar(&c, &r);
    if (e == SIM_OK) while (sim_avanzar(SIM_OPS_TICK)) ;
    sim_nl_prueba(NLP_NADA, 0.0);
    pruebas++;
    if (e != SIM_OK || sim_res()->fin != FIN_COMPLETO) falla(q, "se emula entero", e, SIM_OK);
    return sim_res()->nw.residuo_max;
}

static void test_residuo_mono(void)
{
    const char *q;
    double motor, mono, r0, a0, r1, a1;
    int e;

    printf("[12a] El residuo de Kirchhoff del mono: su acumulador y su m" S_ee "trica\n");

    /* Las dos redes del mono: el motor ve el ulp; el mono, no */
    q = "residuo del mono: la R sin corriente";
    ra_r_sin_corriente();
    sim_residuo_acum_reset();
    e = ra_barre();
    motor = barr_res()->kcl_peor; mono = sim_residuo_acum();
    printf("    %s (barrido de -1 a 1 V): el del motor %.3g, el del mono %.3g\n", q, motor, mono);
    chk_cond(q, "se barre entero", e == SIM_OK);
    chk_cond(q, "el motor ve el redondeo: pasa de 1e-9", motor > 1e-9);
    chk_cond(q, "el mono no: <= 1e-9", mono <= 1e-9);

    q = "residuo del mono: el AO saturado";
    ra_ao_saturado();
    sim_residuo_acum_reset();
    e = ra_barre();
    motor = barr_res()->kcl_peor; mono = sim_residuo_acum();
    printf("    %s (barrido de -1 a 1 V): el del motor %.3g, el del mono %.3g\n", q, motor, mono);
    chk_cond(q, "se barre entero", e == SIM_OK);
    chk_cond(q, "el motor ve el redondeo: pasa de 1e-9", motor > 1e-9);
    chk_cond(q, "el mono no: <= 1e-9", mono <= 1e-9);

    /* Uno de verdad: sin el pulido, el mono lo ve como el motor */
    q = "residuo del mono: uno de verdad";
    sim_residuo_acum_reset();
    r0 = ra_media_onda(q, NLP_NADA);
    a0 = sim_residuo_acum();
    sim_residuo_acum_reset();
    r1 = ra_media_onda(q, NLP_SIN_PULIDO);
    a1 = sim_residuo_acum();
    printf("    %s (el modelo 25): con el pulido, el del motor %.3g y el del mono %.3g;"
           " sin " S_ee "l, %.3g y %.3g\n", q, r0, a0, r1, a1);
    chk_cond(q, "con el pulido, los dos <= 1e-9", r0 <= 1e-9 && a0 <= 1e-9);
    chk_cond(q, "sin el pulido, el del motor pasa de 1e-9", r1 > 1e-9);
    chk_cond(q, "y el del mono tambi" S_ee "n, del mismo orden (> la mitad)", a1 > 1e-9 && a1 > 0.5 * r1);

    /* El barrido ya no pone a 0 el del mono (lo de antes sigue) y su
     * kcl_peor es solo suyo */
    q = "residuo del mono: el barrido no lo borra";
    ra_ao_saturado();
    e = ra_barre();
    printf("    %s: tras el modelo 25 sin pulido, %.3g; tras un barrido, %.3g (su kcl_peor, %.3g)\n",
           q, a1, sim_residuo_acum(), barr_res()->kcl_peor);
    chk_cond(q, "el del mono sigue igual tras el barrido", e == SIM_OK && a1 > 1e-9 && sim_residuo_acum() == a1);
    chk_cond(q, "el kcl_peor del barrido es el suyo (el del AO: 3,7e-9)",
             fabs(barr_res()->kcl_peor - motor) <= 1e-3 * motor);
    sim_residuo_acum_reset();
}

/* --------------------------------------------------------------------------
 *  67b. La red de CC rechazada_cc_099 de [67]: la causa
 *
 *  El oráculo le halla una solución y el motor da NO_CONVERGE («Newton se
 *  atasca en D3»: el D11 del .net, porque sin circuito nombre_el numera con
 *  1 + k % 9). La causa: D11 (Is 4,03e-9, n 1,084) va SIN R en serie
 *  directamente sobre la E ideal de 16 V. Con u = vd/(n·VT) = 571, muy por
 *  encima de 80, la e^u del modelo sigue en su recta (D4): 1,1e29 A, con una
 *  G de 7,9e27 S. En cuanto la G linealizada pasa de ~1e20 S, el pivote de
 *  la rama de la fuente (1/G) queda bajo PIVOTE_NL de su columna y la LU da
 *  singular: en el Newton directo, en cada escalón de gmin y en la rampa a
 *  partir de 1,73 V. Con el umbral de pivote a 0 (una copia de depuración)
 *  el motor sí llega por gmin a la solución del oráculo (I(V0) = 1,0970183e29
 *  A, las mismas cifras), pero esa solución solo existe en la extensión
 *  numérica de e^u, no es física, y aflojar el pivote quitaría la única
 *  guarda contra las matrices singulares de verdad (el AO sin lazo). Se deja
 *  así, con el culpable y el consejo («pon una R en serie»); esta prueba
 *  fija la causa con la red mínima.
 * ----------------------------------------------------------------------- */
static int r099_red(double E, double Rs)
{
    static const NlDiodo D099 = { "D11 de la 099", 4.0279725794013491e-09, 1.0842116475105286, 0.0 };
    NlDiodo d = D099;
    int k;
    d.Rs = Rs;
    sr_init(2);
    a4_cc(0, 1, E);
    k = t4_d_cat(1, 0, &d);
    return k;
}

static void test_red_099(void)
{
    const char *q = "la red 099";
    static Valores v;
    SimDiag d;
    SimNewton nw;
    int k, e;

    printf("[67b] La red 099 de [67]: una uni" S_oo "n sin R sobre la E ideal\n");
    k = r099_red(16.0, 0.0);
    e = sim_op_cc_red(&sr, &v, &d, &nw);
    printf("    16 V sin R: estado %d, culpable %d (el diodo, %d), campo %d\n", e, d.elem, k, d.campo);
    chk_cond(q, "16 V sobre la uni" S_oo "n sin R: NO_CONVERGE con el diodo de culpable (no el tope)",
             e == SIM_NO_CONVERGE && d.elem == k && d.campo == NC_OP);
    k = r099_red(1.0, 0.0);
    e = sim_op_cc_red(&sr, &v, &d, &nw);
    printf("    1 V sin R: estado %d, I %.6g A\n", e, e == SIM_OK ? v.I[k] : 0.0);
    chk_cond(q, "1 V sin R (e^u en su curva): converge", e == SIM_OK && v.I[k] > 0.0);
    k = r099_red(16.0, 1.0);
    e = sim_op_cc_red(&sr, &v, &d, &nw);
    printf("    16 V con 1 ohm en serie: estado %d, I %.6g A\n", e, e == SIM_OK ? v.I[k] : 0.0);
    chk_cond(q, "16 V con la R del consejo (1 " S_OHM "): converge, 15 A y pico",
             e == SIM_OK && v.I[k] > 14.0 && v.I[k] < 16.0);
}
