/* ============================================================================
 *  luc.c  -  Factorización LU compleja (ver luc.h)
 *
 *  Copia de lu.c con Cx. Cualquier cambio en lu.c se repite aquí: el arnés
 *  ([42]) exige que con im = 0 las dos den lo mismo bit a bit. Como en lu.c,
 *  las pruebas van con los bits (cx_nulo son dos dcmpeq en la calculadora),
 *  y con n > MASC_N van las máscaras por fila.
 * ========================================================================= */
#include <string.h>
#include <stdint.h>
#include <math.h>
#include "luc.h"

static unsigned long ops;

static inline uint64_t bits_de(double x) { uint64_t u; memcpy(&u, &x, sizeof u); return u; }
static inline uint64_t mag_de(double x) { return bits_de(x) & 0x7FFFFFFFFFFFFFFFull; }
#define MAG_INF 0x7FF0000000000000ull
/* a > b con sus mag (los de lu.c: un NaN no es mayor ni menor que nada) */
static inline int mayor(uint64_t ma, uint64_t mb) { return ma <= MAG_INF && ma > mb; }
/* cx_nulo: las dos partes ±0 */
static inline int nulo(Cx a) { return ((bits_de(a.re) << 1) | (bits_de(a.im) << 1)) == 0; }

unsigned long luc_ops(void)       { return ops; }
void          luc_ops_reset(void) { ops = 0; }

int luc_empieza(LUC *f, int n)
{
    int i;
    f->ok = 0;
    f->col_mala = -1;
    if (n < 0 || n > LUC_MAX) { f->n = 0; return 0; }
    f->n = n;
    for (i = 0; i < n; i++) memset(f->a[i], 0, (size_t)n * sizeof f->a[0][0]);
    return 1;
}

static unsigned long fuera;

void luc_suma(LUC *f, int i, int j, Cx v)
{
    if (i < 0 || j < 0) return;                  /* la referencia: se omite */
    if (i >= f->n || j >= f->n) { fuera++; return; }
    f->a[i][j] = cx_mas(f->a[i][j], v);
}

unsigned long luc_fuera(void) { return fuera; }
void          luc_fuera_reset(void) { fuera = 0; }

/* La regla de lu.c: un pivote menor que esto, relativo al mayor coeficiente
 * de su columna, es un cero de redondeo */
#define PIVOTE_MIN 1e-12

/* Con n > MASC_N, las máscaras por fila de lu.c (factoriza_masc): un bit a 0
 * es un 0 (las dos partes ±0) que nadie ha tocado */
#define MASC_N 16
#define MASC_TIENE(m, j) (((m)[(j) >> 5] >> ((j) & 31)) & 1u)
#define MASC_PON(m, j)   ((m)[(j) >> 5] |= 1u << ((j) & 31))

static int factoriza_masc(LUC *f)
{
    int n = f->n, i, j, k, nl = 0, nu = 0;
    uint64_t colmax[LUC_MAX];
    unsigned char cols[LUC_MAX];
    Cx *fila[LUC_MAX];
    uint32_t masc[LUC_MAX][2];                    /* la de cada posición: va con su fila */

    f->ok = 0;
    f->col_mala = -1;
    for (j = 0; j < n; j++) colmax[j] = 0;
    for (i = 0; i < n; i++) {
        const Cx *fi = fila[i] = f->a[i];
        uint32_t *mi = masc[i];
        mi[0] = mi[1] = 0;
        f->perm[i] = i;
        for (j = 0; j < n; j++)
            if (!nulo(fi[j])) {
                uint64_t u = mag_de(cx_abs1(fi[j]));
                MASC_PON(mi, j);
                if (mayor(u, colmax[j])) colmax[j] = u;
            }
        if (!(mi[0] | mi[1])) { f->col_mala = i; return 0; }   /* una fila vacía */
    }

    for (k = 0; k < n; k++) {
        int p = k, nc = 0, w = k >> 5;
        uint32_t bit = 1u << (k & 31), rel[2] = { 0, 0 };
        uint64_t um = mag_de(cx_abs1(fila[k][k]));
        double mx, cm;
        Cx *fk;
        for (i = k + 1; i < n; i++) {
            uint64_t u;
            if (!(masc[i][w] & bit) || nulo(fila[i][k])) continue;   /* |0| no gana a nadie */
            u = mag_de(cx_abs1(fila[i][k]));
            if (mayor(u, um)) { um = u; p = i; }
        }
        memcpy(&mx, &um, sizeof mx);
        memcpy(&cm, &colmax[k], sizeof cm);
        if (!(mx > PIVOTE_MIN * cm)) { f->col_mala = k; return 0; }
        if (p != k) {
            Cx *t = fila[k];
            uint32_t m0 = masc[k][0], m1 = masc[k][1];
            int ti;
            fila[k] = fila[p]; fila[p] = t;
            masc[k][0] = masc[p][0]; masc[k][1] = masc[p][1]; masc[p][0] = m0; masc[p][1] = m1;
            ti = f->perm[k]; f->perm[k] = f->perm[p]; f->perm[p] = ti;
        }
        fk = fila[k];
        f->dinv[k] = cx_inv(fk[k]);
        ops++;
        for (j = k + 1; j < n; j++)
            if (MASC_TIENE(masc[k], j) && !nulo(fk[j])) { cols[nc++] = (unsigned char)j; MASC_PON(rel, j); }
        for (i = k + 1; i < n; i++) {
            Cx *fi, m;
            int q;
            if (!(masc[i][w] & bit)) continue;           /* a[i][k] es 0 */
            fi = fila[i];
            m = fi[k];
            if (nulo(m)) continue;
            m = cx_por(m, f->dinv[k]);
            fi[k] = m;                                   /* L */
            ops++;
            for (q = 0; q < nc; q++) { j = cols[q]; fi[j] = cx_menos(fi[j], cx_por(m, fk[j])); }
            ops += (unsigned long)nc;
            masc[i][0] |= rel[0]; masc[i][1] |= rel[1];
        }
    }

    /* las listas, con lo que tiene su bit (lu.c) */
    for (i = 0; i < n; i++) {
        const Cx *fi = fila[i];
        f->lini[i] = nl;
        f->uini[i] = nu;
        for (j = 0; j < n; j++) {
            uint32_t b = masc[i][j >> 5] >> (j & 31);
            if (!b) { j |= 31; continue; }
            if (!(b & 1u) || j == i || nulo(fi[j])) continue;
            if (j < i) { f->lcol[nl] = (unsigned char)j; f->lval[nl++] = fi[j]; }
            else { f->ucol[nu] = (unsigned char)j; f->uval[nu++] = fi[j]; }
        }
    }
    f->lini[n] = nl;
    f->uini[n] = nu;
    f->ok = 1;
    return 1;
}

int luc_factoriza(LUC *f)
{
    int n = f->n, i, j, k, nl = 0, nu = 0;
    uint64_t colmax[LUC_MAX];                     /* mag del mayor cx_abs1 de cada columna */
    unsigned char cols[LUC_MAX];
    Cx *fila[LUC_MAX];                            /* el pivoteo cambia punteros (lu.c) */

    if (n > MASC_N) return factoriza_masc(f);
    f->ok = 0;
    f->col_mala = -1;

    for (j = 0; j < n; j++) colmax[j] = 0;
    for (i = 0; i < n; i++) {
        const Cx *fi = fila[i] = f->a[i];
        int vacia = 1;
        f->perm[i] = i;
        for (j = 0; j < n; j++)
            if (!nulo(fi[j])) {
                uint64_t u = mag_de(cx_abs1(fi[j]));
                vacia = 0;
                if (mayor(u, colmax[j])) colmax[j] = u;
            }
        if (vacia) { f->col_mala = i; return 0; }         /* una fila vacía */
    }

    for (k = 0; k < n; k++) {
        int p = k, nc = 0;
        uint64_t um = mag_de(cx_abs1(fila[k][k]));
        double mx, cm;
        Cx *fk;
        for (i = k + 1; i < n; i++) {
            uint64_t u;
            if (nulo(fila[i][k])) continue;               /* |0| no gana a nadie */
            u = mag_de(cx_abs1(fila[i][k]));
            if (mayor(u, um)) { um = u; p = i; }
        }
        memcpy(&mx, &um, sizeof mx);
        memcpy(&cm, &colmax[k], sizeof cm);
        if (!(mx > PIVOTE_MIN * cm)) { f->col_mala = k; return 0; }
        if (p != k) {
            Cx *t = fila[k];
            int ti;
            fila[k] = fila[p]; fila[p] = t;
            ti = f->perm[k]; f->perm[k] = f->perm[p]; f->perm[p] = ti;
        }
        fk = fila[k];
        f->dinv[k] = cx_inv(fk[k]);
        ops++;
        /* las columnas no nulas de la fila pivote, una vez por k */
        for (j = k + 1; j < n; j++) if (!nulo(fk[j])) cols[nc++] = (unsigned char)j;
        for (i = k + 1; i < n; i++) {
            Cx *fi = fila[i], m = fi[k];
            int q;
            if (nulo(m)) continue;
            m = cx_por(m, f->dinv[k]);
            fi[k] = m;                                   /* L */
            ops++;
            for (q = 0; q < nc; q++) { j = cols[q]; fi[j] = cx_menos(fi[j], cx_por(m, fk[j])); }
            ops += (unsigned long)nc;
        }
    }

    /* Listas de no nulos, por filas, para la sustitución */
    for (i = 0; i < n; i++) {
        const Cx *fi = fila[i];
        f->lini[i] = nl;
        for (j = 0; j < i; j++)
            if (!nulo(fi[j])) { f->lcol[nl] = (unsigned char)j; f->lval[nl++] = fi[j]; }
        f->uini[i] = nu;
        for (j = i + 1; j < n; j++)
            if (!nulo(fi[j])) { f->ucol[nu] = (unsigned char)j; f->uval[nu++] = fi[j]; }
    }
    f->lini[n] = nl;
    f->uini[n] = nu;
    f->ok = 1;
    return 1;
}

void luc_resuelve(const LUC *f, const Cx *b, Cx *x)
{
    Cx y[LUC_MAX];
    int n = f->n, i, q;

    /* y = L^-1 · P · b */
    for (i = 0; i < n; i++) {
        int fila = f->perm[i];
        Cx s = b[fila];
        for (q = f->lini[i]; q < f->lini[i + 1]; q++) s = cx_menos(s, cx_por(f->lval[q], y[f->lcol[q]]));
        y[i] = s;
        ops += (unsigned long)(1 + f->lini[i + 1] - f->lini[i]);
    }
    /* x = U^-1 · y */
    for (i = n - 1; i >= 0; i--) {
        Cx s = y[i];
        for (q = f->uini[i]; q < f->uini[i + 1]; q++) s = cx_menos(s, cx_por(f->uval[q], x[f->ucol[q]]));
        x[i] = cx_por(s, f->dinv[i]);
        ops += (unsigned long)(1 + f->uini[i + 1] - f->uini[i]);
    }
}
