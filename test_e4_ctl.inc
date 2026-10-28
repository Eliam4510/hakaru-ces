/* --------------------------------------------------------------------------
 *  56. Etapa 4: la calculadora de CC con fuentes controladas
 *  57. Etapa 4: controladas en el emulador y en la CA
 *
 *  Se incluye en host_test.c (usa rc, rr, nodos, nuevo_el, cierra_red,
 *  semilla, azar, sr, saj, sr_init, sr_el, canal, ca_sr, car, bode_sr,
 *  u_ca, azar_ca, red_rlc_seno, err_ultimo_periodo y tk_calcula).
 * ----------------------------------------------------------------------- */
#include "proced.h"

/* ---- Redes abstractas en rc (los nodos a mano, como el generador de [5]) ---- */
static void c4_init(int n)
{
    circ_init(&rc, "ctl");
    memset(&rr, 0, sizeof rr);
    nodos = n;
}

static int c4_el(TipoEl t, int a, int b, double v)
{
    int k = nuevo_el(t, a, b);
    if (k < 0) return k;
    rr.c[k] = -1;
    if (v != 0.0) { rc.el[k].val.v = v; rc.el[k].val.est = M_DATO; }
    else if (t != EL_VOLT && t != EL_AMP) rc.el[k].val.est = M_LIBRE;
    return k;
}

/* Una controlada: var = DEP_V o DEP_I, g = la ganancia (0: vacía), ref = el control */
static int c4_dep(TipoEl t, int a, int b, int var, double g, int ref)
{
    int k = c4_el(t, a, b, g);
    if (k < 0) return k;
    rc.el[k].variante = (unsigned char)var;
    rc.el[k].ref = (signed char)ref;
    return k;
}

static const Solucion *c4_tabla(void)
{
    static Solucion s;
    cierra_red();
    resolver(&rc, &rr, &s);
    return &s;
}

static const Valores *c4_emula(SimDiag *d)
{
    static Valores v;
    sim_punto_operacion(&rc, &rr, &v, d);
    return &v;
}

/* El mensaje de la tabla cabe en 4 líneas de 52 y dice lo que debe */
static void c4_msg(const char *q, const Solucion *s, const char *debe)
{
    char m[400], d[520];
    sol_mensaje(&rc, &rr, s, m, sizeof m);
    chk_cond(q, "el mensaje cabe en 4 l" S_ii "neas de 52", lineas(m, 52) <= 4);
    pruebas++;
    if (!strstr(m, debe)) { sprintf(d, "el mensaje dice " S_LARR "%s" S_RARR ": %s", debe, m); falla(q, d, 0, 1); }
}

static void c4_msg_sim(const char *q, const SimRed *s, const SimDiag *dg, const char *debe)
{
    char m[400], d[520];
    sim_mensaje_circ(0, s, dg, m, sizeof m);
    chk_cond(q, "el mensaje cabe en 4 l" S_ii "neas de 52", lineas(m, 52) <= 4);
    pruebas++;
    if (!strstr(m, debe)) { sprintf(d, "el mensaje dice " S_LARR "%s" S_RARR ": %s", debe, m); falla(q, d, 0, 1); }
}

/* E1 = 2 V; R1 = R2 = 1 k en serie (V(R2) = 1 V); E2 = μ·V(R2) sobre RL = 1 k.
 * Nodos: 0 tierra, 1 = E1+, 2 = entre R1 y R2, 3 = E2+. En k: E1, R2, E2, RL */
static void c4_vcvs(double mu, int *k)
{
    c4_init(4);
    k[0] = c4_el(EL_FUENTE, 0, 1, 2.0);
    c4_el(EL_RES, 1, 2, 1e3);
    k[1] = c4_el(EL_RES, 2, 0, 1e3);
    k[2] = c4_dep(EL_EDEP, 0, 3, DEP_V, mu, k[1]);
    k[3] = c4_el(EL_RES, 3, 0, 1e3);
}

static void test_e4_ctl_tabla(void)
{
    static Pasos pz;
    const Solucion *s;
    const Valores *v;
    SimDiag d;
    int k[4], kA, kJ, kE, kR1, kR3, kV;
    unsigned sem0 = semilla;

    printf("[56] Etapa 4: la calculadora de CC con fuentes controladas\n");

    /* ---- VCVS ---- */
    c4_vcvs(10.0, k);
    s = c4_tabla();
    chk_cond("VCVS", "una soluci" S_oo "n", s->estado == SOL_OK && s->nsol == 1);
    if (s->nsol >= 1) {
        chk_t("VCVS", "V(E2) = 10 V", s->s[0].V[k[2]], 10.0, 10.0, 1e-12);
        chk_t("VCVS", "I(E2) = 10 mA (sale por +)", s->s[0].I[k[2]], 0.01, 0.01, 1e-12);
        chk_t("VCVS", "P(E2) = 0,1 W entregada", s->s[0].P[k[2]], 0.1, 0.1, 1e-12);
        chk_t("VCVS", "su valor: " S_MU " = 10", s->s[0].val[k[2]], 10.0, 10.0, 1e-15);
        chk("VCVS", "residuo de la tabla", sol_residuo(&rc, &rr, &s->s[0]), 0.0);
        chk_cond("VCVS", "sin procedimiento (D25)", proced_generar(&rc, &rr, s, 0, &pz, 0) == PROC_NADA);
    }
    /* (d3) el emulador: la ley de nodos de la rama de E2, una sola vez */
    v = c4_emula(&d);
    chk_cond("VCVS", "el emulador da su punto de operaci" S_oo "n", d.estado == SIM_OK);
    chk_t("VCVS", "emulador: I(E2) = 10 mA", v->I[k[2]], 0.01, 0.01, 1e-12);
    chk_t("VCVS", "emulador: V(E2) = 10 V", v->V[k[2]], 10.0, 10.0, 1e-12);
    chk("VCVS", "emulador: residuo de Kirchhoff", sol_residuo(&rc, &rr, v), 0.0);

    /* μ [?!] con V(RL) = 10: μ = 10 y μ = -10 cumplen; la preferencia deja 10 */
    c4_vcvs(0.0, k);
    rc.el[k[2]].val.est = M_BUSCAR;
    rc.el[k[3]].V.v = 10.0; rc.el[k[3]].V.est = M_DATO;
    s = c4_tabla();
    chk_cond("VCVS " S_MU " [?!]", "una soluci" S_oo "n (la preferencia)", s->estado == SOL_OK && s->nsol == 1 && s->ndesc == 1);
    if (s->nsol >= 1) {
        chk_t("VCVS " S_MU " [?!]", S_MU " = 10", s->s[0].val[k[2]], 10.0, 10.0, 1e-12);
        chk("VCVS " S_MU " [?!]", "residuo", sol_residuo(&rc, &rr, &s->s[0]), 0.0);
    }
    /* μ = -10 dato: un inversor (válido) */
    c4_vcvs(-10.0, k);
    s = c4_tabla();
    chk_cond("VCVS " S_MU " < 0", "una soluci" S_oo "n", s->estado == SOL_OK && s->nsol == 1);
    if (s->nsol >= 1) chk_t("VCVS " S_MU " < 0", "V(E2) = -10 V", s->s[0].V[k[2]], -10.0, 10.0, 1e-12);

    /* un dato de V de la controlada: φb - φa, y la preferencia da μ = 10 */
    c4_vcvs(0.0, k);
    rc.el[k[2]].val.est = M_BUSCAR;
    rc.el[k[2]].V.v = 10.0; rc.el[k[2]].V.est = M_DATO;
    s = c4_tabla();
    chk_cond("dato V(E2)", "una soluci" S_oo "n", s->estado == SOL_OK && s->nsol == 1);
    if (s->nsol >= 1) {
        chk_t("dato V(E2)", S_MU " = 10", s->s[0].val[k[2]], 10.0, 10.0, 1e-12);
        chk_t("dato V(E2)", "V(E2) = +10 V", s->s[0].V[k[2]], 10.0, 10.0, 1e-12);
    }

    /* ---- CCCS: E1 = 1 V, R1 = 100 k y A1 ideal en serie; J2 = 100·I(A1) sobre R3 ---- */
    c4_init(4);
    c4_el(EL_FUENTE, 0, 1, 1.0);
    c4_el(EL_RES, 1, 2, 1e5);
    kA = c4_el(EL_AMP, 2, 0, 0.0);
    kJ = c4_dep(EL_JDEP, 0, 3, DEP_I, 100.0, kA);
    kR3 = c4_el(EL_RES, 3, 0, 1e3);
    s = c4_tabla();
    chk_cond("CCCS", "una soluci" S_oo "n", s->estado == SOL_OK && s->nsol == 1);
    if (s->nsol >= 1) {
        chk_t("CCCS", "I(A1) = 10 " S_MU "A", s->s[0].I[kA], 1e-5, 1e-5, 1e-12);
        chk_t("CCCS", "|V(R3)| = 1 V", fabs(s->s[0].V[kR3]), 1.0, 1.0, 1e-12);
        chk_t("CCCS", "I(J2) = 1 mA", s->s[0].I[kJ], 1e-3, 1e-3, 1e-12);
    }
    v = c4_emula(&d);
    chk_cond("CCCS", "el emulador lo da igual", d.estado == SIM_OK && fabs(fabs(v->V[kR3]) - 1.0) <= 1e-12);

    /* ---- CCVS: E1 = 1 V sobre R1 = 1 k (control por la I de una R: sin rama) ---- */
    c4_init(3);
    c4_el(EL_FUENTE, 0, 1, 1.0);
    kR1 = c4_el(EL_RES, 1, 0, 1e3);
    kE = c4_dep(EL_EDEP, 0, 2, DEP_I, 5e3, kR1);
    c4_el(EL_RES, 2, 0, 1e3);
    s = c4_tabla();
    chk_cond("CCVS", "una soluci" S_oo "n", s->estado == SOL_OK && s->nsol == 1);
    if (s->nsol >= 1) chk_t("CCVS", "V(E2) = 5 V", s->s[0].V[kE], 5.0, 5.0, 1e-12);
    v = c4_emula(&d);
    chk_cond("CCVS", "el emulador lo da igual", d.estado == SIM_OK && fabs(v->V[kE] - 5.0) <= 5e-12);

    /* ---- VCCS: E1 = 5 V sobre R1 = 1 k; J2 = 1 mS·V(R1) sobre R2 = 1 k ---- */
    c4_init(3);
    c4_el(EL_FUENTE, 0, 1, 5.0);
    kR1 = c4_el(EL_RES, 1, 0, 1e3);
    c4_dep(EL_JDEP, 0, 2, DEP_V, 1e-3, kR1);
    kR3 = c4_el(EL_RES, 2, 0, 1e3);
    s = c4_tabla();
    chk_cond("VCCS", "una soluci" S_oo "n", s->estado == SOL_OK && s->nsol == 1);
    if (s->nsol >= 1) chk_t("VCCS", "|V(R2)| = 5 V", fabs(s->s[0].V[kR3]), 5.0, 5.0, 1e-12);
    v = c4_emula(&d);
    chk_cond("VCCS", "el emulador lo da igual", d.estado == SIM_OK && fabs(fabs(v->V[kR3]) - 5.0) <= 5e-12);

    /* ---- El lazo: E2 = -10·V(V1) realimenta a x ---- */
    c4_init(4);
    c4_el(EL_FUENTE, 0, 1, 1.0);
    c4_el(EL_RES, 1, 2, 1e3);
    c4_el(EL_RES, 2, 3, 1e3);
    kV = c4_el(EL_VOLT, 2, 0, 0.0);
    c4_dep(EL_EDEP, 0, 3, DEP_V, -10.0, kV);
    s = c4_tabla();
    chk_cond("lazo", "una soluci" S_oo "n", s->estado == SOL_OK && s->nsol == 1);
    if (s->nsol >= 1) {
        chk_t("lazo", S_PHI "x = 1/12", s->s[0].phi[2], 1.0 / 12.0, 1.0, 1e-12);
        chk_t("lazo", S_PHI "y = -10/12", s->s[0].phi[3], -10.0 / 12.0, 1.0, 1e-12);
    }
    v = c4_emula(&d);
    chk_cond("lazo", "el emulador lo da igual", d.estado == SIM_OK && fabs(v->phi[2] - 1.0 / 12.0) <= 1e-12 &&
             fabs(v->phi[3] + 10.0 / 12.0) <= 1e-12);

    /* ---- Errores ---- */
    c4_vcvs(10.0, k);
    rc.el[k[2]].ref = -1;
    s = c4_tabla();
    chk_cond("sin control", "SOL_CTL con E2 y CTL_FALTA", s->estado == SOL_CTL && s->elem == k[2] && s->motivo == CTL_FALTA);
    c4_msg("sin control", s, "fila x");
    v = c4_emula(&d);
    chk_cond("sin control", "el emulador: SIM_CTL_MAL con CTL_FALTA", d.estado == SIM_CTL_MAL && d.elem == k[2] && d.campo == CTL_FALTA);
    chk_cond("sin control", "sim_valida lo mismo", sim_valida(&rc, &d) == SIM_CTL_MAL && d.campo == CTL_FALTA);

    c4_vcvs(10.0, k);
    rc.el[k[2]].ref = (signed char)k[2];
    s = c4_tabla();
    chk_cond("ella misma", "SOL_CTL con CTL_SI_MISMA", s->estado == SOL_CTL && s->elem == k[2] && s->motivo == CTL_SI_MISMA);
    c4_msg("ella misma", s, "a s" S_ii " misma");

    /* J2 = 2·V(R1); E3 = 1k·I(J2): la tabla podría, el emulador no: los dos lo rechazan */
    c4_init(4);
    c4_el(EL_FUENTE, 0, 1, 1.0);
    kR1 = c4_el(EL_RES, 1, 0, 1e3);
    kJ = c4_dep(EL_JDEP, 0, 2, DEP_V, 2.0, kR1);
    c4_el(EL_RES, 2, 0, 1e3);
    kE = c4_dep(EL_EDEP, 0, 3, DEP_I, 1e3, kJ);
    c4_el(EL_RES, 3, 0, 1e3);
    s = c4_tabla();
    chk_cond("I de una J controlada", "SOL_CTL con CTL_I_NO", s->estado == SOL_CTL && s->elem == kE && s->motivo == CTL_I_NO);
    c4_msg("I de una J controlada", s, "amper" S_ii "metro en serie");
    v = c4_emula(&d);
    chk_cond("I de una J controlada", "el emulador: SIM_CTL_MAL con el mismo motivo", d.estado == SIM_CTL_MAL && d.elem == kE && d.campo == CTL_I_NO && d.nodo == kJ);

    /* la ganancia [?!] con el control en 0: no se puede hallar */
    c4_init(5);
    c4_el(EL_FUENTE, 0, 1, 1.0);
    c4_el(EL_RES, 1, 0, 1e3);
    kR3 = c4_el(EL_RES, 1, 4, 1e3);                 /* sin corriente: V(R3) = 0 */
    kE = c4_dep(EL_EDEP, 0, 2, DEP_V, 0.0, kR3);
    rc.el[kE].val.est = M_BUSCAR;
    kR1 = c4_el(EL_RES, 2, 0, 1e3);
    rc.el[kR1].V.v = 5.0; rc.el[kR1].V.est = M_DATO;
    s = c4_tabla();
    chk_cond("control en 0", "SOL_SIN_CORRIENTE con la controlada", s->estado == SOL_SIN_CORRIENTE && s->elem == kE);
    c4_msg("control en 0", s, "su control vale 0");

    /* ---- Thévenin con una controlada: Vth 13,5 V, Rth 4,5, IN 3 A, Pmax 10,125 W ---- */
    {
        static SimRed sth;
        CaThev t;
        double vth, rth;
        int inf, kAm;
        c4_init(4);
        c4_el(EL_FUENTE, 0, 1, 12.0);
        c4_el(EL_RES, 1, 2, 4.0);
        kR3 = c4_el(EL_RES, 2, 0, 12.0);
        c4_dep(EL_EDEP, 2, 3, DEP_V, 0.5, kR3);
        cierra_red();
        chk_cond("Th" S_ee "venin", "la red del emulador", sim_red_de(&rc, &rr, &sth, &d) == SIM_OK);
        chk_cond("Th" S_ee "venin", "sim_thevenin_cc", sim_thevenin_cc(&sth, 3, 0, &vth, &rth, &inf, &d) == SIM_OK && !inf);
        chk_t("Th" S_ee "venin", "Vth = 13,5 V", vth, 13.5, 13.5, 1e-12);
        chk_t("Th" S_ee "venin", "Rth = 4,5", rth, 4.5, 4.5, 1e-12);
        memset(&t, 0, sizeof t);
        chk_cond("Th" S_ee "venin", "ca_thevenin_red en CC", ca_thevenin_red(&sth, 3, 0, 0.0, &t) == SIM_OK && t.hay_no);
        chk_t("Th" S_ee "venin", "IN = 3 A", t.In.re, 3.0, 3.0, 1e-12);
        chk_t("Th" S_ee "venin", "Pmax = 10,125 W", t.Pmax, 10.125, 10.125, 1e-12);
        s = c4_tabla();
        chk_cond("Th" S_ee "venin", "la tabla en vac" S_ii "o", s->estado == SOL_OK && s->nsol == 1);
        if (s->nsol >= 1) chk_t("Th" S_ee "venin", "tabla: V(AB) = 13,5 V", s->s[0].phi[3] - s->s[0].phi[0], 13.5, 13.5, 1e-12);
        kAm = c4_el(EL_AMP, 3, 0, 0.0);
        s = c4_tabla();
        chk_cond("Th" S_ee "venin", "la tabla en corto", s->estado == SOL_OK && s->nsol == 1);
        if (s->nsol >= 1) chk_t("Th" S_ee "venin", "tabla: I(A) = IN = 3 A", s->s[0].I[kAm], 3.0, 3.0, 1e-12);
    }
    semilla = sem0;
}

/* ==========================================================================
 *  [57] Controladas en el emulador y en la CA
 * ======================================================================= */
static unsigned c4_sem = 97531u;
static double c4_u(void) { c4_sem = c4_sem * 1103515245u + 12345u; return (double)((c4_sem >> 8) & 0xFFFFFF) / 16777216.0; }
static int c4_n(int n) { return (int)(c4_u() * (double)n); }
static double c4_g(void) { return (c4_u() < 0.5 ? -0.1 : 0.1) * (double)(1 + c4_n(100)); }   /* ±0,1 .. ±10 */

/* Una red de CC del generador de [27] y [33] (sin la llamada a cierra_red) */
static void c4_red_cc(int caso)
{
    int tipo = caso % 7, f, i, a, b, k;
    f = fuente_sp(1.0 + azar(200));
    if (tipo == 1) { rc.el[f].aux.est = M_DATO; rc.el[f].aux.v = 0.1 * (double)(1 + azar(50)); }
    if (tipo == 2) { rc.el[f].tipo = EL_CORR; rc.el[f].etiq[0] = 'J'; rc.el[f].val.v = 0.5 * (double)(1 + azar(20)); }
    if (tipo >= 3 && nodos > 2)
        for (i = 0; i < 1 + (int)azar(3) && rc.nel < MAX_EL - 4; i++) {
            a = (int)azar((unsigned)nodos); b = (int)azar((unsigned)nodos);
            if (a == b) continue;
            k = nuevo_el(azar(3) == 0 ? EL_FUENTE : azar(2) ? EL_CORR : EL_RES, a, b);
            if (k >= 0 && rc.el[k].tipo != EL_RES) { rc.el[k].val.est = M_DATO; rc.el[k].val.v = 1.0 + azar(30); }
            if (k >= 0 && rc.el[k].tipo == EL_FUENTE && azar(2)) { rc.el[k].aux.est = M_DATO; rc.el[k].aux.v = 0.5 + azar(5); }
        }
    if (tipo == 6 && nodos > 2)
        for (i = 0; i < 1 + (int)azar(3) && rc.nel < MAX_EL - 4; i++) {
            int t = (int)azar(3);
            a = (int)azar((unsigned)nodos); b = (int)azar((unsigned)nodos);
            if (a == b) continue;
            k = nuevo_el(t == 0 ? EL_COND : t == 1 ? EL_BOB : EL_INTER, a, b);
            if (k < 0) continue;
            if (t < 2) { rc.el[k].val.est = M_DATO; rc.el[k].val.v = 1e-6 * (1 + azar(100)); }
            else rc.el[k].variante = (unsigned char)azar(2);
        }
    if (tipo == 5 && nodos > 2 && rc.nel < MAX_EL - 4) {
        a = (int)azar((unsigned)nodos); b = (int)azar((unsigned)nodos);
        if (a != b) {
            k = nuevo_el(EL_VOLT, a, b);
            if (azar(2)) { rc.el[k].val.est = M_DATO; rc.el[k].val.v = 1e3 * (1 + azar(100)); }
        }
        k = 1 + (int)azar((unsigned)(rc.nel - 1));
        if (rc.el[k].tipo == EL_RES && rc.nel < MAX_EL - 4) {
            int nuevo = nodos++, ka = nuevo_el(EL_AMP, nuevo, rr.b[k]);
            rr.b[k] = nuevo;
            if (ka >= 0 && azar(2)) { rc.el[ka].val.est = M_DATO; rc.el[ka].val.v = 0.1 * (1 + azar(10)); }
        }
    }
    /* 1 a 3 controladas de los 4 tipos, con cualquier control (también los que no valen) */
    for (i = 0, b = 1 + c4_n(3); i < b && rc.nel < MAX_EL; i++) {
        int x = c4_n(nodos), y = c4_n(nodos), ref, t = c4_u() < 0.5 ? EL_EDEP : EL_JDEP;
        double u = c4_u();
        if (x == y) y = (x + 1) % nodos;
        ref = u < 0.015 ? -1 : u < 0.03 ? rc.nel : c4_n(rc.nel);  /* sin control, ella misma, otro */
        if (t == EL_EDEP && c4_u() < 0.5 && rc.nel < MAX_EL - 1) {  /* la mitad, con una R en serie */
            int m = nodos++;
            nuevo_el(EL_RES, m, y);
            y = m;
            if (ref == rc.nel - 1) ref = rc.nel;     /* «ella misma» sigue siéndolo */
        }
        k = nuevo_el((TipoEl)t, x, y);
        if (k < 0) break;
        rc.el[k].variante = (unsigned char)(c4_u() < 0.5 ? DEP_V : DEP_I);
        rc.el[k].ref = (signed char)ref;
        rc.el[k].val.v = c4_g(); rc.el[k].val.est = M_DATO;
    }
    for (k = 0; k < rc.nel; k++) rr.c[k] = -1;
}

/* Cuánto se mueve la media del error del canal c entre los dos últimos
 * periodos. Una controlada puede llevar la constante que guarda un lazo de L
 * sin R (o un corte de C) a otro integrador: la red deriva sin fin (un modo
 * s = 0 doble, que los modos no ven) y no hay régimen permanente */
static double c4_deriva(int c, Cx X, double w, double t_fin, double T)
{
    const SimReg *g = sim_reg();
    double s1 = 0.0, s2 = 0.0, m1 = 0.0, m2 = 0.0, a1 = 0.0, a2 = 0.0, dm, da;
    int n1 = 0, n2 = 0, i;
    for (i = 0; i < g->n; i++) {
        double t = g->t[i], d;
        if (t <= t_fin - 2.0 * T * (1.0 - 1e-9)) continue;
        d = g->y[c][i] - (X.re * cos(w * t) - X.im * sin(w * t));
        if (t > t_fin - T * (1.0 - 1e-9)) { s2 += d; n2++; } else { s1 += d; n1++; }
    }
    if (!n1 || !n2) return 0.0;
    m1 = s1 / n1; m2 = s2 / n2;
    /* y la amplitud del resto en cada periodo: un C algebraico que manda una
     * CCVS cuya salida carga otro C algebraico (un doble derivador, una DAE
     * de índice 2) hace crecer el modo (-1)^n del trapecio como n·(-1)^n */
    for (i = 0; i < g->n; i++) {
        double t = g->t[i], d;
        if (t <= t_fin - 2.0 * T * (1.0 - 1e-9)) continue;
        d = g->y[c][i] - (X.re * cos(w * t) - X.im * sin(w * t));
        if (t > t_fin - T * (1.0 - 1e-9)) { if (fabs(d - m2) > a2) a2 = fabs(d - m2); }
        else if (fabs(d - m1) > a1) a1 = fabs(d - m1);
    }
    dm = fabs(m2 - m1); da = fabs(a2 - a1);
    return dm > da ? dm : da;
}

/* (b) Una red RLC senoidal con 1 a 3 controladas (sobre sr) */
static int c4_red_seno(double f, int *kc0, int *kdep)
{
    int kc1, kv, ka, kw, kf, i, n, k;
    kf = red_rlc_seno(f, kc0, &kc1, &kv, &ka, &kw);
    *kdep = -1;
    for (i = 0, n = 1 + c4_n(3); i < n && sr.nel < MAX_EL; i++) {
        int x = c4_n(sr.nn), y = c4_n(sr.nn), ctl = c4_n(sr.nel), ci = c4_u() < 0.5, t;
        if (x == y) y = (x + 1) % sr.nn;
        t = sr.el[ctl].tipo;
        if (t == SE_JC || t == SE_Z || t == SE_D) ci = 0;
        k = sr_el(c4_u() < 0.5 ? SE_EC : SE_JC, x, y, 0.0);
        /* ganancias con la escala de la red: μ y β sin unidades; r en Ω y g en S (z0 ~ 1 k) */
        sr.el[k].val = c4_g() * (sr.el[k].tipo == SE_EC ? (ci ? 1e3 : 1.0) : (ci ? 1.0 : 1e-3));
        sr.el[k].ctl = (signed char)ctl; sr.el[k].ctl_i = (unsigned char)ci;
        if (*kdep < 0) *kdep = k;
    }
    return kf;
}

static void test_e4_ctl_motor(void)
{
    static SimModo md[SIM_MAX_MODOS];
    static CaRes a_t;
    static Solucion s;
    static Valores v;
    SimDiag d;
    unsigned sem0 = semilla, azar0 = azar_ca;
    int caso, k, n_cmp = 0, n_sing = 0, n_ctl = 0, n_otros = 0, n_ok = 0, n_inest = 0, n_lazo = 0, n_deriva = 0;
    int malos_m = 0, malos_t = 0;
    double peor_m = 0.0, peor_t = 0.0;
    char q[48];

    printf("[57] Etapa 4: controladas en el emulador y en la CA\n");

    /* ---- (a) Emulador = tabla, 2000 redes ---- */
    semilla = 424242u;
    for (caso = 0; caso < 2400; caso++) {
        int et, es, m = CTL_OK, corto = 0;
        c4_red_cc(caso);
        cierra_red();
        for (k = 0; k < rc.nel; k++) if (rr.a[k] == rr.b[k]) corto = 1;
        if (corto) { n_otros++; continue; }
        sprintf(q, "ctl %d", caso);
        for (k = 0; k < rc.nel; k++)
            if (tipo_info(rc.el[k].tipo)->ley_cc == LEY_DEP && (m = circ_ctl_motivo(&rc, k)) != CTL_OK) break;
        et = resolver(&rc, &rr, &s);
        es = sim_punto_operacion(&rc, &rr, &v, &d);
        if (m != CTL_OK) {
            n_ctl++;
            chk_cond(q, "tabla: SOL_CTL con el motivo", et == SOL_CTL && s.elem == k && s.motivo == m);
            chk_cond(q, "emulador: SIM_CTL_MAL con el mismo motivo", es == SIM_CTL_MAL && d.elem == k && d.campo == m);
            continue;
        }
        if (et == SOL_OK && s.nsol == 1) {
            double escv = 1e-30, esci = 1e-30;
            if (es != SIM_OK) {
                char mm[200];
                sim_mensaje_circ(&rc, 0, &d, mm, sizeof mm); falla(q, mm, es, SIM_OK);
                if (getenv("HAKARU_DEPURA"))
                    for (k = 0; k < rc.nel; k++)
                        printf("        %2d %-4s a %d b %d val %.6g (est %d) aux %.6g (est %d) var %d ref %d\n", k, rc.el[k].etiq, rr.a[k], rr.b[k],
                               rc.el[k].val.v, rc.el[k].val.est, rc.el[k].aux.v, rc.el[k].aux.est, rc.el[k].variante, rc.el[k].ref);
                continue;
            }
            double emax = 0.0, jmax = 0.0, rmax = 1.0;
            for (k = 0; k < rc.nel; k++) {           /* con las fuentes: la red puede anularlo todo */
                if (fabs(s.s[0].V[k]) > escv) escv = fabs(s.s[0].V[k]);
                if (fabs(s.s[0].I[k]) > esci) esci = fabs(s.s[0].I[k]);
                if (rc.el[k].tipo == EL_FUENTE && fabs(rc.el[k].val.v) > emax) emax = fabs(rc.el[k].val.v);
                if (rc.el[k].tipo == EL_CORR && fabs(rc.el[k].val.v) > jmax) jmax = fabs(rc.el[k].val.v);
                if (rc.el[k].tipo == EL_RES && rc.el[k].val.v > rmax) rmax = rc.el[k].val.v;
            }
            /* la escala natural de la red: las fems, J·R; las J, E/R */
            if (emax > escv) escv = emax;
            if (jmax * rmax > escv) escv = jmax * rmax;
            if (jmax > esci) esci = jmax;
            if (emax / rmax > esci) esci = emax / rmax;
            for (k = 0; k < rc.nel; k++) {
                char det[64];
                pruebas += 2;
                if (fabs(v.V[k] - s.s[0].V[k]) > 1e-9 * escv) { sprintf(det, "V(%s) emulador = tabla", rc.el[k].etiq); falla(q, det, v.V[k], s.s[0].V[k]); }
                if (fabs(v.I[k] - s.s[0].I[k]) > 1e-9 * esci) { sprintf(det, "I(%s) emulador = tabla", rc.el[k].etiq); falla(q, det, v.I[k], s.s[0].I[k]); }
            }
            chk(q, "residuo de Kirchhoff del emulador", sol_residuo(&rc, &rr, &v), 0.0);
            n_cmp++;
        } else if (et == SOL_INDET || et == SOL_INCOMPAT) {
            /* el emulador lo dice con su nombre o con su rescate */
            chk_cond(q, "sin soluci" S_oo "n " S_uu "nica en la tabla: el emulador da un error o su regla",
                     es == SIM_SINGULAR || es == SIM_LAZO_V || es == SIM_CORTE_J ||
                     (es == SIM_OK && (d.avisos & (SIM_AV_FLOTA | SIM_AV_CARGA | SIM_AV_FLUJO | SIM_AV_C_DESC |
                                                   SIM_AV_L_SIN_I | SIM_AV_CI_SALTA))));
            n_sing++;
        } else n_otros++;
    }
    printf("    (a) %d redes iguales en los dos motores, %d sin soluci" S_oo "n " S_uu "nica, %d con un control que no vale, %d otras\n",
           n_cmp, n_sing, n_ctl, n_otros);
    chk_cond("ctl al azar", "al menos 1500 comparadas", n_cmp >= 1500);
    chk_cond("ctl al azar", "los controles que no valen salen (al menos 100)", n_ctl >= 100);

    /* ---- (b) y (c): fasor = trapecio y Tellegen en CA, 150 redes estables ---- */
    azar_ca = 13579u;
    for (caso = 0; caso < 6000 && n_ok < 150; caso++) {
        double f = 100.0 * pow(10.0, 2.0 * u_ca()), T = 1.0 / f, w = 2.0 * PI_T * f, h = T / 100.0;
        double taumax = 0.0, tf, hr, wt, esc, e0, e1;
        int kc0, kdep, kf, nm = 0, i, malo = 0;
        kf = c4_red_seno(f, &kc0, &kdep);
        if (!sim_modos_prueba(&sr, h, md, &nm) || nm == 0) { n_lazo++; continue; }
        for (i = 0; i < nm; i++) {
            double mag = sqrt(md[i].re * md[i].re + md[i].im * md[i].im);
            if (!(md[i].re <= -0.02 * mag)) malo = 1;              /* inestable o casi */
            else if (-1.0 / md[i].re > taumax) taumax = -1.0 / md[i].re;
        }
        if (malo || taumax <= 0.0) { n_inest++; continue; }
        tf = ceil(40.0 * taumax / T) * T + 4.0 * T;
        if (tf / h > 20000.0) { n_inest++; continue; }              /* lenta */
        canal(0, CANAL_V, kc0); canal(1, CANAL_I, kdep);
        saj.h = h; saj.t_fin = tf;
        if (sim_emular(&sr, &saj) != SIM_OK || sim_res()->fin != FIN_COMPLETO) { n_lazo++; continue; }
        hr = sim_res()->h;
        wt = (2.0 / hr) * tan(0.5 * w * hr);
        sr.el[kf].p[1] = wt / (2.0 * PI_T);
        if (ca_sr(0.0) != SIM_OK) { sr.el[kf].p[1] = f; n_lazo++; continue; }
        a_t = car;
        sr.el[kf].p[1] = f;
        esc = 0.0;
        for (i = 0; i < sr.nel; i++) {
            if (cx_abs(a_t.V[i]) > esc) esc = cx_abs(a_t.V[i]);
            if (1e3 * cx_abs(a_t.I[i]) > esc) esc = 1e3 * cx_abs(a_t.I[i]);
        }
        if ((c4_deriva(0, a_t.V[kc0], w, sim_res()->t_fin, T) + 1e3 * c4_deriva(1, a_t.I[kdep], w, sim_res()->t_fin, T)) > 1e-9 * esc) {
            n_deriva++;
            continue;
        }
        e0 = err_ultimo_periodo(0, a_t.V[kc0], w, sim_res()->t_fin, T, 0);
        e1 = err_ultimo_periodo(1, a_t.I[kdep], w, sim_res()->t_fin, T, 0);
        n_ok++;
        {
            double em = (e0 + 1e3 * e1) / esc;
            if (em > peor_m) peor_m = em;
            if (!(em <= 1e-9)) {
                malos_m++;
                if (getenv("HAKARU_DEPURA") && malos_m <= 3) {
                    printf("      caso %d: f %g, e0 %.3e, e1 %.3e, esc %.3e, taumax %.3e, h %.3e, tf %.3e; canales V(%d) I(%d)\n",
                           caso, f, e0, e1, esc, taumax, h, tf, kc0, kdep);
                    for (i = 0; i < sr.nel; i++)
                        printf("        %2d: tipo %d a %d b %d val %.6g ctl %d ci %d | V %.6g%+.6gj I %.6g%+.6gj\n", i, sr.el[i].tipo,
                               sr.el[i].a, sr.el[i].b, sr.el[i].val, sr.el[i].ctl, sr.el[i].ctl_i,
                               a_t.V[i].re, a_t.V[i].im, a_t.I[i].re, a_t.I[i].im);
                }
            }
        }
        {
            /* Tellegen con la escala de la red (una red que lo anula todo
             * deja las S en el redondeo): |Sf - Sc| <= 1e-12·máx(Σ|S|, Vmáx·Imáx) */
            double sv = 0.0, si = 0.0, ss = 0.0;
            for (i = 0; i < sr.nel; i++) {
                if (cx_abs(a_t.V[i]) > sv) sv = cx_abs(a_t.V[i]);
                if (cx_abs(a_t.I[i]) > si) si = cx_abs(a_t.I[i]);
                ss += cx_abs(a_t.S[i]);
            }
            a_t.tellegen = cx_abs(cx_menos(a_t.Sfuentes, a_t.Scargas)) / (ss > sv * si ? ss : sv * si + 1e-300);
        }
        if (a_t.tellegen > peor_t) peor_t = a_t.tellegen;
        if (!(a_t.tellegen <= 1e-12)) {
            malos_t++;
            if (getenv("HAKARU_DEPURA") && malos_t <= 2) {
                printf("      Tellegen %.3e en el caso %d: Sf %.6g%+.6gj Sc %.6g%+.6gj\n", a_t.tellegen, caso,
                       a_t.Sfuentes.re, a_t.Sfuentes.im, a_t.Scargas.re, a_t.Scargas.im);
                for (i = 0; i < sr.nel; i++)
                    printf("        %2d: tipo %d a %d b %d val %.6g ctl %d ci %d instr %d | V %.6g%+.6gj I %.6g%+.6gj S %.6g%+.6gj\n", i, sr.el[i].tipo,
                           sr.el[i].a, sr.el[i].b, sr.el[i].val, sr.el[i].ctl, sr.el[i].ctl_i, sr.el[i].instr,
                           a_t.V[i].re, a_t.V[i].im, a_t.I[i].re, a_t.I[i].im, a_t.S[i].re, a_t.S[i].im);
            }
        }
    }
    printf("    (b) %d redes comparadas (%d inestables o lentas, %d que derivan, %d con un lazo o un corte): muestras %.1e; Tellegen %.1e\n",
           n_ok, n_inest, n_deriva, n_lazo, peor_m, peor_t);
    chk_cond("ctl fasor = trapecio", "las muestras del " S_uu "ltimo periodo, a 1e-9 (por pares)", malos_m == 0);
    chk_cond("ctl fasor = trapecio", "al menos 150 redes", n_ok >= 150);
    chk_cond("ctl Tellegen", "EC y JC entregan: <= 1e-12", malos_t == 0);

    /* ---- (d) errores con nombre ---- */
    {
        int kE1, kR1, kR2, kE2, kE3;
        sr_init(4);
        kE1 = sr_el(SE_E, 0, 1, 0.0); sr.el[kE1].forma = ONDA_CC; sr.el[kE1].p[0] = 2.0;
        kR1 = sr_el(SE_R, 1, 2, 1e3);
        kR2 = sr_el(SE_R, 2, 0, 1e3);
        kE2 = sr_el(SE_EC, 0, 3, 10.0); sr.el[kE2].ctl = (signed char)kR2;
        sr_el(SE_R, 3, 0, 1e3);
        chk_cond("ctl (d)", "la red base vale", sim_op_red(&sr, &v, &d) == SIM_OK && fabs(v.V[kE2] - 10.0) <= 1e-12);
        sr.el[kE2].ctl = -1;
        chk_cond("ctl (d)", "sin control: SIM_CTL_MAL, CTL_FALTA", sim_op_red(&sr, &v, &d) == SIM_CTL_MAL && d.elem == kE2 && d.campo == CTL_FALTA && d.nodo == -1);
        c4_msg_sim("ctl (d) falta", &sr, &d, "sin control");
        sr.el[kE2].ctl = (signed char)kE2;
        chk_cond("ctl (d)", "ella misma: CTL_SI_MISMA", sim_op_red(&sr, &v, &d) == SIM_CTL_MAL && d.campo == CTL_SI_MISMA);
        /* por la I de una J controlada */
        sr.el[kE2].ctl = (signed char)kR2;
        kE3 = sr_el(SE_JC, 0, 3, 1e-3); sr.el[kE3].ctl = (signed char)kR1;
        sr.el[kE2].ctl = (signed char)kE3; sr.el[kE2].ctl_i = 1;
        chk_cond("ctl (d)", "por la I de una J controlada: CTL_I_NO", sim_op_red(&sr, &v, &d) == SIM_CTL_MAL && d.elem == kE2 && d.campo == CTL_I_NO && d.nodo == kE3);
        c4_msg_sim("ctl (d) I_NO", &sr, &d, "no puede mandar");
        /* por la I de una Z, en la CA */
        sr_init(3);
        kE1 = sr_alterna(SE_E, 0, 1, 1.0, 0.0, 50.0);
        kR1 = sr_z(1, 0, 100.0, 30.0, 50.0);
        kE2 = sr_el(SE_EC, 0, 2, 10.0); sr.el[kE2].ctl = (signed char)kR1; sr.el[kE2].ctl_i = 1;
        sr_el(SE_R, 2, 0, 1e3);
        chk_cond("ctl (d)", "CA: por la I de una Z, CTL_I_NO", ca_sr(0.0) == SIM_CTL_MAL && car.d.campo == CTL_I_NO && car.d.elem == kE2);
        sr.el[kE2].ctl_i = 0;
        chk_cond("ctl (d)", "CA: por la V de una Z vale", ca_sr(0.0) == SIM_OK);
        /* dos EC en paralelo; una EC sobre una E ideal */
        sr_init(3);
        kE1 = sr_el(SE_E, 0, 1, 0.0); sr.el[kE1].forma = ONDA_CC; sr.el[kE1].p[0] = 1.0;
        kR1 = sr_el(SE_R, 1, 0, 1e3);
        kE2 = sr_el(SE_EC, 0, 2, 2.0); sr.el[kE2].ctl = (signed char)kR1;
        kE3 = sr_el(SE_EC, 0, 2, 3.0); sr.el[kE3].ctl = (signed char)kR1;
        sr_el(SE_R, 2, 0, 1e3);
        chk_cond("ctl (d)", "dos EC en paralelo: SIM_LAZO_V con las dos", sim_op_red(&sr, &v, &d) == SIM_LAZO_V &&
                 (d.lazo & (1u << kE2)) && (d.lazo & (1u << kE3)));
        sr.el[kE3].a = 0; sr.el[kE3].b = 1;
        chk_cond("ctl (d)", "una EC sobre una E ideal: SIM_LAZO_V con las dos", sim_op_red(&sr, &v, &d) == SIM_LAZO_V &&
                 (d.lazo & (1u << kE1)) && (d.lazo & (1u << kE3)));
    }

    /* ---- (d2) un C con V0 en la salida de una EC; una L sin I0 ---- */
    {
        int kE1, kR2, kE2, kC;
        for (k = 0; k < 2; k++) {
            sr_init(4);
            kE1 = sr_el(SE_E, 0, 1, 0.0); sr.el[kE1].forma = ONDA_CC; sr.el[kE1].p[0] = 1.0;
            sr_el(SE_R, 1, 2, 1e3);
            kR2 = sr_el(SE_R, 2, 0, 1e3);
            kE2 = sr_el(SE_EC, 0, 3, 10.0); sr.el[kE2].ctl = (signed char)kR2;
            kC = sr_el(SE_C, 3, 0, 1e-6);
            sr.el[kC].ci = (unsigned char)(k == 0); sr.el[kC].x0 = 0.0;
            sr_el(SE_R, 3, 0, 1e3);
            if (k == 0) {
                chk_cond("ctl (d2)", "C con V0 en la salida de la EC: CI_SALTA, no SIM_LAZO_V",
                         sim_op_red(&sr, &v, &d) == SIM_OK && (d.avisos & SIM_AV_CI_SALTA) && d.elem_aviso == kC);
                chk_t("ctl (d2)", "en el OP el C va abierto: I(C1) = 0", v.I[kC], 0.0, 1.0, 1e-15);
            } else
                chk_cond("ctl (d2)", "con V0 vac" S_ii "a, sin aviso", sim_op_red(&sr, &v, &d) == SIM_OK && !(d.avisos & SIM_AV_CI_SALTA));
            canal(0, CANAL_V, kC); canal(1, CANAL_NADA, 0);
            saj.t_fin = 1e-3; saj.h = 1e-5;
            chk_cond("ctl (d2)", "se emula", sim_emular(&sr, &saj) == SIM_OK);
            if (k == 0) chk_t("ctl (d2)", "arranca en su V0 (0 V)", sim_res()->E0[kC], 0.0, 1.0, 1e-15);
            else chk_t("ctl (d2)", "sin V0 arranca en 10" S_CDOT "V(R2) = 5 V", sim_res()->E0[kC], 0.5 * 1e-6 * 25.0, 1.25e-5, 1e-12);
            chk_t("ctl (d2)", "al final, 5 V", sim_res()->ultimo.V[kC], 5.0, 5.0, 1e-9);
        }
        /* una L sin I0 en la salida de la EC */
        sr.el[kC].tipo = SE_L; sr.el[kC].val = 1e-3; sr.el[kC].ci = 0;
        chk_cond("ctl (d2)", "una L sin I0 en la salida de la EC: L_SIN_I", sim_op_red(&sr, &v, &d) == SIM_OK && (d.avisos & SIM_AV_L_SIN_I));
    }

    /* ---- (e) Bode: E1 -> VCVS μ = 10 -> RL, plano ---- */
    {
        int kE1, kR1, kRL, i, malos = 0;
        sr_init(3);
        kE1 = sr_el(SE_E, 0, 1, 0.0); sr.el[kE1].forma = ONDA_CC; sr.el[kE1].p[0] = 1.0;
        kR1 = sr_el(SE_R, 1, 0, 1e3);
        k = sr_el(SE_EC, 0, 2, 10.0); sr.el[k].ctl = (signed char)kE1;
        kRL = sr_el(SE_R, 2, 0, 1e3);
        (void)kR1;
        canal(0, CANAL_V, kRL);
        chk_cond("ctl Bode", "se barre", bode_sr(kE1, 0, -1, -1, 0.0, 0.0, 0.0) == SIM_OK && bode_res()->fin == FIN_COMPLETO);
        for (i = 0; i < bode_reg()->n; i++) if (cx_abs(cx_menos(bode_reg()->H[i], cx(10.0, 0.0))) > 1e-12 * 10.0) malos++;
        chk_cond("ctl Bode", "H = 10 en todo el barrido (1e-12)", malos == 0 && bode_reg()->n == BODE_N);
    }

    /* ---- (f) la calculadora de CA con controladas ---- */
    {
        static Circuito c;
        const CalRes *cs;
        int kE1, kE2, kRL;
        /* E1 fasor con R1; E2 = 10·V(E1) sobre RL (tierra común) */
        circ_init(&c, "VCVS CA");
        kE1 = circ_elemento(&c, EL_FUENTE, 0, 1, DIR_ARR);
        pon_val(&c, kE1, 1.0); c.el[kE1].variante = FORMA_FASOR;
        c.el[kE1].val.ang = 0.0; c.el[kE1].val.est_ang = M_DATO;
        c.frec.v = 50.0; c.frec.est = M_DATO;
        pon_val(&c, circ_elemento(&c, EL_RES, 0, 0, DIR_DER), 100.0);
        circ_cable(&c, 1, 0, DIR_ABA); circ_cable(&c, 1, 1, DIR_IZQ); circ_cable(&c, 1, 1, DIR_DER);
        kE2 = circ_elemento(&c, EL_EDEP, 2, 1, DIR_ARR);
        c.el[kE2].variante = DEP_V; c.el[kE2].ref = (signed char)kE1;
        kRL = pon_val(&c, circ_elemento(&c, EL_RES, 2, 0, DIR_DER), 1e3);
        circ_cable(&c, 3, 0, DIR_ABA); circ_cable(&c, 3, 1, DIR_IZQ);
        chk_cond("ctl CA", "el circuito es v" S_aa "lido", circ_valido(&c) && kE2 >= 0 && kRL >= 0);
        c.el[kE2].val.est = M_BUSCAR;
        cs = tk_calcula(&c);
        chk_cond("ctl CA", "la ganancia [?!]: CAL_NO_CA", cs->estado == CAL_NO_CA && cs->elem == kE2);
        tk_msg("ctl CA", &c, cs, "ganancia");
        c.el[kE2].val.est = M_LIBRE;
        cs = tk_calcula(&c);
        chk_cond("ctl CA", "la ganancia vac" S_ii "a: SIM_FALTA", cs->estado == CAL_SIM && cs->d.estado == SIM_FALTA && cs->elem == kE2);
        pon_val(&c, kE2, 10.0);
        c.el[kE1].val.est = M_BUSCAR; c.el[kE1].val.est_ang = M_BUSCAR;
        c.el[kRL].V.v = 10.0; c.el[kRL].V.est = M_DATO; c.el[kRL].V.ang = 0.0; c.el[kRL].V.est_ang = M_DATO;
        cs = tk_calcula(&c);
        chk_cond("ctl CA", "E1 [?!] con V(RL) = 10" S_ANG "0" S_DEG ": una soluci" S_oo "n", cs->estado == CAL_OK && cs->nsol == 1 && cs->ninc == 2);
        if (cs->nsol >= 1) {
            chk_t("ctl CA", "|E1| = 1 V", cs->sol[0].x[0], 1.0, 1.0, 1e-12);
            chk_t("ctl CA", S_ANG "E1 = 0" S_DEG, cs->sol[0].x[1], 0.0, 180.0, 1e-12);
        }
    }
    semilla = sem0; azar_ca = azar0;
}

static void test_e4_ctl(void)
{
    test_e4_ctl_tabla();
    test_e4_ctl_motor();
}
