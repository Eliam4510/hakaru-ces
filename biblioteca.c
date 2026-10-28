/* ============================================================================
 *  biblioteca.c  -  Circuitos modelo  (ver biblioteca.h)
 *
 *  Casi todos se dibujan en la misma caja: la fuente a la izquierda, subiendo
 *  de (2,4) a (2,3); arriba lo que va en serie; a la derecha, en (5,2)-(5,3),
 *  el C o la L que se mira, y en (6,2)-(6,3) lo que va en paralelo con él.
 *  Los del AO lo ponen en (4,2)-(6,4): la - en (4,2), la + en (4,4) (o al
 *  revés, con espejo) y la salida en (6,3), con la tierra abajo. Los del BJT,
 *  en (4,2)-(5,4): la base en (4,3), el colector en (5,2) y el emisor en
 *  (5,4).
 * ========================================================================= */
#include <string.h>
#include "biblioteca.h"
#include "gfx.h"

/* Cable recto de (x0,y0) a (x1,y1) */
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

static int pon(Circuito *c, TipoEl t, int x, int y, int d, double v)
{
    int k = circ_elemento(c, t, x, y, d);
    if (k >= 0 && v != 0.0) { c->el[k].val.v = v; c->el[k].val.est = M_DATO; }
    return k;
}

static void par(Circuito *c, int k, int q, double v)
{
    if (k < 0) return;
    c->el[k].par[q - Q_P1].v = v;
    c->el[k].par[q - Q_P1].est = M_DATO;
}

static void vacio(Circuito *c, int k, int q)
{
    if (k >= 0) c->el[k].par[q - Q_P1].est = M_LIBRE;
}

static void t_fin(Circuito *c, double t) { c->t_fin.v = t; c->t_fin.est = M_DATO; }

static void sonda(Circuito *c, int canal, int tipo, int k, int sg)
{
    Sonda *s = &c->sonda[canal];
    s->tipo = (unsigned char)tipo; s->x = s->y = -1; s->el = (signed char)k; s->sg = (signed char)sg;
}

/* La tensión del punto (x,y) respecto a la tierra */
static void sonda_nodo(Circuito *c, int canal, int x, int y)
{
    Sonda *s = &c->sonda[canal];
    s->tipo = SONDA_NODO; s->x = (signed char)x; s->y = (signed char)y; s->el = -1; s->sg = 0;
}

static void tierra(Circuito *c, int x, int y) { c->tierra_x = (signed char)x; c->tierra_y = (signed char)y; }

/* La caja: la fuente E (subiendo), arriba dos huecos para elementos en
 * serie (o cables) y a la derecha el elemento que se mira. Devuelve la E. */
static int caja(Circuito *c, double E, TipoEl s1, double v1, TipoEl s2, double v2, TipoEl der, double vd)
{
    int kE = pon(c, EL_FUENTE, 2, 4, DIR_ARR, E);
    cab(c, 2, 3, 2, 2);
    if (s1 == EL_N) cab(c, 2, 2, 3, 2); else pon(c, s1, 2, 2, DIR_DER, v1);
    if (s2 == EL_N) cab(c, 3, 2, 4, 2); else pon(c, s2, 3, 2, DIR_DER, v2);
    cab(c, 4, 2, 5, 2);
    pon(c, der, 5, 2, DIR_ABA, vd);
    cab(c, 5, 3, 5, 4);
    cab(c, 5, 4, 2, 4);
    return kE;
}

/* Algo en paralelo con el de la derecha, en la columna 6 */
static int paralelo(Circuito *c, TipoEl t, double v)
{
    int k;
    cab(c, 5, 2, 6, 2);
    k = pon(c, t, 6, 2, DIR_ABA, v);
    cab(c, 6, 3, 6, 4);
    cab(c, 6, 4, 5, 4);
    return k;
}

/* --------------------------------------------------------------------------
 *  Los doce
 * ----------------------------------------------------------------------- */
static void m_rc_carga(Circuito *c)
{
    caja(c, 10.0, EL_RES, 1e3, EL_N, 0.0, EL_COND, 1e-6);
}

static void m_rc_descarga(Circuito *c)
{
    int k;
    cab(c, 2, 4, 2, 2);                      /* sin fuente: un cable */
    pon(c, EL_RES, 2, 2, DIR_DER, 10e3);
    cab(c, 3, 2, 5, 2);
    k = pon(c, EL_COND, 5, 2, DIR_ABA, 10e-6);
    par(c, k, Q_V0, 12.0);
    cab(c, 5, 3, 5, 4);
    cab(c, 5, 4, 2, 4);
}

static void m_rl(Circuito *c)
{
    caja(c, 12.0, EL_RES, 100.0, EL_N, 0.0, EL_BOB, 100e-3);
}

static void m_rlc(Circuito *c, double R)
{
    caja(c, 10.0, EL_RES, R, EL_BOB, 10e-3, EL_COND, 4e-6);
}
static void m_rlc_sub(Circuito *c)   { m_rlc(c, 28.0); }
static void m_rlc_crit(Circuito *c)  { m_rlc(c, 100.0); }
static void m_rlc_sobre(Circuito *c) { m_rlc(c, 260.0); }

static void m_lc(Circuito *c)
{
    int k = pon(c, EL_COND, 3, 2, DIR_ABA, 4e-6);
    par(c, k, Q_V0, 10.0);
    cab(c, 3, 2, 5, 2);
    pon(c, EL_BOB, 5, 2, DIR_ABA, 10e-3);
    cab(c, 5, 3, 3, 3);
}

/* Una cuadrada de 0 a 10 V con frecuencia f */
static int cuadrada(Circuito *c, int kE, double f)
{
    c->el[kE].variante = ONDA_CUADRADA;
    par(c, kE, Q_BASE, 0.0);
    par(c, kE, Q_FREC, f);
    return kE;
}

static void m_integrador(Circuito *c)
{
    cuadrada(c, caja(c, 10.0, EL_RES, 10e3, EL_N, 0.0, EL_COND, 1e-6), 1e3);
    t_fin(c, 100e-3);
}

static void m_derivador(Circuito *c)
{
    int kE = caja(c, 10.0, EL_COND, 100e-9, EL_N, 0.0, EL_RES, 1e3), kR = c->nel - 1;
    cuadrada(c, kE, 100.0);
    sonda(c, 0, SONDA_V, kR, 1);             /* v_R, + arriba */
    sonda(c, 1, SONDA_V, kE, -1);            /* la fuente, con su signo */
}

static void m_patada(Circuito *c)
{
    int kE = caja(c, 10.0, EL_INTER, 0.0, EL_RES, 10.0, EL_BOB, 10e-3), kS = kE + 1, kL = kE + 3;
    c->el[kS].variante = INTER_CERRADO;
    par(c, kS, Q_T1, 0.5e-3);
    vacio(c, kL, Q_I0);                      /* la de CC: 1 A */
    paralelo(c, EL_RES, 1e3);
    sonda(c, 0, SONDA_V, kL, 1);
    sonda(c, 1, SONDA_I, kL, 1);
    t_fin(c, 1e-3);
}

static void m_interruptor(Circuito *c)
{
    int kE = caja(c, 10.0, EL_INTER, 0.0, EL_RES, 1e3, EL_COND, 1e-6), kS = kE + 1;
    c->el[kS].variante = INTER_ABIERTO;
    par(c, kS, Q_T1, 1e-3);
    par(c, kS, Q_T2, 3e-3);
    paralelo(c, EL_RES, 1e3);
}

static void m_senoidal(Circuito *c)
{
    int kE = caja(c, 10.0, EL_RES, 1e3, EL_N, 0.0, EL_COND, 1e-6), kV;
    c->el[kE].variante = ONDA_SENO;
    par(c, kE, Q_FREC, 159.155);
    kV = paralelo(c, EL_VOLT, 0.0);
    c->el[kV].variante = MED_CA;
    t_fin(c, 62.83e-3);
}


/* --------------------------------------------------------------------------
 *  Los diez de la alterna (lección 11)
 * ----------------------------------------------------------------------- */
/* La fuente de la caja pasa a alterna (eficaz, 0°) a la f del circuito */
static int alterna(Circuito *c, int kE, double f)
{
    c->el[kE].variante = FORMA_FASOR;
    c->el[kE].val.ang = 0.0; c->el[kE].val.est_ang = M_DATO;
    c->frec.v = f; c->frec.est = M_DATO;
    return kE;
}

static void m_pb_rc(Circuito *c)
{
    alterna(c, caja(c, 1.0, EL_RES, 1e3, EL_N, 0.0, EL_COND, 1e-6), 1e3);
    sonda(c, 0, SONDA_V, 2, 1);                       /* V(C1) */
}

static void m_pa_rl(Circuito *c)
{
    alterna(c, caja(c, 1.0, EL_RES, 100.0, EL_N, 0.0, EL_BOB, 10e-3), 1e3);
    sonda(c, 0, SONDA_V, 2, 1);                       /* V(L1) */
}

static void m_pbanda(Circuito *c)
{
    alterna(c, caja(c, 1.0, EL_BOB, 10e-3, EL_COND, 1e-6, EL_RES, 10.0), 1e3);
    sonda(c, 0, SONDA_V, 3, 1);                       /* V(R1) */
}

static void m_resonancia(Circuito *c)
{
    alterna(c, caja(c, 1.0, EL_BOB, 10e-3, EL_COND, 1e-6, EL_RES, 10.0), 1591.55);
}

/* R, L y C en las columnas 5, 6 y 8: la L lleva dos líneas de rótulo a su
 * derecha («0,01H» e «I0=0A»), que en la 7 le tapaban la C; la R las pone a
 * su izquierda */
static void m_tanque(Circuito *c)
{
    int kJ = pon(c, EL_CORR, 2, 4, DIR_ARR, 1e-3);
    alterna(c, kJ, 1591.55);
    cab(c, 2, 3, 2, 2);
    cab(c, 2, 2, 8, 2);
    pon(c, EL_RES, 5, 2, DIR_ABA, 10e3);
    pon(c, EL_BOB, 6, 2, DIR_ABA, 10e-3);
    pon(c, EL_COND, 8, 2, DIR_ABA, 1e-6);
    cab(c, 8, 3, 8, 4);
    cab(c, 8, 4, 2, 4);
    cab(c, 5, 3, 5, 4); cab(c, 6, 3, 6, 4);
    c->sonda[0].tipo = SONDA_NODO; c->sonda[0].x = 5; c->sonda[0].y = 2; c->sonda[0].el = -1; c->sonda[0].sg = 0;
}

static void m_t1(Circuito *c)
{
    alterna(c, caja(c, 120.0, EL_RES, 10.0, EL_BOB, 50e-3, EL_COND, 100e-6), 60.0);
}

static void m_carga_rl(Circuito *c)
{
    int kE = caja(c, 220.0, EL_VATI, 0.0, EL_N, 0.0, EL_IMP, 50.0), kZ = c->nel - 1;
    alterna(c, kE, 60.0);
    c->el[kZ].val.ang = 53.13010235415598; c->el[kZ].val.est_ang = M_DATO;   /* 30 + j40 */
}

/* W1 de la columna 2 a la 3, C1 en la 4, W2 de la 6 a la 7 y Z1 en la 8:
 * con W2 justo tras C1 sus rótulos («?», «V0=0V», «fp=0,7i») se pisaban, y
 * con Z1 colgando de W2, el «fp=0,7i» (debajo de W2) le tapaba la caja */
static void m_motor(Circuito *c)
{
    int kE = pon(c, EL_FUENTE, 2, 4, DIR_ARR, 230.0), kW1, kW2, kZ, kC;
    alterna(c, kE, 50.0);
    cab(c, 2, 3, 2, 2);
    kW1 = pon(c, EL_VATI, 2, 2, DIR_DER, 0.0);
    cab(c, 3, 2, 4, 2);
    kC = pon(c, EL_COND, 4, 2, DIR_ABA, 0.0);
    cab(c, 4, 3, 4, 4);
    cab(c, 4, 2, 6, 2);
    kW2 = pon(c, EL_VATI, 6, 2, DIR_DER, 10e3);
    cab(c, 7, 2, 8, 2);
    kZ = pon(c, EL_IMP, 8, 2, DIR_ABA, 0.0);
    cab(c, 8, 3, 8, 4);
    cab(c, 8, 4, 2, 4);
    c->el[kW1].aux.v = 0.95; c->el[kW1].aux.est = M_DATO;             /* el fp que se quiere */
    c->el[kW2].aux.v = 0.7;  c->el[kW2].aux.est = M_DATO;             /* el del motor        */
    c->el[kZ].val.est = M_BUSCAR; c->el[kZ].val.est_ang = M_BUSCAR;
    c->el[kC].val.est = M_BUSCAR;
}

/* V1, por encima de R1 en la fila 0: en la 1, su «V=72V» (debajo) tapaba
 * el «30Ω» de R1 (encima) */
static void m_voltimetro_l(Circuito *c)
{
    int kE = caja(c, 120.0, EL_RES, 30.0, EL_N, 0.0, EL_BOB, 0.0), kL = c->nel - 1, kV;
    alterna(c, kE, 60.0);
    c->el[kL].val.est = M_BUSCAR;
    cab(c, 2, 2, 2, 0);
    kV = pon(c, EL_VOLT, 2, 0, DIR_DER, 0.0);
    cab(c, 3, 0, 3, 2);
    c->el[kV].variante = MED_CA;
    c->el[kV].V.v = 72.0; c->el[kV].V.est = M_DATO;
}

static void m_thevenin(Circuito *c)
{
    alterna(c, caja(c, 10.0, EL_RES, 100.0, EL_N, 0.0, EL_COND, 1e-6), 1e3);
    c->term_x[0] = 5; c->term_y[0] = 2; c->term_x[1] = 5; c->term_y[1] = 3;
}


/* --------------------------------------------------------------------------
 *  Los diez de los diodos (lección 12). Los de CC (punto de operación y
 *  barrido) van sin sondas, para que [=] en el Taller dé el punto de
 *  operación: el barrido elige solo sus canales (I y V del diodo)
 * ----------------------------------------------------------------------- */
/* La fuente k pasa a la forma f (ONDA_*) con frecuencia fr; en la cuadrada y
 * la triangular, v1 es el nivel bajo (el alto es el valor de la ficha) */
static int forma(Circuito *c, int k, int f, double fr, double v1)
{
    if (k < 0) return k;
    c->el[k].variante = (unsigned char)f;
    par(c, k, Q_FREC, fr);
    if (f == ONDA_CUADRADA || f == ONDA_TRIANGULAR) par(c, k, Q_BASE, v1);
    return k;
}

/* E 5 V, R 1 kΩ y un 1N4148 en directa (el ánodo arriba) */
static void m_diodo(Circuito *c)
{
    caja(c, 5.0, EL_RES, 1e3, EL_N, 0.0, EL_DIODO, 0.0);
    c->el[c->nel - 1].variante = DIO_1N4148;
}

/* La senoidal de la caja por un 1N4007 en serie (el ánodo a la fuente) hasta
 * lo de la derecha. Canal 1: lo de la derecha; canal 2: la fuente */
static int rectificador(Circuito *c, TipoEl der, double vd)
{
    int kE = caja(c, 10.0, EL_DIODO, 0.0, EL_N, 0.0, der, vd);
    forma(c, kE, ONDA_SENO, 60.0, 0.0);
    c->el[kE + 1].variante = DIO_1N4007;
    sonda(c, 0, SONDA_V, c->nel - 1, 1);
    sonda(c, 1, SONDA_V, kE, -1);
    return kE;
}

/* Con el paso de T/100 hay una muestra justo en el pico, en T/4 */
static void m_media_onda(Circuito *c)
{
    rectificador(c, EL_RES, 1e3);
    c->paso.v = 1.0 / 6000.0; c->paso.est = M_DATO;
}

static void m_filtro(Circuito *c)
{
    rectificador(c, EL_COND, 100e-6);
    paralelo(c, EL_RES, 1e3);
    t_fin(c, 0.1);
}

/* Toma central: E1 y E2 en contrafase, apiladas en la columna 1 con la toma
 * (la tierra) en (1,3); D1 arriba y D2 abajo llevan cada mitad a la salida */
static void m_onda_completa(Circuito *c)
{
    int k1 = pon(c, EL_FUENTE, 1, 3, DIR_ARR, 10.0), k2 = pon(c, EL_FUENTE, 1, 3, DIR_ABA, 10.0), kC;
    forma(c, k1, ONDA_SENO, 60.0, 0.0);
    forma(c, k2, ONDA_SENO, 60.0, 0.0);
    par(c, k2, Q_FASE, 180.0);
    cab(c, 1, 2, 1, 1);
    pon(c, EL_DIODO, 1, 1, DIR_DER, 0.0);
    cab(c, 1, 4, 1, 5);
    pon(c, EL_DIODO, 1, 5, DIR_DER, 0.0);
    cab(c, 2, 1, 7, 1);
    kC = pon(c, EL_COND, 4, 1, DIR_ABA, 100e-6);
    pon(c, EL_RES, 5, 1, DIR_ABA, 1e3);
    cab(c, 4, 2, 4, 3); cab(c, 5, 2, 5, 3);
    cab(c, 1, 3, 5, 3);
    cab(c, 2, 5, 7, 5);
    cab(c, 7, 5, 7, 1);
    tierra(c, 3, 3);
    sonda(c, 0, SONDA_V, kC, 1);
    sonda(c, 1, SONDA_V, k1, -1);
    t_fin(c, 0.1);
}

/* El puente en escalera tumbada: la fuente flotante en medio, de pie en la
 * columna 2 (el + arriba); D3 y D1 en la fila 1 y D4 y D2 en la fila 4, de
 * la tierra (la columna 1) a la salida (la columna 4). Los diodos van
 * tumbados y separados por un cable: sus rótulos, encima y debajo, no pisan
 * los de la fuente (a su derecha) ni los de al lado */
static void m_puente(Circuito *c)
{
    static const signed char X[4] = { 3, 3, 1, 1 }, Y[4] = { 1, 4, 1, 4 };
    int kE = pon(c, EL_FUENTE, 2, 3, DIR_ARR, 12.0), kC, i;
    forma(c, kE, ONDA_SENO, 60.0, 0.0);
    cab(c, 2, 2, 2, 1); cab(c, 2, 3, 2, 4);
    for (i = 0; i < 4; i++) {
        int k = pon(c, EL_DIODO, X[i], Y[i], DIR_DER, 0.0);
        if (k >= 0) c->el[k].variante = DIO_1N4007;
    }
    cab(c, 2, 1, 3, 1); cab(c, 2, 4, 3, 4);
    cab(c, 1, 1, 1, 5); cab(c, 4, 1, 4, 4);
    cab(c, 4, 1, 8, 1);
    kC = pon(c, EL_COND, 6, 1, DIR_ABA, 100e-6);
    pon(c, EL_RES, 8, 1, DIR_ABA, 1e3);
    cab(c, 6, 2, 6, 5); cab(c, 8, 2, 8, 5);
    cab(c, 1, 5, 8, 5);
    tierra(c, 3, 5);
    sonda(c, 0, SONDA_V, kC, 1);
    sonda(c, 1, SONDA_V, kE, -1);
    t_fin(c, 0.1);
}

/* La salida entre R1 y el diodo (con Vγ), que va a E2 (+ en el cátodo) */
static void m_recortador(Circuito *c)
{
    int kE = pon(c, EL_FUENTE, 2, 4, DIR_ARR, 10.0);
    forma(c, kE, ONDA_SENO, 1e3, 0.0);
    cab(c, 2, 3, 2, 2);
    pon(c, EL_RES, 2, 2, DIR_DER, 1e3);
    cab(c, 3, 2, 5, 2);
    pon(c, EL_DIODO, 5, 2, DIR_ABA, 0.0);
    pon(c, EL_FUENTE, 5, 4, DIR_ARR, 3.0);
    cab(c, 5, 4, 2, 4);
    tierra(c, 3, 4);
    sonda_nodo(c, 0, 5, 2);
    sonda(c, 1, SONDA_V, kE, -1);
}

/* C1 en serie y el diodo (con Vγ) de la salida a tierra, con R en paralelo */
static void m_fijador(Circuito *c)
{
    int kE = caja(c, 5.0, EL_COND, 1e-6, EL_N, 0.0, EL_DIODO, 0.0), kD = c->nel - 1;
    forma(c, kE, ONDA_SENO, 1e3, 0.0);
    paralelo(c, EL_RES, 100e3);
    sonda(c, 0, SONDA_V, kD, 1);
    sonda(c, 1, SONDA_V, kE, -1);
    t_fin(c, 20e-3);
}

static void m_zener(Circuito *c)
{
    int kZ;
    caja(c, 12.0, EL_RES, 220.0, EL_N, 0.0, EL_ZENER, 0.0);
    kZ = c->nel - 1;
    circ_voltear(c, kZ);                              /* el cátodo arriba: en inversa */
    c->el[kZ].variante = ZEN_1N4733A;
    paralelo(c, EL_RES, 1e3);
}

/* R1 con [?!] y el dato I = 20 mA en el LED: la pregunta del enunciado */
static void m_led(Circuito *c)
{
    int kD;
    caja(c, 9.0, EL_RES, 0.0, EL_N, 0.0, EL_LED, 0.0);
    kD = c->nel - 1;
    c->el[1].val.est = M_BUSCAR;
    c->el[kD].I.v = 20e-3; c->el[kD].I.est = M_DATO;
}


/* --------------------------------------------------------------------------
 *  El AO y las fuentes controladas (lección 13). Cada AO lleva su tierra:
 *  su salida se mide desde ella
 * ----------------------------------------------------------------------- */
/* El AO en (4,2)-(6,4): la - en (4,2), la + en (4,4) y la salida en (6,3);
 * con espejo, la + arriba */
static int ao(Circuito *c, int espejo)
{
    int k = circ_elemento(c, EL_AO, 4, 2, DIR_DER);
    if (k >= 0 && espejo) circ_voltear(c, k);
    return k;
}

/* El inversor: E1 (subiendo por la columna 1) y x1 hasta la -; x2 de la - a
 * la salida por arriba; la + a tierra por la fila 5. Con sondas, canal 1 la
 * salida y canal 2 la fuente. Devuelve el AO */
static int inversor(Circuito *c, TipoEl t1, double v1, TipoEl t2, double v2, int sondas)
{
    int kE = pon(c, EL_FUENTE, 1, 4, DIR_ARR, 1.0), kU;
    cab(c, 1, 3, 1, 2);
    cab(c, 1, 2, 2, 2);
    pon(c, t1, 2, 2, DIR_DER, v1);
    cab(c, 3, 2, 4, 2);
    kU = ao(c, 0);
    cab(c, 4, 2, 4, 1);
    pon(c, t2, 4, 1, DIR_DER, v2);
    cab(c, 5, 1, 7, 1);
    cab(c, 7, 1, 7, 3);
    cab(c, 6, 3, 7, 3);
    cab(c, 4, 4, 4, 5);
    cab(c, 1, 4, 1, 5);
    cab(c, 1, 5, 4, 5);
    tierra(c, 4, 5);
    if (sondas) {
        sonda_nodo(c, 0, 7, 3);
        sonda(c, 1, SONDA_V, kE, -1);
    }
    return kU;
}

static void m_ao_transferencia(Circuito *c)
{
    int kU = inversor(c, EL_RES, 1e3, EL_RES, 10e3, 0);
    if (kU < 0) return;
    c->el[kU].variante = AO_REAL;
    par(c, kU, QA_A, 1e5);
}

/* La + (arriba, con espejo) a E1; R1 de la - a tierra y R2 de la - a la
 * salida por abajo; V1 mide la salida */
static void m_no_inversor(Circuito *c)
{
    pon(c, EL_FUENTE, 1, 4, DIR_ARR, 1.0);
    pon(c, EL_RES, 4, 5, DIR_ABA, 1e3);
    pon(c, EL_RES, 4, 5, DIR_DER, 9e3);
    ao(c, 1);
    pon(c, EL_VOLT, 8, 3, DIR_ABA, 0.0);
    cab(c, 1, 3, 1, 2); cab(c, 1, 2, 4, 2);
    cab(c, 4, 4, 4, 5);
    cab(c, 5, 5, 7, 5); cab(c, 7, 5, 7, 3); cab(c, 6, 3, 8, 3);
    cab(c, 8, 4, 8, 6);
    cab(c, 1, 4, 1, 6); cab(c, 1, 6, 8, 6);
    tierra(c, 4, 6);
}

/* E1 por R1 y E2 por R2 a la -; R3 de la - a la salida; V1 la mide */
static void m_sumador(Circuito *c)
{
    pon(c, EL_FUENTE, 2, 4, DIR_ARR, 1.0);
    pon(c, EL_FUENTE, 1, 4, DIR_ARR, 2.0);
    pon(c, EL_RES, 2, 2, DIR_DER, 10e3);
    pon(c, EL_RES, 2, 1, DIR_DER, 10e3);
    pon(c, EL_RES, 4, 1, DIR_DER, 10e3);
    ao(c, 0);
    pon(c, EL_VOLT, 7, 3, DIR_ABA, 0.0);
    cab(c, 2, 3, 2, 2); cab(c, 3, 2, 4, 2);
    cab(c, 1, 3, 1, 1); cab(c, 1, 1, 2, 1);
    cab(c, 3, 1, 4, 1); cab(c, 4, 1, 4, 2);
    cab(c, 5, 1, 7, 1); cab(c, 7, 1, 7, 3); cab(c, 6, 3, 7, 3);
    cab(c, 4, 4, 4, 5);
    cab(c, 1, 4, 1, 5); cab(c, 2, 4, 2, 5); cab(c, 7, 4, 7, 5);
    cab(c, 1, 5, 7, 5);
    tierra(c, 4, 5);
}

/* Cuadrada de ±1 V a 1 kHz por R1; C1 (V0 = 0) en la realimentación */
static void m_integrador_ao(Circuito *c)
{
    inversor(c, EL_RES, 10e3, EL_COND, 100e-9, 1);
    forma(c, 0, ONDA_CUADRADA, 1e3, -1.0);
}

/* Triangular de ±1 V a 1 kHz por C1 (V0 vacía: arranca sin salto); R1 en la
 * realimentación */
static void m_derivador_ao(Circuito *c)
{
    inversor(c, EL_COND, 100e-9, EL_RES, 1e3, 1);
    vacio(c, 1, Q_V0);
    forma(c, 0, ONDA_TRIANGULAR, 1e3, -1.0);
}

/* La + (arriba, con espejo) a la senoidal E1; la - a E2 = 0,5 V, una fila
 * más abajo (en la 5 a la 4 su «0,5V» tocaba el triángulo) */
static void m_comparador(Circuito *c)
{
    int kE = pon(c, EL_FUENTE, 1, 4, DIR_ARR, 1.0);
    forma(c, kE, ONDA_SENO, 1e3, 0.0);
    pon(c, EL_FUENTE, 4, 6, DIR_ARR, 0.5);
    ao(c, 1);
    cab(c, 1, 3, 1, 2); cab(c, 1, 2, 4, 2);
    cab(c, 4, 5, 4, 4);
    cab(c, 1, 4, 1, 6); cab(c, 1, 6, 4, 6);
    cab(c, 6, 3, 7, 3);
    tierra(c, 2, 6);
    sonda_nodo(c, 0, 7, 3);
    sonda(c, 1, SONDA_V, kE, -1);
}

/* R1 de la salida a la + y R2 de la + a tierra (β = 0,5, por abajo); R3 de la
 * salida a la - (por arriba) y C1 (V0 vacía: el aviso de arranque) de la - a
 * tierra */
static void m_astable(Circuito *c)
{
    int kC;
    pon(c, EL_RES, 4, 5, DIR_DER, 10e3);
    pon(c, EL_RES, 4, 5, DIR_ABA, 10e3);
    pon(c, EL_RES, 4, 1, DIR_DER, 10e3);
    kC = pon(c, EL_COND, 2, 2, DIR_ABA, 100e-9);
    vacio(c, kC, Q_V0);
    ao(c, 0);
    cab(c, 4, 2, 4, 1); cab(c, 5, 1, 7, 1); cab(c, 7, 1, 7, 5); cab(c, 6, 3, 7, 3); cab(c, 5, 5, 7, 5);
    cab(c, 4, 4, 4, 5);
    cab(c, 4, 2, 2, 2);
    cab(c, 2, 3, 2, 6); cab(c, 2, 6, 4, 6);
    tierra(c, 3, 6);
    sonda_nodo(c, 0, 7, 3);
    sonda(c, 1, SONDA_V, kC, 1);
    c->paso.v = 10e-6; c->paso.est = M_DATO;
    t_fin(c, 10e-3);
}

/* Un amplificador de libro: la entrada (E1, R1 = Rs y R2 = Rin) y la salida
 * (E2 = 100·V(R2), R3 = Rsal y R4 = RL), con la tierra común */
static void m_controlada(Circuito *c)
{
    int kRin, kE2;
    pon(c, EL_FUENTE, 1, 4, DIR_ARR, 10e-3);
    pon(c, EL_RES, 1, 2, DIR_DER, 1e3);
    kRin = pon(c, EL_RES, 3, 2, DIR_ABA, 9e3);
    kE2 = pon(c, EL_EDEP, 5, 4, DIR_ARR, 100.0);
    if (kE2 >= 0) { c->el[kE2].variante = DEP_V; c->el[kE2].ref = (signed char)kRin; }
    pon(c, EL_RES, 5, 2, DIR_DER, 100.0);
    pon(c, EL_RES, 7, 2, DIR_ABA, 900.0);
    pon(c, EL_VOLT, 8, 2, DIR_ABA, 0.0);
    cab(c, 1, 3, 1, 2); cab(c, 2, 2, 3, 2);
    cab(c, 3, 3, 3, 4);
    cab(c, 5, 3, 5, 2);
    cab(c, 6, 2, 8, 2);
    cab(c, 7, 3, 7, 4); cab(c, 8, 3, 8, 4);
    cab(c, 1, 4, 8, 4);
}

/* --------------------------------------------------------------------------
 *  El BJT (lección 13, paso 13): Q1 con la flecha a la derecha desde su base
 *  en (4,3): el colector en (5,2) y el emisor en (5,4)
 * ----------------------------------------------------------------------- */
static int bjt(Circuito *c)
{
    return circ_elemento(c, EL_BJT, 4, 3, DIR_DER);
}

/* E1 (5 V, columna 1) por R1 = RB a la base; en el colector, D1 (el LED rojo,
 * el ánodo arriba) y R2 = 330 Ω hasta E2 (9 V, columna 8); el emisor a la
 * tierra de la fila 5. R2 va de la columna 7 a la 6: en la 6 a la 5 el
 * rótulo de dos líneas del LED (a su derecha) le tapaba media resistencia */
static void m_bjt_interruptor(Circuito *c)
{
    int kL;
    pon(c, EL_FUENTE, 1, 4, DIR_ARR, 5.0);
    pon(c, EL_RES, 3, 3, DIR_DER, 4.7e3);
    bjt(c);
    kL = pon(c, EL_LED, 5, 1, DIR_ABA, 0.0);
    if (kL >= 0) c->el[kL].variante = LED_ROJO;
    pon(c, EL_RES, 7, 1, DIR_IZQ, 330.0);
    pon(c, EL_FUENTE, 8, 4, DIR_ARR, 9.0);
    cab(c, 1, 3, 3, 3);
    cab(c, 5, 1, 6, 1); cab(c, 7, 1, 8, 1); cab(c, 8, 1, 8, 3);
    cab(c, 5, 4, 5, 5);
    cab(c, 1, 4, 1, 5); cab(c, 1, 5, 8, 5); cab(c, 8, 4, 8, 5);
    tierra(c, 3, 5);
}

/* El emisor común con divisor: E1 = VCC (columna 1, arriba por la fila 0); R1
 * y R2, el divisor de la base (columna 3); R3 = RC y el amperímetro A1 hasta
 * el colector; R4 = RE del emisor a la tierra de la fila 6; V1 de C a E por
 * la columna 8 (lejos del rótulo de Q1) */
static void m_emisor_comun(Circuito *c)
{
    pon(c, EL_FUENTE, 1, 2, DIR_ARR, 12.0);
    pon(c, EL_RES, 3, 1, DIR_ABA, 47e3);
    pon(c, EL_RES, 3, 4, DIR_ABA, 10e3);
    pon(c, EL_RES, 5, 0, DIR_ABA, 2.2e3);
    pon(c, EL_RES, 5, 5, DIR_ABA, 470.0);
    bjt(c);
    pon(c, EL_AMP, 5, 1, DIR_ABA, 0.0);
    pon(c, EL_VOLT, 8, 2, DIR_ABA, 0.0);
    cab(c, 1, 1, 1, 0); cab(c, 1, 0, 5, 0);
    cab(c, 3, 0, 3, 1);
    cab(c, 3, 2, 3, 4); cab(c, 3, 3, 4, 3);
    cab(c, 5, 4, 5, 5);
    cab(c, 5, 2, 8, 2); cab(c, 8, 3, 8, 4); cab(c, 8, 4, 5, 4);
    cab(c, 1, 2, 1, 6); cab(c, 1, 6, 5, 6); cab(c, 3, 5, 3, 6);
    tierra(c, 2, 6);
}

const CircModelo MODELOS[] = {
    { "RC: carga de un condensador",
      "E = 10 V carga a C = 1 " S_MU "F (descargado) a trav" S_ee "s de R = 1 k" S_OHM ". "
      "La constante de tiempo es " S_TAU " = RC = 1 ms.",
      "En t = " S_TAU " va por el 63 %: 6,32 V. [X] 1 [M] [enter] pone el cursor ah" S_ii ".",
      m_rc_carga, 1, 'v', 0, 1e-3, 6.321206, 1e-3 },
    { "RC: descarga",
      "C = 10 " S_MU "F cargado a 12 V se descarga por R = 10 k" S_OHM ": " S_TAU " = 0,1 s. "
      "No hay fuente: la energ" S_ii "a sale del condensador.",
      "Cada " S_TAU " se queda con el 37 %: 4,41 V en 0,1 s. La tabla (tab) da la energ" S_ii "a.",
      m_rc_descarga, 1, 'v', 0, 0.1, 4.414553, 1e-3 },
    { "RL: la corriente sube",
      "E = 12 V sobre R = 100 " S_OHM " y L = 100 mH sin corriente: " S_TAU " = L/R = 1 ms. "
      "La bobina frena los cambios de corriente.",
      "i sube hacia E/R = 120 mA: 75,9 mA en t = " S_TAU ".",
      m_rl, 1, 'v', 0, 1e-3, 0.07585447, 1e-3 },
    { "RLC subamortiguado",
      "Escal" S_oo "n de 10 V sobre R = 28 " S_OHM ", L = 10 mH y C = 4 " S_MU "F en serie: "
      S_ZETA " = 0,28. La tensi" S_oo "n de C se pasa y oscila.",
      "El primer pico de v_C llega a 14 V: [P] lo busca. En la tabla, la respuesta natural.",
      m_rlc_sub, 1, 'M', 0, 0.0, 14.0, 1e-3 },
    { "RLC cr" S_ii "tico",
      "El mismo RLC con R = 100 " S_OHM " = 2" S_SQRT "(L/C): amortiguamiento cr" S_ii "tico, "
      "lo m" S_aa "s r" S_aa "pido sin pasarse.",
      "v_C sube sin oscilar: 2,64 V en 0,2 ms. Compara con R = 28 y R = 260.",
      m_rlc_crit, 1, 'v', 0, 0.2e-3, 2.642411, 1e-3 },
    { "RLC sobreamortiguado",
      "Con R = 260 " S_OHM " hay dos constantes de tiempo reales: 1 ms y 40 " S_MU "s. "
      "La lenta manda.",
      "v_C = 6,17 V en 1 ms. La respuesta natural da s = -1000 y s = -25000.",
      m_rlc_sobre, 1, 'v', 0, 1e-3, 6.167922, 1e-3 },
    { "LC: oscilaci" S_oo "n sin p" S_ee "rdidas",
      "C = 4 " S_MU "F cargado a 10 V se descarga en L = 10 mH. Sin resistencias, "
      "la energ" S_ii "a va y viene entre C y L.",
      "f = 1/(2" S_PI S_SQRT "LC) = 795,8 Hz; i m" S_aa "x = V0" S_CDOT S_SQRT "(C/L) = 0,2 A. W no cambia.",
      m_lc, 1, 'F', 0, 0.0, 795.7747, 1e-3 },
    { "Integrador RC",
      "Cuadrada de 0 a 10 V a 1 kHz sobre R = 10 k" S_OHM " y C = 1 " S_MU "F: " S_TAU " = 10 ms, "
      "mucho mayor que el periodo.",
      "v_C se queda en la media (5 V) con un rizo triangular de 0,25 V p-p. [+] acerca.",
      m_integrador, 1, 'P', 0, 0.0, 0.2499479, 2e-3 },
    { "Derivador RC",
      "Cuadrada de 0 a 10 V a 100 Hz; C = 100 nF en serie y R = 1 k" S_OHM " a tierra: "
      S_TAU " = 0,1 ms, mucho menor que el periodo.",
      "v_R solo ve los flancos: salta a +10 y a -10 V y cae con " S_TAU ".",
      m_derivador, 1, 'v', 0, 0.1e-3, 3.678794, 1e-3 },
    { "Patada inductiva",
      "E = 10 V y R = 10 " S_OHM " alimentan L = 10 mH (con 1 k" S_OHM " en paralelo). "
      "El interruptor abre en 0,5 ms.",
      "La corriente de L no puede saltar: pasa por 1 k" S_OHM " y v_L cae a -1000 V.",
      m_patada, 1, 'm', 0, 0.0, -1000.0, 1e-3 },
    { "Interruptor que cierra y abre",
      "S cierra en 1 ms y abre en 3 ms. E = 10 V carga a C = 1 " S_MU "F por R1 = 1 k" S_OHM "; "
      "R2 = 1 k" S_OHM " en paralelo lo descarga.",
      "Cerrado: sube hacia 5 V con " S_TAU " = 0,5 ms. Abierto: baja con " S_TAU " = 1 ms.",
      m_interruptor, 1, 'v', 0, 3e-3, 4.908422, 1e-3 },
    { "RC con senoidal y voltímetro en CA",
      "e = 10 sen(1000 t) V sobre R = 1 k" S_OHM " y C = 1 " S_MU "F: " S_OMEGA "RC = 1, la "
      "frecuencia de corte. V1 mide C en CA.",
      "V1 marca 5,000 V (10/" S_SQRT "2 por 1/" S_SQRT "2). v_C va 45" S_DEG " (T/8) detr" S_aa "s de e.",
      m_senoidal, 1, 'L', "V1", 0.0, 5.0, 1e-3 },
    /* ---- la alterna (lección 11) ---- */
    { "Pasa-bajas RC",
      "E alterna de 1 V sobre R = 1 k" S_OHM " y C = 1 " S_MU "F; la salida es V(C1). "
      "Deja pasar las frecuencias bajas.",
      "fc = 1/(2" S_PI "RC) = 159,2 Hz: ah" S_ii " cae 3 dB y va 45" S_DEG " detr" S_aa "s. "
      "Arriba baja 20 dB por d" S_ee "cada.",
      m_pb_rc, 1, 'f', 0, 0.0, 159.15494309189535, 1e-6, 2 },
    { "Pasa-altas RL",
      "E alterna de 1 V sobre R = 100 " S_OHM " y L = 10 mH; la salida es V(L1). "
      "Deja pasar las frecuencias altas.",
      "fc = R/(2" S_PI "L) = 1,592 kHz: ah" S_ii " la fase es +45" S_DEG ". Abajo sube 20 dB "
      "por d" S_ee "cada.",
      m_pa_rl, 1, 'f', 0, 0.0, 1591.5494309189535, 1e-6, 2 },
    { "Pasa-banda RLC serie",
      "R = 10 " S_OHM ", L = 10 mH y C = 1 " S_MU "F en serie; la salida es V(R1). "
      "Solo pasa lo que est" S_aa " cerca de f0.",
      "f0 = 1/(2" S_PI S_SQRT "LC) = 1,592 kHz; BW = R/(2" S_PI "L) = 159 Hz; Q = f0/BW = 10.",
      m_pbanda, 1, 'Q', 0, 0.0, 10.0, 1e-6, 2 },
    { "Resonancia serie",
      "El mismo RLC con E = 1 V a 1591,55 Hz (casi f0): XL y XC se anulan y solo "
      "queda R.",
      "I = E/R = 0,1 A en fase con E; V(L1) = V(C1) = 10 V: Q veces E.",
      m_resonancia, 1, 'V', "L1", 0.0, 10.000003575385968, 1e-6, 1 },
    { "Tanque LC paralelo",
      "J alterna de 1 mA sobre 10 k" S_OHM ", 10 mH y 1 " S_MU "F en paralelo; la salida es "
      "la tensi" S_oo "n del nodo.",
      "En f0 = 1,592 kHz L y C se anulan y todo va por R: 10 V. Q = R" S_CDOT S_SQRT "(C/L) = 100.",
      m_tanque, 1, 'Q', 0, 0.0, 100.0, 1e-6, 2 },
    { "RLC serie en fasores",
      "E = 120 V (eficaz) a 60 Hz sobre R = 10 " S_OHM ", L = 50 mH y C = 100 " S_MU "F en "
      "serie. XC > XL: el circuito es capacitivo.",
      "I = 9,519" S_ANG "37,5" S_DEG " A va delante de E. [enter]: el diagrama; [S] suma "
      "VR + VL + VC = E.",
      m_t1, 1, 'I', "E1", 0.0, 9.518861634600457, 1e-9, 1 },
    { "Carga RL con vat" S_ii "metro",
      "220 V a 60 Hz sobre Z1 = 30 + j40 " S_OHM " (un motor: R y L). W1 mide la potencia "
      "que entra.",
      "|Z| = 50 " S_OHM ", I = 4,4 A; W1 marca P = R" S_CDOT "I" S_SUP2 " = 580,8 W con fp 0,6 "
      "inductivo.",
      m_carga_rl, 1, 'L', "W1", 0.0, 580.8, 1e-9, 1 },
    { "Correcci" S_oo "n del fp de un motor",
      "Un motor (Z1) toma 10 kW con fp 0,7 de 230 V a 50 Hz (W2). " S_IQ "Qu" S_ee " C en "
      "paralelo lleva el fp de la l" S_ii "nea (W1) a 0,95?",
      "La calculadora despeja Z1 y C1: C = P(tan " S_PHI "1 - tan " S_PHI "2)/(" S_OMEGA "V" S_SUP2 ") "
      "= 416 " S_MU "F.",
      m_motor, 1, 'c', "C1", 0.0, 416.10139603289948e-6, 1e-6, 1 },
    { "El volt" S_ii "metro marca 72 V: " S_IQ "L?",
      "E = 120 V a 60 Hz sobre R = 30 " S_OHM " y una L que no se conoce. V1 mide 72 V "
      "en la R.",
      "I = 72/30 = 2,4 A; |Z| = 120/2,4 = 50 " S_OHM "; XL = 40 " S_OHM "; L = 106,1 mH.",
      m_voltimetro_l, 1, 'c', "L1", 0.0, 0.10610329539459689, 1e-9, 1 },
    { "Th" S_ee "venin y m" S_aa "xima transferencia",
      "E = 10 V a 1 kHz, R1 = 100 " S_OHM " hasta A y C1 = 1 " S_MU "F entre A y B. " S_IQ "Qu" S_ee
      " carga saca m" S_aa "s potencia?",
      "[T]: Zth = 71,7 - j45,0 " S_OHM "; con ZL = Zth* la carga recibe 0,25 W = "
      "E" S_SUP2 "/(4R1).",
      m_thevenin, 1, 'T', 0, 0.0, 0.25, 1e-9, 1 },
    /* ---- los diodos (lección 12) ---- */
    { "Diodo en directa: su punto de operaci" S_oo "n",
      "E = 5 V empuja corriente por R = 1 k" S_OHM " y un diodo 1N4148 en directa. "
      "[enter] halla el punto de operaci" S_oo "n con Newton.",
      "I = 4,347 mA: el diodo se queda con 0,653 V, no con 0,7 V justos, y el "
      "resto (4,347 V) cae en R.",
      m_diodo, 1, 'I', "D1", 0.0, 4.3467715385002356e-3, 1e-9, 3 },
    { "La curva del diodo: " S_IQ "qu" S_ee " E da 1 mA?",
      "El mismo circuito con E1 barrida de 0 a 5 V: la corriente del diodo apenas "
      "se mueve hasta el codo y luego sube casi en recta.",
      "Para I = 1 mA hace falta E = 1,585 V: 0,585 V en el diodo y 1 V en R.",
      m_diodo, 1, 'x', 0, 1e-3, 1.5847396946292637, 1e-9, 4, "E1", 0.0, 5.0, 0 },
    { "Rectificador de media onda",
      "e = 10 sen(2" S_PI S_CDOT "60t) V por un diodo 1N4007 en serie con R = 1 k" S_OHM ". "
      "El diodo solo deja pasar los semiciclos positivos.",
      "El pico de v_R es 9,34 V: el diodo se queda con 0,66 V. En el semiciclo "
      "negativo, v_R = 0.",
      m_media_onda, 1, 'M', 0, 0.0, 9.3403009505190993, 1e-9 },
    { "Media onda con filtro: el rizado",
      "La media onda con C = 100 " S_MU "F en paralelo con R = 1 k" S_OHM ". Se emula "
      "0,1 s (6 periodos) desde C descargado.",
      "En el " S_uu "ltimo periodo, media 8,67 V y rizado 1,29 V p-p; Vp/(f" S_CDOT "R" S_CDOT "C) "
      "da 1,55 V: C no se descarga todo T.",
      m_filtro, 1, 'A', 0, 0.0, 8.66987970057439, 1e-3 },
    { "Onda completa con dos fuentes",
      "E1 y E2 (10 V de pico, 60 Hz) en contrafase: un transformador con toma "
      "central. Diodos con V" S_GAMMA ", C = 100 " S_MU "F y R = 1 k" S_OHM ".",
      "Cada diodo conduce en su semiciclo: rizado a 120 Hz y de la mitad. Media "
      "del " S_uu "ltimo periodo: 8,98 V.",
      m_onda_completa, 1, 'A', 0, 0.0, 8.98156622274838, 1e-4 },
    { "Puente rectificador con filtro",
      "Cuatro 1N4007 en puente con e = 12 V de pico a 60 Hz, flotante; C = 100 " S_MU "F "
      "y R = 1 k" S_OHM ". Conducen dos diodos a la vez.",
      "Media del " S_uu "ltimo periodo 10,25 V: cada semiciclo pierde dos codos "
      "(" S_APROX " 1,4 V). El rizado va a 120 Hz.",
      m_puente, 1, 'A', 0, 0.0, 10.245198195320043, 1e-3 },
    { "Recortador polarizado",
      "e = 10 sen(2" S_PI S_CDOT "1000t) V por R = 1 k" S_OHM "; en la salida, un diodo con "
      "V" S_GAMMA " = 0,7 V en serie con E2 = 3 V.",
      "La salida no pasa de 3 + 0,7 = 3,7 V: ah" S_ii " el diodo conduce y recorta. "
      "Por debajo, sigue a e.",
      m_recortador, 1, 'M', 0, 0.0, 3.7000062999936993, 1e-9 },
    { "Fijador (sujetador) negativo",
      "e = 5 sen(2" S_PI S_CDOT "1000t) V por C = 1 " S_MU "F; en la salida, un diodo con "
      "V" S_GAMMA " = 0,7 V a tierra y R = 100 k" S_OHM ". 20 ms.",
      "C se carga a 4,3 V y baja la onda: va de +0,7 a -9,3 V. M" S_ii "nimo del " S_uu "ltimo "
      "periodo: -9,28 V.",
      m_fijador, 1, 'u', 0, 0.0, -9.278550321932634, 1e-3 },
    { "Regulador zener",
      "E = 12 V por Rs = R1 = 220 " S_OHM " a un zener 1N4733A (5,1 V) en inversa, con "
      "la carga RL = R2 = 1 k" S_OHM " en paralelo.",
      "La carga ve 4,94 V. R1 lleva 32,1 mA: 4,9 van a la carga y el resto, 27,1 mA, "
      "al zener.",
      m_zener, 1, 'I', "D1", 0.0, 27.133233607210867e-3, 1e-9, 3 },
    { "LED: " S_IQ "qu" S_ee " R para 20 mA?",
      "Un LED rojo (Vf = 1,8 V a 20 mA) con E = 9 V. Se barre su resistencia de 10 " S_OHM
      " a 10 k" S_OHM ".",
      "El cursor ya est" S_aa " en I = 20 mA: R = 360 " S_OHM " = (9\240-\2401,8)/0,02. [tab]: el LED, "
      "a tope.",
      m_led, 1, 'x', 0, 20e-3, 360.0, 1e-9, 4, "R1", 10.0, 10e3, 1 },
    /* ---- el AO y las fuentes controladas (lección 13) ---- */
    { "AO inversor: la transferencia",
      "Inversor con un AO real (A = 100 000), R1 = 1 k" S_OHM " y Rf = R2 = 10 k" S_OHM ". "
      "Se barre E1 de -2 a 2 V.",
      "Una recta de pendiente -R2/R1 = -10 (-9,9989 con A finita) que satura en "
      S_PM "15 V.",
      m_ao_transferencia, 1, 'g', 0, 0.0, -9.9989001209866915, 1e-9, 4, "E1", -2.0, 2.0, 0 },
    { "AO no inversor",
      "AO ideal con E1 = 1 V en la entrada +; R2 = 9 k" S_OHM " de la salida a la - y "
      "R1 = 1 k" S_OHM " de la - a tierra.",
      "Ganancia 1 + R2/R1 = 10: V1 marca 10 V. La - copia a la + (1 V): cortocircuito "
      "virtual.",
      m_no_inversor, 1, 'L', "V1", 0.0, 10.0, 1e-12, 3 },
    { "Sumador inversor",
      "AO ideal: E1 = 1 V y E2 = 2 V entran a la - por R1 y R2; R3 va de la salida a "
      "la -. Las tres, de 10 k" S_OHM ".",
      "La - es masa virtual: las corrientes de R1 y R2 se suman en R3. V1 marca "
      "-(1 + 2) = -3 V.",
      m_sumador, 1, 'L', "V1", 0.0, -3.0, 1e-12, 3 },
    { "Integrador con AO",
      "Cuadrada de " S_PM "1 V a 1 kHz por R1 = 10 k" S_OHM " a la - de un AO ideal; "
      "C1 = 100 nF (V0 = 0) de la salida a la -.",
      "La salida es una triangular de 0,5 V p-p: rampas de -e/(RC) = " S_PM "1 V/ms "
      "durante medio periodo.",
      m_integrador_ao, 1, 'P', 0, 0.0, 0.5, 1e-9 },
    { "Derivador con AO",
      "Triangular de " S_PM "1 V a 1 kHz por C1 = 100 nF a la - de un AO ideal; "
      "R1 = 1 k" S_OHM " de la salida a la -.",
      "Vsal = -RC" S_CDOT "de/dt: la pendiente es " S_PM "4 V/ms, as" S_ii " que sale una "
      "cuadrada de " S_PM "0,4 V.",
      m_derivador_ao, 1, 'U', 0, 0.0, 0.4, 1e-9 },
    { "Comparador",
      "AO ideal sin realimentaci" S_oo "n: e = sen(2" S_PI S_CDOT "1000t) V en la + y "
      "E2 = 0,5 V en la -. Satura a " S_PM "15 V.",
      "+15 V solo mientras e > 0,5 V: un tercio del periodo. La media es -5 V.",
      m_comparador, 1, 'A', 0, 0.0, -5.0, 1e-9 },
    { "Astable con AO",
      "AO ideal (" S_PM "15 V); R1 y R2 de 10 k" S_OHM " a la + (" S_BETA " = 0,5); "
      "R3 = 10 k" S_OHM " y C1 = 100 nF a la -. Paso de 10 " S_MU "s.",
      "C1 va y viene entre " S_PM "7,5 V; la salida es una cuadrada de "
      "f = 1/(2RC" S_CDOT "ln 3) = 455 Hz.",
      m_astable, 1, 'F', 0, 0.0, 455.1196133134187, 1e-4 },
    { "Amplificador con fuente controlada",
      "E1 = 10 mV con R1 = 1 k" S_OHM " sobre la entrada R2 = 9 k" S_OHM "; "
      "E2 = 100" S_CDOT "V(R2) con R3 = 100 " S_OHM " sobre R4 = 900 " S_OHM ".",
      "V(R2) = 9 mV, E2 = 0,9 V y V1 = 0,9" S_CDOT "900/1000 = 0,81 V: cada R en serie "
      "se come una parte.",
      m_controlada, 1, 'L', "V1", 0.0, 0.81, 1e-12, 3 },
    /* ---- el BJT (lección 13, paso 13) ---- */
    { "El transistor como interruptor",
      "Un NPN de interruptor: E1 = 5\240V por R1 = 4,7\240k" S_OHM " a la base; en el colector, "
      "el LED rojo D1 y R2\240=\240330\240" S_OHM " hasta E2 = 9\240V.",
      "IB = 0,89 mA sobra: el NPN satura (VCE = 0,09 V) y el LED lleva 21,5 mA. IC/IB = 24, "
      "no " S_BETA " = 100.",
      m_bjt_interruptor, 1, 'I', "D1", 0.0, 21.506917662571294e-3, 1e-9, 3 },
    { "Emisor com" S_uu "n: el punto Q",
      "NPN (" S_BETA " 100) con VCC = E1 = 12 V; divisor R1 = 47 k" S_OHM " y R2 = 10 k" S_OHM "; "
      "RC = R3 = 2,2 k" S_OHM " y RE = R4 = 470 " S_OHM ". A1 mide IC; V1, VCE.",
      "IC = 2,454 mA y VCE = 5,44 V: activa. Q, en la recta de carga VCE " S_APROX " 12 - "
      "IC" S_CDOT "(RC + RE).",
      m_emisor_comun, 1, 'L', "A1", 0.0, 2.4541907177293845e-3, 1e-9, 3 },
};

const int N_MODELOS = (int)(sizeof MODELOS / sizeof MODELOS[0]);

void modelo_cargar(int i, Circuito *c)
{
    if (i < 0 || i >= N_MODELOS) i = 0;
    circ_init(c, MODELOS[i].titulo);
    MODELOS[i].arma(c);
}
