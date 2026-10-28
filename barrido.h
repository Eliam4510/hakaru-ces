/* ============================================================================
 *  barrido.h  -  El barrido de CC (Etapa 4, §3.J): el motor, por trozos
 *
 *  Barre una E, una J, el valor de una R o la ganancia de una controlada en
 *  la vista de CC de [P] (C abiertos, L cables, fuentes en su nivel de CC, S
 *  al empezar). Cada punto sigue al anterior (continuación): la vuelta de un
 *  Schmitt sale sola (histéresis). Un punto que no converge parte el
 *  intervalo; si aún falla, un OP completo (un salto) o un hueco.
 *
 *  Como bode.c: barr_iniciar prepara, barr_avanzar hace puntos hasta gastar
 *  unas operaciones (solo se para entre puntos) y la pantalla (curva.c) lee
 *  el registro. barr_en, barr_nivel y barr_valores dan lecturas exactas en
 *  cualquier x, desde la continuación de la traza en el punto más cercano.
 * ========================================================================= */
#ifndef BARRIDO_H
#define BARRIDO_H

#include "sim.h"

#define BARR_N      281       /* un punto por columna de los 280 px de la gráfica (+1)           */
#define BARR_TRAZAS 6         /* 2 canales de ida + 2 de vuelta, o una familia de 6              */
enum { BARR_CIRCUITO = 0, BARR_CURVA, BARR_FAMILIA };

typedef struct {
    signed char   el;         /* lo que se barre; -1 = automático                                */
    unsigned char log_x;      /* malla logarítmica (por omisión con una R)                       */
    unsigned char vuelta;     /* ida y vuelta                                                    */
    double        x0, x1;     /* 0 y 0 = automático                                              */
} BarrAjustes;

typedef struct {
    int    n, ntr;
    double x[BARR_N];
    double y[BARR_TRAZAS][BARR_N];
    unsigned char ok[BARR_TRAZAS][BARR_N];     /* 0 = hueco                                       */
    unsigned char it[BARR_TRAZAS][BARR_N];     /* iteraciones de Newton                            */
    unsigned char salto[BARR_TRAZAS][BARR_N];  /* 1 = saltó (histéresis) al llegar a este punto   */
    unsigned char recorrido[BARR_TRAZAS];      /* 0 = ida, 1 = vuelta (su fila de continuaciones) */
    unsigned char canal[BARR_TRAZAS];          /* el canal (0 o 1) de cada traza                  */
} BarrReg;

typedef struct {
    SimDiag  d;               /* SIM_OK con avisos, o el error del primer punto                  */
    int      fin, modo, el, bx;
    double   x0, x1;
    unsigned char log_x, vuelta, xy;           /* xy: la curva I-V (V en x, I en y)               */
    char     nom_x[12], uni_x[4];              /* "E1" y "V"; "R1" y Ω                            */
    SimCanal canal[2];
    char     nom_tr[BARR_TRAZAS][12];          /* "I(D1)"; en la familia, "IB=40µ" o "VGS=3"      */
    double   nivel_dato; signed char tr_dato;  /* el dato de V o I que lee [N] al acabar; -1 = no */
    int      huecos, it_max;
    double   kcl_peor;
    double   x_fallo;                          /* el primer x sin convergencia                    */
    double   t_est;                            /* segundos estimados en la calculadora            */
    unsigned gen;
    char     titulo[40];                       /* el trazador: «Curva I-V de D1 (1N4148)»; vacío
                                                  en un circuito (la cabecera nombra los canales) */
    /* La familia (el trazador de un transistor, modo BARR_FAMILIA): una curva
     * por recorrido, cada una con su IB (o su VGS) */
    double   fam[BARR_TRAZAS];
    unsigned char fam_bjt;                     /* 1: la de un BJT (IC frente a VCE); 0: la
                                                  de un MOSFET (ID frente a VDS)          */
    unsigned char fam_early;                   /* tiene Early o modulación del canal (VA):
                                                  r0 finita                               */
    double   fam_vt;                           /* el MOSFET: su Vt (vov = VGS - Vt)       */
} BarrRes;

int  barr_iniciar(const Circuito *c, const Red *r, const BarrAjustes *aj);
/* El componente k solo (el trazador): la curva I-V de un diodo, zener o LED;
 * la familia de 6 curvas IC(VCE) de un BJT o ID(VDS) de un MOSFET */
int  barr_iniciar_curva(const Circuito *c, int k);
int  barr_iniciar_red(const SimRed *s, int el, int bx, const SimCanal ch[2],
                      double x0, double x1, int log_x, int vuelta);          /* el arnés        */
int  barr_avanzar(unsigned long ops);    /* 1 = queda trabajo; solo para entre puntos           */
void barr_cortar(void);
double barr_progreso(void);
const BarrRes *barr_res(void);
const BarrReg *barr_reg(void);
/* Exacto en x (Newton desde el punto de la malla más cercano) y dy/dx exacta. 0 si no
 * converge. */
int  barr_en(int tr, double x, double *y, double *dydx);
/* La región (REG_*) del componente del trazador (el diodo, o el transistor de
 * la familia) en la última lectura de barr_en; REG_NADA en un circuito o si
 * no convergió */
int  barr_region(void);
/* El x de [xa, xb] donde la traza vale nivel: Brent sobre barr_en (1e-14·|x|, 60 como mucho) */
int  barr_nivel(int tr, double nivel, double xa, double xb, double *x);
/* El nivel de un DATO (una magnitud m, §4.8): busca +m y, si no hay cruce, -m. *sg dice cuál */
int  barr_nivel_dato(int tr, double m, double xa, double xb, double *x, int *sg);
/* Todos los valores en x (P_RESULTADO en el cursor) */
int  barr_valores(int tr, double x, Valores *v, SimDiag *d);
double barr_residuo(void);                /* el residuo de Kirchhoff de los últimos barr_valores */
/* El circuito barrido, con el valor de lo barrido como dato (el del último barr_valores) */
const Circuito *barr_circuito(void);

unsigned long barr_ops(void);
/* La barra de P_EMULANDO y el corte: los puntos hechos y el x del último */
int    barr_hechos(void);
double barr_x_hecho(void);
/* El rango automático de lo que se barre (§3.J): una E o J de valor v, [-|v|, |v|];
 * una R, [v/100, 100·v] logarítmico; una ganancia k, [0, 2k] ([2k, 0] con k < 0) */
void   barr_rango_auto(const Circuito *c, int el, double *x0, double *x1, int *log_x);

#endif /* BARRIDO_H */
