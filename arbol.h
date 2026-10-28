/* ============================================================================
 *  arbol.h  -  Estructura serie-paralelo vista desde la fuente
 *
 *  Es como lo ve el profesor en la pizarra: "la rama izquierda son tres de
 *  3 ohmios en serie, en paralelo con la de la derecha". Se obtiene
 *  reduciendo la red de resistencias entre los dos bornes de la fuente:
 *
 *    - dos ramas entre los mismos dos nodos        -> PARALELO
 *    - un nodo interior al que solo llegan dos     -> SERIE
 *    - un nodo interior al que solo llega una      -> colgante, sin corriente
 *    - una resistencia con los dos extremos unidos -> en corto, sin corriente
 *
 *  hasta que queda una sola rama entre los bornes (la raiz del arbol) o ya
 *  no se puede reducir (un puente, por ejemplo: no es serie-paralelo).
 *
 *  El arbol NO se usa para calcular los resultados: eso lo hace resuelve.c
 *  con cualquier forma de circuito. Se usa para EXPLICAR, y como segundo
 *  camino independiente que el arnes compara con el motor.
 * ========================================================================= */
#ifndef ARBOL_H
#define ARBOL_H

#include "circuito.h"
#include "red.h"

#define ARB_MAX (2 * MAX_EL)

/* AR_MUERTO: nodo absorbido al aplanar (una serie dentro de otra serie). Se
 * queda en el array pero ya no pertenece al arbol. */
enum { AR_HOJA = 0, AR_SERIE, AR_PARALELO, AR_MUERTO };

typedef struct {
    unsigned char tipo;
    signed char   el;              /* elemento, si es hoja                  */
    unsigned char nh;
    unsigned char h[MAX_EL];       /* hijos; en SERIE, en orden de + a -    */
    unsigned int  mascara;         /* elementos que contiene                */
    char          nombre[8];       /* "R2", "R234", "Ra", "Req"             */
} NodoAr;

typedef struct {
    NodoAr n[ARB_MAX];
    int    nn;
    int    raiz;                   /* -1 si la red no es serie-paralelo      */
    int    fuente;                 /* la unica fuente, o -1                  */
    unsigned int corto;            /* resistencias en cortocircuito          */
    unsigned int colgante;         /* resistencias sin camino de vuelta      */
} Arbol;

/* 1 si el circuito tiene una sola fuente y lo demas es serie-paralelo. */
int arbol_construir(const Circuito *c, const Red *r, Arbol *a);

/* Resistencia equivalente del nodo con los valores R[] de las hojas.
 * NaN si alguna hoja interior es desconocida (R[k] no es un numero). */
double arbol_R(const Arbol *a, int nodo, const double *R);

/* Expresion del nodo con nombres de elementos: "R1 + (R2 || R3)". */
void arbol_expresion(const Arbol *a, const Circuito *c, int nodo,
                     char *buf, int cap);

#endif /* ARBOL_H */
