/* ============================================================================
 *  main.c  -  Hakaru CES en la TI-Nspire CX II CAS (Ndless)
 *
 *  Solo el hardware: la FOTO (las 8 medias palabras de la matriz del
 *  teclado y UN touchpad_scan), el reloj, idle() y el volcado. Lo demás es
 *  de la capa de entrada (entrada.c: flancos, repetición, touchpad), del
 *  bucle (bucle.c) y de la interfaz (ui.c), que no saben nada del hardware
 *  y por eso se prueban en el PC. Al arrancar y al salir, la sesión.
 *
 *  El reloj en ms (lo necesita la repetición; el RTC va a saltos de 1 s):
 *    plan A: el 2.º contador del SP804 de 0x900D0000 (el 1.º es el tick del
 *            SO, el de idle y msleep), libre, de 32 bits y sin interrupción.
 *            Solo si al arrancar está apagado y si en RELOJ_CALIBRA ticks de
 *            idle() da la frecuencia que se espera; se restaura al salir si
 *            sigue como se dejó. Vigilado (ent_reloj_*, en entrada.c): un
 *            salto cuenta un tick y, si se para, el plan B;
 *    plan B: las vueltas de idle() por ENT_TICK_US (el tiempo solo avanza
 *            esperando: en una pantalla cara la repetición va más despacio).
 *  Para calibrar, el panel Controles enseña el plan, sus cambios y saltos,
 *  las cuentas del contador por segundo del RTC (ENT_SP804_HZ), el tick de
 *  idle() (ENT_TICK_US) y los idle() por segundo (ent_reloj_rtc); y el
 *  touchpad: Synaptics o CapTIvate (tipo_pad) y su W×H (TP_*_OMISION).
 *
 *  El turbo (turbo.h): aquí solo su hardware (TB_HW: los registros de la
 *  PMU, las IRQ del SO, 32 kHz y el RTC); la lógica se prueba en el PC. NO
 *  va en la versión: ninguna fuente de hoy respalda escribir solo
 *  0x90140030 (turbo.h). Solo con HAKARU_TURBO (make TURBO=1, para probarlo
 *  en la calculadora) se le da el hardware; sin él, turbo_hay() = 0: ni la
 *  fila de Controles ni ningún registro de la PMU. Con él, se sale a 396 por
 *  cualquier camino: turbo_sale antes de guardar la sesión y con atexit (el
 *  crt0 de Ndless llama a exit al volver de main). Ndless no tiene gancho
 *  para el apagado del SO: apagar, un reset o quedarse sin pilas pasan por
 *  el boot1, que vuelve a poner 396.
 *
 *  Los relojes, frente al turbo (el APB es la CPU/4: sube con ella): el plan
 *  A y el tick de idle() son el SP804 de 0x900D0000, a 32 kHz (msleep lo
 *  programa así en la CX II, sleep.c); el banco, el RTC (su oscilador). Nada
 *  va con el APB: no hay que compensar M. La prueba del turbo lo comprueba
 *  (el SP804 contra el RTC, a 396 y a M).
 * ========================================================================= */
#include <libndls.h>
#include "gfx.h"
#include "ui.h"
#include "banco.h"
#include "entrada.h"
#include "bucle.h"
#include "turbo.h"

#define TECLADO  ((volatile unsigned short *)0x900E0010)   /* 8 filas, 1 = abajo */
#define SP_CARGA (*(volatile unsigned *)0x900D0020)
#define SP_VALOR (*(volatile unsigned *)0x900D0024)
#define SP_CTRL  (*(volatile unsigned *)0x900D0028)
#define SP_LIBRE 0x82u                                      /* en marcha, 32 bits, sin IRQ */
#define RTC_SEG  (*(volatile unsigned *)0x90090000)         /* el RTC: segundos */

static int reloj_sp;                          /* 1: el contador lo pusimos en marcha */
static unsigned sp_carga, sp_ctrl;            /* como estaba */
static EntReloj rel;                          /* la cuenta, vigilada */

static void reloj_abre(void)
{
    unsigned v[RELOJ_CALIBRA + 1];
    int i;
    sp_ctrl = SP_CTRL;
    reloj_sp = 0;
    if (ENT_RELOJ_SP804 && !(sp_ctrl & 0x80u)) {           /* en uso: plan B */
        sp_carga = SP_CARGA;
        SP_CARGA = 0xFFFFFFFFu;
        SP_CTRL = SP_LIBRE;
        reloj_sp = 1;
        idle(); idle();                       /* al borde de un tick (una interrupción vieja) */
        for (i = 0; i <= RELOJ_CALIBRA; i++) { if (i) idle(); v[i] = SP_VALOR; }
        if (ent_reloj_abre(&rel, v, RELOJ_CALIBRA)) return;
        SP_CTRL = sp_ctrl; SP_CARGA = sp_carga;             /* no da la frecuencia: plan B */
        reloj_sp = 0;
    }
    ent_reloj_abre(&rel, 0, 0);
}

/* Como estaba, si sigue como lo dejamos (si otro lo reprogramó, es suyo) */
static void reloj_cierra(void)
{
    if (reloj_sp && SP_CTRL == SP_LIBRE) { SP_CTRL = sp_ctrl; SP_CARGA = sp_carga; }
}

static unsigned long reloj(Plataforma *pf)
{
    (void)pf;
    return ent_reloj_ms(&rel, rel.plan_a ? SP_VALOR : 0u);
}

static void foto(Plataforma *pf, EntMuestra *m, int con_pad)
{
    int i;
    (void)pf;
    for (i = 0; i < 8; i++) m->fila[i] = TECLADO[i];
    m->pad = ENT_PAD_NO;
    if (con_pad && is_touchpad) {
        touchpad_report_t r;
        if (touchpad_scan(&r) != 0) m->pad = ENT_PAD_ERROR;
        else {
            m->pad = ENT_PAD_OK;
            m->contacto = r.contact; m->hundido = r.pressed;
            m->x = r.x; m->y = r.y;
        }
    }
}

/* idle() y la cuenta; y, para calibrar (Controles lo enseña), el contador
 * contra los saltos del RTC */
static void espera(Plataforma *pf)
{
    unsigned v;
    (void)pf;
    idle();
    v = rel.plan_a ? SP_VALOR : 0u;
    ent_reloj_tick(&rel, v);
    ent_reloj_rtc(&rel, RTC_SEG, v);
}
static void vuelca(Plataforma *pf) { (void)pf; gfx_flush(); }

/* ¿Synaptics o CapTIvate? Ndless no lo publica: en la CX II envuelve las
 * llamadas del touchpad según el que haya (ndless/src/resources/syscalls.c,
 * sc_install_compat, con el bit 0 del campo 5400 del manuf). El envoltorio
 * del CapTIvate solo traduce las dos lecturas de libndls (0x04-0x07, el
 * tamaño, y las que acaban en 0x0A, el informe) y rechaza cualquier otra; el
 * del Synaptics pasa la lectura al pad. Así que una lectura inocua de otro
 * tramo (0x00-0x01 de la página 4, la de siempre, que touchpad_scan ya lee
 * entera) lo dice: si no sale, CapTIvate (tres intentos, por si el I2C
 * falla una vez). La CX sin II y la clásica solo tienen el Synaptics. */
static int tipo_pad(void)
{
    unsigned char b[2];
    int i;
    if (!is_touchpad) return ENT_TP_NINGUNO;
    if (!is_cx2) return ENT_TP_SYNAPTICS;
    for (i = 0; i < 3; i++) if (touchpad_read(0x00, 0x01, b)) return ENT_TP_SYNAPTICS;
    return ENT_TP_CAPTIVATE;
}

/* El banco de tiempos pregunta por [esc] a mitad de cada medición: solo la
 * matriz, sin I2C (no mete ruido en lo que mide) */
static int esc_abajo(void) { return isKeyPressed(KEY_NSPIRE_ESC); }

#if defined(HAKARU_TURBO) && HAKARU_TURBO
/* ---- El hardware del turbo: solo leer la PMU (0x90140020, 24, 30 y 810),
 *      escribir 0x90140030 (y nada más), las IRQ del SO y los relojes.
 *      Solo con HAKARU_TURBO (make TURBO=1): fuera de la versión ---- */
#define PMU(d)    (*(volatile unsigned *)(d))
#define SP1_CARGA (*(volatile unsigned *)0x900D0000)       /* el 1.er contador: el tick del SO */
#define SP1_VALOR (*(volatile unsigned *)0x900D0004)
#define SP1_CTRL  (*(volatile unsigned *)0x900D0008)

static int tb_cx2(void)                                   /* la syscall, una vez */
{
    static int cx2 = -1;
    if (cx2 < 0) cx2 = is_cx2 ? 1 : 0;
    return cx2;
}
static unsigned tb_lee(unsigned d)
{
    if (d != TURBO_R_DIV && d != TURBO_R_PEND && d != TURBO_R_PLL && d != TURBO_R_FUENTE) return 0;
    return PMU(d);
}
static void tb_pon_pll(unsigned v) { PMU(TURBO_R_PLL) = v; }   /* la única escritura (turbo.c la filtra) */
/* Nucleus: -1 las cierra, 0 las abre; devuelve cómo estaban (NoverII,
 * _show_msgbox.c de Ndless). Un programa de Ndless corre con ellas cerradas
 * (ld_exec, ploaderhook.c): turbo.c las abre solo mientras espera a que el
 * SO atienda INT_POWER */
static int tb_irq(int m) { return TCT_Local_Control_Interrupts(m); }
/* Cuentas de 32 kHz: el 1.er contador del SP804 (el tick que atiende idle()),
 * hacia abajo. Cada lectura suma lo que bajó desde la anterior, con su vuelta
 * (periódico: hasta su carga; libre: 32 o 16 bits) y por su divisor (1, 16 o
 * 256). Vale leído más a menudo que una vuelta (~10 ms): el turbo lo lee en
 * bucles cerrados, y solo mira diferencias dentro de uno */
static unsigned tb_t32(void)
{
    static unsigned ant, cuenta;
    unsigned v = SP1_VALOR, c = SP1_CTRL, d, div = (c >> 2) & 3u;
    if (c & 0x40u) d = v <= ant ? ant - v : ant + (SP1_CARGA - v) + 1u;
    else if (c & 0x02u) d = ant - v;
    else d = (ant - v) & 0xFFFFu;
    ant = v;
    return cuenta += d << (div > 2u ? 8u : 4u * div);
}
static unsigned tb_rtc(void) { return RTC_SEG; }
/* Alguna tecla abajo, solo la matriz (sin I2C): turbo.c no abre las IRQ con
 * una tecla abajo, que el SO leería (ploaderhook.c espera lo mismo al volver) */
static int tb_tecla(void)
{
    int i;
    for (i = 0; i < 8; i++) if (TECLADO[i]) return 1;
    return 0;
}
static const TurboHw TB_HW = { tb_cx2, tb_lee, tb_pon_pll, tb_irq, tb_t32, tb_rtc, tb_tecla };
#endif

int main(int argc, char **argv)
{
    static Plataforma pf;
    touchpad_info_t *tp = is_touchpad ? touchpad_getinfo() : 0;

#if defined(HAKARU_TURBO) && HAKARU_TURBO
    turbo_hw(&TB_HW);                         /* apagado: nada se toca hasta Controles */
    turbo_salida(esc_abajo);
#endif                                        /* sin él, el turbo no es de la versión */
    atexit(turbo_sale);                       /* salga por donde salga, a 396 */
    gfx_init();
    ui_init();
    banco_salida(esc_abajo);
    /* Ndless pasa en argv[0] la ruta del .tns: ahí se guardan los problemas */
    if (argc > 0 && argv[0]) ui_carpeta(argv[0]);
    /* el circuito de la última vez (ces_sesion.tns) y los controles
     * (ces_controles.tns): solo aquí, nunca en ui_init (el arnés) */
    ui_sesion_abrir();
    ui_controles_abrir();
    ui_trabajo(BU_OPS_TICK);
    reloj_abre();
    ent_reloj_a_la_vista(&rel);               /* Controles enseña el plan y lo medido */

    pf.foto = foto; pf.reloj = reloj; pf.espera = espera; pf.vuelca = vuelca;
    pf.w = tp ? tp->width : 0;
    pf.h = tp ? tp->height : 0;
    ent_pad_visto(tipo_pad(), pf.w, pf.h);    /* Controles lo enseña: para calibrar la capa */
    bucle_inicia(&pf);                        /* el [enter] que lanzó el .tns no cuenta */
    while (bucle_paso(&pf)) {}
    bucle_suelta(&pf);                        /* que el [esc] no le llegue al SO */

    turbo_sale();                             /* a 396 antes de escribir nada; la marca, fuera */
    reloj_cierra();
    ui_sesion_guardar();                      /* al volver, «Seguir» lo recupera */
    gfx_deinit();
    return 0;
}
