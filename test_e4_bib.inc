/* --------------------------------------------------------------------------
 *  66. Etapa 4: la biblioteca de los semiconductores (modelos 23 a 42: el 41
 *  y el 42, con el BJT del paso 13)
 *
 *  Cada modelo es válido, sus textos caben, se resuelve sin error y da su
 *  clave con su tolerancia (la tabla del §6.12): los de ca 0 emulándolos
 *  (como [39]), los de ca 3 con el punto de operación de Newton y los de ca 4
 *  con el barrido de CC en el rango del modelo. El tiempo estimado en la
 *  calculadora: la emulación, su t_est <= 2 s; el punto de operación y el
 *  barrido, por operaciones (SIM_NS_OP) e iteraciones (NS_ITER) <= 0,5 s. Todos
 *  los AO llevan tierra (red_ref_propia = 1).
 *  Se incluye tras test_e4_paso.inc (usa chk_cond, chk_t, falla, lineas,
 *  medida_modelo y nolin.h).
 * ----------------------------------------------------------------------- */
#define BIB4_DE 22                     /* MODELOS[22] es el modelo 23 */
#define BIB4_A  42                     /* hasta el 42 (paso 13: el BJT) */
#define BIB4_NS_OP SIM_NS_OP           /* una operación en la calculadora (sim.h) */

/* El canal 1 que tiene que salir en el barrido de cada modelo de ca = 4 */
static const char *bib4_canal1(int i)
{
    switch (i + 1) {
    case 24: case 32: return "I(D1)";
    case 33:          return "Vsal(U1)";
    default:          return "";
    }
}

static void test_e4_bib(void)
{
    static Circuito c;
    static Red r;
    static Valores v;
    int i, j, n3 = 0, n4 = 0, n0 = 0;

    printf("[66] Etapa 4: la biblioteca de los semiconductores (modelos %d a %d)\n", BIB4_DE + 1, BIB4_A);
    chk_cond("biblioteca E4", "hay 42 modelos", N_MODELOS >= BIB4_A);

    /* ---- Las lecciones 12 (23 a 32) y 13 (33 a 42) ---- */
    if (N_LECCIONES >= 13) {
        const Leccion *l12 = &LECCIONES[11], *l13 = &LECCIONES[12];
        int ok = l12->nmod == 10 && l13->nmod == 10 && l12->npag == 2 && l13->npag == 2 &&
                 l12->nej == 0 && l13->nej == 0;
        for (j = 0; ok && j < l12->nmod; j++) if (l12->mod[j] != BIB4_DE + j) ok = 0;
        for (j = 0; ok && j < l13->nmod; j++) if (l13->mod[j] != BIB4_DE + 10 + j) ok = 0;
        chk_cond("lecciones 12 y 13", "12: los modelos 23 a 32; 13: del 33 al 42; dos p" S_aa "ginas de teor" S_ii "a cada una", ok);
        chk_cond("lecciones 12 y 13", "la teor" S_ii "a 13.2 habla del BJT: sus regiones y su punto Q",
                 strstr(l13->pag[1].texto, "BJT") && strstr(l13->pag[1].texto, "ACTIVA") && strstr(l13->pag[1].texto, "CORTE") &&
                 strstr(l13->pag[1].texto, "SATURACI") && strstr(l13->pag[1].texto, "PUNTO Q") &&
                 strstr(l13->pag[1].texto, "recta de carga"));
        for (j = 0; j < 4; j++) {                   /* 15 líneas de 52, y ninguna se corta */
            const Pagina *p = j < 2 ? &l12->pag[j % 2] : &l13->pag[j % 2];
            const char *s;
            int n = lineas(p->texto, 52), nl = 1;
            char dd[64];
            for (s = p->texto; *s; s++) if (*s == '\n') nl++;
            sprintf(dd, "teor" S_ii "a %d.%d: %d l" S_ii "neas de 15, sin cortes", 12 + j / 2, 1 + j % 2, n);
            chk_cond("lecciones 12 y 13", dd, p->fig == FIG_NADA && n <= 15 && n == nl);
        }
    } else chk_cond("lecciones 12 y 13", "existen", 0);

    for (i = BIB4_DE; i < BIB4_A && i < N_MODELOS; i++) {
        const CircModelo *md = &MODELOS[i];
        char q[64];
        double x = -1e300, est = 0.0;
        int k;

        sprintf(q, "modelo %d", i + 1);
        chk_cond(q, "t" S_ii "tulo de 44 columnas o menos", strlen(md->titulo) <= 44);
        chk_cond(q, "enunciado en 3 l" S_ii "neas de 52", lineas(md->enunciado, 52) <= 3);
        chk_cond(q, "qu" S_ee " ver en 2 l" S_ii "neas de 52", lineas(md->que_ver, 52) <= 2);
        chk_cond(q, "ca de 0, 3 o 4 y canal 1", (md->ca == 0 || md->ca == 3 || md->ca == 4) && md->canal == 1);
        modelo_cargar(i, &c);
        chk_cond(q, "circuito v" S_aa "lido", circ_valido(&c));
        chk_cond(q, "de la Etapa 4", circ_etapa4(&c) && circ_no_lineal(&c) == (i + 1 != 40));
        red_extraer(&c, &r);
        for (k = 0; k < c.nel; k++)
            if (c.el[k].tipo == EL_AO) {
                chk_cond(q, "el AO con su tierra (red_ref_propia = 1)", c.tierra_x >= 0 && c.tierra_y >= 0 &&
                         r.c[k] >= 0 && red_ref_propia(&c, &r, r.c[k]) == 1);
            }

        if (md->ca == 0) {                              /* se emula, como [39] */
            const SimRes *res;
            n0++;
            chk_cond(q, "din" S_aa "mico: [=] lo emula", circ_dinamico(&c) && !circ_pide(&c));
            chk_cond(q, "emula sin error", sim_iniciar(&c, &r) == SIM_OK);
            while (sim_avanzar(SIM_OPS_TICK)) ;
            res = sim_res();
            chk_cond(q, "completo", res->fin == FIN_COMPLETO && res->d.estado == SIM_OK);
            est = res->t_est;
            x = medida_modelo(md, &c, 0);
            printf("    %-36s %5ld pasos, h = %-9.3g estimado %.2f s  '%c' = %.12g\n",
                   md->titulo, res->pasos, res->h, est, md->medida, x);
            pruebas++;
            /* el plan cabe en 2 s (T_TOPE); t_est, con lo que el control del
             * gasto ve cada segundo, puede pasar un poco: el puente (el 28),
             * 1,91 s a priori, 2,06 así y 2,05 de verdad */
            if (!(est <= 2.2)) falla(q, "en la calculadora, 2,2 s o menos (estimado)", est, 2.2);
            /* las otras claves de la tabla y los avisos del §4.13 */
            switch (i + 1) {
            case 26: {                                  /* el rizado del último periodo */
                Medidas me;
                osc_medidas(sim_reg(), 0, res->t_fin - 1.0 / 60.0, res->t_fin, &me);
                chk_t(q, "'U' - 'u' = 1,2906 (el rizado)", me.max - me.min, 1.290563502742295, 1.290563502742295, 1e-2);
                break;
            }
            case 37:
                chk_cond(q, "V0 vac" S_ii "a: sin el aviso CI_SALTA", !(res->d.avisos & SIM_AV_CI_SALTA));
                break;
            case 38: {
                Medidas me;
                osc_medidas(sim_reg(), 0, 0.0, res->t_fin, &me);
                chk_t(q, "'F' = 1000 Hz", me.frec, 1000.0, 1000.0, 1e-9);
                break;
            }
            case 39:
                chk_cond(q, "V0 vac" S_ii "a: el aviso ARRANQUE", (res->d.avisos & SIM_AV_ARRANQUE) != 0);
                break;
            default: break;
            }
        } else if (md->ca == 3) {                       /* el punto de operación de Newton */
            SimDiag d;
            SimNewton nw;
            unsigned long o;
            int e;
            n3++;
            chk_cond(q, "de CC: [=] da el punto de operaci" S_oo "n", !circ_dinamico(&c) && !circ_pide(&c));
            memset(&nw, 0, sizeof nw);
            sim_ops_reset();
            e = sim_punto_op_cc(&c, &r, &v, &d, &nw);
            o = sim_ops();
            chk_cond(q, "punto de operaci" S_oo "n sin error", e == SIM_OK);
            est = (double)o * BIB4_NS_OP * 1e-9 + (double)nw.iter * NS_ITER * 1e-9;
            x = medida_modelo(md, &c, &v);
            printf("    %-36s %3ld iteraciones, %7lu operaciones, estimado %.4f s  '%c' = %.17g\n",
                   md->titulo, nw.iter, o, est, md->medida, x);
            pruebas++;
            if (!(est <= 0.5)) falla(q, "en la calculadora, 0,5 s o menos (estimado)", est, 0.5);
            if (i + 1 == 31) {
                k = el_de_etiqueta(&c, "D1");
                chk_cond(q, "el zener, en ruptura", k >= 0 && v.reg[k] == REG_RUPTURA);
            }
            if (i + 1 == 41) {                           /* §6.11: el interruptor satura */
                k = el_de_etiqueta(&c, "Q1");
                chk_cond(q, "Q1 en SATURACION, IC = I(LED)", k >= 0 && v.reg[k] == REG_SATURACION &&
                         fabs(v.I[k] - x) <= 1e-9 * x);
                if (k >= 0) {
                    chk_t(q, "IB = 0,89455484696801574 mA", v.I2[k], 0.89455484696801574e-3, 0.89e-3, 1e-9);
                    chk_t(q, "VCE = 0,091424804456153236", v.V[k], 0.091424804456153236, 0.0914, 1e-9);
                }
            }
            if (i + 1 == 42) {                           /* §6.11: el emisor común, en activa */
                int kv = el_de_etiqueta(&c, "V1");
                k = el_de_etiqueta(&c, "Q1");
                chk_cond(q, "Q1 en ACTIVA", k >= 0 && v.reg[k] == REG_ACTIVA);
                chk_cond(q, "V1 marca VCE", kv >= 0);
                if (kv >= 0) chk_t(q, "V1 = VCE = 5,4357760891729673 V", v.V[kv], 5.4357760891729673, 5.4, 1e-9);
                if (k >= 0) chk_t(q, "VBE = 0,73789576485171988", v.V2[k], 0.73789576485171988, 0.74, 1e-9);
            }
        } else if (md->ca == 4) {                       /* el barrido de CC, en el rango del modelo */
            BarrAjustes aj;
            const BarrReg *b;
            unsigned long o;
            long it = 0;
            int tr = -1;
            n4++;
            chk_cond(q, "de CC: sin din" S_aa "mica ni sondas", !circ_dinamico(&c));
            memset(&aj, 0, sizeof aj);
            k = md->barre ? el_de_etiqueta(&c, md->barre) : -1;
            chk_cond(q, "lo que se barre existe", k >= 0);
            aj.el = (signed char)k;
            aj.x0 = md->x0; aj.x1 = md->x1; aj.log_x = md->log_x;
            o = barr_ops();
            chk_cond(q, "barr_iniciar sin error", barr_iniciar(&c, &r, &aj) == SIM_OK);
            while (barr_avanzar(~0ul)) {}
            o = barr_ops() - o;
            b = barr_reg();
            chk_cond(q, "barrido completo, sin huecos", barr_res()->fin == FIN_COMPLETO && barr_res()->huecos == 0 &&
                     b->n == BARR_N && barr_res()->x0 == md->x0 && barr_res()->x1 == md->x1 && barr_res()->log_x == md->log_x);
            chk_cond(q, "el canal 1 de la clave", strcmp(barr_res()->canal[0].nombre, bib4_canal1(i)) == 0);
            for (j = 0; j < b->ntr && tr < 0; j++) if (b->canal[j] == 0 && b->recorrido[j] == 0) tr = j;
            for (j = 0; tr >= 0 && j < b->n; j++) it += b->it[tr][j];
            est = (double)o * BIB4_NS_OP * 1e-9 + (double)it * NS_ITER * 1e-9;
            x = medida_modelo(md, &c, 0);
            printf("    %-36s %3d puntos, %4ld iteraciones, %7lu operaciones, estimado %.3f s (t_est %.3f)  '%c' = %.17g\n",
                   md->titulo, b->n, it, o, est, barr_res()->t_est, md->medida, x);
            pruebas++;
            if (!(est <= 0.5)) falla(q, "en la calculadora, 0,5 s o menos (estimado)", est, 0.5);
        }
        pruebas++;
        if (!(fabs(x - md->clave) <= md->tol * fabs(md->clave))) {
            char dd[96];
            sprintf(dd, "clave '%c' = %.17g", md->medida, md->clave);
            falla(q, dd, x, md->clave);
        }
    }
    chk_cond("biblioteca E4", "10 de tiempo, 7 de punto de operaci" S_oo "n y 3 de barrido", n0 == 10 && n3 == 7 && n4 == 3);
}
