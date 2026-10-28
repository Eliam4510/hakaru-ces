/* --------------------------------------------------------------------------
 *  62. Etapa 4: la pequeña señal en CA y Bode (paso 8)
 *
 *  Se incluye tras test_e4_ao.inc (usa sr, saj, sr_init, sr_el, canal,
 *  bode_sr, t4_*, a4_*, c4_*, rc, rr, cierra_red y los chk). Los números son
 *  los del §6.9 de la especificación.
 * ----------------------------------------------------------------------- */

/* E de 5 V y 1 mV de alterna en serie, R 1 kΩ y el diodo (del ánodo a tierra).
 * La entrada es la alterna; la salida, V(D). */
static int p4_diodo(const NlDiodo *cat, int *kD)
{
    int ke;
    sr_init(4);
    a4_cc(0, 1, 5.0);
    ke = t4_seno(1, 2, 1e-3, 1e3, 0.0);
    sr_el(SE_R, 2, 3, 1e3);
    if (cat) *kD = t4_d_cat(3, 0, cat);
    else {                                             /* la exponencial de omisión */
        *kD = sr_el(SE_D, 3, 0, 0.0);
        sr.el[*kD].modelo = NL_UNION;
        sr.el[*kD].m[0] = 1e-14; sr.el[*kD].m[1] = 1.0;
        sr.el[*kD].m[5] = nl_vcrit(1e-14, NL_VT);
    }
    canal(0, CANAL_V, *kD); canal(1, CANAL_NADA, 0);
    return ke;
}

static void test_e4_pq(void)
{
    static SimLin L;
    static BodeReg g1;
    static CaRes cr;
    SimDiag d;
    Cx H;
    int ke, kD, k;

    printf("[62] Etapa 4: la peque" S_nn "a se" S_nn "al en CA y Bode\n");

    /* ---- Diodo exponencial y 1N4148: H = V(D)/e y su G en Q ---- */
    ke = p4_diodo(0, &kD);
    chk_cond("peque" S_nn "a se" S_nn "al", "el punto Q", sim_punto_q(&sr, &L, &d, 0) == SIM_OK && L.hay);
    chk_t("exp", "G en Q", L.g[kD][0][0], 0.16652327567247468, 0.1665, 1e-9);
    chk_cond("exp", "se barre", bode_sr(ke, 0, -1, -1, 0.0, 0.0, 0.0) == SIM_OK && bode_res()->fin == FIN_COMPLETO);
    bode_en(1e3, &H);
    chk_t("exp", "H = V(D)/e", cx_abs(H), 0.0059693197616019841, 0.006, 1e-9);
    chk_cond("exp", "aviso PEQ_SENAL y no CC_FUERA", (bode_res()->d.avisos & SIM_AV_PEQ_SENAL) && !(bode_res()->d.avisos & SIM_AV_CC_FUERA));
    ke = p4_diodo(&NL_1N4148, &kD);
    sim_punto_q(&sr, &L, &d, 0);
    chk_t("1N4148", "G en Q", L.g[kD][0][0], 0.090966540328458143, 0.091, 1e-9);
    chk_cond("1N4148", "se barre", bode_sr(ke, 0, -1, -1, 0.0, 0.0, 0.0) == SIM_OK);
    bode_en(1e3, &H);
    chk_t("1N4148", "H = V(D)/e", cx_abs(H), 0.010873519830456858, 0.011, 1e-9);

    /* ---- La linealización del Bode es suya: un ca_analiza de otro circuito
     *      con un diodo en medio no se la cambia ---- */
    ke = p4_diodo(&NL_1N4148, &kD);
    bode_iniciar_red(&sr, ke, &saj.canal[0], -1, -1, 0.0, 0.0, 0.0);
    while (bode_avanzar(~0ul)) {}
    g1 = *bode_reg();
    bode_iniciar_red(&sr, ke, &saj.canal[0], -1, -1, 0.0, 0.0, 0.0);
    bode_avanzar(1ul);
    {
        int kf;
        c4_init(3);
        kf = c4_el(EL_FUENTE, 0, 1, 1.0);
        rc.el[kf].variante = FORMA_FASOR;
        rc.frec.v = 1e3; rc.frec.est = M_DATO;
        c4_el(EL_RES, 1, 2, 1e3);
        k = nuevo_el(EL_DIODO, 2, 0); rr.c[k] = -1; rc.el[k].variante = DIO_EXP;
        cierra_red();
        chk_cond("Bode suyo", "otro an" S_aa "lisis de CA con un diodo en medio", ca_analiza(&rc, &rr, &cr) == SIM_OK);
        chk_cond("Bode suyo", "que deja la linealizaci" S_oo "n en NULL al acabar", ca_lin() == 0);
    }
    while (bode_avanzar(~0ul)) {}
    chk_cond("Bode suyo", "el registro, bit a bit el de sin interrupci" S_oo "n", memcmp(bode_reg(), &g1, sizeof g1) == 0);
    bode_en(1e3, &H);
    chk_t("Bode suyo", "bode_en(1 kHz) con su diodo", cx_abs(H), 0.010873519830456858, 0.011, 1e-9);

    /* ---- Con una J de polarización en vez de la E: el mismo Q ---- */
    {
        double g0;
        sr_init(4);
        k = sr_el(SE_J, 0, 3, 0.0); sr.el[k].forma = ONDA_CC; sr.el[k].p[0] = 5e-3;   /* el Norton de 5 V con 1 kΩ */
        ke = t4_seno(0, 1, 1e-3, 1e3, 0.0);
        sr_el(SE_R, 1, 3, 1e3);
        kD = sr_el(SE_D, 3, 0, 0.0);
        sr.el[kD].modelo = NL_UNION; sr.el[kD].m[0] = 1e-14; sr.el[kD].m[1] = 1.0; sr.el[kD].m[5] = nl_vcrit(1e-14, NL_VT);
        sim_punto_q(&sr, &L, &d, 0);
        g0 = L.g[kD][0][0];
        chk_t("J de polarizaci" S_oo "n", "el mismo Q (G)", g0, 0.16652327567247468, 0.1665, 1e-9);
        canal(0, CANAL_V, kD);
        chk_cond("J de polarizaci" S_oo "n", "el Bode hace el Q antes de apagarla", bode_sr(ke, 0, -1, -1, 0.0, 0.0, 0.0) == SIM_OK);
        bode_en(1e3, &H);
        chk_t("J de polarizaci" S_oo "n", "H = r_d/(R + r_d)", cx_abs(H), (1.0 / g0) / (1e3 + 1.0 / g0), 0.006, 1e-9);
    }

    /* ---- El AO: el inversor real, plano; saturado en Q, H = 0 ---- */
    {
        int kR1, kRf;
        k = a4_inversor(1.0, 1, 1e5, &kR1, &kRf);
        sr.el[0].forma = ONDA_SENO; sr.el[0].p[0] = 1.0; sr.el[0].p[1] = 1e3;
        canal(0, CANAL_NODO, 3);
        chk_cond("AO", "se barre", bode_sr(0, 0, -1, -1, 0.0, 0.0, 0.0) == SIM_OK);
        bode_en(1e3, &H);
        chk_t("AO", "el inversor real: -9,9989001209866915", H.re, -9.9989001209866915, 10.0, 1e-12);
        chk_t("AO", "plano (sin parte imaginaria)", H.im, 0.0, 10.0, 1e-12);
        k = a4_inversor(2.0, 1, 1e5, &kR1, &kRf);          /* 2 V de CC: satura en Q */
        ke = t4_seno(0, 4, 1e-3, 1e3, 0.0);
        sr.nn = 5;
        sr.el[0].a = 4;                                    /* la CC y la alterna en serie */
        canal(0, CANAL_NODO, 3);
        chk_cond("AO saturado", "se barre", bode_sr(ke, 0, -1, -1, 0.0, 0.0, 0.0) == SIM_OK);
        bode_en(1e3, &H);
        chk_t("AO saturado", "H = 0", cx_abs(H), 0.0, 1.0, 1e-12);
        chk_cond("AO saturado", "aviso SATURA", (bode_res()->d.avisos & SIM_AV_SATURA) != 0);
    }

    /* ---- Estado intacto con sim_punto_q ---- */
    {
        static SimRes r0;
        static SimRed red0, otra;
        int kS, kL, kC;
        sr_init(5);
        sr_escalon(0, 1, 10.0);
        kS = sr_el(SE_S, 1, 2, 0.0); sr.el[kS].t1 = 1e-3;
        sr_el(SE_R, 2, 3, 100.0);
        kL = sr_el(SE_L, 3, 4, 10e-3);
        kC = sr_el(SE_C, 4, 0, 1e-6);
        canal(0, CANAL_V, kC); canal(1, CANAL_I, kL);
        saj.t_fin = 5e-3; saj.h = 0.0;
        sim_iniciar_red(&sr, &saj);
        sim_avanzar(20000ul);
        red0 = *sim_red(); r0 = *sim_res();
        otra = sr;
        ke = p4_diodo(&NL_1N4148, &kD);
        sim_punto_q(&sr, &L, &d, 0);
        sr = otra;
        chk_cond("estado intacto", "sim_punto_q no toca la emulaci" S_oo "n",
                 memcmp(&red0, sim_red(), sizeof red0) == 0 && memcmp(&r0, sim_res(), sizeof r0) == 0);
    }
}
