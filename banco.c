/* ============================================================================
 *  banco.c  -  Banco de tiempos  (ver banco.h)
 * ========================================================================= */
#include <string.h>
#include <math.h>
#include "banco.h"
#include "gfx.h"
#include "numfmt.h"
#include "algebra.h"
#include "resuelve.h"
#include "pasos.h"
#include "proced.h"
#include "ejemplos.h"
#if ESPECIAL
#include "especial.h"               /* la versión especial (no se publica) */
#else
/* La versión pública: los ganchos de la especial, vacíos */
#define ESP_BANCO_FNS
#define ESP_BANCO_FNS2
#define ESP_BANCO_REINICIA
#define ESP_BANCO_MIDE
#define ESP_BANCO_MIDE2
#define ESP_BANCO_NOMBRES
#define ESP_BANCO_NOMBRES2
#endif
#include "luc.h"                   /* LUC_MAX: el punto de CA de la 2.ª página */
#include "ui.h"                     /* la 2.ª página: cuadros por ui_dibuja */
#include "sim.h"
#include "ca.h"
#include "bode.h"
#include "turbo.h"                  /* a cuántos MHz se mide (el turbo, el USB) */

#define NFLOP 256                    /* multiplicaciones-resta por vuelta  */

static double ms_de[BN_N];
static int    siguiente = BN_N;
static unsigned long duracion = 4000, ops_peor, ops_ejemplo;
/* Por vuelta de mide() (lo que mide es lo que tarda una vuelta): los pasos,
 * las operaciones y las iteraciones del emulador y las operaciones de un
 * punto de CA. Una vuelta de BN_NL no siempre da un paso (Newton parte el
 * paso): repartirlo por paso sesgaba lo fijo de una iteración (2026-09-27:
 * 192,3 µs en vez de 250,5) */
static double ops_paso, pasos_sim, ops_nl, iter_nl, pasos_nl, ops_ca;
static long   vueltas;                /* las de la última mide()           */

static Circuito bc;
static Red      br;
static Solucion bs;
static Pasos    bp;
static double   fa[NFLOP], fb[NFLOP];
static volatile double sumidero;     /* que el compilador no se salte nada */
static int    (*salir)(void);        /* esc abajo (lo pone main.c)         */
static int    cancelado;
/* La frecuencia de la medida (turbo_mhz: el registro): -2 sin medir aún,
 * -1 si cambió a mitad (el USB, el turbo), 0 si no se sabe (no es una CX
 * II: se cuenta como 396) */
static int    mhz_de = -2;
static void   reloj_visto(void) { int f = turbo_mhz(); mhz_de = mhz_de == -2 || mhz_de == f ? f : -1; }

/* --------------------------------------------------------------------------
 *  El peor caso: una red densa de 20 nodos (semilla fija). Un árbol que los
 *  une a todos y cuerdas entre nodos distintos hasta llenar los 32
 *  elementos: muchos puentes, nada de resistencias en corto.
 * ----------------------------------------------------------------------- */
#define PEOR_NODOS 20
static unsigned sem;
static unsigned az(unsigned n) { sem = sem * 1103515245u + 12345u; return (sem >> 16) % n; }

static int pnodos;

static int nuevo(Circuito *c, Red *r, TipoEl t, int a, int b)
{
    Elemento *e;
    int k = c->nel;
    if (k >= MAX_EL) return -1;
    e = &c->el[k];
    memset(e, 0, sizeof *e);
    e->ref = -1;
    e->tipo = (unsigned char)t;
    e->xa = e->xb = (unsigned char)(k % REJ_COLS);
    e->ya = e->yb = (unsigned char)(k / REJ_COLS);
    if (t == EL_RES) { e->val.v = 0.5 * (double)(1 + az(200)); e->val.est = M_DATO; }
    e->etiq[0] = t == EL_RES ? 'R' : 'E';
    if (k + 1 >= 10) { e->etiq[1] = (char)('0' + (k + 1) / 10); e->etiq[2] = (char)('0' + (k + 1) % 10); }
    else e->etiq[1] = (char)('0' + k + 1);
    r->a[k] = a; r->b[k] = b;
    c->nel++;
    return k;
}

static void cierra(const Circuito *c, Red *r)
{
    r->nn = pnodos;
    r->nel = c->nel;
    r->error = RED_OK;
    r->elem_error = -1;
    red_cierra(c, r);
}

void banco_peor_caso(Circuito *c, Red *r)
{
    int f, i, n;
    sem = 99u;
    circ_init(c, "peor caso");
    memset(r, 0, sizeof *r);
    pnodos = PEOR_NODOS;
    f = nuevo(c, r, EL_FUENTE, 0, 1);
    c->el[f].val.v = 12.0; c->el[f].val.est = M_DATO;
    for (i = 1; i < PEOR_NODOS; i++) nuevo(c, r, EL_RES, i, (int)az((unsigned)i));   /* árbol */
    while (c->nel < MAX_EL) {                                                         /* cuerdas */
        int a = (int)az(PEOR_NODOS), b = (int)az(PEOR_NODOS);
        if (a != b) nuevo(c, r, EL_RES, a, b);
    }
    cierra(c, r);
    resolver(c, r, &bs);
    if (bs.estado != SOL_OK) return;
    for (i = 1, n = 0; i < c->nel && n < 8; i++)
        if (fabs(bs.s[0].I[i]) > 1e-6 && r->a[i] != r->b[i]) {
            c->el[i].I.est = M_DATO; c->el[i].I.v = fabs(bs.s[0].I[i]);
            c->el[i].val.est = M_BUSCAR;
            n++;
        }
}

/* El peor caso del emulador: un árbol de 20 nodos más cuerdas, con R, L y
 * C alternados, y una E cuadrada de 0 a 10 V a 10 kHz */
void banco_peor_sim(Circuito *c, Red *r)
{
    int i, k;
    sem = 7u;
    circ_init(c, "peor emulador");
    memset(r, 0, sizeof *r);
    pnodos = PEOR_NODOS;
    k = nuevo(c, r, EL_FUENTE, 0, 1);
    c->el[k].variante = ONDA_CUADRADA;
    c->el[k].val.v = 10.0; c->el[k].val.est = M_DATO;                  /* V2 */
    c->el[k].par[Q_BASE - Q_P1].v = 0.0; c->el[k].par[Q_BASE - Q_P1].est = M_DATO;
    c->el[k].par[Q_FREC - Q_P1].v = 10e3; c->el[k].par[Q_FREC - Q_P1].est = M_DATO;
    for (i = 1; c->nel < MAX_EL; i++) {
        int a = i < PEOR_NODOS ? i : (int)az(PEOR_NODOS), b = i < PEOR_NODOS ? (int)az((unsigned)i) : (int)az(PEOR_NODOS);
        TipoEl t = (TipoEl)(i % 3 == 0 ? EL_BOB : i % 3 == 1 ? EL_RES : EL_COND);
        if (a == b) continue;
        k = nuevo(c, r, t, a, b);
        if (t == EL_BOB) { c->el[k].val.v = 1e-4 * (double)(1 + az(100)); c->el[k].etiq[0] = 'L'; }
        else if (t == EL_COND) { c->el[k].val.v = 1e-8 * (double)(1 + az(100)); c->el[k].etiq[0] = 'C'; }
        else c->el[k].val.v = 10.0 * (double)(1 + az(100));
        c->el[k].val.est = M_DATO;
        if (t != EL_RES) { c->el[k].par[0].v = 0.0; c->el[k].par[0].est = M_DATO; }   /* V0, I0 = 0 */
    }
    cierra(c, r);
}

void banco_peor_ca(Circuito *c, Red *r)
{
    Elemento *e;
    banco_peor_sim(c, r);
    e = &c->el[0];
    e->variante = FORMA_FASOR;
    e->val.v = 10.0; e->val.est = M_DATO;
    e->val.ang = 0.0; e->val.est_ang = M_DATO;
    c->frec.v = 1e3; c->frec.est = M_DATO;
}

/* El puente con filtro: E de 10 V a 60 Hz entre 1 y 0; D de 1 a 2, de 0 a 2,
 * de 3 a 1 y de 3 a 0; C de 100 µF y R de 1 kΩ entre 2 y 3 (la salida) */
void banco_puente(Circuito *c, Red *r)
{
    static const signed char D[4][2] = { { 1, 2 }, { 0, 2 }, { 3, 1 }, { 3, 0 } };
    int k, i;
    sem = 5u;
    circ_init(c, "puente 1N4007");
    memset(r, 0, sizeof *r);
    pnodos = 4;
    k = nuevo(c, r, EL_FUENTE, 0, 1);
    c->el[k].variante = ONDA_SENO;
    c->el[k].val.v = 10.0; c->el[k].val.est = M_DATO;
    c->el[k].par[Q_BASE - Q_P1].v = 0.0; c->el[k].par[Q_BASE - Q_P1].est = M_DATO;
    c->el[k].par[Q_FREC - Q_P1].v = 60.0; c->el[k].par[Q_FREC - Q_P1].est = M_DATO;
    for (i = 0; i < 4; i++) {
        k = nuevo(c, r, EL_DIODO, D[i][0], D[i][1]);
        c->el[k].variante = DIO_1N4007; c->el[k].etiq[0] = 'D';
    }
    k = nuevo(c, r, EL_COND, 2, 3);
    c->el[k].val.v = 100e-6; c->el[k].val.est = M_DATO; c->el[k].etiq[0] = 'C';
    c->el[k].par[0].v = 0.0; c->el[k].par[0].est = M_DATO;           /* V0 = 0 */
    k = nuevo(c, r, EL_RES, 2, 3);
    c->el[k].val.v = 1e3;
    cierra(c, r);
}

void banco_rc(Circuito *c, Red *r)
{
    int k;
    sem = 11u;
    circ_init(c, "RC");
    memset(r, 0, sizeof *r);
    pnodos = 3;
    k = nuevo(c, r, EL_FUENTE, 0, 1);
    c->el[k].variante = ONDA_CUADRADA;
    c->el[k].val.v = 10.0; c->el[k].val.est = M_DATO;
    c->el[k].par[Q_BASE - Q_P1].v = 0.0; c->el[k].par[Q_BASE - Q_P1].est = M_DATO;
    c->el[k].par[Q_FREC - Q_P1].v = 10e3; c->el[k].par[Q_FREC - Q_P1].est = M_DATO;
    k = nuevo(c, r, EL_RES, 1, 2);
    c->el[k].val.v = 1e3;
    k = nuevo(c, r, EL_COND, 2, 0);
    c->el[k].val.v = 100e-9; c->el[k].val.est = M_DATO; c->el[k].etiq[0] = 'C';
    c->el[k].par[0].v = 0.0; c->el[k].par[0].est = M_DATO;           /* V0 = 0 */
    cierra(c, r);
}

void banco_rlc(Circuito *c, Red *r)
{
    int k;
    sem = 3u;
    circ_init(c, "RLC serie");
    memset(r, 0, sizeof *r);
    pnodos = 4;
    k = nuevo(c, r, EL_FUENTE, 0, 1);
    c->el[k].variante = FORMA_FASOR;
    c->el[k].val.v = 1.0; c->el[k].val.est = M_DATO;
    c->el[k].val.ang = 0.0; c->el[k].val.est_ang = M_DATO;
    c->frec.v = 1e3; c->frec.est = M_DATO;
    k = nuevo(c, r, EL_RES, 1, 2);
    c->el[k].val.v = 10.0;
    k = nuevo(c, r, EL_BOB, 2, 3);
    c->el[k].val.v = 10e-3; c->el[k].val.est = M_DATO; c->el[k].etiq[0] = 'L';
    k = nuevo(c, r, EL_COND, 3, 0);
    c->el[k].val.v = 1e-6; c->el[k].val.est = M_DATO; c->el[k].etiq[0] = 'C';
    cierra(c, r);
}

/* --------------------------------------------------------------------------
 *  Las pruebas
 * ----------------------------------------------------------------------- */
static void p_flop(void)
{
    static double f = 1e-6;
    int i;
    for (i = 0; i < NFLOP; i++) fa[i] -= f * fb[i];
    f = -f;                                     /* que no se vaya a ningún lado */
    sumidero = fa[NFLOP / 2];
}

static void p_ejemplos(void)
{
    int i;
    for (i = 0; i < N_EJEMPLOS; i++) {
        ejemplo_cargar(i, &bc);
        red_extraer(&bc, &br);
        resolver(&bc, &br, &bs);
        proced_generar(&bc, &br, &bs, 0, &bp, 0);
    }
}

static void p_peor(void)
{
    resolver(&bc, &br, &bs);
    proced_generar(&bc, &br, &bs, 0, &bp, 0);
}

/* Las cargas del emulador, fijas: la ventana y el paso que eligió la versión
 * medida el 2026-09-27 (con el paso automático la carga dependería de las
 * constantes que el banco calibra). El arnés las reproduce
 * (test_calibracion) */
#define CARGA_SIM_T 2e-3                     /* BN_SIM: 2 ms con h = 2 µs  */
#define CARGA_SIM_H 2e-6
#define CARGA_NL_T  1.0                      /* BN_NL: 1 s con h = T/45    */
#define CARGA_NL_H  ((1.0 / 60.0) / 45.0)
static void carga(void (*red)(Circuito *, Red *), double t_fin, double h)
{
    red(&bc, &br);
    bc.t_fin.v = t_fin; bc.t_fin.est = M_DATO;
    bc.paso.v = h; bc.paso.est = M_DATO;
    sim_iniciar(&bc, &br);
}

/* Un paso del emulador (sim_avanzar da siempre al menos uno); si acaba, se
 * vuelve a empezar */
static unsigned long sim_o0;
static long sim_p0, sim_base, sim_it0;
static void p_sim(void)
{
    if (!sim_avanzar(1)) {
        sim_base += sim_res()->pasos;
        carga(banco_peor_sim, CARGA_SIM_T, CARGA_SIM_H);
    }
}

/* Un punto del Bode (la matriz a una f, su LU y la salida) en el peor caso */
static double ca_f = 1e3;
static void p_ca(void)
{
    Cx H;
    bode_en(ca_f, &H);
    ca_f = ca_f < 1e5 ? ca_f * 1.37 : 10.0;
    sumidero = H.re;
}

/* El Bode entero del RLC, con autorrango, como lo pide el usuario */
static BodeAjustes baj;
static void p_bode(void)
{
    bode_iniciar(&bc, &br, &baj);
    while (bode_avanzar(~0ul))
        if (salir && salir()) { cancelado = 1; return; }
}

static void p_seno(void)
{
    static double x = 0.1;
    int i;
    double s = 0.0;
    for (i = 0; i < NFLOP; i++) { s += sin(x); x += 0.37; if (x > 100.0) x -= 100.0; }
    sumidero = s;
}

static void p_exp(void)
{
    static double x = -3.0;
    int i;
    double s = 0.0;
    for (i = 0; i < NFLOP; i++) { s += exp(x); x += 0.37; if (x > 30.0) x -= 60.0; }
    sumidero = s;
}

/* Un paso del puente (como p_sim): si acaba, se vuelve a empezar */
static void p_nl(void)
{
    if (!sim_avanzar(1)) {
        sim_base += sim_res()->pasos;
        sim_it0 -= sim_res()->nw.iter;
        carga(banco_puente, CARGA_NL_T, CARGA_NL_H);
    }
}

static void p_pantalla(void) { gfx_flush(); }
ESP_BANCO_FNS

/* --------------------------------------------------------------------------
 *  Las de la segunda página (banco.h). En la calculadora se mide siempre,
 *  tras la primera; en el PC, solo si se pide (las pruebas de siempre y el
 *  mono ven el banco de una página)
 * ----------------------------------------------------------------------- */
#ifdef NDLESS
#define BANCO_EXTRA 1
#else
#define BANCO_EXTRA 0
#endif
static int    con_segunda = BANCO_EXTRA;     /* medirla (banco_extra)             */
static int    pagina;                        /* la que se ve: 0 o 1               */
static double ms2[B2_N], ops2[B2_N], pasos2[B2_N];
static long   reinicios2[2];                 /* los de B2_PASO y los de B2_PEQUE  */
static double ventana2;                      /* t_fin/h de B2_PASO                */
static unsigned long cp15;                   /* el tipo de caché (lee_cp15)       */
static double fq[NFLOP];                     /* los cocientes y las raíces        */

static int total(void) { return BN_N + (con_segunda ? B2_N : 0); }

/* Una división, una comparación y una raíz por elemento, con sus cargas */
static void p_div(void)  { int i; for (i = 0; i < NFLOP; i++) fq[i] = fa[i] / fb[i]; sumidero = fq[NFLOP / 2]; }
static void p_cmp(void)  { int i, n = 0; for (i = 0; i < NFLOP; i++) n += fa[i] < fb[i]; sumidero = (double)n; }
static void p_raiz(void) { int i; for (i = 0; i < NFLOP; i++) fq[i] = sqrt(fabs(fa[i])); sumidero = fq[NFLOP / 2]; }

/* El núcleo sin memoria: N_REGS vueltas de subs y bne (4 ciclos por vuelta
 * en la tabla del ARM926); cargas que aciertan: 8 veces los 2 KB de fa (caben
 * en la caché), una palabra por vuelta (5 ciclos: la carga, 1); cargas que
 * fallan: una por línea de 32 B de la pantalla de trabajo (150 KB, no cabe).
 * En el PC, lo mismo en C: solo para que corra */
#define N_REGS    4096
#define N_ACIERTA (8 * NFLOP * 2)
#define N_FALLA   (SCR_W * SCR_H * 2 / 32)
static void p_regs(void)
{
#if defined(__arm__)
    unsigned n = N_REGS;
    __asm__ volatile("1:\n\tsubs %0, %0, #1\n\tbne 1b" : "+r"(n) : : "cc");
#else
    volatile unsigned n = N_REGS;
    while (n) n--;
#endif
}

static void p_acierta(void)
{
    int k;
    for (k = 0; k < 8; k++) {
#if defined(__arm__)
        const unsigned *p = (const unsigned *)(const void *)fa;
        unsigned n = NFLOP * 2, t;
        __asm__ volatile("1:\n\tldr %2, [%0], #4\n\tsubs %1, %1, #1\n\tbne 1b"
                         : "+r"(p), "+r"(n), "=&r"(t) : : "cc", "memory");
#else
        const volatile unsigned char *p = (const volatile unsigned char *)fa;
        unsigned n, t = 0;
        for (n = 0; n < NFLOP * 8; n += 4) t += p[n];
        sumidero = (double)t;
#endif
    }
}

static void p_falla(void)
{
#if defined(__arm__)
    const unsigned char *p = (const unsigned char *)gfx_buffer();
    unsigned n = N_FALLA, t;
    __asm__ volatile("1:\n\tldr %2, [%0], #32\n\tsubs %1, %1, #1\n\tbne 1b"
                     : "+r"(p), "+r"(n), "=&r"(t) : : "cc", "memory");
#else
    const volatile u16 *p = gfx_buffer();
    unsigned n, t = 0;
    for (n = 0; n < N_FALLA; n++) t += p[n * 16];
    sumidero = (double)t;
#endif
}

/* El tipo de caché de CP15 (MRC p15, 0, Rd, c0, c0, 1): los tamaños de las
 * de instrucciones y de datos. Solo en la calculadora (en qemu-arm, fuera
 * del modo privilegiado, sería una instrucción indefinida) */
static unsigned long lee_cp15(void)
{
#if defined(__arm__) && defined(NDLESS)
    unsigned long v;
    __asm__ volatile("mrc p15, 0, %0, c0, c0, 1" : "=r"(v));
    return v;
#else
    return 0;
#endif
}

/* Los KB de un campo de 12 bits del tipo de caché: 512 << size (x 1,5 con M) */
static long cache_kb(unsigned long campo)
{
    unsigned long s = (campo >> 6) & 7ul, m = (campo >> 2) & 1ul;
    return (long)(((512ul << s) * (m ? 3ul : 2ul) / 2ul) / 1024ul);
}

/* El emulador sin reinicios: una ventana de 16 ms con h = 2 µs, unos 16 600
 * pasos con los de los flancos (menos que SIM_MAX_PASOS): en los 4 s no se
 * acaba mientras un paso tarde más de 0,24 ms (hoy, ~1 ms). Si se acabara,
 * se vuelve a empezar y se cuenta (banco_reinicios2) */
#define CARGA_LARGA_T 16e-3
#define CARGA_RC_T    20e-3                  /* la red pequeña: 12 800 pasos */
static void p_paso2(void)
{
    if (!sim_avanzar(1)) {
        sim_base += sim_res()->pasos;
        reinicios2[0]++;
        carga(banco_peor_sim, CARGA_LARGA_T, CARGA_SIM_H);
    }
}

static void p_inicio(void) { carga(banco_peor_sim, CARGA_SIM_T, CARGA_SIM_H); }

/* La pequeña sí se acaba en 4 s (su paso es corto): cada reinicio es un
 * sim_iniciar de 3 elementos, que se cuenta para descontarlo si pesara */
static void p_peque(void)
{
    if (!sim_avanzar(1)) {
        sim_base += sim_res()->pasos;
        reinicios2[1]++;
        carga(banco_rc, CARGA_RC_T, CARGA_SIM_H);
    }
}

/* Un punto de CA como los de la malla del Bode (bode_avanzar pone la
 * topología una vez por trozo, no por punto): la matriz a f, su LU, las
 * fuentes y la salida, con la topología hecha una vez al empezar */
static SimRed ca_red;
static CaExc  ca_exc;
static void prepara_ca2(void)
{
    SimDiag d;
    memset(&d, 0, sizeof d);
    d.elem = d.nodo = d.elem_aviso = -1;
    banco_peor_ca(&bc, &br);
    sim_red_de(&bc, &br, &ca_red, &d);
    memset(&ca_exc, 0, sizeof ca_exc);
    ca_exc.U[0] = cx(1.0, 0.0);
    ca_exc.activa = 1u;
    ca_apaga(&ca_red, ca_exc.activa);
    ca_topologia(&ca_red, &d);
}

static void p_ca2(void)
{
    static Cx b[LUC_MAX], x[LUC_MAX];
    SimDiag d;
    memset(&d, 0, sizeof d);
    d.elem = d.nodo = d.elem_aviso = -1;
    ca_pon_q(0);
    if (ca_ensambla(&ca_red, ca_f, &d) == SIM_OK) {
        ca_b_fuentes(&ca_red, &ca_exc, b);
        ca_sustituye(b, x);
        sumidero = ca_phi(x, 1).re;
    }
    ca_f = ca_f < 1e5 ? ca_f * 1.37 : 10.0;
}

ESP_BANCO_FNS2

/* Repite f() durante `duracion` ms y devuelve lo que tarda una vuelta. El
 * reloj de la calculadora va a saltos de un segundo: se empieza justo en
 * un salto, y el error es de menos de una vuelta. */
static double mide(void (*f)(void))
{
    unsigned long t0 = gfx_reloj_ms(), t;
    long n = 0;
    while ((t = gfx_reloj_ms()) == t0)
        if (salir && salir()) { cancelado = 1; return -1.0; }
    t0 = t;
    do {
        f(); n++;
        if (salir && salir()) { cancelado = 1; return -1.0; }   /* [esc] a mitad */
    } while ((t = gfx_reloj_ms()) - t0 < duracion);
    vueltas = n;
    return (double)(t - t0) / (double)n;
}

void banco_duracion(unsigned long ms) { duracion = ms > 0 ? ms : 1; }
void banco_salida(int (*f)(void)) { salir = f; }
int  banco_cancelado(void) { return cancelado; }

void banco_reinicia(void)
{
    int i;
    for (i = 0; i < BN_N; i++) ms_de[i] = -1.0;
    for (i = 0; i < B2_N; i++) ms2[i] = ops2[i] = pasos2[i] = -1.0;       /* la segunda página */
    pagina = 0; reinicios2[0] = reinicios2[1] = 0; ventana2 = 0.0; cp15 = 0;
    for (i = 0; i < NFLOP; i++) { fa[i] = 1.0 + 0.37 * i; fb[i] = 2.0 - 0.11 * i; }
    siguiente = 0;
    cancelado = 0;
    mhz_de = -2;
    ESP_BANCO_REINICIA
}

/* Cancelado en la primera, siguiente se queda en BN_N: también está hecho */
int banco_hecho(void) { return cancelado || siguiente >= total(); }
double banco_ms(int i) { return (i >= 0 && i < BN_N) ? ms_de[i] : -1.0; }

void   banco_extra(int si) { con_segunda = si != 0; }
void   banco_pagina(int d) { if (con_segunda && banco_hecho() && d) pagina = !pagina; }
int    banco_en_pagina(void) { return banco_hecho() ? pagina : siguiente >= BN_N; }
double banco_ms2(int i) { return (i >= 0 && i < B2_N) ? ms2[i] : -1.0; }
double banco_ops2(int i) { return (i >= 0 && i < B2_N) ? ops2[i] : -1.0; }
double banco_pasos2(int i) { return (i >= 0 && i < B2_N) ? pasos2[i] : -1.0; }
long   banco_reinicios2(int i) { return i == B2_PASO ? reinicios2[0] : i == B2_PEQUE ? reinicios2[1] : 0; }
double banco_ventana2(void) { return ventana2; }
unsigned long banco_cp15(void) { return cp15; }

/* Mide la línea i de la segunda página (lo que hace banco_tick en ella) */
static void mide2(int i)
{
    unsigned long o0;
    long p0, pasos;
    pagina = 1;                                      /* se ve la que se mide */
    if (i == 0) cp15 = lee_cp15();
    reloj_visto();                                   /* los ciclos, a los MHz de la medida */
    switch (i) {
    case B2_DDIV:    ms2[i] = mide(p_div) / NFLOP; break;
    case B2_DCMP:    ms2[i] = mide(p_cmp) / NFLOP; break;
    case B2_SQRT:    ms2[i] = mide(p_raiz) / NFLOP; break;
    case B2_REGS:    ms2[i] = mide(p_regs) / N_REGS; break;
    case B2_ACIERTA: ms2[i] = mide(p_acierta) / N_ACIERTA; break;
    case B2_FALLA:   ms2[i] = mide(p_falla) / N_FALLA; break;
    case B2_PASO:
    case B2_PEQUE:                                   /* por paso las operaciones; por vuelta los pasos */
        if (i == B2_PASO) {
            carga(banco_peor_sim, CARGA_LARGA_T, CARGA_SIM_H);
            ventana2 = sim_res()->h > 0.0 ? sim_res()->t_fin / sim_res()->h : 0.0;
        } else carga(banco_rc, CARGA_RC_T, CARGA_SIM_H);
        sim_ops_reset();
        o0 = sim_ops(); p0 = sim_res()->pasos; sim_base = 0; reinicios2[i == B2_PEQUE] = 0;
        vueltas = 0;
        ms2[i] = mide(i == B2_PASO ? p_paso2 : p_peque);
        pasos = sim_base + sim_res()->pasos - p0;
        ops2[i] = pasos > 0 ? (double)(sim_ops() - o0) / (double)pasos : 0.0;
        pasos2[i] = vueltas > 0 ? (double)pasos / (double)vueltas : 0.0;
        sim_cortar();
        break;
    case B2_INICIO:                                  /* las operaciones que sí cuenta, por vuelta */
        sim_ops_reset();
        vueltas = 0;
        ms2[i] = mide(p_inicio);
        ops2[i] = vueltas > 0 ? (double)sim_ops() / (double)vueltas : 0.0;
        sim_cortar();
        break;
    case B2_CA:
        prepara_ca2();
        ca_f = 1e3;
        o0 = ca_ops();
        vueltas = 0;
        ms2[i] = mide(p_ca2);
        ops2[i] = vueltas > 0 ? (double)(ca_ops() - o0) / (double)vueltas : 0.0;
        break;
    ESP_BANCO_MIDE2
    }
    reloj_visto();
    siguiente = cancelado ? total() : siguiente + 1;
}

unsigned long banco_ops_peor(void) { return ops_peor; }
unsigned long banco_ops_ejemplo(void) { return ops_ejemplo; }
double banco_ops_paso(void) { return ops_paso; }
double banco_ops_nl(void) { return ops_nl; }
double banco_iter_nl(void) { return iter_nl; }
unsigned long banco_ops_ca(void) { return (unsigned long)(ops_ca + 0.5); }

void banco_tick(void)
{
    if (siguiente >= BN_N && !banco_hecho()) { mide2(siguiente - BN_N); return; }   /* la segunda página */
    if (siguiente >= BN_N) return;
    reloj_visto();
    switch (siguiente) {
    case BN_FLOP:     ms_de[BN_FLOP] = mide(p_flop); break;
    case BN_EJEMPLO:
        sis_ops_reset();
        p_ejemplos();
        ops_ejemplo = sis_ops() / (unsigned long)N_EJEMPLOS;
        ms_de[BN_EJEMPLO] = mide(p_ejemplos) / N_EJEMPLOS;
        break;
    case BN_PEOR:
        banco_peor_caso(&bc, &br);
        sis_ops_reset();
        resolver(&bc, &br, &bs);
        ops_peor = sis_ops();
        ms_de[BN_PEOR] = mide(p_peor);
        break;
    case BN_PANTALLA: ms_de[BN_PANTALLA] = mide(p_pantalla); break;
    ESP_BANCO_MIDE
    case BN_SIM: {
        long pasos;
        double ms;
        carga(banco_peor_sim, CARGA_SIM_T, CARGA_SIM_H);
        sim_ops_reset();
        sim_o0 = sim_ops(); sim_p0 = sim_res()->pasos; sim_base = 0;
        vueltas = 0;
        ms = mide(p_sim);
        pasos = sim_base + sim_res()->pasos - sim_p0;
        ops_paso = pasos > 0 ? (double)(sim_ops() - sim_o0) / (double)pasos : 0.0;   /* por paso */
        pasos_sim = vueltas > 0 ? (double)pasos / (double)vueltas : 0.0;              /* por vuelta */
        sim_cortar();
        ms_de[BN_SIM] = ms;
        break;
    }
    case BN_CA: {
        unsigned long o0;
        banco_peor_ca(&bc, &br);
        baj.ent = -1; baj.sal = BS_AUTO; baj.f_ini = 10.0; baj.f_fin = 1e5;
        bode_iniciar(&bc, &br, &baj);
        ca_f = 1e3;
        p_ca();                                       /* el de 1 kHz, fuera (como el 2026-09-27) */
        o0 = ca_ops();
        vueltas = 0;
        ms_de[BN_CA] = mide(p_ca);
        ops_ca = vueltas > 0 ? (double)(ca_ops() - o0) / (double)vueltas : 0.0;   /* la media de las medidas */
        bode_cortar();
        break;
    }
    case BN_BODE:
        banco_rlc(&bc, &br);
        baj.ent = -1; baj.sal = BS_AUTO; baj.f_ini = baj.f_fin = 0.0;
        ms_de[BN_BODE] = mide(p_bode);
        break;
    case BN_SENO:     ms_de[BN_SENO] = mide(p_seno) / NFLOP; break;
    case BN_EXP:      ms_de[BN_EXP] = mide(p_exp) / NFLOP; break;
    default: {                                            /* BN_NL */
        long pasos;
        double ms;
        carga(banco_puente, CARGA_NL_T, CARGA_NL_H);
        sim_ops_reset();
        sim_o0 = sim_ops(); sim_p0 = sim_res()->pasos; sim_base = 0; sim_it0 = sim_res()->nw.iter;
        vueltas = 0;
        ms = mide(p_nl);
        pasos = sim_base + sim_res()->pasos - sim_p0;
        if (vueltas > 0) {                            /* por vuelta, como ms */
            ops_nl = (double)(sim_ops() - sim_o0) / (double)vueltas;
            iter_nl = (double)(sim_res()->nw.iter - sim_it0) / (double)vueltas;
            pasos_nl = (double)pasos / (double)vueltas;
        } else ops_nl = iter_nl = pasos_nl = 0.0;
        sim_cortar();
        ms_de[BN_NL] = ms;
        break;
    }
    }
    reloj_visto();
    siguiente = cancelado ? BN_N : siguiente + 1;
}

/* --------------------------------------------------------------------------
 *  Dibujo
 * ----------------------------------------------------------------------- */
static const char *const NOMBRE[BN_N] = {
    "Coma flotante: multiplicar y restar",
    "Un ejemplo: resolver y escribir pasos",
    "Peor caso de CC (31 R, 8 datos)",
    "Volcar la pantalla",
    ESP_BANCO_NOMBRES
    "Un paso del emulador (peor caso)",
    "Un punto del Bode (peor caso de CA)",
    "El Bode de un RLC (autorrango)",
    "Una llamada a sin()",
    "Una llamada a exp()",
    "Un paso del puente con 1N4007",
};

/* Unos µs fijos con un decimal (en el PC salen de restar dos números casi
 * iguales: negativos o diminutos, que no dicen nada) */
static void micro(Buf *b, double us)
{
    buf_num(b, us > 0.0 ? us : 0.0, 1);
    buf_str(b, " " S_MU "s");
}

static void tiempo(Buf *b, double ms)
{
    if (ms < 0.0) { buf_str(b, "..."); return; }
    if (ms < 1.0) { buf_sig(b, ms * 1000.0); buf_str(b, " " S_MU "s"); }
    else if (ms < 1000.0) { buf_sig(b, ms); buf_str(b, " ms"); }
    else { buf_sig(b, ms / 1000.0); buf_str(b, " s"); }
}

/* --------------------------------------------------------------------------
 *  La segunda página
 * ----------------------------------------------------------------------- */
static const char *const NOMBRE2[B2_N] = {
    "Una divisi" S_oo "n de double (ddiv)",
    "Una comparaci" S_oo "n de double",
    "Una ra" S_ii "z cuadrada (sqrt)",
    "Una vuelta de registros (subs, bne)",
    "Una carga que acierta en la cach" S_ee,
    "Una carga que falla en la cach" S_ee,
    "Un paso del emulador, sin reiniciar",
    "Arrancar el emulador (peor caso)",
    "Un paso de una red peque" S_nn "a (RC)",
    "Un punto de CA, sin la topolog" S_ii "a",
    ESP_BANCO_NOMBRES2
};

/* Unos ns con un decimal */
static void nanos(Buf *b, double ms)
{
    if (ms < 0.0) { buf_str(b, "..."); return; }
    buf_num(b, ms * 1e6, 1);
    buf_str(b, " ns");
}

static void hexa(Buf *b, unsigned long v)
{
    int i;
    for (i = 28; i >= 0; i -= 4) buf_ch(b, "0123456789ABCDEF"[(v >> i) & 15ul]);
}

/* Un número con un decimal y lo que sigue */
static void uno(Buf *b, double v, const char *tras)
{
    buf_num(b, v, 1);
    buf_str(b, tras);
}

static void dibuja2(void)
{
    char t[64];
    Buf b;
    int i, y = 46, sig = banco_hecho() ? -1 : siguiente - BN_N;     /* la que se mide */
    double ns_op = ms_de[BN_FLOP] >= 0.0 ? ms_de[BN_FLOP] * 1e6 / NFLOP : -1.0;

    gfx_fill(0, 0, SCR_W, 16, C_BARRA);
    gfx_text(6, 2, "Banco de tiempos (2 de 2)", C_TEXTO, 1);
    gfx_text(6, 22, "Para recalibrar el modelo de tiempos.", C_SUAVE, 1);
    gfx_text(6, 35, cancelado ? "Cancelado: [enter] vuelve a empezar." :
                    banco_hecho() ? "Anota tambi" S_ee "n estos n" S_uu "meros."
                                  : "No toques nada: cada prueba dura unos segundos.",
             cancelado ? C_ERROR : banco_hecho() ? C_OK : C_CURSOR, 1);

    for (i = 0; i < B2_N; i++, y += 11) {
        u16 c = i == sig ? C_CURSOR : ms2[i] >= 0.0 ? C_TEXTO : C_TENUE;
        gfx_text(6, y, NOMBRE2[i], c, 1);
        buf_init(&b, t, sizeof t);
        if (i == sig) buf_str(&b, "midiendo");
        else if (i == B2_REGS || i == B2_ACIERTA || i == B2_FALLA) nanos(&b, ms2[i]);
        else tiempo(&b, ms2[i]);
        gfx_text(SCR_W - 6 - gfx_text_w(t, 1), y, t, c, 1);
    }

    /* Lo que sale de ellas, en cuatro líneas: el tipo de caché; los ciclos (a
     * los MHz de la medida: turbo_mhz, como el flop de la primera página; si
     * cambiaron a mitad, se dice) de una vuelta de registros (la k del modelo ARM: 4 en la tabla),
     * de una carga que acierta y de un fallo (P); lo fijo de un paso en la red
     * grande y en la pequeña (SIM_NS_PASO = a + b·n) y de un arranque; y lo que
     * valen una división, una comparación y una raíz en operaciones, y una
     * operación de CA sin la topología (CA_NS_OP) */
    y += 2;
    buf_init(&b, t, sizeof t);
    buf_str(&b, "Cach" S_ee "s (CP15): ");
    if (cp15) {
        buf_str(&b, "I "); buf_int(&b, cache_kb(cp15 & 0xFFFul));
        buf_str(&b, " KB, D "); buf_int(&b, cache_kb((cp15 >> 12) & 0xFFFul));
        buf_str(&b, " KB, 0x"); hexa(&b, cp15);
    } else buf_str(&b, "solo en la calculadora");
    gfx_text(6, y, t, C_SUAVE, 1);
    y += 11;
    if (ms2[B2_REGS] >= 0.0 && ms2[B2_ACIERTA] >= 0.0 && ms2[B2_FALLA] >= 0.0) {
        double g = (mhz_de > 0 ? mhz_de : 396) / 1000.0;          /* 396 / 1000.0 es 0,396 exacto */
        double r = ms2[B2_REGS] * 1e6 * g, a = ms2[B2_ACIERTA] * 1e6 * g, f = ms2[B2_FALLA] * 1e6 * g;
        buf_init(&b, t, sizeof t);
        if (mhz_de == -1) buf_str(&b, "Ciclos: el reloj cambi" S_oo " a mitad: rep" S_ii "telo");
        else {
            buf_str(&b, "Ciclos: vuelta "); uno(&b, r, ", carga ");
            uno(&b, a - r > 0.0 ? a - r : 0.0, ", fallo ");
            uno(&b, f - a > 0.0 ? f - a : 0.0, "");
        }
        gfx_text(6, y, t, C_SUAVE, 1);
        y += 11;
    }
    if (ns_op > 0.0 && ms2[B2_PASO] >= 0.0 && pasos2[B2_PASO] > 0.0 && ms2[B2_PEQUE] >= 0.0 && pasos2[B2_PEQUE] > 0.0) {
        /* como en la primera: lo de una vuelta menos sus operaciones, entre
         * sus pasos; el arranque, menos las operaciones que ya cuenta */
        buf_init(&b, t, sizeof t);
        buf_str(&b, "Fijo: 32 el. ");
        micro(&b, (ms2[B2_PASO] * 1e3 - ops2[B2_PASO] * pasos2[B2_PASO] * ns_op * 1e-3) / pasos2[B2_PASO]);
        buf_str(&b, ", 3 el. ");
        micro(&b, (ms2[B2_PEQUE] * 1e3 - ops2[B2_PEQUE] * pasos2[B2_PEQUE] * ns_op * 1e-3) / pasos2[B2_PEQUE]);
        if (ms2[B2_INICIO] >= 0.0) {
            double ms = ms2[B2_INICIO] - ops2[B2_INICIO] * ns_op * 1e-6;
            buf_str(&b, "; arr. ");
            uno(&b, ms > 0.0 ? ms : 0.0, " ms");
        }
        gfx_text(6, y, t, C_SUAVE, 1);
        y += 11;
    }
    if (ns_op > 0.0 && ms2[B2_DDIV] >= 0.0 && ms2[B2_DCMP] >= 0.0 && ms2[B2_SQRT] >= 0.0) {
        buf_init(&b, t, sizeof t);
        buf_str(&b, "Op.: div. "); uno(&b, ms2[B2_DDIV] * 1e6 / ns_op, ", comp. ");
        uno(&b, ms2[B2_DCMP] * 1e6 / ns_op, ", ra" S_ii "z ");
        uno(&b, ms2[B2_SQRT] * 1e6 / ns_op, "");
        if (ms2[B2_CA] >= 0.0 && ops2[B2_CA] > 0.0) {
            buf_str(&b, "; CA ");
            buf_num(&b, ms2[B2_CA] * 1e3 / ops2[B2_CA], 3);
            buf_str(&b, " " S_MU "s");
        }
        gfx_text(6, y, t, C_SUAVE, 1);
    }

    gfx_fill(0, 226, SCR_W, 14, C_PANEL);
    gfx_text(6, 227, banco_hecho() ? "[enter] repetir  [izq/der] p" S_aa "gina  [esc] volver"
                                   : "[esc] cancelar y volver", C_SUAVE, 1);
}

void banco_dibuja(void)
{
    char t[64];
    Buf b;
    int i, y = 46;

    if (banco_en_pagina() == 1) { dibuja2(); return; }
    gfx_fill(0, 0, SCR_W, 16, C_BARRA);
    gfx_text(6, 2, con_segunda ? "Banco de tiempos (1 de 2)" : "Banco de tiempos", C_TEXTO, 1);
    gfx_text(6, 22, "Lo que tarda cada cosa en esta calculadora.", C_SUAVE, 1);
    gfx_text(6, 35, cancelado ? "Cancelado: [enter] vuelve a empezar." :
                    banco_hecho() ? "Anota estos números: calibran el arnés."
                                  : "No toques nada: cada prueba dura unos segundos.",
             cancelado ? C_ERROR : banco_hecho() ? C_OK : C_CURSOR, 1);

    for (i = 0; i < BN_N; i++, y += 11) {
        u16 c = i == siguiente ? C_CURSOR : ms_de[i] >= 0.0 ? C_TEXTO : C_TENUE;
        gfx_text(6, y, NOMBRE[i], c, 1);
        buf_init(&b, t, sizeof t);
        if (i == BN_FLOP && ms_de[i] >= 0.0) tiempo(&b, ms_de[i] / NFLOP);
        else if (i == siguiente) buf_str(&b, "midiendo");
        else tiempo(&b, ms_de[i]);
        gfx_text(SCR_W - 6 - gfx_text_w(t, 1), y, t, c, 1);
    }

    /* Lo que calibra el arnés, en tres líneas: los ciclos de una operación y
     * lo que vale una exp() en operaciones (NL_OPS_EXP); lo fijo de un paso
     * (SIM_NS_PASO) y de una iteración de Newton (NS_ITER); el Gauss y lo que
     * cuesta una operación de CA (CA_NS_OP). Todo por vuelta, como ms_de */
    y += 2;
    if (ms_de[BN_FLOP] >= 0.0) {
        double ns_op = ms_de[BN_FLOP] * 1e6 / NFLOP;
        int mhz = mhz_de > 0 ? mhz_de : 396;           /* 396.0 / 1000.0 es 0,396 exacto */
        buf_init(&b, t, sizeof t);
        buf_str(&b, "Flop: ");
        if (mhz_de == -1) buf_str(&b, "el reloj cambi" S_oo " a mitad (" S_IQ "el USB?): rep" S_ii "telo");
        else {
            buf_int(&b, (long)(ns_op * (mhz / 1000.0) + 0.5));
            buf_str(&b, " ciclos a "); buf_int(&b, mhz); buf_str(&b, " MHz");
        }
        if (ms_de[BN_EXP] >= 0.0 && mhz_de != -1) {
            buf_str(&b, "; exp(): ");
            buf_int(&b, (long)(ms_de[BN_EXP] * 1e6 / ns_op + 0.5));
            buf_str(&b, " operaciones");
        }
        gfx_text(6, y, t, C_SUAVE, 1);
        y += 11;
    }
    if (ms_de[BN_SIM] >= 0.0 && ms_de[BN_FLOP] >= 0.0 && ops_paso > 0.0 && pasos_sim > 0.0) {
        /* lo fijo de un paso: lo de una vuelta menos sus operaciones, entre
         * sus pasos; lo de una iteración: la vuelta del puente menos sus
         * operaciones y lo fijo de sus pasos, entre sus iteraciones */
        double ns_op = ms_de[BN_FLOP] * 1e6 / NFLOP;
        double fijo = (ms_de[BN_SIM] * 1e3 - ops_paso * pasos_sim * ns_op * 1e-3) / pasos_sim;
        buf_init(&b, t, sizeof t);
        buf_str(&b, "Paso: ");
        buf_int(&b, (long)(ops_paso + 0.5));
        buf_str(&b, " op. y ");
        micro(&b, fijo);
        if (ms_de[BN_NL] >= 0.0 && iter_nl > 0.0) {
            buf_str(&b, "; iteraci" S_oo "n: ");
            micro(&b, (ms_de[BN_NL] * 1e3 - ops_nl * ns_op * 1e-3 - pasos_nl * fijo) / iter_nl);
        }
        gfx_text(6, y, t, C_SUAVE, 1);
        y += 11;
    }
    if (ms_de[BN_PEOR] >= 0.0 && ops_peor > 0) {
        buf_init(&b, t, sizeof t);
        buf_str(&b, "Gauss: ");
        buf_int(&b, (long)ops_peor);
        buf_str(&b, " op. (peor)");
        if (ms_de[BN_CA] >= 0.0 && ops_ca > 0.0) {
            /* una operación de CA, con lo que no se cuenta: CA_NS_OP (ca.h) */
            buf_str(&b, "; CA: ");
            buf_int(&b, (long)(ops_ca + 0.5));
            buf_str(&b, " op. a ");
            buf_num(&b, ms_de[BN_CA] * 1e3 / ops_ca, 3);
            buf_str(&b, " " S_MU "s");
        }
        gfx_text(6, y, t, C_SUAVE, 1);
    }

    gfx_fill(0, 226, SCR_W, 14, C_PANEL);
    gfx_text(6, 227, !banco_hecho() ? "[esc] cancelar y volver" :
                     con_segunda ? "[enter] repetir  [izq/der] p" S_aa "gina  [esc] volver"
                           : "[enter] repetir               [esc] volver", C_SUAVE, 1);
}
