/* --------------------------------------------------------------------------
 *  65e. Etapa 4: símbolos, rótulos e iconos (esquema.c, paso 11)
 *
 *  Se dibuja con esq_dibuja directamente, sin la interfaz: los iconos en su
 *  celda de 20×14, el brillo del LED (esq_led_nivel, esq_mezcla), las patas
 *  del AO del inversor (§6.2) y los puntos interiores, los 3 patas en sus 8
 *  orientaciones, el rombo, la marca roja, la línea del control, las
 *  flechas del cursor al colocar, los rótulos sin resolver y resueltos, y
 *  dos BMP (out/e4_esq_*.bmp: fuera de la numeración de vuelca) de un
 *  circuito con un elemento de cada tipo nuevo.
 *  Se incluye tras test_e4_paso.inc (usa e4_inversor de test_e4_modelo.inc).
 * ----------------------------------------------------------------------- */

static u16 es_px(int x, int y) { return gfx_buffer()[y * SCR_W + x]; }

static void es_vuelca(const char *nombre)
{
    char ruta[256];
    sprintf(ruta, "%s/e4_esq_%s.bmp", dir_out, nombre);
    gfx_dump_bmp(ruta);
}

/* Dibuja c en el marco del editor sobre el fondo, con la vista v */
static void es_dibuja(const Circuito *c, const Vista *v)
{
    Marco m;
    gfx_clip_reset();
    gfx_clear(C_FONDO);
    esq_marco_editor(&m);
    gfx_textos_reset();
    esq_dibuja(c, &m, v);
}

/* Una vista sin rótulos (la capa de potenciales sin solución no escribe
 * nada): para mirar los símbolos píxel a píxel */
static void es_vista_sola(Vista *v)
{
    esq_vista(v);
    v->capa = CAPA_POTENCIALES;
}

/* ¿Todos los píxeles del rectángulo [x0, x1] × [y0, y1] son de color c? */
static int es_todo(int x0, int y0, int x1, int y1, u16 c)
{
    int x, y;
    for (y = y0; y <= y1; y++)
        for (x = x0; x <= x1; x++) if (es_px(x, y) != c) return 0;
    return 1;
}

static int es_hay(u16 c)
{
    const u16 *fb = gfx_buffer();
    int i;
    for (i = 0; i < SCR_W * SCR_H; i++) if (fb[i] == c) return 1;
    return 0;
}

static void es_val(Circuito *c, int k, double v)
{
    if (k < 0) return;
    c->el[k].val.v = v;
    c->el[k].val.est = M_DATO;
}

/* E1 9 V, R1 330 Ω y el LED D1 horizontal de (2,2) a (3,2): su triángulo
 * tiene la base en x = 91 (y de 74 a 84) y la punta en x = 100 */
static int es_led(Circuito *c, int var)
{
    int k;
    circ_init(c, "LED");
    es_val(c, circ_elemento(c, EL_FUENTE, 1, 3, DIR_ARR), 9.0);
    es_val(c, circ_elemento(c, EL_RES, 1, 2, DIR_DER), 330.0);
    k = circ_elemento(c, EL_LED, 2, 2, DIR_DER);
    if (k >= 0) c->el[k].variante = (unsigned char)var;
    circ_cable(c, 3, 2, DIR_ABA); circ_cable(c, 3, 3, DIR_IZQ); circ_cable(c, 2, 3, DIR_IZQ);
    return k;
}

/* Un elemento de cada tipo nuevo (los índices, en el orden de colocación):
 * el inversor con U1 (→ en (1,1): - (1,1), + (1,3), salida (3,2)) y la
 * tierra en (1,4); E2 9 V, R4 100 Ω, el LED D1, el diodo D2 y el zener D3 al
 * revés (columnas 6 y 7); E3 = 2·V(R1) sobre R5 (filas 5 y 6) y J1 =
 * 0,5·I(R4) sobre R6 (columnas 8 y 9). Con tres, además un BJT (↓ en (1,5)) y un MOSFET (↑ en
 * (4,6)): se dibujan, pero no se resuelven hasta los pasos 13 y 14. */
enum { T_E1 = 0, T_R1, T_U1, T_RF, T_RL, T_E2, T_R2, T_LED, T_D, T_Z, T_EC, T_R3, T_JC, T_R4, T_Q, T_M };

static void es_todos(Circuito *c, int tres)
{
    int x, k;
    circ_init(c, "todos");
    es_val(c, circ_elemento(c, EL_FUENTE, 0, 2, DIR_ARR), 1.0);
    es_val(c, circ_elemento(c, EL_RES, 0, 1, DIR_DER), 1e3);
    circ_elemento(c, EL_AO, 1, 1, DIR_DER);
    es_val(c, circ_elemento(c, EL_RES, 1, 0, DIR_DER), 10e3);
    es_val(c, circ_elemento(c, EL_RES, 4, 2, DIR_ABA), 2e3);
    circ_cable(c, 1, 1, DIR_ARR);
    circ_cable(c, 2, 0, DIR_DER); circ_cable(c, 3, 0, DIR_DER);
    circ_cable(c, 4, 0, DIR_ABA); circ_cable(c, 4, 1, DIR_ABA);
    circ_cable(c, 3, 2, DIR_DER);
    circ_cable(c, 4, 3, DIR_ABA);
    for (x = 1; x < 4; x++) circ_cable(c, x, 4, DIR_DER);
    circ_cable(c, 0, 2, DIR_ABA); circ_cable(c, 0, 3, DIR_ABA); circ_cable(c, 0, 4, DIR_DER);
    circ_cable(c, 1, 3, DIR_ABA);
    c->tierra_x = 1; c->tierra_y = 4;
    es_val(c, circ_elemento(c, EL_FUENTE, 6, 4, DIR_ARR), 9.0);
    es_val(c, circ_elemento(c, EL_RES, 6, 3, DIR_ARR), 100.0);
    circ_elemento(c, EL_LED, 6, 2, DIR_ARR);
    circ_elemento(c, EL_DIODO, 6, 1, DIR_ARR);
    circ_elemento(c, EL_ZENER, 7, 1, DIR_ARR);          /* cátodo arriba, en (7,0) */
    circ_cable(c, 6, 0, DIR_DER);
    circ_cable(c, 7, 1, DIR_ABA); circ_cable(c, 7, 2, DIR_ABA); circ_cable(c, 7, 3, DIR_ABA);
    circ_cable(c, 6, 4, DIR_DER);
    k = circ_elemento(c, EL_EDEP, 6, 5, DIR_DER);
    es_val(c, k, 2.0);
    if (k >= 0) c->el[k].ref = T_R1;
    es_val(c, circ_elemento(c, EL_RES, 7, 5, DIR_ABA), 1e3);
    circ_cable(c, 7, 6, DIR_IZQ); circ_cable(c, 6, 6, DIR_ARR);
    k = circ_elemento(c, EL_JDEP, 8, 2, DIR_DER);
    es_val(c, k, 0.5);
    if (k >= 0) { c->el[k].ref = T_R2; c->el[k].variante = DEP_I; }
    circ_cable(c, 9, 2, DIR_ABA); circ_cable(c, 9, 3, DIR_ABA);
    es_val(c, circ_elemento(c, EL_RES, 9, 4, DIR_IZQ), 1e3);
    circ_cable(c, 8, 4, DIR_ARR); circ_cable(c, 8, 3, DIR_ARR);
    if (tres) {
        circ_elemento(c, EL_BJT, 1, 5, DIR_ABA);
        circ_elemento(c, EL_MOS, 4, 6, DIR_ARR);
    }
}

static void test_e4_esq(void)
{
    static Circuito c;
    static Red r;
    static Valores vs;
    static u16 copia[SCR_W * SCR_H];
    static unsigned char br[MAX_EL];
    Vista v;
    Marco m;
    int k, i;

    printf("[65e] Etapa 4: s" S_ii "mbolos, r" S_oo "tulos e iconos\n");
    gfx_desbordes_reset();

    /* ---- Los iconos: ningún píxel fuera de su celda de 20×14 ---- */
    {
        static const int ICO[8] = { ICO_DIODO, ICO_ZENER, ICO_LED, ICO_EDEP, ICO_JDEP, ICO_AO, ICO_BJT, ICO_MOS };
        static const char *const NOM[8] = { "diodo", "zener", "LED", "E controlada", "J controlada",
                                            "AO", "BJT", "MOSFET" };
        const char *q = "iconos";
        const u16 F = C_BARRA;
        chk_cond(q, "ICO_DIODO a ICO_MOS siguen a ICO_VATI, en orden",
                 ICO_DIODO == ICO_VATI + 1 && ICO_ZENER == ICO_VATI + 2 && ICO_LED == ICO_VATI + 3 &&
                 ICO_EDEP == ICO_VATI + 4 && ICO_JDEP == ICO_VATI + 5 && ICO_AO == ICO_VATI + 6 &&
                 ICO_BJT == ICO_VATI + 7 && ICO_MOS == ICO_VATI + 8);
        for (i = 0; i < 8; i++) {
            const u16 *fb;
            int x, y, fuera = 0, dentro = 0;
            char d[96];
            gfx_clip_reset();
            gfx_clear(F);
            esq_icono(ICO[i], 100, 100, C_TEXTO);
            fb = gfx_buffer();
            for (y = 0; y < SCR_H; y++)
                for (x = 0; x < SCR_W; x++) {
                    int en = x >= 100 && x <= 119 && y >= 100 && y <= 113;
                    if (fb[y * SCR_W + x] == F) continue;
                    if (en) dentro++; else fuera++;
                }
            sprintf(d, "%s: ning" S_uu "n p" S_ii "xel fuera de su celda (%d)", NOM[i], fuera);
            chk_cond(q, d, fuera == 0);
            sprintf(d, "%s: se dibuja (%d p" S_ii "xeles)", NOM[i], dentro);
            chk_cond(q, d, dentro >= 30);
        }
        /* y en la esquina de la pantalla, con otro color: tampoco se sale */
        gfx_clear(F);
        for (i = 0; i < 8; i++) esq_icono(ICO[i], 20 * i, 0, C_CURSOR);
        chk_cond(q, "los ocho en fila en y = 0: nada por debajo de la fila 13", es_todo(0, 14, SCR_W - 1, 30, F));
        chk_cond(q, "ni a la derecha de x = 159", es_todo(160, 0, SCR_W - 1, 13, F));
        /* la BMP: todos los iconos en celdas de 20×14 (las de la Etapa 4, en
         * su color de tipo) */
        gfx_clear(C_PANEL);
        for (i = 0; i <= ICO_MOS; i++) {
            int x = 8 + 38 * (i % 8), y = 8 + 24 * (i / 8);
            gfx_fill(x, y, 20, 14, C_FONDO);
            esq_icono(i, x, y, i >= ICO_DIODO ? tipo_info(EL_DIODO + i - ICO_DIODO)->color : C_TEXTO);
        }
        es_vuelca("iconos");
    }

    /* ---- El brillo del LED: esq_led_nivel en sus bordes y esq_mezcla ---- */
    {
        const char *q = "brillo del LED";
        const double If = 1.0, Im = 1.5;
        chk_cond(q, "I < If/100: apagado (0)", esq_led_nivel(0.0099, If, Im) == 0);
        chk_cond(q, "I = If/100: nivel 1 (8" S_CDOT "I/If = 0,08 sube a 1)", esq_led_nivel(0.01, If, Im) == 1);
        chk_cond(q, "negativa y NaN: 0", esq_led_nivel(-0.5, If, Im) == 0 && esq_led_nivel(sqrt(-1.0), If, Im) == 0);
        chk_cond(q, "8" S_CDOT "I/If = 2,5 exacto: redondea a 3", esq_led_nivel(0.3125, If, Im) == 3);
        chk_cond(q, "un poco menos: 2", esq_led_nivel(0.3124, If, Im) == 2);
        chk_cond(q, "I = If/2: 4", esq_led_nivel(0.5, If, Im) == 4);
        chk_cond(q, "I = If: 8", esq_led_nivel(If, If, Im) == 8);
        chk_cond(q, "I = Im: 8 (no pasa de Im)", esq_led_nivel(Im, If, Im) == 8);
        chk_cond(q, "I > Im: 9 (se quemar" S_ii "a)", esq_led_nivel(1.5000001, If, Im) == 9);
        chk_cond(q, "If = 20 mA, Im = 30 mA: 14,5 mA da 6", esq_led_nivel(14.5e-3, 20e-3, 30e-3) == 6);
        chk_cond(q, "If no positiva: 0", esq_led_nivel(1.0, 0.0, 30e-3) == 0);
    }
    {
        const char *q = "esq_mezcla";
        u16 n = RGB(0, 0, 0), b = RGB(255, 255, 255), col = NL_COLOR[LED_ROJO].rgb;
        chk_cond(q, "t = 0: el fondo", esq_mezcla(C_FONDO, col, 0) == C_FONDO);
        chk_cond(q, "t = 256: el color", esq_mezcla(C_FONDO, col, 256) == col);
        chk_cond(q, "fuera de 0..256: recortada", esq_mezcla(C_FONDO, col, -5) == C_FONDO && esq_mezcla(C_FONDO, col, 999) == col);
        chk_cond(q, "negro y blanco a medias: (15, 31, 15)", esq_mezcla(n, b, 128) == (u16)((15 << 11) | (31 << 5) | 15));
        {
            int t = 150, s = 256 - t;
            int rr_ = (((C_FONDO >> 11) & 31) * s + ((col >> 11) & 31) * t) / 256;
            int gg = (((C_FONDO >> 5) & 63) * s + ((col >> 5) & 63) * t) / 256;
            int bb = ((C_FONDO & 31) * s + (col & 31) * t) / 256;
            chk_cond(q, "t = 150 por canal: (f" S_CDOT "(256 - t) + c" S_CDOT "t)/256",
                     esq_mezcla(C_FONDO, col, t) == (u16)((rr_ << 11) | (gg << 5) | bb));
        }
    }

    /* ---- El LED rojo dibujado: el centro del triángulo es (94,79) ---- */
    {
        const char *q = "LED rojo dibujado";
        const u16 col = NL_COLOR[LED_ROJO].rgb;
        int kd = es_led(&c, LED_ROJO);
        chk_cond(q, "se coloca", kd == 2 && circ_valido(&c));
        memset(br, 0, sizeof br);
        es_vista_sola(&v);
        v.brillo = br;
        br[kd] = 8;
        es_dibuja(&c, &v);
        chk_cond(q, "nivel 8: el centro del tri" S_aa "ngulo = esq_mezcla(C_FONDO, rojo, 256) = el rojo",
                 es_px(94, 79) == esq_mezcla(C_FONDO, col, 256) && es_px(94, 79) == col);
        chk_cond(q, "nivel 8: el contorno, en su color", es_px(91, 76) == col);
        chk_cond(q, "nivel 8: dos halos (r 14 y 12) en esq_mezcla(C_FONDO, rojo, 128)",
                 es_px(96, 65) == esq_mezcla(C_FONDO, col, 128) && es_px(96, 67) == esq_mezcla(C_FONDO, col, 128));
        chk_cond(q, "los hilos y la raya, en C_DIODO", es_px(84, 79) == C_DIODO && es_px(100, 76) == C_DIODO);
        br[kd] = 4;
        es_dibuja(&c, &v);
        chk_cond(q, "nivel 4: relleno en esq_mezcla(C_FONDO, rojo, 64 + 24" S_CDOT "4)", es_px(94, 79) == esq_mezcla(C_FONDO, col, 160));
        chk_cond(q, "nivel 4: un halo de radio 12 en esq_mezcla(C_FONDO, rojo, 64)",
                 es_px(96, 67) == esq_mezcla(C_FONDO, col, 64) && es_px(96, 65) == C_FONDO);
        br[kd] = 3;
        es_dibuja(&c, &v);
        chk_cond(q, "nivel 3: sin halo", es_px(96, 67) == C_FONDO && es_px(96, 68) == C_FONDO &&
                 es_px(94, 79) == esq_mezcla(C_FONDO, col, 64 + 72));
        br[kd] = 0;
        es_dibuja(&c, &v);
        chk_cond(q, "nivel 0: hueco, con el contorno en esq_mezcla(C_FONDO, rojo, 150)",
                 es_px(94, 79) == C_FONDO && es_px(91, 76) == esq_mezcla(C_FONDO, col, 150));
        chk_cond(q, "nivel 0: las flechitas debajo, apagadas",
                 es_px(93, 85) == esq_mezcla(C_FONDO, col, 150) && es_px(97, 85) == esq_mezcla(C_FONDO, col, 150));
        br[kd] = 9;
        es_dibuja(&c, &v);
        chk_cond(q, "nivel 9: relleno blanco (C_TEXTO) y halo rojo de radio 14",
                 es_px(94, 79) == C_TEXTO && es_px(96, 65) == C_ERROR);
        chk_cond(q, "nivel 9: el \"!\" junto al c" S_aa "todo", gfx_texto_visto("!") && es_hay(C_ERROR));
        v.brillo = 0;
        es_dibuja(&c, &v);
        chk_cond(q, "sin brillo (el editor): hueco y todo en C_DIODO",
                 es_px(94, 79) == C_FONDO && es_px(91, 76) == C_DIODO && es_px(93, 85) == C_DIODO);
        v.brillo = br; br[kd] = 8; v.atenuar = 1;
        es_dibuja(&c, &v);
        chk_cond(q, "atenuado no se enciende", es_px(94, 79) == C_FONDO && es_px(91, 76) == C_TENUE);
        v.atenuar = 0;
        /* vertical: las flechitas del lado contrario al rótulo (a la izquierda) */
        circ_init(&c, "LED vertical");
        kd = circ_elemento(&c, EL_LED, 4, 3, DIR_ARR);          /* de (144,103) a (144,79) */
        br[kd] = 8;
        es_dibuja(&c, &v);
        chk_cond(q, "vertical: relleno en el centro del tri" S_aa "ngulo (144,93)", es_px(144, 93) == col);
        chk_cond(q, "vertical: las flechitas, a la izquierda (el r" S_oo "tulo va a la derecha)",
                 es_px(138, 94) == col && es_px(150, 94) == C_FONDO);
        {
            double If, Im;
            esq_led_if_im(&c.el[kd], &If, &Im);
            chk_cond(q, "esq_led_if_im: vac" S_ii "os = 20 mA y 30 mA", If == NL_IF && Im == NL_IM);
        }
        {
            double I[MAX_EL];
            memset(I, 0, sizeof I);
            es_led(&c, LED_VERDE);
            I[2] = 45e-3; I[1] = 1.0;
            c.el[2].par[QL_IM - Q_P1].v = 50e-3; c.el[2].par[QL_IM - Q_P1].est = M_DATO;
            esq_brillos(&c, I, br);
            chk_cond(q, "esq_brillos: solo los LED (con su Im de dato: 45 mA < 50 mA da 8)", br[2] == 8 && br[1] == 0 && br[0] == 0);
            I[2] = 51e-3;
            esq_brillos(&c, I, br);
            chk_cond(q, "esq_brillos: 51 mA > 50 mA da 9", br[2] == 9);
        }
        /* la BMP: diez LED de los seis colores, del nivel 0 al 9, resueltos */
        circ_init(&c, "LED");
        memset(&vs, 0, sizeof vs);
        for (i = 0; i < 10; i++) {
            kd = circ_elemento(&c, EL_LED, 3 * (i / 4), 2 * (i % 4), DIR_DER);
            if (kd < 0) continue;
            c.el[kd].variante = (unsigned char)(i % LED_N);
            vs.I[kd] = i == 9 ? 40e-3 : 20e-3 * i / 8.0;
        }
        esq_brillos(&c, vs.I, br);
        esq_vista(&v);
        v.val = &vs;
        v.brillo = br;
        es_dibuja(&c, &v);
        for (i = 0, k = 0; i < 10; i++) if (br[i] != i) k++;
        chk_cond(q, "diez LED con los niveles 0 a 9", k == 0);
        es_vuelca("leds");
    }

    /* ---- El inversor del §6.2: las patas del AO, sus signos y los puntos
     *      interiores sin pintar ---- */
    {
        const char *q = "inversor dibujado";
        e4_inversor(&c);
        esq_vista(&v);
        v.capa = CAPA_POTENCIALES;
        v.rejilla = 1;
        es_dibuja(&c, &v);
        chk_cond(q, "U1 con " S_RARR " en (4,2): la pata - en (144,79) es una uni" S_oo "n y su hilo es C_AO",
                 es_px(144, 79) == C_TEXTO && es_px(148, 79) == C_AO);
        chk_cond(q, "la pata + en (144,127) y la salida en (208,103), en C_AO",
                 es_px(144, 127) == C_AO && es_px(208, 103) == C_AO && es_px(204, 103) == C_AO);
        chk_cond(q, "los v" S_ee "rtices del tri" S_aa "ngulo: (152,73), (152,133) y (200,103)",
                 es_px(152, 73) == C_AO && es_px(152, 133) == C_AO && es_px(200, 103) == C_AO);
        chk_cond(q, "el tri" S_aa "ngulo, relleno de fondo", es_px(176, 103) == C_FONDO && es_px(170, 110) == C_FONDO);
        chk_cond(q, "el - en (160,91) y el + en (160,115)",
                 es_px(158, 91) == C_AO && es_px(162, 91) == C_AO && es_px(160, 89) == C_FONDO &&
                 es_px(160, 113) == C_AO && es_px(158, 115) == C_AO);
        {
            static const int PI_[6][2] = { { 5, 2 }, { 6, 2 }, { 4, 3 }, { 5, 3 }, { 5, 4 }, { 6, 4 } };
            int n = 0, sx, sy;
            esq_marco_editor(&m);
            for (i = 0; i < 6; i++) {
                esq_pt(&m, PI_[i][0], PI_[i][1], &sx, &sy);
                if (es_px(sx, sy) == C_REJILLA || es_px(sx + 1, sy + 1) == C_REJILLA) n++;
            }
            chk_cond(q, "los 6 puntos interiores de la caja no se pintan", n == 0);
            chk_cond(q, "los de fuera, s" S_ii " (8,0) y (9,6)",
                     es_todo(272, 31, 273, 32, C_REJILLA) && es_todo(304, 175, 305, 176, C_REJILLA));
            chk_cond(q, "uni" S_oo "n en (7,3) (grado 3) y no en (6,3) ni en (4,4) (grado 2)",
                     es_px(240, 103) == C_TEXTO && es_px(206, 103) == C_AO && es_px(146, 127) == C_AO);
        }
        v.sel = 2;
        es_dibuja(&c, &v);
        chk_cond(q, "elegido: el recuadro es la caja " S_PM " 8 px (136..216 " S_TIMES " 71..135)",
                 es_px(136, 100) == C_CURSOR && es_px(216, 100) == C_CURSOR && es_px(176, 71) == C_CURSOR &&
                 es_px(176, 135) == C_CURSOR && es_px(135, 100) != C_CURSOR && es_px(217, 100) != C_CURSOR);
        v.sel = -1;
        circ_voltear(&c, 2);
        es_dibuja(&c, &v);
        chk_cond(q, "con espejo, el + pasa arriba (160,91) y el - abajo (160,115)",
                 es_px(160, 89) == C_AO && es_px(160, 93) == C_AO && es_px(160, 113) == C_FONDO && es_px(158, 115) == C_AO);
        chk_cond(q, "con espejo, las patas no se mueven", es_px(148, 79) == C_AO && es_px(144, 127) == C_AO && es_px(208, 103) == C_AO);
    }

    /* ---- Los 3 patas en sus 8 orientaciones: cada pata, en el color del tipo ---- */
    {
        static const int T3[3] = { EL_AO, EL_BJT, EL_MOS };
        const char *q = "3 patas orientados";
        int t, d, esp, mal = 0, puestos = 0, p;
        esq_marco_editor(&m);
        es_vista_sola(&v);
        for (t = 0; t < 3; t++)
            for (d = 0; d < 4; d++)
                for (esp = 0; esp < 2; esp++) {
                    circ_init(&c, "orienta");
                    k = circ_elemento(&c, (TipoEl)T3[t], 4, 3, d);
                    if (k < 0) { mal++; continue; }
                    if (esp) circ_voltear(&c, k);
                    c.el[k].variante = (unsigned char)(d & 1);   /* PNP y canal P en la mitad */
                    puestos++;
                    es_dibuja(&c, &v);
                    for (p = 0; p < 3; p++) {
                        int gx, gy, sx, sy;
                        circ_pata(&c.el[k], p, &gx, &gy);
                        esq_pt(&m, gx, gy, &sx, &sy);
                        if (es_px(sx, sy) != tipo_info(T3[t])->color) mal++;
                    }
                }
        chk_cond(q, "24 colocaciones en (4,3)", puestos == 24);
        chk_cond(q, "cada pata, en el color de su tipo (lp y circ_orienta dicen lo mismo)", mal == 0);
        /* las BMP: las 8 orientaciones del AO y las 16 de los transistores
         * (la mitad en PNP y canal P), cada una en su marco ajustado */
        for (t = 0; t < 2; t++) {
            int n = t ? 16 : 8, j;
            gfx_clip_reset();
            gfx_clear(C_FONDO);
            for (j = 0; j < n; j++) {
                int tipo = t ? (j < 8 ? EL_BJT : EL_MOS) : EL_AO, jj = j % 8;
                int w = 80, h = t ? 60 : 120, x0 = (j % 4) * w, y0 = (j / 4) * h;
                circ_init(&c, "orienta");
                k = circ_elemento(&c, (TipoEl)tipo, 4, 3, jj / 2);
                if (k < 0) continue;
                if (jj & 1) circ_voltear(&c, k);
                c.el[k].variante = (unsigned char)((jj / 2) & 1);
                esq_vista(&v);
                v.capa = CAPA_NOMBRES;
                esq_marco_ajustado(&c, x0, y0, w, h, &m);
                gfx_clip(x0, y0, w, h);
                esq_dibuja(&c, &m, &v);
                gfx_clip_reset();
                gfx_rect(x0, y0, w, h, C_REJILLA);
            }
            es_vuelca(t ? "orienta_trans" : "orienta_ao");
        }
        /* un 3 patas roto (sus patas no son una huella) no se dibuja ni se cae */
        circ_init(&c, "roto");
        k = circ_elemento(&c, EL_AO, 4, 3, DIR_DER);
        c.el[k].xc = 9; c.el[k].yc = 0;
        es_dibuja(&c, &v);
        chk_cond(q, "un AO roto no pinta nada", !es_hay(C_AO));
    }

    /* ---- El rombo de las controladas ---- */
    {
        const char *q = "rombo";
        circ_init(&c, "rombo");
        es_val(&c, circ_elemento(&c, EL_EDEP, 1, 1, DIR_DER), 2.0);     /* centro (64,55) */
        es_val(&c, circ_elemento(&c, EL_JDEP, 1, 3, DIR_DER), 1e-3);    /* centro (64,103) */
        es_vista_sola(&v);
        es_dibuja(&c, &v);
        chk_cond(q, "E: las puntas a " S_PM "9 px a lo largo y " S_PM "7 de lado",
                 es_px(55, 55) == C_FUENTE && es_px(73, 55) == C_FUENTE && es_px(64, 48) == C_FUENTE && es_px(64, 62) == C_FUENTE);
        chk_cond(q, "E: el + en c + 4 (lado b) y el - en c - 4",
                 es_px(68, 53) == C_FUENTE && es_px(68, 57) == C_FUENTE && es_px(58, 55) == C_FUENTE &&
                 es_px(60, 53) == C_FONDO && es_px(64, 55) == C_FONDO);
        chk_cond(q, "E: fuera del rombo, nada", es_px(64, 46) == C_FONDO && es_px(64, 64) == C_FONDO);
        chk_cond(q, "J: el rombo en C_FUENTE_I con la flecha de c - 5 a c + 5",
                 es_px(64, 96) == C_FUENTE_I && es_px(64, 110) == C_FUENTE_I && es_px(59, 103) == C_FUENTE_I &&
                 es_px(64, 103) == C_FUENTE_I && es_px(66, 101) == C_FUENTE_I && es_px(58, 103) == C_FONDO);
    }

    /* ---- La marca roja y las flechas del cursor al colocar ---- */
    {
        const char *q = "marca y flechas";
        circ_init(&c, "vac" S_ii "o");
        esq_vista(&v);
        v.cur_x = 4; v.cur_y = 2;
        es_dibuja(&c, &v);
        chk_cond(q, "sin marca, nada en C_ERROR", !es_hay(C_ERROR));
        v.marca_x = 4; v.marca_y = 2;
        es_dibuja(&c, &v);
        chk_cond(q, "la marca en (4,2): un cuadro de 7" S_TIMES "7 en C_ERROR, encima del cursor",
                 es_todo(141, 76, 147, 82, C_ERROR) && es_px(140, 79) != C_ERROR && es_px(148, 79) != C_ERROR &&
                 es_px(144, 75) != C_ERROR && es_px(144, 83) != C_ERROR);
        v.marca_x = 6; v.marca_y = 3;
        es_dibuja(&c, &v);
        chk_cond(q, "en otro punto (6,3): (205..211, 100..106), y el cursor se ve",
                 es_todo(205, 100, 211, 106, C_ERROR) && es_px(141, 76) == C_CURSOR);
        gfx_clear(C_FONDO);
        esq_flechas_cursor_m(100, 100, (1u << DIR_DER) | (1u << DIR_ARR), C_CURSOR, C_TENUE);
        chk_cond(q, "caben " S_RARR " y " S_UARR ": en col; " S_LARR " y " S_DARR ", en col_no",
                 es_px(109, 100) == C_CURSOR && es_px(100, 91) == C_CURSOR &&
                 es_px(91, 100) == C_TENUE && es_px(100, 109) == C_TENUE);
        gfx_clear(C_FONDO);
        esq_flechas_cursor_m(100, 100, 0xFu, C_CURSOR, C_TENUE);
        memcpy(copia, gfx_buffer(), sizeof copia);
        gfx_clear(C_FONDO);
        esq_flechas_cursor(100, 100, C_CURSOR);
        chk_cond(q, "con las 4, igual que esq_flechas_cursor", memcmp(copia, gfx_buffer(), sizeof copia) == 0);
    }

    /* ---- La línea del control: discontinua, 2 px de trazo y 2 de hueco ---- */
    {
        const char *q = "l" S_ii "nea del control";
        int ke, kr, x, mal = 0;
        circ_init(&c, "control");
        ke = circ_elemento(&c, EL_EDEP, 1, 1, DIR_DER);                 /* centro (64,55) */
        es_val(&c, ke, 2.0);
        kr = circ_elemento(&c, EL_RES, 4, 1, DIR_DER);                  /* centro (160,55) */
        es_val(&c, kr, 1e3);
        c.el[ke].ref = (signed char)kr;
        es_vista_sola(&v);
        es_dibuja(&c, &v);
        chk_cond(q, "sin ctl_de, no hay l" S_ii "nea", es_px(100, 55) == C_FONDO && es_px(64, 55) == C_FONDO);
        v.ctl_de = ke;
        es_dibuja(&c, &v);
        chk_cond(q, "empieza en el centro del rombo", es_px(64, 55) == C_CURSOR);
        for (x = 84; x <= 140; x++)
            if ((es_px(x, 55) == C_CURSOR) != ((x - 64) % 4 < 2)) mal++;
        chk_cond(q, "de ah" S_ii " al centro de R1: 2 px de trazo y 2 de hueco", mal == 0);
        v.ctl_de = kr;
        es_dibuja(&c, &v);
        chk_cond(q, "ctl_de en algo que no es una controlada: nada", es_px(100, 55) == C_FONDO);
        c.el[ke].ref = -1; v.ctl_de = ke;
        es_dibuja(&c, &v);
        chk_cond(q, "una controlada sin control: nada", es_px(100, 55) == C_FONDO);
    }

    /* ---- Regiones ---- */
    {
        const char *q = "regiones";
        chk_cond(q, "nombres", strcmp(esq_region(REG_CONDUCE), "conduce") == 0 && strcmp(esq_region(REG_CORTE), "corte") == 0 &&
                 strcmp(esq_region(REG_SAT_MAS), "satura +") == 0 && strcmp(esq_region(REG_SATURACION), "saturaci" S_oo "n") == 0 &&
                 strcmp(esq_region(REG_OHMICA), S_oo "hmica") == 0 && strcmp(NOM_REG[REG_LINEAL], "lineal") == 0 &&
                 esq_region(REG_NADA)[0] == 0 && esq_region(REG_N)[0] == 0 && esq_region(-1)[0] == 0);
        chk_cond(q, "colores: conduce C_OK, corte C_SUAVE, las dem" S_aa "s C_CURSOR",
                 esq_region_col(REG_CONDUCE) == C_OK && esq_region_col(REG_CORTE) == C_SUAVE &&
                 esq_region_col(REG_RUPTURA) == C_CURSOR && esq_region_col(REG_SAT_MENOS) == C_CURSOR &&
                 esq_region_col(REG_LINEAL) == C_CURSOR && esq_region_col(REG_N) == C_SUAVE);
    }

    /* ---- Los rótulos sin resolver ---- */
    {
        const char *q = "r" S_oo "tulos sin resolver";
        char t[32];
        es_todos(&c, 1);
        chk_cond(q, "el circuito de todos: 16 elementos, v" S_aa "lido y sano",
                 c.nel == 16 && circ_valido(&c) && circuito_sano(&c) == 0);
        esq_vista(&v);
        es_dibuja(&c, &v);
        chk_cond(q, "diodo con V" S_GAMMA ": \"V" S_GAMMA "0,7V\"", gfx_texto_visto("V" S_GAMMA "0,7V"));
        chk_cond(q, "zener de libro: \"Vz5,1V\"", gfx_texto_visto("Vz5,1V"));
        chk_cond(q, "LED: su color", gfx_texto_visto("rojo"));
        chk_cond(q, "E controlada: \"2" S_CDOT "V(R1)\"", gfx_texto_visto("2" S_CDOT "V(R1)"));
        chk_cond(q, "J controlada: \"0,5" S_CDOT "I(R4)\"", gfx_texto_visto("0,5" S_CDOT "I(R4)"));
        chk_cond(q, "AO: \"ideal\"", gfx_texto_visto("ideal"));
        chk_cond(q, "BJT: \"NPN " S_BETA "100\"", gfx_texto_visto("NPN " S_BETA "100"));
        chk_cond(q, "MOSFET: \"N Vt2V\"", gfx_texto_visto("N Vt2V"));
        c.el[T_D].variante = DIO_1N4148; c.el[T_Z].variante = ZEN_1N4733A; c.el[T_LED].variante = LED_AZUL;
        c.el[T_U1].variante = AO_REAL; c.el[T_Q].variante = BJT_PNP; c.el[T_M].variante = MOS_CP;
        c.el[T_EC].val.v = -2.0;
        c.el[T_JC].val.est = M_LIBRE;
        es_dibuja(&c, &v);
        chk_cond(q, "1N4148, 1N4733A, azul", gfx_texto_visto("1N4148") && gfx_texto_visto("1N4733A") && gfx_texto_visto("azul"));
        chk_cond(q, "AO real: \"A100k\"; PNP; canal P", gfx_texto_visto("A100k") && gfx_texto_visto("PNP " S_BETA "100") &&
                 gfx_texto_visto("P Vt2V"));
        chk_cond(q, "ganancia negativa: \"-2" S_CDOT "V(R1)\"", gfx_texto_visto("-2" S_CDOT "V(R1)"));
        chk_cond(q, "ganancia vac" S_ii "a: su nombre, \"" S_BETA S_CDOT "I(R4)\"", gfx_texto_visto(S_BETA S_CDOT "I(R4)"));
        c.el[T_D].variante = DIO_IDEAL;
        es_dibuja(&c, &v);
        chk_cond(q, "diodo ideal: \"ideal\"", gfx_texto_visto("ideal"));
        c.el[T_D].variante = DIO_EXP;
        c.el[T_EC].variante = DEP_I; c.el[T_EC].val.v = 1e3;
        c.el[T_Q].par[QB_BF - Q_P1].v = 150.0; c.el[T_Q].par[QB_BF - Q_P1].est = M_DATO;
        es_dibuja(&c, &v);
        chk_cond(q, "exponencial: \"exp.\"; CCVS: \"1k" S_CDOT "I(R1)\"; " S_BETA " de dato",
                 gfx_texto_visto("exp.") && gfx_texto_visto("1k" S_CDOT "I(R1)") && gfx_texto_visto("PNP " S_BETA "150"));
        c.el[T_EC].ref = -1;
        c.el[T_EC].val.est = M_BUSCAR;
        es_dibuja(&c, &v);
        chk_cond(q, "sin control y con [?!]: \"?" S_CDOT "I(?)\"", gfx_texto_visto("?" S_CDOT "I(?)"));
        esq_valor(&c.el[T_EC], 0, T_EC, t, sizeof t);
        chk_cond(q, "esq_valor sin el circuito: la ganancia sola (\"?\")", strcmp(t, "?") == 0);
        c.el[T_EC].val.est = M_DATO;
        esq_valor(&c.el[T_EC], 0, T_EC, t, sizeof t);
        chk_cond(q, "esq_valor de una CCVS: \"1k" S_OHM "\"", strcmp(t, "1k" S_OHM) == 0);
        esq_valor_c(&c, 0, T_R1, t, sizeof t);
        chk_cond(q, "esq_valor_c de una R: la de esq_valor (\"1k" S_OHM "\")", strcmp(t, "1k" S_OHM) == 0);
        esq_valor_c(&c, 0, 99, t, sizeof t);
        chk_cond(q, "esq_valor_c fuera de rango: vac" S_ii "o", t[0] == 0);
        /* los nombres: el del AO dentro del triángulo */
        v.capa = CAPA_NOMBRES;
        es_dibuja(&c, &v);
        chk_cond(q, "capa de nombres: U1, Q1, M1, D1, E3, J1",
                 gfx_texto_visto("U1") && gfx_texto_visto("Q1") && gfx_texto_visto("M1") &&
                 gfx_texto_visto("D1") && gfx_texto_visto("E3") && gfx_texto_visto("J1"));
        esq_vista(&v);
        es_todos(&c, 1);
        v.rejilla = 1;
        es_dibuja(&c, &v);
        es_vuelca("todos");
    }

    /* ---- Resuelto: el punto de operación de todos menos el BJT y el MOSFET ---- */
    {
        const char *q = "r" S_oo "tulos resueltos";
        SimDiag d;
        SimNewton nw;
        char t[48];
        double If, Im, pct;
        es_todos(&c, 0);
        red_extraer(&c, &r);
        memset(&nw, 0, sizeof nw);
        chk_cond(q, "sim_punto_op_cc da SIM_OK", sim_punto_op_cc(&c, &r, &vs, &d, &nw) == SIM_OK);
        es_todos(&c, 1);                        /* el BJT y el MOSFET, a mano */
        vs.reg[T_Q] = REG_ACTIVA; vs.I[T_Q] = 2.45e-3; vs.V[T_Q] = 5.0;
        vs.reg[T_M] = REG_SATURACION; vs.I[T_M] = 4e-3; vs.V[T_M] = 6.0;
        esq_brillos(&c, vs.I, br);
        esq_vista(&v);
        v.val = &vs;
        v.brillo = br;
        v.rejilla = 1;
        es_dibuja(&c, &v);
        chk_cond(q, "el diodo conduce y el zener rompe", vs.reg[T_D] == REG_CONDUCE && vs.reg[T_Z] == REG_RUPTURA &&
                 gfx_texto_visto("conduce") && gfx_texto_visto("ruptura"));
        chk_cond(q, "la V del zener, con su signo (\"-5,1V\")", gfx_texto_visto("-5,1V"));
        chk_cond(q, "AO: \"Vo -10,0V\" y \"lineal\"", vs.reg[T_U1] == REG_LINEAL &&
                 gfx_texto_visto("Vo -10,0V") && gfx_texto_visto("lineal"));
        chk_cond(q, "E3: su V y su ley (\"2V\", \"2" S_CDOT "V(R1)\")", gfx_texto_visto("\n2V\n") && gfx_texto_visto("2" S_CDOT "V(R1)"));
        chk_cond(q, "BJT: \"activa\" e \"Ic 2,45mA\"; MOSFET: \"saturaci" S_oo "n\" e \"Id 4,00mA\"",
                 gfx_texto_visto("activa") && gfx_texto_visto("Ic 2,45mA") &&
                 gfx_texto_visto("saturaci" S_oo "n") && gfx_texto_visto("Id 4,00mA"));
        esq_led_if_im(&c.el[T_LED], &If, &Im);
        pct = 100.0 * vs.I[T_LED] / If;
        sprintf(t, "%d %%", (int)(pct + 0.5));
        chk_cond(q, "el LED: su porcentaje (100" S_CDOT "I/If redondeado)", gfx_texto_visto(t));
        chk_cond(q, "el LED en el nivel 6 (14,5 mA de 20)", br[T_LED] == 6 && vs.I[T_LED] > 14.2e-3 && vs.I[T_LED] < 14.7e-3);
        sprintf(t, "%.1fmA", vs.I[T_LED] * 1e3);
        {
            char *p = strchr(t, '.');
            if (p) *p = ',';
        }
        chk_cond(q, "su corriente con tres cifras (\"14,5mA\")", gfx_texto_visto(t));
        es_vuelca("todos_valores");
        /* apagado y quemado */
        vs.I[T_LED] = 0.1e-3;
        es_dibuja(&c, &v);
        chk_cond(q, "0,1 mA: \"apagado\"", gfx_texto_visto("apagado"));
        vs.I[T_LED] = 45.2e-3;
        br[T_LED] = 9;
        es_dibuja(&c, &v);
        chk_cond(q, "45,2 mA: \"" S_IE "quema!\" y \"45,2mA\"", gfx_texto_visto(S_IE "quema!") && gfx_texto_visto("45,2mA"));
        /* las demás capas y un marco ajustado pequeño (como la miniatura):
         * nada se sale de la pantalla */
        for (i = CAPA_NOMBRES; i < CAPA_N; i++) { v.capa = (unsigned char)i; es_dibuja(&c, &v); }
        v.capa = CAPA_VALORES;
        for (i = 0; i < 3; i++) {
            static const int W[3] = { 320, 200, 120 }, H[3] = { 150, 110, 70 };
            gfx_clear(C_FONDO);
            esq_marco_ajustado(&c, 0, 20, W[i], H[i], &m);
            for (k = 0; k < c.nel; k++) { v.sel = k; esq_dibuja(&c, &m, &v); }
            v.sel = -1;
        }
        v.capa = CAPA_NOMBRES;
        esq_marco_ajustado(&c, 0, 20, 120, 70, &m);
        esq_dibuja(&c, &m, &v);
    }

    pruebas++;
    if (gfx_desbordes() != 0) falla("s" S_ii "mbolos", "textos que se salen de la pantalla", gfx_desbordes(), 0);
    gfx_clip_reset();
}
