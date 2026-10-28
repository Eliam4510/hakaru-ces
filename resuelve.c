/* ============================================================================
 *  resuelve.c  -  El motor  (ver resuelve.h)
 * ========================================================================= */
#include <string.h>
#include <math.h>
#include "resuelve.h"
#include "algebra.h"
#include "numfmt.h"
#include "gfx.h"

/* Un dato medido: la V o la I de un elemento, en magnitud */
typedef struct { int k; int es_i; double m; } Dato;
typedef char caben_los_datos[MAX_DATOS + 1 <= MAX_TERM ? 1 : -1];

static Sistema sis;                    /* 100 KB: estatico, fuera de la pila */
static double  x[MAX_UNK];
static unsigned char libre[MAX_UNK];

/* Columna del potencial de cada nodo (-1 = referencia, vale 0 V) */
static int col_phi[MAX_NODOS];
static int col_i0;                     /* columna de I del elemento 0        */
/* Fuente real: E ideal y r en serie, con un nodo entre las dos. Su
 * potencial es una incógnita más (-1 si la fuente es ideal). */
static int col_int[MAX_EL];

/* La r de una fuente real (dato o incógnita); vacía = ideal */
static int con_r(const Elemento *el) { return el->tipo == EL_FUENTE && el->aux.est != M_LIBRE; }

static void coef_phi(int eq, int n, double c)
{
    if (n >= 0 && col_phi[n] >= 0) sis_coef(&sis, eq, col_phi[n], c);
}

/* Un instrumento sin R interna: ideal. No es una incógnita. */
static int es_ideal(const Elemento *el) { return ES_IDEAL(el); }

/* R, voltímetro y amperímetro: V = R I con el criterio pasivo */
static int resistivo(int tipo) { return tipo == EL_RES || tipo == EL_VOLT || tipo == EL_AMP; }

/* Los nodos (p, q) de la V del elemento j con el convenio de su tabla (D8):
 * (a, b) en los pasivos, (b, a) en las fuentes y las controladas, (b, la
 * referencia) en el vatímetro */
static void nodos_v(const Circuito *c, const Red *r, int j, int *p, int *q)
{
    int conv = circ_v_convenio(c->el[j].tipo);
    *p = conv == 1 ? r->a[j] : r->b[j];
    *q = conv == -1 ? r->a[j] : conv == 1 ? r->b[j] : -1;
}

/* El control de la controlada k: la V del que manda (de phi) o su I (de I[]) */
static double control_de(const Circuito *c, const Red *r, const double *phi, const double *I, int k)
{
    int j = c->el[k].ref, p, q;
    if (j < 0 || j >= r->nel) return 0.0;
    if (c->el[k].variante == DEP_I) return I[j];
    nodos_v(c, r, j, &p, &q);
    return (p >= 0 ? phi[p] : 0.0) - (q >= 0 ? phi[q] : 0.0);
}

/* Arma el sistema. El dato k va en su propio termino independiente b[1+k]
 * con su magnitud: el signo de cada combinacion lo pone sis_combinar(). */
static void arma(const Circuito *c, const Red *r, const Dato *d, int nd)
{
    int n, k, e, ncol = 0;

    sis_init(&sis);
    sis_terminos(&sis, 1 + nd);
    for (n = 0; n < r->nn; n++) col_phi[n] = r->ref[n] ? -1 : ncol++;
    sis_incognitas(&sis, ncol);
    col_i0 = sis_incognitas(&sis, r->nel);
    for (k = 0; k < r->nel; k++)
        col_int[k] = con_r(&c->el[k]) ? sis_incognitas(&sis, 1) : -1;

    /* Ley de nodos: lo que sale = 0. I va de a hacia b por dentro del
     * elemento, asi que sale del nodo a y entra en el b. */
    for (n = 0; n < r->nn; n++) {
        if (r->ref[n]) continue;
        e = sis_ecuacion(&sis);
        for (k = 0; k < r->nel; k++) {
            if (r->a[k] == n) sis_coef(&sis, e, col_i0 + k,  1.0);
            if (r->b[k] == n) sis_coef(&sis, e, col_i0 + k, -1.0);
        }
    }

    /* Ley de cada elemento, SOLO si su valor es conocido. Lo que en CC es un
     * abierto (voltímetro ideal, C, S abierto: I = 0) o un cable
     * (amperímetro ideal, L, S cerrado: V = 0) tiene ley siempre. */
    for (k = 0; k < r->nel; k++) {
        const Elemento *el = &c->el[k];
        int ab = circ_abierto_cc(el), co = circ_corto_cc(el);
        if (!ab && !co && el->val.est != M_DATO) continue;
        e = sis_ecuacion(&sis);
        if (ab) { sis_coef(&sis, e, col_i0 + k, 1.0); continue; }                    /* I = 0 */
        if (co) { coef_phi(e, r->a[k], 1.0); coef_phi(e, r->b[k], -1.0); continue; } /* V = 0 */
        switch (el->tipo) {
        case EL_RES:                          /* phi_a - phi_b - R I = 0 */
        case EL_VOLT:                         /* un instrumento con su R  */
        case EL_AMP:
            coef_phi(e, r->a[k],  1.0);
            coef_phi(e, r->b[k], -1.0);
            sis_coef(&sis, e, col_i0 + k, -el->val.v);
            break;
        case EL_CORR:                         /* I = J (de a hacia b)     */
            sis_coef(&sis, e, col_i0 + k, 1.0);
            sis_term(&sis, e, el->val.v);
            break;
        case EL_EDEP:                         /* phi_b - phi_a - g·control = 0 */
        case EL_JDEP: {                       /* I - g·control = 0             */
            int j = el->ref, p, q;
            double g = el->val.v;
            if (el->tipo == EL_EDEP) { coef_phi(e, r->b[k], 1.0); coef_phi(e, r->a[k], -1.0); }
            else sis_coef(&sis, e, col_i0 + k, 1.0);
            if (el->variante == DEP_I) sis_coef(&sis, e, col_i0 + j, -g);   /* en la tabla toda I es incógnita */
            else { nodos_v(c, r, j, &p, &q); coef_phi(e, p, -g); coef_phi(e, q, g); }
            break;
        }
        default:                              /* phi_+ - phi_- = E        */
            if (col_int[k] >= 0) sis_coef(&sis, e, col_int[k], 1.0);   /* real: hasta el nodo interno */
            else coef_phi(e, r->b[k],  1.0);
            coef_phi(e, r->a[k], -1.0);
            sis_term(&sis, e, el->val.v);
            break;
        }
    }

    /* La r de una fuente real, si se conoce: phi_int - phi_b - r I = 0 */
    for (k = 0; k < r->nel; k++) {
        const Elemento *el = &c->el[k];
        if (col_int[k] < 0 || el->aux.est != M_DATO) continue;
        e = sis_ecuacion(&sis);
        sis_coef(&sis, e, col_int[k], 1.0);
        coef_phi(e, r->b[k], -1.0);
        sis_coef(&sis, e, col_i0 + k, -el->aux.v);
    }

    /* Un dato por ecuacion, en su termino: V o I = +-m */
    for (k = 0; k < nd; k++) {
        e = sis_ecuacion(&sis);
        if (d[k].es_i) {
            sis_coef(&sis, e, col_i0 + d[k].k, 1.0);
        } else if (circ_v_convenio(c->el[d[k].k].tipo) == -1) {
            coef_phi(e, r->b[d[k].k],  1.0);  /* V de una fuente o una controlada = phi(b) - phi(a) */
            coef_phi(e, r->a[d[k].k], -1.0);
        } else {
            coef_phi(e, r->a[d[k].k],  1.0);
            coef_phi(e, r->b[d[k].k], -1.0);
        }
        sis_term_k(&sis, 1 + k, e, d[k].m);
    }
}

/* Valores de todos los elementos a partir de la solucion del sistema. */
static void extrae(const Circuito *c, const Red *r, Valores *v)
{
    int n, k;
    for (n = 0; n < r->nn; n++) v->phi[n] = col_phi[n] >= 0 ? x[col_phi[n]] : 0.0;
    for (k = 0; k < r->nel; k++) {
        const Elemento *el = &c->el[k];
        double pa = r->a[k] >= 0 ? v->phi[r->a[k]] : 0.0;
        double pb = r->b[k] >= 0 ? v->phi[r->b[k]] : 0.0;
        v->I[k] = x[col_i0 + k];
        if (resistivo(el->tipo)) {
            v->V[k] = pa - pb;
            v->val[k] = (el->val.est == M_DATO) ? el->val.v
                      : es_ideal(el) ? 0.0                    /* ideal: sin valor */
                      : (fabs(v->I[k]) > 0.0 ? v->V[k] / v->I[k] : 0.0);
            v->P[k] = v->V[k] * v->I[k];
        } else if (el->tipo == EL_CORR) {    /* J: su V es la de sus bornes */
            v->V[k] = pb - pa;
            v->val[k] = (el->val.est == M_DATO) ? el->val.v : v->I[k];
            v->P[k] = v->V[k] * v->I[k];
        } else if (el->tipo == EL_VATI) {
            /* bobina de corriente: un cable; bobina de tensión: de b a la
             * referencia (que vale 0). Marca P = V·I */
            v->V[k] = pb;
            v->val[k] = pb * v->I[k];
            v->P[k] = v->val[k];
        } else if (el->tipo == EL_EDEP || el->tipo == EL_JDEP) {
            /* como una fuente: V en bornes; la ganancia, dato o despejada */
            double ctl = control_de(c, r, v->phi, x + col_i0, k);
            v->V[k] = pb - pa;
            if (el->val.est == M_DATO) v->val[k] = el->val.v;
            else {
                double y = el->tipo == EL_EDEP ? v->V[k] : v->I[k];
                v->val[k] = ctl != 0.0 ? y / ctl : 0.0;
            }
            v->P[k] = v->V[k] * v->I[k];
        } else if (!tipo_info(el->tipo)->fuente) {   /* C, L, S: criterio pasivo */
            v->V[k] = pa - pb;
            v->val[k] = (el->val.est == M_DATO) ? el->val.v : 0.0;
            v->P[k] = v->V[k] * v->I[k];
        } else {
            /* Fuente de tensión: V en bornes; si es real, E y r por el nodo interno */
            double pi = col_int[k] >= 0 ? x[col_int[k]] : pb;
            v->V[k] = pb - pa;
            v->val[k] = (el->val.est == M_DATO) ? el->val.v : pi - pa;
            v->aux[k] = (el->aux.est == M_DATO) ? el->aux.v
                      : (col_int[k] >= 0 && fabs(v->I[k]) > 0.0) ? (pi - pb) / v->I[k] : 0.0;
            v->P[k] = v->V[k] * v->I[k];
        }
    }
}

static int iguales(const Valores *p, const Valores *q, int nel)
{
    int k;
    for (k = 0; k < nel; k++) {
        double e1 = fabs(p->I[k] - q->I[k]), s1 = fabs(p->I[k]) + fabs(q->I[k]);
        double e2 = fabs(p->V[k] - q->V[k]), s2 = fabs(p->V[k]) + fabs(q->V[k]);
        if (e1 > 1e-9 * (1.0 + s1) || e2 > 1e-9 * (1.0 + s2)) return 0;
    }
    return 1;
}

/* Qué tiene el elemento que solo es de la alterna: 0 el valor de C o L en
 * [?!], 1 un dato del vatímetro, 2 + q un ángulo en la magnitud q; -1 nada */
static int solo_ca(const Elemento *el)
{
    int q;
    if ((el->tipo == EL_COND || el->tipo == EL_BOB) && el->val.est == M_BUSCAR) return 0;
    if (el->tipo == EL_VATI && (el->val.est == M_DATO || el->aux.est != M_LIBRE)) return 1;
    for (q = 0; q < Q_N; q++) {
        const CampoInfo *ci = circ_campo(el, q);
        if (ci && (ci->flags & CF_ANG) && circ_mag_c(el, q)->est_ang != M_LIBRE) return 2 + q;
    }
    return -1;
}

/* Elemento al que corresponde una columna libre del sistema. */
static int elemento_de_columna(const Circuito *c, const Red *r, int j)
{
    int k, n;
    for (k = 0; k < r->nel; k++) if (col_int[k] == j) return k;   /* nodo interno */
    if (j >= col_i0) return j - col_i0;
    for (n = 0; n < r->nn; n++) if (col_phi[n] == j) break;
    /* un nodo que queda sin fijar entre condensadores o bobinas: la culpa
     * es de su historia (una C o una L), no del S o la R de al lado */
    for (k = 0; k < r->nel; k++)
        if ((r->a[k] == n || r->b[k] == n) && (c->el[k].tipo == EL_COND || c->el[k].tipo == EL_BOB)) return k;
    for (k = 0; k < r->nel; k++)
        if ((r->a[k] == n || r->b[k] == n) && c->el[k].val.est != M_DATO) return k;
    for (k = 0; k < r->nel; k++)
        if (r->a[k] == n || r->b[k] == n) return k;
    return -1;
}

int sol_desconocidos(const Circuito *c, int *primero)
{
    int k, n = 0;
    if (primero) *primero = -1;
    for (k = 0; k < c->nel; k++) {
        const Elemento *el = &c->el[k];
        int m = (circ_val_en_ley(el) && el->val.est != M_DATO) +
                (el->tipo == EL_FUENTE && el->aux.est == M_BUSCAR);    /* r buscada */
        if (m && primero && *primero < 0) *primero = k;
        n += m;
    }
    return n;
}

int resolver(const Circuito *c, const Red *r, Solucion *s)
{
    Dato d[MAX_EL * 2];
    int nd = 0, k, i, hay_ok = 0, hay_indet = 0, elem_indet = -1;
    int elem_neg = -1, elem_sin_i = -1, elem_cero = -1, nf = 0, rango;
    unsigned comb, ncomb;
    static Valores cand;

    memset(s, 0, sizeof *s);
    s->elem = -1;
    s->fuente = -1;

    /* Un circuito de alterna es de los fasores, antes de mirar la red: una E
     * en paralelo con una L es un corto en CC y no en CA */
    if (circ_alterna(c)) {
        for (k = 0; k < c->nel && s->elem < 0; k++) {
            const Elemento *el = &c->el[k];
            if (el->tipo == EL_IMP || ((el->tipo == EL_FUENTE || el->tipo == EL_CORR) && el->variante == FORMA_FASOR))
                s->elem = k;
        }
        s->estado = SOL_CA;
        return s->estado;
    }

    /* La tabla es lineal: un diodo, un AO o un transistor no caben en ella */
    for (k = 0; k < c->nel; k++)
        if (tipo_info(c->el[k].tipo)->ley_cc == LEY_NL) { s->estado = SOL_NO_LINEAL; s->elem = k; return s->estado; }

    if (r->error != RED_OK) { s->estado = SOL_ERR_RED; s->elem = r->elem_error; return s->estado; }

    /* Una controlada sin un control que valga: la misma regla que el
     * emulador (la tabla podría resolver algún control más, pero el mismo
     * circuito fallaría al emularlo) */
    for (k = 0; k < c->nel; k++)
        if (tipo_info(c->el[k].tipo)->ley_cc == LEY_DEP) {
            int m = circ_ctl_motivo(c, k);
            if (m != CTL_OK) { s->estado = SOL_CTL; s->elem = k; s->motivo = m; return s->estado; }
        }

    s->ndesc = sol_desconocidos(c, &s->elem);
    for (k = 0; k < r->nel; k++) {
        const Elemento *el = &c->el[k];
        if (tipo_info(el->tipo)->fuente) { nf++; s->fuente = k; }
        if (el->tipo == EL_FUENTE && el->aux.est == M_DATO && !(el->aux.v >= 0.0)) {
            s->estado = SOL_R_INVALIDA; s->elem = k; return s->estado;
        } else if (resistivo(el->tipo) && el->val.est == M_DATO && !(el->val.v > 0.0)) {
            s->estado = SOL_R_INVALIDA; s->elem = k; return s->estado;
        }
        if (el->V.est == M_DATO) { d[nd].k = k; d[nd].es_i = 0; d[nd].m = fabs(el->V.v); nd++; }
        if (el->I.est == M_DATO) { d[nd].k = k; d[nd].es_i = 1; d[nd].m = fabs(el->I.v); nd++; }
    }
    if (nf != 1) s->fuente = -1;
    s->ndatos = nd;

    /* Lo que solo es de la alterna: el valor de C o L en [?!] (en CC C es un
     * abierto y L un cable), un ángulo, o P o fp de un vatímetro */
    for (k = 0; k < r->nel; k++)
        if (solo_ca(&c->el[k]) >= 0) { s->estado = SOL_SOLO_CA; s->elem = k; return s->estado; }

    /* La calculadora es de CC: una fuente senoidal, cuadrada... no */
    for (k = 0; k < r->nel; k++)
        if ((c->el[k].tipo == EL_FUENTE || c->el[k].tipo == EL_CORR) && c->el[k].variante != ONDA_CC) {
            s->estado = SOL_FORMA; s->elem = k; return s->estado;
        }
    if (nd < s->ndesc) { s->estado = SOL_FALTAN; return s->estado; }
    if (nd > MAX_DATOS) { s->estado = SOL_MUCHOS_DATOS; s->elem = -1; return s->estado; }
    s->elem = -1;

    /* E y r de la misma fuente, las dos desconocidas: con un solo estado del
     * circuito solo se sabe V = E - r·I, así que no se separan nunca. */
    for (k = 0; k < r->nel; k++)
        if (c->el[k].tipo == EL_FUENTE && c->el[k].val.est != M_DATO && c->el[k].aux.est == M_BUSCAR) {
            s->estado = SOL_INDET; s->elem = k; return s->estado;
        }

    /* La matriz no depende de los signos de los datos: se elimina una vez y
     * cada combinacion es solo una suma de columnas ya reducidas. */
    arma(c, r, d, nd);
    if (sis.desborde) { s->estado = SOL_MUCHOS_DATOS; return s->estado; }
    sis_reducir(&sis, &rango, libre);

    ncomb = 1u << nd;
    for (comb = 0; comb < ncomb; comb++) {
        int g, fisica = 1;

        /* Un dato de valor 0 no tiene signo: solo se prueba una vez */
        for (i = 0; i < nd; i++) if ((comb & (1u << i)) && d[i].m == 0.0) break;
        if (i < nd) continue;

        g = sis_combinar_signos(comb, x);

        if (g == GAUSS_INDETERMINADO) {
            int j;
            hay_indet = 1;
            for (j = 0; j < sis.nunk && elem_indet < 0; j++)
                if (libre[j]) elem_indet = elemento_de_columna(c, r, j);
            continue;
        }
        if (g != GAUSS_OK) continue;

        extrae(c, r, &cand);
        hay_ok = 1;

        {
            /* Una R desconocida con corriente pero sin tension vale 0: se
             * decide con tolerancia, no por el signo del redondeo (±1e-15). */
            double esc_v = 0.0, esc_i = 0.0;
            for (k = 0; k < r->nel; k++) {
                if (fabs(cand.V[k]) > esc_v) esc_v = fabs(cand.V[k]);
                if (fabs(cand.I[k]) > esc_i) esc_i = fabs(cand.I[k]);
            }
            for (k = 0; k < r->nel; k++) {
                const Elemento *el = &c->el[k];
                if ((el->tipo == EL_EDEP || el->tipo == EL_JDEP) && el->val.est != M_DATO) {
                    /* una ganancia despejada: con el control en 0 no se puede
                     * hallar (una negativa sí es física: un inversor) */
                    double ctl = control_de(c, r, cand.phi, cand.I, k);
                    if (fabs(ctl) <= 1e-12 * (el->variante == DEP_I ? esc_i : esc_v)) {
                        fisica = 0; if (elem_sin_i < 0) elem_sin_i = k;
                    }
                    continue;
                }
                if (el->tipo == EL_FUENTE && el->aux.est == M_BUSCAR) {
                    /* la r que se busca: sin corriente no se puede hallar (y
                     * la V en bornes queda libre); con corriente, no negativa */
                    if (fabs(cand.I[k]) < 1e-12 * (1.0 + fabs(cand.V[k]))) {
                        fisica = 0; if (elem_sin_i < 0) elem_sin_i = k;
                    } else if (cand.aux[k] < -1e-9 * (1.0 + fabs(cand.val[k] / fabs(cand.I[k])))) {
                        fisica = 0; if (elem_neg < 0) elem_neg = k;
                    }
                }
                if (!resistivo(el->tipo) || el->val.est == M_DATO || es_ideal(el)) continue;
                if (fabs(cand.I[k]) < 1e-12 * (1.0 + fabs(cand.V[k]))) {
                    fisica = 0; if (elem_sin_i < 0) elem_sin_i = k;
                } else if (fabs(cand.V[k]) <= 1e-9 * (1.0 + esc_v)) {
                    fisica = 0; if (elem_cero < 0) elem_cero = k;
                } else if (!(cand.val[k] > 0.0)) {
                    fisica = 0; if (elem_neg < 0) elem_neg = k;
                }
            }
        }
        if (!fisica) continue;

        for (i = 0; i < s->nsol; i++) if (iguales(&s->s[i], &cand, r->nel)) break;
        if (i == s->nsol && s->nsol < MAX_SOL) s->s[s->nsol++] = cand;
    }

    if (s->nsol > 0) {
        /* Preferencia: fems desconocidas >= 0 con la polaridad dibujada. Solo
         * descarta si queda alguna que la cumpla. */
        int buenas = 0, j;
        int ok[MAX_SOL];
        for (i = 0; i < s->nsol; i++) {
            ok[i] = 1;
            for (k = 0; k < r->nel; k++)
                if ((tipo_info(c->el[k].tipo)->fuente || tipo_info(c->el[k].tipo)->ley_cc == LEY_DEP) &&
                    c->el[k].val.est != M_DATO && s->s[i].val[k] < -1e-12 * (1.0 + fabs(s->s[i].val[k])))
                    ok[i] = 0;
            buenas += ok[i];
        }
        if (buenas > 0 && buenas < s->nsol) {
            for (i = 0, j = 0; i < s->nsol; i++) if (ok[i]) s->s[j++] = s->s[i];
            s->nsol = j;
        }
        s->estado = SOL_OK;
        if (s->fuente >= 0 && fabs(s->s[0].I[s->fuente]) > 1e-15) {
            s->hay_req = 1;
            s->req = s->s[0].V[s->fuente] / s->s[0].I[s->fuente];
        }
        return s->estado;
    }

    if (elem_sin_i >= 0)   { s->estado = SOL_SIN_CORRIENTE; s->elem = elem_sin_i; }
    else if (elem_cero >= 0){ s->estado = SOL_R_CERO;       s->elem = elem_cero; }
    else if (elem_neg >= 0){ s->estado = SOL_NO_FISICA;     s->elem = elem_neg; }
    else if (hay_indet)    { s->estado = SOL_INDET;         s->elem = elem_indet; }
    else                   { s->estado = SOL_INCOMPAT; }
    (void)hay_ok;
    return s->estado;
}

/* --------------------------------------------------------------------------
 *  Mensajes
 * ----------------------------------------------------------------------- */
const char *sol_texto_ctl(int motivo, const char *nom, const char *nom_ctl, char *buf, size_t cap)
{
    Buf b;
    const char *nc = nom_ctl ? nom_ctl : "?";
    buf_init(&b, buf, cap);
    switch (motivo) {
    case CTL_SI_MISMA:
        buf_str(&b, nom); buf_str(&b, " no puede controlarse a s" S_ii " misma: elige otro elemento en la fila x.");
        break;
    case CTL_3PATAS:
        buf_str(&b, nom); buf_str(&b, " no puede controlarse con "); buf_str(&b, nc);
        buf_str(&b, " (tiene 3 patas): pon un volt" S_ii "metro o un amper" S_ii "metro donde quieras medir.");
        break;
    case CTL_I_NO:
        buf_str(&b, "La corriente de "); buf_str(&b, nc); buf_str(&b, " no puede mandar a "); buf_str(&b, nom);
        buf_str(&b, ": pon un amper" S_ii "metro en serie con "); buf_str(&b, nc); buf_str(&b, " y controla con el suyo.");
        break;
    default:                                   /* CTL_FALTA */
        buf_str(&b, nom);
        buf_str(&b, " es una fuente controlada sin control: en su ficha, [+][-] en la fila x elige qui" S_ee "n la manda.");
        break;
    }
    return buf;
}

/* Qué es un no lineal, para SOL_NO_LINEAL */
static const char *que_es(int tipo)
{
    switch (tipo) {
    case EL_ZENER: return "un diodo zener";
    case EL_LED:   return "un LED";
    case EL_AO:    return "un amplificador operacional";
    case EL_BJT:   return "un transistor";
    case EL_MOS:   return "un MOSFET";
    default:       return "un diodo";
    }
}

const char *sol_mensaje(const Circuito *c, const Red *r, const Solucion *s,
                        char *buf, size_t cap)
{
    Buf b;
    const char *nom = (s->elem >= 0 && s->elem < c->nel) ? c->el[s->elem].etiq : "?";

    buf_init(&b, buf, cap);
    switch (s->estado) {
    case SOL_OK:
        if (s->nsol > 1) {
            buf_str(&b, "Los datos admiten ");
            buf_int(&b, s->nsol);
            buf_str(&b, " soluciones.");
        }
        break;
    case SOL_ERR_RED:
        if (r->error == RED_VACIA) buf_str(&b, "Dibuja al menos una fuente y una resistencia.");
        else if (r->error == RED_SIN_FUENTE) buf_str(&b, "Falta una fuente: sin ella no circula corriente.");
        else if (r->error == RED_FUENTE_AMP) {
            const Elemento *cu = (r->elem_culpa >= 0 && r->elem_culpa < c->nel) ? &c->el[r->elem_culpa] : 0;
            buf_str(&b, nom);
            if (cu && cu->tipo == EL_BOB) {
                buf_str(&b, " queda en cortocircuito: en CC la bobina "); buf_str(&b, cu->etiq); buf_str(&b, " es un cable.");
            } else if (cu && cu->tipo == EL_INTER) {
                buf_str(&b, " queda en cortocircuito: "); buf_str(&b, cu->etiq); buf_str(&b, " est" S_aa " cerrado al empezar.");
            } else if (cu && cu->tipo == EL_VATI) {
                buf_str(&b, " queda en cortocircuito: el vat" S_ii "metro "); buf_str(&b, cu->etiq);
                buf_str(&b, " es un cable (su bobina de corriente).");
            } else
                buf_str(&b, " queda en cortocircuito: un amper" S_ii "metro ideal es un cable. Ponlo en serie.");
        }
        else if (r->error == RED_J_ABIERTA) {
            buf_str(&b, "La corriente de ");
            buf_str(&b, nom);
            buf_str(&b, " no tiene por d" S_oo "nde volver (un borne al aire, o en serie un volt" S_ii "metro ideal, una C o un S abierto).");
        }
        else { buf_str(&b, nom); buf_str(&b, " est" S_aa " en cortocircuito: un cable une sus dos bornes."); }
        break;
    case SOL_FALTAN:
        buf_str(&b, "Faltan datos: ");
        buf_int(&b, s->ndesc);
        buf_str(&b, s->ndesc == 1 ? " valor desconocido (" : " valores desconocidos (");
        buf_str(&b, nom);
        buf_str(&b, s->ndesc > 1 ? "...)" : ")");
        buf_str(&b, " y ");
        buf_int(&b, s->ndatos);
        buf_str(&b, s->ndatos == 1 ? " dato de V o I." : " datos de V o I.");
        break;
    case SOL_INDET:
        if (s->elem >= 0 && s->elem < c->nel && c->el[s->elem].tipo == EL_FUENTE &&
            c->el[s->elem].val.est != M_DATO && c->el[s->elem].aux.est == M_BUSCAR) {
            buf_str(&b, "E y r de ");
            buf_str(&b, nom);
            buf_str(&b, " no se separan con un solo estado del circuito (solo se sabe V = E - r" S_CDOT "I): da una de las dos.");
        } else if (s->elem >= 0 && s->elem < c->nel &&
                   (c->el[s->elem].tipo == EL_COND || c->el[s->elem].tipo == EL_BOB || c->el[s->elem].tipo == EL_INTER)) {
            buf_str(&b, nom);
            buf_str(&b, " queda sin fijar: en CC, condensadores en serie o bobinas en paralelo dependen de su historia. Sin [?!], [=] lo emula.");
        } else if (s->ndatos == 0) {
            buf_str(&b, "El circuito deja algo sin fijar en ");
            buf_str(&b, nom);
            buf_str(&b, ": fuentes de corriente en serie o instrumentos ideales que no fijan una tensi" S_oo "n o una corriente.");
        } else {
            buf_str(&b, "Los datos no bastan: no determinan ");
            buf_str(&b, nom);
            buf_str(&b, ". Alg" S_uu "n dato no aporta nada nuevo.");
        }
        break;
    case SOL_SIN_CORRIENTE:
        if (s->elem >= 0 && s->elem < c->nel && tipo_info(c->el[s->elem].tipo)->ley_cc == LEY_DEP) {
            buf_str(&b, "La ganancia de "); buf_str(&b, nom);
            buf_str(&b, " no se puede hallar: su control vale 0.");
            break;
        }
        buf_str(&b, nom);
        if (s->elem >= 0 && s->elem < c->nel && c->el[s->elem].tipo == EL_FUENTE)
            buf_str(&b, " no lleva corriente: su r interna no se puede hallar.");
        else
            buf_str(&b, " no lleva corriente: su valor no afecta al circuito y no se puede hallar.");
        break;
    case SOL_INCOMPAT:
        if (s->ndatos == 0)
            buf_str(&b, "El circuito es imposible tal como est" S_aa ": hay fuentes que se contradicen (dos J distintas en serie, o E distintas en paralelo).");
        else
            buf_str(&b, "Los datos se contradicen: no hay circuito que los cumpla todos.");
        break;
    case SOL_NO_FISICA:
        buf_str(&b, "No hay soluci" S_oo "n f" S_ii "sica: ");
        if (s->elem >= 0 && c->el[s->elem].tipo == EL_FUENTE) buf_str(&b, "la r interna de ");
        buf_str(&b, nom);
        buf_str(&b, " tendr" S_ii "a que ser negativa.");
        break;
    case SOL_MUCHOS_DATOS:
        buf_str(&b, "Demasiados datos de V o I (m" S_aa "ximo 8).");
        break;
    case SOL_R_CERO:
        buf_str(&b, "No hay soluci" S_oo "n f" S_ii "sica: ");
        buf_str(&b, nom);
        buf_str(&b, " tendr" S_ii "a que valer 0 (lleva corriente sin tensi" S_oo "n: ser" S_ii "a un cable).");
        break;
    case SOL_FORMA:
        buf_str(&b, nom);
        buf_str(&b, " es ");
        buf_str(&b, (s->elem >= 0 && s->elem < c->nel && c->el[s->elem].variante < ONDA_N) ?
                    NOM_FORMA[c->el[s->elem].variante] : "variable");
        buf_str(&b, ": la calculadora solo resuelve CC. Quita los [?!] y [=] la emula, o ponla en continua o en alterna.");
        break;
    case SOL_CA:
        buf_str(&b, "Circuito de alterna: [=] lo resuelve con fasores.");
        break;
    case SOL_SOLO_CA: {
        const Elemento *el = (s->elem >= 0 && s->elem < c->nel) ? &c->el[s->elem] : 0;
        int que = el ? solo_ca(el) : -1;
        if (que == 0) {
            buf_str(&b, "En CC, "); buf_str(&b, nom);
            buf_str(&b, el->tipo == EL_COND ? " es un abierto" : " es un cable");
            buf_str(&b, ": su valor no cuenta. Para hallarlo, pon la fuente en alterna.");
        } else if (que == 1) {
            buf_str(&b, "Los datos de "); buf_str(&b, nom); buf_str(&b, " son de la alterna.");
        } else if (que >= 2) {
            const CampoInfo *ci = circ_campo(el, que - 2);
            buf_str(&b, ci ? ci->nombre : "V"); buf_ch(&b, '('); buf_str(&b, nom);
            buf_str(&b, ") lleva " S_aa "ngulo: los " S_aa "ngulos son de la alterna.");
        } else
            buf_str(&b, "Eso solo es de la alterna.");
        break;
    }
    case SOL_NO_LINEAL: {
        int k, otro = 0;
        buf_str(&b, nom); buf_str(&b, " es ");
        buf_str(&b, que_es(s->elem >= 0 && s->elem < c->nel ? c->el[s->elem].tipo : EL_DIODO));
        buf_str(&b, ": la calculadora de CC es lineal y no despeja con " S_ee "l. Sin [?!], [=] da su punto de operaci" S_oo "n");
        /* [I] solo halla lo que se barre: un [?!] en el valor de una E, una
         * J, una R o una controlada (barrible en barrido.c, y el pie de
         * P_RESULTADO). Con los [?!] solo en otra cosa (una C, la r de una
         * fuente), no se ofrece */
        for (k = 0; k < c->nel; k++) {
            int t = c->el[k].tipo;
            if (c->el[k].val.est == M_BUSCAR &&
                (t == EL_FUENTE || t == EL_CORR || t == EL_RES || t == EL_EDEP || t == EL_JDEP)) break;
            if (c->el[k].val.est == M_BUSCAR || c->el[k].aux.est == M_BUSCAR) otro = 1;
        }
        buf_str(&b, k < c->nel || !otro ? "; para hallar un valor, barre con [I] y lee con [N]." : ".");
        break;
    }
    case SOL_CTL: {
        const char *nc = (s->elem >= 0 && s->elem < c->nel && c->el[s->elem].ref >= 0 && c->el[s->elem].ref < c->nel)
                         ? c->el[c->el[s->elem].ref].etiq : 0;
        return sol_texto_ctl(s->motivo, nom, nc, buf, cap);
    }
    case SOL_SIM:
        buf_str(&b, "El punto de operaci" S_oo "n no se pudo hallar.");
        break;
    case SOL_R_INVALIDA:
        if (s->elem >= 0 && s->elem < c->nel && c->el[s->elem].tipo == EL_FUENTE) {
            buf_str(&b, "La r interna de ");
            buf_str(&b, nom);
            buf_str(&b, " no puede ser negativa (vac" S_ii "a o 0 = ideal).");
        } else {
            buf_str(&b, "La resistencia ");
            buf_str(&b, nom);
            buf_str(&b, " tiene que ser mayor que 0.");
        }
        break;
    }
    return buf;
}

int sol_vab(const Circuito *c, const Red *r, const Valores *v, double *vab, int *na, int *nb)
{
    int a, b;
    *vab = 0.0; *na = *nb = -1;
    if (c->term_x[0] < 0 || c->term_y[0] < 0 || c->term_x[1] < 0 || c->term_y[1] < 0) return 0;
    a = r->punto[c->term_y[0] * REJ_COLS + c->term_x[0]];
    b = r->punto[c->term_y[1] * REJ_COLS + c->term_x[1]];
    if (a < 0 || b < 0) return 0;
    *na = a; *nb = b;
    if (r->comp[a] != r->comp[b]) return -1;
    *vab = v->phi[a] - v->phi[b];
    return 1;
}

/* --------------------------------------------------------------------------
 *  La potencia absorbida y el balance (la tabla de CC)
 * ----------------------------------------------------------------------- */
double sol_p_abs(const Circuito *c, const Valores *v, int k)
{
    int t = c->el[k].tipo;
    if (t == EL_VATI) return 0.0;                    /* marca la que pasa; no absorbe */
    if (circ_v_convenio(t) == -1) return -v->P[k];   /* E, J y controladas: su P es la entregada */
    if (t == EL_AO) return -v->P[k];                 /* el AO (punto de operación): la que entrega
                                                        por la salida menos la de sus entradas */
    return v->P[k];                                  /* pasivos: V de a a b, I de a hacia b */
}

double sol_balance(const Circuito *c, const Valores *v, double *ent, double *absorb)
{
    double suma = 0.0, esc = 0.0;
    int k;
    *ent = *absorb = 0.0;
    for (k = 0; k < c->nel; k++) {
        double p = sol_p_abs(c, v, k);
        if (p < 0.0) *ent -= p;
        else *absorb += p;
        suma += p;
        esc += fabs(p);
    }
    return esc > 0.0 ? fabs(suma) / esc : 0.0;
}

/* --------------------------------------------------------------------------
 *  Residuo: la comprobacion que no usa el motor
 * ----------------------------------------------------------------------- */
double sol_residuo(const Circuito *c, const Red *r, const Valores *v)
{
    double peor = 0.0, esc_i = 1e-30, esc_v = 1e-30, t;
    int n, k;

    double rmax = 0.0;
    for (k = 0; k < r->nel; k++) {
        if (fabs(v->I[k]) > esc_i) esc_i = fabs(v->I[k]);
        if (fabs(v->V[k]) > esc_v) esc_v = fabs(v->V[k]);
        /* una E en corto (todas las V = 0) deja la escala en su fem */
        if (c->el[k].tipo == EL_FUENTE && fabs(v->val[k]) > esc_v) esc_v = fabs(v->val[k]);
        if (c->el[k].tipo == EL_CORR && fabs(v->val[k]) > esc_i) esc_i = fabs(v->val[k]);
        if (resistivo(c->el[k].tipo) && v->val[k] > rmax) rmax = v->val[k];
    }
    /* Una red que lo anula casi todo (una J que se cierra por controladas a
     * 0 V): las V valen el redondeo; su escala no baja de 1e-6·I·R */
    if (rmax > 0.0) {
        if (esc_v < 1e-6 * esc_i * rmax) esc_v = 1e-6 * esc_i * rmax;
        if (esc_i < 1e-6 * esc_v / rmax) esc_i = 1e-6 * esc_v / rmax;
    }

    for (n = 0; n < r->nn; n++) {                        /* ley de nodos */
        double suma = 0.0;
        for (k = 0; k < r->nel; k++) {
            if (r->a[k] == n) suma += v->I[k];
            if (r->b[k] == n) suma -= v->I[k];
        }
        t = fabs(suma) / esc_i; if (t > peor) peor = t;
    }
    for (k = 0; k < r->nel; k++) {
        const Elemento *el = &c->el[k];
        double pa = v->phi[r->a[k]], pb = v->phi[r->b[k]];
        if (circ_abierto_cc(el)) {
            t = fabs(v->I[k]) / esc_i;                               /* I = 0 */
            if (t > peor) peor = t;
            t = fabs((pa - pb) - v->V[k]) / esc_v;
        } else if (el->tipo == EL_VATI) {
            t = fabs(pa - pb) / esc_v;                               /* un cable */
            if (t > peor) peor = t;
            t = fabs(v->V[k] - pb) / esc_v;                          /* V de la bobina: phi(b) */
        } else if (circ_corto_cc(el)) {
            t = fabs(pa - pb) / esc_v;                               /* V = 0 */
            if (t > peor) peor = t;
            t = fabs((pa - pb) - v->V[k]) / esc_v;
        } else if (resistivo(el->tipo)) {
            t = fabs((pa - pb) - v->val[k] * v->I[k]) / esc_v;       /* Ohm */
            if (t > peor) peor = t;
            t = fabs((pa - pb) - v->V[k]) / esc_v;
        } else if (el->tipo == EL_CORR) {
            t = fabs(v->I[k] - v->val[k]) / esc_i;                   /* I = J */
            if (t > peor) peor = t;
            t = fabs((pb - pa) - v->V[k]) / esc_v;
        } else if (el->tipo == EL_EDEP || el->tipo == EL_JDEP) {
            double ctl = control_de(c, r, v->phi, v->I, k);         /* su ley con sus valores */
            if (el->tipo == EL_EDEP) t = fabs((pb - pa) - v->val[k] * ctl) / esc_v;
            else t = fabs(v->I[k] - v->val[k] * ctl) / esc_i;
            if (t > peor) peor = t;
            t = fabs((pb - pa) - v->V[k]) / esc_v;
        } else {
            /* fuente: V en bornes = E - r I (r = 0 si es ideal) */
            t = fabs((pb - pa) - (v->val[k] - v->aux[k] * v->I[k])) / esc_v;
            if (t > peor) peor = t;
            t = fabs(v->V[k] - (pb - pa)) / esc_v;
            if (t > peor) peor = t;
            if (el->aux.est == M_DATO) t = fabs(v->aux[k] - el->aux.v) / (1.0 + fabs(el->aux.v));
        }
        if (t > peor) peor = t;
        if (el->val.est == M_DATO && !es_ideal(el) && el->tipo != EL_VATI) {
            t = fabs(v->val[k] - el->val.v) / (1.0 + fabs(el->val.v));
            if (t > peor) peor = t;
        }
        if (el->V.est == M_DATO) {
            t = fabs(fabs(v->V[k]) - fabs(el->V.v)) / esc_v;
            if (t > peor) peor = t;
        }
        if (el->I.est == M_DATO) {
            t = fabs(fabs(v->I[k]) - fabs(el->I.v)) / esc_i;
            if (t > peor) peor = t;
        }
    }
    return peor;
}

/* --------------------------------------------------------------------------
 *  La solución como datos (Thévenin desde el resultado)
 * ----------------------------------------------------------------------- */
void sol_aplica(const Circuito *c, const Solucion *s, int i, Circuito *out)
{
    int k;
    if (out != c) *out = *c;
    if (s->estado != SOL_OK || i < 0 || i >= s->nsol) return;
    for (k = 0; k < out->nel; k++) {
        Elemento *e = &out->el[k];
        if (circ_val_en_ley(e) && e->val.est != M_DATO) { e->val.v = s->s[i].val[k]; e->val.est = M_DATO; }
        if (e->tipo == EL_FUENTE && e->aux.est == M_BUSCAR) { e->aux.v = s->s[i].aux[k]; e->aux.est = M_DATO; }
    }
}
