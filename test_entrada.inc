/* ============================================================================
 *  test_entrada.inc  -  [71] La capa de entrada (entrada.c y bucle.c) en
 *  tiempo virtual. HAKARU_SOLO_CAPA la corre sola.
 *
 *  La plataforma simulada: una MANO (las teclas físicas TF_*, más bits crudos
 *  de la matriz para las teclas sin uso: trig, π, var), un DEDO en el pad
 *  (contacto, hundido, x, y; la y crece hacia arriba) y un RELOJ VIRTUAL en
 *  µs: la espera salta al tick siguiente (10,25 ms, el de idle) y el volcado
 *  suma lo que cuesta cada pantalla en la calculadora (lo medido, volcado
 *  incluido: portada 7,5 ms, menú 9,3, editor 10,1, osciloscopio 14,8,
 *  barrido 16,7, CA 16,2, fasorial 19,8, tabla 13,1, Mis 13,6, Bode 38,3...).
 *  Nada de clock(): el resultado no depende del PC.
 *
 *  Con -DPOR_LA_CAPA (build_host.sh capa), teclas() conduce la suite entera
 *  por aquí (capa_tecla: pulsa() y clic_en() en la cuña de cada flecha, con
 *  shift o ctrl abajo) y tiene que dar los mismos checks y BMP.
 *
 *  Las pruebas, en el orden del diseño (controles.json, «pruebas»):
 *   1. independencia del dibujo; 2. flancos; 3. shift y ctrl; 4. la
 *   repetición exacta; 5. una pantalla cara; 6. la lista negra; 7. el cable
 *   con [W]; 8. el touchpad (y con otro tamaño de pad); 9. la energía; 10. la
 *   cola y los cálculos largos; 11. (de la versión especial); y además los saltos con
 *   shift y ctrl, el panel Controles y el grabador; 12. lo que encontró la
 *   revisión (buscar la flecha con el dedo, la deriva del pulgar, el reloj
 *   vigilado con un contador simulado, los fantasmas con [esc] y el de
 *   rectángulo, dos [esc] con la cola llena, el grabador de 10 s y el
 *   tamaño del pad).
 * ========================================================================= */
#include <stddef.h>
#include "entrada.h"
#include "bucle.h"

#define CE_TICK   10250UL                  /* µs: el tick del SO (idle)          */
#define CE_NEV    4096
#define CE_NPROG  8
#define CE_W      TP_W_OMISION
#define CE_H      TP_H_OMISION
#define AYUDA_VIS_T 15                     /* las líneas que se ven de la Ayuda  */
#define TB_VIS_T    13                     /* las filas que se ven de una tabla  */

typedef struct {
    Plataforma pf;
    unsigned long t_us;                    /* el reloj virtual                   */
    unsigned char tf[TF_N];                /* teclas de la mano, abajo           */
    unsigned short extra[8];               /* bits crudos de más (sin traducción) */
    int contacto, hundido, x, y, w, h;     /* el dedo                            */
    int fallan;                            /* los scans fallan                   */
    long scans, cuadros, eventos, trabajos;
    double ms_cuadro, ms_tecla, ms_tick;   /* ms_cuadro > 0: en vez de la tabla  */
    int salio;                             /* bucle_paso dio 0                   */
    int ev[CE_NEV]; int nev;               /* los eventos que llegaron a ui_tecla */
    unsigned long evt[CE_NEV];             /* y cuándo (µs)                      */
    int ult;                               /* el último                          */
    unsigned long t_hunde, t_suelta;       /* del último clic mantenido          */
    struct { int tf; unsigned long a, b; } prog[CE_NPROG];   /* teclas programadas en el tiempo */
    int nprog;
} Mano;
static Mano ce;
static int ce_listo;                       /* la sesión de capa_tecla está abierta */

#if ESPECIAL
#include "test_entrada_esp.inc"         /* la versión especial (no se publica) */
#else
#define ESP_CE_COSTE
#define ESP_CE_INDEP
#define ESP_CE_FLANCOS
#define ESP_CE_PLAN_B
#define ESP_CE_11
#endif

/* Lo que cuesta un cuadro de cada pantalla en la calculadora (ms, con el volcado) */
static double ce_coste(int p)
{
    switch (p) {
    case P_INICIO: return 7.5;   case P_MENU: return 9.3;     case P_TALLER: return 8.7;
    case P_LECCIONES: return 8.7; case P_EJEMPLOS: return 8.7; case P_MODELO: return 8.7;
    case P_TEORIA: return 9.3;   case P_AYUDA: return 9.1;    case P_MIS: return 13.6;
    case P_EDITOR: return 10.1;  case P_ELEMENTO: return 9.4; case P_RESULTADO: return 10.6;
    case P_TABLA: return 13.1;   case P_PASOS: return 10.7;   ESP_CE_COSTE
    case P_EMULANDO: return 7.2;
    case P_OSC: return 14.8;     case P_EMU_TABLA: return 9.7; case P_EMU_ESQ: return 9.7;
    case P_AJUSTES: return 7.4;  case P_CA: return 16.2;      case P_CA_TABLA: return 13.1;
    case P_FASORIAL: return 19.8; case P_BODE: return 38.3;   case P_THEVENIN: return 8.4;
    case P_BARRIDO: return 16.7; default: return 7.5;
    }
}

static int ce_abajo_en(int tf, unsigned long t)
{
    int i;
    if (ce.tf[tf]) return 1;
    for (i = 0; i < ce.nprog; i++) if (ce.prog[i].tf == tf && t >= ce.prog[i].a && t < ce.prog[i].b) return 1;
    return 0;
}

static void ce_foto(Plataforma *pf, EntMuestra *m, int con_pad)
{
    int i, f;
    unsigned b;
    (void)pf;
    for (i = 0; i < 8; i++) m->fila[i] = ce.extra[i];
    for (i = 0; i < TF_N; i++)
        if (ce_abajo_en(i, ce.t_us) && ent_tecla_fis(i, &f, &b)) m->fila[f] = (unsigned short)(m->fila[f] | b);
    m->pad = ENT_PAD_NO;
    if (!con_pad) return;
    ce.scans++;
    if (ce.fallan) { m->pad = ENT_PAD_ERROR; return; }
    m->pad = ENT_PAD_OK;
    m->contacto = (unsigned char)(ce.contacto || ce.hundido);
    m->hundido = (unsigned char)ce.hundido;
    m->x = (unsigned short)ce.x;
    m->y = (unsigned short)ce.y;
}

static unsigned long ce_reloj(Plataforma *pf) { (void)pf; return ce.t_us / 1000UL; }
static void ce_espera(Plataforma *pf) { (void)pf; ce.t_us = (ce.t_us / CE_TICK + 1) * CE_TICK; }
static void ce_vuelca(Plataforma *pf)
{
    (void)pf;
    ce.cuadros++;
    ce.t_us += (unsigned long)((ce.ms_cuadro > 0.0 ? ce.ms_cuadro : ce_coste(ui_pantalla())) * 1000.0);
}
static void ce_tras_tecla(Plataforma *pf, int t)
{
    (void)pf;
    ce.eventos++;
    ce.ult = t;
    if (ce.nev < CE_NEV) { ce.evt[ce.nev] = ce.t_us; ce.ev[ce.nev++] = t; }
    ce.t_us += (unsigned long)(ce.ms_tecla * 1000.0);
}
static void ce_tras_tick(Plataforma *pf) { (void)pf; ce.trabajos++; ce.t_us += (unsigned long)(ce.ms_tick * 1000.0); }

/* Una plataforma nueva (la mano suelta); ce_empieza hace la foto del arranque */
static void ce_nueva(int w, int h, int sin_animar)
{
    memset(&ce, 0, sizeof ce);
    ce.t_us = 1000UL * CE_TICK;            /* ~10 s: lejos del cero */
    ce.w = w; ce.h = h;
    ce.pf.foto = ce_foto; ce.pf.reloj = ce_reloj; ce.pf.espera = ce_espera; ce.pf.vuelca = ce_vuelca;
    ce.pf.tras_tecla = ce_tras_tecla; ce.pf.tras_tick = ce_tras_tick;
    ce.pf.sin_animar = sin_animar;
    ce.pf.w = w; ce.pf.h = h;
    ce_listo = 0;                           /* capa_tecla abrirá la suya */
}
static void ce_empieza(void) { bucle_inicia(&ce.pf); }
static void ce_abre(int w, int h, int sin_animar) { ce_nueva(w, h, sin_animar); ce_empieza(); }

/* Corre el bucle hasta ms más tarde */
static void ce_pasa(double ms)
{
    unsigned long fin = ce.t_us + (unsigned long)(ms * 1000.0 + 0.5);
    long n = 0;
    while (ce.t_us < fin && n++ < 2000000L) if (!bucle_paso(&ce.pf)) ce.salio = 1;
}

/* La mano */
static void ce_abajo(int tf) { ce.tf[tf] = 1; }
static void ce_arriba(int tf) { ce.tf[tf] = 0; }
static void ce_pulsa(int tf) { ce_abajo(tf); ce_pasa(90); ce_arriba(tf); ce_pasa(60); }
static void ce_mantiene(int tf, double ms) { ce_abajo(tf); ce_pasa(ms); ce_arriba(tf); }
static void ce_programa(int tf, double desde, double dura)
{
    if (ce.nprog >= CE_NPROG) return;
    ce.prog[ce.nprog].tf = tf;
    ce.prog[ce.nprog].a = ce.t_us + (unsigned long)(desde * 1000.0);
    ce.prog[ce.nprog].b = ce.prog[ce.nprog].a + (unsigned long)(dura * 1000.0);
    ce.nprog++;
}

/* El dedo, en milésimas del pad (la y desde abajo) */
static void ce_dedo(int xm, int ym)
{
    if (xm < 0) xm = 0;
    if (xm > 999) xm = 999;
    if (ym < 0) ym = 0;
    if (ym > 999) ym = 999;
    ce.contacto = 1;
    ce.x = xm * ce.w / 1000;
    ce.y = ym * ce.h / 1000;
}
static void ce_fuera(void) { ce.contacto = ce.hundido = 0; }
static void ce_hunde(void) { ce.hundido = 1; ce.t_hunde = ce.t_us; }
static void ce_suelta(void) { ce.hundido = 0; ce.t_suelta = ce.t_us; }
static void ce_clic_en(int xm, int ym) { ce_dedo(xm, ym); ce_pasa(20); ce_hunde(); ce_pasa(90); ce_suelta(); ce_pasa(20); ce_fuera(); ce_pasa(50); }
static void ce_clic_mantiene(int xm, int ym, double ms)
{
    ce_dedo(xm, ym); ce_pasa(20); ce_hunde(); ce_pasa(ms); ce_suelta(); ce_pasa(20); ce_fuera(); ce_pasa(40);
}
/* El centro de la cuña de cada flecha y el del pad */
enum { CE_ARR = 0, CE_ABA, CE_IZQ, CE_DER };
static const int CE_X[4] = { 500, 500, 170, 830 }, CE_Y[4] = { 830, 170, 500, 500 };

static unsigned ce_lcg = 1u;
static unsigned ce_azar(unsigned n) { ce_lcg = ce_lcg * 1103515245u + 12345u; return (ce_lcg >> 16) % n; }
static int ce_ruido(int r) { return r ? (int)ce_azar((unsigned)(2 * r + 1)) - r : 0; }

/* Desliza el dedo de (x0, y0) a (x1, y1) en ms, una muestra por tick, con
 * ruido pseudoaleatorio de ±ruido milésimas (semilla fija). Se queda apoyado */
static void ce_desliza(int x0, int y0, int x1, int y1, double ms, int ruido)
{
    int i, n = (int)(ms * 1000.0 / (double)CE_TICK + 0.5);
    if (n < 1) n = 1;
    for (i = 0; i <= n; i++) {
        ce_dedo(x0 + (x1 - x0) * i / n + ce_ruido(ruido), y0 + (y1 - y0) * i / n + ce_ruido(ruido));
        ce_pasa((double)CE_TICK / 1000.0);
    }
}

/* Con HAKARU_DEPURA: los eventos que llegaron, con su hora relativa a t0 (ms) */
static void ce_lista(const char *que, unsigned long t0)
{
    int i;
    if (!getenv("HAKARU_DEPURA")) return;
    printf("    %s: %d eventos\n", que, ce.nev);
    for (i = 0; i < ce.nev; i++)
        printf("      %8.2f ms  0x%04x\n", ((double)ce.evt[i] - (double)t0) / 1000.0, (unsigned)ce.ev[i]);
}

/* Cuántos eventos llegaron con la tecla k (sin marcas ni modificadores) */
static int ce_cuenta(int k)
{
    int i, n = 0;
    for (i = 0; i < ce.nev; i++) if (ce.ev[i] != T_SUELTA && (ce.ev[i] & ~(T_MODS | T_MARCAS)) == k) n++;
    return n;
}
static int ce_cuenta_marca(int m)
{
    int i, n = 0;
    for (i = 0; i < ce.nev; i++) if (ce.ev[i] != T_SUELTA && (ce.ev[i] & m)) n++;
    return n;
}
static int ce_sueltas(void)
{
    int i, n = 0;
    for (i = 0; i < ce.nev; i++) if (ce.ev[i] == T_SUELTA) n++;
    return n;
}

/* Las repeticiones que vencen de t0 (el flanco) a tl (la última muestra con
 * la tecla abajo), en ms, con los tiempos de un perfil: la fórmula del diseño */
static int ce_reps(long t0, long tl, long retardo, long periodo, long cambio, long periodo2, int tope)
{
    long t = t0 + retardo, h;
    int n = 0, m;
    while (t <= tl) {
        h = t - t0;
        m = tope <= 1 ? 1 : h < 1000 ? 1 : h < 2000 ? 2 : h < 3000 ? 4 : 8;
        if (m > tope) m = tope;
        n += m;
        t += cambio > 0 && t - (t0 + retardo) >= cambio ? periodo2 : periodo;
    }
    return n;
}
/* t0 y tl del último clic mantenido (los ticks: el flanco se ve en la foto
 * de la hora de hundir, y la última muestra abajo es la del tick anterior) */
static long ce_t0(void) { return (long)(ce.t_hunde / 1000UL); }
static long ce_tl(void) { return (long)((ce.t_suelta - CE_TICK) / 1000UL); }

#ifdef POR_LA_CAPA
/* La tecla física de una tecla abstracta, o -1 (las flechas son del pad) */
static int ce_tf_de(int k)
{
    switch (k) {
    case T_ESC: return TF_ESC;       case T_TAB: return TF_TAB;       case T_MENU: return TF_MENU;
    case T_ENTER: return TF_ENTER;   case T_BORRAR: return TF_DEL;    case T_PUNTO: return TF_PUNTO;
    case T_SIGNO: return TF_SIGNO;   case T_MAS: return TF_MAS;       case T_MENOS: return TF_MENOS;
    case T_IGUAL: return TF_IGUAL;   case T_INCOG: return TF_INCOG;   case T_EE: return TF_EE;
    case T_CATALOGO: return TF_CAT;  case T_SHIFT: return TF_SHIFT;   case T_CTRL: return TF_CTRL;
    case T_ESPACIO: return TF_ESPACIO; case T_POR: return TF_POR;     case T_ENTRE: return TF_ENTRE;
    case T_POT: return TF_POT;       case T_CUAD: return TF_CUAD;     case T_PAR_A: return TF_PAR_A;
    case T_PAR_C: return TF_PAR_C;
    default: break;
    }
    if (k >= T_DIG0 && k < T_DIG0 + 10) return TF_0 + (k - T_DIG0);
    if (k >= T_LETRA0 && k < T_LETRA0 + 26) return TF_A + (k - T_LETRA0);
    return -1;
}

/* La regresión: cada tecla del guion, con la mano. Una flecha es un clic en
 * el centro de su cuña; T_CON_SHIFT, shift abajo durante la pulsación. Los
 * ticks de las pantallas animadas los da la prueba, como con ui_tecla. */
static long ce_n_capa, ce_n_clics, ce_n_malas;

static void capa_tecla(int t)
{
    int k = t & ~T_MODS, tf = ce_tf_de(k), flecha = k >= T_ARRIBA && k <= T_DER;
    long e0;
    if (!ce_listo) { ce_abre(CE_W, CE_H, 1); ce_listo = 1; }
    if (!flecha && tf < 0) { ui_tecla(t); return; }        /* no es una tecla (T_NADA) */
    e0 = ce.eventos;
    if (t & T_CON_SHIFT) { ce_abajo(TF_SHIFT); ce_pasa(15); }
    if (t & T_CON_CTRL) { ce_abajo(TF_CTRL); ce_pasa(15); }
    if (flecha) { ce_clic_en(CE_X[k - T_ARRIBA], CE_Y[k - T_ARRIBA]); ce_n_clics++; }
    else ce_pulsa(tf);
    if (t & T_MODS) { ce_arriba(TF_SHIFT); ce_arriba(TF_CTRL); ce_pasa(30); }
    ce_n_capa++;
    /* la mano tiene que haber dado exactamente esa tecla, una vez: si no, es
     * un fallo (sin contar una comprobación: la cuenta ha de ser la misma
     * que sin la capa) */
    if (ce.eventos != e0 + 1 || ce.ult != t) {
        ce_n_malas++;
        fallos++;
        if (ce_n_malas <= 10)
            printf("  FALLO  por la capa: la tecla 0x%x dio %ld eventos (el último 0x%x) en la pantalla %d\n",
                   (unsigned)t, ce.eventos - e0, (unsigned)ce.ult, ui_pantalla());
    }
}

/* Lo que pasó por la capa (al final de la suite) */
static void ce_informe(void)
{
    printf("    por la capa: %ld teclas del guion (%ld con clic en el pad), %ld mal entregadas\n",
           ce_n_capa, ce_n_clics, ce_n_malas);
}
#endif

/* --------------------------------------------------------------------------
 *  Los circuitos y los montajes (cada uno deja la interfaz en su pantalla)
 * ----------------------------------------------------------------------- */
static Circuito ce_c;

/* Escalera: E1 (0,1)->(0,0), nueve R arriba y nueve peldaños (modo 0: R; 1:
 * C en los impares; 2: un diodo en el último), el riel de abajo de cable:
 * 19 elementos, más filas de las que caben en una tabla */
static void ce_escalera(Circuito *c, int modo)
{
    int k, x;
    circ_init(c, "Escalera");
    k = circ_elemento(c, EL_FUENTE, 0, 1, DIR_ARR); u4_dato(&c->el[k].val, 10.0);
    for (x = 0; x < 9; x++) { k = circ_elemento(c, EL_RES, x, 0, DIR_DER); u4_dato(&c->el[k].val, 100.0 * (x + 1)); }
    for (x = 1; x <= 9; x++) {
        int tipo = modo == 1 && x % 2 ? EL_COND : modo == 2 && x == 9 ? EL_DIODO : EL_RES;
        k = circ_elemento(c, (TipoEl)tipo, x, 0, DIR_ABA);
        if (tipo == EL_COND) u4_dato(&c->el[k].val, 1e-6);
        else if (tipo == EL_RES) u4_dato(&c->el[k].val, 1000.0);
    }
    for (x = 0; x < 9; x++) circ_cable(c, x, 1, DIR_DER);
}

/* RLC serie en alterna: E1 10∠0° V a 60 Hz, R1 10 Ω, L1 10 mH, C1 100 µF */
static void ce_rlc(Circuito *c)
{
    int k;
    circ_init(c, "RLC");
    k = circ_elemento(c, EL_FUENTE, 0, 1, DIR_ARR);
    c->el[k].variante = FORMA_FASOR;
    u4_dato(&c->el[k].val, 10.0); c->el[k].val.ang = 0.0; c->el[k].val.est_ang = M_DATO;
    u4_dato(&c->frec, 60.0);
    k = circ_elemento(c, EL_RES, 0, 0, DIR_DER); u4_dato(&c->el[k].val, 10.0);
    k = circ_elemento(c, EL_BOB, 1, 0, DIR_DER); u4_dato(&c->el[k].val, 10e-3);
    k = circ_elemento(c, EL_COND, 2, 0, DIR_ABA); u4_dato(&c->el[k].val, 100e-6);
    circ_cable(c, 2, 1, DIR_IZQ); circ_cable(c, 1, 1, DIR_IZQ);
}

static void cm_menu(void)      { ui_init(); TECLAS(T_ENTER); }
static void cm_lecciones(void) { cm_menu(); TECLAS(T_ABAJO, T_ENTER); }
static void cm_teoria(void)    { cm_lecciones(); TECLAS(T_ENTER); }
static void cm_ejemplos(void)  { cm_teoria(); TECLAS(T_ENTER); }
static void cm_modelos(void)
{
    int i;
    cm_lecciones();
    repite(T_ABAJO, 10);
    TECLAS(T_ENTER);
    for (i = 0; i < 20 && ui_pantalla() == P_TEORIA; i++) TECLAS(T_ENTER);
}
static void cm_ayuda(void)     { cm_menu(); TECLAS(T_ABAJO, T_ABAJO, T_ENTER); }
static void cm_mis(void)
{
    int i;
    limpia_mis();
    ce_escalera(&ce_c, 0);
    for (i = 0; i < 7; i++) arch_guardar(i, &ce_c);
    ui_init();
    TECLAS(T_ENTER, T_ENTER, T_ABAJO, T_ABAJO, T_ENTER);
}
static void cm_editor(void)    { ce_escalera(&ce_c, 0); r3u_abre(&ce_c); }
static void cm_ficha(void)     { cm_editor(); TECLAS(T_TAB, T_ENTER); }
static void cm_resultado(void) { cm_editor(); TECLAS(T_IGUAL); }
static void cm_tabla(void)     { cm_resultado(); TECLAS(T_TAB); }
static void cm_pasos(void)     { cm_resultado(); TECLAS(T_ENTER); }
static void cm_tabla_nl(void)  { ce_escalera(&ce_c, 2); r3u_abre(&ce_c); TECLAS(T_IGUAL); TECLAS(T_TAB); }
static void cm_osc(void)       { ce_escalera(&ce_c, 1); r3u_abre(&ce_c); TECLAS(T_IGUAL); r4_espera(); }
static void cm_emu_tabla(void) { cm_osc(); TECLAS(T_TAB); }
static void cm_ajustes(void)   { cm_osc(); TECLAS(T_MENU); }
static void cm_ca(void)        { ce_rlc(&ce_c); r3u_abre(&ce_c); TECLAS(T_IGUAL); }
static void cm_ca_tabla(void)  { cm_ca(); TECLAS(T_TAB); }
static void cm_fasorial(void)  { cm_ca(); TECLAS(T_ENTER); }
static void cm_bode(void)      { ce_rlc(&ce_c); r3u_abre(&ce_c); TECLAS(T_LETRA('B')); r4_espera(); }
static void cm_barrido(void)   { u4_serie(&ce_c); r3u_abre(&ce_c); TECLAS(T_LETRA('I')); r4_espera(); }

/* El punto de la malla más cercano al cursor del Bode y del barrido */
static int ce_k_bode(void)
{
    const BodeReg *g = bode_reg();
    double f = ui_trazador()->fcur[ui_trazador()->activo];
    int k, kb = 0;
    for (k = 0; k < g->n; k++) if (fabs(g->f[k] - f) < fabs(g->f[kb] - f)) kb = k;
    return kb;
}
static int ce_k_barr(void)
{
    const BarrReg *g = barr_reg();
    double x = ui_curva()->xc[ui_curva()->cur];
    int k, kb = 0;
    for (k = 0; k < g->n; k++) if (fabs(g->x[k] - x) < fabs(g->x[kb] - x)) kb = k;
    return kb;
}

/* --------------------------------------------------------------------------
 *  1. Independencia del dibujo: 200 teclas al azar (semilla fija) con
 *     ui_dibuja entre una y otra y sin él dan el mismo estado (ui_foto). Hoy
 *     fallaría en la Ayuda, P_TABLA y P_EMU_TABLA (las cotas del scroll eran
 *     del dibujo): lo prueba la mutación mut_cotas
 * ----------------------------------------------------------------------- */
static unsigned char ce_fa[32768], ce_fb[32768];

static void ce_indep(const char *nom, void (*monta)(void), int pant_esperada)
{
    static const int K[] = {
        T_ARRIBA, T_ABAJO, T_IZQ, T_DER, T_ARRIBA, T_ABAJO, T_IZQ, T_DER, T_ABAJO, T_ABAJO,
        T_ARRIBA | T_CON_SHIFT, T_ABAJO | T_CON_SHIFT, T_IZQ | T_CON_SHIFT, T_DER | T_CON_SHIFT,
        T_ARRIBA | T_CON_CTRL, T_ABAJO | T_CON_CTRL, T_IZQ | T_CON_CTRL, T_DER | T_CON_CTRL,
        T_MAS, T_MENOS, T_BORRAR, T_PUNTO, T_DIG(1), T_DIG(5), T_LETRA('A'), T_LETRA('C'),
        T_LETRA('X'), T_LETRA('M'), T_LETRA('N'), T_LETRA('L'), T_LETRA('Y')
    };
    const int NK = (int)(sizeof K / sizeof K[0]);
    static unsigned long suma[200];                          /* la foto tras cada tecla (FNV-1a) */
    char det[96];
    int pasada, i, j, n[2], llega = 0, distinta = -1;
    for (pasada = 0; pasada < 2; pasada++) {
        monta();
        if (pasada == 0) llega = ui_pantalla() == pant_esperada;
        ce_lcg = 4242u;
        for (i = 0; i < 200; i++) {
            int t = K[ce_azar((unsigned)NK)], nb;
            unsigned long h = 2166136261UL;
            if (pasada == 0) ui_dibuja();
            ui_tecla(t);
            if (ui_animando() == UI_TRABAJANDO) r4_espera();
            /* el estado tras CADA tecla, no solo al final: una cota vieja
             * puede no notarse en el estado final */
            nb = ui_foto(ce_fa, (int)sizeof ce_fa);
            for (j = 0; j < nb; j++) h = ((h ^ ce_fa[j]) * 16777619UL) & 0xFFFFFFFFUL;
            if (pasada == 0) { suma[i] = h; if (i == 0) memcpy(ce_fb, ce_fa, sizeof ce_fb); }
            else if (distinta < 0 && suma[i] != h) {
                distinta = i;
                if (i == 0 && getenv("HAKARU_DEPURA")) {             /* la primera: qué variables */
                    const char *ult = "";
                    printf("    %s: tras la primera tecla difieren:", nom);
                    for (j = 0; j < nb; j++)
                        if (ce_fa[j] != ce_fb[j] && strcmp(ult, ui_foto_campo(j)) != 0) {
                            ult = ui_foto_campo(j);
                            printf(" %s(byte %d)", ult, j);
                        }
                    printf("\n");
                }
            }
        }
        n[pasada] = ui_foto(pasada ? ce_fb : ce_fa, (int)sizeof ce_fa);
    }
    sprintf(det, "%s: se llega a la pantalla", nom);
    chk_cond("independencia del dibujo", det, llega);
    sprintf(det, "%s: 200 teclas con y sin dibujo, el mismo estado", nom);
    chk_cond("independencia del dibujo", det, distinta < 0 && n[0] == n[1] && memcmp(ce_fa, ce_fb, (size_t)n[0]) == 0);
    if (getenv("HAKARU_DEPURA") && distinta >= 0) {
        /* otra vez hasta esa tecla, con dibujo y sin él: qué variables difieren */
        const char *ult = "";
        printf("    %s: el estado difiere tras la tecla %d:", nom, distinta + 1);
        for (pasada = 0; pasada < 2; pasada++) {
            monta();
            ce_lcg = 4242u;
            for (i = 0; i <= distinta; i++) {
                int t = K[ce_azar((unsigned)NK)];
                if (pasada == 0) ui_dibuja();
                ui_tecla(t);
                if (ui_animando() == UI_TRABAJANDO) r4_espera();
                if (i == distinta) printf(pasada ? "" : " (0x%x)", (unsigned)t);
            }
            n[pasada] = ui_foto(pasada ? ce_fb : ce_fa, (int)sizeof ce_fa);
        }
        for (j = 0; j < n[0] && j < n[1]; j++)
            if (ce_fa[j] != ce_fb[j] && strcmp(ult, ui_foto_campo(j)) != 0) { ult = ui_foto_campo(j); printf(" %s", ult); }
        printf("\n");
    }
}

static void ce_independencia(void)
{
    ce_indep("men" S_uu, cm_menu, P_MENU);
    ce_indep("lecciones", cm_lecciones, P_LECCIONES);
    ce_indep("teor" S_ii "a", cm_teoria, P_TEORIA);
    ce_indep("ejemplos", cm_ejemplos, P_EJEMPLOS);
    ce_indep("modelos", cm_modelos, P_EJEMPLOS);
    ce_indep("Ayuda", cm_ayuda, P_AYUDA);
    ce_indep("Mis problemas", cm_mis, P_MIS);
    ce_indep("editor", cm_editor, P_EDITOR);
    ce_indep("ficha", cm_ficha, P_ELEMENTO);
    ce_indep("resultado", cm_resultado, P_RESULTADO);
    ce_indep("tabla (19 elementos)", cm_tabla, P_TABLA);
    ce_indep("pasos", cm_pasos, P_PASOS);
    ce_indep("tabla del punto de operaci" S_oo "n", cm_tabla_nl, P_TABLA);
    ce_indep("osciloscopio", cm_osc, P_OSC);
    ce_indep("tabla de la emulaci" S_oo "n", cm_emu_tabla, P_EMU_TABLA);
    ce_indep("ajustes", cm_ajustes, P_AJUSTES);
    ce_indep("CA", cm_ca, P_CA);
    ce_indep("tabla de CA", cm_ca_tabla, P_CA_TABLA);
    ce_indep("fasorial", cm_fasorial, P_FASORIAL);
    ce_indep("Bode", cm_bode, P_BODE);
    ce_indep("barrido", cm_barrido, P_BARRIDO);
    ESP_CE_INDEP
    limpia_mis();
}

/* --------------------------------------------------------------------------
 *  2. Flancos
 * ----------------------------------------------------------------------- */
static void ce_flancos(void)
{
    const char *q = "capa: flancos";
    ESP_CE_FLANCOS
    ui_init();
    ce_nueva(CE_W, CE_H, 1);
    ce_abajo(TF_ENTER);                                      /* el [enter] que lanzó el .tns */
    ce_empieza();
    ce_pasa(300); ce_arriba(TF_ENTER); ce_pasa(100);
    chk_cond(q, "arrancar con [enter] abajo: no pasa nada", ui_pantalla() == P_INICIO && ce.eventos == 0);
    ce_pulsa(TF_ENTER);
    chk_cond(q, "y el [enter] siguiente s" S_ii " cuenta", ui_pantalla() == P_MENU);

    ui_init();
    TECLAS(T_ENTER);
    ce_abre(CE_W, CE_H, 1);
    ce_dedo(CE_X[CE_ABA], CE_Y[CE_ABA]); ce_pasa(20); ce_hunde(); ce_pasa(40);
    ce_abajo(TF_ENTER); ce_pasa(40);                         /* [enter] baja con la flecha abajo */
    ce_suelta(); ce_fuera(); ce_pasa(20); ce_arriba(TF_ENTER); ce_pasa(60);
    chk_cond(q, "abajo y [enter] solapados en el men" S_uu ": llegan los dos (Aprende)",
             ui_pantalla() == P_LECCIONES && ce.nev == 2 && ce.ev[0] == T_ABAJO && ce.ev[1] == T_ENTER);

    ce_escalera(&ce_c, 0);
    r3u_abre(&ce_c);
    TECLAS(T_SHIFT);
    chk_cond(q, "el editor con un aviso y shift armado", ve("shift") && ve("Abierto."));
    ce_abre(CE_W, CE_H, 1);
    ce.extra[1] = 0x200; ce_pasa(90); ce.extra[1] = 0; ce_pasa(60);         /* trig (× ya es de las cuentas) */
    ce.extra[1] = 0x100; ce_pasa(90); ce.extra[1] = 0; ce_pasa(60);         /* π */
    ce.extra[5] = 0x002; ce_pasa(90); ce.extra[5] = 0; ce_pasa(60);         /* var */
    chk_cond(q, "trig, pi y var: ni evento, ni cambio de aviso, ni de shift", ce.eventos == 0 && ve("shift") && ve("Abierto."));
    ce_pulsa(TF_LETRA('M'));
    chk_cond(q, "(y el shift armado sigue sirviendo)", !ve("shift"));

    /* tres teclas nuevas en la misma muestra: un fantasma de la matriz; dos
     * a la vez sí valen, en el orden de prioridad */
    cm_menu();
    ce_abre(CE_W, CE_H, 1);
    ce_abajo(TF_DIG(3)); ce_abajo(TF_DIG(1)); ce_abajo(TF_DIG(2)); ce_pasa(90);
    ce_arriba(TF_DIG(1)); ce_arriba(TF_DIG(2)); ce_arriba(TF_DIG(3)); ce_pasa(60);
    chk(q, "tres teclas nuevas en la misma muestra (un fantasma): ning" S_uu "n evento", (double)ce.eventos, 0);
    ce_abajo(TF_DIG(2)); ce_abajo(TF_DIG(1)); ce_pasa(90);
    ce_arriba(TF_DIG(1)); ce_arriba(TF_DIG(2)); ce_pasa(60);
    chk_cond(q, "dos a la vez: las dos, en el orden de prioridad (1 y 2)",
             ce.nev == 2 && ce.ev[0] == T_DIG(1) && ce.ev[1] == T_DIG(2));
}

/* --------------------------------------------------------------------------
 *  3. shift y ctrl
 * ----------------------------------------------------------------------- */
static void ce_modificadores(void)
{
    const char *q = "capa: shift y ctrl";
    int k0, k1, n, i, raf;

    cm_editor();
    ce_abre(CE_W, CE_H, 1);
    ce_pulsa(TF_SHIFT);
    chk_cond(q, "shift solo: se arma y se ve «shift»", ve("shift") && ce.nev == 1 && ce.ev[0] == T_SHIFT);
    ce_pulsa(TF_SHIFT);
    chk_cond(q, "dos toques lo anulan", !ve("shift"));

    cm_barrido();
    chk_cond(q, "el barrido", ui_pantalla() == P_BARRIDO);
    ce_abre(CE_W, CE_H, 1);
    k0 = ce_k_barr();
    ce_abajo(TF_SHIFT); ce_pasa(20);
    ce_clic_en(CE_X[CE_DER], CE_Y[CE_DER]);
    ce_arriba(TF_SHIFT); ce_pasa(30);
    k1 = ce_k_barr();
    chk_cond(q, "shift abajo y la flecha: un paso grande (28 puntos)", k1 - k0 == (BARR_N - 1) / 10 && !ve("shift"));

    ce_pulsa(TF_SHIFT);                                       /* armado */
    k0 = ce_k_barr();
    ce.nev = 0;
    ce_clic_mantiene(CE_X[CE_DER], CE_Y[CE_DER], 450);
    k1 = ce_k_barr();
    n = ce_cuenta(T_DER);
    chk_cond(q, "shift armado y la flecha mantenida: saltos de 28 repetidos",
             n >= 3 && k1 - k0 == 28 * n && ce_cuenta_marca(T_REPE) == n - 1);

    cm_ayuda();
    ce_abre(CE_W, CE_H, 1);
    ce_dedo(CE_X[CE_ABA], CE_Y[CE_ABA]); ce_pasa(20); ce_hunde();
    ce_pasa(500);
    ce_pulsa(TF_SHIFT);                                       /* en mitad de la ráfaga */
    ce_pasa(200);
    raf = ui_indice(UI_I_SCROLL);
    ce_suelta(); ce_fuera(); ce_pasa(60);
    for (i = 0; i < ce.nev && ce.ev[i] != T_SHIFT; i++) {}
    chk_cond(q, "shift pulsado durante una r" S_aa "faga no arma nada", !ve("shift") && i == ce.nev && raf > 5);
}

/* --------------------------------------------------------------------------
 *  4. La repetición exacta, en tiempo virtual
 * ----------------------------------------------------------------------- */
static void ce_repeticion(void)
{
    const char *q = "capa: repetici" S_oo "n";
    EntAjustes a;
    int esperado, s0, s1;

    cm_ayuda();
    ce_abre(CE_W, CE_H, 1);
    ce_clic_mantiene(CE_X[CE_ABA], CE_Y[CE_ABA], 1000);
    esperado = 1 + ce_reps(ce_t0(), ce_tl(), REP_CURSOR_RETARDO, REP_CURSOR_PERIODO, 0, 0, 3);
    s0 = ui_indice(UI_I_SCROLL);
    ce_lista("abajo 1 s en la Ayuda", ce.t_hunde);
    chk(q, "abajo 1 s en la Ayuda: las l" S_ii "neas de la f" S_oo "rmula del perfil", s0, esperado);
    chk(q, "(la f" S_oo "rmula: 1 + 16 repeticiones de hasta x3)", esperado, 18);
    ce_pasa(500);
    s1 = ui_indice(UI_I_SCROLL);
    chk_cond(q, "al soltar, nada m" S_aa "s (sin inercia)", s1 == s0);
    chk_cond(q, "al soltar llega T_SUELTA", ce_sueltas() == 1 && ce.ev[ce.nev - 1] == T_SUELTA);

    /* los ajustes: «no» y «lenta» (x1,5) */
    cm_ayuda();
    a.deslizar = 1; a.repetir = REPETIR_NO; a.clic_enter = 1;
    ent_pon_ajustes(&a);
    ce_abre(CE_W, CE_H, 1);
    ce_clic_mantiene(CE_X[CE_ABA], CE_Y[CE_ABA], 1000);
    chk_cond(q, "Repetici" S_oo "n: no  => una l" S_ii "nea", ui_indice(UI_I_SCROLL) == 1);
    cm_ayuda();
    a.repetir = REPETIR_LENTA;
    ent_pon_ajustes(&a);
    ce_abre(CE_W, CE_H, 1);
    ce_clic_mantiene(CE_X[CE_ABA], CE_Y[CE_ABA], 1000);
    esperado = 1 + ce_reps(ce_t0(), ce_tl(), REP_CURSOR_RETARDO * REP_LENTA / 10, REP_CURSOR_PERIODO * REP_LENTA / 10, 0, 0, 3);
    chk_cond(q, "Repetici" S_oo "n: lenta  => los tiempos x1,5", ui_indice(UI_I_SCROLL) == esperado && esperado == 9);
    ent_ajustes_omision();

    /* LISTA: 400 ms y luego 100 ms (600 ms) y 50 ms, en Mis problemas */
    cm_mis();
    ce_abre(CE_W, CE_H, 1);
    ce_clic_mantiene(CE_X[CE_ABA], CE_Y[CE_ABA], 560);
    esperado = 1 + ce_reps(ce_t0(), ce_tl(), REP_LISTA_RETARDO, REP_LISTA_PERIODO, REP_LISTA_CAMBIO, REP_LISTA_PERIODO2, 1);
    chk_cond(q, "una lista: 400 ms y luego cada 100 ms", ui_indice(UI_I_SEL_MIS) == esperado && esperado == 3);
    ce_clic_mantiene(CE_X[CE_ABA], CE_Y[CE_ABA], 3000);
    chk_cond(q, "en el borde la r" S_aa "faga se apaga (y T_SUELTA)",
             ui_indice(UI_I_SEL_MIS) == 6 && ce_sueltas() == 2);
    limpia_mis();
}

/* --------------------------------------------------------------------------
 *  5. Una pantalla cara: el Bode a 42 ms por cuadro avanza lo mismo que una
 *     barata (el reloj manda, no los cuadros), y al soltar, la lectura exacta
 * ----------------------------------------------------------------------- */
static void ce_pantalla_cara(void)
{
    const char *q = "capa: pantalla cara";
    int avance[2], cuadros = 0, i, kf;
    double db, mod, fa;
    char t[64], u[24];
    Buf b;
    for (i = 0; i < 2; i++) {
        int k0;
        cm_bode();
        TECLAS(T_IZQ | T_CON_CTRL);                            /* al principio */
        ce_abre(CE_W, CE_H, 1);
        ce.ms_cuadro = i == 0 ? 42.0 : 5.0;
        k0 = ce_k_bode();
        ce_clic_mantiene(CE_X[CE_DER], CE_Y[CE_DER], 3000);
        avance[i] = ce_k_bode() - k0;
        if (i == 0) cuadros = (int)ce.cuadros;
    }
    chk_cond(q, "el Bode al principio", avance[1] > 100);
    chk_cond(q, "3 s a la derecha a 42 ms por cuadro: los puntos de uno barato (" S_PM "1 repetici" S_oo "n)",
             abs(avance[0] - avance[1]) <= 4);
    printf("    Bode, 3 s a la derecha: %d puntos a 42 ms por cuadro (%d cuadros) y %d a 5 ms\n", avance[0], cuadros, avance[1]);
    chk_cond(q, "los cuadros no pasan de 3000/42", cuadros <= 3000 / 42 + 3);
    chk_cond(q, "al soltar llega T_SUELTA", ce_sueltas() == 1);
    kf = ce_k_bode();
    traz_lee(bode_reg(), bode_reg()->f[kf], &db, &mod, &fa);
    buf_init(&b, t, sizeof t);
    buf_str(&b, "|H| "); buf_str(&b, num_txt(u, sizeof u, db, 2)); buf_str(&b, "dB");
    chk_cond(q, "y el texto, sin «" S_APROX "», es la lectura exacta", !ve(S_APROX) && ve(t) && ui_indice(UI_I_RAFAGA) == 0);
}

/* --------------------------------------------------------------------------
 *  6. La lista negra: lo que nunca se repite
 * ----------------------------------------------------------------------- */
static void ce_lista_negra(void)
{
    const char *q = "capa: lista negra";
    int i;

    cm_menu();
    ce_abre(CE_W, CE_H, 1);
    ce_mantiene(TF_ESC, 1500); ce_pasa(100);
    chk_cond(q, "esc mantenido en el men" S_uu ": solo la portada", ui_pantalla() == P_INICIO && !ce.salio && ce_cuenta(T_ESC) == 1);

    cm_mis();
    ce_abre(CE_W, CE_H, 1);
    ce_mantiene(TF_DEL, 2000); ce_pasa(100);
    chk_cond(q, "[del] 2 s en Mis problemas: solo la confirmaci" S_oo "n; el archivo sigue",
             ui_pantalla() == P_MIS && arch_existe(0) && ce_cuenta(T_BORRAR) == 1 && ve("[del] otra vez para BORRARLO"));
    limpia_mis();

    cm_ficha();
    chk_cond(q, "la ficha de E1 (E = 10 V, dato)", ui_pantalla() == P_ELEMENTO && ui_campo_q() == Q_VAL);
    ce_abre(CE_W, CE_H, 1);
    ce_pulsa(TF_DIG(1)); ce_pulsa(TF_DIG(2)); ce_pulsa(TF_DIG(3));
    ce_mantiene(TF_DEL, 2000); ce_pasa(100);
    chk_cond(q, "«123» y [del] 2 s: se borran las tres cifras y la magnitud sigue siendo dato",
             ce_cuenta(T_BORRAR) == 3 && ui_circuito()->el[0].val.est == M_DATO && ui_circuito()->el[0].val.v == 10.0);
    ce.nev = 0;
    ce_clic_mantiene(CE_X[CE_DER], CE_Y[CE_DER], 1500);
    chk(q, "en la ficha, derecha mantenida 1,5 s: la variante cambia una sola vez", ce_cuenta(T_DER), 1);

    ui_init();
    TECLAS(T_ENTER, T_ENTER, T_ENTER);                        /* Taller > Problema nuevo */
    ce_abre(CE_W, CE_H, 1);
    ce_pulsa(TF_LETRA('E'));
    ce_clic_mantiene(CE_X[CE_DER], CE_Y[CE_DER], 1500);
    chk_cond(q, "E y la flecha mantenida 1,5 s: la pieza puesta, la ficha abierta, la variante y el campo sin tocar",
             ui_pantalla() == P_ELEMENTO && ui_circuito()->nel == 1 && ui_circuito()->el[0].variante == ONDA_CC &&
             ui_indice(UI_I_CAMPO) == 0 && ce_cuenta(T_DER) == 1);

    cm_ca();
    TECLAS(T_MENU);                                           /* los ajustes de la CA: 3 campos con vuelta */
    ce_abre(CE_W, CE_H, 1);
    ce_clic_mantiene(CE_X[CE_ABA], CE_Y[CE_ABA], 1500);
    chk_cond(q, "los ajustes de la CA", ui_pantalla() == P_AJUSTES);
    chk(q, "abajo mantenido en unos campos con vuelta: uno solo", ui_indice(UI_I_AJ_CAMPO), 1);

    cm_menu();
    ce_abre(CE_W, CE_H, 1);
    ce_mantiene(TF_ENTER, 1500); ce_pasa(100);
    chk_cond(q, "[enter] mantenido: un solo efecto", ui_pantalla() == P_TALLER && ce_cuenta(T_ENTER) == 1);
    cm_resultado();
    ce_abre(CE_W, CE_H, 1);
    ce_mantiene(TF_TAB, 1500); ce_pasa(100);
    chk_cond(q, "[tab] mantenido: un solo efecto", ui_pantalla() == P_TABLA && ce_cuenta(T_TAB) == 1);
    cm_editor();
    ce_abre(CE_W, CE_H, 1);
    ce_mantiene(TF_IGUAL, 1500); ce_pasa(100);
    chk_cond(q, "[=] mantenido: un solo efecto", ui_pantalla() == P_RESULTADO && ce_cuenta(T_IGUAL) == 1);
    cm_editor();
    ce_abre(CE_W, CE_H, 1);
    for (i = 0; i < 3; i++) { ce_mantiene(i == 0 ? TF_LETRA('R') : i == 1 ? TF_DIG(7) : TF_MENU, 1200); ce_pasa(60); }
    chk_cond(q, "letras, cifras y [menu] mantenidos: uno cada una",
             ce_cuenta(T_LETRA('R')) == 1 && ce_cuenta(T_DIG(7)) == 1 && ce_cuenta(T_MENU) == 1 && ce_cuenta_marca(T_REPE) == 0);
}

/* --------------------------------------------------------------------------
 *  7. El cable con [W]: una ráfaga, un nivel de deshacer; y el salto
 * ----------------------------------------------------------------------- */
static int ce_cables(const Circuito *c)
{
    int x, y, n = 0;
    for (y = 0; y < REJ_FILAS; y++) for (x = 0; x < REJ_COLS - 1; x++) n += c->h[y][x] == SEG_CABLE;
    for (y = 0; y < REJ_FILAS - 1; y++) for (x = 0; x < REJ_COLS; x++) n += c->v[y][x] == SEG_CABLE;
    return n;
}

static void ce_editor(void)
{
    const char *q = "capa: el cable";
    int h0, k;
    ui_init();
    TECLAS(T_ENTER, T_ENTER, T_ENTER);                        /* Problema nuevo: el cursor en (1,1) */
    h0 = ui_niveles(0);
    ce_abre(CE_W, CE_H, 1);
    ce_pulsa(TF_LETRA('W'));
    ce_clic_mantiene(CE_X[CE_DER], CE_Y[CE_DER], 850);
    chk_cond(q, "[W] y la flecha mantenida 850 ms: 6 tramos y un nivel de deshacer",
             ce_cables(ui_circuito()) == 6 && ui_niveles(0) == h0 + 1 && ui_indice(UI_I_CUR_X) == 7);
    TECLAS(T_CTRL, T_LETRA('Z'));
    chk_cond(q, "[ctrl][Z] lo quita entero", ce_cables(ui_circuito()) == 0 && ui_niveles(0) == h0);

    /* un toque suelto (sin T_SUELTA) y luego un gesto: cada uno, su nivel */
    ui_init();
    TECLAS(T_ENTER, T_ENTER, T_ENTER);                        /* otra vez en (1,1) */
    h0 = ui_niveles(0);
    ce_abre(CE_W, CE_H, 1);
    ce_pulsa(TF_LETRA('W'));
    ce_clic_en(CE_X[CE_DER], CE_Y[CE_DER]);
    ce_dedo(300, 500); ce_pasa(30);
    ce_desliza(300, 500, 620, 500, 300, 0);                   /* un 32 %: 3 pasos de W/10 */
    ce_fuera(); ce_pasa(60);
    chk(q, "[W], un toque y un gesto del dedo: 1 + 3 tramos", ce_cables(ui_circuito()), 4);
    chk(q, "(dos niveles de deshacer: el toque y el gesto)", ui_niveles(0), h0 + 2);
    TECLAS(T_CTRL, T_LETRA('Z'));
    chk(q, "[ctrl][Z] quita el gesto entero", ce_cables(ui_circuito()), 1);

    /* un borne en (5,1): R1 de (5,1) a (5,2) */
    circ_init(&ce_c, "Borne");
    k = circ_elemento(&ce_c, EL_RES, 5, 1, DIR_ABA); u4_dato(&ce_c.el[k].val, 100.0);
    r3u_abre(&ce_c);
    TECLAS(T_DER, T_ABAJO);                                   /* el cursor a (1,1) */
    h0 = ui_niveles(0);
    TECLAS(T_LETRA('W'), T_DER | T_CON_SHIFT);
    chk_cond(q, "shift y la flecha con [W]: el cable hasta el siguiente borne, un nivel",
             ce_cables(ui_circuito()) == 4 && ui_indice(UI_I_CUR_X) == 5 && ui_indice(UI_I_CUR_Y) == 1 &&
             ui_niveles(0) == h0 + 1);
    TECLAS(T_DER | T_CON_CTRL);
    chk_cond(q, "ctrl y la flecha con [W]: hasta el borde, sin sustituir nada (se para antes de R1? no: R1 va abajo)",
             ce_cables(ui_circuito()) == 8 && ui_indice(UI_I_CUR_X) == 9 && ui_circuito()->nel == 1);
}

/* --------------------------------------------------------------------------
 *  8. El touchpad
 * ----------------------------------------------------------------------- */
static void ce_touchpad_tam(int w, int h)
{
    char q[48];
    int k0, lento, rapido, n;
    sprintf(q, "capa: touchpad %dx%d", w, h);

    cm_barrido();
    ce_abre(w, h, 1);
    ce_desliza(500, 500, 500, 500, 5000, 10);                 /* quieto, con ruido del 1 % */
    ce_fuera(); ce_pasa(60);
    chk_cond(q, "dedo quieto con ruido del 1 % durante 5 s: 0 eventos y 0 cuadros", ce.eventos == 0 && ce.cuadros <= 1);

    k0 = ce_k_barr();
    ce_dedo(350, 500); ce_pasa(30);
    ce_desliza(350, 500, 650, 500, 1500, 0);                  /* 30 % despacio */
    ce_pasa(30); ce_fuera(); ce_pasa(60);
    lento = ce_k_barr() - k0;
    chk(q, "deslizar un 30 % despacio en el barrido: 12 pasos de W/40", lento, 12);
    chk_cond(q, "al levantar el dedo, T_SUELTA y la lectura exacta", ce_sueltas() == 1 && !ve(S_APROX));
    k0 = ce_k_barr();
    ce_dedo(350, 500); ce_pasa(30);
    ce_desliza(350, 500, 650, 500, 60, 0);                    /* 30 % deprisa */
    ce_pasa(30); ce_fuera(); ce_pasa(60);
    rapido = ce_k_barr() - k0;
    chk_cond(q, "y deprisa, con balística, más pasos (x2 a x4)", rapido > lento && rapido <= 4 * lento + 8);

    cm_ayuda();
    ce_abre(w, h, 1);
    ce_dedo(500, 150); ce_pasa(30);
    ce_desliza(500, 150, 500, 120, 200, 0);                   /* deriva del 3 % */
    ce_hunde(); ce_pasa(90); ce_suelta(); ce_pasa(20); ce_fuera(); ce_pasa(60);
    chk_cond(q, "deriva del 3 % y clic en el borde: solo la flecha", ce.nev == 1 && ce.ev[0] == T_ABAJO && ui_indice(UI_I_SCROLL) == 1);
    ce.nev = 0;
    ce_clic_en(900, 850);                                     /* una esquina */
    chk_cond(q, "clic en la esquina (0,9; 0,85): derecha (el eje que manda)", ce.nev == 1 && ce.ev[0] == T_DER &&
             ui_indice(UI_I_SCROLL) == 1 + AYUDA_VIS_T);

    /* clic y luego mover sin levantar: nada hasta pasar otra vez del 8 %
     * desde donde se soltó el clic (el dedo ya se había movido un 5 % antes) */
    ce.nev = 0;
    ce_dedo(500, 100); ce_pasa(30);
    ce_desliza(500, 100, 500, 150, 100, 0);                   /* un 5 %: no arranca */
    ce_hunde(); ce_pasa(90); ce_suelta(); ce_pasa(200);
    ce_desliza(500, 150, 500, 210, 150, 0);                   /* 6 % */
    n = ce.nev;
    ce_desliza(500, 210, 500, 300, 150, 0);                   /* hasta el 15 % */
    ce_fuera(); ce_pasa(60);
    chk_cond(q, "clic y luego un 6 %: nada; hasta el 15 %: un paso", n == 1 && ce_cuenta_marca(T_TACTIL) == 1 &&
             ce_cuenta(T_ARRIBA) == 1);

    /* un parpadeo del contacto: un solo gesto */
    {
        EntCuentas c0, c1;
        ce_pasa(300);
        ent_cuentas(&c0);
        ce.nev = 0;
        ce_dedo(500, 800); ce_pasa(30);
        ce_desliza(500, 800, 500, 600, 300, 0);
        ce.contacto = 0; ce_pasa(10.25);                      /* una muestra sin contacto */
        ce_desliza(500, 600, 500, 400, 300, 0);
        ce_fuera(); ce_pasa(60);
        ent_cuentas(&c1);
        chk_cond(q, "un contacto que parpadea una muestra: un solo gesto (4 pasos de H/12)",
                 c1.gestos - c0.gestos == 1 && ce_cuenta(T_ABAJO) == 4 && ce_sueltas() == 1);
    }
    /* un salto del 50 %: se re-ancla sin pasos */
    ce.nev = 0;
    ce_dedo(500, 800); ce_pasa(40);
    ce_dedo(500, 300); ce_pasa(100);
    ce_fuera(); ce_pasa(60);
    chk_cond(q, "un salto del 50 % en una muestra: 0 pasos", ce.nev == 0);
}

static void ce_touchpad(void)
{
    const char *q = "capa: touchpad";
    EntAjustes a;
    int i, n;
    ce_touchpad_tam(CE_W, CE_H);
    ce_touchpad_tam(1000, 700);                               /* los umbrales son relativos */

    cm_menu();
    ce_abre(CE_W, CE_H, 1);
    ce_clic_en(500, 500);
    chk_cond(q, "clic en el centro: [enter]", ui_pantalla() == P_TALLER && ce.nev == 1 && ce.ev[0] == T_ENTER);
    ce_pulsa(TF_ESC);
    ent_ajustes(&a); a.clic_enter = 0; ent_pon_ajustes(&a);
    ce_clic_en(500, 500);
    chk_cond(q, "con «Clic central: nada», nada", ui_pantalla() == P_MENU && ce.nev == 2);
    ent_ajustes_omision();

    cm_ficha();
    ce_abre(CE_W, CE_H, 1);
    ce_dedo(200, 500); ce_pasa(30);
    ce_desliza(200, 500, 800, 500, 500, 0);                   /* 60 % en horizontal */
    ce_fuera(); ce_pasa(60);
    chk_cond(q, "deslizar en horizontal en la ficha: 0 pasos (la variante no se toca)", ce.nev == 0 &&
             ui_circuito()->el[0].variante == ONDA_CC);
    ce_dedo(500, 800); ce_pasa(30);
    ce_desliza(500, 800, 500, 450, 400, 0);                   /* 35 % hacia abajo: 2 campos */
    ce_fuera(); ce_pasa(60);
    chk(q, "y en vertical, de campo en campo (H/6): 2 pasos", ce_cuenta(T_ABAJO), 2);
    chk(q, "(el campo 2)", ui_indice(UI_I_CAMPO), 2);

    /* «pasar página»: la capa del resultado, un paso por contacto y solo si
     * el dedo va deprisa (un 25 % en 350 ms) */
    cm_resultado();
    n = ui_capa();
    ce_abre(CE_W, CE_H, 1);
    ce_dedo(200, 500); ce_pasa(30);
    ce_desliza(200, 500, 800, 500, 250, 0);                   /* 60 % en 250 ms */
    ce_fuera(); ce_pasa(60);
    chk(q, "pasar p" S_aa "gina: un deslizamiento r" S_aa "pido del 60 %, una capa", ce_cuenta(T_DER), 1);
    chk(q, "(la capa siguiente)", ui_capa(), (n + 1) % CAPA_N);
    ce.nev = 0;
    ce_dedo(200, 500); ce_pasa(30);
    ce_desliza(200, 500, 800, 500, 2000, 0);                  /* 60 % en 2 s: despacio */
    ce_fuera(); ce_pasa(60);
    chk(q, "y uno lento, ninguna", ce.nev, 0);

    /* en diagonal en el editor: al arrancar manda el eje que recorrió el
     * doble, y se queda bloqueado todo el contacto */
    ui_init();
    TECLAS(T_ENTER, T_ENTER, T_ENTER);                        /* Problema nuevo: (1,1) */
    ce_abre(CE_W, CE_H, 1);
    ce_dedo(200, 300); ce_pasa(30);
    ce_desliza(200, 300, 650, 480, 400, 0);                   /* 45 % a la derecha y 18 % arriba */
    ce_fuera(); ce_pasa(60);
    chk(q, "en diagonal en el editor: 4 pasos a la derecha (W/10)", ce_cuenta(T_DER), 4);
    chk(q, "y ninguno arriba (el eje se bloquea al arrancar)", ce_cuenta(T_ARRIBA), 0);

    cm_menu();
    ce_abre(CE_W, CE_H, 1);
    ce_dedo(500, 800); ce_pasa(30);
    ce_desliza(500, 800, 500, 660, 150, 0);                   /* 14 %: un paso abajo... */
    ce_pasa(100);                                             /* ...que espera TP_BUSCA_MS (el dedo, quieto) */
    n = ce.nev;
    ce_pulsa(TF_ENTER);                                       /* otra pantalla a mitad del gesto */
    ce_desliza(500, 660, 500, 200, 400, 0);
    ce_fuera(); ce_pasa(60);
    for (i = 0; i < ce.nev && ce.ev[i] != T_ENTER; i++) {}
    chk_cond(q, "la pantalla cambia a mitad del gesto: gesto anulado", n == 1 && ui_pantalla() == P_LECCIONES &&
             ce_cuenta_marca(T_TACTIL) == 1 && ui_indice(UI_I_SEL_LEC) == 0);

    cm_menu();                                                /* (ui_init pone los de omisión) */
    ent_ajustes(&a); a.deslizar = 0; ent_pon_ajustes(&a);
    ce_abre(CE_W, CE_H, 1);
    ce_dedo(500, 800); ce_pasa(30);
    ce_desliza(500, 800, 500, 200, 300, 0);
    ce_fuera(); ce_pasa(60);
    chk(q, "con «Deslizar: no», nada", ce.nev, 0);
    ent_ajustes_omision();

    /* la flecha se fija al hundir; con el pad hundido solo cambia si el dedo
     * pasa a otra cuña con margen (de bajar a mover sin soltar) */
    cm_ayuda();
    ce_abre(CE_W, CE_H, 1);
    ce_dedo(500, 150); ce_pasa(20); ce_hunde(); ce_pasa(100);
    ce_dedo(800, 210); ce_pasa(100);                          /* |u| 0,60 y |v| 0,58: sin margen, abajo */
    n = ce.nev;
    ce_dedo(850, 500); ce_pasa(100);                          /* la de la derecha */
    ce_suelta(); ce_pasa(20); ce_fuera(); ce_pasa(60);
    chk_cond(q, "hundido: moverse dentro de la cu" S_nn "a no cambia la flecha; pasar a otra, s" S_ii,
             n == 1 && ce.ev[0] == T_ABAJO && ce_cuenta(T_DER) == 1 && ce_cuenta(T_ABAJO) == 1);

    /* scans que fallan: 3 se aguantan; a la 4.ª, el pad suelto (sin flecha pegada) */
    cm_ayuda();
    ce_abre(CE_W, CE_H, 1);
    ce_dedo(CE_X[CE_ABA], CE_Y[CE_ABA]); ce_pasa(20); ce_hunde(); ce_pasa(500);
    ce.fallan = 1; ce_pasa(1000);
    n = ui_indice(UI_I_SCROLL);
    ce_pasa(1000);
    chk_cond(q, "el pad deja de responder: se da por suelto y la r" S_aa "faga se corta",
             ui_indice(UI_I_SCROLL) == n && ce_sueltas() == 1);
    ce.fallan = 0; ce_suelta(); ce_fuera(); ce_pasa(60);
}

/* --------------------------------------------------------------------------
 *  9. La energía: el pad según la actividad, y ni un cuadro en reposo
 * ----------------------------------------------------------------------- */
static void ce_energia(void)
{
    const char *q = "capa: energ" S_ii "a";
    long s0, c0;
    double r1, r2, r3, r4, r5;
    char det[128];

    cm_menu();
    ce_abre(CE_W, CE_H, 1);
    ce_pulsa(TF_DIG(1));                                      /* lo último: una tecla */
    ce_pasa(40);
    c0 = ce.cuadros;
    s0 = ce.scans; ce_pasa(800); r1 = (double)(ce.scans - s0) / 0.8;
    ce_pasa(4100);
    s0 = ce.scans; ce_pasa(20000); r2 = (double)(ce.scans - s0) / 20.0;
    ce_pasa(10000);
    s0 = ce.scans; ce_pasa(25000); r3 = (double)(ce.scans - s0) / 25.0;
    sprintf(det, "reposo: %.1f, %.1f y %.1f lecturas del pad por segundo (~98, ~49, ~24)", r1, r2, r3);
    printf("    %s\n", det);
    chk_cond(q, det, r1 > 92.0 && r1 < 100.0 && r2 > 46.0 && r2 < 51.0 && r3 > 23.0 && r3 < 25.5);
    chk_cond(q, "60 s de reposo: 0 cuadros", ce.cuadros == c0);

    ce_dedo(500, 500);                                        /* el dedo apoyado y quieto */
    ce_pasa(500);
    s0 = ce.scans; ce_pasa(4000); r4 = (double)(ce.scans - s0) / 4.0;
    ce_pasa(1500);
    s0 = ce.scans; ce_pasa(3000); r5 = (double)(ce.scans - s0) / 3.0;
    ce_fuera(); ce_pasa(60);
    sprintf(det, "dedo apoyado: %.1f por segundo; quieto pasados 5 s: %.1f", r4, r5);
    printf("    %s\n", det);
    chk_cond(q, det, r4 > 92.0 && r5 > 46.0 && r5 < 51.0);
    chk_cond(q, "y el dedo quieto no pinta", ce.cuadros == c0);

    cm_ayuda();
    ce_abre(CE_W, CE_H, 1);
    c0 = ce.cuadros;
    ce_clic_mantiene(CE_X[CE_ABA], CE_Y[CE_ABA], 2000);
    sprintf(det, "una r" S_aa "faga de 2 s: %ld cuadros (<= 2000/40 + 2)", ce.cuadros - c0);
    printf("    %s\n", det);
    chk_cond(q, det, ce.cuadros - c0 <= 2000 / REP_CURSOR_PERIODO + 2);
}

/* --------------------------------------------------------------------------
 *  10. La cola, los cálculos largos y el troceo del trabajo
 * ----------------------------------------------------------------------- */
static SimRes ce_sres;
static SimReg ce_sreg;
static BodeRes ce_bres;
static BodeReg ce_breg;
static BarrRes ce_rres;
static BarrReg ce_rreg;

/* El mismo trabajo con dos troceos: 1 si da lo mismo (memcmp). r0: la red
 * ya hecha (los circuitos del banco no están en la rejilla), o 0 */
static int ce_troceo_sim(const Circuito *c0, const Red *r0)
{
    static Circuito c;
    static Red r;
    const SimReg *g;
    int k, ok;
    c = *c0;
    if (r0) r = *r0; else red_extraer(&c, &r);
    if (sim_iniciar(&c, &r) != SIM_OK) return 0;
    while (sim_avanzar(SIM_OPS_TICK)) {}
    ce_sres = *sim_res(); ce_sreg = *sim_reg();
    c = *c0;
    if (r0) r = *r0; else red_extraer(&c, &r);
    sim_iniciar(&c, &r);
    while (sim_avanzar(BU_OPS_TICK)) {}
    g = sim_reg();
    ok = memcmp(&ce_sres, sim_res(), sizeof ce_sres) == 0 && ce_sreg.n == g->n && g->n > 10 &&
         memcmp(ce_sreg.t, g->t, (size_t)g->n * sizeof g->t[0]) == 0;
    for (k = 0; k < 2 && ok; k++)
        ok = memcmp(ce_sreg.y[k], g->y[k], (size_t)g->n * sizeof g->t[0]) == 0 &&
             memcmp(ce_sreg.s1[k], g->s1[k], (size_t)g->n * sizeof g->t[0]) == 0 &&
             memcmp(ce_sreg.s2[k], g->s2[k], (size_t)g->n * sizeof g->t[0]) == 0;
    if (!ok && getenv("HAKARU_DEPURA")) {
        const unsigned char *a = (const unsigned char *)&ce_sres, *b = (const unsigned char *)sim_res();
        size_t i;
        for (i = 0; i < sizeof ce_sres && a[i] == b[i]; i++) {}
        printf("    troceo: SimRes difiere en el byte %lu de %lu (nw en %lu, pasos en %lu, fact. en %lu); "
               "n %d y %d, t_fin %.9g y %.9g, pasos %ld y %ld, fact. %d y %d, iter %ld y %ld\n",
               (unsigned long)i, (unsigned long)sizeof ce_sres, (unsigned long)offsetof(SimRes, nw),
               (unsigned long)offsetof(SimRes, pasos), (unsigned long)offsetof(SimRes, factorizaciones),
               ce_sreg.n, g->n, ce_sres.t_fin, sim_res()->t_fin, ce_sres.pasos, sim_res()->pasos,
               ce_sres.factorizaciones, sim_res()->factorizaciones, (long)ce_sres.nw.iter, (long)sim_res()->nw.iter);
    }
    return ok;
}

static int ce_troceo_bode(const Circuito *c0)
{
    static Circuito c;
    static Red r;
    BodeAjustes aj;
    BodeRes b2;
    int ok;
    aj.ent = -1; aj.sal = BS_AUTO; aj.f_ini = aj.f_fin = 0.0;
    c = *c0; red_extraer(&c, &r);
    if (bode_iniciar(&c, &r, &aj) != SIM_OK) return 0;
    while (bode_avanzar(SIM_OPS_TICK)) {}
    ce_bres = *bode_res(); ce_breg = *bode_reg();
    c = *c0; red_extraer(&c, &r);
    bode_iniciar(&c, &r, &aj);
    while (bode_avanzar(BU_OPS_TICK)) {}
    b2 = *bode_res();
    b2.gen = ce_bres.gen;                                     /* cambia en cada barrido */
    ok = memcmp(&ce_bres, &b2, sizeof b2) == 0 && ce_breg.n == bode_reg()->n && ce_breg.n > 10 &&
         memcmp(ce_breg.f, bode_reg()->f, (size_t)ce_breg.n * sizeof ce_breg.f[0]) == 0 &&
         memcmp(ce_breg.H, bode_reg()->H, (size_t)ce_breg.n * sizeof ce_breg.H[0]) == 0;
    return ok;
}

static int ce_troceo_barr(const Circuito *c0)
{
    static Circuito c;
    static Red r;
    BarrAjustes aj;
    BarrRes b2;
    int ok, k;
    memset(&aj, 0, sizeof aj); aj.el = -1;
    c = *c0; red_extraer(&c, &r);
    if (barr_iniciar(&c, &r, &aj) != SIM_OK) return 0;
    while (barr_avanzar(SIM_OPS_TICK)) {}
    ce_rres = *barr_res(); ce_rreg = *barr_reg();
    c = *c0; red_extraer(&c, &r);
    barr_iniciar(&c, &r, &aj);
    while (barr_avanzar(BU_OPS_TICK)) {}
    b2 = *barr_res();
    b2.gen = ce_rres.gen;
    ok = memcmp(&ce_rres, &b2, sizeof b2) == 0 && ce_rreg.n == barr_reg()->n && ce_rreg.n > 10 &&
         memcmp(ce_rreg.x, barr_reg()->x, (size_t)ce_rreg.n * sizeof ce_rreg.x[0]) == 0;
    for (k = 0; k < ce_rreg.ntr && ok; k++)
        ok = memcmp(ce_rreg.y[k], barr_reg()->y[k], (size_t)ce_rreg.n * sizeof ce_rreg.x[0]) == 0;
    return ok;
}

/* La calculadora de CA (cal_avanzar): lo mismo de un tirón y en trozos de
 * 2000 operaciones (un problema de la calculadora cabe entero en un tick
 * de BU_OPS_TICK: así el troceo sí se ejercita) */
static int ce_troceo_cal(const Circuito *c0)
{
    static Circuito c;
    static Red r;
    static CalRes a;
    int trozos = 0;
    c = *c0; red_extraer(&c, &r);
    if (cal_iniciar(&c, &r) != CAL_OK) return 0;
    while (cal_avanzar(SIM_OPS_TICK)) {}
    a = *cal_res();
    c = *c0; red_extraer(&c, &r);
    cal_iniciar(&c, &r);
    while (cal_avanzar(2000UL)) trozos++;
    if (getenv("HAKARU_DEPURA"))
        printf("    troceo de la calculadora: %lu y %lu operaciones (%d trozos), %s\n", (unsigned long)a.ops,
               (unsigned long)cal_res()->ops, trozos, memcmp(&a, cal_res(), sizeof a) == 0 ? "iguales" : "distintos");
    return memcmp(&a, cal_res(), sizeof a) == 0 && trozos >= 3;
}

static void ce_cola(void)
{
    const char *q = "capa: la cola";
    EntMuestra m;
    EntCuentas c;
    int i, n = 0, ultimo = T_NADA, e;

    /* 20 toques mientras nadie saca eventos (un cuadro de 81,6 ms): se
     * entregan 16 y el [esc] del final no se pierde */
    ui_init();
    memset(&m, 0, sizeof m);
    m.pad = ENT_PAD_OK;
    ent_init(CE_W, CE_H, &m, 1000);
    for (i = 0; i < 20; i++) {
        int tf = i < 19 ? TF_DIG(i % 10) : TF_ESC, f;
        unsigned bit;
        ent_tecla_fis(tf, &f, &bit);
        m.fila[f] = (unsigned short)(m.fila[f] | bit); ent_muestra(&m, 1000UL + 4UL * (unsigned long)i);
        m.fila[f] = (unsigned short)(m.fila[f] & ~bit); ent_muestra(&m, 1002UL + 4UL * (unsigned long)i);
    }
    while ((e = ent_saca(1100)) != T_NADA) { n++; ultimo = e; }
    ent_cuentas(&c);
    chk_cond(q, "20 toques en un cuadro de 81,6 ms: se entregan 16 y el [esc] del final no se pierde",
             n == ENT_COLA && ultimo == T_ESC && c.perdidos == 4);

    /* un toque de 60 ms en mitad del trabajo: con ticks de BU_OPS_TICK
     * (~23 ms) se ve; con los de SIM_OPS_TICK (~70 ms, con el banco sin el
     * USB: eran ~95) se pierde si empieza al principio de uno */
    ce_escalera(&ce_c, 1);
    ce_c.t_fin.v = 0.2; ce_c.t_fin.est = M_DATO;
    ce_c.paso.v = 1e-5; ce_c.paso.est = M_DATO;
    r3u_abre(&ce_c);
    ui_trabajo(BU_OPS_TICK);
    TECLAS(T_IGUAL);
    chk_cond(q, "una emulaci" S_oo "n larga", ui_pantalla() == P_EMULANDO);
    ce_abre(CE_W, CE_H, 0);
    ce.ms_tick = (double)BU_OPS_TICK * NS_POR_OP * 1e-6;
    ce_pasa(100);
    ce_programa(TF_ESC, 45.0, 60.0);
    ce_pasa(250);
    chk_cond(q, "[esc] de 60 ms en mitad de un tick de ~23 ms: la emulaci" S_oo "n se corta",
             ui_pantalla() != P_EMULANDO && sim_res()->fin == FIN_CORTADO);
    r3u_abre(&ce_c);
    ui_trabajo(SIM_OPS_TICK);
    TECLAS(T_IGUAL);
    ce_abre(CE_W, CE_H, 0);
    ce.ms_tick = (double)SIM_OPS_TICK * NS_POR_OP * 1e-6;
    ce_pasa(100);
    ce_programa(TF_ESC, 5.0, 60.0);
    ce_pasa(250);
    chk_cond(q, "(con ticks de ~70 ms, ese [esc] cae entero dentro de uno y se pierde)", ui_pantalla() == P_EMULANDO);
    ui_tecla(T_ESC);
    ui_trabajo(SIM_OPS_TICK);

    /* el resultado no depende del troceo: 150 000 o 50 000 operaciones */
    ce_escalera(&ce_c, 1);
    chk_cond(q, "emular con ticks de 150 000 y de 50 000 operaciones: lo mismo (la escalera RC)", ce_troceo_sim(&ce_c, 0));
    {
        static Circuito pu;
        static Red rp;
        banco_puente(&pu, &rp);
        pu.t_fin.v = 2.0 / 60.0; pu.t_fin.est = M_DATO;
        chk_cond(q, "(el puente con 1N4007)", ce_troceo_sim(&pu, &rp));
    }
    ce_rlc(&ce_c);
    chk_cond(q, "el Bode: lo mismo con los dos troceos", ce_troceo_bode(&ce_c));
    u4_serie(&ce_c);
    chk_cond(q, "el barrido: lo mismo con los dos troceos", ce_troceo_barr(&ce_c));
    {
        int k[3];
        tk_vr(&ce_c, 120.0, 60.0, EL_BOB, 0.0, 30.0, 72.0, k);   /* el voltímetro marca 72 V: ¿L? */
        chk_cond(q, "la calculadora de CA: lo mismo con los dos troceos", ce_troceo_cal(&ce_c));
    }
}

/* --------------------------------------------------------------------------
 *  Los saltos: shift = paso grande, ctrl = al extremo
 * ----------------------------------------------------------------------- */
static void ce_saltos(void)
{
    const char *q = "capa: saltos";
    const Osc *o;
    const Trazador *z;
    const Curva *cv;
    int i, max, n;

    cm_ayuda();
    TECLAS(T_ABAJO | T_CON_CTRL);
    max = ui_indice(UI_I_SCROLL);
    chk_cond(q, "Ayuda: ctrl y abajo, al final", max > 300 && ve("la misma licencia."));
    TECLAS(T_ARRIBA | T_CON_SHIFT);
    chk_cond(q, "shift y arriba: una p" S_aa "gina (15)", ui_indice(UI_I_SCROLL) == max - AYUDA_VIS_T);
    TECLAS(T_ARRIBA | T_CON_CTRL, T_DER);
    chk_cond(q, "ctrl y arriba, al principio; derecha, una p" S_aa "gina", ui_indice(UI_I_SCROLL) == AYUDA_VIS_T);
    TECLAS(T_IZQ, T_IZQ);
    chk_cond(q, "izquierda: atrás, sin pasar de 0", ui_indice(UI_I_SCROLL) == 0);

    cm_mis();
    TECLAS(T_ABAJO | T_CON_CTRL);
    chk_cond(q, "Mis problemas: ctrl y abajo, el último", ui_indice(UI_I_SEL_MIS) == 6);
    TECLAS(T_ARRIBA | T_CON_SHIFT);
    chk_cond(q, "shift y arriba: una p" S_aa "gina (5)", ui_indice(UI_I_SEL_MIS) == 1);
    TECLAS(T_ARRIBA | T_CON_CTRL);
    chk_cond(q, "ctrl y arriba: el primero", ui_indice(UI_I_SEL_MIS) == 0);
    limpia_mis();

    cm_tabla();
    TECLAS(T_ABAJO | T_CON_CTRL);
    chk_cond(q, "tabla (20 filas): ctrl y abajo, la " S_uu "ltima p" S_aa "gina", ui_indice(UI_I_SCROLL) == 20 - TB_VIS_T);
    TECLAS(T_ABAJO);
    chk_cond(q, "abajo en el final: nada (la cota es de la tecla)", ui_indice(UI_I_SCROLL) == 20 - TB_VIS_T);
    TECLAS(T_ARRIBA | T_CON_SHIFT);
    chk_cond(q, "shift y arriba: una p" S_aa "gina", ui_indice(UI_I_SCROLL) == 0);

    cm_pasos();
    n = ui_npasos();
    TECLAS(T_DER | T_CON_CTRL);
    chk_cond(q, "pasos: ctrl y derecha, el " S_uu "ltimo", n > 2 && ui_paso() == n - 1);
    TECLAS(T_IZQ | T_CON_CTRL);
    chk_cond(q, "ctrl e izquierda, el primero", ui_paso() == 0);

    cm_teoria();
    TECLAS(T_DER | T_CON_CTRL);
    chk_cond(q, "teor" S_ii "a: ctrl y derecha, la " S_uu "ltima p" S_aa "gina", ui_pantalla() == P_TEORIA &&
             ui_indice(UI_I_PAG) == LECCIONES[ui_indice(UI_I_SEL_LEC)].npag - 1);

    ui_init();
    TECLAS(T_ENTER, T_ENTER, T_ENTER);                        /* Problema nuevo: (1,1) */
    TECLAS(T_DER | T_CON_CTRL);
    chk_cond(q, "editor: ctrl y derecha, al borde", ui_indice(UI_I_CUR_X) == REJ_COLS - 1 && ui_indice(UI_I_CUR_Y) == 1);
    TECLAS(T_ABAJO | T_CON_SHIFT);
    chk_cond(q, "shift y abajo sin nada por medio: al borde", ui_indice(UI_I_CUR_Y) == REJ_FILAS - 1);
    cm_editor();                                              /* la escalera: el cursor en (0,0) */
    TECLAS(T_DER | T_CON_SHIFT);
    chk_cond(q, "shift y derecha: hasta el siguiente borne", ui_indice(UI_I_CUR_X) == 1 && ui_indice(UI_I_CUR_Y) == 0);
    TECLAS(T_LETRA('R'), T_DER | T_CON_SHIFT);
    chk_cond(q, "con una pieza pendiente, shift no salta (la flecha de siempre)",
             ui_pantalla() == P_ELEMENTO && ui_indice(UI_I_CUR_X) == 2);

    cm_osc();
    o = ui_osc();
    TECLAS(T_DER | T_CON_CTRL);
    chk_cond(q, "osciloscopio: ctrl y derecha, la ventana al final del registro",
             fabs(o->tizq - (sim_res()->t_fin - 10.0 * ESCALA[o->itdiv] > 0.0 ? sim_res()->t_fin - 10.0 * ESCALA[o->itdiv] : 0.0)) < 1e-15);
    TECLAS(T_IZQ | T_CON_CTRL, T_LETRA('X'), T_DER | T_CON_CTRL);
    chk_cond(q, "con cursores, el cursor al borde de la ventana", o->tizq == 0.0 && o->cursores &&
             fabs(o->tc[o->cur] - (10.0 * ESCALA[o->itdiv] < sim_res()->t_fin ? 10.0 * ESCALA[o->itdiv] : sim_res()->t_fin)) < 1e-15);

    cm_bode();
    z = ui_trazador();
    TECLAS(T_MAS, T_MAS);
    TECLAS(T_DER | T_CON_CTRL);
    i = ce_k_bode();
    chk_cond(q, "Bode: ctrl y derecha, al borde de la ventana", i < bode_reg()->n - 1 &&
             bode_reg()->f[i] <= pow(10.0, z->lf1) * (1.0 + 1e-9));
    TECLAS(T_DER | T_CON_CTRL);
    chk_cond(q, "otra vez: al final del barrido", ce_k_bode() == bode_reg()->n - 1);

    cm_barrido();
    cv = ui_curva();
    TECLAS(T_MAS, T_MAS);
    TECLAS(T_IZQ | T_CON_CTRL);
    i = ce_k_barr();
    chk_cond(q, "barrido: ctrl e izquierda, al borde de la ventana", i > 0 && barr_reg()->x[i] >= cv->x0 - 1e-9);
    TECLAS(T_IZQ | T_CON_CTRL);
    chk_cond(q, "otra vez: al principio del barrido", ce_k_barr() == 0);

    /* la lectura «≈» del barrido mientras corre la ráfaga */
    ce_abre(CE_W, CE_H, 1);
    ce_dedo(CE_X[CE_DER], CE_Y[CE_DER]); ce_pasa(20); ce_hunde(); ce_pasa(600);
    chk_cond(q, "barrido: mientras corre el cursor, la lectura con «" S_APROX "»", ve(S_APROX) && ui_indice(UI_I_RAFAGA) == 1);
    ce_suelta(); ce_fuera(); ce_pasa(60);
    chk_cond(q, "al soltar, la exacta", !ve(S_APROX) && ui_indice(UI_I_RAFAGA) == 0);
}

/* --------------------------------------------------------------------------
 *  El panel «Controles» y su archivo
 * ----------------------------------------------------------------------- */
static void ce_controles(void)
{
    const char *q = "capa: Controles";
    unsigned char b[8];
    EntAjustes a;

    arch_bytes_borrar("/ces_controles.tns");
    cm_editor();
    TECLAS(T_MENU | T_CON_CTRL);
    chk_cond(q, "[ctrl][menu] en el editor: el panel", ui_pantalla() == P_CONTROLES && ve("Controles: teclas y touchpad"));
    vuelca("controles");
    TECLAS(T_ABAJO, T_DER, T_ABAJO, T_DER);
    chk_cond(q, "repetici" S_oo "n: r" S_aa "pida; clic central: nada", ve("r" S_aa "pida") && ve("nada"));
    TECLAS(T_ENTER);
    ent_ajustes(&a);
    chk_cond(q, "[enter] vuelve al editor con los ajustes puestos", ui_pantalla() == P_EDITOR &&
             a.repetir == REPETIR_RAPIDA && a.clic_enter == 0 && a.deslizar == 1);
    chk_cond(q, "y los guarda en ces_controles.tns (8 bytes)", arch_bytes_leer("/ces_controles.tns", b, 8) == 8 &&
             memcmp(b, "CESK", 4) == 0 && b[4] == 1 && b[5] == 1 && b[6] == REPETIR_RAPIDA && b[7] == 0);
    ui_init();
    ent_ajustes(&a);
    chk_cond(q, "ui_init vuelve a los de omisi" S_oo "n", a.repetir == REPETIR_OMISION && a.clic_enter == CLIC_ENTER_OMISION);
    ui_controles_abrir();
    ent_ajustes(&a);
    chk_cond(q, "ui_controles_abrir los recupera", a.repetir == REPETIR_RAPIDA && a.clic_enter == 0);
    b[6] = 9;
    arch_bytes_guardar("/ces_controles.tns", b, 8);
    ent_ajustes_omision();
    ui_controles_abrir();
    ent_ajustes(&a);
    chk_cond(q, "un valor que no vale: el de omisi" S_oo "n", a.repetir == REPETIR_OMISION && a.clic_enter == 0);
    ent_ajustes_omision();
    arch_bytes_borrar("/ces_controles.tns");

    ui_init();
    TECLAS(T_MENU | T_CON_CTRL);
    chk_cond(q, "tambi" S_ee "n desde la portada", ui_pantalla() == P_CONTROLES);
    TECLAS(T_ESC);
    chk_cond(q, "[esc] vuelve a donde se estaba", ui_pantalla() == P_INICIO);
    arch_bytes_borrar("/ces_controles.tns");
    cm_osc();
    TECLAS(T_IGUAL);
    if (ui_pantalla() == P_EMULANDO) {
        TECLAS(T_MENU | T_CON_CTRL);
        chk_cond(q, "emulando no se abre", ui_pantalla() == P_EMULANDO);
        r4_espera();
    }
    TECLAS(T_MENU);                                           /* sin ctrl: lo de siempre */
    chk_cond(q, "[menu] sin ctrl, lo de siempre", ui_pantalla() == P_AJUSTES);
}

/* --------------------------------------------------------------------------
 *  El grabador: 10 s de muestras crudas en ces_muestras.tns, que la capa
 *  reproduce y da los mismos eventos. HAKARU_GRABACION=<archivo> reproduce uno
 *  de la calculadora y enseña sus eventos (para calibrar)
 * ----------------------------------------------------------------------- */
static unsigned char ce_grab[GRAB_CABECERA + GRAB_MAX * GRAB_BYTES];
static EntMuestra ce_gm[GRAB_MAX];
static unsigned long ce_gt[GRAB_MAX];

/* Reproduce una grabación por la capa, con ui_tecla: los eventos en ev */
static int ce_reproduce(const unsigned char *b, int nb, int *ev, int max, int imprime)
{
    int w, h, n, k, ne = 0, e;
    if (!ent_grabacion_lee(b, nb, &w, &h, ce_gm, ce_gt, GRAB_MAX, &n) || n < 1) return -1;
    ent_init(w, h, &ce_gm[0], ce_gt[0]);
    for (k = 1; k < n; k++) {
        ent_muestra(&ce_gm[k], ce_gt[k]);
        ent_cuadro(ce_gt[k]);
        while ((e = ent_saca(ce_gt[k])) != T_NADA) {
            if (ne < max) ev[ne] = e;
            ne++;
            if (imprime) printf("    %6lu ms  evento 0x%04x  (pantalla %d)\n", ce_gt[k], (unsigned)e, ui_pantalla());
            ui_tecla(e);
        }
    }
    return ne;
}

static void ce_grabador(void)
{
    const char *q = "capa: grabador";
    static int ev[CE_NEV];
    int i, nb, n, g, ne, desde, iguales;
    const char *env = getenv("HAKARU_GRABACION");

    if (env) {                                                /* una de la calculadora */
        FILE *f = fopen(env, "rb");
        if (f) {
            nb = (int)fread(ce_grab, 1, sizeof ce_grab, f);
            fclose(f);
            ui_init();
            printf("    HAKARU_GRABACION=%s: %d eventos\n", env, ce_reproduce(ce_grab, nb, ev, CE_NEV, 1));
        }
    }
    arch_bytes_borrar("/ces_muestras.tns");
    ui_init();
    TECLAS(T_ENTER);
    TECLAS(T_MENU | T_CON_CTRL);
    ce_abre(CE_W, CE_H, 1);
    ce_pulsa(TF_LETRA('G'));
    desde = ce.nev;                                           /* lo que llegó tras la G */
    g = ent_grabacion(&n, 0);
    chk_cond(q, "[G] en Controles: graba", g == GRAB_GRABANDO && ui_animando());
    ce_pulsa(TF_ESC);                                         /* al menú */
    ce_clic_en(CE_X[CE_ABA], CE_Y[CE_ABA]);
    ce_clic_mantiene(CE_X[CE_ARR], CE_Y[CE_ARR], 700);       /* arriba: ya está en el borde */
    ce_dedo(500, 650); ce_pasa(30);
    ce_desliza(500, 650, 500, 300, 400, 3);                   /* abajo un 35 %: dos pasos de H/8 */
    ce_fuera(); ce_pasa(100);
    ce_pulsa(TF_ENTER);                                       /* la Ayuda */
    ce_pulsa(TF_SHIFT);
    ce_pulsa(TF_ESC);
    ce_pasa(10500);
    g = ent_grabacion(&n, 0);
    chk_cond(q, "a los 10 s se guarda ces_muestras.tns", g == GRAB_HECHO && n > 900 && n <= GRAB_MAX);
    nb = arch_bytes_leer("/ces_muestras.tns", ce_grab, (int)sizeof ce_grab);
    chk_cond(q, "con su cabecera «CESM» y 27 bytes por muestra", nb == GRAB_CABECERA + n * GRAB_BYTES && memcmp(ce_grab, "CESM", 4) == 0);
    /* la reproducción, desde el mismo estado: los mismos eventos */
    ui_init();
    TECLAS(T_ENTER);
    ui_tecla(T_MENU | T_CON_CTRL);
    ui_tecla(T_LETRA('G'));
    ent_grabacion_olvida();
    ne = ce_reproduce(ce_grab, nb, ev, CE_NEV, getenv("HAKARU_DEPURA") != 0);
    iguales = ne == ce.nev - desde && ne > 8;
    if (getenv("HAKARU_DEPURA")) {
        printf("    grabador: %d eventos reproducidos, %d en vivo:", ne, ce.nev - desde);
        for (i = desde; i < ce.nev; i++) printf(" %x", (unsigned)ce.ev[i]);
        printf("\n");
    }
    for (i = 0; i < ne && iguales; i++) iguales = ev[i] == ce.ev[desde + i];
    chk_cond(q, "reproducida por la capa, los mismos eventos (y la misma pantalla)", iguales && ui_pantalla() == P_MENU);
    arch_bytes_borrar("/ces_muestras.tns");
    arch_bytes_borrar("/ces_controles.tns");
    ent_grabacion_olvida();
}

/* --------------------------------------------------------------------------
 *  12. Lo que encontró la revisión de la capa: el dedo que busca la flecha,
 *      la deriva del pulgar, el reloj vigilado, los fantasmas con [esc], dos
 *      [esc] con la cola llena, el grabador y el tamaño del pad
 * ----------------------------------------------------------------------- */
/* El dedo se apoya en el centro, va a (cx, cy) en ms y hunde el pad */
static void ce_busca(int cx, int cy, double ms)
{
    ce_dedo(500, 500); ce_pasa(30);
    ce_desliza(500, 500, cx, cy, ms, 0);
    ce_hunde(); ce_pasa(90); ce_suelta(); ce_pasa(20); ce_fuera(); ce_pasa(60);
}

static void ce_buscar_flecha(void)
{
    const char *q = "capa: buscar la flecha";
    EntCuentas c0, c1;
    int x0, n, k0;

    /* quien apoya el dedo en el centro y lo lleva a la cuña antes de hundir:
     * el recorrido no son pasos, solo cuenta el clic */
    ui_init();
    TECLAS(T_ENTER, T_ENTER, T_ENTER);                        /* Problema nuevo: (1,1) */
    ce_abre(CE_W, CE_H, 1);
    x0 = ui_indice(UI_I_CUR_X);
    ent_cuentas(&c0);
    ce_busca(CE_X[CE_DER], CE_Y[CE_DER], 150);
    ent_cuentas(&c1);
    chk_cond(q, "editor: del centro a la cu" S_nn "a derecha en 150 ms y clic: un paso, el del clic",
             ui_indice(UI_I_CUR_X) - x0 == 1 && ce.nev == 1 && ce.ev[0] == T_DER && c1.buscados - c0.buscados == 3);
    cm_mis();
    ce_abre(CE_W, CE_H, 1);
    x0 = ui_indice(UI_I_SEL_MIS);
    ce_busca(CE_X[CE_ABA], CE_Y[CE_ABA], 150);
    chk_cond(q, "Mis problemas: del centro a la cu" S_nn "a de abajo y clic: una fila",
             ui_indice(UI_I_SEL_MIS) - x0 == 1 && ce.nev == 1 && ce.ev[0] == T_ABAJO);
    cm_ficha();
    ce_abre(CE_W, CE_H, 1);
    x0 = ui_indice(UI_I_CAMPO);
    ce_busca(CE_X[CE_ABA], CE_Y[CE_ABA], 150);
    chk(q, "la ficha: del centro a la cu" S_nn "a de abajo y clic: un campo", ui_indice(UI_I_CAMPO) - x0, 1);

    /* un deslizamiento de verdad: los pasos esperan TP_BUSCA_MS y llegan
     * todos; al levantar el dedo, en seguida */
    cm_mis();
    ce_abre(CE_W, CE_H, 1);
    ce_dedo(500, 800); ce_pasa(30);
    ce_desliza(500, 800, 500, 450, 150, 0);                   /* 35 % en 150 ms: dos pasos de H/8 */
    n = ce.nev;
    ce_desliza(500, 450, 500, 450, 150, 0);                   /* quieto: pasan */
    chk_cond(q, "deslizar un 35 % en Mis: a los 150 ms nada; luego las 2 filas",
             n == 0 && ce_cuenta_marca(T_TACTIL) == 2 && ui_indice(UI_I_SEL_MIS) == 2);
    ce_fuera(); ce_pasa(60);
    chk(q, "(y T_SUELTA al levantar)", ce_sueltas(), 1);
    ce.nev = 0;
    ce_dedo(500, 800); ce_pasa(30);
    ce_desliza(500, 800, 500, 450, 100, 0);
    ce_fuera(); ce_pasa(40);                                  /* levantar: ~140 ms desde que echó a andar */
    chk_cond(q, "si el dedo se levanta antes, los pasos pasan al levantarlo", ce_cuenta_marca(T_TACTIL) == 2 &&
             ui_indice(UI_I_SEL_MIS) == 4);
    limpia_mis();

    /* con balística (el Bode) no se espera: el cursor sigue al dedo */
    cm_bode();
    ce_abre(CE_W, CE_H, 1);
    k0 = ce_k_bode();
    ce_dedo(300, 500); ce_pasa(30);
    ce_desliza(300, 500, 600, 500, 100, 0);
    n = ce_k_bode() - k0;                                     /* con el dedo aún abajo */
    ce_fuera(); ce_pasa(60);
    chk_cond(q, "con balística (el Bode) no se espera: el cursor sigue al dedo desde el principio", n > 0);

    /* la ficha con un valor a medio escribir: el dedo no cambia de campo
     * (lo confirmaría), ni si se apoya antes de escribir ni después */
    cm_ficha();
    ce_abre(CE_W, CE_H, 1);
    x0 = ui_indice(UI_I_CAMPO);
    ce_dedo(500, 800); ce_pasa(30);                           /* apoyado antes */
    ce_pulsa(TF_DIG(4)); ce_pulsa(TF_DIG(7));
    ce_desliza(500, 800, 500, 300, 500, 0);                   /* 50 % hacia abajo */
    ce_fuera(); ce_pasa(60);
    chk_cond(q, "la ficha con «47» a medio escribir y el dedo apoyado antes: ni un paso, sin confirmar",
             ui_indice(UI_I_CAMPO) == x0 && ce_cuenta_marca(T_TACTIL) == 0 && ve("47_") &&
             ui_circuito()->el[0].val.v == 10.0);
    ce_dedo(500, 800); ce_pasa(30);                           /* apoyado después */
    ce_desliza(500, 800, 500, 300, 500, 0);
    ce_fuera(); ce_pasa(60);
    chk_cond(q, "... y apoyado despu" S_ee "s de escribir, tampoco", ui_indice(UI_I_CAMPO) == x0 &&
             ce_cuenta_marca(T_TACTIL) == 0 && ve("47_"));
    ce_pulsa(TF_ENTER);
    chk_cond(q, "(y [enter] lo guarda: 47 V)", ui_pantalla() == P_EDITOR && ui_circuito()->el[0].val.v == 47.0);
}

static void ce_deriva(void)
{
    const char *q = "capa: deriva del pulgar";
    EntCuentas c0, c1;
    int i, c;

    /* el pulgar apoyado resbala un 25 % en 30 s (menos del 1 %/s, con ruido
     * del 0,2 %) mientras se escribe «47» en la ficha */
    cm_ficha();
    ce_abre(CE_W, CE_H, 1);
    c = ui_indice(UI_I_CAMPO);
    ce_dedo(500, 600); ce_pasa(30);
    for (i = 0; i < 30; i++) {
        if (i == 5) ce_pulsa(TF_DIG(4));
        if (i == 10) ce_pulsa(TF_DIG(7));
        ce_desliza(500, 600 - i * 250 / 30, 500, 600 - (i + 1) * 250 / 30, 1000, 2);
    }
    ce_fuera(); ce_pasa(60);
    chk_cond(q, "la ficha: el pulgar baja un 25 % en 30 s mientras se escribe «47»: ni un campo",
             ui_indice(UI_I_CAMPO) == c && ce_cuenta_marca(T_TACTIL) == 0 && ve("47_"));

    /* lo mismo en una lista, sin teclear: el ancla sigue al dedo quieto */
    cm_mis();
    ce_abre(CE_W, CE_H, 1);
    ent_cuentas(&c0);
    ce_dedo(500, 700); ce_pasa(30);
    for (i = 0; i < 30; i++) ce_desliza(500, 700 - i * 300 / 30, 500, 700 - (i + 1) * 300 / 30, 1000, 2);
    ce_fuera(); ce_pasa(60);
    ent_cuentas(&c1);
    chk_cond(q, "Mis problemas: un 30 % en 30 s con el dedo apoyado: ni una fila (el ancla se muda)",
             ui_indice(UI_I_SEL_MIS) == 0 && ce.nev == 0 && c1.derivas - c0.derivas >= 5);

    /* una tecla de la matriz: el dedo apoyado vuelve a empezar desde ahí */
    ce.nev = 0;
    ce_dedo(500, 800); ce_pasa(30);
    ce_desliza(500, 800, 500, 600, 150, 0);                   /* 20 %: un paso (H/8)... */
    ce_pasa(100);                                             /* ...que pasa */
    ce_pulsa(TF_DIG(5));                                      /* en Mis no hace nada */
    ce_desliza(500, 600, 500, 500, 150, 0);                   /* un 10 % más */
    ce_pasa(250); ce_fuera(); ce_pasa(60);
    chk_cond(q, "un paso, una tecla y un 10 % m" S_aa "s: desde la tecla no llega a otro paso",
             ce_cuenta_marca(T_TACTIL) == 1 && ui_indice(UI_I_SEL_MIS) == 1 && ce_cuenta(T_DIG(5)) == 1);
    limpia_mis();
}

/* ---- El reloj vigilado ---- */
/* n + 1 lecturas del contador, una cada ms_tick, desde v0, a hz */
static void ce_lecturas(unsigned *v, int n, unsigned v0, double hz, double ms_tick)
{
    int i;
    for (i = 0; i <= n; i++) v[i] = v0 - (unsigned)(i * ms_tick * hz / 1000.0 + 0.5);
}

static void ce_reloj_cuentas(void)
{
    const char *q = "capa: reloj vigilado";
    unsigned v[RELOJ_CALIBRA + 1], c;
    EntReloj r;
    unsigned long m0, m1, ant;
    int i, mono = 1;
    double tick = ENT_TICK_US / 1000.0;

    ce_lecturas(v, RELOJ_CALIBRA, 0xF0000000u, ENT_SP804_HZ, tick);
    chk_cond(q, "al abrir: 8 ticks de idle() a la frecuencia de ENT_SP804_HZ, el plan A", ent_reloj_abre(&r, v, RELOJ_CALIBRA));
    ce_lecturas(v, RELOJ_CALIBRA, 0xF0000000u, 32000.0, tick);
    chk_cond(q, "(a 32 000 Hz, tambi" S_ee "n: dentro del 25 %)", ent_reloj_abre(&r, v, RELOJ_CALIBRA));
    ce_lecturas(v, RELOJ_CALIBRA, 0xF0000000u, 12e6, tick);
    chk_cond(q, "a 12 MHz (el reloj del bus): el plan B", !ent_reloj_abre(&r, v, RELOJ_CALIBRA) && !r.plan_a);
    ce_lecturas(v, RELOJ_CALIBRA, 0xF0000000u, ENT_SP804_HZ * 1.4, tick);
    chk_cond(q, "un 40 % m" S_aa "s r" S_aa "pido: el plan B", !ent_reloj_abre(&r, v, RELOJ_CALIBRA));
    ce_lecturas(v, RELOJ_CALIBRA, 0xF0000000u, 0.0, tick);
    chk_cond(q, "parado: el plan B", !ent_reloj_abre(&r, v, RELOJ_CALIBRA));
    ce_lecturas(v, RELOJ_CALIBRA, 0xF0000000u, ENT_SP804_HZ, tick);
    v[4] = v[3];                                              /* un tick sin avanzar */
    chk_cond(q, "un solo tick raro al abrir: el plan B", !ent_reloj_abre(&r, v, RELOJ_CALIBRA));

    /* en marcha: 1 s en lecturas de 7 ms, con un idle() cada 10,25 ms */
    ce_lecturas(v, RELOJ_CALIBRA, 0x00001000u, ENT_SP804_HZ, tick);    /* cerca del cero: da la vuelta */
    ent_reloj_abre(&r, v, RELOJ_CALIBRA);
    c = v[RELOJ_CALIBRA];
    m0 = m1 = ent_reloj_ms(&r, c);
    for (i = 1; i <= 1000; i++) {
        unsigned w = v[RELOJ_CALIBRA] - (unsigned)(i * ENT_SP804_HZ / 1000.0 + 0.5);
        m1 = ent_reloj_ms(&r, w);
        if (i % 10 == 0) ent_reloj_tick(&r, w);
        c = w;
    }
    chk_cond(q, "en marcha: 1 s de cuentas da 1000 ms (y el contador dio la vuelta por el cero)",
             labs((long)(m1 - m0) - 1000) <= 1 && r.plan_a && r.saltos == 0 && c > 0x80000000u);

    /* un salto (el contador reprogramado: vuelve arriba) cuenta un tick */
    m0 = ent_reloj_ms(&r, c);
    m1 = ent_reloj_ms(&r, 0xFFFFFFFFu);
    chk_cond(q, "el contador recargado (salto hacia arriba): un tick, no 36 horas",
             m1 - m0 <= (unsigned long)tick + 1 && r.saltos == 1 && r.plan_a);
    c = 0xFFFFFFFFu;
    m0 = ent_reloj_ms(&r, c);
    c -= (unsigned)(60.0 * ENT_SP804_HZ);
    m1 = ent_reloj_ms(&r, c);
    chk_cond(q, "un salto de 60 s de una lectura a otra: un tick", m1 - m0 <= (unsigned long)tick + 1 && r.saltos == 2);

    /* parado: a los dos idle(), el plan B, sin volver atrás ni saltar */
    ant = m1;
    for (i = 0; i < 20; i++) {
        unsigned long m;
        ent_reloj_tick(&r, c);                                /* el contador no se mueve */
        m = ent_reloj_ms(&r, c);
        if (m < ant) mono = 0;
        ant = m;
    }
    chk_cond(q, "el contador se para: a los 2 idle(), el plan B (una vez)", !r.plan_a && r.cambios == 1);
    chk_cond(q, "(el tiempo sigue: 17 ticks del plan B, unos 174 ms, y nunca hacia atr" S_aa "s)",
             mono && ant - m1 >= 170 && ant - m1 <= 178);

    /* más lento (el divisor cambiado a /16): también */
    ce_lecturas(v, RELOJ_CALIBRA, 0xF0000000u, ENT_SP804_HZ, tick);
    ent_reloj_abre(&r, v, RELOJ_CALIBRA);
    c = v[RELOJ_CALIBRA];
    for (i = 0; i < 3; i++) { c -= (unsigned)(ENT_SP804_HZ / 16.0 * tick / 1000.0); ent_reloj_tick(&r, c); }
    chk_cond(q, "a 1/16 de la frecuencia: el plan B", !r.plan_a);

    /* el plan B: cada idle() son ENT_TICK_US */
    ent_reloj_abre(&r, 0, 0);
    m0 = ent_reloj_ms(&r, 0);
    for (i = 0; i < 100; i++) ent_reloj_tick(&r, 0);
    chk_cond(q, "el plan B: 100 idle() son 1025 ms", !r.plan_a && ent_reloj_ms(&r, 0) - m0 == 1025);
}

/* La plataforma con el reloj de main.c: el contador sale del reloj virtual
 * (con un salto, o parado) y la cuenta vigilada lo convierte */
static struct { EntReloj r; unsigned base, parado_en; unsigned long salto; int parado; } ce_sp;

static unsigned ce_sp_valor(void)
{
    if (ce_sp.parado) return ce_sp.parado_en;
    return ce_sp.base - (unsigned)((unsigned long long)ce.t_us * ENT_SP804_HZ / 1000000ULL) - (unsigned)ce_sp.salto;
}
static unsigned long ce_sp_reloj(Plataforma *pf) { (void)pf; return ent_reloj_ms(&ce_sp.r, ce_sp_valor()); }
static void ce_sp_espera(Plataforma *pf) { ce_espera(pf); ent_reloj_tick(&ce_sp.r, ce_sp_valor()); }

static void ce_sp_abre(int sin_animar)
{
    unsigned v[RELOJ_CALIBRA + 1];
    int i;
    ce_nueva(CE_W, CE_H, sin_animar);
    memset(&ce_sp, 0, sizeof ce_sp);
    ce_sp.base = 0x7FFFFFFFu;
    ce_espera(&ce.pf);
    for (i = 0; i <= RELOJ_CALIBRA; i++) { if (i) ce_espera(&ce.pf); v[i] = ce_sp_valor(); }
    ent_reloj_abre(&ce_sp.r, v, RELOJ_CALIBRA);
    ce.pf.reloj = ce_sp_reloj;
    ce.pf.espera = ce_sp_espera;
    ce_empieza();
}

static void ce_reloj_bucle(void)
{
    const char *q = "capa: reloj vigilado";
    int i, n;

    /* [del] mantenido en la ficha y el contador salta 60 s: el texto sigue */
    cm_ficha();
    ce_sp_abre(1);
    chk_cond(q, "(la plataforma de main.c: el plan A)", ce_sp.r.plan_a);
    for (i = 0; i < 12; i++) ce_pulsa(TF_DIG(1 + i % 9));
    ce.nev = 0;
    ce_abajo(TF_DEL); ce_pasa(100);                           /* aún sin repetir (450 ms) */
    ce_sp.salto += (unsigned long)(60.0 * ENT_SP804_HZ);      /* otro reprograma el contador */
    ce_pasa(12);
    n = ce_cuenta(T_BORRAR);
    ce_arriba(TF_DEL); ce_pasa(60);
    chk_cond(q, "[del] mantenido y el contador salta 60 s: un borrado, no una r" S_aa "faga",
             n == 1 && ce_sp.r.saltos == 1 && ve("12345678912_"));

    ESP_CE_PLAN_B
    ce.pf.reloj = ce_reloj; ce.pf.espera = ce_espera;
}

/* ---- Los fantasmas, la cola, el grabador y el tamaño del pad ---- */
static void ce_fantasmas_cola(void)
{
    const char *q = "capa: fantasmas y cola";
    EntMuestra m;
    EntCuentas c0, c1;
    int i, e, n = 0, esc = 0, f;
    unsigned bit;

    /* [esc] con otras dos en la misma foto: las otras no, [esc] sí */
    cm_menu();
    ce_abre(CE_W, CE_H, 1);
    ce_abajo(TF_ESC); ce_abajo(TF_DIG(1)); ce_abajo(TF_DIG(2)); ce_pasa(90);
    ce_arriba(TF_ESC); ce_arriba(TF_DIG(1)); ce_arriba(TF_DIG(2)); ce_pasa(60);
    chk_cond(q, "[esc] con otras dos teclas nuevas en la misma foto: llega [esc] (y las otras no)",
             ce.nev == 1 && ce.ev[0] == T_ESC && ui_pantalla() == P_INICIO);

    /* el fantasma de rectángulo: A y B abajo (la fila 4) y baja G (fila 3,
     * la columna de A): la matriz enciende también H (fila 3, la de B) */
    cm_ayuda();
    ce_abre(CE_W, CE_H, 1);
    ent_cuentas(&c0);
    ce_abajo(TF_LETRA('A')); ce_pasa(40);
    ce_abajo(TF_LETRA('B')); ce_pasa(40);
    ce_abajo(TF_LETRA('G'));
    ent_tecla_fis(TF_LETRA('H'), &f, &bit);
    ce.extra[f] = (unsigned short)bit;                        /* la fantasma, en la misma foto */
    ce_pasa(60);
    ce_arriba(TF_LETRA('G')); ce.extra[f] = 0; ce_pasa(40);
    ce_abajo(TF_DIG(5)); ce_pasa(40);                         /* (5, 0x040): sin rectángulo */
    ce_arriba(TF_DIG(5)); ce_arriba(TF_LETRA('A')); ce_arriba(TF_LETRA('B')); ce_pasa(60);
    ent_cuentas(&c1);
    chk_cond(q, "A y B abajo y baja G con su fantasma H: ni G ni H (las dos se descartan)",
             ce_cuenta(T_LETRA('A')) == 1 && ce_cuenta(T_LETRA('B')) == 1 && ce_cuenta(T_LETRA('G')) == 0 &&
             ce_cuenta(T_LETRA('H')) == 0 && c1.fantasmas - c0.fantasmas == 2);
    chk_cond(q, "(una tercera tecla que no cierra rect" S_aa "ngulo s" S_ii " llega: el 5)", ce_cuenta(T_DIG(5)) == 1);
    ce.nev = 0;
    ce_abajo(TF_LETRA('A')); ce_pasa(40);
    ce_abajo(TF_LETRA('B')); ce_pasa(40);
    ce_abajo(TF_LETRA('G')); ce_pasa(60);                     /* una matriz sin fantasmas */
    ce_arriba(TF_LETRA('G')); ce_arriba(TF_LETRA('A')); ce_arriba(TF_LETRA('B')); ce_pasa(60);
    chk(q, "(sin la fantasma, G s" S_ii " llega)", ce_cuenta(T_LETRA('G')), 1);

    /* la cola llena y dos [esc]: los dos (el segundo no pisa al primero) */
    ui_init();
    memset(&m, 0, sizeof m);
    m.pad = ENT_PAD_OK;
    ent_init(CE_W, CE_H, &m, 1000);
    for (i = 0; i < 18; i++) {
        int tf = i < 16 ? TF_DIG(i % 10) : TF_ESC;
        ent_tecla_fis(tf, &f, &bit);
        m.fila[f] = (unsigned short)(m.fila[f] | bit); ent_muestra(&m, 1000UL + 4UL * (unsigned long)i);
        m.fila[f] = (unsigned short)(m.fila[f] & ~bit); ent_muestra(&m, 1002UL + 4UL * (unsigned long)i);
    }
    while ((e = ent_saca(1100)) != T_NADA) { n++; if (e == T_ESC) esc++; }
    ent_cuentas(&c1);
    chk_cond(q, "16 cifras y dos [esc] con la cola llena: llegan los dos [esc] y 14 cifras",
             esc == 2 && n == ENT_COLA && c1.perdidos == 2);
    ent_init(CE_W, CE_H, &m, 2000);
    n = 0;
    for (i = 0; i < ENT_COLA + 1; i++) {
        ent_tecla_fis(TF_ESC, &f, &bit);
        m.fila[f] = (unsigned short)bit; ent_muestra(&m, 2000UL + 4UL * (unsigned long)i);
        m.fila[f] = 0; ent_muestra(&m, 2002UL + 4UL * (unsigned long)i);
    }
    while ((e = ent_saca(2100)) != T_NADA) if (e == T_ESC) n++;
    chk(q, "(17 [esc] seguidos: 16, la cola entera)", n, ENT_COLA);
}

static void ce_grabador_lleno(void)
{
    const char *q = "capa: grabador";
    static int ev[CE_NEV];
    int n, ms, g, nb, w, h, nm, desde, ne, i, iguales, n0, f;
    unsigned bit;
    EntMuestra m;

    /* las fotos que no cuentan (sin pad, la misma matriz, nada que sacar)
     * no se graban; un flanco o una lectura del pad, sí */
    ui_init();
    memset(&m, 0, sizeof m);
    ent_init(CE_W, CE_H, &m, 5000);
    ent_grabar();
    for (i = 0; i < 1000; i++) ent_muestra(&m, 5000UL + (unsigned long)i);
    ent_grabacion(&n0, 0);
    ent_tecla_fis(TF_DIG(1), &f, &bit);
    m.fila[f] = (unsigned short)bit; ent_muestra(&m, 6001);
    m.pad = ENT_PAD_OK; ent_muestra(&m, 6002);
    ent_grabacion(&n, 0);
    chk_cond(q, "1000 fotos sin pad y con la misma matriz no se graban; un flanco y el pad, s" S_ii,
             n0 == 1 && n == 3);
    ent_grabacion_olvida();

    /* abajo mantenido en la Ayuda: el bucle hace 2 o 3 fotos por tick, pero
     * solo se graban las que cuentan y caben los 10 s */
    arch_bytes_borrar("/ces_muestras.tns");
    cm_ayuda();
    TECLAS(T_MENU | T_CON_CTRL);
    ce_abre(CE_W, CE_H, 1);
    ce_pulsa(TF_LETRA('G'));
    desde = ce.nev;
    ce_pulsa(TF_ESC);                                         /* a la Ayuda */
    ce_dedo(CE_X[CE_ABA], CE_Y[CE_ABA]); ce_pasa(20); ce_hunde();
    ce_pasa(9000);
    g = ent_grabacion(&n, &ms);
    chk_cond(q, "abajo mantenido 9 s en la Ayuda: sigue grabando, con sitio, y le queda ~1 s",
             g == GRAB_GRABANDO && n < GRAB_MAX && ms > 500 && ms < 1500);
    ce_suelta(); ce_fuera(); ce_pasa(1500);
    g = ent_grabacion(&n, &ms);
    nb = arch_bytes_leer("/ces_muestras.tns", ce_grab, (int)sizeof ce_grab);
    chk_cond(q, "a los 10 s se guarda, con muestras hasta el final",
             g == GRAB_HECHO && n < GRAB_MAX && ent_grabacion_lee(ce_grab, nb, &w, &h, ce_gm, ce_gt, GRAB_MAX, &nm) &&
             nm == n && ce_gt[nm - 1] >= GRAB_MS - 20);
    printf("    grabador: %d muestras en 10 s con una r" S_aa "faga (%d eventos)\n", n, ce.nev - desde);
    /* reproducida desde el mismo estado: los mismos eventos */
    cm_ayuda();
    ui_tecla(T_MENU | T_CON_CTRL);
    ui_tecla(T_LETRA('G'));
    ent_grabacion_olvida();
    ne = ce_reproduce(ce_grab, nb, ev, CE_NEV, 0);
    iguales = ne == ce.nev - desde && ne > 20;
    for (i = 0; i < ne && iguales; i++) iguales = ev[i] == ce.ev[desde + i];
    chk_cond(q, "reproducida (sin las fotos que no cuentan): los mismos eventos", iguales);
    arch_bytes_borrar("/ces_muestras.tns");
    arch_bytes_borrar("/ces_controles.tns");
    ent_grabacion_olvida();
}

static void ce_tam_pad(void)
{
    const char *q = "capa: tama" S_nn "o del pad";
    static const int W[3][2] = { { 1, 1 }, { 60000, 60000 }, { 0, 50 } };
    int i;
    char det[96];
    for (i = 0; i < 3; i++) {
        cm_ayuda();
        ce_nueva(CE_W, CE_H, 1);
        ce.pf.w = W[i][0]; ce.pf.h = W[i][1];                 /* lo que dice touchpad_getinfo */
        ce_empieza();
        ce_clic_en(CE_X[CE_ABA], CE_Y[CE_ABA]);
        ce_clic_en(CE_X[CE_DER], CE_Y[CE_DER]);
        sprintf(det, "touchpad_getinfo da %dx%d: el de omisi" S_oo "n (las flechas, donde siempre)", W[i][0], W[i][1]);
        chk_cond(q, det, ce.nev == 2 && ce.ev[0] == T_ABAJO && ce.ev[1] == T_DER);
    }
    ce.nev = 0;
    ce.contacto = 1; ce.x = 65535; ce.y = CE_H / 2;           /* una x fuera del pad (un fallo del chip) */
    ce_pasa(20); ce_hunde(); ce_pasa(90); ce_suelta(); ce_pasa(20); ce_fuera(); ce_pasa(60);
    chk_cond(q, "una x de 65535: el borde derecho", ce.nev == 1 && ce.ev[0] == T_DER);
    /* un pad pequeño pero creíble (el CapTIvate no publica el suyo) vale */
    ui_init();
    TECLAS(T_ENTER, T_ENTER, T_ENTER);                        /* Problema nuevo: (1,1) */
    ce_abre(256, 192, 1);
    for (i = 0; i < 4; i++) ce_clic_en(CE_X[i], CE_Y[i]);
    ce_dedo(200, 500); ce_pasa(30);
    ce_desliza(200, 500, 550, 500, 300, 0);                   /* 35 %: 3 pasos de W/10 */
    ce_fuera(); ce_pasa(60);
    chk_cond(q, "un pad de 256x192: sus medidas; las cuatro flechas y 3 pasos de W/10",
             ce.nev >= 7 && ce.ev[0] == T_ARRIBA && ce.ev[1] == T_ABAJO && ce.ev[2] == T_IZQ && ce.ev[3] == T_DER &&
             ce_cuenta_marca(T_TACTIL) == 3);
}

static void ce_revision(void)
{
    ce_buscar_flecha();
    ce_deriva();
    ce_reloj_cuentas();
    ce_reloj_bucle();
    ce_fantasmas_cola();
    ce_grabador_lleno();
    ce_tam_pad();
}

/* --------------------------------------------------------------------------
 *  La tabla física contra keys.h (sin el SDK: los tpad_row y tpad_col que
 *  se copiaron) y la lógica de la matriz
 * ----------------------------------------------------------------------- */
static void ce_tabla(void)
{
    const char *q = "capa: tabla f" S_ii "sica";
    int i, j, f, g, dup = 0, ok = 1;
    unsigned b, c;
    for (i = 0; i < TF_N; i++) {
        int mat = ent_tecla_fis(i, &f, &b);
        if ((i >= TF_ARRIBA && i <= TF_CLIC) == mat) ok = 0;            /* las del pad no son de la matriz */
        if (mat && (f < 0 || f > 7 || !b || (b & (b - 1)))) ok = 0;      /* una fila y un solo bit */
        for (j = 0; j < i && mat; j++) if (ent_tecla_fis(j, &g, &c) && g == f && c == b) dup++;
    }
    chk_cond(q, "cada tecla de la matriz en su fila y su bit, sin repetir", ok && dup == 0);
    ent_tecla_fis(TF_ESC, &f, &b);
    chk_cond(q, "esc: 0x1C/0x080 (fila 6)", f == 6 && b == 0x080);
    ent_tecla_fis(TF_SHIFT, &f, &b);
    chk_cond(q, "shift: 0x1E/0x100 (fila 7)", f == 7 && b == 0x100);
}

#if ESPECIAL
#include "test_entrada_esp_fn.inc"      /* la versión especial (no se publica) */
#endif

static void test_capa_entrada(void)
{
    printf("[71] La capa de entrada: flancos, repetici" S_oo "n, touchpad y energ" S_ii "a (tiempo virtual)\n");
#ifdef POR_LA_CAPA
    ce_directo = 1;                                           /* aquí la mano es de cada prueba */
#endif
    ce_tabla();
    ce_independencia();
    ce_flancos();
    ce_modificadores();
    ce_repeticion();
    ce_pantalla_cara();
    ce_lista_negra();
    ce_editor();
    ce_touchpad();
    ce_energia();
    ce_cola();
    ESP_CE_11
    ce_saltos();
    ce_controles();
    ce_grabador();
    ce_revision();
    ent_ajustes_omision();
    ce_listo = 0;                                             /* capa_tecla abre otra */
#ifdef POR_LA_CAPA
    ce_directo = 0;
#endif
}
