/* --------------------------------------------------------------------------
 *  61. Etapa 4: el amplificador operacional (paso 7)
 *
 *  Se incluye en host_test.c tras test_e4_trans.inc (usa sr, saj, sr_init,
 *  sr_el, canal, emula, t4_seno, t4_c0, t4_balance, c4_*, rc, rr, cierra_red
 *  y los chk). Los números son los del §6.8 de la especificación.
 * ----------------------------------------------------------------------- */

/* Un AO en sr: + en mas, - en menos, la salida en sal medida desde ref */
static int a4_ao(int mas, int menos, int sal, int ref, int real, double A)
{
    int k = sr_el(SE_AO, mas, menos, 0.0);
    SimEl *e = &sr.el[k];
    e->c = (short)sal; e->nref = (short)ref;
    e->modelo = real ? NL_AO_REAL : NL_AO_IDEAL;
    e->m[0] = real ? A : 0.0; e->m[1] = 15.0; e->m[2] = -15.0;
    return k;
}

static int a4_cc(int a, int b, double V)
{
    int k = sr_el(SE_E, a, b, 0.0);
    sr.el[k].forma = ONDA_CC; sr.el[k].p[0] = V;
    return k;
}

/* El inversor: E1 (1) -> R1 1k -> (2) - ; Rf 10k de (2) a la salida (3); + a tierra */
static int a4_inversor(double E, int real, double A, int *kR1, int *kRf)
{
    sr_init(4);
    a4_cc(0, 1, E);
    *kR1 = sr_el(SE_R, 1, 2, 1e3);
    *kRf = sr_el(SE_R, 2, 3, 10e3);
    return a4_ao(0, 2, 3, 0, real, A);
}

/* La salida en el OP, o NaN si no hay */
static double a4_vsal(const char *q, int k)
{
    static Valores v;
    SimDiag d;
    if (sim_op_red(&sr, &v, &d) != SIM_OK) {
        char m[200];
        sim_mensaje(&sr, &d, m, sizeof m);
        falla(q, m, d.estado, SIM_OK);
        return 0.0 / 0.0;
    }
    return v.V[k];
}

/* Una red lineal al azar con AO (§6.8): 0 inversor, 1 no inversor, 2 sumador,
 * 3 diferencial, 4 cadena de dos inversores. Devuelve el número de AO. */
static int a4_red_azar(int tipo, int real, double A, unsigned *sem)
{
    double r[6];
    int i;
    for (i = 0; i < 6; i++) { *sem = *sem * 1103515245u + 12345u; r[i] = 1e3 * (1.0 + (double)((*sem >> 8) % 1000) / 100.0); }
    *sem = *sem * 1103515245u + 12345u;
    switch (tipo) {
    case 0:
        sr_init(4); a4_cc(0, 1, 0.1 + 0.9 * (double)((*sem >> 8) % 1000) / 1000.0);
        sr_el(SE_R, 1, 2, r[0]); sr_el(SE_R, 2, 3, r[1]); sr_el(SE_R, 3, 0, r[2]);
        a4_ao(0, 2, 3, 0, real, A);
        return 1;
    case 1:
        sr_init(4); a4_cc(0, 1, 0.1 + 0.9 * (double)((*sem >> 8) % 1000) / 1000.0);
        sr_el(SE_R, 2, 0, r[0]); sr_el(SE_R, 2, 3, r[1]); sr_el(SE_R, 3, 0, r[2]);
        a4_ao(1, 2, 3, 0, real, A);
        return 1;
    case 2:
        sr_init(5); a4_cc(0, 1, 0.3); a4_cc(0, 2, -0.2);
        sr_el(SE_R, 1, 3, r[0]); sr_el(SE_R, 2, 3, r[1]); sr_el(SE_R, 3, 4, r[2]); sr_el(SE_R, 4, 0, r[3]);
        a4_ao(0, 3, 4, 0, real, A);
        return 1;
    case 3:
        sr_init(6); a4_cc(0, 1, 0.5); a4_cc(0, 2, 0.2);
        sr_el(SE_R, 1, 3, r[0]); sr_el(SE_R, 3, 5, r[1]);
        sr_el(SE_R, 2, 4, r[2]); sr_el(SE_R, 4, 0, r[3]); sr_el(SE_R, 5, 0, r[4]);
        a4_ao(4, 3, 5, 0, real, A);
        return 1;
    default:
        sr_init(6); a4_cc(0, 1, 0.1 + 0.4 * (double)((*sem >> 8) % 1000) / 1000.0);
        sr_el(SE_R, 1, 2, r[0]); sr_el(SE_R, 2, 3, r[1]);
        sr_el(SE_R, 3, 4, r[2]); sr_el(SE_R, 4, 5, r[3]); sr_el(SE_R, 5, 0, r[4]);
        a4_ao(0, 2, 3, 0, real, A);
        a4_ao(0, 4, 5, 0, real, A);
        return 2;
    }
}

static void test_e4_ao(void)
{
    static Valores v, vi, v7, v8;
    static SimRed ideal;
    SimDiag d;
    SimMedidas me;
    int k, kR1, kRf, i;
    char m[300];

    printf("[61] Etapa 4: el amplificador operacional\n");

    /* ---- Inversor ---- */
    k = a4_inversor(1.0, 0, 0.0, &kR1, &kRf);
    chk_t("inversor ideal", "Vsal = -10", a4_vsal("inversor ideal", k), -10.0, 10.0, 1e-12);
    sim_op_red(&sr, &v, &d);
    chk_t("inversor ideal", "I(U1) = Vsal/Rf = -1 mA (la ley de nodos de su rama)", v.I[k], -1e-3, 1e-3, 1e-12);
    chk_t("inversor ideal", "I(R1) = I(Rf): por - no entra nada", v.I[kR1], v.I[kRf], 1e-3, 1e-12);
    chk_cond("inversor ideal", "en su lineal, sin avisos", v.reg[k] == REG_LINEAL && !(d.avisos & (SIM_AV_SATURA | SIM_AV_BIESTABLE)));
    {
        static const double A[4] = { 1e5, 2e5, 1e7, 1e8 };
        static const double V[4] = { -9.9989001209866915, -9.9994500302483363, -9.9999890000121, -9.999998900000121 };
        for (i = 0; i < 4; i++) {
            sprintf(m, "inversor A = %g", A[i]);
            k = a4_inversor(1.0, 1, A[i], &kR1, &kRf);
            chk_t(m, "Vsal", a4_vsal(m, k), V[i], 10.0, 1e-12);
        }
    }
    k = a4_inversor(1.6, 1, 1e5, &kR1, &kRf);
    sim_op_red(&sr, &v, &d);
    chk_t("inversor 1,6 V", "satura: Vsal = -15", v.V[k], -15.0, 15.0, 1e-12);
    chk_t("inversor 1,6 V", "V- = 0,0909...", v.phi[2], 0.090909090909090909, 0.1, 1e-12);
    chk_cond("inversor 1,6 V", "SAT- y el aviso SATURA", v.reg[k] == REG_SAT_MENOS && (d.avisos & SIM_AV_SATURA));
    k = a4_inversor(1.5, 1, 1e5, &kR1, &kRf);
    chk_cond("inversor 1,5 V", "a" S_uu "n lineal (hasta 1,500165)", fabs(a4_vsal("inversor 1,5 V", k)) < 15.0);
    k = a4_inversor(1.5002, 1, 1e5, &kR1, &kRf);
    chk_t("inversor 1,5002 V", "ya satura", a4_vsal("inversor 1,5002 V", k), -15.0, 15.0, 1e-12);

    /* ---- No inversor, seguidor, sumador ---- */
    sr_init(4); a4_cc(0, 1, 1.0); sr_el(SE_R, 2, 0, 1e3); sr_el(SE_R, 2, 3, 9e3); k = a4_ao(1, 2, 3, 0, 0, 0.0);
    chk_t("no inversor ideal", "Vsal = 10", a4_vsal("no inversor", k), 10.0, 10.0, 1e-12);
    sr.el[k].modelo = NL_AO_REAL; sr.el[k].m[0] = 1e5;
    chk_t("no inversor A = 1e5", "Vsal", a4_vsal("no inversor 1e5", k), 9.999000099990001, 10.0, 1e-12);
    sr_init(3); a4_cc(0, 1, 1.0); k = a4_ao(1, 2, 2, 0, 1, 1e5);
    chk_t("seguidor A = 1e5", "Vsal", a4_vsal("seguidor", k), 0.999990000099999, 1.0, 1e-12);
    sr_init(5); a4_cc(0, 1, 1.0); a4_cc(0, 2, 2.0);
    sr_el(SE_R, 1, 3, 10e3); sr_el(SE_R, 2, 3, 10e3); sr_el(SE_R, 3, 4, 10e3); k = a4_ao(0, 3, 4, 0, 0, 0.0);
    chk_t("sumador ideal", "Vsal = -3", a4_vsal("sumador", k), -3.0, 3.0, 1e-12);
    sr.el[k].modelo = NL_AO_REAL; sr.el[k].m[0] = 1e5;
    chk_t("sumador A = 1e5", "Vsal", a4_vsal("sumador 1e5", k), -2.999910002699919, 3.0, 1e-12);

    /* ---- 200 redes lineales al azar: orden 1 en 1/A ---- */
    {
        unsigned sem = 8642u;
        int n_ok = 0, malos = 0, malos_ord = 0, malos_v = 0;
        for (i = 0; i < 400 && n_ok < 200; i++) {
            unsigned s0 = sem;
            double esc = 1.0, e7 = 0.0, e8 = 0.0, dv = 0.0;
            int n, tipo = i % 5, nao, ok = 1;
            nao = a4_red_azar(tipo, 0, 0.0, &sem);
            ideal = sr;
            if (sim_op_red(&sr, &vi, &d) != SIM_OK) continue;
            for (k = 0; k < sr.nel; k++) if (sr.el[k].tipo == SE_AO && vi.reg[k] != REG_LINEAL) ok = 0;
            if (!ok) continue;
            sem = s0; a4_red_azar(tipo, 1, 1e7, &sem);
            if (sim_op_red(&sr, &v7, &d) != SIM_OK) continue;
            sem = s0; a4_red_azar(tipo, 1, 1e8, &sem);
            if (sim_op_red(&sr, &v8, &d) != SIM_OK) continue;
            for (n = 0; n < sr.nn; n++) if (fabs(vi.phi[n]) > esc) esc = fabs(vi.phi[n]);
            for (n = 0; n < sr.nn; n++) {
                if (fabs(v7.phi[n] - vi.phi[n]) > e7) e7 = fabs(v7.phi[n] - vi.phi[n]);
                if (fabs(v8.phi[n] - vi.phi[n]) > e8) e8 = fabs(v8.phi[n] - vi.phi[n]);
            }
            for (k = 0; k < ideal.nel; k++)
                if (ideal.el[k].tipo == SE_AO) {
                    double d0 = fabs(vi.phi[ideal.el[k].a] - vi.phi[ideal.el[k].b]);
                    if (d0 > dv) dv = d0;
                }
            (void)nao;
            n_ok++;
            if (!(e7 <= 1e-4 * esc)) malos++;
            if (!(e8 <= 0.12 * e7 + 1e-12)) malos_ord++;
            if (!(dv <= 1e-15 * esc)) malos_v++;
        }
        printf("    %d redes lineales con AO: ideal frente a A = 1e7 y 1e8 (orden 1 en 1/A)\n", n_ok);
        chk_cond("AO al azar", "200 redes", n_ok >= 200);
        chk_cond("AO al azar", "|x(1e7) - x(ideal)| <= 1e-4 de la escala", malos == 0);
        chk_cond("AO al azar", "|x(1e8) - x(ideal)| <= 0,12 |x(1e7) - x(ideal)| (orden 1)", malos_ord == 0);
        chk_cond("AO al azar", "en el ideal, V+ = V- a 1e-15", malos_v == 0);
    }

    /* ---- La regla: Schmitt y comparador ---- */
    sr_init(4);
    a4_cc(0, 1, 0.0);                                  /* la entrada, en - */
    sr_el(SE_R, 3, 2, 10e3); sr_el(SE_R, 2, 0, 10e3); /* V+ = Vsal/2 */
    k = a4_ao(2, 1, 3, 0, 0, 0.0);
    sim_op_red(&sr, &v, &d);
    chk_cond("Schmitt", "el lineal no vale: empieza en +15 V con BIESTABLE", d.estado == SIM_OK && fabs(v.V[k] - 15.0) < 1e-12 &&
             (d.avisos & SIM_AV_BIESTABLE));
    sr_init(4); a4_cc(0, 1, 1.0); i = a4_cc(0, 2, 0.5); k = a4_ao(1, 2, 3, 0, 0, 0.0);
    sim_op_red(&sr, &v, &d);
    chk_cond("comparador", "sin realimentaci" S_oo "n: a la saturaci" S_oo "n del signo de Vd (+)", d.estado == SIM_OK && v.reg[k] == REG_SAT_MAS &&
             fabs(v.V[k] - 15.0) < 1e-12);
    sr.el[i].p[0] = 1.5;                               /* ahora Vd < 0 */
    sim_op_red(&sr, &v, &d);
    chk_cond("comparador", "con Vd < 0, a -15 V", d.estado == SIM_OK && v.reg[k] == REG_SAT_MENOS && fabs(v.V[k] + 15.0) < 1e-12);

    /* ---- Integrador ideal: -t/1 ms, satura en 15 ms ---- */
    {
        const SimReg *g;
        int kE, kC, j, fin = -1;
        sr_init(4);
        kE = sr_el(SE_E, 0, 1, 0.0); sr.el[kE].forma = ONDA_PULSO; sr.el[kE].p[1] = 1.0;
        sr_el(SE_R, 1, 2, 10e3);
        kC = t4_c0(2, 3, 100e-9);
        k = a4_ao(0, 2, 3, 0, 0, 0.0);
        chk_cond("integrador", "OP sin CI_SALTA y en su lineal", sim_op_red(&sr, &v, &d) == SIM_OK && !(d.avisos & SIM_AV_CI_SALTA) &&
                 v.reg[k] == REG_LINEAL);
        canal(0, CANAL_NODO, 3); canal(1, CANAL_V, kC);
        saj.h = 1e-4; saj.t_fin = 20e-3;
        emula("integrador");
        g = sim_reg();
        for (j = 0; j < g->n; j++) if (g->y[0][j] <= -15.0 + 1e-9) { fin = j; break; }
        chk_cond("integrador", "satura", fin >= 0);
        if (fin >= 0) chk_t("integrador", "satura en t = 15 ms", g->t[fin], 15e-3, 15e-3, 1e-9);
        for (j = 0; j < g->n; j++)
            if (g->t[j] > 0.0 && g->t[j] < 14e-3) { chk_t("integrador", "Vsal = -t/1 ms", g->y[0][j], -g->t[j] / 1e-3, 15.0, 1e-9); break; }
        chk_t("integrador", "al final, -15 V", g->y[0][g->n - 1], -15.0, 15.0, 1e-12);
    }

    /* ---- Derivador ideal: cuadrada de ±0,4 V ---- */
    for (i = 0; i < 2; i++) {
        int kE, kC;
        const char *q = i ? "derivador V0 = 0" : "derivador V0 vac" S_ii "a";
        sr_init(4);
        kE = sr_el(SE_E, 0, 1, 0.0); sr.el[kE].forma = ONDA_TRIANGULAR;
        sr.el[kE].p[0] = 1.0; sr.el[kE].p[1] = 1e3; sr.el[kE].p[2] = 50.0;
        kC = sr_el(SE_C, 1, 2, 100e-9);
        if (i) { sr.el[kC].ci = 1; sr.el[kC].x0 = 0.0; }
        sr_el(SE_R, 2, 3, 1e3);
        k = a4_ao(0, 2, 3, 0, 0, 0.0);
        chk_cond(q, "OP: el AO en su lineal", sim_op_red(&sr, &v, &d) == SIM_OK && v.reg[k] == REG_LINEAL);
        chk_cond(q, i ? "CI_SALTA (el segundo recorrido)" : "sin avisos", i ? (d.avisos & SIM_AV_CI_SALTA) != 0 : d.avisos == 0);
        canal(0, CANAL_NODO, 3); canal(1, CANAL_NADA, 0);
        saj.h = 1e-5; saj.t_fin = 5e-3;
        emula(q);
        sim_medir(0, 4e-3, 5e-3, &me);
        if (getenv("HAKARU_DEPURA")) {
            const SimReg *g = sim_reg();
            int j;
            for (j = 0; j < g->n; j++)
                if (g->t[j] >= 3.9e-3 && fabs(fabs(g->y[0][j]) - 0.4) > 1e-9)
                    printf("      %s: t = %.9g (%.4f T) y = %.12g\n", q, g->t[j], g->t[j] / 1e-3, g->y[0][j]);
        }
        chk_t(q, "m" S_aa "ximo del " S_uu "ltimo periodo 0,4", me.max, 0.4, 0.4, 1e-9);
        chk_t(q, "m" S_ii "nimo del " S_uu "ltimo periodo -0,4", me.min, -0.4, 0.4, 1e-9);
    }

    /* ---- Astable: T = 2RC·ln 3 ---- */
    for (i = 0; i < 2; i++) {
        const SimReg *g;
        int kC, j, primero = -1;
        const char *q = i ? "astable V0 = 0" : "astable V0 vac" S_ii "a";
        sr_init(4);
        sr_el(SE_R, 3, 1, 10e3);
        kC = sr_el(SE_C, 1, 0, 100e-9);
        if (i) { sr.el[kC].ci = 1; sr.el[kC].x0 = 0.0; }
        sr_el(SE_R, 3, 2, 10e3); sr_el(SE_R, 2, 0, 10e3);
        k = a4_ao(2, 1, 3, 0, 0, 0.0);
        sim_op_red(&sr, &v, &d);
        if (i) chk_cond(q, "OP: BIESTABLE y +15 V", (d.avisos & SIM_AV_BIESTABLE) && fabs(v.V[k] - 15.0) < 1e-12);
        else chk_cond(q, "OP: el lineal vale en CC", v.reg[k] == REG_LINEAL);
        canal(0, CANAL_NODO, 3); canal(1, CANAL_V, kC);
        saj.h = 10e-6; saj.t_fin = 10e-3;
        emula(q);
        if (!i) chk_cond(q, "aviso ARRANQUE", (sim_res()->d.avisos & SIM_AV_ARRANQUE) != 0);
        g = sim_reg();
        for (j = 1; j < g->n; j++) if (g->y[0][j] < 0.0 && g->y[0][j - 1] > 0.0) { primero = j; break; }
        chk_cond(q, "cambia", primero > 0);
        if (primero > 0) chk_t(q, "el primer cambio en RC" S_CDOT "ln 2", g->t[primero], 0.69314718055994531e-3, 0.69e-3, 1e-4);
        sim_medir(0, 2e-3, 10e-3, &me);
        chk_t(q, "f = 455,1196 Hz", me.frec, 455.1196133134187, 455.0, 1e-4);
        sim_medir(1, 2e-3, 10e-3, &me);
        chk_t(q, "vC hasta +7,5 V", me.max, 7.5, 7.5, 1e-4);
        chk_t(q, "vC hasta -7,5 V", me.min, -7.5, 7.5, 1e-4);
        {
            int ev = sim_res()->nw.eventos, esperados = (int)(2.0 * 10e-3 / 2.1972245773362194e-3);
            pruebas++;
            if (abs(ev - esperados) > 2) falla(q, "2 cambios por periodo (sin NC_CONMUTA)", ev, esperados);
        }
    }

    /* ---- Comparador ideal en el tiempo ---- */
    {
        const SimReg *g;
        int j, n_fl = 0, malos_fl = 0;
        const double T = 1e-3;
        sr_init(4);
        t4_seno(0, 1, 1.0, 1e3, 0.0);
        a4_cc(0, 2, 0.5);
        k = a4_ao(1, 2, 3, 0, 0, 0.0);
        canal(0, CANAL_NODO, 3); canal(1, CANAL_NADA, 0);
        saj.h = T / 100.0; saj.t_fin = 5e-3;
        emula("comparador");
        sim_medir(0, 4e-3, 5e-3, &me);
        chk_t("comparador", "media del " S_uu "ltimo periodo -5 V", me.media, -5.0, 5.0, 1e-9);
        sim_medir(0, 1e-3, 5e-3, &me);
        chk_t("comparador", "f = 1000 Hz", me.frec, 1000.0, 1000.0, 1e-9);
        g = sim_reg();
        for (j = 1; j < g->n; j++)
            if ((g->y[0][j] > 0.0) != (g->y[0][j - 1] > 0.0) && g->t[j] > 1e-3) {
                double ph = fmod(g->t[j], T) / T, err;
                err = fabs(ph - 1.0 / 12.0) < fabs(ph - 5.0 / 12.0) ? fabs(ph - 1.0 / 12.0) : fabs(ph - 5.0 / 12.0);
                n_fl++;
                if (err > 1e-11) malos_fl++;
            }
        chk_cond("comparador", "flancos en T/12 y 5T/12 a 1e-11" S_CDOT "T", n_fl >= 6 && malos_fl == 0);
    }

    /* ---- Zenón: la regla dice siempre «no vale» desde 1 ms ---- */
    sr_init(3); a4_cc(0, 1, 1.0); k = a4_ao(1, 2, 2, 0, 0, 0.0);
    canal(0, CANAL_NODO, 2); canal(1, CANAL_NADA, 0);
    saj.h = 1e-4; saj.t_fin = 3e-3;
    sim_nl_prueba(NLP_INESTABLE, 1e-3);
    sim_emular(&sr, &saj);
    sim_nl_prueba(NLP_NADA, 0.0);
    chk_cond("Zen" S_oo "n", "NO_CONVERGE con NC_CONMUTA", sim_res()->d.estado == SIM_NO_CONVERGE && sim_res()->d.campo == NC_CONMUTA);
    chk_t("Zen" S_oo "n", "en t = 1 ms", sim_res()->d.t, 1e-3, 1e-3, 1e-9);
    sim_mensaje(&sr, &sim_res()->d, m, sizeof m);
    chk_cond("Zen" S_oo "n", "su mensaje", lineas(m, 52) <= 4 && strstr(m, "conmuta sin parar") != 0);

    /* ---- Errores ---- */
    sr_init(3); a4_cc(0, 1, 1.0); k = a4_ao(1, 2, 2, 0, 0, 0.0);
    a4_cc(0, 2, 2.0);                                  /* la salida atada a una E ideal */
    chk_cond("errores", "la salida sobre una E ideal: SIM_LAZO_V con los dos",
             sim_op_red(&sr, &v, &d) == SIM_LAZO_V && (d.lazo & (1u << k)) && (d.lazo & (1u << (sr.nel - 1))));
    sr_init(3); a4_cc(0, 1, 1.0); k = a4_ao(1, 2, 2, 0, 0, 0.0);
    i = sr_el(SE_AMI, 2, 0, 0.0);
    chk_cond("errores", "un A ideal de la salida a tierra: SIM_LAZO_V", sim_op_red(&sr, &v, &d) == SIM_LAZO_V && (d.lazo & (1u << i)));
    sr_init(3); a4_cc(0, 1, 1.0); k = a4_ao(1, 2, 2, 0, 0, 0.0);
    i = t4_c0(2, 0, 1e-6);
    chk_cond("errores", "un C con V0 = 0 en la salida del seguidor: CI_SALTA y Vsal = 1 V",
             sim_op_red(&sr, &v, &d) == SIM_OK && (d.avisos & SIM_AV_CI_SALTA) && fabs(v.V[k] - 1.0) < 1e-12);
    canal(0, CANAL_V, i); canal(1, CANAL_NADA, 0);
    saj.h = 1e-5; saj.t_fin = 1e-4;
    emula("seguidor con C");
    chk_t("errores", "el C arranca en 0 V", sim_res()->E0[i], 0.0, 1.0, 1e-15);
    sr_init(3); a4_cc(0, 1, 1.0); k = a4_ao(1, 2, 2, 0, 0, 0.0);
    sr.el[k].m[1] = -15.0; sr.el[k].m[2] = 15.0;
    chk_cond("errores", "V+ <= V-: SIM_MODELO con QA_VP", sim_op_red(&sr, &v, &d) == SIM_MODELO && d.campo == QA_VP);
    sim_mensaje(&sr, &d, m, sizeof m);
    chk_cond("errores", "su texto", strstr(m, "V+ tiene que ser mayor") != 0);

    /* ---- Sin referencia, CTL_3PATAS y la tabla: con el Circuito ---- */
    {
        static Solucion so;
        int ka, ke;
        /* el astable sin tierra ni fuentes */
        c4_init(4);
        c4_el(EL_RES, 3, 1, 10e3); c4_el(EL_COND, 1, 0, 100e-9); c4_el(EL_RES, 3, 2, 10e3); c4_el(EL_RES, 2, 0, 10e3);
        ka = nuevo_el(EL_AO, 2, 1);
        for (i = 0; i < rc.nel; i++) rr.c[i] = -1;
        rr.c[ka] = 3;
        cierra_red();
        chk_cond("SIM_AO_REF", "sin tierra ni fuente: AOR_SIN_TIERRA", sim_punto_op_cc(&rc, &rr, &v, &d, 0) == SIM_AO_REF &&
                 d.campo == AOR_SIN_TIERRA && d.elem == ka);
        sim_mensaje_circ(&rc, 0, &d, m, sizeof m);
        chk_cond("SIM_AO_REF", "su texto", lineas(m, 52) <= 4 && strstr(m, "[G]") != 0);
        rc.tierra_x = 0; rc.tierra_y = 0;                  /* rr.punto: todo en el nodo 0 */
        cierra_red();
        chk_cond("SIM_AO_REF", "con tierra vale", sim_punto_op_cc(&rc, &rr, &v, &d, 0) == SIM_OK);
        rr.c[ka] = 0;
        cierra_red();
        chk_cond("SIM_AO_REF", "la salida en la tierra: AOR_CORTO", sim_punto_op_cc(&rc, &rr, &v, &d, 0) == SIM_AO_REF && d.campo == AOR_CORTO);
        /* un inversor sin tierra pero con E1: la referencia es el - de E1 */
        c4_init(4);
        c4_el(EL_FUENTE, 0, 1, 1.0); c4_el(EL_RES, 1, 2, 1e3); c4_el(EL_RES, 2, 3, 10e3);
        ka = nuevo_el(EL_AO, 0, 2);
        for (i = 0; i < rc.nel; i++) rr.c[i] = -1;
        rr.c[ka] = 3;
        cierra_red();
        chk_cond("SIM_AO_REF", "un inversor sin tierra con E1 vale", sim_punto_op_cc(&rc, &rr, &v, &d, 0) == SIM_OK &&
                 fabs(v.V[ka] + 10.0) < 1e-12);
        /* CTL_3PATAS y SOL_NO_LINEAL */
        ke = c4_dep(EL_EDEP, 0, 3, DEP_V, 2.0, ka);
        nodos = 4;
        rr.c[ke] = -1;
        c4_el(EL_RES, 3, 0, 1e3);
        cierra_red();
        chk_cond("CTL_3PATAS", "el emulador", sim_punto_op_cc(&rc, &rr, &v, &d, 0) == SIM_CTL_MAL && d.campo == CTL_3PATAS && d.elem == ke);
        chk_cond("CTL_3PATAS", "circ_ctl_motivo", circ_ctl_motivo(&rc, ke) == CTL_3PATAS);
        resolver(&rc, &rr, &so);
        chk_cond("SOL_NO_LINEAL", "con un AO", so.estado == SOL_NO_LINEAL && so.elem == ka);
        sol_mensaje(&rc, &rr, &so, m, sizeof m);
        chk_cond("SOL_NO_LINEAL", "su texto", strstr(m, "amplificador operacional") != 0 && lineas(m, 52) <= 4);
        /* los canales automáticos: «Vsal(U1)» y V(E1) */
        rc.nel--; rc.nel--;
        cierra_red();
        {
            SimCanal ch[2];
            sim_canales(&rc, &rr, ch);
            chk_cond("canales", "Vsal(U1) y V(E1)", ch[0].tipo == CANAL_NODO && strncmp(ch[0].nombre, "Vsal(", 5) == 0 &&
                     ch[1].tipo == CANAL_V && ch[1].elem == 0);
        }
    }
}
