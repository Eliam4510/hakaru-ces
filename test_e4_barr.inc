/* --------------------------------------------------------------------------
 *  63. Etapa 4: el barrido de CC (paso 9; las familias, pasos 13 y 14)
 *
 *  Se incluye tras test_e4_pq.inc (usa sr, sr_init, sr_el, a4_*, t4_*, c4_*,
 *  rc, rr, cierra_red y los chk). Los números son los del §6.10.
 * ----------------------------------------------------------------------- */
#include "barrido.h"

static SimCanal b4_ch[2];

static void b4_canal(int c, int tipo, int elem, int nodo, int sg)
{
    memset(&b4_ch[c], 0, sizeof b4_ch[c]);
    b4_ch[c].tipo = (unsigned char)tipo; b4_ch[c].elem = (signed char)elem;
    b4_ch[c].nodo = (signed char)nodo; b4_ch[c].sg = (signed char)sg;
}

static int b4_barre(const char *q, int el, int bx, double x0, double x1, int log_x, int vuelta)
{
    int e = barr_iniciar_red(&sr, el, bx, b4_ch, x0, x1, log_x, vuelta);
    if (e == SIM_OK) while (barr_avanzar(~0ul)) {}
    pruebas++;
    if (e != SIM_OK || barr_res()->fin != FIN_COMPLETO) { falla(q, "se barre", barr_res()->d.estado, SIM_OK); return 0; }
    return 1;
}

static void test_e4_barr(void)
{
    static BarrReg g0;
    double y, dy, x, y1, y2;
    int kD, k, i, sg;

    printf("[63] Etapa 4: el barrido de CC\n");

    /* ---- Divisor ---- */
    sr_init(3); k = a4_cc(0, 1, 1.0); sr_el(SE_R, 1, 2, 1e3); i = sr_el(SE_R, 2, 0, 3e3);
    b4_canal(0, CANAL_V, i, -1, 1); b4_canal(1, CANAL_NADA, -1, -1, 1);
    if (b4_barre("divisor", k, BX_E, 0.0, 10.0, 0, 0)) {
        double peor = 0.0;
        for (i = 0; i < barr_reg()->n; i++) {
            double e = fabs(barr_reg()->y[0][i] - 0.75 * barr_reg()->x[i]);
            if (e > peor) peor = e;
        }
        chk_t("divisor", "y = 0,75 x", peor, 0.0, 10.0, 1e-15);
        barr_en(0, 3.3, &y, &dy);
        chk_t("divisor", "dy/dx = 0,75", dy, 0.75, 0.75, 1e-12);
    }

    /* ---- 1N4148 con 1 kΩ ---- */
    sr_init(3); k = a4_cc(0, 1, 1.0); i = sr_el(SE_R, 1, 2, 1e3); kD = t4_d_cat(2, 0, &NL_1N4148);
    b4_canal(0, CANAL_I, kD, -1, 1); b4_canal(1, CANAL_V, kD, -1, 1);
    if (b4_barre("1N4148", k, BX_E, 0.0, 5.0, 0, 0)) {
        barr_en(0, 5.0, &y, &dy);
        chk_t("1N4148", "barr_en(5) = 4,3467715385002356 mA", y, 4.3467715385002356e-3, 4.3e-3, 1e-12);
        chk_t("1N4148", "dI/dE = 0,00098912648016954314", dy, 0.00098912648016954314, 1e-3, 1e-9);
        chk_cond("1N4148", "barr_nivel(1 mA)", barr_nivel(0, 1e-3, 0.0, 5.0, &x));
        chk_t("1N4148", "E = 1,5847396946292637 V", x, 1.5847396946292637, 1.58, 1e-9);
        barr_en(0, 2.0 + 1e-6, &y1, 0); barr_en(0, 2.0 - 1e-6, &y2, 0); barr_en(0, 2.0, &y, &dy);
        chk_t("1N4148", "dy/dx exacta frente a diferencias centradas", dy, (y1 - y2) / 2e-6, fabs(dy), 1e-7);
    }

    /* ---- Cruce con la pequeña señal: dV(D)/dE = la H del Bode ---- */
    sr_init(3); k = a4_cc(0, 1, 1.0); sr_el(SE_R, 1, 2, 1e3);
    kD = sr_el(SE_D, 2, 0, 0.0);
    sr.el[kD].modelo = NL_UNION; sr.el[kD].m[0] = 1e-14; sr.el[kD].m[1] = 1.0; sr.el[kD].m[5] = nl_vcrit(1e-14, NL_VT);
    b4_canal(0, CANAL_V, kD, -1, 1); b4_canal(1, CANAL_NADA, -1, -1, 1);
    if (b4_barre("cruce con el Bode", k, BX_E, 0.0, 6.0, 0, 0)) {
        barr_en(0, 5.0, &y, &dy);
        chk_t("cruce con el Bode", "dV(D)/dE en 5 V = H", dy, 0.0059693197616019841, 0.006, 1e-9);
    }

    /* ---- Derivadas con controladas por la I de lo barrido (§3.J (2)) ---- */
    sr_init(3); a4_cc(0, 1, 1.0); k = sr_el(SE_R, 1, 0, 1e3);
    i = sr_el(SE_EC, 0, 2, 5e3); sr.el[i].ctl = (signed char)k; sr.el[i].ctl_i = 1;
    i = sr_el(SE_R, 2, 0, 1e3);
    b4_canal(0, CANAL_V, i, -1, 1);
    if (b4_barre("CCVS por la I de R1", k, BX_R, 500.0, 2000.0, 0, 0)) {
        barr_en(0, 1000.0, &y, &dy);
        chk_t("CCVS por la I de R1", "V(RL) = 5000/R1", y, 5.0, 5.0, 1e-12);
        chk_t("CCVS por la I de R1", "dy/dx = -5e-3 V/" S_OHM, dy, -5e-3, 5e-3, 1e-12);
    }
    sr_init(3);
    k = sr_el(SE_J, 0, 1, 0.0); sr.el[k].forma = ONDA_CC;
    sr_el(SE_R, 1, 0, 1e3);
    i = sr_el(SE_JC, 0, 2, 2.0); sr.el[i].ctl = (signed char)k; sr.el[i].ctl_i = 1;
    i = sr_el(SE_R, 2, 0, 1e3);
    b4_canal(0, CANAL_V, i, -1, 1);
    if (b4_barre("CCCS por la I de J1", k, BX_J, 0.0, 2e-3, 0, 0)) {
        barr_en(0, 1e-3, &y, &dy);
        chk_t("CCCS por la I de J1", "dV(R2)/dJ1 = 2000 " S_OHM, dy, 2000.0, 2000.0, 1e-12);
    }
    sr_init(3);
    k = a4_cc(0, 1, 1.0); sr.el[k].val = 50.0;          /* E real: r = 50 Ω */
    sr_el(SE_R, 1, 0, 1e3);
    i = sr_el(SE_JC, 0, 2, 3.0); sr.el[i].ctl = (signed char)k; sr.el[i].ctl_i = 1;
    i = sr_el(SE_R, 2, 0, 1e3);
    b4_canal(0, CANAL_V, i, -1, 1);
    if (b4_barre("CCCS por la I de una E real", k, BX_E, 0.0, 10.0, 0, 0)) {
        barr_en(0, 5.0 + 1e-4, &y1, 0); barr_en(0, 5.0 - 1e-4, &y2, 0); barr_en(0, 5.0, &y, &dy);
        chk_t("CCCS por la I de una E real", "D = 1/r frente a diferencias centradas", dy, (y1 - y2) / 2e-4, fabs(dy), 1e-9);
    }

    /* ---- El inversor real: pendiente, lecturas y saturación ---- */
    {
        int kR1, kRf;
        k = a4_inversor(0.0, 1, 1e5, &kR1, &kRf);
        b4_canal(0, CANAL_NODO, -1, 3, 1); b4_canal(1, CANAL_NADA, -1, -1, 1);
        if (b4_barre("inversor", 0, BX_E, -2.0, 2.0, 0, 0)) {
            barr_en(0, 0.0, &y, &dy);
            chk_t("inversor", "dy/dx(0)", dy, -9.9989001209866915, 10.0, 1e-12);
            barr_en(0, 0.5, &y, 0);
            chk_t("inversor", "y(0,5)", y, -4.9994500604933457, 5.0, 1e-12);
            chk_cond("inversor", "barr_nivel(-12)", barr_nivel(0, -12.0, -2.0, 2.0, &x));
            chk_t("inversor", "E1 = 1,200132", x, 1.200132, 1.2, 1e-9);
            chk_cond("inversor", "barr_nivel(-15) (satura desde 1,500165)", barr_nivel(0, -15.0 + 1e-9, 0.0, 2.0, &x) && fabs(x - 1.500165) < 1e-6);
        }
    }

    /* ---- El Schmitt: ida y vuelta, saltos, una continuación por traza ---- */
    sr_init(4);
    k = a4_cc(0, 1, 0.0);
    sr_el(SE_R, 3, 2, 10e3); sr_el(SE_R, 2, 0, 10e3);
    a4_ao(2, 1, 3, 0, 0, 0.0);
    b4_canal(0, CANAL_NODO, -1, 3, 1); b4_canal(1, CANAL_NADA, -1, -1, 1);
    if (b4_barre("Schmitt", k, BX_E, -10.0, 10.0, 0, 1)) {
        const BarrReg *g = barr_reg();
        int ida = -1, vuelta = -1, t;
        for (t = 0; t < g->ntr; t++) { if (g->recorrido[t] == 0) ida = t; else vuelta = t; }
        chk_cond("Schmitt", "dos trazas", ida >= 0 && vuelta >= 0);
        if (ida >= 0 && vuelta >= 0) {
            int n_si = 0, n_sv = 0, j_i = -1, j_v = -1;
            for (i = 0; i < g->n; i++) {
                if (g->salto[ida][i]) { n_si++; j_i = i; }
                if (g->salto[vuelta][i]) { n_sv++; j_v = i; }
            }
            chk_cond("Schmitt", "la ida salta una vez, tras x = 7,5", n_si == 1 && g->x[j_i] > 7.5 && g->x[j_i - 1] <= 7.5 &&
                     g->y[ida][j_i] < -14.9 && g->y[ida][j_i - 1] > 14.9);
            chk_cond("Schmitt", "la vuelta salta una vez, antes de -7,5", n_sv == 1 && g->x[j_v] < -7.5 && g->x[j_v + 1] >= -7.5 &&
                     g->y[vuelta][j_v] > 14.9 && g->y[vuelta][j_v + 1] < -14.9);
            chk_cond("Schmitt", "el aviso SALTO", (barr_res()->d.avisos & SIM_AV_SALTO) != 0);
            barr_en(ida, 0.0, &y1, 0); barr_en(vuelta, 0.0, &y2, 0);
            chk_cond("Schmitt", "en 0: +15 en la ida y -15 en la vuelta (cada una desde su continuaci" S_oo "n)",
                     fabs(y1 - 15.0) < 1e-9 && fabs(y2 + 15.0) < 1e-9);
        }
    }

    /* ---- El tope por punto: con 3 iteraciones, la rodilla del diodo pide
     *      trozos; con 29 000 (el tope cuenta 463 por iteración, lo fijo de
     *      NS_ITER: eran 5000 operaciones solas, unas 50 iteraciones del
     *      diodo, de ~100 cada una) no caben: huecos y la curva sigue ---- */
    sr_init(3); k = a4_cc(0, 1, 1.0); sr_el(SE_R, 1, 2, 1e3); kD = t4_d_cat(2, 0, &NL_1N4148);
    b4_canal(0, CANAL_I, kD, -1, 1); b4_canal(1, CANAL_NADA, -1, -1, 1);
    {
        int h_sin, n_ok;
        unsigned long peor = 0, o0, p1 = 0;
        sim_nl_topes(3, 0);
        barr_iniciar_red(&sr, k, BX_E, b4_ch, 0.0, 50.0, 0, 0);
        do { o0 = barr_ops(); if (!barr_avanzar(1ul)) break; if (barr_ops() - o0 > p1) p1 = barr_ops() - o0; } while (1);
        h_sin = barr_res()->huecos;
        chk_cond("tope por punto", "sin tope: los trozos salvan todos los puntos (y alguno pasa de 5000)", h_sin == 0 && p1 > 5000ul &&
                 barr_res()->fin == FIN_COMPLETO);
        sim_nl_tope_ops(29000);
        barr_iniciar_red(&sr, k, BX_E, b4_ch, 0.0, 50.0, 0, 0);
        do { o0 = barr_ops(); if (!barr_avanzar(1ul)) break; if (barr_ops() - o0 > peor) peor = barr_ops() - o0; } while (1);
        for (n_ok = 0, i = 0; i < barr_reg()->n; i++) n_ok += barr_reg()->ok[0][i];
        chk_cond("tope por punto", "con 29 000: huecos (HUECOS) y la curva sigue hasta el final",
                 barr_res()->huecos >= 1 && barr_res()->fin == FIN_COMPLETO && (barr_res()->d.avisos & SIM_AV_HUECOS) &&
                 n_ok + barr_res()->huecos == barr_reg()->n && barr_reg()->ok[0][barr_reg()->n - 1]);
        chk_cond("tope por punto", "ninguna llamada a barr_avanzar(1) pasa del tope m" S_aa "s un punto", peor <= 2ul * 29000ul);
        sim_nl_tope_ops(0); sim_nl_topes(0, 0);
    }

    /* ---- Trozos: 1, 997 e ilimitadas operaciones dan lo mismo ---- */
    sr_init(3); k = a4_cc(0, 1, 1.0); sr_el(SE_R, 1, 2, 1e3); kD = t4_d_cat(2, 0, &NL_1N4148);
    b4_canal(0, CANAL_I, kD, -1, 1); b4_canal(1, CANAL_V, kD, -1, 1);
    b4_barre("trozos", k, BX_E, 0.0, 5.0, 0, 0);
    g0 = *barr_reg();
    {
        static const unsigned long OPS[2] = { 1ul, 997ul };
        int t;
        for (t = 0; t < 2; t++) {
            barr_iniciar_red(&sr, k, BX_E, b4_ch, 0.0, 5.0, 0, 0);
            while (barr_avanzar(OPS[t])) {}
            chk_cond("trozos", t ? "997 operaciones: bit a bit" : "1 operaci" S_oo "n: bit a bit", memcmp(barr_reg(), &g0, sizeof g0) == 0);
        }
        barr_en(0, g0.x[100], &y, 0);
        chk_cond("trozos", "barr_en en un punto de la malla es la malla", y == g0.y[0][100]);
        {
            static Valores v;
            SimDiag d;
            barr_valores(0, g0.x[100], &v, &d);
            chk_cond("trozos", "barr_valores da los mismos canales", v.I[kD] == g0.y[0][100] && (v.phi[2] - v.phi[0]) == g0.y[1][100]);
        }
    }

    /* ---- Huecos con el gancho ---- */
    sim_nl_prueba(NLP_FALLA_BARR, g0.x[50]);
    barr_iniciar_red(&sr, k, BX_E, b4_ch, 0.0, 5.0, 0, 0);
    while (barr_avanzar(~0ul)) {}
    sim_nl_prueba(NLP_NADA, 0.0);
    chk_cond("huecos", "un hueco en x con HUECOS y la curva sigue", !barr_reg()->ok[0][50] && barr_reg()->ok[0][51] &&
             barr_res()->huecos == 1 && (barr_res()->d.avisos & SIM_AV_HUECOS) && barr_res()->x_fallo == g0.x[50]);

    /* ---- El LED rojo con E = 9 V: R de 10 Ω a 10 kΩ (logarítmico) ---- */
    sr_init(3); a4_cc(0, 1, 9.0); k = sr_el(SE_R, 1, 2, 1e3);
    {
        static SimRed s2;
        SimDiag d;
        c4_init(3);
        c4_el(EL_FUENTE, 0, 1, 9.0); c4_el(EL_RES, 1, 2, 1e3);
        kD = nuevo_el(EL_LED, 2, 0); rr.c[kD] = -1;
        cierra_red();
        sim_red_de(&rc, &rr, &s2, &d);
        sr.el[2] = s2.el[kD]; sr.el[2].k = -1; sr.nel = 3;
    }
    b4_canal(0, CANAL_I, 2, -1, 1); b4_canal(1, CANAL_NADA, -1, -1, 1);
    if (b4_barre("LED rojo", k, BX_R, 10.0, 10e3, 1, 0)) {
        chk_cond("LED rojo", "barr_nivel(20 mA)", barr_nivel(0, 20e-3, 10.0, 10e3, &x));
        chk_t("LED rojo", "R = 360 " S_OHM, x, 360.0, 360.0, 1e-9);
        barr_en(0, 330.0, &y, 0);
        chk_t("LED rojo", "en 330 " S_OHM ": 21,777894098247443 mA", y, 21.777894098247443e-3, 0.02, 1e-9);
    }

    /* ---- El trazador del 1N4148 (barr_iniciar_curva) ---- */
    c4_init(2);
    kD = nuevo_el(EL_DIODO, 1, 0); rr.c[kD] = -1; rc.el[kD].variante = DIO_1N4148;
    if (barr_iniciar_curva(&rc, kD) == SIM_OK) {
        static const double I[3] = { 1e-3, 10e-3, 100e-3 };
        static const double V[3] = { 0.58473969462926372, 0.69419404121982237, 0.84965648028731514 };
        int tv = -1, ti = -1, t;
        while (barr_avanzar(~0ul)) {}
        for (t = 0; t < barr_reg()->ntr; t++) { if (barr_reg()->canal[t] == 0) tv = t; else ti = t; }
        chk_cond("trazador", "la curva I-V (X-Y)", barr_res()->xy && tv >= 0 && ti >= 0);
        for (i = 0; i < 3 && tv >= 0 && ti >= 0; i++) {
            double e;
            if (!barr_nivel(ti, I[i], barr_res()->x0, barr_res()->x1, &e)) { chk_cond("trazador", "el nivel de I", 0); continue; }
            barr_en(tv, e, &y, 0);
            chk_t("trazador", "V del 1N4148", y, V[i], V[i], 1e-9);
        }
    } else chk_cond("trazador", "barr_iniciar_curva", 0);

    /* ---- Con el Circuito: el canal «Vsal(U1)» y el nivel de un dato con signo ---- */
    {
        int ka, t;
        c4_init(4);
        k = c4_el(EL_FUENTE, 0, 1, 1.0); c4_el(EL_RES, 1, 2, 1e3); c4_el(EL_RES, 2, 3, 10e3);
        ka = nuevo_el(EL_AO, 0, 2); rr.c[ka] = 3; rc.el[ka].variante = AO_REAL;
        for (t = 0; t < rc.nel; t++) if (t != ka) rr.c[t] = -1;
        cierra_red();
        {
            BarrAjustes aj;
            memset(&aj, 0, sizeof aj);
            aj.el = (signed char)k; aj.x0 = 0.0; aj.x1 = 2.0;
            chk_cond("canal Vsal", "barr_iniciar con el circuito", barr_iniciar(&rc, &rr, &aj) == SIM_OK);
            while (barr_avanzar(~0ul)) {}
            chk_cond("canal Vsal", "el canal 1 es Vsal(U1)", strncmp(barr_res()->canal[0].nombre, "Vsal(", 5) == 0);
            barr_en(0, 0.5, &y, 0);
            chk_t("canal Vsal", "vale Vsal (no Vd)", y, -4.9994500604933457, 5.0, 1e-12);
            rc.el[ka].V.v = 12.0; rc.el[ka].V.est = M_DATO;     /* el dato V = 12 V en su ficha */
            barr_iniciar(&rc, &rr, &aj);
            while (barr_avanzar(~0ul)) {}
            chk_cond("nivel de un dato", "la traza del dato", barr_res()->tr_dato >= 0 && barr_res()->nivel_dato == 12.0);
            chk_cond("nivel de un dato", "barr_nivel_dato", barr_nivel_dato(barr_res()->tr_dato, 12.0, 0.0, 2.0, &x, &sg));
            chk_t("nivel de un dato", "E1 = 1,200132", x, 1.200132, 1.2, 1e-9);
            chk_cond("nivel de un dato", "con signo -1", sg == -1);
        }
    }

    /* ---- Lo automático: el elemento, el rango, los canales y la vista de CC ---- */
    {
        int kE, kR, kCc;
        BarrAjustes aj;
        c4_init(3);
        kE = c4_el(EL_FUENTE, 0, 1, 5.0); kR = c4_el(EL_RES, 1, 2, 1e3);
        kD = nuevo_el(EL_DIODO, 2, 0); rr.c[kD] = -1; rc.el[kD].variante = DIO_1N4148;
        kCc = c4_el(EL_COND, 2, 0, 1e-6);
        cierra_red();
        memset(&aj, 0, sizeof aj); aj.el = -1;
        chk_cond("autom" S_aa "tico", "barr_iniciar", barr_iniciar(&rc, &rr, &aj) == SIM_OK);
        while (barr_avanzar(~0ul)) {}
        chk_cond("autom" S_aa "tico", "la primera E continua, de -5 a 5 V", barr_res()->el == kE && barr_res()->bx == BX_E &&
                 barr_res()->x0 == -5.0 && barr_res()->x1 == 5.0 && !barr_res()->log_x);
        chk_cond("autom" S_aa "tico", "los canales: I y V del diodo", barr_res()->canal[0].tipo == CANAL_I && barr_res()->canal[0].elem == kD &&
                 barr_res()->canal[1].tipo == CANAL_V && strcmp(barr_res()->canal[0].nombre, "I(D3)") == 0);
        chk_cond("autom" S_aa "tico", "con un C: la vista de CC (CC_BARRIDO) y el C abierto",
                 (barr_res()->d.avisos & SIM_AV_CC_BARRIDO) && barr_en(0, 5.0, &y, 0) && fabs(y - 4.3467715385002356e-3) < 1e-12);
        rc.el[kR].val.est = M_BUSCAR;                  /* R2 con [?!] (sin valor): se barre ella, de 1 ohm a 1 Mohm, log */
        barr_iniciar(&rc, &rr, &aj);
        while (barr_avanzar(~0ul)) {}
        chk_cond("autom" S_aa "tico", "la R con [?!], de 1 " S_OHM " a 1 M" S_OHM " (log)", barr_res()->el == kR && barr_res()->bx == BX_R &&
                 barr_res()->x0 == 1.0 && barr_res()->x1 == 1e6 && barr_res()->log_x && barr_res()->fin == FIN_COMPLETO);
        (void)kCc;
    }

    /* ---- Tope: con 32 elementos (8 diodos), t_est <= 3,5 s ---- */
    {
        int j, n;
        sr_init(10);
        k = a4_cc(0, 1, 5.0);
        for (j = 1; j <= 8; j++) {
            n = j + 1;
            sr_el(SE_R, n - 1, n, 1e3);
            t4_d_cat(n, 0, &NL_1N4148);
            sr_el(SE_R, n, 0, 10e3);
            if (j <= 7) sr_el(SE_R, n, 0, 100e3);
        }
        chk_cond("t_est", "32 elementos", sr.nel == 32);
        b4_canal(0, CANAL_NODO, -1, 9, 1); b4_canal(1, CANAL_NADA, -1, -1, 1);
        if (b4_barre("t_est", k, BX_E, 0.0, 10.0, 0, 0)) {
            if (getenv("HAKARU_DEPURA")) printf("t_est %.3f s, n %d\n", barr_res()->t_est, barr_reg()->n);
            chk_cond("t_est", "<= 3,5 s", barr_res()->t_est <= 3.5);
        }
    }

    /* ---- Estado intacto: una emulación a medias (un rectificador, con la
     *      caché del transitorio llena) no se entera de sim_cc_punto ni de un
     *      barrido entero con sus lecturas ---- */
    {
        static SimRes res1, r0;
        static SimReg reg1;
        static SimRed red0, emu;
        static SimCc io;
        static Valores v;
        SimDiag d;
        int kC, kDr, e;
        sr_init(3);
        t4_seno(0, 1, 10.0, 50.0, 0.0);
        kDr = t4_d_vg(1, 2, 0.7);
        kC = t4_c0(2, 0, 100e-6);
        sr_el(SE_R, 2, 0, 1e3);
        canal(0, CANAL_V, kC); canal(1, CANAL_I, kDr);
        saj.t_fin = 40e-3; saj.h = 0.0;
        emu = sr;
        chk_cond("estado intacto", "la emulaci" S_oo "n de referencia", sim_emular(&sr, &saj) == SIM_OK);
        res1 = *sim_res(); reg1 = *sim_reg();
        e = sim_iniciar_red(&emu, &saj);
        chk_cond("estado intacto", "empieza", e == SIM_OK && sim_avanzar(20000ul));
        red0 = *sim_red(); r0 = *sim_res();
        /* un diodo exponencial punto a punto, y un Schmitt entero (tramos del AO) */
        sr_init(3); k = a4_cc(0, 1, 1.0); sr_el(SE_R, 1, 2, 1e3); t4_d_cat(2, 0, &NL_1N4148);
        memset(&io, 0, sizeof io);
        sim_cc_punto(&sr, k, BX_E, 3.0, &io, &d);
        sim_cc_punto(&sr, k, BX_E, 3.5, &io, &d);
        sr_init(4);
        k = a4_cc(0, 1, 0.0);
        sr_el(SE_R, 3, 2, 10e3); sr_el(SE_R, 2, 0, 10e3);
        a4_ao(2, 1, 3, 0, 0, 0.0);
        b4_canal(0, CANAL_NODO, -1, 3, 1); b4_canal(1, CANAL_NADA, -1, -1, 1);
        b4_barre("estado intacto", k, BX_E, -10.0, 10.0, 0, 1);
        barr_en(1, 0.0, &y, &dy);
        barr_nivel(0, 0.0, -10.0, 10.0, &x);
        barr_valores(0, 1.0, &v, &d);
        chk_cond("estado intacto", "sim_red() y sim_res() sin tocar",
                 memcmp(&red0, sim_red(), sizeof red0) == 0 && memcmp(&r0, sim_res(), sizeof r0) == 0);
        while (sim_avanzar(~0ul)) {}
        chk_cond("estado intacto", "acaba", sim_res()->fin == FIN_COMPLETO);
        chk_cond("estado intacto", "el registro, bit a bit el de un tir" S_oo "n",
                 sim_reg()->n == reg1.n && memcmp(sim_reg()->t, reg1.t, sizeof reg1.t) == 0 &&
                 memcmp(sim_reg()->y, reg1.y, sizeof reg1.y) == 0);
        chk_cond("estado intacto", "sim_res(), bit a bit el de un tir" S_oo "n", memcmp(sim_res(), &res1, sizeof res1) == 0);
    }
}
