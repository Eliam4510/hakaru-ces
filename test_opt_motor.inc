/* ============================================================================
 *  test_opt_motor.inc  -  [75m] El motor optimizado (rev9, fase 1, grupo
 *  motor). Lo que cambió por velocidad tiene que dar lo mismo BIT A BIT y con
 *  las mismas operaciones contadas, y cada caché nueva tiene que olvidarse en
 *  cuanto cambia algo de lo que depende.
 *
 *    M1  LU y LUC sin comparaciones de coma flotante (los bits) y con la
 *        lista de columnas no nulas de la fila pivote, M4, con las filas
 *        por punteros y el relleno, y M11, con las máscaras por fila si
 *        n > 16: contra la factorización de antes (copiada aquí tal cual:
 *        sus ==, sus >, sus memcpy de filas y sin relleno), bit a bit (perm,
 *        1/pivote, listas, solución) y con las mismas operaciones, en
 *        matrices de circuito con −0, NaN, ±∞, ceros que salen por
 *        cancelación, filas vacías (también de −0) y singulares, de n = 1 a
 *        64 (a los dos lados de 16 y de la palabra 32), y con basura en la
 *        columna de relleno
 *    M2  las cachés del paso (2C/hh y 2L/hh, las G de residuo_nodos, el valor
 *        de cada fuente) se rehacen cuando cambia la red (valores, formas,
 *        topología, el paso), en cada emulación, tras un OP de otra red que
 *        guarda y repone la de antes, y con el lado de un flanco: la red B
 *        emulada tras la A da lo mismo, bit a bit y con las mismas ops, que
 *        tras sí misma; y un divisor con una senoidal y con una cuadrada (sus
 *        dos muestras en cada flanco), un RC con escalón y la W de una R,
 *        contra sus valores exactos; el residuo de un OP con un 1N4148 (las
 *        G de su red) y el «diodo C» de test_flanco (la fuente en 0⁻ para el
 *        OP y en 0⁺ para la presonda, en el mismo instante); la Emax (la
 *        del C, ½·C·V²; la de la R y la fuente, 0), y la P y la W de un AO
 *        real con las de sus entradas (s_k = -1), también en el tramo rígido
 *    M7  lo del planificador del tramo rígido en cada modo (|s| y sus
 *        cocientes), bit a bit como las cuentas de antes
 *    M9  el barrido de CC de una red lineal que barre una E o una J: la LU
 *        (y el análisis, si no mira las fuentes) de un punto anterior, bit a
 *        bit y con las mismas ops que el barrido de siempre y que el OP
 *        completo, y nada reutilizado si cambia la red, el elemento, el tipo
 *        o lu_op
 *    M9a guarda y repone con lo usado (nn, nel, NL.n): una emulación a
 *        medias, con no lineales, no se entera de lo que se hace con otra
 *        red más grande entre dos trozos (OP, punto Q, barrido, su derivada,
 *        topología de CA, Thévenin)
 *
 *  Va al final de la batería (tras [74]): no vuelca BMP ni gasta azar().
 *  Usa sr, saj, sr_init, sr_el, sr_escalon, canal, canal_de, a4_ao, a4_cc y
 *  huella_fnv. Sus mutaciones: rev9/motor_herr/solo_motor.py.
 * ========================================================================= */

static unsigned om_sem = 20260928u;
static unsigned om_azar(unsigned n) { om_sem = om_sem * 1103515245u + 12345u; return (om_sem >> 16) % n; }
static int om_bits_igual(const void *a, const void *b, size_t n) { return memcmp(a, b, n) == 0; }

/* ---------------------------------------------------------------------------
 *  M1 y M4. La LU y la LUC contra las de antes
 * ------------------------------------------------------------------------ */
/* La LU de opt_base (lu.c antes de rev9), con sus operaciones en ops */
typedef struct {
    int n, ok, col_mala;
    double a[LU_MAX][LU_MAX];
    int perm[LU_MAX];
    double dinv[LU_MAX];
    int lini[LU_MAX + 1], uini[LU_MAX + 1];
    unsigned char lcol[LU_NNZ], ucol[LU_NNZ];
    double lval[LU_NNZ], uval[LU_NNZ];
    unsigned long ops;
} OmLu;

static int om_lu_ref(OmLu *f, double pivote_min)
{
    int n = f->n, i, j, k, nl = 0, nu = 0;
    double colmax[LU_MAX];
    f->ok = 0;
    f->col_mala = -1;
    for (j = 0; j < n; j++) colmax[j] = 0.0;
    for (i = 0; i < n; i++) {
        int vacia = 1;
        f->perm[i] = i;
        for (j = 0; j < n; j++)
            if (f->a[i][j] != 0.0) {
                vacia = 0;
                if (fabs(f->a[i][j]) > colmax[j]) colmax[j] = fabs(f->a[i][j]);
            }
        if (vacia) { f->col_mala = i; return 0; }
    }
    for (k = 0; k < n; k++) {
        int p = k;
        double mx = fabs(f->a[k][k]), piv;
        for (i = k + 1; i < n; i++)
            if (fabs(f->a[i][k]) > mx) { mx = fabs(f->a[i][k]); p = i; }
        if (!(mx > pivote_min * colmax[k])) { f->col_mala = k; return 0; }
        if (p != k) {
            double t[LU_MAX];
            int ti;
            memcpy(t, f->a[k], (size_t)n * sizeof t[0]);
            memcpy(f->a[k], f->a[p], (size_t)n * sizeof t[0]);
            memcpy(f->a[p], t, (size_t)n * sizeof t[0]);
            ti = f->perm[k]; f->perm[k] = f->perm[p]; f->perm[p] = ti;
        }
        piv = f->a[k][k];
        f->dinv[k] = 1.0 / piv;
        f->ops++;
        for (i = k + 1; i < n; i++) {
            double m = f->a[i][k];
            if (m == 0.0) continue;
            m *= f->dinv[k];
            f->a[i][k] = m;
            f->ops++;
            for (j = k + 1; j < n; j++) {
                if (f->a[k][j] == 0.0) continue;
                f->a[i][j] -= m * f->a[k][j];
                f->ops++;
            }
        }
    }
    for (i = 0; i < n; i++) {
        f->lini[i] = nl;
        for (j = 0; j < i; j++)
            if (f->a[i][j] != 0.0) { f->lcol[nl] = (unsigned char)j; f->lval[nl++] = f->a[i][j]; }
        f->uini[i] = nu;
        for (j = i + 1; j < n; j++)
            if (f->a[i][j] != 0.0) { f->ucol[nu] = (unsigned char)j; f->uval[nu++] = f->a[i][j]; }
    }
    f->lini[n] = nl;
    f->uini[n] = nu;
    f->ok = 1;
    return 1;
}

static void om_lu_ref_resuelve(OmLu *f, const double *b, double *x)
{
    double y[LU_MAX];
    int n = f->n, i, q;
    for (i = 0; i < n; i++) {
        double s = b[f->perm[i]];
        for (q = f->lini[i]; q < f->lini[i + 1]; q++) s -= f->lval[q] * y[f->lcol[q]];
        y[i] = s;
        f->ops += (unsigned long)(1 + f->lini[i + 1] - f->lini[i]);
    }
    for (i = n - 1; i >= 0; i--) {
        double s = y[i];
        for (q = f->uini[i]; q < f->uini[i + 1]; q++) s -= f->uval[q] * x[f->ucol[q]];
        x[i] = s * f->dinv[i];
        f->ops += (unsigned long)(1 + f->uini[i + 1] - f->uini[i]);
    }
}

/* La LUC de opt_base (luc.c antes de rev9) */
typedef struct {
    int n, ok, col_mala;
    Cx  a[LUC_MAX][LUC_MAX];
    int perm[LUC_MAX];
    Cx  dinv[LUC_MAX];
    int lini[LUC_MAX + 1], uini[LUC_MAX + 1];
    unsigned char lcol[LUC_NNZ], ucol[LUC_NNZ];
    Cx  lval[LUC_NNZ], uval[LUC_NNZ];
    unsigned long ops;
} OmLuc;

static int om_luc_ref(OmLuc *f)
{
    int n = f->n, i, j, k, nl = 0, nu = 0;
    double colmax[LUC_MAX];
    f->ok = 0;
    f->col_mala = -1;
    for (j = 0; j < n; j++) colmax[j] = 0.0;
    for (i = 0; i < n; i++) {
        int vacia = 1;
        f->perm[i] = i;
        for (j = 0; j < n; j++)
            if (!cx_nulo(f->a[i][j])) {
                vacia = 0;
                if (cx_abs1(f->a[i][j]) > colmax[j]) colmax[j] = cx_abs1(f->a[i][j]);
            }
        if (vacia) { f->col_mala = i; return 0; }
    }
    for (k = 0; k < n; k++) {
        int p = k;
        double mx = cx_abs1(f->a[k][k]);
        for (i = k + 1; i < n; i++)
            if (cx_abs1(f->a[i][k]) > mx) { mx = cx_abs1(f->a[i][k]); p = i; }
        if (!(mx > 1e-12 * colmax[k])) { f->col_mala = k; return 0; }
        if (p != k) {
            Cx t[LUC_MAX];
            int ti;
            memcpy(t, f->a[k], (size_t)n * sizeof t[0]);
            memcpy(f->a[k], f->a[p], (size_t)n * sizeof t[0]);
            memcpy(f->a[p], t, (size_t)n * sizeof t[0]);
            ti = f->perm[k]; f->perm[k] = f->perm[p]; f->perm[p] = ti;
        }
        f->dinv[k] = cx_inv(f->a[k][k]);
        f->ops++;
        for (i = k + 1; i < n; i++) {
            Cx m = f->a[i][k];
            if (cx_nulo(m)) continue;
            m = cx_por(m, f->dinv[k]);
            f->a[i][k] = m;
            f->ops++;
            for (j = k + 1; j < n; j++) {
                if (cx_nulo(f->a[k][j])) continue;
                f->a[i][j] = cx_menos(f->a[i][j], cx_por(m, f->a[k][j]));
                f->ops++;
            }
        }
    }
    for (i = 0; i < n; i++) {
        f->lini[i] = nl;
        for (j = 0; j < i; j++)
            if (!cx_nulo(f->a[i][j])) { f->lcol[nl] = (unsigned char)j; f->lval[nl++] = f->a[i][j]; }
        f->uini[i] = nu;
        for (j = i + 1; j < n; j++)
            if (!cx_nulo(f->a[i][j])) { f->ucol[nu] = (unsigned char)j; f->uval[nu++] = f->a[i][j]; }
    }
    f->lini[n] = nl;
    f->uini[n] = nu;
    f->ok = 1;
    return 1;
}

static void om_luc_ref_resuelve(OmLuc *f, const Cx *b, Cx *x)
{
    Cx y[LUC_MAX];
    int n = f->n, i, q;
    for (i = 0; i < n; i++) {
        Cx s = b[f->perm[i]];
        for (q = f->lini[i]; q < f->lini[i + 1]; q++) s = cx_menos(s, cx_por(f->lval[q], y[f->lcol[q]]));
        y[i] = s;
        f->ops += (unsigned long)(1 + f->lini[i + 1] - f->lini[i]);
    }
    for (i = n - 1; i >= 0; i--) {
        Cx s = y[i];
        for (q = f->uini[i]; q < f->uini[i + 1]; q++) s = cx_menos(s, cx_por(f->uval[q], x[f->ucol[q]]));
        x[i] = cx_por(s, f->dinv[i]);
        f->ops += (unsigned long)(1 + f->uini[i + 1] - f->uini[i]);
    }
}

/* Lo que sale de una factorización, igual bit a bit: el estado, la columna
 * mala, perm (también a medias, en una singular), los 1/pivote, las listas
 * y las operaciones */
#define OM_IGUAL_LU(F, G, OPS_F, T) \
    ((F)->ok == (G)->ok && (F)->col_mala == (G)->col_mala && (OPS_F) == (G)->ops && \
     om_bits_igual((F)->perm, (G)->perm, (size_t)(F)->n * sizeof(int)) && \
     (!(F)->ok || (om_bits_igual((F)->dinv, (G)->dinv, (size_t)(F)->n * sizeof(T)) && \
                   om_bits_igual((F)->lini, (G)->lini, (size_t)((F)->n + 1) * sizeof(int)) && \
                   om_bits_igual((F)->uini, (G)->uini, (size_t)((F)->n + 1) * sizeof(int)) && \
                   om_bits_igual((F)->lcol, (G)->lcol, (size_t)(F)->lini[(F)->n]) && \
                   om_bits_igual((F)->ucol, (G)->ucol, (size_t)(F)->uini[(F)->n]) && \
                   om_bits_igual((F)->lval, (G)->lval, (size_t)(F)->lini[(F)->n] * sizeof(T)) && \
                   om_bits_igual((F)->uval, (G)->uval, (size_t)(F)->uini[(F)->n] * sizeof(T)))))

static LU om_lu;
static OmLu om_ref;
static LUC om_luc;
static OmLuc om_refc;

/* Un número de circuito: de 1e-9 a 1e6, con su signo */
static double om_valor(void)
{
    double v = pow(10.0, -9.0 + 15.0 * (double)om_azar(1000) / 999.0);
    return om_azar(2) ? v : -v;
}

/* Las rarezas de cada caso (bits de extra): −0 en los huecos, un NaN, un
 * ±∞, una fila que repite otra por 2 (ceros exactos al eliminar y, si es
 * entera, singular), una fila vacía (de ceros o de −0), un pivote de 1e-13
 * de su columna, basura en el relleno */
enum { OM_MENOS0 = 1, OM_NAN = 2, OM_INF = 4, OM_DOBLE = 8, OM_VACIA = 16, OM_TINY = 32, OM_RELLENO = 64 };

/* La matriz m (n x n, con sus rarezas) de un circuito: dispersa, diagonal
 * dominante o no según el caso */
static void om_matriz(double m[LU_MAX][LU_MAX], int n, int extra)
{
    int i, j, v;
    for (i = 0; i < n; i++) for (j = 0; j < n; j++) m[i][j] = 0.0;
    for (i = 0; i < n; i++) {
        double g = om_valor();
        int vecinos = 1 + (int)om_azar(4);
        m[i][i] += (om_azar(4) ? 1.0 : 0.0) * g * (double)(vecinos + 1);   /* a veces sin diagonal: pivoteo */
        for (v = 0; v < vecinos; v++) {
            j = (int)om_azar((unsigned)n);
            if (j == i) continue;
            m[i][j] -= g;
            m[j][i] += om_valor();
        }
    }
    if ((extra & OM_DOBLE) && n > 2) {                    /* 2 × otra en una parte (o entera) */
        int a = (int)om_azar((unsigned)n), b = (int)om_azar((unsigned)n), hasta = om_azar(2) ? n : n / 2;
        if (a != b) for (j = 0; j < hasta; j++) m[b][j] = 2.0 * m[a][j];
    }
    if (extra & OM_TINY) {                                 /* un pivote de 1e-13 de su columna */
        int c = (int)om_azar((unsigned)n);
        for (i = 0; i < n; i++) if (i != c && m[i][c] != 0.0) m[i][c] = 0.0;
        m[c][c] = 1.0;
        if (n > 1) { m[(c + 1) % n][c] = 1e13; m[c][c] = 1.0; }
    }
    if (extra & OM_MENOS0)
        for (i = 0; i < n; i++) for (j = 0; j < n; j++) if (m[i][j] == 0.0 && om_azar(3)) m[i][j] = -0.0;
    if (extra & OM_NAN) m[om_azar((unsigned)n)][om_azar((unsigned)n)] = NAN;
    if (extra & OM_INF) m[om_azar((unsigned)n)][om_azar((unsigned)n)] = om_azar(2) ? INFINITY : -INFINITY;
    if (extra & OM_VACIA) {
        int r = (int)om_azar((unsigned)n);
        for (j = 0; j < n; j++) m[r][j] = om_azar(2) ? -0.0 : 0.0;
    }
}

static void test_opt_lu(void)
{
    static double m[LU_MAX][LU_MAX], b[LU_MAX], x[LU_MAX], xr[LU_MAX];
    static Cx cb[LUC_MAX], cxs[LUC_MAX], cxr[LUC_MAX];
    int caso, total = 0, iguales = 0, totc = 0, igc = 0, singulares = 0, con_nan = 0, con_m0 = 0, piv = 0, n64 = 0;
    int i, j, sol = 0, solc = 0;

    for (caso = 0; caso < 900; caso++) {
        int n = caso < 20 ? 1 + caso % 3 : caso < 40 ? LU_MAX : 1 + (int)om_azar(LU_MAX);
        int extra = (int)om_azar(128) & (caso % 3 ? 0x7F : 0x41);   /* uno de cada 3, con todas las rarezas */
        double pmin = caso % 5 == 4 ? 1e-20 : 1e-12;            /* la del Newton (PIVOTE_NL) y la de siempre */
        unsigned long o0;
        int r1, r2;
        om_matriz(m, n, extra);
        lu_empieza(&om_lu, n);
        memset(om_lu.perm, 0, sizeof om_lu.perm);          /* tras una fila vacía, perm queda a medias */
        memset(&om_ref, 0, sizeof om_ref);
        om_ref.n = n;
        for (i = 0; i < n; i++) {
            for (j = 0; j < n; j++) om_lu.a[i][j] = om_ref.a[i][j] = m[i][j];
            om_lu.a[i][LU_MAX] = (extra & OM_RELLENO) ? NAN : 1e300;           /* el relleno no cuenta */
            if (n < LU_MAX) om_lu.a[i][n] = (extra & OM_RELLENO) ? -INFINITY : 7.0;   /* ni lo de más allá de n */
        }
        o0 = lu_ops();
        r1 = lu_factoriza_tol(&om_lu, pmin);
        r2 = om_lu_ref(&om_ref, pmin);
        total++;
        if (r1 == r2 && OM_IGUAL_LU(&om_lu, &om_ref, lu_ops() - o0, double)) iguales++;
        else if (total - iguales <= 3) printf("  M1: LU distinta en el caso %d (n %d, rarezas 0x%x)\n", caso, n, (unsigned)extra);
        if (!r2) singulares++;
        if (extra & OM_NAN) con_nan++;
        if (extra & (OM_MENOS0 | OM_VACIA)) con_m0++;
        for (i = 0; r2 && i < n; i++) if (om_ref.perm[i] != i) { piv++; break; }
        if (n == LU_MAX) n64++;
        if (r1 && r2) {                                    /* y la sustitución con ellas */
            unsigned long o1;
            for (i = 0; i < n; i++) b[i] = om_valor();
            o1 = lu_ops();
            lu_resuelve(&om_lu, b, x);
            o1 = lu_ops() - o1;
            om_ref.ops = 0;
            om_lu_ref_resuelve(&om_ref, b, xr);
            if (om_bits_igual(x, xr, (size_t)n * sizeof x[0]) && o1 == om_ref.ops) sol++;
            else sol = -100000;
        }
    }
    chk_cond("M1", "LU: 900 matrices de circuito, igual bit a bit que la de antes (perm, 1/pivote, listas y operaciones)",
             iguales == total);
    chk_cond("M1", "LU: la sustituci" S_oo "n con ellas, igual bit a bit y con las mismas operaciones", sol > 0);
    chk_cond("M1", "LU: los casos cubren singulares, NaN, -0, pivoteo y n = 64",
             singulares > 50 && con_nan > 50 && con_m0 > 50 && piv > 100 && n64 >= 20);

    /* La LUC: complejas al azar, reales (im = ±0) y con las mismas rarezas en
     * una parte o en las dos */
    for (caso = 0; caso < 500; caso++) {
        int n = caso < 10 ? LUC_MAX : 1 + (int)om_azar(caso % 4 ? 40 : LUC_MAX);
        int extra = (int)om_azar(128) & (caso % 3 ? 0x7F : 0x41), real = caso % 4 == 1;
        unsigned long o0;
        int r1, r2;
        om_matriz(m, n, extra);
        luc_empieza(&om_luc, n);
        memset(om_luc.perm, 0, sizeof om_luc.perm);
        memset(&om_refc, 0, sizeof om_refc);
        om_refc.n = n;
        for (i = 0; i < n; i++) {
            for (j = 0; j < n; j++) {
                Cx z;
                z.re = m[i][j];
                z.im = real ? (om_azar(2) ? 0.0 : -0.0) : m[i][j] == 0.0 ? (om_azar(3) ? m[i][j] : om_valor()) : om_valor();
                if (!real && m[i][j] == 0.0 && (extra & OM_MENOS0) && om_azar(2)) z.im = -0.0;   /* −0 en las dos partes */
                om_luc.a[i][j] = om_refc.a[i][j] = z;
            }
            om_luc.a[i][LUC_MAX] = cx((extra & OM_RELLENO) ? NAN : 1e300, 1.0);
            if (n < LUC_MAX) om_luc.a[i][n] = cx(7.0, (extra & OM_RELLENO) ? INFINITY : 0.0);
        }
        o0 = luc_ops();
        r1 = luc_factoriza(&om_luc);
        r2 = om_luc_ref(&om_refc);
        totc++;
        if (r1 == r2 && OM_IGUAL_LU(&om_luc, &om_refc, luc_ops() - o0, Cx)) igc++;
        else if (totc - igc <= 3) printf("  M1: LUC distinta en el caso %d (n %d, rarezas 0x%x)\n", caso, n, (unsigned)extra);
        if (r1 && r2) {
            unsigned long o1;
            for (i = 0; i < n; i++) cb[i] = cx(om_valor(), real ? 0.0 : om_valor());
            o1 = luc_ops();
            luc_resuelve(&om_luc, cb, cxs);
            o1 = luc_ops() - o1;
            om_refc.ops = 0;
            om_luc_ref_resuelve(&om_refc, cb, cxr);
            if (om_bits_igual(cxs, cxr, (size_t)n * sizeof cxs[0]) && o1 == om_refc.ops) solc++;
            else solc = -100000;
        }
    }
    chk_cond("M1", "LUC: 500 matrices (complejas, reales y con rarezas), igual bit a bit que la de antes", igc == totc);
    chk_cond("M1", "LUC: la sustituci" S_oo "n con ellas, igual bit a bit y con las mismas operaciones", solc > 0);

    /* Un pivote NaN bajo la diagonal no se elige (fabs(NaN) > x es falso) y
     * uno en la diagonal hace la columna singular, como antes */
    lu_empieza(&om_lu, 3);
    lu_suma(&om_lu, 0, 0, 1.0); lu_suma(&om_lu, 1, 1, 1.0); lu_suma(&om_lu, 2, 2, 1.0);
    om_lu.a[1][0] = NAN; om_lu.a[2][0] = 0.5;
    chk_cond("M1", "LU: un NaN bajo la diagonal no es el pivote", lu_factoriza(&om_lu) && om_lu.perm[0] == 0);
    lu_empieza(&om_lu, 2);
    om_lu.a[0][0] = NAN; om_lu.a[1][0] = 2.0; om_lu.a[1][1] = 1.0; om_lu.a[0][1] = 1.0;
    chk_cond("M1", "LU: con NaN en la diagonal, nada le gana: columna 0 singular", !lu_factoriza(&om_lu) && om_lu.col_mala == 0);
    lu_empieza(&om_lu, 2);
    om_lu.a[0][0] = -0.0; om_lu.a[0][1] = -0.0; om_lu.a[1][1] = 1.0;
    chk_cond("M1", "LU: una fila de -0 es una fila vac" S_ii "a", !lu_factoriza(&om_lu) && om_lu.col_mala == 0);
}

/* ---------------------------------------------------------------------------
 *  M2. Las cachés del paso
 * ------------------------------------------------------------------------ */
static SimRed om_ra, om_rb;
static SimAjustes om_aa, om_ab;

/* La huella de la última emulación: las muestras y sus integrales, todas las
 * medidas, el paso, la ventana, los contadores y el estado de Newton */
static unsigned long long om_huella(void)
{
    const SimRes *r = sim_res();
    const SimReg *g = sim_reg();
    unsigned long long h = 1469598103934665603ULL;
    int c;
    h = huella_fnv(h, g->t, sizeof(double) * (size_t)g->n);
    for (c = 0; c < 2; c++) {
        h = huella_fnv(h, g->y[c], sizeof(double) * (size_t)g->n);
        h = huella_fnv(h, g->s1[c], sizeof(double) * (size_t)g->n);
        h = huella_fnv(h, g->s2[c], sizeof(double) * (size_t)g->n);
    }
    h = huella_fnv(h, r->Vmax, sizeof r->Vmax); h = huella_fnv(h, r->Imax, sizeof r->Imax);
    h = huella_fnv(h, r->Pmax, sizeof r->Pmax); h = huella_fnv(h, r->W, sizeof r->W);
    h = huella_fnv(h, r->Vmed, sizeof r->Vmed); h = huella_fnv(h, r->Vef, sizeof r->Vef);
    h = huella_fnv(h, r->Imed, sizeof r->Imed); h = huella_fnv(h, r->Ief, sizeof r->Ief);
    h = huella_fnv(h, r->Pmed, sizeof r->Pmed); h = huella_fnv(h, r->Emax, sizeof r->Emax);
    h = huella_fnv(h, r->Efin, sizeof r->Efin); h = huella_fnv(h, r->lectura, sizeof r->lectura);
    h = huella_fnv(h, &r->ultimo, sizeof r->ultimo); h = huella_fnv(h, &r->op, sizeof r->op);
    h = huella_fnv(h, &r->dif_balance, sizeof r->dif_balance);
    h = huella_fnv(h, &r->h, sizeof r->h); h = huella_fnv(h, &r->t_fin, sizeof r->t_fin);
    h = huella_fnv(h, &r->t_est, sizeof r->t_est); h = huella_fnv(h, &r->pasos, sizeof r->pasos);
    h = huella_fnv(h, &r->nw.iter, sizeof r->nw.iter); h = huella_fnv(h, &r->nw.sub, sizeof r->nw.sub);   /* campo a campo: */
    h = huella_fnv(h, &r->nw.iter_op, sizeof r->nw.iter_op); h = huella_fnv(h, &r->nw.sonda, sizeof r->nw.sonda);   /* sin el relleno */
    h = huella_fnv(h, &r->nw.residuo_max, sizeof r->nw.residuo_max);
    h = huella_fnv(h, &r->d.estado, sizeof r->d.estado); h = huella_fnv(h, &r->d.avisos, sizeof r->d.avisos);
    return h;
}

/* Emula s con aj: su huella (0 si no dio SIM_OK) y las operaciones */
static unsigned long long om_emula(const SimRed *s, const SimAjustes *aj, unsigned long *ops)
{
    unsigned long o0 = sim_ops();
    int e = sim_emular(s, aj);
    *ops = sim_ops() - o0;
    return e == SIM_OK && sim_res()->fin == FIN_COMPLETO ? om_huella() : 0;
}

/* Una fuente de forma f en sr: la senoidal (A, f, fase 50°), la cuadrada
 * (ciclo 50 %) o el pulso (escalón de 0 a A) */
static int om_fuente(int tipo, int a, int b, int forma, double A, double f)
{
    int k = sr_el(tipo, a, b, 0.0);
    SimEl *e = &sr.el[k];
    e->forma = (unsigned char)forma;
    e->p[0] = forma == ONDA_PULSO ? 0.0 : A; e->p[1] = forma == ONDA_PULSO ? A : f;
    e->p[2] = forma == ONDA_PULSO ? 0.0 : 50.0; e->p[3] = 0.0; e->p[4] = 0.0;
    return k;
}

/* Un 1N4148 del catálogo en sr */
static int om_diodo(int a, int b)
{
    int k = sr_el(SE_D, a, b, 0.0);
    SimEl *x = &sr.el[k];
    x->modelo = NL_UNION;
    x->m[0] = NL_1N4148.Is; x->m[1] = NL_1N4148.n; x->m[2] = NL_1N4148.Rs;
    x->m[5] = nl_vcrit(x->m[0], x->m[1] * NL_VT);
    return k;
}

/* La variante v de la familia f (v = 0: la A; las demás, la B de un par que
 * cambia una cosa), en sr y saj. 0 si no hay tal variante */
static int om_red(int f, int v)
{
    int k;
    switch (f) {
    case 0:                                       /* RC con escalón: C, R, el paso */
        if (v > 3) return 0;
        sr_init(3);
        sr_escalon(0, 1, 10.0);
        sr_el(SE_R, 1, 2, v == 2 ? 2.2e3 : 1e3);
        sr_el(SE_C, 2, 0, v == 1 ? 2.2e-6 : 1e-6);
        canal(0, CANAL_V, 2); canal(1, CANAL_I, 1);
        saj.t_fin = 5e-3;
        if (v == 3) saj.h = 3e-6;
        return 1;
    case 1:                                       /* RL con escalón: L */
        if (v > 1) return 0;
        sr_init(3);
        sr_escalon(0, 1, 5.0);
        sr_el(SE_R, 1, 2, 100.0);
        sr_el(SE_L, 2, 0, v ? 22e-3 : 10e-3);
        canal(0, CANAL_I, 2); canal(1, CANAL_V, 1);
        return 1;
    case 2:                                       /* RC con la fuente: A, f, la forma */
        if (v > 3) return 0;
        sr_init(3);
        om_fuente(SE_E, 0, 1, v == 3 ? ONDA_CUADRADA : ONDA_SENO, v == 1 ? 7.0 : 5.0, v == 2 ? 1.3e3 : 1e3);
        sr_el(SE_R, 1, 2, 1e3);
        sr_el(SE_C, 2, 0, 100e-9);
        canal(0, CANAL_V, 2); canal(1, CANAL_I, 1);
        saj.t_fin = 4e-3;
        return 1;
    case 3:                                       /* la topología: C o L en el mismo sitio */
        if (v > 1) return 0;
        sr_init(3);
        om_fuente(SE_E, 0, 1, ONDA_SENO, 5.0, 1e3);
        sr_el(SE_R, 1, 2, 1e3);
        sr_el(v ? SE_L : SE_C, 2, 0, v ? 10e-3 : 100e-9);
        canal(0, CANAL_V, 2); canal(1, CANAL_I, 2);
        saj.t_fin = 4e-3;
        return 1;
    case 4:                                       /* una J (escalón): su valor */
        if (v > 1) return 0;
        sr_init(2);
        om_fuente(SE_J, 0, 1, ONDA_PULSO, v ? 2e-3 : 1e-3, 0.0);
        sr_el(SE_R, 1, 0, 1e3);
        sr_el(SE_C, 1, 0, 1e-6);
        canal(0, CANAL_V, 2); canal(1, CANAL_I, 1);
        saj.t_fin = 5e-3;
        return 1;
    case 5:                                       /* rectificador con 1N4148: la R (tabla_inv) */
        if (v > 1) return 0;
        sr_init(3);
        om_fuente(SE_E, 0, 1, ONDA_SENO, 10.0, 60.0);
        om_diodo(1, 2);
        sr_el(SE_R, 2, 0, v ? 2.2e3 : 1e3);
        sr_el(SE_C, 2, 0, 10e-6);
        canal(0, CANAL_V, 3); canal(1, CANAL_I, 1);
        saj.t_fin = 1.0 / 30.0;
        return 1;
    case 6:                                       /* inversor con un AO real: Ro y Ri (tabla_inv) */
        if (v > 1) return 0;
        sr_init(4);
        om_fuente(SE_E, 0, 1, ONDA_SENO, 1.0, 1e3);
        sr_el(SE_R, 1, 2, 1e3);
        sr_el(SE_R, 2, 3, 10e3);
        k = a4_ao(0, 2, 3, 0, 1, 1e5);
        sr.el[k].m[3] = v ? 2e6 : 1e6; sr.el[k].m[4] = v ? 150.0 : 75.0;
        sr_el(SE_R, 3, 0, 10e3);
        sr_el(SE_C, 3, 0, 1e-9);
        canal(0, CANAL_NODO, 3); canal(1, CANAL_I, 1);
        saj.t_fin = 3e-3;
        return 1;
    default:
        return 0;
    }
}

/* Los pares: la B tras la A y tras sí misma */
static void test_opt_pares(void)
{
    static const char *NOM[7] = { "RC (C, R, h)", "RL (L)", "RC con fuente (A, f, forma)", "C o L en el mismo sitio",
                                  "J", "rectificador (R)", "AO real (Ro, Ri)" };
    int f, v, bien = 0, total = 0, distintas = 0;
    for (f = 0; f < 7; f++) {
        unsigned long oa, ob1, ob2;
        unsigned long long ha, hb1, hb2;
        om_red(f, 0); om_ra = sr; om_aa = saj;
        for (v = 1; om_red(f, v); v++) {
            om_rb = sr; om_ab = saj;
            ha = om_emula(&om_ra, &om_aa, &oa);
            hb1 = om_emula(&om_rb, &om_ab, &ob1);
            hb2 = om_emula(&om_rb, &om_ab, &ob2);
            total++;
            if (ha && hb1 && hb1 == hb2 && ob1 == ob2) bien++;
            else if (total - bien <= 3) printf("  M2: %s, variante %d: tras la A %016llx (%lu ops), tras s" S_ii " misma %016llx (%lu ops)\n",
                                               NOM[f], v, hb1, ob1, hb2, ob2);
            if (ha != hb1) distintas++;
        }
    }
    chk_cond("M2", "11 pares de redes: la B tras la A = la B tras s" S_ii " misma (bit a bit, mismas ops)", bien == total && total == 11);
    chk_cond("M2", "y cada B es de verdad otra red que su A", distintas == total);
}

/* El punto de operación: la B tras la A (una E de 5 V que pasa a 7; un
 * 1N4148 con 1 kΩ que pasa a 2,2 kΩ) y un OP de otra red por sim_op_cc_red
 * (guarda y repone la de antes) entre dos de la A */
static void om_divisor(double E)
{
    sr_init(3);
    om_fuente(SE_E, 0, 1, ONDA_CC, E, 0.0);
    sr_el(SE_R, 1, 2, 1e3);
    sr_el(SE_R, 2, 0, 3e3);
}

static void om_rect_cc(double R)
{
    sr_init(3);
    om_fuente(SE_E, 0, 1, ONDA_CC, 5.0, 0.0);
    om_diodo(1, 2);
    sr_el(SE_R, 2, 0, R);
}

/* Los contadores de Newton, campo a campo (el residuo, por sus bits) */
static int om_nw_igual(const SimNewton *a, const SimNewton *b)
{
    return a->iter == b->iter && a->iter_max == b->iter_max && a->sub == b->sub && a->eventos == b->eventos &&
           a->homotopia == b->homotopia && a->iter_op == b->iter_op && a->sonda == b->sonda &&
           om_bits_igual(&a->residuo_max, &b->residuo_max, sizeof a->residuo_max);
}

static void test_opt_op(void)
{
    static Valores va1, vb1, vb2, vx;
    SimDiag d;
    SimNewton n1, n2;
    int ok;
    /* el divisor: 5 V y luego 7 V (la fuente, en 0⁻ las dos) */
    om_divisor(5.0); om_ra = sr;
    om_divisor(7.0); om_rb = sr;
    ok = sim_op_red(&om_ra, &va1, &d) == SIM_OK;
    ok = ok && sim_op_red(&om_rb, &vb1, &d) == SIM_OK && sim_op_red(&om_rb, &vb2, &d) == SIM_OK;
    chk_cond("M2", "OP: 7 V tras 5 V = 7 V tras 7 V, bit a bit", ok && om_bits_igual(&vb1, &vb2, sizeof vb1));
    chk_t("M2", "OP: el divisor de 7 V da 5,25 V", vb1.V[2], 5.25, 5.25, 1e-14);
    /* el rectificador en CC: la R de residuo_nodos (tabla_inv) */
    om_rect_cc(1e3); om_ra = sr;
    om_rect_cc(2.2e3); om_rb = sr;
    memset(&n1, 0, sizeof n1); memset(&n2, 0, sizeof n2);
    ok = sim_op_cc_red(&om_ra, &va1, &d, &n1) == SIM_OK;
    memset(&n1, 0, sizeof n1);
    ok = ok && sim_op_cc_red(&om_rb, &vb1, &d, &n1) == SIM_OK && sim_op_cc_red(&om_rb, &vb2, &d, &n2) == SIM_OK;
    chk_cond("M2", "OP con un 1N4148: 2,2 k" S_OHM " tras 1 k" S_OHM " = tras s" S_ii " misma (valores, iteraciones y residuo)",
             ok && om_bits_igual(&vb1, &vb2, sizeof vb1) && om_nw_igual(&n1, &n2));
    /* y su residuo de Kirchhoff, con las G de esta red (con las de otra, la
     * R de la otra no cuadraría en el nodo) */
    chk_cond("M2", "OP con un 1N4148: residuo de Kirchhoff <= 1e-12 (las G de su red)", ok && n1.residuo_max <= 1e-12);
    /* guarda y repone: sim_op_cc_red resuelve otra red y repone la de antes;
     * el OP de esa (por sim_red(): R_ no se vuelve a copiar) tiene que ser el
     * suyo. Sin olvidar las cachés al reponer se quedarían la fuente y la R
     * de la otra (7 V, 2,2 kΩ) */
    om_divisor(5.0); om_ra = sr;
    om_divisor(7.0); om_rb = sr;
    om_rb.el[2].val = 2.2e3;
    ok = sim_op_red(&om_ra, &va1, &d) == SIM_OK;
    ok = ok && sim_op_cc_red(&om_rb, &vb1, &d, 0) == SIM_OK;
    ok = ok && sim_op_red(sim_red(), &vx, &d) == SIM_OK;
    chk_cond("M2", "guarda y repone: el OP de la red repuesta es el suyo, bit a bit", ok && om_bits_igual(&va1, &vx, sizeof vx));
    om_rect_cc(1e3); om_ra = sr;
    om_rect_cc(2.2e3); om_rb = sr;
    om_rb.el[0].p[0] = 7.0;
    ok = sim_op_red(&om_ra, &va1, &d) == SIM_OK;
    ok = ok && sim_op_cc_red(&om_rb, &vb1, &d, 0) == SIM_OK;
    ok = ok && sim_op_red(sim_red(), &vx, &d) == SIM_OK;
    chk_cond("M2", "guarda y repone con un 1N4148 (Newton): el OP de la repuesta es el suyo",
             ok && om_bits_igual(&va1, &vx, sizeof vx));
}

/* Contra los valores exactos: un divisor (1 k y 3 k) con una senoidal (cada
 * muestra, E(t)·3/4; la W de la R de 3 k) y con una cuadrada (en cada flanco
 * dos muestras: el valor de antes y el de después); un RC con escalón (el
 * del paso: con 2C/hh de otro paso, el del arranque, se iría lejos) */
/* El divisor de 1 k y 3 k con la fuente de forma f: cuántas muestras no son
 * E(t)·3/4 (en un instante con dos, la primera es el límite por la
 * izquierda: 0⁻ en t = 0, el valor de antes en un flanco; la última, en
 * t_fin, también, por si cae en un flanco); en *dobles, esos instantes. La
 * segunda de las dos es la y+ extrapolada del arranque: exacta con una
 * fuente constante a trozos, no con una senoidal (ahí no se mira) */
static int om_divisor_malas(int forma, double A, double T, int *dobles)
{
    const SimReg *g;
    int i, malas = 0, ymas = 0;
    sr_init(3);
    om_fuente(SE_E, 0, 1, forma, A, 1.0 / T);
    sr_el(SE_R, 1, 2, 1e3);
    sr_el(SE_R, 2, 0, 3e3);
    canal(0, CANAL_V, 2); canal(1, CANAL_NADA, 0);
    saj.t_fin = 3.0 * T;
    emula("M2 divisor");
    g = sim_reg();
    *dobles = 0;
    for (i = 0; i < g->n; i++) {
        double t = g->t[i], e;
        int doble = i + 1 < g->n && g->t[i + 1] == t, antes = doble || i == g->n - 1, es_ymas = ymas;
        ymas = doble;
        e = 0.75 * onda_valor_lado(forma, sr.el[0].p, t, antes ? -1 : 1);
        if (doble) (*dobles)++;
        if (es_ymas && forma == ONDA_SENO) continue;
        if (!(fabs(g->y[0][i] - e) <= 1e-13 * A)) {
            if (++malas <= 3) printf("  M2: divisor (forma %d), muestra %d en t = %.17g: %.17g (y %.17g)\n", forma, i, t, g->y[0][i], e);
        }
    }
    return malas;
}

static void test_opt_exactos(void)
{
    const SimReg *g;
    const SimRes *r;
    double peor = 0.0, W, T = 1e-3, A = 4.0, tf;
    int i, dobles, malas;
    /* la senoidal (con fase 50°: salta en 0⁺) */
    malas = om_divisor_malas(ONDA_SENO, A, T, &dobles);
    chk_cond("M2", "divisor con senoidal: cada muestra es E(t)" S_CDOT "3/4 (en 0-, la de reposo)", malas == 0 && dobles == 1);
    r = sim_res();
    tf = r->t_fin;
    /* ∫(0,75·A·sen(ωt + φ))²/3k; el trapecio con 100 pasos por periodo (y el
     * arranque), al 1e-4 */
    W = 0.5625 * A * A / 3e3 * (0.5 * tf - (sin(2.0 * (2.0 * PI_T * tf / T + 50.0 * PI_T / 180.0)) - sin(2.0 * 50.0 * PI_T / 180.0)) /
                                (4.0 * 2.0 * PI_T / T));
    chk_t("M2", "divisor con senoidal: la W de la R de 3 k" S_OHM, r->W[2], W, W, 1e-4);
    /* la cuadrada: en 0 y en cada flanco (cada T/2), el valor de antes y el de después */
    malas = om_divisor_malas(ONDA_CUADRADA, A, T, &dobles);
    chk_cond("M2", "divisor con cuadrada: las muestras y, en cada flanco, la de antes y la de despu" S_ee "s",
             malas == 0 && dobles >= 6);
    /* el RC: 10 V por 1 k sobre 1 µF, τ = 1 ms, 5 ms */
    om_red(0, 0);
    emula("M2 RC con escalón");
    g = sim_reg();
    peor = 0.0;
    for (i = 0; i < g->n; i++) {
        double e = fabs(g->y[0][i] - 10.0 * (1.0 - exp(-g->t[i] / 1e-3)));
        if (g->t[i] > 0.0 && e > peor) peor = e;
    }
    chk_t("M2", "RC con escal" S_oo "n: V(C) contra 10" S_CDOT "(1 - e^(-t/" S_TAU "))", peor, 0.0, 10.0, 1e-5);
    r = sim_res();
    chk_cond("M2", "RC con escal" S_oo "n: la Emax del C es C" S_CDOT "V" S_SUP2 "/2 de su V mayor; la de la R y la fuente, 0",
             r->Emax[2] == 0.5 * 1e-6 * r->Vmax[2] * r->Vmax[2] && r->Vmax[2] > 9.9 && r->Emax[0] == 0.0 && r->Emax[1] == 0.0);
    /* El lado de la fuente en el mismo instante: el OP pide la cuadrada en
     * 0⁻ (su reposo, 0 V) y la presonda de la subida (§17.3), justo después,
     * en 0⁺ (5 V): con la de 0⁻, el diodo seguiría en corte y la subida no
     * llevaría su tramo rígido. El «diodo C» de test_flanco (1 nF por 100 Ω,
     * 100 k de carga, 20 kHz): un tramo en cada subida y ninguno en las bajadas */
    fl_diodo_c(20e3, 10e-3, 0.0, 100.0, 1e-9, 100e3);
    emula("M2 diodo C");
    r = sim_res();
    chk_cond("M2", "diodo C: la presonda en 0+ tras el OP en 0- (un tramo en cada subida)",
             r->fin == FIN_COMPLETO && r->tramos > 0 && 2 * r->tramos == r->eventos);
    /* La P de un AO real (registra, con s_k = -1 entero): entrega V·I y
     * absorbe V2·I2 por sus entradas. Saturado, con 1 V entre sus entradas de
     * 1 kΩ (1 mW, frente a los 22 de la salida), en una red de continua: la
     * de cada muestra es la del punto de operación, y su W, P·t */
    {
        static Valores vq;
        SimDiag d;
        int k, ok;
        sr_init(3);
        a4_cc(0, 1, 1.0);
        k = a4_ao(1, 0, 2, 0, 1, 1e5);
        sr.el[k].m[3] = 1e3; sr.el[k].m[4] = 75.0;
        sr_el(SE_R, 2, 0, 10e3);
        canal(0, CANAL_NODO, 2); canal(1, CANAL_NADA, 0);
        saj.t_fin = 1e-3;
        om_ra = sr;
        ok = sim_op_red(&om_ra, &vq, &d) == SIM_OK;
        emula("M2 AO saturado");
        r = sim_res();
        chk_cond("M2", "AO saturado: el OP y su segundo puerto (1 mW)", ok && fabs(vq.V2[k] * vq.I2[k] - 1e-3) < 1e-4);
        chk_t("M2", "AO saturado: su Pmax es la P del OP (V" S_CDOT "I - V2" S_CDOT "I2)", r->Pmax[k], vq.P[k], vq.P[k], 1e-9);
        chk_t("M2", "AO saturado: su W es P" S_CDOT "t", r->W[k], vq.P[k] * r->t_fin, vq.P[k] * r->t_fin, 1e-6);
    }
    /* Y en el tramo rígido (rq_suma): el mismo AO, con Ri = 100 Ω (10 mW en
     * sus entradas, frente a 22 en la salida), una cuadrada de ±1 V en su
     * entrada (satura arriba y abajo, con la misma P) y 10 pF en su salida
     * (τ = 0,74 ns: cada flanco lleva su tramo). La W, P·t: sale a 6e-6 (la
     * carga del C en cada flanco); con la P de las entradas del revés en los
     * tramos, a 4,9e-3 */
    {
        static Valores vq;
        SimDiag d;
        int k, ok;
        sr_init(3);
        a4_cc(0, 1, 1.0);
        k = a4_ao(1, 0, 2, 0, 1, 1e5);
        sr.el[k].m[3] = 100.0; sr.el[k].m[4] = 75.0;
        sr_el(SE_R, 2, 0, 10e3);
        sr_el(SE_C, 2, 0, 10e-12);
        ok = sim_op_red(&sr, &vq, &d) == SIM_OK;               /* la P con +1 V (con -1 V, la misma) */
        sr.el[0].forma = ONDA_CUADRADA; sr.el[0].p[1] = 1e3; sr.el[0].p[2] = 50.0; sr.el[0].p[3] = 0.0;
        canal(0, CANAL_NODO, 2); canal(1, CANAL_NADA, 0);
        saj.t_fin = 3e-3;
        emula("M2 AO con cuadrada");
        r = sim_res();
        if (!(ok && r->tramos >= 6 && fabs(r->W[k] - vq.P[k] * r->t_fin) <= 5e-4 * vq.P[k] * r->t_fin))
            printf("  M2: AO con cuadrada: %d tramos, W %.12g, P" S_CDOT "t %.12g\n", r->tramos, r->W[k], vq.P[k] * r->t_fin);
        chk_cond("M2", "AO saturado arriba y abajo: un tramo r" S_ii "gido en cada flanco", ok && r->tramos >= 6);
        chk_t("M2", "AO saturado arriba y abajo: su W, con la de los tramos, es P" S_CDOT "t", r->W[k], vq.P[k] * r->t_fin,
              vq.P[k] * r->t_fin, 5e-4);
    }
}

/* ---------------------------------------------------------------------------
 *  M7. Lo del planificador en cada modo (sim.h, SimModo): mag = |s|, hl =
 *  RIG_HL/|s| y taus[j-1] = RIG_TAUS/(|s|·j), con las cuentas de antes (bit
 *  a bit), en modos reales y complejos (RC, RL, RLC serie y paralelo) y con
 *  pasos de prueba distintos. RIG_HL = 0,2 y RIG_TAUS = 10 (sim.c)
 * ------------------------------------------------------------------------ */
static void test_opt_modos(void)
{
    static SimModo md[SIM_MAX_MODOS];
    int caso, i, j, n, total = 0, bien = 0, complejos = 0;
    for (caso = 0; caso < 8; caso++) {
        double h0 = caso % 2 ? 1e-6 : 1e-4;
        sr_init(4);
        om_fuente(SE_E, 0, 1, ONDA_SENO, 1.0, 1e3);
        sr_el(SE_R, 1, 2, 10.0 * (double)(1 + caso));
        if (caso / 2 == 0) sr_el(SE_C, 2, 0, 1e-6);                       /* RC */
        else if (caso / 2 == 1) sr_el(SE_L, 2, 0, 1e-3);                  /* RL */
        else if (caso / 2 == 2) { sr_el(SE_L, 2, 3, 10e-3); sr_el(SE_C, 3, 0, 1e-6); }   /* RLC serie */
        else { sr_el(SE_L, 2, 0, 10e-3); sr_el(SE_C, 2, 0, 1e-6); sr_el(SE_R, 2, 0, 1e4); } /* paralelo */
        if (!sim_modos_prueba(&sr, h0, md, &n)) continue;
        for (i = 0; i < n; i++) {
            double mag = sqrt(md[i].re * md[i].re + md[i].im * md[i].im), hl = 0.2 / mag;
            int ok = om_bits_igual(&mag, &md[i].mag, sizeof mag) && om_bits_igual(&hl, &md[i].hl, sizeof hl);
            for (j = 1; j <= SIM_MODO_NJ; j++) {
                double t = 10.0 / (mag * (double)j);
                ok = ok && om_bits_igual(&t, &md[i].taus[j - 1], sizeof t);
            }
            total++;
            if (ok) bien++;
            if (md[i].im > 0.0) complejos++;
        }
    }
    if (!(total >= 8 && bien == total && complejos >= 2)) printf("  M7: %d modos, %d bien, %d complejos\n", total, bien, complejos);
    chk_cond("M7", "cada modo lleva |s|, RIG_HL/|s| y RIG_TAUS/(|s|" S_CDOT "j), bit a bit", total >= 8 && bien == total && complejos >= 2);
}

/* ---------------------------------------------------------------------------
 *  M9. El barrido de CC de una red lineal que barre una E o una J: tras dos
 *  puntos completos, la LU se reutiliza, y el análisis también si no miró
 *  las fuentes. Cada punto tiene que ser, bit a bit y con las mismas
 *  operaciones, el que da el OP completo (sim_cc_punto sin continuación); y
 *  nada se reutiliza si cambió la red (otro valor en cualquier elemento), el
 *  elemento o el tipo barrido, o si alguien volvió a armar lu_op entre dos
 *  puntos (un OP de otra red). Si el análisis mira el valor de las fuentes
 *  (una E en paralelo con una L; una J que carga un C en un trozo que flota)
 *  se hace en cada punto, porque el valor cambia la topología (en 0, otro
 *  aviso), y la LU solo se reutiliza cuando sale lo mismo que en el punto
 *  guardado
 * ------------------------------------------------------------------------ */
typedef struct { int e, n, dn; unsigned avisos; double x[LU_MAX]; Valores v; double res; unsigned long ops; } OmPunto;
static SimCc om_io, om_t;
static OmPunto om_pt[32], om_ref1;

/* El punto de valor v con la continuación io: el estado, los avisos, x, los
 * valores, el residuo y las operaciones */
static void om_cc(const SimRed *s, int k, int bx, double v, SimCc *io, OmPunto *p)
{
    SimDiag d;
    unsigned long o0 = sim_ops();
    memset(&d, 0, sizeof d);
    p->e = sim_cc_punto(s, k, bx, v, io, &d);
    p->ops = sim_ops() - o0;
    p->avisos = d.avisos;
    p->dn = d.n;
    p->n = io->n;
    memcpy(p->x, io->x, sizeof p->x);
    p->v = io->v;
    p->res = io->residuo;
}

/* Igual bit a bit: lo que se usa (las n incógnitas, nel elementos, nn nodos) */
static int om_igual_punto(const OmPunto *a, const OmPunto *b, const SimRed *s, int con_ops)
{
    int k;
    if (a->e != b->e || a->avisos != b->avisos || a->dn != b->dn || (con_ops && a->ops != b->ops)) return 0;
    if (a->e != SIM_OK) return 1;
    if (a->n != b->n || !om_bits_igual(a->x, b->x, (size_t)a->n * sizeof a->x[0]) || !om_bits_igual(&a->res, &b->res, sizeof a->res)) return 0;
    if (!om_bits_igual(a->v.phi, b->v.phi, (size_t)s->nn * sizeof a->v.phi[0])) return 0;
    for (k = 0; k < s->nel; k++)
        if (!om_bits_igual(&a->v.V[k], &b->v.V[k], sizeof(double)) || !om_bits_igual(&a->v.I[k], &b->v.I[k], sizeof(double)) ||
            !om_bits_igual(&a->v.P[k], &b->v.P[k], sizeof(double)) || !om_bits_igual(&a->v.val[k], &b->v.val[k], sizeof(double)))
            return 0;
    return 1;
}

/* La red de los barridos: E ideal (la barrida), R, C (abierto en CC), L
 * (cable) con otra L en paralelo (el reparto por el flujo: un aviso que no
 * depende de las fuentes), una E real, una J y más R; con cual = 1, una R
 * con otro valor */
static void om_red_barr(int cual)
{
    sr_init(7);
    om_fuente(SE_E, 0, 1, ONDA_CC, 5.0, 0.0);
    sr_el(SE_R, 1, 2, 1e3);
    sr_el(SE_C, 2, 0, 1e-6);
    sr_el(SE_L, 2, 3, 1e-3);
    sr_el(SE_R, 3, 0, cual ? 3.3e3 : 2.2e3);
    om_fuente(SE_E, 0, 4, ONDA_CC, 3.0, 0.0); sr.el[5].val = 50.0;       /* real: r = 50 Ω */
    sr_el(SE_R, 4, 3, 470.0);
    om_fuente(SE_J, 0, 5, ONDA_CC, 1e-3, 0.0);
    sr_el(SE_R, 5, 0, 10e3);
    sr_el(SE_R, 5, 3, 3.3e3);
    sr_el(SE_C, 5, 6, 100e-9);
    sr_el(SE_R, 6, 0, 1e3);
    sr_el(SE_L, 2, 3, 2e-3);
}

/* n puntos de v0 a v1 del elemento k (bx) con una continuación, y cada uno
 * contra el mismo barrido sin reutilizar nada (antes de cada punto, un OP de
 * otra red vuelve a armar lu_op: el camino de siempre, con sus operaciones)
 * y, sin las operaciones, contra el OP completo sin continuación: cuántos
 * distintos. En *cortos y *medios, los que no rehicieron la LU (sin análisis
 * y con él) */
static SimRed om_otra;
static int om_barre(const SimRed *s, int k, int bx, double v0, double v1, int n, unsigned long *cortos, unsigned long *medios)
{
    int i, malos = 0;
    unsigned long c0 = sim_cc_cortos(0), m0 = sim_cc_cortos(1);
    Valores vx;
    SimDiag d;
    memset(&om_io, 0, sizeof om_io);
    for (i = 0; i < n; i++) om_cc(s, k, bx, v0 + (v1 - v0) * (double)i / (double)(n - 1), &om_io, &om_pt[i]);
    *cortos = sim_cc_cortos(0) - c0;
    *medios = sim_cc_cortos(1) - m0;
    om_divisor(1.0); om_otra = sr;
    memset(&om_t, 0, sizeof om_t);
    for (i = 0; i < n; i++) {
        sim_op_red(&om_otra, &vx, &d);
        om_cc(s, k, bx, v0 + (v1 - v0) * (double)i / (double)(n - 1), &om_t, &om_ref1);
        if (!om_igual_punto(&om_pt[i], &om_ref1, s, 1)) {
            if (++malos <= 2) printf("  M9: punto %d de %d (elemento %d): distinto del barrido de siempre\n", i, n, k);
        }
    }
    for (i = 0; i < n; i++) {
        memset(&om_t, 0, sizeof om_t);
        om_cc(s, k, bx, v0 + (v1 - v0) * (double)i / (double)(n - 1), &om_t, &om_ref1);
        if (!om_igual_punto(&om_pt[i], &om_ref1, s, 0)) {
            if (++malos <= 2) printf("  M9: punto %d de %d (elemento %d): distinto del OP completo\n", i, n, k);
        }
    }
    return malos;
}

static void test_opt_barrido(void)
{
    static SimRed ra, rb, rc3;
    unsigned long cortos, medios, c0;
    int i, malos, k;
    Valores vx;
    SimDiag d;
    sim_cc_residuo(1);                            /* también el residuo (nl_prepara: Vesc e Iesc) */
    om_red_barr(0); ra = sr;
    om_red_barr(1); rb = sr;
    /* la E ideal, la J y la E real: 21 puntos, 19 por el camino corto */
    malos = om_barre(&ra, 0, BX_E, -10.0, 10.0, 21, &cortos, &medios);
    chk_cond("M9", "barrido de la E ideal: cada punto es el del OP completo (bit a bit, mismas ops), 19 por el camino corto",
             malos == 0 && cortos == 19 && medios == 0);
    malos = om_barre(&ra, 7, BX_J, -2e-3, 2e-3, 21, &cortos, &medios);
    chk_cond("M9", "barrido de la J: igual, 19 por el camino corto", malos == 0 && cortos == 19 && medios == 0);
    malos = om_barre(&ra, 5, BX_E, 0.0, 6.0, 13, &cortos, &medios);
    chk_cond("M9", "barrido de la E real: igual, 11 por el camino corto", malos == 0 && cortos == 11 && medios == 0);
    /* otra red (una R con otro valor), el mismo elemento y la misma
     * continuación: nada del camino corto de la de antes */
    memset(&om_io, 0, sizeof om_io);
    for (i = 0; i < 4; i++) om_cc(&ra, 0, BX_E, (double)i, &om_io, &om_pt[i]);
    for (i = 4; i < 8; i++) om_cc(&rb, 0, BX_E, (double)i, &om_io, &om_pt[i]);
    malos = 0;
    for (i = 0; i < 8; i++) {
        memset(&om_t, 0, sizeof om_t);
        om_cc(i < 4 ? &ra : &rb, 0, BX_E, (double)i, &om_t, &om_ref1);
        if (!om_igual_punto(&om_pt[i], &om_ref1, i < 4 ? &ra : &rb, 1)) malos++;
    }
    chk_cond("M9", "otra red con la misma continuaci" S_oo "n: sus puntos, los suyos", malos == 0);
    /* lu_op armada por otro OP entre dos puntos; y otro elemento barrido */
    memset(&om_io, 0, sizeof om_io);
    for (i = 0; i < 4; i++) om_cc(&ra, 0, BX_E, (double)i, &om_io, &om_pt[i]);
    sim_op_red(&rb, &vx, &d);
    for (i = 4; i < 7; i++) om_cc(&ra, 0, BX_E, (double)i, &om_io, &om_pt[i]);
    for (i = 7; i < 10; i++) om_cc(&ra, 7, BX_J, 1e-4 * (double)i, &om_io, &om_pt[i]);
    malos = 0;
    for (i = 0; i < 10; i++) {
        memset(&om_t, 0, sizeof om_t);
        if (i < 7) om_cc(&ra, 0, BX_E, (double)i, &om_t, &om_ref1);
        else om_cc(&ra, 7, BX_J, 1e-4 * (double)i, &om_t, &om_ref1);
        if (!om_igual_punto(&om_pt[i], &om_ref1, &ra, 1)) malos++;
    }
    chk_cond("M9", "un OP de otra red entre dos puntos, y luego otro elemento barrido: los puntos, los suyos", malos == 0);
    /* el análisis mira las fuentes: E en paralelo con una L (en 0 el lazo
     * cuadra: la regla del flujo; si no, la L sin corriente) y una J que carga
     * un C en un trozo que flota (en 0, un trozo sin corriente anclado). De
     * -1 a 1 en 5 puntos: el 2.º guarda; el 3.º (en 0) y el 4.º cambian de
     * topología y guardan otra vez; el 5.º reutiliza la LU del 4.º */
    sr_init(3);
    om_fuente(SE_E, 0, 1, ONDA_CC, 1.0, 0.0);
    sr_el(SE_L, 1, 0, 1e-3);
    sr_el(SE_R, 1, 2, 1e3);
    om_fuente(SE_J, 0, 2, ONDA_CC, 1e-3, 0.0);
    rc3 = sr;
    c0 = sim_cc_cortos(0);
    malos = om_barre(&rc3, 0, BX_E, -1.0, 1.0, 5, &cortos, &medios);
    k = 0;
    for (i = 0; i < 5; i++) if (om_pt[i].avisos != om_pt[0].avisos) k = 1;
    chk_cond("M9", "E en paralelo con una L: los del OP completo (en 0, otro aviso); el an" S_aa "lisis en cada punto",
             malos == 0 && cortos == 0 && medios == 1 && k);
    sr_init(3);
    om_fuente(SE_J, 0, 1, ONDA_CC, 1e-3, 0.0);
    sr_el(SE_C, 1, 0, 1e-6);
    om_fuente(SE_E, 0, 2, ONDA_CC, 1.0, 0.0);
    sr_el(SE_R, 2, 0, 1e3);
    rc3 = sr;
    malos = om_barre(&rc3, 0, BX_J, -1e-3, 1e-3, 5, &cortos, &medios);
    k = 0;
    for (i = 0; i < 5; i++) if (om_pt[i].avisos != om_pt[0].avisos) k = 1;
    chk_cond("M9", "una J que carga un C en un trozo que flota: los del OP completo (en 0, otro aviso); el an" S_aa "lisis en cada punto",
             malos == 0 && cortos == 0 && medios == 1 && k && sim_cc_cortos(0) == c0);
    sim_cc_residuo(0);
}

/* ---------------------------------------------------------------------------
 *  M9a. Guarda y repone, solo con lo usado: los nn nodos, los nel elementos
 *  y los NL.n no lineales de la red de la emulación. Entre dos trozos de una
 *  emulación de un rectificador con 1N4148 (4 nodos, dos de ellos tierras,
 *  4 elementos, el diodo en el 1 y el C con V0) se hace de todo con otra red
 *  más grande (9 nodos con una sola tierra, 16 elementos, dos L en
 *  paralelo, un trozo que flota y su diodo en el 15), que pisa col_phi,
 *  col_br, ci_ef, ci_val, topo_op y NL.el con otros valores y más allá de
 *  los de la emulación: al reponer, la emulación sigue como si nada, bit a
 *  bit como de un tirón
 * ------------------------------------------------------------------------ */
static SimRed om_grande, om_grande_lin;
static SimAjustes om_aj_g;

/* La de los barridos (om_red_barr) con un trozo que flota (R y C entre los
 * nodos 7 y 8) y, con diodo, un 1N4148 del 6 a masa */
static void om_red_grande(int con_diodo)
{
    om_red_barr(0);
    sr.nn = 9;
    sr_el(SE_R, 7, 8, 1e3);
    sr_el(SE_C, 8, 7, 1e-6);
    if (con_diodo) om_diodo(6, 0);
}

/* El rectificador de la emulación: la fuente en el nodo 2, el diodo del 2
 * al 3, la R a la tierra del nodo 1 y el C (con 1 V) a la del 0 */
static void om_rect_2t(void)
{
    int k;
    sr_init(4);
    sr.ref[1] = 1;
    om_fuente(SE_E, 0, 2, ONDA_SENO, 10.0, 60.0);
    om_diodo(2, 3);
    sr_el(SE_R, 3, 1, 1e3);
    k = sr_el(SE_C, 3, 0, 10e-6);
    sr.el[k].ci = 1; sr.el[k].x0 = 1.0;
    canal(0, CANAL_V, k); canal(1, CANAL_I, 1);
    saj.t_fin = 1.0 / 30.0;
}

/* Todo lo que guarda y repone el estado, con la red grande; cuántos no
 * dieron SIM_OK */
static int om_con_la_grande(double v)
{
    static Valores vx;
    static SimLin lin;
    static SimTopoCA tca;
    static SimCc io;
    SimDiag d;
    double vth, rth, dy;
    int inf, i, malos = 0;
    malos += sim_op_cc_red(&om_grande, &vx, &d, 0) != SIM_OK;
    malos += sim_punto_q(&om_grande, &lin, &d, 0) != SIM_OK;
    malos += sim_topo_ca(&om_grande, &tca, &d) != SIM_OK;
    memset(&io, 0, sizeof io);
    malos += sim_cc_punto(&om_grande, 0, BX_E, v, &io, &d) != SIM_OK;
    malos += sim_cc_punto(&om_grande, 0, BX_E, v + 1.0, &io, &d) != SIM_OK;
    malos += !sim_cc_deriva(&om_grande, 0, BX_E, &io, &om_aj_g.canal[0], &dy);
    malos += sim_thevenin_cc(&om_grande_lin, 3, 0, &vth, &rth, &inf, &d) != SIM_OK;
    memset(&io, 0, sizeof io);                    /* y el barrido lineal por el camino corto */
    for (i = 0; i < 4; i++) malos += sim_cc_punto(&om_grande_lin, 7, BX_J, 1e-3 * (double)i, &io, &d) != SIM_OK;
    return malos;
}

static void test_opt_intacto(void)
{
    static SimRes res1;
    static SimReg reg1;
    static SimRed red0;
    unsigned long c0 = sim_cc_cortos(0), o1;
    unsigned long long h1, h2;
    int trozos = 0, malos = 0, e, cambia = 1;
    om_red_grande(1); om_grande = sr;
    memset(&om_aj_g, 0, sizeof om_aj_g);
    canal_de(&om_aj_g, &om_grande, 0, CANAL_NODO, 3);
    om_red_grande(0); om_grande_lin = sr;
    om_rect_2t(); om_ra = sr; om_aa = saj;
    /* de un tirón */
    h1 = om_emula(&om_ra, &om_aa, &o1);
    res1 = *sim_res(); reg1 = *sim_reg();
    /* a trozos, con la grande entre cada dos */
    e = sim_iniciar_red(&om_ra, &om_aa);
    while (e == SIM_OK && sim_avanzar(20000ul)) {
        red0 = *sim_red();
        malos += om_con_la_grande(1.0 + (double)trozos);
        if (memcmp(&red0, sim_red(), sizeof red0) != 0) cambia = 0;
        trozos++;
    }
    h2 = e == SIM_OK && sim_res()->fin == FIN_COMPLETO ? om_huella() : 0;
    if (!(h1 && h1 == h2)) printf("  M9a: de un tir" S_oo "n %016llx, a trozos %016llx (%d trozos)\n", h1, h2, trozos);
    chk_cond("M9a", "las llamadas con la red grande (OP, Q, CA, barrido, derivada, Th" S_ee "venin) valen", malos == 0);
    chk_cond("M9a", "el barrido lineal fue por el camino corto", sim_cc_cortos(0) - c0 >= 2 * (unsigned long)trozos);
    chk_cond("M9a", "sim_red() sin tocar tras cada una", cambia);
    chk_cond("M9a", "el rectificador a trozos, con la red grande en medio: bit a bit el de un tir" S_oo "n",
             h1 && h1 == h2 && trozos >= 8 && sim_reg()->n == reg1.n &&
             memcmp(sim_reg()->t, reg1.t, sizeof reg1.t) == 0 && memcmp(sim_reg()->y, reg1.y, sizeof reg1.y) == 0 &&
             memcmp(sim_res(), &res1, sizeof res1) == 0);
}

/* ----------------------------------------------------------------------- */
static void test_opt_motor(void)
{
    printf("[75m] El motor optimizado (rev9): bit a bit y las cach" S_ee "s\n");
    test_opt_lu();
    test_opt_pares();
    test_opt_op();
    test_opt_exactos();
    test_opt_modos();
    test_opt_barrido();
    test_opt_intacto();
}
