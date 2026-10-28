/* ============================================================================
 *  bucle.c  -  El bucle de Hakaru CES (ver bucle.h)
 * ========================================================================= */
#include <string.h>
#include "bucle.h"
#include "ui.h"

/* Una foto a la capa. El pad, si toca por la energía (o si se pide) */
static void foto(Plataforma *pf, int pad)
{
    EntMuestra m;
    unsigned long t = pf->reloj(pf);
    if (pad < 0) pad = t - pf->t_pad + 1UL >= (unsigned long)ent_cada(t) * ENT_TICK_US / 1000UL;
    memset(&m, 0, sizeof m);
    pf->foto(pf, &m, pad);
    if (m.pad != ENT_PAD_NO) pf->t_pad = t;
    ent_muestra(&m, t);
}

void bucle_inicia(Plataforma *pf)
{
    EntMuestra m;
    unsigned long t = pf->reloj(pf);
    memset(&m, 0, sizeof m);
    pf->foto(pf, &m, 1);
    ent_init(pf->w, pf->h, &m, t);
    pf->t_pad = t;
    pf->t_anima = t * BU_ANIMA_HZ;
    pf->t_cuadro = t;
    pf->repinta = 1;
    pf->pant = ui_pantalla();
    pf->rotulo = ui_rotulo();
}

int bucle_paso(Plataforma *pf)
{
    unsigned long t, t0;
    int a, n = 0, e, trabaja, dibuja = pf->repinta;

    /* 1. la foto: la matriz siempre; el pad según la energía */
    foto(pf, -1);
    t = pf->reloj(pf);
    /* 2. los eventos, con la matriz entre uno y otro */
    if (ent_pendiente(t)) {
        ent_cuadro(t);
        t0 = t;
        while (n < BU_EVENTOS && (e = ent_saca(t)) != T_NADA) {
            int sigue = ui_tecla(e);
            n++;
            if (pf->tras_tecla) pf->tras_tecla(pf, e);
            if (!sigue) return 0;
            t = pf->reloj(pf);
            if (t - t0 >= BU_MS) break;
            foto(pf, 0);
            t = pf->reloj(pf);
        }
        if (n) dibuja = 1;
    }
    /* 3. la animación y el trabajo */
    a = pf->sin_animar ? 0 : ui_animando();
    trabaja = a == UI_TRABAJANDO;
    if (trabaja) {
        ui_tick();
        if (pf->tras_tick) pf->tras_tick(pf);
        t = pf->reloj(pf);
        /* la barra: cada BU_BARRA_MS, o cada BU_BARRA_TICKS de trabajo (con el
         * reloj del plan B el tiempo no avanza mientras se trabaja) */
        if (ui_animando() != UI_TRABAJANDO || t - pf->t_cuadro >= BU_BARRA_MS || ++pf->n_trabajo >= BU_BARRA_TICKS)
            dibuja = 1;
    } else if (a && t * BU_ANIMA_HZ - pf->t_anima >= 1000UL) {
        /* A plazo fijo, contado con el reloj: t_anima es la hora del tick
         * que toca en ms × BU_ANIMA_HZ (un tick son 1000: sin decimales).
         * Solo se mira tras cada idle(), así que un tick cae en el primer
         * tick del SO después de su hora (3 o 4 de 10,25 ms) y la media es
         * BU_ANIMA_HZ con cualquier tick del SO (poniendo t_anima = t, cada
         * tick duraría 3 del SO: 32,5 por segundo). Tras un atasco de más
         * de un tick, se sigue desde ahora: sin ráfaga. Sin signo: da igual
         * que t × BU_ANIMA_HZ dé la vuelta en 32 bits */
        pf->t_anima += 1000UL;
        if (t * BU_ANIMA_HZ - pf->t_anima >= 1000UL) pf->t_anima = t * BU_ANIMA_HZ;
        ui_tick();
        if (pf->tras_tick) pf->tras_tick(pf);
        dibuja = 1;
    }
    if (ui_pantalla() != pf->pant) { pf->pant = ui_pantalla(); dibuja = 1; }
    if (ui_rotulo() != pf->rotulo) { pf->rotulo = ui_rotulo(); dibuja = 1; }   /* el grabador: sus segundos */
    /* 4. un solo cuadro, y lo que pasó mientras */
    if (dibuja) {
        unsigned long antes = pf->reloj(pf);
        ui_dibuja();
        pf->vuelca(pf);
        pf->repinta = 0;
        pf->n_trabajo = 0;
        pf->t_cuadro = pf->reloj(pf);
        foto(pf, pf->t_cuadro - antes >= ENT_TICK_US / 1000UL);
    }
    /* 5. la espera: idle() hasta el tick del SO */
    if (!trabaja && !ent_pendiente(pf->reloj(pf))) pf->espera(pf);
    return 1;
}

void bucle_suelta(Plataforma *pf)
{
    int i;
    for (i = 0; i < 1000; i++) {                      /* ~10 s como mucho */
        foto(pf, 1);
        if (ent_suelto()) return;
        pf->espera(pf);
    }
}
