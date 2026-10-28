/* --------------------------------------------------------------------------
 *  65l. Etapa 4: hallazgos de la segunda revisión (pequeña señal, potencias
 *  del emulador y pantallas de resultados)
 *
 *  Una prueba por hallazgo confirmado (cada una falla sin su arreglo,
 *  comprobado por mutación): los avisos SATURA y PEQ_SENAL en el Bode y en
 *  P_CA (y el AO que de verdad satura), la P media y la P máxima del
 *  emulador con el segundo puerto del BJT y del AO, la región en Q en la
 *  tabla de P_CA (y no Z = 0), el triángulo de Boucherot con las fuentes
 *  controladas, el pie «[I] barrer» con un [?!] que no se barre y las filas
 *  de carga y flujo de P_TABLA con el punto de operación. Se incluye tras
 *  test_e4_rev.inc (usa r4_led, r4_el, r4_espera, r3u_abre, a4_*, q4_bjt,
 *  sr_*, canal, emula, ve y TECLAS).
 * ----------------------------------------------------------------------- */

/* El LED de r4_led con un C en paralelo: el cable (2,1)-(3,1), C1 de (3,1) a
 * (3,2) y el cable (3,2)-(2,2). Devuelve C1 */
static int rv2_led_c(Circuito *c)
{
    int k;
    r4_led(c, 9.0, 330.0);
    circ_cable(c, 2, 1, DIR_DER);
    k = circ_elemento(c, EL_COND, 3, 1, DIR_ABA);
    circ_cable(c, 3, 2, DIR_IZQ);
    return k;
}

/* ¿P media y P máxima del elemento k cuadran con su P al final? (en CC,
 * sin transitorio: las tres son la misma) */
static int rv2_p_cuadra(int k)
{
    const SimRes *r = sim_res();
    double P = r->ultimo.P[k], tol = 1e-9 * fabs(P);
    return P != 0.0 && fabs(r->Pmed[k] - P) <= tol && fabs(r->Pmax[k] - P) <= tol &&
           fabs(r->W[k] / r->t_fin - P) <= 1e-6 * fabs(P);
}

static void test_rev2_ca_res(void)
{
    static Circuito c;
    static CaRes rv2_car;
    static SimLin L;
    static Fasorial fz;
    char t[160], u[48];
    int k, kU, kE2, kR1, kRf, j, np;
    double P;

    printf("[65l] Etapa 4: hallazgos de la segunda revisi" S_oo "n (CA, potencias y resultados)\n");
    gfx_desbordes_reset();

    /* ---- 1. El Bode y P_CA con no lineales: PEQ_SENAL y, con un AO
     *      saturado en Q, SATURA (que explica la H nula) ---- */
    modelo_cargar(37, &c);                                   /* el comparador */
    r3u_abre(&c);
    TECLAS(T_LETRA('B'));
    r4_espera();
    chk_cond("Bode SATURA", "el comparador: P_BODE dice que U1 satura en Q", ui_pantalla() == P_BODE &&
             (bode_res()->d.avisos & SIM_AV_SATURA) && ve("U1 satura en Q") && strlen(ui_trazador()->aviso) <= 52);
    TECLAS(T_TAB);
    chk_cond("Bode SATURA", "[tab]: P_CA tambi" S_ee "n lo dice", ui_pantalla() == P_CA &&
             (ui_ca()->d.avisos & SIM_AV_PEQ_SENAL) && ve("U1 satura en Q"));
    modelo_cargar(35, &c);                                   /* el integrador: C abierto en Q */
    r3u_abre(&c);
    TECLAS(T_LETRA('B'));
    r4_espera();
    chk_cond("Bode SATURA", "el integrador: SATURA en P_BODE", ui_pantalla() == P_BODE && ve("U1 satura en Q"));
    modelo_cargar(41, &c);                                   /* el emisor común: sin AO */
    r3u_abre(&c);
    TECLAS(T_LETRA('B'));
    r4_espera();
    chk_cond("Bode SATURA", "el emisor com" S_uu "n: PEQ_SENAL en P_BODE", ui_pantalla() == P_BODE &&
             ve("Peque" S_nn "a se" S_nn "al: no lineales linealizados en Q."));
    /* el AO que satura es el que se nombra: U1 seguidor (lineal), U2 compara
     * 1 V con 2 V (satura) */
    sr_init(5);
    a4_cc(0, 1, 1.0);
    a4_ao(1, 2, 2, 0, 0, 0.0);
    a4_cc(0, 3, 2.0);
    kU = a4_ao(2, 3, 4, 0, 0, 0.0);
    sr_el(SE_R, 4, 0, 1e3);
    memset(&rv2_car, 0, sizeof rv2_car);
    rv2_car.d.elem = rv2_car.d.nodo = rv2_car.d.elem_aviso = -1;
    circ_init(&c, "dos AO");
    c.nel = sr.nel;
    for (k = 0; k < sr.nel; k++) {
        static const char *const NOM[5] = { "E1", "U1", "E2", "U2", "R1" };
        c.el[k].tipo = (unsigned char)(sr.el[k].tipo == SE_AO ? EL_AO : sr.el[k].tipo == SE_E ? EL_FUENTE : EL_RES);
        strcpy(c.el[k].etiq, NOM[k]);
    }
    chk_cond("Bode SATURA", "ca_punto_q: SATURA con U2 en elem_aviso", ca_punto_q(&sr, &L, &rv2_car.d) == SIM_OK &&
             (rv2_car.d.avisos & SIM_AV_SATURA) && rv2_car.d.elem_aviso == kU);
    ca_aviso(&c, &rv2_car, SIM_AV_SATURA, t, sizeof t);
    chk_cond("Bode SATURA", "el texto nombra a U2, en 52 columnas", strncmp(t, "U2 satura en Q", 14) == 0 && strlen(t) <= 52);
    {
        SimCanal ch;
        memset(&ch, 0, sizeof ch);
        ch.tipo = CANAL_NODO; ch.nodo = 4; ch.elem = -1; ch.sg = 1;
        bode_iniciar_red(&sr, 0, &ch, -1, -1, 0.0, 0.0, 0.0);
        chk_cond("Bode SATURA", "el Bode: U2 en elem_aviso", (bode_res()->d.avisos & SIM_AV_SATURA) && bode_res()->d.elem_aviso == kU);
    }

    /* ---- 2. P media y P máxima del emulador con los dos puertos ---- */
    sr_init(5);                                              /* el NPN de interruptor, saturado */
    a4_cc(0, 1, 5.0); sr_el(SE_R, 1, 2, 4.7e3);
    a4_cc(0, 3, 9.0); sr_el(SE_R, 3, 4, 330.0);
    k = q4_bjt(4, 2, 0, 1, 100.0, 0.0);
    sr_el(SE_C, 4, 0, 1e-9);                                 /* sin V0: arranca en su CC */
    canal(0, CANAL_NODO, 4); canal(1, CANAL_NADA, 0);
    saj.t_fin = 1e-3; saj.h = 0.0;
    emula("potencias");
    printf("    BJT: P al final %.9g, P media %.9g, P m" S_aa "x %.9g, W/t %.9g\n", sim_res()->ultimo.P[k],
           sim_res()->Pmed[k], sim_res()->Pmax[k], sim_res()->W[k] / sim_res()->t_fin);
    chk_cond("potencias", "BJT: P media = P m" S_aa "x = P al final = W/t (con VBE" S_CDOT "IB)", rv2_p_cuadra(k));
    k = a4_inversor(0.5, 1, 100.0, &kR1, &kRf);              /* AO real: A 100, Ri 1 k, Ro 0 */
    sr.el[k].m[3] = 1e3;
    sr_el(SE_C, 3, 0, 1e-9);
    sr_el(SE_R, 3, 0, 2e3);
    canal(0, CANAL_NODO, 3); canal(1, CANAL_NADA, 0);
    saj.t_fin = 1e-3; saj.h = 0.0;
    emula("potencias");
    printf("    AO: P al final %.9g, P media %.9g, P m" S_aa "x %.9g, W/t %.9g\n", sim_res()->ultimo.P[k],
           sim_res()->Pmed[k], sim_res()->Pmax[k], sim_res()->W[k] / sim_res()->t_fin);
    chk_cond("potencias", "AO: P media = P m" S_aa "x = P al final = W/t (menos Vd" S_CDOT "Ient)", rv2_p_cuadra(k));

    /* ---- 3. La tabla de P_CA: la región en Q de cada no lineal, no Z = 0 ---- */
    num_fasor(u, sizeof u, 0.0, 0.0, 0, 0, S_OHM);          /* lo que era z_txt(0) */
    modelo_cargar(22, &c);                                   /* el 1N4148 con 1 kΩ */
    k = r4_el(&c, "E1");
    c.el[k].variante = ONDA_SENO;
    c.el[k].val.v = 0.1; c.el[k].val.est = M_DATO;
    c.el[k].par[Q_FREC - Q_P1].v = 1e3; c.el[k].par[Q_FREC - Q_P1].est = M_DATO;
    c.el[k].par[Q_BASE - Q_P1].v = 5.0; c.el[k].par[Q_BASE - Q_P1].est = M_DATO;
    r3u_abre(&c);
    TECLAS(T_LETRA('B'));
    r4_espera();
    TECLAS(T_TAB, T_TAB);
    k = r4_el(ui_circuito(), "D1");
    chk_cond("tabla de P_CA", "el diodo: «conduce» y no «0,000" S_ANG "0,0" S_DEG " " S_OHM "»", ui_pantalla() == P_CA_TABLA &&
             ui_ca()->reg[k] == REG_CONDUCE && ve("\nconduce\n") && !ve(u));
    modelo_cargar(41, &c);                                   /* el emisor común */
    r3u_abre(&c);
    TECLAS(T_LETRA('B'));
    r4_espera();
    TECLAS(T_TAB, T_TAB);
    chk_cond("tabla de P_CA", "el BJT: «activa» y no Z = 0", ui_pantalla() == P_CA_TABLA && ve("\nactiva\n") && !ve(u));

    /* ---- 4. Boucherot con una E controlada: entrega, y la cadena de las
     *      cargas acaba en el total ---- */
    modelo_cargar(39, &c);                                   /* el amplificador con E2 = 100·V(R2) */
    k = r4_el(&c, "E1");
    c.el[k].variante = FORMA_FASOR;
    c.el[k].val.v = 10e-3; c.el[k].val.est = M_DATO; c.el[k].val.ang = 0.0; c.el[k].val.est_ang = M_DATO;
    c.frec.v = 1e3; c.frec.est = M_DATO;
    kE2 = r4_el(&c, "E2");
    r3u_abre(&c);
    TECLAS(T_IGUAL);
    P = ui_ca()->Sfuentes.re;
    fz_nuevo(&fz, &c, ui_ca());
    fz.vista = FZ_POTENCIAS; fz.cadena |= FZ_BOUCHEROT;
    printf("    Boucherot: P total %.9g W; escala %.9g px/W (170/P = %.9g)\n", P, fz_escala_pot(&fz, &c, ui_ca()), 170.0 / P);
    chk_cond("Boucherot", "P_CA del modelo 39: 0,81 mW", ui_pantalla() == P_CA && ui_ca()->d.estado == SIM_OK && fabs(P - 0.81e-3) <= 1e-7);
    chk_t("Boucherot", "la cadena acaba en el total: escala 170/P", fz_escala_pot(&fz, &c, ui_ca()), 170.0 / P, 170.0 / P, 1e-12);
    TECLAS(T_ENTER, T_DER, T_LETRA('S'));
    np = fz_npot(&c, ui_ca());
    for (j = 1; j <= np && fz_pot_el(&c, ui_ca(), j) != kE2; j++) ;
    repite(T_ABAJO, j);
    chk_cond("Boucherot", "E2 entrega (no absorbe)", ui_pantalla() == P_FASORIAL && j <= np && ve("E2 entrega") && !ve("E2 absorbe"));

    /* ---- 5. SOL_NO_LINEAL con un [?!] en una C: el pie no ofrece barrerla ---- */
    k = rv2_led_c(&c);
    c.el[k].val.est = M_BUSCAR;
    r3u_abre(&c);
    TECLAS(T_IGUAL);
    chk_cond("[I] barrer", "C1 con [?!]: el pie no promete «[I] barrer C1»", ui_pantalla() == P_RESULTADO &&
             !ve("[I] barrer") && ve("[esc] volver y corregirlo"));
    k = rv2_led_c(&c);
    c.el[k].val.est = M_BUSCAR;
    c.el[r4_el(&c, "R1")].val.est = M_BUSCAR;
    r3u_abre(&c);
    TECLAS(T_IGUAL);
    chk_cond("[I] barrer", "C1 y R1 con [?!]: «[I] barrer R1»", ui_pantalla() == P_RESULTADO && ve("[I] barrer R1"));

    /* ---- 6. P_TABLA del punto de operación: la fila de la carga de C1 ---- */
    k = rv2_led_c(&c);
    c.el[k].val.v = 100e-6; c.el[k].val.est = M_DATO;
    r3u_abre(&c);
    TECLAS(T_LETRA('P'), T_TAB);
    printf("    C1: V = %.9g V, Q = %.9g C\n", ui_valores()->V[k], 100e-6 * ui_valores()->V[k]);
    chk_cond("P_TABLA", "[P] [tab]: C1 con su Q y su W", ui_pantalla() == P_TABLA && ui_res_nl() &&
             ve("\nQ\n") && ve("\nW\n") && ve("Newton: "));

    /* ---- Lo que no es un fallo: un valor vacío se pide con el texto de
     *      sim_mensaje_circ (§4.5), que nombra lo que falta ---- */
    k = rv2_led_c(&c);                                       /* C1 vacía: [P] la pide */
    r3u_abre(&c);
    TECLAS(T_LETRA('P'));
    chk_cond("valor vac" S_ii "o", "[P] con C1 vac" S_ii "a: pide C en C1", ui_pantalla() == P_RESULTADO && ve("falta C en C1"));
    r4_led(&c, 9.0, 330.0);                                  /* R1 vacía: [=] y [I] la piden */
    c.el[r4_el(&c, "R1")].val.est = M_LIBRE;
    r3u_abre(&c);
    TECLAS(T_IGUAL);
    chk_cond("valor vac" S_ii "o", "[=] con R1 vac" S_ii "a: pide R en R1", ui_pantalla() == P_RESULTADO && ve("falta R en R1"));
    TECLAS(T_ESC, T_LETRA('I'));
    chk_cond("valor vac" S_ii "o", "[I] con R1 vac" S_ii "a: pide R en R1", ui_pantalla() == P_BARRIDO && ve("falta R en R1"));

    chk_cond("interfaz", "ning" S_uu "n texto fuera de la pantalla", gfx_desbordes() == 0);
}
