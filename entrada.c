/* ============================================================================
 *  entrada.c  -  La capa de entrada (ver entrada.h)
 *
 *  Cuatro piezas sobre la misma foto:
 *    1. las teclas: el estado de cada tecla física, sus flancos y la cola;
 *    2. la ráfaga: la tecla que sigue abajo y sus repeticiones, contadas por
 *       el reloj (no se encolan: soltar la tecla no deja inercia);
 *    3. el touchpad: la cuña del clic (una flecha más de las teclas) y el
 *       reconocedor de deslizamientos (un gesto por contacto);
 *    4. el grabador de muestras crudas.
 *  Y aparte, la cuenta del reloj del plan A, vigilada (main.c le da las
 *  lecturas del contador).
 *  No sabe nada del hardware: la foto se la da main.c (o el arnés).
 * ========================================================================= */
#include <string.h>
#include "entrada.h"
#include "ui.h"
#include "archivo.h"

/* --------------------------------------------------------------------------
 *  La tabla física: dónde está cada tecla en la matriz y qué tecla abstracta
 *  da. fila = (tpad_row - 0x10) / 2 y bit = tpad_col de keys.h; las flechas
 *  y el clic salen del touchpad (fila -1). El orden de TF_* es la prioridad.
 * ----------------------------------------------------------------------- */
typedef struct { signed char fila; unsigned short bit; int t; } Fisica;

static const Fisica FIS[TF_N] = {
    [TF_ESC]   = { 6, 0x080, T_ESC },       [TF_TAB]    = { 6, 0x200, T_TAB },
    [TF_MENU]  = { 6, 0x020, T_MENU },      [TF_ENTER]  = { 0, 0x002, T_ENTER },
    [TF_RET]   = { 0, 0x001, T_ENTER },
    [TF_ARRIBA] = { -1, 0, T_ARRIBA },      [TF_ABAJO]  = { -1, 0, T_ABAJO },
    [TF_IZQ]    = { -1, 0, T_IZQ },         [TF_DER]    = { -1, 0, T_DER },
    [TF_CLIC]   = { -1, 0, T_ENTER },
    [TF_DEL]   = { 5, 0x200, T_BORRAR },    [TF_PUNTO]  = { 5, 0x010, T_PUNTO },
    [TF_SIGNO] = { 0, 0x008, T_SIGNO },     [TF_MAS]    = { 6, 0x004, T_MAS },
    [TF_MENOS] = { 5, 0x004, T_MENOS },     [TF_IGUAL]  = { 4, 0x080, T_IGUAL },
    [TF_INCOG] = { 0, 0x100, T_INCOG },     [TF_EE]     = { 2, 0x100, T_EE },
    [TF_0 + 0] = { 0, 0x080, T_DIG0 + 0 },  [TF_0 + 1]  = { 1, 0x080, T_DIG0 + 1 },
    [TF_0 + 2] = { 6, 0x010, T_DIG0 + 2 },  [TF_0 + 3]  = { 1, 0x008, T_DIG0 + 3 },
    [TF_0 + 4] = { 2, 0x080, T_DIG0 + 4 },  [TF_0 + 5]  = { 5, 0x040, T_DIG0 + 5 },
    [TF_0 + 6] = { 2, 0x008, T_DIG0 + 6 },  [TF_0 + 7]  = { 3, 0x080, T_DIG0 + 7 },
    [TF_0 + 8] = { 6, 0x040, T_DIG0 + 8 },  [TF_0 + 9]  = { 3, 0x008, T_DIG0 + 9 },
    [TF_A +  0] = { 4, 0x040, T_LETRA0 +  0 },   /* A */
    [TF_A +  1] = { 4, 0x020, T_LETRA0 +  1 },   /* B */
    [TF_A +  2] = { 4, 0x010, T_LETRA0 +  2 },   /* C */
    [TF_A +  3] = { 4, 0x004, T_LETRA0 +  3 },   /* D */
    [TF_A +  4] = { 4, 0x002, T_LETRA0 +  4 },   /* E */
    [TF_A +  5] = { 4, 0x001, T_LETRA0 +  5 },   /* F */
    [TF_A +  6] = { 3, 0x040, T_LETRA0 +  6 },   /* G */
    [TF_A +  7] = { 3, 0x020, T_LETRA0 +  7 },   /* H */
    [TF_A +  8] = { 3, 0x010, T_LETRA0 +  8 },   /* I */
    [TF_A +  9] = { 3, 0x004, T_LETRA0 +  9 },   /* J */
    [TF_A + 10] = { 3, 0x002, T_LETRA0 + 10 },   /* K */
    [TF_A + 11] = { 3, 0x001, T_LETRA0 + 11 },   /* L */
    [TF_A + 12] = { 2, 0x040, T_LETRA0 + 12 },   /* M */
    [TF_A + 13] = { 2, 0x020, T_LETRA0 + 13 },   /* N */
    [TF_A + 14] = { 2, 0x010, T_LETRA0 + 14 },   /* O */
    [TF_A + 15] = { 2, 0x004, T_LETRA0 + 15 },   /* P */
    [TF_A + 16] = { 2, 0x002, T_LETRA0 + 16 },   /* Q */
    [TF_A + 17] = { 2, 0x001, T_LETRA0 + 17 },   /* R */
    [TF_A + 18] = { 1, 0x040, T_LETRA0 + 18 },   /* S */
    [TF_A + 19] = { 1, 0x020, T_LETRA0 + 19 },   /* T */
    [TF_A + 20] = { 1, 0x010, T_LETRA0 + 20 },   /* U */
    [TF_A + 21] = { 1, 0x004, T_LETRA0 + 21 },   /* V */
    [TF_A + 22] = { 1, 0x002, T_LETRA0 + 22 },   /* W */
    [TF_A + 23] = { 1, 0x001, T_LETRA0 + 23 },   /* X */
    [TF_A + 24] = { 0, 0x040, T_LETRA0 + 24 },   /* Y */
    [TF_A + 25] = { 0, 0x020, T_LETRA0 + 25 },   /* Z */
    [TF_CAT]   = { 5, 0x080, T_CATALOGO },
    /* las cuentas de la ficha y el espacio de los nombres (KEY_NSPIRE_SPACE,
     * MULTIPLY, DIVIDE, EXP, SQU, LP y RP) */
    [TF_ESPACIO] = { 0, 0x010, T_ESPACIO },  [TF_POR]   = { 4, 0x100, T_POR },
    [TF_ENTRE]   = { 3, 0x100, T_ENTRE },    [TF_POT]   = { 4, 0x200, T_POT },
    [TF_CUAD]    = { 2, 0x200, T_CUAD },     [TF_PAR_A] = { 5, 0x020, T_PAR_A },
    [TF_PAR_C]   = { 5, 0x008, T_PAR_C },
    [TF_SHIFT] = { 7, 0x100, T_SHIFT },     [TF_CTRL]   = { 7, 0x200, T_CTRL },
};

int ent_tecla_fis(int tf, int *fila, unsigned *bit)
{
    if (tf < 0 || tf >= TF_N) return 0;
    if (fila) *fila = FIS[tf].fila;
    if (bit) *bit = FIS[tf].bit;
    return FIS[tf].fila >= 0;
}

static int es_mod(int tf) { return tf == TF_SHIFT || tf == TF_CTRL; }
static long absl(long a) { return a < 0 ? -a : a; }

/* --------------------------------------------------------------------------
 *  Estado
 * ----------------------------------------------------------------------- */
static int pw = TP_W_OMISION, ph = TP_H_OMISION;     /* el tamaño del pad */
static EntAjustes aj = { DESLIZAR_OMISION, REPETIR_OMISION, CLIC_ENTER_OMISION };
static EntCuentas cu;

static unsigned char abajo[TF_N];                    /* la foto anterior          */
static unsigned char mod_usado[2];                   /* shift, ctrl: fue con otra  */
static unsigned long t_ult;                          /* la hora de la última foto */
static unsigned long t_actividad;                    /* lo último que pasó        */

/* El pad: lo último que se leyó (con los fallos que se aguantan) y la cuña */
static struct {
    int contacto, hundido, x, y;
    int fallos;
    int cuna, cand, ncand;                           /* TF_ARRIBA..TF_CLIC, o -1  */
} pad;

/* La cola de flancos; id: el de la ráfaga que empieza con ese evento (0: no) */
static int cola[ENT_COLA];
static unsigned cola_id[ENT_COLA];
static int c_ini, c_n;

/* La ráfaga: la última tecla que bajó, mientras siga abajo */
static struct {
    int viva, tf, t, entregado, perfil, tope, pantalla;
    unsigned id;
    unsigned long t0, sig;                           /* cuándo bajó; la repetición siguiente */
    long debidos, hechos, reps;
} raf;
static unsigned raf_id;

/* El gesto: un contacto del dedo en el pad */
#define NV (TP_VEL_MUESTRAS + 1)
static struct {
    int activo, n_con, n_sin, x0, y0;                /* x0, y0: la primera muestra con contacto */
    int apagado, anulado, hundido, pantalla;
    UiTactil p;
    int ax, ay, lx, ly, arrancado, eje;
    long v[2];                                       /* recorrido desde el último paso (x1000) */
    int dir[2], uno_hecho[2];
    unsigned long t_mueve, bloqueo, t_quieto;        /* empezó a moverse; sin deslizar hasta; quieto desde */
    int qx, qy;                                      /* dónde empezó a estar quieto */
    int hx[NV], hy[NV], nh;                          /* las últimas posiciones (la velocidad) */
    int pend[2];                                     /* pasos por entregar       */
    int hubo, dado, suelta;                          /* dio algún paso (o lo tiene); entregó alguno; T_SUELTA pendiente */
    int reten;                                       /* los pasos esperan hasta t_reten (buscar la flecha) */
    unsigned long t_reten;
} ges;

/* El grabador */
static unsigned char grab_buf[GRAB_CABECERA + GRAB_MAX * GRAB_BYTES];
static int grab_estado, grab_n, grab_empezado;
static unsigned long grab_t0;
static unsigned short grab_fila[8];                  /* la matriz de la última grabada */

/* --------------------------------------------------------------------------
 *  La cola
 * ----------------------------------------------------------------------- */
static int es_esc(int t) { return (t & ~T_MODS) == T_ESC; }

static void mete(int t, unsigned id)
{
    int i, j;
    if (c_n == ENT_COLA) {
        cu.perdidos++;
        if (!es_esc(t)) return;                      /* llena: se pierde el nuevo... */
        /* ...salvo [esc]: sale el más nuevo que no sea [esc] (otro [esc]
         * que espera no se pisa); si todos lo son, con esos basta */
        for (j = c_n - 1; j >= 0 && es_esc(cola[(c_ini + j) % ENT_COLA]); j--) {}
        if (j < 0) return;
        for (; j < c_n - 1; j++) {
            cola[(c_ini + j) % ENT_COLA] = cola[(c_ini + j + 1) % ENT_COLA];
            cola_id[(c_ini + j) % ENT_COLA] = cola_id[(c_ini + j + 1) % ENT_COLA];
        }
        c_n--;
    }
    i = (c_ini + c_n) % ENT_COLA;
    cola[i] = t;
    cola_id[i] = id;
    c_n++;
}

/* --------------------------------------------------------------------------
 *  La ráfaga
 * ----------------------------------------------------------------------- */
/* ms escalados por el ajuste de repetición */
static long escala(long ms)
{
    if (aj.repetir == REPETIR_LENTA) return ms * REP_LENTA / 10;
    if (aj.repetir == REPETIR_RAPIDA) return ms * REP_RAPIDA / 10;
    return ms;
}

static long retardo_de(int perfil)
{
    switch (perfil) {
    case REP_LISTA:  return escala(REP_LISTA_RETARDO);
    case REP_CURSOR: return escala(REP_CURSOR_RETARDO);
    case REP_ESCALA: return escala(REP_ESCALA_RETARDO);
    case REP_BORRAR: return escala(REP_BORRAR_RETARDO);
    default:         return escala(REP_ESP_RETARDO);
    }
}

/* El periodo tras la repetición de la hora ts (el de LISTA se acorta a los
 * REP_LISTA_CAMBIO ms de repetir) */
static long periodo_de(int perfil, unsigned long ts)
{
    switch (perfil) {
    case REP_LISTA:
        return (long)(ts - (raf.t0 + (unsigned long)retardo_de(perfil))) < escala(REP_LISTA_CAMBIO)
               ? escala(REP_LISTA_PERIODO) : escala(REP_LISTA_PERIODO2);
    case REP_CURSOR: return escala(REP_CURSOR_PERIODO);
    case REP_ESCALA: return escala(REP_ESCALA_PERIODO);
    case REP_BORRAR: return escala(REP_BORRAR_PERIODO);
    default:         return escala(REP_ESP_PERIODO);
    }
}

/* Pasos de la repetición de la hora ts: x1, x2, x4, x8 por segundo mantenido */
static long mult_de(unsigned long ts)
{
    unsigned long m = ts - raf.t0;
    long k = m < REP_MULT_1S ? 1 : m < 2 * REP_MULT_1S ? 2 : m < 3 * REP_MULT_1S ? 4 : 8;
    return k < raf.tope ? k : raf.tope;
}

/* Cuenta las repeticiones que vencieron hasta t */
static void vence(unsigned long t)
{
    int n = 0;
    if (!raf.viva || raf.perfil <= 0) return;
    while ((long)(t - raf.sig) >= 0 && n++ < 10000) {
        raf.debidos += mult_de(raf.sig);
        raf.sig += (unsigned long)periodo_de(raf.perfil, raf.sig);
    }
}

static void muere_rafaga(void)
{
    if (raf.viva && raf.reps > 0) { mete(T_SUELTA, 0); cu.sueltas++; }
    raf.viva = 0;
}

/* --------------------------------------------------------------------------
 *  El gesto
 * ----------------------------------------------------------------------- */
/* ¿Esperan los pasos? Los primeros de un arranque, hasta t_reten, mientras
 * el dedo siga apoyado (al levantarlo, no buscaba ninguna flecha) */
static int retenidos(unsigned long t)
{
    return ges.reten && ges.activo && (long)(t - ges.t_reten) < 0;
}

/* Los pasos pendientes se tiran; T_SUELTA solo si se entregó alguno (los
 * retenidos que tira un clic no llegaron a ui.c: no hay ráfaga que cerrar) */
static void corta_pasos(void)
{
    if (retenidos(t_ult)) cu.buscados += absl(ges.pend[0]) + absl(ges.pend[1]);
    ges.pend[0] = ges.pend[1] = 0;
    if (ges.dado) { mete(T_SUELTA, 0); cu.sueltas++; }
    ges.hubo = ges.dado = 0;
    ges.suelta = 0;
    ges.reten = 0;
}

static void reancla(int x, int y, unsigned long t)
{
    ges.ax = ges.lx = x; ges.ay = ges.ly = y;
    ges.arrancado = 0;
    ges.v[0] = ges.v[1] = 0;
    ges.dir[0] = ges.dir[1] = 0;
    ges.nh = 0;
    ges.t_mueve = t;
}

static void empieza_gesto(unsigned long t)
{
    memset(&ges.p, 0, sizeof ges.p);
    ges.activo = 1;
    ges.n_sin = 0;
    ges.anulado = 0;
    ges.hundido = 0;
    ges.hubo = ges.dado = ges.reten = 0;
    ges.uno_hecho[0] = ges.uno_hecho[1] = 0;
    ges.pend[0] = ges.pend[1] = 0;
    ges.bloqueo = t;
    ges.pantalla = ui_pantalla();
    ges.apagado = !aj.deslizar || !ui_tactil(&ges.p) || !(ges.p.ejes & (TAC_X | TAC_Y));
    reancla(ges.x0, ges.y0, t);
    ges.t_quieto = t; ges.qx = ges.x0; ges.qy = ges.y0;
    t_actividad = t;
}

static void acaba_gesto(unsigned long t)
{
    ges.activo = 0;
    ges.n_con = 0;
    if (ges.hubo) { ges.suelta = 1; ges.hubo = 0; }
    t_actividad = t;
}

/* Pasos del eje e (0 = x, 1 = y). v es el recorrido desde el último paso
 * en unidades del pad por 1000: una milésima de W son pw unidades, y así
 * no se pierde nada redondeando muestra a muestra. Cada paso, en el mismo
 * sentido; para volver atrás, el paso más la histéresis */
static void pasos_eje(int e, unsigned long t)
{
    long dim = e ? ph : pw, paso = (long)(e ? ges.p.paso_y : ges.p.paso_x) * dim, h, u;
    int n = 0, bit = e ? TAC_Y : TAC_X;
    if (ges.p.uno & bit) {                               /* «pasar página»: uno por contacto */
        if (!ges.uno_hecho[e] && absl(ges.v[e]) >= (long)TP_PAGINA * dim && t - ges.t_mueve <= TP_PAGINA_MS) {
            ges.pend[e] += ges.v[e] > 0 ? 1 : -1;
            ges.uno_hecho[e] = 1;
            ges.hubo = 1;
        }
        return;
    }
    if (paso <= 0) return;
    h = paso * TP_HIST_PASO / 1000;
    if (h < (long)TP_HIST_MIN * dim) h = (long)TP_HIST_MIN * dim;
    while (n < TP_TOPE_MUESTRA) {
        if (ges.v[e] >= (u = paso + (ges.dir[e] < 0 ? h : 0))) {
            ges.v[e] -= u; ges.dir[e] = 1; ges.pend[e]++;
        } else if (-ges.v[e] >= (u = paso + (ges.dir[e] > 0 ? h : 0))) {
            ges.v[e] += u; ges.dir[e] = -1; ges.pend[e]--;
        } else break;
        n++;
        ges.hubo = 1;
    }
    if (n == TP_TOPE_MUESTRA) ges.v[e] = 0;               /* lo que sobra, se pierde */
}

/* El multiplicador por la velocidad de las últimas muestras */
static int balistica(void)
{
    long vx, vy, vel;
    int m;
    if (ges.p.balistica <= 1 || ges.nh < NV) return 1;
    vx = absl((long)(ges.hx[(ges.nh - 1) % NV] - ges.hx[ges.nh % NV])) * 1000 / pw;
    vy = absl((long)(ges.hy[(ges.nh - 1) % NV] - ges.hy[ges.nh % NV])) * 1000 / ph;
    vel = (vx > vy ? vx : vy) / TP_VEL_MUESTRAS;           /* milésimas por muestra */
    m = vel < TP_VEL_1 ? 1 : vel < TP_VEL_2 ? 2 : 4;
    return m < ges.p.balistica ? m : ges.p.balistica;
}

/* Una muestra del pad (leída) para el gesto */
static void gesto(unsigned long t)
{
    int toca = pad.contacto || pad.hundido, x = pad.x, y = pad.y;
    long jx, jy;
    if (!toca) {
        ges.n_con = 0;
        if (ges.activo && ++ges.n_sin >= TP_ACABA) acaba_gesto(t);
        return;
    }
    ges.n_sin = 0;
    if (!ges.activo) {
        if (ges.n_con == 0) { ges.x0 = x; ges.y0 = y; }
        if (++ges.n_con >= TP_EMPIEZA) empieza_gesto(t);
        return;
    }
    /* quieto o no (la energía). Y la deriva: sin haber arrancado, un dedo que
     * sale de su caja tras estar quieto en ella TP_DERIVA_MS o más mueve el
     * ancla a donde estuvo quieto. El pulgar que se apoya y resbala despacio
     * mientras se teclea no suma nunca el 8 %; un deslizamiento de verdad
     * sale de la caja en unas pocas muestras y cuenta desde donde se paró */
    if (absl((long)(x - ges.qx)) * 1000 / pw > TP_QUIETO || absl((long)(y - ges.qy)) * 1000 / ph > TP_QUIETO) {
        if (!ges.arrancado && !pad.hundido && !ges.hundido && t - ges.t_quieto >= TP_DERIVA_MS &&
            (ges.ax != ges.qx || ges.ay != ges.qy)) {
            reancla(ges.qx, ges.qy, t);
            cu.derivas++;
        }
        ges.qx = x; ges.qy = y; ges.t_quieto = t;
    }
    if (ges.apagado || ges.anulado) return;
    if (ui_pantalla() != ges.pantalla) {                  /* otra pantalla: nada hasta levantar */
        ges.anulado = 1;
        cu.anulados++;
        corta_pasos();
        return;
    }
    if (pad.hundido) { ges.hundido = 1; return; }          /* congelado: manda la flecha */
    if (ges.hundido) {                                     /* se soltó el clic: se re-ancla */
        ges.hundido = 0;
        reancla(x, y, t);
        ges.bloqueo = t + TP_BLOQUEO_CLIC;
        return;
    }
    if ((long)(t - ges.bloqueo) < 0) return;
    /* un salto: dos dedos o un fallo del chip */
    jx = absl((long)(x - ges.lx)) * 1000 / pw;
    jy = absl((long)(y - ges.ly)) * 1000 / ph;
    if (jx > TP_SALTO || jy > TP_SALTO) { reancla(x, y, t); return; }
    ges.hx[ges.nh % NV] = x; ges.hy[ges.nh % NV] = y; ges.nh++;
    if (!ges.arrancado) {
        /* el recorrido desde el ancla, en milésimas, en los ejes que cuentan */
        long ex = (ges.p.ejes & TAC_X) ? absl((long)(x - ges.ax)) * 1000 / pw : 0;
        long ey = (ges.p.ejes & TAC_Y) ? absl((long)(y - ges.ay)) * 1000 / ph : 0;
        ges.lx = x; ges.ly = y;
        if (ex < TP_QUIETO && ey < TP_QUIETO) ges.t_mueve = t;   /* aún no se movió */
        if (ex < TP_ARRANQUE && ey < TP_ARRANQUE) return;
        ges.arrancado = 1;
        cu.gestos++;
        t_actividad = t;
        /* buscar la flecha: quien apoya el dedo y lo lleva a una cuña para
         * hundirla pasa por aquí. En los perfiles de pasos gruesos (sin
         * balística: listas, la ficha, la rejilla) los pasos esperan hasta
         * TP_BUSCA_MS desde que el dedo echó a andar; si antes se hunde el
         * pad, la tecla del clic los tira (corta_pasos) y solo cuenta el clic */
        ges.reten = ges.p.balistica <= 1;
        ges.t_reten = ges.t_mueve + TP_BUSCA_MS;
        if ((ges.p.ejes & (TAC_X | TAC_Y)) == (TAC_X | TAC_Y)) {
            if (ex >= TP_EJE_DOBLE * ey) ges.eje = TAC_X;
            else if (ey >= TP_EJE_DOBLE * ex) ges.eje = TAC_Y;
            else ges.eje = TAC_X | TAC_Y;
        } else ges.eje = ges.p.ejes;
        ges.v[0] = (long)(x - ges.ax) * 1000;             /* los pasos, desde el ancla */
        ges.v[1] = (long)(y - ges.ay) * 1000;
    } else {
        long m = balistica();
        ges.v[0] += (long)(x - ges.lx) * 1000 * m;
        ges.v[1] += (long)(y - ges.ly) * 1000 * m;
        ges.lx = x; ges.ly = y;
    }
    if (ges.eje & TAC_X) pasos_eje(0, t);
    if (ges.eje & TAC_Y) pasos_eje(1, t);
    if (ges.pend[0] || ges.pend[1]) t_actividad = t;
}

/* --------------------------------------------------------------------------
 *  El pad: la muestra (con los fallos) y la cuña del clic
 * ----------------------------------------------------------------------- */
static int cuna_de(int x, int y, long *pu, long *pv)
{
    long u = (2L * x - pw) * 1000 / pw, v = (2L * y - ph) * 1000 / ph;
    *pu = u; *pv = v;
    if (u * u + v * v < (long)TP_CENTRO * TP_CENTRO) return TF_CLIC;
    if (absl(u) >= absl(v)) return u > 0 ? TF_DER : TF_IZQ;
    return v > 0 ? TF_ARRIBA : TF_ABAJO;
}

/* La cuña con el pad todavía hundido: solo cambia si la otra gana con margen */
static int cuna_margen(int x, int y, int actual)
{
    long u, v, r2;
    int c = cuna_de(x, y, &u, &v);
    r2 = u * u + v * v;
    if (c == actual) return actual;
    if (c == TF_CLIC) return r2 < (long)(TP_CENTRO - TP_CUNA_MARGEN) * (TP_CENTRO - TP_CUNA_MARGEN) ? c : actual;
    if (actual == TF_CLIC) return r2 >= (long)(TP_CENTRO + TP_CUNA_MARGEN) * (TP_CENTRO + TP_CUNA_MARGEN) ? c : actual;
    return absl(absl(u) - absl(v)) >= TP_CUNA_MARGEN ? c : actual;
}

/* 1 si hay una lectura nueva del pad */
static int lee_pad(const EntMuestra *m)
{
    int con, hun, x, y, antes = pad.hundido;
    if (m->pad == ENT_PAD_NO) return 0;
    if (m->pad == ENT_PAD_ERROR) {
        if (++pad.fallos <= TP_FALLOS) return 0;          /* se aguanta la anterior */
        con = hun = 0; x = pad.x; y = pad.y;               /* a la 4.ª, suelto */
    } else {
        pad.fallos = 0;
        con = m->contacto != 0; hun = m->hundido != 0;
        /* dentro del pad: una x de más (un fallo del chip) no desborda
         * u² + v² en el long de 32 bits del ARM (cuna_de) */
        x = m->x < pw ? m->x : pw; y = m->y < ph ? m->y : ph;
    }
    pad.contacto = con; pad.hundido = hun; pad.x = x; pad.y = y;
    if (hun && !antes) {
        long u, v;
        pad.cuna = cuna_de(x, y, &u, &v);
        pad.ncand = 0;
    } else if (hun) {
        int c = cuna_margen(x, y, pad.cuna);
        if (c == pad.cuna) pad.ncand = 0;
        else {
            if (c == pad.cand) pad.ncand++;
            else { pad.cand = c; pad.ncand = 1; }
            if (pad.ncand >= TP_CUNA_MUESTRAS) { pad.cuna = c; pad.ncand = 0; }
        }
    } else pad.cuna = -1;
    return 1;
}

/* --------------------------------------------------------------------------
 *  Las teclas: flancos y sueltas
 * ----------------------------------------------------------------------- */
static int mods_abajo(void)
{
    return (abajo[TF_SHIFT] ? T_CON_SHIFT : 0) | (abajo[TF_CTRL] ? T_CON_CTRL : 0);
}

/* ¿Puede ser un fantasma? La tecla k acaba de bajar y cierra un rectángulo
 * con otras tres que están abajo (su fila, su columna y la esquina de
 * enfrente): en una matriz sin diodos, tres teclas reales en tres esquinas
 * encienden la cuarta, y la real y la fantasma se ven igual */
static int rectangulo(const EntMuestra *m, int k)
{
    int f = FIS[k].fila, g;
    unsigned b = FIS[k].bit;
    if (f < 0) return 0;                                   /* las del pad */
    for (g = 0; g < 8; g++)
        if (g != f && (m->fila[g] & b) && (m->fila[f] & m->fila[g] & ~b)) return 1;
    return 0;
}

static void teclas(const EntMuestra *m, unsigned long t)
{
    unsigned char ahora[TF_N];
    int nuevas[TF_N], nn = 0, nmod = 0, i, otra = 0, mods, mano, fantasma = 0;
    for (i = 0; i < TF_N; i++) {
        if (FIS[i].fila >= 0) ahora[i] = (m->fila[(int)FIS[i].fila] & FIS[i].bit) != 0;
        else ahora[i] = pad.hundido && pad.cuna == i;
    }
    /* sueltas: la ráfaga muere; shift y ctrl sin usar se arman */
    for (i = 0; i < TF_N; i++) {
        if (!abajo[i] || ahora[i]) continue;
        t_actividad = t;
        if (raf.viva && raf.tf == i) muere_rafaga();
        if (es_mod(i) && !mod_usado[i == TF_CTRL]) mete(FIS[i].t, 0);
    }
    for (i = 0; i < TF_N; i++) {
        if (ahora[i] && !abajo[i]) { nuevas[nn++] = i; if (es_mod(i)) nmod++; }
        else if (ahora[i] && !es_mod(i)) otra = 1;          /* una que ya estaba abajo */
    }
    /* los fantasmas de la matriz: tres o más teclas nuevas en la misma foto
     * (sin shift ni ctrl), o una que cierra un rectángulo con otras tres
     * abajo (la real y la fantasma: las dos). No actúan ni dejan armar shift
     * y ctrl; [esc] sí, que nunca se pierde. Siguen abajo: al soltarse, nada */
    mano = nn - nmod >= ENT_FANTASMA;
    for (i = 0; i < nn; i++)
        if (nuevas[i] != TF_ESC && (mano || rectangulo(m, nuevas[i]))) { nuevas[i] = -1; fantasma = 1; cu.fantasmas++; }
    if (fantasma) mod_usado[0] = mod_usado[1] = 1;
    /* los modificadores de esta misma foto van con las teclas de esta foto */
    mods = (ahora[TF_SHIFT] ? T_CON_SHIFT : 0) | (ahora[TF_CTRL] ? T_CON_CTRL : 0);
    for (i = 0; i < nn; i++) {
        int k = nuevas[i];
        if (k < 0 || !es_mod(k)) continue;
        mod_usado[k == TF_CTRL] = (unsigned char)(otra || fantasma);   /* bajó en mitad de otra: no arma */
        t_actividad = t;
    }
    for (i = 0; i < nn; i++) {
        int k = nuevas[i], e;
        if (k < 0 || es_mod(k)) continue;
        t_actividad = t;
        /* una tecla corta el gesto (los pasos retenidos del dedo que buscaba
         * la flecha, también); si es de la matriz y el dedo sigue apoyado,
         * este empieza de nuevo desde donde está (un pulgar que resbala
         * mientras se teclea tiene que volver a recorrer el 8 %) */
        if (ges.pend[0] || ges.pend[1] || ges.hubo) corta_pasos();
        if (FIS[k].fila >= 0 && ges.activo) reancla(pad.x, pad.y, t);
        if (k == TF_CLIC && !aj.clic_enter) continue;      /* «nada» */
        e = FIS[k].t | mods;
        if (mods & T_CON_SHIFT) mod_usado[0] = 1;
        if (mods & T_CON_CTRL) mod_usado[1] = 1;
        muere_rafaga();
        raf.viva = 1; raf.tf = k; raf.t = e; raf.t0 = t;
        raf.entregado = 0; raf.perfil = -1; raf.tope = 1;
        raf.debidos = raf.hechos = raf.reps = 0;
        if (++raf_id == 0) raf_id = 1;                     /* 0 es «ninguna» */
        raf.id = raf_id;
        mete(e, raf.id);
        cu.flancos++;
    }
    memcpy(abajo, ahora, sizeof abajo);
}

/* --------------------------------------------------------------------------
 *  El grabador
 * ----------------------------------------------------------------------- */
static void w16(unsigned char *p, unsigned v) { p[0] = (unsigned char)(v & 0xFF); p[1] = (unsigned char)((v >> 8) & 0xFF); }
static void w32(unsigned char *p, unsigned long v) { w16(p, (unsigned)(v & 0xFFFF)); w16(p + 2, (unsigned)((v >> 16) & 0xFFFF)); }
static unsigned r16(const unsigned char *p) { return (unsigned)p[0] | (unsigned)p[1] << 8; }
static unsigned long r32(const unsigned char *p) { return (unsigned long)r16(p) | (unsigned long)r16(p + 2) << 16; }

static void cabecera_grab(unsigned char *b, int n)
{
    memcpy(b, "CESM", 4);
    b[4] = 1; b[5] = 0;
    w16(b + 6, (unsigned)pw); w16(b + 8, (unsigned)ph); w16(b + 10, (unsigned)n);
}

/* Una foto al grabador. Las que no cambian nada no se guardan: sin pad, con
 * la matriz de la anterior y sin nada que sacar a esa hora, ent_muestra no
 * toca el estado y el bucle no entrega nada (reproducir sin ellas da los
 * mismos eventos). Así caben los 10 s aunque el bucle haga 2 o 3 fotos por
 * tick (entre eventos y tras el cuadro) */
static void graba(const EntMuestra *m, unsigned long t)
{
    unsigned char *p;
    int i, nada = 0;
    if (!grab_empezado) { grab_empezado = 1; grab_t0 = t; }
    else nada = m->pad == ENT_PAD_NO && memcmp(m->fila, grab_fila, sizeof grab_fila) == 0 && !ent_pendiente(t);
    if (!nada && grab_n < GRAB_MAX) {
        memcpy(grab_fila, m->fila, sizeof grab_fila);
        p = grab_buf + GRAB_CABECERA + grab_n * GRAB_BYTES;
        w32(p, t - grab_t0);
        for (i = 0; i < 8; i++) w16(p + 4 + 2 * i, m->fila[i]);
        p[20] = m->pad; p[21] = m->contacto; p[22] = m->hundido;
        w16(p + 23, m->x); w16(p + 25, m->y);
        grab_n++;
    }
    if (grab_n >= GRAB_MAX || t - grab_t0 >= GRAB_MS) {
        cabecera_grab(grab_buf, grab_n);
        grab_estado = arch_bytes_guardar("/ces_muestras.tns", grab_buf, GRAB_CABECERA + grab_n * GRAB_BYTES)
                      ? GRAB_HECHO : GRAB_ERROR;
    }
}

void ent_grabar(void) { grab_estado = GRAB_GRABANDO; grab_n = 0; grab_empezado = 0; }
void ent_grabacion_olvida(void) { grab_estado = GRAB_NADA; grab_n = 0; grab_empezado = 0; }

int ent_grabacion(int *n, int *ms_quedan)
{
    if (n) *n = grab_n;
    if (ms_quedan) {
        long q = grab_estado != GRAB_GRABANDO ? 0 : !grab_empezado ? GRAB_MS : GRAB_MS - (long)(t_ult - grab_t0);
        *ms_quedan = (int)(q < 0 ? 0 : q);
    }
    return grab_estado;
}

int ent_grabacion_bytes(unsigned char *buf, int cap)
{
    int n = GRAB_CABECERA + grab_n * GRAB_BYTES;
    if (n > cap) return 0;
    memcpy(buf, grab_buf, (size_t)n);
    cabecera_grab(buf, grab_n);
    return n;
}

int ent_grabacion_lee(const unsigned char *b, int nbytes, int *w, int *h,
                      EntMuestra *m, unsigned long *t, int max, int *n)
{
    int k, i, total;
    if (nbytes < GRAB_CABECERA || memcmp(b, "CESM", 4) != 0 || b[4] != 1) return 0;
    *w = (int)r16(b + 6); *h = (int)r16(b + 8); total = (int)r16(b + 10);
    if (nbytes < GRAB_CABECERA + total * GRAB_BYTES || *w <= 0 || *h <= 0) return 0;
    for (k = 0; k < total && k < max; k++) {
        const unsigned char *p = b + GRAB_CABECERA + k * GRAB_BYTES;
        t[k] = r32(p);
        for (i = 0; i < 8; i++) m[k].fila[i] = (unsigned short)r16(p + 4 + 2 * i);
        m[k].pad = p[20]; m[k].contacto = p[21]; m[k].hundido = p[22];
        m[k].x = (unsigned short)r16(p + 23); m[k].y = (unsigned short)r16(p + 25);
    }
    *n = k;
    return 1;
}

/* --------------------------------------------------------------------------
 *  La API
 * ----------------------------------------------------------------------- */
/* Un W o un H de touchpad_getinfo (o de una grabación) que no es creíble:
 * el de omisión. Con las x dentro del pad, u y v de la cuña no pasan de
 * ±1000 sea cual sea W; con W ≤ 16384, la histéresis (paso × 250) cabe en
 * 2^31 */
static int dim_pad(int d, int omision) { return d >= TP_DIM_MIN && d <= TP_DIM_MAX ? d : omision; }

void ent_init(int w, int h, const EntMuestra *m, unsigned long t)
{
    int i;
    pw = dim_pad(w, TP_W_OMISION);
    ph = dim_pad(h, TP_H_OMISION);
    memset(&pad, 0, sizeof pad);
    pad.cuna = pad.cand = -1;
    memset(&raf, 0, sizeof raf);
    memset(&ges, 0, sizeof ges);
    memset(&cu, 0, sizeof cu);
    c_ini = c_n = 0;
    mod_usado[0] = mod_usado[1] = 1;                     /* shift abajo al arrancar: no arma */
    t_ult = t_actividad = t;
    if (m) lee_pad(m);
    for (i = 0; i < TF_N; i++) {
        if (FIS[i].fila >= 0) abajo[i] = m ? (m->fila[(int)FIS[i].fila] & FIS[i].bit) != 0 : 0;
        else abajo[i] = pad.hundido && pad.cuna == i;
    }
}

void ent_muestra(const EntMuestra *m, unsigned long t)
{
    int nuevo_pad;
    t_ult = t;
    nuevo_pad = lee_pad(m);
    teclas(m, t);
    if (nuevo_pad) gesto(t);
    if (grab_estado == GRAB_GRABANDO) graba(m, t);
}

int ent_saca(unsigned long t)
{
    UiRepe u;
    int vueltas = 0;
    t_ult = t;
    while (vueltas++ < 4) {
        /* 1. la cola */
        if (c_n) {
            int e = cola[c_ini];
            unsigned id = cola_id[c_ini];
            c_ini = (c_ini + 1) % ENT_COLA;
            c_n--;
            if (id && raf.viva && raf.id == id) { raf.entregado = 1; raf.pantalla = ui_pantalla(); }
            t_actividad = t;
            return e;
        }
        /* 2. los pasos del dedo (si no esperan: buscar la flecha) */
        while ((ges.pend[0] || ges.pend[1]) && !retenidos(t)) {
            int ex = ges.pend[0] != 0, sg = ges.pend[ex ? 0 : 1] > 0 ? 1 : -1;
            int k = ex ? (sg > 0 ? T_DER : T_IZQ) : (sg > 0 ? T_ARRIBA : T_ABAJO);
            k |= T_TACTIL | T_REPE | mods_abajo();
            if (ui_pantalla() != ges.pantalla) { ges.pend[0] = ges.pend[1] = 0; break; }
            u.tope = 1;
            if (!ui_repite(k, &u)) { ges.pend[ex ? 0 : 1] = 0; continue; }    /* el borde */
            ges.pend[ex ? 0 : 1] -= sg;
            if (abajo[TF_SHIFT]) mod_usado[0] = 1;
            if (abajo[TF_CTRL]) mod_usado[1] = 1;
            ges.dado = 1;
            cu.pasos_tactiles++;
            t_actividad = t;
            return k;
        }
        if (ges.suelta && !ges.pend[0] && !ges.pend[1]) { ges.suelta = ges.dado = 0; cu.sueltas++; return T_SUELTA; }
        /* 3. la ráfaga de la tecla */
        if (!raf.viva || !raf.entregado) return T_NADA;
        if (raf.perfil < 0) {                              /* lo primero: su perfil */
            u.tope = 1;
            raf.perfil = aj.repetir == REPETIR_NO || ui_pantalla() != raf.pantalla ? REP_NO : ui_repite(raf.t | T_REPE, &u);
            if (raf.perfil <= REP_NO || raf.perfil >= REP_N) { raf.viva = 0; return T_NADA; }
            raf.tope = u.tope > 1 ? u.tope : 1;
            raf.sig = raf.t0 + (unsigned long)retardo_de(raf.perfil);
        }
        vence(t);
        if (raf.debidos <= raf.hechos) return T_NADA;
        u.tope = 1;
        if (ui_pantalla() != raf.pantalla || !ui_repite(raf.t | T_REPE, &u)) { muere_rafaga(); continue; }
        raf.hechos++;
        raf.reps++;
        cu.repeticiones++;
        t_actividad = t;
        return raf.t | T_REPE;
    }
    return T_NADA;
}

int ent_pendiente(unsigned long t)
{
    if (c_n || ges.suelta || ((ges.pend[0] || ges.pend[1]) && !retenidos(t))) return 1;
    if (!raf.viva || !raf.entregado) return 0;
    if (raf.perfil < 0) return 1;
    vence(t);
    return raf.debidos > raf.hechos;
}

void ent_cuadro(unsigned long t)
{
    vence(t);
    if (raf.viva && raf.debidos - raf.hechos > REP_TOPE_CUADRO) raf.hechos = raf.debidos - REP_TOPE_CUADRO;
}

int ent_suelto(void)
{
    int i;
    for (i = 0; i < TF_N; i++) if (abajo[i]) return 0;
    return !pad.hundido;
}

int ent_cada(unsigned long t)
{
    unsigned long d;
    if (grab_estado == GRAB_GRABANDO || raf.viva || c_n || !ent_suelto()) return 1;
    if (ges.n_con && !ges.activo) return 1;                /* un dedo que llega */
    if (ges.activo) return t - ges.t_quieto >= EN_QUIETO_MS ? EN_REPOSO_CADA : 1;
    d = t - t_actividad;
    return d < EN_ACTIVO_MS ? 1 : d < EN_REPOSO_MS ? EN_REPOSO_CADA : EN_PROFUNDO_CADA;
}

void ent_ajustes(EntAjustes *a) { *a = aj; }

void ent_pon_ajustes(const EntAjustes *a)
{
    aj.deslizar = a->deslizar <= 1 ? a->deslizar : DESLIZAR_OMISION;
    aj.repetir = a->repetir < REPETIR_N ? a->repetir : REPETIR_OMISION;
    aj.clic_enter = a->clic_enter <= 1 ? a->clic_enter : CLIC_ENTER_OMISION;
}

void ent_ajustes_omision(void)
{
    aj.deslizar = DESLIZAR_OMISION;
    aj.repetir = REPETIR_OMISION;
    aj.clic_enter = CLIC_ENTER_OMISION;
}

/* El touchpad que dijo main.c (solo para enseñarlo: la capa usa pw y ph) */
static int pad_tipo = -1, pad_w, pad_h;
void ent_pad_visto(int tipo, int w, int h) { pad_tipo = tipo; pad_w = w; pad_h = h; }
int  ent_pad_tipo(int *w, int *h) { *w = pad_w; *h = pad_h; return pad_tipo; }
void ent_pad_usa(int *w, int *h) { *w = pw; *h = ph; }

/* --------------------------------------------------------------------------
 *  El reloj del plan A, vigilado (ver entrada.h). Todo en 64 bits: el ARM
 *  tiene long de 32 y 2^32 cuentas por 10^8 no caben
 * ----------------------------------------------------------------------- */
typedef unsigned long long U64;

/* ¿d cuentas son un tick de idle() (± la tolerancia)? -1 menos, 0 sí, 1 más */
static int cmp_tick(U64 d)
{
    U64 x = d * 1000000ULL * 100ULL, tick = (U64)ENT_SP804_HZ * ENT_TICK_US;
    if (x < tick * (100 - RELOJ_TOLERANCIA)) return -1;
    return x > tick * (100 + RELOJ_TOLERANCIA) ? 1 : 0;
}

int ent_reloj_abre(EntReloj *r, const unsigned *v, int n)
{
    int i, bien = v && n > 0;
    memset(r, 0, sizeof *r);
    r->saltos_a = -1;                                       /* el contador, sin partida */
    for (i = 0; i < n && bien; i++) bien = cmp_tick((U64)(unsigned)(v[i] - v[i + 1])) == 0;   /* baja */
    r->plan_a = bien;
    if (bien) r->ant = r->ant_tick = v[n];
    return bien;
}

/* Lo que avanzó el contador desde la lectura anterior, en µs */
static void avanza(EntReloj *r, unsigned v)
{
    U64 d = (unsigned)(r->ant - v), x;
    r->ant = v;
    if (d * 1000ULL > (U64)RELOJ_SALTO_MS * ENT_SP804_HZ) {   /* un salto: un tick */
        r->saltos++;
        r->us += ENT_TICK_US;
        return;
    }
    x = d * 1000000ULL + r->resto;
    r->us += x / ENT_SP804_HZ;
    r->resto = x % ENT_SP804_HZ;
}

unsigned long ent_reloj_ms(EntReloj *r, unsigned v)
{
    if (r->plan_a) avanza(r, v);
    return (unsigned long)(r->us / 1000ULL);
}

void ent_reloj_tick(EntReloj *r, unsigned v)
{
    U64 d;
    int c;
    if (!r->plan_a) { r->us += ENT_TICK_US; return; }
    avanza(r, v);
    /* entre dos idle() pasa un tick como poco: si el contador no lo vio
     * (parado, o más lento), no es de fiar */
    d = (unsigned)(r->ant_tick - v);
    c = cmp_tick(d);
    if (c < 0) {
        if (++r->lentos >= RELOJ_LENTOS) { r->plan_a = 0; r->cambios++; }
    } else r->lentos = 0;
    /* un tick justo (el trabajo entre dos idle() cupo en uno): lo que dura
     * un tick del SO, para enseñarlo (el tick de idle() acaba en el borde) */
    if (c == 0) { r->tick_suma += d; r->tick_n++; }
    r->ant_tick = v;
}

void ent_reloj_rtc(EntReloj *r, unsigned long rtc_s, unsigned v)
{
    if (r->idle_n++ == 0 || rtc_s < r->rtc_ult) {        /* el primero, o el RTC va atrás: de nuevo */
        r->rtc_ult = rtc_s;
        r->rtc_n = 0;
        return;
    }
    if (rtc_s == r->rtc_ult) return;
    /* un salto del RTC: pasó en el último idle() (a un tick como mucho) */
    r->rtc_ult = rtc_s;
    if (r->rtc_n++ == 0) {
        r->rtc0 = rtc_s;
        r->idle0 = r->idle_n;
        r->seg = 0;
        r->idle_seg = 0;
    } else {
        r->seg = rtc_s - r->rtc0;
        r->idle_seg = r->idle_n - r->idle0;
    }
    /* el contador, mientras siga el plan A (si se cayó, lo que se midió
     * hasta entonces); si saltó (otro lo reprogramó), desde este salto */
    if (!r->plan_a) return;
    if (r->saltos_a != r->saltos) {
        r->rtc_a = rtc_s;
        r->v_a = v;
        r->saltos_a = r->saltos;
        r->seg_a = r->cuentas = 0;
    } else {
        r->seg_a = rtc_s - r->rtc_a;
        r->cuentas = (unsigned)(r->v_a - v);
    }
}

void ent_reloj_medido(const EntReloj *r, unsigned long *hz, unsigned long *tick_us, unsigned long *idle10)
{
    U64 f = r->seg_a > 0 ? (U64)r->cuentas / r->seg_a : 0;
    *hz = (unsigned long)f;
    if (f == 0) f = ENT_SP804_HZ;
    *tick_us = r->tick_n > 0 ? (unsigned long)((r->tick_suma * 1000000ULL / (U64)r->tick_n + f / 2) / f) : 0;
    *idle10 = r->seg > 0 ? (unsigned long)((U64)r->idle_seg * 10ULL / r->seg) : 0;
}

static const EntReloj *reloj_visto;
void ent_reloj_a_la_vista(const EntReloj *r) { reloj_visto = r; }
const EntReloj *ent_reloj_visto(void) { return reloj_visto; }

void ent_cuentas(EntCuentas *c)
{
    *c = cu;
    c->rafaga = raf.viva;
    c->contacto = ges.activo;
}
