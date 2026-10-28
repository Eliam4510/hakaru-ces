/* ============================================================================
 *  bode.h  -  El trazador de Bode: barrido de frecuencia y sus medidas
 *
 *  H(f) = salida / entrada, con la entrada (una E o una J) a 1∠0 y las demás
 *  fuentes apagadas; la salida es una sonda, V(AB) o el canal automático. Una
 *  factorización por frecuencia (ca_ensambla) y una sustitución.
 *
 *  La malla es de 277 frecuencias logarítmicas (una por columna de la
 *  gráfica). El rango automático sale de un barrido grueso de 49 puntos (1 mHz
 *  a 1 GHz, 4 por década): donde la fase cambia o el dB se curva, más una
 *  década a cada lado. Las medidas (cortes a -3 dB, pico, valle, fase 0) se
 *  afinan con el motor, no se interpolan: sección áurea para el pico y Brent
 *  para los cruces.
 *
 *  Por trozos, como el emulador: bode_avanzar gasta unas operaciones y solo
 *  se para entre evaluaciones completas; el resultado no depende de cómo se
 *  trocee.
 * ========================================================================= */
#ifndef BODE_H
#define BODE_H

#include "ca.h"

#define BODE_N    277                  /* un punto por columna de 276 px (+1)           */
#define BODE_PRE  49                   /* 1 mHz..1 GHz, 4 por década (autorrango)        */
enum { BODE_OTRO = 0, BODE_PLANO, BODE_PB, BODE_PA, BODE_PBANDA, BODE_RB,
       BODE_NULA };                    /* la salida no depende de la entrada (|H| ≤ 1e-12) */
enum { BS_AUTO = 0, BS_SONDA1, BS_SONDA2, BS_AB };

typedef struct {
    signed char ent;                   /* fuente de entrada (índice), o -1 = automática  */
    unsigned char sal;                 /* BS_*                                           */
    double f_ini, f_fin;               /* 0 = automático                                 */
} BodeAjustes;

typedef struct {
    int    n;
    double f[BODE_N];
    Cx     H[BODE_N];
    double db[BODE_N], fase[BODE_N];   /* fase en grados, desenrollada desde el primero  */
    unsigned char ok[BODE_N];          /* 0 = singular (resonancia ideal justo ahí)      */
} BodeReg;                             /* ~11 KB */

typedef struct {
    int    tipo;                       /* BODE_*                                         */
    double M0, Minf;                   /* |H| en f_ini/1e4 y en 1e4·f_fin (los límites)  */
    double fc;                         /* PB: el corte -3 dB de M0; PA: de Minf; 0 = no  */
    double f_pico, db_pico;            /* pico interior afinado; 0 = no hay              */
    double f_valle, db_valle;          /* valle interior afinado; 0 = no hay             */
    double f1, f2, bw, q;              /* PBANDA (respecto al pico) y RB                 */
    double f_fase0;                    /* PBANDA: H real más cerca del pico; 0 = no hay  */
    double pend_ini, pend_fin;         /* dB por década en los extremos                  */
} BodeMedidas;

typedef struct {
    SimDiag d;
    int     fin;                       /* FIN_* como el emulador                          */
    double  f_ini, f_fin;
    unsigned char auto_f;
    char    unidad[4];                 /* "", "S" o S_OHM                                */
    char    nom_ent[8], nom_sal[12];   /* "E1", "V(C1)"                                  */
    int     ent;  SimCanal sal;        /* los que se usaron                              */
    BodeMedidas m;
    int     evaluaciones;
    double  t_est;                     /* segundos estimados en la calculadora            */
    unsigned gen;                      /* cambia en cada barrido                          */
} BodeRes;

int  bode_iniciar(const Circuito *c, const Red *r, const BodeAjustes *aj);
/* Sobre una SimRed (el arnés): la entrada ent, la salida sal (o V(na) - V(nb)
 * si sal->tipo es CANAL_NADA y na >= 0); f_ini = f_fin = 0 es automático */
int  bode_iniciar_red(const SimRed *s, int ent, const SimCanal *sal, int na, int nb,
                      double f_ini, double f_fin, double frec_circ);
int  bode_avanzar(unsigned long ops);      /* 1 = queda trabajo; solo para entre evaluaciones */
void bode_cortar(void);                    /* FIN_CORTADO; lo hecho vale                     */
double bode_progreso(void);
const BodeRes *bode_res(void);
const BodeReg *bode_reg(void);
int  bode_en(double f, Cx *H);             /* H exacta en f con la red del último barrido    */
void bode_medidas(const BodeReg *g, int (*H)(double, Cx *), BodeMedidas *m);   /* pura: el arnés */

#endif /* BODE_H */
