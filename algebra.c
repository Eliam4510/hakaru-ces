/* ============================================================================
 *  algebra.c  -  Sistema lineal AX = B  (ver algebra.h)
 * ========================================================================= */
#include <string.h>
#include <math.h>
#include "algebra.h"

static unsigned long ops, fuera;

unsigned long sis_ops(void)       { return ops; }
void          sis_ops_reset(void) { ops = 0; }
unsigned long sis_fuera(void)     { return fuera; }

void sis_init(Sistema *s)
{
    s->neq = 0;
    s->nunk = 0;
    s->nterm = 1;
    s->desborde = 0;
}

int sis_incognitas(Sistema *s, int n)
{
    int i = s->nunk;
    if (n < 0 || s->nunk + n > MAX_UNK) { s->desborde = 1; return -1; }
    s->nunk += n;
    return i;
}

int sis_ecuacion(Sistema *s)
{
    int t;
    if (s->neq >= MAX_EQ) { s->desborde = 1; return -1; }
    memset(s->a[s->neq], 0, sizeof s->a[0]);      /* fila nueva, entera a 0 */
    for (t = 0; t < MAX_TERM; t++) s->b[t][s->neq] = 0.0;
    return s->neq++;
}

int sis_terminos(Sistema *s, int n)
{
    if (n < 1 || n > MAX_TERM) { s->desborde = 1; return -1; }
    s->nterm = n;
    return 0;
}

void sis_coef(Sistema *s, int eq, int unk, double c)
{
    if (eq < 0 || eq >= s->neq || unk < 0 || unk >= s->nunk) {
        if (!s->desborde) fuera++;                 /* tras un desborde es normal */
        return;
    }
    s->a[eq][unk] += c;
}

void sis_term_k(Sistema *s, int k, int eq, double v)
{
    if (k < 0 || k >= s->nterm || eq < 0 || eq >= s->neq) {
        if (!s->desborde) fuera++;
        return;
    }
    s->b[k][eq] += v;
}

void sis_term(Sistema *s, int eq, double v) { sis_term_k(s, 0, eq, v); }

/* --------------------------------------------------------------------------
 *  Gauss-Jordan con pivoteo parcial
 *
 *  Se reduce a forma escalonada REDUCIDA, de modo que la solucion se lee
 *  directamente. La matriz de trabajo es estatica: 128 x 109 doubles son
 *  110 KB, demasiado para la pila de la calculadora. Se queda guardada para
 *  que sis_combinar() lea de ella las soluciones de cada juego de signos.
 * ----------------------------------------------------------------------- */
#define ANCHO (MAX_UNK + MAX_TERM)

static double m[MAX_EQ][ANCHO];
static double b_orig[MAX_TERM][MAX_EQ];        /* para la escala de cada b */
static int    piv[MAX_UNK];
static unsigned char fila_cero[MAX_EQ];         /* filas sin coeficientes  */
static int    r_neq, r_nunk, r_nterm, r_rango, r_vacio = 1, r_nulas;
static double r_tol_a;
static int    sg_listo;                         /* las sumas de signos, hechas (abajo) */

int sis_reducir(const Sistema *s, int *rango, unsigned char *libre)
{
    static int nz[ANCHO];                       /* columnas no nulas del pivote */
    int i, j, k, t, fila = 0, col, r = 0, ancho;
    double escala_a = 0.0;

    sg_listo = 0;                               /* otro sistema: otras sumas */
    if (rango) *rango = 0;
    r_neq = s->neq; r_nunk = s->nunk; r_nterm = s->nterm; r_rango = 0;
    r_vacio = (s->nunk <= 0 || s->neq <= 0);
    if (libre) for (j = 0; j < s->nunk; j++) libre[j] = 1;
    if (r_vacio) return GAUSS_VACIO;

    ancho = s->nunk + s->nterm;
    for (i = 0; i < s->neq; i++) {
        for (j = 0; j < s->nunk; j++) {
            m[i][j] = s->a[i][j];
            if (fabs(m[i][j]) > escala_a) escala_a = fabs(m[i][j]);
        }
        for (t = 0; t < s->nterm; t++) {
            m[i][s->nunk + t] = s->b[t][i];
            b_orig[t][i] = s->b[t][i];
        }
    }
    /* Relativa: una resistencia de 1 MOhm junto a un coeficiente 1 de una
     * ley de nodo no debe hacer pasar ruido por informacion. */
    r_tol_a = 1e-11 * (escala_a > 1.0 ? escala_a : 1.0);

    for (col = 0; col < s->nunk && fila < s->neq; col++) {
        int mejor = -1, nnz = 0;
        double mx = 0.0, inv;

        for (i = fila; i < s->neq; i++) {
            double v = fabs(m[i][col]);
            if (v > mx) { mx = v; mejor = i; }
        }
        if (mejor < 0 || mx < r_tol_a) continue;      /* columna libre */

        if (mejor != fila)
            for (j = col; j < ancho; j++) {
                double tt = m[fila][j]; m[fila][j] = m[mejor][j]; m[mejor][j] = tt;
            }

        inv = 1.0 / m[fila][col];
        m[fila][col] = 1.0;
        for (j = col + 1; j < ancho; j++)
            if (m[fila][j] != 0.0) { m[fila][j] *= inv; nz[nnz++] = j; }
        ops += (unsigned long)nnz + 1;

        for (i = 0; i < s->neq; i++) {
            double f;
            if (i == fila) continue;
            f = m[i][col];
            if (f == 0.0) continue;
            m[i][col] = 0.0;
            for (k = 0; k < nnz; k++) m[i][nz[k]] -= f * m[fila][nz[k]];
            ops += (unsigned long)nnz;
        }

        piv[r++] = col;
        fila++;
    }

    r_rango = r;
    if (rango) *rango = r;
    if (libre) for (k = 0; k < r; k++) libre[piv[k]] = 0;

    /* Filas que quedaron sin coeficientes: si su termino no es nulo, hay
     * contradiccion (eso depende de los signos: lo mira sis_combinar). */
    r_nulas = 0;
    for (i = r; i < s->neq; i++) {
        fila_cero[i] = 1;
        for (j = 0; j < s->nunk; j++)
            if (fabs(m[i][j]) > r_tol_a) { fila_cero[i] = 0; break; }
        r_nulas += fila_cero[i];
    }
    return GAUSS_OK;
}

#ifndef NDLESS
/* Termino independiente de la fila i para un juego de signos: b0 + sum(+-bk).
 * Sumar o restar es exacto igual que multiplicar por +-1 (IEEE), pero en la
 * calculadora no gasta ni una multiplicacion ni una comparacion. Ahora solo
 * la usa la manera de antes (el arnes, abajo); la de siempre hace las mismas
 * sumas por niveles */
static double suma_signos(const double *fila, unsigned signos)
{
    double v = fila[0];
    int t;
    for (t = 1; t < r_nterm; t++)
        v = (signos & (1u << (t - 1))) ? v - fila[t] : v + fila[t];
    return v;
}
#endif

/* --------------------------------------------------------------------------
 *  Todas las combinaciones de signos, por niveles
 *
 *  suma_signos va termino a termino: tras sumar los t primeros, el valor
 *  solo depende de los t-1 bits bajos del juego. Por niveles salen todos los
 *  juegos a la vez, P[c + 2^(t-1)] = P[c] - b(t) y P[c] = P[c] + b(t):
 *  2^(nd+1) sumas por fila en vez de nd*2^nd, y cada juego hace exactamente
 *  las mismas sumas, con los mismos numeros y en el mismo orden (bit a bit
 *  lo que da suma_signos). Se guarda el penultimo nivel (la mitad de los
 *  juegos, por filas contiguas: lo que lee cada juego esta junto); el ultimo
 *  termino lo suma cada juego. Se hace en la primera llamada tras
 *  sis_reducir, que es lo unico que cambia las filas reducidas.
 * ----------------------------------------------------------------------- */
#define SG_JUEGOS (1 << (MAX_TERM - 1))        /* 256 juegos de signos      */
static double sg_arbol[SG_JUEGOS / 2][MAX_UNK]; /* penultimo nivel: 100 KB   */
static double sg_col[MAX_TERM][MAX_UNK];       /* el termino t de cada fila */
static double sg_fila[SG_JUEGOS];              /* una fila entera, al vuelo */
static double sg_esc[SG_JUEGOS];               /* la escala de b de cada juego */
static unsigned char sg_vistas[SG_JUEGOS];     /* filas nulas miradas       */
static unsigned char sg_incomp[SG_JUEGOS];     /* 1: alguna no da 0         */

/* P[c] = suma_signos(f, c) para los 2^nd juegos */
static void sg_todos(const double *f, int nd, double *P)
{
    int t;
    unsigned c, h;
    P[0] = f[0];
    for (t = 1; t <= nd; t++)
        for (h = 1u << (t - 1), c = 0; c < h; c++) {
            P[c + h] = P[c] - f[t];
            P[c] = P[c] + f[t];
        }
}

static void sg_prepara(void)
{
    int nd = r_nterm - 1, t, k, i;
    unsigned c, h, nj = 1u << nd;

    for (k = 0; k < r_rango; k++) {
        for (t = 0; t < r_nterm; t++) sg_col[t][k] = m[k][r_nunk + t];
        sg_arbol[0][k] = sg_col[0][k];
    }
    for (t = 1; t < nd; t++)                    /* hasta el penultimo nivel */
        for (h = 1u << (t - 1), c = 0; c < h; c++)
            for (k = 0; k < r_rango; k++) {
                sg_arbol[c + h][k] = sg_arbol[c][k] - sg_col[t][k];
                sg_arbol[c][k] = sg_arbol[c][k] + sg_col[t][k];
            }

    /* Con filas sin coeficientes: la escala de b de cada juego (el mayor
     * |b| de todas las filas, en su orden) y, fila nula a fila nula, hasta
     * cual se mira y si alguna no da 0, como en el bucle de antes */
    if (r_nulas) {
        double col[MAX_TERM];
        for (c = 0; c < nj; c++) { sg_esc[c] = 0.0; sg_vistas[c] = 0; sg_incomp[c] = 0; }
        for (i = 0; i < r_neq; i++) {
            for (t = 0; t < r_nterm; t++) col[t] = b_orig[t][i];
            sg_todos(col, nd, sg_fila);
            for (c = 0; c < nj; c++) if (fabs(sg_fila[c]) > sg_esc[c]) sg_esc[c] = fabs(sg_fila[c]);
        }
        for (i = r_rango; i < r_neq; i++) {
            if (!fila_cero[i]) continue;
            sg_todos(&m[i][r_nunk], nd, sg_fila);
            for (c = 0; c < nj; c++) {
                if (sg_incomp[c]) continue;
                sg_vistas[c]++;
                if (fabs(sg_fila[c]) > 1e-8 * (1.0 + sg_esc[c])) sg_incomp[c] = 1;
            }
        }
    }
    sg_listo = 1;
}

#ifndef NDLESS
/* Para el arnes: la de antes, que suma cada juego entero con suma_signos
 * (test_opt_otros compara las dos, bit a bit) */
static int sg_lento;
void sis_signos_lento(int si) { sg_lento = si; }

static int combinar_lento(unsigned signos, double *x)
{
    int i, k;
    static double col[MAX_TERM];

    if (r_nulas) {
        double escala_b = 0.0, tol_b;
        int t;
        for (i = 0; i < r_neq; i++) {
            double v;
            for (t = 0; t < r_nterm; t++) col[t] = b_orig[t][i];
            v = suma_signos(col, signos);
            if (fabs(v) > escala_b) escala_b = fabs(v);
        }
        tol_b = 1e-8 * (1.0 + escala_b);
        ops += (unsigned long)(r_neq * r_nterm);
        for (i = r_rango; i < r_neq; i++) {
            if (!fila_cero[i]) continue;
            ops += (unsigned long)r_nterm;
            if (fabs(suma_signos(&m[i][r_nunk], signos)) > tol_b) return GAUSS_INCOMPATIBLE;
        }
    }

    for (k = 0; k < r_nunk; k++) x[k] = 0.0;
    for (k = 0; k < r_rango; k++) x[piv[k]] = suma_signos(&m[k][r_nunk], signos);
    ops += (unsigned long)(r_rango * r_nterm);

    if (r_rango < r_nunk) return GAUSS_INDETERMINADO;
    return GAUSS_OK;
}
#endif

int sis_combinar_signos(unsigned signos, double *x)
{
    int k, nd = r_nterm - 1;
    unsigned c, mitad;
    const double *p;

    if (r_vacio) return GAUSS_VACIO;
#ifndef NDLESS
    if (sg_lento) return combinar_lento(signos, x);
#endif
    if (!sg_listo) sg_prepara();
    c = signos & ((1u << nd) - 1u);             /* los bits de mas no cuentan */

    /* La tolerancia de incompatibilidad solo hace falta si hay filas que se
     * quedaron sin coeficientes (rango menor que ecuaciones); las mismas
     * operaciones contadas que sumando cada juego */
    if (r_nulas) {
        ops += (unsigned long)(r_neq * r_nterm) + (unsigned long)sg_vistas[c] * (unsigned long)r_nterm;
        if (sg_incomp[c]) return GAUSS_INCOMPATIBLE;
    }

    for (k = 0; k < r_nunk; k++) x[k] = 0.0;
    if (nd == 0) for (k = 0; k < r_rango; k++) x[piv[k]] = sg_arbol[0][k];
    else {
        mitad = 1u << (nd - 1);
        p = sg_arbol[c & (mitad - 1u)];
        if (c & mitad) for (k = 0; k < r_rango; k++) x[piv[k]] = p[k] - sg_col[nd][k];
        else           for (k = 0; k < r_rango; k++) x[piv[k]] = p[k] + sg_col[nd][k];
    }
    ops += (unsigned long)(r_rango * r_nterm);

    if (r_rango < r_nunk) return GAUSS_INDETERMINADO;
    return GAUSS_OK;
}

int sis_combinar(const double *w, double *x)
{
    int i, k, t;
    double escala_b = 0.0, tol_b;

    if (r_vacio) return GAUSS_VACIO;

    for (i = 0; i < r_neq; i++) {                  /* escala del b pedido */
        double v = 0.0;
        for (t = 0; t < r_nterm; t++) if (w[t] != 0.0) v += w[t] * b_orig[t][i];
        if (fabs(v) > escala_b) escala_b = fabs(v);
    }
    tol_b = 1e-8 * (1.0 + escala_b);

    for (i = r_rango; i < r_neq; i++) {
        double v = 0.0;
        if (!fila_cero[i]) continue;
        for (t = 0; t < r_nterm; t++) if (w[t] != 0.0) v += w[t] * m[i][r_nunk + t];
        if (fabs(v) > tol_b) return GAUSS_INCOMPATIBLE;
    }

    for (k = 0; k < r_nunk; k++) x[k] = 0.0;
    for (k = 0; k < r_rango; k++) {
        double v = 0.0;
        for (t = 0; t < r_nterm; t++) if (w[t] != 0.0) v += w[t] * m[k][r_nunk + t];
        x[piv[k]] = v;
    }

    if (r_rango < r_nunk) return GAUSS_INDETERMINADO;
    return GAUSS_OK;
}

int sis_resolver(const Sistema *s, double *x, int *rango, unsigned char *libre)
{
    double w[MAX_TERM];
    int t;
    if (sis_reducir(s, rango, libre) == GAUSS_VACIO) return GAUSS_VACIO;
    for (t = 0; t < MAX_TERM; t++) w[t] = 0.0;
    w[0] = 1.0;
    return sis_combinar(w, x);
}
