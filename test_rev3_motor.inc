/* --------------------------------------------------------------------------
 *  54a. Revisión de la Etapa 3: motor, calculadora y Bode
 *
 *  Cada caso es un defecto que confirmó la revisión adversarial de la
 *  Etapa 3, ya arreglado: el motor de CA (hallazgos 1 y 2), la calculadora
 *  de CA (3, 4, 5 y los dos cambios a mano del LC serie con L y C buscadas)
 *  y el Bode (6 a 12). Se incluye en host_test.c (usa chk_cond, chk_t, falla,
 *  TECLAS, repite, ve, vuelca, sr_init, sr_el, sr_alterna, ca_sr, car,
 *  canal, bode_sr, dibuja_rc, dibuja_t1 y el_de_etiqueta).
 * ----------------------------------------------------------------------- */

static Circuito r3m_c;
static Red      r3m_r;
static CaRes    r3m_res;
static CaThev   r3m_th;
static SimRed   r3m_s;
static int      r3m_nn;

/* ---- Circuitos en la rejilla (pasan por red_extraer y el_de) ---- */
static int r3m_dato(int k, double v) { r3m_c.el[k].val.v = v; r3m_c.el[k].val.est = M_DATO; return k; }

static int r3m_fasor_rej(int k, double ang)
{
    r3m_c.el[k].variante = FORMA_FASOR;
    r3m_c.el[k].val.ang = ang; r3m_c.el[k].val.est_ang = M_DATO;
    return k;
}

/* Dos elementos en paralelo (columnas 1 y 2, de arriba abajo) y, con con_j,
 * una J 1∠0 en la columna 0 que los alimenta. A y B en los bornes del
 * primero. Una Z lleva su ángulo en a1 / a2 */
static void r3m_tanque(int t1, double v1, double a1, int t2, double v2, double a2, int con_j, double f)
{
    int k;
    circ_init(&r3m_c, "tanque");
    k = r3m_dato(circ_elemento(&r3m_c, (TipoEl)t1, 1, 0, DIR_ABA), v1);
    if (t1 == EL_IMP) { r3m_c.el[k].val.ang = a1; r3m_c.el[k].val.est_ang = M_DATO; }
    k = r3m_dato(circ_elemento(&r3m_c, (TipoEl)t2, 2, 0, DIR_ABA), v2);
    if (t2 == EL_IMP) { r3m_c.el[k].val.ang = a2; r3m_c.el[k].val.est_ang = M_DATO; }
    circ_cable(&r3m_c, 1, 0, DIR_DER); circ_cable(&r3m_c, 1, 1, DIR_DER);
    if (con_j) {
        r3m_fasor_rej(r3m_dato(circ_elemento(&r3m_c, EL_CORR, 0, 1, DIR_ARR), 1.0), 0.0);
        circ_cable(&r3m_c, 0, 0, DIR_DER); circ_cable(&r3m_c, 0, 1, DIR_DER);
    }
    r3m_c.frec.v = f; r3m_c.frec.est = M_DATO;
    r3m_c.term_x[0] = 1; r3m_c.term_y[0] = 0; r3m_c.term_x[1] = 1; r3m_c.term_y[1] = 1;
    red_extraer(&r3m_c, &r3m_r);
}

/* ---- Circuitos sin dibujo (la Red a mano, como en la calculadora) ---- */
static void r3m_ini(double f)
{
    circ_init(&r3m_c, "r3m");
    memset(&r3m_r, 0, sizeof r3m_r);
    r3m_nn = 2;
    if (f > 0.0) { r3m_c.frec.v = f; r3m_c.frec.est = M_DATO; }
}

/* v > 0: dato; v == 0: vacío; v < 0: [?!] */
static int r3m_el(TipoEl t, int a, int b, double v)
{
    int k = r3m_c.nel;
    Elemento *e = &r3m_c.el[k];
    memset(e, 0, sizeof *e);
    e->ref = -1;
    e->tipo = (unsigned char)t;
    e->xa = e->xb = (unsigned char)(k % REJ_COLS);
    e->ya = e->yb = (unsigned char)(k / REJ_COLS);
    if (v > 0.0) { e->val.v = v; e->val.est = M_DATO; }
    else if (v < 0.0) e->val.est = M_BUSCAR;
    sprintf(e->etiq, "%c%d", tipo_info(t)->letra ? tipo_info(t)->letra : 'W', k + 1);
    r3m_r.a[k] = a; r3m_r.b[k] = b;
    if (a >= r3m_nn) r3m_nn = a + 1;
    if (b >= r3m_nn) r3m_nn = b + 1;
    r3m_c.nel++;
    return k;
}

static int r3m_fasor(TipoEl t, int a, int b, double ef, double ang)
{
    int k = r3m_el(t, a, b, ef);
    r3m_c.el[k].variante = FORMA_FASOR;
    r3m_c.el[k].val.ang = ang; r3m_c.el[k].val.est_ang = M_DATO;
    return k;
}

static void r3m_cierra(void)
{
    r3m_r.nn = r3m_nn; r3m_r.nel = r3m_c.nel; r3m_r.error = RED_OK; r3m_r.elem_error = -1;
    red_cierra(&r3m_c, &r3m_r);
}

static const CalRes *r3m_calcula(void)
{
    r3m_cierra();
    cal_iniciar(&r3m_c, &r3m_r);
    while (cal_avanzar(~0ul)) {}
    return cal_res();
}

/* El mensaje de la calculadora (en r3m_msg) */
static char r3m_msg[800];
static const char *r3m_mensaje(const CalRes *s)
{
    cal_mensaje(&r3m_c, s, r3m_msg, sizeof r3m_msg);
    return r3m_msg;
}

static int r3m_bits(unsigned u) { int n = 0; while (u) { n += (int)(u & 1u); u >>= 1; } return n; }

/* ¿El elemento k de la SimRed es una L o una C? */
static int r3m_lc(const SimRed *s, int k) { return k >= 0 && k < s->nel && (s->el[k].tipo == SE_L || s->el[k].tipo == SE_C); }

/* Resonancia con nombre: RESONANCIA, el elemento es una L o una C (o una Z
 * con tipo) y el lazo lleva a las dos de la pareja */
static void r3m_resonancia(const char *q, int e, int tipo_z)
{
    int k = car.d.elem, n = 0, j;
    chk_cond(q, "SIM_RESONANCIA (no SIM_SINGULAR ni SIM_OK)", e == SIM_RESONANCIA);
    chk_cond(q, tipo_z ? "nombra a una de las Z" : "nombra a la L o a la C, no al instrumento ni a la fuente",
             tipo_z ? (k >= 0 && k < sr.nel && sr.el[k].tipo == SE_Z) : r3m_lc(&sr, k));
    for (j = 0; j < sr.nel; j++) if ((car.d.lazo >> j) & 1u) n++;
    chk_cond(q, "el lazo marca a la pareja que resuena", n == 2 && ((car.d.lazo >> k) & 1u));
}

static void r3m_motor(void)
{
    const double L = 10e-3, C = 1e-6, f0 = 1.0 / (2.0 * PI_T * sqrt(L * C));
    const double w50 = 2.0 * PI_T * 50.0;
    char m[300];
    int e, k, kz, j;

    /* ---- Hallazgo 1: una Z de ±90° es reactiva pura (R = 0, no |Z|·6e-17) ---- */
    r3m_tanque(EL_IMP, 100.0, 90.0, EL_IMP, 100.0, -90.0, 1, 50.0);
    {
        SimDiag d;
        memset(&d, 0, sizeof d);
        sim_red_de_ex(&r3m_c, &r3m_r, &r3m_s, &d, 0);
        for (k = 0, j = 0; k < r3m_s.nel; k++)
            if (r3m_s.el[k].tipo == SE_Z && r3m_s.el[k].val == 0.0 && fabs(fabs(r3m_s.el[k].p[0]) - 100.0) < 1e-12) j++;
        chk_cond("Z a " S_PM "90" S_DEG, "Z 100" S_ANG "90" S_DEG " y Z 100" S_ANG "-90" S_DEG ": R = 0 exacta y X = " S_PM "100", j == 2);
    }
    e = ca_analiza(&r3m_c, &r3m_r, &r3m_res);
    chk_cond("tanque de Z", "J " S_PAR " Z 100" S_ANG "90" S_DEG " " S_PAR " Z 100" S_ANG "-90" S_DEG ": no da SIM_OK con 1e17 V", e != SIM_OK);
    chk_cond("tanque de Z", "da SIM_RESONANCIA, como el mismo tanque con L y C", e == SIM_RESONANCIA);
    sim_mensaje_circ(&r3m_c, 0, &r3m_res.d, m, sizeof m);
    chk_cond("tanque de Z", "el mensaje dice que una Z resuena sin pérdidas", (strstr(m, "Z1") || strstr(m, "Z2")) &&
             strstr(m, "resuena sin p") && !strstr(m, "una soluci"));
    chk_cond("tanque de Z", "el lazo marca a las dos Z", r3m_bits(r3m_res.d.lazo) == 2);
    r3m_tanque(EL_BOB, 100.0 / w50, 0.0, EL_COND, 1.0 / (w50 * 100.0), 0.0, 1, 50.0);
    chk_cond("tanque de Z", "control: J " S_PAR " L (j100) " S_PAR " C (-j100) da SIM_RESONANCIA",
             ca_analiza(&r3m_c, &r3m_r, &r3m_res) == SIM_RESONANCIA);
    /* el ohmímetro sobre Z1 || Z2 */
    r3m_tanque(EL_IMP, 100.0, 90.0, EL_IMP, 100.0, -90.0, 0, 50.0);
    memset(&r3m_th, 0, sizeof r3m_th);
    e = ca_thevenin(&r3m_c, &r3m_r, 50.0, &r3m_th);
    chk_cond("ohm" S_ii "metro", "Z(AB) de Z 100" S_ANG "90" S_DEG " " S_PAR " Z 100" S_ANG "-90" S_DEG ": resonancia, no 8e17 " S_OHM " resistiva",
             e == SIM_RESONANCIA);
    /* una Z de 90° con corriente: no disipa (R exacta 0 en el análisis) */
    r3m_ini(50.0);
    r3m_fasor(EL_FUENTE, 0, 1, 10.0, 0.0);
    r3m_el(EL_RES, 1, 2, 10.0);
    kz = r3m_el(EL_IMP, 2, 0, 100.0);
    r3m_c.el[kz].val.ang = 90.0; r3m_c.el[kz].val.est_ang = M_DATO;
    r3m_cierra();
    e = ca_analiza(&r3m_c, &r3m_r, &r3m_res);
    chk_cond("Z a 90" S_DEG, "E + R + Z 100" S_ANG "90" S_DEG ": SIM_OK", e == SIM_OK);
    chk_cond("Z a 90" S_DEG, "la Z vale 0 + j100 exactos", r3m_res.Z[kz].re == 0.0 && r3m_res.Z[kz].im == 100.0);
    chk_cond("Z a 90" S_DEG, "su " S_ANG "Z es 90" S_DEG " exactos", r3m_res.val_ang[kz] == 90.0);
    /* la calculadora: |Z| buscada con ∠Z = 90° dato; |I(R2)| = 0,5 A -> |Z| = √300 */
    {
        const CalRes *s;
        int kr;
        r3m_ini(50.0);
        r3m_fasor(EL_FUENTE, 0, 1, 10.0, 0.0);
        kr = r3m_el(EL_RES, 1, 2, 10.0);
        kz = r3m_el(EL_IMP, 2, 0, -1.0);
        r3m_c.el[kz].val.ang = 90.0; r3m_c.el[kz].val.est_ang = M_DATO;
        circ_mag(&r3m_c.el[kr], Q_I)->v = 0.5; circ_mag(&r3m_c.el[kr], Q_I)->est = M_DATO;
        s = r3m_calcula();
        chk_cond("calculadora, Z a 90" S_DEG, "|Z3| buscada con " S_ANG "Z3 = 90" S_DEG ": una soluci" S_oo "n", s->estado == CAL_OK && s->nsol == 1);
        chk_t("calculadora, Z a 90" S_DEG, "|Z3| = " S_SQRT "300 " S_OHM, s->nsol ? s->sol[0].x[0] : 0.0, sqrt(300.0), 17.3, 1e-8);
        chk_cond("calculadora, Z a 90" S_DEG, "en la soluci" S_oo "n la Z3 es reactiva pura: R = 0 exacta",
                 s->nsol == 1 && s->sol[0].g.Z[kz].re == 0.0);
        /* y a -90°: |Z| = √300 también (a 50 Hz, -j17,3) */
        r3m_c.el[kz].val.ang = -90.0;
        s = r3m_calcula();
        chk_cond("calculadora, Z a -90" S_DEG, "en la soluci" S_oo "n la Z3 es reactiva pura: R = 0 exacta",
                 s->estado == CAL_OK && s->nsol == 1 && s->sol[0].g.Z[kz].re == 0.0);
    }

    /* ---- Hallazgo 2: la resonancia sin pérdidas es RESONANCIA aunque la
     *      columna mala sea la de un A, W, S o E, o solo haya Z ---- */
    /* desde el dibujo: E, L, C y un amperímetro dibujado el último */
    circ_init(&r3m_c, "serie");
    r3m_fasor_rej(r3m_dato(circ_elemento(&r3m_c, EL_FUENTE, 0, 1, DIR_ARR), 1.0), 0.0);
    r3m_dato(circ_elemento(&r3m_c, EL_BOB, 0, 0, DIR_DER), L);
    r3m_dato(circ_elemento(&r3m_c, EL_COND, 1, 0, DIR_ABA), C);
    k = circ_elemento(&r3m_c, EL_AMP, 1, 1, DIR_IZQ);
    r3m_c.el[k].variante = MED_CA;
    r3m_c.frec.v = f0; r3m_c.frec.est = M_DATO;
    red_extraer(&r3m_c, &r3m_r);
    e = ca_analiza(&r3m_c, &r3m_r, &r3m_res);
    sim_mensaje_circ(&r3m_c, 0, &r3m_res.d, m, sizeof m);
    chk_cond("resonancia E-L-C-A", "el A dibujado el " S_uu "ltimo: SIM_RESONANCIA", e == SIM_RESONANCIA);
    chk_cond("resonancia E-L-C-A", "el mensaje dice que resuena L1 o C1, y no culpa al A1", (strstr(m, "L1") || strstr(m, "C1")) &&
             strstr(m, "resuena sin p") && !strstr(m, "A1"));
    chk_cond("resonancia E-L-C-A", "el lazo marca a la L y a la C", r3m_bits(r3m_res.d.lazo) == 2);
    /* con la SimRed a mano: W, S cerrado, E la última, J sobre (L + A) || C */
    sr_init(4); sr_alterna(SE_E, 0, 1, 1, 0, f0); sr_el(SE_L, 1, 2, L); sr_el(SE_C, 2, 3, C);
    k = sr_el(SE_AMI, 3, 0, 0.0); sr.el[k].instr = 'A'; sr.el[k].med = MED_CA;
    r3m_resonancia("resonancia E-L-C-A (SimRed)", ca_sr(0.0), 0);
    sr_init(4); sr_alterna(SE_E, 0, 1, 1, 0, f0); sr_el(SE_L, 1, 2, L); sr_el(SE_C, 2, 3, C);
    k = sr_el(SE_AMI, 3, 0, 0.0); sr.el[k].instr = 'W'; sr.el[k].med = MED_CC;
    r3m_resonancia("resonancia E-L-C-W", ca_sr(0.0), 0);
    sr_init(4); sr_alterna(SE_E, 0, 1, 1, 0, f0); sr_el(SE_L, 1, 2, L); sr_el(SE_C, 2, 3, C);
    k = sr_el(SE_S, 3, 0, 0.0); sr.el[k].cerrado0 = 1;
    r3m_resonancia("resonancia E-L-C-S cerrado", ca_sr(0.0), 0);
    sr_init(3); sr_el(SE_L, 1, 2, L); sr_el(SE_C, 2, 0, C); sr_alterna(SE_E, 0, 1, 1, 0, f0);
    r3m_resonancia("resonancia L-C-E (la E la " S_uu "ltima)", ca_sr(0.0), 0);
    sr_init(3); sr_alterna(SE_J, 0, 1, 1, 0, f0);
    sr_el(SE_L, 2, 0, L); sr_el(SE_C, 1, 0, C);
    k = sr_el(SE_AMI, 1, 2, 0.0); sr.el[k].instr = 'A'; sr.el[k].med = MED_CA;
    r3m_resonancia("resonancia J sobre (L + A) " S_PAR " C", ca_sr(0.0), 0);
    /* solo Z, reactivas puras: E + Z j100 + Z -j100 en serie a 50 Hz */
    sr_init(3); sr_alterna(SE_E, 0, 1, 1, 0, 50.0);
    k = sr_el(SE_Z, 1, 2, 0.0); sr.el[k].p[0] = 100.0; sr.el[k].p[1] = 50.0;
    k = sr_el(SE_Z, 2, 0, 0.0); sr.el[k].p[0] = -100.0; sr.el[k].p[1] = 50.0;
    r3m_resonancia("resonancia E-Z-Z", ca_sr(0.0), 1);
    /* control: un pelo fuera de f0 se resuelve */
    sr_init(4); sr_alterna(SE_E, 0, 1, 1, 0, f0 * 1.001); sr_el(SE_L, 1, 2, L); sr_el(SE_C, 2, 3, C);
    k = sr_el(SE_AMI, 3, 0, 0.0); sr.el[k].instr = 'A'; sr.el[k].med = MED_CA;
    chk_cond("resonancia E-L-C-A", "control: a 1,001" S_CDOT "f0 se resuelve (SIM_OK)", ca_sr(0.0) == SIM_OK);
}

/* RLC serie a 1 kHz: E = 1 V, R, L = 10 mH y C [?!]; dato |V(C4)| */
static const CalRes *r3m_rlc_vc(double R, double vc, int *kc)
{
    r3m_ini(1000.0);
    r3m_fasor(EL_FUENTE, 0, 1, 1.0, 0.0);
    r3m_el(EL_RES, 1, 2, R);
    r3m_el(EL_BOB, 2, 3, 0.01);
    *kc = r3m_el(EL_COND, 3, 0, -1.0);
    circ_mag(&r3m_c.el[*kc], Q_V)->v = vc; circ_mag(&r3m_c.el[*kc], Q_V)->est = M_DATO;
    return r3m_calcula();
}

/* LC serie con L y C buscadas: E = 10 V a 50 Hz, un A que marca 1 A, R
 * (0 = sin R) y V(C) = 20 V. Con como_z, la C es una Z capacitiva: |Z|
 * buscada y ∠Z = -90° (dato). rp > 0: una R conocida en paralelo con la E
 * (no toca al LC; fija la impedancia típica Z0 = rp, y con ella las semillas) */
static const CalRes *r3m_lc_serie(double R, int como_z, double rp, int *kl, int *kc)
{
    int ka, n = 2;
    r3m_ini(50.0);
    r3m_fasor(EL_FUENTE, 0, 1, 10.0, 0.0);
    if (rp > 0.0) r3m_el(EL_RES, 1, 0, rp);
    ka = r3m_el(EL_AMP, 1, 2, 0.0);
    if (R > 0.0) { r3m_el(EL_RES, 2, 3, R); n = 3; }
    *kl = r3m_el(EL_BOB, n, n + 1, -1.0);
    *kc = r3m_el(como_z ? EL_IMP : EL_COND, n + 1, 0, -1.0);
    if (como_z) { r3m_c.el[*kc].val.ang = -90.0; r3m_c.el[*kc].val.est_ang = M_DATO; }
    circ_mag(&r3m_c.el[ka], Q_V)->est = 0;
    circ_mag(&r3m_c.el[ka], Q_I)->v = 1.0; circ_mag(&r3m_c.el[ka], Q_I)->est = M_DATO;
    circ_mag(&r3m_c.el[*kc], Q_V)->v = 20.0; circ_mag(&r3m_c.el[*kc], Q_V)->est = M_DATO;
    return r3m_calcula();
}

/* El índice de la incógnita del valor del elemento k en s->inc, o -1 */
static int r3m_inc_de(const CalRes *s, int k)
{
    int i;
    for (i = 0; i < s->ninc; i++) if (s->inc[i].el == k && s->inc[i].que == IN_VAL) return i;
    return -1;
}

static void r3m_calculadora(void)
{
    const CalRes *s;
    const char *m;
    int kc, kl, kr, kw, i;
    double w = 2.0 * PI_T * 50.0;

    /* ---- Hallazgo 3: INCOMPAT con una incógnita, el rango de TODAS las
     *      evaluaciones (el pico de resonancia que la malla salta) ---- */
    s = r3m_rlc_vc(3.0, 25.0, &kc);
    m = r3m_mensaje(s);
    chk_cond("INCOMPAT RLC (Q " S_APROX " 21)", "V(C4) = 25 V: ningún valor de C4 lo da", s->estado == CAL_INCOMPAT);
    chk_t("INCOMPAT RLC (Q " S_APROX " 21)", "el máximo es el pico real, E" S_CDOT S_SQRT "(R" S_SUP2 " + XL" S_SUP2 ")/R = 20,968 V (no 10,3 V)",
          s->hi_dato, sqrt(9.0 + pow(2.0 * PI_T * 10.0, 2.0)) / 3.0, 20.97, 1e-6);
    chk_cond("INCOMPAT RLC (Q " S_APROX " 21)", "el mensaje dice «a 21 V» (no «a 10,3 V»)", strstr(m, "a 21 V.") != 0);
    s = r3m_rlc_vc(3.0, 15.0, &kc);
    chk_cond("INCOMPAT RLC (Q " S_APROX " 21)", "control: V(C4) = 15 V (bajo el pico) da dos soluciones", s->estado == CAL_OK && s->nsol == 2);
    s = r3m_rlc_vc(0.1, 700.0, &kc);
    chk_cond("INCOMPAT RLC (Q " S_APROX " 628)", "V(C4) = 700 V: ningún valor de C4 lo da", s->estado == CAL_INCOMPAT);
    chk_t("INCOMPAT RLC (Q " S_APROX " 628)", "el máximo es el pico real, 628,3 V (no 381 V)",
          s->hi_dato, sqrt(0.01 + pow(2.0 * PI_T * 10.0, 2.0)) / 0.1, 628.3, 1e-6);
    s = r3m_rlc_vc(0.1, 600.0, &kc);
    chk_cond("INCOMPAT RLC (Q " S_APROX " 628)", "control: V(C4) = 600 V da dos soluciones", s->estado == CAL_OK && s->nsol == 2);

    /* ---- Hallazgo 4: el rango de un ángulo se desenrolla; el de un fp va
     *      en φ, con «pasando por 1» ---- */
    r3m_ini(50.0);
    r3m_fasor(EL_FUENTE, 0, 1, 1.0, 170.0);
    r3m_el(EL_RES, 1, 2, 100.0);
    kl = r3m_el(EL_BOB, 2, 0, -1.0);
    circ_mag(&r3m_c.el[kl], Q_V)->ang = 30.0; circ_mag(&r3m_c.el[kl], Q_V)->est_ang = M_DATO;
    s = r3m_calcula();
    m = r3m_mensaje(s);
    chk_cond("INCOMPAT " S_ANG "V(L3)", "E = 1" S_ANG "170°, R = 100, L3 [?!], " S_ANG "V(L3) = 30°: INCOMPAT", s->estado == CAL_INCOMPAT);
    chk_t("INCOMPAT " S_ANG "V(L3)", "el rango empieza en 170° (L3 " S_RARR " " S_INF ")", s->lo_dato, 170.0, 170.0, 1e-3);
    chk_t("INCOMPAT " S_ANG "V(L3)", "y acaba en 260° (L3 " S_RARR " 0): desenrollado, cruza ±180°", s->hi_dato, 260.0, 260.0, 1e-3);
    chk_cond("INCOMPAT " S_ANG "V(L3)", "el rango no contiene los 30° (ni 390°)",
             !(s->lo_dato <= 30.0 && 30.0 <= s->hi_dato) && !(s->lo_dato <= 390.0 && 390.0 <= s->hi_dato));
    /* el motor con la C en paralelo y fp(W2) = 0,3 inductivo, que no se alcanza */
    r3m_ini(50.0);
    r3m_fasor(EL_FUENTE, 0, 1, 230.0, 0.0);
    kw = r3m_el(EL_VATI, 1, 2, 0.0);
    kc = r3m_el(EL_COND, 2, 0, -1.0);
    r3m_el(EL_RES, 2, 3, 10.0);
    r3m_el(EL_BOB, 3, 0, 15.0 / w);
    r3m_c.el[kw].aux.v = 0.3; r3m_c.el[kw].aux.est = M_DATO;
    s = r3m_calcula();
    m = r3m_mensaje(s);
    chk_cond("INCOMPAT fp(W2)", "fp(W2) = 0,3 ind. con C3 [?!] en paralelo con el motor: INCOMPAT", s->estado == CAL_INCOMPAT);
    chk_t("INCOMPAT fp(W2)", "lo más inductivo: " S_PHI " = atan(15/10) (fp 0,555 ind.)", s->hi_dato, atan(1.5), 1.0, 1e-3);
    chk_cond("INCOMPAT fp(W2)", "lo más capacitivo: " S_PHI " cerca de -90° (fp casi 0 cap.)", s->lo_dato < -1.5 && s->lo_dato >= -0.5 * PI_T);
    chk_cond("INCOMPAT fp(W2)", "el " S_PHI " del dato (fp 0,3 ind.) queda fuera del rango", acos(0.3) > s->hi_dato);
    chk_cond("INCOMPAT fp(W2)", "el mensaje dice «0,555 ind.»", strstr(m, "0,555 ind.") != 0);
    chk_cond("INCOMPAT fp(W2)", "y «pasando por 1»", strstr(m, "pasando por 1") != 0);
    chk_cond("INCOMPAT fp(W2)", "el mensaje no da un fp con signo (de -0,985 a 0,986, que contiene el 0,3)", !strstr(m, "va de -"));
    r3m_c.el[kw].aux.v = -0.3;
    s = r3m_calcula();
    chk_cond("INCOMPAT fp(W2)", "control: fp(W2) = 0,3 cap. da una solución", s->estado == CAL_OK && s->nsol == 1);

    /* ---- Hallazgo 5: SIN_EFECTO solo dice «no lleva corriente» si es verdad ---- */
    r3m_ini(50.0);
    r3m_fasor(EL_FUENTE, 0, 1, 10.0, 0.0);
    kr = r3m_el(EL_RES, 1, 0, -1.0);
    r3m_el(EL_RES, 1, 2, 5.0);
    kl = r3m_el(EL_BOB, 2, 0, 0.01);
    circ_mag(&r3m_c.el[kl], Q_I)->v = 1.0; circ_mag(&r3m_c.el[kl], Q_I)->est = M_DATO;
    s = r3m_calcula();
    m = r3m_mensaje(s);
    chk_cond("SIN_EFECTO R2 " S_PAR " E1", "R2 [?!] sobre los bornes de E1: CAL_SIN_EFECTO de R2", s->estado == CAL_SIN_EFECTO && s->elem == kr);
    chk_cond("SIN_EFECTO R2 " S_PAR " E1", "R2 lleva E/R2: el mensaje no dice «no lleva corriente»", !strstr(m, "no lleva corriente"));
    chk_cond("SIN_EFECTO R2 " S_PAR " E1", "dice «R2 no influye en ningún dato»", strstr(m, "R2 no influye en ningún dato") != 0);
    chk_cond("SIN_EFECTO R2 " S_PAR " E1", "su |I| relativa (peor) pasa de 1e-9: sí lleva corriente", s->peor > 1e-9);
    r3m_ini(50.0);
    r3m_fasor(EL_CORR, 0, 1, 2.0, 0.0);
    kr = r3m_el(EL_RES, 1, 2, -1.0);
    i = r3m_el(EL_RES, 2, 0, 10.0);
    circ_mag(&r3m_c.el[i], Q_V)->v = 20.0; circ_mag(&r3m_c.el[i], Q_V)->est = M_DATO;
    s = r3m_calcula();
    m = r3m_mensaje(s);
    chk_cond("SIN_EFECTO R2 en serie con J1", "CAL_SIN_EFECTO de R2", s->estado == CAL_SIN_EFECTO && s->elem == kr);
    chk_cond("SIN_EFECTO R2 en serie con J1", "R2 lleva los 2 A de J1: dice «no influye en ningún dato»",
             strstr(m, "R2 no influye en ningún dato") != 0 && !strstr(m, "no lleva corriente"));
    r3m_ini(50.0);
    r3m_fasor(EL_FUENTE, 0, 1, 10.0, 0.0);
    r3m_el(EL_RES, 1, 0, 5.0);
    i = r3m_el(EL_VOLT, 1, 3, 0.0);
    kr = r3m_el(EL_RES, 3, 0, -1.0);
    circ_mag(&r3m_c.el[i], Q_V)->v = 10.0; circ_mag(&r3m_c.el[i], Q_V)->est = M_DATO;
    s = r3m_calcula();
    m = r3m_mensaje(s);
    chk_cond("SIN_EFECTO R4 con V3 ideal", "control: R4 en serie con un V ideal sí «no lleva corriente»",
             s->estado == CAL_SIN_EFECTO && s->elem == kr && strstr(m, "R4 no lleva corriente") != 0 && s->peor == 0.0);

    /* ---- A mano en calca.c: el LC serie con L y C buscadas da sus DOS
     *      soluciones (L = 31,83 mH o 95,49 mH, C = 159,15 µF), con R = 0
     *      y con R = 0,001. (b) La semilla de la C va un factor 1,7 al lado
     *      de la resonancia con la de la L: sin eso, con R = 0,001 se pierde
     *      una. (a) Un arranque singular se reintenta un pelo al lado: una Z
     *      capacitiva (|Z| buscada a -90°) no lleva el factor 1,7, sus
     *      semillas resuenan con las de la L (con R = 0, la matriz es
     *      singular justo ahí) y sin el reintento se pierde L = 95,49 mH.
     *      La R de 1 Ω en paralelo con la E fija Z0 (y las semillas) ---- */
    for (i = 0; i < 4; i++) {
        const double R = i & 1 ? 0.001 : 0.0;
        const int z = i >= 2;
        char q[64];
        int il, ic;
        s = r3m_lc_serie(R, z, z ? 1.0 : 0.0, &kl, &kc);
        il = r3m_inc_de(s, kl); ic = r3m_inc_de(s, kc);
        sprintf(q, "LC serie, L y %s [?!], R = %s", z ? "|Z| (a -90" S_DEG ", 1 " S_OHM " " S_PAR " E)" : "C", i & 1 ? "0,001" : "0");
        chk_cond(q, "E = 10 V a 50 Hz, I = 1 A y 20 V en la C (o la Z): dos soluciones", s->estado == CAL_OK && s->nsol == 2 && il >= 0 && ic >= 0);
        if (s->nsol == 2 && il >= 0 && ic >= 0) {
            double l0 = s->sol[0].x[il], l1 = s->sol[1].x[il];
            if (l0 > l1) { double t = l0; l0 = l1; l1 = t; }
            chk_t(q, "L = 10/" S_OMEGA " = 31,83 mH", l0, 10.0 / w, 10.0 / w, 1e-6);
            chk_t(q, "L = 30/" S_OMEGA " = 95,49 mH", l1, 30.0 / w, 30.0 / w, 1e-6);
            if (z) {
                chk_t(q, "|Z| = 20 " S_OHM " (una)", s->sol[0].x[ic], 20.0, 20.0, 1e-6);
                chk_t(q, "|Z| = 20 " S_OHM " (otra)", s->sol[1].x[ic], 20.0, 20.0, 1e-6);
            } else {
                chk_t(q, "C = 1/(20" S_OMEGA ") = 159,15 " S_MU "F (una)", s->sol[0].x[ic], 1.0 / (20.0 * w), 1.0 / (20.0 * w), 1e-6);
                chk_t(q, "C = 1/(20" S_OMEGA ") = 159,15 " S_MU "F (otra)", s->sol[1].x[ic], 1.0 / (20.0 * w), 1.0 / (20.0 * w), 1e-6);
            }
        } else { pruebas += 4; falla(q, "sin las dos soluciones no se comparan L y C", (double)s->nsol, 2.0); }
    }
}

/* ---- El Bode ---- */
static void r3m_espera(void) { int k; for (k = 0; k < 4000 && ui_pantalla() == P_EMULANDO; k++) ui_tick(); }

/* Del editor a la lista de modelos de la lección 11 por teclas (sin ui_init:
 * lo que el editor dejó en la sesión sigue ahí) */
static int r3m_a_leccion11(void)
{
    int k;
    for (k = 0; k < 6 && ui_pantalla() != P_MENU; k++) TECLAS(T_ESC);
    if (ui_pantalla() != P_MENU) return 0;
    TECLAS(T_ABAJO, T_ENTER);
    repite(T_ABAJO, 10);
    TECLAS(T_ENTER, T_DER, T_DER);
    return ui_pantalla() == P_EJEMPLOS;
}

/* E (continua, 10 V) hacia abajo, cable, C = 1 uF hacia arriba y el cuarto
 * elemento (tipo t, por teclas: su letra y el valor) de vuelta a la
 * izquierda, como dibuja_rc */
static void r3m_dibuja_4(int letra, const int *val, int nval)
{
    int i;
    ui_init();
    TECLAS(T_ENTER, T_ENTER, T_ENTER);
    TECLAS(T_LETRA('E'), T_ABAJO, T_DIG(1), T_DIG(0), T_ENTER);
    TECLAS(T_LETRA('W'), T_DER, T_ESC);
    TECLAS(T_LETRA('C'), T_ARRIBA, T_DIG(1), T_LETRA('U'), T_ENTER);
    i = T_LETRA0 + (letra - 'A');
    teclas(&i, 1);
    TECLAS(T_IZQ);
    for (i = 0; i < nval; i++) teclas(&val[i], 1);
    TECLAS(T_ENTER);
}

/* Las 175 redes planas de la revisión: E ideal || C || (R + L) con salida
 * V(C1), y divisores con una C o una L en paralelo con la E. Cuántas dan
 * «Plano» con un pico o un valle (de redondeo) */
static int r3m_planos(int *total, int *planos)
{
    static const double CV[] = { 1e-7, 1e-6, 1e-5, 1e-4, 1e-3 };
    static const double LV[] = { 1e-3, 10e-3, 50e-3, 0.1, 1.0 };
    static const double RV[] = { 1.0, 10.0, 100.0, 1e3, 1e4 };
    int i, j, k, kE, kS, malos = 0, fam;
    *total = *planos = 0;
    for (fam = 0; fam < 3; fam++)
        for (i = 0; i < 5; i++) for (j = 0; j < 5; j++) for (k = 0; k < 5; k++) {
            if (fam > 0 && j > 0) continue;
            sr_init(3);
            kE = sr_el(SE_E, 0, 1, 0.0); sr.el[kE].forma = ONDA_CC; sr.el[kE].p[0] = 1.0;
            if (fam == 0) {
                kS = sr_el(SE_C, 1, 0, CV[i]);
                sr_el(SE_R, 1, 2, RV[k]);
                sr_el(SE_L, 2, 0, LV[j]);
            } else {
                if (fam == 1) sr_el(SE_C, 1, 0, CV[i]); else sr_el(SE_L, 1, 0, LV[i]);
                sr_el(SE_R, 1, 2, RV[k]);
                kS = sr_el(SE_R, 2, 0, (fam == 1 ? 2.0 : 3.0) * RV[k]);
            }
            canal(0, CANAL_V, kS);
            if (bode_sr(kE, 0, -1, -1, 0.0, 0.0, 60.0) != SIM_OK) continue;
            (*total)++;
            if (bode_res()->m.tipo != BODE_PLANO) continue;
            (*planos)++;
            if (bode_res()->m.f_pico > 0.0 || bode_res()->m.f_valle > 0.0) malos++;
        }
    return malos;
}

static void r3m_bode(void)
{
    const Circuito *cu = ui_circuito();
    const BodeMedidas *m = &bode_res()->m;
    const double L = 10e-3, C = 1e-6, f0 = 1.0 / (2.0 * PI_T * sqrt(L * C));
    int kE, kC, k, total, planos, malos;

    /* ---- Hallazgo 6: [tab] en un Bode abierto desde P_CA no deja un bucle
     *      de [esc] entre P_CA y P_BODE ---- */
    dibuja_t1();
    TECLAS(T_IGUAL);
    chk_cond("Bode desde P_CA", "T1 y [=]: P_CA", ui_pantalla() == P_CA);
    TECLAS(T_LETRA('B'));
    r3m_espera();
    chk_cond("Bode desde P_CA", "[B] en P_CA: P_BODE", ui_pantalla() == P_BODE);
    TECLAS(T_TAB);
    chk_cond("Bode desde P_CA", "[tab]: los fasores en P_CA", ui_pantalla() == P_CA);
    TECLAS(T_ESC);
    chk_cond("Bode desde P_CA", "[esc] en P_CA vuelve al Bode", ui_pantalla() == P_BODE);
    TECLAS(T_ESC);
    chk_cond("Bode desde P_CA", "[esc] en el Bode vuelve al editor (no a P_CA otra vez)", ui_pantalla() == P_EDITOR);
    /* lo mismo con un modelo de la lección 11 que abre P_CA */
    ui_init();
    TECLAS(T_ENTER, T_ABAJO, T_ENTER);
    repite(T_ABAJO, 10);
    TECLAS(T_ENTER, T_DER, T_DER);
    repite(T_ABAJO, 5);                                         /* RLC serie en fasores */
    TECLAS(T_ENTER);
    r3m_espera();
    chk_cond("Bode desde un modelo", "«RLC serie en fasores» abre P_CA", ui_pantalla() == P_CA);
    TECLAS(T_LETRA('B'));
    r3m_espera();
    TECLAS(T_TAB);
    chk_cond("Bode desde un modelo", "[B] y [tab]: P_CA", ui_pantalla() == P_CA);
    TECLAS(T_ESC, T_ESC);
    chk_cond("Bode desde un modelo", "[esc] [esc]: de vuelta a la lista de modelos", ui_pantalla() == P_EJEMPLOS);

    /* ---- Hallazgo 7: el Bode de un modelo no hereda el rango del editor ---- */
    dibuja_rc();
    TECLAS(T_LETRA('B'));
    r3m_espera();
    TECLAS(T_MENU);
    chk_cond("Bode de un modelo", "[menu] en el Bode del editor: el rango", ui_pantalla() == P_AJUSTES && ve("Bode: rango de frecuencias"));
    TECLAS(T_DIG(1), T_ABAJO, T_DIG(1), T_DIG(0), T_IGUAL);     /* Desde 1, Hasta 10, [=] barre */
    r3m_espera();
    chk_cond("Bode de un modelo", "el editor barre de 1 a 10 Hz (Desde 1, Hasta 10)", ui_pantalla() == P_BODE &&
             bode_res()->f_ini == 1.0 && bode_res()->f_fin == 10.0);
    chk_cond("Bode de un modelo", "del Bode del editor a la lista de la lección 11", r3m_a_leccion11());
    repite(T_ABAJO, 2);                                         /* Pasa-banda RLC serie */
    TECLAS(T_ENTER);
    r3m_espera();
    vuelca("r3m_bode_modelo");
    chk_cond("Bode de un modelo", "«Pasa-banda RLC serie»: P_BODE", ui_pantalla() == P_BODE);
    chk_cond("Bode de un modelo", "con su rango automático (100 Hz a 10 kHz), no el de 1 a 10 Hz",
             bode_res()->f_ini == 100.0 && bode_res()->f_fin == 1e4);
    chk_cond("Bode de un modelo", "pasa-banda con Q = 10, como anuncia la lección", m->tipo == BODE_PBANDA && fabs(m->q - 10.0) < 1e-6 && ve("Q 10,00"));

    /* ---- Hallazgo 8: la salida nula (BODE_NULA) ---- */
    sr_init(3);
    kE = sr_el(SE_E, 0, 1, 0.0); sr.el[kE].forma = ONDA_CC; sr.el[kE].p[0] = 1.0;
    sr_el(SE_R, 1, 2, 1e3);
    sr_el(SE_C, 2, 0, 1e-6);
    canal(0, CANAL_NODO, 0);
    chk_cond("Bode salida nula", "la sonda en la referencia: se barre", bode_sr(kE, 0, -1, -1, 0.0, 0.0, 0.0) == SIM_OK);
    chk_cond("Bode salida nula", "H " S_APROX " 0: BODE_NULA (no pasa-bajas)", m->tipo == BODE_NULA);
    sr_init(4);
    kE = sr_el(SE_E, 0, 1, 0.0); sr.el[kE].forma = ONDA_CC; sr.el[kE].p[0] = 1.0;
    sr_el(SE_R, 1, 2, 1e3); sr_el(SE_C, 2, 0, 1e-6);
    sr_el(SE_R, 1, 3, 3e3); sr_el(SE_C, 3, 0, 1e-6 / 3.0);
    chk_cond("Bode salida nula", "puente RC equilibrado, V(AB): se barre", bode_sr(kE, -1, 2, 3, 0.0, 0.0, 0.0) == SIM_OK);
    chk_cond("Bode salida nula", "puente RC equilibrado: BODE_NULA, sin pico, valle, cortes ni Q inventados",
             m->tipo == BODE_NULA && m->f_pico == 0.0 && m->f_valle == 0.0 && m->fc == 0.0 && m->q == 0.0 && m->f1 == 0.0 && m->f2 == 0.0);
    sr_init(4);
    kE = sr_el(SE_E, 0, 1, 0.0); sr.el[kE].forma = ONDA_CC; sr.el[kE].p[0] = 1.0;
    sr_el(SE_L, 1, 2, 10e-3); sr_el(SE_R, 2, 0, 1e3);
    sr_el(SE_L, 1, 3, 30e-3); sr_el(SE_R, 3, 0, 3e3);
    bode_sr(kE, -1, 2, 3, 0.0, 0.0, 0.0);
    chk_cond("Bode salida nula", "puente RL equilibrado: BODE_NULA (no pasa-altas con fc = 3437 Hz)", m->tipo == BODE_NULA && m->fc == 0.0);
    /* por la interfaz: la sonda de tensión en el borne - de E1 (la referencia) */
    dibuja_rc();
    k = el_de_etiqueta(cu, "E1");
    TECLAS(T_LETRA('O'), T_ENTER);
    chk_cond("Bode salida nula", "[O][enter] en el borne - de E1: la sonda del punto", cu->sonda[0].tipo == SONDA_NODO && k >= 0 &&
             cu->sonda[0].x == cu->el[k].xa && cu->sonda[0].y == cu->el[k].ya);
    TECLAS(T_LETRA('B'));
    r3m_espera();
    vuelca("r3m_bode_nula");
    chk_cond("Bode salida nula", "P_BODE dice «Salida nula», no «Pasa-bajas»", ui_pantalla() == P_BODE && ve("Salida nula") && !ve("Pasa-bajas"));

    /* ---- Hallazgo 9: el LC ideal es un pasa-banda con un pico finito y enorme ---- */
    sr_init(2);
    kE = sr_el(SE_J, 0, 1, 0.0); sr.el[kE].forma = ONDA_CC; sr.el[kE].p[0] = 1.0;
    sr_el(SE_L, 1, 0, L);
    sr_el(SE_C, 1, 0, C);
    canal(0, CANAL_NODO, 1);
    bode_sr(kE, 0, -1, -1, 0.0, 0.0, 0.0);
    chk_cond("Bode LC ideal", "tanque J " S_PAR " L " S_PAR " C: pasa-banda (no «Otra forma»)", m->tipo == BODE_PBANDA);
    chk_t("Bode LC ideal", "tanque: el pico en f0 = 1591,549 Hz", m->f_pico, f0, f0, 1e-6);
    chk_cond("Bode LC ideal", "tanque: el pico es finito y enorme (> 100 dB), no -300 dB", m->db_pico > 100.0 && m->db_pico < 1000.0);
    sr_init(3);
    kE = sr_el(SE_E, 0, 1, 0.0); sr.el[kE].forma = ONDA_CC; sr.el[kE].p[0] = 1.0;
    sr_el(SE_L, 1, 2, L);
    kC = sr_el(SE_C, 2, 0, C);
    canal(0, CANAL_V, kC);
    bode_sr(kE, 0, -1, -1, 0.0, 0.0, 0.0);
    chk_cond("Bode LC ideal", "LC serie, V(C1): pasa-banda con el pico en f0, finito y enorme", m->tipo == BODE_PBANDA &&
             fabs(m->f_pico - f0) <= 1e-6 * f0 && m->db_pico > 100.0 && m->db_pico < 1000.0);
    canal(0, CANAL_V, 1);                                        /* la salida V(L1) */
    bode_sr(kE, 0, -1, -1, 0.0, 0.0, 0.0);
    chk_cond("Bode LC ideal", "LC serie, V(L1): pasa-banda, pico finito (no -300 dB)", m->tipo == BODE_PBANDA && m->db_pico > 100.0 && m->db_pico < 1000.0);
    /* por la interfaz: E - C - L (la R de dibuja_rc cambiada por L = 10 mH) */
    {
        static const int VAL_L[] = { T_DIG(1), T_DIG(0), T_LETRA('M') };
        r3m_dibuja_4('L', VAL_L, 3);
    }
    chk_cond("Bode LC ideal", "E1, C1 y L1 = 10 mH dibujados", cu->nel == 3 && el_de_etiqueta(cu, "L1") >= 0);
    TECLAS(T_LETRA('B'));
    r3m_espera();
    vuelca("r3m_bode_lc_ideal");
    chk_cond("Bode LC ideal", "P_BODE: «Pasa-banda», no «Otra forma»", ui_pantalla() == P_BODE && ve("Pasa-banda") && !ve("Otra forma"));

    /* ---- Hallazgo 10: los avisos OTRAS y Z_ESCALA se ven en el trazador ---- */
    {
        static const int VAL_5[] = { T_DIG(5) };
        r3m_dibuja_4('E', VAL_5, 1);
    }
    chk_cond("Bode avisos", "E1, C1 y E2 dibujados", cu->nel == 3 && el_de_etiqueta(cu, "E2") >= 0);
    TECLAS(T_LETRA('B'));
    r3m_espera();
    vuelca("r3m_bode_otras");
    chk_cond("Bode avisos", "dos fuentes: el barrido lleva SIM_AV_OTRAS", ui_pantalla() == P_BODE && (bode_res()->d.avisos & SIM_AV_OTRAS));
    chk_cond("Bode avisos", "el aviso de las demás fuentes está en el trazador", strstr(ui_trazador()->aviso, "fuentes se apagan") != 0);
    chk_cond("Bode avisos", "y se ve en P_BODE", ve("Las dem" S_aa "s fuentes se apagan (entrada: E1)."));
    TECLAS(T_LETRA('F'));
    r3m_espera();
    chk_cond("Bode avisos", "[F]: la entrada E2, y el aviso sigue (con E2)", ui_pantalla() == P_BODE && ve("(entrada: E2)."));
    /* una Z: a otra f se toma como R y L (o C) */
    ui_init();
    TECLAS(T_ENTER, T_ENTER, T_ENTER);
    TECLAS(T_LETRA('E'), T_ABAJO, T_IZQ, T_DIG(1), T_DIG(2), T_DIG(0), T_ENTER);
    TECLAS(T_LETRA('W'), T_DER, T_ESC);
    TECLAS(T_LETRA('C'), T_ARRIBA, T_DIG(1), T_DIG(0), T_DIG(0), T_LETRA('U'), T_ENTER);
    TECLAS(T_LETRA('Z'), T_IZQ, T_DIG(3), T_DIG(0), T_MAS, T_DIG(4), T_DIG(0), T_ENTER);
    chk_cond("Bode avisos", "E1 alterna, C1 y Z1 = 30+j40 dibujados", cu->nel == 3 && el_de_etiqueta(cu, "Z1") >= 0 && circ_alterna(cu));
    TECLAS(T_LETRA('B'));
    r3m_espera();
    vuelca("r3m_bode_z");
    chk_cond("Bode avisos", "con una Z: el barrido lleva SIM_AV_Z_ESCALA", ui_pantalla() == P_BODE && (bode_res()->d.avisos & SIM_AV_Z_ESCALA));
    chk_cond("Bode avisos", "y P_BODE avisa de que la Z es de la f del circuito", ve("La Z es de la f del circuito: a otra, R y L (o C)."));

    /* ---- Hallazgo 11: una curva plana no tiene pico ni valle de redondeo ---- */
    malos = r3m_planos(&total, &planos);
    chk_cond("Bode plano", "las 175 redes de la revisión se barren", total == 175);
    chk_cond("Bode plano", "muchas salen «Plano»", planos >= 100);
    chk_cond("Bode plano", "ninguna plana con un pico o un valle de redondeo", malos == 0);
    sr_init(3);
    kE = sr_el(SE_E, 0, 1, 0.0); sr.el[kE].forma = ONDA_CC; sr.el[kE].p[0] = 1.0;
    kC = sr_el(SE_C, 1, 0, 1e-4);
    sr_el(SE_R, 1, 2, 10.0);
    sr_el(SE_L, 2, 0, 0.05);
    canal(0, CANAL_V, kC);
    bode_sr(kE, 0, -1, -1, 0.0, 0.0, 60.0);
    chk_cond("Bode plano", "E " S_PAR " C 100 " S_MU "F " S_PAR " (10 " S_OHM " + 50 mH), V(C1): «Plano» sin valle en 2902 Hz",
             m->tipo == BODE_PLANO && m->f_valle == 0.0 && m->f_pico == 0.0);
    /* control: un pico de verdad sí se mide (RLC serie con Q = 1, V(C1)) */
    sr_init(4);
    kE = sr_el(SE_E, 0, 1, 0.0); sr.el[kE].forma = ONDA_CC; sr.el[kE].p[0] = 1.0;
    sr_el(SE_R, 1, 2, sqrt(L / C));
    sr_el(SE_L, 2, 3, L);
    kC = sr_el(SE_C, 3, 0, C);
    canal(0, CANAL_V, kC);
    bode_sr(kE, 0, -1, -1, 0.0, 0.0, 0.0);
    chk_t("Bode plano", "control: RLC serie Q = 1, V(C1): el pico en f0" S_CDOT S_SQRT "(1 - 1/2Q" S_SUP2 ")", m->f_pico, f0 * sqrt(0.5), f0, 1e-6);

    /* ---- Hallazgo 12: [C] no repite la misma salida ---- */
    dibuja_rc();
    TECLAS(T_LETRA('T'), T_DER, T_LETRA('T'));                   /* A y B en los bornes de R1 */
    TECLAS(T_LETRA('O'), T_ABAJO, T_LETRA('O'), T_ABAJO);         /* la sonda 1: V(C1) */
    k = el_de_etiqueta(cu, "C1");
    chk_cond("Bode [C]", "la sonda 1 en V(C1) y A y B puestos", cu->sonda[0].tipo == SONDA_V && cu->sonda[0].el == k &&
             cu->term_x[0] >= 0 && cu->term_x[1] >= 0);
    TECLAS(T_LETRA('B'));
    r3m_espera();
    chk_cond("Bode [C]", "[B]: la salida es la sonda 1, V(C1)", ui_pantalla() == P_BODE && strstr(bode_res()->nom_sal, "C1") != 0);
    TECLAS(T_LETRA('C'));
    r3m_espera();
    chk_cond("Bode [C]", "el primer [C] pasa a V(AB) (no vuelve a barrer V(C1))", strcmp(bode_res()->nom_sal, "V(AB)") == 0);
    TECLAS(T_LETRA('C'));
    r3m_espera();
    chk_cond("Bode [C]", "el segundo [C] vuelve a V(C1)", strstr(bode_res()->nom_sal, "C1") != 0);
    TECLAS(T_LETRA('C'));
    r3m_espera();
    chk_cond("Bode [C]", "y el tercero a V(AB): V(C1) nunca sale dos veces seguidas", strcmp(bode_res()->nom_sal, "V(AB)") == 0);
    TECLAS(T_ESC);

    chk_cond("revisión E3 (Bode)", "sin textos fuera de la pantalla", gfx_desbordes() == 0);
}

/* Lo que quedó tras la primera tanda: la Z capacitiva con L buscada, la
 * raíz doble y SIN_EFECTO con una R grande en paralelo */
static void r3m_despues(void)
{
    const CalRes *s;
    const double w = 2.0 * PI_T * 50.0;
    int kl, kc, i, j, ir, kr, k3, m;
    char q[80];
    /* L y |Z| (a -90°) buscadas, con R = 0,001 y R = 0,1: las dos soluciones */
    for (ir = 0; ir < 2; ir++) {
        double R = ir ? 0.1 : 0.001, r = sqrt(100.0 - R * R);
        s = r3m_lc_serie(R, 1, 0.0, &kl, &kc);
        sprintf(q, "L y Z(-90°) buscadas, R = %g", R);
        chk_cond(q, "dos soluciones (25 arranques con 2 incógnitas)", s->estado == CAL_OK && s->nsol == 2);
        for (i = 0; i < s->nsol && i < 2; i++) {
            int il = r3m_inc_de(s, kl);
            double xl = il >= 0 ? w * s->sol[i].x[il] : 0.0;
            chk_cond(q, "X_L = 20 +- raíz((E/I)^2 - R^2)", fabs(xl - (20.0 - r)) < 1e-6 * 20.0 || fabs(xl - (20.0 + r)) < 1e-6 * 20.0);
        }
    }
    /* La raíz doble: E/I = R justo (R = 10, I = 1 A, V_C = 1 V): una sola
     * solución, X_L = X_C = 1 Ω, aunque los arranques caigan en puntos
     * distintos de su valle plano */
    for (j = 0; j < 2; j++) {
        int ka;
        r3m_ini(50.0);
        r3m_fasor(EL_FUENTE, 0, 1, 10.0, 0.0);
        ka = r3m_el(EL_AMP, 1, 2, 0.0);
        r3m_el(EL_RES, 2, 3, 10.0);
        kl = r3m_el(EL_BOB, 3, 4, -1.0);
        kc = r3m_el(j ? EL_IMP : EL_COND, 4, 0, -1.0);
        if (j) { r3m_c.el[kc].val.ang = -90.0; r3m_c.el[kc].val.est_ang = M_DATO; }
        circ_mag(&r3m_c.el[ka], Q_I)->v = 1.0; circ_mag(&r3m_c.el[ka], Q_I)->est = M_DATO;
        circ_mag(&r3m_c.el[kc], Q_V)->v = 1.0; circ_mag(&r3m_c.el[kc], Q_V)->est = M_DATO;
        s = r3m_calcula();
        sprintf(q, "raíz doble (E/I = R) con %s", j ? "Z(-90°)" : "C");
        chk_cond(q, "una sola solución, no varias casi iguales", s->estado == CAL_OK && s->nsol == 1);
        m = r3m_inc_de(s, kl);
        if (s->nsol >= 1 && m >= 0) chk_t(q, "X_L = X_C = 1 ohm (a 1e-5: el valle es plano)", w * s->sol[0].x[m], 1.0, 1.0, 1e-5);
    }
    /* SIN_EFECTO: R2 [?!] sobre E1 lleva corriente con cualquier valor,
     * aunque otra R de 10 MΩ en paralelo suba la escala (con R3 de 1 mΩ
     * y una L de 10 µH, el dato |I(R3)| = 1 A) */
    r3m_ini(50.0);
    r3m_fasor(EL_FUENTE, 0, 1, 10.0, 0.0);
    kr = r3m_el(EL_RES, 1, 0, -1.0);
    k3 = r3m_el(EL_RES, 1, 2, 1e-3);
    r3m_el(EL_BOB, 2, 0, 1e-5);
    r3m_el(EL_RES, 1, 0, 1e7);
    circ_mag(&r3m_c.el[k3], Q_I)->v = 1.0; circ_mag(&r3m_c.el[k3], Q_I)->est = M_DATO;
    s = r3m_calcula();
    chk_cond("SIN_EFECTO con R de 10 Mohm", "CAL_SIN_EFECTO de R2", s->estado == CAL_SIN_EFECTO && s->elem == kr);
    r3m_mensaje(s);
    chk_cond("SIN_EFECTO con R de 10 Mohm", "no dice «no lleva corriente» (sí la lleva)", strstr(r3m_msg, "no lleva corriente") == 0 &&
             strstr(r3m_msg, "no influye") != 0);
}

static void test_revision_e3_motor(void)
{
    printf("[54a] Revisión de la Etapa 3: motor, calculadora y Bode\n");
    gfx_desbordes_reset();
    r3m_motor();
    r3m_calculadora();
    r3m_bode();
    r3m_despues();
}
