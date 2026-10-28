/* ============================================================================
 *  nolin.h  -  Los modelos no lineales, sin estado (Etapa 4)
 *
 *  Lo que sabe de física un diodo, un zener, un LED, un AO, un BJT o un
 *  MOSFET, y nada del circuito: el motor (sim.c) arma el Newton, la caché y
 *  los eventos; aquí solo se evalúa UN elemento en unas tensiones dadas.
 *  Ninguna función guarda nada entre llamadas (salvo el contador nl_ops): el
 *  estado de Newton de cada elemento lo guarda quien llama.
 *
 *  Dos familias (D2):
 *    - por tramos (diodo ideal y con Vγ, zener de libro, los dos AO): en cada
 *      tramo el elemento es lineal, i = g·v + i0, y el margen dice cuánto le
 *      falta para salir de él (>= 0 dentro, afín en v, 0 justo en la esquina);
 *    - suaves (la unión exponencial con Rs condensada, el BJT y el MOSFET):
 *      nl_evalua los linealiza en sus puertos con los límites de SPICE3.
 *
 *  Convenios de nl_evalua (lo que el motor tiene que saber):
 *    - vold guarda las variables que se LIMITAN, en el sentido del modelo (con
 *      pol), como el state0 de SPICE3: la unión, vd (la de la unión, sin Rs);
 *      el BJT, (pol·vBE, pol·vBC); el MOS, (pol·vGS, pol·vDS).
 *    - L entra con la linealización anterior (el compañero con que se
 *      resolvió) y sale con la nueva. Solo la unión con Rs lee la de entrada:
 *      la vd nueva sale de ese compañero (D4). Si L no es la de vold (L->vd
 *      distinta de vold[0], o L a cero), se rehace en vold[0] con una exp()
 *      más: así vale también un NlLin recién puesto a cero.
 *    - vpuerto == NULL es el arranque (el MODEINITJCT de SPICE3): tras
 *      nl_arranque se evalúa en vold, sin límites.
 *    - (L->v, L->i, L->g) es el compañero sobre la curva del modelo: v es la
 *      tensión de puerto del punto del modelo (en la unión con Rs, vd +
 *      Rs·ij(vd), que es la de la solución salvo lo que le falte a Newton) e
 *      i su corriente; el motor estampa g e ieq_p = i_p - Σ g_pq·v_q (§3.C).
 * ========================================================================= */
#ifndef NOLIN_H
#define NOLIN_H
#include "sim.h"
#define NL_VT      0.025864925786328750   /* kT/q a 27 °C (300,15 K), CODATA 2018            */
#define NL_GMIN    1e-12                  /* S: fuga de cada unión y del corte por tramos     */
#define NL_RON     1e-3                   /* Ω: conducción por tramos con rd vacía o 0        */
#define NL_XLIM    80.0                   /* e^x sigue en recta por encima                    */
/* NL_OPS_EXP y NS_ITER, medidos en la CX II real con el banco (Ayuda > T)
 * SIN el cable USB (2026-09-27, la candidata, a 396 MHz): una exp() 7,8439 µs
 * = 16,9 operaciones de 0,46329 µs (BN_EXP); lo fijo de una iteración, de
 * BN_NL (3530 llamadas en 4 s, 1,1331 ms cada una): esa carga (el puente, 1 s
 * con h = T/45) reproducida en el PC da 3306 pasos (224 llamadas no dan paso:
 * Newton lo parte), 9607 iteraciones y 3 880 138 operaciones (sus 25 712 exp
 * y log a 17, el seno a SIM_OPS_SIN), y (4000 ms − las operaciones·0,46329
 * µs − los pasos·43,45 µs)/9607 = 214,3 µs (lo que no se cuenta de una
 * iteración: los estampados, la LU y la sustitución por dentro y las
 * llamadas). Con la newlib (qemu-arm) salen 3304 pasos y 9636 iteraciones:
 * 213,3 µs. La pantalla de la candidata decía 164,4: por paso y con las exp
 * y el seno a 30 (con las cuentas del PC y su fórmula, 165,5: la carga es
 * esa). test_calibracion (el arnés) reproduce esas cargas. El log() no
 * se midió: cuenta como una exp(). El banco del cierre iba con el USB (288
 * MHz): 250,5 µs, con el seno a 30. BN_NL es la única línea del banco que no
 * escala como el reloj (1,29 frente a ~1,37; con la misma carga, 1,30):
 * escalado por el flop, lo del cierre daría 183 µs; va lo medido sin el USB,
 * que es lo más lento. Antes, 30 y 15 µs, a ojo. */
#define NL_OPS_EXP 17                     /* una exp() o un log() (BN_EXP)                    */
#define NL_OPS_OP  2050000UL              /* tope de un OP (y de un punto del barrido): ~0,95 s,
                                             sus operaciones y lo fijo de sus iteraciones (cada
                                             una, NS_ITER/SIM_NS_OP = 463 operaciones: sim.c).
                                             Eran 1 500 000 operaciones solas: el OP más largo de
                                             la batería que se cortaba, con 1224 iteraciones,
                                             pasaba de 1,26 s con el banco del cierre           */
#define NS_ITER    214300.0               /* ns fijos por iteración de Newton (BN_NL)          */
/* Por omisión (vacío): uno por cada hueco con rótulo de vacío en la ficha (§2.2) */
#define NL_VG      0.7
#define NL_IS_D    1e-14
#define NL_N       1.0                    /* QD_N                                             */
#define NL_RS      0.0                    /* QD_RS                                            */
#define NL_VZ      5.1                    /* QZ_VZ                                            */
#define NL_IZ      5e-3
#define NL_IF      20e-3
#define NL_IM      30e-3
#define NL_A       1e5
#define NL_VSAT    15.0
#define NL_BF      100.0
#define NL_IS_Q    1e-15
#define NL_BR      1.0
#define NL_VT_M    2.0
#define NL_K       1e-3

typedef struct { const char *nombre; double Is, n, Rs; } NlDiodo;       /* catálogo */
extern const NlDiodo NL_1N4148, NL_1N4007;                              /* §3.A */
typedef struct { double Vf, n, Rs; unsigned short rgb; } NlColor;
extern const NlColor NL_COLOR[LED_N];

enum { TR_CORTE = 0, TR_CONDUCE, TR_RUPTURA };       /* D por tramos                          */
enum { TR_LIN = 0, TR_SATP, TR_SATN };               /* AO                                    */
double nl_exp(double x, double *d);                  /* e^x con recta por encima de 80; d = derivada */
double nl_pnjlim(double vnew, double vold, double nvt, double vcrit, int *lim);   /* SPICE3 */
double nl_fetlim(double vnew, double vold, double vto);
double nl_limvds(double vnew, double vold);
/* Los límites del MOS como mos1load (§3.D): directo o inverso según el vDS de antes.
 * x = pol·vGS, y = pol·vDS nuevos; xo, yo los de antes. Devuelve 1 si limitó. */
int    nl_mos_lim(double *x, double *y, double xo, double yo, double vt);
double nl_vcrit(double Is, double nvt);              /* nvt·ln(nvt/(√2·Is))                   */
/* Diodo por tramos: i = g·v + i0 en el tramo; margen >= 0 dentro (afín en v) */
void   nl_tr_recta(const SimEl *e, int tramo, double *g, double *i0);
double nl_tr_margen(const SimEl *e, int tramo, double v);
int    nl_tr_de(const SimEl *e, double v);           /* el tramo que contiene v                */
int    nl_tr_vecino(const SimEl *e, int tramo, double v);   /* por dónde sale                 */
/* Unión con Rs condensada: el compañero en vd, y la vd nueva limitada */
void   nl_union_j(const SimEl *e, double vd, double *ij, double *gj);
void   nl_union_comp(const SimEl *e, double vd, double *G, double *Ieq);
double nl_union_vd(const SimEl *e, double vd_ant, double v, double G, double Ieq, int *lim);
/* BJT (NPN o PNP): corrientes que ENTRAN por C y B, y sus derivadas respecto a vBE y vBC */
typedef struct { double ic, ib, dic_be, dic_bc, dib_be, dib_bc; } NlBjt;
void   nl_bjt(const SimEl *e, double vbe, double vbc, NlBjt *o);
/* MOSFET: ID que ENTRA por D, gm = dID/dVGS, gds = dID/dVDS */
typedef struct { double id, gm, gds; } NlMos;
void   nl_mos(const SimEl *e, double vgs, double vds, NlMos *o);
/* Linealización de un suave en dos puertos (§3.C): la unión usa el puerto 0 (a, b); el BJT
 * (B, E) e (C, E); el MOS (G, S) y (D, S). v: tensiones de puerto ya limitadas; i: la
 * corriente que entra por el + de cada puerto; g: di_p/dv_q; vd: la de la unión (con Rs) */
typedef struct { double v[2], i[2], g[2][2], vd; } NlLin;
/* Evalúa el suave en las tensiones de sus puertos, limitándolas respecto a vold (pnjlim,
 * fetlim, limvds); vold pasa a ser las usadas. 1 si alguna se limitó. */
int    nl_evalua(const SimEl *e, const double vpuerto[2], double vold[2], NlLin *L);
void   nl_arranque(const SimEl *e, double vold[2]);  /* uniones en Vcrit; BJT (Vcrit, 0); MOS (Vt, 0) */
/* AO: margen del tramo con u = V+ - V- (ideal: u = Vsal en el lineal) */
double nl_ao_margen(const SimEl *e, int tramo, double vd, double vsal);
int    nl_region(const SimEl *e, const Valores *v, int k);           /* REG_*                  */
double nl_escala_v(const SimEl *e);                  /* Vγ, Vz, V+ - V-: para Vesc             */
unsigned long nl_ops(void); void nl_ops_reset(void); /* se suma a sim_ops()                    */
unsigned long nl_exps(void);                          /* las exp() y log() de nl_ops (el reset, el mismo) */
#endif
