/* ============================================================================
 *  test_m_flujo.inc  -  [72] Las tres mejoras del flujo que esperaban a la
 *  capa de entrada (rev2/mejoras.json, lente «flujo»). HAKARU_SOLO_FLUJO la
 *  corre sola; va también en HAKARU_SOLO_E4 y al final de la batería.
 *
 *    F1  num_eval (numfmt.c): un número solo da lo mismo que num_lee_ing; la
 *        precedencia (^ a la derecha, -2^2 = -4), los prefijos dentro de la
 *        cuenta y cada error con su código; lo que no cabe en un double
 *        (solo o a medias) y 300 000 cuentas al azar sin infinitos
 *    F2  cuentas en la ficha: «220^2/100» y «1/(1/4k+1/6k)» con el eco, los
 *        signos solo donde tienen sentido, la cuenta rechazada se queda
 *        escrita (el 2.º [esc] la tira), el negativo donde no va signo, x²,
 *        la caja de 31 que enseña el final; en CA, el módulo sí y la Z (su
 *        [+] es +j) no; «-5» y «2E-3» como siempre
 *    F3  copiar, cortar y pegar: los valores sin V, I ni [?!]; cortar es un
 *        nivel de deshacer; el control de una controlada por su etiqueta; el
 *        espejo de un 3 patas; la f de una fuente alterna; el portapapeles
 *        pasa a otro problema; [esc] y las otras teclas dejan de pegar
 *    F4  nombres: Mis problemas > [N] y [ctrl][N] en el editor (le pone
 *        nombre y lo guarda), con [shift], el espacio, [del] y [ctrl][del];
 *        el archivo, el editor y sus niveles de deshacer toman el nombre; un
 *        nombre escrito como los del programa se respeta (y su copia)
 *    F5  por la capa: la tabla física de las teclas nuevas, × que no se
 *        repite, [del] mantenido en un nombre que sí (y para en el vacío)
 *    F6  independencia del dibujo con las teclas nuevas (la ficha con una
 *        cuenta, un nombre a medias, el editor pegando)
 *    F7  los textos nuevos caben en 52 columnas
 *
 *  Se incluye tras test_entrada.inc (usa ce_*, cm_*, r2e_nuevo, r3u_abre,
 *  u4_dato, u4_controladas, lazo, limpia_mis, r4_el y r4_vuelca). Sus
 *  pantallas van a out/f_*.bmp, fuera de la numeración de vuelca().
 * ========================================================================= */

/* El cursor del editor a (x, y) con flechas (sin acción: solo se mueve) */
static void fl_cursor(int x, int y)
{
    int i;
    for (i = 0; i < 20 && ui_indice(UI_I_CUR_X) < x; i++) TECLAS(T_DER);
    for (i = 0; i < 20 && ui_indice(UI_I_CUR_X) > x; i++) TECLAS(T_IZQ);
    for (i = 0; i < 20 && ui_indice(UI_I_CUR_Y) < y; i++) TECLAS(T_ABAJO);
    for (i = 0; i < 20 && ui_indice(UI_I_CUR_Y) > y; i++) TECLAS(T_ARRIBA);
}

/* ---- F1. num_eval ---- */
static void fl_eval_da(const char *q, const char *s, double v)
{
    char det[96];
    int err;
    double r = num_eval(s, &err);
    sprintf(det, "«%s» = %.10g", s, v);
    chk_cond(q, det, err == NE_OK && cerca(r, v, 1e-12));
}

static void fl_eval_mal(const char *q, const char *s, int e)
{
    char det[96];
    int err;
    double r = num_eval(s, &err);
    sprintf(det, "«%s»: el error %d", s, e);
    chk_cond(q, det, err == e && r == 0.0);
}

static void fl_eval(void)
{
    static const char *const SOLOS[] = {
        "4,7k", "2E-3", "-5", "0,025", "1E-14", "4.7", "15", "1,5M", "3,3" "\xB5", "100m", "2,52n",
        "-0,5", "1E300", "7p", "9G", "+3", "0"
    };
    static const char *const MALOS[] = { "4,5E", ",", "-", "", "4kk", "E3", "k", "1,2E-" };
    const char *q = "F1 num_eval";
    char s[128];
    int i, err, ok;
    double a, b;

    for (i = 0; i < (int)(sizeof SOLOS / sizeof SOLOS[0]); i++) {
        a = num_lee_ing(SOLOS[i], &ok);
        b = num_eval(SOLOS[i], &err);
        sprintf(s, "un n" S_uu "mero solo, el mismo double que num_lee_ing: «%s»", SOLOS[i]);
        chk_cond(q, s, ok && err == NE_OK && memcmp(&a, &b, sizeof a) == 0 && !num_es_cuenta(SOLOS[i]));
    }
    for (i = 0; i < (int)(sizeof MALOS / sizeof MALOS[0]); i++) {
        num_lee_ing(MALOS[i], &ok);
        num_eval(MALOS[i], &err);
        sprintf(s, "mal escrito, NE_NUMERO como num_lee_ing: «%s»", MALOS[i]);
        chk_cond(q, s, !ok && err == NE_NUMERO);
    }
    /* las del enunciado y la precedencia */
    fl_eval_da(q, "220^2/100", 484.0);
    fl_eval_da(q, "220^2" S_DIVIDE "100", 484.0);
    fl_eval_da(q, "1/(1/4k+1/6k)", 2400.0);
    fl_eval_da(q, "4,7k+1k", 5700.0);
    fl_eval_da(q, "2+3" S_TIMES "4", 14.0);
    fl_eval_da(q, "(2+3)*4", 20.0);
    fl_eval_da(q, "2^3^2", 512.0);                   /* a la derecha: 2^9 */
    fl_eval_da(q, "-2^2", -4.0);                     /* el - de delante, después */
    fl_eval_da(q, "(-2)^2", 4.0);
    fl_eval_da(q, "2^-1", 0.5);
    fl_eval_da(q, "10-4-3", 3.0);                    /* a la izquierda */
    fl_eval_da(q, "100" S_DIVIDE "10" S_DIVIDE "5", 2.0);
    fl_eval_da(q, "2" S_TIMES "-3", -6.0);
    fl_eval_da(q, "1E3" S_TIMES "2", 2000.0);
    fl_eval_da(q, "2E-3" S_TIMES "1000", 2.0);       /* el - del exponente es del número */
    fl_eval_da(q, "5-2E-3", 4.998);
    fl_eval_da(q, "(1,5)", 1.5);
    fl_eval_da(q, "((((2))))", 2.0);
    fl_eval_da(q, "3^2", 9.0);
    fl_eval_da(q, "1,2k" S_DIVIDE "(1+2)", 400.0);
    fl_eval_da(q, "10m" S_TIMES "10m", 1e-4);
    fl_eval_da(q, "--3", 3.0);
    b = num_eval("0" S_TIMES "-1", &err);
    chk_cond(q, "0" S_TIMES "-1 da 0 (no «-0»)", err == NE_OK && b == 0.0 && 1.0 / b > 0.0);
    /* los errores */
    fl_eval_mal(q, "(1+2", NE_PAR_ABRE);
    fl_eval_mal(q, "((1)", NE_PAR_ABRE);
    fl_eval_mal(q, "1+2)", NE_PAR_CIERRA);
    fl_eval_mal(q, "2+", NE_FALTA);
    fl_eval_mal(q, "()", NE_FALTA);
    fl_eval_mal(q, "2^", NE_FALTA);
    fl_eval_mal(q, S_TIMES "3", NE_FALTA);
    fl_eval_mal(q, "(1)(2)", NE_SIGNO);
    fl_eval_mal(q, "2(3)", NE_SIGNO);
    fl_eval_mal(q, "(2(3))", NE_SIGNO);
    fl_eval_mal(q, "(1)2", NE_SIGNO);
    fl_eval_mal(q, "1/0", NE_CERO);
    fl_eval_mal(q, "1" S_DIVIDE "(2-2)", NE_CERO);
    fl_eval_mal(q, "0^-1", NE_CERO);
    fl_eval_mal(q, "(0-8)^0,5", NE_REAL);
    fl_eval_mal(q, "1E300" S_TIMES "1E300", NE_GRANDE);
    /* lo que no cabe en un double: el número solo, y un infinito a medias
     * que la cuenta escondería después */
    fl_eval_mal(q, "8E505", NE_GRANDE);
    fl_eval_mal(q, "-1E99999", NE_GRANDE);
    fl_eval_mal(q, "(8E505)", NE_GRANDE);
    fl_eval_mal(q, "(1E300" S_TIMES "1E300)^0", NE_GRANDE);
    fl_eval_mal(q, "1" S_DIVIDE "(1E300" S_TIMES "1E300)", NE_GRANDE);
    fl_eval_mal(q, "1E300" S_TIMES "1E300-1E300" S_TIMES "1E300", NE_GRANDE);
    fl_eval_mal(q, "1E308+1E308-1E308", NE_GRANDE);
    fl_eval_mal(q, "10^400" S_DIVIDE "10^400", NE_GRANDE);
    fl_eval_da(q, "1E308" S_DIVIDE "10" S_TIMES "10", 1e308);
    fl_eval_da(q, "1E-300" S_DIVIDE "1E300", 0.0);            /* muy pequeño: 0, sin error */
    fl_eval_mal(q, "2+4,5E", NE_NUMERO);
    fl_eval_mal(q, "4,5E+", NE_NUMERO);
    fl_eval_mal(q, "4kk+1", NE_NUMERO);
    fl_eval_mal(q, "2+j3", NE_NUMERO);
    for (i = 0; i < 45; i++) s[i] = '(';              /* más de 40 paréntesis: no se recurre sin fin */
    s[45] = '1'; s[46] = 0;
    num_eval(s, &err);
    chk_cond(q, "45 par" S_ee "ntesis abiertos: un error, sin pasar de 40 niveles", err == NE_NUMERO);
    chk_cond(q, "num_es_cuenta: -5, 2E-3 y 4,7k no; 5-3, (5) y 2" S_TIMES "3 s" S_ii,
             !num_es_cuenta("-5") && !num_es_cuenta("2E-3") && !num_es_cuenta("4,7k") &&
             num_es_cuenta("5-3") && num_es_cuenta("(5)") && num_es_cuenta("2" S_TIMES "3"));
    {
        /* 300 000 textos al azar con las teclas de la ficha: con NE_OK,
         * siempre un número finito; los errores, de la lista */
        static const char AL[] = "0123456789,E+-\xD7\xF7^()kmMu\xB5npG.";
        unsigned lcg = 777u;
        long k, malos = 0, raros = 0;
        int j, n;
        double v;
        for (k = 0; k < 300000L; k++) {
            lcg = lcg * 1103515245u + 12345u;
            n = 1 + (int)((lcg >> 16) % 31u);
            for (j = 0; j < n; j++) { lcg = lcg * 1103515245u + 12345u; s[j] = AL[(lcg >> 16) % (unsigned)(sizeof AL - 1)]; }
            s[n] = 0;
            v = num_eval(s, &err);
            if (err == NE_OK && !(v - v == 0.0)) malos++;
            if (err < NE_OK || err >= NE_N || (err != NE_OK && v != 0.0)) raros++;
        }
        chk_cond(q, "300 000 cuentas al azar: con NE_OK, siempre finito; los errores, con su c" S_oo "digo y 0",
                 malos == 0 && raros == 0);
    }
}

/* ---- F2. Cuentas en la ficha ---- */
static Circuito fl_c_cuentas;             /* el de f_cuenta_larga: P5 (test_r8_pantallas.inc) mira sus rótulos */
static void fl_ficha(void)
{
    static Circuito c;
    const Circuito *cu = ui_circuito();
    const char *q = "F2 cuentas en la ficha";
    Mag m0;
    int k, i, d0;

    r2e_nuevo();
    TECLAS(T_LETRA('R'), T_DER);                          /* R1 (1,1)-(2,1) */
    TECLAS(T_DIG(2), T_DIG(2), T_DIG(0), T_POT, T_DIG(2), T_ENTRE, T_DIG(1), T_DIG(0), T_DIG(0));
    chk_cond(q, "220^2" S_DIVIDE "100 se escribe y el eco dice = 484 " S_OHM,
             ve("220^2" S_DIVIDE "100_") && ve("= 484 " S_OHM));
    chk_cond(q, "escribiendo, la l" S_ii "nea de ayuda dice las cuentas",
             ve("Cuentas: + - " S_TIMES " " S_DIVIDE " ^ ( ) x" S_SUP2 "   [enter] la guarda"));
    r4_vuelca("f_cuenta_eco");
    TECLAS(T_ENTER);
    chk_cond(q, "[enter] guarda el resultado: R1 = 484 (un dato)", ui_pantalla() == P_EDITOR &&
             cu->el[0].val.est == M_DATO && cu->el[0].val.v == 484.0);

    TECLAS(T_LETRA('R'), T_ABAJO);                        /* R2 (2,1)-(2,2) */
    TECLAS(T_DIG(1), T_ENTRE, T_PAR_A, T_DIG(1), T_ENTRE, T_DIG(4), T_LETRA('K'), T_MAS,
           T_DIG(1), T_ENTRE, T_DIG(6), T_LETRA('K'), T_PAR_C);
    chk_cond(q, "1" S_DIVIDE "(1" S_DIVIDE "4k+1" S_DIVIDE "6k): los prefijos, dentro; = 2400",
             ve("1" S_DIVIDE "(1" S_DIVIDE "4k+1" S_DIVIDE "6k)_") && ve("= 2400 " S_OHM));
    TECLAS(T_ENTER);
    chk(q, "el paralelo de 4k y 6k: 2400", cu->el[1].val.v, 2400.0);

    /* una cuenta que no vale: el aviso, el campo como estaba y lo escrito se
     * queda para corregirlo. Tras un cable: con R3 justo al pie de R2, su
     * «2,4kΩ» (encima) cruzaba el zigzag de R2 (f_cuenta_error) */
    TECLAS(T_LETRA('W'), T_IZQ, T_ESC);                   /* cable (2,2)-(1,2) */
    TECLAS(T_LETRA('R'), T_IZQ);                          /* R3 (1,2)-(0,2), con la última R */
    k = cu->nel - 1;
    m0 = cu->el[k].val;
    TECLAS(T_PAR_A, T_DIG(1), T_MAS, T_DIG(2), T_ENTER);
    chk_cond(q, "«(1+2» y [enter]: Falta cerrar un par" S_ee "ntesis, en la ficha y el valor como estaba",
             ui_pantalla() == P_ELEMENTO && ve("Falta cerrar un par" S_ee "ntesis.") &&
             cu->el[k].val.est == m0.est && cu->el[k].val.v == m0.v);
    chk_cond(q, "... y lo escrito sigue ah" S_ii, ve("(1+2_"));
    r4_vuelca("f_cuenta_error");
    TECLAS(T_ABAJO);
    chk_cond(q, "... una flecha no pasa de campo", ui_campo_q() == Q_VAL && ve("Falta cerrar"));
    TECLAS(T_PAR_C);
    chk_cond(q, "... al escribir, el aviso se va", !ve("Falta cerrar") && ve("(1+2)_") && ve("= 3 "));
    TECLAS(T_ENTER);
    chk_cond(q, "... y [enter]: 3", ui_pantalla() == P_EDITOR && cu->el[k].val.v == 3.0);

    TECLAS(T_LETRA('R'), T_ABAJO);                        /* R4 (0,2)-(0,3) */
    k = cu->nel - 1;
    m0 = cu->el[k].val;
    TECLAS(T_DIG(2), T_MAS, T_ESC);
    chk_cond(q, "«2+» y [esc]: a medias, y se queda", ui_pantalla() == P_ELEMENTO &&
             ve("La cuenta est" S_aa " a medias: falta un n" S_uu "mero.") && ve("2+_"));
    TECLAS(T_ESC);
    chk_cond(q, "... el segundo [esc] la tira y sale (el valor como estaba)", ui_pantalla() == P_EDITOR &&
             cu->el[k].val.est == m0.est && cu->el[k].val.v == m0.v);

    TECLAS(T_LETRA('W'), T_DER, T_ESC);                   /* cable (0,3)-(1,3): al pie de R4, el «0,25Ω» de R5 cruzaba su zigzag */
    TECLAS(T_LETRA('R'), T_DER);                          /* R5 (1,3)-(2,3) */
    k = cu->nel - 1;
    TECLAS(T_DIG(1), T_ENTRE, T_DIG(0), T_ENTER);
    chk_cond(q, "1" S_DIVIDE "0: divisi" S_oo "n entre cero", ui_pantalla() == P_ELEMENTO && ve("Divisi" S_oo "n entre cero."));
    TECLAS(T_BORRAR);
    chk_cond(q, "... [del] corrige (y quita el aviso)", ve("1" S_DIVIDE "_") && !ve("Divisi"));
    TECLAS(T_DIG(4), T_ENTER);
    chk_cond(q, "... 1" S_DIVIDE "4 = 0,25", ui_pantalla() == P_EDITOR && cu->el[k].val.v == 0.25);

    TECLAS(T_LETRA('R'), T_DER);                          /* R6 (2,3)-(3,3) */
    k = cu->nel - 1;
    TECLAS(T_DIG(3), T_CUAD);
    chk_cond(q, "[x" S_SUP2 "] escribe ^2: 3^2 = 9", ve("3^2_") && ve("= 9 "));
    TECLAS(T_ENTER);
    chk_cond(q, "... y lo guarda", cu->el[k].val.v == 9.0);

    /* el negativo donde no se escribe signo */
    TECLAS(T_LETRA('R'), T_DER);                          /* R7 (3,3)-(4,3) */
    k = cu->nel - 1;
    m0 = cu->el[k].val;
    TECLAS(T_DIG(2), T_MENOS, T_DIG(5), T_ENTER);
    chk_cond(q, "R = 2-5: da negativo y aqu" S_ii " no lleva signo", ui_pantalla() == P_ELEMENTO &&
             ve("La cuenta da negativo y aqu" S_ii " no lleva signo.") && cu->el[k].val.v == m0.v);
    TECLAS(T_ESC, T_ESC);
    chk_cond(q, "... [esc][esc] sale y el valor sigue", ui_pantalla() == P_EDITOR && cu->el[k].val.v == m0.v &&
             cu->el[k].val.est == m0.est);

    /* en una E sí hay signo: «-5» y «2E-3» como siempre, y las restas */
    TECLAS(T_LETRA('E'), T_DER);                          /* E1 (4,3)-(5,3) */
    k = cu->nel - 1;
    TECLAS(T_SIGNO, T_DIG(5), T_ENTER);
    chk_cond(q, "E: (-) 5 sigue siendo -5", cu->el[k].val.v == -5.0 && cu->el[k].val.est == M_DATO);
    TECLAS(T_LETRA('E'), T_DER);                          /* E2 (5,3)-(6,3) */
    k = cu->nel - 1;
    TECLAS(T_DIG(2), T_MENOS, T_DIG(5), T_ENTER);
    chk_cond(q, "E: 2 [-] 5 es una resta: -3", cu->el[k].val.v == -3.0);
    TECLAS(T_LETRA('E'), T_DER);                          /* E3 (6,3)-(7,3) */
    k = cu->nel - 1;
    TECLAS(T_DIG(2), T_POR, T_MENOS, T_DIG(3), T_ENTER);
    chk_cond(q, "E: 2 " S_TIMES " (-) 3 = -6 (el - tras " S_TIMES ", del n" S_uu "mero)", cu->el[k].val.v == -6.0);
    TECLAS(T_LETRA('E'), T_DER);                          /* E4 (7,3)-(8,3) */
    k = cu->nel - 1;
    TECLAS(T_DIG(2), T_EE, T_MENOS, T_DIG(3));
    chk_cond(q, "2 [EE] (-) 3: el signo del exponente, = 0,002", ve("2E-3_") && ve("= 0,002 V"));
    TECLAS(T_ENTER);
    chk_cond(q, "... 0,002", cu->el[k].val.v == 0.002);

    /* los signos solo entran donde tienen sentido */
    TECLAS(T_LETRA('R'), T_ABAJO);                        /* R8 (8,3)-(8,4) */
    k = cu->nel - 1;
    TECLAS(T_MAS, T_PAR_C, T_POR, T_ENTRE, T_POT, T_CUAD);
    chk_cond(q, "al principio, + ) " S_TIMES " " S_DIVIDE " ^ x" S_SUP2 " no escriben nada",
             !ve("+_") && !ve(")_") && !ve(S_TIMES "_") && !ve(S_DIVIDE "_") && !ve("^_") && !ve("^2_"));
    TECLAS(T_DIG(7), T_PAR_A, T_PAR_C);
    chk_cond(q, "«(» tras una cifra y «)» sin abrir, tampoco", ve("7_") && !ve("7(") && !ve("7)"));
    TECLAS(T_MAS, T_MAS, T_MENOS, T_POR);
    chk_cond(q, "dos signos seguidos, no («7+»)", ve("7+_"));
    TECLAS(T_PAR_A, T_MENOS, T_EE, T_DIG(2), T_PAR_C, T_PUNTO, T_LETRA('K'), T_EE, T_DIG(3));
    chk_cond(q, "«(», el - de dentro, «)»; tras «)» ni coma, ni prefijo, ni [EE], ni cifra que valga",
             ve("7+(-2)3_") && ve("..."));
    TECLAS(T_BORRAR);
    chk_cond(q, "... = 5", ve("7+(-2)_") && ve("= 5 " S_OHM));
    TECLAS(T_ENTER);
    chk_cond(q, "... 5", ui_pantalla() == P_EDITOR && cu->el[k].val.v == 5.0);

    /* 31 caracteres, y la caja enseña el final */
    TECLAS(T_LETRA('R'), T_ABAJO);                        /* R9 (8,4)-(8,5) */
    k = cu->nel - 1;
    d0 = gfx_desbordes();
    for (i = 0; i < 16; i++) TECLAS(T_DIG(1), T_MAS);
    chk_cond(q, "una cuenta de 31 (el signo 32.º no entra): = 16", ve("+1+1_") && ve("= 16 " S_OHM));
    r4_vuelca("f_cuenta_larga");
    fl_c_cuentas = *cu;
    chk_cond(q, "... no se sale de la pantalla", gfx_desbordes() == d0);
    TECLAS(T_ENTER);
    chk_cond(q, "... 16", cu->el[k].val.v == 16.0);
    chk_cond(q, "(el circuito, sano)", circ_valido(cu));

    /* CA: el módulo de una fuente alterna admite cuentas; la Z, no ([+] es +j) */
    ce_rlc(&c);
    chk_cond(q, "(RLC abierto)", r3u_abre(&c));
    TECLAS(T_TAB, T_ENTER);
    chk_cond(q, "(la ficha de E1, en CA)", ui_pantalla() == P_ELEMENTO && ui_sel() == 0);
    TECLAS(T_DIG(1), T_DIG(0), T_DIG(0), T_POR, T_DIG(2));
    chk_cond(q, "CA: el m" S_oo "dulo de E1, 100" S_TIMES "2 = 200", ve("100" S_TIMES "2_") && ve("= 200 V"));
    TECLAS(T_ENTER);
    chk_cond(q, "... 200 (eficaz), el " S_aa "ngulo como estaba", cu->el[0].val.v == 200.0 &&
             cu->el[0].val.est_ang == M_DATO && cu->el[0].val.ang == 0.0);
    circ_init(&c, "Z");
    k = circ_elemento(&c, EL_FUENTE, 0, 1, DIR_ARR);
    c.el[k].variante = FORMA_FASOR; u4_dato(&c.el[k].val, 10.0); c.el[k].val.est_ang = M_DATO;
    u4_dato(&c.frec, 50.0);
    k = circ_elemento(&c, EL_IMP, 0, 0, DIR_DER); u4_dato(&c.el[k].val, 50.0); c.el[k].val.est_ang = M_DATO;
    circ_cable(&c, 1, 0, DIR_ABA); circ_cable(&c, 1, 1, DIR_IZQ);
    chk_cond(q, "(Z abierta)", r3u_abre(&c));
    TECLAS(T_TAB, T_TAB, T_ENTER, T_DIG(3));
    chk_cond(q, "la Z: escribiendo, sin la ayuda de las cuentas", ve("3_") && !ve("Cuentas:"));
    TECLAS(T_POR);
    chk_cond(q, "la Z: " S_TIMES " no entra y lo dice", ve("3_") && ve("Aqu" S_ii " [+] y [-] son la parte j: sin cuentas."));
    TECLAS(T_MAS, T_DIG(4), T_ENTER);
    chk_cond(q, "... y 3 [+] 4 sigue siendo 3+j4", cu->el[1].val.v == 5.0);
    TECLAS(T_ESC);

    /* la fila x de una controlada: tampoco */
    u4_controladas(&c, 0);
    chk_cond(q, "(la controlada abierta)", r3u_abre(&c));
    TECLAS(T_TAB, T_TAB, T_TAB, T_ENTER);
    for (i = 0; i < 6 && ui_campo_q() != Q_CTRL; i++) TECLAS(T_ABAJO);
    TECLAS(T_PAR_A);
    chk_cond(q, "en la fila x de E2, «(»: el control se elige con [+][-]", ui_campo_q() == Q_CTRL &&
             ve("El control se elige con [+][-]."));
    TECLAS(T_ESC);

    /* un número solo que no cabe en un double: num_eval dice NE_GRANDE,
     * pero la ficha hace lo de siempre (el eco «inf», como en R34, y el
     * aviso del rango, el que dice el máximo); una cuenta que se sale, no */
    r2e_nuevo();
    TECLAS(T_LETRA('R'), T_DER);
    TECLAS(T_DIG(8), T_EE, T_DIG(5), T_DIG(0), T_DIG(5));
    chk_cond(q, "«8E505»: el eco, como siempre, «inf»", ve("8E505_") && ve("inf"));
    TECLAS(T_POR, T_DIG(0));
    chk_cond(q, "«8E505" S_TIMES "0»: la cuenta no esconde el infinito («...»)", ve("8E505" S_TIMES "0_") && ve("...") && !ve("= 0"));
    TECLAS(T_BORRAR, T_BORRAR);
    TECLAS(T_ENTER);
    chk_cond(q, "... y [enter]: «Demasiado grande (m" S_aa "ximo...)», y R1 sin dato", ui_pantalla() == P_ELEMENTO &&
             ve("Demasiado grande (m" S_aa "ximo 1 000 000 000).") && cu->el[0].val.est != M_DATO);
    TECLAS(T_ESC, T_ESC);
    limpia_mis();
}

/* ---- F3. Copiar, cortar y pegar ---- */
static void fl_pegar(void)
{
    static Circuito c;
    const Circuito *cu = ui_circuito();
    const char *q = "F3 copiar y pegar";
    int k, n, d, d0, esp, ox, oy, r1;

    ui_init();
    TECLAS(T_ENTER, T_ENTER, T_ENTER);
    TECLAS(T_LETRA('V') | T_CON_CTRL);
    chk_cond(q, "sin copiar nada, [ctrl][V] lo dice", ve("Nada que pegar: [ctrl][C] copia el elegido.") &&
             ui_indice(UI_I_ACCION) == 0);
    TECLAS(T_LETRA('C') | T_CON_CTRL);
    chk_cond(q, "sin elegido ni nada junto al cursor, [ctrl][C] lo dice", ve("Elige primero un elemento: [tab] o [enter]."));

    /* R1 = 4k7 con I de dato y V [?!]: se copia el valor, no los datos */
    TECLAS(T_LETRA('R'), T_DER, T_DIG(4), T_PUNTO, T_DIG(7), T_LETRA('K'), T_ABAJO, T_INCOG, T_ABAJO, T_DIG(2), T_ENTER);
    chk_cond(q, "(R1 4,7k con V [?!] e I = 2)", cu->nel == 1 && cu->el[0].val.v == 4700.0 &&
             cu->el[0].V.est == M_BUSCAR && cu->el[0].I.est == M_DATO);
    TECLAS(T_ESC, T_TAB, T_LETRA('C') | T_CON_CTRL);
    chk_cond(q, "[tab] y [ctrl][C]: Copiado R1 (su valor)", ve("Copiado R1 (4,7k") && ve("): [ctrl][V] y una flecha."));
    r4_vuelca("f_copiado");
    n = ui_niveles(0);
    TECLAS(T_ABAJO, T_ABAJO, T_LETRA('V') | T_CON_CTRL);
    chk_cond(q, "[ctrl][V]: la instrucci" S_oo "n de pegar", ve("Pegar R1 (4,7k") && ve("): una flecha. [esc] no") &&
             ui_indice(UI_I_ACCION) != 0 && ui_niveles(0) == n);
    r4_vuelca("f_pegar");
    TECLAS(T_DER);
    chk_cond(q, "una flecha pone la copia: R2, 4,7k, en su ficha", cu->nel == 2 && strcmp(cu->el[1].etiq, "R2") == 0 &&
             cu->el[1].val.est == M_DATO && cu->el[1].val.v == 4700.0 && ui_pantalla() == P_ELEMENTO && ui_sel() == 1 &&
             ve("La copia, con sus valores: rev" S_ii "salos."));
    chk_cond(q, "... sin los datos de V e I ni el [?!]", cu->el[1].V.est == M_LIBRE && cu->el[1].I.est == M_LIBRE);
    chk_cond(q, "... en un nivel de deshacer", ui_niveles(0) == n + 1);
    r4_vuelca("f_pegado");
    TECLAS(T_CTRL, T_LETRA('Z'));
    chk_cond(q, "[ctrl][Z] (en la ficha) la quita", ui_pantalla() == P_EDITOR && cu->nel == 1);
    TECLAS(T_CTRL, T_LETRA('Y'));
    chk_cond(q, "[ctrl][Y] la devuelve", cu->nel == 2 && cu->el[1].val.v == 4700.0);

    /* cortar: un nivel; el de detrás se renumera y pegar lo pone en otra parte
     * (rehacer deja el editor sin elegido: [tab] elige R1) */
    TECLAS(T_TAB, T_LETRA('X') | T_CON_CTRL);
    chk_cond(q, "[ctrl][X] corta R1 (queda R2) y lo dice", cu->nel == 1 && strcmp(cu->el[0].etiq, "R2") == 0 &&
             ve("Cortado R1 (4,7k"));
    TECLAS(T_CTRL, T_LETRA('Z'));
    chk_cond(q, "... [ctrl][Z] lo devuelve", cu->nel == 2 && r4_el(cu, "R1") >= 0);
    TECLAS(T_CTRL, T_LETRA('Y'));
    chk_cond(q, "... [ctrl][Y] lo corta otra vez", cu->nel == 1);
    fl_cursor(4, 4);
    TECLAS(T_LETRA('V') | T_CON_CTRL, T_ABAJO);
    k = r4_el(cu, "R1");
    chk_cond(q, "... y se pega en (4,4)-(4,5) con su etiqueta libre (R1) y su valor", cu->nel == 2 && k >= 0 &&
             cu->el[k].xa == 4 && cu->el[k].ya == 4 && cu->el[k].xb == 4 && cu->el[k].yb == 5 && cu->el[k].val.v == 4700.0);
    TECLAS(T_ESC);

    /* sin elegir: el único elemento junto al cursor; con dos, pregunta */
    TECLAS(T_ESC);
    chk_cond(q, "(nada elegido)", ui_sel() < 0);
    TECLAS(T_LETRA('C') | T_CON_CTRL);
    chk_cond(q, "sin elegir, el que toca el cursor (R1)", ve("Copiado R1"));

    /* [esc] deja de pegar, y otra tecla también: una C que se copió no se
     * pone en la R que viene detrás */
    TECLAS(T_LETRA('C'), T_IZQ, T_DIG(1), T_LETRA('U'), T_ENTER);
    chk_cond(q, "(C1 de 1 " S_MU "F, elegido al volver de su ficha)", ui_sel() >= 0 && cu->el[ui_sel()].tipo == EL_COND &&
             cu->el[ui_sel()].val.v == 1e-6);
    TECLAS(T_LETRA('C') | T_CON_CTRL, T_LETRA('V') | T_CON_CTRL, T_ESC);
    chk_cond(q, "[ctrl][V] y [esc]: nada", ui_indice(UI_I_ACCION) == 0);
    n = cu->nel;
    fl_cursor(6, 1);
    TECLAS(T_LETRA('V') | T_CON_CTRL, T_LETRA('R'), T_DER);
    chk_cond(q, "[ctrl][V] y luego [R]: una R nueva, sin los valores de C1", cu->nel == n + 1 &&
             cu->el[n].tipo == EL_RES && cu->el[n].val.v != 1e-6);
    TECLAS(T_ESC);

    /* el portapapeles pasa a otro problema */
    TECLAS(T_ESC, T_ESC);
    chk_cond(q, "(Taller)", ui_pantalla() == P_TALLER);
    TECLAS(T_ARRIBA, T_ENTER, T_LETRA('V') | T_CON_CTRL, T_DER);
    chk_cond(q, "Problema nuevo y [ctrl][V]: C1 de 1 " S_MU "F", ui_pantalla() == P_ELEMENTO && cu->nel == 1 &&
             cu->el[0].tipo == EL_COND && cu->el[0].val.v == 1e-6 && strcmp(cu->el[0].etiq, "C1") == 0);
    TECLAS(T_ESC);

    /* pegar encima de un elemento lo sustituye, y lo dice */
    TECLAS(T_IZQ, T_LETRA('V') | T_CON_CTRL, T_DER);
    chk_cond(q, "pegar sobre C1: lo sustituye y lo dice", ui_pantalla() == P_ELEMENTO && cu->nel == 1 && ve("Sustituye a C1"));
    TECLAS(T_ESC);

    /* una controlada: quién la manda va por la etiqueta */
    u4_controladas(&c, 0);
    c.el[2].ref = 1;                                      /* E2 = μ·V(R1) */
    chk_cond(q, "(E2 controlada por R1, abierta)", r3u_abre(&c));
    TECLAS(T_TAB, T_TAB, T_TAB, T_LETRA('C') | T_CON_CTRL);
    chk_cond(q, "(E2 copiada)", ve("Copiado E2"));
    fl_cursor(0, 0);
    TECLAS(T_BORRAR, T_ABAJO, T_ESC);                     /* se borra E1 (cortarla pisaría la copia) */
    r1 = r4_el(cu, "R1");
    chk_cond(q, "(E1 borrada: R1 es el elemento 0; E2, el 1)", cu->nel == 2 && r1 == 0);
    fl_cursor(3, 3);
    TECLAS(T_LETRA('V') | T_CON_CTRL, T_DER);
    k = cu->nel - 1;
    chk_cond(q, "la copia de E2 la manda R1 (por su etiqueta, no por su n" S_uu "mero de antes)", cu->nel == 3 &&
             cu->el[k].tipo == EL_EDEP && cu->el[k].ref == r1 && cu->el[k].val.v == 2.0);
    chk_cond(q, "... y no pide elegir el control (la ficha, en su primer campo)", ui_campo_q() != Q_CTRL);
    TECLAS(T_ESC, T_ESC, T_ESC);
    TECLAS(T_ARRIBA, T_ENTER, T_LETRA('V') | T_CON_CTRL, T_DER);
    chk_cond(q, "en otro problema, sin R1: sin control, y la ficha va a su fila x", cu->nel == 1 &&
             cu->el[0].ref == -1 && ui_campo_q() == Q_CTRL);
    TECLAS(T_ESC);

    /* un BJT PNP con β = 150 y el espejo ([F]) */
    circ_init(&c, "BJT");
    k = circ_elemento(&c, EL_BJT, 1, 3, DIR_DER);
    c.el[k].variante = BJT_PNP;
    u4_dato(&c.el[k].par[QB_BF - Q_P1], 150.0);
    circ_voltear(&c, k);
    chk_cond(q, "(Q1 con espejo)", circ_orienta(&c.el[k], &d0, &esp, &ox, &oy) && esp == 1);
    chk_cond(q, "(abierto)", r3u_abre(&c));
    TECLAS(T_TAB, T_LETRA('C') | T_CON_CTRL);
    fl_cursor(6, 3);
    TECLAS(T_LETRA('V') | T_CON_CTRL, T_DER);
    k = cu->nel - 1;
    chk_cond(q, "Q2: PNP, " S_BETA " = 150 y con espejo, en su caja", cu->nel == 2 && cu->el[k].tipo == EL_BJT &&
             cu->el[k].variante == BJT_PNP && cu->el[k].par[QB_BF - Q_P1].v == 150.0 &&
             circ_orienta(&cu->el[k], &d, &esp, &ox, &oy) && esp == 1 && d == d0 && circ_valido(cu));
    r4_vuelca("f_pegado_bjt");
    TECLAS(T_ESC);

    /* una fuente alterna de 50 Hz en un problema de CC: pasa a CA con su f */
    ce_rlc(&c);
    u4_dato(&c.frec, 50.0);
    chk_cond(q, "(RLC a 50 Hz)", r3u_abre(&c));
    TECLAS(T_TAB, T_LETRA('C') | T_CON_CTRL, T_ESC, T_ESC);          /* sin elegido; al Taller */
    TECLAS(T_ARRIBA, T_ENTER, T_LETRA('V') | T_CON_CTRL, T_ARRIBA);
    chk_cond(q, "la E alterna en un problema nuevo: CA y f = 50 Hz", cu->nel == 1 && circ_alterna(cu) &&
             cu->frec.est == M_DATO && cu->frec.v == 50.0 && cu->el[0].variante == FORMA_FASOR);
    TECLAS(T_ESC);

    /* desde la ficha: [ctrl] con C, X, V o N confirma lo escrito y lo hace
     * el editor ([ctrl][X] no marca [?!]; [ctrl][N] no escribe un nano) */
    r2e_nuevo();
    TECLAS(T_LETRA('R'), T_DER, T_DIG(3), T_LETRA('C') | T_CON_CTRL);
    chk_cond(q, "ficha: 3 y [ctrl][C] lo confirma y copia R1, en el editor", ui_pantalla() == P_EDITOR &&
             cu->el[0].val.v == 3.0 && ve("Copiado R1"));
    TECLAS(T_ENTER, T_DIG(7), T_LETRA('X') | T_CON_CTRL);
    chk_cond(q, "ficha: 7 y [ctrl][X] lo corta (con el 7), sin [?!]", ui_pantalla() == P_EDITOR && cu->nel == 0 &&
             ve("Cortado R1 (7"));
    TECLAS(T_LETRA('V') | T_CON_CTRL, T_ABAJO);
    chk_cond(q, "... y se pega con el 7", cu->nel == 1 && cu->el[0].val.v == 7.0 && cu->el[0].V.est == M_LIBRE);
    TECLAS(T_LETRA('V') | T_CON_CTRL);
    chk_cond(q, "ficha: [ctrl][V] vuelve al editor pegando", ui_pantalla() == P_EDITOR && ve("Pegar R1 (7"));
    TECLAS(T_ESC, T_ENTER, T_LETRA('N') | T_CON_CTRL);
    chk_cond(q, "ficha: [ctrl][N], el nombre en el editor", ui_pantalla() == P_EDITOR && ve("Nombre: _") &&
             cu->el[0].val.v == 7.0);
    TECLAS(T_ESC);
    limpia_mis();
}

/* ---- F4. Nombres ---- */
static void fl_nombres(void)
{
    static Circuito c, x;
    const Circuito *cu = ui_circuito();
    const char *q = "F4 nombres";
    int i, nr;

    limpia_mis();
    r2e_nuevo();
    TECLAS(T_LETRA('R'), T_DER, T_DIG(4), T_ENTER, T_CTRL, T_LETRA('S'));
    chk_cond(q, "guardar con el nombre del programa lo dice: [ctrl][N] le pone nombre",
             arch_existe(0) && ve("Guardado: Mi problema 1. [ctrl][N] le pone nombre."));
    TECLAS(T_ESC, T_ESC, T_ABAJO, T_ENTER);               /* Taller > Mis problemas */
    chk_cond(q, "Mis problemas dice [N] en el pie", ui_pantalla() == P_MIS &&
             ve("[enter] abrir [N] nombre [del] borrar [esc] volver"));
    TECLAS(T_LETRA('N'));
    chk_cond(q, "[N]: la l" S_ii "nea del nombre, vac" S_ii "a (el que ten" S_ii "a era del programa)",
             ve("Nombre: _") && ve("[letras] nombre [del] borra [enter] guarda [esc] no"));
    TECLAS(T_LETRA('T'), T_LETRA('A'), T_LETRA('R'), T_LETRA('E'), T_LETRA('A'), T_ESPACIO, T_DIG(3));
    chk_cond(q, "se escribe: la primera en may" S_uu "scula, el espacio y la cifra", ve("Nombre: Tarea 3_"));
    TECLAS(T_SHIFT, T_LETRA('B'), T_LETRA('C') | T_CON_SHIFT, T_LETRA('D'));
    chk_cond(q, "[shift] (armado o abajo): may" S_uu "scula", ve("Nombre: Tarea 3BCd_"));
    r4_vuelca("f_nombre_mis");
    TECLAS(T_BORRAR, T_BORRAR, T_BORRAR);
    chk_cond(q, "[del] borra", ve("Nombre: Tarea 3_"));
    TECLAS(T_MENU, T_ARRIBA, T_ABAJO, T_IZQ);
    chk_cond(q, "[menu] y las flechas no hacen nada", ui_pantalla() == P_MIS && ve("Nombre: Tarea 3_"));
    TECLAS(T_ENTER);
    chk_cond(q, "[enter]: en la lista y en el archivo", ui_pantalla() == P_MIS && ve("Tarea 3") &&
             !ve("Nombre:") && arch_cargar(0, &x) && strcmp(x.nombre, "Tarea 3") == 0);
    chk_cond(q, "... y en el editor (es el problema abierto)", strcmp(cu->nombre, "Tarea 3") == 0);
    TECLAS(T_LETRA('N'));
    chk_cond(q, "[N] otra vez: empieza con el que tiene", ve("Nombre: Tarea 3_"));
    TECLAS(T_BORRAR | T_CON_CTRL);
    chk_cond(q, "[ctrl][del]: todo", ve("Nombre: _"));
    TECLAS(T_ESPACIO, T_ESPACIO, T_ENTER);
    chk_cond(q, "vac" S_ii "o (el espacio no entra al principio): [enter] pide un nombre",
             ve("Nombre: _") && ve("Escribe un nombre ([esc] lo deja como estaba)."));
    for (i = 0; i < 25; i++) repite(T_LETRA('A') + (i % 26), 1);
    chk_cond(q, "hasta 23 caracteres, y lo dice", ve("Nombre: Abcdefghijklmnopqrstuvw_") &&
             ve("No caben m" S_aa "s: hasta 23 caracteres."));
    TECLAS(T_ESC);
    chk_cond(q, "[esc]: se deja como estaba", !ve("Nombre:") && arch_cargar(0, &x) && strcmp(x.nombre, "Tarea 3") == 0);

    /* el problema abierto, con un cambio: sus niveles de deshacer toman el
     * nombre (si no, deshacer y Guardar volverían a escribir el de antes) */
    TECLAS(T_ESC, T_ARRIBA, T_ENTER);                     /* Taller > Seguir */
    TECLAS(T_LETRA('R'), T_ABAJO, T_DIG(5), T_ENTER, T_ESC, T_ESC);
    chk_cond(q, "(un cambio: un nivel de deshacer)", ui_niveles(&nr) >= 1 && ui_pantalla() == P_TALLER);
    TECLAS(T_ABAJO, T_ENTER, T_LETRA('N'), T_BORRAR | T_CON_CTRL, T_LETRA('P'), T_LETRA('R'), T_ENTER);
    chk_cond(q, "(renombrado a «Pr»)", arch_cargar(0, &x) && strcmp(x.nombre, "Pr") == 0);
    TECLAS(T_ESC, T_ARRIBA, T_ENTER, T_CTRL, T_LETRA('Z'));
    chk_cond(q, "deshacer tras renombrar: el nombre nuevo", cu->nel == 1 && strcmp(cu->nombre, "Pr") == 0);

    /* [ctrl][N] en el editor: le pone nombre y lo guarda */
    limpia_mis();
    r2e_nuevo();
    TECLAS(T_CTRL, T_LETRA('N'));
    chk_cond(q, "[ctrl][N] con el circuito vac" S_ii "o: nada que guardar", ve("vac" S_ii "o: nada que guardar") && !ve("Nombre:"));
    TECLAS(T_LETRA('R'), T_DER, T_DIG(4), T_ENTER, T_CTRL, T_LETRA('N'));
    chk_cond(q, "[ctrl][N]: la l" S_ii "nea del nombre en el editor", ui_pantalla() == P_EDITOR &&
             ve("Nombre: _") && ve("[letras] nombre [del] borra [enter] guarda [esc] no"));
    TECLAS(T_LETRA('P'), T_LETRA('U'), T_LETRA('E'), T_LETRA('N'), T_LETRA('T'), T_LETRA('E'));
    chk_cond(q, "las letras escriben (la R no pone una resistencia)", ve("Nombre: Puente_") &&
             ui_indice(UI_I_ACCION) == 0 && cu->nel == 1);
    r4_vuelca("f_nombre_editor");
    TECLAS(T_ENTER);
    chk_cond(q, "[enter] lo guarda con ese nombre", arch_cargar(0, &x) && strcmp(x.nombre, "Puente") == 0 &&
             strcmp(cu->nombre, "Puente") == 0 && ve("Guardado: Puente") && !ve("[ctrl][N] le pone"));
    TECLAS(T_CTRL, T_LETRA('N'));
    chk_cond(q, "otra vez: empieza con el que tiene", ve("Nombre: Puente_"));
    TECLAS(T_ESPACIO, T_SHIFT, T_LETRA('W'));
    TECLAS(T_ENTER);
    chk_cond(q, "... y lo cambia en su hueco (sin otro archivo)", arch_cargar(0, &x) &&
             strcmp(x.nombre, "Puente W") == 0 && !arch_existe(1));
    TECLAS(T_CTRL, T_LETRA('N'), T_LETRA('X'), T_ESC);
    chk_cond(q, "[esc]: sin cambiar nada", !ve("Nombre:") && strcmp(cu->nombre, "Puente W") == 0);

    /* un nombre escrito que se parece a los del programa se respeta: en uno
     * nuevo, con [ctrl][N]; y su copia es la suya, «(2)» */
    limpia_mis();
    r2e_nuevo();
    TECLAS(T_LETRA('R'), T_DER, T_DIG(4), T_ENTER, T_CTRL, T_LETRA('N'));
    TECLAS(T_LETRA('M'), T_LETRA('I'), T_ESPACIO, T_LETRA('P'), T_LETRA('R'), T_LETRA('O'), T_LETRA('B'),
           T_LETRA('L'), T_LETRA('E'), T_LETRA('M'), T_LETRA('A'), T_ESPACIO, T_DIG(9));
    TECLAS(T_ENTER);
    chk_cond(q, "[ctrl][N] «Mi problema 9» en uno nuevo: se guarda con ese nombre, sin «le pone nombre»",
             arch_cargar(0, &x) && strcmp(x.nombre, "Mi problema 9") == 0 && strcmp(cu->nombre, "Mi problema 9") == 0 &&
             ve("Guardado: Mi problema 9") && !ve("[ctrl][N] le pone"));
    TECLAS(T_CTRL, T_LETRA('N'));
    chk_cond(q, "... [ctrl][N] otra vez empieza con " S_ee "l (no es del programa)", ve("Nombre: Mi problema 9_"));
    TECLAS(T_ESC);
    TECLAS(T_MENU); repite(T_ARRIBA, 2); TECLAS(T_ENTER);     /* Guardar como copia */
    chk_cond(q, "... y su copia, «Mi problema 9 (2)»", strcmp(cu->nombre, "Mi problema 9 (2)") == 0 &&
             arch_cargar(1, &x) && strcmp(x.nombre, "Mi problema 9 (2)") == 0);
    r2e_nuevo();
    TECLAS(T_LETRA('R'), T_DER, T_DIG(4), T_ENTER, T_CTRL, T_LETRA('N'));
    TECLAS(T_LETRA('P'), T_LETRA('R'), T_LETRA('O'), T_LETRA('B'), T_LETRA('L'), T_LETRA('E'), T_LETRA('M'),
           T_LETRA('A'), T_ESPACIO, T_LETRA('N'), T_LETRA('U'), T_LETRA('E'), T_LETRA('V'), T_LETRA('O'));
    TECLAS(T_ENTER);
    chk_cond(q, "[ctrl][N] «Problema nuevo»: se guarda con ese nombre", arch_cargar(2, &x) &&
             strcmp(x.nombre, "Problema nuevo") == 0 && strcmp(cu->nombre, "Problema nuevo") == 0);
    TECLAS(T_MENU); repite(T_ARRIBA, 2); TECLAS(T_ENTER);
    chk_cond(q, "... y su copia, «Problema nuevo (2)»", strcmp(cu->nombre, "Problema nuevo (2)") == 0);
    /* los del programa siguen siendo del programa: «Mi problema 1» en su
     * hueco, al copiarse, «Mi problema N» del hueco nuevo */
    limpia_mis();
    r2e_nuevo();
    TECLAS(T_LETRA('R'), T_DER, T_DIG(4), T_ENTER, T_CTRL, T_LETRA('S'));
    TECLAS(T_MENU); repite(T_ARRIBA, 2); TECLAS(T_ENTER);
    chk_cond(q, "(«Mi problema 1» en el hueco 1: su copia, «Mi problema 2»)", strcmp(cu->nombre, "Mi problema 2") == 0);

    /* un ejemplo traído tiene nombre propio: [ctrl][N] empieza con él */
    lazo(&c, 0, 12.0, 4.0, 8.0);
    strcpy(c.nombre, "Hallar la corriente");
    chk_cond(q, "(abierto)", r3u_abre(&c));
    TECLAS(T_CTRL, T_LETRA('N'));
    chk_cond(q, "un nombre propio: empieza con " S_ee "l", ve("Nombre: Hallar la corriente_"));
    TECLAS(T_ESC);
    limpia_mis();
}

/* ---- F5. Por la capa ---- */
static void fl_capa(void)
{
    const char *q = "F5 por la capa";
    static const struct { int tf, fila; unsigned bit; } TAB[] = {
        { TF_ESPACIO, 0, 0x010 }, { TF_POR, 4, 0x100 }, { TF_ENTRE, 3, 0x100 }, { TF_POT, 4, 0x200 },
        { TF_CUAD, 2, 0x200 }, { TF_PAR_A, 5, 0x020 }, { TF_PAR_C, 5, 0x008 },
    };
    char txt[48];
    int i, f;
    unsigned b;
#ifdef POR_LA_CAPA
    ce_directo = 1;
#endif
    for (i = 0; i < (int)(sizeof TAB / sizeof TAB[0]); i++) {
        sprintf(txt, "la tecla f" S_ii "sica %d: fila %d, bit 0x%03x (keys.h)", TAB[i].tf, TAB[i].fila, TAB[i].bit);
        chk_cond(q, txt, ent_tecla_fis(TAB[i].tf, &f, &b) && f == TAB[i].fila && b == TAB[i].bit);
    }
    /* × en la ficha: una vez, aunque se mantenga (no está en la lista blanca) */
    cm_ficha();
    ce_abre(CE_W, CE_H, 1);
    ce_pulsa(TF_DIG(3)); ce_pulsa(TF_POR); ce_pulsa(TF_DIG(4));
    chk_cond(q, "3 " S_TIMES " 4 por la capa: = 12", ve("3" S_TIMES "4_") && ve("= 12 V"));
    ce_mantiene(TF_POR, 1500); ce_pasa(60);
    chk_cond(q, S_TIMES " mantenido 1,5 s: uno solo", ve("3" S_TIMES "4" S_TIMES "_") && ce_cuenta(T_POR) == 2);
    ce_pulsa(TF_PAR_A); ce_pulsa(TF_DIG(2)); ce_pulsa(TF_PAR_C); ce_pulsa(TF_CUAD);
    chk_cond(q, "( 2 ) x" S_SUP2 ": = 48", ve("3" S_TIMES "4" S_TIMES "(2)^2_") && ve("= 48 V"));
    ce_pulsa(TF_ESC); ce_pulsa(TF_ESC);
    /* [del] mantenido en un nombre: se repite y para en el vacío */
    cm_mis();
    ce_abre(CE_W, CE_H, 1);
    ce_pulsa(TF_LETRA('N')); ce_pulsa(TF_ESPACIO); ce_pulsa(TF_DIG(2));
    chk_cond(q, "[N], [espacio] y 2 por la capa", ve("Nombre: Escalera 2_"));
    ce.nev = 0;
    ce_mantiene(TF_DEL, 2000); ce_pasa(60);
    chk_cond(q, "[del] mantenido 2 s: borra las 10 y para (10 eventos)", ve("Nombre: _") && ce_cuenta(T_BORRAR) == 10);
    ce_pulsa(TF_ESC);
    chk_cond(q, "[esc]: el nombre como estaba", !ve("Nombre:") && ve("Escalera"));
    ce.nev = 0;
    ce_mantiene(TF_DEL, 1500); ce_pasa(60);
    chk_cond(q, "fuera del nombre, [del] en Mis sigue sin repetirse", ce_cuenta(T_BORRAR) == 1 &&
             ve("[del] otra vez para BORRARLO"));
    ce_pulsa(TF_ESC);
    limpia_mis();
    ce_listo = 0;                                             /* capa_tecla abre otra */
#ifdef POR_LA_CAPA
    ce_directo = 0;
#endif
}

/* ---- F6. Independencia del dibujo con las teclas nuevas ---- */
static void fl_indep_de(const char *nom, void (*monta)(void), int pant_esperada)
{
    static const int K[] = {
        T_ARRIBA, T_ABAJO, T_IZQ, T_DER, T_DIG(1), T_DIG(5), T_PUNTO, T_MAS, T_MENOS, T_BORRAR,
        T_POR, T_ENTRE, T_POT, T_CUAD, T_PAR_A, T_PAR_C, T_ESPACIO, T_LETRA('K'), T_LETRA('N'),
        T_LETRA('A'), T_LETRA('C') | T_CON_CTRL, T_LETRA('V') | T_CON_CTRL, T_LETRA('B') | T_CON_SHIFT, T_EE
    };
    const int NK = (int)(sizeof K / sizeof K[0]);
    static unsigned long suma[150];
    char det[96];
    int pasada, i, j, n[2], llega = 0, distinta = -1;
    for (pasada = 0; pasada < 2; pasada++) {
        monta();
        if (pasada == 0) llega = ui_pantalla() == pant_esperada;
        ce_lcg = 777u;
        for (i = 0; i < 150; i++) {
            int t = K[ce_azar((unsigned)NK)], nb;
            unsigned long h = 2166136261UL;
            if (pasada == 0) ui_dibuja();
            ui_tecla(t);
            nb = ui_foto(ce_fa, (int)sizeof ce_fa);
            for (j = 0; j < nb; j++) h = ((h ^ ce_fa[j]) * 16777619UL) & 0xFFFFFFFFUL;
            if (pasada == 0) suma[i] = h;
            else if (distinta < 0 && suma[i] != h) distinta = i;
        }
        n[pasada] = ui_foto(pasada ? ce_fb : ce_fa, (int)sizeof ce_fa);
    }
    sprintf(det, "%s: se llega a la pantalla", nom);
    chk_cond("F6 independencia del dibujo", det, llega);
    sprintf(det, "%s: 150 teclas con y sin dibujo, el mismo estado", nom);
    chk_cond("F6 independencia del dibujo", det, distinta < 0 && n[0] == n[1] && memcmp(ce_fa, ce_fb, (size_t)n[0]) == 0);
}

static void fl_m_ficha(void)  { r2e_nuevo(); TECLAS(T_LETRA('R'), T_DER, T_DIG(2), T_POR); }
static void fl_m_nombre(void) { cm_mis(); TECLAS(T_LETRA('N')); }
static void fl_m_pegar(void)  { cm_editor(); TECLAS(T_TAB, T_LETRA('C') | T_CON_CTRL); fl_cursor(5, 4); }

static void fl_indep(void)
{
    fl_indep_de("la ficha con una cuenta", fl_m_ficha, P_ELEMENTO);
    fl_indep_de("un nombre en Mis problemas", fl_m_nombre, P_MIS);
    fl_indep_de("el editor con algo copiado", fl_m_pegar, P_EDITOR);
    limpia_mis();
}

/* ---- F7. Los textos nuevos caben en 52 columnas (con las etiquetas y los
 *      valores más largos, «R32 (1,234k)», y un nombre de 23) ---- */
static const char *const FL_TEXTOS[] = {
    "Falta cerrar un par" S_ee "ntesis.",
    "Sobra un par" S_ee "ntesis de cierre.",
    "La cuenta est" S_aa " a medias: falta un n" S_uu "mero.",
    "Falta un signo entre dos n" S_uu "meros: + - " S_TIMES " " S_DIVIDE " ^",
    "Divisi" S_oo "n entre cero.",
    "La cuenta no da un n" S_uu "mero real.",
    "La cuenta da un n" S_uu "mero demasiado grande.",
    "Un n" S_uu "mero de la cuenta est" S_aa " mal escrito.",
    "La cuenta da negativo y aqu" S_ii " no lleva signo.",
    "Cuentas: + - " S_TIMES " " S_DIVIDE " ^ ( ) x" S_SUP2 "   [enter] la guarda",
    "Aqu" S_ii " [+] y [-] son la parte j: sin cuentas.",
    "Copiado R32 (1,234k" S_OHM "): [ctrl][V] y una flecha.",
    "Cortado R32 (1,234k" S_OHM "): [ctrl][V] y una flecha.",
    "Pegar R32 (1,234k" S_OHM "): una flecha. [esc] no",
    "Elige primero un elemento: [tab] o [enter].",
    "Nada que pegar: [ctrl][C] copia el elegido.",
    "La copia, con sus valores: rev" S_ii "salos.",
    "Nombre: Abcdefghijklmnopqrstuvw_",
    "Letras, cifras y [espacio]; [shift] may" S_uu "scula.",
    "Escribe un nombre ([esc] lo deja como estaba).",
    "No caben m" S_aa "s: hasta 23 caracteres.",
    "[letras] nombre [del] borra [enter] guarda [esc] no",
    "[enter] abrir [N] nombre [del] borrar [esc] volver",
    "Guardado: Mi problema 20. [ctrl][N] le pone nombre.",
};

static void fl_textos(void)
{
    int i;
    char det[96];
    for (i = 0; i < (int)(sizeof FL_TEXTOS / sizeof FL_TEXTOS[0]); i++) {
        const char *s = FL_TEXTOS[i];
        strcpy(det, "hasta 52 columnas: ");
        strncat(det, s, 40);
        chk_cond("F7 textos", det, strlen(s) <= 52);
    }
}

static void test_m_flujo(void)
{
    printf("[72] Cuentas en la ficha, copiar y pegar, y nombres\n");
    fl_eval();
    fl_ficha();
    fl_pegar();
    fl_nombres();
    fl_capa();
    fl_indep();
    fl_textos();
}
