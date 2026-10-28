/* ============================================================================
 *  test_opt_final.inc  -  [75i] La integración de la fase 1 de rev9: lo que
 *  encontraron las revisiones de los grupos y se arregló al unirlos. Cada
 *  prueba falla sin su arreglo (las mutaciones de rev9/int_herr).
 *
 *    I1  turbo, el SO que tarda: bajar a 396 lo espera más (0,5 s; si ni así,
 *        sin confirmar: atascado, I20), y atascado se reintenta (al
 *        escribir, al salir, en cada acción); la sesión siguiente no
 *        escribe con el reloj arriba
 *    I2  turbo, de quién es el reloj: del SO solo con M ≤ 33; arriba con otros
 *        bits, atascado y sin escribir; lo que Hakaru sabe bajar, a 396
 *    I3  turbo, las teclas: las IRQ no se abren con una tecla abajo (con tope)
 *    I4  turbo, la prueba que no puede medir el reloj a 396: el nivel no queda
 *        fallido, y una prueba superada le quita el fallido
 *    I5  turbo, la marca de cuelgue: fuera en cuanto el turbo se apaga por otro
 *        camino (el USB, la puerta, [P] sin memoria, la referencia a 396)
 *    I6  turbo, el [esc] de la medida del reloj: cancela y sigue en el panel
 *    I7  turbo, la puerta que falla a mitad de la prueba: su motivo, no «el SO»
 *    I8  turbo, atascado a mitad de la prueba: la marca de la barra sigue
 *    I9  turbo, la FNV de la prueba: un bit de un exp() se ve
 *    I10 turbo, el mono del panel con invariantes (el de la revisión)
 *    I11 el control del gasto del emulador (la revisión de cal): la primera
 *        mirada no corta (caso_244 de [67]: el plan valía)
 *    I12 ...pedir más ventana no da menos (el puente del banco)
 *    I13 ...lo caro que llega tras la última 1-2-5 también se acorta (la
 *        rejilla de la década de abajo)
 *    I14 ...y el aviso dice que se acortó la ventana, no solo que se agrandó
 *        el paso (el puente con 1 s: las dos cosas, I28)
 *    I15 gfx_fila_pares (la revisión de dibujo): nunca fuera de la pantalla
 *    I16 lo visto de paso en el cierre: las medidas del osciloscopio por
 *        debajo de la millonésima de división, 0 («-0,01pV» a 5 V/div)
 *    I17 ...y la banderita de la sonda de nodo no tapa el rótulo de al lado
 *    I18 la segunda página del banco (grupo otros) con el turbo: los ciclos a
 *        los MHz de la medida; si cambian a mitad, se dice
 *    I19 el estado de Newton de antes del paso (la revisión de motor): a
 *        medias, lo mismo que entero (el arnés al azar, test_opt_fuzz.inc)
 *  La revisión final (dos revisores: el turbo y el control del gasto):
 *    I20 turbo: nunca se escribe con el pendiente a 1, y lo que la PMU no
 *        confirma cuenta como atascado (la puerta no escribe) hasta que se
 *        limpia; su mono (el SO que deja de atender, la PMU que no toma lo
 *        escrito con el pendiente a 1)
 *    I21 turbo: [P] mide la CPU (el registro puede cambiar sin ella)
 *    I22 turbo: el [esc] de la medida a M no se come el siguiente
 *    I23 turbo: la puerta no escribe con un reloj que no sabe calcular
 *    I24 el gasto: la rejilla de reserva, de la escala de lo emulado
 *    I25 ...pedir más ventana (el puente, de 0,3 a 1,1 s)
 *    I26 ...el corte, con el ritmo de todo el tramo (caso_258 y las 60
 *        redes de [67] en modo calculadora)
 *    I27 ...con periodos, la ventana de medida entera (la cota de verdad)
 *    I28 ...el aviso PASO_LIM: la ventana, el paso o los dos
 *    I29 ...las operaciones no dependen de lo emulado antes
 *    I30 ...el presupuesto de sim_avanzar y bode_avanzar, por diferencia
 *        (el contador de 32 bits de la calculadora da la vuelta)
 *
 *  Va la última de la batería. No vuelca BMP; sus emulaciones (I11-I14 e
 *  I24-I30) van al final de las huellas. Deja el simulador del turbo como estaba («no
 *  es una CX II»).
 * ========================================================================= */

static int ti_seccion(unsigned char *b)
{
    int n = arch_bytes_leer("/ces_controles.tns", b, 20);
    return n >= 12 && b[8] == 'T' && b[9] >= 1;
}
static int ti_marca_arch(void) { unsigned char b[20]; return ti_seccion(b) ? b[10] : 0; }
static int ti_fallidos_arch(void) { unsigned char b[20]; return ti_seccion(b) ? b[11] : 0; }

/* Un reset (un cuelgue, las pilas): el boot1 pone 396 y el programa arranca
 * de nuevo, como en main (ui_init y ui_controles_abrir) */
static void ti_rearranca(void)
{
    turbo_sim_reinicia();
    tb_s = turbo_sim();
    ui_init();
    ui_controles_abrir();
}

/* Otro programa (el mismo Hakaru, relanzado) sin apagar: el registro sigue
 * como estaba y todo lo de turbo.c, como al arrancar */
static void ti_relanza(void)
{
    TurboSim s = *turbo_sim();
    turbo_sim_reinicia();
    tb_s = turbo_sim();
    tb_s->pll = s.pll; tb_s->pend = s.pend;
    ui_init();
    ui_controles_abrir();
}

/* ---------------------------------------------------------------------------
 *  I1. El SO que tarda en atender INT_POWER
 * ------------------------------------------------------------------------ */
static void ti_so_tarda(void)
{
    const char *q = "[75i] I1 el SO que tarda";
    unsigned char b[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    int r;

    /* ~43 ms en atender la 1.ª escritura (1400 lecturas; 655 por ventana):
     * subir falla, y la bajada espera más y vuelve a 396 (antes: atascado a
     * 408 para siempre, y se salía de Hakaru a 408) */
    tb_limpio();
    tb_s->atiende_1 = 1400;
    r = turbo_pon(34);
    chk_cond(q, "el SO tarda ~43 ms: la PMU no confirm" S_oo " a tiempo, y la bajada s" S_ii " espera: a 396, sin atascar",
             r == TBM_PENDIENTE && !turbo_atascado() && turbo_nivel() == 0 && tb_s->pll == TURBO_SERIE &&
             !strcmp(tb_ms(), "34 33") && turbo_marca() == 0);

    /* a 432, el SO deja de atender: 35 queda sin confirmar y no se escribe
     * nada más encima (la revisión final: antes bajaba igual, con el
     * pendiente a 1, y se daba por bueno); atascado */
    tb_limpio();
    turbo_pon(36);
    tb_s->atiende = -1;
    tb_s->nesc = 0;
    r = turbo_pon(0);
    chk_cond(q, "a 432 el SO deja de atender: 35, sin confirmar, y nada encima: atascado", r == TBM_PENDIENTE &&
             turbo_atascado() && !strcmp(tb_ms(), "35") && tb_s->pll == 0x23020303u && turbo_nivel() == 35);

    /* atascado se reintenta: en la puerta y al salir */
    tb_limpio();
    turbo_pon(34);
    tb_s->pega_m = TURBO_M_SERIE;
    turbo_pon(0);
    tb_s->pega_m = 0;
    tb_s->narch = 0;
    r = arch_bytes_guardar("/ces_tb_prueba.tns", b, 8);
    chk_cond(q, "atascado, una escritura lo reintenta: baja y escribe a 396",
             r && !turbo_atascado() && turbo_nivel() == 0 && tb_s->pll == TURBO_SERIE && tb_s->narch >= 1 &&
             tb_s->arch[tb_s->narch - 1].m == TURBO_M_SERIE);
    arch_bytes_borrar("/ces_tb_prueba.tns");
    tb_limpio();
    turbo_pon(34);
    tb_s->pega_m = TURBO_M_SERIE;
    turbo_pon(0);
    tb_s->pega_m = 0;
    turbo_sale();
    chk_cond(q, "atascado, al salir se reintenta: sale a 396 y sin la marca", !turbo_atascado() && tb_s->pll == TURBO_SERIE &&
             ti_marca_arch() == 0);

    /* sale atascado a 408 (la PMU no toma el 33) y se relanza sin apagar:
     * antes de escribir nada (la marca que quedó, al arrancar), a 396 */
    tb_limpio();
    turbo_pon(34);
    tb_s->pega_m = TURBO_M_SERIE;
    turbo_pon(0);
    turbo_sale();
    chk_cond(q, "sale atascado a 408, con la marca escrita", turbo_atascado() && tb_s->pll == 0x22020303u && ti_marca_arch() == 34);
    tb_s->pega_m = 0;
    ti_relanza();
    chk_cond(q, "relanzado sin apagar: el cuelgue de 408 se dice, y ces_controles.tns se escribe ya a 396",
             turbo_fallidos() == 1u && tb_s->pll == TURBO_SERIE && tb_s->narch >= 1 && tb_s->arch[0].m == TURBO_M_SERIE &&
             ti_marca_arch() == 0 && turbo_motivo() == TBM_AJENO);
    if (ui_pantalla() == P_CONTROLES) TECLAS(T_ENTER);
    turbo_sale();
}

/* ---------------------------------------------------------------------------
 *  I2. De quién es el reloj
 * ------------------------------------------------------------------------ */
static void ti_de_quien(void)
{
    const char *q = "[75i] I2 de qui" S_ee "n es el reloj";
    unsigned char b[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    int r, n0;

    /* a 432, un bit bajo cambia (no M): no es del SO; no se sabe bajar */
    tb_limpio();
    turbo_pon(36);
    tb_s->pll |= 0x4u;
    n0 = tb_s->nesc;
    ui_rotulo();
    tb_s->narch = 0;
    r = arch_bytes_guardar("/ces_tb_prueba.tns", b, 8);
    chk_cond(q, "a 432 cambia un bit bajo: atascado a 432, sin escribir ni la PMU ni archivos (antes: \"del SO\" y a 432)",
             turbo_atascado() && turbo_nivel() == 36 && !r && tb_s->narch == 0 && tb_s->nesc == n0 &&
             turbo_motivo() == TBM_RARO && turbo_mhz_marca() == 432);
    turbo_sale();
    chk_cond(q, "...y al salir tampoco se toca (ni la marca, que se queda)", tb_s->pll == 0x24020307u && tb_s->nesc == n0 &&
             ti_marca_arch() == 36);
    tb_s->pll = 0x18020303u;                                /* el SO pone lo suyo: se acabó */
    ui_rotulo();
    chk_cond(q, "cuando el SO pone lo suyo (288), se acaba el atasco y la marca se va", !turbo_atascado() &&
             turbo_nivel() == 0 && ti_marca_arch() == 0);

    /* a 432 cambia D1 (a 1): atascado, y la marca dice la frecuencia de verdad */
    tb_limpio();
    turbo_pon(36);
    tb_s->pll = 0x24010303u;
    ui_rotulo();
    cm_menu();
    chk_cond(q, "D1 cambia a 1 (864 MHz): atascado, y la marca de la barra dice 864 MHz, no 432",
             turbo_atascado() && turbo_mhz_marca() == 864 && ve("864 MHz") && !ve("432 MHz"));
    tb_s->pll = TURBO_SERIE;
    ui_rotulo();
    TECLAS(T_ESC);

    /* con el aviso, a 456 la PMU deja 39: no es «del SO» (M > 33) */
    tb_limpio();
    turbo_alto(1);
    tb_s->sube_m = 38;
    r = turbo_pon(38);
    tb_s->narch = 0;
    chk_cond(q, "a 456 la PMU deja 39 (468 MHz): no se da por del SO; se intenta bajar y queda atascado, con su marca",
             r == TBM_LECTURA && turbo_atascado() && tb_s->pll == 0x27020303u && !arch_bytes_guardar("/ces_tb_prueba.tns", b, 8) &&
             tb_s->narch == 0 && turbo_mhz_marca() == 468 && tb_escrituras_bien(0));
    tb_s->sube_m = 0;
    turbo_sale();
    chk_cond(q, "...y en cuanto la PMU obedece, al salir, de 39 a 396", !turbo_atascado() && tb_s->pll == TURBO_SERIE &&
             ti_marca_arch() == 0);

    /* a 432, el registro pasa a otro nivel de Hakaru: se baja a 396 */
    tb_limpio();
    turbo_pon(36);
    tb_s->pll = 0x23020303u;
    ui_rotulo();
    chk_cond(q, "a 432 aparece 420 (con los bits de Hakaru): se baja a 396 y se dice", turbo_nivel() == 0 && !turbo_atascado() &&
             tb_s->pll == TURBO_SERIE && turbo_motivo() == TBM_LECTURA && ti_marca_arch() == 0);

    /* el reloj arriba sin el turbo (otro programa que cambió solo M): la
     * puerta baja antes de escribir; con otros bits, no escribe */
    tb_limpio();
    tb_s->pll = 0x23020303u;
    tb_s->narch = 0;
    r = arch_bytes_guardar("/ces_tb_prueba.tns", b, 8);
    chk_cond(q, "a 420 sin el turbo: la puerta baja a 396 antes de escribir", r && tb_s->pll == TURBO_SERIE &&
             tb_s->narch == 1 && tb_s->arch[0].m == TURBO_M_SERIE && turbo_motivo() == TBM_AJENO);
    tb_s->pll = 0x23020307u;
    tb_s->narch = 0;
    r = arch_bytes_guardar("/ces_tb_prueba.tns", b, 8);
    chk_cond(q, "a 420 con otros bits: no se toca ni se escribe", !r && tb_s->narch == 0 && tb_s->pll == 0x23020307u &&
             turbo_motivo() == TBM_RARO);
    tb_s->pll = TURBO_SERIE;
    arch_bytes_borrar("/ces_tb_prueba.tns");
}

/* ---------------------------------------------------------------------------
 *  I3. Las teclas
 * ------------------------------------------------------------------------ */
static void ti_teclas(void)
{
    const char *q = "[75i] I3 teclas";
    int r;
    tb_limpio();
    tb_s->tecla = 50;                                       /* la tecla que pidió el cambio, aún abajo */
    r = turbo_pon(34);
    chk_cond(q, "con la tecla abajo, las IRQ se abren al soltarla (el SO no la lee)", r == TBM_OK && turbo_nivel() == 34 &&
             tb_s->abre_con_tecla == 0 && tb_s->tecla == 0);
    turbo_sale();
    tb_limpio();
    tb_s->tecla = -1;                                       /* una tecla atascada */
    r = turbo_pon(34);
    turbo_sale();
    chk_cond(q, "una tecla atascada no para nada (con tope): sube y vuelve a 396", r == TBM_OK && tb_s->pll == TURBO_SERIE &&
             tb_s->abre_con_tecla > 0);
}

/* ---------------------------------------------------------------------------
 *  I4. La prueba que no puede medir el reloj a 396 (un RTC parado)
 * ------------------------------------------------------------------------ */
static unsigned ti_pll = TURBO_SERIE, ti_t;
static int ti_irq_ab;
static int      ti_cx2(void) { return 1; }
static unsigned ti_lee(unsigned d) { return d == TURBO_R_PLL ? ti_pll : d == TURBO_R_FUENTE ? 0x11u : 0u; }
static void     ti_pon(unsigned v) { ti_pll = v; }
static int      ti_irq(int m) { int a = ti_irq_ab ? 0 : 0xC0; ti_irq_ab = m == 0; return a; }
static unsigned ti_t32(void) { return ti_t += 1; }
static unsigned ti_rtc(void) { return 1234u; }            /* no avanza */
static int      ti_tecla(void) { return 0; }
static const TurboHw ti_hw = { ti_cx2, ti_lee, ti_pon, ti_irq, ti_t32, ti_rtc, ti_tecla };

static void ti_sin_reloj(void)
{
    const char *q = "[75i] I4 sin reloj a 396";
    TurboPrueba p;
    long i;
    tb_limpio();
    turbo_hw(&ti_hw);
    ti_pll = TURBO_SERIE; ti_t = 0; ti_irq_ab = 0;
    tb_panel();
    TECLAS(T_DER, T_ENTER);                               /* 408 */
    TECLAS(T_LETRA('P'));
    for (i = 0; i < 1000 && ui_animando() == UI_TRABAJANDO; i++) ui_tick();
    turbo_prueba_estado(&p);
    chk_cond(q, "el RTC no avanza: la prueba para a 396 y lo dice, sin el \"cambi" S_oo " con la CPU\"",
             p.fase == TBP_MAL && p.motivo == TBM_SIN_RELOJ && ti_pll == TURBO_SERIE && ve("No se pudo medir el reloj a 396") &&
             !ve("cambi" S_oo " con la CPU"));
    chk_cond(q, "...y 408 no queda fallido, ni en memoria ni en ces_controles.tns",
             turbo_fallidos() == 0 && ti_fallidos_arch() == 0);
    TECLAS(T_ESC);
    turbo_hw(0);
    ti_rearranca();
    tb_panel();
    TECLAS(T_DER, T_ENTER);
    chk_cond(q, "otra sesi" S_oo "n: 408 no pide el aviso de \"ya se colg" S_oo "\"", !ve("ya se colg") && turbo_nivel() == 34);
    TECLAS(T_ESC);
    turbo_sale();

    /* un nivel que falló y luego pasa [P]: ya no es fallido */
    tb_limpio();
    turbo_recupera(0, 1u);                                /* 408, de otra vez */
    tb_panel();
    TECLAS(T_DER, T_ENTER, T_ENTER);                      /* el aviso de activar y el del fallido */
    TECLAS(T_LETRA('P'));
    for (i = 0; i < 100000L && ui_animando() == UI_TRABAJANDO; i++) ui_tick();
    turbo_prueba_estado(&p);
    chk_cond(q, "un nivel fallido que pasa [P] deja de serlo", p.fase == TBP_BIEN && turbo_nivel() == 34 && turbo_fallidos() == 0);
    TECLAS(T_ESC);
    turbo_sale();
    chk_cond(q, "...tambi" S_ee "n en ces_controles.tns, al salir", ti_fallidos_arch() == 0 && ti_marca_arch() == 0);
}

/* ---------------------------------------------------------------------------
 *  I5. La marca de cuelgue cuando el turbo se apaga por otro camino
 * ------------------------------------------------------------------------ */
static void ti_marca_caso(const char *q, const char *que, int caso)
{
    char det[240];
    unsigned char b4[4] = { 1, 2, 3, 4 };
    int m_arch, colgo;
    tb_limpio();
    turbo_pon(36);                                          /* 432, marca 36 */
    switch (caso) {
    case 0: tb_s->pll = 0x18020303u; break;                 /* el USB */
    case 1: tb_s->sin_mem = 1; turbo_prueba(36); break;     /* [P] sin memoria */
    case 2: turbo_pausa(); tb_s->pll = 0x18020303u; turbo_sigue(); break;   /* el USB mientras se escribe */
    case 3: tb_s->mal_m = 35; arch_bytes_guardar("/ces_tb_prueba.tns", b4, 4); tb_s->mal_m = 0; break;   /* la puerta, bajando */
    case 4: turbo_pausa(); tb_s->mal_m = 35; turbo_sigue(); tb_s->mal_m = 0; break;   /* la puerta, subiendo */
    default: turbo_prueba(36); turbo_prueba_paso(); turbo_prueba_paso(); break;       /* la referencia, a 396 */
    }
    ui_rotulo();                                            /* la vuelta del bucle */
    m_arch = ti_marca_arch();
    sprintf(det, "%s: apagado (%d MHz) y sin la marca en ces_controles.tns (%d)", que, turbo_mhz(), m_arch);
    chk_cond(q, det, caso == 5 ? m_arch == 0 : turbo_nivel() == 0 && turbo_pedido() == 0 && m_arch == 0);
    ti_rearranca();                                         /* un reset luego, a 396 o 288 */
    colgo = ve("se colg");
    sprintf(det, "%s, y un reset luego: no dice que se colg" S_oo " ni marca 432 (0x%X)", que, turbo_fallidos());
    chk_cond(q, det, !colgo && !(turbo_fallidos() & 4u));
    if (ui_pantalla() == P_CONTROLES) TECLAS(T_ENTER);
    arch_bytes_borrar("/ces_tb_prueba.tns");
}

static void ti_marca(void)
{
    const char *q = "[75i] I5 marca";
    ti_marca_caso(q, "el USB a 432", 0);
    ti_marca_caso(q, "[P] a 432 sin memoria", 1);
    ti_marca_caso(q, "el USB durante una escritura", 2);
    ti_marca_caso(q, "la puerta no baja limpio", 3);
    ti_marca_caso(q, "la puerta no vuelve a subir", 4);
    ti_marca_caso(q, "[P] a 432, en la referencia a 396", 5);
    tb_limpio();
    tb_panel();
    tb_s->sin_mem = 1;
    TECLAS(T_DER, T_ENTER, T_LETRA('P'));
    chk_cond(q, "[P] sin memoria dice que el turbo qued" S_oo " apagado", ve("No hay memoria para el patr" S_oo "n de la prueba: turbo") &&
             ve("apagado.") && turbo_nivel() == 0);
    TECLAS(T_ESC);
    turbo_sale();
}

/* ---------------------------------------------------------------------------
 *  I6. [esc] mientras la prueba mide el reloj (cuenta_segundo bloquea y mira
 *      turbo_salida); por el bucle de verdad (la mano de test_entrada.inc)
 * ------------------------------------------------------------------------ */
static int ti_armado, ti_vistas;
static int ti_salir(void)
{
    if (!ti_armado) return ti_vistas > 0 && ce.tf[TF_ESC];   /* sigue abajo hasta ce_arriba */
    ti_armado = 0;
    ti_vistas++;
    ce.tf[TF_ESC] = 1;                                      /* el usuario aprieta [esc] */
    return 1;
}

static void ti_esc_medida(void)
{
    const char *q = "[75i] I6 [esc] en la medida";
    TurboPrueba p;
    tb_limpio();
    tb_panel();
    TECLAS(T_DER, T_ENTER);                                 /* 408 */
    ce_abre(CE_W, CE_H, 0);
    turbo_salida(ti_salir);
    ti_armado = 1; ti_vistas = 0;
    ce_pulsa(TF_LETRA('P'));                                /* la 1.ª medida, a 396 */
    ce_pasa(120);
    ce_arriba(TF_ESC);
    ce_pasa(200);
    turbo_salida(0);
    turbo_prueba_estado(&p);
    chk_cond(q, "[esc] en la medida del reloj: cancela y sigue en el panel con \"Prueba cancelada\"",
             ti_vistas == 1 && p.motivo == TBM_CANCELADA && ui_pantalla() == P_CONTROLES && ve("Prueba cancelada"));
    TECLAS(T_ESC);
    chk_cond(q, "...y el [esc] siguiente s" S_ii " sale", ui_pantalla() != P_CONTROLES);
    turbo_sale();
}

/* ---------------------------------------------------------------------------
 *  I7 e I8. La prueba a 432 y la puerta de una escritura de fondo (el
 *      grabador sigue en cualquier pantalla)
 * ------------------------------------------------------------------------ */
static void ti_a_la_pasada(void)
{
    TurboPrueba p;
    long i;
    tb_limpio();
    tb_panel();
    TECLAS(T_DER, T_ENTER, T_DER, T_DER, T_LETRA('P'));     /* 432 y [P] */
    for (i = 0; i < 100000L; i++) {
        ui_tick();
        turbo_prueba_estado(&p);
        if (p.fase == TBP_TURBO && p.pasadas == 1) break;
    }
}

static void ti_prueba_puerta(void)
{
    const char *q = "[75i] I7 prueba y puerta";
    unsigned char b4[4] = { 1, 2, 3, 4 };
    TurboPrueba p;
    long i;
    ti_a_la_pasada();
    tb_s->mal_m = 35;                                       /* al bajar de 432 a 420, se lee 408 */
    arch_bytes_guardar("/ces_tb_prueba.tns", b4, 4);
    tb_s->mal_m = 0;
    for (i = 0; i < 100000L && ui_animando() == UI_TRABAJANDO; i++) ui_tick();
    turbo_prueba_estado(&p);
    chk_cond(q, "la PMU no cuadr" S_oo " al bajar para escribir: la prueba lo dice as" S_ii ", no \"el SO\"",
             p.motivo == TBM_LECTURA && ve("El reloj no qued" S_oo " como se escribi" S_oo) && !ve("El SO cambi" S_oo " el reloj") &&
             tb_s->pll == TURBO_SERIE);
    TECLAS(T_ESC);
    turbo_sale();
    arch_bytes_borrar("/ces_tb_prueba.tns");

    q = "[75i] I8 prueba atascada";
    ti_a_la_pasada();
    tb_s->pega_m = 34;                                      /* de 420 a 408, la PMU no lo toma */
    arch_bytes_guardar("/ces_tb_prueba.tns", b4, 4);
    for (i = 0; i < 100000L && ui_animando() == UI_TRABAJANDO; i++) ui_tick();
    TECLAS(T_ESC, T_ESC);                                   /* fuera del panel */
    chk_cond(q, "atascada a 420 en la prueba: el nivel y la marca de la barra lo siguen diciendo",
             turbo_atascado() && turbo_nivel() == 35 && tb_s->pll == 0x23020303u && ve("420 MHz"));
    tb_s->pega_m = 0;
    turbo_sale();
    chk_cond(q, "...y al salir, a 396", !turbo_atascado() && tb_s->pll == TURBO_SERIE);
    arch_bytes_borrar("/ces_tb_prueba.tns");

    ti_a_la_pasada();
    tb_s->pega_m = 35;                                      /* de 432 a 420, la PMU no lo toma: sigue a 432 */
    arch_bytes_guardar("/ces_tb_prueba.tns", b4, 4);
    for (i = 0; i < 100000L && ui_animando() == UI_TRABAJANDO; i++) ui_tick();
    turbo_prueba_estado(&p);
    chk_cond(q, "atascada en su nivel (432): la prueba se para igual, y la marca sigue",
             p.fase == TBP_MAL && turbo_atascado() && tb_s->pll == 0x24020303u && ve("432 MHz") && ve("No se pudo volver a 396"));
    TECLAS(T_ESC);
    tb_s->pega_m = 0;
    turbo_sale();
    chk_cond(q, "...y al salir, a 396", !turbo_atascado() && tb_s->pll == TURBO_SERIE);
    arch_bytes_borrar("/ces_tb_prueba.tns");
}

/* ---------------------------------------------------------------------------
 *  I9. La FNV de la prueba: un bit de un exp() en una pasada a M
 * ------------------------------------------------------------------------ */
static void ti_fnv(void)
{
    const char *q = "[75i] I9 FNV";
    TurboPrueba p;
    int d, bien = 1;
    char det[160];
    for (d = 0; d < 2; d++) {
        tb_limpio();
        tb_s->falla_pasada = 1; tb_s->falla_donde = d; tb_s->falla_bit = 0;
        turbo_prueba(34);
        tb_prueba_entera();
        turbo_prueba_estado(&p);
        if (!(p.fase == TBP_MAL && p.motivo == TBM_PRUEBA && p.pasadas == 0)) {
            bien = 0;
            sprintf(det, "el bit 0 de %s: no se vio (fase %d, motivo %d)", d ? "un exp()" : "la 1.a soluci" S_oo "n de Newton",
                    p.fase, p.motivo);
            chk_cond(q, det, 0);
        }
        turbo_sale();
    }
    chk_cond(q, "un bit bajo de la 1.a soluci" S_oo "n de Newton o de un exp() da otra FNV (la suma se lo com" S_ii "a)", bien);
}

/* ---------------------------------------------------------------------------
 *  I10. El mono del turbo (el de la revisión «caminos»): teclas al azar en el
 *       panel de una CX II simulada, ticks de la prueba, escrituras de fondo,
 *       el USB, lecturas raras, la PMU que no toma un valor, cuelgues y
 *       salidas; con ui_rotulo tras cada paso, como el bucle
 * ------------------------------------------------------------------------ */
static unsigned ti_lcg = 20260928u;
static unsigned ti_azar(unsigned n) { ti_lcg = ti_lcg * 1103515245u + 12345u; return (ti_lcg >> 16) % n; }

static void ti_mono(void)
{
    static const int K[] = { T_IZQ, T_DER, T_DER, T_DER, T_ENTER, T_ENTER, T_ESC, T_LETRA('P'), T_LETRA('P'),
                             T_ARRIBA, T_ABAJO, T_MENU };
    const char *q = "[75i] I10 mono del turbo";
    char det[200];
    unsigned char b4[4] = { 9, 8, 7, 6 };
    long paso, N = getenv("HAKARU_TI_MONO") ? atol(getenv("HAKARU_TI_MONO")) : 20000L;
    long v_arch = 0, v_esc = 0, v_marca = 0, v_nivel = 0, v_arranque = 0, v_salida = 0, n_falso = 0;
    long n_cuelgue = 0, n_arriba = 0, n_salida = 0, n_atasco = 0;
    int i, prevM = TURBO_M_SERIE, cadena = 1;
    tb_limpio();
    for (paso = 0; paso < N; paso++) {
        int a = (int)ti_azar(1000), M, n;
        if (ui_pantalla() != P_CONTROLES) {
            if (ui_pantalla() == P_INICIO) TECLAS(T_ENTER);
            TECLAS(T_MENU | T_CON_CTRL);
            if (ui_pantalla() == P_CONTROLES && ti_azar(3)) TECLAS(T_ABAJO, T_ABAJO, T_ABAJO);
        } else if (a < 450) { int k = K[ti_azar(sizeof K / sizeof K[0])]; teclas(&k, 1); }
        else if (a < 800) { int j, nt = 1 + (int)ti_azar(300); for (j = 0; j < nt; j++) ui_tick(); }
        else if (a < 900) { if (ti_azar(2)) arch_bytes_guardar("/ces_tb_prueba.tns", b4, 4); else arch_bytes_borrar("/ces_tb_prueba.tns"); }
        else if (a < 915) tb_s->mal_m = ti_azar(3) ? 0 : 34 + (int)ti_azar(5);
        else if (a < 925) tb_s->pega_m = ti_azar(4) ? 0 : 33 + (int)ti_azar(6);
        else if (a < 932) tb_s->pll = 0x18020303u;          /* el USB */
        else if (a < 960) {                                 /* un cuelgue o un reset, aquí */
            int f0 = ti_fallidos_arch(), m_arch = ti_marca_arch();
            M = (int)(tb_s->pll >> 24);
            n_cuelgue++;
            ti_rearranca();
            if (M > TURBO_M_SERIE) {
                n_arriba++;
                if (m_arch < M || !(turbo_fallidos() & (1u << (m_arch - TURBO_M_MIN)))) v_arranque++;
            } else if ((unsigned)turbo_fallidos() != (unsigned)f0) n_falso++;
            if (ui_pantalla() == P_CONTROLES) TECLAS(T_ENTER);
            if (ti_azar(4) == 0) { arch_bytes_borrar("/ces_controles.tns"); ti_rearranca(); }
            tb_s->nesc = tb_s->narch = 0;
            prevM = TURBO_M_SERIE; cadena = 1;
            continue;
        } else {                                            /* salir bien y volver a abrir */
            int f0;
            n_salida++;
            tb_s->mal_m = 0; tb_s->pega_m = 0;             /* la PMU, sana: ha de salir a 396 */
            turbo_sale();
            if ((tb_s->pll >> 24) > TURBO_M_SERIE || ti_marca_arch() != 0 || turbo_atascado()) v_salida++;
            f0 = ti_fallidos_arch();
            ti_rearranca();
            if ((unsigned)turbo_fallidos() != (unsigned)f0 || ui_pantalla() == P_CONTROLES) v_salida++;
            tb_s->nesc = tb_s->narch = 0;
            prevM = TURBO_M_SERIE; cadena = 1;
            continue;
        }
        ui_rotulo();                                        /* la vuelta del bucle */
        n = tb_s->narch < 32 ? tb_s->narch : 32;
        for (i = 0; i < n; i++) if (tb_s->arch[i].m > TURBO_M_SERIE) v_arch++;
        n = tb_s->nesc < 64 ? tb_s->nesc : 64;
        for (i = 0; i < n; i++) {
            const TurboEsc *e = &tb_s->esc[i];
            int m = (int)(e->v >> 24);
            if (e->dir != TURBO_R_PLL || (e->v & 0x00FFFFFFu) != (TURBO_SERIE & 0x00FFFFFFu) || m < TURBO_M_SERIE ||
                m > TURBO_M_MAX || e->irq_abiertas || (cadena && m != prevM + 1 && m != prevM - 1 && m != prevM)) v_esc++;
            prevM = m;
        }
        tb_s->narch = tb_s->nesc = 0;
        M = (int)(tb_s->pll >> 24);
        prevM = M;
        cadena = tb_s->mal_m == 0 && tb_s->pega_m == 0;
        if (M > TURBO_M_SERIE && ti_marca_arch() < M) v_marca++;
        if (turbo_atascado()) n_atasco++;
        if (M > TURBO_M_SERIE && turbo_nivel() != M) v_nivel++;
    }
    sprintf(det, "%ld pasos: %ld cuelgues (%ld arriba), %ld salidas, %ld pasos atascado", N, n_cuelgue, n_arriba, n_salida, n_atasco);
    printf("  %s\n", det);
    chk_cond(q, det, n_cuelgue > 0 && n_arriba > 0 && n_salida > 0 && n_atasco > 0);
    sprintf(det, "cada archivo, a 396 o menos (%ld fuera)", v_arch);
    chk_cond(q, det, v_arch == 0);
    sprintf(det, "cada escritura de la PMU: 0x2X020303, M en 33..38, de uno en uno, IRQ cerradas (%ld mal)", v_esc);
    chk_cond(q, det, v_esc == 0);
    sprintf(det, "con el reloj arriba, su marca en el archivo (%ld veces sin ella)", v_marca);
    chk_cond(q, det, v_marca == 0);
    sprintf(det, "con el reloj arriba, turbo_nivel lo sabe, tambi" S_ee "n atascado (%ld veces no)", v_nivel);
    chk_cond(q, det, v_nivel == 0);
    sprintf(det, "un cuelgue arriba: al arrancar, marcado (%ld no)", v_arranque);
    chk_cond(q, det, v_arranque == 0);
    sprintf(det, "salir bien: a 396, sin marca y sin aviso al volver (%ld no)", v_salida);
    chk_cond(q, det, v_salida == 0);
    sprintf(det, "un reset con el turbo apagado no marca ning" S_uu "n nivel (%ld)", n_falso);
    chk_cond(q, det, n_falso == 0);
    tb_s->mal_m = 0; tb_s->pega_m = 0;
    turbo_sale();
    arch_bytes_borrar("/ces_tb_prueba.tns");
}

/* ---------------------------------------------------------------------------
 *  I11-I14. El control del gasto del emulador (sim.c, controla_gasto): la
 *  revisión del grupo cal. «Lo real», como en [75e] (kc_real)
 * ------------------------------------------------------------------------ */
/* caso_244 del oráculo [67] (red al azar de 19 elementos, 3 ms, el paso
 * automático): elige_paso planea 1,98 s y cuesta 1,83; en la primera mirada
 * el ritmo del arranque proyecta 5,3 s. Los valores, exactos (%a) */
static const SimEl TI_CASO244[19] = {
    { 1, 2, 0, 0, -1, 0, 1, 0x0p+0, { 0x1.8p+1, 0x1.4083483483483p+10, 0x1.9p+5, 0x1p+0, 0x0p+0 }, 0x0p+0, -0x1p+0, -0x1p+0, 0, 0, -1, -1, -1, 0, 0, 0, { 0, 0, 0, 0, 0, 0 } },
    { 0, 0, 0, 0, -1, 1, 2, 0x1.d0d58a2213376p+12, { 0, 0, 0, 0, 0 }, 0x0p+0, -0x1p+0, -0x1p+0, 0, 0, -1, -1, -1, 0, 0, 0, { 0, 0, 0, 0, 0, 0 } },
    { 0, 0, 0, 0, -1, 2, 3, 0x1.03b773e0b4b1bp+10, { 0, 0, 0, 0, 0 }, 0x0p+0, -0x1p+0, -0x1p+0, 0, 0, -1, -1, -1, 0, 0, 0, { 0, 0, 0, 0, 0, 0 } },
    { 0, 0, 0, 0, -1, 2, 4, 0x1.bda4a66781529p+7, { 0, 0, 0, 0, 0 }, 0x0p+0, -0x1p+0, -0x1p+0, 0, 0, -1, -1, -1, 0, 0, 0, { 0, 0, 0, 0, 0, 0 } },
    { 3, 0, 1, 0, -1, 3, 4, 0x1.4d3aefc77e91ap-23, { 0, 0, 0, 0, 0 }, 0x1.4p+2, -0x1p+0, -0x1p+0, 0, 0, -1, -1, -1, 0, 0, 0, { 0, 0, 0, 0, 0, 0 } },
    { 0, 0, 0, 0, -1, 0, 5, 0x1.b4509ff634c77p+11, { 0, 0, 0, 0, 0 }, 0x0p+0, -0x1p+0, -0x1p+0, 0, 0, -1, -1, -1, 0, 0, 0, { 0, 0, 0, 0, 0, 0 } },
    { 9, 0, 0, 0, -1, 5, 4, 0x0p+0, { 0, 0, 0, 0, 0 }, 0x0p+0, -0x1p+0, -0x1p+0, 0, 0, -1, -1, -1, 0, 2, 0,
      { 0x1.73db185f98919p-42, 0x1.bfb501p+0, 0x1.e9a151744ba5fp-4, 0x0p+0, 0x0p+0, 0x1.24ee01d4d9858p+0 } },
    { 5, 0, 0, 1, -1, 2, 6, 0x0p+0, { 0, 0, 0, 0, 0 }, 0x0p+0, 0x1.d7dbf487fcb93p-11, 0x1.cffeb074a771ep-10, 0, 0, -1, -1, -1, 0, 0, 0, { 0, 0, 0, 0, 0, 0 } },
    { 0, 0, 0, 0, -1, 6, 1, 0x1.04a79de601bp+7, { 0, 0, 0, 0, 0 }, 0x0p+0, -0x1p+0, -0x1p+0, 0, 0, -1, -1, -1, 0, 0, 0, { 0, 0, 0, 0, 0, 0 } },
    { 11, 0, 0, 0, -1, 0, 7, 0x1.b18322391d2ap-17, { 0, 0, 0, 0, 0 }, 0x0p+0, -0x1p+0, -0x1p+0, 0, 0, -1, -1, 0, 0, 0, 0, { 0, 0, 0, 0, 0, 0 } },
    { 0, 0, 0, 0, -1, 7, 0, 0x1.27fe1dc2d40a7p+13, { 0, 0, 0, 0, 0 }, 0x0p+0, -0x1p+0, -0x1p+0, 0, 0, -1, -1, -1, 0, 0, 0, { 0, 0, 0, 0, 0, 0 } },
    { 0, 0, 0, 0, -1, 7, 8, 0x1.49793d4865a4p+11, { 0, 0, 0, 0, 0 }, 0x0p+0, -0x1p+0, -0x1p+0, 0, 0, -1, -1, -1, 0, 0, 0, { 0, 0, 0, 0, 0, 0 } },
    { 3, 0, 0, 0, -1, 8, 0, 0x1.21c9e9ebee64p-23, { 0, 0, 0, 0, 0 }, 0x0p+0, -0x1p+0, -0x1p+0, 0, 0, -1, -1, -1, 0, 0, 0, { 0, 0, 0, 0, 0, 0 } },
    { 1, 0, 0, 0, -1, 0, 9, 0x0p+0, { -0x1.6p+3, 0, 0, 0, 0 }, 0x0p+0, -0x1p+0, -0x1p+0, 0, 0, -1, -1, -1, 0, 0, 0, { 0, 0, 0, 0, 0, 0 } },
    { 0, 0, 0, 0, -1, 9, 10, 0x1.414a2637fc51p+16, { 0, 0, 0, 0, 0 }, 0x0p+0, -0x1p+0, -0x1p+0, 0, 0, -1, -1, -1, 0, 0, 0, { 0, 0, 0, 0, 0, 0 } },
    { 0, 0, 0, 0, -1, 10, 0, 0x1.1055ac27d279p+14, { 0, 0, 0, 0, 0 }, 0x0p+0, -0x1p+0, -0x1p+0, 0, 0, -1, -1, -1, 0, 0, 0, { 0, 0, 0, 0, 0, 0 } },
    { 0, 0, 0, 0, -1, 2, 10, 0x1.07a8e6a8ad02ap+16, { 0, 0, 0, 0, 0 }, 0x0p+0, -0x1p+0, -0x1p+0, 0, 0, -1, -1, -1, 0, 0, 0, { 0, 0, 0, 0, 0, 0 } },
    { 0, 0, 0, 0, -1, 9, 11, 0x1.1c02c258c6246p+11, { 0, 0, 0, 0, 0 }, 0x0p+0, -0x1p+0, -0x1p+0, 0, 0, -1, -1, -1, 0, 0, 0, { 0, 0, 0, 0, 0, 0 } },
    { 14, 0, 0, 0, -1, 11, 0, 0x0p+0, { 0, 0, 0, 0, 0 }, 0x0p+0, -0x1p+0, -0x1p+0, 0, 0, 10, -1, -1, 0, 6, -1,
      { 0x1.5d3c7bcp+1, 0x1.1ce12add3cb5p-10, 0, 0, 0, 0 } },
};

/* Una emulación de sr con el paso automático y t_fin (0: automática), de un
 * tirón o de un paso en un paso; lo real */
static double ti_emula(double t_fin, unsigned long trozo)
{
    saj.t_fin = t_fin;
    sim_ops_reset();
    if (sim_iniciar_red(&sr, &saj) != SIM_OK) return -1.0;
    while (sim_avanzar(trozo)) {}
    return kc_real();
}

/* K9 (test_opt_cal.inc) con el S en t1 */
static void ti_k9(double t1)
{
    int ks, kC;
    sr_init(4);
    t4_seno(0, 1, 60.0, 1e3, 0.0);
    ks = sr_el(SE_S, 1, 2, 0.0); sr.el[ks].t1 = t1;
    sr_el(SE_R, 2, 0, 1e6); t4_d_cat(2, 3, &NL_1N4148);
    sr_el(SE_R, 3, 0, 100.0); kC = sr_el(SE_C, 3, 0, 10e-6);
    canal(0, CANAL_V, kC); canal(1, CANAL_NADA, 0);
}

static void ti_gasto(void)
{
    const char *q;
    char det[240], txt[120];
    const SimRes *r;
    double real, real1, tf3, tf4;
    unsigned long long h1;
    int i;

    /* I11: el plan de elige_paso valía; el ritmo alto era el del arranque */
    q = "[75i] I11 corte a destiempo";
    sr_init(12);
    sr.nel = 19;
    memcpy(sr.el, TI_CASO244, sizeof TI_CASO244);
    canal_de(&saj, &sr, 0, CANAL_NODO, 8); canal_de(&saj, &sr, 1, CANAL_I, 3);
    real = ti_emula(3e-3, ~0ul);
    r = sim_res();
    printf("    caso_244 de [67]: t_fin %.4g de %.4g, %ld pasos, estimado %.3f s, real %.3f s\n", r->t_fin, r->t_pedido, r->pasos,
           r->t_est, real);
    sprintf(det, "caso_244: el plan (1,98 s) vale y no se corta: los 3 ms enteros, en %.2f s", real);
    chk_cond(q, det, r->fin == FIN_COMPLETO && r->t_fin == 3e-3 && real <= 2.2 && real <= 1.1 * r->t_est);

    /* I12: pedir más no da menos (el puente del banco) */
    q = "[75i] I12 pedir m" S_aa "s";
    kc_puente(0.3, ~0ul); tf3 = sim_res()->t_fin; real = kc_real();
    kc_puente(0.4, ~0ul); tf4 = sim_res()->t_fin; real1 = kc_real();
    printf("    el puente del banco: 0,3 s pedidos, %.4g (%.2f s); 0,4 s, %.4g (%.2f s)\n", tf3, real, tf4, real1);
    sprintf(det, "el puente: 0,4 s pedidos da %.3g s, no menos que 0,3 (%.3g): se acorta a lo que cabe en 3 s, lo que deja correr sin cortar",
            tf4, tf3);
    chk_cond(q, det, tf4 >= tf3 * (1.0 - 1e-9) && real1 <= 3.1 && real <= 3.1);

    /* I13: lo caro llega tras la última 1-2-5 antes del final */
    q = "[75i] I13 caro tras la " S_uu "ltima 1-2-5";
    {
        static const double T1[3] = { 0.21, 0.25, 0.39 }, TF[3] = { 0.3, 0.3, 0.5 };
        for (i = 0; i < 3; i++) {
            ti_k9(T1[i]);
            real = ti_emula(TF[i], ~0ul);
            r = sim_res();
            printf("    un S en %.3g s de %.3g: t_fin %.4g, medida desde %.4g, estimado %.2f s, real %.2f s\n", T1[i], TF[i], r->t_fin,
                   r->t_med0, r->t_est, real);
            sprintf(det, "un S en %.3g s de %.3g pedidos: se acorta igual (a %.4g s), en %.2f s (antes, de 4,6 a 8,2)", T1[i], TF[i],
                    r->t_fin, real);
            chk_cond(q, det, r->fin == FIN_COMPLETO && r->t_fin < TF[i] && r->t_med0 >= T1[i] && real <= 3.3 &&
                     real <= 1.1 * r->t_est && r->t_est <= 1.1 * real);
        }
        /* y sin depender del troceo */
        h1 = kc_huella(); real1 = r->t_fin;
        ti_k9(T1[2]);
        ti_emula(TF[2], 1ul);
        chk_cond(q, "sim_avanzar(1): la misma ventana y el mismo registro (bit a bit)", sim_res()->t_fin == real1 && kc_huella() == h1);
    }

    /* I14: el aviso dice lo que pasó (la ventana; y el paso, que también: I28) */
    q = "[75i] I14 aviso";
    kc_puente(1.0, ~0ul);
    r = sim_res();
    sim_aviso(0, r, SIM_AV_PASO_LIM, txt, (int)sizeof txt);
    sprintf(det, "el puente con 1 s, acortado a %.3g s: \"%.100s\"", r->t_fin, txt);
    chk_cond(q, det, r->t_fin < 1.0 && (r->d.avisos & SIM_AV_PASO_LIM) && strstr(txt, "ventana") != 0 &&
             strlen(txt) <= 52);
}

/* ---------------------------------------------------------------------------
 *  I15. gfx_fila_pares (la revisión de dibujo): la única primitiva que no
 *       recorta tampoco escribe fuera de la pantalla ni de su fila
 * ------------------------------------------------------------------------ */
static void ti_fila_pares(void)
{
    const char *q = "[75i] I15 gfx_fila_pares";
    static unsigned char f[200];
    static u32 pares[256];
    const u16 *fb0 = gfx_buffer();
    int i, bien = 1;
    for (i = 0; i < 256; i++) pares[i] = 0x5A5AA5A5u;
    for (i = 0; i < 200; i++) f[i] = (unsigned char)i;
    gfx_clear(C_FONDO);
    gfx_fila_pares(100, f, 170, pares);                    /* 10 palabras de más */
    for (i = 0; i < SCR_W; i++) if (fb0[101 * SCR_W + i] != C_FONDO || fb0[100 * SCR_W + i] == C_FONDO) bien = 0;
    chk_cond(q, "170 pares en la fila 100: la llena entera y no pasa a la 101", bien);
    gfx_clear(C_FONDO);
    gfx_fila_pares(-1, f, 160, pares);
    gfx_fila_pares(SCR_H, f, 160, pares);
    gfx_fila_pares(-100000, f, 160, pares);
    gfx_fila_pares(SCR_H - 1, f, 160, pares);             /* la última, sí */
    for (i = 0, bien = 1; i < SCR_W * SCR_H; i++)
        if ((fb0[i] != C_FONDO) != (i >= (SCR_H - 1) * SCR_W)) bien = 0;
    chk_cond(q, "filas fuera de la pantalla (-1, SCR_H, muy lejos): no escriben (ni fuera: lo mira ASan); la " S_uu "ltima, s" S_ii, bien);
    gfx_clear(C_FONDO);
}

/* ---------------------------------------------------------------------------
 *  I16 e I17. Lo visto de paso en el cierre: las medidas del osciloscopio
 *       por debajo de su escala, y la banderita de la sonda sobre un rótulo
 * ------------------------------------------------------------------------ */
static SimReg ti_r;
static SimRes ti_sr;
static Osc ti_o;
static u16 ti_fb[SCR_W * SCR_H];

static void ti_vistos(void)
{
    const char *q = "[75i] I16 medidas";
    const u16 *fb = gfx_buffer();
    char det[200];
    int i, n, tapados;

    /* el canal 1, ruido de redondeo (-1e-14 V) a 5 V/div; el 2, 5 V */
    memset(&ti_r, 0, sizeof ti_r);
    for (i = 0; i <= 200; i++) pon(&ti_r, i * 1e-4, -1e-14, 5.0 * sin(2.0 * 3.141592653589793 * 50.0 * i * 1e-4));
    cierra_reg(&ti_r);
    memset(&ti_sr, 0, sizeof ti_sr);
    ti_sr.t_fin = ti_sr.t_pedido = ti_r.t[ti_r.n - 1]; ti_sr.fin = FIN_COMPLETO;
    ti_sr.canal[0].tipo = CANAL_V; ti_sr.canal[1].tipo = CANAL_V;
    strcpy(ti_sr.canal[0].nombre, "V(C1)"); strcpy(ti_sr.canal[1].nombre, "V(E1)");
    osc_nuevo(&ti_o, &ti_r, &ti_sr, 1);
    ti_o.ivdiv[0] = (signed char)osc_escala_ge(5.0);
    gfx_textos_reset();
    osc_dibuja(&ti_o, &ti_r, &ti_sr);
    chk_cond(q, "-1e-14 V a 5 V/div: las medidas dicen 0 V, no \"-0,01pV\"", !gfx_texto_visto("pV") && !gfx_texto_visto("fV") &&
             gfx_texto_visto("0V"));
    ti_o.cursores = 1; ti_o.panel = OSC_CURSORES; ti_o.tc[0] = 0.005; ti_o.tc[1] = 0.01;
    gfx_textos_reset();
    osc_dibuja(&ti_o, &ti_r, &ti_sr);
    chk_cond(q, "...y los cursores y su diferencia, tambi" S_ee "n", !gfx_texto_visto("pV") && !gfx_texto_visto("fV"));

    /* I17: la sonda de nodo junto al rótulo de R (678_emu_sondas) */
    q = "[75i] I17 banderita";
    dibuja_rc();
    ui_dibuja();
    memcpy(ti_fb, fb, sizeof ti_fb);
    TECLAS(T_LETRA('O'), T_ENTER);
    ui_dibuja();
    for (i = 16 * SCR_W, n = 0, tapados = 0; i < 150 * SCR_W; i++)          /* el esquema, bajo la barra */
        if (ti_fb[i] == C_RESIST) { n++; if (fb[i] != C_RESIST) tapados++; }   /* la R y su rótulo */
    sprintf(det, "la banderita del canal 1 no tapa el \"1k" S_OMEGA "\" de al lado (%d de sus %d p" S_ii "xeles tapados)", tapados, n);
    printf("    I17: %d p" S_ii "xeles de la R y su r" S_oo "tulo, %d tapados\n", n, tapados);
    chk_cond(q, det, ui_circuito()->sonda[0].tipo == SONDA_NODO && n > 10 && tapados <= 4);
}

/* ---------------------------------------------------------------------------
 *  I18. La segunda página del banco (grupo otros) con el turbo: sus ciclos,
 *       a los MHz de la medida, y si el reloj cambia a mitad, se dice
 * ------------------------------------------------------------------------ */
static void ti_banco(void)
{
    const char *q = "[75i] I18 banco y turbo";
    int n;
    tb_limpio();
    turbo_pon(36);
    banco_extra(1);
    cm_ayuda();
    TECLAS(T_LETRA('T'));
    for (n = 0; n < 40 && !banco_hecho(); n++) { ui_dibuja(); ui_tick(); }
    chk_cond(q, "las dos p" S_aa "ginas a 432: la segunda da sus ciclos", banco_hecho() && banco_en_pagina() == 1 &&
             ve("Ciclos: vuelta") && !ve("cambi" S_oo " a mitad"));
    TECLAS(T_ENTER);                                        /* otra vez; tras la primera, a 396 */
    for (n = 0; n < 40 && !banco_hecho(); n++) {
        if (n == BN_N) turbo_pon(0);
        ui_dibuja();
        ui_tick();
    }
    chk_cond(q, "el reloj cambia en la segunda: \"Ciclos: el reloj cambi" S_oo " a mitad\" (no los de 432)",
             banco_hecho() && ve("Ciclos: el reloj cambi" S_oo " a mitad") && !ve("Ciclos: vuelta"));
    TECLAS(T_ESC);
    banco_extra(0);
    turbo_sale();
}

/* ---------------------------------------------------------------------------
 *  I19. El estado de Newton de antes del paso (la revisión de motor):
 *       guardado y repuesto a medias (solo NL.el) da lo mismo que entero,
 *       también con sim_op_red de otras redes a mitad de una emulación
 * ------------------------------------------------------------------------ */
static void ti_fuzz(void)
{
    const char *q = "[75i] I19 bak del Newton";
    char det[240];
    unsigned s;
    int mal = 0;
    for (s = 470; s <= 480; s++) {
        unsigned long long h0 = fz_en_hijo(s, 500, 1), h1 = fz_en_hijo(s, 500, 0);
        if (h0 != h1 || !h0) { mal++; printf("    semilla %u: distinta\n", s); }
    }
    sprintf(det, "las semillas 470 a 480 del arn" S_ee "s al azar (la 476, sim_op_red a mitad de una emulaci" S_oo "n no lineal): "
            "a medias, lo mismo que entero (%d distintas)", mal);
    chk_cond(q, det, mal == 0);
}

/* ---------------------------------------------------------------------------
 *  I20. La bajada que la PMU no confirma (la revisión final del turbo): nunca
 *       se escribe con el pendiente a 1, y lo escrito sin confirmar cuenta
 *       como atascado (la puerta no escribe) hasta que el pendiente se limpia
 * ------------------------------------------------------------------------ */
/* Las escrituras de la PMU hechas con el pendiente a 1 (encima de una
 * petición sin atender) */
static int ti_encima(void)
{
    int i, n = tb_s->nesc < 64 ? tb_s->nesc : 64, k = 0;
    for (i = 0; i < n; i++) if (tb_s->esc[i].pend) k++;
    return k;
}

/* Los archivos escritos con la CPU de verdad por encima de 396, o con una
 * petición de la PMU sin atender */
static int ti_arch_mal(void)
{
    int i, n = tb_s->narch < 32 ? tb_s->narch : 32, k = 0;
    for (i = 0; i < n; i++) if (tb_s->arch[i].cpu > TURBO_M_SERIE || tb_s->arch[i].pend) k++;
    return k;
}

static void ti_sin_confirmar(void)
{
    const char *q = "[75i] I20 bajada sin confirmar";
    unsigned char b[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    char det[240];
    unsigned t0;
    int r;

    /* a 432 el SO deja de atender INT_POWER y se guarda un archivo: la puerta
     * escribe 35 y la PMU no lo confirma (antes: 34 y 33 encima, con el
     * pendiente a 1, y el archivo «a 396») */
    tb_limpio();
    turbo_pon(36);
    tb_s->atiende = -1;
    tb_s->nesc = 0; tb_s->narch = 0;
    r = arch_bytes_guardar("/ces_tb_prueba.tns", b, 8);
    sprintf(det, "a 432 el SO deja de atender: la puerta no escribe (guardar %d, %d archivos, escrituras \"%s\", %d encima)",
            r, tb_s->narch, tb_ms(), ti_encima());
    chk_cond(q, det, !r && tb_s->narch == 0 && ti_encima() == 0 && !strcmp(tb_ms(), "35") && turbo_atascado());
    tb_panel();
    chk_cond(q, "...queda atascado: el panel lo dice y la barra sigue con su marca",
             ve("No se pudo volver a 396 MHz: no se guarda nada.") && ve("420 MHz"));
    TECLAS(T_ESC);

    /* el SO vuelve a atender: la escritura siguiente lo confirma, baja y escribe */
    tb_s->atiende = 3;
    tb_s->nesc = 0; tb_s->narch = 0;
    r = arch_bytes_guardar("/ces_tb_prueba.tns", b, 8);
    sprintf(det, "cuando el SO vuelve a atender, la escritura siguiente confirma lo de antes, baja (\"%s\") y escribe a 396", tb_ms());
    chk_cond(q, det, r && !turbo_atascado() && !strcmp(tb_ms(), "34 33") && tb_s->narch >= 1 && ti_arch_mal() == 0 &&
             tb_s->arch[tb_s->narch - 1].m == TURBO_M_SERIE && tb_s->pll == TURBO_SERIE);
    arch_bytes_borrar("/ces_tb_prueba.tns");

    /* el SO no atiende nunca: sube a 408 y no se confirma; no se escribe 33 encima */
    tb_limpio();
    tb_s->atiende = -1;
    r = turbo_pon(34);
    sprintf(det, "el SO no atiende nunca: 34 sin confirmar y nada encima (\"%s\", %d encima): atascado a 408", tb_ms(), ti_encima());
    chk_cond(q, det, r == TBM_PENDIENTE && !strcmp(tb_ms(), "34") && ti_encima() == 0 && turbo_atascado() && turbo_nivel() == 34 &&
             turbo_mhz_marca() == 408);
    turbo_sale();
    chk_cond(q, "...y al salir sigue atascado, sin escribir nada y con la marca de 408", turbo_atascado() && tb_s->nesc == 1 &&
             ti_marca_arch() == 34);

    /* la última bajada (a 396) sin confirmar: el registro ya dice 396, pero
     * la CPU puede seguir a 408; ni se escribe, ni la vuelta del bucle
     * (turbo_vigila) lo da por del SO */
    tb_limpio();
    turbo_pon(34);
    tb_s->atiende = -1;
    tb_s->nesc = 0;
    r = turbo_pon(0);
    tb_s->narch = 0;
    chk_cond(q, "la bajada a 396 sin confirmar: el registro dice 396 y sigue atascado (la marca, 408) sin escribir nada",
             r == TBM_PENDIENTE && !strcmp(tb_ms(), "33") && tb_s->pll == TURBO_SERIE && turbo_atascado() && turbo_mhz_marca() == 408 &&
             !arch_bytes_guardar("/ces_tb_prueba.tns", b, 8) && tb_s->narch == 0);
    ui_rotulo();
    chk_cond(q, "...y la vuelta del bucle no lo da por del SO (396 puede ser lo de Hakaru, sin confirmar)", turbo_atascado() &&
             turbo_mhz_marca() == 408);
    tb_s->atiende = 3;
    r = arch_bytes_guardar("/ces_tb_prueba.tns", b, 8);
    chk_cond(q, "...hasta que el SO la atiende: confirmada, se escribe a 396", r && !turbo_atascado() && ti_arch_mal() == 0 &&
             tb_s->narch == 1 && tb_s->nesc == 1);
    arch_bytes_borrar("/ces_tb_prueba.tns");

    /* la PMU que no lleva a la CPU una petición hecha con la anterior sin
     * atender (el modo 3 de la revisión): antes, la CPU quedaba a 408 con el
     * registro a 396, y se escribía */
    tb_limpio();
    tb_s->no_toma_pend = 1; tb_s->atiende = -1;
    turbo_pon(34);
    tb_s->narch = 0;
    r = arch_bytes_guardar("/ces_tb_prueba.tns", b, 8);
    turbo_sale();
    sprintf(det, "la PMU que no toma lo escrito con el pendiente a 1: ning" S_uu "n archivo con la CPU a 408 (%d mal; guardar %d), "
            "y sale atascado", ti_arch_mal(), r);
    chk_cond(q, det, !r && tb_s->narch == 0 && ti_arch_mal() == 0 && turbo_atascado());

    /* otra petición a medias, sin atender (el pendiente a 1 al empezar a
     * bajar): tampoco bajando se escribe encima (antes, bajando se escribía
     * igual tras el tope) */
    tb_limpio();
    turbo_pon(36);
    tb_s->pend |= 1u; tb_s->atiende = -1;
    tb_s->nesc = 0;
    r = turbo_pon(0);
    sprintf(det, "con otra petici" S_oo "n sin atender, ni bajando se escribe encima (%d escrituras): atascado a 432", tb_s->nesc);
    chk_cond(q, det, r == TBM_PENDIENTE && tb_s->nesc == 0 && turbo_atascado() && tb_s->pll == 0x24020303u);
    tb_s->atiende = 3;
    turbo_sale();
    chk_cond(q, "...y cuando el SO la atiende, a 396", !turbo_atascado() && tb_s->pll == TURBO_SERIE && ti_encima() == 0);

    /* salir desde 456 con el SO que deja de atender: una escritura y ~1 s con
     * las IRQ abiertas (antes: de 12 en 12, cinco y ~4,5 s) */
    tb_limpio();
    turbo_alto(1);
    turbo_pon(38);
    tb_s->atiende = -1;
    tb_s->nesc = 0;
    t0 = tb_s->t;
    turbo_sale();
    sprintf(det, "salir de 456 con el SO que no atiende: %d escritura(s) y %.2f s de espera (antes, 5 y ~4,5)", tb_s->nesc,
            (double)(tb_s->t - t0) / 32768.0);
    chk_cond(q, det, tb_s->nesc == 1 && tb_s->t - t0 <= 2u * 16384u + 4000u && turbo_atascado() && ti_encima() == 0);
    tb_s->atiende = 3;
    turbo_sale();
    chk_cond(q, "...y cuando el SO atiende, sale a 396", !turbo_atascado() && tb_s->pll == TURBO_SERIE);
}

/* El mono de la PMU (el de la revisión final, con el simulador del arnés):
 * acciones al azar del turbo con un SO que deja de atender, atiende tarde o
 * vuelve, la PMU que no toma lo escrito con el pendiente a 1 y el USB. Tras
 * cada una: ninguna escritura con el pendiente a 1, las IRQ cerradas, y
 * ningún archivo con la CPU por encima de 396 ni con una petición sin atender */
static void ti_mono_pmu(void)
{
    const char *q = "[75i] I20 mono de la PMU";
    unsigned char b4[4] = { 4, 3, 2, 1 };
    char det[240];
    long k, N = getenv("HAKARU_TI_MONO") ? atol(getenv("HAKARU_TI_MONO")) : 10000L;
    long v_encima = 0, v_arch = 0, v_irq = 0, v_esc = 0, n_arch = 0, n_atasco = 0, n_esc = 0;
    int i, n;
    tb_limpio();
    turbo_alto(1);
    for (k = 0; k < N; k++) {
        switch (ti_azar(48)) {                              /* el SO y la PMU cambian de humor */
        case 0: case 1: case 2: case 3: tb_s->atiende = -1; break;
        case 4: case 5: tb_s->atiende = 1 + (long)ti_azar(3000); break;
        case 6: case 7: case 8: case 9: case 10: tb_s->atiende = 3; break;
        case 11: tb_s->no_toma_pend = (int)ti_azar(2); break;
        case 12: if (!(tb_s->pend & 1u)) { tb_s->pll = 0x18020303u; tb_s->cpu_m = 0; } break;   /* el USB */
        case 13: case 14: case 15:                          /* sin el USB (apagar y encender) */
            if ((tb_s->pll >> 24) == TURBO_M_USB) { tb_s->pll = TURBO_SERIE; tb_s->cpu_m = 0; tb_s->pend = 0; }
            break;
        default: break;
        }
        switch (ti_azar(12)) {
        case 0: case 1: case 2: turbo_pon(ti_azar(2) ? 0 : TURBO_M_MIN + (int)ti_azar(5)); break;
        case 3: case 4: arch_bytes_guardar("/ces_tb_prueba.tns", b4, 4); break;
        case 5: turbo_vigila(); break;
        case 6: if (turbo_pedido()) turbo_prueba(turbo_pedido()); break;
        case 7: case 8: turbo_prueba_paso(); break;
        case 9: turbo_sale(); break;
        case 10: if (turbo_pausa()) arch_bytes_guardar("/ces_tb_prueba.tns", b4, 4); turbo_sigue(); break;
        default: turbo_prueba_cancela(); break;
        }
        if (tb_s->irq_abiertas) { v_irq++; tb_s->irq_abiertas = 0; }
        v_encima += ti_encima();
        n = tb_s->nesc < 64 ? tb_s->nesc : 64;
        for (i = 0; i < n; i++)
            if ((tb_s->esc[i].v & 0x00FFFFFFu) != (TURBO_SERIE & 0x00FFFFFFu) || tb_s->esc[i].irq_abiertas) v_esc++;
        v_arch += ti_arch_mal();
        n_arch += tb_s->narch;
        n_esc += tb_s->nesc;
        if (turbo_atascado()) n_atasco++;
        tb_s->nesc = tb_s->narch = 0;
    }
    sprintf(det, "%ld acciones: %ld escrituras de la PMU, %ld archivos, %ld veces atascado", N, n_esc, n_arch, n_atasco);
    printf("    I20 mono: %s\n", det);
    chk_cond(q, det, n_esc > 0 && n_arch > 0 && n_atasco > 0);
    sprintf(det, "ninguna escritura con el pendiente a 1 (%ld), ni rara o con las IRQ abiertas (%ld); las IRQ, cerradas al volver (%ld)",
            v_encima, v_esc, v_irq);
    chk_cond(q, det, v_encima == 0 && v_esc == 0 && v_irq == 0);
    sprintf(det, "ning" S_uu "n archivo con la CPU por encima de 396 o con una petici" S_oo "n sin atender (%ld)", v_arch);
    chk_cond(q, det, v_arch == 0);
    tb_s->atiende = 3; tb_s->no_toma_pend = 0;
    turbo_sale();
    arch_bytes_borrar("/ces_tb_prueba.tns");
}

/* ---------------------------------------------------------------------------
 *  I21. [P] mide la CPU (la revisión final: el registro no dice a qué va la
 *       CPU): una carga fija cronometrada con el contador de 32 kHz a 396 y a
 *       M tiene que tardar 33/M; si no, el registro cambió y la CPU no
 * ------------------------------------------------------------------------ */
static void ti_cpu(void)
{
    const char *q = "[75i] I21 [P] mide la CPU";
    TurboPrueba p;
    char det[200];
    long i;
    tb_limpio();
    tb_s->pll_quieta = 1;                                   /* el registro cambia y la CPU no */
    turbo_prueba(34);
    tb_prueba_entera();
    turbo_prueba_estado(&p);
    sprintf(det, "el registro a 408 y la CPU a 396: [P] no la da por buena (fase %d, motivo %d); a 396, apagado y sin marcar el nivel",
            p.fase, p.motivo);
    chk_cond(q, det, p.fase == TBP_MAL && p.motivo == TBM_CPU && tb_s->pll == TURBO_SERIE && turbo_nivel() == 0 &&
             turbo_pedido() == 0 && turbo_fallidos() == 0);
    tb_limpio();
    tb_s->pll_quieta = 1;
    tb_panel();
    TECLAS(T_DER, T_ENTER, T_LETRA('P'));
    for (i = 0; i < 100000L && ui_animando() == UI_TRABAJANDO; i++) ui_tick();
    chk_cond(q, "...y el panel dice que la CPU no va a 408 MHz, y el turbo en \"no\"", ve("La CPU no va a 408 MHz") && ve(S_LARR " no " S_RARR));
    TECLAS(T_ESC);
    tb_limpio();
    turbo_prueba(35);
    tb_prueba_entera();
    turbo_prueba_estado(&p);
    chk_cond(q, "con la CPU a 420 de verdad: superada, como siempre", p.fase == TBP_BIEN && turbo_nivel() == 35);
    turbo_sale();
}

/* ---------------------------------------------------------------------------
 *  I22. El [esc] de la medida del reloj A M (la 2.ª, tras subir; la
 *       revisión final): termina() baja y espera a que se suelte (suelta);
 *       el bucle no lo ve, así que no hay nada que tragar: el [esc] siguiente
 *       sale del panel (I6 prueba la 1.ª medida, a 396, que no baja)
 * ------------------------------------------------------------------------ */
static int ti_armado_m, ti_vistas_m;
static int ti_salir_m(void)
{
    if (!ti_armado_m || (turbo_sim()->pll >> 24) <= TURBO_M_SERIE) return 0;   /* solo la medida a M */
    ti_armado_m = 0;
    ti_vistas_m++;
    turbo_sim()->tecla = 3000;                              /* el [esc], abajo un momento: suelta() lo espera */
    return 1;
}

static void ti_esc_medida_m(void)
{
    const char *q = "[75i] I22 [esc] en la medida a M";
    TurboPrueba p;
    long i;
    tb_limpio();
    tb_panel();
    TECLAS(T_DER, T_ENTER);                                 /* 408 */
    ce_abre(CE_W, CE_H, 0);
    turbo_salida(ti_salir_m);
    ti_armado_m = 1; ti_vistas_m = 0;
    ce_pulsa(TF_LETRA('P'));
    for (i = 0; i < 4000 && ti_vistas_m == 0; i++) ce_pasa(50);
    ce_pasa(300);
    turbo_salida(0);
    turbo_prueba_estado(&p);
    chk_cond(q, "[esc] en la medida a M: cancela, a 396 y en el panel con \"Prueba cancelada\" (ese [esc] ya se solt" S_oo ")",
             ti_vistas_m == 1 && p.motivo == TBM_CANCELADA && !p.esc && tb_s->pll == TURBO_SERIE && ui_pantalla() == P_CONTROLES &&
             ve("Prueba cancelada"));
    TECLAS(T_ESC);
    chk_cond(q, "...y el [esc] siguiente (el del usuario, para salir) sale del panel", ui_pantalla() != P_CONTROLES);
    if (ui_pantalla() == P_CONTROLES) TECLAS(T_ESC);
    turbo_sale();
}

/* ---------------------------------------------------------------------------
 *  I23. La puerta con un reloj que no sabe calcular (D1 = 0: turbo_mhz()
 *       da 0): en una CX II no escribe
 * ------------------------------------------------------------------------ */
static void ti_mhz_cero(void)
{
    const char *q = "[75i] I23 reloj sin calcular";
    unsigned char b[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    int r;
    tb_limpio();
    tb_s->pll = 0x24000303u;                                /* M = 36 y D1 = 0 */
    tb_s->narch = 0;
    r = arch_bytes_guardar("/ces_tb_prueba.tns", b, 8);
    chk_cond(q, "0x90140030 = 0x24000303 (M = 36, D1 = 0): turbo_mhz() da 0 y la puerta no escribe ni toca la PMU",
             turbo_mhz() == 0 && !r && tb_s->narch == 0 && tb_s->nesc == 0 && turbo_motivo() == TBM_RARO);
    tb_s->pll = 0x21000303u;                                /* M = 33 y D1 = 0: tampoco es lo del SO */
    r = arch_bytes_guardar("/ces_tb_prueba.tns", b, 8);
    chk_cond(q, "...ni con 0x21000303 (M = 33, D1 = 0)", !r && tb_s->narch == 0 && tb_s->nesc == 0);
    tb_panel();
    chk_cond(q, "el panel lo dice", ve("Reloj desconocido,"));
    TECLAS(T_ESC);
    tb_s->pll = TURBO_SERIE;
    tb_s->cx2 = 0;                                          /* sin CX II (la versión sin el turbo): turbo_mhz() da 0 */
    tb_s->narch = 0;
    r = arch_bytes_guardar("/ces_tb_prueba.tns", b, 8);
    chk_cond(q, "sin CX II (como la versi" S_oo "n sin el turbo, en que turbo_mhz() tambi" S_ee "n da 0) se escribe como siempre",
             r && tb_s->narch == 1 && tb_s->nesc == 0);
    tb_s->cx2 = 1;
    arch_bytes_borrar("/ces_tb_prueba.tns");
}

/* ---------------------------------------------------------------------------
 *  I24-I30. El control del gasto (la revisión final, lente «gasto»). Las
 *  redes de [67] (caso_245 y caso_258), exactas (%a)
 * ------------------------------------------------------------------------ */
static const SimEl TI_CASO245[15] = {
    { 1, 4, 0, 0, -1, 0, 1, 0x1.07d6d95add744p+3, { 0x0p+0, 0x1.4p+2, 0x1.2599ed7c6fbd3p-11, 0x0p+0, 0x0p+0 }, 0x0p+0, -0x1p+0, -0x1p+0, 0, 0, -1, -1, -1, 0, 0, 0, { 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0 } },
    { 0, 0, 0, 0, -1, 0, 2, 0x1.b2ba297460322p+7, { 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0 }, 0x0p+0, -0x1p+0, -0x1p+0, 0, 0, -1, -1, -1, 0, 0, 0, { 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0 } },
    { 0, 0, 0, 0, -1, 1, 3, 0x1.519e81026a6fcp+7, { 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0 }, 0x0p+0, -0x1p+0, -0x1p+0, 0, 0, -1, -1, -1, 0, 0, 0, { 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0 } },
    { 0, 0, 0, 0, -1, 1, 4, 0x1.1b6b221013434p+7, { 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0 }, 0x0p+0, -0x1p+0, -0x1p+0, 0, 0, -1, -1, -1, 0, 0, 0, { 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0 } },
    { 0, 0, 0, 0, -1, 0, 5, 0x1.137194e105e23p+10, { 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0 }, 0x0p+0, -0x1p+0, -0x1p+0, 0, 0, -1, -1, -1, 0, 0, 0, { 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0 } },
    { 3, 0, 0, 0, -1, 5, 4, 0x1.32c88b7ab58a2p-23, { 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0 }, 0x0p+0, -0x1p+0, -0x1p+0, 0, 0, -1, -1, -1, 0, 0, 0, { 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0 } },
    { 0, 0, 0, 0, -1, 4, 6, 0x1.1ce54b4faf727p+12, { 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0 }, 0x0p+0, -0x1p+0, -0x1p+0, 0, 0, -1, -1, -1, 0, 0, 0, { 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0 } },
    { 9, 0, 0, 0, -1, 6, 2, 0x0p+0, { 0x1.eb851eb851eb8p-6, 0x1.47ae147ae147bp-6, 0x0p+0, 0x0p+0, 0x0p+0 }, 0x0p+0, -0x1p+0, -0x1p+0, 0, 0, -1, -1, -1, 0, 2, 0, { 0x1.5aa8339ec0e3fp-60, 0x1.8p+1, 0x1.ep+3, 0x0p+0, 0x0p+0, 0x1.7d39340694ba3p+1 } },
    { 1, 0, 0, 0, -1, 0, 7, 0x0p+0, { -0x1.2p+3, 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0 }, 0x0p+0, -0x1p+0, -0x1p+0, 0, 0, -1, -1, -1, 0, 0, 0, { 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0 } },
    { 0, 0, 0, 0, -1, 7, 8, 0x1.0c12acb904bb4p+16, { 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0 }, 0x0p+0, -0x1p+0, -0x1p+0, 0, 0, -1, -1, -1, 0, 0, 0, { 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0 } },
    { 0, 0, 0, 0, -1, 8, 0, 0x1.48ee698002ff9p+14, { 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0 }, 0x0p+0, -0x1p+0, -0x1p+0, 0, 0, -1, -1, -1, 0, 0, 0, { 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0 } },
    { 0, 0, 0, 0, -1, 5, 10, 0x1.5e14df46104e9p+11, { 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0 }, 0x0p+0, -0x1p+0, -0x1p+0, 0, 0, -1, -1, -1, 0, 0, 0, { 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0 } },
    { 3, 0, 0, 0, -1, 10, 8, 0x1.d5c31593e5fb8p-18, { 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0 }, 0x0p+0, -0x1p+0, -0x1p+0, 0, 0, -1, -1, -1, 0, 0, 0, { 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0 } },
    { 0, 0, 0, 0, -1, 7, 9, 0x1.e35b1fcecdc51p+10, { 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0 }, 0x0p+0, -0x1p+0, -0x1p+0, 0, 0, -1, -1, -1, 0, 0, 0, { 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0 } },
    { 13, 0, 0, 0, -1, 9, 0, 0x0p+0, { 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0 }, 0x0p+0, -0x1p+0, -0x1p+0, 0, 0, 8, -1, -1, 0, 5, -1, { 0x1.ed2852e4e84e4p-46, 0x1.5b2ddbf4df89dp+6, 0x1.b4da58793e3ffp+1, 0x1.4ad85eb361c5dp+5, 0x0p+0, 0x1.6892640b03087p-1 } },
};

static const SimEl TI_CASO258[15] = {
    { 1, 2, 0, 0, -1, 0, 1, 0x0p+0, { 0x1.2p+3, 0x1.4534085340853p+6, 0x1.18p+6, -0x1p+1, 0x0p+0 }, 0x0p+0, -0x1p+0, -0x1p+0, 0, 0, -1, -1, -1, 0, 0, 0, { 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0 } },
    { 0, 0, 0, 0, -1, 0, 2, 0x1.39a178f753068p+9, { 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0 }, 0x0p+0, -0x1p+0, -0x1p+0, 0, 0, -1, -1, -1, 0, 0, 0, { 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0 } },
    { 0, 0, 0, 0, -1, 1, 3, 0x1.984d3549e3a04p+10, { 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0 }, 0x0p+0, -0x1p+0, -0x1p+0, 0, 0, -1, -1, -1, 0, 0, 0, { 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0 } },
    { 0, 0, 0, 0, -1, 2, 4, 0x1.7a60b0e2996b2p+10, { 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0 }, 0x0p+0, -0x1p+0, -0x1p+0, 0, 0, -1, -1, -1, 0, 0, 0, { 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0 } },
    { 0, 0, 0, 0, -1, 3, 5, 0x1.5851a543c22a4p+12, { 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0 }, 0x0p+0, -0x1p+0, -0x1p+0, 0, 0, -1, -1, -1, 0, 0, 0, { 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0 } },
    { 9, 0, 0, 0, -1, 5, 2, 0x0p+0, { 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0 }, 0x0p+0, -0x1p+0, -0x1p+0, 0, 0, -1, -1, -1, 0, 2, 0, { 0x1.6849b86a12b9bp-47, 0x1p+0, 0x1.541435c537c7fp+2, 0x1.76157b46d9b81p+3, 0x1.d2b7e7746ac84p-9, 0x1.75e888c2223b1p-1 } },
    { 2, 1, 0, 0, -1, 1, 2, 0x0p+0, { 0x1.47ae147ae147bp-8, 0x1.dc30c30c30c3p+8, 0x1.4ap+8, 0x1.0624dd2f1a9fcp-9, 0x0p+0 }, 0x0p+0, -0x1p+0, -0x1p+0, 0, 0, -1, -1, -1, 0, 0, 0, { 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0 } },
    { 11, 0, 0, 0, -1, 0, 6, 0x1.5e902ed2e9cb4p-2, { 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0 }, 0x0p+0, -0x1p+0, -0x1p+0, 0, 0, -1, -1, 4, 1, 0, 0, { 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0 } },
    { 0, 0, 0, 0, -1, 6, 0, 0x1.273833e23b3p+16, { 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0 }, 0x0p+0, -0x1p+0, -0x1p+0, 0, 0, -1, -1, -1, 0, 0, 0, { 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0 } },
    { 1, 0, 0, 0, -1, 0, 7, 0x0p+0, { -0x1.8p+3, 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0 }, 0x0p+0, -0x1p+0, -0x1p+0, 0, 0, -1, -1, -1, 0, 0, 0, { 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0 } },
    { 0, 0, 0, 0, -1, 7, 8, 0x1.4f7b503dfc29p+14, { 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0 }, 0x0p+0, -0x1p+0, -0x1p+0, 0, 0, -1, -1, -1, 0, 0, 0, { 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0 } },
    { 0, 0, 0, 0, -1, 8, 0, 0x1.1646f7e2a6dc1p+11, { 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0 }, 0x0p+0, -0x1p+0, -0x1p+0, 0, 0, -1, -1, -1, 0, 0, 0, { 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0 } },
    { 0, 0, 0, 0, -1, 1, 8, 0x1.d03f2439bf325p+11, { 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0 }, 0x0p+0, -0x1p+0, -0x1p+0, 0, 0, -1, -1, -1, 0, 0, 0, { 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0 } },
    { 0, 0, 0, 0, -1, 7, 9, 0x1.063ca9dea32d4p+11, { 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0 }, 0x0p+0, -0x1p+0, -0x1p+0, 0, 0, -1, -1, -1, 0, 0, 0, { 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0 } },
    { 13, 0, 0, 0, -1, 9, 0, 0x0p+0, { 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0, 0x0p+0 }, 0x0p+0, -0x1p+0, -0x1p+0, 0, 0, 8, -1, -1, 0, 5, -1, { 0x1.36a559c836f97p-45, 0x1.43e272931a4dp+6, 0x1.0949842b227c1p-1, 0x1.7b485029fb3a3p+7, 0x0p+0, 0x1.65835b308e12ep-1 } },
};

static void ti_caso(const SimEl *el, int nel, int nn, int nodo, int ki)
{
    sr_init(nn);
    sr.nel = nel;
    memcpy(sr.el, el, sizeof(SimEl) * (size_t)nel);
    canal_de(&saj, &sr, 0, CANAL_NODO, nodo); canal_de(&saj, &sr, 1, CANAL_I, ki);
}

/* Una red de [67] con sus ajustes (con o sin el corte): lo real */
static double ti_emula_red(const SimRed *s, const SimAjustes *aj, int sin_corte)
{
    SimAjustes a = *aj;
    double real;
    sim_gasto_sin_corte(sin_corte);
    sim_ops_reset();
    if (sim_iniciar_red(s, &a) != SIM_OK) { sim_gasto_sin_corte(0); return -1.0; }
    while (sim_avanzar(~0ul)) {}
    real = kc_real();
    sim_gasto_sin_corte(0);
    return real;
}

/* I24. La rejilla de reserva, de la escala de lo emulado (antes, de la
 * década de t_fin: con 1 s o más, de 0,1 s o más, y lo caro que llegaba al
 * final no se cortaba, o se cortaba lejos del objetivo) */
static void ti_rejilla(void)
{
    const char *q = "[75i] I24 rejilla";
    static const double T1[3] = { 0.9, 1.8, 1.2 }, TF[3] = { 1.0, 2.0, 0.0 }, TB[2] = { 10.0, 101.0 };
    char det[240];
    const SimRes *r;
    double real;
    int i;
    for (i = 0; i < 3; i++) {
        ti_k9(T1[i]);
        real = ti_emula(TF[i], ~0ul);
        r = sim_res();
        sprintf(det, "K9 con el S en %.2g s de %.3g%s: t_fin %.4g, en %.2f s (antes, ~7 s)", T1[i], r->t_pedido,
                TF[i] > 0.0 ? "" : " (la autom" S_aa "tica)", r->t_fin, real);
        printf("    %s\n", det);
        chk_cond(q, det, r->fin == FIN_COMPLETO && r->t_fin < r->t_pedido && real <= 3.3);
    }
    for (i = 0; i < 2; i++) {
        kc_puente(TB[i], ~0ul);
        r = sim_res(); real = kc_real();
        sprintf(det, "el puente con %.3g s pedidos: t_fin %.4g, en %.2f s (antes, 3,6 y 4,5)", TB[i], r->t_fin, real);
        printf("    %s\n", det);
        chk_cond(q, det, r->fin == FIN_COMPLETO && real <= 3.3);
    }
    /* la primera de la rejilla tras lo emulado con un periodo entero detrás
     * se calcula (un salto), no se busca de punto en punto con un tope:
     * caso_245 (se dispara a ~0,58 ms) con una senoidal de 5 kHz en su propio
     * lazo (T = 0,2 ms: 200 puntos de la rejilla de 1 µs). Sin el salto, no
     * se cortaba hasta los 37 ms (6 s) */
    ti_caso(TI_CASO245, 15, 11, 7, 1);
    sr.nn = 12;
    t4_seno(11, 0, 1.0, 5000.0, 0.0);
    sr_el(SE_R, 11, 0, 1e3);
    real = ti_emula(0.05, ~0ul);
    r = sim_res();
    sprintf(det, "caso_245 con una senoidal de 5 kHz aparte (50 ms): se corta un periodo tras lo emulado, a %.4g ms (la medida "
            "desde %.4g), en %.2f s", r->t_fin * 1e3, r->t_med0 * 1e3, real);
    printf("    %s\n", det);
    chk_cond(q, det, r->fin == FIN_COMPLETO && r->t_fin < 1e-3 && fabs((r->t_fin - r->t_med0) / 2e-4 - 1.0) < 1e-6);
}

/* I25. Pedir más ventana: el puente de 0,3 a 1,1 s nunca da menos (con la
 * rejilla de t_fin, 0,99 daba 0,37 y 1 daba 0,3), salvo un par de puntos de
 * la rejilla (2 %: con otro t_fin, elige_paso planea otro paso, y lo emulado
 * no es lo mismo; 0,33 da 0,304 y 0,4, 0,303). caso_245 se dice, sin
 * exigirlo: con más ventana elige_paso planea un h que cuesta cientos de
 * veces más por segundo emulado (queda para el punto 19, con rev_cal 6) */
static void ti_pedir_mas(void)
{
    const char *q = "[75i] I25 pedir m" S_aa "s";
    static const double TP[] = { 0.3, 0.33, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 0.99, 1.0, 1.1 };
    char det[240];
    double ant = 0.0, tf, real, tf42, tf70, r42, r70;
    int i, mal = 0;
    for (i = 0; i < (int)(sizeof TP / sizeof TP[0]); i++) {
        kc_puente(TP[i], ~0ul);
        tf = sim_res()->t_fin; real = kc_real();
        printf("    el puente con %.3g s pedidos: %.4g s en %.2f s\n", TP[i], tf, real);
        if (tf < ant * (1.0 - 0.02) || real > 3.3) mal++;
        if (tf > ant) ant = tf;
    }
    sprintf(det, "el puente de 0,3 a 1,1 s: pedir m" S_aa "s no da menos (a un 2 %%), y ninguna pasa de 3,3 s (%d mal)", mal);
    chk_cond(q, det, mal == 0);
    ti_caso(TI_CASO245, 15, 11, 7, 1);
    r42 = ti_emula(0.042, ~0ul); tf42 = sim_res()->t_fin;
    ti_caso(TI_CASO245, 15, 11, 7, 1);
    r70 = ti_emula(0.07, ~0ul); tf70 = sim_res()->t_fin;
    printf("    caso_245 de [67] (sin exigirlo: el plan de elige_paso, punto 19): 42 ms pedidos, %.4g s en %.2f s; 70 ms, %.4g s en %.2f s\n",
           tf42, r42, tf70, r70);
}

/* I26. El corte, con el ritmo de todo lo gastado desde la mirada de antes
 * además del de la segunda mitad: un pico de medio segundo no corta una
 * ventana que entera cabe (caso_258 con los 30 ms de [67]: 2,5 s entera,
 * y se cortaba a 22 ms). Y las 60 redes de [67] en modo calculadora:
 * ninguna que entera cabe en GC_DISPARA (3 s) se corta */
static void ti_ritmo(void)
{
    const char *q = "[75i] I26 ritmo";
    char det[240];
    const SimRes *r;
    double real, real2;
    int i, cortadas = 0, mal = 0;
    ti_caso(TI_CASO258, 15, 10, 4, 3);
    sim_gasto_sin_corte(1);
    real2 = ti_emula(0.03, ~0ul);
    sim_gasto_sin_corte(0);
    ti_caso(TI_CASO258, 15, 10, 4, 3);
    real = ti_emula(0.03, ~0ul);
    r = sim_res();
    sprintf(det, "caso_258 con 30 ms (entera, %.2f s): no se corta (t_fin %.4g, %.2f s)", real2, r->t_fin, real);
    chk_cond(q, det, real2 <= 3.0 && r->fin == FIN_COMPLETO && r->t_fin == 0.03);
    if (!ti_ora_n) { printf("    (sin [67] en esta corrida: sus redes no se miran)\n"); return; }
    for (i = 0; i < ti_ora_n; i++) {
        double entera = ti_emula_red(&ti_ora_red[i], &ti_ora_aj[i], 1), tf = sim_res()->t_fin, con;
        con = ti_emula_red(&ti_ora_red[i], &ti_ora_aj[i], 0);
        if (sim_res()->t_fin < tf * (1.0 - 1e-9)) {        /* la del control (elige_paso también acorta) */
            cortadas++;
            printf("    caso_%03d de [67]: %.4g s de %.4g (entera, %.2f s; cortada, %.2f s)\n", ORA_NCC + i, sim_res()->t_fin, tf,
                   entera, con);
            if (entera <= 3.0) mal++;                       /* GC_DISPARA */
        }
    }
    sprintf(det, "las %d redes de [67] con el corte: %d cortadas, %d que enteras cab" S_ii "an en 3 s", ti_ora_n, cortadas, mal);
    printf("    %s\n", det);
    chk_cond(q, det, ti_ora_n == ORA_NTR && mal == 0);
}

/* I27. Con fuentes periódicas, la ventana de medida es de periodos enteros:
 * si tras lo emulado no cabe ninguno antes del final, no se corta (cortar
 * dejaría la medida en medio periodo). La cota es GC_DISPARA más un periodo
 * al ritmo del final, no «~3 s» (caso_258 con 90 ms) */
static void ti_periodos(void)
{
    const char *q = "[75i] I27 periodos";
    char det[240];
    const SimRes *r;
    double real, T = 1.0 / TI_CASO258[0].p[1], n;
    ti_caso(TI_CASO258, 15, 10, 4, 3);
    real = ti_emula(0.09, ~0ul);
    r = sim_res();
    n = (r->t_fin - r->t_med0) / T;
    sprintf(det, "caso_258 con 90 ms: t_fin %.4g, la medida desde %.4g (%.3g periodos del seno), en %.2f s", r->t_fin, r->t_med0, n,
            real);
    printf("    %s\n", det);
    chk_cond(q, det, r->fin == FIN_COMPLETO && n >= 1.0 - 1e-9 && fabs(n - floor(n + 0.5)) <= 1e-6 && real <= 4.6);
}

/* I28. El aviso PASO_LIM dice lo que pasó: la ventana, el paso o los dos
 * (antes, con los dos, solo la ventana) */
static SimRes ti_res;
static void ti_aviso(void)
{
    const char *q = "[75i] I28 aviso";
    char det[240], txt[120];
    const SimRes *r;
    kc_puente(1.0, ~0ul);
    r = sim_res();
    sim_aviso(0, r, SIM_AV_PASO_LIM, txt, (int)sizeof txt);
    sprintf(det, "el puente con 1 s: h %.3g (T/100 = %.3g) y t_fin %.3g: \"%.60s\"", r->h, (1.0 / 60.0) / 100.0, r->t_fin, txt);
    chk_cond(q, det, r->h > 1.5 * (1.0 / 60.0) / 100.0 && r->t_fin < r->t_pedido && strstr(txt, "ventana") && strstr(txt, "paso") &&
             strlen(txt) <= 52);
    /* los tres casos, con la misma emulación: solo la ventana, solo el paso */
    ti_res = *r;
    ti_res.paso_mayor = 0;
    sim_aviso(0, &ti_res, SIM_AV_PASO_LIM, txt, (int)sizeof txt);
    sprintf(det, "solo la ventana: \"%.60s\"", txt);
    chk_cond(q, det, strstr(txt, "La ventana se acort") != 0 && !strstr(txt, "paso") && strlen(txt) <= 52);
    ti_res.paso_mayor = 1;
    ti_res.t_fin = ti_res.t_pedido;
    sim_aviso(0, &ti_res, SIM_AV_PASO_LIM, txt, (int)sizeof txt);
    sprintf(det, "solo el paso: \"%.70s\"", txt);
    chk_cond(q, det, strstr(txt, "El paso se agrand") != 0 && !strstr(txt, "ventana"));
}

/* I29. Las operaciones de los pasos (las del control del gasto) no dependen
 * de lo emulado antes: residuo_mono (el del arnés) no cuenta */
static void ti_ops_historia(void)
{
    const char *q = "[75i] I29 ops";
    char det[240];
    unsigned long o1, o2, o3;
    long p1, p2, p3;
    sim_residuo_acum_reset();
    ti_caso(TI_CASO258, 15, 10, 4, 3);
    ti_emula(0.09, ~0ul); o1 = sim_ops(); p1 = sim_res()->pasos;
    ti_caso(TI_CASO258, 15, 10, 4, 3);
    ti_emula(0.09, ~0ul); o2 = sim_ops(); p2 = sim_res()->pasos;
    kc_puente(0.3, ~0ul);
    ti_caso(TI_CASO258, 15, 10, 4, 3);
    ti_emula(0.09, ~0ul); o3 = sim_ops(); p3 = sim_res()->pasos;
    sprintf(det, "caso_258 (90 ms) tras sim_residuo_acum_reset, otra vez y tras el puente: %lu, %lu y %lu operaciones (%ld, %ld y %ld pasos)",
            o1, o2, o3, p1, p2, p3);
    chk_cond(q, det, o1 == o2 && o2 == o3 && p1 == p2 && p2 == p3);
}

/* I30. El presupuesto de sim_avanzar y bode_avanzar, por diferencia: en la
 * calculadora el contador es de 32 bits y da la vuelta; con él cerca, el
 * tope de antes se saturaba y el trozo corría hasta el final, sin volver a
 * la interfaz ni mirar [esc] */
static void ti_vuelta(void)
{
    const char *q = "[75i] I30 vuelta del contador";
    static Circuito c;
    static Red rd;
    int sigue, kE, kC;
    banco_puente(&c, &rd);
    c.t_fin.v = 1.0; c.t_fin.est = M_DATO;
    c.paso.v = (1.0 / 60.0) / 45.0; c.paso.est = M_DATO;   /* a mano: sin el tope de tiempo */
    sim_iniciar(&c, &rd);
    sim_ops_pon(~0ul - 1000ul);
    sigue = sim_avanzar(150000ul);
    chk_cond(q, "sim_avanzar(150000) con el contador a 1000 de su vuelta: vuelve tras ~150000, a medias",
             sigue && sim_progreso() < 0.5);
    sim_cortar();
    sim_ops_reset();
    sr_init(3);
    kE = a4_cc(0, 1, 1.0); sr_el(SE_R, 1, 2, 1e3); kC = sr_el(SE_C, 2, 0, 1e-6);
    canal(0, CANAL_V, kC);
    bode_iniciar_red(&sr, kE, &saj.canal[0], -1, -1, 10.0, 1e5, 0.0);
    ca_ops_pon(~0ul - 1000ul);
    sigue = bode_avanzar(2000ul);
    chk_cond(q, "bode_avanzar(2000) con el contador a 1000 de su vuelta: vuelve tras unos puntos, a medias",
             sigue && bode_progreso() < 0.5);
    bode_cortar();
    ca_ops_reset();
}

/* ----------------------------------------------------------------------- */
static void test_opt_final(void)
{
    printf("[75i] La integraci" S_oo "n de rev9 (fase 1): lo que encontraron las revisiones\n");
    gfx_desbordes_reset();
    ti_so_tarda();
    ti_de_quien();
    ti_teclas();
    ti_sin_reloj();
    ti_marca();
    ti_esc_medida();
    ti_prueba_puerta();
    ti_fnv();
    ti_mono();
    ti_gasto();
    ti_fila_pares();
    ti_vistos();
    ti_banco();
    ti_fuzz();
    ti_sin_confirmar();
    ti_mono_pmu();
    ti_cpu();
    ti_esc_medida_m();
    ti_mhz_cero();
    ti_rejilla();
    ti_pedir_mas();
    ti_ritmo();
    ti_periodos();
    ti_aviso();
    ti_ops_historia();
    ti_vuelta();
    chk_cond("[75i] integraci" S_oo "n", "los textos caben (52 columnas: ninguno se sale)", gfx_desbordes() == 0);
    /* como estaba: «no es una CX II», sin ces_controles.tns */
    turbo_hw(0);
    turbo_salida(0);
    turbo_sim_reinicia();
    turbo_sim()->cx2 = 0;
    arch_bytes_borrar("/ces_controles.tns");
    ent_grabacion_olvida();
    ent_ajustes_omision();
    ui_init();
}
