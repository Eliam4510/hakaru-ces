/* --------------------------------------------------------------------------
 *  65r. Etapa 4: el transitorio no lineal de la tercera revisión (tran_a)
 *
 *  Una prueba por hallazgo (las que tienen arreglo fallan sin él,
 *  comprobado por mutación):
 *  - H1: un interruptor con BJT (o con LED) sin C ni L y una cuadrada: tras
 *    el flanco no hay Katzenelson (no hay por tramos) y partir el paso no
 *    acorta el salto. Lo arregló el reintento de 50 iteraciones de
 *    un_paso_nl (el arreglo B del oráculo): se emula entero y cada punto es
 *    el de su punto de operación. Y el mensaje de NC_TR solo aconseja «otro
 *    modelo del diodo (con Vγ)» a un diodo que lo tiene.
 *  - H2: el superdiodo (AO ideal + 1N4148): una prueba de Illinois cuyo
 *    Newton no converge en 12 iteraciones terminaba la emulación. Ahora,
 *    antes del error, 50 (como en un_paso_nl).
 *  - H3: AO ideal + seguidor de emisor con el cruce por cero en la malla:
 *    también lo cubre el reintento de un_paso_nl; con RE = 100 Ω el fallo
 *    salía en localiza_nl (el de H2).
 *  Sin C ni L la referencia es exacta: cada punto registrado es el punto de
 *  operación con la fuente en su valor de ese instante.
 *  Se incluye tras test_rev2_editor.inc (usa sr, saj, sr_init, sr_el,
 *  canal, a4_ao, a4_cc, q4_bjt, t4_seno, t4_d_cat, r4_el, r4_espera,
 *  r3u_abre, ve, TECLAS y los chk).
 * ----------------------------------------------------------------------- */

/* Resumen de la última emulación (con su mensaje de error, si lo hay) */
static void r3ta_resumen(const char *q)
{
    const SimRes *r = sim_res();
    char m[240];
    m[0] = 0;
    if (r->d.estado != SIM_OK) sim_mensaje(&sr, &r->d, m, sizeof m);
    printf("    %s: fin %d estado %d h %.4g pasos %ld subpasos %ld cambios %d  %s\n", q, r->fin, r->d.estado,
           r->h, r->pasos, r->nw.sub, r->nw.eventos, m);
}

static int r3ta_completa(void) { return sim_res()->fin == FIN_COMPLETO && sim_res()->d.estado == SIM_OK; }

/* Sin C ni L, cada punto del registro es el punto de operación con la
 * fuente kE en su valor de ese instante: el mayor |y - y_OP| del canal c en
 * unos 80 puntos repartidos y en los de alrededor de cada evento (los dos
 * del evento, con el mismo t, no: el y+ sale extrapolado). y_OP es la V
 * (es_i = 0) o la I (es_i = 1) del elemento kel; *nmir, cuántos se miran */
static double r3ta_frente_op(int c, int kE, int kel, int es_i, int *nmir)
{
    static double ts[SIM_MAX_MUESTRAS], ys[SIM_MAX_MUESTRAS];
    static SimRed s2;
    static Valores v;
    const SimReg *g = sim_reg();
    SimDiag d;
    int i, n = g->n, cada = n / 80 + 1;
    double peor = 0.0;
    memcpy(ts, g->t, (size_t)n * sizeof ts[0]);
    memcpy(ys, g->y[c], (size_t)n * sizeof ys[0]);
    *nmir = 0;
    for (i = 1; i + 1 < n; i++) {
        double y;
        int junto = ts[i + 1] == ts[i] || ts[i - 1] == ts[i];
        int cerca = (i + 2 < n && ts[i + 2] == ts[i + 1]) || (i >= 2 && ts[i - 2] == ts[i - 1]);
        if (junto || (i % cada && !cerca)) continue;
        s2 = sr;
        s2.el[kE].forma = ONDA_CC;
        s2.el[kE].p[0] = onda_valor(sr.el[kE].forma, sr.el[kE].p, ts[i]);
        if (sim_op_red(&s2, &v, &d) != SIM_OK) return 1e300;
        y = es_i ? v.I[kel] : v.V[kel];
        if (!(fabs(ys[i] - y) <= peor)) peor = fabs(ys[i] - y);     /* un NaN, también */
        (*nmir)++;
    }
    return peor;
}

/* Interruptor con NPN sin C ni L: VCC en 1; una cuadrada de 0 a VCC a 1 kHz
 * (la fuente 1) por RB a la base; RC de VCC al colector; el emisor a tierra.
 * El canal 0 es IC. Devuelve el BJT */
static int r3ta_interruptor(double Vcc, double RB, double RC)
{
    int k, kq;
    sr_init(5);
    a4_cc(0, 1, Vcc);
    k = sr_el(SE_E, 0, 2, 0.0); sr.el[k].forma = ONDA_CUADRADA;
    sr.el[k].p[0] = 0.5 * Vcc; sr.el[k].p[1] = 1e3; sr.el[k].p[2] = 50.0; sr.el[k].p[3] = 0.5 * Vcc;
    sr_el(SE_R, 2, 3, RB);
    sr_el(SE_R, 1, 4, RC);
    kq = q4_bjt(4, 3, 0, 1, 100.0, 0.0);
    canal(0, CANAL_I, kq); canal(1, CANAL_NODO, 4);
    return kq;
}

/* Un LED de color en sr, como lo arma el adaptador (la curva por (Vf, If)) */
static int r3ta_led(int a, int b, int color)
{
    const NlColor *co = &NL_COLOR[color];
    double ij = NL_IF - NL_GMIN * co->Vf, vd = co->Vf - co->Rs * ij, u = vd / (co->n * NL_VT);
    int k = sr_el(SE_D, a, b, 0.0);
    SimEl *e = &sr.el[k];
    e->modelo = NL_UNION;
    e->m[1] = co->n; e->m[2] = co->Rs;
    e->m[0] = ij / ((u > NL_XLIM ? exp(NL_XLIM) * (1.0 + u - NL_XLIM) : exp(u)) - 1.0);
    e->m[5] = nl_vcrit(e->m[0], e->m[1] * NL_VT);
    e->p[0] = NL_IM; e->p[1] = NL_IF;
    return k;
}

/* El superdiodo (rectificador de precisión): e = 5 sen(2π·1000t + fase) en
 * la +; la salida del AO ideal (el elemento 1, en el nodo 2) al ánodo; el
 * cátodo a la - (nodo 3) y RL = 1 kΩ. d: 0 1N4148, 1 1N4007, 2 1N4148 al
 * revés (la media onda negativa), 3 LED azul. Canales: V(3) y Vsal */
static void r3ta_superdiodo(int d, double fase)
{
    sr_init(4);
    t4_seno(0, 1, 5.0, 1e3, fase);
    a4_ao(1, 3, 2, 0, 0, 0.0);
    if (d == 0) t4_d_cat(2, 3, &NL_1N4148);
    else if (d == 1) t4_d_cat(2, 3, &NL_1N4007);
    else if (d == 2) t4_d_cat(3, 2, &NL_1N4148);
    else r3ta_led(2, 3, LED_AZUL);
    sr_el(SE_R, 3, 0, 1e3);
    canal(0, CANAL_NODO, 3); canal(1, CANAL_NODO, 2);
}

/* AO ideal (el elemento 2) + seguidor de emisor dentro del lazo: e en la +;
 * la salida del AO a la base de un NPN (pol = 1; el colector a 15 V) o de un
 * PNP (pol = -1; a -15 V); el emisor a la - y RE a tierra */
static void r3ta_ao_bjt(int pol, double fase, double RE)
{
    sr_init(5);
    t4_seno(0, 1, 5.0, 1e3, fase);
    a4_cc(0, 4, pol * 15.0);
    a4_ao(1, 3, 2, 0, 0, 0.0);
    q4_bjt(4, 2, 3, pol, 100.0, 0.0);
    sr_el(SE_R, 3, 0, RE);
    canal(0, CANAL_NODO, 3); canal(1, CANAL_NODO, 2);
}

/* El mensaje de NC_TR del elemento k de sr: 1 si aconseja otro modelo */
static int r3ta_aconseja(int k, char *m, int cap)
{
    SimDiag d;
    memset(&d, 0, sizeof d);
    d.estado = SIM_NO_CONVERGE; d.campo = NC_TR; d.elem = k; d.nodo = -1; d.t = 3.21e-3;
    sim_mensaje(&sr, &d, m, cap);
    return strstr(m, "otro modelo") != 0;
}

static void test_rev3_tran_a(void)
{
    static Circuito c;
    static const double RB[2] = { 10e3, 4.7e3 }, RC[2] = { 1e3, 330.0 }, VCC[2] = { 5.0, 9.0 };
    static const char *const NOM_D[4] = { "1N4148", "1N4007", "1N4148 al rev" S_ee "s", "LED azul" };
    char q[120], m[240];
    int j, k, kq, nm, bien, n;
    double err;

    printf("[65r] Etapa 4: el transitorio no lineal de la tercera revisi" S_oo "n (tran_a)\n");

    /* ==== H1. El interruptor con BJT sin C ni L y una cuadrada ==== */
    for (j = 0; j < 2; j++) {
        kq = r3ta_interruptor(VCC[j], RB[j], RC[j]);
        sim_emular(&sr, &saj);
        sprintf(q, "H1 NPN %g V, RB %g, RC %g, cuadrada", VCC[j], RB[j], RC[j]);
        r3ta_resumen(q);
        chk_cond(q, "se emula entero (sin el reintento de 50: NO_CONVERGE en t = 4,88 ns)", r3ta_completa());
        err = r3ta_frente_op(0, 1, kq, 1, &nm);
        printf("      IC frente a su punto de operaci" S_oo "n: %.3g A en %d puntos\n", err, nm);
        chk_cond(q, "cada punto es el de su punto de operaci" S_oo "n (1e-12 A)", nm > 60 && err <= 1e-12);
    }
    /* por la interfaz: el modelo 41 con E1 cuadrada de 5 V a 1 kHz; en la
     * mitad alta el LED lleva los 21,5069 mA del modelo */
    modelo_cargar(40, &c);
    k = r4_el(&c, "E1");
    c.el[k].variante = ONDA_CUADRADA;
    c.el[k].par[Q_FREC - Q_P1].v = 1e3; c.el[k].par[Q_FREC - Q_P1].est = M_DATO;
    r3u_abre(&c);
    TECLAS(T_IGUAL);
    r4_espera();
    k = r4_el(ui_circuito(), "D1");
    printf("    H1 modelo 41 con E1 cuadrada: fin %d estado %d; I(D1) m" S_aa "x %.12g A; \"no converge\" %d\n",
           sim_res()->fin, sim_res()->d.estado, k >= 0 ? sim_res()->Imax[k] : 0.0, ve("no converge"));
    chk_cond("H1 modelo 41 con E1 cuadrada", "[=] lo emula entero, sin \"no converge\"",
             sim_res()->fin == FIN_COMPLETO && !ve("no converge"));
    chk_cond("H1 modelo 41 con E1 cuadrada", "el LED llega a los 21,5069 mA del modelo (1e-9)",
             k >= 0 && fabs(sim_res()->Imax[k] - 21.506917662571294e-3) <= 1e-9 * 21.506917662571294e-3);
    TECLAS(T_ESC);

    /* el mensaje de NC_TR: «otro modelo del diodo (con Vγ)» solo a un diodo
     * exponencial o del catálogo; a un BJT, a un LED o al AO, no */
    kq = r3ta_interruptor(5.0, 10e3, 1e3);
    chk_cond("H1 mensaje de NC_TR con un BJT", "no aconseja otro modelo (con V" S_GAMMA ")", !r3ta_aconseja(kq, m, sizeof m));
    printf("      %s\n", m);
    sr_init(3);
    k = sr_el(SE_E, 0, 1, 0.0); sr.el[k].forma = ONDA_CUADRADA;
    sr.el[k].p[0] = 2.5; sr.el[k].p[1] = 1e3; sr.el[k].p[2] = 50.0; sr.el[k].p[3] = 2.5;
    sr_el(SE_R, 1, 2, 330.0);
    k = r3ta_led(2, 0, LED_BLANCO);
    chk_cond("H1 mensaje de NC_TR con un LED", "no aconseja otro modelo (con V" S_GAMMA ")", !r3ta_aconseja(k, m, sizeof m));
    printf("      %s\n", m);
    r3ta_superdiodo(0, 0.0);
    chk_cond("H1 mensaje de NC_TR con un AO", "no aconseja otro modelo (con V" S_GAMMA ")", !r3ta_aconseja(1, m, sizeof m));
    chk_cond("H1 mensaje de NC_TR con un 1N4148", "aconseja otro modelo del diodo (con V" S_GAMMA ")",
             r3ta_aconseja(2, m, sizeof m) && strstr(m, "otro modelo del diodo (con V" S_GAMMA ")"));
    printf("      %s\n", m);
    /* por la interfaz (sin la SimRed, los índices del circuito): el LED y el
     * BJT del modelo 41 */
    modelo_cargar(40, &c);
    {
        SimDiag d;
        int kd = r4_el(&c, "D1"), kb = r4_el(&c, "Q1");
        memset(&d, 0, sizeof d);
        d.estado = SIM_NO_CONVERGE; d.campo = NC_TR; d.nodo = -1; d.t = 4.883e-9;
        d.elem = kd;
        sim_mensaje_circ(&c, 0, &d, m, sizeof m);
        printf("      %s\n", m);
        chk_cond("H1 mensaje de NC_TR del modelo 41", "a D1 (el LED) no le aconseja otro modelo",
                 kd >= 0 && strstr(m, "D1") && !strstr(m, "otro modelo"));
        d.elem = kb;
        sim_mensaje_circ(&c, 0, &d, m, sizeof m);
        chk_cond("H1 mensaje de NC_TR del modelo 41", "a Q1 no le aconseja otro modelo",
                 kb >= 0 && strstr(m, "Q1") && !strstr(m, "otro modelo"));
    }

    /* ==== H2. El superdiodo: una prueba de Illinois que no converge en 12
     *      iteraciones ya no acaba la emulación ==== */
    bien = n = 0;
    for (j = 0; j < 8; j++) {
        double fase = (j & 1) ? 7.0 : 0.0;
        r3ta_superdiodo(j >> 1, fase);
        sim_emular(&sr, &saj);
        sprintf(q, "H2 superdiodo con %s, fase %g", NOM_D[j >> 1], fase);
        r3ta_resumen(q);
        err = r3ta_completa() ? r3ta_frente_op(1, 0, 1, 0, &nm) : 1e300;
        if (r3ta_completa()) printf("      Vsal frente a su punto de operaci" S_oo "n: %.3g V en %d puntos\n", err, nm);
        n++;
        if (r3ta_completa() && nm > 60 && err <= 1e-9) bien++;
    }
    chk_cond("H2 superdiodo (AO ideal)", "los 8 se emulan enteros y cada Vsal es la de su OP (1e-9 V)", bien == n);
    r3ta_superdiodo(0, 1.0);
    sim_emular(&sr, &saj);
    r3ta_resumen("H2 superdiodo con 1N4148, fase 1");
    chk_cond("H2 superdiodo con 1N4148, fase 1", "se emula entero (sin el arreglo: NO_CONVERGE en 1,5 ms)", r3ta_completa());

    /* ==== H3. AO ideal + seguidor de emisor: el cruce por cero en la malla
     *      (t = T/2 con h = T/400) ==== */
    bien = n = 0;
    for (j = 0; j < 3; j++) {
        int pol = j == 2 ? -1 : 1;
        double RE = j == 0 ? 1e3 : 100.0;
        r3ta_ao_bjt(pol, 0.0, RE);
        sim_emular(&sr, &saj);
        sprintf(q, "H3 AO ideal + %s seguidor, RE %g, fase 0", pol > 0 ? "NPN" : "PNP", RE);
        r3ta_resumen(q);
        err = r3ta_completa() ? r3ta_frente_op(1, 0, 2, 0, &nm) : 1e300;
        if (r3ta_completa()) printf("      Vsal frente a su punto de operaci" S_oo "n: %.3g V en %d puntos\n", err, nm);
        chk_cond(q, "se emula entero y cada Vsal es la de su OP (1e-9 V)", r3ta_completa() && nm > 60 && err <= 1e-9);
    }
}
