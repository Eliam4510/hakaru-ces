/* --------------------------------------------------------------------------
 *  54b. Revisión de la Etapa 3: la interfaz (hallazgos 13 a 28)
 *
 *  Cada bloque repite por teclas el escenario de un hallazgo de la revisión
 *  adversarial de la Etapa 3 y comprueba lo que se ve y lo que se guarda:
 *    13  la paleta (PAL_N piezas en filas de 4) no se sale de PAL[]
 *    15  [.] y [EE] en la parte j: «2,5+j1,5» y «1E3+j2E3»
 *    16  [=] en los ajustes de CA abiertos desde el editor usa la calculadora
 *    17  los rótulos del editor, de la ficha y de las miniaturas, en el
 *        convenio de SU circuito
 *    18  borrar, tapar o sustituir la última Z quita lo que es de la alterna
 *    19  el fp del vatímetro, en [-1, 1]
 *    20  -5 V a alterna y de vuelta: -5 V
 *    21  la P del vatímetro es «P(W1)» en la línea de estado
 *    22  [T] sin A y B avisa en la línea 5 de P_CA
 *    23  el eco del ángulo que se escribe va en grados
 *    24  [arr]/[ab] con un valor fuera de rango se quedan en la fila
 *    25  en el convenio seno, el ángulo vacío de una fuente se ve como 90
 *    26  las cajas de módulo y ángulo no se desbordan (el ángulo, 7 como mucho)
 *    27  la línea de estado de la calculadora de CA cabe en 52 columnas
 *    28  la línea 5 de P_CA con 4+ soluciones y cortada cabe en 52 columnas
 * ----------------------------------------------------------------------- */

/* ¿Algún píxel de este color en el rectángulo [x0, x1] x [y0, y1]? */
static int r3u_hay_color(int x0, int y0, int x1, int y1, u16 col)
{
    const u16 *fb = gfx_buffer();
    int x, y;
    for (y = y0; y <= y1; y++)
        for (x = x0; x <= x1; x++)
            if (x >= 0 && x < SCR_W && y >= 0 && y < SCR_H && fb[y * SCR_W + x] == col) return 1;
    return 0;
}

/* La fila del borde de arriba de una caja de este color que va de xa a xb
 * (la primera fila con esos píxeles seguidos), o -1 */
static int r3u_caja(int xa, int xb, u16 col)
{
    const u16 *fb = gfx_buffer();
    int x, y;
    for (y = 0; y < SCR_H; y++) {
        for (x = xa; x <= xb && fb[y * SCR_W + x] == col; x++) {}
        if (x > xb) return y;
    }
    return -1;
}

/* Un circuito armado a mano, abierto en el editor desde Mis problemas */
static int r3u_abre(const Circuito *c)
{
    limpia_mis();
    if (!arch_guardar(0, c)) return 0;
    ui_init();
    TECLAS(T_ENTER, T_ENTER, T_ABAJO, T_ABAJO, T_ENTER, T_ENTER);  /* portada, menú, Taller, Mis, abrir */
    return ui_pantalla() == P_EDITOR && ui_circuito()->nel == c->nel;
}

/* Los ajustes de CA desde el editor: [menu] y la opción 6 */
static void r3u_ajustes_ca(void)
{
    int j;
    TECLAS(T_MENU);
    for (j = 0; j < 6; j++) TECLAS(T_ABAJO);
    TECLAS(T_ENTER);
}

/* Espera a que acabe lo que se está calculando (la calculadora de CA) */
static void r3u_espera(void)
{
    int j;
    for (j = 0; j < 100000 && ui_pantalla() == P_EMULANDO; j++) ui_tick();
}

/* La línea de estado: una vuelta en serie por el borde de la rejilla con
 * E1 alterna (10 V a 60 Hz): nl L de 10 mH y luego R de 10 Ω (n en total).
 * inc de las L van con [?!] (incógnitas); dat R del final, con |V| de dato
 * (una ecuación cada una); req pedidos en las primeras R, alternando I [?!]
 * y ∠V [?!]; conf = 1: también la f con [?!]. */
static void r3u_cadena(Circuito *c, int nl, int inc, int dat, int req, int conf)
{
    int k, i, x, y, d, n = nl + dat + req, nr = 0;
    circ_init(c, "Estado");
    k = circ_elemento(c, EL_FUENTE, 0, 1, 3);                   /* (0,1) -> (0,0) */
    c->el[k].variante = FORMA_FASOR;
    c->el[k].val.v = 10.0; c->el[k].val.est = M_DATO;
    c->el[k].val.ang = 0.0; c->el[k].val.est_ang = M_DATO;
    c->frec.v = 60.0; c->frec.est = conf ? M_BUSCAR : M_DATO;
    if (n < 1) n = 1;
    for (i = 0; i < 19; i++) {
        if (i < 9) { x = i; y = 0; d = 0; }                     /* arriba, a la derecha */
        else if (i == 9) { x = 9; y = 0; d = 1; }               /* baja */
        else { x = 19 - i; y = 1; d = 2; }                      /* abajo, a la izquierda */
        if (i >= n) { circ_cable(c, x, y, d); continue; }
        if (i < nl) {
            k = circ_elemento(c, EL_BOB, x, y, d);
            c->el[k].val.v = 0.01;
            c->el[k].val.est = i < inc ? M_BUSCAR : M_DATO;
            continue;
        }
        k = circ_elemento(c, EL_RES, x, y, d);
        c->el[k].val.v = 10.0; c->el[k].val.est = M_DATO;
        if (nr < req) {
            if (nr % 2 == 0) c->el[k].I.est = M_BUSCAR;
            else c->el[k].V.est_ang = M_BUSCAR;
        } else { c->el[k].V.v = 1.0; c->el[k].V.est = M_DATO; }
        nr++;
    }
}

static void test_revision_e3_ui(void)
{
    const Circuito *cu = ui_circuito();
    static Circuito c;
    int k, j, n, d0;

    printf("[54b] Revisión de la Etapa 3: la interfaz\n");
    gfx_desbordes_reset();

    /* ---- 13. La paleta (PAL_N piezas en filas de 4) no se sale de PAL[]:
     * en la última fila, [der] no pasa de la última pieza y [enter] no lee
     * fuera. En función de PAL_N (§6.1 de la Etapa 4: vale con 20, 21 y 22
     * piezas): con E1 y W1 en la esquina y el cursor en (6,3), [ab] x6 se
     * queda en la última fila f = (PAL_N - 1)/4 y n [der] (n = 1..3), [enter]
     * y [→] ponen la pieza mín(4f + n, PAL_N - 1), que es válida, se guarda y
     * se abre. La selección de la paleta se recuerda: [izq] x3 la lleva antes
     * a la columna 0 ---- */
    for (j = 1; j <= 3; j++) {
        int f = (PAL_N - 1) / 4, pieza = 4 * f + j < PAL_N - 1 ? 4 * f + j : PAL_N - 1;
        int tp = ui_paleta_tipo(pieza);
        char q[96];
        ui_init();
        TECLAS(T_ENTER, T_ENTER, T_ENTER);
        TECLAS(T_LETRA('E'), T_ABAJO, T_IZQ, T_DIG(1), T_DIG(0), T_ENTER);     /* E1 (1,1)-(1,2) */
        TECLAS(T_LETRA('W'), T_DER, T_ESC);                                    /* (1,2)-(2,2) */
        n = cu->nel;
        TECLAS(T_CATALOGO, T_ABAJO, T_ABAJO, T_ABAJO, T_DER, T_ENTER, T_ARRIBA); /* W1 (2,2)-(2,1) */
        k = cu->nel - 1;
        if (j == 1)
            chk_cond("paleta", "[ab] x3 y [der] siguen dando el vat" S_ii "metro (la pieza 13): W1",
                     cu->nel == n + 1 && k >= 0 && cu->el[k].tipo == EL_VATI && strcmp(cu->el[k].etiq, "W1") == 0);
        if (ui_pantalla() == P_ELEMENTO) TECLAS(T_ESC);
        TECLAS(T_DER, T_DER, T_DER, T_DER, T_ABAJO, T_ABAJO);                  /* el cursor a (6,3) */
        n = cu->nel;
        TECLAS(T_CATALOGO, T_IZQ, T_IZQ, T_IZQ);
        repite(T_ABAJO, 6);
        repite(T_DER, j);
        TECLAS(T_ENTER, T_DER);
        if (ui_pantalla() == P_ELEMENTO) TECLAS(T_ESC);
        k = cu->nel - 1;
        sprintf(q, "[ab] x6 y %d [der] en la " S_uu "ltima fila: la pieza %d, un tipo que existe", j, pieza);
        chk_cond("paleta", q, tp >= 0 && tp < EL_N && cu->nel == n + 1 && k >= 0 && cu->el[k].tipo == tp);
        chk_cond("paleta", "el circuito es v" S_aa "lido (circ_valido)", circ_valido(cu));
        limpia_mis();
        chk_cond("paleta", "y se guarda y se vuelve a abrir", arch_guardar(0, cu) && arch_cargar(0, &c) &&
                 c.nel == cu->nel && k >= 0 && c.el[k].tipo == tp);
        limpia_mis();
    }
    {
        /* [izq] desde la última da la anterior (si la última no empieza su
         * fila); en un problema nuevo del Taller, que no olvida la selección */
        int f = (PAL_N - 1) / 4, ult = 4 * f + 3 < PAL_N - 1 ? 4 * f + 3 : PAL_N - 1;
        int esp = ult % 4 > 0 ? ult - 1 : ult;
        for (j = 0; j < 4 && ui_pantalla() != P_TALLER; j++) TECLAS(T_ESC);
        TECLAS(T_ARRIBA, T_ENTER);                                             /* Problema nuevo */
        TECLAS(T_CATALOGO, T_IZQ, T_ENTER, T_DER);
        if (ui_pantalla() == P_ELEMENTO) TECLAS(T_ESC);
        chk_cond("paleta", "[izq] desde la " S_uu "ltima pieza pone la anterior",
                 ui_paleta_tipo(esp) >= 0 && cu->nel == 1 && cu->el[0].tipo == ui_paleta_tipo(esp) && circ_valido(cu));
    }

    /* ---- 15. La parte j lleva su propia coma y su propio exponente ---- */
    ui_init();
    TECLAS(T_ENTER, T_ENTER, T_ENTER);
    TECLAS(T_LETRA('Z'), T_ARRIBA);
    k = cu->nel - 1;
    TECLAS(T_DIG(2), T_PUNTO, T_DIG(5), T_MAS, T_DIG(1), T_PUNTO, T_DIG(5));
    chk_cond("ficha Z", "2 [.] 5 [+] 1 [.] 5 se escribe «2,5+j1,5»", ve("2,5+j1,5_"));
    TECLAS(T_ENTER);
    chk_t("ficha Z", "2,5+j1,5: |Z| = 2,9155", k >= 0 ? cu->el[k].val.v : 0.0, hypot(2.5, 1.5), 1.0, 1e-12);
    chk_t("ficha Z", "2,5+j1,5: " S_ANG "Z = 30,96" S_DEG, k >= 0 ? cu->el[k].val.ang : 0.0,
          atan2(1.5, 2.5) * 180.0 / 3.14159265358979323846, 1.0, 1e-12);
    TECLAS(T_ENTER, T_DIG(1), T_EE, T_DIG(3), T_MAS, T_DIG(2), T_EE, T_DIG(3));
    chk_cond("ficha Z", "1 [EE] 3 [+] 2 [EE] 3 se escribe «1E3+j2E3»", ve("1E3+j2E3_"));
    TECLAS(T_ENTER);
    chk_t("ficha Z", "1E3+j2E3: |Z| = 2236", k >= 0 ? cu->el[k].val.v : 0.0, hypot(1e3, 2e3), 1e3, 1e-12);
    chk_t("ficha Z", "1E3+j2E3: " S_ANG "Z = 63,43" S_DEG, k >= 0 ? cu->el[k].val.ang : 0.0,
          atan2(2e3, 1e3) * 180.0 / 3.14159265358979323846, 1.0, 1e-12);
    TECLAS(T_ENTER, T_DIG(1), T_MAS, T_EE);
    chk_cond("ficha Z", "[EE] justo detrás de la j no entra («1+j»)", ve("1+j_") && !ve("1+jE"));
    TECLAS(T_ESC, T_ESC);

    /* ---- 16. [=] en los ajustes de CA abiertos desde el editor: con [?!],
     * la calculadora (como [=] en el editor) ---- */
    dibuja_l_voltimetro();
    chk_cond("ajustes CA", "el problema pide L1 y está listo", ve("Pides L1: listo"));
    r3u_ajustes_ca();
    chk_cond("ajustes CA", "[menu] y la opción 6: los ajustes de CA", ui_pantalla() == P_AJUSTES && ve("Ajustes de CA"));
    TECLAS(T_IGUAL);
    r3u_espera();
    chk_cond("ajustes CA", "[=] con [?!]: la calculadora halla L1 = 106,1 mH",
             ui_pantalla() == P_CA && ui_ca_sol() == 0 && ve("L1 = 106,1 mH"));
    chk_cond("ajustes CA", "y no el análisis directo («Para emular falta L»)", !ve("Para emular falta"));
    TECLAS(T_ESC);
    chk_cond("ajustes CA", "[esc] vuelve al editor", ui_pantalla() == P_EDITOR);

    /* ---- 17. Los rótulos de los fasores, en el convenio de SU circuito ---- */
    dibuja_t1();
    TECLAS(T_IGUAL);                                    /* P_CA en eficaces: 120∠0°V */
    chk_cond("convenio", "P_CA en eficaces: 120" S_ANG "0" S_DEG "V", ui_pantalla() == P_CA && ve("120" S_ANG "0" S_DEG "V"));
    TECLAS(T_ESC);
    r3u_ajustes_ca();
    TECLAS(T_ABAJO, T_DER, T_ENTER);                    /* de pico, sin pasar por P_CA */
    chk_cond("convenio", "el circuito queda de pico", ui_pantalla() == P_EDITOR && (cu->conv & CONV_PICO) != 0);
    chk_cond("convenio", "de pico: el editor rotula E1 como 170" S_ANG "0" S_DEG "V",
             ve("170" S_ANG "0" S_DEG "V") && !ve("120" S_ANG "0" S_DEG "V"));
    chk_cond("convenio", "la ficha de E1 enseña 169,7", abre_ficha_de("E1") && ve("169,7"));
    chk_cond("convenio", "y su esquema también rotula 170" S_ANG "0" S_DEG "V",
             ve("170" S_ANG "0" S_DEG "V") && !ve("120" S_ANG "0" S_DEG "V"));
    TECLAS(T_ESC);
    TECLAS(T_IGUAL);                                    /* P_CA de pico */
    chk_cond("convenio", "P_CA de pico: 170" S_ANG "0" S_DEG "V", ui_pantalla() == P_CA && ve("170" S_ANG "0" S_DEG "V"));
    TECLAS(T_ESC);
    r3u_ajustes_ca();
    TECLAS(T_ABAJO, T_IZQ, T_ENTER);                    /* de vuelta a eficaces */
    chk_cond("convenio", "de vuelta a eficaces: el editor rotula 120" S_ANG "0" S_DEG "V",
             ui_pantalla() == P_EDITOR && cu->conv == 0 && ve("120" S_ANG "0" S_DEG "V") && !ve("170" S_ANG "0" S_DEG "V"));
    chk_cond("convenio", "y la ficha de E1, 120 y no 169,7", abre_ficha_de("E1") && ve("120") && !ve("169,7") &&
             ve("120" S_ANG "0" S_DEG "V") && !ve("170" S_ANG "0" S_DEG "V"));
    TECLAS(T_ESC);
    /* la miniatura de Mis problemas: el mismo T1 guardado de pico, con la
     * última P_CA en eficaces */
    c = *cu;
    c.conv = CONV_PICO;
    limpia_mis();
    chk_cond("convenio", "el T1 de pico se guarda", arch_guardar(0, &c));
    TECLAS(T_IGUAL, T_ESC);                             /* la última P_CA, en eficaces */
    for (j = 0; j < 4 && ui_pantalla() == P_EDITOR; j++) TECLAS(T_ESC);
    TECLAS(T_ABAJO, T_ENTER);
    chk_cond("convenio", "Mis problemas: la miniatura del T1 de pico rotula 170" S_ANG "0" S_DEG "V",
             ui_pantalla() == P_MIS && ve("170" S_ANG "0" S_DEG "V") && !ve("120" S_ANG "0" S_DEG "V"));
    TECLAS(T_ESC);
    limpia_mis();

    /* ---- 18. Borrar, tapar con cable o sustituir la última Z: el circuito
     * deja de ser de CA y se quitan los ángulos (que en CC no se ven) ---- */
    for (j = 0; j < 3; j++) {
        static const char *const QUE[3] = { "borrar la Z", "taparla con cable", "poner una R encima" };
        char q[64];
        sprintf(q, "sin la Z (%s)", QUE[j]);
        ui_init();
        TECLAS(T_ENTER, T_ENTER, T_ENTER);
        TECLAS(T_LETRA('E'), T_ABAJO, T_DIG(1), T_DIG(2), T_ENTER);             /* E1 continua de 12 V */
        TECLAS(T_LETRA('W'), T_DER, T_DER, T_ESC);
        TECLAS(T_LETRA('Z'), T_ARRIBA, T_DIG(3), T_DIG(0), T_MAS, T_DIG(4), T_DIG(0), T_ENTER);
        TECLAS(T_LETRA('R'), T_IZQ, T_DIG(1), T_DIG(0), T_ABAJO, T_DIG(7), T_DIG(2), T_TAB, T_DIG(3), T_DIG(0), T_ENTER);
        TECLAS(T_LETRA('W'), T_IZQ, T_ESC);
        k = el_de_etiqueta(cu, "R1");
        chk_cond(q, "antes: CA y V(R1) = 72" S_ANG "30", circ_alterna(cu) && k >= 0 && cu->el[k].V.est == M_DATO &&
                 cu->el[k].V.est_ang == M_DATO && cu->el[k].V.ang == 30.0);
        TECLAS(T_DER, T_DER);                           /* arriba de Z1 */
        if (j == 0) TECLAS(T_BORRAR, T_ABAJO);
        else if (j == 1) TECLAS(T_LETRA('W'), T_ABAJO);
        else TECLAS(T_LETRA('R'), T_ABAJO, T_ENTER);
        k = el_de_etiqueta(cu, "R1");
        chk_cond(q, "ya no es CA y no queda ninguna Z", !circ_alterna(cu) && el_de_etiqueta(cu, "Z1") < 0);
        chk_cond(q, "V(R1) conserva el 72 pero pierde el ángulo", k >= 0 && cu->el[k].V.est == M_DATO &&
                 cu->el[k].V.v == 72.0 && cu->el[k].V.est_ang == M_LIBRE && cu->el[k].V.ang == 0.0);
        chk_cond(q, "el circuito se puede guardar y abrir (circ_valido)", circ_valido(cu));
        /* con [del] o [W] la acción sigue: su línea tapa el aviso, y [esc] lo borra */
        if (j == 2) chk_cond(q, "el editor avisa «Ya no es CA»", ui_pantalla() == P_EDITOR && ve("Ya no es CA"));
        TECLAS(T_ESC);
        if (j == 2) {
            TECLAS(T_IGUAL);
            chk_cond(q, "[=] ya no dice que V(R1) lleva ángulo", !ve("lleva " S_aa "ngulo"));
            TECLAS(T_ESC);
        }
    }

    /* ---- 19 y 21. El vatímetro: su fp, en [-1, 1]; su P, «P(W1)» ---- */
    ui_init();
    TECLAS(T_ENTER, T_ENTER, T_ENTER);
    TECLAS(T_LETRA('E'), T_ABAJO, T_IZQ, T_DIG(1), T_DIG(2), T_DIG(0), T_ENTER);
    TECLAS(T_LETRA('W'), T_DER, T_DER, T_ESC);
    TECLAS(T_CATALOGO, T_ABAJO, T_ABAJO, T_ABAJO, T_DER, T_ENTER, T_ARRIBA);
    k = cu->nel - 1;
    chk_cond("vatímetro", "W1 puesto y su ficha abierta", k >= 0 && cu->el[k].tipo == EL_VATI && ui_pantalla() == P_ELEMENTO);
    TECLAS(T_ABAJO, T_DIG(5), T_ENTER);
    chk_cond("vatímetro", "fp = 5: fuera de rango, no se guarda", k >= 0 && cu->el[k].aux.est != M_DATO &&
             ui_pantalla() == P_ELEMENTO && ve("Fuera de rango: entre -1 y 1"));
    chk_cond("vatímetro", "el circuito sigue siendo válido (se podrá abrir)", circ_valido(cu));
    TECLAS(T_SIGNO, T_DIG(7), T_ENTER);
    chk_cond("vatímetro", "fp = -7: tampoco", k >= 0 && cu->el[k].aux.est != M_DATO && ui_pantalla() == P_ELEMENTO);
    TECLAS(T_DIG(0), T_PUNTO, T_DIG(6), T_ENTER);
    chk_cond("vatímetro", "fp = 0,6 sí", k >= 0 && cu->el[k].aux.est == M_DATO && cu->el[k].aux.v == 0.6 &&
             ui_pantalla() == P_EDITOR);
    TECLAS(T_ENTER);
    if (ui_pantalla() != P_ELEMENTO) TECLAS(T_ARRIBA);
    TECLAS(T_INCOG, T_ENTER);
    chk_cond("vatímetro", "P [?!]: la línea de estado dice «Pides P(W1)»", k >= 0 && cu->el[k].val.est == M_BUSCAR &&
             ve("Pides P(W1)") && !ve("Pides W1"));

    /* ---- 20. -5 V a alterna es 5∠180°, y de vuelta a continua, -5 V ---- */
    ui_init();
    TECLAS(T_ENTER, T_ENTER, T_ENTER);
    TECLAS(T_LETRA('E'), T_ABAJO, T_SIGNO, T_DIG(5), T_ENTER);
    k = el_de_etiqueta(cu, "E1");
    chk_cond("E negativa", "E1 = -5 V continua", k >= 0 && cu->el[k].variante == ONDA_CC && cu->el[k].val.v == -5.0);
    chk_cond("E negativa", "su ficha", abre_ficha_de("E1"));
    TECLAS(T_IZQ);
    chk_cond("E negativa", "[izq] a alterna: 5" S_ANG "180" S_DEG, k >= 0 && cu->el[k].variante == FORMA_FASOR &&
             cu->el[k].val.v == 5.0 && cu->el[k].val.est_ang == M_DATO && fabs(cu->el[k].val.ang - 180.0) < 1e-9);
    TECLAS(T_DER);
    chk_cond("E negativa", "[der] de vuelta a continua: -5 V otra vez", k >= 0 && cu->el[k].variante == ONDA_CC &&
             cu->el[k].val.v == -5.0 && cu->el[k].val.est_ang == M_LIBRE && cu->el[k].val.ang == 0.0);
    TECLAS(T_ENTER);
    chk_cond("E negativa", "el editor rotula -5 V", ui_pantalla() == P_EDITOR && ve("-5"));

    /* ---- 22. [T] sin terminales en P_CA: el aviso, en P_CA ---- */
    dibuja_t1();
    TECLAS(T_IGUAL);
    chk_cond("P_CA [T]", "sin A ni B, P_CA no lo pide aún", ui_pantalla() == P_CA && !ve("Pon los terminales"));
    TECLAS(T_LETRA('T'));
    chk_cond("P_CA [T]", "[T] sin A y B: el aviso se ve en P_CA", ui_pantalla() == P_CA && ve("Pon los terminales A y B"));
    TECLAS(T_DER);
    chk_cond("P_CA [T]", "la nota dura hasta la tecla siguiente", ui_pantalla() == P_CA && !ve("Pon los terminales"));
    TECLAS(T_IZQ, T_LETRA('T'), T_ESC);
    chk_cond("P_CA [T]", "y no se queda para el editor", ui_pantalla() == P_EDITOR && !ve("Pon los terminales"));

    /* ---- 23. El eco del ángulo que se escribe: grados, no la unidad del módulo ---- */
    ui_init();
    TECLAS(T_ENTER, T_ENTER, T_ENTER);
    TECLAS(T_LETRA('E'), T_ABAJO, T_IZQ, T_DIG(1), T_DIG(2), T_DIG(0), T_TAB, T_DIG(3), T_DIG(0));
    chk_cond("eco", "E1: el ángulo que se escribe, «= 30" S_DEG "»", ve("= 30" S_DEG) && !ve("= 30 V"));
    TECLAS(T_ENTER, T_LETRA('W'), T_DER, T_DER, T_ESC, T_LETRA('Z'), T_ARRIBA, T_DIG(5), T_DIG(0), T_TAB,
           T_SIGNO, T_DIG(3), T_DIG(6), T_PUNTO, T_DIG(8), T_DIG(7));
    chk_cond("eco", "Z1: «= -36,87" S_DEG "», no en " S_OHM, ve("= -36,87" S_DEG) && !ve("= -36,87 " S_OHM));
    TECLAS(T_ENTER);
    k = cu->nel - 1;
    chk_cond("eco", "y se guarda 50" S_ANG "-36,87" S_DEG, k >= 0 && cu->el[k].tipo == EL_IMP && cu->el[k].val.v == 50.0 &&
             fabs(cu->el[k].val.ang + 36.87) < 1e-9);

    /* ---- 24. [arr] y [ab] con un valor fuera de rango se quedan en la fila ---- */
    ui_init();
    TECLAS(T_ENTER, T_ENTER, T_ENTER);
    TECLAS(T_LETRA('Z'), T_ARRIBA, T_DIG(3), T_DIG(0), T_TAB, T_DIG(9), T_DIG(5), T_ABAJO);
    k = cu->nel - 1;
    chk_cond("ficha Z", "95 [ab]: se queda en la fila de la Z, con su aviso (de -90 a 90)",
             ve("va de -90 a 90") && !ve("va de -360 a 360"));
    TECLAS(T_DIG(4), T_DIG(5), T_ABAJO);
    chk_cond("ficha Z", "45 [ab]: se guarda 30" S_ANG "45 y baja", k >= 0 && cu->el[k].val.v == 30.0 && cu->el[k].val.ang == 45.0 &&
             !ve("va de -90 a 90"));
    TECLAS(T_TAB, T_DIG(4), T_DIG(0), T_DIG(0), T_ARRIBA);
    chk_cond("ficha Z", "V con 400 [arr]: se queda en la fila de V, con su aviso (de -360 a 360)",
             ve("va de -360 a 360") && !ve("va de -90 a 90") && k >= 0 && cu->el[k].val.ang == 45.0);
    TECLAS(T_BORRAR, T_BORRAR, T_BORRAR, T_ARRIBA);
    chk_cond("ficha Z", "ya sin el 400, [arr] sube", !ve("Fuera de rango") && k >= 0 && cu->el[k].V.est_ang == M_LIBRE);
    TECLAS(T_ESC);

    /* ---- 25. Convenio seno: el ángulo vacío de una fuente vale 90° ---- */
    dibuja_t1();
    r3u_ajustes_ca();
    TECLAS(T_ABAJO, T_ABAJO, T_DER, T_ENTER);
    chk_cond("seno", "el circuito queda en seno", ui_pantalla() == P_EDITOR && (cu->conv & CONV_SENO) != 0);
    chk_cond("seno", "la ficha de E1", abre_ficha_de("E1"));
    TECLAS(T_TAB, T_BORRAR, T_TAB);
    k = el_de_etiqueta(cu, "E1");
    chk_cond("seno", "E1 sin ángulo", k >= 0 && cu->el[k].val.est == M_DATO && cu->el[k].val.est_ang == M_LIBRE);
    chk_cond("seno", "la caja del ángulo vacío enseña 90", ve("\n90\n"));
    chk_cond("seno", "y el rótulo, «dato (" S_ANG "90)»", ve("dato (" S_ANG "90)") && !ve("dato (" S_ANG "0)"));
    TECLAS(T_ESC, T_IGUAL);
    chk_cond("seno", "P_CA: 120" S_ANG "90" S_DEG "V", ui_pantalla() == P_CA && ve("120" S_ANG "90" S_DEG "V"));
    TECLAS(T_ESC);

    /* ---- 26. Las cajas de módulo y ángulo: lo escrito no sale de ellas ---- */
    ui_init();
    TECLAS(T_ENTER, T_ENTER, T_ENTER);
    TECLAS(T_LETRA('Z'), T_ARRIBA, T_DIG(1), T_DIG(2), T_DIG(3), T_DIG(4), T_DIG(5), T_MAS,
           T_DIG(6), T_DIG(7), T_DIG(8), T_DIG(9), T_DIG(0));
    ui_dibuja();
    n = r3u_caja(24, 95, C_CURSOR);
    /* no cabe entero (13 de 11): «« y su final, por caracteres enteros */
    chk_cond("caja del módulo", "se ve la caja del módulo elegida", n > 0 && ve("\n" S_LAQUO "\n45+j67890_\n"));
    ui_dibuja();
    chk_cond("caja del módulo", "«12345+j67890_» (78 px) no pinta a la izquierda de la caja",
             n > 0 && !r3u_hay_color(0, n, 23, n + 13, C_CURSOR));
    TECLAS(T_ESC, T_ESC);
    ui_init();
    TECLAS(T_ENTER, T_ENTER, T_ENTER);
    TECLAS(T_LETRA('E'), T_ABAJO, T_IZQ, T_DIG(1), T_DIG(2), T_DIG(0), T_TAB,
           T_SIGNO, T_DIG(1), T_DIG(2), T_DIG(6), T_PUNTO, T_DIG(8), T_DIG(7), T_DIG(5));
    /* en la caja, «« y el final («6,87_»: 6 de 8); el 5, que sería el 8.º, no entra */
    chk_cond("caja del ángulo", "7 caracteres como mucho: «-126,87»", ve("\n" S_LAQUO "\n6,87_\n") && !ve("6,875"));
    ui_dibuja();
    n = r3u_caja(106, 149, C_CURSOR);
    chk_cond("caja del ángulo", "«-126,87_» (48 px) no pisa el " S_ANG " ni la caja del módulo",
             n > 0 && !r3u_hay_color(96, n, 105, n + 13, C_CURSOR));
    TECLAS(T_ENTER);
    k = el_de_etiqueta(cu, "E1");
    chk_cond("caja del ángulo", "se guarda -126,87", k >= 0 && fabs(cu->el[k].val.ang + 126.87) < 1e-9);

    /* ---- 27. La línea de estado de la calculadora de CA, en 52 columnas:
     * sobra, sobran, solo se comprueba, falta, faltan, más de 4 y 8, con
     * listas de pedidos cada vez más largas (etiquetas de dos cifras, ∠) ---- */
    {
        /* nl L (inc con [?!]), dat datos, f con [?!] (conf), y desde req0 pedidos */
        static const struct { int nl, inc, dat, conf, req0; const char *cola; } CASO[] = {
            { 1, 1, 1, 0, 0, ": listo " S_CHECK " pulsa [=] (CA)" },
            { 1, 1, 2, 0, 0, ": sobra 1 (se comprueba)" },
            { 1, 1, 4, 0, 0, ": sobran 3 (se comprueban)" },
            { 0, 0, 1, 0, 1, ": solo se comprueba" },
            { 0, 0, 3, 0, 1, ": solo se comprueba" },
            { 1, 1, 0, 0, 0, ": falta 1 dato (CA)" },
            { 2, 2, 0, 0, 0, ": faltan 2 datos (CA)" },
            { 3, 3, 0, 1, 0, ": faltan 4 datos (CA)" },
            { 5, 5, 0, 0, 0, ": m" S_aa "s de 4 inc" S_oo "gnitas" },
            { 1, 1, 9, 0, 0, ": m" S_aa "s de 8 datos" },
            { 1, 1, 2, 1, 0, ": listo " S_CHECK " pulsa [=] (CA)" },
        };
        int ca, req, malos = 0, cortes = 0, colas = 0, total = 0;
        gfx_desbordes_silencio(1);                  /* se cuentan aquí, sin llenar la consola */
        for (ca = 0; ca < (int)(sizeof CASO / sizeof CASO[0]); ca++)
            for (req = CASO[ca].req0; CASO[ca].nl + CASO[ca].dat + req <= 19 && req <= 14; req++) {
                r3u_cadena(&c, CASO[ca].nl, CASO[ca].inc, CASO[ca].dat, req, CASO[ca].conf);
                d0 = gfx_desbordes();
                if (!r3u_abre(&c)) { malos++; continue; }
                total++;
                if (!ve(CASO[ca].cola) || !ve("Pides ")) {
                    colas++;
                    if (colas <= 3) printf("    caso %d con %d pedidos: no se ve «%s»\n", ca, req, CASO[ca].cola);
                }
                if (ve("...: ")) cortes++;
                if (gfx_desbordes() != d0) {
                    malos++;
                    if (malos <= 3) printf("    caso %d con %d pedidos: la línea de estado pasa de 52 columnas\n", ca, req);
                }
            }
        gfx_desbordes_silencio(0);
        limpia_mis();
        chk_cond("línea de estado CA", "los circuitos se abren y ninguna línea pasa de 52 columnas", malos == 0 && total > 100);
        chk_cond("línea de estado CA", "cada caso dice lo suyo entero (sobra, sobran, solo se comprueba...)", colas == 0);
        chk_cond("línea de estado CA", "con muchos pedidos, la lista se corta con «...»", cortes >= 20);
    }
    /* la del informe, por teclas: V(R1) [?!], L1 [?!], ∠V(R2) [?!], I(C1) [?!] y I(R3) = 2 */
    ui_init();
    TECLAS(T_ENTER, T_ENTER, T_ENTER);
    TECLAS(T_LETRA('E'), T_ABAJO, T_IZQ, T_DIG(1), T_DIG(2), T_DIG(0), T_ENTER);
    TECLAS(T_LETRA('R'), T_DER, T_DIG(1), T_DIG(0), T_ABAJO, T_INCOG, T_ENTER);
    TECLAS(T_LETRA('L'), T_DER, T_INCOG, T_ENTER);
    TECLAS(T_LETRA('R'), T_ARRIBA, T_ABAJO, T_DIG(5), T_DIG(0), T_TAB, T_INCOG, T_ENTER);
    TECLAS(T_LETRA('C'), T_IZQ, T_DIG(1), T_DIG(0), T_DIG(0), T_LETRA('U'), T_ABAJO, T_ABAJO, T_ABAJO, T_INCOG, T_ENTER);
    TECLAS(T_LETRA('R'), T_IZQ, T_ABAJO, T_ABAJO, T_DIG(2), T_ENTER);
    d0 = gfx_desbordes();
    chk_cond("línea de estado CA", "la del informe: «...: sobra 1 (se comprueba)» entera",
             ve("Pides V(R1),L1,") && ve("sobra 1 (se comprueba)"));
    chk_cond("línea de estado CA", "y en 52 columnas", gfx_desbordes() == d0);

    /* ---- 28. La línea 5 de P_CA con 4 (o más) soluciones y cortada ---- */
    {
        int x, b;
        circ_init(&c, "Ramas");
        k = circ_elemento(&c, EL_FUENTE, 0, 1, 3);                /* E1 = 10 V: (0,1) -> (0,0) */
        c.el[k].variante = FORMA_FASOR;
        c.el[k].val.v = 10.0; c.el[k].val.est = M_DATO; c.el[k].val.ang = 0.0; c.el[k].val.est_ang = M_DATO;
        c.frec.v = 60.0; c.frec.est = M_DATO;
        k = circ_elemento(&c, EL_FUENTE, 0, 2, 3);                /* E2 = 1 V en serie: sin «E1: I = ...» */
        c.el[k].variante = FORMA_FASOR;
        c.el[k].val.v = 1.0; c.el[k].val.est = M_DATO; c.el[k].val.ang = 0.0; c.el[k].val.est_ang = M_DATO;
        circ_cable(&c, 0, 2, 1);
        for (x = 0; x < 8; x++) { circ_cable(&c, x, 0, 0); circ_cable(&c, x, 3, 0); }
        for (b = 0; b < 3; b++) {                                 /* R 10 + (L [?!] || C 100 uF), |I(R)| = 0,2425 */
            x = 2 + 3 * b;
            k = circ_elemento(&c, EL_RES, x, 0, 1);
            c.el[k].val.v = 10.0; c.el[k].val.est = M_DATO; c.el[k].I.v = 0.2425; c.el[k].I.est = M_DATO;
            k = circ_elemento(&c, EL_BOB, x, 1, 1); c.el[k].val.est = M_BUSCAR;
            k = circ_elemento(&c, EL_COND, x + 1, 1, 1); c.el[k].val.v = 100e-6; c.el[k].val.est = M_DATO;
            circ_cable(&c, x, 1, 0); circ_cable(&c, x, 2, 0); circ_cable(&c, x, 2, 1);
        }
        chk_cond("P_CA línea 5", "las tres ramas se abren en el editor", r3u_abre(&c));
        TECLAS(T_IGUAL);
        for (j = 0; j < 100000 && ui_pantalla() == P_EMULANDO && !cal_res()->mas; j++) ui_tick();
        chk_cond("P_CA línea 5", "hay más de 4 soluciones antes de acabar", ui_pantalla() == P_EMULANDO && cal_res()->mas);
        TECLAS(T_ESC);                                            /* se corta */
        chk_cond("P_CA línea 5", "[esc] corta: P_CA con 4 soluciones", ui_pantalla() == P_CA &&
                 cal_res()->estado == CAL_CORTADO && cal_res()->nsol == 4 && cal_res()->mas);
        d0 = gfx_desbordes();
        chk_cond("P_CA línea 5", "«Hay 4+ soluciones; la 1 [arr/ab] (cortado)»", ve("Hay 4+ soluciones; la 1 [arr/ab] (cortado)"));
        chk_cond("P_CA línea 5", "cortada, cabe en 52 columnas", gfx_desbordes() == d0);
        TECLAS(T_ABAJO, T_ABAJO, T_ABAJO);
        chk_cond("P_CA línea 5", "[ab] x3: la 4, y cabe", ve("Hay 4+ soluciones; la 4 [arr/ab] (cortado)") && gfx_desbordes() == d0);
        TECLAS(T_IGUAL);
        r3u_espera();
        chk_cond("P_CA línea 5", "[=] sin cortar: «Hay 4+ soluciones; esta es la 1 [arr/ab]»", ui_pantalla() == P_CA &&
                 cal_res()->estado == CAL_OK && ve("Hay 4+ soluciones; esta es la 1 [arr/ab]") && gfx_desbordes() == d0);
        TECLAS(T_ESC);
        limpia_mis();
    }

    chk_cond("revisión E3 interfaz", "sin textos fuera de la pantalla", gfx_desbordes() == 0);
}
