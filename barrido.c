/* ============================================================================
 *  barrido.c  -  El barrido de CC (ver barrido.h; Etapa 4, §3.J)
 *
 *  Orden del archivo:
 *    1. estado y canales
 *    2. un punto: continuación, trozos, OP de rescate, salto o hueco
 *    3. iniciar (la red, el circuito, la curva de un componente) y avanzar
 *    4. lecturas exactas: barr_en, barr_nivel, barr_nivel_dato, barr_valores
 * ========================================================================= */
#include <string.h>
#include <math.h>
#include "barrido.h"
#include "nolin.h"
#include "numfmt.h"
#include "gfx.h"

#define NS_OP_B   SIM_NS_OP            /* ns por operación en la calculadora (sim.h) */

/* La continuación guardada de cada punto de cada recorrido (la ida, la vuelta
 * o una curva de la familia): barr_en y barr_valores arrancan de la de su
 * traza, así que la vuelta de un Schmitt no lee la rama de la ida */
typedef struct {
    unsigned char hay, tramo[MAX_EL];
    int    n;
    double p, x[LU_MAX], vlim[MAX_EL][2];
} BarrCont;
static BarrCont cont_[BARR_TRAZAS][BARR_N];

static BarrReg reg_;
static BarrRes res_;
static unsigned gen_;
static unsigned long ops_;
static double x_hecho_;                /* el x del último punto hecho          */

static struct {
    int      activo, rec, nrec, i, hechos, total;
    SimRed   s;
    SimCc    io;                       /* la continuación en curso            */
    Circuito c;                        /* el circuito barrido (barr_circuito) */
    int      hay_c;
    int      fam_j;                    /* la familia: la J de la base (su IB, la de
                                          cada recorrido) o la E de la puerta (su
                                          VGS); -1 en los demás                 */
    int      curva_k;                  /* el trazador: el componente en el circuito
                                          del usuario; -1 en los demás           */
} B;

const BarrRes *barr_res(void) { return &res_; }
const BarrReg *barr_reg(void) { return &reg_; }
unsigned long barr_ops(void) { return ops_; }
int    barr_hechos(void) { return B.hechos; }
double barr_x_hecho(void) { return x_hecho_; }

/* ==========================================================================
 *  1. Canales
 * ======================================================================= */
/* El valor de un canal en unos Valores (como canal_val del emulador) */
static double canal_de(const SimCanal *ch, const Valores *v)
{
    const SimEl *e;
    if (ch->tipo == CANAL_NODO) return (ch->nodo >= 0 && ch->nodo < B.s.nn) ? v->phi[ch->nodo] : 0.0;
    if (ch->elem < 0 || ch->elem >= B.s.nel) return 0.0;
    e = &B.s.el[ch->elem];
    if (ch->tipo == CANAL_I) return ch->sg * v->I[ch->elem];
    if (ch->tipo == CANAL_V) return ch->sg * (v->phi[e->a] - v->phi[e->b]);
    return 0.0;
}

/* ==========================================================================
 *  2. Un punto
 * ======================================================================= */
/* Un error del trazador (la red sintética: el componente es el elemento 0 del
 * circuito «uno» y el 2 de la SimRed) con el índice k del circuito del
 * usuario, que es con el que la pantalla lo nombra */
static void diag_curva(SimDiag *d, int k, int en_red)
{
    if (k < 0) return;
    d->elem = d->elem == (en_red ? 2 : 0) ? k : -1;
    d->nodo = -1;                                      /* ni un nodo ni «Con E1 = …» de la red sintética */
    d->elem_aviso = -1;
    d->lazo = 0;
}

/* El punto x desde la continuación io (§3.J): Newton; si no, parte el
 * intervalo desde el último bueno en 2, 4, … 256 trozos; si no, un OP
 * completo (salto). 0 = hueco. El tope de operaciones cuenta el punto entero. */
static int punto(double x, SimCc *io, int *salto)
{
    static SimCc t;
    SimDiag d;
    unsigned long o0 = sim_ops();
    int r, m, j, ok = 0;
    *salto = 0;
    sim_cc_tope(1);
    r = sim_cc_punto(&B.s, res_.el, res_.bx, x, io, &d);
    if (r == SIM_OK) ok = 1;
    else if (io->hay && d.campo != NC_TOPE)
        for (m = 2; m <= 256 && !ok; m *= 2) {
            t = *io;
            for (j = 1; j <= m; j++) {                 /* desde el último bueno (tras un hueco, no es el anterior) */
                double xj = j == m ? x : io->p + (x - io->p) * (double)j / (double)m;
                if (sim_cc_punto(&B.s, res_.el, res_.bx, xj, &t, &d) != SIM_OK) break;
            }
            if (j > m) { *io = t; ok = 1; }
            if (d.campo == NC_TOPE) break;
        }
    if (!ok && d.campo != NC_TOPE) {                   /* el OP completo en x */
        t.hay = 0;
        if (sim_cc_punto(&B.s, res_.el, res_.bx, x, &t, &d) == SIM_OK) {
            ok = 1;
            *salto = io->hay;
            *io = t;
        }
    }
    sim_cc_tope(0);
    if (ok && io->salto) *salto = 1;
    ops_ += sim_ops() - o0;
    if (!ok && res_.d.estado == SIM_OK && B.hechos == 0) { res_.d = d; diag_curva(&res_.d, B.curva_k, 1); }   /* el primero falla: el error */
    return ok;
}

static void guarda_cont(BarrCont *c, const SimCc *io)
{
    c->hay = 1;
    c->n = io->n;
    c->p = io->p;
    memcpy(c->x, io->x, sizeof c->x);
    memcpy(c->vlim, io->vlim, sizeof c->vlim);
    memcpy(c->tramo, io->tramo, sizeof c->tramo);
}

/* En una familia, la J de la base con la IB de la curva rec, o la E de la
 * puerta con su VGS (sin signo: en el PNP y en el canal P van al revés) */
static void fam_pon(int rec)
{
    if (res_.modo == BARR_FAMILIA && B.fam_j >= 0 && rec >= 0 && rec < BARR_TRAZAS)
        B.s.el[B.fam_j].p[0] = res_.fam[rec];
}

static void de_cont(SimCc *io, const BarrCont *c)
{
    memset(io, 0, sizeof *io);
    io->hay = c->hay;
    io->n = c->n;
    io->p = c->p;
    memcpy(io->x, c->x, sizeof io->x);
    memcpy(io->vlim, c->vlim, sizeof io->vlim);
    memcpy(io->tramo, c->tramo, sizeof io->tramo);
}

/* ==========================================================================
 *  3. Iniciar y avanzar
 * ======================================================================= */
static void malla(int n)
{
    int i;
    reg_.n = n;
    for (i = 0; i < n; i++) {
        double u = (double)i / (double)(n - 1);
        reg_.x[i] = res_.log_x ? res_.x0 * pow(res_.x1 / res_.x0, u) : res_.x0 + (res_.x1 - res_.x0) * u;
    }
    reg_.x[0] = res_.x0; reg_.x[n - 1] = res_.x1;
}

int barr_iniciar_red(const SimRed *s, int el, int bx, const SimCanal ch[2],
                     double x0, double x1, int log_x, int vuelta)
{
    int i, nc = 0, t;
    memset(&res_, 0, sizeof res_);
    memset(&reg_, 0, sizeof reg_);
    res_.gen = ++gen_;
    res_.d.elem = res_.d.nodo = res_.d.elem_aviso = -1;
    res_.tr_dato = -1;
    res_.el = el; res_.bx = bx;
    res_.x0 = x0; res_.x1 = x1;
    res_.log_x = (unsigned char)(log_x && x0 > 0.0 && x1 > 0.0);
    res_.vuelta = (unsigned char)(vuelta != 0);
    B.activo = 0;
    B.fam_j = -1;
    B.curva_k = -1;                                    /* el trazador lo pone al volver */
    B.s = *s;
    if (el < 0 || el >= s->nel) { res_.d.estado = SIM_NADA_BARRER; res_.fin = FIN_ERROR; return res_.d.estado; }
    for (i = 0; i < 2; i++) {
        res_.canal[i] = ch[i];
        if (ch[i].tipo != CANAL_NADA) nc++;
    }
    if (nc == 0) { res_.canal[0].tipo = CANAL_V; res_.canal[0].elem = (signed char)el; res_.canal[0].sg = 1; nc = 1; }
    B.nrec = res_.vuelta ? 2 : 1;
    reg_.ntr = 0;
    for (t = 0; t < B.nrec; t++)
        for (i = 0; i < 2; i++)
            if (res_.canal[i].tipo != CANAL_NADA && reg_.ntr < BARR_TRAZAS) {
                reg_.recorrido[reg_.ntr] = (unsigned char)t;
                reg_.canal[reg_.ntr] = (unsigned char)i;
                reg_.ntr++;
            }
    for (t = 0; t < reg_.ntr; t++) {
        Buf b;
        buf_init(&b, res_.nom_tr[t], sizeof res_.nom_tr[t]);
        buf_str(&b, res_.canal[reg_.canal[t]].nombre[0] ? res_.canal[reg_.canal[t]].nombre : "y");
    }
    memset(&B.io, 0, sizeof B.io);
    memset(cont_, 0, sizeof cont_);
    sim_residuo_barr_reset();                          /* el suyo: el del mono sigue */
    B.rec = 0; B.i = 0; B.hechos = 0;
    x_hecho_ = x0;
    malla(BARR_N);                                     /* la definitiva, tras el primer punto */
    B.total = BARR_N * B.nrec;
    res_.fin = FIN_NO;
    B.activo = 1;
    return SIM_OK;
}

/* El primer punto decide la malla: la estimación de tiempo con lo que costó.
 * Un punto de la continuación hace las iteraciones del primero, como mucho
 * IT_PUNTO (el primero es un OP entero: el de una familia con BJT da 30; los
 * de la continuación, de media, 3,9 en el diodo, 2,6 en la familia del BJT,
 * 2,1 en la del MOSFET y 1 con solo tramos), cada una con las operaciones de
 * una del primero y lo fijo de NS_ITER (214,3 µs, lo que más pesa); sin
 * iteraciones (una red lineal), las operaciones del primero. Contar la mitad
 * de las operaciones del primero, como antes, daba la mitad de lo que tarda
 * un barrido lineal y la familia del MOSFET un 20 % corta (y la del BJT,
 * 2,7 veces larga: se quedaba en 141 puntos) */
#define IT_PUNTO 4
static void decide_malla(unsigned long ops_primero, int it_primero)
{
    int it = it_primero < IT_PUNTO ? it_primero : IT_PUNTO, n = BARR_N;
    double ops = it_primero > 0 ? (double)ops_primero * (double)it / (double)it_primero : (double)ops_primero;
    double t = (ops * NS_OP_B + (double)it * NS_ITER) * 1e-9 * (double)(BARR_N * B.nrec);
    if (t > 6.0) n = 71; else if (t > 3.0) n = 141;
    res_.t_est = t * (double)n / (double)BARR_N;
    if (n != BARR_N) { malla(n); B.total = n * B.nrec; }
}

int barr_avanzar(unsigned long ops)
{
    unsigned long hasta = ops_ + ops;
    if (!B.activo) return 0;
    do {
        /* la vuelta recorre la malla al revés; las curvas de una familia, no */
        int idx = B.rec == 0 || res_.modo == BARR_FAMILIA ? B.i : reg_.n - 1 - B.i, salto, ok, t;
        double x = reg_.x[idx];
        unsigned long o0 = ops_;
        fam_pon(B.rec);
        ok = punto(x, &B.io, &salto);
        if (B.hechos == 0) {
            if (!ok) {                                 /* ni el primero: el error del circuito */
                B.activo = 0; res_.fin = FIN_ERROR;
                if (res_.d.estado == SIM_OK) res_.d.estado = SIM_NO_CONVERGE;
                return 0;
            }
            decide_malla(ops_ - o0, B.io.iter);
        }
        for (t = 0; t < reg_.ntr; t++) {
            if (reg_.recorrido[t] != B.rec) continue;
            reg_.ok[t][idx] = (unsigned char)ok;
            reg_.salto[t][idx] = (unsigned char)(ok && salto);
            reg_.it[t][idx] = (unsigned char)(B.io.iter > 255 ? 255 : B.io.iter);
            reg_.y[t][idx] = ok ? canal_de(&res_.canal[reg_.canal[t]], &B.io.v) : 0.0;
        }
        if (ok) {
            guarda_cont(&cont_[B.rec][idx], &B.io);
            if (B.io.iter > res_.it_max) res_.it_max = B.io.iter;
            if (salto) res_.d.avisos |= SIM_AV_SALTO;
        } else {
            if (res_.huecos == 0) res_.x_fallo = x;
            res_.huecos++;
            res_.d.avisos |= SIM_AV_HUECOS;
        }
        B.hechos++;
        x_hecho_ = x;
        if (++B.i >= reg_.n) {
            B.i = 0;
            if (++B.rec >= B.nrec) {
                B.activo = 0;
                res_.fin = FIN_COMPLETO;
                res_.kcl_peor = sim_residuo_barr();
                return 0;
            }
            if (res_.modo == BARR_FAMILIA) memset(&B.io, 0, sizeof B.io);   /* cada curva, con un OP completo */
        }
    } while (ops_ < hasta);
    return 1;
}

void barr_cortar(void)
{
    if (!B.activo) return;
    B.activo = 0;
    res_.fin = FIN_CORTADO;
}

double barr_progreso(void)
{
    if (B.total <= 0) return 0.0;
    return (double)B.hechos / (double)B.total;
}

/* ---- Desde el Circuito ---- */
static int bx_de(int tipo)
{
    switch (tipo) {
    case EL_FUENTE: return BX_E;
    case EL_CORR:   return BX_J;
    case EL_RES:    return BX_R;
    default:        return BX_K;                       /* EDEP, JDEP */
    }
}

static int barrible(int tipo) { return tipo == EL_FUENTE || tipo == EL_CORR || tipo == EL_RES || tipo == EL_EDEP || tipo == EL_JDEP; }

/* El que se barre, automático (§3.J): el que tiene [?!] en su valor; si no, la
 * primera E continua; si no, la primera E o J; si no, la primera R */
static int elige_el(const Circuito *c)
{
    int k;
    for (k = 0; k < c->nel; k++) if (barrible(c->el[k].tipo) && c->el[k].val.est == M_BUSCAR) return k;
    for (k = 0; k < c->nel; k++) if (c->el[k].tipo == EL_FUENTE && c->el[k].variante == ONDA_CC) return k;
    for (k = 0; k < c->nel; k++) if (c->el[k].tipo == EL_FUENTE || c->el[k].tipo == EL_CORR) return k;
    for (k = 0; k < c->nel; k++) if (c->el[k].tipo == EL_RES) return k;
    return -1;
}

static void pon_canal_b(SimCanal *ch, int tipo, int elem, int nodo, int sg, const char *nom)
{
    Buf b;
    memset(ch, 0, sizeof *ch);
    ch->tipo = (unsigned char)tipo; ch->elem = (signed char)elem; ch->nodo = (signed char)nodo; ch->sg = (signed char)sg;
    ch->automatico = 1;
    buf_init(&b, ch->nombre, sizeof ch->nombre);
    buf_str(&b, nom);
}

/* ¿Ya está ese canal entre los n primeros? (el del dato no se repite: con
 * I(D1) = 20 mA de dato, el segundo canal es V(D1) y no otra vez I(D1)) */
static int ya_esta_b(const SimCanal *ch, int n, int tipo, int elem, int nodo)
{
    int i;
    for (i = 0; i < n; i++)
        if (ch[i].tipo == tipo && (tipo == CANAL_NODO ? ch[i].nodo == nodo : ch[i].elem == elem)) return 1;
    return 0;
}

/* Los canales automáticos (§3.J): 1) los V e I con dato o pedidos; 2) la Vsal
 * del primer AO; 3) la I y la V del primer diodo, zener o LED; 4) IC y VCE del
 * primer BJT (ID y VDS del MOS); 5) V(AB); 6) la I de la primera R. Los dos
 * primeros que haya, sin repetir. */
static void canales_auto(const Circuito *c, const Red *r, SimCanal ch[2])
{
    char t[12];
    int k, n = 0;
    Buf b;
    ch[0].tipo = ch[1].tipo = CANAL_NADA;
    for (k = 0; k < c->nel && n < 2; k++) {
        const Elemento *e = &c->el[k];
        int sgv = circ_v_convenio(e->tipo) == -1 ? -1 : 1;
        if (e->V.est == M_DATO || e->V.est == M_BUSCAR) {
            buf_init(&b, t, sizeof t); buf_str(&b, "V("); buf_str(&b, e->etiq); buf_ch(&b, ')');
            if (e->tipo == EL_AO) pon_canal_b(&ch[n++], CANAL_NODO, -1, r->c[k], 1, "");
            else pon_canal_b(&ch[n++], CANAL_V, k, -1, sgv, t);
            if (e->tipo == EL_AO) { buf_init(&b, ch[n - 1].nombre, sizeof ch[n - 1].nombre); buf_str(&b, "Vsal("); buf_str(&b, e->etiq); buf_ch(&b, ')'); }
            if (e->V.est == M_DATO && res_.tr_dato < 0) { res_.tr_dato = (signed char)(n - 1); res_.nivel_dato = fabs(e->V.v); }
        }
        if (n < 2 && (e->I.est == M_DATO || e->I.est == M_BUSCAR)) {
            buf_init(&b, t, sizeof t); buf_str(&b, "I("); buf_str(&b, e->etiq); buf_ch(&b, ')');
            pon_canal_b(&ch[n++], CANAL_I, k, -1, 1, t);
            if (e->I.est == M_DATO && res_.tr_dato < 0) { res_.tr_dato = (signed char)(n - 1); res_.nivel_dato = fabs(e->I.v); }
        }
    }
    for (k = 0; k < c->nel && n < 2; k++)
        if (c->el[k].tipo == EL_AO) {
            buf_init(&b, t, sizeof t); buf_str(&b, "Vsal("); buf_str(&b, c->el[k].etiq); buf_ch(&b, ')');
            if (!ya_esta_b(ch, n, CANAL_NODO, -1, r->c[k])) pon_canal_b(&ch[n++], CANAL_NODO, -1, r->c[k], 1, t);
            break;
        }
    for (k = 0; k < c->nel && n < 2; k++)
        if (c->el[k].tipo == EL_DIODO || c->el[k].tipo == EL_ZENER || c->el[k].tipo == EL_LED) {
            buf_init(&b, t, sizeof t); buf_str(&b, "I("); buf_str(&b, c->el[k].etiq); buf_ch(&b, ')');
            if (!ya_esta_b(ch, n, CANAL_I, k, -1)) pon_canal_b(&ch[n++], CANAL_I, k, -1, 1, t);
            if (n < 2 && !ya_esta_b(ch, n, CANAL_V, k, -1)) {
                buf_init(&b, t, sizeof t); buf_str(&b, "V("); buf_str(&b, c->el[k].etiq); buf_ch(&b, ')');
                pon_canal_b(&ch[n++], CANAL_V, k, -1, 1, t);
            }
            break;
        }
    for (k = 0; k < c->nel && n < 2; k++)
        if (c->el[k].tipo == EL_BJT || c->el[k].tipo == EL_MOS) {
            int bjt = c->el[k].tipo == EL_BJT;
            buf_init(&b, t, sizeof t); buf_str(&b, bjt ? "IC(" : "ID("); buf_str(&b, c->el[k].etiq); buf_ch(&b, ')');
            if (!ya_esta_b(ch, n, CANAL_I, k, -1)) pon_canal_b(&ch[n++], CANAL_I, k, -1, 1, t);
            if (n < 2 && !ya_esta_b(ch, n, CANAL_V, k, -1)) {
                buf_init(&b, t, sizeof t); buf_str(&b, bjt ? "VCE(" : "VDS("); buf_str(&b, c->el[k].etiq); buf_ch(&b, ')');
                pon_canal_b(&ch[n++], CANAL_V, k, -1, 1, t);
            }
            break;
        }
    if (n < 2 && c->term_x[0] >= 0 && c->term_x[1] >= 0) {
        int na = r->punto[c->term_y[0] * REJ_COLS + c->term_x[0]], nb = r->punto[c->term_y[1] * REJ_COLS + c->term_x[1]];
        if (na >= 0 && nb >= 0 && r->ref[nb] && !ya_esta_b(ch, n, CANAL_NODO, -1, na)) pon_canal_b(&ch[n++], CANAL_NODO, -1, na, 1, "V(AB)");
    }
    for (k = 0; k < c->nel && n < 2; k++)
        if (c->el[k].tipo == EL_RES) {
            buf_init(&b, t, sizeof t); buf_str(&b, "I("); buf_str(&b, c->el[k].etiq); buf_ch(&b, ')');
            if (!ya_esta_b(ch, n, CANAL_I, k, -1)) pon_canal_b(&ch[n++], CANAL_I, k, -1, 1, t);
            break;
        }
}

void barr_rango_auto(const Circuito *c, int el, double *x0, double *x1, int *log_x)
{
    int bx = bx_de(c->el[el].tipo);
    double v = c->el[el].val.est == M_DATO ? c->el[el].val.v : 0.0;
    *log_x = 0;
    if (bx == BX_R) {
        if (v > 0.0) { *x0 = v / 100.0; *x1 = 100.0 * v; } else { *x0 = 1.0; *x1 = 1e6; }
        *log_x = 1;
    } else if (bx == BX_K) {                           /* de 0 a 2k; con k < 0, de 2k a 0 (la malla crece) */
        *x0 = v < 0.0 ? 2.0 * v : 0.0;
        *x1 = v > 0.0 ? 2.0 * v : v < 0.0 ? 0.0 : 1.0;
    } else { *x0 = v != 0.0 ? -fabs(v) : -1.0; *x1 = v != 0.0 ? fabs(v) : 1.0; }
}

int barr_iniciar(const Circuito *c, const Red *r, const BarrAjustes *aj)
{
    static SimRed s;
    SimCanal ch[2];
    SimDiag d;
    int el = aj && aj->el >= 0 ? aj->el : elige_el(c), bx, k, e, dinamico = 0;
    double x0, x1;
    int log_x;
    memset(&res_, 0, sizeof res_);
    res_.d.elem = res_.d.nodo = res_.d.elem_aviso = -1;
    res_.tr_dato = -1;
    if (el < 0 || el >= c->nel || !barrible(c->el[el].tipo)) {
        res_.d.estado = SIM_NADA_BARRER; res_.fin = FIN_ERROR; B.activo = 0; return res_.d.estado;
    }
    bx = bx_de(c->el[el].tipo);
    B.c = *c;
    B.hay_c = 1;
    /* el rango, si no se dio */
    if (aj && !(aj->x0 == 0.0 && aj->x1 == 0.0)) { x0 = aj->x0; x1 = aj->x1; log_x = aj->log_x; }
    else barr_rango_auto(&B.c, el, &x0, &x1, &log_x);
    /* el valor de lo barrido como dato (x0) y la red */
    B.c.el[el].val.v = x0 != 0.0 ? x0 : (bx == BX_R ? 1.0 : 0.0);
    B.c.el[el].val.est = M_DATO;
    if (bx == BX_E || bx == BX_J) B.c.el[el].variante = ONDA_CC;
    for (k = 0; k < B.c.nel; k++) {
        if (B.c.el[k].val.est == M_BUSCAR) B.c.el[k].val.est = M_LIBRE;
        if (B.c.el[k].tipo == EL_COND || B.c.el[k].tipo == EL_BOB || B.c.el[k].tipo == EL_INTER) dinamico = 1;
    }
    e = sim_red_de(&B.c, r, &s, &d);
    if (e != SIM_OK) { res_.d = d; res_.fin = FIN_ERROR; B.activo = 0; return e; }
    if (c->sonda[0].tipo != SONDA_NO || c->sonda[1].tipo != SONDA_NO) sim_canales(c, r, ch);
    else canales_auto(c, r, ch);
    {
        signed char trd = res_.tr_dato;
        double nd = res_.nivel_dato;
        e = barr_iniciar_red(&s, el, bx, ch, x0, x1, log_x, aj ? aj->vuelta : 0);
        res_.tr_dato = trd; res_.nivel_dato = nd;
    }
    if (e != SIM_OK) return e;
    res_.modo = BARR_CIRCUITO;
    if (dinamico) res_.d.avisos |= SIM_AV_CC_BARRIDO;
    {
        Buf b;
        buf_init(&b, res_.nom_x, sizeof res_.nom_x); buf_str(&b, c->el[el].etiq);
        buf_init(&b, res_.uni_x, sizeof res_.uni_x);
        if (bx == BX_K) {                              /* la de su ficha: g en S y r en Ω; μ y β sin unidad */
            const CampoInfo *ci = tipo_campo_v(c->el[el].tipo, c->el[el].variante, Q_VAL);
            buf_str(&b, ci ? ci->unidad : "");
        } else buf_str(&b, bx == BX_E ? "V" : bx == BX_J ? "A" : S_OHM);
    }
    return SIM_OK;
}

/* La familia de un transistor (§4.10), 6 curvas, una por recorrido, y cada
 * una empieza con un OP completo:
 *  - BJT: una E ideal entre C y E de 0 a 10 V y una J en la base con IB_k =
 *    k·2 mA/β, k = 0..5 (IC de 0 a 10 mA en activa): IC(VCE);
 *  - MOSFET: una E ideal entre D y S de 0 a 10 V y otra entre G y S con VGS_k
 *    = Vt + 0,5·k V, k = 0..5 (ID = K·(0,5·k)² saturado): ID(VDS).
 * En el PNP y en el canal P las fuentes van al revés: x es VEC (VSD) y la
 * curva, -IC (-ID): la misma figura que la del NPN (canal N). */
static int familia(const Circuito *c, int k)
{
    static SimRed s;
    static Circuito uno;
    static Red r1;
    SimCanal ch[2];
    SimDiag d;
    const Elemento *e = &c->el[k];
    int err, t, pnp, bjt = e->tipo == EL_BJT;
    char ni[12], u[24];
    Buf b;
    circ_init(&uno, "familia");
    uno.el[0] = *e;
    uno.nel = 1;
    memset(&r1, 0, sizeof r1);
    r1.nn = 3; r1.nel = 1; r1.a[0] = 1; r1.b[0] = 0; r1.c[0] = 2; r1.ref[0] = 1;   /* C 1, E 0, B 2 (D, S, G) */
    err = sim_red_de(&uno, &r1, &s, &d);
    if (err != SIM_OK) { res_.d = d; diag_curva(&res_.d, k, 0); res_.fin = FIN_ERROR; return err; }
    pnp = s.el[0].pol < 0;
    s.el[2] = s.el[0];                                 /* el transistor: el 2 */
    s.el[2].k = -1;
    memset(&s.el[0], 0, sizeof s.el[0]);               /* la E de C a E (en el PNP, de E a C) */
    s.el[0].tipo = SE_E; s.el[0].a = (short)(pnp ? 1 : 0); s.el[0].b = (short)(pnp ? 0 : 1);
    s.el[0].k = -1; s.el[0].t1 = s.el[0].t2 = -1.0; s.el[0].forma = ONDA_CC;
    /* la J que entra por la base (en el PNP, sale) o la E de la puerta (con
     * su + en G; en el canal P, en S) */
    memset(&s.el[1], 0, sizeof s.el[1]);
    s.el[1].tipo = (unsigned char)(bjt ? SE_J : SE_E); s.el[1].a = (short)(pnp ? 2 : 0); s.el[1].b = (short)(pnp ? 0 : 2);
    s.el[1].k = -1; s.el[1].t1 = s.el[1].t2 = -1.0; s.el[1].forma = ONDA_CC;
    s.nn = 3; s.ref[0] = 1; s.nel = 3;
    buf_init(&b, ni, sizeof ni); buf_str(&b, pnp ? "-" : ""); buf_str(&b, bjt ? "IC(" : "ID(");
    buf_str(&b, e->etiq); buf_ch(&b, ')');
    pon_canal_b(&ch[0], CANAL_I, 2, -1, pnp ? -1 : 1, ni);
    ch[0].automatico = 0;
    memset(&ch[1], 0, sizeof ch[1]);
    ch[1].tipo = CANAL_NADA;
    err = barr_iniciar_red(&s, 0, BX_E, ch, 0.0, 10.0, 0, 0);
    if (err != SIM_OK) return err;
    B.curva_k = k;
    res_.modo = BARR_FAMILIA;
    res_.fam_bjt = (unsigned char)bjt;
    res_.fam_early = (unsigned char)((bjt ? s.el[2].m[3] : s.el[2].m[2]) > 0.0);
    res_.fam_vt = bjt ? 0.0 : s.el[2].m[0];
    B.fam_j = 1;
    B.nrec = BARR_TRAZAS;
    B.total = BARR_N * B.nrec;
    reg_.ntr = BARR_TRAZAS;
    for (t = 0; t < BARR_TRAZAS; t++) {                /* «IB=40µ» o «VGS=3» */
        res_.fam[t] = bjt ? (double)t * 2e-3 / s.el[2].m[1] : s.el[2].m[0] + 0.5 * (double)t;
        reg_.recorrido[t] = (unsigned char)t;
        reg_.canal[t] = 0;
        buf_init(&b, res_.nom_tr[t], sizeof res_.nom_tr[t]);
        buf_str(&b, bjt ? "IB=" : pnp ? "VSG=" : "VGS="); buf_str(&b, num_pref(u, sizeof u, res_.fam[t], 3, ""));
    }
    fam_pon(0);
    buf_init(&b, res_.nom_x, sizeof res_.nom_x);
    buf_str(&b, bjt ? (pnp ? "VEC" : "VCE") : (pnp ? "VSD" : "VDS"));
    buf_init(&b, res_.uni_x, sizeof res_.uni_x); buf_str(&b, "V");
    buf_init(&b, res_.titulo, sizeof res_.titulo);     /* «IC frente a VCE de Q1», «ID frente a VDS de M1» */
    if (bjt) buf_str(&b, pnp ? "-IC frente a VEC de " : "IC frente a VCE de ");
    else buf_str(&b, pnp ? "-ID frente a VSD de " : "ID frente a VDS de ");
    buf_str(&b, e->etiq);
    return SIM_OK;
}

/* La V en bornes de un diodo, zener o LED (su SimEl) con la corriente directa
 * i: la ley de nolin.c al revés. Por tramos, la recta de conducción, Vγ +
 * Ron·i; la unión, vd = n·VT·ln(1 + ij/Is) (por encima de 80, la recta de
 * nl_exp) más Rs·ij. Sin la fuga GMIN ni la ruptura, que en directa no se
 * notan (la fuga deja la I del final un pelo por encima de i). */
static double v_directa(const SimEl *e, double i)
{
    double y, e80;
    if (e->modelo == NL_TRAMOS) return e->m[0] + e->m[1] * i;
    y = 1.0 + i / e->m[0];
    e80 = exp(NL_XLIM);
    return e->m[1] * NL_VT * (y > e80 ? NL_XLIM + y / e80 - 1.0 : log(y)) + e->m[2] * i;
}

/* El trazador de curvas: el componente k solo, con una E en serie con 1 Ω y la
 * curva I-V (V en x, I en y). Diodo, zener y LED; el BJT y el MOSFET, su
 * familia. E llega a V(I_top) + 1 Ω·I_top (§4.10): la curva acaba en I_top
 * (100 mA; 1 A el 1N4007; 1,2·Im el LED) con cualquier Vγ, n o Vf. */
int barr_iniciar_curva(const Circuito *c, int k)
{
    static SimRed s;
    static Circuito uno;
    SimCanal ch[2];
    SimDiag d;
    const Elemento *e;
    int t, err;
    double x0 = -2.0, x1, itop;
    char nv[12], ni[12];
    Buf b;
    memset(&res_, 0, sizeof res_);
    res_.d.elem = res_.d.nodo = res_.d.elem_aviso = -1;
    res_.tr_dato = -1;
    B.hay_c = 0;                                       /* sin circuito: ni el de un barrido anterior */
    if (k < 0 || k >= c->nel) { res_.d.estado = SIM_NADA_BARRER; res_.fin = FIN_ERROR; return res_.d.estado; }
    e = &c->el[k];
    t = e->tipo;
    if (t == EL_BJT || t == EL_MOS) return familia(c, k);
    if (t != EL_DIODO && t != EL_ZENER && t != EL_LED) { res_.d.estado = SIM_NADA_BARRER; res_.fin = FIN_ERROR; return res_.d.estado; }
    /* la SimRed a mano: E (0 -> 1), 1 Ω (1 -> 2), el componente (2 -> 0) */
    circ_init(&uno, "curva");
    uno.el[0] = *e;
    uno.nel = 1;
    memset(&s, 0, sizeof s);
    s.nn = 3; s.ref[0] = 1; s.nel = 3;
    {
        static Red r1;
        memset(&r1, 0, sizeof r1);
        r1.nn = 3; r1.nel = 1; r1.a[0] = 2; r1.b[0] = 0; r1.c[0] = -1; r1.ref[0] = 1;
        err = sim_red_de(&uno, &r1, &s, &d);
        if (err != SIM_OK) { res_.d = d; diag_curva(&res_.d, k, 0); res_.fin = FIN_ERROR; return err; }
    }
    s.nn = 3; s.ref[0] = 1;
    s.el[2] = s.el[0];                                 /* el componente: el 2 */
    s.el[2].k = -1;
    memset(&s.el[0], 0, sizeof s.el[0]);
    s.el[0].tipo = SE_E; s.el[0].a = 0; s.el[0].b = 1; s.el[0].k = -1; s.el[0].t1 = s.el[0].t2 = -1.0; s.el[0].forma = ONDA_CC;
    memset(&s.el[1], 0, sizeof s.el[1]);
    s.el[1].tipo = SE_R; s.el[1].a = 1; s.el[1].b = 2; s.el[1].val = 1.0; s.el[1].k = -1; s.el[1].t1 = s.el[1].t2 = -1.0;
    s.nel = 3;
    if (t == EL_ZENER) x0 = -(s.el[2].modelo == NL_TRAMOS ? s.el[2].m[2] : s.el[2].m[3]) - 2.0;
    itop = t == EL_LED ? 1.2 * s.el[2].p[0] : t == EL_DIODO && e->variante == DIO_1N4007 ? 1.0 : 0.1;
    x1 = v_directa(&s.el[2], itop) + 1.0 * itop;
    /* los canales con el nombre del componente: «V(D1)» e «I(D1)» */
    buf_init(&b, nv, sizeof nv); buf_str(&b, "V("); buf_str(&b, e->etiq); buf_ch(&b, ')');
    buf_init(&b, ni, sizeof ni); buf_str(&b, "I("); buf_str(&b, e->etiq); buf_ch(&b, ')');
    pon_canal_b(&ch[0], CANAL_V, 2, -1, 1, nv);
    pon_canal_b(&ch[1], CANAL_I, 2, -1, 1, ni);
    ch[0].automatico = ch[1].automatico = 0;
    err = barr_iniciar_red(&s, 0, BX_E, ch, x0, x1, 0, 0);
    if (err != SIM_OK) return err;
    B.curva_k = k;
    res_.modo = BARR_CURVA;
    res_.xy = 1;
    buf_init(&b, res_.nom_x, sizeof res_.nom_x); buf_str(&b, "E");
    buf_init(&b, res_.uni_x, sizeof res_.uni_x); buf_str(&b, "V");
    /* «Curva I-V de D1 (1N4148)»: el nombre de la variante hasta su paréntesis */
    buf_init(&b, res_.titulo, sizeof res_.titulo);
    buf_str(&b, "Curva I-V de "); buf_str(&b, e->etiq);
    {
        const TipoInfo *ti = tipo_info(t);
        if (ti->nom_var && e->variante < ti->nvar) {
            const char *p = ti->nom_var[e->variante];
            buf_str(&b, " (");
            for (; *p && !(p[0] == ' ' && p[1] == '('); p++) buf_ch(&b, *p);
            buf_ch(&b, ')');
        }
    }
    return SIM_OK;
}

/* ==========================================================================
 *  4. Lecturas exactas
 * ======================================================================= */
/* La continuación de la que sale x: la del punto hecho más cercano de los de
 * ANTES en el sentido del recorrido (x_i <= x en la ida y en cada curva de una
 * familia; x_i >= x en la vuelta), como la seguiría el barrido. Entre los dos
 * puntos de un salto, el de después ya está en la otra rama, que también es
 * solución: desde él, un Schmitt leería la otra rama antes de su umbral. Si no
 * hay ninguno antes (x fuera de lo hecho), el más cercano. */
static int mas_cercano(int rec, double x)
{
    int i, mejor = -1, antes = -1, vuelta = rec != 0 && res_.modo != BARR_FAMILIA;
    double dm = 1e300;
    for (i = 0; i < reg_.n; i++) {
        double dd;
        if (!cont_[rec][i].hay) continue;
        if (vuelta ? reg_.x[i] >= x && antes < 0 : reg_.x[i] <= x) antes = i;
        dd = res_.log_x && x > 0.0 && reg_.x[i] > 0.0 ? fabs(log(x / reg_.x[i])) : fabs(x - reg_.x[i]);
        if (dd < dm) { dm = dd; mejor = i; }
    }
    return antes >= 0 ? antes : mejor;
}

/* La solución en x desde la continuación de la traza tr */
static int resuelve_en(int tr, double x, SimCc *io)
{
    int rec, i, salto;
    if (tr < 0 || tr >= reg_.ntr) return 0;
    rec = reg_.recorrido[tr];
    i = mas_cercano(rec, x);
    if (i < 0) return 0;
    de_cont(io, &cont_[rec][i]);
    fam_pon(rec);                                      /* una familia: la IB de esa curva */
    return punto(x, io, &salto);
}

/* Las 4 últimas lecturas (como traz_lee): la pantalla lee el cursor en cada
 * cuadro. Solo con el barrido acabado (mientras avanza, el punto más cercano
 * de la malla puede cambiar y la lectura no sería la misma bit a bit). Un x
 * donde Newton no converge también se recuerda (ok = 0): con el cursor en un
 * hueco, cada cuadro repetiría sus lecturas fallidas enteras, hasta el tope
 * de NL_OPS_OP cada una */
static struct { unsigned gen; int tr, hay_d, ok; double x, y, d; unsigned char reg; } lee_[4];
static int lee_i;
static int reg_lee_;                  /* la región del componente del trazador en la última lectura */

int barr_region(void) { return reg_lee_; }

int barr_en(int tr, double x, double *y, double *dydx)
{
    static SimCc io;
    int q, ok;
    if (!B.activo)
        for (q = 0; q < 4; q++)
            if (lee_[q].gen == res_.gen && lee_[q].tr == tr && lee_[q].x == x && (!dydx || lee_[q].hay_d)) {
                reg_lee_ = lee_[q].reg;
                if (!lee_[q].ok) return 0;
                *y = lee_[q].y;
                if (dydx) *dydx = lee_[q].d;
                return 1;
            }
    ok = resuelve_en(tr, x, &io);
    reg_lee_ = ok && B.curva_k >= 0 ? io.v.reg[2] : REG_NADA;    /* el trazador: el componente es el 2 */
    if (ok) {
        *y = canal_de(&res_.canal[reg_.canal[tr]], &io.v);
        if (dydx && !sim_cc_deriva(&B.s, res_.el, res_.bx, &io, &res_.canal[reg_.canal[tr]], dydx)) *dydx = 0.0;
    }
    if (!B.activo) {
        lee_[lee_i].gen = res_.gen; lee_[lee_i].tr = tr; lee_[lee_i].x = x; lee_[lee_i].ok = ok;
        lee_[lee_i].y = ok ? *y : 0.0;
        lee_[lee_i].hay_d = dydx != 0 || !ok; lee_[lee_i].d = ok && dydx ? *dydx : 0.0;
        lee_[lee_i].reg = (unsigned char)reg_lee_;
        lee_i = (lee_i + 1) & 3;
    }
    return ok;
}

static double residuo_val_;           /* el residuo de Kirchhoff de los últimos barr_valores */

int barr_valores(int tr, double x, Valores *v, SimDiag *d)
{
    static SimCc io;
    int k;
    memset(d, 0, sizeof *d);
    d->elem = d->nodo = d->elem_aviso = -1;
    residuo_val_ = 0.0;
    sim_cc_residuo(1);
    k = resuelve_en(tr, x, &io);
    sim_cc_residuo(0);
    if (!k) { d->estado = SIM_NO_CONVERGE; d->campo = NC_BARR; d->t = x; return d->estado; }
    *v = io.v;
    residuo_val_ = io.residuo;
    if (B.hay_c && res_.el >= 0 && res_.el < B.c.nel) { B.c.el[res_.el].val.v = x; B.c.el[res_.el].val.est = M_DATO; }
    /* el aviso de un AO saturado en este punto, como el del punto de operación
     * (BIESTABLE y HOMOTOPIA no son de un punto de continuación) */
    for (k = 0; B.hay_c && k < B.c.nel; k++)
        if (B.c.el[k].tipo == EL_AO && (v->reg[k] == REG_SAT_MAS || v->reg[k] == REG_SAT_MENOS)) {
            d->avisos |= SIM_AV_SATURA;
            d->elem_aviso = k;
            break;
        }
    d->estado = SIM_OK;
    return SIM_OK;
}

double barr_residuo(void) { return residuo_val_; }

const Circuito *barr_circuito(void) { return B.hay_c ? &B.c : 0; }

/* Brent sobre barr_en - nivel en [a, b] con f(a)·f(b) <= 0 */
static int brent(int tr, double nivel, double a, double b, double fa, double fb, double *x)
{
    double c = a, fc = fa, d = b - a, e = d;
    int it;
    for (it = 0; it < 60; it++) {
        double tol, m, y;
        if ((fb > 0.0) == (fc > 0.0)) { c = a; fc = fa; d = e = b - a; }
        if (fabs(fc) < fabs(fb)) { a = b; b = c; c = a; fa = fb; fb = fc; fc = fa; }
        tol = 1e-14 * fabs(b) + 1e-300;
        m = 0.5 * (c - b);
        if (fabs(m) <= tol || fb == 0.0) { *x = b; return 1; }
        if (fabs(e) >= tol && fabs(fa) > fabs(fb)) {
            double s = fb / fa, p, q, r;
            if (a == c) { p = 2.0 * m * s; q = 1.0 - s; }
            else {
                q = fa / fc; r = fb / fc;
                p = s * (2.0 * m * q * (q - r) - (b - a) * (r - 1.0));
                q = (q - 1.0) * (r - 1.0) * (s - 1.0);
            }
            if (p > 0.0) q = -q; else p = -p;
            if (2.0 * p < 3.0 * m * q - fabs(tol * q) && p < fabs(0.5 * e * q)) { e = d; d = p / q; }
            else { d = m; e = m; }
        } else { d = m; e = m; }
        a = b; fa = fb;
        b += fabs(d) > tol ? d : (m > 0.0 ? tol : -tol);
        if (!barr_en(tr, b, &y, 0)) return 0;
        fb = y - nivel;
    }
    *x = b;
    return 1;
}

int barr_nivel(int tr, double nivel, double xa, double xb, double *x)
{
    int i, prev = -1;
    double lo = xa < xb ? xa : xb, hi = xa < xb ? xb : xa;
    if (tr < 0 || tr >= reg_.ntr) return 0;
    for (i = 0; i < reg_.n; i++) {                     /* el cambio de signo, primero en la malla */
        if (!reg_.ok[tr][i] || reg_.x[i] < lo || reg_.x[i] > hi) continue;
        if (prev >= 0) {
            double f0 = reg_.y[tr][prev] - nivel, f1 = reg_.y[tr][i] - nivel;
            if (f0 == 0.0) { *x = reg_.x[prev]; return 1; }
            if ((f0 < 0.0) != (f1 < 0.0) || f1 == 0.0) {
                double y0, y1;
                if (!barr_en(tr, reg_.x[prev], &y0, 0) || !barr_en(tr, reg_.x[i], &y1, 0)) return 0;
                return brent(tr, nivel, reg_.x[prev], reg_.x[i], y0 - nivel, y1 - nivel, x);
            }
        }
        prev = i;
    }
    return 0;
}

int barr_nivel_dato(int tr, double m, double xa, double xb, double *x, int *sg)
{
    if (barr_nivel(tr, fabs(m), xa, xb, x)) { *sg = 1; return 1; }
    if (barr_nivel(tr, -fabs(m), xa, xb, x)) { *sg = -1; return 1; }
    *sg = 0;
    return 0;
}
