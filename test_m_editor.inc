/* --------------------------------------------------------------------------
 *  70. Mejoras del editor y del flujo (después del renombrado)
 *
 *  Una prueba por mejora (cada una falla sin ella, comprobado por mutación):
 *    M1   la sesión: al salir (y al dejar el editor) se guarda el circuito,
 *         su hueco y el cursor en ces_sesion.tns; main.c la abre al arrancar
 *         y Taller > Seguir la recupera. ui_init no la abre; un archivo roto
 *         no se carga; el hueco solo vale si su archivo sigue ahí y es el
 *         mismo (la huella de la cola)
 *    M2   Problema nuevo, abrir otro y [tab] desde un ejemplo se deshacen con
 *         [ctrl][Z], con su hueco (Guardar no pisa el que se abrió, ni el que
 *         ocupó el hueco de uno borrado en Mis problemas); rehacer no cruza
 *         de un documento a otro
 *    M3   [menu] en la ficha confirma el campo y abre las Opciones del editor
 *    M4   [tab] y luego [enter] abren la ficha del elegido; [tab] en la ficha
 *         de CC pasa a la del siguiente ([shift][tab]: la del anterior)
 *    M6   al volver de un error, el cursor en el culpable y [enter] abre su
 *         ficha en la fila que falla (emulador, calculadora de CC y CA,
 *         punto de operación)
 *    M8   poner una pieza o un cable sobre un elemento, o borrarlo, lo dice
 *         (y [ctrl][Z] en la ficha lo deshace)
 *    M10  mirar una ficha no gasta un nivel de deshacer; [ctrl][Y] rehace
 *         (también desde la ficha)
 *    M12  [ctrl][S] guarda (también desde la ficha); un ejemplo traído
 *         conserva su nombre; Guardar como copia; si falla un hueco nuevo, ni
 *         hueco ni nombre cambian, ni queda medio archivo
 *  Se incluye tras test_rev3_v_modelo.inc (usa r2e_nuevo, r2e_alterna,
 *  r3u_abre, r4_el, r4_espera, r4_vuelca, u4_dato, u4_controladas, lazo,
 *  mismos, escribe_cesp, limpia_mis y carpeta_mis). Sus pantallas van a
 *  out/m_*.bmp, fuera de la numeración de vuelca(). HAKARU_SOLO_M_EDITOR la
 *  corre sola.
 * ----------------------------------------------------------------------- */
#include <signal.h>
#include <sys/resource.h>

/* Del editor al Taller con [esc] (uno quita la elección, otro sale) */
static void me_al_taller(void)
{
    int i;
    for (i = 0; i < 4 && ui_pantalla() != P_TALLER; i++) TECLAS(T_ESC);
}

/* [ctrl][S] con los archivos limitados a 16 bytes, como una flash llena: el
 * archivo se crea y la escritura se corta */
static void me_guarda_cortado(void)
{
    struct rlimit antes, tope;
    fflush(stdout);
    signal(SIGXFSZ, SIG_IGN);                 /* si no, pasarse del tope mata el arnés */
    getrlimit(RLIMIT_FSIZE, &antes);
    tope = antes;
    tope.rlim_cur = 16;
    setrlimit(RLIMIT_FSIZE, &tope);
    TECLAS(T_CTRL, T_LETRA('S'));
    setrlimit(RLIMIT_FSIZE, &antes);
    signal(SIGXFSZ, SIG_DFL);
}

/* Sale del programa con [esc] como en la calculadora: devuelve cuántas
 * teclas hicieron falta (hasta que ui_tecla da 0), o -1 si no salió */
static int me_sale(void)
{
    int i;
    for (i = 1; i <= 12; i++) { ui_dibuja(); if (!ui_tecla(T_ESC)) return i; }
    return -1;
}

/* Arranca como main.c: ui_init, la sesión y portada, menú, Taller */
static void me_arranca(void)
{
    ui_init();
    ui_sesion_abrir();
    TECLAS(T_ENTER, T_ENTER);
}

/* El nombre que debe tener la copia de un problema que no acaba en « (N)»:
 * nombre + suf, recortando el nombre (y sus espacios del final) a 23 */
static void me_copia_de(const char *nom, char *dst, const char *suf)
{
    size_t n = strlen(nom), m = strlen(suf);
    if (n + m > 23) n = 23 - m;
    while (n > 0 && nom[n - 1] == ' ') n--;
    memcpy(dst, nom, n);
    strcpy(dst + n, suf);
}

static void me_ruta_sesion(char *ruta)
{
    sprintf(ruta, "%s/ces_sesion.tns", carpeta_mis);
}

static int me_hay_sesion(void)
{
    char ruta[300];
    FILE *f;
    me_ruta_sesion(ruta);
    f = fopen(ruta, "rb");
    if (f) fclose(f);
    return f != 0;
}

/* E1 5 V (0,1)->(0,0), R1 1 kΩ (0,0)->(1,0), C1 SIN valor (1,0)->(1,1) y el
 * cable de vuelta: [=] emula y el emulador no arranca (falta C en C1) */
static void me_rc_sin_c(Circuito *c)
{
    int k;
    circ_init(c, "RC sin C");
    k = circ_elemento(c, EL_FUENTE, 0, 1, DIR_ARR); u4_dato(&c->el[k].val, 5.0);
    k = circ_elemento(c, EL_RES, 0, 0, DIR_DER);    u4_dato(&c->el[k].val, 1e3);
    circ_elemento(c, EL_COND, 1, 0, DIR_ABA);
    circ_cable(c, 1, 1, DIR_IZQ);
}

/* Un cable recto de (x0,y0) hacia d, n tramos */
static void me_cables(Circuito *c, int x, int y, int d, int n)
{
    int i;
    for (i = 0; i < n; i++) {
        circ_cable(c, x, y, d);
        circ_vecino(x, y, d, &x, &y);
    }
}

/* E1 alterna de 10∠0° V a 50 Hz y Z1 = 1234,567 + j2345,678 Ω (un valor
 * largo), en un lazo ancho: E1 de (0,2) a (0,1) y un cable hasta (0,0), Z1
 * de (0,0) a (1,0) y la vuelta por la columna 3 (el rótulo de E1, de dos
 * líneas, en la fila 1 le tapaba media Z1; y con la vuelta en la columna 1
 * cruzaba su cable) */
static void me_ca_z(Circuito *c)
{
    int k;
    circ_init(c, "Z larga");
    k = circ_elemento(c, EL_FUENTE, 0, 2, DIR_ARR);
    c->el[k].variante = FORMA_FASOR;
    u4_dato(&c->el[k].val, 10.0); c->el[k].val.est_ang = M_DATO;
    u4_dato(&c->frec, 50.0);
    circ_cable(c, 0, 1, DIR_ARR);
    k = circ_elemento(c, EL_IMP, 0, 0, DIR_DER);
    u4_dato(&c->el[k].val, hypot(1234.567, 2345.678));
    c->el[k].val.ang = atan2(2345.678, 1234.567) * 180.0 / PI_T; c->el[k].val.est_ang = M_DATO;
    me_cables(c, 1, 0, DIR_DER, 2); me_cables(c, 3, 0, DIR_ABA, 2); me_cables(c, 3, 2, DIR_IZQ, 3);
}

/* El mismo lazo ancho con E1 alterna de 10∠0° V y sin la f del circuito;
 * R1 de (0,0) a (1,0) y R2 bajando por la columna 3 (en la 1, el rótulo
 * de E1 le tapaba el zigzag y su «?» pisaba la «V» del de E1) */
static void me_ca_sin_f(Circuito *c)
{
    int k;
    circ_init(c, "CA sin f");
    k = circ_elemento(c, EL_FUENTE, 0, 2, DIR_ARR);
    c->el[k].variante = FORMA_FASOR;
    u4_dato(&c->el[k].val, 10.0); c->el[k].val.ang = 0.0; c->el[k].val.est_ang = M_DATO;
    circ_cable(c, 0, 1, DIR_ARR);
    k = circ_elemento(c, EL_RES, 0, 0, DIR_DER);    u4_dato(&c->el[k].val, 1e3);
    me_cables(c, 1, 0, DIR_DER, 2);
    k = circ_elemento(c, EL_RES, 3, 0, DIR_ABA);    u4_dato(&c->el[k].val, 2e3);
    me_cables(c, 3, 1, DIR_ABA, 1); me_cables(c, 3, 2, DIR_IZQ, 3);
}

/* El lazo de siempre (E1 = 12 V de (0,1) a (0,0), R1 = 4 Ω de (0,0) a
 * (1,0)) con R2 = 8 Ω en la columna 3: en la 1, el «12V» de E1 tocaba su
 * zigzag */
static void me_lazo_ancho(Circuito *c)
{
    int k;
    circ_init(c, "lazo");
    k = circ_elemento(c, EL_FUENTE, 0, 1, DIR_ARR); u4_dato(&c->el[k].val, 12.0);
    k = circ_elemento(c, EL_RES, 0, 0, DIR_DER);    u4_dato(&c->el[k].val, 4.0);
    me_cables(c, 1, 0, DIR_DER, 2);
    k = circ_elemento(c, EL_RES, 3, 0, DIR_ABA);    u4_dato(&c->el[k].val, 8.0);
    me_cables(c, 3, 1, DIR_IZQ, 3);
}

/* E1 5 V, R1 1 kΩ y un LED azul con If = 250 mA, más de lo que aguanta:
 * [=] va al punto de operación, que no arranca (SIM_MODELO en la fila If) */
static void me_led_if(Circuito *c)
{
    int k;
    circ_init(c, "LED con If");
    k = circ_elemento(c, EL_FUENTE, 0, 1, DIR_ARR); u4_dato(&c->el[k].val, 5.0);
    k = circ_elemento(c, EL_RES, 0, 0, DIR_DER);    u4_dato(&c->el[k].val, 1e3);
    k = circ_elemento(c, EL_LED, 1, 0, DIR_ABA);
    c->el[k].variante = LED_AZUL;
    u4_dato(&c->el[k].par[QL_IF - Q_P1], 0.25);
    circ_cable(c, 1, 1, DIR_IZQ);
}

static void test_m_editor(void)
{
    static Circuito c, d, x;
    const Circuito *cu = ui_circuito();
    const char *q;
    char ruta[300];
    FILE *f;
    int i, k, h, cx, cy, n, nr;

    printf("[70] Mejoras del editor y del flujo\n");
    gfx_desbordes_reset();

    /* ================= M1. La sesión ================= */
    q = "M1 sesi" S_oo "n";
    limpia_mis();
    arch_sesion_borrar();
    me_arranca();
    TECLAS(T_ENTER);
    chk_cond(q, "sin sesi" S_oo "n: el Taller empieza en Problema nuevo, vac" S_ii "o",
             ui_pantalla() == P_EDITOR && cu->nel == 0 && !ve("Recuperado"));
    TECLAS(T_LETRA('R'), T_DER, T_DIG(4), T_ENTER);
    n = me_sale();
    chk_cond(q, "[esc] hasta salir del programa (5 veces)", n == 5 && ui_pantalla() == P_INICIO);
    ui_sesion_guardar();                                  /* lo que hace main.c al salir */
    chk_cond(q, "queda ces_sesion.tns", me_hay_sesion());
    ui_init();
    TECLAS(T_ENTER, T_ENTER, T_ENTER);
    chk_cond(q, "ui_init NO abre la sesi" S_oo "n (el arn" S_ee "s empieza vac" S_ii "o)",
             ui_pantalla() == P_EDITOR && cu->nel == 0);
    me_arranca();
    chk_cond(q, "abierta, el Taller elige «Seguir» (su nombre y la miniatura, no la ayuda)",
             ui_pantalla() == P_TALLER && !ve("Dibuja el circuito: una tecla y luego una flecha."));
    TECLAS(T_ENTER);
    chk_cond(q, "[enter]: el circuito de la " S_uu "ltima vez, con su aviso",
             ui_pantalla() == P_EDITOR && cu->nel == 1 && cu->el[0].tipo == EL_RES &&
             cu->el[0].val.est == M_DATO && cu->el[0].val.v == 4.0 &&
             ve("Recuperado de la " S_uu "ltima vez: [ctrl][S] lo guarda."));
    chk_cond(q, "sin historial: no hay nada que deshacer", ui_niveles(&nr) == 0 && nr == 0);
    r4_vuelca("m_sesion_recuperada");
    TECLAS(T_DIG(1));                                     /* una tecla que no hace nada */
    chk_cond(q, "el aviso dura una tecla; el cursor, donde estaba (2,1)", !ve("Recuperado") &&
             ve("Aqu" S_ii ": " S_LARR "R1"));
    /* sin entrar con Seguir, Problema nuevo: el aviso de la sesión ya no sale
     * al volver al editor con Seguir (y [ctrl][Z] trae lo recuperado) */
    me_arranca();
    TECLAS(T_ARRIBA, T_ENTER);
    chk_cond(q, "(Problema nuevo tras abrir la sesi" S_oo "n)", ui_pantalla() == P_EDITOR && cu->nel == 0);
    TECLAS(T_ESC, T_ENTER);
    chk_cond(q, "... y otra vez Seguir: sin «Recuperado»", ui_pantalla() == P_EDITOR && cu->nel == 0 && !ve("Recuperado"));
    TECLAS(T_CTRL, T_LETRA('Z'));
    chk_cond(q, "... y [ctrl][Z]: la R de la sesi" S_oo "n", cu->nel == 1 && cu->el[0].val.v == 4.0);

    /* vacío: no queda sesión */
    ui_init();
    ui_sesion_guardar();
    chk_cond(q, "con el circuito vac" S_ii "o se borra el archivo", !me_hay_sesion());

    /* dejar el editor por el Taller, o por [menu] > Salir, ya la guarda */
    r2e_nuevo();
    TECLAS(T_LETRA('R'), T_DER, T_DIG(5), T_ENTER, T_ESC, T_ESC);
    chk_cond(q, "(en el Taller)", ui_pantalla() == P_TALLER);
    me_arranca();
    TECLAS(T_ENTER);
    chk_cond(q, "[esc] del editor al Taller guarda la sesi" S_oo "n", cu->nel == 1 && cu->el[0].val.v == 5.0);
    TECLAS(T_LETRA('R'), T_ARRIBA, T_DIG(6), T_ENTER, T_MENU, T_ARRIBA, T_ENTER);
    chk_cond(q, "(en el men" S_uu ")", ui_pantalla() == P_MENU);
    me_arranca();
    TECLAS(T_ENTER);
    chk_cond(q, "[menu] > Salir al men" S_uu " tambi" S_ee "n", cu->nel == 2);

    /* un archivo roto o cortado no se carga: se empieza vacío, sin avisos */
    me_ruta_sesion(ruta);
    f = fopen(ruta, "wb");
    if (f) { fputs("esto no es un circuito", f); fclose(f); }
    me_arranca();
    TECLAS(T_ENTER);
    chk_cond(q, "basura: vac" S_ii "o y sin aviso", ui_pantalla() == P_EDITOR && cu->nel == 0 && !ve("Recuperado"));
    lazo(&c, 0, 12.0, 4.0, 8.0);
    chk_cond(q, "(la sesi" S_oo "n se escribe)", arch_sesion_guardar(&c, -1, 0, 0));
    {
        static unsigned char buf[8192];
        n = 0;
        f = fopen(ruta, "rb");
        if (f) { n = (int)fread(buf, 1, sizeof buf, f); fclose(f); }
        f = fopen(ruta, "wb");
        if (f) { fwrite(buf, 1, (size_t)(n / 2), f); fclose(f); }
    }
    me_arranca();
    TECLAS(T_ENTER);
    chk_cond(q, "cortada a la mitad: vac" S_ii "o", n > 0 && cu->nel == 0);

    /* la cola: hueco (con su archivo) y cursor; sin cola, el circuito vale igual */
    arch_guardar(19, &c);
    chk_cond(q, "cola: se escribe", arch_sesion_guardar(&c, 19, 4, 5));
    chk_cond(q, "cola: hueco 19 y cursor (4, 5)", arch_sesion_cargar(&d, &h, &cx, &cy) && mismos(&c, &d) &&
             h == 19 && cx == 4 && cy == 5);
    strcpy(d.nombre, "Otro en el 20");
    arch_guardar(19, &d);                                 /* otro problema en ese hueco */
    chk_cond(q, "cola: si el archivo del hueco cambi" S_oo ", hueco -1 (el cursor s" S_ii ")",
             arch_sesion_cargar(&d, &h, &cx, &cy) && h == -1 && cx == 4 && cy == 5);
    arch_borrar(19);
    chk_cond(q, "cola: sin su archivo, hueco -1", arch_sesion_guardar(&c, 19, 4, 5) &&
             arch_sesion_cargar(&d, &h, &cx, &cy) && h == -1);
    chk_cond(q, "cola: un cursor fuera de la rejilla (200, 250) vuelve a (0, 0)", arch_sesion_guardar(&c, -1, 200, 250) &&
             arch_sesion_cargar(&d, &h, &cx, &cy) && mismos(&c, &d) && cx == 0 && cy == 0);
    escribe_cesp(ruta, &c, 0, 0);                         /* un CESP sin la cola */
    chk_cond(q, "sin cola: el circuito, hueco -1 y (0, 0)", arch_sesion_cargar(&d, &h, &cx, &cy) && mismos(&c, &d) &&
             h == -1 && cx == 0 && cy == 0);

    /* el hueco solo vale si su archivo sigue ahí */
    limpia_mis();
    lazo(&d, 0, 5.0, 1.0, 1.0);
    circ_init(&x, "");
    strcpy(d.nombre, "Otro");
    arch_guardar(0, &d);                                  /* el 0 ocupado; el 3 no existe */
    arch_sesion_guardar(&c, 3, 0, 0);
    me_arranca();
    TECLAS(T_ENTER, T_CTRL, T_LETRA('S'));
    chk_cond(q, "hueco 3 sin archivo: [ctrl][S] guarda en uno nuevo (el 2)", arch_existe(1) && !arch_existe(3) &&
             arch_cargar(0, &x) && strcmp(x.nombre, "Otro") == 0);
    limpia_mis();
    arch_guardar(0, &d);
    arch_guardar(3, &c);
    c.el[1].val.v = 7.0;                                  /* la sesión tiene un cambio sin guardar */
    arch_sesion_guardar(&c, 3, 0, 0);
    me_arranca();
    TECLAS(T_ENTER, T_CTRL, T_LETRA('S'));
    chk_cond(q, "hueco 3 con archivo: [ctrl][S] lo sobrescribe", !arch_existe(1) &&
             arch_cargar(3, &x) && x.el[1].val.v == 7.0);
    /* el hueco 3 lo ocupa después OTRO problema (borrado y guardado otro, o
     * copiado con TI Connect): [ctrl][S] no lo pisa, va a uno nuevo */
    limpia_mis();
    arch_guardar(0, &d);
    arch_guardar(3, &c);
    arch_sesion_guardar(&c, 3, 0, 0);
    lazo(&x, 0, 9.0, 9.0, 9.0);
    strcpy(x.nombre, "De otro");
    arch_guardar(3, &x);
    me_arranca();
    TECLAS(T_ENTER, T_CTRL, T_LETRA('S'));
    chk_cond(q, "hueco 3 con otro problema: [ctrl][S] va a uno nuevo (el 2)", arch_existe(1) &&
             arch_cargar(3, &x) && strcmp(x.nombre, "De otro") == 0 && arch_cargar(1, &x) && x.el[1].val.v == 7.0);
    limpia_mis();

    /* ================= M2. Problema nuevo, abrir y [tab] se deshacen ================= */
    q = "M2 nuevo y abrir";
    r2e_nuevo();
    TECLAS(T_LETRA('R'), T_DER, T_DIG(4), T_ENTER, T_LETRA('E'), T_ABAJO, T_DIG(9), T_ENTER);
    chk_cond(q, "(R1 y E1)", cu->nel == 2);
    TECLAS(T_ESC, T_ESC);
    chk_cond(q, "(Taller con «Seguir»)", ui_pantalla() == P_TALLER);
    TECLAS(T_ARRIBA, T_ENTER);
    chk_cond(q, "Problema nuevo: vac" S_ii "o y lo dice", ui_pantalla() == P_EDITOR && cu->nel == 0 &&
             ve("Problema nuevo. [ctrl][Z] recupera el anterior."));
    r4_vuelca("m_nuevo_deshacible");
    TECLAS(T_CTRL, T_LETRA('Z'));
    chk_cond(q, "[ctrl][Z] recupera el de antes", cu->nel == 2 && cu->el[r4_el(cu, "R1")].val.v == 4.0);
    /* abrir otro: deshacer vuelve con su hueco (-1), y Guardar no pisa el abierto */
    lazo(&d, 0, 5.0, 1.0, 1.0);
    strcpy(d.nombre, "Abierto");
    arch_guardar(0, &d);
    TECLAS(T_ESC, T_ABAJO, T_ENTER, T_ENTER);            /* Taller > Mis problemas > el 1 */
    chk_cond(q, "abierto, y dice que [ctrl][Z] vuelve", ui_pantalla() == P_EDITOR && strcmp(cu->nombre, "Abierto") == 0 &&
             ve("Abierto. [ctrl][Z] vuelve al que ten" S_ii "as."));
    TECLAS(T_CTRL, T_LETRA('Z'));
    chk_cond(q, "[ctrl][Z]: el de antes", cu->nel == 2 && strcmp(cu->nombre, "Problema nuevo") == 0);
    TECLAS(T_CTRL, T_LETRA('S'));
    chk_cond(q, "Guardar tras deshacer: en un hueco nuevo, el abierto intacto",
             arch_existe(1) && arch_cargar(0, &x) && mismos(&x, &d) && strcmp(cu->nombre, "Mi problema 2") == 0);
    TECLAS(T_CTRL, T_LETRA('Y'));
    chk_cond(q, "[ctrl][Y]: otra vez el abierto (y su hueco)", strcmp(cu->nombre, "Abierto") == 0);
    TECLAS(T_DER, T_DER, T_LETRA('R'), T_DER, T_ENTER, T_CTRL, T_LETRA('S'));   /* R en (2,0)-(3,0), libre */
    chk_cond(q, "... Guardar lo escribe en el suyo (el 1)", arch_cargar(0, &x) && x.nel == d.nel + 1 && !arch_existe(2));
    /* un ejemplo con [tab] */
    TECLAS(T_ESC, T_ESC, T_ESC, T_ABAJO, T_ENTER);        /* menú > Aprende */
    repite(T_ARRIBA, N_LECCIONES);
    TECLAS(T_ENTER, T_ENTER);
    chk_cond(q, "(los ejemplos de la lecci" S_oo "n 1)", ui_pantalla() == P_EJEMPLOS);
    n = cu->nel;
    TECLAS(T_TAB);
    ejemplo_cargar(LECCIONES[0].ej[0], &x);
    memcpy(ruta, EJEMPLOS[(int)LECCIONES[0].ej[0]].enunciado, 20);   /* el principio del enunciado */
    ruta[20] = 0;
    /* (rev8, cierre) su enunciado no cabe en una línea: ocupa las dos, entero,
     * y que [ctrl][Z] vuelve sale con la tecla siguiente */
    {
        const char *en = EJEMPLOS[(int)LECCIONES[0].ej[0]].enunciado;
        char cola[16];
        size_t ne = strlen(en);
        memcpy(cola, en + (ne > 12 ? ne - 12 : 0), 13);   /* el final («e circula?») */
        cola[12] = 0;
        chk_cond(q, "[tab]: el ejemplo y su enunciado entero, en las dos l" S_ii "neas", ui_pantalla() == P_EDITOR &&
                 mismos(cu, &x) && ve(ruta) && ve(cola) && !ve("Tra" S_ii "do al Taller"));
        r4_vuelca("m_traido");
        TECLAS(T_DIG(1));                                 /* una tecla que no hace nada */
        chk_cond(q, "... una tecla: debajo, que [ctrl][Z] vuelve", !ve(ruta) && !ve(cola) &&
                 ve("Tra" S_ii "do al Taller: [ctrl][Z] vuelve al que ten" S_ii "as."));
    }
    TECLAS(T_DIG(1));
    chk_cond(q, "... otra: vuelve la l" S_ii "nea de estado", !ve(ruta) && !ve("Tra" S_ii "do al Taller"));
    TECLAS(T_CTRL, T_LETRA('Z'));
    chk_cond(q, "[ctrl][Z]: el que hab" S_ii "a", cu->nel == n && strcmp(cu->nombre, "Abierto") == 0);
    limpia_mis();

    /* borrar en Mis problemas el hueco del de antes: deshacer hasta él y
     * Guardar no pisa al que ocupó después ese hueco (va a uno nuevo) */
    r2e_nuevo();
    TECLAS(T_LETRA('R'), T_DER, T_DIG(4), T_ENTER, T_CTRL, T_LETRA('S'));
    chk_cond(q, "(A, en el hueco 1)", arch_existe(0));
    me_al_taller();
    TECLAS(T_ABAJO, T_ENTER, T_BORRAR, T_BORRAR, T_ESC);  /* Mis problemas: se borra el 1 */
    chk_cond(q, "(borrado)", ui_pantalla() == P_TALLER && !arch_existe(0));
    TECLAS(T_ARRIBA, T_ARRIBA, T_ENTER);                  /* Problema nuevo */
    TECLAS(T_LETRA('E'), T_ABAJO, T_DIG(9), T_ENTER, T_CTRL, T_LETRA('S'));
    chk_cond(q, "(B, otra vez en el hueco 1)", arch_cargar(0, &x) && x.nel == 1 && x.el[0].tipo == EL_FUENTE);
    TECLAS(T_CTRL, T_LETRA('Z'), T_CTRL, T_LETRA('Z'));
    chk_cond(q, "(deshecho hasta A)", cu->nel == 1 && cu->el[0].tipo == EL_RES);
    TECLAS(T_CTRL, T_LETRA('S'));
    chk_cond(q, "hueco borrado y reutilizado: Guardar A va a uno nuevo; B, intacto",
             arch_cargar(0, &x) && x.el[0].tipo == EL_FUENTE && arch_cargar(1, &x) && x.el[0].tipo == EL_RES &&
             strcmp(cu->nombre, "Mi problema 2") == 0);
    limpia_mis();

    /* rehacer no cruza a otro documento: con el editor vacío pero con algo
     * que rehacer, Problema nuevo lo olvida */
    r2e_nuevo();
    TECLAS(T_LETRA('R'), T_DER, T_ENTER, T_CTRL, T_LETRA('Z'));
    chk_cond(q, "(vac" S_ii "o, con uno para rehacer)", cu->nel == 0 && ui_niveles(&nr) == 0 && nr == 1);
    me_al_taller();
    TECLAS(T_ARRIBA, T_ENTER, T_CTRL, T_LETRA('Y'));
    chk_cond(q, "Problema nuevo y [ctrl][Y]: nada que rehacer", ui_pantalla() == P_EDITOR && cu->nel == 0 &&
             ve("Nada que rehacer."));

    /* ================= M3. [menu] en la ficha ================= */
    q = "M3 [menu] en la ficha";
    r2e_nuevo();
    TECLAS(T_LETRA('R'), T_DER, T_DIG(4), T_MENU);
    chk_cond(q, "confirma el 4 y abre las Opciones del editor", ui_pantalla() == P_EDITOR && ve("Opciones") &&
             cu->el[0].val.est == M_DATO && cu->el[0].val.v == 4.0);
    r4_vuelca("m_menu_en_ficha");
    TECLAS(T_ESC);
    chk_cond(q, "[esc] las cierra", ui_pantalla() == P_EDITOR && !ve("Opciones"));
    TECLAS(T_LETRA('R'), T_DER, T_DIG(0), T_MENU);
    chk_cond(q, "un valor rechazado: se queda en la ficha con su aviso", ui_pantalla() == P_ELEMENTO &&
             ve("Demasiado peque"));
    TECLAS(T_BORRAR, T_DIG(2), T_ENTER);
    /* en la ficha de un elemento que ya estaba, el cambio cuenta (un nivel)
     * antes de abrir las Opciones: su Deshacer lo quita */
    lazo(&c, 0, 12.0, 4.0, 8.0);
    chk_cond(q, "(abierto)", r3u_abre(&c));
    TECLAS(T_TAB, T_TAB, T_ENTER, T_DIG(9), T_MENU);
    chk_cond(q, "R1 = 9 y las Opciones, con un nivel", cu->el[1].val.v == 9.0 && ve("Opciones") && ui_niveles(0) == 1);
    repite(T_ABAJO, 2); TECLAS(T_ENTER);
    chk_cond(q, "Opciones > Deshacer: R1 vuelve a 4", cu->el[1].val.v == 4.0 && ui_niveles(0) == 0);

    /* ================= M4. [tab] y [enter]; [tab] en la ficha ================= */
    q = "M4 [tab] y [enter]";
    me_lazo_ancho(&c);                                    /* E1, R1 (0,0)-(1,0), R2 (3,0)-(3,1) */
    chk_cond(q, "(abierto)", r3u_abre(&c));
    TECLAS(T_TAB, T_TAB);
    chk_cond(q, "[tab][tab]: R1 elegida y lo dice", ui_sel() == 1 && ve("R1 (4") && ve(": [enter] su ficha  [tab] el siguiente"));
    r4_vuelca("m_tab_elegida");
    TECLAS(T_ENTER);
    chk_cond(q, "[enter]: su ficha, aunque en su borne est" S_ee " E1", ui_pantalla() == P_ELEMENTO && ui_sel() == 1 &&
             ve("R1  Resistencia"));
    chk_cond(q, "el pie de la ficha de CC dice [tab]", ve("[tab] siguiente"));
    r4_vuelca("m_ficha_tab");
    TECLAS(T_DIG(5), T_TAB);
    chk_cond(q, "[tab] en la ficha: guarda el 5 y abre la de R2", ui_pantalla() == P_ELEMENTO && ui_sel() == 2 &&
             ve("R2  Resistencia") && cu->el[1].val.v == 5.0);
    TECLAS(T_TAB);
    chk_cond(q, "... la de E1 (da la vuelta)", ui_pantalla() == P_ELEMENTO && ui_sel() == 0);
    TECLAS(T_SHIFT, T_TAB);
    chk_cond(q, "[shift][tab]: la del anterior (R2)", ui_pantalla() == P_ELEMENTO && ui_sel() == 2);
    TECLAS(T_ESC);
    TECLAS(T_SHIFT, T_TAB);
    chk_cond(q, "[shift][tab] en el editor: R1", ui_pantalla() == P_EDITOR && ui_sel() == 1);
    TECLAS(T_DER, T_IZQ, T_ENTER);
    chk_cond(q, "sin [tab] justo antes, [enter] pregunta la flecha (como hoy)", ui_pantalla() == P_EDITOR &&
             ve("Editar: flecha hacia el elemento"));
    TECLAS(T_ESC);
    /* en CA, [tab] sigue siendo el ángulo */
    r2e_alterna(&c);
    chk_cond(q, "(CA abierto)", r3u_abre(&c));
    TECLAS(T_TAB, T_ENTER);
    chk_cond(q, "CA: la ficha de E1", ui_pantalla() == P_ELEMENTO && ui_sel() == 0 && !ve("[tab] siguiente"));
    TECLAS(T_TAB);
    chk_cond(q, "CA: [tab] no cambia de ficha (es el " S_aa "ngulo)", ui_pantalla() == P_ELEMENTO && ui_sel() == 0);
    TECLAS(T_ESC, T_ESC);
    /* el pie de la ficha de CC dice [tab] donde cabe, en lugar de «[enter]
     * listo»: en las polares sin variante (C, L, W) y en las de variante sin
     * polaridad (V, A, S). Con [F] y variante (E, D, Q...) no cabe */
    r2e_nuevo();
    TECLAS(T_LETRA('C'), T_DER);
    chk_cond(q, "pie de C en CC: [F] girar [tab] siguiente", ui_pantalla() == P_ELEMENTO &&
             ve("[arr/ab] campo [F] girar [tab] siguiente [=]"));
    TECLAS(T_ESC, T_LETRA('V'), T_ABAJO);
    chk_cond(q, "pie de V en CC: [izq/der] modo [tab] siguiente", ui_pantalla() == P_ELEMENTO &&
             ve("[arr/ab] campo [izq/der] modo [tab] siguiente [=]"));
    TECLAS(T_TAB);
    chk_cond(q, "... y [tab] lo hace: la ficha de C1", ui_pantalla() == P_ELEMENTO && ui_sel() == 0);
    TECLAS(T_ESC);

    /* ================= M6. Volver de un error al culpable ================= */
    q = "M6 culpable";
    me_rc_sin_c(&c);
    chk_cond(q, "(abierto)", r3u_abre(&c));
    TECLAS(T_IGUAL);
    r4_espera();
    chk_cond(q, "emular sin C: no arranca y el pie dice ad" S_oo "nde ir" S_aa,
             ui_pantalla() == P_EMU_ESQ && ve("[esc] al editor: el cursor ir" S_aa " a C1"));
    r4_vuelca("m_culpable_emu");
    TECLAS(T_ESC);
    chk_cond(q, "[esc]: C1 elegido y lo que le falta", ui_pantalla() == P_EDITOR && ui_sel() == 2 &&
             ve("C1: falta C. [enter] abre su ficha."));
    r4_vuelca("m_culpable_editor");
    TECLAS(T_ENTER);
    chk_cond(q, "[enter]: su ficha, en la fila de C", ui_pantalla() == P_ELEMENTO && ui_sel() == 2 &&
             ve("C1  Condensador") && ui_campo_q() == Q_VAL);
    r4_vuelca("m_culpable_ficha");
    TECLAS(T_ESC, T_ESC);
    /* la calculadora de CC: E2 sin control (SOL_CTL) va a su fila x */
    u4_controladas(&c, 0);
    chk_cond(q, "(abierto)", r3u_abre(&c));
    TECLAS(T_IGUAL);
    chk_cond(q, "CC: el error y el pie", ui_pantalla() == P_RESULTADO && ve("[esc] al editor: el cursor ir" S_aa " a E2"));
    TECLAS(T_ESC);
    chk_cond(q, "CC: E2 elegido", ui_pantalla() == P_EDITOR && ui_sel() == 2 && ve("El error est" S_aa " en E2: [enter] abre su ficha."));
    r4_vuelca("m_culpable_cc");
    TECLAS(T_ENTER);
    chk_cond(q, "CC: su ficha en la fila x (no la primera)", ui_pantalla() == P_ELEMENTO && ui_sel() == 2 &&
             ui_campo_q() == Q_CTRL);
    TECLAS(T_ESC, T_ESC);
    /* la CA: sin f, a la fila f de la ficha de E1 */
    me_ca_sin_f(&c);
    chk_cond(q, "(abierto)", r3u_abre(&c));
    TECLAS(T_IGUAL);
    chk_cond(q, "CA: el error y el pie", ui_pantalla() == P_CA && ve("[esc] al editor: el cursor ir" S_aa " a E1"));
    TECLAS(T_ESC, T_ENTER);
    chk_cond(q, "CA: la ficha de E1 en la fila f", ui_pantalla() == P_ELEMENTO && ui_sel() == 0 && ui_campo_q() == Q_FCIRC);
    TECLAS(T_ESC, T_ESC);
    /* la calculadora de CA (con un [?!]): sin f, cal_plantea no dice de quién
     * es; el culpable es la fuente alterna, en la fila f, como sin [?!] */
    me_ca_sin_f(&c);
    c.el[2].val.est = M_BUSCAR;                           /* R2 [?!], con |I| = 1 mA */
    u4_dato(&c.el[2].I, 1e-3);
    chk_cond(q, "(abierto)", r3u_abre(&c));
    TECLAS(T_IGUAL);
    r4_espera();
    chk_cond(q, "calculadora de CA sin f: el pie dice E1", ui_pantalla() == P_CA &&
             ve("[esc] al editor: el cursor ir" S_aa " a E1"));
    r4_vuelca("m_culpable_calca");
    TECLAS(T_ESC, T_ENTER);
    chk_cond(q, "... [esc][enter]: la ficha de E1 en la fila f", ui_pantalla() == P_ELEMENTO && ui_sel() == 0 &&
             ui_campo_q() == Q_FCIRC);
    TECLAS(T_ESC, T_ESC);
    /* lo que en CA no se despeja (CAL_NO_CA): la r de E1 con [?!], a su fila */
    me_ca_sin_f(&c);
    u4_dato(&c.frec, 50.0);
    c.el[0].aux.est = M_BUSCAR;
    chk_cond(q, "(abierto)", r3u_abre(&c));
    TECLAS(T_IGUAL);
    r4_espera();
    chk_cond(q, "la r de E1 con [?!] en CA: el pie dice E1", ui_pantalla() == P_CA &&
             ve("[esc] al editor: el cursor ir" S_aa " a E1"));
    TECLAS(T_ESC, T_ENTER);
    chk_cond(q, "... [esc][enter]: la ficha de E1 en la fila r (no en la E)", ui_pantalla() == P_ELEMENTO &&
             ui_sel() == 0 && ui_campo_q() == Q_AUX);
    TECLAS(T_ESC, T_ESC);
    /* el punto de operación (P_RESULTADO con res_nl): R1 sin valor con un D */
    circ_init(&c, "D sin R");
    k = circ_elemento(&c, EL_FUENTE, 0, 1, DIR_ARR); u4_dato(&c.el[k].val, 5.0);
    circ_elemento(&c, EL_RES, 0, 0, DIR_DER);
    circ_elemento(&c, EL_DIODO, 1, 0, DIR_ABA);
    circ_cable(&c, 1, 1, DIR_IZQ);
    chk_cond(q, "(abierto)", r3u_abre(&c));
    TECLAS(T_IGUAL);
    r4_espera();
    chk_cond(q, "punto de operaci" S_oo "n sin R: el pie dice R1", ui_pantalla() == P_RESULTADO &&
             ve("[esc] al editor: el cursor ir" S_aa " a R1"));
    TECLAS(T_ESC, T_ENTER);
    chk_cond(q, "... [esc][enter]: la ficha de R1 en la fila R", ui_pantalla() == P_ELEMENTO && ui_sel() == 1 &&
             ui_campo_q() == Q_VAL);
    TECLAS(T_ESC, T_ESC);
    /* un parámetro imposible del modelo (SIM_MODELO): a su fila, no a la primera */
    me_led_if(&c);
    chk_cond(q, "(abierto)", r3u_abre(&c));
    TECLAS(T_IGUAL);
    r4_espera();
    chk_cond(q, "LED con If de m" S_aa "s: el error va a su ficha", ui_pantalla() == P_RESULTADO &&
             ve("[esc] al editor: el cursor ir" S_aa " a "));
    TECLAS(T_ESC, T_ENTER);
    chk_cond(q, "... [esc][enter]: la ficha del LED en la fila If", ui_pantalla() == P_ELEMENTO && ui_sel() == 2 &&
             ui_campo_q() == QL_IF);
    TECLAS(T_ESC, T_ESC);
    /* sin culpable, como hoy; y la marca dura una tecla */
    TECLAS(T_MENU); repite(T_ABAJO, 4); TECLAS(T_ENTER);  /* Borrar todo */
    TECLAS(T_IGUAL);
    chk_cond(q, "vac" S_ii "o: el pie de siempre", ui_pantalla() == P_RESULTADO && ve("[esc] volver al editor y corregirlo"));
    TECLAS(T_ESC);
    chk_cond(q, "vac" S_ii "o: nada elegido", ui_pantalla() == P_EDITOR && ui_sel() == -1 && !ve("abre su ficha"));
    me_rc_sin_c(&c);
    r3u_abre(&c);
    TECLAS(T_IGUAL);
    r4_espera();
    TECLAS(T_ESC, T_DER, T_IZQ, T_ENTER);
    chk_cond(q, "tras una flecha, [enter] ya no abre la ficha del culpable", ui_pantalla() == P_EDITOR &&
             ve("Editar: flecha hacia el elemento"));
    TECLAS(T_ESC);
    /* un error que no viene del editor no toca el editor, aunque en el
     * circuito del editor haya un culpable: la calculadora de CA halla R1
     * ([?!]: para emular le falta R) y [O] emula su solución desde P_CA, que
     * no arranca porque una Z no existe en el tiempo */
    me_ca_z(&c);
    c.el[1].val.v = 1000.0; c.el[1].val.ang = 0.0;       /* Z1 = 1 kΩ */
    k = circ_elemento(&c, EL_RES, 3, 0, DIR_ABA);         /* R1 en lugar del cable de la columna 3 */
    c.el[k].val.est = M_BUSCAR;
    u4_dato(&c.el[k].I, 5e-3);                            /* |I| = 5 mA: R1 = 1 kΩ */
    chk_cond(q, "(abierto)", r3u_abre(&c));
    TECLAS(T_IGUAL);
    r4_espera();
    chk_cond(q, "(la calculadora de CA halla R1)", ui_pantalla() == P_CA && ui_ca_sol() == 0);
    TECLAS(T_LETRA('O'));
    r4_espera();
    chk_cond(q, "desde la CA: el error, con el pie de siempre", ui_pantalla() == P_EMU_ESQ &&
             ve("[esc] volver") && !ve("el cursor ir"));
    TECLAS(T_ESC);
    chk_cond(q, "... [esc] vuelve a la CA sin elegir nada", ui_pantalla() == P_CA && ui_sel() == -1);
    TECLAS(T_ESC);
    chk_cond(q, "... y el editor, como estaba", ui_pantalla() == P_EDITOR && ui_sel() == -1 && !ve("abre su ficha"));

    /* ================= M8. Sustituir o borrar un elemento lo dice ================= */
    q = "M8 sustituye";
    r2e_nuevo();
    TECLAS(T_LETRA('R'), T_DER, T_DIG(4), T_PUNTO, T_DIG(7), T_LETRA('K'), T_ENTER);
    TECLAS(T_IZQ, T_LETRA('E'), T_DER);
    chk_cond(q, "una E sobre R1: su ficha dice a qui" S_ee "n sustituye", ui_pantalla() == P_ELEMENTO &&
             cu->nel == 1 && cu->el[0].tipo == EL_FUENTE && ve("Sustituye a R1 (4,7k") && ve(": [ctrl][Z] lo deshace."));
    r4_vuelca("m_sustituye");
    TECLAS(T_CTRL, T_LETRA('Z'));
    chk_cond(q, "[ctrl][Z] en la ficha: vuelve R1 con su valor", ui_pantalla() == P_EDITOR && cu->nel == 1 &&
             cu->el[0].tipo == EL_RES && cu->el[0].val.v == 4700.0);
    TECLAS(T_IZQ, T_LETRA('W'), T_DER);
    chk_cond(q, "un cable sobre R1 lo dice", cu->nel == 0 && ve("Cable en lugar de R1 (4,7k") && ve(": [ctrl][Z] lo deshace."));
    r4_vuelca("m_cable_sustituye");
    TECLAS(T_ESC, T_CTRL, T_LETRA('Z'));
    chk_cond(q, "(vuelve R1)", cu->nel == 1);
    TECLAS(T_IZQ, T_BORRAR, T_DER);
    chk_cond(q, "borrar R1 lo dice", cu->nel == 0 && ve("Se borr" S_oo " R1 (4,7k"));
    TECLAS(T_ESC, T_CTRL, T_LETRA('Z'));
    chk_cond(q, "(vuelve R1)", cu->nel == 1);
    TECLAS(T_LETRA('W'), T_ABAJO);                        /* (2,1)-(2,2): libre */
    chk_cond(q, "un cable en un sitio libre no avisa", !ve("lo deshace"));
    TECLAS(T_ESC, T_BORRAR, T_ARRIBA);
    chk_cond(q, "borrar un tramo de cable tampoco", !ve("lo deshace") && cu->nel == 1);
    TECLAS(T_ESC);
    /* un valor largo (una Z en R + jX): si no cabe, el aviso va sin él */
    me_ca_z(&c);
    chk_cond(q, "(abierto)", r3u_abre(&c));
    TECLAS(T_LETRA('R'), T_DER);
    chk_cond(q, "Z larga: el aviso de la ficha cabe entero", ui_pantalla() == P_ELEMENTO &&
             ve("Sustituye a Z1") && ve(": [ctrl][Z] lo deshace."));
    r4_vuelca("m_z_larga");
    TECLAS(T_CTRL, T_LETRA('Z'), T_IZQ, T_LETRA('W'), T_DER);
    chk_cond(q, "Z larga: el del cable tambi" S_ee "n", ve("Cable en lugar de Z1") && ve(": [ctrl][Z] lo deshace."));
    TECLAS(T_ESC, T_CTRL, T_LETRA('Z'), T_IZQ, T_BORRAR, T_DER);
    chk_cond(q, "Z larga: y el de borrar", ve("Se borr" S_oo " Z1") && ve(": [ctrl][Z] lo deshace."));
    TECLAS(T_ESC);

    /* ================= M10. Mirar no gasta; [ctrl][Y] ================= */
    q = "M10 deshacer y rehacer";
    r2e_nuevo();
    TECLAS(T_LETRA('R'), T_DER, T_DIG(4), T_ENTER);
    for (i = 0; i < 3; i++) TECLAS(T_IZQ, T_ENTER, T_ESC, T_DER);
    chk_cond(q, "tres fichas miradas: sigue un nivel", ui_niveles(0) == 1);
    TECLAS(T_TAB, T_ENTER, T_ESC);
    chk_cond(q, "[tab][enter][esc] tampoco gasta", ui_niveles(0) == 1);
    TECLAS(T_CTRL, T_LETRA('Z'));
    chk_cond(q, "un [ctrl][Z] quita la R", cu->nel == 0 && ve("Deshecho (quedan 0). [ctrl][Y] lo rehace."));
    r4_vuelca("m_deshecho");
    TECLAS(T_CTRL, T_LETRA('Y'));
    chk_cond(q, "[ctrl][Y] la rehace", cu->nel == 1 && cu->el[0].val.v == 4.0 && ve("Rehecho."));
    TECLAS(T_CTRL, T_LETRA('Y'));
    chk_cond(q, "no hay m" S_aa "s que rehacer", ve("Nada que rehacer."));
    TECLAS(T_ENTER, T_DIG(7), T_ENTER);                   /* R1, en (1,1) */
    chk_cond(q, "una ficha que cambia gasta un nivel", ui_niveles(0) == 2 && cu->el[0].val.v == 7.0);
    TECLAS(T_CTRL, T_LETRA('Z'));
    chk_cond(q, "... y [ctrl][Z] le devuelve el valor", cu->el[0].val.v == 4.0 && ui_niveles(&nr) == 1 && nr == 1);
    TECLAS(T_LETRA('R'), T_ABAJO, T_ENTER);
    chk_cond(q, "un cambio nuevo vac" S_ii "a rehacer", ui_niveles(&nr) == 2 && nr == 0);
    TECLAS(T_CTRL, T_LETRA('Y'));
    chk_cond(q, "(Nada que rehacer)", ve("Nada que rehacer."));
    /* [ctrl][Y] también en la ficha (como [ctrl][Z]): vuelve al editor y rehace */
    lazo(&c, 0, 12.0, 4.0, 8.0);
    chk_cond(q, "(abierto)", r3u_abre(&c));
    TECLAS(T_TAB, T_TAB, T_ENTER, T_DIG(9), T_ENTER, T_CTRL, T_LETRA('Z'));
    chk_cond(q, "(R1 = 9 deshecho: 4)", cu->el[1].val.v == 4.0 && ui_niveles(&nr) == 0 && nr == 1);
    TECLAS(T_TAB, T_TAB, T_ENTER, T_CTRL, T_LETRA('Y'));
    chk_cond(q, "[ctrl][Y] en la ficha: al editor y R1 = 9", ui_pantalla() == P_EDITOR && cu->el[1].val.v == 9.0 &&
             ve("Rehecho."));
    TECLAS(T_CTRL, T_LETRA('Z'), T_TAB, T_TAB, T_ENTER, T_DIG(7), T_ABAJO, T_CTRL, T_LETRA('Y'));
    chk_cond(q, "... con un cambio en la ficha, ese es lo " S_uu "ltimo: nada que rehacer", ui_pantalla() == P_EDITOR &&
             cu->el[1].val.v == 7.0 && ve("Nada que rehacer.") && ui_niveles(0) == 1);
    /* rehacer tras el primer Guardar sigue en ese hueco (doc_renombra también
     * pasa el hueco a los niveles de rehacer): no hace otro archivo */
    limpia_mis();
    r2e_nuevo();
    TECLAS(T_LETRA('R'), T_DER, T_ENTER, T_LETRA('E'), T_ABAJO, T_ENTER);
    TECLAS(T_CTRL, T_LETRA('Z'), T_CTRL, T_LETRA('S'));
    chk_cond(q, "(sin la E, en el hueco 1)", arch_cargar(0, &x) && x.nel == 1);
    TECLAS(T_CTRL, T_LETRA('Y'), T_CTRL, T_LETRA('S'));
    chk_cond(q, "[ctrl][Y] y Guardar: en el hueco 1, no en otro", !arch_existe(1) && arch_cargar(0, &x) && x.nel == 2 &&
             strcmp(cu->nombre, "Mi problema 1") == 0);
    limpia_mis();
    lazo(&c, 0, 12.0, 4.0, 8.0);
    d = c;
    chk_cond(q, "circ_igual: igual a s" S_ii " mismo", circ_igual(&c, &d));
    d.el[2].par[1].v = 1.0;
    chk_cond(q, "circ_igual: un par" S_aa "metro distinto", !circ_igual(&c, &d));
    d = c; strcpy(d.nombre, "otro");
    chk_cond(q, "circ_igual: el nombre cuenta", !circ_igual(&c, &d));

    /* ================= M12. Guardar mejor ================= */
    q = "M12 guardar";
    limpia_mis();
    r2e_nuevo();
    TECLAS(T_ESC, T_ESC, T_ABAJO, T_ENTER);               /* menú > Aprende */
    repite(T_ARRIBA, N_LECCIONES);
    TECLAS(T_ENTER, T_ENTER, T_TAB);
    ejemplo_cargar(LECCIONES[0].ej[0], &x);
    chk_cond(q, "(el ejemplo en el Taller, sin nada antes: su enunciado)", ui_pantalla() == P_EDITOR && mismos(cu, &x) &&
             !ve("Tra" S_ii "do al Taller"));
    TECLAS(T_CTRL, T_LETRA('S'));
    chk_cond(q, "[ctrl][S] guarda y conserva el nombre del ejemplo", arch_existe(0) && arch_cargar(0, &d) &&
             strcmp(d.nombre, x.nombre) == 0 && strcmp(cu->nombre, x.nombre) == 0 && ve("Guardado: "));
    TECLAS(T_LETRA('R'), T_DER, T_ENTER);                 /* un cambio sin guardar */
    TECLAS(T_MENU); repite(T_ARRIBA, 2); TECLAS(T_ENTER);  /* Guardar como copia */
    me_copia_de(x.nombre, ruta, " (2)");
    chk_cond(q, "como copia: en el hueco 2, con « (2)»", arch_existe(1) && arch_cargar(1, &d) &&
             strcmp(d.nombre, ruta) == 0 && strcmp(cu->nombre, ruta) == 0 && ve("Copia guardada: ") &&
             mismos(&d, cu));
    r4_vuelca("m_copia");
    chk_cond(q, "... el original, intacto", arch_cargar(0, &d) && mismos(&d, &x));
    /* deshacer el cambio de antes de la copia sigue en la copia: Guardar
     * escribe en ella (el hueco 2), no en el original ni en otro */
    TECLAS(T_CTRL, T_LETRA('Z'), T_CTRL, T_LETRA('S'));
    chk_cond(q, "deshacer tras la copia: [ctrl][S] escribe en la copia", !arch_existe(2) && arch_cargar(1, &d) &&
             strcmp(d.nombre, ruta) == 0 && (strcpy(d.nombre, x.nombre), mismos(&d, &x)) &&
             arch_cargar(0, &d) && mismos(&d, &x));
    TECLAS(T_MENU); repite(T_ARRIBA, 2); TECLAS(T_ENTER);  /* la copia de la copia */
    me_copia_de(x.nombre, ruta, " (3)");
    chk_cond(q, "la copia de la copia: « (3)», en el hueco 3", arch_cargar(2, &d) && strcmp(d.nombre, ruta) == 0);
    {
        /* el nombre de la copia: recortado sin dejar espacios, y « (N)» sigue */
        static const char *const NOMS[][2] = {
            { "Nombre de 23 caracteres", "Nombre de 23 caract (2)" },
            { "Doble espacio        x", "Doble espacio (2)" },
            { "Ya es (9)", "Ya es (10)" },
            { "Parentesis (a)", "Parentesis (a) (2)" },
        };
        for (i = 0; i < 4; i++) {
            lazo(&c, 0, 12.0, 4.0, 8.0);
            strcpy(c.nombre, NOMS[i][0]);
            r3u_abre(&c);
            TECLAS(T_MENU); repite(T_ARRIBA, 2); TECLAS(T_ENTER);
            sprintf(ruta, "copia de «%s»: «%s»", NOMS[i][0], NOMS[i][1]);
            chk_cond(q, ruta, strcmp(cu->nombre, NOMS[i][1]) == 0 && arch_cargar(1, &d) && strcmp(d.nombre, NOMS[i][1]) == 0);
        }
    }
    /* Problema nuevo guardado: «Mi problema N»; su copia, «Mi problema M» */
    r2e_nuevo();
    TECLAS(T_LETRA('R'), T_DER, T_ENTER, T_CTRL, T_LETRA('S'));
    chk_cond(q, "Problema nuevo: «Mi problema 3»", strcmp(cu->nombre, "Mi problema 3") == 0 && arch_existe(2));
    TECLAS(T_MENU); repite(T_ARRIBA, 2); TECLAS(T_ENTER);
    chk_cond(q, "su copia: «Mi problema 4»", strcmp(cu->nombre, "Mi problema 4") == 0 && arch_existe(3));
    /* si no se puede escribir en un hueco nuevo: ni hueco ni nombre */
    limpia_mis();
    r2e_nuevo();
    TECLAS(T_LETRA('R'), T_DER, T_ENTER);
    arch_carpeta("/no/existe/esta/carpeta");
    TECLAS(T_CTRL, T_LETRA('S'));
    arch_carpeta(carpeta_mis);
    chk_cond(q, "no se pudo: lo dice y el nombre sigue", ve("No se pudo guardar el archivo.") &&
             strcmp(cu->nombre, "Problema nuevo") == 0);
    lazo(&d, 0, 5.0, 1.0, 1.0);
    strcpy(d.nombre, "Ocupa el 1");
    arch_guardar(0, &d);
    TECLAS(T_CTRL, T_LETRA('S'));
    chk_cond(q, "... ni el hueco: el siguiente Guardar va a uno libre", arch_existe(1) && arch_cargar(0, &x) &&
             mismos(&x, &d) && strcmp(cu->nombre, "Mi problema 2") == 0);
    /* la escritura se corta a medias (la flash llena): el hueco nuevo no se
     * queda con medio archivo, que Mis problemas daría por roto */
    limpia_mis();
    r2e_nuevo();
    TECLAS(T_LETRA('R'), T_DER, T_ENTER);
    me_guarda_cortado();
    chk_cond(q, "escritura cortada: lo dice, sin medio archivo ni nombre nuevo", ve("No se pudo guardar el archivo.") &&
             !arch_existe(0) && strcmp(cu->nombre, "Problema nuevo") == 0);
    TECLAS(T_CTRL, T_LETRA('S'));
    chk_cond(q, "... y luego s" S_ii ": en el hueco 1", arch_existe(0) && strcmp(cu->nombre, "Mi problema 1") == 0);
    /* [ctrl][S] en la ficha: lo escrito vale (como [enter]), al editor y guarda */
    TECLAS(T_LETRA('R'), T_ABAJO, T_DIG(4), T_CTRL, T_LETRA('S'));
    chk_cond(q, "[ctrl][S] en la ficha: el 4 confirmado y guardado", ui_pantalla() == P_EDITOR && arch_cargar(0, &x) &&
             x.nel == 2 && x.el[1].val.est == M_DATO && x.el[1].val.v == 4.0 && ve("Guardado: Mi problema 1"));
    TECLAS(T_LETRA('R'), T_IZQ, T_DIG(0), T_CTRL, T_LETRA('S'));
    chk_cond(q, "... un valor rechazado: se queda en la ficha y no guarda", ui_pantalla() == P_ELEMENTO &&
             ve("Demasiado peque") && arch_cargar(0, &x) && x.nel == 2);
    TECLAS(T_BORRAR, T_DIG(2), T_ENTER);
    limpia_mis();
    arch_sesion_borrar();

    pruebas++;
    if (gfx_desbordes() != 0) falla("mejoras del editor", "textos que se salen de la pantalla", gfx_desbordes(), 0);
    (void)k;
}
