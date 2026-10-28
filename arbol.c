/* ============================================================================
 *  arbol.c  -  Estructura serie-paralelo  (ver arbol.h)
 * ========================================================================= */
#include <string.h>
#include <math.h>
#include "arbol.h"
#include "numfmt.h"
#include "gfx.h"

/* Una rama pendiente de reducir: nodo del arbol entre los nodos u y v de la
 * red, orientada de u a v. */
typedef struct { int u, v, nodo, viva; } Rama;

static Rama ramas[MAX_EL];
static int  nramas;

static int nuevo(Arbol *a, int tipo)
{
    NodoAr *n;
    if (a->nn >= ARB_MAX) return -1;
    n = &a->n[a->nn];
    memset(n, 0, sizeof *n);
    n->tipo = (unsigned char)tipo;
    n->el = -1;
    return a->nn++;
}

/* Da la vuelta al nodo: una serie recorrida al reves. */
static void invierte(Arbol *a, int i)
{
    NodoAr *n = &a->n[i];
    int k;
    if (n->tipo == AR_HOJA) return;
    if (n->tipo == AR_SERIE)
        for (k = 0; k < n->nh / 2; k++) {
            unsigned char t = n->h[k];
            n->h[k] = n->h[n->nh - 1 - k];
            n->h[n->nh - 1 - k] = t;
        }
    for (k = 0; k < n->nh; k++) invierte(a, n->h[k]);
}

/* Anade hijo a un nodo, aplanando si es del mismo tipo */
static void adopta(Arbol *a, int padre, int hijo)
{
    NodoAr *p = &a->n[padre], *h = &a->n[hijo];
    int k;
    p->mascara |= h->mascara;
    if (h->tipo == p->tipo) {
        for (k = 0; k < h->nh && p->nh < MAX_EL; k++) p->h[p->nh++] = h->h[k];
        h->tipo = AR_MUERTO;
        h->nh = 0;
        h->mascara = 0;
    } else if (p->nh < MAX_EL) {
        p->h[p->nh++] = (unsigned char)hijo;
    }
}

static int grado(int w)
{
    int i, g = 0;
    for (i = 0; i < nramas; i++) {
        if (!ramas[i].viva) continue;
        if (ramas[i].u == w) g++;
        if (ramas[i].v == w) g++;
    }
    return g;
}

/* Posicion en el dibujo del nodo del arbol: para ordenar las ramas en
 * paralelo de izquierda a derecha, como estan en la pizarra. */
static int posicion(const Arbol *a, const Circuito *c, int i)
{
    int k, mejor = 1 << 30;
    for (k = 0; k < c->nel; k++)
        if (a->n[i].mascara & (1u << k)) {
            const Elemento *e = &c->el[k];
            int x = e->xa < e->xb ? e->xa : e->xb;
            int y = e->ya < e->yb ? e->ya : e->yb;
            int p = x * 64 + y;
            if (p < mejor) mejor = p;
        }
    return mejor;
}

static void ordena_paralelos(Arbol *a, const Circuito *c, int i)
{
    NodoAr *n = &a->n[i];
    int j, k;
    if (n->tipo == AR_HOJA) return;
    for (k = 0; k < n->nh; k++) ordena_paralelos(a, c, n->h[k]);
    if (n->tipo != AR_PARALELO) return;
    for (j = 1; j < n->nh; j++)                       /* insercion */
        for (k = j; k > 0 && posicion(a, c, n->h[k - 1]) > posicion(a, c, n->h[k]); k--) {
            unsigned char t = n->h[k]; n->h[k] = n->h[k - 1]; n->h[k - 1] = t;
        }
}

/* Nombres: "R234" si son pocas hojas de un digito, si no "Ra", "Rb"... */
static void nombra(Arbol *a, const Circuito *c, int i, int *letra)
{
    NodoAr *n = &a->n[i];
    int k, cuenta = 0, corto = 1;
    if (n->tipo == AR_HOJA) {
        strncpy(n->nombre, c->el[n->el].etiq, sizeof n->nombre - 1);
        return;
    }
    for (k = 0; k < n->nh; k++) nombra(a, c, n->h[k], letra);
    for (k = 0; k < c->nel; k++)
        if (n->mascara & (1u << k)) {
            cuenta++;
            if (c->el[k].etiq[2] != 0) corto = 0;           /* R10 o mas */
        }
    if (corto && cuenta <= 5) {
        int p = 1;
        n->nombre[0] = 'R';
        /* en el orden de las hojas dentro del arbol, que es el del dibujo */
        {
            int pila[ARB_MAX], np = 0;
            pila[np++] = i;
            while (np > 0 && p < 7) {
                int j = pila[--np];
                if (a->n[j].tipo == AR_HOJA) { n->nombre[p++] = c->el[a->n[j].el].etiq[1]; continue; }
                for (k = a->n[j].nh - 1; k >= 0; k--) pila[np++] = a->n[j].h[k];
            }
        }
        n->nombre[p] = 0;
    } else {
        n->nombre[0] = 'R';
        n->nombre[1] = (char)('a' + (*letra)++ % 26);
        n->nombre[2] = 0;
    }
}

int arbol_construir(const Circuito *c, const Red *r, Arbol *a)
{
    int k, i, j, sp, sm, cambio, nf = 0;

    memset(a, 0, sizeof *a);
    a->raiz = -1;
    a->fuente = -1;
    nramas = 0;

    /* Una sola fuente (de cualquier tipo) y todo lo demás resistencias: si
     * hubiera otra fuente u otro componente, el árbol explicaría un
     * circuito que no es el dibujado. */
    for (k = 0; k < r->nel; k++)
        if (tipo_info(c->el[k].tipo)->fuente) { nf++; a->fuente = k; }
    if (nf != 1 || r->error != RED_OK) { a->fuente = -1; return 0; }
    for (k = 0; k < r->nel; k++)
        if (k != a->fuente && c->el[k].tipo != EL_RES) { a->fuente = -1; return 0; }
    sm = r->a[a->fuente];
    sp = r->b[a->fuente];

    for (k = 0; k < r->nel; k++) {
        int h;
        if (c->el[k].tipo != EL_RES) continue;
        if (r->a[k] == r->b[k]) { a->corto |= 1u << k; continue; }
        h = nuevo(a, AR_HOJA);
        a->n[h].el = (signed char)k;
        a->n[h].mascara = 1u << k;
        ramas[nramas].u = r->a[k];
        ramas[nramas].v = r->b[k];
        ramas[nramas].nodo = h;
        ramas[nramas].viva = 1;
        nramas++;
    }

    do {
        cambio = 0;

        /* Colgantes: nodo interior con una sola rama */
        for (i = 0; i < nramas; i++) {
            if (!ramas[i].viva) continue;
            for (j = 0; j < 2; j++) {
                int w = j ? ramas[i].v : ramas[i].u;
                if (w != sp && w != sm && grado(w) == 1) {
                    a->colgante |= a->n[ramas[i].nodo].mascara;
                    ramas[i].viva = 0;
                    cambio = 1;
                    break;
                }
            }
        }

        /* Paralelo: dos ramas entre los mismos nodos */
        for (i = 0; i < nramas && !cambio; i++) {
            if (!ramas[i].viva) continue;
            for (j = i + 1; j < nramas; j++) {
                int p, misma, cruzada;
                if (!ramas[j].viva) continue;
                misma   = ramas[j].u == ramas[i].u && ramas[j].v == ramas[i].v;
                cruzada = ramas[j].u == ramas[i].v && ramas[j].v == ramas[i].u;
                if (!misma && !cruzada) continue;
                if (cruzada) invierte(a, ramas[j].nodo);
                if (a->n[ramas[i].nodo].tipo == AR_PARALELO) p = ramas[i].nodo;
                else {
                    p = nuevo(a, AR_PARALELO);
                    if (p < 0) return 0;
                    adopta(a, p, ramas[i].nodo);
                }
                adopta(a, p, ramas[j].nodo);
                ramas[i].nodo = p;
                ramas[j].viva = 0;
                cambio = 1;
                break;
            }
        }

        /* Serie: nodo interior con exactamente dos ramas */
        for (i = 0; i < nramas && !cambio; i++) {
            int w, e1 = -1, e2 = -1, x, y, s;
            if (!ramas[i].viva) continue;
            for (j = 0; j < 2 && !cambio; j++) {
                e1 = e2 = -1;
                w = j ? ramas[i].v : ramas[i].u;
                if (w == sp || w == sm || grado(w) != 2) continue;
                for (k = 0; k < nramas; k++)
                    if (ramas[k].viva && (ramas[k].u == w || ramas[k].v == w)) {
                        if (e1 < 0) e1 = k; else e2 = k;
                    }
                if (e1 < 0 || e2 < 0 || e1 == e2) continue;
                /* e1 orientada x -> w, e2 orientada w -> y */
                if (ramas[e1].v != w) invierte(a, ramas[e1].nodo);
                x = (ramas[e1].v == w) ? ramas[e1].u : ramas[e1].v;
                if (ramas[e2].u != w) invierte(a, ramas[e2].nodo);
                y = (ramas[e2].u == w) ? ramas[e2].v : ramas[e2].u;

                s = nuevo(a, AR_SERIE);
                if (s < 0) return 0;
                adopta(a, s, ramas[e1].nodo);
                adopta(a, s, ramas[e2].nodo);
                ramas[e1].u = x; ramas[e1].v = y; ramas[e1].nodo = s;
                ramas[e2].viva = 0;
                if (x == y) {                  /* lazo colgado de un nodo */
                    a->colgante |= a->n[s].mascara;
                    ramas[e1].viva = 0;
                }
                cambio = 1;
            }
        }
    } while (cambio);

    /* Tiene que quedar una sola rama, y entre los bornes de la fuente */
    j = -1;
    for (i = 0; i < nramas; i++)
        if (ramas[i].viva) { if (j >= 0) return 0; j = i; }
    if (j < 0) return 0;                       /* circuito abierto */
    if (!((ramas[j].u == sp && ramas[j].v == sm) || (ramas[j].u == sm && ramas[j].v == sp)))
        return 0;
    if (ramas[j].u != sp) invierte(a, ramas[j].nodo);   /* de + a - */

    a->raiz = ramas[j].nodo;
    ordena_paralelos(a, c, a->raiz);
    {
        int letra = 0;
        nombra(a, c, a->raiz, &letra);
    }
    if (a->n[a->raiz].tipo != AR_HOJA) strcpy(a->n[a->raiz].nombre, "Req");
    return 1;
}

double arbol_R(const Arbol *a, int i, const double *R)
{
    const NodoAr *n = &a->n[i];
    double s = 0.0;
    int k;
    if (n->tipo == AR_HOJA) return R[n->el];
    if (n->tipo == AR_MUERTO || n->nh == 0) return 0.0;
    for (k = 0; k < n->nh; k++) {
        double h = arbol_R(a, n->h[k], R);
        if (h != h) return h;                             /* NaN */
        s += (n->tipo == AR_SERIE) ? h : 1.0 / h;
    }
    return (n->tipo == AR_SERIE) ? s : 1.0 / s;
}

static void expr(const Arbol *a, const Circuito *c, int i, Buf *b, int dentro)
{
    const NodoAr *n = &a->n[i];
    int k;
    if (n->tipo == AR_HOJA) { buf_str(b, c->el[n->el].etiq); return; }
    if (dentro) buf_ch(b, '(');
    for (k = 0; k < n->nh; k++) {
        if (k) buf_str(b, n->tipo == AR_SERIE ? " + " : " " S_PAR " ");
        expr(a, c, n->h[k], b, a->n[n->h[k]].tipo != AR_HOJA);
    }
    if (dentro) buf_ch(b, ')');
}

void arbol_expresion(const Arbol *a, const Circuito *c, int nodo, char *buf, int cap)
{
    Buf b;
    buf_init(&b, buf, (size_t)cap);
    if (nodo >= 0) expr(a, c, nodo, &b, 0);
}
