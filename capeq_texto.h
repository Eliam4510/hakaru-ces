/* ============================================================================
 *  capeq_texto.h  -  La Teoría y los Ejemplos de «Capacitores equivalentes»
 *
 *  Taller > «Capacitores equivalentes» > Teoría / Ejemplos (especificación
 *  capacitores/ESPEC_CAPEQ.md, §2). Solo contenido: cadenas en Latin-1 (las
 *  macros S_* de gfx.h), sin estado y sin depender de nada más.
 *
 *  Teoría: páginas con un título de 40 columnas como mucho y un texto ya
 *  cortado en líneas de 52 columnas como mucho, separadas por '\n'. Una
 *  página puede ser más larga que la pantalla: la app la desplaza.
 *
 *  Ejemplos: cada uno con lo que se escribiría en los campos de los datos
 *  (valor, tolerancia, «Capacitores» y bodega). Al elegir uno, la app carga
 *  esos datos y abre su resultado. Lo que dice «ensena» lo comprueba una
 *  fuerza bruta propia (capacitores/contenido/fuerza_ejemplos.py).
 * ========================================================================= */
#ifndef CAPEQ_TEXTO_H
#define CAPEQ_TEXTO_H

/* ---- Teoría ------------------------------------------------------------- */
int cpq_teoria_n(void);                 /* número de páginas                   */
const char *cpq_teoria_titulo(int p);   /* <= 40 columnas; "" si p no existe   */
const char *cpq_teoria_texto(int p);    /* líneas <= 52 columnas con '\n'; ""
                                           si p no existe                      */

/* ---- Ejemplos ----------------------------------------------------------- */
typedef struct {
    const char *titulo;     /* <= 40 columnas                                   */
    const char *ensena;     /* qué enseña: 1 o 2 líneas de <= 52, con '\n'      */
    const char *valor;      /* como en el campo: "22.5"                         */
    const char *tol;        /* como en el campo: "0.5"; "" = exacto             */
    int n;                  /* «Capacitores»: 0 = Auto, 2 o 3                   */
    const char *bodega;     /* como en el campo: "15-42 sin 21-24"              */
} CpqEjemplo;

int cpq_ejemplos_n(void);
/* Nunca devuelve 0: si i no existe, uno vacío (todo "", n = 0) */
const CpqEjemplo *cpq_ejemplo(int i);

#endif /* CAPEQ_TEXTO_H */
