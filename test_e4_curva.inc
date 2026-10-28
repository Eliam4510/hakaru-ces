/* --------------------------------------------------------------------------
 *  63b. Etapa 4: la pantalla del barrido (curva.c, paso 11, §4.8)
 *
 *  Se incluye tras test_e4_paso.inc (usa rc, rr, c4_init, c4_el, nuevo_el,
 *  cierra_red, sr, sr_init, sr_el, a4_*, t4_d_cat, b4_canal, b4_ch y los
 *  chk). Barre con barr_iniciar (un Circuito) y con barr_iniciar_red; prueba
 *  las funciones puras, lo que se ve (cabecera, panel, avisos), las teclas
 *  del §4.8 con sus topes y que nada se sale de la pantalla. Vuelca las
 *  pantallas a out/curva_*.bmp (sin número: los de las demás pruebas no se
 *  mueven).
 * ----------------------------------------------------------------------- */
#include "curva.h"

static Curva cvz;

static int cv_t(int t) { return cv_tecla(&cvz, barr_reg(), barr_res(), t); }
static void cv_d(void) { cv_dibuja(&cvz, barr_reg(), barr_res()); }

static void cv_teclas(const int *t, int n)
{
    int i;
    for (i = 0; i < n; i++) { cv_d(); cv_t(t[i]); }
}
#define CV_TECLAS(...) do { static const int _t[] = { __VA_ARGS__ }; cv_teclas(_t, (int)(sizeof _t / sizeof _t[0])); } while (0)

/* ¿Se ve este texto? (se redibuja entera) */
static int cv_ve(const char *t)
{
    gfx_textos_reset();
    cv_d();
    return gfx_texto_visto(t);
}

static void cv_vuelca(const char *nombre)
{
    char ruta[256];
    cv_d();
    sprintf(ruta, "%s/curva_%s.bmp", dir_out, nombre);
    gfx_dump_bmp(ruta);
}

/* Barre del todo y mira el barrido con una Curva nueva */
static int cv_acaba(void)
{
    while (barr_avanzar(~0ul)) {}
    return barr_res()->fin == FIN_COMPLETO;
}

static void cv_empieza(void)
{
    memset(&cvz, 0, sizeof cvz);
    cv_nuevo(&cvz, barr_reg(), barr_res());
}

/* barr_iniciar_red no nombra lo barrido (el arnés arma la red a mano) */
static void cv_nombra(const char *nx, const char *unx)
{
    BarrRes *w = (BarrRes *)barr_res();
    strcpy(w->nom_x, nx);
    strcpy(w->uni_x, unx);
}

static void cv_canal_nom(int c, const char *nom) { strcpy(b4_ch[c].nombre, nom); }

/* ¿Hay algún píxel de color c en el rectángulo [x0, x1] x [y0, y1]? ¿Cuántos? */
static int cv_px(int x0, int y0, int x1, int y1, u16 c)
{
    const u16 *fb = gfx_buffer();
    int x, y, n = 0;
    for (y = y0; y <= y1; y++)
        for (x = x0; x <= x1; x++)
            if (x >= 0 && x < SCR_W && y >= 0 && y < SCR_H && fb[y * SCR_W + x] == c) n++;
    return n;
}

/* La autoescala deja dentro de la retícula todo lo que se ve de las trazas */
static int cv_dentro(void)
{
    const BarrReg *g = barr_reg();
    int t, i;
    for (t = 0; t < g->ntr; t++) {
        if (cvz.xy && g->canal[t] != 1) continue;
        for (i = 0; i < g->n; i++) {
            int y;
            if (!g->ok[t][i]) continue;
            y = cv_y(&cvz, t, g->y[t][i]);
            if (y < CV_Y0 || y > CV_Y0 + 8 * CV_DY) return 0;
        }
    }
    return 1;
}

/* ¿Empieza el aviso por p? */
static int cv_aviso_empieza(const char *p) { return strncmp(cvz.aviso, p, strlen(p)) == 0; }

static void test_e4_curva(void)
{
    const BarrReg *g;
    BarrAjustes aj;
    double x, y, d, xa, xb, x360, x50;
    int kE, kR, kD, k, i, r;

    printf("[63b] Etapa 4: la pantalla del barrido\n");
    gfx_desbordes_reset();

    /* ---- «¿Qué R da 20 mA?»: el LED rojo con E = 9 V, R1 de 10 Ω a 10 kΩ
     *      (log) y el dato I(D1) = 20 mA; con barr_iniciar ---- */
    c4_init(3);
    kE = c4_el(EL_FUENTE, 0, 1, 9.0);
    kR = c4_el(EL_RES, 1, 2, 1e3);
    kD = nuevo_el(EL_LED, 2, 0); rr.c[kD] = -1;
    strcpy(rc.el[kE].etiq, "E1"); strcpy(rc.el[kR].etiq, "R1"); strcpy(rc.el[kD].etiq, "D1");
    rc.el[kD].I.v = 20e-3; rc.el[kD].I.est = M_DATO;
    cierra_red();
    memset(&aj, 0, sizeof aj);
    aj.el = (signed char)kR; aj.x0 = 10.0; aj.x1 = 10e3; aj.log_x = 1;
    chk_cond("LED 20 mA", "barr_iniciar", barr_iniciar(&rc, &rr, &aj) == SIM_OK && cv_acaba());
    g = barr_reg();
    chk_cond("LED 20 mA", "los canales: I(D1) (el dato) y V(D1), sin repetir",
             strcmp(barr_res()->canal[0].nombre, "I(D1)") == 0 && strcmp(barr_res()->canal[1].nombre, "V(D1)") == 0 &&
             barr_res()->tr_dato == 0 && barr_res()->nivel_dato == 20e-3);
    cv_empieza();
    chk_t("LED 20 mA", "el cursor en el cruce: 360 " S_OHM, cvz.xc[0], 360.0, 360.0, 1e-9);
    x360 = cvz.xc[0];
    chk_cond("LED 20 mA", "el nivel del dato", cvz.nivel_on && cvz.nivel == 20e-3 && cvz.activo == 0);
    chk_cond("LED 20 mA", "el aviso «I(D1) = 20,00 mA en R1 = 360,0 " S_OHM "»",
             strcmp(cvz.aviso, "I(D1) = 20,00 mA en R1 = 360,0 " S_OHM) == 0 && cvz.aviso_col == C_CURSOR);
    chk_cond("LED 20 mA", "la ventana (log10): el barrido entero", cvz.log_x && cvz.x0 == log10(10.0) && cvz.x1 == log10(10e3));
    cv_ventana(&cvz, &xa, &xb);
    chk_t("LED 20 mA", "cv_ventana: 10 " S_OHM, xa, 10.0, 10.0, 1e-12);
    chk_t("LED 20 mA", "cv_ventana: 10 k" S_OHM, xb, 10e3, 10e3, 1e-12);
    chk_cond("LED 20 mA", "cv_x: los bordes y el centro (log)", cv_x(&cvz, 10.0) == CV_X0 && cv_x(&cvz, 10e3) == CV_X0 + 280 &&
             cv_x(&cvz, sqrt(10.0 * 10e3)) == CV_X0 + 140 && cv_x(&cvz, 1.0) == CV_X0 - 1 && cv_x(&cvz, 1e6) == CV_X0 + 281);
    chk_cond("LED 20 mA", "la autoescala deja las trazas dentro", cv_dentro());
    chk_cond("LED 20 mA", "cabecera", cv_ve("I(D1) y V(D1) frente a R1") && cv_ve("10" S_OHM " a 10k" S_OHM));
    chk_cond("LED 20 mA", "panel 1: la lectura exacta", cv_ve("R1 = 360,0 " S_OHM "  I(D1) = 20,00 mA  V(D1) = "));
    chk_cond("LED 20 mA", "panel 2: dy/dx en A/" S_OHM, cv_ve("dy/dx = -") && cv_ve(S_MU "A/" S_OHM));
    chk_cond("LED 20 mA", "panel 3: las escalas", cv_ve("C1 ") && cv_ve("mA/div") && cv_ve("x 0,3 d" S_ee "c/div"));
    chk_cond("LED 20 mA", "panel 4: el aviso", cv_ve("I(D1) = 20,00 mA en R1 = 360,0 " S_OHM));
    chk_cond("LED 20 mA", "el pie", cv_ve("[izq/der] cursor [N] nivel [+-] zoom [tab] circuito"));
    cv_vuelca("led_20mA");
    /* la lectura exacta en el cursor: 20 mA, y la R la baja */
    barr_en(0, x360, &y, &d);
    chk_t("LED 20 mA", "I en el cursor", y, 20e-3, 20e-3, 1e-9);
    chk_cond("LED 20 mA", "dI/dR < 0", d < 0.0);

    /* «[N] 2 0 [M] [enter]»: el mismo cruce */
    CV_TECLAS(T_LETRA('N'), T_DIG(2), T_DIG(0), T_LETRA('M'));
    chk_cond("LED [N]", "escribiendo el nivel", cv_ve(S_RARR " I(D1) = 20m_   (= 20,00 mA)") &&
             cv_ve("[enter] ir ah" S_ii "  [del] borrar  [esc] dejarlo"));
    cv_vuelca("led_escribe");
    CV_TECLAS(T_ENTER);
    chk_cond("LED [N]", "el mismo cruce, bit a bit", cvz.xc[0] == x360 && !cvz.entrada[0] && !cvz.de_nivel);
    chk_cond("LED [N]", "el mismo aviso", strcmp(cvz.aviso, "I(D1) = 20,00 mA en R1 = 360,0 " S_OHM) == 0);
    CV_TECLAS(T_LETRA('N') | T_CON_SHIFT);
    chk_cond("LED [N]", "[shift][N] con un cruce: el mismo", cvz.xc[0] == x360);
    /* sin cruce: el aviso en C_ERROR */
    CV_TECLAS(T_LETRA('N'), T_DIG(1), T_PUNTO, T_DIG(5), T_ENTER);
    chk_cond("LED [N]", "sin cruce: «no llega» en C_ERROR, el cursor no se mueve",
             strstr(cvz.aviso, "I(D1) no llega a 1,5 A: va de ") != 0 && cvz.aviso_col == C_ERROR && cvz.xc[0] == x360);
    chk_cond("LED [N]", "el aviso cabe", strlen(cvz.aviso) <= 52);
    cv_vuelca("led_sin_cruce");
    CV_TECLAS(T_LETRA('N'), T_SIGNO, T_DIG(5), T_LETRA('M'), T_ENTER);
    chk_cond("LED [N]", "un nivel negativo: «no baja a»", strstr(cvz.aviso, "I(D1) no baja a -5 mA") != 0 && cvz.aviso_col == C_ERROR);
    /* escribiendo: prefijos, [N] es nano, (-) tras la E, [del] y [esc] */
    CV_TECLAS(T_LETRA('N'), T_DIG(1), T_LETRA('N'));
    chk_cond("LED [N]", "escribiendo, [N] es el prefijo n", strcmp(cvz.entrada, "1n") == 0 && cvz.de_nivel);
    CV_TECLAS(T_BORRAR, T_BORRAR);
    chk_cond("LED [N]", "[del] borra", !cvz.entrada[0] && cvz.de_nivel);
    CV_TECLAS(T_BORRAR);
    chk_cond("LED [N]", "[del] con nada: deja de escribir", !cvz.de_nivel);
    CV_TECLAS(T_LETRA('N'), T_SIGNO, T_DIG(2), T_EE, T_MENOS, T_DIG(2));
    chk_cond("LED [N]", "(-) al principio y tras la E", strcmp(cvz.entrada, "-2E-2") == 0);
    CV_TECLAS(T_ESC);
    chk_cond("LED [N]", "[esc] lo deja", !cvz.entrada[0] && !cvz.de_nivel && cvz.xc[0] == x360);
    /* un x: la cifra empieza; fuera del barrido, el aviso */
    CV_TECLAS(T_DIG(5), T_LETRA('K'));
    chk_cond("LED x", "escribiendo un x", cv_ve(S_RARR " R1 = 5k_   (= 5,000 k" S_OHM ")"));
    CV_TECLAS(T_ENTER);
    chk_t("LED x", "el cursor en 5 k" S_OHM, cvz.xc[0], 5e3, 5e3, 1e-15);
    CV_TECLAS(T_DIG(2), T_DIG(0), T_LETRA('K'), T_ENTER);
    chk_cond("LED x", "fuera del barrido", strcmp(cvz.aviso, "Fuera del barrido: [menu] cambia el rango.") == 0 &&
             cvz.aviso_col == C_ERROR && cvz.xc[0] == 5e3);
    /* [izq/der]: un punto de la malla; con [shift], una división */
    CV_TECLAS(T_DIG(1), T_DIG(0), T_ENTER, T_IZQ);
    chk_cond("LED cursor", "en el borde no se mueve", cvz.xc[0] == 10.0);
    CV_TECLAS(T_DER);
    chk_cond("LED cursor", "[der]: el punto siguiente", cvz.xc[0] == g->x[1]);
    CV_TECLAS(T_DER | T_CON_SHIFT);
    chk_cond("LED cursor", "[shift][der]: 28 puntos", cvz.xc[0] == g->x[29]);
    CV_TECLAS(T_IZQ | T_CON_SHIFT);
    chk_cond("LED cursor", "[shift][izq]: 28 atr" S_aa "s", cvz.xc[0] == g->x[1]);
    cv_ir_nivel(&cvz, g, barr_res(), 0, 20e-3, 1);
    chk_cond("LED cursor", "cv_ir_nivel (un dato): otra vez 360 " S_OHM, cvz.xc[0] == x360);

    /* [+] y [-]: sus topes (4 intervalos; el barrido entero) */
    for (i = 0; i < 12; i++) CV_TECLAS(T_MAS);
    chk_t("LED zoom", "[+]: el tope, 4 intervalos de la malla", cvz.x1 - cvz.x0, 4.0 * 3.0 / 280.0, 1.0, 1e-12);
    chk_cond("LED zoom", "con el cursor a la vista y el aviso de pocas muestras",
             log10(x360) >= cvz.x0 && log10(x360) <= cvz.x1 && strstr(cvz.aviso, "Pocas muestras aqu" S_ii) != 0);
    chk_cond("LED zoom", "las d" S_ee "cadas por divisi" S_oo "n, con dos cifras", cv_ve("x 0,0043 d" S_ee "c/div"));
    cv_vuelca("led_zoom");
    for (i = 0; i < 12; i++) CV_TECLAS(T_MENOS);
    chk_cond("LED zoom", "[-]: el tope, el barrido entero", cvz.x0 == log10(10.0) && cvz.x1 == log10(10e3));
    /* [=] barre la ventana: cv_nuevo conserva el cursor y busca el dato otra vez */
    CV_TECLAS(T_MAS, T_MAS);
    r = cv_t(T_IGUAL);
    chk_cond("LED [=]", "CV_BARRER", r == CV_BARRER);
    cv_ventana(&cvz, &xa, &xb);
    aj.x0 = xa; aj.x1 = xb;
    chk_cond("LED [=]", "barre lo que se ve", barr_iniciar(&rc, &rr, &aj) == SIM_OK && cv_acaba() && g->n == BARR_N);
    cv_nuevo(&cvz, g, barr_res());
    chk_cond("LED [=]", "la ventana nueva y el mismo cruce", fabs(cvz.x0 - log10(xa)) < 1e-12 && fabs(cvz.x1 - log10(xb)) < 1e-12);
    chk_t("LED [=]", "360 " S_OHM, cvz.xc[0], 360.0, 360.0, 1e-9);
    aj.x0 = 10.0; aj.x1 = 10e3;
    barr_iniciar(&rc, &rr, &aj); cv_acaba(); cv_empieza();

    /* [C], [del], [A], [arr/ab] con sus topes */
    CV_TECLAS(T_LETRA('C'));
    chk_cond("LED canal", "[C]: el canal 2 (el nivel, que era de I(D1), se quita)", cvz.activo == 1 && !cvz.nivel_on);
    CV_TECLAS(T_LETRA('C'));
    chk_cond("LED canal", "[C]: el canal 1", cvz.activo == 0);
    k = cvz.idiv[0];
    for (i = 0; i < 60; i++) CV_TECLAS(T_ARRIBA);
    chk_cond("LED escala", "[arr]: el tope de ESCALA", cvz.idiv[0] == 0);
    for (i = 0; i < 60; i++) CV_TECLAS(T_ABAJO);
    chk_cond("LED escala", "[ab]: el tope de ESCALA", cvz.idiv[0] == N_ESCALA - 1);
    for (i = 0; i < 20; i++) CV_TECLAS(T_ARRIBA | T_CON_SHIFT);
    chk_cond("LED escala", "[shift][arr]: la posici" S_oo "n, tope 8", cvz.pos[0] == 8);
    for (i = 0; i < 20; i++) CV_TECLAS(T_ABAJO | T_CON_SHIFT);
    chk_cond("LED escala", "[shift][ab]: tope -8", cvz.pos[0] == -8);
    CV_TECLAS(T_LETRA('A'));
    chk_cond("LED escala", "[A]: la autoescala", cvz.idiv[0] == k && cv_dentro());
    CV_TECLAS(T_BORRAR);
    chk_cond("LED ver", "[del] oculta la activa", !cvz.ver[0] && cvz.ver[1]);
    CV_TECLAS(T_BORRAR);
    chk_cond("LED ver", "[del] la ense" S_nn "a", cvz.ver[0]);

    /* [X]: dos cursores; [enter] cambia; [X] otra vez, uno */
    CV_TECLAS(T_LETRA('X'));
    chk_cond("LED [X]", "dos cursores, el 2 activo y a la vista", cvz.dos && cvz.cur == 1 && cvz.xc[1] != cvz.xc[0] &&
             cvz.xc[1] >= 10.0 && cvz.xc[1] <= 10e3);
    chk_cond("LED [X]", "panel: x1, x2 y las diferencias", cv_ve("x1 = 360,0 " S_OHM "  x2 = ") && cv_ve(S_DELTA " I(D1) = ") &&
             cv_ve("   " S_DELTA "y/" S_DELTA "x = "));
    cv_vuelca("led_dos_cursores");
    CV_TECLAS(T_ENTER);
    chk_cond("LED [X]", "[enter] cambia de cursor", cvz.cur == 0);
    CV_TECLAS(T_LETRA('X'));
    chk_cond("LED [X]", "[X]: un cursor", !cvz.dos && cvz.cur == 0 && cvz.xc[0] == x360);
    CV_TECLAS(T_ENTER);
    chk_cond("LED [X]", "[enter] con uno: el aviso", strcmp(cvz.aviso, "[X] enciende el segundo cursor.") == 0);

    /* [Y]: X-Y (V(D1) frente a I(D1)) y vuelta */
    CV_TECLAS(T_LETRA('Y'));
    chk_cond("LED [Y]", "X-Y con el canal 2 activo", cvz.xy && cvz.activo == 1 && cv_dentro());
    chk_cond("LED [Y]", "cabecera y derivada de X-Y", cv_ve("V(D1) frente a I(D1)") && cv_ve("r = dV/dI = ") && cv_ve(" (x)"));
    cv_vuelca("led_xy");
    CV_TECLAS(T_LETRA('C'));
    chk_cond("LED [Y]", "en X-Y, [C] no pasa al eje x", cvz.activo == 1);
    CV_TECLAS(T_LETRA('Y'));
    chk_cond("LED [Y]", "y(x) otra vez", !cvz.xy && cv_ve("I(D1) y V(D1) frente a R1"));
    /* [L]: |y| logarítmica, con sus topes */
    CV_TECLAS(T_LETRA('C'), T_LETRA('L'));
    chk_cond("LED [L]", "|I(D1)| en log", cvz.log_y[0] && !cvz.log_y[1] && cvz.activo == 0 && cv_ve("C1 1 d" S_ee "c/div") && cv_dentro());
    cv_vuelca("led_log");
    for (i = 0; i < 12; i++) CV_TECLAS(T_ARRIBA);
    chk_cond("LED [L]", "[arr]: el tope de abajo (1 pA)", cvz.pos[0] == -4);
    for (i = 0; i < 12; i++) CV_TECLAS(T_ABAJO | T_CON_SHIFT);
    chk_cond("LED [L]", "[ab]: el tope de arriba (1 kA)", cvz.pos[0] == 3);
    chk_cond("LED [L]", "el 0 y lo de abajo, al pie", cv_y(&cvz, 0, 0.0) == CV_Y0 + 144 && cv_y(&cvz, 0, -1.0) == CV_Y0 + 144 &&
             cv_y(&cvz, 0, 1e-9) == CV_Y0 + 144 && cv_y(&cvz, 0, 1e3) == CV_Y0 && cv_y(&cvz, 0, 1.0) == CV_Y0 + 54);
    CV_TECLAS(T_LETRA('L'));
    chk_cond("LED [L]", "lineal otra vez", !cvz.log_y[0] && cv_dentro());
    /* [P]: el máximo (en el borde: 10 Ω) y el mínimo (10 kΩ) */
    CV_TECLAS(T_LETRA('P'));
    chk_cond("LED [P]", "el m" S_aa "ximo, en 10 " S_OHM, cvz.xc[0] == 10.0 && cv_aviso_empieza("M" S_aa "ximo: I(D1) = "));
    CV_TECLAS(T_LETRA('P') | T_CON_SHIFT);
    chk_cond("LED [P]", "el m" S_ii "nimo, en 10 k" S_OHM, cvz.xc[0] == 10e3 && cv_aviso_empieza("M" S_ii "nimo: I(D1) = "));
    /* 11: lo que hace ui.c */
    chk_cond("LED 11", "[tab] [menu] [esc]", cv_t(T_TAB) == CV_CIRCUITO && cv_t(T_MENU) == CV_AJUSTES && cv_t(T_ESC) == CV_SALIR);
    chk_cond("LED 11", "las dem" S_aa "s, nada", cv_t(T_LETRA('Q')) == CV_NADA && cv_t(T_CATALOGO) == CV_NADA);

    /* ---- Sin dato: los canales automáticos y su aviso ---- */
    rc.el[kD].I.est = M_LIBRE;
    rc.el[kD].variante = LED_VERDE;
    barr_iniciar(&rc, &rr, &aj); cv_acaba(); cv_empieza();
    chk_cond("sin sondas", "el aviso", strcmp(cvz.aviso, "Sin sondas: I(D1) y V(D1). Pon las tuyas con [O].") == 0 &&
             cvz.aviso_col == C_SUAVE && !cvz.nivel_on);
    cv_vuelca("led_sin_sondas");
    /* R1 con [?!] (lo automático: de 1 Ω a 1 MΩ, log) y el dato: otra vez 360 Ω */
    rc.el[kD].variante = LED_ROJO;
    rc.el[kD].I.v = 20e-3; rc.el[kD].I.est = M_DATO;
    rc.el[kR].val.est = M_BUSCAR;
    memset(&aj, 0, sizeof aj); aj.el = -1;
    chk_cond("R1 con [?!]", "barr_iniciar autom" S_aa "tico", barr_iniciar(&rc, &rr, &aj) == SIM_OK && cv_acaba() &&
             barr_res()->el == kR && barr_res()->x0 == 1.0 && barr_res()->x1 == 1e6 && barr_res()->log_x);
    cv_empieza();
    chk_t("R1 con [?!]", "el cursor en 360 " S_OHM, cvz.xc[0], 360.0, 360.0, 1e-9);
    chk_cond("R1 con [?!]", "se ve 360,0", cv_ve("360,0") && cv_ve("1" S_OHM " a 1M" S_OHM));

    /* ---- El inversor real con el dato V(U1) = 12 V: el cruce en -12 V ---- */
    {
        int ka, t;
        c4_init(4);
        k = c4_el(EL_FUENTE, 0, 1, 1.0); c4_el(EL_RES, 1, 2, 1e3); c4_el(EL_RES, 2, 3, 10e3);
        ka = nuevo_el(EL_AO, 0, 2); rr.c[ka] = 3; rc.el[ka].variante = AO_REAL;
        for (t = 0; t < rc.nel; t++) if (t != ka) rr.c[t] = -1;
        strcpy(rc.el[k].etiq, "E1"); strcpy(rc.el[ka].etiq, "U1");
        rc.el[ka].V.v = 12.0; rc.el[ka].V.est = M_DATO;
        cierra_red();
        memset(&aj, 0, sizeof aj);
        aj.el = (signed char)k; aj.x0 = 0.0; aj.x1 = 2.0;
        chk_cond("inversor", "barr_iniciar", barr_iniciar(&rc, &rr, &aj) == SIM_OK && cv_acaba());
        cv_empieza();
        chk_t("inversor", "el cruce de -12 V: E1 = 1,200132", cvz.xc[0], 1.200132, 1.2, 1e-9);
        chk_cond("inversor", "con el signo del que encontr" S_oo,
                 strcmp(cvz.aviso, "Vsal(U1) = -12,00 V en E1 = 1,200 V") == 0 && cvz.nivel == -12.0);
        chk_cond("inversor", "cabecera: Vsal(U1) y I(R2)", cv_ve("Vsal(U1) y I(R2) frente a E1") && cv_ve("0 a 2 V"));
        CV_TECLAS(T_DIG(0), T_PUNTO, T_DIG(5), T_ENTER);
        chk_cond("inversor", "la ganancia en 0,5 V", cv_ve("ganancia dy/dx = -9,999") && cv_ve("Vsal(U1) = -4,999 V"));
        cv_vuelca("inversor");
        CV_TECLAS(T_LETRA('P') | T_CON_SHIFT);
        barr_en(0, cvz.xc[0], &y, 0);
        chk_cond("inversor", "[shift][P]: el m" S_ii "nimo, saturado", y <= -14.999 && cv_aviso_empieza("M" S_ii "nimo: Vsal(U1) = "));
    }

    /* ---- barr_iniciar_red: el 1N4148 con 1 kΩ, E de 0 a 5 V ---- */
    sr_init(3); k = a4_cc(0, 1, 1.0); sr_el(SE_R, 1, 2, 1e3); kD = t4_d_cat(2, 0, &NL_1N4148);
    b4_canal(0, CANAL_I, kD, -1, 1); b4_canal(1, CANAL_V, kD, -1, 1);
    cv_canal_nom(0, "I(D1)"); cv_canal_nom(1, "V(D1)");
    chk_cond("1N4148", "barr_iniciar_red", barr_iniciar_red(&sr, k, BX_E, b4_ch, 0.0, 5.0, 0, 0) == SIM_OK && cv_acaba());
    cv_nombra("E1", "V");
    cv_empieza();
    x50 = g->x[50];
    chk_cond("1N4148", "sin dato ni aviso", !cvz.aviso[0] && !cvz.nivel_on && cvz.xc[0] == 2.5 && cvz.xc[1] == 3.75);
    chk_cond("1N4148", "cv_x lineal", cv_x(&cvz, 0.0) == CV_X0 && cv_x(&cvz, 5.0) == CV_X0 + 280 && cv_x(&cvz, 2.5) == CV_X0 + 140);
    chk_cond("1N4148", "sin aviso: las teclas", cv_ve("[C] canal [N] nivel [X] 2 cursores [Y] X-Y [L] log") && cv_ve("0 a 5 V"));
    /* cv_valor: la malla exacta; entre dos puntos, la recta; fuera, NaN */
    y = cv_valor(g, 0, 0.5 * (g->x[100] + g->x[101]));
    chk_cond("cv_valor", "en la malla, bit a bit", cv_valor(g, 0, g->x[100]) == g->y[0][100] && cv_valor(g, 1, g->x[0]) == g->y[1][0]);
    chk_t("cv_valor", "entre dos puntos, la media", y, 0.5 * (g->y[0][100] + g->y[0][101]), 1e-3, 1e-15);
    x = cv_valor(g, 0, 5.1); y = cv_valor(g, 0, -0.1); d = cv_valor(g, 5, 1.0);
    chk_cond("cv_valor", "fuera del barrido o de las trazas: NaN", x != x && y != y && d != d);
    /* cv_cruces */
    {
        double xs[4];
        chk_cond("cv_cruces", "1 mA: un cruce", cv_cruces(g, 0, 1e-3, xs, 4) == 1 && fabs(xs[0] - 1.5847396946292637) < 1e-3);
        chk_cond("cv_cruces", "50 mA: ninguno", cv_cruces(g, 0, 50e-3, xs, 4) == 0);
    }
    /* [N] 1 [M] [enter]: el cruce exacto, desde el cursor hacia la derecha dando la vuelta */
    CV_TECLAS(T_LETRA('N'), T_DIG(1), T_LETRA('M'), T_ENTER);
    chk_t("1N4148", "[N] 1 mA: E1 = 1,5847396946292637 V", cvz.xc[0], 1.5847396946292637, 1.58, 1e-9);
    chk_cond("1N4148", "el aviso", strcmp(cvz.aviso, "I(D1) = 1,000 mA en E1 = 1,585 V") == 0);
    CV_TECLAS(T_DIG(5), T_ENTER);
    chk_cond("1N4148", "la lectura y g = dI/dV en 5 V", cv_ve("E1 = 5,000 V  I(D1) = 4,347 mA  V(D1) = ") &&
             cv_ve("g = dI/dV = 989,1 " S_MU "S   r = 1,011 k" S_OHM));
    cv_vuelca("1n4148");
    CV_TECLAS(T_DIG(6), T_ENTER);
    chk_cond("1N4148", "6 V: fuera del barrido", strcmp(cvz.aviso, "Fuera del barrido: [menu] cambia el rango.") == 0 && cvz.xc[0] == 5.0);
    CV_TECLAS(T_LETRA('C'), T_DIG(5), T_ENTER);
    chk_cond("1N4148", "el canal 2 (V frente a V): ganancia", cvz.activo == 1 && cv_ve("ganancia dy/dx = "));
    /* un barrido cortado */
    barr_iniciar_red(&sr, k, BX_E, b4_ch, 0.0, 5.0, 0, 0);
    for (i = 0; i < 40; i++) barr_avanzar(1ul);
    barr_cortar();
    cv_nombra("E1", "V");
    cv_empieza();
    chk_cond("cortado", "el aviso: el x del punto 40", barr_res()->fin == FIN_CORTADO && strcmp(cvz.aviso, "Cortado en E1 = 696,4 mV.") == 0);
    chk_cond("cortado", "los cursores, en lo hecho", cvz.xc[0] == g->x[39] && cvz.xc[1] == g->x[39]);
    cv_vuelca("cortado");

    /* ---- Huecos (sim_nl_prueba): la curva se corta y el aviso ---- */
    sim_nl_prueba(NLP_FALLA_BARR, x50);
    barr_iniciar_red(&sr, k, BX_E, b4_ch, 0.0, 5.0, 0, 0);
    cv_acaba();
    sim_nl_prueba(NLP_NADA, 0.0);
    cv_nombra("E1", "V");
    cv_empieza();
    chk_cond("huecos", "un hueco en x[50]", !g->ok[0][50] && barr_res()->huecos == 1);
    chk_cond("huecos", "el aviso", strcmp(cvz.aviso, "La curva tiene 1 hueco: ah" S_ii " Newton no converge.") == 0);
    chk_cond("huecos", "cv_valor: NaN a los dos lados", cv_valor(g, 0, g->x[50]) != cv_valor(g, 0, g->x[50]) &&
             cv_valor(g, 0, 0.5 * (g->x[49] + g->x[50])) != cv_valor(g, 0, 0.5 * (g->x[49] + g->x[50])) &&
             cv_valor(g, 0, g->x[51]) == g->y[0][51]);
    cv_d();
    chk_cond("huecos", "la traza se corta en su columna", cv_px(CV_X0 + 50, CV_Y0, CV_X0 + 50, CV_Y0 + 144, C_CANAL1) == 0 &&
             cv_px(CV_X0 + 52, CV_Y0, CV_X0 + 52, CV_Y0 + 144, C_CANAL1) > 0);
    cv_vuelca("hueco");

    /* ---- Dos cruces: V(R4) = V(diodo) - E/2 sube y baja ---- */
    {
        double xs[4], x1, x2;
        int kR4;
        sr_init(4); k = a4_cc(0, 1, 0.0); sr_el(SE_R, 1, 2, 1e3); t4_d_cat(2, 0, &NL_1N4148);
        sr_el(SE_R, 1, 3, 1e3); sr_el(SE_R, 3, 0, 1e3); kR4 = sr_el(SE_R, 2, 3, 1e6);
        b4_canal(0, CANAL_V, kR4, -1, 1); b4_canal(1, CANAL_NADA, -1, -1, 1);
        cv_canal_nom(0, "V(R4)");
        barr_iniciar_red(&sr, k, BX_E, b4_ch, 0.0, 5.0, 0, 0);
        chk_cond("dos cruces", "barre", cv_acaba());
        cv_nombra("E1", "V");
        cv_empieza();
        chk_cond("dos cruces", "cv_cruces: 2, de menor a mayor", cv_cruces(g, 0, 0.1, xs, 4) == 2 && xs[0] < xs[1]);
        CV_TECLAS(T_LETRA('N'), T_DIG(1), T_DIG(0), T_DIG(0), T_LETRA('M'), T_ENTER);
        x1 = cvz.xc[0];
        chk_cond("dos cruces", "desde el centro no hay a la derecha: la vuelta, el 1 de 2",
                 strstr(cvz.aviso, "(1 de 2)") != 0 && fabs(x1 - xs[0]) < 0.02);
        barr_en(0, x1, &y, 0);
        chk_t("dos cruces", "V(R4) = 100 mV en el primero", y, 0.1, 0.1, 1e-9);
        cv_vuelca("dos_cruces");
        CV_TECLAS(T_LETRA('N') | T_CON_SHIFT);
        x2 = cvz.xc[0];
        barr_en(0, x2, &y, 0);
        chk_cond("dos cruces", "[shift][N]: el 2 de 2", strstr(cvz.aviso, "(2 de 2)") != 0 && x2 > x1 && fabs(x2 - xs[1]) < 0.02);
        chk_t("dos cruces", "V(R4) = 100 mV en el segundo", y, 0.1, 0.1, 1e-9);
        CV_TECLAS(T_LETRA('N') | T_CON_SHIFT);
        chk_cond("dos cruces", "[shift][N]: la vuelta al 1", cvz.xc[0] == x1 && strstr(cvz.aviso, "(1 de 2)") != 0);
        /* [P]: un máximo de dentro, por la sección áurea */
        CV_TECLAS(T_LETRA('P'));
        barr_en(0, cvz.xc[0], &y, &d);
        for (x = -1e300, i = 0; i < g->n; i++) if (g->y[0][i] > x) x = g->y[0][i];
        chk_cond("dos cruces", "[P]: m" S_aa "s alto que la malla y con dy/dx = 0", y >= x && fabs(d) < 1e-6 && cvz.xc[0] > x1 && cvz.xc[0] < x2);
    }

    /* ---- El Schmitt ideal, ida y vuelta de -10 a 10 V ---- */
    sr_init(4);
    k = a4_cc(0, 1, 0.0);
    sr_el(SE_R, 3, 2, 10e3); sr_el(SE_R, 2, 0, 10e3);
    a4_ao(2, 1, 3, 0, 0, 0.0);
    b4_canal(0, CANAL_NODO, -1, 3, 1); b4_canal(1, CANAL_NADA, -1, -1, 1);
    cv_canal_nom(0, "Vsal(U1)");
    barr_iniciar_red(&sr, k, BX_E, b4_ch, -10.0, 10.0, 0, 1);
    chk_cond("Schmitt", "barre", cv_acaba() && g->ntr == 2 && g->recorrido[1] == 1);
    cv_nombra("E1", "V");
    cv_empieza();
    chk_cond("Schmitt", "el aviso del salto", strcmp(cvz.aviso, "La curva salta en E1 = 7,50 V (hist" S_ee "resis).") == 0);
    CV_TECLAS(T_DIG(0), T_ENTER);
    chk_cond("Schmitt", "en 0, la ida lee +15 V", cvz.activo == 0 && cv_ve("E1 = 0,000 V  Vsal(U1) = 15,00 V"));
    CV_TECLAS(T_LETRA('C'));
    chk_cond("Schmitt", "[C]: la vuelta lee -15 V", cvz.activo == 1 && cv_ve("E1 = 0,000 V  Vsal(U1) = -15,00 V"));
    CV_TECLAS(T_LETRA('C'));
    cv_vuelca("schmitt");
    {
        /* la ida, entera en +15 entre -5 y -1 V; la vuelta, discontinua en -15;
         * el salto de la ida, un trazo vertical punteado en la columna de x[246] */
        int y15 = cv_y(&cvz, 0, 15.0), ym15 = cv_y(&cvz, 1, -15.0), c0 = cv_x(&cvz, -5.0), c1 = cv_x(&cvz, -1.0);
        int n_ida = cv_px(c0, y15, c1, y15, C_CANAL1), n_vta = cv_px(c0, ym15, c1, ym15, C_CANAL1);
        int cs = CV_X0 + 246, n_salto = cv_px(cs, y15 + 2, cs, ym15 - 2, C_CANAL1);
        chk_cond("Schmitt", "la ida, continua", n_ida == c1 - c0 + 1);
        chk_cond("Schmitt", "la vuelta, discontinua", n_vta > (c1 - c0) / 2 && n_vta < c1 - c0 + 1);
        chk_cond("Schmitt", "el salto, punteado", n_salto > 5 && n_salto < ym15 - y15 - 3);
        chk_cond("Schmitt", "la marca del cero", cv_px(315, CV_YC - 3, 319, CV_YC + 3, C_CANAL1) > 0);
    }
    /* un nivel en la ida: [N] 0 [enter] desde 0 va al salto (7,5 V) */
    CV_TECLAS(T_LETRA('N'), T_DIG(0), T_ENTER);
    chk_cond("Schmitt", "el nivel 0 en la ida: entre 7,5 V y el punto siguiente", cvz.xc[0] > 7.5 - 1e-9 && cvz.xc[0] <= g->x[246] &&
             cvz.nivel_on && cvz.nivel == 0.0);
    chk_cond("Schmitt", "en un salto, «salta por»", strcmp(cvz.aviso, "Vsal(U1) salta por 0,000 V en E1 = 7,500 V") == 0);
    CV_TECLAS(T_LETRA('C'), T_LETRA('N') | T_CON_SHIFT);
    chk_cond("Schmitt", "[C] a la vuelta (el mismo canal): el nivel sigue; [shift][N], su salto en -7,5 V",
             cvz.activo == 1 && cvz.nivel_on && fabs(cvz.xc[0] + 7.5) < 0.08 && strstr(cvz.aviso, "salta por 0,000 V en E1 = -7,") != 0);
    cv_vuelca("schmitt_nivel");

    /* ---- El trazador: la curva I-V del 1N4148 (barr_iniciar_curva), en X-Y ---- */
    c4_init(2);
    kD = nuevo_el(EL_DIODO, 1, 0); rr.c[kD] = -1; rc.el[kD].variante = DIO_1N4148;
    strcpy(rc.el[kD].etiq, "D1");
    chk_cond("curva I-V", "barr_iniciar_curva", barr_iniciar_curva(&rc, kD) == SIM_OK && cv_acaba());
    cv_empieza();
    chk_cond("curva I-V", "empieza en X-Y con I(D1) activa", cvz.xy && g->canal[cvz.activo] == 1 && cv_dentro());
    chk_cond("curva I-V", "el t" S_ii "tulo", cv_ve("Curva I-V de D1 (1N4148)") && strcmp(barr_res()->titulo, "Curva I-V de D1 (1N4148)") == 0);
    CV_TECLAS(T_LETRA('N'), T_DIG(1), T_LETRA('M'), T_ENTER);
    barr_en(0, cvz.xc[0], &y, 0);
    chk_t("curva I-V", "V a 1 mA = 0,58473969462926372", y, 0.58473969462926372, 0.58, 1e-9);
    chk_cond("curva I-V", "la lectura y r = n" S_CDOT "VT/I + Rs", cv_ve("V(D1) = 584,7 mV  I(D1) = 1,000 mA") &&
             cv_ve("g = dI/dV = ") && cv_ve("r = 45,88 " S_OHM));
    chk_cond("curva I-V", "el pie sin [tab]", cv_ve("[izq/der] cursor [N] nivel [+-] zoom [esc] volver"));
    cv_vuelca("curva_1n4148");
    r = cv_t(T_TAB);
    chk_cond("curva I-V", "[tab], [=] y [menu]: sin circuito", r == CV_NADA && cv_t(T_IGUAL) == CV_NADA && cv_t(T_MENU) == CV_NADA &&
             strcmp(cvz.aviso, "La curva de un componente no tiene circuito.") == 0);
    CV_TECLAS(T_LETRA('Y'));
    /* E de -2 V a V(100 mA) + 0,1 V = 0,9497 V (§4.10): 295 mV por división */
    chk_cond("curva I-V", "[Y]: y(x) (frente a E)", !cvz.xy && cv_dentro() && cv_ve("x 295 mV/div"));
    cv_vuelca("curva_1n4148_yx");
    rc.el[kD].variante = DIO_VG;
    barr_iniciar_curva(&rc, kD); cv_acaba(); cv_empieza();
    chk_cond("curva I-V", "el t" S_ii "tulo con V" S_GAMMA, cv_ve("Curva I-V de D1 (con V" S_GAMMA ")"));

    /* ---- Nombres largos (11 columnas): todo cabe en 52 (los desbordes, al final) ---- */
    sr_init(3); k = a4_cc(0, 1, 1.0); sr_el(SE_R, 1, 2, 1e3); kD = t4_d_cat(2, 0, &NL_1N4148);
    b4_canal(0, CANAL_I, kD, -1, -1); b4_canal(1, CANAL_V, kD, -1, 1);
    cv_canal_nom(0, "Vsal(U12)ab"); cv_canal_nom(1, "I(R12)abcde");
    barr_iniciar_red(&sr, k, BX_E, b4_ch, -500.0, 999.0, 0, 0);
    cv_acaba();
    cv_nombra("R12abcdefgh", S_OHM);
    cv_empieza();
    CV_TECLAS(T_DIG(7), T_DIG(7), T_DIG(7), T_PUNTO, T_DIG(7), T_DIG(7), T_DIG(7), T_DIG(7), T_DIG(7), T_DIG(7), T_DIG(7));
    cv_d();
    CV_TECLAS(T_ENTER, T_LETRA('P'), T_LETRA('N'), T_SIGNO, T_DIG(9), T_DIG(9), T_DIG(9), T_ENTER);
    chk_cond("nombres largos", "los avisos, en 52", strlen(cvz.aviso) <= 52);
    CV_TECLAS(T_LETRA('N'), T_DIG(1), T_ENTER, T_LETRA('X'));
    cv_d();
    CV_TECLAS(T_LETRA('Y'));
    cv_d();
    CV_TECLAS(T_LETRA('L'), T_LETRA('C'));
    chk_cond("nombres largos", "el rango", cv_ve("-500 a 999 " S_OHM));
    cv_vuelca("nombres_largos");

    /* ---- Un error en el primer punto: el texto y [esc] ---- */
    sr_init(3); k = a4_cc(0, 1, 1.0); sr_el(SE_R, 1, 2, 1e3);
    b4_canal(0, CANAL_V, 1, -1, 1); b4_canal(1, CANAL_NADA, -1, -1, 1);
    chk_cond("error", "barr_iniciar_red con un elemento que no existe", barr_iniciar_red(&sr, 7, BX_E, b4_ch, 0.0, 1.0, 0, 0) != SIM_OK &&
             barr_res()->fin == FIN_ERROR);
    cv_empieza();
    chk_cond("error", "el pie", cv_ve("[esc] volver") && cv_ve("Barrido de CC"));
    cv_vuelca("error");
    chk_cond("error", "solo [esc]", cv_t(T_DER) == CV_NADA && cv_t(T_IGUAL) == CV_NADA && cv_t(T_ESC) == CV_SALIR);

    pruebas++;
    if (gfx_desbordes() != 0) falla("curva", "textos que se salen de la pantalla", gfx_desbordes(), 0);
}
