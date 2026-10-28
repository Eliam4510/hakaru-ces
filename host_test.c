/* ============================================================================
 *  host_test.c  -  Arnés de Hakaru CES en el PC (solo PC, no entra al .tns)
 *
 *  Principio heredado de EstaticaCX: no se compara contra números copiados a
 *  mano sin más, sino contra propiedades que el resultado correcto cumple y
 *  que no aparecen en el camino que lo calculó:
 *
 *    1. Pizarrón: los nueve problemas del PDF, con su respuesta.
 *    2. Ejemplos: los 33 de "Aprende", cada uno con su clave.
 *    3. Residuo: ley de nodos, Ohm, fuentes y datos con los valores finales,
 *       sin mirar el motor (en todo lo que se resuelve).
 *    4. Dos caminos: el procedimiento calcula por el árbol serie-paralelo y
 *       el motor por Kirchhoff; tienen que coincidir en cada elemento.
 *    5. Inverso por construcción: redes aleatorias resueltas en directo; se
 *       oculta una R (o la E) y se da una medición; motor y árbol tienen que
 *       recuperar el valor oculto, que se conoce por construcción.
 *    6. Diagnósticos plantados.
 *    7. Teoría: cada página cabe en su pantalla.
 *    8. Mis problemas: guardar y abrir da el mismo circuito; un archivo roto
 *       no se abre.
 *    9. Interfaz: guion de teclas por las tres partes -> BMP de cada
 *       pantalla, y CERO textos que se salgan.
 * ========================================================================= */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <sys/stat.h>
#include "gfx.h"
#include "lu.h"
#include "cx.h"
#include "luc.h"
#include "ca.h"
#include "bode.h"
#include "trazador.h"
#include "calca.h"
#include "fasorial.h"
#include "font.h"
#include "ondas.h"
#include "sim.h"
#include "osc.h"
#include "biblioteca.h"
#include "barrido.h"

#define PI_T 3.14159265358979323846
#include "circuito.h"
#include "red.h"
#include "resuelve.h"
#include "arbol.h"
#include "proced.h"
#include "ejemplos.h"
#include "lecciones.h"
#include "archivo.h"
#include "esquema.h"
#if ESPECIAL
#include "especial.h"               /* la versión especial (no se publica) */
#include "test_especial.h"
#else
/* La versión pública: los ganchos de la especial en el arnés, vacíos */
#define ESP_T_LIMPIA(ruta)
#define ESP_T_SOLO
#define ESP_T_PRUEBAS_A
#define ESP_T_PRUEBAS_B
#define ESP_T_REV
#define ESP_T_FIJOS
#define ESP_T_MONO(i, n)
#define ESP_TB_ARCH
#define ESP_TB_MARCA
#endif
#include "banco.h"
#include "algebra.h"
#include "numfmt.h"
#include "ui.h"
#include "entrada.h"                /* el mono: los ajustes de Controles */
#include "capmotor.h"               /* el mono: las opciones de los capacitores equivalentes */
#include "kanji_img.h"

static const char *circuito_sano(const Circuito *c);

static int fallos = 0, pruebas = 0;
static const char *dir_out = "out";

static void falla(const char *que, const char *detalle, double a, double b)
{
    fallos++;
    if (fallos <= 40)
        printf("  FALLO  %s: %s  (obtenido %.12g, esperado %.12g)\n", que, detalle, a, b);
}

static int cerca(double a, double b, double tol)
{
    return fabs(a - b) <= tol * (1.0 + fabs(b));
}

static void chk(const char *que, const char *det, double a, double b)
{
    pruebas++;
    if (!cerca(a, b, 1e-9)) falla(que, det, a, b);
}

static void chk_cond(const char *que, const char *det, int cond)
{
    pruebas++;
    if (!cond) falla(que, det, 0, 1);
}

/* --------------------------------------------------------------------------
 *  Comprobaciones comunes a cualquier circuito resuelto
 * ----------------------------------------------------------------------- */
static Pasos    pasos;
static ValArbol va;

/* Líneas que ocupa un texto con el corte de ui.c (52 columnas, \n) */
static int lineas(const char *s, int maxch)
{
    int nl = 0;
    while (s && *s) {
        int n = 0, corte = 0;
        while (s[n] && s[n] != '\n' && n < maxch) { if (s[n] == ' ') corte = n; n++; }
        if (s[n] && s[n] != '\n' && corte > 0) n = corte;
        nl++;
        s += n;
        if (*s == '\n') s++;
        else while (*s == ' ') s++;
    }
    return nl;
}

/* La pantalla de pasos da 6 líneas al detalle y 8 entre detalle y
 * resultado; el título va en una línea con el contador "12/13". */
static void chk_pasos(const char *que)
{
    int i;
    chk_cond(que, "el procedimiento no se trunca", !pasos.desborde);
    for (i = 0; i < pasos.n; i++) {
        const Paso *s = &pasos.items[i];
        char det[80];
        int ld = lineas(s->detalle, 52), lr = lineas(s->resultado, 52);
        sprintf(det, "paso %d: textos caben en su buffer", i + 1);
        chk_cond(que, det, strlen(s->titulo) < PASO_TIT - 1 &&
                           strlen(s->detalle) < PASO_TXT - 1 &&
                           strlen(s->resultado) < PASO_TXT - 1);
        sprintf(det, "paso %d: título en una línea (%d)", i + 1, (int)strlen(s->titulo));
        chk_cond(que, det, gfx_text_w(s->titulo, 1) + 6 + 42 <= SCR_W);
        sprintf(det, "paso %d: detalle en 6 líneas (%d)", i + 1, ld);
        chk_cond(que, det, ld <= 6);
        if (ld > 6 && getenv("HAKARU_DEPURA"))
            printf("    [%s]\n%s\n", s->titulo, s->detalle);
        sprintf(det, "paso %d: detalle + resultado en 8 (%d)", i + 1, ld + lr);
        chk_cond(que, det, ld + lr <= 8);
    }
}

/* Resuelve, exige residuo cero, genera el procedimiento y, si fue por el
 * árbol, exige que el árbol y el motor digan lo mismo. Devuelve el método. */
static int comprueba(const char *que, const Circuito *c, const Red *r, Solucion *s)
{
    int k, met;
    resolver(c, r, s);
    pruebas++;
    if (s->estado != SOL_OK) {
        char m[200];
        sol_mensaje(c, r, s, m, sizeof m);
        falla(que, m, s->estado, SOL_OK);
        return PROC_NADA;
    }
    chk(que, "residuo de Kirchhoff/Ohm/datos", sol_residuo(c, r, &s->s[0]), 0.0);

    met = proced_generar(c, r, s, 0, &pasos, &va);
    chk_pasos(que);
    if (met == PROC_ARBOL) {
        const Valores *v = &s->s[0];
        chk_cond(que, "el árbol calculó el circuito", va.ok);
        for (k = 0; k < c->nel; k++) {
            char d[64];
            if (!(va.tiene & (1u << k))) continue;
            sprintf(d, "%s: |V| árbol = motor", c->el[k].etiq); chk(que, d, va.V[k], fabs(v->V[k]));
            sprintf(d, "%s: |I| árbol = motor", c->el[k].etiq); chk(que, d, va.I[k], fabs(v->I[k]));
            if (c->el[k].tipo == EL_RES) {
                sprintf(d, "%s: R árbol = motor", c->el[k].etiq); chk(que, d, va.R[k], v->val[k]);
            }
        }
        if (s->hay_req) chk(que, "Req árbol = motor", va.req, fabs(s->req));
    }
    return met;
}

static int busca_el(const Circuito *c, const char *etiq)
{
    int k;
    for (k = 0; k < c->nel; k++) if (strcmp(c->el[k].etiq, etiq) == 0) return k;
    return -1;
}

/* --------------------------------------------------------------------------
 *  1. Los problemas del pizarrón
 * ----------------------------------------------------------------------- */
static void test_pizarron(void)
{
    static Circuito c;
    static Red r;
    static Solucion s;
    int i, k;
    char q[40];

    printf("[1] Pizarrón: los nueve problemas del PDF\n");
    for (i = EJ_PA; i <= EJ_PI; i++) {
        const Valores *v;
        int met;
        ejemplo_cargar(i, &c);
        red_extraer(&c, &r);
        sprintf(q, "pizarrón %c)", 'a' + i);
        met = comprueba(q, &c, &r, &s);
        if (s.estado != SOL_OK) continue;
        chk_cond(q, "solución única", s.nsol == 1);
        chk_cond(q, "procedimiento por el árbol serie-paralelo", met == PROC_ARBOL);
        v = &s.s[0];

        switch (i) {
        case EJ_PA:
            chk(q, "R = 440/7", v->val[1], 440.0 / 7.0);
            chk(q, "P = 3080 W", v->P[0], 3080.0);
            break;
        case EJ_PB:
            chk(q, "V = 384 V", v->val[0], 384.0);
            chk(q, "P = 12288 W", v->P[0], 12288.0);
            break;
        case EJ_PC:
            chk(q, "V = 810 V", v->val[0], 810.0);
            chk(q, "P = 12150 W", v->P[0], 12150.0);
            break;
        case EJ_PD:
            chk(q, "V = 45 V", v->val[5], 45.0);
            chk(q, "Req = 15", s.req, 15.0);
            for (k = 0; k < 5; k++) chk(q, "9 V en cada una", fabs(v->V[k]), 9.0);
            chk(q, "P = 135 W", v->P[5], 135.0);
            break;
        case EJ_PE:
            chk(q, "Req = 0,75", s.req, 0.75);
            chk(q, "I = 440/3 A", v->I[2], 440.0 / 3.0);
            for (k = 0; k < 5; k++) if (k != 2) chk(q, "110/3 A por rama", fabs(v->I[k]), 110.0 / 3.0);
            chk(q, "P = 16133,3 W", v->P[2], 110.0 * 440.0 / 3.0);
            break;
        case EJ_PF:
            chk(q, "Req = 2,25", s.req, 2.25);
            chk(q, "V = 15,75", v->val[3], 15.75);
            chk(q, "I izq = 1,75", fabs(v->I[0]), 1.75);
            chk(q, "I izq = 1,75", fabs(v->I[1]), 1.75);
            chk(q, "I izq = 1,75", fabs(v->I[2]), 1.75);
            chk(q, "I der = 5,25", fabs(v->I[4]), 5.25);
            chk(q, "P = 110,25", v->P[3], 110.25);
            break;
        case EJ_PG:
            chk(q, "Req = 1,5", s.req, 1.5);
            chk(q, "I = 220/3", v->I[4], 220.0 / 3.0);
            chk(q, "rama 6 ohm: 55/3", fabs(v->I[0]), 55.0 / 3.0);
            chk(q, "rama 3+1+2: 55/3", fabs(v->I[2]), 55.0 / 3.0);
            chk(q, "rama 1+2: 110/3", fabs(v->I[6]), 110.0 / 3.0);
            chk(q, "P = 8066,7", v->P[4], 110.0 * 220.0 / 3.0);
            break;
        case EJ_PH:
            chk(q, "R = 4", v->val[3], 4.0);
            chk(q, "Req = 1,25", s.req, 1.25);
            chk(q, "0,5 A por 4 ohm", fabs(v->I[2]), 0.5);
            chk(q, "0,5 A por R", fabs(v->I[3]), 0.5);
            chk(q, "rama izq 1 A", fabs(v->I[0]), 1.0);
            chk(q, "rama 5 ohm 1 A", fabs(v->I[4]), 1.0);
            chk(q, "rama der 2 A", fabs(v->I[8]), 2.0);
            chk(q, "P = 20", v->P[5], 20.0);
            break;
        case EJ_PI:
            chk(q, "R = 4", v->val[3], 4.0);
            chk(q, "Req = 1,5", s.req, 1.5);
            chk(q, "rama 9 ohm: 1/3 A", fabs(v->I[5]), 1.0 / 3.0);
            chk(q, "rama 6 ohm: 0,5 A", fabs(v->I[10]), 0.5);
            chk(q, "rama 6+3: 1/3 A", fabs(v->I[0]), 1.0 / 3.0);
            chk(q, "P = 6", v->P[6], 6.0);
            break;
        }
    }
}

/* --------------------------------------------------------------------------
 *  2. Todos los ejemplos de "Aprende", con su clave
 * ----------------------------------------------------------------------- */
/* Los que no se explican por el árbol: puentes, dos fuentes, instrumentos */
static int espera_kirchhoff(int i)
{
    return i == EJ_K_PUENTE || i == EJ_K_EQUIL || i == EJ_K_DOS || i == EJ_I_VOLT || i == EJ_I_AMP;
}

static void test_ejemplos(void)
{
    static Circuito c;
    static Red r;
    static Solucion s;
    int i, n_arbol = 0, n_kir = 0;

    printf("[2] Ejemplos resueltos: %d, cada uno con su clave\n", N_EJEMPLOS);
    for (i = 0; i < N_EJEMPLOS; i++) {
        const Ejemplo *e = &EJEMPLOS[i];
        char q[64];
        int met, k, pide = 0, j;
        double got;

        sprintf(q, "ejemplo %d (%s)", i, e->titulo);
        chk_cond(q, "título cabe en la lista y en la barra", gfx_text_w(e->titulo, 1) + 16 <= SCR_W - 60);
        chk_cond(q, "enunciado en 3 líneas", lineas(e->enunciado, 52) <= 3);
        chk_cond(q, "nota en 2 líneas", lineas(e->nota, 52) <= 2);

        ejemplo_cargar(i, &c);
        red_extraer(&c, &r);
        for (j = 0; j < c.nel; j++)
            if (c.el[j].val.est == M_BUSCAR || c.el[j].V.est == M_BUSCAR || c.el[j].I.est == M_BUSCAR ||
                c.el[j].aux.est == M_BUSCAR) pide = 1;
        chk_cond(q, "pregunta algo (hay un ?)", pide);

        met = comprueba(q, &c, &r, &s);
        if (s.estado != SOL_OK) continue;
        chk_cond(q, "solución única", s.nsol == 1);
        if (espera_kirchhoff(i)) { chk_cond(q, "no reducible: Kirchhoff", met == PROC_KIRCHHOFF); n_kir++; }
        else { chk_cond(q, "procedimiento de pizarra (árbol)", met == PROC_ARBOL); n_arbol++; }

        k = busca_el(&c, e->clave_el);
        chk_cond(q, "el elemento de la clave existe", k >= 0);
        if (k < 0) continue;
        got = e->clave_q == 'X' ? s.s[0].val[k] :
              e->clave_q == 'r' ? s.s[0].aux[k] :
              e->clave_q == 'V' ? fabs(s.s[0].V[k]) : fabs(s.s[0].I[k]);
        chk(q, "la respuesta es la de la clave", got, e->clave);
        {
            const Mag *m = e->clave_q == 'X' ? &c.el[k].val : e->clave_q == 'r' ? &c.el[k].aux :
                           e->clave_q == 'V' ? &c.el[k].V : &c.el[k].I;
            chk_cond(q, "la clave es algo que el ejemplo pregunta", m->est == M_BUSCAR);
        }
    }
    printf("    %d por el árbol, %d por Kirchhoff\n", n_arbol, n_kir);
}

/* --------------------------------------------------------------------------
 *  5. Redes aleatorias
 * ----------------------------------------------------------------------- */
static unsigned semilla = 12345u;
static unsigned azar(unsigned n) { semilla = semilla * 1103515245u + 12345u; return (semilla >> 16) % n; }
static double azar_R(void) { return 0.5 * (double)(1 + azar(200)); }     /* 0,5 .. 100 */

static Circuito rc;
static Red      rr;
static int      nodos;

static int nuevo_el(TipoEl t, int a, int b)
{
    Elemento *e;
    int k = rc.nel;
    if (k >= MAX_EL) return -1;
    e = &rc.el[k];
    memset(e, 0, sizeof *e);
    e->ref = -1;
    e->tipo = (unsigned char)t;
    e->xa = e->xb = (unsigned char)(k % REJ_COLS);
    e->ya = e->yb = (unsigned char)(k / REJ_COLS);
    if (t == EL_RES) { e->val.v = azar_R(); e->val.est = M_DATO; }
    sprintf(e->etiq, "%c%d", tipo_info(t)->letra, k + 1);
    rr.a[k] = a; rr.b[k] = b;
    rc.nel++;
    return k;
}

static void gen(int u, int v, int prof)
{
    int op = (prof >= 3 || rc.nel >= 18) ? 0 : (int)azar(3);
    if (op == 0) nuevo_el(EL_RES, u, v);
    else if (op == 1) { int w = nodos++; gen(u, w, prof + 1); gen(w, v, prof + 1); }
    else { int n = 2 + (int)azar(2), i; for (i = 0; i < n; i++) gen(u, v, prof + 1); }
}

static void cierra_red(void)
{
    rr.nn = nodos;
    rr.nel = rc.nel;
    rr.error = RED_OK;
    rr.elem_error = -1;
    red_cierra(&rc, &rr);
}

static int fuente_sp(double E)
{
    int f;
    circ_init(&rc, "azar");
    nodos = 2;
    f = nuevo_el(EL_FUENTE, 0, 1);
    rc.el[f].val.v = E; rc.el[f].val.est = M_DATO;
    gen(1, 0, 0);
    cierra_red();
    return f;
}

static void test_azar(void)
{
    static Solucion s;
    static Valores directo;
    int caso, k, j, f, met, n_prop = 0, n_prop_ok = 0, n_kirch = 0;
    char q[48];

    printf("[5] Redes aleatorias: directo, inverso por construcción, puentes\n");
    for (caso = 0; caso < 400; caso++) {
        double E = 1.0 + azar(240);
        f = fuente_sp(E);
        sprintf(q, "azar %d directo", caso);
        met = comprueba(q, &rc, &rr, &s);
        if (s.estado != SOL_OK) continue;
        chk_cond(q, "SP -> procedimiento por el árbol", met == PROC_ARBOL);
        directo = s.s[0];

        /* E desconocida, se da la I de la fuente */
        rc.el[f].val.est = M_BUSCAR;
        rc.el[f].I.est = M_DATO; rc.el[f].I.v = fabs(directo.I[f]);
        sprintf(q, "azar %d E oculta", caso);
        comprueba(q, &rc, &rr, &s);
        if (s.estado == SOL_OK) chk(q, "recupera E", s.s[0].val[f], E);

        /* Una R oculta, con E e I de la fuente */
        rc.el[f].val.est = M_DATO;
        k = 1 + (int)azar((unsigned)(rc.nel - 1));
        {
            double Roculta = rc.el[k].val.v;
            rc.el[k].val.est = M_BUSCAR;
            sprintf(q, "azar %d %s oculta", caso, rc.el[k].etiq);
            if (fabs(directo.I[k]) < 1e-9 * fabs(directo.I[f])) {
                resolver(&rc, &rr, &s);
                chk_cond(q, "sin corriente: no determinable", s.estado != SOL_OK);
            } else if (fabs(directo.I[k]) > 1e-6 * fabs(directo.I[f])) {
                met = comprueba(q, &rc, &rr, &s);
                if (s.estado == SOL_OK) {
                    chk(q, "recupera R", s.s[0].val[k], Roculta);
                    chk_cond(q, "inverso por el árbol", met == PROC_ARBOL);
                }
            }
            rc.el[k].val.est = M_DATO;
        }

        /* E desconocida, se da la V de una resistencia (se sube por el árbol) */
        rc.el[f].I.est = M_LIBRE;
        rc.el[f].val.est = M_BUSCAR;
        k = 1 + (int)azar((unsigned)(rc.nel - 1));
        if (fabs(directo.V[k]) > 1e-6 * E) {
            rc.el[k].V.est = M_DATO; rc.el[k].V.v = fabs(directo.V[k]);
            sprintf(q, "azar %d E desde V(%s)", caso, rc.el[k].etiq);
            met = comprueba(q, &rc, &rr, &s);
            if (s.estado == SOL_OK) {
                chk(q, "recupera E", s.s[0].val[f], E);
                chk_cond(q, "por el árbol", met == PROC_ARBOL);
            }
            rc.el[k].V.est = M_LIBRE;
        }
        rc.el[f].val.est = M_DATO;

        /* Propagación: E conocida, una R oculta, y el dato en OTRA
         * resistencia (su I). Es el problema típico que crea un alumno. */
        k = 1 + (int)azar((unsigned)(rc.nel - 1));
        j = 1 + (int)azar((unsigned)(rc.nel - 1));
        if (j != k && fabs(directo.I[k]) > 1e-6 * fabs(directo.I[f]) &&
            fabs(directo.I[j]) > 1e-6 * fabs(directo.I[f])) {
            double Roculta = rc.el[k].val.v;
            rc.el[k].val.est = M_BUSCAR;
            rc.el[j].I.est = M_DATO; rc.el[j].I.v = fabs(directo.I[j]);
            resolver(&rc, &rr, &s);
            if (s.estado == SOL_OK && s.nsol == 1) {
                sprintf(q, "azar %d %s desde I(%s)", caso, rc.el[k].etiq, rc.el[j].etiq);
                met = comprueba(q, &rc, &rr, &s);
                chk(q, "recupera R (motor)", s.s[0].val[k], Roculta);
                n_prop++;
                if (met == PROC_ARBOL) n_prop_ok++;
                else if (getenv("HAKARU_DEPURA")) {
                    static Arbol ab;
                    char ex[400];
                    if (arbol_construir(&rc, &rr, &ab)) {
                        arbol_expresion(&ab, &rc, ab.raiz, ex, sizeof ex);
                        printf("    sin explicar: %s oculta, dato I(%s); Req = %s\n",
                               rc.el[k].etiq, rc.el[j].etiq, ex);
                    }
                }
            }
            rc.el[k].val.est = M_DATO;
            rc.el[j].I.est = M_LIBRE;
        }

        /* Una resistencia de más entre dos nodos cualesquiera: casi siempre
         * deja de ser serie-paralelo (puente). Solo el motor y el residuo. */
        if (nodos > 2 && rc.nel < MAX_EL - 1) {
            int a = (int)azar((unsigned)nodos), b = (int)azar((unsigned)nodos);
            if (a != b) {
                nuevo_el(EL_RES, a, b);
                cierra_red();
                sprintf(q, "azar %d con puente", caso);
                met = comprueba(q, &rc, &rr, &s);
                if (met == PROC_KIRCHHOFF) n_kirch++;
                if (rc.nel < MAX_EL) {
                    int f2 = nuevo_el(EL_FUENTE, (int)azar((unsigned)nodos), (int)azar((unsigned)nodos));
                    rc.el[f2].val.v = 1.0 + azar(50); rc.el[f2].val.est = M_DATO;
                    cierra_red();
                    sprintf(q, "azar %d con dos fuentes", caso);
                    if (rr.a[f2] == rr.b[f2]) {
                        /* fuente en corto: la rechaza el dibujo */
                    } else if ((rr.a[f2] <= 1 && rr.b[f2] <= 1) &&
                               !cerca(rc.el[f2].val.v, E, 1e-12)) {
                        resolver(&rc, &rr, &s);
                        chk_cond(q, "fuentes en paralelo distintas: incompatibles",
                                 s.estado == SOL_INCOMPAT);
                    } else {
                        comprueba(q, &rc, &rr, &s);
                    }
                }
            }
        }
    }
    printf("    propagación: %d de %d explicados por el árbol; %d puentes por Kirchhoff\n",
           n_prop_ok, n_prop, n_kirch);
    pruebas++;
    if (n_prop_ok * 10 < n_prop * 9)
        falla("azar", "la propagación explica menos del 90% de los problemas", n_prop_ok, n_prop);
}

/* --------------------------------------------------------------------------
 *  6. Diagnósticos plantados
 * ----------------------------------------------------------------------- */
static void test_diagnosticos(void)
{
    static Circuito c;
    static Red r;
    static Solucion s;
    int k;

    printf("[6] Diagnósticos\n");

    ejemplo_cargar(EJ_PA, &c);               /* a) sin la I: faltan datos */
    c.el[0].I.est = M_LIBRE;
    red_extraer(&c, &r); resolver(&c, &r, &s);
    chk_cond("diag", "faltan datos", s.estado == SOL_FALTAN);

    ejemplo_cargar(EJ_PB, &c);               /* b) con E = 100: se contradicen */
    c.el[0].val.est = M_DATO; c.el[0].val.v = 100.0;
    red_extraer(&c, &r); resolver(&c, &r, &s);
    chk_cond("diag", "datos contradictorios", s.estado == SOL_INCOMPAT);

    ejemplo_cargar(EJ_PA, &c);               /* a) con la I repetida: coherente */
    c.el[1].I.est = M_DATO; c.el[1].I.v = 7.0;
    red_extraer(&c, &r); resolver(&c, &r, &s);
    chk_cond("diag", "dato redundante coherente", s.estado == SOL_OK);

    ejemplo_cargar(EJ_PF, &c);               /* dos R en serie con su corriente */
    c.el[0].val.est = M_BUSCAR; c.el[1].val.est = M_BUSCAR;
    c.el[3].val.est = M_DATO; c.el[3].val.v = 15.75;
    c.el[2].I.est = M_DATO; c.el[2].I.v = 1.75;
    red_extraer(&c, &r); resolver(&c, &r, &s);
    chk_cond("diag", "dos R en serie con la corriente: indeterminado", s.estado == SOL_INDET);

    ejemplo_cargar(EJ_PA, &c);               /* fuente en corto: un cable la rodea */
    circ_cable(&c, 2, 1, DIR_IZQ);
    circ_cable(&c, 1, 1, DIR_ABA);
    circ_cable(&c, 1, 2, DIR_DER);
    red_extraer(&c, &r); resolver(&c, &r, &s);
    chk_cond("diag", "fuente en cortocircuito", s.estado == SOL_ERR_RED && r.error == RED_FUENTE_CORTO);

    ejemplo_cargar(EJ_PA, &c);               /* R colgante e incógnita */
    c.el[1].val.est = M_DATO; c.el[1].val.v = 10.0;
    k = circ_elemento(&c, EL_RES, 6, 3, DIR_DER);
    c.el[k].val.est = M_BUSCAR;
    c.el[k].I.est = M_DATO; c.el[k].I.v = 0.0;
    c.el[0].I.est = M_LIBRE;
    red_extraer(&c, &r); resolver(&c, &r, &s);
    chk_cond("diag", "R sin corriente no se puede hallar",
             s.estado == SOL_SIN_CORRIENTE || s.estado == SOL_INDET);

    ejemplo_cargar(EJ_PH, &c);               /* h) con I = 1 A: R saldría negativa */
    c.el[5].I.v = 1.0;
    red_extraer(&c, &r); resolver(&c, &r, &s);
    chk_cond("diag", "sin solución física", s.estado == SOL_NO_FISICA);

    /* 12 V, R1 = 4 con V(R1) = 12 V y R2 desconocida en serie: R2 = 0. Antes
     * el veredicto dependía del signo del redondeo (±1e-15 ohm). */
    circ_init(&c, "R a cero");
    k = circ_elemento(&c, EL_FUENTE, 0, 1, DIR_ARR);
    c.el[k].val.est = M_DATO; c.el[k].val.v = 12.0;
    k = circ_elemento(&c, EL_RES, 0, 0, DIR_DER);
    c.el[k].val.est = M_DATO; c.el[k].val.v = 4.0;
    c.el[k].V.est = M_DATO; c.el[k].V.v = 12.0;
    k = circ_elemento(&c, EL_RES, 1, 0, DIR_DER);
    c.el[k].val.est = M_BUSCAR;
    circ_cable(&c, 2, 0, DIR_ABA);
    circ_cable(&c, 2, 1, DIR_IZQ);
    circ_cable(&c, 1, 1, DIR_IZQ);
    red_extraer(&c, &r); resolver(&c, &r, &s);
    chk_cond("diag", "R desconocida que tendría que valer 0", s.estado == SOL_R_CERO && s.elem == k);

    circ_init(&c, "vacío");
    red_extraer(&c, &r); resolver(&c, &r, &s);
    chk_cond("diag", "circuito vacío", s.estado == SOL_ERR_RED && r.error == RED_VACIA);
}

/* --------------------------------------------------------------------------
 *  7. Teoría: cada página cabe
 * ----------------------------------------------------------------------- */
static void test_lecciones(void)
{
    int i, j;
    printf("[7] Lecciones: %d, cada página cabe en su pantalla\n", N_LECCIONES);
    for (i = 0; i < N_LECCIONES; i++) {
        const Leccion *l = &LECCIONES[i];
        char q[64];
        sprintf(q, "lección %d", i + 1);
        chk_cond(q, "título cabe en la barra", gfx_text_w(l->titulo, 1) + 10 + 30 <= SCR_W);
        chk_cond(q, "resumen en una línea", lineas(l->resumen, 52) == 1);
        chk_cond(q, "tiene ejemplos o modelos", l->nej > 0 || l->nmod > 0);
        for (j = 0; j < l->nej; j++)
            chk_cond(q, "ejemplo válido", l->ej[j] >= 0 && l->ej[j] < N_EJEMPLOS);
        for (j = 0; j < l->nmod; j++)
            chk_cond(q, "modelo válido", l->mod[j] >= 0 && l->mod[j] < N_MODELOS);
        for (j = 0; j < l->npag; j++) {
            const Pagina *p = &l->pag[j];
            int max = (p->fig != FIG_NADA) ? 9 : 15, n = lineas(p->texto, 52);
            char d[64];
            sprintf(d, "página %d: %d líneas de %d", j + 1, n, max);
            chk_cond(q, d, n <= max);
            if (p->fig == FIG_CIRCUITO)
                chk_cond(q, "figura con ejemplo válido", p->fig_ej >= 0 && p->fig_ej < N_EJEMPLOS);
        }
    }
}

/* --------------------------------------------------------------------------
 *  8. Mis problemas
 * ----------------------------------------------------------------------- */
static char carpeta_mis[200];

static void limpia_mis(void)
{
    int i;
    for (i = 0; i < ARCH_HUECOS; i++) arch_borrar(i);
}

static int misma_mag(const Mag *a, const Mag *b)
{
    return a->v == b->v && a->ang == b->ang && a->est == b->est && a->est_ang == b->est_ang;
}

static int mismos(const Circuito *a, const Circuito *b)
{
    int k, q;
    if (a->nel != b->nel || strcmp(a->nombre, b->nombre) != 0) return 0;
    if (memcmp(a->h, b->h, sizeof a->h) != 0 || memcmp(a->v, b->v, sizeof a->v) != 0) return 0;
    if (a->tierra_x != b->tierra_x || a->tierra_y != b->tierra_y || a->conv != b->conv) return 0;
    for (k = 0; k < 2; k++) if (a->term_x[k] != b->term_x[k] || a->term_y[k] != b->term_y[k]) return 0;
    if (!misma_mag(&a->frec, &b->frec)) return 0;
    for (k = 0; k < a->nel; k++) {
        const Elemento *x = &a->el[k], *y = &b->el[k];
        if (x->tipo != y->tipo || x->xa != y->xa || x->ya != y->ya || x->xb != y->xb || x->yb != y->yb) return 0;
        if (x->xc != y->xc || x->yc != y->yc) return 0;
        if (strcmp(x->etiq, y->etiq) != 0) return 0;
        for (q = 0; q < Q_TOT; q++) if (!misma_mag(circ_mag_c(x, q), circ_mag_c(y, q))) return 0;
        if (x->variante != y->variante || x->ref != y->ref) return 0;
    }
    for (k = 0; k < 2; k++) {
        const Sonda *sa = &a->sonda[k], *sb = &b->sonda[k];
        if (sa->tipo != sb->tipo || sa->x != sb->x || sa->y != sb->y || sa->el != sb->el || sa->sg != sb->sg) return 0;
    }
    if (!misma_mag(&a->t_fin, &b->t_fin) || !misma_mag(&a->paso, &b->paso)) return 0;
    return 1;
}

/* Un archivo EFK1 (el primer formato, de cuando el programa se llamaba
 * ElectroFock) escrito byte a byte: 12 V y 4 ohm con I buscada. Ya no se
 * lee: sirve para ver que se rechaza. */
static void escribe_efk1(const char *ruta)
{
    static const unsigned char CAB[4] = { 'E', 'F', 'K', '1' };
    static Circuito c;
    FILE *f = fopen(ruta, "wb");
    int x, y, k, i;
    if (!f) return;
    circ_init(&c, "viejo");
    k = circ_elemento(&c, EL_FUENTE, 0, 1, DIR_ARR);
    c.el[k].val.est = M_DATO; c.el[k].val.v = 12.0;
    k = circ_elemento(&c, EL_RES, 0, 0, DIR_DER);
    c.el[k].val.est = M_DATO; c.el[k].val.v = 4.0;
    c.el[k].I.est = M_BUSCAR;
    circ_cable(&c, 1, 0, DIR_ABA);
    circ_cable(&c, 1, 1, DIR_IZQ);
    fwrite(CAB, 1, 4, f);
    fputc(REJ_COLS, f); fputc(REJ_FILAS, f);
    for (i = 0; i < 24; i++) fputc(c.nombre[i], f);
    for (y = 0; y < REJ_FILAS; y++) for (x = 0; x < REJ_COLS - 1; x++) fputc(c.h[y][x], f);
    for (y = 0; y < REJ_FILAS - 1; y++) for (x = 0; x < REJ_COLS; x++) fputc(c.v[y][x], f);
    fputc(c.nel, f);
    for (k = 0; k < c.nel; k++) {
        const Elemento *e = &c.el[k];
        const Mag *m[3];
        int j;
        m[0] = &e->val; m[1] = &e->V; m[2] = &e->I;
        fputc(e->tipo, f); fputc(e->xa, f); fputc(e->ya, f); fputc(e->xb, f); fputc(e->yb, f);
        for (i = 0; i < 6; i++) fputc(e->etiq[i], f);
        for (j = 0; j < 3; j++) { fwrite(&m[j]->v, 8, 1, f); fputc(m[j]->est, f); }
    }
    fclose(f);
}

/* Un archivo CESP escrito byte a byte, sin pasar por archivo.c: fija el
 * formato. Con relleno, los bloques llevan bytes de más al final, como los
 * dejaría una versión futura que añada campos. */
static void w_mag(FILE *f, const Mag *m)
{
    fwrite(&m->v, 8, 1, f); fwrite(&m->ang, 8, 1, f);
    fputc(m->est, f); fputc(m->est_ang, f);
}

static void escribe_cesp(const char *ruta, const Circuito *c, int relleno_g, int relleno_e)
{
    FILE *f = fopen(ruta, "wb");
    int x, y, k, i;
    if (!f) return;
    fwrite("CESP", 1, 4, f);
    fputc(1, f);                                       /* versión */
    fputc(REJ_COLS, f); fputc(REJ_FILAS, f);
    for (i = 0; i < 24; i++) fputc(c->nombre[i], f);
    for (y = 0; y < REJ_FILAS; y++) for (x = 0; x < REJ_COLS - 1; x++) fputc(c->h[y][x], f);
    for (y = 0; y < REJ_FILAS - 1; y++) for (x = 0; x < REJ_COLS; x++) fputc(c->v[y][x], f);
    fputc(25 + relleno_g, f);                          /* bloque global */
    fputc(c->tierra_x & 0xFF, f); fputc(c->tierra_y & 0xFF, f);
    fputc(c->term_x[0] & 0xFF, f); fputc(c->term_y[0] & 0xFF, f);
    fputc(c->term_x[1] & 0xFF, f); fputc(c->term_y[1] & 0xFF, f);
    fputc(c->conv, f);
    w_mag(f, &c->frec);
    for (i = 0; i < relleno_g; i++) fputc(0xA5, f);
    fputc(c->nel, f);
    for (k = 0; k < c->nel; k++) {
        const Elemento *e = &c->el[k];
        fputc(85 + relleno_e, f);
        fputc(e->tipo, f); fputc(e->xa, f); fputc(e->ya, f); fputc(e->xb, f); fputc(e->yb, f);
        for (i = 0; i < 6; i++) fputc(e->etiq[i], f);
        w_mag(f, &e->val); w_mag(f, &e->V); w_mag(f, &e->I); w_mag(f, &e->aux);
        fputc(e->variante, f);
        fputc(e->ref & 0xFF, f);
        for (i = 0; i < relleno_e; i++) fputc(0x5A, f);
    }
    fclose(f);
}

static int lazo(Circuito *c, Red *r, double E, double R1, double R2);

static void test_archivo(void)
{
    static Circuito a, b;
    int i;
    FILE *f;
    char ruta[260];

    printf("[8] Mis problemas: guardar y abrir\n");
    limpia_mis();
    chk_cond("archivo", "carpeta vacía: primer hueco libre = 0", arch_libre() == 0);
    for (i = 0; i < N_EJEMPLOS && i < ARCH_HUECOS; i++) {
        ejemplo_cargar(i, &a);
        chk_cond("archivo", "se guarda", arch_guardar(i, &a));
        chk_cond("archivo", "existe", arch_existe(i));
        chk_cond("archivo", "se abre", arch_cargar(i, &b));
        chk_cond("archivo", "abre lo mismo que se guardó", mismos(&a, &b));
    }
    chk_cond("archivo", "con 20 guardados no queda hueco", arch_libre() == -1);

    /* CESP guarda todo el modelo, también lo que aún no usa ningún tipo */
    limpia_mis();
    ejemplo_cargar(EJ_PH, &a);
    a.tierra_x = 3; a.tierra_y = 4;
    a.term_x[0] = 1; a.term_y[0] = 2; a.term_x[1] = 5; a.term_y[1] = 6;
    a.conv = CONV_PICO | CONV_SENO;
    a.frec.v = 60.0; a.frec.est = M_DATO;
    a.el[0].val.ang = -30.5; a.el[0].val.est_ang = M_DATO;
    a.el[1].aux.v = 0.25; a.el[1].aux.est = M_DATO; a.el[1].aux.ang = 12.0; a.el[1].aux.est_ang = M_BUSCAR;
    a.el[1].variante = 3;
    a.el[1].ref = 0;
    chk_cond("archivo", "el circuito con todo es válido", circ_valido(&a));
    chk_cond("archivo", "CESP: se guarda", arch_guardar(0, &a));
    chk_cond("archivo", "CESP: se abre con todo igual", arch_cargar(0, &b) && mismos(&a, &b));

    /* Borrar el elemento al que apunta una referencia la deja en -1 */
    circ_quitar(&b, b.el[0].xa, b.el[0].ya,
                b.el[0].xb > b.el[0].xa ? DIR_DER : b.el[0].xb < b.el[0].xa ? DIR_IZQ :
                b.el[0].yb > b.el[0].ya ? DIR_ABA : DIR_ARR);
    chk_cond("archivo", "borrar el referido deja la referencia en -1", b.el[0].ref == -1 && circ_valido(&b));

    /* Borrar un elemento anterior al referido: la referencia baja uno */
    {
        static Circuito d;
        static Red rd;
        lazo(&d, &rd, 12.0, 4.0, 8.0);                 /* E1, R1, R2 */
        d.el[0].ref = 2;
        circ_quitar(&d, 0, 0, DIR_DER);                /* se va R1 */
        chk_cond("archivo", "borrar uno anterior renumera la referencia",
                 d.nel == 2 && d.el[0].ref == 1 && circ_valido(&d));
    }

    /* El formato fijado byte a byte: lo que escribe arch_guardar es lo que
     * escribe a mano el arnés, y un archivo con bloques más largos (de una
     * versión futura) se abre igual */
    {
        long n1, n2;
        unsigned char *b1, *b2;
        FILE *f1, *f2;
        char ruta2[260];
        chk_cond("archivo", "CESP: se guarda (para comparar)", arch_guardar(0, &a));
        sprintf(ruta, "%s/ces_01.tns", carpeta_mis);
        sprintf(ruta2, "%s/ces_03.tns", carpeta_mis);
        escribe_cesp(ruta2, &a, 0, 0);
        f1 = fopen(ruta, "rb"); f2 = fopen(ruta2, "rb");
        b1 = (unsigned char *)malloc(4096); b2 = (unsigned char *)malloc(4096);
        n1 = f1 && b1 ? (long)fread(b1, 1, 4096, f1) : -1;
        n2 = f2 && b2 ? (long)fread(b2, 1, 4096, f2) : -2;
        if (f1) fclose(f1);
        if (f2) fclose(f2);
        chk_cond("archivo", "CESP: el archivo es el del formato, byte a byte",
                 n1 > 0 && n1 == n2 && memcmp(b1, b2, (size_t)n1) == 0);
        free(b1); free(b2);
        escribe_cesp(ruta2, &a, 3, 2);
        chk_cond("archivo", "CESP: con bloques más largos se abre igual", arch_cargar(2, &b) && mismos(&a, &b));
    }

    /* Valores que la ficha no deja escribir tampoco entran por un archivo */
    {
        static Circuito d;
        static Red rd;
        double inf = 1e308;
        inf *= 10.0;
        sprintf(ruta, "%s/ces_03.tns", carpeta_mis);
        lazo(&d, &rd, 12.0, 4.0, 8.0);
        d.el[0].val.v = inf;
        escribe_cesp(ruta, &d, 0, 0);
        chk_cond("archivo", "una E infinita no se abre", !arch_cargar(2, &b));
        lazo(&d, &rd, 12.0, 4.0, 8.0);
        d.el[1].val.ang = inf;                         /* aunque no sea un dato */
        chk_cond("archivo", "circ_valido: un ángulo infinito no es válido", !circ_valido(&d));
        d.el[1].val.ang = 0.0;
        d.el[1].V.v = -inf;
        chk_cond("archivo", "circ_valido: un -Inf en un campo vacío tampoco", !circ_valido(&d));
        lazo(&d, &rd, 12.0, 4.0, 8.0);
        d.el[0].aux.est = M_DATO; d.el[0].aux.v = -2.0;
        escribe_cesp(ruta, &d, 0, 0);
        chk_cond("archivo", "una r negativa no se abre", !arch_cargar(2, &b));
        lazo(&d, &rd, 12.0, 4.0, 8.0);
        d.el[1].val.v = 1e-9;
        escribe_cesp(ruta, &d, 0, 0);
        chk_cond("archivo", "una R por debajo del mínimo no se abre", !arch_cargar(2, &b));
        arch_borrar(2);
    }

    /* Los formatos de antes del nombre nuevo ya no se abren: un EFK1, y un
     * EFK3, que por dentro era lo mismo que un CESP (solo cambia la marca).
     * Pero el hueco sigue ocupado, para que Mis problemas lo enseñe y se
     * pueda borrar (ver [19]) */
    sprintf(ruta, "%s/ces_02.tns", carpeta_mis);
    escribe_efk1(ruta);
    chk_cond("archivo", "EFK1: ya no se abre", !arch_cargar(1, &b));
    chk_cond("archivo", "EFK1: pero el hueco sigue ocupado", arch_existe(1) && arch_libre() != 1);
    escribe_cesp(ruta, &a, 0, 0);
    chk_cond("archivo", "el mismo hueco en CESP, escrito a mano, sí se abre", arch_cargar(1, &b) && mismos(&a, &b));
    f = fopen(ruta, "r+b");
    if (f) { fwrite("EFK3", 1, 4, f); fclose(f); }
    chk_cond("archivo", "con la marca EFK3 ya no se abre", !arch_cargar(1, &b));
    arch_borrar(1);

    /* Un archivo truncado no se abre y no toca el circuito de destino */
    sprintf(ruta, "%s/ces_01.tns", carpeta_mis);
    f = fopen(ruta, "wb");
    if (f) { fwrite("CESP", 1, 4, f); fputc(1, f); fputc(REJ_COLS, f); fclose(f); }
    ejemplo_cargar(EJ_PH, &b);
    a = b;
    chk_cond("archivo", "truncado: no se abre", !arch_cargar(0, &b));
    chk_cond("archivo", "truncado: el destino queda igual", mismos(&a, &b));
    limpia_mis();
    chk_cond("archivo", "borrados todos", arch_libre() == 0);

}

/* --------------------------------------------------------------------------
 *  9. Interfaz
 * ----------------------------------------------------------------------- */
static int nbmp = 0;

static void vuelca(const char *nombre)
{
    char ruta[256];
    ui_dibuja();
    sprintf(ruta, "%s/%03d_%s.bmp", dir_out, nbmp++, nombre);
    gfx_dump_bmp(ruta);
}

/* ¿Se ve este texto en la pantalla de ahora? (se redibuja entera) */
static int ve(const char *t)
{
    gfx_textos_reset();
    ui_dibuja();
    return gfx_texto_visto(t);
}

/* Con -DPOR_LA_CAPA (build_host.sh capa) las teclas del guion no van a
 * ui_tecla sino a la capa de entrada: una mano virtual las pulsa (las flechas,
 * un clic en su cuña del touchpad) y el bucle (bucle.c) las entrega. Tiene
 * que dar los mismos checks y los mismos BMP (ver test_entrada.inc) */
#ifdef POR_LA_CAPA
static void capa_tecla(int t);
static int ce_directo;                  /* las pruebas de la propia capa: directo */
#endif

static void teclas(const int *t, int n)
{
    int i;
    for (i = 0; i < n; i++) {
        ui_dibuja();
#ifdef POR_LA_CAPA
        if (!ce_directo) { capa_tecla(t[i]); continue; }
#endif
        ui_tecla(t[i]);
    }
}
#define TECLAS(...) do { static const int _t[] = { __VA_ARGS__ }; teclas(_t, (int)(sizeof _t / sizeof _t[0])); } while (0)

static void repite(int t, int n) { int i; for (i = 0; i < n; i++) teclas(&t, 1); }

static void recorre_pasos(const char *pref)
{
    char nom[48];
    int i;
    for (i = 0; ; i++) {
        sprintf(nom, "%s_paso%02d", pref, i + 1);
        vuelca(nom);
        if (ui_paso() >= ui_npasos() - 1) break;
        TECLAS(T_DER);
    }
}

/* El kanji de la portada (hakaru, «medir») se ve entero y tal cual lo generó
 * herramientas/gen_kanji.py. Se busca en toda la pantalla: dónde va lo
 * decide la portada. */
static int kanji_visto(void)
{
    const u16 *fb = gfx_buffer();
    int x, y, j;
    for (y = 0; y + KANJI_H <= SCR_H; y++)
        for (x = 0; x + KANJI_W <= SCR_W; x++) {
            for (j = 0; j < KANJI_H; j++)
                if (memcmp(fb + (y + j) * SCR_W + x, KANJI_PIX + j * KANJI_W, (size_t)KANJI_W * sizeof *fb)) break;
            if (j == KANJI_H) return 1;
        }
    return 0;
}

/* Se generó con la paleta de hoy: su fondo es C_FONDO (el borde no se nota)
 * y su tinta llena, C_TEXTO. Si cambia la paleta, hay que regenerarlo. */
static int kanji_de_la_paleta(void)
{
    int i, fondo = 0, tinta = 0;
    for (i = 0; i < KANJI_W * KANJI_H; i++) {
        if (KANJI_PIX[i] == C_FONDO) fondo++;
        if (KANJI_PIX[i] == C_TEXTO) tinta++;
    }
    return fondo > 0 && tinta > KANJI_W * KANJI_H / 10;
}

/* Píxeles que no son del fondo en un rectángulo de la pantalla de ahora */
static int pixeles_dibujados(int x0, int y0, int w, int h)
{
    const u16 *fb = gfx_buffer();
    int x, y, n = 0;
    for (y = y0; y < y0 + h; y++)
        for (x = x0; x < x0 + w; x++)
            if (fb[y * SCR_W + x] != C_FONDO) n++;
    return n;
}

static void test_ui(void)
{
    static Circuito c;
    static Red r;
    static Solucion s;
    int i, j, k;

    printf("[9] Interfaz\n");
    gfx_desbordes_reset();
    ui_init();
    vuelca("portada");
    chk_cond("ui", "portada: el título", ve("HAKARU"));
    chk_cond("ui", "portada: la sigla", ve("CES " S_CDOT " Circuit Emulator & Solver"));
    chk_cond("ui", "portada: el lema", ve("Emulador y calculadora de circuitos de CC y CA"));
    chk_cond("ui", "portada: el kanji, entero y tal cual se generó", kanji_visto());
    chk_cond("ui", "el kanji es de la paleta de hoy (C_FONDO y C_TEXTO)", kanji_de_la_paleta());
    /* La miniatura: dib_inicio busca el modelo por el principio de su título
     * («Emisor com...»); si alguien lo cambia, cae en un circuito vacío y la
     * zona de la miniatura (40, 80, 240x102) se queda en C_FONDO. Hoy tiene
     * unos 1400 píxeles dibujados; se pide el 1 % de la zona */
    chk_cond("ui", "portada: la miniatura del emisor común (el modelo está en la biblioteca)",
             pixeles_dibujados(40, 80, 240, 102) > 240 * 102 / 100);
    TECLAS(T_ENTER);
    chk_cond("ui", "menú: el nombre del programa", ve("Hakaru CES"));
    vuelca("menu");
    TECLAS(T_ABAJO); vuelca("menu_aprende");
    TECLAS(T_ABAJO); vuelca("menu_ayuda");
    TECLAS(T_ARRIBA, T_ARRIBA, T_ENTER);
    chk_cond("ui", "menú > Taller", ui_pantalla() == P_TALLER);
    vuelca("taller");
    TECLAS(T_ENTER);
    chk_cond("ui", "Taller > problema nuevo", ui_pantalla() == P_EDITOR && ui_circuito()->nel == 0);
    vuelca("editor_vacio");

    /* ---- Un problema que NO es del pizarrón, dibujado tecla a tecla:
     *      24 V; R1 = 4 en serie con R2 = 6 || R3; por R2 pasan 2 A; R3? */
    TECLAS(T_LETRA('W'), T_DER, T_ESC);
    TECLAS(T_LETRA('R'));
    vuelca("editor_accion_resistencia");
    TECLAS(T_DER);
    chk_cond("ui", "poner R abre su ficha", ui_pantalla() == P_ELEMENTO);
    vuelca("ficha_nueva");
    TECLAS(T_DIG(4), T_ENTER);
    TECLAS(T_LETRA('W'), T_DER, T_DER, T_ESC);
    TECLAS(T_LETRA('R'), T_ABAJO, T_DIG(6), T_ABAJO, T_ABAJO, T_DIG(2));
    vuelca("ficha_r2");
    TECLAS(T_ENTER);
    TECLAS(T_LETRA('W'), T_ABAJO, T_ESC, T_ARRIBA, T_ARRIBA);
    TECLAS(T_LETRA('W'), T_DER, T_ESC);
    TECLAS(T_LETRA('R'), T_ABAJO, T_INCOG, T_ENTER);
    TECLAS(T_LETRA('W'), T_ABAJO, T_IZQ, T_IZQ, T_IZQ, T_IZQ, T_IZQ, T_ARRIBA, T_ESC);
    vuelca("editor_sin_fuente");
    TECLAS(T_LETRA('E'), T_ARRIBA, T_DIG(2), T_DIG(4));
    vuelca("ficha_fuente");
    TECLAS(T_ENTER);
    vuelca("editor_problema_propio");
    chk_cond("ui", "propio: 3 resistencias y 1 fuente", ui_circuito()->nel == 4);

    c = *ui_circuito();
    red_extraer(&c, &r);
    resolver(&c, &r, &s);
    chk_cond("ui", "propio: se resuelve", s.estado == SOL_OK);
    k = busca_el(&c, "R3");
    if (s.estado == SOL_OK && k >= 0) chk("ui", "propio: R3 = 12", s.s[0].val[k], 12.0);

    /* enter junto a un elemento lo abre; con varios, pregunta la flecha */
    TECLAS(T_ENTER);
    vuelca("editor_enter");
    TECLAS(T_ESC);

    /* Deshacer: poner algo y quitarlo con Z */
    k = ui_circuito()->nel;
    repite(T_DER, 7);                                  /* a un sitio libre */
    TECLAS(T_LETRA('R'), T_ABAJO, T_ENTER);
    chk_cond("ui", "se añadió una resistencia", ui_circuito()->nel == k + 1);
    TECLAS(T_LETRA('Z'));
    chk_cond("ui", "Z sola no deshace (avisa)", ui_circuito()->nel == k + 1);
    TECLAS(T_CTRL, T_LETRA('Z'));
    chk_cond("ui", "ctrl y Z la quita", ui_circuito()->nel == k);
    vuelca("editor_deshecho");

    /* Menú de opciones */
    TECLAS(T_MENU);
    vuelca("editor_opciones");
    TECLAS(T_ABAJO, T_ABAJO, T_ABAJO, T_ENTER);        /* Guardar */
    chk_cond("ui", "Guardar deja el archivo", arch_existe(0));
    vuelca("editor_guardado");

    TECLAS(T_IGUAL);
    chk_cond("ui", "= lleva al resultado", ui_pantalla() == P_RESULTADO);
    vuelca("propio_resultado");
    for (i = 0; i < CAPA_N - 1; i++) { TECLAS(T_DER); vuelca("propio_capa"); }
    TECLAS(T_DER, T_TAB);
    vuelca("propio_tabla");
    TECLAS(T_ESC, T_ENTER);
    recorre_pasos("propio");
    TECLAS(T_ESC, T_ESC);
    chk_cond("ui", "esc vuelve al editor", ui_pantalla() == P_EDITOR);

    /* Un error: borrar todo y resolver; luego Z lo recupera */
    TECLAS(T_MENU);
    repite(T_ABAJO, 4);
    TECLAS(T_ENTER);
    chk_cond("ui", "Borrar todo deja el circuito vacío", ui_circuito()->nel == 0);
    vuelca("editor_borrado");
    TECLAS(T_IGUAL);
    chk_cond("ui", "resolver lo vacío da un resultado de error", ui_pantalla() == P_RESULTADO);
    vuelca("resultado_error");
    TECLAS(T_ESC, T_LETRA('Z') | T_CON_CTRL);
    chk_cond("ui", "Z recupera el circuito borrado", ui_circuito()->nel == 4);

    /* Mis problemas: salir al Taller y abrir el guardado */
    for (i = 0; i < 4 && ui_pantalla() == P_EDITOR; i++) TECLAS(T_ESC);
    chk_cond("ui", "esc del editor lleva al Taller", ui_pantalla() == P_TALLER);
    TECLAS(T_ABAJO, T_ABAJO, T_ENTER);
    chk_cond("ui", "Taller > Mis problemas", ui_pantalla() == P_MIS);
    vuelca("mis_problemas");
    TECLAS(T_ENTER);
    chk_cond("ui", "abre el guardado", ui_pantalla() == P_EDITOR &&
             strcmp(ui_circuito()->nombre, "Mi problema 1") == 0);
    vuelca("mis_abierto");

    /* ---- Aprende: todas las lecciones, páginas y ejemplos ---- */
    TECLAS(T_MENU);                                    /* opciones del editor */
    repite(T_ARRIBA, 1);                               /* la última: salir    */
    TECLAS(T_ENTER);
    chk_cond("ui", "opciones > menú principal", ui_pantalla() == P_MENU);
    TECLAS(T_ABAJO, T_ENTER);
    chk_cond("ui", "menú > Aprende", ui_pantalla() == P_LECCIONES);
    vuelca("lecciones");
    for (j = 0; j < N_LECCIONES; j++) {
        const Leccion *l = &LECCIONES[j];
        char nom[48];
        repite(T_ARRIBA, N_LECCIONES);
        repite(T_ABAJO, j);
        sprintf(nom, "lecciones_%d", j + 1);
        vuelca(nom);
        TECLAS(T_ENTER);
        for (i = 0; i < l->npag; i++) {
            sprintf(nom, "L%d_teoria_%d", j + 1, i + 1);
            vuelca(nom);
            TECLAS(T_DER);
        }
        chk_cond("ui", "después de la teoría, los ejemplos", ui_pantalla() == P_EJEMPLOS);
        for (i = 0; i < l->nej; i++) {
            sprintf(nom, "L%d_ejemplo_%d", j + 1, i + 1);
            vuelca(nom);
            TECLAS(T_ENTER);
            chk_cond("ui", "ejemplo resuelto", ui_pantalla() == P_RESULTADO);
            sprintf(nom, "L%d_ej%d_resultado", j + 1, i + 1);
            vuelca(nom);
            TECLAS(T_ENTER);
            sprintf(nom, "L%d_ej%d", j + 1, i + 1);
            recorre_pasos(nom);
            TECLAS(T_ESC, T_ESC);
            chk_cond("ui", "esc vuelve a la lista de ejemplos", ui_pantalla() == P_EJEMPLOS);
            TECLAS(T_ABAJO);
        }
        TECLAS(T_ESC);
        if (ui_pantalla() == P_TEORIA) TECLAS(T_ESC);
        chk_cond("ui", "vuelta a las lecciones", ui_pantalla() == P_LECCIONES);
    }

    /* Un ejemplo al Taller con [tab] */
    repite(T_ARRIBA, N_LECCIONES);
    TECLAS(T_ENTER, T_ENTER);                          /* lección 1 > ejemplos */
    chk_cond("ui", "enter en la teoría lleva a los ejemplos", ui_pantalla() == P_EJEMPLOS);
    TECLAS(T_TAB);
    chk_cond("ui", "[tab] lleva el ejemplo al Taller", ui_pantalla() == P_EDITOR &&
             ui_circuito()->nel > 0);
    vuelca("ejemplo_en_taller");

    /* Ayuda desde las opciones del editor, y desde el menú principal */
    TECLAS(T_MENU);
    repite(T_ABAJO, 5);
    TECLAS(T_ENTER);
    chk_cond("ui", "opciones > Ayuda", ui_pantalla() == P_AYUDA);
    TECLAS(T_ESC);
    chk_cond("ui", "esc en la ayuda vuelve al editor", ui_pantalla() == P_EDITOR);
    for (i = 0; i < 4 && ui_pantalla() == P_EDITOR; i++) TECLAS(T_ESC);
    TECLAS(T_ESC);
    chk_cond("ui", "Taller > esc > menú", ui_pantalla() == P_MENU);
    repite(T_ABAJO, 2);
    TECLAS(T_ENTER);
    chk_cond("ui", "menú > Ayuda", ui_pantalla() == P_AYUDA);
    vuelca("ayuda_1");
    repite(T_ABAJO, 15); vuelca("ayuda_2");
    repite(T_ABAJO, 15); vuelca("ayuda_3");
    TECLAS(T_ESC);
    chk_cond("ui", "esc en la ayuda vuelve al menú", ui_pantalla() == P_MENU);

    /* ---- Valores extremos: 1E9 V sobre 0,001 ohm. I = 1E12 A y P = 1E21 W
     *      tienen que caber en el panel y en la tabla (prefijos, exponente) */
    repite(T_ARRIBA, 2);
    TECLAS(T_ENTER);
    repite(T_ARRIBA, 3);
    TECLAS(T_ENTER);
    chk_cond("ui", "extremo: problema nuevo", ui_pantalla() == P_EDITOR && ui_circuito()->nel == 0);
    TECLAS(T_LETRA('R'), T_DER, T_DIG(0), T_PUNTO, T_DIG(0), T_DIG(0), T_DIG(1),
           T_ABAJO, T_ABAJO, T_INCOG, T_ENTER);
    TECLAS(T_LETRA('W'), T_ABAJO, T_IZQ, T_ESC);
    TECLAS(T_LETRA('E'), T_ARRIBA, T_DIG(1), T_EE, T_DIG(9), T_ENTER);
    chk_cond("ui", "extremo: R y E puestas", ui_circuito()->nel == 2);
    TECLAS(T_IGUAL);
    chk_cond("ui", "extremo: se resuelve", ui_pantalla() == P_RESULTADO);
    vuelca("extremo_resultado");
    TECLAS(T_TAB);
    chk_cond("ui", "extremo: tabla", ui_pantalla() == P_TABLA);
    vuelca("extremo_tabla");
    TECLAS(T_ESC, T_ESC);
    for (i = 0; i < 4 && ui_pantalla() == P_EDITOR; i++) TECLAS(T_ESC);
    TECLAS(T_ESC);
    chk_cond("ui", "extremo: vuelta al menú", ui_pantalla() == P_MENU);

    printf("    %d pantallas volcadas\n", nbmp);
    pruebas++;
    if (gfx_desbordes() != 0) falla("ui", "textos que se salen de la pantalla", gfx_desbordes(), 0);
    limpia_mis();
}

/* --------------------------------------------------------------------------
 *  La Ayuda, línea a línea hasta el final: ¿se ve este texto en algún
 *  momento?
 * ----------------------------------------------------------------------- */
static int ayuda_tiene(const char *t)
{
    int j;
    ui_init();
    TECLAS(T_ENTER, T_ABAJO, T_ABAJO, T_ENTER);                 /* menú > Ayuda */
    for (j = 0; j < 400 && ui_pantalla() == P_AYUDA; j++) {
        if (ve(t)) return 1;
        TECLAS(T_ABAJO);
    }
    return 0;
}

#if ESPECIAL
#include "test_especial.inc"        /* 10 y 16: las de la versión especial */
#endif

/* --------------------------------------------------------------------------
 *  17. Modo práctica: respuestas tapadas, contestar y destapar
 * ----------------------------------------------------------------------- */
static void teclea_numero(double v)
{
    char t[40];
    int i;
    sprintf(t, "%.6g", v);
    for (i = 0; t[i]; i++) {
        int k = t[i] >= '0' && t[i] <= '9' ? T_DIG(t[i] - '0') :
                t[i] == '.' ? T_PUNTO : t[i] == 'e' ? T_EE : t[i] == '-' ? T_SIGNO : T_NADA;
        if (k != T_NADA) teclas(&k, 1);
    }
}

static void test_practica(void)
{
    const Ejemplo *e;
    int bien, total, i, j, n0;

    printf("[17] Modo práctica\n");
    gfx_desbordes_reset();
    ui_init();
    TECLAS(T_ENTER);                                   /* menú */
    TECLAS(T_ABAJO, T_ENTER);                          /* Aprende */
    TECLAS(T_ENTER, T_ENTER);                          /* lección 1 > ejemplos */
    chk_cond("práctica", "en la lista de ejemplos", ui_pantalla() == P_EJEMPLOS);
    vuelca("practica_lista");
    e = &EJEMPLOS[(int)LECCIONES[0].ej[0]];
    TECLAS(T_INCOG);
    chk_cond("práctica", "[?!] en la lista: modo práctica, sin abrir nada", ui_pantalla() == P_EJEMPLOS);
    vuelca("practica_lista_tapada");
    TECLAS(T_ENTER);
    chk_cond("práctica", "enter abre el resultado tapado",
             ui_pantalla() == P_RESULTADO && ui_practica(&bien, &total) == 1 && total >= 1);
    vuelca("practica_tapada");

    /* izq/der no cambian de capa (destaparían la respuesta); tab no abre la tabla */
    i = ui_capa();
    TECLAS(T_DER);
    chk_cond("práctica", "las capas no se mueven", ui_practica(0, 0) == 1 && ui_capa() == i);

    /* Una respuesta mal escrita avisa y no cuenta */
    TECLAS(T_DIG(1), T_EE, T_EE, T_PUNTO, T_PUNTO);
    teclea_numero(12345);
    TECLAS(T_BORRAR);
    vuelca("practica_escribiendo");

    /* Una mala y luego la buena, cada una en su pregunta */
    for (i = 0; i < 20 && ui_practica(0, 0) == 1; i++) TECLAS(T_BORRAR);
    TECLAS(T_DIG(1), T_EE, T_ENTER);                   /* "1E": no es un número */
    ui_practica(&bien, &total);
    chk_cond("práctica", "un número mal escrito no cuenta", ui_practica(0, 0) == 1 && bien == 0);
    vuelca("practica_aviso");
    teclea_numero(e->clave * 2.0 + 1.0);
    TECLAS(T_ENTER);
    ui_practica(&bien, &total);
    vuelca("practica_mal");
    chk_cond("práctica", "una respuesta mal: no suma", bien == 0);
    if (total > 1) {
        /* contestar bien las que quedan no es fácil sin saberlas: se rinde */
        TECLAS(T_TAB);
    }
    chk_cond("práctica", "contestado todo o rendido: ya no está tapado", ui_practica(0, 0) == 2);
    vuelca("practica_hecha");
    i = ui_capa();
    TECLAS(T_DER);
    chk_cond("práctica", "después ya se pueden ver las capas", ui_pantalla() == P_RESULTADO && ui_capa() != i);
    TECLAS(T_IZQ);
    TECLAS(T_ESC);
    chk_cond("práctica", "esc vuelve a la lista", ui_pantalla() == P_EJEMPLOS && ui_practica(0, 0) == 0);

    /* La buena: la clave del ejemplo, redondeada a 4 cifras */
    TECLAS(T_ENTER);
    ui_practica(&bien, &total);
    n0 = total;
    for (j = 0; j < n0; j++) {
        teclea_numero(e->clave);
        TECLAS(T_ENTER);
        if (ui_practica(0, 0) != 1) break;
    }
    ui_practica(&bien, &total);
    chk_cond("práctica", "la clave cuenta como bien", bien >= 1);
    vuelca("practica_bien");
    TECLAS(T_ESC);

    /* Paso a paso con los resultados tapados: [enter] destapa, luego avanza */
    TECLAS(T_ENTER, T_ENTER);
    chk_cond("práctica", "enter sin número: los pasos", ui_pantalla() == P_PASOS);
    vuelca("practica_paso_tapado");
    chk_cond("práctica", "el paso llega tapado", ui_paso_tapado() && ui_paso() == 0);
    TECLAS(T_ENTER);
    chk_cond("práctica", "un enter lo destapa sin avanzar", !ui_paso_tapado() && ui_paso() == 0);
    vuelca("practica_paso_destapado");
    if (ui_npasos() > 1) {
        TECLAS(T_ENTER);
        chk_cond("práctica", "el siguiente avanza y el nuevo paso llega tapado", ui_paso() == 1 && ui_paso_tapado());
        TECLAS(T_IZQ);
        chk_cond("práctica", "volver a uno ya visto: sigue destapado", ui_paso() == 0 && !ui_paso_tapado());
    }
    TECLAS(T_ESC, T_ESC);
    chk_cond("práctica", "vuelta a la lista, aún en práctica", ui_pantalla() == P_EJEMPLOS);
    TECLAS(T_INCOG, T_ENTER);
    chk_cond("práctica", "[?!] otra vez: la lista normal", ui_pantalla() == P_RESULTADO && ui_practica(0, 0) == 0);
    TECLAS(T_ESC, T_ESC);

    /* Desde el editor: [?!] y la opción del menú */
    for (i = 0; i < 6 && ui_pantalla() != P_MENU; i++) TECLAS(T_ESC);
    TECLAS(T_ARRIBA, T_ARRIBA, T_ENTER, T_ENTER);      /* Taller > problema nuevo */
    TECLAS(T_LETRA('R'), T_DER, T_DIG(4), T_ABAJO, T_ABAJO, T_INCOG, T_ENTER);
    TECLAS(T_LETRA('W'), T_ABAJO, T_IZQ, T_ESC);
    TECLAS(T_LETRA('E'), T_ARRIBA, T_DIG(1), T_DIG(2), T_ENTER);
    TECLAS(T_INCOG);
    chk_cond("práctica", "[?!] en el editor", ui_pantalla() == P_RESULTADO && ui_practica(0, 0) == 1);
    teclea_numero(3);
    TECLAS(T_ENTER);
    ui_practica(&bien, &total);
    chk_cond("práctica", "12 V sobre 4 ohm: 3 A, bien", bien == 1 && total == 1 && ui_practica(0, 0) == 2);
    vuelca("practica_editor");
    TECLAS(T_ESC, T_MENU, T_ABAJO, T_ENTER);
    chk_cond("práctica", "opciones > Practicar", ui_pantalla() == P_RESULTADO && ui_practica(0, 0) == 1);
    TECLAS(T_ESC);

    /* Sin nada que preguntar no hay práctica: aviso y se queda en el editor */
    TECLAS(T_MENU);
    repite(T_ABAJO, 4);
    TECLAS(T_ENTER);                                   /* borrar todo */
    TECLAS(T_LETRA('R'), T_DER, T_DIG(4), T_ENTER);
    TECLAS(T_INCOG);
    chk_cond("práctica", "[?!] sin nada marcado: se queda en el editor", ui_pantalla() == P_EDITOR);
    vuelca("practica_sin_pregunta");

    /* Dos soluciones (E2 desconocida, |I(R1)| = 1 A y E1 = 10 V): las
     * respuestas tienen que salir de la misma solución */
    {
        static Circuito c;
        static Red r;
        static Solucion so;
        int kE1, kE2;
        TECLAS(T_MENU);
        repite(T_ABAJO, 4);
        TECLAS(T_ENTER);                               /* borrar todo */
        TECLAS(T_LETRA('W'), T_DER, T_ESC);
        TECLAS(T_LETRA('R'), T_DER, T_DIG(2), T_ABAJO, T_ABAJO, T_DIG(1), T_ENTER);
        TECLAS(T_LETRA('W'), T_DER, T_DER, T_ESC);
        TECLAS(T_LETRA('R'), T_ABAJO, T_DIG(5), T_ENTER);
        TECLAS(T_LETRA('W'), T_ABAJO, T_ESC, T_ARRIBA, T_ARRIBA);
        TECLAS(T_LETRA('W'), T_DER, T_ESC);
        TECLAS(T_LETRA('E'), T_ABAJO, T_DIG(1), T_DIG(0), T_ABAJO, T_INCOG, T_ENTER);
        TECLAS(T_LETRA('W'), T_ABAJO, T_IZQ, T_IZQ, T_IZQ, T_IZQ, T_IZQ, T_ARRIBA, T_ESC);
        TECLAS(T_LETRA('E'), T_ARRIBA, T_INCOG, T_ENTER);
        c = *ui_circuito();
        red_extraer(&c, &r);
        resolver(&c, &r, &so);
        kE1 = busca_el(&c, "E2");                     /* la desconocida (se puso la última) */
        kE2 = busca_el(&c, "E1");                     /* la de 10 V, de la que se pide I     */
        chk_cond("práctica", "el circuito de dos soluciones tiene dos", so.estado == SOL_OK && so.nsol == 2 && kE1 >= 0 && kE2 >= 0);
        if (so.estado == SOL_OK && so.nsol == 2 && kE1 >= 0 && kE2 >= 0 &&
            fabs(fabs(so.s[0].I[kE2]) - fabs(so.s[1].I[kE2])) > 0.1 &&
            fabs(fabs(so.s[0].val[kE1]) - fabs(so.s[1].val[kE1])) > 0.1) {
            int b2, t2;
            TECLAS(T_INCOG);
            ui_practica(&b2, &t2);
            chk_cond("práctica", "dos preguntas", t2 == 2);
            teclea_numero(fabs(so.s[1].I[kE2]));       /* I(E1) de la solución 2 */
            TECLAS(T_ENTER);
            teclea_numero(fabs(so.s[0].val[kE1]));     /* E2 de la solución 1: no casan */
            TECLAS(T_ENTER);
            ui_practica(&b2, &t2);
            chk_cond("práctica", "respuestas de soluciones distintas: solo cuenta una", b2 == 1);
            chk_cond("práctica", "se enseña la solución que cuadra con lo contestado", ui_isol() == 1);
            vuelca("practica_dos_soluciones");
            TECLAS(T_ESC);
        }
    }

    pruebas++;
    if (gfx_desbordes() != 0) falla("práctica", "textos que se salen de la pantalla", gfx_desbordes(), 0);
}

/* --------------------------------------------------------------------------
 *  18. El banco de tiempos (T en la Ayuda)
 * ----------------------------------------------------------------------- */
static int esc_tras;
static int esc_falso(void) { return --esc_tras <= 0; }   /* "se pulsa" a la 3ª consulta */

static void test_banco(void)
{
    int i, bien = 1;
    printf("[18] Banco de tiempos\n");
    gfx_desbordes_reset();
    ui_init();
    TECLAS(T_ENTER, T_ABAJO, T_ABAJO, T_ENTER);        /* menú > Ayuda */
    chk_cond("banco", "en la Ayuda", ui_pantalla() == P_AYUDA);
    TECLAS(T_LETRA('T'));
    chk_cond("banco", "T abre el banco", ui_pantalla() == P_BANCO && ui_animando());
    vuelca("banco_empieza");
    for (i = 0; i < 20 && !banco_hecho(); i++) { ui_dibuja(); ui_tick(); }
    chk_cond("banco", "mide todas las pruebas", banco_hecho() && !ui_animando());
    for (i = 0; i < BN_N; i++) if (!(banco_ms(i) > 0.0)) bien = 0;
    chk_cond("banco", "todas con un tiempo", bien);
    chk_cond("banco", "cuenta las operaciones del peor caso", banco_ops_peor() > 0 && banco_ops_ejemplo() > 0);
    chk_cond("banco", "cuenta las operaciones de un paso del emulador", banco_ops_paso() > 100.0);
    chk_cond("banco", "cuenta las operaciones y las iteraciones de un paso del puente",
             banco_ops_nl() > 100.0 && banco_iter_nl() >= 1.0);
    vuelca("banco_hecho");
    TECLAS(T_ENTER);
    chk_cond("banco", "enter repite", ui_pantalla() == P_BANCO && !banco_hecho());
    /* [esc] a mitad de una prueba: la corta, y el banco queda cancelado */
    esc_tras = 3;
    banco_salida(esc_falso);
    ui_tick();
    banco_salida(0);
    chk_cond("banco", "esc a mitad cancela", banco_hecho() && banco_cancelado() && !ui_animando());
    vuelca("banco_cancelado");
    TECLAS(T_ESC);
    chk_cond("banco", "esc vuelve a la Ayuda", ui_pantalla() == P_AYUDA);
    pruebas++;
    if (gfx_desbordes() != 0) falla("banco", "textos que se salen de la pantalla", gfx_desbordes(), 0);
}

/* --------------------------------------------------------------------------
 *  19. Mis problemas: un archivo dañado, o con otra marca, se ve y se puede
 *      borrar
 * ----------------------------------------------------------------------- */
static void test_mis_danado(void)
{
    /* el aviso de dib_mis (su primera línea: envuelve corta ahí) */
    static const char AVISO[] = "Este archivo está dañado o no es de esta versión.";
    static Circuito a;
    char ruta[260];
    FILE *f;
    printf("[19] Mis problemas con un archivo dañado\n");
    limpia_mis();
    ejemplo_cargar(EJ_PA, &a);
    arch_guardar(0, &a);
    sprintf(ruta, "%s/ces_02.tns", carpeta_mis);
    f = fopen(ruta, "wb");
    if (f) { fwrite("CESP" "\x09" "basura", 1, 11, f); fclose(f); }   /* versión 9: más nueva */
    ui_init();
    TECLAS(T_ENTER, T_ENTER);                          /* menú > Taller */
    TECLAS(T_ABAJO, T_ABAJO, T_ENTER);                 /* Mis problemas */
    chk_cond("archivo", "Mis problemas abierto", ui_pantalla() == P_MIS);
    TECLAS(T_ABAJO);
    chk_cond("archivo", "el de una versión más nueva: «(no se puede abrir)» y el aviso",
             ve("(no se puede abrir)") && ve(AVISO) && ve("[del] lo borra."));
    vuelca("mis_danado");
    TECLAS(T_ENTER);
    chk_cond("archivo", "el dañado no se abre", ui_pantalla() == P_MIS);
    TECLAS(T_BORRAR, T_BORRAR);
    chk_cond("archivo", "el dañado se borra", !arch_existe(1) && arch_existe(0));

    /* Otra marca (la de antes del nombre nuevo, EFK3) con el cuerpo de un
     * CESP bueno: no es un archivo dañado ni más nuevo, pero sale igual y
     * con el mismo aviso (docs/DESIGN.md, «Nombre y formato») */
    arch_guardar(1, &a);
    f = fopen(ruta, "r+b");
    if (f) { fwrite("EFK3", 1, 4, f); fclose(f); }
    TECLAS(T_ESC, T_ENTER);                            /* se vuelve a leer la lista */
    chk_cond("archivo", "otra marca: Mis problemas abierto con los dos", ui_pantalla() == P_MIS &&
             arch_existe(0) && arch_existe(1) && !arch_cargar(1, &a));
    TECLAS(T_ABAJO);
    chk_cond("archivo", "otra marca: «(no se puede abrir)» y el mismo aviso",
             ve("(no se puede abrir)") && ve(AVISO) && ve("[del] lo borra."));
    TECLAS(T_ENTER);
    chk_cond("archivo", "otra marca: no se abre", ui_pantalla() == P_MIS);
    TECLAS(T_BORRAR, T_BORRAR);
    chk_cond("archivo", "otra marca: se borra", !arch_existe(1) && arch_existe(0));
    TECLAS(T_ESC, T_ESC, T_ESC);
    limpia_mis();
}

/* --------------------------------------------------------------------------
 *  20. Entrada de valores: prefijos con eco, signo y las teclas nuevas
 * ----------------------------------------------------------------------- */
static int cables(const Circuito *c)
{
    int x, y, n = 0;
    for (y = 0; y < REJ_FILAS; y++)
        for (x = 0; x < REJ_COLS; x++) {
            if (x < REJ_COLS - 1 && c->h[y][x] == SEG_CABLE) n++;
            if (y < REJ_FILAS - 1 && c->v[y][x] == SEG_CABLE) n++;
        }
    return n;
}

static void test_entrada(void)
{
    const Circuito *c = ui_circuito();
    int h0;

    printf("[20] Entrada de valores: prefijos, eco, signo y teclas\n");
    gfx_desbordes_reset();
    ui_init();
    TECLAS(T_ENTER, T_ENTER, T_ENTER);                 /* un problema nuevo */
    chk_cond("entrada", "en el editor", ui_pantalla() == P_EDITOR && c->nel == 0);

    TECLAS(T_LETRA('R'), T_DER, T_DIG(4), T_DIG(7), T_LETRA('M'));
    vuelca("entrada_eco_mili");
    chk_cond("entrada", "el eco dice cuánto vale lo escrito", ve("= 0,047"));
    TECLAS(T_ENTER);
    chk("entrada", "47 y [M] = 47 mohm", c->el[0].val.v, 0.047);

    TECLAS(T_ENTER, T_DIG(2), T_SHIFT, T_LETRA('M'));
    vuelca("entrada_eco_mega");
    TECLAS(T_ENTER);
    chk("entrada", "2 y [shift][M] = 2 Mohm", c->el[0].val.v, 2e6);

    /* shift se gasta en una tecla, y pulsado dos veces se anula */
    TECLAS(T_ENTER, T_DIG(3), T_LETRA('M'), T_ENTER);
    chk("entrada", "después, [M] sola vuelve a ser mili", c->el[0].val.v, 3e-3);
    TECLAS(T_ENTER, T_DIG(4), T_SHIFT, T_SHIFT, T_LETRA('M'), T_ENTER);
    chk("entrada", "[shift][shift][M]: el shift se anula, mili", c->el[0].val.v, 4e-3);

    /* Una R no tiene signo: (-) al principio no escribe nada */
    TECLAS(T_ENTER, T_SIGNO, T_DIG(7), T_ENTER);
    chk("entrada", "(-) en una R no hace nada", c->el[0].val.v, 7.0);

    TECLAS(T_ENTER, T_DIG(3), T_LETRA('U'), T_ENTER);
    chk("entrada", "3 y [U] = 3 micro", c->el[0].val.v, 3e-6);

    TECLAS(T_ENTER, T_DIG(1), T_LETRA('G'), T_ENTER);
    chk("entrada", "1 y [G] = 1 giga", c->el[0].val.v, 1e9);

    TECLAS(T_ENTER, T_DIG(5), T_LETRA('K'), T_LETRA('M'), T_ENTER);
    chk("entrada", "un solo prefijo: 5k (la M sobra)", c->el[0].val.v, 5e3);

    TECLAS(T_ENTER, T_DIG(1), T_LETRA('P'), T_ENTER);
    chk_cond("entrada", "1 pohm es menos que el mínimo: avisa y no sale de la ficha",
             ui_pantalla() == P_ELEMENTO && c->el[0].val.v == 5e3);
    vuelca("entrada_minimo");
    TECLAS(T_BORRAR, T_BORRAR, T_DIG(8), T_ENTER);
    chk("entrada", "corregido a 8", c->el[0].val.v, 8.0);

    /* Una fuente con E negativa: el signo va al principio */
    TECLAS(T_LETRA('E'), T_ABAJO, T_SIGNO, T_DIG(5));
    vuelca("entrada_negativa");
    TECLAS(T_ENTER);
    chk("entrada", "E = -5 V", c->el[1].val.v, -5.0);
    chk_cond("entrada", "y el signo se ve en el dibujo", ve("-5V"));

    /* C pone un condensador (y abre su ficha); W tira el cable; Z sola no
     * deshace */
    h0 = cables(c);
    TECLAS(T_LETRA('C'), T_IZQ);
    vuelca("entrada_c_condensador");
    chk_cond("entrada", "[C] y una flecha ponen C1 y abren su ficha",
             ui_pantalla() == P_ELEMENTO && c->nel == 3 && c->el[2].tipo == EL_COND && ve("Condensador"));
    chk_cond("entrada", "un C nuevo empieza descargado (V0 = 0, dato)",
             c->el[2].par[Q_V0 - Q_P1].est == M_DATO && c->el[2].par[Q_V0 - Q_P1].v == 0.0);
    TECLAS(T_ESC);
    TECLAS(T_LETRA('W'), T_IZQ, T_ESC);
    chk_cond("entrada", "W tira el cable", cables(c) == h0 + 1);
    TECLAS(T_LETRA('Z'));
    chk_cond("entrada", "Z sola no deshace", cables(c) == h0 + 1);
    TECLAS(T_CTRL);
    vuelca("entrada_ctrl_armado");
    TECLAS(T_LETRA('Z'));
    chk_cond("entrada", "ctrl, Z deshace el cable", cables(c) == h0);
    TECLAS(T_LETRA('Z') | T_CON_CTRL);
    chk_cond("entrada", "ctrl+Z a la vez también (quita el C)", c->nel == 2);
    h0 = c->nel;
    TECLAS(T_LETRA('Z'));
    chk_cond("entrada", "el ctrl se gastó: otra Z sola no deshace", c->nel == h0);

    /* La paleta: [catálogo], flechas y enter es como pulsar la tecla */
    TECLAS(T_CATALOGO);
    vuelca("entrada_paleta");
    TECLAS(T_DER, T_DER, T_DER, T_ENTER);               /* el cuarto: voltímetro */
    h0 = c->nel;
    TECLAS(T_ABAJO, T_ENTER);
    chk_cond("entrada", "la paleta pone un voltímetro", c->nel == h0 + 1 && c->el[c->nel - 1].tipo == EL_VOLT);
    TECLAS(T_CATALOGO, T_ESC);
    chk_cond("entrada", "esc cierra la paleta", ui_pantalla() == P_EDITOR);
    TECLAS(T_CATALOGO, T_LETRA('V'));
    chk_cond("entrada", "con la paleta abierta, su letra también vale",
             ve("Voltímetro: ") && !ve("Componentes  [flechas]"));
    TECLAS(T_ESC);

    pruebas++;
    if (gfx_desbordes() != 0) falla("entrada", "textos que se salen de la pantalla", gfx_desbordes(), 0);
    for (h0 = 0; h0 < 4 && ui_pantalla() == P_EDITOR; h0++) TECLAS(T_ESC);
}

/* --------------------------------------------------------------------------
 *  21. La fuente de corriente J: redes al azar, con una E, y dibujada
 * ----------------------------------------------------------------------- */
static int texto_en_pasos(const char *t)
{
    int i;
    for (i = 0; i < pasos.n; i++)
        if (strstr(pasos.items[i].detalle, t) || strstr(pasos.items[i].resultado, t)) return 1;
    return 0;
}

static void test_fuente_corriente(void)
{
    static Solucion s;
    static Valores directo;
    int caso, k, j, f, met, n_arbol = 0, n_casos = 0;
    char q[48];

    printf("[21] Fuente de corriente J\n");
    for (caso = 0; caso < 300; caso++) {
        double J = 0.5 * (double)(1 + azar(40));
        f = fuente_sp(1.0);
        rc.el[f].tipo = EL_CORR;                   /* la misma red, con una J */
        rc.el[f].etiq[0] = 'J';
        rc.el[f].val.v = J;
        cierra_red();
        sprintf(q, "J %d directo", caso);
        met = comprueba(q, &rc, &rr, &s);
        if (s.estado != SOL_OK) continue;
        n_casos++;
        if (met == PROC_ARBOL) n_arbol++;
        chk_cond(q, "una J sola: por el árbol", met == PROC_ARBOL);
        chk(q, "la J impone su corriente", s.s[0].I[f], J);
        if (caso == 0) chk_cond(q, "el paso dice V = J·Req", texto_en_pasos("V = J" S_CDOT));
        directo = s.s[0];

        /* J desconocida, se da la V de sus bornes */
        rc.el[f].val.est = M_BUSCAR;
        rc.el[f].V.est = M_DATO; rc.el[f].V.v = fabs(directo.V[f]);
        sprintf(q, "J %d J oculta", caso);
        met = comprueba(q, &rc, &rr, &s);
        if (s.estado == SOL_OK) {
            chk(q, "recupera J", fabs(s.s[0].val[f]), J);
            chk_cond(q, "J oculta: por el árbol", met == PROC_ARBOL);
        }

        /* Una R oculta, con J y la V de la J */
        rc.el[f].val.est = M_DATO;
        k = 1 + (int)azar((unsigned)(rc.nel - 1));
        if (fabs(directo.I[k]) > 1e-6 * J) {
            double Roculta = rc.el[k].val.v;
            rc.el[k].val.est = M_BUSCAR;
            sprintf(q, "J %d %s oculta", caso, rc.el[k].etiq);
            met = comprueba(q, &rc, &rr, &s);
            if (s.estado == SOL_OK) chk(q, "recupera R", s.s[0].val[k], Roculta);
            rc.el[k].val.est = M_DATO;
        }
        rc.el[f].V.est = M_LIBRE;

        /* J desconocida, se da la V de una resistencia: se sube por el árbol */
        rc.el[f].val.est = M_BUSCAR;
        j = 1 + (int)azar((unsigned)(rc.nel - 1));
        if (fabs(directo.V[j]) > 1e-6 * fabs(directo.V[f])) {
            rc.el[j].V.est = M_DATO; rc.el[j].V.v = fabs(directo.V[j]);
            sprintf(q, "J %d desde V(%s)", caso, rc.el[j].etiq);
            met = comprueba(q, &rc, &rr, &s);
            if (s.estado == SOL_OK) {
                chk(q, "recupera J", fabs(s.s[0].val[f]), J);
                chk_cond(q, "por el árbol", met == PROC_ARBOL);
                if (caso == 0) chk_cond(q, "el paso de la J", texto_en_pasos("J = I("));
            }
            rc.el[j].V.est = M_LIBRE;
        }
        rc.el[f].val.est = M_DATO;

        /* Con una E además: dos fuentes, Kirchhoff, y la ley de la J */
        if (rc.nel < MAX_EL && nodos > 2) {
            int f2 = nuevo_el(EL_FUENTE, 0, (int)azar((unsigned)nodos));
            rc.el[f2].val.v = 1.0 + azar(50); rc.el[f2].val.est = M_DATO;
            cierra_red();
            if (rr.a[f2] != rr.b[f2]) {
                sprintf(q, "J %d con una E", caso);
                met = comprueba(q, &rc, &rr, &s);
                if (s.estado == SOL_OK) {
                    chk_cond(q, "dos fuentes: Kirchhoff", met == PROC_KIRCHHOFF);
                    chk(q, "la J sigue imponiendo su corriente", s.s[0].I[f], J);
                }
            }
        }
    }
    printf("    %d redes con J, %d por el árbol\n", n_casos, n_arbol);

    /* Una J con un borne al aire */
    {
        static Circuito c;
        static Red r;
        int kj;
        circ_init(&c, "J al aire");
        kj = circ_elemento(&c, EL_CORR, 0, 1, DIR_ARR);
        c.el[kj].val.est = M_DATO; c.el[kj].val.v = 2.0;
        k = circ_elemento(&c, EL_RES, 0, 0, DIR_DER);
        c.el[k].val.est = M_DATO; c.el[k].val.v = 4.0;
        red_extraer(&c, &r); resolver(&c, &r, &s);
        chk_cond("J", "J con un borne al aire: se dice", s.estado == SOL_ERR_RED && r.error == RED_J_ABIERTA && s.elem == kj);
    }

    /* Dibujada en el editor: J = 3 A sobre 6 ohm en paralelo con 3 ohm */
    {
        const Circuito *c = ui_circuito();
        gfx_desbordes_reset();
        ui_init();
        TECLAS(T_ENTER, T_ENTER, T_ENTER);
        TECLAS(T_LETRA('J'));
        vuelca("j_poner");
        TECLAS(T_ARRIBA, T_DIG(3), T_ABAJO, T_INCOG);
        vuelca("j_ficha");
        TECLAS(T_ENTER);
        TECLAS(T_LETRA('W'), T_DER, T_ESC);
        TECLAS(T_LETRA('R'), T_ABAJO, T_DIG(6), T_ENTER);
        TECLAS(T_ARRIBA);                              /* una flecha sola: solo mueve */
        TECLAS(T_LETRA('W'), T_DER, T_ESC);
        TECLAS(T_LETRA('R'), T_ABAJO, T_DIG(3), T_ABAJO, T_ABAJO, T_INCOG, T_ENTER);
        TECLAS(T_LETRA('W'), T_IZQ, T_IZQ, T_ESC);
        vuelca("j_editor");
        chk_cond("J", "dibujada: J, dos R", c->nel == 3 && c->el[0].tipo == EL_CORR);
        TECLAS(T_IGUAL);
        chk_cond("J", "se resuelve", ui_pantalla() == P_RESULTADO);
        vuelca("j_resultado");
        {
            static Circuito cc2;
            static Red r2;
            cc2 = *c;
            red_extraer(&cc2, &r2); resolver(&cc2, &r2, &s);
            chk_cond("J", "3 A en 6 || 3: V = 6 V, I(R2) = 2 A",
                     s.estado == SOL_OK && fabs(fabs(s.s[0].V[0]) - 6.0) < 1e-9 &&
                     fabs(fabs(s.s[0].I[2]) - 2.0) < 1e-9);
        }
        TECLAS(T_DER);
        vuelca("j_corrientes");
        TECLAS(T_ENTER);
        recorre_pasos("j");
        TECLAS(T_ESC, T_ESC);
        pruebas++;
        if (gfx_desbordes() != 0) falla("J", "textos que se salen de la pantalla", gfx_desbordes(), 0);
        for (k = 0; k < 4 && ui_pantalla() == P_EDITOR; k++) TECLAS(T_ESC);
    }
}

/* --------------------------------------------------------------------------
 *  22. Voltímetro y amperímetro
 * ----------------------------------------------------------------------- */
/* Un lazo: fuente E, y en serie R1 y R2 (y lo que se añada después) */
static int lazo(Circuito *c, Red *r, double E, double R1, double R2)
{
    int f;
    (void)r;
    circ_init(c, "lazo");
    f = circ_elemento(c, EL_FUENTE, 0, 1, DIR_ARR);
    c->el[f].val.est = M_DATO; c->el[f].val.v = E;
    circ_elemento(c, EL_RES, 0, 0, DIR_DER);
    c->el[1].val.est = M_DATO; c->el[1].val.v = R1;
    circ_elemento(c, EL_RES, 1, 0, DIR_ABA);
    c->el[2].val.est = M_DATO; c->el[2].val.v = R2;
    circ_cable(c, 1, 1, DIR_IZQ);
    return f;
}

static void test_instrumentos(void)
{
    static Circuito c;
    static Red r;
    static Solucion s, s0;
    int caso, k, n_ok = 0;
    char q[48];

    printf("[22] Voltímetro y amperímetro\n");

    /* Divisor 10k + 10k a 12 V; el voltímetro sobre R2 */
    lazo(&c, &r, 12.0, 1e4, 1e4);
    k = circ_elemento(&c, EL_VOLT, 2, 0, DIR_ABA);        /* de (2,0) a (2,1) */
    circ_cable(&c, 1, 0, DIR_DER);
    circ_cable(&c, 2, 1, DIR_IZQ);
    c.el[k].V.est = M_BUSCAR;
    red_extraer(&c, &r);
    comprueba("voltímetro ideal", &c, &r, &s);
    if (s.estado == SOL_OK) chk("voltímetro", "ideal: marca 6 V", fabs(s.s[0].V[k]), 6.0);
    c.el[k].val.est = M_DATO; c.el[k].val.v = 1e4;
    comprueba("voltímetro de 10 k", &c, &r, &s);
    if (s.estado == SOL_OK) chk("voltímetro", "de 10 kohm: marca 4 V (efecto de carga)", fabs(s.s[0].V[k]), 4.0);

    /* Inverso: marca 4 V; ¿qué R interna tiene? */
    c.el[k].val.est = M_BUSCAR;
    c.el[k].V.est = M_DATO; c.el[k].V.v = 4.0;
    comprueba("R del voltímetro", &c, &r, &s);
    if (s.estado == SOL_OK) chk("voltímetro", "si marca 4 V, su R es 10 kohm", s.s[0].val[k], 1e4);

    /* Amperímetro en serie: 12 V sobre 4 ohm (R1 = 1, R2 = 3) */
    lazo(&c, &r, 12.0, 1.0, 3.0);
    circ_quitar(&c, 1, 1, DIR_IZQ);                       /* se abre el lazo abajo */
    k = circ_elemento(&c, EL_AMP, 1, 1, DIR_IZQ);
    c.el[k].I.est = M_BUSCAR;
    red_extraer(&c, &r);
    comprueba("amperímetro ideal", &c, &r, &s);
    if (s.estado == SOL_OK) chk("amperímetro", "ideal: marca 3 A", fabs(s.s[0].I[k]), 3.0);
    c.el[k].val.est = M_DATO; c.el[k].val.v = 2.0;
    comprueba("amperímetro de 2 ohm", &c, &r, &s);
    if (s.estado == SOL_OK) chk("amperímetro", "con 2 ohm dentro: marca 2 A", fabs(s.s[0].I[k]), 2.0);

    /* Mal usado: en paralelo con la fuente la pone en corto */
    {
        static Circuito d;
        int kf, ka;
        circ_init(&d, "A en paralelo");
        kf = circ_elemento(&d, EL_FUENTE, 0, 1, DIR_ARR);
        d.el[kf].val.est = M_DATO; d.el[kf].val.v = 12.0;
        circ_cable(&d, 0, 0, DIR_DER);
        ka = circ_elemento(&d, EL_AMP, 1, 0, DIR_ABA);
        circ_cable(&d, 1, 1, DIR_IZQ);
        k = circ_elemento(&d, EL_RES, 1, 0, DIR_DER);
        d.el[k].val.est = M_DATO; d.el[k].val.v = 4.0;
        circ_cable(&d, 2, 0, DIR_ABA);
        circ_cable(&d, 2, 1, DIR_IZQ);
        red_extraer(&d, &r); resolver(&d, &r, &s);
        chk_cond("amperímetro", "en paralelo con la fuente: la pone en corto, y se dice",
                 s.estado == SOL_ERR_RED && r.error == RED_FUENTE_AMP && s.elem == kf);
        (void)ka;
    }

    /* Redes al azar: un instrumento ideal no cambia nada, y marca lo que hay */
    for (caso = 0; caso < 250; caso++) {
        int a, b, kv, ka, kr, nuevo;
        fuente_sp(1.0 + azar(200));
        resolver(&rc, &rr, &s0);
        if (s0.estado != SOL_OK || nodos < 3) continue;

        a = (int)azar((unsigned)nodos); b = (int)azar((unsigned)nodos);
        if (a == b) continue;
        kv = nuevo_el(EL_VOLT, a, b);
        rc.el[kv].val.est = M_LIBRE;
        rc.el[kv].V.est = M_BUSCAR;
        kr = 1 + (int)azar((unsigned)(rc.nel - 2));       /* una R de la red */
        nuevo = nodos++;
        ka = nuevo_el(EL_AMP, nuevo, rr.b[kr]);           /* en serie con ella */
        rc.el[ka].val.est = M_LIBRE;
        rr.b[kr] = nuevo;
        cierra_red();
        sprintf(q, "instrumentos %d", caso);
        comprueba(q, &rc, &rr, &s);
        if (s.estado != SOL_OK) continue;
        for (k = 0; k < s0.s[0].I[0] * 0 + kv; k++) {
            char d[48];
            sprintf(d, "%s no cambia", rc.el[k].etiq);
            chk(q, d, s.s[0].I[k], s0.s[0].I[k]);
        }
        chk(q, "el voltímetro marca phi(a) - phi(b)",
            fabs(s.s[0].V[kv]), fabs(s0.s[0].phi[a] - s0.s[0].phi[b]));
        chk(q, "el amperímetro marca la I de su R", fabs(s.s[0].I[ka]), fabs(s0.s[0].I[kr]));
        n_ok++;
    }
    printf("    %d redes con un voltímetro y un amperímetro ideales\n", n_ok);

    /* Dibujado: un voltímetro sobre una R */
    {
        const Circuito *cu = ui_circuito();
        gfx_desbordes_reset();
        ui_init();
        TECLAS(T_ENTER, T_ENTER, T_ENTER);
        /* E en la columna 1, R vertical en la 2, el amperímetro cierra el
         * lazo abajo y el voltímetro va en paralelo con R, en la columna 3 */
        TECLAS(T_LETRA('E'), T_ARRIBA, T_DIG(1), T_DIG(2), T_ENTER);
        TECLAS(T_LETRA('W'), T_DER, T_ESC);
        TECLAS(T_LETRA('R'), T_ABAJO, T_DIG(4), T_ENTER);
        TECLAS(T_LETRA('A'), T_IZQ);
        vuelca("inst_ficha_amperimetro");
        TECLAS(T_INCOG, T_ENTER);
        TECLAS(T_DER, T_ARRIBA);
        TECLAS(T_LETRA('W'), T_DER, T_ESC);
        TECLAS(T_LETRA('V'), T_ABAJO);
        vuelca("inst_ficha_voltimetro");
        TECLAS(T_INCOG, T_ENTER);
        TECLAS(T_LETRA('W'), T_IZQ, T_ESC);
        vuelca("inst_editor");
        chk_cond("instrumentos", "dibujados", cu->nel == 4);
        TECLAS(T_IGUAL);
        vuelca("inst_resultado");
        chk_cond("instrumentos", "se resuelve", ui_pantalla() == P_RESULTADO);
        TECLAS(T_TAB);
        vuelca("inst_tabla");
        chk_cond("instrumentos", "la tabla no da 0 ohm a un instrumento ideal",
                 ui_pantalla() == P_TABLA && ve("ideal"));
        TECLAS(T_ESC);
        {
            static Circuito c2;
            c2 = *cu;
            red_extraer(&c2, &r); resolver(&c2, &r, &s);
            chk_cond("instrumentos", "12 V sobre 4 ohm: A marca 3 A y V marca 12 V",
                     s.estado == SOL_OK && fabs(fabs(s.s[0].I[2]) - 3.0) < 1e-9 &&
                     fabs(fabs(s.s[0].V[3]) - 12.0) < 1e-9);
        }
        TECLAS(T_ENTER);
        recorre_pasos("inst");
        TECLAS(T_ESC, T_ESC);
        pruebas++;
        if (gfx_desbordes() != 0) falla("instrumentos", "textos que se salen", gfx_desbordes(), 0);
        for (k = 0; k < 4 && ui_pantalla() == P_EDITOR; k++) TECLAS(T_ESC);
    }
}

/* --------------------------------------------------------------------------
 *  23. Fuente real (E, r)
 * ----------------------------------------------------------------------- */
static void test_fuente_real(void)
{
    static Circuito c;
    static Red r;
    static Solucion s;
    int caso, f, k, met, n_ok = 0, n_arbol = 0;
    char q[48];

    printf("[23] Fuente real (E, r)\n");

    /* E = 12 V, r = 0,5 ohm y R = 5,5 ohm: I = 2 A y 11 V en bornes */
    f = lazo(&c, &r, 12.0, 2.5, 3.0);
    c.el[f].aux.est = M_DATO; c.el[f].aux.v = 0.5;
    red_extraer(&c, &r);
    met = comprueba("fuente real", &c, &r, &s);
    if (s.estado == SOL_OK) {
        chk("fuente real", "I = E/(r + R) = 2 A", fabs(s.s[0].I[f]), 2.0);
        chk("fuente real", "en bornes E - r I = 11 V", fabs(s.s[0].V[f]), 11.0);
        chk_cond("fuente real", "por el árbol, con la r en serie", met == PROC_ARBOL && texto_en_pasos("r1"));
        chk_cond("fuente real", "la raíz con la r se llama Rtot, no Req", texto_en_pasos("Rtot"));
        chk_cond("fuente real", "paso de la tensión en bornes, con su número",
                 texto_en_pasos("V = E - r") && texto_en_pasos("11 V"));
        chk_cond("fuente real", "y la potencia entregada y la perdida en r",
                 texto_en_pasos("Entregada al circuito") && texto_en_pasos("Perdida en r"));
    }

    /* La r desconocida, con la I de la fuente */
    c.el[f].aux.est = M_BUSCAR;
    c.el[f].I.est = M_DATO; c.el[f].I.v = 2.0;
    met = comprueba("r desconocida", &c, &r, &s);
    if (s.estado == SOL_OK) {
        chk("fuente real", "r = 0,5 ohm", s.s[0].aux[f], 0.5);
        chk_cond("fuente real", "r desconocida: por el árbol", met == PROC_ARBOL);
    }

    /* Una I mayor que E/R pide una r negativa: no hay fuente así */
    c.el[f].I.v = 3.0;
    resolver(&c, &r, &s);
    chk_cond("fuente real", "r negativa: sin solución física", s.estado == SOL_NO_FISICA && s.elem == f);
    {
        char m[200];
        sol_mensaje(&c, &r, &s, m, sizeof m);
        chk_cond("fuente real", "el mensaje habla de la r interna", strstr(m, "r interna") != 0);
    }

    /* Redes al azar: la misma red con una r en la fuente */
    for (caso = 0; caso < 250; caso++) {
        static Solucion s0;
        double E = 1.0 + azar(200), rint = 0.1 * (double)(1 + azar(50));
        f = fuente_sp(E);
        rc.el[f].aux.est = M_DATO; rc.el[f].aux.v = rint;
        sprintf(q, "real %d", caso);
        met = comprueba(q, &rc, &rr, &s0);
        if (s0.estado != SOL_OK) continue;
        n_ok++;
        if (met == PROC_ARBOL) n_arbol++;
        chk(q, "en bornes: E - r I", s0.s[0].V[f], E - rint * s0.s[0].I[f]);
        /* la r oculta, con la I de la fuente */
        rc.el[f].aux.est = M_BUSCAR;
        rc.el[f].I.est = M_DATO; rc.el[f].I.v = fabs(s0.s[0].I[f]);
        sprintf(q, "real %d r oculta", caso);
        comprueba(q, &rc, &rr, &s);
        if (s.estado == SOL_OK) chk(q, "recupera r", s.s[0].aux[f], rint);
        rc.el[f].I.est = M_LIBRE;
        /* E oculta, con la V de una resistencia */
        rc.el[f].aux.est = M_DATO;
        rc.el[f].val.est = M_BUSCAR;
        k = 1 + (int)azar((unsigned)(rc.nel - 1));
        if (fabs(s0.s[0].V[k]) > 1e-6 * E) {
            rc.el[k].V.est = M_DATO; rc.el[k].V.v = fabs(s0.s[0].V[k]);
            sprintf(q, "real %d E desde V(%s)", caso, rc.el[k].etiq);
            comprueba(q, &rc, &rr, &s);
            if (s.estado == SOL_OK) chk(q, "recupera E", s.s[0].val[f], E);
        }
    }
    printf("    %d redes con fuente real, %d por el árbol\n", n_ok, n_arbol);

    /* En el editor: la ficha tiene 4 campos y se desplaza */
    {
        const Circuito *cu = ui_circuito();
        gfx_desbordes_reset();
        ui_init();
        TECLAS(T_ENTER, T_ENTER, T_ENTER);
        TECLAS(T_LETRA('E'), T_ARRIBA, T_DIG(1), T_DIG(2), T_ABAJO, T_ABAJO);
        vuelca("real_ficha_r");
        TECLAS(T_PUNTO, T_DIG(5), T_ABAJO);
        vuelca("real_ficha_v");
        TECLAS(T_INCOG, T_ENTER);
        chk_cond("fuente real", "r = 0,5 y V buscada, desde la ficha",
                 cu->el[0].aux.est == M_DATO && fabs(cu->el[0].aux.v - 0.5) < 1e-12 && cu->el[0].V.est == M_BUSCAR);
        TECLAS(T_LETRA('W'), T_DER, T_ESC);
        TECLAS(T_LETRA('R'), T_ABAJO, T_DIG(5), T_PUNTO, T_DIG(5), T_ENTER);
        TECLAS(T_LETRA('W'), T_IZQ, T_ESC);
        vuelca("real_editor");
        TECLAS(T_IGUAL);
        vuelca("real_resultado");
        chk_cond("fuente real", "la r se ve en el dibujo", ve("r=0,5"));
        TECLAS(T_ENTER);
        recorre_pasos("real");
        TECLAS(T_ESC, T_ESC);
        pruebas++;
        if (gfx_desbordes() != 0) falla("fuente real", "textos que se salen", gfx_desbordes(), 0);
        for (k = 0; k < 4 && ui_pantalla() == P_EDITOR; k++) TECLAS(T_ESC);
    }
}

/* --------------------------------------------------------------------------
 *  24. Tierra, potenciales y terminales A-B
 * ----------------------------------------------------------------------- */
static void test_tierra(void)
{
    static Circuito c;
    static Red r;
    static Solucion s;
    int np, nm, k;

    printf("[24] Tierra, potenciales y terminales\n");
    /* 12 V, 4 ohm y 12 ohm en serie, la tierra entre las dos */
    lazo(&c, &r, 12.0, 4.0, 12.0);
    c.tierra_x = 1; c.tierra_y = 0;
    red_extraer(&c, &r);
    comprueba("tierra", &c, &r, &s);
    np = r.punto[0 * REJ_COLS + 0];                     /* el + de la fuente */
    nm = r.punto[1 * REJ_COLS + 0];                     /* el - */
    chk_cond("tierra", "la tierra es la referencia", r.ref[r.punto[0 * REJ_COLS + 1]] == 1);
    if (s.estado == SOL_OK) {
        chk("tierra", "el + queda a 3 V", s.s[0].phi[np], 3.0);
        chk("tierra", "el - queda a -9 V", s.s[0].phi[nm], -9.0);
    }
    c.tierra_x = c.tierra_y = -1;                       /* sin tierra: 0 V en el - */
    red_extraer(&c, &r); resolver(&c, &r, &s);
    if (s.estado == SOL_OK) chk("tierra", "sin tierra, el + a 12 V", s.s[0].phi[r.punto[0]], 12.0);

    /* Dibujado: tierra con [G], terminales con [T], capa de potenciales */
    {
        const Circuito *cu = ui_circuito();
        gfx_desbordes_reset();
        ui_init();
        TECLAS(T_ENTER, T_ENTER, T_ENTER);
        /* se piden tres cosas: I(E1), V(R1) y V(R2) */
        TECLAS(T_LETRA('E'), T_ARRIBA, T_DIG(1), T_DIG(2), T_ABAJO, T_INCOG, T_ENTER);  /* de (1,1) a (1,0) */
        TECLAS(T_LETRA('T'));                                          /* A en el + */
        TECLAS(T_LETRA('R'), T_DER, T_DIG(4), T_ABAJO, T_INCOG, T_ENTER);
        TECLAS(T_LETRA('G'));                                          /* tierra entre R1 y R2 */
        TECLAS(T_LETRA('R'), T_ABAJO, T_DIG(1), T_DIG(2), T_ABAJO, T_INCOG, T_ENTER);
        TECLAS(T_LETRA('W'), T_IZQ, T_ESC);
        TECLAS(T_LETRA('T'));                                          /* B en el - */
        vuelca("tierra_editor");
        chk_cond("tierra", "tierra y terminales puestos",
                 cu->tierra_x == 2 && cu->tierra_y == 0 && cu->term_x[0] == 1 && cu->term_y[0] == 0 &&
                 cu->term_x[1] == 1 && cu->term_y[1] == 1);
        TECLAS(T_IGUAL);
        vuelca("tierra_resultado");
        chk_cond("tierra", "con tres cosas pedidas, V(AB) también sale", ve("V(AB) = V"));
        {
            static Circuito c2;
            double vab;
            int na, nb;
            c2 = *cu;
            red_extraer(&c2, &r); resolver(&c2, &r, &s);
            chk_cond("tierra", "V(AB) = +12 V (A en el +, B en el -)",
                     s.estado == SOL_OK && sol_vab(&c2, &r, &s.s[0], &vab, &na, &nb) == 1 &&
                     fabs(vab - 12.0) < 1e-9);
        }
        for (k = 0; k < CAPA_N && ui_capa() != CAPA_POTENCIALES; k++) TECLAS(T_DER);
        chk_cond("tierra", "capa de potenciales", ui_capa() == CAPA_POTENCIALES);
        vuelca("tierra_potenciales");
        chk_cond("tierra", "los potenciales se ven: 3 V y -9 V", ve("3V") && ve("-9V"));
        TECLAS(T_ESC);
        TECLAS(T_ARRIBA, T_DER);                        /* el cursor, de B a la tierra */
        TECLAS(T_LETRA('G'));                           /* G sobre la tierra: la quita */
        chk_cond("tierra", "G sobre la tierra la quita", cu->tierra_x == -1 && cu->tierra_y == -1);
        TECLAS(T_IZQ, T_ABAJO);
        TECLAS(T_LETRA('T'));                           /* T en el B: lo quita */
        chk_cond("tierra", "T sobre un terminal lo quita",
                 cu->term_x[1] == -1 && circuito_sano(cu) == 0);
        TECLAS(T_DER, T_DER, T_DER, T_DER, T_ABAJO, T_ABAJO);
        TECLAS(T_LETRA('G'));                           /* en un punto suelto: no */
        chk_cond("tierra", "G en un punto suelto: avisa y no la pone",
                 cu->tierra_x == -1 && ve("no hay circuito"));
        pruebas++;
        if (gfx_desbordes() != 0) falla("tierra", "textos que se salen", gfx_desbordes(), 0);
        for (k = 0; k < 4 && ui_pantalla() == P_EDITOR; k++) TECLAS(T_ESC);
    }
}

/* --------------------------------------------------------------------------
 *  25. Lo que encontró la revisión de la Etapa 1: cada caso fue un fallo real
 * ----------------------------------------------------------------------- */
/* Una pila en la columna 1 (+ arriba), R en la columna 2 y un voltímetro en
 * sus bornes, en la columna 0 */
static int pila_v(Circuito *c, double R, int *kv)
{
    int f, k;
    circ_init(c, "pila");
    f = circ_elemento(c, EL_FUENTE, 1, 1, DIR_ARR);
    circ_cable(c, 1, 0, DIR_DER);
    k = circ_elemento(c, EL_RES, 2, 0, DIR_ABA);
    c->el[k].val.est = M_DATO; c->el[k].val.v = R;
    circ_cable(c, 2, 1, DIR_IZQ);
    circ_cable(c, 1, 0, DIR_IZQ);
    *kv = circ_elemento(c, EL_VOLT, 0, 0, DIR_ABA);
    circ_cable(c, 0, 1, DIR_DER);
    return f;
}

/* Fuente de (0,1) a (0,0) y otro elemento en la columna 1 cerrando el lazo */
static int fuente_y(Circuito *c, TipoEl tf, double val, TipoEl otro, int *ko)
{
    int f;
    circ_init(c, "caso");
    f = circ_elemento(c, tf, 0, 1, DIR_ARR);
    c->el[f].val.est = M_DATO; c->el[f].val.v = val;
    circ_cable(c, 0, 0, DIR_DER);
    *ko = circ_elemento(c, otro, 1, 0, DIR_ABA);
    circ_cable(c, 1, 1, DIR_IZQ);
    return f;
}

static void test_revision_e1(void)
{
    static Circuito c;
    static Red r;
    static Solucion s;
    char m[240];
    int f, k, k2, i, met;

    printf("[25] Revisión de la Etapa 1\n");

    /* r buscada en una fuente sin corriente: no se puede hallar */
    f = fuente_y(&c, EL_FUENTE, 10.0, EL_VOLT, &k);
    c.el[f].aux.est = M_BUSCAR;
    c.el[k].V.est = M_DATO; c.el[k].V.v = 8.0;
    red_extraer(&c, &r); resolver(&c, &r, &s);
    sol_mensaje(&c, &r, &s, m, sizeof m);
    chk_cond("revisión", "r buscada sin corriente: no se puede hallar",
             s.estado == SOL_SIN_CORRIENTE && s.elem == f && strstr(m, "r interna") != 0);

    /* La pila con r = 1 y R = 5; el voltímetro en bornes marca 10 V: E = 12.
     * Va por Kirchhoff (hay un instrumento), con la ley de la fuente real */
    f = pila_v(&c, 5.0, &k);
    c.el[f].val.est = M_BUSCAR;
    c.el[f].aux.est = M_DATO; c.el[f].aux.v = 1.0;
    c.el[k].V.est = M_DATO; c.el[k].V.v = 10.0;
    red_extraer(&c, &r);
    met = comprueba("pila: E desde el voltímetro", &c, &r, &s);
    if (s.estado == SOL_OK) {
        chk("revisión", "E = 12 V", s.s[0].val[f], 12.0);
        chk_cond("revisión", "Kirchhoff escribe la ley de la pila con E",
                 met == PROC_KIRCHHOFF && texto_en_pasos("= E - 1"));
        chk_cond("revisión", "y despeja E con la r: E = V + r·I", texto_en_pasos("+ r·I"));
    }
    /* La misma con E = 12 y la r buscada */
    c.el[f].val.est = M_DATO; c.el[f].val.v = 12.0;
    c.el[f].aux.est = M_BUSCAR;
    met = comprueba("pila: r desde el voltímetro", &c, &r, &s);
    if (s.estado == SOL_OK) {
        chk("revisión", "r = 1 ohm", s.s[0].aux[f], 1.0);
        chk_cond("revisión", "el paso a paso da la r", texto_en_pasos("r = (E - V)/I"));
    }
    /* E y r a la vez: con un solo estado no se separan */
    c.el[f].val.est = M_BUSCAR;
    c.el[f].I.est = M_DATO; c.el[f].I.v = 2.0;
    resolver(&c, &r, &s);
    sol_mensaje(&c, &r, &s, m, sizeof m);
    chk_cond("revisión", "E y r a la vez: se dice que no se separan",
             s.estado == SOL_INDET && s.elem == f && strstr(m, "no se separan") != 0);

    /* Fuente ideal con E buscada y la V en bornes como dato: sale por el árbol */
    f = lazo(&c, &r, 0.0, 1.0, 3.0);
    c.el[f].val.est = M_BUSCAR;
    c.el[f].V.est = M_DATO; c.el[f].V.v = 12.0;
    red_extraer(&c, &r);
    met = comprueba("V en bornes como dato", &c, &r, &s);
    if (s.estado == SOL_OK) {
        chk("revisión", "E = V en bornes = 12 V", s.s[0].val[f], 12.0);
        chk_cond("revisión", "con V en bornes, por el árbol", met == PROC_ARBOL);
    }

    /* Una pila en corto: I = E/r (con amperímetro ideal y con cable) */
    f = fuente_y(&c, EL_FUENTE, 12.0, EL_AMP, &k);
    c.el[f].aux.est = M_DATO; c.el[f].aux.v = 0.5;
    c.el[k].I.est = M_BUSCAR;
    red_extraer(&c, &r);
    comprueba("pila en corto con amperímetro", &c, &r, &s);
    if (s.estado == SOL_OK) chk("revisión", "corriente de corto: E/r = 24 A", fabs(s.s[0].I[k]), 24.0);
    c.el[f].aux.est = M_BUSCAR;
    c.el[k].I.est = M_DATO; c.el[k].I.v = 24.0;
    comprueba("r desde la corriente de corto", &c, &r, &s);
    if (s.estado == SOL_OK) chk("revisión", "r = E/Icc = 0,5 ohm", s.s[0].aux[f], 0.5);
    circ_init(&c, "corto");
    f = circ_elemento(&c, EL_FUENTE, 0, 1, DIR_ARR);
    c.el[f].val.est = M_DATO; c.el[f].val.v = 12.0;
    c.el[f].aux.est = M_DATO; c.el[f].aux.v = 0.5;
    c.el[f].I.est = M_BUSCAR;
    circ_cable(&c, 0, 0, DIR_DER); circ_cable(&c, 1, 0, DIR_ABA); circ_cable(&c, 1, 1, DIR_IZQ);
    red_extraer(&c, &r);
    comprueba("pila en corto con cable", &c, &r, &s);
    if (s.estado == SOL_OK) chk("revisión", "con un cable: 24 A", fabs(s.s[0].I[f]), 24.0);
    c.el[f].aux.est = M_LIBRE;                            /* ideal: imposible */
    red_extraer(&c, &r);
    chk_cond("revisión", "una fuente ideal en corto sigue siendo un error", r.error == RED_FUENTE_CORTO);

    /* J sin camino de vuelta: lazo abierto más allá, o un voltímetro ideal */
    circ_init(&c, "J abierta");
    f = circ_elemento(&c, EL_CORR, 0, 1, DIR_ARR);
    c.el[f].val.est = M_DATO; c.el[f].val.v = 2.0;
    k = circ_elemento(&c, EL_RES, 0, 0, DIR_DER);
    c.el[k].val.est = M_DATO; c.el[k].val.v = 5.0;
    k = circ_elemento(&c, EL_RES, 1, 1, DIR_IZQ);
    c.el[k].val.est = M_DATO; c.el[k].val.v = 5.0;
    red_extraer(&c, &r); resolver(&c, &r, &s);
    chk_cond("revisión", "J con el lazo abierto: sin camino de vuelta",
             r.error == RED_J_ABIERTA && s.elem == f);
    f = fuente_y(&c, EL_CORR, 2.0, EL_VOLT, &k);
    red_extraer(&c, &r);
    chk_cond("revisión", "J con un voltímetro ideal en serie: sin camino", r.error == RED_J_ABIERTA);
    c.el[k].val.est = M_DATO; c.el[k].val.v = 1e4;
    c.el[k].V.est = M_BUSCAR;
    red_extraer(&c, &r);
    comprueba("J con un voltímetro de 10 k", &c, &r, &s);
    if (s.estado == SOL_OK) chk("revisión", "con R interna sí: marca 20 kV", fabs(s.s[0].V[k]), 2e4);

    /* Dos J en serie: el circuito deja un potencial sin fijar (no hay datos) */
    circ_init(&c, "dos J");
    f = circ_elemento(&c, EL_CORR, 0, 1, DIR_ARR);
    c.el[f].val.est = M_DATO; c.el[f].val.v = 2.0;
    circ_cable(&c, 0, 0, DIR_DER);
    k = circ_elemento(&c, EL_CORR, 1, 0, DIR_ABA);
    c.el[k].val.est = M_DATO; c.el[k].val.v = 2.0;
    k2 = circ_elemento(&c, EL_RES, 1, 1, DIR_IZQ);
    c.el[k2].val.est = M_DATO; c.el[k2].val.v = 5.0;
    red_extraer(&c, &r); resolver(&c, &r, &s);
    sol_mensaje(&c, &r, &s, m, sizeof m);
    chk_cond("revisión", "dos J en serie: el mensaje no culpa a los datos",
             s.estado == SOL_INDET && strstr(m, "Los datos") == 0);
    c.el[k].val.v = 3.0;
    resolver(&c, &r, &s);
    sol_mensaje(&c, &r, &s, m, sizeof m);
    chk_cond("revisión", "dos J distintas en serie: imposible, sin culpar a los datos",
             s.estado == SOL_INCOMPAT && strstr(m, "Los datos") == 0);

    /* 32 elementos con una pila: no cabe la vista con la r, así que no se usa
     * el árbol (daría la r por 0) */
    circ_init(&rc, "32");
    nodos = 32;
    f = nuevo_el(EL_FUENTE, 0, 1);
    rc.el[f].val.est = M_DATO; rc.el[f].val.v = 46.5;
    rc.el[f].aux.est = M_DATO; rc.el[f].aux.v = 15.5;
    for (i = 0; i < 31; i++) {
        k = nuevo_el(EL_RES, 1 + i, i == 30 ? 0 : 2 + i);
        if (k >= 0) rc.el[k].val.v = 1.0;
    }
    cierra_red();
    met = comprueba("32 elementos con fuente real", &rc, &rr, &s);
    if (s.estado == SOL_OK) {
        chk("revisión", "32 elementos: I = 46,5/(15,5 + 31) = 1 A", fabs(s.s[0].I[f]), 1.0);
        chk_cond("revisión", "32 elementos con r: por Kirchhoff, no por el árbol sin r",
                 rc.nel == 32 && met == PROC_KIRCHHOFF);
    }

    /* La J de valor vacío: los pasos la calculan, y no la llaman E */
    circ_init(&rc, "J vacía");
    nodos = 2;
    f = nuevo_el(EL_CORR, 0, 1);
    rc.el[f].val.est = M_LIBRE;
    k = nuevo_el(EL_RES, 1, 0); rc.el[k].val.v = 6.0;
    rc.el[k].V.est = M_DATO; rc.el[k].V.v = 6.0;
    k2 = nuevo_el(EL_RES, 1, 0); rc.el[k2].val.v = 3.0;
    cierra_red();
    met = comprueba("J vacía", &rc, &rr, &s);
    if (s.estado == SOL_OK) {
        chk("revisión", "J = 3 A", fabs(s.s[0].val[f]), 3.0);
        chk_cond("revisión", "los pasos dan J", met == PROC_ARBOL && texto_en_pasos("J = I("));
        chk_cond("revisión", "y no dicen E de una J", !texto_en_pasos(") = E"));
    }

    /* La J vacía también cuando el árbol va por propagación (hay una R
     * desconocida): R1 buscada con su I, R2 = 3 con su V */
    circ_init(&rc, "J vacía, propagación");
    nodos = 2;
    f = nuevo_el(EL_CORR, 0, 1);
    rc.el[f].val.est = M_LIBRE;
    k = nuevo_el(EL_RES, 1, 0); rc.el[k].val.est = M_BUSCAR;
    rc.el[k].I.est = M_DATO; rc.el[k].I.v = 1.0;
    k2 = nuevo_el(EL_RES, 1, 0); rc.el[k2].val.v = 3.0;
    rc.el[k2].V.est = M_DATO; rc.el[k2].V.v = 6.0;
    cierra_red();
    met = comprueba("J vacía por propagación", &rc, &rr, &s);
    if (s.estado == SOL_OK) {
        chk("revisión", "R1 = 6 ohm y J = 3 A", s.s[0].val[k] + fabs(s.s[0].val[f]), 9.0);
        chk_cond("revisión", "propagando, los pasos también dan J", met == PROC_ARBOL && texto_en_pasos("J = I("));
    }

    /* Una R vacía que sale por Kirchhoff no es un amperímetro ideal */
    circ_init(&rc, "dos fuentes");
    nodos = 3;
    f = nuevo_el(EL_FUENTE, 0, 1); rc.el[f].val.est = M_DATO; rc.el[f].val.v = 10.0;
    k = nuevo_el(EL_RES, 1, 2); rc.el[k].val.est = M_LIBRE;
    rc.el[k].I.est = M_DATO; rc.el[k].I.v = 1.0;
    nuevo_el(EL_RES, 2, 0);
    k2 = nuevo_el(EL_FUENTE, 0, 2); rc.el[k2].val.est = M_DATO; rc.el[k2].val.v = 5.0;
    cierra_red();
    met = comprueba("R vacía por Kirchhoff", &rc, &rr, &s);
    if (s.estado == SOL_OK) {
        chk("revisión", "R vacía = 5 ohm", s.s[0].val[k], 5.0);
        chk_cond("revisión", "una R vacía cuenta como desconocida y no es un amperímetro",
                 met == PROC_KIRCHHOFF && sol_desconocidos(&rc, 0) == 1 && !texto_en_pasos("amperímetro"));
    }

    /* Una r negativa (de un archivo): el mensaje es de la r */
    f = lazo(&c, &r, 12.0, 2.0, 3.0);
    c.el[f].aux.est = M_DATO; c.el[f].aux.v = -1.0;
    red_extraer(&c, &r); resolver(&c, &r, &s);
    sol_mensaje(&c, &r, &s, m, sizeof m);
    chk_cond("revisión", "r negativa: el mensaje es de la r interna",
             s.estado == SOL_R_INVALIDA && strstr(m, "r interna") != 0 && strstr(m, "negativa") != 0);

    /* En el editor: la línea de estado cuenta la r buscada como el motor */
    {
        const Circuito *cu = ui_circuito();
        ui_init();
        TECLAS(T_ENTER, T_ENTER, T_ENTER);
        TECLAS(T_LETRA('E'), T_ARRIBA, T_DIG(1), T_DIG(2), T_ABAJO, T_DIG(2), T_ABAJO, T_INCOG, T_ENTER);
        TECLAS(T_LETRA('W'), T_DER, T_ESC);
        TECLAS(T_LETRA('R'), T_ABAJO, T_DIG(5), T_ENTER);
        TECLAS(T_LETRA('W'), T_IZQ, T_ESC);
        vuelca("revision_r_buscada");
        chk_cond("revisión", "E, I y r?: el editor dice listo",
                 cu->nel == 2 && cu->el[0].aux.est == M_BUSCAR && ve("listo"));
        TECLAS(T_IGUAL);
        chk_cond("revisión", "y se resuelve", ui_pantalla() == P_RESULTADO);
        for (i = 0; i < 4 && ui_pantalla() != P_EDITOR; i++) TECLAS(T_ESC);
        for (i = 0; i < 4 && ui_pantalla() == P_EDITOR; i++) TECLAS(T_ESC);
    }

    /* Practicar la r de una pila: se corrige contra la r, en ohmios */
    {
        int li, j = 0, bien = 0, total = 0;
        for (li = 0; li < N_LECCIONES; li++) {
            for (j = 0; j < LECCIONES[li].nej && LECCIONES[li].ej[j] != EJ_F_R; j++) ;
            if (j < LECCIONES[li].nej) break;
        }
        pruebas++;
        if (li == N_LECCIONES) falla("revisión", "EJ_F_R está en alguna lección", 0, 1);
        else {
            ui_init();
            TECLAS(T_ENTER, T_ABAJO, T_ENTER);             /* menú > Aprende */
            repite(T_ABAJO, li);
            TECLAS(T_ENTER, T_ENTER);                      /* la lección > ejemplos */
            repite(T_ABAJO, j);
            TECLAS(T_INCOG, T_ENTER);
            chk_cond("revisión", "práctica de la r: pregunta en ohmios",
                     ui_practica(0, 0) == 1 && ve("r(E1) = _ " S_OHM));
            teclea_numero(2.0);
            TECLAS(T_ENTER);
            ui_practica(&bien, &total);
            chk_cond("revisión", "la r buena (2 ohm) se da por buena", bien == 1);
            TECLAS(T_ESC, T_ENTER);
            if (ui_practica(0, 0) == 1) {
                teclea_numero(1.5);                        /* la I, no la r */
                TECLAS(T_ENTER);
                ui_practica(&bien, &total);
                chk_cond("revisión", "la I en lugar de la r no vale", bien == 0);
            }
            for (i = 0; i < 6; i++) TECLAS(T_ESC);
        }
    }

    ESP_T_REV
}

/* --------------------------------------------------------------------------
 *  26. El álgebra del emulador (LU) y las formas de onda
 * ----------------------------------------------------------------------- */
static LU lu_a;

/* Residuo relativo de A x = b con la matriz original m */
static double residuo_lu(double m[][LU_MAX], int n, const double *x, const double *b)
{
    double peor = 0.0, nb = 1e-300;
    int i, j;
    for (i = 0; i < n; i++) if (fabs(b[i]) > nb) nb = fabs(b[i]);
    for (i = 0; i < n; i++) {
        double s = -b[i], esc = fabs(b[i]);
        for (j = 0; j < n; j++) { s += m[i][j] * x[j]; esc += fabs(m[i][j] * x[j]); }
        if (fabs(s) / (esc + nb) > peor) peor = fabs(s) / (esc + nb);
    }
    return peor;
}

static void test_lu_ondas(void)
{
    static double m[LU_MAX][LU_MAX];
    static double b[LU_MAX], x[LU_MAX];
    int caso, n, i, j, malos = 0;
    double peor = 0.0;
    unsigned long o0, o1;

    printf("[26] Álgebra del emulador (LU) y formas de onda\n");

    /* Matrices al azar con la forma de las de un circuito: dispersas, con
     * coeficientes de escalas muy distintas (1e-9 .. 1e6) y filas de +-1 */
    for (caso = 0; caso < 400; caso++) {
        n = 1 + (int)azar(caso < 380 ? 40 : LU_MAX);
        lu_empieza(&lu_a, n);
        for (i = 0; i < n; i++) for (j = 0; j < n; j++) m[i][j] = 0.0;
        for (i = 0; i < n; i++) {
            double g = pow(10.0, -9.0 + 15.0 * (double)azar(1000) / 999.0);
            int vecinos = 1 + (int)azar(3), v;
            m[i][i] += g * (double)(vecinos + 1);
            for (v = 0; v < vecinos; v++) {
                j = (int)azar((unsigned)n);
                if (j == i) continue;
                m[i][j] -= g; m[j][j] += g * 0.5; m[j][i] -= g * 0.5;
            }
            if (azar(5) == 0 && n > 1) {                  /* una fila de incidencia */
                j = (int)azar((unsigned)n);
                if (j != i) { m[i][j] += 1.0; m[j][i] += 1.0; }
            }
            b[i] = (double)azar(2001) - 1000.0;
        }
        for (i = 0; i < n; i++) for (j = 0; j < n; j++) lu_suma(&lu_a, i, j, m[i][j]);
        if (!lu_factoriza(&lu_a)) { malos++; continue; }
        lu_resuelve(&lu_a, b, x);
        {
            double r = residuo_lu(m, n, x, b);
            if (r > peor) peor = r;
        }
    }
    pruebas += 2;
    if (peor > 1e-9) falla("lu", "residuo de A x = b en matrices de circuito al azar", peor, 0.0);
    if (malos > 20) falla("lu", "casi ninguna de estas matrices es singular", malos, 20);

    /* Singular: dos filas iguales; y una fila vacía */
    lu_empieza(&lu_a, 3);
    lu_suma(&lu_a, 0, 0, 1.0); lu_suma(&lu_a, 0, 1, 2.0);
    lu_suma(&lu_a, 1, 0, 2.0); lu_suma(&lu_a, 1, 1, 4.0);
    lu_suma(&lu_a, 2, 2, 1.0);
    chk_cond("lu", "singular: dos filas proporcionales", !lu_factoriza(&lu_a) && lu_a.col_mala >= 0);
    lu_empieza(&lu_a, 2);
    lu_suma(&lu_a, 0, 0, 1.0);
    chk_cond("lu", "singular: una fila vacía", !lu_factoriza(&lu_a));
    {
        /* la tercera fila es la suma de las otras dos, con decimales que no
         * son exactos: la eliminación deja un residuo de redondeo que el
         * umbral relativo tiene que dar por cero */
        static const double F[3][3] = { { 0.1, 0.7, 0.3 }, { 0.2, 0.4, 0.9 }, { 0.3, 1.1, 1.2 } };
        int ii, jj;
        lu_empieza(&lu_a, 3);
        for (ii = 0; ii < 3; ii++) for (jj = 0; jj < 3; jj++) lu_suma(&lu_a, ii, jj, F[ii][jj]);
        chk_cond("lu", "casi singular por redondeo: singular", !lu_factoriza(&lu_a));
    }
    lu_empieza(&lu_a, 2);
    lu_fuera_reset();
    lu_suma(&lu_a, 0, 0, 1e-9); lu_suma(&lu_a, 1, 1, 1e6); lu_suma(&lu_a, -1, 0, 5.0); lu_suma(&lu_a, 0, 7, 5.0);
    chk_cond("lu", "el -1 (referencia) se omite y el 7 (fuera) se cuenta", lu_fuera() == 1);
    lu_fuera_reset();
    b[0] = 2e-9; b[1] = 3e6;
    chk_cond("lu", "1 GOhm y 1 uOhm juntos no son singulares", lu_factoriza(&lu_a));
    lu_resuelve(&lu_a, b, x);
    chk("lu", "x0 = 2", x[0], 2.0);
    chk("lu", "x1 = 3", x[1], 3.0);

    /* La sustitución de una red dispersa cuesta mucho menos que n^2 */
    n = 60;
    lu_empieza(&lu_a, n);
    for (i = 0; i < n; i++) {                          /* una escalera de resistencias */
        lu_suma(&lu_a, i, i, 2.0);
        if (i > 0) { lu_suma(&lu_a, i, i - 1, -1.0); lu_suma(&lu_a, i - 1, i, -1.0); }
        b[i] = 1.0;
    }
    lu_factoriza(&lu_a);
    o0 = lu_ops();
    lu_resuelve(&lu_a, b, b);                          /* b y x el mismo vector */
    o1 = lu_ops();
    pruebas++;
    if (o1 - o0 > (unsigned long)(4 * n)) falla("lu", "sustituir una escalera cuesta O(n)", (double)(o1 - o0), 4.0 * n);
    chk("lu", "escalera: x0 = n/2", b[0], n / 2.0);

    /* Formas de onda */
    {
        double sq[ONDA_PAR] = { 1.0, 1.0, 30.0, 0.0, 0.0 };
        double tr[ONDA_PAR] = { 1.0, 1.0, 50.0, 0.0, 0.0 };
        double sn[ONDA_PAR] = { 2.0, 50.0, 90.0, 1.0, 0.0 };
        double pu[ONDA_PAR] = { 0.0, 5.0, 1.0, 2.0, 0.0 };
        chk("ondas", "cuadrada: arriba al empezar", onda_valor(ONDA_CUADRADA, sq, 0.0), 1.0);
        chk("ondas", "cuadrada: arriba justo antes del flanco", onda_valor(ONDA_CUADRADA, sq, 0.29), 1.0);
        chk("ondas", "cuadrada: en el flanco ya vale lo nuevo", onda_valor(ONDA_CUADRADA, sq, 0.3), -1.0);
        chk("ondas", "cuadrada: casi en el flanco cuenta como el flanco", onda_valor(ONDA_CUADRADA, sq, 0.3 - 1e-13), -1.0);
        chk("ondas", "cuadrada: el periodo siguiente", onda_valor(ONDA_CUADRADA, sq, 1.0), 1.0);
        chk("ondas", "cuadrada: 3·0,1 cae en el flanco", onda_valor(ONDA_CUADRADA, sq, 3.0 * 0.1), -1.0);
        chk("ondas", "cuadrada: evento tras 0", onda_evento(ONDA_CUADRADA, sq, 0.0), 0.3);
        chk("ondas", "cuadrada: evento tras 0,3", onda_evento(ONDA_CUADRADA, sq, 0.3), 1.0);
        chk("ondas", "cuadrada: evento tras 1", onda_evento(ONDA_CUADRADA, sq, 1.0), 1.3);
        chk("ondas", "cuadrada: periodo", onda_periodo(ONDA_CUADRADA, sq), 1.0);
        chk("ondas", "triangular: abajo en 0", onda_valor(ONDA_TRIANGULAR, tr, 0.0), -1.0);
        chk("ondas", "triangular: 0 en T/4", onda_valor(ONDA_TRIANGULAR, tr, 0.25), 0.0);
        chk("ondas", "triangular: arriba en T/2", onda_valor(ONDA_TRIANGULAR, tr, 0.5), 1.0);
        chk("ondas", "triangular: 0 en 3T/4", onda_valor(ONDA_TRIANGULAR, tr, 0.75), 0.0);
        chk("ondas", "triangular: vértice siguiente", onda_evento(ONDA_TRIANGULAR, tr, 0.1), 0.5);
        chk("ondas", "senoidal: nivel + A·sen(90°)", onda_valor(ONDA_SENO, sn, 0.0), 3.0);
        chk("ondas", "senoidal: medio periodo después", onda_valor(ONDA_SENO, sn, 0.01), -1.0);
        pruebas++;
        if (onda_evento(ONDA_SENO, sn, 0.0) < 1e299) falla("ondas", "la senoidal no tiene eventos", 0, 1);
        chk("ondas", "pulso: V1 antes", onda_valor(ONDA_PULSO, pu, 0.5), 0.0);
        chk("ondas", "pulso: V2 en td", onda_valor(ONDA_PULSO, pu, 1.0), 5.0);
        chk("ondas", "pulso: V2 justo antes de acabar", onda_valor(ONDA_PULSO, pu, 2.9), 5.0);
        chk("ondas", "pulso: V1 al acabar", onda_valor(ONDA_PULSO, pu, 3.0), 0.0);
        chk("ondas", "pulso: evento td", onda_evento(ONDA_PULSO, pu, 0.0), 1.0);
        chk("ondas", "pulso: evento del fin", onda_evento(ONDA_PULSO, pu, 1.0), 3.0);
        pruebas++;
        if (onda_evento(ONDA_PULSO, pu, 3.0) < 1e299) falla("ondas", "un pulso solo no tiene más eventos", 0, 1);
        pu[4] = 4.0;                                   /* periódico */
        chk("ondas", "pulso periódico: V2 en td + periodo", onda_valor(ONDA_PULSO, pu, 5.0), 5.0);
        chk("ondas", "pulso periódico: V1 en td + periodo + ancho", onda_valor(ONDA_PULSO, pu, 7.0), 0.0);
        chk("ondas", "pulso periódico: evento tras 3", onda_evento(ONDA_PULSO, pu, 3.0), 5.0);
        pu[3] = 0.0;                                   /* escalón */
        chk("ondas", "escalón: V2 para siempre", onda_valor(ONDA_PULSO, pu, 100.0), 5.0);
        pruebas++;
        if (onda_evento(ONDA_PULSO, pu, 1.5) < 1e299) falla("ondas", "el escalón no tiene más eventos", 0, 1);
    }
}

/* --------------------------------------------------------------------------
 *  27. El punto de operación del emulador contra la calculadora
 *
 *  Dos motores independientes: la tabla (resuelve.c, que despeja) y el
 *  análisis nodal modificado (sim.c, que emula). Sin incógnitas, tienen que
 *  dar lo mismo en cada red; y cuando la tabla dice que no hay solución
 *  única, el emulador también.
 * ----------------------------------------------------------------------- */
static int n_op_igual, n_op_singular;

static void compara_op(const char *q, const Circuito *c, const Red *r)
{
    static Solucion s;
    static Valores v;
    SimDiag d;
    int k, e_sim, e_tab;
    e_tab = resolver(c, r, &s);
    e_sim = sim_punto_operacion(c, r, &v, &d);
    if (e_tab == SOL_OK && s.nsol == 1) {
        double escv = 1e-30, esci = 1e-30;
        pruebas++;
        if (e_sim != SIM_OK) { char m[160]; sim_mensaje_circ(c, 0, &d, m, sizeof m); falla(q, m, e_sim, SIM_OK); return; }
        for (k = 0; k < r->nel; k++) {
            if (fabs(s.s[0].V[k]) > escv) escv = fabs(s.s[0].V[k]);
            if (fabs(s.s[0].I[k]) > esci) esci = fabs(s.s[0].I[k]);
        }
        for (k = 0; k < r->nel; k++) {
            char det[64];
            pruebas += 2;
            if (fabs(v.V[k] - s.s[0].V[k]) > 1e-9 * escv) {
                sprintf(det, "V(%s) emulador = tabla", c->el[k].etiq); falla(q, det, v.V[k], s.s[0].V[k]);
            }
            if (fabs(v.I[k] - s.s[0].I[k]) > 1e-9 * esci) {
                sprintf(det, "I(%s) emulador = tabla", c->el[k].etiq); falla(q, det, v.I[k], s.s[0].I[k]);
            }
        }
        for (k = 0; k < r->nn; k++) {
            pruebas++;
            if (fabs(v.phi[k] - s.s[0].phi[k]) > 1e-9 * escv) falla(q, "potencial emulador = tabla", v.phi[k], s.s[0].phi[k]);
        }
        chk(q, "residuo de Kirchhoff del emulador", sol_residuo(c, r, &v), 0.0);
        n_op_igual++;
    } else if (e_tab == SOL_INDET) {
        /* el emulador lo dice con su propio nombre (un lazo de fuentes, una J
         * sin vuelta), o resuelve con una regla física y la avisa: un trozo
         * que flota, el reparto de carga de C en serie, el de flujo de L en
         * paralelo */
        chk_cond(q, "indeterminado en la tabla: el emulador da un error o avisa de su regla",
                 e_sim == SIM_SINGULAR || e_sim == SIM_LAZO_V || e_sim == SIM_CORTE_J ||
                 (e_sim == SIM_OK && (d.avisos & (SIM_AV_FLOTA | SIM_AV_CARGA | SIM_AV_FLUJO))));
        n_op_singular++;
    } else if (e_tab == SOL_INCOMPAT) {
        /* sin régimen de CC: el emulador da el error o su rescate (una C que
         * empieza descargada, una L sin corriente) */
        chk_cond(q, "incompatible en la tabla: el emulador da un error o su rescate",
                 e_sim == SIM_SINGULAR || e_sim == SIM_LAZO_V || e_sim == SIM_CORTE_J ||
                 (e_sim == SIM_OK && (d.avisos & (SIM_AV_C_DESC | SIM_AV_L_SIN_I | SIM_AV_CI_SALTA))));
        n_op_singular++;
    }
}

static void test_sim_op(void)
{
    int caso, f, k, a, b, i;
    char q[48];

    printf("[27] Punto de operación: emulador contra calculadora\n");
    n_op_igual = n_op_singular = 0;
    for (caso = 0; caso < 2100; caso++) {
        int tipo = caso % 7;
        f = fuente_sp(1.0 + azar(200));
        if (tipo == 1) { rc.el[f].aux.est = M_DATO; rc.el[f].aux.v = 0.1 * (double)(1 + azar(50)); }
        if (tipo == 2) { rc.el[f].tipo = EL_CORR; rc.el[f].etiq[0] = 'J'; rc.el[f].val.v = 0.5 * (double)(1 + azar(20)); }
        if (tipo >= 3 && nodos > 2) {                  /* puentes y más fuentes */
            for (i = 0; i < 1 + (int)azar(3) && rc.nel < MAX_EL; i++) {
                a = (int)azar((unsigned)nodos); b = (int)azar((unsigned)nodos);
                if (a == b) continue;
                k = nuevo_el(azar(3) == 0 ? EL_FUENTE : azar(2) ? EL_CORR : EL_RES, a, b);
                if (k >= 0 && rc.el[k].tipo != EL_RES) { rc.el[k].val.est = M_DATO; rc.el[k].val.v = 1.0 + azar(30); }
                if (k >= 0 && rc.el[k].tipo == EL_FUENTE && azar(2)) { rc.el[k].aux.est = M_DATO; rc.el[k].aux.v = 0.5 + azar(5); }
            }
        }
        if (tipo == 6 && nodos > 2) {                  /* C, L y S (V0 e I0 vacías) */
            for (i = 0; i < 1 + (int)azar(3) && rc.nel < MAX_EL; i++) {
                int t = (int)azar(3);
                a = (int)azar((unsigned)nodos); b = (int)azar((unsigned)nodos);
                if (a == b) continue;
                k = nuevo_el(t == 0 ? EL_COND : t == 1 ? EL_BOB : EL_INTER, a, b);
                if (k < 0) continue;
                if (t < 2) { rc.el[k].val.est = M_DATO; rc.el[k].val.v = 1e-6 * (1 + azar(100)); }
                else rc.el[k].variante = (unsigned char)azar(2);
            }
        }
        if (tipo == 5 && nodos > 2 && rc.nel < MAX_EL - 1) {   /* instrumentos */
            a = (int)azar((unsigned)nodos); b = (int)azar((unsigned)nodos);
            if (a != b) {
                k = nuevo_el(EL_VOLT, a, b);
                if (azar(2)) { rc.el[k].val.est = M_DATO; rc.el[k].val.v = 1e3 * (1 + azar(100)); }
            }
            k = 1 + (int)azar((unsigned)(rc.nel - 1));
            if (rc.el[k].tipo == EL_RES && rc.nel < MAX_EL) {
                int nuevo = nodos++, ka = nuevo_el(EL_AMP, nuevo, rr.b[k]);
                rr.b[k] = nuevo;
                if (ka >= 0 && azar(2)) { rc.el[ka].val.est = M_DATO; rc.el[ka].val.v = 0.1 * (1 + azar(10)); }
            }
        }
        cierra_red();
        {
            int corto = 0;
            for (k = 0; k < rc.nel; k++) if (rr.a[k] == rr.b[k]) corto = 1;
            if (corto) continue;                       /* un corto lo rechaza el dibujo */
        }
        sprintf(q, "op %d", caso);
        compara_op(q, &rc, &rr);
    }
    printf("    %d redes iguales en los dos motores, %d sin solución única en los dos\n",
           n_op_igual, n_op_singular);
    pruebas++;
    if (n_op_igual < 1000) falla("op", "se compararon pocas redes", n_op_igual, 1000);
}

/* --------------------------------------------------------------------------
 *  28. El transitorio contra soluciones exactas
 * ----------------------------------------------------------------------- */
static SimRed sr;
static SimAjustes saj;

static void sr_init(int nn)
{
    memset(&sr, 0, sizeof sr);
    memset(&saj, 0, sizeof saj);
    sr.nn = nn;
    sr.ref[0] = 1;
}

static int sr_el(int tipo, int a, int b, double val)
{
    SimEl *e = &sr.el[sr.nel];
    memset(e, 0, sizeof *e);
    e->tipo = (unsigned char)tipo; e->a = (short)a; e->b = (short)b; e->val = val;
    e->k = -1; e->t1 = e->t2 = -1.0;
    return sr.nel++;
}

/* Fuente de tensión ideal: escalón de 0 a V en t = 0 (o continua) */
static int sr_escalon(int a, int b, double V)
{
    int k = sr_el(SE_E, a, b, 0.0);
    sr.el[k].forma = ONDA_PULSO;
    sr.el[k].p[0] = 0.0; sr.el[k].p[1] = V; sr.el[k].p[2] = 0.0; sr.el[k].p[3] = 0.0; sr.el[k].p[4] = 0.0;
    return k;
}

static int sr_cc(int a, int b, double V)
{
    int k = sr_el(SE_E, a, b, 0.0);
    sr.el[k].forma = ONDA_CC; sr.el[k].p[0] = V;
    return k;
}

/* Canal del emulador: un nodo, o la I o la V de un elemento. La V de una
 * fuente con su convenio (phi(b) - phi(a)): sg = -1 */
static void canal_de(SimAjustes *aj, const SimRed *s, int c, int tipo, int id)
{
    SimCanal *ch = &aj->canal[c];
    memset(ch, 0, sizeof *ch);
    ch->tipo = (unsigned char)tipo;
    ch->elem = ch->nodo = -1;
    ch->sg = 1;
    if (tipo == CANAL_NODO) ch->nodo = (signed char)id;
    else if (tipo != CANAL_NADA) {
        ch->elem = (signed char)id;
        if (tipo == CANAL_V && (s->el[id].tipo == SE_E || s->el[id].tipo == SE_J)) ch->sg = -1;
    }
}

static void canal(int c, int tipo, int id) { canal_de(&saj, &sr, c, tipo, id); }

static void emula(const char *q)
{
    int e = sim_emular(&sr, &saj);
    pruebas++;
    if (e != SIM_OK) { char m[200]; sim_mensaje(&sr, &sim_res()->d, m, sizeof m); falla(q, m, e, SIM_OK); }
}

/* Error relativo a una escala */
static void chk_t(const char *q, const char *det, double a, double b, double esc, double tol)
{
    pruebas++;
    if (!(fabs(a - b) <= tol * esc)) falla(q, det, a, b);
}

static void test_transitorio(void)
{
    const SimRes *r;
    SimMedidas me;
    int kC, kR, kL, kE, kS, i;
    double tau = 1e-3, t, e1, e2;

    printf("[28] Transitorio contra soluciones exactas\n");

    /* ---- Carga de un RC con un escalón de 10 V: v = 10 (1 - e^(-t/tau)) ---- */
    sr_init(3);
    kE = sr_escalon(0, 1, 10.0);
    kR = sr_el(SE_R, 1, 2, 1e3);
    kC = sr_el(SE_C, 2, 0, 1e-6);
    canal(0, CANAL_V, kC); canal(1, CANAL_V, kE);
    emula("RC carga");
    r = sim_res();
    chk_t("RC carga", "tau hallada = RC", r->tau_max, tau, tau, 1e-6);
    chk_t("RC carga", "ventana automática = 5 tau", r->t_fin, 5e-3, 5e-3, 1e-9);
    chk_t("RC carga", "v(0-) = 0 (OP antes del escalón)", sim_reg()->y[0][0], 0.0, 10.0, 1e-12);
    chk_t("RC carga", "v(0+) = 0 (C no salta)", sim_canal_en(0, 0.0), 0.0, 10.0, 1e-7);
    for (i = 1; i <= 4; i++) {
        t = i * tau;
        chk_t("RC carga", "v(t) = 10(1 - e^-t/tau)", sim_canal_en(0, t), 10.0 * (1.0 - exp(-t / tau)), 10.0, 2e-6);
    }
    chk_t("RC carga", "energía en R = ½CE²(1 - e^-10)", r->W[kR], 0.5e-6 * 100.0 * (1.0 - exp(-10.0)), 5e-5, 1e-4);
    chk_t("RC carga", "energía guardada en C", r->Efin[kC], 0.5e-6 * pow(10.0 * (1.0 - exp(-5.0)), 2.0), 5e-5, 1e-5);
    chk_t("RC carga", "la fuente es un escalón: 10 V en 1 ms", sim_canal_en(1, 1e-3), 10.0, 10.0, 1e-12);

    /* ---- Descarga: C con V0 = 10 V sobre R ---- */
    sr_init(2);
    kC = sr_el(SE_C, 1, 0, 1e-6);
    sr.el[kC].ci = 1; sr.el[kC].x0 = 10.0;
    kR = sr_el(SE_R, 1, 0, 1e3);
    canal(0, CANAL_V, kC);
    emula("RC descarga");
    chk_t("RC descarga", "v(0) = V0", sim_canal_en(0, 0.0), 10.0, 10.0, 1e-12);
    chk_t("RC descarga", "v(2 ms) = 10 e^-2", sim_canal_en(0, 2e-3), 10.0 * exp(-2.0), 10.0, 2e-6);

    /* ---- RL: i = (E/R)(1 - e^(-tR/L)) ---- */
    sr_init(3);
    sr_escalon(0, 1, 10.0);
    sr_el(SE_R, 1, 2, 10.0);
    kL = sr_el(SE_L, 2, 0, 10e-3);
    canal(0, CANAL_I, kL);
    emula("RL");
    chk_t("RL", "tau = L/R", sim_res()->tau_max, 1e-3, 1e-3, 1e-6);
    for (i = 1; i <= 3; i++) {
        t = i * 1e-3;
        chk_t("RL", "i(t) = (E/R)(1 - e^-t/tau)", sim_canal_en(0, t), 1.0 - exp(-t / 1e-3), 1.0, 2e-6);
    }

    /* ---- RLC serie, respuesta al escalón de 1 V en v_C ---- */
    {
        static const double RR[3] = { 20.0, 200.0, 1000.0 };
        double L = 10e-3, C = 1e-6, w0 = 1.0 / sqrt(L * C);
        int caso;
        for (caso = 0; caso < 3; caso++) {
            double R = RR[caso], al = R / (2.0 * L), peor = 0.0;
            char q[32];
            sprintf(q, "RLC %s", caso == 0 ? "sub" : caso == 1 ? "crítico" : "sobre");
            sr_init(4);
            sr_escalon(0, 1, 1.0);
            sr_el(SE_R, 1, 2, R);
            sr_el(SE_L, 2, 3, L);
            kC = sr_el(SE_C, 3, 0, C);
            canal(0, CANAL_V, kC);
            saj.t_fin = 2e-3;
            emula(q);
            for (i = 1; i <= 40; i++) {
                double v, ex;
                t = i * 5e-5;
                if (caso == 0) {
                    double wd = sqrt(w0 * w0 - al * al);
                    ex = 1.0 - exp(-al * t) * (cos(wd * t) + al / wd * sin(wd * t));
                } else if (caso == 1) ex = 1.0 - exp(-al * t) * (1.0 + al * t);
                else {
                    double d = sqrt(al * al - w0 * w0), s1 = -al + d, s2 = -al - d;
                    ex = 1.0 + (s2 * exp(s1 * t) - s1 * exp(s2 * t)) / (s1 - s2);
                }
                v = sim_canal_en(0, t);
                if (fabs(v - ex) > peor) peor = fabs(v - ex);
            }
            chk_t(q, "v_C(t) exacta (40 instantes)", peor, 0.0, 1.0, 1e-4);
            if (caso == 0) {
                double wd = sqrt(w0 * w0 - al * al);
                chk_t(q, "periodo de la oscilación = 2 pi / wd", sim_res()->t_osc, 2.0 * PI_T / wd, 2.0 * PI_T / wd, 1e-6);
                chk_t(q, "tau = 2L/R", sim_res()->tau_max, 1.0 / al, 1.0 / al, 1e-6);
            }
        }
    }

    /* ---- LC ideal: v = cos(w t), energía constante ---- */
    {
        double L = 1e-3, C = 1e-6, w = 1.0 / sqrt(L * C);
        const SimRes *rr2;
        sr_init(2);
        kC = sr_el(SE_C, 1, 0, C);
        sr.el[kC].ci = 1; sr.el[kC].x0 = 1.0;
        kL = sr_el(SE_L, 1, 0, L);
        canal(0, CANAL_V, kC); canal(1, CANAL_I, kL);
        emula("LC");
        rr2 = sim_res();
        chk_t("LC", "periodo = 2 pi sqrt(LC)", rr2->t_osc, 2.0 * PI_T * sqrt(L * C), 2.0 * PI_T * sqrt(L * C), 1e-6);
        chk_t("LC", "v(t) = cos(w t) en 1 periodo", sim_canal_en(0, 2.0 * PI_T / w), 1.0, 1.0, 1e-3);
        /* el trapecio conserva la energía de un LC; solo el arranque de
         * Euler en t = 0 (dos medios pasos) la amortigua un poco */
        chk_t("LC", "energía al final = ½CV0²", rr2->Efin[kC] + rr2->Efin[kL], 0.5 * C, 0.5 * C, 1e-3);
    }

    /* ---- LC con V0 e I0 dados: sin evento en 0, la energía se conserva exacta
     *      (el trapecio es simpléctico en un sistema lineal) ---- */
    {
        double L = 10e-3, C = 1e-6, Ei = 0.5 * C * 25.0, peor = 0.0;
        const double *tt, *yv, *yi;
        sr_init(2);
        kC = sr_el(SE_C, 1, 0, C); sr.el[kC].ci = 1; sr.el[kC].x0 = 5.0;
        kL = sr_el(SE_L, 1, 0, L); sr.el[kL].ci = 1; sr.el[kL].x0 = 0.0;
        canal(0, CANAL_V, kC); canal(1, CANAL_I, kL);
        saj.t_fin = 50.0 / 1591.549430918953;
        saj.h = saj.t_fin / 10000.0;
        emula("LC con V0 e I0");
        tt = sim_reg()->t; yv = sim_reg()->y[0]; yi = sim_reg()->y[1];
        for (i = 0; i < sim_reg()->n; i++) {
            double E = 0.5 * C * yv[i] * yv[i] + 0.5 * L * yi[i] * yi[i];
            if (fabs(E - Ei) > peor) peor = fabs(E - Ei);
        }
        (void)tt;
        chk_t("LC con V0 e I0", "energía constante (1e-12) en 50 periodos", peor, 0.0, Ei, 1e-12);
        sim_medir(0, 0.0, saj.t_fin, &me);
        {
            /* el trapecio oscila a w_TR = (2/h)·atan(w h/2): (w h)²/12 menos
             * (8e-5 con 200 pasos por periodo); la medida tiene que dar esa */
            double w = 2.0 * PI_T * 1591.549430918953, f_tr = (2.0 / saj.h) * atan(w * saj.h / 2.0) / (2.0 * PI_T);
            chk_t("LC con V0 e I0", "frecuencia medida = la del trapecio", me.frec, f_tr, f_tr, 1e-6);
            chk_t("LC con V0 e I0", "y a 1e-4 de la exacta, 1591,549 Hz", me.frec, 1591.549430918953, 1591.549430918953, 1e-4);
        }
    }

    /* ---- El emulador ES el trapecio: RC con E continua y V0 = 0 (sin evento
     *      en 0): v_n = a·v_{n-1} + b·E, a = (1-x/2)/(1+x/2), b = x/(1+x/2) ---- */
    {
        double h = 20e-6, x = h / 1e-3, a = (1.0 - x / 2.0) / (1.0 + x / 2.0), b = x / (1.0 + x / 2.0), v = 0.0, peor = 0.0;
        const double *yv;
        sr_init(3);
        sr_cc(0, 1, 10.0);
        sr_el(SE_R, 1, 2, 1e3);
        kC = sr_el(SE_C, 2, 0, 1e-6); sr.el[kC].ci = 1; sr.el[kC].x0 = 0.0;
        canal(0, CANAL_V, kC);
        saj.t_fin = 1e-3; saj.h = h;
        emula("trapecio exacto");
        yv = sim_reg()->y[0];
        for (i = 1; i < sim_reg()->n; i++) {
            v = a * v + b * 10.0;
            if (fabs(yv[i] - v) > peor) peor = fabs(yv[i] - v);
        }
        chk_t("trapecio exacto", "v_n = a v_(n-1) + b E en cada paso", peor, 0.0, 10.0, 1e-12);
        chk_cond("trapecio exacto", "50 pasos, sin arranque", sim_reg()->n == 51);
    }

    /* ---- Interruptor que se cierra en 1 ms: carga de un RC desde entonces ---- */
    sr_init(4);
    sr_cc(0, 1, 10.0);
    kS = sr_el(SE_S, 1, 2, 0.0);
    sr.el[kS].cerrado0 = 0; sr.el[kS].t1 = 1e-3;
    sr_el(SE_R, 2, 3, 1e3);
    kC = sr_el(SE_C, 3, 0, 1e-6);
    canal(0, CANAL_V, kC); canal(1, CANAL_I, kS);
    saj.t_fin = 5e-3;
    emula("interruptor");
    chk_t("interruptor", "antes de cerrar: 0 V", sim_canal_en(0, 0.9e-3), 0.0, 10.0, 1e-12);
    chk_t("interruptor", "1 ms después de cerrar: 6,32 V", sim_canal_en(0, 2e-3), 10.0 * (1.0 - exp(-1.0)), 10.0, 2e-6);
    chk_t("interruptor", "después de cerrar: i = (E/R) e^-(t-t1)/tau", sim_canal_en(1, 1.5e-3), 1e-2 * exp(-0.5), 1e-2, 2e-5);

    /* ---- Senoidal sobre un RC en su frecuencia de corte: amplitud 1/sqrt 2 ---- */
    sr_init(3);
    kE = sr_el(SE_E, 0, 1, 0.0);
    sr.el[kE].forma = ONDA_SENO; sr.el[kE].p[0] = 1.0; sr.el[kE].p[1] = 1e3;
    sr_el(SE_R, 1, 2, 1e3);
    kC = sr_el(SE_C, 2, 0, 1.0 / (2.0 * PI_T * 1e3 * 1e3));
    canal(0, CANAL_V, kC); canal(1, CANAL_V, kE);
    saj.t_fin = 20e-3; saj.h = 1e-3 / 400.0;
    emula("seno RC");
    sim_medir(0, 15e-3, 20e-3, &me);
    chk_t("seno RC", "amplitud en fc = 1/sqrt 2", 0.5 * me.pp, 1.0 / sqrt(2.0), 1.0, 1e-4);
    chk_t("seno RC", "eficaz = amplitud/sqrt 2", me.eficaz, 0.5, 1.0, 1e-4);
    chk_t("seno RC", "media 0", me.media, 0.0, 1.0, 1e-4);
    chk_t("seno RC", "frecuencia medida 1 kHz", me.frec, 1e3, 1e3, 1e-4);
    sim_medir(1, 15e-3, 20e-3, &me);
    chk_t("seno RC", "la fuente: eficaz 1/sqrt 2", me.eficaz, 1.0 / sqrt(2.0), 1.0, 1e-5);

    /* ---- C sobre una E senoidal ideal (1 kHz, 10 V de pico, 45°) con 1k en
     *      paralelo: i_C = C·dE/dt. El error del trapecio en i_C no se amortigua
     *      (alterna de signo), así que depende de la historia que deja el
     *      arranque: con CDA4+BDF2 es de orden 2 (con CDA2 sería de orden 1) ---- */
    {
        static const int NP[3] = { 50, 100, 400 };
        static const double TOLC[3] = { 1.5e-3, 3.6e-4, 2.5e-5 };
        double C = 1e-6, w = 2.0 * PI_T * 1e3, amp = C * 10.0 * w;
        int c3;
        for (c3 = 0; c3 < 3; c3++) {
            const double *tt, *yy;
            double peor = 0.0;
            char q[40];
            sr_init(2);
            kE = sr_el(SE_E, 0, 1, 0.0);
            sr.el[kE].forma = ONDA_SENO; sr.el[kE].p[0] = 10.0; sr.el[kE].p[1] = 1e3; sr.el[kE].p[2] = 45.0;
            kC = sr_el(SE_C, 1, 0, C);
            sr_el(SE_R, 1, 0, 1e3);
            canal(0, CANAL_I, kC);
            saj.t_fin = 6e-3; saj.h = 1e-3 / NP[c3];
            sprintf(q, "C sobre seno, N = %d", NP[c3]);
            emula(q);
            tt = sim_reg()->t; yy = sim_reg()->y[0];
            for (i = 0; i < sim_reg()->n; i++) {
                double ex;
                if (tt[i] < 3e-3) continue;
                ex = amp * cos(w * tt[i] + PI_T / 4.0);
                if (fabs(yy[i] - ex) > peor) peor = fabs(yy[i] - ex);
            }
            chk_t(q, "error relativo de i_C desde 3T (CDA4, orden 2)", peor / amp, 0.0, 1.0, TOLC[c3]);
        }
    }

    /* ---- Cuadrada sobre un RC con T = tau: pico = A·tanh(T/4tau) ---- */
    sr_init(3);
    kE = sr_el(SE_E, 0, 1, 0.0);
    sr.el[kE].forma = ONDA_CUADRADA; sr.el[kE].p[0] = 1.0; sr.el[kE].p[1] = 1e3; sr.el[kE].p[2] = 50.0;
    sr_el(SE_R, 1, 2, 1e3);
    kC = sr_el(SE_C, 2, 0, 1e-6);
    canal(0, CANAL_V, kC); canal(1, CANAL_V, kE);
    saj.t_fin = 20e-3;
    emula("cuadrada RC");
    sim_medir(0, 15e-3, 20e-3, &me);
    chk_t("cuadrada RC", "pico en régimen = tanh(1/4)", me.max, tanh(0.25), 1.0, 1e-4);
    chk_t("cuadrada RC", "valle = -tanh(1/4)", me.min, -tanh(0.25), 1.0, 1e-4);
    sim_medir(1, 15e-3, 20e-3, &me);
    chk_t("cuadrada RC", "la fuente: 1 kHz", me.frec, 1e3, 1e3, 1e-6);
    chk_t("cuadrada RC", "la fuente: eficaz 1", me.eficaz, 1.0, 1.0, 1e-6);

    /* ---- Condensadores en serie: el del medio flota; reparto de carga ---- */
    sr_init(4);
    sr_escalon(0, 1, 3.0);
    sr_el(SE_R, 1, 2, 1e3);
    kC = sr_el(SE_C, 2, 3, 1e-6);
    i = sr_el(SE_C, 3, 0, 2e-6);
    canal(0, CANAL_V, kC); canal(1, CANAL_V, i);
    saj.t_fin = 10e-3;
    emula("C en serie");
    chk_cond("C en serie", "avisa del reparto de carga", (sim_res()->d.avisos & SIM_AV_CARGA) != 0);
    chk_t("C en serie", "C1 acaba con 2 V", sim_canal_en(0, 10e-3), 2.0, 3.0, 1e-4);
    chk_t("C en serie", "C2 acaba con 1 V", sim_canal_en(1, 10e-3), 1.0, 3.0, 1e-4);

    /* ---- J que carga un C sola: no hay CC; empieza descargado; rampa ---- */
    sr_init(2);
    i = sr_el(SE_J, 0, 1, 0.0);
    sr.el[i].forma = ONDA_CC; sr.el[i].p[0] = 1e-3;
    kC = sr_el(SE_C, 1, 0, 1e-6);
    canal(0, CANAL_V, kC);
    saj.t_fin = 1e-3;
    emula("J y C");
    chk_cond("J y C", "avisa: el C empieza descargado", (sim_res()->d.avisos & SIM_AV_C_DESC) != 0);
    chk_t("J y C", "v = J t / C", sim_canal_en(0, 1e-3), 1.0, 1.0, 1e-9);

    /* ---- L en paralelo con una E: la corriente crece en rampa ---- */
    sr_init(2);
    sr_cc(0, 1, 1.0);
    kL = sr_el(SE_L, 1, 0, 1e-3);
    canal(0, CANAL_I, kL);
    saj.t_fin = 1e-3;
    emula("E y L");
    chk_cond("E y L", "avisa: la L empieza sin corriente", (sim_res()->d.avisos & SIM_AV_L_SIN_I) != 0);
    chk_t("E y L", "i = E t / L", sim_canal_en(0, 1e-3), 1.0, 1.0, 1e-9);

    /* ---- Errores con nombre ---- */
    sr_init(2);
    sr_cc(0, 1, 1.0);
    sr_cc(0, 1, 2.0);
    sr_el(SE_R, 1, 0, 1.0);
    chk_cond("errores", "dos E distintas en paralelo: lazo", sim_emular(&sr, &saj) == SIM_LAZO_V);
    sr_init(3);
    i = sr_el(SE_J, 0, 1, 0.0); sr.el[i].p[0] = 1.0;
    kS = sr_el(SE_S, 1, 2, 0.0); sr.el[kS].cerrado0 = 1; sr.el[kS].t1 = 1e-3;
    sr_el(SE_R, 2, 0, 1.0);
    saj.t_fin = 2e-3;
    chk_cond("errores", "abrir el único camino de una J: corte", sim_emular(&sr, &saj) == SIM_CORTE_J);
    chk_t("errores", "y dice cuándo", sim_res()->d.t, 1e-3, 1e-3, 1e-9);

    /* ---- El trapecio es de segundo orden: con la mitad del paso, un cuarto
     *      del error (RLC subamortiguado, v_C en 1 ms) ---- */
    {
        double L = 10e-3, C = 1e-6, R = 20.0, al = R / (2.0 * L), w0 = 1.0 / sqrt(L * C);
        double wd = sqrt(w0 * w0 - al * al), ex;
        t = 1e-3;
        ex = 1.0 - exp(-al * t) * (cos(wd * t) + al / wd * sin(wd * t));
        sr_init(4);
        sr_escalon(0, 1, 1.0); sr_el(SE_R, 1, 2, R); sr_el(SE_L, 2, 3, L);
        kC = sr_el(SE_C, 3, 0, C);
        canal(0, CANAL_V, kC);
        saj.t_fin = 1e-3; saj.h = 2e-6;
        emula("orden h");
        e1 = fabs(sim_canal_en(0, t) - ex);
        saj.h = 1e-6;
        emula("orden h/2");
        e2 = fabs(sim_canal_en(0, t) - ex);
        pruebas++;
        if (!(e1 / e2 > 3.0 && e1 / e2 < 5.0)) falla("orden", "error(h)/error(h/2) cerca de 4", e1 / e2, 4.0);
    }
}

/* --------------------------------------------------------------------------
 *  33. Punto de operación del emulador con C, L y S: reglas, rescates y
 *      errores con nombre
 * ----------------------------------------------------------------------- */
static int sr_j(int a, int b, double J)
{
    int k = sr_el(SE_J, a, b, 0.0);
    sr.el[k].forma = ONDA_CC; sr.el[k].p[0] = J;
    return k;
}

static const SimDiag *op_sr(Valores *v)
{
    static SimDiag d;
    sim_op_red(&sr, v, &d);
    return &d;
}

static void test_op_clS(void)
{
    static Valores v;
    const SimDiag *d;
    int kE, kE2, kC1, kC2, kL1, kL2, kJ, kS, kR, kA, i;
    double t;

    printf("[33] Punto de operación con C, L y S\n");

    /* ---- Divisor capacitivo: E = 12 con C1 = 1 µF y C2 = 3 µF en serie ---- */
    sr_init(3);
    sr_cc(0, 1, 12.0);
    kC1 = sr_el(SE_C, 1, 2, 1e-6);
    kC2 = sr_el(SE_C, 2, 0, 3e-6);
    d = op_sr(&v);
    chk_cond("divisor C", "sin error", d->estado == SIM_OK);
    chk_t("divisor C", "v(C1) = 9 V", v.V[kC1], 9.0, 12.0, 1e-12);
    chk_t("divisor C", "v(C2) = 3 V", v.V[kC2], 3.0, 12.0, 1e-12);
    chk_cond("divisor C", "aviso CARGA", (d->avisos & SIM_AV_CARGA) != 0);
    chk_cond("divisor C", "sin otros avisos", (d->avisos & ~SIM_AV_CARGA) == 0);
    /* con R = 100 en serie y V0 = 0: a 30 tau, el mismo reparto */
    sr_init(4);
    sr_cc(0, 1, 12.0);
    sr_el(SE_R, 1, 2, 100.0);
    kC1 = sr_el(SE_C, 2, 3, 1e-6); sr.el[kC1].ci = 1; sr.el[kC1].x0 = 0.0;
    kC2 = sr_el(SE_C, 3, 0, 3e-6); sr.el[kC2].ci = 1; sr.el[kC2].x0 = 0.0;
    d = op_sr(&v);
    chk_cond("divisor C con R", "V0 = 0: sin aviso CARGA", d->estado == SIM_OK && !(d->avisos & SIM_AV_CARGA));
    chk_t("divisor C con R", "i(0) = 12/100", v.I[kC1], 0.12, 0.12, 1e-12);
    canal(0, CANAL_V, kC1); canal(1, CANAL_V, kC2);
    saj.t_fin = 30.0 * 75e-6; saj.h = 0.75e-6;
    emula("divisor C con R");
    chk_t("divisor C con R", "a 30 tau, v(C1) = 9", sim_res()->ultimo.V[kC1], 9.0, 12.0, 1e-9);
    chk_t("divisor C con R", "a 30 tau, v(C2) = 3", sim_res()->ultimo.V[kC2], 3.0, 12.0, 1e-9);

    /* ---- Divisor inductivo: J = 4 A sobre L1 = 1 mH y L2 = 3 mH en paralelo ---- */
    sr_init(2);
    sr_j(0, 1, 4.0);
    kL1 = sr_el(SE_L, 1, 0, 1e-3);
    kL2 = sr_el(SE_L, 1, 0, 3e-3);
    d = op_sr(&v);
    chk_cond("divisor L", "sin error", d->estado == SIM_OK);
    chk_t("divisor L", "i(L1) = 3 A", v.I[kL1], 3.0, 4.0, 1e-12);
    chk_t("divisor L", "i(L2) = 1 A", v.I[kL2], 1.0, 4.0, 1e-12);
    chk_cond("divisor L", "aviso FLUJO en una de las L", (d->avisos & SIM_AV_FLUJO) != 0 &&
             (d->elem_aviso == kL1 || d->elem_aviso == kL2));

    /* ---- Rescates: sin régimen de CC ---- */
    sr_init(2);
    sr_j(0, 1, 1e-3);
    kC1 = sr_el(SE_C, 1, 0, 1e-6);
    d = op_sr(&v);
    chk_cond("rescate C", "J sobre C sin V0: aviso C_DESC", d->estado == SIM_OK && (d->avisos & SIM_AV_C_DESC));
    chk_t("rescate C", "empieza descargado", v.V[kC1], 0.0, 1.0, 1e-15);
    canal(0, CANAL_V, kC1);
    saj.t_fin = 2e-3; saj.h = 1e-5;
    emula("rescate C");
    for (i = 1; i <= 8; i++) {
        t = 0.25e-3 * i;
        chk_t("rescate C", "v = 1000 t (el trapecio es exacto en una rampa)", sim_canal_en(0, t), 1000.0 * t, 1000.0 * t, 1e-12);
    }
    sr_init(2);
    sr_cc(0, 1, 1.0);
    kL1 = sr_el(SE_L, 1, 0, 1.0);
    d = op_sr(&v);
    chk_cond("rescate L", "E sobre L sin I0: aviso L_SIN_I", d->estado == SIM_OK && (d->avisos & SIM_AV_L_SIN_I) &&
             d->elem_aviso == kL1);
    canal(0, CANAL_I, kL1);
    saj.t_fin = 2e-3; saj.h = 1e-5;
    emula("rescate L");
    for (i = 1; i <= 8; i++) {
        t = 0.25e-3 * i;
        chk_t("rescate L", "i = t", sim_canal_en(0, t), t, t, 1e-12);
    }

    /* ---- CI_SALTA: E = 10 ideal sobre C con V0 = 0 ---- */
    sr_init(2);
    sr_cc(0, 1, 10.0);
    kC1 = sr_el(SE_C, 1, 0, 1e-6); sr.el[kC1].ci = 1; sr.el[kC1].x0 = 0.0;
    d = op_sr(&v);
    chk_cond("CI_SALTA", "aviso: la V0 no cuadra con E", d->estado == SIM_OK && (d->avisos & SIM_AV_CI_SALTA));

    /* ---- Errores con nombre ---- */
    sr_init(2);
    kE = sr_cc(0, 1, 10.0);
    kE2 = sr_cc(0, 1, 12.0);
    d = op_sr(&v);
    chk_cond("errores", "E1 = 10 con E2 = 12 en paralelo: SIM_LAZO_V", d->estado == SIM_LAZO_V);
    chk_cond("errores", "el lazo es {E1, E2}", d->lazo == ((1u << kE) | (1u << kE2)));
    chk_cond("errores", "en el punto de operación: t = -1", d->t == -1.0);
    sr_init(2);
    sr_cc(0, 1, 10.0); sr_cc(0, 1, 10.0);
    chk_cond("errores", "E1 = E2 en paralelo: SIM_LAZO_V", op_sr(&v)->estado == SIM_LAZO_V);
    sr_init(2);
    kE = sr_cc(0, 1, 10.0);
    kA = sr_el(SE_AMI, 1, 0, 0.0);
    d = op_sr(&v);
    chk_cond("errores", "A ideal sobre E: SIM_LAZO_V con {E, A}", d->estado == SIM_LAZO_V &&
             d->lazo == ((1u << kE) | (1u << kA)));
    sr_init(3);
    kJ = sr_j(0, 1, 1.0);
    i = sr_el(SE_VMI, 1, 2, 0.0);
    sr_el(SE_R, 2, 0, 10.0);
    d = op_sr(&v);
    chk_cond("errores", "J con un V ideal en serie: SIM_CORTE_J", d->estado == SIM_CORTE_J && d->elem == kJ);
    chk_cond("errores", "el corte es {J, V}", d->lazo == ((1u << kJ) | (1u << i)));
    sr_init(3);
    kJ = sr_j(0, 1, 2.0);
    sr_j(1, 2, 2.0);
    sr_el(SE_R, 2, 0, 10.0);
    chk_cond("errores", "J1 = J2 en serie: SIM_CORTE_J", op_sr(&v)->estado == SIM_CORTE_J);
    /* un S que al cerrarse en 2 ms pone E1 en corto: sale antes de correr */
    sr_init(2);
    kE = sr_cc(0, 1, 10.0);
    sr_el(SE_R, 1, 0, 10.0);
    kS = sr_el(SE_S, 1, 0, 0.0); sr.el[kS].cerrado0 = 0; sr.el[kS].t1 = 2e-3;
    saj.t_fin = 5e-3;
    chk_cond("errores", "S que cierra en corto con E1: SIM_LAZO_V", sim_emular(&sr, &saj) == SIM_LAZO_V);
    chk_t("errores", "con el instante: 2 ms", sim_res()->d.t, 2e-3, 2e-3, 1e-12);
    chk_cond("errores", "el lazo es {E1, S1}", sim_res()->d.lazo == ((1u << kE) | (1u << kS)));
    chk_cond("errores", "antes de correr: ningún paso", sim_res()->pasos == 0);
    {
        char m[200];
        sim_mensaje(&sr, &sim_res()->d, m, sizeof m);
        chk_cond("errores", "el mensaje dice cuándo", strstr(m, "2 ms") != 0);
    }

    /* ---- FLOTA: un trozo de resistencias detrás de un S abierto ---- */
    sr_init(4);
    sr_cc(0, 1, 10.0);
    sr_el(SE_R, 1, 0, 10.0);
    kS = sr_el(SE_S, 1, 2, 0.0);
    kR = sr_el(SE_R, 2, 3, 5.0);
    sr_el(SE_R, 3, 2, 7.0);
    d = op_sr(&v);
    chk_cond("FLOTA", "SIM_OK con el aviso", d->estado == SIM_OK && (d->avisos & SIM_AV_FLOTA));
    chk_t("FLOTA", "el trozo queda a 0 V", v.phi[2], 0.0, 1.0, 1e-15);
    chk_t("FLOTA", "y sin corriente", v.I[kR], 0.0, 1.0, 1e-15);
    chk_t("FLOTA", "el S abierto aguanta 10 V", v.V[kS], 10.0, 10.0, 1e-12);
}

/* --------------------------------------------------------------------------
 *  34. Transitorio de la Etapa 2: el trapecio, balance, lecturas y trozos
 * ----------------------------------------------------------------------- */
/* El valor de un canal justo ANTES de t (en un evento, la primera de las
 * dos muestras con el mismo t) */
static double y_antes(int c, double t)
{
    const SimReg *g = sim_reg();
    int i;
    for (i = 0; i < g->n; i++)
        if (fabs(g->t[i] - t) <= 1e-12 * (fabs(t) + 1e-9)) return g->y[c][i];
    return sim_canal_en(c, t);
}

static int muestras_en(double t)
{
    const SimReg *g = sim_reg();
    int i, n = 0;
    for (i = 0; i < g->n; i++) if (g->t[i] == t) n++;
    return n;
}

/* RC con E continua y V0 = 0 (sin evento en 0): error en 1 ms con N pasos */
static double error_rc(int N, double *v1)
{
    int kC;
    sr_init(3);
    sr_cc(0, 1, 10.0);
    sr_el(SE_R, 1, 2, 1e3);
    kC = sr_el(SE_C, 2, 0, 1e-6); sr.el[kC].ci = 1; sr.el[kC].x0 = 0.0;
    canal(0, CANAL_V, kC);
    saj.t_fin = 1e-3; saj.h = 1e-3 / N;
    emula("RC orden");
    *v1 = sim_canal_en(0, 1e-3);
    return fabs(*v1 - 10.0 * (1.0 - exp(-1.0)));
}

static double error_rl(int N)
{
    int kL;
    sr_init(3);
    sr_cc(0, 1, 10.0);
    sr_el(SE_R, 1, 2, 10.0);
    kL = sr_el(SE_L, 2, 0, 10e-3); sr.el[kL].ci = 1; sr.el[kL].x0 = 0.0;
    canal(0, CANAL_I, kL);
    saj.t_fin = 1e-3; saj.h = 1e-3 / N;
    emula("RL orden");
    return fabs(sim_canal_en(0, 1e-3) - (1.0 - exp(-1.0)));
}

/* RLC serie con E = 10 continua, V0 = 0 e I0 = 0: error máximo de v_C */
static double error_rlc(double R, double ventana, int N)
{
    double L = 10e-3, C = 1e-6, al = R / (2.0 * L), w0 = 1.0 / sqrt(L * C), peor = 0.0;
    const SimReg *g;
    int kC, kL, i;
    sr_init(4);
    sr_cc(0, 1, 10.0);
    sr_el(SE_R, 1, 2, R);
    kL = sr_el(SE_L, 2, 3, L); sr.el[kL].ci = 1;
    kC = sr_el(SE_C, 3, 0, C); sr.el[kC].ci = 1;
    canal(0, CANAL_V, kC);
    saj.t_fin = ventana; saj.h = ventana / N;
    emula("RLC orden");
    g = sim_reg();
    for (i = 0; i < g->n; i++) {
        double t = g->t[i], ex;
        if (al < w0 * (1.0 - 1e-12)) {
            double wd = sqrt(w0 * w0 - al * al);
            ex = 10.0 * (1.0 - exp(-al * t) * (cos(wd * t) + al / wd * sin(wd * t)));
        } else if (al <= w0 * (1.0 + 1e-12)) ex = 10.0 * (1.0 - exp(-al * t) * (1.0 + al * t));
        else {
            double d = sqrt(al * al - w0 * w0), s1 = -al + d, s2 = -al - d;
            ex = 10.0 * (1.0 + (s2 * exp(s1 * t) - s1 * exp(s2 * t)) / (s1 - s2));
        }
        if (fabs(g->y[0][i] - ex) > peor) peor = fabs(g->y[0][i] - ex);
    }
    return peor;
}

static unsigned azar_sim = 12345u;
static double u01(void) { azar_sim = azar_sim * 1103515245u + 12345u; return (double)((azar_sim >> 8) & 0xFFFFFF) / 16777216.0; }

/* Red RLC al azar con fuentes continuas y V0/I0 dados */
static void red_rlc_azar(void)
{
    int nn = 3 + (int)(u01() * 4.0), n, k, nel;
    sr_init(nn);
    for (n = 1; n < nn; n++) sr_el(SE_R, n - 1, n, 10.0 + 990.0 * u01());   /* conexa */
    nel = 3 + (int)(u01() * 6.0);
    for (k = 0; k < nel; k++) {
        int a = (int)(u01() * nn), b = (int)(u01() * nn), q;
        double r = u01();
        if (a == b) b = (a + 1) % nn;
        if (r < 0.25) q = sr_el(SE_R, a, b, 10.0 + 990.0 * u01());
        else if (r < 0.45) { q = sr_el(SE_C, a, b, 1e-7 + 1e-6 * u01()); sr.el[q].ci = 1; sr.el[q].x0 = 10.0 * u01() - 5.0; }
        else if (r < 0.65) { q = sr_el(SE_L, a, b, 1e-3 + 1e-2 * u01()); sr.el[q].ci = 1; sr.el[q].x0 = 0.02 * u01() - 0.01; }
        else if (r < 0.8) { q = sr_el(SE_E, a, b, 10.0 + 90.0 * u01()); sr.el[q].forma = ONDA_CC; sr.el[q].p[0] = 20.0 * u01() - 10.0; }
        else { q = sr_el(SE_J, a, b, 0.0); sr.el[q].forma = ONDA_CC; sr.el[q].p[0] = 0.02 * u01() - 0.01; }
        (void)q;
    }
}

static void test_transitorio_e2(void)
{
    const SimRes *r;
    SimMedidas me;
    int kE, kC, kL, kR, kS, kV, kA, i, n_bal = 0;
    double e1, e2, e3, v1;

    printf("[34] Transitorio: trapecio, balance, lecturas y trozos\n");

    /* ---- RC y RL: las cifras del trapecio ---- */
    e1 = error_rc(50, &v1);
    chk_t("RC", "error en 1 ms con N = 50: 1,226e-4", e1, 1.226e-4, 1.226e-4, 0.02);
    e2 = error_rc(100, &v1);
    chk_t("RC", "con N = 100: 3,066e-5", e2, 3.066e-5, 3.066e-5, 0.02);
    e3 = error_rc(200, &v1);
    chk_t("RC", "con N = 200: 7,664e-6", e3, 7.664e-6, 7.664e-6, 0.02);
    chk_t("RC", "v(1 ms) = 6,321205588285577", v1, 6.321205588285577, 1.0, 1e-4);
    e1 = error_rl(50); e2 = error_rl(100);
    chk_t("RL", "i = 1 - e^(-t/1 ms): el mismo patrón (N = 50)", e1, 1.226e-5, 1.226e-5, 0.02);
    chk_t("RL", "N = 100", e2, 3.066e-6, 3.066e-6, 0.02);

    /* ---- Descarga de un RC sin fuentes ---- */
    sr_init(2);
    kC = sr_el(SE_C, 1, 0, 1e-6); sr.el[kC].ci = 1; sr.el[kC].x0 = 5.0;
    sr_el(SE_R, 1, 0, 1e3);
    canal(0, CANAL_V, kC);
    emula("descarga RC");
    chk_t("descarga RC", "v(1 ms) = 5 e^-1", sim_canal_en(0, 1e-3), 5.0 * exp(-1.0), 5.0, 1e-5);
    chk_t("descarga RC", "v(3 ms) = 5 e^-3", sim_canal_en(0, 3e-3), 5.0 * exp(-3.0), 5.0, 1e-5);

    /* ---- RLC serie: error máximo de v_C (tabla del prototipo del trapecio) ---- */
    {
        static const double RR[3] = { 20.0, 200.0, 1000.0 };
        /* trapecio puro calculado aparte (tr_rlc.py, sin el emulador); la
         * tabla de la especificación tenía mal la fila de R = 1000 */
        static const double ERR[3][3] = { { 6.888e-3, 1.722e-3, 4.305e-4 }, { 1.501e-3, 3.754e-4, 9.382e-5 }, { 1.227e-3, 3.048e-4, 7.559e-5 } };
        int c3, j;
        for (c3 = 0; c3 < 3; c3++) {
            double ven = c3 == 0 ? 6.0 * 2.0 * PI_T / 9949.874371066199 : 5e-3, e[3];
            char q[40];
            sprintf(q, "RLC R = %g", RR[c3]);
            for (j = 0; j < 3; j++) {
                e[j] = error_rlc(RR[c3], ven, 800 << j);
                chk_t(q, "error máximo de v_C = el del trapecio (±1 %)", e[j], ERR[c3][j], ERR[c3][j], 0.01);
            }
            pruebas += 2;
            if (!(e[0] / e[1] >= 3.6 && e[0] / e[1] <= 4.4)) falla(q, "cociente de errores 800/1600 en [3,6; 4,4]", e[0] / e[1], 4.0);
            if (!(e[1] / e[2] >= 3.6 && e[1] / e[2] <= 4.4)) falla(q, "cociente de errores 1600/3200 en [3,6; 4,4]", e[1] / e[2], 4.0);
        }
        /* el pico del subamortiguado */
        {
            const SimReg *g;
            int im = 0;
            (void)error_rlc(20.0, 6.0 * 2.0 * PI_T / 9949.874371066199, 1600);
            g = sim_reg();
            double tp, c2;
            for (i = 1; i < g->n; i++) if (g->y[0][i] > g->y[0][im]) im = i;
            /* vértice de la parábola por las tres muestras del pico */
            c2 = g->y[0][im - 1] - 2.0 * g->y[0][im] + g->y[0][im + 1];
            tp = g->t[im] - 0.5 * (g->t[im + 1] - g->t[im]) * (g->y[0][im + 1] - g->y[0][im - 1]) / c2;
            sim_medir(0, 0.0, 0.6e-3, &me);
            chk_t("RLC R = 20", "pico de v_C = 17,29247614287671 V", me.max, 17.29247614287671, 17.29247614287671, 2e-3);
            chk_t("RLC R = 20", "en t = 0,3157419417 ms", tp, 0.3157419417e-3, 0.3157419417e-3, 2e-3);
        }
    }

    /* ---- Balance discreto en redes RLC al azar (sin eventos: todo trapecio) ---- */
    for (i = 0; i < 300; i++) {
        double esc = 0.0;
        int k;
        red_rlc_azar();
        canal(0, CANAL_NADA, 0); canal(1, CANAL_NADA, 0);
        saj.t_fin = 2e-3; saj.h = 2e-6;
        if (sim_emular(&sr, &saj) != SIM_OK) continue;
        r = sim_res();
        if (r->eventos != 0 || (r->d.avisos & (SIM_AV_CI_SALTA | SIM_AV_IMPULSO))) continue;
        for (k = 0; k < sr.nel; k++) esc += fabs(r->W[k]) + r->E0[k] + r->Efin[k];
        n_bal++;
        pruebas++;
        if (!(fabs(r->dif_balance) <= 1e-12 * (esc + 1e-30))) { falla("balance", "entregada = disipada + almacenada (1e-12)", r->dif_balance, 0.0); break; }
    }
    printf("    balance exacto en %d redes RLC al azar\n", n_bal);
    pruebas++;
    if (n_bal < 150) falla("balance", "pocas redes sin eventos", n_bal, 150);

    /* ---- Régimen = punto de operación (sin condiciones iniciales) ---- */
    {
        static Valores vop;
        SimDiag dd;
        double tau;
        int k;
        sr_init(4);
        kE = sr_cc(0, 1, 10.0);
        sr_el(SE_R, 1, 2, 100.0);
        kL = sr_el(SE_L, 2, 3, 10e-3);
        kR = sr_el(SE_R, 3, 0, 200.0);
        kC = sr_el(SE_C, 3, 0, 1e-6);
        sim_op_red(&sr, &vop, &dd);
        sr.el[kL].ci = 1; sr.el[kL].x0 = 0.5;
        sr.el[kC].ci = 1; sr.el[kC].x0 = -2.0;
        emula("régimen");
        tau = sim_res()->tau_max;
        saj.t_fin = 30.0 * tau; saj.h = tau / 200.0;
        emula("régimen");
        r = sim_res();
        for (k = 0; k < sr.nel; k++) {
            chk_t("régimen", "V a 30 tau = la del punto de operación", r->ultimo.V[k], vop.V[k], 10.0, 1e-9);
            chk_t("régimen", "I a 30 tau = la del punto de operación", r->ultimo.I[k], vop.I[k], 0.1, 1e-9);
        }
        (void)kE; (void)kR;
    }

    /* ---- Lecturas de los instrumentos: CC, CA y EF ---- */
    {
        sr_init(3);
        kE = sr_el(SE_E, 0, 1, 0.0);
        sr.el[kE].forma = ONDA_SENO; sr.el[kE].p[0] = 10.0; sr.el[kE].p[1] = 1e3; sr.el[kE].p[3] = 3.0;
        sr_el(SE_R, 1, 2, 1e3);
        kV = sr_el(SE_VMI, 2, 0, 0.0); sr.el[kV].instr = 'V'; sr.el[kV].med = MED_CA;
        kA = sr_el(SE_AMI, 2, 0, 0.0); sr.el[kA].instr = 'A'; sr.el[kA].med = MED_EF;
        kR = sr_el(SE_R, 1, 0, 1e3);   sr.el[kR].instr = 'V'; sr.el[kR].med = MED_CC;
        kC = sr_el(SE_VMI, 1, 0, 0.0); sr.el[kC].instr = 'V'; sr.el[kC].med = MED_CA;
        saj.t_fin = 10e-3; saj.h = 1e-3 / 200.0;
        emula("lecturas");
        r = sim_res();
        chk_t("lecturas", "V en CC sobre 3 + 10 sen: 3", r->lectura[kR], 3.0, 3.0, 1e-6);
        chk_t("lecturas", "V en CA sobre 3 + 10 sen: solo la alterna, 10/sqrt 2", r->lectura[kC], 10.0 / sqrt(2.0), 10.0, 1e-6);
        chk_t("lecturas", "V ideal en CA en corto: 0", r->lectura[kV], 0.0, 1.0, 1e-9);
        chk_t("lecturas", "A en EF: sqrt(3² + 10²/2)/1k", r->lectura[kA], sqrt(9.0 + 50.0) / 1e3, 1e-2, 1e-6);
        chk_t("lecturas", "Vef de la R", r->Vef[kR], sqrt(9.0 + 50.0), 10.0, 1e-6);
        chk_t("lecturas", "Pmed de la R = Vef²/R", r->Pmed[kR], r->Vef[kR] * r->Vef[kR] / 1e3, 0.059, 1e-12);
        chk_t("lecturas", "y = 59 mW", r->Pmed[kR], 59.0 / 1e3, 0.059, 1e-6);
        chk_cond("lecturas", "ventana de medida: periodos enteros al final", fabs(r->t_med0 - 5e-3) < 1e-12);
    }

    /* ---- Por trozos: 1, 997 e ilimitadas operaciones dan lo mismo, bit a bit ---- */
    {
        static SimReg g0;
        static SimRes r0;
        static const unsigned long TROZO[3] = { 0ul, 1ul, 997ul };
        int t3;
        sr_init(4);
        kE = sr_el(SE_E, 0, 1, 0.0);
        sr.el[kE].forma = ONDA_CUADRADA; sr.el[kE].p[0] = 5.0; sr.el[kE].p[1] = 1e3; sr.el[kE].p[2] = 30.0; sr.el[kE].p[3] = 5.0;
        sr_el(SE_R, 1, 2, 100.0);
        kL = sr_el(SE_L, 2, 3, 10e-3);
        kC = sr_el(SE_C, 3, 0, 1e-6);
        kS = sr_el(SE_S, 3, 0, 0.0); sr.el[kS].t1 = 1.37e-3; sr.el[kS].t2 = 2.5e-3;
        kV = sr_el(SE_VMI, 3, 0, 0.0); sr.el[kV].instr = 'V'; sr.el[kV].med = MED_EF;
        canal(0, CANAL_V, kC); canal(1, CANAL_I, kL);
        saj.t_fin = 5e-3; saj.h = 0.0;
        for (t3 = 0; t3 < 3; t3++) {
            sim_iniciar_red(&sr, &saj);
            if (TROZO[t3] == 0) while (sim_avanzar(~0ul)) ;
            else while (sim_avanzar(TROZO[t3])) ;
            if (t3 == 0) { g0 = *sim_reg(); r0 = *sim_res(); continue; }
            r = sim_res();
            chk_cond("trozos", "mismas muestras bit a bit", sim_reg()->n == g0.n &&
                     memcmp(sim_reg()->t, g0.t, sizeof(double) * (size_t)g0.n) == 0 &&
                     memcmp(sim_reg()->y[0], g0.y[0], sizeof(double) * (size_t)g0.n) == 0 &&
                     memcmp(sim_reg()->y[1], g0.y[1], sizeof(double) * (size_t)g0.n) == 0);
            chk_cond("trozos", "mismos máximos, energías y lecturas", memcmp(r->Vmax, r0.Vmax, sizeof r0.Vmax) == 0 &&
                     memcmp(r->Imax, r0.Imax, sizeof r0.Imax) == 0 && memcmp(r->W, r0.W, sizeof r0.W) == 0 &&
                     memcmp(r->lectura, r0.lectura, sizeof r0.lectura) == 0 && r->fin == FIN_COMPLETO);
        }
        chk_cond("trozos", "la emulación acabó completa", r0.fin == FIN_COMPLETO && r0.pasos > 0);
    }

    /* ---- Cortar: lo hecho vale ---- */
    sr_init(3);
    sr_escalon(0, 1, 10.0);
    sr_el(SE_R, 1, 2, 1e3);
    kC = sr_el(SE_C, 2, 0, 1e-6);
    canal(0, CANAL_V, kC);
    saj.t_fin = 5e-3; saj.h = 1e-6;
    sim_iniciar_red(&sr, &saj);
    sim_avanzar(20000ul);
    sim_cortar();
    r = sim_res();
    chk_cond("cortar", "FIN_CORTADO, con pasos hechos", r->fin == FIN_CORTADO && r->pasos > 0 && r->pasos < 5000);
    chk_cond("cortar", "y ya no avanza", sim_avanzar(1000000ul) == 0);
    {
        const SimReg *g = sim_reg();
        double tc = g->t[g->n - 1];
        chk_t("cortar", "lo hecho vale", g->y[0][g->n - 1], 10.0 * (1.0 - exp(-tc / 1e-3)), 10.0, 1e-5);
    }
}

/* --------------------------------------------------------------------------
 *  35. Eventos, formas e impulsos
 * ----------------------------------------------------------------------- */
static double reparto(double t1, double h, double *vmenos, double *vmas, int *nsuc, double *dif, double imax[MAX_EL])
{
    int kC1, kC2, kS, k;
    sr_init(3);
    kC1 = sr_el(SE_C, 1, 0, 1e-6); sr.el[kC1].ci = 1; sr.el[kC1].x0 = 10.0;
    sr_el(SE_R, 1, 0, 1e3);
    kS = sr_el(SE_S, 1, 2, 0.0); sr.el[kS].cerrado0 = 0; sr.el[kS].t1 = t1;
    kC2 = sr_el(SE_C, 2, 0, 3e-6); sr.el[kC2].ci = 1; sr.el[kC2].x0 = 0.0;
    canal(0, CANAL_V, kC1); canal(1, CANAL_V, kC2);
    saj.t_fin = 3e-3; saj.h = h;
    emula("reparto de carga");
    *vmenos = y_antes(0, t1);
    *vmas = sim_canal_en(0, t1);
    *nsuc = sim_res()->nsucesos;
    *dif = sim_res()->dif_balance;
    for (k = 0; k < sr.nel; k++) imax[k] = sim_res()->Imax[k];
    (void)kC2;
    return sim_canal_en(0, 3e-3);
}

static void test_eventos(void)
{
    const SimRes *r;
    SimMedidas me;
    int kE, kC, kL, kR, kS, kJ, i;
    double e1 = 0.0, e2 = 0.0;

    printf("[35] Eventos, formas e impulsos\n");
    (void)e1; (void)e2;

    /* ---- RC con cuadrada 0/10 V, 1 kHz, 50 %, tau = T/4 ---- */
    {
        double err[2], alto = 8.807970779778824, bajo = 1.1920292202211755;
        int j;
        for (j = 0; j < 2; j++) {
            sr_init(3);
            kE = sr_el(SE_E, 0, 1, 0.0);
            sr.el[kE].forma = ONDA_CUADRADA; sr.el[kE].p[0] = 5.0; sr.el[kE].p[1] = 1e3; sr.el[kE].p[2] = 50.0; sr.el[kE].p[3] = 5.0;
            sr_el(SE_R, 1, 2, 250.0);
            kC = sr_el(SE_C, 2, 0, 1e-6);
            canal(0, CANAL_V, kC);
            saj.t_fin = 20e-3; saj.h = 1e-3 / (200 << j);
            emula("RC cuadrada");
            err[j] = fabs(y_antes(0, 19.5e-3) - alto);
            if (j == 0) {
                r = sim_res();
                chk_t("RC cuadrada", "al final del semiperiodo alto: 8,80797", y_antes(0, 19.5e-3), alto, 10.0, 2.5e-5);
                chk_t("RC cuadrada", "al final del bajo: 1,19203", y_antes(0, 20e-3 - 1e-12 * 0) , bajo, 10.0, 2.5e-5);
                chk_t("RC cuadrada", "error con 200 pasos por periodo ~1,93e-4", err[0], 0.0, 1.0, 2.5e-4);
                sim_medir(0, 19e-3, 20e-3, &me);
                chk_t("RC cuadrada", "media del último periodo = 5 V", me.media, 5.0, 5.0, 1e-4);
                chk_t("RC cuadrada", "Vmed de C en la ventana = 5 V (la calculadora con E = 5)", r->Vmed[kC], 5.0, 5.0, 1e-4);
                chk_cond("RC cuadrada", "dos factorizaciones (trapecio y arranque): la malla cae en los flancos", r->factorizaciones == 2);
                chk_cond("RC cuadrada", "40 eventos", r->eventos == 40);
            }
        }
        pruebas++;
        if (!(err[0] / err[1] >= 3.6 && err[0] / err[1] <= 4.4)) falla("RC cuadrada", "cociente 200/400 en [3,6; 4,4]", err[0] / err[1], 4.0);
    }

    /* ---- El dual de C sobre E senoidal: J senoidal en serie con L = 1 mH y 10 Ω ---- */
    {
        static const int NP[3] = { 50, 100, 400 };
        static const double TOLC[3] = { 1.5e-3, 3.6e-4, 2.5e-5 };
        double L = 1e-3, w = 2.0 * PI_T * 1e3, amp = L * 10.0 * w;
        int c3;
        for (c3 = 0; c3 < 3; c3++) {
            const SimReg *g;
            double peor = 0.0;
            char q[40];
            sr_init(3);
            kJ = sr_el(SE_J, 0, 1, 0.0);
            sr.el[kJ].forma = ONDA_SENO; sr.el[kJ].p[0] = 10.0; sr.el[kJ].p[1] = 1e3; sr.el[kJ].p[2] = 45.0;
            kL = sr_el(SE_L, 1, 2, L);
            sr_el(SE_R, 2, 0, 10.0);
            canal(0, CANAL_V, kL);
            saj.t_fin = 6e-3; saj.h = 1e-3 / NP[c3];
            sprintf(q, "L con J senoidal, N = %d", NP[c3]);
            emula(q);
            g = sim_reg();
            for (i = 0; i < g->n; i++) {
                double ex;
                if (g->t[i] < 3e-3) continue;
                ex = amp * cos(w * g->t[i] + PI_T / 4.0);
                if (fabs(g->y[0][i] - ex) > peor) peor = fabs(g->y[0][i] - ex);
            }
            chk_t(q, "error relativo de v_L desde 3T (el dual)", peor / amp, 0.0, 1.0, TOLC[c3]);
            if (c3 == 0) chk_cond(q, "la J salta en t = 0 sobre la L: impulso inductivo",
                                  sim_res()->nsucesos >= 1 && sim_res()->suceso[0].tipo == SUC_IMPULSO_L);
        }
    }

    /* ---- Reparto de carga: C1 = 1 µF a 10 V sobre 1k; S cierra en 1 ms y pone
     *      en paralelo C2 = 3 µF a 0 V ---- */
    {
        double vm, vp, dif, im1[MAX_EL], im2[MAX_EL], d;
        int ns, k, ok = 1;
        (void)reparto(1e-3, 5e-6, &vm, &vp, &ns, &dif, im1);
        r = sim_res();
        chk_t("reparto", "v- = 10 e^-1 = 3,6787944", vm, 3.6787944117144233, 1.0, 1e-5);
        chk_t("reparto", "v+ = v-/4 = 0,9196986", vp, 0.9196986029286058, 1.0, 1e-5);
        chk_cond("reparto", "un SUC_IMPULSO_C en t = 1 ms", ns == 1 && r->suceso[0].tipo == SUC_IMPULSO_C &&
                 fabs(r->suceso[0].t - 1e-3) < 1e-15 && (r->d.avisos & SIM_AV_IMPULSO));
        chk_t("reparto", "la energía perdida en el salto: 5,075073 µJ", dif, 5.075073121372977e-6, 5.075073121372977e-6, 0.01);
        (void)reparto(1e-3, 2.5e-6, &vm, &vp, &ns, &dif, im2);
        for (k = 0; k < sr.nel; k++) {
            d = fabs(fabs(im1[k]) - fabs(im2[k]));
            if (d > 0.01 * fabs(im2[k]) + 1e-9) ok = 0;
        }
        chk_cond("reparto", "ningún máximo de corriente cambia más de un 1 % entre h y h/2", ok);
    }
    /* lo mismo con t1 = 0,37 ms, fuera de la malla: orden >= 1,8 */
    {
        double vm, vp, dif, im[MAX_EL], err[3], ex;
        int ns, k;
        ex = 10.0 * exp(-0.37) / 4.0 * exp(-(3e-3 - 0.37e-3) / 4e-3);
        for (k = 0; k < 3; k++) err[k] = fabs(reparto(0.37e-3, 3e-3 / (97 << k), &vm, &vp, &ns, &dif, im) - ex);
        pruebas += 2;
        if (!(log(err[0] / err[1]) / log(2.0) >= 1.8)) falla("reparto fuera de malla", "orden >= 1,8 (97 y 194 pasos)", log(err[0] / err[1]) / log(2.0), 2.0);
        if (!(log(err[1] / err[2]) / log(2.0) >= 1.8)) falla("reparto fuera de malla", "orden >= 1,8 (194 y 388 pasos)", log(err[1] / err[2]) / log(2.0), 2.0);
    }

    /* ---- Chispa: E = 10, S cerrado, L = 10 mH y R = 10; S abre en 5 ms ---- */
    sr_init(4);
    sr_cc(0, 1, 10.0);
    kS = sr_el(SE_S, 1, 2, 0.0); sr.el[kS].cerrado0 = 1; sr.el[kS].t1 = 5e-3;
    kL = sr_el(SE_L, 2, 3, 10e-3);
    sr_el(SE_R, 3, 0, 10.0);
    canal(0, CANAL_I, kL);
    saj.t_fin = 8e-3;
    emula("chispa");
    r = sim_res();
    chk_cond("chispa", "SUC_IMPULSO_L en 5 ms", r->nsucesos == 1 && r->suceso[0].tipo == SUC_IMPULSO_L &&
             r->suceso[0].elem == kL && fabs(r->suceso[0].t - 5e-3) < 1e-15);
    chk_t("chispa", "antes, i_L = E/R = 1 A (E continua: ya estaba en régimen)", y_antes(0, 5e-3), 1.0, 1.0, 1e-9);
    chk_t("chispa", "después, i_L = 0", sim_canal_en(0, 6e-3), 0.0, 1.0, 1e-12);

    /* ---- Derivador: cuadrada 0/10 V a 100 Hz, C = 100 nF en serie, R = 1k ---- */
    sr_init(3);
    kE = sr_el(SE_E, 0, 1, 0.0);
    sr.el[kE].forma = ONDA_CUADRADA; sr.el[kE].p[0] = 5.0; sr.el[kE].p[1] = 100.0; sr.el[kE].p[2] = 50.0; sr.el[kE].p[3] = 5.0;
    kC = sr_el(SE_C, 1, 2, 100e-9);
    kR = sr_el(SE_R, 2, 0, 1e3);
    canal(0, CANAL_V, kR);
    saj.t_fin = 0.0; saj.h = 0.0;
    emula("derivador");
    chk_cond("derivador", "dos muestras con t = 0", muestras_en(0.0) == 2);
    chk_t("derivador", "y+ de v_R en 0 = 10", sim_canal_en(0, 0.0), 10.0, 10.0, 1e-3);
    chk_t("derivador", "v_R(0,1 ms) = 3,678794", sim_canal_en(0, 0.1e-3), 3.678794, 3.678794, 1e-3);
    sim_medir(0, 0.0, sim_res()->t_fin, &me);
    chk_t("derivador", "mínimo = -10", me.min, -10.0, 10.0, 1e-3);
    chk_t("derivador", "máximo = 10", me.max, 10.0, 10.0, 1e-3);
    (void)kC;

    /* ---- ¿Es t = 0 una discontinuidad? ---- */
    {
        static const struct { int forma; double p[5]; int disc; const char *q; } CASOS[6] = {
            { ONDA_SENO,       { 1.0, 1e3, 0.0, 0.0, 0.0 }, 1, "seno con fase 0" },
            { ONDA_TRIANGULAR, { 1.0, 1e3, 50.0, 0.0, 0.0 }, 1, "triangular" },
            { ONDA_PULSO,      { 0.0, 1.0, 0.0, 0.0, 0.0 }, 1, "pulso con t0 = 0" },
            { ONDA_PULSO,      { 0.0, 1.0, 1e-3, 0.0, 0.0 }, 0, "pulso con t0 = 1 ms" },
            { ONDA_CC,         { 1.0, 0.0, 0.0, 0.0, 0.0 }, 0, "E continua con V0 consistente" },
            { -1,              { 1.0, 0.0, 0.0, 0.0, 0.0 }, 1, "S con t1 = 0" },
        };
        int c6;
        for (c6 = 0; c6 < 6; c6++) {
            sr_init(3);
            kE = sr_el(SE_E, 0, 1, 0.0);
            sr.el[kE].forma = (unsigned char)(CASOS[c6].forma < 0 ? ONDA_CC : CASOS[c6].forma);
            memcpy(sr.el[kE].p, CASOS[c6].p, sizeof CASOS[c6].p);
            if (CASOS[c6].forma < 0) {
                kS = sr_el(SE_S, 1, 2, 0.0); sr.el[kS].t1 = 0.0;
            } else sr_el(SE_R, 1, 2, 1e3);
            kC = sr_el(SE_C, 2, 0, 1e-6);
            if (CASOS[c6].forma == ONDA_CC) { sr.el[kC].ci = 1; sr.el[kC].x0 = 1.0; }
            sr_el(SE_R, 2, 0, 1e3);
            canal(0, CANAL_V, kC);
            saj.t_fin = 2e-3; saj.h = 1e-5;
            emula(CASOS[c6].q);
            chk_cond(CASOS[c6].q, CASOS[c6].disc ? "t = 0 es una discontinuidad (dos muestras)" : "t = 0 no es una discontinuidad",
                     (muestras_en(0.0) == 2) == CASOS[c6].disc);
        }
    }

    /* ---- Dos S que actúan "a la vez" con un ulp de diferencia (lo halló el
     *      oráculo): S1 abre y S2 cierra; no se pierde ninguno ---- */
    {
        double t1 = 2.24e-3, t2 = nextafter(t1, 1.0);
        int kS1, kS2;
        sr_init(4);
        sr_cc(0, 1, 10.0);
        sr_el(SE_R, 1, 2, 100.0);
        kS2 = sr_el(SE_S, 2, 0, 0.0); sr.el[kS2].cerrado0 = 0; sr.el[kS2].t1 = t1;   /* cierra */
        kS1 = sr_el(SE_S, 1, 3, 0.0); sr.el[kS1].cerrado0 = 1; sr.el[kS1].t1 = t2;   /* abre   */
        kR = sr_el(SE_R, 3, 0, 100.0);
        canal(0, CANAL_I, kR); canal(1, CANAL_I, kS2);
        saj.t_fin = 5e-3; saj.h = 1e-5;
        emula("S simultáneos");
        chk_t("S simultáneos", "antes: 100 mA por R2 (S1 cerrado)", sim_canal_en(0, 1.5e-3), 0.1, 0.1, 1e-9);
        chk_t("S simultáneos", "después: S1 abierto, R2 sin corriente", sim_canal_en(0, 3e-3), 0.0, 0.1, 1e-9);
        chk_t("S simultáneos", "y S2 cerrado lleva E/R1 = 100 mA", sim_canal_en(1, 3e-3), 0.1, 0.1, 1e-9);
        (void)kS1;
    }

    /* ---- Un C libre en paralelo con otro que ya se mueve en t = 0 (lo halló
     *      el oráculo): t = 0 es una discontinuidad y la corriente se reparte
     *      por las capacidades, sin oscilación del trapecio ---- */
    {
        int kC1, kC2, j, alterna = 0;
        sr_init(3);
        sr_cc(0, 1, 10.0);
        sr_el(SE_R, 1, 2, 1e3);
        kC1 = sr_el(SE_C, 2, 0, 1e-6); sr.el[kC1].ci = 1; sr.el[kC1].x0 = 0.0;
        kC2 = sr_el(SE_C, 2, 0, 3e-6);                         /* sin V0: libre */
        canal(0, CANAL_I, kC1); canal(1, CANAL_I, kC2);
        saj.t_fin = 2e-3; saj.h = 1e-5;
        emula("C libre en paralelo");
        chk_cond("C libre en paralelo", "t = 0 es una discontinuidad", muestras_en(0.0) == 2);
        for (j = 1; j <= 20; j++) {
            double t = 0.1e-3 + j * 1e-5, i1 = sim_canal_en(0, t), i2 = sim_canal_en(1, t);
            if (fabs(i2 - 3.0 * i1) > 1e-6 * fabs(i2)) alterna = 1;
        }
        chk_cond("C libre en paralelo", "i(C2) = 3 i(C1) en cada paso (sin oscilar)", !alterna);
        chk_t("C libre en paralelo", "i total = (10/1k) e^(-t/4 ms)", sim_canal_en(0, 1e-3) + sim_canal_en(1, 1e-3),
              0.01 * exp(-0.25), 0.01, 1e-5);
    }

    /* ---- Dos C con V0 en paralelo: en el punto de operación uno lleva i = 0
     *      (la regla del lazo que cuadra), así que t = 0 es discontinuidad y
     *      la corriente se reparte 1:3 sin oscilar ---- */
    {
        int kC1, kC2, j, alterna = 0;
        sr_init(3);
        sr_cc(0, 1, 10.0);
        sr_el(SE_R, 1, 2, 1e3);
        kC1 = sr_el(SE_C, 2, 0, 1e-6); sr.el[kC1].ci = 1; sr.el[kC1].x0 = 0.0;
        kC2 = sr_el(SE_C, 2, 0, 3e-6); sr.el[kC2].ci = 1; sr.el[kC2].x0 = 0.0;
        canal(0, CANAL_I, kC1); canal(1, CANAL_I, kC2);
        saj.t_fin = 2e-3; saj.h = 1e-5;
        emula("dos C con V0");
        chk_cond("dos C con V0", "t = 0 es una discontinuidad", muestras_en(0.0) == 2);
        for (j = 1; j <= 20; j++) {
            double t = 0.1e-3 + j * 1e-5, i1 = sim_canal_en(0, t), i2 = sim_canal_en(1, t);
            if (fabs(i2 - 3.0 * i1) > 1e-6 * fabs(i2)) alterna = 1;
        }
        chk_cond("dos C con V0", "i(C2) = 3 i(C1) en cada paso (sin oscilar)", !alterna);
    }

    /* ---- Alineación: un pulso en t0 = 0,123 ms cae en la malla (h se
     *      achica un poco) y no hace falta partir ningún paso ---- */
    {
        double td = 0.123e-3, k;
        sr_init(3);
        kE = sr_el(SE_E, 0, 1, 0.0);
        sr.el[kE].forma = ONDA_PULSO; sr.el[kE].p[0] = 0.0; sr.el[kE].p[1] = 10.0; sr.el[kE].p[2] = td;
        sr_el(SE_R, 1, 2, 1e3);
        canal(0, CANAL_V, sr_el(SE_C, 2, 0, 1e-6));
        saj.t_fin = 0.0; saj.h = 0.0;
        emula("alineación");
        r = sim_res();
        k = td / r->h;
        chk_cond("alineación", "t0 es un múltiplo entero de h", fabs(k - floor(k + 0.5)) < 1e-9);
        {
            /* la muestra de antes del evento está a un paso entero: no se
             * partió ningún paso para llegar a t0 */
            const SimReg *g = sim_reg();
            int j;
            for (j = 1; j < g->n && g->t[j] != td; j++) ;
            chk_cond("alineación", "el evento cae tras un paso entero (sin paso partido)",
                     j < g->n && fabs(td - g->t[j - 1] - r->h) <= 1e-9 * r->h);
        }
    }

    /* ---- CI_SALTA: E = 10 ideal sobre C con V0 = 0: suceso capacitivo en 0 ---- */
    sr_init(2);
    sr_cc(0, 1, 10.0);
    kC = sr_el(SE_C, 1, 0, 1e-6); sr.el[kC].ci = 1; sr.el[kC].x0 = 0.0;
    canal(0, CANAL_V, kC);
    saj.t_fin = 1e-3; saj.h = 1e-5;
    emula("CI_SALTA");
    r = sim_res();
    chk_cond("CI_SALTA", "aviso y suceso capacitivo en t = 0", (r->d.avisos & SIM_AV_CI_SALTA) && r->nsucesos == 1 &&
             r->suceso[0].tipo == SUC_IMPULSO_C && r->suceso[0].t == 0.0 && r->suceso[0].elem == kC);
    chk_t("CI_SALTA", "v(0+) = 10", sim_canal_en(0, 0.0), 10.0, 10.0, 1e-9);
}

/* --------------------------------------------------------------------------
 *  36. Modos, paso y ventana automáticos
 * ----------------------------------------------------------------------- */
static void rlc_serie(double R)
{
    sr_init(4);
    sr_escalon(0, 1, 10.0);
    sr_el(SE_R, 1, 2, R);
    sr_el(SE_L, 2, 3, 10e-3);
    canal(0, CANAL_V, sr_el(SE_C, 3, 0, 1e-6));
}

static void test_modos_auto(void)
{
    static const double H0[5] = { 1e-6, 1e-5, 1e-4, 1e-2, 1.0 };
    SimModo m[MAX_EL];
    const SimRes *r;
    int n, j, kE, kC;
    (void)kE;

    printf("[36] Modos, paso y ventana automáticos\n");
    for (j = 0; j < 5; j++) {
        char q[48];
        sprintf(q, "modos RLC con h0 = %g", H0[j]);
        rlc_serie(20.0);
        sim_modos_prueba(&sr, H0[j], m, &n);
        chk_cond(q, "R = 20: un par complejo", n == 1);
        chk_t(q, "R = 20: -1000", m[0].re, -1000.0, 1000.0, 1e-9);
        chk_t(q, "R = 20: ± j9949,8743710662", m[0].im, 9949.8743710662, 9949.8743710662, 1e-9);
        rlc_serie(200.0);
        sim_modos_prueba(&sr, H0[j], m, &n);
        {
            double al = n == 2 ? -0.5 * (m[0].re + m[1].re) : -m[0].re;
            double wn = n == 2 ? sqrt(m[0].re * m[1].re) : sqrt(m[0].re * m[0].re + m[0].im * m[0].im);
            chk_cond(q, "R = 200: -10000 doble (uno o dos)", n == 1 || n == 2);
            chk_t(q, "R = 200: alfa = 10000", al, 10000.0, 10000.0, 1e-6);
            chk_t(q, "R = 200: zeta = 1", al / wn, 1.0, 1.0, 1e-6);
        }
        rlc_serie(1000.0);
        sim_modos_prueba(&sr, H0[j], m, &n);
        chk_cond(q, "R = 1000: dos reales", n == 2 && m[0].im == 0.0 && m[1].im == 0.0);
        if (n == 2) {
            double a = m[0].re > m[1].re ? m[0].re : m[1].re, b = m[0].re > m[1].re ? m[1].re : m[0].re;
            chk_t(q, "R = 1000: -1010,2051443364398", a, -1010.2051443364398, 1010.2, 1e-9);
            chk_t(q, "R = 1000: -98989,79485566355", b, -98989.79485566355, 98989.8, 1e-9);
        }
    }
    /* LC: ±j1e4 */
    sr_init(2);
    kC = sr_el(SE_C, 1, 0, 1e-6); sr.el[kC].ci = 1; sr.el[kC].x0 = 5.0;
    sr_el(SE_L, 1, 0, 10e-3);
    sim_modos_prueba(&sr, 1e-4, m, &n);
    chk_cond("modos LC", "un par", n == 1);
    chk_t("modos LC", "±j1e4", m[0].im, 1e4, 1e4, 1e-9);
    chk_t("modos LC", "sin amortiguar", m[0].re, 0.0, 1e4, 1e-9);
    /* C en serie: el modo constante se descarta */
    sr_init(4);
    sr_escalon(0, 1, 1.0);
    sr_el(SE_R, 1, 2, 1e3);
    sr_el(SE_C, 2, 3, 1e-6);
    sr_el(SE_C, 3, 0, 1e-6);
    sim_modos_prueba(&sr, 1e-4, m, &n);
    chk_cond("modos C en serie", "un modo (el de la carga se descarta)", n == 1);
    chk_t("modos C en serie", "tau = R·C1C2/(C1+C2)", -1.0 / m[0].re, 0.5e-3, 0.5e-3, 1e-9);
    /* C sobre E: el algebraico se descarta */
    sr_init(3);
    sr_cc(0, 1, 1.0);
    sr_el(SE_C, 1, 0, 1e-6);
    sr_el(SE_R, 1, 2, 1e3);
    sr_el(SE_C, 2, 0, 2e-6);
    sim_modos_prueba(&sr, 1e-4, m, &n);
    chk_cond("modos C sobre E", "un modo (el algebraico se descarta)", n == 1);
    chk_t("modos C sobre E", "tau = R·C2", -1.0 / m[0].re, 2e-3, 2e-3, 1e-9);

    /* ---- Automático ---- */
    sr_init(3);
    sr_escalon(0, 1, 10.0);
    sr_el(SE_R, 1, 2, 1e3);
    canal(0, CANAL_V, sr_el(SE_C, 2, 0, 1e-6));
    emula("auto RC");
    r = sim_res();
    chk_t("auto RC", "t_fin = 5 ms", r->t_fin, 5e-3, 5e-3, 1e-12);
    chk_t("auto RC", "h = 2,5 µs", r->h, 2.5e-6, 2.5e-6, 1e-12);
    chk_cond("auto RC", "automáticos", r->h_auto && r->t_auto);
    rlc_serie(20.0);
    emula("auto RLC");
    chk_t("auto RLC", "t_fin = 5 ms", sim_res()->t_fin, 5e-3, 5e-3, 1e-12);
    sr_init(3);
    kE = sr_el(SE_E, 0, 1, 0.0);
    sr.el[kE].forma = ONDA_CUADRADA; sr.el[kE].p[0] = 5.0; sr.el[kE].p[1] = 1e3; sr.el[kE].p[2] = 50.0; sr.el[kE].p[3] = 5.0;
    sr_el(SE_R, 1, 2, 10e3);
    canal(0, CANAL_V, sr_el(SE_C, 2, 0, 1e-6));
    emula("auto integrador");
    r = sim_res();
    chk_t("auto integrador", "t_fin = 50 ms", r->t_fin, 50e-3, 50e-3, 1e-12);
    chk_t("auto integrador", "h = T/50 = 20 µs (una cuadrada es constante a trozos)", r->h, 20e-6, 20e-6, 1e-12);
    sr_init(2);
    kC = sr_el(SE_C, 1, 0, 1e-6); sr.el[kC].ci = 1; sr.el[kC].x0 = 5.0;
    sr_el(SE_L, 1, 0, 10e-3);
    canal(0, CANAL_V, kC);
    emula("auto LC");
    chk_t("auto LC", "t_fin = 5 ms", sim_res()->t_fin, 5e-3, 5e-3, 1e-12);
    /* nada cambia */
    sr_init(2);
    sr_cc(0, 1, 10.0);
    sr_el(SE_R, 1, 0, 1e3);
    emula("auto quieto");
    chk_cond("auto quieto", "10 ms y el aviso QUIETO", sim_res()->t_fin == 10e-3 && (sim_res()->d.avisos & SIM_AV_QUIETO));

    /* ---- El peor caso (el del banco): 20 nodos, 32 elementos RLC y una
     *      cuadrada, por la API del Circuito y con los canales automáticos ---- */
    {
        static Circuito pc;
        static Red pr;
        SimDiag dd;
        banco_peor_sim(&pc, &pr);
        chk_cond("peor caso", "32 elementos que se pueden emular", pc.nel == 32 && sim_valida(&pc, &dd) == SIM_OK);
        chk_cond("peor caso", "sim_iniciar con el Circuito", sim_iniciar(&pc, &pr) == SIM_OK);
        while (sim_avanzar(SIM_OPS_TICK)) ;
        r = sim_res();
        printf("    peor caso: %ld pasos, h = %g, t_fin = %g, estimado %.2f s\n", r->pasos, r->h, r->t_fin, r->t_est);
        chk_cond("peor caso", "N <= 20000 y muestras <= el tope", r->pasos <= SIM_MAX_PASOS && sim_reg()->n <= SIM_MAX_MUESTRAS);
        chk_cond("peor caso", "estimado <= 2,5 s, con PASO_LIM", r->t_est <= 2.5 && (r->d.avisos & SIM_AV_PASO_LIM));
        /* Tiene modos de τ < 5h: tras cada uno de sus 100 flancos va el tramo
         * rígido (§17.3), que cuenta en la estimación de los 2 s (§17.4). Con
         * 5 ms no cabe (se estimaban 4,8 s; lo real, por las operaciones, 5,0
         * s): la ventana baja a 2 ms (20 periodos) y avisa NO_ESTAC. Antes,
         * 3400 pasos en 5 ms, estimado 1,96 s (lo real, 2,5 s) */
        chk_cond("peor caso", "el tramo cuenta en los 2 s: la ventana baja de 5 a 2 ms (NO_ESTAC)",
                 r->t_fin == 2e-3 && (r->d.avisos & SIM_AV_NO_ESTAC));
        chk_cond("peor caso", "acabó completo", r->fin == FIN_COMPLETO);
        chk_cond("peor caso", "canal 1 automático: la V del primer C", r->canal[0].automatico && r->canal[0].tipo == CANAL_V &&
                 pc.el[r->canal[0].elem].tipo == EL_COND && r->canal[0].nombre[0] == 'V');
        chk_cond("peor caso", "canal 2: la I de la primera L", r->canal[1].tipo == CANAL_I && pc.el[r->canal[1].elem].tipo == EL_BOB);
    }
}

/* --------------------------------------------------------------------------
 *  37. El osciloscopio: funciones puras sobre registros sintéticos
 * ----------------------------------------------------------------------- */
static SimReg greg;
static SimRes gres;

/* Cierra un registro sintético: integrales acumuladas y extremos */
static void cierra_reg(SimReg *g)
{
    int i, k;
    for (k = 0; k < 2; k++) {
        g->s1[k][0] = g->s2[k][0] = 0.0;
        g->ymin[k] = g->ymax[k] = g->y[k][0];
        for (i = 1; i < g->n; i++) {
            double dt = g->t[i] - g->t[i - 1], a = g->y[k][i - 1], b = g->y[k][i];
            g->s1[k][i] = g->s1[k][i - 1] + 0.5 * (a + b) * dt;
            g->s2[k][i] = g->s2[k][i - 1] + 0.5 * (a * a + b * b) * dt;
            if (b < g->ymin[k]) g->ymin[k] = b;
            if (b > g->ymax[k]) g->ymax[k] = b;
        }
    }
    g->gen++;
}

static void pon(SimReg *g, double t, double y0, double y1)
{
    g->t[g->n] = t; g->y[0][g->n] = y0; g->y[1][g->n] = y1; g->n++;
}

static void test_osc(void)
{
    static Osc o;
    Medidas m;
    char b[40];
    int i, n, c, solo, sigue;
    double t;

    printf("[37] Osciloscopio (registros sintéticos)\n");

    /* ---- num_pref ---- */
    chk_cond("num_pref", "6,32V", strcmp(num_pref(b, sizeof b, 6.321, 3, "V"), "6,32V") == 0);
    chk_cond("num_pref", "-500mA", strcmp(num_pref(b, sizeof b, -0.5, 3, "A"), "-500mA") == 0);
    chk_cond("num_pref", "1,59kHz", strcmp(num_pref(b, sizeof b, 1591.55, 3, "Hz"), "1,59kHz") == 0);
    chk_cond("num_pref", "2 ms (con espacio)", strcmp(num_pref(b, sizeof b, 2e-3, 3, " s"), "2 ms") == 0);
    chk_cond("num_pref", "0V", strcmp(num_pref(b, sizeof b, 0.0, 3, "V"), "0V") == 0);
    chk_cond("num_pref", "999,7 a 3 cifras: 1kV", strcmp(num_pref(b, sizeof b, 999.7, 3, "V"), "1kV") == 0);
    chk_cond("num_pref", "2,5" S_MU "s", strcmp(num_pref(b, sizeof b, 2.5e-6, 3, "s"), "2,5" S_MU "s") == 0);
    chk_cond("num_pref", "ancho <= cifras + 3 + unidad", strlen(num_pref(b, sizeof b, -123.456e-9, 3, "A")) <= 3 + 3 + 1);
    chk_cond("num_pref", "NaN: ?", strcmp(num_pref(b, sizeof b, sqrt(-1.0), 3, "V"), "?") == 0);

    /* ---- 10 sen(2 pi 1000 t): 200 muestras por periodo, 5 periodos ---- */
    greg.n = 0;
    for (i = 0; i <= 1000; i++) { t = i * 5e-6; pon(&greg, t, 10.0 * sin(2.0 * PI_T * 1000.0 * t), 10.0 * sin(2.0 * PI_T * 1000.0 * t + PI_T / 6.0)); }
    cierra_reg(&greg);
    osc_medidas(&greg, 0, 0.0, 5e-3, &m);
    chk_t("medidas seno", "máx 10", m.max, 10.0, 10.0, 1e-12);
    chk_t("medidas seno", "mín -10", m.min, -10.0, 10.0, 1e-12);
    chk_t("medidas seno", "p-p 20", m.pp, 20.0, 20.0, 1e-12);
    chk_t("medidas seno", "media 0", m.media, 0.0, 1.0, 1e-12);
    chk_t("medidas seno", "eficaz 10/sqrt 2", m.eficaz, 7.0710678118654755, 7.07, 1e-9);
    chk_t("medidas seno", "f = 1000", m.frec, 1000.0, 1000.0, 1e-9);
    chk_cond("medidas seno", "hay y hay frecuencia", m.hay && m.hay_frec);
    /* disparo: sen(2 pi 1000 t + 30°) cruza 0 subiendo en (k + 11/12) ms */
    chk_cond("evento", "el primero", osc_evento(&greg, 1, 0.0, 1, 0.0, 1, &t));
    chk_t("evento", "en 11/12 ms", t, 11.0 / 12.0 * 1e-3, 1e-3, 1e-9);
    chk_cond("evento", "el siguiente", osc_evento(&greg, 1, 0.0, 1, t, 1, &t));
    chk_t("evento", "en 1 + 11/12 ms", t, (1.0 + 11.0 / 12.0) * 1e-3, 1e-3, 1e-9);
    chk_cond("evento", "hacia atrás desde 3 ms", osc_evento(&greg, 1, 0.0, 1, 3e-3, -1, &t));
    chk_t("evento", "en 2 + 11/12 ms", t, (2.0 + 11.0 / 12.0) * 1e-3, 1e-3, 1e-9);
    chk_cond("evento", "bajando: en 5/12 ms", osc_evento(&greg, 1, 0.0, 0, 0.0, 1, &t) && fabs(t - 5.0 / 12.0 * 1e-3) < 1e-12);
    chk_cond("evento", "no cruza 20", !osc_evento(&greg, 1, 20.0, 1, 0.0, 1, &t));
    /* picos */
    chk_cond("pico", "el primer máximo", osc_pico(&greg, 0, 0.0, 1, &t, &m.max));
    chk_t("pico", "en 0,25 ms", t, 0.25e-3, 1e-3, 1e-9);
    chk_t("pico", "vale 10", m.max, 10.0, 10.0, 1e-9);
    chk_cond("pico", "el siguiente mínimo", osc_pico(&greg, 0, t, 0, &t, &m.min) && fabs(t - 0.75e-3) < 1e-9 && fabs(m.min + 10.0) < 1e-9);
    chk_cond("pico", "no hay más máximos tras el último", !osc_pico(&greg, 0, 4.3e-3, 1, &t, &m.max));
    /* columnas: continuas */
    osc_columnas(&greg, 0, 0.0, 5e-3 / 300.0, 300, o.mn[0], o.mx[0], o.ok[0]);
    sigue = 1;
    for (c = 0; c + 1 < 300; c++)
        if (!(o.ok[0][c] && o.ok[0][c + 1] && o.mx[0][c] >= o.mn[0][c + 1] && o.mn[0][c] <= o.mx[0][c + 1])) sigue = 0;
    chk_cond("columnas", "las vecinas se tocan (traza continua)", sigue);
    osc_columnas(&greg, 0, 4e-3, 5e-3 / 300.0, 300, o.mn[0], o.mx[0], o.ok[0]);
    chk_cond("columnas", "después del registro no hay dato", o.ok[0][100] == 0 && o.ok[0][50] == 1);

    /* ---- frecuencia con rizado: 10 sen(1 kHz) + 0,3 sen(50 kHz): la
     *      histéresis del 5 % no cuenta los cruces del rizado ---- */
    greg.n = 0;
    for (i = 0; i <= 5000; i++) { t = i * 1e-6; pon(&greg, t, 10.0 * sin(2.0 * PI_T * 1e3 * t) + 0.3 * sin(2.0 * PI_T * 5e4 * t), 0.0); }
    cierra_reg(&greg);
    osc_medidas(&greg, 0, 0.0, 5e-3, &m);
    chk_t("medidas con rizado", "f = 1000 (la del seno, no la del rizado)", m.frec, 1000.0, 1000.0, 1e-3);

    /* ---- un pico entre dos muestras: 10 sen(2 pi 1000 t + 0,3), cada 5 µs ---- */
    greg.n = 0;
    for (i = 0; i <= 400; i++) { t = i * 5e-6; pon(&greg, t, 10.0 * sin(2.0 * PI_T * 1e3 * t + 0.3), 0.0); }
    cierra_reg(&greg);
    {
        double tp, vp, te = (PI_T / 2.0 - 0.3) / (2.0 * PI_T * 1e3);
        chk_cond("pico entre muestras", "lo encuentra", osc_pico(&greg, 0, 0.0, 1, &tp, &vp));
        chk_t("pico entre muestras", "refinado: t exacto a 1 ns", tp, te, 1e-3, 1e-6);
        chk_t("pico entre muestras", "refinado: v = 10 a 1e-5", vp, 10.0, 10.0, 1e-5);
    }

    /* ---- cuadrada de 0 a 5 con D = 25 %, dos muestras por flanco ---- */
    greg.n = 0;
    for (i = 0; i < 4; i++) {
        double t0 = i * 1e-3;
        int j;
        pon(&greg, t0, 0.0, 0.0); pon(&greg, t0, 5.0, 0.0);
        for (j = 1; j < 25; j++) pon(&greg, t0 + j * 1e-5, 5.0, 0.0);
        pon(&greg, t0 + 0.25e-3, 5.0, 0.0); pon(&greg, t0 + 0.25e-3, 0.0, 0.0);
        for (j = 26; j < 100; j++) pon(&greg, t0 + j * 1e-5, 0.0, 0.0);
    }
    pon(&greg, 4e-3, 0.0, 0.0);
    cierra_reg(&greg);
    osc_medidas(&greg, 0, 0.0, 4e-3, &m);
    chk_t("medidas cuadrada", "media 1,25", m.media, 1.25, 1.25, 1e-12);
    chk_t("medidas cuadrada", "eficaz 2,5", m.eficaz, 2.5, 2.5, 1e-12);
    chk_t("medidas cuadrada", "f = 1 kHz", m.frec, 1000.0, 1000.0, 1e-9);
    chk_t("valor", "en el flanco, el de después", osc_valor(&greg, 0, 1e-3), 5.0, 5.0, 0.0);
    chk_t("valor", "en la bajada, el de después", osc_valor(&greg, 0, 1.25e-3), 0.0, 5.0, 0.0);
    osc_columnas(&greg, 0, 0.0, 4e-3 / 300.0, 300, o.mn[0], o.mx[0], o.ok[0]);
    sigue = 1;
    for (c = 0; c < 300; c++) if (o.mn[0][c] != o.mn[0][c] || o.mx[0][c] != o.mx[0][c]) sigue = 0;
    chk_cond("columnas", "con muestras repetidas: ningún NaN", sigue);

    /* ---- triangular de 0 a 1 ---- */
    greg.n = 0;
    for (i = 0; i <= 2000; i++) { double f = fmod(i / 500.0, 1.0); pon(&greg, i * 2e-6, f < 0.5 ? 2.0 * f : 2.0 - 2.0 * f, 0.0); }
    cierra_reg(&greg);
    osc_medidas(&greg, 0, 0.0, 4e-3, &m);
    chk_t("medidas triangular", "media 0,5", m.media, 0.5, 1.0, 1e-4);
    chk_t("medidas triangular", "eficaz 1/sqrt 3", m.eficaz, 0.57735, 1.0, 1e-4);

    /* ---- un pico de una sola muestra entre 10 000 ceros ---- */
    greg.n = 0;
    for (i = 0; i <= 10000; i++) pon(&greg, i * 1e-6, i == 5003 ? 100.0 : 0.0, 0.0);
    cierra_reg(&greg);
    osc_columnas(&greg, 0, 0.0, 10e-3 / 300.0, 300, o.mn[0], o.mx[0], o.ok[0]);
    for (c = 0, n = 0, solo = -1; c < 300; c++) if (o.mx[0][c] == 100.0) { n++; solo = c; }
    chk_cond("columnas", "un pico de una muestra: solo su columna llega a 100", n == 1 && solo == (int)(5.003e-3 / (10e-3 / 300.0)));

    /* ---- osc_pix ---- */
    chk_cond("osc_pix", "1e6 con 1 nV/div: arriba, dentro", osc_pix(1e6, 1e-9, 0) >= 17 && osc_pix(1e6, 1e-9, 0) <= 187);
    chk_cond("osc_pix", "-1e6: abajo, dentro", osc_pix(-1e6, 1e-9, 0) >= 17 && osc_pix(-1e6, 1e-9, 0) <= 187);
    chk_cond("osc_pix", "NaN: dentro", osc_pix(sqrt(-1.0), 1.0, 0) >= 17 && osc_pix(sqrt(-1.0), 1.0, 0) <= 187);
    chk_cond("osc_pix", "0 en el centro", osc_pix(0.0, 1.0, 0) == OSC_YC);

    /* ---- autoescala del RC: 0 a 9,933 V en 5 ms ---- */
    greg.n = 0;
    for (i = 0; i <= 2000; i++) { t = i * 2.5e-6; pon(&greg, t, 10.0 * (1.0 - exp(-t / 1e-3)), 1e-2 * exp(-t / 1e-3)); }
    cierra_reg(&greg);
    memset(&gres, 0, sizeof gres);
    gres.t_fin = gres.t_pedido = 5e-3; gres.fin = FIN_COMPLETO;
    gres.canal[0].tipo = CANAL_V; gres.canal[1].tipo = CANAL_V;
    strcpy(gres.canal[0].nombre, "V(C1)"); strcpy(gres.canal[1].nombre, "V(R1)");
    osc_nuevo(&o, &greg, &gres, 1);
    chk_t("autoescala", "0,5 ms/div", ESCALA[o.itdiv], 0.5e-3, 0.5e-3, 1e-12);
    chk_t("autoescala", "2 V/div", ESCALA[o.ivdiv[0]], 2.0, 2.0, 1e-12);
    chk_cond("autoescala", "pos = -5", o.pos[0] == -5);
    chk_cond("autoescala", "dos tensiones: la misma escala y el mismo cero", o.ivdiv[1] == o.ivdiv[0] && o.pos[1] == o.pos[0]);
    gres.canal[1].tipo = CANAL_I;
    osc_nuevo(&o, &greg, &gres, 1);
    chk_t("autoescala", "una corriente de 10 mA: 2 mA/div", ESCALA[o.ivdiv[1]], 2e-3, 2e-3, 1e-12);

    /* ---- teclas ---- */
    gres.canal[1].tipo = CANAL_V;
    osc_nuevo(&o, &greg, &gres, 1);
    osc_tecla(&o, &greg, &gres, T_MAS);
    chk_t("teclas", "[+]: 0,2 ms/div", ESCALA[o.itdiv], 0.2e-3, 0.2e-3, 1e-12);
    for (i = 0; i < 60; i++) osc_tecla(&o, &greg, &gres, T_MAS);
    chk_t("teclas", "[+] no pasa de 1 ns/div", ESCALA[o.itdiv], 1e-9, 1e-9, 1e-12);
    for (i = 0; i < 60; i++) osc_tecla(&o, &greg, &gres, T_MENOS);
    chk_t("teclas", "[-] no pasa de 10 s/div", ESCALA[o.itdiv], 10.0, 10.0, 1e-12);
    osc_tecla(&o, &greg, &gres, T_LETRA('A'));
    chk_t("teclas", "[A]: vuelve a 0,5 ms/div", ESCALA[o.itdiv], 0.5e-3, 0.5e-3, 1e-12);
    osc_tecla(&o, &greg, &gres, T_LETRA('X'));
    osc_tecla(&o, &greg, &gres, T_DIG(1));
    osc_tecla(&o, &greg, &gres, T_LETRA('M'));
    chk_cond("teclas", "escribiendo: 1m", strcmp(o.entrada, "1m") == 0);
    osc_tecla(&o, &greg, &gres, T_ENTER);
    chk_t("teclas", "[X] 1 [M] [enter]: T1 = 1 ms", o.tc[0], 1e-3, 1e-3, 1e-15);
    gfx_textos_reset();
    osc_dibuja(&o, &greg, &gres);
    chk_cond("teclas", "el panel de cursores enseña 6,321V", gfx_texto_visto("6,321V"));
    {
        char ruta[256];
        sprintf(ruta, "%s/osc_cursores.bmp", dir_out);
        gfx_dump_bmp(ruta);
    }
    osc_tecla(&o, &greg, &gres, T_LETRA('M'));
    gfx_textos_reset();
    osc_dibuja(&o, &greg, &gres);
    chk_cond("teclas", "[M]: el panel de medidas", gfx_texto_visto("eficaz"));
    {
        char ruta[256];
        sprintf(ruta, "%s/osc_medidas.bmp", dir_out);
        gfx_dump_bmp(ruta);
    }
    osc_tecla(&o, &greg, &gres, T_LETRA('C'));
    chk_cond("teclas", "[C]: canal 2 activo", o.activo == 1);
    osc_tecla(&o, &greg, &gres, T_ARRIBA);
    chk_cond("teclas", "[arriba]: V/div del activo más pequeño y ajustado", o.ivdiv[1] == o.ivdiv[0] - 1 && o.ajustado);
    chk_cond("teclas", "[=], [tab], [menu] y [esc]", osc_tecla(&o, &greg, &gres, T_IGUAL) == OSC_EMULAR &&
             osc_tecla(&o, &greg, &gres, T_TAB) == OSC_TABLA && osc_tecla(&o, &greg, &gres, T_MENU) == OSC_AJUSTES &&
             osc_tecla(&o, &greg, &gres, T_ESC) == OSC_SALIR);
    gfx_desbordes_reset();
    for (i = 0; i < 400; i++) {
        static const int K[14] = { T_MAS, T_MENOS, T_IZQ, T_DER, T_ARRIBA, T_ABAJO, T_ENTER, T_BORRAR, T_PUNTO };
        int tk = (i % 7 == 0) ? T_LETRA("XPDMCA"[i % 6]) : K[i % 9];
        if (i % 11 == 0) tk |= T_CON_SHIFT;
        osc_tecla(&o, &greg, &gres, tk);
        osc_dibuja(&o, &greg, &gres);
    }
    pruebas++;
    if (gfx_desbordes() != 0) falla("osciloscopio", "textos que se salen de la pantalla", gfx_desbordes(), 0);
}

/* --------------------------------------------------------------------------
 *  38. La interfaz del emulador: [=], sondas, osciloscopio, tabla, esquema
 * ----------------------------------------------------------------------- */
/* Un RC en el editor: E = 10 V hacia abajo, cable, C = 1 µF hacia arriba y
 * R = 1k de vuelta. Deja el editor con el circuito. */
static void dibuja_rc(void)
{
    ui_init();
    TECLAS(T_ENTER, T_ENTER, T_ENTER);                         /* editor nuevo */
    TECLAS(T_LETRA('E'), T_ABAJO, T_DIG(1), T_DIG(0), T_ENTER);
    TECLAS(T_LETRA('W'), T_DER, T_ESC);
    TECLAS(T_LETRA('C'), T_ARRIBA, T_DIG(1), T_LETRA('U'), T_ENTER);
    TECLAS(T_LETRA('R'), T_IZQ, T_DIG(1), T_LETRA('K'), T_ENTER);
}

static int emula_hasta_osc(int ticks_max)
{
    int i;
    for (i = 0; i < ticks_max && ui_pantalla() == P_EMULANDO; i++) ui_tick();
    return ui_pantalla();
}

static void test_ui_emu(void)
{
    const Circuito *cu = ui_circuito();
    static Circuito antes;
    int i;

    printf("[38] Interfaz del emulador\n");
    gfx_desbordes_reset();

    /* ---- [=] con R y E sin [?!]: la calculadora, como siempre ---- */
    ui_init();
    TECLAS(T_ENTER, T_ENTER, T_ENTER);
    TECLAS(T_LETRA('E'), T_ABAJO, T_DIG(1), T_DIG(0), T_ENTER);
    TECLAS(T_LETRA('W'), T_DER, T_ESC);
    TECLAS(T_LETRA('R'), T_ARRIBA, T_DIG(5), T_ENTER, T_LETRA('W'), T_IZQ, T_ESC);
    TECLAS(T_IGUAL);
    chk_cond("ui emu", "R y E sin [?!]: P_RESULTADO", ui_pantalla() == P_RESULTADO);
    TECLAS(T_ESC);
    TECLAS(T_IGUAL);
    TECLAS(T_ESC);

    /* ---- vacío: P_RESULTADO con el error ---- */
    ui_init();
    TECLAS(T_ENTER, T_ENTER, T_ENTER, T_IGUAL);
    chk_cond("ui emu", "vacío: P_RESULTADO", ui_pantalla() == P_RESULTADO);

    /* ---- un RC: la línea de estado, [=] emula y acaba en el osciloscopio ---- */
    dibuja_rc();
    chk_cond("ui emu", "RC dibujado: E, cable, C y R", cu->nel == 3 && cu->el[1].tipo == EL_COND && cu->el[1].val.v == 1e-6 &&
             cu->el[2].val.v == 1e3);
    chk_cond("ui emu", "estado: listo para emular (canales auto)", ve("Listo: pulsa [=] para emular (canales auto)"));
    antes = *cu;
    TECLAS(T_IGUAL);
    chk_cond("ui emu", "[=] con un C: P_EMULANDO", ui_pantalla() == P_EMULANDO && ui_animando() == UI_TRABAJANDO);
    vuelca("emu_emulando");
    chk_cond("ui emu", "tras los ticks, P_OSC", emula_hasta_osc(100) == P_OSC);
    chk_cond("ui emu", "canales auto: V(C1)", ui_simres()->canal[0].automatico && strcmp(ui_simres()->canal[0].nombre, "V(C1)") == 0);
    chk_cond("ui emu", "la leyenda dice auto", ve("auto"));
    chk_t("ui emu", "t_fin automático: 5 ms", ui_simres()->t_fin, 5e-3, 5e-3, 1e-12);
    vuelca("emu_osc");
    TECLAS(T_LETRA('X'), T_DIG(1), T_LETRA('M'), T_ENTER);
    chk_t("ui emu", "[X] 1 [M] [enter]: T1 = 1 ms", ui_osc()->tc[0], 1e-3, 1e-3, 1e-15);
    chk_cond("ui emu", "y el panel dice 6,321V", ve("6,321V"));
    vuelca("emu_osc_cursores");
    TECLAS(T_LETRA('X'));
    i = ui_osc()->itdiv;
    TECLAS(T_MAS);
    chk_cond("ui emu", "[+] baja t/div", ui_osc()->itdiv == i - 1);
    repite(T_MENOS, 60);
    chk_cond("ui emu", "[-] se para en 10 s/div", ESCALA[ui_osc()->itdiv] == 10.0);
    TECLAS(T_LETRA('A'));
    chk_cond("ui emu", "[A]: 0,5 ms/div", ESCALA[ui_osc()->itdiv] == 0.5e-3);
    TECLAS(T_TAB);
    chk_cond("ui emu", "[tab]: la tabla", ui_pantalla() == P_EMU_TABLA);
    vuelca("emu_tabla1");
    TECLAS(T_DER); vuelca("emu_tabla2");
    TECLAS(T_DER); vuelca("emu_tabla3");
    TECLAS(T_DER);
    chk_cond("ui emu", "la página de la energía, con el balance", ve("Entregada por las fuentes"));
    vuelca("emu_tabla4");
    TECLAS(T_DER);
    chk_cond("ui emu", "la respuesta natural: la tau", ve(S_TAU " = 1ms") || ve(S_TAU " = 1 ms"));
    vuelca("emu_tabla5");
    TECLAS(T_TAB);
    chk_cond("ui emu", "[tab]: el esquema emulado", ui_pantalla() == P_EMU_ESQ && ve("Emulado de 0 a 5 ms"));
    vuelca("emu_esquema");
    TECLAS(T_DER);
    vuelca("emu_esquema_capa");
    TECLAS(T_TAB);
    chk_cond("ui emu", "[tab]: vuelta al osciloscopio", ui_pantalla() == P_OSC);
    TECLAS(T_ESC);
    chk_cond("ui emu", "[esc]: al editor sin tocar el circuito", ui_pantalla() == P_EDITOR && memcmp(cu, &antes, sizeof antes) == 0);

    /* ---- con [?!] en la V de C: la calculadora ---- */
    TECLAS(T_TAB, T_TAB);                                   /* selecciona C1 */
    TECLAS(T_ENTER, T_ABAJO, T_ABAJO, T_INCOG, T_ENTER);
    chk_cond("ui emu", "V(C1) marcada con [?!]", cu->el[1].V.est == M_BUSCAR);
    TECLAS(T_IGUAL);
    chk_cond("ui emu", "con [?!]: P_RESULTADO", ui_pantalla() == P_RESULTADO);
    TECLAS(T_ESC);

    /* ---- cancelar: 1 s de ventana, un tick y [esc] ---- */
    dibuja_rc();
    TECLAS(T_MENU); repite(T_ABAJO, 6); TECLAS(T_ENTER);
    chk_cond("ui emu", "la opción 6 del menú: P_AJUSTES", ui_pantalla() == P_AJUSTES);
    vuelca("emu_ajustes");
    TECLAS(T_DIG(1), T_ENTER);
    chk_cond("ui emu", "tiempo total = 1 s", cu->t_fin.est == M_DATO && cu->t_fin.v == 1.0 && ui_pantalla() == P_EDITOR);
    TECLAS(T_IGUAL);
    ui_tick();
    TECLAS(T_ESC);
    chk_cond("ui emu", "[esc] emulando: cortado y al osciloscopio", ui_pantalla() == P_OSC && ui_simres()->fin == FIN_CORTADO &&
             ve("Cortado en t ="));
    vuelca("emu_cortado");
    TECLAS(T_ESC);
    TECLAS(T_MENU, T_ARRIBA, T_ENTER);
    chk_cond("ui emu", "uno arriba desde el 0 sigue siendo Salir", ui_pantalla() == P_MENU);

    /* ---- sondas ---- */
    dibuja_rc();
    TECLAS(T_LETRA('O'), T_ENTER);
    chk_cond("ui emu", "[O][enter]: la tensión del punto en el canal 1", cu->sonda[0].tipo == SONDA_NODO);
    TECLAS(T_LETRA('O'), T_ABAJO);
    chk_cond("ui emu", "[O] y una flecha: la I en el canal 2", cu->sonda[1].tipo == SONDA_I);
    TECLAS(T_LETRA('O'), T_ABAJO);
    chk_cond("ui emu", "otra vez: la V", cu->sonda[1].tipo == SONDA_V);
    vuelca("emu_sondas");
    TECLAS(T_LETRA('O'), T_ABAJO);
    chk_cond("ui emu", "otra vez: se quita", cu->sonda[1].tipo == SONDA_NO);
    TECLAS(T_LETRA('O'), T_DER);
    chk_cond("ui emu", "en otro elemento: canal 2", cu->sonda[1].tipo == SONDA_I);
    TECLAS(T_ABAJO, T_LETRA('O'), T_ARRIBA);
    chk_cond("ui emu", "una tercera sustituye al canal 2", cu->sonda[1].tipo == SONDA_I && cu->sonda[1].el == 0 && ve("Canal 2 movido"));
    TECLAS(T_LETRA('Z') | T_CON_CTRL);
    chk_cond("ui emu", "[ctrl][Z] deshace la sonda", cu->sonda[1].tipo == SONDA_I && cu->sonda[1].el == 2);
    TECLAS(T_IGUAL);
    chk_cond("ui emu", "con sondas: emula con ellas", emula_hasta_osc(100) == P_OSC && !ui_simres()->canal[0].automatico &&
             ui_simres()->canal[0].tipo == CANAL_NODO);
    TECLAS(T_ESC);

    /* ---- la paleta: [catálogo] [ab][ab] [enter] pone una C ---- */
    TECLAS(T_CATALOGO, T_ABAJO, T_ABAJO);
    vuelca("emu_paleta");
    TECLAS(T_ENTER);
    chk_cond("ui emu", "la paleta: C", ve(tipo_info(EL_COND)->al_poner));
    TECLAS(T_ESC);

    /* ---- error: un S que al cerrarse en 2 ms pone a E1 en corto ---- */
    ui_init();
    TECLAS(T_ENTER, T_ENTER, T_ENTER);
    TECLAS(T_LETRA('E'), T_ABAJO, T_DIG(1), T_DIG(0), T_ENTER);     /* (1,1)-(1,2) */
    TECLAS(T_LETRA('W'), T_DER, T_ESC);                               /* (1,2)-(2,2) */
    TECLAS(T_LETRA('R'), T_ARRIBA, T_DIG(1), T_DIG(0), T_ENTER);     /* (2,2)-(2,1) */
    TECLAS(T_LETRA('W'), T_IZQ, T_ESC);                               /* (2,1)-(1,1) */
    TECLAS(T_LETRA('W'), T_IZQ, T_ESC);                               /* (1,1)-(0,1) */
    TECLAS(T_LETRA('S'), T_ABAJO);                                    /* (0,1)-(0,2) */
    chk_cond("ui emu", "S puesto y su ficha abierta", ui_pantalla() == P_ELEMENTO && cu->el[cu->nel - 1].tipo == EL_INTER);
    TECLAS(T_DIG(2), T_LETRA('M'), T_ENTER);
    chk_cond("ui emu", "t1 = 2 ms", cu->el[cu->nel - 1].par[Q_T1 - Q_P1].v == 2e-3);
    TECLAS(T_LETRA('W'), T_DER, T_ESC);                               /* (0,2)-(1,2) */
    TECLAS(T_IGUAL);
    chk_cond("ui emu", "el S en corto: P_EMU_ESQ", ui_pantalla() == P_EMU_ESQ);
    chk_cond("ui emu", "resalta E1 y S1", ui_simres()->d.lazo == ((1u << 0) | (1u << (cu->nel - 1))));
    chk_cond("ui emu", "el texto dice 2 ms", ve("2 ms"));
    vuelca("emu_error");
    TECLAS(T_ESC);
    chk_cond("ui emu", "[esc]: al editor", ui_pantalla() == P_EDITOR);

    pruebas++;
    if (gfx_desbordes() != 0) falla("ui emu", "textos que se salen de la pantalla", gfx_desbordes(), 0);
}

/* --------------------------------------------------------------------------
 *  39. La biblioteca: cada modelo emula y da su clave
 * ----------------------------------------------------------------------- */
static int canal_de_nombre(const char *nom)
{
    const SimRes *r = sim_res();
    int k;
    for (k = 0; k < 2; k++) if (strcmp(r->canal[k].nombre, nom) == 0) return k;
    return -1;
}

/* La clave de un modelo, de lo último que se calculó: la emulación (ca = 0),
 * el punto de operación op (ca = 3) o el barrido (ca = 4, la traza de ida
 * del canal 1) */
static double medida_modelo(const CircModelo *md, const Circuito *c, const Valores *op)
{
    const SimReg *g = sim_reg();
    const SimRes *r = sim_res();
    Medidas me;
    int k = md->canal - 1, e;
    double T = 0.0, t, v;
    if (md->ca == 3) {
        for (e = 0; e < c->nel && !(md->clave_el && strcmp(c->el[e].etiq, md->clave_el) == 0); e++) ;
        if (!op || e >= c->nel) return -1e300;
        switch (md->medida) {
        case 'I': return fabs(op->I[e]);
        case 'V': return op->V[e];                  /* su V, con el signo de su tabla */
        case 'L': return c->el[e].tipo == EL_AMP ? op->I[e] : op->V[e];   /* lo que marca: un A, su I */
        default:  return -1e300;
        }
    }
    if (md->ca == 4) {
        const BarrReg *b = barr_reg();
        int tr = -1, sg;
        for (e = 0; e < b->ntr && tr < 0; e++) if (b->canal[e] == k && b->recorrido[e] == 0) tr = e;
        if (tr < 0) return -1e300;
        switch (md->medida) {
        case 'x': return barr_nivel_dato(tr, md->t, barr_res()->x0, barr_res()->x1, &t, &sg) ? t : -1e300;
        case 'y': return barr_en(tr, md->t, &v, 0) ? v : -1e300;
        case 'g': return barr_en(tr, md->t, &v, &t) ? t : -1e300;
        default:  return -1e300;
        }
    }
    for (e = 0; e < c->nel; e++)
        if ((c->el[e].tipo == EL_FUENTE || c->el[e].tipo == EL_CORR) && c->el[e].variante != ONDA_CC)
            T = 1.0 / c->el[e].par[Q_FREC - Q_P1].v;
    switch (md->medida) {
    case 'v': return osc_valor(g, k, md->t);
    case 'M': osc_medidas(g, k, 0.0, r->t_fin, &me); return me.max;
    case 'm': osc_medidas(g, k, 0.0, r->t_fin, &me); return me.min;
    case 'U': osc_medidas(g, k, r->t_fin - T, r->t_fin, &me); return me.max;
    case 'u': osc_medidas(g, k, r->t_fin - T, r->t_fin, &me); return me.min;
    case 'P': osc_medidas(g, k, r->t_fin - T, r->t_fin, &me); return me.pp;
    case 'A': osc_medidas(g, k, r->t_fin - T, r->t_fin, &me); return me.media;
    case 'F': osc_medidas(g, k, 0.0, r->t_fin, &me); return me.frec;
    case 'L':
        for (e = 0; e < c->nel; e++) if (strcmp(c->el[e].etiq, md->clave_el) == 0) return r->lectura[e];
        return -1e300;
    case 'W':
        for (e = 0; e < c->nel; e++) if (strcmp(c->el[e].etiq, md->clave_el) == 0) return r->Efin[e];
        return -1e300;
    default: (void)t; (void)v; return -1e300;
    }
}

static void test_biblioteca(void)
{
    static Circuito c;
    static Red r;
    int i, x0;

    /* los doce de la Etapa 2 (los de los semiconductores, en [66]) */
    for (i = 0, x0 = 0; i < 12 && i < N_MODELOS; i++) if (MODELOS[i].ca == 0) x0++;
    printf("[39] Biblioteca: %d circuitos modelo en el tiempo\n", x0);
    gfx_desbordes_reset();
    for (i = 0; i < 12 && i < N_MODELOS; i++) {
        const CircModelo *md = &MODELOS[i];
        const SimRes *res;
        char q[64];
        double x;
        if (md->ca) continue;                        /* los de la alterna: [53] */
        sprintf(q, "modelo %d", i + 1);
        chk_cond(q, "título de 44 columnas o menos", strlen(md->titulo) <= 44);
        chk_cond(q, "enunciado en 3 líneas de 52", lineas(md->enunciado, 52) <= 3);
        chk_cond(q, "qué ver en 2 líneas de 52", lineas(md->que_ver, 52) <= 2);
        modelo_cargar(i, &c);
        chk_cond(q, "circuito válido", circ_valido(&c));
        chk_cond(q, "dinámico: [=] lo emula", circ_dinamico(&c) && !circ_pide(&c));
        red_extraer(&c, &r);
        chk_cond(q, "emula sin error", sim_iniciar(&c, &r) == SIM_OK);
        while (sim_avanzar(SIM_OPS_TICK)) ;
        res = sim_res();
        chk_cond(q, "completo", res->fin == FIN_COMPLETO);
        printf("    %-40s %5ld pasos, h = %-9.3g estimado %.2f s\n", md->titulo, res->pasos, res->h, res->t_est);
        /* menos de 1 s: con lo fijo de un paso medido en la calculadora (43,45
         * µs sin el USB, BN_SIM; antes 20 a ojo) y la ventana de medida contada,
         * el integrador RC (6400 pasos) se estima en 0,60 s y el derivador
         * (5070) en 0,44 (con el banco del USB, 0,78 y 0,60); antes cabían en 0,5 */
        chk_cond(q, "en la calculadora, menos de 1 s (estimado)", res->t_est < 1.0);
        x = medida_modelo(md, &c, 0);
        pruebas++;
        if (!(fabs(x - md->clave) <= md->tol * fabs(md->clave))) {
            char d[96];
            sprintf(d, "clave '%c' = %.10g", md->medida, md->clave);
            falla(q, d, x, md->clave);
        }
        /* las otras claves de la tabla */
        switch (i) {
        case 6: {                                   /* LC */
            Medidas me;
            int kL = canal_de_nombre("I(L1)");
            osc_medidas(sim_reg(), kL < 0 ? 1 : kL, 0.0, res->t_fin, &me);
            chk_t(q, "i máx = 0,2 A", me.max, 0.2, 0.2, 1e-3);
            chk_t(q, "W final = 200 µJ", res->Efin[0] + res->Efin[1], 200e-6, 200e-6, 5e-3);
            break;
        }
        case 7: {                                   /* integrador */
            Medidas me;
            osc_medidas(sim_reg(), 0, res->t_fin - 1e-3, res->t_fin, &me);
            chk_t(q, "media del último periodo = 5 V", me.media, 5.0, 5.0, 1e-3);
            break;
        }
        case 8: {                                   /* derivador */
            Medidas me;
            osc_medidas(sim_reg(), 0, 0.0, res->t_fin, &me);
            chk_t(q, "máx 10", me.max, 10.0, 10.0, 1e-3);
            chk_t(q, "mín -10", me.min, -10.0, 10.0, 1e-3);
            break;
        }
        case 9:                                     /* patada */
            chk_t(q, "i_L(0,51 ms) = 0,3678794", osc_valor(sim_reg(), 1, 0.51e-3), 0.3678794, 0.3678794, 1e-3);
            break;
        case 10:                                    /* interruptor */
            chk_t(q, "v(4 ms) = 1,805707", osc_valor(sim_reg(), 0, 4e-3), 1.805707, 1.805707, 1e-3);
            break;
        case 11: {                                  /* senoidal */
            int kV, k;
            double te = 0.0, tc = 0.0, T = 1.0 / 159.155;
            for (kV = 0; kV < c.nel && strcmp(c.el[kV].etiq, "V1") != 0; kV++) ;
            chk_t(q, "en CA, la media no cuenta: la CC es ~0", res->Vmed[kV], 0.0, 1.0, 1e-3);
            /* desfase: cruces de subida de e (canal 2) y de v_C (canal 1) */
            k = osc_evento(sim_reg(), 1, 0.0, 1, res->t_fin - 3.0 * T, 1, &te) &&
                osc_evento(sim_reg(), 0, 0.0, 1, te, 1, &tc);
            chk_cond(q, "los cruces existen", k);
            chk_t(q, "v_C va T/8 detrás de e", tc - te, 0.785398e-3, 0.785398e-3, 0.01);
            break;
        }
        default: break;
        }
    }
    /* pico del modelo 4 con osc_pico: t = 0,65450 ms (±1 paso), v = 14 (±0,1 %) */
    modelo_cargar(3, &c);
    red_extraer(&c, &r);
    sim_iniciar(&c, &r);
    while (sim_avanzar(SIM_OPS_TICK)) continue;
    {
        double tp, vp;
        chk_cond("modelo 4", "osc_pico encuentra el primer máximo", osc_pico(sim_reg(), 0, 0.0, 1, &tp, &vp));
        chk_t("modelo 4", "en t = 0,65450 ms (±1 paso)", tp, 0.65450e-3, sim_res()->h, 1.0);
        chk_t("modelo 4", "v = 14,000 (±0,1 %)", vp, 14.0, 14.0, 1e-3);
    }
    /* En la interfaz: la lección 10, [enter] emula y [tab] lleva al Taller */
    ui_init();
    TECLAS(T_ENTER, T_ABAJO, T_ENTER);                        /* Aprende */
    repite(T_ABAJO, 9);                                         /* la 10 */
    TECLAS(T_ENTER);
    chk_cond("lección 10", "empieza por la teoría", ui_pantalla() == P_TEORIA);
    vuelca("leccion10_teoria");
    TECLAS(T_DER, T_DER);
    chk_cond("lección 10", "y luego la lista de modelos", ui_pantalla() == P_EJEMPLOS && ve("RC: carga de un condensador"));
    vuelca("leccion10_modelos");
    TECLAS(T_ENTER);
    chk_cond("lección 10", "[enter] emula", ui_pantalla() == P_EMULANDO || ui_pantalla() == P_OSC);
    emula_hasta_osc(100);
    chk_cond("lección 10", "y enseña el osciloscopio", ui_pantalla() == P_OSC);
    vuelca("leccion10_osc");
    TECLAS(T_ESC);
    chk_cond("lección 10", "[esc] vuelve a la lista", ui_pantalla() == P_EJEMPLOS);
    TECLAS(T_ABAJO, T_TAB);
    chk_cond("lección 10", "[tab] lo lleva al Taller", ui_pantalla() == P_EDITOR && ui_circuito()->nel == 2 &&
             ui_circuito()->el[1].tipo == EL_COND);
    pruebas++;
    if (gfx_desbordes() != 0) falla("biblioteca", "textos que se salen de la pantalla", gfx_desbordes(), 0);
}

/* --------------------------------------------------------------------------
 *  40. Revisión adversarial de la Etapa 2: cada defecto confirmado, con su
 *      prueba (fallaría sin el arreglo)
 * ----------------------------------------------------------------------- */
static int red_xyz(Circuito *c, double E, int tx, double vx, int ty, double vy, int tz, double vz);

static void test_revision_e2(void)
{
    static Circuito c;
    static Red r;
    static Solucion s;
    char m[260];
    int k, x, y;

    printf("[40] Revisión de la Etapa 2\n");

    /* ---- Con 32 elementos, poner uno sobre un cable no borra el cable ---- */
    circ_init(&c, "lleno");
    for (y = 0, k = 0; y < 5 && c.nel < MAX_EL; y++)
        for (x = 0; x < REJ_COLS - 1 && c.nel < MAX_EL; x++) circ_elemento(&c, EL_RES, x, y, DIR_DER);
    circ_cable(&c, 0, 5, DIR_ABA);
    k = circ_elemento(&c, EL_RES, 0, 5, DIR_ABA);
    chk_cond("lleno", "32 elementos: no cabe otro", c.nel == MAX_EL && k < 0);
    chk_cond("lleno", "y el cable sigue ahí", circ_seg(&c, 0, 5, DIR_ABA) == SEG_CABLE);
    k = circ_elemento(&c, EL_BOB, 0, 0, DIR_DER);
    chk_cond("lleno", "sobre un elemento sí: lo sustituye", k >= 0 && c.nel == MAX_EL && c.el[k].tipo == EL_BOB);

    /* ---- La culpa del corto es de lo que está en el camino entre los bornes:
     *      E1 con un A1 ideal en paralelo y L1 en serie con R1 aparte ---- */
    circ_init(&c, "corto");
    k = circ_elemento(&c, EL_FUENTE, 1, 2, DIR_ARR);
    c.el[k].val.est = M_DATO; c.el[k].val.v = 12.0;
    k = circ_elemento(&c, EL_BOB, 1, 1, DIR_DER); c.el[k].val.est = M_DATO; c.el[k].val.v = 0.1;
    k = circ_elemento(&c, EL_RES, 2, 1, DIR_ABA); c.el[k].val.est = M_DATO; c.el[k].val.v = 4.0;
    circ_cable(&c, 2, 2, DIR_IZQ);
    circ_cable(&c, 1, 1, DIR_IZQ);
    k = circ_elemento(&c, EL_AMP, 0, 1, DIR_ABA);
    c.el[k].I.est = M_BUSCAR;
    circ_cable(&c, 0, 2, DIR_DER);
    red_extraer(&c, &r);
    chk_cond("culpa del corto", "RED_FUENTE_AMP con la culpa en A1, no en L1", r.error == RED_FUENTE_AMP && r.elem_culpa == k);
    resolver(&c, &r, &s);
    sol_mensaje(&c, &r, &s, m, sizeof m);
    chk_cond("culpa del corto", "el mensaje habla del amperímetro, no de la bobina",
             strstr(m, "amperímetro") != 0 && strstr(m, "bobina") == 0 && strstr(m, "L1") == 0);

    /* ---- SOL_INDET en el RC con S abierto: nombra a C1 con «su historia»,
     *      en cualquier orden de dibujo ---- */
    for (x = 0; x < 2; x++) {
        int kC;
        if (x == 0) red_xyz(&c, 10.0, EL_RES, 1e3, EL_INTER, 0.0, -1, 0.0);
        else red_xyz(&c, 10.0, EL_INTER, 0.0, EL_RES, 1e3, -1, 0.0);
        circ_quitar(&c, 1, 1, DIR_IZQ);
        kC = circ_elemento(&c, EL_COND, 1, 1, DIR_IZQ);
        c.el[kC].val.est = M_DATO; c.el[kC].val.v = 1e-6;
        c.el[kC].V.est = M_BUSCAR;
        red_extraer(&c, &r);
        resolver(&c, &r, &s);
        sol_mensaje(&c, &r, &s, m, sizeof m);
        chk_cond("RC con S abierto", x ? "orden E, S, R, C: INDET en C1 con su historia" : "orden E, R, S, C: INDET en C1 con su historia",
                 s.estado == SOL_INDET && s.elem == kC && strstr(m, "historia") != 0);
    }

    /* ---- Residuo de una E real en corto por dos L (todas las V = 0) ---- */
    k = red_xyz(&c, 12.0, EL_BOB, 0.5, EL_BOB, 0.2, -1, 0.0);
    c.el[k].aux.est = M_DATO; c.el[k].aux.v = 2.0;
    c.el[1].I.est = M_BUSCAR;
    red_extraer(&c, &r);
    resolver(&c, &r, &s);
    chk_cond("E real en corto", "se resuelve: I = E/r = 6 A", s.estado == SOL_OK && fabs(fabs(s.s[0].I[1]) - 6.0) < 1e-9);
    if (s.estado == SOL_OK) chk_cond("E real en corto", "residuo pequeño (la escala es la fem)", sol_residuo(&c, &r, &s.s[0]) < 1e-9);
}

/* El mayor error de un canal contra f(t) en las muestras de (t_a, t_b) */
static double err_canal(int c, double (*f)(double), double t_a, double t_b, int con_abs)
{
    const SimReg *g = sim_reg();
    double e = 0.0;
    int i;
    for (i = 0; i < g->n; i++) {
        double y = con_abs ? fabs(g->y[c][i]) : g->y[c][i], d;
        if (!(g->t[i] > t_a && g->t[i] < t_b)) continue;
        d = fabs(y - f(g->t[i]));
        if (d > e) e = d;
    }
    return e;
}
static double f_rl_rapida(double t) { return 0.1 * (1.0 - exp(-t / 1e-4)); }
static double f_diez_ma(double t) { (void)t; return 0.01; }
static double f_l1_serie(double t) { return 5.0 * exp(-t / 2e-4); }
static double f_cero(double t) { (void)t; return 0.0; }
static double f_diez(double t) { (void)t; return 10.0; }
static double f_rc_tarde(double t) { return t > 1e-3 + 1e-12 ? 10.0 * (1.0 - exp(-(t - 1e-3 - 1e-12) / 1e-3)) : 0.0; }
static double f_rampa(double t) { return 1.0 - (t - 0.9) * (t - 0.9); }

static void test_revision_e2_motor(void)
{
    static Osc o;
    const SimRes *r;
    int i, x, e, kL, kC, kE, kS, kJ, kR, sin_salto;
    double t, p[5];

    printf("[41] Revisión de la Etapa 2: motor y osciloscopio\n");

    /* ---- Todo con V0/I0 (lo que pone el editor) y fuentes de CC: el modo
     *      de la L (tau = 0,1 ms) que el paso no resuelve no oscila ---- */
    sr_init(4);
    sr_cc(0, 1, 10.0);
    sr_el(SE_R, 1, 2, 100.0);
    kL = sr_el(SE_L, 2, 0, 10e-3); sr.el[kL].ci = 1; sr.el[kL].x0 = 0.0;
    sr_el(SE_R, 1, 3, 10e3);
    kC = sr_el(SE_C, 3, 0, 100e-6); sr.el[kC].ci = 1; sr.el[kC].x0 = 0.0;
    canal(0, CANAL_I, kL); canal(1, CANAL_V, kC);
    emula("CI y CC, L rápida");
    chk_cond("CI y CC, L rápida", "el paso no resuelve la L (h > 2 tau)", sim_res()->h > 2e-4 && sim_res()->h < 3e-3);
    chk_t("CI y CC, L rápida", "i_L sube a 0,1 A sin oscilar", err_canal(0, f_rl_rapida, 3e-3, 1e30, 1), 0.0, 0.1, 1e-3);
    sr.el[0].p[0] = 10.0;
    sr.el[1].val = 1e3; sr.el[kL].val = 1e-3;          /* tau = 1 us: factor del trapecio -0,999 */
    emula("CI y CC, L muy rápida");
    chk_t("CI y CC, L muy rápida", "i_L = 10 mA en toda la ventana", err_canal(0, f_diez_ma, 3e-3, 1e30, 1), 0.0, 0.01, 1e-3);

    /* ---- Un nodo anclado entre dos L con I0: la v_L de la historia no es
     *      la del ancla ---- */
    sr_init(4);
    sr_cc(0, 1, 10.0);
    kL = sr_el(SE_L, 1, 2, 1e-3); sr.el[kL].ci = 1;
    x = sr_el(SE_L, 2, 3, 1e-3); sr.el[x].ci = 1;
    sr_el(SE_R, 3, 0, 10.0);
    canal(0, CANAL_V, kL);
    emula("dos L en serie con I0");
    chk_cond("dos L en serie con I0", "el punto de operación ancla el nodo del medio", (sim_res()->d.avisos & SIM_AV_FLOTA) != 0);
    chk_t("dos L en serie con I0", "v_L1 = 5 e^-t/tau, sin alternar", err_canal(0, f_l1_serie, 0.0, 1e30, 1), 0.0, 5.0, 1e-2);
    /* y una L con I0 = 0 que cuelga de un S abierto */
    sr_init(4);
    sr_cc(0, 1, 10.0);
    sr_el(SE_R, 1, 2, 100.0);
    kL = sr_el(SE_L, 2, 3, 10e-3); sr.el[kL].ci = 1;
    kS = sr_el(SE_S, 3, 0, 0.0); sr.el[kS].cerrado0 = 0; sr.el[kS].t1 = 1e-3;
    canal(0, CANAL_V, kL); canal(1, CANAL_V, kS);
    emula("L con I0 tras un S abierto");
    chk_t("L con I0 tras un S abierto", "antes de t1: v_L = 0", err_canal(0, f_cero, 0.0, 1e-3 * (1.0 - 1e-9), 1), 0.0, 10.0, 1e-4);
    chk_t("L con I0 tras un S abierto", "antes de t1: v_S = 10", err_canal(1, f_diez, 0.0, 1e-3 * (1.0 - 1e-9), 1), 0.0, 10.0, 1e-4);

    /* ---- J en serie con una L con I0: se emula, cuadre o no la I0 ---- */
    sin_salto = 0;
    for (x = 0; x < 2; x++) {
        sr_init(3);
        kJ = sr_el(SE_J, 0, 1, 0.0); sr.el[kJ].forma = ONDA_CC; sr.el[kJ].p[0] = 1.0;
        kL = sr_el(SE_L, 1, 2, 1e-3); sr.el[kL].ci = 1; sr.el[kL].x0 = x ? -1.0 : 1.0;
        sr_el(SE_R, 2, 0, 10.0);
        canal(0, CANAL_I, kL);
        e = sim_emular(&sr, &saj);
        if (e == SIM_OK && !(sim_res()->d.avisos & SIM_AV_CI_SALTA)) sin_salto++;
        chk_cond("J en serie con L", x ? "I0 = -1 A: se emula, i_L = 1 A" : "I0 = 1 A: se emula, i_L = 1 A",
                 e == SIM_OK && fabs(fabs(sim_res()->ultimo.I[kL]) - 1.0) < 1e-6);
    }
    chk_cond("J en serie con L", "solo la I0 que no cuadra avisa del salto", sin_salto == 1);
    sr_init(3);
    kJ = sr_el(SE_J, 0, 1, 0.0); sr.el[kJ].forma = ONDA_PULSO;
    sr.el[kJ].p[0] = 0.0; sr.el[kJ].p[1] = 1.0; sr.el[kJ].p[2] = 1e-3;
    kL = sr_el(SE_L, 1, 2, 1e-3); sr.el[kL].ci = 1;
    sr_el(SE_R, 2, 0, 10.0);
    canal(0, CANAL_I, kL);
    e = sim_emular(&sr, &saj);
    chk_cond("J escalón en serie con L", "se emula: i_L acaba en 1 A", e == SIM_OK && fabs(fabs(sim_res()->ultimo.I[kL]) - 1.0) < 1e-6);

    /* ---- El tope de pasos cuenta los del arranque de cada evento ---- */
    sr_init(3);
    kE = sr_el(SE_E, 0, 1, 0.0); sr.el[kE].forma = ONDA_SENO;
    sr.el[kE].p[0] = 10.0; sr.el[kE].p[1] = 50.0;
    sr_el(SE_R, 1, 2, 1e3);
    kC = sr_el(SE_C, 2, 0, 1e-3);
    canal(0, CANAL_V, kC);
    emula("seno de 50 Hz, tau = 1 s");
    r = sim_res();
    chk_cond("seno de 50 Hz, tau = 1 s", "llega a t_fin sin el tope de pasos",
             r->fin == FIN_COMPLETO && sim_reg()->t[sim_reg()->n - 1] == r->t_fin && r->pasos <= SIM_MAX_PASOS);
    sr.el[kE].forma = ONDA_CUADRADA; sr.el[kE].p[1] = 1e3; sr.el[kE].p[2] = 50.0;
    emula("cuadrada de 1 kHz, tau = 1 s");
    r = sim_res();
    chk_cond("cuadrada de 1 kHz, tau = 1 s", "llega a t_fin sin el tope de pasos",
             r->fin == FIN_COMPLETO && sim_reg()->t[sim_reg()->n - 1] == r->t_fin && r->pasos <= SIM_MAX_PASOS);

    /* ---- Un pulso más estrecho que 1e-6 de su escala no se pierde ---- */
    p[0] = 0.0; p[1] = 10.0; p[2] = 1.0; p[3] = 0.5e-6; p[4] = 0.0;
    chk_cond("pulso estrecho", "por la izquierda del flanco de bajada vale 10",
             onda_valor_lado(ONDA_PULSO, p, 1.0 + 0.5e-6, -1) == 10.0);
    sr_init(3);
    kE = sr_el(SE_E, 0, 1, 0.0); sr.el[kE].forma = ONDA_PULSO;
    for (i = 0; i < 5; i++) sr.el[kE].p[i] = p[i];
    sr_el(SE_R, 1, 2, 1e3);
    kC = sr_el(SE_C, 2, 0, 1e-9);
    canal(0, CANAL_V, kC);
    emula("pulso de 0,5 us en 1 s");
    chk_cond("pulso de 0,5 us en 1 s", "el C se carga (exacto 3,93 V)", sim_res()->Vmax[kC] > 3.0 && sim_res()->Vmax[kC] < 4.5);
    p[4] = 1.0 + 1e-7;                                  /* periódico, con el hueco más estrecho */
    p[2] = 0.0; p[3] = 1.0;
    chk_cond("pulso estrecho", "periódico: por la izquierda del hueco vale 0",
             onda_valor_lado(ONDA_PULSO, p, 1.0 + 1e-7, -1) == 0.0);

    /* ---- QUIETO solo si nada se mueve ---- */
    sr_init(2);
    kJ = sr_el(SE_J, 0, 1, 0.0); sr.el[kJ].forma = ONDA_CC; sr.el[kJ].p[0] = 1e-3;
    kC = sr_el(SE_C, 1, 0, 1e-6); sr.el[kC].ci = 1;
    canal(0, CANAL_V, kC);
    emula("J sobre C");
    chk_cond("J sobre C", "integra: sin el aviso de «nada cambia»", !(sim_res()->d.avisos & SIM_AV_QUIETO));
    sr_init(2);
    sr_cc(0, 1, 1.0);
    kL = sr_el(SE_L, 1, 0, 1e-3); sr.el[kL].ci = 1;
    canal(0, CANAL_I, kL);
    emula("E sobre L");
    chk_cond("E sobre L", "integra: sin el aviso de «nada cambia»", !(sim_res()->d.avisos & SIM_AV_QUIETO));
    sr_init(2);
    sr_cc(0, 1, 1.0);
    sr_el(SE_R, 1, 0, 1e3);
    canal(0, CANAL_NODO, 1);
    emula("E sobre R");
    chk_cond("E sobre R", "ahí sí: nada cambia", (sim_res()->d.avisos & SIM_AV_QUIETO) != 0);

    /* ---- Un modo lento con otro 2e9 veces más rápido no es «constante» ---- */
    sr_init(4);
    sr_escalon(0, 1, 10.0);
    sr_el(SE_R, 1, 2, 1e3);
    kC = sr_el(SE_C, 2, 0, 1e-3);
    kR = sr_el(SE_R, 1, 3, 1.0);
    sr_el(SE_C, 3, 0, 0.5e-9);
    canal(0, CANAL_V, kC);
    emula("modos a 1 s y 0,5 ns");
    r = sim_res();
    chk_t("modos a 1 s y 0,5 ns", "tau_max = 1 s", r->tau_max, 1.0, 1.0, 1e-6);
    chk_cond("modos a 1 s y 0,5 ns", "la ventana ve la carga lenta", r->t_fin >= 4.0);
    {
        /* El balance con el tramo rígido (§17.3; el juez de [68], prueba a).
         * El C de 0,5 nF se carga en 0,5 ns y h = 2,5 ms: la y+ consistente
         * (10 A por la R de 1 Ω) es solo una muestra. Con el trapecio desde
         * ella hasta te + h/2, la R aparecía con W = 31 mJ (disipa ½CV² =
         * 25 nJ) y el balance quedaba un 19 % descuadrado; con los pesos de
         * las etapas de SDIRK2 en el producto V·I, W(R) ~ 2e-12 J y los 25 nJ
         * iban a la diferencia (la revisión: con una fuente de Thévenin no hay
         * impulso: es la R la que los disipa). Con la media de los extremos
         * del subpaso (el primero, la y+) por la carga del método (rq_suma),
         * W(R) = V̄·C·ΔV = ½CV² y, sin L, el balance es exacto. */
        double e12 = 0.5 * 0.5e-9 * 10.0 * 10.0;
        printf("    modos a 1 s y 0,5 ns: W(R de 1 ohm) = %.6g J, diferencia del balance = %.4g J (½CV² = %.3g J; entregada %.4g J)\n",
               r->W[kR], r->dif_balance, e12, r->W[0]);
        pruebas += 2;
        if (!(fabs(r->W[kR] - e12) <= 1e-3 * e12)) falla("modos a 1 s y 0,5 ns", "W de la R de 1 ohm = ½CV² (25 nJ) a 1e-3", r->W[kR], e12);
        if (!(fabs(r->dif_balance) <= 1e-3 * e12))
            falla("modos a 1 s y 0,5 ns", "|diferencia del balance| <= 1e-3·½CV² (sin L, exacto)", r->dif_balance, e12);
    }

    /* ---- Un flanco 1 ps después de un punto de la malla ---- */
    sr_init(3);
    kE = sr_el(SE_E, 0, 1, 0.0); sr.el[kE].forma = ONDA_PULSO;
    sr.el[kE].p[0] = 0.0; sr.el[kE].p[1] = 10.0; sr.el[kE].p[2] = 1e-3 + 1e-12; sr.el[kE].p[3] = 1e-3;
    sr_el(SE_R, 1, 2, 1e3);
    kC = sr_el(SE_C, 2, 0, 1e-6);
    canal(0, CANAL_V, kC);
    saj.h = 10e-6; saj.t_fin = 1.5e-3;
    emula("flanco a 1 ms + 1 ps");
    chk_t("flanco a 1 ms + 1 ps", "v_C(1 ms) = 0: el flanco no se adelanta", y_antes(0, 1e-3), 0.0, 10.0, 1e-4);
    chk_t("flanco a 1 ms + 1 ps", "después, la carga del RC", err_canal(0, f_rc_tarde, 1e-3 + 2e-12, 1.5e-3, 0), 0.0, 10.0, 1e-4);

    /* ---- Osciloscopio ---- */
    greg.n = 0;
    for (i = 0; i <= 2000; i++) { t = i * 5e-4; pon(&greg, t, f_rampa(t), 10.0 - 10.0 * t); }
    cierra_reg(&greg);
    memset(&gres, 0, sizeof gres);
    gres.t_fin = gres.t_pedido = 1.0; gres.fin = FIN_COMPLETO;
    gres.canal[0].tipo = CANAL_V; gres.canal[1].tipo = CANAL_V;
    strcpy(gres.canal[0].nombre, "V(C1)"); strcpy(gres.canal[1].nombre, "V(R1)");
    /* [P] con 1 ns/div y el pico a 0,9 s: la ventana salta de una vez */
    osc_nuevo(&o, &greg, &gres, 1);
    o.itdiv = 0; o.tizq = 0.0;                         /* 1 ns/div */
    osc_tecla(&o, &greg, &gres, T_LETRA('P'));
    chk_cond("osc [P] lejos", "el pico queda en la ventana",
             o.tc[o.cur] >= o.tizq && o.tc[o.cur] <= o.tizq + 10.0 * ESCALA[o.itdiv] + 1e-12 && fabs(o.tc[o.cur] - 0.9) < 1e-6);
    /* cursores a 1e9 columnas de la ventana: se dibujan sin desbordar */
    gfx_desbordes_reset();
    o.cursores = 1; o.tc[0] = 0.0; o.tc[1] = 1.0; o.tizq = 0.5;
    osc_dibuja(&o, &greg, &gres);
    chk_cond("osc cursores lejos", "sin escribir fuera de la pantalla", gfx_desbordes() == 0);
    /* [D] sobre una señal que solo baja: dispara bajando */
    osc_nuevo(&o, &greg, &gres, 1);
    o.activo = 1; o.disparo = DISP_LIBRE;
    osc_tecla(&o, &greg, &gres, T_LETRA('D'));
    chk_cond("osc [D]", "no sube nunca: dispara bajando", o.disparo == DISP_BAJADA && o.aviso[0] == 0);
    /* hacia atrás sin cruce: «antes de» */
    osc_tecla(&o, &greg, &gres, T_IZQ);
    chk_cond("osc [D] hacia atrás", "el aviso dice «antes de»", strstr(o.aviso, "antes de") != 0);
    /* [C] sin el otro canal nombra el que falta */
    gres.canal[0].tipo = CANAL_NADA;
    osc_nuevo(&o, &greg, &gres, 1);
    o.activo = 1;
    osc_tecla(&o, &greg, &gres, T_LETRA('C'));
    chk_cond("osc [C]", "«No hay canal 1»", strstr(o.aviso, "canal 1") != 0);
    gres.canal[0].tipo = CANAL_V;
    /* escribir un tiempo con los cursores apagados: quedan en [0, t_fin] */
    osc_nuevo(&o, &greg, &gres, 1);
    o.cursores = 0; o.tizq = 0.95;
    osc_tecla(&o, &greg, &gres, T_DIG(5));
    chk_cond("osc escribir", "los cursores nacen dentro del registro",
             o.tc[0] >= 0.0 && o.tc[0] <= 1.0 && o.tc[1] >= 0.0 && o.tc[1] <= 1.0);
    osc_tecla(&o, &greg, &gres, T_ESC);
}

/* --------------------------------------------------------------------------
 *  42. Etapa 3: complejos, formato de fasores y LU compleja
 * ----------------------------------------------------------------------- */
static LUC luc_a;

/* Una matriz con la forma de las de un circuito, como en [26] */
static void matriz_circuito(double m[][LU_MAX], int n, double *b)
{
    int i, j;
    for (i = 0; i < n; i++) for (j = 0; j < n; j++) m[i][j] = 0.0;
    for (i = 0; i < n; i++) {
        double g = pow(10.0, -9.0 + 15.0 * (double)azar(1000) / 999.0);
        int vecinos = 1 + (int)azar(3), v;
        m[i][i] += g * (double)(vecinos + 1);
        for (v = 0; v < vecinos; v++) {
            j = (int)azar((unsigned)n);
            if (j == i) continue;
            m[i][j] -= g; m[j][j] += g * 0.5; m[j][i] -= g * 0.5;
        }
        if (azar(5) == 0 && n > 1) {
            j = (int)azar((unsigned)n);
            if (j != i) { m[i][j] += 1.0; m[j][i] += 1.0; }
        }
        b[i] = (double)azar(2001) - 1000.0;
    }
}

/* Un real al azar de 1e-6 a 1e6, con signo */
static double real_azar(void)
{
    return ((double)azar(2000001) - 1000000.0) / 1e6 * pow(10.0, (double)azar(13) - 6.0);
}

static void test_complejos(void)
{
    static double m[LU_MAX][LU_MAX], b[LU_MAX], x[LU_MAX];
    static double m2[LU_MAX][LU_MAX], b2[LU_MAX], x2[LU_MAX];
    static Cx cm[LU_MAX][LU_MAX], cb[LU_MAX], cxs[LU_MAX];
    char t[64];
    int caso, n, i, j, iguales = 0, total = 0, ok, anchos = 1, anchos_c = 1;
    double peor = 0.0, peor2 = 0.0, im, re;

    printf("[42] Etapa 3: complejos, fasores y LU compleja\n");

    /* ---- cx ---- */
    for (caso = 0; caso < 1000; caso++) {
        Cx a = cx(real_azar(), real_azar()), d = cx(real_azar(), real_azar()), q, r;
        if (cx_nulo(d)) continue;
        q = cx_div(a, d);
        r = cx_menos(a, cx_por(q, d));
        if (cx_abs(r) / cx_abs(a) > peor) peor = cx_abs(r) / cx_abs(a);
    }
    chk_t("cx", "a - (a/b)·b en 1000 pares al azar", peor, 0.0, 1.0, 2e-15);
    {
        Cx q = cx_div(cx(1e300, 1e300), cx(1e300, 0.0)), q2 = cx_div(cx(1e300, 1e300), cx(1e300, 1e300));
        chk_cond("cx", "(1e300 + 1e300j)/1e300 = 1 + 1j sin desbordarse", q.re == 1.0 && q.im == 1.0);
        chk_cond("cx", "(1e300 + 1e300j)/(1e300 + 1e300j) = 1", fabs(q2.re - 1.0) < 1e-15 && fabs(q2.im) < 1e-15);
    }
    chk_cond("cx", "arg(-1) = pi", cx_arg(cx(-1.0, 0.0)) == PI_T && cx_arg(cx(-1.0, -0.0)) == PI_T);
    chk_cond("cx", "arg(0) = 0", cx_arg(cx(0.0, 0.0)) == 0.0);
    chk_t("cx", "envuelve(3 pi) = pi", cx_envuelve(3.0 * PI_T), PI_T, 1.0, 1e-15);
    chk_t("cx", "envuelve(-pi) = pi", cx_envuelve(-PI_T), PI_T, 1.0, 1e-15);
    chk_t("cx", "envuelve(-pi/2) = -pi/2", cx_envuelve(-0.5 * PI_T), -0.5 * PI_T, 1.0, 1e-15);
    chk_t("cx", "polar(2, pi/2) = 2j", cx_polar(2.0, 0.5 * PI_T).im, 2.0, 1.0, 1e-15);
    chk_cond("cx", "inv(4) = 0,25 exacto", cx_inv(cx(4.0, 0.0)).re == 0.25);

    /* ---- LU compleja con im = 0: la real, bit a bit ---- */
    for (caso = 0; caso < 120; caso++) {
        int r1, r2, mismo;
        n = 1 + (int)azar(caso < 100 ? 40 : LU_MAX);
        matriz_circuito(m, n, b);
        if (caso % 10 == 9 && n > 2) { for (j = 0; j < n; j++) m[n - 1][j] = m[0][j]; }   /* singular */
        lu_empieza(&lu_a, n); luc_empieza(&luc_a, n);
        for (i = 0; i < n; i++) for (j = 0; j < n; j++) { lu_suma(&lu_a, i, j, m[i][j]); luc_suma(&luc_a, i, j, cx(m[i][j], 0.0)); }
        lu_ops_reset(); luc_ops_reset();
        r1 = lu_factoriza(&lu_a); r2 = luc_factoriza(&luc_a);
        mismo = r1 == r2 && lu_a.col_mala == luc_a.col_mala;
        if (r1 && r2) {
            lu_resuelve(&lu_a, b, x);
            for (i = 0; i < n; i++) cb[i] = cx(b[i], 0.0);
            luc_resuelve(&luc_a, cb, cxs);
            for (i = 0; i < n; i++) if (!(x[i] == cxs[i].re) || cxs[i].im != 0.0 || lu_a.perm[i] != luc_a.perm[i]) mismo = 0;
        }
        if (lu_ops() != luc_ops()) mismo = 0;
        total++;
        if (mismo) iguales++;
    }
    chk_cond("luc", "con im = 0: x, perm, col_mala y operaciones iguales a la LU real", iguales == total);

    /* ---- 200 sistemas complejos al azar ---- */
    peor = 0.0;
    for (caso = 0; caso < 200; caso++) {
        double na = 0.0, nx = 0.0, nr = 0.0;
        int denso = caso % 2;
        n = 1 + (int)azar(LU_MAX);
        luc_empieza(&luc_a, n);
        for (i = 0; i < n; i++) {
            for (j = 0; j < n; j++) cm[i][j] = cx(0.0, 0.0);
            for (j = 0; j < n; j++)
                if (denso || i == j || azar(6) == 0) cm[i][j] = cx(real_azar(), real_azar());
            cb[i] = cx(real_azar(), real_azar());
        }
        for (i = 0; i < n; i++) for (j = 0; j < n; j++) luc_suma(&luc_a, i, j, cm[i][j]);
        if (!luc_factoriza(&luc_a)) continue;
        luc_resuelve(&luc_a, cb, cxs);
        for (i = 0; i < n; i++) {
            Cx s = cx_menos(cx(0.0, 0.0), cb[i]);
            double fila = 0.0;
            for (j = 0; j < n; j++) { s = cx_mas(s, cx_por(cm[i][j], cxs[j])); fila += cx_abs(cm[i][j]); }
            if (fila > na) na = fila;
            if (cx_abs(cxs[i]) > nx) nx = cx_abs(cxs[i]);
            if (cx_abs(s) > nr) nr = cx_abs(s);
        }
        if (nr / (na * nx + 1e-300) > peor) peor = nr / (na * nx + 1e-300);
    }
    chk_t("luc", "residuo de 200 sistemas complejos al azar (n de 1 a 64)", peor, 0.0, 1.0, 1e-13);

    /* ---- Contra la LU real con 2n: [Re -Im; Im Re] ---- */
    peor2 = 0.0;
    for (caso = 0; caso < 60; caso++) {
        double nx = 0.0, dif = 0.0;
        n = 1 + (int)azar(32);
        luc_empieza(&luc_a, n);
        lu_empieza(&lu_a, 2 * n);
        for (i = 0; i < n; i++) {
            for (j = 0; j < n; j++) {
                cm[i][j] = (caso % 2 || azar(4) == 0) ? cx(real_azar() * 1e-3, real_azar() * 1e-3) : cx(0.0, 0.0);
                if (i == j) cm[i][j] = cx_mas(cm[i][j], cx(1e3 * (double)n, 1e3 * (double)n));
            }
            cb[i] = cx(real_azar(), real_azar());
        }
        for (i = 0; i < n; i++)
            for (j = 0; j < n; j++) {
                luc_suma(&luc_a, i, j, cm[i][j]);
                m2[i][j] = cm[i][j].re; m2[i][j + n] = -cm[i][j].im;
                m2[i + n][j] = cm[i][j].im; m2[i + n][j + n] = cm[i][j].re;
            }
        for (i = 0; i < 2 * n; i++) for (j = 0; j < 2 * n; j++) lu_suma(&lu_a, i, j, m2[i][j]);
        for (i = 0; i < n; i++) { b2[i] = cb[i].re; b2[i + n] = cb[i].im; }
        if (!luc_factoriza(&luc_a) || !lu_factoriza(&lu_a)) { peor2 = 1.0; continue; }
        luc_resuelve(&luc_a, cb, cxs);
        lu_resuelve(&lu_a, b2, x2);
        for (i = 0; i < n; i++) {
            if (cx_abs(cxs[i]) > nx) nx = cx_abs(cxs[i]);
            if (cx_abs(cx_menos(cxs[i], cx(x2[i], x2[i + n]))) > dif) dif = cx_abs(cx_menos(cxs[i], cx(x2[i], x2[i + n])));
        }
        if (dif / (nx + 1e-300) > peor2) peor2 = dif / (nx + 1e-300);
    }
    chk_t("luc", "igual que la LU real de 2n incógnitas (n <= 32)", peor2, 0.0, 1.0, 1e-12);

    /* ---- Singulares ---- */
    luc_empieza(&luc_a, 3);
    luc_suma(&luc_a, 0, 0, cx(1.0, 1.0)); luc_suma(&luc_a, 0, 2, cx(2.0, 0.0));
    luc_suma(&luc_a, 1, 0, cx(0.0, 3.0)); luc_suma(&luc_a, 1, 2, cx(1.0, 0.0));
    luc_suma(&luc_a, 2, 0, cx(1.0, 0.0)); luc_suma(&luc_a, 2, 2, cx(0.0, 1.0));
    chk_cond("luc", "columna nula: col_mala = 1", !luc_factoriza(&luc_a) && luc_a.col_mala == 1);
    luc_empieza(&luc_a, 3);
    luc_suma(&luc_a, 0, 0, cx(1.0, 1.0)); luc_suma(&luc_a, 1, 1, cx(0.0, 2.0));
    chk_cond("luc", "fila vacía: col_mala = 2", !luc_factoriza(&luc_a) && luc_a.col_mala == 2);
    luc_empieza(&luc_a, 2);
    luc_suma(&luc_a, 0, 0, cx(1.0, 1.0)); luc_suma(&luc_a, 0, 1, cx(1.0, 1.0));
    luc_suma(&luc_a, 1, 0, cx(1.0, 1.0)); luc_suma(&luc_a, 1, 1, cx(1.0 + 1e-13, 1.0));
    chk_cond("luc", "pivote de 1e-13 relativo: col_mala = 1", !luc_factoriza(&luc_a) && luc_a.col_mala == 1);
    luc_empieza(&luc_a, 1);
    luc_suma(&luc_a, 0, 0, cx(1e-9, 2.0 * PI_T * 1e3 * 1e-12));
    chk_cond("luc", "1 GOhm en paralelo con 1 pF a 1 kHz no es singular", luc_factoriza(&luc_a));
    luc_fuera_reset();
    luc_suma(&luc_a, -1, 0, cx(1.0, 0.0)); luc_suma(&luc_a, 0, 3, cx(1.0, 0.0));
    chk_cond("luc", "el -1 se omite y el 3 (fuera) se cuenta", luc_fuera() == 1);
    luc_fuera_reset();

    /* ---- num_ang ---- */
    chk("num_ang", "-180 -> 180", num_ang(-180.0), 180.0);
    chk("num_ang", "540 -> 180", num_ang(540.0), 180.0);
    chk("num_ang", "-190 -> 170", num_ang(-190.0), 170.0);
    chk("num_ang", "359 -> -1", num_ang(359.0), -1.0);

    /* ---- num_fasor ---- */
    chk_cond("num_fasor", "9,519" S_ANG "37,5" S_DEG " A",
             strcmp(num_fasor(t, sizeof t, 9.518861634600457, 37.510810145539, 0, 0, "A"), "9,519" S_ANG "37,5" S_DEG " A") == 0);
    chk_cond("num_fasor", "rectangular 7,55+j5,80 A",
             strcmp(num_fasor(t, sizeof t, 9.518861634600457, 37.510810145539, 1, 0, "A"), "7,55+j5,80 A") == 0);
    chk_cond("num_fasor", "corto 9,52" S_ANG "38" S_DEG "A",
             strcmp(num_fasor(t, sizeof t, 9.518861634600457, 37.510810145539, 0, 1, "A"), "9,52" S_ANG "38" S_DEG "A") == 0);
    chk_cond("num_fasor", "1,000" S_ANG "-90,0" S_DEG,
             strcmp(num_fasor(t, sizeof t, 1.0, -90.0, 0, 0, ""), "1,000" S_ANG "-90,0" S_DEG) == 0);
    chk_cond("num_fasor", "-180 y 180: 1,000" S_ANG "180" S_DEG,
             strcmp(num_fasor(t, sizeof t, 1.0, -180.0, 0, 0, ""), "1,000" S_ANG "180" S_DEG) == 0 &&
             strcmp(num_fasor(t, sizeof t, 1.0, 180.0, 0, 0, ""), "1,000" S_ANG "180" S_DEG) == 0);
    chk_cond("num_fasor", "1 mA: 1,000" S_ANG "0,0" S_DEG " mA",
             strcmp(num_fasor(t, sizeof t, 1e-3, 0.0, 0, 0, "A"), "1,000" S_ANG "0,0" S_DEG " mA") == 0);
    chk_cond("num_fasor", "rectangular con parte j negativa: 60,0-j60,0 V",
             strcmp(num_fasor(t, sizeof t, 84.8528137423857, -45.0, 1, 0, "V"), "60,0-j60,0 V") == 0);
    chk_cond("num_fasor", "-0,0 no lleva signo", strcmp(num_fasor(t, sizeof t, 2.0, -0.01, 0, 0, "V"), "2,000" S_ANG "0,0" S_DEG " V") == 0);
    for (caso = 0; caso < 100000; caso++) {
        double mo = pow(10.0, -12.0 + 23.0 * (double)azar(100000) / 99999.0) * (1.0 + (double)azar(9));
        double g = ((double)azar(3600001) - 1800000.0) / 1e4;
        static const char *const U[3] = { "V", "A", S_OHM };
        if (mo >= 9.99e11) mo = 9.99e11;
        if ((int)strlen(num_fasor(t, sizeof t, mo, g, (int)azar(2), 0, U[caso % 3])) > 15) anchos = 0;
        if ((int)strlen(num_fasor(t, sizeof t, mo, g, (int)azar(2), 1, U[caso % 3])) > 12) anchos_c = 0;
    }
    chk_cond("num_fasor", "100 000 al azar: 15 columnas como mucho en las tablas", anchos);
    chk_cond("num_fasor", "y 12 en el esquema", anchos_c);

    /* ---- num_lee_cx ---- */
    re = num_lee_cx("30+j40", &im, &ok);
    chk_cond("num_lee_cx", "30+j40", ok && re == 30.0 && im == 40.0);
    re = num_lee_cx("30-j40", &im, &ok);
    chk_cond("num_lee_cx", "30-j40", ok && re == 30.0 && im == -40.0);
    re = num_lee_cx("j40", &im, &ok);
    chk_cond("num_lee_cx", "j40", ok && re == 0.0 && im == 40.0);
    re = num_lee_cx("1,5k+j2m", &im, &ok);
    chk_cond("num_lee_cx", "1,5k+j2m", ok && re == 1500.0 && fabs(im - 0.002) < 1e-18);
    re = num_lee_cx("4,7E3-j1E-3", &im, &ok);
    chk_cond("num_lee_cx", "4,7E3-j1E-3", ok && fabs(re - 4700.0) < 1e-12 && fabs(im + 0.001) < 1e-18);
    re = num_lee_cx("50", &im, &ok);
    chk_cond("num_lee_cx", "50 (un real)", ok && re == 50.0 && im == 0.0);
    num_lee_cx("30+j", &im, &ok);   chk_cond("num_lee_cx", "30+j no vale", !ok);
    num_lee_cx("j", &im, &ok);      chk_cond("num_lee_cx", "j no vale", !ok);
    num_lee_cx("30+j40j", &im, &ok); chk_cond("num_lee_cx", "30+j40j no vale", !ok);
    num_lee_cx("+j40", &im, &ok);   chk_cond("num_lee_cx", "+j40 no vale", !ok);
    num_lee_cx("30j40", &im, &ok);  chk_cond("num_lee_cx", "30j40 no vale", !ok);
    num_lee_cx("30+j-4", &im, &ok); chk_cond("num_lee_cx", "30+j-4 no vale", !ok);

    /* ---- El glifo del ángulo (0x7F) ---- */
    {
        const u8 *g12 = font_glifo12(0x7F), *g8 = font_glifo(0x7F);
        int y, lleno12 = 0, lleno8 = 0, caja12 = 1, caja8 = 1;
        for (y = 0; y < 12; y++) {
            u8 c = (u8)(y == 2 || y == 9 ? 0x1F : (y > 2 && y < 9 ? 0x11 : 0));
            if (g12[y]) lleno12 = 1;
            if (g12[y] != c) caja12 = 0;
        }
        for (y = 0; y < 8; y++) {
            u8 c = (u8)(y == 1 || y == 6 ? 0x1F : (y > 1 && y < 6 ? 0x11 : 0));
            if (g8[y]) lleno8 = 1;
            if (g8[y] != c) caja8 = 0;
        }
        chk_cond("glifo", "el " S_ANG " de la 6x12 existe y no es el recuadro", lleno12 && !caja12);
        chk_cond("glifo", "el " S_ANG " de la 6x8 existe y no es el recuadro", lleno8 && !caja8);
    }
}

/* --------------------------------------------------------------------------
 *  43. Etapa 3: modelo y archivo de la alterna
 * ----------------------------------------------------------------------- */
/* Versión y longitud de los bloques de un archivo: global y de cada elemento */
static int bloques_de(const char *ruta, int *version, int *global, int *lens, int maxn)
{
    FILE *f = fopen(ruta, "rb");
    unsigned char cab[5];
    int i, k, n, nel;
    if (!f) return -1;
    if (fread(cab, 1, 5, f) != 5) { fclose(f); return -1; }
    *version = cab[4];
    for (i = 0; i < 2 + 24 + REJ_FILAS * (REJ_COLS - 1) + (REJ_FILAS - 1) * REJ_COLS; i++) (void)fgetc(f);
    *global = fgetc(f);
    for (i = 0; i < *global; i++) (void)fgetc(f);
    nel = fgetc(f);
    for (k = 0; k < nel; k++) {
        n = fgetc(f);
        if (k < maxn) lens[k] = n;
        for (i = 0; i < n; i++) (void)fgetc(f);
    }
    fclose(f);
    return nel;
}

static int lector_e1(const char *ruta, Circuito *t);          /* [30] */

/* Un lector que sabe hasta el modelo de la Etapa 3, lo que importa de él:
 * acepta un CESP hasta la versión 3 (la política de versiones de archivo.c) */
static int lector_e3(const char *ruta)
{
    FILE *f = fopen(ruta, "rb");
    unsigned char cab[5];
    int ok;
    if (!f) return 0;
    ok = fread(cab, 1, 5, f) == 5 && memcmp(cab, "CESP", 4) == 0 && cab[4] >= 1 && cab[4] <= 3;
    fclose(f);
    return ok;
}

/* Uno que sabe hasta el de la Etapa 2: acepta un CESP hasta la versión 2 */
static int lector_e2(const char *ruta)
{
    FILE *f = fopen(ruta, "rb");
    unsigned char cab[5];
    int ok;
    if (!f) return 0;
    ok = fread(cab, 1, 5, f) == 5 && memcmp(cab, "CESP", 4) == 0 && cab[4] >= 1 && cab[4] <= 2;
    fclose(f);
    return ok;
}

/* El circuito del viaje completo: E1 y J1 alternas, R1, L1, C1, Z1, W1, V1 */
static void circuito_ca_completo(Circuito *c)
{
    int k;
    circ_init(c, "viaje CA");
    k = circ_elemento(c, EL_FUENTE, 0, 1, DIR_ARR);
    c->el[k].variante = FORMA_FASOR;
    c->el[k].val.v = 120.0; c->el[k].val.est = M_DATO; c->el[k].val.ang = 30.0; c->el[k].val.est_ang = M_DATO;
    c->frec.v = 60.0; c->frec.est = M_DATO;
    k = circ_elemento(c, EL_RES, 0, 0, DIR_DER);
    c->el[k].V.v = 10.0; c->el[k].V.est = M_DATO; c->el[k].V.ang = -45.0; c->el[k].V.est_ang = M_DATO;
    k = circ_elemento(c, EL_BOB, 1, 0, DIR_DER);
    c->el[k].val.est = M_BUSCAR; c->el[k].I.est_ang = M_BUSCAR;
    k = circ_elemento(c, EL_COND, 2, 0, DIR_DER);
    c->el[k].val.v = 100e-6; c->el[k].val.est = M_DATO;
    k = circ_elemento(c, EL_IMP, 3, 0, DIR_DER);
    c->el[k].val.v = 5.0; c->el[k].val.est = M_DATO; c->el[k].val.ang = 36.87; c->el[k].val.est_ang = M_DATO;
    k = circ_elemento(c, EL_VATI, 4, 0, DIR_DER);
    c->el[k].val.v = 500.0; c->el[k].val.est = M_DATO; c->el[k].aux.v = -0.8; c->el[k].aux.est = M_DATO;
    k = circ_elemento(c, EL_CORR, 0, 3, DIR_ARR);
    c->el[k].variante = FORMA_FASOR;
    c->el[k].val.v = 2.0; c->el[k].val.est = M_DATO; c->el[k].val.ang = -45.0; c->el[k].val.est_ang = M_BUSCAR;
    k = circ_elemento(c, EL_VOLT, 2, 2, DIR_ABA);
    c->el[k].variante = MED_CA; c->el[k].V.v = 72.0; c->el[k].V.est = M_DATO;
    c->conv = CONV_PICO;
}

static void test_modelo_e3(void)
{
    static Circuito a, b, d;
    char ruta[300];
    double p[ONDA_PAR], peor;
    int forma, q, k, i, ver, glob, lens[MAX_EL], nel;

    printf("[43] Etapa 3: modelo y archivo de la alterna\n");

    /* ---- circ_onda_c ---- */
    circ_init(&d, "onda");
    k = circ_elemento(&d, EL_FUENTE, 0, 1, DIR_ARR);
    d.el[k].variante = FORMA_FASOR;
    d.el[k].val.v = 120.0; d.el[k].val.est = M_DATO; d.el[k].val.ang = 0.0; d.el[k].val.est_ang = M_DATO;
    d.frec.v = 60.0; d.frec.est = M_DATO;
    chk_cond("circ_onda_c", "120" S_ANG "0 a 60 Hz: una senoidal", circ_onda_c(&d, &d.el[k], &forma, p, &q) && forma == ONDA_SENO);
    chk_t("circ_onda_c", "pico = 120" S_CDOT S_SQRT "2", p[0], 169.7056274847714, 170.0, 1e-15);
    chk_cond("circ_onda_c", "f = 60, fase 90, CC 0", p[1] == 60.0 && p[2] == 90.0 && p[3] == 0.0 && p[4] == 0.0);
    d.el[k].val.v = 230.0; d.el[k].val.ang = 30.0; d.frec.v = 50.0;
    circ_onda_c(&d, &d.el[k], &forma, p, &q);
    chk_t("circ_onda_c", "230" S_ANG "30 a 50 Hz: pico", p[0], 325.2691193458119, 325.0, 1e-15);
    chk_cond("circ_onda_c", "fase 120", p[1] == 50.0 && p[2] == 120.0);
    chk_t("circ_onda_c", "en t = 0 vale 230" S_CDOT S_SQRT "2" S_CDOT "cos 30", onda_valor(ONDA_SENO, p, 0.0), 281.6913204200655, 325.0, 1e-14);
    peor = 0.0;
    for (i = 0; i < 1000; i++) {
        double t = 0.1 * (double)azar(1000001) / 1e6;
        double e = fabs(onda_valor(ONDA_SENO, p, t) - 230.0 * sqrt(2.0) * cos(2.0 * PI_T * 50.0 * t + 30.0 * PI_T / 180.0));
        if (e > peor) peor = e;
    }
    chk_t("circ_onda_c", "en 1000 instantes: 230" S_CDOT S_SQRT "2" S_CDOT "cos(2" S_PI "50t + 30" S_DEG ")", peor, 0.0, 325.0, 1e-12);
    d.conv = CONV_PICO | CONV_SENO;
    {
        double p2[ONDA_PAR];
        circ_onda_c(&d, &d.el[k], &forma, p2, &q);
        chk_cond("circ_onda_c", "el convenio no cambia la senoidal (lo guardado es canónico)", memcmp(p, p2, sizeof p) == 0);
    }
    d.frec.est = M_LIBRE;
    chk_cond("circ_onda_c", "sin frec: falta Q_FCIRC", !circ_onda_c(&d, &d.el[k], &forma, p, &q) && q == Q_FCIRC);
    d.frec.est = M_DATO; d.el[k].val.est = M_LIBRE;
    chk_cond("circ_onda_c", "sin el valor: falta Q_VAL", !circ_onda_c(&d, &d.el[k], &forma, p, &q) && q == Q_VAL);
    d.el[k].val.est = M_DATO; d.el[k].val.est_ang = M_LIBRE;
    circ_onda_c(&d, &d.el[k], &forma, p, &q);
    chk_cond("circ_onda_c", "ángulo vacío = 0" S_DEG, p[2] == 90.0);
    d.el[k].variante = ONDA_SENO;
    d.el[k].par[Q_FREC - Q_P1].v = 1e3; d.el[k].par[Q_FREC - Q_P1].est = M_DATO;
    {
        double p2[ONDA_PAR];
        int f2, q2;
        circ_onda_c(&d, &d.el[k], &forma, p, &q);
        circ_onda(&d.el[k], &f2, p2, &q2);
        chk_cond("circ_onda_c", "un generador: lo mismo que circ_onda", forma == f2 && memcmp(p, p2, sizeof p) == 0);
    }

    /* ---- circ_alterna ---- */
    lazo(&d, 0, 12.0, 4.0, 8.0);
    chk_cond("circ_alterna", "CC: no", !circ_alterna(&d));
    d.el[0].variante = ONDA_SENO;
    chk_cond("circ_alterna", "un generador senoidal: no", !circ_alterna(&d));
    d.el[0].variante = FORMA_FASOR;
    chk_cond("circ_alterna", "una E alterna: sí", circ_alterna(&d));
    circ_init(&d, "j");
    k = circ_elemento(&d, EL_CORR, 0, 1, DIR_ARR);
    d.el[k].variante = FORMA_FASOR;
    chk_cond("circ_alterna", "una J alterna: sí", circ_alterna(&d));
    lazo(&d, 0, 12.0, 4.0, 8.0);
    circ_elemento(&d, EL_IMP, 5, 5, DIR_DER);
    chk_cond("circ_alterna", "una Z con una E continua: sí", circ_alterna(&d));
    lazo(&d, 0, 12.0, 4.0, 8.0);
    circ_elemento(&d, EL_VATI, 5, 5, DIR_DER);
    chk_cond("circ_alterna", "un vatímetro: no", !circ_alterna(&d));

    /* ---- circ_pide ---- */
    lazo(&d, 0, 12.0, 4.0, 8.0);                       /* E1, R1, R2 */
    d.el[1].V.est_ang = M_BUSCAR;
    chk_cond("circ_pide", "un ángulo [?!] en la V de una R", circ_pide(&d));
    ejemplo_cargar(EJ_PH, &d);
    i = circ_pide(&d);                                 /* EJ_PH ya pide algo */
    d.el[1].aux.est_ang = M_BUSCAR;
    chk_cond("circ_pide", "un ángulo en un campo sin ángulo no cuenta", circ_pide(&d) == i);
    lazo(&d, 0, 12.0, 4.0, 8.0);
    d.el[1].aux.est_ang = M_BUSCAR; d.el[1].val.est_ang = M_BUSCAR;
    chk_cond("circ_pide", "el ángulo del valor o del aux de una R no pide", !circ_pide(&d));
    lazo(&d, 0, 12.0, 4.0, 8.0);
    d.frec.est = M_BUSCAR;
    chk_cond("circ_pide", "frec [?!] en CC no pide", !circ_pide(&d));
    d.el[0].variante = FORMA_FASOR;
    d.el[0].val.v = 12.0;
    chk_cond("circ_pide", "frec [?!] en alterna sí", circ_pide(&d));

    /* ---- Viaje completo ---- */
    limpia_mis();
    sprintf(ruta, "%s/ces_01.tns", carpeta_mis);
    circuito_ca_completo(&a);
    chk_cond("viaje CA", "es válido", circ_valido(&a));
    chk_cond("viaje CA", "se guarda", arch_guardar(0, &a));
    chk_cond("viaje CA", "se abre igual", arch_cargar(0, &b) && mismos(&a, &b));
    nel = bloques_de(ruta, &ver, &glob, lens, MAX_EL);
    chk_cond("viaje CA", "versión 3", ver == 3);
    chk_cond("viaje CA", "bloque global de 25", glob == 25 && nel == a.nel);
    {
        int bien = 1;
        for (k = 0; k < nel && k < a.nel; k++) {
            int esperado = (a.el[k].tipo == EL_COND || a.el[k].tipo == EL_BOB) ? 139 : 85;
            if (lens[k] != esperado) bien = 0;
        }
        chk_cond("viaje CA", "bloques de 85 (139 con V0 o I0)", bien);
    }
    chk_cond("viaje CA", "el lector de la Etapa 2 lo rechaza", !lector_e2(ruta));
    chk_cond("viaje CA", "el de la Etapa 1 también", !lector_e1(ruta, &b));
    /* sin nada de la alterna: vuelve a la versión 2 */
    d = a;
    for (k = d.nel - 1; k >= 0; k--)
        if (d.el[k].tipo == EL_IMP || d.el[k].tipo == EL_VATI) {
            int x0 = d.el[k].xa, y0 = d.el[k].ya, dir;
            for (dir = 0; dir < 4; dir++) { int nx, ny; if (circ_vecino(x0, y0, dir, &nx, &ny) && nx == d.el[k].xb && ny == d.el[k].yb) break; }
            circ_quitar(&d, x0, y0, dir);
        }
    for (k = 0; k < d.nel; k++)
        if (d.el[k].tipo == EL_FUENTE || d.el[k].tipo == EL_CORR) d.el[k].variante = ONDA_CC;
    chk_cond("circ_quita_ca", "quita algo", circ_quita_ca(&d));
    chk_cond("circ_quita_ca", "ya no es alterna, sin [?!] y válido", !circ_alterna(&d) && circ_valido(&d));
    arch_guardar(0, &d);
    bloques_de(ruta, &ver, &glob, lens, MAX_EL);
    chk_cond("circ_quita_ca", "vuelve a la versión 2", ver == 2 && lector_e2(ruta));
    chk_cond("circ_quita_ca", "otra vez: nada que quitar", !circ_quita_ca(&d));

    /* ---- circ_valido rechaza, uno a uno ---- */
    {
        static const char *const QUE[] = {
            "variante 6 en una E", "una E alterna con el módulo -1", "una Z con |Z| = 0",
            "una Z con ángulo 95", "una Z con aux dato", "una Z con variante 1", "una Z con un parámetro dato",
            "un W con fp 1,5", "un W con P = -1", "un W con ángulo en P", "el ángulo 400 en la V de una R",
            "frec = 0 dato",
        };
        int caso, kz, kw;
        for (caso = 0; caso < (int)(sizeof QUE / sizeof QUE[0]); caso++) {
            lazo(&d, 0, 12.0, 4.0, 8.0);                  /* E1, R1, R2 */
            kz = circ_elemento(&d, EL_IMP, 5, 5, DIR_DER);
            d.el[kz].val.v = 10.0; d.el[kz].val.est = M_DATO;
            kw = circ_elemento(&d, EL_VATI, 5, 4, DIR_DER);
            switch (caso) {
            case 0: d.el[0].variante = 6; break;
            case 1: d.el[0].variante = FORMA_FASOR; d.el[0].val.v = -1.0; break;
            case 2: d.el[kz].val.v = 0.0; break;
            case 3: d.el[kz].val.ang = 95.0; d.el[kz].val.est_ang = M_DATO; break;
            case 4: d.el[kz].aux.v = 1.0; d.el[kz].aux.est = M_DATO; break;
            case 5: d.el[kz].variante = 1; break;
            case 6: d.el[kz].par[0].v = 1.0; d.el[kz].par[0].est = M_DATO; break;
            case 7: d.el[kw].aux.v = 1.5; d.el[kw].aux.est = M_DATO; break;
            case 8: d.el[kw].val.v = -1.0; d.el[kw].val.est = M_DATO; break;
            case 9: d.el[kw].val.ang = 3.0; d.el[kw].val.est_ang = M_DATO; break;
            case 10: d.el[1].V.ang = 400.0; d.el[1].V.est_ang = M_DATO; break;
            default: d.frec.v = 0.0; d.frec.est = M_DATO; break;
            }
            chk_cond("modelo E3: circ_valido rechaza", QUE[caso], !circ_valido(&d));
        }
        lazo(&d, 0, 12.0, 4.0, 8.0);
        kz = circ_elemento(&d, EL_IMP, 5, 5, DIR_DER);
        kw = circ_elemento(&d, EL_VATI, 5, 4, DIR_DER);
        d.el[kz].val.v = 10.0; d.el[kz].val.est = M_DATO; d.el[kz].val.ang = -90.0; d.el[kz].val.est_ang = M_DATO;
        d.el[kw].aux.v = -1.0; d.el[kw].aux.est = M_DATO;
        chk_cond("modelo E3: circ_valido acepta", "Z a -90" S_DEG " y un W con fp -1", circ_valido(&d));
    }
    /* ---- circ_valido acepta ---- */
    lazo(&d, 0, 12.0, 4.0, 8.0);
    k = circ_elemento(&d, EL_COND, 5, 5, DIR_DER);
    d.el[k].val.est = M_BUSCAR;
    chk_cond("modelo E3: circ_valido acepta", "una C con el valor [?!] en CC", circ_valido(&d));
    lazo(&d, 0, 12.0, 4.0, 8.0);
    d.frec.est = M_BUSCAR;
    chk_cond("modelo E3: circ_valido acepta", "frec [?!]", circ_valido(&d));
    ejemplo_cargar(EJ_PH, &d);
    d.el[0].val.ang = -30.5; d.el[0].val.est_ang = M_DATO;
    d.el[1].aux.v = 0.25; d.el[1].aux.est = M_DATO; d.el[1].aux.ang = 12.0; d.el[1].aux.est_ang = M_BUSCAR;
    chk_cond("modelo E3: circ_valido acepta", "los ángulos de [8] en campos sin ángulo", circ_valido(&d));

    /* ---- tipo_rango ---- */
    {
        double lo, hi;
        tipo_rango(EL_VATI, Q_AUX, &lo, &hi);
        chk_cond("tipo_rango", "fp del W: -1..1", lo == -1.0 && hi == 1.0);
        tipo_rango(EL_FUENTE, Q_AUX, &lo, &hi);
        chk_cond("tipo_rango", "r de la E: 0..1e9", lo == 0.0 && hi == 1e9);
        tipo_rango(EL_FUENTE, Q_FCIRC, &lo, &hi);
        chk_cond("tipo_rango", "f del circuito: 1e-3..1e9", lo == 1e-3 && hi == 1e9);
    }
    /* ---- Los campos: en CC la ficha no cambia ---- */
    chk_cond("campos", "la E en continua: E, I, r, V (como en la Etapa 1)",
             tipo_campo_v(EL_FUENTE, ONDA_CC, Q_VAL) == &TIPOS[EL_FUENTE].campos[0] &&
             !(tipo_campo_v(EL_FUENTE, ONDA_CC, Q_VAL)->flags & CF_ANG) && tipo_campo_v(EL_FUENTE, ONDA_CC, Q_FCIRC) == 0);
    chk_cond("campos", "la E alterna: E con ángulo y f del circuito",
             (tipo_campo_v(EL_FUENTE, FORMA_FASOR, Q_VAL)->flags & CF_ANG) && tipo_campo_v(EL_FUENTE, FORMA_FASOR, Q_FCIRC) != 0 &&
             tipo_campo_v(EL_FUENTE, FORMA_FASOR, Q_FREC) == 0);
    chk_cond("campos", "la I de una C solo se ve en alterna", (tipo_campo(EL_COND, Q_I)->flags & CF_VER_CA) != 0);
    chk_cond("campos", "la Z tiene su tecla y el W no (la W es el cable)", TIPOS[EL_IMP].tecla == 'Z' && TIPOS[EL_VATI].tecla == 0);
    chk_cond("campos", "el nombre de la forma alterna", strcmp(NOM_FORMA[FORMA_FASOR], "alterna (fasor)") == 0);
    limpia_mis();
}

/* --------------------------------------------------------------------------
 *  44. Etapa 3: la calculadora de CC y el emulador ante lo nuevo
 * ----------------------------------------------------------------------- */
/* E (0,1)->(0,0) de valor E con la tierra en su -, W1 de (0,0) a (1,0), R de
 * (1,0) a (1,1) y cable abajo. Con gira, el W al revés */
static int red_vatimetro(Circuito *c, double E, double R, int gira)
{
    int kE, kW, kR;
    circ_init(c, "vatimetro");
    kE = circ_elemento(c, EL_FUENTE, 0, 1, DIR_ARR);
    c->el[kE].val.v = E; c->el[kE].val.est = M_DATO;
    kW = gira ? circ_elemento(c, EL_VATI, 1, 0, DIR_IZQ) : circ_elemento(c, EL_VATI, 0, 0, DIR_DER);
    kR = circ_elemento(c, EL_RES, 1, 0, DIR_ABA);
    c->el[kR].val.v = R; c->el[kR].val.est = M_DATO;
    circ_cable(c, 1, 1, DIR_IZQ);
    c->tierra_x = 0; c->tierra_y = 1;
    return kW;
}

static void test_cc_ante_ca(void)
{
    static Circuito c, c2;
    static Red r;
    static Solucion s;
    char m[300];
    int k, kW, kL;

    printf("[44] Etapa 3: la calculadora de CC ante la alterna\n");

    /* ---- SOL_CA antes que la red: E1 alterna en paralelo con una L ---- */
    circ_init(&c, "EL");
    k = circ_elemento(&c, EL_FUENTE, 0, 1, DIR_ARR);
    c.el[k].variante = FORMA_FASOR; c.el[k].val.v = 10.0; c.el[k].val.est = M_DATO;
    kL = circ_elemento(&c, EL_BOB, 1, 0, DIR_ABA);
    c.el[kL].val.v = 1e-3; c.el[kL].val.est = M_DATO;
    circ_cable(&c, 0, 0, DIR_DER); circ_cable(&c, 0, 1, DIR_DER);
    c.frec.v = 60.0; c.frec.est = M_DATO;
    red_extraer(&c, &r);
    chk_cond("SOL_CA", "en CC sería un corto (la red lo dice)", r.error == RED_FUENTE_AMP);
    resolver(&c, &r, &s);
    sol_mensaje(&c, &r, &s, m, sizeof m);
    chk_cond("SOL_CA", "pero es de alterna: SOL_CA con E1", s.estado == SOL_CA && s.elem == k && strstr(m, "alterna") != 0);
    chk_cond("SOL_CA", "el mensaje cabe en 4 líneas", lineas(m, 52) <= 4);
    c.el[k].variante = ONDA_CC;
    k = circ_elemento(&c, EL_IMP, 3, 3, DIR_DER);
    c.el[k].val.v = 5.0; c.el[k].val.est = M_DATO;
    red_extraer(&c, &r);
    resolver(&c, &r, &s);
    chk_cond("SOL_CA", "una Z también: SOL_CA con Z1", s.estado == SOL_CA && s.elem == k);

    /* ---- SOL_SOLO_CA ---- */
    red_xyz(&c, 12.0, EL_RES, 4.0, EL_COND, 1e-6, -1, 0.0);
    c.el[2].val.est = M_BUSCAR;
    red_extraer(&c, &r);
    resolver(&c, &r, &s);
    sol_mensaje(&c, &r, &s, m, sizeof m);
    chk_cond("SOL_SOLO_CA", "el valor de C1 [?!]: «abierto»", s.estado == SOL_SOLO_CA && s.elem == 2 && strstr(m, "abierto") != 0 && strstr(m, "C1") != 0);
    red_xyz(&c, 12.0, EL_RES, 4.0, EL_BOB, 1e-3, -1, 0.0);
    c.el[2].val.est = M_BUSCAR;
    c.el[1].I.est = M_BUSCAR;
    red_extraer(&c, &r);
    resolver(&c, &r, &s);
    sol_mensaje(&c, &r, &s, m, sizeof m);
    chk_cond("SOL_SOLO_CA", "el valor de L1 [?!]: «cable»", s.estado == SOL_SOLO_CA && s.elem == 2 && strstr(m, "cable") != 0);
    red_xyz(&c, 12.0, EL_RES, 4.0, EL_RES, 8.0, -1, 0.0);
    c.el[1].V.v = 3.0; c.el[1].V.est = M_DATO; c.el[1].V.ang = 30.0; c.el[1].V.est_ang = M_DATO;
    red_extraer(&c, &r);
    resolver(&c, &r, &s);
    sol_mensaje(&c, &r, &s, m, sizeof m);
    chk_cond("SOL_SOLO_CA", "un ángulo dato en V(R1): «ángulo»", s.estado == SOL_SOLO_CA && s.elem == 1 && strstr(m, "ngulo") != 0 && strstr(m, "V(R1)") != 0);
    chk_cond("SOL_SOLO_CA", "el mensaje cabe en 4 líneas", lineas(m, 52) <= 4);

    /* ---- El vatímetro en CC ---- */
    kW = red_vatimetro(&c, 12.0, 4.0, 0);
    chk_cond("vatímetro en CC", "el circuito es válido", circ_valido(&c));
    red_extraer(&c, &r);
    resolver(&c, &r, &s);
    chk_cond("vatímetro en CC", "se resuelve", s.estado == SOL_OK);
    if (s.estado == SOL_OK) {
        chk("vatímetro en CC", "I = 3 A", fabs(s.s[0].I[kW]), 3.0);
        chk("vatímetro en CC", "V(W1) = 12 V (de b a la tierra)", s.s[0].V[kW], 12.0);
        chk("vatímetro en CC", "P(W1) = 36 W", s.s[0].val[kW], 36.0);
        chk("vatímetro en CC", "y en la potencia", s.s[0].P[kW], 36.0);
        chk_t("vatímetro en CC", "residuo 0", sol_residuo(&c, &r, &s.s[0]), 0.0, 1.0, 1e-12);
    }
    kW = red_vatimetro(&c, 12.0, 4.0, 1);
    red_extraer(&c, &r);
    resolver(&c, &r, &s);
    chk_cond("vatímetro en CC", "girado: se resuelve", s.estado == SOL_OK);
    if (s.estado == SOL_OK) {
        chk("vatímetro en CC", "girado: P = -36 W", s.s[0].val[kW], -36.0);
        chk_t("vatímetro en CC", "girado: residuo 0", sol_residuo(&c, &r, &s.s[0]), 0.0, 1.0, 1e-12);
    }
    /* la P [?!] se admite en CC (es un pedido); un número no */
    kW = red_vatimetro(&c, 12.0, 4.0, 0);
    c.el[kW].val.est = M_BUSCAR;
    red_extraer(&c, &r);
    resolver(&c, &r, &s);
    chk_cond("vatímetro en CC", "P [?!] se calcula", s.estado == SOL_OK && s.nsol == 1 && fabs(s.s[0].val[kW] - 36.0) < 1e-12);
    c.el[kW].val.v = 36.0; c.el[kW].val.est = M_DATO;
    red_extraer(&c, &r);
    resolver(&c, &r, &s);
    sol_mensaje(&c, &r, &s, m, sizeof m);
    chk_cond("vatímetro en CC", "P dato: SOL_SOLO_CA con «W1»", s.estado == SOL_SOLO_CA && s.elem == kW && strstr(m, "W1") != 0);
    /* W1 en paralelo con E1 ideal: la culpa del corto es del vatímetro */
    circ_init(&c, "W corto");
    k = circ_elemento(&c, EL_FUENTE, 0, 1, DIR_ARR);
    c.el[k].val.v = 12.0; c.el[k].val.est = M_DATO;
    kW = circ_elemento(&c, EL_VATI, 1, 0, DIR_ABA);
    circ_cable(&c, 0, 0, DIR_DER); circ_cable(&c, 0, 1, DIR_DER);
    red_extraer(&c, &r);
    resolver(&c, &r, &s);
    sol_mensaje(&c, &r, &s, m, sizeof m);
    chk_cond("vatímetro en CC", "en paralelo con E1: RED_FUENTE_AMP con la culpa en W1",
             r.error == RED_FUENTE_AMP && r.elem_culpa == kW && strstr(m, "W1") != 0 && strstr(m, "vat") != 0);
    /* una L en el camino se lleva la culpa antes que el vatímetro */
    circ_init(&c, "W y L");
    k = circ_elemento(&c, EL_FUENTE, 0, 1, DIR_ARR);
    c.el[k].val.v = 12.0; c.el[k].val.est = M_DATO;
    kW = circ_elemento(&c, EL_VATI, 0, 0, DIR_DER);
    kL = circ_elemento(&c, EL_BOB, 1, 0, DIR_ABA);
    c.el[kL].val.v = 1e-3; c.el[kL].val.est = M_DATO;
    circ_cable(&c, 1, 1, DIR_IZQ);
    red_extraer(&c, &r);
    chk_cond("vatímetro en CC", "W y L en el camino: la culpa es de la L", r.error == RED_FUENTE_AMP && r.elem_culpa == kL);

    /* ---- sol_aplica ---- */
    red_xyz(&c, 12.0, EL_RES, 4.0, EL_RES, 8.0, -1, 0.0);
    c.el[2].val.est = M_LIBRE;
    c.el[1].I.v = 1.0; c.el[1].I.est = M_DATO;
    red_extraer(&c, &r);
    resolver(&c, &r, &s);
    sol_aplica(&c, &s, 0, &c2);
    chk_cond("sol_aplica", "R2 hallada pasa a dato", s.estado == SOL_OK && c2.el[2].val.est == M_DATO && fabs(c2.el[2].val.v - 8.0) < 1e-12);
    chk_cond("sol_aplica", "lo demás no cambia", c2.el[1].I.est == M_DATO && c2.el[0].val.v == 12.0 && circ_valido(&c2));
}

/* El RC en lazo con una E de (0,1) a (0,0), R de (0,0) a (1,0), C de (1,0) a (1,1) */
static int rc_con_e(Circuito *c, double R, double C)
{
    int kE, k;
    circ_init(c, "RC");
    kE = circ_elemento(c, EL_FUENTE, 0, 1, DIR_ARR);
    k = circ_elemento(c, EL_RES, 0, 0, DIR_DER); c->el[k].val.v = R; c->el[k].val.est = M_DATO;
    k = circ_elemento(c, EL_COND, 1, 0, DIR_ABA); c->el[k].val.v = C; c->el[k].val.est = M_DATO;
    circ_cable(c, 1, 1, DIR_IZQ);
    return kE;
}

static double muestras_1[SIM_MAX_MUESTRAS];

static void test_sim_ante_ca(void)
{
    static Circuito c;
    static Red r;
    static SimRed s1, s2, copia_red;
    static SimRes copia_res;
    static SimTopoCA tca;
    SimDiag d;
    SimAjustes aj;
    char m[300], m2[300];
    int k, kE, kW, kC, n1, iguales;
    double vth, rth;
    int inf;

    printf("[44] (sigue) el emulador ante la alterna\n");

    /* ---- Una Z no se emula ---- */
    kE = rc_con_e(&c, 1e3, 1e-6);
    c.el[kE].val.v = 10.0; c.el[kE].val.est = M_DATO;
    k = circ_elemento(&c, EL_IMP, 0, 0, DIR_DER);            /* sustituye a la R */
    c.el[k].val.v = 50.0; c.el[k].val.est = M_DATO; c.el[k].val.ang = 30.0; c.el[k].val.est_ang = M_DATO;
    c.frec.v = 60.0; c.frec.est = M_DATO;
    red_extraer(&c, &r);
    chk_cond("Z en el tiempo", "sim_iniciar: SIM_SOLO_CA con la Z", sim_iniciar(&c, &r) == SIM_SOLO_CA && sim_res()->d.elem >= 0 &&
             c.el[sim_red()->el[sim_res()->d.elem].k].tipo == EL_IMP);
    sim_mensaje_circ(&c, sim_red(), &sim_res()->d, m, sizeof m);
    chk_cond("Z en el tiempo", "el mensaje: «Z1 es una impedancia de CA»", strstr(m, "Z1 es una impedancia de CA") != 0 && lineas(m, 52) <= 4);
    sim_red_de(&c, &r, &s1, &d);
    chk_cond("Z en el tiempo", "en la red: R = |Z| cos, X = |Z| sen, f0 = 60",
             fabs(s1.el[k].val - 50.0 * cos(PI_T / 6.0)) < 1e-12 && fabs(s1.el[k].p[0] - 25.0) < 1e-12 && s1.el[k].p[1] == 60.0 && s1.el[k].tipo == SE_Z);
    c.frec.est = M_LIBRE;
    chk_cond("Z en el tiempo", "sin frec: falta la f", sim_red_de(&c, &r, &s1, &d) == SIM_FALTA && d.campo == Q_FCIRC);
    {
        SimRed sz;
        memset(&sz, 0, sizeof sz);
        sz.nn = 2; sz.ref[0] = 1; sz.nel = 1;
        sz.el[0].tipo = SE_Z; sz.el[0].a = 1; sz.el[0].b = 0; sz.el[0].val = 1.0; sz.el[0].p[1] = 50.0; sz.el[0].k = -1;
        memset(&d, 0, sizeof d); d.estado = SIM_SOLO_CA; d.elem = 0; d.nodo = -1;
        sim_mensaje(&sz, &d, m, sizeof m);
        chk_cond("Z en el tiempo", "sin circuito se llama Z1", strncmp(m, "Z1 ", 3) == 0);
    }

    /* ---- La E alterna en el tiempo: la misma senoidal, bit a bit ---- */
    kE = rc_con_e(&c, 1e3, 1e-6);
    c.el[kE].variante = FORMA_FASOR;
    c.el[kE].val.v = 10.0; c.el[kE].val.est = M_DATO; c.el[kE].val.ang = 0.0; c.el[kE].val.est_ang = M_DATO;
    c.frec.v = 1e3; c.frec.est = M_DATO;
    red_extraer(&c, &r);
    sim_red_de(&c, &r, &s1, &d);
    c.el[kE].variante = ONDA_SENO;
    c.el[kE].val.v = 10.0 * sqrt(2.0);
    c.el[kE].par[Q_FREC - Q_P1].v = 1e3; c.el[kE].par[Q_FREC - Q_P1].est = M_DATO;
    c.el[kE].par[Q_FASE - Q_P1].v = 90.0; c.el[kE].par[Q_FASE - Q_P1].est = M_DATO;
    sim_red_de(&c, &r, &s2, &d);
    chk_cond("E alterna en el tiempo", "los mismos parámetros que el generador", memcmp(s1.el[kE].p, s2.el[kE].p, sizeof s1.el[kE].p) == 0 &&
             s1.el[kE].forma == ONDA_SENO && s2.el[kE].forma == ONDA_SENO);
    memset(&aj, 0, sizeof aj);
    canal_de(&aj, &s1, 0, CANAL_V, 2);
    sim_emular(&s1, &aj);
    n1 = sim_reg()->n;
    memcpy(muestras_1, sim_reg()->y[0], sizeof(double) * (size_t)n1);
    sim_emular(&s2, &aj);
    iguales = sim_reg()->n == n1 && memcmp(muestras_1, sim_reg()->y[0], sizeof(double) * (size_t)n1) == 0;
    chk_cond("E alterna en el tiempo", "las mismas muestras bit a bit", iguales);

    /* ---- El vatímetro en el tiempo ---- */
    sr_init(4);
    k = sr_el(SE_E, 0, 1, 0.0); sr.el[k].forma = ONDA_SENO; sr.el[k].p[0] = 10.0; sr.el[k].p[1] = 1e3;
    kW = sr_el(SE_AMI, 1, 2, 0.0); sr.el[kW].instr = 'W';
    sr_el(SE_R, 2, 3, 100.0);
    kC = sr_el(SE_C, 3, 0, 1e-6);
    canal(0, CANAL_V, kC);
    saj.h = 1e-5; saj.t_fin = 20e-3;
    emula("vatímetro en el tiempo");
    {
        const SimRes *rs = sim_res();
        double entregada = rs->W[0];
        chk_t("vatímetro en el tiempo", "marca la P del trapecio (con " S_OMEGA "~)", rs->lectura[kW], 0.1415883859606732, 0.1416, 1e-9);
        chk_t("vatímetro en el tiempo", "a 5e-4 de la P exacta", rs->lectura[kW], 0.14152159983755108, 0.1415, 5e-4);
        /* con el arranque de Euler en t = 0 el balance no es exacto (1e-8);
         * si el W contara como disipada, faltaría toda su P·t */
        chk_t("vatímetro en el tiempo", "el balance no lo cuenta como disipada", rs->dif_balance, 0.0, fabs(entregada), 1e-6);
        chk("vatímetro en el tiempo", "su V es la de b (la de E1)", rs->ultimo.V[kW], rs->ultimo.V[0]);
    }

    /* ---- sim_topo_ca y sim_thevenin_cc no tocan la última emulación ---- */
    {
        static Circuito rcm;
        static Red rrc;
        modelo_cargar(0, &rcm);
        red_extraer(&rcm, &rrc);
        sim_iniciar(&rcm, &rrc);
        while (sim_avanzar(SIM_OPS_TICK)) {}
        copia_red = *sim_red();
        copia_res = *sim_res();
        sim_mensaje(sim_red(), &sim_res()->d, m, sizeof m);
        lazo(&c, 0, 12.0, 4.0, 12.0);
        c.term_x[0] = 1; c.term_y[0] = 0; c.term_x[1] = 1; c.term_y[1] = 1;
        red_extraer(&c, &r);
        sim_red_de(&c, &r, &s1, &d);
        sim_topo_ca(&s1, &tca, &d);
        chk_cond("estado intacto", "sim_topo_ca: SIM_OK con n incógnitas", d.estado == SIM_OK && tca.n == 3);
        k = sim_thevenin_cc(&s1, r.punto[0 * REJ_COLS + 1], r.punto[1 * REJ_COLS + 1], &vth, &rth, &inf, &d);
        chk_cond("Thévenin de CC", "E = 12, R1 = 4, R2 = 12: Vth = 9 V", k == SIM_OK && fabs(vth - 9.0) < 1e-12);
        chk_cond("Thévenin de CC", "Rth = 3 " S_OHM, fabs(rth - 3.0) < 1e-12 && !inf);
        chk_cond("estado intacto", "sim_red() igual", memcmp(&copia_red, sim_red(), sizeof copia_red) == 0);
        chk_cond("estado intacto", "sim_res() igual", memcmp(&copia_res, sim_res(), sizeof copia_res) == 0);
        sim_mensaje(sim_red(), &sim_res()->d, m2, sizeof m2);
        chk_cond("estado intacto", "el mismo mensaje", strcmp(m, m2) == 0);
    }
    /* un C entre A y el resto: R infinita en CC */
    circ_init(&c, "C a A");
    kE = circ_elemento(&c, EL_FUENTE, 0, 1, DIR_ARR); c.el[kE].val.v = 10.0; c.el[kE].val.est = M_DATO;
    k = circ_elemento(&c, EL_RES, 0, 0, DIR_DER); c.el[k].val.v = 100.0; c.el[k].val.est = M_DATO;
    k = circ_elemento(&c, EL_COND, 1, 0, DIR_DER); c.el[k].val.v = 1e-6; c.el[k].val.est = M_DATO;
    c.term_x[0] = 2; c.term_y[0] = 0; c.term_x[1] = 0; c.term_y[1] = 1;
    red_extraer(&c, &r);
    sim_red_de(&c, &r, &s1, &d);
    k = sim_thevenin_cc(&s1, r.punto[0 * REJ_COLS + 2], r.punto[1 * REJ_COLS + 0], &vth, &rth, &inf, &d);
    chk_cond("Thévenin de CC", "un C por medio: R infinita", k == SIM_OK && inf == 1);
    /* la J que carga un C: sin régimen */
    circ_init(&c, "J C");
    kE = circ_elemento(&c, EL_CORR, 0, 1, DIR_ARR); c.el[kE].val.v = 1e-3; c.el[kE].val.est = M_DATO;
    k = circ_elemento(&c, EL_COND, 1, 0, DIR_ABA); c.el[k].val.v = 1e-6; c.el[k].val.est = M_DATO;
    circ_cable(&c, 0, 0, DIR_DER); circ_cable(&c, 0, 1, DIR_DER);
    red_extraer(&c, &r);
    sim_red_de(&c, &r, &s1, &d);
    k = sim_thevenin_cc(&s1, r.punto[0 * REJ_COLS + 1], r.punto[1 * REJ_COLS + 1], &vth, &rth, &inf, &d);
    chk_cond("Thévenin de CC", "una J que carga un C: SIM_SIN_REGIMEN", k == SIM_SIN_REGIMEN);
    sim_mensaje_circ(&c, &s1, &d, m, sizeof m);
    chk_cond("Thévenin de CC", "con su mensaje", strstr(m, "no hay Th") != 0 && lineas(m, 52) <= 4);

    /* ---- sim_mascara_final ---- */
    sr_init(3);
    k = sr_el(SE_S, 0, 1, 0.0); sr.el[k].cerrado0 = 0; sr.el[k].t1 = 1e-3;
    k = sr_el(SE_S, 1, 2, 0.0); sr.el[k].cerrado0 = 1; sr.el[k].t1 = 1e-3;
    k = sr_el(SE_S, 2, 0, 0.0); sr.el[k].cerrado0 = 1; sr.el[k].t1 = 1e-3; sr.el[k].t2 = 2e-3;
    chk_cond("sim_mascara_final", "abierto que cierra, cerrado que abre, y el que vuelve", sim_mascara_final(&sr) == (1u | 4u));

    /* ---- Los mensajes nuevos caben ---- */
    {
        static const int E[] = { SIM_CA_FORMA, SIM_CA_FRECS, SIM_CA_SIN_F, SIM_RESONANCIA, SIM_SIN_AB,
                                 SIM_AB_SEPARADOS, SIM_SIN_REGIMEN, SIM_SIN_FUENTE };
        int i, caben = 1;
        sr_init(3);
        k = sr_el(SE_E, 0, 1, 0.0); sr.el[k].forma = ONDA_SENO; sr.el[k].p[1] = 60.0;
        k = sr_el(SE_E, 2, 1, 0.0); sr.el[k].forma = ONDA_SENO; sr.el[k].p[1] = 1e3;
        for (i = 0; i < (int)(sizeof E / sizeof E[0]); i++) {
            memset(&d, 0, sizeof d); d.estado = E[i]; d.elem = 1; d.nodo = -1; d.lazo = 3u; d.t = 1591.549;
            sim_mensaje(&sr, &d, m, sizeof m);
            if (!(lineas(m, 52) <= 4 && strlen(m) > 20)) caben = 0;
        }
        chk_cond("mensajes de CA", "los ocho caben en 4 líneas", caben);
        memset(&d, 0, sizeof d); d.estado = SIM_CA_FRECS; d.elem = 1; d.nodo = -1; d.lazo = 3u;
        sim_mensaje(&sr, &d, m, sizeof m);
        chk_cond("mensajes de CA", "«E1 va a 60Hz y E2 a 1kHz»", strstr(m, "E1 va a 60Hz y E2 a 1kHz") != 0);
    }
}

/* --------------------------------------------------------------------------
 *  45. Etapa 3: el motor de CA, casos cerrados
 * ----------------------------------------------------------------------- */
#define R2_T 1.41421356237309504880
static CaRes car;

/* Análisis de CA de la red sr (una copia: ca_apaga la cambia) */
static int ca_sr(double frec)
{
    static SimRed s;
    CaExc x;
    double f;
    s = sr;
    memset(&car, 0, sizeof car);
    car.d.elem = car.d.nodo = car.d.elem_aviso = -1;
    car.fuente = -1;
    f = ca_frecuencia(&s, frec, &car.d);
    if (car.d.estado != SIM_OK) return car.d.estado;
    ca_excitacion(&s, f, &x, &car.d);
    if (car.d.estado != SIM_OK) return car.d.estado;
    ca_apaga(&s, x.activa);
    return ca_resuelve_red(&s, f, &x, &car);
}

/* Fuente alterna (eficaz, coseno y grados) como la saca el adaptador */
static int sr_alterna(int tipo, int a, int b, double ef, double ang, double f)
{
    int k = sr_el(tipo, a, b, 0.0);
    sr.el[k].forma = ONDA_SENO;
    sr.el[k].p[0] = sqrt(2.0) * ef; sr.el[k].p[1] = f; sr.el[k].p[2] = ang + 90.0;
    return k;
}

static int sr_z(int a, int b, double mod, double ang_g, double f0)
{
    int k = sr_el(SE_Z, a, b, mod * cos(ang_g * PI_T / 180.0));
    sr.el[k].p[0] = mod * sin(ang_g * PI_T / 180.0); sr.el[k].p[1] = f0;
    return k;
}

static void chk_cx(const char *q, const char *det, Cx a, Cx b, double esc, double tol)
{
    pruebas++;
    if (!(cx_abs(cx_menos(a, b)) <= tol * esc)) {
        char t[200];
        sprintf(t, "%s: (%.15g %+.15gj) frente a (%.15g %+.15gj)", det, a.re, a.im, b.re, b.im);
        falla(q, t, cx_abs(cx_menos(a, b)), 0.0);
    }
}

/* Módulo eficaz y ángulo en grados (el convenio por omisión) */
static void ef_ang(Cx X, double *m, double *a) { ca_a_conv(0, X, m, a); }

static void test_ca_cerrados(void)
{
    static Circuito c;
    static Red r;
    const double ef = 1.0 / R2_T;
    int kE, kR, kL, kC, kW, kZ, kS, kJ, e;
    double m, a, f0;

    printf("[45] Etapa 3: el motor de CA, casos cerrados\n");
    (void)kJ;

    /* ---- RC en su corte: E = 10 (eficaz) a 1/(2 pi RC) ---- */
    sr_init(3);
    kE = sr_alterna(SE_E, 0, 1, 10.0, 0.0, 159.15494309189535);
    kR = sr_el(SE_R, 1, 2, 1e3);
    kC = sr_el(SE_C, 2, 0, 1e-6);
    chk_cond("RC en su corte", "se analiza", ca_sr(0.0) == SIM_OK);
    chk_cx("RC en su corte", "V_C = 5 - j5", cx_esc(car.V[kC], ef), cx(5.0, -5.0), 7.07, 1e-12);
    chk_cx("RC en su corte", "I = 5 + j5 mA", cx_esc(car.I[kE], ef), cx(0.005, 0.005), 0.00707, 1e-12);
    chk_cx("RC en su corte", "S(E1) = 0,05 - j0,05", car.S[kE], cx(0.05, -0.05), 0.0707, 1e-12);
    chk_t("RC en su corte", "P_R = 0,05", car.S[kR].re, 0.05, 0.05, 1e-12);
    chk_t("RC en su corte", "Q_C = -0,05", car.S[kC].im, -0.05, 0.05, 1e-12);
    chk_t("RC en su corte", "fp(E1) = -0,7071 (capacitivo)", car.fp[kE], -0.7071067811865475, 1.0, 1e-12);

    /* ---- RLC serie en resonancia ---- */
    sr_init(4);
    kE = sr_alterna(SE_E, 0, 1, 1.0, 0.0, 1591.5494309189535);
    kR = sr_el(SE_R, 1, 2, 10.0);
    kL = sr_el(SE_L, 2, 3, 10e-3);
    kC = sr_el(SE_C, 3, 0, 1e-6);
    chk_cond("RLC en resonancia", "se analiza", ca_sr(0.0) == SIM_OK);
    chk_cx("RLC en resonancia", "I = 0,1" S_ANG "0", cx_esc(car.I[kE], ef), cx(0.1, 0.0), 0.1, 1e-12);
    chk_cx("RLC en resonancia", "V_L = 10" S_ANG "90", cx_esc(car.V[kL], ef), cx(0.0, 10.0), 10.0, 1e-12);
    chk_cx("RLC en resonancia", "V_C = 10" S_ANG "-90", cx_esc(car.V[kC], ef), cx(0.0, -10.0), 10.0, 1e-12);

    /* ---- T1: RLC serie, 120 V a 60 Hz, 10 ohm, 50 mH, 100 uF ---- */
    sr_init(4);
    kE = sr_alterna(SE_E, 0, 1, 120.0, 0.0, 60.0);
    kR = sr_el(SE_R, 1, 2, 10.0);
    kL = sr_el(SE_L, 2, 3, 50e-3);
    kC = sr_el(SE_C, 3, 0, 100e-6);
    chk_cond("T1", "se analiza", ca_sr(0.0) == SIM_OK);
    chk_cond("T1", "una sola fuente: E1", car.fuente == kE);
    chk_cx("T1", "Zeq = 10 - j7,676", car.Zeq, cx(10.0, -7.676267927110466), 12.6, 1e-12);
    chk_cx("T1", "I = 7,5507 + j5,7961", cx_esc(car.I[kE], ef), cx(7.550727234889041, 5.796140529953823), 9.52, 1e-12);
    ef_ang(car.I[kE], &m, &a);
    chk_t("T1", "|I| = 9,51886", m, 9.518861634600457, 9.52, 1e-12);
    chk_t("T1", S_ANG "I = 37,5108" S_DEG, a, 37.510810145539, 1.0, 1e-9);
    ef_ang(car.V[kR], &m, &a);
    chk_t("T1", "|V_R| = 95,1886", m, 95.18861634600458, 95.2, 1e-12);
    chk_t("T1", S_ANG "V_R = 37,51" S_DEG, a, 37.510810145539, 1.0, 1e-9);
    ef_ang(car.V[kL], &m, &a);
    chk_t("T1", "|V_L| = 179,426", m, 179.4263146907912, 179.4, 1e-12);
    chk_t("T1", S_ANG "V_L = 127,51" S_DEG, a, 127.51081014554, 1.0, 1e-9);
    ef_ang(car.V[kC], &m, &a);
    chk_t("T1", "|V_C| = 252,496", m, 252.4956469590769, 252.5, 1e-12);
    chk_t("T1", S_ANG "V_C = -52,49" S_DEG, a, -52.489189854461, 1.0, 1e-9);
    chk_cx("T1", "S(E1) = 906,09 - j695,54", car.S[kE], cx(906.0872681866849, -695.5368635944589), 1142.3, 1e-12);
    chk_t("T1", "|S| = 1142,26", cx_abs(car.S[kE]), 1142.263396152055, 1142.3, 1e-12);
    chk_t("T1", "fp = -0,79324", car.fp[kE], -0.7932384695500382, 1.0, 1e-12);
    chk_t("T1", "Boucherot: P_R", car.S[kR].re, 906.0872681866849, 1142.3, 1e-12);
    chk_t("T1", "Boucherot: Q_L", car.S[kL].im, 1707.9342631479203, 1142.3, 1e-12);
    chk_t("T1", "Boucherot: Q_C", car.S[kC].im, -2403.471126742379, 1142.3, 1e-12);
    chk_cond("T1", "Boucherot: P_L y P_C nulas", fabs(car.S[kL].re) <= 1e-12 * 1142.3 && fabs(car.S[kC].re) <= 1e-12 * 1142.3);
    chk_cond("T1", "Tellegen <= 1e-13", car.tellegen <= 1e-13);
    ca_a_conv(CONV_PICO, car.I[kE], &m, &a);
    chk_t("T1", "con PICO, |I| = 13,4617", m, 13.461703222004896, 13.5, 1e-12);
    ca_a_conv(CONV_SENO, car.I[kE], &m, &a);
    chk_t("T1", "con SENO, " S_ANG "I = 127,51" S_DEG, a, 127.510810145539, 1.0, 1e-9);

    /* ---- Motor con vatímetro: 230 V a 50 Hz, Z1 = 3,703 a 45,57 ---- */
    sr_init(3);
    kE = sr_alterna(SE_E, 0, 1, 230.0, 0.0, 50.0);
    kW = sr_el(SE_AMI, 1, 2, 0.0); sr.el[kW].instr = 'W';
    kZ = sr_z(2, 0, 3.703, 45.572995999194296, 50.0);
    chk_cond("motor", "se analiza", ca_sr(0.0) == SIM_OK);
    chk_t("motor", "P(W1) = 10 000 W", car.lectura[kW], 10000.0, 10000.0, 1e-12);
    chk_t("motor", "fp(W1) = +0,7 (inductivo)", car.fp[kW], 0.7, 1.0, 1e-12);
    chk_t("motor", "|I| = 62,1118 A", cx_abs(car.I[kE]) * ef, 62.11180124223603, 62.1, 1e-12);
    kC = sr_el(SE_C, 2, 0, 416.10139603289948e-6);
    chk_cond("motor", "con C1: se analiza", ca_sr(0.0) == SIM_OK);
    chk_t("motor", "con C1: fp(W1) = 0,95", car.fp[kW], 0.95, 1.0, 1e-12);
    ef_ang(car.I[kE], &m, &a);
    chk_t("motor", "con C1: |I(E1)| = 45,7666", m, 45.76659038901602, 45.8, 1e-12);
    chk_t("motor", "con C1: " S_ANG "I(E1) = -18,195" S_DEG, a, -18.19487233876678, 1.0, 1e-9);
    sr.el[kW].a = 2; sr.el[kW].b = 1;                        /* el W girado */
    sr.nel = 3;                                              /* sin la C */
    chk_cond("motor", "W girado: se analiza", ca_sr(0.0) == SIM_OK);
    chk_t("motor", "W girado: P = -10 000", car.lectura[kW], -10000.0, 10000.0, 1e-12);
    chk_t("motor", "W girado: fp sigue +0,7", car.fp[kW], 0.7, 1.0, 1e-12);
    (void)kZ;

    /* ---- Un generador en fasores ---- */
    sr_init(2);
    kE = sr_el(SE_E, 0, 1, 0.0); sr.el[kE].forma = ONDA_SENO; sr.el[kE].p[0] = 10.0; sr.el[kE].p[1] = 1e3;
    kR = sr_el(SE_R, 1, 0, 100.0);
    chk_cond("generador", "se analiza", ca_sr(0.0) == SIM_OK && !(car.d.avisos & SIM_AV_CC_FUERA));
    ca_a_conv(0, car.V[kE], &m, &a);
    chk_cond("generador", "10 Vp, " S_THETA " = 0: 7,071" S_ANG "-90" S_DEG " (eficaz, coseno)", fabs(m - 7.0710678118654755) < 1e-12 && fabs(a + 90.0) < 1e-9);
    ca_a_conv(CONV_PICO | CONV_SENO, car.V[kE], &m, &a);
    chk_cond("generador", "y 10" S_ANG "0" S_DEG " con pico y seno", fabs(m - 10.0) < 1e-12 && fabs(a) < 1e-9);
    sr.el[kE].p[3] = 2.0;
    chk_cond("generador", "con CC = 2 V: lo mismo y el aviso CC_FUERA", ca_sr(0.0) == SIM_OK && (car.d.avisos & SIM_AV_CC_FUERA) &&
             fabs(cx_abs(car.V[kE]) - 10.0) < 1e-12);

    /* ---- La Z a otra frecuencia ---- */
    {
        SimEl z;
        Cx w;
        memset(&z, 0, sizeof z);
        z.tipo = SE_Z; z.val = 3.0; z.p[0] = 4.0; z.p[1] = 50.0;
        w = ca_z_de(&z, 100.0); chk_cx("Z a otra f", "3 + j4 a 50 Hz: 3 + j8 a 100 Hz", w, cx(3.0, 8.0), 8.5, 1e-15);
        w = ca_z_de(&z, 25.0);  chk_cx("Z a otra f", "3 + j2 a 25 Hz", w, cx(3.0, 2.0), 3.6, 1e-15);
        z.p[0] = -4.0;
        w = ca_z_de(&z, 100.0); chk_cx("Z a otra f", "3 - j4: 3 - j2 a 100 Hz", w, cx(3.0, -2.0), 3.6, 1e-15);
        w = ca_z_de(&z, 25.0);  chk_cx("Z a otra f", "3 - j8 a 25 Hz", w, cx(3.0, -8.0), 8.5, 1e-15);
    }
    {
        static const double F[2] = { 50.0, 100.0 };
        int i, sig;
        for (sig = 0; sig < 2; sig++)
            for (i = 0; i < 2; i++) {
                Cx Iz;
                sr_init(2);
                kE = sr_alterna(SE_E, 0, 1, 10.0, 0.0, F[i]);
                sr_z(1, 0, 5.0, (sig ? -1.0 : 1.0) * 53.13010235415598, 50.0);    /* 3 +- j4 */
                e = ca_sr(0.0);
                Iz = car.I[kE];
                chk_cond("Z a otra f", F[i] == 50.0 ? "a su f, sin aviso" : "a otra f: aviso Z_ESCALA",
                         e == SIM_OK && ((car.d.avisos & SIM_AV_Z_ESCALA) != 0) == (F[i] != 50.0));
                sr_init(3);
                kE = sr_alterna(SE_E, 0, 1, 10.0, 0.0, F[i]);
                sr_el(SE_R, 1, 2, 3.0);
                if (!sig) sr_el(SE_L, 2, 0, 0.012732395447351627);
                else sr_el(SE_C, 2, 0, 795.7747154594767e-6);
                ca_sr(0.0);
                chk_cx("Z a otra f", sig ? "3 - j4 = R 3 con C 795,77 uF" : "3 + j4 = R 3 con L 12,73 mH", Iz, car.I[kE], cx_abs(Iz), 1e-12);
            }
    }

    /* ---- Los S en su estado final ---- */
    sr_init(3);
    kE = sr_alterna(SE_E, 0, 1, 10.0, 0.0, 60.0);
    sr_el(SE_R, 1, 2, 10.0);
    kS = sr_el(SE_S, 2, 0, 0.0); sr.el[kS].cerrado0 = 0; sr.el[kS].t1 = 1e-3;
    chk_cond("S en su estado final", "abierto que cierra: I = 1" S_ANG "0 y el aviso",
             ca_sr(0.0) == SIM_OK && cx_abs(cx_menos(cx_esc(car.I[kE], ef), cx(1.0, 0.0))) < 1e-12 && (car.d.avisos & SIM_AV_S_FINAL));
    sr.el[kS].cerrado0 = 1;
    chk_cond("S en su estado final", "cerrado que abre: I = 0", ca_sr(0.0) == SIM_OK && cx_abs(car.I[kE]) == 0.0);

    /* ---- Errores con nombre ---- */
    sr_init(2);
    kE = sr_alterna(SE_E, 0, 1, 10.0, 0.0, 60.0);
    kR = sr_alterna(SE_E, 0, 1, 10.0, 0.0, 60.0);
    sr_el(SE_R, 1, 0, 10.0);
    e = ca_sr(0.0);
    chk_cond("errores", "dos E alternas en paralelo: LAZO_V con las dos", e == SIM_LAZO_V && (car.d.lazo & 3u) == 3u);
    sr_init(3);
    kJ = sr_alterna(SE_J, 0, 1, 1.0, 0.0, 60.0);
    sr_el(SE_VMI, 1, 2, 0.0);
    sr_el(SE_R, 2, 0, 10.0);
    chk_cond("errores", "J alterna en serie con un V ideal: CORTE_J", ca_sr(0.0) == SIM_CORTE_J);
    sr_init(2);
    kJ = sr_alterna(SE_J, 0, 1, 1.0, 0.0, 60.0);
    kC = sr_el(SE_C, 1, 0, 1e-6);
    chk_cond("errores", "J alterna en serie con una C: en CA sí se puede", ca_sr(0.0) == SIM_OK &&
             fabs(cx_abs(car.V[kC]) - sqrt(2.0) / (2.0 * PI_T * 60.0 * 1e-6)) < 1e-9);
    sr_init(2);
    kE = sr_alterna(SE_E, 0, 1, 10.0, 0.0, 60.0);
    kL = sr_el(SE_L, 1, 0, 10e-3);
    chk_cond("errores", "E alterna en paralelo con una L: en CA sí", ca_sr(0.0) == SIM_OK &&
             fabs(cx_abs(car.I[kL]) - sqrt(2.0) * 10.0 / (2.0 * PI_T * 60.0 * 10e-3)) < 1e-9);
    sr_init(2);
    kE = sr_el(SE_E, 0, 1, 0.0); sr.el[kE].forma = ONDA_CUADRADA; sr.el[kE].p[0] = 1.0; sr.el[kE].p[1] = 1e3; sr.el[kE].p[2] = 50.0;
    sr_el(SE_R, 1, 0, 10.0);
    chk_cond("errores", "una cuadrada: CA_FORMA", ca_sr(0.0) == SIM_CA_FORMA && car.d.elem == kE);
    sr_init(3);
    kE = sr_alterna(SE_E, 0, 1, 10.0, 0.0, 60.0);
    kR = sr_el(SE_E, 1, 2, 0.0); sr.el[kR].forma = ONDA_SENO; sr.el[kR].p[0] = 1.0; sr.el[kR].p[1] = 1e3;
    sr_el(SE_R, 2, 0, 10.0);
    chk_cond("errores", "un generador a 1 kHz con una alterna a 60 Hz: CA_FRECS", ca_sr(60.0) == SIM_CA_FRECS && car.d.elem == kR &&
             car.d.lazo == ((1u << kE) | (1u << kR)));
    sr_init(2);
    sr_el(SE_R, 1, 0, 10.0);
    chk_cond("errores", "sin fuentes ni f: CA_SIN_F", ca_sr(0.0) == SIM_CA_SIN_F);
    circ_init(&c, "sin f");
    kE = circ_elemento(&c, EL_FUENTE, 0, 1, DIR_ARR);
    c.el[kE].variante = FORMA_FASOR; c.el[kE].val.v = 10.0; c.el[kE].val.est = M_DATO;
    kR = circ_elemento(&c, EL_RES, 1, 0, DIR_ABA); c.el[kR].val.v = 10.0; c.el[kR].val.est = M_DATO;
    circ_cable(&c, 0, 0, DIR_DER); circ_cable(&c, 0, 1, DIR_DER);
    red_extraer(&c, &r);
    e = ca_analiza(&c, &r, &car);
    chk_cond("errores", "una alterna sin frec: FALTA la f", e == SIM_FALTA && car.d.campo == Q_FCIRC && car.d.elem == kE);
    c.frec.v = 60.0; c.frec.est = M_DATO;
    e = ca_analiza(&c, &r, &car);
    chk_cond("errores", "con la f: 1" S_ANG "0 A", e == SIM_OK && cx_abs(cx_menos(cx_esc(car.I[kR], ef), cx(1.0, 0.0))) < 1e-12);
    /* resonancia sin pérdidas */
    f0 = 1.0 / (2.0 * PI_T * sqrt(10e-3 * 1e-6));
    sr_init(3);
    kE = sr_el(SE_E, 0, 1, 0.0); sr.el[kE].forma = ONDA_SENO; sr.el[kE].p[0] = 1.0; sr.el[kE].p[1] = f0;
    kL = sr_el(SE_L, 1, 2, 10e-3);
    kC = sr_el(SE_C, 2, 0, 1e-6);
    e = ca_sr(0.0);
    chk_cond("errores", "L y C en serie sobre una E ideal a f0: RESONANCIA", e == SIM_RESONANCIA && (car.d.elem == kL || car.d.elem == kC));
    {
        char msg[300];
        sim_mensaje(&sr, &car.d, msg, sizeof msg);
        chk_cond("errores", "el mensaje nombra a L1 y C1", strstr(msg, "resuenan") != 0 && lineas(msg, 52) <= 4);
    }
    sr.el[kE].p[1] = f0 * (1.0 + 1e-3);
    e = ca_sr(0.0);
    chk_cond("errores", "a f0" S_CDOT "1,001: se analiza sin NaN", e == SIM_OK && cx_abs(car.I[kE]) == cx_abs(car.I[kE]));
    chk_t("errores", "|I| = 5,0025 A", cx_abs(car.I[kE]), 5.0024987506245475, 5.0, 1e-9);
    /* una Z con solo una E continua */
    circ_init(&c, "Z y CC");
    kE = circ_elemento(&c, EL_FUENTE, 0, 1, DIR_ARR); c.el[kE].val.v = 12.0; c.el[kE].val.est = M_DATO;
    kZ = circ_elemento(&c, EL_IMP, 1, 0, DIR_ABA);
    c.el[kZ].val.v = 5.0; c.el[kZ].val.est = M_DATO; c.el[kZ].val.ang = 30.0; c.el[kZ].val.est_ang = M_DATO;
    circ_cable(&c, 0, 0, DIR_DER); circ_cable(&c, 0, 1, DIR_DER);
    c.frec.v = 60.0; c.frec.est = M_DATO;
    red_extraer(&c, &r);
    e = ca_analiza(&c, &r, &car);
    chk_cond("errores", "una Z con una E continua: todo 0 y el aviso CC_FUERA", e == SIM_OK && cx_abs(car.I[kZ]) == 0.0 &&
             cx_abs(car.V[kZ]) == 0.0 && (car.d.avisos & SIM_AV_CC_FUERA));
    {
        char msg[300];
        ca_aviso(&c, &car, SIM_AV_CC_FUERA, msg, sizeof msg);
        chk_cond("errores", "el aviso nombra a E1", strncmp(msg, "E1 ", 3) == 0 && lineas(msg, 52) <= 4);
    }

    /* ---- Convenios ---- */
    {
        double mo, an, efv, ang;
        ca_canon_a_conv(CONV_PICO, 120.0, 30.0, &mo, &an);
        chk_cond("convenios", "120 eficaz a 30: 169,7056" S_ANG "30 con pico", fabs(mo - 169.7056274847714) < 1e-12 && an == 30.0);
        ca_canon_a_conv(CONV_SENO, 120.0, 30.0, &mo, &an);
        chk_cond("convenios", "120" S_ANG "120 con seno", mo == 120.0 && an == 120.0);
        ca_canon_a_conv(CONV_PICO | CONV_SENO, 120.0, 30.0, &mo, &an);
        chk_cond("convenios", "169,7056" S_ANG "120 con los dos", fabs(mo - 169.7056274847714) < 1e-12 && an == 120.0);
        ca_conv_a_canon(CONV_PICO | CONV_SENO, mo, an, &efv, &ang);
        chk_cond("convenios", "ida y vuelta exacta", fabs(efv - 120.0) <= 1e-15 * 120.0 && fabs(ang - 30.0) <= 1e-15 * 30.0);
        ca_conv_a_canon(CONV_PICO, 170.0, 0.0, &efv, &ang);
        chk_t("convenios", "con pico, 170 se guarda 120,208", efv, 120.20815280171308, 120.0, 1e-15);
        {
            Cx X = ca_de_conv(CONV_SENO, 10.0, 0.0), Y;
            ca_a_conv(CONV_SENO, X, &mo, &an);
            chk_cond("convenios", "de_conv y a_conv: la vuelta", fabs(mo - 10.0) < 1e-12 && fabs(an) < 1e-12);
            Y = ca_de_conv(0, 1.0, 0.0);
            chk_cx("convenios", "1 eficaz" S_ANG "0 es sqrt 2 de pico", Y, cx(R2_T, 0.0), 1.0, 1e-15);
        }
    }
}

/* --------------------------------------------------------------------------
 *  46. Etapa 3: cruces al azar
 * ----------------------------------------------------------------------- */
static unsigned azar_ca = 777u;
static double u_ca(void) { azar_ca = azar_ca * 1103515245u + 12345u; return (double)((azar_ca >> 8) & 0xFFFFFF) / 16777216.0; }

/* Red de CA al azar: un árbol de R que la conecta y luego R, C, L, Z, E y J
 * alternas o generadores a f, instrumentos (V, A ideales o con R, W) y S */
static void red_ca_azar(double f)
{
    int nn = 3 + (int)(u_ca() * 5.0), n, k, nel, q;
    sr_init(nn);
    for (n = 1; n < nn; n++) sr_el(SE_R, (int)(u_ca() * n), n, 10.0 + 990.0 * u_ca());
    q = sr_el(SE_E, 0, 1, u_ca() < 0.3 ? 1.0 + 20.0 * u_ca() : 0.0);           /* una fuente seguro */
    sr.el[q].forma = ONDA_SENO; sr.el[q].p[0] = 1.0 + 20.0 * u_ca(); sr.el[q].p[1] = f; sr.el[q].p[2] = 360.0 * u_ca();
    nel = 2 + (int)(u_ca() * 8.0);
    for (k = 0; k < nel && sr.nel < MAX_EL; k++) {
        int a = (int)(u_ca() * nn), b = (int)(u_ca() * nn);
        double r = u_ca(), w = 2.0 * PI_T * f;
        if (a == b) b = (a + 1) % nn;
        if (r < 0.15) sr_el(SE_R, a, b, 10.0 + 990.0 * u_ca());
        else if (r < 0.3) sr_el(SE_C, a, b, (0.2 + 5.0 * u_ca()) / (w * 300.0));
        else if (r < 0.45) sr_el(SE_L, a, b, (0.2 + 5.0 * u_ca()) * 300.0 / w);
        else if (r < 0.55) sr_z(a, b, 10.0 + 990.0 * u_ca(), 180.0 * u_ca() - 90.0, f);
        else if (r < 0.62) { q = sr_el(SE_E, a, b, u_ca() < 0.5 ? 1.0 + 50.0 * u_ca() : 0.0); sr.el[q].forma = ONDA_SENO;
                             sr.el[q].p[0] = 10.0 * u_ca(); sr.el[q].p[1] = f; sr.el[q].p[2] = 360.0 * u_ca(); }
        else if (r < 0.69) { q = sr_el(SE_J, a, b, 0.0); sr.el[q].forma = ONDA_SENO;
                             sr.el[q].p[0] = 0.05 * u_ca(); sr.el[q].p[1] = f; sr.el[q].p[2] = 360.0 * u_ca(); }
        else if (r < 0.74) { q = sr_el(SE_VMI, a, b, 0.0); sr.el[q].instr = 'V'; }
        else if (r < 0.79) { q = sr_el(SE_AMI, a, b, 0.0); sr.el[q].instr = 'A'; }
        else if (r < 0.84) { q = sr_el(SE_R, a, b, 1e3 + 1e5 * u_ca()); sr.el[q].instr = u_ca() < 0.5 ? 'V' : 'A'; }
        else if (r < 0.9) { q = sr_el(SE_AMI, a, b, 0.0); sr.el[q].instr = 'W'; }
        else { q = sr_el(SE_S, a, b, 0.0); sr.el[q].cerrado0 = (unsigned char)(u_ca() < 0.5); if (u_ca() < 0.5) sr.el[q].t1 = 1e-3; }
    }
}

/* Red de CC al azar para ω -> 0: R, C, L, S (sin t1 ni t2), E y J continuas,
 * con V0 e I0 vacías */
static void red_cc_azar(void)
{
    int nn = 3 + (int)(u_ca() * 5.0), n, k, nel, q;
    sr_init(nn);
    for (n = 1; n < nn; n++) sr_el(SE_R, (int)(u_ca() * n), n, 10.0 + 990.0 * u_ca());
    q = sr_el(SE_E, 0, 1, u_ca() < 0.3 ? 1.0 + 20.0 * u_ca() : 0.0);
    sr.el[q].forma = ONDA_CC; sr.el[q].p[0] = 20.0 * u_ca() - 10.0;
    nel = 2 + (int)(u_ca() * 7.0);
    for (k = 0; k < nel && sr.nel < MAX_EL; k++) {
        int a = (int)(u_ca() * nn), b = (int)(u_ca() * nn);
        double r = u_ca();
        if (a == b) b = (a + 1) % nn;
        if (r < 0.25) sr_el(SE_R, a, b, 10.0 + 990.0 * u_ca());
        else if (r < 0.45) sr_el(SE_C, a, b, 1e-7 + 1e-5 * u_ca());
        else if (r < 0.65) sr_el(SE_L, a, b, 1e-3 + 1e-1 * u_ca());
        else if (r < 0.75) { q = sr_el(SE_E, a, b, u_ca() < 0.5 ? 1.0 + 50.0 * u_ca() : 0.0); sr.el[q].forma = ONDA_CC; sr.el[q].p[0] = 20.0 * u_ca() - 10.0; }
        else if (r < 0.85) { q = sr_el(SE_J, a, b, 0.0); sr.el[q].forma = ONDA_CC; sr.el[q].p[0] = 0.02 * u_ca() - 0.01; }
        else { q = sr_el(SE_S, a, b, 0.0); sr.el[q].cerrado0 = (unsigned char)(u_ca() < 0.5); }
    }
}

static void test_ca_cruces(void)
{
    static Valores v;
    static SimRed guarda;
    SimDiag d;
    int caso, k, n, n_ok = 0, n_bou = 0, n_w0 = 0, n_topo = 0, malos_tel = 0, malos_kcl = 0, malos_bou = 0;
    int malos_w0 = 0, malos_topo = 0, salta = 0;
    double peor_tel = 0.0, peor_kcl = 0.0, peor_bou = 0.0, peor_re = 0.0, peor_im = 0.0;

    printf("[46] Etapa 3: cruces al azar\n");

    /* ---- (a) Tellegen y ley de nodos; (b) Boucherot con las fórmulas ---- */
    for (caso = 0; caso < 1000; caso++) {
        double f = pow(10.0, 1.0 + 4.0 * u_ca()), w = 2.0 * PI_T * f, esc = 0.0, P = 0.0, Q = 0.0, sabs = 0.0;
        red_ca_azar(f);
        if (ca_sr(0.0) != SIM_OK) continue;
        n_ok++;
        if (car.tellegen > peor_tel) peor_tel = car.tellegen;
        if (!(car.tellegen <= 1e-12)) malos_tel++;
        for (k = 0; k < sr.nel; k++) if (cx_abs(car.I[k]) > esc) esc = cx_abs(car.I[k]);
        for (n = 0; n < sr.nn; n++) {
            Cx suma = cx(0.0, 0.0);
            for (k = 0; k < sr.nel; k++) {
                if (sr.el[k].a == n) suma = cx_mas(suma, car.I[k]);
                if (sr.el[k].b == n) suma = cx_menos(suma, car.I[k]);
            }
            if (sr.ref[n]) continue;
            if (cx_abs(suma) > peor_kcl * (esc + 1e-300)) peor_kcl = cx_abs(suma) / (esc + 1e-300);
            if (!(cx_abs(suma) <= 1e-12 * (esc + 1e-300))) malos_kcl++;
        }
        /* Boucherot: cada elemento con su fórmula */
        for (k = 0; k < sr.nel; k++) {
            const SimEl *e = &sr.el[k];
            double i2 = cx_abs(car.I[k]) * cx_abs(car.I[k]), v2 = cx_abs(car.V[k]) * cx_abs(car.V[k]);
            switch (e->tipo) {
            case SE_R: P += 0.5 * e->val * i2; break;
            case SE_L: Q += 0.5 * w * e->val * i2; break;
            case SE_C: Q -= 0.5 * w * e->val * v2; break;
            case SE_Z: { Cx z = ca_z_de(e, f); P += 0.5 * z.re * i2; Q += 0.5 * z.im * i2; break; }
            default: break;
            }
            sabs += cx_abs(car.S[k]);
        }
        if (sabs > 0.0) {
            double eb = (fabs(P - car.Sfuentes.re) + fabs(Q - car.Sfuentes.im)) / sabs;
            n_bou++;
            if (eb > peor_bou) peor_bou = eb;
            if (!(eb <= 1e-10)) malos_bou++;
        }
    }
    printf("    %d redes de CA al azar analizadas (Tellegen %.1e, nodos %.1e, Boucherot %.1e)\n", n_ok, peor_tel, peor_kcl, peor_bou);
    chk_cond("cruces", "Tellegen <= 1e-12 en todas", malos_tel == 0);
    chk_cond("cruces", "ley de nodos <= 1e-12 de la escala", malos_kcl == 0);
    chk_cond("cruces", "Boucherot con las fórmulas de cada elemento (1e-10)", malos_bou == 0);
    chk_cond("cruces", "al menos 500 redes se analizan", n_ok >= 500);

    /* ---- (c) ω -> 0: el punto de operación, CARGA y FLUJO incluidas ---- */
    for (caso = 0; caso < 1000; caso++) {
        double esc = 1e-300, eri = 0.0, eim = 0.0;
        red_cc_azar();
        guarda = sr;
        if (sim_op_red(&sr, &v, &d) != SIM_OK) continue;
        if (d.avisos & (SIM_AV_C_DESC | SIM_AV_L_SIN_I | SIM_AV_CI_SALTA)) { salta++; continue; }
        for (k = 0; k < sr.nel; k++)
            if ((sr.el[k].tipo == SE_E || sr.el[k].tipo == SE_J) && sr.el[k].forma == ONDA_CC) {
                double v0 = sr.el[k].p[0];
                sr.el[k].forma = ONDA_SENO;
                sr.el[k].p[0] = v0; sr.el[k].p[1] = 1e-6; sr.el[k].p[2] = 90.0; sr.el[k].p[3] = 0.0;
            }
        if (ca_sr(0.0) != SIM_OK) { sr = guarda; continue; }
        n_w0++;
        for (n = 0; n < sr.nn; n++) if (fabs(v.phi[n]) > esc) esc = fabs(v.phi[n]);
        for (k = 0; k < sr.nel; k++) {
            if (fabs(v.V[k]) > esc) esc = fabs(v.V[k]);
            if (fabs(v.I[k]) * 1e3 > esc) esc = fabs(v.I[k]) * 1e3;
        }
        for (n = 0; n < sr.nn; n++) {
            if (fabs(car.phi[n].re - v.phi[n]) > eri) eri = fabs(car.phi[n].re - v.phi[n]);
            if (fabs(car.phi[n].im) > eim) eim = fabs(car.phi[n].im);
        }
        for (k = 0; k < sr.nel; k++) {
            if (fabs(car.V[k].re - v.V[k]) > eri) eri = fabs(car.V[k].re - v.V[k]);
            if (fabs(car.I[k].re - v.I[k]) * 1e3 > eri) eri = fabs(car.I[k].re - v.I[k]) * 1e3;
            if (fabs(car.V[k].im) > eim) eim = fabs(car.V[k].im);
            if (fabs(car.I[k].im) * 1e3 > eim) eim = fabs(car.I[k].im) * 1e3;
        }
        if (eri / esc > peor_re) peor_re = eri / esc;
        if (eim / esc > peor_im) peor_im = eim / esc;
        if (!(eri <= 1e-9 * esc && eim <= 1e-5 * esc)) malos_w0++;
        sr = guarda;
    }
    printf("    " S_OMEGA " -> 0: %d redes comparadas con el punto de operación (%d sin régimen); real %.1e, imaginaria %.1e\n",
           n_w0, salta, peor_re, peor_im);
    chk_cond("cruces", S_OMEGA " -> 0: igual que el punto de operación", malos_w0 == 0);
    chk_cond("cruces", S_OMEGA " -> 0: al menos 700 redes", n_w0 >= 700);

    /* ---- (e) Topología: si el emulador corre, la CA también; si la CA da
     *      LAZO_V o CORTE_J, el emulador también falla ---- */
    for (caso = 0; caso < 500; caso++) {
        static SimTopoCA t;
        SimAjustes aj;
        int e_ca, e_emu;
        red_ca_azar(1e3);
        e_ca = sim_topo_ca(&sr, &t, &d);
        memset(&aj, 0, sizeof aj);
        aj.t_fin = 3e-3; aj.h = 1e-5; aj.max_pasos = 400;
        e_emu = sim_emular(&sr, &aj);
        if (e_emu == SIM_SOLO_CA) continue;
        n_topo++;
        if (e_emu == SIM_OK && e_ca != SIM_OK) malos_topo++;
        if ((e_ca == SIM_LAZO_V || e_ca == SIM_CORTE_J) && e_emu == SIM_OK) malos_topo++;
    }
    printf("    topología: %d redes contra el emulador\n", n_topo);
    chk_cond("cruces", "topología de CA coherente con la del emulador", malos_topo == 0 && n_topo >= 200);
}

/* --------------------------------------------------------------------------
 *  47. Etapa 3: Thévenin, Norton y ohmímetro
 * ----------------------------------------------------------------------- */
static int pon_val(Circuito *c, int k, double v) { c->el[k].val.v = v; c->el[k].val.est = M_DATO; return k; }

/* E de (0,1) a (0,0) (alterna si f > 0), R1 de (0,0) a (1,0) = A y X de
 * (1,0) a (1,1) = B; cable de (1,1) a (0,1) */
static void red_t6(Circuito *c, double E, double f, int tx, double vx)
{
    int k;
    circ_init(c, "T6");
    k = circ_elemento(c, EL_FUENTE, 0, 1, DIR_ARR);
    pon_val(c, k, E);
    if (f > 0.0) { c->el[k].variante = FORMA_FASOR; c->frec.v = f; c->frec.est = M_DATO; }
    pon_val(c, circ_elemento(c, EL_RES, 0, 0, DIR_DER), 100.0);
    k = circ_elemento(c, (TipoEl)tx, 1, 0, DIR_ABA);
    if (tx != EL_INTER) pon_val(c, k, vx);
    circ_cable(c, 1, 1, DIR_IZQ);
    c->term_x[0] = 1; c->term_y[0] = 0; c->term_x[1] = 1; c->term_y[1] = 1;
}

static void test_thevenin(void)
{
    static Circuito c;
    static Red r;
    static Solucion so;
    static CaThev t;
    const double ef = 1.0 / R2_T;
    int k, e;

    printf("[47] Etapa 3: Thévenin, Norton y ohmímetro\n");

    /* ---- T6: E = 10 (eficaz) a 1 kHz, R1 = 100, C1 = 1 uF entre A y B ---- */
    red_t6(&c, 10.0, 1e3, EL_COND, 1e-6);
    red_extraer(&c, &r);
    e = ca_thevenin(&c, &r, 1e3, &t);
    chk_cond("T6", "se calcula, con Thévenin y Norton", e == SIM_OK && t.hay_th && t.hay_no && t.max_finito);
    chk_cx("T6", "Zth = 71,696 - j45,048", t.Zth, cx(71.69568003248978, -45.04772433683886), 84.7, 1e-12);
    chk_cx("T6", "Vth = 7,1696 - j4,5048 (eficaz)", cx_esc(t.Vth, ef), cx(7.169568003248978, -4.504772433683886), 8.47, 1e-12);
    chk_cx("T6", "IN = 0,1" S_ANG "0 (eficaz)", cx_esc(t.In, ef), cx(0.1, 0.0), 0.1, 1e-12);
    chk_cx("T6", "ZL = Zth*", t.Zl, cx(71.69568003248978, 45.04772433683886), 84.7, 1e-12);
    chk_t("T6", "su parte reactiva: L = 7,1696 mH", t.Lc, 7.169568003248979e-3, 7.17e-3, 1e-12);
    chk_cond("T6", "sin C", t.Cc == 0.0);
    chk_t("T6", "Pmax = 0,25 W (= E" S_SUP2 "/4R1: la C no disipa)", t.Pmax, 0.25, 0.25, 1e-12);
    chk_t("T6", "solo R: 84,673 " S_OHM, t.Rsola, 84.67330159648304, 84.7, 1e-12);
    chk_t("T6", "solo R: 0,22925 W", t.Psola, 0.22925160503573186, 0.23, 1e-12);

    /* ---- El puente a 10^4 rad/s ---- */
    {
        int kr2;
        double f = 1591.5494309189535;
        circ_init(&c, "puente");
        k = circ_elemento(&c, EL_FUENTE, 0, 2, DIR_ARR);
        pon_val(&c, k, 10.0); c.el[k].variante = FORMA_FASOR;
        c.frec.v = f; c.frec.est = M_DATO;
        circ_cable(&c, 0, 1, DIR_ARR);
        circ_cable(&c, 0, 0, DIR_DER); circ_cable(&c, 1, 0, DIR_DER);
        pon_val(&c, circ_elemento(&c, EL_RES, 1, 0, DIR_ABA), 100.0);
        pon_val(&c, circ_elemento(&c, EL_COND, 1, 1, DIR_ABA), 1e-6);
        pon_val(&c, circ_elemento(&c, EL_BOB, 2, 0, DIR_ABA), 10e-3);
        kr2 = pon_val(&c, circ_elemento(&c, EL_RES, 2, 1, DIR_ABA), 200.0);
        circ_cable(&c, 1, 2, DIR_IZQ); circ_cable(&c, 2, 2, DIR_IZQ);
        c.term_x[0] = 1; c.term_y[0] = 1; c.term_x[1] = 2; c.term_y[1] = 1;
        red_extraer(&c, &r);
        e = ca_thevenin(&c, &r, f, &t);
        chk_cond("puente", "se calcula", e == SIM_OK && t.hay_no && t.max_finito);
        chk_cx("puente", "Vth = -3 - j1 (eficaz)", cx_esc(t.Vth, ef), cx(-3.0, -1.0), 3.16, 1e-12);
        chk_cx("puente", "Zth = 90 + j30", t.Zth, cx(90.0, 30.0), 94.9, 1e-12);
        chk_cx("puente", "IN = -1/30 (eficaz)", cx_esc(t.In, ef), cx(-1.0 / 30.0, 0.0), 0.0333, 1e-12);
        chk_cx("puente", "ZL = 90 - j30", t.Zl, cx(90.0, -30.0), 94.9, 1e-12);
        chk_t("puente", "con una C de 3,333 uF", t.Cc, 3.333333333333333e-6, 3.33e-6, 1e-12);
        chk_t("puente", "Pmax = 1/36 W", t.Pmax, 1.0 / 36.0, 1.0 / 36.0, 1e-12);
        chk_t("puente", "solo R: 94,868 " S_OHM, t.Rsola, 94.86832980505137, 94.9, 1e-12);
        chk_t("puente", "solo R: 0,027046 W", t.Psola, 0.0270462766947299, 0.027, 1e-12);
        c.el[kr2].val.v = 100.0;
        red_extraer(&c, &r);
        e = ca_thevenin(&c, &r, f, &t);
        chk_cond("puente", "con R2 = 100 se equilibra: Vth = 0", e == SIM_OK && cx_abs(t.Vth) <= 1e-12 * 10.0);
    }

    /* ---- CC: E = 12, R1 = 4 hasta A, R2 = 12 de A a B ---- */
    red_t6(&c, 12.0, 0.0, EL_RES, 12.0);
    c.el[1].val.v = 4.0;
    red_extraer(&c, &r);
    e = ca_thevenin(&c, &r, 0.0, &t);
    chk_cond("CC", "se calcula", e == SIM_OK && t.hay_th && t.hay_no && t.max_finito && !t.inf);
    chk_t("CC", "Vth = 9 V", t.Vth.re, 9.0, 9.0, 1e-12);
    chk_t("CC", "Rth = 3 " S_OHM, t.Zth.re, 3.0, 3.0, 1e-12);
    chk_t("CC", "IN = 3 A", t.In.re, 3.0, 3.0, 1e-12);
    chk_t("CC", "Pmax = 6,75 W", t.Pmax, 6.75, 6.75, 1e-12);
    resolver(&c, &r, &so);
    {
        double vab;
        int na, nb;
        chk_cond("CC", "la tabla: V(AB) en vacío = 9", so.estado == SOL_OK && sol_vab(&c, &r, &so.s[0], &vab, &na, &nb) == 1 &&
                 fabs(fabs(vab) - 9.0) < 1e-12);
    }
    circ_quitar(&c, 1, 0, DIR_ABA);
    k = circ_elemento(&c, EL_AMP, 1, 0, DIR_ABA);
    red_extraer(&c, &r);
    resolver(&c, &r, &so);
    chk_cond("CC", "la tabla: con un A ideal entre A y B, IN = 3 A", so.estado == SOL_OK && fabs(fabs(so.s[0].I[k]) - 3.0) < 1e-12);

    /* ---- Casos ---- */
    red_t6(&c, 10.0, 0.0, EL_FUENTE, 10.0);
    red_extraer(&c, &r);
    e = ca_thevenin(&c, &r, 0.0, &t);
    chk_cond("casos", "una E ideal entre A y B (CC): Zth = 0 exacto, sin Norton", e == SIM_OK && t.Zth.re == 0.0 && !t.hay_no && !t.max_finito);
    red_t6(&c, 10.0, 60.0, EL_FUENTE, 10.0);
    c.el[2].variante = FORMA_FASOR;
    red_extraer(&c, &r);
    e = ca_thevenin(&c, &r, 60.0, &t);
    chk_cond("casos", "y en CA", e == SIM_OK && cx_abs(t.Zth) == 0.0 && !t.hay_no);
    red_t6(&c, 10.0, 0.0, EL_RES, 12.0);
    c.term_x[1] = 5; c.term_y[1] = 5;
    k = circ_elemento(&c, EL_RES, 5, 5, DIR_DER); pon_val(&c, k, 1.0);
    red_extraer(&c, &r);
    chk_cond("casos", "A y B en circuitos separados: AB_SEPARADOS", ca_thevenin(&c, &r, 0.0, &t) == SIM_AB_SEPARADOS);
    {
        char msg[300];
        sim_mensaje_circ(&c, 0, &t.d, msg, sizeof msg);
        chk_cond("casos", "con su mensaje", strstr(msg, "A y B no est") != 0);
    }
    c.term_x[0] = -1; c.term_y[0] = -1;
    chk_cond("casos", "sin terminales: SIN_AB", ca_thevenin(&c, &r, 0.0, &t) == SIM_SIN_AB);
    /* E, R y un C hasta A: en CC R infinita y Vth = E (la regla de la carga) */
    circ_init(&c, "C a A");
    pon_val(&c, circ_elemento(&c, EL_FUENTE, 0, 1, DIR_ARR), 10.0);
    pon_val(&c, circ_elemento(&c, EL_RES, 0, 0, DIR_DER), 100.0);
    pon_val(&c, circ_elemento(&c, EL_COND, 1, 0, DIR_DER), 1e-6);
    c.term_x[0] = 2; c.term_y[0] = 0; c.term_x[1] = 0; c.term_y[1] = 1;
    red_extraer(&c, &r);
    e = ca_thevenin(&c, &r, 0.0, &t);
    chk_cond("casos", "un C hasta A en CC: R infinita y Vth = E", e == SIM_OK && t.inf && !t.hay_th && fabs(t.Vth.re - 10.0) < 1e-12);
    /* una J alterna sola entre A y B: CORTE_J */
    circ_init(&c, "J sola");
    k = circ_elemento(&c, EL_CORR, 0, 1, DIR_ARR);
    pon_val(&c, k, 1.0); c.el[k].variante = FORMA_FASOR;
    c.frec.v = 60.0; c.frec.est = M_DATO;
    c.term_x[0] = 0; c.term_y[0] = 0; c.term_x[1] = 0; c.term_y[1] = 1;
    red_extraer(&c, &r);
    chk_cond("casos", "una J alterna sola entre A y B: CORTE_J", ca_thevenin(&c, &r, 60.0, &t) == SIM_CORTE_J);
    /* una L sola en CC entre A y B */
    red_t6(&c, 10.0, 0.0, EL_BOB, 1e-3);
    red_extraer(&c, &r);
    e = ca_thevenin(&c, &r, 0.0, &t);
    chk_cond("casos", "una L entre A y B en CC: Rth = 0, sin máximo", e == SIM_OK && t.Zth.re == 0.0 && !t.max_finito);

    /* ---- Ohmímetro: Z(AB) sin fuentes ---- */
    circ_init(&c, "ohm");
    pon_val(&c, circ_elemento(&c, EL_RES, 1, 0, DIR_ABA), 1e3);
    pon_val(&c, circ_elemento(&c, EL_RES, 2, 0, DIR_ABA), 1e3);
    circ_cable(&c, 1, 0, DIR_DER); circ_cable(&c, 1, 1, DIR_DER);
    c.term_x[0] = 1; c.term_y[0] = 0; c.term_x[1] = 1; c.term_y[1] = 1;
    red_extraer(&c, &r);
    e = ca_thevenin(&c, &r, 0.0, &t);
    chk_cond("ohmímetro", "1 k" S_OHM " en paralelo con 1 k" S_OHM ": 500 " S_OHM, e == SIM_OK && fabs(t.Zth.re - 500.0) < 1e-12 && t.Vth.re == 0.0);
    circ_init(&c, "tanque");
    pon_val(&c, circ_elemento(&c, EL_RES, 1, 0, DIR_ABA), 5.0);
    pon_val(&c, circ_elemento(&c, EL_BOB, 1, 1, DIR_ABA), 10e-3);
    pon_val(&c, circ_elemento(&c, EL_COND, 2, 0, DIR_ABA), 1e-6);
    circ_cable(&c, 1, 0, DIR_DER); circ_cable(&c, 2, 1, DIR_ABA); circ_cable(&c, 1, 2, DIR_DER);
    c.term_x[0] = 1; c.term_y[0] = 0; c.term_x[1] = 1; c.term_y[1] = 2;
    red_extraer(&c, &r);
    e = ca_thevenin(&c, &r, 1589.5587491756316, &t);
    chk_cond("ohmímetro", "el tanque se calcula", e == SIM_OK);
    chk_cx("ohmímetro", "el tanque a su fase 0: Z(AB) = 2000 " S_OHM, t.Zth, cx(2000.0, 0.0), 2000.0, 1e-9);
}

/* --------------------------------------------------------------------------
 *  46 (d). El fasor a ω~ = (2/h)·tan(ωh/2) es el régimen del trapecio
 * ----------------------------------------------------------------------- */
/* Red RLC al azar con una sola fuente senoidal de frecuencia f, un V y un A
 * en modo CA y a veces un W; kc0 y kc1: los elementos de los canales */
static int red_rlc_seno(double f, int *kc0, int *kc1, int *kv, int *ka, int *kw)
{
    int nn = 3 + (int)(u_ca() * 5.0), n, k, nel, q, kf;
    double w = 2.0 * PI_T * f, z0 = 100.0 + 900.0 * u_ca();
    sr_init(nn);
    for (n = 1; n < nn; n++) sr_el(SE_R, (int)(u_ca() * n), n, z0 * (0.2 + 2.0 * u_ca()));
    kf = sr_el(u_ca() < 0.7 ? SE_E : SE_J, 0, 1, 0.0);
    if (sr.el[kf].tipo == SE_E && u_ca() < 0.3) sr.el[kf].val = z0 * u_ca();
    sr.el[kf].forma = ONDA_SENO;
    sr.el[kf].p[0] = sr.el[kf].tipo == SE_E ? 1.0 + 10.0 * u_ca() : (1.0 + 10.0 * u_ca()) / z0;
    sr.el[kf].p[1] = f; sr.el[kf].p[2] = 360.0 * u_ca();
    *kc0 = *kc1 = -1;
    nel = 2 + (int)(u_ca() * 6.0);
    for (k = 0; k < nel && sr.nel < MAX_EL - 4; k++) {
        int a = (int)(u_ca() * nn), b = (int)(u_ca() * nn);
        double r = u_ca();
        if (a == b) b = (a + 1) % nn;
        if (r < 0.3) q = sr_el(SE_R, a, b, z0 * (0.2 + 2.0 * u_ca()));
        else if (r < 0.65) q = sr_el(SE_C, a, b, 1.0 / (w * z0 * (0.3 + 3.0 * u_ca())));
        else q = sr_el(SE_L, a, b, z0 * (0.3 + 3.0 * u_ca()) / w);
        if (*kc0 < 0 && sr.el[q].tipo != SE_R) *kc0 = q;
        else if (*kc1 < 0) *kc1 = q;
    }
    if (*kc0 < 0) *kc0 = 1;
    if (*kc1 < 0) *kc1 = 2;
    /* instrumentos en modo CA: un V entre dos nodos, un A y quizá un W en serie con una R nueva */
    *kv = sr_el(SE_VMI, 0, nn - 1, 0.0); sr.el[*kv].instr = 'V'; sr.el[*kv].med = MED_CA;
    n = sr.nn++;
    *ka = sr_el(SE_AMI, (int)(u_ca() * nn), n, 0.0); sr.el[*ka].instr = 'A'; sr.el[*ka].med = MED_CA;
    sr_el(SE_R, n, (int)(u_ca() * nn), z0 * (0.2 + 2.0 * u_ca()));
    *kw = -1;
    if (u_ca() < 0.5) {
        n = sr.nn++;
        *kw = sr_el(SE_AMI, (int)(u_ca() * nn), n, 0.0); sr.el[*kw].instr = 'W';
        sr_el(SE_R, n, (int)(u_ca() * nn), z0 * (0.2 + 2.0 * u_ca()));
    }
    return kf;
}

/* El mayor error del canal c en el último periodo contra Re{X·e^(jωt)}.
 *  - Sin su media: una C sin camino resistivo (o un lazo de L sin R) guarda la
 *    carga (el flujo) que le dejó el arranque del seno, una constante que el
 *    fasor no ve (es el régimen de la parte alterna, como la lectura en CA).
 *  - Con la media de cada par de muestras seguidas, que cancela el modo
 *    (-1)^n del trapecio: en un elemento algebraico (un C sobre una E ideal)
 *    no se amortigua nunca, y su amplitud la fija la historia del arranque
 *    (la BDF2, con un error de (ωh)^4). En *bruto, sin cancelarlo. */
static double err_ultimo_periodo(int c, Cx X, double w, double t_fin, double T, double *bruto)
{
    const SimReg *g = sim_reg();
    static double d[SIM_MAX_MUESTRAS];
    double e = 0.0, eb = 0.0, media = 0.0;
    int i, n = 0;
    for (i = 0; i < g->n; i++) {
        double t = g->t[i];
        if (t < t_fin - T * (1.0 - 1e-9)) continue;          /* el periodo (t_fin - T, t_fin] */
        d[n++] = g->y[c][i] - (X.re * cos(w * t) - X.im * sin(w * t));
    }
    for (i = 0; i < n; i++) media += d[i];
    if (n) media /= n;
    for (i = 0; i < n; i++) {
        double di = d[i] - media;
        if (fabs(di) > eb) eb = fabs(di);
        if (i > 0 && fabs(0.5 * (di + d[i - 1] - media)) > e) e = fabs(0.5 * (di + d[i - 1] - media));
    }
    if (bruto) *bruto = eb;
    return e;
}

static void test_fasor_trapecio(void)
{
    static SimModo md[SIM_MAX_MODOS];
    static CaRes a_t, a_w;
    int caso, n_ok = 0, malos_m = 0, malos_b = 0, malos_l = 0, n_ord = 0, malos_ord = 0, malos_abs = 0;
    double peor_m = 0.0, peor_b = 0.0, peor_l = 0.0, rmin = 1e30, rmax = 0.0, peor_abs = 0.0;

    printf("[46] (sigue) el fasor a " S_OMEGA "~ es el régimen del trapecio\n");
    for (caso = 0; caso < 300; caso++) {
        double f = 100.0 * pow(10.0, 2.0 * u_ca()), T = 1.0 / f, w = 2.0 * PI_T * f, h = T / 100.0;
        double taumax = 0.0, taumin = 1e30, tf, hr, wt, esc, e0, e1, b0, b1, amp;
        int kc0, kc1, kv, ka, kw, kf, nm = 0, i, malo = 0;
        kf = red_rlc_seno(f, &kc0, &kc1, &kv, &ka, &kw);
        sim_modos_prueba(&sr, h, md, &nm);
        if (nm == 0) continue;
        for (i = 0; i < nm; i++) {
            double mag = sqrt(md[i].re * md[i].re + md[i].im * md[i].im);
            if (fabs(md[i].re) < 1e-3 * mag) malo = 1;                   /* LC sin pérdidas */
            if (md[i].re < 0.0) {
                if (-1.0 / md[i].re > taumax) taumax = -1.0 / md[i].re;
                if (-1.0 / md[i].re < taumin) taumin = -1.0 / md[i].re;
            }
        }
        if (malo || taumax <= 0.0 || taumin < 0.5 * h) continue;
        tf = ceil(40.0 * taumax / T) * T + 4.0 * T;
        if (tf / h > 20000.0) continue;
        canal(0, CANAL_V, kc0); canal(1, CANAL_I, kc1);
        saj.h = h; saj.t_fin = tf;
        if (sim_emular(&sr, &saj) != SIM_OK || sim_res()->fin != FIN_COMPLETO) continue;
        hr = sim_res()->h;
        wt = (2.0 / hr) * tan(0.5 * w * hr);
        /* la CA a f~ = ω~/2π: la misma red con la fuente a f~ */
        sr.el[kf].p[1] = wt / (2.0 * PI_T);
        if (ca_sr(0.0) != SIM_OK) { sr.el[kf].p[1] = f; continue; }
        a_t = car;
        sr.el[kf].p[1] = f;
        /* escala: la de la red (el mayor fasor de V, y de I por 1e3) */
        amp = 0.0;
        for (i = 0; i < sr.nel; i++) {
            if (cx_abs(a_t.V[i]) > amp) amp = cx_abs(a_t.V[i]);
            if (1e3 * cx_abs(a_t.I[i]) > amp) amp = 1e3 * cx_abs(a_t.I[i]);
        }
        esc = amp;
        n_ok++;
        e0 = err_ultimo_periodo(0, a_t.V[kc0], w, sim_res()->t_fin, T, &b0);
        e1 = err_ultimo_periodo(1, a_t.I[kc1], w, sim_res()->t_fin, T, &b1);
        {
            double em = (e0 + e1 * 1e3) / esc, eb = (b0 + b1 * 1e3) / esc;
            if (em > peor_m) peor_m = em;
            if (!(em <= 1e-9)) malos_m++;
            if (eb > peor_b) peor_b = eb;
            if (!(eb <= 1e-4)) malos_b++;
        }
        {
            /* las lecturas: V y A en CA y la P del W, las del análisis a ω~ */
            const SimRes *rs = sim_res();
            double el = fabs(rs->lectura[kv] - a_t.lectura[kv]) / esc;
            double ea = 1e3 * fabs(rs->lectura[ka] - a_t.lectura[ka]) / esc;
            if (ea > el) el = ea;
            if (kw >= 0) {
                double ew = fabs(rs->lectura[kw] - a_t.lectura[kw]) / (1e-3 * esc * esc);
                if (ew > el) el = ew;
            }
            if (el > peor_l) peor_l = el;
            if (!(el <= 1e-9)) malos_l++;
        }
        /* con la ω de verdad: error de orden 2 en h */
        if (n_ord < 60 && fabs(hr - h) <= 1e-12 * h) {
            double err100, err200, rz;
            if (ca_sr(0.0) != SIM_OK) continue;
            a_w = car;
            err100 = (err_ultimo_periodo(0, a_w.V[kc0], w, sim_res()->t_fin, T, 0) +
                      1e3 * err_ultimo_periodo(1, a_w.I[kc1], w, sim_res()->t_fin, T, 0)) / esc;
            saj.h = h / 2.0;
            if (sim_emular(&sr, &saj) != SIM_OK || fabs(sim_res()->h - h / 2.0) > 1e-12 * h) continue;
            err200 = (err_ultimo_periodo(0, a_w.V[kc0], w, sim_res()->t_fin, T, 0) +
                      1e3 * err_ultimo_periodo(1, a_w.I[kc1], w, sim_res()->t_fin, T, 0)) / esc;
            if (err100 < 1e-7) continue;                   /* el canal apenas depende de ω */
            n_ord++;
            if (err100 > peor_abs) peor_abs = err100;
            if (!(err100 <= 5e-3)) malos_abs++;
            rz = err100 / (err200 + 1e-300);
            if (rz < rmin) rmin = rz;
            if (rz > rmax) rmax = rz;
            if (!(rz >= 3.6 && rz <= 4.4)) malos_ord++;
        }
    }
    printf("    %d redes: muestras %.1e (en bruto %.1e: el modo (-1)^n), lecturas %.1e\n", n_ok, peor_m, peor_b, peor_l);
    printf("    con " S_OMEGA " de verdad (%d): error %.1e, cociente h/(h/2) de %.2f a %.2f\n", n_ord, peor_abs, rmin, rmax);
    chk_cond("fasor = trapecio", "las muestras del último periodo, a 1e-9 (por pares)", malos_m == 0);
    chk_cond("fasor = trapecio", "el modo (-1)^n de los algebraicos, a 1e-4", malos_b == 0);
    chk_cond("fasor = trapecio", "las lecturas de V, A y W, a 1e-9", malos_l == 0);
    chk_cond("fasor = trapecio", "al menos 150 redes", n_ok >= 150);
    chk_cond("fasor = trapecio", "con " S_OMEGA ": error(T/100) <= 5e-3", malos_abs == 0 && n_ord >= 30);
    chk_cond("fasor = trapecio", "con " S_OMEGA ": orden 2 (cociente 3,6 a 4,4)", malos_ord == 0);
}

/* --------------------------------------------------------------------------
 *  48. Etapa 3: el trazador de Bode
 * ----------------------------------------------------------------------- */
/* Barre la red sr de la entrada ent a la salida (canal c de saj, o V(na) -
 * V(nb) si c < 0), todo de una vez */
static int bode_sr(int ent, int c, int na, int nb, double f_ini, double f_fin, double frec)
{
    int e = bode_iniciar_red(&sr, ent, c >= 0 ? &saj.canal[c] : 0, na, nb, f_ini, f_fin, frec);
    if (e != SIM_OK) return e;
    while (bode_avanzar(~0ul)) {}
    return bode_res()->d.estado;
}

static void chk_rango(const char *q, double a, double b)
{
    chk_cond(q, "el rango automático", fabs(bode_res()->f_ini - a) <= 1e-12 * a && fabs(bode_res()->f_fin - b) <= 1e-12 * b);
}

static double db_en(double f) { Cx H; bode_en(f, &H); return 20.0 * log10(cx_abs(H)); }
static double fase_en(double f) { Cx H; bode_en(f, &H); return cx_arg(H) * 180.0 / PI_T; }

static BodeReg breg1;
static BodeMedidas bmed1;

static void test_bode(void)
{
    const BodeMedidas *m = &bode_res()->m;
    int kE, kR, kL, kC, kJ, i, iguales;
    double f0;

    printf("[48] Etapa 3: el trazador de Bode\n");

    /* ---- RC paso bajo: 1 k, 1 uF, salida V(C1) ---- */
    sr_init(3);
    kE = sr_el(SE_E, 0, 1, 0.0); sr.el[kE].forma = ONDA_CC; sr.el[kE].p[0] = 1.0;
    sr_el(SE_R, 1, 2, 1e3);
    kC = sr_el(SE_C, 2, 0, 1e-6);
    canal(0, CANAL_V, kC);
    chk_cond("Bode RC", "se barre", bode_sr(kE, 0, -1, -1, 0.0, 0.0, 0.0) == SIM_OK && bode_res()->fin == FIN_COMPLETO);
    chk_rango("Bode RC", 10.0, 1e4);
    chk_cond("Bode RC", "pasa-bajas", m->tipo == BODE_PB);
    chk_t("Bode RC", "fc = 159,155 Hz", m->fc, 159.15494309189535, 159.2, 1e-9);
    chk_t("Bode RC", "en fc, -3,0103 dB", db_en(m->fc), -3.0102999566398116, 3.0, 1e-6);
    chk_t("Bode RC", "en fc, -45" S_DEG, fase_en(m->fc), -45.0, 45.0, 1e-6);
    chk_t("Bode RC", "en 10 fc, -20,0432 dB", db_en(10.0 * 159.15494309189535), -20.043213737826427, 20.0, 1e-9);
    chk_t("Bode RC", "en 10 fc, -84,289" S_DEG, fase_en(10.0 * 159.15494309189535), -84.28940686250037, 84.3, 1e-9);
    chk_t("Bode RC", "pendiente final -20 dB/década", m->pend_fin, -20.0, 20.0, 5e-3);
    chk_cond("Bode RC", "la unidad es V/V, sin nombre", bode_res()->unidad[0] == 0);
    chk_cond("Bode RC", "277 puntos", bode_reg()->n == BODE_N);
    {
        Cx H;
        const BodeReg *g = bode_reg();
        chk_cond("Bode RC", "bode_en en un punto de la malla: igual bit a bit", bode_en(g->f[100], &H) && memcmp(&H, &g->H[100], sizeof H) == 0);
    }

    /* ---- Por trozos: con 1, 997 e ilimitadas operaciones, lo mismo ---- */
    breg1 = *bode_reg(); bmed1 = bode_res()->m;
    {
        static const unsigned long OPS[2] = { 1ul, 997ul };
        int t;
        for (t = 0; t < 2; t++) {
            bode_iniciar_red(&sr, kE, &saj.canal[0], -1, -1, 0.0, 0.0, 0.0);
            while (bode_avanzar(OPS[t])) {}
            iguales = memcmp(&breg1, bode_reg(), sizeof breg1) == 0 && memcmp(&bmed1, &bode_res()->m, sizeof bmed1) == 0;
            chk_cond("Bode por trozos", t ? "997 operaciones por trozo: igual" : "1 operación por trozo: igual", iguales);
        }
    }
    /* cortado a medias: lo barrido vale */
    bode_iniciar_red(&sr, kE, &saj.canal[0], -1, -1, 0.0, 0.0, 0.0);
    for (i = 0; i < 80; i++) bode_avanzar(1ul);
    bode_cortar();
    chk_cond("Bode cortado", "FIN_CORTADO con parte de la malla", bode_res()->fin == FIN_CORTADO && bode_reg()->n > 2 && bode_reg()->n < BODE_N);

    /* ---- RL paso alto: 100, 10 mH, salida V(L1) ---- */
    sr_init(3);
    kE = sr_el(SE_E, 0, 1, 0.0); sr.el[kE].forma = ONDA_CC; sr.el[kE].p[0] = 1.0;
    sr_el(SE_R, 1, 2, 100.0);
    kL = sr_el(SE_L, 2, 0, 10e-3);
    canal(0, CANAL_V, kL);
    chk_cond("Bode RL", "se barre", bode_sr(kE, 0, -1, -1, 0.0, 0.0, 0.0) == SIM_OK);
    chk_rango("Bode RL", 100.0, 1e5);
    chk_cond("Bode RL", "pasa-altas", m->tipo == BODE_PA);
    chk_t("Bode RL", "fc = 1591,55 Hz", m->fc, 1591.5494309189532, 1591.5, 1e-9);
    chk_t("Bode RL", "+45" S_DEG " en fc", fase_en(m->fc), 45.0, 45.0, 1e-6);

    /* ---- RLC pasa-banda: 10, 10 mH, 1 uF, salida V(R1) ---- */
    sr_init(4);
    kE = sr_el(SE_E, 0, 1, 0.0); sr.el[kE].forma = ONDA_CC; sr.el[kE].p[0] = 1.0;
    sr_el(SE_L, 1, 2, 10e-3);
    sr_el(SE_C, 2, 3, 1e-6);
    kR = sr_el(SE_R, 3, 0, 10.0);
    canal(0, CANAL_V, kR);
    chk_cond("Bode RLC", "se barre", bode_sr(kE, 0, -1, -1, 0.0, 0.0, 0.0) == SIM_OK);
    chk_rango("Bode RLC", 100.0, 1e4);
    chk_cond("Bode RLC", "pasa-banda", m->tipo == BODE_PBANDA);
    chk_t("Bode RLC", "f1 = 1513,96", m->f1, 1513.9601543154847, 1514.0, 1e-9);
    chk_t("Bode RLC", "f2 = 1673,12", m->f2, 1673.1150974073803, 1673.1, 1e-9);
    chk_t("Bode RLC", "BW = 159,155", m->bw, 159.15494309189535, 159.2, 1e-9);
    chk_t("Bode RLC", "fase 0 en f0", m->f_fase0, 1591.5494309189535, 1591.5, 1e-9);
    chk_t("Bode RLC", "el pico en f0", m->f_pico, 1591.5494309189535, 1591.5, 1e-7);
    chk_t("Bode RLC", "Q = 10", m->q, 10.0, 10.0, 1e-7);
    chk_t("Bode RLC", "pico de 0 dB", m->db_pico, 0.0, 1.0, 1e-9);

    /* ---- RLC paso bajo con pico: R = 20, salida V(C1) ---- */
    sr.el[kR].val = 20.0;
    kC = 2;
    canal(0, CANAL_V, kC);
    sr.el[kR].a = 1; sr.el[kR].b = 2;                      /* E - R - L - C - tierra */
    sr.el[1].a = 2; sr.el[1].b = 3;
    sr.el[2].a = 3; sr.el[2].b = 0;
    chk_cond("Bode RLC con pico", "se barre", bode_sr(kE, 0, -1, -1, 0.0, 0.0, 0.0) == SIM_OK);
    chk_rango("Bode RLC con pico", 100.0, 1e4);
    chk_cond("Bode RLC con pico", "pasa-bajas", m->tipo == BODE_PB);
    chk_t("Bode RLC con pico", "fc = 2455,40", m->fc, 2455.396560563857, 2455.4, 1e-9);
    chk_t("Bode RLC con pico", "pico en 1575,55 Hz", m->f_pico, 1575.5535532749356, 1575.6, 1e-7);
    chk_t("Bode RLC con pico", "de 14,023 dB", m->db_pico, 14.023048140744877, 14.0, 1e-9);
    f0 = 1591.5494309189535;
    chk_t("Bode RLC con pico", "en f0, +13,979 dB", db_en(f0), 13.979400086720377, 14.0, 1e-9);
    chk_t("Bode RLC con pico", "en f0, -90" S_DEG, fase_en(f0), -90.0, 90.0, 1e-9);
    chk_t("Bode RLC con pico", "en 100 f0, -179,885" S_DEG, fase_en(100.0 * f0), -179.88539713350613, 180.0, 1e-9);
    {
        const BodeReg *g = bode_reg();
        int salta = 0;
        for (i = 1; i < g->n; i++) if (fabs(g->fase[i] - g->fase[i - 1]) > 90.0) salta = 1;
        chk_cond("Bode RLC con pico", "la fase desenrollada no salta", !salta && g->fase[g->n - 1] < -170.0);
    }

    /* ---- Tanque: J 1" S_ANG "0 A con 10 k || 10 mH || 1 uF (V/J, ohm) ---- */
    sr_init(2);
    kJ = sr_el(SE_J, 0, 1, 0.0); sr.el[kJ].forma = ONDA_CC; sr.el[kJ].p[0] = 1.0;
    sr_el(SE_R, 1, 0, 1e4);
    sr_el(SE_L, 1, 0, 10e-3);
    sr_el(SE_C, 1, 0, 1e-6);
    canal(0, CANAL_NODO, 1);
    chk_cond("Bode tanque", "se barre", bode_sr(kJ, 0, -1, -1, 0.0, 0.0, 0.0) == SIM_OK);
    chk_rango("Bode tanque", 100.0, 1e4);
    chk_cond("Bode tanque", "pasa-banda en " S_OHM, m->tipo == BODE_PBANDA && strcmp(bode_res()->unidad, S_OHM) == 0);
    chk_t("Bode tanque", "pico de 80 dB" S_OHM, m->db_pico, 80.0, 80.0, 1e-9);
    chk_t("Bode tanque", "f1 = 1583,61", m->f1, 1583.6115780079072, 1583.6, 1e-9);
    chk_t("Bode tanque", "f2 = 1599,53", m->f2, 1599.5270723170966, 1599.5, 1e-9);
    chk_t("Bode tanque", "BW = 15,915", m->bw, 15.915494309189535, 15.9, 1e-9);
    chk_t("Bode tanque", "Q = 100", m->q, 100.0, 100.0, 1e-7);
    chk_t("Bode tanque", "fase 0 en f0", m->f_fase0, 1591.5494309189535, 1591.5, 1e-9);

    /* ---- Tanque con la r de la bobina: 5 + 10 mH en paralelo con 1 uF ---- */
    sr_init(3);
    kJ = sr_el(SE_J, 0, 1, 0.0); sr.el[kJ].forma = ONDA_CC; sr.el[kJ].p[0] = 1.0;
    sr_el(SE_R, 1, 2, 5.0);
    sr_el(SE_L, 2, 0, 10e-3);
    sr_el(SE_C, 1, 0, 1e-6);
    canal(0, CANAL_NODO, 1);
    chk_cond("Bode tanque rL", "se barre", bode_sr(kJ, 0, -1, -1, 0.0, 0.0, 0.0) == SIM_OK);
    chk_rango("Bode tanque rL", 1.0, 1e4);
    chk_cond("Bode tanque rL", "pasa-banda", m->tipo == BODE_PBANDA);
    chk_t("Bode tanque rL", "fase 0 en 1589,56", m->f_fase0, 1589.5587491756316, 1589.6, 1e-9);
    chk_t("Bode tanque rL", "pico en 1591,547", m->f_pico, 1591.546950318664, 1591.5, 1e-7);
    chk_t("Bode tanque rL", "|Z| = 2002,50", pow(10.0, m->db_pico / 20.0), 2002.4984433417379, 2002.5, 1e-9);
    chk_t("Bode tanque rL", "f1 = 1552,25", m->f1, 1552.2530168417356, 1552.3, 1e-9);
    chk_t("Bode tanque rL", "f2 = 1631,83", m->f2, 1631.8304886969872, 1631.8, 1e-9);
    chk_t("Bode tanque rL", "BW = 79,577", m->bw, 79.577471855251617, 79.6, 1e-9);
    chk_t("Bode tanque rL", "Q = 19,99997", m->q, 19.99996875012, 20.0, 1e-7);

    /* ---- Rechaza-banda: E - 100 - A - 10 mH - 1 uF - tierra, sonda en A ---- */
    sr_init(4);
    kE = sr_el(SE_E, 0, 1, 0.0); sr.el[kE].forma = ONDA_CC; sr.el[kE].p[0] = 1.0;
    sr_el(SE_R, 1, 2, 100.0);
    sr_el(SE_L, 2, 3, 10e-3);
    sr_el(SE_C, 3, 0, 1e-6);
    canal(0, CANAL_NODO, 2);
    chk_cond("Bode rechaza-banda", "se barre", bode_sr(kE, 0, -1, -1, 0.0, 0.0, 0.0) == SIM_OK);
    chk_rango("Bode rechaza-banda", 100.0, 1e5);
    chk_cond("Bode rechaza-banda", "rechaza-banda", m->tipo == BODE_RB);
    chk_t("Bode rechaza-banda", "el valle en f0", m->f_valle, 1591.5494309189535, 1591.5, 1e-7);
    chk_t("Bode rechaza-banda", "f1 = 983,63", m->f1, 983.6316430834661, 983.6, 1e-9);
    chk_t("Bode rechaza-banda", "f2 = 2575,18", m->f2, 2575.1810740024193, 2575.2, 1e-9);
    chk_t("Bode rechaza-banda", "BW = 1591,55", m->bw, 1591.5494309189535, 1591.5, 1e-9);
    chk_t("Bode rechaza-banda", "Q = 1", m->q, 1.0, 1.0, 1e-7);

    /* ---- Resistivo con frec = 60: plano de 0,6 a 6000 Hz ---- */
    sr_init(3);
    kE = sr_el(SE_E, 0, 1, 0.0); sr.el[kE].forma = ONDA_CC; sr.el[kE].p[0] = 1.0;
    sr_el(SE_R, 1, 2, 1e3);
    kR = sr_el(SE_R, 2, 0, 1e3);
    canal(0, CANAL_V, kR);
    chk_cond("Bode resistivo", "se barre", bode_sr(kE, 0, -1, -1, 0.0, 0.0, 60.0) == SIM_OK);
    chk_rango("Bode resistivo", 0.6, 6000.0);
    chk_cond("Bode resistivo", "plano", m->tipo == BODE_PLANO);
    /* la salida V(AB) y otra entrada: aviso OTRAS */
    kJ = sr_el(SE_J, 2, 0, 0.0); sr.el[kJ].forma = ONDA_CC; sr.el[kJ].p[0] = 1e-3;
    chk_cond("Bode resistivo", "con V(AB) y dos fuentes: OTRAS", bode_sr(kE, -1, 2, 0, 0.0, 0.0, 60.0) == SIM_OK &&
             strcmp(bode_res()->nom_sal, "V(AB)") == 0 && (bode_res()->d.avisos & SIM_AV_OTRAS));
    chk_t("Bode resistivo", "V(AB)/E1 = 1/2", cx_abs(bode_reg()->H[10]), 0.5, 0.5, 1e-12);
    chk_cond("Bode resistivo", "desde la J: V(AB)/J en " S_OHM, bode_sr(kJ, -1, 2, 0, 0.0, 0.0, 60.0) == SIM_OK &&
             strcmp(bode_res()->unidad, S_OHM) == 0 && fabs(cx_abs(bode_reg()->H[10]) - 500.0) < 1e-9);

    /* ---- LC sin R con un punto justo en f0: ok = 0 ahí y nada de NaN ---- */
    sr_init(3);
    kE = sr_el(SE_E, 0, 1, 0.0); sr.el[kE].forma = ONDA_CC; sr.el[kE].p[0] = 1.0;
    sr_el(SE_L, 1, 2, 10e-3);
    kC = sr_el(SE_C, 2, 0, 1e-6);
    canal(0, CANAL_V, kC);
    f0 = 1.0 / (2.0 * PI_T * sqrt(10e-3 * 1e-6));
    chk_cond("Bode LC sin R", "se barre", bode_sr(kE, 0, -1, -1, f0 / 100.0, f0 * 100.0, 0.0) == SIM_OK);
    {
        const BodeReg *g = bode_reg();
        const BodeMedidas *mm = &bode_res()->m;
        int k0 = (g->n - 1) / 2, nan = 0;
        const double *v = &mm->M0;
        chk_cond("Bode LC sin R", "el punto de f0: ok = 0", g->ok[k0] == 0 && fabs(g->f[k0] - f0) < 1e-9 * f0);
        for (i = 0; i < (int)(sizeof *mm / sizeof(double)) - 1; i++) if (v[i] != v[i]) nan = 1;
        chk_cond("Bode LC sin R", "ninguna medida es NaN", !nan);
    }

    /* ---- Sin fuente de entrada ---- */
    sr_init(2);
    sr_el(SE_R, 1, 0, 10.0);
    chk_cond("Bode", "sin E ni J: SIN_FUENTE", bode_iniciar_red(&sr, 0, 0, 1, 0, 0.0, 0.0, 0.0) == SIM_SIN_FUENTE);

    /* ---- Tope: con 32 elementos, t_est <= 4 s (y lo gastado <= 3,5) ---- */
    {
        int n;
        sr_init(20);
        kE = sr_el(SE_E, 0, 1, 0.0); sr.el[kE].forma = ONDA_CC; sr.el[kE].p[0] = 1.0;
        for (n = 1; n < 20; n++) sr_el(n % 3 == 0 ? SE_C : n % 3 == 1 ? SE_R : SE_L, n - 1, n, n % 3 == 0 ? 1e-6 : n % 3 == 1 ? 100.0 : 1e-3);
        for (n = 0; sr.nel < MAX_EL; n++) sr_el(n % 2 ? SE_C : SE_R, n % 20, (n * 7 + 3) % 20 == n % 20 ? 0 : (n * 7 + 3) % 20, n % 2 ? 1e-7 : 1e3);
        canal(0, CANAL_NODO, 10);
        /* Con la CA medida en la calculadora sin el USB (CA_NS_OP, 1,23 µs por
         * operación; con el USB 1,69; antes 0,63 y 40 µs por evaluación) una
         * evaluación de esta red cuesta ~12 ms: la malla baja a 139 puntos
         * (con el USB, ~16 ms y 70) y t_est, que cuenta 120 evaluaciones de las
         * medidas por si acaso, pasa de 3,5 s. Lo que gasta de verdad (sus
         * operaciones de CA) sí cabe en 3,5 s */
        {
            unsigned long c0 = ca_ops();
            double e;
            bode_iniciar_red(&sr, kE, &saj.canal[0], -1, -1, 0.0, 0.0, 0.0);
            while (bode_avanzar(~0ul)) {}
            e = (double)(ca_ops() - c0) * CA_NS_OP * 1e-9;
            printf("    peor caso: %d puntos, %d evaluaciones, t_est = %.2f s, por sus operaciones %.2f s\n",
                   bode_reg()->n > 0 ? bode_reg()->n : 0, bode_res()->evaluaciones, bode_res()->t_est, e);
            chk_cond("Bode peor caso", "t_est <= 4 s, y por sus operaciones <= 3,5 s", bode_res()->t_est <= 4.0 && e <= 3.5);
        }
    }
}

/* --------------------------------------------------------------------------
 *  52. Etapa 3: la interfaz de la alterna (por guion de teclas)
 * ----------------------------------------------------------------------- */
static int el_de_etiqueta(const Circuito *c, const char *etiq);

/* T1 por teclas: E alterna 120 V hacia abajo, cable, C = 100 uF hacia
 * arriba, L = 50 mH y R = 10 de vuelta */
static void dibuja_t1(void)
{
    ui_init();
    TECLAS(T_ENTER, T_ENTER, T_ENTER);                          /* editor nuevo */
    TECLAS(T_LETRA('E'), T_ABAJO, T_IZQ, T_DIG(1), T_DIG(2), T_DIG(0), T_ENTER);
    TECLAS(T_LETRA('W'), T_DER, T_DER, T_ESC);
    TECLAS(T_LETRA('C'), T_ARRIBA, T_DIG(1), T_DIG(0), T_DIG(0), T_LETRA('U'), T_ENTER);
    TECLAS(T_LETRA('L'), T_IZQ, T_DIG(5), T_DIG(0), T_LETRA('M'), T_ENTER);
    TECLAS(T_LETRA('R'), T_IZQ, T_DIG(1), T_DIG(0), T_ENTER);
}

static void test_ui_ca(void)
{
    const Circuito *cu = ui_circuito();
    int k;

    printf("[52] Etapa 3: la interfaz de la alterna\n");
    gfx_desbordes_reset();

    /* ---- La ficha: [izq] en una E continua la pasa a alterna ---- */
    ui_init();
    TECLAS(T_ENTER, T_ENTER, T_ENTER);
    TECLAS(T_LETRA('E'), T_ABAJO, T_DIG(1), T_DIG(2), T_DIG(0), T_ENTER);
    TECLAS(T_ENTER);                                            /* vuelve a la ficha de E1 */
    k = 0;
    cu = ui_circuito();
    TECLAS(T_IZQ);
    vuelca("ca_ficha_e");
    chk_cond("ficha CA", "[izq] desde continua: alterna (fasor)", ui_pantalla() == P_ELEMENTO && ve("alterna (fasor)") &&
             cu->el[k].variante == FORMA_FASOR);
    chk_cond("ficha CA", "f del circuito = 60 Hz y el ángulo 0 (dato)", cu->frec.est == M_DATO && cu->frec.v == 60.0 &&
             cu->el[k].val.est_ang == M_DATO && cu->el[k].val.ang == 0.0 && cu->el[k].val.v == 120.0);
    TECLAS(T_DIG(1), T_DIG(2), T_DIG(0), T_TAB, T_DIG(3), T_DIG(0), T_ENTER);
    chk_cond("ficha CA", "120 [tab] 30: 120" S_ANG "30 guardado", cu->el[k].val.v == 120.0 && cu->el[k].val.ang == 30.0);
    TECLAS(T_ENTER, T_TAB, T_DIG(4), T_DIG(0), T_DIG(0), T_ENTER);
    chk_cond("ficha CA", "el ángulo 400: fuera de rango", ve("va de -360 a 360"));
    TECLAS(T_BORRAR, T_BORRAR, T_BORRAR, T_SIGNO, T_DIG(1), T_DIG(9), T_DIG(0), T_ENTER);
    chk_cond("ficha CA", "(-)190 se guarda como 170", fabs(cu->el[k].val.ang - 170.0) < 1e-12);
    TECLAS(T_ENTER, T_ABAJO);
    chk_cond("ficha CA", "la fila de la f", ui_pantalla() == P_ELEMENTO && ve("Frecuencia (la de todo el circuito)"));
    TECLAS(T_TAB);
    chk_cond("ficha CA", "[tab] en la f: no tiene ángulo", ve("Este valor no tiene"));
    TECLAS(T_ESC);
    /* la Z: 30 [+] 40 */
    TECLAS(T_LETRA('W'), T_DER, T_ESC, T_LETRA('Z'), T_ARRIBA);
    chk_cond("ficha CA", "[Z] y una flecha ponen Z1", ui_pantalla() == P_ELEMENTO && ve("Impedancia"));
    k = cu->nel - 1;
    TECLAS(T_DIG(3), T_DIG(0), T_MAS, T_DIG(4), T_DIG(0));
    vuelca("ca_ficha_z");
    chk_cond("ficha CA", "el eco de 30+j40", ve("= 30+j40 = 50,00" S_ANG "53,1" S_DEG " " S_OHM));
    TECLAS(T_ENTER);
    chk_cond("ficha CA", "30 [+] 40: 50" S_ANG "53,13", fabs(cu->el[k].val.v - 50.0) < 1e-12 &&
             fabs(cu->el[k].val.ang - 53.13010235415598) < 1e-12);
    TECLAS(T_ENTER, T_TAB, T_DIG(9), T_DIG(5), T_ENTER);
    chk_cond("ficha CA", "el ángulo 95 en la Z: fuera de rango", ve("va de -90 a 90"));
    TECLAS(T_ESC, T_ESC);

    /* ---- [=] y P_CA con el T1 ---- */
    dibuja_t1();
    chk_cond("P_CA", "el T1 dibujado", cu->nel == 4 && circ_alterna(cu));
    chk_cond("P_CA", "estado: listo para la CA a 60 Hz", ve("Listo: [=] analiza en CA a 60 Hz"));
    TECLAS(T_IGUAL);
    vuelca("ca_resultado");
    chk_cond("P_CA", "[=] con la alterna: P_CA", ui_pantalla() == P_CA && ui_ca()->d.estado == SIM_OK);
    chk_cond("P_CA", "I = 9,519" S_ANG "37,5" S_DEG " A", ve("9,519" S_ANG "37,5" S_DEG " A"));
    chk_cond("P_CA", "las potencias", ve("P = 906,1 W  Q = -695,5 var  S = 1142 VA"));
    TECLAS(T_PUNTO);
    chk_cond("P_CA", "[.]: 7,55+j5,80 A", ve("7,55+j5,80 A"));
    TECLAS(T_PUNTO, T_DER, T_DER);
    vuelca("ca_resultado_tensiones");
    chk_cond("P_CA", "capa de tensiones: 179" S_ANG "128" S_DEG "V", ve("179" S_ANG "128" S_DEG "V"));
    TECLAS(T_TAB);
    vuelca("ca_tabla_1");
    chk_cond("P_CA", "[tab]: la tabla", ui_pantalla() == P_CA_TABLA);
    TECLAS(T_DER);
    vuelca("ca_tabla_2");
    TECLAS(T_DER);
    vuelca("ca_tabla_3");
    chk_cond("P_CA tabla", "página 3: 906,1 W y Tellegen", ve("906,1 W") && ve("Tellegen"));
    TECLAS(T_DER);
    vuelca("ca_tabla_4");
    TECLAS(T_ESC, T_MENU);
    vuelca("ca_ajustes");
    chk_cond("P_CA", "[menu]: los ajustes de CA", ui_pantalla() == P_AJUSTES && ve("Ajustes de CA"));
    TECLAS(T_ABAJO, T_DER);
    chk_cond("ajustes CA", "Fasores de pico", ve("de pico"));
    TECLAS(T_IGUAL);
    chk_cond("ajustes CA", "[=]: P_CA en pico, |I| = 13,46", ui_pantalla() == P_CA && ve("13,46"));
    TECLAS(T_MENU, T_ABAJO, T_IZQ, T_ENTER, T_ESC);

    /* ---- El Bode por teclas: el RC paso bajo ---- */
    dibuja_rc();
    TECLAS(T_LETRA('B'));
    for (k = 0; k < 400 && ui_pantalla() == P_EMULANDO; k++) ui_tick();
    vuelca("ca_bode_rc");
    chk_cond("Bode", "[B] en el editor: P_BODE con el pasa-bajas", ui_pantalla() == P_BODE && ve("Pasa-bajas"));
    TECLAS(T_DIG(1), T_LETRA('K'), T_ENTER);
    chk_cond("Bode", "1 [K] [enter]: el cursor en 1 kHz", fabs(ui_trazador()->fcur[ui_trazador()->activo] - 1e3) < 1e-6 * 1e3 ||
             fabs(log10(ui_trazador()->fcur[ui_trazador()->activo] / 1e3)) < 3.0 / 276.0);
    TECLAS(T_TAB);
    vuelca("ca_bode_fasores");
    chk_cond("Bode", "[tab]: los fasores a la f del cursor", ui_pantalla() == P_CA && fabs(ui_ca()->f - ui_trazador()->fcur[ui_trazador()->activo]) < 1e-9 * 1e3);
    TECLAS(T_ESC);
    chk_cond("Bode", "[esc] vuelve al Bode", ui_pantalla() == P_BODE);
    TECLAS(T_MENU);
    chk_cond("Bode", "[menu]: el rango del barrido", ui_pantalla() == P_AJUSTES && ve("Bode: rango de frecuencias"));
    TECLAS(T_ESC, T_ESC);
    chk_cond("Bode", "[esc] al editor", ui_pantalla() == P_EDITOR);
    TECLAS(T_MENU);
    for (k = 0; k < 7; k++) TECLAS(T_ABAJO);
    TECLAS(T_ENTER);
    for (k = 0; k < 400 && ui_pantalla() == P_EMULANDO; k++) ui_tick();
    chk_cond("Bode", "la opción 7 del menú también barre", ui_pantalla() == P_BODE);
    TECLAS(T_ESC, T_MENU, T_ARRIBA, T_ENTER);          /* [arr] da la vuelta a la última (con 11 opciones, Etapa 4) */
    chk_cond("menú", "y la última sigue siendo Salir", ui_pantalla() == P_MENU);

    /* ---- Thévenin desde el resultado de CC ---- */
    ui_init();
    TECLAS(T_ENTER, T_ENTER, T_ENTER);
    TECLAS(T_LETRA('E'), T_ABAJO, T_DIG(1), T_DIG(2), T_ENTER);
    TECLAS(T_LETRA('W'), T_DER, T_ESC);
    TECLAS(T_LETRA('R'), T_ARRIBA, T_DIG(1), T_DIG(2), T_ENTER);
    TECLAS(T_LETRA('R'), T_IZQ, T_DIG(4), T_ENTER);
    TECLAS(T_DER, T_LETRA('T'), T_ABAJO, T_LETRA('T'));        /* A arriba y B abajo del R de 12 */
    TECLAS(T_IGUAL);
    vuelca("cc_resultado_thevenin");
    chk_cond("Thévenin CC", "el resultado lleva [T] Thévenin", ui_pantalla() == P_RESULTADO && ve("[T] Th" S_ee "venin"));
    TECLAS(T_LETRA('T'));
    vuelca("cc_thevenin");
    chk_cond("Thévenin CC", "[T]: P_THEVENIN con PL = 6,75 W", ui_pantalla() == P_THEVENIN && ve("PL = 6,75 W"));
    TECLAS(T_ESC, T_ESC);
    chk_cond("interfaz CA", "sin textos fuera de la pantalla", gfx_desbordes() == 0);
}

/* El primer problema de la calculadora de CA por teclas: E = 120 V a 60 Hz,
 * R = 30 hacia arriba, L [?!] y V1 sobre R que marca 72 */
static void dibuja_l_voltimetro(void)
{
    ui_init();
    TECLAS(T_ENTER, T_ENTER, T_ENTER);
    TECLAS(T_LETRA('E'), T_ABAJO, T_IZQ, T_DIG(1), T_DIG(2), T_DIG(0), T_ENTER);
    TECLAS(T_LETRA('W'), T_DER, T_DER, T_ESC);
    TECLAS(T_LETRA('R'), T_ARRIBA, T_DIG(3), T_DIG(0), T_ENTER);
    TECLAS(T_LETRA('L'), T_IZQ, T_INCOG, T_ENTER);
    TECLAS(T_LETRA('W'), T_IZQ, T_ESC);
    TECLAS(T_DER, T_DER);                                       /* arriba de R1 */
    TECLAS(T_LETRA('W'), T_DER, T_ESC);
    TECLAS(T_LETRA('V'), T_ABAJO, T_DIG(7), T_DIG(2), T_ENTER);
    TECLAS(T_LETRA('W'), T_IZQ, T_ESC);
}

/* Abre la ficha de un elemento por teclas: [tab] hasta él y [enter] (con
 * una flecha si junto al cursor hay más de uno) */
static int abre_ficha_de(const char *etiq)
{
    const Circuito *c = ui_circuito();
    int k = el_de_etiqueta(c, etiq), j;
    const Elemento *e;
    if (k < 0) return 0;
    if (ui_pantalla() == P_ELEMENTO) TECLAS(T_ESC);
    TECLAS(T_TAB, T_ESC);                                       /* sin elegido */
    for (j = 0; j <= k; j++) TECLAS(T_TAB);
    e = &c->el[k];
    TECLAS(T_ENTER);
    if (ui_pantalla() != P_ELEMENTO)
        repite(e->xb > e->xa ? T_DER : e->xb < e->xa ? T_IZQ : e->yb > e->ya ? T_ABAJO : T_ARRIBA, 1);
    return ui_pantalla() == P_ELEMENTO;
}

static void test_ui_ca2(void)
{
    const Circuito *cu = ui_circuito();
    int k, j;

    printf("[52b] Etapa 3: la interfaz de la alterna (2)\n");
    gfx_desbordes_reset();

    /* ---- La ficha de la Z: [J] y [-] ---- */
    ui_init();
    TECLAS(T_ENTER, T_ENTER, T_ENTER);
    TECLAS(T_LETRA('Z'), T_ARRIBA, T_LETRA('J'), T_DIG(4), T_DIG(0), T_ENTER);
    k = cu->nel - 1;
    chk_cond("ficha Z", "[J] 4 0: 40" S_ANG "90", k >= 0 && cu->el[k].tipo == EL_IMP && fabs(cu->el[k].val.v - 40.0) < 1e-12 &&
             fabs(cu->el[k].val.ang - 90.0) < 1e-12);
    TECLAS(T_ENTER, T_DIG(3), T_DIG(0), T_MENOS, T_DIG(4), T_DIG(0), T_ENTER);
    chk_cond("ficha Z", "3 0 [-] 4 0: 50" S_ANG "-53,13", fabs(cu->el[k].val.v - 50.0) < 1e-12 &&
             fabs(cu->el[k].val.ang + 53.13010235415598) < 1e-12);
    /* [ctrl][Z] deshace (antes que la tecla de la Z) */
    TECLAS(T_LETRA('Z') | T_CON_CTRL);
    chk_cond("editor", "[ctrl][Z] deshace, no pone otra Z", ui_pantalla() == P_EDITOR && cu->nel == 1 &&
             fabs(cu->el[0].val.v - 40.0) < 1e-12);

    /* ---- La ficha con el T1: [?!] en V, en C y el cambio de forma ---- */
    dibuja_t1();
    k = el_de_etiqueta(cu, "R1");
    chk_cond("ficha CA", "el T1 tiene R1", k >= 0);
    chk_cond("ficha CA", "la ficha de R1", abre_ficha_de("R1"));
    TECLAS(T_ABAJO, T_INCOG);
    chk_cond("ficha CA", "[?!] en V: buscas módulo y ángulo", k >= 0 && cu->el[k].V.est == M_BUSCAR && cu->el[k].V.est_ang == M_BUSCAR);
    chk_cond("ficha CA", "«lo que buscas»", ve("lo que buscas"));
    TECLAS(T_TAB, T_BORRAR);
    vuelca("ca_ficha_buscas_modulo");
    chk_cond("ficha CA", "[tab][del]: buscas el módulo", k >= 0 && cu->el[k].V.est == M_BUSCAR && cu->el[k].V.est_ang == M_LIBRE &&
             ve("buscas el m" S_oo "dulo"));
    TECLAS(T_TAB, T_BORRAR, T_ESC);
    chk_cond("ficha CA", "[del] lo deja vacío", k >= 0 && cu->el[k].V.est == M_LIBRE);
    TECLAS(T_ESC);
    /* [?!] en el valor de C1: en CA se admite */
    k = el_de_etiqueta(cu, "C1");
    chk_cond("ficha CA", "de vuelta en el editor", ui_pantalla() == P_EDITOR);
    chk_cond("ficha CA", "la ficha de C1", abre_ficha_de("C1"));
    TECLAS(T_INCOG);
    chk_cond("ficha CA", "[?!] en el valor de C1: se busca en CA", k >= 0 && cu->el[k].val.est == M_BUSCAR);
    TECLAS(T_ENTER);
    chk_cond("estado", "Pides C1 en CA", ve("Pides C1"));
    /* la E vuelve a continua: se quitan ángulos y el [?!] de C1 */
    k = el_de_etiqueta(cu, "E1");
    chk_cond("ficha CA", "la ficha de E1", abre_ficha_de("E1") && ve("alterna (fasor)"));
    TECLAS(T_DER);
    vuelca("ca_ficha_quita_ca");
    chk_cond("ficha CA", "[der] a continua: ya no es CA", k >= 0 && cu->el[k].variante == ONDA_CC && !circ_alterna(cu) &&
             cu->el[el_de_etiqueta(cu, "C1")].val.est != M_BUSCAR && ve("Ya no es CA"));
    TECLAS(T_ESC);

    /* ---- El convenio de pico: 170 en la E guarda 120,208 ---- */
    dibuja_t1();
    TECLAS(T_MENU);
    for (j = 0; j < 6; j++) TECLAS(T_ABAJO);
    TECLAS(T_ENTER);
    chk_cond("menú", "la opción 6 en alterna: los ajustes de CA", ui_pantalla() == P_AJUSTES && ve("Ajustes de CA"));
    TECLAS(T_ABAJO, T_DER, T_ENTER);
    chk_cond("ajustes CA", "el convenio de pico queda en el circuito", (cu->conv & CONV_PICO) != 0);
    k = el_de_etiqueta(cu, "E1");
    abre_ficha_de("E1");
    TECLAS(T_DIG(1), T_DIG(7), T_DIG(0), T_ENTER);
    chk_t("ficha CA", "de pico: 170 se guarda como 170/raiz(2)", k >= 0 ? cu->el[k].val.v : 0.0, 120.20815280171308, 120.0, 1e-14);
    chk_cond("ficha CA", "y se ve 170", abre_ficha_de("E1") && ve("170"));
    TECLAS(T_ESC);

    /* ---- La línea de estado sin f ---- */
    dibuja_t1();
    abre_ficha_de("E1");
    for (j = 0; j < 8 && !ve("Frecuencia (la de todo el circuito)"); j++) TECLAS(T_ABAJO);
    TECLAS(T_BORRAR, T_ESC);
    chk_cond("estado", "sin f: «Para CA falta f»", cu->frec.est == M_LIBRE && ve("Para CA falta f"));

    /* ---- El T1: el diagrama fasorial por teclas ---- */
    dibuja_t1();
    TECLAS(T_IGUAL, T_ENTER);
    vuelca("ca_fasorial_t1");
    chk_cond("fasorial", "[enter] en P_CA: P_FASORIAL", ui_pantalla() == P_FASORIAL);
    {
        const Fasorial *z = ui_fasorial();
        const CaRes *r = ui_ca();
        int kE = el_de_etiqueta(cu, "E1"), kR = el_de_etiqueta(cu, "R1"), kL = el_de_etiqueta(cu, "L1"), kC = el_de_etiqueta(cu, "C1");
        int iVE = fz_busca(z, kE, 'V'), iIE = fz_busca(z, kE, 'I'), iVR = fz_busca(z, kR, 'V');
        int iVL = fz_busca(z, kL, 'V'), iVC = fz_busca(z, kC, 'V');
        int x, y, xe, ye;
        chk_cond("fasorial", "se ven V(E1), I(E1), V(R1), V(L1) y V(C1)", iVE >= 0 && iIE >= 0 && iVR >= 0 && iVL >= 0 && iVC >= 0 &&
                 z->ver[iVE] && z->ver[iIE] && z->ver[iVR] && z->ver[iVL] && z->ver[iVC]);
        for (j = 0; j < 64 && z->sel != iIE; j++) TECLAS(T_ABAJO);
        TECLAS(T_LETRA('R'));
        fz_punta(z, r, iVR, FZ_CX, FZ_CY, &x, &y);
        chk_cond("fasorial", "[R] sobre I(E1): V(R1) en el eje", z->ref == iIE && y == FZ_CY);
        TECLAS(T_LETRA('R'));
        chk_cond("fasorial", "[R] otra vez: sin referencia", z->ref < 0);
        for (j = 0; j < 64 && z->sel != iVR; j++) TECLAS(T_ABAJO);
        TECLAS(T_LETRA('S'));
        vuelca("ca_fasorial_cadena");
        fz_cadena_fin(z, cu, r, cu->conv, &x, &y);
        fz_punta(z, r, iVE, FZ_CX, FZ_CY, &xe, &ye);
        chk_cond("fasorial", "[S] sobre V: la cadena acaba en E1 (±1 px)", abs(x - xe) <= 1 && abs(y - ye) <= 1);
        TECLAS(T_DER);
        vuelca("ca_fasorial_potencias");
        chk_cond("fasorial", "[der]: P = 906,1 W y Q = -695,5 var", ve("P = 906,1 W") && ve("Q = -695,5 var"));
        chk_cond("fasorial", "S = 1142 VA", ve("S = 1142 VA"));
        TECLAS(T_PUNTO);
        chk_cond("fasorial", "[.] cambia la forma de la sesión", ui_pantalla() == P_FASORIAL);
        TECLAS(T_PUNTO, T_ESC);
        chk_cond("fasorial", "[esc] vuelve a P_CA", ui_pantalla() == P_CA);
    }
    /* [tab][tab]: de la tabla también se llega */
    TECLAS(T_TAB, T_TAB);
    chk_cond("fasorial", "[tab] en la tabla: el diagrama", ui_pantalla() == P_FASORIAL);
    TECLAS(T_TAB);
    chk_cond("fasorial", "[tab] en el diagrama: P_CA", ui_pantalla() == P_CA);
    /* [O]: el osciloscopio enseña cómo se llega al régimen */
    TECLAS(T_LETRA('O'));
    for (j = 0; j < 4000 && ui_pantalla() == P_EMULANDO; j++) ui_tick();
    chk_cond("P_CA", "[O]: emula y enseña el osciloscopio", ui_pantalla() == P_OSC);
    TECLAS(T_ESC);
    chk_cond("P_CA", "[esc] vuelve a P_CA", ui_pantalla() == P_CA);
    /* [B] desde P_CA barre y [esc] vuelve */
    TECLAS(T_LETRA('B'));
    for (j = 0; j < 4000 && ui_pantalla() == P_EMULANDO; j++) ui_tick();
    chk_cond("P_CA", "[B]: el Bode del T1", ui_pantalla() == P_BODE);
    TECLAS(T_ESC);
    chk_cond("P_CA", "[esc] del Bode vuelve a P_CA", ui_pantalla() == P_CA);
    TECLAS(T_ESC);

    /* ---- La calculadora de CA por teclas ---- */
    dibuja_l_voltimetro();
    vuelca("ca_calc_editor");
    k = el_de_etiqueta(cu, "V1");
    chk_cond("calculadora CA", "el V nuevo en alterna sale en MED_CA", k >= 0 && cu->el[k].variante == MED_CA);
    chk_cond("calculadora CA", "estado: Pides L1: listo", ve("Pides L1: listo"));
    TECLAS(T_IGUAL);
    for (j = 0; j < 4000 && ui_pantalla() == P_EMULANDO; j++) ui_tick();
    vuelca("ca_calc_resultado");
    chk_cond("calculadora CA", "[=]: P_CA con L1 = 106,1 mH", ui_pantalla() == P_CA && ui_ca_sol() == 0 && ve("L1 = 106,1 mH"));
    k = el_de_etiqueta(cu, "L1");
    chk_cond("calculadora CA", "el circuito del editor sigue pidiendo L1", k >= 0 && cu->el[k].val.est == M_BUSCAR);
    TECLAS(T_ENTER);
    chk_cond("calculadora CA", "[enter]: el diagrama de la solución", ui_pantalla() == P_FASORIAL);
    TECLAS(T_ESC, T_LETRA('T'));
    chk_cond("calculadora CA", "[T] sin A y B: aviso", ui_pantalla() == P_CA);
    TECLAS(T_ESC);
    chk_cond("calculadora CA", "[esc]: al editor", ui_pantalla() == P_EDITOR);
    /* con V1 = 130: no se alcanza */
    abre_ficha_de("V1");
    TECLAS(T_DIG(1), T_DIG(3), T_DIG(0), T_ENTER, T_IGUAL);
    for (j = 0; j < 4000 && ui_pantalla() == P_EMULANDO; j++) ui_tick();
    vuelca("ca_calc_incompat");
    chk_cond("calculadora CA", "V1 = 130: «de casi 0 a 120 V»", ui_pantalla() == P_CA && ve("casi 0"));
    TECLAS(T_ESC);

    /* ---- La paleta: la Z y el vatímetro ---- */
    ui_init();
    TECLAS(T_ENTER, T_ENTER, T_ENTER);
    TECLAS(T_LETRA('E'), T_ABAJO, T_DIG(1), T_DIG(0), T_ENTER);
    TECLAS(T_LETRA('W'), T_DER, T_ESC);
    TECLAS(T_CATALOGO);
    vuelca("ca_paleta");
    chk_cond("paleta", "la Z y el vatímetro en la paleta", ve("Impedancia") && ve("Vat" S_ii "metro"));
    TECLAS(T_ABAJO, T_ABAJO, T_ABAJO, T_DER, T_ENTER, T_ARRIBA);
    k = cu->nel - 1;
    chk_cond("paleta", "[catálogo] 3 [ab] [der] [enter] y una flecha: W1", k >= 0 && cu->el[k].tipo == EL_VATI &&
             strcmp(cu->el[k].etiq, "W1") == 0);
    /* el vatímetro en CC: [?!] se admite; un número no */
    if (ui_pantalla() != P_ELEMENTO) TECLAS(T_ENTER);
    TECLAS(T_DIG(5));
    chk_cond("vatímetro CC", "un número: «son de la CA»", ve("son de la CA"));
    TECLAS(T_INCOG);
    chk_cond("vatímetro CC", "[?!] en P se admite", k >= 0 && cu->el[k].val.est == M_BUSCAR);
    TECLAS(T_ESC, T_ESC);

    /* ---- El menú en CC: la opción 6 es la emulación; [arr] en el menú es Salir ---- */
    TECLAS(T_MENU);
    for (j = 0; j < 6; j++) TECLAS(T_ABAJO);
    TECLAS(T_ENTER);
    chk_cond("menú", "la opción 6 en CC: tiempo y paso", ui_pantalla() == P_AJUSTES && ve("Tiempo total"));
    TECLAS(T_ESC, T_MENU, T_ARRIBA, T_ENTER);
    chk_cond("menú", "[menu][arr][enter]: salir al menú principal", ui_pantalla() == P_MENU);
    chk_cond("interfaz CA (2)", "sin textos fuera de la pantalla", gfx_desbordes() == 0);

    /* ---- Las páginas Modelo: el texto del tipo (y el de la forma de E y J)
     * cabe en las 15 líneas de 52 de la pantalla ---- */
    {
        int t, v, peor = 0;
        for (t = 0; t < EL_N; t++) {
            const TipoInfo *ti = tipo_info(t);
            int nv = (t == EL_FUENTE || t == EL_CORR) ? FORMA_N : 1;
            for (v = 0; v < nv; v++) {
                static char txt[1200];
                char q[64];
                int n;
                sprintf(txt, "%s%s", ti->modelo ? ti->modelo : "", nv > 1 ? FORMA_MODELO[v] : "");
                n = lineas(txt, 52);
                if (n > peor) peor = n;
                sprintf(q, "Modelo de %s (forma %d)", ti->nombre, v);
                chk_cond(q, "cabe en 15 líneas de 52", n <= 15);
            }
        }
        printf("    páginas Modelo: la más larga, %d líneas\n", peor);
    }
}

/* --------------------------------------------------------------------------
 *  53. Etapa 3: la biblioteca de la alterna (lección 11)
 * ----------------------------------------------------------------------- */
static int el_de_etiqueta(const Circuito *c, const char *etiq)
{
    int k;
    for (k = 0; etiq && k < c->nel; k++) if (strcmp(c->el[k].etiq, etiq) == 0) return k;
    return -1;
}

/* La clave de un modelo de CA y el tiempo estimado en la calculadora */
static double medida_ca(const CircModelo *md, Circuito *c, double *t_est)
{
    static Red r;
    static CaRes cr;
    static CaThev th;
    int k = el_de_etiqueta(c, md->clave_el);
    red_extraer(c, &r);
    *t_est = 0.0;
    switch (md->medida) {
    case 'f':
    case 'Q': {
        BodeAjustes aj;
        aj.ent = -1; aj.sal = BS_AUTO; aj.f_ini = aj.f_fin = 0.0;
        if (bode_iniciar(c, &r, &aj) != SIM_OK) return -1e300;
        while (bode_avanzar(~0ul)) {}
        *t_est = bode_res()->t_est;
        if (md->medida == 'Q') return bode_res()->m.q;
        return bode_res()->m.tipo == BODE_PBANDA ? bode_res()->m.f1 : bode_res()->m.fc;
    }
    case 'T':
        ca_ops_reset();
        if (ca_thevenin(c, &r, c->frec.v, &th) != SIM_OK) return -1e300;
        *t_est = (double)ca_ops() * CA_NS_OP * 1e-9;          /* lo medido (ca.h) */
        return th.Pmax;
    case 'c': {                                        /* la calculadora de CA */
        const CalRes *s;
        int j;
        if (cal_iniciar(c, &r) != CAL_OK) return -1e300;
        while (cal_avanzar(~0ul)) {}
        s = cal_res();
        *t_est = (double)s->ops * SIM_NS_OP * 1e-9;
        if (s->nsol != 1) return -1e300;               /* una sola solución */
        for (j = 0; j < s->ninc; j++)
            if (s->inc[j].el == k && s->inc[j].que == IN_VAL) return s->sol[0].x[j];
        return -1e300;
    }
    default:
        ca_ops_reset();
        if (ca_analiza(c, &r, &cr) != SIM_OK || k < 0) return -1e300;
        *t_est = (double)ca_ops() * CA_NS_OP * 1e-9;          /* lo medido (ca.h) */
        switch (md->medida) {
        case 'V': return cx_abs(cr.V[k]) / sqrt(2.0);
        case 'I': return cx_abs(cr.I[k]) / sqrt(2.0);
        case 'L': return cr.lectura[k];
        case 'p': return cr.fp[k];
        default:  return -1e300;
        }
    }
}

static void test_biblioteca_ca(void)
{
    static Circuito c;
    int i, n = 0;

    printf("[53] Etapa 3: la biblioteca de la alterna\n");
    for (i = 0; i < N_MODELOS; i++) {
        const CircModelo *md = &MODELOS[i];
        char q[64];
        double x, te;
        if (!(md->ca == 1 || md->ca == 2)) continue;  /* 3 y 4 (CC de la Etapa 4): [66] */
        n++;
        sprintf(q, "modelo %d", i + 1);
        chk_cond(q, "título de 44 columnas o menos", strlen(md->titulo) <= 44);
        chk_cond(q, "enunciado en 3 líneas de 52", lineas(md->enunciado, 52) <= 3);
        chk_cond(q, "qué ver en 2 líneas de 52", lineas(md->que_ver, 52) <= 2);
        modelo_cargar(i, &c);
        chk_cond(q, "circuito válido y de alterna", circ_valido(&c) && circ_alterna(&c));
        chk_cond(q, "pide algo solo si es de la calculadora", circ_pide(&c) == (md->medida == 'c'));
        x = medida_ca(md, &c, &te);
        printf("    %-40s clave '%c' = %-12.10g estimado %.3f s\n", md->titulo, md->medida, x, te);
        chk_cond(q, "en la calculadora, menos de 0,5 s (estimado)", te < 0.5);
        pruebas++;
        if (!(fabs(x - md->clave) <= md->tol * fabs(md->clave))) {
            char d[96];
            sprintf(d, "clave '%c' = %.10g", md->medida, md->clave);
            falla(q, d, x, md->clave);
        }
    }
    chk_cond("biblioteca CA", "diez modelos de la alterna", n == 10);
    /* Por la interfaz: la lección 11 y cada modelo con [enter] */
    for (i = 0; i < 10; i++) {
        const CircModelo *md = &MODELOS[12 + i];
        char q[64];
        int k, p;
        sprintf(q, "lección 11, modelo %d", i + 1);
        ui_init();
        TECLAS(T_ENTER, T_ABAJO, T_ENTER);
        repite(T_ABAJO, 10);
        TECLAS(T_ENTER, T_DER, T_DER);
        if (i == 0) chk_cond("lección 11", "la lista de modelos de la alterna", ui_pantalla() == P_EJEMPLOS && ve(md->titulo));
        repite(T_ABAJO, i);
        TECLAS(T_ENTER);
        for (k = 0; k < 2000 && ui_pantalla() == P_EMULANDO; k++) ui_tick();
        p = ui_pantalla();
        if (md->ca == 2) { chk_cond(q, "abre el Bode", p == P_BODE); continue; }
        chk_cond(q, "abre P_CA sin error", p == P_CA && ui_ca()->d.estado == SIM_OK);
        if (md->medida == 'c') {
            char d[64];
            modelo_cargar(12 + i, &c);
            k = el_de_etiqueta(&c, md->clave_el);
            chk_cond(q, "con la calculadora: lo hallado en su color", ui_ca_sol() == 0 && k >= 0 && ((ui_ca()->buscado >> k) & 1u));
            sprintf(d, "%s = ", md->clave_el ? md->clave_el : "?");
            chk_cond(q, "y escrito en el panel", ve(d));
            if (k >= 0) chk_t(q, "el valor hallado", ui_ca()->val[k], md->clave, md->clave, md->tol);
            TECLAS(T_IGUAL);
            for (k = 0; k < 2000 && ui_pantalla() == P_EMULANDO; k++) ui_tick();
            chk_cond(q, "[=] vuelve a calcular lo mismo", ui_pantalla() == P_CA && ui_ca_sol() == 0);
        } else chk_cond(q, "sin la calculadora", ui_ca_sol() < 0);
        TECLAS(T_ENTER);
        chk_cond(q, "[enter]: el diagrama fasorial", ui_pantalla() == P_FASORIAL);
        TECLAS(T_DER);
        TECLAS(T_ESC);
        chk_cond(q, "[esc] vuelve a P_CA", ui_pantalla() == P_CA);
        TECLAS(T_ESC);
        chk_cond(q, "[esc] vuelve a la lista", ui_pantalla() == P_EJEMPLOS);
    }
    /* el fp de la carga RL: 0,6 inductivo */
    for (i = 0; i < N_MODELOS; i++)
        if (MODELOS[i].medida == 'L' && (MODELOS[i].ca == 1 || MODELOS[i].ca == 2)) {
            static Red r;
            static CaRes cr;
            int k;
            modelo_cargar(i, &c);
            red_extraer(&c, &r);
            ca_analiza(&c, &r, &cr);
            k = el_de_etiqueta(&c, "W1");
            chk_t("carga RL", "fp(W1) = +0,6", k >= 0 ? cr.fp[k] : 0.0, 0.6, 1.0, 1e-9);
        }
}

#include "test_trazador.inc"

#include "test_calca.inc"

#include "test_fasorial.inc"

#include "test_rev3_motor.inc"

#include "test_rev3_ui.inc"

#include "test_rev3_modelo.inc"

#include "test_e4_modelo.inc"

#include "test_e4_ctl.inc"

#include "test_e4_diodo.inc"

#include "test_e4_trans.inc"

#include "test_e4_ao.inc"

#include "test_e4_pq.inc"

#include "test_e4_barr.inc"

#include "test_e4_paso.inc"

#include "test_e4_esq.inc"

#include "test_e4_curva.inc"

#include "test_e4_ui.inc"

#include "test_e4_res.inc"

#include "test_e4_bib.inc"

#include "test_e4_bjt.inc"

#include "test_e4_mos.inc"
#include "test_e4_rev.inc"
#include "test_e4_ora.inc"
#include "test_rev2_barr_a.inc"
#include "test_rev2_barr_b.inc"
#include "test_rev2_ca_res.inc"
#include "test_rev2_dibujo_a.inc"
#include "test_rev2_dibujo_b.inc"
#include "test_rev2_editor.inc"
#include "test_rev3_op_a.inc"
#include "test_rev3_op_b.inc"
#include "test_rev3_tran_a.inc"
#include "test_rev3_tran_b.inc"
#include "test_rev3_v_modelo.inc"
#include "test_m_editor.inc"
#include "test_m_resultados.inc"
#include "test_flanco.inc"

#include "test_nolin.inc"

/* --------------------------------------------------------------------------
 *  29. Redes al azar para el oráculo independiente
 *
 *  El arnés arma redes RLC al azar (fuentes de todas las formas,
 *  interruptores, instrumentos), las emula y escribe en out/oraculo:
 *    caso_NNN.net  el netlist, en el formato de herramientas/oraculo_sim.py
 *    caso_NNN.ces  t, canal 1, canal 2 del emulador, en los instantes k·dt
 *  herramientas/compara_oraculo.py corre el oráculo sobre cada netlist y
 *  compara. Aquí solo se comprueba que el emulador las acepta.
 * ----------------------------------------------------------------------- */
static void escribe_forma(FILE *f, const SimEl *e)
{
    switch (e->forma) {
    case ONDA_SENO:       fprintf(f, "SIN %.17g %.17g %.17g %.17g", e->p[0], e->p[1], e->p[2], e->p[3]); break;
    case ONDA_CUADRADA:   fprintf(f, "SQR %.17g %.17g %.17g %.17g", e->p[0], e->p[1], e->p[2], e->p[3]); break;
    case ONDA_TRIANGULAR: fprintf(f, "TRI %.17g %.17g %.17g %.17g", e->p[0], e->p[1], e->p[2], e->p[3]); break;
    case ONDA_PULSO:      fprintf(f, "PUL %.17g %.17g %.17g %.17g %.17g", e->p[0], e->p[1], e->p[2], e->p[3], e->p[4]); break;
    default:              fprintf(f, "DC %.17g", e->p[0]); break;
    }
}

/* Nodo del netlist: las referencias van al 0 (unir subcircuitos por su
 * referencia no cambia nada: no hay camino para una corriente) */
static int nodo_net(const SimRed *s, int n) { return s->ref[n] ? 0 : n + 1; }

/* El estado del que arranca el emulador: v0 de cada C e i0 de cada L (la
 * de la ficha, o la del punto de operación con las fuentes en reposo). Se
 * escribe explícito: el oráculo resuelve su punto de operación con las
 * fuentes en t = 0, no en reposo, y así los dos parten de lo mismo. */
static double estado_inicial(const SimRed *s, int k)
{
    const SimEl *e = &s->el[k];
    if (e->ci) return e->x0;
    return e->tipo == SE_C ? sim_res()->op.V[k] : sim_res()->op.I[k];
}

static void escribe_netlist(const char *ruta, const SimRed *s, double tfin, double dt,
                            const SimAjustes *aj)
{
    FILE *f = fopen(ruta, "w");
    int k;
    if (!f) return;
    fprintf(f, "# Hakaru CES: red al azar para el oraculo (texto ASCII)\n");
    fprintf(f, "tfin %.17g h %.17g\n", tfin, dt);
    for (k = 0; k < s->nel; k++) {
        const SimEl *e = &s->el[k];
        int a = nodo_net(s, e->a), b = nodo_net(s, e->b);
        switch (e->tipo) {
        case SE_R:   fprintf(f, "R R%d %d %d %.17g\n", k, a, b, e->val); break;
        case SE_C: fprintf(f, "C C%d %d %d %.17g %.17g\n", k, a, b, e->val, estado_inicial(s, k)); break;
        case SE_L: fprintf(f, "L L%d %d %d %.17g %.17g\n", k, a, b, e->val, estado_inicial(s, k)); break;
        case SE_E:   fprintf(f, "V V%d %d %d %.17g ", k, a, b, e->val); escribe_forma(f, e); fprintf(f, "\n"); break;
        case SE_J:   fprintf(f, "I I%d %d %d ", k, a, b); escribe_forma(f, e); fprintf(f, "\n"); break;
        case SE_S: {
            /* aquí t1 = 0 conmuta en 0+; en el oráculo, t1 = 0 es "nunca": se
             * escribe el estado de 0+ */
            int c0 = e->cerrado0;
            double t1 = e->t1, t2 = e->t2 > 0.0 ? e->t2 : 0.0;
            if (t1 == 0.0) { c0 = !c0; t1 = 0.0; }
            fprintf(f, "S S%d %d %d %d %.17g %.17g\n", k, a, b, c0, t1 > 0.0 ? t1 : 0.0, t2);
            break;
        }
        case SE_AMI: fprintf(f, "AM A%d %d %d 0\n", k, a, b); break;
        case SE_VMI: fprintf(f, "VM M%d %d %d 0\n", k, a, b); break;
        default: break;
        }
    }
    /* qué comparar: los dos canales, como nodo (tensión) o corriente */
    for (k = 0; k < 2; k++) {
        const SimCanal *c = &aj->canal[k];
        if (c->tipo == CANAL_NODO) fprintf(f, "# canal %d V %d\n", k + 1, nodo_net(s, c->nodo));
        else if (c->tipo == CANAL_I) {
            static const char *const LETRA[SE_N] = { "R", "V", "I", "C", "L", "S", "A", "M", "Z" };
            fprintf(f, "# canal %d I %s%d\n", k + 1, LETRA[s->el[c->elem].tipo], c->elem);
        }
    }
    fclose(f);
}

static double forma_al_azar(SimEl *e, double escala_t)
{
    double f = 1.0 / (escala_t * (0.5 + 0.1 * (double)azar(40)));
    int fo = (int)azar(5);
    e->forma = (unsigned char)fo;
    switch (fo) {
    case ONDA_SENO:       e->p[0] = 1.0 + azar(9); e->p[1] = f; e->p[2] = 15.0 * (double)azar(24); e->p[3] = (double)azar(5) - 2.0; break;
    case ONDA_CUADRADA:
    case ONDA_TRIANGULAR: e->p[0] = 1.0 + azar(9); e->p[1] = f; e->p[2] = 10.0 * (double)(1 + azar(9)); e->p[3] = (double)azar(5) - 2.0; break;
    case ONDA_PULSO:
        e->p[0] = (double)azar(3); e->p[1] = 1.0 + azar(9);
        e->p[2] = escala_t * 0.1 * (double)azar(10);
        e->p[3] = azar(3) ? escala_t * 0.1 * (double)(1 + azar(20)) : 0.0;
        e->p[4] = (e->p[3] > 0.0 && azar(2)) ? e->p[3] * (1.5 + 0.5 * (double)azar(4)) : 0.0;
        break;
    default:              e->p[0] = 1.0 + azar(20); break;
    }
    return f;
}

/* El valor de un canal en t, y si t cae a menos de tol de una muestra (un
 * evento), el de después (como escribe el oráculo) */
static double canal_en_tol(int c, double t, double tol)
{
    const SimReg *g = sim_reg();
    int lo = 0, hi = g->n, i;
    while (lo < hi) { int m = (lo + hi) / 2; if (g->t[m] < t - tol) lo = m + 1; else hi = m; }
    i = lo;
    if (i < g->n && fabs(g->t[i] - t) <= tol) {
        while (i + 1 < g->n && fabs(g->t[i + 1] - t) <= tol) i++;
        return g->y[c][i];
    }
    return sim_canal_en(c, t);
}

static void test_oraculo_export(void)
{
    char dir[300], ruta[340];
    int caso, n_ok = 0, n_err = 0, k, i;
    const int NCASOS = 120;

    printf("[29] Redes al azar para el oráculo (out/oraculo)\n");
    sprintf(dir, "%s/oraculo", dir_out);
    mkdir(dir, 0755);
    for (caso = 0; caso < NCASOS; caso++) {
        int nn = 3 + (int)azar(5), nel = 3 + (int)azar(8), fuente;
        double tau = 1e-3 * (0.2 + 0.1 * (double)azar(30));
        SimAjustes aj;
        memset(&sr, 0, sizeof sr);
        memset(&aj, 0, sizeof aj);
        sr.nn = nn; sr.ref[0] = 1;
        /* una fuente de tensión (con r a veces) entre 0 y 1 */
        fuente = sr_el(SE_E, 0, 1, azar(3) == 0 ? 1.0 + azar(20) : 0.0);
        forma_al_azar(&sr.el[fuente], tau);
        /* un árbol que conecta todos los nodos con R, luego el resto al azar */
        for (i = 2; i < nn; i++) sr_el(SE_R, (int)azar((unsigned)i), i, 100.0 * (1 + azar(50)));
        for (i = sr.nel; i < nel + nn - 2 && sr.nel < MAX_EL; i++) {
            int a = (int)azar((unsigned)nn), b = (int)azar((unsigned)nn), t = (int)azar(10);
            if (a == b) continue;
            if (t < 3) sr_el(SE_R, a, b, 100.0 * (1 + azar(50)));
            else if (t < 6) {
                k = sr_el(SE_C, a, b, tau / (100.0 * (1 + azar(50))) * (0.5 + azar(3)));
                if (azar(4) == 0) { sr.el[k].ci = 1; sr.el[k].x0 = (double)azar(11) - 5.0; }
            } else if (t < 8) {
                k = sr_el(SE_L, a, b, tau * 100.0 * (1 + azar(50)) * (0.5 + azar(3)));
                if (azar(4) == 0) { sr.el[k].ci = 1; sr.el[k].x0 = 0.01 * ((double)azar(11) - 5.0); }
            } else if (t == 8) {
                k = sr_el(SE_S, a, b, 0.0);
                sr.el[k].cerrado0 = (unsigned char)azar(2);
                sr.el[k].t1 = tau * (0.5 + 0.1 * (double)azar(20));
                if (azar(2)) sr.el[k].t2 = sr.el[k].t1 + tau * (0.5 + 0.1 * (double)azar(20));
            } else {
                k = sr_el(SE_J, a, b, 0.0);
                forma_al_azar(&sr.el[k], tau);
                for (i = 0; i < ONDA_PAR; i++) sr.el[k].p[i] *= (i == 0 || (sr.el[k].forma == ONDA_PULSO && i < 2) ||
                                                                 (sr.el[k].forma != ONDA_PULSO && i == 3)) ? 1e-3 : 1.0;
            }
        }
        canal_de(&aj, &sr, 0, CANAL_NODO, 1 + (int)azar((unsigned)(nn - 1)));
        canal_de(&aj, &sr, 1, CANAL_I, azar(2) ? fuente : (int)azar((unsigned)sr.nel));
        aj.t_fin = 10.0 * tau;
        if (sim_emular(&sr, &aj) != SIM_OK) { n_err++; continue; }
        n_ok++;
        sprintf(ruta, "%s/caso_%03d.net", dir, caso);
        escribe_netlist(ruta, &sr, aj.t_fin, aj.t_fin / 400.0, &aj);
        /* los modos de la topología de 0+ (cada par complejo una vez) */
        sprintf(ruta, "%s/caso_%03d.modos", dir, caso);
        {
            FILE *f = fopen(ruta, "w");
            if (f) {
                const SimRes *rm = sim_res();
                for (i = 0; i < rm->nmodos[0]; i++) fprintf(f, "%.17g %.17g\n", rm->modo[0][i].re, rm->modo[0][i].im);
                fclose(f);
            }
        }
        /* con el paso automático y con la mitad: el cociente de los errores
         * dice el orden */
        {
            double h = sim_res()->h;
            int vez;
            for (vez = 0; vez < 2; vez++) {
                FILE *f;
                if (vez == 1) { aj.h = 0.5 * h; if (sim_emular(&sr, &aj) != SIM_OK) break; }
                sprintf(ruta, "%s/caso_%03d%s.ces", dir, caso, vez ? "_h2" : "");
                f = fopen(ruta, "w");
                if (!f) continue;
                fprintf(f, "t,y1,y2\n");
                for (i = 0; i <= 400; i++) {
                    double t = aj.t_fin * i / 400.0, tol = 1e-12 * aj.t_fin;
                    fprintf(f, "%.17g,%.17g,%.17g\n", t, canal_en_tol(0, t, tol), canal_en_tol(1, t, tol));
                }
                fclose(f);
            }
            aj.h = 0.0;
        }
    }
    printf("    %d redes emuladas y exportadas; %d rechazadas (lazos, cortes)\n", n_ok, n_err);
    pruebas++;
    if (n_ok < NCASOS / 2) falla("oráculo", "el emulador acepta la mayoría de las redes al azar", n_ok, NCASOS / 2);
}

/* --------------------------------------------------------------------------
 *  29b. Redes de CA al azar para el oráculo de CA
 *
 *  Redes con R, C, L, Z, fuentes alternas (E con r, J), amperímetros y
 *  voltímetros con y sin R, interruptores (cuentan en su estado final) y
 *  vatímetros, a una f al azar de 10 Hz a 10 kHz. En out/oraculo_ca:
 *    caso_NNN.net  la red en CESNET 2 (herramientas/oraculo_ca.py)
 *    caso_NNN.ces  lo que da el programa: fasores de nodo y corrientes
 *                  (pico, coseno), lecturas y cuatro puntos del Bode
 *  herramientas/oraculo_ca.py las resuelve con su propio MNA complejo y
 *  compara: fasores a 1e-12 de la escala, el Bode a 1e-10.
 * ----------------------------------------------------------------------- */
static const char *nombre_ca(const SimRed *s, int k, char *buf)
{
    static const char *const LETRA[SE_N] = { "R", "V", "I", "C", "L", "S", "A", "M", "Z" };
    const SimEl *e = &s->el[k];
    sprintf(buf, "%s%d", e->tipo == SE_AMI && e->instr == 'W' ? "W" : LETRA[e->tipo], k);
    return buf;
}

static void test_oraculo_ca_export(void)
{
    char dir[300], ruta[340], nom[16];
    int caso, n_ok = 0, n_err = 0;
    const int NCASOS = 200;

    printf("[29b] Redes de CA al azar para el oráculo de CA (out/oraculo_ca)\n");
    sprintf(dir, "%s/oraculo_ca", dir_out);
    mkdir(dir, 0755);
    for (caso = 0; caso < NCASOS; caso++) {
        int nn = 3 + (int)azar(5), nel = 3 + (int)azar(8), fuente, k, i, nodo;
        double f = pow(10.0, 1.0 + 3.0 * (double)azar(1000) / 1000.0), X;
        unsigned mask;
        FILE *fo;
        SimAjustes aj;
        sr_init(nn);
        fuente = sr_alterna(SE_E, 0, 1, 1.0 + azar(20), 15.0 * (double)azar(24) - 180.0, f);
        if (azar(3) == 0) sr.el[fuente].val = 1.0 + azar(20);          /* con r */
        for (i = 2; i < nn; i++) sr_el(SE_R, (int)azar((unsigned)i), i, 100.0 * (1 + azar(50)));
        X = 1.0 / (2.0 * PI_T * f);
        for (i = sr.nel; i < nel + nn - 2 && sr.nel < MAX_EL; i++) {
            int a = (int)azar((unsigned)nn), b = (int)azar((unsigned)nn), t = (int)azar(12);
            if (a == b) continue;
            if (t < 2) sr_el(SE_R, a, b, 100.0 * (1 + azar(50)));
            else if (t < 4) sr_el(SE_C, a, b, X / (100.0 * (1 + azar(50))));
            else if (t < 6) sr_el(SE_L, a, b, 100.0 * (1 + azar(50)) * X);
            else if (t == 6) sr_z(a, b, 100.0 * (1 + azar(50)), (double)azar(179) - 89.0, f);
            else if (t == 7) {
                k = sr_alterna(SE_J, a, b, 1e-3 * (1 + azar(20)), 15.0 * (double)azar(24) - 180.0, f);
                (void)k;
            } else if (t == 8) {
                k = sr_el(SE_AMI, a, b, 0.0); sr.el[k].instr = 'A'; sr.el[k].med = MED_CA;
                if (azar(2)) { sr.el[k].tipo = SE_R; sr.el[k].val = 1.0 + azar(10); }   /* A con R: una R */
            } else if (t == 9) {
                k = sr_el(SE_VMI, a, b, 0.0); sr.el[k].instr = 'V'; sr.el[k].med = MED_CA;
                if (azar(2)) { sr.el[k].tipo = SE_R; sr.el[k].val = 1e4 * (1 + azar(10)); }
            } else if (t == 10) {
                k = sr_el(SE_S, a, b, 0.0);
                sr.el[k].cerrado0 = (unsigned char)azar(2);
                if (azar(2)) sr.el[k].t1 = 1e-3 * (1 + azar(9));
                if (sr.el[k].t1 > 0.0 && azar(2)) sr.el[k].t2 = sr.el[k].t1 + 1e-3 * (1 + azar(9));
            } else {
                k = sr_el(SE_AMI, a, b, 0.0); sr.el[k].instr = 'W';
            }
        }
        if (ca_sr(0.0) != SIM_OK) { n_err++; continue; }
        n_ok++;
        mask = sim_mascara_final(&sr);
        /* la red */
        sprintf(ruta, "%s/caso_%03d.net", dir, caso);
        fo = fopen(ruta, "w");
        if (!fo) continue;
        fprintf(fo, "# Hakaru CES: red de CA al azar para el oraculo de CA (texto ASCII)\ncesnet 2\nca %.17g\n", f);
        for (k = 0; k < sr.nel; k++) {
            const SimEl *e = &sr.el[k];
            int a = nodo_net(&sr, e->a), b = nodo_net(&sr, e->b);
            nombre_ca(&sr, k, nom);
            switch (e->tipo) {
            case SE_R: fprintf(fo, "R %s %d %d %.17g\n", nom, a, b, e->val); break;
            case SE_C: fprintf(fo, "C %s %d %d %.17g\n", nom, a, b, e->val); break;
            case SE_L: fprintf(fo, "L %s %d %d %.17g\n", nom, a, b, e->val); break;
            case SE_Z: fprintf(fo, "Z %s %d %d %.17g %.17g %.17g\n", nom, a, b, e->val, e->p[0], e->p[1]); break;
            case SE_E: fprintf(fo, "V %s %d %d %.17g %.17g %.17g\n", nom, a, b, e->val, e->p[0], e->p[2] - 90.0); break;
            case SE_J: fprintf(fo, "I %s %d %d %.17g %.17g\n", nom, a, b, e->p[0], e->p[2] - 90.0); break;
            case SE_S: fprintf(fo, "S %s %d %d %d\n", nom, a, b, (mask >> k) & 1u ? 1 : 0); break;
            case SE_AMI: fprintf(fo, e->instr == 'W' ? "W %s %d %d\n" : "AM %s %d %d 0\n", nom, a, b); break;
            case SE_VMI: fprintf(fo, "VM %s %d %d 0\n", nom, a, b); break;
            default: break;
            }
        }
        /* el Bode: de la E a un nodo, cuatro puntos de la malla */
        nodo = 1 + (int)azar((unsigned)(nn - 1));
        memset(&aj, 0, sizeof aj);
        canal_de(&aj, &sr, 0, CANAL_NODO, nodo);
        {
            int bo = bode_iniciar_red(&sr, fuente, &aj.canal[0], -1, -1, f / 100.0, f * 100.0, f) == SIM_OK;
            if (bo) while (bode_avanzar(~0ul)) {}
            if (bo && bode_reg()->n > 3) {
                const BodeReg *g = bode_reg();
                int q[4];
                q[0] = 0; q[1] = g->n / 3; q[2] = 2 * g->n / 3; q[3] = g->n - 1;
                for (i = 0; i < 4; i++)
                    if (g->ok[q[i]]) fprintf(fo, "bode %s %d %.17g\n", nombre_ca(&sr, fuente, nom), nodo_net(&sr, nodo), g->f[q[i]]);
            }
            fclose(fo);
            /* lo que da el programa */
            sprintf(ruta, "%s/caso_%03d.ces", dir, caso);
            fo = fopen(ruta, "w");
            if (!fo) continue;
            /* ca_sr otra vez: el Bode usa la misma LUC estática */
            ca_sr(0.0);
            for (i = 0; i < sr.nn; i++) if (!sr.ref[i]) fprintf(fo, "V %d %.17g %.17g\n", nodo_net(&sr, i), car.phi[i].re, car.phi[i].im);
            for (k = 0; k < sr.nel; k++) {
                const SimEl *e = &sr.el[k];
                nombre_ca(&sr, k, nom);
                fprintf(fo, "I %s %.17g %.17g\n", nom, car.I[k].re, car.I[k].im);
                if (e->tipo == SE_AMI || e->tipo == SE_VMI) fprintf(fo, "M %s %.17g\n", nom, car.lectura[k]);
            }
            if (bo && bode_reg()->n > 3) {
                const BodeReg *g = bode_reg();
                int q[4];
                q[0] = 0; q[1] = g->n / 3; q[2] = 2 * g->n / 3; q[3] = g->n - 1;
                for (i = 0; i < 4; i++)
                    if (g->ok[q[i]]) fprintf(fo, "H %.17g %.17g %.17g\n", g->f[q[i]], g->H[q[i]].re, g->H[q[i]].im);
            }
            fclose(fo);
        }
    }
    printf("    %d redes de CA exportadas; %d rechazadas (lazos, cortes, resonancias)\n", n_ok, n_err);
    pruebas++;
    if (n_ok < NCASOS / 2) falla("oráculo de CA", "el análisis acepta la mayoría de las redes al azar", n_ok, NCASOS / 2);
}

/* --------------------------------------------------------------------------
 *  30. Modelo y archivo de la Etapa 2: parámetros, sondas, secciones
 * ----------------------------------------------------------------------- */
/* Un lector que sabe solo el modelo de la Etapa 1, copiado aquí (versión
 * exacta 1, mínimos 25 y 85, salta lo que sobra de cada bloque): lo que
 * escribe la Etapa 2 sin nada suyo, o solo con sondas, lo tiene que seguir
 * abriendo */
static int lector_e1(const char *ruta, Circuito *t)
{
    FILE *f = fopen(ruta, "rb");
    unsigned char cab[5];
    int x, y, k, i, n, ok = 1;
    if (!f) return 0;
    circ_init(t, "");
    if (fread(cab, 1, 5, f) != 5 || memcmp(cab, "CESP", 4) != 0 || cab[4] != 1) { fclose(f); return 0; }
    if (fgetc(f) != REJ_COLS || fgetc(f) != REJ_FILAS) { fclose(f); return 0; }
    for (i = 0; i < 24; i++) t->nombre[i] = (char)fgetc(f);
    t->nombre[23] = 0;
    for (y = 0; y < REJ_FILAS; y++) for (x = 0; x < REJ_COLS - 1; x++) t->h[y][x] = (unsigned char)fgetc(f);
    for (y = 0; y < REJ_FILAS - 1; y++) for (x = 0; x < REJ_COLS; x++) t->v[y][x] = (unsigned char)fgetc(f);
    n = fgetc(f);                                     /* bloque global */
    if (n < 25) ok = 0;
    t->tierra_x = (signed char)fgetc(f); t->tierra_y = (signed char)fgetc(f);
    for (i = 0; i < n - 2; i++) (void)fgetc(f);       /* el resto no se mira aquí */
    t->nel = fgetc(f);
    for (k = 0; ok && k < t->nel && k < MAX_EL; k++) {
        Elemento *e = &t->el[k];
        n = fgetc(f);
        if (n < 85) { ok = 0; break; }
        e->tipo = (unsigned char)fgetc(f);
        e->xa = (unsigned char)fgetc(f); e->ya = (unsigned char)fgetc(f);
        e->xb = (unsigned char)fgetc(f); e->yb = (unsigned char)fgetc(f);
        for (i = 0; i < 6; i++) e->etiq[i] = (char)fgetc(f);
        if (fread(&e->val.v, 8, 1, f) != 1) ok = 0;
        for (i = 0; i < n - 5 - 6 - 8; i++) (void)fgetc(f);
    }
    if (feof(f)) ok = 0;
    fclose(f);
    return ok;
}

static void test_modelo_e2(void)
{
    static Circuito a, b, d;
    static Red rd;
    char ruta[300];
    FILE *f;
    long tam;

    printf("[30] Modelo y archivo de la Etapa 2\n");
    limpia_mis();
    sprintf(ruta, "%s/ces_01.tns", carpeta_mis);

    /* Un circuito de la Etapa 1: versión 1, bloques de 25 y 85, como siempre */
    ejemplo_cargar(EJ_PH, &a);
    chk_cond("modelo", "sin nada de la Etapa 2: se guarda", arch_guardar(0, &a));
    f = fopen(ruta, "rb");
    if (f) { fseek(f, 0, SEEK_END); tam = ftell(f); fclose(f); } else tam = -1;
    chk_cond("modelo", "sin nada de la Etapa 2: tamaño de la Etapa 1",
             tam == 4 + 1 + 2 + 24 + REJ_FILAS * (REJ_COLS - 1) + (REJ_FILAS - 1) * REJ_COLS + 1 + 25 + 1 + a.nel * (1 + 85));
    chk_cond("modelo", "y el lector de la Etapa 1 lo abre", lector_e1(ruta, &b) && b.nel == a.nel);

    /* Con sondas, tiempo y paso: versión 1 (la Etapa 1 lo lee sin ellos) y
     * bloque global de 53 */
    a.sonda[0].tipo = SONDA_NODO; a.sonda[0].x = 3; a.sonda[0].y = 2;
    a.sonda[1].tipo = SONDA_I; a.sonda[1].el = 1; a.sonda[1].sg = -1;
    a.t_fin.v = 20e-3; a.t_fin.est = M_DATO;
    a.paso.v = 5e-6; a.paso.est = M_DATO;
    a.el[0].variante = MED_CA;                        /* en una R no se valida */
    chk_cond("modelo", "con sondas: es válido", circ_valido(&a));
    chk_cond("modelo", "con sondas: se guarda", arch_guardar(0, &a));
    chk_cond("modelo", "con sondas: se abre igual", arch_cargar(0, &b) && mismos(&a, &b));
    f = fopen(ruta, "rb");
    if (f) { unsigned char v5[5]; if (fread(v5, 1, 5, f) != 5) v5[4] = 0; fclose(f); chk_cond("modelo", "con sondas: versión 1", v5[4] == 1); }
    chk_cond("modelo", "con sondas: el lector de la Etapa 1 lo abre (sin ellas)",
             lector_e1(ruta, &b) && b.nel == a.nel && b.el[0].val.v == a.el[0].val.v);

    /* Un bloque de elemento con una sección 2 de basura (estado 0x5A) no pasa */
    {
        static Circuito g;
        FILE *fo;
        int k2, x, y, i;
        lazo(&g, &rd, 12.0, 4.0, 8.0);
        fo = fopen(ruta, "wb");
        if (fo) {
            fwrite("CESP", 1, 4, fo); fputc(1, fo); fputc(REJ_COLS, fo); fputc(REJ_FILAS, fo);
            for (i = 0; i < 24; i++) fputc(g.nombre[i], fo);
            for (y = 0; y < REJ_FILAS; y++) for (x = 0; x < REJ_COLS - 1; x++) fputc(g.h[y][x], fo);
            for (y = 0; y < REJ_FILAS - 1; y++) for (x = 0; x < REJ_COLS; x++) fputc(g.v[y][x], fo);
            fputc(25, fo);
            fputc(g.tierra_x & 0xFF, fo); fputc(g.tierra_y & 0xFF, fo);
            for (i = 0; i < 4; i++) fputc(0xFF, fo);
            fputc(0, fo); w_mag(fo, &g.frec);
            fputc(g.nel, fo);
            for (k2 = 0; k2 < g.nel; k2++) {
                const Elemento *e = &g.el[k2];
                fputc(85 + 54, fo);
                fputc(e->tipo, fo); fputc(e->xa, fo); fputc(e->ya, fo); fputc(e->xb, fo); fputc(e->yb, fo);
                for (i = 0; i < 6; i++) fputc(e->etiq[i], fo);
                w_mag(fo, &e->val); w_mag(fo, &e->V); w_mag(fo, &e->I); w_mag(fo, &e->aux);
                fputc(0, fo); fputc(0xFF, fo);
                for (i = 0; i < 54; i++) fputc(0x5A, fo);
            }
            fclose(fo);
        }
        chk_cond("modelo", "una sección 2 de basura no se abre", !arch_cargar(0, &b));
    }

    /* circ_valido rechaza, uno a uno */
    {
        struct { const char *que; int caso; } R[] = {
            { "variante 6 en una E (la 5 es la alterna)", 0 }, { "variante 3 en un voltímetro", 1 }, { "[?!] en un parámetro", 2 },
            { "parámetro dato en una R", 3 }, { "sonda con el = nel", 4 }, { "sonda NODO con el", 5 },
            { "sonda con x = 10", 6 }, { "sonda I con sg = 0", 7 }, { "t_fin = 0 dato", 8 },
            { "NaN en un parámetro", 9 }, { "ángulo en un parámetro", 10 }, { "paso de 2 s", 11 },
        };
        int i, kv;
        double cero = 0.0;
        for (i = 0; i < (int)(sizeof R / sizeof R[0]); i++) {
            lazo(&d, &rd, 12.0, 4.0, 8.0);                /* E1, R1, R2 */
            kv = circ_elemento(&d, EL_VOLT, 2, 0, DIR_ABA);
            switch (R[i].caso) {
            case 0: d.el[0].variante = 6; break;
            case 1: d.el[kv].variante = 3; break;
            case 2: d.el[1].par[0].est = M_BUSCAR; break;
            case 3: d.el[1].par[2].est = M_DATO; d.el[1].par[2].v = 1.0; break;
            case 4: d.sonda[0].tipo = SONDA_I; d.sonda[0].el = (signed char)d.nel; d.sonda[0].sg = 1; break;
            case 5: d.sonda[0].tipo = SONDA_NODO; d.sonda[0].x = 1; d.sonda[0].y = 1; d.sonda[0].el = 0; break;
            case 6: d.sonda[0].tipo = SONDA_NODO; d.sonda[0].x = 10; d.sonda[0].y = 1; break;
            case 7: d.sonda[1].tipo = SONDA_I; d.sonda[1].el = 0; d.sonda[1].sg = 0; break;
            case 8: d.t_fin.est = M_DATO; d.t_fin.v = 0.0; break;
            case 9: d.el[0].par[1].v = cero / cero; break;
            case 10: d.el[2].par[3].ang = 5.0; break;
            default: d.paso.est = M_DATO; d.paso.v = 2.0; break;
            }
            chk_cond("modelo: circ_valido rechaza", R[i].que, !circ_valido(&d));
        }
    }

    /* Sondas: siguen a su elemento al borrar y al girar; las NODO sueltas se quitan */
    {
        int canal, t;
        lazo(&d, &rd, 12.0, 4.0, 8.0);                    /* E1 (0), R1 (1), R2 (2) */
        t = circ_sonda_ciclo(&d, 2, 1, &canal);
        chk_cond("sondas", "la primera va al canal 1, como I", t == SONDA_I && canal == 0 && d.sonda[0].el == 2);
        circ_quitar(&d, 0, 0, DIR_DER);                   /* se va R1 */
        chk_cond("sondas", "borrar un anterior la renumera", d.sonda[0].el == 1 && circ_valido(&d));
        circ_voltear(&d, 1);
        chk_cond("sondas", "girar el elemento invierte su signo", d.sonda[0].sg == -1);
        t = circ_sonda_ciclo(&d, 1, 1, &canal);
        chk_cond("sondas", "otra vez en el mismo: pasa a V", t == SONDA_V && canal == 0);
        t = circ_sonda_ciclo(&d, 1, 1, &canal);
        chk_cond("sondas", "y otra: se quita", t == SONDA_NO && d.sonda[0].tipo == SONDA_NO && circ_valido(&d));
        circ_sonda_ciclo(&d, 1, 1, &canal);
        circ_quitar(&d, 1, 0, DIR_ABA);                   /* se va el de la sonda */
        chk_cond("sondas", "borrar el elemento de la sonda la quita", d.sonda[0].tipo == SONDA_NO && circ_valido(&d));
        t = circ_sonda_pon_nodo(&d, 5, 5, &canal);
        chk_cond("sondas", "una NODO en un punto", t == SONDA_NODO && circ_sonda_nodo(&d, 5, 5) == canal);
        circ_sondas_limpia(&d);
        chk_cond("sondas", "suelta (sin circuito): se quita", circ_sonda_nodo(&d, 5, 5) == -1 && circ_valido(&d));
        circ_sonda_pon_nodo(&d, 0, 0, &canal);
        circ_sonda_pon_nodo(&d, 0, 1, &canal);
        t = circ_sonda_pon_nodo(&d, 1, 1, &canal);
        chk_cond("sondas", "la tercera sustituye al canal 2", t == SONDA_NODO && canal == 1 && d.sonda[1].x == 1);
        t = circ_sonda_pon_nodo(&d, 0, 0, &canal);
        chk_cond("sondas", "otra vez en el mismo punto: se quita", t == SONDA_NO && canal == 0);
    }

    /* circ_dinamico y circ_pide */
    lazo(&d, &rd, 12.0, 4.0, 8.0);
    chk_cond("modelo", "un lazo de CC no es dinámico", !circ_dinamico(&d) && !circ_pide(&d));
    d.el[1].I.est = M_BUSCAR;
    chk_cond("modelo", "con un [?!] pide", circ_pide(&d));
    d.sonda[0].tipo = SONDA_NODO; d.sonda[0].x = 0; d.sonda[0].y = 0;
    chk_cond("modelo", "con una sonda es dinámico", circ_dinamico(&d));
    limpia_mis();
}

/* --------------------------------------------------------------------------
 *  31. La calculadora con C, L y S (en CC: C abierto, L cable, S en su
 *      estado al empezar)
 * ----------------------------------------------------------------------- */
/* E (0,1)->(0,0) de valor E; X de (0,0) a (1,0); Y de (1,0) a (1,1); y en la
 * columna 2 otro elemento Z en paralelo con Y (si z >= 0); cable abajo */
static int red_xyz(Circuito *c, double E, int tx, double vx, int ty, double vy, int tz, double vz)
{
    int f, k;
    circ_init(c, "caso");
    f = circ_elemento(c, EL_FUENTE, 0, 1, DIR_ARR);
    c->el[f].val.est = M_DATO; c->el[f].val.v = E;
    k = circ_elemento(c, (TipoEl)tx, 0, 0, DIR_DER);
    if (tx != EL_INTER) { c->el[k].val.est = M_DATO; c->el[k].val.v = vx; }
    k = circ_elemento(c, (TipoEl)ty, 1, 0, DIR_ABA);
    if (ty != EL_INTER) { c->el[k].val.est = M_DATO; c->el[k].val.v = vy; }
    circ_cable(c, 1, 1, DIR_IZQ);
    if (tz >= 0) {
        circ_cable(c, 1, 0, DIR_DER);
        k = circ_elemento(c, (TipoEl)tz, 2, 0, DIR_ABA);
        if (tz != EL_INTER) { c->el[k].val.est = M_DATO; c->el[k].val.v = vz; }
        circ_cable(c, 2, 1, DIR_IZQ);
    }
    return f;
}

static void test_calc_clS(void)
{
    static Circuito c;
    static Red r;
    static Solucion s;
    char m[260];
    int f, met, kC, kL, kS;

    printf("[31] Calculadora con C, L y S\n");

    /* E = 12, R1 = 4 y R2 = 8 en paralelo con C1 = 10 uF: el C es un abierto */
    f = red_xyz(&c, 12.0, EL_RES, 4.0, EL_RES, 8.0, EL_COND, 10e-6);
    kC = 3;
    c.el[kC].V.est = M_BUSCAR;
    red_extraer(&c, &r);
    met = comprueba("CC con C", &c, &r, &s);
    if (s.estado == SOL_OK) {
        chk("CC con C", "I = 1 A", fabs(s.s[0].I[f]), 1.0);
        chk("CC con C", "V(C1) = 8 V", fabs(s.s[0].V[kC]), 8.0);
        chk("CC con C", "I(C1) = 0", s.s[0].I[kC], 0.0);
        chk_cond("CC con C", "por Kirchhoff, con «abierto en CC»", met == PROC_KIRCHHOFF && texto_en_pasos("abierto en CC"));
        chk_cond("CC con C", "y la energía guardada: Q = C·V y W = 320 uJ",
                 texto_en_pasos("Q = C") && (texto_en_pasos("0,00032 J") || texto_en_pasos("3,2E-4 J")));
    }

    /* E = 12, R1 = 2, L1 = 0,5 H y R2 = 4 en serie: la L es un cable */
    f = red_xyz(&c, 12.0, EL_RES, 2.0, EL_BOB, 0.5, -1, 0.0);
    circ_quitar(&c, 1, 1, DIR_IZQ);
    circ_elemento(&c, EL_RES, 1, 1, DIR_IZQ);
    c.el[3].val.est = M_DATO; c.el[3].val.v = 4.0;
    kL = 2;
    c.el[kL].I.est = M_BUSCAR;
    red_extraer(&c, &r);
    comprueba("CC con L", &c, &r, &s);
    if (s.estado == SOL_OK) {
        chk("CC con L", "I = 2 A", fabs(s.s[0].I[kL]), 2.0);
        chk("CC con L", "V(L1) = 0", s.s[0].V[kL], 0.0);
        chk_cond("CC con L", "flujo y W = 1 J en los pasos", texto_en_pasos(" = L·I") && texto_en_pasos(" 1 J"));
    }

    /* E = 10, R1 = 5, S1 y R2 = 5 en serie: cerrado, 1 A; abierto, 0 A y 10 V */
    f = red_xyz(&c, 10.0, EL_RES, 5.0, EL_INTER, 0.0, -1, 0.0);
    circ_quitar(&c, 1, 1, DIR_IZQ);
    circ_elemento(&c, EL_RES, 1, 1, DIR_IZQ);
    c.el[3].val.est = M_DATO; c.el[3].val.v = 5.0;
    kS = 2;
    c.el[kS].variante = INTER_CERRADO;
    c.el[kS].V.est = M_BUSCAR;
    red_extraer(&c, &r);
    comprueba("CC con S cerrado", &c, &r, &s);
    if (s.estado == SOL_OK) {
        chk("S cerrado", "I = 1 A", fabs(s.s[0].I[f]), 1.0);
        chk("S cerrado", "V(S1) = 0", s.s[0].V[kS], 0.0);
    }
    c.el[kS].variante = INTER_ABIERTO;
    red_extraer(&c, &r);
    resolver(&c, &r, &s);
    if (s.estado == SOL_OK) {
        chk("S abierto", "I = 0", s.s[0].I[f], 0.0);
        chk("S abierto", "V(S1) = 10 V", fabs(s.s[0].V[kS]), 10.0);
    } else { pruebas++; falla("S abierto", "se resuelve", s.estado, SOL_OK); }

    /* Despeje: E = 12, R1 ?, R2 = 6 en paralelo con C1, y V(C1) = 4 V dato */
    f = red_xyz(&c, 12.0, EL_RES, 1.0, EL_RES, 6.0, EL_COND, 1e-6);
    c.el[1].val.est = M_BUSCAR;
    c.el[3].V.est = M_DATO; c.el[3].V.v = 4.0;
    red_extraer(&c, &r);
    comprueba("despeje con C", &c, &r, &s);
    if (s.estado == SOL_OK) chk("despeje con C", "R1 = 12 ohm", s.s[0].val[1], 12.0);
    chk_cond("despeje con C", "una sola incógnita (la C no cuenta)", sol_desconocidos(&c, 0) == 1);

    /* La red: una E ideal con una L o un S cerrado encima es un corto, y se
     * dice quién tiene la culpa */
    f = red_xyz(&c, 12.0, EL_RES, 4.0, EL_RES, 8.0, EL_BOB, 1e-3);
    circ_quitar(&c, 0, 0, DIR_DER);                    /* sin R1: la L sobre la E */
    circ_cable(&c, 0, 0, DIR_DER);
    red_extraer(&c, &r); resolver(&c, &r, &s);
    sol_mensaje(&c, &r, &s, m, sizeof m);
    chk_cond("red con L", "E en paralelo con L: corto, y la culpa es la L",
             r.error == RED_FUENTE_AMP && r.elem_culpa >= 0 && c.el[r.elem_culpa].tipo == EL_BOB && strstr(m, "bobina") != 0);
    c.el[r.elem_culpa].aux.est = M_LIBRE;
    c.el[f].aux.est = M_DATO; c.el[f].aux.v = 2.0;         /* con r = 2: I = E/r = 6 A */
    red_extraer(&c, &r);
    comprueba("E real sobre L", &c, &r, &s);
    if (s.estado == SOL_OK) chk("E real sobre L", "I = E/r = 6 A", fabs(s.s[0].I[f]), 6.0);

    f = red_xyz(&c, 12.0, EL_RES, 4.0, EL_RES, 8.0, EL_INTER, 0.0);
    circ_quitar(&c, 0, 0, DIR_DER);
    circ_cable(&c, 0, 0, DIR_DER);
    kS = c.nel - 1;
    c.el[kS].variante = INTER_CERRADO;
    red_extraer(&c, &r); resolver(&c, &r, &s);
    sol_mensaje(&c, &r, &s, m, sizeof m);
    chk_cond("red con S", "S cerrado sobre la E: corto, y la culpa es el S",
             r.error == RED_FUENTE_AMP && r.elem_culpa == kS && strstr(m, "cerrado al empezar") != 0);
    c.el[kS].variante = INTER_ABIERTO;
    red_extraer(&c, &r);
    chk_cond("red con S", "abierto, no hay corto", r.error == RED_OK);

    /* J en serie con un C: sin camino de vuelta en CC */
    circ_init(&c, "J y C");
    f = circ_elemento(&c, EL_CORR, 0, 1, DIR_ARR);
    c.el[f].val.est = M_DATO; c.el[f].val.v = 2.0;
    circ_cable(&c, 0, 0, DIR_DER);
    kC = circ_elemento(&c, EL_COND, 1, 0, DIR_ABA);
    c.el[kC].val.est = M_DATO; c.el[kC].val.v = 1e-6;
    circ_cable(&c, 1, 1, DIR_IZQ);
    red_extraer(&c, &r); resolver(&c, &r, &s);
    sol_mensaje(&c, &r, &s, m, sizeof m);
    chk_cond("J y C", "J en serie con un C: sin camino, y lo dice", r.error == RED_J_ABIERTA && strstr(m, "una C") != 0);

    /* J = 2 en paralelo con L1 y R = 5: toda la corriente por la L */
    f = red_xyz(&c, 0.0, EL_RES, 1.0, EL_BOB, 1e-3, EL_RES, 5.0);
    circ_quitar(&c, 0, 1, DIR_ARR);
    f = circ_elemento(&c, EL_CORR, 0, 1, DIR_ARR);
    c.el[f].val.est = M_DATO; c.el[f].val.v = 2.0;
    circ_quitar(&c, 0, 0, DIR_DER);
    circ_cable(&c, 0, 0, DIR_DER);
    kL = 0;
    { int k; for (k = 0; k < c.nel; k++) if (c.el[k].tipo == EL_BOB) kL = k; }
    red_extraer(&c, &r);
    comprueba("J con L y R", &c, &r, &s);
    if (s.estado == SOL_OK) chk("J con L y R", "I(L1) = 2 A", fabs(s.s[0].I[kL]), 2.0);

    /* Dos C en serie sobre la E: en CC la carga no está fijada */
    circ_init(&c, "C en serie");
    f = circ_elemento(&c, EL_FUENTE, 0, 1, DIR_ARR);
    c.el[f].val.est = M_DATO; c.el[f].val.v = 12.0;
    kC = circ_elemento(&c, EL_COND, 0, 0, DIR_DER);
    c.el[kC].val.est = M_DATO; c.el[kC].val.v = 1e-6;
    kC = circ_elemento(&c, EL_COND, 1, 0, DIR_ABA);
    c.el[kC].val.est = M_DATO; c.el[kC].val.v = 3e-6;
    circ_cable(&c, 1, 1, DIR_IZQ);
    c.el[kC].V.est = M_BUSCAR;
    red_extraer(&c, &r); resolver(&c, &r, &s);
    sol_mensaje(&c, &r, &s, m, sizeof m);
    chk_cond("C en serie", "SOL_INDET en una C, «su historia»",
             s.estado == SOL_INDET && s.elem >= 0 && c.el[s.elem].tipo == EL_COND && strstr(m, "historia") != 0 &&
             lineas(m, 52) <= 4);

    /* Una fuente senoidal con [?!]: la calculadora es de CC */
    f = red_xyz(&c, 10.0, EL_RES, 4.0, EL_RES, 6.0, -1, 0.0);
    c.el[f].variante = ONDA_SENO;
    c.el[1].I.est = M_BUSCAR;
    red_extraer(&c, &r); resolver(&c, &r, &s);
    sol_mensaje(&c, &r, &s, m, sizeof m);
    chk_cond("forma", "SOL_FORMA, en la fuente, y cabe en 4 líneas",
             s.estado == SOL_FORMA && s.elem == f && strstr(m, "senoidal") != 0 && lineas(m, 52) <= 4);

    /* En la interfaz: poner C, L y S, con sus fichas y su página Modelo */
    {
        const Circuito *cu = ui_circuito();
        int i;
        ui_init();
        TECLAS(T_ENTER, T_ENTER, T_ENTER);
        TECLAS(T_LETRA('E'), T_ARRIBA, T_DIG(1), T_DIG(2), T_ENTER);
        TECLAS(T_LETRA('R'), T_DER, T_DIG(1), T_LETRA('K'), T_ENTER);
        TECLAS(T_LETRA('C'), T_ABAJO);
        vuelca("cls_ficha_c");
        chk_cond("C, L y S", "la ficha del C: C, V0 y V", ui_pantalla() == P_ELEMENTO && ve("V" S_SUB0) && ve("falta"));
        TECLAS(T_INCOG);
        chk_cond("C, L y S", "[?!] en el valor de un C no hace nada y avisa",
                 cu->el[2].val.est == M_LIBRE && ve("abierto: pide su V"));
        TECLAS(T_DIG(1), T_LETRA('U'), T_CATALOGO);
        vuelca("cls_modelo_c");
        chk_cond("C, L y S", "[catálogo]: la página Modelo", ui_pantalla() == P_MODELO && ve("dv/dt"));
        TECLAS(T_ESC);
        chk_cond("C, L y S", "[esc] vuelve a la ficha, con C = 1 uF", ui_pantalla() == P_ELEMENTO &&
                 cu->el[2].val.est == M_DATO && fabs(cu->el[2].val.v - 1e-6) < 1e-18);
        TECLAS(T_ENTER);
        TECLAS(T_LETRA('W'), T_IZQ, T_ESC);
        TECLAS(T_LETRA('S'), T_DER);
        vuelca("cls_ficha_s");
        chk_cond("C, L y S", "un S nuevo: abierto, t1 = 0", ui_pantalla() == P_ELEMENTO &&
                 cu->el[3].variante == INTER_ABIERTO && cu->el[3].par[Q_T1 - Q_P1].est == M_DATO && ve("abierto al empezar"));
        TECLAS(T_DER);
        chk_cond("C, L y S", "[der] lo deja cerrado al empezar", cu->el[3].variante == INTER_CERRADO && ve("cerrado al empezar"));
        TECLAS(T_ENTER);
        TECLAS(T_LETRA('L'), T_DER);
        vuelca("cls_ficha_l");
        chk_cond("C, L y S", "la ficha de la L", ui_pantalla() == P_ELEMENTO && cu->el[4].tipo == EL_BOB && ve("I" S_SUB0));
        TECLAS(T_DIG(1), T_DIG(0), T_LETRA('M'), T_ENTER);
        vuelca("cls_editor");
        TECLAS(T_CATALOGO);
        vuelca("cls_paleta");
        chk_cond("C, L y S", "la paleta de 4x3 con C, L, S y la sonda", ve("Condensador") && ve("Interruptor") && ve("Sonda"));
        TECLAS(T_ESC);
        for (i = 0; i < 4 && ui_pantalla() == P_EDITOR; i++) TECLAS(T_ESC);
    }
}

/* --------------------------------------------------------------------------
 *  32. Formas de onda en la ficha (E y J): circ_onda, reposo, límites
 * ----------------------------------------------------------------------- */
static void pon_par(Elemento *e, int q, double v) { e->par[q - Q_P1].v = v; e->par[q - Q_P1].est = M_DATO; }

static void test_formas(void)
{
    static Circuito c;
    static Red r;
    double p[ONDA_PAR], pl[ONDA_PAR] = { 1.0, 1.0, 30.0, 0.0, 0.0 };
    int forma, q, ok;

    printf("[32] Formas de onda de E y J\n");
    lazo(&c, &r, 10.0, 4.0, 6.0);

    /* Cuadrada de V1 = 2 a V2 = 10, 1 kHz, D = 30 % */
    c.el[0].variante = ONDA_CUADRADA;
    pon_par(&c.el[0], Q_BASE, 2.0); pon_par(&c.el[0], Q_FREC, 1e3); pon_par(&c.el[0], Q_CICLO, 30.0);
    ok = circ_onda(&c.el[0], &forma, p, &q);
    chk_cond("formas", "cuadrada: circ_onda completa y válida", ok && forma == ONDA_CUADRADA && circ_valido(&c));
    chk("formas", "cuadrada: A = (V2 - V1)/2 = 4", p[0], 4.0);
    chk("formas", "cuadrada: nivel = (V1 + V2)/2 = 6", p[3], 6.0);
    chk("formas", "cuadrada: 10 en 0", onda_valor(forma, p, 0.0), 10.0);
    chk("formas", "cuadrada: 10 en 0,299 ms", onda_valor(forma, p, 0.299e-3), 10.0);
    chk("formas", "cuadrada: 2 en 0,3 ms", onda_valor(forma, p, 0.3e-3), 2.0);
    chk("formas", "cuadrada: 10 en 1 ms", onda_valor(forma, p, 1e-3), 10.0);
    chk("formas", "cuadrada: reposo = V1 = 2", onda_reposo(forma, p), 2.0);

    /* Triangular de 0 a 1 (D por omisión: 50 %) */
    c.el[0].variante = ONDA_TRIANGULAR;
    c.el[0].val.v = 1.0;
    c.el[0].par[Q_BASE - Q_P1].est = M_LIBRE; c.el[0].par[Q_CICLO - Q_P1].est = M_LIBRE;
    ok = circ_onda(&c.el[0], &forma, p, &q);
    chk("formas", "triangular: 0 en 0", onda_valor(forma, p, 0.0), 0.0);
    chk("formas", "triangular: 1 en T/2", onda_valor(forma, p, 0.5e-3), 1.0);

    /* Pulso: V1 = 0, V2 = 5, t0 = 1 s, sin ancho: escalón */
    c.el[0].variante = ONDA_PULSO;
    c.el[0].val.v = 5.0;
    pon_par(&c.el[0], Q_T0, 1.0);
    ok = circ_onda(&c.el[0], &forma, p, &q);
    chk_cond("formas", "pulso: circ_onda completa", ok && forma == ONDA_PULSO);
    chk("formas", "escalón: 5 en 100 s", onda_valor(forma, p, 100.0), 5.0);
    chk("formas", "escalón: 0 antes de t0", onda_valor(forma, p, 0.5), 0.0);

    /* Senoidal sin f: falta la f */
    c.el[0].variante = ONDA_SENO;
    c.el[0].par[Q_FREC - Q_P1].est = M_LIBRE;
    ok = circ_onda(&c.el[0], &forma, p, &q);
    chk_cond("formas", "senoidal sin f: falta la f", !ok && q == Q_FREC);
    pon_par(&c.el[0], Q_FREC, 50.0); pon_par(&c.el[0], Q_BASE, 1.0);
    ok = circ_onda(&c.el[0], &forma, p, &q);
    chk("formas", "senoidal con CC 1: reposo 1", onda_reposo(forma, p), 1.0);

    /* Por la izquierda */
    chk("formas", "izq: en el flanco, el valor de antes", onda_valor_lado(ONDA_CUADRADA, pl, 0.3, -1), 1.0);
    chk("formas", "izq: al final del periodo, el nivel bajo", onda_valor_lado(ONDA_CUADRADA, pl, 1.0, -1), -1.0);
    chk("formas", "izq: en 0, el reposo", onda_valor_lado(ONDA_CUADRADA, pl, 0.0, -1), -1.0);
    chk("formas", "der: lo de siempre", onda_valor_lado(ONDA_CUADRADA, pl, 0.3, 1), -1.0);

    /* circ_valido: rangos de los parámetros */
    lazo(&c, &r, 10.0, 4.0, 6.0);
    c.el[0].variante = ONDA_CUADRADA; pon_par(&c.el[0], Q_FREC, 1e3);
    chk_cond("formas", "válida con f = 1 kHz", circ_valido(&c));
    pon_par(&c.el[0], Q_CICLO, 101.0);
    chk_cond("formas", "D = 101 % no es válido", !circ_valido(&c));
    c.el[0].par[Q_CICLO - Q_P1].est = M_LIBRE;
    pon_par(&c.el[0], Q_FASE, 400.0);
    chk_cond("formas", "fase de 400° no es válida", !circ_valido(&c));
    c.el[0].par[Q_FASE - Q_P1].est = M_LIBRE;
    pon_par(&c.el[0], Q_FREC, 0.0);
    chk_cond("formas", "f = 0 no es válida", !circ_valido(&c));

    /* En la interfaz */
    {
        const Circuito *cu = ui_circuito();
        int i;
        ui_init();
        TECLAS(T_ENTER, T_ENTER, T_ENTER);
        TECLAS(T_LETRA('E'), T_ARRIBA, T_DIG(1), T_DIG(0), T_DER);
        vuelca("formas_seno");
        chk_cond("formas", "[der]: senoidal, Vp = 10 y f = 1 kHz",
                 cu->el[0].variante == ONDA_SENO && ve("senoidal") && cu->el[0].val.v == 10.0 &&
                 cu->el[0].par[Q_FREC - Q_P1].est == M_DATO && cu->el[0].par[Q_FREC - Q_P1].v == 1000.0);
        chk_cond("formas", "V e I quedan libres", cu->el[0].V.est == M_LIBRE && cu->el[0].I.est == M_LIBRE);
        TECLAS(T_ABAJO, T_ABAJO, T_ABAJO);
        vuelca("formas_seno_fase");
        TECLAS(T_SIGNO, T_DIG(3), T_DIG(0), T_CATALOGO);
        vuelca("formas_modelo");
        chk_cond("formas", "fase -30° y la página Modelo con su ecuación",
                 cu->el[0].par[Q_FASE - Q_P1].est == M_DATO && cu->el[0].par[Q_FASE - Q_P1].v == -30.0 &&
                 ui_pantalla() == P_MODELO && ve("sen(2"));
        TECLAS(T_ESC, T_DER);
        chk_cond("formas", "[der]: cuadrada", cu->el[0].variante == ONDA_CUADRADA && ve("cuadrada"));
        TECLAS(T_ABAJO, T_ABAJO, T_ABAJO, T_DIG(1), T_DIG(0), T_DIG(1), T_ENTER);
        vuelca("formas_ciclo_101");
        chk_cond("formas", "D = 101: avisa y no cambia", ui_pantalla() == P_ELEMENTO && ve("Fuera de rango") &&
                 cu->el[0].par[Q_CICLO - Q_P1].est == M_LIBRE);
        TECLAS(T_BORRAR, T_BORRAR, T_BORRAR, T_DIG(2), T_DIG(5), T_ENTER);
        chk("formas", "D = 25", cu->el[0].par[Q_CICLO - Q_P1].v, 25.0);
        vuelca("formas_editor_cuadrada");
        TECLAS(T_ENTER, T_IZQ, T_IZQ);
        chk_cond("formas", "[izq][izq]: vuelve a continua con E, I, r y V", cu->el[0].variante == ONDA_CC && ve("continua"));
        TECLAS(T_ENTER);
        for (i = 0; i < 4 && ui_pantalla() == P_EDITOR; i++) TECLAS(T_ESC);
    }
}

/* --------------------------------------------------------------------------
 *  15. Fracciones exactas en los resultados
 * ----------------------------------------------------------------------- */
static void test_fracciones(void)
{
    static const struct { double v; long p, q; } CASOS[] = {
        { 440.0 / 7.0, 440, 7 }, { 220.0 / 3.0, 220, 3 }, { 12.0 / 17.0, 12, 17 },
        { 5.0 / 2200.0, 1, 440 }, { 2.0 / 3.0, 2, 3 }, { 1.0 / 999.0, 1, 999 },
        { -55.0 / 3.0, -55, 3 }, { 999998.0 / 7.0, 999998, 7 },
        { 2e6 / 3.0, 0, 0 },                     /* numerador largo: cabe en 32 bits */
        { 0.75, 0, 0 }, { 15.75, 0, 0 }, { 4.0, 0, 0 }, { 3.14159265358979, 0, 0 },
        { 0.001, 0, 0 }, { 1.0 / 1001.0, 0, 0 }, { 2e9 / 3.0, 0, 0 }, { 0.0, 0, 0 },
    };
    int i;
    printf("[15] Fracciones exactas\n");
    for (i = 0; i < (int)(sizeof CASOS / sizeof CASOS[0]); i++) {
        long p = 0, q = 0;
        int hay = num_fraccion(CASOS[i].v, &p, &q, 999);
        char d[64];
        sprintf(d, "%.10g -> %ld/%ld", CASOS[i].v, CASOS[i].p, CASOS[i].q);
        if (CASOS[i].q) chk_cond("fracción", d, hay && p == CASOS[i].p && q == CASOS[i].q);
        else chk_cond("fracción", d, !hay);
    }
}

/* --------------------------------------------------------------------------
 *  11. Un circuito sano: lo que ninguna secuencia de teclas puede romper
 * ----------------------------------------------------------------------- */
static int seg_de(const Circuito *c, int xa, int ya, int xb, int yb)
{
    if (ya == yb) return c->h[ya][xa < xb ? xa : xb];
    return c->v[ya < yb ? ya : yb][xa];
}

static const char *circuito_sano(const Circuito *c)
{
    int k, j, x, y;
    if (c->nel < 0 || c->nel > MAX_EL) return "nel fuera de rango";
    for (y = 0; y < REJ_FILAS; y++)
        for (x = 0; x < REJ_COLS; x++) {
            int v[2], i;
            v[0] = (x < REJ_COLS - 1) ? c->h[y][x] : 0;
            v[1] = (y < REJ_FILAS - 1) ? c->v[y][x] : 0;
            for (i = 0; i < 2; i++) {
                const Elemento *e;
                if (v[i] < SEG_EL0) continue;
                if (v[i] - SEG_EL0 >= c->nel) return "segmento apunta a un elemento que no existe";
                e = &c->el[v[i] - SEG_EL0];
                if (circ_npatas(e) == 3) {                 /* un segmento de su caja */
                    int x0, y0, x1, y1;
                    circ_caja(e, &x0, &y0, &x1, &y1);
                    if (x < x0 || y < y0 || (i == 0 && (x + 1 > x1 || y > y1)) || (i == 1 && (y + 1 > y1 || x > x1)))
                        return "segmento de una caja fuera de su caja";
                    continue;
                }
                if (i == 0 && !((e->ya == y && e->yb == y) && ((e->xa == x && e->xb == x + 1) || (e->xb == x && e->xa == x + 1))))
                    return "segmento horizontal y elemento no coinciden";
                if (i == 1 && !((e->xa == x && e->xb == x) && ((e->ya == y && e->yb == y + 1) || (e->yb == y && e->ya == y + 1))))
                    return "segmento vertical y elemento no coinciden";
            }
        }
    for (k = 0; k < c->nel; k++) {
        const Elemento *e = &c->el[k];
        int dx = e->xa - e->xb, dy = e->ya - e->yb;
        if (e->tipo >= EL_N) return "tipo de elemento inválido";
        if (e->ref < -1 || e->ref >= c->nel || e->ref == k) return "referencia a un elemento que no existe";
        if (e->xa >= REJ_COLS || e->xb >= REJ_COLS || e->ya >= REJ_FILAS || e->yb >= REJ_FILAS) return "extremo fuera de la rejilla";
        if (circ_npatas(e) == 3) {
            int dd, esp, ox, oy;
            if (!circ_orienta(e, &dd, &esp, &ox, &oy)) return "las patas no forman una huella";
        } else {
            if (e->xc != 0 || e->yc != 0) return "pata c en uno de dos bornes";
            if (dx * dx + dy * dy != 1) return "extremos no vecinos";
            if (seg_de(c, e->xa, e->ya, e->xb, e->yb) != SEG_EL0 + k) return "el elemento no está en su segmento";
        }
        if (e->val.est > M_BUSCAR || e->V.est > M_BUSCAR || e->I.est > M_BUSCAR || e->aux.est > M_BUSCAR)
            return "estado de magnitud inválido";
        if (!e->etiq[0]) return "elemento sin nombre";
        for (j = 0; j < k; j++) if (strcmp(e->etiq, c->el[j].etiq) == 0) return "nombre repetido";
    }
    if (!circ_valido(c)) return "circ_valido lo rechaza";
    return 0;
}

/* --------------------------------------------------------------------------
 *  12. El mono: cientos de miles de teclas al azar por toda la aplicación.
 *  No se busca un resultado concreto, sino que nada se rompa: ni un acceso
 *  fuera de sitio (lo caza AddressSanitizer), ni un circuito corrupto, ni
 *  una pantalla que no existe, ni un texto que se salga, ni una solución de
 *  Newton que no cumpla Kirchhoff (en toda la sesión: [12a]).
 *
 *  Dos fases. La primera, n teclas a ui_tecla (la de siempre: la misma
 *  semilla da las mismas teclas que antes). La segunda, n/4 más (o
 *  HAKARU_MONO_CAPA; 0 = sin ella) con lo que manda la capa de entrada: una
 *  tecla que se queda abajo (su ráfaga de T_REPE), el dedo que se desliza
 *  por el touchpad (pasos T_TACTIL | T_REPE) y el T_SUELTA que cierra cada
 *  una, con las guardas de entrada.c (ui_repite antes de cada repetición y
 *  de cada paso, ui_tactil y sus «uno», la pantalla de su primer evento y
 *  los ajustes de Controles).
 *
 *  Corre en su propia carpeta, vacía (out/guardados/mono): el recorrido
 *  depende de lo que haya en Mis problemas y de los récords, y out/guardados
 *  guarda lo de otras corridas. Así la semilla sola lo repite.
 * ----------------------------------------------------------------------- */
static char mono_dir[300];

static void mono_carpeta(void)
{
    static const char *const FIJOS[] = { "controles", "muestras", ESP_T_FIJOS "sesion" };
    char ruta[340];
    int h;
    sprintf(mono_dir, "%s/mono", carpeta_mis);
    mkdir(mono_dir, 0755);
    for (h = 1; h <= ARCH_HUECOS; h++) { sprintf(ruta, "%s/ces_%02d.tns", mono_dir, h); remove(ruta); }
    for (h = 0; h < (int)(sizeof FIJOS / sizeof FIJOS[0]); h++) { sprintf(ruta, "%s/ces_%s.tns", mono_dir, FIJOS[h]); remove(ruta); }
    arch_carpeta(mono_dir);
}

/* Las pantallas con alguna tecla (un arreglo: aguanta más de 32 pantallas) */
static int mono_visitadas(const long *cuenta, int n)
{
    int p, v = 0;
    for (p = 0; p < n; p++) if (cuenta[p] > 0) v++;
    return v;
}

typedef struct {
    long cuenta[P_N];                   /* teclas por pantalla                  */
    int  salidas, maxel;
    long rafagas, repes, gestos, pasos, sueltas;   /* la segunda fase         */
} Mono;

/* n teclas al azar; con capa, también las ráfagas y los gestos de la capa de
 * entrada. 0 si algo se rompió (ya dicho con falla). */
static int mono_fase(long n, int capa, Mono *m)
{
    static const int T[] = {
        T_ARRIBA, T_ABAJO, T_IZQ, T_DER, T_ARRIBA, T_ABAJO, T_IZQ, T_DER,
        T_ARRIBA, T_ABAJO, T_IZQ, T_DER, T_ENTER, T_ENTER, T_ENTER, T_ESC,
        T_TAB, T_BORRAR, T_MENU, T_PUNTO, T_SIGNO, T_MAS, T_MENOS, T_IGUAL,
        T_INCOG, T_EE, T_DIG(0), T_DIG(1), T_DIG(2), T_DIG(3), T_DIG(4),
        T_DIG(5), T_DIG(6), T_DIG(7), T_DIG(8), T_DIG(9), T_DIG(6), T_DIG(7),
        T_LETRA('R'), T_LETRA('E'), T_LETRA('W'), T_LETRA('R'), T_LETRA('W'),
        T_LETRA('F'), T_LETRA('K'), T_LETRA('M'), T_LETRA('X'), T_LETRA('Z'),
        T_LETRA('W'), T_LETRA('U'), T_LETRA('N'), T_LETRA('P'), T_LETRA('G'), T_LETRA('C'),
        T_SHIFT, T_CTRL, T_CATALOGO, T_LETRA('Z') | T_CON_CTRL, T_LETRA('M') | T_CON_SHIFT,
        T_LETRA('J'), T_LETRA('J'), T_LETRA('V'), T_LETRA('A'), T_LETRA('G'), T_LETRA('T'),
        T_LETRA('T'), T_LETRA('A'), T_LETRA('Q'), T_INCOG, T_INCOG,
        T_LETRA('C'), T_LETRA('L'), T_LETRA('S'), T_LETRA('O'), T_LETRA('D'), T_CATALOGO,
        T_LETRA('B'), T_LETRA('T'), T_IZQ, T_IGUAL,
        T_LETRA('I'), T_LETRA('Y'),
        /* [72]: las cuentas de la ficha, copiar y pegar y los nombres */
        T_POR, T_ENTRE, T_POT, T_CUAD, T_PAR_A, T_PAR_C, T_ESPACIO,
        T_LETRA('C') | T_CON_CTRL, T_LETRA('X') | T_CON_CTRL, T_LETRA('V') | T_CON_CTRL, T_LETRA('N') | T_CON_CTRL,
        T_NADA
    };
    const int NT = (int)(sizeof T / sizeof T[0]);
    /* La capa: la tecla que sigue abajo (rf_n repeticiones más; rf_rep las
     * dadas; rf_pant, la pantalla de su primer evento), el dedo que sigue en
     * el pad (g_n pasos más, desde la pantalla g_pant) y un T_SUELTA debido */
    int rf_t = 0, rf_n = 0, rf_rep = 0, rf_pant = -1, g_t = 0, g_n = 0, g_pant = -1, suelta = 0;
    long i;

    memset(m, 0, sizeof *m);
    for (i = 0; i < n; i++) {
        int t, p;
        const char *mal;
        /* El azar casi nunca arma un circuito de CA que se resuelva: en el
         * último 5 %, cada 150 teclas se abre un modelo de la lección 11 (o
         * de la 12 o la 13, por turno) o el de Thévenin (y, en la versión
         * especial, sus pantallas) */
        if (i >= n - n / 20 && (i - (n - n / 20)) % 150 == 0) {
            ui_init();
            rf_n = g_n = suelta = 0;
            ESP_T_MONO(i, n)
            if (((i - (n - n / 20)) / 150) % 8 == 5) {     /* el modelo de Thévenin y su [T] */
                TECLAS(T_ENTER, T_ABAJO, T_ENTER);
                repite(T_ABAJO, 10);
                TECLAS(T_ENTER, T_DER, T_DER);
                repite(T_ABAJO, 9);
                TECLAS(T_ENTER, T_LETRA('T'));
            }
            else {                          /* un modelo de las lecciones 11, 12 y 13, por turno */
                TECLAS(T_ENTER, T_ABAJO, T_ENTER);
                repite(T_ABAJO, 10 + (int)(((i - (n - n / 20)) / 150) % 3));
                TECLAS(T_ENTER, T_DER, T_DER);
                repite(T_ABAJO, (int)azar(10));
                TECLAS(T_ENTER);
            }
        }
        if (!capa) t = T[azar((unsigned)NT)];
        else if (suelta) { t = T_SUELTA; suelta = 0; m->sueltas++; }
        else {
            UiRepe u;
            t = T_NADA;
            u.tope = 1;
            if (rf_n > 0) {                     /* la tecla sigue abajo */
                if (ui_pantalla() == rf_pant && ui_repite(rf_t | T_REPE, &u)) {
                    t = rf_t | T_REPE; rf_rep++; m->repes++;
                    if (--rf_n == 0) suelta = 1;               /* se suelta: cierra su ráfaga */
                } else {                        /* se apaga (el borde, otro modo, otra pantalla) */
                    rf_n = 0;
                    if (rf_rep) { t = T_SUELTA; m->sueltas++; }
                }
            } else if (g_n > 0) {               /* el dedo sigue */
                if (ui_pantalla() == g_pant && ui_repite(g_t, &u)) { t = g_t; m->pasos++; g_n--; }
                else g_n = 0;                   /* el borde, u otra pantalla */
                if (g_n == 0) suelta = 1;       /* al levantarlo */
            }
            if (t == T_NADA) {                  /* una tecla nueva, o el dedo */
                EntAjustes a;
                unsigned r = azar(16);
                ent_ajustes(&a);
                t = T[azar((unsigned)NT)];
                if (r < 3 && t != T_NADA && t != T_SHIFT && t != T_CTRL && a.repetir != REPETIR_NO) {
                    rf_t = t; rf_n = 1 + (int)azar(12); rf_rep = 0; rf_pant = ui_pantalla();
                    m->rafagas++;
                } else if (r == 3) {            /* se apoya el dedo y se desliza */
                    UiTactil tp;
                    t = T_NADA;
                    if (a.deslizar && ui_tactil(&tp) && (tp.ejes & (TAC_X | TAC_Y))) {
                        int ex = (tp.ejes & TAC_X) && (!(tp.ejes & TAC_Y) || azar(2)), sg = azar(2) ? 1 : -1;
                        int md = (int)azar(6);
                        g_t = (ex ? (sg > 0 ? T_DER : T_IZQ) : (sg > 0 ? T_ARRIBA : T_ABAJO)) | T_TACTIL | T_REPE |
                              (md == 4 ? T_CON_SHIFT : md == 5 ? T_CON_CTRL : 0);
                        g_n = (tp.uno & (ex ? TAC_X : TAC_Y)) ? 1 : 1 + (int)azar(10);
                        g_pant = ui_pantalla();
                        m->gestos++;
                        if (ui_repite(g_t, &u)) { t = g_t; m->pasos++; g_n--; }
                        else g_n = 0;
                        if (g_n == 0) suelta = 1;
                    }
                }
            }
        }
        if (!ui_tecla(t)) { m->salidas++; ui_init(); rf_n = g_n = suelta = 0; }
        if (ui_animando()) { int k = (int)azar(4); while (k--) ui_tick(); }
        p = ui_pantalla();
        if (p < 0 || p >= P_N) { falla("mono", "pantalla inexistente", p, 0); return 0; }
        m->cuenta[p]++;
        if (i % 5 == 0) ui_dibuja();
        mal = circuito_sano(ui_circuito());
        if (mal) { falla("mono", mal, (double)i, 0); return 0; }
        if (ui_circuito()->nel > m->maxel) m->maxel = ui_circuito()->nel;
    }
    return 1;
}

/* rev10: la fase de los capacitores equivalentes (la v3 en rev12). Las dos
 * de siempre no la ven (ui_cpm_oculto, que main pone: la 4.ª fila del
 * Taller y la sección de la Ayuda no cuentan para sus teclas), así que su
 * camino, sus emulaciones y sus huellas son las de base10. Esta va ANTES de
 * ellas, con todo a la vista, su azar propio, y deja como estaban el azar
 * del arnés y la semilla de los juegos (ui_init, lo demás). Solo en sus
 * siete pantallas (el menú, los datos, el resultado, el dibujo, la Teoría y
 * su página, y los Ejemplos): si una tecla sale de ellas (al Taller, al menú
 * de la app, a Controles), se entra otra vez desde la portada; así no emula
 * ni escribe archivos. Con teclas sueltas, ráfagas (T_REPE mientras
 * ui_repite lo diga) y pasos del dedo (ui_tactil). Cuenta además la pestaña
 * Paso a paso, los dibujos de tres y las ediciones con el cursor en medio
 * del texto */
static long mono_cpm_paso, mono_cpm_tres, mono_cpm_medio;
static int mono_es_cpm(int p)
{
    return p == P_CPM || p == P_CPM_RES || p == P_CPM_DIB || p == P_CPM_MENU || p == P_CPM_TEO || p == P_CPM_PAG ||
           p == P_CPM_EJ;
}
static int mono_cpm_pantallas(const Mono *m)
{
    return (m->cuenta[P_CPM] > 0) + (m->cuenta[P_CPM_RES] > 0) + (m->cuenta[P_CPM_DIB] > 0) + (m->cuenta[P_CPM_MENU] > 0) +
           (m->cuenta[P_CPM_TEO] > 0) + (m->cuenta[P_CPM_PAG] > 0) + (m->cuenta[P_CPM_EJ] > 0);
}

static void mono_cpm_entra(void)
{
    static const int K[] = { T_ENTER, T_ENTER, T_ABAJO, T_ABAJO, T_ABAJO, T_ENTER };
    int i;
    ui_init();
    for (i = 0; i < (int)(sizeof K / sizeof K[0]); i++) ui_tecla(K[i]);
}

static int mono_cpm(long n, Mono *m)
{
    static const int T[] = {
        T_ARRIBA, T_ABAJO, T_IZQ, T_DER, T_ARRIBA, T_ABAJO, T_IZQ, T_DER, T_ENTER, T_ENTER, T_ENTER, T_ESC,
        T_TAB, T_BORRAR, T_BORRAR, T_BORRAR | T_CON_CTRL, T_PUNTO, T_ESPACIO, T_ESPACIO, T_MENOS, T_SIGNO,
        T_IGUAL, T_DIG(0), T_DIG(1), T_DIG(2), T_DIG(3), T_DIG(4), T_DIG(5), T_DIG(6), T_DIG(7), T_DIG(8),
        T_DIG(9), T_DIG(1), T_DIG(2), T_LETRA('A'), T_LETRA('B'), T_LETRA('S'), T_LETRA('I'), T_LETRA('N'),
        T_LETRA('X'), T_MENU, T_MENU | T_CON_CTRL, T_SHIFT, T_CTRL, T_ARRIBA | T_CON_SHIFT, T_ABAJO | T_CON_CTRL,
        T_IZQ | T_CON_CTRL, T_DER | T_CON_CTRL, T_NADA,
        /* la v3: el cursor de valor en valor, las pestañas, las páginas */
        T_IZQ | T_CON_SHIFT, T_DER | T_CON_SHIFT, T_ABAJO | T_CON_SHIFT, T_IZQ, T_DER, T_TAB, T_ESC
    };
    const int NT = (int)(sizeof T / sizeof T[0]);
    unsigned s0 = semilla, v0 = ui_vueltas();
    long i;
    int ok = 1, oc = ui_cpm_oculto_esta();
    ui_cpm_oculto(0);
    semilla = s0 * 7u + 2026u;                         /* el suyo, de la semilla del mono */
    memset(m, 0, sizeof *m);
    mono_cpm_paso = mono_cpm_tres = mono_cpm_medio = 0;
    mono_cpm_entra();
    for (i = 0; i < n && ok; i++) {
        int t = T[azar((unsigned)NT)], p, r = (int)azar(10), k, campo = -1, pos = 0;
        char antes[CPM_TXT + 1];
        UiRepe u;
        UiTactil tp;
        if (ui_pantalla() == P_CPM && ui_indice(UI_I_CPM_CAMPO) < 3) {   /* ¿el cursor en medio del texto? */
            campo = ui_indice(UI_I_CPM_CAMPO);
            pos = ui_indice(UI_I_CPM_POS);
            strcpy(antes, ui_cpm_texto(campo));
            if (!(pos > 0 && pos < (int)strlen(antes) && !ui_indice(UI_I_CPM_NUEVO))) campo = -1;
        }
        if (r == 0 && ui_tactil(&tp) && tp.ejes) {        /* el dedo: unos pasos y se levanta */
            int ex = (tp.ejes & TAC_X) && (!(tp.ejes & TAC_Y) || azar(2)), sg = azar(2) ? 1 : -1;
            t = (ex ? (sg > 0 ? T_DER : T_IZQ) : (sg > 0 ? T_ARRIBA : T_ABAJO)) | T_TACTIL | T_REPE;
            for (k = 1 + (int)azar(6); k > 0 && ui_repite(t, &u); k--) { ui_tecla(t); m->pasos++; }
            t = T_SUELTA;
        }
        if (!ui_tecla(t)) m->salidas++;
        if (r == 1 && t != T_SUELTA && t != T_SHIFT && t != T_CTRL && t != T_NADA) {   /* la tecla sigue abajo */
            for (k = 1 + (int)azar(8); k > 0 && ui_repite(t | T_REPE, &u); k--) { ui_tecla(t | T_REPE); m->repes++; }
            ui_tecla(T_SUELTA);
        }
        p = ui_pantalla();
        if (p < 0 || p >= P_N) { falla("mono", "pantalla inexistente (capacitores equivalentes)", p, 0); ok = 0; break; }
        m->cuenta[p]++;
        if (p == P_CPM_RES && ui_indice(UI_I_CPM_TAB) == 1) mono_cpm_paso++;
        if (p == P_CPM_DIB && ui_cpm()->nval > 0 && cpm_ncap(ui_cpm()->val[ui_indice(UI_I_CPM_SEL)].tipo) == 3) mono_cpm_tres++;
        if (campo >= 0 && p == P_CPM && strcmp(antes, ui_cpm_texto(campo)) != 0) mono_cpm_medio++;
        if (i % 5 == 0) ui_dibuja();
        if (!mono_es_cpm(p)) mono_cpm_entra();
    }
    semilla = s0;
    ui_semilla(v0);
    ui_cpm_oculto(oc);
    return ok;
}

static void mono_informe(const Mono *m)
{
    int i;
    if (getenv("HAKARU_DEPURA")) {
        printf("    teclas por pantalla:");
        for (i = 0; i < P_N; i++) printf(" %d:%ld", i, m->cuenta[i]);
        printf("\n");
    }
    if (mono_visitadas(m->cuenta, P_N) < P_N) {
        printf("    sin visitar:");
        for (i = 0; i < P_N; i++) if (m->cuenta[i] == 0) printf(" %d", i);
        printf("\n");
    }
}

static void test_mono(void)
{
    static Mono m1, m2, m3;
    const char *env = getenv("HAKARU_MONO"), *env2 = getenv("HAKARU_MONO_CAPA");
    long n = env ? atol(env) : 150000, n2 = env2 ? atol(env2) : n / 4;
    int ok, visitadas, i, ok3;

    printf("[12] El mono: %ld teclas al azar\n", n);
    gfx_desbordes_reset();
    sim_tope_pasos(2000);
    gfx_desbordes_silencio(getenv("HAKARU_DEPURA") ? 0 : 1);
    mono_carpeta();
    semilla = getenv("HAKARU_SEMILLA") ? (unsigned)atol(getenv("HAKARU_SEMILLA")) : 777u;
    ok3 = mono_cpm(n / 10, &m3);        /* rev10: antes de las de siempre, que no lo ven */
    printf("    antes, %ld en los capacitores equivalentes: %d de sus 7 pantallas, %ld repeticiones, %ld pasos del dedo;"
           " Paso a paso %ld, dibujos de tres %ld, ediciones con el cursor en medio %ld\n",
           n / 10, mono_cpm_pantallas(&m3), m3.repes, m3.pasos, mono_cpm_paso, mono_cpm_tres, mono_cpm_medio);
    sim_residuo_acum_reset();           /* el mono hace de fuzz del residuo de Newton */
    ui_init();
    ok = mono_fase(n, 0, &m1) && ok3;
    for (i = 0; i < P_N; i++) m1.cuenta[i] += m3.cuenta[i];   /* las pantallas: las de las dos fases */
    visitadas = mono_visitadas(m1.cuenta, P_N);
    printf("    %d de %d pantallas visitadas, %d salidas del programa, circuitos de hasta %d elementos\n",
           visitadas, (int)P_N, m1.salidas, m1.maxel);
    mono_informe(&m1);
    if (ok && n2 > 0) {
        ui_init();
        ok = mono_fase(n2, 1, &m2);
        printf("    y %ld con la capa: %ld r" S_aa "fagas (%ld repeticiones), %ld gestos (%ld pasos del dedo),"
               " %ld T_SUELTA; %d de %d pantallas\n", n2, m2.rafagas, m2.repes, m2.gestos, m2.pasos, m2.sueltas,
               mono_visitadas(m2.cuenta, P_N), (int)P_N);
        if (getenv("HAKARU_DEPURA")) mono_informe(&m2);
        pruebas += 3;
        if (ok && !(m2.repes > 0)) falla("mono", "la fase de la capa manda repeticiones (T_REPE)", (double)m2.repes, 1);
        if (ok && !(m2.pasos > 0)) falla("mono", "la fase de la capa manda pasos del dedo (T_TACTIL)", (double)m2.pasos, 1);
        if (ok && !(m2.sueltas > 0)) falla("mono", "la fase de la capa cierra sus ráfagas (T_SUELTA)", (double)m2.sueltas, 1);
    }
    gfx_desbordes_silencio(0);
    sim_tope_pasos(0);
    arch_carpeta(carpeta_mis);
    pruebas += 4;
    if (visitadas < P_N) falla("mono", "no llegó a todas las pantallas", visitadas, P_N);
    chk_cond("mono", "la fase de los capacitores equivalentes llega a sus siete pantallas, con repeticiones y pasos del dedo",
             mono_cpm_pantallas(&m3) == 7 && m3.repes > 0 && m3.pasos > 0);
    chk_cond("mono", "...a la pestaña Paso a paso, a los dibujos de tres y a la edición con el cursor en medio",
             mono_cpm_paso > 0 && mono_cpm_tres > 0 && mono_cpm_medio > 0);
    if (!(sim_residuo_acum() <= 1e-9)) falla("mono", "el residuo de Kirchhoff de alguna solución de Newton", sim_residuo_acum(), 1e-9);
    if (gfx_desbordes() != 0) falla("mono", "textos que se salen de la pantalla", gfx_desbordes(), 0);

    /* Lo que el mono da por hecho: su carpeta empieza vacía aunque haya
     * guardados de otra corrida, y cuenta más de 32 pantallas */
    {
        static long c40[40];
        static Circuito c;
        unsigned char b[8] = { 'C', 'E', 'S', 'K', 1, 1, 2, 1 };
        int h, hay = 0;
        arch_carpeta(mono_dir);
        ejemplo_cargar(0, &c);
        arch_guardar(0, &c); arch_guardar(ARCH_HUECOS - 1, &c);
        arch_bytes_guardar("/ces_controles.tns", b, 8);
        arch_bytes_guardar("/ces_muestras.tns", b, 8);
        mono_carpeta();
        for (h = 0; h < ARCH_HUECOS; h++) hay += arch_existe(h);
        hay += arch_bytes_leer("/ces_controles.tns", b, 8) >= 0;
        hay += arch_bytes_leer("/ces_muestras.tns", b, 8) >= 0;
        arch_carpeta(carpeta_mis);
        chk_cond("mono", "su carpeta empieza vac" S_ii "a (sin problemas, controles ni muestras de antes)", hay == 0);
        memset(c40, 0, sizeof c40);
        c40[0] = c40[31] = c40[32] = c40[39] = 1;
        chk_cond("mono", "cuenta las pantallas visitadas con m" S_aa "s de 32", mono_visitadas(c40, 40) == 4);
    }
}

/* --------------------------------------------------------------------------
 *  13. Archivos rotos: abrir basura nunca cuelga ni deja un circuito corrupto
 * ----------------------------------------------------------------------- */
static void test_archivo_roto(void)
{
    static Circuito a, b;
    static unsigned char bytes[4096];
    char ruta[260];
    int caso, n, i, abiertos = 0;
    FILE *f;

    printf("[13] Archivos rotos\n");
    sprintf(ruta, "%s/ces_01.tns", carpeta_mis);
    for (caso = 0; caso < 3000; caso++) {
        if (caso % 4 == 3) e4_circ_3patas(&a);          /* con un AO y un BJT (Etapa 4) */
        else ejemplo_cargar(caso % N_EJEMPLOS, &a);
        arch_guardar(0, &a);
        f = fopen(ruta, "rb");
        if (!f) break;
        n = (int)fread(bytes, 1, sizeof bytes, f);
        fclose(f);
        if (caso % 3 == 0) n = (int)azar((unsigned)n + 1);                 /* truncado */
        else for (i = 0; i < 1 + (int)azar(6); i++)                          /* bytes cambiados */
            bytes[azar((unsigned)n)] = (unsigned char)azar(256);
        f = fopen(ruta, "wb");
        if (!f) break;
        fwrite(bytes, 1, (size_t)n, f);
        fclose(f);
        if (arch_cargar(0, &b)) {
            const char *mal = circuito_sano(&b);
            abiertos++;
            if (mal) { falla("archivo roto", mal, caso, 0); break; }
        }
    }
    arch_borrar(0);
    printf("    3000 archivos rotos: %d se abrieron (y estaban sanos), el resto se rechazó\n", abiertos);
    pruebas++;
}

/* --------------------------------------------------------------------------
 *  14. Tiempos: lo que más cuesta, medido en el PC. La calculadora (ARM926
 *  a 396 MHz, coma flotante por software) es del orden de 100 veces más
 *  lenta en cálculo con double y ~20 en dibujo con enteros: se informa la
 *  estimación, y se exige que el peor caso quede por debajo de lo que un
 *  alumno aguanta esperando.
 * ----------------------------------------------------------------------- */
#include <time.h>
static double seg_de_reloj(clock_t a, clock_t b) { return (double)(b - a) / CLOCKS_PER_SEC; }

/* Medido en la CX II real con el banco de tiempos (Ayuda > T) SIN el cable
 * USB, tras apagar y encender (2026-09-27, la candidata, a 396
 * MHz): multiplicar y restar 0,46329 µs (183 ciclos), un ejemplo 2,439 ms (53
 * operaciones de Gauss), el peor caso 62,5 ms (119 914 operaciones), volcar
 * la pantalla 3,1546 ms; un cuadro de las pantallas animadas, de 3,13 a 6,78
 * ms sin el volcado. En este PC (el mismo WSL, sin carga) esos cuadros van
 * en ~0,021 a ~0,080 ms: dibujar, ~150 veces el PC; descomprimir, ~85 (el
 * inflate son enteros y tablas: escala menos que el dibujo); el ejemplo,
 * ~0,004 ms: el resto del código, ~600 veces. El banco
 * del cierre (0,63369 µs, 4,3384 ms…) iba con el USB, a 288 MHz: todo ~1,37
 * veces más lento. Antes (2026-09-25, también con el USB): 0,630
 * µs, 3,03 y 81,6 ms. */
#define NS_POR_OP SIM_NS_OP  /* una operación contada en la calculadora (sim.h) */
#define F_RESTO   800.0      /* el resto del código: ~600 veces el PC (ejemplo) */
#define F_CUADRO  150.0      /* dibujar un cuadro: ~133 a ~149 veces el PC      */
#define MS_VOLCAR 3.1546     /* volcar la pantalla                              */
#define NS_PASO_E4 SIM_NS_PASO   /* lo fijo de un paso del emulador (sim.h, §17.4) */

#if ESPECIAL
#include "test_tiempos_esp.inc"         /* la versión especial (no se publica) */
#else
#define ESP_T_TIEMPOS
#endif

/* La calibración: el banco de tiempos en la CX II real (Ayuda > T) SIN el
 * cable USB, tras apagar y encender (2026-09-27, la candidata, a 396 MHz).
 * Cada prueba dura 4 s del RTC; con sus vueltas (4000 ms entre lo que tardó
 * cada una) y la misma carga (el paso y la ventana que eligió aquella
 * versión, ahora a mano: los de banco.c), el modelo con las constantes del
 * código tiene que dar esos 4 s. Si cambia lo que se cuenta (una operación,
 * una exp(), una sin()) o el camino del motor, esto falla: hay que volver a
 * calibrar (sim.h, nolin.h, ca.h). El banco del cierre (con el USB, a 288
 * MHz: 3866, 2727 y 459 vueltas) daba lo mismo ~1,37 veces más lento. */
#define CAL_MS 4000.0
static double cal_emula(void (*red)(Circuito *, Red *), double t_fin, double h, long vueltas,
                        long *np, long *iter, unsigned long *ops, unsigned long *exps)
{
    static Circuito c;
    static Red r;
    unsigned long o0;
    long n, base = 0, it0, p0;
    red(&c, &r);
    c.t_fin.v = t_fin; c.t_fin.est = M_DATO; c.paso.v = h; c.paso.est = M_DATO;
    sim_iniciar(&c, &r);
    sim_ops_reset();                            /* como el banco: tras sim_iniciar */
    o0 = sim_ops(); p0 = sim_res()->pasos; it0 = sim_res()->nw.iter;
    for (n = 0; n < vueltas; n++)
        if (!sim_avanzar(1)) {                  /* acabó: otra vez (como p_sim y p_nl) */
            base += sim_res()->pasos;
            it0 -= sim_res()->nw.iter;
            red(&c, &r);
            c.t_fin.v = t_fin; c.t_fin.est = M_DATO; c.paso.v = h; c.paso.est = M_DATO;
            sim_iniciar(&c, &r);
        }
    *np = base + sim_res()->pasos - p0;
    *iter = sim_res()->nw.iter - it0;
    *ops = sim_ops() - o0;
    *exps = nl_exps();
    sim_cortar();
    return ((double)*ops * SIM_NS_OP + (double)*np * SIM_NS_PASO + (double)*iter * NS_ITER) * 1e-6;
}

static void test_calibracion(void)
{
    const char *q = "calibración";
    char det[160];
    long np, iter;
    unsigned long ops, exps;
    double ms, t;

    printf("  la calibraci" S_oo "n con el banco de la calculadora real, sin el USB (2026-09-27)\n");
    /* BN_EXP: una exp() 7,8439 µs; BN_SENO: una sin() 7,2573 µs */
    t = NL_OPS_EXP * SIM_NS_OP * 1e-3;
    sprintf(det, "una exp(): NL_OPS_EXP = %d operaciones, %.3f " S_MU "s (medido 7,8439; " S_PM "3 %%)", NL_OPS_EXP, t);
    chk_cond(q, det, fabs(t / 7.8439 - 1.0) <= 0.03);
    t = SIM_OPS_SIN * SIM_NS_OP * 1e-3;
    sprintf(det, "una sin(): SIM_OPS_SIN = %d operaciones, %.3f " S_MU "s (medido 7,2573; " S_PM "3 %%)", SIM_OPS_SIN, t);
    chk_cond(q, det, fabs(t / 7.2573 - 1.0) <= 0.03);

    /* BN_SIM: 5370 vueltas de 744,88 µs, un paso del peor emulador cada una
     * (aquella versión eligió h = 2 µs y 2 ms de ventana: dos arranques) */
    ms = cal_emula(banco_peor_sim, 2e-3, 2e-6, 5370, &np, &iter, &ops, &exps);
    printf("    BN_SIM: 5370 vueltas, %ld pasos, %lu operaciones (%.2f por paso): el modelo, %.1f ms (medido 4000)\n",
           np, ops, (double)ops / (double)np, ms);
    chk_cond(q, "BN_SIM: 5370 pasos de 1514,02 operaciones de media (lo que contó la calculadora: 1514)",
             np == 5370 && fabs((double)ops / (double)np - 1514.02) < 0.01);
    sprintf(det, "BN_SIM: operaciones·SIM_NS_OP + pasos·SIM_NS_PASO = %.1f ms, los 4000 medidos (" S_PM "0,5 %%)", ms);
    chk_cond(q, det, fabs(ms / CAL_MS - 1.0) <= 0.005);

    /* BN_NL: 3530 vueltas de 1,1331 ms del puente con 1N4007 (h = T/45, el
     * de aquella versión con 1 s de ventana). Hay vueltas sin paso (Newton
     * no converge o el control de error rechaza: se parte el paso), así que
     * lo fijo de una iteración sale de las cuentas totales */
    nl_ops_reset();
    ms = cal_emula(banco_puente, 1.0, (1.0 / 60.0) / 45.0, 3530, &np, &iter, &ops, &exps);
    printf("    BN_NL: 3530 vueltas, %ld pasos, %ld iteraciones, %lu operaciones (%lu exp y log): el modelo, %.1f ms (medido 4000)\n",
           np, iter, ops, exps, ms);
    chk_cond(q, "BN_NL: la carga de la calculadora (3306 pasos, 9607 iteraciones, 25 712 exp y log)",
             np == 3306 && iter == 9607 && exps == 25712);
    sprintf(det, "BN_NL: operaciones·SIM_NS_OP + pasos·SIM_NS_PASO + iteraciones·NS_ITER = %.1f ms, los 4000 medidos (" S_PM "0,5 %%)", ms);
    chk_cond(q, det, fabs(ms / CAL_MS - 1.0) <= 0.005);

    /* BN_CA: 632 puntos de 6,3291 ms (bode_en en el peor caso de CA, con la
     * f que va de 1 kHz por 1,37 hasta 100 kHz y vuelve a 10 Hz; el de 1 kHz,
     * fuera: es el que el banco contaba, 4112 operaciones). La candidata
     * rehacía la topología en cada punto; si bode_en la reutiliza, las
     * operaciones no cambian y lo real baja (ver CA_NS_OP en ca.h) */
    {
        static Circuito c;
        static Red r;
        BodeAjustes aj;
        Cx H;
        double f = 1e3;
        unsigned long c0;
        int k;
        banco_peor_ca(&c, &r);
        aj.ent = -1; aj.sal = BS_AUTO; aj.f_ini = 10.0; aj.f_fin = 1e5;
        bode_iniciar(&c, &r, &aj);
        c0 = ca_ops();
        bode_en(f, &H);
        f *= 1.37;
        chk_cond(q, "BN_CA: el primer punto, 4112 operaciones (las que enseñaba el banco)", ca_ops() - c0 == 4112);
        c0 = ca_ops();
        for (k = 0; k < 632; k++) { bode_en(f, &H); f = f < 1e5 ? f * 1.37 : 10.0; }
        ops = ca_ops() - c0;
        bode_cortar();
        ms = (double)ops * CA_NS_OP * 1e-6;
        printf("    BN_CA: 632 puntos, %lu operaciones de CA (%.1f de media): el modelo, %.1f ms (medido 4000)\n",
               ops, (double)ops / 632.0, ms);
        sprintf(det, "BN_CA: operaciones de CA·CA_NS_OP = %.1f ms, los 4000 medidos (" S_PM "0,5 %%)", ms);
        chk_cond(q, det, fabs(ms / CAL_MS - 1.0) <= 0.005);
        /* BN_BODE: el Bode del RLC con autorrango, 100 ms (40 en 4 s). Lo
         * estima con una evaluación y las que calcula (del lado seguro); por
         * sus operaciones, en medio */
        banco_rlc(&c, &r);
        aj.ent = -1; aj.sal = BS_AUTO; aj.f_ini = aj.f_fin = 0.0;
        c0 = ca_ops();
        bode_iniciar(&c, &r, &aj);
        while (bode_avanzar(~0ul)) {}
        ops = ca_ops() - c0;
        t = bode_res()->t_est * 1e3;
        ms = (double)ops * CA_NS_OP * 1e-6;
        printf("    BN_BODE: %d evaluaciones, %lu operaciones de CA (%.1f ms por ellas); t_est %.1f ms (medido 100)\n",
               bode_res()->evaluaciones, ops, ms, t);
        sprintf(det, "BN_BODE: t_est %.1f ms y las operaciones %.1f ms, entre 1 y 1,5 veces los 100 medidos", t, ms);
        chk_cond(q, det, t >= 100.0 && t <= 1.5 * 100.0 && ms >= 100.0 && ms <= 1.5 * 100.0);
    }
}

static void test_tiempos(void)
{
    static Solucion s;
    clock_t t0;
    int i, n;
    double t_ej, t_peor, est_peor, est_ej;
    unsigned long ops_ej, ops_peor;

    printf("[14] Tiempos (PC, y estimación para la calculadora)\n");

    /* Los 33 ejemplos: resolver y escribir el procedimiento */
    t0 = clock();
    for (n = 0; n < 20; n++)
        for (i = 0; i < N_EJEMPLOS; i++) {
            static Circuito c; static Red r;
            ejemplo_cargar(i, &c); red_extraer(&c, &r);
            resolver(&c, &r, &s);
            proced_generar(&c, &r, &s, 0, &pasos, 0);
        }
    t_ej = seg_de_reloj(t0, clock()) / (20.0 * N_EJEMPLOS);

    /* Operaciones del Gauss de un ejemplo, en promedio */
    sis_ops_reset();
    for (i = 0; i < N_EJEMPLOS; i++) {
        static Circuito c; static Red r;
        ejemplo_cargar(i, &c); red_extraer(&c, &r);
        resolver(&c, &r, &s);
    }
    ops_ej = sis_ops() / (unsigned long)N_EJEMPLOS;

    /* Peor caso (el mismo que mide el banco en la calculadora): 30
     * resistencias en una red con puentes y 8 corrientes medidas, 2^8
     * combinaciones de signo sobre una sola eliminación */
    banco_peor_caso(&rc, &rr);
    {
        int k2, autolazos = 0;
        for (k2 = 0; k2 < rr.nel; k2++) if (rr.a[k2] == rr.b[k2]) autolazos++;
        pruebas++;
        if (rr.nn < 16 || autolazos) falla("tiempos", "el peor caso es una red densa sin cortos", rr.nn, 16);
    }
    /* Se mide una sola corrida, así que un tirón del sistema anfitrión la
     * multiplica: se toma la mejor de 5. Lo que no es el Gauss (el
     * procedimiento) se mide aparte: es lo que se escala con F_RESTO. */
    t_peor = 1e9;
    for (n = 0; n < 5; n++) {
        double t;
        sis_ops_reset();
        resolver(&rc, &rr, &s);
        ops_peor = sis_ops();
        t0 = clock();
        proced_generar(&rc, &rr, &s, 0, &pasos, 0);
        t = seg_de_reloj(t0, clock());
        if (t < t_peor) t_peor = t;
    }
    pruebas++;
    if (s.estado != SOL_OK || s.ndatos != 8) falla("tiempos", "el peor caso tiene 8 datos y se resuelve", s.ndatos, 8);

    /* Modelo de la calculadora: el Gauss por sus operaciones (cada una,
     * NS_POR_OP ns: soft-float del ARM926 con sus cargas; lo calibra el banco
     * de tiempos) y el resto con el factor de siempre. */
    est_ej = (double)ops_ej * NS_POR_OP * 1e-9 + t_ej * F_RESTO;
    est_peor = (double)ops_peor * NS_POR_OP * 1e-9 + t_peor * F_RESTO;
    printf("    resolver + procedimiento de un ejemplo: %.3f ms, %lu operaciones de Gauss  (calculadora ~%.0f ms)\n",
           t_ej * 1e3, ops_ej, est_ej * 1e3);
    printf("    peor caso (%d elementos, %d nodos, %d datos): %.2f ms, %lu operaciones  (calculadora ~%.0f ms)\n",
           rc.nel, rr.nn, s.ndatos, t_peor * 1e3, ops_peor, est_peor * 1e3);
    pruebas += 2;
#if defined(__SANITIZE_ADDRESS__)
    /* Con ASan todo va varias veces más lento: los tiempos no dicen nada */
    printf("    (con ASan los tiempos solo se muestran)\n");
    if (0)
#endif
    {
    if (est_ej > 0.5) falla("tiempos", "un ejemplo tardaría más de medio segundo", est_ej, 0.5);
    if (est_peor > 0.5) falla("tiempos", "el peor caso de CC tardaría más de medio segundo", est_peor, 0.5);
    }
    ESP_T_TIEMPOS

    /* El emulador: el RLC con una cuadrada de 1 kHz de la hoja de ruta, y una
     * red grande (32 elementos: escalera de 15 L en serie con 15 C a tierra).
     * Se estima por operaciones (0,46329 µs cada una) más lo fijo de cada
     * paso (bucles, llamadas: 43,45 µs, BN_SIM; antes, un 30 % a ojo): no
     * depende del PC, vale también con ASan. */
    {
        unsigned long o_rlc, o_grande;
        double e_rlc, e_grande, te_rlc, te_grande;
        long p_rlc, p_grande;
        int q, kq;
        sr_init(4);
        kq = sr_el(SE_E, 0, 1, 0.0);
        sr.el[kq].forma = ONDA_CUADRADA; sr.el[kq].p[0] = 1.0; sr.el[kq].p[1] = 1e3; sr.el[kq].p[2] = 50.0;
        sr_el(SE_R, 1, 2, 20.0); sr_el(SE_L, 2, 3, 10e-3);
        q = sr_el(SE_C, 3, 0, 1e-6);
        canal(0, CANAL_V, q);
        saj.t_fin = 5e-3;
        sim_ops_reset(); sim_emular(&sr, &saj); o_rlc = sim_ops(); p_rlc = sim_res()->pasos; te_rlc = sim_res()->t_est;
        sr_init(17);
        sr_escalon(0, 1, 10.0);
        for (q = 1; q <= 15; q++) { sr_el(SE_L, q, q + 1, 1e-3); sr_el(SE_C, q + 1, 0, 1e-6); }
        sr_el(SE_R, 16, 0, 10.0);
        canal(0, CANAL_NODO, 16);
        saj.t_fin = 5e-3;
        sim_ops_reset(); sim_emular(&sr, &saj); o_grande = sim_ops(); p_grande = sim_res()->pasos; te_grande = sim_res()->t_est;
        e_rlc = (double)o_rlc * NS_POR_OP * 1e-9 + (double)p_rlc * NS_PASO_E4 * 1e-9;
        e_grande = (double)o_grande * NS_POR_OP * 1e-9 + (double)p_grande * NS_PASO_E4 * 1e-9;
        printf("    emular un RLC con cuadrada: %ld pasos, %lu operaciones (calculadora ~%.0f ms; t_est %.0f ms)\n", p_rlc, o_rlc,
               e_rlc * 1e3, te_rlc * 1e3);
        printf("    emular 32 elementos (15 L y 15 C): %ld pasos, %lu operaciones (calculadora ~%.2f s; t_est %.2f s)\n", p_grande,
               o_grande, e_grande, te_grande);
        pruebas += 2;
        if (e_rlc > 0.5) falla("tiempos", "emular un RLC tardaría más de medio segundo", e_rlc, 0.5);
        if (e_grande > 3.0) falla("tiempos", "emular la red grande tardaría más de 3 s", e_grande, 3.0);
    }

    /* La Etapa 3, por operaciones: el análisis de CA del peor caso (el del
     * banco, BN_CA), el Bode del RLC (BN_BODE) y la calculadora del motor */
    {
        static Circuito c;
        static Red r;
        static CaRes cr;
        BodeAjustes aj;
        unsigned long o_ca;
        double e_ca, e_bode, e_motor = -1.0;
        int m, nel_ca, nn_ca;
        banco_peor_ca(&c, &r);
        nel_ca = c.nel; nn_ca = r.nn;
        ca_ops_reset();
        chk_cond("tiempos CA", "el peor caso de CA se analiza", ca_analiza(&c, &r, &cr) == SIM_OK);
        o_ca = ca_ops();
        e_ca = (double)o_ca * CA_NS_OP * 1e-9;           /* lo medido con BN_CA (ca.h) */
        banco_rlc(&c, &r);
        aj.ent = -1; aj.sal = BS_AUTO; aj.f_ini = aj.f_fin = 0.0;
        chk_cond("tiempos CA", "el Bode del RLC se barre", bode_iniciar(&c, &r, &aj) == SIM_OK);
        while (bode_avanzar(~0ul)) {}
        e_bode = bode_res()->t_est;
        for (m = 0; m < N_MODELOS; m++)
            if (MODELOS[m].medida == 'c' && strcmp(MODELOS[m].clave_el, "C1") == 0) {
                unsigned long c0, oca;
                modelo_cargar(m, &c);
                red_extraer(&c, &r);
                c0 = ca_ops();
                if (cal_iniciar(&c, &r) == CAL_OK) while (cal_avanzar(~0ul)) {}
                /* sus operaciones de CA a CA_NS_OP y las del ajuste a SIM_NS_OP
                 * (antes, todas a 0,63 µs y un 30 % más, a ojo) */
                oca = ca_ops() - c0;
                e_motor = (double)oca * CA_NS_OP * 1e-9 + (cal_res()->ops > oca ? (double)(cal_res()->ops - oca) * NS_POR_OP * 1e-9 : 0.0);
            }
        printf("    CA del peor caso (%d elementos, %d nodos): %lu operaciones (calculadora ~%.1f ms)\n", nel_ca, nn_ca, o_ca, e_ca * 1e3);
        printf("    Bode del RLC: %d puntos (calculadora ~%.3f s); calculadora del motor ~%.3f s\n",
               bode_reg()->n, e_bode, e_motor);
        pruebas += 3;
        if (e_ca > 0.05) falla("tiempos", "la CA del peor caso tardaría más de 50 ms", e_ca, 0.05);
        if (!(e_bode <= 0.3)) falla("tiempos", "el Bode del RLC tardaría más de 0,3 s", e_bode, 0.3);
        if (!(e_motor >= 0.0 && e_motor <= 0.5)) falla("tiempos", "la calculadora del motor tardaría más de 0,5 s", e_motor, 0.5);
    }

    /* La Etapa 4 con el modelo del §8.2, medido en la calculadora sin el
     * USB (2026-09-27): 0,46329 µs por operación (las exp y los log cuentan
     * NL_OPS_EXP = 17), NS_ITER = 214,3 µs fijos por iteración de Newton y
     * NS_PASO_E4 = 43,45 µs por paso (lo que no se cuenta: bucles y
     * llamadas; el banco los mide con BN_NL y BN_SIM; test_calibracion lo
     * comprueba).
     * El punto de operación del peor caso sin transistores, el puente con
     * 1N4007 en 5 periodos, el barrido de un diodo y el astable. */
    {
        static Circuito c;
        static Red r;
        static Valores v;
        SimDiag d;
        SimNewton nw;
        unsigned long o;
        double e_op, e_puente, e_barr, e_ast;
        int j, kk, e;
        long it_barr = 0;
        /* el peor OP: 32 elementos, 20 nodos, 10 diodos exponenciales y 2 AO */
        sr_init(20);
        a4_cc(0, 1, 5.0); a4_cc(0, 10, 2.0);
        sr_el(SE_R, 1, 2, 1e3); sr_el(SE_R, 2, 3, 2e3);
        a4_ao(0, 2, 3, 0, 0, 0.0);
        {
            static const signed char A[5] = { 1, 4, 5, 6, 7 }, B[5] = { 3, 9, 13, 14, 15 };
            static const signed char NA[5] = { 4, 5, 6, 7, 8 }, NB[5] = { 9, 13, 14, 15, 16 };
            for (j = 0; j < 5; j++) {
                sr_el(SE_R, A[j], NA[j], 1e3); t4_d_cat(NA[j], 0, &NL_1N4148);
                sr_el(SE_R, B[j], NB[j], 1e3); t4_d_cat(0, NB[j], &NL_1N4148);
            }
        }
        a4_ao(6, 17, 17, 0, 0, 0.0);
        sr_el(SE_R, 17, 18, 1e3); sr_el(SE_R, 18, 0, 1e3);
        sr_el(SE_R, 10, 11, 1e3); sr_el(SE_R, 11, 12, 1e3); sr_el(SE_R, 12, 19, 1e3); sr_el(SE_R, 19, 0, 1e3);
        chk_cond("tiempos E4", "el peor OP: 32 elementos y 20 nodos", sr.nel == 32 && sr.nn == 20);
        memset(&nw, 0, sizeof nw);
        sim_ops_reset();
        e = sim_op_cc_red(&sr, &v, &d, &nw);
        o = sim_ops();
        e_op = (double)o * NS_POR_OP * 1e-9 + (double)nw.iter * NS_ITER * 1e-9;
        chk_cond("tiempos E4", "el peor OP converge", e == SIM_OK);
        printf("    OP del peor caso (32 elementos, 10 diodos, 2 AO): %ld iteraciones, %lu operaciones (calculadora ~%.0f ms)\n",
               nw.iter, o, e_op * 1e3);
        pruebas += 2;
        if (e_op > 0.5) falla("tiempos", "el peor OP tardaría más de 0,5 s", e_op, 0.5);
        if (o >= NL_OPS_OP) falla("tiempos", "el peor OP pasa del tope NL_OPS_OP", (double)o, (double)NL_OPS_OP);

        /* el puente con filtro de 1N4007: 5 periodos, el paso automático */
        banco_puente(&c, &r);
        c.t_fin.v = 5.0 / 60.0; c.t_fin.est = M_DATO;
        sim_ops_reset();
        chk_cond("tiempos E4", "el puente se emula", sim_iniciar(&c, &r) == SIM_OK);
        while (sim_avanzar(~0ul)) {}
        o = sim_ops();
        e_puente = (double)o * NS_POR_OP * 1e-9 + (double)sim_res()->nw.iter * NS_ITER * 1e-9 +
                   (double)sim_res()->pasos * NS_PASO_E4 * 1e-9;
        printf("    puente con 1N4007, 5 periodos: %ld pasos, %ld iteraciones, %lu operaciones (calculadora ~%.2f s; t_est %.2f s)\n",
               sim_res()->pasos, sim_res()->nw.iter, o, e_puente, sim_res()->t_est);
        /* Con lo medido sin el USB (2026-09-27) el tope de 2 s no cuenta: h es
         * t_fin/2000 = 41,7 µs y el plan, 1,90 s. Lo real sale ~8 % más:
         * elige_paso cuenta ⌈t_fin/h⌉ = 2000 pasos con 2 iteraciones, y la
         * subdivisión de Newton da 2377 pasos (4513 iteraciones); en las redes
         * suaves del arnés la mediana es 1,1 pasos y 2,0 iteraciones por paso
         * contado, y el puente, de los peores. El control del gasto, que mira
         * cada segundo gastado, corrige t_est a 2,07 s con el ritmo que ve (con
         * el banco del USB, el tope llevaba h a 49,9 µs: 2090 pasos, 2,43 s con
         * 1,99) */
        pruebas += 3;
        if (sim_res()->fin != FIN_COMPLETO) falla("tiempos", "el puente acaba", sim_res()->fin, FIN_COMPLETO);
        if (e_puente > 2.5) falla("tiempos", "el puente tardaría más de 2,5 s", e_puente, 2.5);
        if (!(e_puente <= 1.3 * sim_res()->t_est))
            falla("tiempos", "el puente: lo real, como mucho 1,3 veces t_est", e_puente, 1.3 * sim_res()->t_est);

        /* el barrido de 281 puntos de un diodo */
        sr_init(3);
        kk = a4_cc(0, 1, 1.0); sr_el(SE_R, 1, 2, 1e3); j = t4_d_cat(2, 0, &NL_1N4148);
        {
            SimCanal ch[2];
            memset(ch, 0, sizeof ch);
            ch[0].tipo = CANAL_I; ch[0].elem = (signed char)j; ch[0].sg = 1; ch[1].tipo = CANAL_NADA;
            o = barr_ops();
            barr_iniciar_red(&sr, kk, BX_E, ch, 0.0, 5.0, 0, 0);
            while (barr_avanzar(~0ul)) {}
            o = barr_ops() - o;
            for (j = 0; j < barr_reg()->n; j++) it_barr += barr_reg()->it[0][j];
        }
        e_barr = (double)o * NS_POR_OP * 1e-9 + (double)it_barr * NS_ITER * 1e-9;
        printf("    barrido de un diodo: %d puntos, %ld iteraciones, %lu operaciones (calculadora ~%.2f s; t_est %.2f s)\n",
               barr_reg()->n, it_barr, o, e_barr, barr_res()->t_est);
        pruebas += 2;
        if (barr_reg()->n != BARR_N || barr_res()->fin != FIN_COMPLETO) falla("tiempos", "el barrido del diodo: 281 puntos", barr_reg()->n, BARR_N);
        if (e_barr > 1.0) falla("tiempos", "el barrido del diodo tardaría más de 1 s", e_barr, 1.0);

        /* el astable (el de la biblioteca: R = 10 k, C = 100 n, β = 0,5), con
         * la ventana y el paso automáticos */
        sr_init(4);
        sr_el(SE_R, 3, 1, 10e3);
        j = sr_el(SE_C, 1, 0, 100e-9);
        sr_el(SE_R, 3, 2, 10e3); sr_el(SE_R, 2, 0, 10e3);
        a4_ao(2, 1, 3, 0, 0, 0.0);
        canal(0, CANAL_NODO, 3); canal(1, CANAL_V, j);
        saj.t_fin = 0.0; saj.h = 0.0;
        sim_ops_reset();
        sim_emular(&sr, &saj);
        o = sim_ops();
        e_ast = (double)o * NS_POR_OP * 1e-9 + (double)sim_res()->nw.iter * NS_ITER * 1e-9 +
                (double)sim_res()->pasos * NS_PASO_E4 * 1e-9;
        printf("    astable: %ld pasos, %ld iteraciones, %lu operaciones (calculadora ~%.2f s; t_est %.2f s)\n",
               sim_res()->pasos, sim_res()->nw.iter, o, e_ast, sim_res()->t_est);
        pruebas++;
        if (e_ast > 2.0) falla("tiempos", "el astable tardaría más de 2 s", e_ast, 2.0);
    }
    q4_tiempos();                       /* el paso 13: el peor OP con BJT y su familia */
    m4_tiempos();                       /* el paso 14: la familia del MOSFET */
    test_calibracion();                 /* las constantes, contra el banco de la calculadora */
}

/* --------------------------------------------------------------------------
 *  La huella de cada emulación (HAKARU_HUELLA_LOG=ruta, o «-»: en la salida,
 *  entre los títulos de las secciones): para auditar qué emulaciones cambian
 *  con un cambio del motor. Una línea por emulación (la llama sim.c al
 *  cerrarla: sim_al_terminar) con sus cuentas y el FNV-1a del registro (t,
 *  y1, y2) y de W, Vmed e Imed de cada elemento; dos corridas se comparan
 *  línea a línea por el hash y los pasos (el juez del flanco,
 *  rev3/juez_flanco/compara_huellas.py). rig: la red tiene un modo de τ < 5h;
 *  tram: los eventos con el tramo rígido. Con HAKARU_HUELLA_RED, también la red.
 * ----------------------------------------------------------------------- */
static FILE *huella_f;
static long huella_n;

static unsigned long long huella_fnv(unsigned long long h, const void *p, size_t n)
{
    const unsigned char *b = (const unsigned char *)p;
    size_t i;
    for (i = 0; i < n; i++) { h ^= b[i]; h *= 1099511628211ULL; }
    return h;
}

static void huella_emulacion(void)
{
    const SimRes *r = sim_res();
    const SimReg *g = sim_reg();
    const SimRed *s = sim_red();
    unsigned long long hs = 1469598103934665603ULL;   /* (el del juez, para comparar con sus corridas) */
    double ent = 0.0;
    int i, c, ncl = 0, nnl = 0;
    if (!huella_f) return;
    for (i = 0; i < g->n; i++) {
        hs = huella_fnv(hs, &g->t[i], sizeof g->t[i]);
        for (c = 0; c < 2; c++) hs = huella_fnv(hs, &g->y[c][i], sizeof g->y[c][i]);
    }
    for (i = 0; i < s->nel; i++) {
        const SimEl *e = &s->el[i];
        double q[3];
        q[0] = r->W[i]; q[1] = r->Vmed[i]; q[2] = r->Imed[i];
        hs = huella_fnv(hs, q, sizeof q);
        if (e->tipo == SE_C || e->tipo == SE_L) ncl++;
        if (e->tipo == SE_D || e->tipo == SE_AO || e->tipo == SE_BJT || e->tipo == SE_MOS) nnl++;
        if (e->tipo == SE_E || e->tipo == SE_J || e->tipo == SE_EC || e->tipo == SE_JC || e->tipo == SE_AO) ent += fabs(r->W[i]);
    }
    /* rig: la red tiene un modo de τ < 5h (el tramo rígido en cada evento);
     * tram: los eventos que tuvieron el tramo (con no lineales, la sonda de te⁺) */
    fprintf(huella_f, "%ld nel %d n %d pasos %ld ev %d nl %d h %.6g tfin %.6g fin %d ncl %d fact %d rig %d tram %d ops %lu bal %.3e ent %.3e %016llx\n",
            huella_n++, s->nel, g->n, r->pasos, r->eventos, nnl, r->h, r->t_fin, r->fin, ncl, r->factorizaciones,
            r->tau_min > 0.0 && r->h > 0.2 * r->tau_min, r->tramos, sim_ops(), r->dif_balance, ent, hs);
    if (getenv("HAKARU_HUELLA_RED")) {
        for (i = 0; i < s->nel; i++) {
            const SimEl *e = &s->el[i];
            fprintf(huella_f, "   el %d tipo %d %d-%d val %.4g forma %d p %.4g %.4g %.4g %.4g %.4g t1 %.4g t2 %.4g W %.4g\n", i, e->tipo,
                    e->a, e->b, e->val, e->forma, e->p[0], e->p[1], e->p[2], e->p[3], e->p[4], e->t1, e->t2, r->W[i]);
        }
        fprintf(huella_f, "   tau_min %.4g modos0 %d nsucesos %d avisos 0x%x sonda %ld\n", r->tau_min, r->nmodos[0], r->nsucesos,
                r->d.avisos, r->nw.sonda);
    }
    fflush(huella_f);
}

/* La capa de entrada (entrada.c, bucle.c) en tiempo virtual ([70]) */
#include "test_entrada.inc"

/* Las mejoras que esperaban a la capa: cuentas, copiar y pegar, nombres ([72]) */
#include "test_m_flujo.inc"

/* El residuo de Kirchhoff del mono: su acumulador y su métrica ([12a]) */
#include "test_arnes.inc"

/* El cierre de la Etapa 4: lo que encontró la verificación final ([73]) */
#include "test_cierre.inc"

/* El modelo de tiempos con el banco sin el USB ([75e]) */
#include "test_opt_cal.inc"

/* Las pantallas de la verificación final ([74]) */
#include "test_r8_pantallas.inc"

/* El motor optimizado, bit a bit, y sus cachés ([75m], rev9) */
#include "test_opt_motor.inc"

/* rev9, grupo cabode: el Bode y la CA optimizados ([75c]) */
#include "test_opt_cabode.inc"

/* La optimización del grupo «otros» ([75o]): signos, potencias de 10, pasos, banco */
#include "test_opt_otros.inc"

/* La optimización del dibujo ([75d]) */
#include "test_opt_dibujo.inc"

/* El turbo (experimental), con el simulador de la PMU ([75]) */
#include "test_opt_turbo.inc"

/* La integración de la fase 1: lo que encontraron las revisiones ([75i]); el
 * arnés al azar de la API del motor (de la revisión de motor), para I19 */
#include "test_opt_fuzz.inc"
#include "test_opt_final.inc"

/* rev10: los capacitores equivalentes (Taller, 4.ª opción; la v3 en rev12)
 * y el volcado de su oráculo ([76]) */
#include "capmotor.h"
#include "test_capmotor.inc"

/* ----------------------------------------------------------------------- */
int main(int argc, char **argv)
{
    if (argc > 1) dir_out = argv[1];
    if (getenv("HAKARU_HUELLA_LOG")) {     /* la huella de cada emulación (arriba); «-»: en la salida, entre las secciones */
        huella_f = strcmp(getenv("HAKARU_HUELLA_LOG"), "-") ? fopen(getenv("HAKARU_HUELLA_LOG"), "w") : stdout;
        if (huella_f) sim_al_terminar(huella_emulacion);
    }
    sprintf(carpeta_mis, "%s/guardados", dir_out);
    mkdir(carpeta_mis, 0755);
    arch_carpeta(carpeta_mis);
    {                                   /* cada corrida empieza sin sesión */
        char ruta[300];
        ESP_T_LIMPIA(ruta)
        sprintf(ruta, "%s/ces_sesion.tns", carpeta_mis);
        remove(ruta);
    }
    gfx_init();
    banco_duracion(3);                  /* en el PC basta con unos ms por prueba */
    /* Las pruebas de antes de rev10 se escribieron para un Taller de tres
     * filas y una Ayuda que acaba en ACERCA DE: lo de los capacitores
     * equivalentes no cuenta para sus teclas (ui.h); test_capmotor y la fase
     * del mono de los capacitores lo ven todo */
    ui_cpm_oculto(1);
    if (getenv("HAKARU_SOLO_CAPMOTOR")) {  /* solo los capacitores equivalentes ([76]); con HAKARU_CAPMOTOR_VOLCADO, el volcado del oráculo (v3) */
        if (getenv("HAKARU_CAPMOTOR_VOLCADO")) return cpm_volcado(getenv("HAKARU_CAPMOTOR_VOLCADO"), getenv("HAKARU_CAPMOTOR_SALIDA"));
        test_capmotor();
#ifdef POR_LA_CAPA
        ce_informe();
#endif
        printf("%d comprobaciones, %d fallos\n", pruebas, fallos);
        return fallos != 0;
    }
    ESP_T_SOLO
    if (getenv("HAKARU_SOLO_TIEMPOS")) {   /* los de reloj, solos (con otras baterías a la vez fallan por la carga) */
        test_tiempos();
        printf("%d comprobaciones, %d fallos\n", pruebas, fallos);
        return fallos != 0;
    }
    if (getenv("HAKARU_SOLO_MONO")) {      /* depurar el mono sin lo demás */
        test_mono();
        test_residuo_mono();
        printf("%d comprobaciones, %d fallos\n", pruebas, fallos);
        return fallos != 0;
    }
    if (getenv("HAKARU_SOLO_REV")) {       /* solo los hallazgos de la revisión (las mutaciones) */
        test_e4_rev();
        printf("%d comprobaciones, %d fallos\n", pruebas, fallos);
        return fallos != 0;
    }
    if (getenv("HAKARU_SOLO_REV2")) {      /* solo los hallazgos de las dos revisiones (las mutaciones) */
        test_e4_rev();
        test_rev2_barr_a();
        test_rev2_barr_b();
        test_rev2_ca_res();
        test_rev2_dibujo_a();
        test_rev2_dibujo_b();
        test_rev2_editor();
        test_rev3_op_a();
        test_rev3_op_b();
        test_rev3_tran_a();
        test_rev3_tran_b();
        test_rev3_modelo();
        printf("%d comprobaciones, %d fallos\n", pruebas, fallos);
        return fallos != 0;
    }
    if (getenv("HAKARU_SOLO_M_EDITOR")) {  /* solo las mejoras del editor y del flujo ([70]) */
        test_m_editor();
        printf("%d comprobaciones, %d fallos\n", pruebas, fallos);
        return fallos != 0;
    }
    if (getenv("HAKARU_SOLO_M_RESULTADOS")) {   /* solo las mejoras de resultados e instrumentos */
        test_m_resultados();
        printf("%d comprobaciones, %d fallos\n", pruebas, fallos);
        return fallos != 0;
    }
    if (getenv("HAKARU_SOLO_CAPA")) {      /* solo la capa de entrada ([71]) */
        test_capa_entrada();
        printf("%d comprobaciones, %d fallos\n", pruebas, fallos);
        return fallos != 0;
    }
    if (getenv("HAKARU_SOLO_FLUJO")) {     /* solo cuentas, copiar y pegar y nombres ([72]) */
        test_m_flujo();
#ifdef POR_LA_CAPA
        ce_informe();
#endif
        printf("%d comprobaciones, %d fallos\n", pruebas, fallos);
        return fallos != 0;
    }
    if (getenv("HAKARU_SOLO_CAL")) {       /* solo el modelo de tiempos ([75e]) y la calibración */
        test_opt_cal();
        test_calibracion();
        printf("%d comprobaciones, %d fallos\n", pruebas, fallos);
        return fallos != 0;
    }
    if (getenv("HAKARU_SOLO_FINAL")) {     /* solo el turbo ([75]) y la integración ([75i]) */
        test_opt_turbo();
        test_opt_final();
        printf("%d comprobaciones, %d fallos\n", pruebas, fallos);
        return fallos != 0;
    }
    if (getenv("HAKARU_SOLO_CIERRE")) {    /* solo el cierre de la Etapa 4 ([73]) */
        test_cierre();
#ifdef POR_LA_CAPA
        ce_informe();
#endif
        printf("%d comprobaciones, %d fallos\n", pruebas, fallos);
        return fallos != 0;
    }
    if (getenv("HAKARU_SOLO_PANTALLAS")) { /* solo las pantallas de la verificación final ([74]) */
        test_r8_pantallas();
#ifdef POR_LA_CAPA
        ce_informe();
#endif
        printf("%d comprobaciones, %d fallos\n", pruebas, fallos);
        return fallos != 0;
    }
    if (getenv("HAKARU_SOLO_ORA")) {       /* solo las redes del oráculo no lineal ([67]) */
        test_e4_ora();
        test_red_099();
        printf("%d comprobaciones, %d fallos\n", pruebas, fallos);
        return fallos != 0;
    }
    if (getenv("HAKARU_SOLO_FLANCO")) {    /* el banco del arranque tras un flanco (test_flanco.inc) */
        test_flanco();
        printf("%d comprobaciones, %d fallos\n", pruebas, fallos);
        return fallos != 0;
    }
    if (getenv("HAKARU_SOLO_E2T")) {       /* el transitorio lineal de la Etapa 2 (y el oráculo [29]) */
        test_lu_ondas();
        test_sim_op();
        test_transitorio();
        test_op_clS();
        test_transitorio_e2();
        test_eventos();
        test_modos_auto();
        test_biblioteca();
        test_revision_e2();
        test_revision_e2_motor();
        test_fasor_trapecio();
        test_oraculo_export();
        printf("%d comprobaciones, %d fallos\n", pruebas, fallos);
        return fallos != 0;
    }
    if (getenv("HAKARU_SOLO_E4")) {        /* depurar la Etapa 4 sin lo demás */
        test_e4_modelo();
        test_nolin();
        test_e4_ctl();
        test_e4_diodo();
        test_e4_trans();
        test_e4_ao();
        test_e4_pq();
        test_e4_barr();
        test_e4_paso();
        test_e4_esq();
        test_e4_curva();
        test_e4_ui();
        test_e4_res();
        test_e4_bib();
        test_e4_bjt();
        test_e4_mos();
        test_e4_rev();
        test_e4_ora();
        test_rev2_barr_a();
        test_rev2_barr_b();
        test_rev2_ca_res();
        test_rev2_dibujo_a();
        test_rev2_dibujo_b();
        test_rev2_editor();
        test_rev3_op_a();
        test_rev3_op_b();
        test_rev3_tran_a();
        test_rev3_tran_b();
        test_rev3_modelo();
        test_m_editor();
        test_m_resultados();
        test_flanco();
        test_capa_entrada();
        test_m_flujo();
        test_cierre();
#ifdef POR_LA_CAPA
        ce_informe();
#endif
        printf("%d comprobaciones, %d fallos\n", pruebas, fallos);
        return fallos != 0;
    }

    test_pizarron();
    test_ejemplos();
    test_azar();
    test_diagnosticos();
    test_lecciones();
    test_archivo();
    test_ui();
    ESP_T_PRUEBAS_A
    test_practica();
    test_banco();
    test_mis_danado();
    test_entrada();
    test_fuente_corriente();
    test_instrumentos();
    test_fuente_real();
    test_tierra();
    test_revision_e1();
    test_lu_ondas();
    test_sim_op();
    test_transitorio();
    test_op_clS();
    test_transitorio_e2();
    test_eventos();
    test_modos_auto();
    test_osc();
    test_ui_emu();
    test_biblioteca();
    test_revision_e2();
    test_revision_e2_motor();
    test_complejos();
    test_modelo_e3();
    test_cc_ante_ca();
    test_sim_ante_ca();
    test_ca_cerrados();
    test_ca_cruces();
    test_fasor_trapecio();
    test_thevenin();
    test_calca();
    test_fasorial();
    test_revision_e3_motor();
    test_revision_e3_ui();
    test_revision_e3_modelo();
    test_e4_modelo();
    test_nolin();
    test_e4_ctl();
    test_e4_diodo();
    test_e4_trans();
    test_e4_ao();
    test_e4_pq();
    test_e4_barr();
    test_e4_paso();
    test_e4_esq();
    test_e4_curva();
    test_e4_ui();
    test_e4_res();
    test_e4_bib();
    test_e4_bjt();
    test_e4_mos();
    test_e4_rev();
    test_e4_ora();
    test_rev2_barr_a();
    test_rev2_barr_b();
    test_rev2_ca_res();
    test_rev2_dibujo_a();
    test_rev2_dibujo_b();
    test_rev2_editor();
    test_rev3_op_a();
    test_rev3_op_b();
    test_rev3_tran_a();
    test_rev3_tran_b();
    test_rev3_modelo();
    test_m_editor();
    test_m_resultados();
    test_flanco();
    test_bode();
    test_trazador();
    test_ui_ca();
    test_ui_ca2();
    test_biblioteca_ca();
    test_oraculo_export();
    test_oraculo_ca_export();
    test_modelo_e2();
    test_calc_clS();
    test_formas();
    test_fracciones();
    test_archivo_roto();
    test_mono();
    test_residuo_mono();                /* detrás: no toca el camino del mono */
    test_red_099();
    test_tiempos();
    test_capa_entrada();                /* al final: sus BMP no corren la numeración */
    test_m_flujo();                     /* y estas (out/f_*.bmp), tampoco */
#ifdef POR_LA_CAPA
    ce_informe();
#endif
    ESP_T_PRUEBAS_B
    test_cierre();
    test_r8_pantallas();
    test_opt_cal();                     /* el modelo de tiempos: sin pantallas ni teclas */
    test_opt_motor();                   /* sus emulaciones van al final de las huellas, tras las de cal */
    test_opt_cabode();                  /* el Bode y la CA optimizados: al final, sin BMP */
    test_opt_otros();                   /* al final: sus redes no mueven las huellas ni las BMP */
    test_opt_dibujo();                  /* la optimización del dibujo: al final, tras todas */
    test_opt_turbo();                   /* el turbo: al final, sus BMP (tb_*) y sin emular */
    test_opt_final();                   /* la integración: la última (sin BMP; sus huellas, al final) */
    test_capmotor();                    /* rev10, los capacitores equivalentes: tras todo (sus BMP, out/cm_*) */

    pruebas++;
    if (sis_fuera() != 0) falla("álgebra", "coeficientes escritos fuera del sistema", (double)sis_fuera(), 0);
    pruebas++;
    if (sim_fuera() != 0) falla("emulador", "escrituras fuera de sus tablas", (double)sim_fuera(), 0);

    printf("\n%d comprobaciones, %d fallos\n", pruebas, fallos);
    if (fallos == 0) printf("TODO CORRECTO\n");
    return fallos ? 1 : 0;
}
