/* --------------------------------------------------------------------------
 *  59. Etapa 4: el transitorio por tramos (paso 5)
 *  60. Etapa 4: el transitorio de los suaves (paso 6)
 *
 *  Se incluye en host_test.c tras test_e4_diodo.inc (usa sr, saj, sr_init,
 *  sr_el, canal, emula, chk_t, chk_cond, pruebas y falla). Las referencias
 *  del [59] son los regímenes cerrados de la especificación (§6.6); las del
 *  [60], una integración RK4 fina del arnés (la unión con Rs por Newton).
 * ----------------------------------------------------------------------- */
#define T4_PI 3.14159265358979323846

static int t4_seno(int a, int b, double Vp, double f, double fase)
{
    int k = sr_el(SE_E, a, b, 0.0);
    sr.el[k].forma = ONDA_SENO;
    sr.el[k].p[0] = Vp; sr.el[k].p[1] = f; sr.el[k].p[2] = fase;
    return k;
}

static int t4_d_vg(int a, int b, double vg)
{
    int k = sr_el(SE_D, a, b, 0.0);
    sr.el[k].modelo = NL_TRAMOS;
    sr.el[k].m[0] = vg; sr.el[k].m[1] = NL_RON; sr.el[k].m[3] = NL_RON;
    return k;
}

static int t4_d_cat(int a, int b, const NlDiodo *c)
{
    int k = sr_el(SE_D, a, b, 0.0);
    sr.el[k].modelo = NL_UNION;
    sr.el[k].m[0] = c->Is; sr.el[k].m[1] = c->n; sr.el[k].m[2] = c->Rs;
    sr.el[k].m[5] = nl_vcrit(c->Is, c->n * NL_VT);
    return k;
}

static int t4_c0(int a, int b, double C)
{
    int k = sr_el(SE_C, a, b, C);
    sr.el[k].ci = 1; sr.el[k].x0 = 0.0;
    return k;
}

/* El balance. Tellegen en los puntos registrados (la entregada, W de las
 * fuentes, es la absorbida, W de todo lo demás, C y L incluidos) es exacto:
 * KCL y KVL valen en cada punto, también en los extrapolados tras un evento,
 * que son combinaciones de soluciones de la misma red. dif_balance compara
 * además la W integrada de C y L con ½CΔv²: el trapecio la da exacta, pero
 * el arranque de Euler tras cada evento no (como en las redes lineales: el
 * [34] solo la exige exacta sin eventos); con ~12 eventos queda en ~1e-5. */
static void t4_balance(const char *q, double tol)
{
    const SimRes *r = sim_res();
    double ent = 0.0, abs_ = 0.0;
    int k;
    for (k = 0; k < sr.nel; k++) {
        int t = sr.el[k].tipo;
        if (t == SE_E || t == SE_J || t == SE_EC || t == SE_JC) ent += r->W[k];
        else if (t != SE_AMI) abs_ += r->W[k];
    }
    pruebas += 2;
    if (!(fabs(ent - abs_) <= 1e-9 * (fabs(ent) > 0.0 ? fabs(ent) : 1.0))) falla(q, "Tellegen en los puntos (1e-9)", ent - abs_, 0.0);
    if (!(fabs(r->dif_balance) <= tol * (fabs(ent) > 0.0 ? fabs(ent) : 1.0))) falla(q, "dif_balance", r->dif_balance, 0.0);
}

/* Trozos: 1, 997 y sin tope dan lo mismo bit a bit */
static void t4_trozos(const char *q)
{
    static SimReg g0;
    static SimRes r0;
    static const unsigned long OPS[2] = { 1ul, 997ul };
    int t;
    sim_emular(&sr, &saj);
    g0 = *sim_reg(); r0 = *sim_res();
    for (t = 0; t < 2; t++) {
        sim_iniciar_red(&sr, &saj);
        while (sim_avanzar(OPS[t])) {}
        chk_cond(q, t ? "997 operaciones por trozo: bit a bit" : "1 operaci" S_oo "n por trozo: bit a bit",
                 sim_reg()->n == g0.n && memcmp(sim_reg()->t, g0.t, sizeof g0.t) == 0 &&
                 memcmp(sim_reg()->y, g0.y, sizeof g0.y) == 0 && memcmp(sim_res(), &r0, sizeof r0) == 0);
    }
}

/* El régimen cerrado de un rectificador con C ∥ R (Ron = 0, GMIN = 0): en
 * conducción Vp·|sen θ| - Vd entre θ_on y θ_off; si no, la descarga desde
 * θ_off. per = π (onda completa) o 2π (media onda) */
static double t4_cerrada(double th, double Vp, double Vd, double on, double off, double wrc, double per)
{
    double voff = Vp * sin(off) - Vd;
    th = fmod(th, per);
    if (th < 0.0) th += per;
    if (th >= on && th <= off) return Vp * fabs(sin(th)) - Vd;
    return voff * exp(-((th > off ? th : th + per) - off) / wrc);
}

typedef struct { const char *nom; int tipo; double Vp, Vd, vmin, med, on, off; } T4Rect;

static void t4_rectificador(const T4Rect *c)
{
    const double f = 60.0, T = 1.0 / f, w = 2.0 * T4_PI * f, C = 100e-6, R = 1e3;
    const SimReg *g;
    SimMedidas me;
    double peor = 0.0, on = -1.0, off = -1.0, thr = 1e-6;
    int kC, kD, i, conduce = 0;
    const double per = c->tipo <= 1 ? 2.0 * T4_PI : T4_PI;
    /* 0: media onda con Vγ; 1: media onda ideal; 2: onda completa; 3: puente */
    if (c->tipo <= 1) {
        sr_init(3);
        t4_seno(0, 1, c->Vp, f, 0.0);
        kD = c->tipo == 0 ? t4_d_vg(1, 2, 0.7) : t4_d_vg(1, 2, 0.0);
        kC = t4_c0(2, 0, C);
        sr_el(SE_R, 2, 0, R);
    } else if (c->tipo == 2) {
        sr_init(4);
        t4_seno(0, 1, c->Vp, f, 0.0);
        t4_seno(0, 3, c->Vp, f, 180.0);
        kD = t4_d_vg(1, 2, 0.7);
        t4_d_vg(3, 2, 0.7);
        kC = t4_c0(2, 0, C);
        sr_el(SE_R, 2, 0, R);
    } else {                                           /* A = 0, B = 1, O = 2, G = 3 */
        sr_init(4);
        t4_seno(0, 1, c->Vp, f, 0.0);
        kD = t4_d_vg(1, 2, 0.7);
        t4_d_vg(0, 2, 0.7);
        t4_d_vg(3, 1, 0.7);
        t4_d_vg(3, 0, 0.7);
        kC = t4_c0(2, 3, C);
        sr_el(SE_R, 2, 3, R);
    }
    canal(0, CANAL_V, kC); canal(1, CANAL_I, kD);
    saj.h = T / 100.0; saj.t_fin = 6.0 * T;
    emula(c->nom);
    g = sim_reg();
    /* cada muestra del último periodo, frente a la cerrada en su instante */
    for (i = 0; i < g->n; i++) {
        double t = g->t[i], th, e;
        if (t < 5.0 * T - 1e-12) continue;
        th = w * (t - 5.0 * T);
        e = fabs(g->y[0][i] - t4_cerrada(th, c->Vp, c->Vd, c->on * T4_PI / 180.0, c->off * T4_PI / 180.0, w * R * C, per));
        if (e > peor) {
            peor = e;
            if (getenv("HAKARU_DEPURA") && e > 5e-4)
                printf("      %s: t = %.9g (%.4f" S_DEG ") emulado %.9g, cerrada %.9g, I(D) %.3e\n", c->nom, t, th * 180.0 / T4_PI,
                       g->y[0][i], t4_cerrada(th, c->Vp, c->Vd, c->on * T4_PI / 180.0, c->off * T4_PI / 180.0, w * R * C, per), g->y[1][i]);
        }
        /* θ_on: la última muestra sin corriente antes de conducir; θ_off: la
         * primera sin corriente tras conducir (los eventos son muestras) */
        if (th < per) {
            if (!conduce && g->y[1][i] > thr) { conduce = 1; if (on < 0.0 && i > 0) on = w * (g->t[i - 1] - 5.0 * T); }
            else if (conduce && g->y[1][i] <= thr && off < 0.0) { conduce = 0; off = th; }
        }
    }
    chk_t(c->nom, "cada muestra del " S_uu "ltimo periodo frente a la cerrada", peor, 0.0, 1.0, 5e-4);
    sim_medir(0, 5.0 * T, 6.0 * T, &me);
    if (getenv("HAKARU_DEPURA"))
        for (i = 0; i < g->n; i++)
            if (g->t[i] >= 5.0 * T - 1e-12 && g->y[0][i] == me.min)
                printf("      %s: Vmin en %.4f" S_DEG " (%.12g; cerrada %.12g), I(D) %.3e\n", c->nom, w * (g->t[i] - 5.0 * T) * 180.0 / T4_PI,
                       me.min, t4_cerrada(w * (g->t[i] - 5.0 * T), c->Vp, c->Vd, c->on * T4_PI / 180.0, c->off * T4_PI / 180.0, w * R * C, per), g->y[1][i]);
    pruebas++;
    if (!(me.max <= c->Vp - c->Vd + 1e-9 && me.max >= c->Vp - c->Vd - 5e-3)) falla(c->nom, "Vmax por debajo de Vp - Vd y a 5e-3", me.max, c->Vp - c->Vd);
    /* Vmin: el mínimo muestreado es la y+ del encendido, que cae sobre la
     * variedad lenta: Ron·i+ por debajo del valor continuo (el modo Ron·C =
     * 0,1 µs ya pasó a h/8): 1,9e-4 V en la media onda, 3,3e-4 en el puente */
    chk_t(c->nom, "Vmin (y+ del encendido: Ron" S_CDOT "i+)", me.min, c->vmin, 1.0, 5e-4);
    chk_t(c->nom, "rizado", me.max - me.min, c->Vp - c->Vd - c->vmin, 1.0, 5e-3);
    chk_t(c->nom, "media (1e-4 relativo)", me.media, c->med, c->med, 1e-4);
    if (getenv("HAKARU_DEPURA"))
        printf("      %s: on %.6f (cerrada %.6f), off %.6f (cerrada %.6f)\n", c->nom, fmod(on * 180.0 / T4_PI, per * 180.0 / T4_PI), c->on,
               fmod(off * 180.0 / T4_PI, per * 180.0 / T4_PI), c->off);
    chk_t(c->nom, S_THETA "_on a 0,01" S_DEG, fmod(on * 180.0 / T4_PI, per * 180.0 / T4_PI), c->on, 1.0, 0.01);
    chk_t(c->nom, S_THETA "_off a 0,02" S_DEG, fmod(off * 180.0 / T4_PI, per * 180.0 / T4_PI), c->off, 1.0, 0.02);
    {
        int ev = sim_res()->nw.eventos, esperados = (c->tipo == 3 || c->tipo == 2 ? 4 : 2) * 6;
        pruebas++;
        if (!(abs(ev - esperados) <= (c->tipo >= 2 ? 2 : 1))) falla(c->nom, "cambios de tramo por periodo", ev, esperados);
    }
    t4_balance(c->nom, 5e-3);
}

/* ---- La referencia RK4 de la unión con Rs (1N4007) ---- */
static double t4_ij(const NlDiodo *c, double vd) { return c->Is * (exp(vd / (c->n * NL_VT)) - 1.0); }

/* La corriente de la unión con Rs para una tensión u en bornes (Newton en vd) */
static double t4_i_diodo(const NlDiodo *c, double u)
{
    double vd = u < 0.6 ? u : 0.6, nvt = c->n * NL_VT;
    int i;
    for (i = 0; i < 100; i++) {
        double ij = t4_ij(c, vd), g = c->Is * exp(vd / nvt) / nvt;
        double fv = vd + c->Rs * ij - u, dv = fv / (1.0 + c->Rs * g);
        if (dv > 0.1) dv = 0.1;                       /* sin pasarse de largo en la exponencial */
        vd -= dv;
        if (fabs(dv) < 1e-15) break;
    }
    return t4_ij(c, vd) + NL_GMIN * u;
}

/* v' = (i_D(Vs - v) - v/R)/C (media onda) o con dos diodos en serie (puente) */
static double t4_dv(const NlDiodo *c, double t, double v, double Vp, double w, double R, double C, int puente)
{
    double vs = Vp * sin(w * t), i;
    if (puente) i = t4_i_diodo(c, 0.5 * (fabs(vs) - v));
    else i = t4_i_diodo(c, vs - v);
    return (i - v / R) / C;
}

static double t4_ref[120001];

static void t4_referencia(const NlDiodo *c, double Vp, int puente, double T)
{
    const double w = 2.0 * T4_PI / T, R = 1e3, C = 100e-6, dt = 6.0 * T / 120000.0;
    double v = 0.0;
    int i;
    t4_ref[0] = 0.0;
    for (i = 0; i < 120000; i++) {
        double t = i * dt, k1, k2, k3, k4;
        k1 = t4_dv(c, t, v, Vp, w, R, C, puente);
        k2 = t4_dv(c, t + 0.5 * dt, v + 0.5 * dt * k1, Vp, w, R, C, puente);
        k3 = t4_dv(c, t + 0.5 * dt, v + 0.5 * dt * k2, Vp, w, R, C, puente);
        k4 = t4_dv(c, t + dt, v + dt * k3, Vp, w, R, C, puente);
        v += dt / 6.0 * (k1 + 2.0 * k2 + 2.0 * k3 + k4);
        t4_ref[i + 1] = v;
    }
}

static double t4_ref_en(double t, double T)
{
    double u = t / (6.0 * T / 120000.0);
    int i = (int)u;
    if (i >= 120000) return t4_ref[120000];
    return t4_ref[i] + (u - i) * (t4_ref[i + 1] - t4_ref[i]);
}

static void t4_suave(const char *q, int puente, double Vp, double h, double media)
{
    const double T = 1.0 / 60.0;
    const SimReg *g;
    const SimRes *r;
    SimMedidas me;
    double peor = 0.0;
    int kC, i;
    if (!puente) {
        sr_init(3);
        t4_seno(0, 1, Vp, 60.0, 0.0);
        t4_d_cat(1, 2, &NL_1N4007);
        kC = t4_c0(2, 0, 100e-6);
        sr_el(SE_R, 2, 0, 1e3);
    } else {
        sr_init(4);
        t4_seno(0, 1, Vp, 60.0, 0.0);
        t4_d_cat(1, 2, &NL_1N4007);
        t4_d_cat(0, 2, &NL_1N4007);
        t4_d_cat(3, 1, &NL_1N4007);
        t4_d_cat(3, 0, &NL_1N4007);
        kC = t4_c0(2, 3, 100e-6);
        sr_el(SE_R, 2, 3, 1e3);
    }
    canal(0, CANAL_V, kC); canal(1, CANAL_NADA, 0);
    saj.h = h; saj.t_fin = 6.0 * T;
    emula(q);
    g = sim_reg(); r = sim_res();
    for (i = 0; i < g->n; i++) {
        double e;
        if (g->t[i] < 5.0 * T - 1e-12) continue;
        e = fabs(g->y[0][i] - t4_ref_en(g->t[i], T));
        if (e > peor) peor = e;
    }
    chk_t(q, "cada muestra del " S_uu "ltimo periodo, a 1e-3" S_CDOT "Vp de la referencia", peor, 0.0, Vp, 1e-3);
    sim_medir(0, 5.0 * T, 6.0 * T, &me);
    chk_t(q, "la media del " S_uu "ltimo periodo (1e-3 relativo)", me.media, media, media, 1e-3);
    pruebas++;
    if (!((double)r->nw.iter / (double)(r->pasos > 0 ? r->pasos : 1) < 3.0)) falla(q, "iteraciones por paso < 3 de media", (double)r->nw.iter / (double)r->pasos, 3.0);
    pruebas++;
    if (!(r->nw.residuo_max <= 1e-9)) falla(q, "residuo <= 1e-9 en cada paso", r->nw.residuo_max, 0.0);
    t4_balance(q, 5e-3);
    printf("    %s: %ld pasos, %ld iteraciones, %ld subpasos de m" S_aa "s; error %.1e V\n", q, r->pasos, r->nw.iter, r->nw.sub, peor);
}

static void test_e4_trans(void)
{
    static const T4Rect RECT[4] = {
        { "media onda",           0, 10.0, 0.7, 7.98398262081831, 8.64575304946355, 60.2730373768404, 91.4131115517566 },
        { "media onda ideal",     1, 10.0, 0.0, 8.58941315611351, 9.29961706891035, 59.1979208795881, 91.5194614473286 },
        { "onda completa",        2, 10.0, 0.7, 8.64240954057463, 8.98156622274838, 69.1058598043297, 91.4131115517566 },
        { "puente",               3, 12.0, 1.4, 9.84803287246722, 10.2354225047336, 69.6088916670063, 91.3422114404318 }
    };
    SimMedidas me;
    int i, k;

    printf("[59] Etapa 4: el transitorio por tramos\n");
    for (i = 0; i < 4; i++) t4_rectificador(&RECT[i]);
    t4_trozos("puente: trozos");

    /* ---- Recortador: máximo de la salida 3,7000062999936993 V ---- */
    sr_init(4);
    t4_seno(0, 1, 10.0, 1e3, 0.0);
    sr_el(SE_R, 1, 2, 1e3);
    t4_d_vg(2, 3, 0.7);
    k = sr_el(SE_E, 0, 3, 0.0); sr.el[k].forma = ONDA_CC; sr.el[k].p[0] = 3.0;
    canal(0, CANAL_NODO, 2); canal(1, CANAL_NADA, 0);
    saj.h = 1e-3 / 100.0; saj.t_fin = 3e-3;
    emula("recortador");
    sim_medir(0, 2e-3, 3e-3, &me);
    chk_t("recortador", "m" S_aa "ximo de la salida", me.max, 3.7000062999936993, 3.7, 1e-9);
    t4_balance("recortador", 5e-3);

    /* ---- Fijador: mínimo -9,278550321932634 V y máximo 0,700004099003878 V ---- */
    sr_init(3);
    t4_seno(0, 1, 5.0, 1e3, 0.0);
    t4_c0(1, 2, 1e-6);
    t4_d_vg(2, 0, 0.7);
    sr_el(SE_R, 2, 0, 100e3);
    canal(0, CANAL_NODO, 2); canal(1, CANAL_NADA, 0);
    saj.h = 1e-3 / 200.0; saj.t_fin = 20e-3;
    emula("fijador");
    sim_medir(0, 19e-3, 20e-3, &me);
    chk_t("fijador", "m" S_ii "nimo del " S_uu "ltimo periodo", me.min, -9.278550321932634, 1.0, 1e-3);
    chk_t("fijador", "m" S_aa "ximo del " S_uu "ltimo periodo", me.max, 0.700004099003878, 1.0, 1e-3);
    t4_balance("fijador", 5e-3);

    printf("[60] Etapa 4: el transitorio de los suaves\n");
    t4_referencia(&NL_1N4007, 10.0, 0, 1.0 / 60.0);
    t4_suave("1N4007 media onda T/100", 0, 10.0, 1.0 / 6000.0, 8.66987970057439);
    t4_suave("1N4007 media onda T/200", 0, 10.0, 1.0 / 12000.0, 8.66987970057439);
    t4_referencia(&NL_1N4007, 12.0, 1, 1.0 / 60.0);
    t4_suave("1N4007 puente", 1, 12.0, 0.0, 10.245198195320043);
    t4_trozos("1N4007 puente: trozos");

    /* ---- Media onda sin C: el pico de V(R) en T/4 ---- */
    sr_init(3);
    t4_seno(0, 1, 10.0, 60.0, 0.0);
    t4_d_cat(1, 2, &NL_1N4007);
    k = sr_el(SE_R, 2, 0, 1e3);
    canal(0, CANAL_V, k); canal(1, CANAL_NADA, 0);
    saj.h = 1.0 / 6000.0; saj.t_fin = 1.0 / 60.0;
    emula("media onda sin C");
    sim_medir(0, 0.0, 1.0 / 60.0, &me);
    chk_t("media onda sin C", "pico de V(R) (la muestra de T/4)", me.max, 9.3403009505190993, 9.34, 1e-9);

    /* ---- Patada inductiva con diodo de rueda libre ---- */
    {
        const SimReg *g;
        int kS, kL, cambios = 0, prev = 0;
        sr_init(4);
        k = sr_el(SE_E, 0, 1, 0.0); sr.el[k].forma = ONDA_CC; sr.el[k].p[0] = 10.0;
        sr_el(SE_R, 1, 2, 10.0);
        kS = sr_el(SE_S, 2, 3, 0.0); sr.el[kS].cerrado0 = 1; sr.el[kS].t1 = 1e-3;
        kL = sr_el(SE_L, 3, 0, 10e-3);
        t4_d_cat(0, 3, &NL_1N4148);
        canal(0, CANAL_V, kL); canal(1, CANAL_I, kL);
        saj.h = 1e-5; saj.t_fin = 3e-3;
        emula("patada inductiva");
        g = sim_reg();
        for (i = 0; i < g->n; i++) {
            int s;
            if (g->t[i] <= 1e-3 + 2e-5) continue;
            s = g->y[0][i] > 1e-9 ? 1 : g->y[0][i] < -1e-9 ? -1 : 0;
            if (s && prev && s != prev) cambios++;
            if (s) prev = s;
        }
        chk_cond("patada inductiva", "V(L) no cambia de signo tras abrir (sin oscilaci" S_oo "n)", cambios == 0);
        chk_cond("patada inductiva", "la corriente de la L baja por el diodo", g->y[1][g->n - 1] > 0.0 && g->y[1][g->n - 1] < 0.9);
        printf("    patada inductiva: %ld pasos, %ld subpasos de m" S_aa "s, %d cambios de signo de V(L)\n",
               sim_res()->pasos, sim_res()->nw.sub, cambios);
        t4_balance("patada inductiva", 5e-3);
    }
}
