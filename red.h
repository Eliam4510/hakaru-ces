/* ============================================================================
 *  red.h  -  Del dibujo a la red electrica
 *
 *  Union-find sobre los puntos de la rejilla: un cable une sus dos puntos.
 *  Cada conjunto que toca un borne de elemento es un NODO electrico. Los
 *  nodos se nombran A, B, C... en orden de lectura (arriba-izquierda primero;
 *  con terminales, A y B son los suyos: red_letra).
 *
 *  El motor (resuelve.c), el arbol serie-paralelo (arbol.c) y el
 *  procedimiento solo ven la Red y los valores de los elementos: no la
 *  rejilla. Por eso el arnes puede construir redes aleatorias a mano sin
 *  dibujarlas.
 * ========================================================================= */
#ifndef RED_H
#define RED_H

#include "circuito.h"

#define MAX_NODOS REJ_PUNTOS   /* 70: un nodo tiene al menos un punto (con 3 patas caben 65) */

/* Avisos y errores de la extraccion */
enum {
    RED_OK = 0,
    RED_VACIA,          /* no hay elementos                               */
    RED_SIN_FUENTE,     /* no hay ninguna fuente                          */
    RED_FUENTE_CORTO,   /* una fuente sin r con sus dos bornes en el mismo nodo */
    RED_J_ABIERTA,      /* una fuente de corriente sin camino de vuelta   */
    RED_FUENTE_AMP      /* un amperímetro ideal cortocircuita una fuente sin r */
};

typedef struct {
    int nn;                        /* numero de nodos                      */
    int nel;
    int a[MAX_EL], b[MAX_EL];      /* nodo de cada borne                   */
    int c[MAX_EL];                 /* nodo de la pata c (Etapa 4); -1 en los de dos bornes */
    int comp[MAX_NODOS];           /* subcircuito al que pertenece el nodo */
    int ref[MAX_NODOS];            /* 1 si es la referencia (0 V) de su subcircuito */
    int ncomp;
    int punto[REJ_PUNTOS];         /* nodo de cada punto de la rejilla, -1 */
    int error;                     /* RED_*                                */
    int elem_error;                /* elemento que causa el error, -1      */
    int elem_culpa;                /* RED_FUENTE_AMP: el amperímetro, la L o
                                      el interruptor que hace de cable; -1 */
    /* Las letras con terminales (red_letra): el nodo de cada uno (-1 si no
     * está puesto o no cae en un nodo) y las letras que se reservan (bit 0,
     * la A, si A está puesto; bit 1, la B). red_cierra los deja a -1 y 0 (las
     * letras de siempre); red_extraer los pone. */
    int nodo_term[2];
    int letras_term;
} Red;

/* Extrae la red del dibujo. Devuelve red->error. nn, a, b, comp, ref y
 * punto se rellenan SIEMPRE, también con error: el emulador los usa (solo
 * se fija en RED_VACIA; un circuito sin fuentes se puede emular). */
int  red_extraer(const Circuito *c, Red *r);

/* Completa comp[], ref[] y ncomp a partir de nn, nel, a[] y b[]. La usa
 * red_extraer y la usa el arnes con redes armadas a mano. La referencia de
 * cada subcircuito es el borne - de su primera fuente, o su primer nodo. */
void red_cierra(const Circuito *c, Red *r);

/* Grado del nodo: cuantos bornes de elemento llegan a el. */
int  red_grado(const Red *r, int n);

/* La referencia (0 V) del subcircuito del nodo n: la de red_cierra (ref[m] &&
 * comp[m] == comp[n]); -1 si n < 0. La salida de un AO se mide desde ahí. */
int  red_ref_de(const Red *r, int n);
/* 1 si esa referencia es de verdad: la tierra está en el subcircuito de n, o el
 * - de una fuente independiente (tipo_info()->fuente). 0 si red_cierra tomó
 * «su primer nodo». */
int  red_ref_propia(const Circuito *c, const Red *r, int n);

/* La letra del nodo n: la del puesto red_puesto(r, n) en A, B, C... (y
 * a, b... a partir del 26). Sin terminales, el puesto es n (orden de
 * lectura). Con terminales, A y B son las de los nodos de A y de B (las
 * marcas amarillas y V(AB) = VA - VB) y los demás nodos siguen, en su
 * orden, con las letras que quedan libres: la A o la B de un terminal que
 * no está y luego C, D... Así ninguna letra de nodo repite la de un
 * terminal puesto en otro sitio. Con r == NULL, la de siempre. */
char red_letra(const Red *r, int n);
int  red_puesto(const Red *r, int n);
/* El nodo del puesto p (el inverso: para listar los nodos en orden
 * alfabético); -1 si ninguno lo tiene (la B de un terminal en el nodo de A) */
int  red_nodo_puesto(const Red *r, int p);

#endif /* RED_H */
