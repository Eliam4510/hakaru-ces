/* ============================================================================
 *  ca.h  -  La corriente alterna: fasores a una frecuencia
 *
 *  El tercer motor. Trabaja sobre la misma SimRed que el emulador y con su
 *  análisis estructural (sim_topo_ca: el del transitorio, con C y L como
 *  conductancias y los S en su estado final), así que los errores con nombre
 *  (un lazo de fuentes, una J sin vuelta) son los mismos.
 *
 *  La matriz es la del trapecio con g = jωC y z = jωL: por eso el fasor a
 *  ω~ = (2/h)·tan(ωh/2) es exactamente el régimen permanente del emulador
 *  (el arnés lo cruza).
 *
 *  Convenios. Dentro del motor, fasores de PICO referidos al COSENO y en
 *  radianes: x(t) = Re{X·e^{jωt}}, S = ½·V·I*. En el Circuito se guarda la
 *  forma canónica (eficaz, coseno y grados) y Circuito.conv solo cambia lo
 *  que se ve (ca_a_conv y compañía). Los instrumentos marcan eficaces.
 *
 *  Valores con los convenios del motor de tabla: en un pasivo V = phi(a) -
 *  phi(b) e I de a hacia b; en una fuente V = phi(b) - phi(a) (bornes) e I la
 *  que sale por +; en un vatímetro V = phi(b) (su bobina de tensión).
 * ========================================================================= */
#ifndef CA_H
#define CA_H

#include "cx.h"
#include "sim.h"

#define CA_TOL_F 1e-9                  /* dos f son la misma (relativo)                     */

/* Excitación: el fasor interno (pico, coseno) de cada fuente activa */
typedef struct { Cx U[MAX_EL]; unsigned int activa; } CaExc;

/* Resultado a una frecuencia */
typedef struct CaRes {
    SimDiag d;                          /* SIM_OK o el error; avisos SIM_AV_*           */
    double  f, w;
    unsigned int activa;                /* bit k: la fuente k excita                     */
    Cx      phi[MAX_NODOS];
    Cx      V[MAX_EL], I[MAX_EL];
    Cx      S[MAX_EL];                  /* ½·V·I*: absorbida (pasivos) o entregada (E, J);
                                           W: la que pasa hacia la carga                  */
    Cx      Z[MAX_EL];                  /* R, C, L, Z a esta f; 0 en el resto            */
    double  val[MAX_EL], val_ang[MAX_EL];  /* el valor usado: R, C, L, |Z|, |E| eficaz;
                                              ∠Z o ∠E en grados (coseno)                  */
    double  lectura[MAX_EL];            /* V, A: módulo eficaz; W: P (W, con signo)      */
    double  fp[MAX_EL];                 /* W y fuentes: + inductivo, - capacitivo; 0 = S nula */
    Cx      Sfuentes, Scargas;          /* Σ entregada (E, J) y Σ absorbida (R, C, L, Z,
                                           instrumentos con R)                           */
    double  tellegen;                   /* |Sfuentes - Scargas| / Σ|S_k|                 */
    int     fuente;                     /* la única fuente activa, o -1                  */
    Cx      Zeq;                        /* con una sola fuente: la Z que ve (V/I)        */
    unsigned int buscado;               /* bit k: su valor lo halló la calculadora       */
    unsigned char reg[MAX_EL];          /* Etapa 4: la región de cada no lineal en Q     */
} CaRes;

/* ---- Desde el Circuito ---- */
int    ca_analiza(const Circuito *c, const Red *r, CaRes *res);             /* [=] sin [?!]     */
int    ca_analiza_f(const Circuito *c, const Red *r, double f, CaRes *res); /* a otra f (Bode)  */
/* ¿Se puede analizar? (la línea de estado; no necesita la Red). SIM_OK con
 * la f en *f, o el error en d */
int    ca_valida(const Circuito *c, SimDiag *d, double *f);

/* ---- Sobre una SimRed ---- */
double ca_frecuencia(const SimRed *s, double frec_circ, SimDiag *d);  /* la f común, o 0 con error */
void   ca_excitacion(const SimRed *s, double f, CaExc *x, SimDiag *d);/* avisos; error FORMA   */
void   ca_apaga(SimRed *s, unsigned int activa);  /* J apagadas -> SE_VMI; E reales apagadas -> su r */
int    ca_resuelve_red(const SimRed *s, double f, const CaExc *x, CaRes *res);

/* ---- Capa baja (Bode, Thévenin, calculadora). Un sistema a la vez: la LUC es estática ---- */
int    ca_topologia(const SimRed *s, SimDiag *d);            /* sim_topo_ca, una vez por red  */
/* La generación de la topología de ca.c: cambia con cada ca_topologia (de
 * quien sea). Quien guarda la suya (el Bode) la compara para no rehacerla
 * mientras nadie la cambie; 0 = no se reutiliza (salió mal, o su análisis
 * contó operaciones del emulador, que rehacerla vuelve a contar) */
unsigned ca_topo_gen(void);
int    ca_ensambla(const SimRed *s, double f, SimDiag *d);   /* matriz a f y LUC; RESONANCIA  */
/* Lo mismo, bit a bit, para una red que no cambia (ni ella ni la
 * linealización de ca_pon_q) mientras no cambie la topología: lo que no
 * depende de f se hace una vez por topología y cada f solo estampa las
 * filas de C y L (con una Z, todo; ver ca.c). Solo el Bode */
int    ca_ensambla_fija(const SimRed *s, double f, SimDiag *d);
int    ca_n(void);
const SimTopoCA *ca_topo(void);
void   ca_b_fuentes(const SimRed *s, const CaExc *x, Cx *b);
void   ca_b_inyecta(int na, int nb, Cx I, Cx *b);            /* I entra por na y sale por nb  */
void   ca_sustituye(const Cx *b, Cx *x);                     /* anclas a 0; luc_resuelve       */
void   ca_valores(const SimRed *s, const CaExc *e, const Cx *x, double f, CaRes *res);
void   ca_vi(const SimRed *s, const CaExc *e, const Cx *x, double f, int k, Cx *V, Cx *I);
Cx     ca_phi(const Cx *x, int n);
/* La Z de un elemento pasivo a f (R, C, L, Z); 0 si no lo es */
Cx     ca_z_de(const SimEl *e, double f);

/* ---- Sensibilidades (la calculadora de CA) ----
 * Qué es una incógnita de la calculadora; vive aquí para que calca.h no sea
 * circular. La derivada es respecto a u: el logaritmo en los valores (así un
 * paso es relativo y el valor nunca cambia de signo) y los radianes en los
 * ángulos. */
enum { IN_VAL = 0,        /* R, C, L: ln del valor; Z: ln |Z|           */
       IN_ANGZ,           /* ∠Z en radianes, en [-π/2, π/2]              */
       IN_MODU,           /* ln |E| (|J|) de una fuente alterna          */
       IN_ANGU,           /* ∠E en radianes, envuelto                    */
       IN_F };            /* ln f                                        */
/* v = (dA/du)·x - db/du de la incógnita (k, que); k = -1 con IN_F. Con la
 * misma LU, dx/du = -A⁻¹·v (ca_sustituye). Las filas ancladas valen 0. Con
 * IN_F la Z que tiene p[1] = f no cambia (es la del circuito, a su f); la
 * que tiene otra f0 escala como R+L o R+C. */
void   ca_derivada(const SimRed *s, const CaExc *e, const Cx *x, double f, int k, int que, Cx *v);
/* dV y dI del elemento m (con los convenios de ca_vi): la parte que pasa
 * por dx (= dx/du) y la explícita de la incógnita (k, que): la I de una R o
 * una Z con su valor, la de una fuente real o una J con su U */
void   ca_dvi(const SimRed *s, const CaExc *e, const Cx *x, const Cx *dx, double f,
              int m, int k, int que, Cx *dV, Cx *dI);

/* ---- Pequeña señal (Etapa 4, §3.I) ---- */
/* El punto Q de la red SIN apagar (las J de polarización cuentan) y su
 * linealización en L (de quien llama). SIM_OK, o el error del OP. Con no
 * lineales, el aviso PEQ_SENAL y SATURA por cada AO saturado en Q. */
int  ca_punto_q(const SimRed *s, SimLin *L, SimDiag *d);
/* La linealización que ensambla_en estampa desde ahora (NULL = ninguna): quien
 * ensambla la pone antes de cada ca_ensambla */
void ca_pon_q(const SimLin *L);
const SimLin *ca_lin(void);

/* ---- Thévenin, Norton y ohmímetro ---- */
typedef struct {
    SimDiag d;
    double  f;                          /* 0 = CC                                         */
    int     na, nb;
    unsigned char hay_th, hay_no;       /* Z finita; Z no nula                             */
    unsigned char inf;                  /* CC: R infinita (A y B separados por un C)       */
    unsigned char max_finito;           /* 0 = R_th = 0                                    */
    Cx      Vth, Zth, In;               /* internos (pico y coseno); en CC, reales         */
    Cx      Zl;  double Pmax;           /* carga de máxima transferencia y su P (W)        */
    double  Lc, Cc;                     /* su parte reactiva con una L o una C a f (0 = no) */
    double  Rsola, Psola;               /* si la carga solo puede ser una R                */
} CaThev;
int  ca_thevenin(const Circuito *c, const Red *r, double f, CaThev *t);    /* f = 0: CC   */
int  ca_thevenin_red(const SimRed *s, int na, int nb, double f, CaThev *t);

/* ---- Convenios ---- */
void ca_a_conv(int conv, Cx X, double *mod, double *ang_grados);      /* interno -> se ve  */
Cx   ca_de_conv(int conv, double mod, double ang_grados);             /* se ve -> interno  */
void ca_canon_a_conv(int conv, double ef, double ang, double *mod, double *a);   /* guardado -> se ve */
void ca_conv_a_canon(int conv, double mod, double a, double *ef, double *ang);   /* se ve -> guardado */
/* Texto de un aviso (un bit SIM_AV_*) del análisis de CA */
const char *ca_aviso(const Circuito *c, const CaRes *s, unsigned bit, char *buf, int cap);

unsigned long ca_ops(void); void ca_ops_reset(void);    /* reales equivalentes, luc incluida */
#ifndef NDLESS
void ca_ops_pon(unsigned long v);                        /* el arnés: ca_ops() = v (cerca de la vuelta) */
#endif
/* Lo que cuesta en la calculadora una operación de ca_ops, con lo que no se
 * cuenta (las llamadas de cx.c con sus Cx por valor, los cx_nulo y cx_abs1
 * de la LU compleja): medido en la CX II real con el banco (BN_CA) SIN el
 * cable USB (2026-09-27, la candidata, a 396 MHz), 632 puntos del Bode del
 * peor caso en 4 s, 6,3291 ms cada uno, tras el de 1 kHz (4112 operaciones:
 * el banco solo contaba ese): 3 243 304 operaciones, 5131,8 de media, 1,2333
 * µs cada una, 2,7 veces una multiplicación-resta. En el Bode de un RLC
 * (BN_BODE: 100 ms, 401 evaluaciones de 247,6 operaciones) salen 1,01 µs por
 * operación: el modelo lo estima un 22 % largo. Un fijo por evaluación no
 * cuadra con los dos (daría negativo): va todo por operación. La candidata
 * rehacía la topología en cada punto de bode_en; con la de rev9 (grupo
 * cabode), que la reutiliza en todo el barrido con las mismas operaciones
 * contadas, un punto cuesta ~5 % menos en el PC y el Bode del RLC un 27 %
 * menos de instrucciones: esto queda del lado seguro hasta repetir el banco.
 * El banco del cierre iba con el USB (288 MHz): 1,6938 µs (1,6938/1,2333 =
 * 1,373 ≈ 396/288). Antes, 0,63 µs y 40 µs fijos, a ojo. */
#define CA_NS_OP 1233.3                 /* ns por operación de ca_ops         */

#endif /* CA_H */
