/* --------------------------------------------------------------------------
 *  65k. Etapa 4: la segunda revisión del barrido de CC y de la familia
 *
 *  Una prueba por hallazgo (cada una falla sin su arreglo, comprobado por
 *  mutación): el cursor sobre un hueco no repite en cada cuadro la
 *  resolución que ya falló; la familia sin VA da la pendiente exacta fuera
 *  de su zona plana (la saturación del BJT, la óhmica del MOSFET) y no
 *  «r0 = ∞»; y lo barrido lleva su unidad cuando es la ganancia de una
 *  controlada (g en S, r en Ω). Se incluye tras test_e4_rev.inc (usa sr,
 *  sr_init, sr_el, a4_cc, t4_d_cat, b4_canal, rc, rr, c4_*, cierra_red,
 *  cvz, cv_* y los chk).
 * ----------------------------------------------------------------------- */

/* ¿Se ve «pre» seguido de x con 4 cifras y la unidad u (con su espacio)? */
static int rb2_ve_num(const char *pre, double x, const char *u)
{
    char t[64], n[24];
    Buf b;
    buf_init(&b, t, sizeof t);
    buf_str(&b, pre);
    buf_str(&b, cv_fija(n, sizeof n, x, 4, u));
    return cv_ve(t);
}

static void test_rev2_barr_b(void)
{
    static Circuito c;
    static Valores v;
    const BarrReg *g;
    BarrAjustes aj;
    SimDiag dd;
    double y, d, xh = 0.0;
    int k, kD, i;

    printf("[65k] Etapa 4: la segunda revisi" S_oo "n del barrido y de la familia\n");
    gfx_desbordes_reset();

    /* ---- 1. El cursor sobre un hueco: el fallo se recuerda como una
     *      lectura buena (con los topes del arnés, que dan huecos de verdad) ---- */
    sr_init(3); k = a4_cc(0, 1, 1.0); sr_el(SE_R, 1, 2, 1e3); kD = t4_d_cat(2, 0, &NL_1N4148);
    b4_canal(0, CANAL_I, kD, -1, 1); b4_canal(1, CANAL_V, kD, -1, 1);
    cv_canal_nom(0, "I(D1)"); cv_canal_nom(1, "V(D1)");
    sim_nl_topes(3, 0);
    sim_nl_tope_ops(29000);                    /* 463 por iteración (NS_ITER): eran 5000 */
    barr_iniciar_red(&sr, k, BX_E, b4_ch, 0.0, 50.0, 0, 0);
    cv_acaba();
    cv_nombra("E1", "V");
    cv_empieza();
    g = barr_reg();
    {
        int ih = -1, ib = -1;
        for (i = 1; i < g->n; i++) {
            if (!g->ok[0][i] && ih < 0) ih = i;
            if (g->ok[0][i] && ib < 0 && g->x[i] > 20.0) ib = i;
        }
        chk_cond("hueco", "el barrido con los topes tiene huecos", barr_res()->fin == FIN_COMPLETO && ih > 0 && ib > 0);
        if (ih > 0 && ib > 0) {
            unsigned long o0, b1, b2, h1, h2;
            cvz.xc[0] = g->x[ib];
            o0 = sim_ops(); cv_d(); b1 = sim_ops() - o0;
            o0 = sim_ops(); cv_d(); b2 = sim_ops() - o0;
            xh = cvz.xc[0] = g->x[ih];
            o0 = sim_ops(); cv_d(); h1 = sim_ops() - o0;
            o0 = sim_ops(); cv_d(); h2 = sim_ops() - o0;
            printf("    cursor en un punto bueno: cuadro 1 %lu ops, cuadro 2 %lu; en el hueco (x = %g): cuadro 1 %lu, cuadro 2 %lu\n",
                   b1, b2, g->x[ih], h1, h2);
            chk_cond("hueco", "en el hueco no hay lectura", cv_ve("Aqu" S_ii " Newton no converge: sin lectura.") &&
                     !barr_en(0, g->x[ih], &y, &d));
            chk_cond("hueco", "un punto bueno: el segundo cuadro no resuelve nada", b1 > 0 && b2 == 0);
            chk_cond("hueco", "el hueco: el segundo cuadro tampoco (el fallo se recuerda)", h1 > 0 && h2 == 0);
            chk_cond("hueco", "y el punto bueno se sigue leyendo (la malla, 1e-9)", barr_en(0, g->x[ib], &y, 0) &&
                     fabs(y - g->y[0][ib]) <= 1e-9 * fabs(g->y[0][ib]));
        }
    }
    sim_nl_tope_ops(0); sim_nl_topes(0, 0);
    /* otro barrido (sin topes) en el mismo x no hereda el fallo */
    if (barr_iniciar_red(&sr, k, BX_E, b4_ch, 0.0, 50.0, 0, 0) == SIM_OK && cv_acaba()) {
        int j;
        for (j = 0; j < g->n && g->ok[0][j]; j++) {}
        chk_cond("hueco", "sin topes: sin huecos y se lee en el x del hueco", j == g->n && xh > 0.0 && barr_en(0, xh, &y, &d));
    }

    /* ---- 2. La familia sin VA: «r0 = ∞» solo en su zona plana ---- */
    circ_init(&c, "familia");
    k = circ_elemento(&c, EL_MOS, 4, 3, DIR_DER);
    chk_cond("familia sin VA", "el MOSFET por omisi" S_oo "n: su familia", k >= 0 && barr_iniciar_curva(&c, k) == SIM_OK &&
             cv_acaba() && !barr_res()->fam_early && !barr_res()->fam_bjt);
    cv_empieza();
    cvz.activo = 5;                                       /* VGS = 4,5 V: vov = 2,5 V */
    cvz.xc[0] = 0.5;                                      /* VDS = 0,5 V: óhmica */
    barr_en(5, 0.5, &y, &d);
    barr_valores(5, 0.5, &v, &dd);
    printf("    MOSFET, VGS = 4,5 V, VDS = 0,5 V: dID/dVDS = %.9g S (regi" S_oo "n %d); ve 'r0 = inf (sin VA)' = %d\n",
           d, v.reg[2], cv_ve(S_INF " (sin VA)"));
    chk_cond("familia sin VA", "MOSFET en la " S_oo "hmica: dID/dVDS = 2" S_CDOT "K" S_CDOT "(vov - VDS) = 4 mS",
             v.reg[2] == REG_OHMICA && fabs(d - 4e-3) < 1e-9);
    chk_cond("familia sin VA", "y la pantalla: «r0 = dVDS/dID = 250,0 " S_OHM "», no «" S_INF " (sin VA)»",
             cv_ve("r0 = dVDS/dID = 250,0 " S_OHM) && !cv_ve(S_INF " (sin VA)"));
    cvz.xc[0] = 5.0;                                      /* saturación: sin VA la pendiente es 0 */
    barr_valores(5, 5.0, &v, &dd);
    chk_cond("familia sin VA", "MOSFET saturado: «r0 = " S_INF " (sin VA)»", v.reg[2] == REG_SATURACION &&
             cv_ve("r0 = " S_INF " (sin VA)"));
    chk_cond("interfaz", "la l" S_ii "nea cabe en 52 columnas", gfx_desbordes() == 0);

    circ_init(&c, "familia");
    k = circ_elemento(&c, EL_BJT, 4, 3, DIR_DER);
    chk_cond("familia sin Early", "el BJT por omisi" S_oo "n: su familia", k >= 0 && barr_iniciar_curva(&c, k) == SIM_OK &&
             cv_acaba() && !barr_res()->fam_early && barr_res()->fam_bjt);
    cv_empieza();
    cvz.activo = 3;                                       /* IB = 60 µA */
    for (i = 0; i < 2; i++) {
        double x = i ? 0.05 : 0.1;                        /* VCE: saturado */
        cvz.xc[0] = x;
        barr_en(3, x, &y, &d);
        barr_valores(3, x, &v, &dd);
        printf("    BJT, IB = 60 " S_MU "A, VCE = %g V: dIC/dVCE = %.9g S (1/d = %.6g ohm, regi" S_oo "n %d); ve 'sin Early' = %d\n",
               x, d, 1.0 / d, v.reg[2], cv_ve("sin Early"));
        chk_cond("familia sin Early", i ? "BJT en VCE = 50 mV: saturado, mS de pendiente" : "BJT en VCE = 100 mV: saturado, mS de pendiente",
                 v.reg[2] == REG_SATURACION && d > 1e-3);
        chk_cond("familia sin Early", i ? "VCE = 50 mV: «r0 = dVCE/dIC = 1/d», no «" S_INF " (sin Early)»" :
                                          "VCE = 100 mV: «r0 = dVCE/dIC = 1/d», no «" S_INF " (sin Early)»",
                 rb2_ve_num("r0 = dVCE/dIC = ", 1.0 / d, " " S_OHM) && !cv_ve("sin Early"));
    }
    cvz.xc[0] = 5.0;                                      /* activa: sin Early la pendiente es 0 */
    barr_valores(3, 5.0, &v, &dd);
    chk_cond("familia sin Early", "BJT en activa: «r0 = " S_INF " (sin Early)» y " S_BETA " = 100,0", v.reg[2] == REG_ACTIVA &&
             cv_ve("r0 = " S_INF " (sin Early)") && cv_ve(S_BETA " = IC/IB = 100,0"));
    cvz.activo = 0;                                       /* IB = 0: en corte */
    chk_cond("familia sin Early", "IB = 0: en corte, r0 (sin Early)", cv_ve("IB = 0: en corte") && cv_ve(S_INF " (sin Early)"));
    /* con Early, en saturación, lo de siempre: la pendiente exacta */
    c.el[k].par[QB_VA - Q_P1].v = 100.0; c.el[k].par[QB_VA - Q_P1].est = M_DATO;
    if (barr_iniciar_curva(&c, k) == SIM_OK && cv_acaba()) {
        cv_empieza();
        cvz.activo = 3; cvz.xc[0] = 0.1;
        barr_en(3, 0.1, &y, &d);
        chk_cond("familia sin Early", "con VA, saturado: «r0 = dVCE/dIC = 1/d»", barr_res()->fam_early &&
                 rb2_ve_num("r0 = dVCE/dIC = ", 1.0 / d, " " S_OHM));
    } else chk_cond("familia sin Early", "con VA: su familia", 0);

    /* ---- 3. La unidad de lo barrido cuando es una ganancia: g en S, r en Ω,
     *      μ y β sin unidad (las de la ficha y las de los ajustes) ---- */
    for (i = 0; i < 4; i++) {
        static const char *const UNI[4] = { "S", "", "", S_OHM };
        int kE, kR1, kJ, kR2, tj = i < 2 ? EL_JDEP : EL_EDEP, var = i == 0 || i == 2 ? DEP_V : DEP_I;
        c4_init(3);
        kE = c4_el(EL_FUENTE, 0, 1, 2.0);
        kR1 = c4_el(EL_RES, 1, 0, 1e3);
        kJ = c4_dep((TipoEl)tj, 0, 2, var, var == DEP_V ? 1e-3 : 2.0, kR1);
        kR2 = c4_el(EL_RES, 2, 0, 1e3);
        rc.el[kR2].V.est = M_BUSCAR;                      /* V(R2) pedida: el canal 1 */
        cierra_red();
        memset(&aj, 0, sizeof aj);
        aj.el = (signed char)kJ;                          /* el rango, automático: de 0 a 2·g */
        chk_cond("unidad de la ganancia", "barr_iniciar", barr_iniciar(&rc, &rr, &aj) == SIM_OK && cv_acaba());
        printf("    %s (variante %d): uni_x |%s|, la de su ficha |%s|\n", rc.el[kJ].etiq, var, barr_res()->uni_x,
               tipo_campo_v(tj, var, Q_VAL)->unidad);
        chk_cond("unidad de la ganancia", i == 0 ? "g: uni_x = S" : i == 1 ? "beta: sin unidad" : i == 2 ? "mu: sin unidad" : "r: uni_x = " S_OHM,
                 strcmp(barr_res()->uni_x, UNI[i]) == 0 && strcmp(barr_res()->uni_x, tipo_campo_v(tj, var, Q_VAL)->unidad) == 0);
        if (i == 0) {                                     /* V(R2) = g·V(R1)·R2 = 2000·g */
            char t[40];
            Buf b;
            cv_empieza();
            cvz.xc[0] = 1e-3;
            barr_en(0, 1e-3, &y, &d);
            buf_init(&b, t, sizeof t); buf_str(&b, rc.el[kJ].etiq); buf_str(&b, " = 1,000 mS");
            chk_cond("unidad de la ganancia", "g: la lectura «J3 = 1,000 mS» y la cabecera «0 a 2 mS»", fabs(y - 2.0) < 1e-12 &&
                     cv_ve(t) && cv_ve("0 a 2 mS") && cv_ve("x 200 " S_MU "S/div"));
            chk_cond("unidad de la ganancia", "g: dV/dg = 2000 V/S (no «kV»)", fabs(d - 2000.0) < 1e-9 &&
                     cv_ve("dy/dx = 2,000 kV/S"));
        }
        (void)kE;
    }

    chk_cond("interfaz", "ning" S_uu "n texto fuera de la pantalla", gfx_desbordes() == 0);
}
