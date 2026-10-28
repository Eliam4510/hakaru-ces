/* ============================================================================
 *  ejemplos.c  -  Los ejemplos resueltos  (ver ejemplos.h)
 *
 *  Los datos de la fuente van como los da el enunciado: la V de la fuente es
 *  su E, y la I es la corriente total que entrega.
 *
 *  El orden en que se colocan las resistencias decide sus nombres (R1, R2...)
 *  y los enunciados los usan: al retocar un circuito, respetar ese orden.
 * ========================================================================= */
#include <string.h>
#include "ejemplos.h"
#include "gfx.h"

/* --------------------------------------------------------------------------
 *  Utilidades para dibujar
 * ----------------------------------------------------------------------- */
/* Cable recto de (x0,y0) a (x1,y1), en horizontal o en vertical */
static void cab(Circuito *c, int x0, int y0, int x1, int y1)
{
    int d = (x1 > x0) ? DIR_DER : (x1 < x0) ? DIR_IZQ : (y1 > y0) ? DIR_ABA : DIR_ARR;
    while (x0 != x1 || y0 != y1) {
        int nx, ny;
        circ_cable(c, x0, y0, d);
        circ_vecino(x0, y0, d, &nx, &ny);
        x0 = nx; y0 = ny;
    }
}

static int res(Circuito *c, int x, int y, int d, double R)
{
    int k = circ_elemento(c, EL_RES, x, y, d);
    if (k >= 0) { c->el[k].val.v = R; c->el[k].val.est = M_DATO; }
    return k;
}

static int resx(Circuito *c, int x, int y, int d)
{
    int k = circ_elemento(c, EL_RES, x, y, d);
    if (k >= 0) c->el[k].val.est = M_BUSCAR;
    return k;
}

/* Fuente con el + hacia d. E > 0 dato, E < 0 incógnita, E = 0 sin dato.
 * Lo mismo con I. */
static int fue(Circuito *c, int x, int y, int d, double E, double I)
{
    int k = circ_elemento(c, EL_FUENTE, x, y, d);
    if (k < 0) return k;
    if (E > 0) { c->el[k].val.v = E; c->el[k].val.est = M_DATO; }
    else if (E < 0) c->el[k].val.est = M_BUSCAR;
    if (I > 0) { c->el[k].I.v = I; c->el[k].I.est = M_DATO; }
    else if (I < 0) c->el[k].I.est = M_BUSCAR;
    return k;
}

/* Un tramo de un camino: cable, o resistencia con sus datos */
typedef struct { double R; unsigned char est; char dq; double dv; char pq; } Tr;
#define CB        { 0, 255, 0, 0, 0 }        /* cable                     */
#define RD(v)     { v, M_DATO, 0, 0, 0 }     /* R conocida                */
#define RQ        { 0, M_BUSCAR, 0, 0, 0 }   /* R que se busca            */
#define RDI(v,i)  { v, M_DATO, 'I', i, 0 }   /* R conocida y dato de su I */
#define RDV(v,u)  { v, M_DATO, 'V', u, 0 }   /* R conocida y dato de su V */
#define RPI(v)    { v, M_DATO, 0, 0, 'I' }   /* R conocida; se pide su I  */
#define RPV(v)    { v, M_DATO, 0, 0, 'V' }   /* R conocida; se pide su V  */

static void tramo(Circuito *c, int x, int y, int d, const Tr *t)
{
    int k;
    if (t->est == 255) { circ_cable(c, x, y, d); return; }
    k = circ_elemento(c, EL_RES, x, y, d);
    if (k < 0) return;
    c->el[k].val.est = t->est;
    c->el[k].val.v = t->R;
    if (t->dq == 'I') { c->el[k].I.est = M_DATO; c->el[k].I.v = t->dv; }
    if (t->dq == 'V') { c->el[k].V.est = M_DATO; c->el[k].V.v = t->dv; }
    if (t->pq == 'I') c->el[k].I.est = M_BUSCAR;
    if (t->pq == 'V') c->el[k].V.est = M_BUSCAR;
}

static void camino(Circuito *c, int x, int y, int d, const Tr *t, int n)
{
    int i, nx, ny;
    for (i = 0; i < n; i++) {
        tramo(c, x, y, d, &t[i]);
        if (!circ_vecino(x, y, d, &nx, &ny)) return;
        x = nx; y = ny;
    }
}
#define CAMINO(c, x, y, d, ...) do { static const Tr _t[] = { __VA_ARGS__ }; \
    camino(c, x, y, d, _t, (int)(sizeof _t / sizeof _t[0])); } while (0)

/* --------------------------------------------------------------------------
 *  Plantillas de circuito
 * ----------------------------------------------------------------------- */
/* Un solo lazo: fuente a la izquierda, resistencia a la derecha */
static void lazo(Circuito *c, double E, double I, double R)
{
    cab(c, 2, 1, 6, 1);
    cab(c, 2, 3, 6, 3);
    fue(c, 2, 2, DIR_ARR, E, I);
    cab(c, 2, 2, 2, 3);
    if (R > 0) res(c, 6, 1, DIR_ABA, R); else resx(c, 6, 1, DIR_ABA);
    cab(c, 6, 2, 6, 3);
}

/* Fuente vertical en la columna 1 entre las filas 1 (+) y 3 */
static void fuente_izq(Circuito *c, double E, double I)
{
    fue(c, 1, 2, DIR_ARR, E, I);
    cab(c, 1, 2, 1, 3);
}

/* Parte de abajo de un lazo o de un paralelo: cable de (1,3) a (x,3) */
static void abajo(Circuito *c, int x) { cab(c, 1, 3, x, 3); }

/* --------------------------------------------------------------------------
 *  Problemas del pizarrón
 * ----------------------------------------------------------------------- */
static void ej_a(Circuito *c) { lazo(c, 440, 7, -1); }
static void ej_b(Circuito *c) { lazo(c, -1, 32, 12); }
static void ej_c(Circuito *c) { lazo(c, -1, 15, 54); }

static void ej_d(Circuito *c)
{
    cab(c, 1, 1, 2, 1); res(c, 2, 1, DIR_DER, 3); cab(c, 3, 1, 4, 1);
    res(c, 4, 1, DIR_DER, 3); cab(c, 5, 1, 7, 1);
    res(c, 7, 1, DIR_ABA, 3); cab(c, 7, 2, 7, 3);
    cab(c, 1, 3, 2, 3); res(c, 2, 3, DIR_DER, 3); cab(c, 3, 3, 4, 3);
    res(c, 4, 3, DIR_DER, 3); cab(c, 5, 3, 7, 3);
    fue(c, 1, 2, DIR_ARR, -1, 3); cab(c, 1, 2, 1, 3);
}

static void ej_e(Circuito *c)
{
    int x;
    cab(c, 1, 1, 9, 1);
    cab(c, 1, 3, 9, 3);
    for (x = 1; x <= 9; x += 2) {
        if (x == 5) fue(c, 5, 2, DIR_ARR, 110, -1);
        else res(c, x, 1, DIR_ABA, 3);
        cab(c, x, 2, x, 3);
    }
}

static void ej_f(Circuito *c)
{
    cab(c, 1, 1, 2, 1); res(c, 2, 1, DIR_DER, 3); cab(c, 3, 1, 6, 1);
    res(c, 1, 1, DIR_ABA, 3); cab(c, 1, 2, 1, 3);
    cab(c, 1, 3, 2, 3); res(c, 2, 3, DIR_DER, 3); cab(c, 3, 3, 6, 3);
    fue(c, 4, 2, DIR_ARR, -1, 7); cab(c, 4, 2, 4, 3);
    res(c, 6, 1, DIR_ABA, 3); cab(c, 6, 2, 6, 3);
}

static void ej_g(Circuito *c)
{
    cab(c, 1, 1, 5, 1);
    cab(c, 1, 4, 7, 4);
    cab(c, 1, 1, 1, 2); res(c, 1, 2, DIR_ABA, 6); cab(c, 1, 3, 1, 4);
    res(c, 3, 1, DIR_ABA, 3); res(c, 3, 2, DIR_ABA, 1); res(c, 3, 3, DIR_ABA, 2);
    cab(c, 5, 1, 5, 2); fue(c, 5, 3, DIR_ARR, 110, -1); cab(c, 5, 3, 5, 4);
    res(c, 5, 1, DIR_DER, 1); cab(c, 6, 1, 7, 1); cab(c, 7, 1, 7, 2);
    res(c, 7, 2, DIR_ABA, 2); cab(c, 7, 3, 7, 4);
}

static void ej_h(Circuito *c)
{
    cab(c, 1, 1, 5, 1);
    cab(c, 1, 4, 6, 4);
    res(c, 1, 1, DIR_ABA, 2); res(c, 1, 2, DIR_ABA, 1);
    cab(c, 1, 3, 0, 3); res(c, 0, 3, DIR_ABA, 4); cab(c, 0, 4, 1, 4);
    resx(c, 1, 3, DIR_ABA);
    cab(c, 3, 1, 3, 2); res(c, 3, 2, DIR_ABA, 5); cab(c, 3, 3, 3, 4);
    cab(c, 5, 1, 5, 2); fue(c, 5, 3, DIR_ARR, 5, 4); cab(c, 5, 3, 5, 4);
    res(c, 5, 1, DIR_DER, 1); cab(c, 6, 1, 7, 1); cab(c, 7, 1, 7, 2);
    res(c, 7, 2, DIR_ABA, 1); cab(c, 7, 3, 7, 4);
    res(c, 6, 4, DIR_DER, 0.5);
}

static void ej_i(Circuito *c)
{
    cab(c, 0, 1, 9, 1);
    cab(c, 0, 4, 9, 4);
    res(c, 0, 1, DIR_ABA, 6); cab(c, 0, 2, 0, 3); res(c, 0, 3, DIR_ABA, 3);
    res(c, 2, 1, DIR_ABA, 3); resx(c, 2, 2, DIR_ABA); res(c, 2, 3, DIR_ABA, 2);
    cab(c, 4, 1, 4, 2); res(c, 4, 2, DIR_ABA, 9); cab(c, 4, 3, 4, 4);
    cab(c, 5, 1, 5, 2); fue(c, 5, 3, DIR_ARR, 3, 2); cab(c, 5, 3, 5, 4);
    res(c, 7, 1, DIR_ABA, 1); res(c, 7, 2, DIR_ABA, 2); res(c, 7, 3, DIR_ABA, 3);
    cab(c, 9, 1, 9, 2); res(c, 9, 2, DIR_ABA, 6); cab(c, 9, 3, 9, 4);
}

/* --------------------------------------------------------------------------
 *  Ley de Ohm
 * ----------------------------------------------------------------------- */
static void ohm_i(Circuito *c) { lazo(c, 12, -1, 4); }
static void ohm_v(Circuito *c) { lazo(c, -1, 2, 10); }
static void ohm_r(Circuito *c) { lazo(c, 9, 0.5, -1); }
static void ohm_k(Circuito *c) { lazo(c, 5, -1, 2200); }

/* --------------------------------------------------------------------------
 *  Serie: fuente a la izquierda, resistencias arriba y a la derecha
 * ----------------------------------------------------------------------- */
static void s_dos(Circuito *c)
{
    CAMINO(c, 1, 1, DIR_DER, CB, RD(2), CB, RPV(4), CB);
    CAMINO(c, 6, 1, DIR_ABA, CB, CB);
    abajo(c, 6);
    fuente_izq(c, 12, -1);
}

static void s_tres(Circuito *c)
{
    CAMINO(c, 1, 1, DIR_DER, CB, RD(5), CB, RD(10), CB);
    CAMINO(c, 6, 1, DIR_ABA, RD(15), CB);
    abajo(c, 6);
    fuente_izq(c, 30, -1);
}

static void s_div(Circuito *c)
{
    CAMINO(c, 1, 1, DIR_DER, CB, RD(1000), CB);
    CAMINO(c, 4, 1, DIR_ABA, RPV(2000), CB);
    abajo(c, 4);
    fuente_izq(c, 9, 0);
}

static void s_dato(Circuito *c)
{
    CAMINO(c, 1, 1, DIR_DER, CB, RD(2), CB, RDV(3, 6), CB);
    CAMINO(c, 6, 1, DIR_ABA, RD(5), CB);
    abajo(c, 6);
    fuente_izq(c, -1, 0);
}

static void s_r(Circuito *c)
{
    CAMINO(c, 1, 1, DIR_DER, CB, RD(4), CB, RQ, CB);
    CAMINO(c, 6, 1, DIR_ABA, CB, CB);
    abajo(c, 6);
    fuente_izq(c, 24, 2);
}

/* --------------------------------------------------------------------------
 *  Paralelo: fuente a la izquierda, ramas en las columnas 3, 5 y 7
 * ----------------------------------------------------------------------- */
static void paralelo(Circuito *c, int xmax)
{
    cab(c, 1, 1, xmax, 1);
    abajo(c, xmax);
}

static void p_dos(Circuito *c)
{
    paralelo(c, 5);
    fuente_izq(c, 12, -1);
    CAMINO(c, 3, 1, DIR_ABA, RPI(6), CB);
    CAMINO(c, 5, 1, DIR_ABA, RPI(3), CB);
}

static void p_tres(Circuito *c)
{
    paralelo(c, 7);
    fuente_izq(c, 20, -1);
    CAMINO(c, 3, 1, DIR_ABA, RD(10), CB);
    CAMINO(c, 5, 1, DIR_ABA, RD(20), CB);
    CAMINO(c, 7, 1, DIR_ABA, RD(20), CB);
}

static void p_div(Circuito *c)
{
    paralelo(c, 5);
    fuente_izq(c, 0, 3);
    CAMINO(c, 3, 1, DIR_ABA, RPI(2), CB);
    CAMINO(c, 5, 1, DIR_ABA, RPI(4), CB);
}

static void p_r(Circuito *c)
{
    paralelo(c, 5);
    fuente_izq(c, 12, 3);
    CAMINO(c, 3, 1, DIR_ABA, RD(6), CB);
    CAMINO(c, 5, 1, DIR_ABA, RQ, CB);
}

/* --------------------------------------------------------------------------
 *  Mixtos
 * ----------------------------------------------------------------------- */
/* R1 en serie y luego R2 en paralelo con R3: la plantilla de varios ejemplos */
static void serie_par(Circuito *c, double E, double I, Tr r1, Tr r2, Tr r3)
{
    fuente_izq(c, E, I);
    cab(c, 1, 1, 2, 1);
    tramo(c, 2, 1, DIR_DER, &r1);
    cab(c, 3, 1, 6, 1);
    tramo(c, 4, 1, DIR_ABA, &r2);
    cab(c, 4, 2, 4, 3);
    tramo(c, 6, 1, DIR_ABA, &r3);
    cab(c, 6, 2, 6, 3);
    abajo(c, 6);
}

static void m_sp(Circuito *c)
{
    static const Tr a = RD(4), b = RD(6), d = RPI(3);
    serie_par(c, 24, -1, a, b, d);
}

static void m_rama(Circuito *c)
{
    cab(c, 1, 1, 5, 1);
    cab(c, 1, 4, 6, 4);
    cab(c, 1, 1, 1, 2); fue(c, 1, 3, DIR_ARR, 10, -1); cab(c, 1, 3, 1, 4);
    CAMINO(c, 3, 1, DIR_ABA, CB, RD(5), CB);
    res(c, 5, 1, DIR_ABA, 2);
    res(c, 5, 2, DIR_ABA, 6); cab(c, 5, 3, 5, 4);
    cab(c, 5, 2, 6, 2); res(c, 6, 2, DIR_ABA, 3); cab(c, 6, 3, 6, 4);
}

static void m_esc(Circuito *c)
{
    fuente_izq(c, 18, -1);
    CAMINO(c, 1, 1, DIR_DER, CB, RD(2), CB);
    CAMINO(c, 4, 1, DIR_ABA, RD(6), CB);
    CAMINO(c, 4, 1, DIR_DER, CB, RD(1), CB);
    CAMINO(c, 7, 1, DIR_ABA, RPI(2), CB);
    abajo(c, 7);
}

/* --------------------------------------------------------------------------
 *  Despejar
 * ----------------------------------------------------------------------- */
static void d_otra(Circuito *c)
{
    static const Tr a = RD(4), b = RDI(6, 2), d = RQ;
    serie_par(c, 24, 0, a, b, d);
}

static void d_fuente(Circuito *c)
{
    static const Tr a = RD(10), b = RD(20), d = RDI(20, 0.5);
    serie_par(c, -1, 0, a, b, d);
}

static void d_dos(Circuito *c)
{
    CAMINO(c, 1, 1, DIR_DER, CB, RDV(2, 4), CB, RQ, CB);
    CAMINO(c, 6, 1, DIR_ABA, CB, CB);
    abajo(c, 6);
    fuente_izq(c, 12, 0);
}

/* --------------------------------------------------------------------------
 *  Potencia
 * ----------------------------------------------------------------------- */
static void w_serie(Circuito *c)
{
    CAMINO(c, 1, 1, DIR_DER, CB, RD(2), CB, RD(4), CB);
    CAMINO(c, 6, 1, DIR_ABA, CB, CB);
    abajo(c, 6);
    fuente_izq(c, 12, -1);
}

static void w_par(Circuito *c)
{
    paralelo(c, 5);
    fuente_izq(c, 12, -1);
    CAMINO(c, 3, 1, DIR_ABA, RD(6), CB);
    CAMINO(c, 5, 1, DIR_ABA, RD(3), CB);
}

/* --------------------------------------------------------------------------
 *  Kirchhoff: lo que no se reduce
 *  Puente: R1 arriba-izq, R2 arriba-der, R3 abajo-izq, R4 abajo-der, R5 en
 *  medio (de B a C).
 * ----------------------------------------------------------------------- */
static void puente(Circuito *c, double r1, double r2, double r3, double r4, double r5)
{
    static Tr m;
    cab(c, 1, 1, 1, 3);
    fue(c, 1, 4, DIR_ARR, 12, -1);
    cab(c, 1, 4, 1, 5);
    cab(c, 1, 1, 6, 1);
    cab(c, 1, 5, 6, 5);
    res(c, 2, 1, DIR_ABA, r1);
    res(c, 6, 1, DIR_ABA, r2);
    res(c, 2, 3, DIR_ABA, r3);
    res(c, 6, 3, DIR_ABA, r4);
    m.R = r5; m.est = M_DATO; m.dq = 0; m.dv = 0; m.pq = 'I';
    tramo(c, 3, 3, DIR_DER, &m);
    cab(c, 2, 2, 2, 3); cab(c, 2, 4, 2, 5);
    cab(c, 6, 2, 6, 3); cab(c, 6, 4, 6, 5);
    cab(c, 2, 3, 3, 3); cab(c, 4, 3, 6, 3);
}

static void k_puente(Circuito *c) { puente(c, 2, 4, 4, 2, 3); }
static void k_equil(Circuito *c)  { puente(c, 2, 3, 4, 6, 5); }

static void k_dos(Circuito *c)
{
    cab(c, 1, 1, 7, 1);
    cab(c, 1, 4, 7, 4);
    res(c, 1, 1, DIR_ABA, 2);
    fue(c, 1, 3, DIR_ARR, 12, 0);
    cab(c, 1, 3, 1, 4);
    CAMINO(c, 4, 1, DIR_ABA, RPI(4), CB, CB);
    res(c, 7, 1, DIR_ABA, 2);
    fue(c, 7, 3, DIR_ARR, 6, -1);
    cab(c, 7, 3, 7, 4);
}

/* --------------------------------------------------------------------------
 *  Fuentes de corriente, instrumentos y fuente real
 * ----------------------------------------------------------------------- */
/* Fuente de corriente en la columna 1, flecha hacia arriba (su corriente
 * sale por la fila 1). J > 0 dato, J < 0 incógnita; pide_v: se pregunta su V. */
static int jfu_izq(Circuito *c, double J, int pide_v)
{
    int k = circ_elemento(c, EL_CORR, 1, 2, DIR_ARR);
    if (k < 0) return k;
    if (J > 0) { c->el[k].val.v = J; c->el[k].val.est = M_DATO; }
    else if (J < 0) c->el[k].val.est = M_BUSCAR;
    if (pide_v) c->el[k].V.est = M_BUSCAR;
    cab(c, 1, 2, 1, 3);
    return k;
}

static void j_sola(Circuito *c)
{
    paralelo(c, 5);
    jfu_izq(c, 3, 1);
    CAMINO(c, 3, 1, DIR_ABA, RD(6), CB);
    CAMINO(c, 5, 1, DIR_ABA, RD(3), CB);
}

static void j_serie(Circuito *c)
{
    CAMINO(c, 1, 1, DIR_DER, CB, RD(5), CB, CB, CB);
    CAMINO(c, 6, 1, DIR_ABA, RPV(10), CB);
    abajo(c, 6);
    jfu_izq(c, 2, 0);
}

static void j_hallar(Circuito *c)
{
    paralelo(c, 5);
    jfu_izq(c, -1, 0);
    CAMINO(c, 3, 1, DIR_ABA, RDV(4, 6), CB);
    CAMINO(c, 5, 1, DIR_ABA, RD(12), CB);
}

/* Divisor de 10 k + 10 k a 12 V, con un voltímetro de 10 k sobre R2 */
static void i_volt(Circuito *c)
{
    int k;
    cab(c, 1, 1, 4, 1);
    abajo(c, 6);
    fuente_izq(c, 12, 0);
    res(c, 4, 1, DIR_ABA, 10000);
    res(c, 4, 2, DIR_ABA, 10000);
    cab(c, 4, 2, 6, 2);
    k = circ_elemento(c, EL_VOLT, 6, 2, DIR_ABA);
    if (k < 0) return;
    c->el[k].val.v = 10000; c->el[k].val.est = M_DATO;
    c->el[k].V.est = M_BUSCAR;
}

/* 12 V con 1 y 3 ohm en serie; el amperímetro ideal, en el cable de abajo */
static void i_amp(Circuito *c)
{
    int k;
    CAMINO(c, 1, 1, DIR_DER, CB, RD(1), CB, RD(3), CB);
    CAMINO(c, 6, 1, DIR_ABA, CB, CB);
    fuente_izq(c, 12, 0);
    cab(c, 1, 3, 3, 3);
    k = circ_elemento(c, EL_AMP, 4, 3, DIR_IZQ);
    if (k >= 0) c->el[k].I.est = M_BUSCAR;
    cab(c, 4, 3, 6, 3);
}

/* Lazo con una fuente real: r > 0 dato, r < 0 incógnita */
static void lazo_real(Circuito *c, double E, double r, double I, double R, int pide_v)
{
    int k;
    cab(c, 2, 1, 6, 1);
    cab(c, 2, 3, 6, 3);
    k = fue(c, 2, 2, DIR_ARR, E, I);
    if (k >= 0) {
        if (r > 0) { c->el[k].aux.v = r; c->el[k].aux.est = M_DATO; }
        else if (r < 0) c->el[k].aux.est = M_BUSCAR;
        if (pide_v) c->el[k].V.est = M_BUSCAR;
    }
    cab(c, 2, 2, 2, 3);
    res(c, 6, 1, DIR_ABA, R);
    cab(c, 6, 2, 6, 3);
}

static void f_real(Circuito *c) { lazo_real(c, 12, 0.5, 0, 5.5, 1); }
static void f_r(Circuito *c)    { lazo_real(c, 9, -1, 1.5, 4, 0); }

/* --------------------------------------------------------------------------
 *  El catálogo
 * ----------------------------------------------------------------------- */
#define O S_OHM

const Ejemplo EJEMPLOS[EJ_N] = {
    /* ---- Pizarrón ---- */
    [EJ_PA] = { "a) Ohm: hallar R",
        "Una fuente de 440\240V da 7\240A a una resistencia. ¿Cuánto vale R?",
        "Ohm despejando R: R = V/I.",
        ej_a, "R1", 'X', 440.0 / 7.0 },
    [EJ_PB] = { "b) Ohm: hallar V",
        "Por una resistencia de 12\240" O " pasan 32\240A. ¿Qué tensión tiene la fuente?",
        "Ohm directo: V = I·R.",
        ej_b, "E1", 'X', 384.0 },
    [EJ_PC] = { "c) Ohm: hallar V",
        "Por una resistencia de 54\240" O " pasan 15\240A. ¿Qué tensión tiene la fuente?",
        "Ohm directo: V = I·R.",
        ej_c, "E1", 'X', 810.0 },
    [EJ_PD] = { "d) Cinco en serie",
        "Cinco resistencias de 3\240" O " en serie; la corriente es 3\240A. ¿Qué tensión da la fuente?",
        "En serie se suman: Req = 15\240" O ". Cada una se lleva 9\240V.",
        ej_d, "E1", 'X', 45.0 },
    [EJ_PE] = { "e) Cuatro en paralelo",
        "Cuatro resistencias de 3\240" O " en paralelo con una fuente de 110\240V. ¿Qué corriente da la fuente?",
        "n iguales en paralelo: Req = R/n.",
        ej_e, "E1", 'I', 440.0 / 3.0 },
    [EJ_PF] = { "f) Serie dentro de paralelo",
        "La fuente da 7\240A. ¿Qué tensión tiene? La rama izquierda son tres de 3\240" O " en serie.",
        "Primero la serie de la rama, luego el paralelo.",
        ej_f, "E1", 'X', 15.75 },
    [EJ_PG] = { "g) Tres ramas",
        "Fuente de 110\240V con tres ramas. ¿Cuánto valen Req y la corriente total?",
        "Sumar cada rama y combinar las tres en paralelo.",
        ej_g, "E1", 'I', 220.0 / 3.0 },
    [EJ_PH] = { "h) R en un paralelo",
        "V = 5\240V e I = 4\240A. ¿Cuánto vale R (en paralelo con la de 4\240" O ")?",
        "Req = V/I y luego despejar bajando por el circuito.",
        ej_h, "R4", 'X', 4.0 },
    [EJ_PI] = { "i) R en una de seis ramas",
        "V = 3\240V e I = 2\240A. ¿Cuánto vale R?",
        "Restar las conductancias de las ramas conocidas.",
        ej_i, "R4", 'X', 4.0 },

    /* ---- Ley de Ohm ---- */
    [EJ_OHM_I] = { "Hallar la corriente",
        "Una fuente de 12\240V alimenta una resistencia de 4\240" O ". ¿Qué corriente circula?",
        "I = V/R = 12/4 = 3\240A.",
        ohm_i, "E1", 'I', 3.0 },
    [EJ_OHM_V] = { "Hallar la tensión",
        "Por una resistencia de 10\240" O " circulan 2\240A. ¿Qué tensión da la fuente?",
        "V = I·R = 2·10 = 20\240V.",
        ohm_v, "E1", 'X', 20.0 },
    [EJ_OHM_R] = { "Hallar la resistencia",
        "Una fuente de 9\240V da 0,5\240A. ¿Qué resistencia tiene conectada?",
        "R = V/I = 9/0,5 = 18\240" O ".",
        ohm_r, "R1", 'X', 18.0 },
    [EJ_OHM_K] = { "Con kilohmios",
        "Una fuente de 5\240V y una resistencia de 2,2\240k" O ". ¿Qué corriente circula?",
        "2,2\240k" O " = 2200\240" O ". La corriente sale en milésimas de amperio (mA).",
        ohm_k, "E1", 'I', 5.0 / 2200.0 },

    /* ---- Serie ---- */
    [EJ_S_DOS] = { "Dos en serie",
        "12\240V con R1 = 2\240" O " y R2 = 4\240" O " en serie. ¿Corriente? ¿Tensión en R2?",
        "Misma corriente en las dos; las tensiones suman 12\240V.",
        s_dos, "R2", 'V', 8.0 },
    [EJ_S_TRES] = { "Tres en serie",
        "30\240V con 5, 10 y 15\240" O " en serie. ¿Qué corriente circula?",
        "Req = 5 + 10 + 15 = 30\240" O ", así que I = 1\240A.",
        s_tres, "E1", 'I', 1.0 },
    [EJ_S_DIV] = { "Divisor de tensión",
        "9\240V con 1\240k" O " y 2\240k" O " en serie. ¿Cuánta tensión hay en la de 2\240k" O "?",
        "La tensión se reparte en proporción: V2 = E·R2/(R1+R2).",
        s_div, "R2", 'V', 6.0 },
    [EJ_S_DATO] = { "El dato en una resistencia",
        "2, 3 y 5\240" O " en serie. En la de 3\240" O " se miden 6\240V. ¿Qué tensión da la fuente?",
        "Ohm en R2 da la corriente, que es la misma en todas.",
        s_dato, "E1", 'X', 20.0 },
    [EJ_S_R] = { "Hallar R en serie",
        "24\240V; en serie R1 = 4\240" O " y R2 desconocida. Circulan 2\240A. ¿R2?",
        "Req = 24/2 = 12\240" O " y R2 = Req - R1.",
        s_r, "R2", 'X', 8.0 },

    /* ---- Paralelo ---- */
    [EJ_P_DOS] = { "Dos en paralelo",
        "12\240V con 6\240" O " y 3\240" O " en paralelo. ¿Corriente total y de cada rama?",
        "Misma tensión en las dos; la de 3\240" O " lleva el doble.",
        p_dos, "E1", 'I', 6.0 },
    [EJ_P_TRES] = { "Tres distintas",
        "20\240V con 10, 20 y 20\240" O " en paralelo. ¿Qué corriente da la fuente?",
        "1/Req = 1/10 + 1/20 + 1/20 = 1/5, Req = 5\240" O ".",
        p_tres, "E1", 'I', 4.0 },
    [EJ_P_DIV] = { "Divisor de corriente",
        "Entran 3\240A a 2\240" O " y 4\240" O " en paralelo. ¿Cuánto va por cada una?",
        "La rama de menor resistencia lleva más corriente.",
        p_div, "R1", 'I', 2.0 },
    [EJ_P_R] = { "Hallar R en paralelo",
        "12\240V; en paralelo 6\240" O " y R2 desconocida. La fuente da 3\240A. ¿R2?",
        "1/R2 = 1/Req - 1/R1, con Req = 12/3 = 4\240" O ".",
        p_r, "R2", 'X', 12.0 },

    /* ---- Mixtos ---- */
    [EJ_M_SP] = { "Serie y paralelo",
        "24\240V; R1 = 4\240" O " en serie con 6\240" O " " S_PAR " 3\240" O ". ¿Corriente total? ¿Y por la de 3\240" O "?",
        "Primero el paralelo (2\240" O "), luego la serie (6\240" O ").",
        m_sp, "E1", 'I', 4.0 },
    [EJ_M_RAMA] = { "Una rama mixta",
        "10\240V con dos ramas: 5\240" O ", y 2\240" O " en serie con 6\240" O " " S_PAR " 3\240" O ". ¿Corriente total?",
        "Reducir la rama compuesta hasta un solo valor.",
        m_rama, "E1", 'I', 4.5 },
    [EJ_M_ESC] = { "Escalera",
        "18\240V; 2\240" O " en serie con (6\240" O " " S_PAR " (1\240" O " + 2\240" O ")). ¿Corriente por la de 2\240" O " del final?",
        "Se reduce desde el fondo de la escalera hacia la fuente.",
        m_esc, "R4", 'I', 3.0 },

    /* ---- Despejar ---- */
    [EJ_D_OTRA] = { "Dato en una, pregunta en otra",
        "24\240V; R1 = 4\240" O " en serie con R2 = 6\240" O " " S_PAR " R3. Por R2 pasan 2\240A. ¿R3?",
        "Se avanza paso a paso: Ohm, serie, paralelo y sumas.",
        d_otra, "R3", 'X', 12.0 },
    [EJ_D_FUENTE] = { "¿Qué fuente hace falta?",
        "10\240" O " en serie con 20\240" O " " S_PAR " 20\240" O ". Por R3 pasan 0,5\240A. ¿Qué tensión da la fuente?",
        "Del dato se sube hasta la fuente.",
        d_fuente, "E1", 'X', 20.0 },
    [EJ_D_DOS] = { "Dos incógnitas encadenadas",
        "12\240V; en serie R1 = 2\240" O " (con 4\240V) y R2. ¿Cuánto vale R2?",
        "Ohm en R1 da la corriente; la tensión de R2 es 12 - 4.",
        d_dos, "R2", 'X', 4.0 },

    /* ---- Potencia ---- */
    [EJ_W_SERIE] = { "Potencia en serie",
        "12\240V con 2\240" O " y 4\240" O " en serie. ¿Cuál disipa más? (mira la tabla)",
        "En serie disipa más la resistencia mayor: P = I²·R.",
        w_serie, "E1", 'I', 2.0 },
    [EJ_W_PAR] = { "Potencia en paralelo",
        "12\240V con 6\240" O " y 3\240" O " en paralelo. ¿Cuál disipa más? (mira la tabla)",
        "En paralelo disipa más la resistencia menor: P = V²/R.",
        w_par, "E1", 'I', 6.0 },

    /* ---- Kirchhoff ---- */
    [EJ_K_PUENTE] = { "Puente de Wheatstone",
        "12\240V; puente con R1 = 2, R2 = 4, R3 = 4, R4 = 2 y R5 = 3\240" O " en medio. ¿Corriente por R5?",
        "No es serie ni paralelo: se resuelve con las leyes de Kirchhoff.",
        k_puente, "R5", 'I', 12.0 / 17.0 },
    [EJ_K_EQUIL] = { "Puente equilibrado",
        "12\240V; R1 = 2, R2 = 3, R3 = 4, R4 = 6 y R5 = 5\240" O ". ¿Corriente por R5?",
        "Como R1/R3 = R2/R4, por la del medio no pasa corriente.",
        k_equil, "R5", 'I', 0.0 },
    [EJ_K_DOS] = { "Dos fuentes",
        "12\240V con 2\240" O " a la izquierda, 6\240V con 2\240" O " a la derecha y 4\240" O " en medio. ¿Corriente por la del medio?",
        "Con dos fuentes, una puede estar cargándose.",
        k_dos, "R2", 'I', 1.8 },

    /* ---- Fuentes de corriente, instrumentos, fuente real ---- */
    [EJ_J_SOLA] = { "Una fuente de corriente",
        "J = 3\240A alimenta 6\240" O " " S_PAR " 3\240" O ". ¿Qué tensión tiene la fuente?",
        "Con una J se conoce la corriente: V = J·Req.",
        j_sola, "J1", 'V', 6.0 },
    [EJ_J_SERIE] = { "La J manda en su rama",
        "J = 2\240A con 5\240" O " y 10\240" O " en serie. ¿Tensión en la de 10\240" O "?",
        "Todo lo que está en serie con J lleva su corriente.",
        j_serie, "R2", 'V', 20.0 },
    [EJ_J_HALLAR] = { "¿Qué J hace falta?",
        "Una J alimenta 4\240" O " " S_PAR " 12\240" O ". La de 4\240" O " tiene 6\240V. ¿Cuánto vale J?",
        "La J es la suma de las corrientes que reparte.",
        j_hallar, "J1", 'X', 2.0 },
    [EJ_I_VOLT] = { "Voltímetro no ideal",
        "12\240V sobre 10\240k" O " + 10\240k" O ". Un voltímetro de 10\240k" O " mide la segunda. ¿Qué marca?",
        "Sin él habría 6\240V; en paralelo con la R, la baja.",
        i_volt, "V1", 'V', 4.0 },
    [EJ_I_AMP] = { "Un amperímetro en serie",
        "12\240V con 1\240" O " y 3\240" O " en serie; un amperímetro ideal en la vuelta. ¿Qué marca?",
        "Ideal es un cable: marca la corriente de su rama.",
        i_amp, "A1", 'I', 3.0 },
    [EJ_F_REAL] = { "Una fuente real",
        "Una pila de 12\240V con r = 0,5\240" O " alimenta 5,5\240" O ". ¿Tensión en sus bornes?",
        "La r va en serie: I = E/(r + R) y V = E - r·I.",
        f_real, "E1", 'V', 11.0 },
    [EJ_F_R] = { "Hallar la r de una pila",
        "Una pila de 9\240V da 1,5\240A a una R de 4\240" O ". ¿Su resistencia interna?",
        "Rtot = E/I = 6\240" O "; la r es lo que falta: 6 - 4.",
        f_r, "E1", 'r', 2.0 },
};
const int N_EJEMPLOS = EJ_N;

void ejemplo_cargar(int i, Circuito *c)
{
    if (i < 0 || i >= N_EJEMPLOS) return;
    circ_init(c, "");
    if (i <= EJ_PI) {
        strcpy(c->nombre, "Problema  )");
        c->nombre[9] = EJEMPLOS[i].titulo[0];
    } else {
        strncpy(c->nombre, EJEMPLOS[i].titulo, sizeof c->nombre - 1);
    }
    EJEMPLOS[i].arma(c);
}
