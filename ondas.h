/* ============================================================================
 *  ondas.h  -  Formas de onda de las fuentes (el generador de funciones)
 *
 *  Una fuente de tensión o de corriente puede ser continua o variar en el
 *  tiempo. Los parámetros van en p[0..4], con este significado por forma:
 *
 *    ONDA_CC          p0 = valor
 *    ONDA_SENO        p0 = amplitud A (pico), p1 = f (Hz), p2 = fase (grados),
 *                     p3 = nivel de CC:    nivel + A·sen(2πft + fase)
 *    ONDA_CUADRADA    p0 = A, p1 = f, p2 = ciclo de trabajo (%), p3 = nivel:
 *                     nivel + A en [0, ciclo·T) y nivel - A en [ciclo·T, T)
 *    ONDA_TRIANGULAR  p0 = A, p1 = f, p2 = ciclo (%), p3 = nivel: sube en
 *                     línea recta de nivel - A a nivel + A en [0, ciclo·T) y
 *                     baja en [ciclo·T, T) (ciclo 50 = simétrica, 100 = diente
 *                     de sierra)
 *    ONDA_PULSO       p0 = V1, p1 = V2, p2 = retardo td, p3 = ancho,
 *                     p4 = periodo: V1 antes de td, V2 en [td, td + ancho) y
 *                     luego V1; con periodo > 0 se repite cada periodo desde
 *                     td; con ancho = 0 es un escalón (V2 para siempre)
 *
 *  Es la misma definición que usa el oráculo independiente
 *  (herramientas/oraculo_sim.py): el emulador y el oráculo se comparan.
 *  Los intervalos son semiabiertos: en el instante de un flanco ya vale el
 *  valor nuevo.
 * ========================================================================= */
#ifndef ONDAS_H
#define ONDAS_H

enum { ONDA_CC = 0, ONDA_SENO, ONDA_CUADRADA, ONDA_TRIANGULAR, ONDA_PULSO, ONDA_N };

#define ONDA_PAR 5

/* Valor en el instante t (t >= 0) */
double onda_valor(int forma, const double *p, double t);

/* Valor antes de t = 0: el REPOSO (el generador se enciende en t = 0). CC:
 * p0; SENO: p3 (su nivel de CC); CUADRADA y TRIANGULAR: p3 - p0 (el nivel
 * bajo); PULSO: p0. Es lo que ve el punto de operación. */
double onda_reposo(int forma, const double *p);

/* Como onda_valor, pero con lado = -1 da el límite por la IZQUIERDA: en un
 * flanco (o a menos de la tolerancia de él), el valor de antes; con t <= 0,
 * el reposo. lado = +1 es onda_valor. */
double onda_valor_lado(int forma, const double *p, double t, int lado);

/* Siguiente instante > t en que la onda salta o cambia de pendiente (un
 * flanco de la cuadrada o del pulso, un vértice de la triangular). Un valor
 * muy grande (1e300) si ya no hay ninguno. La senoidal y la continua no
 * tienen: son suaves. */
double onda_evento(int forma, const double *p, double t);

/* Periodo de la onda (0 si no es periódica): sirve para elegir el paso y la
 * ventana del osciloscopio. */
double onda_periodo(int forma, const double *p);

/* El nivel de CC de la onda (el punto de operación de [P], el punto Q y el
 * barrido, Etapa 4): continua p0; senoidal p3; cuadrada p3 + p0·(p2/50 - 1)
 * (su media); triangular p3; pulso p0 (V1, el de reposo) */
double onda_nivel_cc(int forma, const double *p);

/* Nombre para la ficha: "continua", "senoidal"... */
const char *onda_nombre(int forma);

#endif /* ONDAS_H */
