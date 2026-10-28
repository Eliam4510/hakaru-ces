/* ============================================================================
 *  lecciones.h  -  "Aprende": teoría con sus ejemplos resueltos
 *
 *  Cada lección son unas pocas páginas de teoría (texto y, si ayuda, una
 *  figura) seguidas de sus ejemplos. Los ejemplos son los de ejemplos.c: el
 *  procedimiento paso a paso lo escribe el motor, verificado, así que no hay
 *  ninguna solución escrita a mano que pueda estar mal.
 * ========================================================================= */
#ifndef LECCIONES_H
#define LECCIONES_H

enum {
    FIG_NADA = 0,
    FIG_TRIANGULO,     /* V arriba, I y R abajo                         */
    FIG_POTENCIA,      /* P arriba, V e I abajo                         */
    FIG_NODO,          /* un nodo con corrientes que entran y salen     */
    FIG_CIRCUITO       /* el circuito de un ejemplo (fig_ej)            */
};

#define FIG_ALTO 84    /* alto reservado a la figura, en píxeles        */

typedef struct {
    const char   *texto;     /* con \n; se corta por palabras a 52 columnas */
    unsigned char fig;
    signed char   fig_ej;    /* ejemplo que se dibuja con FIG_CIRCUITO      */
} Pagina;

typedef struct {
    const char   *titulo;
    const char   *resumen;   /* una línea para la lista                    */
    const Pagina *pag;
    int           npag;
    const signed char *ej;   /* índices de EJEMPLOS                         */
    int           nej;
    const signed char *mod;  /* índices de MODELOS (biblioteca.h): se emulan */
    int           nmod;
} Leccion;

extern const Leccion LECCIONES[];
extern const int     N_LECCIONES;

#endif /* LECCIONES_H */
