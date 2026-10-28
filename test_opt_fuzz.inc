/* ============================================================================
 *  test_opt_fuzz.inc  -  Secuencias al azar de la API del motor ([75i] I19)
 *
 *  El arnés diferencial de la revisión del grupo motor (rev9), para el
 *  arnés: llamadas públicas del motor al azar (emulaciones a trozos, OP,
 *  punto Q, barrido de CC con continuación, derivada, topología de CA,
 *  Thévenin, modos) sobre redes al azar, con la huella de lo que devuelve
 *  cada llamada y sim_ops(). fz_corre da la huella de toda la secuencia: la
 *  misma semilla con el estado de Newton de antes del paso guardado entero
 *  (sim_bak_entero(1), lo de antes de rev9) y a medias (lo de ahora) tiene
 *  que salir igual. La semilla 476 llama a sim_op_red de otras redes con una
 *  emulación no lineal a medias y sigue: con localiza_nl reponiendo entero lo
 *  que un_paso_nl guardó a medias acababa en SIM_SINGULAR (la revisión).
 * ========================================================================= */
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

static unsigned long long rf_s;
static unsigned rf_u(unsigned n) { rf_s ^= rf_s << 13; rf_s ^= rf_s >> 7; rf_s ^= rf_s << 17; return (unsigned)((rf_s >> 11) % n); }
static double rf_d(double a, double b) { return a + (b - a) * (double)rf_u(1000001) / 1e6; }
static unsigned long long rf_h, rf_total;
static void rf_mez(const void *p, size_t n) { rf_h = huella_fnv(rf_h, p, n); }
static void rf_mezd(double x) { rf_mez(&x, sizeof x); }
static void rf_mezi(long x) { rf_mez(&x, sizeof x); }
/* Cada llamada: su resultado, su huella y sim_ops, en la de toda la secuencia */
static void rf_linea(int r)
{
    unsigned long o = sim_ops();
    rf_mezi(r);
    rf_mez(&o, sizeof o);
    rf_total = huella_fnv(rf_total, &rf_h, sizeof rf_h);
    rf_h = 1469598103934665603ULL;
}

static SimEl *rf_el(SimRed *s, int tipo, int a, int b, double val)
{
    SimEl *e = &s->el[s->nel++];
    memset(e, 0, sizeof *e);
    e->tipo = (unsigned char)tipo; e->a = (short)a; e->b = (short)b; e->val = val;
    e->k = -1; e->t1 = e->t2 = -1.0; e->c = e->nref = -1; e->ctl = -1;
    return e;
}

static void rf_fuente(SimEl *e, double esc)
{
    switch (rf_u(7)) {
    case 0: case 1: e->forma = ONDA_CC; e->p[0] = esc * rf_d(-10, 10); if (rf_u(8) == 0) e->p[0] = rf_u(2) ? 0.0 : -0.0; break;
    case 2: e->forma = ONDA_SENO; e->p[0] = esc * rf_d(0.1, 5); e->p[1] = rf_d(200, 5000); e->p[2] = rf_d(-90, 90); e->p[3] = esc * rf_d(-2, 2); break;
    case 3: e->forma = ONDA_CUADRADA; e->p[0] = esc * rf_d(0.1, 5); e->p[1] = rf_d(200, 5000); e->p[2] = rf_u(2) ? 50.0 : rf_d(10, 90); e->p[3] = esc * rf_d(-1, 1); break;
    case 4: e->forma = ONDA_TRIANGULAR; e->p[0] = esc * rf_d(0.1, 5); e->p[1] = rf_d(200, 5000); e->p[2] = rf_u(3) ? 50.0 : rf_u(2) ? 0.0 : 100.0; e->p[3] = 0.0; break;
    default:
        e->forma = ONDA_PULSO; e->p[0] = esc * rf_d(-1, 1); e->p[1] = esc * rf_d(-5, 5);
        e->p[2] = rf_u(2) ? 0.0 : rf_d(0, 1e-3); e->p[3] = rf_u(2) ? 0.0 : rf_d(1e-4, 5e-4); e->p[4] = rf_u(2) ? 0.0 : rf_d(6e-4, 2e-3);
        break;
    }
}

/* Una red al azar: nl = 0 lineal; 1 con D, AO, BJT o MOS */
static void rf_red(SimRed *s, int nl)
{
    int grande = rf_u(3) == 0, nn = grande ? 6 + (int)rf_u(9) : 2 + (int)rf_u(6), n, i, ne;
    memset(s, 0, sizeof *s);
    s->nn = nn; s->ref[0] = 1;
    for (n = 1; n < nn; n++) {
        if (n > 1 && rf_u(7) == 0) continue;                  /* un trozo que flota */
        rf_el(s, SE_R, (int)rf_u((unsigned)n), n, pow(10.0, rf_d(1, 5)));
    }
    ne = grande ? 6 + (int)rf_u(12) : 1 + (int)rf_u(6);
    for (i = 0; i < ne && s->nel < MAX_EL - 6; i++) {
        int a = (int)rf_u((unsigned)nn), b = (int)rf_u((unsigned)nn);
        SimEl *e;
        double v;
        if (a == b) b = (a + 1) % nn;
        switch (rf_u(15)) {
        case 11: case 12:                                      /* una controlada por otro elemento */
            if (!s->nel) break;
            {
                int ctl = (int)rf_u((unsigned)s->nel), t = s->el[ctl].tipo, ci = (int)rf_u(2);
                if (t == SE_JC || t == SE_D || t == SE_AO) ci = 0;
                e = rf_el(s, rf_u(2) ? SE_EC : SE_JC, a, b, 0.0);
                e->val = rf_d(-3, 3) * (e->tipo == SE_EC ? (ci ? 1e3 : 1.0) : (ci ? 1.0 : 1e-3));
                e->ctl = (signed char)ctl; e->ctl_i = (unsigned char)ci;
            }
            break;
        case 13:                                               /* un instrumento */
            e = rf_el(s, rf_u(2) ? SE_VMI : SE_AMI, a, b, 0.0);
            e->instr = e->tipo == SE_VMI ? 'V' : rf_u(3) ? 'A' : 'W';
            break;
        case 14:                                               /* rígido: C pequeño contra R grande */
            rf_el(s, SE_C, a, b, 1e-12);
            rf_el(s, SE_R, a, b, 1e6);
            break;
        case 0: rf_el(s, SE_R, a, b, pow(10.0, rf_d(0, 5))); break;
        case 1: e = rf_el(s, SE_C, a, b, pow(10.0, rf_d(-9, -5))); if (rf_u(3) == 0) { e->ci = 1; e->x0 = rf_u(2) ? 0.0 : rf_d(-3, 3); } break;
        case 2: e = rf_el(s, SE_L, a, b, pow(10.0, rf_d(-4, -1))); if (rf_u(3) == 0) { e->ci = 1; e->x0 = rf_d(-0.01, 0.01); } break;
        case 3: case 4: e = rf_el(s, SE_E, a, b, rf_u(3) ? 0.0 : pow(10.0, rf_d(-1, 2))); rf_fuente(e, 1.0); break;
        case 5: e = rf_el(s, SE_J, a, b, 0.0); rf_fuente(e, 1e-3); break;
        case 6:
            e = rf_el(s, SE_S, a, b, 0.0); e->cerrado0 = (unsigned char)rf_u(2);
            e->t1 = rf_u(2) ? rf_d(1e-4, 2e-3) : -1.0; e->t2 = rf_u(2) ? rf_d(2e-3, 4e-3) : -1.0;
            break;
        case 7:                                                /* un lazo E-C (cuadra o no) */
            e = rf_el(s, SE_E, a, b, 0.0); e->forma = ONDA_CC; v = rf_d(-5, 5); e->p[0] = v;
            e = rf_el(s, SE_C, a, b, 1e-6); e->ci = 1; e->x0 = rf_u(2) ? -v : rf_d(-5, 5);
            break;
        case 8:                                                /* E en paralelo con L (un lazo en CC) */
            e = rf_el(s, SE_E, a, b, 0.0); rf_fuente(e, 1.0); if (rf_u(2)) { e->forma = ONDA_CC; e->p[0] = rf_u(2) ? 0.0 : rf_d(-3, 3); }
            e = rf_el(s, SE_L, a, b, 1e-3); if (rf_u(2)) { e->ci = 1; e->x0 = 0.0; }
            break;
        case 9:                                                /* J hacia un nodo nuevo sin más (flota) */
            if (s->nn < MAX_NODOS - 1) { e = rf_el(s, SE_J, a, s->nn, 0.0); rf_fuente(e, 1e-3); s->nn++; e = rf_el(s, SE_C, s->nn - 1, 0, 1e-6); if (rf_u(2)) { e->ci = 1; e->x0 = 1.0; } }
            break;
        default: e = rf_el(s, SE_E, a, b, 0.0); e->forma = ONDA_CC; e->p[0] = rf_d(-5, 5); break;
        }
    }
    if (nl) {
        int q, nq = 1 + (int)rf_u(2);
        for (q = 0; q < nq && s->nel < MAX_EL - 3; q++) {
            int a = (int)rf_u((unsigned)s->nn), b = (int)rf_u((unsigned)s->nn);
            SimEl *e;
            if (a == b) b = (a + 1) % s->nn;
            switch (rf_u(5)) {
            case 3: {                                          /* BJT */
                int nb = (int)rf_u((unsigned)s->nn);
                e = rf_el(s, SE_BJT, a, b, 0.0);
                e->c = (short)nb; e->modelo = NL_BJT; e->pol = rf_u(2) ? 1 : -1;
                e->m[0] = 1e-15; e->m[1] = rf_d(50, 200); e->m[2] = 1.0; e->m[3] = rf_u(2) ? 0.0 : 50.0;
                e->m[5] = nl_vcrit(1e-15, NL_VT);
                break;
            }
            case 4: {                                          /* MOS */
                int ng = (int)rf_u((unsigned)s->nn);
                e = rf_el(s, SE_MOS, a, b, 0.0);
                e->c = (short)ng; e->modelo = NL_MOS; e->pol = rf_u(2) ? 1 : -1;
                e->m[0] = rf_d(0.5, 2); e->m[1] = rf_d(1e-4, 1e-2); e->m[2] = rf_u(2) ? 0.0 : 40.0;
                break;
            }
            case 0:
                e = rf_el(s, SE_D, a, b, 0.0); e->modelo = NL_TRAMOS;
                e->m[0] = rf_d(0.3, 0.8); e->m[1] = rf_u(2) ? NL_RON : rf_d(0.1, 10); e->m[3] = NL_RON;
                if (rf_u(3) == 0) e->m[2] = rf_d(3, 6);
                break;
            case 1:
                e = rf_el(s, SE_D, a, b, 0.0); e->modelo = NL_UNION;
                e->m[0] = NL_1N4148.Is; e->m[1] = NL_1N4148.n; e->m[2] = NL_1N4148.Rs; e->m[5] = nl_vcrit(NL_1N4148.Is, NL_1N4148.n * NL_VT);
                break;
            default: {                                         /* AO con realimentación */
                int sal = (int)rf_u((unsigned)s->nn), men = (int)rf_u((unsigned)s->nn), mas = (int)rf_u((unsigned)s->nn), real = (int)rf_u(2);
                if (sal == 0) sal = 1;
                e = rf_el(s, SE_AO, mas, men, 0.0);
                e->c = (short)sal; e->nref = 0;
                e->modelo = real ? NL_AO_REAL : NL_AO_IDEAL;
                e->m[0] = real ? pow(10.0, rf_d(3, 5)) : 0.0; e->m[1] = 15.0; e->m[2] = -15.0;
                if (real) { e->m[3] = rf_u(2) ? 0.0 : 1e6; e->m[4] = rf_u(2) ? 0.0 : 75.0; }
                if (men != sal) rf_el(s, SE_R, sal, men, pow(10.0, rf_d(3, 5)));
                break;
            }
            }
        }
    }
}

static void rf_valores(const Valores *v) { rf_mez(v, sizeof *v); }
static void rf_diag(const SimDiag *d) { rf_mez(d, sizeof *d); }
static void rf_red_act(void)
{
    const SimRed *s = sim_red();
    int k;
    rf_mezi(s->nn); rf_mezi(s->nel);
    rf_mez(s->ref, (size_t)s->nn);
    for (k = 0; k < s->nel && k < MAX_EL; k++) {
        const SimEl *e = &s->el[k];
        rf_mezi(e->tipo); rf_mezi(e->forma); rf_mezi(e->a); rf_mezi(e->b); rf_mezd(e->val);
        rf_mez(e->p, sizeof e->p); rf_mezd(e->x0); rf_mezd(e->t1); rf_mezd(e->t2); rf_mez(e->m, sizeof e->m);
    }
}
static void rf_nw(const SimNewton *n)
{
    rf_mezi(n->iter); rf_mezi(n->iter_max); rf_mezi(n->sub); rf_mezi(n->eventos); rf_mezi(n->homotopia);
    rf_mezi(n->iter_op); rf_mezd(n->residuo_max); rf_mezi(n->sonda);
}
static void rf_res(int todo)
{
    const SimRes *r = sim_res();
    const SimReg *g = sim_reg();
    int i, k, c;
    rf_diag(&r->d); rf_mezi(r->fin); rf_mezd(r->h); rf_mezd(r->t_fin); rf_mezd(r->t_pedido); rf_mezd(r->t_med0);
    rf_mezi(r->h_auto); rf_mezi(r->t_auto);
    rf_mezi(g->n);
    if (g->n > 0) { rf_mezd(g->t[g->n - 1]); rf_mezd(g->y[0][g->n - 1]); rf_mezd(g->y[1][g->n - 1]); }
    rf_mezi(r->pasos); rf_mezi(r->factorizaciones); rf_mezi(r->eventos); rf_mezi(r->tramos); rf_nw(&r->nw);
    rf_mez(r->W, sizeof r->W); rf_mez(r->Vmax, sizeof r->Vmax); rf_mez(r->Imax, sizeof r->Imax); rf_mez(r->Pmax, sizeof r->Pmax);
    rf_mez(r->Emax, sizeof r->Emax);
    if (!todo) return;
    rf_valores(&r->op); rf_valores(&r->ultimo);
    rf_mez(r->Vmed, sizeof r->Vmed); rf_mez(r->Vef, sizeof r->Vef); rf_mez(r->Imed, sizeof r->Imed); rf_mez(r->Ief, sizeof r->Ief);
    rf_mez(r->Pmed, sizeof r->Pmed); rf_mez(r->E0, sizeof r->E0); rf_mez(r->Efin, sizeof r->Efin); rf_mez(r->lectura, sizeof r->lectura);
    rf_mezd(r->dif_balance); rf_mezi(r->ntop);
    for (i = 0; i < r->ntop && i < SIM_MAX_TOP; i++) {
        rf_mezi(r->mascara[i]); rf_mezd(r->t_top[i]); rf_mezi(r->nmodos[i]); rf_mezi((long)r->tramos_top[i]);
        for (k = 0; k < r->nmodos[i] && k < SIM_MAX_MODOS; k++) { rf_mezd(r->modo[i][k].re); rf_mezd(r->modo[i][k].im); }
    }
    rf_mezd(r->tau_min); rf_mezd(r->tau_max); rf_mezd(r->t_osc); rf_mezi(r->nsucesos);
    for (i = 0; i < r->nsucesos && i < SIM_MAX_SUCESOS; i++) { rf_mezd(r->suceso[i].t); rf_mezi(r->suceso[i].tipo); rf_mezi(r->suceso[i].elem); rf_mezi(r->suceso[i].lazo); }
    rf_mezd(r->t_est);
    for (i = 0; i < g->n; i++) { rf_mezd(g->t[i]); for (c = 0; c < 2; c++) { rf_mezd(g->y[c][i]); rf_mezd(g->s1[c][i]); rf_mezd(g->s2[c][i]); } }
}
static void rf_cc(const SimCc *io)
{
    rf_mezi(io->hay); rf_mezi(io->n); rf_mez(io->x, sizeof io->x); rf_mez(io->vlim, sizeof io->vlim); rf_mez(io->tramo, sizeof io->tramo);
    rf_valores(&io->v); rf_mezi(io->iter); rf_mezi(io->salto); rf_mezd(io->p); rf_mezd(io->residuo);
}

#define RF_POOL 5
static SimRed rf_pool[RF_POOL];
static SimAjustes rf_aj;

/* Un elemento que barrer en s: E o J (bx E/J), R (BX_R); -1 si no hay */
static int rf_barrible(const SimRed *s, int *bx)
{
    int k, i, cand[MAX_EL], nc = 0;
    for (k = 0; k < s->nel; k++)
        if (s->el[k].tipo == SE_E || s->el[k].tipo == SE_J || s->el[k].tipo == SE_R || s->el[k].tipo == SE_EC || s->el[k].tipo == SE_JC)
            cand[nc++] = k;
    if (!nc) return -1;
    i = cand[rf_u((unsigned)nc)];
    *bx = s->el[i].tipo == SE_E ? BX_E : s->el[i].tipo == SE_J ? BX_J : s->el[i].tipo == SE_R ? BX_R : BX_K;
    if (rf_u(10) == 0) *bx = (int)rf_u(4);                   /* a veces un tipo que no es el suyo */
    return i;
}

/* La huella de la secuencia de la semilla (vueltas llamadas) */
static unsigned long long fz_corre(unsigned sem, int vueltas)
{
    static SimCc io, io2;
    static Valores v;
    static SimLin L;
    static SimTopoCA tca;
    static SimModo md[SIM_MAX_MODOS];
    SimDiag d;
    SimNewton nw;
    int it, emu = 0, i, bk = -1, bbx = 0, bp = 0;
    double bv = 0.0;
    rf_s = 88172645463325252ULL ^ (unsigned long long)sem * 0x9E3779B97F4A7C15ULL;
    if (!rf_s) rf_s = 1;
    rf_h = rf_total = 1469598103934665603ULL;
    sim_ops_reset();                                          /* sim_ops entra en cada huella */
    sim_cc_residuo(0); sim_cc_tope(0); sim_nl_topes(0, 0); sim_nl_tope_ops(0);   /* los de omisión */
    memset(&io2, 0, sizeof io2); memset(&v, 0, sizeof v); memset(&L, 0, sizeof L);   /* como en un programa nuevo */
    memset(&tca, 0, sizeof tca); memset(md, 0, sizeof md); memset(&rf_aj, 0, sizeof rf_aj);
    sim_tope_pasos(2500);
    for (i = 0; i < RF_POOL; i++) rf_red(&rf_pool[i], (int)(rf_u(3) == 0));
    memset(&io, 0, sizeof io);
    for (it = 0; it < vueltas; it++) {
        int op = (int)rf_u(16), p = (int)rf_u(RF_POOL), r = 0;
        SimRed *s = &rf_pool[p];
        switch (op) {
        case 0:                                               /* una red nueva en el cajón */
            rf_red(s, (int)(rf_u(3) == 0));
            rf_linea(0);
            break;
        case 1: {                                             /* cambia un valor de una red (la de un barrido, a veces) */
            int k;
            if (bk >= 0 && rf_u(2)) s = &rf_pool[bp];
            if (!s->nel) break;
            k = (int)rf_u((unsigned)s->nel);
            if (s->el[k].tipo == SE_R || s->el[k].tipo == SE_C || s->el[k].tipo == SE_L) s->el[k].val *= rf_d(0.5, 2.0);
            else if (s->el[k].tipo == SE_E || s->el[k].tipo == SE_J) s->el[k].p[0] += rf_d(-1, 1) * (s->el[k].tipo == SE_J ? 1e-3 : 1.0);
            rf_linea(k);
            break;
        }
        case 2:                                               /* empieza una emulación */
            memset(&rf_aj, 0, sizeof rf_aj);
            rf_aj.t_fin = rf_u(2) ? 0.0 : rf_d(5e-4, 5e-3);
            if (rf_u(3) == 0) rf_aj.h = rf_d(1e-6, 5e-5);
            if (rf_u(4) == 0) rf_aj.max_pasos = 200 + (long)rf_u(2000);
            sim_cc_residuo((int)rf_u(2));
            canal_de(&rf_aj, s, 0, CANAL_NODO, s->nn > 1 ? 1 : 0);
            if (s->nel) canal_de(&rf_aj, s, 1, rf_u(2) ? CANAL_I : CANAL_V, (int)rf_u((unsigned)s->nel));
            r = sim_iniciar_red(s, &rf_aj);
            emu = r == SIM_OK;
            rf_res(1); rf_red_act();
            rf_linea(r);
            break;
        case 3: case 4:                                       /* un trozo de la emulación */
            if (!emu) break;
            r = sim_avanzar(20000ul + rf_u(300000));
            rf_res(!r);
            if (!r) emu = 0;
            rf_linea(r);
            break;
        case 5:                                               /* el OP de otra red, sin reponer la emulación */
            r = sim_op_red(s, &v, &d);
            rf_valores(&v); rf_diag(&d); rf_red_act();
            rf_linea(r);
            break;
        case 6:
            memset(&nw, 0, sizeof nw);
            r = sim_op_cc_red(s, &v, &d, &nw);
            rf_valores(&v); rf_diag(&d); rf_nw(&nw); rf_red_act();
            rf_linea(r);
            break;
        case 7:
            memset(&nw, 0, sizeof nw);
            r = sim_punto_q(s, &L, &d, &nw);
            rf_mez(&L, sizeof L); rf_diag(&d); rf_nw(&nw); rf_red_act();
            rf_linea(r);
            break;
        case 8: case 9: case 10: {                            /* unos puntos de un barrido */
            int n, m = 1 + (int)rf_u(6);
            if (bk < 0 || rf_u(4) == 0) {                     /* otro barrido: otra red o el mismo io */
                bp = p;
                bk = rf_barrible(&rf_pool[bp], &bbx);
                if (bk < 0) break;
                if (rf_u(3)) memset(&io, 0, sizeof io);
                bv = rf_d(-5, 5);
            }
            sim_cc_tope(1);
            for (n = 0; n < m; n++) {
                if (rf_u(5) == 0) bv = rf_u(2) ? 0.0 : -0.0;
                else bv += rf_d(-1, 1);
                r = sim_cc_punto(&rf_pool[bp], bk, bbx, bbx == BX_R ? fabs(bv) * 1e3 + 1.0 : bv, &io, &d);
                rf_cc(&io); rf_diag(&d); rf_red_act();
                rf_linea(r);
                if (r != SIM_OK && rf_u(2)) io.hay = 0;
            }
            sim_cc_tope(0);
            break;
        }
        case 11: {
            SimCanal ch;
            double dy = 0.0;
            if (bk < 0 || !io.hay) break;
            memset(&ch, 0, sizeof ch);
            ch.tipo = CANAL_NODO; ch.nodo = (signed char)(rf_pool[bp].nn > 1 ? 1 : 0); ch.elem = -1; ch.sg = 1;
            r = sim_cc_deriva(&rf_pool[bp], bk, bbx, &io, &ch, &dy);
            rf_mezd(dy); rf_red_act();
            rf_linea(r);
            break;
        }
        case 12:
            r = sim_topo_ca(s, &tca, &d);
            rf_mez(&tca, sizeof tca); rf_diag(&d); rf_red_act();
            rf_linea(r);
            break;
        case 13: {
            double vth, rth;
            int inf;
            r = sim_thevenin_cc(s, (int)rf_u((unsigned)s->nn), (int)rf_u((unsigned)s->nn), &vth, &rth, &inf, &d);
            rf_mezd(vth); rf_mezd(rth); rf_mezi(inf); rf_diag(&d); rf_red_act();
            rf_linea(r);
            break;
        }
        case 14: {                                            /* un barrido suelto con su propio io (otra red) */
            int n, k2, bx2 = 0;
            k2 = rf_barrible(s, &bx2);
            if (k2 < 0) break;
            memset(&io2, 0, sizeof io2);
            for (n = 0; n < 3; n++) {
                r = sim_cc_punto(s, k2, bx2, rf_d(-3, 3), &io2, &d);
                rf_cc(&io2); rf_diag(&d);
                rf_linea(r);
            }
            break;
        }
        default:
            if (emu) break;                                   /* borra res_: no con una a medias */
            {
                int nm = 0;
                memset(md, 0, sizeof md);
                r = sim_modos_prueba(s, rf_d(1e-6, 1e-4), md, &nm);
                rf_mezi(nm);
                for (i = 0; i < nm && i < SIM_MAX_MODOS; i++) { rf_mezd(md[i].re); rf_mezd(md[i].im); }
                rf_linea(r);
            }
            break;
        }
    }
    while (emu) emu = sim_avanzar(1000000ul);
    rf_res(1); rf_red_act();
    rf_linea(0);
    sim_tope_pasos(0); sim_cc_residuo(0); sim_cc_tope(0);  /* como estaba */
    return rf_total;
}

/* La huella de la secuencia en un proceso hijo (fork): los dos modos parten
 * del mismo estado (el motor recuerda cosas de una llamada a otra, como el
 * arranque de Newton, y el revisor corría cada semilla en un programa
 * nuevo); el hijo no deja huellas de sus emulaciones ni imprime nada */
static unsigned long long fz_en_hijo(unsigned sem, int vueltas, int entero)
{
    int p[2];
    pid_t pid;
    unsigned long long h = 0;
    fflush(stdout);
    if (huella_f) fflush(huella_f);
    if (pipe(p) != 0) return 0;
    pid = fork();
    if (pid == 0) {
        huella_f = 0;
        close(p[0]);
        sim_bak_entero(entero);
        h = fz_corre(sem, vueltas);
        if (write(p[1], &h, sizeof h) != (ssize_t)sizeof h) _exit(1);
        _exit(0);
    }
    close(p[1]);
    if (pid < 0 || read(p[0], &h, sizeof h) != (ssize_t)sizeof h) h = 0;
    close(p[0]);
    if (pid > 0) waitpid(pid, 0, 0);
    return h;
}
