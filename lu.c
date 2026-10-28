/* ============================================================================
 *  lu.c  -  Factorización LU para el emulador  (ver lu.h)
 *
 *  Las pruebas de la factorización («¿es cero?», «¿es mayor en módulo?») van
 *  con los bits del double y no en coma flotante: en la calculadora, sin
 *  FPU, cada ==, < o > entre double es una llamada a libgcc
 *  (__aeabi_dcmpeq, ~60 ciclos), y las del triple bucle eran O(n³). Dan
 *  exactamente lo mismo que las de coma flotante, con ±0 y con NaN (ver
 *  mayor), y las operaciones son las mismas en el mismo orden: bit a bit
 *  igual, con las mismas ops. Con muchas incógnitas (n > MASC_N), una
 *  máscara por fila evita leer los ceros de las columnas (ver
 *  factoriza_masc).
 * ========================================================================= */
#include <string.h>
#include <stdint.h>
#include <math.h>
#include "lu.h"

static unsigned long ops;

static inline uint64_t bits_de(double x) { uint64_t u; memcpy(&u, &x, sizeof u); return u; }
/* Los bits de |x| (fabs solo borra el signo). Sin signo, el orden de los
 * enteros es el de los double: |a| > |b| si mag(a) > mag(b), salvo NaN */
static inline uint64_t mag_de(double x) { return bits_de(x) & 0x7FFFFFFFFFFFFFFFull; }
#define MAG_INF 0x7FF0000000000000ull            /* el de ∞; por encima, NaN   */
/* fabs(a) > fabs(b) con sus mag: como en coma flotante, un NaN no es mayor
 * que nada ni nada es mayor que un NaN (sus bits son los más altos) */
static inline int mayor(uint64_t ma, uint64_t mb) { return ma <= MAG_INF && ma > mb; }
/* x == 0.0 (+0 o -0; un NaN no es cero, como en C) */
static inline int es_cero(double x) { return (bits_de(x) << 1) == 0; }

unsigned long lu_ops(void)       { return ops; }
void          lu_ops_reset(void) { ops = 0; }
void          lu_ops_suma(unsigned long n) { ops += n; }

int lu_empieza(LU *f, int n)
{
    int i;
    f->ok = 0;
    f->col_mala = -1;
    if (n < 0 || n > LU_MAX) { f->n = 0; return 0; }
    f->n = n;
    for (i = 0; i < n; i++) memset(f->a[i], 0, (size_t)n * sizeof f->a[0][0]);
    return 1;
}

static unsigned long fuera;

void lu_suma(LU *f, int i, int j, double v)
{
    if (i < 0 || j < 0) return;                  /* la referencia: se omite */
    if (i >= f->n || j >= f->n) { fuera++; return; }
    f->a[i][j] += v;
}

unsigned long lu_fuera(void) { return fuera; }
void          lu_fuera_reset(void) { fuera = 0; }

/* Un pivote más pequeño que esto, relativo al mayor coeficiente que tenía su
 * columna, es un cero de redondeo: la columna no tiene pivote. La columna es
 * la de una incógnita (un nodo con sus conductancias), así que un nodo que
 * solo cuelga de 1 GΩ tiene su propia escala y no se confunde con un cero. */
#define PIVOTE_MIN 1e-12

int lu_factoriza(LU *f) { return lu_factoriza_tol(f, PIVOTE_MIN); }

/* Con n > MASC_N la matriz ya no cabe en la D-caché (8 KB en el modelo del
 * perfil ARM; una fila de la LU ocupa 520 B y una de la LUC 1040 B), y cada
 * a[i][k] que se lee bajando por una columna (el pivote, las filas que
 * eliminar) es una línea que falla; las listas del final vuelven a leerlo
 * todo. Entonces cada fila lleva una máscara (dos palabras: las columnas
 * 0-31 y 32-63) con un bit por coeficiente que puede no ser 0: nace con los
 * no nulos, gana el relleno de cada eliminación y no pierde los que se
 * cancelan (esos se siguen probando con el valor). Un bit a 0 es un ±0 que
 * nadie ha tocado: no gana el pivote, no se elimina ni va a las listas, y
 * saltarlo da lo mismo, bit a bit y con las mismas ops (punto de CA de 40
 * incógnitas: de 4256 a 3070 fallos de la D-caché, −8 %). Con n pequeño la
 * matriz cabe en la caché y las máscaras solo añaden instrucciones: ahí va
 * la factorización de siempre */
#define MASC_N 16
#define MASC_TIENE(m, j) (((m)[(j) >> 5] >> ((j) & 31)) & 1u)
#define MASC_PON(m, j)   ((m)[(j) >> 5] |= 1u << ((j) & 31))

static int factoriza_masc(LU *f, double pivote_min)
{
    int n = f->n, i, j, k, nl = 0, nu = 0;
    uint64_t colmax[LU_MAX];
    unsigned char cols[LU_MAX];
    double *fila[LU_MAX];
    uint32_t masc[LU_MAX][2];                     /* la de cada posición: va con su fila */

    f->ok = 0;
    f->col_mala = -1;
    for (j = 0; j < n; j++) colmax[j] = 0;
    for (i = 0; i < n; i++) {
        const double *fi = fila[i] = f->a[i];
        uint32_t *mi = masc[i];
        mi[0] = mi[1] = 0;
        f->perm[i] = i;
        for (j = 0; j < n; j++) {
            uint64_t u = mag_de(fi[j]);
            if (u != 0) {                                 /* no es ±0 */
                MASC_PON(mi, j);
                if (mayor(u, colmax[j])) colmax[j] = u;
            }
        }
        if (!(mi[0] | mi[1])) { f->col_mala = i; return 0; }   /* una fila vacía */
    }

    for (k = 0; k < n; k++) {
        int p = k, nc = 0, w = k >> 5;
        uint32_t bit = 1u << (k & 31), rel[2] = { 0, 0 };
        uint64_t um = mag_de(fila[k][k]);
        double mx, cm, piv, *fk;
        for (i = k + 1; i < n; i++)
            if (masc[i][w] & bit) {                       /* sin su bit: ±0, no gana */
                uint64_t u = mag_de(fila[i][k]);
                if (mayor(u, um)) { um = u; p = i; }
            }
        memcpy(&mx, &um, sizeof mx);
        memcpy(&cm, &colmax[k], sizeof cm);
        if (!(mx > pivote_min * cm)) { f->col_mala = k; return 0; }
        if (p != k) {
            double *t = fila[k];
            uint32_t m0 = masc[k][0], m1 = masc[k][1];
            int ti;
            fila[k] = fila[p]; fila[p] = t;
            masc[k][0] = masc[p][0]; masc[k][1] = masc[p][1]; masc[p][0] = m0; masc[p][1] = m1;
            ti = f->perm[k]; f->perm[k] = f->perm[p]; f->perm[p] = ti;
        }
        fk = fila[k];
        piv = fk[k];
        f->dinv[k] = 1.0 / piv;
        ops++;
        /* las columnas no nulas de la fila pivote y su máscara: el relleno */
        for (j = k + 1; j < n; j++)
            if (MASC_TIENE(masc[k], j) && !es_cero(fk[j])) { cols[nc++] = (unsigned char)j; MASC_PON(rel, j); }
        for (i = k + 1; i < n; i++) {
            double *fi, m;
            int q;
            if (!(masc[i][w] & bit)) continue;           /* a[i][k] es ±0 */
            fi = fila[i];
            m = fi[k];
            if (es_cero(m)) continue;
            m *= f->dinv[k];
            fi[k] = m;                                   /* L */
            ops++;
            for (q = 0; q < nc; q++) { j = cols[q]; fi[j] -= m * fk[j]; }
            ops += (unsigned long)nc;
            masc[i][0] |= rel[0]; masc[i][1] |= rel[1];
        }
    }

    /* Las listas, con lo que tiene su bit (en orden de j: primero las de L,
     * luego las de U; una palabra sin más bits se salta entera) */
    for (i = 0; i < n; i++) {
        const double *fi = fila[i];
        f->lini[i] = nl;
        f->uini[i] = nu;
        for (j = 0; j < n; j++) {
            uint32_t b = masc[i][j >> 5] >> (j & 31);
            if (!b) { j |= 31; continue; }
            if (!(b & 1u) || j == i || es_cero(fi[j])) continue;
            if (j < i) { f->lcol[nl] = (unsigned char)j; f->lval[nl++] = fi[j]; }
            else { f->ucol[nu] = (unsigned char)j; f->uval[nu++] = fi[j]; }
        }
    }
    f->lini[n] = nl;
    f->uini[n] = nu;
    f->ok = 1;
    return 1;
}

int lu_factoriza_tol(LU *f, double pivote_min)
{
    int n = f->n, i, j, k, nl = 0, nu = 0;
    uint64_t colmax[LU_MAX];                      /* mag del mayor de cada columna */
    unsigned char cols[LU_MAX];
    /* La fila de cada posición: el pivoteo intercambia los punteros, no las
     * filas (3 memcpy de n double por intercambio). Así f->a no queda en el
     * orden de perm, pero nadie la lee tras factorizar: las listas de abajo
     * ya salen en ese orden */
    double *fila[LU_MAX];

    if (n > MASC_N) return factoriza_masc(f, pivote_min);
    f->ok = 0;
    f->col_mala = -1;

    /* Pivoteo parcial SIN normalizar las filas (como LAPACK): normalizarlas
     * deja competir como pivote a la fila de un nodo de muy alta impedancia
     * (1e-9 S) con las demás, y la fila elegida llena las otras de
     * coeficientes que luego se cancelan: con una tensión de GV en ese nodo
     * se pierden 4 o 5 cifras en el resto. */
    for (j = 0; j < n; j++) colmax[j] = 0;
    for (i = 0; i < n; i++) {
        const double *fi = fila[i] = f->a[i];
        int vacia = 1;
        f->perm[i] = i;
        for (j = 0; j < n; j++) {
            uint64_t u = mag_de(fi[j]);
            if (u != 0) {                                 /* no es ±0 */
                vacia = 0;
                if (mayor(u, colmax[j])) colmax[j] = u;
            }
        }
        if (vacia) { f->col_mala = i; return 0; }         /* una fila vacía */
    }

    for (k = 0; k < n; k++) {
        int p = k, nc = 0;
        uint64_t um = mag_de(fila[k][k]);
        double mx, cm, piv, *fk;
        for (i = k + 1; i < n; i++) {
            uint64_t u = mag_de(fila[i][k]);
            if (mayor(u, um)) { um = u; p = i; }
        }
        memcpy(&mx, &um, sizeof mx);                      /* el fabs del pivote */
        memcpy(&cm, &colmax[k], sizeof cm);
        if (!(mx > pivote_min * cm)) { f->col_mala = k; return 0; }
        if (p != k) {
            double *t = fila[k];
            int ti;
            fila[k] = fila[p]; fila[p] = t;
            ti = f->perm[k]; f->perm[k] = f->perm[p]; f->perm[p] = ti;
        }
        fk = fila[k];
        piv = fk[k];
        f->dinv[k] = 1.0 / piv;
        ops++;
        /* La fila pivote es dispersa: sus columnas no nulas, una vez por k (la
         * eliminación de las filas de abajo no la cambia) */
        for (j = k + 1; j < n; j++) if (!es_cero(fk[j])) cols[nc++] = (unsigned char)j;
        for (i = k + 1; i < n; i++) {
            double *fi = fila[i], m = fi[k];
            int q;
            if (es_cero(m)) continue;
            m *= f->dinv[k];
            fi[k] = m;                                   /* L */
            ops++;
            for (q = 0; q < nc; q++) { j = cols[q]; fi[j] -= m * fk[j]; }
            ops += (unsigned long)nc;
        }
    }

    /* Listas de no nulos, por filas, para la sustitución */
    for (i = 0; i < n; i++) {
        const double *fi = fila[i];
        f->lini[i] = nl;
        for (j = 0; j < i; j++)
            if (!es_cero(fi[j])) { f->lcol[nl] = (unsigned char)j; f->lval[nl++] = fi[j]; }
        f->uini[i] = nu;
        for (j = i + 1; j < n; j++)
            if (!es_cero(fi[j])) { f->ucol[nu] = (unsigned char)j; f->uval[nu++] = fi[j]; }
    }
    f->lini[n] = nl;
    f->uini[n] = nu;
    f->ok = 1;
    return 1;
}

void lu_resuelve(const LU *f, const double *b, double *x)
{
    double y[LU_MAX];
    int n = f->n, i, q;

    /* y = L^-1 · P · b */
    for (i = 0; i < n; i++) {
        int fila = f->perm[i];
        double s = b[fila];
        for (q = f->lini[i]; q < f->lini[i + 1]; q++) s -= f->lval[q] * y[f->lcol[q]];
        y[i] = s;
        ops += (unsigned long)(1 + f->lini[i + 1] - f->lini[i]);
    }
    /* x = U^-1 · y */
    for (i = n - 1; i >= 0; i--) {
        double s = y[i];
        for (q = f->uini[i]; q < f->uini[i + 1]; q++) s -= f->uval[q] * x[f->ucol[q]];
        x[i] = s * f->dinv[i];
        ops += (unsigned long)(1 + f->uini[i + 1] - f->uini[i]);
    }
}
