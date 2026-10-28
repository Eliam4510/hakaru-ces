/* ============================================================================
 *  circuito.c  -  El modelo  (ver circuito.h)
 * ========================================================================= */
#include <string.h>
#include <math.h>
#include "circuito.h"

static void sonda_nada(Sonda *s) { s->tipo = SONDA_NO; s->x = s->y = s->el = -1; s->sg = 0; }

void circ_init(Circuito *c, const char *nombre)
{
    memset(c, 0, sizeof *c);
    c->tierra_x = c->tierra_y = -1;
    c->term_x[0] = c->term_y[0] = c->term_x[1] = c->term_y[1] = -1;
    sonda_nada(&c->sonda[0]);
    sonda_nada(&c->sonda[1]);
    if (nombre) {
        strncpy(c->nombre, nombre, sizeof c->nombre - 1);
        c->nombre[sizeof c->nombre - 1] = 0;
    }
}

int circ_vecino(int x, int y, int d, int *nx, int *ny)
{
    static const int DX[4] = { 1, 0, -1, 0 };
    static const int DY[4] = { 0, 1, 0, -1 };
    int a, b;
    if (d < 0 || d > 3) return 0;
    a = x + DX[d];
    b = y + DY[d];
    if (a < 0 || a >= REJ_COLS || b < 0 || b >= REJ_FILAS) return 0;
    *nx = a; *ny = b;
    return 1;
}

/* Casilla del segmento, en su forma canonica (desde el punto de menor
 * coordenada). NULL si el segmento se sale de la rejilla. */
static unsigned char *casilla(Circuito *c, int x, int y, int d)
{
    int nx, ny;
    if (x < 0 || x >= REJ_COLS || y < 0 || y >= REJ_FILAS) return 0;
    if (!circ_vecino(x, y, d, &nx, &ny)) return 0;
    if (ny == y) return &c->h[y][nx < x ? nx : x];
    return &c->v[ny < y ? ny : y][x];
}

int circ_seg(const Circuito *c, int x, int y, int d)
{
    const unsigned char *p = casilla((Circuito *)c, x, y, d);
    return p ? *p : -1;
}

Mag *circ_mag(Elemento *e, int q)
{
    if (q >= Q_P1 && q < Q_TOT) return &e->par[q - Q_P1];
    return q == Q_VAL ? &e->val : q == Q_V ? &e->V : q == Q_I ? &e->I : &e->aux;
}

const Mag *circ_mag_c(const Elemento *e, int q) { return circ_mag((Elemento *)e, q); }

/* Quita el elemento k del array y renumera los segmentos que apuntan a los
 * que van detras, y las referencias de un elemento a otro (la de uno que
 * apuntaba al borrado queda en -1). Las etiquetas no cambian: "R3" sigue
 * siendo R3. */
static void borra_elemento(Circuito *c, int k)
{
    int x, y, i;
    for (y = 0; y < REJ_FILAS; y++)
        for (x = 0; x < REJ_COLS; x++) {
            unsigned char *p[2];
            p[0] = (x < REJ_COLS - 1) ? &c->h[y][x] : 0;
            p[1] = (y < REJ_FILAS - 1) ? &c->v[y][x] : 0;
            for (i = 0; i < 2; i++) {
                if (!p[i] || *p[i] < SEG_EL0) continue;
                if (*p[i] == SEG_EL0 + k)     *p[i] = SEG_VACIO;
                else if (*p[i] > SEG_EL0 + k) (*p[i])--;
            }
        }
    for (i = k; i < c->nel - 1; i++) c->el[i] = c->el[i + 1];
    c->nel--;
    for (i = 0; i < c->nel; i++) {
        if (c->el[i].ref == k) c->el[i].ref = -1;
        else if (c->el[i].ref > k) c->el[i].ref--;
    }
    /* las sondas de un elemento lo siguen; la del borrado se quita */
    for (i = 0; i < 2; i++) {
        Sonda *s = &c->sonda[i];
        if (s->tipo != SONDA_I && s->tipo != SONDA_V) continue;
        if (s->el == k) sonda_nada(s);
        else if (s->el > k) s->el--;
    }
}

void circ_borra(Circuito *c, int k)
{
    if (k >= 0 && k < c->nel) borra_elemento(c, k);
}

int circ_quitar(Circuito *c, int x, int y, int d)
{
    unsigned char *p = casilla(c, x, y, d);
    if (!p || *p == SEG_VACIO) return 0;
    if (*p >= SEG_EL0) borra_elemento(c, *p - SEG_EL0);
    else *p = SEG_VACIO;
    return 1;
}

int circ_cable(Circuito *c, int x, int y, int d)
{
    unsigned char *p = casilla(c, x, y, d);
    if (!p || !circ_seg_libre(c, x, y, d, 0)) return 0;
    if (*p >= SEG_EL0) { circ_quitar(c, x, y, d); p = casilla(c, x, y, d); }
    *p = SEG_CABLE;
    return 1;
}

/* Siguiente numero libre para la etiqueta: al borrar R2 y poner otra
 * resistencia, la nueva vuelve a ser R2. */
static void etiqueta(Circuito *c, Elemento *e)
{
    char pre = tipo_info(e->tipo)->letra;
    int n, i, usado;
    for (n = 1; n < 100; n++) {
        usado = 0;
        for (i = 0; i < c->nel; i++) {
            const char *t = c->el[i].etiq;
            int num = 0, j = 1;
            if (&c->el[i] == e || t[0] != pre) continue;
            while (t[j] >= '0' && t[j] <= '9') num = num * 10 + (t[j++] - '0');
            if (num == n) { usado = 1; break; }
        }
        if (!usado) break;
    }
    e->etiq[0] = pre;
    if (n < 10) { e->etiq[1] = (char)('0' + n); e->etiq[2] = 0; }
    else { e->etiq[1] = (char)('0' + n / 10); e->etiq[2] = (char)('0' + n % 10); e->etiq[3] = 0; }
}

static void patas_de(int tipo, int d, int esp, int ox, int oy, int px[NPATAS], int py[NPATAS]);

/* Un componente de 3 patas: todos los segmentos de su caja a SEG_EL0 + k */
static int coloca_huella(Circuito *c, TipoEl t, int x, int y, int d)
{
    int px[NPATAS], py[NPATAS], k, x0, y0, x1, y1, qx, qy;
    Elemento *e;
    if (circ_cabe(c, t, x, y, d, 0) != CM_OK) return -1;
    patas_de(t, d, 0, x, y, px, py);
    k = c->nel++;
    e = &c->el[k];
    memset(e, 0, sizeof *e);
    e->ref = -1;
    e->tipo = (unsigned char)t;
    e->xa = (unsigned char)px[0]; e->ya = (unsigned char)py[0];
    e->xb = (unsigned char)px[1]; e->yb = (unsigned char)py[1];
    e->xc = (unsigned char)px[2]; e->yc = (unsigned char)py[2];
    etiqueta(c, e);
    circ_caja(e, &x0, &y0, &x1, &y1);
    for (qy = y0; qy <= y1; qy++)
        for (qx = x0; qx <= x1; qx++) {
            if (qx < x1) c->h[qy][qx] = (unsigned char)(SEG_EL0 + k);
            if (qy < y1) c->v[qy][qx] = (unsigned char)(SEG_EL0 + k);
        }
    return k;
}

int circ_elemento(Circuito *c, TipoEl t, int x, int y, int d)
{
    unsigned char *p = casilla(c, x, y, d);
    Elemento *e;
    int nx = x, ny = y, k;

    if ((unsigned)t >= (unsigned)EL_N) return -1;       /* también un negativo (en el ARM el enum no tiene signo) */
    if (tipo_info(t)->huella) return coloca_huella(c, t, x, y, d);
    if (!p || !circ_seg_libre(c, x, y, d, 0)) return -1;
    /* Lleno: si ahí no hay un elemento que se vaya a sustituir, no se toca
     * nada (ni el cable que hubiera) */
    if (*p < SEG_EL0 && c->nel >= MAX_EL) return -1;
    if (*p != SEG_VACIO) { circ_quitar(c, x, y, d); p = casilla(c, x, y, d); }
    if (c->nel >= MAX_EL) return -1;
    circ_vecino(x, y, d, &nx, &ny);

    k = c->nel++;
    e = &c->el[k];
    memset(e, 0, sizeof *e);
    e->ref = -1;
    e->tipo = (unsigned char)t;
    e->xa = (unsigned char)x;  e->ya = (unsigned char)y;
    e->xb = (unsigned char)nx; e->yb = (unsigned char)ny;
    etiqueta(c, e);
    /* Valores por omisión (el 90 % de los enunciados): un C nuevo empieza
     * descargado, una L sin corriente, un S abierto que se cierra en t = 0 */
    switch (t) {
    case EL_COND: e->par[Q_V0 - Q_P1].v = 0.0; e->par[Q_V0 - Q_P1].est = M_DATO; break;
    case EL_BOB:  e->par[Q_I0 - Q_P1].v = 0.0; e->par[Q_I0 - Q_P1].est = M_DATO; break;
    case EL_INTER:
        e->variante = INTER_ABIERTO;
        e->par[Q_T1 - Q_P1].v = 0.0; e->par[Q_T1 - Q_P1].est = M_DATO;
        break;
    case EL_DIODO: e->variante = DIO_VG; break;          /* el del libro */
    default: break;
    }
    *p = (unsigned char)(SEG_EL0 + k);
    return k;
}

void circ_voltear(Circuito *c, int k)
{
    Elemento *e;
    unsigned char t;
    if (k < 0 || k >= c->nel) return;
    e = &c->el[k];
    t = e->xa; e->xa = e->xb; e->xb = t;
    t = e->ya; e->ya = e->yb; e->yb = t;
    {                                   /* la sonda mide lo mismo: cambia su signo */
        int i;
        for (i = 0; i < 2; i++)
            if ((c->sonda[i].tipo == SONDA_I || c->sonda[i].tipo == SONDA_V) && c->sonda[i].el == k)
                c->sonda[i].sg = (signed char)-c->sonda[i].sg;
    }
}

/* Cuenta CONEXIONES: los cables y los de dos bornes que llegan, y las patas
 * de los de 3 patas que están ahí (los segmentos de una caja no cuentan).
 * Sin componentes de 3 patas da lo mismo que contar segmentos */
int circ_grado(const Circuito *c, int x, int y)
{
    int d, n = 0, k;
    for (d = 0; d < 4; d++) {
        int s = circ_seg(c, x, y, d);
        if (s == SEG_CABLE || (s >= SEG_EL0 && s - SEG_EL0 < c->nel && circ_npatas(&c->el[s - SEG_EL0]) == 2)) n++;
    }
    for (k = 0; k < c->nel; k++)
        if (circ_npatas(&c->el[k]) == 3 && circ_pata_en(&c->el[k], x, y) >= 0) n++;
    return n;
}

/* --------------------------------------------------------------------------
 *  Etapa 4: componentes de 3 patas (§3.N de la especificación)
 * ----------------------------------------------------------------------- */
int circ_npatas(const Elemento *e) { return tipo_info(e->tipo)->huella ? 3 : 2; }

void circ_pata(const Elemento *e, int p, int *x, int *y)
{
    if (p == 0) { *x = e->xa; *y = e->ya; }
    else if (p == 1) { *x = e->xb; *y = e->yb; }
    else { *x = e->xc; *y = e->yc; }
}

int circ_pata_en(const Elemento *e, int x, int y)
{
    int p, n = circ_npatas(e), px, py;
    for (p = 0; p < n; p++) { circ_pata(e, p, &px, &py); if (px == x && py == y) return p; }
    return -1;
}

/* (ux, uy): la flecha; v' = v con espejo reflejado; la v va hacia abajo si la
 * flecha es horizontal y hacia la derecha si es vertical */
void circ_huella_pt(int tipo, int d, int esp, int ox, int oy, int u, int v, int *x, int *y)
{
    static const int DX[4] = { 1, 0, -1, 0 };
    static const int DY[4] = { 0, 1, 0, -1 };
    const Huella *h = tipo_info(tipo)->huella;
    int ux = DX[d & 3], uy = DY[d & 3], vv = v;
    if (h && esp) vv = h->v0 + h->v1 - v;
    *x = ox + u * ux + vv * (uy < 0 ? -uy : uy);
    *y = oy + u * uy + vv * (ux < 0 ? -ux : ux);
}

void circ_caja(const Elemento *e, int *x0, int *y0, int *x1, int *y1)
{
    int p, n = circ_npatas(e), x, y;
    circ_pata(e, 0, x0, y0);
    *x1 = *x0; *y1 = *y0;
    for (p = 1; p < n; p++) {
        circ_pata(e, p, &x, &y);
        if (x < *x0) *x0 = x;
        if (x > *x1) *x1 = x;
        if (y < *y0) *y0 = y;
        if (y > *y1) *y1 = y;
    }
}

/* Las tres patas de un tipo colocado con el ancla en (ox, oy) */
static void patas_de(int tipo, int d, int esp, int ox, int oy, int px[NPATAS], int py[NPATAS])
{
    const Huella *h = tipo_info(tipo)->huella;
    int p;
    for (p = 0; p < NPATAS; p++) circ_huella_pt(tipo, d, esp, ox, oy, h->pu[p], h->pv[p], &px[p], &py[p]);
}

int circ_orienta(const Elemento *e, int *d, int *esp, int *ox, int *oy)
{
    const Huella *h = tipo_info(e->tipo)->huella;
    int dd, ee, px[NPATAS], py[NPATAS], ax, ay, p;
    if (!h) return 0;
    circ_pata(e, h->ancla, &ax, &ay);
    for (dd = 0; dd < 4; dd++)
        for (ee = 0; ee < 2; ee++) {
            /* con espejo la pata ancla ya no está en el origen (en el AO el -
             * pasa a donde estaba el +): el origen sale de dónde cae ella */
            int bien = 1, qx, qy;
            circ_huella_pt(e->tipo, dd, ee, 0, 0, h->pu[h->ancla], h->pv[h->ancla], &qx, &qy);
            ax -= qx; ay -= qy;
            patas_de(e->tipo, dd, ee, ax, ay, px, py);
            for (p = 0; p < NPATAS && bien; p++) {
                int x, y;
                circ_pata(e, p, &x, &y);
                if (x != px[p] || y != py[p]) bien = 0;
            }
            if (bien) { *d = dd; *esp = ee; *ox = ax; *oy = ay; return 1; }
            ax += qx; ay += qy;
        }
    return 0;
}

int circ_mapa_huellas(const Circuito *c, signed char dueno[REJ_PUNTOS], signed char pata[REJ_PUNTOS])
{
    int k, x, y, x0, y0, x1, y1, ok = 1;
    for (k = 0; k < REJ_PUNTOS; k++) { dueno[k] = -1; pata[k] = -1; }
    for (k = 0; k < c->nel; k++) {
        const Elemento *e = &c->el[k];
        if (circ_npatas(e) != 3) continue;
        circ_caja(e, &x0, &y0, &x1, &y1);
        for (y = y0; y <= y1; y++)
            for (x = x0; x <= x1; x++) {
                int i = y * REJ_COLS + x;
                if (x < 0 || x >= REJ_COLS || y < 0 || y >= REJ_FILAS) { ok = 0; continue; }
                if (dueno[i] >= 0) { ok = 0; continue; }
                dueno[i] = (signed char)k;
                pata[i] = (signed char)circ_pata_en(e, x, y);
            }
    }
    return ok;
}

int circ_interior(const Circuito *c, int x, int y)
{
    signed char dueno[REJ_PUNTOS], pata[REJ_PUNTOS];
    int i = y * REJ_COLS + x;
    if (x < 0 || x >= REJ_COLS || y < 0 || y >= REJ_FILAS) return -1;
    circ_mapa_huellas(c, dueno, pata);
    return dueno[i] >= 0 && pata[i] < 0 ? dueno[i] : -1;
}

/* ¿Hay una marca (tierra, terminal, sonda de nodo) en el punto? Su letra, o 0 */
static char marca_en(const Circuito *c, int x, int y)
{
    int i;
    if (c->tierra_x == x && c->tierra_y == y) return 'G';
    for (i = 0; i < 2; i++) if (c->term_x[i] == x && c->term_y[i] == y) return (char)('A' + i);
    for (i = 0; i < 2; i++)
        if (c->sonda[i].tipo == SONDA_NODO && c->sonda[i].x == x && c->sonda[i].y == y) return (char)('1' + i);
    return 0;
}

int circ_cabe(const Circuito *c, int tipo, int x, int y, int d, CabeInfo *ci)
{
    signed char dueno[REJ_PUNTOS], pata[REJ_PUNTOS];
    int px[NPATAS], py[NPATAS], x0, y0, x1, y1, p, qx, qy, dd;
    CabeInfo tmp;
    if (!ci) ci = &tmp;
    ci->motivo = CM_OK; ci->x = x; ci->y = y; ci->el = -1; ci->que = 0;
    if (c->nel >= MAX_EL) { ci->motivo = CM_LLENO; return CM_LLENO; }
    if (!tipo_info(tipo)->huella) return CM_OK;
    patas_de(tipo, d, 0, x, y, px, py);
    x0 = x1 = px[0]; y0 = y1 = py[0];
    for (p = 1; p < NPATAS; p++) {
        if (px[p] < x0) x0 = px[p];
        if (px[p] > x1) x1 = px[p];
        if (py[p] < y0) y0 = py[p];
        if (py[p] > y1) y1 = py[p];
    }
    /* las esquinas de la caja */
    {
        int cx[4], cy[4], i;
        cx[0] = x0; cy[0] = y0; cx[1] = x1; cy[1] = y0; cx[2] = x0; cy[2] = y1; cx[3] = x1; cy[3] = y1;
        for (i = 0; i < 4; i++)
            if (cx[i] < 0 || cx[i] >= REJ_COLS || cy[i] < 0 || cy[i] >= REJ_FILAS) {
                ci->motivo = CM_FUERA; ci->x = cx[i]; ci->y = cy[i];
                return CM_FUERA;
            }
    }
    circ_mapa_huellas(c, dueno, pata);
    for (qy = y0; qy <= y1; qy++)
        for (qx = x0; qx <= x1; qx++) {
            int i = qy * REJ_COLS + qx, es_pata = 0;
            char m;
            for (p = 0; p < NPATAS; p++) if (px[p] == qx && py[p] == qy) es_pata = 1;
            ci->x = qx; ci->y = qy;
            if (dueno[i] >= 0) { ci->motivo = CM_OCUPADO; ci->el = dueno[i]; return CM_OCUPADO; }
            for (dd = 0; dd < 4; dd++) {
                int s = circ_seg(c, qx, qy, dd), nx = qx, ny = qy, dentro;
                if (s <= 0) continue;
                circ_vecino(qx, qy, dd, &nx, &ny);
                dentro = nx >= x0 && nx <= x1 && ny >= y0 && ny <= y1;
                if (dentro || !es_pata) {
                    ci->motivo = CM_OCUPADO;
                    ci->el = s >= SEG_EL0 ? s - SEG_EL0 : -1;
                    return CM_OCUPADO;
                }
            }
            if (!es_pata && (m = marca_en(c, qx, qy)) != 0) { ci->motivo = CM_MARCA; ci->que = m; return CM_MARCA; }
        }
    ci->x = x; ci->y = y;
    return CM_OK;
}

int circ_seg_libre(const Circuito *c, int x, int y, int d, int *k)
{
    int s = circ_seg(c, x, y, d), nx = x, ny = y, i;
    if (k) *k = -1;
    if (s < 0) return 0;
    if (s >= SEG_EL0 && circ_npatas(&c->el[s - SEG_EL0]) == 3) { if (k) *k = s - SEG_EL0; return 0; }
    circ_vecino(x, y, d, &nx, &ny);
    if ((i = circ_interior(c, x, y)) >= 0 || (i = circ_interior(c, nx, ny)) >= 0) { if (k) *k = i; return 0; }
    return 1;
}

/* --------------------------------------------------------------------------
 *  Etapa 4: consultas del circuito y fuentes controladas
 * ----------------------------------------------------------------------- */
int circ_etapa4(const Circuito *c)
{
    int k;
    for (k = 0; k < c->nel; k++) if (c->el[k].tipo >= EL_DIODO) return 1;
    return 0;
}

int circ_no_lineal(const Circuito *c)
{
    int k;
    for (k = 0; k < c->nel; k++) if (tipo_info(c->el[k].tipo)->ley_cc == LEY_NL) return 1;
    return 0;
}

int circ_busca_valor(const Circuito *c)
{
    int k;
    for (k = 0; k < c->nel; k++)
        if (c->el[k].val.est == M_BUSCAR || c->el[k].aux.est == M_BUSCAR) return 1;
    return 0;
}

int circ_v_convenio(int tipo)
{
    switch (tipo) {
    case EL_FUENTE: case EL_CORR: case EL_EDEP: case EL_JDEP: return -1;
    case EL_VATI: return 2;
    default: return 1;
    }
}

int circ_control_sig(const Circuito *c, int j, int dir)
{
    int n = c->nel, i, k, desde;
    if (j < 0 || j >= n) return -1;
    desde = c->el[j].ref;
    if (desde < -1 || desde >= n) desde = -1;
    for (i = 1; i <= n + 1; i++) {
        if (desde < 0) k = dir > 0 ? i - 1 : n - i;                 /* desde -1: el primero o el último */
        else k = ((desde + (dir > 0 ? i : -i)) % n + n) % n;
        if (k < 0 || k >= n) continue;
        if (k == j || circ_npatas(&c->el[k]) != 2) continue;
        return k;
    }
    return -1;
}

int circ_ctl_motivo(const Circuito *c, int k)
{
    const Elemento *e = &c->el[k], *x;
    if (e->ref < 0 || e->ref >= c->nel) return CTL_FALTA;
    if (e->ref == k) return CTL_SI_MISMA;
    x = &c->el[e->ref];
    if (circ_npatas(x) == 3) return CTL_3PATAS;
    if (e->variante == DEP_I && (x->tipo == EL_DIODO || x->tipo == EL_ZENER || x->tipo == EL_LED ||
                                 x->tipo == EL_IMP || x->tipo == EL_JDEP)) return CTL_I_NO;
    return CTL_OK;
}

/* --------------------------------------------------------------------------
 *  Leyes de CC, forma de las fuentes y sondas
 * ----------------------------------------------------------------------- */
int circ_abierto_cc(const Elemento *e)
{
    const TipoInfo *t = tipo_info(e->tipo);
    if (t->ley_cc == LEY_ABIERTO) return 1;
    if (t->ley_cc == LEY_INTER) return e->variante == INTER_ABIERTO;
    return e->tipo == EL_VOLT && ES_IDEAL(e);
}

int circ_corto_cc(const Elemento *e)
{
    const TipoInfo *t = tipo_info(e->tipo);
    if (t->ley_cc == LEY_CORTO) return 1;
    if (t->ley_cc == LEY_INTER) return e->variante == INTER_CERRADO;
    return e->tipo == EL_AMP && ES_IDEAL(e);
}

int circ_val_en_ley(const Elemento *e)
{
    int ley = tipo_info(e->tipo)->ley_cc;
    return (ley == LEY_VALOR && !ES_IDEAL(e)) || ley == LEY_DEP;   /* la ganancia de una controlada */
}

int circ_pide(const Circuito *c)
{
    int k, q;
    for (k = 0; k < c->nel; k++)
        for (q = 0; q < Q_N; q++) {
            const Mag *m = circ_mag_c(&c->el[k], q);
            const CampoInfo *ci;
            if (m->est == M_BUSCAR) return 1;
            /* un ángulo pedido solo cuenta en un campo con ángulo */
            ci = circ_campo(&c->el[k], q);
            if (m->est_ang == M_BUSCAR && ci && (ci->flags & CF_ANG)) return 1;
        }
    return c->frec.est == M_BUSCAR && circ_alterna(c);
}

/* --------------------------------------------------------------------------
 *  La alterna (Etapa 3)
 * ----------------------------------------------------------------------- */
int circ_alterna(const Circuito *c)
{
    int k;
    for (k = 0; k < c->nel; k++) {
        const Elemento *e = &c->el[k];
        if ((e->tipo == EL_FUENTE || e->tipo == EL_CORR) && e->variante == FORMA_FASOR) return 1;
        if (e->tipo == EL_IMP) return 1;
    }
    return 0;
}

const CampoInfo *circ_campo(const Elemento *e, int q)
{
    return tipo_campo_v(e->tipo, e->variante, q);
}

/* ¿Tiene el tipo algún campo de q con ángulo (en cualquier variante)? */
static int campo_con_ang(int tipo, int q)
{
    const TipoInfo *t = tipo_info(tipo);
    int i;
    for (i = 0; i < t->ncampos; i++)
        if (t->campos[i].q == q && (t->campos[i].flags & CF_ANG)) return 1;
    return 0;
}

int circ_quita_ca(Circuito *c)
{
    int k, q, quito = 0;
    for (k = 0; k < c->nel; k++) {
        Elemento *e = &c->el[k];
        for (q = 0; q < Q_N; q++) {
            Mag *m = circ_mag(e, q);
            if (!campo_con_ang(e->tipo, q)) continue;
            if (m->est_ang != M_LIBRE || m->ang != 0.0) { m->est_ang = M_LIBRE; m->ang = 0.0; quito = 1; }
        }
        if ((e->tipo == EL_COND || e->tipo == EL_BOB) && e->val.est == M_BUSCAR) { e->val.est = M_LIBRE; quito = 1; }
        /* la I de un C y la V de una L solo se ven en CA (en CC son 0): en CC
         * serían datos que no se ven ni se pueden borrar */
        if (e->tipo == EL_COND && e->I.est != M_LIBRE) { e->I.est = M_LIBRE; e->I.v = 0.0; quito = 1; }
        if (e->tipo == EL_BOB && e->V.est != M_LIBRE) { e->V.est = M_LIBRE; e->V.v = 0.0; quito = 1; }
        if (e->tipo == EL_VATI) {
            if (e->val.est == M_DATO) { e->val.est = M_LIBRE; e->val.v = 0.0; quito = 1; }
            if (e->aux.est != M_LIBRE) { e->aux.est = M_LIBRE; e->aux.v = 0.0; quito = 1; }
        }
    }
    if (c->frec.est == M_BUSCAR) { c->frec.est = M_LIBRE; quito = 1; }
    return quito;
}

int circ_onda_c(const Circuito *c, const Elemento *e, int *forma, double p[ONDA_PAR], int *q)
{
    int i;
    if (!c || !((e->tipo == EL_FUENTE || e->tipo == EL_CORR) && e->variante == FORMA_FASOR))
        return circ_onda(e, forma, p, q);
    for (i = 0; i < ONDA_PAR; i++) p[i] = 0.0;
    *forma = ONDA_SENO;
    if (q) *q = -1;
    if (e->val.est != M_DATO) { if (q) *q = Q_VAL; return 0; }
    if (c->frec.est != M_DATO) { if (q) *q = Q_FCIRC; return 0; }
    /* eficaz y coseno -> pico y seno: cos(x) = sen(x + 90°) */
    p[0] = sqrt(2.0) * e->val.v;
    p[1] = c->frec.v;
    p[2] = (e->val.est_ang == M_DATO ? e->val.ang : 0.0) + 90.0;
    return 1;
}

int circ_dinamico(const Circuito *c)
{
    int k;
    if (c->sonda[0].tipo != SONDA_NO || c->sonda[1].tipo != SONDA_NO) return 1;
    for (k = 0; k < c->nel; k++) {
        const Elemento *e = &c->el[k];
        if (tipo_info(e->tipo)->dinamico) return 1;
        if ((e->tipo == EL_FUENTE || e->tipo == EL_CORR) && e->variante != ONDA_CC) return 1;
        if ((e->tipo == EL_VOLT || e->tipo == EL_AMP) && e->variante != MED_CC) return 1;
    }
    return 0;
}

/* Un parámetro: su valor si es dato, o el de por omisión */
static double par_o(const Elemento *e, int q, double omision)
{
    const Mag *m = circ_mag_c(e, q);
    return m->est == M_DATO ? m->v : omision;
}

int circ_onda(const Elemento *e, int *forma, double p[ONDA_PAR], int *q)
{
    int i, f = e->variante < ONDA_N ? e->variante : ONDA_CC;
    double v1, v2;
    for (i = 0; i < ONDA_PAR; i++) p[i] = 0.0;
    *forma = f;
    if (q) *q = -1;
    if (e->val.est != M_DATO) { if (q) *q = Q_VAL; return 0; }
    if ((f == ONDA_SENO || f == ONDA_CUADRADA || f == ONDA_TRIANGULAR) && !(e->par[Q_FREC - Q_P1].est == M_DATO)) {
        if (q) *q = Q_FREC;
        return 0;
    }
    switch (f) {
    case ONDA_SENO:
        p[0] = e->val.v; p[1] = par_o(e, Q_FREC, 0.0); p[2] = par_o(e, Q_FASE, 0.0); p[3] = par_o(e, Q_BASE, 0.0);
        break;
    case ONDA_CUADRADA:
    case ONDA_TRIANGULAR:
        v2 = e->val.v; v1 = par_o(e, Q_BASE, 0.0);
        p[0] = 0.5 * (v2 - v1); p[1] = par_o(e, Q_FREC, 0.0); p[2] = par_o(e, Q_CICLO, 50.0); p[3] = 0.5 * (v1 + v2);
        break;
    case ONDA_PULSO:
        p[0] = par_o(e, Q_BASE, 0.0); p[1] = e->val.v; p[2] = par_o(e, Q_T0, 0.0); p[3] = par_o(e, Q_ANCHO, 0.0); p[4] = 0.0;
        break;
    default:
        p[0] = e->val.v;
        break;
    }
    return 1;
}

int circ_sonda_nodo(const Circuito *c, int x, int y)
{
    int i;
    for (i = 0; i < 2; i++)
        if (c->sonda[i].tipo == SONDA_NODO && c->sonda[i].x == x && c->sonda[i].y == y) return i;
    return -1;
}

int circ_sonda_elem(const Circuito *c, int k)
{
    int i;
    for (i = 0; i < 2; i++)
        if ((c->sonda[i].tipo == SONDA_I || c->sonda[i].tipo == SONDA_V) && c->sonda[i].el == k) return i;
    return -1;
}

/* El primer canal libre; con los dos ocupados, el 2 (se sustituye) */
static int canal_libre(const Circuito *c)
{
    if (c->sonda[0].tipo == SONDA_NO) return 0;
    if (c->sonda[1].tipo == SONDA_NO) return 1;
    return 1;
}

int circ_sonda_pon_nodo(Circuito *c, int x, int y, int *canal)
{
    int i = circ_sonda_nodo(c, x, y);
    if (i >= 0) { sonda_nada(&c->sonda[i]); *canal = i; return SONDA_NO; }
    i = canal_libre(c);
    sonda_nada(&c->sonda[i]);
    c->sonda[i].tipo = SONDA_NODO;
    c->sonda[i].x = (signed char)x; c->sonda[i].y = (signed char)y;
    *canal = i;
    return SONDA_NODO;
}

int circ_sonda_ciclo(Circuito *c, int k, int sg, int *canal)
{
    int i = circ_sonda_elem(c, k);
    if (i >= 0) {
        Sonda *s = &c->sonda[i];
        *canal = i;
        if (s->tipo == SONDA_I) { s->tipo = SONDA_V; return SONDA_V; }
        sonda_nada(s);
        return SONDA_NO;
    }
    i = canal_libre(c);
    sonda_nada(&c->sonda[i]);
    c->sonda[i].tipo = SONDA_I;
    c->sonda[i].el = (signed char)k;
    c->sonda[i].sg = (signed char)(sg < 0 ? -1 : 1);
    *canal = i;
    return SONDA_I;
}

void circ_sondas_limpia(Circuito *c)
{
    int i;
    for (i = 0; i < 2; i++)
        if (c->sonda[i].tipo == SONDA_NODO && circ_grado(c, c->sonda[i].x, c->sonda[i].y) == 0)
            sonda_nada(&c->sonda[i]);
}

const char *circ_nom_val(const Elemento *e)
{
    return tipo_nom_val(e->tipo);
}

int circ_valido(const Circuito *c)
{
    int k, j, x, y, d, cuenta = 0, nseg = 0;

    if (c->nel < 0 || c->nel > MAX_EL) return 0;
    for (k = 0; k < c->nel; k++) {
        const Elemento *e = &c->el[k];
        int dx = e->xa - e->xb, dy = e->ya - e->yb;
        if (e->tipo >= EL_N) return 0;
        if (e->xa >= REJ_COLS || e->xb >= REJ_COLS || e->ya >= REJ_FILAS || e->yb >= REJ_FILAS) return 0;
        if (circ_npatas(e) == 3) {
            /* 3 patas: forman una huella, su caja es suya entera */
            int dd, esp, ox, oy, x0, y0, x1, y1, qx, qy;
            if (e->xc >= REJ_COLS || e->yc >= REJ_FILAS) return 0;
            if (!circ_orienta(e, &dd, &esp, &ox, &oy)) return 0;
            circ_caja(e, &x0, &y0, &x1, &y1);
            for (qy = y0; qy <= y1; qy++)
                for (qx = x0; qx <= x1; qx++) {
                    if (qx < x1 && c->h[qy][qx] != SEG_EL0 + k) return 0;
                    if (qy < y1 && c->v[qy][qx] != SEG_EL0 + k) return 0;
                }
            nseg += (x1 - x0) * (y1 - y0 + 1) + (x1 - x0 + 1) * (y1 - y0);
            if (e->variante >= tipo_info(e->tipo)->nvar) return 0;
        } else {
            if (e->xc != 0 || e->yc != 0) return 0;
            if (dx * dx + dy * dy != 1) return 0;
            for (d = 0; d < 4; d++) {                 /* la dirección de a hacia b */
                int nx, ny;
                if (circ_vecino(e->xa, e->ya, d, &nx, &ny) && nx == e->xb && ny == e->yb) break;
            }
            if (d == 4 || circ_seg(c, e->xa, e->ya, d) != SEG_EL0 + k) return 0;
            nseg++;
        }
        for (d = 0; d < Q_TOT; d++) {
            const Mag *m = circ_mag_c(e, d);
            double lo, hi;
            if (!FINITO(m->v) || !FINITO(m->ang)) return 0;       /* NaN o Inf */
            if (d < Q_N) {
                const CampoInfo *ci = circ_campo(e, d);
                if (m->est > M_BUSCAR || m->est_ang > M_BUSCAR) return 0;
                /* el ángulo de un campo que lo tiene: en su rango. En los
                 * demás no significa nada y se acepta (lo de la Etapa 1) */
                if (ci && (ci->flags & CF_ANG) && m->est_ang == M_DATO &&
                    !(fabs(m->ang) <= ((ci->flags & CF_ANG90) ? 90.0 : 360.0))) return 0;
                if (m->est != M_DATO) continue;
                /* el rango que admite la ficha: un archivo dañado no pasa */
                lo = -1e9; hi = 1e9;
                if (d == Q_VAL) { lo = tipo_info(e->tipo)->val_min; hi = tipo_info(e->tipo)->val_max; }
                else if (d == Q_AUX) tipo_rango(e->tipo, Q_AUX, &lo, &hi);
                if (!(m->v >= lo && m->v <= hi)) return 0;
                continue;
            }
            /* parámetro del emulador: dato o vacío, sin ángulo, en su rango;
             * si el tipo no usa esa ranura, vacío */
            if (m->est > M_DATO || m->ang != 0.0 || m->est_ang != M_LIBRE) return 0;
            tipo_rango(e->tipo, d, &lo, &hi);
            if (m->est == M_DATO && (lo == 0.0 && hi == 0.0)) return 0;
            if (m->est == M_DATO && !(m->v >= lo && m->v <= hi)) return 0;
        }
        /* la variante, según el tipo (en R no se valida: no se usa) */
        if ((e->tipo == EL_FUENTE || e->tipo == EL_CORR) && e->variante >= FORMA_N) return 0;
        /* un fasor tiene módulo: sin signo */
        if ((e->tipo == EL_FUENTE || e->tipo == EL_CORR) && e->variante == FORMA_FASOR &&
            e->val.est == M_DATO && e->val.v < 0.0) return 0;
        if ((e->tipo == EL_VOLT || e->tipo == EL_AMP) && e->variante >= MED_N) return 0;
        if (e->tipo == EL_INTER && e->variante >= INTER_N) return 0;
        if ((e->tipo == EL_COND || e->tipo == EL_BOB || e->tipo == EL_IMP || e->tipo == EL_VATI) && e->variante != 0) return 0;
        /* C, L, S y Z no tienen aux; S no tiene valor. El valor de C o L en
         * [?!] se admite: en CC es un error de la calculadora (SOL_SOLO_CA),
         * en CA una incógnita */
        if ((e->tipo == EL_COND || e->tipo == EL_BOB || e->tipo == EL_INTER || e->tipo == EL_IMP) && e->aux.est != M_LIBRE) return 0;
        /* el vatímetro: P y fp sin ángulo */
        if (e->tipo == EL_VATI && (e->val.est_ang != M_LIBRE || e->aux.est_ang != M_LIBRE)) return 0;
        if (e->tipo == EL_INTER && e->val.est != M_LIBRE) return 0;
        /* Etapa 4: la variante en su tabla (NOM_DIODO, NL_COLOR...), sin aux; el
         * AO con V+ > V-; el control de una controlada, de dos bornes */
        if (e->tipo >= EL_DIODO) {
            if (e->variante >= tipo_info(e->tipo)->nvar) return 0;
            if (e->aux.est != M_LIBRE) return 0;
            if (tipo_info(e->tipo)->ley_cc == LEY_NL && e->val.est != M_LIBRE) return 0;
        }
        if (e->tipo == EL_AO) {                          /* con los vacíos, 15 y -15 (NL_VSAT) */
            double vp = e->par[QA_VP - Q_P1].est == M_DATO ? e->par[QA_VP - Q_P1].v : 15.0;
            double vn = e->par[QA_VN - Q_P1].est == M_DATO ? e->par[QA_VN - Q_P1].v : -15.0;
            if (!(vp > vn)) return 0;
        }
        if ((e->tipo == EL_EDEP || e->tipo == EL_JDEP) && e->ref >= 0 && e->ref < c->nel &&
            circ_npatas(&c->el[e->ref]) != 2) return 0;
        if (e->ref < -1 || e->ref >= c->nel || e->ref == k) return 0;
        if (!e->etiq[0] || memchr(e->etiq, 0, sizeof e->etiq) == 0) return 0;
        for (j = 0; j < k; j++) if (strcmp(e->etiq, c->el[j].etiq) == 0) return 0;
    }
    /* Tierra y terminales: un punto de la rejilla o -1 (las dos coordenadas) */
    {
        signed char px[3], py[3];
        int i;
        px[0] = c->tierra_x; py[0] = c->tierra_y;
        px[1] = c->term_x[0]; py[1] = c->term_y[0];
        px[2] = c->term_x[1]; py[2] = c->term_y[1];
        for (i = 0; i < 3; i++) {
            if (px[i] == -1 && py[i] == -1) continue;
            if (px[i] < 0 || px[i] >= REJ_COLS || py[i] < 0 || py[i] >= REJ_FILAS) return 0;
        }
    }
    if (c->conv > (CONV_PICO | CONV_SENO)) return 0;
    if (c->frec.est > M_BUSCAR || c->frec.est_ang > M_BUSCAR || !FINITO(c->frec.v) || !FINITO(c->frec.ang)) return 0;
    if (c->frec.est == M_DATO && !(c->frec.v >= 1e-3 && c->frec.v <= 1e9)) return 0;
    /* Sondas */
    for (k = 0; k < 2; k++) {
        const Sonda *s = &c->sonda[k];
        switch (s->tipo) {
        case SONDA_NO:
            if (s->x != -1 || s->y != -1 || s->el != -1 || s->sg != 0) return 0;
            break;
        case SONDA_NODO:
            if (s->x < 0 || s->x >= REJ_COLS || s->y < 0 || s->y >= REJ_FILAS || s->el != -1 || s->sg != 0) return 0;
            break;
        case SONDA_I:
        case SONDA_V:
            if (s->el < 0 || s->el >= c->nel || (s->sg != 1 && s->sg != -1) || s->x != -1 || s->y != -1) return 0;
            if (circ_npatas(&c->el[s->el]) == 3) return 0;      /* sin sondas en las patas (Etapa 5) */
            break;
        default:
            return 0;
        }
    }
    /* Tiempo y paso del emulador */
    if (c->t_fin.est > M_DATO || c->paso.est > M_DATO || !FINITO(c->t_fin.v) || !FINITO(c->paso.v)) return 0;
    if (c->t_fin.est == M_DATO && !(c->t_fin.v > 0.0 && c->t_fin.v <= 1e6)) return 0;
    if (c->paso.est == M_DATO && !(c->paso.v > 0.0 && c->paso.v <= 1.0)) return 0;

    /* Las cajas: no se pisan, y un punto interior no tiene más segmentos que
     * los de su dueño, ni tierra, ni terminal, ni sonda */
    {
        signed char dueno[REJ_PUNTOS], pata[REJ_PUNTOS];
        if (!circ_mapa_huellas(c, dueno, pata)) return 0;
        for (y = 0; y < REJ_FILAS; y++)
            for (x = 0; x < REJ_COLS; x++) {
                int i = y * REJ_COLS + x;
                if (dueno[i] < 0 || pata[i] >= 0) continue;
                for (d = 0; d < 4; d++) {
                    int s = circ_seg(c, x, y, d);
                    if (s > 0 && s != SEG_EL0 + dueno[i]) return 0;
                }
                if (marca_en(c, x, y)) return 0;
            }
    }
    /* Y al revés: los segmentos con elemento son exactamente los de todos
     * (uno por elemento de dos bornes, los de su caja por uno de 3 patas) */
    for (y = 0; y < REJ_FILAS; y++)
        for (x = 0; x < REJ_COLS; x++) {
            if (x < REJ_COLS - 1 && c->h[y][x] >= SEG_EL0) cuenta++;
            if (y < REJ_FILAS - 1 && c->v[y][x] >= SEG_EL0) cuenta++;
        }
    return cuenta == nseg;
}

/* Dos magnitudes iguales campo a campo (con el ángulo y sus dos estados) */
static int mag_igual(const Mag *a, const Mag *b)
{
    return a->v == b->v && a->ang == b->ang && a->est == b->est && a->est_ang == b->est_ang;
}

int circ_igual(const Circuito *a, const Circuito *b)
{
    int k, q;
    if (a->nel != b->nel || strcmp(a->nombre, b->nombre) != 0) return 0;
    if (memcmp(a->h, b->h, sizeof a->h) != 0 || memcmp(a->v, b->v, sizeof a->v) != 0) return 0;
    if (a->tierra_x != b->tierra_x || a->tierra_y != b->tierra_y || a->conv != b->conv) return 0;
    for (k = 0; k < 2; k++) if (a->term_x[k] != b->term_x[k] || a->term_y[k] != b->term_y[k]) return 0;
    if (!mag_igual(&a->frec, &b->frec) || !mag_igual(&a->t_fin, &b->t_fin) || !mag_igual(&a->paso, &b->paso)) return 0;
    for (k = 0; k < 2; k++) {
        const Sonda *sa = &a->sonda[k], *sb = &b->sonda[k];
        if (sa->tipo != sb->tipo || sa->x != sb->x || sa->y != sb->y || sa->el != sb->el || sa->sg != sb->sg) return 0;
    }
    for (k = 0; k < a->nel; k++) {
        const Elemento *x = &a->el[k], *y = &b->el[k];
        if (x->tipo != y->tipo || x->xa != y->xa || x->ya != y->ya || x->xb != y->xb || x->yb != y->yb) return 0;
        if (x->xc != y->xc || x->yc != y->yc || x->variante != y->variante || x->ref != y->ref) return 0;
        if (strcmp(x->etiq, y->etiq) != 0) return 0;
        for (q = 0; q < Q_TOT; q++) if (!mag_igual(circ_mag_c(x, q), circ_mag_c(y, q))) return 0;
    }
    return 1;
}
