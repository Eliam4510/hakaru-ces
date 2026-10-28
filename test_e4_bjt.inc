/* --------------------------------------------------------------------------
 *  64. Etapa 4: el BJT en el motor (paso 13)
 *
 *  El BJT (Ebers-Moll de transporte con Early, D9) con el motor de verdad:
 *  el punto de operación de los casos cerrados del §6.11 (mpmath en
 *  e4juez/n2.py; los mismos que test_nolin comprueba con su Newton propio),
 *  sus dos puertos en Valores (V = VCE, I = IC, V2 = VBE, I2 = IB), las
 *  regiones, el balance del transitorio con los dos puertos, la pequeña
 *  señal (el emisor común del §6.9, [62]), la familia del trazador (§6.10,
 *  [63]) y sus tiempos ([14]). La interfaz con el BJT va en test_e4_ui.inc
 *  ([65]) y los modelos 41 y 42, en test_e4_bib.inc ([66]).
 *  Se incluye tras test_e4_bib.inc (usa sr, saj, sr_init, sr_el, canal,
 *  emula, a4_cc, t4_*, d4_i_union, b4_*, bode_sr, rc, rr y los chk).
 * ----------------------------------------------------------------------- */

/* Un BJT en sr: el colector en nc, la base en nb y el emisor en ne; Is 1 fA
 * y βr 1 (los de omisión) */
static int q4_bjt(int nc, int nb, int ne, int pol, double bf, double vaf)
{
    int k = sr_el(SE_BJT, nc, ne, 0.0);
    SimEl *x = &sr.el[k];
    x->c = (short)nb;
    x->modelo = NL_BJT;
    x->pol = (signed char)pol;
    x->m[0] = NL_IS_Q; x->m[1] = bf; x->m[2] = NL_BR; x->m[3] = vaf;
    x->m[5] = nl_vcrit(NL_IS_Q, NL_VT);
    return k;
}

/* El emisor común con divisor: 1 VCC, 2 B, 3 E, 4 C; R1 de VCC a B, R2 de B a
 * tierra, RC de VCC a C y RE de E a tierra. Devuelve el BJT */
static int q4_ec(double VCC, double R1, double R2, double RC, double RE, int pol, double bf, double vaf)
{
    sr_init(5);
    a4_cc(0, 1, VCC);
    sr_el(SE_R, 1, 2, R1);
    sr_el(SE_R, 2, 0, R2);
    sr_el(SE_R, 1, 4, RC);
    sr_el(SE_R, 3, 0, RE);
    return q4_bjt(4, 2, 3, pol, bf, vaf);
}

/* Un LED rojo (D6: la Is que pasa justo por 1,8 V a 20 mA con GMIN) */
static int q4_led_rojo(int a, int b)
{
    const NlColor *co = &NL_COLOR[LED_ROJO];
    double ij = NL_IF - NL_GMIN * co->Vf, vd = co->Vf - co->Rs * ij;
    int k = sr_el(SE_D, a, b, 0.0);
    SimEl *x = &sr.el[k];
    x->modelo = NL_UNION;
    x->m[0] = ij / (exp(vd / (co->n * NL_VT)) - 1.0); x->m[1] = co->n; x->m[2] = co->Rs;
    x->m[5] = nl_vcrit(x->m[0], co->n * NL_VT);
    x->p[0] = NL_IM; x->p[1] = NL_IF;
    return k;
}

/* kcl_de y ley_de con el BJT: la ley de nodos con sus dos corrientes (IB
 * entra por B, IC por C y las dos salen por E; el AO, la de su salida y la de
 * Ri) y su ley, nl_bjt en las V de los Valores (las uniones, la de
 * d4_i_union) */
static void q4_criterios(const char *q, const SimRed *s, const Valores *v)
{
    static double suma[MAX_NODOS], modu[MAX_NODOS];
    double peor_k = 0.0, peor_l = 0.0, t;
    int k, n;
    for (n = 0; n < s->nn; n++) suma[n] = modu[n] = 0.0;
    for (k = 0; k < s->nel; k++) {
        const SimEl *e = &s->el[k];
        double m = fabs(v->I[k]);
        if (e->tipo == SE_R) m += 2.2e-4 / e->val * (fabs(v->phi[e->a]) + fabs(v->phi[e->b]));
        if (e->tipo == SE_BJT) {
            NlBjt o;
            suma[e->c] += v->I2[k]; modu[e->c] += fabs(v->I2[k]);
            suma[e->b] -= v->I2[k]; modu[e->b] += fabs(v->I2[k]);
            nl_bjt(e, v->V2[k], v->V2[k] - v->V[k], &o);
            t = fabs(v->I[k] - o.ic) / (fabs(o.ic) + 1e-300); if (t > peor_l) peor_l = t;
            t = fabs(v->I2[k] - o.ib) / (fabs(o.ib) + 1e-300); if (t > peor_l) peor_l = t;
            t = fabs(v->V2[k] - (v->phi[e->c] - v->phi[e->b])); if (t > peor_l) peor_l = t;
        }
        if (e->tipo == SE_D && e->modelo == NL_UNION) {
            double im = d4_i_union(e->m, v->V[k]);
            t = fabs(v->I[k] - im) / (fabs(im) + 1e-300);
            if (t > peor_l) peor_l = t;
        }
        if (e->tipo == SE_AO) {                         /* entrega I por la salida (de nref a c); Ri entre + y - */
            suma[e->nref] += v->I[k]; modu[e->nref] += m;
            suma[e->c] -= v->I[k]; modu[e->c] += m;
            suma[e->a] += v->I2[k]; modu[e->a] += fabs(v->I2[k]);
            suma[e->b] -= v->I2[k]; modu[e->b] += fabs(v->I2[k]);
            continue;
        }
        suma[e->a] += v->I[k]; modu[e->a] += m;
        suma[e->b] -= v->I[k]; modu[e->b] += m;
    }
    for (n = 0; n < s->nn; n++) {                  /* con el piso del motor: 1 fA (la base al aire) */
        t = fabs(suma[n]) / (modu[n] + 1e-15);
        if (!s->ref[n] && t > peor_k) peor_k = t;
    }
    pruebas += 2;
    if (!(peor_k <= 1e-10)) falla(q, "kcl_de (el arnés) <= 1e-10", peor_k, 0.0);
    if (!(peor_l <= 1e-10)) falla(q, "ley_de (el arnés, nl_bjt) <= 1e-10", peor_l, 0.0);
}

/* El punto de operación de sr (sim_op_cc_red) con sus criterios */
static int q4_op(const char *q, Valores *v, SimNewton *nw)
{
    SimDiag d;
    int e;
    memset(nw, 0, sizeof *nw);
    e = sim_op_cc_red(&sr, v, &d, nw);
    chk_cond(q, "el punto de operaci" S_oo "n converge", e == SIM_OK);
    if (e != SIM_OK) return 0;
    q4_criterios(q, &sr, v);
    chk_cond(q, "residuo de Kirchhoff del motor <= 1e-12", nw->residuo_max <= 1e-12);
    return 1;
}

static void q4_rel(const char *q, const char *det, double a, double b, double tol) { chk_t(q, det, a, b, fabs(b), tol); }

/* ---- [64] el punto de operación, los puertos, las regiones y el tiempo ---- */
static void q4_motor(void)
{
    static Valores v, w;
    static SimLin L;
    SimNewton nw;
    SimDiag d;
    NlBjt o;
    int k, kd, i;
    const char *q;

    /* ---- El emisor común de Boylestad (β 140), sin y con Early ---- */
    q = "emisor com" S_uu "n de Boylestad";
    k = q4_ec(22.0, 39e3, 3.9e3, 10e3, 1.5e3, 1, 140.0, 0.0);
    if (q4_op(q, &v, &nw)) {
        q4_rel(q, "VBE = 0,71015607662840807", v.V2[k], 0.71015607662840807, 1e-9);
        q4_rel(q, "IB = 5,9979952449108933 " S_MU "A", v.I2[k], 5.9979952449108933e-6, 1e-9);
        q4_rel(q, "IC = 0,83972087399969434 mA", v.I[k], 0.83972087399969434e-3, 1e-9);
        q4_rel(q, "VCE = 12,334212956136149", v.V[k], 12.334212956136149, 1e-9);
        chk_cond(q, "sin homotop" S_ii "a, en menos de 20 iteraciones", nw.homotopia == 0 && nw.iter < 20);
    }
    k = q4_ec(22.0, 39e3, 3.9e3, 10e3, 1.5e3, 1, 140.0, 100.0);
    if (q4_op("Boylestad con VA 100 V", &v, &nw)) {
        q4_rel("Boylestad con VA 100 V", "VBE = 0,7074386661541133", v.V2[k], 0.7074386661541133, 1e-9);
        q4_rel("Boylestad con VA 100 V", "IC = 0,84354456360751576 mA", v.I[k], 0.84354456360751576e-3, 1e-9);
        q4_rel("Boylestad con VA 100 V", "VCE = 12,291137805735711", v.V[k], 12.291137805735711, 1e-9);
    }

    /* ---- El emisor común de la biblioteca (β 100): Q, gm, rπ, región, P ---- */
    q = "emisor com" S_uu "n de la biblioteca";
    k = q4_ec(12.0, 47e3, 10e3, 2.2e3, 470.0, 1, 100.0, 0.0);
    if (q4_op(q, &v, &nw)) {
        q4_rel(q, "VB = 1,9029000966741065", v.phi[2], 1.9029000966741065, 1e-9);
        q4_rel(q, "VBE = 0,73789576485171988", v.V2[k], 0.73789576485171988, 1e-9);
        q4_rel(q, "IB = 24,541903169310483 " S_MU "A", v.I2[k], 24.541903169310483e-6, 1e-9);
        q4_rel(q, "IC = 2,4541907177293845 mA", v.I[k], 2.4541907177293845e-3, 1e-9);
        q4_rel(q, "VCE = 5,4357760891729673", v.V[k], 5.4357760891729673, 1e-9);
        chk_cond(q, "regi" S_oo "n ACTIVA", v.reg[k] == REG_ACTIVA);
        q4_rel(q, "P absorbida = VCE" S_CDOT "IC + VBE" S_CDOT "IB", v.P[k], v.V[k] * v.I[k] + v.V2[k] * v.I2[k], 1e-15);
        q4_rel(q, "el emisor: IE = IC + IB por RE", v.phi[3] / 470.0, v.I[k] + v.I2[k], 1e-12);
        chk_cond(q, "el punto Q", sim_punto_q(&sr, &L, &d, 0) == SIM_OK && L.hay && L.reg[k] == REG_ACTIVA);
        nl_bjt(&sr.el[k], v.V2[k], v.V2[k] - v.V[k], &o);
        q4_rel(q, "gm = 0,094884892897226069 S (nl_bjt en Q)", o.dic_be, 0.094884892897226069, 1e-9);
        q4_rel(q, "r" S_PI " = 1053,9085500462489 " S_OHM " (nl_bjt en Q)", 1.0 / o.dib_be, 1053.9085500462489, 1e-9);
        q4_rel(q, "g10 = dIC/dVBE con VCE fija = gm", L.g[k][1][0], 0.094884892897226069, 1e-9);
        q4_rel(q, "g00 = dIB/dVBE = 1/r" S_PI, L.g[k][0][0], 1.0 / 1053.9085500462489, 1e-8);
        chk_cond(q, "g01 y g11: casi nada sin Early (GMIN y la inversa)", fabs(L.g[k][1][1]) < 1e-10 && fabs(L.g[k][0][1]) < 1e-10);
        /* por las dos rutas: sim_op_red (el OP de la emulación) da lo mismo */
        chk_cond(q, "sim_op_red: lo mismo", sim_op_red(&sr, &w, &d) == SIM_OK && fabs(w.I[k] - v.I[k]) <= 1e-12 * v.I[k] &&
                 fabs(w.V2[k] - v.V2[k]) <= 1e-12);
    }

    /* ---- El PNP: VCC = -12 V, las mismas cambiadas de signo ---- */
    q = "PNP";
    k = q4_ec(-12.0, 47e3, 10e3, 2.2e3, 470.0, -1, 100.0, 0.0);
    if (q4_op(q, &w, &nw)) {
        q4_rel(q, "VB = -1,9029000966741065", w.phi[2], -1.9029000966741065, 1e-12 * 1e3);
        q4_rel(q, "VBE, IB, IC y VCE: las del NPN con el signo cambiado", w.V2[k], -v.V2[k], 1e-12);
        q4_rel(q, "IB", w.I2[k], -v.I2[k], 1e-12);
        q4_rel(q, "IC", w.I[k], -v.I[k], 1e-12);
        q4_rel(q, "VCE", w.V[k], -v.V[k], 1e-12);
        chk_cond(q, "regi" S_oo "n ACTIVA", w.reg[k] == REG_ACTIVA);
    }

    /* ---- El interruptor: 5 V por RB 4,7 kΩ a la base; LED rojo y 330 Ω a 9 V
     *      en el colector. Satura: IC/IB = 24, no β ---- */
    q = "interruptor con LED";
    sr_init(6);                                         /* 1 VBB, 2 B, 3 VCC, 4 ánodo, 5 C */
    a4_cc(0, 1, 5.0);
    a4_cc(0, 3, 9.0);
    sr_el(SE_R, 1, 2, 4.7e3);
    k = q4_bjt(5, 2, 0, 1, 100.0, 0.0);
    sr_el(SE_R, 3, 4, 330.0);
    kd = q4_led_rojo(4, 5);
    if (q4_op(q, &v, &nw)) {
        q4_rel(q, "VBE = 0,79559221925032602", v.V2[k], 0.79559221925032602, 1e-9);
        q4_rel(q, "VCE = 0,091424804456153236", v.V[k], 0.091424804456153236, 1e-9);
        q4_rel(q, "I(LED) = 21,506917662571294 mA", v.I[kd], 21.506917662571294e-3, 1e-9);
        q4_rel(q, "IC = I(LED)", v.I[k], 21.506917662571294e-3, 1e-9);
        q4_rel(q, "IB = 0,89455484696801574 mA", v.I2[k], 0.89455484696801574e-3, 1e-9);
        q4_rel(q, "IC/IB = 24,042033571743936", v.I[k] / v.I2[k], 24.042033571743936, 1e-9);
        chk_cond(q, "regi" S_oo "n SATURACION", v.reg[k] == REG_SATURACION);
    }

    /* ---- Las demás regiones: corte (la base al aire) e inversa (C y E
     *      cambiados) ---- */
    q = "regiones";
    k = q4_ec(12.0, 47e3, 10e3, 2.2e3, 470.0, 1, 100.0, 0.0);
    sr.el[1].val = 1e12;                                /* R1 de 1 TΩ: la base, sin corriente */
    if (q4_op("corte", &v, &nw)) chk_cond(q, "sin corriente de base: CORTE", v.reg[k] == REG_CORTE && fabs(v.I[k]) < 1e-6);
    sr_init(4);                                         /* 1 VCC, 2 B, 3 C; el emisor a VCC por RC */
    a4_cc(0, 1, 12.0);
    sr_el(SE_R, 1, 2, 47e3);
    sr_el(SE_R, 1, 3, 2.2e3);
    k = q4_bjt(0, 2, 3, 1, 100.0, 0.0);                 /* C a tierra, E hacia VCC: al revés */
    if (q4_op("inversa", &v, &nw)) chk_cond(q, "C y E cambiados: INVERSA", v.reg[k] == REG_INVERSA);

    /* ---- Estructura: el BJT es una G entre sus tres patas (sin FLOTA) ---- */
    q = "estructura";
    sr_init(4);                                         /* la base solo toca al BJT */
    a4_cc(0, 1, 5.0);
    sr_el(SE_R, 1, 3, 1e3);
    k = q4_bjt(3, 2, 0, 1, 100.0, 0.0);
    chk_cond(q, "la base al aire: sin FLOTA, en corte", q4_op(q, &v, &nw) && !(sim_op_cc_red(&sr, &v, &d, 0) == SIM_OK && (d.avisos & SIM_AV_FLOTA)) &&
             v.reg[k] == REG_CORTE);
    sr_init(3);                                         /* una J que solo llega por el BJT: no es un corte */
    k = sr_el(SE_J, 0, 2, 0.0); sr.el[k].forma = ONDA_CC; sr.el[k].p[0] = 10e-6;
    a4_cc(0, 1, 5.0);
    k = q4_bjt(1, 2, 0, 1, 100.0, 0.0);
    chk_cond(q, "IB de una J: IC = " S_BETA S_CDOT "IB (1 mA)", q4_op(q, &v, &nw) && fabs(v.I[k] - 1e-3) < 1e-6);

    /* ---- La tabla es lineal: SOL_NO_LINEAL con un transistor; y un BJT no
     *      manda a una controlada (CTL_3PATAS) ---- */
    {
        static Solucion so;
        int kq, ke;
        c4_init(4);
        c4_el(EL_FUENTE, 0, 1, 5.0);
        c4_el(EL_RES, 1, 2, 1e3);
        kq = nuevo_el(EL_BJT, 2, 0); rr.c[kq] = 3;
        c4_el(EL_RES, 1, 3, 100e3);
        ke = c4_dep(EL_EDEP, 0, 3, DEP_V, 2.0, kq);
        (void)ke;
        cierra_red();
        resolver(&rc, &rr, &so);
        chk_cond("tabla", "un BJT: SOL_NO_LINEAL", so.estado == SOL_NO_LINEAL && so.elem == kq);
        chk_cond("control", "un BJT no manda a una controlada: CTL_3PATAS", circ_ctl_motivo(&rc, rc.nel - 1) == CTL_3PATAS);
    }

    /* ---- En el tiempo: un emisor común con una senoidal a la base por Cin y
     *      CE en RE. El balance con los dos puertos (s_k = +1, §3.H) es exacto
     *      en los puntos; trozos bit a bit ---- */
    q = "emisor com" S_uu "n en el tiempo";
    k = q4_ec(12.0, 47e3, 10e3, 2.2e3, 470.0, 1, 100.0, 0.0);
    sr.nn = 6;
    i = t4_seno(0, 5, 10e-3, 1e3, 0.0);                  /* 10 mV a 1 kHz */
    sr_el(SE_C, 5, 2, 10e-6);
    sr_el(SE_C, 3, 0, 100e-6);
    canal(0, CANAL_NODO, 4); canal(1, CANAL_I, k);
    saj.t_fin = 5e-3; saj.h = 0.0;
    emula(q);
    if (sim_res()->fin == FIN_COMPLETO) {
        const SimRes *r = sim_res();
        t4_balance(q, 1e-2);
        chk_cond(q, "sin errores, con Newton en cada paso", r->d.estado == SIM_OK && r->nw.iter > r->pasos);
        chk_cond(q, "el residuo de Kirchhoff de la emulaci" S_oo "n <= 1e-9", sim_residuo_max() <= 1e-9);
        chk_cond(q, "el BJT, en activa al acabar", r->ultimo.reg[k] == REG_ACTIVA);
        q4_rel(q, "IB e IC al empezar: las del OP (IC = 2,454 mA)", r->op.I[k], 2.4541907177293845e-3, 1e-9);
        chk_cond(q, "V2 = VBE en el registro del OP", fabs(r->op.V2[k] - 0.73789576485171988) < 1e-9);
    }
    (void)i;
    t4_trozos(q);
}

/* ---- [62] el emisor común en pequeña señal (§6.9) ---- */
static void q4_bode(void)
{
    static const double F[4] = { 10.0, 100.0, 1e3, 10e3 };
    static const double HR[4] = { -2.2796025175134517, -54.471578424495574, -202.87069724233534, -208.6863050586752 };
    static const double HI[4] = { -12.402598395470426, -90.315241479703668, -34.390096411599195, -3.5383875996049011 };
    const char *q = "emisor com" S_uu "n (Bode)";
    static CaRes cr;
    Cx H;
    int k, ke, i;
    k = q4_ec(12.0, 47e3, 10e3, 2.2e3, 470.0, 1, 100.0, 0.0);
    sr.nn = 6;
    ke = t4_seno(0, 5, 1e-3, 1e3, 0.0);                  /* la señal, a la base por Cin */
    sr_el(SE_C, 5, 2, 10e-6);
    sr_el(SE_C, 3, 0, 100e-6);
    canal(0, CANAL_NODO, 4); canal(1, CANAL_NADA, 0);
    chk_cond(q, "se barre", bode_sr(ke, 0, -1, -1, 0.0, 0.0, 0.0) == SIM_OK && bode_res()->fin == FIN_COMPLETO);
    chk_cond(q, "aviso PEQ_SENAL y no CC_FUERA", (bode_res()->d.avisos & SIM_AV_PEQ_SENAL) && !(bode_res()->d.avisos & SIM_AV_CC_FUERA));
    for (i = 0; i < 4; i++) {
        char det[96];
        double m = hypot(HR[i], HI[i]);
        bode_en(F[i], &H);
        sprintf(det, "H(%g Hz) = %.17g %+.17g j (1e-9)", F[i], HR[i], HI[i]);
        pruebas++;
        if (!(hypot(H.re - HR[i], H.im - HI[i]) <= 1e-9 * m)) falla(q, det, hypot(H.re - HR[i], H.im - HI[i]) / m, 0.0);
    }
    bode_en(10e3, &H);
    chk_t(q, "46,391127366805791 dB en 10 kHz", 20.0 * log10(cx_abs(H)), 46.391127366805791, 46.4, 1e-9);
    bode_en(1e6, &H);                                   /* Cin y CE, cortos: -gm·RC */
    chk_t(q, "en la banda media (1 MHz) tiende a -gm" S_CDOT "RC = -208,74676437389735", H.re, -208.74676437389735, 208.7, 1e-3);
    (void)k;
    /* P_CA en 1 kHz: la de pequeña señal, con la región en Q y Tellegen */
    {
        SimDiag d;
        CaExc x;
        static SimLin L;
        memset(&x, 0, sizeof x);
        chk_cond(q, "ca_punto_q", ca_punto_q(&sr, &L, &d) == SIM_OK && L.hay && L.reg[k] == REG_ACTIVA);
        ca_pon_q(&L);
        x.activa = 1u << ke;
        x.U[ke] = cx(1.0, 0.0);
        memset(&cr, 0, sizeof cr);
        chk_cond(q, "ca_resuelve_red en 1 kHz", ca_resuelve_red(&sr, 1e3, &x, &cr) == SIM_OK);
        ca_pon_q(0);
        chk_t(q, "V(C) = H(1 kHz)", cr.phi[4].re, HR[2], fabs(HR[2]), 1e-9);
        chk_t(q, "Tellegen en P_CA con los dos puertos del BJT", cr.tellegen, 0.0, 1.0, 1e-9);
        q4_rel(q, "IC en se" S_nn "al = gm" S_CDOT "vbe (casi)", cx_abs(cr.I[k]), 0.094884892897226069 * cx_abs(cx_menos(cr.phi[2], cr.phi[3])), 1e-6);
    }
}

/* ---- [63] la familia del trazador (§6.10) ---- */
static void q4_familia(void)
{
    static Circuito c;
    static Valores v;
    const BarrRes *r;
    const BarrReg *g;
    SimDiag d;
    double y, dy, y1, y2;
    int k, i, pol;
    for (pol = 0; pol < 2; pol++) {
        const char *q = pol ? "familia del PNP" : "familia del BJT";
        circ_init(&c, "familia");
        k = circ_elemento(&c, EL_BJT, 4, 3, DIR_DER);
        if (k < 0) { chk_cond(q, "se coloca", 0); return; }
        if (pol) c.el[k].variante = BJT_PNP;
        chk_cond(q, "barr_iniciar_curva", barr_iniciar_curva(&c, k) == SIM_OK);
        while (barr_avanzar(~0ul)) {}
        r = barr_res(); g = barr_reg();
        chk_cond(q, "6 curvas, completa y sin huecos", r->modo == BARR_FAMILIA && r->fin == FIN_COMPLETO && r->huecos == 0 &&
                 g->ntr == 6 && r->fam_bjt && !r->fam_early);
        chk_cond(q, "cada curva, su recorrido y el canal 1", g->recorrido[5] == 5 && g->canal[5] == 0 && g->recorrido[0] == 0);
        chk_cond(q, "de 0 a 10 V; IB = k" S_CDOT "20 " S_MU "A (\"IB=40" S_MU "\")",
                 r->x0 == 0.0 && r->x1 == 10.0 && strcmp(r->nom_tr[2], "IB=40" S_MU) == 0 && strcmp(r->nom_tr[0], "IB=0") == 0 &&
                 fabs(r->fam[5] - 100e-6) < 1e-18);
        chk_cond(q, pol ? "\"-IC frente a VEC de Q1\"" : "\"IC frente a VCE de Q1\"",
                 strcmp(r->titulo, pol ? "-IC frente a VEC de Q1" : "IC frente a VCE de Q1") == 0 &&
                 strcmp(r->canal[0].nombre, pol ? "-IC(Q1)" : "IC(Q1)") == 0);
        chk_cond(q, "barr_en(1, 5 V)", barr_en(1, 5.0, &y, &dy));
        q4_rel(q, "IB = 20 " S_MU "A: IC = 2,0000003578488944 mA", y, 2.0000003578488944e-3, 1e-9);
        chk_cond(q, "barr_valores(1, 5 V)", barr_valores(1, 5.0, &v, &d) == SIM_OK);
        q4_rel(q, "... VBE = 0,73260251552082675", pol ? -v.V2[2] : v.V2[2], 0.73260251552082675, 1e-9);
        chk_cond(q, "barr_en(2, 5 V)", barr_en(2, 5.0, &y, 0));
        q4_rel(q, "IB = 40 " S_MU "A: IC = 4,0000003542453266 mA", y, 4.0000003542453266e-3, 1e-9);
        barr_valores(2, 5.0, &v, &d);
        q4_rel(q, "... VBE = 0,75053071359549415", pol ? -v.V2[2] : v.V2[2], 0.75053071359549415, 1e-9);
        chk_cond(q, "IB = 0: en corte (pA)", barr_en(0, 5.0, &y, 0) && fabs(y) < 1e-9);
        /* dy/dx exacta frente a diferencias centradas, en la zona de saturación (VCE 0,1 V) */
        barr_en(3, 0.1 + 1e-6, &y1, 0); barr_en(3, 0.1 - 1e-6, &y2, 0); barr_en(3, 0.1, &y, &dy);
        chk_t(q, "dIC/dVCE exacta frente a diferencias centradas", dy, (y1 - y2) / 2e-6, fabs(dy), 1e-6);
        /* la malla es barr_en en sus puntos (desde su continuación, su curva);
         * en saturación, las dos uniones en directa, el último Newton puede
         * mover el último bit: 1e-12 */
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
    /* con Early: la pendiente de cada curva es IC/(VA + VCE) (r0 finita) */
    {
        const char *q = "familia con Early";
        circ_init(&c, "familia");
        k = circ_elemento(&c, EL_BJT, 4, 3, DIR_DER);
        c.el[k].par[QB_VA - Q_P1].v = 100.0; c.el[k].par[QB_VA - Q_P1].est = M_DATO;
        barr_iniciar_curva(&c, k);
        while (barr_avanzar(~0ul)) {}
        chk_cond(q, "fam_early", barr_res()->fam_early == 1);
        barr_en(2, 5.0, &y, &dy);
        chk_t(q, "r0 = dVCE/dIC " S_APROX " (VA + VCE - VBE)/IC", 1.0 / dy, (100.0 + 5.0 - 0.75) / y, (100.0 + 5.0) / y, 1e-3);
    }
    /* un componente sin curva: SIM_NADA_BARRER */
    circ_init(&c, "nada");
    k = circ_elemento(&c, EL_RES, 1, 1, DIR_DER);
    chk_cond("familia", "una R no tiene curva", barr_iniciar_curva(&c, k) == SIM_NADA_BARRER);
}

/* ---- [14] los tiempos con el BJT: el peor punto de operación con 8 diodos,
 *      2 AO y 2 BJT, y la familia del trazador ---- */
#define Q4_NS_OP SIM_NS_OP
static void q4_tiempos(void)
{
    static Valores v;
    static Circuito c;
    SimDiag d;
    SimNewton nw;
    unsigned long o;
    double e_op, e_fam;
    long it = 0;
    int j, k, e;
    /* el peor OP del [14] con dos pares de diodos cambiados por un NPN (la
     * escalera de 5 V) y un PNP (la de la salida de U1, a -10 V) */
    sr_init(20);
    a4_cc(0, 1, 5.0); a4_cc(0, 10, 2.0);
    sr_el(SE_R, 1, 2, 1e3); sr_el(SE_R, 2, 3, 2e3);
    a4_ao(0, 2, 3, 0, 0, 0.0);
    {
        static const signed char A[5] = { 1, 4, 5, 6, 7 }, B[5] = { 3, 9, 13, 14, 15 };
        static const signed char NA[5] = { 4, 5, 6, 7, 8 }, NB[5] = { 9, 13, 14, 15, 16 };
        for (j = 0; j < 5; j++) {
            sr_el(SE_R, A[j], NA[j], 1e3);
            if (j < 4) t4_d_cat(NA[j], 0, &NL_1N4148); else q4_bjt(11, NA[j], 0, 1, 100.0, 0.0);
            sr_el(SE_R, B[j], NB[j], 1e3);
            if (j < 4) t4_d_cat(0, NB[j], &NL_1N4148); else q4_bjt(3, NB[j], 0, -1, 100.0, 0.0);
        }
    }
    a4_ao(6, 17, 17, 0, 0, 0.0);
    sr_el(SE_R, 17, 18, 1e3); sr_el(SE_R, 18, 0, 1e3);
    sr_el(SE_R, 10, 11, 1e3); sr_el(SE_R, 11, 12, 1e3); sr_el(SE_R, 12, 19, 1e3); sr_el(SE_R, 19, 0, 1e3);
    chk_cond("tiempos E4", "el peor OP con BJT: 32 elementos y 20 nodos", sr.nel == 32 && sr.nn == 20);
    memset(&nw, 0, sizeof nw);
    sim_ops_reset();
    e = sim_op_cc_red(&sr, &v, &d, &nw);
    o = sim_ops();
    e_op = (double)o * Q4_NS_OP * 1e-9 + (double)nw.iter * NS_ITER * 1e-9;
    chk_cond("tiempos E4", "el peor OP con BJT converge", e == SIM_OK);
    if (e == SIM_OK) q4_criterios("tiempos E4", &sr, &v);
    printf("    OP del peor caso con BJT (32 elementos, 8 diodos, 2 AO, 2 BJT): %ld iteraciones, %lu operaciones (calculadora ~%.0f ms)\n",
           nw.iter, o, e_op * 1e3);
    pruebas += 2;
    if (e_op > 0.5) falla("tiempos", "el peor OP con BJT tardar" S_ii "a m" S_aa "s de 0,5 s", e_op, 0.5);
    if (o >= NL_OPS_OP) falla("tiempos", "el peor OP con BJT pasa del tope NL_OPS_OP", (double)o, (double)NL_OPS_OP);

    /* la familia de un BJT: 6 curvas de 281 puntos (o las que decida la malla) */
    circ_init(&c, "familia");
    k = circ_elemento(&c, EL_BJT, 4, 3, DIR_DER);
    o = barr_ops();
    chk_cond("tiempos E4", "la familia empieza", barr_iniciar_curva(&c, k) == SIM_OK);
    while (barr_avanzar(~0ul)) {}
    o = barr_ops() - o;
    for (j = 0; j < barr_reg()->ntr; j++)
        for (k = 0; k < barr_reg()->n; k++) it += barr_reg()->it[j][k];
    e_fam = (double)o * Q4_NS_OP * 1e-9 + (double)it * NS_ITER * 1e-9;
    printf("    familia de un BJT: 6 curvas de %d puntos, %ld iteraciones, %lu operaciones (calculadora ~%.2f s; t_est %.2f s)\n",
           barr_reg()->n, it, o, e_fam, barr_res()->t_est);
    pruebas += 2;
    if (barr_res()->fin != FIN_COMPLETO) falla("tiempos", "la familia acaba", barr_res()->fin, FIN_COMPLETO);
    if (e_fam > 3.0) falla("tiempos", "la familia de un BJT tardar" S_ii "a m" S_aa "s de 3 s", e_fam, 3.0);
}

/* ---- [65] el BJT en la interfaz: la tecla [Q], la pieza 21, la ficha, la
 *      página Modelo con su familia, un elemento de cada tipo y los modelos
 *      41 y 42 por teclas ---- */
static void q4_interfaz(void)
{
    const Circuito *cu = ui_circuito();
    static Circuito c, d2;
    const char *q;
    int k, i, l13;

    gfx_desbordes_reset();
    /* ---- [Q] y la ficha ---- */
    q = "BJT: [Q] y su ficha";
    chk_cond(q, "[Q] es la tecla del BJT", TIPOS[EL_BJT].tecla == 'Q');   /* la [M], en el paso 14 */
    ui_init();
    TECLAS(T_ENTER, T_ENTER, T_ENTER);                         /* editor nuevo: el cursor en (1,1) */
    TECLAS(T_DER, T_DER, T_DER, T_ABAJO, T_ABAJO);              /* (4,3) */
    TECLAS(T_LETRA('Q'));
    chk_cond(q, "[Q]: la instrucci" S_oo "n del transistor", ve("Transistor: flecha de la base a C y E"));
    chk_cond(q, "en (4,3) caben las cuatro flechas", u4_flecha(4, 3, DIR_DER, C_CURSOR) && u4_flecha(4, 3, DIR_ABA, C_CURSOR) &&
             u4_flecha(4, 3, DIR_IZQ, C_CURSOR) && u4_flecha(4, 3, DIR_ARR, C_CURSOR));
    TECLAS(T_DER);
    chk_cond(q, "[" S_RARR "] coloca Q1: B (4,3), C (5,2), E (5,4)", cu->nel == 1 && cu->el[0].tipo == EL_BJT &&
             strcmp(cu->el[0].etiq, "Q1") == 0 && cu->el[0].xc == 4 && cu->el[0].yc == 3 && cu->el[0].xa == 5 &&
             cu->el[0].ya == 2 && cu->el[0].xb == 5 && cu->el[0].yb == 4 && circ_valido(cu));
    chk_cond(q, "y abre su ficha: NPN, con su eco", ui_pantalla() == P_ELEMENTO && cu->el[0].variante == BJT_NPN &&
             ve("NPN: " S_BETA " 100 " S_CDOT " Is 1 fA " S_CDOT " sin Early"));
    chk_cond(q, "el pie: [izq/der] tipo [F] espejo", ve("[arr/ab] campo [izq/der] tipo [F] espejo [=]"));
    TECLAS(T_DER);
    chk_cond(q, "[der]: PNP y su eco", cu->el[0].variante == BJT_PNP && ve("PNP: " S_BETA " 100"));
    TECLAS(T_IZQ, T_ESC);
    chk_cond(q, "el cursor salt" S_oo " al colector: Aqu" S_ii ": Q1 (C)", ui_pantalla() == P_EDITOR && cu->el[0].variante == BJT_NPN &&
             ve("Aqu" S_ii ": Q1 (C)"));
    TECLAS(T_LETRA('F'));
    chk_cond(q, "[F]: espejo, C y E cambian de sitio", ve("Espejo: C y E de Q1 cambian de sitio.") && cu->el[0].xa == 5 &&
             cu->el[0].ya == 4 && cu->el[0].xb == 5 && cu->el[0].yb == 2 && cu->el[0].xc == 4 && cu->el[0].yc == 3 &&
             circ_valido(cu));
    TECLAS(T_LETRA('Z') | T_CON_CTRL);
    TECLAS(T_ENTER, T_CATALOGO);
    chk_cond(q, "[cat" S_aa "logo]: la p" S_aa "gina Modelo del BJT, con [I]", ui_pantalla() == P_MODELO && ve("Ebers-Moll") &&
             ve("[I] su curva I-V"));
    TECLAS(T_LETRA('I'));
    r4_espera();
    chk_cond(q, "[I]: la familia de Q1 en P_BARRIDO", ui_pantalla() == P_BARRIDO && barr_res()->modo == BARR_FAMILIA &&
             ve("IC frente a VCE de Q1") && ve("0 a 10 V"));
    chk_cond(q, "la derivada de la curva de IB = 0: en corte, r0 (sin Early)", ve("IB = 0: en corte") && ve("sin Early"));
    r4_vuelca("e4_bjt_familia");
    TECLAS(T_LETRA('C'), T_LETRA('C'));
    chk_cond(q, "[C] x2: la curva de IB = 40 " S_MU "A, " S_BETA " = 100,0", ui_curva()->activo == 2 && ve(S_BETA " = IC/IB = 100,0"));
    TECLAS(T_TAB);
    chk_cond(q, "[tab]: la curva es del componente solo", ui_pantalla() == P_BARRIDO && ve("no tiene circuito"));
    TECLAS(T_ESC);
    chk_cond(q, "[esc] vuelve a la p" S_aa "gina Modelo", ui_pantalla() == P_MODELO);
    TECLAS(T_ESC, T_ESC);

    /* ---- La pieza 21 de la paleta ---- */
    q = "paleta";
    chk_cond(q, "la pieza 20: el transistor", PAL_N >= 21 && ui_paleta_tipo(20) == EL_BJT);
    ui_init();
    TECLAS(T_ENTER, T_ENTER, T_ENTER);
    TECLAS(T_CATALOGO);
    chk_cond(q, "el transistor, con su tecla", ve("Transistor") && ve("[Q]"));
    repite(T_ABAJO, 5);
    ui_dibuja();
    chk_cond(q, "[ab] x5: la celda 20, en (16, 191)", gfx_buffer()[191 * SCR_W + 16] == C_BARRA &&
             gfx_buffer()[191 * SCR_W + 90] == C_PANEL);
    /* la 20 ya no es la última: que [der] no pasa de la última lo mira la
     * prueba del MOSFET, la pieza 21 (paso 14, test_e4_mos.inc) */
    TECLAS(T_ENTER, T_DER);
    chk_cond(q, "[enter][" S_RARR "]: Q1 en (1,1)", cu->nel == 1 && cu->el[0].tipo == EL_BJT &&
             cu->el[0].xc == 1 && cu->el[0].yc == 1 && ui_pantalla() == P_ELEMENTO);
    TECLAS(T_ESC);

    /* ---- Un elemento de cada tipo (§8.1, riesgo 8): el BJT (el emisor
     *      común de la biblioteca) se dibuja, se guarda, se resuelve, se
     *      barre, se analiza en CA (pequeña señal) y se emula ---- */
    q = "BJT";
    modelo_cargar(41, &c);
    chk_cond(q, "v" S_aa "lido", circ_valido(&c));
    chk_cond(q, "se guarda y se abre igual", arch_guardar(0, &c) && arch_cargar(0, &d2) && mismos(&c, &d2));
    chk_cond(q, "se dibuja (en el editor)", r3u_abre(&c));
    chk_cond(q, "la l" S_ii "nea de estado: el punto de operaci" S_oo "n", ve("Listo: [=] da el punto de operaci" S_oo "n"));
    TECLAS(T_IGUAL);
    r4_espera();
    chk_cond(q, "[=] lo resuelve: Newton", ui_pantalla() == P_RESULTADO && ui_res_nl() && !ve("corregirlo") &&
             ve("Punto de operaci" S_oo "n: Newton, "));
    k = r4_el(ui_circuito(), "Q1");
    chk_cond(q, "IC = 2,454 mA en la tabla de valores", k >= 0 && fabs(ui_valores()->I[k] - 2.4541907177293845e-3) < 1e-12);
    TECLAS(T_TAB);
    chk_cond(q, "[tab]: su regi" S_oo "n y su segundo puerto", ui_pantalla() == P_TABLA && ve("activa") && ve("Q1  VBE ") &&
             ve("  IB ") && ve("Kirchhoff"));
    r4_vuelca("e4_bjt_tabla");
    TECLAS(T_ESC, T_ENTER);
    chk_cond(q, "[enter]: sin paso a paso", ve("Sin paso a paso: Q1 no es lineal."));
    TECLAS(T_ESC, T_LETRA('I'));
    r4_espera();
    chk_cond(q, "[I] lo barre (E1)", ui_pantalla() == P_BARRIDO && barr_res()->fin == FIN_COMPLETO && barr_res()->huecos == 0 &&
             ve("frente a E1"));
    TECLAS(T_ESC, T_LETRA('B'));
    r4_espera();
    chk_cond(q, "[B] el Bode (de peque" S_nn "a se" S_nn "al)", ui_pantalla() == P_BODE && bode_res()->fin == FIN_COMPLETO &&
             (bode_res()->d.avisos & SIM_AV_PEQ_SENAL));
    TECLAS(T_ESC);
    k = r4_el(&c, "E1");                                /* con una senoidal (12 V de pico): se emula */
    c.el[k].variante = ONDA_SENO;
    c.el[k].par[Q_FREC - Q_P1].v = 50.0; c.el[k].par[Q_FREC - Q_P1].est = M_DATO;
    c.el[k].par[Q_BASE - Q_P1].v = 0.0; c.el[k].par[Q_BASE - Q_P1].est = M_DATO;
    r3u_abre(&c);
    TECLAS(T_IGUAL);
    r4_espera();
    chk_cond(q, "con una senoidal, [=] lo emula (en corte, activa, saturaci" S_oo "n e inversa)",
             sim_res()->fin == FIN_COMPLETO && sim_res()->d.estado == SIM_OK &&
             (ui_pantalla() == P_OSC || ui_pantalla() == P_EMU_ESQ));
    for (i = 0; i < 6 && ui_pantalla() != P_EMU_ESQ; i++) TECLAS(T_TAB);
    chk_cond(q, "P_EMU_ESQ con Newton", ui_pantalla() == P_EMU_ESQ && ve("iteraciones/paso"));
    limpia_mis();

    /* ---- Los modelos 41 y 42 por teclas: la lección 13 llega a 10 ---- */
    q = "[66] por teclas";
    for (l13 = 0; l13 < N_LECCIONES && strncmp(LECCIONES[l13].titulo, "13.", 3) != 0; l13++) ;
    chk_cond(q, "la lecci" S_oo "n 13 existe", l13 < N_LECCIONES);
    ui_init();
    TECLAS(T_ENTER, T_ABAJO, T_ENTER);                   /* portada, Aprende */
    repite(T_ABAJO, l13);
    TECLAS(T_ENTER, T_DER);
    chk_cond(q, "la teor" S_ii "a 13.2: el BJT", ve("TRANSISTOR BJT") && ve("recta de carga"));
    r4_vuelca("e4_bjt_teoria");
    TECLAS(T_DER);
    chk_cond(q, "los modelos de la lecci" S_oo "n 13", ui_pantalla() == P_EJEMPLOS);
    repite(T_ABAJO, 8);
    chk_cond(q, "el 41 (el noveno): [enter] calcular", ve("El transistor como interruptor") && ve("[enter] calcular"));
    TECLAS(T_ENTER);
    chk_cond(q, "el 41: P_RESULTADO, el LED a tope", ui_pantalla() == P_RESULTADO && ui_res_nl() &&
             ve("Punto de operaci" S_oo "n: Newton") && ve("rojo 21,5mA 108 %"));
    r4_vuelca("e4_bjt_modelo41");
    TECLAS(T_TAB);
    chk_cond(q, "el 41: Q1 en saturaci" S_oo "n", ui_pantalla() == P_TABLA && ve("saturaci" S_oo "n"));
    TECLAS(T_ESC, T_ESC, T_ABAJO);
    chk_cond(q, "el 42, el d" S_ee "cimo", ve("Emisor com" S_uu "n: el punto Q"));
    TECLAS(T_ENTER);
    chk_cond(q, "el 42: P_RESULTADO", ui_pantalla() == P_RESULTADO && ui_res_nl());
    modelo_cargar(41, &c);                              /* sus elementos, en el orden de P_RESULTADO */
    k = r4_el(&c, "A1");
    chk_cond(q, "el 42: A1 marca IC = 2,454 mA", k >= 0 && fabs(ui_valores()->I[k] - 2.4541907177293845e-3) < 1e-12);
    r4_vuelca("e4_bjt_modelo42");
    TECLAS(T_ESC, T_ESC, T_ESC);

    chk_cond("interfaz del BJT", "ning" S_uu "n texto fuera de la pantalla", gfx_desbordes() == 0);
}

static void test_e4_bjt(void)
{
    printf("[64] Etapa 4: el BJT (motor, peque" S_nn "a se" S_nn "al y familia)\n");
    q4_motor();
    q4_bode();
    q4_familia();
    printf("[65c] Etapa 4: el BJT en la interfaz\n");
    q4_interfaz();
}
