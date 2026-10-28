/* ============================================================================
 *  test_opt_cal.inc  -  [75e] El modelo de tiempos con el banco sin el USB
 *
 *  El banco que calibró el cierre iba con el cable USB (la CX II a 288 MHz):
 *  las constantes salen ahora del banco sin él (2026-09-27, a 396 MHz; ver
 *  test_calibracion). Aquí, lo que el modelo hacía mal:
 *    K1  el costo a priori de un paso: la ventana de medida (16 operaciones
 *        por elemento) y el valor de las fuentes (dos por paso; un seno, su
 *        sin() de SIM_OPS_SIN y 4): las redes lineales salían ~17 % cortas
 *    K2  el control del gasto: el puente del banco con 1 s pedido y el paso
 *        automático tardaba ~7,4 s con 1,96 estimados (Newton parte el paso
 *        e itera de más); ahora, a los dos segundos (rev9, integración: la
 *        primera mirada no corta), la ventana se acorta a la que cabe en
 *        3 s. No depende del troceo de sim_avanzar (bit a bit)
 *    K3  con el paso a mano no hay tope: solo se corrige lo estimado (con la
 *        última mirada, no la mayor)
 *    K4  el tope de un OP cuenta lo fijo de sus iteraciones: ~0,95 s
 *    K5  la malla del barrido: las operaciones de un punto
 *    K6  la calculadora de CA: su tope, ~3,2 s con CA_NS_OP
 *    K7  lo estimado frente a lo real en la carga de BN_SIM (con su paso)
 *    K8  con solo tramos, el residuo de la iteración de cada paso
 *    K9  un tramo caro que llega tarde: se sigue mirando cada segundo, y al
 *        acortar ya dentro de la ventana de medida, sus integrales de antes
 *        vuelven a cero
 *  «Lo real» es el modelo calibrado con las cuentas de verdad: operaciones·
 *  SIM_NS_OP + pasos·SIM_NS_PASO + iteraciones (con las de la presonda)·
 *  NS_ITER; el de un OP o un barrido, sin los pasos. HAKARU_SOLO_CAL lo corre
 *  solo; va al final de la batería (no le cambia nada a nadie).
 *  Se incluye tras test_cierre.inc (usa sr_*, canal, emula, a4_cc, t4_*,
 *  r099_red, NL_1N4148).
 * ========================================================================= */

/* Lo real de la última emulación (sim_ops puesto a cero antes de iniciarla) */
static double kc_real(void)
{
    const SimRes *r = sim_res();
    return ((double)sim_ops() * SIM_NS_OP + (double)r->pasos * SIM_NS_PASO + (double)(r->nw.iter + r->nw.sonda) * NS_ITER) * 1e-9;
}

/* El puente del banco con la ventana t_fin (0 = automática) y el paso
 * automático, emulado con sim_avanzar(trozo); deja sim_ops desde el arranque */
static void kc_puente(double t_fin, unsigned long trozo)
{
    static Circuito c;
    static Red r;
    banco_puente(&c, &r);
    if (t_fin > 0.0) { c.t_fin.v = t_fin; c.t_fin.est = M_DATO; }
    sim_ops_reset();
    sim_iniciar(&c, &r);
    while (sim_avanzar(trozo)) {}
}

/* La huella del registro y de las medidas de la última emulación */
static unsigned long long kc_huella(void)
{
    const SimReg *g = sim_reg();
    const SimRes *r = sim_res();
    unsigned long long h = 1469598103934665603ULL;
    const unsigned char *b;
    size_t i, n;
    b = (const unsigned char *)g->t; n = sizeof(double) * (size_t)g->n;
    for (i = 0; i < n; i++) { h ^= b[i]; h *= 1099511628211ULL; }
    b = (const unsigned char *)g->y[0]; for (i = 0; i < n; i++) { h ^= b[i]; h *= 1099511628211ULL; }
    b = (const unsigned char *)r->W; n = sizeof r->W;
    for (i = 0; i < n; i++) { h ^= b[i]; h *= 1099511628211ULL; }
    b = (const unsigned char *)r->Vmed; n = sizeof r->Vmed;
    for (i = 0; i < n; i++) { h ^= b[i]; h *= 1099511628211ULL; }
    return h;
}

static void test_opt_cal(void)
{
    char det[200];
    const SimRes *r;
    double real, t_est;

    printf("[75e] El modelo de tiempos con el banco sin el USB\n");

    /* ---- K1. Una red lineal: lo estimado es lo real (a ±10 %) ---- */
    {
        const char *q = "K1 costo de un paso";
        int kE, kC, kL;
        /* RLC con una senoidal de 1 kHz (la ventana, periodos al final) */
        sr_init(4);
        kE = sr_el(SE_E, 0, 1, 0.0);
        sr.el[kE].forma = ONDA_SENO; sr.el[kE].p[0] = 1.0; sr.el[kE].p[1] = 1e3;
        sr_el(SE_R, 1, 2, 20.0); kL = sr_el(SE_L, 2, 3, 10e-3); kC = sr_el(SE_C, 3, 0, 1e-6);
        canal(0, CANAL_V, kC); canal(1, CANAL_I, kL);
        sim_ops_reset();
        emula(q);
        r = sim_res(); real = kc_real();
        printf("    RLC con senoidal: %ld pasos, h %.4g, t_fin %.4g, ventana %.2f, estimado %.3f s, real %.3f s\n",
               r->pasos, r->h, r->t_fin, (r->t_fin - r->t_med0) / r->t_fin, r->t_est, real);
        sprintf(det, "RLC con senoidal: lo real entre 0,9 y 1,1 veces lo estimado (%.3f y %.3f s)", real, r->t_est);
        chk_cond(q, det, real >= 0.9 * r->t_est && real <= 1.1 * r->t_est);
        /* la escalera RC del arnés, 10 elementos, con un escalón */
        sr_init(6);
        sr_escalon(0, 1, 1.0);
        for (kE = 1; kE < 5; kE++) { sr_el(SE_R, kE, kE + 1, 1e3); sr_el(SE_C, kE + 1, 0, 1e-7); }
        canal(0, CANAL_NODO, 5); canal(1, CANAL_NADA, 0);
        sim_ops_reset();
        emula(q);
        r = sim_res(); real = kc_real();
        printf("    escalera RC: %ld pasos, h %.4g, t_fin %.4g, estimado %.3f s, real %.3f s\n", r->pasos, r->h, r->t_fin, r->t_est, real);
        sprintf(det, "escalera RC: lo real entre 0,9 y 1,1 veces lo estimado (%.3f y %.3f s)", real, r->t_est);
        chk_cond(q, det, real >= 0.9 * r->t_est && real <= 1.1 * r->t_est);
    }

    /* ---- K2. El control del gasto: el puente del banco, 1 s pedido ---- */
    {
        const char *q = "K2 control del gasto";
        unsigned long long h1;
        double tf1, t1;
        long p1;
        kc_puente(1.0, ~0ul);
        r = sim_res(); real = kc_real();
        printf("    puente del banco, 1 s pedido, paso autom" S_aa "tico: t_fin %.4g, h %.4g, %ld pasos, %ld iteraciones, estimado %.2f s, real %.2f s\n",
               r->t_fin, r->h, r->pasos, r->nw.iter, r->t_est, real);
        chk_cond(q, "acaba, con la ventana acortada (PASO_LIM) y la medida al final de ella",
                 r->fin == FIN_COMPLETO && r->t_fin < 1.0 && r->t_pedido == 1.0 && (r->d.avisos & SIM_AV_PASO_LIM) &&
                 r->t_med0 > 0.0 && r->t_med0 < r->t_fin);
        sprintf(det, "lo real, %.2f s: ~3 s o menos (sin el control, ~7,4 con 1,96 estimados; el corte apunta a 3 s)", real);
        chk_cond(q, det, real <= 3.1);
        sprintf(det, "lo estimado tras acortar (%.2f s) no promete de m" S_aa "s: lo real, como mucho 1,1 veces", r->t_est);
        chk_cond(q, det, real <= 1.1 * r->t_est);
        /* no depende del troceo: de un paso en un paso, por ticks, de golpe */
        h1 = kc_huella(); tf1 = r->t_fin; p1 = r->pasos; t1 = r->t_est;
        kc_puente(1.0, 1ul);
        chk_cond(q, "sim_avanzar(1): la misma ventana, los mismos pasos y el mismo registro (bit a bit)",
                 sim_res()->t_fin == tf1 && sim_res()->pasos == p1 && sim_res()->t_est == t1 && kc_huella() == h1);
        kc_puente(1.0, SIM_OPS_TICK);
        chk_cond(q, "sim_avanzar(SIM_OPS_TICK): lo mismo (bit a bit)",
                 sim_res()->t_fin == tf1 && sim_res()->pasos == p1 && sim_res()->t_est == t1 && kc_huella() == h1);
        /* la ventana automática (5·τ del filtro, 0,5 s) */
        kc_puente(0.0, ~0ul);
        r = sim_res(); real = kc_real();
        printf("    puente del banco, todo autom" S_aa "tico: t_fin %.4g, h %.4g, %ld pasos, estimado %.2f s, real %.2f s\n",
               r->t_fin, r->h, r->pasos, r->t_est, real);
        sprintf(det, "la ventana autom" S_aa "tica: lo real, %.2f s, ~3 s o menos (sin el control, ~4,5)", real);
        chk_cond(q, det, r->fin == FIN_COMPLETO && real <= 3.1);
    }

    /* ---- K3. Con el paso a mano: la carga de BN_NL entera ---- */
    {
        const char *q = "K3 paso a mano";
        static Circuito c;
        static Red rd;
        double t0;
        banco_puente(&c, &rd);
        c.t_fin.v = 1.0; c.t_fin.est = M_DATO;
        c.paso.v = (1.0 / 60.0) / 45.0; c.paso.est = M_DATO;
        sim_ops_reset();
        sim_iniciar(&c, &rd);
        t0 = sim_res()->t_est;
        while (sim_avanzar(~0ul)) {}
        r = sim_res(); real = kc_real();
        printf("    puente, 1 s con h = T/45 a mano (la carga de BN_NL): %ld pasos, %ld iteraciones, estimado %.2f s (a priori %.2f), real %.2f s\n",
               r->pasos, r->nw.iter, r->t_est, t0, real);
        chk_cond(q, "la ventana y el paso pedidos (sin tope que los cambie)", r->fin == FIN_COMPLETO && r->t_fin == 1.0 &&
                 fabs(r->h - c.paso.v) <= 1e-12 * c.paso.v);
        sprintf(det, "lo estimado se corrige cada segundo: lo real (%.2f s), como mucho 1,1 veces (a priori, %.2f)", real, t0);
        chk_cond(q, det, real <= 1.1 * r->t_est && t0 < r->t_est);
        sprintf(det, "y lo estimado (%.2f s), como mucho 1,1 veces lo real: vale la " S_uu "ltima mirada", r->t_est);
        chk_cond(q, det, r->t_est <= 1.1 * real);
    }

    /* ---- K4. El tope de un OP: con lo fijo de sus iteraciones, ~0,95 s.
     *      Con el de antes (1,5 M operaciones solas) el corte más largo de la
     *      batería (1224 iteraciones) iba a 0,96 s, pero una red pequeña, de
     *      ~100 operaciones por iteración, habría llegado a ~4 s ---- */
    {
        const char *q = "K4 tope del OP";
        static Valores v;
        SimDiag d;
        SimNewton nw;
        unsigned long o;
        double e = (double)NL_OPS_OP * SIM_NS_OP * 1e-9;
        sprintf(det, "NL_OPS_OP: %.3f s, entre 0,9 y 1; una iteraci" S_oo "n cuenta NS_ITER (%.1f " S_MU "s en el tope)",
                e, (double)(unsigned long)(NS_ITER / SIM_NS_OP + 0.5) * SIM_NS_OP * 1e-3);
        chk_cond(q, det, e >= 0.9 && e <= 1.0);
        /* un OP que converge (la unión sin R a 1 V) con su tope justo: sus
         * operaciones solas no bastan, sus operaciones y sus iteraciones sí */
        r099_red(1.0, 0.0);
        memset(&nw, 0, sizeof nw);
        sim_ops_reset();
        chk_cond(q, "la uni" S_oo "n sin R a 1 V converge", sim_op_cc_red(&sr, &v, &d, &nw) == SIM_OK && nw.iter >= 3);
        o = sim_ops();
        printf("    OP de la uni" S_oo "n a 1 V: %ld iteraciones, %lu operaciones, real %.2f ms (%.2f de sus operaciones)\n",
               nw.iter, o, ((double)o * SIM_NS_OP + (double)nw.iter * NS_ITER) * 1e-6, (double)o * SIM_NS_OP * 1e-6);
        sim_nl_tope_ops(o);
        chk_cond(q, "con el tope en sus operaciones solas: NC_TOPE (lo fijo de las iteraciones cuenta)",
                 sim_op_cc_red(&sr, &v, &d, 0) == SIM_NO_CONVERGE && d.campo == NC_TOPE);
        sim_nl_tope_ops(o + (unsigned long)nw.iter * (unsigned long)(NS_ITER / SIM_NS_OP + 0.5) + 100ul);
        chk_cond(q, "con sus operaciones y 463 por iteraci" S_oo "n: converge", sim_op_cc_red(&sr, &v, &d, 0) == SIM_OK);
        sim_nl_tope_ops(0);
    }

    /* ---- K5. La malla del barrido: lo estimado frente a lo real ---- */
    {
        const char *q = "K5 barrido";
        SimCanal ch[2];
        unsigned long o;
        long it;
        int j, kk;
        /* el diodo de [14] */
        sr_init(3);
        kk = a4_cc(0, 1, 1.0); sr_el(SE_R, 1, 2, 1e3); j = t4_d_cat(2, 0, &NL_1N4148);
        memset(ch, 0, sizeof ch);
        ch[0].tipo = CANAL_I; ch[0].elem = (signed char)j; ch[0].sg = 1; ch[1].tipo = CANAL_NADA;
        o = barr_ops();
        barr_iniciar_red(&sr, kk, BX_E, ch, 0.0, 5.0, 0, 0);
        while (barr_avanzar(~0ul)) {}
        o = barr_ops() - o;
        for (it = 0, j = 0; j < barr_reg()->n; j++) it += barr_reg()->it[0][j];
        real = ((double)o * SIM_NS_OP + (double)it * NS_ITER) * 1e-9;
        printf("    barrido del diodo: %d puntos, estimado %.3f s, real %.3f s\n", barr_reg()->n, barr_res()->t_est, real);
        sprintf(det, "el diodo: lo real entre 0,8 y 1,25 veces lo estimado (%.3f y %.3f s)", real, barr_res()->t_est);
        chk_cond(q, det, real >= 0.8 * barr_res()->t_est && real <= 1.25 * barr_res()->t_est);
        /* un divisor lineal: sin iteraciones, cada punto como el primero
         * (antes se contaba la mitad) */
        sr_init(3);
        kk = a4_cc(0, 1, 1.0); sr_el(SE_R, 1, 2, 1e3); j = sr_el(SE_R, 2, 0, 1e3);
        memset(ch, 0, sizeof ch);
        ch[0].tipo = CANAL_I; ch[0].elem = (signed char)j; ch[0].sg = 1; ch[1].tipo = CANAL_NADA;
        o = barr_ops();
        barr_iniciar_red(&sr, kk, BX_E, ch, 0.0, 5.0, 0, 0);
        while (barr_avanzar(~0ul)) {}
        o = barr_ops() - o;
        real = (double)o * SIM_NS_OP * 1e-9;
        printf("    barrido de un divisor: %d puntos, estimado %.4f s, real %.4f s\n", barr_reg()->n, barr_res()->t_est, real);
        sprintf(det, "el divisor: lo real entre 0,9 y 1,1 veces lo estimado (%.4f y %.4f s)", real, barr_res()->t_est);
        chk_cond(q, det, real >= 0.9 * barr_res()->t_est && real <= 1.1 * barr_res()->t_est);
    }

    /* ---- K6. La calculadora de CA: su tope en tiempo ---- */
    {
        const char *q = "K6 calculadora de CA";
        t_est = (double)CAL_OPS_TOPE * CA_NS_OP * 1e-9;
        sprintf(det, "CAL_OPS_TOPE, todas de CA (las m" S_aa "s caras): %.2f s, entre 3 y 3,3", t_est);
        chk_cond(q, det, t_est >= 3.0 && t_est <= 3.3);
    }

    /* ---- K7. Las cargas del banco con su paso: lo estimado frente a lo real ---- */
    {
        const char *q = "K7 banco";
        static Circuito c;
        static Red rd;
        banco_peor_sim(&c, &rd);
        c.t_fin.v = 2e-3; c.t_fin.est = M_DATO; c.paso.v = 2e-6; c.paso.est = M_DATO;
        sim_ops_reset();
        sim_iniciar(&c, &rd);
        while (sim_avanzar(~0ul)) {}
        r = sim_res(); real = kc_real();
        printf("    BN_SIM (el peor emulador, 2 ms con h = 2 us): %ld pasos, estimado %.3f s, real %.3f s\n", r->pasos, r->t_est, real);
        sprintf(det, "BN_SIM: lo real entre 0,9 y 1,1 veces lo estimado (%.3f y %.3f s)", real, r->t_est);
        chk_cond(q, det, real >= 0.9 * r->t_est && real <= 1.1 * r->t_est);
    }

    /* ---- K8. Con solo tramos, cada paso acaba su iteración con el residuo
     *      (residuo_de, 6 operaciones por elemento): sin contarlo, lo real
     *      salía ~10 % largo de mediana en las redes con tramos (aquí, 1,08 y
     *      1,10 veces; con él, 1,05 y 1,06: lo que queda son las
     *      conmutaciones del diodo, que elige_paso no ve) ---- */
    {
        const char *q = "K8 tramos";
        int kC, kD;
        /* el recorte: senoidal de 10 V y 1 kHz, 1 k y un diodo de 0,7 V a tierra */
        sr_init(3);
        t4_seno(0, 1, 10.0, 1e3, 0.0); sr_el(SE_R, 1, 2, 1e3); kD = t4_d_vg(2, 0, 0.7);
        canal(0, CANAL_V, kD); canal(1, CANAL_NADA, 0);
        sim_ops_reset();
        emula(q);
        r = sim_res(); real = kc_real();
        printf("    recorte con un diodo por tramos: %ld pasos, %ld iteraciones, estimado %.3f s, real %.3f s\n",
               r->pasos, r->nw.iter, r->t_est, real);
        sprintf(det, "el recorte: lo real entre 0,95 y 1,07 veces lo estimado (%.3f y %.3f s)", real, r->t_est);
        chk_cond(q, det, real >= 0.95 * r->t_est && real <= 1.07 * r->t_est);
        /* media onda con filtro: 60 Hz, 100 µF y 1 k, 0,1 s con el paso automático */
        sr_init(3);
        t4_seno(0, 1, 10.0, 60.0, 0.0); t4_d_vg(1, 2, 0.7); kC = sr_el(SE_C, 2, 0, 100e-6); sr_el(SE_R, 2, 0, 1e3);
        canal(0, CANAL_V, kC); canal(1, CANAL_NADA, 0);
        saj.t_fin = 0.1;
        sim_ops_reset();
        emula(q);
        r = sim_res(); real = kc_real();
        printf("    media onda con filtro por tramos: %ld pasos, %ld iteraciones, estimado %.3f s, real %.3f s\n",
               r->pasos, r->nw.iter, r->t_est, real);
        sprintf(det, "la media onda: lo real entre 0,95 y 1,07 veces lo estimado (%.3f y %.3f s)", real, r->t_est);
        chk_cond(q, det, real >= 0.95 * r->t_est && real <= 1.07 * r->t_est);
    }

    /* ---- K9. Un tramo caro que llega tarde: un S conecta en 0,155 s (de 0,3
     *      pedidos, con el paso automático) una senoidal de 60 V y 1 kHz a una
     *      media onda con un 1N4148, 100 ohm y 10 µF. La ventana de medida
     *      empezaba en 0,15 s. La primera mirada (0,161 s) no corta (rev9,
     *      integración: su ritmo puede ser el del arranque); en la segunda el
     *      ritmo ya es ~75 veces el de antes (y el de todo el segundo, también
     *      de sobra) y la ventana se acorta a 0,189 s (entre lo emulado y lo
     *      que cabe en 3 s no hay una 1-2-5: la rejilla del 1 % de la escala,
     *      0,001 s; la revisión final), con la medida desde después de lo
     *      emulado; lo que sus integrales llevaban desde 0,15 s vuelve a cero.
     *      t_est acaba en lo que cuesta de verdad (~3 s: el corte apunta a
     *      GC_DISPARA al menor de los dos ritmos) ---- */
    {
        const char *q = "K9 caro tarde";
        const SimReg *g;
        unsigned long long h1;
        double v_reg, tf1, tm1, te1;
        long p1;
        int kC, ks, i, i0 = -1, trozo;
        for (trozo = 0; trozo < 2; trozo++) {
            sr_init(4);
            t4_seno(0, 1, 60.0, 1e3, 0.0);
            ks = sr_el(SE_S, 1, 2, 0.0); sr.el[ks].t1 = 0.155;
            sr_el(SE_R, 2, 0, 1e6); t4_d_cat(2, 3, &NL_1N4148);
            sr_el(SE_R, 3, 0, 100.0); kC = sr_el(SE_C, 3, 0, 10e-6);
            canal(0, CANAL_V, kC); canal(1, CANAL_NADA, 0);
            saj.t_fin = 0.3;
            sim_ops_reset();
            if (sim_iniciar_red(&sr, &saj) != SIM_OK) { chk_cond(q, "arranca", 0); return; }
            while (sim_avanzar(trozo ? 1ul : ~0ul)) {}
            r = sim_res();
            if (trozo) break;
            real = kc_real();
            g = sim_reg();
            for (i = 0; i < g->n; i++) if (g->t[i] >= r->t_med0 - 1e-12) { i0 = i; break; }
            v_reg = i0 >= 0 && g->t[g->n - 1] > g->t[i0] ? (g->s1[0][g->n - 1] - g->s1[0][i0]) / (g->t[g->n - 1] - g->t[i0]) : 0.0;
            printf("    un S conecta en 0,155 s (0,3 pedidos): t_fin %.4g, medida desde %.4g, %ld pasos, estimado %.2f s, real %.2f s;"
                   " V(C) media %.9g (por el registro %.9g)\n", r->t_fin, r->t_med0, r->pasos, r->t_est, real, r->Vmed[kC], v_reg);
            chk_cond(q, "acaba, con la ventana acortada (PASO_LIM) a 0,189 s y la medida tras lo emulado en la segunda mirada (~3 s)",
                     r->fin == FIN_COMPLETO && r->t_pedido == 0.3 && fabs(r->t_fin - 0.189) < 1e-12 && (r->d.avisos & SIM_AV_PASO_LIM) &&
                     r->t_med0 > 0.155 && r->t_med0 < r->t_fin && real <= 3.1);
            sprintf(det, "la media de V(C), %.6g: la del registro en [t_med0, t_fin], %.6g (sin lo de antes de la ventana)", r->Vmed[kC], v_reg);
            chk_cond(q, det, i0 >= 0 && fabs(g->t[i0] - r->t_med0) <= 1e-12 && fabs(r->Vmed[kC] - v_reg) <= 1e-9 * (fabs(v_reg) + 1.0));
            sprintf(det, "t_est, %.2f s, se corrige al ritmo de cada segundo (la " S_uu "ltima mirada): lo real (%.2f s), a " S_PM "10 %%",
                    r->t_est, real);
            chk_cond(q, det, real <= 1.1 * r->t_est && r->t_est <= 1.1 * real);
            h1 = kc_huella(); tf1 = r->t_fin; tm1 = r->t_med0; te1 = r->t_est; p1 = r->pasos;
        }
        chk_cond(q, "sim_avanzar(1): la misma ventana, la misma medida y el mismo registro (bit a bit)",
                 r->t_fin == tf1 && r->t_med0 == tm1 && r->t_est == te1 && r->pasos == p1 && kc_huella() == h1);
    }
}
