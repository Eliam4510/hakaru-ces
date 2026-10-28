/* ============================================================================
 *  turbo.h  -  El turbo (experimental): el reloj de la CX II de 396 a 408-456
 *              MHz, a mano, desde el panel Controles
 *
 *  En la CX II el reloj del ARM es el multiplicador M del PLL: los bits 24-31
 *  del registro 0x90140030 de la PMU «Aladdin». Con D1 = 2 (bits 16-20) y
 *  D2 = 0 (bits 20-23 de 0x90140020), CPU = 12 MHz·M: 396 es M = 33, y el SO
 *  lo baja a 288 (M = 24) con el USB. El set_cpu_speed de Ndless no hace nada
 *  en la CX II (set_cpu_speed.c: «if (is_cx2) return 0»). Fuentes (vistas en
 *  overclock_investigacion.json, con su verificación):
 *    - el driver de Linux de la CX II (F. Vogt, clk-nspire-cx2.c): la fórmula
 *      24 MHz·M/D1/(1+D2), 48 MHz si el bit 4 de 0x90140810 está a 0 y 24 MHz
 *      si lo está el bit 4 de 0x90140030;
 *    - zephray (2020): los dos valores medidos, 0x21020303 (396) y 0x18020303
 *      (288 con el USB); Firebird (cx2.cpp): 0x21020303 al arrancar, y
 *      escribir 0x90140030 levanta INT_POWER, pendiente en el bit 0 de
 *      0x90140024, que se limpia al atenderlo;
 *    - NoverII (critor, 2020, nover.c): escribe 0x90140030 con
 *      TCT_Local_Control_Interrupts(0), que ABRE las IRQ (0 = abrir en
 *      Nucleus; -1 las cierra): el SO tiene que atender INT_POWER;
 *    - el reproductor de vídeo de GigaZelensky (performance_clock.c), en dos
 *      versiones: la del 2026-09-27 (1a1fe47) escribía 0x90140030 con las
 *      IRQ cerradas, las abría para esperar a que se limpiara el pendiente
 *      de 0x90140024 y, si no se limpiaba, lo daba por fallo; la del
 *      2026-09-29 (0a60d89, «Fix CX II overclocking») ya NO escribe
 *      0x90140030: llama a la copia en SRAM de la transición del SO (tras
 *      comprobar su FNV), que pone la SDRAM en autorrefresco, confirma el PLL
 *      por 0x90140020, cambia los tiempos de la EMI y reconoce la PMU; con
 *      las IRQ cerradas todo el rato y sin empezar con el pendiente a 1.
 *  Este diseño (código propio) cambia SOLO los bits 24-31 de 0x90140030,
 *  como NoverII (2020, con otro SO): nunca escribe 0x90140020 ni
 *  0x90140810, ni toca D1 ni D2. NINGUNA fuente de hoy lo respalda: el único
 *  programa que lo hacía solo lo acaba de sustituir (y dice que ahora el
 *  reloj se aplica al momento y con los tiempos de la RAM). Así que el turbo
 *  NO va en la versión: main.c solo le da el hardware con HAKARU_TURBO (make
 *  TURBO=1), y sin él turbo_hay() = 0: ni la fila ni ningún registro. Hasta
 *  probarlo en la calculadora; [P] mide la CPU, no solo el registro.
 *
 *  Reglas (la decisión del usuario):
 *    - apagado al arrancar, siempre; solo se actúa si la calculadora está a
 *      396 sin USB: 0x90140030 = 0x21020303, D2 = 0 y el bit 1 de 0x90140020
 *      a 0, el bit 4 de 0x90140810 a 1 y es una CX II; si no, nada se toca;
 *    - tope 432 (M = 36); 444 y 456 (M = 37, 38) solo tras el aviso que el
 *      usuario confirma (turbo_alto); nunca más de 456;
 *    - de 12 en 12 MHz, subiendo y bajando; en cada paso, el pendiente con
 *      tope en cuentas de 32 kHz (el SP804 de 0x900D0000, que no va con el
 *      APB) y la lectura de vuelta; si algo no cuadra, a 396, apagado y dicho.
 *      Nunca se escribe con el pendiente a 1 (una petición sin atender).
 *      Bajando, el tope es mayor (0,5 s: un SO que tarda), pero lo que la PMU
 *      no confirma queda «sin confirmar» (la CPU, a lo de antes o a lo
 *      escrito) y cuenta como atascado; lo atascado se reintenta en cada
 *      acción, al escribir y al salir (con las IRQ abiertas otro rato);
 *    - del SO es solo lo que el SO pone (M ≤ 33: 288 con el USB o 396): eso
 *      no se toca. Por encima de 396, lo que Hakaru sabe bajar (sus bits bajos)
 *      se baja; lo demás queda «atascado», sin escribir nada. Y, pase lo que
 *      pase, la puerta no escribe con el reloj por encima de 396;
 *    - a 396 ANTES de escribir cualquier archivo (la puerta de archivo.c,
 *      turbo_pausa y turbo_sigue) y al salir por cualquier camino (main.c y
 *      atexit: turbo_sale); solo si el registro sigue con lo que puso Hakaru
 *      (si el SO lo cambió, el USB, se deja el suyo);
 *    - la marca de cuelgue (como NoverII): antes de subir, a 396, se escribe
 *      «prueba en curso a M» en ces_controles.tns (turbo_marcador); se borra
 *      al salir bien y en cuanto el turbo queda apagado por otro camino (el
 *      USB, una puerta que falla: en turbo_vigila, fuera de la puerta). Si al
 *      arrancar sigue ahí, ese M colgó (turbo_recupera);
 *    - las IRQ se abren sin teclas abajo (con ellas abiertas el SO lee el
 *      teclado: ploaderhook.c espera lo mismo al volver), con tope.
 *
 *  El hardware (MMIO, las IRQ del SO, los relojes) es de main.c, que lo pone
 *  con turbo_hw; en el PC hay un simulador de la PMU (turbo_sim_*) para el
 *  arnés. Aquí solo hay la lógica, que se prueba entera en el PC.
 * ========================================================================= */
#ifndef TURBO_H
#define TURBO_H

#define TURBO_M_USB    24         /* 288 MHz: el SO, con el USB                */
#define TURBO_M_SERIE  33         /* 396 MHz: la CX II sin USB                 */
#define TURBO_M_MIN    34         /* 408 MHz: el primer nivel                  */
#define TURBO_M_TOPE   36         /* 432 MHz: el tope normal                   */
#define TURBO_M_MAX    38         /* 456 MHz: nunca más (37-38, tras el aviso) */
#define TURBO_MHZ(m)   (12 * (m)) /* con D1 = 2 y D2 = 0                       */

#define TURBO_R_DIV    0x90140020u   /* D2 en los bits 20-23; el bit 1 duerme (nunca se escribe) */
#define TURBO_R_PEND   0x90140024u   /* bit 0: INT_POWER pendiente                */
#define TURBO_R_PLL    0x90140030u   /* M en los bits 24-31, D1 en los 16-20      */
#define TURBO_R_FUENTE 0x90140810u   /* bit 4 a 1: el PLL (bit 8: la tecla ON)    */
#define TURBO_SERIE    0x21020303u   /* 396 MHz: el único patrón en que se actúa  */

/* Lo que no dejó actuar o lo que salió mal (turbo_motivo) */
enum {
    TBM_OK = 0,
    TBM_NO_CX2,       /* no es una CX II                                     */
    TBM_USB,          /* M = 24: el SO la bajó a 288 por el USB              */
    TBM_PATRON,       /* 0x90140030 no es 0x21020303 (turbo_patron: cuál)    */
    TBM_DIV,          /* D2 != 0 o el bit 1 en 0x90140020                    */
    TBM_FUENTE,       /* el bit 4 de 0x90140810 a 0                          */
    TBM_NIVEL,        /* fuera de 34..38, o 37-38 sin el aviso               */
    TBM_MARCA,        /* no se pudo escribir la marca de cuelgue             */
    TBM_PENDIENTE,    /* el pendiente no se limpió a tiempo                  */
    TBM_LECTURA,      /* lo que se lee no es lo que se escribió              */
    TBM_SO,           /* el SO cambió el reloj (el USB): se deja el suyo     */
    TBM_PRUEBA,       /* la prueba de estabilidad dio distinto               */
    TBM_RELOJ32,      /* el contador de 32 kHz cambió con la CPU             */
    TBM_CANCELADA,    /* [esc] en la prueba                                  */
    TBM_MEMORIA,      /* sin memoria para el patrón de la prueba             */
    TBM_SIN_RELOJ,    /* la prueba no pudo medir el reloj a 396 (el RTC)     */
    TBM_RARO,         /* por encima de 396 con otros bits, o sin calcular: no se toca */
    TBM_AJENO,        /* por encima de 396 sin el turbo: se bajó para escribir */
    TBM_CPU,          /* [P]: el registro cambió y la CPU no (no va a M)      */
    TBM_N
};

/* El hardware, que pone main.c (sin él, «no es una CX II» y nada se toca) */
typedef struct {
    int      (*cx2)(void);              /* is_cx2 (nl_hwsubtype() == 2)                 */
    unsigned (*lee)(unsigned dir);      /* 0x90140020, 24, 30 u 810                     */
    void     (*pon_pll)(unsigned v);    /* la ÚNICA escritura: 0x90140030               */
    int      (*irq)(int m);             /* TCT_Local_Control_Interrupts: -1 las cierra,
                                           0 las abre; devuelve cómo estaban            */
    unsigned (*t32)(void);              /* cuentas de 32 kHz (llamada a menudo: < 10 ms) */
    unsigned (*rtc)(void);              /* el RTC, en segundos                          */
    int      (*tecla)(void);            /* alguna tecla abajo (la matriz; 0 si no hay)  */
} TurboHw;
void turbo_hw(const TurboHw *hw);
/* [esc] abajo, para lo que bloquea en la prueba (medir el reloj: ~2 s) */
void turbo_salida(int (*esc_abajo)(void));

/* ---- Lo que se ve ---- */
int      turbo_hay(void);        /* ¿es una CX II? (si no, el panel no enseña el turbo) */
int      turbo_mhz(void);        /* la frecuencia real, del registro (Linux); 0 si no es una CX II */
int      turbo_mhz_marca(void);  /* la de la marca de la barra: la del nivel, la real si atascado; 0 */
int      turbo_nivel(void);      /* el M que puso Hakaru y sigue puesto (34..38), o 0 */
int      turbo_pedido(void);     /* el que se quiere (0: apagado), aunque esté en pausa */
int      turbo_motivo(void);     /* lo último que salió mal (TBM_OK si nada)          */
unsigned turbo_patron(void);     /* 0x90140030 al mirar las precondiciones            */
int      turbo_atascado(void);   /* no pudo volver a 396, o sin confirmar: no se escribe nada */
int      turbo_puede(void);      /* las precondiciones, solo leyendo: TBM_OK o el motivo */

/* ---- Lo que se hace ---- */
/* 0: apagar (a 396); 34..38: a ese nivel, siempre pasando por 396 y con la
 * marca escrita antes de subir. TBM_OK o el motivo (y queda a 396, apagado) */
int  turbo_pon(int m);
void turbo_alto(int si);         /* 444 y 456, tras el aviso confirmado        */
int  turbo_permite(int m);       /* ¿se puede pedir m? (34..36, o 37-38 con el aviso) */
/* La puerta de archivo.c: turbo_pausa a 396 antes de escribir (0 si no se
 * pudo: entonces no se escribe) y turbo_sigue de vuelta al nivel después (si
 * se sigue queriendo y se cumplen las precondiciones). Se anidan */
int  turbo_pausa(void);
void turbo_sigue(void);
/* Cada vuelta del bucle (ui_rotulo): si el SO cambió el reloj, se apaga sin
 * tocarlo (si es otra cosa, a 396 o atascado). 1 si se apagó ahora. Y la
 * marca de cuelgue, fuera si el turbo quedó apagado por otro camino */
int  turbo_vigila(void);
/* Al salir, por cualquier camino (main.c y atexit): a 396 y la marca fuera */
void turbo_sale(void);

/* ---- La marca de cuelgue y los niveles que fallaron, en ces_controles.tns
 *      (los escribe ui.c: turbo_marcador). Bit m - TURBO_M_MIN ---- */
void     turbo_marcador(int (*escribe)(void));   /* 1 si se pudo escribir         */
int      turbo_marca(void);                      /* el M marcado, o 0             */
unsigned turbo_fallidos(void);
/* Al arrancar, lo leído: una marca que sigue ahí es un cuelgue a ese M, que
 * pasa a fallido (y la marca, a 0). Devuelve ese M, o 0 */
int      turbo_recupera(int marca, unsigned fallidos);

/* ---- La prueba de estabilidad (Probar en el panel): a 396, una pasada de
 *      referencia (TB_REF_S segundos de carga del motor más el patrón de
 *      memoria) y su FNV; sube a M, mira que la CPU vaya a M (una carga
 *      fija, 33/M de lo de 396) y repite pasadas TB_TURBO_S segundos,
 *      comparando cada FNV bit a bit. Cualquier diferencia: a 396, apagado ---- */
enum { TBP_NADA = 0, TBP_REFERENCIA, TBP_TURBO, TBP_BIEN, TBP_MAL };
typedef struct {
    int fase;                    /* TBP_*                                          */
    int m;                       /* el nivel que se prueba                         */
    int vueltas;                 /* las de una pasada (las de la referencia)       */
    int pasadas;                 /* completas a M, iguales a la referencia         */
    int seg, seg_total;          /* segundos a M y los que tocan                   */
    unsigned long kb;            /* KB del patrón de memoria                       */
    int motivo;                  /* al acabar: TBM_OK o por qué no                 */
    int esc;                     /* cancelada por el [esc] que vio la medida del
                                    reloj (turbo_salida), aún abajo al acabar: ese
                                    [esc] no es para salir del panel               */
} TurboPrueba;
int  turbo_prueba(int m);        /* empieza: TBM_OK o el motivo                    */
int  turbo_prueba_paso(void);    /* un trozo (~10-30 ms); 0 cuando acabó (o nada)  */
void turbo_prueba_cancela(void); /* [esc]: a 396, apagado                          */
void turbo_prueba_estado(TurboPrueba *p);

#ifndef NDLESS
/* ---- El simulador de la PMU del arnés (el hw por omisión en el PC) ---- */
typedef struct { unsigned dir, v; int irq_abiertas, pend; } TurboEsc;   /* pend: el pendiente al escribir */
typedef struct { int m, nesc; char nombre[24]; int cpu, pend; } TurboArch;   /* el M y las escrituras de
                                                   antes; el M de la CPU de verdad y el pendiente */
typedef struct {
    int      cx2;
    unsigned pll, div, fuente, pend;
    int      irq_abiertas;  /* como en Ndless, cerradas al empezar             */
    long     atiende;       /* lecturas del pendiente con las IRQ abiertas hasta
                               que el «SO» atiende INT_POWER; < 0: nunca       */
    long     atiende_1;     /* ...en la primera escritura (0: como las demás)   */
    unsigned t32_paso;      /* cuentas de 32 kHz por lectura del contador       */
    unsigned rtc_paso;      /* y por lectura del RTC                            */
    int      t32_cpu;       /* el contador va con la CPU (lo que no debe pasar) */
    int      mal_m;         /* escribir este M deja M - 1 (la lectura no cuadra)*/
    int      usb_en;        /* al atender la escritura n.º tal, el SO pone 288  */
    long     usb_tras;      /* o tras tantas lecturas del pendiente con las IRQ
                               abiertas (en cualquier espera; 0: nunca)        */
    int      falla_pasada;  /* la prueba: en esa pasada a M, un bit de un
                               resultado intermedio de la carga cambia...      */
    int      falla_donde;   /* ...en Newton (0: la 1.ª iteración) o en un exp (1) */
    int      falla_bit;     /* ...ese bit del double                            */
    int      falla_mem;     /* la prueba: un bit del patrón cambia a M          */
    int      sin_mem;       /* la prueba: malloc no da nada                     */
    int      pega_m;        /* escribir este M no cambia el registro             */
    int      sube_m;        /* escribir este M deja M + 1                        */
    long     tecla;         /* lecturas del teclado con una tecla abajo; < 0: siempre */
    int      pll_quieta;    /* la PMU guarda lo escrito y la CPU no cambia (el PLL
                               lo confirmaría otra cosa: 0x90140020)            */
    int      no_toma_pend;  /* lo escrito con el pendiente a 1 no llega a la CPU */
    /* lo que pasó */
    int      nesc;          /* escrituras                                       */
    TurboEsc esc[64];
    int      narch;         /* archivos escritos o borrados (y el M entonces)   */
    TurboArch arch[32];
    unsigned t;             /* la hora del simulador, en cuentas de 32 kHz      */
    long     esperas;       /* lecturas del pendiente                           */
    int      abre_con_tecla;/* las IRQ abiertas con una tecla abajo             */
    int      cpu_m;         /* el M de la CPU de verdad; 0: el del registro     */
} TurboSim;
/* Al empezar, «no es una CX II»: las pruebas de antes del turbo ven el panel
 * y el banco de siempre (y el mono no da con el turbo). turbo_sim_reinicia
 * pone una CX II a 396 sin USB y el turbo como al arrancar */
TurboSim *turbo_sim(void);           /* para mirarlo y tocarlo                    */
void turbo_sim_reinicia(void);
void turbo_sim_archivo(const char *ruta);   /* la puerta: el reloj en cada escritura */
#define TURBO_TESTIGO(r) turbo_sim_archivo(r)
#else
#define TURBO_TESTIGO(r) ((void)0)
#endif

#endif /* TURBO_H */
