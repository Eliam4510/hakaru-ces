/* ============================================================================
 *  proced.c  -  El procedimiento  (ver proced.h)
 * ========================================================================= */
#include <string.h>
#include <math.h>
#include "proced.h"
#include "arbol.h"
#include "numfmt.h"
#include "gfx.h"

static Arbol arb;
static double Rn[ARB_MAX], Vn[ARB_MAX], In[ARB_MAX];
static double Rel[MAX_EL];                  /* R de cada hoja; NaN = incognita */

static const Circuito *cc;
static Pasos *pp;

#define NOM(i)  (arb.n[i].nombre)
#define NAN_D   (0.0 / 0.0)

/* --------------------------------------------------------------------------
 *  Utilidades de texto
 * ----------------------------------------------------------------------- */
static Paso *paso(const char *t1, const char *t2, unsigned int mascara, Buf *det)
{
    char t[PASO_TIT];
    Buf b;
    Paso *s;
    buf_init(&b, t, sizeof t);
    buf_str(&b, t1);
    if (t2) buf_str(&b, t2);
    s = pasos_nuevo(pp, t, mascara);
    if (s && det) buf_init(det, s->detalle, PASO_TXT);
    return s;
}

static void res_init(Paso *s, Buf *b) { buf_init(b, s->resultado, PASO_TXT); }

/* "R23 = 3 ohm" o "R23 aprox 3,333 ohm" */
static void igual_u(Buf *b, const char *nom, double v, const char *u)
{
    buf_str(b, nom);
    buf_igual(b, v);
    buf_ch(b, ' ');
    buf_str(b, u);
}

/* --------------------------------------------------------------------------
 *  Reduccion (paso 2)
 * ----------------------------------------------------------------------- */
static int todos_iguales(int i)
{
    const NodoAr *n = &arb.n[i];
    int k;
    for (k = 1; k < n->nh; k++)
        if (fabs(Rn[n->h[k]] - Rn[n->h[0]]) > 1e-12 * Rn[n->h[0]]) return 0;
    return 1;
}

static void paso_reduccion(int i)
{
    const NodoAr *n = &arb.n[i];
    Paso *s;
    Buf b;
    int k;
    int es_raiz = (i == arb.raiz);

    if (n->tipo == AR_SERIE) {
        s = paso(es_raiz ? "Resistencia equivalente (serie)" : "Serie: ",
                 es_raiz ? 0 : NOM(i), n->mascara, &b);
        if (!s) return;
        buf_str(&b, NOM(i)); buf_str(&b, " = ");
        for (k = 0; k < n->nh; k++) { if (k) buf_str(&b, " + "); buf_str(&b, NOM(n->h[k])); }
        buf_str(&b, "\n= ");
        for (k = 0; k < n->nh; k++) { if (k) buf_str(&b, " + "); buf_sig(&b, Rn[n->h[k]]); }
    } else if (todos_iguales(i)) {
        s = paso(es_raiz ? "Resistencia equivalente (paralelo)" : "Paralelo: ",
                 es_raiz ? 0 : NOM(i), n->mascara, &b);
        if (!s) return;
        buf_int(&b, n->nh);
        buf_str(&b, " iguales en paralelo: R/n\n");
        buf_str(&b, NOM(i)); buf_str(&b, " = ");
        buf_sig(&b, Rn[n->h[0]]); buf_ch(&b, '/'); buf_int(&b, n->nh);
    } else if (n->nh == 2) {
        const char *a = NOM(n->h[0]), *c2 = NOM(n->h[1]);
        s = paso(es_raiz ? "Resistencia equivalente (paralelo)" : "Paralelo: ",
                 es_raiz ? 0 : NOM(i), n->mascara, &b);
        if (!s) return;
        buf_str(&b, NOM(i)); buf_str(&b, " = ");
        buf_str(&b, a); buf_str(&b, S_CDOT); buf_str(&b, c2);
        buf_str(&b, "/("); buf_str(&b, a); buf_str(&b, " + "); buf_str(&b, c2);
        buf_str(&b, ")\n= ");
        buf_sig(&b, Rn[n->h[0]]); buf_str(&b, S_CDOT); buf_sig(&b, Rn[n->h[1]]);
        buf_str(&b, "/("); buf_sig(&b, Rn[n->h[0]]); buf_str(&b, " + ");
        buf_sig(&b, Rn[n->h[1]]); buf_ch(&b, ')');
    } else {
        double g = 0.0;
        s = paso(es_raiz ? "Resistencia equivalente (paralelo)" : "Paralelo: ",
                 es_raiz ? 0 : NOM(i), n->mascara, &b);
        if (!s) return;
        /* Con muchas ramas la fórmula entera no cabe en la pantalla: se
         * escriben las tres primeras, "..." y la última. */
        int largo = n->nh > 6;
        buf_str(&b, "1/"); buf_str(&b, NOM(i)); buf_str(&b, " = ");
        for (k = 0; k < n->nh; k++) {
            if (largo && k >= 3 && k < n->nh - 1) { if (k == 3) buf_str(&b, " + ..."); continue; }
            if (k) buf_str(&b, " + ");
            buf_str(&b, "1/"); buf_str(&b, NOM(n->h[k]));
        }
        if (largo) { buf_str(&b, "  ("); buf_int(&b, n->nh); buf_str(&b, " ramas)"); }
        buf_str(&b, "\n= ");
        for (k = 0; k < n->nh; k++) {
            g += 1.0 / Rn[n->h[k]];
            if (largo && k >= 3 && k < n->nh - 1) { if (k == 3) buf_str(&b, " + ..."); continue; }
            if (k) buf_str(&b, " + ");
            buf_str(&b, "1/"); buf_sig(&b, Rn[n->h[k]]);
        }
        buf_igual(&b, g);
    }
    res_init(s, &b);
    igual_u(&b, NOM(i), Rn[i], S_OHM);
}

/* Post-orden: primero los hijos. Salta los nodos marcados en 'omitir'
 * (el camino hacia la incognita, en el problema inverso). */
static void reduce(int i, const unsigned char *omitir)
{
    const NodoAr *n = &arb.n[i];
    int k;
    if (n->tipo == AR_HOJA) return;
    for (k = 0; k < n->nh; k++) reduce(n->h[k], omitir);
    if (omitir && omitir[i]) return;
    paso_reduccion(i);
}

/* --------------------------------------------------------------------------
 *  Reparto (paso 5): con V e I de un nodo, las de sus hijos
 * ----------------------------------------------------------------------- */
#define POR_PASO 4

static void reparte(int i)
{
    const NodoAr *n = &arb.n[i];
    int k, desde;

    if (n->tipo == AR_HOJA) return;

    for (k = 0; k < n->nh; k++) {
        int h = n->h[k];
        if (n->tipo == AR_PARALELO) { Vn[h] = Vn[i]; In[h] = Vn[i] / Rn[h]; }
        else                        { In[h] = In[i]; Vn[h] = In[i] * Rn[h]; }
    }

    for (desde = 0; desde < n->nh; desde += POR_PASO) {
        int hasta = desde + POR_PASO < n->nh ? desde + POR_PASO : n->nh;
        int ultimo = (hasta == n->nh);
        Paso *s;
        Buf b;

        if (n->tipo == AR_PARALELO) {
            s = paso(desde ? "(sigue) " : "Paralelo ", NOM(i), n->mascara, &b);
            if (!s) return;
            if (!desde) { buf_str(&b, "Misma tensi" S_oo "n en cada rama: V"); buf_igual(&b, Vn[i]); buf_str(&b, " V\n"); }
            for (k = desde; k < hasta; k++) {
                int h = n->h[k];
                buf_str(&b, "I("); buf_str(&b, NOM(h)); buf_str(&b, ") = ");
                buf_sig(&b, Vn[i]); buf_ch(&b, '/'); buf_sig(&b, Rn[h]);
                buf_igual(&b, In[h]); buf_str(&b, " A\n");
            }
        } else {
            s = paso(desde ? "(sigue) " : "Serie ", NOM(i), n->mascara, &b);
            if (!s) return;
            if (!desde) { buf_str(&b, "Misma corriente en todas: I"); buf_igual(&b, In[i]); buf_str(&b, " A\n"); }
            for (k = desde; k < hasta; k++) {
                int h = n->h[k];
                buf_str(&b, "V("); buf_str(&b, NOM(h)); buf_str(&b, ") = ");
                buf_sig(&b, In[i]); buf_str(&b, S_CDOT); buf_sig(&b, Rn[h]);
                buf_igual(&b, Vn[h]); buf_str(&b, " V\n");
            }
        }
        res_init(s, &b);
        if (ultimo) {
            double suma = 0.0;
            buf_str(&b, "Suma: ");
            for (k = 0; k < n->nh; k++) {
                double v = (n->tipo == AR_PARALELO) ? In[n->h[k]] : Vn[n->h[k]];
                if (k) buf_str(&b, " + ");
                buf_sig(&b, v);
                suma += v;
            }
            buf_igual(&b, suma);
            buf_str(&b, n->tipo == AR_PARALELO ? " A " : " V ");
            buf_str(&b, S_CHECK);
        }
    }

    for (k = 0; k < n->nh; k++) reparte(n->h[k]);
}

/* --------------------------------------------------------------------------
 *  Camino del arbol
 * ----------------------------------------------------------------------- */
static int padre_de(int hijo)
{
    int i, k;
    for (i = 0; i < arb.nn; i++)
        for (k = 0; k < arb.n[i].nh; k++)
            if (arb.n[i].h[k] == hijo) return i;
    return -1;
}

/* Hoja que cuelga de la raiz: las ramas colgantes o en corto tambien tienen
 * su hoja en el array, pero no llevan corriente ni entran en el balance. */
static int hoja_viva(int i)
{
    return arb.n[i].tipo == AR_HOJA && arb.raiz >= 0 &&
           (arb.n[arb.raiz].mascara & (1u << arb.n[i].el));
}

static void calcula_R(void)
{
    int i;
    for (i = 0; i < arb.nn; i++) Rn[i] = arbol_R(&arb, i, Rel);
}

/* Nombres de las dos magnitudes de la fuente en los textos: "E" e "I" en una
 * fuente de tensión; en una de corriente, "V" (la tensión en sus bornes) y
 * "J" (la corriente que impone). Ver proced_generar. */
static const char *NE = "E", *NI = "I";
/* 1 mientras el árbol trabaja con la vista de una fuente real: su raíz es
 * r + Req (se llama Rtot) y la potencia E·I es la de la fem, no la entregada */
static int vista_real = 0;

/* Una fuente de tensión sin r (vacía o 0): en bornes, V = E */
static int sin_r(const Elemento *e)
{
    return e->aux.est == M_LIBRE || (e->aux.est == M_DATO && e->aux.v == 0.0);
}

/* Un número dentro de una cuenta: entre paréntesis si es negativo */
static void buf_sig_par(Buf *b, double v)
{
    if (v < 0.0) { buf_ch(b, '('); buf_sig(b, v); buf_ch(b, ')'); }
    else buf_sig(b, v);
}

static void paso_estructura(const Circuito *c)
{
    Paso *s;
    Buf b;
    unsigned int sin_i = arb.corto | arb.colgante;
    int k;

    s = paso("Estructura del circuito", 0, arb.n[arb.raiz].mascara, &b);
    if (!s) return;
    if (arb.n[arb.raiz].tipo == AR_HOJA) {
        buf_str(&b, "Un solo lazo: la fuente ");
        buf_str(&b, c->el[arb.fuente].etiq);
        buf_str(&b, " y la resistencia ");
        buf_str(&b, NOM(arb.raiz));
        buf_ch(&b, '.');
    } else {
        char e[PASO_TXT];
        arbol_expresion(&arb, c, arb.raiz, e, sizeof e);
        buf_str(&b, "Vista desde la fuente:\n");
        buf_str(&b, NOM(arb.raiz)); buf_str(&b, " = ");
        buf_str(&b, e);
        res_init(s, &b);
        buf_str(&b, "Se reduce paso a paso a una sola resistencia.");
    }

    if (sin_i) {
        s = paso("Sin corriente", 0, sin_i, &b);
        if (!s) return;
        for (k = 0; k < c->nel; k++) {
            if (!(sin_i & (1u << k))) continue;
            buf_str(&b, c->el[k].etiq);
            buf_str(&b, (arb.corto & (1u << k)) ? " est" S_aa " en cortocircuito. "
                                                 : " no tiene camino de vuelta. ");
        }
        res_init(s, &b);
        buf_str(&b, "No influye: se quita del circuito.");
    }
}

/* Potencia y balance: la ultima comprobacion usa una ley que no se ha
 * usado para calcular (la conservacion de la energia). */
static void paso_potencia(const Circuito *c, double E, double I)
{
    Paso *s;
    Buf b;
    double suma = 0.0;
    int k;
    unsigned int todos = arb.n[arb.raiz].mascara | (1u << arb.fuente);

    s = paso(vista_real ? "Potencia de la fem" : "Potencia", 0, 1u << arb.fuente, &b);
    if (!s) return;
    buf_str(&b, "P = "); buf_str(&b, NE); buf_str(&b, S_CDOT); buf_str(&b, NI); buf_str(&b, " = ");
    buf_sig(&b, E); buf_str(&b, S_CDOT); buf_sig(&b, I);
    res_init(s, &b);
    igual_u(&b, "P", E * I, "W");

    s = paso("Comprobaci" S_oo "n: balance de potencia", 0, todos, &b);
    if (!s) return;
    buf_str(&b, "Suma de V" S_CDOT "I en las resistencias:\n");
    for (k = 0; k < arb.nn; k++) {
        if (!hoja_viva(k)) continue;
        suma += Vn[k] * In[k];
    }
    {
        int primero = 1;
        for (k = 0; k < arb.nn; k++) {
            if (!hoja_viva(k)) continue;
            if (!primero) buf_str(&b, " + ");
            buf_sig(&b, Vn[k] * In[k]);
            primero = 0;
        }
    }
    res_init(s, &b);
    buf_str(&b, "Suma"); buf_igual(&b, suma);
    if (vista_real) buf_str(&b, " W = E" S_CDOT "I ");      /* la r también disipa */
    else { buf_str(&b, " W = P de "); buf_str(&b, c->el[arb.fuente].etiq); buf_ch(&b, ' '); }
    buf_str(&b, S_CHECK);
}

static void guarda(ValArbol *va, double E, double I)
{
    int i;
    if (!va) return;
    memset(va, 0, sizeof *va);
    va->ok = 1;
    va->E = E; va->Itot = I; va->req = Rn[arb.raiz];
    for (i = 0; i < arb.nn; i++) {
        int k = arb.n[i].el;
        if (!hoja_viva(i)) continue;
        va->V[k] = Vn[i]; va->I[k] = In[i]; va->R[k] = Rn[i];
        va->tiene |= 1u << k;
    }
    va->V[arb.fuente] = E; va->I[arb.fuente] = I;
    va->tiene |= 1u << arb.fuente;
    for (i = 0; i < cc->nel; i++)
        if ((arb.corto | arb.colgante) & (1u << i)) {
            va->I[i] = 0.0; va->R[i] = Rel[i]; va->tiene |= 1u << i;
            /* la V de una colgante no es 0: depende de a donde cuelga */
            if (arb.corto & (1u << i)) va->V[i] = 0.0;
            else va->tiene &= ~(1u << i);
        }
}

static int camino_arbol(const Circuito *c, const Red *r, ValArbol *va)
{
    const Elemento *f;
    int k, desc = -1, ndesc = 0, dato_r = -1, raiz;
    double E, I;
    Paso *s;
    Buf b;

    (void)r;
    raiz = arb.raiz;
    f = &c->el[arb.fuente];

    for (k = 0; k < c->nel; k++) {
        const Elemento *e = &c->el[k];
        if (e->tipo != EL_RES) continue;
        if (!(arb.n[raiz].mascara & (1u << k))) {
            if (e->val.est != M_DATO) return 0;    /* incognita fuera del arbol */
            Rel[k] = e->val.v;
            continue;
        }
        if (e->val.est == M_DATO) Rel[k] = e->val.v;
        else { Rel[k] = NAN_D; desc = k; ndesc++; }
        if (dato_r < 0 && (e->V.est == M_DATO || e->I.est == M_DATO)) dato_r = k;
    }
    calcula_R();

    /* ---- Directo con la fuente conocida ---- */
    if (ndesc == 0 && f->val.est == M_DATO) {
        E = fabs(f->val.v);
        paso_estructura(c);
        reduce(raiz, 0);
        I = E / Rn[raiz];
        s = paso("Ley de Ohm en el circuito", 0, (1u << arb.fuente) | arb.n[raiz].mascara, &b);
        if (s) {
            buf_str(&b, NI); buf_str(&b, " = "); buf_str(&b, NE); buf_ch(&b, '/');
            buf_str(&b, NOM(raiz)); buf_str(&b, " = ");
            buf_sig(&b, E); buf_ch(&b, '/'); buf_sig(&b, Rn[raiz]);
            res_init(s, &b); igual_u(&b, NI, I, "A");
        }
    }
    /* ---- Directo con la corriente de la fuente ---- */
    else if (ndesc == 0 && f->I.est == M_DATO) {
        I = fabs(f->I.v);
        paso_estructura(c);
        reduce(raiz, 0);
        E = I * Rn[raiz];
        s = paso("Ley de Ohm en el circuito", 0, (1u << arb.fuente) | arb.n[raiz].mascara, &b);
        if (s) {
            buf_str(&b, NE); buf_str(&b, " = "); buf_str(&b, NI); buf_str(&b, S_CDOT);
            buf_str(&b, NOM(raiz)); buf_str(&b, " = ");
            buf_sig(&b, I); buf_str(&b, S_CDOT); buf_sig(&b, Rn[raiz]);
            res_init(s, &b); igual_u(&b, NE, E, "V");
        }
    }
    /* ---- Directo con un dato en una resistencia: se sube hasta la raiz ---- */
    else if (ndesc == 0 && dato_r >= 0) {
        const Elemento *e = &c->el[dato_r];
        int h, p;
        for (h = 0; h < arb.nn; h++) if (arb.n[h].tipo == AR_HOJA && arb.n[h].el == dato_r) break;
        paso_estructura(c);
        reduce(raiz, 0);
        s = paso("Ley de Ohm en ", e->etiq, 1u << dato_r, &b);
        if (!s) return 0;
        if (e->I.est == M_DATO) {
            In[h] = fabs(e->I.v); Vn[h] = In[h] * Rn[h];
            buf_str(&b, "V = I"); buf_str(&b, S_CDOT); buf_str(&b, "R = ");
            buf_sig(&b, In[h]); buf_str(&b, S_CDOT); buf_sig(&b, Rn[h]);
            res_init(s, &b); igual_u(&b, "V", Vn[h], "V");
        } else {
            Vn[h] = fabs(e->V.v); In[h] = Vn[h] / Rn[h];
            buf_str(&b, "I = V/R = ");
            buf_sig(&b, Vn[h]); buf_ch(&b, '/'); buf_sig(&b, Rn[h]);
            res_init(s, &b); igual_u(&b, "I", In[h], "A");
        }
        for (p = padre_de(h); p >= 0; h = p, p = padre_de(p)) {
            const NodoAr *n = &arb.n[p];
            if (n->tipo == AR_SERIE) {
                In[p] = In[h]; Vn[p] = In[p] * Rn[p];
                s = paso("Hacia la fuente: serie ", NOM(p), n->mascara, &b);
                if (!s) return 0;
                buf_str(&b, "Misma corriente: I = "); buf_sig(&b, In[p]);
                buf_str(&b, " A\nV("); buf_str(&b, NOM(p)); buf_str(&b, ") = ");
                buf_sig(&b, In[p]); buf_str(&b, S_CDOT); buf_sig(&b, Rn[p]);
                res_init(s, &b); igual_u(&b, "V", Vn[p], "V");
            } else {
                Vn[p] = Vn[h]; In[p] = Vn[p] / Rn[p];
                s = paso("Hacia la fuente: paralelo ", NOM(p), n->mascara, &b);
                if (!s) return 0;
                buf_str(&b, "Misma tensi" S_oo "n: V = "); buf_sig(&b, Vn[p]);
                buf_str(&b, " V\nI("); buf_str(&b, NOM(p)); buf_str(&b, ") = ");
                buf_sig(&b, Vn[p]); buf_ch(&b, '/'); buf_sig(&b, Rn[p]);
                res_init(s, &b); igual_u(&b, "I", In[p], "A");
            }
        }
        I = In[raiz]; E = Vn[raiz];
        if (f->val.est != M_DATO) {
            s = paso("Tensi" S_oo "n de la fuente", 0, 1u << arb.fuente, &b);
            if (s) {
                buf_str(&b, NE); buf_str(&b, " = V("); buf_str(&b, NOM(raiz)); buf_str(&b, ") = ");
                buf_str(&b, NI); buf_str(&b, S_CDOT); buf_str(&b, NOM(raiz));
                res_init(s, &b); igual_u(&b, NE, E, "V");
            }
        }
        if (NI[0] == 'J' && f->I.est != M_DATO) {     /* la J que no se sabía */
            s = paso("Corriente de la fuente", 0, 1u << arb.fuente, &b);
            if (s) {
                buf_str(&b, "J = I("); buf_str(&b, NOM(raiz)); buf_ch(&b, ')');
                res_init(s, &b); igual_u(&b, "J", I, "A");
            }
        }
    }
    /* ---- Inverso: una R desconocida, con E e I de la fuente ---- */
    else if (ndesc == 1 && f->val.est == M_DATO && f->I.est == M_DATO) {
        unsigned char en_camino[ARB_MAX];
        int hoja, cam[ARB_MAX], nc = 0, j;
        double X;

        memset(en_camino, 0, sizeof en_camino);
        for (hoja = 0; hoja < arb.nn; hoja++)
            if (arb.n[hoja].tipo == AR_HOJA && arb.n[hoja].el == desc) break;
        for (j = hoja; j >= 0; j = padre_de(j)) { en_camino[j] = 1; cam[nc++] = j; }

        E = fabs(f->val.v); I = fabs(f->I.v);
        paso_estructura(c);
        reduce(raiz, en_camino);

        X = E / I;
        s = paso("Ley de Ohm en el circuito", 0, (1u << arb.fuente) | arb.n[raiz].mascara, &b);
        if (!s) return 0;
        buf_str(&b, NOM(raiz)); buf_str(&b, " = "); buf_str(&b, NE); buf_ch(&b, '/');
        buf_str(&b, NI); buf_str(&b, " = ");
        buf_sig(&b, E); buf_ch(&b, '/'); buf_sig(&b, I);
        res_init(s, &b); igual_u(&b, NOM(raiz), X, S_OHM);

        /* Bajar por el camino despejando: cam[nc-1] es la raiz */
        for (j = nc - 1; j > 0; j--) {
            int p = cam[j], h = cam[j - 1], k2;
            const NodoAr *n = &arb.n[p];
            double conocida = 0.0, Xh;

            s = paso("Despejar ", NOM(h), n->mascara, &b);
            if (!s) return 0;
            if (n->tipo == AR_PARALELO) {
                buf_str(&b, "1/"); buf_str(&b, NOM(h)); buf_str(&b, " = 1/"); buf_str(&b, NOM(p));
                buf_str(&b, n->nh > 2 ? " - (" : " - ");
                for (k2 = 0, k = 0; k < n->nh; k++) {
                    if (n->h[k] == h) continue;
                    if (k2++) buf_str(&b, " + ");
                    buf_str(&b, "1/"); buf_str(&b, NOM(n->h[k]));
                    conocida += 1.0 / Rn[n->h[k]];
                }
                buf_str(&b, n->nh > 2 ? ")\n= 1/" : "\n= 1/"); buf_sig(&b, X); buf_str(&b, " - ");
                buf_sig(&b, conocida);
                Xh = 1.0 / X - conocida;
                res_init(s, &b);
                if (!(Xh > 0.0)) { buf_str(&b, "Imposible: la rama tendr" S_ii "a R negativa."); return 0; }
                buf_str(&b, "1/"); buf_str(&b, NOM(h)); buf_igual(&b, Xh);
                buf_ch(&b, ' '); buf_str(&b, S_RARR); buf_ch(&b, ' ');
                Xh = 1.0 / Xh;
                igual_u(&b, NOM(h), Xh, S_OHM);
            } else {
                buf_str(&b, NOM(h)); buf_str(&b, " = "); buf_str(&b, NOM(p));
                buf_str(&b, n->nh > 2 ? " - (" : " - ");
                for (k2 = 0, k = 0; k < n->nh; k++) {
                    if (n->h[k] == h) continue;
                    if (k2++) buf_str(&b, " + ");
                    buf_str(&b, NOM(n->h[k]));
                    conocida += Rn[n->h[k]];
                }
                buf_str(&b, n->nh > 2 ? ")\n= " : "\n= "); buf_sig(&b, X); buf_str(&b, " - ");
                buf_sig(&b, conocida);
                Xh = X - conocida;
                res_init(s, &b);
                if (!(Xh > 0.0)) { buf_str(&b, "Imposible: la rama tendr" S_ii "a R negativa."); return 0; }
                igual_u(&b, NOM(h), Xh, S_OHM);
            }
            X = Xh;
        }
        Rel[desc] = X;
        calcula_R();

        s = paso("Comprobaci" S_oo "n", 0, (1u << arb.fuente) | arb.n[raiz].mascara, &b);
        if (s) {
            buf_str(&b, "Con "); buf_str(&b, c->el[desc].etiq); buf_igual(&b, X);
            buf_ch(&b, ' '); buf_str(&b, S_OHM); buf_str(&b, ": "); buf_str(&b, NOM(raiz)); buf_igual(&b, Rn[raiz]);
            buf_ch(&b, ' '); buf_str(&b, S_OHM);
            res_init(s, &b);
            buf_str(&b, NI); buf_str(&b, " = "); buf_str(&b, NE); buf_ch(&b, '/'); buf_str(&b, NOM(raiz)); buf_str(&b, " = ");
            buf_sig(&b, E); buf_ch(&b, '/'); buf_sig(&b, Rn[raiz]);
            buf_igual(&b, E / Rn[raiz]); buf_str(&b, " A "); buf_str(&b, S_CHECK);
        }
    }
    else return 0;

    /* ---- Repartir y potencia ---- */
    Vn[raiz] = E;
    In[raiz] = I;
    reparte(raiz);
    paso_potencia(c, E, I);
    guarda(va, E, I);
    return 1;
}

/* --------------------------------------------------------------------------
 *  Propagacion por el arbol: para los problemas que no son de los cuatro
 *  esquemas de arriba (un dato en una resistencia y la incognita en otra,
 *  dos datos repartidos...). Es lo que hace un estudiante: en cada paso,
 *  una relacion con UNA sola cosa desconocida:
 *
 *    - Ohm en un tramo:            dos de {V, I, R} -> la tercera
 *    - serie:                      misma I en todos; V y R suman
 *    - paralelo:                   misma V en todos; I y 1/R suman
 *
 *  Si se atasca antes de saberlo todo, se pasa a Kirchhoff.
 * ----------------------------------------------------------------------- */
static unsigned char kR[ARB_MAX], kV[ARB_MAX], kI[ARB_MAX];
static int orden[ARB_MAX], norden;          /* nodos vivos en preorden */

static void preorden(int i)
{
    int k;
    orden[norden++] = i;
    if (arb.n[i].tipo == AR_HOJA) return;
    for (k = 0; k < arb.n[i].nh; k++) preorden(arb.n[i].h[k]);
}

static unsigned int mascara_de(int i)
{
    unsigned int m = arb.n[i].mascara;
    if (i == arb.raiz) m |= 1u << arb.fuente;
    return m;
}

static void nom_mag(Buf *b, char q, int i)
{
    if (q == 'R') { buf_str(b, NOM(i)); return; }
    buf_ch(b, q); buf_ch(b, '('); buf_str(b, NOM(i)); buf_ch(b, ')');
}

static const char *unidad_q(char q) { return q == 'V' ? "V" : q == 'I' ? "A" : S_OHM; }

/* Regla 1: reducir un tramo cuyos hijos se conocen todos */
static int p_reduce(void)
{
    int j, k;
    for (j = norden - 1; j >= 0; j--) {
        int i = orden[j];
        const NodoAr *n = &arb.n[i];
        if (n->tipo == AR_HOJA || kR[i]) continue;
        for (k = 0; k < n->nh; k++) if (!kR[n->h[k]]) break;
        if (k < n->nh) continue;
        Rn[i] = 0.0;
        for (k = 0; k < n->nh; k++)
            Rn[i] += (n->tipo == AR_SERIE) ? Rn[n->h[k]] : 1.0 / Rn[n->h[k]];
        if (n->tipo == AR_PARALELO) Rn[i] = 1.0 / Rn[i];
        kR[i] = 1;
        paso_reduccion(i);
        return 1;
    }
    return 0;
}

/* Regla 2: Ohm en un tramo con dos de las tres magnitudes */
static int p_ohm(void)
{
    int j;
    for (j = 0; j < norden; j++) {
        int i = orden[j];
        Paso *s;
        Buf b;
        char q;
        double v;
        if (kR[i] + kV[i] + kI[i] != 2) continue;

        s = paso("Ley de Ohm en ", NOM(i), mascara_de(i), &b);
        if (!s) return 0;
        if (!kV[i]) {
            q = 'V'; v = In[i] * Rn[i]; Vn[i] = v; kV[i] = 1;
            buf_str(&b, "V = I" S_CDOT "R = "); buf_sig(&b, In[i]); buf_str(&b, S_CDOT); buf_sig(&b, Rn[i]);
        } else if (!kI[i]) {
            q = 'I'; v = Vn[i] / Rn[i]; In[i] = v; kI[i] = 1;
            buf_str(&b, "I = V/R = "); buf_sig(&b, Vn[i]); buf_ch(&b, '/'); buf_sig(&b, Rn[i]);
        } else {
            q = 'R'; v = Vn[i] / In[i]; Rn[i] = v; kR[i] = 1;
            buf_str(&b, "R = V/I = "); buf_sig(&b, Vn[i]); buf_ch(&b, '/'); buf_sig(&b, In[i]);
        }
        res_init(s, &b);
        nom_mag(&b, q, i); buf_igual(&b, v); buf_ch(&b, ' '); buf_str(&b, unidad_q(q));
        return 1;
    }
    return 0;
}

/* Regla 3: la misma I en una serie, la misma V en un paralelo */
static int p_igual(void)
{
    int j, k;
    for (j = 0; j < norden; j++) {
        int i = orden[j], fuente_i = -1, faltan = 0;
        const NodoAr *n = &arb.n[i];
        unsigned char *kk;
        double *val;
        char q;
        Paso *s;
        Buf b;

        if (n->tipo == AR_HOJA) continue;
        q = (n->tipo == AR_SERIE) ? 'I' : 'V';
        kk = (q == 'I') ? kI : kV;
        val = (q == 'I') ? In : Vn;
        if (kk[i]) fuente_i = i;
        for (k = 0; k < n->nh; k++) {
            if (kk[n->h[k]]) { if (fuente_i < 0) fuente_i = n->h[k]; }
            else faltan++;
        }
        if (!kk[i]) faltan++;
        if (fuente_i < 0 || faltan == 0) continue;

        s = paso(q == 'I' ? "Serie " : "Paralelo ", NOM(i), n->mascara, &b);
        if (!s) return 0;
        buf_str(&b, q == 'I' ? "En serie todos llevan la misma corriente:\n"
                             : "En paralelo todos tienen la misma tensi" S_oo "n:\n");
        nom_mag(&b, q, fuente_i);
        buf_igual(&b, val[fuente_i]); buf_ch(&b, ' '); buf_str(&b, unidad_q(q));
        res_init(s, &b);
        {
            int primero = 1;
            if (!kk[i]) { nom_mag(&b, q, i); primero = 0; }
            for (k = 0; k < n->nh; k++)
                if (!kk[n->h[k]]) {
                    if (!primero) buf_str(&b, " = ");
                    nom_mag(&b, q, n->h[k]);
                    primero = 0;
                }
            buf_igual(&b, val[fuente_i]); buf_ch(&b, ' '); buf_str(&b, unidad_q(q));
        }
        val[i] = val[fuente_i]; kk[i] = 1;
        for (k = 0; k < n->nh; k++) { val[n->h[k]] = val[fuente_i]; kk[n->h[k]] = 1; }
        return 1;
    }
    return 0;
}

/* Reglas 4 y 5: una suma con un solo termino desconocido. En serie suman
 * V y R; en paralelo suman I y 1/R. */
static int p_suma(int de_R)
{
    int j, k;
    for (j = 0; j < norden; j++) {
        int i = orden[j], falta = -1, nf = 0;
        const NodoAr *n = &arb.n[i];
        unsigned char *kk;
        double *val, suma = 0.0, x;
        int inv;                         /* suma de inversos: 1/R en paralelo */
        char q;
        Paso *s;
        Buf b;

        if (n->tipo == AR_HOJA) continue;
        if (de_R) { q = 'R'; kk = kR; val = Rn; inv = (n->tipo == AR_PARALELO); }
        else if (n->tipo == AR_SERIE) { q = 'V'; kk = kV; val = Vn; inv = 0; }
        else { q = 'I'; kk = kI; val = In; inv = 0; }

        if (!kk[i]) { falta = i; nf++; }
        for (k = 0; k < n->nh; k++) if (!kk[n->h[k]]) { falta = n->h[k]; nf++; }
        if (nf != 1) continue;
        if (de_R && falta == i) continue;               /* eso es p_reduce */

        for (k = 0; k < n->nh; k++)
            if (n->h[k] != falta) suma += inv ? 1.0 / val[n->h[k]] : val[n->h[k]];

        if (falta == i) {
            x = suma;
            s = paso(n->tipo == AR_SERIE ? "Serie " : "Paralelo ", NOM(i), n->mascara, &b);
            if (!s) return 0;
            buf_str(&b, q == 'V' ? "Las tensiones en serie suman:\n" : "Las corrientes en paralelo suman:\n");
            nom_mag(&b, q, i); buf_str(&b, " = ");
            for (k = 0; k < n->nh; k++) { if (k) buf_str(&b, " + "); buf_sig(&b, val[n->h[k]]); }
        } else {
            double tot = inv ? 1.0 / val[i] : val[i];
            x = tot - suma;
            s = paso(de_R ? "Despejar " : (n->tipo == AR_SERIE ? "Serie " : "Paralelo "),
                     de_R ? NOM(falta) : NOM(i), n->mascara, &b);
            if (!s) return 0;
            if (!de_R) buf_str(&b, q == 'V' ? "Las tensiones en serie suman:\n"
                                            : "Las corrientes en paralelo suman:\n");
            if (inv) buf_str(&b, "1/");
            nom_mag(&b, q, falta); buf_str(&b, inv ? " = 1/" : " = ");
            nom_mag(&b, q, i); buf_str(&b, n->nh > 2 ? " - (" : " - ");
            {
                int prim = 1;
                for (k = 0; k < n->nh; k++) {
                    if (n->h[k] == falta) continue;
                    if (!prim) buf_str(&b, " + ");
                    if (inv) buf_str(&b, "1/");
                    nom_mag(&b, q, n->h[k]);
                    prim = 0;
                }
            }
            buf_str(&b, n->nh > 2 ? ")\n= " : "\n= ");
            if (inv) buf_str(&b, "1/");
            buf_sig(&b, val[i]); buf_str(&b, " - "); buf_sig(&b, suma);
            if (!(x > 0.0)) {
                res_init(s, &b);
                buf_str(&b, "Imposible: saldr" S_ii "a cero o negativo.");
                return -1;
            }
            if (inv) {
                res_init(s, &b);
                buf_str(&b, "1/"); nom_mag(&b, q, falta); buf_igual(&b, x);
                buf_ch(&b, ' '); buf_str(&b, S_RARR); buf_ch(&b, ' ');
                x = 1.0 / x;
                nom_mag(&b, q, falta); buf_igual(&b, x); buf_ch(&b, ' '); buf_str(&b, unidad_q(q));
                val[falta] = x; kk[falta] = 1;
                return 1;
            }
        }
        res_init(s, &b);
        nom_mag(&b, q, falta); buf_igual(&b, x); buf_ch(&b, ' '); buf_str(&b, unidad_q(q));
        val[falta] = x; kk[falta] = 1;
        return 1;
    }
    return 0;
}

/* Regla 6: la suma con los tramos de R conocida agrupados. En una serie con
 * V total conocida y la corriente sin saber, si cada tramo tiene su V o su
 * R conocida:
 *     V(X) = suma de V conocidas + I · (suma de R conocidas)
 *     I = (V(X) - suma de V) / (suma de R)
 * y lo mismo en paralelo con las corrientes y 1/R:
 *     V = (I(X) - suma de I) / (suma de 1/R)                              */
static int p_mixta(void)
{
    int j, k;
    for (j = 0; j < norden; j++) {
        int i = orden[j], nR = 0, bien = 1, serie;
        const NodoAr *n = &arb.n[i];
        double sv = 0.0, sr = 0.0, x;
        Paso *s;
        Buf b;
        int primero;

        if (n->tipo == AR_HOJA) continue;
        serie = (n->tipo == AR_SERIE);
        if (serie ? (kI[i] || !kV[i]) : (kV[i] || !kI[i])) continue;
        for (k = 0; k < n->nh; k++) {
            int h = n->h[k];
            int conoce = serie ? kV[h] : kI[h];
            if (conoce) sv += serie ? Vn[h] : In[h];
            else if (kR[h]) { sr += serie ? Rn[h] : 1.0 / Rn[h]; nR++; }
            else { bien = 0; break; }
        }
        if (!bien || nR == 0) continue;

        s = paso(serie ? "Serie " : "Paralelo ", NOM(i), n->mascara, &b);
        if (!s) return 0;
        if (serie) {
            buf_str(&b, "Misma I en todas; las R conocidas se agrupan:\n");
            nom_mag(&b, 'V', i); buf_str(&b, " = ");
            primero = 1;
            for (k = 0; k < n->nh; k++) if (kV[n->h[k]]) {
                if (!primero) buf_str(&b, " + ");
                nom_mag(&b, 'V', n->h[k]); primero = 0;
            }
            if (!primero) buf_str(&b, " + ");
            buf_str(&b, "I" S_CDOT "(");
            primero = 1;
            for (k = 0; k < n->nh; k++) if (!kV[n->h[k]]) {
                if (!primero) buf_str(&b, " + ");
                buf_str(&b, NOM(n->h[k])); primero = 0;
            }
            buf_str(&b, ")\nI = (");
            buf_sig(&b, Vn[i]); buf_str(&b, " - "); buf_sig(&b, sv);
            buf_str(&b, ") / "); buf_sig(&b, sr);
            x = (Vn[i] - sv) / sr;
        } else {
            buf_str(&b, "Misma V en todas; I = V/R en las de R conocida:\n");
            nom_mag(&b, 'I', i); buf_str(&b, " = ");
            primero = 1;
            for (k = 0; k < n->nh; k++) if (kI[n->h[k]]) {
                if (!primero) buf_str(&b, " + ");
                nom_mag(&b, 'I', n->h[k]); primero = 0;
            }
            if (!primero) buf_str(&b, " + ");
            buf_str(&b, "V" S_CDOT "(");
            primero = 1;
            for (k = 0; k < n->nh; k++) if (!kI[n->h[k]]) {
                if (!primero) buf_str(&b, " + ");
                buf_str(&b, "1/"); buf_str(&b, NOM(n->h[k])); primero = 0;
            }
            buf_str(&b, ")\nV = (");
            buf_sig(&b, In[i]); buf_str(&b, " - "); buf_sig(&b, sv);
            buf_str(&b, ") / "); buf_sig(&b, sr);
            x = (In[i] - sv) / sr;
        }
        res_init(s, &b);
        if (!(x > 0.0)) { buf_str(&b, "Imposible: saldría cero o negativo."); return -1; }
        buf_str(&b, serie ? "I" : "V");
        buf_igual(&b, x);
        buf_str(&b, serie ? " A" : " V");
        if (serie) { In[i] = x; kI[i] = 1; }
        else       { Vn[i] = x; kV[i] = 1; }
        return 1;
    }
    return 0;
}

static int camino_propaga(const Circuito *c, ValArbol *va)
{
    const Elemento *f = &c->el[arb.fuente];
    int raiz = arb.raiz, j, k, r, completo;
    double E, I;
    Paso *s;
    Buf b;

    norden = 0;
    preorden(raiz);
    memset(kR, 0, sizeof kR); memset(kV, 0, sizeof kV); memset(kI, 0, sizeof kI);

    for (k = 0; k < c->nel; k++) {
        const Elemento *e = &c->el[k];
        if (e->tipo != EL_RES) continue;
        if (!(arb.n[raiz].mascara & (1u << k))) {
            if (e->val.est != M_DATO) return 0;          /* fuera del arbol */
            Rel[k] = e->val.v;
        }
    }
    for (j = 0; j < norden; j++) {
        int i = orden[j];
        const Elemento *e;
        if (arb.n[i].tipo != AR_HOJA) continue;
        e = &c->el[arb.n[i].el];
        if (e->val.est == M_DATO) { Rn[i] = e->val.v; kR[i] = 1; }
        if (e->V.est == M_DATO)   { Vn[i] = fabs(e->V.v); kV[i] = 1; }
        if (e->I.est == M_DATO)   { In[i] = fabs(e->I.v); kI[i] = 1; }
    }
    if (f->val.est == M_DATO) { Vn[raiz] = fabs(f->val.v); kV[raiz] = 1; }
    else if (NE[0] == 'E' && f->V.est == M_DATO && sin_r(f)) { Vn[raiz] = fabs(f->V.v); kV[raiz] = 1; }
    if (f->I.est == M_DATO)   { In[raiz] = fabs(f->I.v);   kI[raiz] = 1; }

    paso_estructura(c);
    if (kV[raiz] || kI[raiz]) {
        s = paso("Datos de la fuente", 0, 1u << arb.fuente, &b);
        if (s) {
            buf_str(&b, "La fuente est" S_aa " conectada a los extremos de ");
            buf_str(&b, NOM(raiz)); buf_ch(&b, ':');
            res_init(s, &b);
            if (kV[raiz]) { buf_str(&b, "V("); buf_str(&b, NOM(raiz)); buf_str(&b, ") = "); buf_str(&b, NE); buf_igual(&b, Vn[raiz]); buf_str(&b, " V  "); }
            if (kI[raiz]) { buf_str(&b, "I("); buf_str(&b, NOM(raiz)); buf_str(&b, ")"); buf_igual(&b, In[raiz]); buf_str(&b, " A"); }
        }
    }

    for (;;) {
        completo = 1;
        for (j = 0; j < norden; j++)
            if (!(kR[orden[j]] && kV[orden[j]] && kI[orden[j]])) { completo = 0; break; }
        if (completo) break;
        if (p_reduce()) continue;
        if (p_ohm()) continue;
        if (p_igual()) continue;
        r = p_suma(0); if (r < 0) return 0; if (r) continue;
        r = p_mixta(); if (r < 0) return 0; if (r) continue;
        r = p_suma(1); if (r < 0) return 0; if (r) continue;
        return 0;                                          /* atascado */
    }

    E = Vn[raiz];
    I = In[raiz];
    if (f->val.est != M_DATO) {
        s = paso("Tensi" S_oo "n de la fuente", 0, 1u << arb.fuente, &b);
        if (s) {
            buf_str(&b, NE); buf_str(&b, " = V("); buf_str(&b, NOM(raiz)); buf_ch(&b, ')');
            res_init(s, &b); igual_u(&b, NE, E, "V");
        }
    }
    if (NI[0] == 'J' && f->I.est != M_DATO) {         /* la J que no se sabía */
        s = paso("Corriente de la fuente", 0, 1u << arb.fuente, &b);
        if (s) {
            buf_str(&b, "J = I("); buf_str(&b, NOM(raiz)); buf_ch(&b, ')');
            res_init(s, &b); igual_u(&b, "J", I, "A");
        }
    }
    for (j = 0; j < norden; j++) {
        int i = orden[j];
        if (arb.n[i].tipo == AR_HOJA) Rel[arb.n[i].el] = Rn[i];
    }
    paso_potencia(c, E, I);
    guarda(va, E, I);
    return 1;
}

/* --------------------------------------------------------------------------
 *  Caso general: Kirchhoff con los numeros y la solucion del motor
 * ----------------------------------------------------------------------- */
#define POR_PASO_K 6

static void nombre_i(Buf *b, const Elemento *e) { buf_ch(b, 'I'); buf_str(b, e->etiq); }
static void nombre_v(Buf *b, const Red *r, int n) { char t[3] = { 'V', 0, 0 }; t[1] = red_letra(r, n); buf_str(b, t); }

static void kirchhoff(const Circuito *c, const Red *r, const Valores *v)
{
    Paso *s;
    Buf b;
    int n, k, prim, p, pmax = r->nn + 2;
    unsigned int m;

    /* Los nodos, en el orden de sus letras (con terminales, A y B son los
     * suyos: red_letra); un puesto sin nodo (la B de un terminal puesto en
     * el nodo de A) se salta */
    s = paso("Leyes de Kirchhoff", 0, 0, &b);
    if (!s) return;
    buf_str(&b, "Se plantean las leyes de nodos y la ley de cada elemento en todo el circuito.\nNodos: ");
    for (p = 0, prim = 1; p < pmax; p++) {
        if ((n = red_nodo_puesto(r, p)) < 0) continue;
        if (!prim) buf_str(&b, ", ");
        buf_ch(&b, red_letra(r, n));
        prim = 0;
    }
    res_init(s, &b);
    buf_str(&b, "Referencia (0 V):");
    for (p = 0; p < pmax; p++) if ((n = red_nodo_puesto(r, p)) >= 0 && r->ref[n]) { buf_ch(&b, ' '); buf_ch(&b, red_letra(r, n)); }

    for (p = 0; p < pmax; p++) {
        char t[12] = "Nodo ";
        if ((n = red_nodo_puesto(r, p)) < 0 || r->ref[n]) continue;
        t[5] = red_letra(r, n); t[6] = 0;
        m = 0;
        for (k = 0; k < r->nel; k++) if (r->a[k] == n || r->b[k] == n) m |= 1u << k;
        s = paso(t, ": lo que entra = lo que sale", m, &b);
        if (!s) return;
        s->nodo = n;                                       /* el esquema resalta su letra y sus puntos */
        prim = 1;
        for (k = 0; k < r->nel; k++) {
            if (r->a[k] == r->b[k]) continue;
            if (r->b[k] == n) { if (!prim) buf_str(&b, "+"); nombre_i(&b, &c->el[k]); prim = 0; }
        }
        if (prim) buf_ch(&b, '0');
        buf_str(&b, " = ");
        prim = 1;
        for (k = 0; k < r->nel; k++) {
            if (r->a[k] == r->b[k]) continue;
            if (r->a[k] == n) { if (!prim) buf_str(&b, "+"); nombre_i(&b, &c->el[k]); prim = 0; }
        }
        if (prim) buf_ch(&b, '0');
    }

    /* Una ecuacion por linea y como mucho POR_PASO_K lineas por paso: el
     * panel de la calculadora da seis al detalle. Cada paso resalta los
     * elementos de sus propias ecuaciones. */
    {
        int en_paso = 0;
        for (k = 0; k < r->nel; k++) {
            const Elemento *e = &c->el[k];
            int que;
            for (que = 0; que < 3; que++) {
                /* 0 = ley del elemento, 1 = dato de V, 2 = dato de I */
                /* la ley de una fuente real se escribe siempre (con E si no se sabe) */
                if (que == 0 && !circ_abierto_cc(e) && !circ_corto_cc(e) && e->val.est != M_DATO &&
                    !(e->tipo == EL_FUENTE && !sin_r(e))) continue;
                if (que == 1 && e->V.est != M_DATO) continue;
                if (que == 2 && e->I.est != M_DATO) continue;
                if (en_paso == 0 || en_paso == POR_PASO_K) {
                    s = paso(en_paso ? "(sigue) Ley de cada elemento y datos"
                                     : "Ley de cada elemento y datos", 0, 0, &b);
                    if (!s) return;
                    en_paso = 0;
                }
                s->resalta |= 1u << k;
                if (que == 0 && circ_abierto_cc(e)) {                     /* no deja pasar corriente */
                    nombre_i(&b, e);
                    buf_str(&b, e->tipo == EL_VOLT ? " = 0 (volt" S_ii "metro ideal)" :
                                tipo_info(e->tipo)->ley_cc == LEY_ABIERTO ? " = 0 (condensador: abierto en CC)" :
                                " = 0 (interruptor abierto)");
                } else if (que == 0 && circ_corto_cc(e)) {                /* es un cable */
                    nombre_v(&b, r, r->a[k]); buf_str(&b, " = "); nombre_v(&b, r, r->b[k]);
                    buf_str(&b, e->tipo == EL_AMP ? " (amper" S_ii "metro ideal)" :
                                e->tipo == EL_VATI ? " (vat" S_ii "metro: su bobina de corriente es un cable)" :
                                tipo_info(e->tipo)->ley_cc == LEY_CORTO ? " (bobina: cable en CC)" :
                                " (interruptor cerrado)");
                } else if (que == 0 && e->tipo != EL_FUENTE && e->tipo != EL_CORR) {
                    nombre_v(&b, r, r->a[k]); buf_str(&b, " - "); nombre_v(&b, r, r->b[k]);
                    buf_str(&b, " = "); buf_sig(&b, e->val.v); buf_str(&b, S_CDOT); nombre_i(&b, e);
                } else if (que == 0 && e->tipo == EL_CORR) {     /* la J fija su corriente */
                    nombre_i(&b, e); buf_str(&b, " = "); buf_sig(&b, e->val.v);
                } else if (que == 0 && e->tipo == EL_FUENTE && !sin_r(e)) {
                    /* fuente real: en bornes, E - r I */
                    nombre_v(&b, r, r->b[k]); buf_str(&b, " - "); nombre_v(&b, r, r->a[k]);
                    buf_str(&b, " = ");
                    if (e->val.est == M_DATO) buf_sig(&b, e->val.v); else buf_ch(&b, 'E');
                    buf_str(&b, " - ");
                    if (e->aux.est == M_DATO) buf_sig(&b, e->aux.v); else buf_str(&b, "r");
                    buf_str(&b, S_CDOT); nombre_i(&b, e);
                } else if (que == 0 || que == 1) {
                    int fu = tipo_info(e->tipo)->fuente;
                    int pa = fu ? r->b[k] : r->a[k];
                    int pb = fu ? r->a[k] : r->b[k];
                    nombre_v(&b, r, pa); buf_str(&b, " - "); nombre_v(&b, r, pb);
                    buf_str(&b, " = ");
                    if (que == 1 && e->tipo != EL_FUENTE) buf_str(&b, S_PM);
                    buf_sig(&b, que == 0 ? e->val.v : fabs(e->V.v));
                } else {
                    nombre_i(&b, e); buf_str(&b, " = " S_PM); buf_sig(&b, fabs(e->I.v));
                }
                buf_str(&b, "\n");
                en_paso++;
            }
        }
    }

    for (k = 0; k < r->nel; k++) {
        if (k % POR_PASO_K == 0) {
            s = paso(k ? "(sigue) Resolviendo el sistema" : "Resolviendo el sistema", 0, 0, &b);
            if (!s) return;
        }
        s->resalta |= 1u << k;
        nombre_i(&b, &c->el[k]); buf_igual(&b, v->I[k]); buf_str(&b, " A\n");
    }
    for (k = 0; k < r->nel; k++) {
        const Elemento *e = &c->el[k];
        if (circ_val_en_ley(e) && e->val.est != M_DATO) {
            s = paso(!tipo_info(e->tipo)->fuente ? "Resistencia desconocida " : "Fuente desconocida ",
                     e->etiq, 1u << k, &b);
            if (!s) return;
            if (!tipo_info(e->tipo)->fuente) {
                buf_str(&b, "R = V/I = "); buf_sig(&b, fabs(v->V[k])); buf_ch(&b, '/'); buf_sig(&b, fabs(v->I[k]));
                res_init(s, &b); igual_u(&b, e->etiq, v->val[k], S_OHM);
            } else if (e->tipo == EL_CORR) {
                buf_str(&b, "J = "); nombre_i(&b, e);
                res_init(s, &b); igual_u(&b, e->etiq, v->val[k], "A");
            } else if (!sin_r(e)) {
                /* real: en bornes queda E - r·I, así que E = V + r·I */
                buf_str(&b, "E = V(+) - V(-) + r" S_CDOT "I = ");
                buf_sig_par(&b, v->V[k]); buf_str(&b, " + ");
                buf_sig(&b, v->aux[k]); buf_str(&b, S_CDOT); buf_sig_par(&b, v->I[k]);
                res_init(s, &b); igual_u(&b, e->etiq, v->val[k], "V");
            } else {
                buf_str(&b, "E = V(+) - V(-)");
                res_init(s, &b); igual_u(&b, e->etiq, v->val[k], "V");
            }
        }
        if (e->tipo == EL_VATI) {
            /* lo que marca: la V de su bobina de tensión (de b a la
             * referencia) por la I de su bobina de corriente */
            s = paso("Lo que marca ", e->etiq, 1u << k, &b);
            if (!s) return;
            buf_str(&b, "P = V(b)" S_CDOT "I = "); buf_sig_par(&b, v->V[k]); buf_str(&b, S_CDOT); buf_sig_par(&b, v->I[k]);
            res_init(s, &b); igual_u(&b, "P", v->val[k], "W");
        }
        if ((e->tipo == EL_COND || e->tipo == EL_BOB) && e->val.est == M_DATO) {
            /* lo que guarda: carga o flujo y energía */
            s = paso("Energ" S_ii "a almacenada en ", e->etiq, 1u << k, &b);
            if (!s) return;
            if (e->tipo == EL_COND) {
                buf_str(&b, "Q = C" S_CDOT "V = "); buf_sig(&b, e->val.v); buf_str(&b, S_CDOT); buf_sig_par(&b, v->V[k]);
                buf_str(&b, "\nW = " "\xBD" S_CDOT "C" S_CDOT "V" S_SUP2);
                res_init(s, &b);
                igual_u(&b, "Q", e->val.v * v->V[k], "C");
                buf_str(&b, "  ");
                igual_u(&b, "W", 0.5 * e->val.v * v->V[k] * v->V[k], "J");
            } else {
                buf_str(&b, S_PHI " = L" S_CDOT "I = "); buf_sig(&b, e->val.v); buf_str(&b, S_CDOT); buf_sig_par(&b, v->I[k]);
                buf_str(&b, "\nW = " "\xBD" S_CDOT "L" S_CDOT "I" S_SUP2);
                res_init(s, &b);
                igual_u(&b, S_PHI, e->val.v * v->I[k], "Wb");
                buf_str(&b, "  ");
                igual_u(&b, "W", 0.5 * e->val.v * v->I[k] * v->I[k], "J");
            }
        }
        if (e->tipo == EL_FUENTE && e->aux.est == M_BUSCAR) {
            char t[8] = "r";
            strncpy(t + 1, e->etiq + 1, sizeof t - 2);            /* r1 para E1 */
            s = paso("r interna desconocida de ", e->etiq, 1u << k, &b);
            if (!s) return;
            buf_str(&b, "r = (E - V)/I = (");
            buf_sig(&b, v->val[k]); buf_str(&b, " - "); buf_sig_par(&b, v->V[k]);
            buf_str(&b, ")/"); buf_sig_par(&b, v->I[k]);
            res_init(s, &b); igual_u(&b, t, v->aux[k], S_OHM);
        }
    }
}

/* ----------------------------------------------------------------------- */
/* La única fuente, si es una fuente de corriente; si no, -1 */
static int unica_j(const Circuito *c)
{
    int k, nf = 0, j = -1;
    for (k = 0; k < c->nel; k++)
        if (tipo_info(c->el[k].tipo)->fuente) { nf++; if (c->el[k].tipo == EL_CORR) j = k; }
    return nf == 1 ? j : -1;
}

/* La única fuente, cualquiera que sea; -1 si hay varias o ninguna */
static int unica_fuente(const Circuito *c)
{
    int k, nf = 0, f = -1;
    for (k = 0; k < c->nel; k++) if (tipo_info(c->el[k].tipo)->fuente) { nf++; f = k; }
    return nf == 1 ? f : -1;
}

int proced_generar(const Circuito *c, const Red *r, const Solucion *s,
                   int isol, Pasos *p, ValArbol *va)
{
    static Circuito vista;
    static Red rvista;
    const Circuito *ct = c;
    const Red *rt = r;
    int j, f, kr = -1;

    pp = p;
    pasos_init(p);
    if (va) memset(va, 0, sizeof *va);
    if (s->estado != SOL_OK || isol < 0 || isol >= s->nsol) return PROC_NADA;
    /* Con una fuente controlada no hay procedimiento de serie-paralelo (D25) */
    for (j = 0; j < c->nel; j++) if (tipo_info(c->el[j].tipo)->ley_cc == LEY_DEP) return PROC_NADA;

    /* Con una fuente de corriente sola, la pizarra es la misma que con una
     * de tensión con los papeles cambiados: se conoce la corriente (J) y se
     * busca la tensión (V = J·Req). El árbol la ve como una fuente cuya "E"
     * es la V de sus bornes y cuya "I" es J; los textos dicen V y J. */
    NE = "E"; NI = "I";
    vista_real = 0;
    j = unica_j(c);
    if (j >= 0) {
        vista = *c;
        vista.el[j].tipo = EL_FUENTE;
        vista.el[j].val = c->el[j].V;
        vista.el[j].I = c->el[j].val;
        vista.el[j].V.est = M_LIBRE;
        ct = &vista;
        NE = "V"; NI = "J";
    }

    /* Una fuente real (E, r) sola: la pizarra pone la r como una resistencia
     * más, en serie: I = E/(r + Req). El árbol la ve así, con un nodo entre
     * la E ideal y la r. Lo que se compara con el motor (V en bornes, Req
     * vista desde los bornes) se corrige al final. */
    f = unica_fuente(c);
    if (f >= 0 && c->el[f].tipo == EL_FUENTE && !sin_r(&c->el[f]) &&
        (c->nel >= MAX_EL || r->nn >= MAX_NODOS))
        f = -2;                          /* no cabe la vista: sin árbol (daría la r por 0) */
    else if (f >= 0 && c->el[f].tipo == EL_FUENTE && !sin_r(&c->el[f])) {
        Elemento *e;
        const char *d = c->el[f].etiq + 1;
        int i;
        vista = *c;
        rvista = *r;
        kr = vista.nel++;
        e = &vista.el[kr];
        memset(e, 0, sizeof *e);
        e->tipo = EL_RES;
        e->ref = -1;
        e->etiq[0] = 'r';
        for (i = 1; i < 5 && *d; i++) e->etiq[i] = *d++;   /* r1 para E1 */
        e->val = c->el[f].aux;
        rvista.nel = vista.nel;
        rvista.a[kr] = rvista.nn;                        /* el nodo interno */
        rvista.b[kr] = r->b[f];
        rvista.b[f] = rvista.nn;
        rvista.nn++;
        red_cierra(&vista, &rvista);
        rvista.nodo_term[0] = r->nodo_term[0];          /* las letras, las del circuito */
        rvista.nodo_term[1] = r->nodo_term[1];
        rvista.letras_term = r->letras_term;
        ct = &vista;
        rt = &rvista;
    }
    cc = ct;

    if (f != -2 && arbol_construir(ct, rt, &arb)) {
        if (kr >= 0) {
            vista_real = 1;
            if (arb.n[arb.raiz].tipo != AR_HOJA) strcpy(arb.n[arb.raiz].nombre, "Rtot");
        }
        int ok = camino_arbol(ct, rt, va);
        if (!ok) {
            pasos_init(p);
            if (va) memset(va, 0, sizeof *va);
            ok = camino_propaga(ct, va);
        }
        if (ok) {
            if (kr >= 0) {
                /* La tensión que queda en los bornes: lo que se pierde en r */
                const Valores *v = &s->s[isol];
                Buf b;
                Paso *ps = paso("Tensi" S_oo "n en bornes de ", c->el[f].etiq, 1u << f, &b);
                if (ps) {
                    double Vb = fabs(v->V[f]), Ib = fabs(v->I[f]);
                    buf_str(&b, "Dentro se pierde r" S_CDOT "I:\nV = E - r" S_CDOT "I = ");
                    buf_sig(&b, fabs(v->val[f])); buf_str(&b, " - ");
                    buf_sig(&b, fabs(v->aux[f])); buf_str(&b, S_CDOT); buf_sig(&b, Ib);
                    buf_str(&b, "\nEntregada al circuito: V" S_CDOT "I = "); buf_sig(&b, Vb * Ib);
                    buf_str(&b, " W\nPerdida en r: r" S_CDOT "I" S_SUP2 " = "); buf_sig(&b, fabs(v->aux[f]) * Ib * Ib);
                    buf_str(&b, " W");
                    res_init(ps, &b); igual_u(&b, "V", Vb, "V");
                }
            }
            if (kr >= 0 && va && va->ok) {
                double rr = va->R[kr];
                va->V[f] = fabs(va->V[f] - rr * va->I[f]);    /* en bornes: E - r I */
                va->req -= rr;                                 /* vista desde los bornes */
                va->tiene &= ~(1u << kr);
            }
            return PROC_ARBOL;
        }
    }
    cc = c;
    NE = "E"; NI = "I";
    vista_real = 0;

    pasos_init(p);
    if (va) memset(va, 0, sizeof *va);
    kirchhoff(c, r, &s->s[isol]);
    return PROC_KIRCHHOFF;
}
