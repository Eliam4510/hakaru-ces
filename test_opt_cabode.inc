/* ============================================================================
 *  test_opt_cabode.inc  -  [75c] El Bode y la CA optimizados (rev9, grupo
 *  «cabode»): los mismos bits con menos trabajo, y cada caché se olvida en
 *  cuanto cambia algo de lo que depende. Cada prueba falla con la mutación
 *  que dice su comentario (comprobado).
 *
 *    K1  las medidas del Bode: por reg_ (con el |H| que ya sacó la malla) y
 *        por una copia (sin él) salen iguales bit a bit; en cada cruce, |H|
 *        por √2 es la referencia de ESE Bode (ln(ref/√2), una vez por
 *        referencia; cada g de los cruces, una vez)
 *    K2  el rango automático: potencias de 10 con los bits de pow (las del
 *        barrido grueso, que se hace una vez), también en los topes
 *    K3  la topología, una vez: bode_en con otro análisis de CA en medio da
 *        los mismos bits, y el Bode por trozos con otro análisis entre trozo
 *        y trozo, el mismo registro y las mismas medidas
 *    K4  la red fija (la base de la matriz, sin las filas de C y L): H por el
 *        Bode = H con la matriz entera (ca_ensambla), bit a bit, en redes al
 *        azar (R, C, L, E, J, E real, instrumentos, controladas, Z y trozos
 *        que flotan), con no lineales en Q y con resonancias ideales, y con
 *        las mismas operaciones contadas; se rehace si cambia la topología,
 *        la red o la linealización
 *    K5  1/a sin lo que sobra de 1 + 0j (cx_inv) = cx_div(1, a) bit a bit
 *    K6  la fase desenrollada sin la división: la de siempre, bit a bit
 *
 *  Se incluye tras test_r8_pantallas.inc (usa sr, saj, sr_init, sr_el,
 *  canal, bode_sr, red_rlc_seno, u_ca, a4_cc, a4_ao, q4_ec, t4_seno,
 *  p4_diodo y los chk). Va al final de la batería: no vuelca BMP ni gasta
 *  azar(), y deja azar_ca como estaba.
 * ========================================================================= */

/* H(f) sin el Bode, con la matriz entera: la red de bode_iniciar_red (el
 * punto Q, la entrada a 1∠0 y las demás apagadas), ca_topologia y
 * ca_ensambla. La salida, el canal ch (un nodo, o la V o la I de un
 * elemento). 1, 0 si es singular, -1 si la red no vale */
static int oc_h_plano(const SimRed *s0, int ent, const SimCanal *ch, double f, Cx *H)
{
    static SimRed s;
    static SimLin L;
    static Cx b[LUC_MAX], x[LUC_MAX];
    SimDiag d;
    CaExc ex;
    int ok;
    s = *s0;
    memset(&d, 0, sizeof d);
    d.elem = d.nodo = d.elem_aviso = -1;
    *H = cx(0.0, 0.0);
    if (ca_punto_q(&s, &L, &d) != SIM_OK) return -1;
    memset(&ex, 0, sizeof ex);
    ex.U[ent] = cx(1.0, 0.0);
    ex.activa = 1u << ent;
    ca_apaga(&s, ex.activa);
    if (ca_topologia(&s, &d) != SIM_OK) return -1;
    ca_pon_q(L.hay ? &L : 0);
    ok = ca_ensambla(&s, f, &d) == SIM_OK;
    if (ok) {
        ca_b_fuentes(&s, &ex, b);
        ca_sustituye(b, x);
        if (ch->tipo == CANAL_NODO) *H = ca_phi(x, ch->nodo);
        else if (ch->tipo == CANAL_V)
            *H = cx_esc(cx_menos(ca_phi(x, s.el[ch->elem].a), ca_phi(x, s.el[ch->elem].b)), (double)ch->sg);
        else {
            Cx V, I;
            ca_vi(&s, &ex, x, f, ch->elem, &V, &I);
            *H = cx_esc(I, (double)ch->sg);
        }
    }
    ca_pon_q(0);
    return ok;
}

/* ¿Los mismos bits (y el mismo ok) por bode_en que con la matriz entera, en
 * nf frecuencias? Primero todas por bode_en (la base se reutiliza), luego
 * las de oc_h_plano (cada una rehace la topología) */
static int oc_igual_plano(int ent, const SimCanal *ch, const double *F, int nf)
{
    Cx H1[8], H2[8];
    int i, ok1[8], ok2[8], r = 1;
    for (i = 0; i < nf; i++) ok1[i] = bode_en(F[i], &H1[i]);
    for (i = 0; i < nf; i++) ok2[i] = oc_h_plano(&sr, ent, ch, F[i], &H2[i]);
    for (i = 0; i < nf; i++)
        if (ok1[i] != ok2[i] || memcmp(&H1[i], &H2[i], sizeof H1[i]) != 0) r = 0;
    return r;
}

/* ---------------------------------------------------------------------------
 *  K1. Las medidas
 * ------------------------------------------------------------------------ */
static BodeReg oc_copia;

/* bode_medidas por reg_ y por una copia: los mismos bits, y los de la
 * barrida (res_.m) si acabó.
 * Mutación: el |H| de la malla (mod_) mal guardado, o de otro punto */
static int oc_medidas_igual(void)
{
    BodeMedidas m1, m2;
    oc_copia = *bode_reg();
    bode_medidas(bode_reg(), bode_en, &m1);
    bode_medidas(&oc_copia, bode_en, &m2);
    if (memcmp(&m1, &m2, sizeof m1) != 0) return 0;
    return bode_res()->fin != FIN_COMPLETO || memcmp(&m1, &bode_res()->m, sizeof m1) == 0;
}

/* |H(c)|·√2/ref - 1 en un cruce c (0 si no lo hay) */
static double oc_cruce(double c, double ref)
{
    Cx H;
    if (!(c > 0.0) || !bode_en(c, &H)) return 0.0;
    return cx_abs(H) * 1.41421356237309504880 / ref - 1.0;
}

/* En cada cruce del Bode que acaba de hacerse, |H| = ref/√2 con SU ref.
 * Mutación: pon_ref que falta en una rama (la ln(ref/√2) del Bode de antes),
 * o la gb de los cruces que no se arrastra */
static int oc_cruces_bien(int tipo)
{
    const BodeMedidas *m = &bode_res()->m;
    Cx H;
    double ref, e = 0.0;
    if (m->tipo != tipo) return 0;
    switch (tipo) {
    case BODE_PB: e = oc_cruce(m->fc, m->M0); break;
    case BODE_PA: e = oc_cruce(m->fc, m->Minf); break;
    case BODE_PBANDA:
        if (!bode_en(m->f_pico, &H)) return 0;
        ref = cx_abs(H);
        e = fabs(oc_cruce(m->f1, ref)) + fabs(oc_cruce(m->f2, ref));
        if (!(m->f1 > 0.0 && m->f2 > m->f1)) return 0;
        break;
    case BODE_RB:
        ref = sqrt(m->M0 * m->Minf);
        e = fabs(oc_cruce(m->f1, ref)) + fabs(oc_cruce(m->f2, ref));
        if (!(m->f1 > 0.0 && m->f2 > m->f1)) return 0;
        break;
    default:
        return 1;
    }
    return (tipo == BODE_PB || tipo == BODE_PA ? m->fc > 0.0 : 1) && fabs(e) <= 1e-9;
}

static void oc_k1(void)
{
    static const char *const NOM[4] = { "RLC pasa-banda (Mp = 1)", "RC con divisor (M0 = 1/2)",
                                        "RL con divisor (Minf = 1/4)", "doble T (rechaza-banda)" };
    int caso, kE, kS, i;
    char q[64];

    /* uno tras otro, cada uno con su referencia: la del anterior no sirve */
    for (caso = 0; caso < 4; caso++) {
        int tipo;
        sprintf(q, "K1 %s", NOM[caso]);
        switch (caso) {
        case 0:                                                  /* E - L - C - R: V(R), 0 dB en f0 */
            sr_init(4);
            kE = a4_cc(0, 1, 1.0);
            sr_el(SE_L, 1, 2, 10e-3); sr_el(SE_C, 2, 3, 1e-6);
            kS = sr_el(SE_R, 3, 0, 10.0);
            tipo = BODE_PBANDA;
            break;
        case 1:                                                  /* E - R - (R ‖ C): V(C), M0 = 1/2 */
            sr_init(3);
            kE = a4_cc(0, 1, 1.0);
            sr_el(SE_R, 1, 2, 1e3); sr_el(SE_R, 2, 0, 1e3);
            kS = sr_el(SE_C, 2, 0, 1e-6);
            tipo = BODE_PB;
            break;
        case 2:                                                  /* E - R 3k - (R 1k ‖ L): V(L), Minf = 1/4 */
            sr_init(3);
            kE = a4_cc(0, 1, 1.0);
            sr_el(SE_R, 1, 2, 3e3); sr_el(SE_R, 2, 0, 1e3);
            kS = sr_el(SE_L, 2, 0, 0.1);
            tipo = BODE_PA;
            break;
        default:                                                 /* doble T de 1 k y 1 uF, con carga */
            sr_init(5);
            kE = a4_cc(0, 1, 1.0);
            sr_el(SE_R, 1, 2, 1e3); sr_el(SE_R, 2, 3, 1e3); sr_el(SE_C, 2, 0, 2e-6);
            sr_el(SE_C, 1, 4, 1e-6); sr_el(SE_C, 4, 3, 1e-6); sr_el(SE_R, 4, 0, 500.0);
            kS = sr_el(SE_R, 3, 0, 1e6);
            tipo = BODE_RB;
            break;
        }
        canal(0, CANAL_V, kS);
        chk_cond(q, "se barre", bode_sr(kE, 0, -1, -1, 0.0, 0.0, 0.0) == SIM_OK && bode_res()->fin == FIN_COMPLETO);
        chk_cond(q, "cada cruce, en la ref/" S_SQRT "2 de este Bode", oc_cruces_bien(tipo));
        chk_cond(q, "medidas: por reg_ = por una copia, bit a bit", oc_medidas_igual());
    }

    /* a medias (sin cortar) y cortado: lo barrido, igual por los dos caminos */
    sprintf(q, "K1 a medias");
    bode_iniciar_red(&sr, kE, &saj.canal[0], -1, -1, 10.0, 1e5, 0.0);
    for (i = 0; i < 40; i++) bode_avanzar(1ul);
    chk_cond(q, "medidas: por reg_ = por una copia", bode_reg()->n > 2 && oc_medidas_igual());
    bode_cortar();
    chk_cond(q, "y cortado", bode_res()->fin == FIN_CORTADO && bode_reg()->n == 40 && oc_medidas_igual());

    /* con puntos singulares en la malla (el LC ideal: H = 0 y ok = 0 en f0) */
    sprintf(q, "K1 LC ideal");
    sr_init(3);
    kE = a4_cc(0, 1, 1.0);
    sr_el(SE_L, 1, 2, 10e-3);
    kS = sr_el(SE_C, 2, 0, 1e-6);
    canal(0, CANAL_V, kS);
    {
        double f0 = 1.0 / (2.0 * PI_T * sqrt(10e-3 * 1e-6));
        chk_cond(q, "se barre", bode_sr(kE, 0, -1, -1, f0 / 10.0, f0 * 10.0, 0.0) == SIM_OK);
        chk_cond(q, "medidas: por reg_ = por una copia", oc_medidas_igual());
    }
}

/* ---------------------------------------------------------------------------
 *  K2. El rango automático: 10^d con los bits de pow(10, d).
 *  Mutación: pot10 con otro índice del barrido grueso
 * ------------------------------------------------------------------------ */
static void oc_k2(void)
{
    static const double FC[4] = { 159.15494309189535, 1.5915494309189535e-4, 3.0e8, 12.0 };
    int caso, kE, kC;
    char q[64];
    for (caso = 0; caso < 4; caso++) {
        double pi, pf;
        int d0, d1;
        sprintf(q, "K2 RC de fc %g Hz", FC[caso]);
        sr_init(3);
        kE = a4_cc(0, 1, 1.0);
        sr_el(SE_R, 1, 2, 1e3);
        kC = sr_el(SE_C, 2, 0, 1.0 / (2.0 * PI_T * 1e3 * FC[caso]));
        canal(0, CANAL_V, kC);
        chk_cond(q, "se barre", bode_sr(kE, 0, -1, -1, 0.0, 0.0, 0.0) == SIM_OK);
        d0 = (int)floor(log10(bode_res()->f_ini) + 0.5);
        d1 = (int)floor(log10(bode_res()->f_fin) + 0.5);
        pi = pow(10.0, (double)d0);
        pf = pow(10.0, (double)d1);
        chk_cond(q, "f_ini y f_fin, las pow(10, d) exactas", memcmp(&pi, &bode_res()->f_ini, sizeof pi) == 0 &&
                                                            memcmp(&pf, &bode_res()->f_fin, sizeof pf) == 0);
        if (caso == 1) chk_cond(q, "el tope de abajo, 1 mHz", d0 == -3);
        if (caso == 2) chk_cond(q, "el tope de arriba, 1 GHz", d1 == 9);
        if (caso == 0) chk_cond(q, "de 10 Hz a 10 kHz", d0 == 1 && d1 == 4);
    }
}

/* ---------------------------------------------------------------------------
 *  K3. La topología, una vez
 * ------------------------------------------------------------------------ */
static SimRed oc_b;                               /* otra red, de otra topología */
static SimCanal oc_chb;
static BodeReg oc_g1;
static BodeMedidas oc_m1;

/* Un análisis de CA de otra red (cambia la topología de ca.c) */
static void oc_otro(void)
{
    Cx H;
    oc_h_plano(&oc_b, 0, &oc_chb, 1e3, &H);
}

static void oc_k3(void)
{
    static const double F[4] = { 100.0, 1e3, 1591.5494309189535, 5e3 };
    static CaThev th;
    Cx H1[4], H2;
    int kE, kS, i, igual = 1, trozos = 0;
    const char *q = "K3 la topolog" S_ii "a";

    /* la otra: E - R - C ‖ (R - L ‖ (R - R)), de 5 nodos */
    sr_init(5);
    a4_cc(0, 1, 1.0);
    sr_el(SE_R, 1, 2, 470.0); sr_el(SE_C, 2, 0, 1e-7);
    sr_el(SE_R, 2, 3, 100.0); sr_el(SE_L, 3, 0, 1e-3); sr_el(SE_R, 3, 4, 10.0); sr_el(SE_R, 4, 0, 1e3);
    oc_b = sr;
    canal_de(&saj, &oc_b, 1, CANAL_V, 4);
    oc_chb = saj.canal[1];

    /* la del Bode: el RLC pasa-banda */
    sr_init(4);
    kE = a4_cc(0, 1, 1.0);
    sr_el(SE_L, 1, 2, 10e-3); sr_el(SE_C, 2, 3, 1e-6);
    kS = sr_el(SE_R, 3, 0, 10.0);
    canal(0, CANAL_V, kS);
    chk_cond(q, "se barre", bode_sr(kE, 0, -1, -1, 0.0, 0.0, 0.0) == SIM_OK);
    oc_g1 = *bode_reg(); oc_m1 = bode_res()->m;

    /* bode_en con otro análisis en medio: los mismos bits.
     * Mutación: H_en que no mira ca_topo_gen, o ca_topologia que no la sube */
    for (i = 0; i < 4; i++) bode_en(F[i], &H1[i]);
    for (i = 0; i < 4; i++) {
        oc_otro();
        bode_en(F[i], &H2);
        if (memcmp(&H1[i], &H2, sizeof H2) != 0) igual = 0;
    }
    chk_cond(q, "bode_en tras otro an" S_aa "lisis de CA: los mismos bits", igual);
    ca_thevenin_red(&oc_b, 2, 0, 1e3, &th);
    bode_en(F[1], &H2);
    chk_cond(q, "y tras un Th" S_ee "venin", memcmp(&H1[1], &H2, sizeof H2) == 0);
    chk_cond(q, "en la malla, el de la malla", bode_en(oc_g1.f[100], &H2) && memcmp(&H2, &oc_g1.H[100], sizeof H2) == 0);

    /* por trozos, con otro análisis entre trozo y trozo: el mismo Bode */
    bode_iniciar_red(&sr, kE, &saj.canal[0], -1, -1, 0.0, 0.0, 0.0);
    while (bode_avanzar(60ul)) { oc_otro(); trozos++; }
    chk_cond(q, "por trozos con otro an" S_aa "lisis en medio: el mismo registro y las mismas medidas",
             trozos > 100 && memcmp(&oc_g1, bode_reg(), sizeof oc_g1) == 0 && memcmp(&oc_m1, &bode_res()->m, sizeof oc_m1) == 0);
    /* y otro Bode en medio: al volver, bode_en es el de la red del último */
    bode_en(F[2], &H2);
    canal_de(&saj, &oc_b, 1, CANAL_V, 4);
    bode_iniciar_red(&oc_b, 0, &saj.canal[1], -1, -1, 0.0, 0.0, 0.0);
    {
        Cx Hb, Hp;
        int okb, okp;
        okb = bode_en(F[2], &Hb);
        okp = oc_h_plano(&oc_b, 0, &oc_chb, F[2], &Hp);
        chk_cond(q, "otro Bode: bode_en ya es el suyo", okb == okp && memcmp(&Hb, &Hp, sizeof Hb) == 0 &&
                                                        memcmp(&Hb, &H2, sizeof Hb) != 0);
    }
}

/* ---------------------------------------------------------------------------
 *  K4. La red fija
 * ------------------------------------------------------------------------ */
/* Resuelve con ca_ensambla_fija (fija = 1) o ca_ensambla sobre la
 * topología de ahora: la excitación ex, la solución en x y las operaciones
 * que contó ca_ensambla en *ops */
static int oc_resuelve(const SimRed *s, const CaExc *ex, double f, int fija, Cx *x, unsigned long *ops)
{
    static Cx b[LUC_MAX];
    SimDiag d;
    unsigned long o0 = ca_ops();
    int e;
    memset(&d, 0, sizeof d);
    d.elem = d.nodo = d.elem_aviso = -1;
    e = fija ? ca_ensambla_fija(s, f, &d) : ca_ensambla(s, f, &d);
    *ops = ca_ops() - o0;
    if (e != SIM_OK) return e;
    ca_b_fuentes(s, ex, b);
    ca_sustituye(b, x);
    return SIM_OK;
}

static void oc_k4(void)
{
    static const double FR[5] = { 0.01, 0.3, 1.0, 3.7, 100.0 };   /* por la f de la red */
    static SimRed s1, s2;
    static SimLin L1, L2;
    static Cx x1[LUC_MAX], x2[LUC_MAX];
    unsigned az0 = azar_ca, c40 = c4_sem;
    int caso, redes = 0, malos = 0, conz = 0, ctl = 0, i, kE, kS, kD;
    double F[5];
    const char *q = "K4 la red fija";

    /* ---- Redes al azar: el Bode (la base) contra la matriz entera ----
     * Mutación: la base sin mirar la generación de la topología (la de la
     * red de antes), o con la Y(f) de una Z dentro */
    azar_ca = 20260928u; c4_sem = 8642u;
    for (caso = 0; caso < 400; caso++) {
        double f = 50.0 * pow(10.0, 3.0 * u_ca());
        int kc0, kc1, kv, ka, kw, kf;
        kf = red_rlc_seno(f, &kc0, &kc1, &kv, &ka, &kw);
        if (caso % 3 == 1 && sr.nel < MAX_EL - 3) {             /* una o dos controladas */
            int n, k;
            for (n = 0; n < 1 + caso % 2; n++) {
                int x = (int)(c4_u() * sr.nn), y = (int)(c4_u() * sr.nn), c = (int)(c4_u() * sr.nel), ci = c4_u() < 0.5;
                if (x == y) y = (x + 1) % sr.nn;
                if (sr.el[c].tipo == SE_JC || sr.el[c].tipo == SE_Z) ci = 0;
                k = sr_el(c4_u() < 0.5 ? SE_EC : SE_JC, x, y, 0.0);
                sr.el[k].val = (c4_u() < 0.5 ? -0.5 : 0.5) * (sr.el[k].tipo == SE_EC ? (ci ? 1e3 : 1.0) : (ci ? 1.0 : 1e-3));
                sr.el[k].ctl = (signed char)c; sr.el[k].ctl_i = (unsigned char)ci;
            }
            ctl++;
        }
        if (caso % 5 == 2 && sr.nel < MAX_EL - 1) {             /* una Z: sin base */
            int k = sr_el(SE_Z, (int)(u_ca() * sr.nn), 0, 200.0 + 800.0 * u_ca());
            if (sr.el[k].a == 0) sr.el[k].a = 1;
            sr.el[k].p[0] = (u_ca() < 0.5 ? -1.0 : 1.0) * 500.0 * u_ca();
            sr.el[k].p[1] = f;
            conz++;
        }
        if (caso % 7 == 3 && sr.nel < MAX_EL - 3 && sr.nn < MAX_NODOS - 2) {   /* un trozo que flota: su ancla */
            int n = sr.nn, k;
            sr.nn += 2;
            sr_el(SE_R, n, n + 1, 1e3);
            sr_el(SE_L, n, n + 1, 1e-3);
            k = sr_el(SE_VMI, n, 1, 0.0);                        /* solo un voltímetro lo toca */
            sr.el[k].instr = 'V'; sr.el[k].med = MED_CA;
        }
        canal(0, CANAL_V, kc0);
        if (bode_iniciar_red(&sr, kf, &saj.canal[0], -1, -1, f / 100.0, f * 100.0, 0.0) != SIM_OK) continue;
        redes++;
        for (i = 0; i < 5; i++) F[i] = f * FR[i];
        if (!oc_igual_plano(kf, &saj.canal[0], F, 5)) malos++;
    }
    azar_ca = az0; c4_sem = c40;
    chk_cond(q, "redes al azar: al menos 300, con controladas y con Z", redes >= 300 && ctl >= 100 && conz >= 60);
    chk_cond(q, "redes al azar: H por el Bode = con la matriz entera, bit a bit", malos == 0);

    /* ---- La malla entera, en un Bode con controladas y una Z ---- */
    {
        const BodeReg *g;
        Cx H;
        int k, mal = 0;
        sr_init(4);
        kE = a4_cc(0, 1, 1.0);
        sr_el(SE_R, 1, 2, 1e3); sr_el(SE_C, 2, 0, 1e-7);
        i = sr_el(SE_JC, 3, 0, 2e-3); sr.el[i].ctl = 1; sr.el[i].ctl_i = 0;   /* g·V(R1) */
        sr_el(SE_L, 3, 0, 1e-2);
        kS = sr_el(SE_R, 3, 0, 2e3);
        canal(0, CANAL_V, kS);
        chk_cond(q, "con una controlada: se barre", bode_sr(kE, 0, -1, -1, 0.0, 0.0, 0.0) == SIM_OK);
        g = bode_reg();
        for (k = 0; k < g->n; k += 3) {
            int ok = oc_h_plano(&sr, kE, &saj.canal[0], g->f[k], &H);
            if (ok != g->ok[k] || memcmp(&H, &g->H[k], sizeof H) != 0) mal++;
        }
        chk_cond(q, "con una controlada: la malla, la de la matriz entera", mal == 0);
        i = sr_el(SE_Z, 2, 3, 300.0); sr.el[i].p[0] = 400.0; sr.el[i].p[1] = 1e3;
        chk_cond(q, "con una Z: se barre", bode_sr(kE, 0, -1, -1, 0.0, 0.0, 0.0) == SIM_OK);
        g = bode_reg();
        for (k = 0, mal = 0; k < g->n; k += 3) {
            int ok = oc_h_plano(&sr, kE, &saj.canal[0], g->f[k], &H);
            if (ok != g->ok[k] || memcmp(&H, &g->H[k], sizeof H) != 0) mal++;
        }
        chk_cond(q, "con una Z: la malla, la de la matriz entera", mal == 0);
    }

    /* ---- Con no lineales en Q: el BJT, el AO y el diodo ---- */
    {
        static const double FN[5] = { 10.0, 100.0, 1e3, 10e3, 1e6 };
        int kx;
        q4_ec(12.0, 47e3, 10e3, 2.2e3, 470.0, 1, 100.0, 0.0);     /* el emisor común de [62] */
        sr.nn = 6;
        kx = t4_seno(0, 5, 1e-3, 1e3, 0.0);
        sr_el(SE_C, 5, 2, 10e-6);
        sr_el(SE_C, 3, 0, 100e-6);
        canal(0, CANAL_NODO, 4);
        chk_cond(q, "BJT: se barre", bode_sr(kx, 0, -1, -1, 0.0, 0.0, 0.0) == SIM_OK);
        chk_cond(q, "BJT: H por el Bode = con la matriz entera", oc_igual_plano(kx, &saj.canal[0], FN, 5));
        sr_init(5);                                                /* el integrador con pérdidas: R, C y Rf */
        kx = t4_seno(0, 1, 1.0, 1e3, 0.0);
        sr_el(SE_R, 1, 2, 1e3); sr_el(SE_C, 2, 3, 1e-7); sr_el(SE_R, 2, 3, 1e5);
        kS = a4_ao(0, 2, 3, 0, 1, 1e5);
        canal(0, CANAL_NODO, 3);
        chk_cond(q, "AO: se barre", bode_sr(kx, 0, -1, -1, 0.0, 0.0, 0.0) == SIM_OK);
        chk_cond(q, "AO: H por el Bode = con la matriz entera", oc_igual_plano(kx, &saj.canal[0], FN, 5));
        kx = p4_diodo(0, &kD);
        sr.nn = 5;
        sr_el(SE_C, 3, 4, 1e-6); sr_el(SE_R, 4, 0, 100.0);
        chk_cond(q, "diodo: se barre", bode_sr(kx, 0, -1, -1, 0.0, 0.0, 0.0) == SIM_OK);
        chk_cond(q, "diodo: H por el Bode = con la matriz entera", oc_igual_plano(kx, &saj.canal[0], FN, 5));
    }

    /* ---- La resonancia ideal: singular en f0 por los dos caminos ---- */
    {
        double f0 = 1.0 / (2.0 * PI_T * sqrt(10e-3 * 1e-6)), FS[3];
        Cx H;
        sr_init(3);
        kE = a4_cc(0, 1, 1.0);
        sr_el(SE_L, 1, 2, 10e-3);
        kS = sr_el(SE_C, 2, 0, 1e-6);
        canal(0, CANAL_V, kS);
        FS[0] = f0; FS[1] = f0 * 0.5; FS[2] = f0 * 2.0;
        chk_cond(q, "LC ideal: se inicia", bode_iniciar_red(&sr, kE, &saj.canal[0], -1, -1, 100.0, 1e4, 0.0) == SIM_OK);
        chk_cond(q, "LC ideal: en f0, singular (H = 0) por los dos caminos",
                 bode_en(f0, &H) == 0 && oc_igual_plano(kE, &saj.canal[0], FS, 3));
    }

    /* ---- La base es de (topología, red, linealización) ----
     * Con la misma topología, otra red (otra R) u otra linealización la
     * rehacen: la solución, la de la matriz entera, y las mismas operaciones.
     * Mutación: la clave de la base sin la red o sin la linealización */
    {
        CaExc ex;
        SimDiag d;
        unsigned long o1, o2;
        int e1, e2;
        sr_init(4);
        kE = a4_cc(0, 1, 1.0);
        sr_el(SE_R, 1, 2, 1e3); sr_el(SE_C, 2, 0, 1e-6); sr_el(SE_L, 2, 3, 1e-2);
        kS = sr_el(SE_R, 3, 0, 50.0);
        s1 = sr; s2 = sr; s2.el[kS].val = 75.0;
        memset(&ex, 0, sizeof ex);
        ex.U[kE] = cx(1.0, 0.0); ex.activa = 1u << kE;
        memset(&d, 0, sizeof d);
        ca_topologia(&s1, &d);
        ca_pon_q(0);
        oc_resuelve(&s1, &ex, 500.0, 1, x1, &o1);                 /* la base de s1 */
        e1 = oc_resuelve(&s2, &ex, 500.0, 1, x1, &o1);            /* s2: misma topología, otra R */
        e2 = oc_resuelve(&s2, &ex, 500.0, 0, x2, &o2);
        chk_cond(q, "otra red con la misma topolog" S_ii "a: la rehace", e1 == SIM_OK && e2 == SIM_OK &&
                 memcmp(x1, x2, (size_t)ca_n() * sizeof x1[0]) == 0);
        chk_cond(q, "y cuenta las mismas operaciones", o1 == o2 && o1 > 0);

        p4_diodo(0, &kD);
        s1 = sr;
        memset(&d, 0, sizeof d);
        chk_cond(q, "diodo: el punto Q", ca_punto_q(&s1, &L1, &d) == SIM_OK && L1.hay);
        L2 = L1;
        L2.g[kD][0][0] *= 3.0;
        memset(&ex, 0, sizeof ex);
        ex.U[1] = cx(1.0, 0.0); ex.activa = 1u << 1;
        ca_apaga(&s1, ex.activa);
        ca_topologia(&s1, &d);
        ca_pon_q(&L1);
        oc_resuelve(&s1, &ex, 1e3, 1, x1, &o1);                   /* la base con L1 */
        ca_pon_q(&L2);
        e1 = oc_resuelve(&s1, &ex, 1e3, 1, x1, &o1);
        e2 = oc_resuelve(&s1, &ex, 1e3, 0, x2, &o2);
        chk_cond(q, "otra linealizaci" S_oo "n: la rehace", e1 == SIM_OK && e2 == SIM_OK &&
                 memcmp(x1, x2, (size_t)ca_n() * sizeof x1[0]) == 0 && o1 == o2);
        ca_pon_q(0);
    }
}

/* ---------------------------------------------------------------------------
 *  K5. 1/a (cx_inv, sin lo que sobra de 1 + 0j) = cx_div(1, a), bit a bit:
 *  ceros con signo, subnormales, enormes, infinitos, NaN y al azar.
 *  Mutación: -r en vez de 0 - r (el -0), r en vez de r + 0 (el -0), o el
 *  cero de 0/re sin su signo
 * ------------------------------------------------------------------------ */
static const double oc_cero = 0.0;

static void oc_k5(void)
{
    static const double V[14] = { 0.0, -0.0, 1.0, -1.0, 3.0, -0.75, 1e-310, -4.9e-324, 1e300, -1.7e308,
                                  1e-160, 2.5e-170, 0.0, 0.0 };
    unsigned sem = 13579u;
    int i, j, mal = 0, n = 0;
    double w[14];
    memcpy(w, V, sizeof w);
    w[12] = -HUGE_VAL;                                        /* -inf */
    w[13] = oc_cero / oc_cero;                                /* NaN */
    for (i = 0; i < 14; i++)
        for (j = 0; j < 14; j++) {
            Cx a = cx(w[i], w[j]), p = cx_inv(a), q = cx_div(cx(1.0, 0.0), a);
            n++;
            if (memcmp(&p, &q, sizeof p) != 0) mal++;
        }
    for (i = 0; i < 20000; i++) {                               /* al azar, de 1e-300 a 1e300 y con signo */
        double x[2];
        Cx a, p, q;
        for (j = 0; j < 2; j++) {
            sem = sem * 1103515245u + 12345u;
            x[j] = ldexp((double)(sem >> 8) / 16777216.0 + 0.5, (int)(sem % 2000u) - 1000) * ((sem >> 4) & 1u ? -1.0 : 1.0);
            if ((sem >> 5) % 16u == 0) x[j] = 0.0;                 /* reales e imaginarios puros */
        }
        a = cx(x[0], x[1]); p = cx_inv(a); q = cx_div(cx(1.0, 0.0), a);
        n++;
        if (memcmp(&p, &q, sizeof p) != 0) mal++;
    }
    chk_cond("K5 cx_inv", "1/a = cx_div(1, a) bit a bit (casos raros y 20 000 al azar)", n == 20196 && mal == 0);
}

/* ---------------------------------------------------------------------------
 *  K6. La fase desenrollada (sin la división cuando el salto cabe): la de
 *  la fórmula de siempre, bit a bit, en un Bode que da la vuelta.
 *  Mutación: el atajo con otro borde (t <= 360) o sin el t >= 0
 * ------------------------------------------------------------------------ */
static void oc_k6(void)
{
    const BodeReg *g;
    int kE, k, st, adelanto;
    const char *q = "K6 la fase desenrollada";
    /* Tres etapas con seguidores: de atraso (990 Ω en serie y 10 Ω con 1 uF a
     * tierra: polo en 159 Hz, cero en 15,9 kHz), la fase baja a -236° y
     * vuelve; de adelanto (990 Ω ‖ 1 uF en serie y 10 Ω a tierra), sube a
     * +236° y vuelve. Bajando de -180° la cruda salta +360 (el d + 180 pasa
     * de 360) y subiendo de +180°, -360 (queda por debajo de 0) */
    for (adelanto = 0; adelanto < 2; adelanto++) {
        double ant = 0.0, fext = 0.0;
        int mal = 0;
        sr_init(10);
        kE = a4_cc(0, 1, 1.0);
        for (st = 0; st < 3; st++) {
            int e = 1 + 3 * st;                                /* 1, 4, 7: la entrada de la etapa */
            sr_el(SE_R, e, e + 1, 990.0);
            if (adelanto) { sr_el(SE_C, e, e + 1, 1e-6); sr_el(SE_R, e + 1, 0, 10.0); sr_el(SE_R, e + 2, 0, 1e3); }   /* e + 2, sin usar */
            else { sr_el(SE_R, e + 1, e + 2, 10.0); sr_el(SE_C, e + 2, 0, 1e-6); }
            if (st < 2) a4_ao(e + 1, e + 3, e + 3, 0, 0, 0.0);    /* el seguidor a la siguiente */
        }
        canal(0, CANAL_NODO, 8);
        chk_cond(q, adelanto ? "adelanto: se barre" : "atraso: se barre", bode_sr(kE, 0, -1, -1, 1.0, 1e6, 0.0) == SIM_OK);
        g = bode_reg();
        for (k = 0; k < g->n; k++) {
            double fa = g->ok[k] ? cx_arg(g->H[k]) * 180.0 / PI_T : (k > 0 ? ant : 0.0);
            if (k > 0 && g->ok[k]) {
                double d = fa - ant;
                d -= 360.0 * floor((d + 180.0) / 360.0);
                fa = ant + d;
            }
            if (memcmp(&fa, &g->fase[k], sizeof fa) != 0) mal++;
            if (fabs(fa) > fabs(fext)) fext = fa;
            ant = fa;
        }
        chk_cond(q, adelanto ? "adelanto: sube de +180" S_DEG ", vuelve y es la de siempre, bit a bit"
                             : "atraso: baja de -180" S_DEG ", vuelve y es la de siempre, bit a bit",
                 (adelanto ? fext > 200.0 : fext < -200.0) && fabs(ant) < 30.0 && mal == 0);
    }
}

/* ---------------------------------------------------------------------------
 *  K7. ca_sustituye con la lista de anclas de la topología (en vez de mirar
 *  los MAX_NODOS nodos): un b con todo lleno da lo que da con las filas
 *  ancladas puestas a 0 a mano, con una red de dos anclas, luego una sin
 *  ninguna y otra vez la primera.
 *  Mutación: la lista sin rehacer en ca_topologia, o sin usar
 * ------------------------------------------------------------------------ */
static int oc_anclas_bien(const SimRed *s, int *nanc)
{
    static Cx b[LUC_MAX], b2[LUC_MAX], x[LUC_MAX], x2[LUC_MAX];
    const SimTopoCA *t;
    SimDiag d;
    int k, n;
    memset(&d, 0, sizeof d);
    d.elem = d.nodo = d.elem_aviso = -1;
    if (ca_topologia(s, &d) != SIM_OK) return 0;
    ca_pon_q(0);
    if (ca_ensambla(s, 1e3, &d) != SIM_OK) return 0;
    t = ca_topo();
    n = ca_n();
    for (k = 0; k < n; k++) { b[k] = cx(1.0 + k, -0.5 * k); b2[k] = b[k]; }
    for (*nanc = 0, k = 0; k < MAX_NODOS; k++)
        if (t->col_phi[k] >= 0 && t->ancla[k]) { b2[t->col_phi[k]] = cx(0.0, 0.0); (*nanc)++; }
    ca_sustituye(b, x);
    ca_sustituye(b2, x2);
    return memcmp(x, x2, (size_t)n * sizeof x[0]) == 0;
}

static void oc_k7(void)
{
    static SimRed s1, s2;
    int na1, na2, na3, b1, b2, b3;
    /* dos trozos que solo toca un voltímetro: dos anclas */
    sr_init(6);
    a4_cc(0, 1, 1.0);
    sr_el(SE_R, 1, 0, 1e3);
    sr_el(SE_R, 2, 3, 1e3); sr_el(SE_C, 3, 2, 1e-6);
    sr_el(SE_R, 4, 5, 2e3); sr_el(SE_L, 4, 5, 1e-3);
    sr.el[sr_el(SE_VMI, 2, 1, 0.0)].instr = 'V';
    sr.el[sr_el(SE_VMI, 4, 1, 0.0)].instr = 'V';
    s1 = sr;
    sr_init(3);
    a4_cc(0, 1, 1.0);
    sr_el(SE_R, 1, 2, 1e3); sr_el(SE_C, 2, 0, 1e-6);
    s2 = sr;
    b1 = oc_anclas_bien(&s1, &na1);
    b2 = oc_anclas_bien(&s2, &na2);
    b3 = oc_anclas_bien(&s1, &na3);
    chk_cond("K7 las anclas", "ca_sustituye: las filas ancladas a 0 (2 anclas, 0 y 2 otra vez)",
             b1 && b2 && b3 && na1 == 2 && na2 == 0 && na3 == 2);
}

static void test_opt_cabode(void)
{
    printf("[75c] rev9: el Bode y la CA con menos trabajo (grupo cabode)\n");
    oc_k1();
    oc_k2();
    oc_k3();
    oc_k4();
    oc_k5();
    oc_k6();
    oc_k7();
}
