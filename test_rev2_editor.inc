/* --------------------------------------------------------------------------
 *  65o. Etapa 4: los hallazgos del editor de la segunda revisión
 *
 *  Una prueba por hallazgo (cada una falla sin su arreglo, comprobado por
 *  mutación): el Is de un diodo en la ficha (la caja, el eco, la fila de CA y
 *  el aviso de rango), un AO suelto en un circuito que ya tiene tierra, la
 *  salida de un AO en su referencia, la línea de estado con una fuente
 *  alterna, un no lineal y un [?!], y [P] con el circuito vacío. Se incluye
 *  tras test_e4_rev.inc (usa r4_led, r4_el, r4_espera, r3u_abre, ve, repite,
 *  TECLAS y chk_cond).
 * ----------------------------------------------------------------------- */

/* Taller > problema nuevo, desde el arranque */
static void r2e_nuevo(void)
{
    ui_init();
    TECLAS(T_ENTER, T_ENTER, T_ENTER);                   /* portada, menú, Taller > problema nuevo */
}

/* E1 alterna (5 V a 50 Hz), R1 de 1 kΩ, D1 con Vγ y R2 de 1 kΩ en un lazo;
 * R2 (el cuarto elemento) con [?!] */
static void r2e_alterna(Circuito *c)
{
    int k;
    circ_init(c, "alterna");
    k = circ_elemento(c, EL_FUENTE, 1, 2, DIR_ARR);
    c->el[k].variante = FORMA_FASOR;
    c->el[k].val.v = 5.0; c->el[k].val.est = M_DATO;
    c->el[k].val.ang = 0.0; c->el[k].val.est_ang = M_DATO;
    c->frec.v = 50.0; c->frec.est = M_DATO;
    k = circ_elemento(c, EL_RES, 1, 1, DIR_DER);
    c->el[k].val.v = 1e3; c->el[k].val.est = M_DATO;
    k = circ_elemento(c, EL_DIODO, 2, 1, DIR_ABA);
    c->el[k].variante = DIO_VG;
    k = circ_elemento(c, EL_RES, 2, 2, DIR_IZQ);
    c->el[k].val.est = M_BUSCAR;
    c->tierra_x = 1; c->tierra_y = 2;
}

/* Los textos nuevos, con los nombres más largos que pueden salir */
static const char *const R2E_TEXTOS[] = {
    "U7 no llega a la tierra ni a una fuente: " S_uu "nelo.",
    "La salida de U7 est" S_aa " en su referencia: mu" S_ee "vela.",
    "Nada que calcular: el circuito est" S_aa " vac" S_ii "o.",
    "Para emular falta R en R32.",
    "Fuera de rango: entre 1E-20 y 1m A.",
    "Fuera de rango: entre 1E-15 y 1M F.",
};

static void test_rev2_editor(void)
{
    static Circuito c;
    const Circuito *cu = ui_circuito();
    const Mag *is;
    const char *q;
    int k, kD, i;

    printf("[65o] Etapa 4: los hallazgos del editor (segunda revisi" S_oo "n)\n");
    gfx_desbordes_reset();

    /* ---- El Is de un diodo: la caja y el eco enseñan sus cifras (no «0»)
     *      y el aviso de rango dice su mínimo (no «0n») ---- */
    q = "Is en la ficha";
    r2e_nuevo();
    chk_cond(q, "Taller > problema nuevo", ui_pantalla() == P_EDITOR && cu->nel == 0);
    TECLAS(T_LETRA('D'), T_DER, T_DER);                  /* D1 y su ficha; [der]: exponencial */
    kD = r4_el(cu, "D1");
    chk_cond(q, "D1 exponencial, en la fila Is", ui_pantalla() == P_ELEMENTO && kD >= 0 &&
             cu->el[kD].variante == DIO_EXP && ve("Corriente de saturaci" S_oo "n"));
    is = &cu->el[kD >= 0 ? kD : 0].par[QD_IS - Q_P1];
    TECLAS(T_DIG(1), T_EE, T_SIGNO, T_DIG(1), T_DIG(4));
    chk_cond(q, "el eco de 1E-14: «= 1E-14 A», no «= 0 A»", ve("= 1E-14 A") && !ve("= 0 A"));
    TECLAS(T_ABAJO, T_ARRIBA);                           /* se guarda y se vuelve a la fila */
    chk_cond(q, "guardado 1E-14; la caja: «1E-14»", is->est == M_DATO && fabs(is->v - 1e-14) <= 1e-12 * 1e-14 &&
             ve("\n1E-14\n"));
    TECLAS(T_BORRAR, T_DIG(2), T_PUNTO, T_DIG(5), T_DIG(2), T_LETRA('N'));
    chk_cond(q, "el eco de 2,52n: «= 2,52E-9 A»", ve("= 2,52E-9 A") && !ve("0,000000003"));
    TECLAS(T_ABAJO, T_ARRIBA);
    chk_cond(q, "la caja: «2,52E-9»", fabs(is->v - 2.52e-9) <= 1e-12 * 2.52e-9 && ve("\n2,52E-9\n"));
    TECLAS(T_DIG(1), T_EE, T_SIGNO, T_DIG(2), T_DIG(1), T_ENTER);
    chk_cond(q, "1E-21: «Fuera de rango: entre 1E-20 y 1m A.»", ui_pantalla() == P_ELEMENTO &&
             ve("Fuera de rango: entre 1E-20 y 1m A.") && !ve("entre 0n"));
    TECLAS(T_DIG(5), T_EE, T_SIGNO, T_DIG(2), T_DIG(0), T_ABAJO, T_ARRIBA);
    chk_cond(q, "5E-20 s" S_ii " vale: la caja «5E-20»", fabs(is->v - 5e-20) <= 1e-12 * 5e-20 && ve("\n5E-20\n"));
    TECLAS(T_BORRAR, T_DIG(1), T_DIG(0), T_DIG(0), T_LETRA('N'), T_ABAJO, T_ARRIBA);
    chk_cond(q, "uno exacto con 9 decimales, como siempre: «0,0000001»", ve("\n0,0000001\n"));
    /* la fila de CA (una fuente alterna en el circuito) */
    r2e_alterna(&c);
    k = r4_el(&c, "D1");
    c.el[k].variante = DIO_EXP;
    c.el[k].par[QD_IS - Q_P1].v = 1e-14; c.el[k].par[QD_IS - Q_P1].est = M_DATO;
    chk_cond(q, "(abierto)", circ_valido(&c) && r3u_abre(&c));
    TECLAS(T_DER, T_DER, T_ABAJO, T_ENTER, T_ABAJO);     /* (2,1): la ficha de D1 */
    chk_cond(q, "en CA, la fila Is: «1E-14»", ui_pantalla() == P_ELEMENTO && ve("Corriente de saturaci" S_oo "n") &&
             ve("\n1E-14\n"));
    TECLAS(T_ESC);

    /* ---- Un AO recién puesto (suelto) en un circuito con tierra y fuente:
     *      ni «Sin [G] ni fuentes» ni «pon [G]», que movería la tierra ---- */
    q = "AO suelto";
    r2e_nuevo();
    TECLAS(T_LETRA('E'), T_ABAJO, T_DIG(5), T_ENTER);
    TECLAS(T_LETRA('R'), T_DER, T_DIG(1), T_ENTER);
    TECLAS(T_LETRA('G'));
    repite(T_DER, 4);
    TECLAS(T_LETRA('U'), T_DER);
    k = r4_el(cu, "U1");
    chk_cond(q, "U1 puesto, con E1, R1 y la tierra", ui_pantalla() == P_ELEMENTO && cu->nel == 3 && k >= 0 &&
             cu->tierra_x >= 0 && r4_el(cu, "E1") >= 0);
    chk_cond(q, "la ficha no dice «Sin [G] ni fuentes» ni «pon [G]»", !ve("Sin [G]") && !ve("pon [G]"));
    TECLAS(T_ESC);
    chk_cond(q, "la l" S_ii "nea de estado: que lo una, sin [G]", ui_pantalla() == P_EDITOR &&
             ve("U1 no llega a la tierra ni a una fuente: " S_uu "nelo.") && !ve("pon la tierra"));
    TECLAS(T_IGUAL);
    chk_cond(q, "[=]: lo mismo, sin «Pon la tierra con [G]»", ui_pantalla() == P_RESULTADO &&
             ve("no llega a ella") && ve(S_uu "nelo al circuito.") && !ve("Pon la tierra"));
    TECLAS(T_ESC);
    /* sin tierra en el circuito, lo de siempre */
    circ_init(&c, "AO sin tierra");
    circ_elemento(&c, EL_AO, 4, 2, DIR_DER);
    k = circ_elemento(&c, EL_RES, 6, 3, DIR_DER); c.el[k].val.v = 10e3; c.el[k].val.est = M_DATO;
    k = circ_elemento(&c, EL_RES, 4, 2, DIR_IZQ); c.el[k].val.v = 10e3; c.el[k].val.est = M_DATO;
    chk_cond(q, "(abierto)", circ_valido(&c) && r3u_abre(&c));
    chk_cond(q, "sin tierra: «pon la tierra con [G]», como antes", ve("U1 sin referencia: pon la tierra con [G]."));
    TECLAS(T_IGUAL);
    chk_cond(q, "sin tierra, [=]: «Pon la tierra con [G].»", ui_pantalla() == P_RESULTADO &&
             ve("Pon la tierra con [G]."));

    /* ---- La salida de un AO en su referencia: la línea de estado lo dice,
     *      como [=] (AOR_CORTO), y no «Listo» ---- */
    q = "salida en su referencia";
    r2e_nuevo();
    TECLAS(T_LETRA('E'), T_ABAJO, T_DIG(1), T_ENTER);    /* E1 (1,1)-(1,2) */
    TECLAS(T_LETRA('W'), T_DER, T_DER, T_DER, T_ESC);   /* (1,2)-(4,2) */
    TECLAS(T_LETRA('U'), T_DER, T_ESC, T_LETRA('G'));   /* U1; la tierra en su salida */
    k = r4_el(cu, "U1");
    chk_cond(q, "la tierra en la salida de U1, (6,3)", k >= 0 && cu->el[k].xc == 6 && cu->el[k].yc == 3 &&
             cu->tierra_x == 6 && cu->tierra_y == 3);
    chk_cond(q, "la l" S_ii "nea de estado: «La salida de U1 est" S_aa " en su referencia»", ui_pantalla() == P_EDITOR &&
             ve("La salida de U1 est" S_aa " en su referencia: mu" S_ee "vela.") && !ve("Listo"));
    TECLAS(T_IGUAL);
    chk_cond(q, "y [=] da ese error", ui_pantalla() == P_RESULTADO && ve("unida a su referencia"));

    /* ---- Fuente alterna, un no lineal y un [?!]: la línea de estado dice lo
     *      que hace [=] (emular, y le falta R2) y el error nombra a R2 ---- */
    q = "alterna con [?!]";
    r4_led(&c, 9.0, 330.0);                              /* antes, una emulación de 3 elementos */
    rv_seno(&c);
    chk_cond(q, "(abierto)", r3u_abre(&c));
    TECLAS(T_IGUAL);
    r4_espera();
    chk_cond(q, "(emulado)", sim_res()->fin != FIN_ERROR);
    r2e_alterna(&c);
    chk_cond(q, "(abierto)", circ_valido(&c) && r3u_abre(&c));
    chk_cond(q, "la l" S_ii "nea de estado: para emular falta R en R2", ve("Para emular falta R en R2.") &&
             !ve("no se despeja"));
    TECLAS(T_IGUAL);
    r4_espera();
    chk_cond(q, "[=] emula y el error nombra a R2", ui_pantalla() == P_EMU_ESQ && ve("falta R en R2") &&
             !ve("un elemento"));
    /* sin alterna, lo de siempre: SOL_NO_LINEAL lleva a [I] */
    k = r4_el(&c, "E1");
    c.el[k].variante = ONDA_CC;
    chk_cond(q, "(abierto)", circ_valido(&c) && r3u_abre(&c));
    chk_cond(q, "sin alterna: «[I] barre y [N] lee»", ve("Con D1 no se despeja: [I] barre y [N] lee"));

    /* ---- [P] (y la opción 9) con el circuito vacío: un aviso, no «emular» ---- */
    q = "[P] vac" S_ii "o";
    r2e_nuevo();
    TECLAS(T_LETRA('P'));
    chk_cond(q, "[P]: un aviso en el editor", ui_pantalla() == P_EDITOR &&
             ve("Nada que calcular: el circuito est" S_aa " vac" S_ii "o.") && !ve("emular"));
    TECLAS(T_MENU);
    repite(T_ABAJO, 9);
    TECLAS(T_ENTER);
    chk_cond(q, "la opci" S_oo "n 9, igual", ui_pantalla() == P_EDITOR &&
             ve("Nada que calcular: el circuito est" S_aa " vac" S_ii "o."));

    for (i = 0; i < (int)(sizeof R2E_TEXTOS / sizeof R2E_TEXTOS[0]); i++)
        chk_cond("textos", R2E_TEXTOS[i], (int)strlen(R2E_TEXTOS[i]) <= 52);
    limpia_mis();
    chk_cond("interfaz", "ning" S_uu "n texto fuera de la pantalla", gfx_desbordes() == 0);
}
