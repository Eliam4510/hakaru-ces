/* --------------------------------------------------------------------------
 *  65t. Etapa 4: los hallazgos del grupo modelo de la tercera revisión
 *
 *  Una prueba por hallazgo (cada una falla sin su arreglo, comprobado por
 *  mutación):
 *    H1  una J controlada con un borne al aire (o en serie con un V ideal) y
 *        una E controlada en corto: la tabla dice lo mismo que con una J o
 *        una E independiente (RED_J_ABIERTA, RED_FUENTE_CORTO), como el
 *        emulador, y no «fuentes que se contradicen»
 *    H2  la entrada del Bode elegida con [F] sigue a su fuente al borrar un
 *        elemento de delante; si se borra ella, vuelve la automática
 *    H3  un valor al final de la emulación que es ruido de redondeo frente a
 *        la escala del elemento (e(t_fin) = 12·sen(12π) ≈ -1,8E-14 V) se
 *        rotula 0, y la tabla no escribe «-0 V»
 *    H4  un [?!] en algo que no se barre (una C): la línea de estado y el
 *        texto de SOL_NO_LINEAL no mandan a [I], como el pie
 *  Se incluye tras test_rev2_editor.inc (usa rv_ctl, rv_al_menu, rv_modelos,
 *  r4_el, r4_cab, r4_espera, r3u_abre y rv2_led_c).
 * ----------------------------------------------------------------------- */

/* ---- H1. J controlada al aire, E controlada en corto ---- */

/* rv_ctl con la controlada tipo (J1 = 1m·V(R2) o E2 = 2·V(R2)) y lo que se
 * pide: 0 quita el cable de vuelta (5,2)-(4,2) (un borne al aire); 1 pone en
 * su sitio un voltímetro ideal; 2 une los dos bornes de la controlada con un
 * cable (4,1)-(3,1)-(3,2); 3 lo mismo con un amperímetro ideal en (4,1)-(3,1);
 * otro, nada. Con indep, la fuente del mismo tipo independiente en su sitio
 * (el control). Devuelve el índice de la fuente */
static int r3v_ctl(Circuito *c, int tipo, int como, int indep)
{
    int k;
    rv_ctl(c, tipo, DEP_V, tipo == EL_JDEP ? 1e-3 : 2.0);
    if (como <= 1) circ_quitar(c, 5, 2, DIR_IZQ);
    if (como == 1) circ_elemento(c, EL_VOLT, 5, 2, DIR_IZQ);
    if (como == 2) { circ_cable(c, 4, 1, DIR_IZQ); circ_cable(c, 3, 1, DIR_ABA); }
    if (como == 3) { circ_elemento(c, EL_AMP, 4, 1, DIR_IZQ); circ_cable(c, 3, 1, DIR_ABA); }
    if (indep) {
        circ_quitar(c, 4, 2, DIR_ARR);
        k = circ_elemento(c, tipo == EL_JDEP ? EL_CORR : EL_FUENTE, 4, 2, DIR_ARR);
        c->el[k].val.v = tipo == EL_JDEP ? 0.5e-3 : 2.0; c->el[k].val.est = M_DATO;
        return k;
    }
    for (k = 0; k < c->nel && c->el[k].tipo != tipo; k++) ;
    return k;
}

static void r3v_fuentes_ctl(void)
{
    static const char *const COMO[4] = { "un borne al aire", "en serie con un V ideal", "en corto con un cable",
                                          "en corto con un A ideal" };
    static Circuito c;
    static Red r;
    static Solucion s;
    static Valores v;
    SimDiag d;
    char msg[240], msg2[240], q[80];
    int k, e, como, indep, err;

    for (como = 0; como < 4; como++)
        for (indep = 1; indep >= 0; indep--) {
            int tipo = como <= 1 ? EL_JDEP : EL_EDEP;
            k = r3v_ctl(&c, tipo, como, indep);
            red_extraer(&c, &r);
            resolver(&c, &r, &s);
            sol_mensaje(&c, &r, &s, msg, sizeof msg);
            e = sim_punto_operacion(&c, &r, &v, &d);
            sim_mensaje_circ(&c, 0, &d, msg2, (int)sizeof msg2);
            sprintf(q, "H1 %s %s %s", tipo == EL_JDEP ? "J" : "E", indep ? "independiente" : "controlada", COMO[como]);
            printf("  %s (%s): v" S_aa "lido %d, red %d (el %d), tabla %d: %s\n", q, c.el[k].etiq, circ_valido(&c),
                   r.error, r.elem_error, s.estado, msg);
            printf("      emulador %d: %s\n", e, msg2);
            err = como <= 1 ? RED_J_ABIERTA : como == 2 ? RED_FUENTE_CORTO : RED_FUENTE_AMP;
            chk_cond(q, "la red lo ve, con el elemento", circ_valido(&c) && r.error == err && r.elem_error == k &&
                     s.estado == SOL_ERR_RED && s.elem == k);
            chk_cond(q, "la tabla lo dice (y no «fuentes que se contradicen»)",
                     strstr(msg, c.el[k].etiq) != 0 && strstr(msg, "contradicen") == 0 &&
                     strstr(msg, como <= 1 ? "no tiene por d" : "cortocircuito") != 0);
            chk_cond(q, "el emulador tampoco lo emula", e == (como <= 1 ? SIM_CORTE_J : SIM_LAZO_V));
        }

    /* Lo que no cambia: una J controlada con su camino de vuelta, y una en
     * serie con una J independiente (sí se contradicen) */
    k = r3v_ctl(&c, EL_JDEP, 9, 0);
    red_extraer(&c, &r);
    resolver(&c, &r, &s);
    chk_cond("H1 J controlada", "con su camino de vuelta: se resuelve", r.error == RED_OK && s.estado == SOL_OK &&
             fabs(s.s[0].I[k] - 0.5e-3) <= 1e-15);
    circ_quitar(&c, 5, 2, DIR_IZQ);
    k = circ_elemento(&c, EL_CORR, 5, 2, DIR_IZQ);
    c.el[k].val.v = 1e-3; c.el[k].val.est = M_DATO;
    red_extraer(&c, &r);
    resolver(&c, &r, &s);
    sol_mensaje(&c, &r, &s, msg, sizeof msg);
    printf("  H1 J controlada en serie con una J: red %d, tabla %d: %s\n", r.error, s.estado, msg);
    chk_cond("H1 J controlada", "en serie con J2 (0,5 mA contra 1 mA): se contradicen", r.error == RED_OK &&
             s.estado == SOL_INCOMPAT && strstr(msg, "contradicen") != 0);

    /* En la interfaz: [=] en el editor */
    for (como = 0; como < 3; como += 2) {
        k = r3v_ctl(&c, como ? EL_EDEP : EL_JDEP, como, 0);
        sprintf(q, "H1 [=] %s", COMO[como]);
        if (r3u_abre(&c)) {
            TECLAS(T_IGUAL);
            r4_espera();
            printf("  %s: pantalla %d, «contradicen» %d\n", q, ui_pantalla(), ve("contradicen"));
            chk_cond(q, "P_RESULTADO lo dice, no «se contradicen»", ui_pantalla() == P_RESULTADO && !ve("contradicen") &&
                     ve(como ? "cortocircuito" : "no tiene por d"));
        } else chk_cond(q, "se abre en el editor", 0);
    }
}

/* ---- H2. La entrada del Bode sigue a su fuente ---- */

/* 0 R1 suelta en (8,0)-(9,0), 1 E1, 2 R2, 3 E2, 4 J1 (fasores), 5 C1, 6 R3,
 * 7 R4 y la sonda 1 en (2,1), a 1 kHz */
static void r3v_bode_circ(Circuito *c)
{
    int k;
    circ_init(c, "bode");
    k = circ_elemento(c, EL_RES, 8, 0, DIR_DER); c->el[k].val.v = 1e3; c->el[k].val.est = M_DATO;
    k = circ_elemento(c, EL_FUENTE, 0, 2, DIR_ARR);
    c->el[k].variante = FORMA_FASOR; c->el[k].val.v = 1.0; c->el[k].val.est = M_DATO;
    k = circ_elemento(c, EL_RES, 0, 1, DIR_DER); c->el[k].val.v = 1e3; c->el[k].val.est = M_DATO;
    k = circ_elemento(c, EL_FUENTE, 3, 2, DIR_ARR);
    c->el[k].variante = FORMA_FASOR; c->el[k].val.v = 1.0; c->el[k].val.est = M_DATO;
    k = circ_elemento(c, EL_CORR, 5, 2, DIR_ARR);
    c->el[k].variante = FORMA_FASOR; c->el[k].val.v = 1e-3; c->el[k].val.est = M_DATO;
    k = circ_elemento(c, EL_COND, 2, 1, DIR_ABA); c->el[k].val.v = 1e-6; c->el[k].val.est = M_DATO;
    k = circ_elemento(c, EL_RES, 3, 1, DIR_IZQ); c->el[k].val.v = 1e3; c->el[k].val.est = M_DATO;
    k = circ_elemento(c, EL_RES, 5, 1, DIR_IZQ); c->el[k].val.v = 1e3; c->el[k].val.est = M_DATO;
    r4_cab(c, 1, 1, 2, 1); r4_cab(c, 4, 1, 3, 1); r4_cab(c, 0, 2, 5, 2);
    c->frec.v = 1e3; c->frec.est = M_DATO;
    c->tierra_x = 0; c->tierra_y = 2;
    c->sonda[0].tipo = SONDA_NODO; c->sonda[0].x = 2; c->sonda[0].y = 1; c->sonda[0].el = -1; c->sonda[0].sg = 0;
}

/* Del Bode al editor, el cursor a (x, y) y [del] hacia la derecha o arriba */
static void r3v_borra(int x, int y, int arriba)
{
    int j;
    TECLAS(T_ESC);
    for (j = 0; j < 12; j++) TECLAS(T_IZQ);
    for (j = 0; j < 8; j++) TECLAS(T_ARRIBA);
    for (j = 0; j < x; j++) TECLAS(T_DER);
    for (j = 0; j < y; j++) TECLAS(T_ABAJO);
    TECLAS(T_BORRAR);
    if (arriba) TECLAS(T_ARRIBA); else TECLAS(T_DER);
}

static void r3v_bode(void)
{
    static Circuito c;
    const char *q = "H2 entrada del Bode";
    r3v_bode_circ(&c);
    chk_cond(q, "el circuito es v" S_aa "lido", circ_valido(&c) && c.nel == 8);
    if (!r3u_abre(&c)) { chk_cond(q, "se abre en el editor", 0); return; }
    TECLAS(T_LETRA('B'));
    r4_espera();
    printf("  %s: [B] entrada «%s»\n", q, bode_res()->nom_ent);
    chk_cond(q, "[B]: la autom" S_aa "tica, E1", ui_pantalla() == P_BODE && strcmp(bode_res()->nom_ent, "E1") == 0);
    TECLAS(T_LETRA('F'));
    r4_espera();
    chk_cond(q, "[F] elige E2", ui_pantalla() == P_BODE && strcmp(bode_res()->nom_ent, "E2") == 0);
    r3v_borra(8, 0, 0);                                 /* R1, delante de E2 */
    printf("  %s: R1 borrada: nel %d; E2 en el %d\n", q, ui_circuito()->nel, r4_el(ui_circuito(), "E2"));
    chk_cond(q, "R1 borrada (7 elementos)", ui_pantalla() == P_EDITOR && ui_circuito()->nel == 7 &&
             r4_el(ui_circuito(), "R1") < 0 && r4_el(ui_circuito(), "E2") == 2);
    TECLAS(T_LETRA('B'));
    r4_espera();
    printf("  %s: [B] tras borrar R1: entrada «%s»\n", q, bode_res()->nom_ent);
    chk_cond(q, "tras borrar R1, la entrada sigue siendo E2", ui_pantalla() == P_BODE &&
             strcmp(bode_res()->nom_ent, "E2") == 0 && bode_res()->ent == 2);
    r3v_borra(3, 2, 1);                                 /* E2 misma */
    chk_cond(q, "E2 borrada", ui_pantalla() == P_EDITOR && ui_circuito()->nel == 6 && r4_el(ui_circuito(), "E2") < 0);
    TECLAS(T_LETRA('B'));
    r4_espera();
    printf("  %s: [B] tras borrar E2: entrada «%s»\n", q, bode_res()->nom_ent);
    chk_cond(q, "sin E2, vuelve la autom" S_aa "tica (E1), no la que qued" S_oo " en su sitio",
             ui_pantalla() == P_BODE && strcmp(bode_res()->nom_ent, "E1") == 0);
}

/* ---- H3. El ruido de redondeo al final de la emulación ---- */

/* El modelo i (0..41) de la biblioteca, abierto desde Aprende con [enter] */
static void r3v_modelo(int i)
{
    rv_al_menu();
    rv_modelos(i < 32 ? "12." : "13.");
    repite(T_ABAJO, i < 32 ? i - 22 : i - 32);
    TECLAS(T_ENTER);
    r4_espera();
}

/* La capa que se ve (por su nombre en la barra) y el paso a otra con [der] */
static int r3v_en_capa(int capa)
{
    char t[24];
    sprintf(t, "\n%s\n", esq_capa_nombre(capa));
    return ve(t);
}

static const char *r3v_capa_vista(void)
{
    int k;
    for (k = 0; k < CAPA_N && !r3v_en_capa(k); k++) ;
    return k < CAPA_N ? esq_capa_nombre(k) : "?";
}

static int r3v_a_capa(int capa)
{
    int j;
    for (j = 0; j < CAPA_N && !r3v_en_capa(capa); j++) TECLAS(T_DER);
    return r3v_en_capa(capa);
}

static void r3v_ruido(void)
{
    static Circuito c;
    int i, j, capa, nruido = 0;
    const char *q = "H3 ruido al final";

    ui_init();
    TECLAS(T_ENTER);
    for (i = 22; i < 42; i++) {
        int ruido_esq = 0, menos0 = 0;
        const SimRes *s;
        char d[96], t[24];
        r3v_modelo(i);
        if (ui_pantalla() != P_OSC) continue;              /* los que se emulan */
        s = sim_res();
        modelo_cargar(i, &c);
        for (j = 0; j < c.nel; j++)
            if ((c.el[j].tipo == EL_FUENTE || c.el[j].tipo == EL_CORR) && fabs(s->ultimo.val[j]) <= 1e-12 * fabs(s->Vmax[j])) {
                printf("    modelo %d: %s(t_fin) = %.3g (m" S_aa "x. %.3g)\n", i + 1, c.el[j].etiq, s->ultimo.val[j], s->Vmax[j]);
                nruido++;
            }
        TECLAS(T_TAB);                                   /* P_EMU_TABLA, «Al final» */
        if (ui_pantalla() == P_EMU_TABLA) menos0 = ve("-0 V") || ve("-0 A") || ve("-0 W");
        TECLAS(T_TAB);                                   /* P_EMU_ESQ, sus seis capas */
        for (capa = 0; capa < CAPA_N; capa++) {
            if (ui_pantalla() == P_EMU_ESQ && ve(",0000")) {
                ruido_esq = 1;
                printf("    modelo %d: P_EMU_ESQ, %s: un r" S_oo "tulo con ruido\n", i + 1, r3v_capa_vista());
            }
            TECLAS(T_DER);
        }
        sprintf(d, "modelo %d: P_EMU_ESQ sin r" S_oo "tulos de ruido («-0,000015nV»)", i + 1);
        chk_cond(q, d, ui_pantalla() == P_EMU_ESQ && !ruido_esq);
        sprintf(d, "modelo %d: P_EMU_TABLA sin «-0»", i + 1);
        chk_cond(q, d, !menos0);
        if (i != 26) continue;
        /* El 27 (onda completa con dos fuentes): E1 se rotula 0 y la fuga de D1
         * en inversa (pA frente a 0,38 A: no es ruido) se queda como está */
        j = r4_el(&c, "D1");
        num_corto(t, sizeof t, fabs(s->ultimo.I[j]));
        strcat(t, "A");
        sprintf(d, "\n%s\n", t);
        printf("    modelo 27: I(D1) = %.3g A (m" S_aa "x. %.3g): «%s»\n", s->ultimo.I[j], s->Imax[j], t);
        chk_cond(q, "modelo 27: E1 y E2 se rotulan 0V", r3v_a_capa(CAPA_VALORES) && ve("\n0V\n"));
        chk_cond(q, "modelo 27: I(D1) no es ruido, y no se rotula 0", r3v_a_capa(CAPA_CORRIENTES) &&
                 fabs(s->ultimo.I[j]) > 1e-12 * fabs(s->Imax[j]) && ve(d) && strcmp(t, "0nA") != 0 && strcmp(t, "0A") != 0);
    }
    chk_cond(q, "hay modelos con e(t_fin) de ruido (el caso del hallazgo)", nruido >= 5);
}

/* Un potencial de -1E-20 V (el redondeo de un 0; la tabla o el punto de
 * operación) se rotula sin «-0»; uno negativo de verdad, con su signo */
static void r3v_potencial(void)
{
    static Circuito c;
    static Red r;
    static Valores val;
    Marco m;
    Vista v;
    const char *q = "H3 potenciales";
    rv_ctl(&c, EL_JDEP, DEP_V, 1e-3);
    red_extraer(&c, &r);
    memset(&val, 0, sizeof val);
    val.phi[0] = -2.5;                                   /* (1,1): un potencial negativo */
    val.phi[1] = -1e-20;                                 /* (2,1): un cero con ruido */
    esq_vista(&v);
    v.capa = CAPA_POTENCIALES; v.val = &val; v.red = &r;
    esq_marco_editor(&m);
    gfx_clip_reset();
    gfx_clear(C_FONDO);
    gfx_textos_reset();
    esq_dibuja(&c, &m, &v);
    printf("  %s: «-2,5V» %d, «-0» %d\n", q, gfx_texto_visto("-2,5V\n"), gfx_texto_visto("-0"));
    chk_cond(q, "-2,5V con su signo y el -1E-20 sin «-0»", r.nn >= 3 && gfx_texto_visto("-2,5V\n") && !gfx_texto_visto("-0"));
}

/* ---- H4. Un [?!] en una C con un no lineal ---- */
static void r3v_estado(void)
{
    static Circuito c;
    static Red r;
    static Solucion s;
    char msg[240];
    int k, conR;
    const char *q = "H4 [?!] en una C";

    for (conR = 0; conR < 2; conR++) {
        k = rv2_led_c(&c);
        c.el[k].val.est = M_BUSCAR;
        if (conR) c.el[r4_el(&c, "R1")].val.est = M_BUSCAR;
        red_extraer(&c, &r);
        resolver(&c, &r, &s);
        sol_mensaje(&c, &r, &s, msg, sizeof msg);
        printf("  %s%s: tabla %d: %s\n", q, conR ? " y en R1" : "", s.estado, msg);
        chk_cond(q, conR ? "C1 y R1: SOL_NO_LINEAL manda a [I]" : "C1: SOL_NO_LINEAL no manda a [I]",
                 s.estado == SOL_NO_LINEAL && (strstr(msg, "[I]") != 0) == conR);
        if (!r3u_abre(&c)) { chk_cond(q, "se abre en el editor", 0); continue; }
        printf("      editor: «[I] barre» %d, «quita el [?!] de C1» %d\n", ve("[I] barre"), ve("quita el [?!] de C1"));
        chk_cond(q, conR ? "C1 y R1: la l" S_ii "nea de estado manda a [I]" : "C1: la l" S_ii "nea de estado no manda a [I]",
                 ui_pantalla() == P_EDITOR && ve(conR ? "Con D1 no se despeja: [I] barre y [N] lee" :
                                                        "Con D1 no se despeja: quita el [?!] de C1") && ve("[I] barre") == conR);
        TECLAS(T_IGUAL);
        chk_cond(q, conR ? "C1 y R1: [=], el texto y el pie con [I]" : "C1: [=], ni el texto ni el pie con [I]",
                 ui_pantalla() == P_RESULTADO && ve("[I]") == conR && ve("[esc] volver y corregirlo"));
    }
    chk_cond(q, "la l" S_ii "nea nueva cabe en 52 columnas con nombres de 3",
             strlen("Con D32 no se despeja: quita el [?!] de C32") <= 52);
}

static void test_rev3_modelo(void)
{
    printf("[65t] Etapa 4: los hallazgos del grupo modelo (rev3)\n");
    gfx_desbordes_reset();
    r3v_fuentes_ctl();
    r3v_bode();
    r3v_ruido();
    r3v_potencial();
    r3v_estado();
    chk_cond("interfaz", "ning" S_uu "n texto fuera de la pantalla", gfx_desbordes() == 0);
}
