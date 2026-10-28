/* --------------------------------------------------------------------------
 *  61b. Etapa 4: paso, ventana y modos con no lineales (paso 10, §3.K)
 *
 *  Se incluye tras test_e4_barr.inc (usa sr, a4_*, t4_*, canal, emula).
 * ----------------------------------------------------------------------- */

/* La ventana automática de la red sr (sin emular: la elige sim_iniciar_red) */
static double p4_ventana(const char *q)
{
    double t;
    saj.t_fin = 0.0; saj.h = 0.0;
    chk_cond(q, "empieza", sim_iniciar_red(&sr, &saj) == SIM_OK);
    t = sim_res()->t_fin;
    sim_cortar();
    return t;
}

static void test_e4_paso(void)
{
    SimMedidas me;
    int k, kC, i;

    printf("[61b] Etapa 4: paso, ventana y modos con no lineales\n");

    /* ---- El astable (V0 vacía): la topología saturada, su τ = RC y la
     *      ventana de 10·τ (~4,5 periodos), con el paso automático ---- */
    {
        const char *q = "astable autom" S_aa "tico";
        const SimRes *r;
        sr_init(4);
        sr_el(SE_R, 3, 1, 10e3);
        kC = sr_el(SE_C, 1, 0, 100e-9);
        sr_el(SE_R, 3, 2, 10e3); sr_el(SE_R, 2, 0, 10e3);
        k = a4_ao(2, 1, 3, 0, 0, 0.0);
        canal(0, CANAL_NODO, 3); canal(1, CANAL_V, kC);
        saj.t_fin = 0.0; saj.h = 0.0;
        emula(q);
        r = sim_res();
        chk_cond(q, "dos topolog" S_ii "as: la del OP (U1 lineal) y la de U1 en V+",
                 r->ntop == 2 && r->mascara[1] == r->mascara[0] &&
                 ((r->tramos_top[0] >> (2 * k)) & 3u) == TR_LIN && ((r->tramos_top[1] >> (2 * k)) & 3u) == TR_SATP);
        chk_cond(q, "la saturada tiene un modo real", r->nmodos[1] == 1 && r->modo[1][0].im == 0.0);
        if (r->nmodos[1] == 1) chk_t(q, "s = -1/(RC)", r->modo[1][0].re, -1000.0, 1000.0, 1e-9);
        chk_t(q, "la ventana: 10" S_CDOT S_TAU " = 10 ms", r->t_fin, 10e-3, 10e-3, 1e-12);
        chk_cond(q, "ARRANQUE y sin errores", r->d.estado == SIM_OK && (r->d.avisos & SIM_AV_ARRANQUE) && r->fin == FIN_COMPLETO);
        sim_medir(0, 2e-3, 10e-3, &me);
        chk_t(q, "f = 455,1196 Hz con el paso autom" S_aa "tico", me.frec, 455.1196133134187, 455.0, 1e-3);
        {
            int ev = r->nw.eventos, esperados = (int)(2.0 * 10e-3 / 2.1972245773362194e-3);
            pruebas++;
            if (abs(ev - esperados) > 2) falla(q, "2 cambios por periodo", ev, esperados);
        }
        chk_cond(q, "estimado <= 2 s", r->t_est <= 2.0);
        if (getenv("HAKARU_DEPURA")) printf("    astable: h %g, %ld pasos, t_est %.3f s\n", r->h, r->pasos, r->t_est);
    }

    /* ---- El rectificador en puente: con fuentes periódicas, 5·T; la τ de
     *      la descarga no cuenta si pasa de 10·T ---- */
    {
        static const double C[2] = { 1000e-6, 100e-6 }, VENT[2] = { 0.1, 0.5 };
        for (i = 0; i < 2; i++) {
            const char *q = i ? "puente, RC = 6 T" : "puente, RC = 60 T";
            sr_init(4);
            t4_seno(0, 1, 10.0, 60.0, 0.0);
            t4_d_cat(1, 2, &NL_1N4007);
            t4_d_cat(0, 2, &NL_1N4007);
            t4_d_cat(3, 1, &NL_1N4007);
            t4_d_cat(3, 0, &NL_1N4007);
            kC = t4_c0(2, 3, C[i]);
            sr_el(SE_R, 2, 3, 1e3);
            canal(0, CANAL_V, kC); canal(1, CANAL_NADA, 0);
            chk_t(q, i ? "la ventana: 5" S_CDOT S_TAU " = 0,5 s (la " S_TAU " no pasa de 10 T)" : "la ventana: 5 T redondeada = 0,1 s",
                  p4_ventana(q), VENT[i], VENT[i], 1e-12);
        }
    }

    /* ---- El Schmitt con una senoidal: el OP ya está en V+ (la saturada es
     *      la misma topología: no se repite) y la ventana es 5·T ---- */
    {
        const char *q = "Schmitt con senoidal";
        sr_init(4);
        t4_seno(0, 1, 10.0, 1e3, 0.0);
        sr_el(SE_R, 3, 2, 10e3); sr_el(SE_R, 2, 0, 10e3);
        k = a4_ao(2, 1, 3, 0, 0, 0.0);
        canal(0, CANAL_NODO, 3); canal(1, CANAL_NADA, 0);
        chk_t(q, "la ventana: 5 T = 5 ms", p4_ventana(q), 5e-3, 5e-3, 1e-12);
        chk_cond(q, "una topolog" S_ii "a, con U1 en V+", sim_res()->ntop == 1 &&
                 ((sim_res()->tramos_top[0] >> (2 * k)) & 3u) == TR_SATP);
    }

    /* ---- Una red lineal no cambia: sin tramos y sin topología de más ---- */
    {
        const char *q = "RC lineal";
        sr_init(3);
        sr_escalon(0, 1, 1.0);
        sr_el(SE_R, 1, 2, 1e3);
        kC = sr_el(SE_C, 2, 0, 1e-6);
        canal(0, CANAL_V, kC); canal(1, CANAL_NADA, 0);
        chk_t(q, "la ventana: 5" S_CDOT S_TAU " = 5 ms", p4_ventana(q), 5e-3, 5e-3, 1e-12);
        chk_cond(q, "una topolog" S_ii "a sin tramos", sim_res()->ntop == 1 && sim_res()->tramos_top[0] == 0);
    }
}
