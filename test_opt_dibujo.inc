/* ============================================================================
 *  test_opt_dibujo.inc  -  [75d] La optimización del dibujo (rev9, grupo
 *  «dibujo»): los mismos píxeles con menos trabajo. Cada prueba falla con la
 *  mutación que dice su comentario (comprobado).
 *
 *    D1  una sola limpieza por cuadro: ui_dibuja no borra las pantallas que
 *        se borran (o se cubren) solas, y aun así cada pantalla da el mismo
 *        cuadro sea cual sea lo que había debajo
 *    D2  gfx_clear por palabras: cada píxel, con cualquier color
 *    D3  gfx_hline por palabras: cada trazo, como el de la regla
 *    D4  el texto sin gfx_px: como el motor de antes, cortado o no
 *    D5  gfx_isin sin %: de -3000 a 3000 grados y los extremos
 *    D6  la caché de las curvas del Bode: la clave entera, y que se usa
 *    D7  el osciloscopio: las columnas como la regla, y las filas guardadas
 *    D10 las filas por pares (gfx_fila_pares)
 *
 *  D8, D9 y lo demás de D1 y D10 son de la versión especial (ESP_OD_*).
 *
 *  Se incluye tras test_r8_pantallas.inc (usa cm_osc, cm_bode, cm_fasorial,
 *  cm_barrido, cm_ca, cm_editor, ci_monta, r3u_abre, u4_serie, u4_dato,
 *  TECLAS, sr_init, sr_el, canal, sr, saj, bode_sr, pon y cierra_reg).
 *  Va al final de la batería, tras [74]: sus
 *  teclas no cambian la semilla de nadie. No deja BMP.
 * ========================================================================= */

static u16 od_fb[SCR_W * SCR_H];

/* ¿El mismo cuadro sobre dos fondos distintos? Si lo de debajo no se ve,
 * da lo mismo que ui_dibuja lo borre o no */
static int od_indep(void)
{
    gfx_clear(RGB(8, 8, 8));
    ui_dibuja();
    memcpy(od_fb, gfx_buffer(), sizeof od_fb);
    gfx_clear(RGB(255, 0, 255));
    ui_dibuja();
    return memcmp(od_fb, gfx_buffer(), sizeof od_fb) == 0;
}

#if ESPECIAL
#include "test_opt_dibujo_esp.inc"      /* la versión especial (no se publica) */
#else
#define ESP_OD_D1
#define ESP_OD_D8
#define ESP_OD_D9
#define ESP_OD_D10
#endif

/* ---- D1. Una sola limpieza por cuadro ----
 * Mutaciones: P_CA en se_borra_sola (ui.c) falla «la CA»; sin el
 * «!= FIN_ERROR» del Bode o del barrido fallan sus errores */
static void od_una_limpieza(void)
{
    const char *q = "D1 una limpieza por cuadro";
    static Circuito c;
    int k;

    cm_editor();
    chk_cond(q, "el editor (ui_dibuja lo borra)", ui_pantalla() == P_EDITOR && od_indep());
    cm_osc();
    chk_cond(q, "el osciloscopio", ui_pantalla() == P_OSC && od_indep());
    TECLAS(T_LETRA('X'));
    chk_cond(q, "el osciloscopio con cursores", ui_pantalla() == P_OSC && od_indep());
    cm_ca();
    chk_cond(q, "la CA (ui_dibuja la borra)", ui_pantalla() == P_CA && od_indep());
    TECLAS(T_ENTER);
    chk_cond(q, "el diagrama fasorial", ui_pantalla() == P_FASORIAL && od_indep());
    TECLAS(T_DER);
    chk_cond(q, "el tri" S_aa "ngulo de potencias", ui_pantalla() == P_FASORIAL && od_indep());
    cm_bode();
    chk_cond(q, "el Bode", ui_pantalla() == P_BODE && bode_res()->fin == FIN_COMPLETO && od_indep());
    cm_barrido();
    chk_cond(q, "el barrido", ui_pantalla() == P_BARRIDO && barr_res()->fin != FIN_ERROR && od_indep());

    /* los errores: dib_bode y dib_barrido pintan su texto sin borrar */
    circ_init(&c, "Sin fuente");
    k = circ_elemento(&c, EL_RES, 0, 0, DIR_DER); u4_dato(&c.el[k].val, 1e3);
    r3u_abre(&c);
    TECLAS(T_LETRA('B'));
    chk_cond(q, "el Bode con error", ui_pantalla() == P_BODE && bode_res()->fin == FIN_ERROR && od_indep());
    u4_serie(&c);
    c.el[1].val.est = M_LIBRE;                          /* R1 sin valor: el barrido no arranca */
    r3u_abre(&c);
    TECLAS(T_LETRA('I'));
    chk_cond(q, "el barrido con error", ui_pantalla() == P_BARRIDO && barr_res()->fin == FIN_ERROR && od_indep());

    ESP_OD_D1
}

/* ---- D2. gfx_clear por palabras ----
 * Mutaciones: la palabra sin la mitad de arriba (v = c) o una vuelta de
 * menos (fin - 8) dejan píxeles sin borrar */
static void od_clear(void)
{
    const char *q = "D2 gfx_clear por palabras";
    static const u16 C[5] = { 0x0000, 0xFFFF, 0x0001, 0x8000, 0x1234 };
    const u16 *fb = gfx_buffer();
    int i, j, mal = 0;
    chk_cond(q, "el backbuffer, alineado a 4 bytes", ((size_t)fb & 3u) == 0);
    for (i = 0; i < 5; i++) {
        gfx_clear((u16)~C[i]);
        gfx_clear(C[i]);
        for (j = 0; j < SCR_W * SCR_H; j++) if (fb[j] != C[i]) mal++;
    }
    chk_cond(q, "cinco colores (las dos mitades distintas de cero): los 76 800 p" S_ii "xeles", mal == 0);
    gfx_clip(10, 10, 20, 20);
    gfx_clear(0x5555);
    for (mal = 0, j = 0; j < SCR_W * SCR_H; j++) if (fb[j] != 0x5555) mal++;
    gfx_clip_reset();
    chk_cond(q, "no mira el recorte: la pantalla entera", mal == 0);
}

/* ---- D3. gfx_hline por palabras ----
 * Cada x de -3 a 12 y de 306 a 321, cada ancho de -1 a 14, en filas pares
 * e impares (la primera y la última del backbuffer), con y sin recorte: la
 * fila tiene que salir como la de la regla (píxel a píxel) y las de al
 * lado, sin tocar. Mutaciones: sin el suelto de la izquierda o el de la
 * derecha, o con «p < fin» */
static void od_hline(void)
{
    const char *q = "D3 gfx_hline por palabras";
    static const int Y[4] = { 0, 1, 118, 239 };
    static const int CL[3][2] = { { 0, SCR_W - 1 }, { 5, 9 }, { 4, 310 } };
    const u16 *fb = gfx_buffer();
    int iy, ic, lado, x, w, p, mal = 0, fuera = 0, casos = 0;
    for (iy = 0; iy < 4; iy++)
        for (ic = 0; ic < 3; ic++)
            for (lado = 0; lado < 2; lado++)
                for (x = lado ? 306 : -3; x <= (lado ? 321 : 12); x++)
                    for (w = -1; w <= 14; w++) {
                        int y = Y[iy], c0 = CL[ic][0], c1 = CL[ic][1];
                        gfx_clear(0x0841);
                        gfx_clip(c0, 0, c1 - c0 + 1, SCR_H);
                        gfx_hline(x, y, w, 0xF81F);
                        gfx_clip_reset();
                        casos++;
                        for (p = 0; p < SCR_W; p++) {
                            int dentro = p >= x && p <= x + w - 1 && p >= c0 && p <= c1;
                            if (fb[y * SCR_W + p] != (dentro ? 0xF81F : 0x0841)) mal++;
                        }
                        if (y > 0) for (p = 0; p < SCR_W; p++) if (fb[(y - 1) * SCR_W + p] != 0x0841) fuera++;
                        if (y < SCR_H - 1) for (p = 0; p < SCR_W; p++) if (fb[(y + 1) * SCR_W + p] != 0x0841) fuera++;
                    }
    printf("    %d trazos: %d p" S_ii "xeles mal en su fila y %d en las de al lado\n", casos, mal, fuera);
    chk_cond(q, "cada trazo, como el de la regla, p" S_ii "xel a p" S_ii "xel", mal == 0);
    chk_cond(q, "las filas de al lado, sin tocar", fuera == 0);
    gfx_clear(0x0841);
    gfx_clip(0, 10, SCR_W, 5);
    gfx_hline(0, 9, SCR_W, 0xF81F);
    gfx_hline(0, 15, SCR_W, 0xF81F);
    gfx_clip_reset();
    for (mal = 0, p = 0; p < SCR_W * SCR_H; p++) if (fb[p] != 0x0841) mal++;
    chk_cond(q, "las filas fuera del recorte, sin tocar", mal == 0);
}

/* ---- D4. El texto sin gfx_px ----
 * La regla: el motor de antes (gfx_px, o gfx_fill con escala, por punto).
 * Los 224 caracteres de 0x20 a 0xFF, en las dos fuentes y con escala 1 a
 * 3, cortados por cada borde de la pantalla y de un recorte. Mutaciones:
 * sin mirar el borde de arriba o el derecho del recorte, o bajando una
 * sola fila por fila de glifo con escala */
static void od_texto_regla(int x, int y, const char *s, u16 fg, int escala, int chica)
{
    int px = x, gy, gx, alto = chica ? FONT_H_P : FONT_H, ancho = chica ? 5 : 6;
    for (; *s; s++, px += FONT_W * escala) {
        const u8 *g = chica ? font_glifo((unsigned char)*s) : font_glifo12((unsigned char)*s);
        for (gy = 0; gy < alto; gy++)
            for (gx = 0; gx < ancho; gx++) {
                if (!(g[gy] & (1u << gx))) continue;
                if (escala == 1) gfx_px(px + gx, y + gy, fg);
                else gfx_fill(px + gx * escala, y + gy * escala, escala, escala, fg);
            }
    }
}

static void od_texto(void)
{
    const char *q = "D4 el texto sin gfx_px";
    static const int X[11] = { -13, -7, -1, 0, 1, 5, 151, 300, 310, 313, 316 };
    static const int Y[9] = { -13, -5, 0, 3, 100, 227, 228, 235, 239 };
    char s[5][48];
    int i, ix, iy, e, ic, n = 0, mal = 0;
    for (i = 0; i < 224; i++) s[i / 45][i % 45] = (char)(0x20 + i);
    for (i = 0; i < 5; i++) s[i][i < 4 ? 45 : 224 - 180] = 0;
    gfx_desbordes_silencio(1);
    for (i = 0; i < 5; i++)
        for (ix = 0; ix < 11; ix++)
            for (iy = 0; iy < 9; iy++)
                for (e = 0; e < 4; e++)                     /* 0: la 6x8; 1 a 3: la 6x12 con esa escala */
                    for (ic = 0; ic < 2; ic++) {
                        int x = X[ix], y = Y[iy];
                        if (e >= 2) y = y * 2 / e - (e == 3 ? 6 : 0);
                        gfx_clear(0x0841);
                        if (ic) gfx_clip(9, 20, 290, 180);
                        od_texto_regla(x, y, s[i], 0xF81F, e ? e : 1, e == 0);
                        memcpy(od_fb, gfx_buffer(), sizeof od_fb);
                        gfx_clear(0x0841);
                        if (e == 0) gfx_text_p(x, y, s[i], 0xF81F);
                        else gfx_text(x, y, s[i], 0xF81F, e);
                        gfx_clip_reset();
                        n++;
                        if (memcmp(od_fb, gfx_buffer(), sizeof od_fb) != 0) mal++;
                    }
    gfx_desbordes_silencio(0);
    gfx_desbordes_reset();
    printf("    %d textos: %d distintos de la regla\n", n, mal);
    chk_cond(q, "las dos fuentes, escala 1 a 3, cortados por la pantalla y por un recorte: como la regla", n == 3960 && mal == 0);
}

/* ---- D5. gfx_isin sin % dentro de [0, 720) ----
 * La regla: la de antes (grados % 360 siempre; la tabla, la de gfx_isin en
 * [0, 90]). Mutaciones: sin el «d += 360» de los negativos, o restando 360
 * hasta 1080 */
static int od_isin_regla(int g)
{
    int d = g % 360;
    if (d < 0) d += 360;
    if (d <= 90) return gfx_isin(d);
    if (d <= 180) return gfx_isin(180 - d);
    if (d <= 270) return -gfx_isin(d - 180);
    return -gfx_isin(360 - d);
}

static void od_isin(void)
{
    const char *q = "D5 gfx_isin sin % en [0, 720)";
    static const int EXTREMO[6] = { 2147483647, -2147483647 - 1, 2147483557, -2147483558, 1000000, -1000000 };
    int g, mal = 0;
    for (g = -3000; g <= 3000; g++)
        if (gfx_isin(g) != od_isin_regla(g) || gfx_icos(g) != od_isin_regla(g + 90)) mal++;
    for (g = 0; g < 6; g++) if (gfx_isin(EXTREMO[g]) != od_isin_regla(EXTREMO[g])) mal++;
    chk_cond(q, "de -3000 a 3000 grados (seno y coseno) y los extremos de int, como la regla", mal == 0);
}

/* ---- D6. La caché de las curvas del Bode ----
 * Cada comprobación: un cuadro (la caché queda con lo de antes), un cambio
 * y el cuadro siguiente, frente al frío: el mismo Trazador con una copia
 * del registro (otro puntero: la caché no vale); y, con traz_filas_hechas,
 * cuántas curvas rehizo: ninguna sin cambios ni con el cursor movido (su
 * lectura exacta, bode_en, cuenta evaluaciones). Mutaciones: quitar de la
 * clave el registro, el gen, n, lf0, lf1, iesc o tope; guardar también a
 * medio barrer o con gen 0; no guardar nunca */
static BodeReg od_g, od_g2;

/* Un divisor resistivo (plano: |H| = R2/(R1 + R2)) de 10 Hz a 10 kHz; con
 * n > 0, lo empieza y barre solo n puntos */
static int od_divisor(double R1, double R2, int n)
{
    int kE, k, i;
    sr_init(3);
    kE = sr_el(SE_E, 0, 1, 0.0); sr.el[kE].forma = ONDA_CC; sr.el[kE].p[0] = 1.0;
    sr_el(SE_R, 1, 2, R1); k = sr_el(SE_R, 2, 0, R2);
    canal(0, CANAL_V, k); strcpy(saj.canal[0].nombre, "V(R2)");
    if (n <= 0) return bode_sr(kE, 0, -1, -1, 10.0, 1e4, 60.0);
    if (bode_iniciar_red(&sr, kE, &saj.canal[0], -1, -1, 10.0, 1e4, 60.0) != SIM_OK) return -1;
    for (i = 0; i < n; i++) bode_avanzar(1ul);
    return SIM_OK;
}

/* El cuadro de z (con lo que haya en la caché) frente al frío; en *rehizo,
 * las curvas que rehizo el primero */
static int od_traz_frio(Trazador *z, const BodeReg *g, const BodeRes *r, int *rehizo)
{
    Trazador zf = *z;
    int h = traz_filas_hechas();
    traz_dibuja(z, g, r);
    *rehizo = traz_filas_hechas() - h;
    memcpy(od_fb, gfx_buffer(), sizeof od_fb);
    od_g2 = *g;
    traz_dibuja(&zf, &od_g2, r);
    return memcmp(od_fb, gfx_buffer(), sizeof od_fb) == 0;
}

static void od_bode(void)
{
    const char *q = "D6 la cach" S_ee " del Bode";
    static Trazador z;
    static BodeRes r0;
    int k, re, ev;

    chk_cond(q, "el divisor 1k/1k, barrido", od_divisor(1e3, 1e3, 0) == SIM_OK && bode_reg()->n == BODE_N);
    memset(&z, 0, sizeof z);
    traz_nuevo(&z, bode_reg(), bode_res());
    z.idb = 3; z.db_top = 0; z.ifase = 0; z.fase_top = 2;          /* 10 dB/div: el divisor a -6 y a -12 dB */
    traz_dibuja(&z, bode_reg(), bode_res());
    chk_cond(q, "otra vez, sin cambios: ninguna curva rehecha", od_traz_frio(&z, bode_reg(), bode_res(), &re) && re == 0);
#define OD_PASO(cambio, det, nre) do { traz_dibuja(&z, bode_reg(), bode_res()); cambio; \
        chk_cond(q, det, od_traz_frio(&z, bode_reg(), bode_res(), &re) && re == (nre)); } while (0)
    ev = bode_res()->evaluaciones;
    OD_PASO(z.fcur[0] *= 1.7, "el cursor movido: ninguna rehecha", 0);
    chk_cond(q, "(su lectura exacta cont" S_oo " evaluaciones)", bode_res()->evaluaciones > ev);
    OD_PASO(z.idb = 4, "otra escala de dB (idb): la magnitud, rehecha", 1);
    OD_PASO(z.db_top = -1, "otro tope de dB: la magnitud, rehecha", 1);
    OD_PASO(z.ifase = 1, "otra escala de la fase (ifase): la fase, rehecha", 1);
    OD_PASO(z.fase_top = 3, "otro tope de la fase: la fase, rehecha", 1);
    OD_PASO(z.lf0 += 0.25, "otra ventana (lf0): las dos, rehechas", 2);
    OD_PASO(z.lf1 -= 0.25, "otra ventana (lf1): las dos, rehechas", 2);
    z.idb = 3; z.db_top = 0;
    traz_dibuja(&z, bode_reg(), bode_res());
    chk_cond(q, "otro barrido (3k/1k: -12 dB) con la misma malla y la misma ventana (el gen)",
             od_divisor(3e3, 1e3, 0) == SIM_OK && od_traz_frio(&z, bode_reg(), bode_res(), &re) && re == 2);

    /* a medio barrer (fin = FIN_NO): más puntos con el mismo gen y la misma n */
    chk_cond(q, "el 1k/1k a medio barrer", od_divisor(1e3, 1e3, 60) == SIM_OK && bode_reg()->n == BODE_N &&
             bode_res()->fin == FIN_NO);
    OD_PASO(for (k = 0; k < 90; k++) bode_avanzar(1ul), "90 puntos m" S_aa "s, el mismo gen y la misma n: las dos, rehechas", 2);
    OD_PASO(bode_cortar(), "cortado: las dos, rehechas", 2);
    OD_PASO((void)0, "cortado, otra vez: ninguna", 0);

    /* otro registro con el mismo gen; el mismo con menos puntos; uno de gen 0 que cambia */
    od_divisor(1e3, 1e3, 0);
    od_g = *bode_reg();
    for (k = 0; k < od_g.n; k++) od_g.db[k] -= 7.0;
    traz_dibuja(&z, bode_reg(), bode_res());
    chk_cond(q, "otro registro (-7 dB) con el mismo gen", od_traz_frio(&z, &od_g, bode_res(), &re) && re == 2);
    traz_dibuja(&z, &od_g, bode_res());
    od_g.n = 150;
    chk_cond(q, "el mismo registro con 150 puntos (n)", od_traz_frio(&z, &od_g, bode_res(), &re) && re == 2);
    od_g.n = BODE_N;
    r0 = *bode_res();
    r0.gen = 0;
    traz_dibuja(&z, &od_g, &r0);
    for (k = 0; k < od_g.n; k++) { od_g.db[k] += 3.0; od_g.fase[k] -= 20.0; }
    chk_cond(q, "gen 0: el registro cambia sin que cambie nada de la clave", od_traz_frio(&z, &od_g, &r0, &re) && re == 2);
    chk_cond(q, "el de verdad tras el de gen 0: las dos, rehechas", od_traz_frio(&z, bode_reg(), bode_res(), &re) && re == 2);
    OD_PASO((void)0, "y luego guardadas: ninguna", 0);
#undef OD_PASO
}

/* ---- D7. El osciloscopio: las columnas y sus filas ----
 * (a) osc_columnas (el borde que se reutiliza y el índice que avanza)
 *     frente a la regla, la de antes (osc_valor en cada borde): bit a bit,
 *     en una emulación de verdad y en un registro con eventos (dos muestras
 *     con el mismo t) y muestras justo en los bordes, con ventanas que
 *     empiezan antes, acaban después o no tocan el registro, de columnas
 *     sin muestras a columnas con cientos, y un tizq NaN.
 * (b) las filas guardadas en el Osc: el cuadro tras cada cambio frente al
 *     frío (una copia con f_ok a 0); y que sin cambios se usan (una fila
 *     envenenada se ve).
 * Mutaciones: sin el «f_ok[k] = 0» de prepara, sin f_iv o f_pos en la
 * clave, rehacerlas siempre; «t[j1] < t1»; reutilizar el borde sin mirar
 * que sea el mismo t (con tizq NaN) */
static SimReg od_r;
static SimRes od_s;
static Osc od_o, od_of;

static double od_valor_regla(const SimReg *r, int k, double t)
{
    const double *y = r->y[k ? 1 : 0];
    int lo = 0, hi = r->n, j;
    if (r->n <= 0) return 0.0;
    while (lo < hi) { int m = (lo + hi) / 2; if (r->t[m] > t) hi = m; else lo = m + 1; }
    j = lo;
    if (j <= 0) return y[0];
    if (j >= r->n) return y[r->n - 1];
    if (r->t[j - 1] == t) return y[j - 1];
    return y[j - 1] + (y[j] - y[j - 1]) * (t - r->t[j - 1]) / (r->t[j] - r->t[j - 1]);
}

static int od_columnas_regla(const SimReg *r, int k, double tizq, double dcol, int ncol,
                             double *mn, double *mx, unsigned char *ok)
{
    const double *t = r->t, *y = r->y[k ? 1 : 0];
    int c, j, lo0 = 0, hi0 = r->n, nok = 0;
    double tult;
    if (r->n <= 0) { memset(ok, 0, (size_t)ncol); return 0; }
    tult = t[r->n - 1];
    while (lo0 < hi0) { int m = (lo0 + hi0) / 2; if (t[m] > tizq) hi0 = m; else lo0 = m + 1; }
    j = lo0;
    for (c = 0; c < ncol; c++) {
        double t0 = tizq + c * dcol, t1 = tizq + (c + 1) * dcol, a, b, lo, hi;
        if (t0 >= tult || t1 <= t[0]) { ok[c] = 0; continue; }
        if (t1 > tult) t1 = tult;
        a = od_valor_regla(r, k, t0 > t[0] ? t0 : t[0]);
        b = od_valor_regla(r, k, t1);
        lo = a < b ? a : b; hi = a < b ? b : a;
        while (j < r->n && t[j] <= t0) j++;
        while (j < r->n && t[j] <= t1) {
            if (y[j] < lo) lo = y[j];
            if (y[j] > hi) hi = y[j];
            j++;
        }
        mn[c] = lo; mx[c] = hi; ok[c] = 1;
        nok++;
    }
    return nok;
}

/* osc_columnas y la regla, bit a bit (lo que no escriben, con el mismo relleno) */
static int od_cols_igual(const SimReg *r, int k, double tizq, double dcol)
{
    static double mn1[OSC_NCOL], mx1[OSC_NCOL], mn2[OSC_NCOL], mx2[OSC_NCOL];
    static unsigned char ok1[OSC_NCOL], ok2[OSC_NCOL];
    int n1, n2;
    memset(mn1, 0x5A, sizeof mn1); memset(mx1, 0x5A, sizeof mx1); memset(ok1, 0x5A, sizeof ok1);
    memset(mn2, 0x5A, sizeof mn2); memset(mx2, 0x5A, sizeof mx2); memset(ok2, 0x5A, sizeof ok2);
    n1 = od_columnas_regla(r, k, tizq, dcol, OSC_NCOL, mn1, mx1, ok1);
    n2 = osc_columnas(r, k, tizq, dcol, OSC_NCOL, mn2, mx2, ok2);
    return n1 == n2 && memcmp(mn1, mn2, sizeof mn1) == 0 && memcmp(mx1, mx2, sizeof mx1) == 0 &&
           memcmp(ok1, ok2, sizeof ok1) == 0;
}

/* ¿Todas las ventanas de r (los dos canales), como la regla? */
static int od_cols_todas(const SimReg *r, int *casos)
{
    double t0 = r->t[0], tf = r->t[r->n - 1], T = tf - t0, nan = 0.0 / 0.0;
    double TI[9], DC[7];
    int i, j, k, mal = 0;
    TI[0] = t0 - 0.3 * T; TI[1] = t0; TI[2] = t0 + 0.1 * T; TI[3] = t0 + 0.5 * T; TI[4] = tf - 0.2 * T;
    TI[5] = tf + 0.1 * T; TI[6] = t0 - 2.0 * T; TI[7] = 0.0; TI[8] = nan;
    DC[0] = T / 300.0; DC[1] = T / 3000.0; DC[2] = T / 30.0; DC[3] = T / 3e5; DC[4] = T * 0.01; DC[5] = 0.01; DC[6] = T / 299.0;
    for (k = 0; k < 2; k++)
        for (i = 0; i < 9; i++)
            for (j = 0; j < 7; j++) { (*casos)++; if (!od_cols_igual(r, k, TI[i], DC[j])) mal++; }
    return mal;
}

/* El cuadro de o (con sus filas guardadas) frente al frío */
static int od_osc_frio(Osc *o, const SimReg *r, const SimRes *s)
{
    osc_dibuja(o, r, s);
    memcpy(od_fb, gfx_buffer(), sizeof od_fb);
    od_of = *o;
    od_of.f_ok[0] = od_of.f_ok[1] = 0;
    osc_dibuja(&od_of, r, s);
    return memcmp(od_fb, gfx_buffer(), sizeof od_fb) == 0;
}

static void od_osc(void)
{
    const char *q = "D7 el osciloscopio";
    int i, c, casos = 0, mal;

    /* (a) las columnas */
    cm_osc();
    mal = od_cols_todas(sim_reg(), &casos);
    memset(&od_r, 0, sizeof od_r);
    for (i = 0; i <= 100; i++) {                      /* muestras en 0,01·i; eventos en 0,25 y 0,5 */
        double t = i * 0.01;
        pon(&od_r, t, sin(7.0 * t) + (t >= 0.25 ? 1.0 : 0.0), t < 0.5 ? -t : 2.0 - t);
        if (i == 25 || i == 50) pon(&od_r, t, sin(7.0 * t) + 1.0, t < 0.5 ? -0.25 : 1.5);
    }
    for (i = 1; i <= 400; i++) pon(&od_r, 1.0 + i * 1e-4, cos(300.0 * i * 1e-4), 1.5 + 0.001 * i);   /* y un tramo denso */
    cierra_reg(&od_r);
    mal += od_cols_todas(&od_r, &casos);
    printf("    %d ventanas: %d con otras columnas que la regla\n", casos, mal);
    chk_cond(q, "las columnas, bit a bit como la regla (una emulaci" S_oo "n y un registro con eventos)", casos == 252 && mal == 0);

    /* (b) las filas, con el registro de eventos y los dos canales */
    memset(&od_s, 0, sizeof od_s);
    od_s.t_fin = od_s.t_pedido = od_r.t[od_r.n - 1]; od_s.fin = FIN_COMPLETO;
    od_s.canal[0].tipo = CANAL_V; od_s.canal[1].tipo = CANAL_I;
    strcpy(od_s.canal[0].nombre, "V(C1)"); strcpy(od_s.canal[1].nombre, "I(R1)");
    osc_nuevo(&od_o, &od_r, &od_s, 1);
    osc_dibuja(&od_o, &od_r, &od_s);
    chk_cond(q, "otra vez, sin cambios", od_osc_frio(&od_o, &od_r, &od_s) && od_o.f_ok[0] && od_o.f_ok[1]);
    for (c = 0; c < OSC_NCOL && !od_o.ok[0][c]; c++) {}
    if (c < OSC_NCOL) od_o.f_y0[0][c] = (unsigned char)(od_o.f_y0[0][c] < OSC_Y0 + 60 ? OSC_Y0 + 120 : OSC_Y0 + 10);
    chk_cond(q, "sin cambios, las filas guardadas se usan (una envenenada se ve)", c < OSC_NCOL && !od_osc_frio(&od_o, &od_r, &od_s));
    od_o.f_ok[0] = 0;
#define OD_OPASO(cambio, det) do { osc_dibuja(&od_o, &od_r, &od_s); cambio; \
        chk_cond(q, det, od_osc_frio(&od_o, &od_r, &od_s)); } while (0)
    OD_OPASO(od_o.ivdiv[0]--, "otra escala del canal 1");
    OD_OPASO(od_o.pos[1] += 2, "otra posici" S_oo "n del canal 2");
    OD_OPASO(od_o.ivdiv[1] -= 2, "otra escala del canal 2");
    OD_OPASO(od_o.pos[0]--, "otra posici" S_oo "n del canal 1");
    OD_OPASO(od_o.tizq += 0.1, "otra ventana (tizq)");
    OD_OPASO(od_o.itdiv--, "otra escala de tiempo");
    OD_OPASO((od_o.cursores = 1, od_o.tc[0] = 0.3, od_o.tc[1] = 0.6), "los cursores (las medidas, entre ellos)");
    OD_OPASO(od_o.tc[1] = 0.7, "un cursor movido");
    OD_OPASO(osc_tecla(&od_o, &od_r, &od_s, T_ARRIBA | T_CON_SHIFT), "[shift][arr]: sube el canal activo");
    OD_OPASO(osc_tecla(&od_o, &od_r, &od_s, T_ABAJO), "[ab]: otra escala del activo");
    for (i = 0; i < od_r.n; i++) { od_r.y[0][i] *= -0.5; od_r.y[1][i] += 0.25; }
    cierra_reg(&od_r);                                 /* otra emulación: otro gen, las escalas a mano */
    OD_OPASO(osc_nuevo(&od_o, &od_r, &od_s, 0), "otra emulaci" S_oo "n con las mismas escalas");
    OD_OPASO(osc_nuevo(&od_o, &od_r, &od_s, 1), "otra emulaci" S_oo "n con autoescala");
#undef OD_OPASO
}

/* ---- D10. Las filas por pares de píxeles (gfx_fila_pares) ----
 * gfx_fila_pares: cada byte pinta los dos píxeles de su palabra de la tabla
 * (el de la izquierda, el de la dirección más baja), en la fila de arriba y
 * en la de abajo, con 160, 1 y 0 bytes; las demás filas, sin tocar.
 * Mutaciones: un par de menos por fila, otra fila */
static int od_fila_pares_bien(int y, int n)
{
    static u32 par[256];
    union { u16 h[2]; u32 w; } u;
    static unsigned char f[160];
    const u16 *fb = gfx_buffer();
    int i, x, mal = 0;
    for (i = 0; i < 256; i++) { u.h[0] = (u16)(0x1000 + i); u.h[1] = (u16)(0x8000 + 3 * i); par[i] = u.w; }
    for (i = 0; i < 160; i++) f[i] = (unsigned char)(i * 37 + 11);
    gfx_clear(0x0841);
    gfx_fila_pares(y, f, n, par);
    for (i = 0; i < SCR_W * SCR_H; i++) {
        int fy = i / SCR_W;
        x = i % SCR_W;
        if (fy == y && x < 2 * n) { if (fb[i] != (x & 1 ? 0x8000 + 3 * f[x / 2] : 0x1000 + f[x / 2])) mal++; }
        else if (fb[i] != 0x0841) mal++;
    }
    return mal == 0;
}

static void od_pares(void)
{
    const char *q = "D10 las filas por pares";
    chk_cond(q, "gfx_fila_pares: la fila de arriba, la de abajo, 1 y 0 pares",
             od_fila_pares_bien(0, 160) && od_fila_pares_bien(SCR_H - 1, 160) && od_fila_pares_bien(77, 1) &&
             od_fila_pares_bien(78, 0));
    ESP_OD_D10
}

static void test_opt_dibujo(void)
{
    printf("[75d] La optimizaci" S_oo "n del dibujo: los mismos p" S_ii "xeles con menos trabajo\n");
    od_una_limpieza();
    od_clear();
    od_hline();
    od_texto();
    od_isin();
    od_bode();
    od_osc();
    ESP_OD_D8
    ESP_OD_D9
    od_pares();
}
