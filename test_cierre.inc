/* ============================================================================
 *  test_cierre.inc  -  [73] El cierre de la Etapa 4: lo que encontró la
 *  verificación final al juntar los trabajos (rev8, frentes «integracion» y
 *  «docs»). HAKARU_SOLO_CIERRE lo corre solo; va también en HAKARU_SOLO_E4 y al
 *  final de la batería.
 *
 *    C2  [ctrl] pegajoso y [menu] abren Controles también en la portada y en
 *        el banco ya hecho
 *    C3  con la paleta abierta, [ctrl] con Y, N y X hacen lo del editor
 *    C5  el grabador fuera de Controles: se ve y cuenta los segundos (el
 *        bucle repinta una vez por segundo, sin teclas)
 *    C6  los textos: el aviso de [G] en un punto suelto cabe en una línea; el
 *        enunciado traído con [tab] se ve entero (dos líneas) y que
 *        [ctrl][Z] vuelve sale después; la línea 5 de la Ayuda
 *    C7  la sesión tras «Problema nuevo» con el editor vacío: el de antes
 *    C8  el reloj de la capa a la vista (Controles): el plan, sus cambios y
 *        saltos, las cuentas por segundo del RTC, el tick y los idle()
 *    C9  el touchpad a la vista (Controles): Synaptics o CapTIvate y su W×H
 *        (el de omisión si no es creíble), para calibrar la capa
 *
 *  C1 y C4 (el ritmo de las animaciones y la tecla mantenida en una) son de
 *  la versión especial (ESP_CI_*).
 *
 *  Se incluye tras test_entrada.inc (usa ce_*, cm_*, r2e_nuevo, r3u_abre,
 *  me_*, limpia_mis, ayuda_tiene y r4_vuelca). Sus pantallas van a
 *  out/c_*.bmp, fuera de la numeración de vuelca().
 * ========================================================================= */

#if ESPECIAL
#include "test_cierre_esp.inc"          /* la versión especial (no se publica) */
#else
#define ESP_CI_C1
#define ESP_CI_MONTA
#define ESP_CI_PS
#define ESP_CI_NOM
#define ESP_CI_CTRL
#define ESP_CI_C4
#endif

/* ---------------------------------------------------------------------------
 *  C2. [ctrl] pegajoso y [menu]
 * ------------------------------------------------------------------------ */
static void ci_monta(int p)
{
    int i;
    ui_init();
    ui_semilla(3u);
    switch (p) {
    ESP_CI_MONTA
    case P_BANCO:
        TECLAS(T_ENTER, T_ABAJO, T_ABAJO, T_ENTER, T_LETRA('T'));
        for (i = 0; i < 20 && !banco_hecho(); i++) { ui_dibuja(); ui_tick(); }
        break;
    default: break;
    }
}

static void ci_ctrl_menu(void)
{
    static const int PS[] = { P_INICIO, ESP_CI_PS P_BANCO };
    static const char *const NOM[] = { "la portada", ESP_CI_NOM "el banco hecho" };
    const char *q = "C2 [ctrl][menu]";
    char det[128];
    int i, a, b, c;
    for (i = 0; i < (int)(sizeof PS / sizeof PS[0]); i++) {
        ci_monta(PS[i]);
        a = ui_pantalla();
        TECLAS(T_CTRL);
        c = ve("ctrl");
        TECLAS(T_MENU);
        b = ui_pantalla();
        TECLAS(T_ESC);
        sprintf(det, "desde %s, [ctrl] pegajoso (se ve «ctrl») y [menu]: Controles; [esc] vuelve", NOM[i]);
        chk_cond(q, det, a == PS[i] && c && b == P_CONTROLES && ui_pantalla() == PS[i]);
    }
    ESP_CI_CTRL
    /* midiendo el banco, [ctrl] no se arma (ni [ctrl][menu] abre) */
    ci_monta(P_BANCO);
    TECLAS(T_ENTER);                                        /* repetir: midiendo */
    TECLAS(T_CTRL);
    chk_cond(q, "con el banco midiendo, [ctrl] no se arma", !banco_hecho() && !ve("ctrl"));
    TECLAS(T_ESC, T_ESC);
}

/* ---------------------------------------------------------------------------
 *  C3. La paleta abierta y [ctrl]
 * ------------------------------------------------------------------------ */
/* La paleta, abierta (si ya lo estaba, [catálogo] la cerraría) */
static int ci_paleta_abre(void)
{
    if (!ve("Componentes")) TECLAS(T_CATALOGO);
    return ve("Componentes");
}

static void ci_paleta(void)
{
    const char *q = "C3 paleta";
    int nr, nr2, nel, ab;
    ce_escalera(&ce_c, 0);
    r3u_abre(&ce_c);
    TECLAS(T_LETRA('G'));                                   /* un cambio (la tierra en (0,0))... */
    TECLAS(T_CTRL, T_LETRA('Z'));                           /* ...deshecho: queda un rehacer */
    ui_niveles(&nr);
    ab = ci_paleta_abre();
    TECLAS(T_LETRA('Y') | T_CON_CTRL);
    ui_niveles(&nr2);
    chk_cond(q, "[ctrl][Y] con la paleta abierta: rehace, y la cierra", ab && nr == 1 && nr2 == 0 && !ve("Componentes"));
    ab = ci_paleta_abre();
    TECLAS(T_CTRL, T_LETRA('N'));                           /* también con [ctrl] pegajoso */
    chk_cond(q, "[ctrl][N] con la paleta abierta: a escribir el nombre", ab && ve("[letras] nombre") && !ve("Componentes"));
    TECLAS(T_ESC);
    if (ve("Componentes")) TECLAS(T_ESC);
    nel = ui_circuito()->nel;
    TECLAS(T_TAB);                                          /* el elegido... */
    ab = ci_paleta_abre();
    TECLAS(T_LETRA('X') | T_CON_CTRL);                      /* ...cortado */
    chk_cond(q, "[ctrl][X] con la paleta abierta: corta el elegido", ab && ui_circuito()->nel == nel - 1 && ve("Cortado "));
    ab = ci_paleta_abre();
    TECLAS(T_LETRA('Z') | T_CON_CTRL);
    chk_cond(q, "[ctrl][Z] con la paleta abierta: deshace, como antes", ab && ui_circuito()->nel == nel);
    ab = ci_paleta_abre();
    TECLAS(T_LETRA('R'));
    chk_cond(q, "[R] con la paleta abierta: la resistencia, como antes",
             ab && !ve("Componentes") && ui_indice(UI_I_ACCION) == 1);    /* ACC_PONER */
    TECLAS(T_ESC);
    limpia_mis();
}

/* ---------------------------------------------------------------------------
 *  C5. El grabador fuera del panel
 * ------------------------------------------------------------------------ */
static void ci_grabador(void)
{
    const char *q = "C5 grabador";
    unsigned char b[GRAB_CABECERA];
    long c0, c1, c2;
    int n, ms, g, v10, v5;
    arch_bytes_borrar("/ces_muestras.tns");
    ui_init();
    TECLAS(T_ENTER);                                        /* el menú */
    TECLAS(T_MENU | T_CON_CTRL);                            /* Controles */
    ce_abre(CE_W, CE_H, 1);
    ce_pulsa(TF_LETRA('G'));
    chk_cond(q, "(en el panel no sale la marca: lo dice su l" S_ii "nea)", ve("Grabando: ") && !ve("grabando "));
    ce_pulsa(TF_ESC);                                       /* al menú, grabando */
    v10 = ve("grabando 10 s");
    r4_vuelca("c_grabando_fuera");
    c0 = ce.cuadros;
    ce_pasa(5000);                                          /* sin tocar nada */
    c1 = ce.cuadros;
    v5 = ve("grabando 5 s") || ve("grabando 4 s");
    g = ent_grabacion(&n, &ms);
    chk_cond(q, "fuera del panel (el men" S_uu "), la marca «grabando 10 s»", ui_pantalla() == P_MENU && g == GRAB_GRABANDO && v10);
    chk_cond(q, "sin teclas, el bucle repinta una vez por segundo: la cuenta atr" S_aa "s", c1 - c0 >= 4 && c1 - c0 <= 6 && v5);
    ce_pasa(6000);
    c2 = ce.cuadros;
    g = ent_grabacion(&n, &ms);
    chk_cond(q, "a los 10 s, grabado: la marca se va (con su cuadro) y el archivo est" S_aa,
             g == GRAB_HECHO && !ve("grabando") && c2 - c1 >= 4 && arch_bytes_leer("/ces_muestras.tns", b, GRAB_CABECERA) == GRAB_CABECERA);
    TECLAS(T_MENU | T_CON_CTRL);
    chk_cond(q, "y en Controles, lo grabado", ve("Grabado: "));
    TECLAS(T_ESC);
    arch_bytes_borrar("/ces_muestras.tns");
    arch_bytes_borrar("/ces_controles.tns");
    ent_grabacion_olvida();
}

/* ---------------------------------------------------------------------------
 *  C6. Los textos
 * ------------------------------------------------------------------------ */
/* Al Taller con [tab] el ejemplo (o el modelo) j de la lección l, con la
 * escalera en el editor (así hay algo que [ctrl][Z] recupera) */
static void ci_trae(int l, int j)
{
    int i;
    ce_escalera(&ce_c, 0);
    r3u_abre(&ce_c);
    me_al_taller();
    TECLAS(T_ESC, T_ABAJO, T_ENTER);                        /* menú > Aprende */
    for (i = 0; i < l; i++) TECLAS(T_ABAJO);
    TECLAS(T_ENTER);
    for (i = 0; i < 40 && ui_pantalla() == P_TEORIA; i++) TECLAS(T_ENTER);
    for (i = 0; i < j; i++) TECLAS(T_ABAJO);
    TECLAS(T_TAB);
}

/* El final de s (sus últimas 12 columnas) */
static void ci_cola(const char *s, char *cola)
{
    size_t n = strlen(s);
    memcpy(cola, s + (n > 12 ? n - 12 : 0), 13);
    cola[12] = 0;
}

static void ci_textos(void)
{
    const char *q = "C6 textos";
    char det[160], cola[16], cabeza[24];
    int i, j, l, malos = 0, peor = 0, corta_l = -1, corta_j = -1, larga_l = -1, larga_j = -1;

    /* [G] en un punto suelto: el aviso entero en UNA línea (la de estado sigue) */
    ce_escalera(&ce_c, 0);
    r3u_abre(&ce_c);
    TECLAS(T_ABAJO, T_ABAJO, T_ABAJO, T_ABAJO);
    chk_cond(q, "(un punto suelto)", circ_grado(ui_circuito(), ui_indice(UI_I_CUR_X), ui_indice(UI_I_CUR_Y)) == 0);
    TECLAS(T_LETRA('G'));
    chk_cond(q, "[G] en un punto suelto: el aviso entero, en una l" S_ii "nea",
             ve("Aqu" S_ii " no hay circuito: la tierra, en cable o borne.") && ui_circuito()->tierra_x < 0);
    r4_vuelca("c_tierra_suelta");

    /* los enunciados y lo que hay que ver de los modelos: dos líneas como
     * mucho, y caben en el aviso del editor */
    for (i = 0; i < N_EJEMPLOS; i++) {
        int nl = lineas(EJEMPLOS[i].enunciado, 52);
        if (nl > 2 || strlen(EJEMPLOS[i].enunciado) > 127) malos++;
        if (nl > peor) peor = nl;
    }
    for (i = 0; i < N_MODELOS; i++) {
        int nl = lineas(MODELOS[i].que_ver, 52);
        if (nl > 2 || strlen(MODELOS[i].que_ver) > 127) malos++;
        if (nl > peor) peor = nl;
    }
    sprintf(det, "enunciados y «qu" S_ee " ver» de los modelos: dos l" S_ii "neas como mucho (%d no; el peor, %d)", malos, peor);
    chk_cond(q, det, malos == 0);

    /* uno de una línea y uno de dos, de las lecciones con ejemplos */
    for (l = 0; l < N_LECCIONES; l++)
        for (j = 0; j < LECCIONES[l].nej && LECCIONES[l].nmod == 0; j++) {
            int nl = lineas(EJEMPLOS[(int)LECCIONES[l].ej[j]].enunciado, 52);
            if (nl == 1 && corta_l < 0) { corta_l = l; corta_j = j; }
            if (nl == 2 && larga_l < 0) { larga_l = l; larga_j = j; }
        }
    chk_cond(q, "(hay un ejemplo de una l" S_ii "nea y otro de dos)", corta_l >= 0 && larga_l >= 0);
    if (larga_l >= 0) {
        const char *en = EJEMPLOS[(int)LECCIONES[larga_l].ej[larga_j]].enunciado;
        ci_cola(en, cola);
        memcpy(cabeza, en, 20); cabeza[20] = 0;
        ci_trae(larga_l, larga_j);
        chk_cond(q, "[tab] con un enunciado de dos l" S_ii "neas: entero, y a" S_uu "n sin «Tra" S_ii "do»",
                 ui_pantalla() == P_EDITOR && ve(cabeza) && ve(cola) && !ve("Tra" S_ii "do al Taller"));
        r4_vuelca("c_traido_dos");
        TECLAS(T_DER);                                      /* una tecla del editor */
        chk_cond(q, "... la tecla siguiente: «Tra" S_ii "do al Taller: [ctrl][Z] vuelve»",
                 !ve(cola) && ve("Tra" S_ii "do al Taller: [ctrl][Z] vuelve al que ten" S_ii "as."));
        TECLAS(T_TAB);                                      /* una tecla con su aviso (de una línea) */
        chk_cond(q, "... y con la siguiente, se va", !ve("Tra" S_ii "do al Taller"));
    }
    if (corta_l >= 0) {
        const char *en = EJEMPLOS[(int)LECCIONES[corta_l].ej[corta_j]].enunciado;
        ci_trae(corta_l, corta_j);
        chk_cond(q, "[tab] con un enunciado de una l" S_ii "nea: debajo, «Tra" S_ii "do al Taller», como siempre",
                 ui_pantalla() == P_EDITOR && ve(en) && ve("Tra" S_ii "do al Taller: [ctrl][Z] vuelve al que ten" S_ii "as."));
    }
    /* un modelo de dos líneas (la primera lección con modelos) */
    for (l = 0; l < N_LECCIONES && LECCIONES[l].nmod == 0; l++) {}
    if (l < N_LECCIONES) {
        for (j = 0; j < LECCIONES[l].nmod && lineas(MODELOS[(int)LECCIONES[l].mod[j]].que_ver, 52) < 2; j++) {}
        if (j < LECCIONES[l].nmod) {
            ci_cola(MODELOS[(int)LECCIONES[l].mod[j]].que_ver, cola);
            ci_trae(l, j);
            chk_cond(q, "[tab] con un modelo: lo que hay que ver, entero", ui_pantalla() == P_EDITOR && ve(cola));
            r4_vuelca("c_traido_modelo");
        }
    }
    limpia_mis();

    /* la Ayuda: la línea 5 ya no contradice a «EMULAR O CALCULAR» */
    chk_cond(q, "la Ayuda: «5. [=]: con [?!] despeja; sin [?!], calcula, emula»",
             ayuda_tiene("5. [=]: con [?!] despeja; sin [?!], calcula, emula"));
    chk_cond(q, "la Ayuda: ya no dice «con [?!] calcula; si no, emula»", !ayuda_tiene("si no, emula"));
    chk_cond(q, "la Ayuda: [ctrl] con la paleta abierta", ayuda_tiene("ficha o con la paleta abierta, [ctrl] con Z, Y,"));
}

/* ---------------------------------------------------------------------------
 *  C7. La sesión tras «Problema nuevo»
 * ------------------------------------------------------------------------ */
static void ci_sesion(void)
{
    const char *q = "C7 sesi" S_oo "n";
    static Circuito s;
    const Circuito *cu = ui_circuito();
    int h, cx, cy;
    limpia_mis();
    arch_sesion_borrar();
    r2e_nuevo();
    TECLAS(T_LETRA('R'), T_DER, T_DIG(4), T_ENTER);        /* R1 de 4 Ω, sin guardar */
    me_al_taller();
    TECLAS(T_ARRIBA, T_ENTER);                              /* Problema nuevo */
    chk_cond(q, "(Problema nuevo: vac" S_ii "o, y [ctrl][Z] recupera el anterior)",
             ui_pantalla() == P_EDITOR && cu->nel == 0 && ve("[ctrl][Z] recupera el anterior"));
    ui_sesion_guardar();                                    /* salir (main.c) */
    chk_cond(q, "salir con el editor vac" S_ii "o tras Problema nuevo: queda la sesi" S_oo "n, con el de antes",
             me_hay_sesion() && arch_sesion_cargar(&s, &h, &cx, &cy) && s.nel == 1 && s.el[0].val.v == 4.0 && h == -1);
    me_arranca();
    TECLAS(T_ENTER);                                        /* Seguir */
    chk_cond(q, "... y la pr" S_oo "xima vez, Seguir lo trae",
             ui_pantalla() == P_EDITOR && cu->nel == 1 && cu->el[0].val.v == 4.0 && ve("Recuperado de la " S_uu "ltima vez"));

    /* guardado en Mis (hueco 0) y luego Problema nuevo: la sesión con su hueco */
    TECLAS(T_CTRL, T_LETRA('S'));
    chk_cond(q, "(guardado en el hueco 0)", arch_existe(0) && !arch_existe(1));
    me_al_taller();
    TECLAS(T_ARRIBA, T_ENTER);
    ui_sesion_guardar();
    chk_cond(q, "con su hueco: el del archivo", arch_sesion_cargar(&s, &h, &cx, &cy) && s.nel == 1 && h == 0);
    me_arranca();
    TECLAS(T_ENTER, T_LETRA('R'), T_ABAJO, T_ENTER, T_CTRL, T_LETRA('S'));   /* otro cambio y Guardar */
    chk_cond(q, "... y Guardar va a su hueco, no a otro", arch_existe(0) && !arch_existe(1));

    /* sin nada antes: vacío y sin historial, no queda sesión */
    r2e_nuevo();
    ui_sesion_guardar();
    chk_cond(q, "vac" S_ii "o y sin nada que recuperar: no queda sesi" S_oo "n", !me_hay_sesion());
    limpia_mis();
    arch_sesion_borrar();
}

/* ---------------------------------------------------------------------------
 *  C8. El reloj de la capa a la vista
 * ------------------------------------------------------------------------ */
/* La plataforma de main.c con un contador de hz cuentas por segundo (el
 * SP804) y el RTC, los dos del reloj virtual */
static struct { EntReloj r; unsigned base, parado_en; unsigned long hz, salto; int parado; } ci_sp;

static unsigned ci_valor(void)
{
    if (ci_sp.parado) return ci_sp.parado_en;
    return ci_sp.base - (unsigned)((unsigned long long)ce.t_us * ci_sp.hz / 1000000ULL) - (unsigned)ci_sp.salto;
}
static unsigned long ci_rtc_s(void) { return (unsigned long)(ce.t_us / 1000000UL) + 5000UL; }
static unsigned long ci_sp_reloj(Plataforma *pf) { (void)pf; return ent_reloj_ms(&ci_sp.r, ci_valor()); }
static void ci_sp_espera(Plataforma *pf)
{
    unsigned v;
    ce_espera(pf);
    v = ci_sp.r.plan_a ? ci_valor() : 0u;
    ent_reloj_tick(&ci_sp.r, v);
    ent_reloj_rtc(&ci_sp.r, ci_rtc_s(), v);
}
static void ci_sp_abre(unsigned long hz)
{
    unsigned v[RELOJ_CALIBRA + 1];
    int i;
    ce_nueva(CE_W, CE_H, 1);
    memset(&ci_sp, 0, sizeof ci_sp);
    ci_sp.base = 0x7FFFFFFFu;
    ci_sp.hz = hz;
    ce.t_us += 400000UL;                                    /* a mitad de un segundo del RTC */
    ce_espera(&ce.pf);
    for (i = 0; i <= RELOJ_CALIBRA; i++) { if (i) ce_espera(&ce.pf); v[i] = ci_valor(); }
    ent_reloj_abre(&ci_sp.r, v, RELOJ_CALIBRA);
    ce.pf.reloj = ci_sp_reloj;
    ce.pf.espera = ci_sp_espera;
    ce_empieza();
}

static void ci_reloj(void)
{
    const char *q = "C8 reloj a la vista";
    char det[160];
    unsigned long hz, tick_us, idle10;

    cm_menu();
    TECLAS(T_MENU | T_CON_CTRL);
    chk_cond(q, "sin reloj puesto (el arn" S_ee "s), Controles no lo ense" S_nn "a",
             ent_reloj_visto() == 0 && ui_pantalla() == P_CONTROLES && !ve("Reloj de la capa"));
    TECLAS(T_ESC);
    ci_sp_abre(32000UL);                                    /* ¿32 000 en vez de 32 768? */
    ce_pasa(12000);
    ent_reloj_medido(&ci_sp.r, &hz, &tick_us, &idle10);
    printf("    reloj: plan %c, %ld s de RTC; SP804 %lu/s, tick %lu " S_MU "s, idle %lu,%lu/s\n",
           ci_sp.r.plan_a ? 'A' : 'B', (long)ci_sp.r.seg, hz, tick_us, idle10 / 10, idle10 % 10);
    sprintf(det, "un contador a 32 000 Hz: el plan A, y medido contra el RTC %lu/s (" S_PM "0,5 %%)", hz);
    chk_cond(q, det, ci_sp.r.plan_a && hz >= 31840UL && hz <= 32160UL);
    sprintf(det, "el tick de idle(): %lu " S_MU "s (10 250 " S_PM "1 %%)", tick_us);
    chk_cond(q, det, tick_us >= 10148UL && tick_us <= 10352UL);
    sprintf(det, "los idle() por segundo sin trabajo: %lu,%lu (97,6 " S_PM "2 %%)", idle10 / 10, idle10 % 10);
    chk_cond(q, det, idle10 >= 956UL && idle10 <= 996UL);
    /* otro reprograma el contador (un salto de 60 s): se vuelve a medir desde ahí */
    ci_sp.salto += 60UL * ci_sp.hz;
    ce_pasa(8000);
    ent_reloj_medido(&ci_sp.r, &hz, &tick_us, &idle10);
    sprintf(det, "tras un salto del contador, otra vez %lu/s y el tick %lu " S_MU "s (el salto, contado y fuera)", hz, tick_us);
    chk_cond(q, det, ci_sp.r.saltos == 1 && hz >= 31840UL && hz <= 32160UL && tick_us >= 10148UL && tick_us <= 10352UL);
    /* a la vista, en Controles */
    ent_reloj_a_la_vista(&ci_sp.r);
    cm_menu();
    TECLAS(T_MENU | T_CON_CTRL);
    chk_cond(q, "Controles: el plan, los cambios y los saltos",
             ve("Reloj de la capa: plan A; cambios 0, saltos 1"));
    sprintf(det, "SP804 %lu/s; tick %lu " S_MU "s; idle %lu,%lu/s (", hz, tick_us, idle10 / 10, idle10 % 10);
    chk_cond(q, "Controles: las cuentas por segundo, el tick y los idle()", ve(det));
    r4_vuelca("c_controles_reloj");
    TECLAS(T_ESC);
    /* parado: el plan B, un cambio; lo medido del contador se queda */
    ci_sp_abre(32768UL);
    ce_pasa(4000);
    ci_sp.parado_en = ci_valor(); ci_sp.parado = 1;
    ce_pasa(3000);
    ent_reloj_a_la_vista(&ci_sp.r);
    cm_menu();
    TECLAS(T_MENU | T_CON_CTRL);
    ent_reloj_medido(&ci_sp.r, &hz, &tick_us, &idle10);
    chk_cond(q, "con el contador parado: «plan B; cambios 1», y lo que midi" S_oo " antes",
             ve("Reloj de la capa: plan B; cambios 1, saltos 0") && hz >= 32600UL && hz <= 32940UL);
    r4_vuelca("c_controles_plan_b");
    TECLAS(T_ESC);
    ent_reloj_a_la_vista(0);
    ce.pf.reloj = ce_reloj; ce.pf.espera = ce_espera;
}

/* ---------------------------------------------------------------------------
 *  C9. El touchpad a la vista: el tipo que vio main.c (Synaptics o
 *  CapTIvate, por el envoltorio de Ndless) y su W×H, para calibrar la capa
 * ------------------------------------------------------------------------ */
static void ci_pad(void)
{
    const char *q = "C9 touchpad a la vista";
    char det[96];
    int w, h;

    cm_menu();
    TECLAS(T_MENU | T_CON_CTRL);
    chk_cond(q, "sin touchpad puesto (el arn" S_ee "s), Controles no lo ense" S_nn "a",
             ent_pad_tipo(&w, &h) < 0 && ui_pantalla() == P_CONTROLES && !ve("Touchpad:"));
    TECLAS(T_ESC);
    ent_pad_usa(&w, &h);
    chk_cond(q, "la capa trabaja con el de omisi" S_oo "n (el Synaptics, 2328x1691)", w == TP_W_OMISION && h == TP_H_OMISION);
    ent_pad_visto(ENT_TP_CAPTIVATE, 0, 0);                  /* el CapTIvate no dice su tamaño */
    cm_menu();
    TECLAS(T_MENU | T_CON_CTRL);
    sprintf(det, "Touchpad: CapTIvate, sin tama" S_nn "o (usa %dx%d)", TP_W_OMISION, TP_H_OMISION);
    chk_cond(q, "CapTIvate sin tama" S_nn "o: el tipo y el de omisi" S_oo "n que usa la capa", ve(det));
    r4_vuelca("c_controles_pad");
    TECLAS(T_ESC);
    ent_pad_visto(ENT_TP_CAPTIVATE, 16384, 16384);
    cm_menu();
    TECLAS(T_MENU | T_CON_CTRL);
    chk_cond(q, "un W" S_TIMES "H que la capa no usar" S_ii "a: los dos, en 52 columnas",
             ve("Touchpad: CapTIvate, 16384x16384 (usa 2328x1691)") && gfx_text_w("Touchpad: CapTIvate, 16384x16384 (usa 2328x1691)", 1) <= SCR_W - 16);
    TECLAS(T_ESC);
    ent_pad_visto(ENT_TP_SYNAPTICS, TP_W_OMISION, TP_H_OMISION);
    cm_menu();
    TECLAS(T_MENU | T_CON_CTRL);
    chk_cond(q, "el Synaptics con su tama" S_nn "o: sin «usa»", ve("Touchpad: Synaptics, 2328x1691") && !ve("(usa"));
    TECLAS(T_ESC);
    ent_pad_visto(ENT_TP_NINGUNO, 0, 0);
    cm_menu();
    TECLAS(T_MENU | T_CON_CTRL);
    chk_cond(q, "sin touchpad (la clickpad): «no hay»", ve("Touchpad: no hay"));
    TECLAS(T_ESC);
    ent_pad_visto(-1, 0, 0);                                /* como antes: el arnés no tiene pad */
}

static void test_cierre(void)
{
    printf("[73] El cierre de la Etapa 4: [ctrl], la paleta, el grabador, los textos, la sesi" S_oo "n, el reloj y el touchpad\n");
    gfx_desbordes_reset();
    ESP_CI_C1
    ci_ctrl_menu();
    ci_paleta();
    ESP_CI_C4
    ci_grabador();
    ci_textos();
    ci_sesion();
    ci_reloj();
    ci_pad();
    pruebas++;
    if (gfx_desbordes() != 0) falla("cierre", "textos que se salen de la pantalla", gfx_desbordes(), 0);
    ce_listo = 0;                                           /* capa_tecla abre otra */
}
