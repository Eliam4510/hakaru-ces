/* ============================================================================
 *  calca.c  -  La calculadora de CA  (ver calca.h)
 *
 *  Orden del archivo:
 *    1. planteamiento: incógnitas, ecuaciones, escalas y referencia de fase
 *    2. la red de trabajo y una evaluación: residuos y jacobiano exacto
 *    3. soluciones: pulido, juicio (tope, condicionamiento) y orden
 *    4. tareas por trozos: Levenberg-Marquardt, raíz con salvaguarda, cruce
 *       por cero y subdivisión
 *    5. la malla (una incógnita), los arranques (varias) y el diagnóstico
 *    6. la API por trozos
 *    7. mensajes y aplicar una solución
 *
 *  Todo es estático (un problema a la vez). Cada paso de la máquina es
 *  atómico y hace una evaluación completa (a veces ninguna; dos o tres en
 *  los casos raros de evalua1 y plano): cal_avanzar solo para entre dos
 *  pasos, así que el resultado no depende del tamaño de los trozos.
 * ========================================================================= */
#include <string.h>
#include <math.h>
#include "calca.h"
#include "luc.h"
#include "numfmt.h"
#include "gfx.h"

#define PI_K     3.14159265358979323846
#define RAIZ2_K  1.41421356237309504880
#define LN10_K   2.30258509299404568402

#define CAL_TOL    1e-9       /* un residuo así (sin dimensiones) cuadra el dato      */
#define TOL_R      1e-12      /* LM: residuo para parar...                            */
#define TOL_D      1e-10      /* ...y paso (en u)                                     */
#define COND_MAX   1e8        /* peor condicionamiento que se acepta                  */
#define COL_NULA   1e-12      /* |dr/du| así en todas las ecuaciones: no influye       */
#define MIN_R      0.3        /* un mínimo de |r| así, sin cambio de signo, se mira    */
#define LM_ITER    60
#define MALLA_MAX  480        /* 131 + 21 por 16 frecuencias características          */
#define PILA_MAX   16
#define DISTINTAS  16         /* soluciones distintas que se recuerdan (para "mas")   */

static int es_fuente(const Elemento *e) { return e->tipo == EL_FUENTE || e->tipo == EL_CORR; }
static int angular(int que) { return que == EC_ANG || que == EC_FP; }

/* ==========================================================================
 *  1. Planteamiento
 * ======================================================================= */
/* Listas provisionales: con CAL_MUCHOS se cuenta todo aunque no quepa */
static CalInc inc_tmp[2 * MAX_EL + 1];
static CalEcu ecu_tmp[4 * MAX_EL];

static CalInc inc_de(int que, int el, double lo, double hi, double semilla)
{
    CalInc in;
    in.que = (unsigned char)que;
    in.log = (unsigned char)(que == IN_VAL || que == IN_MODU || que == IN_F);
    in.el = (signed char)el;
    in.lo = lo; in.hi = hi;
    in.semilla = semilla < lo ? lo : semilla > hi ? hi : semilla;
    return in;
}

static CalEcu ecu_de(int que, int q, int el, double v, double esc)
{
    CalEcu e;
    e.que = (unsigned char)que;
    e.q = (unsigned char)q;
    e.el = (signed char)el;
    e.v = v;
    e.esc = esc;
    return e;
}

static int con_sim(CalRes *s, int estado, int elem)
{
    s->estado = CAL_SIM;
    s->d.estado = estado;
    s->d.elem = elem;
    s->elem = elem;
    return s->estado;
}

static int con_no_ca(CalRes *s, int elem) { s->estado = CAL_NO_CA; s->elem = elem; return s->estado; }

int cal_plantea(const Circuito *c, CalRes *s)
{
    int k, q, i, ni = 0, ne = 0, gen = -1, hay_z = 0, primera = -1, nz = 0, hay_ang = 0, fase_fija = 0;
    double f = 0.0, w, z0, lz = 0.0, vesc = 0.0, vmax = 0.0, imax = 0.0;

    memset(s, 0, sizeof *s);
    s->estado = CAL_OK;
    s->elem = s->iecu = -1;
    s->ref_fase = -1;
    s->d.elem = s->d.nodo = s->d.elem_aviso = -1;
    s->d.t = -1.0;

    /* ---- Formas, y lo que en CA no se despeja ---- */
    for (k = 0; k < c->nel; k++) {
        const Elemento *e = &c->el[k];
        if (es_fuente(e)) {
            if (e->variante == ONDA_SENO) { if (gen < 0) gen = k; }
            else if (e->variante != ONDA_CC && e->variante != FORMA_FASOR) return con_sim(s, SIM_CA_FORMA, k);
        }
        if (e->tipo == EL_IMP) hay_z = 1;
    }
    for (k = 0; k < c->nel; k++) {
        const Elemento *e = &c->el[k];
        if (e->tipo == EL_FUENTE && e->aux.est == M_BUSCAR) return con_no_ca(s, k);
        if ((e->tipo == EL_VOLT || e->tipo == EL_AMP) && e->val.est == M_BUSCAR) return con_no_ca(s, k);
        /* la calculadora de CA es de redes lineales (D14) */
        if (tipo_info(e->tipo)->ley_cc == LEY_NL) return con_sim(s, SIM_NO_LINEAL, k);
        /* la ganancia de una controlada: en CA, dato (el switch de las
         * incógnitas no la ve nunca buscada) */
        if (e->tipo == EL_EDEP || e->tipo == EL_JDEP) {
            if (e->val.est == M_BUSCAR) return con_no_ca(s, k);
            if (e->val.est != M_DATO) { con_sim(s, SIM_FALTA, k); s->d.campo = Q_VAL; return s->estado; }
        }
    }
    if (c->frec.est == M_BUSCAR && gen >= 0) return con_no_ca(s, gen);   /* un generador fija la f */

    /* ---- La frecuencia: la de ca_frecuencia (las alternas van a frec, los
     *      generadores a la suya); con frec [?!], la semilla ---- */
    if (c->frec.est == M_BUSCAR) f = 1e3;
    else {
        for (k = 0; k < c->nel; k++) {
            const Elemento *e = &c->el[k];
            double fk;
            if (!es_fuente(e)) continue;
            if (e->variante == FORMA_FASOR) {
                if (c->frec.est != M_DATO) return con_sim(s, SIM_CA_SIN_F, -1);
                fk = c->frec.v;
            } else if (e->variante == ONDA_SENO) {
                if (e->par[Q_FREC - Q_P1].est != M_DATO) { con_sim(s, SIM_FALTA, k); s->d.campo = Q_FREC; return s->estado; }
                fk = e->par[Q_FREC - Q_P1].v;
            } else continue;
            if (f == 0.0) { f = fk; primera = k; }
            else if (fabs(fk - f) > CA_TOL_F * f) {
                con_sim(s, SIM_CA_FRECS, k);
                s->d.lazo = (1u << primera) | (1u << k);
                return s->estado;
            }
        }
        if (f == 0.0) {
            if (c->frec.est == M_DATO) f = c->frec.v;
            else return con_sim(s, SIM_CA_SIN_F, -1);
        }
        if (hay_z && c->frec.est != M_DATO) return con_sim(s, SIM_CA_SIN_F, -1);   /* la Z es a frec */
    }
    s->f = f;
    w = 2.0 * PI_K * f;

    /* ---- La impedancia típica Z0: la media geométrica de lo conocido. Da
     *      las semillas y las escalas ---- */
    for (k = 0; k < c->nel; k++) {
        const Elemento *e = &c->el[k];
        double z = 0.0;
        if (e->val.est != M_DATO || !(e->val.v > 0.0)) continue;
        switch (e->tipo) {
        case EL_RES: case EL_IMP: z = e->val.v; break;
        case EL_COND: z = 1.0 / (w * e->val.v); break;
        case EL_BOB: z = w * e->val.v; break;
        default: break;
        }
        if (z > 0.0 && FINITO(z)) { lz += log(z); nz++; }
    }
    z0 = nz ? exp(lz / (double)nz) : 1e3;

    /* ---- Las ecuaciones (los datos), en orden de elemento. Pasan a lo
     *      interno: el módulo eficaz a pico, los grados a radianes ---- */
    for (k = 0; k < c->nel; k++) {
        const Elemento *e = &c->el[k];
        if (e->tipo == EL_VATI) {
            if (e->val.est == M_DATO) ecu_tmp[ne++] = ecu_de(EC_P, 0, k, fabs(e->val.v), fabs(e->val.v));
            if (e->aux.est == M_DATO) {
                double fp = e->aux.v, a = acos(fabs(fp) < 1.0 ? fabs(fp) : 1.0);
                ecu_tmp[ne++] = ecu_de(EC_FP, 0, k, fp < 0.0 ? -a : a, 1.0);   /* + inductivo */
            }
            continue;
        }
        /* una fuente continua (o un generador) no es un fasor: en CA está
         * apagada y sus datos de V e I son de la CC */
        if (es_fuente(e) && e->variante != FORMA_FASOR) continue;
        for (q = Q_V; q <= Q_I; q++) {
            const CampoInfo *ci = circ_campo(e, q);
            const Mag *m = circ_mag_c(e, q);
            int angd;
            if (!ci) continue;
            angd = (ci->flags & CF_ANG) && m->est_ang == M_DATO;
            if (m->est == M_DATO) {
                double v = fabs(m->v);
                if (q == Q_V && v > vmax) vmax = v;
                if (q == Q_I && v > imax) imax = v;
                if (v == 0.0) {                 /* un fasor nulo: sus dos partes (el ángulo no añade) */
                    ecu_tmp[ne++] = ecu_de(EC_RE, q, k, 0.0, 1.0);
                    ecu_tmp[ne++] = ecu_de(EC_IM, q, k, 0.0, 1.0);
                    continue;
                }
                ecu_tmp[ne++] = ecu_de(EC_MOD, q, k, v * RAIZ2_K, 1.0);
            }
            if (angd) { ecu_tmp[ne++] = ecu_de(EC_ANG, q, k, m->ang * PI_K / 180.0, 1.0); hay_ang = 1; }
        }
    }

    /* ---- Escalas de los residuos que no son relativos: la mayor amplitud
     *      conocida de las fuentes (una J, por Z0) ---- */
    for (k = 0; k < c->nel; k++) {
        const Elemento *e = &c->el[k];
        double a;
        if (!es_fuente(e) || e->val.est != M_DATO) continue;
        if (e->variante == FORMA_FASOR) a = RAIZ2_K * fabs(e->val.v);
        else if (e->variante == ONDA_SENO) a = fabs(e->val.v);
        else continue;                                   /* las continuas se apagan */
        if (e->tipo == EL_CORR) a *= z0;
        if (a > vesc) vesc = a;
    }
    if (!(vesc > 0.0)) vesc = RAIZ2_K * (vmax > 0.0 ? vmax : 10.0);
    for (i = 0; i < ne; i++) {
        CalEcu *q2 = &ecu_tmp[i];
        if (q2->que == EC_RE || q2->que == EC_IM) q2->esc = q2->q == Q_V ? vesc : vesc / z0;
        if (q2->que == EC_P && q2->v == 0.0) q2->esc = 0.5 * vesc * vesc / z0;
    }

    /* ---- Las incógnitas, en orden de elemento; la f al final ---- */
    for (k = 0; k < c->nel; k++) {
        const Elemento *e = &c->el[k];
        const TipoInfo *t = tipo_info(e->tipo);
        switch (e->tipo) {
        case EL_RES: case EL_COND: case EL_BOB:
            if (e->val.est != M_DATO)
                inc_tmp[ni++] = inc_de(IN_VAL, k, t->val_min, t->val_max,
                                       e->tipo == EL_RES ? z0 : e->tipo == EL_COND ? 1.0 / (w * z0) : z0 / w);
            break;
        case EL_IMP:
            if (e->val.est != M_DATO) inc_tmp[ni++] = inc_de(IN_VAL, k, t->val_min, t->val_max, z0);
            if (e->val.est_ang == M_BUSCAR) inc_tmp[ni++] = inc_de(IN_ANGZ, k, -90.0, 90.0, 0.0);
            break;
        case EL_FUENTE: case EL_CORR:
            if (e->variante == ONDA_SENO) { fase_fija = 1; break; }
            if (e->variante != FORMA_FASOR) break;
            if (e->val.est != M_DATO)
                inc_tmp[ni++] = inc_de(IN_MODU, k, 1e-9, 1e9, e->tipo == EL_FUENTE ? (vmax > 0.0 ? vmax : 10.0)
                                                                                  : (imax > 0.0 ? imax : 10.0 / z0));
            if (e->val.est_ang == M_BUSCAR) inc_tmp[ni++] = inc_de(IN_ANGU, k, -180.0, 180.0, 0.0);
            else fase_fija = 1;
            break;
        default:
            break;
        }
    }
    if (c->frec.est == M_BUSCAR) inc_tmp[ni++] = inc_de(IN_F, -1, 1e-3, 1e9, 1e3);

    /* ---- Referencia de fase. Si nada fija los ángulos (ni un dato de
     *      ángulo ni una fuente con el suyo), girar todos los fasores a la
     *      vez no cambia ningún módulo ni ninguna potencia: el ángulo de la
     *      primera fuente buscada se fija en 0° y deja de contar. Un fasor
     *      nulo (EC_RE y EC_IM) tampoco fija nada: 0 girado sigue siendo 0 ---- */
    if (!hay_ang && !fase_fija)
        for (i = 0; i < ni; i++)
            if (inc_tmp[i].que == IN_ANGU) {
                s->ref_fase = inc_tmp[i].el;
                memmove(&inc_tmp[i], &inc_tmp[i + 1], (size_t)(ni - i - 1) * sizeof inc_tmp[0]);
                ni--;
                break;
            }

    s->ninc = ni;
    s->necu = ne;
    for (i = 0; i < ni && i < CAL_MAX_INC; i++) s->inc[i] = inc_tmp[i];
    for (i = 0; i < ne && i < CAL_MAX_ECU; i++) s->ecu[i] = ecu_tmp[i];
    if (ni > CAL_MAX_INC || ne > CAL_MAX_ECU) { s->estado = CAL_MUCHOS; return s->estado; }
    if (ni > ne) { s->estado = CAL_FALTAN; return s->estado; }
    return s->estado;
}

/* ==========================================================================
 *  2. La red de trabajo y una evaluación
 * ======================================================================= */
static CalRes  res_;
static SimRed  red0_;                       /* del circuito, fuentes apagadas: la topología */
static SimRed  red_;                        /* la de trabajo: sus valores son las incógnitas */
static CaExc   exc_;
static double  f_;                          /* la f de la evaluación en curso               */
static double  modz_[MAX_EL], angz_[MAX_EL];/* Z: |Z| y ∠Z (rad)                             */
static double  modu_[MAX_EL], angu_[MAX_EL];/* fuentes alternas: |U| de pico y ∠U (rad)      */
static unsigned char tocada_[MAX_EL];       /* Z o fuente con alguna incógnita              */
static unsigned char alterna_[MAX_EL];      /* fuente alterna (su f es la del circuito)      */
static int     frec_busca_;                 /* la f es una incógnita                         */
static double  ulo_[CAL_MAX_INC], uhi_[CAL_MAX_INC];
static int     col_viva_[CAL_MAX_INC];      /* su columna no fue nula en algún punto         */
static int     una_ok_;                     /* alguna evaluación fue bien                    */
static SimDiag fallo_;                      /* la última que no                              */
static Cx      b_[LUC_MAX], v_[LUC_MAX], dx_[LUC_MAX];

typedef struct {
    double u[CAL_MAX_INC];
    double r[CAL_MAX_ECU];
    double J[CAL_MAX_ECU][CAL_MAX_INC];
    double rn, rinf;                        /* Σ r² y el mayor |r|                           */
    double aux, dato;                       /* una incógnita: fase de X (o P) y lo que marca */
    Cx     x[LUC_MAX];                      /* la solución del sistema en u                  */
} Eval;

static Eval ev_, cur_;                      /* la de paso y la de la tarea en curso          */

/* Una incógnita y un dato de módulo o de P: lo que marca en TODAS las
 * evaluaciones (la sección áurea llega al pico que la malla salta) */
static int    dhay_;
static double dlo_, dhi_;

/* Lo más cerca que se llegó (para INCOMPAT con varias) */
static int    mejor_hay_;
static double mejor_rn_, mejor_r_[CAL_MAX_ECU];

/* Lo que dice por qué no hay soluciones */
static int indet_, indet_i1_, indet_i2_;
static int borde_, borde_i_, borde_e_;
static double borde_dir_;

/* Soluciones distintas vistas (en u), aunque no quepan en sol[] */
static double dist_u_[DISTINTAS][CAL_MAX_INC];
static int    ndist_;

/* Pone el punto u en la red de trabajo y en la excitación */
static void pon_u(const double *u)
{
    int i, k;
    f_ = res_.f;
    for (i = 0; i < res_.ninc; i++) {
        const CalInc *in = &res_.inc[i];
        k = in->el;
        switch (in->que) {
        case IN_VAL:
            if (red_.el[k].tipo == SE_Z) modz_[k] = exp(u[i]);
            else red_.el[k].val = exp(u[i]);
            break;
        case IN_ANGZ: angz_[k] = u[i]; break;
        case IN_MODU: modu_[k] = RAIZ2_K * exp(u[i]); break;
        case IN_ANGU: angu_[k] = u[i]; break;
        case IN_F:    f_ = exp(u[i]); break;
        default: break;
        }
    }
    for (k = 0; k < red_.nel; k++) {
        SimEl *e = &red_.el[k];
        if (tocada_[k] && e->tipo == SE_Z) {
            double cz = cos(angz_[k]);
            if (fabs(cz) < 1e-15) cz = 0.0;                 /* ±90°: reactiva pura */
            e->val = modz_[k] * cz;
            e->p[0] = modz_[k] * sin(angz_[k]);
        }
        if (tocada_[k] && alterna_[k]) {     /* el fasor directo, sin pasar por grados */
            exc_.U[k] = cx_polar(modu_[k], angu_[k]);
            e->p[0] = modu_[k];
            e->p[2] = angu_[k] * 180.0 / PI_K + 90.0;
        }
        /* con la f buscada, la Z y las alternas son las del circuito a esa f */
        if (frec_busca_ && (e->tipo == SE_Z || alterna_[k])) e->p[1] = f_;
    }
}

/* S = ½·V·I* de un vatímetro (V la de su bobina de tensión) */
static Cx s_de(Cx V, Cx I) { return cx_esc(cx_por(V, cx_conj(I)), 0.5); }

/* El residuo sin dimensiones de una ecuación */
static double residuo(const CalEcu *q, Cx X, Cx V, Cx I)
{
    double m;
    Cx S;
    switch (q->que) {
    case EC_MOD:
        m = cx_abs(X);
        return log((m > 1e-300 ? m : 1e-300) / q->v);
    case EC_ANG:
        return cx_envuelve(cx_arg(X) - q->v);
    case EC_RE:
        return X.re / q->esc;
    case EC_IM:
        return X.im / q->esc;
    case EC_P:
        S = s_de(V, I);
        return q->v > 0.0 ? (fabs(S.re) - q->v) / q->esc : S.re / q->esc;
    case EC_FP:                                  /* φ con signo: + si P y Q van a la par */
        S = s_de(V, I);
        return atan2(S.re < 0.0 ? -S.im : S.im, fabs(S.re)) - q->v;
    default:
        return 0.0;
    }
}

/* Su derivada, con dX (dV, dI) de ca_dvi */
static double dresiduo(const CalEcu *q, Cx X, Cx V, Cx I, Cx dX, Cx dV, Cx dI)
{
    double m2 = X.re * X.re + X.im * X.im;
    Cx S, dS;
    switch (q->que) {
    case EC_MOD: return m2 > 0.0 ? (dX.re * X.re + dX.im * X.im) / m2 : 0.0;   /* Re(dX/X) */
    case EC_ANG: return m2 > 0.0 ? (dX.im * X.re - dX.re * X.im) / m2 : 0.0;   /* Im(dX/X) */
    case EC_RE:  return dX.re / q->esc;
    case EC_IM:  return dX.im / q->esc;
    default: break;
    }
    S = s_de(V, I);
    dS = cx_esc(cx_mas(cx_por(dV, cx_conj(I)), cx_por(V, cx_conj(dI))), 0.5);
    if (q->que == EC_P) {
        if (q->v > 0.0) return (S.re < 0.0 ? -dS.re : dS.re) / q->esc;
        return dS.re / q->esc;
    }
    m2 = S.re * S.re + S.im * S.im;                                             /* EC_FP */
    return m2 > 0.0 ? (S.re * dS.im - S.im * dS.re) / m2 : 0.0;
}

/* Una evaluación completa en u: una LU, los residuos y, con una
 * sustitución más por incógnita, el jacobiano exacto. 0 si la matriz es
 * singular en u (una resonancia sin pérdidas justo ahí) */
static int evalua(const double *u, Eval *ev)
{
    unsigned long o0 = ca_ops();
    Cx X[CAL_MAX_ECU], Vw[CAL_MAX_ECU], Iw[CAL_MAX_ECU];
    SimDiag d;
    int i, m, ok = 1, n;

    for (i = 0; i < res_.ninc; i++) ev->u[i] = u[i];
    pon_u(u);
    res_.evaluaciones++;
    memset(&d, 0, sizeof d);
    d.elem = d.nodo = d.elem_aviso = -1;
    d.t = -1.0;
    if (ca_ensambla(&red_, f_, &d) != SIM_OK) {
        fallo_ = d;
        res_.ops += ca_ops() - o0;
        return 0;
    }
    ca_b_fuentes(&red_, &exc_, b_);
    ca_sustituye(b_, ev->x);
    ev->rn = ev->rinf = 0.0;
    for (i = 0; i < res_.necu; i++) {
        const CalEcu *q = &res_.ecu[i];
        ca_vi(&red_, &exc_, ev->x, f_, q->el, &Vw[i], &Iw[i]);
        X[i] = q->q == Q_I ? Iw[i] : Vw[i];
        ev->r[i] = residuo(q, X[i], Vw[i], Iw[i]);
        if (!FINITO(ev->r[i])) ok = 0;
        ev->rn += ev->r[i] * ev->r[i];
        if (fabs(ev->r[i]) > ev->rinf) ev->rinf = fabs(ev->r[i]);
    }
    n = ca_n();
    for (m = 0; m < res_.ninc && ok; m++) {
        const CalInc *in = &res_.inc[m];
        int kk = in->que == IN_F ? -1 : in->el;
        double cmax = 0.0;
        ca_derivada(&red_, &exc_, ev->x, f_, kk, in->que, v_);
        for (i = 0; i < n; i++) v_[i] = cx(-v_[i].re, -v_[i].im);   /* dx = -A⁻¹·v */
        ca_sustituye(v_, dx_);
        for (i = 0; i < res_.necu; i++) {
            const CalEcu *q = &res_.ecu[i];
            Cx dV, dI;
            ca_dvi(&red_, &exc_, ev->x, dx_, f_, q->el, kk, in->que, &dV, &dI);
            ev->J[i][m] = dresiduo(q, X[i], Vw[i], Iw[i], q->q == Q_I ? dI : dV, dV, dI);
            if (!FINITO(ev->J[i][m])) ok = 0;
            if (fabs(ev->J[i][m]) > cmax) cmax = fabs(ev->J[i][m]);
        }
        if (ok && cmax > COL_NULA) col_viva_[m] = 1;
    }
    /* una incógnita: lo que marca el dato y su fase (los cruces por cero) */
    ev->aux = ev->dato = 0.0;
    if (ok && res_.ninc == 1 && res_.necu >= 1) {
        const CalEcu *q = &res_.ecu[0];
        Cx S = s_de(Vw[0], Iw[0]);
        double t;
        switch (q->que) {
        case EC_MOD: ev->aux = cx_arg(X[0]); ev->dato = cx_abs(X[0]) / RAIZ2_K; break;
        case EC_ANG: ev->aux = cx_arg(X[0]); ev->dato = ev->aux * 180.0 / PI_K; break;
        case EC_P:   ev->aux = S.re; ev->dato = fabs(S.re); break;
        case EC_FP:
            t = atan2(S.re < 0.0 ? -S.im : S.im, fabs(S.re));
            ev->aux = t; ev->dato = (t < 0.0 ? -1.0 : 1.0) * cos(t);
            break;
        default: break;
        }
        if (ok && res_.necu == 1 && (q->que == EC_MOD || q->que == EC_P) && FINITO(ev->dato)) {
            if (!dhay_ || ev->dato < dlo_) dlo_ = ev->dato;
            if (!dhay_ || ev->dato > dhi_) dhi_ = ev->dato;
            dhay_ = 1;
        }
    }
    res_.ops += ca_ops() - o0;
    if (!ok) { fallo_.estado = SIM_SINGULAR; return 0; }
    una_ok_ = 1;
    if (!mejor_hay_ || ev->rn < mejor_rn_) {
        mejor_hay_ = 1;
        mejor_rn_ = ev->rn;
        memcpy(mejor_r_, ev->r, sizeof mejor_r_);
    }
    return 1;
}

/* Con una incógnita. Justo en una resonancia sin pérdidas la matriz es
 * singular, y no por casualidad: con un solo reactivo conocido, la semilla
 * de C es 1/(ω·Z0) = 1/(ω²·L), la resonancia misma. Un pelo al lado el valor
 * es enorme pero sirve: dice que ahí hay un polo, con las raíces a cada lado */
static int evalua1(double u, Eval *ev)
{
    double v[CAL_MAX_INC];
    v[0] = u; v[1] = v[2] = v[3] = 0.0;
    if (evalua(v, ev)) return 1;
    v[0] = u + 1e-9 * (1.0 + fabs(u));
    return evalua(v, ev);
}

/* ==========================================================================
 *  3. Soluciones
 * ======================================================================= */
/* (JᵀJ + λ·diag(JᵀJ) + 1e-14·traza/m·I)·d = -Jᵀr, por Cholesky (m <= 4).
 * Con λ = 0 es Gauss-Newton (Newton puro con tantas ecuaciones como
 * incógnitas); el 1e-14 solo evita dividir por cero si J pierde rango */
static void paso_lm(const Eval *ev, double lam, double *d)
{
    double A[CAL_MAX_INC][CAL_MAX_INC], L[CAL_MAX_INC][CAL_MAX_INC], g[CAL_MAX_INC], y[CAL_MAX_INC], tr = 0.0, t;
    int m = res_.ninc, n = res_.necu, i, j, k;
    for (i = 0; i < m; i++) {
        d[i] = 0.0;
        g[i] = 0.0;
        for (k = 0; k < n; k++) g[i] += ev->J[k][i] * ev->r[k];
        for (j = 0; j < m; j++) {
            A[i][j] = 0.0;
            for (k = 0; k < n; k++) A[i][j] += ev->J[k][i] * ev->J[k][j];
        }
        tr += A[i][i];
    }
    res_.ops += (unsigned long)(2 * n * m * (m + 1) + m * m * m);
    if (!(tr > 0.0) || !FINITO(tr)) return;
    for (i = 0; i < m; i++) A[i][i] += lam * A[i][i] + 1e-14 * tr / (double)m;
    for (i = 0; i < m; i++)
        for (j = 0; j <= i; j++) {
            t = A[i][j];
            for (k = 0; k < j; k++) t -= L[i][k] * L[j][k];
            if (i == j) { if (!(t > 0.0)) return; L[i][i] = sqrt(t); }
            else L[i][j] = t / L[j][j];
        }
    for (i = 0; i < m; i++) {
        t = -g[i];
        for (k = 0; k < i; k++) t -= L[i][k] * y[k];
        y[i] = t / L[i][i];
    }
    for (i = m - 1; i >= 0; i--) {
        t = y[i];
        for (k = i + 1; k < m; k++) t -= L[k][i] * d[k];
        d[i] = t / L[i][i];
    }
}

/* up = u + d dentro del rango (los ángulos de fuente, envueltos); devuelve
 * el paso que de verdad se da, ‖up - u‖∞ */
static double sujeta_paso(const double *u, const double *d, double *up)
{
    double dinf = 0.0;
    int i;
    for (i = 0; i < res_.ninc; i++) {
        double v = u[i] + d[i], dd;
        if (!FINITO(v)) v = u[i];
        if (res_.inc[i].que == IN_ANGU) { v = cx_envuelve(v); dd = cx_envuelve(v - u[i]); }
        else {
            if (v < ulo_[i]) v = ulo_[i];
            if (v > uhi_[i]) v = uhi_[i];
            dd = v - u[i];
        }
        up[i] = v;
        if (fabs(dd) > dinf) dinf = fabs(dd);
    }
    return dinf;
}

/* Condicionamiento de J: √(λmáx/λmín) de JᵀJ por Jacobi, y las dos
 * incógnitas de más peso en el vector propio de λmín (las que los datos no
 * separan), en el orden de inc[] */
static double cond_de(const Eval *ev, int *i1, int *i2)
{
    double A[CAL_MAX_INC][CAL_MAX_INC], V[CAL_MAX_INC][CAL_MAX_INC], lmax, lmin;
    int m = res_.ninc, n = res_.necu, i, j, k, p, q, barrido, imax = 0, imin = 0;
    for (i = 0; i < m; i++)
        for (j = 0; j < m; j++) {
            A[i][j] = 0.0;
            for (k = 0; k < n; k++) A[i][j] += ev->J[k][i] * ev->J[k][j];
            V[i][j] = i == j ? 1.0 : 0.0;
        }
    for (barrido = 0; barrido < 30; barrido++) {
        double fuera = 0.0, diag = 0.0;
        for (p = 0; p < m; p++) {
            diag += A[p][p] * A[p][p];
            for (q = p + 1; q < m; q++) fuera += A[p][q] * A[p][q];
        }
        if (fuera <= 1e-30 * diag) break;
        for (p = 0; p < m; p++)
            for (q = p + 1; q < m; q++) {
                double th, t, cs, sn;
                if (A[p][q] == 0.0) continue;
                th = (A[q][q] - A[p][p]) / (2.0 * A[p][q]);
                t = (th >= 0.0 ? 1.0 : -1.0) / (fabs(th) + sqrt(th * th + 1.0));
                cs = 1.0 / sqrt(t * t + 1.0);
                sn = t * cs;
                for (k = 0; k < m; k++) {
                    double akp = A[k][p], akq = A[k][q];
                    A[k][p] = cs * akp - sn * akq; A[k][q] = sn * akp + cs * akq;
                }
                for (k = 0; k < m; k++) {
                    double apk = A[p][k], aqk = A[q][k];
                    A[p][k] = cs * apk - sn * aqk; A[q][k] = sn * apk + cs * aqk;
                }
                for (k = 0; k < m; k++) {
                    double vkp = V[k][p], vkq = V[k][q];
                    V[k][p] = cs * vkp - sn * vkq; V[k][q] = sn * vkp + cs * vkq;
                }
            }
    }
    res_.ops += (unsigned long)(40 * m * m * m);
    for (i = 1; i < m; i++) {
        if (A[i][i] > A[imax][imax]) imax = i;
        if (A[i][i] < A[imin][imin]) imin = i;
    }
    lmax = A[imax][imax];
    lmin = A[imin][imin];
    /* las dos de más peso en el vector de λmín, en el orden de inc[] */
    *i1 = -1; *i2 = -1;
    for (i = 0; i < m; i++) {
        double a = fabs(V[i][imin]);
        if (*i1 < 0 || a > fabs(V[*i1][imin])) { *i2 = *i1; *i1 = i; }
        else if (*i2 < 0 || a > fabs(V[*i2][imin])) *i2 = i;
    }
    if (*i1 > *i2) { i = *i1; *i1 = *i2; *i2 = i; }
    if (!(lmax > 0.0) || !(lmin > 0.0)) return 1e300;
    return sqrt(lmax / lmin);
}

/* Dos puntos son la misma solución (los ángulos, módulo 2π) */
static int misma(const double *a, const double *b)
{
    int i;
    for (i = 0; i < res_.ninc; i++) {
        double d = a[i] - b[i];
        if (res_.inc[i].que == IN_ANGU || res_.inc[i].que == IN_ANGZ) d = cx_envuelve(d);
        if (!(fabs(d) <= 1e-7)) return 0;
    }
    return 1;
}

/* Una raíz doble (el dato toca la curva sin cruzarla: E/I = R justo en un
 * RLC) no es un punto sino un valle plano de ancho √CAL_TOL: los arranques
 * convergen a puntos distintos del mismo valle. Dos puntos a menos de 1e-3
 * (en u) cuyo punto medio también cuadra son la misma solución; dos raíces
 * de verdad así de cerca no cuadran en el medio (el residuo crece con el
 * cuadrado de la distancia) */
static Eval ev_medio_;
static int doble(const double *a, const double *b)
{
    double u[CAL_MAX_INC];
    int i;
    for (i = 0; i < res_.ninc; i++) {
        double d = a[i] - b[i];
        if (res_.inc[i].que == IN_ANGU || res_.inc[i].que == IN_ANGZ) d = cx_envuelve(d);
        if (!(fabs(d) <= 1e-3)) return 0;
        u[i] = b[i] + 0.5 * d;
    }
    return evalua(u, &ev_medio_) && ev_medio_.rinf <= CAL_TOL;
}

/* De u a lo que se ve: Ω, F, H, Hz, V o A eficaces; los ángulos en grados */
static double fisico(int i, double u)
{
    if (res_.inc[i].log) return exp(u);
    return (res_.inc[i].que == IN_ANGU ? cx_envuelve(u) : u) * 180.0 / PI_K;
}

/* Una solución nueva, en su sitio (por x[0]); con más de CAL_MAX_SOL se
 * quedan las de x[0] menor y se marca "mas" */
static void inserta(Eval *ev, double cond)
{
    unsigned long o0 = ca_ops();
    double x[CAL_MAX_INC];
    int i, pos, n = res_.nsol, k;
    CalSol *s;

    for (i = 0; i < CAL_MAX_INC; i++) x[i] = i < res_.ninc ? fisico(i, ev->u[i]) : 0.0;
    for (pos = 0; pos < n; pos++) if (x[0] < res_.sol[pos].x[0]) break;
    if (n >= CAL_MAX_SOL) {
        res_.mas = 1;
        if (pos >= CAL_MAX_SOL) return;
        n = CAL_MAX_SOL - 1;
    }
    for (i = n; i > pos; i--) res_.sol[i] = res_.sol[i - 1];
    s = &res_.sol[pos];
    memset(s, 0, sizeof *s);
    for (i = 0; i < CAL_MAX_INC; i++) s->x[i] = x[i];
    s->resid = ev->rinf;
    s->cond = cond;
    /* el circuito resuelto con ella: la x de su evaluación */
    pon_u(ev->u);
    ca_valores(&red_, &exc_, ev->x, f_, &s->g);
    s->g.d = res_.d;
    s->g.d.estado = SIM_OK;
    for (i = 0; i < res_.ninc; i++) {
        k = res_.inc[i].el;
        if (k >= 0) s->g.buscado |= 1u << k;
    }
    res_.nsol = n + 1;
    res_.ops += ca_ops() - o0;
}

static void anota_borde(int i, double dir)
{
    if (borde_) return;
    borde_ = 1;
    borde_i_ = i;
    borde_e_ = 0;
    borde_dir_ = dir;
}

/* ¿Una "solución" en la asíntota? Cuando el dato es justo el valor al que
 * se tiende con u -> ±∞, el residuo llega a 0 en coma flotante mucho antes
 * del tope. Se reconoce porque la incógnita apenas cuenta (su columna es
 * casi nula) y, movida 2 décadas hacia un lado, los datos siguen cuadrando.
 * Una raíz doble (el dato es justo un máximo) también tiene la columna nula,
 * pero ahí el residuo crece enseguida a los dos lados. Hasta dos
 * evaluaciones más, solo en estos casos raros */
static int plano(const Eval *ev, int i, double *dir)
{
    double u2[CAL_MAX_INC], mejor = -1.0, cmax = 0.0;
    int j, lado;
    for (j = 0; j < res_.necu; j++) if (fabs(ev->J[j][i]) > cmax) cmax = fabs(ev->J[j][i]);
    if (cmax > 1e-6) return 0;
    for (lado = -1; lado <= 1; lado += 2) {
        for (j = 0; j < res_.ninc; j++) u2[j] = ev->u[j];
        u2[i] += 2.0 * LN10_K * (double)lado;
        if (u2[i] < ulo_[i]) u2[i] = ulo_[i];
        if (u2[i] > uhi_[i]) u2[i] = uhi_[i];
        if (u2[i] == ev->u[i]) continue;
        if (evalua(u2, &ev_) && ev_.rinf <= CAL_TOL && (mejor < 0.0 || ev_.rinf < mejor)) {
            mejor = ev_.rinf;
            *dir = (double)lado;
        }
    }
    return mejor >= 0.0;
}

/* Un punto que convergió: ¿es una solución? Si su residuo no cuadra, no;
 * pegado a un tope de un valor, o en su asíntota (el valor tiende a 0 o a
 * ∞), se anota para CAL_BORDE; con J casi sin rango, para CAL_INDET; si ya
 * se tenía, nada */
static void candidato(Eval *ev)
{
    double cond = 1.0, dir;
    int i, i1 = 0, i2 = 1;
    if (!(ev->rinf <= CAL_TOL)) return;
    for (i = 0; i < res_.ninc; i++) {
        if (res_.inc[i].que == IN_ANGU) ev->u[i] = cx_envuelve(ev->u[i]);
        if (res_.inc[i].log && (ev->u[i] <= ulo_[i] + 1e-9 || ev->u[i] >= uhi_[i] - 1e-9)) {
            anota_borde(i, ev->u[i] <= ulo_[i] + 1e-9 ? -1.0 : 1.0);
            return;
        }
    }
    for (i = 0; i < res_.ninc; i++)
        if (res_.inc[i].log && plano(ev, i, &dir)) { anota_borde(i, dir); return; }
    if (res_.ninc >= 2) {
        cond = cond_de(ev, &i1, &i2);
        if (cond > COND_MAX) {
            if (!indet_) { indet_ = 1; indet_i1_ = i1; indet_i2_ = i2; }
            return;
        }
    }
    for (i = 0; i < ndist_; i++) if (misma(dist_u_[i], ev->u) || doble(dist_u_[i], ev->u)) return;
    if (ndist_ < DISTINTAS) {
        for (i = 0; i < CAL_MAX_INC; i++) dist_u_[ndist_][i] = i < res_.ninc ? ev->u[i] : 0.0;
        ndist_++;
    }
    inserta(ev, cond);
}

/* ==========================================================================
 *  4. Tareas por trozos: LM, raíz con salvaguarda (rtsafe), cruce por cero
 *     (Illinois) y mínimo de |r| (sección áurea). Una tarea puede dejar
 *     otras en una pila (las raíces que encuentra); cada llamada a paso_*
 *     es un paso de la máquina.
 * ======================================================================= */
enum { FA_FIN = 0, FA_UNA, FA_MALLA, FA_BARRE, FA_ARRANQUES, FA_TAREA };
enum { T_LM = 1, T_RAIZ, T_CERO, T_MIN };

typedef struct {
    unsigned char tipo;                     /* T_*                                           */
    double u[CAL_MAX_INC];                  /* T_LM: el arranque                             */
    double a, b, ra, rb;                    /* el intervalo y r en sus extremos              */
    double xa, ga, gb;                      /* T_CERO: la referencia y la función en a y b   */
    double c, rc;                           /* T_MIN: el punto de la malla y su r            */
} Tarea;

static struct {
    int    fase, vuelta;                    /* FA_*; a cuál se vuelve tras las tareas        */
    int    unod;                            /* una incógnita y una ecuación: raíces          */
    int    circular;                        /* la malla da la vuelta (∠E)                    */
    int    nm, im;                          /* malla: puntos y el siguiente                  */
    int    ib, sub;                         /* barrido: el punto (0) o el intervalo (1)      */
    int    ia;                              /* el siguiente arranque                         */
    int    np;
    Tarea  pila[PILA_MAX];
    Tarea  t;                               /* la tarea en curso y su estado                 */
    int    paso, it, npul, lado;
    double lam, xl, xh, rts, dx, dxold, ga, gb, xm, fm;
} M;

/* La malla de una incógnita */
static double gu_[MALLA_MAX], gr_[MALLA_MAX], grn_[MALLA_MAX], gaux_[MALLA_MAX], gdato_[MALLA_MAX];
static unsigned char gok_[MALLA_MAX];

static void empuja(const Tarea *t)
{
    if (M.np < PILA_MAX) M.pila[M.np++] = *t;
}

static void empieza(const Tarea *t)
{
    if (M.fase != FA_TAREA) M.vuelta = M.fase;
    M.fase = FA_TAREA;
    M.t = *t;
    M.paso = 0;
    M.it = 0;
    M.npul = 0;
}

static void acaba_tarea(void)
{
    if (M.np > 0) { Tarea t = M.pila[--M.np]; empieza(&t); }
    else M.fase = M.vuelta;
}

static Tarea tarea(int tipo, double a, double b, double ra, double rb)
{
    Tarea t;
    memset(&t, 0, sizeof t);
    t.tipo = (unsigned char)tipo;
    t.a = a; t.b = b; t.ra = ra; t.rb = rb;
    t.u[0] = a;
    return t;
}

/* ¿Cambia de signo r entre dos puntos? En un residuo de ángulo, un cambio
 * con |ra| + |rb| >= π es el salto de la envoltura, no una raíz */
static int cambio(double ra, double rb)
{
    if (!((ra < 0.0 && rb > 0.0) || (ra > 0.0 && rb < 0.0))) return 0;
    return !angular(res_.ecu[0].que) || fabs(ra) + fabs(rb) < PI_K;
}

/* Pulido: dos pasos de Gauss-Newton (Newton puro si hay tantas ecuaciones
 * como incógnitas) que se aceptan si no empeoran; y a juzgar */
static void pule(void)
{
    double d[CAL_MAX_INC], up[CAL_MAX_INC];
    if (M.npul >= 2) { candidato(&cur_); acaba_tarea(); return; }
    M.npul++;
    paso_lm(&cur_, 0.0, d);
    if (sujeta_paso(cur_.u, d, up) == 0.0) return;
    if (evalua(up, &ev_) && ev_.rn <= cur_.rn) cur_ = ev_;
}

static void empieza_pulido(void) { M.paso = 99; M.npul = 0; }

/* Levenberg-Marquardt desde t.u */
static void paso_lm_tarea(void)
{
    double d[CAL_MAX_INC], up[CAL_MAX_INC], dinf;
    if (M.paso == 0) {
        if (!evalua(M.t.u, &cur_)) {
            /* un arranque singular (las semillas de L y C resuenan entre sí):
             * un pelo al lado, como en la malla de una incógnita */
            int i;
            for (i = 0; i < res_.ninc; i++) M.t.u[i] += 1e-9 * (1.0 + fabs(M.t.u[i]));
            if (!evalua(M.t.u, &cur_)) { acaba_tarea(); return; }
        }
        M.lam = 1e-3;
        M.paso = 1;
        return;
    }
    if (M.it >= LM_ITER || M.lam > 1e12) {                 /* no convergió: ¿casi? */
        if (cur_.rinf <= CAL_TOL) empieza_pulido();
        else acaba_tarea();
        return;
    }
    paso_lm(&cur_, M.lam, d);
    dinf = sujeta_paso(cur_.u, d, up);
    if (dinf <= TOL_D && cur_.rinf <= TOL_R) { empieza_pulido(); return; }
    M.it++;
    if (dinf <= 1e-14) {                                   /* quieto: un mínimo o un tope */
        if (cur_.rinf <= CAL_TOL) empieza_pulido();
        else acaba_tarea();
        return;
    }
    if (evalua(up, &ev_) && ev_.rn < cur_.rn) {
        cur_ = ev_;
        M.lam = M.lam / 10.0 > 1e-12 ? M.lam / 10.0 : 1e-12;
        if (cur_.rinf <= TOL_R && dinf <= TOL_D) empieza_pulido();
    } else M.lam *= 10.0;
}

/* Raíz de r entre a y b (r cambia de signo): Newton con salvaguarda de
 * bisección (rtsafe), con la derivada exacta */
static void paso_raiz(void)
{
    double fr, df;
    if (M.paso == 0) {
        if (M.t.ra < 0.0) { M.xl = M.t.a; M.xh = M.t.b; } else { M.xl = M.t.b; M.xh = M.t.a; }
        M.rts = 0.5 * (M.t.a + M.t.b);
        M.dxold = fabs(M.t.b - M.t.a);
        M.dx = M.dxold;
        if (!evalua1(M.rts, &cur_)) { acaba_tarea(); return; }
        if (cur_.r[0] < 0.0) M.xl = M.rts; else M.xh = M.rts;
        M.paso = 1;
        return;
    }
    fr = cur_.r[0];
    df = cur_.J[0][0];
    if (fr == 0.0 || M.it >= 100) { empieza_pulido(); return; }
    if ((((M.rts - M.xh) * df - fr) * ((M.rts - M.xl) * df - fr) > 0.0) || fabs(2.0 * fr) > fabs(M.dxold * df)) {
        M.dxold = M.dx; M.dx = 0.5 * (M.xh - M.xl); M.rts = M.xl + M.dx;     /* bisección */
    } else {
        M.dxold = M.dx; M.dx = fr / df; M.rts -= M.dx;                       /* Newton */
    }
    M.it++;
    if (!evalua1(M.rts, &ev_)) { acaba_tarea(); return; }
    cur_ = ev_;
    if (fabs(M.dx) <= 1e-14 * (1.0 + fabs(M.rts))) { empieza_pulido(); return; }
    if (cur_.r[0] < 0.0) M.xl = M.rts; else M.xh = M.rts;
}

/* La función cuyo cero se busca en un cruce: con un módulo, X proyectado
 * sobre su dirección en a (en una red real, X cambia de signo); con P, P */
static double g_cero(const Eval *ev)
{
    if (res_.ecu[0].que == EC_MOD) return ev->dato * cos(ev->aux - M.t.xa);
    return ev->aux * M.t.xa;                               /* EC_P: xa = el signo de P en a */
}

/* Un cruce por cero de X (o de P) entre a y b: ahí |X| se hunde y puede
 * haber dos raíces juntas que la malla no ve. Regula falsi (Illinois) para
 * el cero; después, las raíces a cada lado */
static void paso_cero(void)
{
    double c, gc, lo, hi;
    if (M.paso == 0) {
        M.xl = M.t.a; M.xh = M.t.b; M.ga = M.t.ga; M.gb = M.t.gb;
        M.lado = 0;
        M.paso = 1;
    }
    lo = M.xl < M.xh ? M.xl : M.xh;
    hi = M.xl < M.xh ? M.xh : M.xl;
    c = (M.xl * M.gb - M.xh * M.ga) / (M.gb - M.ga);
    if (!(c > lo && c < hi)) c = 0.5 * (M.xl + M.xh);
    if (!evalua1(c, &ev_)) { acaba_tarea(); return; }
    gc = g_cero(&ev_);
    M.it++;
    if (gc > 0.0) { M.xl = c; M.ga = gc; if (M.lado > 0) M.gb *= 0.5; M.lado = 1; }
    else if (gc < 0.0) { M.xh = c; M.gb = gc; if (M.lado < 0) M.ga *= 0.5; M.lado = -1; }
    if (gc == 0.0 || fabs(M.xh - M.xl) <= 1e-13 * (1.0 + fabs(M.xl)) || M.it >= 60) {
        double rz = ev_.r[0], a = M.t.a, b = M.t.b, ra = M.t.ra, rb = M.t.rb;
        Tarea t;
        acaba_tarea();                                     /* las raíces van a la pila */
        if (cambio(ra, rz)) { t = tarea(T_RAIZ, a, c, ra, rz); if (M.fase == FA_TAREA) empuja(&t); else empieza(&t); }
        if (cambio(rz, rb)) { t = tarea(T_RAIZ, c, b, rz, rb); if (M.fase == FA_TAREA) empuja(&t); else empieza(&t); }
    }
}

/* Un mínimo de |r| sin cambio de signo en la malla: una raíz doble (el dato
 * es justo un máximo), o dos raíces juntas entre dos puntos de la malla,
 * que es lo que pasa en una resonancia estrecha: el pico cae entre dos
 * puntos y la malla lo ve mucho más bajo. Sección áurea sobre |r| entre los
 * vecinos; en cuanto r cambia de signo, las dos raíces a cada lado. Si
 * converge sin cambiar de signo y |r| es pequeño, LM (la raíz doble) */
static void paso_min(void)
{
    double u, ru, s0 = M.t.rc < 0.0 ? -1.0 : 1.0;
    if (M.paso == 0) {
        M.xl = M.t.a; M.xh = M.t.b;
        M.xm = M.t.c; M.fm = fabs(M.t.rc);
        M.paso = 1;
    }
    if (M.it >= 40 || M.xh - M.xl <= 1e-9 * (1.0 + fabs(M.xm))) {
        double xm = M.xm, fm = M.fm;
        acaba_tarea();
        if (fm < MIN_R) {
            Tarea t = tarea(T_LM, xm, 0.0, 0.0, 0.0);
            if (M.fase == FA_TAREA) empuja(&t); else empieza(&t);
        }
        return;
    }
    u = M.xh - M.xm > M.xm - M.xl ? M.xm + 0.3819660112501051 * (M.xh - M.xm)
                                  : M.xm - 0.3819660112501051 * (M.xm - M.xl);
    M.it++;
    if (!evalua1(u, &ev_)) { acaba_tarea(); return; }
    ru = ev_.r[0];
    if (cambio(s0 * M.fm, ru)) {                       /* dos raíces: a cada lado de u */
        double xm = M.xm, rm = s0 * M.fm, fin = u > xm ? M.t.b : M.t.a, rfin = u > xm ? M.t.rb : M.t.ra;
        Tarea t1 = tarea(T_RAIZ, xm, u, rm, ru), t2 = tarea(T_RAIZ, u, fin, ru, rfin);
        acaba_tarea();
        if (M.fase == FA_TAREA) empuja(&t1); else empieza(&t1);
        if (cambio(ru, rfin)) empuja(&t2);
        return;
    }
    if (fabs(ru) < M.fm) {
        if (u > M.xm) M.xl = M.xm; else M.xh = M.xm;
        M.xm = u; M.fm = fabs(ru);
    } else if (u > M.xm) M.xh = u;
    else M.xl = u;
}

static void paso_tarea(void)
{
    if (M.paso == 99) { pule(); return; }
    switch (M.t.tipo) {
    case T_LM:     paso_lm_tarea(); break;
    case T_RAIZ:   paso_raiz(); break;
    case T_CERO:   paso_cero(); break;
    case T_MIN:    paso_min(); break;
    default:       acaba_tarea(); break;
    }
}

/* ==========================================================================
 *  5. Malla, arranques y diagnóstico
 * ======================================================================= */
/* Las frecuencias características de los pares de elementos conocidos
 * (1/(2π√(LC)), R/(2πL), 1/(2πRC); 16 como mucho): 21 puntos en ±0,2
 * décadas alrededor de cada una, donde una resonancia estrecha esconde dos
 * raíces entre dos puntos de la malla normal */
static int caracteristicas(int n)
{
    int i, j, t, k, nc = 0;
    for (t = 0; t < 3; t++)
        for (i = 0; i < red0_.nel; i++)
            for (j = 0; j < red0_.nel && nc < 16; j++) {
                const SimEl *a = &red0_.el[i], *b = &red0_.el[j];
                double fc;
                if (a->instr || b->instr) continue;
                if (t == 0 && a->tipo == SE_L && b->tipo == SE_C) fc = 1.0 / (2.0 * PI_K * sqrt(a->val * b->val));
                else if (t == 1 && a->tipo == SE_R && b->tipo == SE_L) fc = a->val / (2.0 * PI_K * b->val);
                else if (t == 2 && a->tipo == SE_R && b->tipo == SE_C) fc = 1.0 / (2.0 * PI_K * a->val * b->val);
                else continue;
                if (!(fc > 0.0) || !FINITO(fc)) continue;
                nc++;
                for (k = -10; k <= 10; k++) {
                    double u = log(fc) + 0.02 * (double)k * LN10_K;
                    if (u >= ulo_[0] && u <= uhi_[0] && n < MALLA_MAX) gu_[n++] = u;
                }
            }
    return n;
}

static void arma_malla(void)
{
    const CalInc *in = &res_.inc[0];
    int k, n = 0, i, j;
    M.circular = 0;
    switch (in->que) {
    case IN_ANGU:                                /* toda la vuelta, cada 5°: circular */
        for (k = 0; k < 72; k++) gu_[n++] = (-175.0 + 5.0 * (double)k) * PI_K / 180.0;
        M.circular = 1;
        break;
    case IN_ANGZ:                                /* de -90° a 90°, cada 5° */
        for (k = 0; k <= 36; k++) gu_[n++] = (-90.0 + 5.0 * (double)k) * PI_K / 180.0;
        break;
    default: {                                   /* 13 décadas alrededor de la semilla */
        double u0 = log(in->semilla);
        for (k = -65; k <= 65; k++) {
            double u = u0 + 0.1 * (double)k * LN10_K;
            if (u >= ulo_[0] && u <= uhi_[0]) gu_[n++] = u;
        }
        if (in->que == IN_F) n = caracteristicas(n);
        for (i = 1; i < n; i++) {                /* en orden y sin repetidos */
            double t = gu_[i];
            for (j = i; j > 0 && gu_[j - 1] > t; j--) gu_[j] = gu_[j - 1];
            gu_[j] = t;
        }
        for (i = 1, j = 1; i < n; i++) if (gu_[i] - gu_[j - 1] > 1e-9) gu_[j++] = gu_[i];
        if (n > 1) n = j;
        break;
    }
    }
    M.nm = n;
}

static void termina(void);

static void paso_malla(void)
{
    int i = M.im;
    gok_[i] = (unsigned char)evalua1(gu_[i], &ev_);
    if (gok_[i]) { gu_[i] = ev_.u[0]; gr_[i] = ev_.r[0]; grn_[i] = ev_.rn; gaux_[i] = ev_.aux; gdato_[i] = ev_.dato; }
    M.im++;
    res_.hechos = M.im;
    if (M.im >= M.nm) {
        /* si el valor no cambia nada en toda la malla, no hay nada que buscar */
        if (!una_ok_ || !col_viva_[0]) { termina(); return; }
        M.fase = FA_BARRE;
        M.ib = 0;
        M.sub = 0;
    }
}

/* Un cruce por cero de X (o de P) entre dos puntos de la malla */
static int cruce(int i, int n)
{
    const CalEcu *q = &res_.ecu[0];
    if (!M.unod) return 0;
    if (q->que == EC_MOD) return fabs(cx_envuelve(gaux_[n] - gaux_[i])) > 0.5 * PI_K;
    if (q->que == EC_P && q->v > 0.0) return gaux_[i] * gaux_[n] < 0.0;
    return 0;
}

/* u del vecino n de i, desenvuelto si la malla da la vuelta */
static double u_vecino(int i, int n)
{
    if (M.circular && n < i && i - n > 1) return gu_[n] + 2.0 * PI_K;
    if (M.circular && n > i && n - i > 1) return gu_[n] - 2.0 * PI_K;
    return gu_[n];
}

/* Recorre la malla punto a punto e intervalo a intervalo y lanza las
 * tareas: raíces donde r cambia de signo, cruces por cero, mínimos */
static void paso_barre(void)
{
    int i = M.ib, n = M.nm, prev, next;
    Tarea t;
    if (i >= n) { termina(); return; }
    prev = i > 0 ? i - 1 : (M.circular ? n - 1 : -1);
    next = i < n - 1 ? i + 1 : (M.circular ? 0 : -1);
    if (M.sub == 0) {                                      /* el punto: ¿un mínimo? */
        int okp = prev >= 0 && gok_[prev], okn = next >= 0 && gok_[next];
        M.sub = 1;
        if (!gok_[i]) return;
        if (!M.unod) {                                     /* varias ecuaciones: mínimos de ‖r‖ */
            if ((!okp || grn_[i] < grn_[prev]) && (!okn || grn_[i] <= grn_[next])) {
                t = tarea(T_LM, gu_[i], 0.0, 0.0, 0.0);
                empieza(&t);
            }
            return;
        }
        if (okp && okn) {                                  /* dentro: sección áurea */
            if (fabs(gr_[i]) < fabs(gr_[prev]) && fabs(gr_[i]) <= fabs(gr_[next]) &&
                !cambio(gr_[prev], gr_[i]) && !cambio(gr_[i], gr_[next])) {
                t = tarea(T_MIN, u_vecino(i, prev), u_vecino(i, next), gr_[prev], gr_[next]);
                t.c = gu_[i];
                t.rc = gr_[i];
                empieza(&t);
            }
            return;
        }
        if (!(fabs(gr_[i]) < MIN_R)) return;
        if (okp || okn) {                                  /* un extremo: la raíz puede estar más allá */
            int o = okp ? prev : next;
            if (fabs(gr_[i]) < fabs(gr_[o]) && !cambio(gr_[i], gr_[o]) && !cruce(i, o)) {
                t = tarea(T_LM, gu_[i], 0.0, 0.0, 0.0);
                empieza(&t);
            }
        } else {
            t = tarea(T_LM, gu_[i], 0.0, 0.0, 0.0);
            empieza(&t);
        }
        return;
    }
    M.sub = 0;                                             /* el intervalo (i, next) */
    M.ib++;
    if (!M.unod || next < 0 || !gok_[i] || !gok_[next]) return;
    if (cambio(gr_[i], gr_[next])) {
        t = tarea(T_RAIZ, gu_[i], u_vecino(i, next), gr_[i], gr_[next]);
        empieza(&t);
    } else if (cruce(i, next)) {
        t = tarea(T_CERO, gu_[i], u_vecino(i, next), gr_[i], gr_[next]);
        if (res_.ecu[0].que == EC_MOD) {
            t.xa = gaux_[i];
            t.ga = gdato_[i];
            t.gb = gdato_[next] * cos(gaux_[next] - gaux_[i]);
        } else {
            t.xa = gaux_[i] < 0.0 ? -1.0 : 1.0;
            t.ga = gaux_[i] * t.xa;
            t.gb = gaux_[next] * t.xa;
        }
        empieza(&t);
    }
}

/* Radical inverso en base b (los puntos de Halton de 4 incógnitas) */
static double halton(int i, int b)
{
    double f = 1.0, r = 0.0;
    while (i > 0) { f /= (double)b; r += f * (double)(i % b); i /= b; }
    return r;
}

/* El arranque s: con 2 y 3 incógnitas, la semilla por 10^{0, -1, +1} en
 * cada una (los ángulos en {0, -π/3, π/3}; ∠Z en {0, -π/4, π/4}); con 4,
 * 16 puntos de Halton en ±2 décadas. El primero es la semilla */
static void arranque_u(int s, double *u)
{
    static const int BASE[CAL_MAX_INC] = { 2, 3, 5, 7 };
    static const double H5[5] = { 0.0, -1.0, 1.0, -0.5, 0.5 };
    int m = res_.ninc, i, p = 1, h5[2] = { 0, 0 };
    if (m == 2 && s >= 9) {
        /* con 2 incógnitas, 25 arranques: tras los 9 de antes, los 16 que
         * faltan de la malla de 5×5 en medias décadas (alguno en ±0,5). Nueve
         * no bastaban: un LC serie con L y C (o una Z capacitiva) buscadas
         * perdía una de sus dos soluciones */
        int t = s - 9, n = 0, a, b;
        for (a = 0; a < 5; a++)
            for (b = 0; b < 5; b++) {
                if (a < 3 && b < 3) continue;
                if (n++ == t) { h5[0] = a; h5[1] = b; }
            }
    }
    for (i = 0; i < m; i++) {
        const CalInc *in = &res_.inc[i];
        double h, dec = 1.0;
        if (m == 4) { h = 2.0 * halton(s + 1, BASE[i]) - 1.0; dec = 2.0; }
        else if (m == 2 && s >= 9) h = H5[h5[i]];
        else {
            int dig = (s / p) % 3;
            h = dig == 0 ? 0.0 : dig == 1 ? -1.0 : 1.0;
            p *= 3;
        }
        switch (in->que) {
        case IN_ANGU: u[i] = h * PI_K / 3.0; break;
        case IN_ANGZ: u[i] = h * PI_K / 4.0; break;
        default:
            u[i] = log(in->semilla) + h * dec * LN10_K;
            /* las semillas de L (Z0/ω) y de C (1/(ω·Z0)) resuenan entre sí, y con
             * los mismos saltos de década 3 de los 9 arranques caerían justo en
             * la resonancia: la C va un factor 1,7 (no una potencia de 10) al lado */
            if (in->que == IN_VAL && in->el >= 0 && in->el < red_.nel &&
                (red_.el[in->el].tipo == SE_C || (red_.el[in->el].tipo == SE_Z && !(angz_[in->el] > 0.0))))
                u[i] += 0.53062825106217040;              /* ln 1,7: la C o la Z que no es inductiva */
            if (u[i] < ulo_[i]) u[i] = ulo_[i];
            if (u[i] > uhi_[i]) u[i] = uhi_[i];
            break;
        }
    }
}

static void paso_arranque(void)
{
    Tarea t;
    if (M.ia >= res_.arranques) { termina(); return; }
    memset(&t, 0, sizeof t);
    t.tipo = T_LM;
    arranque_u(M.ia, t.u);
    M.ia++;
    res_.hechos = M.ia;
    empieza(&t);
}

/* El error de la ecuación k en lo más cerca que se llegó, como se ve */
static double peor_de(int k)
{
    const CalEcu *q = &res_.ecu[k];
    double r = mejor_r_[k], th, fp, fpd;
    int j;
    switch (q->que) {
    case EC_MOD: return fabs(exp(r) - 1.0);
    case EC_ANG: return fabs(r) * 180.0 / PI_K;
    case EC_P:   return q->v > 0.0 ? fabs(r) : fabs(r * q->esc);
    case EC_FP:
        th = r + q->v;
        fp = (th < 0.0 ? -1.0 : 1.0) * cos(th);
        fpd = (q->v < 0.0 ? -1.0 : 1.0) * cos(q->v);
        return fabs(fp - fpd);
    default:                                     /* EC_RE y EC_IM: el módulo eficaz */
        j = q->que == EC_RE ? k + 1 : k - 1;
        if (j < 0 || j >= res_.necu) j = k;
        return q->esc * sqrt(r * r + (j != k ? mejor_r_[j] * mejor_r_[j] : 0.0)) / RAIZ2_K;
    }
}

/* INCOMPAT con un ángulo o un fp: el rango sobre la malla EN ORDEN. El
 * ángulo se desenrolla (un ∠ que va de 170° a 260° cruza ±180°: su mínimo y
 * su máximo sin desenrollar darían casi toda la vuelta); del fp se lleva φ,
 * continuo en [-90°, 90°], porque el fp con signo salta de +1 a -1 al pasar
 * por resistivo. lo_dato y hi_dato: el ∠ en grados (hi puede pasar de 180);
 * con fp, φ en radianes (+ inductivo) */
static void rango_angular(void)
{
    static unsigned short ord[MALLA_MAX];
    int i, j, n = 0, hay = 0, ang = res_.ecu[0].que == EC_ANG;
    double acc = 0.0, ant = 0.0, lo = 0.0, hi = 0.0;
    for (i = 0; i < M.nm && i < MALLA_MAX; i++) {
        if (!gok_[i]) continue;
        for (j = n; j > 0 && gu_[ord[j - 1]] > gu_[i]; j--) ord[j] = ord[j - 1];
        ord[j] = (unsigned short)i;
        n++;
    }
    for (j = 0; j < n; j++) {
        double a = gaux_[ord[j]];
        if (!hay) { acc = a; hay = 1; }
        else acc += ang ? cx_envuelve(a - ant) : a - ant;
        ant = a;
        if (j == 0 || acc < lo) lo = acc;
        if (j == 0 || acc > hi) hi = acc;
    }
    if (ang) {
        double w = hi - lo;
        if (w >= 2.0 * PI_K) { lo = -PI_K; w = 2.0 * PI_K; }
        lo = cx_envuelve(lo);                    /* el principio en (-180°, 180°] */
        res_.lo_dato = lo * 180.0 / PI_K;
        res_.hi_dato = (lo + w) * 180.0 / PI_K;
    } else { res_.lo_dato = lo; res_.hi_dato = hi; }
}

/* SIN_EFECTO: ¿el elemento lleva corriente? Su valor es incógnita, así que
 * se mira con él mil veces más chico y mil veces más grande (una R en
 * paralelo con una E lleva corriente con cualquier valor; una en serie con
 * un voltímetro ideal, con ninguno): el mayor |I|/|I|máx de la red de las
 * dos (0 = no lleva); -1 si no se sabe */
static double sin_corriente(int k)
{
    double peor = -1.0, u[CAL_MAX_INC];
    int i, j, q, m = -1;
    for (i = 0; i < res_.ninc; i++) if (res_.inc[i].el == k && res_.inc[i].que == IN_VAL) m = i;
    if (k < 0 || k >= red_.nel || m < 0) return -1.0;
    for (j = 0; j < 2; j++) {
        double imax = 0.0, ik = 0.0;
        Cx V, I;
        for (i = 0; i < res_.ninc; i++) u[i] = ev_.u[i];
        u[m] = log(res_.inc[m].semilla) + (j ? 3.0 : -3.0) * LN10_K;
        if (!evalua(u, &ev_)) return -1.0;
        for (q = 0; q < red_.nel; q++) {
            double a;
            ca_vi(&red_, &exc_, ev_.x, f_, q, &V, &I);
            a = cx_abs(I);
            if (q == k) ik = a;
            if (a > imax) imax = a;
        }
        if (!(imax > 0.0)) return -1.0;
        if (ik / imax > peor) peor = ik / imax;
    }
    return peor;
}

static void termina(void)
{
    int i, k;
    M.fase = FA_FIN;
    if (res_.estado != CAL_OK) return;
    if (!una_ok_) {                              /* el modelo directo no se pudo resolver */
        unsigned av = res_.d.avisos;
        res_.estado = CAL_SIM;
        res_.d = fallo_;
        res_.d.avisos |= av;
        res_.elem = fallo_.elem;
        res_.nsol = 0;
        return;
    }
    /* antes de aceptar raíces: una incógnita que no mueve nada "cumple" los
     * datos con cualquier valor */
    for (i = 0; i < res_.ninc; i++)
        if (!col_viva_[i]) {
            res_.estado = CAL_SIN_EFECTO;
            res_.elem = res_.inc[i].el;
            res_.nsol = 0;
            res_.mas = 0;
            res_.peor = sin_corriente(res_.elem);
            return;
        }
    if (res_.nsol > 0) return;
    if (indet_) { res_.estado = CAL_INDET; res_.elem = indet_i1_; res_.iecu = indet_i2_; return; }
    if (borde_) { res_.estado = CAL_BORDE; res_.elem = borde_i_; res_.iecu = borde_e_; res_.peor = borde_dir_; return; }
    res_.estado = CAL_INCOMPAT;
    if (M.unod) {                                /* lo que el dato puede marcar */
        res_.iecu = 0;
        if (angular(res_.ecu[0].que)) rango_angular();
        else { res_.lo_dato = dlo_; res_.hi_dato = dhi_; }
        return;
    }
    k = 0;
    for (i = 1; i < res_.necu; i++) if (fabs(mejor_r_[i]) > fabs(mejor_r_[k])) k = i;
    res_.iecu = k;
    res_.peor = res_.necu > 0 ? peor_de(k) : 0.0;
}

static void corta(void)
{
    res_.estado = CAL_CORTADO;
    M.fase = FA_FIN;
}

static void un_paso(void)
{
    double u0[CAL_MAX_INC] = { 0.0, 0.0, 0.0, 0.0 };
    switch (M.fase) {
    case FA_UNA:                                 /* sin incógnitas: se resuelve y se comprueba */
        if (evalua(u0, &cur_)) candidato(&cur_);
        termina();
        break;
    case FA_MALLA:     paso_malla(); break;
    case FA_BARRE:     paso_barre(); break;
    case FA_ARRANQUES: paso_arranque(); break;
    case FA_TAREA:     paso_tarea(); break;
    default:           M.fase = FA_FIN; break;
    }
}

/* ==========================================================================
 *  6. La API por trozos
 * ======================================================================= */
static int falla_sim(const SimDiag *d)
{
    res_.estado = CAL_SIM;
    res_.d = *d;
    res_.elem = d->elem;
    M.fase = FA_FIN;
    return res_.estado;
}

int cal_iniciar(const Circuito *c, const Red *r)
{
    SimDiag d;
    unsigned av;
    int i, k, ea;

    memset(&M, 0, sizeof M);
    M.fase = FA_FIN;
    una_ok_ = mejor_hay_ = indet_ = borde_ = dhay_ = 0;
    dlo_ = dhi_ = 0.0;
    ndist_ = 0;
    memset(col_viva_, 0, sizeof col_viva_);
    memset(&fallo_, 0, sizeof fallo_);
    fallo_.elem = fallo_.nodo = fallo_.elem_aviso = -1;
    if (cal_plantea(c, &res_) != CAL_OK) return res_.estado;

    if (sim_red_de_ex(c, r, &red0_, &d, 1) != SIM_OK) return falla_sim(&d);
    frec_busca_ = c->frec.est == M_BUSCAR;
    for (k = 0; k < red0_.nel; k++) {
        const Elemento *e = &c->el[k];
        alterna_[k] = (unsigned char)(es_fuente(e) && e->variante == FORMA_FASOR);
        tocada_[k] = 0;
        if (frec_busca_ && (alterna_[k] || red0_.el[k].tipo == SE_Z)) red0_.el[k].p[1] = res_.f;
        /* los valores de partida de Z y fuentes, del circuito (el adaptador
         * pone 0° a la Z sin módulo) */
        modz_[k] = e->tipo == EL_IMP && e->val.est == M_DATO ? e->val.v : 1e3;
        angz_[k] = e->tipo == EL_IMP && e->val.est_ang == M_DATO ? e->val.ang * PI_K / 180.0 : 0.0;
        modu_[k] = RAIZ2_K * (alterna_[k] && e->val.est == M_DATO ? e->val.v : 1.0);
        angu_[k] = alterna_[k] && e->val.est_ang == M_DATO ? e->val.ang * PI_K / 180.0 : 0.0;
    }
    memset(&d, 0, sizeof d);
    d.elem = d.nodo = d.elem_aviso = -1;
    d.t = -1.0;
    ca_excitacion(&red0_, res_.f, &exc_, &d);
    if (d.estado != SIM_OK) return falla_sim(&d);
    ca_apaga(&red0_, exc_.activa);
    av = d.avisos;
    ea = d.elem_aviso;
    if (ca_topologia(&red0_, &d) != SIM_OK) { d.avisos |= av; return falla_sim(&d); }
    d.avisos |= av;
    if (d.elem_aviso < 0) d.elem_aviso = ea;
    res_.d = d;
    red_ = red0_;

    for (i = 0; i < res_.ninc; i++) {
        const CalInc *in = &res_.inc[i];
        if (in->el >= 0) tocada_[in->el] = 1;
        switch (in->que) {
        case IN_ANGZ: ulo_[i] = -0.5 * PI_K; uhi_[i] = 0.5 * PI_K; break;
        case IN_ANGU: ulo_[i] = -PI_K; uhi_[i] = PI_K; break;
        default: ulo_[i] = log(in->lo); uhi_[i] = log(in->hi); break;
        }
    }
    if (res_.ninc == 0) { M.fase = FA_UNA; res_.arranques = 1; }
    else if (res_.ninc == 1) {
        arma_malla();
        M.unod = res_.necu == 1;
        M.fase = FA_MALLA;
        res_.arranques = M.nm;
    } else {
        res_.arranques = res_.ninc == 2 ? 25 : res_.ninc == 3 ? 27 : 16;
        M.fase = FA_ARRANQUES;
    }
    return res_.estado;
}

int cal_avanzar(unsigned long ops)
{
    unsigned long hecho = 0;
    SimDiag d;
    if (M.fase == FA_FIN) return 0;
    /* la topología de la red de trabajo, por si otro análisis la cambió
     * entre dos trozos (es la misma: no depende de los valores) */
    ca_topologia(&red0_, &d);
    do {
        unsigned long antes = res_.ops;
        if (res_.ops >= CAL_OPS_TOPE) { corta(); break; }
        un_paso();
        hecho += res_.ops - antes;
    } while (M.fase != FA_FIN && hecho < ops);
    return M.fase != FA_FIN;
}

void cal_cortar(void) { if (M.fase != FA_FIN) corta(); }

double cal_progreso(void)
{
    double p;
    if (M.fase == FA_FIN) return 1.0;
    if (res_.ninc == 1 && M.nm > 0) {
        p = M.fase == FA_MALLA ? 0.5 * (double)M.im / (double)M.nm : 0.5 + 0.5 * (double)M.ib / (double)M.nm;
        return p < 0.99 ? p : 0.99;
    }
    if (res_.arranques > 0) return (double)res_.hechos / (double)(res_.arranques + 1);
    return 0.0;
}

const CalRes *cal_res(void) { return &res_; }

/* ==========================================================================
 *  7. Mensajes y aplicar una solución
 * ======================================================================= */
/* El nombre de la incógnita i: "R1", "|Z1|" y "∠Z1" si están las dos, "f" */
static void nombre_inc(Buf *b, const Circuito *c, const CalRes *s, int i)
{
    const CalInc *in;
    int j, doble = 0;
    if (i < 0 || i >= s->ninc || i >= CAL_MAX_INC) { buf_ch(b, '?'); return; }
    in = &s->inc[i];
    if (in->que == IN_F) { buf_ch(b, 'f'); return; }
    for (j = 0; j < s->ninc && j < CAL_MAX_INC; j++) if (j != i && s->inc[j].el == in->el) doble = 1;
    if (in->que == IN_ANGZ || in->que == IN_ANGU) buf_str(b, S_ANG);
    else if (doble) buf_ch(b, '|');
    buf_str(b, (in->el >= 0 && in->el < c->nel) ? c->el[in->el].etiq : "?");
    if (doble && (in->que == IN_VAL || in->que == IN_MODU)) buf_ch(b, '|');
}

/* El nombre del dato: "V1" (la lectura), "V(R1)", "∠I(E1)", "P(W1)" */
static void nombre_ecu(Buf *b, const Circuito *c, const CalEcu *e)
{
    const Elemento *el;
    if (e->el < 0 || e->el >= c->nel) { buf_ch(b, '?'); return; }
    el = &c->el[e->el];
    if (e->que == EC_P || e->que == EC_FP) {
        buf_str(b, e->que == EC_P ? "P(" : "fp(");
        buf_str(b, el->etiq);
        buf_ch(b, ')');
        return;
    }
    if (e->que == EC_ANG) buf_str(b, S_ANG);
    if ((el->tipo == EL_VOLT && e->q == Q_V) || (el->tipo == EL_AMP && e->q == Q_I)) { buf_str(b, el->etiq); return; }
    buf_str(b, e->q == Q_I ? "I(" : "V(");
    buf_str(b, el->etiq);
    buf_ch(b, ')');
}

static const char *unidad_ecu(const CalEcu *e)
{
    if (e->que == EC_P) return " W";
    return e->q == Q_I ? " A" : " V";
}

/* Un valor de lo que marca el dato, como se ve ("130 V", "30°", "0,95") */
static void valor_ecu(Buf *b, const CalEcu *e, double v, int cifras)
{
    char t[32];
    switch (e->que) {
    case EC_ANG: buf_auto(b, v, 1); buf_str(b, S_DEG); break;
    case EC_FP:  buf_auto(b, v, 3); break;
    default:     buf_str(b, num_pref(t, sizeof t, v, cifras, unidad_ecu(e))); break;
    }
}

/* Un fp dado por su φ (radianes, + inductivo): "0,555 ind.", "1" */
static void fp_de_fi(Buf *b, double fi)
{
    if (fabs(fi) < 1e-9) { buf_ch(b, '1'); return; }
    buf_auto(b, cos(fi), 3);
    buf_str(b, fi > 0.0 ? " ind." : " cap.");
}

/* El dato, como se escribió */
static double dato_visto(const CalEcu *e)
{
    switch (e->que) {
    case EC_MOD: return e->v / RAIZ2_K;
    case EC_ANG: return e->v * 180.0 / PI_K;
    case EC_P:   return e->v;
    case EC_FP:  return (e->v < 0.0 ? -1.0 : 1.0) * cos(e->v);
    default:     return 0.0;
    }
}

/* "V1 a un 3,2 % del dato", "∠V(R1) a 5° del dato", "V1 en 0,32 V (el dato es 0)";
 * con queda, "V1 queda a..." */
static void error_ecu(Buf *b, const Circuito *c, const CalRes *s, int queda)
{
    const CalEcu *e = &s->ecu[s->iecu >= 0 && s->iecu < CAL_MAX_ECU ? s->iecu : 0];
    char t[32];
    nombre_ecu(b, c, e);
    if (queda) buf_str(b, " queda");
    switch (e->que) {
    case EC_MOD:
    case EC_P:
        if (e->v > 0.0) {
            double p = 100.0 * s->peor;
            int dec = p >= 1.0 ? 1 : 1 - (int)floor(log10(p > 1e-9 ? p : 1e-9));
            buf_str(b, " a un ");
            buf_auto(b, p, dec > 9 ? 9 : dec);
            buf_str(b, " % del dato");
            break;
        }
        /* P = 0: como un fasor nulo */
        /* fall through */
    case EC_RE:
    case EC_IM:
        buf_str(b, " en ");
        buf_str(b, num_pref(t, sizeof t, s->peor, 3, unidad_ecu(e)));
        buf_str(b, " (el dato es 0)");
        break;
    case EC_ANG:
        buf_str(b, " a ");
        buf_auto(b, s->peor, 1);
        buf_str(b, S_DEG " del dato");
        break;
    default:
        buf_str(b, " a ");
        buf_auto(b, s->peor, 3);
        buf_str(b, " del dato");
        break;
    }
}

/* CAL_SIM: el texto del modelo directo. sim_mensaje_circ mira la forma y la
 * f de las fuentes en la SimRed: se arma una con lo del circuito */
static SimRed red_msg_;
static const char *mensaje_sim(const Circuito *c, const CalRes *s, char *buf, int cap)
{
    int k, q;
    memset(&red_msg_, 0, sizeof red_msg_);
    red_msg_.nel = c->nel;
    for (k = 0; k < c->nel; k++) {
        const Elemento *e = &c->el[k];
        SimEl *x = &red_msg_.el[k];
        int forma = ONDA_CC;
        x->k = (short)k;
        x->tipo = SE_R;
        if (es_fuente(e)) {
            x->tipo = e->tipo == EL_FUENTE ? SE_E : SE_J;
            circ_onda_c(c, e, &forma, x->p, &q);
            x->forma = (unsigned char)forma;
        }
    }
    return sim_mensaje_circ(c, &red_msg_, &s->d, buf, cap);
}

const char *cal_mensaje(const Circuito *c, const CalRes *s, char *buf, int cap)
{
    Buf b;
    int i;
    const Elemento *e = (s->elem >= 0 && s->elem < c->nel) ? &c->el[s->elem] : 0;
    buf_init(&b, buf, (size_t)cap);
    switch (s->estado) {
    case CAL_OK:
        break;
    case CAL_SIM:
        return mensaje_sim(c, s, buf, cap);
    case CAL_FALTAN:
        buf_str(&b, "Faltan datos: ");
        buf_int(&b, s->ninc);
        buf_str(&b, s->ninc == 1 ? " inc" S_oo "gnita (" : " inc" S_oo "gnitas (");
        for (i = 0; i < s->ninc && i < CAL_MAX_INC; i++) { if (i) buf_str(&b, ", "); nombre_inc(&b, c, s, i); }
        buf_str(&b, ") y ");
        if (s->necu == 0) buf_str(&b, "ning" S_uu "n dato");
        else { buf_int(&b, s->necu); buf_str(&b, s->necu == 1 ? " dato" : " datos"); }
        buf_str(&b, ". Un m" S_oo "dulo o un " S_aa "ngulo cuentan 1; un fasor entero, 2.");
        break;
    case CAL_MUCHOS:
        buf_str(&b, "La calculadora de CA despeja hasta 4 valores (con 8 datos como mucho): aqu" S_ii " hay ");
        if (s->ninc > CAL_MAX_INC) { buf_int(&b, s->ninc); buf_ch(&b, '.'); }
        else { buf_int(&b, s->necu); buf_str(&b, " datos (un fasor cuenta 2)."); }
        break;
    case CAL_NO_CA:
        if (e && es_fuente(e) && e->variante == ONDA_SENO) {
            buf_str(&b, "La f la fija "); buf_str(&b, e->etiq);
            buf_str(&b, " (un generador senoidal): no se puede buscar. Quita el [?!] de la f o pasa ");
            buf_str(&b, e->etiq); buf_str(&b, " a alterna.");
        } else if (e && (e->tipo == EL_VOLT || e->tipo == EL_AMP)) {
            buf_str(&b, "En CA no se despeja la R de "); buf_str(&b, e->etiq);
            buf_str(&b, " (ni la r de una fuente): ponla como dato.");
        } else if (e && (e->tipo == EL_EDEP || e->tipo == EL_JDEP)) {
            buf_str(&b, "En CA no se despeja la ganancia de "); buf_str(&b, e->etiq);
            buf_str(&b, ": ponla como dato (en CC, [=] s" S_ii " la despeja).");
        } else {
            buf_str(&b, "En CA no se despeja la r de "); buf_str(&b, e ? e->etiq : "una fuente");
            buf_str(&b, " (ni la R de un instrumento): ponla como dato.");
        }
        break;
    case CAL_INDET:
        buf_str(&b, "Los datos no separan ");
        nombre_inc(&b, c, s, s->elem);
        buf_str(&b, " y ");
        nombre_inc(&b, c, s, s->iecu);
        buf_str(&b, ": solo fijan una combinaci" S_oo "n de las dos. Hace falta otro dato.");
        break;
    case CAL_INCOMPAT:
        if (s->ninc == 1 && s->necu == 1) {
            const CalEcu *q = &s->ecu[0];
            int positiva = q->que == EC_MOD || q->que == EC_P;
            buf_str(&b, "Ning" S_uu "n valor de "); nombre_inc(&b, c, s, 0);
            buf_str(&b, " da "); nombre_ecu(&b, c, q); buf_str(&b, " = ");
            valor_ecu(&b, q, dato_visto(q), 4);
            buf_str(&b, ": al variar "); nombre_inc(&b, c, s, 0);
            buf_str(&b, ", "); nombre_ecu(&b, c, q); buf_str(&b, " va de ");
            if (q->que == EC_FP) {                 /* φ: de lo más capacitivo a lo más inductivo */
                fp_de_fi(&b, s->lo_dato);
                buf_str(&b, " a ");
                fp_de_fi(&b, s->hi_dato);
                if (s->lo_dato < 0.0 && s->hi_dato > 0.0) buf_str(&b, ", pasando por 1");
            } else {
                if (positiva && s->lo_dato < 1e-6 * s->hi_dato) buf_str(&b, "casi 0");
                else valor_ecu(&b, q, s->lo_dato, 3);
                buf_str(&b, " a ");
                valor_ecu(&b, q, s->hi_dato, 3);
            }
            buf_ch(&b, '.');
        } else if (s->ninc == 0) {
            buf_str(&b, "Los datos no cuadran con el circuito: ");
            error_ecu(&b, c, s, 1);
            buf_ch(&b, '.');
        } else {
            buf_str(&b, "Los datos se contradicen: lo m" S_aa "s cerca que se llega deja ");
            error_ecu(&b, c, s, 0);
            buf_ch(&b, '.');
        }
        break;
    case CAL_BORDE:
        if (s->necu == 1) {
            nombre_ecu(&b, c, &s->ecu[0]);
            buf_str(&b, " = ");
            valor_ecu(&b, &s->ecu[0], dato_visto(&s->ecu[0]), 4);
            buf_str(&b, " solo se alcanza en el borde: con ");
        } else buf_str(&b, "Los datos solo se alcanzan en el borde: con ");
        nombre_inc(&b, c, s, s->elem);
        buf_str(&b, " " S_RARR " ");
        buf_str(&b, s->peor < 0.0 ? "0" : S_INF);
        buf_ch(&b, '.');
        break;
    case CAL_SIN_EFECTO:
        if (!e) buf_str(&b, "La f no cambia ning" S_uu "n dato: no se puede hallar.");
        else if ((e->tipo == EL_RES || e->tipo == EL_COND || e->tipo == EL_BOB || e->tipo == EL_IMP) &&
                 s->peor >= 0.0 && s->peor <= 1e-9) {
            buf_str(&b, e->etiq);
            buf_str(&b, " no lleva corriente: su valor no cambia nada y no se puede hallar.");
        } else if (e->tipo == EL_RES || e->tipo == EL_COND || e->tipo == EL_BOB || e->tipo == EL_IMP) {
            buf_str(&b, e->etiq);
            buf_str(&b, " no influye en ning" S_uu "n dato: su valor no cambia nada y no se puede hallar.");
        } else {
            buf_str(&b, e->etiq);
            buf_str(&b, " no cambia ning" S_uu "n dato: no se puede hallar.");
        }
        break;
    case CAL_CORTADO:
        buf_str(&b, "Cortado: se ense" S_nn "a lo hallado hasta ahora (puede haber m" S_aa "s soluciones).");
        break;
    default:
        break;
    }
    return buf;
}

void cal_aplica(const CalRes *s, int i, Circuito *c)
{
    int j;
    if (i < 0 || i >= s->nsol || (s->estado != CAL_OK && s->estado != CAL_CORTADO)) return;
    for (j = 0; j < s->ninc && j < CAL_MAX_INC; j++) {
        const CalInc *in = &s->inc[j];
        double x = s->sol[i].x[j];
        Elemento *e = (in->el >= 0 && in->el < c->nel) ? &c->el[in->el] : 0;
        switch (in->que) {
        case IN_VAL:
        case IN_MODU:
            if (e) { e->val.v = x; e->val.est = M_DATO; }
            break;
        case IN_ANGZ:
        case IN_ANGU:
            if (e) { e->val.ang = x; e->val.est_ang = M_DATO; }
            break;
        case IN_F:
            c->frec.v = x; c->frec.est = M_DATO;
            break;
        default:
            break;
        }
    }
    if (s->ref_fase >= 0 && s->ref_fase < c->nel) {       /* la referencia: 0° */
        c->el[s->ref_fase].val.ang = 0.0;
        c->el[s->ref_fase].val.est_ang = M_DATO;
    }
}
