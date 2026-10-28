/* --------------------------------------------------------------------------
 *  64. Etapa 4: el MOSFET en el motor (paso 14)
 *
 *  El MOSFET (Shichman-Hodges, nivel 1, con modulación del canal y
 *  simétrico, D10) con el motor de verdad: el punto de operación de los
 *  casos cerrados del §6.11 (mpmath en e4juez/n2.py y e4juez2/r4_mos_inv.py;
 *  los mismos que test_nolin comprueba con su Newton propio), el MOSFET
 *  dibujado al revés (nl_mos_lim, el modo inverso de mos1load), sus dos
 *  puertos en Valores (V = VDS, I = ID, V2 = VGS, I2 = IG = 0), las regiones,
 *  el adaptador, el balance del transitorio, la pequeña señal (la fuente
 *  común), la familia del trazador (§6.10, [63]), sus tiempos ([14]) y la
 *  interfaz ([65]: la tecla [M], la pieza 22, la ficha, la página Modelo con
 *  su familia y la Ayuda). Un elemento de cada tipo, con el BJT y el MOSFET,
 *  va en test_e4_res.inc (r4_mos).
 *  Se incluye tras test_e4_bjt.inc (usa sr, saj, sr_init, sr_el, canal,
 *  emula, a4_cc, t4_seno, t4_balance, t4_trozos, q4_op, q4_rel, Q4_NS_OP,
 *  bode_sr, c4_*, r4_*, r3u_abre, u4_flecha y los chk).
 * ----------------------------------------------------------------------- */

/* Un MOSFET en sr: el drenador en nd, la puerta en ng y el surtidor en ns */
static int m4_mos(int nd, int ng, int ns, int pol, double vt, double K, double vaf)
{
    int k = sr_el(SE_MOS, nd, ns, 0.0);
    SimEl *x = &sr.el[k];
    x->c = (short)ng;
    x->modelo = NL_MOS;
    x->pol = (signed char)pol;
    x->m[0] = vt; x->m[1] = K; x->m[2] = vaf;
    return k;
}

/* La ley de cada MOSFET en los Valores: ID = nl_mos(VGS, VDS), VGS la de sus
 * patas, la puerta sin corriente (I2 = 0) y P = VDS·ID */
static void m4_ley(const char *q, const SimRed *s, const Valores *v)
{
    double peor = 0.0, t;
    int k;
    for (k = 0; k < s->nel; k++) {
        const SimEl *e = &s->el[k];
        NlMos o;
        if (e->tipo != SE_MOS) continue;
        nl_mos(e, v->V2[k], v->V[k], &o);
        t = fabs(v->I[k] - o.id) / (fabs(o.id) + 1e-300); if (t > peor) peor = t;
        t = fabs(v->V2[k] - (v->phi[e->c] - v->phi[e->b])); if (t > peor) peor = t;
        t = fabs(v->P[k] - v->V[k] * v->I[k]); if (t > peor) peor = t;
        if (v->I2[k] != 0.0) peor = 1.0;
    }
    pruebas++;
    if (!(peor <= 1e-10)) falla(q, "ley_de del MOSFET (nl_mos en VGS y VDS; IG = 0; P = VDS" S_CDOT "ID) <= 1e-10", peor, 0.0);
}

/* El punto de operación de sr con los criterios de q4_op (la ley de nodos y
 * el residuo del motor) y la ley del MOSFET */
static int m4_op(const char *q, Valores *v, SimNewton *nw)
{
    if (!q4_op(q, v, nw)) return 0;
    m4_ley(q, &sr, v);
    return 1;
}

/* El caso a) del §6.11: 1 VDD, 2 D, 3 G; VGS con una E, RD de VDD a D. En el
 * canal P, las dos fuentes al revés */
static int m4_caso_a(int pol, double vaf)
{
    sr_init(4);
    a4_cc(0, 1, pol * 12.0);
    a4_cc(0, 3, pol * 4.0);
    sr_el(SE_R, 1, 2, 2e3);
    return m4_mos(2, 3, 0, pol, 2.0, 1e-3, vaf);
}

/* La fuente común: VDD 12 V; el divisor de 200 kΩ y 100 kΩ (VG = 4 V); RS 1
 * kΩ con CS 100 µF; RD 2 kΩ. ID = K·(VG - ID·RS - Vt)²: 1 mA, VGS = 3 V y
 * VDS = 9 V (más lo de GMIN). Nodos: 1 VDD, 2 G, 3 S, 4 D, 5 la entrada (la E
 * de la señal la pone quien llama), con Cin 1 µF de 5 a G. Devuelve el MOSFET */
#define M4_R1  200e3
#define M4_R2  100e3
#define M4_RD  2e3
#define M4_RS  1e3
#define M4_CS  100e-6
#define M4_CIN 1e-6
static int m4_fc(void)
{
    sr_init(6);
    a4_cc(0, 1, 12.0);
    sr_el(SE_R, 1, 2, M4_R1);
    sr_el(SE_R, 2, 0, M4_R2);
    sr_el(SE_R, 1, 4, M4_RD);
    sr_el(SE_R, 3, 0, M4_RS);
    sr_el(SE_C, 3, 0, M4_CS);
    sr_el(SE_C, 5, 2, M4_CIN);
    return m4_mos(4, 2, 3, 1, 2.0, 1e-3, 0.0);
}

/* ---- [64] el punto de operación, los puertos, las regiones y el tiempo ---- */
static void m4_motor(void)
{
    static Valores v, w;
    static Circuito c;
    static Red r;
    static SimRed s;
    SimNewton nw;
    SimDiag d;
    int k, i;
    const char *q;

    /* ---- a) VGS = 4 V (una E), RD 2 kΩ, VDD 12 V: saturado ---- */
    q = "MOSFET a)";
    k = m4_caso_a(1, 0.0);
    if (m4_op(q, &v, &nw)) {
        q4_rel(q, "ID = 4,000000004 mA", v.I[k], 4.000000004e-3, 1e-9);
        q4_rel(q, "VDS = 3,999999992 V", v.V[k], 3.999999992, 1e-9);
        chk_cond(q, "V2 = VGS = 4 V e I2 = IG = 0", fabs(v.V2[k] - 4.0) <= 1e-12 && v.I2[k] == 0.0);
        chk_cond(q, "regi" S_oo "n SATURACION", v.reg[k] == REG_SATURACION);
        chk_cond(q, "sin homotop" S_ii "a, en menos de 10 iteraciones", nw.homotopia == 0 && nw.iter < 10);
        chk_cond(q, "sim_op_red (el OP de la emulaci" S_oo "n): lo mismo", sim_op_red(&sr, &w, &d) == SIM_OK &&
                 fabs(w.I[k] - v.I[k]) <= 1e-12 * v.I[k] && fabs(w.V[k] - v.V[k]) <= 1e-12 * v.V[k]);
    }
    k = m4_caso_a(1, 50.0);
    if (m4_op("MOSFET a) con VA 50 V", &v, &nw)) {
        q4_rel("MOSFET a) con VA 50 V", "ID = 4,2758620719381688 mA", v.I[k], 4.2758620719381688e-3, 1e-9);
        q4_rel("MOSFET a) con VA 50 V", "VDS = 3,4482758561236623 V", v.V[k], 3.4482758561236623, 1e-9);
    }

    /* ---- El canal P: el caso a) con -12 V y -4 V, lo mismo cambiado de signo ---- */
    q = "MOSFET canal P";
    k = m4_caso_a(1, 0.0);
    m4_op(q, &v, &nw);
    k = m4_caso_a(-1, 0.0);
    if (m4_op(q, &w, &nw)) {
        q4_rel(q, "ID = -4,000000004 mA", w.I[k], -4.000000004e-3, 1e-9);
        q4_rel(q, "ID, VDS y VGS: las del canal N con el signo cambiado", w.I[k], -v.I[k], 1e-12);
        q4_rel(q, "VDS", w.V[k], -v.V[k], 1e-12);
        q4_rel(q, "VGS", w.V2[k], -v.V2[k], 1e-12);
        chk_cond(q, "regi" S_oo "n SATURACION", w.reg[k] == REG_SATURACION);
    }

    /* ---- b) VG = 6 V, RS 1 kΩ, RD 2 kΩ: 1 VDD, 2 D, 3 G, 4 S ---- */
    q = "MOSFET b)";
    sr_init(5);
    a4_cc(0, 1, 12.0);
    a4_cc(0, 3, 6.0);
    sr_el(SE_R, 1, 2, 2e3);
    sr_el(SE_R, 4, 0, 1e3);
    k = m4_mos(2, 3, 4, 1, 2.0, 1e-3, 0.0);
    if (m4_op(q, &v, &nw)) {
        q4_rel(q, "ID = 2,4384471883273663 mA", v.I[k], 2.4384471883273663e-3, 1e-9);
        q4_rel(q, "VGS = 3,5615528116726337 V", v.V2[k], 3.5615528116726337, 1e-9);
        q4_rel(q, "VDS = 4,6846584350179011 V", v.V[k], 4.6846584350179011, 1e-9);
        chk_cond(q, "regi" S_oo "n SATURACION", v.reg[k] == REG_SATURACION);
    }

    /* ---- El MOSFET dibujado al revés (§6.11): la pata D a tierra, la E de
     *      4 V entre G y tierra y RD de 12 V a la pata S. Con el modo inverso
     *      de mos1load, Newton sin homotopía; sin él, limvds recortaría vDS a
     *      -0,5 V en cada iteración y no convergería nunca ---- */
    q = "MOSFET dibujado al rev" S_ee "s";
    sr_init(4);                                          /* 1 VDD, 2 la pata S, 3 G */
    a4_cc(0, 1, 12.0);
    a4_cc(0, 3, 4.0);
    sr_el(SE_R, 1, 2, 2e3);
    k = m4_mos(0, 3, 2, 1, 2.0, 1e-3, 0.0);
    if (m4_op(q, &v, &nw)) {
        q4_rel(q, "ID = -4,000000004 mA", v.I[k], -4.000000004e-3, 1e-9);
        q4_rel(q, "VDS = -3,999999992 V", v.V[k], -3.999999992, 1e-9);
        chk_cond(q, "por Newton sin homotop" S_ii "a y en menos de 10 iteraciones", nw.homotopia == 0 && nw.iter < 10);
        chk_cond(q, "regi" S_oo "n SATURACION (con D y S cambiados)", v.reg[k] == REG_SATURACION);
        printf("    MOSFET dibujado al rev" S_ee "s: %ld iteraciones de Newton (homotop" S_ii "a %d)\n", nw.iter, nw.homotopia);
    }

    /* ---- Los puntos del modelo con el motor (VA 50 V): dos E ideales en G
     *      y D; la I es la de la tabla (valores_de) ---- */
    {
        static const double VGS[5] = { 3.0, 3.0, 4.0, 1.0, -4.0 }, VDS[5] = { 0.5, 5.0, -1.0, 5.0, -4.0 };
        static const double ID[5] = { 0.7575000005e-3, 1.100000005e-3, -5.100000001e-3, 5e-12, -4.000000004e-3 };
        static const int REG[5] = { REG_OHMICA, REG_SATURACION, REG_OHMICA, REG_CORTE, REG_SATURACION };
        static const char *const DET[5] = { "(3; 0,5) " S_oo "hmica: 0,7575000005 mA", "(3; 5) saturaci" S_oo "n: 1,100000005 mA",
                                             "(4; -1) D y S cambiados: -5,100000001 mA", "(1; 5) corte: 5 pA",
                                             "canal P sin VA en (-4; -4): -4,000000004 mA" };
        q = "puntos del MOSFET con el motor";
        for (i = 0; i < 5; i++) {
            sr_init(3);                                  /* 1 G, 2 D */
            a4_cc(0, 1, VGS[i]);
            a4_cc(0, 2, VDS[i]);
            k = m4_mos(2, 1, 0, i < 4 ? 1 : -1, 2.0, 1e-3, i < 4 ? 50.0 : 0.0);
            if (m4_op(q, &v, &nw)) {
                q4_rel(q, DET[i], v.I[k], ID[i], 1e-9);
                chk_cond(q, "su regi" S_oo "n", v.reg[k] == REG[i]);
            }
        }
    }

    /* ---- Estructura: la puerta al aire flota (FLOTA la ancla a 0 V): en corte ---- */
    q = "estructura";
    sr_init(4);                                          /* 1 VDD, 2 D, 3 la puerta, que no toca nada */
    a4_cc(0, 1, 5.0);
    sr_el(SE_R, 1, 2, 1e3);
    k = m4_mos(2, 3, 0, 1, 2.0, 1e-3, 0.0);
    chk_cond(q, "la puerta al aire: FLOTA y en corte (5 pA de GMIN)", m4_op(q, &v, &nw) &&
             sim_op_cc_red(&sr, &w, &d, 0) == SIM_OK && (d.avisos & SIM_AV_FLOTA) && v.reg[k] == REG_CORTE &&
             fabs(v.I[k] - 5e-12) <= 1e-18);

    /* ---- El adaptador (el_de): la ficha a m[] y pol; los vacíos, los de
     *      nolin.h; un K imposible, SIM_MODELO ---- */
    q = "adaptador";
    circ_init(&c, "MOS");
    k = circ_elemento(&c, EL_MOS, 4, 3, DIR_DER);
    red_extraer(&c, &r);
    chk_cond(q, "vac" S_ii "os: canal N, Vt 2 V, K 1 mA/V" S_SUP2 ", sin VA", k >= 0 && sim_red_de(&c, &r, &s, &d) == SIM_OK &&
             s.el[k].tipo == SE_MOS && s.el[k].modelo == NL_MOS && s.el[k].pol == 1 && s.el[k].m[0] == NL_VT_M &&
             s.el[k].m[1] == NL_K && s.el[k].m[2] == 0.0 && s.el[k].c == r.c[k]);
    c.el[k].variante = MOS_CP;
    c.el[k].par[QM_VT - Q_P1].v = 1.5; c.el[k].par[QM_VT - Q_P1].est = M_DATO;
    c.el[k].par[QM_K - Q_P1].v = 2e-3; c.el[k].par[QM_K - Q_P1].est = M_DATO;
    c.el[k].par[QM_VA - Q_P1].v = 50.0; c.el[k].par[QM_VA - Q_P1].est = M_DATO;
    chk_cond(q, "canal P con Vt 1,5 V, K 2 mA/V" S_SUP2 " y VA 50 V", sim_red_de(&c, &r, &s, &d) == SIM_OK && s.el[k].pol == -1 &&
             s.el[k].m[0] == 1.5 && s.el[k].m[1] == 2e-3 && s.el[k].m[2] == 50.0);
    c.el[k].par[QM_K - Q_P1].v = 0.0;
    chk_cond(q, "K = 0: SIM_MODELO", sim_red_de(&c, &r, &s, &d) == SIM_MODELO && d.elem == k);
    {
        char m[200];
        sim_mensaje_circ(&c, 0, &d, m, (int)sizeof m);
        chk_cond(q, "... con su texto: no cuadran", strstr(m, "no cuadran") != 0);
    }
    k = m4_caso_a(1, 0.0);
    sr.el[k].m[1] = 0.0;
    chk_cond(q, "valida: K = 0 en una red a mano, SIM_MODELO", sim_op_cc_red(&sr, &v, &d, 0) == SIM_MODELO && d.elem == k);

    /* ---- La tabla es lineal: SOL_NO_LINEAL con un MOSFET; y un MOSFET no
     *      manda a una controlada (CTL_3PATAS) ---- */
    {
        static Solucion so;
        int km;
        c4_init(4);
        c4_el(EL_FUENTE, 0, 1, 5.0);
        c4_el(EL_RES, 1, 2, 1e3);
        km = nuevo_el(EL_MOS, 2, 0); rr.c[km] = 3;
        c4_el(EL_RES, 1, 3, 100e3);
        c4_dep(EL_EDEP, 0, 3, DEP_V, 2.0, km);
        cierra_red();
        resolver(&rc, &rr, &so);
        chk_cond("tabla", "un MOSFET: SOL_NO_LINEAL", so.estado == SOL_NO_LINEAL && so.elem == km);
        chk_cond("control", "un MOSFET no manda a una controlada: CTL_3PATAS", circ_ctl_motivo(&rc, rc.nel - 1) == CTL_3PATAS);
    }

    /* ---- En el tiempo: la fuente común con una senoidal por Cin a la
     *      puerta. El balance (s_k = +1, §3.H) es exacto en los puntos;
     *      trozos bit a bit ---- */
    q = "fuente com" S_uu "n en el tiempo";
    k = m4_fc();
    i = t4_seno(0, 5, 0.1, 1e3, 0.0);                   /* 100 mV a 1 kHz */
    canal(0, CANAL_NODO, 4); canal(1, CANAL_I, k);
    saj.t_fin = 5e-3; saj.h = 0.0;
    emula(q);
    if (sim_res()->fin == FIN_COMPLETO) {
        const SimRes *rs = sim_res();
        t4_balance(q, 1e-2);
        chk_cond(q, "sin errores, con Newton en cada paso", rs->d.estado == SIM_OK && rs->nw.iter > rs->pasos);
        chk_cond(q, "el residuo de Kirchhoff de la emulaci" S_oo "n <= 1e-9", sim_residuo_max() <= 1e-9);
        chk_cond(q, "el MOSFET, saturado al acabar", rs->ultimo.reg[k] == REG_SATURACION);
        q4_rel(q, "ID al empezar: la del OP (1 mA)", rs->op.I[k], 1e-3, 1e-6);
        chk_cond(q, "V2 = VGS (3 V) e I2 = 0 en el registro del OP", fabs(rs->op.V2[k] - 3.0) < 1e-6 && rs->op.I2[k] == 0.0);
    }
    (void)i;
    t4_trozos(q);
}

/* ---- [62] la fuente común en pequeña señal: H = vD/vin frente a la cerrada
 *      con gm y gds de nl_mos en Q ---- */
static Cx m4_h(double f, double gm, double gds)
{
    double w = 2.0 * 3.14159265358979323846 * f;
    Cx ycin = cx(0.0, w * M4_CIN), ys = cx(1.0 / M4_RS, w * M4_CS);
    Cx vg = cx_div(ycin, cx_mas(ycin, cx(1.0 / M4_R1 + 1.0 / M4_R2, 0.0)));        /* vG/vin */
    /* D: vD/RD + iD = 0; S: vS·Ys = iD; iD = gm·(vG - vS) + gds·(vD - vS) */
    Cx den = cx_mas(cx(1.0 / M4_RD + gds, 0.0), cx_div(cx(gm + gds, 0.0), cx_esc(ys, M4_RD)));
    return cx_esc(cx_div(vg, den), -gm);
}

static void m4_bode(void)
{
    static const double F[4] = { 10.0, 100.0, 1e3, 10e3 };
    const char *q = "fuente com" S_uu "n (Bode)";
    static Valores v;
    static CaRes cr;
    static SimLin L;
    SimNewton nw;
    SimDiag d;
    NlMos o;
    Cx H, He;
    int k, ke, i;
    k = m4_fc();
    ke = t4_seno(0, 5, 1e-3, 1e3, 0.0);
    canal(0, CANAL_NODO, 4); canal(1, CANAL_NADA, 0);
    if (!m4_op(q, &v, &nw)) return;
    nl_mos(&sr.el[k], v.V2[k], v.V[k], &o);
    q4_rel(q, "gm en Q = 2K" S_CDOT "vov (2 mS)", o.gm, 2e-3, 1e-6);
    chk_cond(q, "se barre", bode_sr(ke, 0, -1, -1, 0.0, 0.0, 0.0) == SIM_OK && bode_res()->fin == FIN_COMPLETO);
    chk_cond(q, "aviso PEQ_SENAL y no CC_FUERA", (bode_res()->d.avisos & SIM_AV_PEQ_SENAL) && !(bode_res()->d.avisos & SIM_AV_CC_FUERA));
    for (i = 0; i < 4; i++) {
        char det[96];
        bode_en(F[i], &H);
        He = m4_h(F[i], o.gm, o.gds);
        sprintf(det, "H(%g Hz) = la cerrada con gm y gds en Q (1e-9)", F[i]);
        pruebas++;
        if (!(cx_abs(cx_menos(H, He)) <= 1e-9 * cx_abs(He))) falla(q, det, cx_abs(cx_menos(H, He)) / cx_abs(He), 0.0);
    }
    bode_en(1e6, &H);                                   /* Cin y CS, cortos: -gm·RD */
    chk_t(q, "en la banda media (1 MHz) tiende a -gm" S_CDOT "RD = -4", H.re, -4.0, 4.0, 1e-3);
    /* P_CA en 1 kHz: la de pequeña señal, con la región en Q y Tellegen */
    {
        CaExc x;
        memset(&x, 0, sizeof x);
        chk_cond(q, "ca_punto_q", ca_punto_q(&sr, &L, &d) == SIM_OK && L.hay && L.reg[k] == REG_SATURACION);
        q4_rel(q, "g10 = gm y g11 = gds en Q", L.g[k][1][0], o.gm, 1e-12);
        chk_cond(q, "la puerta sin corriente: g00 = g01 = 0", L.g[k][0][0] == 0.0 && L.g[k][0][1] == 0.0);
        ca_pon_q(&L);
        x.activa = 1u << ke;
        x.U[ke] = cx(1.0, 0.0);
        memset(&cr, 0, sizeof cr);
        chk_cond(q, "ca_resuelve_red en 1 kHz", ca_resuelve_red(&sr, 1e3, &x, &cr) == SIM_OK);
        ca_pon_q(0);
        He = m4_h(1e3, o.gm, o.gds);
        chk_t(q, "V(D) = H(1 kHz)", cx_abs(cx_menos(cr.phi[4], He)), 0.0, cx_abs(He), 1e-9);
        chk_t(q, "Tellegen en P_CA con el MOSFET (absorbe por D-S)", cr.tellegen, 0.0, 1.0, 1e-9);
        q4_rel(q, "ID en se" S_nn "al = gm" S_CDOT "vgs + gds" S_CDOT "vds", cx_abs(cr.I[k]),
               cx_abs(cx_mas(cx_esc(cx_menos(cr.phi[2], cr.phi[3]), o.gm), cx_esc(cx_menos(cr.phi[4], cr.phi[3]), o.gds))), 1e-9);
    }
}

/* ---- [63] la familia del trazador (§6.10): ID(VDS) con VGS = Vt + 0,5·k ---- */
static void m4_familia(void)
{
    static const double ID[6] = { 5e-12, 0.250000005e-3, 1.000000005e-3, 2.250000005e-3, 4.000000005e-3, 6.250000005e-3 };
    static Circuito c;
    static Valores v;
    const BarrRes *r;
    const BarrReg *g;
    SimDiag d;
    double y, dy, y1, y2;
    int k, i, pol;
    for (pol = 0; pol < 2; pol++) {
        const char *q = pol ? "familia del canal P" : "familia del MOSFET";
        char det[96];
        circ_init(&c, "familia");
        k = circ_elemento(&c, EL_MOS, 4, 3, DIR_DER);
        if (k < 0) { chk_cond(q, "se coloca", 0); return; }
        if (pol) c.el[k].variante = MOS_CP;
        chk_cond(q, "barr_iniciar_curva", barr_iniciar_curva(&c, k) == SIM_OK);
        while (barr_avanzar(~0ul)) {}
        r = barr_res(); g = barr_reg();
        chk_cond(q, "6 curvas, completa y sin huecos", r->modo == BARR_FAMILIA && r->fin == FIN_COMPLETO && r->huecos == 0 &&
                 g->ntr == 6 && !r->fam_bjt && !r->fam_early && r->fam_vt == 2.0);
        chk_cond(q, "cada curva, su recorrido y el canal 1", g->recorrido[5] == 5 && g->canal[5] == 0 && g->recorrido[0] == 0);
        chk_cond(q, pol ? "de 0 a 10 V; VSG = Vt + 0,5" S_CDOT "k (\"VSG=2,5\")" : "de 0 a 10 V; VGS = Vt + 0,5" S_CDOT "k (\"VGS=2,5\")",
                 r->x0 == 0.0 && r->x1 == 10.0 && strcmp(r->nom_tr[1], pol ? "VSG=2,5" : "VGS=2,5") == 0 &&
                 strcmp(r->nom_tr[0], pol ? "VSG=2" : "VGS=2") == 0 && r->fam[5] == 4.5);
        chk_cond(q, pol ? "\"-ID frente a VSD de M1\"" : "\"ID frente a VDS de M1\"",
                 strcmp(r->titulo, pol ? "-ID frente a VSD de M1" : "ID frente a VDS de M1") == 0 &&
                 strcmp(r->canal[0].nombre, pol ? "-ID(M1)" : "ID(M1)") == 0 && strcmp(r->nom_x, pol ? "VSD" : "VDS") == 0);
        for (i = 0; i < 6; i++) {                        /* VDS = 5 V: 5 pA; 0,250000005 ... 6,250000005 mA */
            sprintf(det, "VGS = Vt + %g V: ID en VDS = 5 V (1e-9)", 0.5 * i);
            chk_cond(q, "barr_en(k, 5 V)", barr_en(i, 5.0, &y, 0));
            q4_rel(q, det, y, ID[i], 1e-9);
        }
        chk_cond(q, "barr_valores(3, 5 V)", barr_valores(3, 5.0, &v, &d) == SIM_OK);
        q4_rel(q, "... VGS = 3,5 V (la E de la puerta)", pol ? -v.V2[2] : v.V2[2], 3.5, 1e-12);
        chk_cond(q, "... saturado, sin corriente de puerta", v.reg[2] == REG_SATURACION && v.I2[2] == 0.0);
        /* dy/dx exacta frente a diferencias centradas, en la zona óhmica (VDS 0,5 V, vov 2 V) */
        barr_en(4, 0.5 + 1e-6, &y1, 0); barr_en(4, 0.5 - 1e-6, &y2, 0); barr_en(4, 0.5, &y, &dy);
        chk_t(q, "dID/dVDS exacta frente a diferencias centradas (" S_oo "hmica)", dy, (y1 - y2) / 2e-6, fabs(dy), 1e-6);
        q4_rel(q, "... = 2K" S_CDOT "(vov - VDS) (3 mS)", dy, 3e-3, 1e-6);
        /* la malla es barr_en en sus puntos (desde su continuación, su curva) */
        for (i = 0; i < 6; i++) {
            int j;
            for (j = 1; j < g->n; j += 35)
                if (!(barr_en(i, g->x[j], &y, 0) && fabs(y - g->y[i][j]) <= 1e-12 * fabs(y) + 1e-24)) break;
            chk_cond(q, "la malla de cada curva es barr_en (1e-12)", j >= g->n);
        }
        if (!pol) {                                     /* trozos: con 1 operación, bit a bit */
            static BarrReg g0;
            g0 = *barr_reg();
            barr_iniciar_curva(&c, k);
            while (barr_avanzar(1ul)) {}
            chk_cond(q, "barr_avanzar(1) una y otra vez: el registro, bit a bit", memcmp(barr_reg(), &g0, sizeof g0) == 0);
        }
    }
    /* con VA: la pendiente de cada curva saturada es ID/(VA + VDS) (r0 finita) */
    {
        const char *q = "familia con VA";
        circ_init(&c, "familia");
        k = circ_elemento(&c, EL_MOS, 4, 3, DIR_DER);
        c.el[k].par[QM_VA - Q_P1].v = 50.0; c.el[k].par[QM_VA - Q_P1].est = M_DATO;
        barr_iniciar_curva(&c, k);
        while (barr_avanzar(~0ul)) {}
        chk_cond(q, "fam_early", barr_res()->fam_early == 1 && barr_res()->fin == FIN_COMPLETO);
        barr_en(2, 5.0, &y, &dy);
        q4_rel(q, "ID = K" S_CDOT "vov" S_SUP2 S_CDOT "(1 + VDS/VA) = 1,1 mA", y, 1.100000005e-3, 1e-9);
        chk_t(q, "r0 = dVDS/dID = (VA + VDS)/ID", 1.0 / dy, 55.0 / y, 55.0 / y, 1e-6);
    }
}

/* ---- [14] el tiempo de la familia de un MOSFET ---- */
static void m4_tiempos(void)
{
    static Circuito c;
    unsigned long o;
    double e_fam;
    long it = 0;
    int j, k;
    circ_init(&c, "familia");
    k = circ_elemento(&c, EL_MOS, 4, 3, DIR_DER);
    o = barr_ops();
    chk_cond("tiempos E4", "la familia del MOSFET empieza", barr_iniciar_curva(&c, k) == SIM_OK);
    while (barr_avanzar(~0ul)) {}
    o = barr_ops() - o;
    for (j = 0; j < barr_reg()->ntr; j++)
        for (k = 0; k < barr_reg()->n; k++) it += barr_reg()->it[j][k];
    e_fam = (double)o * Q4_NS_OP * 1e-9 + (double)it * NS_ITER * 1e-9;
    printf("    familia de un MOSFET: 6 curvas de %d puntos, %ld iteraciones, %lu operaciones (calculadora ~%.2f s; t_est %.2f s)\n",
           barr_reg()->n, it, o, e_fam, barr_res()->t_est);
    pruebas += 2;
    if (barr_res()->fin != FIN_COMPLETO) falla("tiempos", "la familia del MOSFET acaba", barr_res()->fin, FIN_COMPLETO);
    if (e_fam > 3.0) falla("tiempos", "la familia de un MOSFET tardar" S_ii "a m" S_aa "s de 3 s", e_fam, 3.0);
}

/* ---- [65] el MOSFET en la interfaz: la tecla [M], la pieza 22, la ficha, la
 *      página Modelo con su familia, el resultado y la Ayuda ---- */
static void m4_interfaz(void)
{
    const Circuito *cu = ui_circuito();
    static Circuito c;
    const char *q;
    int k;

    gfx_desbordes_reset();
    /* ---- [M] y la ficha ---- */
    q = "MOSFET: [M] y su ficha";
    chk_cond(q, "[M] es la tecla del MOSFET", TIPOS[EL_MOS].tecla == 'M' && TIPOS[EL_BJT].tecla == 'Q');
    ui_init();
    TECLAS(T_ENTER, T_ENTER, T_ENTER);                         /* editor nuevo: el cursor en (1,1) */
    TECLAS(T_DER, T_DER, T_DER, T_ABAJO, T_ABAJO);              /* (4,3) */
    TECLAS(T_LETRA('M'));
    chk_cond(q, "[M]: la instrucci" S_oo "n del MOSFET", ve("MOSFET: flecha de la puerta G a D y S"));
    chk_cond(q, "en (4,3) caben las cuatro flechas", u4_flecha(4, 3, DIR_DER, C_CURSOR) && u4_flecha(4, 3, DIR_ABA, C_CURSOR) &&
             u4_flecha(4, 3, DIR_IZQ, C_CURSOR) && u4_flecha(4, 3, DIR_ARR, C_CURSOR));
    TECLAS(T_DER);
    chk_cond(q, "[" S_RARR "] coloca M1: G (4,3), D (5,2), S (5,4)", cu->nel == 1 && cu->el[0].tipo == EL_MOS &&
             strcmp(cu->el[0].etiq, "M1") == 0 && cu->el[0].xc == 4 && cu->el[0].yc == 3 && cu->el[0].xa == 5 &&
             cu->el[0].ya == 2 && cu->el[0].xb == 5 && cu->el[0].yb == 4 && circ_valido(cu));
    chk_cond(q, "y abre su ficha: canal N, con su eco", ui_pantalla() == P_ELEMENTO && cu->el[0].variante == MOS_CN &&
             ve("Canal N: Vt 2 V " S_CDOT " K 1 mA/V" S_SUP2));
    chk_cond(q, "el pie: [izq/der] canal [F] espejo", ve("[arr/ab] campo [izq/der] canal [F] espejo [=]"));
    r4_vuelca("e4_mos_ficha");
    TECLAS(T_DER);
    chk_cond(q, "[der]: canal P y su eco", cu->el[0].variante == MOS_CP && ve("Canal P: Vt 2 V"));
    TECLAS(T_IZQ, T_ESC);
    chk_cond(q, "el cursor salt" S_oo " al drenador: Aqu" S_ii ": M1 (D)", ui_pantalla() == P_EDITOR && cu->el[0].variante == MOS_CN &&
             ve("Aqu" S_ii ": M1 (D)"));
    chk_cond(q, "el r" S_oo "tulo sin resolver: \"N Vt2V\"", ve("N Vt2V"));
    TECLAS(T_LETRA('F'));
    chk_cond(q, "[F]: espejo, D y S cambian de sitio", ve("Espejo: D y S de M1 cambian de sitio.") && cu->el[0].xa == 5 &&
             cu->el[0].ya == 4 && cu->el[0].xb == 5 && cu->el[0].yb == 2 && cu->el[0].xc == 4 && cu->el[0].yc == 3 &&
             circ_valido(cu));
    TECLAS(T_LETRA('Z') | T_CON_CTRL);
    TECLAS(T_ENTER, T_CATALOGO);
    chk_cond(q, "[cat" S_aa "logo]: la p" S_aa "gina Modelo del MOSFET, con [I]", ui_pantalla() == P_MODELO &&
             ve("Nivel 1, con vov = |vGS| - Vt:") && ve("[I] su curva I-V"));
    TECLAS(T_LETRA('I'));
    r4_espera();
    chk_cond(q, "[I]: la familia de M1 en P_BARRIDO", ui_pantalla() == P_BARRIDO && barr_res()->modo == BARR_FAMILIA &&
             !barr_res()->fam_bjt && ve("ID frente a VDS de M1") && ve("0 a 10 V"));
    chk_cond(q, "la derivada de la curva de VGS = Vt: en corte, r0 (sin VA)", ve("vov = 0: en corte") && ve("sin VA"));
    r4_vuelca("e4_mos_familia");
    TECLAS(T_LETRA('C'), T_LETRA('C'));
    chk_cond(q, "[C] x2: la curva de VGS = 3 V, ID/vov" S_SUP2 " = K", ui_curva()->activo == 2 &&
             ve("ID/vov" S_SUP2 " = 1,000 mA/V" S_SUP2));
    TECLAS(T_TAB);
    chk_cond(q, "[tab]: la curva es del componente solo", ui_pantalla() == P_BARRIDO && ve("no tiene circuito"));
    TECLAS(T_ESC);
    chk_cond(q, "[esc] vuelve a la p" S_aa "gina Modelo", ui_pantalla() == P_MODELO);
    TECLAS(T_ESC, T_ESC);

    /* ---- La pieza 22 de la paleta (la 21): 5x[ab] y [der] ---- */
    q = "paleta";
    chk_cond(q, "la pieza 21: el MOSFET, la " S_uu "ltima", PAL_N == 22 && ui_paleta_tipo(21) == EL_MOS && ui_paleta_tipo(20) == EL_BJT);
    ui_init();
    TECLAS(T_ENTER, T_ENTER, T_ENTER);
    TECLAS(T_CATALOGO);
    chk_cond(q, "el MOSFET, con su tecla", ve("MOSFET") && ve("[M]"));
    repite(T_ABAJO, 5);
    TECLAS(T_DER);
    ui_dibuja();
    chk_cond(q, "[ab] x5 y [der]: la celda 21, en (88, 191)", gfx_buffer()[191 * SCR_W + 88] == C_BARRA &&
             gfx_buffer()[191 * SCR_W + 16] == C_PANEL && ve("MOSFET"));
    r4_vuelca("e4_mos_paleta");
    repite(T_DER, 2);
    ui_dibuja();
    chk_cond(q, "[der] no pasa de la " S_uu "ltima", gfx_buffer()[191 * SCR_W + 88] == C_BARRA && gfx_buffer()[191 * SCR_W + 160] == C_PANEL);
    TECLAS(T_IZQ);
    ui_dibuja();
    chk_cond(q, "[izq] desde la " S_uu "ltima: el transistor", gfx_buffer()[191 * SCR_W + 16] == C_BARRA &&
             gfx_buffer()[191 * SCR_W + 88] == C_PANEL);
    TECLAS(T_DER, T_ENTER, T_DER);
    chk_cond(q, "[enter][" S_RARR "]: M1 en (1,1)", cu->nel == 1 && cu->el[0].tipo == EL_MOS &&
             cu->el[0].xc == 1 && cu->el[0].yc == 1 && ui_pantalla() == P_ELEMENTO);
    TECLAS(T_ESC);

    /* ---- El resultado: [=] con el caso a) dibujado; su tabla con VGS; y
     *      el canal P da lo mismo con el signo cambiado ---- */
    q = "MOSFET: [=]";
    k = r4_mos(&c, 0);
    chk_cond(q, "se dibuja (en el editor)", r3u_abre(&c));
    chk_cond(q, "la l" S_ii "nea de estado: el punto de operaci" S_oo "n", ve("Listo: [=] da el punto de operaci" S_oo "n"));
    TECLAS(T_IGUAL);
    r4_espera();
    chk_cond(q, "[=] lo resuelve: Newton", ui_pantalla() == P_RESULTADO && ui_res_nl() && !ve("corregirlo") &&
             ve("Punto de operaci" S_oo "n: Newton, "));
    chk_cond(q, "ID = 4,000000004 mA en la tabla de valores", fabs(ui_valores()->I[k] - 4.000000004e-3) <= 1e-9 * 4e-3);
    chk_cond(q, "su r" S_oo "tulo: la regi" S_oo "n e \"Id 4,00mA\"", ve("Id 4,00mA"));
    r4_vuelca("e4_mos_resultado");
    TECLAS(T_TAB);
    chk_cond(q, "[tab]: su regi" S_oo "n y su segundo puerto", ui_pantalla() == P_TABLA && ve("saturaci" S_oo "n") &&
             ve("M1  VGS ") && ve("Kirchhoff"));
    r4_vuelca("e4_mos_tabla");
    TECLAS(T_ESC, T_ENTER);
    chk_cond(q, "[enter]: sin paso a paso", ve("Sin paso a paso: M1 no es lineal."));
    TECLAS(T_ESC, T_ESC);
    k = r4_mos(&c, 1);
    r3u_abre(&c);
    TECLAS(T_IGUAL);
    r4_espera();
    chk_cond(q, "el canal P: ID = -4,000000004 mA", ui_pantalla() == P_RESULTADO &&
             fabs(ui_valores()->I[k] + 4.000000004e-3) <= 1e-9 * 4e-3);
    TECLAS(T_ESC, T_ESC);
    limpia_mis();

    /* ---- La Ayuda: «[Q] BJT  [M] MOSFET» en Dibujar y la sección TRANSISTORES ---- */
    q = "Ayuda";
    ui_init();
    TECLAS(T_ENTER, T_ABAJO, T_ABAJO, T_ENTER);                 /* menú > Ayuda */
    chk_cond(q, "en la Ayuda", ui_pantalla() == P_AYUDA);
    repite(T_ABAJO, 15);
    chk_cond(q, "Dibujar: [Q] BJT  [M] MOSFET", ve("   [D] diodo  [U] amp. op.  [Q] BJT  [M] MOSFET"));
    repite(T_ABAJO, 215);                                      /* +5: [U] y el disparo, y «en el tiempo» */
    chk_cond(q, "la secci" S_oo "n TRANSISTORES", ve("TRANSISTORES") && ve("[Q] y una flecha: el BJT, de la base hacia C y") &&
             ve("[M] y una flecha: el MOSFET, de la puerta G hacia"));
    r4_vuelca("e4_mos_ayuda");
    repite(T_ABAJO, 120);
    chk_cond(q, "hasta el final, ACERCA DE", ve("Hakaru CES, versi" S_oo "n 4.0, para TI-Nspire CX II"));
    TECLAS(T_ESC);

    chk_cond("interfaz del MOSFET", "ning" S_uu "n texto fuera de la pantalla", gfx_desbordes() == 0);
}

static void test_e4_mos(void)
{
    printf("[64b] Etapa 4: el MOSFET (motor, peque" S_nn "a se" S_nn "al y familia)\n");
    m4_motor();
    m4_bode();
    m4_familia();
    printf("[65d] Etapa 4: el MOSFET en la interfaz\n");
    m4_interfaz();
}
