/* --------------------------------------------------------------------------
 *  50. Etapa 3: el diagrama fasorial y el triángulo de potencias
 *
 *  Se incluye en host_test.c (usa sr, car, ca_sr, sr_alterna, pon_val, chk_t
 *  y chk_cx de [45] y [47]). P_FASORIAL todavía no está en ui.c: la pantalla
 *  se dibuja con fz_dibuja y los textos se leen como ve() (gfx_textos_reset y
 *  gfx_texto_visto) con fzt_ve.
 * ----------------------------------------------------------------------- */
#include "fasorial.h"

static Fasorial fzt;

/* ¿Se ve este texto en la pantalla del diagrama? (se redibuja entera) */
static int fzt_ve(const Circuito *c, int conv, int rect, const char *t)
{
    gfx_textos_reset();
    fz_dibuja(&fzt, c, &car, conv, rect);
    return gfx_texto_visto(t);
}

static void fzt_vuelca(const Circuito *c, int conv, int rect, const char *nombre)
{
    char ruta[256];
    fz_dibuja(&fzt, c, &car, conv, rect);
    sprintf(ruta, "%s/fasorial_%s.bmp", dir_out, nombre);
    gfx_dump_bmp(ruta);
}

static void fzt_tecla(const Circuito *c, int t, int veces)
{
    int i;
    for (i = 0; i < veces; i++) fz_tecla(&fzt, c, &car, t);
}

/* Números al azar propios: no se toca la semilla de las demás pruebas */
static unsigned fzt_semilla = 2718u;
static unsigned fzt_azar(unsigned n) { fzt_semilla = fzt_semilla * 1103515245u + 12345u; return (fzt_semilla >> 16) % n; }

/* T1 como Circuito, con los índices de la red sr (E1 0, R1 1, L1 2, C1 3):
 * E1 de (0,1) hacia arriba, R1 y L1 por arriba, C1 bajando y el cable de
 * vuelta */
static void fzt_t1(Circuito *c)
{
    int k;
    circ_init(c, "T1");
    k = circ_elemento(c, EL_FUENTE, 0, 1, DIR_ARR);
    pon_val(c, k, 120.0);
    c->el[k].variante = FORMA_FASOR; c->frec.v = 60.0; c->frec.est = M_DATO;
    pon_val(c, circ_elemento(c, EL_RES, 0, 0, DIR_DER), 10.0);
    pon_val(c, circ_elemento(c, EL_BOB, 1, 0, DIR_DER), 50e-3);
    pon_val(c, circ_elemento(c, EL_COND, 2, 0, DIR_ABA), 100e-6);
    circ_cable(c, 2, 1, DIR_IZQ);
    circ_cable(c, 1, 1, DIR_IZQ);
}

/* ¿El estado del diagrama es coherente? */
static int fzt_sano(const Circuito *c)
{
    int np = fz_npot(c, &car);
    if (fzt.n < 0 || fzt.n > FZ_MAX) return 0;
    if (fzt.n > 0 && (fzt.sel < 0 || fzt.sel >= fzt.n || fzt.ini > fzt.sel || fzt.sel >= fzt.ini + 10)) return 0;
    if (fzt.ref < -1 || fzt.ref >= fzt.n) return 0;
    if (fzt.iesc[0] < 0 || fzt.iesc[0] >= N_ESCALA || fzt.iesc[1] < 0 || fzt.iesc[1] >= N_ESCALA) return 0;
    if (fzt.selp < 0 || fzt.selp > np || fzt.inip > fzt.selp || fzt.selp >= fzt.inip + 10) return 0;
    return fzt.vista == FZ_FASORES || fzt.vista == FZ_POTENCIAS;
}

static void test_fasorial(void)
{
    static Circuito c;
    static Red r;
    static CaRes g;
    int kE, kR, kL, kC, iVE, iIE, iVR, iIR, iVL, iVC, iIL, iIC, x, y, xe, ye, i, n, q, conv, bien;
    double dx, dy, dx0, dy0, mx, k;

    printf("[50] Etapa 3: diagrama fasorial\n");

    /* ---- T1 (como en [45]) y su Circuito ---- */
    sr_init(4);
    kE = sr_alterna(SE_E, 0, 1, 120.0, 0.0, 60.0);
    kR = sr_el(SE_R, 1, 2, 10.0);
    kL = sr_el(SE_L, 2, 3, 50e-3);
    kC = sr_el(SE_C, 3, 0, 100e-6);
    chk_cond("fasorial", "T1 se analiza", ca_sr(0.0) == SIM_OK);
    fzt_t1(&c);
    chk_cond("fasorial", "el Circuito: E1, R1, L1 y C1 con los índices de la red",
             c.nel == 4 && strcmp(c.el[kE].etiq, "E1") == 0 && strcmp(c.el[kR].etiq, "R1") == 0 &&
             strcmp(c.el[kL].etiq, "L1") == 0 && strcmp(c.el[kC].etiq, "C1") == 0);
    red_extraer(&c, &r);
    chk_cond("fasorial", "y es el mismo T1: ca_analiza da la misma I(E1)",
             ca_analiza(&c, &r, &g) == SIM_OK && cx_abs(cx_menos(g.I[kE], car.I[kE])) <= 1e-12 * cx_abs(car.I[kE]));

    /* ---- Al empezar ---- */
    fz_nuevo(&fzt, &c, &car);
    iVE = fz_busca(&fzt, kE, 'V'); iIE = fz_busca(&fzt, kE, 'I');
    iVR = fz_busca(&fzt, kR, 'V'); iIR = fz_busca(&fzt, kR, 'I');
    iVL = fz_busca(&fzt, kL, 'V'); iIL = fz_busca(&fzt, kL, 'I');
    iVC = fz_busca(&fzt, kC, 'V'); iIC = fz_busca(&fzt, kC, 'I');
    chk_cond("fasorial", "la lista: la V y la I de los cuatro, en orden",
             fzt.n == 8 && iVE == 0 && iIE == 1 && iVR == 2 && iIR == 3 && iVL == 4 && iIL == 5 && iVC == 6 && iIC == 7);
    if (fzt.n != 8 || iIC != 7) return;
    chk_cond("fasorial", "se ven V(E1), I(E1), V(R1), V(L1) y V(C1) (serie)",
             fzt.ver[iVE] && fzt.ver[iIE] && fzt.ver[iVR] && fzt.ver[iVL] && fzt.ver[iVC]);
    chk_cond("fasorial", "y no las I de R1, L1 y C1", !fzt.ver[iIR] && !fzt.ver[iIL] && !fzt.ver[iIC]);
    chk_cond("fasorial", "fasores, sin referencia ni cadena, elegida V(E1)",
             fzt.vista == FZ_FASORES && fzt.ref == -1 && fzt.cadena == 0 && fzt.sel == iVE && fzt.ini == 0);
    chk_cond("fasorial", "se ve la pantalla", fzt_ve(&c, 0, 0, "Diagrama fasorial") && gfx_texto_visto("V(C1)") &&
             gfx_texto_visto("120,0" S_ANG "0,0" S_DEG " V") && gfx_texto_visto("60 Hz  eficaz, coseno"));

    /* ---- Escalas automáticas: el mayor visible al 90 % de R ---- */
    chk_t("fasorial", "escala de las V: 500 V (V_C = 252,5 V)", ESCALA[fzt.iesc[0]], 500.0, 500.0, 1e-12);
    chk_t("fasorial", "escala de las I: 20 A (I = 9,519 A)", ESCALA[fzt.iesc[1]], 20.0, 20.0, 1e-12);
    for (q = 0; q < 2; q++) {
        mx = 0.0;
        for (i = 0; i < fzt.n; i++) {
            if (!fzt.ver[i] || (fzt.que[i] == 'I') != (q == 1)) continue;
            fz_vector(&fzt, &car, i, 0, &dx, &dy);
            if (sqrt(dx * dx + dy * dy) > mx) mx = sqrt(dx * dx + dy * dy);
        }
        chk_cond("fasorial", q ? "I: el mayor, a menos del 90 % de R" : "V: el mayor, a menos del 90 % de R", mx <= 0.9 * FZ_R);
        chk_cond("fasorial", q ? "I: con la escala anterior pasaría del 90 %" : "V: con la escala anterior pasaría del 90 %",
                 fzt.iesc[q] > 0 && mx * ESCALA[fzt.iesc[q]] / ESCALA[fzt.iesc[q] - 1] > 0.9 * FZ_R);
    }
    chk_cond("fasorial", "abajo, lo que vale el borde: «500 V  20 A»", fzt_ve(&c, 0, 0, "500 V  20 A"));

    /* ---- fz_punta: pura ---- */
    {
        double L = 95.18861634600458 / 500.0 * FZ_R, a = 37.510810145539 * PI_T / 180.0;
        fz_punta(&fzt, &car, iVR, FZ_CX, FZ_CY, &x, &y);
        chk_cond("fz_punta", "V(R1): 95,19 V a 37,5" S_DEG " con 500 V en el borde",
                 x == (int)floor(FZ_CX + L * cos(a) + 0.5) && y == (int)floor(FZ_CY - L * sin(a) + 0.5));
        fz_punta(&fzt, &car, iVR, 10.25, 200.5, &x, &y);
        chk_cond("fz_punta", "desde otro origen",
                 x == (int)floor(10.25 + L * cos(a) + 0.5) && y == (int)floor(200.5 - L * sin(a) + 0.5));
        L = 9.518861634600457 / 20.0 * FZ_R;
        fz_punta(&fzt, &car, iIE, FZ_CX, FZ_CY, &x, &y);
        chk_cond("fz_punta", "I(E1): 9,519 A con 20 A en el borde",
                 x == (int)floor(FZ_CX + L * cos(a) + 0.5) && y == (int)floor(FZ_CY - L * sin(a) + 0.5));
        fz_punta(&fzt, &car, iVL, FZ_CX, FZ_CY, &x, &y);
        fz_punta(&fzt, &car, iVC, FZ_CX, FZ_CY, &xe, &ye);
        chk_cond("fz_punta", "sin referencia, V(L1) y V(C1) no son verticales", x != FZ_CX && xe != FZ_CX);
    }

    /* ---- [R] sobre I(E1): V(R1) queda sobre el eje (está en fase con I) ---- */
    fzt_tecla(&c, T_ABAJO, 1);
    chk_cond("[R]", "[ab]: elegida I(E1)", fzt.sel == iIE);
    fzt_tecla(&c, T_LETRA('R'), 1);
    chk_cond("[R]", "la referencia es I(E1)", fzt.ref == iIE);
    fz_punta(&fzt, &car, iVR, FZ_CX, FZ_CY, &x, &y);
    chk_cond("[R]", "la punta de V(R1) con y = FZ_CY", y == FZ_CY && x > FZ_CX);
    fz_punta(&fzt, &car, iIE, FZ_CX, FZ_CY, &x, &y);
    chk_cond("[R]", "y la de I(E1) también", y == FZ_CY && x > FZ_CX);
    fz_punta(&fzt, &car, iVL, FZ_CX, FZ_CY, &x, &y);
    fz_punta(&fzt, &car, iVC, FZ_CX, FZ_CY, &xe, &ye);
    chk_cond("[R]", "V(L1) arriba y V(C1) abajo, verticales", x == FZ_CX && y < FZ_CY && xe == FZ_CX && ye > FZ_CY);
    chk_cond("[R]", "la leyenda: «0" S_DEG "», «(ref.)» y I(E1) a 0" S_DEG,
             fzt_ve(&c, 0, 0, "0" S_DEG) && gfx_texto_visto("I(E1) (ref.)") && gfx_texto_visto("9,519" S_ANG "0,0" S_DEG " A"));
    fzt_vuelca(&c, 0, 1, "ref");
    fzt_tecla(&c, T_LETRA('R'), 1);
    chk_cond("[R]", "otra vez sobre el mismo: ángulos absolutos", fzt.ref == -1);
    chk_cond("[R]", "I(E1) vuelve a 37,5" S_DEG, fzt_ve(&c, 0, 0, "9,519" S_ANG "37,5" S_DEG " A"));

    /* ---- [S] sobre V: VR + VL + VC acaba en la punta de E1 ---- */
    fzt_tecla(&c, T_ABAJO, 1);
    chk_cond("[S]", "[ab]: elegida V(R1)", fzt.sel == iVR);
    chk_cond("[S]", "la leyenda enseña V(R1) en polar y en rectangular",
             fzt_ve(&c, 0, 0, "95,19" S_ANG "37,5" S_DEG " V") && gfx_texto_visto("75,5+j58,0 V"));
    fzt_tecla(&c, T_LETRA('S'), 1);
    chk_cond("[S]", "cadena de las V", (fzt.cadena & 3) == 1);
    n = fz_cadena_fin(&fzt, &c, &car, 0, &x, &y);
    fz_punta(&fzt, &car, iVE, FZ_CX, FZ_CY, &xe, &ye);
    chk_cond("[S]", "tres eslabones (R1, L1 y C1; E1 se queda en el origen)", n == 3);
    chk_cond("[S]", "la punta final a " S_PM "1 px de la de E1", abs(x - xe) <= 1 && abs(y - ye) <= 1);
    chk_t("[S]", "la escala sigue en 500 V (el eslabón más lejano: 203 V)", ESCALA[fzt.iesc[0]], 500.0, 500.0, 1e-12);
    chk_cond("[S]", "se ve la suma, " S_SIGMA, fzt_ve(&c, 0, 0, S_SIGMA));
    fzt_vuelca(&c, 0, 0, "cadena");
    /* con la referencia en I(E1) la cadena sigue acabando en E1 */
    fzt_tecla(&c, T_ARRIBA, 1);
    fzt_tecla(&c, T_LETRA('R'), 1);
    fz_cadena_fin(&fzt, &c, &car, 0, &x, &y);
    fz_punta(&fzt, &car, iVE, FZ_CX, FZ_CY, &xe, &ye);
    chk_cond("[S]", "con referencia: también a " S_PM "1 px", abs(x - xe) <= 1 && abs(y - ye) <= 1);
    fzt_tecla(&c, T_LETRA('R'), 1);
    fzt_tecla(&c, T_ABAJO, 1);
    fzt_tecla(&c, T_LETRA('S'), 1);
    chk_cond("[S]", "otra vez: sin cadena", fzt.cadena == 0 && fz_cadena_fin(&fzt, &c, &car, 0, &x, &y) == 0 &&
             x == FZ_CX && y == FZ_CY);

    /* ---- [+] y [-]: la escala de la familia del elegido (1-2-5) ---- */
    fzt_tecla(&c, T_MAS, 1);
    chk_t("[+]", "V: 200 V en el borde", ESCALA[fzt.iesc[0]], 200.0, 200.0, 1e-12);
    chk_cond("[+]", "ajustada a mano, y las I no cambian", fzt.ajustado[0] && !fzt.ajustado[1] && ESCALA[fzt.iesc[1]] == 20.0);
    fzt_tecla(&c, T_MENOS, 2);
    chk_t("[-]", "V: 1 kV", ESCALA[fzt.iesc[0]], 1000.0, 1000.0, 1e-12);
    chk_cond("[-]", "se lee «1 kV  20 A»", fzt_ve(&c, 0, 0, "1 kV  20 A"));
    fzt_tecla(&c, T_ENTER, 1);
    chk_cond("[enter]", "oculta V(R1); ajustada a mano, la escala se queda", !fzt.ver[iVR] && ESCALA[fzt.iesc[0]] == 1000.0);
    fzt_tecla(&c, T_MAS, 60);
    chk_cond("[+]", "no pasa del primero", fzt.iesc[0] == 0);
    fzt_tecla(&c, T_MENOS, 60);
    chk_cond("[-]", "ni del último", fzt.iesc[0] == N_ESCALA - 1);
    fzt_tecla(&c, T_LETRA('A'), 1);
    chk_cond("[A]", "automática otra vez: 500 V y 20 A", ESCALA[fzt.iesc[0]] == 500.0 && ESCALA[fzt.iesc[1]] == 20.0 &&
             !fzt.ajustado[0] && !fzt.ajustado[1]);
    fzt_tecla(&c, T_ENTER, 1);
    chk_cond("[enter]", "enseña V(R1) otra vez", fzt.ver[iVR]);
    /* ocultar la mayor: la automática baja a 200 V (V_L = 179,4 V al 89,7 %) */
    fzt_tecla(&c, T_ABAJO, 4);
    chk_cond("[enter]", "[ab] x4: elegida V(C1)", fzt.sel == iVC);
    fzt_tecla(&c, T_ENTER, 1);
    chk_cond("[enter]", "oculta V(C1) y la escala pasa a 200 V", !fzt.ver[iVC] && ESCALA[fzt.iesc[0]] == 200.0);
    fzt_tecla(&c, T_ENTER, 1);
    chk_cond("[enter]", "la vuelve a enseñar: 500 V", fzt.ver[iVC] && ESCALA[fzt.iesc[0]] == 500.0);
    fzt_tecla(&c, T_ABAJO, 1);
    fzt_tecla(&c, T_ENTER, 1);
    chk_cond("[enter]", "I(C1) se ve; las I siguen en 20 A", fzt.sel == iIC && fzt.ver[iIC] && ESCALA[fzt.iesc[1]] == 20.0);
    fzt_tecla(&c, T_ENTER, 1);
    fzt_tecla(&c, T_ABAJO, 5);
    chk_cond("[arr/ab]", "abajo no pasa del último", fzt.sel == fzt.n - 1);
    fzt_tecla(&c, T_ARRIBA, 20);
    chk_cond("[arr/ab]", "arriba no pasa del primero", fzt.sel == 0 && fzt.ini == 0);
    chk_cond("[tab] [esc]", "vuelven a P_CA", fz_tecla(&fzt, &c, &car, T_TAB) == FZ_SALIR && fz_tecla(&fzt, &c, &car, T_ESC) == FZ_SALIR);
    chk_cond("fasorial", "las otras teclas no hacen nada", fz_tecla(&fzt, &c, &car, T_DIG(5)) == FZ_NADA && fz_tecla(&fzt, &c, &car, T_IGUAL) == FZ_NADA);
    fzt_vuelca(&c, 0, 0, "fasores");

    /* ---- Convenio: pico y seno ---- */
    c.conv = CONV_PICO | CONV_SENO;
    fz_nuevo(&fzt, &c, &car);
    fz_vector(&fzt, &car, iIE, 0, &dx0, &dy0);
    fz_vector(&fzt, &car, iIE, c.conv, &dx, &dy);
    chk_cond("convenio", "con pico y seno las escalas son las mismas (500 V: 357 V; 20 A: 13,5 A)",
             ESCALA[fzt.iesc[0]] == 500.0 && ESCALA[fzt.iesc[1]] == 20.0);
    chk_cond("convenio", "I(E1) se dibuja " S_SQRT "2 más larga y girada 90" S_DEG,
             fabs(dx - R2_T * dy0) <= 1e-9 * FZ_R && fabs(dy + R2_T * dx0) <= 1e-9 * FZ_R);
    fzt_tecla(&c, T_ABAJO, 1);
    chk_cond("convenio", "la leyenda: 13,46" S_ANG "128" S_DEG " A y «pico, seno»",
             fzt_ve(&c, c.conv, 0, "13,46" S_ANG "128" S_DEG " A") && gfx_texto_visto("60 Hz  pico, seno"));
    fzt_vuelca(&c, c.conv, 0, "pico_seno");
    c.conv = 0;
    fz_nuevo(&fzt, &c, &car);

    /* ---- Vista de potencias ---- */
    fzt_tecla(&c, T_DER, 1);
    chk_cond("potencias", "[der]: el triángulo", fzt.vista == FZ_POTENCIAS && fzt_ve(&c, 0, 0, "Triángulo de potencias"));
    chk_cond("potencias", "P = 906,1 W", fzt_ve(&c, 0, 0, "P = 906,1 W"));
    chk_cond("potencias", "Q = -695,5 var", gfx_texto_visto("Q = -695,5 var"));
    chk_cond("potencias", "S = 1142 VA", gfx_texto_visto("S = 1142 VA"));
    chk_cond("potencias", "fp 0,793 cap.", gfx_texto_visto("fp 0,793 cap."));
    chk_cond("potencias", "«Total entregado» y el ángulo " S_PHI " -37,5" S_DEG,
             gfx_texto_visto("Total entregado") && gfx_texto_visto(S_PHI " -37,5" S_DEG));
    chk_cond("potencias", "cuatro elementos con S " S_NE " 0", fz_npot(&c, &car) == 4 && fz_pot_el(&c, &car, 1) == kE &&
             fz_pot_el(&c, &car, 4) == kC && fz_pot_el(&c, &car, 5) == -1);
    k = fz_escala_pot(&fzt, &c, &car);
    chk_t("potencias", "k = mín(170/|P|, 95/|Q|) = 95/695,54", k, 95.0 / 695.5368635944589, 0.137, 1e-12);
    fzt_vuelca(&c, 0, 0, "potencias");
    fzt_tecla(&c, T_LETRA('S'), 1);
    chk_cond("Boucherot", "[S]: encadena las cargas", (fzt.cadena & FZ_BOUCHEROT) && fzt_ve(&c, 0, 0, "Total (Boucherot)"));
    k = fz_escala_pot(&fzt, &c, &car);
    chk_t("Boucherot", "la escala cabe con R + L: 95/1708", k, 95.0 / 1707.9342631479203, 0.0556, 1e-12);
    fzt_vuelca(&c, 0, 0, "boucherot");
    fzt_tecla(&c, T_ABAJO, 1);
    chk_cond("potencias", "[ab]: E1 entrega 906,1 W", fzt.selp == 1 && fzt_ve(&c, 0, 0, "E1 entrega") && gfx_texto_visto("P = 906,1 W"));
    fzt_tecla(&c, T_ABAJO, 1);
    chk_cond("potencias", "R1 absorbe 906,1 W, Q = 0 y fp 1", fzt.selp == 2 && fzt_ve(&c, 0, 0, "R1 absorbe") &&
             gfx_texto_visto("P = 906,1 W") && gfx_texto_visto("Q = 0 var") && gfx_texto_visto("fp 1,000"));
    fzt_tecla(&c, T_ABAJO, 1);
    chk_cond("potencias", "L1: Q = 1708 var, P = 0 y fp 0 inductivo", fzt_ve(&c, 0, 0, "Q = 1708 var") &&
             gfx_texto_visto("P = 0 W") && gfx_texto_visto("fp 0,000 ind."));
    fzt_tecla(&c, T_ABAJO, 5);
    chk_cond("potencias", "C1: Q = -2403 var (y no pasa del último)", fzt.selp == 4 && fzt_ve(&c, 0, 0, "Q = -2403 var"));
    fzt_tecla(&c, T_LETRA('S'), 1);
    k = fz_escala_pot(&fzt, &c, &car);
    chk_t("potencias", "sin Boucherot, el triángulo de C1: 95/2403", k, 95.0 / 2403.471126742379, 0.04, 1e-12);
    fzt_tecla(&c, T_ARRIBA, 9);
    chk_cond("potencias", "arriba: el total", fzt.selp == 0);
    chk_cond("potencias", "[tab] y [esc] vuelven a P_CA", fz_tecla(&fzt, &c, &car, T_TAB) == FZ_SALIR && fz_tecla(&fzt, &c, &car, T_ESC) == FZ_SALIR);
    fzt_tecla(&c, T_IZQ, 1);
    chk_cond("potencias", "[izq]: a los fasores", fzt.vista == FZ_FASORES && fzt_ve(&c, 0, 0, "Diagrama fasorial"));

    /* ---- Nada se sale de la pantalla ---- */
    gfx_desbordes_reset();
    for (i = 0; i < 32; i++) {
        conv = (i & 3) == 0 ? 0 : (i & 3) == 1 ? CONV_PICO : (i & 3) == 2 ? CONV_SENO : CONV_PICO | CONV_SENO;
        fzt.vista = (unsigned char)((i >> 2) & 1);
        fzt.cadena = (unsigned char)((i >> 3) & 1 ? 1 | FZ_BOUCHEROT : 0);
        fzt.ref = (i >> 4) & 1 ? iIE : -1;
        fz_dibuja(&fzt, &c, &car, conv, (i >> 1) & 1);
    }
    pruebas++;
    if (gfx_desbordes() != 0) falla("fasorial", "textos que se salen de la pantalla (T1)", gfx_desbordes(), 0);
    fz_nuevo(&fzt, &c, &car);

    /* ---- Paralelo: se ven las I; la cadena de las I acaba en I(E1) ---- */
    {
        static Circuito cp;
        int pE, pR, pL, pC, jE;
        sr_init(2);
        pE = sr_alterna(SE_E, 0, 1, 120.0, 0.0, 60.0);
        pR = sr_el(SE_R, 1, 0, 10.0);
        pL = sr_el(SE_L, 1, 0, 50e-3);
        pC = sr_el(SE_C, 1, 0, 100e-6);
        chk_cond("paralelo", "se analiza", ca_sr(0.0) == SIM_OK);
        circ_init(&cp, "RLC paralelo");
        pon_val(&cp, circ_elemento(&cp, EL_FUENTE, 0, 1, DIR_ARR), 120.0);
        cp.el[pE].variante = FORMA_FASOR; cp.frec.v = 60.0; cp.frec.est = M_DATO;
        pon_val(&cp, circ_elemento(&cp, EL_RES, 1, 0, DIR_ABA), 10.0);
        pon_val(&cp, circ_elemento(&cp, EL_BOB, 2, 0, DIR_ABA), 50e-3);
        pon_val(&cp, circ_elemento(&cp, EL_COND, 3, 0, DIR_ABA), 100e-6);
        for (i = 0; i < 3; i++) { circ_cable(&cp, i, 0, DIR_DER); circ_cable(&cp, i, 1, DIR_DER); }
        fz_nuevo(&fzt, &cp, &car);
        jE = fz_busca(&fzt, pE, 'I');
        chk_cond("paralelo", "se ven V(E1), I(E1) y las I de R1, L1 y C1",
                 fzt.n == 8 && jE >= 0 && fzt.ver[fz_busca(&fzt, pE, 'V')] && fzt.ver[jE] && fzt.ver[fz_busca(&fzt, pR, 'I')] &&
                 fzt.ver[fz_busca(&fzt, pL, 'I')] && fzt.ver[fz_busca(&fzt, pC, 'I')]);
        chk_cond("paralelo", "y no sus V", !fzt.ver[fz_busca(&fzt, pR, 'V')] && !fzt.ver[fz_busca(&fzt, pL, 'V')] &&
                 !fzt.ver[fz_busca(&fzt, pC, 'V')]);
        chk_t("paralelo", "escala de las I: 20 A (I_R = 12 A)", ESCALA[fzt.iesc[1]], 20.0, 20.0, 1e-12);
        fzt_tecla(&cp, T_ABAJO, 1);
        fzt_tecla(&cp, T_LETRA('S'), 1);
        n = fz_cadena_fin(&fzt, &cp, &car, 0, &x, &y);
        fz_punta(&fzt, &car, jE, FZ_CX, FZ_CY, &xe, &ye);
        chk_cond("paralelo", "[S] sobre I: IR + IL + IC acaba a " S_PM "1 px de I(E1)", n == 3 && abs(x - xe) <= 1 && abs(y - ye) <= 1);
        fzt_vuelca(&cp, 0, 0, "paralelo");
        chk_cond("paralelo", "potencias: P = 1440 W, Q = 221,1 var, fp 0,988 inductivo",
                 (fzt_tecla(&cp, T_DER, 1), fzt_ve(&cp, 0, 0, "P = 1440 W")) && gfx_texto_visto("Q = 221,1 var") &&
                 gfx_texto_visto("fp 0,988 ind."));
        fzt_vuelca(&cp, 0, 0, "paralelo_potencias");
    }

    /* ---- Una escalera con 16 fasores (la lista se desplaza) y el mono ---- */
    {
        static Circuito ce;
        static const int TI[8] = { SE_E, SE_R, SE_L, SE_R, SE_C, SE_R, SE_L, SE_C };
        static const int TC[8] = { EL_FUENTE, EL_RES, EL_BOB, EL_RES, EL_COND, EL_RES, EL_BOB, EL_COND };
        static const int NA[8] = { 0, 1, 2, 2, 3, 3, 4, 4 }, NB[8] = { 1, 2, 0, 3, 0, 4, 0, 0 };
        static const double VA[8] = { 0.0, 10.0, 20e-3, 5.0, 220e-6, 8.0, 50e-3, 47e-6 };
        static const int K[16] = { T_ARRIBA, T_ABAJO, T_ABAJO, T_ENTER, T_LETRA('R'), T_LETRA('S'), T_MAS, T_MENOS,
                                   T_LETRA('A'), T_IZQ, T_DER, T_ABAJO, T_ABAJO, T_ENTER, T_PUNTO, T_LETRA('S') };
        sr_init(5);
        circ_init(&ce, "Escalera");
        for (i = 0; i < 8; i++) {
            if (i == 0) sr_alterna(SE_E, 0, 1, 230.0, 30.0, 50.0);
            else sr_el(TI[i], NA[i], NB[i], VA[i]);
            pon_val(&ce, circ_elemento(&ce, (TipoEl)TC[i], i, 3, DIR_DER), i ? VA[i] : 230.0);
        }
        ce.el[0].variante = FORMA_FASOR; ce.frec.v = 50.0; ce.frec.est = M_DATO;
        chk_cond("escalera", "se analiza", ca_sr(0.0) == SIM_OK);
        fz_nuevo(&fzt, &ce, &car);
        chk_cond("escalera", "16 fasores; ni serie ni paralelo: se ven las V", fzt.n == 16 && fzt.ver[fz_busca(&fzt, 1, 'V')] &&
                 !fzt.ver[fz_busca(&fzt, 1, 'I')]);
        fzt_tecla(&ce, T_ABAJO, 12);
        chk_cond("escalera", "la lista baja con el elegido", fzt.sel == 12 && fzt.ini == 3);
        fzt_vuelca(&ce, 0, 0, "escalera");
        fzt_tecla(&ce, T_DER, 1);
        fzt_tecla(&ce, T_LETRA('S'), 1);
        fzt_vuelca(&ce, 0, 0, "escalera_potencias");
        gfx_desbordes_reset();
        bien = 1;
        for (i = 0; i < 600; i++) {
            int tk = K[fzt_azar(16)];
            if (fzt_azar(7) == 0) tk |= T_CON_SHIFT;
            fz_tecla(&fzt, &ce, &car, tk);
            if (!fzt_sano(&ce)) bien = 0;
            fz_dibuja(&fzt, &ce, &car, (int)fzt_azar(4), (int)fzt_azar(2));
        }
        chk_cond("escalera", "600 teclas al azar: el estado siempre es coherente", bien);
        pruebas++;
        if (gfx_desbordes() != 0) falla("fasorial", "textos que se salen de la pantalla (escalera)", gfx_desbordes(), 0);
    }

    /* ---- El peor caso: 32 elementos y los 64 fasores a la vista ---- */
    {
        static Circuito cw;
        unsigned long t0, dt;
        int veces = 0;
        sr_init(17);
        circ_init(&cw, "Peor caso");
        sr_alterna(SE_E, 0, 1, 100.0, 0.0, 1e3);
        pon_val(&cw, circ_elemento(&cw, EL_FUENTE, 0, 0, DIR_DER), 100.0);
        for (i = 1; i < MAX_EL; i++) {
            int t = i % 3 == 0 ? SE_C : i % 3 == 1 ? SE_R : SE_L;
            double v = t == SE_C ? 1e-6 * (1 + i % 5) : t == SE_R ? 10.0 * (1 + i % 7) : 1e-3 * (1 + i % 4);
            /* escalera: en serie (i par) del nudo i/2 al siguiente; en derivación (i impar) a tierra */
            sr_el(t, i % 2 ? (i + 1) / 2 : i / 2, i % 2 ? 0 : i / 2 + 1, v);
            pon_val(&cw, circ_elemento(&cw, t == SE_C ? EL_COND : t == SE_R ? EL_RES : EL_BOB, i % 9, i / 9, DIR_DER), v);
        }
        cw.el[0].variante = FORMA_FASOR; cw.frec.v = 1e3; cw.frec.est = M_DATO;
        chk_cond("peor caso", "32 elementos: se analiza", cw.nel == MAX_EL && ca_sr(0.0) == SIM_OK);
        fz_nuevo(&fzt, &cw, &car);
        for (i = 0; i < fzt.n; i++) fzt.ver[i] = 1;
        fzt.cadena = 1;
        fz_autoescala(&fzt, &cw, &car, 0);
        gfx_desbordes_reset();
        t0 = gfx_reloj_ms();
        do { fz_dibuja(&fzt, &cw, &car, 0, 0); veces++; dt = gfx_reloj_ms() - t0; } while (dt < 200 && veces < 1000);
        printf("    peor caso: %d fasores, %.2f ms por cuadro en el PC (~%.0f ms en la calculadora, x1000)\n",
               fzt.n, (double)dt / veces, 1000.0 * (double)dt / veces);
        chk_cond("peor caso", "64 fasores en la lista", fzt.n == 2 * MAX_EL);
        pruebas++;
        if (gfx_desbordes() != 0) falla("fasorial", "textos que se salen de la pantalla (peor caso)", gfx_desbordes(), 0);
        fzt_vuelca(&cw, 0, 0, "peor");
    }

    /* ---- Sin resultado (un error del análisis) ---- */
    memset(&car, 0, sizeof car);
    car.d.estado = SIM_LAZO_V;
    fz_nuevo(&fzt, &c, &car);
    chk_cond("sin resultado", "la lista está vacía", fzt.n == 0 && fz_npot(&c, &car) == 0);
    gfx_desbordes_reset();
    chk_cond("sin resultado", "lo dice", fzt_ve(&c, 0, 0, "no dio") && (fzt_tecla(&c, T_DER, 1), fzt_ve(&c, 0, 0, "no dio")));
    fzt_tecla(&c, T_ABAJO, 3); fzt_tecla(&c, T_LETRA('S'), 1); fzt_tecla(&c, T_IZQ, 1); fzt_tecla(&c, T_ENTER, 1);
    fzt_tecla(&c, T_LETRA('R'), 1); fzt_tecla(&c, T_MAS, 1);
    fz_dibuja(&fzt, &c, &car, 0, 0);
    chk_cond("sin resultado", "las teclas no rompen nada", fzt.n == 0 && fzt.ref == -1);
    pruebas++;
    if (gfx_desbordes() != 0) falla("fasorial", "textos que se salen de la pantalla (sin resultado)", gfx_desbordes(), 0);
    ca_sr(0.0);
}
