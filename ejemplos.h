/* ============================================================================
 *  ejemplos.h  -  Los ejemplos resueltos
 *
 *  Cada ejemplo es un circuito dibujado en la rejilla con sus datos e
 *  incógnitas, más un enunciado y una nota de qué enseña. Sirven tres veces:
 *  en "Aprende" como ejemplos explicados (el procedimiento paso a paso lo
 *  escribe el motor), en el Taller como plantilla para modificar, y en el
 *  arnés como clave de respuestas: cada uno lleva la suya.
 *
 *  Los nueve primeros son los del pizarrón (circuitos_pizarron.pdf), en su
 *  orden a) ... i).
 * ========================================================================= */
#ifndef EJEMPLOS_H
#define EJEMPLOS_H

#include "circuito.h"

enum {
    EJ_PA = 0, EJ_PB, EJ_PC, EJ_PD, EJ_PE, EJ_PF, EJ_PG, EJ_PH, EJ_PI,
    EJ_OHM_I, EJ_OHM_V, EJ_OHM_R, EJ_OHM_K,
    EJ_S_DOS, EJ_S_TRES, EJ_S_DIV, EJ_S_DATO, EJ_S_R,
    EJ_P_DOS, EJ_P_TRES, EJ_P_DIV, EJ_P_R,
    EJ_M_SP, EJ_M_RAMA, EJ_M_ESC,
    EJ_D_OTRA, EJ_D_FUENTE, EJ_D_DOS,
    EJ_W_SERIE, EJ_W_PAR,
    EJ_K_PUENTE, EJ_K_EQUIL, EJ_K_DOS,
    EJ_J_SOLA, EJ_J_SERIE, EJ_J_HALLAR, EJ_I_VOLT, EJ_I_AMP, EJ_F_REAL, EJ_F_R,
    EJ_N
};

typedef struct {
    const char *titulo;       /* para la lista                            */
    const char *enunciado;    /* datos e incógnitas, como en clase        */
    const char *nota;         /* qué se aprende con él                    */
    void (*arma)(Circuito *c);
    const char *clave_el;     /* elemento de la respuesta: "R3", "E1"     */
    char        clave_q;      /* 'X' su R, E o J, 'V' su tensión, 'I' su
                                 corriente, 'r' la r interna de una fuente */
    double      clave;        /* la respuesta, en magnitud                */
} Ejemplo;

extern const Ejemplo EJEMPLOS[];
extern const int     N_EJEMPLOS;

void ejemplo_cargar(int i, Circuito *c);

#endif /* EJEMPLOS_H */
