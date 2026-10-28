/* --------------------------------------------------------------------------
 *  65j. Etapa 4: los hallazgos del grupo barr_a (segunda revisión)
 *
 *  El barrido de CC y el trazador, una prueba por hallazgo (cada una falla
 *  sin su arreglo, comprobado por mutación):
 *    - el Schmitt con un umbral que no cae en la malla: la ida y la vuelta
 *      leen su rama hasta el umbral de verdad (barr_en, barr_nivel, [N]);
 *    - el rango del trazador (§4.10): la curva llega a I_top; y el LED de
 *      Vf alta con n = 2, que no pasaba por (Vf, If) (su Is salía de exp()
 *      y el modelo sigue por la recta de nl_exp por encima de 80);
 *    - la derivada exacta de la I de un AO real con Ro > 0;
 *    - el rango automático de una ganancia negativa.
 *  Se incluye tras test_e4_rev.inc (usa sr, sr_init, sr_el, a4_*, b4_*, c4_*,
 *  rc, rr, cvz, cv_*, rv_ctl, r3u_abre, r4_espera, ve y los chk).
 * ----------------------------------------------------------------------- */

/* El Schmitt del arnés (AO ideal, ±15 V, umbrales ±7,5 V) barrido de x0 a
 * x1, ida y vuelta; *ida y *vta, sus trazas */
static int rb2_schmitt(double x0, double x1, int *ida, int *vta)
{
    int k, t;
    sr_init(4);
    k = a4_cc(0, 1, 0.0);
    sr_el(SE_R, 3, 2, 10e3); sr_el(SE_R, 2, 0, 10e3);
    a4_ao(2, 1, 3, 0, 0, 0.0);
    b4_canal(0, CANAL_NODO, -1, 3, 1); b4_canal(1, CANAL_NADA, -1, -1, 1);
    cv_canal_nom(0, "Vsal(U1)");
    *ida = *vta = -1;
    if (!b4_barre("Schmitt", k, BX_E, x0, x1, 0, 1)) return 0;
    for (t = 0; t < barr_reg()->ntr; t++) { if (barr_reg()->recorrido[t] == 0) *ida = t; else *vta = t; }
    cv_nombra("E1", "V");
    return *ida >= 0 && *vta >= 0;
}

/* ¿barr_en de la traza tr en x vale y (a 1e-9)? */
static int rb2_lee(int tr, double x, double y)
{
    double v;
    return barr_en(tr, x, &v, 0) && fabs(v - y) < 1e-9;
}

/* La curva I-V del componente k de rc (el trazador): su I máxima y el final
 * del rango */
static double rb2_itop(int k, double *x1)
{
    const BarrReg *g = barr_reg();
    double im = -1e300;
    int t, i;
    *x1 = 0.0;
    if (barr_iniciar_curva(&rc, k) != SIM_OK || !cv_acaba()) return 0.0;
    for (t = 0; t < g->ntr; t++)
        if (g->canal[t] == 1)
            for (i = 0; i < g->n; i++) if (g->ok[t][i] && g->y[t][i] > im) im = g->y[t][i];
    *x1 = barr_res()->x1;
    return im;
}

/* Un diodo, zener o LED solo en rc, con su variante */
static int rb2_comp(int tipo, int var)
{
    int k;
    c4_init(2);
    k = nuevo_el((TipoEl)tipo, 1, 0);
    rr.c[k] = -1;
    rc.el[k].variante = (unsigned char)var;
    strcpy(rc.el[k].etiq, "D1");
    return k;
}

static void rb2_par(int k, int q, double v)
{
    rc.el[k].par[q - Q_P1].v = v;
    rc.el[k].par[q - Q_P1].est = M_DATO;
}

static void test_rev2_barr_a(void)
{
    static Circuito c;
    BarrAjustes aj;
    double x, y, y1, y2, dy, x0, x1, im;
    int ida, vta, k, kD, kR, kG, lg;

    printf("[65j] Etapa 4: los hallazgos del grupo barr_a\n");
    gfx_desbordes_reset();

    /* ---- El Schmitt con el umbral de 7,5 V fuera de la malla: entre el
     *      punto medio del intervalo del salto y el umbral, cada traza sigue
     *      en su rama (la continuación viene del punto de antes en su sentido) ---- */
    if (rb2_schmitt(-10.0, 10.1, &ida, &vta)) {
        const BarrReg *g = barr_reg();
        int j;
        for (j = 1; j < g->n && !g->salto[ida][j]; j++) ;
        chk_cond("Schmitt -10 a 10,1", "el salto de la ida, con el punto medio antes de 7,5",
                 j < g->n && g->x[j - 1] < 7.5 && g->x[j] > 7.5 && 0.5 * (g->x[j - 1] + g->x[j]) < 7.5);
        chk_cond("Schmitt -10 a 10,1", "la ida lee +15 V en 7,48 y en 7,49", rb2_lee(ida, 7.48, 15.0) && rb2_lee(ida, 7.49, 15.0));
        chk_cond("Schmitt -10 a 10,1", "y -15 V pasado el umbral (7,51)", rb2_lee(ida, 7.51, -15.0));
        /* el umbral, a lo que Newton da por bueno el margen del tramo (1e-9·Vesc = 3e-8 V) */
        chk_cond("Schmitt -10 a 10,1", "barr_nivel(0) en la ida: el umbral, 7,5 V",
                 barr_nivel(ida, 0.0, -10.0, 10.1, &x) && fabs(x - 7.5) < 1e-6);
        cv_empieza();
        CV_TECLAS(T_LETRA('N'), T_DIG(0), T_ENTER);
        chk_cond("Schmitt -10 a 10,1", "[N] 0: «salta por 0,000 V en E1 = 7,500 V»",
                 strcmp(cvz.aviso, "Vsal(U1) salta por 0,000 V en E1 = 7,500 V") == 0);
        CV_TECLAS(T_DIG(7), T_PUNTO, T_DIG(4), T_DIG(9), T_ENTER);
        chk_cond("Schmitt -10 a 10,1", "el cursor en 7,49: la ida en +15 V", cv_ve("E1 = 7,490 V  Vsal(U1) = 15,00 V"));
    } else chk_cond("Schmitt -10 a 10,1", "barre", 0);
    if (rb2_schmitt(-9.0, 9.0, &ida, &vta)) {        /* el rango automático de una E1 de 9 V */
        chk_cond("Schmitt -9 a 9", "7,495: +15 V en la ida; -7,495: -15 V en la vuelta",
                 rb2_lee(ida, 7.495, 15.0) && rb2_lee(vta, -7.495, -15.0));
        chk_cond("Schmitt -9 a 9", "pasado el umbral, la otra rama", rb2_lee(ida, 7.505, -15.0) && rb2_lee(vta, -7.505, 15.0));
        chk_cond("Schmitt -9 a 9", "barr_nivel(0): 7,5 en la ida y -7,5 en la vuelta",
                 barr_nivel(ida, 0.0, -9.0, 9.0, &x) && fabs(x - 7.5) < 1e-6 &&
                 barr_nivel(vta, 0.0, -9.0, 9.0, &y) && fabs(y + 7.5) < 1e-6);
    } else chk_cond("Schmitt -9 a 9", "barre", 0);

    /* ---- El trazador (§4.10): E hasta V(I_top) + 1 Ω·I_top, así que cada
     *      curva llega a I_top (100 mA; 1 A el 1N4007; 1,2·Im el LED) ---- */
    kD = rb2_comp(EL_DIODO, DIO_VG);
    rb2_par(kD, QD_VG, 3.0);
    im = rb2_itop(kD, &x1);
    chk_t("trazador", "diodo con V" S_GAMMA " = 3 V: llega a 100 mA", im, 0.1, 0.1, 1e-6);
    kD = rb2_comp(EL_DIODO, DIO_EXP);
    rb2_par(kD, QD_N, 4.0);
    im = rb2_itop(kD, &x1);
    chk_t("trazador", "exponencial con n = 4: llega a 100 mA", im, 0.1, 0.1, 1e-6);
    kD = rb2_comp(EL_LED, LED_AZUL);
    rb2_par(kD, QL_VF, 6.0);
    im = rb2_itop(kD, &x1);
    chk_t("trazador", "LED azul con Vf = 6 V: llega a 1,2" S_CDOT "Im = 36 mA", im, 36e-3, 36e-3, 1e-6);
    cv_empieza();
    CV_TECLAS(T_LETRA('N'), T_DIG(2), T_DIG(0), T_LETRA('M'), T_ENTER);
    chk_cond("trazador", "[N] 20 mA: el cruce, en V(D1) = 6 V", cv_aviso_empieza("I(D1) = 20,00 mA en E = ") &&
             barr_en(0, cvz.xc[0], &y, 0) && fabs(y - 6.0) < 1e-6);
    kD = rb2_comp(EL_LED, LED_ROJO);
    im = rb2_itop(kD, &x1);
    chk_t("trazador", "LED rojo: 36 mA, no 0,52 A", im, 36e-3, 36e-3, 1e-6);
    /* el LED rojo (n = 2) con Vf = 6 V: vd/(n·VT) = 114 pasa de 80, donde el
     * modelo sigue por la recta de nl_exp; su Is tiene que salir de esa misma
     * curva para que pase por (Vf, If) (D6), y la curva I-V, sin huecos */
    rb2_par(kD, QL_VF, 6.0);
    im = rb2_itop(kD, &x1);
    chk_cond("LED rojo con Vf = 6 V", "la curva I-V, entera y hasta 36 mA", barr_res()->huecos == 0 && fabs(im - 36e-3) < 36e-9);
    cv_empieza();
    CV_TECLAS(T_LETRA('N'), T_DIG(2), T_DIG(0), T_LETRA('M'), T_ENTER);
    chk_cond("LED rojo con Vf = 6 V", "pasa por (Vf, If): 20 mA en V(D1) = 6 V", cv_aviso_empieza("I(D1) = 20,00 mA en E = ") &&
             barr_en(0, cvz.xc[0], &y, 0) && fabs(y - 6.0) < 1e-6);
    kD = rb2_comp(EL_DIODO, DIO_1N4148);
    im = rb2_itop(kD, &x1);
    chk_t("trazador", "1N4148: 100 mA, no 0,71 A", im, 0.1, 0.1, 1e-6);
    chk_t("trazador", "1N4148: E acaba en V(100 mA) + 0,1 V", x1, 0.84965648028731514 + 0.1, 1.0, 1e-9);
    kD = rb2_comp(EL_DIODO, DIO_1N4007);
    im = rb2_itop(kD, &x1);
    chk_t("trazador", "1N4007: 1 A", im, 1.0, 1.0, 1e-6);
    kD = rb2_comp(EL_ZENER, ZEN_LIBRO);
    im = rb2_itop(kD, &x1);
    chk_t("trazador", "zener de libro: 100 mA en directa", im, 0.1, 0.1, 1e-6);
    chk_cond("trazador", "zener de libro: la inversa sigue desde -Vz0 - 2 V", barr_res()->x0 < -5.0);

    /* ---- La I de un AO real con Ro > 0: su dy/dx exacta ---- */
    {
        int kR1, kRf;
        k = a4_inversor(0.0, 1, 1e5, &kR1, &kRf);
        sr.el[k].m[4] = 100.0;                        /* Ro = 100 Ω: sin rama */
        b4_canal(0, CANAL_I, k, -1, 1); b4_canal(1, CANAL_NADA, -1, -1, 1);
        cv_canal_nom(0, "I(U1)");
        if (b4_barre("AO real Ro = 100", 0, BX_E, -2.0, 2.0, 0, 0)) {
            barr_en(0, 0.5 + 1e-4, &y1, 0); barr_en(0, 0.5 - 1e-4, &y2, 0); barr_en(0, 0.5, &y, &dy);
            chk_t("AO real Ro = 100", "lineal: dI(U1)/dE exacta = diferencias centradas", dy, (y1 - y2) / 2e-4, 1e-3, 1e-7);
            barr_en(0, 1.8 + 1e-4, &y1, 0); barr_en(0, 1.8 - 1e-4, &y2, 0); barr_en(0, 1.8, &y, &dy);
            chk_t("AO real Ro = 100", "saturado: -1/(R1 + Rf + Ro)", dy, -1.0 / 11100.0, 1e-4, 1e-9);
            chk_t("AO real Ro = 100", "saturado: = diferencias centradas", dy, (y1 - y2) / 2e-4, 1e-4, 1e-7);
            cv_nombra("E1", "V");
            cv_empieza();
            CV_TECLAS(T_DIG(0), T_PUNTO, T_DIG(5), T_ENTER);
            chk_cond("AO real Ro = 100", "el panel: «g = dI/dV = -999,9 " S_MU "S»", cv_ve("g = dI/dV = -999,9 " S_MU "S"));
        }
    }

    /* ---- La ganancia negativa: el rango automático va de 2k a 0 ---- */
    c4_init(3);
    c4_el(EL_FUENTE, 0, 1, 1.0);
    kR = c4_el(EL_RES, 1, 0, 1e3);
    kG = c4_dep(EL_EDEP, 0, 2, DEP_V, -10.0, kR);
    c4_el(EL_RES, 2, 0, 1e3);
    cierra_red();
    barr_rango_auto(&rc, kG, &x0, &x1, &lg);
    chk_cond("ganancia negativa", "barr_rango_auto(mu = -10): de -20 a 0", x0 == -20.0 && x1 == 0.0 && !lg);
    memset(&aj, 0, sizeof aj);
    aj.el = (signed char)kG;
    chk_cond("ganancia negativa", "barr_iniciar con el rango autom" S_aa "tico",
             barr_iniciar(&rc, &rr, &aj) == SIM_OK && cv_acaba() && barr_reg()->x[0] == -20.0 && barr_reg()->x[barr_reg()->n - 1] == 0.0);
    cv_empieza();
    chk_cond("ganancia negativa", "la curva se ve: ni «Sin puntos» ni el 0 a 1", !cv_ve("Sin puntos") && cv_ve("-20 a 0"));
    /* en los ajustes: «auto (-20)» y «auto (0)» se aplican */
    rv_ctl(&c, EL_EDEP, DEP_V, -10.0);
    r3u_abre(&c);
    TECLAS(T_LETRA('I'));
    r4_espera();
    TECLAS(T_MENU, T_DER, T_DER, T_DER);
    chk_cond("ganancia negativa", "los ajustes: la controlada, de auto (-20) a auto (0)", ui_pantalla() == P_AJUSTES &&
             ve("auto (-20)") && ve("auto (0)"));
    TECLAS(T_IGUAL);
    r4_espera();
    chk_cond("ganancia negativa", "[=]: barre de -20 a 0 y se ve", ui_pantalla() == P_BARRIDO && barr_res()->fin == FIN_COMPLETO &&
             barr_res()->el == 3 && barr_res()->x0 == -20.0 && barr_res()->x1 == 0.0 && !ve("Sin puntos") &&
             !ve("Desde tiene que"));

    chk_cond("interfaz", "ning" S_uu "n texto fuera de la pantalla", gfx_desbordes() == 0);
}
