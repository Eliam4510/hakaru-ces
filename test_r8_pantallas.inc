/* ============================================================================
 *  test_r8_pantallas.inc  -  [74] Las pantallas de la verificación final
 *  (rev8/verificacion_final.json, frentes «bmp» e «integracion»). Una prueba
 *  por arreglo; cada una falla sin él (comprobado por mutación).
 *
 *    P1  las letras de los nodos con terminales: A y B son las de sus nodos
 *        y los demás siguen con las libres (red_letra); V(AB) = VA - VB, la
 *        capa Potenciales sin dos «B», la letra amarilla que no pisa un
 *        rótulo, los pasos, la tabla de CA y el canal del osciloscopio
 *    P2  el osciloscopio: los rótulos, encima de las líneas de T1 y T2, y
 *        las cajitas «1» y «2» que no se montan
 *    P3  la capa Potenciales: rótulos con un píxel de aire («C 12V +»)
 *    P4  los rótulos no salen por encima del recorte (la barra de título)
 *    P5  la biblioteca y los circuitos de las pruebas nuevas: ningún rótulo
 *        pisa otro ni tapa o toca el símbolo de otro elemento
 *    P6  el recuadro del elegido encierra su símbolo y sus rótulos
 *    P7  un campo con una cuenta larga: «« y caracteres enteros
 *    P8  los títulos largos: Circuito.nombre[40], la sección 3 del CESP, la
 *        barra que corta con «...» y la lista de Mis problemas
 *    P9  textos: «Valor/reg.» y los prefijos de las tablas, el balance en el
 *        panel vacío y ω con exponente
 *    (P11 es de la versión especial: ESP_P8_P11)
 *
 *  Los tres arreglos que el frente «integracion» hizo también (el aviso de
 *  [G] en un punto suelto, el enunciado traído con [tab] en dos líneas y
 *  el punto 5 de la Ayuda) se prueban una vez, en [73] C6 (test_cierre.inc):
 *  de aquí salieron la Ayuda y la tierra de P9 y P10 entera.
 *
 *  Se incluye tras test_cierre.inc (usa lazo, r3u_abre, r4_vuelca, r4_el,
 *  es_px, mr_cuenta_glifo, mr_glifo_en, mr_dos_fuentes, mr_modelo,
 *  me_ca_sin_f, me_ca_z, me_lazo_ancho, fl_c_cuentas, lector_e1,
 *  limpia_mis, ve, TECLAS y dir_out). Va al final de la batería y sola
 *  con HAKARU_SOLO_PANTALLAS=1. Sus pantallas van a
 *  out/p8_*.bmp, fuera de la numeración de vuelca().
 * ========================================================================= */

/* ---- P1. Las letras de los nodos con terminales ---- */

/* Las letras de los nodos de c (lazo: (0,0) el + de E1, (1,0) y (0,1) el
 * -) con los terminales en ta y tb (x, y; -1 = no está), en l[0..2] */
static void p8_letras_lazo(int tax, int tay, int tbx, int tby, char l[3])
{
    static Circuito c;
    static Red r;
    int n;
    lazo(&c, &r, 12.0, 4.0, 12.0);
    c.term_x[0] = (signed char)tax; c.term_y[0] = (signed char)tay;
    c.term_x[1] = (signed char)tbx; c.term_y[1] = (signed char)tby;
    red_extraer(&c, &r);
    for (n = 0; n < 3; n++) l[n] = n < r.nn ? red_letra(&r, n) : '?';
}

static void p8_letras_terminales(void)
{
    static Circuito c;
    static Red r;
    const char *q = "letras de nodo con terminales";
    char l[3];
    int k, x, y, xa, ya, ng, na, n, ok;

    /* En el lazo los nodos son (0,0), (1,0) y (0,1), en ese orden */
    p8_letras_lazo(-1, -1, -1, -1, l);
    chk_cond(q, "sin terminales: A, B y C en orden de lectura", l[0] == 'A' && l[1] == 'B' && l[2] == 'C');
    p8_letras_lazo(0, 0, 0, 1, l);
    printf("    A en (0,0) y B en (0,1): %c %c %c\n", l[0], l[1], l[2]);
    chk_cond(q, "A en el nodo 0 y B en el 2: A, C y B", l[0] == 'A' && l[1] == 'C' && l[2] == 'B');
    p8_letras_lazo(1, 0, 0, 0, l);
    chk_cond(q, "A en el nodo 1 y B en el 0: B, A y C", l[0] == 'B' && l[1] == 'A' && l[2] == 'C');
    p8_letras_lazo(-1, -1, 1, 0, l);
    chk_cond(q, "solo B, en el nodo 1: A, B y C (la A, libre, para el primero)", l[0] == 'A' && l[1] == 'B' && l[2] == 'C');
    p8_letras_lazo(-1, -1, 0, 0, l);
    chk_cond(q, "solo B, en el nodo 0: B, A y C", l[0] == 'B' && l[1] == 'A' && l[2] == 'C');
    p8_letras_lazo(0, 1, 0, 1, l);
    chk_cond(q, "A y B en el nodo 2: C, D y A (la B no la lleva ninguno)", l[0] == 'C' && l[1] == 'D' && l[2] == 'A');
    p8_letras_lazo(3, 3, 1, 0, l);
    chk_cond(q, "A en un punto suelto y B en el nodo 1: C, B y D (la A no es de otro nodo)",
             l[0] == 'C' && l[1] == 'B' && l[2] == 'D');
    lazo(&c, &r, 12.0, 4.0, 12.0);
    c.term_x[0] = 0; c.term_y[0] = 0; c.term_x[1] = 0; c.term_y[1] = 1;
    red_extraer(&c, &r);
    chk_cond(q, "red_nodo_puesto: el inverso", red_nodo_puesto(&r, 0) == 0 && red_nodo_puesto(&r, 1) == 2 &&
             red_nodo_puesto(&r, 2) == 1 && red_nodo_puesto(&r, 3) == -1);

    /* La pantalla de 664: A en el + y B en el - de E1, la tierra entre R1 y
     * R2. V(AB) = VA - VB; en Potenciales, una sola «B» de cada color y la
     * verde, junto a la amarilla (el mismo nodo) */
    ui_init();
    TECLAS(T_ENTER, T_ENTER, T_ENTER);
    TECLAS(T_LETRA('E'), T_ARRIBA, T_DIG(1), T_DIG(2), T_ABAJO, T_INCOG, T_ENTER);
    TECLAS(T_LETRA('T'));
    TECLAS(T_LETRA('R'), T_DER, T_DIG(4), T_ABAJO, T_INCOG, T_ENTER);
    TECLAS(T_LETRA('G'));
    TECLAS(T_LETRA('R'), T_ABAJO, T_DIG(1), T_DIG(2), T_ABAJO, T_INCOG, T_ENTER);
    TECLAS(T_LETRA('W'), T_IZQ, T_ESC);
    TECLAS(T_LETRA('T'));
    /* En el editor, la «B» amarilla (en el - de E1) no pisa los rótulos de
     * E1 («12V», «I=?»: arriba a la izquierda salía «I=B»): busca sitio */
    {
        const EsqCaja *cj;
        int nc, i, pisa = 0;
        ui_dibuja();
        na = mr_cuenta_glifo(0, 16, SCR_W, 188, 'B', C_CURSOR, &xa, &ya);
        nc = esq_cajas(&cj);
        for (i = 0; i < nc && na == 1; i++)
            if (xa - 1 < cj[i].x + cj[i].w && cj[i].x < xa + FONT_W && ya < cj[i].y + cj[i].h && cj[i].y < ya + FONT_H) pisa++;
        printf("    editor: la «B» amarilla en (%d,%d) (%d); pisa %d de %d rótulos\n", xa, ya, na, pisa, nc);
        chk_cond(q, "editor: la letra del terminal B se ve entera y no pisa ningún rótulo", na == 1 && nc >= 3 && pisa == 0);
    }
    TECLAS(T_IGUAL);
    printf("    V(AB) = VA - VB = 12 V: %d\n", ve("V(AB) = VA - VB = 12 V"));
    chk_cond(q, "el resultado: «V(AB) = VA - VB = 12 V»", ui_pantalla() == P_RESULTADO && ve("V(AB) = VA - VB = 12 V"));
    for (k = 0; k < CAPA_N && ui_capa() != CAPA_POTENCIALES; k++) TECLAS(T_DER);
    ui_dibuja();
    ng = mr_cuenta_glifo(0, 16, SCR_W, 158, 'B', C_TENSION, &x, &y);
    na = mr_cuenta_glifo(0, 16, SCR_W, 158, 'B', C_CURSOR, &xa, &ya);
    {
        Marco m;
        int tx, ty;
        esq_marco_ajustado(ui_circuito(), 0, 16, SCR_W, 158 - 16, &m);
        esq_pt(&m, ui_circuito()->term_x[1], ui_circuito()->term_y[1], &tx, &ty);
        printf("    Potenciales: «B» verde %d (en %d,%d), amarilla %d (en %d,%d); el terminal B en (%d,%d)\n", ng, x, y, na,
               xa, ya, tx, ty);
        chk_cond(q, "Potenciales: una «B» de cada color, las dos junto al punto del terminal B",
                 ui_capa() == CAPA_POTENCIALES && ng == 1 && na == 1 && abs(x - tx) <= 20 && abs(y - ty) <= 20 &&
                 abs(xa - tx) <= 20 && abs(ya - ty) <= 20);
    }
    r4_vuelca("p8_terminales_potenciales");
    for (n = 0; n < 4 && ui_pantalla() != P_EDITOR; n++) TECLAS(T_ESC);

    /* Los pasos de Kirchhoff (dos fuentes) con A en el nodo 2 y B en el 0:
     * los nodos, en el orden de sus letras */
    {
        static Solucion s;
        static Pasos ps;
        int p, prev = 0;
        chk_cond(q, "el circuito de dos fuentes", mr_dos_fuentes());
        c = *ui_circuito();
        red_extraer(&c, &r);
        for (p = 0; p < REJ_PUNTOS && r.nn == 3; p++) {
            if (r.punto[p] == 2 && c.term_x[0] < 0) { c.term_x[0] = (signed char)(p % REJ_COLS); c.term_y[0] = (signed char)(p / REJ_COLS); }
            if (r.punto[p] == 0 && c.term_x[1] < 0) { c.term_x[1] = (signed char)(p % REJ_COLS); c.term_y[1] = (signed char)(p / REJ_COLS); }
        }
        red_extraer(&c, &r);
        resolver(&c, &r, &s);
        pasos_init(&ps);
        ok = proced_generar(&c, &r, &s, 0, &ps, 0) == PROC_KIRCHHOFF && ps.n > 0;
        printf("    Kirchhoff con terminales: letras %c %c %c; «%s»\n", red_letra(&r, 0), red_letra(&r, 1), red_letra(&r, 2),
               ok ? ps.items[0].detalle : "");
        chk_cond(q, "Kirchhoff: A en el nodo 2, B en el 0 y C en el 1",
                 red_letra(&r, 2) == 'A' && red_letra(&r, 0) == 'B' && red_letra(&r, 1) == 'C');
        chk_cond(q, "Kirchhoff: «Nodos: A, B, C», en orden", ok && strstr(ps.items[0].detalle, "Nodos: A, B, C") != 0);
        for (p = 0; ok && p < ps.n; p++)
            if (strncmp(ps.items[p].titulo, "Nodo ", 5) == 0) {
                if (ps.items[p].titulo[5] <= prev || ps.items[p].titulo[5] != red_letra(&r, ps.items[p].nodo)) ok = 0;
                prev = ps.items[p].titulo[5];
            }
        chk_cond(q, "Kirchhoff: los pasos «Nodo X» en orden alfabético y con la letra de su nodo", ok && prev != 0);
        for (n = 0; n < 4 && ui_pantalla() != P_EDITOR; n++) TECLAS(T_ESC);
    }
}

/* ---- P3. Los rótulos de la capa Potenciales, con aire ---- */

/* Dos fuentes enfrentadas (mr_dos_fuentes): cada rótulo de nodo («C 12V»)
 * tiene un píxel libre alrededor (el «+» de E2 no se le pega: «C 12V +») */
static void p8_potenciales_aire(void)
{
    static Red r;
    const char *q = "Potenciales: rótulos con aire";
    const Valores *v;
    char u[24], t[32];
    int n, j, x, y, lx, ly, w, pegados = 0, ok;
    chk_cond(q, "el circuito de dos fuentes", mr_dos_fuentes());
    TECLAS(T_IGUAL);
    for (j = 0; j < CAPA_N && ui_capa() != CAPA_POTENCIALES; j++) TECLAS(T_DER);
    red_extraer(ui_circuito(), &r);
    v = ui_valores();
    ui_dibuja();
    ok = ui_pantalla() == P_RESULTADO && ui_capa() == CAPA_POTENCIALES && r.nn == 3;
    for (n = 0; ok && n < r.nn; n++) {
        int anillo = 0;
        if (mr_cuenta_glifo(0, 16, SCR_W, 158, red_letra(&r, n), C_TENSION, &lx, &ly) != 1) { ok = 0; break; }
        num_corto(u, sizeof u, v->phi[n]);
        sprintf(t, "%sV", u);
        w = FONT_W + 3 + gfx_text_w(t, 1);
        for (y = ly - 1; y <= ly + FONT_H; y++)
            for (x = lx - 1; x <= lx + w; x++)
                if ((y == ly - 1 || y == ly + FONT_H || x == lx - 1 || x == lx + w) && es_px(x, y) != C_FONDO) anillo++;
        printf("    «%c %s» en (%d,%d): %d píxeles pegados alrededor\n", red_letra(&r, n), t, lx, ly, anillo);
        if (anillo) pegados++;
    }
    chk_cond(q, "dos fuentes: los tres rótulos se leen y ninguno se pega a nada", ok && pegados == 0);
    r4_vuelca("p8_potenciales_aire");
    TECLAS(T_ESC);
}

/* ---- P4. Los rótulos no salen por encima del recorte (la barra) ---- */

/* Una E controlada horizontal en la fila 0 del editor (m_culpable_cc): su
 * «2·V(?)» iba 4 px más arriba que el recorte (y = 12, bajo la barra de
 * título, que empieza el dibujo en 16) y se veía cortado. Sola, va debajo;
 * con segunda línea, arrimada al borde. Y un BJT boca abajo en la fila 1,
 * con sus dos líneas encima de la base: tampoco salen */
static void p8_rotulo_arriba(void)
{
    static Circuito c;
    const char *q = "rótulos bajo la barra de título";
    const EsqCaja *cj;
    Marco m;
    Vista v;
    int caso, k, nc, i, ymin;
    for (caso = 0; caso < 3; caso++) {
        circ_init(&c, "Arriba");
        if (caso < 2) {
            k = circ_elemento(&c, EL_EDEP, 2, 0, DIR_DER);
            r3d_dato(&c.el[k].val, 2.0);
            if (caso == 1) r3d_dato(&c.el[k].V, 3.0);          /* con segunda línea («V=3V») */
        } else {
            k = circ_elemento(&c, EL_BJT, 4, 1, DIR_ARR);
        }
        esq_marco_editor(&m);
        esq_vista(&v);
        v.capa = CAPA_VALORES;
        gfx_clip_reset(); gfx_clear(C_FONDO);
        gfx_clip(0, 16, SCR_W, 188 - 16);                    /* como el editor: debajo de la barra */
        esq_dibuja(&c, &m, &v);
        gfx_clip_reset();
        nc = esq_cajas(&cj);
        for (i = 0, ymin = SCR_H; i < nc; i++) if (cj[i].y < ymin) ymin = cj[i].y;
        printf("    caso %d: %d rótulos, el más alto en y = %d\n", caso, nc, ymin);
        chk_cond(q, caso == 0 ? "E controlada en la fila 0, una línea: su rótulo, entero bajo la barra" :
                    caso == 1 ? "E controlada en la fila 0, dos líneas: las dos, bajo la barra" :
                                "BJT boca arriba en la fila 1: sus rótulos, bajo la barra",
                 k >= 0 && nc >= 1 && ymin >= 16);
    }
}

/* ---- P7. Un campo con una cuenta larga: «« y caracteres enteros ---- */

/* En la ficha, la cuenta de 31 de f_cuenta_larga: se ve su final (16
 * caracteres de 124 hacia atrás) y, delante, «« en C_SUAVE; entre el
 * borde de la caja (x = 24) y el «« (x = 28), nada: antes asomaba media
 * «+» pegada al borde */
static void p8_campo_largo(void)
{
    const char *q = "campo con una cuenta larga";
    int i, x, y, suelto = 0, y0 = 146 + 5;
    r2e_nuevo();
    TECLAS(T_LETRA('R'), T_DER);
    for (i = 0; i < 16; i++) TECLAS(T_DIG(1), T_MAS);
    ui_dibuja();
    for (y = y0; y < y0 + FONT_H; y++)
        for (x = 25; x < 28; x++) if (es_px(x, y) != C_PANEL) suelto++;
    printf("    la cuenta de 31: «« en (28,%d) %d; %d píxeles sueltos entre el borde y el ««\n", y0,
           mr_glifo_en(28, y0, (char)0xAB, C_SUAVE), suelto);
    chk_cond(q, "se ve su final («+1+1_») tras un «« y sin medio glifo junto al borde",
             ui_pantalla() == P_ELEMENTO && ve("+1+1_") && mr_glifo_en(28, y0, (char)0xAB, C_SUAVE) && suelto == 0);
    r4_vuelca("p8_campo_largo");
    TECLAS(T_ESC, T_ESC, T_ESC);
    /* corta, como siempre: arrimada a la derecha y sin «« */
    r2e_nuevo();
    TECLAS(T_LETRA('R'), T_DER, T_DIG(4), T_DIG(7));
    ui_dibuja();
    chk_cond(q, "una corta, como siempre: «47_» arrimado a 124 y sin ««",
             ve("47_") && !mr_glifo_en(28, y0, (char)0xAB, C_SUAVE) && mr_glifo_en(124 - 3 * FONT_W, y0, '4', C_CURSOR));
    TECLAS(T_ESC, T_ESC, T_ESC);
}

/* ---- P8. Los títulos largos (Circuito.nombre[40] y la sección 3) ---- */

/* El bloque global de un archivo CESP: su longitud (el byte tras la
 * cabecera: CESP, versión, columnas, filas, nombre[24] y los segmentos) */
static int p8_largo_global(int hueco)
{
    char ruta[300];
    FILE *f;
    int n;
    sprintf(ruta, "%s/ces_%02d.tns", carpeta_mis, hueco + 1);
    f = fopen(ruta, "rb");
    if (!f) return -1;
    fseek(f, 4 + 1 + 2 + 24 + REJ_FILAS * (REJ_COLS - 1) + (REJ_FILAS - 1) * REJ_COLS, SEEK_SET);
    n = fgetc(f);
    fclose(f);
    return n;
}

static void p8_titulos_largos(void)
{
    static Circuito c, d, e1;
    const char *q = "títulos largos";
    const char *largo = "Diodo en directa: su punto de operaci" S_oo "n";
    int i, mejor = -1, lmax = 0, x, y, xt = -1, xd = SCR_W, g2, ok;
    char ruta[300];
    /* el título más largo de la biblioteca, entero */
    for (i = 0; i < N_MODELOS; i++) if ((int)strlen(MODELOS[i].titulo) > lmax) { lmax = (int)strlen(MODELOS[i].titulo); mejor = i; }
    modelo_cargar(mejor, &c);
    printf("    el título más largo: «%s» (%d), en el circuito «%s»\n", MODELOS[mejor].titulo, lmax, c.nombre);
    chk_cond(q, "modelo_cargar: el título entero (39 como mucho, antes 23)",
             lmax > 23 && lmax <= 39 && strcmp(c.nombre, MODELOS[mejor].titulo) == 0);
    /* el archivo: con la sección 3 se guarda entero; hasta 23, sin ella */
    limpia_mis();
    ok = arch_guardar(0, &c) && arch_cargar(0, &d) && strcmp(d.nombre, c.nombre) == 0;
    g2 = p8_largo_global(0);
    printf("    guardado y leído: «%s»; el bloque global mide %d\n", d.nombre, g2);
    chk_cond(q, "el archivo guarda los 39 (la sección 3: el bloque global de 25 + 28 + 16)", ok && g2 == 69);
    strcpy(c.nombre, "Nombre de 24 caracteres.");               /* justo 24: el byte 24 va en la cabecera */
    ok = arch_guardar(0, &c) && arch_cargar(0, &d) && strcmp(d.nombre, c.nombre) == 0;
    chk_cond(q, "24 caracteres: también enteros", ok && p8_largo_global(0) == 69);
    strcpy(c.nombre, "Nombre de 23 caracteres");
    lazo(&e1, 0, 12.0, 4.0, 8.0);
    strcpy(e1.nombre, c.nombre);
    ok = arch_guardar(0, &e1) && arch_cargar(0, &d) && strcmp(d.nombre, e1.nombre) == 0;
    chk_cond(q, "23 caracteres y sin sondas: el archivo de siempre (bloque global de 25, sin secciones)",
             ok && p8_largo_global(0) == 25);
    /* un lector de antes (el de la Etapa 1) lee el de nombre largo: se
     * queda con 23 y el circuito, igual */
    strcpy(e1.nombre, largo);
    sprintf(ruta, "%s/ces_01.tns", carpeta_mis);
    ok = arch_guardar(0, &e1) && lector_e1(ruta, &d);
    printf("    el lector de la Etapa 1: %d, «%s», %d elementos\n", ok, ok ? d.nombre : "", ok ? d.nel : -1);
    chk_cond(q, "un lector anterior salta la sección 3: el nombre en 23 y el circuito entero",
             ok && strncmp(d.nombre, largo, 23) == 0 && strlen(d.nombre) == 23 && d.nel == e1.nel);
    /* la lista de Mis problemas lo enseña entero */
    limpia_mis();
    arch_guardar(0, &e1);
    ui_init();
    TECLAS(T_ENTER, T_ENTER);                                       /* menú > Taller */
    TECLAS(T_ABAJO, T_ABAJO, T_ENTER);                              /* Mis problemas */
    chk_cond(q, "Mis problemas: el nombre entero en la lista", ui_pantalla() == P_MIS && ve(largo));
    r4_vuelca("p8_mis_largo");
    limpia_mis();
    /* en la barra del editor, con «[menu] opciones», no cabe: se corta en
     * una palabra con «...» y no llega a lo de la derecha */
    lazo(&e1, 0, 12.0, 4.0, 8.0);
    strcpy(e1.nombre, largo);
    r3u_abre(&e1);
    ui_dibuja();
    for (y = 2; y < 14; y++)
        for (x = 0; x < SCR_W; x++) {
            if (es_px(x, y) == C_TEXTO && x > xt) xt = x;
            if (es_px(x, y) == C_CURSOR && x < xd) xd = x;
        }
    printf("    barra del editor: el título acaba en x = %d y «[menu] opciones» empieza en %d\n", xt, xd);
    chk_cond(q, "barra: el título, cortado con «...», no toca «[menu] opciones»",
             ve("[menu] opciones") && ve("Diodo en directa: su punto de...") && xt < xd - 3);
    for (i = 0; i < 6 && ui_pantalla() != P_MENU; i++) TECLAS(T_ESC);
    {
        /* la foto del estado (la independencia del dibujo) sigue cabiendo
         * entera en el búfer de 32768 de test_entrada.inc: con el nombre de
         * 40 y el aviso de 160 no se deja nada fuera */
        static unsigned char foto[65536];
        int nf = ui_foto(foto, (int)sizeof foto);
        printf("    la foto del estado: %d bytes\n", nf);
        chk_cond(q, "la foto del estado cabe en los 32768 del arnés", nf > 0 && nf <= 32768);
    }
}

/* ---- P9. Textos: tablas, el panel vacío y ω ---- */
static void p8_textos(void)
{
    static Circuito c;
    const char *q = "textos de las pantallas";
    int i, k;
    char t[64];

    /* la tabla del punto de operación (el emisor común): «Valor/reg.» con
     * aire hasta la V y, por debajo de 0,1, con prefijo (P de E1: -32,028
     * mW y no -0,032028 W al borde; R3: 13,251 mW junto a R4: 2,8877 mW) */
    i = mr_modelo("Emisor com" S_uu "n: el punto Q");
    chk_cond(q, "el modelo del emisor común", i >= 0);
    if (i >= 0) {
        int x, xmax = -1;
        modelo_cargar(i, &c);
        r3u_abre(&c);
        TECLAS(T_IGUAL);
        r4_espera();
        TECLAS(T_TAB);
        ui_dibuja();
        for (x = 40; x < 114; x++)                        /* la cabecera: hasta dónde llega el «Valor/reg.» */
            for (k = 16 + 4; k < 16 + 16; k++) if (es_px(x, k) == C_SUAVE && x > xmax) xmax = x;   /* bajo la barra de 16 */
        printf("    tabla: «Valor/reg.» %d (acaba en x = %d), -32,028 mW %d, 13,251 mW %d, 0,013251 W %d\n", ve("Valor/reg."),
               xmax, ve("\n-32,028 mW\n"), ve("\n13,251 mW\n"), ve("0,013251 W"));
        chk_cond(q, "tabla: «Valor/reg.» con 3 px o más de aire hasta la V", ui_pantalla() == P_TABLA && ve("Valor/reg.") &&
                 xmax >= 40 && xmax < 114 - 3);
        chk_cond(q, "tabla: por debajo de 0,1, con prefijo («-32,028 mW», «13,251 mW»)",
                 ve("\n-32,028 mW\n") && ve("\n13,251 mW\n") && !ve("0,013251 W") && !ve("0,032028 W"));
        for (k = 0; k < 4 && ui_pantalla() != P_EDITOR; k++) TECLAS(T_ESC);
    }

    /* el resultado de CC sin [?!] ni una fuente sola: el balance, no un
     * panel vacío */
    chk_cond(q, "el circuito de dos fuentes", mr_dos_fuentes());
    TECLAS(T_IGUAL);
    chk_cond(q, "resultado sin [?!] de dos fuentes: el balance en el panel",
             ui_pantalla() == P_RESULTADO && ve("Entregan 36 W " S_CDOT " absorben 36 W " S_CHECK));
    r4_vuelca("p8_resultado_balance");
    TECLAS(T_ESC);

    /* ω desde 1E6, con exponente; 9,99996E6 redondea a 1E7 (no «10E6») */
    i = mr_modelo("RLC serie en fasores");
    if (i >= 0) {
        modelo_cargar(i, &c);
        r3d_dato(&c.frec, 9.99996e6 / (2.0 * PI_T));
        r3u_abre(&c);
        TECLAS(T_IGUAL, T_TAB, T_IZQ);
        sprintf(t, "En el tiempo (" S_OMEGA " = 1E7 rad/s)");
        chk_cond(q, S_OMEGA " = 9,99996E6: «1E7 rad/s» y «cos(1E7t»", ui_pantalla() == P_CA_TABLA && ve(t) && ve("cos(1E7t"));
        for (k = 0; k < 4 && ui_pantalla() != P_EDITOR; k++) TECLAS(T_ESC);
    }
    /* (el punto 5 de la Ayuda y el aviso de [G] en un punto suelto, en
     * [73] C6) */
    for (k = 0; k < 4 && ui_pantalla() != P_MENU; k++) TECLAS(T_ESC);
}

#if ESPECIAL
#include "test_r8_pantallas_esp.inc"    /* la versión especial (no se publica) */
#else
#define ESP_P8_P11
#endif

/* ---- P6. El recuadro del elegido encierra su símbolo y sus rótulos ---- */

/* c, con un solo elemento, pintado en el editor sin elegir y elegido: el
 * recuadro es lo que cambia; todo lo que pinta el elemento (el símbolo, su
 * + o su forma, sus rótulos) tiene que caer dentro, sin tocar el borde.
 * Devuelve los píxeles que caen fuera o en el borde (-1 sin recuadro) */
static int p8_recuadro_fuera(const Circuito *c)
{
    static u16 sin[SCR_W * SCR_H];
    Marco m;
    Vista v;
    int x, y, x0 = SCR_W, y0 = SCR_H, x1 = -1, y1 = -1, fuera = 0;
    esq_marco_editor(&m);
    esq_vista(&v);
    v.capa = CAPA_VALORES;
    gfx_clip_reset(); gfx_clear(C_FONDO);
    esq_dibuja(c, &m, &v);
    memcpy(sin, gfx_buffer(), sizeof sin);
    v.sel = 0;
    gfx_clear(C_FONDO);
    esq_dibuja(c, &m, &v);
    for (y = 0; y < SCR_H; y++)
        for (x = 0; x < SCR_W; x++)
            if (es_px(x, y) != sin[y * SCR_W + x]) {
                if (x < x0) x0 = x;
                if (x > x1) x1 = x;
                if (y < y0) y0 = y;
                if (y > y1) y1 = y;
            }
    if (x1 < 0) return -1;
    for (y = 0; y < SCR_H; y++)
        for (x = 0; x < SCR_W; x++)
            if (sin[y * SCR_W + x] != C_FONDO && !(x0 < x && x < x1 && y0 < y && y < y1)) fuera++;
    return fuera;
}

static void p8_recuadro(void)
{
    static Circuito c;
    const char *q = "recuadro del elegido";
    static const char *const NOM[7] = { "R de 0,047 ohm", "E vertical con r", "C con V0 = 5 V", "J senoidal",
                                         "LED", "vatímetro", "BJT PNP" };
    int caso, f, mal = 0;
    for (caso = 0; caso < 7; caso++) {
        int k;
        circ_init(&c, "Recuadro");
        switch (caso) {
        case 0: k = circ_elemento(&c, EL_RES, 3, 3, DIR_DER); r3d_dato(&c.el[k].val, 0.047); break;
        case 1: k = circ_elemento(&c, EL_FUENTE, 4, 4, DIR_ARR); r3d_dato(&c.el[k].val, 12.0); r3d_dato(&c.el[k].aux, 0.5); break;
        case 2: k = circ_elemento(&c, EL_COND, 3, 3, DIR_DER); r3d_dato(&c.el[k].val, 1e-6); r3d_dato(&c.el[k].par[Q_V0 - Q_P1], 5.0); break;
        case 3: k = circ_elemento(&c, EL_CORR, 4, 4, DIR_ARR); r3d_dato(&c.el[k].val, 1.0); c.el[k].variante = ONDA_SENO; break;
        case 4: k = circ_elemento(&c, EL_LED, 4, 2, DIR_ABA); c.el[k].variante = LED_ROJO; break;
        case 5: k = circ_elemento(&c, EL_VATI, 3, 3, DIR_DER); break;
        default: k = circ_elemento(&c, EL_BJT, 4, 3, DIR_DER); c.el[k].variante = BJT_PNP; break;
        }
        f = k == 0 ? p8_recuadro_fuera(&c) : -2;
        printf("    %s: %d píxeles suyos fuera del recuadro o en su borde\n", NOM[caso], f);
        if (f != 0) mal++;
    }
    chk_cond(q, "cada tipo (R, E con r, C con V0, J con forma, LED, W, BJT): el recuadro encierra su símbolo y sus rótulos",
             mal == 0);
}

/* ---- P1b. Las letras con terminales en la tabla de CA y en el osciloscopio ---- */
static void p8_letras_terminales_2(void)
{
    static Circuito c;
    static Red r;
    const char *q = "letras de nodo con terminales (CA y osciloscopio)";
    int i, p, n, x, y, xb, yb, xc, yc, ok;
    char t[32];

    /* La tabla de CA, página de los nodos: en el orden de sus letras (A y
     * B, los de los terminales, primero) */
    i = mr_modelo("RLC serie en fasores");
    chk_cond(q, "el modelo «RLC serie en fasores»", i >= 0);
    if (i >= 0) {
        modelo_cargar(i, &c);
        red_extraer(&c, &r);
        for (p = REJ_PUNTOS - 1; p >= 0 && r.punto[p] < 0; p--) {}           /* A, en el último nodo */
        c.term_x[0] = (signed char)(p % REJ_COLS); c.term_y[0] = (signed char)(p / REJ_COLS);
        red_extraer(&c, &r);
        n = r.punto[p];
        r3u_abre(&c);
        TECLAS(T_IGUAL, T_TAB);
        for (i = 0; i < 6 && !ve("4/5"); i++) TECLAS(T_DER);
        ui_dibuja();
        ok = mr_cuenta_glifo(0, 16, 12, 220, 'A', C_TENSION, &x, &y) == 1 &&
             mr_cuenta_glifo(158, 16, 170, 220, 'B', C_TENSION, &xb, &yb) == 1 &&
             mr_cuenta_glifo(0, 16, 12, 220, 'C', C_TENSION, &xc, &yc) == 1;
        printf("    CA, los nodos: A en el nodo %d (de %d); A (%d,%d), B (%d,%d), C (%d,%d)\n", n, r.nn, x, y, xb, yb, xc, yc);
        chk_cond(q, "tabla de CA: A (el terminal, en el último nodo) primero; luego B y C, en orden",
                 ui_pantalla() == P_CA_TABLA && n == r.nn - 1 && red_letra(&r, n) == 'A' && ok && yb == y && yc == y + 14);
        for (i = 0; i < 4 && ui_pantalla() != P_EDITOR; i++) TECLAS(T_ESC);
    }

    /* El osciloscopio: la sonda de nodo se llama con la letra del esquema
     * (el RC de siempre, con A en el nodo de R1 y C1 y la sonda ahí) */
    circ_init(&c, "RC");
    i = circ_elemento(&c, EL_FUENTE, 0, 1, DIR_ARR); r3d_dato(&c.el[i].val, 10.0);
    i = circ_elemento(&c, EL_RES, 0, 0, DIR_DER);    r3d_dato(&c.el[i].val, 1e3);
    i = circ_elemento(&c, EL_COND, 1, 0, DIR_ABA);   r3d_dato(&c.el[i].val, 1e-6);
    circ_cable(&c, 1, 1, DIR_IZQ);
    c.term_x[0] = 1; c.term_y[0] = 0;                                  /* A: el nodo 1 */
    c.sonda[0].tipo = SONDA_NODO; c.sonda[0].x = 1; c.sonda[0].y = 0; c.sonda[0].el = -1; c.sonda[0].sg = 0;
    red_extraer(&c, &r);
    r3u_abre(&c);
    TECLAS(T_IGUAL);
    r4_espera();
    sprintf(t, "nodo %c", red_letra(&r, r.punto[1]));
    printf("    osciloscopio: el canal 1 se llama «%s» (el nodo %d se llama %c)\n", sim_res()->canal[0].nombre, r.punto[1],
           red_letra(&r, r.punto[1]));
    chk_cond(q, "osciloscopio: la sonda en el nodo de A se llama «nodo A»", ui_pantalla() == P_OSC &&
             strcmp(sim_res()->canal[0].nombre, t) == 0 && strcmp(t, "nodo A") == 0 && ve("1 nodo A"));
    for (i = 0; i < 4 && ui_pantalla() != P_EDITOR; i++) TECLAS(T_ESC);
}

/* ---- P2. El osciloscopio: los cursores no cruzan los rótulos ---- */

/* ¿Se ve entero el texto t que se pintó en (x, y) en fg sobre bg? Se copia
 * su caja (sus primeras filas: lo de abajo lo puede tapar un panel), se
 * pinta otra vez solo y se compara: si algo lo cruzó después, cambia. Deja
 * la pantalla como estaba */
static int p8_rotulo_entero(int x, int y, const char *t, u16 fg, u16 bg, int filas)
{
    static u16 copia[SCR_W * FONT_H];
    int w = gfx_text_w(t, 1), i, j, igual = 1;
    if (x < 0 || y < 0 || x + w > SCR_W || y + FONT_H > SCR_H || filas > FONT_H) return 0;
    for (j = 0; j < FONT_H; j++) for (i = 0; i < w; i++) copia[j * SCR_W + i] = es_px(x + i, y + j);
    gfx_text_bg(x, y, t, fg, bg, 1);
    for (j = 0; j < FONT_H; j++)
        for (i = 0; i < w; i++) {
            if (j < filas && es_px(x + i, y + j) != copia[j * SCR_W + i]) igual = 0;
            gfx_buffer()[(y + j) * SCR_W + x + i] = copia[j * SCR_W + i];
        }
    return igual;
}

/* ¿Está la cifra ch de una cajita de cursor con su esquina en (x, y)? Las 9
 * filas de la celda que caen en la caja: el glifo en C_FONDO y el resto no */
static int p8_cifra_en(int x, int y, char ch)
{
    const u8 *g = font_glifo12((unsigned char)ch);
    int gx, gy;
    if (x < 0 || y < 0 || x + FONT_W > SCR_W || y + 9 > SCR_H) return 0;
    for (gy = 0; gy < 9; gy++)
        for (gx = 0; gx < FONT_W - 1; gx++)
            if ((int)((g[gy] >> gx) & 1u) != (es_px(x + gx, y + gy) == C_FONDO)) return 0;
    return 1;
}

static int p8_cuenta_cifra(char ch)
{
    int x, y, n = 0;
    for (y = OSC_Y0 + 8 * OSC_DY - 40; y < OSC_Y0 + 8 * OSC_DY; y++)
        for (x = OSC_X0; x < OSC_X0 + 10 * OSC_DX; x++) n += p8_cifra_en(x, y, ch);
    return n;
}

static void p8_osc_cursores(void)
{
    static Circuito c;
    const char *q = "osciloscopio: cursores y rótulos";
    const char *pm = "T1-T2: pocas muestras [=]";
    char t[24];
    int i = mr_modelo("Media onda con filtro: el rizado"), x1, x2, xr, xt, dentro_r, dentro_t, n1, n2;
    const Osc *o;
    double td;
    chk_cond(q, "el modelo del filtro", i >= 0);
    if (i < 0) return;
    modelo_cargar(i, &c);
    r3u_abre(&c);
    TECLAS(T_IGUAL);
    r4_espera();
    TECLAS(T_LETRA('U'));
    TECLAS(T_DIG(9), T_DIG(9), T_LETRA('M'), T_ENTER, T_LETRA('M'));   /* T1 a 99 ms; T2, al final (100 ms) */
    o = ui_osc();
    chk_cond(q, "T1 a 99 ms y T2 a 100 ms: «T1-T2: pocas muestras [=]»", ui_pantalla() == P_OSC && ve(pm));
    ui_dibuja();
    td = ESCALA[o->itdiv];
    x1 = OSC_X0 + (int)floor((o->tc[0] - o->tizq) / td * OSC_DX + 0.5);
    x2 = OSC_X0 + (int)floor((o->tc[1] - o->tizq) / td * OSC_DX + 0.5);
    xr = OSC_X0 + 10 * OSC_DX - 2 - gfx_text_w(pm, 1);
    num_pref(t, sizeof t, o->tizq + 10.0 * td, 3, "s");
    xt = OSC_X0 + 10 * OSC_DX - 2 - gfx_text_w(t, 1);
    dentro_r = x1 >= xr && x1 < xr + gfx_text_w(pm, 1);
    dentro_t = x1 >= xt && x1 < xt + gfx_text_w(t, 1);
    printf("    T1 en x = %d, T2 en x = %d; el rótulo de %d a %d, «%s» de %d a %d\n", x1, x2, xr, xr + gfx_text_w(pm, 1),
           t, xt, xt + gfx_text_w(t, 1));
    chk_cond(q, "la línea de T1 cruza el rótulo y el tiempo del borde (la prueba vale)", dentro_r && dentro_t);
    chk_cond(q, "«T1-T2: pocas muestras [=]», entero: encima de las líneas", p8_rotulo_entero(xr, OSC_Y0 + 2, pm, C_TEXTO, C_FONDO, FONT_H));
    chk_cond(q, "el tiempo del borde, entero: encima de las líneas", p8_rotulo_entero(xt, 177, t, C_SUAVE, C_FONDO, OSC_PANEL_Y - 177));
    n1 = p8_cuenta_cifra('1');
    n2 = p8_cuenta_cifra('2');
    printf("    cursores a %d px: la cajita «1» se ve %d veces y la «2», %d\n", abs(x2 - x1), n1, n2);
    chk_cond(q, "cursores juntos: las cajitas «1» y «2» se leen las dos (no se montan)", abs(x2 - x1) < 10 && n1 == 1 && n2 == 1);
    r4_vuelca("p8_osc_cursores_juntos");
    /* separados, cada cajita en su sitio de siempre */
    repite(T_IZQ, 10);
    ui_dibuja();
    n1 = p8_cuenta_cifra('1');
    n2 = p8_cuenta_cifra('2');
    x1 = OSC_X0 + (int)floor((o->tc[0] - o->tizq) / td * OSC_DX + 0.5);
    chk_cond(q, "cursores separados: las dos cajitas, a la misma altura",
             n1 == 1 && n2 == 1 && p8_cifra_en(x1 - 2, OSC_Y0 + 8 * OSC_DY - 21, '1') && p8_cifra_en(x2 - 2, OSC_Y0 + 8 * OSC_DY - 21, '2'));
    TECLAS(T_ESC);
}

/* ---- P5. Rótulos de elementos que se pisan ---- */

/* Un circuito pintado en la capa Valores (sin resolver) con el marco m, entre
 * y0 e y1 (el recorte): cuenta los pares de rótulos de elementos distintos
 * que se pisan y los rótulos que tapan o tocan el símbolo de otro elemento
 * (sus píxeles en la caja o en la columna de cada lado, con el circuito
 * pintado sin cables ni rótulos y ese elemento en C_TENUE). Los cables no
 * cuentan: el rótulo de una vertical va a la altura del cable de su punta.
 * Suma a n[0] y n[1]; con nombre, dice cuáles */
static void p8_rotulos_de(const Circuito *c0, const Marco *m, int y0, int y1, const char *nombre, int n[2])
{
    static Circuito c;
    static EsqCaja cj[2 * MAX_EL];
    const EsqCaja *pc;
    Vista v;
    int nc, i, j, x, y, antes = n[0] + n[1];
    c = *c0;
    esq_vista(&v);
    v.capa = CAPA_VALORES;
    gfx_clip_reset(); gfx_clear(C_FONDO);
    gfx_clip(0, y0, SCR_W, y1 - y0);
    esq_dibuja(&c, m, &v);
    gfx_clip_reset();
    nc = esq_cajas(&pc);
    memcpy(cj, pc, (size_t)nc * sizeof cj[0]);
    for (y = 0; y < REJ_FILAS; y++)                       /* sin cables */
        for (x = 0; x < REJ_COLS; x++) {
            if (x < REJ_COLS - 1 && c.h[y][x] == SEG_CABLE) c.h[y][x] = 0;
            if (y < REJ_FILAS - 1 && c.v[y][x] == SEG_CABLE) c.v[y][x] = 0;
        }
    for (i = 0; i < nc; i++) {
        int tapa = 0;
        esq_vista(&v);
        v.capa = CAPA_POTENCIALES;                        /* sin rótulos */
        v.atenuar = 1; v.resalta = 1u << cj[i].k;         /* los demás, en C_TENUE */
        gfx_clip_reset(); gfx_clear(C_FONDO);
        gfx_clip(0, y0, SCR_W, y1 - y0);
        esq_dibuja(&c, m, &v);
        gfx_clip_reset();
        for (y = cj[i].y; y < cj[i].y + cj[i].h; y++)
            for (x = cj[i].x - 1; x <= cj[i].x + cj[i].w; x++)
                if (x >= 0 && x < SCR_W && y >= y0 && y < y1 && es_px(x, y) == C_TENUE) tapa++;
        if (tapa > 0) {
            n[1]++;
            if (nombre) printf("    %s: el rótulo de %s (%d,%d %dx%d) tapa o toca %d píxeles de otro símbolo\n", nombre,
                               c.el[cj[i].k].etiq, cj[i].x, cj[i].y, cj[i].w, cj[i].h, tapa);
        }
        for (j = i + 1; j < nc; j++)
            if (cj[i].k != cj[j].k && cj[i].x < cj[j].x + cj[j].w && cj[j].x < cj[i].x + cj[i].w &&
                cj[i].y < cj[j].y + cj[j].h && cj[j].y < cj[i].y + cj[i].h) {
                n[0]++;
                if (nombre) printf("    %s: los rótulos de %s y %s se pisan\n", nombre, c.el[cj[i].k].etiq, c.el[cj[j].k].etiq);
            }
    }
    if (n[0] + n[1] > antes && getenv("HAKARU_P8_VOLCADO")) {  /* para mirarlos: out/p8s_*.bmp */
        static int nv = 0;
        char ruta[256];
        esq_vista(&v);
        gfx_clear(C_FONDO);
        gfx_clip(0, y0, SCR_W, y1 - y0);
        esq_dibuja(c0, m, &v);
        gfx_clip_reset();
        sprintf(ruta, "%s/p8s_%02d.bmp", dir_out, nv++);
        gfx_dump_bmp(ruta);
    }
}

/* Toda la biblioteca (modelos y ejemplos), en el editor y en el resultado */
static void p8_rotulos_biblioteca(void)
{
    static Circuito c;
    const char *q = "rótulos que se pisan en la biblioteca";
    int lib, i, n_ed[2] = { 0, 0 }, n_rs[2] = { 0, 0 }, nc = 0;
    Marco m;
    for (lib = 0; lib < 2; lib++)
        for (i = 0; i < (lib ? N_MODELOS : N_EJEMPLOS); i++) {
            const char *nombre = lib ? MODELOS[i].titulo : EJEMPLOS[i].titulo;
            if (lib) modelo_cargar(i, &c); else ejemplo_cargar(i, &c);
            nc++;
            esq_marco_editor(&m);
            p8_rotulos_de(&c, &m, 16, 188, nombre, n_ed);
            esq_marco_ajustado(&c, 0, 16, SCR_W, 158 - 16, &m);
            p8_rotulos_de(&c, &m, 16, 158, nombre, n_rs);
        }
    printf("    %d circuitos: en el editor, %d pares de rótulos se pisan y %d tapan o tocan otro símbolo; en el resultado, %d y %d\n",
           nc, n_ed[0], n_ed[1], n_rs[0], n_rs[1]);
    chk_cond(q, "ningún par de rótulos se pisa (editor y resultado)", n_ed[0] == 0 && n_rs[0] == 0);
    chk_cond(q, "ningún rótulo tapa ni toca otro símbolo (editor y resultado)", n_ed[1] == 0 && n_rs[1] == 0);

    /* Los circuitos de las pantallas de las pruebas nuevas que se pisaban
     * (m_culpable_calca, m_z_larga, m_ficha_tab y m_tab_elegida, y las
     * f_cuenta_*): con sitio, ya no */
    {
        int f, n_f[2] = { 0, 0 }, nf = 0;
        for (f = 0; f < 4; f++) {
            if (f == 0) { me_ca_sin_f(&c); c.el[2].val.est = M_BUSCAR; u4_dato(&c.el[2].I, 1e-3); }
            else if (f == 1) me_ca_z(&c);
            else if (f == 2) me_lazo_ancho(&c);
            else if (fl_c_cuentas.nel > 0) c = fl_c_cuentas;   /* el de f_cuenta_larga, si corrió [72] */
            else continue;
            nf++;
            esq_marco_editor(&m);
            p8_rotulos_de(&c, &m, 16, 188, c.nombre, n_f);
            esq_marco_ajustado(&c, 0, 16, SCR_W, 158 - 16, &m);
            p8_rotulos_de(&c, &m, 16, 158, c.nombre, n_f);
        }
        printf("    %d circuitos de las pruebas nuevas: %d pares se pisan y %d rótulos tapan o tocan otro símbolo\n", nf, n_f[0],
               n_f[1]);
        chk_cond(q, "los de las pruebas nuevas (CA sin f, Z larga, el lazo de [tab], las cuentas): nada se pisa",
                 nf >= 3 && n_f[0] == 0 && n_f[1] == 0);
    }
}

/* ----------------------------------------------------------------------- */
static void test_r8_pantallas(void)
{
    printf("[74] Las pantallas de la verificación final\n");
    p8_letras_terminales();
    p8_letras_terminales_2();
    p8_osc_cursores();
    p8_potenciales_aire();
    p8_rotulo_arriba();
    p8_recuadro();
    p8_campo_largo();
    p8_titulos_largos();
    p8_textos();
    ESP_P8_P11
    p8_rotulos_biblioteca();
}
