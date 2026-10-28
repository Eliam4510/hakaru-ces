/* ============================================================================
 *  luc.h  -  Factorización LU compleja para la corriente alterna
 *
 *  Es lu.c línea a línea con Cx en vez de double: la misma regla de pivote
 *  (el módulo |re| + |im| en vez de |x|, relativo al mayor de SU columna),
 *  las mismas listas de no nulos y el mismo tamaño (LUC_MAX = LU_MAX = 64).
 *  Con la parte imaginaria nula hace exactamente las mismas operaciones que
 *  lu.c (a·1 - 0·0 = a·1 en coma flotante): el arnés las compara bit a bit.
 *
 *  Cuenta las operaciones igual que lu.c, pero complejas: para el tiempo en
 *  la calculadora, una compleja son 4 reales (2,52 µs).
 * ========================================================================= */
#ifndef LUC_H
#define LUC_H

#include "lu.h"
#include "cx.h"

#define LUC_MAX LU_MAX                          /* 64 */
#define LUC_NNZ LU_NNZ

typedef struct {
    int n, ok, col_mala;
    Cx  a[LUC_MAX][LUC_MAX + 1];                /* + relleno (lu.h): filas de 1040 B, no 1 KB */
    int perm[LUC_MAX];
    Cx  dinv[LUC_MAX];
    int lini[LUC_MAX + 1], uini[LUC_MAX + 1];
    unsigned char lcol[LUC_NNZ], ucol[LUC_NNZ];
    Cx  lval[LUC_NNZ], uval[LUC_NNZ];
} LUC;                                          /* ~135 KB: siempre estática */

int  luc_empieza(LUC *f, int n);
void luc_suma(LUC *f, int i, int j, Cx v);      /* índice < 0: se omite; >= n: fuera++ */
int  luc_factoriza(LUC *f);                     /* pivote por cx_abs1; nulo si < 1e-12 × máx. de su columna */
void luc_resuelve(const LUC *f, const Cx *b, Cx *x);
unsigned long luc_ops(void);   void luc_ops_reset(void);    /* operaciones COMPLEJAS */
unsigned long luc_fuera(void); void luc_fuera_reset(void);

#endif /* LUC_H */
