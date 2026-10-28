/* ============================================================================
 *  proced.h  -  El procedimiento, como en la pizarra
 *
 *  Con una sola fuente y el resto serie-paralelo (todos los problemas del
 *  pizarron), el procedimiento sigue el arbol de arbol.c:
 *
 *    1. estructura: Req = R1 + (R2 || R3)
 *    2. reducir: cada serie se suma, cada paralelo por la formula que toque
 *    3. ley de Ohm con la fuente: I = E/Req, o E = I*Req, o Req = E/I
 *    4. (inverso) despejar la R desconocida bajando por el arbol
 *    5. repartir: en paralelo la misma tension, en serie la misma corriente,
 *       con la suma comprobada en cada reparto
 *    6. potencia y balance
 *
 *  Todos los numeros de estos pasos salen del ARBOL, no del motor. Asi son
 *  un segundo camino independiente: el arnes exige que coincidan (ValArbol).
 *
 *  Si el circuito no es serie-paralelo (un puente, varias fuentes) o los
 *  datos no encajan en ese esquema, el procedimiento son las leyes de
 *  Kirchhoff escritas con los numeros y la solucion del motor.
 * ========================================================================= */
#ifndef PROCED_H
#define PROCED_H

#include "circuito.h"
#include "red.h"
#include "resuelve.h"
#include "pasos.h"

enum { PROC_ARBOL = 0, PROC_KIRCHHOFF, PROC_NADA };

typedef struct {
    int          ok;               /* 1 si el arbol calculo todo el circuito */
    unsigned int tiene;            /* elementos con V, I y R calculados      */
    double       V[MAX_EL], I[MAX_EL], R[MAX_EL];   /* en magnitud           */
    double       E, Itot, req;
} ValArbol;

/* Escribe el procedimiento de la solucion isol. va puede ser NULL.
 * Devuelve el metodo usado. */
int proced_generar(const Circuito *c, const Red *r, const Solucion *s,
                   int isol, Pasos *p, ValArbol *va);

#endif /* PROCED_H */
