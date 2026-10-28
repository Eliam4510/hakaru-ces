/* --------------------------------------------------------------------------
 *  65n. Etapa 4: los hallazgos del grupo dibujo_b de la segunda revisión
 *
 *  Una prueba por hallazgo (cada una falla sin su arreglo, comprobado por
 *  mutación): el voltímetro del punto de operación con su signo (el modelo
 *  35 marca -3 V), el «qué ver» de las medias del último periodo (modelos
 *  26, 27 y 28), los rótulos de la fuente del puente (modelo 28) sin tapar y
 *  la ganancia del emisor común en la teoría 13.2. Se incluye tras
 *  test_e4_rev.inc (usa rv_al_menu, rv_modelos, r4_el, r4_espera, r3u_abre,
 *  q4_ec, t4_seno, bode_sr y canal).
 * ----------------------------------------------------------------------- */

/* Los píxeles de los rótulos del elemento 0 de c (el esquema con solo él,
 * con y sin rótulos, en el marco m) que la pantalla de ahora no enseña
 * iguales: los que otro rótulo, una sonda o un símbolo le tapan. En *n, los
 * píxeles de sus rótulos */
static int rd2_tapados(const Circuito *c, const Marco *m, const Valores *val, int *n)
{
    static Circuito s;
    static u16 pant[SCR_W * SCR_H], sinr[SCR_W * SCR_H];
    const u16 *fb = gfx_buffer();
    Vista v;
    int i, tap = 0;
    memcpy(pant, fb, sizeof pant);
    circ_init(&s, "solo");                    /* solo el 0, en su sitio */
    circ_elemento(&s, c->el[0].tipo, c->el[0].xa, c->el[0].ya,
                  c->el[0].xb > c->el[0].xa ? DIR_DER : c->el[0].xb < c->el[0].xa ? DIR_IZQ :
                  c->el[0].yb > c->el[0].ya ? DIR_ABA : DIR_ARR);
    s.el[0] = c->el[0];
    esq_vista(&v);
    v.capa = CAPA_POTENCIALES;                /* sin rótulos */
    gfx_clip_reset();
    gfx_clear(C_FONDO);
    esq_dibuja(&s, m, &v);
    memcpy(sinr, fb, sizeof sinr);
    esq_vista(&v);                            /* con los suyos */
    v.val = val;
    gfx_clear(C_FONDO);
    esq_dibuja(&s, m, &v);
    *n = 0;
    for (i = 0; i < SCR_W * SCR_H; i++)
        if (fb[i] != sinr[i]) { (*n)++; if (pant[i] != fb[i]) tap++; }
    return tap;
}

static void test_rev2_dibujo_b(void)
{
    static Circuito c;
    char t[128], u[24];
    int k, i, n, tap;

    printf("[65n] Etapa 4: los hallazgos del grupo dibujo_b (rev2)\n");
    gfx_desbordes_reset();
    ui_init();
    TECLAS(T_ENTER);

    /* ---- El voltímetro del punto de operación marca con su signo ---- */
    chk_cond("V con signo", "la lecci" S_oo "n 13", rv_modelos("13."));
    repite(T_ABAJO, 2);
    TECLAS(T_ENTER);                                      /* el 35: el sumador */
    chk_cond("V con signo", "el modelo 35: V1 marca -3V, como Vo y la clave", ui_pantalla() == P_RESULTADO &&
             ui_res_nl() && ve("\n-3V\n") && !ve("\n3V\n") && ve("Vo -3,00V"));
    TECLAS(T_TAB);
    chk_cond("V con signo", "y en P_TABLA, -3 V", ui_pantalla() == P_TABLA && ve("-3 V"));
    for (i = 0; i < 2; i++) {                             /* en CA, 0; el eficaz total, 3 */
        modelo_cargar(34, &c);
        k = r4_el(&c, "V1");
        c.el[k].variante = (unsigned char)(i == 0 ? MED_CA : MED_EF);
        r3u_abre(&c);
        TECLAS(T_LETRA('P'));                             /* con un V en CA, [=] emula */
        chk_cond("V con signo", i == 0 ? "V1 en CA: ~0V" : "V1 en CA+CC: 3V", ui_pantalla() == P_RESULTADO && ui_res_nl() &&
                 ve(i == 0 ? "\n~0V\n" : "\n3V\n") && !ve("\n-3V\n"));
    }

    /* ---- Lo que el «qué ver» da del último periodo lo dice: el
     *      osciloscopio mide la ventana entera ---- */
    for (i = 22; i < 42; i++) {
        const CircModelo *md = &MODELOS[i];
        int j, osc;
        if (md->ca != 0 || !strchr("APUu", md->medida)) continue;
        rv_al_menu();
        rv_modelos(i < 32 ? "12." : "13.");
        repite(T_ABAJO, i < 32 ? i - 22 : i - 32);
        TECLAS(T_ENTER);
        r4_espera();
        num_pref(u, sizeof u, md->clave, 3, "V");
        sprintf(t, "\n%s\n", u);
        osc = ui_pantalla() == P_OSC && ve(t);
        j = strstr(md->que_ver, "ltimo periodo") != 0;
        printf("    modelo %d ('%c'): el osciloscopio %s %s; el texto %s\n", i + 1, md->medida,
               osc ? "ense" S_nn "a" : "no ense" S_nn "a", u, j ? "dice «" S_uu "ltimo periodo»" : "no lo dice");
        sprintf(t, "modelo %d: su clave (%s) se ve o el texto dice «" S_uu "ltimo periodo»", i + 1, u);
        chk_cond("qu" S_ee " ver", t, ui_pantalla() == P_OSC && (osc || j));
    }

    /* ---- El puente: los rótulos de la fuente se leen ---- */
    modelo_cargar(27, &c);
    rv_al_menu();
    rv_modelos("12.");
    repite(T_ABAJO, 5);
    TECLAS(T_TAB);                                        /* al Taller */
    {
        Marco m;
        ui_dibuja();
        esq_marco_editor(&m);
        tap = rd2_tapados(&c, &m, 0, &n);
        printf("    puente en el editor: %d de %d p" S_ii "xeles de los r" S_oo "tulos de E1 tapados\n", tap, n);
        chk_cond("puente", "el editor: 12V y 60Hz, sin tapar", ui_pantalla() == P_EDITOR && ve("\n12V\n") && ve("\n60Hz\n") &&
                 n > 0 && tap == 0);
    }
    rv_al_menu();
    rv_modelos("12.");
    repite(T_ABAJO, 5);
    TECLAS(T_ENTER);
    r4_espera();
    for (i = 0; i < 4 && ui_pantalla() != P_EMU_ESQ; i++) TECLAS(T_TAB);
    {
        Marco m;
        static Valores vf;
        /* lo que rotula P_EMU_ESQ: e(t_fin) = 12·sen(12π) es ruido de
         * redondeo frente a los 12 V de E1 y se rotula 0 (revisión 3) */
        vf = sim_res()->ultimo;
        if (fabs(vf.val[0]) <= 1e-12 * fabs(sim_res()->Vmax[0])) vf.val[0] = 0.0;
        ui_dibuja();
        esq_marco_ajustado(&c, 0, 16, SCR_W, 158 - 16, &m);
        tap = rd2_tapados(&c, &m, &vf, &n);
        printf("    puente emulado: %d de %d p" S_ii "xeles de los r" S_oo "tulos de E1 tapados\n", tap, n);
        chk_cond("puente", "P_EMU_ESQ: el valor final de E1, sin tapar", ui_pantalla() == P_EMU_ESQ && n > 0 && tap == 0);
    }

    /* ---- La teoría 13.2: -gm·RC con RE desacoplada; sin el C, -RC/RE ---- */
    {
        const Pagina *p = &LECCIONES[12].pag[1];
        Cx H;
        int ke;
        for (i = 0; i < 2; i++) {                         /* el emisor común del modelo 42 */
            q4_ec(12.0, 47e3, 10e3, 2.2e3, 470.0, 1, 100.0, 0.0);
            sr.nn = 6;
            ke = t4_seno(0, 5, 1e-3, 1e3, 0.0);          /* la señal, a la base por Cin */
            sr_el(SE_C, 5, 2, 10e-6);
            if (i) sr_el(SE_C, 3, 0, 100e-6);            /* CE: RE desacoplada */
            canal(0, CANAL_NODO, 4); canal(1, CANAL_NADA, 0);
            bode_sr(ke, 0, -1, -1, 0.0, 0.0, 0.0);
            bode_en(10e3, &H);
            printf("    %s: H(10 kHz) = %.4f %+.4f j\n", i ? "con CE" : "sin CE", H.re, H.im);
            chk_cond("teor" S_ii "a 13.2", i ? "con CE, -gm" S_CDOT "RC (-208,7)" : "sin CE, " S_APROX " -RC/RE (-4,68)",
                     i ? fabs(H.re + 208.7) < 1.0 : fabs(H.re + 2.2e3 / 470.0) < 0.05 * 2.2e3 / 470.0);
        }
        chk_cond("teor" S_ii "a 13.2", "-gm" S_CDOT "RC, con RE desacoplada; sin el C, -RC/RE",
                 strstr(p->texto, "-gm" S_CDOT "RC con RE\ndesacoplada") && strstr(p->texto, S_APROX " -RC/RE"));
    }

    chk_cond("interfaz", "ning" S_uu "n texto fuera de la pantalla", gfx_desbordes() == 0);
}
