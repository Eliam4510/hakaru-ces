/* --------------------------------------------------------------------------
 *  49. Etapa 3: la calculadora de CA
 *
 *  Se incluye en host_test.c (usa chk, chk_t, chk_cond, chk_cx, falla,
 *  lineas, pon_val, sr_init, sr_el, sr_alterna, sr_z y la red sr). Los
 *  números de la tabla se calcularon aparte con mpmath (40 cifras).
 * ----------------------------------------------------------------------- */
#include "calca.h"

#define R2_K 1.41421356237309504880

static Red tk_red;
static unsigned long tk_max_ops;              /* el mayor gasto de la tabla */

static unsigned tk_sem = 24681357u;
static double tk_u(void) { tk_sem = tk_sem * 1103515245u + 12345u; return (double)((tk_sem >> 8) & 0xFFFFFF) / 16777216.0; }
static int tk_n(int n) { return (int)(tk_u() * (double)n); }

/* La calculadora de un tirón */
static const CalRes *tk_calcula(const Circuito *c)
{
    const CalRes *s;
    red_extraer(c, &tk_red);
    cal_iniciar(c, &tk_red);
    while (cal_avanzar(~0ul)) {}
    s = cal_res();
    if (s->ops > tk_max_ops) tk_max_ops = s->ops;
    return s;
}

/* El mensaje cabe en 4 líneas de 52 y dice lo que debe */
static void tk_msg(const char *q, const Circuito *c, const CalRes *s, const char *debe)
{
    char m[400], d[520];
    cal_mensaje(c, s, m, sizeof m);
    chk_cond(q, "el mensaje cabe en 4 l" S_ii "neas de 52", lineas(m, 52) <= 4);
    if (debe) {
        pruebas++;
        if (!strstr(m, debe)) { sprintf(d, "el mensaje dice " S_LARR "%s" S_RARR ": %s", debe, m); falla(q, d, 0, 1); }
    }
}

/* E1 alterna (E∠0) de (0,1) a (0,0); K1 (el tipo t1) de (0,0) a (1,0), con
 * el valor v1 (o [?!] si v1 <= 0); R1 de (1,0) a (1,1); V1 en paralelo con
 * R1 (de (2,0) a (2,1)) marcando «lectura»; cable de (1,1) a (0,1). En k:
 * K1, R1 y V1 */
static int tk_vr(Circuito *c, double E, double f, int t1, double v1, double R, double lectura, int *k)
{
    int kE;
    circ_init(c, "VR");
    kE = circ_elemento(c, EL_FUENTE, 0, 1, DIR_ARR);
    pon_val(c, kE, E);
    c->el[kE].variante = FORMA_FASOR;
    c->el[kE].val.ang = 0.0; c->el[kE].val.est_ang = M_DATO;
    c->frec.v = f; c->frec.est = M_DATO;
    k[0] = circ_elemento(c, (TipoEl)t1, 0, 0, DIR_DER);
    if (v1 > 0.0) pon_val(c, k[0], v1); else c->el[k[0]].val.est = M_BUSCAR;
    k[1] = pon_val(c, circ_elemento(c, EL_RES, 1, 0, DIR_ABA), R);
    circ_cable(c, 1, 0, DIR_DER);
    k[2] = circ_elemento(c, EL_VOLT, 2, 0, DIR_ABA);
    c->el[k[2]].V.v = lectura; c->el[k[2]].V.est = M_DATO;
    circ_cable(c, 2, 1, DIR_IZQ);
    circ_cable(c, 1, 1, DIR_IZQ);
    return kE;
}

/* RLC serie (10 ohm, 10 mH, 1 uF) con E = 1 V, frec [?!] y A1 marcando 0,05 */
static void tk_rlc_f(Circuito *c)
{
    int k;
    circ_init(c, "RLC f");
    k = circ_elemento(c, EL_FUENTE, 0, 1, DIR_ARR);
    pon_val(c, k, 1.0);
    c->el[k].variante = FORMA_FASOR;
    c->frec.est = M_BUSCAR;
    pon_val(c, circ_elemento(c, EL_RES, 0, 0, DIR_DER), 10.0);
    pon_val(c, circ_elemento(c, EL_BOB, 1, 0, DIR_DER), 10e-3);
    pon_val(c, circ_elemento(c, EL_COND, 2, 0, DIR_DER), 1e-6);
    k = circ_elemento(c, EL_AMP, 3, 0, DIR_ABA);
    c->el[k].I.v = 0.05; c->el[k].I.est = M_DATO;
    circ_cable(c, 3, 1, DIR_IZQ); circ_cable(c, 2, 1, DIR_IZQ); circ_cable(c, 1, 1, DIR_IZQ);
}

/* El motor: E1 = 230 V a 50 Hz; W1 en la línea con fp = fp1; C1 [?!] en
 * paralelo con W2 y Z1; W2 en serie con Z1 con P = 10 000 y fp = +0,7 */
static void tk_motor(Circuito *c, double fp1)
{
    int k;
    circ_init(c, "motor");
    k = circ_elemento(c, EL_FUENTE, 0, 1, DIR_ARR);
    pon_val(c, k, 230.0);
    c->el[k].variante = FORMA_FASOR;
    c->frec.v = 50.0; c->frec.est = M_DATO;
    k = circ_elemento(c, EL_VATI, 0, 0, DIR_DER);
    c->el[k].aux.v = fp1; c->el[k].aux.est = M_DATO;
    k = circ_elemento(c, EL_COND, 1, 0, DIR_ABA);
    c->el[k].val.est = M_BUSCAR;
    circ_cable(c, 1, 0, DIR_DER);
    k = circ_elemento(c, EL_VATI, 2, 0, DIR_DER);
    c->el[k].val.v = 10000.0; c->el[k].val.est = M_DATO;
    c->el[k].aux.v = 0.7; c->el[k].aux.est = M_DATO;
    k = circ_elemento(c, EL_IMP, 3, 0, DIR_ABA);
    c->el[k].val.est = M_BUSCAR; c->el[k].val.est_ang = M_BUSCAR;
    circ_cable(c, 3, 1, DIR_IZQ); circ_cable(c, 2, 1, DIR_IZQ); circ_cable(c, 1, 1, DIR_IZQ);
}

/* El puente de Maxwell a f: E1 = 10 V; arriba a la izquierda R1 = 1 k en
 * paralelo con C1 = 100 nF, abajo R2 = 500; arriba a la derecha R3 = 200,
 * abajo Rx y Lx en serie ([?!] los dos); V1 entre los puntos medios marca 0 */
static void tk_maxwell(Circuito *c, double f, int *kx, int *kl)
{
    int k;
    circ_init(c, "Maxwell");
    k = circ_elemento(c, EL_FUENTE, 0, 2, DIR_ARR);
    pon_val(c, k, 10.0);
    c->el[k].variante = FORMA_FASOR;
    c->frec.v = f; c->frec.est = M_DATO;
    circ_cable(c, 0, 1, DIR_ARR); circ_cable(c, 0, 0, DIR_DER);
    circ_cable(c, 0, 2, DIR_ABA); circ_cable(c, 0, 3, DIR_DER);
    circ_cable(c, 1, 0, DIR_DER); circ_cable(c, 2, 0, DIR_DER); circ_cable(c, 3, 0, DIR_DER);
    pon_val(c, circ_elemento(c, EL_RES, 1, 0, DIR_ABA), 1e3);
    pon_val(c, circ_elemento(c, EL_COND, 2, 0, DIR_ABA), 100e-9);
    circ_cable(c, 1, 1, DIR_DER);
    pon_val(c, circ_elemento(c, EL_RES, 1, 1, DIR_ABA), 500.0);
    circ_cable(c, 1, 2, DIR_ABA);
    pon_val(c, circ_elemento(c, EL_RES, 4, 0, DIR_ABA), 200.0);
    *kx = circ_elemento(c, EL_RES, 4, 1, DIR_ABA);
    c->el[*kx].val.est = M_BUSCAR;
    *kl = circ_elemento(c, EL_BOB, 4, 2, DIR_ABA);
    c->el[*kl].val.est = M_BUSCAR;
    circ_cable(c, 1, 3, DIR_DER); circ_cable(c, 2, 3, DIR_DER); circ_cable(c, 3, 3, DIR_DER);
    k = circ_elemento(c, EL_VOLT, 2, 1, DIR_DER);
    c->el[k].V.v = 0.0; c->el[k].V.est = M_DATO;
    circ_cable(c, 3, 1, DIR_DER);
}

/* Dos resultados iguales bit a bit (lo que se enseña de cada solución) */
static int tk_iguales(const CalRes *a, const CalRes *b)
{
    int i;
    if (a->estado != b->estado || a->nsol != b->nsol || a->mas != b->mas ||
        a->evaluaciones != b->evaluaciones || a->ops != b->ops || a->elem != b->elem || a->iecu != b->iecu) return 0;
    if (memcmp(&a->peor, &b->peor, sizeof a->peor) || memcmp(&a->lo_dato, &b->lo_dato, sizeof a->lo_dato) ||
        memcmp(&a->hi_dato, &b->hi_dato, sizeof a->hi_dato)) return 0;
    for (i = 0; i < a->nsol; i++) {
        const CalSol *x = &a->sol[i], *y = &b->sol[i];
        if (memcmp(x->x, y->x, sizeof x->x) || memcmp(&x->resid, &y->resid, sizeof x->resid) ||
            memcmp(&x->cond, &y->cond, sizeof x->cond)) return 0;
        if (memcmp(x->g.V, y->g.V, sizeof x->g.V) || memcmp(x->g.I, y->g.I, sizeof x->g.I) ||
            memcmp(x->g.phi, y->g.phi, sizeof x->g.phi) || x->g.buscado != y->g.buscado) return 0;
    }
    return 1;
}

/* Por trozos: con `paso` operaciones por llamada. Devuelve las llamadas */
static int tk_trozos(const Circuito *c, unsigned long paso)
{
    int n = 1;
    red_extraer(c, &tk_red);
    cal_iniciar(c, &tk_red);
    while (cal_avanzar(paso)) n++;
    return n;
}

/* ---- El jacobiano: la red sr a f, con su LU (para ca_derivada) ---- */
static SimRed tk_s;
static CaExc  tk_exc;
static Cx     tk_b[LUC_MAX], tk_x[LUC_MAX], tk_v[LUC_MAX], tk_dx[LUC_MAX];

static int tk_vi(const SimRed *s0, double f, Cx *V, Cx *I)
{
    SimDiag d;
    int k;
    tk_s = *s0;
    memset(&d, 0, sizeof d);
    d.elem = d.nodo = d.elem_aviso = -1;
    ca_excitacion(&tk_s, f, &tk_exc, &d);
    if (d.estado != SIM_OK) return 0;
    ca_apaga(&tk_s, tk_exc.activa);
    if (ca_topologia(&tk_s, &d) != SIM_OK || ca_ensambla(&tk_s, f, &d) != SIM_OK) return 0;
    ca_b_fuentes(&tk_s, &tk_exc, tk_b);
    ca_sustituye(tk_b, tk_x);
    for (k = 0; k < tk_s.nel; k++) ca_vi(&tk_s, &tk_exc, tk_x, f, k, &V[k], &I[k]);
    return 1;
}

static void tk_pasivo(int a, int b, double f)
{
    double r = tk_u(), X = 10.0 + 990.0 * tk_u(), w = 2.0 * PI_T * f;
    if (r < 0.3) sr_el(SE_R, a, b, X);
    else if (r < 0.5) sr_el(SE_C, a, b, 1.0 / (w * X));
    else if (r < 0.7) sr_el(SE_L, a, b, X / w);
    else sr_z(a, b, X, (tk_u() < 0.5 ? -1.0 : 1.0) * (10.0 + 70.0 * tk_u()), tk_u() < 0.5 ? f : f * (0.5 + 1.5 * tk_u()));
}

/* Red de CA al azar: un árbol de R, C, L y Z (a su f o a otra), una E
 * alterna, y más: pasivos, E reales, J, vatímetros y voltímetros */
static void tk_red_jac(double f)
{
    int nn = 3 + tk_n(4), n, k, q, nel;
    sr_init(nn);
    for (n = 1; n < nn; n++) tk_pasivo(tk_n(n), n, f);
    sr_alterna(SE_E, 0, 1, 1.0 + 20.0 * tk_u(), 360.0 * tk_u() - 180.0, f);
    nel = 2 + tk_n(5);
    for (k = 0; k < nel && sr.nel < MAX_EL; k++) {
        int a = tk_n(nn), b = tk_n(nn);
        double r = tk_u();
        if (a == b) b = (a + 1) % nn;
        if (r < 0.45) tk_pasivo(a, b, f);
        else if (r < 0.55) { q = sr_alterna(SE_E, a, b, 1.0 + 10.0 * tk_u(), 360.0 * tk_u() - 180.0, f); sr.el[q].val = 10.0 + 100.0 * tk_u(); }
        else if (r < 0.65) sr_alterna(SE_J, a, b, 0.001 + 0.05 * tk_u(), 360.0 * tk_u() - 180.0, f);
        else if (r < 0.75) { q = sr_el(SE_AMI, a, b, 0.0); sr.el[q].instr = 'W'; }
        else if (r < 0.85) { q = sr_el(SE_VMI, a, b, 0.0); sr.el[q].instr = 'V'; }
        else { q = sr_el(SE_R, a, b, 1e3 + 1e4 * tk_u()); sr.el[q].instr = 'V'; }
    }
}

/* La red s movida en la incógnita (k, que) un paso h en u */
static void tk_perturba(SimRed *s, double *f, int k, int que, double h)
{
    SimEl *e = k >= 0 ? &s->el[k] : 0;
    int i;
    switch (que) {
    case IN_VAL:
        e->val *= exp(h);
        if (e->tipo == SE_Z) e->p[0] *= exp(h);
        break;
    case IN_ANGZ: {
        double m = hypot(e->val, e->p[0]), a = atan2(e->p[0], e->val) + h;
        e->val = m * cos(a); e->p[0] = m * sin(a);
        break;
    }
    case IN_MODU: e->p[0] *= exp(h); break;
    case IN_ANGU: e->p[2] += h * 180.0 / PI_T; break;
    default: {                                       /* IN_F: la Z del circuito sigue a la f */
        double f2 = *f * exp(h);
        for (i = 0; i < s->nel; i++) {
            SimEl *x = &s->el[i];
            if ((x->tipo == SE_E || x->tipo == SE_J) && x->forma == ONDA_SENO) x->p[1] = f2;
            if (x->tipo == SE_Z && x->p[1] == *f) x->p[1] = f2;
        }
        *f = f2;
        break;
    }
    }
}

/* Diferencia centrada de V e I de todos los elementos en la incógnita (k, que)
 * con paso h en u. 0 si alguna de las dos redes no se puede resolver */
static int tk_dif(const SimRed *b0, double f, int kk, int que, double h, Cx *dV, Cx *dI)
{
    static Cx Vp[MAX_EL], Ip[MAX_EL], Vm[MAX_EL], Im[MAX_EL];
    static SimRed mov;
    double fmas = f, fmenos = f;
    int m;
    mov = *b0;
    tk_perturba(&mov, &fmas, kk, que, h);
    if (!tk_vi(&mov, fmas, Vp, Ip)) return 0;
    mov = *b0;
    tk_perturba(&mov, &fmenos, kk, que, -h);
    if (!tk_vi(&mov, fmenos, Vm, Im)) return 0;
    for (m = 0; m < mov.nel; m++) {
        dV[m] = cx_esc(cx_menos(Vp[m], Vm[m]), 0.5 / h);
        dI[m] = cx_esc(cx_menos(Ip[m], Im[m]), 0.5 / h);
    }
    return 1;
}

/* ---- Circuitos sin dibujo (la Red a mano, como [5]) ---- */
static Circuito tk_c, tk_copia;
static Red      tk_r;
static int      tk_nn;

static int tk_el(TipoEl t, int a, int b, double v)
{
    Elemento *e;
    int k = tk_c.nel;
    if (k >= MAX_EL) return -1;
    e = &tk_c.el[k];
    memset(e, 0, sizeof *e);
    e->ref = -1;
    e->tipo = (unsigned char)t;
    e->xa = e->xb = (unsigned char)(k % REJ_COLS);
    e->ya = e->yb = (unsigned char)(k / REJ_COLS);
    if (v > 0.0) { e->val.v = v; e->val.est = M_DATO; }
    sprintf(e->etiq, "%c%d", tipo_info(t)->letra, k + 1);
    tk_r.a[k] = a; tk_r.b[k] = b;
    tk_c.nel++;
    return k;
}

static void tk_cierra(void)
{
    tk_r.nn = tk_nn;
    tk_r.nel = tk_c.nel;
    tk_r.error = RED_OK;
    tk_r.elem_error = -1;
    red_cierra(&tk_c, &tk_r);
}

/* Serie-paralelo al azar de resistencias (el generador de [5]) */
static void tk_sp(int a, int b, int prof)
{
    int op = (prof >= 3 || tk_c.nel >= 14) ? 0 : tk_n(3);
    if (op == 0) tk_el(EL_RES, a, b, 0.5 * (double)(1 + tk_n(200)));
    else if (op == 1) { int w = tk_nn++; tk_sp(a, w, prof + 1); tk_sp(w, b, prof + 1); }
    else { int n = 2 + tk_n(2), i; for (i = 0; i < n; i++) tk_sp(a, b, prof + 1); }
}

/* R, L o C al azar entre a y b, con |Z| de 10 a 1000 ohm a ω */
static int tk_rlc(int a, int b, double w)
{
    double r = tk_u(), X = 10.0 + 990.0 * tk_u();
    if (r < 0.4) return tk_el(EL_RES, a, b, X);
    if (r < 0.7) return tk_el(EL_BOB, a, b, X / w);
    return tk_el(EL_COND, a, b, 1.0 / (w * X));
}

/* El dato de la prueba inversa: módulo (o fasor) de V o I de kd, o |P| de kw */
static double tk_dato_de(const CaRes *g, int kd, int qd, int kw, double *ang)
{
    Cx X;
    if (kw >= 0) { *ang = 0.0; return fabs(g->lectura[kw]); }
    X = qd == Q_V ? g->V[kd] : g->I[kd];
    *ang = cx_arg(X) * 180.0 / PI_T;
    return cx_abs(X) / R2_K;
}

static void test_calca(void)
{
    static Circuito c, c2;
    static CalRes r1, r2;
    static CaRes ga;
    const CalRes *s;
    int k[4], kE, i, kx, kl, llamadas;
    const double ef = 1.0 / R2_K;

    printf("[49] Etapa 3: la calculadora de CA\n");
    tk_max_ops = 0;

    /* ---- E = 120 V a 60 Hz; R = 30; L [?!]; V1 sobre R marca 72 ---- */
    kE = tk_vr(&c, 120.0, 60.0, EL_BOB, 0.0, 30.0, 72.0, k);
    s = tk_calcula(&c);
    chk_cond("CA: L", "una inc" S_oo "gnita, un dato, una soluci" S_oo "n", s->estado == CAL_OK && s->ninc == 1 && s->necu == 1 && s->nsol == 1 && !s->mas);
    if (s->nsol >= 1) {
        chk_t("CA: L", "L = 0,10610329539459689 H", s->sol[0].x[0], 0.10610329539459689, 0.1061, 1e-12);
        chk_t("CA: L", "|I| = 2,4 A", cx_abs(s->sol[0].g.I[k[1]]) * ef, 2.4, 2.4, 1e-12);
        chk_t("CA: L", "|V_L| = 96 V", cx_abs(s->sol[0].g.V[k[0]]) * ef, 96.0, 96.0, 1e-12);
        chk_t("CA: L", "V1 marca 72", s->sol[0].g.lectura[k[2]], 72.0, 72.0, 1e-12);
        chk_cond("CA: L", "L1 sale como hallada", ((s->sol[0].g.buscado >> k[0]) & 1u) && s->sol[0].g.d.estado == SIM_OK);
        /* la solución como dato: el análisis directo da la lectura */
        c2 = c;
        cal_aplica(s, 0, &c2);
        red_extraer(&c2, &tk_red);
        chk_cond("CA: L", "cal_aplica: L1 pasa a dato", c2.el[k[0]].val.est == M_DATO && !circ_pide(&c2));
        chk_cond("CA: L", "cal_aplica: el an" S_aa "lisis directo marca 72", ca_analiza(&c2, &tk_red, &ga) == SIM_OK &&
                 fabs(ga.lectura[k[2]] - 72.0) <= 1e-12 * 72.0);
    }
    tk_msg("CA: L", &c, s, 0);
    {
        CalRes p;
        chk_cond("CA: L", "cal_plantea: listo (la l" S_ii "nea de estado)", cal_plantea(&c, &p) == CAL_OK && p.ninc == 1 && p.necu == 1 &&
                 p.inc[0].que == IN_VAL && p.inc[0].el == k[0] && p.ecu[0].que == EC_MOD && fabs(p.ecu[0].v - 72.0 * R2_K) < 1e-12);
    }

    /* ---- La misma con una C ---- */
    tk_vr(&c, 120.0, 60.0, EL_COND, 0.0, 30.0, 72.0, k);
    s = tk_calcula(&c);
    chk_cond("CA: C", "una soluci" S_oo "n", s->estado == CAL_OK && s->nsol == 1);
    if (s->nsol >= 1) chk_t("CA: C", "C = 66,31455962162306 " S_MU "F", s->sol[0].x[0], 66.31455962162306e-6, 66.3e-6, 1e-12);

    /* ---- V1 = 130: no se alcanza ---- */
    tk_vr(&c, 120.0, 60.0, EL_BOB, 0.0, 30.0, 130.0, k);
    s = tk_calcula(&c);
    chk_cond("CA: V1 = 130", "CAL_INCOMPAT sin soluciones", s->estado == CAL_INCOMPAT && s->nsol == 0);
    chk_t("CA: V1 = 130", "lo m" S_aa "s que marca V1: 120 V", s->hi_dato, 120.0, 120.0, 1e-6);
    chk_cond("CA: V1 = 130", "lo menos: casi 0 (3,79e-5 en la malla de " S_PM "6,5 d" S_ee "cadas)",
             s->lo_dato <= 1e-6 * s->hi_dato && fabs(s->lo_dato - 3.794733192e-5) <= 1e-8);
    tk_msg("CA: V1 = 130", &c, s, "de casi 0 a 120 V");
    tk_msg("CA: V1 = 130", &c, s, "Ning" S_uu "n valor de L1 da V1 = 130 V");

    /* ---- V1 = 120: solo en el borde, con L1 -> 0 ---- */
    tk_vr(&c, 120.0, 60.0, EL_BOB, 0.0, 30.0, 120.0, k);
    s = tk_calcula(&c);
    chk_cond("CA: V1 = 120", "CAL_BORDE con L1 " S_RARR " 0", s->estado == CAL_BORDE && s->nsol == 0 && s->elem == 0 && s->peor < 0.0);
    tk_msg("CA: V1 = 120", &c, s, "solo se alcanza en el borde: con L1 " S_RARR " 0");

    /* ---- RLC con frec [?!]: A1 marca 0,05 -> dos frecuencias ---- */
    tk_rlc_f(&c);
    s = tk_calcula(&c);
    chk_cond("CA: f", "dos soluciones", s->estado == CAL_OK && s->nsol == 2 && s->ninc == 1 && s->inc[0].que == IN_F);
    if (s->nsol == 2) {
        chk_t("CA: f", "f1 = 1459,6743686165150 Hz", s->sol[0].x[0], 1459.6743686165150, 1459.7, 1e-11);
        chk_t("CA: f", "f2 = 1735,3388163274111 Hz", s->sol[1].x[0], 1735.3388163274111, 1735.3, 1e-11);
        chk_t("CA: f", "en f1, |I| = 0,05 A", cx_abs(s->sol[0].g.I[0]) * ef, 0.05, 0.05, 1e-12);
        chk_t("CA: f", "la CaRes va a f1", s->sol[0].g.f, 1459.6743686165150, 1459.7, 1e-11);
    }
    tk_msg("CA: f", &c, s, 0);

    /* ---- E = 100∠0; Z1 [?!] (módulo y ángulo); I(Z1) = 5∠-30 ---- */
    circ_init(&c, "Z");
    kE = circ_elemento(&c, EL_FUENTE, 0, 1, DIR_ARR);
    pon_val(&c, kE, 100.0); c.el[kE].variante = FORMA_FASOR;
    c.frec.v = 60.0; c.frec.est = M_DATO;
    i = circ_elemento(&c, EL_IMP, 0, 0, DIR_DER);
    c.el[i].val.est = M_BUSCAR; c.el[i].val.est_ang = M_BUSCAR;
    c.el[i].I.v = 5.0; c.el[i].I.est = M_DATO; c.el[i].I.ang = -30.0; c.el[i].I.est_ang = M_DATO;
    circ_cable(&c, 1, 0, DIR_ABA); circ_cable(&c, 1, 1, DIR_IZQ);
    s = tk_calcula(&c);
    chk_cond("CA: Z", "|Z1| y " S_ANG "Z1: una soluci" S_oo "n", s->estado == CAL_OK && s->nsol == 1 && s->ninc == 2 &&
             s->inc[0].que == IN_VAL && s->inc[1].que == IN_ANGZ);
    if (s->nsol >= 1) {
        chk_t("CA: Z", "|Z1| = 20", s->sol[0].x[0], 20.0, 20.0, 1e-12);
        chk_t("CA: Z", S_ANG "Z1 = 30" S_DEG, s->sol[0].x[1], 30.0, 30.0, 1e-12);
        chk_cond("CA: Z", "bien condicionada", s->sol[0].cond < 1e3);
    }

    /* ---- R = 3; L a 50 Hz; E1 [?!] (módulo y ángulo); I(R1) = 2∠-30 ---- */
    circ_init(&c, "E");
    kE = circ_elemento(&c, EL_FUENTE, 0, 1, DIR_ARR);
    c.el[kE].variante = FORMA_FASOR; c.el[kE].val.est = M_BUSCAR; c.el[kE].val.est_ang = M_BUSCAR;
    c.frec.v = 50.0; c.frec.est = M_DATO;
    i = pon_val(&c, circ_elemento(&c, EL_RES, 0, 0, DIR_DER), 3.0);
    c.el[i].I.v = 2.0; c.el[i].I.est = M_DATO; c.el[i].I.ang = -30.0; c.el[i].I.est_ang = M_DATO;
    pon_val(&c, circ_elemento(&c, EL_BOB, 1, 0, DIR_ABA), 0.012732395447351627);
    circ_cable(&c, 1, 1, DIR_IZQ);
    s = tk_calcula(&c);
    chk_cond("CA: E1", "una soluci" S_oo "n, sin referencia (hay un " S_aa "ngulo dato)", s->estado == CAL_OK && s->nsol == 1 && s->ref_fase < 0 && s->ninc == 2);
    if (s->nsol >= 1) {
        chk_t("CA: E1", "|E1| = 10", s->sol[0].x[0], 10.0, 10.0, 1e-12);
        chk_t("CA: E1", S_ANG "E1 = 23,130102354155979" S_DEG, s->sol[0].x[1], 23.130102354155979, 23.13, 1e-12);
        chk_t("CA: E1", "la CaRes lleva E1", s->sol[0].g.val_ang[kE], 23.130102354155979, 23.13, 1e-12);
    }

    /* ---- El puente de Maxwell, a 1 kHz y a 7 kHz: Rx = 100, Lx = 10 mH ---- */
    for (i = 0; i < 2; i++) {
        const char *q = i ? "CA: Maxwell 7 kHz" : "CA: Maxwell 1 kHz";
        tk_maxwell(&c, i ? 7e3 : 1e3, &kx, &kl);
        chk_cond(q, "el circuito es v" S_aa "lido", circ_valido(&c));
        s = tk_calcula(&c);
        chk_cond(q, "V1 = 0 cuenta 2: una soluci" S_oo "n", s->estado == CAL_OK && s->nsol == 1 && s->necu == 2 &&
                 s->ecu[0].que == EC_RE && s->ecu[1].que == EC_IM);
        if (s->nsol >= 1) {
            chk_t(q, "Rx = 100 " S_OHM, s->sol[0].x[0], 100.0, 100.0, 1e-10);
            chk_t(q, "Lx = 10 mH", s->sol[0].x[1], 0.01, 0.01, 1e-10);
            chk_cond(q, "V1 marca 0", s->sol[0].g.lectura[s->ecu[0].el] <= 1e-9);
        }
    }

    /* ---- El motor y la corrección de su fp ---- */
    tk_motor(&c, 0.95);
    s = tk_calcula(&c);
    chk_cond("CA: motor", "C1, |Z1| y " S_ANG "Z1: una soluci" S_oo "n", s->estado == CAL_OK && s->nsol == 1 && s->ninc == 3 && s->necu == 3);
    if (s->nsol >= 1) {
        chk_t("CA: motor", "C1 = 416,10139603289948 " S_MU "F", s->sol[0].x[0], 416.10139603289948e-6, 416e-6, 1e-10);
        chk_t("CA: motor", "|Z1| = 3,703", s->sol[0].x[1], 3.703, 3.7, 1e-10);
        chk_t("CA: motor", S_ANG "Z1 = 45,572995999194296" S_DEG, s->sol[0].x[2], 45.572995999194296, 45.6, 1e-10);
        chk_t("CA: motor", "P(W2) = 10 000 W", s->sol[0].g.lectura[3], 10000.0, 10000.0, 1e-10);
        chk_t("CA: motor", "fp(W1) = 0,95", s->sol[0].g.fp[1], 0.95, 1.0, 1e-10);
    }
    chk_cond("CA: motor", "gasta menos de 1 M operaciones", s->ops <= 1000000ul);
    printf("    el motor: %lu operaciones, %d evaluaciones, %d arranques\n", s->ops, s->evaluaciones, s->arranques);
    tk_motor(&c, -0.95);
    s = tk_calcula(&c);
    chk_cond("CA: motor capacitivo", "una soluci" S_oo "n", s->estado == CAL_OK && s->nsol == 1);
    if (s->nsol >= 1) {
        chk_t("CA: motor capacitivo", "C1 = 811,65300325371227 " S_MU "F", s->sol[0].x[0], 811.65300325371227e-6, 812e-6, 1e-10);
        chk_t("CA: motor capacitivo", "fp(W1) = -0,95", s->sol[0].g.fp[1], -0.95, 1.0, 1e-10);
    }

    /* ---- R1 y R2 [?!] en serie; A1 marca 1 y W1 marca 10 W: lo mismo ---- */
    circ_init(&c, "indet");
    kE = circ_elemento(&c, EL_FUENTE, 0, 1, DIR_ARR);
    pon_val(&c, kE, 10.0); c.el[kE].variante = FORMA_FASOR;
    c.frec.v = 60.0; c.frec.est = M_DATO;
    i = circ_elemento(&c, EL_VATI, 0, 0, DIR_DER);
    c.el[i].val.v = 10.0; c.el[i].val.est = M_DATO;
    i = circ_elemento(&c, EL_AMP, 1, 0, DIR_DER);
    c.el[i].I.v = 1.0; c.el[i].I.est = M_DATO;
    i = circ_elemento(&c, EL_RES, 2, 0, DIR_DER); c.el[i].val.est = M_BUSCAR;
    i = circ_elemento(&c, EL_RES, 3, 0, DIR_ABA); c.el[i].val.est = M_BUSCAR;
    circ_cable(&c, 3, 1, DIR_IZQ); circ_cable(&c, 2, 1, DIR_IZQ); circ_cable(&c, 1, 1, DIR_IZQ);
    s = tk_calcula(&c);
    chk_cond("CA: indeterminado", "CAL_INDET con R1 y R2", s->estado == CAL_INDET && s->nsol == 0 &&
             ((s->elem == 0 && s->iecu == 1) || (s->elem == 1 && s->iecu == 0)));
    tk_msg("CA: indeterminado", &c, s, "no separan R1 y R2");

    /* ---- R3 [?!] en serie con un V ideal: no lleva corriente ---- */
    circ_init(&c, "sin efecto");
    kE = circ_elemento(&c, EL_FUENTE, 0, 1, DIR_ARR);
    pon_val(&c, kE, 10.0); c.el[kE].variante = FORMA_FASOR;
    c.frec.v = 60.0; c.frec.est = M_DATO;
    circ_cable(&c, 0, 0, DIR_DER);
    pon_val(&c, circ_elemento(&c, EL_RES, 1, 0, DIR_ABA), 5.0);
    circ_cable(&c, 1, 1, DIR_IZQ);
    circ_cable(&c, 1, 0, DIR_DER);
    i = circ_elemento(&c, EL_VOLT, 2, 0, DIR_ABA);
    c.el[i].V.v = 10.0; c.el[i].V.est = M_DATO;
    kx = circ_elemento(&c, EL_RES, 2, 1, DIR_ABA);
    c.el[kx].val.est = M_BUSCAR;
    circ_cable(&c, 2, 2, DIR_IZQ); circ_cable(&c, 1, 2, DIR_ARR);
    s = tk_calcula(&c);
    chk_cond("CA: sin efecto", "CAL_SIN_EFECTO con la R del voltímetro", s->estado == CAL_SIN_EFECTO && s->elem == kx && s->nsol == 0);
    {
        char t[40];
        sprintf(t, "%s no lleva corriente", c.el[kx].etiq);
        tk_msg("CA: sin efecto", &c, s, t);
    }

    /* ---- L1 y frec [?!] con solo |V_R|: faltan ---- */
    tk_vr(&c, 120.0, 60.0, EL_BOB, 0.0, 30.0, 72.0, k);
    c.frec.est = M_BUSCAR;
    s = tk_calcula(&c);
    chk_cond("CA: faltan", "CAL_FALTAN: 2 inc" S_oo "gnitas y 1 dato", s->estado == CAL_FALTAN && s->ninc == 2 && s->necu == 1);
    tk_msg("CA: faltan", &c, s, "2 inc" S_oo "gnitas (L1, f) y 1 dato");

    /* ---- Con E2 continua en serie: la misma L y el aviso CC_FUERA ---- */
    tk_vr(&c, 120.0, 60.0, EL_BOB, 0.0, 30.0, 72.0, k);
    i = circ_elemento(&c, EL_FUENTE, 1, 1, DIR_IZQ);
    pon_val(&c, i, 5.0);
    s = tk_calcula(&c);
    chk_cond("CA: CC fuera", "una soluci" S_oo "n y el aviso CC_FUERA", s->estado == CAL_OK && s->nsol == 1 &&
             (s->d.avisos & SIM_AV_CC_FUERA) && (s->sol[0].g.d.avisos & SIM_AV_CC_FUERA));
    if (s->nsol >= 1) chk_t("CA: CC fuera", "la misma L (E2 apagada es un corto)", s->sol[0].x[0], 0.10610329539459689, 0.1061, 1e-12);
    c.el[i].I.v = 3.0; c.el[i].I.est = M_DATO;              /* un dato de CC en E2: no es de la alterna */
    s = tk_calcula(&c);
    chk_cond("CA: CC fuera", "el dato de I(E2) continua no cuenta", s->estado == CAL_OK && s->necu == 1 && s->nsol == 1 &&
             fabs(s->sol[0].x[0] - 0.10610329539459689) <= 1e-12 * 0.1061);

    /* ---- Lo que en CA no se despeja ---- */
    kE = tk_vr(&c, 120.0, 60.0, EL_BOB, 0.0, 30.0, 72.0, k);
    c.el[kE].aux.est = M_BUSCAR;
    s = tk_calcula(&c);
    chk_cond("CA: no se despeja", "la r de E1: CAL_NO_CA", s->estado == CAL_NO_CA && s->elem == kE);
    tk_msg("CA: no se despeja", &c, s, "la r de E1");
    chk_cond("CA: no se despeja", "cal_avanzar no hace nada", cal_avanzar(~0ul) == 0 && cal_progreso() == 1.0);
    tk_vr(&c, 120.0, 60.0, EL_BOB, 0.0, 30.0, 72.0, k);
    c.el[k[2]].val.est = M_BUSCAR;
    s = tk_calcula(&c);
    chk_cond("CA: no se despeja", "la R de V1: CAL_NO_CA", s->estado == CAL_NO_CA && s->elem == k[2]);
    tk_msg("CA: no se despeja", &c, s, "la R de V1");
    kE = tk_vr(&c, 120.0, 60.0, EL_BOB, 0.0, 30.0, 72.0, k);
    c.el[kE].variante = ONDA_SENO;
    c.el[kE].par[Q_FREC - Q_P1].v = 60.0; c.el[kE].par[Q_FREC - Q_P1].est = M_DATO;
    c.frec.est = M_BUSCAR;
    s = tk_calcula(&c);
    chk_cond("CA: no se despeja", "la f con un generador: CAL_NO_CA", s->estado == CAL_NO_CA && s->elem == kE);
    tk_msg("CA: no se despeja", &c, s, "La f la fija E1");

    /* ---- Cinco incógnitas ---- */
    circ_init(&c, "cinco");
    kE = circ_elemento(&c, EL_FUENTE, 0, 1, DIR_ARR);
    pon_val(&c, kE, 10.0); c.el[kE].variante = FORMA_FASOR;
    c.frec.v = 60.0; c.frec.est = M_DATO;
    for (i = 0; i < 4; i++) c.el[circ_elemento(&c, EL_RES, i, 0, DIR_DER)].val.est = M_BUSCAR;
    i = circ_elemento(&c, EL_BOB, 4, 0, DIR_ABA);
    c.el[i].val.est = M_BUSCAR;
    c.el[i].I.v = 1.0; c.el[i].I.est = M_DATO;
    for (i = 4; i > 0; i--) circ_cable(&c, i, 1, DIR_IZQ);
    s = tk_calcula(&c);
    chk_cond("CA: muchas", "CAL_MUCHOS con 5", s->estado == CAL_MUCHOS && s->ninc == 5);
    tk_msg("CA: muchas", &c, s, "aqu" S_ii " hay 5.");

    /* ---- Referencia de fase: solo módulos y E1 [?!] ---- */
    kE = tk_vr(&c, 120.0, 60.0, EL_BOB, 0.10610329539459689, 30.0, 72.0, k);
    c.el[kE].val.est = M_BUSCAR; c.el[kE].val.est_ang = M_BUSCAR;
    s = tk_calcula(&c);
    chk_cond("CA: referencia", "el " S_ANG " de E1 queda en 0" S_DEG " y no cuenta", s->estado == CAL_OK && s->ref_fase == kE &&
             s->ninc == 1 && s->inc[0].que == IN_MODU && s->nsol == 1);
    if (s->nsol >= 1) {
        chk_t("CA: referencia", "|E1| = 120 V", s->sol[0].x[0], 120.0, 120.0, 1e-12);
        chk_cond("CA: referencia", S_ANG "E1 = 0" S_DEG, s->sol[0].g.val_ang[kE] == 0.0);
        c2 = c;
        cal_aplica(s, 0, &c2);
        chk_cond("CA: referencia", "cal_aplica lo deja en 120" S_ANG "0 dato", c2.el[kE].val.est == M_DATO && c2.el[kE].val.est_ang == M_DATO &&
                 c2.el[kE].val.ang == 0.0 && fabs(c2.el[kE].val.v - 120.0) < 1e-12 * 120.0);
    }

    /* ---- Sin incógnitas: una evaluación, y los datos se comprueban ---- */
    tk_vr(&c, 120.0, 60.0, EL_BOB, 0.10610329539459689, 30.0, 72.0, k);
    c.el[k[1]].I.est = M_BUSCAR;                              /* un pedido */
    s = tk_calcula(&c);
    chk_cond("CA: sin inc" S_oo "gnitas", "cuadra: una soluci" S_oo "n con el pedido", s->estado == CAL_OK && s->ninc == 0 && s->nsol == 1 &&
             s->evaluaciones == 1 && fabs(cx_abs(s->sol[0].g.I[k[1]]) * ef - 2.4) < 1e-12);
    c.el[k[2]].V.v = 75.0;
    s = tk_calcula(&c);
    chk_cond("CA: sin inc" S_oo "gnitas", "no cuadra: CAL_INCOMPAT a un 4 %", s->estado == CAL_INCOMPAT && fabs(s->peor - 0.04) < 1e-12);
    tk_msg("CA: sin inc" S_oo "gnitas", &c, s, "V1 queda a un 4 % del dato");

    /* ---- Varias incógnitas que se contradicen ---- */
    tk_vr(&c, 120.0, 60.0, EL_BOB, 0.0, 30.0, 72.0, k);
    c.el[k[1]].val.est = M_BUSCAR;                            /* R1 y L1 */
    c.el[k[1]].I.v = 2.4; c.el[k[1]].I.est = M_DATO;
    c.el[k[0]].V.v = 96.0; c.el[k[0]].V.est = M_DATO;
    c.el[kE].I.v = 3.0; c.el[kE].I.est = M_DATO;              /* 3 A no cuadra con 2,4 */
    s = tk_calcula(&c);
    chk_cond("CA: contradicci" S_oo "n", "CAL_INCOMPAT con varias", s->estado == CAL_INCOMPAT && s->ninc == 2 && s->necu == 4 && s->peor > 0.01);
    tk_msg("CA: contradicci" S_oo "n", &c, s, "Los datos se contradicen");

    /* ---- Mensajes de un error del modelo directo ---- */
    tk_vr(&c, 120.0, 60.0, EL_BOB, 0.0, 30.0, 72.0, k);
    c.frec.est = M_LIBRE;
    s = tk_calcula(&c);
    chk_cond("CA: sin f", "CAL_SIM con SIM_CA_SIN_F", s->estado == CAL_SIM && s->d.estado == SIM_CA_SIN_F);
    tk_msg("CA: sin f", &c, s, "Falta la frecuencia");
    kE = tk_vr(&c, 120.0, 60.0, EL_BOB, 0.0, 30.0, 72.0, k);
    i = circ_elemento(&c, EL_FUENTE, 1, 1, DIR_IZQ);
    pon_val(&c, i, 5.0);
    c.el[i].variante = ONDA_CUADRADA;
    c.el[i].par[Q_FREC - Q_P1].v = 60.0; c.el[i].par[Q_FREC - Q_P1].est = M_DATO;
    s = tk_calcula(&c);
    chk_cond("CA: cuadrada", "CAL_SIM con SIM_CA_FORMA", s->estado == CAL_SIM && s->d.estado == SIM_CA_FORMA && s->d.elem == i);
    tk_msg("CA: cuadrada", &c, s, "E2 es cuadrada");

    /* ---- Los modelos de la biblioteca con la clave 'c' (el valor que halla
     *      la calculadora de CA en clave_el; [53] los deja para aquí) ---- */
    {
        int nmod = 0;
        for (i = 0; i < N_MODELOS; i++) {
            const CircModelo *md = &MODELOS[i];
            char q[48];
            int j, e2, k2 = -1;
            if (md->medida != 'c') continue;
            nmod++;
            sprintf(q, "modelo %d", i + 1);
            modelo_cargar(i, &c);
            s = tk_calcula(&c);
            for (e2 = 0; e2 < c.nel; e2++) if (md->clave_el && strcmp(c.el[e2].etiq, md->clave_el) == 0) break;
            for (j = 0; j < s->ninc && j < CAL_MAX_INC; j++) if (s->inc[j].el == e2 && s->inc[j].que == IN_VAL) k2 = j;
            chk_cond(q, "la calculadora de CA lo resuelve: una soluci" S_oo "n", s->estado == CAL_OK && s->nsol == 1 && k2 >= 0);
            if (s->nsol >= 1 && k2 >= 0) chk_t(q, "su clave", s->sol[0].x[k2], md->clave, fabs(md->clave), md->tol);
        }
        if (nmod) printf("    %d modelos de la biblioteca resueltos con la calculadora\n", nmod);
    }

    /* ---- Por trozos: 1, 997 e ilimitadas operaciones por llamada ---- */
    tk_rlc_f(&c);
    r1 = *tk_calcula(&c);
    llamadas = tk_trozos(&c, 1);
    r2 = *cal_res();
    chk_cond("por trozos", "RLC con f: 1 operaci" S_oo "n por llamada, id" S_ee "ntico", tk_iguales(&r1, &r2) && llamadas > 100);
    tk_trozos(&c, 997);
    r2 = *cal_res();
    chk_cond("por trozos", "RLC con f: 997 por llamada, id" S_ee "ntico", tk_iguales(&r1, &r2));
    tk_motor(&c, 0.95);
    r1 = *tk_calcula(&c);
    llamadas = tk_trozos(&c, 1);
    r2 = *cal_res();
    chk_cond("por trozos", "motor: 1 operaci" S_oo "n por llamada, id" S_ee "ntico", tk_iguales(&r1, &r2) && llamadas > 100);
    tk_trozos(&c, 997);
    r2 = *cal_res();
    chk_cond("por trozos", "motor: 997 por llamada, id" S_ee "ntico", tk_iguales(&r1, &r2));
    /* el progreso sube hasta 1 */
    {
        double p = 0.0, ant = 0.0;
        int sube = 1;
        tk_rlc_f(&c);
        red_extraer(&c, &tk_red);
        cal_iniciar(&c, &tk_red);
        while (cal_avanzar(997)) { p = cal_progreso(); if (p < ant || p > 1.0) sube = 0; ant = p; }
        chk_cond("por trozos", "el progreso sube y acaba en 1", sube && cal_progreso() == 1.0);
    }
    /* cortar: lo hallado se queda */
    tk_rlc_f(&c);
    red_extraer(&c, &tk_red);
    cal_iniciar(&c, &tk_red);
    while (cal_res()->nsol == 0 && cal_avanzar(997)) {}
    cal_cortar();
    s = cal_res();
    chk_cond("cortar", "CAL_CORTADO con lo hallado", s->estado == CAL_CORTADO && s->nsol == 1 &&
             (fabs(s->sol[0].x[0] - 1459.6743686165150) <= 1e-11 * 1459.7 || fabs(s->sol[0].x[0] - 1735.3388163274111) <= 1e-11 * 1735.3));
    chk_cond("cortar", "ya no avanza", cal_avanzar(~0ul) == 0);
    tk_msg("cortar", &c, s, "Cortado");
    c2 = c;
    cal_aplica(s, 0, &c2);
    chk_cond("cortar", "lo hallado se aplica", c2.frec.est == M_DATO && fabs(c2.frec.v - s->sol[0].x[0]) == 0.0);

    /* ---- El jacobiano exacto frente a diferencias centradas. La referencia
     *      es la diferencia centrada con extrapolación de Richardson (h y 2h
     *      con h = 1e-4 en u): con h = 1e-6 el redondeo de la propia
     *      referencia llega a 2e-6 en redes mal condicionadas ---- */
    {
        static Cx V0[MAX_EL], I0[MAX_EL], dV[MAX_EL], dI[MAX_EL], fV1[MAX_EL], fI1[MAX_EL], fV2[MAX_EL], fI2[MAX_EL];
        static SimRed base;
        int caso, probados = 0, malos = 0, m, que, kk, n, intentos, tipos[5] = { 0, 0, 0, 0, 0 };
        double peor = 0.0;
        const double h = 1e-4;
        for (caso = 0; caso < 400 && probados < 200; caso++) {
            double f = pow(10.0, 1.0 + 4.0 * tk_u()), escV = 0.0, escI = 0.0;
            int mal = 0;
            tk_red_jac(f);
            base = sr;
            kk = -1; que = IN_F;
            if (tk_u() < 0.8)
                for (intentos = 0; intentos < 20; intentos++) {
                    int ke = tk_n(base.nel);
                    const SimEl *el = &base.el[ke];
                    if (el->instr) continue;
                    if (el->tipo == SE_R || el->tipo == SE_C || el->tipo == SE_L) { kk = ke; que = IN_VAL; break; }
                    if (el->tipo == SE_Z) { kk = ke; que = tk_u() < 0.5 ? IN_VAL : IN_ANGZ; break; }
                    if (el->tipo == SE_E || el->tipo == SE_J) { kk = ke; que = tk_u() < 0.5 ? IN_MODU : IN_ANGU; break; }
                }
            if (kk < 0) que = IN_F;
            if (!tk_dif(&base, f, kk, que, h, fV1, fI1) || !tk_dif(&base, f, kk, que, 2.0 * h, fV2, fI2)) continue;
            if (!tk_vi(&base, f, V0, I0)) continue;               /* la última: su LU se queda */
            n = ca_n();
            ca_derivada(&tk_s, &tk_exc, tk_x, f, kk, que, tk_v);
            for (m = 0; m < n; m++) tk_v[m] = cx(-tk_v[m].re, -tk_v[m].im);
            ca_sustituye(tk_v, tk_dx);
            for (m = 0; m < tk_s.nel; m++) {
                ca_dvi(&tk_s, &tk_exc, tk_x, tk_dx, f, m, kk, que, &dV[m], &dI[m]);
                if (cx_abs(V0[m]) > escV) escV = cx_abs(V0[m]);
                if (cx_abs(I0[m]) > escI) escI = cx_abs(I0[m]);
            }
            for (m = 0; m < tk_s.nel; m++) {
                Cx rV = cx_esc(cx_menos(cx_esc(fV1[m], 4.0), fV2[m]), 1.0 / 3.0);   /* Richardson */
                Cx rI = cx_esc(cx_menos(cx_esc(fI1[m], 4.0), fI2[m]), 1.0 / 3.0);
                double eV = cx_abs(cx_menos(dV[m], rV)) / (cx_abs(dV[m]) + 1e-3 * escV + 1e-300);
                double eI = cx_abs(cx_menos(dI[m], rI)) / (cx_abs(dI[m]) + 1e-3 * escI + 1e-300);
                if (eV > peor) peor = eV;
                if (eI > peor) peor = eI;
                if (!(eV <= 1e-6 && eI <= 1e-6)) mal = 1;
            }
            probados++;
            tipos[que]++;
            if (mal) {
                malos++;
                if (malos <= 3) printf("    jacobiano: caso %d, inc" S_oo "gnita %d del elemento %d\n", caso, que, kk);
            }
        }
        printf("    jacobiano exacto frente a diferencias centradas: %d problemas (peor %.1e; valor %d, " S_ANG "Z %d, |U| %d, " S_ANG "U %d, f %d)\n",
               probados, peor, tipos[IN_VAL], tipos[IN_ANGZ], tipos[IN_MODU], tipos[IN_ANGU], tipos[IN_F]);
        chk_cond("jacobiano", "el exacto = diferencias centradas (1e-6)", malos == 0);
        chk_cond("jacobiano", "200 problemas, con todas las clases de inc" S_oo "gnita",
                 probados >= 200 && tipos[IN_VAL] >= 10 && tipos[IN_ANGZ] >= 10 && tipos[IN_MODU] >= 10 && tipos[IN_ANGU] >= 10 && tipos[IN_F] >= 10);
    }

    /* ---- Inverso por construcción: 300 redes RLC al azar ---- */
    {
        static CaRes directo, re;
        int caso, probados = 0, malos = 0, saltados = 0, n_mod = 0, n_fasor = 0, n_w = 0, clases[5] = { 0, 0, 0, 0, 0 };
        for (caso = 0; caso < 900 && probados < 300; caso++) {
            double f = pow(10.0, 1.0 + 4.0 * tk_u()), w = 2.0 * PI_T * f, oculto, d, ang, dm, am, sens;
            int nn = 3 + tk_n(4), n, kf, kh, kd, qd, kw = -1, que, clase, j, esta, mal = 0, extras;
            circ_init(&tk_c, "inverso");
            memset(&tk_r, 0, sizeof tk_r);
            kf = tk_el(EL_FUENTE, 0, 1, 1.0 + 99.0 * tk_u());
            tk_c.el[kf].variante = FORMA_FASOR;
            tk_c.el[kf].val.ang = 360.0 * tk_u() - 180.0; tk_c.el[kf].val.est_ang = M_DATO;
            tk_c.frec.v = f; tk_c.frec.est = M_DATO;
            /* un árbol desde los bornes de E1 y una segunda rama por nodo: sin
             * ramas muertas (con E1 ideal, un árbol colgado de ella no tendría
             * lazos) */
            for (n = 2; n < nn; n++) {
                int a = tk_n(n), b = tk_n(n - 1);
                if (b >= a) b++;
                tk_rlc(a, n, w);
                tk_rlc(n, b, w);
            }
            if (nn == 2 || tk_u() < 0.3) tk_rlc(1, 0, w);
            extras = tk_n(3);
            for (n = 0; n < extras; n++) {
                int a = tk_n(nn), b = tk_n(nn);
                if (a != b) tk_rlc(a, b, w);
            }
            tk_nn = nn;
            /* la incógnita y el dato */
            j = tk_n(10);
            if (j < 2) { kh = kf; que = IN_MODU; }
            else if (j < 4) { kh = kf; que = IN_ANGU; }
            else { kh = 1 + tk_n(tk_c.nel - 1); que = IN_VAL; }
            kd = 1 + tk_n(tk_c.nel - 1);
            qd = tk_u() < 0.5 ? Q_V : Q_I;
            j = tk_n(10);
            clase = que == IN_ANGU ? 1 : j < 6 ? 0 : j < 8 ? 1 : 2;        /* módulo, fasor o P de un W */
            if (clase == 2) {                                              /* el W, en serie con kd */
                kw = tk_el(EL_VATI, tk_nn, tk_r.b[kd], 0.0);
                tk_r.b[kd] = tk_nn++;
            }
            tk_cierra();
            if (ca_analiza(&tk_c, &tk_r, &directo) != SIM_OK) { saltados++; continue; }
            d = tk_dato_de(&directo, kd, qd, kw, &ang);
            if (!(d > 1e-9)) { saltados++; continue; }
            oculto = que == IN_ANGU ? tk_c.el[kh].val.ang : tk_c.el[kh].val.v;
            /* ¿el dato ve la incógnita? (si no, no se puede hallar) */
            tk_copia = tk_c;
            if (que == IN_ANGU) tk_copia.el[kh].val.ang += 1e-4 * 180.0 / PI_T;
            else tk_copia.el[kh].val.v *= 1.0 + 1e-4;
            if (ca_analiza(&tk_copia, &tk_r, &re) != SIM_OK) { saltados++; continue; }
            dm = tk_dato_de(&re, kd, qd, kw, &am);
            sens = fabs(log(dm / d)) / 1e-4;
            if (clase == 1) { double sa = fabs(cx_envuelve((am - ang) * PI_T / 180.0)) / 1e-4; if (sa > sens) sens = sa; }
            if (!(sens > 1e-3)) { saltados++; continue; }
            /* el problema */
            if (kw >= 0) { tk_c.el[kw].val.v = d; tk_c.el[kw].val.est = M_DATO; }
            else {
                Mag *mg = circ_mag(&tk_c.el[kd], qd);
                mg->v = d; mg->est = M_DATO;
                if (clase == 1) { mg->ang = ang; mg->est_ang = M_DATO; }
            }
            if (que == IN_ANGU) tk_c.el[kh].val.est_ang = M_BUSCAR;
            else tk_c.el[kh].val.est = M_BUSCAR;
            cal_iniciar(&tk_c, &tk_r);
            while (cal_avanzar(~0ul)) {}
            s = cal_res();
            if (s->ops > tk_max_ops) tk_max_ops = s->ops;
            probados++;
            clases[que]++;
            if (clase == 0) n_mod++; else if (clase == 1) n_fasor++; else n_w++;
            if (s->estado != CAL_OK || s->nsol < 1) mal = 1;
            esta = 0;
            for (j = 0; !mal && j < s->nsol; j++) {
                double x = s->sol[j].x[0], dd, aa;
                if (que == IN_ANGU) { if (fabs(cx_envuelve((x - oculto) * PI_T / 180.0)) <= 1e-7) esta = 1; }
                else if (fabs(x - oculto) <= 1e-7 * oculto) esta = 1;
                /* cada solución cumple los datos: con ella de dato, el análisis directo */
                tk_copia = tk_c;
                cal_aplica(s, j, &tk_copia);
                if (ca_analiza(&tk_copia, &tk_r, &re) != SIM_OK) { mal = 1; break; }
                dd = tk_dato_de(&re, kd, qd, kw, &aa);
                if (!(fabs(dd / d - 1.0) <= 1e-9)) mal = 1;
                if (clase == 1 && !(fabs(cx_envuelve((aa - ang) * PI_T / 180.0)) <= 1e-9)) mal = 1;
            }
            if (!esta) mal = 1;
            if (mal) {
                malos++;
                if (malos <= 3) {
                    char m[300];
                    cal_mensaje(&tk_c, s, m, sizeof m);
                    printf("    inverso %d: incógnita %d de %s (%.12g), dato %d de %s (clase %d): estado %d, %d soluciones (%.12g) %s\n",
                           caso, que, tk_c.el[kh].etiq, oculto, qd, tk_c.el[kd].etiq, clase, s->estado, s->nsol,
                           s->nsol ? s->sol[0].x[0] : 0.0, m);
                }
            }
        }
        printf("    inverso por construcci" S_oo "n: %d redes (%d saltadas; m" S_oo "dulo %d, fasor %d, P %d; valor %d, |E| %d, " S_ANG "E %d)\n",
               probados, saltados, n_mod, n_fasor, n_w, clases[IN_VAL], clases[IN_MODU], clases[IN_ANGU]);
        chk_cond("inverso", "el valor oculto est" S_aa " entre las soluciones y todas cumplen los datos", malos == 0);
        chk_cond("inverso", "300 redes, con m" S_oo "dulos, fasores y vat" S_ii "metros", probados >= 300 && n_mod >= 50 && n_fasor >= 50 && n_w >= 20);
    }

    /* ---- La calculadora de CA = la tabla: 300 problemas resistivos ---- */
    {
        static Solucion s0, s1;
        int caso, comparados = 0, distintos = 0, saltados = 0, dobles = 0;
        for (caso = 0; caso < 900 && comparados < 300; caso++) {
            int kf, kh, kd, qd, i2, j2, ok;
            double d;
            circ_init(&tk_c, "tabla");
            memset(&tk_r, 0, sizeof tk_r);
            tk_nn = 2;
            kf = tk_el(EL_FUENTE, 0, 1, 1.0 + (double)tk_n(240));
            if (caso % 3 == 0) {
                /* un puente de Wheatstone (1 = +, 2 y 3 los puntos medios) con el
                 * dato en su rama central: cruza por cero al equilibrarse, y
                 * |dato| suele tener una solución a cada lado */
                tk_nn = 4;
                for (i2 = 0; i2 < 5; i2++) {
                    static const int A[5] = { 1, 2, 1, 3, 2 }, B[5] = { 2, 0, 3, 0, 3 };
                    tk_el(EL_RES, A[i2], B[i2], 0.5 * (double)(1 + tk_n(200)));
                }
            } else {
                tk_sp(1, 0, 0);
                if (tk_u() < 0.4 && tk_nn > 2) {                         /* un puente */
                    int a = tk_n(tk_nn), b = tk_n(tk_nn);
                    if (a != b) tk_el(EL_RES, a, b, 0.5 * (double)(1 + tk_n(200)));
                }
            }
            tk_cierra();
            resolver(&tk_c, &tk_r, &s0);
            if (s0.estado != SOL_OK || tk_c.nel < 3) { saltados++; continue; }
            kh = 1 + tk_n(tk_c.nel - 1);
            kd = caso % 3 == 0 ? 5 : 1 + tk_n(tk_c.nel - 1);
            qd = tk_u() < 0.5 ? Q_V : Q_I;
            d = fabs(qd == Q_V ? s0.s[0].V[kd] : s0.s[0].I[kd]);
            if (!(d > 1e-6 * tk_c.el[kf].val.v) || !(fabs(s0.s[0].I[kh]) > 1e-6 * fabs(s0.s[0].I[kf]))) { saltados++; continue; }
            tk_c.el[kh].val.est = M_BUSCAR;
            circ_mag(&tk_c.el[kd], qd)->v = d;
            circ_mag(&tk_c.el[kd], qd)->est = M_DATO;
            resolver(&tk_c, &tk_r, &s1);
            if (s1.estado != SOL_OK) { saltados++; continue; }
            /* la misma, con la fuente en alterna (ángulo 0, 60 Hz) */
            tk_c.el[kf].variante = FORMA_FASOR;
            tk_c.el[kf].val.ang = 0.0; tk_c.el[kf].val.est_ang = M_DATO;
            tk_c.frec.v = 60.0; tk_c.frec.est = M_DATO;
            cal_iniciar(&tk_c, &tk_r);
            while (cal_avanzar(~0ul)) {}
            s = cal_res();
            if (s->ops > tk_max_ops) tk_max_ops = s->ops;
            comparados++;
            if (s1.nsol > 1) dobles++;
            ok = s->estado == CAL_OK && s->nsol == s1.nsol;
            for (i2 = 0; ok && i2 < s1.nsol; i2++) {
                double R = s1.s[i2].val[kh];
                int hallada = 0;
                for (j2 = 0; j2 < s->nsol; j2++) if (fabs(s->sol[j2].x[0] - R) <= 1e-9 * R) hallada = 1;
                if (!hallada) ok = 0;
            }
            if (!ok) {
                distintos++;
                if (distintos <= 3)
                    printf("    tabla %d: %s oculta, dato %c(%s) = %.12g: la tabla da %d (%.12g, %.12g), la CA estado %d con %d (%.12g, %.12g)\n",
                           caso, tk_c.el[kh].etiq, qd == Q_V ? 'V' : 'I', tk_c.el[kd].etiq, d, s1.nsol, s1.s[0].val[kh],
                           s1.nsol > 1 ? s1.s[1].val[kh] : 0.0, s->estado, s->nsol, s->nsol ? s->sol[0].x[0] : 0.0,
                           s->nsol > 1 ? s->sol[1].x[0] : 0.0);
            }
        }
        printf("    la calculadora de CA = la tabla: %d problemas (%d con dos soluciones; %d saltados)\n", comparados, dobles, saltados);
        chk_cond("CA = tabla", "el mismo conjunto de R (1e-9)", distintos == 0);
        chk_cond("CA = tabla", "300 problemas, 30 con dos soluciones", comparados >= 300 && dobles >= 30);
    }

    /* ---- Todos los mensajes, con nombres largos: 4 líneas de 52 ---- */
    {
        static CalRes m;
        static const int ES[] = { CAL_SIM, CAL_FALTAN, CAL_MUCHOS, CAL_NO_CA, CAL_INDET, CAL_INCOMPAT, CAL_BORDE, CAL_SIN_EFECTO, CAL_CORTADO };
        static const int EC[] = { EC_MOD, EC_ANG, EC_RE, EC_P, EC_FP };
        static const int SIMS[] = { SIM_FALTA, SIM_LAZO_V, SIM_CORTE_J, SIM_CA_FORMA, SIM_CA_FRECS, SIM_CA_SIN_F, SIM_RESONANCIA, SIM_SINGULAR };
        int e, q2, ni, kz, kj, kw, kv, malos = 0, n = 0, kg;
        char txt[400];
        /* un circuito con etiquetas de dos cifras */
        circ_init(&c, "largo");
        for (i = 0; i < 12; i++) circ_elemento(&c, EL_RES, i % 9, i / 9, DIR_DER);
        kE = circ_elemento(&c, EL_FUENTE, 0, 3, DIR_DER);
        kz = circ_elemento(&c, EL_IMP, 1, 3, DIR_DER);
        kj = circ_elemento(&c, EL_CORR, 2, 3, DIR_DER);
        kw = circ_elemento(&c, EL_VATI, 3, 3, DIR_DER);
        kv = circ_elemento(&c, EL_VOLT, 4, 3, DIR_DER);
        kg = circ_elemento(&c, EL_FUENTE, 5, 3, DIR_DER);
        c.el[kg].variante = ONDA_SENO;
        c.el[kE].variante = FORMA_FASOR;
        for (i = 0; i < c.nel; i++) {                      /* R10, E10, Z10, J10, W10, V10, E11 */
            if (c.el[i].tipo == EL_RES) continue;
            c.el[i].etiq[1] = '1'; c.el[i].etiq[2] = (char)('0' + (i == kg)); c.el[i].etiq[3] = 0;
        }
        for (e = 0; e < (int)(sizeof ES / sizeof ES[0]); e++)
            for (q2 = 0; q2 < (int)(sizeof EC / sizeof EC[0]); q2++)
                for (ni = 0; ni <= CAL_MAX_INC; ni++) {
                    memset(&m, 0, sizeof m);
                    m.estado = ES[e];
                    m.ref_fase = -1;
                    m.ninc = ni == 0 ? 0 : ni;
                    m.necu = ni == 0 ? 3 : (q2 == 0 ? 1 : ni - 1);
                    m.inc[0].que = IN_VAL;  m.inc[0].el = (signed char)kz;
                    m.inc[1].que = IN_ANGZ; m.inc[1].el = (signed char)kz;
                    m.inc[2].que = IN_MODU; m.inc[2].el = (signed char)kE;
                    m.inc[3].que = IN_ANGU; m.inc[3].el = (signed char)kE;
                    m.ecu[0].que = (unsigned char)EC[q2];
                    m.ecu[0].q = (unsigned char)(EC[q2] == EC_P || EC[q2] == EC_FP ? 0 : Q_V);
                    m.ecu[0].el = (signed char)(EC[q2] == EC_P || EC[q2] == EC_FP ? kw : 11);
                    m.ecu[0].v = EC[q2] == EC_MOD ? 123456.7 : EC[q2] == EC_ANG ? -2.5 : EC[q2] == EC_P ? 98765.4 : -0.3;
                    m.ecu[0].esc = 1.0;
                    m.ecu[1] = m.ecu[0]; m.ecu[1].que = EC_IM;
                    m.iecu = 0;
                    m.elem = ES[e] == CAL_NO_CA ? (ni % 3 == 0 ? kE : ni % 3 == 1 ? kv : kg) :
                             ES[e] == CAL_SIN_EFECTO ? (ni % 3 == 0 ? 11 : ni % 3 == 1 ? kj : -1) :
                             ES[e] == CAL_INDET ? 1 : ES[e] == CAL_BORDE ? 2 : kz;
                    if (ES[e] == CAL_INDET) m.iecu = 3;
                    if (ES[e] == CAL_MUCHOS) { m.ninc = ni + 5; m.necu = 12; }
                    m.lo_dato = 1.234567e-3; m.hi_dato = 987654.3;
                    m.peor = ES[e] == CAL_BORDE ? (ni & 1 ? 1.0 : -1.0) : 0.0123456;
                    m.d.estado = SIMS[(e + q2 + ni) % (int)(sizeof SIMS / sizeof SIMS[0])];
                    m.d.elem = kg; m.d.campo = Q_FREC; m.d.lazo = (1u << kE) | (1u << kg); m.d.t = 1591.55;
                    cal_mensaje(&c, &m, txt, sizeof txt);
                    n++;
                    if (lineas(txt, 52) > 4 || strlen(txt) >= sizeof txt - 1) {
                        malos++;
                        if (malos <= 3) printf("    mensaje largo (%d l" S_ii "neas): %s\n", lineas(txt, 52), txt);
                    }
                }
        chk_cond("mensajes", "todos caben en 4 l" S_ii "neas de 52", malos == 0 && n > 200);
    }

    chk_cond("presupuesto", "ning" S_uu "n problema pasa de CAL_OPS_TOPE", tk_max_ops < CAL_OPS_TOPE);
    printf("    el mayor gasto: %lu operaciones (%.2f s en la calculadora)\n", tk_max_ops, (double)tk_max_ops * CA_NS_OP * 1e-9);
}
