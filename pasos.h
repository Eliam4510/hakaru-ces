/* ============================================================================
 *  pasos.h  -  Procedimiento paso a paso
 *
 *  Copiado de EstaticaCX (commit eca703d). Cada paso gana una MASCARA de
 *  elementos: la pantalla de pasos resalta en el esquema los elementos que
 *  usa el paso, asi que el procedimiento es a la vez la demostracion y no hay
 *  que escribir ninguna a mano.
 * ========================================================================= */
#ifndef PASOS_H
#define PASOS_H

#define PASO_MAX 128   /* la propagacion da un paso por regla */
#define PASO_TIT  50
#define PASO_TXT  320   /* seis lineas de 52 columnas, y algo de margen */

typedef struct {
    char titulo[PASO_TIT];
    char detalle[PASO_TXT];
    char resultado[PASO_TXT];
    unsigned int resalta;     /* bit k = elemento k del circuito */
    int nodo;                 /* el nodo del paso (Kirchhoff: «Nodo B»); -1 = ninguno */
} Paso;

typedef struct {
    Paso items[PASO_MAX];
    int  n;
    int  desborde;
} Pasos;

void pasos_init(Pasos *p);

/* Anade un paso ya formado. detalle y resultado pueden ser NULL. */
void pasos_add(Pasos *p, const char *titulo, const char *detalle,
               const char *resultado, unsigned int resalta);

/* Reserva el siguiente paso y lo devuelve para escribir detalle/resultado
 * directamente con un Buf, sin cadenas temporales. NULL si ya no cabe. */
Paso *pasos_nuevo(Pasos *p, const char *titulo, unsigned int resalta);

#endif /* PASOS_H */
