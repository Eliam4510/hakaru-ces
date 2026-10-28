/* ============================================================================
 *  lu.h  -  Factorización LU para el emulador
 *
 *  El emulador resuelve el MISMO sistema miles de veces (uno por paso de
 *  tiempo): la matriz solo cambia cuando cambia la topología (un interruptor)
 *  o el paso. Por eso se factoriza una vez, A = P·L·U, y en cada paso solo se
 *  sustituye hacia delante y hacia atrás.
 *
 *  - Pivoteo parcial por filas, como LAPACK. En un circuito conviven
 *    conductancias de 1e-9 S (1 GΩ) y de 1e6 S (1 µΩ): un pivote se da por
 *    cero comparándolo con el mayor coeficiente de SU columna (la de su
 *    incógnita), no con uno absoluto que daría por singular una red sana.
 *  - Tras factorizar se guardan listas con los no nulos de L y de U: la
 *    matriz de un circuito es muy dispersa (3 a 5 coeficientes por fila) y la
 *    sustitución solo recorre esos. En la calculadora cada operación de coma
 *    flotante cuesta 0,63 µs: con 20 incógnitas, un paso son ~100 operaciones
 *    en vez de las 400 de una sustitución densa.
 *  - Cuenta las operaciones (multiplicar y restar, dividir) para el modelo de
 *    tiempos del arnés, como sis_ops() en algebra.c.
 * ========================================================================= */
#ifndef LU_H
#define LU_H

#define LU_MAX 64          /* 2·MAX_EL: nodos - subcircuitos <= elementos, y una rama por elemento */
#define LU_NNZ (LU_MAX * (LU_MAX - 1) / 2)     /* no nulos de L (o de U)     */

typedef struct {
    int    n;
    int    ok;                    /* 1 si se factorizó                     */
    int    col_mala;              /* columna sin pivote (singular), o -1   */
    double a[LU_MAX][LU_MAX + 1]; /* la matriz (fuera de lu.c solo se lee
                                     antes de factorizar). La columna de
                                     relleno: con filas de 512 B, potencia de
                                     dos, las de una columna caían en 4
                                     conjuntos de la D-caché de 8 KB           */
    int    perm[LU_MAX];          /* fila original en cada posición        */
    double dinv[LU_MAX];          /* 1 / pivote                            */
    int    lini[LU_MAX + 1], uini[LU_MAX + 1];
    unsigned char lcol[LU_NNZ], ucol[LU_NNZ];
    double lval[LU_NNZ], uval[LU_NNZ];
} LU;

/* Empieza una matriz n x n llena de ceros (n <= LU_MAX). 0 si no cabe. */
int  lu_empieza(LU *f, int n);

/* Suma v al coeficiente (i, j): así se "estampa" cada elemento. Los índices
 * fuera de rango (una referencia, -1) se ignoran. */
void lu_suma(LU *f, int i, int j, double v);

/* Factoriza. 1 si todo bien; 0 si la matriz es singular (en col_mala, la
 * primera columna sin pivote). */
int  lu_factoriza(LU *f);
/* Lo mismo con otro umbral de pivote (relativo a su columna). Los no lineales
 * usan uno mucho menor: un bloque que solo cuelga de diodos en corte lo sujeta
 * GMIN = 1e-12 S y es regular (Etapa 4) */
int  lu_factoriza_tol(LU *f, double pivote_min);

/* x = A^-1 b (b y x pueden ser el mismo vector). Solo tras lu_factoriza. */
void lu_resuelve(const LU *f, const double *b, double *x);

/* Operaciones de coma flotante hechas (factorizar y sustituir) */
unsigned long lu_ops(void);
void          lu_ops_reset(void);
/* Suma n: las de una factorización que se reutiliza en vez de rehacerla
 * (el barrido lineal de sim.c), para que el modelo de tiempos no cambie */
void          lu_ops_suma(unsigned long n);
/* Coeficientes con un índice >= n (un error de mapa): el arnés exige 0 */
unsigned long lu_fuera(void);
void          lu_fuera_reset(void);

#endif /* LU_H */
