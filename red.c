/* ============================================================================
 *  red.c  -  Del dibujo a la red electrica  (ver red.h)
 * ========================================================================= */
#include "red.h"

static int padre[REJ_PUNTOS];

static int raiz(int i)
{
    while (padre[i] != i) { padre[i] = padre[padre[i]]; i = padre[i]; }
    return i;
}

static void une(int i, int j)
{
    i = raiz(i); j = raiz(j);
    if (i == j) return;
    if (i < j) padre[j] = i; else padre[i] = j;   /* raiz = punto menor */
}

#define PT(x, y) ((y) * REJ_COLS + (x))

/* Una fuente de tensión sin r (vacía, o r = 0): en corto sería imposible.
 * Con r, el corto es un problema de verdad (I = E/r). Una E controlada no
 * tiene r: en corto es como una E sin r (el emulador da SIM_LAZO_V con las
 * dos). */
static int fuente_sin_r(const Elemento *el)
{
    if (el->tipo == EL_EDEP) return 1;
    return el->tipo == EL_FUENTE &&
           (el->aux.est == M_LIBRE || (el->aux.est == M_DATO && el->aux.v == 0.0));
}

/* Grupos de nodos unidos por los elementos que cumplen el filtro (hasta que
 * no cambia nada: la red es pequeña). */
static void agrupa(const Circuito *c, const Red *r, int *grupo, int (*pasa)(const Circuito *, int, int), int extra)
{
    int k, n, cambio;
    for (n = 0; n < r->nn; n++) grupo[n] = n;
    do {
        cambio = 0;
        for (k = 0; k < c->nel; k++) {
            int ga, gb;
            if (!pasa(c, k, extra)) continue;
            ga = grupo[r->a[k]]; gb = grupo[r->b[k]];
            if (ga != gb) {
                for (n = 0; n < r->nn; n++) if (grupo[n] == gb) grupo[n] = ga;
                cambio = 1;
            }
            if (circ_npatas(&c->el[k]) == 3 && r->c[k] >= 0 && grupo[r->c[k]] != grupo[r->a[k]]) {
                gb = grupo[r->c[k]]; ga = grupo[r->a[k]];
                for (n = 0; n < r->nn; n++) if (grupo[n] == gb) grupo[n] = ga;
                cambio = 1;
            }
        }
    } while (cambio);
}

/* Lo que en CC es un cable: amperímetro ideal, bobina, interruptor cerrado */
static int es_cable_cc(const Circuito *c, int k, int extra)
{
    (void)extra;
    return circ_corto_cc(&c->el[k]);
}

/* Camino de vuelta de la J 'extra': cualquier elemento menos ella misma y
 * lo que en CC es un abierto (voltímetro ideal, condensador, interruptor
 * abierto) */
static int deja_pasar(const Circuito *c, int k, int extra)
{
    return k != extra && !circ_abierto_cc(&c->el[k]);
}

/* Un instrumento que es un cable: el amperímetro ideal y el vatímetro */
static int es_instr(int tipo) { return tipo == EL_AMP || tipo == EL_VATI; }

/* Quién pone en corto a la fuente k: los elementos que hacen de cable en CC
 * en un camino (el más corto) de un borne al otro; de ellos, una L o un S
 * antes que un amperímetro o un vatímetro. Un elemento que está en el mismo
 * grupo pero fuera de ese camino no tiene la culpa. */
static int culpa_corto(const Circuito *c, const Red *r, int k)
{
    int prev_el[MAX_NODOS], cola[MAX_NODOS], ini = 0, fin = 0, n, j, culpa = -1;
    for (n = 0; n < r->nn; n++) prev_el[n] = -2;
    prev_el[r->a[k]] = -1;
    cola[fin++] = r->a[k];
    while (ini < fin && prev_el[r->b[k]] == -2) {
        int u = cola[ini++];
        for (j = 0; j < c->nel; j++) {
            int v;
            if (j == k || !circ_corto_cc(&c->el[j])) continue;
            if (r->a[j] == u) v = r->b[j];
            else if (r->b[j] == u) v = r->a[j];
            else continue;
            if (prev_el[v] != -2) continue;
            prev_el[v] = j;
            cola[fin++] = v;
        }
    }
    if (prev_el[r->b[k]] < 0) return -1;
    for (n = r->b[k]; n != r->a[k]; ) {
        j = prev_el[n];
        if (culpa < 0 || (es_instr(c->el[culpa].tipo) && !es_instr(c->el[j].tipo))) culpa = j;
        n = r->a[j] == n ? r->b[j] : r->a[j];
    }
    return culpa;
}

int red_extraer(const Circuito *c, Red *r)
{
    int x, y, i, k, nodo_de_raiz[REJ_PUNTOS];
    int usado[REJ_PUNTOS];

    r->nn = 0;
    r->nel = c->nel;
    r->ncomp = 0;
    r->error = RED_OK;
    r->elem_error = -1;
    r->elem_culpa = -1;

    for (i = 0; i < REJ_PUNTOS; i++) {
        padre[i] = i;
        nodo_de_raiz[i] = -1;
        usado[i] = 0;
        r->punto[i] = -1;
    }

    for (y = 0; y < REJ_FILAS; y++)
        for (x = 0; x < REJ_COLS; x++) {
            if (x < REJ_COLS - 1 && c->h[y][x] == SEG_CABLE) une(PT(x, y), PT(x + 1, y));
            if (y < REJ_FILAS - 1 && c->v[y][x] == SEG_CABLE) une(PT(x, y), PT(x, y + 1));
        }

    /* Los conjuntos que tocan un borne son nodos. Se numeran en orden de
     * lectura del punto mas alto-izquierdo del conjunto, que es su raiz. */
    for (k = 0; k < c->nel; k++) {
        usado[raiz(PT(c->el[k].xa, c->el[k].ya))] = 1;
        usado[raiz(PT(c->el[k].xb, c->el[k].yb))] = 1;
        if (circ_npatas(&c->el[k]) == 3) usado[raiz(PT(c->el[k].xc, c->el[k].yc))] = 1;
    }
    for (i = 0; i < REJ_PUNTOS; i++)
        if (usado[i] && raiz(i) == i && r->nn < MAX_NODOS)
            nodo_de_raiz[i] = r->nn++;

    for (i = 0; i < REJ_PUNTOS; i++) {
        int rz = raiz(i);
        /* Un punto con segmentos pero sin borne (un cable suelto) no es nodo */
        r->punto[i] = nodo_de_raiz[rz];
    }

    for (k = 0; k < c->nel; k++) {
        r->a[k] = nodo_de_raiz[raiz(PT(c->el[k].xa, c->el[k].ya))];
        r->b[k] = nodo_de_raiz[raiz(PT(c->el[k].xb, c->el[k].yb))];
        r->c[k] = circ_npatas(&c->el[k]) == 3 ? nodo_de_raiz[raiz(PT(c->el[k].xc, c->el[k].yc))] : -1;
    }

    red_cierra(c, r);

    /* Los terminales puestos reservan su letra; la de su nodo es esa */
    for (k = 0; k < 2; k++)
        if (c->term_x[k] >= 0 && c->term_y[k] >= 0 && c->term_x[k] < REJ_COLS && c->term_y[k] < REJ_FILAS) {
            r->letras_term |= 1 << k;
            r->nodo_term[k] = r->punto[PT(c->term_x[k], c->term_y[k])];
        }

    if (c->nel == 0) { r->error = RED_VACIA; return r->error; }
    for (k = 0; k < c->nel; k++)
        if (tipo_info(c->el[k].tipo)->fuente) break;
    if (k == c->nel) { r->error = RED_SIN_FUENTE; return r->error; }
    for (k = 0; k < c->nel; k++)
        if (fuente_sin_r(&c->el[k]) && r->a[k] == r->b[k]) {
            r->error = RED_FUENTE_CORTO;
            r->elem_error = k;
            return r->error;
        }
    {
        int grupo[MAX_NODOS];
        /* Un amperímetro ideal (o una bobina, o un interruptor cerrado) es
         * un cable en CC: si une (él solo o con otros) los dos bornes de una
         * fuente de tensión sin r, la pone en corto. La culpa se la lleva,
         * de preferencia, una L o un S (el amperímetro es el caso de hoy). */
        agrupa(c, r, grupo, es_cable_cc, -1);
        for (k = 0; k < c->nel; k++)
            if (fuente_sin_r(&c->el[k]) && grupo[r->a[k]] == grupo[r->b[k]]) {
                int j;
                r->error = RED_FUENTE_AMP;
                r->elem_error = k;
                r->elem_culpa = culpa_corto(c, r, k);
                (void)j;
                return r->error;
            }
        /* Una J cuyos bornes no se unen por ningún otro camino (un borne al
         * aire, un lazo abierto, un voltímetro ideal en serie): su corriente
         * no tiene por dónde volver y el sistema sería imposible. Una J
         * controlada, igual (el emulador la cuenta como una J: SIM_CORTE_J) */
        for (k = 0; k < c->nel; k++) {
            if ((c->el[k].tipo != EL_CORR && c->el[k].tipo != EL_JDEP) || r->a[k] == r->b[k]) continue;
            agrupa(c, r, grupo, deja_pasar, k);
            if (grupo[r->a[k]] != grupo[r->b[k]]) {
                r->error = RED_J_ABIERTA;
                r->elem_error = k;
                return r->error;
            }
        }
    }
    return r->error;
}

void red_cierra(const Circuito *c, Red *r)
{
    int i, k, cambio;

    for (i = 0; i < r->nn; i++) { r->comp[i] = i; r->ref[i] = 0; }
    r->nodo_term[0] = r->nodo_term[1] = -1;       /* las letras de siempre (red_extraer pone las de los terminales) */
    r->letras_term = 0;

    /* Subcircuitos: propagar la etiqueta menor por los elementos */
    do {
        cambio = 0;
        for (k = 0; k < r->nel; k++) {
            int a = r->a[k], b = r->b[k];
            if (a < 0 || b < 0) continue;
            if (r->comp[a] < r->comp[b]) { r->comp[b] = r->comp[a]; cambio = 1; }
            else if (r->comp[b] < r->comp[a]) { r->comp[a] = r->comp[b]; cambio = 1; }
            /* un 3 patas une también su pata c (con una red armada a mano, c
             * solo cuenta en los tipos de 3 patas) */
            if (circ_npatas(&c->el[k]) == 3 && r->c[k] >= 0) {
                int cc = r->c[k];
                if (r->comp[a] < r->comp[cc]) { r->comp[cc] = r->comp[a]; cambio = 1; }
                else if (r->comp[cc] < r->comp[a]) { r->comp[a] = r->comp[cc]; cambio = 1; }
            }
        }
    } while (cambio);

    /* Referencia de cada subcircuito: la tierra, si está en él; si no, el
     * borne - de su primera fuente; si no tiene fuente, su primer nodo. */
    r->ncomp = 0;
    for (i = 0; i < r->nn; i++) {
        int cp = r->comp[i], elegido = -1;
        if (cp != i) continue;                    /* no es el representante */
        r->ncomp++;
        if (c->tierra_x >= 0 && c->tierra_y >= 0) {
            int nt = r->punto[c->tierra_y * REJ_COLS + c->tierra_x];
            if (nt >= 0 && r->comp[nt] == cp) elegido = nt;
        }
        for (k = 0; k < r->nel && elegido < 0; k++)
            if (tipo_info(c->el[k].tipo)->fuente && r->a[k] >= 0 && r->comp[r->a[k]] == cp)
                elegido = r->a[k];
        if (elegido < 0) elegido = i;
        r->ref[elegido] = 1;
    }
}

int red_grado(const Red *r, int n)
{
    int k, g = 0;
    for (k = 0; k < r->nel; k++) {
        if (r->a[k] == n) g++;
        if (r->b[k] == n) g++;
        if (r->c[k] == n && n >= 0) g++;
    }
    return g;
}

int red_ref_de(const Red *r, int n)
{
    int m;
    if (n < 0 || n >= r->nn) return -1;
    for (m = 0; m < r->nn; m++) if (r->ref[m] && r->comp[m] == r->comp[n]) return m;
    return -1;
}

int red_ref_propia(const Circuito *c, const Red *r, int n)
{
    int k;
    if (n < 0 || n >= r->nn) return 0;
    if (c->tierra_x >= 0 && c->tierra_y >= 0) {
        int nt = r->punto[c->tierra_y * REJ_COLS + c->tierra_x];
        if (nt >= 0 && r->comp[nt] == r->comp[n]) return 1;
    }
    for (k = 0; k < r->nel; k++)
        if (tipo_info(c->el[k].tipo)->fuente && r->a[k] >= 0 && r->comp[r->a[k]] == r->comp[n]) return 1;
    return 0;
}

int red_puesto(const Red *r, int n)
{
    int k, i, libre[2], nl = 0;
    if (!r || !r->letras_term || n < 0) return n;
    for (k = 0; k < 2; k++) if (n == r->nodo_term[k]) return k;    /* los dos en un nodo: la A */
    /* el i-ésimo de los demás nodos (en su orden) toma la i-ésima letra libre */
    i = n;
    if (r->nodo_term[0] >= 0 && r->nodo_term[0] < n) i--;
    if (r->nodo_term[1] >= 0 && r->nodo_term[1] < n && r->nodo_term[1] != r->nodo_term[0]) i--;
    for (k = 0; k < 2; k++) if (!((r->letras_term >> k) & 1)) libre[nl++] = k;
    return i < nl ? libre[i] : 2 + i - nl;
}

int red_nodo_puesto(const Red *r, int p)
{
    int n;
    for (n = 0; n < r->nn; n++) if (red_puesto(r, n) == p) return n;
    return -1;
}

char red_letra(const Red *r, int n)
{
    int p = red_puesto(r, n);
    if (p < 26) return (char)('A' + p);
    return (char)('a' + (p - 26) % 26);
}
