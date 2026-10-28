/* ============================================================================
 *  calca.h  -  La calculadora de CA: despejar con fasores
 *
 *  La calculadora de CC (resuelve.c) es lineal: con una R desconocida su ley
 *  de Ohm no se escribe y el dato ocupa su sitio. En alterna eso ya no vale:
 *  una L o una f desconocidas entran en la matriz como jωL, y un dato de
 *  módulo (lo que marca un voltímetro) no es lineal. Así que aquí se hace lo
 *  que haría un alumno con paciencia: se prueba el circuito DIRECTO (ca.c)
 *  con valores de las incógnitas y se corrigen hasta que los datos cuadran.
 *
 *    - Newton-Levenberg-Marquardt sobre el modelo directo, con el jacobiano
 *      exacto: cada incógnita cuesta una sustitución más con la MISMA LU
 *      (ca_derivada y ca_dvi), no otra factorización.
 *    - Una incógnita: búsqueda exhaustiva en una malla (13 décadas, o toda la
 *      vuelta de un ángulo) y todas sus raíces. De 2 a 4: arranques múltiples.
 *    - Se enseñan todas las soluciones distintas (hasta 4), y si no hay
 *      ninguna se dice por qué, con nombre: faltan datos, no separan dos
 *      incógnitas, no se alcanza el dato (y qué rango sí), solo en el borde,
 *      el valor no cambia nada...
 *
 *  Incógnitas (4 reales como mucho): el valor de R, L o C (vacío o [?!]);
 *  |Z| y/o ∠Z; |E| (|J|) y/o ∠E de una fuente alterna; la f del circuito.
 *  Datos (8 ecuaciones reales como mucho): V e I de R, C, L, Z, S y fuentes
 *  alternas en módulo (1) o fasor (2); la lectura de un V o un A (1); P y fp
 *  de un vatímetro (1 y 1). Un módulo 0 cuenta 2 (el equilibrio de un
 *  puente). Los de una fuente continua no cuentan (en CA está apagada).
 *  [?!] en una V o una I es un pedido, no una incógnita.
 *
 *  Por trozos, como el emulador: cal_iniciar planta el problema y
 *  cal_avanzar gasta unas operaciones y se para SOLO entre evaluaciones
 *  completas, así que el resultado no depende del tamaño de los trozos.
 *  Un sistema a la vez: comparte con el análisis de CA la LUC estática.
 * ========================================================================= */
#ifndef CALCA_H
#define CALCA_H

#include "ca.h"

#define CAL_MAX_INC  4
#define CAL_MAX_ECU  8
#define CAL_MAX_SOL  4
#define CAL_OPS_TOPE 2600000UL                 /* ~3,2 s en la calculadora: casi todas
                                                  son de CA (CA_NS_OP, 1,23 µs; las del
                                                  ajuste, a SIM_NS_OP, abaratan). Eran
                                                  5 M, «~3,2 s» a 0,63 µs: hasta ~6 s */
/* IN_*: en ca.h */

enum { EC_MOD = 0, EC_ANG, EC_RE, EC_IM,       /* de un V o una I (fasor o módulo) */
       EC_P, EC_FP };                          /* de un vatímetro                  */
enum { CAL_OK = 0, CAL_SIM, CAL_FALTAN, CAL_MUCHOS, CAL_NO_CA, CAL_INDET,
       CAL_INCOMPAT, CAL_BORDE, CAL_SIN_EFECTO, CAL_CORTADO };

/* Una incógnita: que = IN_*; log = 1 si u = ln x (valores y f), 0 si es un
 * ángulo; el = el elemento (-1: la f). lo, hi y semilla en las unidades de
 * CalSol.x (los ángulos en grados). */
typedef struct { unsigned char que, log; signed char el; double lo, hi, semilla; } CalInc;
typedef struct {
    unsigned char que;          /* EC_*                                                  */
    unsigned char q;            /* Q_V o Q_I (EC_MOD..EC_IM); en EC_P y EC_FP, el W      */
    signed char   el;
    double        v;            /* ya en lo interno: pico y radianes; EC_FP: φ con signo */
    double        esc;          /* escala del residuo                                    */
} CalEcu;
typedef struct {
    double x[CAL_MAX_INC];      /* físicas y canónicas: Ω, F, H, Hz, V o A eficaces, grados */
    double resid, cond;         /* el mayor residuo (sin dimensiones) y el condicionamiento */
    CaRes  g;                   /* el circuito resuelto con esta solución                */
} CalSol;
typedef struct {
    int     estado, elem, iecu; /* CAL_*; el elemento y la ecuación (índice en ecu[]) del
                                   mensaje, o -1. (La especificación llamaba "ecu" a los
                                   dos campos: el índice pasa a iecu.) CAL_INDET: elem e
                                   iecu son las DOS incógnitas que los datos no separan
                                   (índices en inc[]); CAL_BORDE: elem es la incógnita
                                   (índice en inc[]) e iecu la ecuación                  */
    SimDiag d;                  /* CAL_SIM: el error del modelo directo; si no, SIM_OK
                                   con los avisos del análisis (CC_FUERA...)             */
    double  f;                  /* la f del problema; con IN_F, la semilla               */
    int     ninc, necu;         /* con CAL_MUCHOS, lo contado (aunque no quepa en inc[]) */
    CalInc  inc[CAL_MAX_INC];
    CalEcu  ecu[CAL_MAX_ECU];
    int     ref_fase;           /* la fuente cuyo ángulo se fijó en 0° (referencia), o -1 */
    int     nsol, mas;          /* mas = 1: había más de CAL_MAX_SOL                      */
    CalSol  sol[CAL_MAX_SOL];   /* ordenadas por x[0]                                    */
    double  lo_dato, hi_dato;   /* INCOMPAT con 1 incógnita: lo que puede marcar el dato
                                   (en todas las evaluaciones; un ∠ desenrollado, en grados,
                                   con hi quizá > 180; un fp como φ en radianes, + inductivo) */
    double  peor;               /* INCOMPAT con varias: el error de la ecuación ecu en lo
                                   más cerca que se llega (EC_MOD y EC_P relativo; EC_ANG
                                   en grados; EC_FP en fp; EC_RE/EC_IM el módulo eficaz).
                                   CAL_BORDE: -1 si la incógnita tiende a 0, +1 si a ∞;
                                   CAL_SIN_EFECTO: su |I| sobre la mayor de la red (0 = no
                                   lleva corriente; -1 si no se sabe)                     */
    int     arranques, hechos, evaluaciones;
    unsigned long ops;          /* reales equivalentes gastadas (ca_ops)                  */
} CalRes;                       /* ~17 KB */

/* Incógnitas y ecuaciones (la línea de estado y el motor): no necesita la Red */
int  cal_plantea(const Circuito *c, CalRes *s);
/* Empieza a calcular (copia lo que necesita): CAL_OK si hay trabajo, o el
 * diagnóstico (en cal_res()) */
int  cal_iniciar(const Circuito *c, const Red *r);
int  cal_avanzar(unsigned long ops);                /* 1 = queda trabajo */
void cal_cortar(void);                              /* CAL_CORTADO; lo hallado vale */
double cal_progreso(void);
const CalRes *cal_res(void);
/* El mensaje del estado (4 líneas de 52 como mucho); "" con CAL_OK */
const char *cal_mensaje(const Circuito *c, const CalRes *s, char *buf, int cap);
/* La solución i como datos (para [T], [B], [O]) */
void cal_aplica(const CalRes *s, int i, Circuito *c);

#endif /* CALCA_H */
