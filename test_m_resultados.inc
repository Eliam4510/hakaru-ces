/* --------------------------------------------------------------------------
 *  69. Mejoras de resultados e instrumentos
 *
 *  Una prueba por mejora (cada una falla sin ella, comprobado por
 *  mutación): la letra de cada nodo en la capa Potenciales y en los pasos
 *  de Kirchhoff (con el nodo del paso resaltado; que se lean y no tapen
 *  nada, mirado en los píxeles, en toda la biblioteca); la P absorbida con
 *  signo y el balance en la tabla de CC (y en la del punto de operación);
 *  la página «En el tiempo» de la tabla de CA; [U] y las medidas entre los
 *  cursores en el osciloscopio (y [=] con ellos); el disparo al nivel de
 *  lo que se ve (y en otra emulación). Se incluye tras
 *  test_rev3_v_modelo.inc (usa r3u_abre, r3d_dato, r3d_fasor, r4_espera,
 *  r4_vuelca, r4_el, es_px, pon, cierra_reg, greg, gres, fuente_sp,
 *  nuevo_el, cierra_red, rc, rr, nodos, azar, semilla, ve, repite, dir_out
 *  y TECLAS). Se corre sola con HAKARU_SOLO_M_RESULTADOS=1; HAKARU_MR=<trozo>
 *  corre solo las partes con ese trozo en el nombre.
 * ----------------------------------------------------------------------- */

/* El índice del modelo con ese título; -1 si no está */
static int mr_modelo(const char *titulo)
{
    int i;
    for (i = 0; i < N_MODELOS; i++) if (strcmp(MODELOS[i].titulo, titulo) == 0) return i;
    return -1;
}

/* E1 = 12 V y E2 = 6 V enfrentadas con R1 = 2 Ω, dibujadas en el editor:
 * E1 hacia abajo (+ abajo), cable a la derecha, E2 hacia abajo desde
 * arriba (+ abajo: contra E1) y R1 de vuelta. I = 3 A; E1 entrega 36 W, E2
 * absorbe 18 W y R1 18 W. Tres nodos: 0, 12 y 6 V (la referencia es el -
 * de E1). 1 si quedan los tres elementos */
static int mr_dos_fuentes(void)
{
    ui_init();
    TECLAS(T_ENTER, T_ENTER, T_ENTER);                           /* editor nuevo */
    TECLAS(T_LETRA('E'), T_ABAJO, T_DIG(1), T_DIG(2), T_ENTER);
    TECLAS(T_LETRA('W'), T_DER, T_ESC);
    TECLAS(T_ARRIBA);
    TECLAS(T_LETRA('E'), T_ABAJO, T_DIG(6), T_ENTER);
    TECLAS(T_ARRIBA);
    TECLAS(T_LETRA('R'), T_IZQ, T_DIG(2), T_ENTER);
    return ui_pantalla() == P_EDITOR && ui_circuito()->nel == 3;
}

/* ¿Está en (x, y) la letra ch entera y en el color col? Se mira la celda de
 * 6×12 entera: los píxeles del glifo en col y los demás de otro color (una
 * letra tapada, o un trozo de símbolo del mismo color, no casan) */
static int mr_glifo_en(int x, int y, char ch, u16 col)
{
    const u8 *g = font_glifo12((unsigned char)ch);
    int gx, gy;
    if (x < 0 || y < 0 || x + FONT_W > SCR_W || y + FONT_H > SCR_H) return 0;
    for (gy = 0; gy < FONT_H; gy++)
        for (gx = 0; gx < FONT_W; gx++)
            if ((int)((g[gy] >> gx) & 1u) != (es_px(x + gx, y + gy) == col)) return 0;
    return 1;
}

/* Cuántas veces está la letra ch en col con la esquina en [x0, x1)×[y0, y1)
 * (la última, en *lx, *ly) */
static int mr_cuenta_glifo(int x0, int y0, int x1, int y1, char ch, u16 col, int *lx, int *ly)
{
    int x, y, n = 0;
    for (y = y0; y < y1; y++)
        for (x = x0; x < x1; x++)
            if (mr_glifo_en(x, y, ch, col)) { n++; *lx = x; *ly = y; }
    return n;
}

/* El esquema de un paso como lo pinta dib_pasos (el marco ajustado entre la
 * barra de 16 y el panel en y = 104, con su recorte): el paso p de ps, con
 * las letras de los nodos o sin ellas */
#define MR_PS_Y0 16
#define MR_PS_Y1 104
static int mr_y1 = MR_PS_Y1;                           /* el borde de abajo (104; más arriba, más apretado) */
static void mr_pinta_paso(const Circuito *c, const Red *r, const Paso *s, int letras)
{
    Marco m;
    Vista v;
    esq_vista(&v);
    v.capa = CAPA_NOMBRES;
    v.resalta = s->resalta;
    v.atenuar = s->resalta != 0;
    v.red = r;
    v.letras = (unsigned char)letras;
    v.nodo_resalta = s->nodo;
    gfx_clip_reset();
    gfx_clear(C_FONDO);
    esq_marco_ajustado(c, 0, MR_PS_Y0, SCR_W, mr_y1 - MR_PS_Y0 - 4, &m);
    gfx_clip(0, MR_PS_Y0, SCR_W, mr_y1 - MR_PS_Y0);
    esq_dibuja(c, &m, &v);
    gfx_clip_reset();
}

/* ---- 5. Las letras de los nodos ---- */
static void mr_letras(void)
{
    static Red r;
    static Pasos ps;
    static Solucion s;
    const Valores *v;
    const char *q = "letras de nodo";
    char t[48], u[24];
    Marco m;
    Vista vi;
    int n, j, ok, nres, nodo_b = -1, cuantos = 0;

    /* La capa Potenciales: «B» y su valor, dos textos seguidos */
    chk_cond(q, "el circuito de dos fuentes", mr_dos_fuentes());
    TECLAS(T_IGUAL);
    chk_cond(q, "[=]: el resultado de CC", ui_pantalla() == P_RESULTADO && !ui_res_nl());
    for (j = 0; j < CAPA_N && ui_capa() != CAPA_POTENCIALES; j++) TECLAS(T_DER);
    red_extraer(ui_circuito(), &r);
    v = ui_valores();
    ok = ui_capa() == CAPA_POTENCIALES && r.nn == 3;
    for (n = 0; n < r.nn; n++) {
        num_corto(u, sizeof u, v->phi[n]);
        sprintf(t, "\n%c\n%sV\n", red_letra(&r, n), u);
        printf("    nodo %c: %s V, se ve «%c %sV» %d\n", red_letra(&r, n), u, red_letra(&r, n), u, ve(t));
        if (!ve(t)) ok = 0;
    }
    chk_cond(q, "Potenciales: cada nodo con su letra delante del valor (A, B y C)", ok);
    r4_vuelca("mr_potenciales_letras");
    TECLAS(T_ESC);

    /* En CA: la letra y el fasor corto de cada nodo */
    {
        static Circuito c;
        int i = mr_modelo("RLC serie en fasores");
        const CaRes *ca;
        chk_cond(q, "el modelo «RLC serie en fasores»", i >= 0);
        if (i >= 0) {
            modelo_cargar(i, &c);
            r3u_abre(&c);
            TECLAS(T_IGUAL);
            chk_cond(q, "CA: [=] da P_CA", ui_pantalla() == P_CA);
            for (j = 0; j < CAPA_N && !ve("Potenciales"); j++) TECLAS(T_DER);
            red_extraer(ui_circuito(), &r);
            ca = ui_ca();
            ok = ve("Potenciales") && r.nn >= 3;
            for (n = 0; n < r.nn; n++) {
                double mo, an;
                ca_a_conv(ui_circuito()->conv, ca->phi[n], &mo, &an);
                num_fasor(u, sizeof u, mo, an, 0, 1, "V");
                sprintf(t, "\n%c\n%s\n", red_letra(&r, n), u);
                if (!ve(t)) ok = 0;
            }
            chk_cond(q, "CA: cada nodo con su letra delante del fasor", ok);
            r4_vuelca("mr_potenciales_letras_ca");
            /* los fasores son largos: en el marco del editor, el rótulo de B
             * (arriba a la derecha pisaría el de A) va a otro sitio. Se
             * miran las cajas de los dos rótulos (la letra, en C_TENSION,
             * y detrás su fasor): no se cortan */
            {
                Vista vc;
                Marco mc;
                int ax = -1, ay = -1, bx = -1, by = -1, wa, wb, na, nb;
                double mo, an;
                esq_vista(&vc);
                vc.capa = CAPA_POTENCIALES; vc.ca = ca; vc.conv = ui_circuito()->conv; vc.red = &r;
                esq_marco_editor(&mc);
                gfx_clip_reset();
                gfx_clear(C_FONDO);
                esq_dibuja(ui_circuito(), &mc, &vc);
                na = mr_cuenta_glifo(0, 0, SCR_W, SCR_H, red_letra(&r, 0), C_TENSION, &ax, &ay);
                nb = mr_cuenta_glifo(0, 0, SCR_W, SCR_H, red_letra(&r, 1), C_TENSION, &bx, &by);
                ca_a_conv(ui_circuito()->conv, ca->phi[0], &mo, &an);
                wa = FONT_W + 3 + gfx_text_w(num_fasor(u, sizeof u, mo, an, 0, 1, "V"), 1);
                ca_a_conv(ui_circuito()->conv, ca->phi[1], &mo, &an);
                wb = FONT_W + 3 + gfx_text_w(num_fasor(u, sizeof u, mo, an, 0, 1, "V"), 1);
                printf("    CA: el rótulo de A en (%d,%d) de %d px; el de B en (%d,%d) de %d px\n", ax, ay, wa, bx, by, wb);
                chk_cond(q, "CA: el r" S_oo "tulo de B no pisa el de A", na == 1 && nb == 1 &&
                         !(ax < bx + wb && bx < ax + wa && ay < by + FONT_H && by < ay + FONT_H));
            }
        }
    }

    /* Kirchhoff: el paso «Nodo X» guarda su nodo; los demás, -1 */
    mr_dos_fuentes();
    {
        static Circuito c;
        c = *ui_circuito();
        red_extraer(&c, &r);
        resolver(&c, &r, &s);
        pasos_init(&ps);
        j = proced_generar(&c, &r, &s, 0, &ps, 0);
        ok = j == PROC_KIRCHHOFF;
        for (n = 0; n < ps.n; n++) {
            const Paso *p = &ps.items[n];
            if (strncmp(p->titulo, "Nodo ", 5) == 0) {
                cuantos++;
                if (p->nodo != p->titulo[5] - 'A') ok = 0;
            } else if (p->nodo != -1) ok = 0;
        }
        printf("    Kirchhoff: %d pasos, %d de nodo\n", ps.n, cuantos);
        chk_cond(q, "los pasos «Nodo X» guardan su nodo (y los dem" S_aa "s, -1)", ok && cuantos == 2);
    }

    /* El nodo del paso, resaltado: todos sus puntos en C_RESALTA (y sin él,
     * no). Se mira el ÚLTIMO punto de un nodo con dos o más: el primero ya
     * lleva el punto de la letra */
    {
        const Circuito *cu = ui_circuito();
        int p, sx = 0, sy = 0, hay = 0, vez, npt;
        red_extraer(cu, &r);
        for (n = 0; n < r.nn; n++) {
            for (p = 0, npt = 0; p < REJ_PUNTOS; p++)
                if (r.punto[p] == n && circ_grado(cu, p % REJ_COLS, p / REJ_COLS) > 0) npt++;
            if (npt >= 2) break;
        }
        for (p = 0; p < REJ_PUNTOS && n < r.nn; p++)
            if (r.punto[p] == n && circ_grado(cu, p % REJ_COLS, p / REJ_COLS) > 0) {
                esq_marco_editor(&m);
                esq_pt(&m, p % REJ_COLS, p / REJ_COLS, &sx, &sy);
                hay = 1;
            }
        for (vez = 0; vez < 2; vez++) {
            nres = vez ? n : -1;
            esq_vista(&vi);
            vi.capa = CAPA_NOMBRES; vi.red = &r; vi.letras = 1; vi.nodo_resalta = nres;
            esq_marco_editor(&m);
            gfx_clip_reset();
            gfx_clear(C_FONDO);
            gfx_textos_reset();
            gfx_text(0, 0, "", C_FONDO, 1);            /* la primera letra, tras un salto */
            esq_dibuja(cu, &m, &vi);
            printf("    resalta %d: el punto (%d,%d) del nodo %c en C_RESALTA %d; letras A B C %d %d %d\n", nres, sx, sy,
                   red_letra(&r, n), es_px(sx, sy) == C_RESALTA, gfx_texto_visto("\nA\n"), gfx_texto_visto("\nB\n"),
                   gfx_texto_visto("\nC\n"));
            chk_cond(q, nres < 0 ? "sin nodo del paso: el punto no se resalta" : "el nodo del paso: su punto en C_RESALTA",
                     hay && (es_px(sx, sy) == C_RESALTA) == (nres >= 0));
            chk_cond(q, "letras: las de los tres nodos",
                     gfx_texto_visto("\nA\n") && gfx_texto_visto("\nB\n") && gfx_texto_visto("\nC\n"));
        }
    }

    /* En la pantalla de pasos: las letras en los de Kirchhoff, leídas en
     * los píxeles del esquema (entre la barra y el panel). En el primer
     * paso (las incógnitas) las tres en C_TENSION; en el de un nodo, la
     * suya en C_RESALTA */
    TECLAS(T_IGUAL, T_ENTER);
    ok = ui_pantalla() == P_PASOS && ui_paso() == 0 && !ve("Nodo ");
    {
        int x, y, nten = 0, nresal = 0;
        ui_dibuja();
        for (n = 0; n < 3; n++) {
            nten += mr_cuenta_glifo(0, MR_PS_Y0, SCR_W, MR_PS_Y1, (char)('A' + n), C_TENSION, &x, &y) == 1;
            nresal += mr_cuenta_glifo(0, MR_PS_Y0, SCR_W, MR_PS_Y1, (char)('A' + n), C_RESALTA, &x, &y);
        }
        printf("    P_PASOS, paso 1: letras en C_TENSION %d, en C_RESALTA %d\n", nten, nresal);
        chk_cond(q, "P_PASOS, paso 1 (sin nodo): A, B y C en C_TENSION, ninguna resaltada", ok && nten == 3 && nresal == 0);
    }
    for (j = 0; j < 40 && ui_pantalla() == P_PASOS && !ve("Nodo "); j++) TECLAS(T_DER);
    for (n = 0; n < 3; n++) {
        sprintf(t, "Nodo %c: lo que entra", 'A' + n);
        if (ve(t)) nodo_b = n;
    }
    printf("    P_PASOS: paso %d de %d, el del nodo %c\n", ui_paso() + 1, ui_npasos(), nodo_b >= 0 ? 'A' + nodo_b : '?');
    chk_cond(q, "P_PASOS de Kirchhoff: el paso de un nodo y las tres letras", nodo_b >= 0 &&
             ve("\nA\n") && ve("\nB\n") && ve("\nC\n"));
    {
        int x, y, bien = nodo_b >= 0;
        ui_dibuja();
        for (n = 0; n < 3 && bien; n++) {
            int res = mr_cuenta_glifo(0, MR_PS_Y0, SCR_W, MR_PS_Y1, (char)('A' + n), C_RESALTA, &x, &y);
            int ten = mr_cuenta_glifo(0, MR_PS_Y0, SCR_W, MR_PS_Y1, (char)('A' + n), C_TENSION, &x, &y);
            if (n == nodo_b ? res != 1 || ten != 0 : res != 0 || ten != 1) bien = 0;
        }
        chk_cond(q, "P_PASOS, el paso del nodo: su letra en C_RESALTA y las otras dos en C_TENSION", bien);
    }
    r4_vuelca("mr_pasos_nodo");
    TECLAS(T_ESC, T_ESC);
}

/* Los pasos de Kirchhoff de un circuito: en cada paso, cada letra de nodo
 * se lee (está una vez, entera, en su color: la del nodo del paso en
 * C_RESALTA y las demás en C_TENSION); y, en el mismo esquema sin letras,
 * no tapa nada (su celda y la columna de su izquierda están vacías), tiene
 * aire (dos columnas más a cada lado, vacías) y no tiene cerca (a menos de
 * 3 px) el punto de otro nodo. Suma a n[]: letras, no se leen, tapan, del
 * nodo del paso, sin aire y cerca de otro; -1 si no es de Kirchhoff */
enum { MRL_N, MRL_MAL, MRL_TAPA, MRL_RES, MRL_AIRE, MRL_CERCA, MRL_TOT };
static int mr_letras_kirchhoff(const Circuito *c0, const char *nombre, int n_[MRL_TOT])
{
    static Circuito c;
    static Red r;
    static Solucion s;
    static Pasos ps;
    static int lx[MAX_NODOS], ly[MAX_NODOS];
    Marco m;
    int j, n, q;
    c = *c0;
    if (circ_no_lineal(&c) || circ_alterna(&c) || circ_dinamico(&c)) return -1;
    red_extraer(&c, &r);
    if (r.error != RED_OK) return -1;
    if (resolver(&c, &r, &s) != SOL_OK || s.estado != SOL_OK) return -1;
    pasos_init(&ps);
    if (proced_generar(&c, &r, &s, 0, &ps, 0) != PROC_KIRCHHOFF) return -1;
    esq_marco_ajustado(&c, 0, MR_PS_Y0, SCR_W, mr_y1 - MR_PS_Y0 - 4, &m);
    for (j = 0; j < ps.n; j++) {
        const Paso *p = &ps.items[j];
        mr_pinta_paso(&c, &r, p, 1);
        for (n = 0; n < r.nn; n++) {
            u16 col = n == p->nodo ? C_RESALTA : C_TENSION;
            int veces = mr_cuenta_glifo(0, MR_PS_Y0, SCR_W, mr_y1, red_letra(&r, n), col, &lx[n], &ly[n]);
            n_[MRL_N]++;
            if (n == p->nodo) n_[MRL_RES]++;
            if (veces != 1) {
                n_[MRL_MAL]++;
                lx[n] = -1;
                printf("    %s, paso %d: la letra %c%s se ve %d veces\n", nombre, j + 1, red_letra(&r, n),
                       n == p->nodo ? " (la del paso)" : "", veces);
            }
        }
        mr_pinta_paso(&c, &r, p, 0);
        for (n = 0; n < r.nn; n++) {
            int oc;
            if (lx[n] < 0) continue;
            oc = gfx_ocupados(lx[n] - 1, ly[n], FONT_W + 1, FONT_H, C_FONDO);
            if (oc > 0) {
                n_[MRL_TAPA]++;
                printf("    %s, paso %d: la letra %c tapa %d p" S_ii "xeles\n", nombre, j + 1, red_letra(&r, n), oc);
            } else if (gfx_ocupados(lx[n] - 2, ly[n], FONT_W + 4, FONT_H, C_FONDO) > 0) {
                n_[MRL_AIRE]++;
                printf("    %s, paso %d: la letra %c, sin aire\n", nombre, j + 1, red_letra(&r, n));
            }
            for (q = 0; q < REJ_PUNTOS; q++) {
                int qx, qy;
                if (r.punto[q] < 0 || r.punto[q] == n || circ_grado(&c, q % REJ_COLS, q / REJ_COLS) <= 0) continue;
                esq_pt(&m, q % REJ_COLS, q / REJ_COLS, &qx, &qy);
                if (qx >= lx[n] - 3 && qx < lx[n] + FONT_W + 3 && qy >= ly[n] - 3 && qy < ly[n] + FONT_H + 3) {
                    n_[MRL_CERCA]++;
                    printf("    %s, paso %d: la letra %c, junto al nodo %c\n", nombre, j + 1, red_letra(&r, n), red_letra(&r, r.punto[q]));
                    break;
                }
            }
        }
    }
    return 0;
}

/* Todos los circuitos de Kirchhoff de la biblioteca */
static void mr_letras_biblioteca(void)
{
    static Circuito c;
    const char *q = "letras de nodo en los pasos";
    int lib, i, nk = 0, n_[MRL_TOT] = { 0, 0, 0, 0, 0, 0 };
    for (lib = 0; lib < 2; lib++)
        for (i = 0; i < (lib ? N_MODELOS : N_EJEMPLOS); i++) {
            if (lib) modelo_cargar(i, &c); else ejemplo_cargar(i, &c);
            if (mr_letras_kirchhoff(&c, lib ? MODELOS[i].titulo : EJEMPLOS[i].titulo, n_) == 0) nk++;
        }
    printf("    %d circuitos de Kirchhoff: %d letras en sus pasos (%d del nodo del paso); %d no se leen, %d tapan algo,"
           " %d sin aire, %d junto a otro nodo\n", nk, n_[MRL_N], n_[MRL_RES], n_[MRL_MAL], n_[MRL_TAPA], n_[MRL_AIRE],
           n_[MRL_CERCA]);
    chk_cond(q, "la biblioteca: cada letra se lee, en su color (la del nodo del paso resaltada)",
             nk >= 5 && n_[MRL_RES] >= 10 && n_[MRL_MAL] == 0);
    chk_cond(q, "la biblioteca: ninguna letra tapa un r" S_oo "tulo, un s" S_ii "mbolo ni un cable", nk >= 5 && n_[MRL_TAPA] == 0);
    chk_cond(q, "la biblioteca: ninguna letra junto al punto de otro nodo", nk >= 5 && n_[MRL_CERCA] == 0);
    /* todas con aire: B y D de «Dos fuentes» (su único punto, entre dos
     * símbolos con sus rótulos) lo encuentran 2 px más afuera; antes
     * quedaban pegadas a la R en sus 6 pasos («B≡R1») */
    chk_cond(q, "la biblioteca: todas con aire a los lados (también B y D de «Dos fuentes»)",
             nk >= 5 && n_[MRL_AIRE] == 0);
    /* más apretados (el esquema hasta y = 76 y no 104) */
    {
        int m_[MRL_TOT] = { 0, 0, 0, 0, 0, 0 };
        mr_y1 = 76;
        for (lib = 0; lib < 2; lib++)
            for (i = 0; i < (lib ? N_MODELOS : N_EJEMPLOS); i++) {
                if (lib) modelo_cargar(i, &c); else ejemplo_cargar(i, &c);
                mr_letras_kirchhoff(&c, lib ? MODELOS[i].titulo : EJEMPLOS[i].titulo, m_);
            }
        mr_y1 = MR_PS_Y1;
        printf("    apretados: %d letras; %d no se leen, %d tapan algo, %d sin aire, %d junto a otro nodo\n", m_[MRL_N],
               m_[MRL_MAL], m_[MRL_TAPA], m_[MRL_AIRE], m_[MRL_CERCA]);
        chk_cond(q, "m" S_aa "s apretados: cada letra se lee y ninguna junto al punto de otro nodo",
                 m_[MRL_N] == n_[MRL_N] && m_[MRL_MAL] == 0 && m_[MRL_CERCA] == 0);
    }
}

/* La capa Potenciales de la pantalla de ahora (P_RESULTADO o P_CA): cada
 * letra se lee (una vez, entera, en C_TENSION, en el esquema: y = 16 a
 * 158) y ningún rótulo tapa el punto de otro nodo (el primero de cada uno
 * sigue en C_TENSION, o en C_TEXTO si es una unión). Suma a los contadores */
static void mr_cuenta_potenciales(const char *nombre, int *nl, int *nmal, int *ntapa)
{
    static Red r;
    Marco m;
    int n, p, sx, sy, x, y;
    ui_dibuja();
    red_extraer(ui_circuito(), &r);
    esq_marco_ajustado(ui_circuito(), 0, 16, SCR_W, 158 - 16, &m);
    for (n = 0; n < r.nn; n++) {
        for (p = 0; p < REJ_PUNTOS; p++) if (r.punto[p] == n) break;
        if (p == REJ_PUNTOS) continue;
        (*nl)++;
        if (mr_cuenta_glifo(0, 16, SCR_W, 158, red_letra(&r, n), C_TENSION, &x, &y) != 1) {
            (*nmal)++;
            printf("    %s: la letra %c no se lee\n", nombre, red_letra(&r, n));
        }
        esq_pt(&m, p % REJ_COLS, p / REJ_COLS, &sx, &sy);
        if (es_px(sx, sy) != C_TENSION && es_px(sx, sy) != C_TEXTO) {
            (*ntapa)++;
            printf("    %s: el punto del nodo %c est" S_aa " tapado\n", nombre, red_letra(&r, n));
        }
    }
}

/* La capa Potenciales en toda la biblioteca: la CC y la CA */
static void mr_letras_potenciales(void)
{
    static Circuito c;
    const char *q = "letras de nodo en Potenciales";
    int lib, i, j, ncc = 0, nca = 0, nl = 0, nmal = 0, ntapa = 0;
    for (lib = 0; lib < 2; lib++)
        for (i = 0; i < (lib ? N_MODELOS : N_EJEMPLOS); i++) {
            const char *nombre = lib ? MODELOS[i].titulo : EJEMPLOS[i].titulo;
            int ca;
            if (lib) modelo_cargar(i, &c); else ejemplo_cargar(i, &c);
            ca = circ_alterna(&c);
            if (circ_no_lineal(&c) || (ca && circ_pide(&c)) || (!ca && circ_dinamico(&c))) continue;
            if (!r3u_abre(&c)) continue;
            TECLAS(T_IGUAL);
            if (ui_pantalla() != (ca ? P_CA : P_RESULTADO)) { TECLAS(T_ESC); continue; }
            for (j = 0; j < CAPA_N && !ve("Potenciales"); j++) TECLAS(T_DER);
            if (!ve("Potenciales")) { TECLAS(T_ESC); continue; }
            if (ca) nca++; else ncc++;
            mr_cuenta_potenciales(nombre, &nl, &nmal, &ntapa);
            TECLAS(T_ESC, T_ESC);
        }
    printf("    Potenciales: %d circuitos de CC y %d de CA, %d letras; %d no se leen, %d puntos tapados\n", ncc, nca, nl, nmal,
           ntapa);
    chk_cond(q, "la biblioteca: cada letra se lee y ning" S_uu "n r" S_oo "tulo tapa el punto de otro nodo",
             ncc >= 20 && nca >= 5 && nmal == 0 && ntapa == 0);
}

/* Un esquema apretado (28 px entre columnas y 14 entre filas): E1 de
 * (0,1) a (0,0), R3 de (0,0) a (1,0), R1 de (1,0) a (1,1) y R2 de (1,1) a
 * (0,1), con la tierra en (1,1). El rótulo del nodo de (0,1) («C -4V»),
 * arriba a la derecha, taparía el punto del de (1,0): se recoloca. Cada
 * punto de nodo sigue a la vista y cada letra se lee */
static void mr_letras_apretado(void)
{
    static Circuito c;
    static Red r;
    static Solucion s;
    const char *q = "letras de nodo, apretadas";
    Marco m;
    Vista v;
    int k, n, p, sx, sy, x, y, ntapa = 0, nmal = 0;
    circ_init(&c, "Apretado");
    k = circ_elemento(&c, EL_FUENTE, 0, 1, DIR_ARR); r3d_dato(&c.el[k].val, 12.0);
    k = circ_elemento(&c, EL_RES, 0, 0, DIR_DER); r3d_dato(&c.el[k].val, 1.0);
    k = circ_elemento(&c, EL_RES, 1, 0, DIR_ABA); r3d_dato(&c.el[k].val, 1.0);
    k = circ_elemento(&c, EL_RES, 1, 1, DIR_IZQ); r3d_dato(&c.el[k].val, 1.0);
    c.tierra_x = 1; c.tierra_y = 1;                    /* (0,1) a -4 V: su rótulo es largo */
    red_extraer(&c, &r);
    chk_cond(q, "se resuelve", r.error == RED_OK && resolver(&c, &r, &s) == SOL_OK && s.estado == SOL_OK && r.nn == 4);
    esq_vista(&v);
    v.capa = CAPA_POTENCIALES; v.val = &s.s[0]; v.red = &r;
    m.x0 = 60; m.y0 = 60; m.px = 28; m.py = 14;
    gfx_clip_reset();
    gfx_clear(C_FONDO);
    esq_dibuja(&c, &m, &v);
    {
        char ruta[256];
        sprintf(ruta, "%s/mr_letras_apretado.bmp", dir_out);   /* lo dibujado aquí (r4_vuelca redibujaría la interfaz) */
        gfx_dump_bmp(ruta);
    }
    for (n = 0; n < r.nn; n++) {
        for (p = 0; p < REJ_PUNTOS; p++) if (r.punto[p] == n) break;
        esq_pt(&m, p % REJ_COLS, p / REJ_COLS, &sx, &sy);
        if (es_px(sx, sy) != C_TENSION && es_px(sx, sy) != C_TEXTO) { ntapa++; printf("    el punto del nodo %c, tapado\n", red_letra(&r, n)); }
        if (mr_cuenta_glifo(0, 0, SCR_W, SCR_H, red_letra(&r, n), C_TENSION, &x, &y) != 1) { nmal++; printf("    la letra %c no se lee\n", red_letra(&r, n)); }
    }
    chk_cond(q, "28 x 14 px: ning" S_uu "n r" S_oo "tulo tapa el punto de otro nodo y las cuatro letras se leen",
             ntapa == 0 && nmal == 0);
}

static void mr_serie(Circuito *c, double E, double R1, double R2, int con_w);

/* Las letras de los pasos con el recorte justo encima de la fila de
 * arriba: lo que cae fuera del recorte no vale (no se vería), y cada letra
 * sigue leyéndose dentro */
static void mr_letras_recorte(void)
{
    static Circuito c;
    static Red r;
    const char *q = "letras de nodo, con recorte";
    Marco m;
    Vista v;
    int n, x, y, nmal = 0, sx, sy;
    mr_serie(&c, 12.0, 1.0, 3.0, 0);
    red_extraer(&c, &r);
    esq_vista(&v);
    v.capa = CAPA_NOMBRES; v.red = &r; v.letras = 1;
    esq_marco_editor(&m);
    esq_pt(&m, 0, 0, &sx, &sy);
    gfx_clip_reset();
    gfx_clear(C_FONDO);
    gfx_clip(0, sy - 3, SCR_W, SCR_H - (sy - 3));      /* nada por encima de la fila 0 */
    esq_dibuja(&c, &m, &v);
    gfx_clip_reset();
    for (n = 0; n < r.nn; n++)
        if (mr_cuenta_glifo(0, 0, SCR_W, SCR_H, red_letra(&r, n), C_TENSION, &x, &y) != 1) { nmal++; printf("    la letra %c no se ve\n", red_letra(&r, n)); }
    chk_cond(q, "el recorte a 3 px de la fila de arriba: las tres letras se ven", r.nn == 3 && nmal == 0);
}

/* ---- 7. La tabla de CC: P absorbida con signo y el balance ---- */

/* Una serie armada a mano: E1 de (0,1) a (0,0), W1 al revés de (1,0) a
 * (0,0) (con_w; si no, un cable), R1 de (1,0) a (1,1) y R2 de (1,1) a
 * (0,1), y la tierra en (1,1). Con E = 12, R1 = 1 y R2 = 3: I = 3 A, E1
 * entrega 36 W, R1 absorbe 9 y R2 27; el vatímetro marca V(b)·I, con V(b)
 * = V(R1) = 3 V y su I al revés: -9 W */
static void mr_serie(Circuito *c, double E, double R1, double R2, int con_w)
{
    int k;
    circ_init(c, "Serie");
    k = circ_elemento(c, EL_FUENTE, 0, 1, DIR_ARR);
    r3d_dato(&c->el[k].val, E);
    if (con_w) circ_elemento(c, EL_VATI, 1, 0, DIR_IZQ);
    else circ_cable(c, 1, 0, DIR_IZQ);
    k = circ_elemento(c, EL_RES, 1, 0, DIR_ABA);
    r3d_dato(&c->el[k].val, R1);
    k = circ_elemento(c, EL_RES, 1, 1, DIR_IZQ);
    r3d_dato(&c->el[k].val, R2);
    c->tierra_x = 1; c->tierra_y = 1;
}

/* La fila del balance, sola: con visto bueno si cuadra; si no, en rojo con
 * el residuo, y siempre en 52 columnas (con números largos, sin la cifra) */
static void mr_fila_balance(void)
{
    static const double MAG[] = { 0.0, 3e-15, 1.234e-11, 0.1234, 12.34, 1234.5, 1.2345e5, 1.2345e8, 9.9999e11, 1.234e21 };
    static const double RES[] = { 0.0, 1e-10, 1.234e-5, 0.5, 1.0 };
    const char *q = "tabla de CC: la fila del balance";
    char t[96];
    int a, b, r, n = 0, largo = 0, mal = 0, bien;
    bien = ui_fila_balance(t, sizeof t, 36.0, 36.0, 0.0);
    chk_cond(q, "cuadra: «Entregan 36 W " S_CDOT " absorben 36 W " S_CHECK "»",
             bien && strcmp(t, "Entregan 36 W " S_CDOT " absorben 36 W " S_CHECK) == 0);
    bien = ui_fila_balance(t, sizeof t, 36.0, 30.0, 6.0 / 66.0);
    printf("    no cuadra: «%s» (%d)\n", t, bien);
    chk_cond(q, "no cuadra: «(no cuadra: 9,1E-2)», sin visto bueno",
             !bien && strcmp(t, "Entregan 36 W " S_CDOT " absorben 30 W (no cuadra: 9,1E-2)") == 0);
    bien = ui_fila_balance(t, sizeof t, 36.0, 36.0, nan(""));
    chk_cond(q, "con un NaN: «(no cuadra)»", !bien && strstr(t, "(no cuadra)") != 0);
    for (a = 0; a < (int)(sizeof MAG / sizeof MAG[0]); a++)
        for (b = 0; b < (int)(sizeof MAG / sizeof MAG[0]); b++)
            for (r = 0; r < (int)(sizeof RES / sizeof RES[0]); r++) {
                bien = ui_fila_balance(t, sizeof t, MAG[a], MAG[b], RES[r]);
                n++;
                if ((int)strlen(t) > largo) largo = (int)strlen(t);
                if ((int)strlen(t) > 52 || bien != (RES[r] <= 1e-9) || strncmp(t, "Entregan ", 9) != 0) {
                    if (mal++ < 3) printf("    mal: «%s» (%d)\n", t, bien);
                }
            }
    printf("    %d filas de 1E-15 a 1E21 W: la m" S_aa "s larga, %d columnas\n", n, largo);
    chk_cond(q, "de 1E-15 a 1E21 W, cuadre o no: en 52 columnas y con su visto bueno solo si cuadra", mal == 0 && largo > 40);
}

/* Con un vatímetro: su fila dice «marca» y lo que marca con su signo; no
 * cuenta en el balance (lo que marca pasa hacia la carga) */
static void mr_tabla_vatimetro(void)
{
    static Circuito c;
    const char *q = "tabla de CC con un vatímetro";
    int k;
    mr_serie(&c, 12.0, 1.0, 3.0, 1);
    chk_cond(q, "la serie con W1, abierta", r3u_abre(&c));
    TECLAS(T_IGUAL, T_TAB);
    chk_cond(q, "[=] [tab]: la tabla de CC", ui_pantalla() == P_TABLA && !ui_res_nl());
    k = r4_el(ui_circuito(), "W1");
    printf("    W1: marca %.12g W, P abs %.12g W\n", k >= 0 ? ui_valores()->P[k] : 0.0,
           k >= 0 ? sol_p_abs(ui_circuito(), ui_valores(), k) : 1.0);
    chk_cond(q, "W1 marca -9 W y no absorbe", k >= 0 && fabs(ui_valores()->P[k] + 9.0) <= 1e-12 * 9.0 &&
             sol_p_abs(ui_circuito(), ui_valores(), k) == 0.0);
    /* lo que marca, en la columna del valor; bajo «P abs», su 0 W (la
     * columna suma lo del balance: antes llevaba ahí el -9 W) */
    chk_cond(q, "su fila: «marca -9 W» (con su signo) y 0 W en P abs", ve("\nmarca -9 W\n") && ve("\n0 W\n") &&
             !ve("\n-9 W\n"));
    chk_cond(q, "el balance, sin el vatímetro: 36 W y 36 W, cuadra",
             ve("Entregan 36 W " S_CDOT " absorben 36 W " S_CHECK) && !ve("no cuadra"));
    r4_vuelca("mr_tabla_vatimetro");
    TECLAS(T_ESC, T_ESC);
}

/* Potencias fuera de p..G: con exponente (y cuadran) */
static void mr_tabla_extremos(void)
{
    static Circuito c;
    const char *q = "tabla de CC: extremos";
    mr_serie(&c, 2e6, 5e-4, 5e-4, 0);                  /* 2 MV en 1 mΩ: 4E15 W */
    r3u_abre(&c);
    TECLAS(T_IGUAL, T_TAB);
    printf("    4E15: %d\n", ve("Entregan 4E15 W " S_CDOT " absorben 4E15 W " S_CHECK));
    chk_cond(q, "2 MV en 1 m" S_OHM ": «Entregan 4E15 W " S_CDOT " absorben 4E15 W " S_CHECK "»",
             ui_pantalla() == P_TABLA && ve("Entregan 4E15 W " S_CDOT " absorben 4E15 W " S_CHECK));
    TECLAS(T_ESC, T_ESC);
    mr_serie(&c, 2e-7, 0.5, 0.5, 0);                   /* 0,2 µV en 1 Ω: 4E-14 W */
    r3u_abre(&c);
    TECLAS(T_IGUAL, T_TAB);
    chk_cond(q, "0,2 " S_MU "V en 1 " S_OHM ": «Entregan 4E-14 W " S_CDOT " absorben 4E-14 W " S_CHECK "»",
             ui_pantalla() == P_TABLA && ve("Entregan 4E-14 W " S_CDOT " absorben 4E-14 W " S_CHECK));
    TECLAS(T_ESC, T_ESC);
}

/* El ruido de un cero: E1 = 770 MV con R1 = 0,11 y R2 = 0,37 Ω deja el
 * punto medio a 593,5 MV, y ahí E2 (de igual valor, por R3 = 0,05 Ω) no
 * debería dar corriente. Da 663 nA de ruido: su P es -394 W frente a 1E18
 * W, por debajo de 1E-12 de la escala, y sale «0 W». num_sig ya da «0» por
 * debajo de 0,5 nW; el umbral se nota con potencias así. E2 es el último
 * elemento: su P va justo antes de la fila del balance */
static void mr_tabla_ruido(void)
{
    static Circuito c;
    static Red r;
    static Solucion s;
    const char *q = "tabla de CC: el ruido de un cero";
    double ent, ab, p;
    int k;
    circ_init(&c, "Ruido");
    k = circ_elemento(&c, EL_FUENTE, 0, 1, DIR_ARR); r3d_dato(&c.el[k].val, 7.7e8);
    k = circ_elemento(&c, EL_RES, 0, 0, DIR_DER); r3d_dato(&c.el[k].val, 0.11);
    k = circ_elemento(&c, EL_RES, 1, 0, DIR_ABA); r3d_dato(&c.el[k].val, 0.37);
    circ_cable(&c, 1, 1, DIR_IZQ);
    k = circ_elemento(&c, EL_RES, 1, 0, DIR_DER); r3d_dato(&c.el[k].val, 0.05);
    k = circ_elemento(&c, EL_FUENTE, 2, 1, DIR_ARR); r3d_dato(&c.el[k].val, 7.7e8 * 0.37 / 0.48);
    circ_cable(&c, 2, 1, DIR_IZQ);
    red_extraer(&c, &r);
    resolver(&c, &r, &s);
    p = sol_p_abs(&c, &s.s[0], k);
    sol_balance(&c, &s.s[0], &ent, &ab);
    printf("    E2: P abs %.6g W (escala %.3g W)\n", p, ent + ab);
    chk_cond(q, "la P de E2 es ruido: no es 0, pero s" S_ii " menor que 1E-12 de la escala (y mayor que 0,5 nW)",
             p != 0.0 && fabs(p) <= 1e-12 * (ent + ab) && fabs(p) > 5e-10);
    chk_cond(q, "abierto", r3u_abre(&c));
    TECLAS(T_IGUAL, T_TAB);
    chk_cond(q, "en la tabla, la P de E2 sale «0 W»", ui_pantalla() == P_TABLA && ve("\n0 W\nEntregan "));
    TECLAS(T_ESC, T_ESC);
}

/* La tabla del punto de operación ([P] y la de Newton) con el mismo
 * convenio: «P abs», y en la red lineal los mismos números que la de [=];
 * en los no lineales de la biblioteca, sus P abs suman 0 (con el AO, que
 * entrega, y el BJT y el MOSFET, que absorben por sus dos puertos) */
static void mr_tabla_op(void)
{
    static Circuito c;
    const char *q = "tabla del punto de operaci" S_oo "n";
    int lib, i, k, n = 0, nmal = 0, tipos = 0;
    double peor = 0.0;
    mr_dos_fuentes();
    TECLAS(T_LETRA('P'), T_TAB);
    printf("    [P] [tab]: pantalla %d, nl %d; P abs %d, -36 W %d, 36 W %d\n", ui_pantalla(), ui_res_nl(), ve("P abs"),
           ve("\n-36 W\n"), ve("\n36 W\n"));
    chk_cond(q, "[P] [tab] en la red lineal: «P abs», E1 -36 W y E2 y R1 18 W, como con [=]",
             ui_pantalla() == P_TABLA && ui_res_nl() && ve("Red lineal, sin Newton") && ve("P abs") && ve("\n-36 W\n") &&
             ve("\n18 W\n") && !ve("\n36 W\n") && !ve("\n-18 W\n"));
    r4_vuelca("mr_tabla_op_lineal");
    TECLAS(T_ESC, T_ESC);
    /* la biblioteca (lib = 2: «El transistor como interruptor» con un
     * MOSFET de canal N en el sitio del BJT: no hay ninguno) */
    for (lib = 0; lib < 3; lib++)
        for (i = 0; i < (lib == 0 ? N_EJEMPLOS : N_MODELOS); i++) {
            double ent, ab, res;
            if (lib == 2 && strcmp(MODELOS[i].titulo, "El transistor como interruptor") != 0) continue;
            if (lib) modelo_cargar(i, &c); else ejemplo_cargar(i, &c);
            if (lib == 2)
                for (k = 0; k < c.nel; k++)
                    if (c.el[k].tipo == EL_BJT) {
                        int j;
                        c.el[k].tipo = EL_MOS;
                        c.el[k].variante = MOS_CN;
                        for (j = 0; j < NPAR; j++) c.el[k].par[j].est = M_LIBRE;   /* Vt 2 V y K 1 mA/V² */
                    }
            if (!circ_no_lineal(&c) || circ_alterna(&c)) continue;
            if (!r3u_abre(&c)) continue;
            TECLAS(T_LETRA('P'));                          /* con C y L: en t = 0-, sin P */
            if (ui_pantalla() != P_RESULTADO || !ui_res_nl()) { TECLAS(T_ESC); continue; }
            res = sol_balance(ui_circuito(), ui_valores(), &ent, &ab);
            if (!(ent + ab > 1e-9)) { TECLAS(T_ESC); continue; }   /* una senoidal en t = 0: sin potencias */
            n++;
            if (res > peor) peor = res;
            if (!(res <= 1e-6)) { nmal++; printf("    %s: no cuadra, %.3g\n", lib ? MODELOS[i].titulo : EJEMPLOS[i].titulo, res); }
            for (k = 0; k < c.nel; k++) {
                if (c.el[k].tipo == EL_AO) tipos |= 1;
                if (c.el[k].tipo == EL_BJT) tipos |= 2;
                if (c.el[k].tipo == EL_MOS) tipos |= 4;
            }
            TECLAS(T_ESC);
        }
    printf("    %d puntos de operaci" S_oo "n de la biblioteca (tipos %d): el peor |" S_SIGMA "P|/" S_SIGMA "|P| = %.3g\n", n, tipos,
           peor);
    chk_cond(q, "los no lineales de la biblioteca: " S_SIGMA "P abs = 0 (con AO, BJT y MOSFET)", n >= 8 && tipos == 7 && nmal == 0);
}

static void mr_tabla_cc(void)
{
    static Solucion s;
    const char *q = "tabla de CC";
    const Circuito *cu;
    const Valores *v;
    double ent, ab, res, peor = 0.0;
    int k, caso, nmal = 0, n = 0, nw = 0;
    unsigned guarda;

    chk_cond(q, "el circuito de dos fuentes", mr_dos_fuentes());
    TECLAS(T_IGUAL, T_TAB);
    cu = ui_circuito();
    v = ui_valores();
    chk_cond(q, "[=] [tab]: P_TABLA de CC", ui_pantalla() == P_TABLA && !ui_res_nl());
    k = r4_el(cu, "E1");
    chk_t(q, "P abs(E1) = -36 W (entrega)", k >= 0 ? sol_p_abs(cu, v, k) : 0.0, -36.0, 36.0, 1e-12);
    k = r4_el(cu, "E2");
    chk_t(q, "P abs(E2) = 18 W (absorbe: se carga)", k >= 0 ? sol_p_abs(cu, v, k) : 0.0, 18.0, 36.0, 1e-12);
    k = r4_el(cu, "R1");
    chk_t(q, "P abs(R1) = 18 W", k >= 0 ? sol_p_abs(cu, v, k) : 0.0, 18.0, 36.0, 1e-12);
    res = sol_balance(cu, v, &ent, &ab);
    printf("    entregan %.12g W, absorben %.12g W, residuo %.3g\n", ent, ab, res);
    chk_cond(q, "balance: entregan 36 y absorben 36", fabs(ent - 36.0) <= 1e-12 * 36.0 && fabs(ab - 36.0) <= 1e-12 * 36.0 && res <= 1e-12);
    chk_cond(q, "se ve «P abs», «-36 W» en E1 y «18 W» en E2 y R1",
             ve("P abs") && ve("\n-36 W\n") && ve("\n18 W\n") && !ve("\n36 W\n"));
    chk_cond(q, "la fila del balance, con su visto bueno, tras las de los tres elementos",
             ve("Entregan 36 W " S_CDOT " absorben 36 W " S_CHECK) && !ve("no cuadra") &&
             ve("\nE1\n") && ve("\nE2\n") && ve("\nR1\n"));
    r4_vuelca("mr_tabla_cc");
    TECLAS(T_ESC, T_ESC);

    /* 400 redes al azar (la del arnés, con un puente, una segunda E o J, y
     * a veces una E real, una controlada, un vatímetro con su carga y un V
     * y un A reales): la suma de las P abs es 0 a 1e-9 de la escala */
    guarda = semilla;
    semilla = 4242u;
    for (caso = 0; caso < 400; caso++) {
        double E = 1.0 + azar(240);
        int f, f2, u, w;
        f = fuente_sp(E);
        if (nodos > 2) nuevo_el(EL_RES, (int)azar((unsigned)nodos), (int)azar((unsigned)nodos));
        f2 = nuevo_el(azar(3) == 0 ? EL_CORR : EL_FUENTE, (int)azar((unsigned)nodos), (int)azar((unsigned)nodos));
        if (f2 >= 0) { rc.el[f2].val.v = 1.0 + azar(50); rc.el[f2].val.est = M_DATO; }
        if (caso % 2) {
            rc.el[f].aux.v = 0.1 + 0.1 * azar(50); rc.el[f].aux.est = M_DATO;          /* E real */
            u = (int)azar((unsigned)nodos); w = (u + 1 + (int)azar((unsigned)nodos - 1)) % nodos;
            k = nuevo_el(azar(2) ? EL_EDEP : EL_JDEP, u, w);
            if (k >= 0) {
                rc.el[k].ref = (signed char)azar((unsigned)k);
                rc.el[k].variante = (unsigned char)azar(2);
                rc.el[k].val.v = 0.1 + 0.1 * azar(20); rc.el[k].val.est = M_DATO;
            }
            u = (int)azar((unsigned)nodos); w = nodos++;                                 /* W y su carga */
            nuevo_el(EL_VATI, u, w);
            nuevo_el(EL_RES, w, (int)azar((unsigned)nodos - 1));
            k = nuevo_el(EL_VOLT, (int)azar((unsigned)nodos), (int)azar((unsigned)nodos));
            if (k >= 0) { rc.el[k].val.v = 1e4; rc.el[k].val.est = M_DATO; }
            u = (int)azar((unsigned)nodos); w = nodos++;
            k = nuevo_el(EL_AMP, u, w);
            if (k >= 0) { rc.el[k].val.v = 0.5; rc.el[k].val.est = M_DATO; }
            nuevo_el(EL_RES, w, (int)azar((unsigned)nodos - 1));
        }
        cierra_red();
        if (resolver(&rc, &rr, &s) != SOL_OK || s.estado != SOL_OK) continue;
        res = sol_balance(&rc, &s.s[0], &ent, &ab);
        n++;
        if (caso % 2) nw++;
        if (res > peor) peor = res;
        if (!(res <= 1e-9)) nmal++;
    }
    semilla = guarda;
    printf("    %d redes resueltas (%d con E real, controlada, W, V y A); el peor |" S_SIGMA "P|/" S_SIGMA "|P| = %.3g\n", n, nw,
           peor);
    chk_cond(q, "400 redes al azar: " S_SIGMA "P abs = 0 a 1e-9 en todas", n >= 200 && nw >= 80 && nmal == 0);

    mr_fila_balance();
    mr_tabla_vatimetro();
    mr_tabla_extremos();
    mr_tabla_ruido();
    mr_tabla_op();
}

/* ---- 9. CA: la página «En el tiempo» ---- */

/* E1 = 10 V (fasor, 50 Hz) de (0,1) a (0,0) (el + arriba); A1 ideal de
 * (0,0) a (1,0); R1 = 10 Ω de (1,0) a (1,1); V1 ideal en paralelo con R1,
 * de (2,0) a (2,1); y en la vuelta, E2 = 5 V de CC de (1,1) a (0,1). En la
 * alterna E2 es un cable: I = 1 A eficaz (1,414 de pico) */
static void mr_ca_instr(Circuito *c)
{
    int k;
    circ_init(c, "Instrumentos");
    k = circ_elemento(c, EL_FUENTE, 0, 1, DIR_ARR);
    c->el[k].variante = FORMA_FASOR;
    r3d_fasor(&c->el[k].val, 10.0, 0.0);
    circ_elemento(c, EL_AMP, 0, 0, DIR_DER);
    k = circ_elemento(c, EL_RES, 1, 0, DIR_ABA);
    r3d_dato(&c->el[k].val, 10.0);
    circ_cable(c, 1, 0, DIR_DER);
    circ_elemento(c, EL_VOLT, 2, 0, DIR_ABA);
    circ_cable(c, 2, 1, DIR_IZQ);
    k = circ_elemento(c, EL_FUENTE, 1, 1, DIR_IZQ);
    r3d_dato(&c->el[k].val, 5.0);
    r3d_dato(&c->frec, 50.0);
}

/* La página 5 sin lo nulo (la I del V ideal, la V del A ideal y la de E2,
 * que en la alterna es un cable) y con el aviso de la parte de CC */
static void mr_ca_tiempo_nulos(void)
{
    static Circuito c;
    const char *q = "CA en el tiempo: lo nulo";
    mr_ca_instr(&c);
    chk_cond(q, "E1, A1, R1, V1 y E2, abierto", r3u_abre(&c));
    TECLAS(T_IGUAL, T_TAB, T_IZQ);
    printf("    pantalla %d; 5/5 %d; «Solo la parte alterna» %d\n", ui_pantalla(), ve("5/5"), ve("Solo la parte alterna"));
    chk_cond(q, "con una E de CC: «Solo la parte alterna: falta la de CC (punto Q)»",
             ui_pantalla() == P_CA_TABLA && ve("5/5") && ve("Solo la parte alterna: falta la de CC (punto Q)") &&
             !ve("Amplitudes de pico"));
    chk_cond(q, "A1: solo su i (su v es 0)", ve("\nA1\ni = 1,414 cos(314,2t) A\nR1\n"));
    chk_cond(q, "V1: solo su v (su i es 0)", ve("\nV1\nv = 14,14 cos(314,2t) V\nE2\n"));
    chk_cond(q, "ninguna l" S_ii "nea con amplitud 0", !ve(" = 0 cos(") && !ve(" = 0 sen("));
    r4_vuelca("mr_ca_tiempo_nulos");
    TECLAS(T_ESC, T_ESC, T_ESC);
}

/* Con un no lineal (el rectificador de media onda, sin fuentes de CC): los
 * fasores del Bode ([B] y [tab]) son la pequeña señal en Q, y la página 5
 * lo dice */
static void mr_ca_tiempo_nl(void)
{
    static Circuito c;
    const char *q = "CA en el tiempo: un no lineal";
    int i = mr_modelo("Rectificador de media onda"), k, cc = 0;
    chk_cond(q, "el modelo del rectificador", i >= 0);
    if (i < 0) return;
    modelo_cargar(i, &c);
    for (k = 0; k < c.nel; k++)
        if ((c.el[k].tipo == EL_FUENTE || c.el[k].tipo == EL_CORR) && c.el[k].variante == ONDA_CC) cc++;
    r3u_abre(&c);
    TECLAS(T_LETRA('B'));
    r4_espera();
    chk_cond(q, "[B]: el Bode", ui_pantalla() == P_BODE);
    TECLAS(T_TAB, T_TAB, T_IZQ);
    printf("    pantalla %d, fuentes de CC %d; 5/5 %d\n", ui_pantalla(), cc, ve("5/5"));
    chk_cond(q, "sin fuentes de CC, la p" S_aa "gina 5 dice «Solo la parte alterna: falta la de CC (punto Q)»",
             cc == 0 && ui_pantalla() == P_CA_TABLA && ve("5/5") && ve("Solo la parte alterna: falta la de CC (punto Q)"));
    r4_vuelca("mr_ca_tiempo_nl");
    TECLAS(T_ESC, T_ESC, T_ESC, T_ESC);
}

static void mr_ca_tiempo(void)
{
    static Circuito c;
    const char *q = "CA en el tiempo";
    const CaRes *ca;
    char t[64], u[24];
    int i = mr_modelo("RLC serie en fasores"), k, j;
    double ip, ang;

    chk_cond(q, "el modelo «RLC serie en fasores»", i >= 0);
    if (i < 0) return;
    modelo_cargar(i, &c);
    r3u_abre(&c);
    TECLAS(T_IGUAL, T_TAB);
    chk_cond(q, "[=] [tab]: la tabla de CA", ui_pantalla() == P_CA_TABLA);
    chk_cond(q, "la primera es 1/5", ve("1/5"));
    TECLAS(T_IZQ);
    ca = ui_ca();
    k = r4_el(ui_circuito(), "E1");
    ip = k >= 0 ? cx_abs(ca->I[k]) : 0.0;
    ang = k >= 0 ? cx_arg(ca->I[k]) * 180.0 / PI_T : 0.0;
    sprintf(t, "i = %.2f cos(377,0t + %.1f" S_DEG ") A", ip, ang);
    for (j = 0; t[j]; j++) if (t[j] == '.') t[j] = ',';
    printf("    E1: I de pico %.9g A a %.6g" S_DEG "; se ve «%s» %d\n", ip, ang, t, ve(t));
    chk_cond(q, "[izq]: la 5/5, «En el tiempo» con " S_OMEGA " = 377,0 rad/s",
             ve("5/5") && ve("En el tiempo (" S_OMEGA " = 377,0 rad/s)"));
    chk_cond(q, "la I de pico: 13,46 cos(377,0t + 37,5" S_DEG ") A", ve("i = 13,46 cos(377,0t + 37,5" S_DEG ") A") && ve(t));
    chk_t(q, "la I de pico es la eficaz por " S_SQRT "2", ip, 9.518861634600457 * sqrt(2.0), 13.46, 1e-9);
    chk_cond(q, "con coseno: lo dice", ve("Amplitudes de pico, con coseno (el convenio)"));
    /* la E, a 0°: sin «+ 0,0°» */
    chk_cond(q, "la E: v = 169,7 cos(377,0t) V, sin + 0,0" S_DEG, ve("\nv = 169,7 cos(377,0t) V\n") && !ve("+ 0,0" S_DEG));
    r4_vuelca("mr_ca_tiempo");
    for (j = 0; j < 30; j++) TECLAS(T_ABAJO);
    chk_cond(q, "[abajo] no se sale", ui_pantalla() == P_CA_TABLA && ve("5/5"));
    TECLAS(T_DER);
    chk_cond(q, "[der] desde la 5/5 vuelve a la 1/5", ve("1/5"));

    /* De pico y con seno: el ángulo gana 90° */
    c.conv = CONV_PICO | CONV_SENO;
    r3u_abre(&c);
    TECLAS(T_IGUAL, T_TAB, T_IZQ);
    chk_cond(q, "de pico y seno: 13,46 sen(377,0t + 127,5" S_DEG ") A",
             ve("i = 13,46 sen(377,0t + 127,5" S_DEG ") A") && ve("con seno"));
    /* la E: 120 V eficaces son 169,7 de pico, sin ángulo */
    chk_cond(q, "la E: v = 169,7 sen(377,0t + 90,0" S_DEG ") V", ve("v = 169,7 sen(377,0t + 90,0" S_DEG ") V"));
    TECLAS(T_ESC, T_ESC);

    /* ω = 1E9 rad/s, sin prefijo y, desde 1E6, con exponente: «1E9t» (no
     * «1000000000t», diez cifras que costaba leer); ningún texto se sale */
    modelo_cargar(i, &c);
    r3d_dato(&c.frec, 1e9 / (2.0 * PI_T));
    r3u_abre(&c);
    TECLAS(T_IGUAL, T_TAB, T_IZQ);
    j = gfx_desbordes();
    sprintf(t, "En el tiempo (" S_OMEGA " = 1E9 rad/s)");
    chk_cond(q, "con " S_OMEGA " = 1E9: «1E9t» y nada se sale de la pantalla",
             ui_pantalla() == P_CA_TABLA && ve(t) && ve("cos(1E9t") && !ve("1000000000") && gfx_desbordes() == j);
    r4_vuelca("mr_ca_tiempo_1e9");
    TECLAS(T_ESC, T_ESC);
    (void)u;
    mr_ca_tiempo_nulos();
    mr_ca_tiempo_nl();
}

/* Un registro sintético: una rampa de 0 a 9 V en 10 ms y luego un rizado
 * de 8 a 9 V a 60 Hz (canal 1), y 100 V + 2 V a 60 Hz (canal 2); 110 ms,
 * 500 muestras por periodo */
static void mr_reg_rizado(void)
{
    int i;
    greg.n = 0;
    for (i = 0; i <= 3300; i++) {
        double t = i * (1.0 / 30000.0);
        double y = t < 0.01 ? 900.0 * t : 8.5 + 0.5 * sin(2.0 * PI_T * 60.0 * (t - 0.01));
        pon(&greg, t, y, 100.0 + 2.0 * sin(2.0 * PI_T * 60.0 * t));
    }
    cierra_reg(&greg);
    memset(&gres, 0, sizeof gres);
    gres.t_fin = gres.t_pedido = greg.t[greg.n - 1]; gres.fin = FIN_COMPLETO;
    gres.canal[0].tipo = CANAL_V; gres.canal[1].tipo = CANAL_V;
    strcpy(gres.canal[0].nombre, "V(C1)"); strcpy(gres.canal[1].nombre, "V(E1)");
}

/* ---- 11. [U] el último periodo y las medidas entre los cursores ---- */
static void mr_ultimo_periodo(void)
{
    static Osc o;
    static Circuito c;
    Medidas m;
    const char *q = "[U] " S_uu "ltimo periodo";
    double ta = 0.0, tb = 0.0, med1, med2;
    int i;

    /* Pura: 10 sen(2π·60t) con una rampa que se apaga en 20 ms */
    greg.n = 0;
    for (i = 0; i <= 3000; i++) {
        double t = i * (1.0 / 30000.0);
        pon(&greg, t, 10.0 * sin(2.0 * PI_T * 60.0 * t) + (t < 0.02 ? 5.0 * (1.0 - t / 0.02) : 0.0), 0.0);
    }
    cierra_reg(&greg);
    chk_cond(q, "senoidal con rampa: hay periodo", osc_ultimo_periodo(&greg, 0, &ta, &tb));
    printf("    ta %.12g, tb %.12g, T %.12g (1/60 = %.12g)\n", ta, tb, tb - ta, 1.0 / 60.0);
    chk_t(q, "tb = el final del registro", tb, 0.1, 0.1, 1e-15);
    chk_t(q, "tb - ta = 1/60 a 1e-9 s", tb - ta, 1.0 / 60.0, 1.0, 1e-9);
    osc_medidas(&greg, 0, ta, tb, &m);
    chk_t(q, "la media del periodo, 0", m.media, 0.0, 10.0, 1e-6);
    chk_t(q, "el eficaz del periodo, 10/" S_SQRT "2", m.eficaz, 10.0 / sqrt(2.0), 7.07, 1e-6);
    /* el rizado tras la carga: el nivel del registro entero no lo cruza */
    mr_reg_rizado();
    chk_cond(q, "el rizado tras la rampa: hay periodo de 1/60",
             osc_ultimo_periodo(&greg, 0, &ta, &tb) && fabs(tb - ta - 1.0 / 60.0) <= 1e-9);
    /* una carga de RC: no se repite */
    greg.n = 0;
    for (i = 0; i <= 2000; i++) { double t = i * 2.5e-6; pon(&greg, t, 10.0 * (1.0 - exp(-t / 1e-3)), 0.0); }
    cierra_reg(&greg);
    chk_cond(q, "la carga de un RC: sin periodo", !osc_ultimo_periodo(&greg, 0, &ta, &tb));
    memset(&gres, 0, sizeof gres);
    gres.t_fin = gres.t_pedido = 5e-3; gres.fin = FIN_COMPLETO;
    gres.canal[0].tipo = CANAL_V; gres.canal[1].tipo = CANAL_NADA;
    strcpy(gres.canal[0].nombre, "V(C1)");
    osc_nuevo(&o, &greg, &gres, 1);
    osc_tecla(&o, &greg, &gres, T_LETRA('U'));
    chk_cond(q, "[U] sin periodo: lo avisa y no enciende los cursores",
             strstr(o.aviso, "Sin periodo") != 0 && !o.cursores);
    /* registros cortos, una senoidal pura de 60 Hz: con 2,1 y 2,7 periodos
     * hay periodo (el registro entero, al final); con 1,8, no */
    {
        static const double TF[3] = { 0.045, 0.035, 0.030 };
        int caso, hay[3];
        double T[3];
        for (caso = 0; caso < 3; caso++) {
            greg.n = 0;
            for (i = 0; i <= (int)(TF[caso] * 30000.0 + 0.5); i++) {
                double t = i * (1.0 / 30000.0);
                pon(&greg, t, 10.0 * sin(2.0 * PI_T * 60.0 * t), 0.0);
            }
            cierra_reg(&greg);
            hay[caso] = osc_ultimo_periodo(&greg, 0, &ta, &tb);
            T[caso] = hay[caso] ? tb - ta : 0.0;
            printf("    senoidal de %.0f ms (%.1f periodos): periodo %d, T %.12g\n", TF[caso] * 1e3, TF[caso] * 60.0, hay[caso], T[caso]);
        }
        chk_cond(q, "registros cortos: con 2,7 y 2,1 periodos, T = 1/60 (a 1e-9 s); con 1,8, sin periodo",
                 hay[0] && hay[1] && !hay[2] && fabs(T[0] - 1.0 / 60.0) <= 1e-9 && fabs(T[1] - 1.0 / 60.0) <= 1e-9);
    }
    /* [U] en el rizado de 60 Hz: la frec es 1/T; otra emulación con las
     * escalas a mano (una senoidal de 50 Hz) con T1 y T2 en el mismo sitio:
     * ya no es la de [U] (en 16,7 ms no caben dos cruces de 50 Hz) */
    mr_reg_rizado();
    osc_nuevo(&o, &greg, &gres, 1);
    osc_tecla(&o, &greg, &gres, T_LETRA('U'));
    osc_dibuja(&o, &greg, &gres);
    med1 = o.med[0].hay_frec ? o.med[0].frec : 0.0;
    ta = o.tc[0]; tb = o.tc[1];
    greg.n = 0;
    for (i = 0; i <= 3300; i++) { double t = i * (1.0 / 30000.0); pon(&greg, t, 8.5 + 0.5 * sin(2.0 * PI_T * 50.0 * t), 0.0); }
    cierra_reg(&greg);
    osc_nuevo(&o, &greg, &gres, 0);
    osc_dibuja(&o, &greg, &gres);
    med2 = o.med[0].hay_frec ? o.med[0].frec : 0.0;
    printf("    [U] a 60 Hz: frec %.9g; otra emulaci" S_oo "n (50 Hz), T1 y T2 iguales: frec %.9g\n", med1, med2);
    chk_cond(q, "la frec 1/T de [U] no pasa a otra emulaci" S_oo "n", fabs(med1 - 60.0) <= 1e-6 && fabs(med2 - 60.0) > 1e-3 &&
             o.cursores && o.tc[0] == ta && o.tc[1] == tb);
    /* T2 antes que T1: las medidas son las de [T2, T1] */
    mr_reg_rizado();
    osc_nuevo(&o, &greg, &gres, 1);
    o.cursores = 1; o.panel = OSC_MEDIDAS; o.tc[0] = 0.09; o.tc[1] = 0.07;
    osc_dibuja(&o, &greg, &gres);
    osc_medidas(&greg, 0, 0.07, 0.09, &m);
    printf("    T1 = 90 ms y T2 = 70 ms: media %.12g (la de [70, 90] ms: %.12g)\n", o.med[0].media, m.media);
    chk_cond(q, "T2 antes que T1: las medidas de [T2, T1]", o.med[0].hay && o.med[0].media == m.media && o.med[0].max == m.max);
    /* el tramo que se mide, sombreado (C_PANEL) entre T1 y T2 y no fuera */
    {
        double td = ESCALA[o.itdiv];
        int xa = OSC_X0 + (int)floor((0.07 - o.tizq) / td * OSC_DX + 0.5), xb = OSC_X0 + (int)floor((0.09 - o.tizq) / td * OSC_DX + 0.5);
        int x, y, dentro_ = 0, fuera = 0, alto = 8 * OSC_DY;
        for (x = xa + 1; x < xb; x++) if ((x - OSC_X0) % OSC_DX != 0) break;      /* una columna sin raya de la retícula */
        for (y = OSC_Y0; y < OSC_Y0 + alto; y++) dentro_ += es_px(x, y) == C_PANEL;
        for (y = OSC_Y0; y < OSC_Y0 + alto; y++) fuera += es_px(xa - 3, y) == C_PANEL;
        printf("    sombreado: columna %d (T1-T2 de %d a %d): %d de %d en C_PANEL; fuera, %d\n", x, xa, xb, dentro_, alto, fuera);
        chk_cond(q, "el tramo T1-T2, sombreado; fuera, no", dentro_ >= alto / 2 && fuera == 0);
    }

    /* En la interfaz: el filtro de media onda (lección 12) */
    i = mr_modelo("Media onda con filtro: el rizado");
    chk_cond(q, "el modelo del filtro", i >= 0);
    if (i < 0) return;
    modelo_cargar(i, &c);
    r3u_abre(&c);
    TECLAS(T_IGUAL);
    r4_espera();
    chk_cond(q, "[=]: el osciloscopio", ui_pantalla() == P_OSC);
    printf("    antes de [U]: media 8,55V %d; canal 1 = %s\n", ve("8,55V"), sim_res()->canal[0].nombre);
    TECLAS(T_LETRA('U'));
    ve("");
    printf("    [U]: T1 %.9g, T2 %.9g, t_fin %.9g; media %.9g, p-p %.9g\n", ui_osc()->tc[0], ui_osc()->tc[1],
           sim_res()->t_fin, ui_osc()->med[0].media, ui_osc()->med[0].pp);
    chk_cond(q, "[U]: los cursores en el " S_uu "ltimo periodo (1/60 s) y el panel de medidas",
             ui_osc()->cursores && ui_osc()->panel == OSC_MEDIDAS && fabs(ui_osc()->tc[1] - sim_reg()->t[sim_reg()->n - 1]) <= 1e-12 &&
             fabs(ui_osc()->tc[1] - ui_osc()->tc[0] - 1.0 / 60.0) <= 1e-6);
    chk_cond(q, "[U]: media 8,67V y rizado 1,29V p-p, como dice el modelo; sin el 8,55V de la ventana",
             ve("8,67V") && ve("1,29V") && !ve("8,55V"));
    chk_cond(q, "[U]: la marca y el aviso", ve("medidas en T1-T2") && ve("Último periodo: 16,67 ms"));
    {
        const Osc *uo = ui_osc();
        double T = uo->tc[1] - uo->tc[0];
        int it = osc_escala_ge(T / 8.0);
        printf("    [U]: t/div %.6g (T/8 = %.6g), ventana [%.9g, %.9g]; frec %d %.9g Hz\n", ESCALA[uo->itdiv], T / 8.0, uo->tizq,
               uo->tizq + 10.0 * ESCALA[uo->itdiv], uo->med[0].hay_frec, uo->med[0].frec);
        chk_cond(q, "[U]: t/div, el primero que deja el periodo en 8 divisiones o menos", uo->itdiv == it);
        chk_cond(q, "[U]: la ventana acaba en el final del registro",
                 fabs(uo->tizq + 10.0 * ESCALA[uo->itdiv] - sim_reg()->t[sim_reg()->n - 1]) <= 1e-12);
        chk_cond(q, "[U]: la frec de la medida es 1/T (60Hz)", uo->med[0].hay_frec && fabs(uo->med[0].frec * T - 1.0) <= 1e-12 &&
                 ve("\n60Hz\n"));
    }
    r4_vuelca("mr_osc_ultimo_periodo");
    /* T1 a 99 ms: T1-T2 es 1 ms y tiene pocas muestras; [=] emula más fino
     * y T1 y T2 siguen donde estaban */
    {
        double t0 = ui_osc()->tc[0], t1 = ui_osc()->tc[1], h0 = sim_res()->h;
        int n0;
        TECLAS(T_DIG(9), T_DIG(9), T_LETRA('M'), T_ENTER, T_LETRA('M'));
        ve("");
        t0 = ui_osc()->tc[0]; n0 = ui_osc()->n_med;
        printf("    T1 %.9g, T2 %.9g: %d muestras (en la ventana %d); h %.6g\n", t0, t1, n0, ui_osc()->n_vent, h0);
        chk_cond(q, "T1-T2 de 1 ms: «T1-T2: pocas muestras [=]»", fabs(t0 - 0.099) <= 1e-12 && n0 < 150 &&
                 ui_osc()->panel == OSC_MEDIDAS && ve("T1-T2: pocas muestras [=]"));
        r4_vuelca("mr_osc_pocas_muestras");
        TECLAS(T_IGUAL);
        r4_espera();
        ve("");
        printf("    tras [=]: pantalla %d, cursores %d, T1 %.9g, T2 %.9g, %d muestras; h %.6g\n", ui_pantalla(), ui_osc()->cursores,
               ui_osc()->tc[0], ui_osc()->tc[1], ui_osc()->n_med, sim_res()->h);
        chk_cond(q, "[=]: m" S_aa "s fino, y T1 y T2 donde estaban, ya con 150 muestras o m" S_aa "s",
                 ui_pantalla() == P_OSC && ui_osc()->cursores && ui_osc()->tc[0] == t0 && ui_osc()->tc[1] == t1 &&
                 sim_res()->h < h0 && ui_osc()->n_med >= 150 && ve("medidas en T1-T2") && !ve("pocas muestras"));
        r4_vuelca("mr_osc_pocas_muestras_igual");
    }
    TECLAS(T_ESC);
    r3u_abre(&c);
    TECLAS(T_IGUAL);
    r4_espera();
    TECLAS(T_LETRA('U'));
    /* [M]: los cursores; otra vez [M], las medidas */
    TECLAS(T_LETRA('M'));
    chk_cond(q, "[M]: el panel de cursores, sin la marca", ui_osc()->panel == OSC_CURSORES && !ve("medidas en T1-T2"));
    TECLAS(T_LETRA('M'));

    /* Cursores a mano: las medidas son las de [T1, T2] y cambian al moverlos */
    TECLAS(T_ESC);
    TECLAS(T_IGUAL);
    r4_espera();
    TECLAS(T_LETRA('X'), T_LETRA('M'));
    ve("");
    osc_medidas(sim_reg(), 0, ui_osc()->tc[0] < ui_osc()->tc[1] ? ui_osc()->tc[0] : ui_osc()->tc[1],
                ui_osc()->tc[0] < ui_osc()->tc[1] ? ui_osc()->tc[1] : ui_osc()->tc[0], &m);
    med1 = ui_osc()->med[0].media;
    chk_cond(q, "[X] [M]: la media es la de [T1, T2]", fabs(med1 - m.media) <= 1e-12 * (fabs(m.media) + 1.0));
    repite(T_DER, 10);                                /* T1, una división */
    ve("");
    med2 = ui_osc()->med[0].media;
    printf("    cursores a mano: media %.9g y, movido T1, %.9g\n", med1, med2);
    chk_cond(q, "mover T1 cambia la media", med2 != med1);
    TECLAS(T_ESC);

    /* El fijador: el mínimo del último periodo, -9,28 V */
    i = mr_modelo("Fijador (sujetador) negativo");
    chk_cond(q, "el modelo del fijador", i >= 0);
    if (i < 0) return;
    modelo_cargar(i, &c);
    r3u_abre(&c);
    TECLAS(T_IGUAL);
    r4_espera();
    TECLAS(T_LETRA('U'));
    ve("");                                           /* las medidas son las del dibujo: las de [U] */
    printf("    fijador: m" S_ii "n %.9g (clave %.9g)\n", ui_osc()->med[0].min, MODELOS[i].clave);
    chk_cond(q, "el fijador: m" S_ii "n -9,28V", ui_pantalla() == P_OSC && ve("-9,28V") &&
             fabs(ui_osc()->med[0].min - MODELOS[i].clave) <= 1e-3 * fabs(MODELOS[i].clave));
    TECLAS(T_ESC);
}

/* ---- 13. El disparo al nivel de lo que se ve ---- */
static void mr_disparo(void)
{
    static Osc o;
    static Circuito c;
    const char *q = "disparo";
    double t0, t1, niv;
    int i;

    /* Puro: la ventana en el rizado (tras la rampa de 0 a 9 V) */
    mr_reg_rizado();
    osc_nuevo(&o, &greg, &gres, 1);
    o.itdiv = (signed char)osc_escala_ge(5e-3);
    o.tizq = 0.05;
    osc_tecla(&o, &greg, &gres, T_LETRA('D'));
    printf("    D: disparo %d, nivel %.9g, tizq %.9g, aviso «%s»\n", o.disparo, o.nivel_disp, o.tizq, o.aviso);
    chk_cond(q, "en el rizado: dispara subiendo, sin aviso", o.disparo == DISP_SUBIDA && o.aviso[0] == 0);
    chk_t(q, "el nivel, el medio de lo que se ve (8,5 V)", o.nivel_disp, 8.5, 8.5, 1e-4);
    t0 = o.tizq;
    osc_tecla(&o, &greg, &gres, T_DER);
    chk_t(q, "[der]: un periodo del rizado", o.tizq - t0, 1.0 / 60.0, 1.0 / 60.0, 1e-6);
    /* la marca de la derecha, en el nivel */
    gfx_textos_reset();
    osc_dibuja(&o, &greg, &gres);
    {
        int y = osc_pix(8.5, ESCALA[o.ivdiv[0]], o.pos[0]);
        chk_cond(q, "el tri" S_aa "ngulo del nivel, en 8,5 V", es_px(316, y) == C_CANAL1);
    }
    /* [C]: el nivel pasa al canal 2 (100 V) */
    osc_tecla(&o, &greg, &gres, T_LETRA('C'));
    niv = o.nivel_disp;
    osc_tecla(&o, &greg, &gres, T_DER);
    printf("    [C]: nivel %.9g; aviso «%s»\n", niv, o.aviso);
    chk_t(q, "[C]: el nivel del canal 2 en la ventana (100 V)", niv, 100.0, 100.0, 1e-3);
    chk_cond(q, "[C] y [der]: dispara en el canal 2 sin aviso", o.activo == 1 && o.aviso[0] == 0);
    /* la ventana pasado el final (sin muestras): el medio del registro entero */
    osc_nuevo(&o, &greg, &gres, 1);
    o.tizq = 0.2;
    osc_tecla(&o, &greg, &gres, T_LETRA('D'));
    printf("    ventana sin muestras: nivel %.9g\n", o.nivel_disp);
    chk_t(q, "sin muestras en la ventana: el medio del registro entero (4,5 V)", o.nivel_disp, 4.5, 4.5, 1e-12);

    /* En la interfaz: el filtro de media onda, en el rizado */
    i = mr_modelo("Media onda con filtro: el rizado");
    if (i < 0) { chk_cond(q, "el modelo del filtro", 0); return; }
    modelo_cargar(i, &c);
    r3u_abre(&c);
    TECLAS(T_IGUAL);
    r4_espera();
    TECLAS(T_MAS);                                     /* 5 ms/div */
    for (i = 0; i < 10; i++) TECLAS(T_DER);            /* hasta el final: [50, 100] ms */
    t0 = ui_osc()->tizq;
    TECLAS(T_LETRA('D'));
    printf("    modelo: tizq %.9g -> %.9g, disparo %d, nivel %.9g\n", t0, ui_osc()->tizq, ui_osc()->disparo, ui_osc()->nivel_disp);
    chk_cond(q, "el filtro en el rizado: [D] dispara subiendo, sin «no cruza»",
             ui_pantalla() == P_OSC && ui_osc()->disparo == DISP_SUBIDA && !ve("no cruza"));
    t0 = ui_osc()->tizq;
    TECLAS(T_DER);
    t1 = ui_osc()->tizq;
    printf("    [der]: %.12g (1/60 = %.12g)\n", t1 - t0, 1.0 / 60.0);
    chk_t(q, "[der]: un periodo del rizado (16,67 ms)", t1 - t0, 1.0 / 60.0, 1.0 / 60.0, 1e-6);
    r4_vuelca("mr_osc_disparo_rizado");
    TECLAS(T_ESC);

    /* Otro circuito con las escalas a mano (V/div tocado): el disparo sigue
     * puesto y su nivel se rehace con la señal nueva (el filtro con E/10) */
    {
        static Circuito c2;
        int j, k;
        double lo, hi, e2 = 0.0;
        modelo_cargar(i = mr_modelo("Media onda con filtro: el rizado"), &c);
        c2 = c;
        for (k = 0; k < c2.nel; k++) if (c2.el[k].tipo == EL_FUENTE) c2.el[k].val.v *= 0.1;
        chk_cond(q, "otro circuito: el filtro, abierto", r3u_abre(&c));
        arch_guardar(1, &c2);
        TECLAS(T_IGUAL);
        r4_espera();
        TECLAS(T_ARRIBA, T_LETRA('D'));
        niv = ui_osc()->nivel_disp;
        chk_cond(q, "otro circuito: [arriba] y [D], con el disparo puesto", ui_osc()->ajustado && ui_osc()->disparo != DISP_LIBRE);
        TECLAS(T_ESC);
        for (j = 0; j < 4 && ui_pantalla() != P_TALLER; j++) TECLAS(T_ESC);
        TECLAS(T_ABAJO, T_ABAJO, T_ENTER);                 /* Mis problemas */
        TECLAS(T_ARRIBA, T_ARRIBA, T_ABAJO, T_ENTER);      /* el segundo */
        for (k = 0; k < ui_circuito()->nel; k++) if (ui_circuito()->el[k].tipo == EL_FUENTE) e2 = ui_circuito()->el[k].val.v;
        TECLAS(T_IGUAL);
        r4_espera();
        lo = sim_reg()->ymin[ui_osc()->activo]; hi = sim_reg()->ymax[ui_osc()->activo];
        printf("    otro circuito (E = %.6g): nivel %.6g -> %.6g; la se" S_nn "al va de %.6g a %.6g\n", e2, niv,
               ui_osc()->nivel_disp, lo, hi);
        chk_cond(q, "otro circuito: el nivel, de la se" S_nn "al nueva (dentro de su m" S_ii "nimo y su m" S_aa "ximo)",
                 ui_pantalla() == P_OSC && ui_osc()->disparo != DISP_LIBRE && ui_osc()->nivel_disp > lo &&
                 ui_osc()->nivel_disp < hi && ui_osc()->nivel_disp < 0.5 * niv);
        TECLAS(T_DER);
        chk_cond(q, "otro circuito: [der] dispara, sin «no cruza»", ui_osc()->aviso[0] == 0 && !ve("no cruza"));
        r4_vuelca("mr_osc_disparo_otro");
        TECLAS(T_ESC);
    }
}

/* ---- La Ayuda cuenta lo nuevo ---- */
static int mr_busca_ayuda(const char *t)
{
    int j;
    ui_init();
    TECLAS(T_ENTER, T_ABAJO, T_ABAJO, T_ENTER);                 /* menú > Ayuda */
    for (j = 0; j < 400 && ui_pantalla() == P_AYUDA && !ve(t); j++) TECLAS(T_ABAJO);
    return ui_pantalla() == P_AYUDA && ve(t);
}

static void mr_ayuda(void)
{
    const char *q = "Ayuda";
    /* se busca la última línea de cada trozo: al aparecer abajo, las de
     * antes también se ven */
    chk_cond(q, "el osciloscopio: [U], las medidas entre T1 y T2 y [=] con cursores",
             mr_busca_ayuda("cursores). [tab] la tabla; [menu] tiempo y paso.") &&
             ve("[U] el " S_uu "ltimo periodo (el r" S_ee "gimen permanente):") &&
             ve("con cursores, las medidas son entre T1 y T2.") &&
             ve("medio de lo que se ve. [A] autoescala. [=] emula") &&
             ve("hasta donde miras, o m" S_aa "s fino (en T1-T2 si hay"));
    chk_cond(q, "la CA: la tabla en el tiempo", mr_busca_ayuda("Tellegen), nodos y en el tiempo: v(t) e i(t) de"));
    chk_cond(q, "el resultado de CC: P abs, el balance y las letras",
             mr_busca_ayuda("abajo el balance: entregan = absorben.") &&
             ve("nombres. [tab] tabla completa: P abs es la que") &&
             ve("fuente real, la de bornes y no la de la fem), y") &&
             ve("potenciales de nodo (con su letra: B 7,5V) o"));
    TECLAS(T_ESC);
}

/* HAKARU_MR=<trozo del nombre> corre solo las partes cuyo nombre lo lleva
 * (para depurar: HAKARU_MR=letras) */
static int mr_toca(const char *nombre)
{
    const char *s = getenv("HAKARU_MR");
    return !s || strstr(nombre, s) != 0;
}

static void test_m_resultados(void)
{
    printf("[69] Mejoras de resultados e instrumentos\n");
    gfx_desbordes_reset();
    if (mr_toca("letras")) mr_letras();
    if (mr_toca("letras_biblioteca")) mr_letras_biblioteca();
    if (mr_toca("letras_potenciales")) mr_letras_potenciales();
    if (mr_toca("letras_apretado")) mr_letras_apretado();
    if (mr_toca("letras_recorte")) mr_letras_recorte();
    if (mr_toca("tabla_cc")) mr_tabla_cc();
    if (mr_toca("ca_tiempo")) mr_ca_tiempo();
    if (mr_toca("ultimo_periodo")) mr_ultimo_periodo();
    if (mr_toca("disparo")) mr_disparo();
    if (mr_toca("ayuda")) mr_ayuda();
    limpia_mis();
    chk_cond("interfaz", "ning" S_uu "n texto fuera de la pantalla", gfx_desbordes() == 0);
}
