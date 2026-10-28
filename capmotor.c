/* ============================================================================
 *  capmotor.c  -  Capacitores equivalentes: sustituir un capacitor con los
 *  de la bodega (ver capmotor.h)
 *
 *  Tres partes: leer lo escrito (el valor, la tolerancia y la bodega, con sus
 *  rangos, sus repetidos y lo que se quita), resolver (las combinaciones de
 *  uno, dos y tres que valen, en el orden de la lista; si no hay, la más
 *  cercana y lo que faltaría) y los textos del procedimiento, con palabras y
 *  con las cuentas del profe (el PDF del usuario, §5): «12 + ¿5? → no hay 5»,
 *  «10 + ¿7? → sí hay 7 ✔ (C₂ = 17 - 10 = 7)».
 * ========================================================================= */
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "capmotor.h"
#include "numfmt.h"
#include "gfx.h"                    /* solo las macros S_* de los glifos */

const char CPM_BODEGA_A[] = "1 2 3 7 9 10 12 28 32 34 35 38 42 60";
const char CPM_BODEGA_B[] = "15-42";

#define DEC_V 3                     /* lo escrito: hasta tres decimales  */
#define DEC_R 1                     /* lo calculado que no es un decimal
                                       corto: uno, como el profe (46.7)  */

/* --------------------------------------------------------------------------
 *  Leer lo escrito
 * ----------------------------------------------------------------------- */
/* 10^k, exacto hasta 10^22 */
static double p10(int k)
{
    double p = 1.0;
    while (k-- > 0) p *= 10.0;
    return p;
}

/* Un número sin signo en *p: cifras y un decimal con '.' (o con ',' si coma).
 * La mantisa entera entre 10^k, una sola división: el mismo double que
 * strtod y que float() de Python (el oráculo). 1 si había número (y avanza
 * *p), 0 si no, -1 si tiene más de 15 cifras */
static int lee_num(const char **p, double *v, int coma)
{
    const char *s = *p;
    double m = 0.0;
    int nd = 0, k = 0, punto = 0;
    for (;; s++) {
        if (*s >= '0' && *s <= '9') {
            if (++nd > 15) return -1;
            m = m * 10.0 + (double)(*s - '0');
            if (punto) k++;
        }
        else if (!punto && (*s == '.' || (coma && *s == ','))) punto = 1;
        else break;
    }
    if (nd == 0) return 0;
    *v = m / p10(k);
    *p = s;
    return 1;
}

int cpm_lee_valor(const char *s, double *v)
{
    double x = 0.0;
    int r;
    while (*s == ' ') s++;
    r = lee_num(&s, &x, 1);
    while (*s == ' ') s++;
    if (r < 0) return CPM_E_GRANDE;
    if (r == 0 || *s || !(x > 0.0)) return CPM_E_VALOR;
    if (x > CPM_TOPE) return CPM_E_GRANDE;
    if (!(x > CPM_MARGEN)) return CPM_E_CHICO;           /* C - 0.05 tiene que quedar > 0 */
    *v = x;
    return CPM_OK;
}

int cpm_lee_tol(const char *s, double *t)
{
    double x = 0.0;
    int r;
    while (*s == ' ') s++;
    if (!*s) { *t = 0.0; return CPM_OK; }          /* vacía: exacto */
    r = lee_num(&s, &x, 1);
    while (*s == ' ') s++;
    if (r < 0) return CPM_E_GRANDE;
    if (r == 0 || *s) return CPM_E_TOL;
    if (x > CPM_TOPE) return CPM_E_GRANDE;
    *t = x;
    return CPM_OK;
}

static int es_sep(char c) { return c == ' ' || c == ',' || c == ';' || c == '\t' || c == '\n'; }

static int cmp_d(const void *a, const void *b)
{
    double x = *(const double *)a, y = *(const double *)b;
    return x < y ? -1 : x > y ? 1 : 0;
}

/* El primer índice con v >= x (n si ninguno) */
static int primero_desde(const double *v, int n, double x)
{
    int lo = 0, hi = n;
    while (lo < hi) { int m = (lo + hi) / 2; if (v[m] < x) lo = m + 1; else hi = m; }
    return lo;
}

/* Lo que se pone (valores distintos, ascendentes, con cuántos; la cuenta se
 * queda en CPM_MAX + 1, que ya sobra) y lo que se quita (distintos) */
static double pon_v[CPM_TMP], quita_v[CPM_TMP];
static int pon_q[CPM_TMP], npon, nquita;

/* x a la lista ordenada l (de *n), con su cuenta en cu (o sin cuenta). 0 si
 * ya hay CPM_TMP valores distintos y x es otro */
static int junta(double *l, int *cu, int *n, double x)
{
    int k = primero_desde(l, *n, x);
    if (k < *n && l[k] == x) { if (cu && cu[k] <= CPM_MAX) cu[k]++; return 1; }
    if (*n >= CPM_TMP) return 0;
    memmove(l + k + 1, l + k, (size_t)(*n - k) * sizeof *l);
    if (cu) { memmove(cu + k + 1, cu + k, (size_t)(*n - k) * sizeof *cu); cu[k] = 1; }
    l[k] = x;
    (*n)++;
    return 1;
}

int cpm_lee_bodega(const char *s, double *v, int *n)
{
    int sin = 0, i, k, m;
    npon = nquita = 0;
    *n = 0;
    for (;;) {
        double a = 0.0, b, x;
        int menos = 0, r;
        while (es_sep(*s)) s++;
        if (!*s) break;
        if ((s[0] == 's' || s[0] == 'S') && (s[1] == 'i' || s[1] == 'I') && (s[2] == 'n' || s[2] == 'N') &&
            (!s[3] || es_sep(s[3]))) { sin = 1; s += 3; continue; }  /* lo que sigue se quita */
        if (*s == '-') { menos = 1; s++; }
        r = lee_num(&s, &a, 0);
        if (r < 0) return CPM_E_GRANDE;
        if (r == 0) return CPM_E_NUMERO;
        b = a;
        if (*s == '-') {                                  /* un rango de enteros */
            s++;
            r = lee_num(&s, &b, 0);
            if (r < 0) return CPM_E_GRANDE;
            if (r == 0) return CPM_E_NUMERO;
            if (a != floor(a) || b != floor(b)) return CPM_E_RANGO;
            if (b < a) { x = a; a = b; b = x; }
        }
        if (*s && !es_sep(*s)) return CPM_E_NUMERO;       /* «12x», «3-4-5» */
        if (a > CPM_TOPE || b > CPM_TOPE) return CPM_E_GRANDE;
        if (!(a > 0.0)) return CPM_E_CERO;
        if (b - a + 1.0 > (double)CPM_TMP) return CPM_E_MUCHOS;
        for (x = a; x <= b; x += 1.0) {                   /* un rango: uno de cada */
            if (!(sin || menos ? junta(quita_v, 0, &nquita, x) : junta(pon_v, pon_q, &npon, x))) return CPM_E_MUCHOS;
            if (a == b) break;                            /* un decimal suelto */
        }
    }
    for (i = m = 0; i < npon; i++) {                      /* fuera los quitados (todos los de ese valor) */
        k = primero_desde(quita_v, nquita, pon_v[i]);
        if (k < nquita && quita_v[k] == pon_v[i]) continue;
        if (m + pon_q[i] > CPM_MAX) return CPM_E_MUCHOS;
        for (k = 0; k < pon_q[i]; k++) v[m++] = pon_v[i];
    }
    *n = m;
    return CPM_OK;
}

const char *cpm_error(int e)
{
    switch (e) {
    case CPM_OK:       return "";
    case CPM_E_VALOR:  return "Valor: escribe un n" S_uu "mero mayor que 0.";
    case CPM_E_TOL:    return "Tolerancia: vac" S_ii "a, o un n" S_uu "mero menor que el valor.";
    case CPM_E_NUMERO: return "Bodega: hay algo que no es un n" S_uu "mero.";
    case CPM_E_CERO:   return "Bodega: el 0 no vale (todo tiene que ser > 0).";
    case CPM_E_RANGO:  return "Bodega: un rango va de entero a entero (15-42).";
    case CPM_E_MUCHOS: return "Bodega: son m" S_aa "s de 100 capacitores; quita algunos.";
    case CPM_E_CHICO:  return "Valor: tiene que ser mayor que 0.05 MF.";
    default:           return "Un n" S_uu "mero demasiado grande (hasta 100000 MF).";
    }
}

/* --------------------------------------------------------------------------
 *  Resolver
 * ----------------------------------------------------------------------- */
static double tol_ef(const CpmRes *r) { return r->tol > 0.0 ? r->tol : CPM_MARGEN; }

int cpm_vale(const CpmRes *r, double ct) { return fabs(ct - r->C) <= tol_ef(r) + CPM_EPS; }

int cpm_ncap(int tipo) { return tipo == CPM_UNO ? 1 : tipo <= CPM_SER ? 2 : 3; }

int cpm_mascara(int modo) { return modo == 2 ? 2 : modo == 3 ? 4 : 7; }

/* |CT - C| en unidades de 1e-9: dos cuentas iguales empatan aunque la coma
 * flotante las deje a un ulp */
static long long dist_q(const CpmRes *r, double ct) { return (long long)floor(fabs(ct - r->C) * 1e9 + 0.5); }

double cpm_ct(const CpmRes *r, int tipo, int a, int b, int c)
{
    double x = r->v[a], y = r->v[b], z = r->v[c], p;
    switch (tipo) {
    case CPM_UNO:  return x;
    case CPM_PAR:  return x + y;
    case CPM_SER:  return x * y / (x + y);
    case CPM_PAR3: return x + y + z;
    case CPM_SER3: return x * y * z / (x * y + y * z + x * z);
    case CPM_MIX1: p = x + y; return p * z / (p + z);
    default:       p = x * y / (x + y); return p + z;
    }
}

/* El tipo y luego los valores (índices de la bodega ordenada) */
static int cmp_tipo_valores(const CpmComb *x, const CpmComb *y)
{
    if (x->tipo != y->tipo) return x->tipo < y->tipo ? -1 : 1;
    if (x->a != y->a) return x->a < y->a ? -1 : 1;
    if (x->b != y->b) return x->b < y->b ? -1 : 1;
    if (x->c != y->c) return x->c < y->c ? -1 : 1;
    return 0;
}

int cpm_compara(const CpmRes *r, const CpmComb *x, const CpmComb *y)
{
    int nx = cpm_ncap(x->tipo), ny = cpm_ncap(y->tipo);
    long long dx, dy;
    if (nx != ny) return nx < ny ? -1 : 1;
    dx = dist_q(r, x->ct); dy = dist_q(r, y->ct);
    if (dx != dy) return dx < dy ? -1 : 1;
    return cmp_tipo_valores(x, y);
}

int cpm_compara_cerca(const CpmRes *r, const CpmComb *x, const CpmComb *y)
{
    long long dx = dist_q(r, x->ct), dy = dist_q(r, y->ct);
    int nx = cpm_ncap(x->tipo), ny = cpm_ncap(y->tipo);
    if (dx != dy) return dx < dy ? -1 : 1;
    if (nx != ny) return nx < ny ? -1 : 1;
    return cmp_tipo_valores(x, y);
}

static CpmComb comb(int tipo, int a, int b, int c, double ct)
{
    CpmComb k;
    k.tipo = (unsigned char)tipo; k.a = (unsigned char)a; k.b = (unsigned char)b; k.c = (unsigned char)c;
    k.ct = ct;
    return k;
}

/* ¿Hay capacitores para usar a y b (índices de valores)? ¿Y a, b y c? */
static int cabe2(const CpmRes *r, int i, int j) { return i != j || r->q[i] >= 2; }
static int cabe3(const CpmRes *r, int i, int j, int k)
{
    int ni = 1 + (j == i) + (k == i), nj = 1 + (i == j) + (k == j), nk = 1 + (i == k) + (j == k);
    return r->q[i] >= ni && r->q[j] >= nj && r->q[k] >= nk;
}

/* De tres, con a y b fijos el CT crece con el tercero: el primer k de [k0,
 * n) con CT >= x (n si ninguno) */
static int busca3(const CpmRes *r, int tipo, int i, int j, int k0, double x)
{
    int lo = k0, hi = r->n;
    while (lo < hi) { int m = (lo + hi) / 2; if (cpm_ct(r, tipo, i, j, m) < x) lo = m + 1; else hi = m; }
    return lo;
}

void cpm_recorre(const CpmRes *r, int mascara, CpmVisita f, void *ctx)
{
    int i, j, k, t;
    CpmComb x;
    if (mascara & 1)
        for (i = 0; i < r->n; i++)
            if (cpm_vale(r, r->v[i])) { x = comb(CPM_UNO, i, i, i, r->v[i]); f(ctx, &x); }
    if (mascara & 2)
        for (i = 0; i < r->n; i++)
            for (j = i; j < r->n; j++) {
                if (!cabe2(r, i, j)) continue;
                for (t = CPM_PAR; t <= CPM_SER; t++) {
                    double ct = cpm_ct(r, t, i, j, j);
                    if (cpm_vale(r, ct)) { x = comb(t, i, j, j, ct); f(ctx, &x); }
                }
            }
    if (mascara & 4)
        for (i = 0; i < r->n; i++)
            for (j = i; j < r->n; j++) {
                if (!cabe2(r, i, j)) continue;
                for (t = CPM_PAR3; t <= CPM_MIX2; t++) {
                    /* el tercero, solo donde el CT puede valer (con holgura;
                     * cpm_vale decide) */
                    for (k = busca3(r, t, i, j, t <= CPM_SER3 ? j : 0, r->lo - 1e-6); k < r->n; k++) {
                        double ct = cpm_ct(r, t, i, j, k);
                        if (ct > r->hi + 1e-6) break;
                        if (cabe3(r, i, j, k) && cpm_vale(r, ct)) { x = comb(t, i, j, k, ct); f(ctx, &x); }
                    }
                }
            }
}

/* Lo que se enseña: de cada número de capacitores, las primeras en el orden
 * de la lista (todas las de uno, CPM_VER2 de dos y CPM_VER3 de tres), y la
 * cuenta de todas */
static struct {
    const CpmRes *r;
    int guarda;                     /* la máscara del modo */
    int max[4], n[4];
    long tot[4];
    CpmComb l[4][CPM_MAX];
} J;

static void inserta(const CpmRes *r, CpmComb *l, int *n, int max, const CpmComb *k)
{
    int lo = 0, hi = *n;
    if (*n >= max && cpm_compara(r, k, &l[max - 1]) >= 0) return;
    while (lo < hi) { int m = (lo + hi) / 2; if (cpm_compara(r, &l[m], k) < 0) lo = m + 1; else hi = m; }
    if (*n >= max) *n = max - 1;
    memmove(&l[lo + 1], &l[lo], (size_t)(*n - lo) * sizeof *l);
    l[lo] = *k;
    (*n)++;
}

static void junta_sol(void *ctx, const CpmComb *k)
{
    int c = cpm_ncap(k->tipo);
    (void)ctx;
    J.tot[c]++;
    if (J.guarda & (1 << (c - 1))) inserta(J.r, J.l[c], &J.n[c], J.max[c], k);
}

static void mejor_cerca(const CpmRes *r, CpmComb *m, int *hay, CpmComb k)
{
    if (!*hay || cpm_compara_cerca(r, &k, m) < 0) { *m = k; *hay = 1; }
}

/* La más cercana con los números de la máscara, aunque no valga */
static void cerca_de(const CpmRes *r, int mascara, CpmComb *m, int *hay)
{
    int i, j, k, t;
    *hay = 0;
    if (mascara & 1)
        for (i = 0; i < r->n; i++) mejor_cerca(r, m, hay, comb(CPM_UNO, i, i, i, r->v[i]));
    if (mascara & 2)
        for (i = 0; i < r->n; i++)
            for (j = i; j < r->n; j++)
                if (cabe2(r, i, j))
                    for (t = CPM_PAR; t <= CPM_SER; t++) mejor_cerca(r, m, hay, comb(t, i, j, j, cpm_ct(r, t, i, j, j)));
    if (mascara & 4)
        for (i = 0; i < r->n; i++)
            for (j = i; j < r->n; j++) {
                if (!cabe2(r, i, j)) continue;
                for (t = CPM_PAR3; t <= CPM_MIX2; t++) {
                    /* el CT crece con el tercero: lo más cercano es el que
                     * cabe justo encima de C o justo debajo (y los de debajo
                     * que empatan con él, que van antes por ser menores) */
                    int k0 = t <= CPM_SER3 ? j : 0, kc = busca3(r, t, i, j, k0, r->C);
                    for (k = kc; k < r->n && !cabe3(r, i, j, k); k++) {}
                    if (k < r->n) mejor_cerca(r, m, hay, comb(t, i, j, k, cpm_ct(r, t, i, j, k)));
                    for (k = kc - 1; k >= k0 && !cabe3(r, i, j, k); k--) {}
                    if (k >= k0) {
                        long long d = dist_q(r, cpm_ct(r, t, i, j, k));
                        for (; k >= k0; k--) {
                            double ct;
                            if (!cabe3(r, i, j, k)) continue;
                            ct = cpm_ct(r, t, i, j, k);
                            if (dist_q(r, ct) != d) break;
                            mejor_cerca(r, m, hay, comb(t, i, j, k, ct));
                        }
                    }
                }
            }
}

void cpm_resuelve_n(CpmRes *r, double C, double tol, int modo, const double *v, int n)
{
    static double w[CPM_MAX];
    int i, c;
    if (n > CPM_MAX) n = CPM_MAX;
    if (n < 0) n = 0;
    for (i = 0; i < n; i++) w[i] = v[i];
    if (n > 1) qsort(w, (size_t)n, sizeof w[0], cmp_d);
    r->n = 0;
    for (i = 0; i < n; i++) {                             /* los distintos, con cuántos */
        if (r->n > 0 && w[i] == r->v[r->n - 1]) r->q[r->n - 1]++;
        else { r->v[r->n] = w[i]; r->q[r->n] = 1; r->n++; }
    }
    r->ncap = n;
    r->C = C;
    r->tol = tol > 0.0 ? tol : 0.0;
    r->lo = C - tol_ef(r);
    r->hi = C + tol_ef(r);
    r->modo = modo == 2 || modo == 3 ? modo : CPM_AUTO;
    memset(&J, 0, sizeof J);
    J.r = r;
    J.guarda = cpm_mascara(r->modo);
    J.max[1] = CPM_MAX; J.max[2] = CPM_VER2; J.max[3] = CPM_VER3;
    cpm_recorre(r, 7, junta_sol, 0);                      /* las tres cuentas, siempre */
    r->nval = 0;
    r->ntot = 0;
    r->tot[0] = 0; r->nver[0] = 0;
    for (c = 1; c <= 3; c++) {
        int si = (J.guarda >> (c - 1)) & 1;
        r->tot[c] = J.tot[c];
        r->nver[c] = si ? J.n[c] : 0;
        if (si) r->ntot += J.tot[c];
        for (i = 0; i < r->nver[c]; i++) r->val[r->nval++] = J.l[c][i];
    }
    r->hay_cerca = 0;
    if (r->ntot == 0) cerca_de(r, J.guarda, &r->cerca, &r->hay_cerca);
}

void cpm_resuelve(CpmRes *r, double C, double tol, const double *v, int n)
{
    cpm_resuelve_n(r, C, tol, CPM_AUTO, v, n);
}

int cpm_con_faltas(const CpmRes *r)
{
    return r->modo != 3 && r->tot[2] == 0 && (r->modo == 2 || r->tot[1] == 0);   /* con «2», aunque uno solo valga */
}

void cpm_falta_directo(const CpmRes *r, double *lo, double *hi)
{
    *lo = r->lo > 0.0 ? r->lo : 0.0;
    *hi = r->hi;
}

int cpm_faltas(const CpmRes *r, int tipo, CpmFalta *f, int max)
{
    int i, m = 0;
    for (i = 0; i < r->n && m < max; i++) {
        double c1 = r->v[i];
        if (tipo == CPM_PAR && c1 < r->C) {
            f[m].c1 = c1;
            f[m].lo = r->lo - c1 > 0.0 ? r->lo - c1 : 0.0;
            f[m].hi = r->hi - c1;
            m++;
        } else if (tipo == CPM_SER && c1 > r->hi) {
            f[m].c1 = c1;
            f[m].lo = r->lo > 0.0 ? r->lo * c1 / (c1 - r->lo) : 0.0;
            f[m].hi = r->hi * c1 / (c1 - r->hi);
            m++;
        }
    }
    return m;
}

/* --------------------------------------------------------------------------
 *  La edición con cursor
 * ----------------------------------------------------------------------- */
int cpm_lineas_campo(const char *s, int w, int *ini, int max)
{
    int len = (int)strlen(s), p = 0, n = 0;
    if (max < 1) return 0;
    if (w < 2) w = 2;
    ini[n++] = 0;
    while (len - p > w && n < max) {
        int k, corte = -1;
        for (k = p + w; k > p; k--) if (s[k] == ' ') { corte = k; break; }
        p = corte > p ? corte + 1 : p + w;
        ini[n++] = p;
    }
    return n;
}

int cpm_linea_de(const int *ini, int nl, int len, int pos, int *col)
{
    int i = nl - 1;
    (void)len;
    while (i > 0 && ini[i] > pos) i--;
    if (col) *col = pos - ini[i];
    return i;
}

int cpm_salta_valor(const char *s, int pos, int dir)
{
    int len = (int)strlen(s);
    if (pos > len) pos = len;
    if (pos < 0) pos = 0;
    if (dir > 0) {
        while (pos < len && s[pos] != ' ') pos++;
        while (pos < len && s[pos] == ' ') pos++;
    } else {
        while (pos > 0 && s[pos - 1] == ' ') pos--;
        while (pos > 0 && s[pos - 1] != ' ') pos--;
    }
    return pos;
}

/* --------------------------------------------------------------------------
 *  Textos
 * ----------------------------------------------------------------------- */
char *cpm_num(char *buf, size_t cap, double v, int dec)
{
    char *p;
    num_auto(buf, cap, v, dec);
    for (p = buf; *p; p++) if (*p == ',') *p = '.';
    return buf;
}

static void bnum(Buf *b, double v, int dec)
{
    char t[32];
    buf_str(b, cpm_num(t, sizeof t, v, dec));
}

static void bval(Buf *b, double v) { bnum(b, v, DEC_V); }

/* ¿x es un decimal corto, de DEC_V cifras como mucho (7.05, 22.4, 210)? Lo
 * escrito lo es, y sus sumas; una serie casi nunca (46.666...) */
static int corto(double x)
{
    double m = fabs(x) * p10(DEC_V);
    return fabs(m - floor(m + 0.5)) < 1e-6;
}

/* Los decimales de un valor calculado: los suyos si es corto (7.05, no 7.1);
 * si no, DEC_R como el profe (46.7), o más si con DEC_R sería 0 */
static int dec_calc(double x)
{
    int d = DEC_R;
    if (corto(x)) return DEC_V;
    while (d < DEC_V && fabs(x) * p10(d) < 0.5) d++;
    return d;
}

/* Los decimales con que se escribe lo escrito (17: 0; 22.5: 1; 17.05: 2) */
static int dec_de(double x)
{
    char t[32];
    const char *p = strchr(cpm_num(t, sizeof t, x, DEC_V), '.');
    return p ? (int)strlen(p + 1) : 0;
}

/* Los decimales con que bcalc escribe un valor calculado x que se compara
 * con ref (lo pedido o un borde); -1 si x es ref (a 1e-9, lo mismo que el
 * orden de la lista) y se escribe como ref, «2.25» y no «2.3». Si no, los
 * de dec_calc, no menos que los de ref y más si así se escribiría igual que
 * ref sin serlo: «13.98» frente a 14, no «14» */
static int dcalc(double x, double ref)
{
    char t[32], u[32];
    int d = dec_calc(x), dr = dec_de(ref);
    if (fabs(x - ref) * 1e9 < 0.5) return -1;
    if (d < dr) d = dr;
    cpm_num(u, sizeof u, ref, DEC_V);
    while (d < 9 && !strcmp(cpm_num(t, sizeof t, x, d), u)) d++;
    return d;
}

static void bcalc(Buf *b, double x, double ref)
{
    int d = dcalc(x, ref);
    if (d < 0) bnum(b, ref, DEC_V);
    else bnum(b, x, d);
}

/* Un valor intermedio (el paralelo o la serie de dos dentro de un mixto):
 * con sus decimales si es corto; si no, «≈ 4.6» */
static void bmedio(Buf *b, double x)
{
    if (!corto(x)) buf_str(b, S_APROX " ");
    bnum(b, x, dec_calc(x));
}

/* Los decimales de la diferencia ct - C: los suyos si es un decimal corto
 * (dif. -0.05); si no, uno (dos por debajo de 0.1); no menos que los de C, y
 * los que la distinguen de 0 */
static int dif_dec(const CpmRes *r, double ct)
{
    double d = ct - r->C;
    int k = corto(d) ? DEC_V : fabs(d) < 0.1 ? 2 : DEC_R, kc = dec_de(r->C);
    if (k < kc) k = kc;
    while (k < 9 && fabs(d) * p10(k) < 0.5) k++;      /* nunca «dif. +0» */
    return k;
}

/* «exacto» o «dif. +0.4», con k decimales */
static void dif_k(const CpmRes *r, double ct, int k, char *buf, size_t cap)
{
    double d = ct - r->C;
    Buf b;
    buf_init(&b, buf, cap);
    if (dist_q(r, ct) == 0) { buf_str(&b, "exacto"); return; }
    buf_str(&b, "dif. ");
    if (d > 0.0) buf_ch(&b, '+');
    bnum(&b, d, k);
}

void cpm_dif(const CpmRes *r, double ct, char *buf, size_t cap) { dif_k(r, ct, dif_dec(r, ct), buf, cap); }

/* Los decimales con que x se distingue de y: si están a distancias
 * distintas de C (y ninguna es exacta), los primeros, desde los de las dos,
 * a partir de los cuales |x - C| y |y - C| ya no se escriben igual con
 * ninguno más (0.145 y 0.155 se distinguen con uno, 0.1 y 0.2, pero con dos
 * no: 0.15 y 0.15; así, si otra vecina pide más, siguen distintas). Si no,
 * los de x */
static int dif_dec_frente(const CpmRes *r, double x, double y)
{
    char t[32], u[32];
    int k = dif_dec(r, x), ky = dif_dec(r, y), j;
    if (dist_q(r, x) == dist_q(r, y) || dist_q(r, x) == 0 || dist_q(r, y) == 0) return k;
    if (k < ky) k = ky;
    for (j = 9; j >= k; j--)                              /* la más fina con que se escriben igual */
        if (!strcmp(cpm_num(t, sizeof t, fabs(x - r->C), j), cpm_num(u, sizeof u, fabs(y - r->C), j))) return j < 9 ? j + 1 : 9;
    return k;
}

void cpm_dif_op(const CpmRes *r, const CpmComb *k, char *buf, size_t cap)
{
    int i, s, e, d = dif_dec(r, k->ct), c = cpm_ncap(k->tipo);
    long long dk = dist_q(r, k->ct);
    for (i = 0; i < r->nval && &r->val[i] != k; i++) {}
    if (i < r->nval) {
        /* las que están a su misma distancia (seguidas en la lista) se
         * escriben igual: los decimales salen de las vecinas de esa tanda */
        for (s = i; s > 0 && cpm_ncap(r->val[s - 1].tipo) == c && dist_q(r, r->val[s - 1].ct) == dk; s--) {}
        for (e = i; e + 1 < r->nval && cpm_ncap(r->val[e + 1].tipo) == c && dist_q(r, r->val[e + 1].ct) == dk; e++) {}
        if (s > 0 && cpm_ncap(r->val[s - 1].tipo) == c) { int kj = dif_dec_frente(r, k->ct, r->val[s - 1].ct); if (d < kj) d = kj; }
        if (e + 1 < r->nval && cpm_ncap(r->val[e + 1].tipo) == c) { int kj = dif_dec_frente(r, k->ct, r->val[e + 1].ct); if (d < kj) d = kj; }
    }
    dif_k(r, k->ct, d, buf, cap);
}

/* La diferencia de ct con lo pedido: «exacto» o «dif. +0.4» */
static void bdif(Buf *b, const CpmRes *r, double ct)
{
    char t[40];
    cpm_dif(r, ct, t, sizeof t);
    buf_str(b, t);
}

/* La de una opción de la lista (cpm_dif_op) */
static void bdif_op(Buf *b, const CpmRes *r, const CpmComb *k)
{
    char t[40];
    cpm_dif_op(r, k, t, sizeof t);
    buf_str(b, t);
}

/* La de x, con los decimales que la distinguen de la de y */
static void bdif_frente(Buf *b, const CpmRes *r, double x, double y)
{
    char t[40];
    dif_k(r, x, dif_dec_frente(r, x, y), t, sizeof t);
    buf_str(b, t);
}

/* Cómo se escribe un título (para que quepa: linea_titulo y cpm_fila_lista) */
#define F_LARGA   1                 /* con «en serie»                          */
#define F_SIN_MF  2                 /* sin « MF»                               */
#define F_JUNTO   4                 /* «a∥b», «a,b»: sin espacios             */

/* Los valores de k, sin el CT, como en el título */
static void bformula(Buf *b, const CpmRes *r, const CpmComb *k, int fl)
{
    const double *v = r->v;
    const char *par = (fl & F_JUNTO) ? S_PAR : " " S_PAR " ";
    int larga = fl & F_LARGA;
    switch (k->tipo) {
    case CPM_UNO:
        bval(b, v[k->a]);
        break;
    case CPM_PAR:                   /* el mayor primero: C₁ = 10, C₂ = 7 */
        bval(b, v[k->b]); buf_str(b, par); bval(b, v[k->a]);
        break;
    case CPM_SER:
        bval(b, v[k->a]); buf_str(b, " y "); bval(b, v[k->b]);
        if (larga) buf_str(b, " en serie");
        break;
    case CPM_PAR3:                  /* como el paralelo de dos: el mayor primero */
        bval(b, v[k->c]); buf_str(b, par); bval(b, v[k->b]); buf_str(b, par); bval(b, v[k->a]);
        break;
    case CPM_SER3:
        bval(b, v[k->a]); buf_str(b, (fl & F_JUNTO) ? "," : ", "); bval(b, v[k->b]); buf_str(b, " y "); bval(b, v[k->c]);
        if (larga) buf_str(b, " en serie");
        break;
    case CPM_MIX1:
        buf_ch(b, '('); bval(b, v[k->a]); buf_str(b, par); bval(b, v[k->b]);
        buf_str(b, ") y "); bval(b, v[k->c]);
        if (larga) buf_str(b, " en serie");
        break;
    default:
        buf_ch(b, '('); bval(b, v[k->a]); buf_str(b, " y "); bval(b, v[k->b]);
        buf_str(b, larga ? " en serie)" : ")"); buf_str(b, par); bval(b, v[k->c]);
        break;
    }
}

static void btitulo_f(Buf *b, const CpmRes *r, const CpmComb *k, int fl)
{
    bformula(b, r, k, fl);
    if (k->tipo == CPM_UNO) { buf_str(b, (fl & F_SIN_MF) ? " solo" : " MF solo"); return; }
    buf_str(b, " = "); bcalc(b, k->ct, r->C);
    if (!(fl & F_SIN_MF)) buf_str(b, " MF");
}

static void btitulo(Buf *b, const CpmRes *r, const CpmComb *k) { btitulo_f(b, r, k, F_LARGA); }

/* Una línea del veredicto: pre, el título de k y suf, en CPM_VER_COLS
 * columnas. Con «en serie» y « MF» si caben; si no, sin «en serie», sin
 * « MF», con suf2 (el corto) y juntando ∥. Si ni así cabe, pre (sin sus
 * espacios) se queda en su línea y el título pasa a la siguiente, otra vez
 * de lo más largo a lo más corto (lo que aún no quepa lo corta la pantalla) */
static void linea_titulo(Buf *b, const CpmRes *r, const CpmComb *k, const char *pre, const char *suf, const char *suf2)
{
    static const int FL[5] = { F_LARGA, 0, F_SIN_MF, F_SIN_MF, F_SIN_MF | F_JUNTO };
    char t[200];
    Buf bt;
    int pase, i;
    for (pase = 0; pase < 2; pase++)
        for (i = 0; i < 5; i++) {
            buf_init(&bt, t, sizeof t);
            if (pase == 0) buf_str(&bt, pre);
            btitulo_f(&bt, r, k, FL[i]);
            buf_str(&bt, i >= 3 ? suf2 : suf);
            if ((int)bt.n <= CPM_VER_COLS || (pase == 1 && i == 4)) {
                if (pase == 1) {                          /* pre en su línea */
                    size_t m = strlen(pre);
                    while (m > 0 && pre[m - 1] == ' ') m--;
                    for (i = 0; i < (int)m; i++) buf_ch(b, pre[i]);
                    buf_ch(b, '\n');
                }
                buf_str(b, t);
                return;
            }
        }
}

void cpm_titulo(const CpmRes *r, const CpmComb *k, char *buf, size_t cap)
{
    Buf b;
    buf_init(&b, buf, cap);
    btitulo(&b, r, k);
}

static const char *const TIPO_FILA[CPM_NT] = { "uno solo: ", "paralelo: ", "serie: ", "paralelo: ", "serie: ", "mixto: ", "mixto: " };

void cpm_fila(const CpmRes *r, const CpmComb *k, char *buf, size_t cap)
{
    Buf b;
    buf_init(&b, buf, cap);
    buf_str(&b, TIPO_FILA[k->tipo < CPM_NT ? k->tipo : 0]);
    bformula(&b, r, k, 0);
    if (k->tipo != CPM_UNO) { buf_str(&b, " = "); bcalc(&b, k->ct, r->C); }
    buf_str(&b, " MF");
}

void cpm_fila_lista(const CpmRes *r, const CpmComb *k, int num, int maxch, char *buf, size_t cap)
{
    char d[40];
    int v;
    Buf b;
    cpm_dif_op(r, k, d, sizeof d);
    for (v = 0; v < 5; v++) {   /* 0 entera; 1 sin « MF»; 2 sin el tipo; 3 sin «dif. »; 4 ∥ sin espacios */
        buf_init(&b, buf, cap);
        buf_int(&b, num); buf_str(&b, ". ");
        if (v < 2) buf_str(&b, TIPO_FILA[k->tipo < CPM_NT ? k->tipo : 0]);
        bformula(&b, r, k, v >= 4 ? F_JUNTO : 0);
        if (k->tipo != CPM_UNO) { buf_str(&b, " = "); bcalc(&b, k->ct, r->C); }
        if (v < 1) buf_str(&b, " MF");
        buf_str(&b, " ("); buf_str(&b, v >= 3 && !strncmp(d, "dif. ", 5) ? d + 5 : d); buf_ch(&b, ')');
        if ((int)b.n <= maxch) return;
    }
    if (maxch > 3 && (size_t)maxch < cap) strcpy(buf + maxch - 3, "...");
}

/* El de uno que vale más cerca de C (-1 si no hay) */
static int uno_cercano(const CpmRes *r)
{
    int i, m = -1;
    for (i = 0; i < r->n; i++)
        if (cpm_vale(r, r->v[i]) && (m < 0 || dist_q(r, r->v[i]) < dist_q(r, r->v[m]))) m = i;
    return m;
}

void cpm_veredicto(const CpmRes *r, char *buf, size_t cap)
{
    char t[64], d[40], s1[48], s2[48];
    Buf b, bt;
    buf_init(&b, buf, cap);
    if (r->n == 0) { buf_str(&b, "IRRESOLUBLE: la bodega est" S_aa " vac" S_ii "a."); return; }
    if (r->nval > 0) {
        const CpmComb *S = &r->val[0];
        int c = cpm_ncap(S->tipo);
        buf_init(&bt, t, sizeof t);
        if (r->modo == CPM_AUTO && c == 3) {
            buf_str(&b, "Con dos no se puede (" S_IQ "el profe lo puso mal?);\n");
            buf_str(&bt, "con tres s" S_ii ": ");
        } else { buf_str(&bt, "Con "); buf_int(&bt, c); buf_str(&bt, ": "); }
        linea_titulo(&b, r, S, t, " " S_CHECK, " " S_CHECK);
        return;
    }
    if (r->modo == 2) {
        int u = uno_cercano(r);
        if (u >= 0) {                                     /* el profe pide dos, pero uno solo ya vale */
            buf_init(&bt, t, sizeof t);
            buf_str(&bt, "IRRESOLUBLE con dos (el "); bval(&bt, r->v[u]); buf_str(&bt, " solo s" S_ii ": elige Auto).");
            buf_str(&b, (int)bt.n <= CPM_VER_COLS ? t : "IRRESOLUBLE con dos (uno solo s" S_ii ": elige Auto).");
        } else buf_str(&b, r->tot[3] > 0 ? "IRRESOLUBLE con dos (con tres s" S_ii ": elige Auto)." : "IRRESOLUBLE con dos capacitores.");
    } else if (r->modo == 3)
        buf_str(&b, r->tot[1] + r->tot[2] > 0 ? "IRRESOLUBLE con tres (con menos s" S_ii ": elige Auto)." : "IRRESOLUBLE con tres capacitores.");
    else buf_str(&b, "IRRESOLUBLE con uno, dos o tres capacitores.");
    if (r->hay_cerca) {
        cpm_dif(r, r->cerca.ct, d, sizeof d);
        buf_init(&bt, s1, sizeof s1); buf_str(&bt, " ("); buf_str(&bt, d); buf_ch(&bt, ')');
        buf_init(&bt, s2, sizeof s2); buf_str(&bt, " ("); buf_str(&bt, strncmp(d, "dif. ", 5) ? d : d + 5); buf_ch(&bt, ')');
        buf_ch(&b, '\n');
        linea_titulo(&b, r, &r->cerca, "Lo m" S_aa "s cercano: ", s1, s2);
    }
}

int cpm_envuelve(const char *txt, int maxch, CpmLinea *l, int max)
{
    const char *s = txt;
    int n = 0;
    while (*s && n < max) {
        int lead = 0, sang = 0;
        while (s[lead] == ' ') lead++;
        for (;;) {                                        /* una línea del texto, hasta '\n' */
            int ancho = maxch - sang, k = 0, corte = 0;
            if (ancho < 8) ancho = 8;
            while (s[k] && s[k] != '\n' && k < ancho) { if (s[k] == ' ') corte = k; k++; }
            if (s[k] && s[k] != '\n' && corte > 0) k = corte;
            l[n].ini = (unsigned short)(s - txt);
            l[n].len = (unsigned char)k;
            l[n].sangria = (unsigned char)sang;
            n++;
            s += k;
            while (*s == ' ') s++;
            if (*s == '\n') { s++; break; }
            if (!*s || n >= max) break;
            sang = lead > 0 ? lead + 2 : 3;               /* la continuación, con sangría */
        }
    }
    return n;
}

const char *cpm_leyenda(const CpmRes *r, const CpmComb *k)
{
    (void)r;
    switch (k->tipo) {
    case CPM_UNO:  return "Sustituci" S_oo "n directa: uno solo entre A y T.";
    case CPM_PAR:  return "Paralelo: cada capacitor toca A y T.";
    case CPM_SER:  return "Serie: uno toca A, el otro T; el medio, nada.";
    case CPM_PAR3: return "Paralelo de 3: cada uno toca A y T.";
    case CPM_SER3: return "Serie de 3: dos puntos medios, sin m" S_aa "s.";
    case CPM_MIX1: return "Mixto: el paralelo de dos, en serie con el otro.";
    default:       return "Mixto: los de arriba en serie, " S_PAR " al de abajo.";
    }
}

/* ---- El procedimiento ---- */

/* Un intento del profe: C₁ y lo que tendría que valer C₂ */
typedef struct {
    int i;                          /* el índice de C₁                         */
    double c1;
    double t;                       /* C₂ nominal (con def)                    */
    double rlo, rhi;                /* el rango de C₂ (rhi < 0: sin tope)      */
    int def;                        /* hay C₂ nominal (C₁ < C o C₁ > C)       */
    int solo;                       /* C₁ ya vale solo                         */
    int w;                          /* el C₂ que hay en la bodega, o -1        */
    double ct;
    int elegida;                    /* es la opción elegida                    */
    int falta_otro;                 /* C₂ tendría que ser otro como C₁ y solo hay uno */
} Intento;

static Intento it[CPM_MAX];

static int entero(double x) { return fabs(x - floor(x + 0.5)) < 1e-9 * (1.0 + fabs(x)); }

/* Un rango de C₂ redondeado hacia dentro a un decimal (lo que se enseña,
 * dentro): «de 52.3 a 58.2»; sin tope, «desde 500». Si a un decimal se
 * queda en un punto (o en nada), con más: «de 6.99 a 7.01», no «de 7 a 7».
 * Sin «de» (de = 0): «2 a 3» */
static void brango_de(Buf *b, double lo, double hi, int de)
{
    double l, h;
    int dec = DEC_R;
    for (;;) {
        double p = p10(dec), xl = lo * p, xh = hi * p;
        l = ceil(xl - 1e-9 - 1e-12 * fabs(xl)) / p;
        h = floor(xh + 1e-9 + 1e-12 * fabs(xh)) / p;
        if (hi < 0.0 || l < h || dec >= 6) break;
        dec++;
    }
    if (hi >= 0.0 && !(l < h)) { l = lo; h = hi; }
    if (hi < 0.0) { buf_str(b, "desde "); bnum(b, l, dec); return; }
    if (de) buf_str(b, "de ");
    bnum(b, l, dec); buf_str(b, " a "); bnum(b, h, dec);
}

static void brango(Buf *b, double lo, double hi) { brango_de(b, lo, hi, 1); }

/* «sí»: el C₂ que hace falta está en la bodega, o sea, la elegida da justo
 * lo pedido (a 1e-9, como «exacto» y la comprobación sin «dif.») */
static int intento_si(const CpmRes *r, const Intento *q)
{
    return q->w >= 0 && q->elegida && r->tol == 0.0 && q->def && dist_q(r, q->ct) == 0;
}

/* Lo de después de la flecha */
static void intento_fin(Buf *b, const CpmRes *r, const Intento *q, int serie, int si)
{
    buf_str(b, " " S_RARR " ");
    if (q->w < 0) {
        if (q->falta_otro) { buf_str(b, "solo hay un "); bval(b, q->c1); }
        else if (!serie && r->tol == 0.0 && q->def) { buf_str(b, "no hay "); bnum(b, q->t, dec_calc(q->t)); }
        else buf_str(b, "no hay");
    } else if (q->w == q->i) {                            /* el mismo valor: hacen falta dos */
        buf_str(b, "hay dos de "); bval(b, q->c1);
        if (q->elegida) buf_str(b, " " S_CHECK);
        else { buf_str(b, ": sirve (CT "); bcalc(b, q->ct, r->C); buf_ch(b, ')'); }
    } else if (q->elegida) {
        buf_str(b, si ? "s" S_ii " hay " : "hay "); bval(b, r->v[q->w]); buf_str(b, " " S_CHECK);
    } else {
        buf_str(b, "hay "); bval(b, r->v[q->w]);
        buf_str(b, ": sirve (CT "); bcalc(b, q->ct, r->C); buf_ch(b, ')');   /* 12.02: no se confunde con la exacta */
    }
}

/* El intento del paralelo: «12 + ¿5? → no hay 5», «10 + ¿7? → sí hay 7 ✔
 * (C₂ = 17 - 10 = 7)»; con tolerancia, «20 + ¿2 a 3? → no hay» */
static void intento_par(Buf *b, const CpmRes *r, const Intento *q)
{
    int si = intento_si(r, q);
    buf_str(b, "     "); bval(b, q->c1);
    if (q->solo && q->w < 0) { buf_str(b, " ya vale solo\n"); return; }
    buf_str(b, " + " S_IQ);
    if (r->tol > 0.0 || !q->def) brango_de(b, q->rlo, q->rhi, 0);
    else if (si) bval(b, r->v[q->w]);
    else bnum(b, q->t, dec_calc(q->t));
    buf_ch(b, '?');
    intento_fin(b, r, q, 0, si);
    if (si) {
        buf_str(b, " (C" S_SUB2 " = "); bval(b, r->C); buf_str(b, " - "); bval(b, q->c1); buf_str(b, " = ");
        bval(b, r->v[q->w]); buf_ch(b, ')');
    }
    buf_ch(b, '\n');
}

/* El intento de la serie: «con 20 falta 46.7 → no hay», «con 21 falta 294/7
 * = 42 → sí hay 42 ✔»; con tolerancia, «con 25 falta de 52.3 a 58.2 → no hay» */
static void intento_ser(Buf *b, const CpmRes *r, const Intento *q)
{
    int si = intento_si(r, q);
    buf_str(b, "     con "); bval(b, q->c1);
    if (q->solo && q->w < 0) { buf_str(b, " ya vale solo\n"); return; }
    buf_str(b, " falta ");
    if (r->tol > 0.0 || !q->def) brango(b, q->rlo, q->rhi);
    else {
        /* la fracción del PDF, si es de enteros: 294/7 = 42 */
        if (q->w >= 0 && entero(r->C * q->c1) && entero(q->c1 - r->C) && entero(q->t)) {
            bnum(b, r->C * q->c1, 0); buf_ch(b, '/'); bnum(b, q->c1 - r->C, 0); buf_str(b, " = ");
        }
        if (si) bval(b, r->v[q->w]);
        else bnum(b, q->t, dec_calc(q->t));
    }
    intento_fin(b, r, q, 1, si);
    buf_ch(b, '\n');
}

static int falta_corrida(const Intento *q) { return q->w < 0 && !q->solo && !q->falta_otro; }

/* Con tolerancia, ¿el rango de C₂ de q toca o pisa el de p, el intento de
 * antes? (en paralelo los rangos suben y en serie bajan; rhi < 0: sin tope) */
static int seguidos(const Intento *p, const Intento *q, int serie)
{
    if (serie) return q->rhi < 0.0 || q->rhi >= p->rlo - 1e-9 * (1.0 + p->rlo);
    return q->rlo <= p->rhi + 1e-9 * (1.0 + p->rhi);
}

/* Los intentos, con las tandas largas de «no hay» resumidas sin perder el
 * primero ni el último: «con 15 a 19 falta de 210 a 53.2: no hay» y luego
 * el del 20 entero. Con tolerancia, el resumen da todo lo que se buscó (la
 * unión de los rangos: «del 48 al 43: faltaría de 1 a 8»), y por eso una
 * tanda se corta donde los rangos dejan un hueco */
static void intentos(Buf *b, const CpmRes *r, int ni, int serie)
{
    int i = 0, k;
    while (i < ni) {
        int j = i;
        while (j < ni && falta_corrida(&it[j]) && (j == i || r->tol == 0.0 || seguidos(&it[j - 1], &it[j], serie))) j++;
        if (j - i > 4) {
            const Intento *p = &it[i], *u = &it[j - 2];
            buf_str(b, serie ? "     con " : "     del "); bval(b, p->c1); buf_str(b, serie ? " a " : " al "); bval(b, u->c1);
            if (r->tol > 0.0) {
                buf_str(b, serie ? " falta " : ": faltar" S_ii "a ");
                if (serie) brango(b, u->rlo, p->rhi); else brango(b, p->rlo, u->rhi);
                buf_str(b, serie ? ": no hay\n" : "; no hay\n");
            } else if (p->def && u->def) {
                buf_str(b, serie ? " falta de " : ": faltar" S_ii "a de ");
                bnum(b, p->t, dec_calc(p->t)); buf_str(b, " a "); bnum(b, u->t, dec_calc(u->t));
                buf_str(b, serie ? ": no hay\n" : "; no hay\n");
            } else buf_str(b, ": no hay ninguno\n");
            if (serie) intento_ser(b, r, &it[j - 1]); else intento_par(b, r, &it[j - 1]);
            i = j;
        } else if (j > i) {
            for (k = i; k < j; k++) { if (serie) intento_ser(b, r, &it[k]); else intento_par(b, r, &it[k]); }
            i = j;
        } else { if (serie) intento_ser(b, r, &it[i]); else intento_par(b, r, &it[i]); i++; }
    }
}

/* El paralelo del profe: C₁ baja desde el mayor menor que C (con la
 * tolerancia, menor que C + t), C₂ no mayor que C₁ (el mismo valor, si hay
 * dos); hasta la elegida o hasta que dos veces C₁ ya no llega. Devuelve
 * cuántos intentos */
static int barre_par(const CpmRes *r, const CpmComb *S)
{
    int i, j, ni = 0;
    for (i = r->n - 1; i >= 0; i--) {
        double c1 = r->v[i];
        Intento *q;
        if (!(c1 < r->hi)) continue;
        if (2.0 * c1 < r->lo - CPM_EPS) break;           /* ningún C₂ <= C₁ llega */
        q = &it[ni++];
        q->i = i;
        q->c1 = c1;
        q->def = c1 < r->C;
        q->t = r->C - c1;
        q->rlo = r->lo - c1 > 0.0 ? r->lo - c1 : 0.0;
        q->rhi = r->hi - c1;
        q->solo = cpm_vale(r, c1);
        q->w = -1; q->ct = 0.0;
        for (j = 0; j <= i; j++) {
            double ct;
            if (!cabe2(r, j, i)) continue;
            ct = r->v[j] + c1;
            if (cpm_vale(r, ct) && (q->w < 0 || dist_q(r, ct) < dist_q(r, q->ct))) { q->w = j; q->ct = ct; }
        }
        q->elegida = S && S->tipo == CPM_PAR && S->b == i;
        if (q->elegida) { q->w = S->a; q->ct = S->ct; }
        q->falta_otro = q->w < 0 && r->q[i] < 2 && cpm_vale(r, c1 + c1);
        if (q->elegida) break;
    }
    return ni;
}

/* La serie del profe: C₁ sube desde el menor mayor que C (que C - t), C₂ no
 * menor que C₁; hasta la elegida o hasta que C₁/2 ya se pasa */
static int barre_ser(const CpmRes *r, const CpmComb *S)
{
    int i, j, ni = 0;
    for (i = 0; i < r->n; i++) {
        double c1 = r->v[i];
        Intento *q;
        if (!(c1 > r->lo)) continue;
        if (c1 > 2.0 * r->hi + CPM_EPS) break;           /* CT >= C₁/2: se pasa */
        q = &it[ni++];
        q->i = i;
        q->c1 = c1;
        q->def = c1 > r->C;
        q->t = q->def ? r->C * c1 / (c1 - r->C) : 0.0;
        q->rlo = r->lo > 0.0 ? r->lo * c1 / (c1 - r->lo) : 0.0;
        q->rhi = c1 > r->hi ? r->hi * c1 / (c1 - r->hi) : -1.0;
        q->solo = cpm_vale(r, c1);
        q->w = -1; q->ct = 0.0;
        for (j = i; j < r->n; j++) {
            double ct;
            if (!cabe2(r, i, j)) continue;
            ct = c1 * r->v[j] / (c1 + r->v[j]);
            if (cpm_vale(r, ct) && (q->w < 0 || dist_q(r, ct) < dist_q(r, q->ct))) { q->w = j; q->ct = ct; }
        }
        q->elegida = S && S->tipo == CPM_SER && S->a == i;
        if (q->elegida) { q->w = S->b; q->ct = S->ct; }
        q->falta_otro = q->w < 0 && r->q[i] < 2 && cpm_vale(r, c1 / 2.0);
        if (q->elegida) break;
    }
    return ni;
}

static int alguno(int ni) { int i; for (i = 0; i < ni; i++) if (it[i].w >= 0) return 1; return 0; }

/* «Izquierdas del 7 y del 10 unidas → A.» y lo demás del cableado. Con
 * valores repetidos, «los dos de 7», «un 28» y «el otro 28»; en la serie de
 * tres, el 1.º, el 2.º y el 3.º */
static void cableado(Buf *b, const CpmRes *r, const CpmComb *k)
{
    const double *v = r->v;
    buf_str(b, "   L" S_ii "nea 1 " S_RARR " C (la punteada).\n");
    switch (k->tipo) {
    case CPM_UNO:
        buf_str(b, "   A " S_RARR " una terminal del "); bval(b, v[k->a]); buf_str(b, ".\n");
        buf_str(b, "   La otra " S_RARR " l" S_ii "nea 2 y puente a T.\n");
        break;
    case CPM_PAR:
        if (k->a == k->b) { buf_str(b, "   Izquierdas de los dos de "); bval(b, v[k->a]); }
        else { buf_str(b, "   Izquierdas del "); bval(b, v[k->a]); buf_str(b, " y del "); bval(b, v[k->b]); }
        buf_str(b, " unidas " S_RARR " A.\n");
        buf_str(b, "   Derechas unidas " S_RARR " l" S_ii "nea 2 y puente a T.\n");
        break;
    case CPM_SER:
        if (k->a == k->b) {
            buf_str(b, "   A " S_RARR " izquierda de un "); bval(b, v[k->a]); buf_str(b, ".\n");
            buf_str(b, "   Su derecha " S_RARR " izquierda del otro "); bval(b, v[k->b]);
            buf_str(b, " (punto medio, sin otra conexi" S_oo "n).\n");
            buf_str(b, "   Derecha del otro "); bval(b, v[k->b]); buf_str(b, " " S_RARR " l" S_ii "nea 2 y T.\n");
            break;
        }
        buf_str(b, "   A " S_RARR " izquierda del "); bval(b, v[k->a]); buf_str(b, ".\n");
        buf_str(b, "   Derecha del "); bval(b, v[k->a]); buf_str(b, " " S_RARR " izquierda del ");
        bval(b, v[k->b]); buf_str(b, " (punto medio, sin otra conexi" S_oo "n).\n");
        buf_str(b, "   Derecha del "); bval(b, v[k->b]); buf_str(b, " " S_RARR " l" S_ii "nea 2 y T.\n");
        break;
    case CPM_PAR3:
        buf_str(b, "   Izquierdas de los tres ("); bval(b, v[k->a]); buf_str(b, ", "); bval(b, v[k->b]);
        buf_str(b, " y "); bval(b, v[k->c]); buf_str(b, ") unidas " S_RARR " A.\n");
        buf_str(b, "   Derechas unidas " S_RARR " l" S_ii "nea 2 y puente a T.\n");
        break;
    case CPM_SER3:
        buf_str(b, "   A " S_RARR " izquierda del 1.º, el "); bval(b, v[k->a]); buf_str(b, ".\n");
        buf_str(b, "   Derecha del 1.º " S_RARR " izquierda del 2.º, el "); bval(b, v[k->b]);
        buf_str(b, " (punto medio).\n");
        buf_str(b, "   Derecha del 2.º " S_RARR " izquierda del 3.º, el "); bval(b, v[k->c]);
        buf_str(b, " (otro punto medio).\n");
        buf_str(b, "   Derecha del 3.º " S_RARR " l" S_ii "nea 2 y T.\n");
        break;
    case CPM_MIX1: {
        /* el de la serie, si vale lo mismo que uno del paralelo: «el otro 7»
         * (o «el tercer 7», con los dos del paralelo también de 7) */
        const char *otro = k->c == k->a && k->a == k->b ? "tercer " : k->c == k->a || k->c == k->b ? "otro " : "";
        if (k->a == k->b) { buf_str(b, "   A " S_RARR " izquierdas de los dos de "); bval(b, v[k->a]); }
        else { buf_str(b, "   A " S_RARR " izquierdas del "); bval(b, v[k->a]); buf_str(b, " y del "); bval(b, v[k->b]); }
        buf_str(b, " (unidas).\n");
        buf_str(b, "   Sus derechas (unidas) " S_RARR " izquierda del "); buf_str(b, otro); bval(b, v[k->c]);
        buf_str(b, " (punto medio).\n");
        buf_str(b, "   Derecha del "); buf_str(b, otro); bval(b, v[k->c]); buf_str(b, " " S_RARR " l" S_ii "nea 2 y T.\n");
        break;
    }
    default:                        /* a y b en serie arriba, c abajo */
        buf_str(b, "   A " S_RARR " izquierdas del "); bval(b, v[k->a]); buf_str(b, " de arriba y del ");
        bval(b, v[k->c]); buf_str(b, " de abajo (unidas).\n");
        buf_str(b, "   Arriba: derecha del "); bval(b, v[k->a]);
        buf_str(b, k->a == k->b ? " " S_RARR " izquierda del otro " : " " S_RARR " izquierda del ");
        bval(b, v[k->b]); buf_str(b, " (punto medio).\n");
        buf_str(b, "   Derechas del "); bval(b, v[k->b]); buf_str(b, " de arriba y del "); bval(b, v[k->c]);
        buf_str(b, " de abajo (unidas) " S_RARR " l" S_ii "nea 2 y T.\n");
        break;
    }
}

/* El que falta, en el texto. Con tolerancia, los enteros que sirven (el
 * convenio del PDF y de la especificación: «el 42 con uno de 47 a 50», de
 * 46.2 a 50.8) y un ejemplo; si no hay ningún entero, el rango con
 * decimales. Sin tolerancia, el valor (con sus decimales: 7.05); si es el
 * mismo c1 de la bodega, «otro de 7» */
static void bfalta(Buf *b, const CpmRes *r, double lo, double hi, double t, double c1)
{
    if (r->tol > 0.0) {
        double el = ceil(lo - 1e-9 * (1.0 + fabs(lo))), eh = floor(hi + 1e-9 * (1.0 + fabs(hi)));
        long e;
        buf_str(b, "uno ");
        if (el < 1.0) el = 1.0;
        if (el > eh) { brango(b, lo, hi); return; }
        buf_str(b, "de "); bnum(b, el, 0);
        if (eh > el) {
            buf_str(b, " a "); bnum(b, eh, 0);
            /* y un entero de ejemplo, si el rango de verdad es más ancho: el
             * más cercano al nominal (con el 38 en serie, 55: el del profe) */
            if (!(entero(lo) && entero(hi))) {
                e = (long)floor(t + 0.5);
                if ((double)e < el) e = (long)el;
                if ((double)e > eh) e = (long)eh;
                buf_str(b, " (p. ej. "); buf_int(b, e); buf_ch(b, ')');
            }
        }
    } else if (c1 > 0.0 && fabs(t - c1) <= 1e-9 * (1.0 + c1)) { buf_str(b, "otro de "); bval(b, c1); }
    else { buf_str(b, "uno de "); bnum(b, t, dec_calc(t)); }
}

/* Lo que faltaría para hacerlo con dos, de menor a mayor lo que falta */
static void faltas(Buf *b, const CpmRes *r, const char *titulo)
{
    static CpmFalta fp[CPM_MAX], fs[CPM_MAX];
    int np = cpm_faltas(r, CPM_PAR, fp, CPM_MAX), ns = cpm_faltas(r, CPM_SER, fs, CPM_MAX), i;
    double lo, hi, mlo;
    const char *mde = "directo";
    double mc1 = 0.0, mhi, mt = r->C;
    int u = uno_cercano(r);
    cpm_falta_directo(r, &lo, &hi);
    buf_str(b, titulo);
    if (u >= 0) {                                         /* (con «2»: uno solo ya vale) */
        buf_str(b, "   Directo: ya lo hay (el "); bval(b, r->v[u]); buf_str(b, "), pero se piden dos.\n");
        lo = 1e300;                                       /* (no cuenta para el menor) */
    } else { buf_str(b, "   Directo: "); bfalta(b, r, lo, hi, r->C, 0.0); buf_str(b, ".\n"); }
    mlo = lo; mhi = hi;
    if (np > 0) buf_str(b, "   Paralelo:\n");
    for (i = np - 1; i >= 0; i--) {                       /* el 20 con 2 a 3, antes que el 15 */
        buf_str(b, "     el "); bval(b, fp[i].c1); buf_str(b, " con ");
        bfalta(b, r, fp[i].lo, fp[i].hi, r->C - fp[i].c1, fp[i].c1); buf_ch(b, '\n');
        if (fp[i].lo < mlo) { mlo = fp[i].lo; mhi = fp[i].hi; mc1 = fp[i].c1; mde = "paralelo"; mt = r->C - fp[i].c1; }
    }
    if (ns > 0) buf_str(b, "   Serie:\n");
    for (i = ns - 1; i >= 0; i--) {                       /* el 42 con 47 a 50, antes que el 38 */
        double t = r->C * fs[i].c1 / (fs[i].c1 - r->C);
        buf_str(b, "     el "); bval(b, fs[i].c1); buf_str(b, " con ");
        bfalta(b, r, fs[i].lo, fs[i].hi, t, fs[i].c1); buf_ch(b, '\n');
        if (fs[i].lo < mlo) { mlo = fs[i].lo; mhi = fs[i].hi; mc1 = fs[i].c1; mde = "serie"; mt = t; }
    }
    if (np + ns > 0) {
        buf_str(b, "   El menor que har" S_ii "a falta: ");
        bfalta(b, r, mlo, mhi, mt, mde[0] == 'd' ? 0.0 : mc1);
        if (mde[0] == 'd') buf_str(b, ", directo.\n");
        else { buf_str(b, ", con el "); bval(b, mc1); buf_str(b, " en "); buf_str(b, mde); buf_str(b, ".\n"); }
    }
}

/* «Bodega: 10 capacitores, de 1 a 12; repetidos: 7×2.» */
static void bbodega(Buf *b, const CpmRes *r)
{
    int i, rep = 0;
    buf_str(b, "Bodega: ");
    if (r->ncap == 1) { buf_str(b, "un solo capacitor, de "); bval(b, r->v[0]); buf_str(b, " MF.\n"); return; }
    buf_int(b, r->ncap); buf_str(b, " capacitores");
    if (r->n > 1) { buf_str(b, ", de "); bval(b, r->v[0]); buf_str(b, " a "); bval(b, r->v[r->n - 1]); }
    else { buf_str(b, ", todos de "); bval(b, r->v[0]); }
    for (i = 0; i < r->n; i++) if (r->q[i] > 1) rep++;
    if (rep == 0) { buf_str(b, " (uno de cada).\n"); return; }
    if (r->n == 1) { buf_str(b, ".\n"); return; }
    buf_str(b, "; repetidos:");
    for (i = 0, rep = 0; i < r->n; i++)
        if (r->q[i] > 1) {
            if (++rep > 8) { buf_str(b, " ..."); break; }
            buf_ch(b, ' '); bval(b, r->v[i]); buf_str(b, S_TIMES); buf_int(b, r->q[i]);
        }
    buf_str(b, ".\n");
}

/* El paso del paralelo de dos (con sus intentos); devuelve si alguno sirve */
static int paso_par(Buf *b, const CpmRes *r, const CpmComb *S, int paso, int primera)
{
    int ni, m, hay_menor = r->n > 0 && r->v[0] < r->C, sirve = 0;
    buf_int(b, paso);
    if (r->ncap < 2) { buf_str(b, ". Paralelo: hacen falta dos y solo hay un capacitor.\n"); return 0; }
    if (!hay_menor) { buf_str(b, ". Paralelo: no hay valores menores que "); bval(b, r->C); buf_str(b, ".\n"); return 0; }
    {
        /* los dos capacitores más chicos (el mismo valor, si hay dos) */
        double x = r->v[0], y = r->q[0] > 1 ? r->v[0] : r->v[1];
        if (x + y > r->hi + CPM_EPS) {
            buf_str(b, ". Paralelo no sirve: la suma m" S_ii "nima es ");
            bval(b, x); buf_str(b, " + "); bval(b, y); buf_str(b, " = ");
            bcalc(b, x + y, r->hi); buf_str(b, " MF > "); bval(b, r->hi); buf_str(b, ".\n");
            return 0;
        }
    }
    ni = barre_par(r, S);
    if (ni == 0) {
        m = 0;
        while (m < r->n && r->v[m] < r->hi) m++;
        if (m >= 1 && (m >= 2 || r->q[0] > 1)) {
            double x = r->v[m - 1], y = r->q[m - 1] > 1 ? x : r->v[m - 2];
            buf_str(b, ". Paralelo no sirve: la suma m" S_aa "xima es ");
            bval(b, x); buf_str(b, " + "); bval(b, y); buf_str(b, " = ");
            bcalc(b, x + y, r->lo); buf_str(b, " MF < "); bval(b, r->lo); buf_str(b, ".\n");
        } else { buf_str(b, ". Paralelo: solo hay un capacitor menor que "); bval(b, r->C); buf_str(b, ".\n"); }
        return 0;
    }
    buf_str(b, ". En paralelo los valores SE SUMAN: busco dos que sumen ");
    if (r->tol > 0.0) { buf_str(b, "de "); bval(b, r->lo); buf_str(b, " a "); bval(b, r->hi); }
    else bval(b, r->C);
    buf_str(b, ". Pruebo con el m" S_aa "s grande que cabe y miro si existe el que falta:\n");
    intentos(b, r, ni, 0);
    sirve = alguno(ni);
    if (!sirve) buf_str(b, "   No sale en paralelo.\n");
    else if (S && S->tipo == CPM_SER && !primera) buf_str(b, "   Sirve; la opci" S_oo "n elegida es en serie.\n");
    else if (S && S->tipo == CPM_SER) {
        /* el mejor paralelo que sirve, frente a la serie: las dos
         * diferencias, con los decimales que las distinguen */
        int i, mp = -1;
        for (i = 0; i < ni; i++) if (it[i].w >= 0 && (mp < 0 || dist_q(r, it[i].ct) < dist_q(r, it[mp].ct))) mp = i;
        buf_str(b, "   Sirve ("); bdif_frente(b, r, it[mp].ct, S->ct);
        buf_str(b, "), pero en serie queda m" S_aa "s cerca ("); bdif_frente(b, r, S->ct, it[mp].ct); buf_str(b, ").\n");
    }
    return sirve;
}

/* El paso de la serie de dos */
static void paso_ser(Buf *b, const CpmRes *r, const CpmComb *S, int paso)
{
    /* hay con qué: un valor mayor que C o, si la tolerancia llega a C/2 (dos
     * menores que C pueden dar lo que vale), mayor que C - t */
    int ni, i, hay_mayor = r->n > 0 && (r->v[r->n - 1] > r->C || (r->C / 2.0 >= r->lo - CPM_EPS && r->v[r->n - 1] > r->lo));
    buf_int(b, paso);
    if (r->ncap < 2) { buf_str(b, ". Serie: hacen falta dos y solo hay un capacitor.\n"); return; }
    if (!hay_mayor) { buf_str(b, ". Serie: no hay valores mayores que "); bval(b, r->C); buf_str(b, ".\n"); return; }
    {
        /* los dos más grandes (el mismo valor, si hay dos) */
        double c = r->v[r->n - 1], a = r->q[r->n - 1] > 1 ? c : r->v[r->n - 2], smax = a * c / (a + c);
        int k2 = -1;
        if (smax < r->lo - CPM_EPS) {
            buf_str(b, ". Serie con dos no sirve: el m" S_aa "ximo posible es ");
            bval(b, a); buf_str(b, S_CDOT); bval(b, c); buf_ch(b, '/'); bval(b, a + c);
            buf_str(b, " = "); bcalc(b, smax, r->lo); buf_str(b, " MF < "); bval(b, r->lo); buf_str(b, ".\n");
            return;
        }
        ni = barre_ser(r, S);
        buf_str(b, ". En serie el total queda MENOR que el m" S_aa "s chico: busco dos mayores que ");
        bval(b, r->tol > 0.0 ? r->lo : r->C);
        buf_str(b, ". Para cada C" S_SUB1 " el que falta es C" S_SUB2 " = ");
        if (r->tol > 0.0) buf_str(b, "C"); else bval(b, r->C);
        buf_str(b, S_CDOT "C" S_SUB1 "/(C" S_SUB1 " - ");
        if (r->tol > 0.0) buf_str(b, "C"); else bval(b, r->C);
        buf_ch(b, ')');
        if (r->tol > 0.0) { buf_str(b, ", con C de "); bval(b, r->lo); buf_str(b, " a "); bval(b, r->hi); }
        if (ni > 0) { buf_str(b, "; subo desde el "); bval(b, it[0].c1); }
        buf_str(b, ":\n");
        /* la regla de los iguales: dos de 2C darían C (el más cercano) */
        for (i = 0; i < r->n; i++)
            if (cpm_vale(r, r->v[i] / 2.0) && (k2 < 0 || fabs(r->v[i] / 2.0 - r->C) < fabs(r->v[k2] / 2.0 - r->C))) k2 = i;
        if (k2 >= 0) {
            buf_str(b, "     Dos iguales dan la mitad: dos de "); bval(b, r->v[k2]); buf_str(b, " dar" S_ii "an ");
            bcalc(b, r->v[k2] / 2.0, r->C);
            if (r->q[k2] < 2) buf_str(b, ", pero solo hay uno.\n");
            else if (S && S->tipo == CPM_SER && S->a == k2 && S->b == k2) buf_str(b, " " S_RARR " hay dos " S_CHECK "\n");
            else buf_str(b, " " S_RARR " hay dos (sirve).\n");
        }
        if (ni == 0) {
            int m = 0;
            while (m < r->n && !(r->v[m] > r->lo)) m++;
            if (m < r->n && (r->n - m >= 2 || r->q[m] > 1)) {
                double x = r->v[m], y = r->q[m] > 1 ? x : r->v[m + 1];
                buf_str(b, "     No sirve: el m" S_ii "nimo es "); bval(b, x); buf_str(b, S_CDOT); bval(b, y);
                buf_ch(b, '/'); bval(b, x + y); buf_str(b, " = "); bcalc(b, x * y / (x + y), r->hi);
                buf_str(b, " MF > "); bval(b, r->hi); buf_str(b, ".\n");
            } else buf_str(b, "     No sale en serie: solo hay un capacitor mayor.\n");
        } else {
            intentos(b, r, ni, 1);
            if (!alguno(ni)) buf_str(b, "     No sale en serie.\n");
        }
    }
}

/* El paso de tres (§5.4): cómo se busca, con palabras; la cuenta va en la
 * comprobación */
static void paso_tres(Buf *b, const CpmRes *r, const CpmComb *S, int paso)
{
    buf_int(b, paso);
    switch (S->tipo) {
    case CPM_PAR3:
        buf_str(b, ". Con tres en paralelo se suman los tres: busco tres que sumen ");
        bval(b, r->C);
        buf_str(b, ". Para cada par a y b, el que falta es c = "); bval(b, r->C);
        buf_str(b, " - a - b; si est" S_aa " en la bodega, sirve.\n");
        break;
    case CPM_SER3:
        buf_str(b, ". Con tres en serie se suman los INVERSOS: 1/CT = 1/a + 1/b + 1/c, y el total queda menor que el m"
                   S_aa "s chico: busco tres mayores que ");
        bval(b, r->tol > 0.0 ? r->lo : r->C);
        buf_str(b, ". Para cada par a y b, el que falta es c = 1/(1/"); bval(b, r->C); buf_str(b, " - 1/a - 1/b).\n");
        break;
    case CPM_MIX1:
        buf_str(b, ". Mixto: primero el paralelo (a + b) y luego en serie con c: CT = (a + b)" S_CDOT "c/(a + b + c). "
                   "Para cada par a " S_PAR " b que sume m" S_aa "s que ");
        bval(b, r->C); buf_str(b, ", el que falta es c = "); bval(b, r->C); buf_str(b, S_CDOT "(a + b)/(a + b - ");
        bval(b, r->C); buf_str(b, ").\n");
        break;
    default:
        buf_str(b, ". Mixto: primero la serie a" S_CDOT "b/(a + b) y luego en paralelo con c: CT = a" S_CDOT "b/(a + b) + c. "
                   "Para cada par a y b en serie que d" S_ee " menos que ");
        bval(b, r->C); buf_str(b, ", el que falta es c = "); bval(b, r->C); buf_str(b, " - a" S_CDOT "b/(a + b).\n");
        break;
    }
    if (r->tol > 0.0) { buf_str(b, "   Con la tolerancia vale de "); bval(b, r->lo); buf_str(b, " a "); bval(b, r->hi); buf_str(b, ".\n"); }
    buf_str(b, "   Prob" S_ee " todos los tr" S_ii "os (cada capacitor, una vez): ");
    buf_int(b, r->tot[3]); buf_str(b, r->tot[3] == 1 ? " entra" : " entran");
    if (r->tot[3] > r->nver[3] && r->nver[3] > 0) { buf_str(b, "; la lista ense" S_nn "a los "); buf_int(b, r->nver[3]); buf_str(b, " m" S_aa "s cercanos"); }
    buf_str(b, ".\n");
}

/* La comprobación de k */
static void comprobacion(Buf *b, const CpmRes *r, const CpmComb *S, int paso)
{
    const double *v = r->v;
    buf_int(b, paso); buf_str(b, ". Comprobaci" S_oo "n:");
    switch (S->tipo) {
    case CPM_UNO:
        buf_str(b, " CT = ");
        break;
    case CPM_PAR:
        buf_str(b, " CT = "); bval(b, v[S->b]); buf_str(b, " + "); bval(b, v[S->a]); buf_str(b, " = ");
        break;
    case CPM_SER: {
        double a = v[S->a], c = v[S->b];
        buf_str(b, "\n   CT = ");
        bval(b, a); buf_str(b, S_CDOT); bval(b, c); buf_str(b, "/("); bval(b, a); buf_str(b, " + "); bval(b, c); buf_str(b, ") = ");
        if (entero(a * c) && entero(a + c)) { bnum(b, a * c, 0); buf_ch(b, '/'); bnum(b, a + c, 0); buf_str(b, " = "); }
        break;
    }
    case CPM_PAR3:
        buf_str(b, " CT = "); bval(b, v[S->c]); buf_str(b, " + "); bval(b, v[S->b]); buf_str(b, " + "); bval(b, v[S->a]);
        buf_str(b, " = ");
        break;
    case CPM_SER3: {
        double x = v[S->a], y = v[S->b], z = v[S->c];
        buf_str(b, "\n   1/CT = 1/"); bval(b, x); buf_str(b, " + 1/"); bval(b, y); buf_str(b, " + 1/"); bval(b, z);
        buf_str(b, "\n   CT = "); bval(b, x); buf_str(b, S_CDOT); bval(b, y); buf_str(b, S_CDOT); bval(b, z);
        buf_str(b, "/("); bval(b, x); buf_str(b, S_CDOT); bval(b, y); buf_str(b, " + "); bval(b, y); buf_str(b, S_CDOT);
        bval(b, z); buf_str(b, " + "); bval(b, x); buf_str(b, S_CDOT); bval(b, z); buf_str(b, ")\n      = ");
        if (entero(x * y * z) && entero(x * y + y * z + x * z)) {
            bnum(b, x * y * z, 0); buf_ch(b, '/'); bnum(b, x * y + y * z + x * z, 0); buf_str(b, " = ");
        }
        break;
    }
    case CPM_MIX1: {
        double x = v[S->a], y = v[S->b], z = v[S->c], s = x + y;
        buf_str(b, "\n   "); bval(b, x); buf_str(b, " " S_PAR " "); bval(b, y); buf_str(b, " = "); bval(b, x);
        buf_str(b, " + "); bval(b, y); buf_str(b, " = "); bmedio(b, s); buf_str(b, " MF\n   ");
        bmedio(b, s); buf_str(b, " en serie con "); bval(b, z); buf_str(b, ": ");
        bmedio(b, s); buf_str(b, S_CDOT); bval(b, z); buf_str(b, "/("); bmedio(b, s); buf_str(b, " + "); bval(b, z); buf_str(b, ") = ");
        if (entero(s * z) && entero(s + z)) { bnum(b, s * z, 0); buf_ch(b, '/'); bnum(b, s + z, 0); buf_str(b, " = "); }
        break;
    }
    default: {
        /* la serie de a y b, con los decimales del CT (y no menos que los de
         * c), para que la suma que se ve dé el CT que se ve: «9·32/(9 + 32) =
         * 288/41 ≈ 7.02 MF» y «CT = 7.02 + 10 = 17.02 MF» */
        double x = v[S->a], y = v[S->b], z = v[S->c], p = x * y / (x + y), pr;
        int d = dcalc(S->ct, r->C), dz = dec_de(z), exacta;
        if (d < 0) d = dec_de(r->C);
        if (d < dz) d = dz;
        pr = floor(p * p10(d) + 0.5) / p10(d);
        exacta = fabs(p - pr) <= 1e-9 * (1.0 + p);
        buf_str(b, "\n   "); bval(b, x); buf_str(b, " y "); bval(b, y); buf_str(b, " en serie: "); bval(b, x);
        buf_str(b, S_CDOT); bval(b, y); buf_str(b, "/("); bval(b, x); buf_str(b, " + "); bval(b, y); buf_ch(b, ')');
        if (entero(x * y) && entero(x + y)) { buf_str(b, " = "); bnum(b, x * y, 0); buf_ch(b, '/'); bnum(b, x + y, 0); }
        buf_str(b, exacta ? " = " : " " S_APROX " "); bnum(b, pr, d); buf_str(b, " MF\n   ");
        bnum(b, pr, d); buf_str(b, " " S_PAR " "); bval(b, z); buf_str(b, ": CT = "); bnum(b, pr, d); buf_str(b, " + "); bval(b, z);
        buf_str(b, " = ");
        if (dcalc(S->ct, r->C) < 0) bnum(b, r->C, DEC_V); else bnum(b, pr + z, d);
        buf_str(b, " MF " S_CHECK);
        if (dist_q(r, S->ct) != 0) { buf_str(b, " ("); bdif_op(b, r, S); buf_ch(b, ')'); }
        buf_ch(b, '\n');
        return;
    }
    }
    bcalc(b, S->ct, r->C); buf_str(b, " MF " S_CHECK);
    if (dist_q(r, S->ct) != 0) { buf_str(b, " ("); bdif_op(b, r, S); buf_ch(b, ')'); }
    buf_ch(b, '\n');
}

/* La línea del porqué: paralelo o serie (§4) */
static void porque(Buf *b, const CpmRes *r)
{
    int hay_menor = r->v[0] < r->C, hay_mayor = r->v[r->n - 1] > r->C, todos;
    if (r->modo == 3) {
        buf_str(b, "Con tres: en paralelo se suman (da m" S_aa "s que el mayor); en serie queda menos que el m" S_aa "s chico; "
                   "los mixtos quedan en medio.\n");
        return;
    }
    if (hay_menor && hay_mayor) {
        buf_str(b, "Hay valores menores y mayores que "); bval(b, r->C); buf_str(b, ": pueden servir paralelo y serie.\n");
    } else if (hay_mayor) {
        todos = r->v[0] > r->C;
        buf_str(b, todos ? "Todos son mayores que " : "Ninguno es menor que "); bval(b, r->C);
        buf_str(b, ": el paralelo no puede (sumar" S_ii "a m" S_aa "s); solo la serie.\n");
    } else if (hay_menor) {
        todos = r->v[r->n - 1] < r->C;
        buf_str(b, todos ? "Todos son menores que " : "Ninguno es mayor que "); bval(b, r->C);
        if (r->C / 2.0 < r->lo - CPM_EPS)
            buf_str(b, ": la serie no puede (dar" S_ii "a menos que el menor); solo el paralelo.\n");
        else {                                            /* (dos iguales a C dan C/2: la tolerancia llega) */
            buf_str(b, ": sirve el paralelo; con esta tolerancia (vale desde "); bval(b, r->lo);
            buf_str(b, ") quiz" S_aa " tambi" S_ee "n la serie, aunque da menos que el menor.\n");
        }
    }
}

int cpm_proced(const CpmRes *r, const CpmComb *S, char *txt, size_t cap)
{
    int paso = 1, c, primera, k;
    long t12 = r->tot[1] + r->tot[2];
    Buf b;
    buf_init(&b, txt, cap);
    if (!S && r->nval > 0) S = &r->val[0];
    c = S ? cpm_ncap(S->tipo) : 0;
    /* ¿es la primera de su número de capacitores (la más cercana)? */
    primera = 0;
    for (k = 0; S && k < r->nval; k++)
        if (cpm_ncap(r->val[k].tipo) == c) { primera = !cmp_tipo_valores(&r->val[k], S); break; }

    /* El dato y la bodega */
    if (r->tol > 0.0) {
        buf_str(&b, "Rango aceptable: "); bval(&b, r->lo); buf_str(&b, " a "); bval(&b, r->hi);
        buf_str(&b, " MF ("); bval(&b, r->C); buf_str(&b, " " S_PM " "); bval(&b, r->tol); buf_str(&b, ").\n");
    } else {
        buf_str(&b, "Se busca "); bval(&b, r->C);
        buf_str(&b, " MF exacto (a un decimal: " S_PM "0.05).\n");
    }
    if (r->n == 0) {
        buf_str(&b, "La bodega est" S_aa " vac" S_ii "a: no hay con qu" S_ee ".\n");
        buf_str(&b, "IRRESOLUBLE con esta bodega.\n");
        if (cpm_con_faltas(r)) faltas(&b, r, "Te faltar" S_ii "a (uno m" S_aa "s en la bodega lo resuelve):\n");
        return paso;
    }
    bbodega(&b, r);
    if (r->modo == 2) buf_str(&b, "Capacitores: 2 (lo pide el profe): solo arreglos de dos.\n");
    if (r->modo == 3) buf_str(&b, "Capacitores: 3 (lo pide el profe): solo arreglos de tres.\n");
    porque(&b, r);
    if (r->modo == 2 && uno_cercano(r) >= 0) {           /* con «2», pero uno solo ya vale */
        int u = uno_cercano(r);
        buf_str(&b, "Ojo: el "); bval(&b, r->v[u]); buf_str(&b, " solo ya vale ("); bdif(&b, r, r->v[u]);
        buf_str(&b, "): es la sustituci" S_oo "n directa; aqu" S_ii " se piden dos.\n");
    }

    if (r->modo == 3) {
        if (S) {
            paso_tres(&b, r, S, paso++);
            comprobacion(&b, r, S, paso++);
            buf_int(&b, paso++); buf_str(&b, ". Cableado (como en el dibujo):\n");
            cableado(&b, r, S);
            return paso;
        }
        buf_str(&b, "IRRESOLUBLE con tres capacitores.\n");
        if (r->ncap < 3) buf_str(&b, "   Hacen falta tres y en la bodega hay menos.\n");
        if (r->hay_cerca) {
            buf_str(&b, "Lo m" S_aa "s cercano de tres: "); btitulo(&b, r, &r->cerca);
            buf_str(&b, " ("); bdif(&b, r, r->cerca.ct); buf_str(&b, "): no entra.\n");
        }
        if (t12 > 0) buf_str(&b, "Con uno o dos s" S_ii " se puede: elige Capacitores: Auto.\n");
        return paso;
    }

    /* 1. ¿Uno solo? (en Auto) */
    if (r->modo == CPM_AUTO) {
        const CpmComb *u = 0;
        for (k = 0; k < r->nval && !u; k++) if (r->val[k].tipo == CPM_UNO) u = &r->val[k];
        buf_int(&b, paso++); buf_str(&b, ". " S_IQ "Uno solo? ");
        if (S && c == 1) {
            buf_str(&b, "Hay de "); bval(&b, r->v[S->a]);
            if (dist_q(r, S->ct) != 0) { buf_str(&b, " ("); bdif_op(&b, r, S); buf_ch(&b, ')'); }
            buf_str(&b, ": sustituci" S_oo "n directa.\n");
        } else if (u) {
            buf_str(&b, "Hay de "); bval(&b, r->v[u->a]); buf_str(&b, " ("); bdif_op(&b, r, u);
            buf_str(&b, "): es la opci" S_oo "n 1. Sigo con la que elegiste.\n");
        } else if (r->tol > 0.0) {
            buf_str(&b, "No hay ninguno de "); bval(&b, r->lo); buf_str(&b, " a "); bval(&b, r->hi); buf_str(&b, ".\n");
        } else { buf_str(&b, "No hay de "); bval(&b, r->C); buf_str(&b, ".\n"); }
    }
    if (S && c == 1) {
        comprobacion(&b, r, S, paso++);
        buf_int(&b, paso++); buf_str(&b, ". Cableado (como en el dibujo):\n");
        cableado(&b, r, S);
        return paso;
    }
    if (S && c == 3 && t12 > 0) {
        /* lo eligió de la lista aunque con menos ya se puede */
        buf_int(&b, paso++);
        if (r->nver[1] + r->nver[2] == 1) buf_str(&b, ". Con uno o dos ya se puede (la opci" S_oo "n 1); esta es con tres.\n");
        else {
            buf_str(&b, ". Con uno o dos ya se puede (las opciones 1 a "); buf_int(&b, r->nver[1] + r->nver[2]);
            buf_str(&b, "); esta es con tres.\n");
        }
        paso_tres(&b, r, S, paso++);
        comprobacion(&b, r, S, paso++);
        buf_int(&b, paso++); buf_str(&b, ". Cableado (como en el dibujo):\n");
        cableado(&b, r, S);
        return paso;
    }

    /* 2. Paralelo y 3. serie, como el profe */
    paso_par(&b, r, S, paso++, primera);
    if (!S || S->tipo != CPM_PAR) paso_ser(&b, r, S, paso++);

    if (S && c == 3) {
        buf_int(&b, paso++); buf_str(&b, ". Con dos no se puede (" S_IQ "el profe lo puso mal?); con tres s" S_ii ":\n");
        paso_tres(&b, r, S, paso++);
    }
    if (S) {
        comprobacion(&b, r, S, paso++);
        buf_int(&b, paso++); buf_str(&b, ". Cableado (como en el dibujo):\n");
        cableado(&b, r, S);
        if (c == 3 && cpm_con_faltas(r))
            faltas(&b, r, "Si tiene que ser con dos, te faltar" S_ii "a (uno m" S_aa "s en la bodega):\n");
        return paso;
    }

    /* Irresoluble: lo más cercano y lo que faltaría */
    if (r->modo == 2) buf_str(&b, "IRRESOLUBLE con dos capacitores.\n");
    else {
        buf_str(&b, "IRRESOLUBLE con esta bodega (ni con uno, dos o tres).\n");
        if (r->ncap >= 3) buf_str(&b, "   Con tres tampoco sale: prob" S_ee " todos los tr" S_ii "os.\n");
    }
    if (r->hay_cerca) {
        buf_str(&b, "Lo m" S_aa "s cercano: "); btitulo(&b, r, &r->cerca);
        buf_str(&b, " ("); bdif(&b, r, r->cerca.ct); buf_str(&b, "): no entra.\n");
    }
    if (cpm_con_faltas(r)) faltas(&b, r, "Te faltar" S_ii "a (uno m" S_aa "s en la bodega lo resuelve con dos):\n");
    if (r->modo == 2 && uno_cercano(r) >= 0) {
        buf_str(&b, "Con uno solo s" S_ii " se puede: el "); bval(&b, r->v[uno_cercano(r)]);
        buf_str(&b, " (elige Capacitores: Auto).\n");
    }
    if (r->modo == 2 && r->tot[3] > 0) {
        buf_str(&b, "Con tres s" S_ii " se puede: "); buf_int(&b, r->tot[3]);
        buf_str(&b, r->tot[3] == 1 ? " arreglo (elige Capacitores: Auto o 3).\n" : " arreglos (elige Capacitores: Auto o 3).\n");
    }
    return paso;
}
