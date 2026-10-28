/* --------------------------------------------------------------------------
 *  54c. Revisión de la Etapa 3: modelo y memoria
 *
 *  Los hallazgos 29 a 34 de la revisión adversarial de la alterna, mirados
 *  desde el modelo: lo que queda en el Circuito, lo que admite circ_valido,
 *  lo que se guarda y se vuelve a abrir (arch_guardar, arch_cargar, "Mis
 *  problemas") y lo que dice la calculadora de CC (resolver). Los circuitos
 *  se arman con circuito.c y se abren por "Mis problemas", como un problema
 *  guardado; las teclas solo hacen lo que el hallazgo dice que hace el alumno.
 *
 *    R29  el fp del vatímetro fuera de [-1, 1] no entra: el problema guardado
 *         se vuelve a abrir
 *    R30  la paleta tiene PAL_N piezas: lo que se pone es un tipo que existe
 *    R31  sustituir, tapar con cable o borrar la última Z: fuera los ángulos y
 *         el fp del W, y en CC no queda nada escondido
 *    R32  circ_quita_ca vacía la I de un C y la V de una L (filas de CA)
 *    R33  la línea de estado de la calculadora de CA cabe en la pantalla
 *    R34  un módulo infinito (9E300G+j1 en una Z): sin conversiones
 *         indefinidas y no se guarda
 * ----------------------------------------------------------------------- */
#include <fenv.h>
#include <fcntl.h>
#include <unistd.h>

static void r3d_dato(Mag *m, double v) { m->v = v; m->est = M_DATO; }
static void r3d_fasor(Mag *m, double v, double ang) { m->v = v; m->est = M_DATO; m->ang = ang; m->est_ang = M_DATO; }
static void r3d_pide(Mag *m) { m->est = M_BUSCAR; m->est_ang = M_BUSCAR; }

/* Guarda c en el hueco 0 (el único) y lo abre en el editor por Mis problemas */
static int r3d_abre(const Circuito *c)
{
    limpia_mis();
    if (!arch_guardar(0, c)) return 0;
    ui_init();
    TECLAS(T_ENTER, T_ENTER);                       /* menú > Taller */
    TECLAS(T_ABAJO, T_ABAJO, T_ENTER);              /* Mis problemas */
    TECLAS(T_ENTER);                                /* el hueco 0 */
    return ui_pantalla() == P_EDITOR;
}

/* [menu] > Guardar (la opción 3 del editor; en Mis problemas) */
static void r3d_guarda_menu(void)
{
    if (ui_pantalla() == P_ELEMENTO) TECLAS(T_ESC);
    TECLAS(T_MENU, T_ABAJO, T_ABAJO, T_ABAJO, T_ENTER);
}

/* La calculadora de CC sobre el circuito: su estado y su mensaje */
static Red      r3d_r;
static Solucion r3d_s;
static char     r3d_msj[240];
static int r3d_resuelve(const Circuito *c)
{
    red_extraer(c, &r3d_r);
    resolver(c, &r3d_r, &r3d_s);
    sol_mensaje(c, &r3d_r, &r3d_s, r3d_msj, sizeof r3d_msj);
    return r3d_s.estado;
}

/* Nada de lo que solo es de la alterna queda en el circuito: ni ángulos, ni
 * P o fp del W, ni la I de un C o la V de una L, ni [?!] en el valor de C o
 * L, ni la f buscada */
static int r3d_sin_ca(const Circuito *c)
{
    int k, q;
    for (k = 0; k < c->nel; k++) {
        const Elemento *e = &c->el[k];
        for (q = 0; q < Q_N; q++)
            if (circ_mag_c(e, q)->est_ang != M_LIBRE) return 0;
        if (e->tipo == EL_VATI && (e->val.est == M_DATO || e->aux.est != M_LIBRE)) return 0;
        if (e->tipo == EL_COND && (e->I.est != M_LIBRE || e->val.est == M_BUSCAR)) return 0;
        if (e->tipo == EL_BOB && (e->V.est != M_LIBRE || e->val.est == M_BUSCAR)) return 0;
    }
    return c->frec.est != M_BUSCAR;
}

/* R31: E1 continua de 12 V, W1 con fp = 0,6 (dato de CA), R1 = 4 con
 * V = 12∠0 (escrito en CA) y Z1 = 8∠0, que es lo único que hace el circuito
 * de alterna. En paralelo, Z1 va al lado de R1 (se puede sustituir o borrar
 * y V(R1) sigue siendo 12); en serie, detrás de R1 (tapada con un cable,
 * V(R1) = 12). Z1 siempre de (2,0) a (2,1). */
static void r3d_deja_ca(Circuito *c, int paralelo)
{
    int k;
    circ_init(c, "deja la CA");
    k = circ_elemento(c, EL_FUENTE, 0, 1, DIR_ARR);
    r3d_dato(&c->el[k].val, 12.0);
    k = circ_elemento(c, EL_VATI, 0, 0, DIR_DER);
    r3d_dato(&c->el[k].aux, 0.6);
    if (paralelo) {
        k = circ_elemento(c, EL_RES, 1, 0, DIR_ABA);
        circ_cable(c, 1, 1, DIR_IZQ);
        circ_cable(c, 1, 0, DIR_DER);
        circ_cable(c, 2, 1, DIR_IZQ);
    } else {
        k = circ_elemento(c, EL_RES, 1, 0, DIR_DER);
        circ_cable(c, 2, 1, DIR_IZQ);
        circ_cable(c, 1, 1, DIR_IZQ);
    }
    r3d_dato(&c->el[k].val, 4.0);
    r3d_fasor(&c->el[k].V, 12.0, 0.0);
    k = circ_elemento(c, EL_IMP, 2, 0, DIR_ABA);
    r3d_fasor(&c->el[k].val, 8.0, 0.0);
    r3d_dato(&c->frec, 60.0);
}

/* R31: lo que tiene que quedar tras dejar de ser CA, en el modelo */
static void r3d_tras_dejar_ca(const char *como)
{
    static Circuito b;
    const Circuito *cu = ui_circuito();
    char det[400];
    int st;
    sprintf(det, "%s: el circuito queda en CC (sin Z ni fuentes alternas)", como);
    chk_cond("R31 deja la CA", det, !circ_alterna(cu));
    sprintf(det, "%s: sin ángulos (V de R1) ni fp de W1 escondidos en el modelo", como);
    chk_cond("R31 deja la CA", det, r3d_sin_ca(cu));
    sprintf(det, "%s: el fp de W1 se vacía", como);
    chk_cond("R31 deja la CA", det, el_de_etiqueta(cu, "W1") >= 0 && cu->el[el_de_etiqueta(cu, "W1")].aux.est == M_LIBRE);
    sprintf(det, "%s: el módulo de V(R1) se queda (12 V)", como);
    chk_cond("R31 deja la CA", det, el_de_etiqueta(cu, "R1") >= 0 && cu->el[el_de_etiqueta(cu, "R1")].V.est == M_DATO &&
             cu->el[el_de_etiqueta(cu, "R1")].V.v == 12.0);
    st = r3d_resuelve(cu);
    sprintf(det, "%s: la calculadora de CC lo resuelve (dice «%s»)", como, r3d_msj);
    chk_cond("R31 deja la CA", det, st == SOL_OK);
    chk_cond("R31 deja la CA", "y no dice que algo es de la alterna", st != SOL_SOLO_CA && !strstr(r3d_msj, "alterna"));
    sprintf(det, "%s: circ_valido", como);
    chk_cond("R31 deja la CA", det, circ_valido(cu));
    sprintf(det, "%s: se guarda y se vuelve a abrir igual, sin nada de la alterna", como);
    chk_cond("R31 deja la CA", det, arch_guardar(1, cu) && arch_cargar(1, &b) && mismos(&b, cu) && r3d_sin_ca(&b));
    arch_borrar(1);
}

/* R32: E1 fasor 10∠0 a 1 kHz, R1 = 1 k con I [?!] y C1 = 1 µF con
 * I = 5 mA (o L1 = 0,1 H con V = 5 V): la fila «(fasor)» solo se ve en CA */
static void r3d_rc(Circuito *c, int bob)
{
    int k;
    circ_init(c, bob ? "V de L" : "I de C");
    k = circ_elemento(c, EL_FUENTE, 0, 1, DIR_ARR);
    c->el[k].variante = FORMA_FASOR;
    r3d_fasor(&c->el[k].val, 10.0, 0.0);
    r3d_dato(&c->frec, 1000.0);
    k = circ_elemento(c, EL_RES, 0, 0, DIR_DER);
    r3d_dato(&c->el[k].val, 1000.0);
    r3d_pide(&c->el[k].I);
    k = circ_elemento(c, bob ? EL_BOB : EL_COND, 1, 0, DIR_ABA);
    r3d_dato(&c->el[k].val, bob ? 0.1 : 1e-6);
    if (bob) r3d_dato(&c->el[k].V, 5.0);
    else r3d_dato(&c->el[k].I, 5e-3);
    circ_cable(c, 1, 1, DIR_IZQ);
}

/* R33: serie E, R, Z, L, C con etiquetas de una o de dos cifras */
static void r3d_serie_ca(Circuito *c, int dos)
{
    static const char *const ET[2][5] = { { "E1", "R1", "Z1", "L1", "C1" }, { "E10", "R11", "Z12", "L13", "C14" } };
    int k;
    circ_init(c, "estado de CA");
    k = circ_elemento(c, EL_FUENTE, 0, 1, DIR_ARR);
    c->el[k].variante = FORMA_FASOR;
    r3d_fasor(&c->el[k].val, 10.0, 0.0);
    k = circ_elemento(c, EL_RES, 0, 0, DIR_DER);
    r3d_dato(&c->el[k].val, 10.0);
    k = circ_elemento(c, EL_IMP, 1, 0, DIR_DER);
    r3d_fasor(&c->el[k].val, 5.0, 30.0);
    k = circ_elemento(c, EL_BOB, 2, 0, DIR_ABA);
    r3d_dato(&c->el[k].val, 0.1);
    k = circ_elemento(c, EL_COND, 2, 1, DIR_IZQ);
    r3d_dato(&c->el[k].val, 100e-6);
    circ_cable(c, 1, 1, DIR_IZQ);
    r3d_dato(&c->frec, 60.0);
    for (k = 0; k < 5; k++) strcpy(c->el[k].etiq, ET[dos][k]);
}

/* R34: stderr (donde escribe el UBSan de la variante asan) se desvía a un
 * archivo mientras se escribe y se dibuja; luego se cuenta lo que llegó */
static int  r3d_err_fd = -1, r3d_err_copia = -1;
static char r3d_err_ruta[300];
static void r3d_stderr_captura(void)
{
    sprintf(r3d_err_ruta, "%s/r3d_stderr.txt", dir_out);
    fflush(stderr);
    r3d_err_copia = dup(2);
    r3d_err_fd = open(r3d_err_ruta, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (r3d_err_copia >= 0 && r3d_err_fd >= 0) dup2(r3d_err_fd, 2);
}
static long r3d_stderr_suelta(void)
{
    FILE *f;
    long n;
    char lin[240];
    fflush(stderr);
    if (r3d_err_copia >= 0) { dup2(r3d_err_copia, 2); close(r3d_err_copia); }
    if (r3d_err_fd >= 0) close(r3d_err_fd);
    if (r3d_err_copia < 0 || r3d_err_fd < 0) { r3d_err_copia = r3d_err_fd = -1; return -1; }
    r3d_err_copia = r3d_err_fd = -1;
    f = fopen(r3d_err_ruta, "r");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    rewind(f);
    while (n > 0 && fgets(lin, sizeof lin, f)) printf("    [stderr] %s", lin);   /* que se vea el aviso */
    fclose(f);
    remove(r3d_err_ruta);
    return n;
}

static void test_revision_e3_modelo(void)
{
    static Circuito c, b, d;
    static CalRes pl;
    const Circuito *cu = ui_circuito();
    char det[400];
    int k, n, st;

    printf("[54c] Revisión de la Etapa 3: modelo y memoria\n");
    gfx_desbordes_reset();

    /* ---- R29: el fp del vatímetro, en [-1, 1] ---- */
    circ_init(&c, "fp del W");
    k = circ_elemento(&c, EL_FUENTE, 0, 1, DIR_ARR);
    c.el[k].variante = FORMA_FASOR;
    r3d_fasor(&c.el[k].val, 120.0, 0.0);
    r3d_dato(&c.frec, 60.0);
    k = circ_elemento(&c, EL_VATI, 0, 0, DIR_DER);
    r3d_dato(&c.el[k].aux, 0.6);
    k = circ_elemento(&c, EL_RES, 1, 0, DIR_ABA);
    r3d_dato(&c.el[k].val, 10.0);
    circ_cable(&c, 1, 1, DIR_IZQ);
    b = c;
    b.el[1].aux.v = 5.0;
    chk_cond("R29 fp W", "el modelo no admite fp = 5 (circ_valido)", circ_valido(&c) && !circ_valido(&b));
    chk_cond("R29 fp W", "el problema bueno (fp = 0,6) se abre", r3d_abre(&c) && abre_ficha_de("W1"));
    k = el_de_etiqueta(cu, "W1");
    TECLAS(T_ABAJO, T_DIG(5), T_ENTER);                             /* fila fp: 5 */
    chk_cond("R29 fp W", "fp = 5 [enter]: se queda en la ficha con «Fuera de rango: entre -1 y 1»",
             ui_pantalla() == P_ELEMENTO && ve("Fuera de rango: entre -1 y 1"));
    chk_cond("R29 fp W", "fp = 5 no entra en el modelo: sigue 0,6", k >= 0 && cu->el[k].aux.est == M_DATO && cu->el[k].aux.v == 0.6);
    chk_cond("R29 fp W", "el circuito en edición sigue válido (circ_valido)", circ_valido(cu));
    TECLAS(T_SIGNO, T_DIG(7), T_ENTER);                             /* (-) 7 */
    chk_cond("R29 fp W", "fp = -7 tampoco entra", ui_pantalla() == P_ELEMENTO && k >= 0 && cu->el[k].aux.v == 0.6 && circ_valido(cu));
    TECLAS(T_SIGNO, T_DIG(1), T_ENTER);                             /* (-) 1: el borde */
    chk_cond("R29 fp W", "fp = -1 (el borde) sí entra", ui_pantalla() == P_EDITOR && k >= 0 &&
             cu->el[k].aux.est == M_DATO && cu->el[k].aux.v == -1.0);
    r3d_guarda_menu();
    chk_cond("R29 fp W", "[menu] > Guardar: «Guardado»", ve("Guardado"));
    chk_cond("R29 fp W", "el hueco guardado se vuelve a abrir (arch_cargar) con fp = -1",
             arch_cargar(0, &b) && b.el[1].tipo == EL_VATI && b.el[1].aux.est == M_DATO && b.el[1].aux.v == -1.0);
    for (n = 0; n < 3 && ui_pantalla() != P_TALLER; n++) TECLAS(T_ESC);
    TECLAS(T_ABAJO, T_ENTER);                                       /* Mis problemas */
    chk_cond("R29 fp W", "en Mis problemas no sale «(no se puede abrir)»", ui_pantalla() == P_MIS && !ve("(no se puede abrir)"));
    TECLAS(T_ENTER);
    chk_cond("R29 fp W", "[enter] lo abre en el editor con fp = -1", ui_pantalla() == P_EDITOR && cu->nel == 3 &&
             cu->el[1].aux.est == M_DATO && cu->el[1].aux.v == -1.0);
    limpia_mis();

    /* ---- R30: la paleta tiene PAL_N piezas; [der] en la última fila no pasa
     * de la última. En función de PAL_N (§6.1 de la Etapa 4: vale con 20, 21 y
     * 22 piezas): en un editor nuevo (el cursor en (1,1)), [ab] x6 se queda en
     * la última fila f = (PAL_N - 1)/4 y n [der], [enter] y [→] ponen la pieza
     * mín(4f + n, PAL_N - 1) ---- */
    for (n = 1; n <= 3; n++) {
        int f = (PAL_N - 1) / 4, pieza = 4 * f + n < PAL_N - 1 ? 4 * f + n : PAL_N - 1;
        int tp = ui_paleta_tipo(pieza);
        ui_init();
        TECLAS(T_ENTER, T_ENTER, T_ENTER);                          /* editor nuevo */
        TECLAS(T_CATALOGO);
        repite(T_ABAJO, 6);                                         /* más filas de las que hay */
        repite(T_DER, n);
        TECLAS(T_ENTER, T_DER);
        if (ui_pantalla() == P_ELEMENTO) TECLAS(T_ESC);
        sprintf(det, "[ab] x6, %d [der], [enter] y una flecha: la pieza %d (tipo < EL_N)", n, pieza);
        chk_cond("R30 paleta", det, tp >= 0 && tp < EL_N && cu->nel == 1 && cu->el[0].tipo == tp);
        chk_cond("R30 paleta", "el circuito es v" S_aa "lido (circ_valido)", circ_valido(cu));
        chk_cond("R30 paleta", "y se guarda y se vuelve a abrir", arch_guardar(0, cu) && arch_cargar(0, &b) &&
                 b.nel == 1 && b.el[0].tipo == tp);
        limpia_mis();
    }

    /* ---- R31: al dejar de ser CA en el editor, circ_quita_ca ---- */
    r3d_deja_ca(&c, 1);
    chk_cond("R31 deja la CA", "el de partida es de alterna, válido, con V(R1) = 12" S_ANG "0 y fp(W1) = 0,6",
             circ_alterna(&c) && circ_valido(&c) && c.el[2].V.est_ang == M_DATO && c.el[1].aux.est == M_DATO);
    st = r3d_resuelve(&c);
    chk_cond("R31 deja la CA", "(la calculadora de CC lo manda a la CA)", st == SOL_CA);
    /* a) sustituir la Z por una R de 8 */
    chk_cond("R31 deja la CA", "abierto (sustituir)", r3d_abre(&c));
    TECLAS(T_DER, T_DER, T_LETRA('R'), T_ABAJO);                    /* una R sobre la Z1 */
    chk_cond("R31 deja la CA", "la R nueva abre su ficha", ui_pantalla() == P_ELEMENTO && el_de_etiqueta(cu, "Z1") < 0);
    TECLAS(T_DIG(8), T_ENTER);
    r3d_tras_dejar_ca("sustituir Z1 por R2 = 8");
    /* b) taparla con un cable (en serie) */
    r3d_deja_ca(&c, 0);
    chk_cond("R31 deja la CA", "abierto (tapar con cable)", r3d_abre(&c) && circ_alterna(cu));
    TECLAS(T_DER, T_DER, T_LETRA('W'), T_ABAJO, T_ESC);             /* un cable sobre la Z1 */
    chk_cond("R31 deja la CA", "la Z1 ya no está", el_de_etiqueta(cu, "Z1") < 0 && cu->nel == 3);
    r3d_tras_dejar_ca("tapar Z1 con un cable");
    /* c) borrarla */
    r3d_deja_ca(&c, 1);
    chk_cond("R31 deja la CA", "abierto (borrar)", r3d_abre(&c) && circ_alterna(cu));
    TECLAS(T_DER, T_DER, T_BORRAR, T_ABAJO, T_ESC);                 /* [del] y la flecha */
    chk_cond("R31 deja la CA", "la Z1 ya no está", el_de_etiqueta(cu, "Z1") < 0 && cu->nel == 3);
    r3d_tras_dejar_ca("borrar Z1");
    limpia_mis();

    /* ---- R32: la I de un C y la V de una L, fuera al salir de la CA ---- */
    for (n = 0; n < 2; n++) {
        const char *nom = n ? "L1" : "C1";
        const int q = n ? Q_V : Q_I;
        int kc, kr;
        r3d_rc(&c, n);
        kc = el_de_etiqueta(&c, nom);
        kr = el_de_etiqueta(&c, "R1");
        /* en el modelo, a mano: E1 a continua y circ_quita_ca */
        d = c;
        d.el[0].variante = ONDA_CC;
        d.el[0].val.est_ang = M_LIBRE;
        d.el[0].val.ang = 0.0;
        d.el[kr].I.est_ang = M_LIBRE;
        sprintf(det, "solo con %s de %s como dato de CA: circ_quita_ca dice que quitó algo", n ? "la V" : "la I", nom);
        chk_cond("R32 C y L en CC", det, circ_quita_ca(&d) == 1);
        sprintf(det, "y %s de %s queda vacía (M_LIBRE y 0)", n ? "la V" : "la I", nom);
        chk_cond("R32 C y L en CC", det, circ_mag_c(&d.el[kc], q)->est == M_LIBRE && circ_mag_c(&d.el[kc], q)->v == 0.0);
        chk_cond("R32 C y L en CC", "una segunda pasada ya no quita nada", circ_quita_ca(&d) == 0);
        st = r3d_resuelve(&d);
        sprintf(det, "%s: la calculadora de CC lo resuelve (dice «%s»)", nom, r3d_msj);
        chk_cond("R32 C y L en CC", det, st == SOL_OK && !strstr(r3d_msj, "contradicen"));
        /* por teclas: la ficha de E1, de alterna a continua */
        chk_cond("R32 C y L en CC", "abierto", r3d_abre(&c) && abre_ficha_de("E1"));
        TECLAS(T_DER, T_ENTER);
        sprintf(det, "E1 a continua: %s de %s ya no queda como dato escondido", n ? "la V" : "la I", nom);
        chk_cond("R32 C y L en CC", det, !circ_alterna(cu) && kc < cu->nel && circ_mag_c(&cu->el[kc], q)->est == M_LIBRE);
        chk_cond("R32 C y L en CC", "nada de la alterna en el modelo", r3d_sin_ca(cu) && circ_valido(cu));
        st = r3d_resuelve(cu);
        sprintf(det, "%s: sin «se contradicen» (dice «%s»)", nom, r3d_msj);
        chk_cond("R32 C y L en CC", det, st == SOL_OK && !strstr(r3d_msj, "contradicen"));
        sprintf(det, "%s: I(R1) = %s", nom, n ? "10 mA (L es un cable)" : "0 (C es un abierto)");
        chk_t("R32 C y L en CC", det, st == SOL_OK ? fabs(r3d_s.s[0].I[kr]) : -1.0, n ? 0.01 : 0.0, 0.01, 1e-12);
        r3d_guarda_menu();
        sprintf(det, "guardado y vuelto a abrir: %s de %s sigue vacía", n ? "la V" : "la I", nom);
        chk_cond("R32 C y L en CC", det, arch_cargar(0, &b) && circ_mag_c(&b.el[kc], q)->est == M_LIBRE && r3d_resuelve(&b) == SOL_OK);
        limpia_mis();
    }

    /* ---- R33: la línea de estado de la calculadora de CA, en 52 columnas ---- */
    {
        static const char *const COLA[5] = { "", "listo " S_CHECK " pulsa [=] (CA)", "sobra 1 (se comprueba)",
                                             "sobran 2 (se comprueban)", "sobran 3 (se comprueban)" };
        int dos, ang, nd, d0;
        for (dos = 0; dos < 2; dos++)
            for (ang = 0; ang < 2; ang++)
                for (nd = 0; nd <= 4; nd++) {
                    char cola[48], caso[120];
                    r3d_serie_ca(&c, dos);
                    if (nd == 0) {
                        /* sin incógnitas: V(R) de dato y todo lo demás pedido */
                        r3d_dato(&c.el[1].V, 5.0);
                        r3d_pide(&c.el[1].I);
                        for (k = 2; k < 5; k++) { r3d_pide(&c.el[k].V); r3d_pide(&c.el[k].I); }
                    } else {
                        /* el valor de R buscado y nd datos */
                        c.el[1].val.est = M_BUSCAR;
                        r3d_dato(&c.el[1].V, 5.0);
                        if (nd > 1) r3d_dato(&c.el[2].V, 2.0);
                        if (nd > 2) r3d_dato(&c.el[2].I, 0.3);
                        if (nd > 3) r3d_dato(&c.el[3].V, 3.0);
                        r3d_pide(&c.el[1].I);
                        r3d_pide(&c.el[3].I);
                        r3d_pide(&c.el[4].V);
                        r3d_pide(&c.el[4].I);
                    }
                    if (ang) c.el[1].V.est_ang = M_BUSCAR;               /* «∠V(R)» en la lista */
                    sprintf(caso, "etiquetas de %s cifra%s, %s, %d dato%s", dos ? "dos" : "una", dos ? "s" : "",
                            ang ? "con un ángulo pedido" : "sin ángulos pedidos", nd ? nd : 1, nd > 1 ? "s" : "");
                    cal_plantea(&c, &pl);
                    sprintf(det, "%s: la calculadora de CA plantea %d incógnitas y %d ecuaciones", caso, nd ? 1 : 0, nd ? nd : 1);
                    chk_cond("R33 estado CA", det, pl.ninc == (nd ? 1 : 0) && pl.necu == (nd ? nd : 1));
                    chk_cond("R33 estado CA", "abierto", r3d_abre(&c));
                    strcpy(cola, ": ");
                    strcat(cola, nd ? COLA[nd] : "solo se comprueba");
                    d0 = gfx_desbordes();
                    sprintf(det, "%s: «Pides ...%s» se ve entera, sin salirse de la pantalla", caso, cola);
                    chk_cond("R33 estado CA", det, ve("Pides ") && ve(cola) && gfx_desbordes() == d0);
                }
    }
    limpia_mis();

    /* ---- R34: un módulo infinito en la ficha de una Z ---- */
    {
        volatile double grande = 9e300;
        double inf = grande * 1e9;                                     /* 9E300G: +Inf */
        char t[48];
        int kz, inv, vio_g, vio_ca;
        long nerr;
        /* num_fasor con |Z| = Inf: en x86 un double fuera de rango convertido
         * a int levanta FE_INVALID (el UBSan de gcc no lo mira sin
         * -fsanitize=float-cast-overflow) */
        feclearexcept(FE_ALL_EXCEPT);
        num_fasor(t, sizeof t, hypot(inf, 1.0), 0.0, 0, 0, S_OHM);
        inv = fetestexcept(FE_INVALID) != 0;
        chk_cond("R34 módulo inf", "num_fasor(|Inf+j1|): sin conversiones inválidas (FE_INVALID)", !inv);
        chk_cond("R34 módulo inf", "y escribe «inf»", strstr(t, "inf") != 0);
        feclearexcept(FE_ALL_EXCEPT);
        num_fasor(t, sizeof t, inf, 45.0, 0, 1, "A");
        inv = fetestexcept(FE_INVALID) != 0;
        chk_cond("R34 módulo inf", "num_fasor corto (esquema) con Inf: sin FE_INVALID", !inv && strstr(t, "inf") != 0);
        /* por teclas: 9 [EE] 3 0 0 [G] [+] 1 en la ficha de Z1 */
        circ_init(&c, "inf");
        k = circ_elemento(&c, EL_FUENTE, 0, 1, DIR_ARR);
        c.el[k].variante = FORMA_FASOR;
        r3d_fasor(&c.el[k].val, 10.0, 0.0);
        r3d_dato(&c.frec, 60.0);
        k = circ_elemento(&c, EL_IMP, 0, 0, DIR_DER);
        r3d_fasor(&c.el[k].val, 5.0, 0.0);
        circ_cable(&c, 1, 0, DIR_ABA);
        circ_cable(&c, 1, 1, DIR_IZQ);
        chk_cond("R34 módulo inf", "abierto con la ficha de Z1", r3d_abre(&c) && abre_ficha_de("Z1"));
        kz = el_de_etiqueta(cu, "Z1");
        r3d_stderr_captura();
        TECLAS(T_DIG(9), T_EE, T_DIG(3), T_DIG(0), T_DIG(0), T_LETRA('G'));    /* el eco general */
        vio_g = ve("inf");
        TECLAS(T_MAS, T_DIG(1));                                               /* el eco de la CA */
        vio_ca = ve("inf");
        nerr = r3d_stderr_suelta();
        chk_cond("R34 módulo inf", "el eco de 9E300G dice «inf»", vio_g);
        chk_cond("R34 módulo inf", "el eco de 9E300G+j1 dice «inf»", vio_ca);
        chk_cond("R34 módulo inf", "escribir y dibujar 9E300G+j1 no deja avisos en stderr (el UBSan de asan)", nerr == 0);
        TECLAS(T_ENTER);
        chk_cond("R34 módulo inf", "[enter]: «Fuera de rango» y sigue en la ficha", ui_pantalla() == P_ELEMENTO && ve("Fuera de rango"));
        chk_cond("R34 módulo inf", "el valor no se guarda: Z1 sigue en 5" S_ANG "0",
                 kz >= 0 && cu->el[kz].val.est == M_DATO && cu->el[kz].val.v == 5.0 &&
                 cu->el[kz].val.est_ang == M_DATO && cu->el[kz].val.ang == 0.0);
        chk_cond("R34 módulo inf", "el circuito en edición sigue válido (circ_valido)", circ_valido(cu));
        TECLAS(T_DIG(5), T_ENTER);
        r3d_guarda_menu();
        chk_cond("R34 módulo inf", "guardado y vuelto a abrir: Z1 = 5" S_ANG "0, finito",
                 ve("Guardado") && arch_cargar(0, &b) && kz >= 0 && b.el[kz].val.v == 5.0 && FINITO(b.el[kz].val.v) &&
                 b.el[kz].val.ang == 0.0);
        limpia_mis();
    }

    ui_init();
    chk_cond("revisión E3 modelo", "sin textos fuera de la pantalla", gfx_desbordes() == 0);
}
