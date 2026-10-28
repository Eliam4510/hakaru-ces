/* ============================================================================
 *  sim.c  -  El emulador  (ver sim.h)
 *
 *  Orden del archivo:
 *    1. adaptador (Circuito -> SimRed) y mapa de incógnitas
 *    2. análisis estructural: lo que haría singular la matriz, convertido en
 *       una regla física o en un error con nombre
 *    3. ensamblado y término independiente
 *    4. punto de operación
 *    5. transitorio: eventos, pasos, arranque tras discontinuidades
 *    6. constantes de tiempo (modos), paso y ventana automáticos
 *    7. muestras y medidas
 * ========================================================================= */
#include <string.h>
#include <stdint.h>
#include <math.h>
#include "sim.h"
#include "lu.h"
#include "nolin.h"
#include "numfmt.h"
#include "gfx.h"

#define NUNCA 1e300
#define PI_D  3.14159265358979323846

static unsigned long ops_propias;

unsigned long sim_ops(void)       { return ops_propias + lu_ops() + nl_ops(); }
void          sim_ops_reset(void) { ops_propias = 0; lu_ops_reset(); nl_ops_reset(); }
#ifndef NDLESS
/* El arnés: el contador en v (cerca de su vuelta: la de 32 bits de la calculadora) */
void          sim_ops_pon(unsigned long v) { sim_ops_reset(); ops_propias = v; }
#endif

/* ==========================================================================
 *  1. Adaptador y mapa
 * ======================================================================= */
static int fuente_ideal_c(const Elemento *e)
{
    return e->aux.est == M_LIBRE || (e->aux.est == M_DATO && e->aux.v == 0.0);
}

/* Un valor que tiene que ser dato; con tolera (la calculadora de CA), el
 * provisional si no lo es (la calculadora lo sustituye) */
static int valor_o(const Mag *m, double provisional, int tolera, double *v)
{
    if (m->est == M_DATO) { *v = m->v; return m->v > 0.0; }
    if (!tolera) return 0;
    *v = provisional;
    return 1;
}

static int el_diodo(const Elemento *e, SimEl *x, int *q);
static double par_o(const Elemento *e, int q, double omision);

/* Un elemento del Circuito al del emulador. 1 si vale; 0 si falta algo (en
 * *q, la magnitud que falta; Q_CTRL en una controlada sin control); -1 si
 * los parámetros son imposibles (en *q, el hueco culpable). La fuente
 * alterna sale como una senoidal (pico y seno); con tolera, un valor o una f
 * que no son dato toman uno provisional. */
static int el_de(const Circuito *c, const Elemento *e, SimEl *x, int *q, int tolera)
{
    const Mag *m;
    *q = Q_VAL;
    x->t1 = x->t2 = -1.0;
    x->ctl = -1;
    if (e->tipo < EL_N && tipo_info(e->tipo)->nvar > 0 && e->variante >= tipo_info(e->tipo)->nvar &&
        e->tipo >= EL_DIODO) return -1;               /* un archivo roto: no indexa fuera de su tabla */
    switch (e->tipo) {
    case EL_DIODO:
    case EL_ZENER:
    case EL_LED:
        return el_diodo(e, x, q);
    case EL_AO:                                          /* D7: raíles V+ y V-, ±15 V por omisión */
        x->tipo = SE_AO;
        x->modelo = e->variante == AO_REAL ? NL_AO_REAL : NL_AO_IDEAL;
        memset(x->m, 0, sizeof x->m);
        x->m[1] = par_o(e, QA_VP, NL_VSAT);
        x->m[2] = par_o(e, QA_VN, -NL_VSAT);
        if (x->modelo == NL_AO_REAL) {
            x->m[0] = par_o(e, QA_A, NL_A);
            x->m[3] = par_o(e, QA_RI, 0.0);              /* vacía: infinita */
            x->m[4] = par_o(e, QA_RO, 0.0);
        }
        if (!(x->m[1] > x->m[2])) { *q = QA_VP; return -1; }
        return 1;
    case EL_BJT:                                         /* D9: Ebers-Moll de transporte con Early */
        x->tipo = SE_BJT;
        x->modelo = NL_BJT;
        x->pol = (signed char)(e->variante == BJT_PNP ? -1 : 1);
        memset(x->m, 0, sizeof x->m);
        x->m[0] = par_o(e, QB_IS, NL_IS_Q);
        x->m[1] = par_o(e, QB_BF, NL_BF);
        x->m[2] = par_o(e, QB_BR, NL_BR);
        x->m[3] = par_o(e, QB_VA, 0.0);                  /* vacía: sin Early */
        /* los rangos de la ficha ya lo impiden; QB_BF (Q_P1) da el texto de
         * «no cuadran» (Q_P2 es también QZ_RZ, y su texto es del zener) */
        if (!(x->m[0] > 0.0 && x->m[1] > 0.0 && x->m[2] > 0.0 && x->m[3] >= 0.0)) { *q = QB_BF; return -1; }
        x->m[5] = nl_vcrit(x->m[0], NL_VT);
        return 1;
    case EL_MOS:                                         /* D10: Shichman-Hodges (nivel 1), simétrico */
        x->tipo = SE_MOS;
        x->modelo = NL_MOS;
        x->pol = (signed char)(e->variante == MOS_CP ? -1 : 1);
        memset(x->m, 0, sizeof x->m);
        x->m[0] = par_o(e, QM_VT, NL_VT_M);              /* Vt en módulo (también en el canal P) */
        x->m[1] = par_o(e, QM_K, NL_K);
        x->m[2] = par_o(e, QM_VA, 0.0);                  /* vacía: sin modulación del canal */
        /* como en el BJT, el texto de «no cuadran» sale de Q_P1 (QM_VT) */
        if (!(x->m[0] >= 0.0 && x->m[1] > 0.0 && x->m[2] >= 0.0)) { *q = QM_VT; return -1; }
        return 1;
    case EL_EDEP:
    case EL_JDEP:
        x->tipo = e->tipo == EL_EDEP ? SE_EC : SE_JC;
        x->ctl_i = (unsigned char)(e->variante == DEP_I);
        if (e->ref < 0 || (c && e->ref >= c->nel)) { *q = Q_CTRL; return 0; }
        x->ctl = e->ref;
        if (e->val.est == M_DATO) x->val = e->val.v;
        else if (tolera) x->val = 0.0;                /* provisional: calca.c ya paró antes */
        else return 0;
        return 1;
    case EL_RES:
        x->tipo = SE_R;
        return valor_o(&e->val, 1e3, tolera, &x->val);
    case EL_IMP: {
        double mod, ang;
        x->tipo = SE_Z;
        if (!valor_o(&e->val, 1e3, tolera, &mod)) return 0;
        ang = (e->val.est == M_DATO && e->val.est_ang == M_DATO) ? e->val.ang * PI_D / 180.0 : 0.0;
        /* a ±90° es reactiva pura: cos(pi/2) en coma flotante (6e-17) no es una R */
        x->val = fabs(e->val.ang) == 90.0 && ang != 0.0 ? 0.0 : mod * cos(ang);
        x->p[0] = mod * sin(ang);
        if (c && c->frec.est == M_DATO) x->p[1] = c->frec.v;
        else if (tolera) x->p[1] = 1e3;
        else { *q = Q_FCIRC; return 0; }
        return 1;
    }
    case EL_VATI:
        x->tipo = SE_AMI; x->instr = 'W'; x->med = MED_CC;
        return 1;
    case EL_FUENTE:
    case EL_CORR: {
        int forma;
        x->tipo = e->tipo == EL_FUENTE ? SE_E : SE_J;
        if (tolera && c && e->variante == FORMA_FASOR && (e->val.est != M_DATO || c->frec.est != M_DATO)) {
            double v = e->val.est == M_DATO ? e->val.v : 1.0;
            memset(x->p, 0, sizeof x->p);
            x->p[0] = sqrt(2.0) * v;
            x->p[1] = c->frec.est == M_DATO ? c->frec.v : 1e3;
            x->p[2] = (e->val.est_ang == M_DATO ? e->val.ang : 0.0) + 90.0;
            forma = ONDA_SENO;
        } else if (!circ_onda_c(c, e, &forma, x->p, q)) return 0;
        x->forma = (unsigned char)forma;
        if (e->tipo == EL_FUENTE) {
            *q = Q_AUX;
            if (fuente_ideal_c(e)) x->val = 0.0;
            else if (e->aux.est == M_DATO && e->aux.v > 0.0) x->val = e->aux.v;
            else return 0;                           /* r buscada: no se emula */
        }
        return 1;
    }
    case EL_VOLT:
    case EL_AMP:
        x->instr = e->tipo == EL_VOLT ? 'V' : 'A';
        x->med = (unsigned char)(e->variante < MED_N ? e->variante : MED_CC);
        if (ES_IDEAL(e)) { x->tipo = e->tipo == EL_VOLT ? SE_VMI : SE_AMI; return 1; }
        x->tipo = SE_R; x->val = e->val.v;
        return e->val.est == M_DATO && e->val.v > 0.0;
    case EL_COND:
    case EL_BOB:
        x->tipo = e->tipo == EL_COND ? SE_C : SE_L;
        if (!valor_o(&e->val, e->tipo == EL_COND ? 1e-6 : 1e-3, tolera, &x->val)) return 0;
        m = &e->par[Q_P1 - Q_P1];                    /* V0 o I0 */
        x->ci = (unsigned char)(m->est == M_DATO);
        x->x0 = m->est == M_DATO ? m->v : 0.0;
        return 1;
    case EL_INTER:
        x->tipo = SE_S;
        x->cerrado0 = (unsigned char)(e->variante == INTER_CERRADO);
        m = &e->par[Q_T1 - Q_P1];
        x->t1 = m->est == M_DATO ? m->v : -1.0;
        m = &e->par[Q_T2 - Q_P1];
        x->t2 = m->est == M_DATO ? m->v : -1.0;
        return 1;
    default:
        return 0;
    }
}

/* Un hueco de la ficha: el dato, o la omisión de nolin.h si está vacío */
static double par_o(const Elemento *e, int q, double omision)
{
    const Mag *m = &e->par[q - Q_P1];
    return m->est == M_DATO ? m->v : omision;
}

/* Diodo, zener y LED (§3.A, D3-D6): el modelo y sus m[] (el contrato de
 * sim.h). -1 con *q = el hueco culpable si los parámetros no cuadran. */
static int el_diodo(const Elemento *e, SimEl *x, int *q)
{
    double *m = x->m;
    x->tipo = SE_D;
    memset(x->m, 0, sizeof x->m);
    switch (e->tipo) {
    case EL_DIODO:
        switch (e->variante) {
        case DIO_IDEAL:                                  /* Vγ = 0 con RON */
            x->modelo = NL_TRAMOS; m[1] = NL_RON; m[3] = NL_RON;
            return 1;
        case DIO_VG: {
            double rd = par_o(e, QD_RD, 0.0);
            x->modelo = NL_TRAMOS; m[0] = par_o(e, QD_VG, NL_VG); m[1] = rd > 0.0 ? rd : NL_RON; m[3] = NL_RON;
            return 1;
        }
        case DIO_EXP:
            x->modelo = NL_UNION; m[0] = par_o(e, QD_IS, NL_IS_D); m[1] = par_o(e, QD_N, NL_N); m[2] = par_o(e, QD_RS, NL_RS);
            break;
        default: {                                       /* el catálogo, sin mirar los huecos */
            const NlDiodo *cat = e->variante == DIO_1N4148 ? &NL_1N4148 : &NL_1N4007;
            x->modelo = NL_UNION; m[0] = cat->Is; m[1] = cat->n; m[2] = cat->Rs;
            break;
        }
        }
        break;
    case EL_ZENER: {
        double vz = par_o(e, QZ_VZ, NL_VZ), rz = par_o(e, QZ_RZ, 0.0), iz = par_o(e, QZ_IZ, NL_IZ);
        if (e->variante == ZEN_1N4733A) { vz = 5.1; iz = 49e-3; rz = 7.0; }
        if (e->variante == ZEN_LIBRO) {                  /* pasa por (-Vz, -Iz) con pendiente rz (D5) */
            x->modelo = NL_TRAMOS; m[0] = par_o(e, QZ_VG, NL_VG); m[1] = NL_RON;
            m[3] = rz > 0.0 ? rz : NL_RON;
            m[2] = vz - m[3] * iz;
            if (!(m[2] > 0.0)) { *q = QZ_RZ; return -1; }
            return 1;
        }
        /* exponencial y catálogo: la directa de 1e-14 y la ruptura por (-Vz, -Iz) */
        x->modelo = NL_UNION; m[0] = 1e-14; m[1] = 1.0;
        m[2] = rz - NL_VT / iz > 0.0 ? rz - NL_VT / iz : 0.0;
        m[3] = vz - m[2] * iz;
        m[4] = iz;
        if (!(m[3] > 0.0)) { *q = QZ_RZ; return -1; }   /* BV = 0 es «sin ruptura»: la perdería */
        break;
    }
    default: {                                           /* LED: la curva pasa por (Vf, If) (D6) */
        const NlColor *co = &NL_COLOR[e->variante];
        double vf = par_o(e, QL_VF, co->Vf), If = par_o(e, QL_IF, NL_IF), ij = If - NL_GMIN * vf, vd;
        x->modelo = NL_UNION; m[1] = co->n; m[2] = co->Rs;
        vd = vf - co->Rs * ij;
        if (!(vd > 0.0) || !(ij > 0.0)) { *q = QL_IF; return -1; }
        /* con la exponencial del modelo: por encima de 80 (Vf de más de ~4,2 V
         * con n = 2), la recta de nl_exp; con exp() no pasaría por (Vf, If) */
        {
            double u = vd / (co->n * NL_VT);
            m[0] = ij / ((u > NL_XLIM ? exp(NL_XLIM) * (1.0 + u - NL_XLIM) : exp(u)) - 1.0);
        }
        if (!(m[0] > 0.0)) { *q = QL_IF; return -1; }
        x->p[0] = par_o(e, QL_IM, NL_IM);                /* un D no usa p[]: Im (QUEMA) e If (brillo) */
        x->p[1] = If;
        break;
    }
    }
    m[5] = nl_vcrit(m[0], m[1] * NL_VT);
    return 1;
}

/* El error de el_de (r = 0 o -1) en el diagnóstico */
static void error_el(SimDiag *d, int r, int k, int q)
{
    d->elem = k;
    if (r < 0) { d->estado = SIM_MODELO; d->campo = q; }
    else if (q == Q_CTRL) { d->estado = SIM_CTL_MAL; d->campo = CTL_FALTA; d->nodo = -1; }
    else { d->estado = SIM_FALTA; d->campo = q; }
}

int sim_valida(const Circuito *c, SimDiag *d)
{
    static SimEl x;
    int k, q, r;
    memset(d, 0, sizeof *d);
    d->elem = d->nodo = -1;
    if (c->nel == 0) { d->estado = SIM_VACIO; return d->estado; }
    for (k = 0; k < c->nel; k++) {
        memset(&x, 0, sizeof x);
        r = el_de(c, &c->el[k], &x, &q, 0);
        if (r <= 0) { error_el(d, r, k, q); return d->estado; }
        if (x.tipo == SE_EC || x.tipo == SE_JC) {          /* la misma regla que la tabla */
            int m = circ_ctl_motivo(c, k);
            if (m != CTL_OK) { d->estado = SIM_CTL_MAL; d->elem = k; d->nodo = c->el[k].ref; d->campo = m; return d->estado; }
        }
    }
    return SIM_OK;
}

int sim_red_de(const Circuito *c, const Red *r, SimRed *s, SimDiag *d) { return sim_red_de_ex(c, r, s, d, 0); }

int sim_red_de_ex(const Circuito *c, const Red *r, SimRed *s, SimDiag *d, int tolera)
{
    int k, n, q;
    memset(d, 0, sizeof *d);
    d->elem = d->nodo = -1;
    memset(s, 0, sizeof *s);
    s->nn = r->nn;
    s->nel = r->nel;
    for (n = 0; n < r->nn; n++) s->ref[n] = (unsigned char)(r->ref[n] != 0);
    if (r->nel == 0) { d->estado = SIM_VACIO; return d->estado; }
    for (k = 0; k < r->nel; k++) {
        SimEl *x = &s->el[k];
        int e;
        x->k = (short)k;
        x->a = (short)r->a[k];
        x->b = (short)r->b[k];
        x->c = (short)r->c[k];
        e = el_de(c, &c->el[k], x, &q, tolera);
        if (e <= 0) { error_el(d, e, k, q); return d->estado; }
        if (x->tipo == SE_AO) {
            /* la salida se mide desde la referencia de su subcircuito (su
             * alimentación): la tierra o el - de una fuente independiente */
            x->nref = (short)red_ref_de(r, r->c[k]);
            if (!red_ref_propia(c, r, r->c[k]) || x->nref < 0) {
                d->estado = SIM_AO_REF; d->elem = k; d->campo = AOR_SIN_TIERRA; return d->estado;
            }
            if (x->nref == x->c) { d->estado = SIM_AO_REF; d->elem = k; d->campo = AOR_CORTO; return d->estado; }
        }
        if (x->tipo == SE_EC || x->tipo == SE_JC) {       /* en el orden de la tabla (SOL_CTL) */
            int m = circ_ctl_motivo(c, k);
            if (m != CTL_OK) { d->estado = SIM_CTL_MAL; d->elem = k; d->nodo = c->el[k].ref; d->campo = m; return d->estado; }
        }
    }
    return SIM_OK;
}

/* La red con la que se trabaja (copia: el llamador puede cambiar la suya) */
static SimRed R_;
/* Su generación: sube cada vez que R_ cambia (las tres asignaciones: la de
 * op_estructura, la de repone_estado y la de sim_topo_ca; nada más la
 * escribe). Las cachés que salen de R_ (tabla_tg, tabla_inv, fuente_en) se
 * rehacen cuando no es la suya. Nunca vale 0, el de una caché vacía */
static unsigned r_gen_ = 1;
static void red_nueva(void) { if (++r_gen_ == 0) r_gen_ = 1; }

/* Los bits de un double: las claves de las cachés (un t o un paso son el
 * mismo si son los mismos bits) y las comparaciones |a| > |b| sin coma
 * flotante */
static uint64_t bits_de(double x) { uint64_t u; memcpy(&u, &x, sizeof u); return u; }
/* fabs(a) > fabs(b) con los bits: sin signo, el orden de los enteros es el
 * de los double; un NaN (por encima del ∞) no es mayor que nada y nada es
 * mayor que él, como en coma flotante */
static int mayor_abs(double a, double b)
{
    uint64_t ma = bits_de(a) & 0x7FFFFFFFFFFFFFFFull, mb = bits_de(b) & 0x7FFFFFFFFFFFFFFFull;
    return ma <= 0x7FF0000000000000ull && ma > mb;
}
static int col_phi[MAX_NODOS];      /* columna del potencial; -1 = referencia */
typedef char cabe_lu[2 * MAX_EL <= LU_MAX ? 1 : -1];
static int col_br[MAX_EL];          /* columna de la corriente de rama, o -1  */
static int nincog;
static int npot;                    /* las primeras npot columnas son potenciales */

static int con_rama(const SimEl *e)
{
    switch (e->tipo) {
    case SE_E: return e->val == 0.0;
    case SE_C: case SE_L: case SE_S: case SE_AMI: return 1;
    case SE_EC: return 1;
    /* El diodo por tramos lleva su corriente como incógnita: con Ron = 1 mΩ,
     * G·V + I0 perdería 11 cifras por cancelación (V se conoce a 1e-16
     * absoluto); con la rama la I sale de la LU. Sigue siendo una G. */
    case SE_D: return e->modelo == NL_TRAMOS;
    case SE_AO: return e->modelo == NL_AO_IDEAL || e->m[4] == 0.0;   /* ideal, o real con Ro = 0 */
    default: return 0;
    }
}

static void mapa(void)
{
    int n, k;
    nincog = 0;
    for (n = 0; n < R_.nn; n++) col_phi[n] = R_.ref[n] ? -1 : nincog++;
    npot = nincog;
    for (k = 0; k < R_.nel; k++) col_br[k] = con_rama(&R_.el[k]) ? nincog++ : -1;
}

static int cphi(int n) { return n >= 0 ? col_phi[n] : -1; }

/* ---- El control de las controladas ----
 * La V o la I del elemento que manda (las de su tabla, D8) como
 * Σ c[i]·x[col[i]] más una parte conocida: la I de una E real o de una J
 * (*fuente = ese elemento, *f0 = 1/r o 1), que cada motor pone con su
 * excitación. V: (a, b) en pasivos, D e instrumentos; (b, a) en E, J, EC,
 * JC y una J apagada de la CA; (b, referencia) en el vatímetro. I: la de su
 * rama; (phi_a - phi_b)/R en una R; en una E real, (phi_a - phi_b)/r + E/r;
 * en una J, J; en un voltímetro ideal, 0. */
static int ctl_coef_en(const SimRed *s, const int *phi, const int *br, int k, int col[3], double c[3],
                       int *fuente, double *f0)
{
    const SimEl *e = &s->el[k], *x;
    int j;
    *fuente = -1; *f0 = 0.0;
    if (e->ctl < 0 || e->ctl >= s->nel) return 0;
    x = &s->el[e->ctl];
    j = br[e->ctl];
    if (!e->ctl_i) {
        int p = x->a, q = x->b;
        if (x->tipo == SE_E || x->tipo == SE_J || x->tipo == SE_EC || x->tipo == SE_JC ||
            (x->tipo == SE_VMI && x->instr == 'j')) { p = x->b; q = x->a; }
        else if (x->tipo == SE_AMI && x->instr == 'W') { p = x->b; q = -1; }
        col[0] = p >= 0 ? phi[p] : -1; c[0] = 1.0;
        col[1] = q >= 0 ? phi[q] : -1; c[1] = -1.0;
        return 2;
    }
    switch (x->tipo) {
    case SE_R:
        col[0] = phi[x->a]; c[0] = 1.0 / x->val;
        col[1] = phi[x->b]; c[1] = -1.0 / x->val;
        return 2;
    case SE_E:
        if (j >= 0) { col[0] = j; c[0] = 1.0; return 1; }
        col[0] = phi[x->a]; c[0] = 1.0 / x->val;
        col[1] = phi[x->b]; c[1] = -1.0 / x->val;
        *fuente = e->ctl; *f0 = 1.0 / x->val;
        return 2;
    case SE_J:
        *fuente = e->ctl; *f0 = 1.0;
        return 0;
    default:                                /* una rama (A, W, S, C, L, EC); V ideal: 0 */
        if (j >= 0) { col[0] = j; c[0] = 1.0; return 1; }
        return 0;
    }
}

int sim_ctl_coef(const SimRed *s, const SimMapa *mp, int k, int col[3], double c[3], int *fuente, double *f0)
{
    return ctl_coef_en(s, mp->phi, mp->br, k, col, c, fuente, f0);
}

int sim_ctl_motivo(const SimRed *s, int k)
{
    const SimEl *e = &s->el[k];
    int t;
    if (e->ctl < 0 || e->ctl >= s->nel) return CTL_FALTA;
    if (e->ctl == k) return CTL_SI_MISMA;
    t = s->el[e->ctl].tipo;
    if (t == SE_AO || t == SE_BJT || t == SE_MOS) return CTL_3PATAS;
    if (e->ctl_i && (t == SE_D || t == SE_Z || t == SE_JC)) return CTL_I_NO;
    return CTL_OK;
}

/* Las filas de una controlada (sin la ley de nodos de su rama, que pone la
 * línea común de cada ensamblado): EC, phi_b - phi_a - g·control = g·c0;
 * JC, g·control sale de a y entra en b */
static void estampa_ctl(const SimRed *s, const int *phi, const int *br, int k, SimSuma suma, void *m)
{
    const SimEl *e = &s->el[k];
    int col[3], i, nt, fu, ca = e->a >= 0 ? phi[e->a] : -1, cb = e->b >= 0 ? phi[e->b] : -1;
    double cf[3], f0;
    nt = ctl_coef_en(s, phi, br, k, col, cf, &fu, &f0);
    if (e->tipo == SE_EC) {
        int j = br[k];
        suma(m, j, cb, 1.0); suma(m, j, ca, -1.0);
        for (i = 0; i < nt; i++) suma(m, j, col[i], -e->val * cf[i]);
    } else
        for (i = 0; i < nt; i++) { suma(m, ca, col[i], e->val * cf[i]); suma(m, cb, col[i], -e->val * cf[i]); }
}

/* g(a, b, G) con la suma de quien llama */
static void suma_g(SimSuma suma, void *m, int ca, int cb, double G)
{
    suma(m, ca, ca, G); suma(m, cb, cb, G); suma(m, ca, cb, -G); suma(m, cb, ca, -G);
}

/* §3.C en la CA: las controladas y los no lineales linealizados en Q (sin el
 * término independiente y sin la ley de nodos de las ramas, que pone la línea
 * común de ensambla_en con el puerto 0): el diodo por tramos con la ley de su
 * tramo en su fila; la unión con su G; el AO con su tramo (saturado, la salida
 * fija, que en pequeña señal es 0); el BJT y el MOS con sus g en Q */
void sim_estampa_lin(const SimRed *s, const SimLin *L, const SimMapa *mp, SimSuma suma, void *m)
{
    int k;
    for (k = 0; k < s->nel; k++) {
        const SimEl *e = &s->el[k];
        int j = mp->br[k], ca = e->a >= 0 ? mp->phi[e->a] : -1, cb = e->b >= 0 ? mp->phi[e->b] : -1;
        int tau = L && L->hay ? L->tramo[k] : 0;
        switch (e->tipo) {
        case SE_EC:
        case SE_JC:
            estampa_ctl(s, mp->phi, mp->br, k, suma, m);
            break;
        case SE_D:
            if (e->modelo == NL_TRAMOS && j >= 0) {
                if (tau == TR_CORTE) { suma(m, j, j, 1.0); suma(m, j, ca, -NL_GMIN); suma(m, j, cb, NL_GMIN); }
                else { suma(m, j, ca, 1.0); suma(m, j, cb, -1.0); suma(m, j, j, -(tau == TR_CONDUCE ? e->m[1] : e->m[3])); }
            } else if (e->modelo == NL_UNION)
                suma_g(suma, m, ca, cb, L && L->hay ? L->g[k][0][0] : NL_GMIN);
            break;
        case SE_AO: {
            int cc = e->c >= 0 ? mp->phi[e->c] : -1, cr = e->nref >= 0 ? mp->phi[e->nref] : -1;
            double A = e->m[0];
            if (e->modelo == NL_AO_REAL && e->m[3] > 0.0) suma_g(suma, m, ca, cb, 1.0 / e->m[3]);
            if (j >= 0) {
                if (tau != TR_LIN) { suma(m, j, cc, 1.0); suma(m, j, cr, -1.0); }         /* vsal = 0 */
                else if (e->modelo == NL_AO_IDEAL) { suma(m, j, ca, 1.0); suma(m, j, cb, -1.0); }
                else { suma(m, j, cc, 1.0); suma(m, j, cr, -1.0); suma(m, j, ca, -A); suma(m, j, cb, A); }
            } else {
                double go = 1.0 / e->m[4];
                suma_g(suma, m, cc, cr, go);
                if (tau == TR_LIN) {
                    suma(m, cc, ca, -A * go); suma(m, cc, cb, A * go);
                    suma(m, cr, ca, A * go); suma(m, cr, cb, -A * go);
                }
            }
            break;
        }
        case SE_BJT:                                     /* sus g en Q: puertos (B, E) y (C, E) */
        case SE_MOS: {                                   /* (G, S) sin corriente y (D, S) */
            int P[2], p, q;
            P[0] = e->c >= 0 ? mp->phi[e->c] : -1; P[1] = ca;
            if (!(L && L->hay)) break;
            for (p = 0; p < 2; p++)
                for (q = 0; q < 2; q++) {
                    double g = L->g[k][p][q];
                    if (g == 0.0) continue;
                    suma(m, P[p], P[q], g); suma(m, P[p], cb, -g);
                    suma(m, cb, P[q], -g); suma(m, cb, cb, g);
                }
            break;
        }
        default:
            break;
        }
    }
}

/* Valor de una fuente: por la derecha (lo normal) o por la izquierda (el
 * paso que termina justo en un flanco, y el punto de operación en 0⁻).
 * La misma fuente se pide 2 o 3 veces con el mismo t (termino, valores_de,
 * rq_etapa1) y en cada iteración de Newton (termino y residuo_nodos): se
 * recuerda el último valor de cada una, por (t, lado) y la red (r_gen_).
 * Las operaciones se cuentan igual (el modelo de tiempos no cambia) */
static double fe_v_[MAX_EL];
static uint64_t fe_t_[MAX_EL];
static unsigned fe_gen_[MAX_EL];
static unsigned char fe_izq_[MAX_EL];
static unsigned long fe_n_;                 /* las veces que se pidió una fuente (sim_cc_punto) */

static double fuente_en(const SimEl *e, double t, int izq)
{
    int k = (int)(e - R_.el);                   /* siempre una de R_ */
    uint64_t bt = bits_de(t);
    ops_propias += (e->forma == ONDA_SENO) ? SIM_OPS_SIN + 4 : 4;     /* la sin() y lo de toda fuente */
    fe_n_++;
    izq = izq != 0;
    if (fe_gen_[k] != r_gen_ || fe_t_[k] != bt || fe_izq_[k] != izq) {
        fe_v_[k] = onda_valor_lado(e->forma, e->p, t, izq ? -1 : 1);
        fe_gen_[k] = r_gen_; fe_t_[k] = bt; fe_izq_[k] = (unsigned char)izq;
    }
    return fe_v_[k];
}

/* ==========================================================================
 *  2. Análisis estructural
 *
 *  Papel de cada elemento: impone una tensión (V), una corriente (I), es
 *  una conductancia (G) o nada. En el punto de operación un C es abierto
 *  (I = 0) o, con V0, una fuente de tensión; una L es un corto (V = 0) o,
 *  con I0, una fuente de corriente. En el transitorio C y L son G (sus
 *  compañeros).
 * ======================================================================= */
enum { ROL_G = 0, ROL_V, ROL_I, ROL_NADA };
enum { SU_NO = 0, SU_I0, SU_FLUJO, SU_ANCLA, SU_CARGA };

typedef struct {
    unsigned char rol[MAX_EL];
    unsigned char sust_el[MAX_EL];       /* sustitución de la fila de rama  */
    unsigned char sust_nodo[MAX_NODOS];  /* sustitución de la fila del nodo */
    int  pieza[MAX_NODOS];               /* trozo al que pertenece el nodo  */
    signed char flujo_s[MAX_EL][MAX_EL]; /* SU_FLUJO: signo de cada L del lazo */
    double ancla[MAX_NODOS];             /* SU_ANCLA: potencial fijado      */
} Topo;

static unsigned char ci_ef[MAX_EL];      /* C, L: la V0/I0 cuenta en el OP  */
static double        ci_val[MAX_EL];
static unsigned int  avisos;
static int           elem_aviso;

/* elem_aviso: el elemento del primer aviso que lo tiene. Los de un AO se
 * enseñan antes que los demás (P_EMU_ESQ, P_RESULTADO), y de ellos, el que
 * explica la saturación (BIESTABLE, ARRANQUE) antes que SATURA: el primero
 * del rango más alto se queda con su AO aunque otro (un C con CI_SALTA, u
 * otro AO que solo satura) haya avisado antes */
static int rango_aviso(unsigned int bits)
{
    return (bits & (SIM_AV_BIESTABLE | SIM_AV_ARRANQUE)) ? 2 : (bits & SIM_AV_SATURA) ? 1 : 0;
}
static void avisa(unsigned int bit, int k)
{
    if (!(avisos & bit) && (elem_aviso < 0 || rango_aviso(bit) > rango_aviso(avisos))) elem_aviso = k;
    avisos |= bit;
}

/* Unión y búsqueda sobre los nodos */
static int uf[MAX_NODOS];
static int uf_raiz(int i) { while (uf[i] != i) { uf[i] = uf[uf[i]]; i = uf[i]; } return i; }
static void uf_une(int i, int j) { i = uf_raiz(i); j = uf_raiz(j); if (i != j) uf[j] = i; }

static unsigned mask_de(double t, int izq);

/* Papel de un elemento. modo 0 = punto de operación; 1 = transitorio con los
 * interruptores de `mask` cerrados */
static int rol_de(int k, int modo, unsigned mask)
{
    const SimEl *e = &R_.el[k];
    switch (e->tipo) {
    case SE_R:   return ROL_G;
    case SE_Z:   return ROL_G;                   /* solo en la alterna */
    case SE_E:   return e->val > 0.0 ? ROL_G : ROL_V;
    case SE_J:   return ROL_I;
    case SE_VMI: return ROL_NADA;
    case SE_AMI: return ROL_V;
    case SE_S:   return (mask >> k) & 1u ? ROL_V : ROL_I;
    case SE_C:   return modo == 0 ? (ci_ef[k] ? ROL_V : ROL_I) : ROL_G;
    case SE_L:   return modo == 0 ? (ci_ef[k] ? ROL_I : ROL_V) : ROL_G;
    case SE_D:   return ROL_G;                   /* por tramos o suave: siempre una G */
    case SE_EC:  return ROL_V;
    case SE_JC:  return ROL_I;
    case SE_AO:  return (e->modelo == NL_AO_IDEAL || e->m[4] == 0.0) ? ROL_V : ROL_G;   /* su salida */
    case SE_BJT: return ROL_G;                   /* su puerto 0, B-E */
    case SE_MOS: return ROL_G;                   /* D-S (la puerta no conduce: no es un puerto) */
    default:     return ROL_NADA;
    }
}

/* Los puertos de un elemento en el análisis estructural (§3.B): sus nodos y
 * su papel. Los de dos bornes, uno: (a, b, rol_de). El AO, dos: la salida
 * (nref, c), V (ideal o Ro = 0) o G; y las entradas (a, b), G con Ri o nada.
 * El BJT, dos G: B-E (c, b) y B-C (c, a). El MOS, uno: D-S (a, b, G); su
 * puerta no conduce (una puerta que no toca nada más flota: se ancla). */
static int puertos(int k, int modo, unsigned mask, short pa[2], short pb[2], unsigned char rol[2])
{
    const SimEl *e = &R_.el[k];
    if (e->tipo == SE_AO) {
        pa[0] = e->nref; pb[0] = e->c;
        rol[0] = (unsigned char)rol_de(k, modo, mask);
        pa[1] = e->a; pb[1] = e->b;
        rol[1] = (unsigned char)((e->modelo == NL_AO_REAL && e->m[3] > 0.0) ? ROL_G : ROL_NADA);
        return 2;
    }
    if (e->tipo == SE_BJT) {
        pa[0] = e->c; pb[0] = e->b; rol[0] = ROL_G;
        pa[1] = e->c; pb[1] = e->a; rol[1] = ROL_G;
        return 2;
    }
    pa[0] = e->a; pb[0] = e->b;
    rol[0] = (unsigned char)rol_de(k, modo, mask);
    return 1;
}

/* Tensión que impone un elemento de papel V, como phi(a) - phi(b) */
static double v_impuesta(int k, double t)
{
    const SimEl *e = &R_.el[k];
    if (e->tipo == SE_E) return -fuente_en(e, t, 1);
    if (e->tipo == SE_C) return ci_val[k];
    return 0.0;
}

/* Corriente que impone un elemento de papel I, de a hacia b */
static double i_impuesta(int k, double t, int izq)
{
    const SimEl *e = &R_.el[k];
    if (e->tipo == SE_J) return fuente_en(e, t, izq);
    if (e->tipo == SE_L) return ci_val[k];
    return 0.0;
}

/* Camino por el bosque de ramas V entre a y b: lista de (elemento, signo) */
static int bosque_n;
static short bosque_a[MAX_EL], bosque_b[MAX_EL], bosque_k[MAX_EL];

static int camino(int a, int b, int *el, signed char *sg)
{
    int prev_k[MAX_NODOS], prev_n[MAX_NODOS], cola[MAX_NODOS], ini = 0, fin = 0, n, i, len = 0;
    for (n = 0; n < R_.nn; n++) prev_n[n] = -2;
    prev_n[a] = -1; cola[fin++] = a;
    while (ini < fin && prev_n[b] == -2) {
        int u = cola[ini++];
        for (i = 0; i < bosque_n; i++) {
            int v = -1;
            if (bosque_a[i] == u) v = bosque_b[i];
            else if (bosque_b[i] == u) v = bosque_a[i];
            if (v < 0 || prev_n[v] != -2) continue;
            prev_n[v] = u; prev_k[v] = i; cola[fin++] = v;
        }
    }
    if (prev_n[b] == -2) return -1;
    for (n = b; n != a; n = prev_n[n]) {
        int i2 = prev_k[n], k = bosque_k[i2];
        el[len] = k;
        /* se recorre de prev_n[n] a n: de a hacia b del puerto es +1 */
        sg[len] = (signed char)(bosque_a[i2] == prev_n[n] && bosque_b[i2] == n ? 1 : -1);
        len++;
    }
    return len;
}

/* ¿Tiene ley la corriente que circula por el lazo (k y los len del camino)?
 * Sí si una controlada (de cualquier sitio) lo está por la corriente de una
 * rama del lazo: esa corriente entra en su ley (la de una EC, o la ley de
 * nodos que toca una JC) y ya no circula libre. La matriz no queda singular
 * por el lazo (si lo es por las ganancias, lo dice la LU). */
static int lazo_con_ley(int k, const int *el, int len)
{
    unsigned lazo = 1u << k;
    int i, j;
    for (i = 0; i < len; i++) lazo |= 1u << el[i];
    for (j = 0; j < R_.nel; j++) {
        const SimEl *e = &R_.el[j];
        if ((e->tipo != SE_EC && e->tipo != SE_JC) || !e->ctl_i) continue;
        if (e->ctl >= 0 && e->ctl < R_.nel && ((lazo >> e->ctl) & 1u)) return 1;
    }
    return 0;
}

/* El segundo recorrido del §B (modo 0, solo con un AO ideal y algún C con V0
 * efectiva): los lazos de tensiones que cierra el nulador, que el primero no
 * ve (el nulador no lleva corriente). Union-find con lo que impone tensión: E
 * ideal, EC, la salida de un AO real con Ro = 0, A, S cerrado, L en corto y
 * el nulador de cada AO ideal (+ con -: el OP arranca con los AO en su
 * lineal); al final, los C con V0. La salida del AO ideal (el norador) no
 * entra: su tensión es libre (si entrara, el integrador perdería su V0). Un C
 * que cierra un lazo con un nulador: con una EC o la salida de un AO real en
 * él, CI_SALTA; si no, se comprueba con las tensiones conocidas: cuadra ->
 * SU_I0 (el C no lleva corriente en el OP); no -> CI_SALTA. */
static int segundo_recorrido(Topo *tp, double t)
{
    int k, n, o, hay_ao = 0, hay_c = 0;
    for (k = 0; k < R_.nel; k++) {
        if (R_.el[k].tipo == SE_AO && R_.el[k].modelo == NL_AO_IDEAL) hay_ao = 1;
        if (R_.el[k].tipo == SE_C && ci_ef[k] && tp->sust_el[k] == SU_NO) hay_c = 1;
    }
    if (!hay_ao || !hay_c) return SIM_OK;
    for (n = 0; n < R_.nn; n++) uf[n] = n;
    bosque_n = 0;
    for (o = 0; o < 2; o++)
        for (k = 0; k < R_.nel; k++) {
            const SimEl *e = &R_.el[k];
            int a = e->a, b = e->b, entra;
            if (o == 0) {                                /* lo que impone tensión */
                if (e->tipo == SE_AO) {
                    if (e->modelo == NL_AO_IDEAL) entra = 1;            /* el nulador: (a, b) */
                    else { entra = e->m[4] == 0.0; a = e->nref; b = e->c; }
                } else entra = (e->tipo == SE_E && e->val == 0.0) || e->tipo == SE_EC || e->tipo == SE_AMI ||
                               (e->tipo != SE_C && tp->rol[k] == ROL_V);
                if (!entra) continue;
                if (uf_raiz(a) != uf_raiz(b)) {
                    uf_une(a, b);
                    bosque_a[bosque_n] = (short)a; bosque_b[bosque_n] = (short)b; bosque_k[bosque_n] = (short)k;
                    bosque_n++;
                }
                continue;
            }
            /* o == 1: los C con V0 */
            if (e->tipo != SE_C || !ci_ef[k] || tp->sust_el[k] != SU_NO) continue;
            if (uf_raiz(a) != uf_raiz(b)) {
                uf_une(a, b);
                bosque_a[bosque_n] = (short)a; bosque_b[bosque_n] = (short)b; bosque_k[bosque_n] = (short)k;
                bosque_n++;
                continue;
            } else {
                int el[MAX_EL], len, i, nul = 0, dep = 0;
                signed char sg[MAX_EL];
                double res, esc;
                len = camino(a, b, el, sg);
                for (i = 0; i < len; i++) {
                    const SimEl *f = &R_.el[el[i]];
                    if (f->tipo == SE_AO && f->modelo == NL_AO_IDEAL) nul = 1;
                    if (f->tipo == SE_EC || (f->tipo == SE_AO && f->modelo == NL_AO_REAL)) dep = 1;
                }
                if (!nul) continue;                      /* ya lo miró el primer recorrido */
                if (dep) { ci_ef[k] = 0; avisa(SIM_AV_CI_SALTA, k); return -1; }
                res = ci_val[k];
                esc = 1.0 + fabs(res);
                for (i = 0; i < len; i++) {
                    const SimEl *f = &R_.el[el[i]];
                    double vi = f->tipo == SE_E ? -fuente_en(f, t, 1) : f->tipo == SE_C ? ci_val[el[i]] : 0.0;
                    res -= sg[i] * vi;
                    if (fabs(vi) + 1.0 > esc) esc = fabs(vi) + 1.0;
                }
                if (fabs(res) > 1e-9 * esc) { ci_ef[k] = 0; avisa(SIM_AV_CI_SALTA, k); return -1; }
                tp->sust_el[k] = SU_I0;
            }
        }
    return SIM_OK;
}

/* Analiza la topología. Rellena tp; devuelve SIM_OK, un error, o -1 si
 * cambió alguna condición inicial efectiva (hay que repetir). */
static int analiza(Topo *tp, int modo, unsigned mask, double t, SimDiag *d)
{
    /* Las ramas V de corriente libre; el C va el último (es él quien cierra
     * el lazo cuando lo hay) y la L antes que el C */
    static const unsigned char ORDEN[7] = { SE_E, SE_EC, SE_AO, SE_AMI, SE_S, SE_L, SE_C };
    int k, n, o, pz;

    memset(tp->sust_el, 0, sizeof tp->sust_el);
    memset(tp->sust_nodo, 0, sizeof tp->sust_nodo);
    for (k = 0; k < R_.nel; k++) tp->rol[k] = (unsigned char)rol_de(k, modo, mask);

    /* ---- Lazos de ramas V (por puertos: la salida del AO es (nref, c)) ---- */
    for (n = 0; n < R_.nn; n++) uf[n] = n;
    bosque_n = 0;
    for (o = 0; o < 7; o++)
        for (k = 0; k < R_.nel; k++) {
            const SimEl *e = &R_.el[k];
            int el[MAX_EL], len, i, dep;
            short pa[2], pb[2];
            unsigned char rp[2];
            signed char sg[MAX_EL];
            double res, esc;
            if (e->tipo != ORDEN[o]) continue;
            puertos(k, modo, mask, pa, pb, rp);
            if (rp[0] != ROL_V) continue;
            if (uf_raiz(pa[0]) != uf_raiz(pb[0])) {
                uf_une(pa[0], pb[0]);
                bosque_a[bosque_n] = pa[0]; bosque_b[bosque_n] = pb[0]; bosque_k[bosque_n] = (short)k;
                bosque_n++;
                continue;
            }
            len = camino(pa[0], pb[0], el, sg);
            /* Un lazo con una E controlada (cerrándolo o en el camino): su
             * tensión no se sabe antes de resolver. Si lo cierra un C con V0
             * (o una L en corto), el rescate de siempre: el C va abierto en el
             * OP y salta en 0+ (la L empieza sin corriente); si no, un lazo de
             * fuentes sin mirar si cuadra. */
            dep = e->tipo == SE_EC || e->tipo == SE_AO;
            for (i = 0; i < len && !dep; i++) dep = R_.el[el[i]].tipo == SE_EC || R_.el[el[i]].tipo == SE_AO;
            if (dep) {
                if (lazo_con_ley(k, el, len)) continue;
                if (modo == 0 && e->tipo == SE_C) { ci_ef[k] = 0; avisa(SIM_AV_CI_SALTA, k); return -1; }
                if (modo == 0 && e->tipo == SE_L) { ci_ef[k] = 1; ci_val[k] = 0.0; avisa(SIM_AV_L_SIN_I, k); return -1; }
                d->estado = SIM_LAZO_V; d->elem = k; d->t = modo == 0 ? -1.0 : t;
                d->lazo = 1u << k;
                for (i = 0; i < len; i++) d->lazo |= 1u << el[i];
                return d->estado;
            }
            res = v_impuesta(k, t);
            esc = 1.0 + fabs(res);
            for (i = 0; i < len; i++) {
                double vi = v_impuesta(el[i], t);
                res -= sg[i] * vi;
                if (fabs(vi) + 1.0 > esc) esc = fabs(vi) + 1.0;
            }
            if (fabs(res) > 1e-9 * esc) {                  /* el lazo no cuadra */
                if (modo == 0 && e->tipo == SE_L) {
                    ci_ef[k] = 1; ci_val[k] = 0.0; avisa(SIM_AV_L_SIN_I, k); return -1;
                }
                if (modo == 0 && e->tipo == SE_C) {
                    ci_ef[k] = 0; avisa(SIM_AV_CI_SALTA, k); return -1;
                }
                d->estado = SIM_LAZO_V; d->elem = k; d->t = modo == 0 ? -1.0 : t;
                d->lazo = 1u << k;
                for (i = 0; i < len; i++) d->lazo |= 1u << el[i];
                return d->estado;
            }
            if (modo == 0 && e->tipo == SE_C) { tp->sust_el[k] = SU_I0; continue; }
            if (modo == 0 && e->tipo == SE_L) {
                /* Bobinas en paralelo (o en un lazo de cables): en CC el
                 * reparto no lo fija ninguna ley de Ohm; lo fija el flujo,
                 * que empieza en cero: sum(signo·L·i) por el lazo = L_k·i_k.
                 * flujo_s[k][q] es el signo de la bobina q en el camino. */
                memset(tp->flujo_s[k], 0, sizeof tp->flujo_s[k]);
                for (i = 0; i < len; i++)
                    if (R_.el[el[i]].tipo == SE_L) tp->flujo_s[k][el[i]] = sg[i];
                tp->sust_el[k] = SU_FLUJO;
                avisa(SIM_AV_FLUJO, k);
                continue;
            }
            d->estado = SIM_LAZO_V; d->elem = k; d->t = modo == 0 ? -1.0 : t;   /* E, A o S en lazo */
            d->lazo = 1u << k;
            for (i = 0; i < len; i++) d->lazo |= 1u << el[i];
            return d->estado;
        }

    /* ---- El segundo recorrido (modo 0, con un AO ideal y algún C con V0):
     *      los lazos de tensiones que cierra el nulador ---- */
    if (modo == 0) {
        int e2 = segundo_recorrido(tp, t);
        if (e2 != SIM_OK) return e2;
    }

    /* ---- Trozos: nodos unidos por puertos G y V ---- */
    for (n = 0; n < R_.nn; n++) uf[n] = n;
    for (k = 0; k < R_.nel; k++) {
        short pa[2], pb[2];
        unsigned char rp[2];
        int p, np = puertos(k, modo, mask, pa, pb, rp);
        for (p = 0; p < np; p++)
            if (rp[p] == ROL_G || rp[p] == ROL_V) uf_une(pa[p], pb[p]);
    }
    for (n = 0; n < R_.nn; n++) tp->pieza[n] = uf_raiz(n);

    for (pz = 0; pz < R_.nn; pz++) {
        int a_tierra = 0, tieneC = 0, tieneJ = 0, tieneL = 0, rep = -1, desc = 0;
        double neto = 0.0, esc = 1.0;
        if (tp->pieza[pz] != pz) continue;                 /* no es raíz */
        for (n = 0; n < R_.nn; n++)
            if (tp->pieza[n] == pz) { if (R_.ref[n]) a_tierra = 1; if (rep < 0) rep = n; }
        if (a_tierra) continue;
        for (k = 0; k < R_.nel; k++) {
            const SimEl *e = &R_.el[k];
            short pa[2], pb[2];
            unsigned char rp[2];
            int ina, inb;
            double i;
            puertos(k, modo, mask, pa, pb, rp);        /* las entradas de un AO no llevan corriente */
            ina = tp->pieza[pa[0]] == pz; inb = tp->pieza[pb[0]] == pz;
            if (ina == inb || (rp[0] != ROL_I && rp[0] != ROL_NADA)) continue;
            i = rp[0] == ROL_I ? i_impuesta(k, t, modo == 0) : 0.0;
            neto += ina ? i : -i;
            if (fabs(i) + 1.0 > esc) esc = fabs(i) + 1.0;
            if (e->tipo == SE_J || e->tipo == SE_JC) tieneJ = 1;
            if (e->tipo == SE_JC) desc = 1;            /* una J de valor desconocido */
            if (e->tipo == SE_C && modo == 0 && !ci_ef[k]) tieneC = 1;
            if (e->tipo == SE_L && modo == 0 && ci_ef[k]) tieneL = 1;
        }
        if (modo == 0 && (fabs(neto) > 1e-9 * esc || desc) && tieneC) {
            /* una J carga un condensador: no hay régimen de CC; los C del
             * corte empiezan descargados */
            for (k = 0; k < R_.nel; k++) {
                const SimEl *e = &R_.el[k];
                if (e->tipo == SE_C && !ci_ef[k] && (tp->pieza[e->a] == pz) != (tp->pieza[e->b] == pz)) {
                    ci_ef[k] = 1; ci_val[k] = 0.0;
                }
            }
            avisa(SIM_AV_C_DESC, -1);
            return -1;
        }
        if (modo == 0 && tieneL && (fabs(neto) > 1e-9 * esc || tieneJ)) {
            /* las L con I0 del corte hacen de cable en CC (la J vuelve por
             * ellas); si su I0 no cuadra, además saltan en 0+ */
            for (k = 0; k < R_.nel; k++) {
                const SimEl *e = &R_.el[k];
                if (e->tipo == SE_L && ci_ef[k] && (tp->pieza[e->a] == pz) != (tp->pieza[e->b] == pz)) ci_ef[k] = 0;
            }
            if (fabs(neto) > 1e-9 * esc) avisa(SIM_AV_CI_SALTA, -1);
            return -1;
        }
        if (tieneJ && (modo == 1 || !tieneC)) {
            int q;
            d->lazo = 0;
            d->elem = -1;
            for (q = 0; q < R_.nel; q++) {
                const SimEl *e = &R_.el[q];
                short pa[2], pb[2];
                unsigned char rp[2];
                int p, np = puertos(q, modo, mask, pa, pb, rp);
                for (p = 0; p < np; p++) {
                    if ((tp->pieza[pa[p]] == pz) == (tp->pieza[pb[p]] == pz)) continue;
                    if (rp[p] != ROL_I && rp[p] != ROL_NADA) continue;
                    d->lazo |= 1u << q;
                    if ((e->tipo == SE_J || e->tipo == SE_JC) && d->elem < 0) d->elem = q;
                }
            }
            d->estado = SIM_CORTE_J; d->t = modo == 0 ? -1.0 : t;
            return d->estado;
        }
        /* El trozo flota: si cuelga de condensadores sin V0 (en el OP), su
         * carga total es cero; si no, se ancla */
        {
            int conC = 0;
            if (modo == 0)
                for (k = 0; k < R_.nel; k++) {
                    const SimEl *e = &R_.el[k];
                    if (e->tipo == SE_C && !ci_ef[k] && (tp->pieza[e->a] == pz) != (tp->pieza[e->b] == pz)) conC = 1;
                }
            if (conC) { tp->sust_nodo[rep] = SU_CARGA; avisa(SIM_AV_CARGA, -1); }
            else { tp->sust_nodo[rep] = SU_ANCLA; tp->ancla[rep] = 0.0; avisa(SIM_AV_FLOTA, -1); }
        }
    }
    /* Un grupo de trozos unidos solo por condensadores y sin tierra: la
     * carga fija las diferencias, pero hace falta un ancla en uno de ellos */
    if (modo == 0) {
        int grupo[MAX_NODOS];
        for (n = 0; n < R_.nn; n++) uf[n] = tp->pieza[n];
        for (k = 0; k < R_.nel; k++) {
            const SimEl *e = &R_.el[k];
            if (e->tipo == SE_C && !ci_ef[k]) uf_une(tp->pieza[e->a], tp->pieza[e->b]);
        }
        for (n = 0; n < R_.nn; n++) grupo[n] = uf_raiz(tp->pieza[n]);
        for (n = 0; n < R_.nn; n++) {
            int g = grupo[n], m, tierra = 0, hay = -1;
            if (tp->sust_nodo[n] != SU_CARGA) continue;
            for (m = 0; m < R_.nn; m++)
                if (grupo[m] == g) {
                    if (R_.ref[m]) tierra = 1;
                    if (tp->sust_nodo[m] == SU_ANCLA) tierra = 1;
                    if (tp->sust_nodo[m] == SU_CARGA && hay < 0) hay = m;
                }
            if (!tierra && hay == n) { tp->sust_nodo[n] = SU_ANCLA; tp->ancla[n] = 0.0; }
        }
    }
    return SIM_OK;
}

/* ==========================================================================
 *  2b. No lineales: la lista, el estado de Newton y los topes (Etapa 4)
 *
 *  Todo va detrás de NL.n: una red sin D, AO, BJT ni MOS sigue el camino de
 *  siempre bit a bit (D11).
 * ======================================================================= */
typedef struct {
    unsigned char tramo[MAX_EL];         /* D por tramos y AO                                 */
    double vlim[MAX_EL][2];              /* lo que se limita (nolin.h), de la última evaluación */
    NlLin  lin[MAX_EL];                  /* la última linealización de cada suave             */
    unsigned char frio;                  /* 1: aún no hay linealización (nl_arranque)         */
} NlEst;

static struct {
    int  n; short el[MAX_EL];            /* D, AO, BJT y MOS                                  */
    unsigned char suave, tramos;         /* hay alguno de cada clase                          */
    unsigned char dos;                   /* algún suave de dos puertos (BJT, MOS): el refinado */
    double Vesc, Iesc;                   /* las escalas de los criterios                      */
    int  it_op, it_tr;                   /* topes del arnés (sim_nl_topes); 0 = los de siempre */
    unsigned long tope_ops;              /* sim_nl_tope_ops; 0 = NL_OPS_OP                    */
    int  prueba; double t_prueba;        /* sim_nl_prueba                                     */
} NL;
static NlEst *nl_act;                    /* con el que se ensambla y se leen los valores      */
static double residuo_acum;              /* el mayor de todas las soluciones (el mono), con el
                                            suelo de redondeo de la escala (residuo_mono)     */
static double residuo_barr;              /* el mayor desde barr_iniciar_red (su kcl_peor): el
                                            barrido no pone a 0 el del mono                   */

static int es_nl(int tipo) { return tipo == SE_D || tipo == SE_AO || tipo == SE_BJT || tipo == SE_MOS; }
static int es_suave(const SimEl *e)
{
    return es_nl(e->tipo) && (e->modelo == NL_UNION || e->modelo == NL_BJT || e->modelo == NL_MOS);
}

static int primer_nl(const SimRed *s)
{
    int k;
    for (k = 0; k < s->nel; k++) if (es_nl(s->el[k].tipo)) return k;
    return -1;
}
int sim_hay_nl(const SimRed *s) { return primer_nl(s) >= 0; }

void   sim_nl_topes(int it_op, int it_tr)  { NL.it_op = it_op; NL.it_tr = it_tr; }
void   sim_nl_tope_ops(unsigned long ops)  { NL.tope_ops = ops; }
void   sim_nl_prueba(int que, double t)    { NL.prueba = que; NL.t_prueba = t; }
double sim_residuo_acum(void)              { return residuo_acum; }
void   sim_residuo_acum_reset(void)        { residuo_acum = 0.0; }
double sim_residuo_barr(void)              { return residuo_barr; }
void   sim_residuo_barr_reset(void)        { residuo_barr = 0.0; }

/* Lo más alto que da una fuente (para las escalas) */
static double amplitud(const SimEl *e)
{
    switch (e->forma) {
    case ONDA_CC:    return fabs(e->p[0]);
    case ONDA_PULSO: return fabs(e->p[0]) > fabs(e->p[1]) ? fabs(e->p[0]) : fabs(e->p[1]);
    default:         return fabs(e->p[0]) + fabs(e->p[3]);
    }
}

/* La lista de R_ y sus escalas: Vesc = máx(amplitud de las E, Vγ, Vz, V+ - V-,
 * 1 V); Iesc = máx(|J|, Vesc/Rmín). En st, el estado de reposo (D en corte,
 * AO en su lineal, sin linealización). */
static void nl_prepara(NlEst *st)
{
    int k;
    double jmax = 0.0, rmin = 0.0;
    NL.n = 0; NL.suave = NL.tramos = NL.dos = 0;
    NL.Vesc = 1.0;
    for (k = 0; k < R_.nel; k++) {
        const SimEl *e = &R_.el[k];
        double v;
        if (es_nl(e->tipo)) {
            NL.el[NL.n++] = (short)k;
            if (es_suave(e)) NL.suave = 1; else NL.tramos = 1;
            if (es_suave(e) && e->tipo != SE_D) NL.dos = 1;
            v = nl_escala_v(e);
            if (v > NL.Vesc) NL.Vesc = v;
        } else if (e->tipo == SE_E) { v = amplitud(e); if (v > NL.Vesc) NL.Vesc = v; }
        else if (e->tipo == SE_J) { v = amplitud(e); if (v > jmax) jmax = v; }
        else if (e->tipo == SE_R && (rmin == 0.0 || e->val < rmin)) rmin = e->val;
    }
    NL.Iesc = rmin > 0.0 ? NL.Vesc / rmin : NL.Vesc;
    if (jmax > NL.Iesc) NL.Iesc = jmax;
    if (st) { memset(st, 0, sizeof *st); st->frio = 1; }
}

/* Los puertos de un suave (§3.C): la unión, (a, b); el BJT y el MOS, (c, b)
 * y (a, b) */
static int npuertos(const SimEl *e) { return e->tipo == SE_D ? 1 : 2; }
static void nodos_puerto(const SimEl *e, int p, int *np, int *nm)
{
    *np = (e->tipo == SE_D || p == 1) ? e->a : e->c;
    *nm = e->b;
}
static double vnodo(const double *x, int n)
{
    int c = n >= 0 ? col_phi[n] : -1;
    return c >= 0 ? x[c] : 0.0;
}
static void puertos_v(const SimEl *e, const double *x, double v[2])
{
    int p, np, nm;
    v[0] = v[1] = 0.0;
    for (p = 0; p < npuertos(e); p++) { nodos_puerto(e, p, &np, &nm); v[p] = vnodo(x, np) - vnodo(x, nm); }
}

/* Evalúa los suaves de st en x con los límites de SPICE3 (nl_evalua); frío,
 * en su arranque (MODEINITJCT). 1 si alguno se limitó. */
static int evalua_suaves(NlEst *st, const double *x)
{
    int q, lim = 0;
    for (q = 0; q < NL.n; q++) {
        int k = NL.el[q];
        const SimEl *e = &R_.el[k];
        if (!es_suave(e)) continue;
        if (st->frio) { nl_arranque(e, st->vlim[k]); nl_evalua(e, 0, st->vlim[k], &st->lin[k]); }
        else {
            double vp[2];
            puertos_v(e, x, vp);
            lim |= nl_evalua(e, vp, st->vlim[k], &st->lin[k]);
        }
    }
    st->frio = 0;
    return lim;
}

/* ==========================================================================
 *  3. Ensamblado y término independiente
 * ======================================================================= */
static void estampa_g(LU *f, int a, int b, double G)
{
    int ca = cphi(a), cb = cphi(b);
    lu_suma(f, ca, ca, G); lu_suma(f, cb, cb, G);
    lu_suma(f, ca, cb, -G); lu_suma(f, cb, ca, -G);
    ops_propias += 4;
}

static void fila_a_cero(LU *f, int fila)
{
    if (fila >= 0 && fila < f->n) memset(f->a[fila], 0, (size_t)f->n * sizeof f->a[0][0]);
}

static void suma_lu(void *m, int i, int j, double v) { lu_suma((LU *)m, i, j, v); }

/* Los nodos del puerto 0 (la ley de nodos de su rama): (nref, c) en el AO,
 * (a, b) en los demás */
static void puerto0(const SimEl *e, int *p, int *q)
{
    if (e->tipo == SE_AO) { *p = e->nref; *q = e->c; }
    else { *p = e->a; *q = e->b; }
}

/* modo 0: punto de operación; modo 1: compañeros con el paso hh. tr: el
 * tramo de cada elemento por tramos (NULL: todos en corte) */
static void ensambla(LU *f, const Topo *tp, int modo, double hh, const unsigned char *tr)
{
    int k, n;
    lu_empieza(f, nincog);
    for (k = 0; k < R_.nel; k++) {
        const SimEl *e = &R_.el[k];
        int a = e->a, b = e->b, j = col_br[k], ca = cphi(a), cb = cphi(b);
        if (j >= 0) {                            /* la ley de nodos de la rama, una sola vez */
            int p, q;
            puerto0(e, &p, &q);
            lu_suma(f, cphi(p), j, 1.0); lu_suma(f, cphi(q), j, -1.0);
        }
        switch (e->tipo) {
        case SE_R:
            estampa_g(f, a, b, 1.0 / e->val);
            break;
        case SE_E:
            if (j >= 0) { lu_suma(f, j, cb, 1.0); lu_suma(f, j, ca, -1.0); }
            else estampa_g(f, a, b, 1.0 / e->val);
            break;
        case SE_AMI:
            lu_suma(f, j, ca, 1.0); lu_suma(f, j, cb, -1.0);
            break;
        case SE_S:
            if (tp->rol[k] == ROL_V) { lu_suma(f, j, ca, 1.0); lu_suma(f, j, cb, -1.0); }
            else lu_suma(f, j, j, 1.0);
            break;
        case SE_C:
            if (modo == 1) {                     /* i - g (phi_a - phi_b) = ... */
                double g = 2.0 * e->val / hh;
                lu_suma(f, j, j, 1.0); lu_suma(f, j, ca, -g); lu_suma(f, j, cb, g);
            } else if (tp->rol[k] == ROL_V && tp->sust_el[k] == SU_NO) {
                lu_suma(f, j, ca, 1.0); lu_suma(f, j, cb, -1.0);
            } else lu_suma(f, j, j, 1.0);        /* abierto, o SU_I0 */
            break;
        case SE_L:
            if (modo == 1) {                     /* (phi_a - phi_b) - z i = ... */
                double z = 2.0 * e->val / hh;
                lu_suma(f, j, ca, 1.0); lu_suma(f, j, cb, -1.0); lu_suma(f, j, j, -z);
            } else if (tp->sust_el[k] == SU_FLUJO) {
                int q;
                fila_a_cero(f, j);
                lu_suma(f, j, j, -e->val);       /* sum sg L i (camino) - L_k i_k = 0 */
                for (q = 0; q < R_.nel; q++)
                    if (tp->flujo_s[k][q]) lu_suma(f, j, col_br[q], tp->flujo_s[k][q] * R_.el[q].val);
            } else if (tp->rol[k] == ROL_V) {
                lu_suma(f, j, ca, 1.0); lu_suma(f, j, cb, -1.0);
            } else lu_suma(f, j, j, 1.0);        /* con I0 */
            break;
        case SE_EC:
        case SE_JC:
            estampa_ctl(&R_, col_phi, col_br, k, suma_lu, f);
            ops_propias += 6;
            break;
        case SE_D:                               /* por tramos: la ley del tramo en su fila */
            if (j >= 0) {
                int tau = tr ? tr[k] : TR_CORTE;
                if (tau == TR_CORTE) {           /* i - GMIN (phi_a - phi_b) = 0 */
                    lu_suma(f, j, j, 1.0); lu_suma(f, j, ca, -NL_GMIN); lu_suma(f, j, cb, NL_GMIN);
                } else {                         /* (phi_a - phi_b) - R i = ... (R = Ron o Rz) */
                    lu_suma(f, j, ca, 1.0); lu_suma(f, j, cb, -1.0);
                    lu_suma(f, j, j, -(tau == TR_CONDUCE ? e->m[1] : e->m[3]));
                }
                ops_propias += 3;
            }
            break;                               /* la unión: su compañero lo estampa el Newton */
        case SE_AO: {                            /* §3.C: la rama (si la hay) sale por (nref, c) */
            int tau = tr ? tr[k] : TR_LIN, cc = cphi(e->c), cr = cphi(e->nref);
            double A = e->m[0];
            if (e->modelo == NL_AO_REAL && e->m[3] > 0.0) estampa_g(f, a, b, 1.0 / e->m[3]);   /* Ri */
            if (j >= 0) {
                if (tau != TR_LIN) { lu_suma(f, j, cc, 1.0); lu_suma(f, j, cr, -1.0); }         /* Vsal = V± */
                else if (e->modelo == NL_AO_IDEAL) { lu_suma(f, j, ca, 1.0); lu_suma(f, j, cb, -1.0); }   /* V+ = V- */
                else {                                                                        /* Vsal = A·Vd */
                    lu_suma(f, j, cc, 1.0); lu_suma(f, j, cr, -1.0);
                    lu_suma(f, j, ca, -A); lu_suma(f, j, cb, A);
                }
            } else {                             /* real con Ro > 0: su Norton */
                double go = 1.0 / e->m[4];
                estampa_g(f, e->c, e->nref, go);
                if (tau == TR_LIN) {
                    lu_suma(f, cc, ca, -A * go); lu_suma(f, cc, cb, A * go);
                    lu_suma(f, cr, ca, A * go); lu_suma(f, cr, cb, -A * go);
                }
            }
            ops_propias += 6;
            break;
        }
        default:                                 /* J y voltímetro ideal: nada */
            break;
        }
    }
    /* Filas de nodo sustituidas: anclas y conservación de la carga */
    for (n = 0; n < R_.nn; n++) {
        int cn = col_phi[n];
        if (cn < 0 || tp->sust_nodo[n] == SU_NO) continue;
        fila_a_cero(f, cn);
        if (tp->sust_nodo[n] == SU_ANCLA) { lu_suma(f, cn, cn, 1.0); continue; }
        for (k = 0; k < R_.nel; k++) {           /* SU_CARGA: sum s C (phi_a - phi_b) = 0 */
            const SimEl *e = &R_.el[k];
            int ina = tp->pieza[e->a] == tp->pieza[n], inb = tp->pieza[e->b] == tp->pieza[n];
            double s;
            if (e->tipo != SE_C || ci_ef[k] || ina == inb) continue;
            s = ina ? e->val : -e->val;
            lu_suma(f, cn, cphi(e->a), s); lu_suma(f, cn, cphi(e->b), -s);
        }
    }
}

/* Inyección de una corriente I de a hacia b por dentro (sale de a) */
static void inyecta(double *rhs, int a, int b, double I)
{
    int ca = cphi(a), cb = cphi(b);
    if (ca >= 0) rhs[ca] -= I;
    if (cb >= 0) rhs[cb] += I;
}

/* Estado de C y L, y su historia para el trapecio */
static double vC[MAX_EL], iC[MAX_EL], iL[MAX_EL], vL[MAX_EL];

/* 2C/hh de cada C y 2L/hh de cada L para termino: la misma cuenta de
 * siempre, hecha una vez por red (r_gen_) y paso hh (sus bits), no en cada
 * llamada (33 ddiv por paso en el peor caso) */
static double tg_[MAX_EL];
static unsigned tg_gen_;
static uint64_t tg_hh_;
static const double *tabla_tg(double hh)
{
    uint64_t b = bits_de(hh);
    if (tg_gen_ != r_gen_ || tg_hh_ != b) {
        int k;
        for (k = 0; k < R_.nel; k++)
            if (R_.el[k].tipo == SE_C || R_.el[k].tipo == SE_L) tg_[k] = 2.0 * R_.el[k].val / hh;
        tg_gen_ = r_gen_; tg_hh_ = b;
    }
    return tg_;
}

/* metodo: 0 = punto de operación, 1 = trapecio, 2 = Euler implícito (con la
 * matriz del trapecio: hh = 2 * medio paso). tr: los tramos (NULL: corte);
 * lam: la rampa de fuentes (E y J por lam; 1 fuera de la rampa) */
static void termino(double *rhs, const Topo *tp, int metodo, double t, int izq, double hh,
                    const unsigned char *tr, double lam)
{
    const double *tg = 0;                        /* 2C/hh y 2L/hh (tabla_tg), si hacen falta */
    int k, n;
    for (k = 0; k < nincog; k++) rhs[k] = 0.0;
    for (k = 0; k < R_.nel; k++) {
        const SimEl *e = &R_.el[k];
        int j = col_br[k];
        switch (e->tipo) {
        case SE_E: {
            double u = fuente_en(e, t, izq);
            if (lam != 1.0) u *= lam;
            if (j >= 0) rhs[j] = u;
            else inyecta(rhs, e->a, e->b, u / e->val);
            break;
        }
        case SE_J: {
            double u = fuente_en(e, t, izq);
            if (lam != 1.0) u *= lam;
            inyecta(rhs, e->a, e->b, u);
            break;
        }
        case SE_EC:
        case SE_JC: {                            /* la parte conocida del control */
            int col[3], fu;
            double cf[3], f0, c0;
            ctl_coef_en(&R_, col_phi, col_br, k, col, cf, &fu, &f0);
            c0 = fu >= 0 ? f0 * fuente_en(&R_.el[fu], t, izq) : 0.0;
            if (lam != 1.0) c0 *= lam;
            if (e->tipo == SE_EC) rhs[j] = e->val * c0;
            else if (c0 != 0.0) inyecta(rhs, e->a, e->b, e->val * c0);
            break;
        }
        case SE_D:                               /* el término del tramo */
            if (j >= 0) {
                int tau = tr ? tr[k] : TR_CORTE;
                if (tau == TR_CONDUCE) rhs[j] = e->m[0] * (1.0 - e->m[1] * NL_GMIN);
                else if (tau == TR_RUPTURA) rhs[j] = -e->m[2] * (1.0 - e->m[3] * NL_GMIN);
            }
            break;
        case SE_AO: {                            /* saturado: la salida en su raíl (no entra en la rampa) */
            int tau = tr ? tr[k] : TR_LIN;
            double vs;
            if (tau == TR_LIN) break;
            vs = tau == TR_SATP ? e->m[1] : e->m[2];
            if (j >= 0) rhs[j] = vs;
            else inyecta(rhs, e->nref, e->c, vs / e->m[4]);
            break;
        }
        case SE_C:                               /* (x - 0.0 es x: el Euler va sin la resta) */
            if (metodo == 0) rhs[j] = (tp->rol[k] == ROL_V && tp->sust_el[k] == SU_NO) ? ci_val[k] : 0.0;
            else {
                if (!tg) tg = tabla_tg(hh);
                rhs[j] = metodo == 1 ? -tg[k] * vC[k] - iC[k] : -tg[k] * vC[k];
                ops_propias += 2;
            }
            break;
        case SE_L:
            if (metodo == 0) rhs[j] = (tp->rol[k] == ROL_I && tp->sust_el[k] == SU_NO) ? ci_val[k] : 0.0;
            else {
                if (!tg) tg = tabla_tg(hh);
                rhs[j] = metodo == 1 ? -tg[k] * iL[k] - vL[k] : -tg[k] * iL[k];
                ops_propias += 2;
            }
            break;
        default:
            break;
        }
    }
    for (n = 0; n < R_.nn; n++) {
        int cn = col_phi[n];
        if (cn < 0) continue;
        if (tp->sust_nodo[n] == SU_ANCLA) rhs[cn] = tp->ancla[n];
        else if (tp->sust_nodo[n] == SU_CARGA) rhs[cn] = 0.0;
    }
}

/* ==========================================================================
 *  4. Punto de operación
 * ======================================================================= */
static LU lu_op;
static unsigned lu_op_gen_;              /* sube cada vez que se arma lu_op (sim_cc_punto) */
static Topo topo_op;
static double x_op[LU_MAX];

/* El control de la controlada k con la solución x (en t; lam, la rampa) */
static double ctl_valor(const double *x, int k, double t, int izq, double lam)
{
    int col[3], i, nt, fu;
    double cf[3], f0, y = 0.0;
    nt = ctl_coef_en(&R_, col_phi, col_br, k, col, cf, &fu, &f0);
    for (i = 0; i < nt; i++) if (col[i] >= 0) y += cf[i] * x[col[i]];
    if (fu >= 0) y += lam * f0 * fuente_en(&R_.el[fu], t, izq);
    ops_propias += 6;
    return y;
}

/* La tensión interna u de un AO en su tramo: A·Vd en el lineal, V± saturado */
static double ao_u(const SimEl *e, int tramo, double vd)
{
    if (tramo == TR_SATP) return e->m[1];
    if (tramo == TR_SATN) return e->m[2];
    return e->m[0] * vd;
}

/* La corriente de una unión en la solución: la del modelo en su vd (la de la
 * última evaluación, que es la de x) más GMIN·V */
static double i_union(const NlLin *L, double V)
{
    return (L->i[0] - NL_GMIN * L->v[0]) + NL_GMIN * V;
}

/* Las corrientes de los puertos de un suave de dos puertos (el BJT: IB e IC;
 * el MOS: 0 e ID) en las tensiones de puerto vp, con la linealización L (la
 * de la última evaluación, que en una solución convergida es la de vp salvo
 * el redondeo de vCE = vBE - vBC: la corrección de primer orden lo recoge) */
static void i_puertos(const NlLin *L, const double vp[2], double ip[2])
{
    int p;
    for (p = 0; p < 2; p++)
        ip[p] = L->i[p] + L->g[p][0] * (vp[0] - L->v[0]) + L->g[p][1] * (vp[1] - L->v[1]);
    ops_propias += 8;
}

/* Valores de cada elemento a partir de x (convenios del motor de tabla) */
static void valores_de(const double *x, Valores *v, double t, int izq)
{
    int k, n;
    for (n = 0; n < R_.nn; n++) v->phi[n] = col_phi[n] >= 0 ? x[col_phi[n]] : 0.0;
    for (k = 0; k < R_.nel; k++) {
        const SimEl *e = &R_.el[k];
        double pa = v->phi[e->a], pb = v->phi[e->b], V2 = 0.0, I2 = 0.0;
        int j = col_br[k];
        v->aux[k] = 0.0;
        switch (e->tipo) {
        case SE_E:
            v->V[k] = pb - pa;
            v->val[k] = fuente_en(e, t, izq);
            if (j >= 0) v->I[k] = x[j];
            else { v->aux[k] = e->val; v->I[k] = (v->val[k] - v->V[k]) / e->val; }
            break;
        case SE_J:
            v->V[k] = pb - pa;
            v->val[k] = v->I[k] = fuente_en(e, t, izq);
            break;
        case SE_R:
            v->V[k] = pa - pb; v->val[k] = e->val; v->I[k] = v->V[k] / e->val;
            break;
        case SE_EC:                             /* como una fuente: V en bornes, I la que sale por + */
            v->V[k] = pb - pa; v->val[k] = e->val; v->I[k] = x[j];
            break;
        case SE_JC:
            v->V[k] = pb - pa; v->val[k] = e->val; v->I[k] = e->val * ctl_valor(x, k, t, izq, 1.0);
            break;
        case SE_D:                              /* por tramos, su rama; la unión, su modelo */
            v->V[k] = pa - pb; v->val[k] = 0.0;
            v->I[k] = j >= 0 ? x[j] : nl_act ? i_union(&nl_act->lin[k], v->V[k]) : 0.0;
            break;
        case SE_AO:                             /* V = Vsal, I la que entrega por c; V2 = Vd */
            v->V[k] = v->phi[e->c] - v->phi[e->nref]; v->val[k] = 0.0;
            v->I[k] = j >= 0 ? x[j] : (ao_u(e, nl_act ? nl_act->tramo[k] : TR_LIN, pa - pb) - v->V[k]) / e->m[4];
            break;
        case SE_BJT: {                          /* V = VCE, I = IC; V2 = VBE, I2 = IB */
            double vp[2], ip[2] = { 0.0, 0.0 };
            v->V[k] = pa - pb; v->val[k] = 0.0;
            puertos_v(e, x, vp);
            if (nl_act) i_puertos(&nl_act->lin[k], vp, ip);
            v->I[k] = ip[1];
            V2 = vp[0]; I2 = ip[0];
            break;
        }
        case SE_MOS: {                          /* V = VDS, I = ID; V2 = VGS (la puerta, sin corriente) */
            double vp[2], ip[2] = { 0.0, 0.0 };
            v->V[k] = pa - pb; v->val[k] = 0.0;
            puertos_v(e, x, vp);
            if (nl_act) i_puertos(&nl_act->lin[k], vp, ip);
            v->I[k] = ip[1];
            V2 = vp[0];
            break;
        }
        case SE_VMI:
            v->V[k] = pa - pb; v->val[k] = 0.0; v->I[k] = 0.0;
            break;
        case SE_AMI:
            if (e->instr == 'W') {              /* vatímetro: V de la bobina de tensión */
                v->I[k] = x[j]; v->V[k] = pb; v->val[k] = 0.0;
                break;
            }
            /* amperímetro ideal: como el resto */
            v->V[k] = pa - pb; v->val[k] = 0.0; v->I[k] = x[j];
            break;
        default:                                /* C, L, S */
            v->V[k] = pa - pb; v->val[k] = (e->tipo == SE_C || e->tipo == SE_L) ? e->val : 0.0;
            v->I[k] = j >= 0 ? x[j] : 0.0;
            break;
        }
        v->P[k] = v->V[k] * v->I[k];
        v->V2[k] = V2; v->I2[k] = I2;
        if (e->tipo == SE_BJT) v->P[k] += V2 * I2;   /* el BJT absorbe por sus dos puertos */
        if (e->tipo == SE_AO) {                 /* el 2.º puerto: las entradas (Ri); entrega V·I - V2·I2 */
            v->V2[k] = pa - pb;
            v->I2[k] = e->modelo == NL_AO_REAL && e->m[3] > 0.0 ? v->V2[k] / e->m[3] : 0.0;
            v->P[k] -= v->V2[k] * v->I2[k];
        }
        v->reg[k] = (unsigned char)(es_nl(e->tipo) ? nl_region(e, v, k) : REG_NADA);
    }
    ops_propias += (unsigned long)(4 * R_.nel);
}

/* ca = 1: la alterna (la Z vale); ca = 0: el tiempo (la Z no existe) */
static int valida(SimDiag *d, int ca)
{
    int k;
    for (k = 0; k < R_.nel; k++) {
        const SimEl *e = &R_.el[k];
        int mal = 0;
        if (e->a < 0 || e->b < 0 || e->a >= R_.nn || e->b >= R_.nn) mal = 1;
        switch (e->tipo) {
        case SE_R: case SE_C: case SE_L: mal |= !(e->val > 0.0); break;
        case SE_E: mal |= !(e->val >= 0.0); break;
        case SE_Z:
            if (!ca) { d->estado = SIM_SOLO_CA; d->elem = k; return 0; }
            mal |= !(e->val >= 0.0 && hypot(e->val, e->p[0]) > 0.0 && e->p[1] > 0.0);
            break;
        case SE_D: {
            const double *m = e->m;
            int bien;
            if (e->modelo == NL_TRAMOS) {
                if (!(m[2] >= 0.0)) { d->estado = SIM_MODELO; d->elem = k; d->campo = QZ_RZ; return 0; }
                bien = m[0] >= 0.0 && m[1] > 0.0 && m[3] > 0.0;
            } else
                bien = e->modelo == NL_UNION && m[0] > 0.0 && m[1] > 0.0 && m[2] >= 0.0 && m[3] >= 0.0 && m[4] >= 0.0 &&
                       fabs(m[5]) < 1e3;
            if (!bien && !mal) { d->estado = SIM_MODELO; d->elem = k; d->campo = Q_VAL; return 0; }
            break;
        }
        case SE_AO:
            if (!(e->m[1] > e->m[2])) { d->estado = SIM_MODELO; d->elem = k; d->campo = QA_VP; return 0; }
            if (e->modelo == NL_AO_REAL && !(e->m[0] > 0.0 && e->m[3] >= 0.0 && e->m[4] >= 0.0)) {
                d->estado = SIM_MODELO; d->elem = k; d->campo = Q_VAL; return 0;
            }
            if (e->modelo != NL_AO_REAL && e->modelo != NL_AO_IDEAL) { d->estado = SIM_MODELO; d->elem = k; d->campo = Q_VAL; return 0; }
            if (e->c < 0 || e->c >= R_.nn || e->nref < 0 || e->nref >= R_.nn) mal = 1;
            else if (e->c == e->nref) { d->estado = SIM_AO_REF; d->elem = k; d->campo = AOR_CORTO; return 0; }
            break;
        case SE_BJT: {                          /* Is, βF, βR > 0; VA >= 0; la base en rango */
            const double *m = e->m;
            if (e->c < 0 || e->c >= R_.nn) { mal = 1; break; }
            if (!(e->modelo == NL_BJT && (e->pol == 1 || e->pol == -1) && m[0] > 0.0 && m[1] > 0.0 && m[2] > 0.0 &&
                  m[3] >= 0.0 && fabs(m[5]) < 1e3)) {
                d->estado = SIM_MODELO; d->elem = k; d->campo = Q_VAL; return 0;
            }
            break;
        }
        case SE_MOS: {                          /* Vt >= 0, K > 0, VA >= 0; la puerta en rango */
            const double *m = e->m;
            if (e->c < 0 || e->c >= R_.nn) { mal = 1; break; }
            if (!(e->modelo == NL_MOS && (e->pol == 1 || e->pol == -1) && m[0] >= 0.0 && m[0] < 1e30 &&
                  m[1] > 0.0 && m[1] < 1e30 && m[2] >= 0.0 && m[2] < 1e30)) {
                d->estado = SIM_MODELO; d->elem = k; d->campo = Q_VAL; return 0;
            }
            break;
        }
        case SE_EC: case SE_JC: {
            int m;
            mal |= !(fabs(e->val) <= 1e30);       /* la ganancia: cualquier número (también < 0) */
            if (mal) break;
            m = sim_ctl_motivo(&R_, k);
            if (m != CTL_OK) {
                d->estado = SIM_CTL_MAL; d->elem = k; d->campo = m;
                d->nodo = (e->ctl >= 0 && e->ctl < R_.nel) ? e->ctl : -1;
                return 0;
            }
            break;
        }
        default: break;
        }
        if (mal) { d->estado = SIM_FALTA; d->elem = k; return 0; }
    }
    return 1;
}

/* --------------------------------------------------------------------------
 *  El Newton (§3.D): un solo núcleo para el punto de operación y el paso. La
 *  matriz y el término los dice NC: en el OP, la del modo 0 (topo_op) con
 *  los tramos de st; en el transitorio, la de la caché (máscara, tramos, hh)
 *  o la suelta de un paso partido.
 * ----------------------------------------------------------------------- */
enum { NLM_OP = 0, NLM_TR, NLM_BARR };
static struct {
    int modo, metodo, izq, suelta;
    double t, hh;
    unsigned mask;
    int fijo;              /* un AO que Katzenelson no mueve (la prueba del biestable), o -1 */
    int arranque0;         /* el primer medio paso en t = 0: una salida por la regla es ARRANQUE */
} NC;
static LU *matriz_tr(const NlEst *st, const Topo **tp, double (**base)[LU_MAX]);
static LU *regla_tr(const NlEst *st, const Topo **tp, double (**base)[LU_MAX]);
/* Una LU singular que el Newton resuelve (un AO en su lineal sin lazo) no
 * es un error de la emulación: el diagnóstico se repone */
static void diag_foto(void);
static void diag_vuelve(void);
enum { NLR_OK = 0, NLR_NOCONV, NLR_CICLA, NLR_SINGULAR, NLR_TOPE };
static LU lu_nl;                                   /* la del Newton con suaves (de usar y tirar) */
static double lu_base_op[LU_MAX][LU_MAX];          /* la del modo 0 con los tramos, sin factorizar */
static unsigned char op_tr[MAX_EL];                /* los tramos con que se armó lu_op (o la base) */
static int op_lista;                               /* 1: lu_op factorizada para op_tr; 2: la base */
static int nl_culpable, nl_col_mala;
static unsigned long nl_ops0;                      /* sim_ops() al empezar el OP (el tope)       */
static unsigned long nl_it_op, nl_it0;             /* las iteraciones fuera del paso, y al empezar */
static NlEst st_op;                                /* el estado de sim_op_red                    */
static SimNewton nw_op;                            /* y sus contadores                           */

static unsigned long tope_op(void) { return NL.tope_ops ? NL.tope_ops : NL_OPS_OP; }
/* El tope es de tiempo: las operaciones del OP y lo fijo de sus iteraciones,
 * NS_ITER cada una (463 operaciones de SIM_NS_OP; nolin.h) */
#define NL_OPS_IT ((unsigned long)(NS_ITER / SIM_NS_OP + 0.5))
static void tope_empieza(void) { nl_ops0 = sim_ops(); nl_it0 = nl_it_op; }
static int op_tope_fijo;                           /* el barrido: el tope cuenta desde su punto */

void sim_cc_tope(int empieza)
{
    op_tope_fijo = empieza;
    if (empieza) tope_empieza();
}

static int cc_con_residuo;                         /* sim_cc_punto calcula SimCc.residuo */
void sim_cc_residuo(int si) { cc_con_residuo = si; }

/* Con no lineales el pivote puede ser de GMIN: un bloque que solo cuelga de
 * diodos en corte (el C y la R de un puente con los cuatro apagados) es
 * regular gracias a GMIN = 1e-12 S (D3), y lu_factoriza lo daría por singular */
#define PIVOTE_NL 1e-20
static int factoriza_nl(LU *f) { return lu_factoriza_tol(f, PIVOTE_NL); }

/* La fila de ley de nodos del nodo n, o -1 (referencia, o sustituida) */
static int fila_libre(const Topo *tp, int n)
{
    return (n >= 0 && col_phi[n] >= 0 && tp->sust_nodo[n] == SU_NO) ? col_phi[n] : -1;
}

/* El compañero de un suave: g en la matriz f (si no es NULL) e ieq_p = i_p -
 * Σ g_pq·v_q en el término rhs (si no es NULL), como en el §3.C */
static void estampa_suave(LU *f, double *rhs, const Topo *tp, const SimEl *e, const NlLin *L)
{
    int p, q, P[2], Pm[2], np = npuertos(e);
    for (p = 0; p < np; p++) nodos_puerto(e, p, &P[p], &Pm[p]);
    for (p = 0; p < np; p++) {
        int rp = fila_libre(tp, P[p]), rm = fila_libre(tp, Pm[p]);
        double ieq = L->i[p];
        for (q = 0; q < np; q++) {
            double g = L->g[p][q];
            ieq -= g * L->v[q];
            if (f && g != 0.0) {
                int cp = cphi(P[q]), cm = cphi(Pm[q]);
                lu_suma(f, rp, cp, g); lu_suma(f, rp, cm, -g);
                lu_suma(f, rm, cp, -g); lu_suma(f, rm, cm, g);
            }
        }
        if (rhs) { if (rp >= 0) rhs[rp] -= ieq; if (rm >= 0) rhs[rm] += ieq; }
    }
    ops_propias += (unsigned long)(4 * np * np + 4 * np);
}

/* La matriz del modo 0 con los tramos de st: se arma (y se factoriza) solo si
 * los tramos cambiaron. Con suaves o gmin se deja sin factorizar en
 * lu_base_op, que cada iteración copia a lu_nl. 0 si es singular. */
static int matriz_op(const NlEst *st, int base)
{
    int i;
    if (op_lista == (base ? 2 : 1) && memcmp(op_tr, st->tramo, (size_t)R_.nel) == 0) return 1;
    ensambla(&lu_op, &topo_op, 0, 1.0, st->tramo);
    lu_op_gen_++;
    memcpy(op_tr, st->tramo, (size_t)R_.nel);
    if (base) {
        for (i = 0; i < nincog; i++) memcpy(lu_base_op[i], lu_op.a[i], (size_t)nincog * sizeof lu_op.a[0][0]);
        op_lista = 2;
        return 1;
    }
    op_lista = 0;
    if (!factoriza_nl(&lu_op)) { nl_col_mala = lu_op.col_mala; return 0; }
    op_lista = 1;
    return 1;
}

/* Las G de residuo_nodos: 1/R de cada R y de cada E real, 1/Ro y 1/Ri de
 * cada AO. La misma división de siempre (1.0/val), una vez por red (r_gen_)
 * y no en cada llamada (2,6 por paso en el puente). Se hacen todas, también
 * las que no se usarán (una E ideal: 1/0 = ∞, sin excepción) */
static double inv_[MAX_EL], inv_ri_[MAX_EL];
static unsigned inv_gen_;
static const double *tabla_inv(void)
{
    int k;
    if (inv_gen_ == r_gen_) return inv_;
    for (k = 0; k < R_.nel; k++) {
        const SimEl *e = &R_.el[k];
        if (e->tipo == SE_R || e->tipo == SE_E) inv_[k] = 1.0 / e->val;
        else if (e->tipo == SE_AO) { inv_[k] = 1.0 / e->m[4]; inv_ri_[k] = 1.0 / e->m[3]; }
    }
    inv_gen_ = r_gen_;
    return inv_;
}

/* El residuo de Kirchhoff de x (O(nel)): en cada nodo con su ley, lo que
 * sale por cada elemento con su ley de verdad (los suaves con la evaluación
 * de st, que es la de x) sobre la suma de los módulos. Las filas de rama son
 * lineales: las cumple la LU.
 * La suma de los módulos lleva además el ruido de redondeo de cada corriente
 * que sale de una resta de potenciales, 1e12·ε·G·(|φa| + |φb|): un diodo en
 * inversa en serie con una R deja en el nodo pocos pA mientras la R, con sus
 * potenciales de voltios, da la suya con 1e-18 A de redondeo (1e-7 relativo
 * de lo que pasa). Así «≤ 1e-12» es «relativo, o al nivel del redondeo».
 * vpiso: el suelo de |φa| + |φb| en ese término (0 = el de siempre, el del
 * motor; residuo_mono da el del mono). */
static double residuo_nodos(const double *x, const Topo *tp, const NlEst *st, double t, int izq, double lam, double vpiso)
{
    static double suma[MAX_NODOS], modu[MAX_NODOS];
    const double *inv = tabla_inv();
    int k, n, piso = vpiso > 0.0;                /* sin piso, s < vpiso nunca: s >= 0 */
    double r = 0.0, s;
    for (n = 0; n < R_.nn; n++) suma[n] = modu[n] = 0.0;
    for (k = 0; k < R_.nel; k++) {
        const SimEl *e = &R_.el[k];
        int j = col_br[k];
        double I, G = 0.0, va = vnodo(x, e->a), vb = vnodo(x, e->b), m;
        switch (e->tipo) {
        case SE_R:   G = inv[k]; I = (va - vb) * G; break;
        case SE_E:
            if (j >= 0) I = x[j];
            else { G = inv[k]; I = (lam * fuente_en(e, t, izq) - (vb - va)) * G; }
            break;
        case SE_J:   I = lam * fuente_en(e, t, izq); break;
        case SE_VMI: I = 0.0; break;
        case SE_JC:  I = e->val * ctl_valor(x, k, t, izq, lam); break;
        case SE_D:
            if (j >= 0) I = x[j];
            else { G = st->lin[k].g[0][0]; I = i_union(&st->lin[k], va - vb); }
            break;
        case SE_AO: {                            /* la salida entre nref y c; Ri entre a y b */
            double vc = vnodo(x, e->c), vr = vnodo(x, e->nref), Io, Gi = 0.0;
            if (j >= 0) Io = x[j];
            else { G = inv[k]; Io = (ao_u(e, st->tramo[k], va - vb) - (vc - vr)) * G; }
            m = fabs(Io) + 2.2e-4 * G * (fabs(vc) + fabs(vr));
            /* en su lineal, u = A·(V+ - V-): el redondeo de V+ y V- llega a Io
             * multiplicado por A/Ro (1333 S con 1e5 y 75 Ω) */
            if (st->tramo[k] == TR_LIN) m += 2.2e-4 * fabs(e->m[0]) * G * (fabs(va) + fabs(vb));
            suma[e->nref] += Io; modu[e->nref] += m;
            suma[e->c] -= Io; modu[e->c] += m;
            if (e->modelo == NL_AO_REAL && e->m[3] > 0.0) Gi = inv_ri_[k];
            I = (va - vb) * Gi; G = Gi;
            break;
        }
        case SE_BJT:                             /* IB entra por B (c) e IC por C (a); salen por E */
        case SE_MOS: {                           /* ID entra por D (a) y sale por S; por G, nada */
            const NlLin *L = &st->lin[k];
            double vp[2], ip[2], vc = vnodo(x, e->c);
            puertos_v(e, x, vp);
            i_puertos(L, vp, ip);
            /* el redondeo de cada corriente: el de las tensiones de sus dos puertos */
            m = fabs(ip[0]) + 2.2e-4 * (fabs(L->g[0][0]) * (fabs(vc) + fabs(vb)) + fabs(L->g[0][1]) * (fabs(va) + fabs(vb)));
            suma[e->c] += ip[0]; modu[e->c] += m;
            suma[e->b] -= ip[0]; modu[e->b] += m;
            m = fabs(ip[1]) + 2.2e-4 * (fabs(L->g[1][0]) * (fabs(vc) + fabs(vb)) + fabs(L->g[1][1]) * (fabs(va) + fabs(vb)));
            suma[e->a] += ip[1]; modu[e->a] += m;
            suma[e->b] -= ip[1]; modu[e->b] += m;
            continue;                            /* sus dos puertos ya están */
        }
        default:     I = j >= 0 ? x[j] : 0.0; break;          /* C, L, S, A, W, EC */
        }
        s = fabs(va) + fabs(vb);
        if (piso && s < vpiso) s = vpiso;
        m = fabs(I) + 2.2e-4 * G * s;
        if (e->a >= 0) { suma[e->a] += I; modu[e->a] += m; }
        if (e->b >= 0) { suma[e->b] -= I; modu[e->b] += m; }
    }
    for (n = 0; n < R_.nn; n++) {                /* con el piso de los incrementos: 1e-15 A + 1e-9·Iesc */
        double q;
        if (fila_libre(tp, n) < 0) continue;
        q = fabs(suma[n]) / (modu[n] + 1e-15 + 1e-9 * NL.Iesc);
        if (q > r) r = q;
    }
    return r;
}

/* El del motor (las decisiones de Newton, SimNewton.residuo_max, la tabla):
 * el ruido de cada corriente, con los potenciales de sus dos bornes */
static double residuo_de(const double *x, const Topo *tp, const NlEst *st, double t, int izq, double lam)
{
    double r = residuo_nodos(x, tp, st, t, izq, lam, 0.0);
    ops_propias += (unsigned long)(6 * R_.nel);
    return r;
}

/* El del mono (sim_residuo_acum), que busca soluciones que Newton acepta sin
 * cumplir Kirchhoff. Un potencial que sale de restar otros (el de un nodo sin
 * corriente, φ = φ1 - E) lleva el redondeo de ESOS, ε·|φ1|, aunque él valga
 * 0: con 1 ulp de 0,99 V en un nodo que tendría que dar 0, su R de 10 Ω
 * (1,1e-17 A) frente al piso 1e-9·Iesc = 1e-10 A da 1,1e-7, un «Kirchhoff
 * roto» que es redondeo. Aquí el suelo de |φa| + |φb| es el mayor |φ| de la
 * red: el ruido de ese término pasa de ε·(los suyos) a ε·(la escala). En un
 * nodo con corriente no cambia nada (manda |I|); en uno sin ella, en vez de
 * pedir 1e-18·Iesc (por debajo de un ulp) pide 1000 ulps de G·(la escala),
 * como el resto de la métrica a 1e-9. No cuenta operaciones (ni las que
 * suma residuo_nodos con fuente_en, ctl_valor e i_puertos, ni las fuentes
 * pedidas): no decide nada del motor, y solo se mira cuando el del motor,
 * que nunca es menor, pasa del acumulado del arnés, que depende de lo
 * emulado antes; si contara, el gasto de una emulación (el control del
 * gasto, t_est) dependería de la historia. */
static double residuo_mono(const double *x, const Topo *tp, const NlEst *st, double t, int izq, double lam)
{
    unsigned long o = ops_propias, f = fe_n_;
    double esc = 0.0, r;
    int n;
    for (n = 0; n < R_.nn; n++) if (fabs(vnodo(x, n)) > esc) esc = fabs(vnodo(x, n));
    r = residuo_nodos(x, tp, st, t, izq, lam, esc);
    ops_propias = o; fe_n_ = f;
    return r;
}

/* Incrementos pequeños: |xn - x| <= 1e-6·máx(|xn|, |x|) + tol, con tol =
 * 1e-9·Vesc en los potenciales y 1e-15 A + 1e-9·Iesc en las ramas */
static int incrementos(const double *x, const double *xn)
{
    int i;
    for (i = 0; i < nincog; i++) {
        double tol = i < npot ? 1e-9 * NL.Vesc : 1e-15 + 1e-9 * NL.Iesc;
        double m = fabs(xn[i]) > fabs(x[i]) ? fabs(xn[i]) : fabs(x[i]);
        if (fabs(xn[i] - x[i]) > 1e-6 * m + tol) return 0;
    }
    return 1;
}

/* Las leyes: en cada puerto de un suave, la corriente del modelo en la v
 * nueva frente a la del compañero con que se resolvió (ant) en esa v */
static int leyes(const NlEst *st, const NlLin *ant)
{
    int q, p, r;
    for (q = 0; q < NL.n; q++) {
        int k = NL.el[q];
        const SimEl *e = &R_.el[k];
        const NlLin *L = &st->lin[k], *A = &ant[k];
        if (!es_suave(e)) continue;
        for (p = 0; p < npuertos(e); p++) {
            double il = A->i[p], m;
            for (r = 0; r < npuertos(e); r++) il += A->g[p][r] * (L->v[r] - A->v[r]);
            m = fabs(L->i[p]) > fabs(il) ? fabs(L->i[p]) : fabs(il);
            if (fabs(L->i[p] - il) > 1e-6 * m + 1e-15 + 1e-9 * NL.Iesc) return 0;
        }
    }
    return 1;
}

/* Con un BJT, la de lu_nl antes de factorizar: el refinado (abajo) */
static double a_nl[LU_MAX][LU_MAX];

/* Un paso de refinado iterativo en la precisión de trabajo (con un suave de
 * dos puertos). La transconductancia del BJT hace de pivote a la fila del
 * colector en la columna de la base (gm = β/rπ): la fila de la base, de
 * conductancias de pA/V, se mezcla con las de las R del colector y la
 * sustitución la cumple solo a ε·|A|·|x| de la fila grande (con la base al
 * aire, IB = 3e-21 A frente a sus 5e-12 A: el residuo, 7e-10). El punto
 * fijo del Newton hereda ese error. Un paso con el residuo de la matriz sin
 * factorizar, fila a fila, lo baja a ε de cada fila (Skeel): r = b - A·x,
 * x += A⁻¹·r. */
static void refina_nl(const double *rhs, double *x)
{
    static double r[LU_MAX], dx[LU_MAX];
    int i, j, n = nincog;
    for (i = 0; i < n; i++) {
        double s = rhs[i];
        for (j = 0; j < n; j++) if (a_nl[i][j] != 0.0) s -= a_nl[i][j] * x[j];
        r[i] = s;
    }
    lu_resuelve(&lu_nl, r, dx);
    for (i = 0; i < n; i++) x[i] += dx[i];
    ops_propias += (unsigned long)(3 * n * n / 4 + 2 * n);
}

/* La matriz de la iteración: la factorizada de los tramos (sin suaves ni
 * gmin) o la base con los compañeros de los suaves y gd, en lu_nl. En *tpp la
 * topología (sus filas sustituidas y sus anclas). NULL si es singular. */
static LU *matriz_nl(const NlEst *st, double gd, const Topo **tpp)
{
    double (*bs)[LU_MAX] = 0;
    int i, q, n = nincog;
    if (NC.modo != NLM_TR) {
        *tpp = &topo_op;
        if (!NL.suave && gd == 0.0) return matriz_op(st, 0) ? &lu_op : 0;
        matriz_op(st, 1);
        bs = lu_base_op;
    } else {
        LU *f = matriz_tr(st, tpp, &bs);
        if (!f) return 0;
        if (!NL.suave && gd == 0.0) return f;        /* ya factorizada */
    }
    lu_empieza(&lu_nl, n);
    for (i = 0; i < n; i++) memcpy(lu_nl.a[i], bs[i], (size_t)n * sizeof lu_nl.a[0][0]);
    for (q = 0; q < NL.n; q++)
        if (es_suave(&R_.el[NL.el[q]])) estampa_suave(&lu_nl, 0, *tpp, &R_.el[NL.el[q]], &st->lin[NL.el[q]]);
    if (gd > 0.0)
        for (i = 0; i < R_.nn; i++) { int c = fila_libre(*tpp, i); if (c >= 0) lu_suma(&lu_nl, c, c, gd); }
    if (NL.dos) for (i = 0; i < n; i++) memcpy(a_nl[i], lu_nl.a[i], (size_t)n * sizeof a_nl[0][0]);
    if (!factoriza_nl(&lu_nl)) { nl_col_mala = lu_nl.col_mala; return 0; }
    return &lu_nl;
}

/* ---- La regla de la realimentación del AO (§3.F) ---- */
static LU lu_regla;                      /* con suaves: la base con los compañeros */

/* La LU con que se mira la regla con los tramos de st: en el OP, la del
 * modo 0; en el transitorio, siempre la de paso h (la del trapecio con esos
 * tramos): con la del arranque (h/4) diría otra cosa y el AO iría y vendría
 * en cada evento. NULL si es singular. */
static LU *lu_de_regla(const NlEst *st, const Topo **tpp)
{
    double (*bs)[LU_MAX] = 0;
    int i, q, n = nincog;
    if (NC.modo != NLM_TR) {
        *tpp = &topo_op;
        if (!NL.suave) return matriz_op(st, 0) ? &lu_op : 0;
        matriz_op(st, 1);
        bs = lu_base_op;
    } else {
        LU *f = regla_tr(st, tpp, &bs);
        if (!f) return 0;
        if (!NL.suave) return f;
    }
    lu_empieza(&lu_regla, n);
    for (i = 0; i < n; i++) memcpy(lu_regla.a[i], bs[i], (size_t)n * sizeof lu_regla.a[0][0]);
    for (q = 0; q < NL.n; q++)
        if (es_suave(&R_.el[NL.el[q]])) estampa_suave(&lu_regla, 0, *tpp, &R_.el[NL.el[q]], &st->lin[NL.el[q]]);
    return factoriza_nl(&lu_regla) ? &lu_regla : 0;
}

/* La salida de un AO en x */
static double ao_vsal(const SimEl *e, const double *x) { return vnodo(x, e->c) - vnodo(x, e->nref); }

/* La regla del AO ideal k: dx es la respuesta a δ = 1 en su fila, y vale si
 * S = dVsal/dδ < 0. Si su salida la fija el nulador de otro ideal en su
 * lineal (un seguidor dentro del lazo de otro AO, el detector de pico con
 * seguidor), S = 0 y no dice nada. El ideal es el límite del real (Ro = 0)
 * con A = 1/ε → ∞; con todos los ideales en LIN así, S(ε) = [(M - εI)⁻¹]kk
 * = Σ ε^(m-1)·[M^-m]kk, con M⁻¹ = dVsal/dδ de esos AO, y decide el primer
 * término que no es nulo (el del real con A grande). Cada término es una
 * sustitución más, con los δ de los otros = sus Vsal de la anterior (la de
 * k es nula); a lo más uno por ideal en LIN. Nulo: <= 1e-12 de lo que se
 * mueve lo demás. Sin otro ideal que se mueva, la regla de siempre. */
static int ideal_vale(LU *f, int k, const unsigned char *tr, double *dx)
{
    static double rhs[LU_MAX];
    const SimEl *e = &R_.el[k];
    double s1 = ao_vsal(e, dx), s = s1, esc = 1.0;
    int i, q, m, nid = 0;
    for (q = 0; q < NL.n; q++) {
        const SimEl *o = &R_.el[NL.el[q]];
        if (o->tipo == SE_AO && o->modelo == NL_AO_IDEAL && tr[NL.el[q]] == TR_LIN) nid++;
    }
    for (m = 1; ; m++) {
        double otros = 0.0;
        for (q = 0; q < NL.n; q++) {
            int kq = NL.el[q];
            const SimEl *o = &R_.el[kq];
            if (kq != k && o->tipo == SE_AO && o->modelo == NL_AO_IDEAL && tr[kq] == TR_LIN &&
                fabs(ao_vsal(o, dx)) > otros) otros = fabs(ao_vsal(o, dx));
        }
        if (otros > esc) esc = otros;
        if (fabs(s) > 1e-12 * esc) return s < 0.0;
        if (otros == 0.0 || m >= nid) return s1 < 0.0;
        for (i = 0; i < nincog; i++) rhs[i] = 0.0;
        esc = 0.0;
        for (q = 0; q < NL.n; q++) {
            int kq = NL.el[q];
            const SimEl *o = &R_.el[kq];
            if (kq != k && o->tipo == SE_AO && o->modelo == NL_AO_IDEAL && tr[kq] == TR_LIN) {
                rhs[col_br[kq]] = ao_vsal(o, dx);
                if (fabs(rhs[col_br[kq]]) > esc) esc = fabs(rhs[col_br[kq]]);
            }
        }
        ops_propias += (unsigned long)(nincog + 4 * NL.n);
        lu_resuelve(f, rhs, dx);
        s = ao_vsal(e, dx);
    }
}

/* ¿Vale el lineal del AO k con la LU f (los tramos tr, k en LIN)? Una
 * sustitución con una perturbación (§3.F): el ideal, δ en V+ - V-, y vale si
 * dVsal/dδ < 0 (ideal_vale); el real con Ro = 0, δ en Vsal - A·Vd, y vale si
 * dVsal/dδ > 0; el real con Ro > 0, δ en su fuente interna, y vale si
 * du/dδ > 0 */
static int ao_lineal_vale(LU *f, const Topo *tp, int k, const unsigned char *tr)
{
    static double rhs[LU_MAX], dx[LU_MAX];
    const SimEl *e = &R_.el[k];
    int i, j = col_br[k];
    if (NL.prueba == NLP_INESTABLE && NC.modo == NLM_TR && NC.t >= NL.t_prueba) return 0;   /* el gancho (Zenón) */
    for (i = 0; i < nincog; i++) rhs[i] = 0.0;
    if (j >= 0) rhs[j] = 1.0;
    else {
        int cr = fila_libre(tp, e->nref), cc = fila_libre(tp, e->c);
        if (cr >= 0) rhs[cr] -= 1.0 / e->m[4];
        if (cc >= 0) rhs[cc] += 1.0 / e->m[4];
    }
    lu_resuelve(f, rhs, dx);
    if (e->modelo == NL_AO_IDEAL) return ideal_vale(f, k, tr, dx);
    if (j >= 0) return ao_vsal(e, dx) > 0.0;
    return 1.0 + e->m[0] * (vnodo(dx, e->a) - vnodo(dx, e->b)) > 0.0;
}

/* ¿Valdría el lineal del AO k con los demás tramos de st? */
static int lin_vale(NlEst *st, int k)
{
    unsigned char viejo = st->tramo[k];
    const Topo *tp;
    LU *f;
    int ok;
    st->tramo[k] = TR_LIN;
    f = lu_de_regla(st, &tp);
    ok = f ? ao_lineal_vale(f, tp, k, st->tramo) : 0;   /* singular: un comparador sin lazo */
    st->tramo[k] = viejo;
    return ok;
}

/* La u de un AO en x (el ideal, su salida) y la mitad de sus raíles */
static double ao_u_de(const SimEl *e, const double *x)
{
    if (e->modelo == NL_AO_IDEAL) return vnodo(x, e->c) - vnodo(x, e->nref);
    return e->m[0] * (vnodo(x, e->a) - vnodo(x, e->b));
}
static double ao_margen(const SimEl *e, int tramo, const double *x)
{
    return nl_ao_margen(e, tramo, vnodo(x, e->a) - vnodo(x, e->b), vnodo(x, e->c) - vnodo(x, e->nref));
}
/* Desde el lineal, a la saturación del lado de u (V+ si u >= la mitad) */
static int ao_satura(const SimEl *e, const double *x)
{
    return ao_u_de(e, x) >= 0.5 * (e->m[1] + e->m[2]) ? TR_SATP : TR_SATN;
}

/* El margen de un elemento por tramos (diodo o AO) en x; en el lineal del AO
 * y en el corte del diodo, si lado >= 0, el de la recta del lado por el que
 * sale (lado = el tramo al que va) */
static double margen_tr(const SimEl *e, int tramo, const double *x, int lado)
{
    if (e->tipo == SE_AO) {
        if (tramo == TR_LIN && lado >= 0) {
            double u = ao_u_de(e, x);
            return lado == TR_SATP ? e->m[1] - u : u - e->m[2];
        }
        return ao_margen(e, tramo, x);
    } else {
        double v[2];
        puertos_v(e, x, v);
        if (tramo == TR_CORTE && lado >= 0) return -nl_tr_margen(e, lado, v[0]);
        return nl_tr_margen(e, tramo, v[0]);
    }
}
static int es_tramos(const SimEl *e) { return e->tipo == SE_AO || (e->tipo == SE_D && e->modelo == NL_TRAMOS); }

/* ¿Lleva el diodo por tramos k, en conducción o en ruptura, corriente del
 * sentido que su tramo no admite, más de 1e-15 A + 1e-9·Iesc? La de su rama
 * (la LU la da sin cancelar). Su margen en V es R·(i - la de la esquina), y
 * con Ron = 1 mΩ el -1e-9·Vesc que se tolera serían 1e-6·Vesc A del revés:
 * el diodo haría de pila de Vγ (o de Vz0) donde tiene que cortarse, en el
 * OP (Katzenelson) y al final de un paso con los tramos congelados. */
static int al_reves(const SimEl *e, int k, int tramo, const double *x)
{
    double di;
    if (e->tipo != SE_D || e->modelo != NL_TRAMOS || tramo == TR_CORTE) return 0;
    di = x[col_br[k]] - NL_GMIN * (tramo == TR_CONDUCE ? e->m[0] : -e->m[2]);
    if (tramo == TR_RUPTURA) di = -di;
    return di < -(1e-15 + 1e-9 * NL.Iesc);
}

/* El Newton. katz: los tramos pueden cambiar (paso de Katzenelson); lam: la
 * rampa de fuentes; gd: gmin en la diagonal de cada nodo. x entra (el
 * arranque, o el predictor) y sale. La matriz y el término, los de NC. */
static int newton(int katz, double lam, double gd, double *x, int itmax, NlEst *st, SimNewton *cnt)
{
    static double rhs[LU_MAX], xn[LU_MAX];
    static unsigned char flips[MAX_EL];
    static NlLin ant[MAX_EL];
    int it, i, q, pulido = 0, n = nincog;
    const Topo *tp = &topo_op;
    LU *f;
    memset(flips, 0, sizeof flips);
    nl_culpable = -1;
    if (NL.suave) evalua_suaves(st, x);
    for (it = 1; it <= itmax; it++) {
        int lim = 0, kq = -1, ok = 1;
        double th = 1.0, peor = -1.0;
        if (NC.modo != NLM_TR) {
            if (sim_ops() - nl_ops0 + (nl_it_op - nl_it0) * NL_OPS_IT > tope_op()) return NLR_TOPE;
            nl_it_op++;
        }
        /* 1. la matriz. Singular con un AO en su lineal: ese AO no puede estar
         *    en él (un comparador sin realimentación); va a la saturación del
         *    signo de su u en x (D7: con u = 0, como al empezar con x = 0, a
         *    V+) y Katzenelson lo lleva a la de su lado. Siempre a V+, dos AO
         *    anidados (uno con las entradas fijadas por el otro, saturado)
         *    podían ir y venir entre V+ y el lineal hasta NLR_CICLA */
        if (NC.modo == NLM_TR) diag_foto();
        f = matriz_nl(st, gd, &tp);
        if (!f) {
            int culp = -1;
            for (q = 0; q < NL.n; q++) {
                int k = NL.el[q];
                const SimEl *e = &R_.el[k];
                if (e->tipo != SE_AO || st->tramo[k] != TR_LIN || k == NC.fijo) continue;
                if (culp < 0 || col_br[k] == nl_col_mala || cphi(e->c) == nl_col_mala) culp = k;
            }
            if (culp < 0) {
                /* sin AO: el culpable (si no converge) es el suave de mayor G,
                 * una unión sin R sobre una E ideal o la salida de un AO cuya G
                 * pasa de 1e20 S y deja el pivote de la fuente bajo PIVOTE_NL */
                double gm = 0.0;
                for (q = 0; q < NL.n; q++) {
                    int k = NL.el[q], p, s;
                    const SimEl *e = &R_.el[k];
                    if (!es_suave(e)) continue;
                    for (p = 0; p < npuertos(e); p++)
                        for (s = 0; s < npuertos(e); s++)
                            if (fabs(st->lin[k].g[p][s]) > gm) { gm = fabs(st->lin[k].g[p][s]); nl_culpable = k; }
                }
                return NLR_SINGULAR;
            }
            if (NC.modo == NLM_TR) diag_vuelve();
            st->tramo[culp] = (unsigned char)(ao_u_de(&R_.el[culp], x) < 0.0 ? TR_SATN : TR_SATP);
            if (++flips[culp] > 8) { nl_culpable = culp; return NLR_CICLA; }
            continue;
        }
        /* 2. el término y la sustitución */
        termino(rhs, tp, NC.metodo, NC.t, NC.izq, NC.hh, st->tramo, lam);
        for (q = 0; q < NL.n; q++)
            if (es_suave(&R_.el[NL.el[q]])) estampa_suave(0, rhs, tp, &R_.el[NL.el[q]], &st->lin[NL.el[q]]);
        lu_resuelve(f, rhs, xn);
        if (NL.dos && f == &lu_nl) refina_nl(rhs, xn);     /* con un BJT: fila a fila */
        /* 3. Katzenelson: se avanza hasta el primer cambio de tramo y se
         *    cambia ese solo. En corte el margen es el menor de dos rectas: se
         *    interpola con la del lado por el que sale. */
        if (katz && NL.tramos) {
            int regla = 0;
            /* 3.a: un AO en su lineal que no vale (la regla): sale sin avanzar */
            for (q = 0; q < NL.n && kq < 0; q++) {
                int k = NL.el[q];
                if (R_.el[k].tipo == SE_AO && st->tramo[k] == TR_LIN && k != NC.fijo && !lin_vale(st, k)) {
                    kq = k; th = 0.0; regla = 1;
                }
            }
            /* 3.b: el primer tramo que se cruza */
            for (q = 0; q < NL.n && !regla; q++) {
                int k = NL.el[q], tau, s = -1;
                const SimEl *e = &R_.el[k];
                double m0, m1, tq;
                if (!es_tramos(e) || k == NC.fijo) continue;
                tau = st->tramo[k];
                m1 = margen_tr(e, tau, xn, -1);
                if (m1 >= -1e-9 * NL.Vesc && (m1 >= 0.0 || !al_reves(e, k, tau, xn))) continue;
                if (e->tipo == SE_AO && tau == TR_LIN) s = ao_satura(e, xn);
                else if (e->tipo == SE_D && tau == TR_CORTE) {
                    double v1[2];
                    puertos_v(e, xn, v1);
                    s = nl_tr_vecino(e, TR_CORTE, v1[0]);
                }
                m1 = margen_tr(e, tau, xn, s);
                m0 = margen_tr(e, tau, x, s);
                tq = m0 > 0.0 ? m0 / (m0 - m1) : 0.0;
                if (kq < 0 || tq < th) { kq = k; th = tq; }
            }
            if (kq >= 0) {
                const SimEl *e = &R_.el[kq];
                for (i = 0; i < n; i++) x[i] += th * (xn[i] - x[i]);
                if (e->tipo == SE_AO) {
                    int tau = st->tramo[kq];
                    if (tau == TR_LIN) st->tramo[kq] = (unsigned char)ao_satura(e, x);
                    else st->tramo[kq] = (unsigned char)(lin_vale(st, kq) ? TR_LIN : tau == TR_SATP ? TR_SATN : TR_SATP);
                    if (regla && NC.arranque0) avisa(SIM_AV_ARRANQUE, kq);
                } else {
                    double v[2];
                    puertos_v(e, x, v);
                    st->tramo[kq] = (unsigned char)nl_tr_vecino(e, st->tramo[kq], v[0]);
                }
                ops_propias += (unsigned long)(2 * n);
                if (cnt) cnt->iter++;
                if (++flips[kq] > 8) { nl_culpable = kq; return NLR_CICLA; }
                if (NL.suave) evalua_suaves(st, x);
                continue;
            }
        }
        /* 4. los suaves en xn, con límites: la linealización de la siguiente */
        if (NL.suave) {
            for (q = 0; q < NL.n; q++) ant[NL.el[q]] = st->lin[NL.el[q]];
            lim = evalua_suaves(st, xn);
            for (q = 0; q < NL.n; q++) {                 /* el culpable, si no converge */
                int k = NL.el[q];
                double dv, rel;
                if (!es_suave(&R_.el[k])) continue;
                dv = fabs(st->lin[k].v[0] - ant[k].v[0]) + fabs(st->lin[k].v[1] - ant[k].v[1]);
                rel = dv / (1e-6 * (fabs(st->lin[k].v[0]) + fabs(st->lin[k].v[1])) + 1e-9 * NL.Vesc);
                if (rel > peor) { peor = rel; nl_culpable = k; }
            }
            /* 5. convergencia: sin límites, y los incrementos y las leyes, o el residuo */
            ok = !lim && ((incrementos(x, xn) && leyes(st, ant)) || residuo_de(xn, tp, st, NC.t, NC.izq, lam) <= 1e-12);
        }
        memcpy(x, xn, (size_t)n * sizeof x[0]);
        if (cnt) cnt->iter++;
        if (ok) {
            double r;
            if (NL.suave && !pulido && NC.modo != NLM_TR) { pulido = 1; continue; }   /* una más */
            r = residuo_de(x, tp, st, NC.t, NC.izq, lam);
            /* En el paso no hay pulido: los incrementos (1e-6 de un potencial de
             * voltios) dejan a una unión que conduce un error de su vd de hasta
             * 1e-4·n·VT y un residuo de ~1e-9 (la media onda del modelo 25, con
             * 2e-9). Solo entonces, una más: los demás pasos no cambian. */
            if (NL.suave && !pulido && NC.modo == NLM_TR && r > 1e-10 && NL.prueba != NLP_SIN_PULIDO) { pulido = 1; continue; }
            /* Con gd > 0 (los pasos de gmin_escalonado) x es la solución de
             * otro circuito, con gd en cada nodo, que residuo_de no cuenta: su
             * residuo (0,1 a 1) no es el de nada que se enseñe. Cuenta el del
             * último paso, con gd = 0 */
            if (gd > 0.0) r = 0.0;
            if (cnt && r > cnt->residuo_max) cnt->residuo_max = r;
            if (cnt && it > cnt->iter_max) cnt->iter_max = it;
            if (r > residuo_barr) residuo_barr = r;
            if (r > residuo_acum) {                  /* el del mono no pasa del del motor */
                double rm = residuo_mono(x, tp, st, NC.t, NC.izq, lam);
                if (rm > residuo_acum) residuo_acum = rm;
            }
            return NLR_OK;
        }
    }
    return NLR_NOCONV;
}

/* gmin en la diagonal de cada nodo, de 1e-2 S a 0, con factor que se ablanda
 * al fallar (§3.E) */
static int gmin_escalonado(double *x, NlEst *st, SimNewton *cnt)
{
    static double x_ok[LU_MAX];
    static NlEst st_ok;
    double g = 1e-2, fac = 10.0, g_ok = -1.0;
    int r;
    memcpy(x_ok, x, sizeof x_ok);
    st_ok = *st;
    for (;;) {
        r = newton(1, 1.0, g, x, 50, st, cnt);
        if (r == NLR_TOPE) return r;
        if (r == NLR_OK) {
            memcpy(x_ok, x, sizeof x_ok); st_ok = *st; g_ok = g;
            if (g == 0.0) return NLR_OK;
            g = g / fac < 1e-12 ? 0.0 : g / fac;
            fac = 2.0 * fac < 10.0 ? 2.0 * fac : 10.0;
        } else {
            memcpy(x, x_ok, sizeof x_ok); *st = st_ok;
            if (g_ok < 0.0) {                            /* ni la primera: más gmin */
                if (g < 0.1) g = 0.1; else if (g < 1.0) g = 1.0; else return r;
                continue;
            }
            fac = sqrt(fac);
            if (fac < 1.001) return r;
            g = g_ok / fac;
        }
    }
}

/* Las fuentes (E y J; no las ganancias ni Vγ, Vz o V±) suben de 0 a 1 con
 * paso adaptativo (§3.E) */
static int rampa_fuentes(double *x, NlEst *st, SimNewton *cnt)
{
    static double x_ok[LU_MAX];
    static NlEst st_ok;
    double lam = 0.0, dl = 0.1;
    int r;
    memset(x, 0, sizeof x_ok);
    memset(x_ok, 0, sizeof x_ok);
    nl_prepara(st);
    st_ok = *st;
    while (lam < 1.0) {
        double l2 = lam + dl < 1.0 ? lam + dl : 1.0;
        r = newton(1, l2, 0.0, x, 50, st, cnt);
        if (r == NLR_TOPE) return r;
        if (r == NLR_OK) {
            lam = l2; memcpy(x_ok, x, sizeof x_ok); st_ok = *st;
            dl = 1.5 * dl < 0.5 ? 1.5 * dl : 0.5;
        } else {
            memcpy(x, x_ok, sizeof x_ok); *st = st_ok;
            dl /= 4.0;
            if (dl < 1e-4) return r;
        }
    }
    return NLR_OK;
}

/* El punto de operación con no lineales (§3.E): Newton, gmin, rampa */
static int op_nl(SimDiag *d, NlEst *st, SimNewton *cnt)
{
    int r, itmax, q, nt = 0;
    unsigned char homot = 0;
    long it0 = cnt->iter;
    for (q = 0; q < NL.n; q++) if (!es_suave(&R_.el[NL.el[q]])) nt++;
    itmax = NL.it_op > 0 ? NL.it_op : NL.suave ? 100 : 4 * nt + 20;
    if (!op_tope_fijo) tope_empieza();
    op_lista = 0;
    memset(x_op, 0, sizeof x_op);
    NC.modo = NLM_OP; NC.metodo = 0; NC.t = 0.0; NC.izq = 1; NC.hh = 1.0; NC.suelta = 0; NC.mask = 0;
    NC.fijo = -1; NC.arranque0 = 0;
    r = newton(1, 1.0, 0.0, x_op, itmax, st, cnt);
    if (r != NLR_OK && r != NLR_TOPE && NL.suave &&
        NL.prueba != NLP_SIN_GMIN && NL.prueba != NLP_SIN_HOMOTOPIA) {
        r = gmin_escalonado(x_op, st, cnt);
        if (r == NLR_OK) homot = 1;
    }
    if (r != NLR_OK && r != NLR_TOPE && NL.prueba != NLP_SIN_HOMOTOPIA) {
        r = rampa_fuentes(x_op, st, cnt);
        if (r == NLR_OK) homot = 2;
    }
    cnt->iter_op = (int)(cnt->iter - it0);
    cnt->homotopia = homot;
    /* Si no converge, SIM_NO_CONVERGE con el culpable (§3.E), también con
     * una LU singular: lo singular del circuito ya lo dijo analiza, y esta
     * es la de una unión sin R sobre una E ideal o la salida de un AO, cuya
     * G pasa de 1e20 S y deja el pivote de la rama de la fuente bajo
     * PIVOTE_NL (el culpable, el suave de mayor G) */
    if (r != NLR_OK) {
        d->estado = SIM_NO_CONVERGE;
        d->elem = nl_culpable >= 0 ? nl_culpable : NL.el[0];
        d->t = -1.0;
        d->campo = r == NLR_TOPE ? NC_TOPE : NC_OP;
        return d->estado;
    }
    /* §3.F: cada AO saturado cuyo lineal no vale se prueba en la otra
     * saturación; si también cuadra, BIESTABLE (se queda la primera) */
    for (q = 0; q < NL.n; q++) {
        static double xb[LU_MAX];
        static NlEst stb;
        SimNewton cb;
        int kk = NL.el[q], otra, r2;
        if (R_.el[kk].tipo != SE_AO || st->tramo[kk] == TR_LIN) continue;
        avisa(SIM_AV_SATURA, kk);
        if (lin_vale(st, kk)) continue;
        otra = st->tramo[kk] == TR_SATP ? TR_SATN : TR_SATP;
        memcpy(xb, x_op, sizeof xb);
        stb = *st;
        stb.tramo[kk] = (unsigned char)otra;
        memset(&cb, 0, sizeof cb);
        NC.fijo = kk;
        r2 = newton(1, 1.0, 0.0, xb, itmax, &stb, &cb);
        NC.fijo = -1;
        if (r2 == NLR_OK && stb.tramo[kk] == otra && ao_margen(&R_.el[kk], otra, xb) >= -1e-9 * NL.Vesc)
            avisa(SIM_AV_BIESTABLE, kk);
    }
    op_lista = 0;                                        /* lu_op puede ser de la prueba */
    if (homot) avisa(SIM_AV_HOMOTOPIA, -1);
    d->avisos = avisos;
    d->elem_aviso = elem_aviso;
    d->estado = SIM_OK;
    return SIM_OK;
}

/* R_, el mapa y el análisis estructural del modo 0 con sus rescates
 * (topo_op): lo que el OP hace antes de resolver. SIM_OK o el error. */
static int op_estructura(const SimRed *s, SimDiag *d)
{
    int k, vueltas, e;

    memset(d, 0, sizeof *d);
    d->elem = d->nodo = d->elem_aviso = -1;
    d->t = -1.0;
    if (s != &R_) { R_ = *s; red_nueva(); }
    if (R_.nel == 0) { d->estado = SIM_VACIO; return d->estado; }
    if (!valida(d, 0)) return d->estado;
    mapa();
    d->n = nincog;
    if (nincog > LU_MAX) { d->estado = SIM_GRANDE; return d->estado; }
    avisos = 0;
    elem_aviso = -1;
    for (k = 0; k < R_.nel; k++) { ci_ef[k] = R_.el[k].ci; ci_val[k] = R_.el[k].x0; }

    for (vueltas = 0; ; vueltas++) {
        e = analiza(&topo_op, 0, mask_de(0.0, 1), 0.0, d);
        if (e != -1) break;
        if (vueltas > 2 * R_.nel + 2) { d->estado = SIM_SINGULAR; return d->estado; }
    }
    d->avisos = avisos;
    d->elem_aviso = elem_aviso;
    return e;
}

/* Prepara R_ y resuelve el punto de operación en x_op. st y cnt: el estado de
 * Newton y los contadores de quien llama (NULL: los de sim_op_red). */
/* El OP de una red lineal ya analizada (op_estructura) y preparada
 * (nl_prepara): la matriz del modo 0, su LU y la solución en x_op. En
 * op_lin_ens_ y op_lin_lu_, las operaciones que contó el ensamblado y las de
 * la factorización (el barrido lineal las repone al reutilizarla) */
static unsigned long op_lin_ens_, op_lin_lu_;
static int op_lineal(SimDiag *d)
{
    static double rhs[LU_MAX];
    unsigned long o0 = ops_propias, l0;
    int k;
    ensambla(&lu_op, &topo_op, 0, 1.0, 0);
    lu_op_gen_++;
    op_lin_ens_ = ops_propias - o0;
    op_lista = 0;
    l0 = lu_ops();
    if (!lu_factoriza(&lu_op)) {
        int col = lu_op.col_mala;
        d->estado = SIM_SINGULAR;
        for (k = 0; k < R_.nel; k++) if (col_br[k] == col) d->elem = k;
        for (k = 0; k < R_.nn; k++) if (col_phi[k] == col) d->nodo = k;
        return d->estado;
    }
    op_lin_lu_ = lu_ops() - l0;
    termino(rhs, &topo_op, 0, 0.0, 1, 1.0, 0, 1.0);
    lu_resuelve(&lu_op, rhs, x_op);
    d->estado = SIM_OK;
    return SIM_OK;
}

static int op_interno(const SimRed *s, SimDiag *d, NlEst *st, SimNewton *cnt)
{
    int e;

    e = op_estructura(s, d);
    if (e != SIM_OK) return e;

    if (!st) { st = &st_op; memset(&nw_op, 0, sizeof nw_op); }
    if (!cnt) cnt = &nw_op;
    nl_act = st;
    nl_prepara(st);
    if (NL.n > 0) return op_nl(d, st, cnt);
    return op_lineal(d);
}

int sim_op_red(const SimRed *s, Valores *v, SimDiag *d)
{
    memset(v, 0, sizeof *v);
    if (op_interno(s, d, 0, 0) != SIM_OK) return d->estado;
    valores_de(x_op, v, 0.0, 1);
    return SIM_OK;
}

int sim_punto_operacion(const Circuito *c, const Red *r, Valores *v, SimDiag *d)
{
    static SimRed s;
    memset(v, 0, sizeof *v);
    if (sim_red_de(c, r, &s, d) != SIM_OK) return d->estado;
    return sim_op_red(&s, v, d);
}

/* --------------------------------------------------------------------------
 *  Para la alterna (ca.c): la topología del transitorio y el Thévenin de CC.
 *  Usan R_, el mapa y el punto de operación: guardan y reponen lo de la
 *  última emulación (la pantalla del emulador sigue leyendo sim_red()).
 * ----------------------------------------------------------------------- */
static struct {
    SimRed r;
    int col_phi[MAX_NODOS], col_br[MAX_EL], nincog;
    unsigned int avisos;
    int elem_aviso;
    unsigned char ci_ef[MAX_EL];
    double ci_val[MAX_EL], x_op[LU_MAX];
    Topo topo_op;
    int npot;
    NlEst *nl_act;
} guarda_;
static struct { int n; short el[MAX_EL]; unsigned char suave, tramos, dos; double Vesc, Iesc; } guarda_nl_;

/* De una topología, lo que se lee de ella para una red de nel elementos y nn
 * nodos: los papeles y las sustituciones, los trozos, las filas del flujo de
 * las L con SU_FLUJO y el potencial de los nodos con SU_ANCLA (lo demás no
 * lo lee nadie: ensambla, termino y fila_libre miran estos) */
static void topo_copia(Topo *a, const Topo *b, int nel, int nn)
{
    int k;
    memcpy(a->rol, b->rol, (size_t)nel); memcpy(a->sust_el, b->sust_el, (size_t)nel);
    memcpy(a->sust_nodo, b->sust_nodo, (size_t)nn); memcpy(a->pieza, b->pieza, (size_t)nn * sizeof b->pieza[0]);
    for (k = 0; k < nel; k++) if (b->sust_el[k] == SU_FLUJO) memcpy(a->flujo_s[k], b->flujo_s[k], (size_t)nel);
    for (k = 0; k < nn; k++) if (b->sust_nodo[k] == SU_ANCLA) a->ancla[k] = b->ancla[k];
}

/* Las tablas de la red, en lo que se usa: nn nodos, nel elementos (con los
 * de R_ en el momento; R_ va entera, porque sim_red() la enseña) */
static void guarda_estado(void)
{
    int nn = R_.nn, nel = R_.nel;
    guarda_.r = R_;
    memcpy(guarda_.col_phi, col_phi, (size_t)nn * sizeof col_phi[0]);
    memcpy(guarda_.col_br, col_br, (size_t)nel * sizeof col_br[0]);
    guarda_.nincog = nincog;
    guarda_.avisos = avisos; guarda_.elem_aviso = elem_aviso;
    memcpy(guarda_.ci_ef, ci_ef, (size_t)nel);
    memcpy(guarda_.ci_val, ci_val, (size_t)nel * sizeof ci_val[0]);
    memcpy(guarda_.x_op, x_op, sizeof x_op);
    topo_copia(&guarda_.topo_op, &topo_op, nel, nn);
    guarda_.npot = npot;
    guarda_.nl_act = nl_act;
    guarda_nl_.n = NL.n; memcpy(guarda_nl_.el, NL.el, (size_t)NL.n * sizeof NL.el[0]);
    guarda_nl_.suave = NL.suave; guarda_nl_.tramos = NL.tramos; guarda_nl_.dos = NL.dos;
    guarda_nl_.Vesc = NL.Vesc; guarda_nl_.Iesc = NL.Iesc;
}

static void repone_estado(void)
{
    int nn, nel;
    R_ = guarda_.r;
    red_nueva();
    nn = R_.nn; nel = R_.nel;
    memcpy(col_phi, guarda_.col_phi, (size_t)nn * sizeof col_phi[0]);
    memcpy(col_br, guarda_.col_br, (size_t)nel * sizeof col_br[0]);
    nincog = guarda_.nincog;
    avisos = guarda_.avisos; elem_aviso = guarda_.elem_aviso;
    memcpy(ci_ef, guarda_.ci_ef, (size_t)nel);
    memcpy(ci_val, guarda_.ci_val, (size_t)nel * sizeof ci_val[0]);
    memcpy(x_op, guarda_.x_op, sizeof x_op);
    topo_copia(&topo_op, &guarda_.topo_op, nel, nn);
    npot = guarda_.npot;
    nl_act = guarda_.nl_act;
    NL.n = guarda_nl_.n; memcpy(NL.el, guarda_nl_.el, (size_t)NL.n * sizeof NL.el[0]);
    NL.suave = guarda_nl_.suave; NL.tramos = guarda_nl_.tramos; NL.dos = guarda_nl_.dos;
    NL.Vesc = guarda_nl_.Vesc; NL.Iesc = guarda_nl_.Iesc;
    op_lista = 0;                           /* lu_op es de otra red */
}

/* ---- El punto de operación de CC en la vista de la calculadora (§3.E) ---- */
int sim_op_cc_red(const SimRed *s, Valores *v, SimDiag *d, SimNewton *nw)
{
    static SimRed s2;
    static NlEst st;
    static SimNewton c;
    int k, e;
    memset(v, 0, sizeof *v);
    memset(&c, 0, sizeof c);
    s2 = *s;
    for (k = 0; k < s2.nel; k++) {
        SimEl *el = &s2.el[k];
        if (el->tipo == SE_C || el->tipo == SE_L) { el->ci = 0; el->x0 = 0.0; }
        if (el->tipo == SE_S) el->t1 = el->t2 = -1.0;            /* su estado al empezar */
        if (el->tipo == SE_E || el->tipo == SE_J) {              /* su nivel de CC */
            double niv = onda_nivel_cc(el->forma, el->p);
            memset(el->p, 0, sizeof el->p);
            el->p[0] = niv;
            el->forma = ONDA_CC;
        }
    }
    guarda_estado();
    e = op_interno(&s2, d, &st, &c);
    if (e == SIM_OK) {
        /* el residuo de la solución que se da (una red lineal no pasa por
         * Newton y no lo tendría) */
        double r = residuo_de(x_op, &topo_op, &st, 0.0, 1, 1.0);
        if (r > c.residuo_max) c.residuo_max = r;
        valores_de(x_op, v, 0.0, 1);
    }
    repone_estado();
    if (nw) *nw = c;
    return e;
}

/* El punto Q (§3.I): las fuentes en su nivel de CC (una alterna fasor ya es
 * una senoidal de nivel 0), C y L sin V0 ni I0 y los S en su estado final (el
 * de la CA); el OP con homotopías; la linealización en Q. No toca res_, M ni
 * la caché del transitorio. */
int sim_punto_q(const SimRed *s, SimLin *L, SimDiag *d, SimNewton *nw)
{
    static SimRed s2;
    static NlEst st;
    static SimNewton c;
    int k, e;
    memset(L, 0, sizeof *L);
    memset(&c, 0, sizeof c);
    s2 = *s;
    for (k = 0; k < s2.nel; k++) {
        SimEl *el = &s2.el[k];
        if (el->tipo == SE_C || el->tipo == SE_L) { el->ci = 0; el->x0 = 0.0; }
        if (el->tipo == SE_S) { el->cerrado0 = (unsigned char)(el->cerrado0 ^ (el->t1 >= 0.0) ^ (el->t2 >= 0.0)); el->t1 = el->t2 = -1.0; }
        if (el->tipo == SE_E || el->tipo == SE_J) {
            double niv = onda_nivel_cc(el->forma, el->p);
            memset(el->p, 0, sizeof el->p);
            el->p[0] = niv;
            el->forma = ONDA_CC;
        }
    }
    guarda_estado();
    e = op_interno(&s2, d, &st, &c);
    if (e == SIM_OK) {
        valores_de(x_op, &L->q, 0.0, 1);
        L->hay = NL.n > 0;
        for (k = 0; k < R_.nel; k++) {
            const SimEl *el = &R_.el[k];
            if (!es_nl(el->tipo)) continue;
            L->tramo[k] = st.tramo[k];
            if (es_suave(el)) memcpy(L->g[k], st.lin[k].g, sizeof L->g[k]);
            L->reg[k] = L->q.reg[k];
        }
    }
    repone_estado();
    if (nw) *nw = c;
    return e;
}

/* ---- El barrido de CC (§3.J) ---- */
/* La vista de CC de s (la de sim_op_cc_red) con el parámetro del elemento k en v */
static void vista_cc(SimRed *s2, const SimRed *s, int k, int bx, double v)
{
    int i;
    *s2 = *s;
    for (i = 0; i < s2->nel; i++) {
        SimEl *el = &s2->el[i];
        if (el->tipo == SE_C || el->tipo == SE_L) { el->ci = 0; el->x0 = 0.0; }
        if (el->tipo == SE_S) el->t1 = el->t2 = -1.0;
        if (el->tipo == SE_E || el->tipo == SE_J) {
            double niv = onda_nivel_cc(el->forma, el->p);
            memset(el->p, 0, sizeof el->p);
            el->p[0] = niv;
            el->forma = ONDA_CC;
        }
    }
    if (k >= 0 && k < s2->nel) {
        SimEl *e = &s2->el[k];
        if (bx == BX_E || bx == BX_J) e->p[0] = v;
        else e->val = v;                                 /* una R, o la ganancia de una controlada */
    }
}

/* ---- El barrido de una red lineal que barre una E o una J (§3.J) ----
 * La matriz no depende del valor de la fuente: con el mismo análisis, la
 * misma red da la misma LU. Tras un punto completo (con continuación: su
 * op_estructura y el op_interno que la repite, con la LU), en los siguientes
 * de la misma red (la de quien llama, comparada entera, el mismo elemento y
 * el mismo tipo) y con lu_op como quedó (nadie la volvió a armar:
 * lu_op_gen_):
 *  - si aquel análisis no pidió ninguna fuente (solo lo hacen un lazo de
 *    ramas V y un trozo que flota: v_impuesta, i_impuesta), no depende del
 *    valor barrido: el camino corto pone el valor en R_, repone lo del
 *    análisis y sustituye (bl_corto);
 *  - si no, se analiza (el valor puede cambiar la topología: una E en
 *    paralelo con una L que en 0 cuadra) y, si todo sale igual que en el
 *    punto guardado, se sustituye sin repetir el análisis ni la LU
 *    (bl_medio).
 * Los dos suman las operaciones de lo que no se rehace (el análisis de
 * op_interno, el ensamblado y la LU): mismos bits y mismas operaciones que
 * el camino completo */
static struct {
    int vale, k, bx, sin_fuentes;
    unsigned lu_gen;
    SimRed base, vista;                  /* la red de quien llama y su vista de CC */
    Topo topo;
    int col_phi[MAX_NODOS], col_br[MAX_EL], nincog, npot;
    unsigned char ci_ef[MAX_EL];
    double ci_val[MAX_EL];
    unsigned int avisos;
    int elem_aviso;
    SimDiag d;
    unsigned long ops_ens, ops_lu;
} bl_;

static int bl_sirve(const SimRed *s, int k, int bx)
{
    return bl_.vale && bl_.k == k && bl_.bx == bx && bl_.lu_gen == lu_op_gen_ && s->nn == bl_.base.nn &&
           s->nel == bl_.base.nel && memcmp(s->ref, bl_.base.ref, (size_t)s->nn) == 0 &&
           memcmp(s->el, bl_.base.el, (size_t)s->nel * sizeof s->el[0]) == 0;
}

/* ¿Dio el análisis de este punto (op_estructura) lo mismo que el guardado? */
static int bl_mismo_analisis(const SimDiag *d)
{
    return nincog == bl_.nincog && npot == bl_.npot && avisos == bl_.avisos && elem_aviso == bl_.elem_aviso &&
           memcmp(d, &bl_.d, sizeof *d) == 0 && memcmp(&topo_op, &bl_.topo, sizeof topo_op) == 0 &&
           memcmp(col_phi, bl_.col_phi, sizeof col_phi) == 0 && memcmp(col_br, bl_.col_br, sizeof col_br) == 0 &&
           memcmp(ci_ef, bl_.ci_ef, sizeof ci_ef) == 0 && memcmp(ci_val, bl_.ci_val, sizeof ci_val) == 0;
}

/* Tras un punto completo que valió (R_ y el análisis son los de su vista);
 * sin_fuentes: su análisis no pidió ninguna fuente */
static void bl_guarda(const SimRed *s, int k, int bx, const SimDiag *d, int sin_fuentes)
{
    bl_.vale = 1; bl_.k = k; bl_.bx = bx; bl_.lu_gen = lu_op_gen_; bl_.sin_fuentes = sin_fuentes;
    bl_.base = *s; bl_.vista = R_; bl_.topo = topo_op;
    memcpy(bl_.col_phi, col_phi, sizeof col_phi); memcpy(bl_.col_br, col_br, sizeof col_br);
    bl_.nincog = nincog; bl_.npot = npot;
    memcpy(bl_.ci_ef, ci_ef, sizeof ci_ef); memcpy(bl_.ci_val, ci_val, sizeof ci_val);
    bl_.avisos = avisos; bl_.elem_aviso = elem_aviso;
    bl_.d = *d;
    bl_.ops_ens = op_lin_ens_; bl_.ops_lu = op_lin_lu_;
}

static unsigned long bl_cortos_, bl_medios_;
unsigned long sim_cc_cortos(int medio) { return medio ? bl_medios_ : bl_cortos_; }

/* Con el análisis ya hecho (el de bl_, y el d de este punto): lo que haría
 * op_interno (que lo repetiría: ops_ana, las que contó) sin rehacer la LU */
static void bl_medio(unsigned long ops_ana, SimDiag *d)
{
    static double rhs[LU_MAX];
    bl_medios_++;
    ops_propias += ops_ana + bl_.ops_ens;
    lu_ops_suma(bl_.ops_lu);
    op_lista = 0;
    termino(rhs, &topo_op, 0, 0.0, 1, 1.0, 0, 1.0);
    lu_resuelve(&lu_op, rhs, x_op);
    d->estado = SIM_OK;
}

/* El punto de valor v por el camino corto: sin análisis (lo que dejaría
 * op_estructura, repuesto) y sin LU */
static void bl_corto(int k, double v, SimDiag *d, NlEst *st)
{
    static double rhs[LU_MAX];
    bl_cortos_++;
    R_ = bl_.vista;
    R_.el[k].p[0] = v;
    red_nueva();
    memcpy(col_phi, bl_.col_phi, sizeof col_phi); memcpy(col_br, bl_.col_br, sizeof col_br);
    nincog = bl_.nincog; npot = bl_.npot;
    memcpy(ci_ef, bl_.ci_ef, sizeof ci_ef); memcpy(ci_val, bl_.ci_val, sizeof ci_val);
    avisos = bl_.avisos; elem_aviso = bl_.elem_aviso;
    topo_op = bl_.topo;
    *d = bl_.d;
    nl_act = st;
    nl_prepara(st);                               /* NL.n = 0; Vesc e Iesc con este valor */
    ops_propias += bl_.ops_ens;
    lu_ops_suma(bl_.ops_lu);
    op_lista = 0;
    termino(rhs, &topo_op, 0, 0.0, 1, 1.0, 0, 1.0);
    lu_resuelve(&lu_op, rhs, x_op);
}

int sim_cc_punto(const SimRed *s, int k, int bx, double v, SimCc *io, SimDiag *d)
{
    static SimRed s2;
    static NlEst st;
    static SimNewton c;
    int e, q, ex = io->hay && (bx == BX_E || bx == BX_J) && k >= 0 && k < s->nel, sirve = ex && bl_sirve(s, k, bx);
    int corto = sirve && bl_.sin_fuentes;
    if (!corto) vista_cc(&s2, s, k, bx, v);
    guarda_estado();
    memset(&c, 0, sizeof c);
    if (NL.prueba == NLP_FALLA_BARR && v == NL.t_prueba) {            /* el gancho del arnés */
        memset(d, 0, sizeof *d);
        d->estado = SIM_NO_CONVERGE; d->campo = NC_BARR; d->t = v; d->elem = -1; d->nodo = k;   /* NC_BARR: nodo = lo barrido */
        repone_estado();
        return d->estado;
    }
    if (corto) { bl_corto(k, v, d, &st); e = SIM_OK; }
    else if (!io->hay) e = op_interno(&s2, d, &st, &c);               /* el primero: el OP completo (§E) */
    else {
        unsigned long f0 = fe_n_, o0 = ops_propias, ops_ana;
        e = op_estructura(&s2, d);
        ops_ana = ops_propias - o0;               /* las de las fuentes que pidió el análisis */
        if (e == SIM_OK) {
            nl_act = &st;
            nl_prepara(&st);
            if (NL.n == 0 && sirve && bl_mismo_analisis(d)) bl_medio(ops_ana, d);
            else if (NL.n == 0) {
                int sin_fuentes = fe_n_ == f0 && ops_ana == 0;
                e = op_interno(&s2, d, &st, &c);
                if (e == SIM_OK && ex) bl_guarda(s, k, bx, d, sin_fuentes);
            } else if (io->n != nincog) e = op_interno(&s2, d, &st, &c);
            else {
                int r;
                memcpy(x_op, io->x, sizeof x_op);
                memcpy(st.tramo, io->tramo, sizeof st.tramo);
                memcpy(st.vlim, io->vlim, sizeof st.vlim);
                st.frio = 0;
                NC.modo = NLM_BARR; NC.metodo = 0; NC.t = 0.0; NC.izq = 1; NC.hh = 1.0; NC.suelta = 0; NC.mask = 0;
                NC.fijo = -1; NC.arranque0 = 0;
                op_lista = 0;
                if (!op_tope_fijo) tope_empieza();
                r = newton(1, 1.0, 0.0, x_op, NL.it_op > 0 ? NL.it_op : 30, &st, &c);
                if (r == NLR_OK) { d->estado = SIM_OK; e = SIM_OK; }
                else {
                    d->estado = r == NLR_SINGULAR ? SIM_SINGULAR : SIM_NO_CONVERGE;
                    d->campo = r == NLR_TOPE ? NC_TOPE : NC_BARR;
                    d->t = v; d->elem = nl_culpable; d->nodo = k;
                    e = d->estado;
                }
            }
        }
    }
    if (e == SIM_OK) {
        io->salto = 0;
        for (q = 0; q < NL.n; q++) {                     /* un AO que pasa de una saturación a la otra */
            int kk = NL.el[q];
            if (R_.el[kk].tipo == SE_AO && io->hay && io->tramo[kk] != TR_LIN && st.tramo[kk] != TR_LIN &&
                io->tramo[kk] != st.tramo[kk]) io->salto = 1;
        }
        io->hay = 1;
        io->n = nincog;
        memcpy(io->x, x_op, sizeof io->x);
        memcpy(io->tramo, st.tramo, sizeof io->tramo);
        memcpy(io->vlim, st.vlim, sizeof io->vlim);
        io->iter = (int)c.iter;
        io->p = v;
        /* el de esta solución, solo si se pide (la tabla del cursor): en cada
         * punto del barrido costaría sus exp() y cambiaría su tiempo */
        io->residuo = cc_con_residuo ? residuo_de(x_op, &topo_op, &st, 0.0, 1, 1.0) : 0.0;
        valores_de(x_op, &io->v, 0.0, 1);
    }
    repone_estado();
    return e;
}

/* La parte conocida y los coeficientes del control de la controlada q, en x:
 * su valor (V o I) */
static double control_x(int q, const double *x, double lam)
{
    return ctl_valor(x, q, 0.0, 1, lam);
}

/* D = d(I_ctl)/dp de una controlada q por la I del elemento barrido k (§3.J (2)) */
static double d_ictl(int q, int k, int bx, const double *x)
{
    const SimEl *e = &R_.el[q], *x0;
    if (e->ctl != k || !e->ctl_i) return 0.0;
    x0 = &R_.el[k];
    if (x0->tipo == SE_R && bx == BX_R) return -(vnodo(x, x0->a) - vnodo(x, x0->b)) / (x0->val * x0->val);
    if (x0->tipo == SE_E && bx == BX_E && col_br[k] < 0) return 1.0 / x0->val;
    if (x0->tipo == SE_J && bx == BX_J) return 1.0;
    return 0.0;                                          /* E ideal, una controlada barrida: su rama */
}

int sim_cc_deriva(const SimRed *s, int k, int bx, const SimCc *io, const SimCanal *ch, double *dy)
{
    static SimRed s2;
    static NlEst st;
    static double r[LU_MAX], dx[LU_MAX];
    SimDiag d;
    const Topo *tp;
    const SimEl *e;
    LU *f;
    int i, q, n, ok = 0;
    *dy = 0.0;
    if (!io->hay) return 0;
    vista_cc(&s2, s, k, bx, io->p);
    guarda_estado();
    if (op_estructura(&s2, &d) != SIM_OK || io->n != nincog || k < 0 || k >= R_.nel) { repone_estado(); return 0; }
    nl_act = &st;
    nl_prepara(&st);
    memcpy(st.tramo, io->tramo, sizeof st.tramo);
    memcpy(st.vlim, io->vlim, sizeof st.vlim);
    st.frio = 0;
    if (NL.suave) evalua_suaves(&st, io->x);             /* la linealización en la solución */
    NC.modo = NLM_BARR; NC.metodo = 0; NC.t = 0.0; NC.izq = 1; NC.hh = 1.0; NC.suelta = 0; NC.mask = 0; NC.fijo = -1;
    op_lista = 0;
    f = matriz_nl(&st, 0.0, &tp);                        /* el jacobiano en la solución */
    if (f) {
        const double *x = io->x;
        e = &R_.el[k];
        for (i = 0; i < nincog; i++) r[i] = 0.0;
        /* (1) el elemento barrido: r = db/dp - (dA/dp)·x */
        switch (bx) {
        case BX_E:
            if (col_br[k] >= 0) r[col_br[k]] = 1.0;
            else inyecta(r, e->a, e->b, 1.0 / e->val);
            break;
        case BX_J:
            inyecta(r, e->a, e->b, 1.0);
            break;
        case BX_R: {
            double c0 = (vnodo(x, e->a) - vnodo(x, e->b)) / (e->val * e->val);
            int ca = cphi(e->a), cb = cphi(e->b);
            if (ca >= 0) r[ca] += c0;
            if (cb >= 0) r[cb] -= c0;
            break;
        }
        default: {                                       /* la ganancia de la controlada k */
            double ctl = control_x(k, x, 1.0);
            if (e->tipo == SE_EC) r[col_br[k]] += ctl;
            else inyecta(r, e->a, e->b, ctl);
            break;
        }
        }
        /* (2) las controladas por la I del elemento barrido */
        for (q = 0; q < R_.nel; q++) {
            const SimEl *eq = &R_.el[q];
            double D;
            if (eq->tipo != SE_EC && eq->tipo != SE_JC) continue;
            D = d_ictl(q, k, bx, x);
            if (D == 0.0) continue;
            if (eq->tipo == SE_EC) r[col_br[q]] += eq->val * D;
            else inyecta(r, eq->a, eq->b, eq->val * D);
        }
        for (n = 0; n < R_.nn; n++) { int c = col_phi[n]; if (c >= 0 && tp->sust_nodo[n] != SU_NO) r[c] = 0.0; }
        lu_resuelve(f, r, dx);
        /* el canal aplicado a dx, con lo que depende de p a la vista */
        if (ch->tipo == CANAL_NODO) *dy = vnodo(dx, ch->nodo);
        else if (ch->elem >= 0 && ch->elem < R_.nel) {
            const SimEl *c = &R_.el[ch->elem];
            double dva = vnodo(dx, c->a), dvb = vnodo(dx, c->b), di = 0.0;
            int j = col_br[ch->elem], mismo = ch->elem == k;
            if (ch->tipo == CANAL_V) *dy = ch->sg * (dva - dvb);
            else {
                switch (c->tipo) {
                case SE_R:
                    di = (dva - dvb) / c->val;
                    if (mismo && bx == BX_R) di -= (vnodo(x, c->a) - vnodo(x, c->b)) / (c->val * c->val);
                    break;
                case SE_E:
                    if (j >= 0) di = dx[j];
                    else di = ((mismo && bx == BX_E ? 1.0 : 0.0) + dva - dvb) / c->val;
                    break;
                case SE_J:  di = mismo && bx == BX_J ? 1.0 : 0.0; break;
                case SE_D:  di = j >= 0 ? dx[j] : st.lin[ch->elem].g[0][0] * (dva - dvb); break;
                case SE_BJT:                             /* dIC = g10·dVBE + g11·dVCE */
                case SE_MOS: {                           /* dID = gm·dVGS + gds·dVDS */
                    const NlLin *L = &st.lin[ch->elem];
                    di = L->g[1][0] * (vnodo(dx, c->c) - dvb) + L->g[1][1] * (dva - dvb);
                    break;
                }
                case SE_JC: {
                    int col[3], fu, t;
                    double cf[3], f0, dc = 0.0;
                    int nt = ctl_coef_en(&R_, col_phi, col_br, ch->elem, col, cf, &fu, &f0);
                    for (t = 0; t < nt; t++) if (col[t] >= 0) dc += cf[t] * dx[col[t]];
                    di = c->val * (dc + d_ictl(ch->elem, k, bx, x));
                    if (mismo && bx == BX_K) di += control_x(k, x, 1.0);
                    break;
                }
                case SE_AO:                              /* real con Ro: I = (u - Vsal)/Ro (valores_de) */
                    if (j >= 0) di = dx[j];
                    else di = ((st.tramo[ch->elem] == TR_LIN ? c->m[0] * (dva - dvb) : 0.0) -
                               (vnodo(dx, c->c) - vnodo(dx, c->nref))) / c->m[4];
                    break;
                default:    di = j >= 0 ? dx[j] : 0.0; break;
                }
                *dy = ch->sg * di;
            }
        }
        ops_propias += (unsigned long)(4 * nincog);
        ok = 1;
    }
    repone_estado();
    return ok;
}

int sim_punto_op_cc(const Circuito *c, const Red *r, Valores *v, SimDiag *d, SimNewton *nw)
{
    static SimRed s;
    memset(v, 0, sizeof *v);
    if (sim_red_de(c, r, &s, d) != SIM_OK) return d->estado;
    return sim_op_cc_red(&s, v, d, nw);
}

unsigned sim_mascara_final(const SimRed *s)
{
    unsigned m = 0;
    int k;
    for (k = 0; k < s->nel && k < 32; k++) {
        const SimEl *e = &s->el[k];
        int cerrado;
        if (e->tipo != SE_S) continue;
        cerrado = e->cerrado0 ^ (e->t1 >= 0.0) ^ (e->t2 >= 0.0);
        if (cerrado) m |= 1u << k;
    }
    return m;
}

int sim_topo_ca(const SimRed *s, SimTopoCA *t, SimDiag *d)
{
    static Topo tp;
    int e, n, k;
    memset(d, 0, sizeof *d);
    d->elem = d->nodo = d->elem_aviso = -1;
    d->t = -1.0;
    memset(t, 0, sizeof *t);
    guarda_estado();
    R_ = *s;
    red_nueva();
    if (R_.nel == 0) { d->estado = SIM_VACIO; repone_estado(); return d->estado; }
    if (!valida(d, 1)) { e = d->estado; repone_estado(); return e; }
    mapa();
    d->n = nincog;
    if (nincog > LU_MAX) { d->estado = SIM_GRANDE; repone_estado(); return d->estado; }
    avisos = 0;
    elem_aviso = -1;
    t->mask = sim_mascara_final(s);
    e = analiza(&tp, 1, t->mask, 0.0, d);
    if (e == SIM_OK) {
        t->n = nincog;
        for (n = 0; n < R_.nn; n++) {
            t->col_phi[n] = (signed char)col_phi[n];
            t->ancla[n] = (unsigned char)(tp.sust_nodo[n] == SU_ANCLA);
            t->pieza[n] = (short)tp.pieza[n];
        }
        for (k = 0; k < R_.nel; k++) t->col_br[k] = (signed char)col_br[k];
        d->estado = SIM_OK;
        d->avisos |= avisos;
        d->elem_aviso = elem_aviso;
    }
    repone_estado();
    return e;
}

int sim_thevenin_cc(const SimRed *s, int na, int nb, double *vth, double *rth, int *inf, SimDiag *d)
{
    static SimRed s2;
    static double rhs[LU_MAX], x[LU_MAX];
    int k, n, e;
    *vth = *rth = 0.0;
    *inf = 0;
    if ((k = primer_nl(s)) >= 0) {                  /* Thévenin es de redes lineales (D22) */
        memset(d, 0, sizeof *d);
        d->nodo = d->elem_aviso = -1; d->t = -1.0;
        d->estado = SIM_NO_LINEAL; d->elem = k;
        return d->estado;
    }
    guarda_estado();
    s2 = *s;
    for (k = 0; k < s2.nel; k++) {
        SimEl *el = &s2.el[k];
        if (el->tipo == SE_C || el->tipo == SE_L) { el->ci = 0; el->x0 = 0.0; }
        if (el->tipo == SE_S) el->t1 = el->t2 = -1.0;             /* la vista de la calculadora */
    }
    e = op_interno(&s2, d, 0, 0);
    if (e != SIM_OK) { repone_estado(); return e; }
    if (d->avisos & (SIM_AV_C_DESC | SIM_AV_L_SIN_I | SIM_AV_CI_SALTA)) {
        d->estado = SIM_SIN_REGIMEN; repone_estado(); return d->estado;
    }
    if (na < 0 || nb < 0 || na >= R_.nn || nb >= R_.nn) { d->estado = SIM_SIN_AB; repone_estado(); return d->estado; }
    *vth = (col_phi[na] >= 0 ? x_op[col_phi[na]] : 0.0) - (col_phi[nb] >= 0 ? x_op[col_phi[nb]] : 0.0);
    if (topo_op.pieza[na] != topo_op.pieza[nb]) { *inf = 1; repone_estado(); return SIM_OK; }
    /* 1 A que entra por A y sale por B, con las fuentes apagadas: la matriz
     * del punto de operación no depende de las fuentes */
    for (k = 0; k < nincog; k++) rhs[k] = 0.0;
    inyecta(rhs, nb, na, 1.0);
    for (n = 0; n < R_.nn; n++)
        if (col_phi[n] >= 0 && topo_op.sust_nodo[n] != SU_NO) rhs[col_phi[n]] = 0.0;
    lu_resuelve(&lu_op, rhs, x);
    *rth = (col_phi[na] >= 0 ? x[col_phi[na]] : 0.0) - (col_phi[nb] >= 0 ? x[col_phi[nb]] : 0.0);
    ops_propias += 10;
    repone_estado();
    return SIM_OK;
}

/* ==========================================================================
 *  5. Transitorio
 * ======================================================================= */
/* Caché de factorizaciones del trapecio: (máscara, tramos, paso). Sin no
 * lineales se usan 4 entradas, como siempre (la misma expulsión, bit a bit);
 * con ellos, 8 (el arranque del puente: 3 combinaciones de tramos × 2 pasos).
 * Con suaves cada entrada guarda la matriz sin factorizar (lu_base): el Newton
 * la copia a lu_nl y le suma los compañeros. */
#define N_CACHE 8
static LU lu_cache[N_CACHE];
static Topo topo_cache[N_CACHE];
static unsigned cache_mask[N_CACHE];
static double cache_hh[N_CACHE];
static SimTramos cache_tr[N_CACHE];
static int cache_uso[N_CACHE], cache_reloj;
static double lu_base[N_CACHE][LU_MAX][LU_MAX];
static LU lu_suelta;                       /* pasos partidos y modos */
static Topo topo_suelta;
static double lu_base_suelta[LU_MAX][LU_MAX];
static int suelta_ok;                      /* lu_suelta vale para (suelta_mask, suelta_tr, suelta_hh) */
static unsigned suelta_mask;
static double suelta_hh;
static SimTramos suelta_tr;

static SimAjustes aj_;
static SimRes res_;
static SimReg reg_;
static unsigned long fuera_;
static long tope_pasos_;
static unsigned gen_;
static void (*al_terminar_)(void);          /* el arnés: la huella de cada emulación */

/* Integrales de la ventana de medida [t_med0, t_fin], por elemento */
static double iv_V[MAX_EL], iv_V2[MAX_EL], iv_I[MAX_EL], iv_I2[MAX_EL], iv_P[MAX_EL];

/* Un punto (instante, V e I de cada elemento, el segundo puerto del AO, los
 * dos canales) */
typedef struct { double t, V[MAX_EL], I[MAX_EL], V2[MAX_EL], I2[MAX_EL], y[2]; } Punto;

/* Estado de la marcha */
static struct {
    int     activa, hecho, estado;
    int     disc0;                 /* t = 0 es una discontinuidad         */
    double  t, t_ancla, t_ev, h, t_fin;
    long    k;                     /* pasos del trapecio desde el ancla   */
    int     arranque;              /* medios pasos de Euler que quedan    */
    int     pend;                  /* falta la y+ del último evento       */
    int     en_arr;                /* el arranque está en curso           */
    int     ic_arr;                /* la caché del arranque (Euler, h/8; en el tramo rígido, SDIRK2) */
    int     rig_h;                 /* pasos h del tramo rígido tras cada evento (0: no hay) */
    int     rig_nh;                /* los de este evento (con no lineales, los de su clase) */
    int     rig_n, rig;            /* sus subpasos, y los que quedan      */
    int     ic_rig2;               /* la caché de sus subpasos de h/RIG_SUB2 */
    int     ymas;                  /* su y+: 1 = ya salió (la consistente); -1 = no se pudo */
    double  tau_y;                 /* la τ de su ε (RIG_EPS·τ) y de su modo más rápido */
    double  rig_amp, rig_fac;      /* con no lineales: lo que queda de ese modo; lo que lo apaga el subpaso */
    int     sonda_red;             /* la red tiene clases de evento con su presonda (§17.3) */
    int     ev_tramo;              /* el evento en curso es un cambio de tramo */
    double  rx[2][MAX_EL];         /* v de C, i de L en el último te + k·h y te + (k + ½)·h del tramo */

    int     impulso;               /* el último evento tuvo un impulso    */
    int     quietos;               /* pasos seguidos sin avanzar el tiempo */
    long    tope;                  /* pasos como mucho                    */
    double  t_evento;              /* instante del último evento          */
    Punto   b[4];                  /* los medios pasos del arranque       */
    unsigned mask;
    int     ic;                    /* entrada de la caché en uso          */
    double  x[LU_MAX];
    int     hay_p;                 /* ya hay un punto registrado          */
    double  pt, pV[MAX_EL], pI[MAX_EL];   /* el último punto registrado   */
    double  ancla_val[MAX_NODOS];
    /* Etapa 4: los no lineales */
    NlEst   nl;                    /* el estado de Newton de la emulación  */
    int     katz;                  /* el próximo paso decide los tramos (Katzenelson) */
    int     nivel, buenos, pos;    /* subpasos: h/2^nivel; pos en h/1024 dentro del paso */
    double  xa[LU_MAX], ta;        /* el aceptado anterior (el predictor) */
    int     hay_xa;
    int     nh;                    /* historia del error local (C y L)    */
    double  ht[3], hx[3][MAX_EL];
    double  imax_nl[MAX_EL], inl[MAX_EL];   /* el mayor |i| y el último de cada suave (Δi) */
    int     ev_mismo_t;            /* Zenón                               */
    double  t_ev_nl;
} M;

/* Las clases de evento de una red no lineal con su presonda (§17.3): la
 * linealización en te⁺ de la primera de cada clase, desde el punto de
 * operación, y sus modos. Una clase: los interruptores antes y después y el
 * signo del salto de cada fuente (y el de la pendiente en un vértice). */
#define RIG_NCL   8                         /* clases con su presonda                */
#define RIG_NENUM 64                        /* eventos que se miran para las clases  */
static struct {
    int n;                                  /* clases                                */
    unsigned long long clave[RIG_NCL];
    double te[RIG_NCL];                     /* la primera de cada una                */
    int cuenta[RIG_NCL], resto;             /* sus eventos entre los mirados; los de las que no caben */
    int nm[RIG_NCL];                        /* sus modos en te⁺ (-1: la presonda no pudo) */
    SimModo md[RIG_NCL][SIM_MAX_MODOS];
    double tau[RIG_NCL];                    /* 1/|s| del más rápido                   */
    int rig[RIG_NCL];                       /* su tramo con el h elegido (pasos h)   */
    int alguna;                             /* alguna clase tiene otro tramo que la red */
    double costo;                           /* las operaciones de la presonda (una vez) */
} PS;

double sim_residuo_max(void) { return res_.nw.residuo_max; }

/* Interruptores cerrados en t (por la izquierda: el estado de antes). Dos
 * instantes a menos de 1e-9·h son el mismo (como en siguiente_evento): si
 * no, de dos interruptores que actúan "a la vez" con un ulp de diferencia,
 * uno se perdería. */
static unsigned mask_de(double t, int izq)
{
    unsigned m = 0;
    int k;
    double tol = (M.h > 0.0 ? 1e-9 * M.h : 0.0) + 1e-13 * fabs(t);
    for (k = 0; k < R_.nel; k++) {
        const SimEl *e = &R_.el[k];
        int c, cambios = 0;
        if (e->tipo != SE_S) continue;
        c = e->cerrado0;
        if (e->t1 >= 0.0 && (izq ? t > e->t1 + tol : t >= e->t1 - tol)) cambios++;
        if (e->t2 >= 0.0 && (izq ? t > e->t2 + tol : t >= e->t2 - tol)) cambios++;
        if (cambios & 1) c = !c;
        if (c) m |= 1u << k;
    }
    return m;
}

static void anota_diag(int estado, int elem, double t)
{
    res_.d.estado = estado; res_.d.elem = elem; res_.d.t = t;
}

static const unsigned char *tramos_act(void) { return nl_act ? nl_act->tramo : 0; }

/* La clave de unos tramos: 2 bits por elemento */
static SimTramos clave_tr(const unsigned char *tr)
{
    SimTramos c = 0;
    int k;
    if (!tr) return 0;
    for (k = 0; k < R_.nel && k < 32; k++) c |= (SimTramos)(tr[k] & 3u) << (2 * k);
    return c;
}

/* Factorización (del trapecio con paso hh) para la máscara y los tramos tr;
 * la topología del transitorio se analiza con los potenciales de ancla del
 * momento. Con bs, la matriz se deja sin factorizar en bs (la base de los
 * suaves); con suaves = 1, se le suman los compañeros de nl_act (los modos) */
static int prepara_lu(LU *f, Topo *tp, unsigned mask, double hh, double t, const unsigned char *tr,
                      double (*bs)[LU_MAX], int suaves)
{
    SimDiag d;
    int n, e;
    memset(&d, 0, sizeof d);
    d.elem = d.nodo = -1;
    e = analiza(tp, 1, mask, t, &d);
    if (e != SIM_OK) { anota_diag(d.estado, d.elem, t); res_.d.lazo = d.lazo; return 0; }
    for (n = 0; n < R_.nn; n++)
        if (tp->sust_nodo[n] == SU_ANCLA) { tp->ancla[n] = M.ancla_val[n]; avisa(SIM_AV_FLOTA, -1); }
    ensambla(f, tp, 1, hh, tr);
    if (bs) {
        for (n = 0; n < nincog; n++) memcpy(bs[n], f->a[n], (size_t)nincog * sizeof f->a[0][0]);
        return 1;
    }
    if (suaves && NL.suave && nl_act) {
        int q;
        for (q = 0; q < NL.n; q++)
            if (es_suave(&R_.el[NL.el[q]])) estampa_suave(f, 0, tp, &R_.el[NL.el[q]], &nl_act->lin[NL.el[q]]);
    }
    if (!(NL.n ? factoriza_nl(f) : lu_factoriza(f))) { anota_diag(SIM_SINGULAR, -1, t); return 0; }
    return 1;
}

/* Antes de correr: la topología de cada instante en que actúa un
 * interruptor, para que un lazo o un corte salga ya, con su instante */
static int revisa_topologias(void)
{
    static Topo tp;
    int k, q;
    for (k = 0; k < R_.nel; k++) {
        const SimEl *e = &R_.el[k];
        if (e->tipo != SE_S) continue;
        for (q = 0; q < 2; q++) {
            double t = q == 0 ? e->t1 : e->t2;
            SimDiag d;
            if (!(t >= 0.0)) continue;
            memset(&d, 0, sizeof d);
            d.elem = d.nodo = -1;
            if (analiza(&tp, 1, mask_de(t, 0), t, &d) != SIM_OK &&
                (res_.d.estado == SIM_OK || t < res_.d.t)) {    /* el primero en el tiempo */
                res_.d.estado = d.estado; res_.d.elem = d.elem; res_.d.nodo = d.nodo;
                res_.d.lazo = d.lazo; res_.d.t = t;
            }
        }
    }
    return res_.d.estado == SIM_OK;
}

static int cache_de(unsigned mask, const unsigned char *tr, double hh, double t)
{
    int i, peor = 0, nc = NL.n || M.rig_h ? N_CACHE : 4;   /* el tramo rígido usa hasta 4 por topología */
    SimTramos c = clave_tr(tr);
    for (i = 0; i < nc; i++)
        if (cache_uso[i] && cache_mask[i] == mask && cache_hh[i] == hh && cache_tr[i] == c) {
            cache_uso[i] = ++cache_reloj;
            return i;
        }
    for (i = 1; i < nc; i++) if (cache_uso[i] < cache_uso[peor]) peor = i;
    cache_uso[peor] = 0;
    if (!prepara_lu(&lu_cache[peor], &topo_cache[peor], mask, hh, t, tr, NL.suave ? lu_base[peor] : 0, 0)) return -1;
    res_.factorizaciones++;
    cache_mask[peor] = mask; cache_hh[peor] = hh; cache_tr[peor] = c; cache_uso[peor] = ++cache_reloj;
    return peor;
}

static SimDiag diag_snap_;
static void diag_foto(void)   { diag_snap_ = res_.d; }
static void diag_vuelve(void) { res_.d = diag_snap_; }

/* La LU de la regla en el transitorio: la entrada de paso h con esos tramos
 * (singular: el lineal no vale; no es un error de la emulación) */
static LU *regla_tr(const NlEst *st, const Topo **tp, double (**bs)[LU_MAX])
{
    int e, n;
    diag_foto();
    e = cache_de(NC.mask, st->tramo, M.h, NC.t);
    if (e < 0) { diag_vuelve(); return 0; }
    for (n = 0; n < R_.nn; n++) if (topo_cache[e].sust_nodo[n] == SU_ANCLA) topo_cache[e].ancla[n] = M.ancla_val[n];
    *tp = &topo_cache[e];
    *bs = lu_base[e];
    return &lu_cache[e];
}

/* La matriz del paso del Newton (NC): la entrada de la caché o, en un paso
 * partido, la suelta (que se recuerda mientras no cambien los tramos). Las
 * anclas, las de ahora. */
static LU *matriz_tr(const NlEst *st, const Topo **tp, double (**bs)[LU_MAX])
{
    int i, n;
    Topo *t;
    if (NC.suelta) {
        SimTramos c = clave_tr(st->tramo);
        if (!suelta_ok || suelta_mask != NC.mask || suelta_hh != NC.hh || suelta_tr != c) {
            suelta_ok = 0;
            if (!prepara_lu(&lu_suelta, &topo_suelta, NC.mask, NC.hh, M.t, st->tramo, NL.suave ? lu_base_suelta : 0, 0))
                return 0;
            res_.factorizaciones++;
            suelta_ok = 1; suelta_mask = NC.mask; suelta_hh = NC.hh; suelta_tr = c;
        }
        t = &topo_suelta;
        *bs = lu_base_suelta;
        *tp = t;
        for (n = 0; n < R_.nn; n++) if (t->sust_nodo[n] == SU_ANCLA) t->ancla[n] = M.ancla_val[n];
        return &lu_suelta;
    }
    i = cache_de(NC.mask, st->tramo, NC.hh, NC.t);
    if (i < 0) return 0;
    t = &topo_cache[i];
    for (n = 0; n < R_.nn; n++) if (t->sust_nodo[n] == SU_ANCLA) t->ancla[n] = M.ancla_val[n];
    *tp = t;
    *bs = lu_base[i];
    return &lu_cache[i];
}

/* El siguiente evento después de t: un flanco, un vértice, un interruptor */
static double siguiente_evento(double t)
{
    double m = NUNCA, tol = 1e-9 * M.h;
    int k;
    for (k = 0; k < R_.nel; k++) {
        const SimEl *e = &R_.el[k];
        if (e->tipo == SE_E || e->tipo == SE_J) {
            double ev = onda_evento(e->forma, e->p, t);
            if (ev < m) m = ev;
        } else if (e->tipo == SE_S) {
            if (e->t1 >= 0.0 && e->t1 > t + tol && e->t1 < m) m = e->t1;
            if (e->t2 >= 0.0 && e->t2 > t + tol && e->t2 < m) m = e->t2;
        }
    }
    return m;
}

/* La clase de un evento en t (§17.3, la presonda): los interruptores antes y
 * después, el signo del salto de cada fuente y, en un vértice de una
 * triangular, el de su pendiente nueva (FNV-1a) */
static unsigned long long clase_de(double t)
{
    unsigned long long c = 1469598103934665603ULL;
    unsigned m[2];
    int k, i;
    m[0] = mask_de(t, 1); m[1] = mask_de(t, 0);
    for (i = 0; i < 8; i++) { c ^= (m[i >> 2] >> (8 * (i & 3))) & 0xFFu; c *= 1099511628211ULL; }
    for (k = 0; k < R_.nel; k++) {
        const SimEl *e = &R_.el[k];
        double a, b, tol;
        int s = 0, p = 0;
        if (e->tipo != SE_E && e->tipo != SE_J) continue;
        a = onda_valor_lado(e->forma, e->p, t, -1);
        b = onda_valor_lado(e->forma, e->p, t, 1);
        tol = 1e-12 * (fabs(a) + fabs(b)) + 1e-300;
        s = b > a + tol ? 1 : b < a - tol ? -1 : 0;
        if (e->forma == ONDA_TRIANGULAR && onda_periodo(e->forma, e->p) > 0.0) {
            double d = 1e-6 * onda_periodo(e->forma, e->p), v0 = onda_valor(e->forma, e->p, t);
            double va = onda_valor(e->forma, e->p, t - d), vb = onda_valor(e->forma, e->p, t + d);
            if ((vb - v0) * (v0 - va) < 0.0) p = vb > v0 ? 1 : -1;      /* un vértice */
        }
        c ^= (unsigned long long)(k * 9 + (s + 1) * 3 + (p + 1)); c *= 1099511628211ULL;
    }
    ops_propias += (unsigned long)(6 * R_.nel);
    return c;
}

/* Valor de un canal: NODO, phi; I, sg·I; V, sg·(phi(a) - phi(b)) */
static double canal_val(const Valores *v, int c)
{
    const SimCanal *ca = &aj_.canal[c];
    const SimEl *e;
    if (ca->tipo == CANAL_NODO) return (ca->nodo >= 0 && ca->nodo < R_.nn) ? v->phi[ca->nodo] : 0.0;
    if (ca->elem < 0 || ca->elem >= R_.nel) return 0.0;
    e = &R_.el[ca->elem];
    if (ca->tipo == CANAL_I) return ca->sg * v->I[ca->elem];
    if (ca->tipo == CANAL_V) return ca->sg * (v->phi[e->a] - v->phi[e->b]);
    return 0.0;
}

/* Las integrales dentro del tramo rígido (§17.3): del evento al final del
 * tramo, W, la ventana de medida y las integrales de los canales no van por
 * el trapecio entre las muestras registradas, sino subpaso a subpaso
 * (rq_suma: V e I con los pesos del método; sus productos, con la media de
 * los extremos por la integral del método). rq_s1/rq_s2: lo de los canales
 * desde la última muestra; rq_V... e rq_y: la etapa 1 del subpaso (rq_e1);
 * rq_V0... e rq_y0: el principio del subpaso (la y+ en el primero) */
static int rq_on, rq_e1;
static double rq_s1[2], rq_s2[2];
static double rq_V[MAX_EL], rq_I[MAX_EL], rq_V2[MAX_EL], rq_I2[MAX_EL], rq_y[2];
static double rq_V0[MAX_EL], rq_I0[MAX_EL], rq_V20[MAX_EL], rq_I20[MAX_EL], rq_y0[2];

static void rq_desde(const double *V, const double *I, const double *V2, const double *I2, const double *y)
{
    memcpy(rq_V0, V, sizeof rq_V0); memcpy(rq_I0, I, sizeof rq_I0);
    memcpy(rq_V20, V2, sizeof rq_V20); memcpy(rq_I20, I2, sizeof rq_I20);
    rq_y0[0] = y[0]; rq_y0[1] = y[1];
}

/* ---- Registro: muestras, máximos, energías y ventana de medida ---- */
static void muestra(double t, const double *y)
{
    int n = reg_.n, c;
    if (n >= SIM_MAX_MUESTRAS) { fuera_++; return; }
    reg_.t[n] = t;
    for (c = 0; c < 2; c++) {
        double yy = y[c];
        reg_.y[c][n] = yy;
        if (n == 0) {
            reg_.s1[c][0] = reg_.s2[c][0] = 0.0;
            reg_.ymin[c] = reg_.ymax[c] = yy;
        } else {
            double dt = t - reg_.t[n - 1], ya = reg_.y[c][n - 1];
            if (rq_on) {                          /* en el tramo rígido: su cuadratura hasta aquí */
                reg_.s1[c][n] = reg_.s1[c][n - 1] + (dt > 0.0 ? rq_s1[c] : 0.0);
                reg_.s2[c][n] = reg_.s2[c][n - 1] + (dt > 0.0 ? rq_s2[c] : 0.0);
                if (dt > 0.0) rq_s1[c] = rq_s2[c] = 0.0;
            } else {
                reg_.s1[c][n] = reg_.s1[c][n - 1] + 0.5 * (ya + yy) * dt;
                reg_.s2[c][n] = reg_.s2[c][n - 1] + 0.5 * (ya * ya + yy * yy) * dt;
            }
            if (yy < reg_.ymin[c]) reg_.ymin[c] = yy;
            if (yy > reg_.ymax[c]) reg_.ymax[c] = yy;
        }
    }
    reg_.n = n + 1;
    ops_propias += 12;
}

/* El segundo puerto del último punto (el AO: Vd y su I por Ri; el BJT: VBE
 * e IB), que el balance cuenta con su signo s_k (§3.H): -1 en el AO (entrega
 * V·I y absorbe V2·I2), +1 en el BJT y el MOS (absorben por los dos) */
static double p2V[MAX_EL], p2I[MAX_EL], r2V[MAX_EL], r2I[MAX_EL];
/* s_k como entero (el ±1 de los productos va como (double)s2: la misma
 * multiplicación de antes) */
static int s2_de(const SimEl *e) { return e->tipo == SE_AO ? -1 : (e->tipo == SE_BJT || e->tipo == SE_MOS) ? 1 : 0; }

/* La parte de un tramo [ta, tb] que cae en la ventana de medida: lo que no
 * depende del elemento, una vez por registra. 0 si el tramo acaba antes;
 * si empieza antes, corta y la fracción f de [ta, tb] que queda fuera; hd,
 * la mitad del trozo de dentro */
typedef struct { int corta; double f, hd; } Ventana;
static int ventana_tramo(double ta, double tb, Ventana *w)
{
    double t0 = res_.t_med0;
    if (tb <= t0) return 0;
    w->corta = ta < t0;
    if (w->corta) { w->f = (t0 - ta) / (tb - ta); ta = t0; }
    w->hd = 0.5 * (tb - ta);
    return 1;
}

/* Lo de cada elemento en ese trozo; la P, con el segundo puerto (s2 = s_k),
 * como Valores.P y la W */
static void ventana(int k, const Ventana *w, double Va, double Vb, double Ia, double Ib, int s2)
{
    double hd = w->hd, V2a = p2V[k], V2b = r2V[k], I2a = p2I[k], I2b = r2I[k];
    if (w->corta) {
        double f = w->f;
        Va += f * (Vb - Va); Ia += f * (Ib - Ia);
        V2a += f * (V2b - V2a); I2a += f * (I2b - I2a);
    }
    iv_V[k] += hd * (Va + Vb);  iv_V2[k] += hd * (Va * Va + Vb * Vb);
    iv_I[k] += hd * (Ia + Ib);  iv_I2[k] += hd * (Ia * Ia + Ib * Ib);
    iv_P[k] += hd * (Va * Ia + Vb * Ib);           /* en una R: Vef²/R exacto */
    if (s2) iv_P[k] += hd * (double)s2 * (V2a * I2a + V2b * I2b);
}

/* Lo que no cambia de un elemento a otro va fuera del bucle: si se integra
 * (dt > 0 fuera del tramo rígido), dt/4, el trozo de la ventana. Los |a| >
 * |b| van con los bits (mayor_abs: lo mismo, NaN incluido), y la E solo en
 * C y L: en los demás era -1 > Emax = 0 (el memset de sim_iniciar_red, y
 * nadie más la escribe), que nunca pasa */
static void registra(double t, const double *V, const double *I, const double *y)
{
    Ventana w = { 0, 0.0, 0.0 };
    int k, integra, en_ventana = 0;
    double dt = M.hay_p ? t - M.pt : 0.0, dq = 0.0;
    muestra(t, y);
    integra = dt > 0.0 && !rq_on;                 /* en el tramo rígido, rq_suma */
    if (integra) { dq = dt * 0.25; en_ventana = ventana_tramo(M.pt, t, &w); }
    for (k = 0; k < R_.nel; k++) {
        const SimEl *e = &R_.el[k];
        int s2 = s2_de(e);
        double P = V[k] * I[k];
        if (s2) P += (double)s2 * r2V[k] * r2I[k];     /* los dos puertos, como Valores.P */
        if (mayor_abs(V[k], res_.Vmax[k])) res_.Vmax[k] = V[k];
        if (mayor_abs(I[k], res_.Imax[k])) res_.Imax[k] = I[k];
        if (mayor_abs(P, res_.Pmax[k])) res_.Pmax[k] = P;
        if (integra) {
            /* en el trapecio, V e I medias hacen exacto el balance (Tellegen
             * en los puntos medios) */
            res_.W[k] += dq * (M.pV[k] + V[k]) * (M.pI[k] + I[k]);
            if (e->tipo == SE_AO) res_.W[k] -= dq * (p2V[k] + r2V[k]) * (p2I[k] + r2I[k]);
            else if (e->tipo == SE_BJT || e->tipo == SE_MOS) res_.W[k] += dq * (p2V[k] + r2V[k]) * (p2I[k] + r2I[k]);
            if (en_ventana) ventana(k, &w, M.pV[k], V[k], M.pI[k], I[k], s2);
        }
        if (s2) { p2V[k] = r2V[k]; p2I[k] = r2I[k]; }
        if (e->tipo == SE_C || e->tipo == SE_L) {
            double E = e->tipo == SE_C ? 0.5 * e->val * V[k] * V[k] : 0.5 * e->val * I[k] * I[k];
            if (E > res_.Emax[k]) res_.Emax[k] = E;
        }
        M.pV[k] = V[k]; M.pI[k] = I[k];
    }
    M.pt = t; M.hay_p = 1;
    ops_propias += (unsigned long)((en_ventana ? 26 : 10) * R_.nel);   /* 16 por elemento en la ventana */
}

static void emite(const Punto *p)
{
    memcpy(r2V, p->V2, sizeof r2V); memcpy(r2I, p->I2, sizeof r2I);
    registra(p->t, p->V, p->I, p->y);
}

/* y+ en t_evento: extrapolación de Lagrange con los puntos idx[] del
 * arranque */
static void emite_mas(const int *idx, int n)
{
    static Punto q;
    int i, m, k;
    q.t = M.t_evento;
    for (k = 0; k < R_.nel; k++) q.V[k] = q.I[k] = q.V2[k] = q.I2[k] = 0.0;
    q.y[0] = q.y[1] = 0.0;
    for (i = 0; i < n; i++) {
        const Punto *p = &M.b[idx[i]];
        double c = 1.0;
        for (m = 0; m < n; m++)
            if (m != i) c *= (M.t_evento - M.b[idx[m]].t) / (p->t - M.b[idx[m]].t);
        for (k = 0; k < R_.nel; k++) {
            q.V[k] += c * p->V[k]; q.I[k] += c * p->I[k];
            if (R_.el[k].tipo == SE_AO || R_.el[k].tipo == SE_BJT || R_.el[k].tipo == SE_MOS) {
                q.V2[k] += c * p->V2[k]; q.I2[k] += c * p->I2[k];
            }
        }
        q.y[0] += c * p->y[0]; q.y[1] += c * p->y[1];
    }
    ops_propias += (unsigned long)(n * (4 * R_.nel + 8));
    emite(&q);
}

/* El arranque tras un evento: ARR_PASOS pasos de Euler implícito de h/8
 * (L-estable: amortigua lo que el trapecio dejaría oscilando; con pasos
 * cortos, su error de primer orden es pequeño), y luego el trapecio desde
 * te + h con una historia BDF2 (de segundo orden) de los tres últimos.
 *
 * Salida del paso j (1..8). La y+ (el valor justo después del evento) sale
 * por extrapolación cuadrática: sin impulso, de los pasos 1, 2 y 3; con
 * impulso, de los 2, 3 y 4 (el primero lleva la corriente del impulso, que
 * no es una muestra). Luego se registran te + h/2 y te + h. Si el arranque
 * se corta (corta = acaba aquí por un evento o por el final), se registra
 * el último paso.
 *
 * El TRAMO RÍGIDO: en una red con un modo de τ < 5h (h·|λ| > RIG_HL), el
 * error de primer orden de esos 8 pasos (2,6e-2 del salto con τ = 0,3h en
 * te + h; 4,9e-3 con τ = 3h) no se apaga: el trapecio lo arrastra con
 * ρ = (1 − hλ/2)/(1 + hλ/2) por paso (→ −1 si τ << h; 1,7e-2 en te + 2h), y
 * con τ/h entre 0,3 y 3 el propio trapecio desde te + h deja 1e-2 aunque
 * arranque exacto. Ahí, en vez del arranque, tras cada evento van subpasos
 * con SDIRK2 (Alexander, γ = 1 − √2/2: L-estable, de segundo orden,
 * rígidamente preciso y sin la derivada en te⁺, que tras un evento no se
 * sabe; sus dos etapas son Euler implícitos con la misma matriz, la del
 * trapecio con hh = 2γ·subpaso): de h/RIG_SUB hasta te + h y de h/RIG_SUB2
 * (el modo ya bajó a e^-2 o menos) hasta te + n·h, n = ⌈RIG_TAUS/(|λ|·h)⌉
 * del rígido más lento (el modo ya se apagó: e^-10), y luego el trapecio de
 * siempre. Con SDIRK2 y el paso h desde te + h quedaba 3,6e-3 con τ = 0,27h;
 * con 8 subpasos en vez de 16, el banco [68] fallaba en 8 a 12 casos.
 * Se registran las mismas muestras (y+, te + h/2, te + h, te + 2h...); la
 * y+, la consistente (rig_ymas), no extrapolada; W, la ventana y los canales,
 * subpaso a subpaso (rq_suma). Con no lineales, el tramo de un evento de las
 * fuentes o de un S lo deciden los modos en te⁺ de su clase (presonda, a
 * priori en elige_paso: así cuenta en los 2 s y en los topes), y los primeros
 * subpasos se parten hasta que z = s/τ no pase de 1 + √2 (paso_nl_rig). Sin
 * modos rígidos no cambia nada. */
#define ARR_PASOS 8
#define RIG_HL    0.2                       /* h·|λ| de un modo rígido (τ < 5h)   */
#define RIG_SUB   16                        /* subpasos en [te, te + h]           */
#define RIG_SUB2  4                         /* y en cada h que sigue              */
#define RIG_TAUS  10.0                      /* el tramo dura RIG_TAUS/|λ|...      */
#define RIG_NMAX  5                         /* ...y como mucho RIG_NMAX pasos h   */
#define RIG_G     0.29289321881345248       /* γ = 1 − √2/2                       */
#define RIG_K     2.4142135623730950        /* (1 − γ)/γ = 1 + √2                 */
#define RIG_EPS   1e-3                      /* la y+: Euler implícito de RIG_EPS·τmín */

/* El instante del tramo tras j subpasos más una fracción f del siguiente */
static double rig_t(int j, double f)
{
    if (j < RIG_SUB) return M.t_ancla + ((double)j + f) * (M.h / RIG_SUB);
    return M.t_ancla + ((double)(RIG_SUB2 + j - RIG_SUB) + f) * (M.h / RIG_SUB2);
}

/* Si el subpaso j acaba en te + i·h/2, i; si no, -1 */
static int rig_medio(int j)
{
    if (j <= RIG_SUB) return j % (RIG_SUB / 2) == 0 ? j / (RIG_SUB / 2) : -1;
    return (j - RIG_SUB) % (RIG_SUB2 / 2) == 0 ? 2 + (j - RIG_SUB) / (RIG_SUB2 / 2) : -1;
}

static void arranque_salida(int j, int corta, const Punto *act)
{
    static const int P1[1] = { 0 }, P2[1] = { 1 }, P12[2] = { 0, 1 }, P123[3] = { 0, 1, 2 },
                     P23[2] = { 1, 2 }, P234[3] = { 1, 2, 3 };
    int i = M.rig_n ? rig_medio(j) : j == ARR_PASOS / 2 ? 1 : j == ARR_PASOS ? 2 : -1;   /* en te + i·h/2 */
    if (!M.en_arr) return;
    if (M.pend && (corta || j == (M.impulso ? 4 : 3))) {
        if (M.impulso) {
            if (j >= 4) emite_mas(P234, 3);
            else if (j == 3) emite_mas(P23, 2);
            else if (j == 2) emite_mas(P2, 1);
            else emite_mas(P1, 1);
        } else {
            if (j >= 3) emite_mas(P123, 3);
            else if (j == 2) emite_mas(P12, 2);
            else emite_mas(P1, 1);
        }
        M.pend = 0;
    }
    if (corta || i == 1 || (i > 0 && i % 2 == 0)) emite(act);           /* te + h/2, te + h, te + 2h... */
    if (corta || j == (M.rig_n ? M.rig_n : ARR_PASOS)) M.en_arr = 0;
}

/* ¿Es algebraico este C (esta L) en la topología de ahora? Un C lo es si
 * sus bornes se unen por otras E ideales, A, S cerrados o C (un lazo
 * capacitivo: su i_C sigue a la derivada de las fuentes); una L, si sus
 * bornes no se unen por ramas que no sean J, V, S abiertos ni L (un corte
 * inductivo). */
static int algebraica(int k)
{
    const SimEl *e = &R_.el[k];
    int n, j, ref = -1;
    for (n = 0; n < R_.nn; n++) uf[n] = n;
    for (n = 0; n < R_.nn; n++)
        if (R_.ref[n]) { if (ref < 0) ref = n; else uf_une(ref, n); }
    for (j = 0; j < R_.nel; j++) {
        const SimEl *f = &R_.el[j];
        int cerrado = f->tipo == SE_S && ((M.mask >> j) & 1u), une;
        if (j == k) continue;
        if (f->tipo == SE_AO) {
            /* la salida (una rama V, o su Norton) y las entradas: el nulador en el
             * lineal del ideal (la masa virtual) para un C; Ri para una L */
            if (e->tipo == SE_C) {
                if (f->modelo == NL_AO_IDEAL || f->m[4] == 0.0) uf_une(f->nref, f->c);
                if (f->modelo == NL_AO_IDEAL && nl_act && nl_act->tramo[j] == TR_LIN) uf_une(f->a, f->b);
            } else {
                uf_une(f->nref, f->c);
                if (f->modelo == NL_AO_REAL && f->m[3] > 0.0) uf_une(f->a, f->b);
            }
            continue;
        }
        if (e->tipo == SE_C)
            une = (f->tipo == SE_E && f->val == 0.0) || f->tipo == SE_EC || f->tipo == SE_AMI || f->tipo == SE_C || cerrado ||
                  /* un diodo por tramos que conduce con R_τ·C < h: el modo R_τ·C, mucho más
                   * rápido que h, no queda zumbando en el trapecio (§3.B) */
                  (f->tipo == SE_D && f->modelo == NL_TRAMOS && nl_act && nl_act->tramo[j] != TR_CORTE &&
                   (nl_act->tramo[j] == TR_CONDUCE ? f->m[1] : f->m[3]) * e->val < M.h);
        else {
            une = !(f->tipo == SE_J || f->tipo == SE_JC || f->tipo == SE_VMI || f->tipo == SE_L || (f->tipo == SE_S && !cerrado));
            if (f->tipo == SE_BJT) uf_une(f->c, f->b);  /* una G entre sus tres patas */
        }
        if (une) uf_une(f->a, f->b);
    }
    ops_propias += (unsigned long)(3 * (R_.nn + R_.nel));
    return (uf_raiz(e->a) == uf_raiz(e->b)) == (e->tipo == SE_C);
}

/* La historia del trapecio al acabar el arranque (en te + h).
 *  - Un C o una L con estado propio se queda con la i_C (v_L) de la
 *    solución en te + h: es la derivada exacta de su estado, la misma que
 *    llevaría el trapecio, y no arrastra los modos que el paso no resuelve
 *    (una L de tau << h ya está en su régimen).
 *  - Un C o una L algebraicos (i_C = C·dE/dt en un C sobre una E ideal)
 *    toman i_C = C·dv/dt y v_L = L·di/dt por BDF2 con separación h/2, con
 *    el estado en te+ (extrapolado como la y+), en te + h/2 y en te + h.
 *    Con separación h/2 la BDF2 tiene el mismo sesgo que el trapecio
 *    (D - h²D³/12): el trapecio sigue sin oscilación alterna. La del
 *    Euler implícito no cuadra con la del trapecio y oscilaría.
 * Al final de un tramo rígido (te + n·h), lo mismo con el estado en
 * te + (n − 1)·h (con n = 1, el de te⁺ extrapolado), te + (n − ½)·h y te + n·h. */
static void historia_bdf2(const Punto *ult)
{
    static const int P123[3] = { 0, 1, 2 }, P234[3] = { 1, 2, 3 };
    const int *p = M.impulso ? P234 : P123;
    double w[3];
    int i, m, k;
    for (i = 0; i < 3; i++) {
        w[i] = 1.0;
        for (m = 0; m < 3; m++)
            if (m != i) w[i] *= (M.t_evento - M.b[p[m]].t) / (M.b[p[i]].t - M.b[p[m]].t);
    }
    for (k = 0; k < R_.nel; k++) {
        const SimEl *e = &R_.el[k];
        double x0 = 0.0, x4, x8, der;
        if (e->tipo != SE_C && e->tipo != SE_L) continue;
        if (!algebraica(k)) continue;               /* iC/vL: los de te + h */
        if (M.rig_n > RIG_SUB || (M.rig_n && M.ymas > 0)) x0 = M.rx[0][k];   /* te + (n − 1)·h, o la y+ consistente */
        else for (i = 0; i < 3; i++) x0 += w[i] * (e->tipo == SE_C ? M.b[p[i]].V[k] : M.b[p[i]].I[k]);
        x4 = M.rig_n ? M.rx[1][k] : e->tipo == SE_C ? M.b[3].V[k] : M.b[3].I[k];
        x8 = e->tipo == SE_C ? ult->V[k] : ult->I[k];
        der = (3.0 * x8 - 4.0 * x4 + x0) / M.h;
        if (e->tipo == SE_C) iC[k] = e->val * der; else vL[k] = e->val * der;
    }
    ops_propias += (unsigned long)(12 * R_.nel);
}

static void vacia_pendiente(void)
{
    M.en_arr = 0;
    M.pend = 0;
}

/* ---- El tramo rígido (SDIRK2 con subpasos de h/RIG_SUB y h/RIG_SUB2) ----
 * Pasos h del tramo con el paso h: ⌈RIG_TAUS/(|λ|·h)⌉ del más lento de los
 * modos con h·|λ| > RIG_HL, como mucho RIG_NMAX; 0 si no hay ninguno (no es
 * rígida con ese h). De unos modos (los de una clase de evento, de su
 * presonda), o de los de todas las topologías (la red) */
static int rig_pasos_de(const SimModo *s, int nm, double h, int n)
{
    int k;
    for (k = 0; k < nm; k++) {
        double mag = s[k].mag, m;
        if (!(mag * h > RIG_HL)) continue;
        m = ceil(RIG_TAUS / (mag * h) - 1e-9);
        if (m > (double)n) n = (int)m;
    }
    return n > RIG_NMAX ? RIG_NMAX : n;
}

static int rig_pasos(double h)
{
    int i, n = 0;
    for (i = 0; i < res_.ntop; i++) n = rig_pasos_de(res_.modo[i], res_.nmodos[i], h, n);
    return n;
}

/* Los subpasos del tramo de un evento con n pasos h (0: el arranque de Euler) */
static void rig_pon(int n)
{
    M.rig_nh = n;
    M.rig_n = n ? RIG_SUB + RIG_SUB2 * (n - 1) : 0;
    M.rig = M.rig_n;
    M.rig_amp = 1.0;                              /* el modo rápido, entero (paso_nl_rig) */
    M.arranque = M.rig_n ? 0 : ARR_PASOS;
    rq_on = M.rig_n > 0;                          /* sus integrales, con la cuadratura */
    if (rq_on) {                                  /* desde el último punto (te⁻), hasta que salga la y+ */
        double y[2];
        y[0] = reg_.n > 0 ? reg_.y[0][reg_.n - 1] : 0.0;
        y[1] = reg_.n > 0 ? reg_.y[1][reg_.n - 1] : 0.0;
        rq_desde(M.pV, M.pI, p2V, p2I, y);
    }
}

/* V e I de cada elemento en x (los convenios de valores_de, con el segundo
 * puerto de AO, BJT y MOS) y los dos canales, en rq_V... y rq_y: la etapa 1
 * de un subpaso del tramo, para rq_suma (sin val, P ni regiones: lo ligero) */
static void rq_etapa1(const double *x, double t)
{
    int k, c;
    for (k = 0; k < R_.nel; k++) {
        const SimEl *e = &R_.el[k];
        double pa = vnodo(x, e->a), pb = vnodo(x, e->b), V = pa - pb, I = 0.0, V2 = 0.0, I2 = 0.0;
        int j = col_br[k];
        switch (e->tipo) {
        case SE_E:
            V = pb - pa;
            I = j >= 0 ? x[j] : (fuente_en(e, t, 0) - V) / e->val;
            break;
        case SE_J:  V = pb - pa; I = fuente_en(e, t, 0); break;
        case SE_R:  I = V / e->val; break;
        case SE_EC: V = pb - pa; I = x[j]; break;
        case SE_JC: V = pb - pa; I = e->val * ctl_valor(x, k, t, 0, 1.0); break;
        case SE_D:  I = j >= 0 ? x[j] : nl_act ? i_union(&nl_act->lin[k], V) : 0.0; break;
        case SE_AO:
            V = vnodo(x, e->c) - vnodo(x, e->nref);
            I = j >= 0 ? x[j] : (ao_u(e, nl_act ? nl_act->tramo[k] : TR_LIN, pa - pb) - V) / e->m[4];
            V2 = pa - pb;
            I2 = e->modelo == NL_AO_REAL && e->m[3] > 0.0 ? V2 / e->m[3] : 0.0;
            break;
        case SE_BJT:
        case SE_MOS: {
            double vp[2], ip[2] = { 0.0, 0.0 };
            puertos_v(e, x, vp);
            if (nl_act) i_puertos(&nl_act->lin[k], vp, ip);
            I = ip[1]; V2 = vp[0];
            if (e->tipo == SE_BJT) I2 = ip[0];
            break;
        }
        case SE_VMI: break;
        case SE_AMI: I = x[j]; if (e->instr == 'W') V = pb; break;
        default:    I = j >= 0 ? x[j] : 0.0; break;    /* C, L, S */
        }
        rq_V[k] = V; rq_I[k] = I; rq_V2[k] = V2; rq_I2[k] = I2;
    }
    for (c = 0; c < 2; c++) {                     /* los canales, como canal_val */
        const SimCanal *ca = &aj_.canal[c];
        rq_y[c] = 0.0;
        if (ca->tipo == CANAL_NODO) rq_y[c] = ca->nodo >= 0 && ca->nodo < R_.nn ? vnodo(x, ca->nodo) : 0.0;
        else if (ca->elem >= 0 && ca->elem < R_.nel) {
            if (ca->tipo == CANAL_I) rq_y[c] = ca->sg * rq_I[ca->elem];
            else if (ca->tipo == CANAL_V) rq_y[c] = ca->sg * (vnodo(x, R_.el[ca->elem].a) - vnodo(x, R_.el[ca->elem].b));
        }
    }
    rq_e1 = 1;
    ops_propias += (unsigned long)(2 * R_.nel + 4);
}

/* La cuadratura de un subpaso [t0, t0 + s] del tramo. Las integrales de V y
 * de I, con los pesos del método (1 − γ y γ en sus dos etapas: la 1 en
 * rq_V..., la 2 en v, y; sin etapa 1, el Euler de un subpaso entero o de
 * localiza_nl, s en el final): así ∫I de un C es su carga, C·Δv. Las de un
 * producto (V·I, V², I², y²), con la media de sus dos extremos por la
 * integral del método: ∫V·I = V̄·∫I, V̄ = (V0 + V)/2 (V0: el principio del
 * subpaso; en el primero, la y+). Los extremos cumplen Kirchhoff de
 * tensiones y las etapas el de corrientes: Tellegen vale y la suma de las W
 * es exacta; en un C, W = C·V̄·Δv = Δ(½CV²), así que en una red sin L el
 * balance es exacto. Y lo que disipa un modo que se apaga dentro de una
 * etapa lo ve la y+: en [41] (0,5 nF por 1 Ω, h = 2,5 ms) la R lleva
 * V̄·C·ΔV = ½CV²; con los pesos de las etapas en el producto, 0 (los 25 nJ
 * quedaban en la diferencia del balance y en la entregada: −53 % con 47
 * flancos de τ ≈ h/13). Suma a W (con el segundo puerto, como registra), a
 * la ventana (la parte del subpaso tras t_med0) y a las integrales de los
 * canales hasta la próxima muestra. */
static void rq_suma(const Valores *v, const double *y, double t0, double s)
{
    double w1 = rq_e1 ? (1.0 - RIG_G) * s : 0.0, w2 = rq_e1 ? RIG_G * s : s, f = 0.0;
    int k, c, e1 = rq_e1, dentro;
    if (t0 + s > res_.t_med0) f = t0 >= res_.t_med0 ? 1.0 : (t0 + s - res_.t_med0) / s;
    dentro = f > 0.0;                             /* fuera del bucle, como en registra */
    for (k = 0; k < R_.nel; k++) {
        int s2 = s2_de(&R_.el[k]);
        double sV = w2 * v->V[k], sI = w2 * v->I[k], mV = 0.5 * (rq_V0[k] + v->V[k]), mI = 0.5 * (rq_I0[k] + v->I[k]), P;
        if (e1) { sV += w1 * rq_V[k]; sI += w1 * rq_I[k]; }
        P = mV * sI;
        /* s2·0,5 era ±0,5 exacto */
        if (s2) P += (s2 > 0 ? 0.5 : -0.5) * (rq_V20[k] + v->V2[k]) * (w2 * v->I2[k] + (e1 ? w1 * rq_I2[k] : 0.0));
        res_.W[k] += P;
        if (dentro) {
            iv_V[k] += f * sV;
            iv_V2[k] += f * mV * sV;
            iv_I[k] += f * sI;
            iv_I2[k] += f * mI * sI;
            iv_P[k] += f * P;
        }
    }
    for (c = 0; c < 2; c++) {
        double sy = w2 * y[c] + (rq_e1 ? w1 * rq_y[c] : 0.0);
        rq_s1[c] += sy;
        rq_s2[c] += 0.5 * (rq_y0[c] + y[c]) * sy;
    }
    rq_desde(v->V, v->I, v->V2, v->I2, y);        /* el principio del subpaso siguiente */
    rq_e1 = 0;
    ops_propias += (unsigned long)((dentro ? 20 : 10) * R_.nel + 16);
}

/* Tras la etapa 1 (Euler implícito de γ·s hasta tn + γ·s, con la solución
 * x1), la historia de la etapa 2 (otro Euler implícito de γ·s con la misma
 * matriz, hasta tn + s): v = vn + ((1 − γ)/γ)·(v1 − vn) en cada C e
 * i = in + ((1 − γ)/γ)·(i1 − in) en cada L (vC e iL guardan vn, in) */
static void rig_etapa(const double *x)
{
    int k;
    for (k = 0; k < R_.nel; k++) {
        const SimEl *e = &R_.el[k];
        if (e->tipo == SE_C) vC[k] += RIG_K * (vnodo(x, e->a) - vnodo(x, e->b) - vC[k]);
        else if (e->tipo == SE_L) iL[k] += RIG_K * (x[col_br[k]] - iL[k]);
        else continue;
        ops_propias += 3;
    }
}

/* El subpaso j del tramo (ya resuelto, en M.t, con sus valores v y canales
 * y): los puntos de la y+ (los 4 primeros), el estado de C y L en cada
 * te + k·h/2 (la historia BDF2 de los algebraicos al final), la salida (la
 * de arranque_salida) y, al final del tramo, el trapecio desde te + n·h */
static void rig_acepta(const Valores *v, const double *y, int corta)
{
    static Punto q;
    int j = M.rig_n + 1 - M.rig, k;
    Punto *p = j <= 4 ? &M.b[j - 1] : &q;
    if (j == 1) res_.tramos++;
    p->t = M.t; p->y[0] = y[0]; p->y[1] = y[1];
    memcpy(p->V, v->V, sizeof(double) * (size_t)R_.nel);
    memcpy(p->I, v->I, sizeof(double) * (size_t)R_.nel);
    memcpy(p->V2, v->V2, sizeof(double) * (size_t)R_.nel);
    memcpy(p->I2, v->I2, sizeof(double) * (size_t)R_.nel);
    M.rig--;
    if (!corta && M.rig > 0 && rig_medio(j) > 0) {
        double *d = M.rx[rig_medio(j) & 1];          /* [0]: te + k·h; [1]: te + (k + ½)·h */
        for (k = 0; k < R_.nel; k++)
            d[k] = R_.el[k].tipo == SE_C ? v->V[k] : R_.el[k].tipo == SE_L ? v->I[k] : 0.0;
        ops_propias += (unsigned long)R_.nel;
    }
    if (M.rig == 0 && !corta) {
        historia_bdf2(p);
        /* la historia del error local de los suaves (control_nl): te + (n − 1)·h
         * (con n = 1 no: es la y+), te + (n − ½)·h y te + n·h. Con los tres
         * últimos subpasos (a h/16) la tercera diferencia dividida aumenta su
         * error 256 veces y el trapecio que sigue se parte de más: el diodo C
         * de 100 k a 20 kHz daba 4200 pasos en vez de 3900, y con h = 2 µs a
         * mano y 26 ms acababa en FIN_TOPE */
        M.nh = 0;
        if (M.rig_nh > 1) { M.ht[0] = M.t - M.h; memcpy(M.hx[0], M.rx[0], sizeof M.hx[0]); M.nh = 1; }
        M.ht[M.nh] = M.t - 0.5 * M.h; memcpy(M.hx[M.nh], M.rx[1], sizeof M.hx[0]); M.nh++;
        for (k = 0; k < R_.nel; k++)
            M.hx[M.nh][k] = R_.el[k].tipo == SE_C ? vC[k] : R_.el[k].tipo == SE_L ? iL[k] : 0.0;
        M.ht[M.nh] = M.t; M.nh++;
    }
    arranque_salida(j, corta, p);
    if (corta || M.rig == 0) rq_on = 0;                   /* desde su última muestra, el trapecio */
    if (corta) { M.rig = 0; M.t_ancla = M.t; M.k = 0; }   /* se cortó: sigue desde aquí */
    else if (M.rig == 0) M.k = M.rig_nh;                  /* t = t_ancla + n·h */
}

/* La y+ del tramo rígido: Euler implícitos de ε = RIG_EPS·τmín en te con
 * las fuentes de te⁺. Los C y las L con estado propio se quedan en su valor
 * y lo demás toma lo que vale con ellos en te⁺. Sin impulso, el primero
 * parte del estado s de te⁻ (el de ahora) y el segundo de 2·s − (el estado
 * del primero): así su estado vuelve a s con un error O(ε²) (1e-6 del salto
 * con RIG_EPS = 1e-3, y con ε no tan chico la i de un C lento no pierde
 * cifras: con 1e-6·τmín, un C de τ = 4 ms junto a un modo de 1 ns perdía
 * 4e-4). Con un impulso, el primero reparte la carga (o el flujo) pero sus
 * corrientes son las del impulso (~ΔQ/ε, no una muestra) y además mueve un
 * ε·f los estados que no saltan; el segundo, desde lo repartido, los mueve
 * otro ε·f: el estado de te⁺ es sJ = 2·(el del primero) − (el del segundo)
 * (lo repartido sin los ε·f, O(ε²)), y los dos últimos son los de antes con
 * s = sJ. Con solo tres, un C que no salta quedaba con ε·i/C (1e-3 del
 * salto; 5 mV en un RC de τ = 0,3h con un C sobre la E). La extrapolación de
 * los subpasos no ve un modo que se apaga en el primero (con τ = h/100 daba
 * 1,7 veces el salto). Se registra y su estado es el de te⁺ de la historia
 * de los algebraicos (rx[0]). */
static void rig_ymas_emite(const double *x)
{
    static Valores v;
    static Punto q;
    int k;
    valores_de(x, &v, M.t, 0);
    q.t = M.t; q.y[0] = canal_val(&v, 0); q.y[1] = canal_val(&v, 1);
    memcpy(q.V, v.V, sizeof(double) * (size_t)R_.nel);
    memcpy(q.I, v.I, sizeof(double) * (size_t)R_.nel);
    memcpy(q.V2, v.V2, sizeof(double) * (size_t)R_.nel);
    memcpy(q.I2, v.I2, sizeof(double) * (size_t)R_.nel);
    for (k = 0; k < R_.nel; k++) M.rx[0][k] = R_.el[k].tipo == SE_C ? v.V[k] : R_.el[k].tipo == SE_L ? v.I[k] : 0.0;
    ops_propias += (unsigned long)R_.nel;
    rq_desde(q.V, q.I, q.V2, q.I2, q.y);          /* el primer subpaso empieza en la y+ (rq_suma) */
    emite(&q);
    M.pend = 0;
    M.ymas = 1;
}

/* El estado de partida del ε siguiente (en vC, iL): sin s, el de x (lo que
 * dejó el impulso); con s (sv, si), 2·s − el de x */
static void rig_hist_eps(const double *x, const double *sv, const double *si)
{
    int k;
    for (k = 0; k < R_.nel; k++) {
        const SimEl *e = &R_.el[k];
        if (e->tipo == SE_C) {
            double v = vnodo(x, e->a) - vnodo(x, e->b);
            vC[k] = sv ? 2.0 * sv[k] - v : v;
        } else if (e->tipo == SE_L) iL[k] = si ? 2.0 * si[k] - x[col_br[k]] : x[col_br[k]];
        else continue;
        ops_propias += 3;
    }
}

/* La y+ en la red lineal: una LU más por topología (en la caché) y 2
 * sustituciones (4 con impulso); si no se puede, queda la extrapolación */
static void rig_ymas(void)
{
    static double rhs[LU_MAX], x[LU_MAX], v0[MAX_EL], i0[MAX_EL], sv[MAX_EL], si[MAX_EL];
    double hh = 2.0 * RIG_EPS * res_.tau_min;
    int ic, n, paso;
    diag_foto();
    ic = cache_de(M.mask, tramos_act(), hh, M.t);
    if (ic < 0) { diag_vuelve(); M.ymas = -1; return; }
    for (n = 0; n < R_.nn; n++) if (topo_cache[ic].sust_nodo[n] == SU_ANCLA) topo_cache[ic].ancla[n] = M.ancla_val[n];
    memcpy(v0, vC, sizeof vC); memcpy(i0, iL, sizeof iL);
    for (paso = 0; paso < (M.impulso ? 4 : 2); paso++) {
        if (paso == 1 && M.impulso) rig_hist_eps(x, 0, 0);            /* desde lo repartido */
        else if (paso) {                                               /* sJ y las correcciones */
            memcpy(sv, vC, sizeof vC); memcpy(si, iL, sizeof iL);
            rig_hist_eps(x, sv, si);
        }
        termino(rhs, &topo_cache[ic], 2, M.t, 0, hh, nl_act ? nl_act->tramo : 0, 1.0);
        lu_resuelve(&lu_cache[ic], rhs, x);
    }
    memcpy(vC, v0, sizeof vC); memcpy(iL, i0, sizeof iL);
    rig_ymas_emite(x);
}

/* ---- Impulsos: lo que salta en un evento ---- */
static void suceso(double t, int tipo, int elem, unsigned lazo)
{
    SimSuceso *s;
    avisa(SIM_AV_IMPULSO, elem);
    M.impulso = 1;
    if (res_.nsucesos >= SIM_MAX_SUCESOS) return;
    s = &res_.suceso[res_.nsucesos++];
    s->t = t; s->tipo = (unsigned char)tipo; s->elem = (signed char)elem; s->lazo = lazo;
}

/* Con el estado de te⁻ y las fuentes de te⁺, en la topología nueva:
 *  - capacitivo: un lazo de E ideales, A ideales, S cerrados y C con su
 *    v(te⁻) que no cuadra (lo cierra una C);
 *  - inductivo: un trozo de ramas G y V (C como G) con una L en su corte,
 *    junto a J, S abiertos o V ideales, cuyas corrientes no suman cero. */
static void busca_impulsos(double te, unsigned mask)
{
    static const unsigned char ORDEN[5] = { SE_E, SE_EC, SE_AMI, SE_S, SE_C };
    int k, n, o, pz;
    int pieza[MAX_NODOS];

    M.impulso = 0;
    for (n = 0; n < R_.nn; n++) uf[n] = n;
    bosque_n = 0;
    for (o = 0; o < 5; o++)
        for (k = 0; k < R_.nel; k++) {
            const SimEl *e = &R_.el[k];
            int el[MAX_EL], len, i;
            signed char sg[MAX_EL];
            double res, esc, vk;
            if (e->tipo != ORDEN[o]) continue;
            if (e->tipo == SE_E && e->val != 0.0) continue;
            if (e->tipo == SE_S && !((mask >> k) & 1u)) continue;
            if (uf_raiz(e->a) != uf_raiz(e->b)) {
                uf_une(e->a, e->b);
                bosque_a[bosque_n] = e->a; bosque_b[bosque_n] = e->b; bosque_k[bosque_n] = (short)k;
                bosque_n++;
                continue;
            }
            if (e->tipo != SE_C) continue;           /* los demás lazos ya se revisaron */
            len = camino(e->a, e->b, el, sg);
            for (i = 0; i < len; i++) if (R_.el[el[i]].tipo == SE_EC) break;
            if (i < len) continue;                   /* la tensión de una EC no se sabe aquí */
            vk = vC[k];
            res = vk; esc = 1.0 + fabs(vk);
            for (i = 0; i < len; i++) {
                const SimEl *q = &R_.el[el[i]];
                double vi = q->tipo == SE_C ? vC[el[i]] : q->tipo == SE_E ? -onda_valor_lado(q->forma, q->p, te, 1) : 0.0;
                res -= sg[i] * vi;
                if (fabs(vi) + 1.0 > esc) esc = fabs(vi) + 1.0;
            }
            if (fabs(res) > 1e-9 * esc) {
                unsigned lazo = 1u << k;
                for (i = 0; i < len; i++) lazo |= 1u << el[i];
                suceso(te, SUC_IMPULSO_C, k, lazo);
            }
        }

    /* Trozos por ramas G y V */
    for (n = 0; n < R_.nn; n++) uf[n] = n;
    for (k = 0; k < R_.nel; k++) {
        const SimEl *e = &R_.el[k];
        int une = e->tipo == SE_R || e->tipo == SE_E || e->tipo == SE_C || e->tipo == SE_AMI ||
                  e->tipo == SE_EC || e->tipo == SE_D || e->tipo == SE_BJT || e->tipo == SE_MOS ||
                  (e->tipo == SE_S && ((mask >> k) & 1u));
        /* el BJT, B-E y C-E: una G entre sus tres patas; el MOS, solo D-S (la
         * puerta no conduce) */
        if (e->tipo == SE_BJT) uf_une(e->c, e->b);
        if (e->tipo == SE_AO) {                   /* su salida siempre; las entradas con Ri */
            uf_une(e->nref, e->c);
            une = e->modelo == NL_AO_REAL && e->m[3] > 0.0;
        }
        if (une) uf_une(e->a, e->b);
    }
    for (n = 0; n < R_.nn; n++) pieza[n] = uf_raiz(n);
    for (pz = 0; pz < R_.nn; pz++) {
        int a_tierra = 0, primeraL = -1, desc = 0;
        double neto = 0.0, esc = 1.0;
        unsigned corte = 0;
        if (pieza[pz] != pz) continue;
        for (n = 0; n < R_.nn; n++) if (pieza[n] == pz && R_.ref[n]) a_tierra = 1;
        if (a_tierra) continue;
        for (k = 0; k < R_.nel; k++) {
            const SimEl *e = &R_.el[k];
            int ina = pieza[e->a] == pz, inb = pieza[e->b] == pz;
            double i = 0.0;
            if (ina == inb) continue;
            if (e->tipo == SE_JC) { desc = 1; continue; }  /* su corriente no se sabe aquí */
            if (e->tipo == SE_L) { i = iL[k]; if (primeraL < 0) primeraL = k; }
            else if (e->tipo == SE_J) i = onda_valor_lado(e->forma, e->p, te, 1);
            else if (!(e->tipo == SE_S || e->tipo == SE_VMI)) continue;
            corte |= 1u << k;
            neto += ina ? i : -i;
            if (fabs(i) + 1.0 > esc) esc = fabs(i) + 1.0;
        }
        if (primeraL >= 0 && !desc && fabs(neto) > 1e-9 * esc) suceso(te, SUC_IMPULSO_L, primeraL, corte);
    }
    ops_propias += (unsigned long)(10 * R_.nel);
}

/* Los subpasos del paso con no lineales (§3.G): con suaves, al partirlo */
#define NIVEL_MAX      10        /* subpasos de h/1024 como mucho           */
#define NIVEL_MAX_ARR  6         /* en el arranque, h/8/64                  */

/* El tramo de un evento en t (pasos h; 0: el arranque) y la τ del ε de su
 * y+ (en M.tau_y). El de la red; en una red con clases (§17.3), el de la
 * clase del evento, y la τ de su modo más rápido si es menor. Un cambio de
 * tramo (evento_nl), un evento de una clase que no se miró o una red en que
 * ninguna clase cambia el tramo: el de la red (sin mirar la clase) */
static int rig_de_evento(double t)
{
    unsigned long long k;
    int c;
    M.tau_y = res_.tau_min;
    if (!PS.alguna || M.ev_tramo) return M.rig_h;
    k = clase_de(t);
    for (c = 0; c < PS.n && PS.clave[c] != k; c++) ;
    if (c == PS.n || PS.nm[c] < 0) return M.rig_h;
    if (PS.rig[c] > 0 && PS.tau[c] > 0.0 && !(M.tau_y > 0.0 && M.tau_y <= PS.tau[c])) M.tau_y = PS.tau[c];
    return PS.rig[c];
}

static void aplica_evento(double t)
{
    int n;
    vacia_pendiente();
    /* un evento a media subida de un tramo rígido (un cambio de tramo, o uno
     * a menos de 1e-6·h del subpaso): W ya va hasta ahora con su cuadratura;
     * se registra el punto de ahora (te⁻) para que lo que sigue no lo cuente
     * otra vez con el trapecio desde la última muestra */
    if (rq_on && M.hay_p && M.t > M.pt) {
        static Valores v;
        double y[2];
        valores_de(M.x, &v, M.t, 0);
        y[0] = canal_val(&v, 0); y[1] = canal_val(&v, 1);
        memcpy(r2V, v.V2, sizeof r2V); memcpy(r2I, v.I2, sizeof r2I);
        registra(M.t, v.V, v.I, y);
    }
    rq_s1[0] = rq_s1[1] = rq_s2[0] = rq_s2[1] = 0.0;
    rq_e1 = 0;
    /* anclas de los trozos que flotan: el potencial de ahora (continuidad) */
    for (n = 0; n < R_.nn; n++) M.ancla_val[n] = col_phi[n] >= 0 ? M.x[col_phi[n]] : 0.0;
    M.mask = mask_de(t, 0);
    M.ic = cache_de(M.mask, tramos_act(), M.h, t);
    rig_pon(rig_de_evento(t));                    /* con un modo rígido: el tramo SDIRK2 */
    if (M.ic >= 0) M.ic_arr = cache_de(M.mask, tramos_act(), M.rig_n ? 2.0 * RIG_G * M.h / RIG_SUB : M.h / (0.5 * ARR_PASOS), t);
    M.ic_rig2 = M.ic >= 0 && M.ic_arr >= 0 && M.rig_nh > 1 ? cache_de(M.mask, tramos_act(), 2.0 * RIG_G * M.h / RIG_SUB2, t) : M.ic_arr;
    if (M.ic < 0 || M.ic_arr < 0 || M.ic_rig2 < 0) { M.estado = res_.d.estado; M.hecho = 1; return; }
    for (n = 0; n < R_.nn; n++)
        if (topo_cache[M.ic].sust_nodo[n] == SU_ANCLA) {
            topo_cache[M.ic].ancla[n] = M.ancla_val[n];
            topo_cache[M.ic_arr].ancla[n] = M.ancla_val[n];
            topo_cache[M.ic_rig2].ancla[n] = M.ancla_val[n];
        }
    busca_impulsos(t, M.mask);
    M.t_ancla = t;
    M.k = 0;
    M.ymas = 0;
    M.pend = 1;
    M.en_arr = 1;
    M.t_evento = t;
    M.t_ev = siguiente_evento(t);
    res_.eventos++;
    if (NL.n) {                  /* el primer medio paso decide los tramos; sin historia */
        M.katz = NL.tramos;
        M.nivel = M.pos = M.buenos = 0;
        M.nh = 0;
        M.hay_xa = 0;
        /* en el tramo rígido, los primeros subpasos partidos hasta que su z =
         * s/τ no pase de 1 + √2 (paso_nl_rig): SDIRK2 sin rebote mientras el
         * modo rápido no se apague; luego vuelven a h/16 (un_paso_nl) */
        if (M.rig_n && M.tau_y > 0.0)
            while (M.nivel < NIVEL_MAX_ARR && M.h / RIG_SUB / (double)(1 << M.nivel) > RIG_K * M.tau_y) M.nivel++;
    }
}

/* ---- El paso con no lineales (§3.G) ---- */
static void error_nl(int campo, int elem, double t)
{
    res_.d.estado = SIM_NO_CONVERGE; res_.d.campo = campo; res_.d.elem = elem; res_.d.t = t;
    M.estado = SIM_NO_CONVERGE;
    M.hecho = 1;
}

/* Un Newton del paso de M.t a t (NC del transitorio) */
static int paso_nl(int metodo, double t, int izq, double hh, int suelta, int katz, double *x, int itmax)
{
    NC.modo = NLM_TR; NC.metodo = metodo; NC.t = t; NC.izq = izq; NC.hh = hh;
    NC.mask = M.mask; NC.suelta = suelta; NC.fijo = -1;
    NC.arranque0 = katz && M.t == 0.0;
    suelta_ok = 0;
    return newton(katz, 1.0, 0.0, x, itmax, &M.nl, &res_.nw);
}

/* El menor margen en x de los por tramos marcados en sale (0: todos) */
static double margen_de(const double *x, const unsigned char *sale)
{
    double m = 1e300;
    int q;
    for (q = 0; q < NL.n; q++) {
        int k = NL.el[q];
        const SimEl *e = &R_.el[k];
        double mk;
        if (!es_tramos(e) || (sale && !sale[k])) continue;
        mk = margen_tr(e, M.nl.tramo[k], x, -1);
        if (mk < m) m = mk;
    }
    return m;
}

/* ¿Algún diodo por tramos con la corriente del revés en x (al_reves)? */
static int reves_de(const double *x)
{
    int q;
    for (q = 0; q < NL.n; q++) if (al_reves(&R_.el[NL.el[q]], NL.el[q], M.nl.tramo[NL.el[q]], x)) return 1;
    return 0;
}

/* El cambio de tramo en t (§3.G): salen los que tienen el margen en 0 (el
 * primer medio paso, con Katzenelson, corrige lo que haga falta); Zenón */
static void evento_nl(double t)
{
    int q;
    if (fabs(t - M.t_ev_nl) <= 1e-9 * M.h) {
        if (++M.ev_mismo_t > 4 * MAX_EL) { error_nl(NC_CONMUTA, NL.n ? NL.el[0] : -1, t); return; }
    } else { M.ev_mismo_t = 0; M.t_ev_nl = t; }
    NC.modo = NLM_TR; NC.mask = M.mask; NC.t = t; NC.fijo = -1;
    for (q = 0; q < NL.n; q++) {
        int k = NL.el[q];
        const SimEl *e = &R_.el[k];
        if (e->tipo == SE_AO) {                          /* el AO: su margen o la regla */
            int tau = M.nl.tramo[k];
            if (tau == TR_LIN) {
                if (ao_margen(e, TR_LIN, M.x) <= 1e-9 * NL.Vesc || !lin_vale(&M.nl, k))
                    M.nl.tramo[k] = (unsigned char)ao_satura(e, M.x);
            } else if (ao_margen(e, tau, M.x) <= 1e-9 * NL.Vesc)
                M.nl.tramo[k] = (unsigned char)(lin_vale(&M.nl, k) ? TR_LIN : tau == TR_SATP ? TR_SATN : TR_SATP);
        } else if (e->modelo == NL_TRAMOS) {
            double v[2];
            puertos_v(e, M.x, v);
            if (nl_tr_margen(e, M.nl.tramo[k], v[0]) <= 1e-9 * NL.Vesc)
                M.nl.tramo[k] = (unsigned char)nl_tr_vecino(e, M.nl.tramo[k], v[0]);
        }
    }
    res_.nw.eventos++;
    M.ev_tramo = 1;           /* un cambio de tramo no salta las fuentes: el tramo, el de la red */
    aplica_evento(t);
    M.ev_tramo = 0;
}

/* Acepta el paso a t con la solución x: estados de C y L, valores, canales y
 * registro, como un_paso; los suaves, su historia */
static void acepta_nl(const double *x, double t, int izq, int metodo, int parcial)
{
    static Valores v;
    static Punto q;
    double y[2], tol = 1e-9 * M.h;
    int k, fin;
    for (k = 0; k < R_.nel; k++) {
        const SimEl *e = &R_.el[k];
        double pa, pb;
        if (e->tipo != SE_C && e->tipo != SE_L) continue;
        pa = vnodo(x, e->a); pb = vnodo(x, e->b);
        if (e->tipo == SE_C) { vC[k] = pa - pb; iC[k] = x[col_br[k]]; }
        else { vL[k] = pa - pb; iL[k] = x[col_br[k]]; }
    }
    memcpy(M.xa, M.x, sizeof M.xa); M.ta = M.t; M.hay_xa = 1;
    memcpy(M.x, x, (size_t)nincog * sizeof x[0]);
    M.t = t;
    valores_de(M.x, &v, M.t, izq);
    y[0] = canal_val(&v, 0); y[1] = canal_val(&v, 1);
    memcpy(r2V, v.V2, sizeof r2V); memcpy(r2I, v.I2, sizeof r2I);
    /* la historia del error local (C y L) y la de las corrientes de los suaves */
    if (M.nh == 3) {
        M.ht[0] = M.ht[1]; M.ht[1] = M.ht[2];
        memcpy(M.hx[0], M.hx[1], sizeof M.hx[0]); memcpy(M.hx[1], M.hx[2], sizeof M.hx[0]);
        M.nh = 2;
    }
    M.ht[M.nh] = t;
    for (k = 0; k < R_.nel; k++)
        M.hx[M.nh][k] = R_.el[k].tipo == SE_C ? vC[k] : R_.el[k].tipo == SE_L ? iL[k] : 0.0;
    M.nh++;
    for (k = 0; k < R_.nel; k++)
        if (es_suave(&R_.el[k])) {
            M.inl[k] = v.I[k];
            if (fabs(v.I[k]) > M.imax_nl[k]) M.imax_nl[k] = fabs(v.I[k]);
        }
    if (parcial) { M.pos = 0; fin = 1; }
    else { M.pos += 1024 >> M.nivel; fin = M.pos >= 1024; if (fin) M.pos = 0; }
    if (metodo == 2 && M.rig > 0) {                  /* el tramo rígido: su cuadratura y el final de un subpaso */
        M.rig_amp *= M.rig_fac;
        if (rq_on) rq_suma(&v, y, M.ta, M.t - M.ta);
        rq_e1 = 0;
        if (fin) rig_acepta(&v, y, parcial || fabs(M.t - M.t_ev) <= tol || M.t >= M.t_fin - tol);
    } else if (metodo == 2) {
        if (fin) {                                   /* el final de un medio paso de h/8 */
            int j = ARR_PASOS + 1 - M.arranque, corta;
            Punto *p = j <= 4 ? &M.b[j - 1] : &q;
            p->t = M.t; p->y[0] = y[0]; p->y[1] = y[1];
            memcpy(p->V, v.V, sizeof(double) * (size_t)R_.nel);
            memcpy(p->I, v.I, sizeof(double) * (size_t)R_.nel);
            memcpy(p->V2, v.V2, sizeof(double) * (size_t)R_.nel);
            memcpy(p->I2, v.I2, sizeof(double) * (size_t)R_.nel);
            M.arranque--;
            corta = parcial || fabs(M.t - M.t_ev) <= tol || M.t >= M.t_fin - tol;
            if (M.arranque == 0 && !corta) historia_bdf2(p);
            arranque_salida(j, corta, p);
            if (corta) { M.arranque = 0; M.t_ancla = M.t; M.k = 0; }
            else if (M.arranque == 0) M.k = 1;
        }
    } else {
        if (parcial) { M.t_ancla = M.t; M.k = 0; }
        else if (fin) M.k++;
        registra(M.t, v.V, v.I, y);
    }
    res_.pasos++;
}

/* Los suaves en un subpaso del trapecio (§3.G.3): 1 = se rechaza (el error
 * local de un C o una L pasa de 1e-4 de su escala, o la corriente de un suave
 * cambia más del 10 % de su escala); 2 = vale y va holgado (LTE < tol/16 y
 * Δi < 2,5 %); 0 = vale */
static int control_nl(const double *x, double t, double hh)
{
    int k, bueno = 1;
    for (k = 0; k < R_.nel; k++) {
        const SimEl *e = &R_.el[k];
        double i1, di, esc;
        if (!es_suave(e) || (e->tipo != SE_D && e->tipo != SE_BJT && e->tipo != SE_MOS)) continue;
        if (e->tipo == SE_BJT || e->tipo == SE_MOS) {  /* IC o ID, la I de su tabla */
            double vp[2], ip[2];
            puertos_v(e, x, vp);
            i_puertos(&M.nl.lin[k], vp, ip);
            i1 = ip[1];
        } else i1 = i_union(&M.nl.lin[k], vnodo(x, e->a) - vnodo(x, e->b));
        di = fabs(i1 - M.inl[k]);
        esc = M.imax_nl[k] > 1e-3 * NL.Iesc ? M.imax_nl[k] : 1e-3 * NL.Iesc;
        if (fabs(i1) > esc) esc = fabs(i1);
        if (di > 0.1 * esc) return 1;
        if (di > 0.025 * esc) bueno = 0;
    }
    if (M.nh >= 3)
        for (k = 0; k < R_.nel; k++) {
            const SimEl *e = &R_.el[k];
            double f3, f01, f12, f23, f012, f123, xn, lte, tol;
            if (e->tipo != SE_C && e->tipo != SE_L) continue;
            xn = e->tipo == SE_C ? vnodo(x, e->a) - vnodo(x, e->b) : x[col_br[k]];
            f01 = (M.hx[1][k] - M.hx[0][k]) / (M.ht[1] - M.ht[0]);
            f12 = (M.hx[2][k] - M.hx[1][k]) / (M.ht[2] - M.ht[1]);
            f23 = (xn - M.hx[2][k]) / (t - M.ht[2]);
            f012 = (f12 - f01) / (M.ht[2] - M.ht[0]);
            f123 = (f23 - f12) / (t - M.ht[1]);
            f3 = (f123 - f012) / (t - M.ht[0]);
            lte = hh * hh * hh / 12.0 * 6.0 * fabs(f3);
            tol = 1e-4 * (fabs(xn) > (e->tipo == SE_C ? NL.Vesc : NL.Iesc) ? fabs(xn) : (e->tipo == SE_C ? NL.Vesc : NL.Iesc));
            ops_propias += 20;
            if (lte > tol) return 1;
            if (lte > tol / 16.0) bueno = 0;
        }
    return bueno ? 2 : 0;
}

/* Localiza el cambio de tramo entre M.t y t_obj (Illinois, §3.G.2), acepta
 * el paso partido hasta t* y hace el evento. El estado de Newton de antes
 * del paso (bak) se repone como se guardó (nl_copia: lo de NL.el y frio) */
static void nl_copia(NlEst *a, const NlEst *b);
static int localiza_nl(int metodo, double t_obj, const double *x_obj, const NlEst *bak)
{
    static double xc[LU_MAX];
    static unsigned char sale[MAX_EL];
    double ta = M.t, tb = t_obj, fa, fb, tc = t_obj, fc = 0.0;
    int it, lado = 0, r, q;
    memset(sale, 0, sizeof sale);
    for (q = 0; q < NL.n; q++) {
        int k = NL.el[q];
        const SimEl *e = &R_.el[k];
        if (es_tramos(e) && (margen_tr(e, M.nl.tramo[k], x_obj, -1) < -1e-9 * NL.Vesc ||
                             al_reves(e, k, M.nl.tramo[k], x_obj))) sale[k] = 1;
    }
    nl_copia(&M.nl, bak);
    fa = margen_de(M.x, sale);
    fb = margen_de(x_obj, sale);
    if (fa <= 1e-9 * NL.Vesc) { evento_nl(M.t); return M.estado == SIM_OK; }     /* el cambio es ya */
    for (it = 0; it < 30; it++) {
        tc = tb - fb * (tb - ta) / (fb - fa);
        memcpy(xc, M.x, (size_t)nincog * sizeof xc[0]);
        nl_copia(&M.nl, bak);
        r = paso_nl(metodo, tc, 0, (metodo == 2 ? 2.0 : 1.0) * (tc - M.t), 1, 0, xc, NL.suave ? 12 : 1);
        if (r == NLR_NOCONV && NL.suave) {
            /* Antes de dar el error, 50 iteraciones (como en un_paso_nl): cerca
             * del cambio el nulador pide a una unión que conduce que se corte
             * (el superdiodo al cruzar por cero) y la unión baja desde su
             * codo a pasos de n·VT. Solo aquí: lo que convergía sigue igual. */
            memcpy(xc, M.x, (size_t)nincog * sizeof xc[0]);
            nl_copia(&M.nl, bak);
            r = paso_nl(metodo, tc, 0, (metodo == 2 ? 2.0 : 1.0) * (tc - M.t), 1, 0, xc, 50);
        }
        if (r != NLR_OK) {
            if (res_.d.estado != SIM_OK) { M.estado = res_.d.estado; M.hecho = 1; }
            else error_nl(NC_TR, nl_culpable >= 0 ? nl_culpable : NL.el[0], tc);
            return 0;
        }
        fc = margen_de(xc, sale);
        if (fabs(fc) < 1e-12 * NL.Vesc || tb - ta < 1e-12 * M.h) break;
        if (fc < 0.0) { tb = tc; fb = fc; if (lado < 0) fa /= 2.0; lado = -1; }
        else { ta = tc; fa = fc; if (lado > 0) fb /= 2.0; lado = 1; }
    }
    rq_e1 = 0;                                    /* en el tramo rígido: un Euler, su final */
    acepta_nl(xc, tc, 1, metodo, 1);
    evento_nl(tc);
    return M.estado == SIM_OK;
}

/* Un subpaso SDIRK2 del tramo rígido con Newton: la etapa 1 hasta
 * M.t + γ·(t − M.t) y la 2 hasta t, las dos Euler implícitos (metodo 2) con
 * el mismo hh = 2γ·(t − M.t). Devuelve lo de paso_nl; en x, la solución en
 * t o, si la etapa 1 falla o sale de su tramo, la suya (en *tx, su
 * instante). El estado de C y L queda como estaba (acepta_nl lo toma de x).
 *
 * Con z = s/τ > 1 + √2 (s el subpaso, τ la del modo más rápido del evento)
 * la segunda etapa parte de una historia más allá de la asíntota del modo
 * (vn + (1 + √2)·(v1 − vn)): en lo lineal eso es R(z) < 0, un rebote que los
 * subpasos siguientes apagan; con un diodo que carga un C, el C queda
 * sobrecargado, el diodo se corta y el rebote se queda (con τ = h/70, 0,24
 * de la escala en te + h/2 y 4e-2 tras cada flanco). Por eso los primeros
 * subpasos van partidos (aplica_evento; un_paso_nl los vuelve a juntar
 * cuando el modo se apaga) hasta que z ≤ 1 + √2; si ni con h/16/64 se llega,
 * mientras el modo no se haya apagado (M.rig_amp > RIG_AMP) cada trozo es un
 * Euler implícito entero (monótono: 1/(1 + z), sin rebote). En M.rig_fac, lo
 * que apaga el modo este subpaso */
#define RIG_AMP 1e-4
static int paso_nl_rig(double t, int izq, double hh, int suelta, int katz, double *x, int itmax, double *tx)
{
    static double v0[MAX_EL], i0[MAX_EL];
    double z = M.tau_y > 0.0 ? hh / (2.0 * RIG_G) / M.tau_y : 0.0;
    int r;
    if (z > RIG_K && M.rig_amp > RIG_AMP) {       /* el Euler implícito del subpaso entero */
        M.rig_fac = 1.0 / (1.0 + z);
        rq_e1 = 0;
        *tx = t;
        return paso_nl(2, t, izq, hh / RIG_G, suelta, katz, x, itmax);
    }
    M.rig_fac = fabs(1.0 - (1.0 - 2.0 * RIG_G) * z) / ((1.0 + RIG_G * z) * (1.0 + RIG_G * z));   /* |R(−z)| */
    memcpy(v0, vC, sizeof v0); memcpy(i0, iL, sizeof i0);
    rq_e1 = 0;
    *tx = M.t + RIG_G * (t - M.t);
    r = paso_nl(2, *tx, 0, hh, suelta, katz, x, itmax);
    if (r == NLR_OK && (katz || !NL.tramos || margen_de(x, 0) >= -1e-9 * NL.Vesc)) {
        if (rq_on) rq_etapa1(x, *tx);             /* con su linealización (la de x), para la cuadratura */
        rig_etapa(x);
        *tx = t;
        r = paso_nl(2, t, izq, hh, suelta, katz, x, itmax);
    }
    memcpy(vC, v0, sizeof v0); memcpy(iL, i0, sizeof i0);
    return r;
}

/* La y+ del tramo rígido con no lineales: el Newton del Euler implícito de
 * ε (con Katzenelson si toca), sin aceptarlo; si no converge, queda la
 * extrapolación */
static void rig_ymas_nl(void)
{
    static double x[LU_MAX], v0[MAX_EL], i0[MAX_EL], sv[MAX_EL], si[MAX_EL];
    static NlEst bak;
    int r = NLR_OK, paso;
    bak = M.nl;
    diag_foto();
    memcpy(x, M.x, (size_t)nincog * sizeof x[0]);
    memcpy(v0, vC, sizeof vC); memcpy(i0, iL, sizeof iL);
    for (paso = 0; paso < (M.impulso ? 4 : 2) && r == NLR_OK; paso++) {   /* como en rig_ymas */
        if (paso == 1 && M.impulso) rig_hist_eps(x, 0, 0);
        else if (paso) {
            memcpy(sv, vC, sizeof vC); memcpy(si, iL, sizeof iL);
            rig_hist_eps(x, sv, si);
        }
        r = paso_nl(2, M.t, 0, 2.0 * RIG_EPS * M.tau_y, 1, M.katz, x, 50);   /* τ: la de su clase si es menor */
    }
    memcpy(vC, v0, sizeof vC); memcpy(iL, i0, sizeof iL);
    if (r == NLR_OK) rig_ymas_emite(x);
    else { diag_vuelve(); M.ymas = -1; }
    M.nl = bak;
}

/* El estado de Newton de antes del paso (un_paso_nl), para volver a él. Se
 * guarda y se repone solo lo de los no lineales (NL.el) y frio: lo demás,
 * aquí y en M.nl, está a 0 (el memset de sim_iniciar_red y el de
 * nl_prepara) y nadie lo escribe durante la emulación. Se repone siempre
 * como se guardó (también localiza_nl): con una llamada de fuera a mitad de
 * una emulación que cambie NL.el (sim_op_red de otra red), guardar poco y
 * reponerlo todo daba otra emulación que la copia entera (la revisión de
 * motor). En el PC, sim_bak_entero(1) hace la copia entera, la de antes,
 * para compararlas */
static NlEst paso_bak_;
#ifndef NDLESS
static int bak_entero_;
void sim_bak_entero(int si) { bak_entero_ = si != 0; }
#endif
static void nl_copia(NlEst *a, const NlEst *b)
{
    int q;
#ifndef NDLESS
    if (bak_entero_) { *a = *b; return; }
#endif
    for (q = 0; q < NL.n; q++) {
        int k = NL.el[q];
        a->tramo[k] = b->tramo[k];
        a->vlim[k][0] = b->vlim[k][0]; a->vlim[k][1] = b->vlim[k][1];
        a->lin[k] = b->lin[k];
    }
    a->frio = b->frio;
}

static int un_paso_nl(void)
{
    static double x[LU_MAX], x0[LU_MAX];
    double t_obj, hh, tol = 1e-9 * M.h, paso, tx;
    int metodo, izq = 0, parcial = 0, r, katz = M.katz, itmax, i, ctl = 0, rig;

    rig = M.rig > 0;
    metodo = M.arranque > 0 || rig ? 2 : 1;       /* el tramo rígido: SDIRK2, dos Euler implícitos */
    paso = rig ? M.h / (M.rig_n - M.rig < RIG_SUB ? RIG_SUB : RIG_SUB2) : metodo == 2 ? M.h / ARR_PASOS : M.h;
    if (rig) t_obj = rig_t(M.rig_n - M.rig, (double)(M.pos + (1024 >> M.nivel)) / 1024.0);
    else t_obj = M.t_ancla + ((double)(metodo == 2 ? ARR_PASOS - M.arranque : M.k) +
                              (double)(M.pos + (1024 >> M.nivel)) / 1024.0) * paso;
    hh = (rig ? 2.0 * RIG_G : metodo == 2 ? 2.0 : 1.0) * paso / (double)(1 << M.nivel);

    if (M.t_ev < t_obj - tol) {                   /* un evento cae antes */
        if (M.t_ev - M.t < 1e-6 * M.h) { M.t = M.t_ev; aplica_evento(M.t); return M.estado == SIM_OK; }
        t_obj = M.t_ev; parcial = 1; izq = 1;
    } else if (M.t_ev - t_obj <= tol + 1e-9 * fabs(M.t_ev)) { t_obj = M.t_ev; izq = 1; }
    if (t_obj > M.t_fin + tol) { t_obj = M.t_fin; parcial = 1; izq = 0; }
    else if (fabs(t_obj - M.t_fin) <= tol) t_obj = M.t_fin;
    if (parcial) hh = (rig ? 2.0 * RIG_G : metodo == 2 ? 2.0 : 1.0) * (t_obj - M.t);

    /* con los tramos congelados, un AO en su lineal que ya no vale con la
     * matriz de paso h (§3.F): es un evento ahora (el astable sin V0) */
    if (!katz) {
        int q;
        NC.modo = NLM_TR; NC.mask = M.mask; NC.t = M.t; NC.fijo = -1;
        for (q = 0; q < NL.n; q++) {
            int k = NL.el[q];
            if (R_.el[k].tipo == SE_AO && M.nl.tramo[k] == TR_LIN && !lin_vale(&M.nl, k)) {
                evento_nl(M.t);
                return M.estado == SIM_OK;
            }
        }
    }
    if (rig && M.pend && M.ymas == 0 && M.rig == M.rig_n && M.pos == 0) rig_ymas_nl();   /* la y+ */
    nl_copia(&paso_bak_, &M.nl);
    /* de dónde arranca Newton: tras un evento, de ahora; si no, el predictor lineal */
    if (!katz && M.hay_xa && M.t > M.ta) {
        double q = (t_obj - M.t) / (M.t - M.ta);  /* una vez: gcc no la saca (x[i] podría ser M.t) */
        for (i = 0; i < nincog; i++) x[i] = M.x[i] + q * (M.x[i] - M.xa[i]);
    } else memcpy(x, M.x, (size_t)nincog * sizeof x[0]);
    itmax = katz ? 50 : NL.suave ? (NL.it_tr > 0 ? NL.it_tr : 12) : 1;
    memcpy(x0, x, (size_t)nincog * sizeof x[0]);
    tx = t_obj;
    r = rig ? paso_nl_rig(t_obj, izq, hh, parcial, katz, x, itmax, &tx) : paso_nl(metodo, t_obj, izq, hh, parcial, katz, x, itmax);
    if (r == NLR_NOCONV && NL.suave && itmax < 50 &&
        (parcial || M.nivel >= (metodo == 2 ? NIVEL_MAX_ARR : NIVEL_MAX))) {
        /* Antes de dar el error, 50 iteraciones: tras un flanco la unión de
         * un LED azul o blanco (Is ~ 1e-18, n = 3) sube desde el corte con
         * los pasos cortos de pnjlim, y partir el paso no ayuda (la fuente
         * ya saltó entera). Solo aquí: lo que convergía sigue igual. */
        nl_copia(&M.nl, &paso_bak_);
        memcpy(x, x0, (size_t)nincog * sizeof x[0]);
        r = rig ? paso_nl_rig(t_obj, izq, hh, parcial, katz, x, 50, &tx) : paso_nl(metodo, t_obj, izq, hh, parcial, katz, x, 50);
    }
    if (r == NLR_CICLA) { error_nl(NC_CONMUTA, nl_culpable, M.t); return 0; }    /* el instante del cambio */
    if (r != NLR_OK) {
        nl_copia(&M.nl, &paso_bak_);
        if (r == NLR_NOCONV && NL.suave && !parcial && M.nivel < (metodo == 2 ? NIVEL_MAX_ARR : NIVEL_MAX)) {
            M.nivel++; res_.nw.sub++; M.buenos = 0;
            return 1;
        }
        if (res_.d.estado != SIM_OK) { M.estado = res_.d.estado; M.hecho = 1; return 0; }
        if (r == NLR_SINGULAR) { anota_diag(SIM_SINGULAR, -1, t_obj); M.estado = SIM_SINGULAR; M.hecho = 1; return 0; }
        error_nl(NC_TR, nl_culpable >= 0 ? nl_culpable : NL.el[0], t_obj);
        return 0;
    }
    /* un cambio de tramo con los tramos congelados: se localiza */
    if (!katz && NL.tramos && (margen_de(x, 0) < -1e-9 * NL.Vesc || reves_de(x)))
        return localiza_nl(metodo, tx, x, &paso_bak_);
    if (katz) {                                          /* el AO que salió por la regla en 0+ */
        if (avisos & SIM_AV_ARRANQUE) res_.d.avisos |= SIM_AV_ARRANQUE;
    }
    M.katz = 0;
    /* los suaves: el error local y Δi (en el trapecio) */
    if (NL.suave && metodo == 1 && !parcial) {
        ctl = control_nl(x, t_obj, hh);
        if (ctl == 1 && M.nivel < NIVEL_MAX) { nl_copia(&M.nl, &paso_bak_); M.nivel++; res_.nw.sub++; M.buenos = 0; return 1; }
    }
    acepta_nl(x, t_obj, izq, metodo, parcial);
    if (NL.suave && metodo == 1) {                /* sube el nivel si va holgado */
        if (ctl == 2) M.buenos++; else M.buenos = 0;
        if (M.buenos >= 2 && M.nivel > 0 && M.pos % (2 * (1024 >> M.nivel)) == 0) { M.nivel--; M.buenos = 0; }
    } else if (rig && M.rig > 0 && M.rig_amp <= RIG_AMP && M.nivel > 0 && M.pos % (2 * (1024 >> M.nivel)) == 0)
        M.nivel--;                                /* el tramo, con el modo rápido ya apagado */
    if (fabs(M.t - M.t_ev) <= tol && M.t < M.t_fin - tol) aplica_evento(M.t);
    if (M.t >= M.t_fin - tol) M.hecho = 1;
    return M.estado == SIM_OK;
}

/* Un subpaso del tramo rígido (lineal): SDIRK2 de M.t a t_obj, sus dos
 * etapas con la misma LU (la de la caché, o una suelta si se parte para caer
 * en un evento o en el final) */
static int paso_rig(void)
{
    static double rhs[LU_MAX];
    static Valores v;
    double s, t_obj, hh, tol = 1e-9 * M.h, y[2];
    int izq = 0, parcial = 0, k, j = M.rig_n - M.rig, ic = j < RIG_SUB ? M.ic_arr : M.ic_rig2;   /* j: los hechos */
    LU *f;
    Topo *tp;

    s = M.h / (j < RIG_SUB ? RIG_SUB : RIG_SUB2);
    t_obj = rig_t(j, 1.0);
    if (M.t_ev < t_obj - tol) {                   /* un evento cae antes (como en un_paso) */
        if (M.t_ev - M.t < 1e-6 * M.h) { M.t = M.t_ev; aplica_evento(M.t); return M.estado == SIM_OK; }
        t_obj = M.t_ev; parcial = 1; izq = 1;
    } else if (M.t_ev - t_obj <= tol + 1e-9 * fabs(M.t_ev)) { t_obj = M.t_ev; izq = 1; }
    if (t_obj > M.t_fin + tol) { t_obj = M.t_fin; parcial = 1; izq = 0; }
    else if (fabs(t_obj - M.t_fin) <= tol) t_obj = M.t_fin;
    if (M.pend && M.ymas == 0 && j == 0) rig_ymas();     /* la y+, antes del primer subpaso */

    hh = 2.0 * RIG_G * (parcial ? t_obj - M.t : s);
    if (parcial) {
        if (!prepara_lu(&lu_suelta, &topo_suelta, M.mask, hh, M.t, 0, 0, 0)) { M.estado = res_.d.estado; return 0; }
        res_.factorizaciones++;
        for (k = 0; k < R_.nn; k++)
            if (topo_suelta.sust_nodo[k] == SU_ANCLA) topo_suelta.ancla[k] = M.ancla_val[k];
        f = &lu_suelta; tp = &topo_suelta;
    } else { f = &lu_cache[ic]; tp = &topo_cache[ic]; }

    /* etapa 1: Euler implícito de γ·(t_obj − M.t); etapa 2: otro, desde su historia */
    termino(rhs, tp, 2, M.t + RIG_G * (t_obj - M.t), 0, hh, nl_act ? nl_act->tramo : 0, 1.0);
    lu_resuelve(f, rhs, M.x);
    if (rq_on) rq_etapa1(M.x, M.t + RIG_G * (t_obj - M.t));    /* para la cuadratura */
    rig_etapa(M.x);
    termino(rhs, tp, 2, t_obj, izq, hh, nl_act ? nl_act->tramo : 0, 1.0);
    lu_resuelve(f, rhs, M.x);

    for (k = 0; k < R_.nel; k++) {                /* el estado y la historia, como en un_paso */
        const SimEl *e = &R_.el[k];
        double pa, pb;
        if (e->tipo != SE_C && e->tipo != SE_L) continue;
        pa = col_phi[e->a] >= 0 ? M.x[col_phi[e->a]] : 0.0;
        pb = col_phi[e->b] >= 0 ? M.x[col_phi[e->b]] : 0.0;
        if (e->tipo == SE_C) { vC[k] = pa - pb; iC[k] = M.x[col_br[k]]; }
        else { vL[k] = pa - pb; iL[k] = M.x[col_br[k]]; }
    }
    valores_de(M.x, &v, t_obj, izq);
    y[0] = canal_val(&v, 0); y[1] = canal_val(&v, 1);
    if (rq_on) rq_suma(&v, y, M.t, t_obj - M.t);
    rq_e1 = 0;
    M.t = t_obj;
    rig_acepta(&v, y, parcial || fabs(M.t - M.t_ev) <= tol || M.t >= M.t_fin - tol);
    res_.pasos++;

    if (fabs(M.t - M.t_ev) <= tol && M.t < M.t_fin - tol) aplica_evento(M.t);
    if (M.t >= M.t_fin - tol) M.hecho = 1;
    return M.estado == SIM_OK;
}

/* Un paso: medio paso de Euler implícito (arranque), paso del trapecio, o
 * un paso partido para caer justo en un evento o en el final. 0 si hubo un
 * error. */
static int un_paso(void)
{
    static double rhs[LU_MAX];
    static Valores v;
    double t_obj, hh = M.h, tol = 1e-9 * M.h, y[2];
    int metodo, izq = 0, parcial = 0, k;
    LU *f;
    Topo *tp;

    if (NL.n) return un_paso_nl();               /* con no lineales (§3.G) */
    if (M.rig > 0) return paso_rig();            /* el tramo rígido tras un evento */
    if (M.arranque > 0) { metodo = 2; t_obj = M.t_ancla + (double)(ARR_PASOS + 1 - M.arranque) * M.h / ARR_PASOS; hh = M.h / (0.5 * ARR_PASOS); }
    else { metodo = 1; t_obj = M.t_ancla + (double)(M.k + 1) * M.h; }

    if (M.t_ev < t_obj - tol) {                   /* un evento cae antes */
        if (M.t_ev - M.t < 1e-6 * M.h) {
            /* Casi en el instante de ahora: se toma aquí, adelantando el
             * tiempo hasta el evento (un error de menos de 1e-6·h). Sin
             * adelantarlo, el mismo evento volvería una y otra vez. */
            M.t = M.t_ev;
            aplica_evento(M.t);
            return M.estado == SIM_OK;
        }
        t_obj = M.t_ev; parcial = 1; izq = 1;
    } else if (M.t_ev - t_obj <= tol + 1e-9 * fabs(M.t_ev)) { t_obj = M.t_ev; izq = 1; }   /* la tolerancia de ondas.c */
    if (t_obj > M.t_fin + tol) { t_obj = M.t_fin; parcial = 1; izq = 0; }
    else if (fabs(t_obj - M.t_fin) <= tol) t_obj = M.t_fin;

    if (parcial) {                                /* paso a medida: factorización suelta */
        double dt = t_obj - M.t;
        hh = metodo == 2 ? 2.0 * dt : dt;
        if (!prepara_lu(&lu_suelta, &topo_suelta, M.mask, hh, M.t, 0, 0, 0)) { M.estado = res_.d.estado; return 0; }
        res_.factorizaciones++;
        for (k = 0; k < R_.nn; k++)
            if (topo_suelta.sust_nodo[k] == SU_ANCLA) topo_suelta.ancla[k] = M.ancla_val[k];
        f = &lu_suelta; tp = &topo_suelta;
    } else if (metodo == 2) { f = &lu_cache[M.ic_arr]; tp = &topo_cache[M.ic_arr]; }
    else { f = &lu_cache[M.ic]; tp = &topo_cache[M.ic]; }

    termino(rhs, tp, metodo, t_obj, izq, hh, nl_act ? nl_act->tramo : 0, 1.0);
    lu_resuelve(f, rhs, M.x);

    /* Nuevo estado; la historia del trapecio es la i del C y la v de la L */
    for (k = 0; k < R_.nel; k++) {
        const SimEl *e = &R_.el[k];
        double pa, pb;
        if (e->tipo != SE_C && e->tipo != SE_L) continue;
        pa = col_phi[e->a] >= 0 ? M.x[col_phi[e->a]] : 0.0;
        pb = col_phi[e->b] >= 0 ? M.x[col_phi[e->b]] : 0.0;
        if (e->tipo == SE_C) { vC[k] = pa - pb; iC[k] = M.x[col_br[k]]; }
        else { vL[k] = pa - pb; iL[k] = M.x[col_br[k]]; }
    }
    M.t = t_obj;
    valores_de(M.x, &v, M.t, izq);
    y[0] = canal_val(&v, 0); y[1] = canal_val(&v, 1);

    if (metodo == 2) {
        static Punto q;
        int j = ARR_PASOS + 1 - M.arranque, corta;
        Punto *p = j <= 4 ? &M.b[j - 1] : &q;
        p->t = M.t; p->y[0] = y[0]; p->y[1] = y[1];
        memcpy(p->V, v.V, sizeof(double) * (size_t)R_.nel);
        memcpy(p->I, v.I, sizeof(double) * (size_t)R_.nel);
        M.arranque--;
        corta = parcial || fabs(M.t - M.t_ev) <= tol || M.t >= M.t_fin - tol;
        if (M.arranque == 0 && !corta) historia_bdf2(p);
        arranque_salida(j, corta, p);
        if (corta) { M.arranque = 0; M.t_ancla = M.t; M.k = 0; }   /* se cortó: sigue desde aquí */
        else if (M.arranque == 0) M.k = 1;                          /* t = t_ancla + h */
    } else {
        if (parcial) { M.t_ancla = M.t; M.k = 0; } else M.k++;
        registra(M.t, v.V, v.I, y);
    }
    res_.pasos++;

    if (fabs(M.t - M.t_ev) <= tol && M.t < M.t_fin - tol) aplica_evento(M.t);
    if (M.t >= M.t_fin - tol) M.hecho = 1;
    return M.estado == SIM_OK;
}

/* ==========================================================================
 *  6. Modos, paso y ventana
 * ======================================================================= */
/* Valores propios de una matriz real n x n (Hessenberg + QR de Francis,
 * elmhes + hqr de Numerical Recipes). wr, wi: partes real e imaginaria.
 * Devuelve 0 si no convergió. */
#define MOD_MAX 32
static int valores_propios(double a[MOD_MAX][MOD_MAX], int n, double *wr, double *wi)
{
    int m, j, i, nn, its, l, mmin;
    double z = 0.0, y, x, w, v, u, t, s, r = 0.0, q = 0.0, p = 0.0, anorm = 0.0;

    /* Reducción a Hessenberg por eliminación con pivoteo */
    for (m = 1; m < n - 1; m++) {
        x = 0.0; i = m;
        for (j = m; j < n; j++) if (fabs(a[j][m - 1]) > fabs(x)) { x = a[j][m - 1]; i = j; }
        if (i != m) {
            for (j = m - 1; j < n; j++) { t = a[i][j]; a[i][j] = a[m][j]; a[m][j] = t; }
            for (j = 0; j < n; j++) { t = a[j][i]; a[j][i] = a[j][m]; a[j][m] = t; }
        }
        if (x != 0.0)
            for (i = m + 1; i < n; i++) {
                y = a[i][m - 1];
                if (y != 0.0) {
                    y /= x; a[i][m - 1] = y;
                    for (j = m; j < n; j++) a[i][j] -= y * a[m][j];
                    for (j = 0; j < n; j++) a[j][m] += y * a[j][i];
                }
            }
    }
    ops_propias += (unsigned long)(4 * n * n * n);

    for (i = 0; i < n; i++)
        for (j = (i > 0 ? i - 1 : 0); j < n; j++) anorm += fabs(a[i][j]);
    nn = n - 1;
    t = 0.0;
    while (nn >= 0) {
        its = 0;
        do {
            for (l = nn; l >= 1; l--) {
                s = fabs(a[l - 1][l - 1]) + fabs(a[l][l]);
                if (s == 0.0) s = anorm;
                if (fabs(a[l][l - 1]) + s == s) { a[l][l - 1] = 0.0; break; }
            }
            x = a[nn][nn];
            if (l == nn) { wr[nn] = x + t; wi[nn--] = 0.0; }
            else {
                y = a[nn - 1][nn - 1];
                w = a[nn][nn - 1] * a[nn - 1][nn];
                if (l == nn - 1) {
                    p = 0.5 * (y - x); q = p * p + w; z = sqrt(fabs(q)); x += t;
                    if (q >= 0.0) {
                        z = p + (p >= 0.0 ? fabs(z) : -fabs(z));
                        wr[nn - 1] = wr[nn] = x + z;
                        if (z != 0.0) wr[nn] = x - w / z;
                        wi[nn - 1] = wi[nn] = 0.0;
                    } else {
                        wr[nn - 1] = wr[nn] = x + p;
                        wi[nn - 1] = -(wi[nn] = z);
                    }
                    nn -= 2;
                } else {
                    if (its == 60) return 0;
                    if (its == 10 || its == 20) {
                        t += x;
                        for (i = 0; i <= nn; i++) a[i][i] -= x;
                        s = fabs(a[nn][nn - 1]) + fabs(a[nn - 1][nn - 2]);
                        y = x = 0.75 * s;
                        w = -0.4375 * s * s;
                    }
                    ++its;
                    for (m = nn - 2; m >= l; m--) {
                        z = a[m][m]; r = x - z; s = y - z;
                        p = (r * s - w) / a[m + 1][m] + a[m][m + 1];
                        q = a[m + 1][m + 1] - z - r - s;
                        r = a[m + 2][m + 1];
                        s = fabs(p) + fabs(q) + fabs(r);
                        p /= s; q /= s; r /= s;
                        if (m == l) break;
                        u = fabs(a[m][m - 1]) * (fabs(q) + fabs(r));
                        v = fabs(p) * (fabs(a[m - 1][m - 1]) + fabs(z) + fabs(a[m + 1][m + 1]));
                        if (u + v == v) break;
                    }
                    for (i = m; i <= nn - 2; i++) {
                        a[i + 2][i] = 0.0;
                        if (i != m) a[i + 2][i - 1] = 0.0;
                    }
                    for (j = m; j <= nn - 1; j++) {
                        if (j != m) {
                            p = a[j][j - 1]; q = a[j + 1][j - 1]; r = 0.0;
                            if (j != nn - 1) r = a[j + 2][j - 1];
                            if ((x = fabs(p) + fabs(q) + fabs(r)) != 0.0) { p /= x; q /= x; r /= x; }
                        }
                        s = sqrt(p * p + q * q + r * r);
                        if (p < 0.0) s = -s;
                        if (s != 0.0) {
                            if (j == m) { if (l != m) a[j][j - 1] = -a[j][j - 1]; }
                            else a[j][j - 1] = -s * x;
                            p += s; x = p / s; y = q / s; z = r / s; q /= p; r /= p;
                            for (i = j; i <= nn; i++) {
                                p = a[j][i] + q * a[j + 1][i];
                                if (j != nn - 1) { p += r * a[j + 2][i]; a[j + 2][i] -= p * z; }
                                a[j + 1][i] -= p * y; a[j][i] -= p * x;
                            }
                            mmin = nn < j + 3 ? nn : j + 3;
                            for (i = l; i <= mmin; i++) {
                                p = x * a[i][j] + y * a[i][j + 1];
                                if (j != nn - 1) { p += z * a[i][j + 2]; a[i][j + 2] -= p * r; }
                                a[i][j + 1] -= p * q; a[i][j] -= p;
                            }
                        }
                    }
                    ops_propias += (unsigned long)(10 * n * n);
                }
            }
        } while (l < nn - 1);
    }
    return 1;
}

static unsigned long ops_lu;              /* lo que costó la última factorización */

/* Lo del planificador del modo s (sim.h), con las mismas cuentas que hacían
 * umbral_modos y rig_pasos_de en cada llamada (unas 200 por arranque: 4000
 * sqrt y 15 000 ddiv en el peor caso): bit a bit lo mismo */
typedef char cabe_taus[SIM_MODO_NJ == RIG_NMAX - 1 ? 1 : -1];
static void modo_prepara(SimModo *s)
{
    int j;
    s->mag = sqrt(s->re * s->re + s->im * s->im);
    s->hl = RIG_HL / s->mag;
    for (j = 1; j < RIG_NMAX; j++) s->taus[j - 1] = RIG_TAUS / (s->mag * (double)j);
}

/* Modos de una topología: valores propios del mapa de un medio paso de
 * Euler implícito con paso h0 (mu = 1/(1 - (h0/2)·s)), s = (2/h0)(1 - 1/mu).
 * Se descartan los algebraicos (|mu| ~ 0: una C sobre una E) y los
 * constantes (s ~ 0: la carga de un nodo entre C, el flujo de un lazo de
 * L). Cada par complejo sale una vez, con im > 0. 0 si no convergió.
 * Con no lineales, la linealización de unos tramos tr (los D y los AO) y las
 * g de los suaves en el punto de operación (§3.K). */
static int modos_de(unsigned mask, const unsigned char *tr, double h0, double t, SimModo *out, int *nout)
{
    static double phi[MOD_MAX][MOD_MAX];
    static double rhs[LU_MAX], x[LU_MAX];
    int est[MOD_MAX], m = 0, j, i, k, nm = 0;
    double wr[MOD_MAX], wi[MOD_MAX], lr[MOD_MAX], li[MOD_MAX], lmax = 0.0;

    *nout = 0;
    for (k = 0; k < R_.nel; k++)
        if ((R_.el[k].tipo == SE_C || R_.el[k].tipo == SE_L) && m < MOD_MAX) est[m++] = k;
    ops_lu = sim_ops();
    suelta_ok = 0;
    if (!prepara_lu(&lu_suelta, &topo_suelta, mask, h0, t, tr, 0, 1)) return 0;
    ops_lu = sim_ops() - ops_lu;
    /* sin C ni L no hay modos, pero la LU se prepara igual: elige_paso toma
     * de ella (y de ops_lu) el costo de un paso, que así es el de este
     * circuito y no el de lo que se emuló antes */
    if (m == 0) return 1;

    for (j = 0; j < m; j++) {
        /* fuentes y anclas a cero; estado = e_j */
        for (k = 0; k < nincog; k++) rhs[k] = 0.0;
        for (i = 0; i < m; i++) {
            const SimEl *e = &R_.el[est[i]];
            rhs[col_br[est[i]]] = i == j ? -(2.0 * e->val / h0) : 0.0;
        }
        lu_resuelve(&lu_suelta, rhs, x);
        for (i = 0; i < m; i++) {
            const SimEl *e = &R_.el[est[i]];
            if (e->tipo == SE_C) {
                double pa = col_phi[e->a] >= 0 ? x[col_phi[e->a]] : 0.0;
                double pb = col_phi[e->b] >= 0 ? x[col_phi[e->b]] : 0.0;
                phi[i][j] = pa - pb;
            } else phi[i][j] = x[col_br[est[i]]];
        }
    }
    if (!valores_propios(phi, m, wr, wi)) return 0;
    for (i = 0; i < m; i++) {
        double mod2 = wr[i] * wr[i] + wi[i] * wi[i];
        if (mod2 <= 1e-24) continue;                 /* algebraico */
        lr[nm] = (2.0 / h0) * (1.0 - wr[i] / mod2);
        li[nm] = (2.0 / h0) * (wi[i] / mod2);
        if (sqrt(lr[nm] * lr[nm] + li[nm] * li[nm]) > lmax) lmax = sqrt(lr[nm] * lr[nm] + li[nm] * li[nm]);
        nm++;
    }
    for (i = 0; i < nm; i++) {
        double mag = sqrt(lr[i] * lr[i] + li[i] * li[i]);
        if (mag <= 1e-9 * (2.0 / h0)) continue;       /* constante: bajo la resolución de h0 */
        if (li[i] < -1e-9 * mag) continue;            /* el conjugado */
        out[*nout].re = lr[i];
        out[*nout].im = fabs(li[i]) > 1e-9 * mag ? fabs(li[i]) : 0.0;
        modo_prepara(&out[*nout]);
        (*nout)++;
    }
    return 1;
}

/* Modos con un paso de prueba afinado: si h0 queda muy lejos de la escala
 * de los modos, se repite con un paso a su medida (más precisión) */
static int modos_afinados(unsigned mask, const unsigned char *tr, double h0, double t, SimModo *out, int *nout)
{
    int i;
    double smin = 0.0, smax = 0.0, ref;
    if (!modos_de(mask, tr, h0, t, out, nout)) return 0;
    for (i = 0; i < *nout; i++) {
        double mag = out[i].mag;
        if (smin == 0.0 || mag < smin) smin = mag;
        if (mag > smax) smax = mag;
    }
    if (smax <= 0.0) return 1;
    ref = 1.0 / sqrt(smin * smax);
    if (h0 > 10.0 * ref || h0 < 0.01 * ref) return modos_de(mask, tr, ref, t, out, nout);
    return 1;
}

/* La presonda de una clase de evento (§17.3; el injerto B de [68], hecho a
 * priori). En una red no lineal los modos del punto de operación no ven el
 * modo rígido de un no lineal que empieza a conducir con el evento: en el
 * «diodo C» de [68] el diodo está en corte en el punto de operación (τ =
 * 10 µs) y, al subir la fuente, conduce con un modo de 0,1 µs (h = 1 µs).
 * Desde el estado del punto de operación, con las fuentes y los interruptores
 * de te⁺ (te: la primera de la clase), el Newton del Euler implícito de ε =
 * RIG_EPS·τ (τ: la menor de los modos de la red; con Katzenelson, los tramos
 * nuevos) da, sin aceptarlo, la linealización en te⁺, y modos_afinados sus
 * modos (en md; su número, o -1 si no se pudo). Se reponen M.nl (vC e iL no
 * se tocan), el diagnóstico, los avisos, el residuo acumulado y ops_lu; sus
 * iteraciones van a nw.sonda (no a nw.iter). */
static int presonda_clase(double te, double h0, SimModo *md, double *tau)
{
    static double x[LU_MAX];
    static NlEst bak;
    SimNewton cnt;
    SimDiag d0 = res_.d;
    unsigned int av0 = avisos;
    unsigned long olu = ops_lu;
    unsigned mask = mask_de(te, 0);
    int ea0 = elem_aviso, nm = 0, r, k;
    double ref = res_.tau_min > 0.0 ? res_.tau_min : h0, ra0 = residuo_acum, rb0 = residuo_barr, smax = 0.0;
    bak = M.nl;
    memcpy(x, M.x, (size_t)nincog * sizeof x[0]);
    memset(&cnt, 0, sizeof cnt);
    NC.modo = NLM_TR; NC.metodo = 2; NC.t = te; NC.izq = 0; NC.hh = 2.0 * RIG_EPS * ref;
    NC.mask = mask; NC.suelta = 1; NC.fijo = -1; NC.arranque0 = NL.tramos && te == 0.0;
    suelta_ok = 0;
    r = newton(NL.tramos, 1.0, 0.0, x, 50, &M.nl, &cnt);
    res_.nw.sonda += cnt.iter;
    if (r != NLR_OK || !modos_afinados(mask, M.nl.tramo, h0, te, md, &nm)) nm = -1;
    for (k = 0; k < nm; k++) if (md[k].mag > smax) smax = md[k].mag;
    *tau = smax > 0.0 ? 1.0 / smax : 0.0;
    M.nl = bak;
    res_.d = d0; avisos = av0; elem_aviso = ea0; residuo_acum = ra0; residuo_barr = rb0; ops_lu = olu;
    suelta_ok = 0;
    return nm;
}

/* Las clases de los eventos de las fuentes y los S en [0, t_hasta] (los
 * RIG_NENUM primeros; con t = 0 si es una discontinuidad) y la presonda de
 * cada una (RIG_NCL como mucho: los eventos de las que no caben van con el
 * tramo de la red). Solo en una red con uniones, BJT, MOS o diodos por
 * tramos y con C o L (sonda_red); en las demás los modos de la red valen
 * para todo evento (los lineales no dependen del estado; con solo AO, los
 * del punto de operación y los del AO saturado). */
static void presonda(double t_hasta, double h0)
{
    unsigned long ops0 = sim_ops();
    int nev = 0, c;
    double t, h_prov = M.h;
    memset(&PS, 0, sizeof PS);
    if (!M.sonda_red) return;
    M.h = h0;                                     /* las tolerancias de los eventos, con un paso a su escala */
    for (t = M.disc0 ? 0.0 : siguiente_evento(0.0); t <= t_hasta && nev < RIG_NENUM; t = siguiente_evento(t), nev++) {
        unsigned long long k = clase_de(t);
        for (c = 0; c < PS.n && PS.clave[c] != k; c++) ;
        if (c < PS.n) PS.cuenta[c]++;
        else if (PS.n < RIG_NCL) { PS.clave[c] = k; PS.te[c] = t; PS.cuenta[c] = 1; PS.n++; }
        else PS.resto++;
    }
    for (c = 0; c < PS.n; c++) PS.nm[c] = presonda_clase(PS.te[c], h0, PS.md[c], &PS.tau[c]);
    M.h = h_prov;
    PS.costo = (double)(sim_ops() - ops0);
}

int sim_modos_prueba(const SimRed *s, double h0, SimModo *m, int *n)
{
    SimDiag d;
    *n = 0;
    memset(&res_, 0, sizeof res_);
    if (op_interno(s, &d, 0, 0) != SIM_OK) return 0;
    memset(M.ancla_val, 0, sizeof M.ancla_val);
    return modos_de(mask_de(0.0, 0), tramos_act(), h0, 0.0, m, n);
}

/* Redondea hacia arriba a 1, 2 o 5 por una potencia de 10 */
static double redondea_125(double x)
{
    double p, m;
    if (!(x > 0.0)) return 1.0;
    p = pow(10.0, floor(log10(x)));
    m = x / p;
    if (m <= 1.0 + 1e-9) return p;
    if (m <= 2.0 + 1e-9) return 2.0 * p;
    if (m <= 5.0 + 1e-9) return 5.0 * p;
    return 10.0 * p;
}

/* ¿Es el punto de operación un régimen? Ningún C con corriente ni ninguna L
 * con tensión (una J que carga un C o una E sobre una L integran) */
static int op_en_regimen(void)
{
    double esc = 0.0;
    int k;
    for (k = 0; k < R_.nel; k++) {
        if (fabs(res_.op.I[k]) > esc) esc = fabs(res_.op.I[k]);
        if (fabs(res_.op.V[k]) > esc) esc = fabs(res_.op.V[k]);
    }
    for (k = 0; k < R_.nel; k++) {
        if (R_.el[k].tipo == SE_C && fabs(res_.op.I[k]) > 1e-9 * esc) return 0;
        if (R_.el[k].tipo == SE_L && fabs(res_.op.V[k]) > 1e-9 * esc) return 0;
    }
    return 1;
}

/* El valor 1-2-5 anterior */
static double baja_125(double x)
{
    double p = pow(10.0, floor(log10(x) + 1e-9)), m = x / p;
    if (m > 5.0 + 1e-9) return 5.0 * p;
    if (m > 2.0 + 1e-9) return 2.0 * p;
    if (m > 1.0 + 1e-9) return p;
    return 0.5 * p;
}

/* Máximo común divisor en coma flotante (tolerancia relativa) */
static double mcd_f(double a, double b, double tol)
{
    int i;
    if (a < b) { double t = a; a = b; b = t; }
    for (i = 0; i < 60 && b > tol; i++) {
        double r = fmod(a, b);
        if (r > b - tol) r = 0.0;
        a = b; b = r;
    }
    return a;
}

/* Costo fijo de cada paso en la calculadora (bucles y llamadas), además de
 * las operaciones de coma flotante (0,46329 µs cada una): los medidos (sim.h).
 * Con el paso automático, una emulación no debería pasar de T_TOPE s */
#define NS_OP   SIM_NS_OP
#define NS_PASO SIM_NS_PASO
#define T_TOPE  2.0

/* Eventos en [0, tf] y lo que se sabe de las fuentes */
typedef struct {
    double t_per_max, t_per_min, t_unico, sep, g, T0;
    double t_seno_min;     /* el periodo más corto de una senoidal */
    double ev_por_s;       /* eventos periódicos por segundo */
    int    ev_unicos;
} Fuentes;

static void mira_fuentes(Fuentes *F)
{
    int k;
    memset(F, 0, sizeof *F);
    F->t_per_min = F->sep = F->t_seno_min = NUNCA;
    for (k = 0; k < R_.nel; k++) {
        const SimEl *e = &R_.el[k];
        double c[4];
        int nc = 0, q;
        if (e->tipo == SE_E || e->tipo == SE_J) {
            double T = onda_periodo(e->forma, e->p);
            if (e->forma == ONDA_PULSO && !(e->p[4] > 0.0)) {           /* un solo pulso o escalón */
                double fin = e->p[2] + (e->p[3] > 0.0 ? e->p[3] : 0.0);
                if (fin > F->t_unico) F->t_unico = fin;
                F->ev_unicos += e->p[3] > 0.0 ? 2 : 1;
                if (e->p[3] > 0.0 && e->p[3] < F->sep) F->sep = e->p[3];
                if (e->p[2] > 0.0) c[nc++] = e->p[2];
                if (e->p[3] > 0.0) c[nc++] = e->p[3];
            } else if (T > 0.0) {
                double d = 0.0;
                int por_T = 0;
                if (T > F->t_per_max) F->t_per_max = T;
                if (T < F->t_per_min) F->t_per_min = T;
                if (e->forma == ONDA_SENO && T < F->t_seno_min) F->t_seno_min = T;
                F->T0 = F->T0 > 0.0 ? F->T0 / mcd_f(F->T0, T, 1e-9 * (F->T0 > T ? F->T0 : T)) * T : T;
                c[nc++] = T;
                if (e->forma == ONDA_CUADRADA || e->forma == ONDA_TRIANGULAR) {
                    d = e->p[2] * 0.01 * T;
                    por_T = (d > 0.0 && d < T) ? 2 : (e->forma == ONDA_TRIANGULAR ? 1 : 0);
                } else if (e->forma == ONDA_PULSO) {
                    d = e->p[3];
                    por_T = (d > 0.0 && d < T) ? 2 : 0;
                    if (e->p[2] > 0.0) c[nc++] = e->p[2];
                    F->ev_unicos++;
                }
                if (d > 0.0 && d < T) {
                    c[nc++] = d;
                    if (d < F->sep) F->sep = d;
                    if (T - d < F->sep) F->sep = T - d;
                }
                F->ev_por_s += (double)por_T / T;
            }
        } else if (e->tipo == SE_S) {
            if (e->t1 >= 0.0) { F->ev_unicos++; if (e->t1 > F->t_unico) F->t_unico = e->t1; if (e->t1 > 0.0) c[nc++] = e->t1; }
            if (e->t2 >= 0.0) { F->ev_unicos++; if (e->t2 > F->t_unico) F->t_unico = e->t2; if (e->t2 > 0.0) c[nc++] = e->t2; }
            if (e->t1 >= 0.0 && e->t2 >= 0.0 && fabs(e->t2 - e->t1) > 0.0 && fabs(e->t2 - e->t1) < F->sep)
                F->sep = fabs(e->t2 - e->t1);
        }
        for (q = 0; q < nc; q++)
            F->g = F->g > 0.0 ? mcd_f(F->g, c[q], 1e-9 * (F->g > c[q] ? F->g : c[q])) : c[q];
    }
}

/* Alinea h con la malla de los eventos: g/h entero (h no crece) */
static double alinea(double h, double g)
{
    if (g > 0.0 && g >= h / 1000.0) return g / ceil(g / h - 1e-9);
    return h;
}

/* ¿Algún AO saturado en estos tramos? */
static int ao_saturado(const unsigned char *tr)
{
    int k;
    for (k = 0; tr && k < R_.nel; k++) if (R_.el[k].tipo == SE_AO && tr[k] != TR_LIN) return 1;
    return 0;
}

/* Lo que cuesta evaluar un no lineal en una iteración (§3.L), en operaciones */
static double c_eval_nl(const SimEl *e)
{
    if (e->tipo == SE_AO) return 6.0;                       /* su tramo y sus márgenes */
    if (!es_suave(e)) return 4.0;                           /* un D por tramos         */
    if (e->modelo == NL_BJT) return 2.0 * NL_OPS_EXP + 40.0;
    if (e->modelo == NL_MOS) return 30.0;
    return NL_OPS_EXP + 20.0;                               /* una unión               */
}

/* Ventana de medida: periodos enteros al final (o el último 10 %) */
static double t_med0_de(const Fuentes *F, double t_fin)
{
    double T0 = F->T0, t0;
    if (!(T0 > 0.0) || T0 > t_fin / 2.0) T0 = F->t_per_max;
    if (T0 > 0.0 && T0 <= t_fin / 2.0) {
        double n = floor(0.5 * t_fin / T0 + 1e-9);
        t0 = t_fin - (n > 1.0 ? n : 1.0) * T0;
    } else t0 = 0.9 * t_fin;
    return t0 < 0.0 ? 0.0 : t0;
}

/* Lo que elige_paso sabe del costo (§17.4): por paso, por subpaso del tramo,
 * por evento; los topes */
typedef struct {
    const Fuentes *F;
    double c_paso, c_etapa, c_ymas, c_lu, costo_modos, ns_fijo, ns_extra, ns_ymas, nmax;
    double c_vent;                /* lo de un paso dentro de la ventana de medida */
    int ntop, h_auto, nC;
} CostoPaso;

/* Lo de un evento con un tramo de n pasos h (0: el arranque de 8 medios
 * pasos): sus pasos (arr) y los n pasos h que cubre (rn), sus operaciones
 * aparte de arr·c_paso (el tramo: la segunda etapa de cada subpaso y la
 * cuadratura, con la parte de los eventos que cae en la ventana de medida,
 * y la y+; sin los n pasos h que no se dan) y su tiempo fijo aparte de
 * (arr − rn)·ns_fijo (con suaves, el segundo Newton de cada subpaso y los
 * de la y+). Se suman por clase (w: sus eventos) */
typedef struct { double w, arr, rn, ops, fijo; int lus, rig; } CostoEv;

static void costo_ev_suma(const CostoPaso *cp, double t_fin, int n, double h, double tau, double w, CostoEv *ce)
{
    double arr = n ? (double)(RIG_SUB + RIG_SUB2 * (n - 1)) : ARR_PASOS;
    if (n && NL.n && tau > 0.0) {                 /* los subpasos partidos del principio (aplica_evento) */
        int niv = 0;
        while (niv < NIVEL_MAX_ARR && h / RIG_SUB / (double)(1 << niv) > RIG_K * tau) niv++;
        if (niv) arr += (double)(niv + 6);
    }
    ce->w += w;
    ce->arr += w * arr;
    ce->rn += w * (double)n;
    if (n) {
        double fv = (t_fin - t_med0_de(cp->F, t_fin)) / t_fin;
        ce->ops += w * (arr * (cp->c_etapa + (8.0 + 12.0 * fv) * R_.nel + 16.0 + 3.0 * cp->nC) + cp->c_ymas - (double)n * cp->c_paso);
        ce->fijo += w * (arr * cp->ns_extra + cp->ns_ymas);
        if ((n > 1 ? 4 : 3) > ce->lus) ce->lus = n > 1 ? 4 : 3;   /* LU por topología: h, las del tramo y la de la y+ */
        ce->rig = 1;
    }
}

/* El tramo de los eventos de la clase c con el paso h (nr: el de la red, si
 * su presonda no pudo) */
static int rig_clase(int c, double h, int nr)
{
    int n;
    if (PS.nm[c] < 0) return nr;
    n = rig_pasos_de(PS.md[c], PS.nm[c], h, 0);
    return n;
}

/* El costo medio de un evento con el paso h: el tramo de la red en todos o,
 * con clases (§17.3), el de cada clase con sus eventos (y el de la red en los
 * que no caben). Con pesos enteros: sin tramo, la media es 8 exacta */
static void costo_ev(const CostoPaso *cp, double h, double t_fin, CostoEv *ce)
{
    int nr = rig_pasos(h), c;
    memset(ce, 0, sizeof *ce);
    ce->lus = 2;                                  /* LU por topología: h y el arranque */
    if (!PS.n) costo_ev_suma(cp, t_fin, nr, h, res_.tau_min, 1.0, ce);
    else {
        for (c = 0; c < PS.n; c++) {
            int n = rig_clase(c, h, nr);
            double tau = PS.nm[c] >= 0 && n > 0 && PS.tau[c] > 0.0 && !(res_.tau_min > 0.0 && res_.tau_min <= PS.tau[c]) ? PS.tau[c] : res_.tau_min;
            costo_ev_suma(cp, t_fin, n, h, tau, (double)PS.cuenta[c], ce);
        }
        if (PS.resto) costo_ev_suma(cp, t_fin, nr, h, res_.tau_min, (double)PS.resto, ce);
    }
    ce->arr /= ce->w; ce->rn /= ce->w; ce->ops /= ce->w; ce->fijo /= ce->w;
}

/* Los pasos que caben con h en t_fin (los topes de pasos y de muestras y,
 * con el paso automático, los T_TOPE s), y la estimación T; *rig: algún
 * evento lleva el tramo. Un paso cuesta c_paso y, dentro de la ventana de
 * medida (una fracción fv de t_fin), c_vent más */
static double pasos_caben(const CostoPaso *cp, double h, double t_fin, double *T, int *rig)
{
    const Fuentes *F = cp->F;
    CostoEv ce;
    double N = ceil(t_fin / h - 1e-9), nev = F->ev_unicos + ceil(F->ev_por_s * t_fin), Nlim, Nm, fijo;
    double c_paso = cp->c_paso + (t_fin - t_med0_de(F, t_fin)) / t_fin * cp->c_vent;
    costo_ev(cp, h, t_fin, &ce);
    fijo = ce.rig ? PS.costo : 0.0;               /* la presonda cuenta si decide algún tramo */
    *T = (N * c_paso + nev * ce.arr * c_paso + nev * ce.ops + (F->ev_unicos + ce.lus * cp->ntop) * cp->c_lu + cp->costo_modos + fijo) *
             NS_OP * 1e-9 + (N + nev * (ce.arr - ce.rn)) * cp->ns_fijo * 1e-9 + nev * ce.fijo * 1e-9;
    Nlim = cp->nmax - (nev + 1.0) * ce.arr; Nm = (double)(SIM_MAX_MUESTRAS - 2) - 3.0 * nev;
    if (Nm < Nlim) Nlim = Nm;
    if (cp->h_auto) {
        double Nt = (T_TOPE - (nev * ce.arr * c_paso + nev * ce.ops + (F->ev_unicos + ce.lus * cp->ntop) * cp->c_lu + cp->costo_modos + fijo) * NS_OP * 1e-9 -
                     nev * (ce.arr - ce.rn) * cp->ns_fijo * 1e-9 - nev * ce.fijo * 1e-9) / ((c_paso * NS_OP + cp->ns_fijo) * 1e-9);
        if (Nt < Nlim) Nlim = Nt;
    }
    *rig = ce.rig;
    return Nlim;
}

/* El menor h > h0 en que cambia el tramo de algún modo de s: su umbral
 * RIG_HL/|s| o un cambio de n = ⌈RIG_TAUS/(|s|·h)⌉ (hasta m) */
static double umbral_modos(const SimModo *s, int nm, double h0, double m)
{
    int k, j;
    for (k = 0; k < nm; k++) {
        double b;
        if (!(s[k].mag > 0.0)) continue;
        b = s[k].hl;
        if (b > h0 && b < m) m = b;
        for (j = 1; j < RIG_NMAX; j++) {
            b = s[k].taus[j - 1];
            if (b > h0 && b < m && b > s[k].hl) m = b;
        }
    }
    return m;
}

/* El siguiente h (mayor que h0) en que cambia el costo de un evento: el de
 * los modos de la red y de las clases; alineado hacia arriba con la malla de
 * los eventos (g). NUNCA si no hay */
static double rig_umbral_sig(double h0, double g)
{
    double m = NUNCA;
    int i, c;
    for (i = 0; i < res_.ntop; i++) m = umbral_modos(res_.modo[i], res_.nmodos[i], h0, m);
    for (c = 0; c < PS.n; c++) if (PS.nm[c] > 0) m = umbral_modos(PS.md[c], PS.nm[c], h0, m);
    if (m >= NUNCA) return NUNCA;
    m *= 1.0 + 1e-6;                              /* ya del otro lado */
    if (g > 0.0 && g >= m) m = g / floor(g / m + 1e-9);
    return m;
}

/* Con el tramo rígido el costo de un evento cambia con h, y no siempre crece:
 * pasado el umbral de un modo, su tramo de 5 pasos h se acorta al crecer h.
 * El menor h en (h0, hasta) donde caben más pasos que nlim (y 200 si m200);
 * 0 si no hay */
static double paso_mas_barato(const CostoPaso *cp, double h0, double hasta, double t_fin, double nlim, int m200)
{
    double b = h0, T;
    int it, rig;
    for (it = 0; it < 4 * SIM_MAX_MODOS * RIG_NMAX; it++) {
        double nl;
        b = rig_umbral_sig(b, cp->F->g);
        if (!(b < hasta)) return 0.0;
        nl = pasos_caben(cp, b, t_fin, &T, &rig);
        if (nl > nlim && (!m200 || nl >= 200.0)) return b;
    }
    return 0.0;
}

/* El mayor h (alineado) en que ningún evento lleva el tramo; 0 si no hay
 * modos */
static double paso_sin_tramo(double g)
{
    double m = 0.0;
    int i, k, c, red = !PS.n || PS.resto;
    for (c = 0; c < PS.n; c++) {
        if (PS.nm[c] < 0) red = 1;
        for (k = 0; k < PS.nm[c]; k++) if (PS.md[c][k].mag > m) m = PS.md[c][k].mag;
    }
    for (i = 0; red && i < res_.ntop; i++)
        for (k = 0; k < res_.nmodos[i]; k++) if (res_.modo[i][k].mag > m) m = res_.modo[i][k].mag;
    return m > 0.0 ? alinea(RIG_HL / m * (1.0 - 1e-6), g) : 0.0;
}

/* El control del gasto (sim_avanzar, §17.4): lo que la emulación lleva
 * gastado en la calculadora por el modelo con sus cuentas de verdad (las
 * operaciones, los pasos y las iteraciones de cada paso, desde el arranque),
 * y lo que elige_paso le deja para mirarlo, cada GC_MIRA s gastados */
#define GC_MIRA     1.0          /* s: la mitad de T_TOPE, cuando ya se ve el ritmo */
#define GC_DISPARA  3.0          /* s: 1,5 veces T_TOPE                              */
#define GC_OPS_PASO ((unsigned long)(NS_PASO / NS_OP + 0.5))   /* lo fijo, en operaciones */
#define GC_OPS_IT   ((unsigned long)(NS_ITER / NS_OP + 0.5))
#define GC_OPS_MIRA ((unsigned long)(GC_MIRA / (NS_OP * 1e-9)))
static struct {
    Fuentes F;                  /* las de elige_paso (la ventana de medida)      */
    unsigned long ops;          /* las operaciones de los pasos dados            */
    long   pasos, it;           /* sus pasos y sus iteraciones (con la presonda) */
    double ini;                 /* s de t_est de sim_iniciar (modos, presonda)   */
    double plan;                /* el t_est de elige_paso (0 tras acortar)       */
    double t_rel;               /* hasta dónde hace falta para el régimen        */
    unsigned long sig;          /* la próxima mirada: lo gastado, en operaciones */
    int    corta;               /* 1: con el paso automático (el tope de T_TOPE) */
    int    miradas;             /* las hechas (la primera no corta)              */
    int    mitad;               /* 1: ya pasó la mitad hasta la próxima mirada,  */
    double t_mitad, g_mitad;    /*    en t_mitad con g_mitad gastados            */
    double t_ant, g_ant;        /* la mirada de antes: lo emulado y lo gastado   */
} GC;
#ifndef NDLESS
/* El arnés (el oráculo [67], que compara la ventana entera): sin cortar, el
 * control solo corrige t_est */
static int gc_sin_corte_;
void sim_gasto_sin_corte(int si) { gc_sin_corte_ = si != 0; }
#define GC_CORTA(h_auto) ((h_auto) && !gc_sin_corte_)
#else
#define GC_CORTA(h_auto) (h_auto)
#endif

static void elige_paso(void)
{
    static unsigned char tr_top[SIM_MAX_TOP][MAX_EL];
    Fuentes F;
    double h0, tau_s = 0.0, tau_v = 0.0, tau_sat = 0.0, tosc_libre = 0.0, t_rel, t_fin, h;
    double tmin_mod = NUNCA, h_mod = NUNCA, costo_modos, c_paso, c_sust, c_lu, c_nl = 0.0, c_ens = 0.0, ns_fijo = NS_PASO;
    double c_etapa, c_ymas, h_ini, c_fu = 0.0;
    CostoPaso cp;
    int i, k, ntop = 0, nC = 0, t_auto = !(aj_.t_fin > 0.0), h_auto = !(aj_.h > 0.0);
    int hay_ao = 0, extra = -1, rig = 0, rig_b;
    unsigned long ops0;

    mira_fuentes(&F);
    for (k = 0; k < R_.nel; k++) {
        double c_f = 0.0;
        if (R_.el[k].tipo == SE_C || R_.el[k].tipo == SE_L) nC++;
        if (R_.el[k].tipo == SE_E || R_.el[k].tipo == SE_J) c_f = R_.el[k].forma == ONDA_SENO ? SIM_OPS_SIN + 4.0 : 4.0;
        c_fu += c_f;                              /* el valor de las fuentes (fuente_en: un seno, su sin() y 4) */
        if (R_.el[k].tipo == SE_AO) hay_ao = 1;
        if (es_nl(R_.el[k].tipo)) c_nl += c_eval_nl(&R_.el[k]);
        /* el ensamblado de una iteración: el término de cada fuente, el
         * compañero de cada suave y el residuo (§3.L) */
        c_ens += c_f;
        if (es_suave(&R_.el[k])) c_ens += 8.0;
        c_ens += 10.0;
    }

    /* Topologías: la de 0⁺ y la de cada instante en que actúa un S */
    res_.mascara[0] = mask_de(0.0, 0); res_.t_top[0] = 0.0; ntop = 1;
    for (k = 0; k < R_.nel; k++) {
        const SimEl *e = &R_.el[k];
        int q;
        if (e->tipo != SE_S) continue;
        for (q = 0; q < 2; q++) {
            double t = q == 0 ? e->t1 : e->t2;
            unsigned m;
            if (!(t >= 0.0)) continue;
            m = mask_de(t, 0);
            for (i = 0; i < ntop && res_.mascara[i] != m; i++) ;
            if (i < ntop) { if (t < res_.t_top[i]) res_.t_top[i] = t; continue; }
            if (ntop < SIM_MAX_TOP) { res_.mascara[ntop] = m; res_.t_top[ntop] = t; ntop++; }
        }
    }
    /* Con no lineales, todas con los tramos del punto de operación (§3.K) y,
     * con algún AO, una más con cada AO en V+ (la del astable: su τ = R·C) */
    for (i = 0; i < ntop; i++) {
        if (tramos_act()) memcpy(tr_top[i], tramos_act(), sizeof tr_top[i]);
        else memset(tr_top[i], 0, sizeof tr_top[i]);
    }
    if (hay_ao && ntop < SIM_MAX_TOP) {
        memcpy(tr_top[ntop], tr_top[0], sizeof tr_top[ntop]);
        for (k = 0; k < R_.nel; k++) if (R_.el[k].tipo == SE_AO) tr_top[ntop][k] = TR_SATP;
        for (i = 0; i < ntop && !(res_.mascara[i] == res_.mascara[0] && clave_tr(tr_top[i]) == clave_tr(tr_top[ntop])); i++) ;
        if (i == ntop) { res_.mascara[ntop] = res_.mascara[0]; res_.t_top[ntop] = 0.0; extra = ntop++; }
    }
    for (i = 0; i < ntop; i++) res_.tramos_top[i] = NL.n ? clave_tr(tr_top[i]) : 0;
    res_.ntop = ntop;

    /* Modos de cada una, con un paso de prueba de fuentes y eventos */
    h0 = 1e-4;
    if (F.t_per_min < NUNCA) h0 = F.t_per_min / 100.0;
    if (F.sep < NUNCA && F.sep / 4.0 < h0) h0 = F.sep / 4.0;
    ops0 = sim_ops();
    c_lu = 0.0;
    c_sust = 0.0;
    for (i = 0; i < ntop; i++) {
        SimDiag d0 = res_.d;
        if (!modos_afinados(res_.mascara[i], NL.n ? tr_top[i] : 0, h0, res_.t_top[i], res_.modo[i], &res_.nmodos[i])) {
            if (i == extra) {                      /* la saturada es hipotética: sin ella y sin rastro */
                res_.d = d0; M.estado = SIM_OK;
                ntop = res_.ntop = extra;
                break;
            }
            if (res_.d.estado != SIM_OK) { M.estado = res_.d.estado; return; }
            avisa(SIM_AV_SIN_MODOS, -1);
            res_.nmodos[i] = 0;
        }
        if (i == extra) res_.d = d0;
        else c_sust = (double)(lu_suelta.lini[lu_suelta.n] + lu_suelta.uini[lu_suelta.n] + lu_suelta.n);
        if (i == 0) c_lu = (double)ops_lu;
    }
    costo_modos = (double)(sim_ops() - ops0);
    /* costo de un paso: la sustitución por la LU (la de la última topología
     * real, como siempre: la saturada no cuenta), el término (el valor de
     * cada fuente) y los valores (otra vez); con suaves, dos iteraciones de
     * media, cada una con su ensamblado (y su residuo), su LU, su
     * sustitución y sus evaluaciones, más el registro con la historia y los
     * valores, y NS_ITER fijos por iteración; con solo tramos, los márgenes
     * (§3.K) y los NS_ITER de su iteración (un paso con tramos también pasa
     * por newton(), con una, que acaba con el residuo: residuo_de, 6 por
     * elemento; sin él, lo real salía ~10 % largo de mediana en las redes
     * con tramos de la batería). Dentro de la ventana de medida, sus
     * integrales (ventana(): 16 por elemento). Lo que Newton pueda partir el
     * paso o iterar de más no se sabe aquí: lo mira el control del gasto */
    if (c_sust <= 0.0) c_sust = (double)(lu_suelta.lini[lu_suelta.n] + lu_suelta.uini[lu_suelta.n] + lu_suelta.n);
    c_paso = c_sust + 2.0 * nC + 2.0 * c_fu + 15.0 * R_.nel + 12.0;
    if (c_lu <= 0.0) c_lu = (double)(lu_suelta.n * lu_suelta.n * lu_suelta.n) / 3.0;
    if (NL.suave) {
        c_paso = 2.0 * (c_ens + c_lu + c_sust + c_nl) + 2.0 * nC + c_fu + 37.0 * R_.nel + 12.0;
        ns_fijo = NS_PASO + 2.0 * NS_ITER;
    } else if (NL.n) {
        c_paso += c_nl + 6.0 * R_.nel;
        ns_fijo = NS_PASO + NS_ITER;
    }
    /* el tramo rígido (§17.3): la segunda etapa de un subpaso (la sustitución
     * y el término; con suaves, otro Newton); la y+ de un evento (2 a 4
     * sustituciones de ε; con no lineales, sus Newton) */
    c_etapa = NL.suave ? 2.0 * (c_ens + c_lu + c_sust + c_nl) + 2.0 * nC + c_fu :
              c_sust + 2.0 * nC + 2.0 * c_fu + c_nl + (NL.n ? 6.0 * R_.nel : 0.0);
    c_ymas = NL.n ? 3.0 * c_paso : 3.0 * c_sust;

    /* Constantes de tiempo y oscilaciones de todas las topologías. Con no
     * lineales y fuentes periódicas, la ventana no mira las τ de más de
     * 10·T_máx (la descarga del C de un rectificador con los diodos en corte:
     * el C se recarga en cada ciclo); con AO, las de las saturadas aparte */
    for (i = 0; i < ntop; i++)
        for (k = 0; k < res_.nmodos[i]; k++) {
            const SimModo *s = &res_.modo[i][k];
            double al = -s->re, mag = sqrt(s->re * s->re + s->im * s->im);
            if (al > 1e-9 * mag) {
                if (1.0 / al > tau_s) tau_s = 1.0 / al;
                if (!(NL.n && F.t_per_max > 0.0 && 1.0 / al > 10.0 * F.t_per_max) && 1.0 / al > tau_v) tau_v = 1.0 / al;
                if (NL.n && ao_saturado(tr_top[i]) && 1.0 / al > tau_sat) tau_sat = 1.0 / al;
            }
            else if (s->im > 0.0 && 2.0 * PI_D / s->im > tosc_libre) tosc_libre = 2.0 * PI_D / s->im;
            if (s->im > 0.0 && 2.0 * PI_D / s->im > res_.t_osc) res_.t_osc = 2.0 * PI_D / s->im;
            if (mag > 0.0 && 1.0 / mag < tmin_mod) tmin_mod = 1.0 / mag;
        }
    res_.tau_max = tau_s;
    res_.tau_min = tmin_mod < NUNCA ? tmin_mod : 0.0;

    /* Ventana (§3.K): con no lineales y fuentes periódicas, 5·T_máx; sin
     * fuentes periódicas y con algún AO, 10·τ de las saturadas (un astable de
     * β = 0,5 da ~4,5 periodos); si no, la regla de siempre */
    t_rel = F.t_unico + 5.0 * tau_v;
    if (tau_v == 0.0 && F.t_unico > 0.0) t_rel = 1.2 * F.t_unico;   /* que el último evento se vea */
    if (t_auto) {
        t_fin = t_rel;
        if (NL.n && F.t_per_max > 0.0) { if (5.0 * F.t_per_max > t_fin) t_fin = 5.0 * F.t_per_max; }
        else if (3.0 * F.t_per_max > t_fin) t_fin = 3.0 * F.t_per_max;
        if (hay_ao && !(F.t_per_max > 0.0) && 10.0 * tau_sat > t_fin) t_fin = 10.0 * tau_sat;
        if (5.0 * tosc_libre > t_fin) t_fin = 5.0 * tosc_libre;
        if (!(t_fin > 0.0)) { t_fin = 10e-3; if (op_en_regimen()) avisa(SIM_AV_QUIETO, -1); }
        if (t_fin < 1e-9) t_fin = 1e-9;
        if (t_fin > 1e4) { t_fin = 1e4; avisa(SIM_AV_NO_ESTAC, -1); }
        t_fin = redondea_125(t_fin);
    } else {
        t_fin = aj_.t_fin;
        if (t_rel > t_fin * (1.0 + 1e-9)) avisa(SIM_AV_NO_ESTAC, -1);
    }
    res_.t_pedido = t_fin;

    /* Paso */
    for (i = 0; i < ntop; i++)
        for (k = 0; k < res_.nmodos[i]; k++) {
            const SimModo *s = &res_.modo[i][k];
            double al = -s->re;
            if (al > 0.0 && 1.0 / al >= t_fin / 5000.0 && 0.1 / al < h_mod) h_mod = 0.1 / al;
            if (s->im > 0.0) {
                double T = 2.0 * PI_D / s->im;
                if (T >= t_fin / 2500.0 && T / 50.0 < h_mod) h_mod = T / 50.0;
            }
        }
    if (h_auto) {
        /* una senoidal, 100 pasos por periodo; una cuadrada, una triangular
         * o un pulso son rectas a trozos (sus flancos y vértices caen en
         * eventos exactos): con 50 basta */
        h = t_fin / 2000.0;
        if (F.t_seno_min < NUNCA && F.t_seno_min / 100.0 < h) h = F.t_seno_min / 100.0;
        if (F.t_per_min < NUNCA && F.t_per_min / 50.0 < h) h = F.t_per_min / 50.0;
        if (h_mod < h) h = h_mod;
        if (F.sep < NUNCA && F.sep / 4.0 < h) h = F.sep / 4.0;
    } else h = aj_.h;
    if (h > t_fin) h = t_fin;
    h = alinea(h, F.g);

    /* Las clases de los eventos y su presonda (§17.3): con uniones, BJT, MOS
     * o diodos por tramos y C o L, el tramo de cada evento lo deciden los
     * modos de la linealización en te⁺ de su clase, y así cuenta aquí */
    presonda(t_fin, h0);

    /* Topes: pasos, muestras y, si el paso es automático, T_TOPE s en la
     * calculadora (un paso dado a mano se respeta: solo lo cambian los
     * topes de pasos y de muestras). Tras cada evento, el arranque (8 pasos)
     * o el tramo rígido (sus subpasos, de dos etapas cada uno), con su costo
     * medio por clase (pasos_caben). Sin tramo, la cuenta de siempre. */
    cp.F = &F; cp.c_paso = c_paso; cp.c_etapa = c_etapa; cp.c_ymas = c_ymas; cp.c_lu = c_lu; cp.c_vent = 16.0 * R_.nel;
    cp.costo_modos = costo_modos; cp.ns_fijo = ns_fijo; cp.nmax = (double)M.tope;
    cp.ns_extra = NL.suave ? 2.0 * NS_ITER : NL.n ? NS_ITER : 0.0;        /* el segundo Newton de cada subpaso */
    cp.ns_ymas = NL.suave ? 6.0 * NS_ITER : NL.n ? 3.0 * NS_ITER : 0.0;   /* los de la y+ */
    cp.ntop = ntop; cp.h_auto = h_auto; cp.nC = nC;
    h_ini = h;
    for (i = 0; i < 200; i++) {
        double N = ceil(t_fin / h - 1e-9), T, Nlim = pasos_caben(&cp, h, t_fin, &T, &rig), hb;
        res_.t_est = T;
        if (N <= Nlim) break;
        if (Nlim >= 200.0 && t_fin / Nlim < t_fin) {
            /* un paso mayor (se vuelve a alinear hacia arriba); con el tramo
             * rígido, antes uno menor donde un evento cueste menos */
            double hn = t_fin / floor(Nlim);
            if (F.g > 0.0 && F.g >= hn) hn = F.g / floor(F.g / hn + 1e-9);
            if (hn <= h) hn = h * 1.0001;
            hb = paso_mas_barato(&cp, h, hn, t_fin, Nlim, 0);
            h = hb > 0.0 ? hb : hn;
            avisa(SIM_AV_PASO_LIM, -1);
            if (t_rel > t_fin * (1.0 + 1e-9)) avisa(SIM_AV_NO_ESTAC, -1);
        } else if ((hb = paso_mas_barato(&cp, h, t_fin / 200.0, t_fin, Nlim, 1)) > 0.0) {
            /* ni con 200 pasos a este costo; un h mayor donde el tramo es
             * más corto */
            h = hb;
            avisa(SIM_AV_PASO_LIM, -1);
            if (t_rel > t_fin * (1.0 + 1e-9)) avisa(SIM_AV_NO_ESTAC, -1);
        } else if (rig && (hb = paso_sin_tramo(F.g)) > 0.0 && hb < h_ini * (1.0 - 1e-9) &&
                   ceil(t_fin / hb - 1e-9) <= pasos_caben(&cp, hb, t_fin, &T, &rig_b) && !rig_b) {
            /* ni con el tramo: un h menor que el pedido en que ningún evento
             * lo lleva (el arranque cuesta menos) y que cabe */
            h = hb;
            avisa(SIM_AV_PASO_LIM, -1);
        } else {
            /* ni con 200 pasos: una ventana más corta; si el paso ya llevaba
             * el tramo, se vuelve a buscar desde el de antes (con la ventana
             * más corta, uno sin tramo puede caber) */
            t_fin = baja_125(t_fin);
            if (rig) h = h_ini;
            if (h > t_fin / 200.0) h = alinea(t_fin / 200.0, F.g);
            avisa(SIM_AV_PASO_LIM, -1);
            avisa(SIM_AV_NO_ESTAC, -1);
        }
    }
    res_.paso_mayor = (unsigned char)(h > h_ini * (1.0 + 1e-9));   /* la curva, menos fina (sim_aviso) */
    /* el tramo de cada clase con este h, y si alguna cambia el de la red */
    M.rig_h = rig_pasos(h);
    rig = M.rig_h > 0 && (!PS.n || PS.resto);
    for (i = 0; i < PS.n; i++) {
        PS.rig[i] = rig_clase(i, h, M.rig_h);
        if (PS.rig[i] != M.rig_h) PS.alguna = 1;
        if (PS.rig[i] > 0) rig = 1;
    }
    /* la presonda de una red en que no decide ningún tramo se hace (una vez),
     * pero no mueve el paso: la red queda como sin ella */
    if (!rig) res_.t_est += PS.costo * NS_OP * 1e-9;

    res_.t_med0 = t_med0_de(&F, t_fin);           /* la ventana de medida */

    M.h = h;
    M.t_fin = t_fin;
    res_.h = h;
    res_.t_fin = t_fin;
    res_.h_auto = (unsigned char)h_auto;
    res_.t_auto = (unsigned char)t_auto;

    /* lo que el control del gasto necesita de este arranque */
    GC.F = F; GC.t_rel = t_rel; GC.corta = GC_CORTA(h_auto); GC.miradas = 0; GC.mitad = 0; GC.sig = GC_OPS_MIRA;
    GC.t_ant = GC.g_ant = 0.0;
    GC.ops = 0; GC.pasos = GC.it = 0;
    GC.ini = (costo_modos + PS.costo) * NS_OP * 1e-9;
    GC.plan = res_.t_est;
}

/* ==========================================================================
 *  Marcha
 * ======================================================================= */
static SimRed red_emu;
static Circuito circ_;
static int hay_circ_;


/* Cierra la emulación: lo que va hecho vale (también si se cortó) */
static void termina(int fin)
{
    int k;
    double D = M.t - res_.t_med0, entregada = 0.0, disipada = 0.0, dalm = 0.0;
    if (!M.activa) return;
    vacia_pendiente();
    M.activa = 0;
    M.hecho = 1;
    res_.fin = fin;
    valores_de(M.x, &res_.ultimo, M.t, 0);
    /* SATURA también al final (§2.6): un AO que empezó en su lineal y acaba
     * saturado (el integrador) */
    for (k = 0; k < R_.nel; k++)
        if (R_.el[k].tipo == SE_AO && (res_.ultimo.reg[k] == REG_SAT_MAS || res_.ultimo.reg[k] == REG_SAT_MENOS))
            avisa(SIM_AV_SATURA, k);
    res_.d.avisos |= avisos;
    res_.d.elem_aviso = elem_aviso;               /* el de avisa: el del OP, o el AO de un aviso de más rango */
    for (k = 0; k < R_.nel; k++) {
        const SimEl *e = &R_.el[k];
        double q, qm, qef;
        if (e->tipo == SE_C) res_.Efin[k] = 0.5 * e->val * vC[k] * vC[k];
        else if (e->tipo == SE_L) res_.Efin[k] = 0.5 * e->val * iL[k] * iL[k];
        if (D > 0.0) {
            res_.Vmed[k] = iv_V[k] / D;  res_.Vef[k] = sqrt(iv_V2[k] / D > 0.0 ? iv_V2[k] / D : 0.0);
            res_.Imed[k] = iv_I[k] / D;  res_.Ief[k] = sqrt(iv_I2[k] / D > 0.0 ? iv_I2[k] / D : 0.0);
            res_.Pmed[k] = iv_P[k] / D;
        } else {                                      /* se cortó antes de la ventana */
            res_.Vmed[k] = M.pV[k]; res_.Vef[k] = fabs(M.pV[k]);
            res_.Imed[k] = M.pI[k]; res_.Ief[k] = fabs(M.pI[k]);
            res_.Pmed[k] = M.pV[k] * M.pI[k];
            if (s2_de(e)) res_.Pmed[k] += (double)s2_de(e) * p2V[k] * p2I[k];
        }
        /* lo que marca un instrumento: CC = media; CA = eficaz de la parte
         * alterna (como Multisim); EF = eficaz total */
        if (e->instr == 'W') res_.lectura[k] = res_.Pmed[k];      /* P media de v_b·i */
        else if (e->instr) {
            qm = e->instr == 'A' ? res_.Imed[k] : res_.Vmed[k];
            qef = e->instr == 'A' ? res_.Ief[k] : res_.Vef[k];
            q = e->med == MED_CA ? qef * qef - qm * qm : 0.0;
            res_.lectura[k] = e->med == MED_CC ? qm : e->med == MED_CA ? sqrt(q > 0.0 ? q : 0.0) : qef;
        }
        if (e->tipo == SE_E || e->tipo == SE_J || e->tipo == SE_EC || e->tipo == SE_JC || e->tipo == SE_AO) entregada += res_.W[k];
        else if (e->tipo == SE_C || e->tipo == SE_L) dalm += res_.Efin[k] - res_.E0[k];
        else if (e->tipo != SE_S && e->tipo != SE_AMI) disipada += res_.W[k];   /* S, A y W: sin caída */
    }
    res_.dif_balance = entregada - disipada - dalm;
    ops_propias += (unsigned long)(20 * R_.nel);
    if (al_terminar_) al_terminar_();
}

int sim_iniciar_red(const SimRed *s, const SimAjustes *aj)
{
    SimDiag d;
    int k, n, i, mueve = 0;
    double y[2];

    memset(&res_, 0, sizeof res_);
    memset(&M, 0, sizeof M);
    memset(&PS, 0, sizeof PS);
    memset(&paso_bak_, 0, sizeof paso_bak_);
    rq_on = rq_e1 = 0;
    rq_s1[0] = rq_s1[1] = rq_s2[0] = rq_s2[1] = 0.0;
    memset(iv_V, 0, sizeof iv_V); memset(iv_V2, 0, sizeof iv_V2);
    memset(iv_I, 0, sizeof iv_I); memset(iv_I2, 0, sizeof iv_I2); memset(iv_P, 0, sizeof iv_P);
    reg_.n = 0;
    reg_.gen = ++gen_;
    aj_ = *aj;
    res_.canal[0] = aj_.canal[0]; res_.canal[1] = aj_.canal[1];
    for (i = 0; i < N_CACHE; i++) cache_uso[i] = 0;
    cache_reloj = 0;
    M.tope = aj_.max_pasos > 0 ? aj_.max_pasos : SIM_MAX_PASOS;
    if (tope_pasos_ > 0 && tope_pasos_ < M.tope) M.tope = tope_pasos_;
    res_.d.elem = res_.d.nodo = res_.d.elem_aviso = -1;

    if (op_interno(s, &d, &M.nl, &res_.nw) != SIM_OK) {
        res_.d = d; M.hecho = 1; M.estado = d.estado; res_.fin = FIN_ERROR;
        return d.estado;
    }
    res_.d = d;
    M.estado = SIM_OK;
    valores_de(x_op, &res_.op, 0.0, 1);
    for (k = 0; k < R_.nel; k++)                  /* la escala de Δi de los suaves: la del OP */
        if (es_suave(&R_.el[k])) { M.inl[k] = res_.op.I[k]; M.imax_nl[k] = fabs(res_.op.I[k]); }

    /* Estado inicial: el de la ficha si lo hay; si no, el del OP */
    for (k = 0; k < R_.nel; k++) {
        const SimEl *e = &R_.el[k];
        double pa, pb;
        if (e->tipo != SE_C && e->tipo != SE_L) continue;
        pa = col_phi[e->a] >= 0 ? x_op[col_phi[e->a]] : 0.0;
        pb = col_phi[e->b] >= 0 ? x_op[col_phi[e->b]] : 0.0;
        /* La V0/I0 de la ficha es el estado del que ARRANCA el circuito en
         * t = 0⁺, aunque no sea un estado de CC posible (un C cargado sobre
         * una L): el punto de operación solo da el resto de la red. Sin
         * V0/I0, el estado es el del punto de operación. */
        if (e->tipo == SE_C) {
            vC[k] = e->ci ? e->x0 : (ci_ef[k] ? ci_val[k] : pa - pb);
            iC[k] = x_op[col_br[k]];
            res_.E0[k] = 0.5 * e->val * vC[k] * vC[k];
        } else {
            iL[k] = e->ci ? e->x0 : (ci_ef[k] ? ci_val[k] : x_op[col_br[k]]);
            vL[k] = pa - pb;
            res_.E0[k] = 0.5 * e->val * iL[k] * iL[k];
        }
        res_.Emax[k] = res_.E0[k];
    }
    memcpy(M.x, x_op, sizeof(double) * (size_t)nincog);
    for (n = 0; n < R_.nn; n++) M.ancla_val[n] = col_phi[n] >= 0 ? x_op[col_phi[n]] : 0.0;
    M.mask = mask_de(0.0, 1);
    if (!revisa_topologias()) { M.hecho = 1; M.estado = res_.d.estado; res_.fin = FIN_ERROR; return M.estado; }

    /* ¿Es t = 0 una discontinuidad? Una fuente periódica (se enciende en 0:
     * salta o cambia de pendiente), un pulso que empieza en 0, un interruptor
     * que actúa en 0, o una condición inicial que no cuadra con el circuito.
     * Si no, el trapecio arranca directamente del punto de operación (así un
     * LC con V0 e I0 conserva la energía exacta). */
    M.disc0 = mask_de(0.0, 0) != M.mask || (avisos & SIM_AV_CI_SALTA) != 0;
    /* Un C en un lazo de tensiones que cuadra (dos C con V0 en paralelo, o
     * sobre una E) tiene i = 0 en el punto de operación, pero en el
     * transitorio las corrientes se reparten por las capacidades: el
     * trapecio arrancaría con una historia que no cuadra y oscilaría. El
     * arranque de Euler implícito lo reparte. */
    for (k = 0; k < R_.nel && !M.disc0; k++)
        if (R_.el[k].tipo == SE_C && topo_op.sust_el[k] == SU_I0) M.disc0 = 1;
    /* Y si el punto de operación no es un régimen: hay C o L con su
     * condición inicial que ya se mueven (i_C o v_L no nulas) junto a C o L
     * libres (abiertos o en corto en el punto de operación, con i_C = 0 o
     * v_L = 0, que en 0+ ya no valen). Con todo dado (un LC con V0 e I0) el
     * punto de operación da las derivadas bien y el trapecio arranca solo. */
    /* Un trozo anclado en el punto de operación: las v_L o i_C que lo cruzan
     * salen del ancla (0 V), no son una historia del trapecio */
    if (avisos & SIM_AV_FLOTA) M.disc0 = 1;
    {
        int libre = 0;
        double esc = 0.0;
        for (k = 0; k < R_.nel; k++) {
            if (fabs(res_.op.I[k]) > esc) esc = fabs(res_.op.I[k]);
            if (fabs(res_.op.V[k]) > esc) esc = fabs(res_.op.V[k]);
        }
        for (k = 0; k < R_.nel; k++) {
            const SimEl *e = &R_.el[k];
            if (e->tipo != SE_C && e->tipo != SE_L) continue;
            if (!ci_ef[k]) libre = 1;
            else if (fabs(e->tipo == SE_C ? res_.op.I[k] : res_.op.V[k]) > 1e-9 * esc) mueve = 1;
        }
        if (libre && mueve) M.disc0 = 1;
    }
    for (k = 0; k < R_.nel && !M.disc0; k++) if (R_.el[k].tipo == SE_AO) M.disc0 = 1;
    for (k = 0; k < R_.nel && !M.disc0; k++) {
        const SimEl *e = &R_.el[k];
        if ((e->tipo == SE_E || e->tipo == SE_J) &&
            (e->forma == ONDA_SENO || e->forma == ONDA_CUADRADA || e->forma == ONDA_TRIANGULAR ||
             (e->forma == ONDA_PULSO && e->p[2] <= 1e-9 * (fabs(e->p[2]) + fabs(e->p[3]) + 1e-30))))
            M.disc0 = 1;
    }

    /* con uniones, BJT, MOS o D por tramos (y C o L), el tramo de cada evento
     * de las fuentes o de un S lo deciden los modos en te⁺ de su clase (la
     * presonda, en elige_paso); con solo AO, los de la red (los del punto de
     * operación y los del AO saturado) */
    for (k = 0; k < R_.nel && NL.n; k++)
        if (R_.el[k].tipo == SE_C || R_.el[k].tipo == SE_L) M.sonda_red |= 1;
        else if (es_suave(&R_.el[k]) || (R_.el[k].tipo == SE_D && R_.el[k].modelo == NL_TRAMOS)) M.sonda_red |= 2;
    M.sonda_red = M.sonda_red == 3;
    M.h = 1.0;                                    /* provisional, para los modos */
    elige_paso();
    if (M.estado != SIM_OK) { M.hecho = 1; res_.fin = FIN_ERROR; return M.estado; }
    /* Con todo dado y fuentes de CC, pero un modo que el paso no resuelve
     * (h > 2·tau) y que arranca fuera del reposo: el trapecio lo dejaría
     * oscilando con signo alterno; el arranque de Euler implícito lo mata */
    if (!M.disc0 && mueve && res_.tau_min > 0.0 && M.h > 2.0 * res_.tau_min) M.disc0 = 1;

    /* Primera muestra: el punto de operación (en 0⁻) */
    M.activa = 1;
    y[0] = canal_val(&res_.op, 0); y[1] = canal_val(&res_.op, 1);
    memcpy(r2V, res_.op.V2, sizeof r2V); memcpy(r2I, res_.op.I2, sizeof r2I);
    registra(0.0, res_.op.V, res_.op.I, y);

    M.t = 0.0; M.t_ancla = 0.0; M.k = 0;
    M.ic = cache_de(M.mask, tramos_act(), M.h, 0.0);
    if (M.ic < 0) { M.estado = res_.d.estado; termina(FIN_ERROR); return M.estado; }
    M.t_ev = siguiente_evento(0.0);
    if (M.disc0) aplica_evento(0.0);
    if (M.estado != SIM_OK) { termina(FIN_ERROR); return M.estado; }
    res_.d.avisos |= avisos;
    return M.estado;
}

/* ---- Canales: las sondas, o los automáticos ---- */
static void nombre_canal(const Circuito *c, const Red *r, SimCanal *ch)
{
    Buf b;
    int k;
    buf_init(&b, ch->nombre, sizeof ch->nombre);
    if (ch->tipo == CANAL_NODO && c && r)                /* la salida de un AO: «Vsal(U1)» */
        for (k = 0; k < c->nel; k++)
            if (c->el[k].tipo == EL_AO && r->c[k] == ch->nodo) {
                buf_str(&b, "Vsal("); buf_str(&b, c->el[k].etiq); buf_ch(&b, ')');
                return;
            }
    if (ch->tipo == CANAL_NODO) { buf_str(&b, "nodo "); buf_ch(&b, red_letra(r, ch->nodo)); }
    else if (ch->tipo == CANAL_I || ch->tipo == CANAL_V) {
        buf_ch(&b, ch->tipo == CANAL_I ? 'I' : 'V');
        buf_ch(&b, '(');
        buf_str(&b, (c && ch->elem >= 0 && ch->elem < c->nel) ? c->el[ch->elem].etiq : "?");
        buf_ch(&b, ')');
    }
}

static void pon_canal(SimCanal *ch, int tipo, int elem, int sg)
{
    ch->tipo = (unsigned char)tipo; ch->elem = (signed char)elem; ch->nodo = -1; ch->sg = (signed char)sg;
}

void sim_canales(const Circuito *c, const Red *r, SimCanal ch[2])
{
    int i, k, uno = -1, tipo1 = -1;
    memset(ch, 0, 2 * sizeof *ch);
    for (i = 0; i < 2; i++) { ch[i].elem = ch[i].nodo = -1; ch[i].sg = 1; }
    if (c->sonda[0].tipo != SONDA_NO || c->sonda[1].tipo != SONDA_NO) {
        for (i = 0; i < 2; i++) {
            const Sonda *s = &c->sonda[i];
            if (s->tipo == SONDA_NODO) {
                int n = (s->x >= 0 && s->y >= 0 && s->x < REJ_COLS && s->y < REJ_FILAS) ? r->punto[s->y * REJ_COLS + s->x] : -1;
                if (n >= 0) { ch[i].tipo = CANAL_NODO; ch[i].nodo = (signed char)n; }
            } else if ((s->tipo == SONDA_I || s->tipo == SONDA_V) && s->el >= 0 && s->el < c->nel)
                pon_canal(&ch[i], s->tipo == SONDA_I ? CANAL_I : CANAL_V, s->el, s->sg ? s->sg : 1);
            nombre_canal(c, r, &ch[i]);
        }
        return;
    }
    /* Con un AO: su salida, «Vsal(U1)», y la V de la primera E (§4.11) */
    for (k = 0; k < c->nel; k++)
        if (c->el[k].tipo == EL_AO && r->c[k] >= 0) {
            int f;
            ch[0].tipo = CANAL_NODO; ch[0].nodo = (signed char)r->c[k]; ch[0].elem = -1; ch[0].sg = 1;
            for (f = 0; f < c->nel; f++)
                if (c->el[f].tipo == EL_FUENTE) { pon_canal(&ch[1], CANAL_V, f, -1); break; }
            for (i = 0; i < 2; i++) { ch[i].automatico = 1; nombre_canal(c, r, &ch[i]); }
            return;
        }
    /* Sin sondas: la V del primer C; si no, la I de la primera L; si no, la
     * I de la primera E; si no, la V de la primera J */
    for (k = 0; k < c->nel && uno < 0; k++) if (c->el[k].tipo == EL_COND) { uno = k; tipo1 = CANAL_V; }
    for (k = 0; k < c->nel && uno < 0; k++) if (c->el[k].tipo == EL_BOB) { uno = k; tipo1 = CANAL_I; }
    for (k = 0; k < c->nel && uno < 0; k++) if (c->el[k].tipo == EL_FUENTE) { uno = k; tipo1 = CANAL_I; }
    for (k = 0; k < c->nel && uno < 0; k++) if (c->el[k].tipo == EL_CORR) { uno = k; tipo1 = CANAL_V; }
    if (uno >= 0) pon_canal(&ch[0], tipo1, uno, c->el[uno].tipo == EL_CORR ? -1 : 1);
    /* El 2: con un C, la I de la primera L; si no, la primera fuente que
     * no es continua (la V de una E, la I de una J) */
    if (uno >= 0 && c->el[uno].tipo == EL_COND)
        for (k = 0; k < c->nel; k++) if (c->el[k].tipo == EL_BOB) { pon_canal(&ch[1], CANAL_I, k, 1); break; }
    if (ch[1].tipo == CANAL_NADA)
        for (k = 0; k < c->nel; k++) {
            const Elemento *e = &c->el[k];
            if ((e->tipo == EL_FUENTE || e->tipo == EL_CORR) && e->variante != ONDA_CC) {
                int t2 = e->tipo == EL_FUENTE ? CANAL_V : CANAL_I;
                if (k == uno && t2 == tipo1) t2 = t2 == CANAL_V ? CANAL_I : CANAL_V;
                pon_canal(&ch[1], t2, k, (t2 == CANAL_V) ? -1 : 1);
                break;
            }
        }
    for (i = 0; i < 2; i++) { ch[i].automatico = 1; nombre_canal(c, r, &ch[i]); }
}

int sim_iniciar(const Circuito *c, const Red *r)
{
    static SimAjustes aj;
    SimDiag d;
    memcpy(&circ_, c, sizeof circ_);
    hay_circ_ = 1;
    memset(&aj, 0, sizeof aj);
    if (sim_red_de(c, r, &red_emu, &d) != SIM_OK) {
        memset(&res_, 0, sizeof res_);
        memset(&M, 0, sizeof M);
        reg_.n = 0; reg_.gen = ++gen_;
        res_.d = d; res_.fin = FIN_ERROR; M.hecho = 1; M.estado = d.estado;
        return d.estado;
    }
    aj.t_fin = c->t_fin.est == M_DATO ? c->t_fin.v : 0.0;
    aj.h = c->paso.est == M_DATO ? c->paso.v : 0.0;
    sim_canales(c, r, aj.canal);
    return sim_iniciar_red(&red_emu, &aj);
}

/* El control del gasto. elige_paso cuenta dos iteraciones por paso y el paso
 * entero, y en una red suave difícil Newton parte el paso e itera de más (y
 * agrandar h no lo abarata: la subdivisión crece): el puente del banco con
 * 1 s pedido tardaba ~7,4 s con 1,96 estimados; con la ventana automática
 * (0,5 s), ~4,5; el peor de la batería, 4,4 con 2,0. Así que, al llevar
 * GC_MIRA s gastados (por el modelo, con las cuentas de verdad: el paso de
 * cada llamada, sin mirar sim_ops, que el arnés y el banco ponen a cero; no
 * depende del troceo) se mira el ritmo, el de la segunda mitad de lo gastado
 * (el arranque y el primer transitorio suelen costar más: con el ritmo
 * medio, el puente de la biblioteca salía 2,41 s por 2,05 de verdad; así,
 * 1,98). t_est pasa a ser lo gastado más lo que falta a ese ritmo, si es
 * más que lo planeado. Se vuelve a mirar cada GC_MIRA s, con el ritmo de la
 * segunda mitad del último (un tramo caro que llega tarde también se ve).
 * Con el paso automático, desde la segunda mirada (el ritmo del primer
 * segundo gastado puede ser aún el del arranque: caso_244 de [67]
 * proyectaba 5,3 s y costaba 1,8), si la emulación entera pasaría de
 * GC_DISPARA s tanto al ritmo de la segunda mitad como al de todo lo gastado
 * desde la mirada de antes (un pico de medio segundo no basta: caso_258 de
 * [67], con sus 30 ms, proyectaba 10,5 s en la segunda mirada y entero
 * cuesta 2,5), la ventana se acorta ahí mismo, al menor de los dos ritmos
 * (el mismo que no la corta: con el mismo paso, pedir más no da menos): a la
 * mayor 1-2-5 que cabe en GC_DISPARA s; si no queda ninguna tras lo
 * emulado, a la mayor de la rejilla del 1 % de la escala (la de lo que
 * cabe, o la de lo emulado si ya no cabe nada; no la de t_fin: con 1 s o más
 * no tenía ningún punto entre lo emulado y el final, y lo caro que llegaba
 * al final no se cortaba), o a la primera de esa rejilla tras lo emulado en
 * que cabe la ventana de medida (con periodos, uno entero después: se salta
 * hasta ahí). El paso no cambia y lo emulado vale: no se vuelve a empezar.
 * La ventana de medida (periodos enteros al final, o el último 10 %) no
 * puede empezar antes de lo ya emulado: se queda con los periodos que caben
 * después, y las integrales de lo ya emulado (antes de ella) vuelven a
 * cero. Avisa PASO_LIM (su texto dice que se acortó la ventana, y también
 * el paso si elige_paso lo agrandó: sim_aviso). Con el paso a mano no hay
 * tope: solo se corrige t_est (la carga entera de BN_NL: 2,6 s a priori, 8,2
 * de verdad).
 * Lo que NO asegura (la revisión final):
 *  - que pedir más ventana dé siempre más: con más, elige_paso planea un h
 *    mayor, y en algunas redes ese h cuesta cientos de veces más por segundo
 *    emulado (caso_245 de [67]: 42 ms enteros en 1,7 s; con 70 pedidos, 0,7
 *    ms en 3,7 s); el control solo acorta. Es el plan de elige_paso (el
 *    punto 19, con rev_cal 6);
 *  - ~3 s con fuentes periódicas: la ventana de medida es de periodos
 *    enteros, y si tras lo emulado no cabe ninguno antes del final no se
 *    corta (dejaría la medida en medio periodo); la cota es GC_DISPARA más
 *    un periodo al ritmo del final (caso_258 de [67] con 90 ms, ~4,3 s) */

/* El periodo de la ventana de medida de [0, t_fin] (el de t_med0_de): T0, o
 * el más largo; 0 si ninguno cabe dos veces */
static double per_med(const Fuentes *F, double t_fin)
{
    double T0 = F->T0;
    if (!(T0 > 0.0) || T0 > t_fin / 2.0) T0 = F->t_per_max;
    return T0 > 0.0 && T0 <= t_fin / 2.0 ? T0 : 0.0;
}

/* La ventana de medida de [0, t_fin] que empieza en t_min o después; -1 si
 * no cabe ni un periodo */
static double t_med0_desde(const Fuentes *F, double t_fin, double t_min)
{
    double t0 = t_med0_de(F, t_fin), T0 = per_med(F, t_fin), n;
    if (t0 >= t_min) return t0;
    if (!(T0 > 0.0)) return t_min;                                 /* el final, desde t_min */
    n = floor((t_fin - t_min) / T0 + 1e-9);
    return n >= 1.0 ? t_fin - n * T0 : -1.0;
}

/* Lo gastado hasta ahora, en s de la calculadora */
static double gc_gasto(void)
{
    return ((double)GC.ops * NS_OP + (double)GC.pasos * NS_PASO + (double)GC.it * NS_ITER) * 1e-9;
}

/* ¿Vale tp como final de la ventana? Tras lo emulado, antes del final y con
 * su ventana de medida después de lo emulado: su comienzo, o -1 */
static double gc_final(double tp)
{
    double t0;
    if (!(tp > M.t * (1.0 + 1e-9) && tp < M.t_fin * (1.0 - 1e-9))) return -1.0;
    t0 = t_med0_desde(&GC.F, tp, M.t);
    return t0 >= M.t ? t0 : -1.0;
}

static void controla_gasto(void)
{
    double g = gc_gasto(), r, ra, tp = 0.0, t0 = -1.0, proy, obj, q, k, T;
    int i;
    if (!(M.t > 0.0) || M.hecho) return;
    GC.miradas++;
    /* s de la calculadora por s emulado: el de la segunda mitad; y el de todo
     * lo gastado desde la mirada de antes (en la primera, el medio) */
    r = GC.mitad && M.t > GC.t_mitad ? (g - GC.g_mitad) / (M.t - GC.t_mitad) : g / M.t;
    ra = GC.miradas > 1 && M.t > GC.t_ant ? (g - GC.g_ant) / (M.t - GC.t_ant) : g / M.t;
    GC.t_ant = M.t; GC.g_ant = g;
    proy = g + r * (M.t_fin - M.t);
    res_.t_est = GC.ini + proy > GC.plan ? GC.ini + proy : GC.plan;
    /* con el paso a mano no hay tope; la primera mirada no corta (su ritmo
     * puede ser aún el del arranque); y han de pasarse los dos ritmos: el
     * corte, al menor */
    if (ra < r) r = ra;
    if (!GC.corta || GC.miradas < 2 || g + r * (M.t_fin - M.t) <= GC_DISPARA) return;
    obj = M.t + (GC_DISPARA - g) / r;             /* hasta ahí cabe sin pasar de GC_DISPARA */
    q = pow(10.0, floor(log10(obj > M.t ? obj : M.t) + 1e-9) - 2.0);   /* la rejilla: el 1 % de la escala */
    if (obj > M.t) {                              /* la mayor 1-2-5 que cabe */
        tp = redondea_125(obj);
        if (tp > obj * (1.0 + 1e-9)) tp = baja_125(tp);
        for (; tp > M.t * (1.0 + 1e-9) && (t0 = gc_final(tp)) < 0.0; tp = baja_125(tp)) {}
        for (k = floor(obj / q + 1e-9); t0 < 0.0 && k * q > M.t * (1.0 + 1e-9); k -= 1.0)
            t0 = gc_final(tp = k * q);            /* si no, la mayor de la rejilla (menos de 1000) */
    }
    /* si no, la primera de la rejilla tras lo emulado en que cabe la ventana
     * de medida: con periodos, uno entero después (se salta hasta ahí) */
    for (i = 0, k = floor(M.t / q + 1e-9) + 1.0; t0 < 0.0 && i < 16 && k * q < M.t_fin * (1.0 - 1e-9); i++) {
        t0 = gc_final(tp = k * q);
        T = per_med(&GC.F, tp);
        k = T > 0.0 && ceil((M.t + T) / q - 1e-9) > k + 1.0 ? ceil((M.t + T) / q - 1e-9) : k + 1.0;
    }
    if (t0 < 0.0) return;
    M.t_fin = tp;
    res_.t_fin = tp;
    res_.t_med0 = t0;
    memset(iv_V, 0, sizeof iv_V); memset(iv_V2, 0, sizeof iv_V2);
    memset(iv_I, 0, sizeof iv_I); memset(iv_I2, 0, sizeof iv_I2); memset(iv_P, 0, sizeof iv_P);
    res_.t_est = GC.ini + g + r * (tp - M.t);
    GC.plan = 0.0;
    avisa(SIM_AV_PASO_LIM, -1);
    if (GC.t_rel > tp * (1.0 + 1e-9)) avisa(SIM_AV_NO_ESTAC, -1);
}

/* ops: el presupuesto de este trozo, por diferencia con el contador (en la
 * calculadora es de 32 bits y da la vuelta: un tope «ahora + ops» saturado
 * correría hasta el final sin volver a la interfaz) */
int sim_avanzar(unsigned long ops)
{
    unsigned long ahora = sim_ops();
    if (!M.activa) return 0;
    do {                                          /* al menos un paso */
        double t0 = M.t;
        unsigned long o0 = sim_ops();
        long p0 = res_.pasos, i0 = res_.nw.iter + res_.nw.sonda;
        if (!un_paso()) { termina(FIN_ERROR); return 0; }
        /* el gasto del paso, sin mirar el contador de antes (el arnés y el
         * banco lo ponen a cero entre llamadas) */
        GC.ops += sim_ops() - o0; GC.pasos += res_.pasos - p0; GC.it += res_.nw.iter + res_.nw.sonda - i0;
        {
            unsigned long e = GC.ops + (unsigned long)GC.pasos * GC_OPS_PASO + (unsigned long)GC.it * GC_OPS_IT;
            if (!GC.mitad && e >= GC.sig - GC_OPS_MIRA / 2) { GC.mitad = 1; GC.t_mitad = M.t; GC.g_mitad = gc_gasto(); }
            if (e >= GC.sig) { controla_gasto(); GC.mitad = 0; GC.sig = e + GC_OPS_MIRA; }
        }
        /* red de seguridad: el tiempo tiene que avanzar */
        if (M.t > t0) M.quietos = 0;
        else if (++M.quietos > 1000) { M.estado = SIM_SINGULAR; anota_diag(SIM_SINGULAR, -1, M.t); termina(FIN_ERROR); return 0; }
        if (res_.pasos >= M.tope && !M.hecho) { termina(FIN_TOPE); return 0; }
        if (reg_.n >= SIM_MAX_MUESTRAS - 4 && !M.hecho) { termina(FIN_TOPE); return 0; }
    } while (!M.hecho && sim_ops() - ahora < ops);
    if (M.hecho) { termina(M.estado == SIM_OK ? FIN_COMPLETO : FIN_ERROR); return 0; }
    return 1;
}

void sim_cortar(void) { if (M.activa) termina(FIN_CORTADO); }

double sim_progreso(void)
{
    if (!M.activa) return 1.0;
    return M.t_fin > 0.0 ? M.t / M.t_fin : 0.0;
}

int sim_emular(const SimRed *s, const SimAjustes *aj)
{
    if (sim_iniciar_red(s, aj) != SIM_OK) return res_.d.estado;
    while (sim_avanzar(1000000ul)) ;
    return res_.d.estado;
}

const SimRes *sim_res(void) { return &res_; }
const SimReg *sim_reg(void) { return &reg_; }
const SimRed *sim_red(void) { return &R_; }
unsigned long sim_fuera(void) { return fuera_ + lu_fuera(); }
void sim_tope_pasos(long n) { tope_pasos_ = n; }
void sim_al_terminar(void (*f)(void)) { al_terminar_ = f; }

/* ==========================================================================
 *  7. Muestras y medidas
 * ======================================================================= */
/* Primera muestra con t >= x */
static int busca(double x)
{
    int lo = 0, hi = reg_.n;
    while (lo < hi) { int mid = (lo + hi) / 2; if (reg_.t[mid] < x) lo = mid + 1; else hi = mid; }
    return lo;
}

double sim_canal_en(int canal, double t)
{
    const double *y = reg_.y[canal ? 1 : 0], *mt = reg_.t;
    int i, n = reg_.n;
    if (n == 0) return 0.0;
    if (t < mt[0]) return y[0];
    if (t >= mt[n - 1]) return y[n - 1];
    i = busca(t);
    if (mt[i] == t) {                            /* en un evento, el de después */
        while (i + 1 < n && mt[i + 1] == t) i++;
        return y[i];
    }
    return y[i - 1] + (y[i] - y[i - 1]) * (t - mt[i - 1]) / (mt[i] - mt[i - 1]);
}

/* Integrales de un canal entre t0 y t1: s1 = ∫y dt y s2 = ∫y² dt, las dos
 * por la regla del trapecio sobre las muestras (en periodos enteros de una
 * señal suave es casi exacta; integrar el cuadrado de la recta entre
 * muestras daría un sesgo de (ωh)²/12) */
static void integra(int canal, double t0, double t1, double *s1, double *s2)
{
    const double *y = reg_.y[canal ? 1 : 0], *mt = reg_.t;
    int i = busca(t0);
    double tp = t0, yp = sim_canal_en(canal, t0), yb, dt;
    *s1 = *s2 = 0.0;
    for (; i < reg_.n && mt[i] < t1; i++) {
        dt = mt[i] - tp;
        if (dt > 0.0) {
            *s1 += 0.5 * (y[i] + yp) * dt;
            *s2 += 0.5 * (yp * yp + y[i] * y[i]) * dt;
        }
        tp = mt[i]; yp = y[i];
    }
    yb = sim_canal_en(canal, t1);
    dt = t1 - tp;
    if (dt > 0.0) {
        *s1 += 0.5 * (yb + yp) * dt;
        *s2 += 0.5 * (yp * yp + yb * yb) * dt;
    }
}

void sim_medir(int canal, double ta, double tb, SimMedidas *m)
{
    const double *y = reg_.y[canal ? 1 : 0], *mt = reg_.t;
    int i, i0, i1, nfl = 0, arriba;
    double ya, yb, nivel, hist, t_pri = 0.0, t_ult = 0.0, t_cruce = ta, tp, yp, s1, s2, T;

    memset(m, 0, sizeof *m);
    if (reg_.n < 2 || !(tb > ta)) return;
    ya = sim_canal_en(canal, ta); yb = sim_canal_en(canal, tb);
    m->max = ya > yb ? ya : yb; m->min = ya < yb ? ya : yb;
    i0 = busca(ta); i1 = busca(tb);
    for (i = i0; i < i1; i++) {
        double yi = y[i];
        /* un extremo entre tres muestras equiespaciadas: el vértice de la
         * parábola que pasa por ellas (un pico de senoide no cae justo en
         * una muestra) */
        if (i > i0 && i + 1 < i1 && mt[i] > mt[i - 1] && mt[i + 1] > mt[i] &&
            fabs((mt[i + 1] - mt[i]) - (mt[i] - mt[i - 1])) < 1e-6 * (mt[i + 1] - mt[i])) {
            double c = y[i - 1] - 2.0 * yi + y[i + 1], b2 = y[i + 1] - y[i - 1];
            if (c != 0.0 && ((c < 0.0 && yi >= y[i - 1] && yi >= y[i + 1]) || (c > 0.0 && yi <= y[i - 1] && yi <= y[i + 1])))
                yi -= b2 * b2 / (8.0 * c);
        }
        if (yi > m->max) m->max = yi;
        if (yi < m->min) m->min = yi;
    }
    m->pp = m->max - m->min;

    /* Flancos de subida por el nivel medio, con histéresis del 10 % */
    nivel = 0.5 * (m->max + m->min);
    hist = 0.1 * m->pp;
    arriba = ya >= nivel;
    tp = ta; yp = ya;
    for (i = i0; i <= i1 && m->pp > 0.0; i++) {
        double tt = i < i1 ? mt[i] : tb, yy = i < i1 ? y[i] : yb;
        if (yp < nivel && yy >= nivel && tt > tp)        /* cruce del nivel */
            t_cruce = tp + (nivel - yp) * (tt - tp) / (yy - yp);
        else if (yp < nivel && yy >= nivel) t_cruce = tt;
        if (!arriba && yy >= nivel + hist) {
            arriba = 1;
            if (nfl == 0) t_pri = t_cruce;
            t_ult = t_cruce;
            nfl++;
        } else if (arriba && yy <= nivel - hist) arriba = 0;
        tp = tt; yp = yy;
    }
    if (nfl >= 2 && t_ult > t_pri) {
        T = t_ult - t_pri;
        m->frec = (double)(nfl - 1) / T;
        integra(canal, t_pri, t_ult, &s1, &s2);       /* ciclos enteros */
    } else {
        T = tb - ta;
        integra(canal, ta, tb, &s1, &s2);
    }
    m->media = s1 / T;
    m->eficaz = sqrt(s2 / T > 0.0 ? s2 / T : 0.0);
}

/* ==========================================================================
 *  Mensajes
 * ======================================================================= */
static const char *nombre_el(const Circuito *c, const SimRed *s, int e, char *t)
{
    static const char *const LETRA = "REJCLSAVZDEJUQM";
    if (!s) return (c && e >= 0 && e < c->nel) ? c->el[e].etiq : 0;
    if (e < 0 || e >= s->nel) return 0;
    if (c && s->el[e].k >= 0 && s->el[e].k < c->nel) return c->el[s->el[e].k].etiq;
    t[0] = LETRA[s->el[e].tipo < SE_N ? s->el[e].tipo : 0];
    t[1] = (char)('1' + e % 9); t[2] = 0;
    return t;
}

/* La letra del nodo n como la ve el usuario (red_letra: con terminales, A y
 * B son los suyos), con la red del circuito si se tiene */
static char letra_nodo(const Circuito *c, int n)
{
    static Red r;
    if (!c) return red_letra(0, n);
    red_extraer(c, &r);
    return red_letra(&r, n);
}

const char *sim_mensaje_circ(const Circuito *c, const SimRed *s, const SimDiag *d, char *buf, int cap)
{
    Buf b;
    char t[4];
    const char *nom = nombre_el(c, s, d->elem, t);
    buf_init(&b, buf, (size_t)cap);
    switch (d->estado) {
    case SIM_OK: break;
    case SIM_VACIO: buf_str(&b, "No hay nada que emular."); break;
    case SIM_NADA_BARRER:
        buf_str(&b, "Nada que barrer: el barrido cambia una E, una J, el valor de una R o la ganancia de una "
                    "fuente controlada.");
        break;
    case SIM_FALTA:
        if (c && d->elem >= 0 && d->elem < c->nel) {
            const CampoInfo *ci = tipo_campo_v(c->el[d->elem].tipo, c->el[d->elem].variante, d->campo);
            buf_str(&b, "Para emular falta ");
            buf_str(&b, ci ? ci->nombre : "el valor");
            buf_str(&b, " en ");
            buf_str(&b, nom ? nom : "un elemento");
            buf_str(&b, d->campo == Q_AUX ? " (su r no se emula buscada)." : " (un dato, no [?!]).");
        } else {
            buf_str(&b, "Para emular, ");
            buf_str(&b, nom ? nom : "un elemento");
            buf_str(&b, " necesita su valor (un dato, no [?!]).");
        }
        break;
    case SIM_GRANDE: buf_str(&b, "El circuito es demasiado grande para emularlo."); break;
    case SIM_LAZO_V:
        if (d->t >= 0.0) { char tt[24]; buf_str(&b, "En t = "); buf_str(&b, num_pref(tt, sizeof tt, d->t, 3, " s")); buf_str(&b, ", "); }
        buf_str(&b, nom ? nom : "Una fuente");
        buf_str(&b, " cierra un lazo de fuentes, amper" S_ii "metros o interruptores cerrados: la corriente no queda fijada (o ser" S_ii "a infinita).");
        break;
    case SIM_CORTE_J:
        if (d->t >= 0.0) { char tt[24]; buf_str(&b, "En t = "); buf_str(&b, num_pref(tt, sizeof tt, d->t, 3, " s")); buf_str(&b, ", la"); }
        else buf_str(&b, "La");
        buf_str(&b, " corriente de ");
        buf_str(&b, nom ? nom : "una J");
        buf_str(&b, " no tiene por d" S_oo "nde volver (un lazo abierto, un interruptor abierto o un volt" S_ii "metro ideal).");
        break;
    case SIM_CANCELADO: buf_str(&b, "Emulaci" S_oo "n cancelada."); break;
    case SIM_SOLO_CA:
        buf_str(&b, nom ? nom : "Una Z");
        buf_str(&b, " es una impedancia de CA: no existe en el tiempo. Dib" S_uu "jala como R y L (o R y C) para emularla.");
        break;
    case SIM_CA_FORMA: {
        int f = (s && d->elem >= 0 && d->elem < s->nel) ? s->el[d->elem].forma : ONDA_CUADRADA;
        buf_str(&b, nom ? nom : "Una fuente");
        buf_str(&b, " es ");
        buf_str(&b, NOM_FORMA[f < FORMA_N ? f : ONDA_CUADRADA]);
        buf_str(&b, ": el an" S_aa "lisis de CA solo sabe de senoidales. Para verla, emula en el tiempo.");
        break;
    }
    case SIM_CA_FRECS: {
        int k, otra = -1;
        char u[24], t2[4];
        for (k = 0; s && k < s->nel; k++) if (((d->lazo >> k) & 1u) && k != d->elem) { otra = k; break; }
        if (s && otra >= 0 && d->elem >= 0 && d->elem < s->nel) {
            const char *n2 = nombre_el(c, s, otra, t2);
            buf_str(&b, n2 ? n2 : "?"); buf_str(&b, " va a ");
            buf_str(&b, num_pref(u, sizeof u, s->el[otra].p[1], 4, "Hz"));
            buf_str(&b, " y "); buf_str(&b, nom ? nom : "?"); buf_str(&b, " a ");
            buf_str(&b, num_pref(u, sizeof u, s->el[d->elem].p[1], 4, "Hz"));
        } else buf_str(&b, "Hay fuentes de frecuencias distintas");
        buf_str(&b, ": el an" S_aa "lisis de CA es de una sola frecuencia. Emula en el tiempo para verlas juntas.");
        break;
    }
    case SIM_CA_SIN_F:
        buf_str(&b, "Falta la frecuencia: ponla en la ficha de una fuente alterna o en [menu] > Ajustes de CA.");
        break;
    case SIM_RESONANCIA: {
        int k, otra = -1;
        char u[24], t2[4];
        for (k = 0; s && k < s->nel; k++) if (((d->lazo >> k) & 1u) && k != d->elem) { otra = k; break; }
        buf_str(&b, "A "); buf_str(&b, num_pref(u, sizeof u, d->t, 4, "Hz")); buf_str(&b, ", ");
        buf_str(&b, nom ? nom : "un LC");
        if (otra >= 0) { const char *n2 = nombre_el(c, s, otra, t2); buf_str(&b, " y "); buf_str(&b, n2 ? n2 : "?"); buf_str(&b, " resuenan"); }
        else buf_str(&b, " resuena");
        buf_str(&b, " sin p" S_ee "rdidas: la corriente (o la tensi" S_oo "n) ser" S_ii "a infinita. Pon una R o cambia f.");
        break;
    }
    case SIM_SIN_AB:
        buf_str(&b, "Pon los terminales A y B con [T] en el editor, en puntos del circuito.");
        break;
    case SIM_AB_SEPARADOS:
        buf_str(&b, "A y B no est" S_aa "n unidos por ning" S_uu "n camino: no hay equivalente entre ellos.");
        break;
    case SIM_SIN_REGIMEN:
        buf_str(&b, "En CC no hay r" S_ee "gimen (una J carga un C, o una E alimenta una L sola): no hay Th" S_ee "venin de CC.");
        break;
    case SIM_SIN_FUENTE:
        buf_str(&b, "El Bode necesita una fuente de entrada: pon una E o una J.");
        break;
    case SIM_CTL_MAL: {
        char t2[4];
        const char *nc = nombre_el(c, s, d->nodo, t2);
        return sol_texto_ctl(d->campo, nom ? nom : "?", nc, buf, (size_t)cap);
    }
    case SIM_NO_LINEAL:
        buf_str(&b, nom ? nom : "Un elemento");
        buf_str(&b, " no es lineal: aqu" S_ii " (Th" S_ee "venin, calculadora de CA) solo caben redes lineales. [B] da el Bode en su punto de operaci" S_oo "n.");
        break;
    case SIM_MODELO: {
        /* QZ_RZ, QL_IF y QA_VP son el mismo hueco: lo dice el tipo */
        int tel = c && d->elem >= 0 && d->elem < c->nel ? c->el[d->elem].tipo : -1;
        const SimEl *se = s && d->elem >= 0 && d->elem < s->nel ? &s->el[d->elem] : 0;
        int led = tel == EL_LED || (tel < 0 && se && se->tipo == SE_D && se->p[0] > 0.0);
        int ao = tel == EL_AO || (tel < 0 && se && se->tipo == SE_AO);
        buf_str(&b, nom ? nom : "Un elemento");
        if (d->campo == Q_VAL) buf_str(&b, " tiene un modelo que no existe: el" S_ii "gelo otra vez en su ficha.");
        else if (d->campo == QA_VP && ao) buf_str(&b, ": V+ tiene que ser mayor que V-.");
        else if (d->campo == QL_IF && led) {
            char u[24];
            double If = tel >= 0 ? par_o(&c->el[d->elem], QL_IF, NL_IF) : se->p[1];
            buf_str(&b, ": con If = "); buf_str(&b, num_pref(u, sizeof u, If, 3, "A"));
            buf_str(&b, ", Rs" S_CDOT "If pasa de Vf y la curva no puede pasar por (Vf, If). Baja If o sube Vf.");
        } else if (d->campo == QZ_RZ) buf_str(&b, ": rz" S_CDOT "Iz tiene que ser menor que Vz; si no, la ruptura quedar" S_ii "a en 0 V.");
        else buf_str(&b, ": los par" S_aa "metros de su modelo no cuadran.");
        break;
    }
    case SIM_AO_REF:
        if (d->campo == AOR_CORTO) {
            buf_str(&b, "La salida de "); buf_str(&b, nom ? nom : "un AO");
            buf_str(&b, " est" S_aa " unida a su referencia (la tierra o el - de la primera fuente): quedar" S_ii "a en corto. Mu" S_ee "vela.");
        } else {
            buf_str(&b, nom ? nom : "Un AO");
            buf_str(&b, " no tiene referencia: su salida se mide desde la tierra (su alimentaci" S_oo "n), y ");
            /* con la tierra en otra parte, [G] la quitaría de allí: que lo una */
            if (c && c->tierra_x >= 0) buf_str(&b, "no llega a ella ni a una fuente: " S_uu "nelo al circuito.");
            else buf_str(&b, "aqu" S_ii " no hay tierra ni fuente. Pon la tierra con [G].");
        }
        break;
    case SIM_NO_CONVERGE:
        if (d->campo == NC_TOPE) {
            buf_str(&b, "El punto de operaci" S_oo "n tarda demasiado (muchos no lineales): simplifica el circuito.");
            break;
        }
        if (d->campo == NC_OP) {
            buf_str(&b, "El punto de operaci" S_oo "n no converge: Newton se atasca en "); buf_str(&b, nom ? nom : "un no lineal");
            buf_str(&b, ", aun subiendo las fuentes poco a poco. Revisa los valores o pon una R en serie con ");
            buf_str(&b, nom ? nom : "él"); buf_ch(&b, '.');
            break;
        }
        {
            char tt[24];
            if (d->campo == NC_CONMUTA) {
                buf_str(&b, "En t = "); buf_str(&b, num_pref(tt, sizeof tt, d->t, 3, "s")); buf_str(&b, ", ");
                buf_str(&b, nom ? nom : "un AO");
                buf_str(&b, " conmuta sin parar (ning" S_uu "n estado de su salida cuadra). Revisa c" S_oo "mo est" S_aa " realimentado.");
            } else if (d->campo == NC_TR) {
                /* «otro modelo del diodo (con Vγ)» solo a un diodo exponencial
                 * o del catálogo: un LED, un zener, un BJT, un MOSFET o un AO
                 * no lo tienen (sin la SimRed, los índices son los del circuito) */
                const SimEl *se = s && d->elem >= 0 && d->elem < s->nel ? &s->el[d->elem] : 0;
                int dio = se ? se->tipo == SE_D && se->modelo == NL_UNION && !(se->p[0] > 0.0) && !(se->m[3] > 0.0)
                             : c && d->elem >= 0 && d->elem < c->nel && c->el[d->elem].tipo == EL_DIODO &&
                               c->el[d->elem].variante >= DIO_EXP;
                buf_str(&b, "En t = "); buf_str(&b, num_pref(tt, sizeof tt, d->t, 3, "s"));
                buf_str(&b, " Newton no converge en "); buf_str(&b, nom ? nom : "un no lineal");
                buf_str(&b, ", ni partiendo el paso. Revisa los valores");
                buf_str(&b, dio ? " o usa otro modelo del diodo (con V" S_GAMMA ")." : ".");
            } else {
                char t2[4];
                const char *nb = d->nodo >= 0 ? nombre_el(c, s, d->nodo, t2) : 0;
                if (nb) {                                  /* «Con E1 = 4,20 V ...» */
                    /* la unidad de la fila de su variante (β sin unidad, r en Ω) */
                    const CampoInfo *ci = c && d->nodo < c->nel ? tipo_campo_v(c->el[d->nodo].tipo, c->el[d->nodo].variante, Q_VAL) : 0;
                    buf_str(&b, "Con "); buf_str(&b, nb); buf_str(&b, " = ");
                    buf_str(&b, num_pref(tt, sizeof tt, d->t, 3,
                                         ci ? ci->unidad : c && d->nodo < c->nel ? tipo_uni_val(c->el[d->nodo].tipo) : ""));
                    buf_ch(&b, ' ');
                }
                buf_str(&b, "Newton no converge en "); buf_str(&b, nom ? nom : "un no lineal");
                buf_str(&b, ": la curva queda con un hueco ah" S_ii ".");
            }
        }
        break;
    default:
        buf_str(&b, "El circuito no tiene una soluci" S_oo "n " S_uu "nica");
        if (nom) { buf_str(&b, " (mira "); buf_str(&b, nom); buf_ch(&b, ')'); }
        else if (d->nodo >= 0) { buf_str(&b, " (el nodo "); buf_ch(&b, letra_nodo(c, d->nodo)); buf_str(&b, " flota)"); }
        buf_ch(&b, '.');
        break;
    }
    return buf;
}

const char *sim_mensaje(const SimRed *s, const SimDiag *d, char *buf, int cap)
{
    return sim_mensaje_circ(hay_circ_ && s == &red_emu ? &circ_ : 0, s, d, buf, cap);
}

/* Texto de un aviso */
const char *sim_aviso(const Circuito *c, const SimRes *s, unsigned bit, char *buf, int cap)
{
    Buf b;
    char t[4], tt[24];
    const char *nom = s->d.elem_aviso >= 0 ? nombre_el(c, c ? 0 : &R_, s->d.elem_aviso, t) : 0;
    buf_init(&b, buf, (size_t)cap);
    switch (bit) {
    case SIM_AV_FLOTA:
        buf_str(&b, "Una parte del circuito no toca la referencia: su potencial se fija (0 V al empezar).");
        break;
    case SIM_AV_CARGA:
        buf_str(&b, "C en serie sin V0: en CC se reparten la carga (Q = C" S_CDOT "V igual en la serie).");
        break;
    case SIM_AV_FLUJO:
        buf_str(&b, "L en paralelo sin I0: en CC se reparten el flujo (L" S_CDOT "I igual en el lazo).");
        break;
    case SIM_AV_C_DESC:
        buf_str(&b, "Sin r" S_ee "gimen de CC (una J carga un C): los C empiezan descargados.");
        break;
    case SIM_AV_L_SIN_I:
        buf_str(&b, "Sin r" S_ee "gimen de CC (una E sobre una L");
        if (nom) { buf_str(&b, ", "); buf_str(&b, nom); }
        buf_str(&b, "): la L empieza sin corriente.");
        break;
    case SIM_AV_CI_SALTA:
        buf_str(&b, "Una V0 o I0 no cuadra con el circuito");
        if (nom) { buf_str(&b, " ("); buf_str(&b, nom); buf_ch(&b, ')'); }
        buf_str(&b, ": salta en t = 0+.");
        break;
    case SIM_AV_IMPULSO:
        buf_str(&b, "Hubo saltos instant" S_aa "neos (impulsos de corriente o de tensi" S_oo "n)");
        if (s->nsucesos > 0) {
            buf_str(&b, ", el primero en t = ");
            buf_str(&b, num_pref(tt, sizeof tt, s->suceso[0].t, 3, " s"));
        }
        buf_ch(&b, '.');
        break;
    case SIM_AV_PASO_LIM:                            /* lo que cambió: la ventana (se acortó), el paso o los dos */
        if (s->t_pedido > 0.0 && s->t_fin < s->t_pedido * (1.0 - 1e-9))
            buf_str(&b, s->paso_mayor ? "Para no tardar: ventana m" S_aa "s corta y paso mayor." :
                                        "La ventana se acort" S_oo " para no tardar demasiado.");
        else buf_str(&b, "El paso se agrand" S_oo " para no tardar demasiado: la curva es menos fina.");
        break;
    case SIM_AV_NO_ESTAC:
        buf_str(&b, "La ventana no llega al r" S_ee "gimen permanente.");
        break;
    case SIM_AV_QUIETO:
        buf_str(&b, "Nada cambia con el tiempo: es un circuito de CC.");
        break;
    case SIM_AV_SIN_MODOS:
        buf_str(&b, "No se hallaron las constantes de tiempo: el paso y la ventana son aproximados.");
        break;
    case SIM_AV_HOMOTOPIA:
        buf_str(&b, "Hizo falta subir las fuentes poco a poco (vale).");
        break;
    case SIM_AV_BIESTABLE:                           /* la primera solución: V+, o V- (raíles asimétricos) */
        buf_str(&b, nom ? nom : "Un AO");
        buf_str(&b, s->d.elem_aviso >= 0 && s->d.elem_aviso < MAX_EL && s->op.reg[s->d.elem_aviso] == REG_SAT_MENOS ?
                    " puede empezar en V+ o en V-: empieza en V-." : " puede empezar en V+ o en V-: empieza en V+.");
        break;
    case SIM_AV_ARRANQUE:
        buf_str(&b, nom ? nom : "Un AO"); buf_str(&b, " arranca inestable: su salida se va a un ra" S_ii "l.");
        break;
    case SIM_AV_SATURA: {
        /* «U1 satura en +15 V» (§3.M): el raíl del final si acaba saturado;
         * si no, el del punto de operación. El valor, el del circuito (o el
         * de la SimRed sin él) */
        int k = s->d.elem_aviso, r = REG_NADA;
        double vs = 0.0;
        if (k >= 0 && k < MAX_EL) {
            r = s->ultimo.reg[k];
            if (r != REG_SAT_MAS && r != REG_SAT_MENOS) r = s->op.reg[k];
        }
        if (r != REG_SAT_MAS && r != REG_SAT_MENOS) r = REG_NADA;
        else if (c && k < c->nel && c->el[k].tipo == EL_AO)
            vs = r == REG_SAT_MAS ? par_o(&c->el[k], QA_VP, NL_VSAT) : par_o(&c->el[k], QA_VN, -NL_VSAT);
        else if (!c && k < R_.nel && R_.el[k].tipo == SE_AO) vs = R_.el[k].m[r == REG_SAT_MAS ? 1 : 2];
        else r = REG_NADA;
        buf_str(&b, nom ? nom : "Un AO");
        if (r != REG_NADA) {
            buf_str(&b, vs > 0.0 ? " satura en +" : " satura en ");
            buf_str(&b, num_pref(tt, sizeof tt, vs, 3, " V"));
            buf_str(&b, ": la salida ya no sigue.");
        } else buf_str(&b, " satura: la salida ya no sigue.");
        break;
    }
    case SIM_AV_PEQ_SENAL:
        buf_str(&b, "Peque" S_nn "a se" S_nn "al: no lineales linealizados en Q.");
        break;
    case SIM_AV_QUEMA:
        buf_str(&b, nom ? nom : "Un LED"); buf_str(&b, " pasa de su corriente m" S_aa "xima: se quemar" S_ii "a.");
        break;
    case SIM_AV_CC_BARRIDO:
        buf_str(&b, "Barrido de CC: C abiertos, L cables.");
        break;
    case SIM_AV_SALTO:
        buf_str(&b, "La curva salta (hist" S_ee "resis).");
        break;
    case SIM_AV_HUECOS:
        buf_str(&b, "La curva tiene huecos: ah" S_ii " Newton no converge.");
        break;
    default:
        break;
    }
    return buf;
}
