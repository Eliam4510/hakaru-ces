/* --------------------------------------------------------------------------
 *  65s. Etapa 4: el transitorio (tercera revisión, grupo tran_b)
 *
 *  Una prueba por hallazgo (cada una falla sin su arreglo, comprobado por
 *  mutación): el paso automático sin C ni L no depende de la emulación
 *  anterior; la página «Energía» del emulador cuenta el AO y las
 *  controladas como entregadas; un AO que acaba saturado da SATURA con la
 *  tensión en que satura; BIESTABLE (y ARRANQUE) se ven antes que SATURA
 *  del mismo AO. Se incluye tras test_rev2_editor.inc (usa sr, saj, sr_init,
 *  sr_el, sr_escalon, canal, t4_seno, t4_d_cat, t4_c0, a4_ao, r3u_abre,
 *  r4_espera, r4_el, rd2_al_esq, ve, TECLAS y chk_cond). Se corre sola con
 *  HAKARU_SOLO_R3_V_TRAN_B=1.
 * ----------------------------------------------------------------------- */

/* Una red de 30 C entre 8 nodos (su LU cuesta): la emulación «anterior» */
static void r3tb_red_densa(void)
{
    int a, b, k;
    sr_init(8);
    k = sr_el(SE_E, 0, 1, 0.0); sr.el[k].forma = ONDA_SENO; sr.el[k].p[0] = 1.0; sr.el[k].p[1] = 1e3;
    for (a = 0; a < 8; a++)
        for (b = a + 1; b < 8 && sr.nel < 30; b++) sr_el(SE_C, a, b, 1e-9 * (1 + a + b));
    sr_el(SE_R, 7, 0, 1e3);
    canal(0, CANAL_NODO, 7); canal(1, CANAL_NADA, 0);
}

/* Un circuito sin C ni L (10 V a 60 Hz sobre R = 1 kΩ en serie con el
 * 1N4007 si diodo, o con otra R de 1 kΩ), 0,35 s con el paso automático,
 * justo después de emular un RC (antes = 0) o la red densa (antes = 1).
 * Eran 0,5 s: con el banco del cierre (con el USB: NS_ITER 250,5 µs) sus
 * 3000 pasos se estimaban en ~2,5 s y el tope de 2 s agrandaba h; con 0,35
 * s (2100) cabe (1,4 s con el banco sin el USB), y el paso sigue siendo el
 * de la regla, T/100 (por debajo de 1/3 s manda t_fin/2000) */
static void r3tb_sin_cl_tras(int antes, int diodo, double *h, double *t_est, unsigned *avisos)
{
    if (antes) r3tb_red_densa();
    else { sr_init(3); sr_escalon(0, 1, 1.0); sr_el(SE_R, 1, 2, 1e3); t4_c0(2, 0, 1e-6); canal(0, CANAL_NODO, 2); canal(1, CANAL_NADA, 0); }
    sim_emular(&sr, &saj);
    sr_init(3);
    t4_seno(0, 1, 10.0, 60.0, 0.0);
    if (diodo) t4_d_cat(1, 2, &NL_1N4007);
    else sr_el(SE_R, 1, 2, 1e3);
    sr_el(SE_R, 2, 0, 1e3);
    canal(0, CANAL_NODO, 2); canal(1, CANAL_NADA, 0);
    saj.t_fin = 0.35; saj.h = 0.0;
    sim_emular(&sr, &saj);
    *h = sim_res()->h;
    *t_est = sim_res()->t_est;
    *avisos = sim_res()->d.avisos;
}

/* La página «Energía» (4/5) del emulador: la línea de la entregada es la W
 * de las fuentes, las controladas y los AO (el criterio de termina) */
static int r3tb_energia(const char *q, int modelo, void (*cambia)(Circuito *c))
{
    static Circuito c;
    const SimRes *s;
    const Circuito *cu;
    char u[24], lin[32], lin2[32];
    double ent = 0.0, dis = 0.0;
    int j, k, ok;
    modelo_cargar(modelo, &c);
    if (cambia) cambia(&c);
    r3u_abre(&c);
    TECLAS(T_IGUAL);
    r4_espera();
    for (j = 0; j < 6 && ui_pantalla() != P_EMU_TABLA; j++) TECLAS(T_TAB);
    TECLAS(T_DER, T_DER, T_DER);
    s = sim_res();
    cu = ui_circuito();
    for (k = 0; k < cu->nel; k++) {
        int t = cu->el[k].tipo;
        if (t == EL_FUENTE || t == EL_CORR || t == EL_EDEP || t == EL_JDEP || t == EL_AO) ent += s->W[k];
        else if (t != EL_COND && t != EL_BOB && t != EL_INTER) dis += s->W[k];
    }
    sprintf(lin, "\n%s\n", num_pref(u, sizeof u, ent, 4, "J"));      /* la entregada va sola, sin % */
    sprintf(lin2, "\n%s  ", num_pref(u, sizeof u, dis, 4, "J"));     /* la disipada, con su % */
    ok = ui_pantalla() == P_EMU_TABLA && ve("Entregada por las fuentes") && ve(lin) && ve(lin2);
    printf("    %s: en P_EMU_TABLA %d; entregada %.6g J, disipada %.6g J, dif_balance %.3g J; se ven %d %d\n", q,
           ui_pantalla() == P_EMU_TABLA, ent, dis, s->dif_balance, ve(lin), ve(lin2));
    TECLAS(T_ESC);
    return ok && ent != 0.0;
}

/* El amplificador con fuente controlada (40) con E1 senoidal (10 mV a
 * 1 kHz): así [=] lo emula */
static void r3tb_controlada_seno(Circuito *c)
{
    int k = r4_el(c, "E1");
    c->el[k].variante = ONDA_SENO;
    c->el[k].par[Q_FREC - Q_P1].v = 1e3; c->el[k].par[Q_FREC - Q_P1].est = M_DATO;
    c->el[k].par[Q_BASE - Q_P1].v = 0.0; c->el[k].par[Q_BASE - Q_P1].est = M_DATO;
}

/* P_EMU_ESQ de un modelo (con cambia() aplicado) */
static void r3tb_al_esq(int modelo, void (*cambia)(Circuito *c))
{
    static Circuito c;
    modelo_cargar(modelo, &c);
    if (cambia) cambia(&c);
    r3u_abre(&c);
    TECLAS(T_IGUAL);
    r4_espera();
    rd2_al_esq();
}

/* El integrador (36) con E1 continua de 1 V, 20 ms y paso de 100 µs */
static void r3tb_integrador_cc(Circuito *c)
{
    c->el[0].variante = ONDA_CC; c->el[0].val.v = 1.0; c->el[0].val.est = M_DATO;
    c->t_fin.v = 20e-3; c->t_fin.est = M_DATO;
    c->paso.v = 1e-4; c->paso.est = M_DATO;
}

/* El astable (39) con la V0 del C = 0 dato: BIESTABLE en el OP (§6.8) */
static void r3tb_astable_v0(Circuito *c)
{
    int k = r4_el(c, "C1");
    c->el[k].par[Q_V0 - Q_P1].v = 0.0; c->el[k].par[Q_V0 - Q_P1].est = M_DATO;
}

/* Un Schmitt: el astable (39) con una E continua de 0 V en lugar de C1 (la
 * entrada, en la -); sin C, [P] y [=] dan su punto de operación */
static void r3tb_schmitt(Circuito *c)
{
    int k;
    for (k = 0; k < 2; k++) { c->sonda[k].tipo = SONDA_NO; c->sonda[k].x = c->sonda[k].y = -1; c->sonda[k].el = -1; c->sonda[k].sg = 0; }
    circ_quitar(c, 2, 2, DIR_ABA);                         /* C1, de (2, 2) a (2, 3) */
    k = circ_elemento(c, EL_FUENTE, 2, 3, DIR_ARR);        /* el + arriba, en la - */
    c->el[k].variante = ONDA_CC; c->el[k].val.v = 0.0; c->el[k].val.est = M_DATO;
}

static void test_rev3_tran_b(void)
{
    char q[120];
    int k;

    printf("[65s] Etapa 4: el transitorio (tercera revisi" S_oo "n, grupo tran_b)\n");
    gfx_desbordes_reset();

    /* ---- H4. Sin C ni L, el paso y t_est no dependen de lo emulado antes ---- */
    {
        double h0, h1, e0, e1;
        unsigned a0, a1;
        r3tb_sin_cl_tras(0, 1, &h0, &e0, &a0);
        r3tb_sin_cl_tras(1, 1, &h1, &e1, &a1);
        printf("    media onda 1N4007, 0,35 s: tras un RC h %.6g t_est %.4g s avisos %#x; tras 30 C h %.6g t_est %.4g s avisos %#x\n",
               h0, e0, a0, h1, e1, a1);
        chk_cond("paso sin C ni L", "media onda: el mismo h y el mismo t_est venga de donde venga", h0 == h1 && e0 == e1);
        chk_cond("paso sin C ni L", "media onda: 100 pasos por periodo, sin PASO_LIM", fabs(h0 - 1.0 / 6000.0) < 1e-15 &&
                 !(a0 & SIM_AV_PASO_LIM) && !(a1 & SIM_AV_PASO_LIM));
        r3tb_sin_cl_tras(0, 0, &h0, &e0, &a0);
        r3tb_sin_cl_tras(1, 0, &h1, &e1, &a1);
        printf("    divisor lineal, 0,35 s: tras un RC h %.6g t_est %.4g s; tras 30 C h %.6g t_est %.4g s\n", h0, e0, h1, e1);
        chk_cond("paso sin C ni L", "divisor lineal: el mismo h y el mismo t_est", h0 == h1 && e0 == e1);
    }

    /* ---- H5. La página «Energía»: el AO y las controladas entregan ---- */
    chk_cond("energ" S_ii "a", "astable (39): entregada = la W del AO", r3tb_energia("astable (39)", 38, 0));
    chk_cond("energ" S_ii "a", "controlada (40): entregada = E1 + E2", r3tb_energia("controlada (40)", 39, r3tb_controlada_seno));

    /* ---- H6. Un AO que acaba saturado: SATURA, con su tensión ---- */
    for (k = 0; k < 2; k++) {                          /* k = 1: con un C de más que da CI_SALTA en el OP */
        int kE, kU, kC = -1;
        char t[96];
        sr_init(4);
        kE = sr_el(SE_E, 0, 1, 0.0); sr.el[kE].forma = ONDA_PULSO; sr.el[kE].p[1] = 1.0;
        if (k) { kC = sr_el(SE_C, 1, 0, 1e-6); sr.el[kC].ci = 1; sr.el[kC].x0 = 2.0; }   /* sobre E1, V0 = 2 V */
        sr_el(SE_R, 1, 2, 10e3);
        t4_c0(2, 3, 100e-9);
        kU = a4_ao(0, 2, 3, 0, 0, 0.0);
        canal(0, CANAL_NODO, 3); canal(1, CANAL_NADA, 0);
        saj.h = 1e-4; saj.t_fin = 20e-3;
        sim_emular(&sr, &saj);
        sim_aviso(0, sim_res(), SIM_AV_SATURA, t, sizeof t);
        printf("    integrador (sr)%s: regi" S_oo "n final %d, avisos %#x, elem_aviso %d: %s\n", k ? " y un C con CI_SALTA" : "",
               sim_res()->ultimo.reg[kU], sim_res()->d.avisos, sim_res()->d.elem_aviso, t);
        chk_cond("SATURA al final", k ? "integrador (sr) con CI_SALTA de un C: SATURA nombra al AO, no al C" :
                                        "integrador (sr): acaba en SAT- con el aviso SATURA de U1",
                 sim_res()->d.estado == SIM_OK && sim_res()->ultimo.reg[kU] == REG_SAT_MENOS &&
                 (sim_res()->d.avisos & SIM_AV_SATURA) && sim_res()->d.elem_aviso == kU &&
                 (!k || (sim_res()->d.avisos & SIM_AV_CI_SALTA)) && t[0] == 'U' &&
                 strstr(t, " satura en -15 V: la salida ya no sigue.") != 0);
    }
    r3tb_al_esq(35, r3tb_integrador_cc);
    k = r4_el(ui_circuito(), "U1");
    printf("    integrador (36) 1 V, 20 ms: U1 al final %.4g V, aviso SATURA %d, «satura en -15 V» %d\n",
           k >= 0 ? sim_res()->ultimo.V[k] : 0.0, (sim_res()->d.avisos & SIM_AV_SATURA) != 0, ve("U1 satura en -15 V: la salida ya no sigue."));
    chk_cond("SATURA al final", "integrador (36): P_EMU_ESQ dice «U1 satura en -15 V: la salida ya no sigue.»",
             ui_pantalla() == P_EMU_ESQ && (sim_res()->d.avisos & SIM_AV_SATURA) && ve("U1 satura en -15 V: la salida ya no sigue."));
    TECLAS(T_ESC);

    /* ---- El texto de SATURA lleva la tensión (§3.M) ---- */
    r3tb_al_esq(37, 0);                                    /* el comparador: satura ya en el OP */
    chk_cond("SATURA con su tensi" S_oo "n", "comparador (38): P_EMU_ESQ, «U1 satura en -15 V: ...»",
             ui_pantalla() == P_EMU_ESQ && ve("U1 satura en -15 V: la salida ya no sigue."));
    TECLAS(T_ESC, T_LETRA('P'));
    chk_cond("SATURA con su tensi" S_oo "n", "comparador (38): [P], «U1 satura en -15 V: ...»",
             ui_pantalla() == P_RESULTADO && ve("U1 satura en -15 V: la salida ya no sigue."));
    TECLAS(T_ESC);

    /* ---- BIESTABLE antes que SATURA del mismo AO; ARRANQUE, también ---- */
    r3tb_al_esq(38, r3tb_astable_v0);
    sprintf(q, "astable con V0 = 0: avisos %#x", sim_res()->d.avisos);
    chk_cond(q, "P_EMU_ESQ: «U1 puede empezar en V+ o en V-: empieza en V+.»", ui_pantalla() == P_EMU_ESQ &&
             (sim_res()->d.avisos & SIM_AV_BIESTABLE) && ve("U1 puede empezar en V+ o en V-: empieza en V+."));
    TECLAS(T_ESC);
    {                                                      /* un Schmitt con [P] */
        static Circuito c;
        modelo_cargar(38, &c);
        r3tb_schmitt(&c);
        r3u_abre(&c);
        TECLAS(T_LETRA('P'));
        chk_cond("Schmitt", "[P]: «U1 puede empezar en V+ o en V-: empieza en V+.»", ui_pantalla() == P_RESULTADO &&
                 ve("U1 puede empezar en V+ o en V-: empieza en V+.") && !ve("U1 satura"));
        TECLAS(T_ESC);
    }
    {                                                      /* un Schmitt con raíles de 0 y 5 V: empieza en V- */
        static SimRes rs;
        static Valores v;
        SimDiag d;
        char t[96];
        sr_init(4);
        a4_cc(0, 1, 1.0);                                  /* la entrada, 1 V en la - */
        sr_el(SE_R, 3, 2, 10e3); sr_el(SE_R, 2, 0, 10e3);  /* V+ = Vsal/2 */
        k = a4_ao(2, 1, 3, 0, 0, 0.0);
        sr.el[k].m[1] = 5.0; sr.el[k].m[2] = 0.0;
        sim_op_red(&sr, &v, &d);
        memset(&rs, 0, sizeof rs);
        rs.d = d; rs.op = v;
        sim_aviso(0, &rs, SIM_AV_BIESTABLE, t, sizeof t);
        printf("    Schmitt de 0 a 5 V: regi" S_oo "n %d, avisos %#x: %s\n", v.reg[k], d.avisos, t);
        chk_cond("BIESTABLE", "Schmitt de 0 a 5 V: empieza en V- (0 V) y el aviso lo dice", d.estado == SIM_OK &&
                 v.reg[k] == REG_SAT_MENOS && fabs(v.V[k]) < 1e-12 && (d.avisos & SIM_AV_BIESTABLE) &&
                 strstr(t, "puede empezar en V+ o en V-: empieza en V-.") != 0);
        sim_aviso(0, &rs, SIM_AV_SATURA, t, sizeof t);
        chk_cond("SATURA con su tensi" S_oo "n", "Schmitt de 0 a 5 V: «satura en 0 V»", strstr(t, " satura en 0 V: la salida ya no sigue.") != 0);
    }
    {                                                      /* dos AO: U1 solo satura, U2 (un Schmitt) es biestable */
        static SimRes rs;
        static Valores v;
        SimDiag d;
        char t[96], nom[4];
        int k2;
        sr_init(6);
        a4_cc(0, 1, 1.0);
        k = a4_ao(1, 0, 2, 0, 0, 0.0);                     /* un comparador: +15 V */
        sr_el(SE_R, 2, 0, 1e3);
        a4_cc(0, 3, 0.0);
        sr_el(SE_R, 5, 4, 10e3); sr_el(SE_R, 4, 0, 10e3);
        k2 = a4_ao(4, 3, 5, 0, 0, 0.0);                    /* el Schmitt con la entrada a 0 V */
        sim_op_red(&sr, &v, &d);
        memset(&rs, 0, sizeof rs);
        rs.d = d; rs.op = v;
        sim_aviso(0, &rs, SIM_AV_BIESTABLE, t, sizeof t);
        nom[0] = 'U'; nom[1] = (char)('1' + k2 % 9); nom[2] = ' '; nom[3] = 0;   /* el nombre sin circuito (nombre_el) */
        printf("    dos AO: avisos %#x, elem_aviso %d (U1 %d, U2 %d): %s\n", d.avisos, d.elem_aviso, k, k2, t);
        chk_cond("BIESTABLE", "dos AO: el aviso nombra al biestable (U2), no al que solo satura (U1)",
                 d.estado == SIM_OK && (d.avisos & SIM_AV_BIESTABLE) && v.reg[k] == REG_SAT_MAS && d.elem_aviso == k2 &&
                 strncmp(t, nom, 3) == 0);
    }
    r3tb_al_esq(38, 0);                                    /* V0 vacía: ARRANQUE, y acaba saturado */
    sprintf(q, "astable: avisos %#x", sim_res()->d.avisos);
    chk_cond(q, "P_EMU_ESQ: «U1 arranca inestable: ...», no «U1 satura»", ui_pantalla() == P_EMU_ESQ &&
             ve("U1 arranca inestable: su salida se va a un ra" S_ii "l.") && !ve("U1 satura"));
    TECLAS(T_ESC);
    chk_cond("tran_b", "ning" S_uu "n texto se sale de la pantalla", gfx_desbordes() == 0);
}
