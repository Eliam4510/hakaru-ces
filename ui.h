/* ============================================================================
 *  ui.h  -  Interfaz de Hakaru CES
 *
 *  Separada del teclado a proposito, como en EstaticaCX: ui_tecla() recibe un
 *  codigo abstracto, no una tecla de la Nspire. Asi la app entera se conduce
 *  desde el PC con un guion de teclas y cada pantalla se vuelca a BMP.
 * ========================================================================= */
#ifndef UI_H
#define UI_H

#include "circuito.h"
#include "osc.h"
#if ESPECIAL
#include "especial_api.h"              /* la versión especial (no se publica) */
#endif

enum {
    T_NADA = 0,
    T_ARRIBA, T_ABAJO, T_IZQ, T_DER,
    T_ENTER, T_ESC, T_TAB, T_BORRAR, T_MENU,
    T_PUNTO, T_SIGNO,     /* SIGNO es la tecla (-) de negacion   */
    T_MAS, T_MENOS,
    T_IGUAL,              /* =  : resolver                      */
    T_INCOG,              /* ?! : marcar como incognita         */
    T_EE,                 /* EE : exponente                     */
    T_SHIFT, T_CTRL,      /* pegajosas: cambian la tecla siguiente */
    T_CATALOGO,           /* la del libro: la paleta de componentes */
    T_DIG0                /* T_DIG0 + n para el digito n        */
};
/* Modificadores que acompañan a una tecla (main.c los pone si shift o ctrl
 * están abajo a la vez; ui.c, si se pulsaron justo antes). */
#define T_CON_SHIFT 0x100
#define T_CON_CTRL  0x200
#define T_MODS      (T_CON_SHIFT | T_CON_CTRL)
#define T_DIG(n)    (T_DIG0 + (n))
#define T_LETRA0    (T_DIG0 + 10)
#define T_LETRA(c)  (T_LETRA0 + ((c) - 'A'))    /* 'A'..'Z' */
/* Las que llegaron con la capa de entrada, DETRÁS de las letras (así no se
 * corre ningún código de antes) y por debajo de 0x100: las cuentas de la
 * ficha (× ÷ ^ x² y los paréntesis) y el espacio de los nombres */
enum {
    T_ESPACIO = T_LETRA0 + 26,
    T_POR,                /* ×                                  */
    T_ENTRE,              /* ÷                                  */
    T_POT,                /* ^                                  */
    T_CUAD,               /* x²: escribe ^2                     */
    T_PAR_A, T_PAR_C,     /* ( y )                              */
    T_FIN                 /* la primera libre                   */
};

/* Marcas de la capa de entrada (entrada.c); caben sobre T_MODS (las letras
 * acaban antes de 0x100):
 *   T_REPE   repetición o paso táctil: no es el primer evento de su ráfaga.
 *            Lleva los modificadores del primero (ui.c los recuerda) y no
 *            gasta ni arma los pegajosos;
 *   T_TACTIL el paso viene de deslizar el dedo en el touchpad (siempre con
 *            T_REPE); sus modificadores son los que están ABAJO;
 *   T_SUELTA fin de ráfaga (se soltó, bajó otra tecla, cambió la pantalla o
 *            se llegó al borde): ui_tecla lo atiende arriba del todo, antes
 *            de nada, y solo cierra la ráfaga (la lectura exacta del Bode y
 *            del barrido, la entrada del historial del cable). */
#define T_REPE      0x400
#define T_TACTIL    0x800
#define T_SUELTA    0x1000
#define T_MARCAS    (T_REPE | T_TACTIL)

void ui_init(void);
void ui_dibuja(void);        /* pinta la pantalla actual en el backbuffer */
/* Pinta la pantalla p como ui_dibuja, sin cambiar a ella: el banco (2.ª
 * página) mide así un cuadro de una pantalla animada, con su gfx_clear */
void ui_dibuja_en(int p);
int  ui_tecla(int t);        /* procesa una tecla; devuelve 0 para salir  */

/* La repetición: ui.c dice QUÉ se repite (una lista blanca: por omisión,
 * nada) y entrada.c CUÁNDO (los tiempos de cada perfil, en entrada.h). Se
 * consulta ANTES DE CADA repetición y antes de cada paso táctil: 0 si no se
 * repite o si el paso no cambiaría nada (el borde); si no, el perfil, y en
 * p->tope el multiplicador máximo (1 = sin multiplicador). */
enum { REP_NO = 0, REP_LISTA, REP_CURSOR, REP_ESCALA, REP_BORRAR,
#if ESPECIAL
       ESP_REPS                                /* la versión especial */
#endif
       REP_N };
typedef struct { int tope; } UiRepe;
int ui_repite(int t, UiRepe *p);

/* Deslizar en el touchpad: el perfil de la pantalla que se ve. 0 = apagado.
 * Los pasos van en milésimas de W (x) o de H (y) del pad; balistica, el
 * multiplicador máximo por velocidad (1 = sin); uno, los ejes en los que un
 * contacto da un solo paso («pasar página»). */
enum { TAC_X = 1, TAC_Y = 2 };
typedef struct { int ejes, paso_x, paso_y, balistica, uno; } UiTactil;
int ui_tactil(UiTactil *p);

/* Pantallas. Viven aqui y no dentro de ui.c porque el arnes las nombra. */
enum {
    P_INICIO = 0, P_MENU,
    P_TALLER, P_EDITOR, P_ELEMENTO, P_MIS,            /* 1. Taller  */
    P_LECCIONES, P_TEORIA, P_EJEMPLOS,                /* 2. Aprende */
    P_AYUDA,                                          /* 3. Ayuda   */
    P_RESULTADO, P_TABLA, P_PASOS,                    /* comunes    */
#if ESPECIAL
    ESP_PANTALLAS                                     /* la versión especial */
#endif
    P_BANCO,                                          /* T en la Ayuda       */
    P_MODELO,                                         /* [catálogo] en la ficha */
    P_EMULANDO, P_OSC, P_EMU_TABLA, P_EMU_ESQ,        /* el emulador            */
    P_AJUSTES,                                        /* tiempo y paso          */
    P_CA, P_CA_TABLA,                                 /* la alterna: fasores    */
    P_FASORIAL,                                       /* diagrama y potencias   */
    P_BODE,                                           /* el trazador de Bode    */
    P_THEVENIN,                                       /* Thévenin y Norton      */
    P_BARRIDO,                                        /* el barrido de CC (curva.c) */
    P_CONTROLES,                                      /* [ctrl][menu]: deslizar, repetición, clic */
    P_CPM, P_CPM_RES, P_CPM_DIB,                      /* los capacitores equivalentes (Taller, 4.ª):
                                                         datos, resultado y dibujo (capmotor.c) */
    P_CPM_MENU, P_CPM_TEO, P_CPM_PAG, P_CPM_EJ,       /* su menú, la Teoría (índice y página) y los
                                                         Ejemplos (capeq_texto.c) */
    P_N
};

/* Pantallas animadas (el banco midiendo, la cuenta atrás del grabador): el
 * bucle de bucle.c llama a ui_tick() BU_ANIMA_HZ (30) veces por segundo de media, a
 * plazo fijo, y dibuja tras cada una. En las demás pantallas solo dibuja si
 * algo cambió: ahorra batería. */
int  ui_animando(void);
void ui_tick(void);
/* Lo que se ve y no sale de una tecla ni de un tick (el grabador de la capa
 * y sus segundos, en cualquier pantalla): si cambia, el bucle repinta */
int  ui_rotulo(void);
/* ui_animando() devuelve esto cuando hay trabajo (emulando): main.c no
 * duerme entre ticks */
#define UI_TRABAJANDO 2

/* Carpeta donde se guardan "Mis problemas": en la calculadora, la ruta del
 * propio programa (argv[0]); en el PC, una carpeta de pruebas. */
void ui_carpeta(const char *ruta);

/* La sesión (ces_sesion.tns, junto a Mis problemas): el circuito del editor,
 * su hueco y el cursor. main.c la abre al arrancar (después de ui_carpeta) y
 * la guarda al salir; el editor la guarda también al dejarlo por el Taller o
 * el menú. ui_init NO la abre: el arnés arranca siempre vacío (y borra el
 * archivo al empezar). Si no se puede escribir, no se dice nada. */
void ui_sesion_abrir(void);
void ui_sesion_guardar(void);

/* Los ajustes del panel «Controles» (ces_controles.tns, junto a Mis
 * problemas): main.c los abre al arrancar, después de ui_carpeta. Sin
 * archivo, los de entrada.h. ui_init no los abre (el arnés empieza siempre
 * con los de omisión). */
void ui_controles_abrir(void);

/* Operaciones por tick de trabajo (emular, el Bode, la calculadora de CA y
 * el barrido): SIM_OPS_TICK por omisión (el arnés); main.c pone BU_OPS_TICK
 * (entrada.h), más pequeño, para que [esc] se vea antes. El resultado no
 * depende del troceo (lo prueba el arnés). */
void ui_trabajo(unsigned long ops);

/* Para el arnes: pantalla actual y el circuito en edicion. */
int ui_pantalla(void);
/* Modo práctica: 0 no, 1 respuestas tapadas, 2 ya contestado. */
int ui_practica(int *bien, int *total);
int ui_capa(void);           /* capa del dibujo en el resultado        */
int ui_isol(void);           /* solución que se enseña                 */
int ui_paso_tapado(void);    /* el paso a la vista está tapado (práctica) */
const Circuito *ui_circuito(void);
int ui_sel(void);            /* el elemento elegido en el editor, o -1  */
int ui_campo_q(void);        /* en la ficha: la magnitud (Q_*) de la fila elegida; -1 fuera */
int ui_niveles(int *rehacer);/* niveles de deshacer (y en *rehacer, los de rehacer) */
int ui_paso(void);           /* paso que se ve en la pantalla de pasos */
int ui_npasos(void);
const SimRes *ui_simres(void);   /* la última emulación                */
const Osc *ui_osc(void);         /* el osciloscopio                    */
const struct CaRes *ui_ca(void); /* el último análisis de CA           */
const struct Trazador *ui_trazador(void);   /* la pantalla del Bode    */
const struct Fasorial *ui_fasorial(void);   /* el diagrama fasorial    */
int ui_ca_sol(void);             /* la solución de la calculadora que se ve */
/* El arnés: la semilla de las animaciones (hoy, un resumen de todas las
 * teclas desde el arranque); fijarla hace que no dependan de cuántas teclas
 * pulsaron las pruebas de antes */
void ui_semilla(unsigned s);
const struct Curva *ui_curva(void);         /* la pantalla del barrido de CC */
int ui_res_nl(void);             /* P_RESULTADO viene del punto de operación (Newton) */
const Valores *ui_valores(void);            /* los valores que enseña P_RESULTADO */
/* La línea de los LED (§4.5), para el arnés: 0 sin LED, 1 normal, 2 si se quemaría */
int ui_led_linea(char *buf, int cap, const char *pre, const Circuito *c, const double *I);
/* La fila del balance de la tabla de CC (ent y ab: lo que entregan y
 * absorben; res: |ΣP abs| / Σ|P abs|), para el arnés: 1 si cuadra */
int ui_fila_balance(char *buf, int cap, double ent, double ab, double res);

/* La paleta de componentes ([catálogo]): PAL_N piezas en filas de 4. Los
 * índices no se mueven: las piezas nuevas entran al final con su modelo (20 en
 * el paso 11 de la Etapa 4, hasta «Amp. op.»; 21 y 22 en los pasos 13 y 14). */
#define PAL_N 22
int ui_paleta_tipo(int i);       /* el tipo (EL_*) que pone la pieza i, o -1 (cable, tierra, terminal, sonda) */

#ifndef NDLESS
/* Para el arnés: la foto del estado que deciden las teclas (pantalla,
 * scroll, selecciones, cursores, avisos, el circuito y las pantallas de
 * osc.c, trazador.c, curva.c y fasorial.c, sin sus cachés), para comprobar
 * que no depende de si se dibujó entre una tecla y otra. Devuelve los bytes
 * escritos (como mucho cap). */
int ui_foto(unsigned char *buf, int cap);
const char *ui_foto_campo(int byte);       /* la variable de ese byte de la última foto */
/* Y unos índices sueltos, para mirarlos sin dibujar */
enum { UI_I_SCROLL = 0, UI_I_SEL_MENU, UI_I_SEL_MIS, UI_I_SEL_LEC, UI_I_SEL_EJL, UI_I_PASO,
       UI_I_PAG, UI_I_CUR_X, UI_I_CUR_Y, UI_I_CAMPO, UI_I_ACCION, UI_I_RAFAGA, UI_I_AJ_CAMPO,
       UI_I_SEL_TALLER, UI_I_CPM_CAMPO, UI_I_CPM_SEL,
       /* los capacitores equivalentes (v3): el cursor, si la tecla escribe encima, la primera
        * línea de la caja de la bodega, el modo, la pestaña, la primera fila de la lista, lo
        * elegido en su menú, en la Teoría y en los Ejemplos, y las líneas del paso a paso */
       UI_I_CPM_POS, UI_I_CPM_NUEVO, UI_I_CPM_BTOP, UI_I_CPM_MODO, UI_I_CPM_TAB, UI_I_CPM_LSC,
       UI_I_CPM_MSEL, UI_I_CPM_TSEL, UI_I_CPM_ESEL, UI_I_CPM_NLIN, UI_I_N };
int ui_indice(int que);
/* Los capacitores equivalentes: el último resultado (capmotor.h), lo
 * escrito en cada campo (0 el valor, 1 la tolerancia, 2 la bodega) y el
 * paso a paso de la opción elegida */
const struct CpmRes *ui_cpm(void);
const char *ui_cpm_texto(int campo);
const char *ui_cpm_paso(void);
/* Las pruebas de antes de rev10 se escribieron para un Taller de tres filas
 * (bajan dos veces para llegar a Mis problemas) y una Ayuda que acaba en
 * ACERCA DE: con oculto (main lo pone para toda la batería), la 4.ª fila
 * del Taller y la sección del capacitor (al final de la Ayuda) no cuentan
 * para las teclas (el dibujo sí enseña la fila). Así sus caminos, sus
 * emulaciones y sus huellas siguen siendo los de base10, también los del
 * mono (sus ráfagas miran los bordes). Sin oculto, todo (las pruebas del
 * capacitor y la fase nueva del mono). Y la semilla de los juegos, para que
 * esa fase la deje como estaba (ui_semilla la repone) */
void ui_cpm_oculto(int si);
int  ui_cpm_oculto_esta(void);
unsigned ui_vueltas(void);
#endif

#endif /* UI_H */
