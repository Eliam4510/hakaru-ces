/* --------------------------------------------------------------------------
 *  51. Etapa 3: la pantalla del Bode (trazador.c)
 *
 *  Se incluye en host_test.c detrás de [48] (usa sr, saj, bode_sr, db_en y
 *  fase_en). Prueba las funciones puras (columna, filas, valor interpolado,
 *  autoescala, marcas y lecturas), las teclas de §4.7 con el RC paso bajo y
 *  el pasa-banda RLC, los textos del panel, los topes del zoom y que dibujar
 *  no se sale de la pantalla con cada tipo de filtro. Vuelca las pantallas a
 *  out/bode_*.bmp.
 * ----------------------------------------------------------------------- */
#include "trazador.h"

static Trazador trz;

static int tz_tecla(int t) { return traz_tecla(&trz, bode_reg(), bode_res(), t); }
static void tz_dibuja(void) { traz_dibuja(&trz, bode_reg(), bode_res()); }

static void tz_teclas(const int *t, int n)
{
    int i;
    for (i = 0; i < n; i++) { tz_dibuja(); tz_tecla(t[i]); }
}
#define TZ_TECLAS(...) do { static const int _t[] = { __VA_ARGS__ }; tz_teclas(_t, (int)(sizeof _t / sizeof _t[0])); } while (0)

/* ¿Se ve este texto? (se redibuja entera) */
static int tz_ve(const char *t)
{
    gfx_textos_reset();
    tz_dibuja();
    return gfx_texto_visto(t);
}

static void tz_vuelca(const char *nombre)
{
    char ruta[256];
    tz_dibuja();
    sprintf(ruta, "%s/bode_%s.bmp", dir_out, nombre);
    gfx_dump_bmp(ruta);
}

/* ¿Hay algún píxel de color c en el rectángulo [x0, x1] x [y0, y1]? */
static int tz_hay_color(int x0, int y0, int x1, int y1, u16 c)
{
    const u16 *fb = gfx_buffer();
    int x, y;
    for (y = y0; y <= y1; y++)
        for (x = x0; x <= x1; x++)
            if (x >= 0 && x < SCR_W && y >= 0 && y < SCR_H && fb[y * SCR_W + x] == c) return 1;
    return 0;
}

static void tz_empieza(void)
{
    memset(&trz, 0, sizeof trz);
    traz_nuevo(&trz, bode_reg(), bode_res());
}

/* Los circuitos del Bode ([48]), con el nombre de la salida para la cabecera.
 * 0 RC paso bajo, 1 RL paso alto, 2 RLC pasa-banda, 3 RLC paso bajo con
 * pico, 4 tanque (V/J, en ohmios), 5 rechaza-banda, 6 resistivo (plano),
 * 7 LC sin R con un punto justo en f0 (hueco), 8 RC cortado a medias */
static int tz_circuito(int cual)
{
    int kE = 0, k = 0;
    double f0 = 1.0 / (2.0 * PI_T * sqrt(10e-3 * 1e-6));
    sr_init(cual == 2 || cual == 3 || cual == 5 ? 4 : cual == 4 ? 2 : 3);
    if (cual == 4) { kE = sr_el(SE_J, 0, 1, 0.0); sr.el[kE].forma = ONDA_CC; sr.el[kE].p[0] = 1.0; }
    else { kE = sr_el(SE_E, 0, 1, 0.0); sr.el[kE].forma = ONDA_CC; sr.el[kE].p[0] = 1.0; }
    switch (cual) {
    case 0: case 8:
        sr_el(SE_R, 1, 2, 1e3); k = sr_el(SE_C, 2, 0, 1e-6);
        canal(0, CANAL_V, k); strcpy(saj.canal[0].nombre, "V(C1)");
        break;
    case 1:
        sr_el(SE_R, 1, 2, 100.0); k = sr_el(SE_L, 2, 0, 10e-3);
        canal(0, CANAL_V, k); strcpy(saj.canal[0].nombre, "V(L1)");
        break;
    case 2:
        sr_el(SE_L, 1, 2, 10e-3); sr_el(SE_C, 2, 3, 1e-6); k = sr_el(SE_R, 3, 0, 10.0);
        canal(0, CANAL_V, k); strcpy(saj.canal[0].nombre, "V(R1)");
        break;
    case 3:
        sr_el(SE_R, 1, 2, 20.0); sr_el(SE_L, 2, 3, 10e-3); k = sr_el(SE_C, 3, 0, 1e-6);
        canal(0, CANAL_V, k); strcpy(saj.canal[0].nombre, "V(C1)");
        break;
    case 4:
        sr_el(SE_R, 1, 0, 1e4); sr_el(SE_L, 1, 0, 10e-3); sr_el(SE_C, 1, 0, 1e-6);
        canal(0, CANAL_NODO, 1); strcpy(saj.canal[0].nombre, "V(J1)");
        break;
    case 5:
        sr_el(SE_R, 1, 2, 100.0); sr_el(SE_L, 2, 3, 10e-3); sr_el(SE_C, 3, 0, 1e-6);
        canal(0, CANAL_NODO, 2); strcpy(saj.canal[0].nombre, "V(L1)");
        break;
    case 6:
        sr_el(SE_R, 1, 2, 1e3); k = sr_el(SE_R, 2, 0, 1e3);
        canal(0, CANAL_V, k); strcpy(saj.canal[0].nombre, "V(R2)");
        break;
    default:
        sr_el(SE_L, 1, 2, 10e-3); k = sr_el(SE_C, 2, 0, 1e-6);
        canal(0, CANAL_V, k); strcpy(saj.canal[0].nombre, "V(C1)");
        break;
    }
    if (cual == 8) {
        int i;
        bode_iniciar_red(&sr, kE, &saj.canal[0], -1, -1, 0.0, 0.0, 0.0);
        for (i = 0; i < 80; i++) bode_avanzar(1ul);
        bode_cortar();
        return bode_res()->fin == FIN_CORTADO ? SIM_OK : -1;
    }
    if (cual == 7) return bode_sr(kE, 0, -1, -1, f0 / 100.0, f0 * 100.0, 0.0);
    return bode_sr(kE, 0, -1, -1, 0.0, 0.0, cual == 6 ? 60.0 : 0.0);
}

/* La ventana dentro del barrido y no más estrecha que el tope */
static int tz_ventana_sana(void)
{
    const BodeReg *g = bode_reg();
    double L0 = log10(g->f[0]), L1 = log10(g->f[g->n - 1]), w = trz.lf1 - trz.lf0, wmin = traz_ancho_min(g);
    if (wmin > L1 - L0) wmin = L1 - L0;
    return trz.lf0 >= L0 - 1e-12 && trz.lf1 <= L1 + 1e-12 && w >= wmin * (1.0 - 1e-9) &&
           trz.idb >= 0 && trz.idb < 6 && trz.ifase >= 0 && trz.ifase < 4 &&
           trz.fcur[0] >= g->f[0] * (1.0 - 1e-12) && trz.fcur[0] <= g->f[g->n - 1] * (1.0 + 1e-12) &&
           (!trz.dos || (trz.fcur[1] >= g->f[0] * (1.0 - 1e-12) && trz.fcur[1] <= g->f[g->n - 1] * (1.0 + 1e-12)));
}

static void test_trazador(void)
{
    const BodeReg *g = bode_reg();
    const BodeRes *r = bode_res();
    const BodeMedidas *m = &r->m;
    double paso, fm, mx_db, mx_fa, f[8];
    const char *nom[8];
    int i, k, n, bien, dentro, cual;

    printf("[51] Etapa 3: la pantalla del Bode\n");

    /* ================= Funciones puras, con el RC paso bajo ================= */
    chk_cond("trazador RC", "se barre: pasa-bajas en [10, 1e4] con 277 puntos",
             tz_circuito(0) == SIM_OK && m->tipo == BODE_PB && g->n == BODE_N && g->f[0] == 10.0 && g->f[g->n - 1] == 1e4);
    tz_empieza();
    chk_cond("traz_nuevo", "la ventana es todo el barrido", trz.lf0 == 1.0 && trz.lf1 == 4.0 && traz_en_vista(&trz, g) == BODE_N);
    chk_cond("traz_nuevo", "el cursor al centro, uno solo", fabs(log10(trz.fcur[0]) - 2.5) < 1e-12 && !trz.dos && trz.activo == 0);

    /* ---- traz_x ---- */
    chk_cond("traz_x", "10 Hz en el borde izquierdo", traz_x(&trz, 10.0) == BD_X0);
    chk_cond("traz_x", "10 kHz en el derecho", traz_x(&trz, 1e4) == BD_X0 + BD_W);
    chk_cond("traz_x", "una década = 92 px", traz_x(&trz, 100.0) == BD_X0 + 92 && traz_x(&trz, 1e3) == BD_X0 + 184);
    for (k = 0, bien = 1; k < g->n; k++) if (traz_x(&trz, g->f[k]) != BD_X0 + k) bien = 0;
    chk_cond("traz_x", "cada punto de la malla en su columna", bien);
    {
        static const double RARO[6] = { 0.0, -5.0, 1e-300, 1e300, 1.0, 1e9 };
        for (i = 0, bien = 1; i < 6; i++) {
            int x = traz_x(&trz, RARO[i]);
            if (x < BD_X0 - 1 || x > BD_X0 + BD_W + 1) bien = 0;
        }
        if (traz_x(&trz, sqrt(-1.0)) != BD_X0 - 1 || traz_x(&trz, HUGE_VAL) != BD_X0 + BD_W + 1) bien = 0;
        chk_cond("traz_x", "0, negativa, NaN, 1e300 e infinita: dentro de [37, 315]", bien);
    }

    /* ---- traz_y_db y traz_y_fase: con todas las escalas y varios topes ---- */
    {
        static const double RARO[6] = { 1e300, -1e300, 1e30, -1e30, 0.0, -300.0 };
        static const signed char TOPES[4] = { -127, -10, 0, 127 };
        int bien_db = 1, bien_fa = 1, raro_db = 1, raro_fa = 1;
        for (i = 0; i < 6; i++)
            for (k = 0; k < 4; k++) {
                double d = DBDIV[i], top = TOPES[k] * d;
                int j;
                trz.idb = (signed char)i; trz.db_top = TOPES[k];
                if (traz_y_db(&trz, top) != BD_MY0 || traz_y_db(&trz, top - d) != BD_MY0 + BD_MDY ||
                    traz_y_db(&trz, top - 4.0 * d) != BD_MY0 + 4 * BD_MDY || traz_y_db(&trz, top - 2.25 * d) != BD_MY0 + 47 ||
                    traz_y_db(&trz, 1e300) != BD_MY0 - 1 || traz_y_db(&trz, -1e300) != BD_MY0 + 85) bien_db = 0;
                for (j = 0; j < 6; j++) { int y = traz_y_db(&trz, RARO[j]); if (y < BD_MY0 - 1 || y > BD_MY0 + 85) raro_db = 0; }
                { int y = traz_y_db(&trz, sqrt(-1.0)); if (y < BD_MY0 - 1 || y > BD_MY0 + 85) raro_db = 0; }
                if (i < 4) {
                    d = FDIV[i]; top = TOPES[k] * d;
                    trz.ifase = (signed char)i; trz.fase_top = TOPES[k];
                    if (traz_y_fase(&trz, top) != BD_FY0 || traz_y_fase(&trz, top - d) != BD_FY0 + BD_FDY ||
                        traz_y_fase(&trz, top - 4.0 * d) != BD_FY0 + 4 * BD_FDY ||
                        traz_y_fase(&trz, 1e300) != BD_FY0 - 1 || traz_y_fase(&trz, -1e300) != BD_FY0 + 57) bien_fa = 0;
                    for (j = 0; j < 6; j++) { int y = traz_y_fase(&trz, RARO[j]); if (y < BD_FY0 - 1 || y > BD_FY0 + 57) raro_fa = 0; }
                    { int y = traz_y_fase(&trz, sqrt(-1.0)); if (y < BD_FY0 - 1 || y > BD_FY0 + 57) raro_fa = 0; }
                }
            }
        chk_cond("traz_y_db", "el tope arriba, 4 divisiones de 21 px", bien_db);
        chk_cond("traz_y_db", "NaN y ±1e300: dentro de [16, 102]", raro_db);
        chk_cond("traz_y_fase", "el tope arriba, 4 divisiones de 14 px", bien_fa);
        chk_cond("traz_y_fase", "NaN y ±1e300: dentro de [105, 163]", raro_fa);
    }

    /* ---- traz_valor: en la malla, igual; entre dos puntos, contra el motor ---- */
    for (k = 0, bien = 1; k < g->n; k++)
        if (traz_valor(g, g->f[k], 0) != g->db[k] || traz_valor(g, g->f[k], 1) != g->fase[k]) bien = 0;
    chk_cond("traz_valor", "en cada punto de la malla, el suyo (bit a bit)", bien);
    mx_db = mx_fa = 0.0;
    for (k = 0; k + 1 < g->n; k++) {
        fm = sqrt(g->f[k] * g->f[k + 1]);
        if (fabs(traz_valor(g, fm, 0) - db_en(fm)) > mx_db) mx_db = fabs(traz_valor(g, fm, 0) - db_en(fm));
        if (fabs(traz_valor(g, fm, 1) - fase_en(fm)) > mx_fa) mx_fa = fabs(traz_valor(g, fm, 1) - fase_en(fm));
    }
    printf("    RC entre dos puntos: %.2e dB y %.2e grados de la curva exacta\n", mx_db, mx_fa);
    chk_cond("traz_valor", "entre dos puntos: a 1e-3 dB de bode_en", mx_db < 1e-3);
    chk_cond("traz_valor", "entre dos puntos: a 1e-2" S_DEG " de bode_en", mx_fa < 1e-2);
    fm = traz_valor(g, 5.0, 0);
    chk_cond("traz_valor", "fuera del barrido: NaN", fm != fm && traz_valor(g, 2e4, 1) != traz_valor(g, 2e4, 1) &&
             traz_valor(g, 0.0, 0) != traz_valor(g, 0.0, 0) && traz_valor(g, sqrt(-1.0), 0) != traz_valor(g, sqrt(-1.0), 0));
    chk_cond("traz_valor", "en los bordes (10^lf de la ventana), el del borde",
             traz_valor(g, pow(10.0, trz.lf0), 0) == g->db[0] && traz_valor(g, pow(10.0, trz.lf1), 0) == g->db[g->n - 1]);

    /* ---- traz_autoescala ---- */
    traz_autoescala(&trz, g);
    chk_cond("traz_autoescala", "RC: 10 dB/div con el tope en 0 dB", trz.idb == 3 && trz.db_top == 0);
    chk_cond("traz_autoescala", "RC: 30" S_DEG "/div con el tope en 0" S_DEG, trz.ifase == 1 && trz.fase_top == 0);
    for (k = 0, dentro = 1; k < g->n; k++) {
        int y = traz_y_db(&trz, g->db[k]), yf = traz_y_fase(&trz, g->fase[k]);
        if (y < BD_MY0 || y > BD_MY0 + 4 * BD_MDY || yf < BD_FY0 || yf > BD_FY0 + 4 * BD_FDY) dentro = 0;
    }
    chk_cond("traz_autoescala", "RC: toda la curva dentro de las dos gráficas", dentro);

    /* ---- traz_lee: la lectura es la del motor ---- */
    {
        double db, mod, fa;
        chk_cond("traz_lee", "en 1 kHz, la de bode_en", traz_lee(g, 1e3, &db, &mod, &fa) &&
                 db == db_en(1e3) && fa == fase_en(1e3) && fabs(mod - 0.15717672547758987) < 1e-15);
        chk_cond("traz_lee", "en fc: -3,0103 dB y -45" S_DEG, traz_lee(g, m->fc, &db, &mod, &fa) &&
                 fabs(db + 3.0102999566398116) < 1e-6 && fabs(fa + 45.0) < 1e-6);
    }

    /* ---- traz_marcas ---- */
    n = traz_marcas(r, f, nom, 8);
    chk_cond("traz_marcas", "RC: solo fc", n == 1 && f[0] == m->fc && strcmp(nom[0], "fc") == 0);

    /* ================= Pantalla y teclas del RC ================= */
    tz_empieza();
    chk_cond("Bode RC", "la cabecera: Bode V(C1)/E1 y el rango", tz_ve("Bode V(C1)/E1") && tz_ve("10Hz a 10kHz"));
    chk_cond("Bode RC", "ve(\"Pasa-bajas\")", tz_ve("Pasa-bajas"));
    chk_cond("Bode RC", "el tipo: fc 159,2Hz (-3 dB de 0,00 dB)", tz_ve("Pasa-bajas  fc 159,2Hz  (-3 dB de 0,00 dB)"));
    /* abajo, en 10 Hz: -20·(f/fc)²/(1 + (f/fc)²) = -0,079 dB/década */
    chk_cond("Bode RC", "las pendientes: -20,0 arriba y -0,1 abajo", tz_ve("Arriba: -20,0 dB/d" S_ee "c   abajo: -0,1 dB/d" S_ee "c"));
    chk_cond("Bode RC", "la entrada y la salida", tz_ve("Entrada E1 [F]   Salida V(C1) [C]   [X] 2 cursores"));
    chk_cond("Bode RC", "el pie", tz_ve("[izq/der] cursor [+-] zoom [M] marcas [tab] fasores"));
    chk_cond("Bode RC", "rótulos del eje: 10, 100, 1k y 10k", tz_ve("\n10\n") && tz_ve("\n100\n") && tz_ve("\n1k\n") && tz_ve("\n10k\n"));
    chk_cond("Bode RC", "rótulos de y: 0 y -40 dB, 0" S_DEG " y -120" S_DEG, tz_ve("\n0\n") && tz_ve("\n-40\n") && tz_ve("\ndB\n") &&
             tz_ve("\n0" S_DEG "\n") && tz_ve("\n-120" S_DEG "\n"));
    /* la curva: cada columna tiene su píxel de C_CANAL1 */
    tz_dibuja();
    for (k = 0, n = 0; k <= BD_W; k++) {
        double v = traz_valor(g, pow(10.0, trz.lf0 + (trz.lf1 - trz.lf0) * k / BD_W), 0);
        int y = traz_y_db(&trz, v);
        if (v == v && y >= BD_MY0 && y <= BD_MY0 + 4 * BD_MDY && gfx_buffer()[y * SCR_W + BD_X0 + k] == C_CANAL1) n++;
    }
    chk_cond("Bode RC", "la curva de la magnitud pasa por su columna (270 de 277 o más)", n >= 270);
    {
        int x = traz_x(&trz, m->fc), y = traz_y_db(&trz, traz_valor(g, m->fc, 0));
        chk_cond("Bode RC", "un " S_DARR " en fc, sobre la curva", tz_hay_color(x - 2, y - 7, x + 2, y - 2, C_CURSOR));
        y = traz_y_db(&trz, -3.0103 + 20.0 * log10(m->M0));
        chk_cond("Bode RC", "la referencia de -3 dB, discontinua", tz_hay_color(BD_X0, y, BD_X0 + 20, y, C_TENUE));
    }

    /* ---- 1. escribir una f: «1 [K] [enter]» ---- */
    TZ_TECLAS(T_DIG(1), T_LETRA('K'));
    chk_cond("Bode RC", "escribiendo: 1k", strcmp(trz.entrada, "1k") == 0);
    chk_cond("Bode RC", "el eco: " S_RARR "f = 1k_  (= 1,000 kHz)", tz_ve(S_RARR "f = 1k_  (= 1,000 kHz)"));
    tz_vuelca("rc_escribe");
    TZ_TECLAS(T_ENTER);
    paso = (log10(g->f[g->n - 1]) - log10(g->f[0])) / (g->n - 1);
    chk_cond("Bode RC", "1 [K] [enter]: el cursor a menos de un punto de 1 kHz", fabs(log10(trz.fcur[0] / 1e3)) < paso && trz.entrada[0] == 0);
    chk_cond("Bode RC", "la lectura exacta en 1 kHz",
             tz_ve("f 1,000kHz  |H| -16,07dB = 0,1572  " S_PHI " -81,0" S_DEG));
    tz_vuelca("rc");
    TZ_TECLAS(T_DIG(5), T_DIG(0), T_LETRA('K'), T_ENTER);
    chk_cond("Bode RC", "50k: fuera del barrido y el cursor no se mueve", trz.fcur[0] == 1e3 && tz_ve("Fuera del barrido: [menu] cambia el rango."));
    TZ_TECLAS(T_PUNTO, T_ENTER);
    chk_cond("Bode RC", "«.»: no es una frecuencia", trz.fcur[0] == 1e3 && tz_ve("Eso no es una frecuencia."));
    TZ_TECLAS(T_DIG(3), T_BORRAR, T_DIG(2), T_EE, T_MENOS, T_DIG(1), T_LETRA('M') | T_CON_SHIFT);
    chk_cond("Bode RC", "[EE], [-] tras la E, [del] y [shift][M] (mega)", strcmp(trz.entrada, "2E-1M") == 0);
    chk_cond("Bode RC", "[esc] escribiendo solo borra (no sale)", tz_tecla(T_ESC) == TZ_NADA && trz.entrada[0] == 0 && trz.fcur[0] == 1e3);

    /* ---- 5. [M]: la marca fc, con su aviso ---- */
    tz_tecla(T_LETRA('M'));
    chk_cond("Bode RC", "[M]: el cursor a fc", trz.fcur[0] == m->fc && tz_ve("fc: -3,01 dB"));
    tz_tecla(T_LETRA('M'));
    chk_cond("Bode RC", "[M] otra vez: fc (es la única)", trz.fcur[0] == m->fc);

    /* ---- 2. [izq/der]: un punto de la malla; con shift, una década ---- */
    tz_tecla(T_DER);
    for (k = 0; k < g->n && !(g->f[k] > m->fc); k++) {}
    chk_cond("Bode RC", "[der] desde fc: el punto siguiente de la malla", k < g->n && trz.fcur[0] == g->f[k]);
    tz_tecla(T_DER);
    chk_cond("Bode RC", "[der]: el otro", trz.fcur[0] == g->f[k + 1]);
    tz_tecla(T_IZQ);
    chk_cond("Bode RC", "[izq]: vuelve", trz.fcur[0] == g->f[k]);
    tz_tecla(T_DER | T_CON_SHIFT);
    chk_cond("Bode RC", "[shift][der]: una década", fabs(trz.fcur[0] / g->f[k] - 10.0) < 1e-12);
    for (i = 0; i < 5; i++) tz_tecla(T_DER | T_CON_SHIFT);
    chk_cond("Bode RC", "[shift][der] se para en el final del barrido", trz.fcur[0] == 1e4);
    tz_tecla(T_DER);
    chk_cond("Bode RC", "[der] en el final: se queda", trz.fcur[0] == 1e4);
    for (i = 0; i < 5; i++) tz_tecla(T_IZQ | T_CON_SHIFT);
    tz_tecla(T_IZQ);
    chk_cond("Bode RC", "[shift][izq] y [izq]: se paran en el principio", trz.fcur[0] == 10.0 && tz_ventana_sana());

    /* ---- 3. [+] y [-]: zoom alrededor del cursor, con topes ---- */
    TZ_TECLAS(T_DIG(1), T_LETRA('K'), T_ENTER, T_MAS);
    chk_cond("Bode RC", "[+]: la mitad, centrada en el cursor", fabs(trz.lf0 - 2.25) < 1e-12 && fabs(trz.lf1 - 3.75) < 1e-12);
    chk_cond("Bode RC", "[+] con 139 puntos: sin aviso", traz_en_vista(&trz, g) >= 138 && !tz_ve("Pocas muestras"));
    for (i = 0, bien = 1; i < 30; i++) { tz_tecla(T_MAS); if (!tz_ventana_sana()) bien = 0; }
    chk_cond("Bode RC", "[+] 30 veces: dentro del barrido y no más estrecha que el tope", bien);
    chk_t("Bode RC", "[+] se para en 4 intervalos de la malla", trz.lf1 - trz.lf0, traz_ancho_min(g), 1.0, 1e-12);
    chk_cond("Bode RC", "con menos de 30 puntos, el aviso", traz_en_vista(&trz, g) < 30 && tz_ve("Pocas muestras aqu" S_ii ": [=] afina lo que se ve."));
    chk_cond("Bode RC", "el cursor sigue a la vista", traz_x(&trz, trz.fcur[0]) >= BD_X0 && traz_x(&trz, trz.fcur[0]) <= BD_X0 + BD_W);
    chk_cond("Bode RC", "de cerca, los rótulos del eje no faltan", tz_ve("1k\n") || tz_ve("1,02k\n") || tz_ve("977\n"));
    tz_vuelca("rc_zoom");
    for (i = 0, bien = 1; i < 30; i++) { tz_tecla(T_MENOS); if (!tz_ventana_sana()) bien = 0; }
    chk_cond("Bode RC", "[-] 30 veces: se para en todo el barrido", bien && trz.lf0 == 1.0 && trz.lf1 == 4.0);
    tz_tecla(T_LETRA('M'));
    tz_tecla(T_MAS); tz_tecla(T_MAS); tz_tecla(T_MAS);
    tz_tecla(T_DER | T_CON_SHIFT);
    chk_cond("Bode RC", "[shift][der] fuera de la ventana: la ventana lo sigue", traz_x(&trz, trz.fcur[0]) >= BD_X0 &&
             traz_x(&trz, trz.fcur[0]) <= BD_X0 + BD_W && tz_ventana_sana());

    /* ---- 4. [arr/ab]: dB por división; con shift, una división ---- */
    tz_tecla(T_LETRA('A'));
    chk_cond("Bode RC", "[A]: vista entera y autoescala", trz.lf0 == 1.0 && trz.lf1 == 4.0 && trz.idb == 3 && trz.db_top == 0 && !trz.ajustado);
    tz_tecla(T_ARRIBA);
    chk_cond("Bode RC", "[arr]: 5 dB/div, el centro en -20 dB y ajustado", trz.idb == 2 && (trz.db_top - 2) * 5 == -20 && trz.ajustado);
    for (i = 0; i < 10; i++) tz_tecla(T_ARRIBA);
    chk_cond("Bode RC", "[arr] se para en 1 dB/div", trz.idb == 0);
    for (i = 0; i < 10; i++) tz_tecla(T_ABAJO);
    chk_cond("Bode RC", "[ab] se para en 40 dB/div", trz.idb == 5);
    k = trz.db_top;
    tz_tecla(T_ARRIBA | T_CON_SHIFT);
    chk_cond("Bode RC", "[shift][arr]: la curva sube una división", trz.db_top == k - 1 && trz.idb == 5);
    tz_tecla(T_ABAJO | T_CON_SHIFT); tz_tecla(T_ABAJO | T_CON_SHIFT);
    chk_cond("Bode RC", "[shift][ab]: baja", trz.db_top == k + 1);
    traz_nuevo(&trz, g, r);
    chk_cond("Bode RC", "traz_nuevo con ajustado: se queda la escala de dB", trz.idb == 5 && trz.db_top == k + 1 && trz.ifase == 1);
    tz_tecla(T_LETRA('A'));

    /* ---- 6. [P] y [shift][P]: sin pico ni valle, el máximo y el mínimo ---- */
    tz_tecla(T_LETRA('P'));
    chk_cond("Bode RC", "[P] sin pico: al máximo (10 Hz)", trz.fcur[0] == 10.0 && tz_ve("Sin pico; el m" S_aa "ximo: -0,02 dB"));
    tz_tecla(T_LETRA('P') | T_CON_SHIFT);
    chk_cond("Bode RC", "[shift][P] sin valle: al mínimo (10 kHz)", trz.fcur[0] == 1e4 && tz_ve("Sin valle; el m" S_ii "nimo: -35,96 dB"));

    /* ---- 7. [X] y [enter]: dos cursores ---- */
    TZ_TECLAS(T_DIG(1), T_LETRA('K'), T_ENTER, T_LETRA('X'));
    chk_cond("Bode RC", "[X]: dos cursores, el nuevo activo", trz.dos && trz.activo == 1 && trz.fcur[1] != trz.fcur[0]);
    TZ_TECLAS(T_DIG(1), T_DIG(0), T_LETRA('K'), T_ENTER);
    chk_cond("Bode RC", "el segundo a 10 kHz", trz.fcur[1] == 1e4 && trz.fcur[0] == 1e3);
    chk_cond("Bode RC", "la diferencia: " S_DELTA " f2/f1 = 10,00  " S_DELTA "dB -19,89  " S_DELTA S_PHI " -8,1" S_DEG,
             tz_ve(S_DELTA " f2/f1 = 10,00  " S_DELTA "dB -19,89  " S_DELTA S_PHI " -8,1" S_DEG));
    chk_cond("Bode RC", "y las dos lecturas", tz_ve(" f1 1,000kHz  |H| -16,07dB") && tz_ve(S_RARR "f2 10,00kHz  |H| -35,96dB = 0,01591"));
    chk_cond("Bode RC", "[X] 1 cursor en la última línea", tz_ve("[X] 1 cursor"));
    tz_vuelca("rc_cursores");
    tz_tecla(T_ENTER);
    chk_cond("Bode RC", "[enter]: el otro cursor", trz.activo == 0 && tz_ve(S_RARR "f1 1,000kHz"));
    tz_tecla(T_IZQ);
    chk_cond("Bode RC", "[izq] mueve el activo", trz.fcur[0] < 1e3 && trz.fcur[1] == 1e4);
    tz_tecla(T_LETRA('X'));
    chk_cond("Bode RC", "[X] otra vez: un cursor", !trz.dos && trz.activo == 0 && tz_ve("Pasa-bajas"));
    tz_tecla(T_ENTER);
    chk_cond("Bode RC", "[enter] con un cursor: el aviso", tz_ve("[X] enciende el segundo cursor."));

    /* ---- 9 y 10: lo que hace ui.c ---- */
    chk_cond("Bode RC", "[F], [C], [=], [tab], [menu] y [esc]",
             tz_tecla(T_LETRA('F')) == TZ_ENTRADA && tz_tecla(T_LETRA('C')) == TZ_SALIDA && tz_tecla(T_IGUAL) == TZ_BARRER &&
             tz_tecla(T_TAB) == TZ_FASORES && tz_tecla(T_MENU) == TZ_AJUSTES && tz_tecla(T_ESC) == TZ_SALIR);

    /* ================= El pasa-banda RLC ================= */
    chk_cond("trazador RLC", "se barre: pasa-banda", tz_circuito(2) == SIM_OK && m->tipo == BODE_PBANDA);
    tz_empieza();
    chk_cond("traz_autoescala", "RLC: 20 dB/div con el tope en 20 dB; 45" S_DEG "/div con el tope en 90" S_DEG,
             trz.idb == 4 && trz.db_top == 1 && trz.ifase == 2 && trz.fase_top == 2);
    {
        /* la lectura de 1 kHz del RC quedó recordada: con otro barrido no vale */
        double db, mod, fa;
        chk_cond("traz_lee", "otro barrido: la lectura es la de la red nueva", traz_lee(g, 1e3, &db, &mod, &fa) &&
                 db == db_en(1e3) && fa == fase_en(1e3) && db < -5.0);
    }
    chk_cond("Bode RLC", "el tipo: Pasa-banda  f0 1,592kHz  pico 0,00dB", tz_ve("Pasa-banda  f0 1,592kHz  pico 0,00dB"));
    chk_cond("Bode RLC", "fc1, fc2, BW y Q", tz_ve("fc1 1,514kHz  fc2 1,673kHz  BW 159,2Hz  Q 10,00"));
    n = traz_marcas(r, f, nom, 8);
    chk_cond("traz_marcas", "RLC: fc1, f0 y fc2, en orden", n == 3 && f[0] == m->f1 && f[1] == m->f_pico && f[2] == m->f2 &&
             strcmp(nom[0], "fc1") == 0 && strcmp(nom[1], "f0") == 0 && strcmp(nom[2], "fc2") == 0);
    tz_tecla(T_LETRA('M'));
    chk_cond("Bode RLC", "[M]: fc1, «fc1: -3,01 dB»", trz.fcur[0] == m->f1 && tz_ve("fc1: -3,01 dB"));
    tz_tecla(T_LETRA('M'));
    chk_cond("Bode RLC", "[M]: f0, «f0: 0,00 dB»", trz.fcur[0] == m->f_pico && tz_ve("f0: 0,00 dB"));
    tz_tecla(T_LETRA('M'));
    chk_cond("Bode RLC", "[M]: fc2, «fc2: -3,01 dB»", trz.fcur[0] == m->f2 && tz_ve("fc2: -3,01 dB"));
    tz_tecla(T_LETRA('M'));
    chk_cond("Bode RLC", "[M]: y vuelta a fc1", trz.fcur[0] == m->f1);
    tz_tecla(T_LETRA('P'));
    chk_cond("Bode RLC", "[P]: al pico", trz.fcur[0] == m->f_pico && tz_ve("pico: 0,00 dB") && tz_ve("f 1,592kHz  |H| 0,00dB = 1,000  " S_PHI " 0,0" S_DEG));
    tz_vuelca("pbanda");
    {
        int x = traz_x(&trz, m->f_pico), y = traz_y_db(&trz, traz_valor(g, m->f_pico, 0)), x1 = traz_x(&trz, m->f1), y1 = traz_y_db(&trz, traz_valor(g, m->f1, 0));
        tz_dibuja();
        /* con la ventana entera, los cortes están a 3 px del pico: no se pintan encima */
        chk_cond("Bode RLC", "un " S_UARR " bajo el pico; los " S_DARR " de los cortes, no (a 3 px)",
                 tz_hay_color(x - 2, y + 3, x + 2, y + 7, C_CURSOR) && !tz_hay_color(x1 - 2, y1 - 7, x1 - 1, y1 - 5, C_CURSOR));
        for (i = 0; i < 3; i++) tz_tecla(T_MAS);
        x = traz_x(&trz, m->f2); y = traz_y_db(&trz, traz_valor(g, m->f2, 0));
        x1 = traz_x(&trz, m->f1); y1 = traz_y_db(&trz, traz_valor(g, m->f1, 0));
        tz_dibuja();
        chk_cond("Bode RLC", "de cerca (1/4 de década), un " S_DARR " en cada corte", x1 < traz_x(&trz, m->f_pico) - 10 &&
                 tz_hay_color(x1 - 2, y1 - 7, x1 + 2, y1 - 2, C_CURSOR) && tz_hay_color(x - 2, y - 7, x + 2, y - 2, C_CURSOR));
        tz_vuelca("pbanda_cerca");
    }
    /* el zoom no pierde las marcas: [M] lleva la ventana a la marca */
    /* (la siguiente es f0, a la vista; la otra, fc2, cae fuera de la ventana) */
    for (i = 0; i < 3; i++) tz_tecla(T_MAS);
    chk_cond("Bode RLC", "de cerca, fc2 cae fuera", traz_x(&trz, m->f2) > BD_X0 + BD_W);
    tz_tecla(T_LETRA('M'));
    chk_cond("Bode RLC", "de cerca, [M]: f0", trz.fcur[0] == m->f_pico);
    tz_tecla(T_LETRA('M'));
    chk_cond("Bode RLC", "de cerca, [M] lleva la ventana a fc2", trz.fcur[0] == m->f2 && traz_x(&trz, m->f2) >= BD_X0 &&
             traz_x(&trz, m->f2) <= BD_X0 + BD_W && tz_ventana_sana());
    tz_vuelca("pbanda_zoom");

    /* ================= Cada tipo: su texto, y dibujar no se sale ================= */
    gfx_desbordes_reset();
    for (cual = 0; cual <= 8; cual++) {
        static const char *const NOMBRE[9] = { "pb", "pa", "pbanda2", "pb_pico", "tanque", "rb", "plano", "hueco", "cortado" };
        static const int K[16] = { T_MAS, T_MENOS, T_IZQ, T_DER, T_ARRIBA, T_ABAJO, T_ENTER, T_BORRAR,
                                   T_PUNTO, T_ESC, T_EE, T_IGUAL, T_TAB, T_MENU, T_DIG(7), T_DIG(0) };
        static const char LET[] = "XPMAKFCGUN";
        chk_cond("trazador tipos", NOMBRE[cual], tz_circuito(cual) == SIM_OK);
        tz_empieza();
        switch (cual) {
        case 1: chk_cond("Bode RL", "Pasa-altas  fc 1,592kHz", m->tipo == BODE_PA && tz_ve("Pasa-altas  fc 1,592kHz  (-3 dB de 0,00 dB)")); break;
        case 3: chk_cond("Bode RLC con pico", "Pico 1,576kHz a 14,02dB", m->tipo == BODE_PB && tz_ve("Pico 1,576kHz a 14,02dB"));
                n = traz_marcas(r, f, nom, 8);
                chk_cond("Bode RLC con pico", "marcas: el pico y fc", n == 2 && strcmp(nom[0], "pico") == 0 && strcmp(nom[1], "fc") == 0);
                tz_tecla(T_LETRA('M'));
                chk_cond("Bode RLC con pico", "[M]: «pico: 14,02 dB»", tz_ve("pico: 14,02 dB"));
                break;
        case 4: chk_cond("Bode tanque", "en ohmios: dB" S_OHM ", pico 80,00dB" S_OHM, m->tipo == BODE_PBANDA &&
                         tz_ve("dB" S_OHM "\n") && tz_ve("Pasa-banda  f0 1,592kHz  pico 80,00dB" S_OHM) && tz_ve("Q 100,0"));
                tz_tecla(T_LETRA('M'));
                chk_cond("Bode tanque", "[M]: «fc1: 76,99 dB" S_OHM "»", tz_ve("fc1: 76,99 dB" S_OHM));
                tz_tecla(T_LETRA('P'));
                chk_cond("Bode tanque", "la lectura: 10,00k" S_OHM, tz_ve("|H| 80,00dB" S_OHM " = 10,00k" S_OHM));
                break;
        case 5: chk_cond("Bode rechaza-banda", "Rechaza-banda  f0 1,592kHz", m->tipo == BODE_RB && tz_ve("Rechaza-banda  f0 1,592kHz") &&
                         tz_ve("fc1 983,6Hz  fc2 2,575kHz  BW 1,592kHz  Q 1,000")); break;
        case 6: chk_cond("Bode resistivo", "Plano, |H| = -6,02 dB", m->tipo == BODE_PLANO && tz_ve("Plano") &&
                         tz_ve("|H| = -6,02 dB en todo el barrido") && tz_ve("600mHz a 6kHz")); break;
        case 7: {
            int k0 = (g->n - 1) / 2, x;
            tz_dibuja();
            x = BD_X0 + k0;
            chk_cond("Bode LC sin R", "el punto con ok = 0 deja hueco", !g->ok[k0] && !tz_hay_color(x, BD_MY0, x, BD_MY0 + 4 * BD_MDY, C_CANAL1) &&
                     tz_hay_color(x - 1, BD_MY0, x - 1, BD_MY0 + 4 * BD_MDY, C_CANAL1));
            trz.fcur[0] = g->f[k0];
            chk_cond("Bode LC sin R", "el cursor justo ahí: singular", tz_ve("singular: resonancia ideal justo aqu" S_ii));
            break;
        }
        case 8: chk_cond("Bode cortado", "sin medidas y con lo barrido", g->n > 2 && g->n < BODE_N && tz_ve("Cortado: sin medidas") &&
                         traz_marcas(r, f, nom, 8) == 0 && trz.lf1 == log10(g->f[g->n - 1])); break;
        default: break;
        }
        tz_vuelca(NOMBRE[cual]);
        if (cual == 4) { tz_tecla(T_LETRA('X')); tz_vuelca("tanque_cursores"); }
        /* un mono: teclas al azar, dibujando cada vez */
        for (i = 0, bien = 1; i < 300; i++) {
            int tk = (i % 5 == 0) ? T_LETRA(LET[(i / 5) % 10]) : K[(i * 7 + cual) % 16];
            if (i % 11 == 0) tk |= T_CON_SHIFT;
            tz_tecla(tk);
            tz_dibuja();
            if (!tz_ventana_sana()) bien = 0;
        }
        chk_cond("trazador tipos", "300 teclas al azar: la ventana, las escalas y los cursores, sanos", bien);
        trz.dos = 1; trz.activo = 1; trz.fcur[1] = g->f[g->n - 1]; trz.fcur[0] = g->f[0];
        tz_dibuja();
    }
    pruebas++;
    if (gfx_desbordes() != 0) falla("trazador", "textos que se salen de la pantalla", gfx_desbordes(), 0);
}
