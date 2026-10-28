/* ============================================================================
 *  algebra.h  -  Sistema lineal AX = B y su resolucion
 *
 *  Copiado de EstaticaCX (commit eca703d) con estos cambios:
 *    - topes mayores: 64 nodos + 32 elementos = 96 incognitas como mucho
 *    - tolerancia de pivote RELATIVA: aqui conviven coeficientes de 1 (leyes
 *      de nodo) con resistencias de megaohmios,
 *    - se exponen las columnas libres, que dicen QUE incognita quedo sin
 *      determinar: con eso el diagnostico puede nombrar el elemento.
 *    - VARIOS terminos independientes a la vez (b[0] fijo y uno por dato
 *      medido). La matriz A no depende de los signos de los datos, solo el
 *      termino independiente, y lo hace de forma lineal: se elimina UNA vez
 *      y cada combinacion de signos sale de sumar columnas ya reducidas
 *      (sis_combinar). Con 8 datos son 9 columnas en vez de 256 sistemas.
 *    - la eliminacion se salta los ceros de la fila pivote y divide una
 *      sola vez por pivote: la tabla de un circuito es muy dispersa.
 * ========================================================================= */
#ifndef ALGEBRA_H
#define ALGEBRA_H

#define MAX_EQ   128
#define MAX_UNK  100
#define MAX_TERM   9                   /* b[0] fijo + 8 datos medidos      */

typedef struct {
    double a[MAX_EQ][MAX_UNK];
    double b[MAX_TERM][MAX_EQ];        /* terminos independientes          */
    int    neq, nunk, nterm;
    int    desborde;                   /* 1 si no cupo algo                */
} Sistema;

enum {
    GAUSS_OK = 0,
    GAUSS_INDETERMINADO,  /* rango < incognitas: faltan datos             */
    GAUSS_INCOMPATIBLE,   /* ecuaciones contradictorias                   */
    GAUSS_VACIO
};

/* No borra los 100 KB de la matriz: cada fila se pone a cero al crearla. */
void sis_init(Sistema *s);

/* Reserva n incognitas / una ecuacion. Devuelven el indice o -1 si no cabe. */
int  sis_incognitas(Sistema *s, int n);
int  sis_ecuacion(Sistema *s);
/* Numero de terminos independientes (1 por omision). 0 si cabe. */
int  sis_terminos(Sistema *s, int n);

void sis_coef(Sistema *s, int eq, int unk, double c);  /* acumula en A    */
void sis_term(Sistema *s, int eq, double v);           /* acumula en b[0] */
void sis_term_k(Sistema *s, int k, int eq, double v);  /* acumula en b[k] */

/* Reduce [A | b0 .. b(nterm-1)] a forma escalonada reducida con pivoteo
 * parcial y la guarda (una sola a la vez, estatica). Escribe el rango y, si
 * libre != NULL, libre[j] = 1 para cada columna sin pivote. Devuelve
 * GAUSS_VACIO si no hay nada que resolver, o GAUSS_OK.                     */
int  sis_reducir(const Sistema *s, int *rango, unsigned char *libre);

/* Solucion del ULTIMO sistema reducido para el termino independiente
 * w[0]*b0 + w[1]*b1 + ... (las variables libres quedan a 0). Devuelve
 * GAUSS_OK, GAUSS_INDETERMINADO, GAUSS_INCOMPATIBLE o GAUSS_VACIO.        */
int  sis_combinar(const double *w, double *x);

/* Lo mismo para w = (1, +-1, +-1, ...): el bit k-1 de signos a 1 = el
 * termino k resta. Es lo que usa el motor para los datos en magnitud: ni
 * multiplica ni calcula la tolerancia si el rango es completo. La primera
 * llamada tras sis_reducir suma todos los juegos a la vez, por niveles (100
 * KB estaticos); las demas solo leen su juego. */
int  sis_combinar_signos(unsigned signos, double *x);
#ifndef NDLESS
/* Para el arnes: 1 = cada juego se suma entero en cada llamada, como antes
 * (tiene que dar lo mismo, bit a bit, y las mismas operaciones contadas) */
void sis_signos_lento(int si);
#endif

/* Lo de siempre: reducir y resolver con b0 solo. */
int  sis_resolver(const Sistema *s, double *x, int *rango, unsigned char *libre);

/* Para el arnes: operaciones de coma flotante (una por cada
 * multiplicacion-resta y cada division de la eliminacion, y una por cada
 * suma de sis_combinar_signos) y coeficientes que se
 * intentaron escribir fuera del sistema (tiene que ser 0: un indice asi es
 * un error del que arma las ecuaciones, no un dato). */
unsigned long sis_ops(void);
void          sis_ops_reset(void);
unsigned long sis_fuera(void);

#endif /* ALGEBRA_H */
