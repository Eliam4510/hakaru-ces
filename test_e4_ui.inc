/* --------------------------------------------------------------------------
 *  65. Etapa 4: la interfaz del editor (paso 11, §4.1 a §4.4 y §6.12)
 *
 *  Por guion de teclas, como un alumno: la ficha del diodo (sus modelos, el
 *  eco del catálogo y [catálogo]), la controlada (la fila x: [+][-], [izq],
 *  [del], las cifras), el AO (las flechas que caben, colocarlo, los cables y
 *  las marcas contra su caja, la marca de lo que estorba, el espejo), el menú
 *  de 11 opciones, la paleta de PAL_N piezas (y sus iconos dentro de su
 *  celda) y la línea de estado de los circuitos de la Etapa 4. Los textos
 *  nuevos caben en 52 columnas y nada se sale de la pantalla.
 *
 *  Se incluye tras test_e4_paso.inc (usa chk_cond, ve, TECLAS, repite,
 *  limpia_mis, lineas, el_de_etiqueta y r3u_abre y r3u_hay_color de [54b]).
 *  No vuelca BMP: las de después conservan su número.
 * ----------------------------------------------------------------------- */

/* La pantalla de un punto de la rejilla del editor (y de la ficha) */
static void u4_pt(int gx, int gy, int *sx, int *sy)
{
    Marco m;
    esq_marco_editor(&m);
    esq_pt(&m, gx, gy, sx, sy);
}

/* ¿Algún píxel de ese color alrededor (± r) del punto (gx, gy)? Redibuja */
static int u4_color_en(int gx, int gy, int r, u16 col)
{
    int sx, sy;
    ui_dibuja();
    u4_pt(gx, gy, &sx, &sy);
    return r3u_hay_color(sx - r, sy - r, sx + r, sy + r, col);
}

/* ¿La flecha del cursor hacia d, en ese color? (las de esq_flechas_cursor) */
static int u4_flecha(int gx, int gy, int d, u16 col)
{
    int sx, sy;
    ui_dibuja();
    u4_pt(gx, gy, &sx, &sy);
    switch (d) {
    case DIR_DER: return r3u_hay_color(sx + 8, sy - 3, sx + 11, sy + 3, col);
    case DIR_ABA: return r3u_hay_color(sx - 3, sy + 8, sx + 3, sy + 11, col);
    case DIR_IZQ: return r3u_hay_color(sx - 11, sy - 3, sx - 8, sy + 3, col);
    default:      return r3u_hay_color(sx - 3, sy - 11, sx + 3, sy - 8, col);
    }
}

static void u4_dato(Mag *m, double v) { m->v = v; m->est = M_DATO; }

/* E1 (5 V, + arriba) - R1 (1 kΩ) - D1 (con Vγ) en serie, en el rincón:
 * E1 (1,2)->(1,1), R1 (1,1)->(2,1), D1 (2,1)->(3,1) y los cables de vuelta */
static void u4_serie(Circuito *c)
{
    int k;
    circ_init(c, "Serie con D1");
    k = circ_elemento(c, EL_FUENTE, 1, 2, DIR_ARR); u4_dato(&c->el[k].val, 5.0);
    k = circ_elemento(c, EL_RES, 1, 1, DIR_DER);    u4_dato(&c->el[k].val, 1e3);
    circ_elemento(c, EL_DIODO, 2, 1, DIR_DER);
    circ_cable(c, 3, 1, DIR_ABA); circ_cable(c, 3, 2, DIR_IZQ); circ_cable(c, 2, 2, DIR_IZQ);
}

/* E1 (0,1)->(0,0), R1 (0,0)->(1,0), la controlada k (1,0)->(2,0) o, con dos,
 * J1 (1,0)->(2,0) por la V de R1 y E2 (2,0)->(3,0) por la I de J1 */
static void u4_controladas(Circuito *c, int dos)
{
    int k;
    circ_init(c, dos ? "Control por J1" : "E2 sin control");
    k = circ_elemento(c, EL_FUENTE, 0, 1, DIR_ARR); u4_dato(&c->el[k].val, 5.0);
    k = circ_elemento(c, EL_RES, 0, 0, DIR_DER);    u4_dato(&c->el[k].val, 1e3);
    if (!dos) {
        circ_elemento(c, EL_EDEP, 1, 0, DIR_DER);   /* E2: sin control (ref = -1) */
        c->el[2].variante = DEP_V; u4_dato(&c->el[2].val, 2.0);
        circ_cable(c, 2, 0, DIR_ABA); circ_cable(c, 2, 1, DIR_IZQ); circ_cable(c, 1, 1, DIR_IZQ);
        return;
    }
    k = circ_elemento(c, EL_JDEP, 1, 0, DIR_DER);   /* J1 = g·V(R1) */
    c->el[k].variante = DEP_V; c->el[k].ref = 1; u4_dato(&c->el[k].val, 1e-3);
    k = circ_elemento(c, EL_EDEP, 2, 0, DIR_DER);   /* E2 = r·I(J1): CTL_I_NO */
    c->el[k].variante = DEP_I; c->el[k].ref = 2; u4_dato(&c->el[k].val, 1e3);
    circ_cable(c, 3, 0, DIR_ABA); circ_cable(c, 3, 1, DIR_IZQ); circ_cable(c, 2, 1, DIR_IZQ); circ_cable(c, 1, 1, DIR_IZQ);
}

/* Los textos nuevos del editor, de la ficha y de la línea de estado, con los
 * nombres más largos que pueden salir (un AO no pasa de U7: su caja es de 3x3
 * y las cajas no se tocan) */
static const char *const U4_TEXTOS[] = {
    "Con D32 no se despeja: [I] barre y [N] lee",
    "E32 no tiene control: el" S_ii "gelo en su ficha.",
    "El control de E32 no vale: mira su ficha.",
    "U7 sin referencia: pon la tierra con [G].",
    "Para el punto de operaci" S_oo "n falta V" S_GAMMA " en D32.",
    "Para el punto de operaci" S_oo "n falta un valor en D32.",
    "D32: su modelo no cuadra: [=] dice por qu" S_ee ".",
    "Algo no cuadra en D32: [=] dice qu" S_ee ".",
    "Listo: [=] da el punto de operaci" S_oo "n",
    "Listo: pulsa [=] para emular (canales auto)",
    "La pr" S_aa "ctica tapada es de CC lineal.",
    "Nada que barrer: pon una E, una J o una R.",
    "[+][-] elige qui" S_ee "n la manda (en naranja).",
    "Se enciende en el resultado: [=].",
    "Sin [G], la salida de U7 va al - de E32.",
    "Sin [G] ni fuentes, U7 no tiene referencia: pon [G].",
    "No cabe hacia " S_RARR ": se sale de la rejilla.",
    "No cabe hacia " S_RARR ": estorba D32.",
    "No cabe hacia " S_RARR ": estorba un cable.",
    "No cabe hacia " S_RARR ": estorba la tierra.",
    "No cabe hacia " S_RARR ": estorba el terminal A.",
    "No cabe hacia " S_RARR ": estorba la sonda 1.",
    "Ah" S_ii " est" S_aa " U7: los cables van a sus patas.",
    "Ese punto est" S_aa " dentro de U7: no es una pata.",
    "Dentro de U7 no hay borne: ponla en una pata.",
    "Sin sondas en las patas: [O] en el punto o un A.",
    "Espejo: + y - de U7 cambian de sitio.",
    "Espejo: C y E de Q9 cambian de sitio.",
    "Espejo: D y S de M9 cambian de sitio.",
    "Aqu" S_ii ": dentro de U7 (no es una pata)",
    "No hay otro elemento de dos bornes.",
    "El control se elige con [+][-].",
    "El control no se despeja: el" S_ii "gelo con [+][-].",
    "[+][-] elige  [del] ninguno  [cat" S_aa "logo]=modelo",
    "[arr/ab] campo [+][-] control [izq/der] tipo [=]",
    "[arr/ab] campo [izq/der] modelo [F] espejo [=]",
    "[arr/ab] campo [izq/der] modelo [F] girar [=]",
    "[arr/ab] campo [izq/der] canal [F] espejo [=]",
    "1N4148: Is 2,52 nA " S_CDOT " n 1,752 " S_CDOT " Rs 0,568 " S_OHM,
    "1N4007: Is 7,03 nA " S_CDOT " n 1,808 " S_CDOT " Rs 34 m" S_OHM,
    "1N4733A: Vz 5,1 V a 49 mA " S_CDOT " rz 7 " S_OHM,
    "Amarillo: Vf 1,85 V a 100 mA (n 2, Rs 5 " S_OHM ")",
    "Ideal: sin corriente de entrada; en lineal Vd = 0",
    "PNP: " S_BETA " 1000 " S_CDOT " Is 10,5 fA " S_CDOT " VA 1,5 kV",
    "Canal P: Vt 2,5 V " S_CDOT " K 1,25 mA/V" S_SUP2,
    "Barrido de CC: curvas        [I]",
    "Punto de operaci" S_oo "n de CC     [P]",
};

static void test_e4_ui(void)
{
    const Circuito *cu = ui_circuito();
    static Circuito c;
    const char *q;
    int k, j, n, sx, sy;

    printf("[65] Etapa 4: la interfaz del editor\n");
    gfx_desbordes_reset();

    /* ---- Los textos nuevos, en una línea de 52 columnas ---- */
    for (j = 0; j < (int)(sizeof U4_TEXTOS / sizeof U4_TEXTOS[0]); j++) {
        pruebas++;
        if (strlen(U4_TEXTOS[j]) > 52 || lineas(U4_TEXTOS[j], 52) != 1) {
            falla("textos E4", U4_TEXTOS[j], (double)strlen(U4_TEXTOS[j]), 52);
        }
    }

    /* ---- Las teclas: D y U ponen el diodo y el AO; Q y M, en los pasos 13 y 14 ---- */
    chk_cond("teclas", "[D] diodo y [U] amp. op.", TIPOS[EL_DIODO].tecla == 'D' && TIPOS[EL_AO].tecla == 'U');
    chk_cond("teclas", "zener, LED y controladas, sin tecla (solo la paleta)", TIPOS[EL_ZENER].tecla == 0 &&
             TIPOS[EL_LED].tecla == 0 && TIPOS[EL_EDEP].tecla == 0 && TIPOS[EL_JDEP].tecla == 0);

    /* ---- Ficha del diodo: sus modelos con [izq/der], el eco del catálogo y
     *      [catálogo] a la página Modelo ---- */
    q = "ficha del diodo";
    ui_init();
    TECLAS(T_ENTER, T_ENTER, T_ENTER);                          /* editor nuevo: el cursor en (1,1) */
    TECLAS(T_LETRA('D'), T_DER);
    chk_cond(q, "[D][der] pone D1 con V" S_GAMMA " y abre su ficha", cu->nel == 1 && cu->el[0].tipo == EL_DIODO &&
             strcmp(cu->el[0].etiq, "D1") == 0 && cu->el[0].variante == DIO_VG && ui_pantalla() == P_ELEMENTO &&
             ve("con V" S_GAMMA));
    chk_cond(q, "(1,1) -> (2,1): el " S_aa "nodo en a", cu->el[0].xa == 1 && cu->el[0].ya == 1 && cu->el[0].xb == 2 && cu->el[0].yb == 1);
    chk_cond(q, "el pie: [izq/der] modelo [F] girar", ve("[arr/ab] campo [izq/der] modelo [F] girar [=]"));
    TECLAS(T_DER);
    chk_cond(q, "[der]: exponencial", cu->el[0].variante == DIO_EXP && ve("exponencial"));
    TECLAS(T_DER);
    chk_cond(q, "[der]: 1N4148, y el eco del cat" S_aa "logo", cu->el[0].variante == DIO_1N4148 && ve("1N4148") &&
             ve("1N4148: Is 2,52 nA " S_CDOT " n 1,752 " S_CDOT " Rs 0,568 " S_OHM));
    TECLAS(T_DER);
    chk_cond(q, "[der]: 1N4007 y su eco", cu->el[0].variante == DIO_1N4007 && ve("Is 7,03 nA " S_CDOT " n 1,808"));
    TECLAS(T_IZQ, T_CATALOGO);
    chk_cond(q, "[cat" S_aa "logo]: la p" S_aa "gina Modelo", ui_pantalla() == P_MODELO);
    TECLAS(T_ESC);
    chk_cond(q, "[esc] vuelve a la ficha, con el 1N4148", ui_pantalla() == P_ELEMENTO && cu->el[0].variante == DIO_1N4148);
    TECLAS(T_IZQ, T_IZQ);
    chk_cond(q, "[izq][izq]: otra vez con V" S_GAMMA ", en la misma fila (la V)", cu->el[0].variante == DIO_VG &&
             ve("Tensi" S_oo "n " S_aa "nodo - c" S_aa "todo"));
    TECLAS(T_ARRIBA, T_ARRIBA);
    chk_cond(q, "[arr][arr]: la fila V" S_GAMMA, ve("Tensi" S_oo "n de codo"));
    chk_cond(q, "con V" S_GAMMA " no hay eco de cat" S_aa "logo", !ve("Is 2,52 nA"));
    TECLAS(T_DIG(0), T_PUNTO, T_DIG(6), T_ENTER);
    chk_cond(q, "0 [.] 6 [enter] en la fila V" S_GAMMA " guarda 0,6", ui_pantalla() == P_EDITOR &&
             cu->el[0].par[QD_VG - Q_P1].est == M_DATO && cu->el[0].par[QD_VG - Q_P1].v == 0.6);
    chk_cond(q, "el diodo sigue siendo v" S_aa "lido", circ_valido(cu));

    /* ---- La controlada: la fila x ---- */
    q = "controlada";
    ui_init();
    TECLAS(T_ENTER, T_ENTER, T_ENTER);
    TECLAS(T_LETRA('E'), T_ABAJO, T_DIG(9), T_ENTER);          /* E1 (1,1)-(1,2) de 9 V */
    TECLAS(T_DER, T_DER);                                       /* el cursor a (3,2) */
    TECLAS(T_CATALOGO);
    repite(T_ABAJO, 4);
    TECLAS(T_DER, T_ENTER, T_DER);                              /* E2 (3,2)-(4,2) */
    k = cu->nel - 1;
    chk_cond(q, "la paleta ([ab] x4 [der]) pone E2, una E controlada sin control", k == 1 &&
             cu->el[k].tipo == EL_EDEP && strcmp(cu->el[k].etiq, "E2") == 0 && cu->el[k].ref == -1);
    chk_cond(q, "su ficha se abre en la fila x: falta", ui_pantalla() == P_ELEMENTO && ve("falta") &&
             ve("[arr/ab] campo [+][-] control [izq/der] tipo [=]"));
    chk_cond(q, "y el aviso de c" S_oo "mo se elige", ve("[+][-] elige qui" S_ee "n la manda (en naranja)."));
    ui_dibuja();
    u4_pt(1, 1, &sx, &sy);
    chk_cond(q, "sin control, E1 no va en naranja", !r3u_hay_color(sx - 10, sy, sx + 10, sy + 24, C_RESALTA));
    TECLAS(T_MAS);
    chk_cond(q, "[+]: la V de E1 la manda", k >= 0 && cu->el[k].ref == 0 && ve("V(E1)") && ve("control"));
    ui_dibuja();
    chk_cond(q, "y E1 se ve en naranja (C_RESALTA)", r3u_hay_color(sx - 10, sy, sx + 10, sy + 24, C_RESALTA));
    TECLAS(T_IZQ);
    chk_cond(q, "[izq]: por una I, I(E1), en la misma fila", cu->el[k].variante == DEP_I && ve("I(E1)") &&
             ve("[+][-] control"));
    TECLAS(T_BORRAR);
    chk_cond(q, "[del]: sin control", cu->el[k].ref == -1 && ve("falta"));
    TECLAS(T_DIG(5));
    chk_cond(q, "5: el control no se escribe", ve("El control se elige con [+][-].") && cu->el[k].ref == -1);
    TECLAS(T_INCOG);
    chk_cond(q, "[?!]: no se despeja", ve("El control no se despeja: el" S_ii "gelo con [+][-]."));
    chk_cond(q, "aux sigue vac" S_ii "a (la fila x no es una magnitud)", cu->el[k].aux.est == M_LIBRE &&
             cu->el[k].aux.v == 0.0 && cu->el[k].val.est == M_LIBRE);
    TECLAS(T_MENOS);
    chk_cond(q, "[-] desde ninguno: el " S_uu "ltimo de dos bornes que no es ella (E1)", cu->el[k].ref == 0);
    TECLAS(T_ESC);
    ui_dibuja();
    chk_cond(q, "en el editor, con E2 elegida, E1 sigue en naranja", ui_pantalla() == P_EDITOR &&
             r3u_hay_color(sx - 10, sy, sx + 10, sy + 24, C_RESALTA));
    chk_cond(q, "el circuito es v" S_aa "lido", circ_valido(cu));
    /* sola, no tiene a quién elegir */
    ui_init();
    TECLAS(T_ENTER, T_ENTER, T_ENTER);
    TECLAS(T_CATALOGO);
    repite(T_ABAJO, 4);
    TECLAS(T_DER, T_DER, T_ENTER, T_DER);                       /* J1: una J controlada */
    chk_cond(q, "[ab] x4 [der] x2: J1, una J controlada", cu->nel == 1 && cu->el[0].tipo == EL_JDEP &&
             strcmp(cu->el[0].etiq, "J1") == 0 && ui_pantalla() == P_ELEMENTO);
    TECLAS(T_MAS);
    chk_cond(q, "sola, [+]: no hay otro elemento de dos bornes", cu->el[0].ref == -1 &&
             ve("No hay otro elemento de dos bornes."));
    /* un control que no vale: E2 por la I de una J controlada (CTL_I_NO) */
    u4_controladas(&c, 1);
    chk_cond(q, "(armado: v" S_aa "lido, y E2 con CTL_I_NO)", circ_valido(&c) && circ_ctl_motivo(&c, 3) == CTL_I_NO &&
             circ_ctl_motivo(&c, 2) == CTL_OK);
    chk_cond(q, "abierto", r3u_abre(&c));
    chk_cond(q, "la l" S_ii "nea de estado: el control de E2 no vale", ve("El control de E2 no vale: mira su ficha."));
    TECLAS(T_DER, T_DER, T_DER, T_ENTER);                       /* (3,0): E2 a la izquierda */
    chk_cond(q, "[enter] abre E2 (r, x, V, I)", ui_pantalla() == P_ELEMENTO && ve("E2  E controlada"));
    TECLAS(T_ABAJO);
    chk_cond(q, "su fila x: no vale, y el eco dice por qu" S_ee, ve("I(J1)") && ve("no vale") &&
             ve("La corriente de J1 no puede mandar a E2"));
    TECLAS(T_ESC);
    limpia_mis();

    /* ---- El AO: las flechas que caben, colocarlo y lo que no se puede ---- */
    q = "AO";
    ui_init();
    TECLAS(T_ENTER, T_ENTER, T_ENTER);
    TECLAS(T_DER, T_DER, T_DER, T_DER, T_DER, T_DER, T_DER, T_ABAJO);   /* (8,2) */
    TECLAS(T_LETRA('U'));
    chk_cond(q, "[U]: la instrucci" S_oo "n del AO", ve("Amp. op.: flecha hacia la salida"));
    chk_cond(q, "en (8,2) solo cabe hacia la izquierda: esa flecha en C_CURSOR",
             u4_flecha(8, 2, DIR_IZQ, C_CURSOR) && !u4_flecha(8, 2, DIR_IZQ, C_TENUE));
    chk_cond(q, "y las otras tres, tenues", u4_flecha(8, 2, DIR_DER, C_TENUE) && u4_flecha(8, 2, DIR_ABA, C_TENUE) &&
             u4_flecha(8, 2, DIR_ARR, C_TENUE) && !u4_flecha(8, 2, DIR_DER, C_CURSOR));
    TECLAS(T_DER);
    chk_cond(q, "[" S_RARR "] en (8,2): no cabe, se sale de la rejilla; no se toca nada", cu->nel == 0 &&
             ve("No cabe hacia " S_RARR ": se sale de la rejilla."));
    TECLAS(T_ESC, T_IZQ, T_IZQ, T_IZQ, T_IZQ);                  /* a (4,2) */
    TECLAS(T_LETRA('U'));
    chk_cond(q, "en (4,2) caben las cuatro", u4_flecha(4, 2, DIR_DER, C_CURSOR) && u4_flecha(4, 2, DIR_ABA, C_CURSOR) &&
             u4_flecha(4, 2, DIR_IZQ, C_CURSOR) && u4_flecha(4, 2, DIR_ARR, C_CURSOR));
    TECLAS(T_DER);
    chk_cond(q, "[" S_RARR "] coloca U1: - en (4,2), + en (4,4), salida en (6,3)", cu->nel == 1 && cu->el[0].tipo == EL_AO &&
             strcmp(cu->el[0].etiq, "U1") == 0 && cu->el[0].xb == 4 && cu->el[0].yb == 2 && cu->el[0].xa == 4 &&
             cu->el[0].ya == 4 && cu->el[0].xc == 6 && cu->el[0].yc == 3 && circ_valido(cu));
    chk_cond(q, "y abre su ficha: ideal, con su eco", ui_pantalla() == P_ELEMENTO && cu->el[0].variante == AO_IDEAL &&
             ve("Ideal: sin corriente de entrada; en lineal Vd = 0"));
    chk_cond(q, "sin tierra ni fuentes: U1 no tiene referencia", ve("Sin [G] ni fuentes, U1 no tiene referencia: pon [G]."));
    chk_cond(q, "el pie: [F] espejo", ve("[arr/ab] campo [izq/der] modelo [F] espejo [=]"));
    TECLAS(T_DER);
    chk_cond(q, "[der]: real", cu->el[0].variante == AO_REAL && ve("real (A, Ri, Ro)"));
    TECLAS(T_ARRIBA);
    chk_cond(q, "con la fila A", ve("Ganancia en lazo abierto"));
    TECLAS(T_ABAJO, T_ABAJO, T_ABAJO);
    chk_cond(q, "con la fila Ri", ve("Resistencia entre + y -"));
    TECLAS(T_ABAJO);
    chk_cond(q, "y con la fila Ro", ve("Resistencia de salida"));
    TECLAS(T_ESC);
    chk_cond(q, "el cursor salt" S_oo " a la salida: Aqu" S_ii ": U1 (sal)", ui_pantalla() == P_EDITOR &&
             ve("Aqu" S_ii ": U1 (sal)"));
    TECLAS(T_ENTER);
    chk_cond(q, "[enter] en una pata: U1 (sus segmentos son uno)", ui_pantalla() == P_ELEMENTO && ve("U1  Amp. operacional"));
    TECLAS(T_ESC);
    /* un cable contra la caja */
    TECLAS(T_LETRA('W'), T_IZQ);
    chk_cond(q, "[W][" S_LARR "] desde la salida: ah" S_ii " est" S_aa " U1", cu->nel == 1 &&
             circ_seg(cu, 6, 3, DIR_IZQ) == SEG_EL0 && ve("Ah" S_ii " est" S_aa " U1: los cables van a sus patas."));
    TECLAS(T_DER);
    chk_cond(q, "el cursor no se movi" S_oo ": [" S_RARR "] pone el cable (6,3)-(7,3)", circ_seg(cu, 6, 3, DIR_DER) == SEG_CABLE);
    TECLAS(T_ESC, T_IZQ, T_IZQ);                                /* a (5,3), dentro de la caja */
    chk_cond(q, "en (5,3): dentro de U1", ve("Aqu" S_ii ": dentro de U1 (no es una pata)"));
    TECLAS(T_LETRA('G'));
    chk_cond(q, "[G] no pone la tierra dentro", cu->tierra_x < 0 && ve("Dentro de U1 no hay borne: ponla en una pata."));
    TECLAS(T_LETRA('T'));
    chk_cond(q, "[T] tampoco un terminal", cu->term_x[0] < 0 && cu->term_x[1] < 0 && ve("Dentro de U1 no hay borne"));
    TECLAS(T_LETRA('O'), T_ENTER);
    chk_cond(q, "ni [O][enter] una sonda", cu->sonda[0].tipo == SONDA_NO && cu->sonda[1].tipo == SONDA_NO &&
             ve("Dentro de U1 no hay borne"));
    TECLAS(T_ENTER);
    chk_cond(q, "[enter] en un punto interior abre U1", ui_pantalla() == P_ELEMENTO && ve("U1  Amp. operacional"));
    TECLAS(T_ESC, T_ARRIBA);                                    /* a (5,2), también interior */
    TECLAS(T_LETRA('W'), T_ARRIBA);
    chk_cond(q, "un cable desde un punto interior: no es una pata", circ_seg(cu, 5, 2, DIR_ARR) == SEG_VACIO &&
             ve("Ese punto est" S_aa " dentro de U1: no es una pata."));
    TECLAS(T_LETRA('R'), T_ARRIBA);
    chk_cond(q, "ni un elemento de dos bornes", cu->nel == 1 && ve("dentro de U1"));
    TECLAS(T_ESC, T_LETRA('O'), T_IZQ);
    chk_cond(q, "[O] hacia la caja: sin sondas en las patas", cu->sonda[0].tipo == SONDA_NO &&
             ve("Sin sondas en las patas: [O] en el punto o un A."));
    TECLAS(T_IZQ);                                              /* a (4,2), la pata - */
    chk_cond(q, "en la pata -: U1 (-)", ve("Aqu" S_ii ": U1 (-)"));
    /* otro AO que no cabe: la marca roja en el punto que estorba */
    TECLAS(T_LETRA('U'), T_IZQ);
    chk_cond(q, "[U][" S_LARR "] en (4,2): no cabe, estorba U1", cu->nel == 1 && ve("No cabe hacia " S_LARR ": estorba U1."));
    chk_cond(q, "la marca roja en (4,2)", u4_color_en(4, 2, 4, C_ERROR));
    TECLAS(T_DIG(5));
    chk_cond(q, "la acci" S_oo "n sigue (vuelve su instrucci" S_oo "n) y la marca se va",
             ve("Amp. op.: flecha hacia la salida") && !u4_color_en(4, 2, 4, C_ERROR));
    TECLAS(T_ESC);
    /* el espejo con U1 elegido */
    TECLAS(T_LETRA('F'));
    chk_cond(q, "[F]: espejo, + y - cambian de sitio", ve("Espejo: + y - de U1 cambian de sitio.") &&
             cu->el[0].xa == 4 && cu->el[0].ya == 2 && cu->el[0].xb == 4 && cu->el[0].yb == 4 &&
             cu->el[0].xc == 6 && cu->el[0].yc == 3 && circ_valido(cu));
    TECLAS(T_LETRA('Z') | T_CON_CTRL);
    chk_cond(q, "[ctrl][Z] lo deshace", cu->el[0].xa == 4 && cu->el[0].ya == 4 && cu->el[0].xb == 4 && cu->el[0].yb == 2);
    /* [del] hacia la caja borra el componente entero */
    TECLAS(T_BORRAR, T_DER);
    chk_cond(q, "[del][" S_RARR "] desde la pata -: U1 entero fuera", cu->nel == 0 && circ_seg(cu, 4, 2, DIR_ABA) == SEG_VACIO &&
             circ_seg(cu, 6, 3, DIR_DER) == SEG_CABLE);
    TECLAS(T_LETRA('Z') | T_CON_CTRL);
    chk_cond(q, "y [ctrl][Z] lo recupera", cu->nel == 1 && cu->el[0].tipo == EL_AO && circ_valido(cu));

    /* un AO con una fuente en su subcircuito, y lo que estorba */
    ui_init();
    TECLAS(T_ENTER, T_ENTER, T_ENTER);
    TECLAS(T_LETRA('E'), T_ABAJO, T_DIG(5), T_ENTER);          /* E1 (1,1)-(1,2) */
    TECLAS(T_LETRA('W'), T_DER, T_DER, T_DER, T_ESC);           /* (1,2)-(4,2) */
    TECLAS(T_LETRA('U'), T_DER);
    chk_cond(q, "U1 unido a E1: su salida va al - de E1", cu->nel == 2 && cu->el[1].tipo == EL_AO &&
             ve("Sin [G], la salida de U1 va al - de E1."));
    TECLAS(T_ESC);
    circ_init(&c, "Estorba");
    k = circ_elemento(&c, EL_RES, 0, 0, DIR_DER); u4_dato(&c.el[k].val, 1e3);
    circ_cable(&c, 5, 1, DIR_ABA);                              /* un cable que entra en (5,2) */
    chk_cond(q, "(abierto)", r3u_abre(&c));
    TECLAS(T_DER, T_DER, T_DER, T_DER, T_ABAJO, T_ABAJO);       /* (4,2) */
    TECLAS(T_LETRA('U'), T_DER);
    chk_cond(q, "un cable en un punto interior: estorba un cable", cu->nel == 1 &&
             ve("No cabe hacia " S_RARR ": estorba un cable.") && u4_color_en(5, 2, 4, C_ERROR));
    TECLAS(T_ESC);
    circ_init(&c, "Estorba la tierra");
    k = circ_elemento(&c, EL_RES, 0, 0, DIR_DER); u4_dato(&c.el[k].val, 1e3);
    c.tierra_x = 5; c.tierra_y = 3;
    chk_cond(q, "(abierto)", r3u_abre(&c));
    TECLAS(T_DER, T_DER, T_DER, T_DER, T_ABAJO, T_ABAJO, T_LETRA('U'), T_DER);
    chk_cond(q, "la tierra en un punto interior: estorba la tierra", cu->nel == 1 &&
             ve("No cabe hacia " S_RARR ": estorba la tierra.") && u4_color_en(5, 3, 4, C_ERROR));
    TECLAS(T_ESC);
    limpia_mis();

    /* ---- El menú de opciones: 12 (la 11, «Guardar como copia», entró
     *      antes de «Salir»). La caja sube (empezaba en y = 40) para que
     *      siga acabando en 209: la línea de avisos (ED_AQUI) no se tapa ---- */
    q = "men" S_uu;
    ui_init();
    TECLAS(T_ENTER, T_ENTER, T_ENTER);
    TECLAS(T_MENU);
    ui_dibuja();
    chk_cond(q, "la caja va de y = 27 a 209", gfx_buffer()[27 * SCR_W + 100] == C_CURSOR &&
             gfx_buffer()[208 * SCR_W + 100] == C_CURSOR && gfx_buffer()[26 * SCR_W + 100] != C_CURSOR);
    {
        /* en y = 209 a 221, ni el borde de la caja ni su relleno: se lee
         * «Aquí: nada (punto vacío)» (con 12 filas desde y = 40, la tapaba) */
        int yy, xx, borde = 0, texto = 0;
        for (yy = 209; yy < 222; yy++) {
            if (gfx_buffer()[yy * SCR_W + 34] == C_CURSOR) borde = 1;
            for (xx = 40; xx < 150; xx++) if (gfx_buffer()[yy * SCR_W + xx] == C_TEXTO) texto = 1;
        }
        chk_cond(q, "... y no tapa la l" S_ii "nea de avisos (y = 209 a 221)", !borde && texto);
    }
    chk_cond(q, "con el barrido [I] y el punto de operaci" S_oo "n [P]", ve("Barrido de CC: curvas        [I]") &&
             ve("Punto de operaci" S_oo "n de CC     [P]") && ve("Salir al men" S_uu " principal"));
    TECLAS(T_ARRIBA, T_ENTER);
    chk_cond(q, "[menu][arr][enter]: salir", ui_pantalla() == P_MENU);
    TECLAS(T_ENTER, T_ENTER);                                   /* Taller, problema nuevo (vacío) */
    TECLAS(T_LETRA('I'));
    chk_cond(q, "[I] en vac" S_ii "o: nada que barrer", ui_pantalla() == P_EDITOR &&
             ve("Nada que barrer: pon una E, una J o una R."));
    TECLAS(T_MENU);
    repite(T_ABAJO, 8);
    TECLAS(T_ENTER);
    chk_cond(q, "la opci" S_oo "n 8, igual", ui_pantalla() == P_EDITOR && ve("Nada que barrer: pon una E, una J o una R."));
    /* con un circuito, la 8 hace lo mismo que [I] y la 9 lo mismo que [P] */
    u4_serie(&c);
    for (j = 0; j < 2; j++) {
        static u16 fb[SCR_W * SCR_H];
        int p1, p2, tec = j ? T_LETRA('P') : T_LETRA('I');
        chk_cond(q, "(abierto)", r3u_abre(&c));
        teclas(&tec, 1);
        for (n = 0; n < 100000 && ui_pantalla() == P_EMULANDO; n++) ui_tick();
        p1 = ui_pantalla();
        ui_dibuja();
        memcpy(fb, gfx_buffer(), sizeof fb);
        chk_cond(q, "(abierto otra vez)", r3u_abre(&c));
        TECLAS(T_MENU);
        repite(T_ABAJO, 8 + j);
        TECLAS(T_ENTER);
        for (n = 0; n < 100000 && ui_pantalla() == P_EMULANDO; n++) ui_tick();
        p2 = ui_pantalla();
        ui_dibuja();
        chk_cond(q, j ? "la opci" S_oo "n 9 es [P]" : "la opci" S_oo "n 8 es [I]",
                 p1 == p2 && p1 != P_MENU && memcmp(fb, gfx_buffer(), sizeof fb) == 0);
    }
    limpia_mis();

    /* ---- La paleta de PAL_N piezas (20 en el paso 11: hasta «Amp. op.») ---- */
    q = "paleta";
    chk_cond(q, "las piezas 0 a 13 no se mueven (la 13, el vat" S_ii "metro)", ui_paleta_tipo(0) == EL_RES &&
             ui_paleta_tipo(1) == EL_FUENTE && ui_paleta_tipo(8) == EL_COND && ui_paleta_tipo(12) == EL_IMP &&
             ui_paleta_tipo(13) == EL_VATI && ui_paleta_tipo(5) == -1 && ui_paleta_tipo(11) == -1);
    chk_cond(q, "del 14 al 19: diodo, zener, LED, E y J controladas y el AO", PAL_N >= 20 &&
             ui_paleta_tipo(14) == EL_DIODO && ui_paleta_tipo(15) == EL_ZENER && ui_paleta_tipo(16) == EL_LED &&
             ui_paleta_tipo(17) == EL_EDEP && ui_paleta_tipo(18) == EL_JDEP && ui_paleta_tipo(19) == EL_AO);
    chk_cond(q, "fuera de la paleta, -1", ui_paleta_tipo(-1) == -1 && ui_paleta_tipo(PAL_N) == -1);
    ui_init();
    TECLAS(T_ENTER, T_ENTER, T_ENTER);
    TECLAS(T_CATALOGO);
    chk_cond(q, "las piezas nuevas, con su tecla", ve("Diodo") && ve("[D]") && ve("Zener") && ve("LED") &&
             ve("E control.") && ve("J control.") && ve("Amp. op.") && ve("[U]"));
    repite(T_ABAJO, 4);
    repite(T_DER, 3);
    ui_dibuja();
    chk_cond(q, "[ab] x4 [der] x3: la celda 19, en (232, 161) de 70 x 30", gfx_buffer()[161 * SCR_W + 232] == C_BARRA &&
             gfx_buffer()[190 * SCR_W + 301] == C_BARRA && gfx_buffer()[161 * SCR_W + 160] == C_PANEL);
    chk_cond(q, "el panel acaba en y = 224", gfx_buffer()[223 * SCR_W + 100] == C_CURSOR && ve("Amp. op."));
    TECLAS(T_ENTER, T_DER);
    chk_cond(q, "[enter] y [" S_RARR "]: U1", cu->nel == 1 && cu->el[0].tipo == EL_AO && ui_pantalla() == P_ELEMENTO);
    TECLAS(T_ESC, T_CATALOGO, T_ARRIBA, T_ENTER, T_DER);        /* la 15: el zener, de (3,2) a (4,2) */
    chk_cond(q, "[arr] desde el AO: el zener, sin tecla", cu->nel == 2 && cu->el[1].tipo == EL_ZENER &&
             ui_pantalla() == P_ELEMENTO && ve("[izq/der] modelo [F] girar"));
    TECLAS(T_DER, T_DER);
    chk_cond(q, "el zener 1N4733A y su eco", cu->el[1].variante == ZEN_1N4733A && ve("1N4733A: Vz 5,1 V a 49 mA"));
    TECLAS(T_ESC, T_CATALOGO, T_DER);                           /* la 15 acaba su fila: [der] no pasa */
    TECLAS(T_ABAJO, T_IZQ, T_IZQ, T_IZQ, T_ENTER, T_ARRIBA);    /* la 16: el LED, de (4,2) a (4,1) */
    chk_cond(q, "[ab] y [izq] x3: el LED, y se enciende en el resultado", cu->nel == 3 && cu->el[2].tipo == EL_LED &&
             ve("Se enciende en el resultado: [=].") && ve("[izq/der] color"));
    chk_cond(q, "rojo: el eco de su color", ve("Rojo: Vf 1,8 V a 20 mA (n 2, Rs 5 " S_OHM ")"));
    TECLAS(T_DER);
    chk_cond(q, "naranja", ve("Naranja: Vf 2 V a 20 mA (n 2, Rs 5 " S_OHM ")"));
    TECLAS(T_ESC);
    /* los iconos de 20 x 14, dentro de su celda: se pintan sobre un fondo y
     * se mira el marco de alrededor */
    for (j = ICO_DIODO; j <= ICO_MOS; j++) {
        int x, y, fuera = 0;
        gfx_clear(C_FONDO);
        gfx_fill(96, 96, 28, 22, C_PANEL);
        esq_icono(j, 100, 100, C_CURSOR);
        for (y = 96; y < 118; y++)
            for (x = 96; x < 124; x++)
                if ((x < 100 || x > 119 || y < 100 || y > 113) && gfx_buffer()[y * SCR_W + x] != C_PANEL) fuera++;
        pruebas++;
        if (fuera) falla("iconos", "un icono de la Etapa 4 se sale de su celda de 20 x 14", j, 0);
    }

    /* ---- La línea de estado ---- */
    q = "l" S_ii "nea de estado";
    u4_serie(&c);
    chk_cond(q, "(abierto)", r3u_abre(&c));
    chk_cond(q, "E, R y un diodo: [=] da el punto de operaci" S_oo "n", ve("Listo: [=] da el punto de operaci" S_oo "n"));
    TECLAS(T_INCOG);
    chk_cond(q, "[?!]: la pr" S_aa "ctica tapada es lineal", ui_pantalla() == P_EDITOR &&
             ve("La pr" S_aa "ctica tapada es de CC lineal."));
    c.el[1].val.est = M_BUSCAR;
    chk_cond(q, "(abierto)", r3u_abre(&c));
    chk_cond(q, "con R1 [?!]: se barre y se lee", ve("Con D1 no se despeja: [I] barre y [N] lee"));
    c.el[1].val.est = M_LIBRE;
    chk_cond(q, "(abierto)", r3u_abre(&c));
    chk_cond(q, "sin el valor de R1: falta", ve("Para el punto de operaci" S_oo "n falta R en R1."));
    c.el[1].val.est = M_DATO;
    c.el[2].tipo = EL_ZENER; c.el[2].variante = ZEN_LIBRO;     /* rz·Iz = 10 V > Vz */
    u4_dato(&c.el[2].par[QZ_RZ - Q_P1], 1e3); u4_dato(&c.el[2].par[QZ_IZ - Q_P1], 10e-3);
    chk_cond(q, "(abierto)", circ_valido(&c) && r3u_abre(&c));
    chk_cond(q, "un zener imposible: su modelo no cuadra", ve("D1: su modelo no cuadra: [=] dice por qu" S_ee "."));
    u4_serie(&c);
    c.el[0].variante = ONDA_SENO; u4_dato(&c.el[0].par[Q_FREC - Q_P1], 1e3);
    chk_cond(q, "(abierto)", r3u_abre(&c));
    chk_cond(q, "no lineal y din" S_aa "mico: se emula", ve("Listo: pulsa [=] para emular"));
    u4_controladas(&c, 0);
    chk_cond(q, "(abierto)", circ_valido(&c) && r3u_abre(&c));
    chk_cond(q, "E2 sin control", ve("E2 no tiene control: el" S_ii "gelo en su ficha."));
    /* un astable sin tierra ni fuentes: U1 sin referencia */
    circ_init(&c, "Astable sin tierra");
    circ_elemento(&c, EL_AO, 4, 2, DIR_DER);
    k = circ_elemento(&c, EL_RES, 6, 3, DIR_DER);  u4_dato(&c.el[k].val, 10e3);
    k = circ_elemento(&c, EL_COND, 4, 2, DIR_IZQ); u4_dato(&c.el[k].val, 100e-9);
    k = circ_elemento(&c, EL_RES, 3, 2, DIR_ABA);  u4_dato(&c.el[k].val, 10e3);
    circ_cable(&c, 3, 3, DIR_ABA); circ_cable(&c, 3, 4, DIR_DER);
    chk_cond(q, "(abierto)", c.nel == 4 && circ_valido(&c) && r3u_abre(&c));
    chk_cond(q, "un AO sin referencia", ve("U1 sin referencia: pon la tierra con [G]."));
    limpia_mis();

    /* ---- Mis problemas: lo que hace [=] ---- */
    q = "Mis problemas";
    u4_serie(&c);
    limpia_mis();
    chk_cond(q, "(guardado)", arch_guardar(0, &c));
    ui_init();
    TECLAS(T_ENTER, T_ENTER, T_ABAJO, T_ABAJO, T_ENTER);        /* portada, menú, Taller, Mis */
    chk_cond(q, "un no lineal sin dinámica: se da el punto de operaci" S_oo "n", ui_pantalla() == P_MIS &&
             ve("Se da el punto de operaci" S_oo "n."));
    c.el[0].variante = ONDA_SENO; u4_dato(&c.el[0].par[Q_FREC - Q_P1], 1e3);
    chk_cond(q, "(guardado)", arch_guardar(0, &c));
    ui_init();
    TECLAS(T_ENTER, T_ENTER, T_ABAJO, T_ABAJO, T_ENTER);
    chk_cond(q, "din" S_aa "mico: se emula", ui_pantalla() == P_MIS && ve("Se emula."));
    limpia_mis();

    chk_cond("interfaz E4", "sin textos fuera de la pantalla", gfx_desbordes() == 0);
}
