/* ============================================================================
 *  entrada.h  -  La capa de entrada de Hakaru CES: de la foto del hardware a
 *                las teclas abstractas de ui.h
 *
 *  Sin hardware: se compila igual en el .tns y en el arnés. main.c le da,
 *  en cada tick del SO, una FOTO (las 8 medias palabras de la matriz en
 *  0x900E0010 y, si toca, UN touchpad_scan) y la hora en ms; la capa
 *  devuelve eventos:
 *
 *    - una tecla actúa al BAJAR (el flanco), no al soltar; si bajan varias en
 *      la misma foto, salen en la prioridad de siempre (esc, tab, menu,
 *      enter...). Una tecla que sigue abajo nunca vuelve a actuar;
 *    - shift y ctrl: si están abajo, van con la tecla (T_CON_SHIFT); si se
 *      sueltan sin que bajara otra, sale T_SHIFT (T_CTRL) y ui.c los arma;
 *    - la repetición: la lista blanca es de ui.c (ui_repite); los tiempos,
 *      de aquí (los perfiles REP_*). No se encola: se cuentan las
 *      repeticiones vencidas por el reloj;
 *    - las flechas salen del touchpad hundido: la cuña de la mitad (eje
 *      dominante: las esquinas ya no se pierden) y el centro, [enter];
 *    - deslizar el dedo (sin hundir) da pasos T_TACTIL | T_REPE según el
 *      perfil de la pantalla (ui_tactil);
 *    - una cola de ENT_COLA flancos donde [esc] nunca se pierde.
 *
 *  TODOS los números de abajo son ESTIMADOS (controles.json, «diseño»): se
 *  calibran en la calculadora real con el grabador (Controles > [G]) y el
 *  arnés los reproduce. Ver docs/DESIGN.md, «La capa de entrada».
 * ========================================================================= */
#ifndef ENTRADA_H
#define ENTRADA_H

/* --------------------------------------------------------------------------
 *  Constantes que se calibran en la calculadora
 * ----------------------------------------------------------------------- */
/* ---- Tiempo ---- */
#define ENT_TICK_US        10250  /* periodo de idle(): el tick del SO (~97,6 Hz) */
#define ENT_SP804_HZ       32768  /* 2.º contador de 0x900D0000: ¿32768 o 32000? (medir 10 s de RTC) */
#define ENT_RELOJ_SP804        1  /* 0: plan B siempre (el reloj son los ticks de idle) */
/* El plan A, vigilado (ent_reloj_*): nadie documenta si el SO usa ese contador */
#define RELOJ_CALIBRA          8  /* al abrir: 8 ticks de idle() medidos con el contador... */
#define RELOJ_TOLERANCIA      25  /* ...cada uno, ENT_TICK_US ± 25 %; si no, el plan B      */
#define RELOJ_SALTO_MS       500  /* una lectura que avanza más: un salto, cuenta un tick   */
#define RELOJ_LENTOS           2  /* idle() seguidos en que avanza menos de un tick: plan B */

/* ---- Repetición (ms). El ajuste «lenta» multiplica por REP_LENTA/10 y
 * «rápida» por REP_RAPIDA/10 los retardos y los periodos ---- */
#define REP_LISTA_RETARDO    400  /* menús, listas, tablas, pasos, la rejilla del editor */
#define REP_LISTA_PERIODO    100  /* los primeros REP_LISTA_CAMBIO ms repitiendo...     */
#define REP_LISTA_CAMBIO     600
#define REP_LISTA_PERIODO2    50  /* ...y luego este                                    */
#define REP_CURSOR_RETARDO   350  /* los cursores del Bode, del barrido y del osciloscopio; la Ayuda */
#define REP_CURSOR_PERIODO    40  /* 25 Hz                                              */
#define REP_MULT_1S         1000  /* multiplicador x1 hasta aquí, x2 hasta 2 s,         */
                                  /* x4 hasta 3 s y x8 después (con el tope de ui_repite) */
#define REP_ESCALA_RETARDO   500  /* V/div, t/div, dB/div, zoom, la ventana del osc.    */
#define REP_ESCALA_PERIODO   180
#define REP_BORRAR_RETARDO   450  /* [del] en un texto que se escribe                  */
#define REP_BORRAR_PERIODO    70
#define REP_ESP_RETARDO      150  /* el perfil de la versión especial                   */
#define REP_ESP_PERIODO       50
#define REP_LENTA             15  /* x1,5                                               */
#define REP_RAPIDA             6  /* x0,6                                               */
#define REP_TOPE_CUADRO       32  /* pasos de repetición por cuadro, como mucho        */

/* ---- La cola y los fantasmas ---- */
#define ENT_COLA              16  /* flancos en espera; [esc] nunca se pierde           */
#define ENT_FANTASMA           3  /* tantos flancos nuevos a la vez (sin shift ni ctrl),
                                     o uno que cierra un rectángulo con otras tres abajo:
                                     un fantasma de la matriz, se ignoran (salvo [esc]) */

/* ---- Touchpad: las flechas (milésimas del medio pad; la y crece hacia arriba) ---- */
#define TP_W_OMISION        2328  /* si touchpad_getinfo no responde: el Synaptics      */
#define TP_H_OMISION        1691
#define TP_DIM_MIN           100  /* un W o un H fuera de 100..16384 no es creíble: los */
#define TP_DIM_MAX         16384  /* de omisión (el CapTIvate no publica el suyo: puede
                                     ser pequeño); con las x acotadas al pad, ninguna
                                     cuenta pasa de 2^31 en el long del ARM             */
#define TP_CENTRO            300  /* clic central si u² + v² < 0,30²                    */
#define TP_CUNA_MARGEN       100  /* cambiar de cuña con el pad hundido: 0,10 de margen... */
#define TP_CUNA_MUESTRAS       2  /* ...durante 2 muestras                              */
#define TP_FALLOS              3  /* scans fallidos que se aguantan con la muestra anterior */

/* ---- Touchpad: deslizar (milésimas de W o de H; muestras = lecturas del pad) ---- */
#define TP_EMPIEZA             2  /* muestras con contacto para empezar un gesto        */
#define TP_ACABA               3  /* muestras sin contacto para acabarlo                */
#define TP_ARRANQUE           80  /* nada hasta alejarse un 8 % del ancla               */
#define TP_EJE_DOBLE           2  /* un eje se bloquea si recorrió el doble que el otro */
#define TP_HIST_PASO         250  /* para volver atrás: el paso más un 25 % de él...    */
#define TP_HIST_MIN           15  /* ...o el 1,5 % (de W o de H: el del eje), lo mayor */
#define TP_VEL_MUESTRAS        3  /* la velocidad, en las 3 últimas muestras (~30 ms)   */
#define TP_VEL_1              10  /* por muestra (10 ms): x1 por debajo del 1 %,        */
#define TP_VEL_2              25  /* x2 hasta el 2,5 % y x4 por encima (con el tope)    */
#define TP_TOPE_MUESTRA        8  /* pasos por muestra, como mucho                      */
#define TP_SALTO             400  /* un salto de más del 40 % en una muestra: se re-ancla */
#define TP_BLOQUEO_CLIC      150  /* ms sin deslizar tras soltar el clic                */
#define TP_PAGINA            250  /* «pasar página»: recorrer un 25 %...                */
#define TP_PAGINA_MS         350  /* ...en 350 ms como mucho                            */
#define TP_QUIETO             20  /* moverse menos de un 2 %: el dedo está quieto       */
#define TP_DERIVA_MS         400  /* sin arrancar, quieto (TP_QUIETO) tanto: el ancla pasa
                                     ahí (un pulgar que resbala despacio no suma el 8 %) */
#define TP_BUSCA_MS          200  /* los primeros pasos sin balística esperan esto desde
                                     que el dedo echa a andar: si se hunde el pad antes,
                                     el dedo buscaba la flecha y esos pasos no cuentan  */

/* ---- Energía: cada cuántos ticks se lee el pad (la matriz, en todos) ---- */
#define EN_ACTIVO_MS        1000  /* menos de 1 s desde lo último: cada tick            */
#define EN_REPOSO_MS       30000  /* hasta 30 s: cada EN_REPOSO_CADA; luego, cada EN_PROFUNDO_CADA */
#define EN_REPOSO_CADA         2
#define EN_PROFUNDO_CADA       4
#define EN_QUIETO_MS        5000  /* dedo apoyado y quieto más de 5 s: cada EN_REPOSO_CADA */

/* ---- El bucle (bucle.c) ---- */
#define BU_EVENTOS            32  /* eventos por cuadro, como mucho...                  */
#define BU_MS                 40  /* ...o estos ms                                       */
#define BU_ANIMA_HZ           30  /* las pantallas animadas: ticks por segundo de
                                     media, a plazo fijo (3 o 4 del SO) */
#define BU_BARRA_MS          200  /* emulando: la barra, como mucho cada 200 ms         */
#define BU_BARRA_TICKS         6  /* ...o cada 6 ticks de trabajo (~200 ms; el plan B)  */
#define BU_OPS_TICK       50000UL /* operaciones por tick de trabajo (~23 ms de ellas sin el USB, ~32
                                     con él; el arnés usa SIM_OPS_TICK) */

/* ---- El grabador (Controles > [G]) ---- */
#define GRAB_MS            10000  /* 10 s de muestras crudas...                        */
#define GRAB_MAX            2000  /* ...las fotos que cuentan (el pad a ~97,6 Hz, más
                                     los flancos y las que sacan eventos: ~125/s con una
                                     ráfaga), con margen: 54 KB                          */

/* ---- Los ajustes del panel «Controles» y sus valores de omisión ---- */
enum { REPETIR_NO = 0, REPETIR_LENTA, REPETIR_NORMAL, REPETIR_RAPIDA, REPETIR_N };
#define DESLIZAR_OMISION       1
#define REPETIR_OMISION        REPETIR_NORMAL
#define CLIC_ENTER_OMISION     1

/* --------------------------------------------------------------------------
 *  Las teclas físicas, en el orden de prioridad (el de siempre: esc, tab,
 *  menu, enter, las flechas...). Las flechas y el clic central no son de la
 *  matriz: salen del touchpad.
 * ----------------------------------------------------------------------- */
enum {
    TF_ESC = 0, TF_TAB, TF_MENU, TF_ENTER, TF_RET,
    TF_ARRIBA, TF_ABAJO, TF_IZQ, TF_DER, TF_CLIC,          /* del touchpad */
    TF_DEL, TF_PUNTO, TF_SIGNO, TF_MAS, TF_MENOS, TF_IGUAL, TF_INCOG, TF_EE,
    TF_0,                                                  /* TF_0 + n       */
    TF_A = TF_0 + 10,                                      /* TF_A + (c-'A') */
    TF_CAT = TF_A + 26,
    TF_ESPACIO, TF_POR, TF_ENTRE, TF_POT, TF_CUAD, TF_PAR_A, TF_PAR_C,   /* las cuentas y los nombres */
    TF_SHIFT, TF_CTRL,
    TF_N
};
#define TF_DIG(n)   (TF_0 + (n))
#define TF_LETRA(c) (TF_A + ((c) - 'A'))

/* Dónde está cada tecla de la matriz: la media palabra (0 a 7, de
 * 0x900E0010) y su bit; fila < 0 si no es de la matriz. Son los tpad_row y
 * tpad_col de keys.h (Ndless SDK), con fila = (tpad_row - 0x10) / 2. */
int ent_tecla_fis(int tf, int *fila, unsigned *bit);

/* --------------------------------------------------------------------------
 *  La foto
 * ----------------------------------------------------------------------- */
enum { ENT_PAD_NO = 0, ENT_PAD_OK, ENT_PAD_ERROR };     /* no se leyó, bien, falló */
typedef struct {
    unsigned short fila[8];          /* 0x900E0010..0x900E001E: 1 = abajo */
    unsigned char  pad;              /* ENT_PAD_*                          */
    unsigned char  contacto, hundido;
    unsigned short x, y;             /* del pad: la y crece hacia arriba   */
} EntMuestra;

/* El tamaño del pad (touchpad_getinfo; 0, o fuera de TP_DIM_MIN..TP_DIM_MAX,
 * = el de omisión) y la foto del arranque: lo que está abajo al empezar (el
 * [enter] que lanzó el .tns) no cuenta. Vacía la cola, la ráfaga y el gesto;
 * no toca los ajustes. */
void ent_init(int w, int h, const EntMuestra *m, unsigned long t);
/* Una foto a la hora t (ms): la matriz siempre; el pad si m->pad != ENT_PAD_NO */
void ent_muestra(const EntMuestra *m, unsigned long t);
/* El siguiente evento (T_* con T_CON_*, T_REPE, T_TACTIL, o T_SUELTA), o
 * T_NADA. Consulta ui_repite antes de cada repetición y cada paso táctil. */
int  ent_saca(unsigned long t);
/* ¿Hay algo que sacar ya? (sin consultar a ui.c) */
int  ent_pendiente(unsigned long t);
/* Al empezar un cuadro: las repeticiones vencidas se recortan a
 * REP_TOPE_CUADRO (sin inercia si el cuadro fue caro) */
void ent_cuadro(unsigned long t);
/* Cada cuántos ticks hay que leer el pad (1, EN_REPOSO_CADA o EN_PROFUNDO_CADA) */
int  ent_cada(unsigned long t);
/* ¿No queda nada abajo (teclas de la matriz ni el pad hundido)? */
int  ent_suelto(void);

/* --------------------------------------------------------------------------
 *  Los ajustes del panel «Controles»
 * ----------------------------------------------------------------------- */
typedef struct {
    unsigned char deslizar;          /* 1 = sí                                  */
    unsigned char repetir;           /* REPETIR_*                               */
    unsigned char clic_enter;        /* 1 = el clic central es [enter]; 0 = nada */
} EntAjustes;
void ent_ajustes(EntAjustes *a);
void ent_pon_ajustes(const EntAjustes *a);   /* valores fuera de rango: los de omisión */
void ent_ajustes_omision(void);

/* El touchpad que hay, para calibrar la capa (Controles lo enseña): main.c
 * lo mira al arrancar (el tipo y el W×H de touchpad_getinfo, 0 si no
 * respondió). ent_pad_tipo: el tipo, o -1 si nadie lo dijo (el arnés), y
 * ese W×H; ent_pad_usa: el W×H con que trabaja la capa (el de omisión si
 * aquel no es creíble) */
enum { ENT_TP_NINGUNO = 0, ENT_TP_SYNAPTICS, ENT_TP_CAPTIVATE };
void ent_pad_visto(int tipo, int w, int h);
int  ent_pad_tipo(int *w, int *h);
void ent_pad_usa(int *w, int *h);

/* --------------------------------------------------------------------------
 *  El reloj del plan A, vigilado. main.c lee el 2.º contador del SP804 (baja
 *  a ENT_SP804_HZ, 32 bits) y le pasa aquí las lecturas: la cuenta no sabe
 *  nada del hardware y el arnés la prueba con un contador simulado.
 *    - al abrir, las RELOJ_CALIBRA + 1 lecturas (una tras cada idle(), ya en
 *      el borde de un tick) tienen que dar ENT_TICK_US ± RELOJ_TOLERANCIA %
 *      por tick; si no (otra frecuencia, parado), el plan B;
 *    - una lectura que avanza más de RELOJ_SALTO_MS (el contador reprogramado
 *      o dado la vuelta) cuenta como un tick: no hay ráfagas de golpe;
 *    - tras cada idle() tiene que haber avanzado un tick como poco (menos la
 *      tolerancia); RELOJ_LENTOS veces seguidas que no (parado, más lento):
 *      el plan B hasta salir.
 *  En el plan B, cada idle() es ENT_TICK_US. El tiempo nunca va hacia atrás
 *  ni salta al cambiar de plan.
 * ----------------------------------------------------------------------- */
typedef struct {
    int plan_a;                      /* 1: el contador; 0: los ticks de idle()   */
    unsigned ant, ant_tick;          /* la última lectura; la del último idle()  */
    int lentos;                      /* idle() seguidos que avanzó de menos      */
    unsigned long long us, resto;    /* µs hasta ahora; cuentas × 10^6 sobrantes */
    long saltos, cambios;            /* el arnés y Controles                     */
    /* Lo que se calibra, a la vista (Controles): contra los saltos de un
     * segundo del RTC (ent_reloj_rtc), desde el primero que se ve */
    unsigned long rtc_ult, rtc0;     /* el último segundo del RTC; el del primer salto */
    int rtc_n;                       /* saltos vistos (0: rtc0 sin fijar)        */
    unsigned long seg;               /* segundos desde rtc0...                   */
    long idle_n, idle0, idle_seg;    /* ...e idle(): todos; en rtc0; en seg      */
    unsigned long rtc_a, seg_a;      /* el contador (plan A): el salto de partida, */
    unsigned long cuentas;           /* los segundos desde él y lo que bajó...   */
    unsigned v_a;                    /* ...su lectura y los saltos del contador  */
    long saltos_a;                   /* entonces (-1: sin partida; si salta, otra) */
    unsigned long long tick_suma;    /* cuentas de los idle() de un tick (±25 %)... */
    long tick_n;                     /* ...y cuántos                             */
} EntReloj;
/* v[0..n]: n + 1 lecturas, una tras cada idle(); n = 0: el plan B. 1 si plan A */
int  ent_reloj_abre(EntReloj *r, const unsigned *v, int n);
/* La hora en ms con una lectura del contador (en el plan B, v no cuenta) */
unsigned long ent_reloj_ms(EntReloj *r, unsigned v);
/* Tras cada idle(), con una lectura nueva */
void ent_reloj_tick(EntReloj *r, unsigned v);
/* Tras cada idle() (después de ent_reloj_tick), los segundos del RTC y la
 * misma lectura: cuenta los idle() y, en el plan A, lo que baja el contador
 * entre saltos del RTC (con el error de un tick en la cuenta entera, no en
 * cada segundo) */
void ent_reloj_rtc(EntReloj *r, unsigned long rtc_s, unsigned v);
/* Lo medido, en enteros: las cuentas por segundo del RTC (0 si aún no hay
 * un segundo del plan A), el tick de idle() en µs (con esas cuentas o, si
 * no, ENT_SP804_HZ; 0 sin ningún tick medido) y los idle() por segundo × 10
 * (0 sin un segundo: también cuentan los segundos con trabajo) */
void ent_reloj_medido(const EntReloj *r, unsigned long *hz, unsigned long *tick_us, unsigned long *idle10);
/* El reloj de la plataforma, para enseñarlo en Controles: lo pone main.c;
 * el arnés, solo en su prueba (sin él, el panel no lo enseña) */
void ent_reloj_a_la_vista(const EntReloj *r);
const EntReloj *ent_reloj_visto(void);

/* --------------------------------------------------------------------------
 *  El grabador: GRAB_MS de muestras crudas (hora, las 8 filas, el pad) en
 *  ces_muestras.tns; el arnés las reproduce por la capa. No se graban las
 *  fotos que no cambian nada (sin pad, la matriz de la anterior y nada que
 *  sacar): reproducir sin ellas da los mismos eventos
 * ----------------------------------------------------------------------- */
enum { GRAB_NADA = 0, GRAB_GRABANDO, GRAB_HECHO, GRAB_ERROR };
void ent_grabar(void);                       /* empieza en la foto siguiente    */
int  ent_grabacion(int *n, int *ms_quedan);  /* GRAB_*; n = muestras            */
void ent_grabacion_olvida(void);             /* vuelve a GRAB_NADA (ui_init)    */
/* El formato «CESM»: marca, versión, W, H, n y cada muestra (27 bytes).
 * Serializa lo grabado en buf (devuelve los bytes, o 0 si no cabe) y lee
 * uno: 1 si vale, con w, h, n y las muestras (y sus horas) en m y t. */
int  ent_grabacion_bytes(unsigned char *buf, int cap);
int  ent_grabacion_lee(const unsigned char *buf, int nbytes, int *w, int *h,
                       EntMuestra *m, unsigned long *t, int max, int *n);
#define GRAB_CABECERA 12
#define GRAB_BYTES    27

/* --------------------------------------------------------------------------
 *  Cuentas para el arnés
 * ----------------------------------------------------------------------- */
typedef struct {
    long flancos, perdidos;          /* a la cola; descartados por llena     */
    long repeticiones, pasos_tactiles, sueltas;
    long gestos, anulados;           /* contactos que arrancaron; anulados   */
    long fantasmas;                  /* flancos descartados por fantasmas    */
    long buscados;                   /* pasos retenidos que tiró un clic     */
    long derivas;                    /* anclas que se mudaron por deriva     */
    int  rafaga;                     /* hay una ráfaga de tecla viva          */
    int  contacto;                   /* hay un dedo en el pad (gesto)         */
} EntCuentas;
void ent_cuentas(EntCuentas *c);

#endif /* ENTRADA_H */
