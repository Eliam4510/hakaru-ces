/* --------------------------------------------------------------------------
 *  65b. Etapa 4: el punto de operación y el barrido en la interfaz (paso 11)
 *
 *  P_RESULTADO y P_TABLA con res_nl, P_BARRIDO con sus ajustes, la barra del
 *  barrido, P_MODELO con la curva I-V y el emulador con no lineales. Se
 *  incluye tras test_e4_ui.inc (usa r3u_abre, r3u_espera, ve, vuelca, TECLAS).
 * ----------------------------------------------------------------------- */

/* E de 9 V, R de 330 Ω y un LED rojo en un lazo: E (1,2)->(1,1), R (1,1)->(2,1),
 * el LED (2,1)->(2,2) y el cable (2,2)-(1,2); la tierra en (1,2) */
static void r4_led(Circuito *c, double E, double R)
{
    int k;
    circ_init(c, "LED");
    k = circ_elemento(c, EL_FUENTE, 1, 2, DIR_ARR);
    c->el[k].val.v = E; c->el[k].val.est = M_DATO;
    k = circ_elemento(c, EL_RES, 1, 1, DIR_DER);
    c->el[k].val.v = R; c->el[k].val.est = M_DATO;
    k = circ_elemento(c, EL_LED, 2, 1, DIR_ABA);
    c->el[k].variante = LED_ROJO;
    circ_cable(c, 2, 2, DIR_IZQ);
    c->tierra_x = 1; c->tierra_y = 2;
}

/* Un lazo por el borde de la rejilla con E, R y n diodos en serie (de la
 * variante var), en el sentido de la corriente */
static void r4_serie(Circuito *c, double E, double R, int n, int var)
{
    static const signed char X[30] = { 0,1,2,3,4,5,6,7,8, 9,9,9,9,9,9, 9,8,7,6,5,4,3,2,1, 0,0,0,0,0,0 };
    static const signed char Y[30] = { 0,0,0,0,0,0,0,0,0, 0,1,2,3,4,5, 6,6,6,6,6,6,6,6,6, 6,5,4,3,2,1 };
    static const signed char D[30] = { DIR_DER,DIR_DER,DIR_DER,DIR_DER,DIR_DER,DIR_DER,DIR_DER,DIR_DER,DIR_DER,
                                       DIR_ABA,DIR_ABA,DIR_ABA,DIR_ABA,DIR_ABA,DIR_ABA,
                                       DIR_IZQ,DIR_IZQ,DIR_IZQ,DIR_IZQ,DIR_IZQ,DIR_IZQ,DIR_IZQ,DIR_IZQ,DIR_IZQ,
                                       DIR_ARR,DIR_ARR,DIR_ARR,DIR_ARR,DIR_ARR,DIR_ARR };
    int i, k;
    circ_init(c, "serie");
    for (i = 0; i < 30; i++) {
        if (i == 29) { k = circ_elemento(c, EL_FUENTE, X[i], Y[i], D[i]); c->el[k].val.v = E; c->el[k].val.est = M_DATO; }
        else if (i == 0) { k = circ_elemento(c, EL_RES, X[i], Y[i], D[i]); c->el[k].val.v = R; c->el[k].val.est = M_DATO; }
        else if (i <= n) { k = circ_elemento(c, EL_DIODO, X[i], Y[i], D[i]); c->el[k].variante = (unsigned char)var; }
        else circ_cable(c, X[i], Y[i], D[i]);
    }
    c->tierra_x = 0; c->tierra_y = 6;
}

/* Un cable recto de (x0, y0) a (x1, y1), tramo a tramo (el cab de biblioteca.c) */
static void r4_cab(Circuito *c, int x0, int y0, int x1, int y1)
{
    int d = (x1 > x0) ? DIR_DER : (x1 < x0) ? DIR_IZQ : (y1 > y0) ? DIR_ABA : DIR_ARR;
    while (x0 != x1 || y0 != y1) {
        int nx, ny;
        circ_cable(c, x0, y0, d);
        circ_vecino(x0, y0, d, &nx, &ny);
        x0 = nx; y0 = ny;
    }
}

/* El MOSFET del caso a) del §6.11 dibujado: E1 = VDD (12 V, columna 8), R1 =
 * RD (2 kΩ) al drenador, E2 = VGS (4 V, columna 1) a la puerta y el
 * surtidor a la tierra de la fila 5. M1 con la flecha a la derecha desde su
 * puerta en (4,3): D en (5,2) y S en (5,4). Con canal_p, el canal P con las
 * dos fuentes al revés (-12 V y -4 V) */
static int r4_mos(Circuito *c, int canal_p)
{
    int k, kM;
    circ_init(c, "MOSFET");
    k = circ_elemento(c, EL_FUENTE, 8, 4, DIR_ARR); c->el[k].val.v = canal_p ? -12.0 : 12.0; c->el[k].val.est = M_DATO;
    k = circ_elemento(c, EL_FUENTE, 1, 4, DIR_ARR); c->el[k].val.v = canal_p ? -4.0 : 4.0; c->el[k].val.est = M_DATO;
    k = circ_elemento(c, EL_RES, 6, 1, DIR_IZQ); c->el[k].val.v = 2e3; c->el[k].val.est = M_DATO;
    kM = circ_elemento(c, EL_MOS, 4, 3, DIR_DER);
    if (kM >= 0 && canal_p) c->el[kM].variante = MOS_CP;
    r4_cab(c, 5, 1, 5, 2);                               /* el drenador, a RD */
    r4_cab(c, 1, 3, 4, 3);                               /* la puerta, a E2 */
    r4_cab(c, 6, 1, 8, 1); r4_cab(c, 8, 1, 8, 3);       /* RD, a VDD */
    r4_cab(c, 5, 4, 5, 5);                               /* el surtidor, a la tierra */
    r4_cab(c, 1, 4, 1, 5); r4_cab(c, 1, 5, 8, 5); r4_cab(c, 8, 4, 8, 5);
    c->tierra_x = 3; c->tierra_y = 5;
    return kM;
}

/* Espera a que acabe el trabajo de P_EMULANDO */
static void r4_espera(void)
{
    int j;
    for (j = 0; j < 200000 && ui_pantalla() == P_EMULANDO; j++) ui_tick();
}

/* La pantalla a out/<nombre>.bmp, fuera de la numeración de vuelca() (así
 * no corre los números de las BMP de las pruebas siguientes) */
static void r4_vuelca(const char *nombre)
{
    char ruta[256];
    ui_dibuja();
    sprintf(ruta, "%s/%s.bmp", dir_out, nombre);
    gfx_dump_bmp(ruta);
}

/* El índice del elemento con esa etiqueta */
static int r4_el(const Circuito *c, const char *etiq)
{
    int k;
    for (k = 0; k < c->nel; k++) if (strcmp(c->el[k].etiq, etiq) == 0) return k;
    return -1;
}

static void test_e4_res(void)
{
    static Circuito c;
    static SimRed s;
    static Red r;
    static Valores v;
    SimDiag d;
    char t[128];
    int k, i;

    printf("[65b] Etapa 4: el punto de operaci" S_oo "n y el barrido en la interfaz\n");
    gfx_desbordes_reset();

    /* ---- [=] con E 9 V + R 330 Ω + LED rojo ---- */
    r4_led(&c, 9.0, 330.0);
    chk_cond("LED", "se abre", r3u_abre(&c));
    chk_cond("LED", "la l" S_ii "nea de estado", ve("Listo: [=] da el punto de operaci" S_oo "n"));
    TECLAS(T_IGUAL);
    chk_cond("LED", "P_RESULTADO del punto de operaci" S_oo "n", ui_pantalla() == P_RESULTADO && ui_res_nl() == 1);
    chk_cond("LED", "Newton en la l" S_ii "nea 4", ve("Punto de operaci" S_oo "n: Newton, "));
    chk_cond("LED", "109 %", ve("109 %"));
    k = r4_el(ui_circuito(), "D1");
    red_extraer(&c, &r);
    sim_red_de(&c, &r, &s, &d);
    chk_cond("LED", "los valores, bit a bit los de sim_op_cc_red",
             sim_op_cc_red(&s, &v, &d, 0) == SIM_OK && k >= 0 && ui_valores()->I[k] == v.I[k] && ui_valores()->V[k] == v.V[k]);
    r4_vuelca("e4_res_led");
    TECLAS(T_ENTER);
    chk_cond("LED", "[enter]: sin paso a paso", ve("Sin paso a paso: D1 no es lineal."));
    TECLAS(T_LETRA('T'));
    chk_cond("LED", "[T]: Th" S_ee "venin no", ve("Th" S_ee "venin es de redes lineales"));
    TECLAS(T_TAB);
    chk_cond("LED", "[tab]: la tabla con Kirchhoff y la regi" S_oo "n", ui_pantalla() == P_TABLA && ve("Kirchhoff") && ve("conduce") &&
             ve("Valor/reg."));                       /* «Valor/región» tocaba la V (rev8) */
    r4_vuelca("e4_res_led_tabla");
    TECLAS(T_ESC);
    chk_cond("LED", "[esc] vuelve al resultado", ui_pantalla() == P_RESULTADO);
    TECLAS(T_ESC);
    chk_cond("LED", "[esc] vuelve al editor", ui_pantalla() == P_EDITOR);

    /* ---- Los casos cerrados de [58] por [=]: bit a bit los de sim_op_cc_red ---- */
    {
        static const int VAR[3] = { DIO_EXP, DIO_1N4148, DIO_1N4007 };
        for (i = 0; i < 3; i++) {
            r4_serie(&c, 5.0, 1e3, 1, VAR[i]);
            r3u_abre(&c);
            TECLAS(T_IGUAL);
            red_extraer(&c, &r);
            sim_red_de(&c, &r, &s, &d);
            k = r4_el(&c, "D1");
            chk_cond("casos cerrados", "el diodo con 5 V y 1 k" S_OHM ": bit a bit",
                     ui_res_nl() && sim_op_cc_red(&s, &v, &d, 0) == SIM_OK && ui_valores()->I[k] == v.I[k]);
        }
    }

    /* ---- La línea 4 con la rampa: [P] y los 10 diodos en serie ---- */
    r4_serie(&c, 10.0, 10.0, 10, DIO_EXP);
    r3u_abre(&c);
    sim_nl_topes(5, 0);
    sim_nl_prueba(NLP_SIN_GMIN, 0.0);
    TECLAS(T_LETRA('P'));
    sim_nl_prueba(NLP_NADA, 0.0);
    sim_nl_topes(0, 0);
    chk_cond("l" S_ii "nea 4", "[P]: CC, C abiertos, L cables: Newton+rampa", ui_pantalla() == P_RESULTADO &&
             ve("CC, C abiertos, L cables: Newton+rampa, "));
    r4_vuelca("e4_res_rampa");
    TECLAS(T_ESC);

    /* ---- La línea de los LED en sus peores casos: 52 columnas ---- */
    {
        static const unsigned char COL[4] = { LED_AMARILLO, LED_NARANJA, LED_AMARILLO, LED_NARANJA };
        static const double I4[4] = { -1.234e-3, 19.87e-3, 25.43e-3, 1.234e-6 };
        int lr, n;
        circ_init(&c, "LED");
        for (i = 0; i < 4; i++) {
            Elemento *e = &c.el[c.nel++];
            memset(e, 0, sizeof *e);
            e->tipo = EL_LED; e->variante = COL[i]; e->ref = -1;
            sprintf(e->etiq, "D%d", 10 + i);
        }
        for (i = 0; i < 4; i++) v.I[i] = I4[i];
        lr = ui_led_linea(t, sizeof t, "", &c, v.I);
        n = (int)strlen(t);
        chk_cond("l" S_ii "nea de LED", "cuatro LED: « +N» y 52 columnas", lr == 1 && n <= 52 && strstr(t, " +") != 0);
        lr = ui_led_linea(t, sizeof t, "Media: ", &c, v.I);
        chk_cond("l" S_ii "nea de LED", "con «Media: »: 52 columnas", lr == 1 && (int)strlen(t) <= 52 && strncmp(t, "Media: ", 7) == 0);
        strcpy(c.el[2].etiq, "D32"); strcpy(c.el[1].etiq, "D31");
        v.I[2] = -1.2345e-3; v.I[1] = -9.876e-3;
        lr = ui_led_linea(t, sizeof t, "Media: ", &c, v.I);
        chk_cond("l" S_ii "nea de LED", "D32 y D31 con signo: 52 columnas", lr == 1 && (int)strlen(t) <= 52);
        v.I[3] = 145.2e-3;
        lr = ui_led_linea(t, sizeof t, "", &c, v.I);
        chk_cond("l" S_ii "nea de LED", "el que se quemar" S_ii "a, solo", lr == 2 && strstr(t, "D13 naranja: ") == t && strstr(t, "se quemar") &&
                 (int)strlen(t) <= 52);
    }

    /* ---- SOL_SIM: el culpable es del circuito del punto de operación ---- */
    {
        static Circuito otro;
        r4_led(&otro, 9.0, 330.0);                        /* se emula antes otro circuito */
        otro.el[0].variante = ONDA_SENO;
        otro.el[0].par[Q_FREC - Q_P1].v = 50.0; otro.el[0].par[Q_FREC - Q_P1].est = M_DATO;
        otro.el[0].par[Q_BASE - Q_P1].v = 0.0; otro.el[0].par[Q_BASE - Q_P1].est = M_DATO;
        r3u_abre(&otro);
        TECLAS(T_IGUAL);
        r4_espera();
        r4_serie(&c, 5.0, 1e3, 3, DIO_EXP);
        c.el[r4_el(&c, "D1")].variante = DIO_VG;
        r3u_abre(&c);
        sim_nl_topes(1, 0);
        sim_nl_prueba(NLP_SIN_HOMOTOPIA, 0.0);
        TECLAS(T_IGUAL);
        sim_nl_prueba(NLP_NADA, 0.0);
        sim_nl_topes(0, 0);
        chk_cond("SOL_SIM", "no converge: el texto nombra a un diodo de este circuito",
                 ui_pantalla() == P_RESULTADO && ui_res_nl() && (ve("D2") || ve("D3") || ve("D1")) && ve("[esc] volver y corregirlo"));
        r4_vuelca("e4_res_sol_sim");
        TECLAS(T_ESC);
    }

    /* ---- «¿Qué R da 20 mA?»: R1 con [?!] e I(D1) = 20 mA dato ---- */
    r4_led(&c, 9.0, 330.0);
    k = r4_el(&c, "R1");
    c.el[k].val.est = M_BUSCAR;
    i = r4_el(&c, "D1");
    c.el[i].I.v = 20e-3; c.el[i].I.est = M_DATO;
    r3u_abre(&c);
    chk_cond("20 mA", "la l" S_ii "nea de estado", ve("[I] barre"));
    TECLAS(T_IGUAL);
    chk_cond("20 mA", "[=]: barre con [I]", ui_pantalla() == P_RESULTADO && ve("barre con [I]") && ve("[I] barrer R1"));
    TECLAS(T_LETRA('I'));
    r4_espera();
    chk_cond("20 mA", "[I]: P_BARRIDO con el cursor en 360 " S_OHM, ui_pantalla() == P_BARRIDO && ve("360,0"));
    r4_vuelca("e4_res_barrido_led");
    TECLAS(T_TAB);
    chk_cond("20 mA", "[tab]: el circuito en el cursor", ui_pantalla() == P_RESULTADO && ve("En el cursor: R1 = 360,0"));
    r4_vuelca("e4_res_cursor");
    TECLAS(T_ESC);
    chk_cond("20 mA", "[esc] vuelve al barrido", ui_pantalla() == P_BARRIDO);
    TECLAS(T_LETRA('N'), T_DIG(2), T_DIG(0), T_LETRA('M'), T_ENTER);
    chk_cond("20 mA", "[N] 20m [enter]: el mismo cruce", ve("360,0"));
    TECLAS(T_ESC);

    /* ---- Los ajustes del barrido: qué se barre, el rango y sus avisos ---- */
    r4_led(&c, 9.0, 330.0);
    r3u_abre(&c);
    TECLAS(T_LETRA('I'));
    r4_espera();
    chk_cond("ajustes", "[I]: se barre E1 (la primera E continua)", ui_pantalla() == P_BARRIDO && ve("frente a E1"));
    TECLAS(T_MENU);
    chk_cond("ajustes", "[menu]: los ajustes del barrido", ui_pantalla() == P_AJUSTES && ve("Barre"));
    r4_vuelca("e4_res_ajustes");
    TECLAS(T_ABAJO, T_DIG(5), T_ABAJO, T_DIG(1), T_ENTER);
    chk_cond("ajustes", "Desde > Hasta: su aviso", ve("Desde tiene que ser menor que Hasta."));
    /* Desde y Hasta automáticos (E1 de -9 a 9 V) y la escala logarítmica: su aviso */
    TECLAS(T_ARRIBA, T_BORRAR, T_BORRAR, T_ABAJO, T_BORRAR, T_BORRAR, T_ABAJO, T_DER, T_ENTER);
    chk_cond("ajustes", "log con Desde <= 0: su aviso", ve("La escala logar" S_ii "tmica pide Desde > 0."));
    /* otra vez lineal; [der] en «Barre» pasa a R1 (de 3,3 a 33 k, logarítmico) */
    TECLAS(T_IZQ, T_ARRIBA, T_ARRIBA, T_ARRIBA, T_DER);
    chk_cond("ajustes", "[der]: R1 con su valor", ve("R1 (330"));
    TECLAS(T_IGUAL);
    r4_espera();
    chk_cond("ajustes", "[=]: frente a R1", ui_pantalla() == P_BARRIDO && ve("frente a R1"));
    TECLAS(T_ESC);

    /* ---- La barra del barrido y [esc] a medias (un barrido largo) ---- */
    r4_serie(&c, 10.0, 100.0, 8, DIO_1N4148);
    r3u_abre(&c);
    TECLAS(T_LETRA('I'));
    if (ui_pantalla() == P_EMULANDO) {
        chk_cond("barra", "Barriendo E1 de", ve("Barriendo E1 de"));
        r4_vuelca("e4_res_barriendo");
        ui_tick();
        TECLAS(T_ESC);
        chk_cond("barra", "[esc]: cortado, con los puntos hechos", ui_pantalla() == P_BARRIDO && ve("Cortado en E1 = "));
    } else chk_cond("barra", "el barrido largo pasa por P_EMULANDO", 0);
    TECLAS(T_ESC);

    /* ---- P_MODELO: la curva I-V del diodo ---- */
    r4_led(&c, 9.0, 330.0);
    r3u_abre(&c);
    {
        /* la ficha del LED: el cursor en su cátodo (2,2), donde solo está él */
        TECLAS(T_DER, T_DER, T_ABAJO, T_ABAJO, T_ENTER);
    }
    if (ui_pantalla() == P_ELEMENTO) {
        TECLAS(T_CATALOGO);
        chk_cond("modelo", "[cat" S_aa "logo]: la p" S_aa "gina Modelo con [I]", ui_pantalla() == P_MODELO && ve("[I] su curva I-V"));
        TECLAS(T_LETRA('I'));
        r4_espera();
        chk_cond("modelo", "[I]: la curva I-V del LED", ui_pantalla() == P_BARRIDO && ve("Curva I-V de D1"));
        r4_vuelca("e4_res_curva_led");
        TECLAS(T_ESC);
        chk_cond("modelo", "[esc] vuelve a la p" S_aa "gina Modelo", ui_pantalla() == P_MODELO);
    } else chk_cond("modelo", "la ficha del LED se abre", 0);

    /* ---- El emulador con no lineales: un LED con una senoidal ---- */
    r4_led(&c, 9.0, 330.0);
    k = r4_el(&c, "E1");
    c.el[k].variante = ONDA_SENO;
    c.el[k].par[Q_FREC - Q_P1].v = 50.0; c.el[k].par[Q_FREC - Q_P1].est = M_DATO;
    c.el[k].par[Q_BASE - Q_P1].v = 0.0; c.el[k].par[Q_BASE - Q_P1].est = M_DATO;
    r3u_abre(&c);
    TECLAS(T_IGUAL);
    r4_espera();
    if (ui_pantalla() == P_OSC) TECLAS(T_TAB);
    for (i = 0; i < 6 && ui_pantalla() != P_EMU_ESQ; i++) TECLAS(T_TAB);
    chk_cond("emulador", "P_EMU_ESQ con Newton y la media de los LED", ui_pantalla() == P_EMU_ESQ &&
             ve("iteraciones/paso") && ve("Media: D1 rojo"));
    r4_vuelca("e4_res_emu_esq");

    /* ---- Un elemento de cada tipo de la paleta (§8.1, riesgo 8): se dibuja,
     *      se guarda, se resuelve, se barre, se analiza en CA y se emula. En
     *      el paso 14, todos: también el BJT (el emisor común de la
     *      biblioteca) y el MOSFET (el caso a) del §6.11) ---- */
    {
        static const int CADA[8] = { EL_DIODO, EL_ZENER, EL_LED, EL_EDEP, EL_JDEP, EL_AO, EL_BJT, EL_MOS };
        static const char *const NOM[8] = { "diodo", "zener", "LED", "E controlada", "J controlada", "AO", "BJT", "MOSFET" };
        static Circuito d2;
        int j;
        for (j = 0; j < 8; j++) {
            const char *q = NOM[j];
            int kE;
            if (CADA[j] == EL_AO) e4_inversor(&c);
            else if (CADA[j] == EL_BJT) modelo_cargar(41, &c);
            else if (CADA[j] == EL_MOS) r4_mos(&c, 0);
            else if (CADA[j] == EL_EDEP || CADA[j] == EL_JDEP) {
                /* E1, R1 y R2 en un lazo; la controlada (por la V de R2) con R3 y R4 en otro */
                int kR2, kX;
                circ_init(&c, "controlada");
                kE = circ_elemento(&c, EL_FUENTE, 1, 2, DIR_ARR); c.el[kE].val.v = 1.0; c.el[kE].val.est = M_DATO;
                k = circ_elemento(&c, EL_RES, 1, 1, DIR_DER); c.el[k].val.v = 1e3; c.el[k].val.est = M_DATO;
                kR2 = circ_elemento(&c, EL_RES, 2, 1, DIR_ABA); c.el[kR2].val.v = 1e3; c.el[kR2].val.est = M_DATO;
                circ_cable(&c, 2, 2, DIR_IZQ); circ_cable(&c, 2, 2, DIR_DER); circ_cable(&c, 3, 2, DIR_DER);
                kX = circ_elemento(&c, (TipoEl)CADA[j], 4, 2, DIR_ARR);
                c.el[kX].variante = DEP_V; c.el[kX].ref = (signed char)kR2;
                c.el[kX].val.v = CADA[j] == EL_EDEP ? 2.0 : 2e-3; c.el[kX].val.est = M_DATO;
                k = circ_elemento(&c, EL_RES, 4, 1, DIR_DER); c.el[k].val.v = 1e3; c.el[k].val.est = M_DATO;
                k = circ_elemento(&c, EL_RES, 5, 1, DIR_ABA); c.el[k].val.v = 1e3; c.el[k].val.est = M_DATO;
                circ_cable(&c, 5, 2, DIR_IZQ);
                c.tierra_x = 1; c.tierra_y = 2;
            } else {
                r4_led(&c, 9.0, 330.0);
                k = r4_el(&c, "D1");
                c.el[k].tipo = (unsigned char)CADA[j];
                c.el[k].variante = 0;
                if (CADA[j] == EL_ZENER) { c.el[k].par[QZ_VZ - Q_P1].v = 5.1; c.el[k].par[QZ_VZ - Q_P1].est = M_DATO; }
            }
            chk_cond(q, "v" S_aa "lido", circ_valido(&c));
            chk_cond(q, "se guarda y se abre igual", arch_guardar(0, &c) && arch_cargar(0, &d2) && mismos(&c, &d2));
            chk_cond(q, "se dibuja (en el editor)", r3u_abre(&c));
            TECLAS(T_IGUAL);
            r4_espera();
            chk_cond(q, "[=] lo resuelve", ui_pantalla() == P_RESULTADO && !ve("corregirlo"));
            TECLAS(T_ESC, T_LETRA('I'));
            r4_espera();
            chk_cond(q, "[I] lo barre", ui_pantalla() == P_BARRIDO && barr_res()->fin == FIN_COMPLETO && barr_res()->huecos == 0);
            TECLAS(T_ESC, T_LETRA('B'));
            r4_espera();
            chk_cond(q, "[B] el Bode (de peque" S_nn "a se" S_nn "al)", ui_pantalla() == P_BODE && bode_res()->fin == FIN_COMPLETO);
            TECLAS(T_ESC);
            /* con una senoidal: se emula */
            kE = r4_el(&c, "E1");
            c.el[kE].variante = ONDA_SENO;
            c.el[kE].par[Q_FREC - Q_P1].v = 50.0; c.el[kE].par[Q_FREC - Q_P1].est = M_DATO;
            c.el[kE].par[Q_BASE - Q_P1].v = 0.0; c.el[kE].par[Q_BASE - Q_P1].est = M_DATO;
            r3u_abre(&c);
            TECLAS(T_IGUAL);
            r4_espera();
            chk_cond(q, "con una senoidal, [=] lo emula", sim_res()->fin == FIN_COMPLETO && sim_res()->d.estado == SIM_OK &&
                     (ui_pantalla() == P_OSC || ui_pantalla() == P_EMU_ESQ));
        }
        limpia_mis();
    }

    /* ---- [66] por teclas: las lecciones 12 y 13 desde Aprende; los de ca 3
     *      van a P_RESULTADO y los de ca 4 a P_BARRIDO con el cursor en la
     *      respuesta (paso 12) ---- */
    {
        int l12;
        for (l12 = 0; l12 < N_LECCIONES && strncmp(LECCIONES[l12].titulo, "12.", 3) != 0; l12++) ;
        chk_cond("[66] por teclas", "la lecci" S_oo "n 12 existe", l12 < N_LECCIONES);
        ui_init();
        TECLAS(T_ENTER, T_ABAJO, T_ENTER);                   /* portada, Aprende */
        repite(T_ABAJO, l12);
        TECLAS(T_ENTER, T_DER, T_DER);                       /* la teoría, a los modelos */
        chk_cond("[66] por teclas", "los modelos de la lecci" S_oo "n 12", ui_pantalla() == P_EJEMPLOS && ve("[enter] calcular"));
        TECLAS(T_ENTER);
        chk_cond("[66] por teclas", "el 23 ([enter]): P_RESULTADO, no P_CA", ui_pantalla() == P_RESULTADO && ui_res_nl() &&
                 ve("Punto de operaci" S_oo "n: Newton"));
        TECLAS(T_ESC, T_ABAJO);
        chk_cond("[66] por teclas", "el 24: [enter] barrer", ve("[enter] barrer"));
        TECLAS(T_ENTER);
        r4_espera();
        chk_cond("[66] por teclas", "el 24: P_BARRIDO con el cursor en 1 mA", ui_pantalla() == P_BARRIDO && ve("1,585"));
        r4_vuelca("e4_res_modelo24");
        TECLAS(T_ESC);
        repite(T_ABAJO, 8);
        TECLAS(T_ENTER);
        r4_espera();
        chk_cond("[66] por teclas", "el 32: P_BARRIDO con el cursor en 360 " S_OHM, ui_pantalla() == P_BARRIDO && ve("360,0"));
        r4_vuelca("e4_res_modelo32");
        TECLAS(T_ESC, T_ESC, T_ESC, T_ABAJO, T_ENTER, T_DER, T_DER);
        chk_cond("[66] por teclas", "la lecci" S_oo "n 13", ui_pantalla() == P_EJEMPLOS);
    }

    chk_cond("interfaz", "ning" S_uu "n texto fuera de la pantalla", gfx_desbordes() == 0);
    (void)t;
}
