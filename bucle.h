/* ============================================================================
 *  bucle.h  -  El bucle de Hakaru CES, con la plataforma inyectable
 *
 *  Lo que antes vivía en main.c (esperar, leer, procesar, repintar), sin
 *  hardware: la plataforma le da la foto, la hora, la espera (idle) y el
 *  volcado. En la calculadora es main.c; en el arnés, una mano y un reloj
 *  virtuales (test_entrada.inc). Una vuelta (bucle_paso):
 *
 *    1. la foto: la matriz siempre y el pad cada ent_cada() ticks;
 *    2. los eventos: hasta BU_EVENTOS o BU_MS, con una foto de la matriz
 *       entre uno y otro ([esc] se ve aunque el cuadro sea caro);
 *    3. la animación (BU_ANIMA_HZ ticks por segundo de media, a plazo fijo)
 *       o un tick de trabajo (emulando: la barra como mucho cada
 *       BU_BARRA_MS);
 *    4. UN cuadro si algo cambió (una tecla, un tick, la pantalla o
 *       ui_rotulo: los segundos del grabador) y otra foto;
 *    5. si no queda nada que hacer, la espera: idle() hasta el tick del SO.
 *
 *  Sin eventos no hay cuadros: una pantalla quieta no gasta.
 * ========================================================================= */
#ifndef BUCLE_H
#define BUCLE_H

#include "entrada.h"

typedef struct Plataforma Plataforma;
struct Plataforma {
    /* la foto: la matriz siempre; el pad solo si con_pad (si no, ENT_PAD_NO) */
    void (*foto)(Plataforma *pf, EntMuestra *m, int con_pad);
    unsigned long (*reloj)(Plataforma *pf);          /* ms                         */
    void (*espera)(Plataforma *pf);                  /* hasta el tick siguiente    */
    void (*vuelca)(Plataforma *pf);                  /* el backbuffer a la pantalla */
    /* Opcionales (el arnés): lo que costó un evento y un tick de trabajo */
    void (*tras_tecla)(Plataforma *pf, int t);
    void (*tras_tick)(Plataforma *pf);
    int  sin_animar;                                 /* el arnés: los ticks los da la prueba */
    int  w, h;                                       /* el pad (0: el de omisión)  */
    /* del bucle */
    unsigned long t_pad, t_cuadro;                  /* el último scan del pad y el último cuadro */
    unsigned long t_anima;                          /* el tick de animación que toca (ms × BU_ANIMA_HZ) */
    int repinta, pant, rotulo;                       /* rotulo: el último ui_rotulo() */
    int n_trabajo;                                   /* ticks de trabajo desde el último cuadro */
    void *datos;
};

/* La foto del arranque (lo que está abajo no cuenta) y el primer cuadro */
void bucle_inicia(Plataforma *pf);
/* Una vuelta del bucle: 0 cuando hay que salir del programa */
int  bucle_paso(Plataforma *pf);
/* Al salir: espera a que se suelte todo, para que el SO no reciba el [esc] */
void bucle_suelta(Plataforma *pf);

#endif /* BUCLE_H */
