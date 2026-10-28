/* --------------------------------------------------------------------------
 *  65m. Etapa 4: hallazgos de la segunda revisión (el esquema y los avisos
 *       y errores de P_EMU_ESQ)
 *
 *  Una prueba por hallazgo arreglado (cada una falla sin su arreglo,
 *  comprobado por mutación): el aviso SATURA con el nombre de otro
 *  elemento, los errores de la emulación que no llega a arrancar («?» o
 *  sin nombre), la segunda línea de un diodo en la fila de abajo cortada
 *  por la caja y el rótulo del LED del modelo 41 sobre R2. Se incluye tras
 *  test_e4_rev.inc (usa r4_led, r4_espera, r4_el, r3u_abre, rv_al_menu y
 *  rv_modelos).
 * ----------------------------------------------------------------------- */

/* Hasta P_EMU_ESQ con [tab] (desde el osciloscopio o las tablas) */
static void rd2_al_esq(void)
{
    int j;
    for (j = 0; j < 8 && ui_pantalla() != P_EMU_ESQ; j++) TECLAS(T_TAB);
}

/* Una emulación de 3 elementos (E1 senoidal, R1 y el LED): la SimRed de la
 * última emulación queda con nel = 3 */
static void rd2_emula_pequeno(Circuito *c)
{
    int k;
    r4_led(c, 9.0, 330.0);
    k = r4_el(c, "E1");
    c->el[k].variante = ONDA_SENO;
    c->el[k].par[Q_FREC - Q_P1].v = 50.0; c->el[k].par[Q_FREC - Q_P1].est = M_DATO;
    c->el[k].par[Q_BASE - Q_P1].v = 0.0; c->el[k].par[Q_BASE - Q_P1].est = M_DATO;
    r3u_abre(c);
    TECLAS(T_IGUAL);
    r4_espera();
}

/* El esquema de c resuelto (val) como en P_RESULTADO y P_EMU_ESQ: capa
 * Valores, el brillo de los LED y las sondas, en el marco ajustado a
 * (0, y, SCR_W, h), sobre el fondo y sin recortar. Con rotulos = 0, la
 * capa de potenciales sin red: los mismos símbolos sin ningún rótulo */
static void rd2_dibuja(const Circuito *c, const Valores *val, int y, int h, int rotulos, Marco *m)
{
    static unsigned char br[MAX_EL];
    Vista v;
    esq_marco_ajustado(c, 0, y, SCR_W, h, m);
    esq_brillos(c, val->I, br);
    esq_vista(&v);
    v.capa = (unsigned char)(rotulos ? CAPA_VALORES : CAPA_POTENCIALES);
    v.val = val;
    v.brillo = br;
    v.sondas = 1;
    gfx_clip_reset();
    gfx_clear(C_FONDO);
    esq_dibuja(c, m, &v);
}

/* Píxeles del esquema resuelto que caen fuera de su caja (las filas de y a
 * y + h - 1): lo que el recorte de la pantalla cortaría */
static int rd2_fuera(const Circuito *c, const Valores *val, int y, int h)
{
    const u16 *fb = gfx_buffer();
    Marco m;
    int i, n = 0;
    rd2_dibuja(c, val, y, h, 1, &m);
    for (i = 0; i < SCR_W * SCR_H; i++)
        if ((i / SCR_W < y || i / SCR_W >= y + h) && fb[i] != C_FONDO) n++;
    return n;
}

/* Píxeles del símbolo del elemento horizontal k (su mitad de abajo, sin los
 * extremos) que los rótulos tapan: con rótulos contra sin ellos */
static int rd2_tapado(const Circuito *c, const Valores *val, int k, int y, int h)
{
    static u16 sin[SCR_W * SCR_H];
    const u16 *fb = gfx_buffer();
    Marco m;
    int xa, ya, xb, yb, x, yy, n = 0;
    rd2_dibuja(c, val, y, h, 0, &m);
    memcpy(sin, fb, sizeof sin);
    rd2_dibuja(c, val, y, h, 1, &m);
    esq_pt(&m, c->el[k].xa, c->el[k].ya, &xa, &ya);
    esq_pt(&m, c->el[k].xb, c->el[k].yb, &xb, &yb);
    if (xa > xb) { x = xa; xa = xb; xb = x; }
    for (yy = ya - 1; yy <= ya + 6; yy++)
        for (x = xa + 6; x <= xb - 6; x++)
            if (sin[yy * SCR_W + x] != C_FONDO && fb[yy * SCR_W + x] != sin[yy * SCR_W + x]) n++;
    return n;
}

static void test_rev2_dibujo_a(void)
{
    static Circuito c;
    static Red r;
    static Valores vs;
    SimDiag d;
    SimNewton nw;
    char t[64];
    int k, j, fuera;

    printf("[65m] Etapa 4: el esquema y los avisos de P_EMU_ESQ (revisi" S_oo "n 2)\n");
    gfx_desbordes_reset();

    /* ---- SATURA nombra al AO aunque un C haya avisado antes (CI_SALTA) ---- */
    modelo_cargar(37, &c);                                /* el 38: el comparador (E1, E2, U1) */
    k = circ_elemento(&c, EL_COND, 0, 4, DIR_ARR);        /* C1 sobre la senoidal, con V0 = 2 V */
    c.el[k].val.v = 1e-6; c.el[k].val.est = M_DATO;
    c.el[k].par[Q_V0 - Q_P1].v = 2.0; c.el[k].par[Q_V0 - Q_P1].est = M_DATO;
    circ_cable(&c, 0, 3, DIR_DER); circ_cable(&c, 0, 4, DIR_DER);
    chk_cond("SATURA", "(abierto)", circ_valido(&c) && r3u_abre(&c));
    TECLAS(T_IGUAL);
    r4_espera();
    rd2_al_esq();
    chk_cond("SATURA", "P_EMU_ESQ: CI_SALTA de C1 y SATURA de U1",
             ui_pantalla() == P_EMU_ESQ && (sim_res()->d.avisos & SIM_AV_CI_SALTA) &&
             (sim_res()->d.avisos & SIM_AV_SATURA));
    chk_cond("SATURA", "P_EMU_ESQ: «U1 satura», no «C1 satura»",
             ve("U1 satura en -15 V: la salida ya no sigue.") && !ve("C1 satura"));
    TECLAS(T_ESC, T_LETRA('P'));
    chk_cond("SATURA", "[P]: «U1 satura», no «C1 satura»", ui_pantalla() == P_RESULTADO &&
             ve("U1 satura") && !ve("C1 satura"));

    /* ---- Los errores que no dejan arrancar la emulación, con su nombre
     *      (tras emular un circuito de 3 elementos) ---- */
    /* E1 10 V, R1, C1, R2 y E2 controlada sin control (5 elementos) */
    rd2_emula_pequeno(&c);
    circ_init(&c, "E2 sin control");
    k = circ_elemento(&c, EL_FUENTE, 1, 2, DIR_ARR); c.el[k].val.v = 10.0; c.el[k].val.est = M_DATO;
    k = circ_elemento(&c, EL_RES, 1, 1, DIR_DER);    c.el[k].val.v = 1e3; c.el[k].val.est = M_DATO;
    k = circ_elemento(&c, EL_COND, 2, 1, DIR_ABA);   c.el[k].val.v = 1e-6; c.el[k].val.est = M_DATO;
    k = circ_elemento(&c, EL_RES, 2, 1, DIR_DER);    c.el[k].val.v = 1e3; c.el[k].val.est = M_DATO;
    k = circ_elemento(&c, EL_EDEP, 3, 1, DIR_ABA);   c.el[k].variante = DEP_V; c.el[k].ref = -1;
    c.el[k].val.v = 2.0; c.el[k].val.est = M_DATO;
    circ_cable(&c, 2, 2, DIR_IZQ); circ_cable(&c, 3, 2, DIR_IZQ);
    c.tierra_x = 1; c.tierra_y = 2;
    chk_cond("errores", "(abierto)", c.nel == 5 && r3u_abre(&c));
    TECLAS(T_IGUAL);
    chk_cond("errores", "[=]: P_EMU_ESQ con «E2 es una fuente controlada sin control»",
             ui_pantalla() == P_EMU_ESQ && sim_res()->d.estado == SIM_CTL_MAL &&
             ve("E2 es una fuente controlada sin control") && !ve("? es una"));
    /* el astable sin tierra: U1 sin referencia */
    rd2_emula_pequeno(&c);
    modelo_cargar(38, &c);                                /* el 39: el astable */
    c.tierra_x = c.tierra_y = -1;
    chk_cond("errores", "(abierto)", r3u_abre(&c));
    TECLAS(T_IGUAL);
    chk_cond("errores", "[=]: P_EMU_ESQ con «U1 no tiene referencia»",
             ui_pantalla() == P_EMU_ESQ && sim_res()->d.estado == SIM_AO_REF &&
             ve("U1 no tiene referencia") && !ve("Un AO no tiene"));

    /* ---- Los modelos de la Etapa 4 resueltos (punto de operación o al
     *      final de la emulación), en la caja de P_RESULTADO y P_EMU_ESQ
     *      (0, 16, 320, 142): nada se sale (el 27: la V de D2, en la fila
     *      de abajo) ---- */
    for (k = 22; k < N_MODELOS; k++) {
        modelo_cargar(k, &c);
        red_extraer(&c, &r);
        if (MODELOS[k].ca == 3) {
            memset(&nw, 0, sizeof nw);
            if (sim_punto_op_cc(&c, &r, &vs, &d, &nw) != SIM_OK) { fuera = -1; }
            else fuera = rd2_fuera(&c, &vs, 16, 142);
        } else if (MODELOS[k].ca == 0) {
            if (sim_iniciar(&c, &r) != SIM_OK) fuera = -1;
            else {
                for (j = 0; j < 100000 && sim_avanzar(10000000ul); j++) {}
                fuera = rd2_fuera(&c, &sim_res()->ultimo, 16, 142);
            }
        } else continue;                                  /* los barridos: sin esquema resuelto */
        sprintf(t, "el %d resuelto: nada fuera de su caja", k + 1);
        pruebas++;
        if (fuera != 0) falla("caja del esquema", t, fuera, 0);
    }

    /* ---- El 41: el rótulo del LED (a su derecha, dos líneas) no tapa R2 ---- */
    modelo_cargar(40, &c);
    red_extraer(&c, &r);
    memset(&nw, 0, sizeof nw);
    k = r4_el(&c, "R2");
    chk_cond("modelo 41", "el punto de operaci" S_oo "n", k >= 0 && sim_punto_op_cc(&c, &r, &vs, &d, &nw) == SIM_OK);
    j = k >= 0 ? rd2_tapado(&c, &vs, k, 16, 142) : -1;
    pruebas++;
    if (j != 0) falla("modelo 41", "R2 entera: ning" S_uu "n r" S_oo "tulo la tapa", j, 0);
    gfx_clear(C_FONDO);

    chk_cond("interfaz", "ning" S_uu "n texto fuera de la pantalla", gfx_desbordes() == 0);
}
