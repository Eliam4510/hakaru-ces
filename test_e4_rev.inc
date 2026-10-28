/* --------------------------------------------------------------------------
 *  65i. Etapa 4: los hallazgos confirmados de la revisión adversarial
 *
 *  Una prueba por hallazgo (cada una falla sin su arreglo, comprobado por
 *  mutación): los ajustes del barrido de otro circuito, el bucle de [esc]
 *  entre P_RESULTADO y P_BARRIDO, «[I] barrer R1» con los ajustes tocados, el
 *  error del trazador con el nombre de otro elemento, la unidad de la
 *  ganancia de una controlada, el punto de operación de una red lineal, el
 *  aviso SATURA y el residuo en el cursor, y la línea «se quemaría» con
 *  «Media: ». Se incluye tras test_e4_res.inc (usa r4_led, r4_serie,
 *  r4_espera, r4_el, r3u_abre y e4_inversor).
 * ----------------------------------------------------------------------- */

/* Al menú principal con [esc] */
static void rv_al_menu(void)
{
    int j;
    for (j = 0; j < 10 && ui_pantalla() != P_MENU; j++) TECLAS(T_ESC);
}

/* Del menú principal a los modelos de la lección que empieza por «pre» */
static int rv_modelos(const char *pre)
{
    int l;
    for (l = 0; l < N_LECCIONES && strncmp(LECCIONES[l].titulo, pre, strlen(pre)) != 0; l++) ;
    if (l >= N_LECCIONES) return 0;
    TECLAS(T_ARRIBA, T_ARRIBA, T_ARRIBA, T_ABAJO, T_ENTER);    /* Aprende */
    repite(T_ARRIBA, N_LECCIONES);
    repite(T_ABAJO, l);
    TECLAS(T_ENTER, T_DER, T_DER);                           /* la teoría, a los modelos */
    return ui_pantalla() == P_EJEMPLOS;
}

/* E1, R1 y R2 en un lazo; la controlada tipo (variante var, ganancia g) por
 * la V o la I de R2, con R3 y R4 en otro lazo */
static void rv_ctl(Circuito *c, int tipo, int var, double g)
{
    int k, kR2, kX;
    circ_init(c, "controlada");
    k = circ_elemento(c, EL_FUENTE, 1, 2, DIR_ARR); c->el[k].val.v = 1.0; c->el[k].val.est = M_DATO;
    k = circ_elemento(c, EL_RES, 1, 1, DIR_DER); c->el[k].val.v = 1e3; c->el[k].val.est = M_DATO;
    kR2 = circ_elemento(c, EL_RES, 2, 1, DIR_ABA); c->el[kR2].val.v = 1e3; c->el[kR2].val.est = M_DATO;
    circ_cable(c, 2, 2, DIR_IZQ); circ_cable(c, 2, 2, DIR_DER); circ_cable(c, 3, 2, DIR_DER);
    kX = circ_elemento(c, tipo, 4, 2, DIR_ARR);
    c->el[kX].variante = (unsigned char)var; c->el[kX].ref = (signed char)kR2;
    c->el[kX].val.v = g; c->el[kX].val.est = M_DATO;
    k = circ_elemento(c, EL_RES, 4, 1, DIR_DER); c->el[k].val.v = 1e3; c->el[k].val.est = M_DATO;
    k = circ_elemento(c, EL_RES, 5, 1, DIR_ABA); c->el[k].val.v = 1e3; c->el[k].val.est = M_DATO;
    circ_cable(c, 5, 2, DIR_IZQ);
    c->tierra_x = 1; c->tierra_y = 2;
}

/* Con una E1 senoidal de 50 Hz (sin nivel de CC) */
static void rv_seno(Circuito *c)
{
    int k = r4_el(c, "E1");
    c->el[k].variante = ONDA_SENO;
    c->el[k].par[Q_FREC - Q_P1].v = 50.0; c->el[k].par[Q_FREC - Q_P1].est = M_DATO;
    c->el[k].par[Q_BASE - Q_P1].v = 0.0; c->el[k].par[Q_BASE - Q_P1].est = M_DATO;
}

/* ¿El barrido que se ve barre «nom» de x0 a x1 (con esa escala)? */
static int rv_barre(const char *nom, double x0, double x1, int lg)
{
    const BarrRes *r = barr_res();
    return ui_pantalla() == P_BARRIDO && r->fin != FIN_ERROR && strcmp(r->nom_x, nom) == 0 &&
           r->x0 == x0 && r->x1 == x1 && r->log_x == lg;
}

static void test_e4_rev(void)
{
    static Circuito c;
    char t[128];
    int k, kD, i;
    double i0;

    printf("[65i] Etapa 4: los hallazgos de la revisi" S_oo "n adversarial\n");
    gfx_desbordes_reset();

    /* ---- [esc] entre P_RESULTADO y P_BARRIDO tras [I] y [tab]: se vuelve a
     *      la P_RESULTADO de partida y luego a su origen ---- */
    r4_led(&c, 9.0, 330.0);
    k = r4_el(&c, "R1");
    c.el[k].val.est = M_BUSCAR;
    kD = r4_el(&c, "D1");
    c.el[kD].I.v = 20e-3; c.el[kD].I.est = M_DATO;
    r3u_abre(&c);
    TECLAS(T_IGUAL);
    chk_cond("[esc] del cursor", "[=]: «[I] barrer R1»", ui_pantalla() == P_RESULTADO && ve("[I] barrer R1"));
    TECLAS(T_LETRA('I'));
    r4_espera();
    TECLAS(T_TAB);
    chk_cond("[esc] del cursor", "[tab]: el cursor, con sus valores (I(D1) = 20 mA)", ui_pantalla() == P_RESULTADO &&
             ve("En el cursor: R1 = 360,0") && fabs(ui_valores()->I[kD] - 20e-3) < 1e-6);
    TECLAS(T_LETRA('I'));
    chk_cond("[esc] del cursor", "[I] en el cursor: su barrido", ui_pantalla() == P_BARRIDO);
    TECLAS(T_TAB, T_ESC, T_TAB, T_ESC);
    chk_cond("[esc] del cursor", "dos [tab] y [esc]: P_BARRIDO", ui_pantalla() == P_BARRIDO);
    TECLAS(T_ESC);
    chk_cond("[esc] del cursor", "[esc]: la P_RESULTADO de partida", ui_pantalla() == P_RESULTADO &&
             ve("[I] barrer R1") && !ve("En el cursor"));
    TECLAS(T_ESC);
    chk_cond("[esc] del cursor", "[esc]: el editor", ui_pantalla() == P_EDITOR);

    /* el punto de operación: sus valores, bit a bit, y los ajustes por medio */
    r4_led(&c, 9.0, 330.0);
    r3u_abre(&c);
    TECLAS(T_IGUAL);
    kD = r4_el(ui_circuito(), "D1");
    i0 = ui_valores()->I[kD];
    TECLAS(T_LETRA('I'));
    r4_espera();
    TECLAS(T_TAB, T_ESC, T_MENU, T_IGUAL);               /* el cursor; los ajustes; barre otra vez */
    r4_espera();
    TECLAS(T_ESC);
    chk_cond("[esc] del cursor", "el punto de operaci" S_oo "n de partida, bit a bit", ui_pantalla() == P_RESULTADO &&
             ui_res_nl() && ve("109 %") && !ve("En el cursor") && ui_valores()->I[kD] == i0);
    TECLAS(T_ESC);
    chk_cond("[esc] del cursor", "y el editor", ui_pantalla() == P_EDITOR);

    /* ---- Los ajustes del barrido son de un circuito: ni un modelo ni el
     *      editor heredan los de otro ---- */
    r4_led(&c, 9.0, 330.0);                               /* R1 es el 1, como en el modelo 32 */
    r3u_abre(&c);
    rv_al_menu();
    chk_cond("ajustes de otro", "la lecci" S_oo "n 12", rv_modelos("12."));
    TECLAS(T_ENTER, T_LETRA('I'));                        /* el 23: [I] desde su punto de operación */
    r4_espera();
    chk_cond("ajustes de otro", "el 23: E1 de -5 a 5", rv_barre("E1", -5.0, 5.0, 0));
    TECLAS(T_ESC, T_ESC);
    repite(T_ABAJO, 9);
    TECLAS(T_ENTER);                                      /* el 32: R1 de 10 a 10 k, logarítmico */
    r4_espera();
    chk_cond("ajustes de otro", "el 32: R1 de 10 a 10 k", rv_barre("R1", 10.0, 1e4, 1));
    TECLAS(T_ESC);
    repite(T_ARRIBA, 9);
    TECLAS(T_ENTER, T_LETRA('I'));
    r4_espera();
    chk_cond("ajustes de otro", "el 23 tras el 32: E1 de -5 a 5", rv_barre("E1", -5.0, 5.0, 0));
    TECLAS(T_ESC, T_ESC);
    repite(T_ABAJO, 9);
    TECLAS(T_ENTER);
    r4_espera();
    rv_al_menu();
    TECLAS(T_ARRIBA, T_ARRIBA, T_ARRIBA, T_ENTER);        /* Taller */
    TECLAS(T_ARRIBA, T_ARRIBA, T_ARRIBA, T_ABAJO, T_ENTER);   /* seguir con el problema actual */
    chk_cond("ajustes de otro", "Taller > seguir: el LED", ui_pantalla() == P_EDITOR && ui_circuito()->nel == 3);
    TECLAS(T_LETRA('I'));
    r4_espera();
    chk_cond("ajustes de otro", "el editor tras el 32: E1 de -9 a 9", rv_barre("E1", -9.0, 9.0, 0));
    /* y al revés: R1 en los ajustes del editor no llega al modelo 23 */
    TECLAS(T_MENU, T_DER, T_ENTER);
    chk_cond("ajustes de otro", "los ajustes: R1", ui_pantalla() == P_BARRIDO);
    TECLAS(T_ESC);
    rv_al_menu();
    rv_modelos("12.");
    TECLAS(T_ENTER, T_LETRA('I'));
    r4_espera();
    chk_cond("ajustes de otro", "el 23 tras los ajustes del editor: E1 de -5 a 5", rv_barre("E1", -5.0, 5.0, 0));

    /* ---- «[I] barrer R1» barre R1 aunque los ajustes digan E1 ---- */
    r4_led(&c, 9.0, 330.0);
    kD = r4_el(&c, "D1");
    c.el[kD].I.v = 20e-3; c.el[kD].I.est = M_DATO;
    r3u_abre(&c);
    TECLAS(T_LETRA('I'));
    r4_espera();
    TECLAS(T_MENU, T_ENTER, T_ESC);                       /* [enter] en los ajustes: E1 fijado */
    chk_cond("barrer R1", "de vuelta al editor", ui_pantalla() == P_EDITOR);
    TECLAS(T_DER, T_ABAJO, T_ENTER, T_DER, T_INCOG, T_ENTER);   /* la ficha de R1: [?!] */
    k = r4_el(ui_circuito(), "R1");
    chk_cond("barrer R1", "R1 con [?!]", k >= 0 && ui_circuito()->el[k].val.est == M_BUSCAR);
    TECLAS(T_IGUAL);
    chk_cond("barrer R1", "[=]: «[I] barrer R1»", ui_pantalla() == P_RESULTADO && ve("[I] barrer R1"));
    TECLAS(T_LETRA('I'));
    r4_espera();
    chk_cond("barrer R1", "[I]: barre R1 y el cursor en 360", ui_pantalla() == P_BARRIDO && barr_res()->fin != FIN_ERROR &&
             strcmp(barr_res()->nom_x, "R1") == 0 && ve("360,0"));

    /* ---- El error del trazador nombra al componente, con su texto ---- */
    for (i = 0; i < 2; i++) {
        r4_led(&c, 9.0, 330.0);
        kD = r4_el(&c, "D1");
        if (i == 0) {                                     /* un zener con rz·Iz > Vz */
            c.el[kD].tipo = EL_ZENER; c.el[kD].variante = ZEN_LIBRO;
            c.el[kD].par[QZ_VZ - Q_P1].v = 5.1; c.el[kD].par[QZ_VZ - Q_P1].est = M_DATO;
            c.el[kD].par[QZ_RZ - Q_P1].v = 2e3; c.el[kD].par[QZ_RZ - Q_P1].est = M_DATO;
        } else {                                          /* un LED con Rs·If > Vf */
            c.el[kD].par[QL_IF - Q_P1].v = 1.0; c.el[kD].par[QL_IF - Q_P1].est = M_DATO;
        }
        chk_cond("trazador", "barr_iniciar_curva: el error es del componente",
                 barr_iniciar_curva(&c, kD) != SIM_OK && barr_res()->d.elem == kD);
        r3u_abre(&c);
        TECLAS(T_DER, T_DER, T_ABAJO, T_ABAJO, T_ENTER, T_CATALOGO, T_LETRA('I'));
        r4_espera();
        chk_cond("trazador", i == 0 ? "el zener: «D1: rz·Iz ...»" : "el LED: «D1: con If ...»", ui_pantalla() == P_BARRIDO &&
                 ve(i == 0 ? "D1: rz" : "D1: con If") && !ve("E1: ") && (i == 0 || !ve("rz" S_CDOT "Iz")));
    }

    /* ---- La unidad de la ganancia: β sin unidad y r en Ω (P_TABLA y P_EMU_TABLA) ---- */
    for (i = 0; i < 2; i++) {
        int j;
        rv_ctl(&c, i == 0 ? EL_JDEP : EL_EDEP, DEP_I, i == 0 ? 50.0 : 2000.0);
        r3u_abre(&c);
        TECLAS(T_IGUAL, T_TAB);
        chk_cond("unidad de la ganancia", i == 0 ? "P_TABLA: beta = 50, sin S" : "P_TABLA: r = 2000 " S_OHM,
                 ui_pantalla() == P_TABLA && (i == 0 ? ve("50 \n") && !ve("50 S") : ve("2000 " S_OHM)));
        rv_seno(&c);
        r3u_abre(&c);
        TECLAS(T_IGUAL);
        r4_espera();
        for (j = 0; j < 6 && ui_pantalla() != P_EMU_TABLA; j++) TECLAS(T_TAB);
        chk_cond("unidad de la ganancia", i == 0 ? "P_EMU_TABLA: beta = 50, sin S" : "P_EMU_TABLA: r = 2 k" S_OHM,
                 ui_pantalla() == P_EMU_TABLA && (i == 0 ? !ve("50S") : ve("2k" S_OHM)));
    }

    /* ---- El punto de operación de una red lineal: textos ciertos ---- */
    r4_led(&c, 9.0, 330.0);
    kD = r4_el(&c, "D1");
    c.el[kD].tipo = EL_RES; c.el[kD].variante = 0;
    c.el[kD].val.v = 1e3; c.el[kD].val.est = M_DATO;
    c.term_x[0] = 2; c.term_y[0] = 1; c.term_x[1] = 2; c.term_y[1] = 2;
    r3u_abre(&c);
    TECLAS(T_LETRA('P'));
    chk_cond("red lineal", "[P]: «red lineal, sin Newton» y «[T] Th" S_ee "venin»", ui_pantalla() == P_RESULTADO && ui_res_nl() &&
             ve("red lineal, sin Newton") && !ve("por tramos") && ve("[T] Th" S_ee "venin"));
    TECLAS(T_ENTER);
    chk_cond("red lineal", "[enter]: el paso a paso es de [=]", ve("[=] en el editor lo da") && !ve("no es lineal"));
    TECLAS(T_LETRA('T'));
    chk_cond("red lineal", "[T]: Th" S_ee "venin", ui_pantalla() == P_THEVENIN);
    TECLAS(T_ESC, T_TAB);
    chk_cond("red lineal", "[tab]: «Red lineal, sin Newton»", ui_pantalla() == P_TABLA && ve("Red lineal, sin Newton") &&
             !ve("Newton: 0"));
    rv_al_menu();
    chk_cond("red lineal", "la lecci" S_oo "n 13", rv_modelos("13."));
    repite(T_ABAJO, 7);
    TECLAS(T_ENTER);                                      /* el 40: con una E controlada */
    chk_cond("red lineal", "el modelo 40: «red lineal»", ui_pantalla() == P_RESULTADO && ui_res_nl() &&
             ve("red lineal, sin Newton"));
    TECLAS(T_ENTER);
    chk_cond("red lineal", "el modelo 40, [enter]: con controladas no hay paso a paso", ve("no lo hay con fuentes controladas"));
    TECLAS(T_LETRA('T'));
    chk_cond("red lineal", "el modelo 40, [T]: ni «?» ni «no lo es»", !ve(" no lo es.") && !ve("? no"));

    /* ---- El cursor del barrido: el aviso SATURA y el residuo de sus valores ---- */
    e4_inversor(&c);
    c.el[0].val.v = 2.0;
    r3u_abre(&c);
    TECLAS(T_LETRA('P'));
    chk_cond("cursor", "[P]: U1 satura", ve("U1 satura"));
    TECLAS(T_ESC, T_LETRA('I'));
    r4_espera();
    repite(T_DER, 300);
    TECLAS(T_TAB);
    chk_cond("cursor", "[tab] en E1 = 2 V: U1 satura, como [P]", ui_pantalla() == P_RESULTADO && ve("En el cursor") &&
             ve("U1 satura"));
    r4_serie(&c, 10.0, 100.0, 8, DIO_1N4148);
    r3u_abre(&c);
    TECLAS(T_LETRA('I'));
    ui_tick();
    TECLAS(T_ESC, T_TAB, T_TAB);                          /* cortado; el cursor; su tabla */
    chk_cond("cursor", "la tabla del cursor: el residuo de sus valores, no 0", ui_pantalla() == P_TABLA &&
             barr_res()->fin == FIN_CORTADO && barr_residuo() > 0.0 && barr_residuo() <= 1e-9 && !ve("Kirchhoff 0 "));

    /* ---- «se quemaría» con «Media: »: 52 columnas ---- */
    {
        int lr;
        static Valores v;
        circ_init(&c, "LED");
        c.nel = 1;
        memset(&c.el[0], 0, sizeof c.el[0]);
        c.el[0].tipo = EL_LED; c.el[0].variante = LED_AMARILLO; c.el[0].ref = -1;
        strcpy(c.el[0].etiq, "D1");
        c.el[0].par[QL_IM - Q_P1].v = 25.5e-3; c.el[0].par[QL_IM - Q_P1].est = M_DATO;
        v.I[0] = 154.7e-3;
        lr = ui_led_linea(t, sizeof t, "Media: ", &c, v.I);
        chk_cond("se quemar" S_ii "a", "Im de 3 cifras con «Media: »: 52 columnas", lr == 2 && (int)strlen(t) <= 52 &&
                 strncmp(t, "Media: D1", 9) == 0 && strstr(t, "se quemar"));
        c.el[0].par[QL_IM - Q_P1].est = M_LIBRE;
        strcpy(c.el[0].etiq, "D13");
        lr = ui_led_linea(t, sizeof t, "Media: ", &c, v.I);
        chk_cond("se quemar" S_ii "a", "D13 con «Media: »: 52 columnas", lr == 2 && (int)strlen(t) <= 52 && strstr(t, "D13"));
        lr = ui_led_linea(t, sizeof t, "", &c, v.I);
        chk_cond("se quemar" S_ii "a", "sin prefijo: con el color y el m" S_aa "ximo", lr == 2 && strstr(t, "amarillo") &&
                 strstr(t, "(m" S_aa "x. "));
    }

    /* ---- El oráculo [67]: un LED azul o blanco (Is ~ 1e-18, n = 3) con un
     *      escalón de 0 a 5 V; tras el flanco Newton no convergía en 12
     *      iteraciones y partir el paso no ayudaba («En t = 1ms Newton no
     *      converge en D1») ---- */
    for (i = 0; i < 2; i++) {
        static const double VD[2] = { 2.906095625, -1.0 };
        r4_led(&c, 5.0, 330.0);
        k = r4_el(&c, "E1");
        kD = r4_el(&c, "D1");
        if (k < 0 || kD < 0) { chk_cond("flanco", "el circuito", 0); continue; }
        c.el[k].variante = ONDA_PULSO;
        c.el[k].par[Q_BASE - Q_P1].v = 0.0; c.el[k].par[Q_BASE - Q_P1].est = M_DATO;
        c.el[k].par[Q_T0 - Q_P1].v = 1e-3; c.el[k].par[Q_T0 - Q_P1].est = M_DATO;
        c.el[kD].variante = i == 0 ? LED_AZUL : LED_BLANCO;
        r3u_abre(&c);
        TECLAS(T_IGUAL);
        r4_espera();
        chk_cond("flanco", i == 0 ? "el LED azul con un escal" S_oo "n: se emula" : "el LED blanco con un escal" S_oo "n: se emula",
                 sim_res()->fin == FIN_COMPLETO && sim_res()->d.estado == SIM_OK &&
                 (VD[i] < 0.0 || fabs(sim_res()->ultimo.V[kD] - VD[i]) < 1e-6));
    }

    chk_cond("interfaz", "ning" S_uu "n texto fuera de la pantalla", gfx_desbordes() == 0);
}
