/* ============================================================================
 *  biblioteca.h  -  Circuitos modelo (lecciones 10 a 13 de Aprende)
 *
 *  Doce circuitos de libro que cambian con el tiempo: cargas y descargas,
 *  los tres regímenes del RLC, el LC que oscila, el integrador y el
 *  derivador con una cuadrada, la patada inductiva, un interruptor que se
 *  cierra y se abre y un RC con senoidal medido con un voltímetro en CA.
 *  Diez de la alterna (lección 11), diez de diodos (lección 12) y los del
 *  AO y las fuentes controladas (lección 13).
 *  Cada uno trae una CLAVE: un número que el emulador tiene que dar (con
 *  su tolerancia) y que el arnés comprueba contra la solución exacta.
 * ========================================================================= */
#ifndef BIBLIOTECA_H
#define BIBLIOTECA_H

#include "circuito.h"

typedef struct {
    const char *titulo;        /* <= 44 columnas                            */
    const char *enunciado;     /* <= 3 líneas de 52                         */
    const char *que_ver;       /* <= 2 líneas de 52: qué mirar en la pantalla */
    void (*arma)(Circuito *c);
    unsigned char canal;       /* 1 o 2: el canal de la clave               */
    char medida;               /* 'v' valor en t; 'M' máx; 'm' mín; 'P' p-p y
                                  'A' media del último periodo; 'F' frecuencia;
                                  'L' lectura de clave_el; 'W' energía final;
                                  'U' máx y 'u' mín del ÚLTIMO periodo      */
    const char *clave_el;      /* 'L', 'W', 'I', 'V': la etiqueta del elemento */
    double t, clave, tol;      /* t de 'v' (y de 'x', 'y', 'g'); tol relativa */
    /* Etapa 3 (0 en los doce de antes) */
    unsigned char ca;          /* 0 = se emula en el tiempo; 1 = CA (fasores
                                  o la calculadora de CA); 2 = el Bode;
                                  3 = punto de operación de Newton (CC);
                                  4 = barrido de CC                        */
    /* medidas de la CA: 'V' |V| eficaz de clave_el; 'I' |I| eficaz; 'p' fp
     * de un W; 'L' la lectura de clave_el (en un W, su P); 'f' el corte (fc,
     * o f1 en un pasa-banda); 'Q' la Q; 'c' el valor que halla la
     * calculadora en clave_el; 'T' la P máxima de Thévenin.
     * Del punto de operación (ca = 3): 'I' |I| de clave_el; 'V' su V con
     * signo; 'L' lo que marca clave_el (un voltímetro, su V; un
     * amperímetro, su I).
     * Del barrido (ca = 4): 'x' el x donde el canal 1 vale t; 'y' el canal 1
     * en x = t; 'g' dy/dx del canal 1 en x = t. */
    /* Etapa 4 (0 en los 22 de antes) */
    const char   *barre;       /* ca = 4: la etiqueta de lo que se barre ("E1", "R1") */
    double        x0, x1;      /* su rango                                  */
    unsigned char log_x;       /* malla logarítmica                         */
} CircModelo;

extern const CircModelo MODELOS[];
extern const int N_MODELOS;

void modelo_cargar(int i, Circuito *c);

#endif /* BIBLIOTECA_H */
