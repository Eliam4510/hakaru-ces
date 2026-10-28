/* ============================================================================
 *  sim.h  -  El emulador: análisis nodal modificado (MNA) en el tiempo
 *
 *  El motor de resuelve.c es la CALCULADORA: despeja incógnitas a partir de
 *  datos. El emulador hace lo que haría el circuito de verdad: todos los
 *  valores son conocidos y calcula lo que pasa, primero el punto de
 *  operación de CC y después la evolución en el tiempo.
 *
 *  Trabaja sobre su propia lista de elementos (SimRed): el adaptador
 *  sim_red_de() la saca del Circuito y de su Red, el arnés la arma a mano
 *  para los casos con solución exacta y el exportador del oráculo la
 *  escribe como texto.
 *
 *  Incógnitas: el potencial de cada nodo (menos la referencia de cada
 *  subcircuito) y la corriente de rama de todo lo que impone una tensión o
 *  tiene estado: fuente ideal, amperímetro ideal, interruptor, bobina y
 *  condensador. Una fuente real (E, r) entra como su Norton (1/r y E/r).
 *
 *  En el tiempo: C y L como modelos compañeros (una conductancia más una
 *  fuente que dependen del paso), trapecio de paso fijo, y tras cada
 *  discontinuidad (un interruptor, un flanco, un vértice) dos medios pasos
 *  de Euler implícito, que usan la MISMA factorización que el trapecio
 *  (Euler con h/2 tiene la matriz del trapecio con h) y matan las
 *  oscilaciones falsas del trapecio. La malla se reancla en cada evento; un
 *  evento que cae entre dos puntos parte el paso.
 *
 *  Convenio (el de todo el proyecto): I va de a hacia b por dentro del
 *  elemento; V = phi(a) - phi(b) en un pasivo; en una fuente b es el + y su V
 *  es la de sus bornes, phi(b) - phi(a).
 * ========================================================================= */
#ifndef SIM_H
#define SIM_H

#include "circuito.h"
#include "red.h"
#include "resuelve.h"
#include "ondas.h"
#include "lu.h"                 /* LU_MAX (SimCc); lu.h no incluye nada del motor */

/* Tipos del emulador */
enum {
    SE_R = 0,           /* resistencia (también un instrumento con su R)  */
    SE_E,               /* fuente de tensión; val = r (0 = ideal)         */
    SE_J,               /* fuente de corriente                            */
    SE_C, SE_L,         /* condensador, bobina                            */
    SE_S,               /* interruptor ideal                              */
    SE_AMI,             /* amperímetro ideal: un cable que mide           */
    SE_VMI,             /* voltímetro ideal: no hace nada (mide)          */
    SE_Z,               /* impedancia de CA: val = R, p[0] = X a la f p[1] (Hz) */
    /* Etapa 4 */
    SE_D,               /* diodo, zener, LED: a = ánodo, b = cátodo       */
    SE_EC,              /* E controlada: b = +; val = μ o r               */
    SE_JC,              /* J controlada: de a hacia b; val = g o β        */
    SE_AO,              /* a = +, b = -, c = salida, nref = referencia de la salida */
    SE_BJT,             /* a = C, b = E, c = B                            */
    SE_MOS,             /* a = D, b = S, c = G                            */
    SE_N
};
/* Modelos de los no lineales (SimEl.modelo) */
enum { NL_NO = 0, NL_TRAMOS, NL_UNION, NL_AO_IDEAL, NL_AO_REAL, NL_BJT, NL_MOS };
#define SIM_NM 6
/* El vatímetro es un SE_AMI con instr = 'W': su V es phi(b) (la bobina de
 * tensión, a la referencia) y marca la P media de v·i. */

typedef struct {
    unsigned char tipo;         /* SE_*                                      */
    unsigned char forma;        /* E, J: ONDA_*                              */
    unsigned char ci;           /* C, L: 1 si x0 es su valor inicial         */
    unsigned char cerrado0;     /* S: 1 si empieza cerrado                   */
    short  k;                   /* elemento del Circuito, o -1               */
    short  a, b;                /* nodos                                     */
    double val;                 /* R, C, L; en una fuente de tensión, su r   */
    double p[ONDA_PAR];         /* E, J: parámetros de la forma (ondas.h)    */
    double x0;                  /* C: v0 = phi(a)-phi(b); L: i0 de a hacia b */
    double t1, t2;              /* S: conmuta en t1 y vuelve en t2 (<0 nunca) */
    unsigned char instr;        /* 'V', 'A' o 'W' si es un instrumento; si no, 0 */
    unsigned char med;          /* instrumento: MED_* (qué marca)            */
    /* Etapa 4. Con memset a 0 los tipos de hoy no cambian: estos campos solo se
     * leen en SE_D, SE_EC, SE_JC, SE_AO, SE_BJT y SE_MOS. */
    short  c;                   /* AO: salida; BJT: B; MOS: G (-1 en los de dos bornes) */
    short  nref;                /* AO: nodo de la referencia de su salida    */
    signed char ctl;            /* EC, JC: el elemento que la controla       */
    unsigned char ctl_i;        /* EC, JC: 1 = su corriente; 0 = su tensión  */
    unsigned char modelo;       /* NL_*                                      */
    signed char pol;            /* BJT, MOS: +1 NPN o canal N; -1 PNP o canal P */
    double m[SIM_NM];           /* parámetros del modelo, ya resueltos       */
} SimEl;
/* Contrato de m[] (el adaptador pone los vacíos, el catálogo y las cuentas):
 *   NL_TRAMOS (D)     Vγ, Ron (> 0), Vz0 (0 = sin ruptura), Rz (> 0)
 *   NL_UNION (D)      Is, n, Rs, BV (0 = sin ruptura), Ibv, Vcrit
 *   NL_AO_*           A (ideal: 0), V+, V-, Ri (0 = ∞), Ro
 *   NL_BJT            Is, βF, βR, VA (0 = ∞), -, Vcrit
 *   NL_MOS            Vt (>= 0), K, VA (0 = ∞) */

typedef struct {
    int    nn, nel;
    unsigned char ref[MAX_NODOS];   /* 1 = referencia (0 V) de su subcircuito */
    SimEl  el[MAX_EL];
} SimRed;

enum {
    SIM_OK = 0,
    SIM_VACIO,          /* no hay elementos                                  */
    SIM_FALTA,          /* un valor que el emulador necesita no es dato      */
    SIM_SINGULAR,       /* el circuito no tiene una solución única           */
    SIM_GRANDE,         /* no cabe                                           */
    SIM_LAZO_V,         /* un lazo de fuentes, amperímetros o interruptores  */
    SIM_CORTE_J,        /* una J sin camino de vuelta                        */
    SIM_CANCELADO,
    /* Etapa 3 */
    SIM_SOLO_CA,        /* una Z al emular en el tiempo o en el punto de operación */
    SIM_CA_FORMA,       /* una cuadrada, triangular o pulso en fasores       */
    SIM_CA_FRECS,       /* dos senoidales de frecuencias distintas (elem y lazo) */
    SIM_CA_SIN_F,       /* ni fuentes senoidales ni f del circuito           */
    SIM_RESONANCIA,     /* a esa f (en t) un LC resuena sin pérdidas (elem: la L o la C) */
    SIM_SIN_AB,         /* Thévenin: faltan A o B, o no están en el circuito */
    SIM_AB_SEPARADOS,   /* Thévenin: A y B no están unidos por nada          */
    SIM_SIN_REGIMEN,    /* Thévenin de CC: una J carga un C, una E alimenta una L */
    SIM_SIN_FUENTE,     /* Bode: no hay E ni J de entrada                    */
    /* Etapa 4 */
    SIM_NO_CONVERGE,    /* Newton no converge: elem = el culpable; t = instante (-1 en el
                           OP); campo = NC_*; en NC_BARR, t = el valor del parámetro */
    SIM_CTL_MAL,        /* una controlada sin control válido: elem = ella; nodo = el que
                           la controla (o -1); campo = CTL_* (los de circ_ctl_motivo) */
    SIM_NO_LINEAL,      /* Thévenin y la calculadora de CA con un no lineal (elem) */
    SIM_NADA_BARRER,    /* el barrido: no hay E, J, R ni controlada que barrer */
    SIM_MODELO,         /* parámetros imposibles del modelo: elem, campo = el hueco
                           (QZ_RZ, QL_IF, QA_VP; Q_VAL = una variante que no existe) */
    SIM_AO_REF          /* la salida de un AO sin referencia: elem = el AO; campo = AOR_* */
};
enum { NC_OP = 0, NC_TR, NC_BARR, NC_CONMUTA, NC_TOPE };
enum { AOR_SIN_TIERRA = 0,           /* su subcircuito no tiene tierra ni fuente independiente */
       AOR_CORTO };                  /* la salida es su propia referencia (c == nref)          */

/* Avisos: el resultado vale, pero hay algo que decir */
#define SIM_AV_FLOTA     0x0001u  /* parte sin referencia: potencial anclado          */
#define SIM_AV_CARGA     0x0002u  /* C en serie sin V0: regla de la carga             */
#define SIM_AV_FLUJO     0x0004u  /* L en paralelo sin I0: regla del flujo            */
#define SIM_AV_C_DESC    0x0008u  /* sin régimen de CC: una C empieza descargada      */
#define SIM_AV_L_SIN_I   0x0010u  /* sin régimen de CC: una L empieza sin corriente   */
#define SIM_AV_CI_SALTA  0x0020u  /* una condición inicial no cuadra: salta en 0+     */
#define SIM_AV_IMPULSO   0x0040u  /* hubo saltos instantáneos                         */
#define SIM_AV_PASO_LIM  0x0080u  /* el tiempo de cálculo agrandó el paso             */
#define SIM_AV_NO_ESTAC  0x0100u  /* la ventana no llega al régimen permanente        */
#define SIM_AV_QUIETO    0x0200u  /* nada cambia con el tiempo                        */
#define SIM_AV_SIN_MODOS 0x0400u  /* no se hallaron los modos: paso y ventana a ojo   */
/* Etapa 3 */
#define SIM_AV_CC_FUERA  0x0800u  /* CA: una fuente continua o un nivel de CC se apagan */
#define SIM_AV_S_FINAL   0x1000u  /* CA: los S cuentan en su estado final             */
#define SIM_AV_CI_NO     0x2000u  /* CA: V0 e I0 no cuentan                           */
#define SIM_AV_Z_ESCALA  0x4000u  /* una Z a otra f que la del circuito (R+L o R+C)   */
#define SIM_AV_OTRAS     0x8000u  /* Bode: las demás fuentes se apagan                */
/* Etapa 4 */
#define SIM_AV_BIESTABLE  0x010000u  /* un AO puede empezar en V+ o en V-: empieza en V+    */
#define SIM_AV_ARRANQUE   0x020000u  /* un AO arranca en su punto inestable (astable)       */
#define SIM_AV_HOMOTOPIA  0x040000u  /* el OP necesitó gmin o la rampa de fuentes           */
#define SIM_AV_PEQ_SENAL  0x080000u  /* CA o Bode de pequeña señal en el punto Q            */
#define SIM_AV_SATURA     0x100000u  /* un AO satura (en el OP, en Q o al final)            */
#define SIM_AV_QUEMA      0x200000u  /* un LED pasa de su Im                                */
#define SIM_AV_CC_BARRIDO 0x400000u  /* el barrido es de CC (C abiertos, L cables)          */
#define SIM_AV_SALTO      0x800000u  /* el barrido salta (histéresis)                       */
#define SIM_AV_HUECOS     0x1000000u /* el barrido tiene huecos (Newton no convergió ahí)   */

/* Regiones (solo para enseñar: tabla, rótulos, esquema) */
enum { REG_NADA = 0, REG_CORTE, REG_CONDUCE, REG_RUPTURA, REG_ACTIVA, REG_SATURACION,
       REG_INVERSA, REG_OHMICA, REG_LINEAL, REG_SAT_MAS, REG_SAT_MENOS, REG_N };

/* Los contadores de Newton. Newton los suma en el SimNewton que le pasa quien
 * llama: el emulador pasa &res_.nw; el punto de operación, el punto Q y el
 * barrido, uno suyo. Así ninguna ruta de OP escribe en res_. */
typedef struct {
    long   iter;                /* iteraciones de Newton                     */
    int    iter_max;            /* la mayor en un paso (o en el OP)          */
    long   sub;                 /* subpasos de más (la subdivisión)          */
    int    eventos;             /* cambios de tramo localizados              */
    unsigned char homotopia;    /* OP: 0 directo, 1 gmin, 2 rampa de fuentes */
    int    iter_op;             /* las del OP                                */
    double residuo_max;         /* el mayor residuo de Kirchhoff relativo    */
    long   sonda;               /* las de la presonda (una vez por clase de
                                   evento de una red no lineal), aparte de iter */
} SimNewton;
typedef unsigned long long SimTramos;    /* 2 bits por elemento: el tramo de cada D y AO */

typedef struct {
    int estado;         /* SIM_*                                             */
    int elem;           /* elemento (de la SimRed) al que se refiere, o -1   */
    int nodo;
    int n;              /* incógnitas del sistema                            */
    int campo;          /* SIM_FALTA: la magnitud (Q_*) que falta             */
    unsigned int lazo;  /* LAZO_V, CORTE_J: bit k = el elemento k está en el
                           lazo o en el corte                                */
    unsigned int avisos;/* SIM_AV_*                                          */
    int elem_aviso;     /* el primer elemento que dio un aviso (el AO de
                           BIESTABLE o ARRANQUE; si no, el de SATURA, si
                           los hay), o -1                                    */
    double t;           /* instante de la topología del problema; -1 = el
                           punto de operación                                */
} SimDiag;

/* Del Circuito (con su Red ya extraída) a la red del emulador */
int  sim_red_de(const Circuito *c, const Red *r, SimRed *s, SimDiag *d);
/* Como sim_red_de. Con tolera = 1 (la calculadora de CA) un valor que no es
 * dato toma uno provisional (R 1 kΩ, C 1 µF, L 1 mH, |Z| 1 kΩ, |E| 1, frec
 * 1 kHz) en vez de dar SIM_FALTA. La r buscada de una fuente y la R buscada
 * de un instrumento siguen dando SIM_FALTA. */
int  sim_red_de_ex(const Circuito *c, const Red *r, SimRed *s, SimDiag *d, int tolera);

/* ---- Para la alterna (ca.c) ---- */
/* Topología y mapa para la alterna: la del transitorio (C y L como
 * conductancias) con los S en su estado final. No toca la última
 * emulación: R_, el mapa y los avisos se guardan y se reponen. */
typedef struct {
    int n;                               /* incógnitas                               */
    signed char   col_phi[MAX_NODOS];    /* columna del potencial; -1 = referencia    */
    signed char   col_br[MAX_EL];        /* columna de la corriente de rama, o -1     */
    unsigned char ancla[MAX_NODOS];      /* 1: la fila del nodo pasa a phi = 0        */
    short         pieza[MAX_NODOS];      /* trozo (G y V) de cada nodo                */
    unsigned int  mask;                  /* S cerrados (estado final)                 */
} SimTopoCA;
unsigned sim_mascara_final(const SimRed *s);     /* cerrado0 ^ (t1 >= 0) ^ (t2 >= 0) */
int      sim_topo_ca(const SimRed *s, SimTopoCA *t, SimDiag *d);
/* Thévenin de CC entre los nodos na y nb: punto de operación sin V0 ni I0 y
 * con los S al empezar; *inf = 1 si A y B están en trozos distintos de CC (R
 * infinita). Guarda y repone el estado de la última emulación. */
int      sim_thevenin_cc(const SimRed *s, int na, int nb, double *vth, double *rth, int *inf, SimDiag *d);

/* ¿Tiene el circuito todo lo que el emulador necesita? SIM_OK o SIM_FALTA
 * (en d, el elemento y la magnitud que falta). No necesita la Red. */
int  sim_valida(const Circuito *c, SimDiag *d);

/* Punto de operación de CC (fuentes en t = 0⁻, C abiertos y L en corto, o
 * con su V0/I0). Valores con los convenios del motor de tabla. */
int  sim_op_red(const SimRed *s, Valores *v, SimDiag *d);
int  sim_punto_operacion(const Circuito *c, const Red *r, Valores *v, SimDiag *d);

/* ---- Transitorio ---- */
#define SIM_MAX_PASOS     20000
#define SIM_MAX_MUESTRAS  24576
#define SIM_MAX_SUCESOS   16
#define SIM_MAX_TOP       8          /* topologías (máscaras de interruptores) */
#define SIM_MAX_MODOS     MAX_EL
#define SIM_OPS_TICK      150000UL   /* ~70 ms de operaciones en la calculadora */
/* El modelo de tiempos de la calculadora (§17.4; el de barrido.c y el del
 * arnés), medido en la CX II real con el banco (Ayuda > T) SIN el cable USB,
 * tras apagar y encender (2026-09-27, la candidata, a 396 MHz): una operación
 * contada (multiplicar y restar, BN_FLOP) 0,46329 µs = 183 ciclos; una sin()
 * de la libm (BN_SENO) 7,2573 µs = 16 operaciones; lo fijo de un paso, de
 * BN_SIM (el peor emulador, 2 ms con h = 2 µs: 5370 pasos de 744,88 µs en
 * 4 s, 8 130 284 operaciones con sus dos arranques) (4000 ms − las
 * operaciones·0,46329 µs)/5370 = 43,45 µs. Sale de una red de 32 elementos:
 * una pequeña paga menos (del lado seguro). Separarlo en a + b·n pide otra
 * línea del banco: un paso de una red pequeña con h fija (la segunda página
 * que añade rev9, B2_PEQUE con B2_PASO, lo mide); BN_NL no sirve (su paso y
 * sus iteraciones van juntos en la misma vuelta). El banco del cierre iba
 * con el USB, y con él la CX II va a 288 MHz: 0,63369 µs (0,63369/0,46329 =
 * 1,368 ≈ 396/288) y 73,8 µs; lo fijo es una resta pequeña de dos números
 * grandes (un 1 % del flop lo mueve un 16 %). Antes aún, 0,63 y 20 µs, a
 * ojo. Lo fijo de una iteración de Newton es NS_ITER (nolin.h). */
#define SIM_NS_OP         463.29     /* ns por operación contada             */
#define SIM_NS_PASO       43450.0    /* ns fijos por paso del emulador       */
#define SIM_OPS_SIN       16         /* una sin() de la libm (el seno de una fuente) */

enum { CANAL_NADA = 0, CANAL_NODO, CANAL_I, CANAL_V };
typedef struct {
    unsigned char tipo;         /* CANAL_*                                   */
    unsigned char automatico;   /* 1 = lo eligió el emulador (sin sondas)    */
    signed char   elem, nodo;   /* I, V: el elemento; NODO: el nodo; si no, -1 */
    signed char   sg;           /* I: +1 de a hacia b; V: +1 phi(a) - phi(b)  */
    char          nombre[12];   /* "V(C1)", "I(L1)", "nodo B"                */
} SimCanal;

enum { FIN_NO = 0, FIN_COMPLETO, FIN_CORTADO, FIN_TOPE, FIN_ERROR };
enum { SUC_IMPULSO_C = 1, SUC_IMPULSO_L };
typedef struct {
    double t;
    unsigned char tipo;         /* SUC_*                                     */
    signed char elem;           /* el C que cierra el lazo o la L del corte  */
    unsigned int lazo;          /* bit k: el elemento k está en el lazo o corte */
} SimSuceso;
/* Un modo: s = re ± j·im, im >= 0. Con él, lo que el planificador del tramo
 * rígido (§17.3) mira de su módulo, calculado una vez al hallarlo (sim.c,
 * modos_de) y no en cada pregunta de elige_paso: mag = |s|, hl = RIG_HL/|s|
 * (el h desde el que es rígido) y taus[j - 1] = RIG_TAUS/(|s|·j), j = 1 a
 * SIM_MODO_NJ (el h desde el que su tramo baja de j + 1 a j pasos h) */
#define SIM_MODO_NJ 4
typedef struct { double re, im, mag, hl, taus[SIM_MODO_NJ]; } SimModo;

typedef struct {
    double t_fin;       /* 0 = automático                                    */
    double h;           /* 0 = automático (si no, es un máximo)              */
    SimCanal canal[2];
    long   max_pasos;   /* 0 = SIM_MAX_PASOS                                 */
} SimAjustes;

typedef struct {
    SimDiag  d;
    int      fin;                     /* FIN_*                               */
    double   h, t_fin, t_pedido, t_med0;   /* t_pedido: el t_fin sin topes    */
    unsigned char h_auto, t_auto;
    unsigned char paso_mayor;         /* elige_paso agrandó el paso por el tiempo (PASO_LIM) */
    SimCanal canal[2];
    Valores  op;                      /* en t = 0⁻, con los convenios de la tabla */
    Valores  ultimo;                  /* en t_fin (límite por la derecha)    */
    double   Vmax[MAX_EL], Imax[MAX_EL], Pmax[MAX_EL];   /* con signo: el de mayor módulo */
    double   W[MAX_EL];               /* energía en [0, t_fin]: absorbida
                                         (pasivos) o entregada (fuentes)     */
    double   Vmed[MAX_EL], Vef[MAX_EL], Imed[MAX_EL], Ief[MAX_EL], Pmed[MAX_EL];  /* en [t_med0, t_fin] */
    double   E0[MAX_EL], Efin[MAX_EL], Emax[MAX_EL];     /* almacenada: C y L */
    double   lectura[MAX_EL];         /* V y A: lo que marcan según su modo  */
    double   dif_balance;             /* entregada - disipada - Δalmacenada  */
    int      ntop;
    unsigned int mascara[SIM_MAX_TOP];
    double   t_top[SIM_MAX_TOP];
    int      nmodos[SIM_MAX_TOP];
    SimModo  modo[SIM_MAX_TOP][SIM_MAX_MODOS];
    double   tau_min, tau_max, t_osc; /* de los modos: 1/|s| mínimo, 1/alfa
                                         máximo, periodo más largo (0 = no)  */
    int      nsucesos;
    SimSuceso suceso[SIM_MAX_SUCESOS];
    long     pasos;
    int      factorizaciones, eventos;
    int      tramos;                  /* eventos con el tramo rígido (§17.3) */
    double   t_est;                   /* segundos estimados en la calculadora */
    /* Etapa 4 */
    SimNewton nw;                     /* los de la emulación (con su OP)     */
    SimTramos tramos_top[SIM_MAX_TOP];/* los tramos de cada topología de los modos */
} SimRes;

typedef struct {
    int      n;
    double   t[SIM_MAX_MUESTRAS];     /* no decreciente; en un evento, dos con el mismo t */
    double   y[2][SIM_MAX_MUESTRAS];
    double   s1[2][SIM_MAX_MUESTRAS]; /* ∫y dt por trapecio, acumulada        */
    double   s2[2][SIM_MAX_MUESTRAS]; /* ∫y² dt                               */
    double   ymin[2], ymax[2];
    unsigned gen;                     /* cambia en cada emulación            */
} SimReg;

/* Canales: las sondas del circuito, o los automáticos */
void sim_canales(const Circuito *c, const Red *r, SimCanal ch[2]);

/* Empieza a emular el circuito (lo copia): punto de operación, topologías,
 * modos, paso y ventana. SIM_OK o el error (en sim_res()->d). */
int  sim_iniciar(const Circuito *c, const Red *r);
/* Lo mismo sobre una SimRed y con ajustes a mano (el arnés) */
int  sim_iniciar_red(const SimRed *s, const SimAjustes *aj);
/* Avanza pasos enteros hasta gastar unas `ops` operaciones. 1 = queda trabajo */
int  sim_avanzar(unsigned long ops);
/* Para: FIN_CORTADO; lo hecho vale */
void sim_cortar(void);
/* Lo que va hecho, de 0 a 1 */
double sim_progreso(void);
/* Todo de una vez (el arnés). Devuelve el estado. */
int  sim_emular(const SimRed *s, const SimAjustes *aj);

const SimRes *sim_res(void);
const SimReg *sim_reg(void);
/* La SimRed de la última emulación */
const SimRed *sim_red(void);

/* Valor de un canal en t, interpolado (en un evento, el de después) */
double sim_canal_en(int canal, double t);
/* Medidas de un canal en [ta, tb]; media y eficaz en ciclos enteros */
typedef struct {
    double max, min, pp, media, eficaz, frec;   /* frec = 0 si no se halló */
} SimMedidas;
void sim_medir(int canal, double ta, double tb, SimMedidas *m);

/* Mensaje en español del diagnóstico, y de un aviso (un bit SIM_AV_*) */
const char *sim_mensaje(const SimRed *s, const SimDiag *d, char *buf, int cap);
const char *sim_mensaje_circ(const Circuito *c, const SimRed *s, const SimDiag *d, char *buf, int cap);
const char *sim_aviso(const Circuito *c, const SimRes *s, unsigned bit, char *buf, int cap);

/* Modos de la topología inicial con un paso de prueba dado (el arnés) */
int  sim_modos_prueba(const SimRed *s, double h0, SimModo *m, int *n);

/* Escrituras con un índice fuera de su tabla: el arnés exige 0 */
unsigned long sim_fuera(void);
/* Tope de pasos (el mono): 0 = el de siempre */
void sim_tope_pasos(long n);
/* El arnés: una función que se llama al cerrar cada emulación (la huella de
 * HAKARU_HUELLA_LOG); 0 = ninguna (la calculadora) */
void sim_al_terminar(void (*f)(void));

/* ---- Etapa 4: no lineales ---- */
int  sim_hay_nl(const SimRed *s);          /* SE_D, SE_AO, SE_BJT o SE_MOS                    */
/* Punto de operación de CC en la vista de la calculadora ([=] sin dinámica, [P]): C
 * abiertos, L cables, sin V0 ni I0, fuentes en su nivel de CC, S al empezar. Valores con
 * los convenios de la tabla (V2, I2 y reg incluidos). Guarda y repone la última emulación
 * y no toca la caché del transitorio ni res_. nw puede ser NULL. */
int  sim_op_cc_red(const SimRed *s, Valores *v, SimDiag *d, SimNewton *nw);
int  sim_punto_op_cc(const Circuito *c, const Red *r, Valores *v, SimDiag *d, SimNewton *nw);
double sim_residuo_max(void);              /* el arnés: el de la última emulación (res_.nw)  */
double sim_residuo_acum(void);             /* el arnés: el mayor desde el último reset, de
                                              TODAS las soluciones de Newton (el mono), con
                                              el suelo de redondeo de la escala de la red   */
void   sim_residuo_acum_reset(void);
double sim_residuo_barr(void);             /* el del motor desde el último reset: el barrido
                                              (su kcl_peor); no toca el del mono            */
void   sim_residuo_barr_reset(void);
void   sim_nl_topes(int it_op, int it_tr); /* el arnés: 0 = los de siempre                 */
void   sim_nl_tope_ops(unsigned long ops); /* el arnés: el tope de un OP; 0 = NL_OPS_OP    */
enum { NLP_NADA = 0, NLP_INESTABLE, NLP_SIN_GMIN, NLP_SIN_HOMOTOPIA, NLP_FALLA_BARR,
       NLP_SIN_PULIDO };                    /* el paso sin su iteración de más: un residuo de
                                               verdad para la prueba del residuo del mono  */
void   sim_nl_prueba(int que, double t);    /* el arnés: ganchos (desde t, o en x = t)      */

/* El estampado de las controladas (y, desde el paso 8, de los linealizados),
 * el mismo para la LU real y la compleja (ca.c pasa un envoltorio a
 * luc_suma). El mapa es el del que llama. Sin la ley de nodos de las ramas
 * (la pone la línea común de cada ensamblado, con el puerto 0). */
typedef struct { int phi[MAX_NODOS]; int br[MAX_EL]; } SimMapa;
typedef void (*SimSuma)(void *m, int i, int j, double v);
/* Punto Q y linealización (CA y Bode, §3.I). Fuentes en su nivel de CC, C y L
 * sin V0 ni I0, S en su estado final. Guarda y repone la última emulación. */
typedef struct SimLin {
    unsigned char hay;                    /* 1: hay no lineales linealizados            */
    unsigned char tramo[MAX_EL];          /* D por tramos y AO: el de Q                 */
    double g[MAX_EL][2][2];               /* suaves: di_p/dv_q en Q (puertos del §3.C)  */
    unsigned char reg[MAX_EL];
    Valores q;
} SimLin;
int  sim_punto_q(const SimRed *s, SimLin *L, SimDiag *d, SimNewton *nw);   /* nw: NULL vale */
/* L NULL: los no lineales en su estado de reposo (D en corte, AO en su lineal) */
void sim_estampa_lin(const SimRed *s, const SimLin *L, const SimMapa *mp, SimSuma suma, void *m);
/* El control de la controlada k de s como combinación de incógnitas: la V o
 * la I del que la controla (con los convenios de su tabla) = Σ c[i]·x[col[i]]
 * más una parte conocida (la I de una E real o de una J, que pone quien
 * llama con su excitación). Devuelve cuántos términos (0 a 2). *fuente = el
 * elemento cuya excitación entra en la parte conocida, con su factor en *f0
 * (1/r en una E real, 1 en una J), o -1. */
int  sim_ctl_coef(const SimRed *s, const SimMapa *mp, int k, int col[3], double c[3], int *fuente, double *f0);
/* CTL_OK o por qué no vale el control de la controlada k (la regla de
 * circ_ctl_motivo sobre la SimRed) */
int  sim_ctl_motivo(const SimRed *s, int k);

/* ---- Etapa 4: un punto del barrido de CC (§3.J) ----
 * El parámetro del elemento k (BX_*) vale v; arranca de io (si io->hay) y lo
 * deja con la solución. Vista de CC de sim_op_cc_red. SIM_OK, o el error
 * (SIM_NO_CONVERGE con NC_BARR y t = v, o NC_TOPE). */
enum { BX_E = 0, BX_J, BX_R, BX_K };      /* valor de una E o J; una R; la ganancia de una controlada */
typedef struct {
    int    hay, n;
    double x[LU_MAX];
    double vlim[MAX_EL][2];
    unsigned char tramo[MAX_EL];
    Valores v;
    int    iter;
    unsigned char salto;                  /* un AO cambió de saturación en este punto   */
    double p;                             /* el valor del parámetro de esta solución    */
    double residuo;                       /* su residuo de Kirchhoff relativo (con sim_cc_residuo(1)) */
} SimCc;
int  sim_cc_punto(const SimRed *s, int k, int bx, double v, SimCc *io, SimDiag *d);
/* dy/dp exacta del canal en la solución de io (una sustitución con la LU del jacobiano) */
int  sim_cc_deriva(const SimRed *s, int k, int bx, const SimCc *io, const SimCanal *ch, double *dy);
/* El tope de operaciones de un punto del barrido (con sus trozos y su OP de rescate):
 * 1 = empieza a contar ahora; 0 = cada OP vuelve a contar desde el suyo */
void sim_cc_tope(int empieza);
/* 1: sim_cc_punto calcula también el residuo de su solución (SimCc.residuo); 0: no */
void sim_cc_residuo(int si);
/* El arnés: los puntos del barrido lineal (una E o una J de una red lineal)
 * hechos sin rehacer la LU: con medio = 0, también sin el análisis (el de un
 * punto anterior, que no miró las fuentes); con medio = 1, con él */
unsigned long sim_cc_cortos(int medio);
#ifndef NDLESS
/* El arnés: 1 = el estado de Newton de antes del paso se guarda y se repone
 * entero (lo de antes de rev9), para compararlo con lo de ahora (solo NL.el) */
void sim_bak_entero(int si);
/* El arnés: 1 = el control del gasto no acorta la ventana (solo corrige
 * t_est), para comparar la ventana entera con el oráculo [67] */
void sim_gasto_sin_corte(int si);
/* El arnés: sim_ops() = v (cerca de su vuelta, como el de 32 bits de la calculadora) */
void sim_ops_pon(unsigned long v);
#endif

/* Operaciones de coma flotante del emulador, para el modelo de tiempos */
unsigned long sim_ops(void);
void          sim_ops_reset(void);

#endif /* SIM_H */
