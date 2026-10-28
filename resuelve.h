/* ============================================================================
 *  resuelve.h  -  El motor: cualquier red de fuentes y resistencias
 *
 *  La forma del circuito no importa: serie, paralelo, una fuente en medio de
 *  las ramas, puentes, varias fuentes. Se plantea el sistema de Kirchhoff
 *  completo (analisis por tabla) y se resuelve con Gauss:
 *
 *    incognitas:  potencial de cada nodo (menos la referencia, a 0 V)
 *                 corriente de cada elemento
 *    ecuaciones:  ley de nodos en cada nodo (menos la referencia)
 *                 ley de Ohm de cada resistencia con R conocida
 *                 ecuacion de cada fuente con E conocida
 *                 una por cada dato de V o de I
 *
 *  La observacion que hace lineales los problemas INVERSOS: si la R de un
 *  elemento es desconocida, su ley de Ohm simplemente no se escribe. Su V y
 *  su I siguen siendo incognitas del sistema, el dato del enunciado ocupa el
 *  lugar de esa ecuacion, y al final R = V / I. Lo mismo con una E
 *  desconocida. De ahi la regla que se ensena:
 *
 *      hacen falta tantos datos de V o I como valores desconocidos haya.
 *
 *  Los datos van en magnitud. Con m datos hay 2^m combinaciones de signo;
 *  se resuelven todas y se quedan las fisicas (toda R desconocida > 0, y de
 *  preferencia toda E desconocida >= 0 con la polaridad dibujada). Si quedan
 *  varias, se muestran todas: el programa no elige en silencio.
 * ========================================================================= */
#ifndef RESUELVE_H
#define RESUELVE_H

#include <stddef.h>
#include "circuito.h"
#include "red.h"

#define MAX_SOL    4
#define MAX_DATOS  8     /* 2^8 = 256 combinaciones de signo, una eliminacion */

enum {
    SOL_OK = 0,
    SOL_ERR_RED,       /* ver Red.error                                     */
    SOL_FALTAN,        /* menos datos que valores desconocidos              */
    SOL_INDET,         /* hay datos suficientes pero alguno no aporta nada  */
    SOL_SIN_CORRIENTE, /* R desconocida en un elemento sin corriente        */
    SOL_INCOMPAT,      /* los datos se contradicen                          */
    SOL_NO_FISICA,     /* solo hay solucion con una R negativa              */
    SOL_MUCHOS_DATOS,  /* mas de MAX_DATOS datos medidos                    */
    SOL_R_INVALIDA,    /* una R dada no es positiva                         */
    SOL_R_CERO,        /* una R desconocida tendria que valer 0             */
    SOL_FORMA,         /* una fuente no continua con [?!]: la calculadora es de CC */
    SOL_CA,            /* un circuito de alterna: lo resuelven los fasores (ca.c) */
    SOL_SOLO_CA,       /* algo que solo es de la alterna: el valor de C o L con
                          [?!], un ángulo, o P o fp de un vatímetro         */
    /* Etapa 4 */
    SOL_NO_LINEAL,     /* un D, zener, LED, AO, BJT o MOS: la tabla es lineal */
    SOL_CTL,           /* una fuente controlada sin control                  */
    SOL_SIM            /* solo la interfaz: el punto de operación dio un error */
};

typedef struct {
    double val[MAX_EL];     /* R de cada resistencia, E de cada fuente     */
    double V[MAX_EL];       /* resistencia: phi(a)-phi(b); fuente: E en bornes */
    double I[MAX_EL];       /* corriente de a hacia b                       */
    double P[MAX_EL];       /* disipada (resistencia) o entregada (fuente)  */
    double aux[MAX_EL];     /* r interna de una fuente real (0 si ideal)    */
    double phi[MAX_NODOS];
    /* Etapa 4 (0 en la tabla y en los de dos bornes) */
    double V2[MAX_EL], I2[MAX_EL];     /* el 2.º puerto: VBE e IB, VGS, Vd e I(+) del AO */
    unsigned char reg[MAX_EL];         /* REG_* de cada no lineal                        */
} Valores;

typedef struct {
    int     estado;          /* SOL_*                                       */
    int     elem;            /* elemento al que se refiere el mensaje, -1   */
    int     ndesc, ndatos;   /* valores desconocidos y datos medidos        */
    int     nsol;            /* soluciones fisicas distintas (1..MAX_SOL)   */
    Valores s[MAX_SOL];
    int     fuente;          /* indice de la unica fuente, o -1             */
    int     hay_req;         /* Req = E / I de la unica fuente              */
    double  req;
    int     motivo;          /* SOL_CTL: CTL_* de circ_ctl_motivo (0 en lo demás) */
} Solucion;

int  resolver(const Circuito *c, const Red *r, Solucion *s);

/* Valores desconocidos (R, E, J, R de un instrumento no ideal, r buscada):
 * la regla es tantos datos de V o I como estos. La usan el motor y la línea
 * de estado del editor. En *primero (si no es 0), el primer elemento. */
int  sol_desconocidos(const Circuito *c, int *primero);

/* El texto de un control que no vale (SOL_CTL y SIM_CTL_MAL dicen lo mismo):
 * nom = la controlada, nom_ctl = el que la controla (NULL si no hay) */
const char *sol_texto_ctl(int motivo, const char *nom, const char *nom_ctl, char *buf, size_t cap);

/* Mensaje en espanol del estado (con glifos de la fuente 6x8). */
const char *sol_mensaje(const Circuito *c, const Red *r, const Solucion *s,
                        char *buf, size_t cap);

/* Comprobacion independiente del motor: con los valores dados, mayor
 * residuo relativo de la ley de nodos, la de Ohm, las fuentes y los datos
 * (en magnitud). Cero si todo cuadra. */
double sol_residuo(const Circuito *c, const Red *r, const Valores *v);

/* V(AB) entre los terminales A y B: phi(A) - phi(B). Devuelve 1 si se puede
 * (con los nodos en *na y *nb), 0 si A o B no están en un punto del
 * circuito (o no se han puesto) y -1 si están en circuitos separados. */
int sol_vab(const Circuito *c, const Red *r, const Valores *v, double *vab, int *na, int *nb);

/* La solución i como datos: los R, E, J, r y R de instrumento hallados pasan
 * a dato (para Thévenin desde el resultado). *out puede ser c. */
void sol_aplica(const Circuito *c, const Solucion *s, int i, Circuito *out);

/* La potencia ABSORBIDA por el elemento k, con el convenio pasivo del libro
 * (la columna «P abs» de la tabla de CC): positiva si absorbe y negativa si
 * entrega. En las fuentes y las controladas es -P (su P es la entregada);
 * en un vatímetro, 0 (lo que marca pasa hacia la carga: él no lo absorbe).
 * En una fuente real es la de bornes, no la de la fem (E·I). Vale también
 * para los Valores del punto de operación (la tabla con Newton): el AO
 * entrega (-P) y el BJT y el MOSFET absorben por sus dos puertos (P). */
double sol_p_abs(const Circuito *c, const Valores *v, int k);
/* El balance de la tabla: en *ent lo que entregan (la suma de las P abs
 * negativas, en positivo) y en *absorb lo que absorben. Devuelve |ΣP abs| /
 * Σ|P abs| (0 si todas son 0): Tellegen dice que es 0. */
double sol_balance(const Circuito *c, const Valores *v, double *ent, double *absorb);

#endif /* RESUELVE_H */
