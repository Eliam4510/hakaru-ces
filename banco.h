/* ============================================================================
 *  banco.h  -  Banco de tiempos: lo que tarda cada cosa EN la calculadora
 *
 *  Oculto en la Ayuda (tecla T). El arnés estima los tiempos de la Nspire a
 *  partir de lo que mide en el PC, pero la coma flotante por software de un
 *  ARM926 no escala igual que la de un PC: este banco mide de verdad, en la
 *  propia calculadora, y sus números calibran el modelo del arnés.
 *
 *  Cada prueba se repite durante unos segundos contados con el reloj RTC
 *  (que solo cuenta segundos): el tiempo por vuelta sale de dividir.
 * ========================================================================= */
#ifndef BANCO_H
#define BANCO_H

#include "circuito.h"
#include "red.h"
#if ESPECIAL
#include "especial_api.h"              /* la versión especial (no se publica) */
#endif

enum {
    BN_FLOP = 0,      /* una multiplicación-resta en coma flotante          */
    BN_EJEMPLO,       /* resolver y escribir los pasos de un ejemplo        */
    BN_PEOR,          /* el peor caso de CC: 31 R, 20 nodos, 8 datos        */
    BN_PANTALLA,      /* volcar la pantalla (lcd_blit)                      */
#if ESPECIAL
    ESP_BN            /* la versión especial: sus pantallas animadas        */
#endif
    BN_SIM,           /* un paso del peor emulador: calibra SIM_NS_PASO     */
    BN_CA,            /* un punto del Bode en el peor caso: calibra CA_NS_OP */
    BN_BODE,          /* el Bode entero (autorrango) de un RLC serie        */
    BN_SENO,          /* una llamada a sin() de la libm                     */
    BN_EXP,           /* una llamada a exp(): calibra NL_OPS_EXP            */
    BN_NL,            /* un paso del puente con 1N4007: calibra NS_ITER     */
    BN_N
};

/* La segunda página ([izq/der]): lo que hace falta para recalibrar el
 * modelo de tiempos después de optimizar (rev9, punto 19: las rutinas de
 * coma flotante y el motor cambian lo que cuesta cada operación contada, y
 * la primera página mezcla cosas). Se mide tras la primera: en la
 * calculadora siempre; en el PC solo con banco_extra(1) (las pruebas de
 * siempre y el mono ven el banco de una página). Lo que calibra cada línea:
 *   B2_DDIV, B2_DCMP  lo que vale una división y una comparación en
 *                     operaciones (BN_FLOP): las cuentas las pesan 1 y 0, y
 *                     con ellas se reparte SIM_NS_OP (sim.h) y CA_NS_OP (ca.h)
 *   B2_SQRT           la raíz de hypot y cx_abs (el Bode, sus dB): CA_NS_OP
 *   B2_REGS           la k del modelo del perfil ARM (rev9/perf_arm,
 *                     analiza.py): una vuelta de subs y bne son 4 ciclos en
 *                     la tabla del ARM926; B2_ACIERTA, lo de una carga que
 *                     acierta; B2_FALLA, lo de un fallo de la caché de datos
 *                     (la P del modelo); y el tipo de caché de CP15, los
 *                     tamaños de las cachés del modelo (banco_cp15)
 *   B2_PASO           SIM_NS_PASO (sim.h) sin los reinicios que BN_SIM mete
 *                     en su medida (una ventana que no se acaba en 4 s)
 *   B2_INICIO         sim_iniciar del peor emulador: lo que hoy no ve t_est
 *                     (quitadas las operaciones que sí cuenta)
 *   B2_PEQUE          con B2_PASO, lo fijo de un paso en dos tamaños (32 y 3
 *                     elementos): SIM_NS_PASO como a + b·n
 *   B2_CA             CA_NS_OP (ca.h) en un punto con la matriz entera
 *                     (ca_ensambla: la calculadora de CA, el Thévenin) y sin
 *                     la topología. BN_CA (bode_en) ya tampoco la rehace
 *                     (rev9, grupo cabode) y ensambla solo lo que depende
 *                     de f (ca_ensambla_fija): las dos, los dos caminos */
enum {
    B2_DDIV = 0,      /* una división de double (__aeabi_ddiv)               */
    B2_DCMP,          /* una comparación de double (__aeabi_dcmplt)          */
    B2_SQRT,          /* una raíz cuadrada (sqrt de la libm)                 */
    B2_REGS,          /* una vuelta de subs y bne, sin memoria               */
    B2_ACIERTA,       /* una carga que acierta en la caché de datos          */
    B2_FALLA,         /* una carga que falla (una por línea de 32 B)         */
    B2_PASO,          /* un paso del peor emulador, sin reinicios            */
    B2_INICIO,        /* arrancar el peor emulador (circuito y sim_iniciar)  */
    B2_PEQUE,         /* un paso de una red pequeña (banco_rc)               */
    B2_CA,            /* un punto de CA del peor caso sin la topología       */
#if ESPECIAL
    ESP_B2            /* la versión especial: sus cuadros por ui_dibuja      */
#endif
    B2_N
};
void   banco_extra(int si);          /* medir también la segunda página     */
void   banco_pagina(int d);          /* [izq/der], con el banco hecho       */
int    banco_en_pagina(void);        /* la que se ve: 0 o 1                 */
double banco_ms2(int linea);         /* ms por vuelta; < 0 si aún no        */
/* Por vuelta de la línea: las operaciones (del emulador en B2_PASO, B2_INICIO
 * y B2_PEQUE, por paso en los dos pasos; de CA en B2_CA) y los pasos */
double banco_ops2(int linea);
double banco_pasos2(int linea);
/* Las veces que se acabó la ventana y se volvió a arrancar, en B2_PASO (0:
 * la suya no se acaba en 4 s) y en B2_PEQUE */
long   banco_reinicios2(int linea);
double banco_ventana2(void);         /* los pasos de B2_PASO sin flancos (t_fin/h) */
unsigned long banco_cp15(void);      /* tipo de caché de CP15; 0 fuera del ARM */
/* La red pequeña de B2_PEQUE: E cuadrada de 0 a 10 V a 10 kHz (como el peor
 * emulador), R de 1 kΩ y C de 100 nF en serie. Deja la red extraída en r. */
void   banco_rc(Circuito *c, Red *r);

void   banco_reinicia(void);
void   banco_tick(void);             /* mide la prueba siguiente (bloquea)  */
void   banco_dibuja(void);
int    banco_hecho(void);
double banco_ms(int prueba);         /* ms por vuelta; < 0 si aún no        */
void   banco_duracion(unsigned long ms);   /* por prueba; 4000 por omisión  */
/* Función que dice si [esc] está abajo: se consulta a mitad de cada prueba
 * (la pone main.c, que es quien lee el teclado). Si se pulsa, el banco se
 * cancela y banco_hecho() pasa a 1. */
void   banco_salida(int (*esc_abajo)(void));
int    banco_cancelado(void);

/* El peor caso de CC, el mismo para el banco y para el arnés: 31
 * resistencias en una red densa de 20 nodos y 8 corrientes medidas cuyas R
 * se buscan (2^8 combinaciones de signo). Deja la red cerrada en r. */
void   banco_peor_caso(Circuito *c, Red *r);

/* Operaciones de coma flotante del Gauss (sis_ops) en una resolución del
 * peor caso y, en promedio, en la de un ejemplo. */
unsigned long banco_ops_peor(void);
unsigned long banco_ops_ejemplo(void);

/* El peor caso del emulador, el mismo para el banco y para el arnés: 20
 * nodos, 32 elementos (R, L y C al azar con semilla fija) y una E cuadrada
 * de 10 kHz. Deja la red extraída en r. */
void   banco_peor_sim(Circuito *c, Red *r);
/* Operaciones por paso del emulador en el peor caso (tras BN_SIM) */
double banco_ops_paso(void);

/* El peor caso de la CA: el del emulador con la E alterna (fasor de 10 V
 * a 1 kHz), el mismo para el banco y para el arnés. Deja la red en r. */
void   banco_peor_ca(Circuito *c, Red *r);
/* El RLC serie del Bode (R 10, L 10 mH, C 1 µF; E alterna de 1 V) */
void   banco_rlc(Circuito *c, Red *r);
/* Operaciones de CA de un punto del Bode en el peor caso (tras BN_CA: la
 * media de los que midió, sin el primero, el de 1 kHz) */
unsigned long banco_ops_ca(void);

/* El puente con filtro de la Etapa 4, el mismo para el banco y para el
 * arnés: E senoidal de 10 V a 60 Hz, cuatro 1N4007 (exponenciales), C de
 * 100 µF y R de 1 kΩ a la salida. Deja la red extraída en r. */
void   banco_puente(Circuito *c, Red *r);
/* Tras BN_NL: operaciones e iteraciones de Newton por vuelta (por llamada a
 * sim_avanzar(1), como el tiempo: no todas dan paso) del puente */
double banco_ops_nl(void);
double banco_iter_nl(void);

#endif /* BANCO_H */
